#import "GlkTextBufferWindowPrivate.h"

#import "GlkController.h"
#import "Theme.h"
#import "MarginContainer.h"
#import "MarginImage.h"
#import "MyAttachmentCell.h"
#import "BufferTextView.h"

// In release builds, suppress NSLog entirely.
#ifndef DEBUG
#define NSLog(...)
#endif // DEBUG

// Rolling scrollback cap. When the live text storage grows past the limit
// (the current theme's scrollbackLimit attribute) the oldest committed text is
// trimmed back to two thirds of the limit at a paragraph boundary, so layout,
// margin-image anchoring, and scrolling stay bounded over very long sessions.
// The active input region is never trimmed, and trimming only happens while
// the view is scrolled to the bottom.
static const NSUInteger kScrollbackTrimDefault = 30000;
// Floor for the setting, so the keep math stays meaningful and paging
// still works even if someone sets a tiny value.
static const NSUInteger kScrollbackTrimMinimum = 2000;

@implementation GlkTextBufferWindow (Scrolling)

- (void)layoutManager:(NSLayoutManager *)layoutManager didCompleteLayoutForTextContainer:(NSTextContainer *)textContainer atEnd:(BOOL)layoutFinishedFlag {
    if (layoutFinishedFlag == YES && layoutManager == layoutmanager && textContainer == container) {
        if (scrollAdjustTimeStamp.timeIntervalSinceNow > -1 && !self.glkctl.ignoreResizes && !self.inLiveResize && !_pendingScroll) {
            if (!scrolling) {
                [self restoreScroll:self];
            } else {
                [self reallyPerformScroll];
            }
        }
    }
}

// Trigger a full layout pass so glyph positions are up to date.
// Skipped for very large documents (>50k chars) to avoid performance issues.
- (void)forceLayout{
    if (textstorage.length < 50000)
        [layoutmanager glyphRangeForTextContainer:container];
}

// Compute a front-trim cut point at a paragraph boundary. Starting from
// (length - targetKeep), scan forward to the next line boundary so the kept
// text always starts at the beginning of a line; never cut past safeLimit
// (which protects the active input region). Returns 0 when no clean trim is
// available. Pure and side-effect free so it can be unit tested.
+ (NSUInteger)scrollbackCutPointForString:(NSString *)string
                               targetKeep:(NSUInteger)targetKeep
                                safeLimit:(NSUInteger)safeLimit {
    NSUInteger length = string.length;
    if (length <= targetKeep)
        return 0;

    NSUInteger want = length - targetKeep;
    if (want > safeLimit)
        want = safeLimit;
    if (want == 0)
        return 0;

    NSUInteger cut = want;
    while (cut < safeLimit) {
        unichar c = [string characterAtIndex:cut - 1];
        if (c == '\n' || c == '\r')
            break;
        cut++;
    }

    // No paragraph boundary available below the safe limit: skip this round.
    if (cut >= safeLimit)
        return 0;

    return cut;
}

// Clamp a raw scrollback-limit value into the supported range. A value of
// 0 (or negative) means unlimited - the scrollback is never trimmed. Any other
// positive value is clamped up to a sensible floor. Shared by the per-window
// getter below and by the Preferences UI.
+ (NSUInteger)clampScrollbackLimit:(NSInteger)value {
    if (value <= 0)
        return 0; // unlimited - no trimming
    if ((NSUInteger)value < kScrollbackTrimMinimum)
        return kScrollbackTrimMinimum;
    return (NSUInteger)value;
}

// The scrollback character limit for this window, taken from the current
// theme's scrollbackLimit attribute (clamped). 0 means unlimited.
- (NSUInteger)scrollbackLimit {
    return [GlkTextBufferWindow clampScrollbackLimit:self.theme.scrollbackLimit];
}

// Enforce the rolling scrollback cap: when the buffer grows past the limit,
// delete the oldest text down to the target size and fix up every absolute
// offset we cache (fence, paging markers, VoiceOver move ranges, attachment
// cells, and margin-image / flow-break anchors).
//
// Trim only while the user is following output at the bottom, or while a
// command script is replaying. A front trim deletes from the start of the text
// storage, which shifts every glyph below it and so invalidates the whole
// document's layout; the next height query then forces a full synchronous
// re-layout. That is cheap for a small buffer but can stall for seconds on a
// large one with many margin images - so we must not do it underneath a user
// who has paged or scrolled up to read, where it would blank the view mid-read.
// While following at the bottom the auto-scroll repositions to the bottom
// afterwards, and a script is an automated playthrough, so neither disturbs a
// reader.
- (void)trimScrollbackIfNeeded:(BOOL)wasAtBottom {
    if (!wasAtBottom && !self.glkctl.commandScriptRunning) {
        // Scrolled up / paging: a synchronous front trim would force an O(N)
        // relayout and blank the view mid-read. When opted in, and once the
        // buffer has grown well past the limit, hand off to a background trim
        // that rebuilds a trimmed, pre-laid-out text system off the main thread
        // and swaps it in with the scroll position preserved.
        NSUInteger limit = self.scrollbackLimit;
        if (limit != 0 && textstorage.length > limit * 2)
            [self scheduleBackgroundTrimToLimit:limit];
        return;
    }

    NSUInteger limit = self.scrollbackLimit;
    if (limit == 0)
        return; // unlimited scrollback
    if (textstorage.length <= limit)
        return;

    // During a line request, never trim into the editable input region.
    NSUInteger safeLimit = line_request ? fence : textstorage.length;

    NSUInteger cut = [GlkTextBufferWindow scrollbackCutPointForString:textstorage.string
                                                          targetKeep:(limit * 2 / 3)
                                                           safeLimit:safeLimit];
    if (cut == 0)
        return;

    // Measure the removed height (the y at which the first kept line currently
    // sits) so we can shift _lastseen, which is a y-coordinate, correctly.
    NSUInteger keptGlyph = [layoutmanager glyphRangeForCharacterRange:NSMakeRange(cut, 1)
                                                actualCharacterRange:NULL].location;
    NSRect keptLineRect = [layoutmanager lineFragmentRectForGlyphAtIndex:keptGlyph
                                                         effectiveRange:NULL];
    CGFloat removedHeight = keptLineRect.origin.y;

    // Capture the current scroll origin so we can shift it up by the removed
    // height after the trim. Without this, the clip origin stays in the old
    // coordinate system while _lastseen and the document height move to the new
    // one; reallyPerformScroll's one-viewport cap is then computed against a
    // stale position and over-advances, scrolling unread text off the top.
    NSClipView *clipView = scrollview.contentView;
    NSPoint scrollOrigin = clipView.bounds.origin;

    [textstorage deleteCharactersInRange:NSMakeRange(0, cut)];

    // Re-anchor every attachment cell to its new character position.
    [textstorage enumerateAttribute:NSAttachmentAttributeName
                            inRange:NSMakeRange(0, textstorage.length)
                            options:0
                         usingBlock:^(NSTextAttachment *attachment, NSRange range, BOOL *stop) {
        MyAttachmentCell *cell = (MyAttachmentCell *)attachment.attachmentCell;
        if (cell)
            cell.pos = range.location;
    }];

    // Drop margin images / flow breaks whose anchor was trimmed, shift the rest.
    [container shiftAnchorsAfterTrimOf:cut];

    [self shiftCachedOffsetsAfterTrimOf:cut removedHeight:removedHeight];

    [container invalidateLayout:nil];

    // Hold the visible content in place: everything moved up by removedHeight,
    // so move the scroll origin up by the same amount. This keeps currentPos
    // consistent with the shifted _lastseen and document height, so the
    // subsequent auto-scroll still advances by at most one viewport. Skipped
    // during a command script, where there is no reader to disturb and the
    // auto-scroll repositions anyway - the extra redraw just slows replay.
    if (!self.glkctl.commandScriptRunning) {
        scrollOrigin.y -= removedHeight;
        if (scrollOrigin.y < 0)
            scrollOrigin.y = 0;
        [clipView setBoundsOrigin:scrollOrigin];
        [scrollview reflectScrolledClipView:clipView];
    }
}

// Shift every cached absolute offset to account for a front trim of `cut`
// characters (which removed `removedHeight` points of laid-out text). fence,
// lastVisible and printPositionOnInput are character indices; _lastseen is a
// y-coordinate; moveRanges are character ranges (dropped if fully trimmed,
// clamped if they straddle the cut). Shared by the inline and background trims.
- (void)shiftCachedOffsetsAfterTrimOf:(NSUInteger)cut removedHeight:(CGFloat)removedHeight {
    fence = (fence > cut) ? fence - cut : 0;
    lastVisible = (lastVisible > cut) ? lastVisible - cut : 0;
    _printPositionOnInput = (_printPositionOnInput > cut) ? _printPositionOnInput - cut : 0;
    _lastseen = ((CGFloat)_lastseen > removedHeight) ? _lastseen - (NSInteger)removedHeight : 0;

    if (pendingEchoDeleteRange.length) {
        if (NSMaxRange(pendingEchoDeleteRange) <= cut) {
            // Ghost was entirely within the trimmed region.
            pendingEchoDeleteRange = NSMakeRange(0, 0);
        } else if (pendingEchoDeleteRange.location >= cut) {
            pendingEchoDeleteRange.location -= cut;
        } else {
            pendingEchoDeleteRange.length = NSMaxRange(pendingEchoDeleteRange) - cut;
            pendingEchoDeleteRange.location = 0;
        }
    }

    NSMutableArray<NSValue *> *shifted =
        [NSMutableArray arrayWithCapacity:self.moveRanges.count];
    for (NSValue *value in self.moveRanges) {
        NSRange r = value.rangeValue;
        if (NSMaxRange(r) <= cut)
            continue; // entirely within the trimmed region
        if (r.location >= cut) {
            r.location -= cut;
        } else {
            // Straddles the cut: keep the surviving tail.
            r.length = NSMaxRange(r) - cut;
            r.location = 0;
        }
        [shifted addObject:[NSValue valueWithRange:r]];
    }
    self.moveRanges = shifted;
    moveRangeIndex = shifted.count;
}

// Trim the scrollback without blocking the main thread, so it can run while the
// user has paged or scrolled up to read. We build a trimmed, fully-laid-out
// text system on a background thread and swap it in instantly, then restore the
// scroll position by character anchor (cheap, since the new system is already
// laid out). The live view keeps showing the old content until the swap, so
// there is no blank. Opt-in (BufferBackgroundTrim) and conservatively gated; it
// degrades gracefully - if it can't run, the buffer simply stays large.
- (void)scheduleBackgroundTrimToLimit:(NSUInteger)limit {
    if (backgroundLayoutInProgress)
        return;
    if (![NSUserDefaults.standardUserDefaults boolForKey:@"BufferBackgroundTrim"])
        return;

    // Choose the cut on the main thread, bounded so the removed region is
    // entirely above the visible area (never trim what the user can see).
    NSUInteger safeLimit = line_request ? fence : textstorage.length;
    NSRect visibleRect = scrollview.documentVisibleRect;
    if (NSHeight(visibleRect) >= 1) {
        NSUInteger firstVisibleChar =
            [layoutmanager characterIndexForPoint:NSMakePoint(NSMinX(visibleRect), NSMinY(visibleRect))
                                  inTextContainer:container
         fractionOfDistanceBetweenInsertionPoints:nil];
        if (firstVisibleChar < safeLimit)
            safeLimit = firstVisibleChar;
    }
    NSUInteger cut = [GlkTextBufferWindow scrollbackCutPointForString:textstorage.string
                                                          targetKeep:(limit * 2 / 3)
                                                           safeLimit:safeLimit];
    if (cut == 0)
        return;

    // Record the scroll anchor (bottom-of-viewport character + sub-line offset)
    // so we can restore the exact position in the trimmed system after the swap.
    [self storeScrollOffset];
    if (lastAtBottom || lastAtTop)
        return; // not actually parked in the middle; let the inline path handle it

    NSUInteger keptGlyph = [layoutmanager glyphRangeForCharacterRange:NSMakeRange(cut, 1)
                                                actualCharacterRange:NULL].location;
    CGFloat removedHeight = [layoutmanager lineFragmentRectForGlyphAtIndex:keptGlyph
                                                           effectiveRange:NULL].origin.y;
    CGFloat anchorOffset = lastScrollOffset;

    backgroundLayoutInProgress = YES;
    NSUInteger generation = ++backgroundLayoutGeneration;

    // Snapshot the *trimmed* text and the container to lay out off-thread.
    NSMutableAttributedString *trimmedText = [textstorage mutableCopy];
    [trimmedText deleteCharactersInRange:NSMakeRange(0, cut)];
    NSData *containerArchive =
        [NSKeyedArchiver archivedDataWithRootObject:container
                              requiringSecureCoding:YES
                                              error:nil];
    NSSize containerSize = container.size;
    CGFloat lineFragmentPadding = container.lineFragmentPadding;

    NSSize   inset           = _textview.textContainerInset;
    BOOL     isEditable      = _textview.editable;
    NSRange  selection       = _textview.selectedRange;
    NSColor *bgColor         = _textview.backgroundColor;
    NSColor *cursorColor     = _textview.insertionPointColor;
    NSDictionary *linkAttrs  = [_textview.linkTextAttributes copy];
    BOOL     shouldDrawCaret = _textview.shouldDrawCaret;
    CGFloat  bottomPadding   = _textview.bottomPadding;

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        @autoreleasepool {
            NSTextStorage *bgStorage =
                [[NSTextStorage alloc] initWithAttributedString:trimmedText];

            NSLayoutManager *bgLayoutManager = [[NSLayoutManager alloc] init];
            bgLayoutManager.allowsNonContiguousLayout = NO;
            bgLayoutManager.backgroundLayoutEnabled = NO; // synchronous layout
            [bgStorage addLayoutManager:bgLayoutManager];

            NSError *error = nil;
            MarginContainer *bgContainer =
                [NSKeyedUnarchiver unarchivedObjectOfClass:[MarginContainer class]
                                                  fromData:containerArchive
                                                     error:&error];
            if (!bgContainer)
                bgContainer = [[MarginContainer alloc] initWithContainerSize:containerSize];
            bgContainer.size = containerSize;
            bgContainer.lineFragmentPadding = lineFragmentPadding;
            bgContainer.widthTracksTextView  = YES;
            bgContainer.heightTracksTextView = NO;
            [bgLayoutManager addTextContainer:bgContainer];

            // Match the container's images/breaks to the trimmed text.
            [bgContainer shiftAnchorsAfterTrimOf:cut];

            [bgLayoutManager ensureLayoutForTextContainer:bgContainer];

            dispatch_async(dispatch_get_main_queue(), ^{
                if (generation != self->backgroundLayoutGeneration) {
                    self->backgroundLayoutInProgress = NO;
                    return; // text changed while we worked; discard
                }

                BufferTextView *newTextView =
                    [[BufferTextView alloc] initWithFrame:NSMakeRect(0, 0, 0, 10000000)
                                            textContainer:bgContainer];
                [self configureBufferTextView:newTextView];
                newTextView.textContainerInset  = inset;
                newTextView.backgroundColor     = bgColor;
                newTextView.insertionPointColor = cursorColor;
                newTextView.linkTextAttributes  = linkAttrs;
                newTextView.shouldDrawCaret     = shouldDrawCaret;
                newTextView.bottomPadding       = bottomPadding;
                newTextView.editable            = isEditable;
                newTextView.delegate = self;
                bgStorage.delegate   = self;
                bgLayoutManager.delegate = self;
                bgContainer.textView = newTextView;

                // Reconnect margin-image attachment cells to the (unarchived,
                // trimmed) MarginImage objects, re-deriving positions from the
                // trimmed text.
                NSMutableDictionary<NSString *, MarginImage *> *imgsByUUID =
                    [[NSMutableDictionary alloc] initWithCapacity:bgContainer.marginImages.count];
                for (MarginImage *img in bgContainer.marginImages) {
                    img.container = bgContainer;
                    img.accessibilityParent = newTextView;
                    if (img.uuid)
                        imgsByUUID[img.uuid] = img;
                }
                [bgStorage enumerateAttribute:NSAttachmentAttributeName
                                      inRange:NSMakeRange(0, bgStorage.length)
                                      options:0
                                   usingBlock:^(NSTextAttachment *att, NSRange range, BOOL *stop) {
                    MyAttachmentCell *cell = (MyAttachmentCell *)att.attachmentCell;
                    if (!cell) return;
                    cell.pos = range.location;
                    if (cell.marginImgUUID) {
                        MarginImage *img = imgsByUUID[cell.marginImgUUID];
                        if (img) {
                            cell.marginImage = img;
                            img.pos = range.location;
                            img.bounds = [img boundsWithLayout:bgLayoutManager];
                            cell.accessibilityLabel = cell.customA11yLabel;
                        }
                    }
                }];

                BOOL wasFirstResponder = (self.window.firstResponder == self.textview);

                self->scrollview.documentView = newTextView;
                self->textstorage   = bgStorage;
                self->layoutmanager = bgLayoutManager;
                self->container     = bgContainer;
                self.textview       = newTextView;

                // Shift every cached offset to account for the trim.
                [self shiftCachedOffsetsAfterTrimOf:cut removedHeight:removedHeight];

                // Shift and restore the text selection.
                NSUInteger selLoc = (selection.location > cut) ? selection.location - cut : 0;
                if (selLoc + selection.length <= bgStorage.length)
                    newTextView.selectedRange = NSMakeRange(selLoc, selection.length);

                [newTextView enableCaret:nil];
                if (wasFirstResponder)
                    [self.window makeFirstResponder:newTextView];

                // Restore the scroll position by character anchor in the
                // already-laid-out new system (lastVisible was shifted above).
                BOOL savedPause = self->pauseScrolling;
                self->pauseScrolling = NO;
                [self scrollToCharacter:self->lastVisible withOffset:anchorOffset animate:NO];
                self->pauseScrolling = savedPause;

                self->backgroundLayoutInProgress = NO;
            });
        }
    });
}

// Record the Y-coordinate of the last visible line of text. This is
// used by performScroll to decide whether to scroll to the bottom or
// to the "last seen" position (showing a page at a time for long output).
// Also records _printPositionOnInput for speech move tracking.
- (void)markLastSeen {
    _printPositionOnInput = textstorage.length;
    if (fence > 0 && !char_request) {
        _printPositionOnInput = fence;
    }

    if (textstorage.length == 0) {
        _lastseen = 0;
        docHeightAtLastSeen = 0;
        return;
    }
    // Clamp _lastseen to the actual document height: if the current
    // document is shorter than the viewport (e.g. a small "STOP THE
    // TAPE" prompt in a tall viewport with no graphics window yet),
    // using the raw viewport bottom over-records what the user has
    // seen — the y-coords above the doc end contain no text. When the
    // game subsequently grows the document and the viewport shrinks
    // (graphics window appears), reallyPerformScroll's pagination would
    // then jump to _lastseen, which is a full viewport past the actual
    // end of the read content — surfacing as a one-viewport-too-far
    // auto-scroll on the first keypress after STOP THE TAPE.
    NSInteger viewportBottom =
        (NSInteger)(NSMaxY(scrollview.contentView.bounds)
                    - 2.0 * _textview.textContainerInset.height);
    // Use the layout-manager's last-line rect for docBottom rather than
    // NSHeight(_textview.frame). The textview is verticallyResizable with
    // minSize.height = 10000000, so its frame size is unreliable until a
    // layout pass nails it down — we'd otherwise snapshot ~10M and the
    // "doc fit in viewport" gate below would never trigger.
    NSRange glyphs = [layoutmanager glyphRangeForTextContainer:container];
    NSInteger docBottom = 0;
    if (glyphs.length) {
        NSRect lastLine = [layoutmanager
                           lineFragmentRectForGlyphAtIndex:NSMaxRange(glyphs) - 1
                           effectiveRange:nil];
        docBottom = (NSInteger)ceil(NSMaxY(lastLine));
    }
    _lastseen = MIN(viewportBottom, docBottom);
    // Snapshot the (true) document height so the auto-scroll caps can
    // use (docHeightAtLastSeen - lineHeight) as the target: the last
    // line of the on-screen content at keypress time lands at the top
    // of the new viewport, with the game's response stacked below —
    // the classic "more"-page anchor applied to a doc that hadn't
    // overflowed yet.
    docHeightAtLastSeen = (CGFloat)docBottom;
}

// Capture the current scroll position for later restoration (e.g. after
// a theme change or window resize). Records whether we're at the top,
// bottom, or a mid-document position. For mid-document positions, stores
// the character index at the bottom-right of the visible rect and the
// fractional offset (in cell heights) for pixel-accurate restoration.
- (void)storeScrollOffset {
    if (_pendingScrollRestore)
        return;
    if (self.scrolledToBottom) {
        lastAtBottom = YES;
        lastAtTop = NO;
    } else {
        lastAtTop = self.scrolledToTop;
        lastAtBottom = NO;
    }

    if (lastAtBottom || lastAtTop || textstorage.length < 1) {
        return;
    }

    NSRect visibleRect = scrollview.documentVisibleRect;

    lastVisible = [layoutmanager characterIndexForPoint:NSMakePoint(NSMaxX(visibleRect),
                                                                    NSMaxY(visibleRect))
                                        inTextContainer:container
               fractionOfDistanceBetweenInsertionPoints:nil];

    if (lastVisible != 0)
        lastVisible--;
    if (lastVisible >= textstorage.length) {
        lastVisible = textstorage.length - 1;
    }

    NSRect lastRect =
    [layoutmanager lineFragmentRectForGlyphAtIndex:lastVisible
                                    effectiveRange:nil withoutAdditionalLayout:YES];

    lastScrollOffset = (NSMaxY(visibleRect) - NSMaxY(lastRect)) / self.theme.bufferCellHeight;

    if (isnan(lastScrollOffset) || isinf(lastScrollOffset))
        lastScrollOffset = 0;
}

// Restore the scroll position previously captured by storeScrollOffset.
// Handles three cases: was at bottom, was at top, or was at a specific
// character offset. Called after theme changes or window resizing.
- (void)restoreScroll:(id)sender {
    scrollAdjustTimeStamp = [NSDate date];
    // A restore is in progress while _pendingScrollRestore is set. The
    // scheduled (nil-sender) call is the authoritative end of the restore
    // window and clears it; layoutComplete callbacks (non-nil sender) that
    // fire during the window leave it set so they, too, restore uncapped.
    BOOL restoring = _pendingScrollRestore;
    BOOL followupScroll = NO;
    if (sender == nil) {
        _pendingScrollRestore = NO;
        // A frame width change invalidated every y-coordinate captured in
        // the old layout. The new layout exists by now, so recompute
        // _lastseen from the character it pointed at — otherwise the
        // auto-scroll caps keep aiming at old-layout positions, which can
        // strand the view mid-scrollback when the rewrap grew the document
        // (wide margins and large line spacing make the growth big).
        if (rewrapLastseenChar != NSNotFound) {
            if (rewrapLastseenChar < textstorage.length) {
                NSUInteger glyph =
                    [layoutmanager glyphRangeForCharacterRange:NSMakeRange(rewrapLastseenChar, 1)
                                          actualCharacterRange:NULL].location;
                if (glyph != NSNotFound) {
                    NSRect line = [layoutmanager lineFragmentRectForGlyphAtIndex:glyph
                                                                  effectiveRange:nil];
                    // In the doc-fit case markLastSeen recorded
                    // docHeightAtLastSeen == _lastseen (both were the last
                    // line's bottom); keep them in step. When they differ the
                    // doc had overflowed and the docHAL anchor gate is off.
                    BOOL docFit = (docHeightAtLastSeen == (CGFloat)_lastseen);
                    _lastseen = (NSInteger)ceil(NSMaxY(line));
                    if (docFit)
                        docHeightAtLastSeen = (CGFloat)_lastseen;
                }
            }
            rewrapLastseenChar = NSNotFound;
        }
        followupScroll = rewrapFollowupScroll;
        rewrapFollowupScroll = NO;
    }
    _pendingScroll = NO;
    //    NSLog(@"GlkTextBufferWindow %ld restoreScroll", self.name);
    //    NSLog(@"lastVisible: %ld lastScrollOffset:%f", lastVisible, lastScrollOffset);
    if (_textview.bounds.size.height <= scrollview.bounds.size.height) {
        if (_textview.bounds.size.height == scrollview.bounds.size.height) {
            _textview.frame = self.bounds;
        }
        return;
    }

    if (lastAtBottom) {
        // Whether to cap this scroll depends on who called us, distinguished
        // by sender:
        //
        // sender == nil — an explicit resize/theme/fullscreen restore (every
        // such caller invokes us via performSelector:withObject:nil). Here we
        // must NOT cap: when the viewport shrank by more than one viewport-
        // height — e.g. exiting fullscreen on a large monitor leaves
        // clipView.bounds.origin.y at its fullscreen value while the windowed
        // target is further down — the one-viewport advance cap would stop the
        // scroll one line short of the actual bottom.
        //
        // sender != nil — the layoutComplete callback's !scrolling branch
        // (which passes self). During live game output this DOES fire: chatty
        // games (the Level 9 Adrian Mole / Archers titles) emit PRINT-then-
        // INITCHAR bursts in which the callback runs while scrolling==NO, so it
        // reaches this path rather than reallyPerformScroll. An uncapped jump to
        // the new bottom there races past unread text — the exact "don't skip
        // unread text" invariant. Respect caps so the auto-scroll advances by
        // at most one viewport and never past _lastseen, matching the normal
        // reallyPerformScroll auto path.
        //
        // The exception is `restoring`: the same callback also fires while an
        // autorestore/theme/resize restore is still in flight (see
        // postRestoreAdjustments and prefsDidChange). Those callbacks are part
        // of the restore, not live output, and must NOT cap — on a fresh restore
        // _lastseen is 0, so the cap would pin the window to the top instead of
        // the saved bottom.
        [self scrollToBottomAnimated:NO respectCaps:(sender != nil && !restoring)];
        return;
    }

    if (lastAtTop) {
        [self scrollToTop];
        return;
    }

    if (!lastVisible) {
        [self scrollToTop];
        return;
    }

    [self scrollToCharacter:lastVisible withOffset:lastScrollOffset animate:NO];

    // The auto-scroll that was pending when the rewrap hit ran against
    // old-layout coordinates and typically moved nowhere. Now that the
    // anchor is re-applied and _lastseen is valid in the new layout, run
    // it again so a short response (echo plus fresh prompt) just below the
    // anchor is revealed instead of stranded under the fold; long unread
    // output still paginates through the usual caps.
    if (followupScroll)
        [self performSelector:@selector(reallyPerformScroll) withObject:nil afterDelay:0.1];
    return;
}

// Scroll so that the given character index is visible at the bottom
// of the viewport, adjusted by offset (in cell-height units).
// If the target character is very close to the top, scrolls to top instead.
- (void)scrollToCharacter:(NSUInteger)character withOffset:(CGFloat)offset animate:(BOOL)animate {

    CGFloat charHeight = self.theme.bufferCellHeight;
    if (pauseScrolling)
        return;

    NSRect line;

    if (character >= textstorage.length - 1 || !textstorage.length) {
        return;
    }

    offset = offset * charHeight;

    line = [layoutmanager lineFragmentRectForGlyphAtIndex:character
                                           effectiveRange:nil withoutAdditionalLayout:YES];

    CGFloat charbottom = NSMaxY(line); // bottom of the line
    if (fabs(charbottom - NSHeight(scrollview.frame)) < charHeight && NSHeight(scrollview.frame) / charHeight > 3) {
        //        NSLog(@"scrollToCharacter: too close to the top!");
        [self scrollToTop];
        return;
    }
    charbottom = charbottom + offset;
    [self scrollToPosition:floor(charbottom - NSHeight(scrollview.frame)) animate:animate];
}

// Request a deferred scroll. The actual scrolling happens in
// reallyPerformScroll, which is called during the next flushDisplay cycle.
- (void)performScroll {
    if (_pendingScrollRestore)
        return;
    _pendingScroll = YES;
}

// Execute the deferred scroll. If the new content since _lastseen is
// taller than the viewport, scrolls to the _lastseen position (paging
// through output). Otherwise scrolls all the way to the bottom.
// Skips layout for very large documents (>1M chars) for performance.
// Disables animation when a command script is running.
- (void)reallyPerformScroll {
    // If a scrollToBottomAnimated callback is already queued from a prior
    // call, skip this one. Otherwise each layoutComplete callback that
    // arrives during a single text burst would advance one more viewport
    // (via the IF-branch cap below or via the dispatch's cap), compounding
    // into many viewports of total advance for a single "round" of game
    // output. The pending callback already captured the right intent;
    // letting it run alone matches the per-burst pacing of the slow-draw
    // path.
    if (scrollToBottomPending) {
        _pendingScroll = NO;
        self.glkctl.shouldScrollOnCharEvent = NO;
        return;
    }

    _pendingScroll = NO;
    self.glkctl.shouldScrollOnCharEvent = NO;

    if (pauseScrolling || _lastseen == 0 || textstorage.length == 0)
        return;
//
//    if (textstorage.length < 1000000)
//        // first, force a layout so we have the correct textview frame
//        [layoutmanager ensureLayoutForTextContainer:container];

    // then, get the bottom
    CGFloat bottom = NSHeight(_textview.frame);

    // If _lastseen is stale (e.g. a game restart truncated the textstorage
    // without going through the clear-window path that resets it), it can
    // point past the new document. Don't paginate to a position that no
    // longer exists; treat it as 0 so the player's fresh viewport is held.
    if ((CGFloat)_lastseen > bottom) {
        _lastseen = 0;
        docHeightAtLastSeen = 0;
    }

    BOOL animate = !self.glkctl.commandScriptRunning;
    if (bottom - (CGFloat)_lastseen > NSHeight(scrollview.frame)) {
        // Cap the paginate jump to at most one viewport's advance (minus
        // one line of overlap) from the current scroll position. _lastseen
        // is the y-coord of the bottom of the viewport at the user's last
        // keypress, but if the viewport has since shrunk (e.g. a graphics
        // window appeared above the text window), _lastseen can sit more
        // than one viewport below the current scroll position — paginating
        // straight to it would skip past unread text and look like a "one
        // viewport too far" auto-scroll. The ELSE branch's
        // scrollToBottomAnimated:respectCaps applies the same cap; mirror
        // it here so the IF branch behaves consistently.
        CGFloat currentPos = scrollview.contentView.bounds.origin.y;
        CGFloat viewportHeight = NSHeight(scrollview.contentView.bounds);
        CGFloat lineHeight = self.theme.bufferCellHeight;
        CGFloat advance = viewportHeight - lineHeight;
        if (advance < lineHeight) advance = viewportHeight;
        CGFloat target = (CGFloat)_lastseen;
        if (target - currentPos > advance) {
            target = currentPos + advance;
        }
        // Anchor to "last on-screen line at top, with one line of
        // overlap": when the doc fit fully in the viewport at the
        // keypress, the natural page-down target is one lineHeight
        // above the doc-as-of-keypress, so the last pre-response line
        // sits at the top of the new viewport with the response stacked
        // below it. Gated on docHAL <= viewportHeight so it only
        // applies in the doc-fit case; when the doc already overflowed
        // ("more"-prompt mode) the page-down advance above is right.
        if (docHeightAtLastSeen > 0
            && docHeightAtLastSeen <= viewportHeight) {
            CGFloat anchored = docHeightAtLastSeen - lineHeight;
            if (anchored > currentPos && target > anchored) {
                target = anchored;
            }
        }
        [self scrollToPosition:target animate:animate];
    } else {
        scrollAdjustTimeStamp = [NSDate date];
        [self scrollToBottomAnimated:animate respectCaps:YES];
    }
}

// Check if the scroll view is at the bottom of its content.
// Considers the text container inset and bottom padding in the threshold.
// Returns YES for zero-height windows (e.g. Kerkerkruip's invisible
// buffer used to catch key input).
- (BOOL)scrolledToBottom {
    NSView *clipView = scrollview.contentView;

    if (!clipView || NSHeight(clipView.bounds) == 0) {
        return YES;
    }

    return (NSHeight(_textview.bounds) - NSMaxY(clipView.bounds) <
            2 + _textview.textContainerInset.height + _textview.bottomPadding);
}

// Scroll to the very bottom of the text content. Call sites that surface
// game output (reallyPerformScroll's auto path) should pass respectCaps:YES
// so the callback doesn't race past unread text; call sites that act on the
// user's explicit request (End key, line-input cursor follow, command-script
// replay, restoreScroll) should pass NO so the scroll actually reaches the
// bottom of the document.
- (void)scrollToBottomAnimated:(BOOL)animate {
    [self scrollToBottomAnimated:animate respectCaps:NO];
}

- (void)scrollToBottomAnimated:(BOOL)animate respectCaps:(BOOL)respectCaps {
    lastAtTop = NO;
    lastAtBottom = YES;

    scrollAdjustTimeStamp = [NSDate date];

    // Coalesce overlapping scroll-to-bottom requests. Without this, every
    // PRINT-then-INITCHAR cycle from a chatty game queues a new 100ms-delayed
    // callback; each one advances by one viewport (see the cap below), so N
    // callbacks compound into N viewports of advance and the player misses
    // unread text in between.
    if (respectCaps && scrollToBottomPending)
        return;
    if (respectCaps)
        scrollToBottomPending = YES;

    BufferTextView *blockTextView = _textview;

    scrolling = YES;

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        if (respectCaps)
            self->scrollToBottomPending = NO;

        NSScrollView *blockscrollview = blockTextView.enclosingScrollView;
        CGFloat viewportHeight = NSHeight(blockscrollview.contentView.bounds);
        CGFloat currentPosition = blockscrollview.contentView.bounds.origin.y;
        CGFloat bottom = NSMaxY(blockTextView.frame) - viewportHeight + blockTextView.textContainerInset.height;

        if (respectCaps) {
            // The 100ms delay above means `bottom` is read after later PRINTs
            // may have grown the document. If this callback fires after a
            // different performScroll already paginated past the old bottom,
            // jumping all the way to the new bottom would skip past unread
            // text. Cap each callback to a single viewport's worth of advance
            // (minus one line of overlap so the last line of the old viewport
            // becomes the first line of the new one — the standard "page
            // down" convention, which gives the reader a continuity anchor
            // instead of cutting off mid-paragraph) so it behaves like one
            // "more"-page step instead of a jump-to-end.
            CGFloat lineHeight = self.theme.bufferCellHeight;
            CGFloat advance = viewportHeight - lineHeight;
            if (advance < lineHeight) advance = viewportHeight;
            if (bottom - currentPosition > advance) {
                bottom = currentPosition + advance;
            }

            // Also never advance past `_lastseen` — the bottom of the
            // viewport at the user's last keypress. Auto-scroll exists to
            // surface what the game just emitted; it must not race past
            // content the player hasn't had a chance to acknowledge.
            // Paginate scrolls to exactly _lastseen, so without this cap a
            // follow-up scroll-to-bottom callback would advance another
            // viewport and the player would miss the screen paginate just
            // revealed. When _lastseen == 0 (the player hasn't pressed any
            // key yet — e.g. the very first chunk of game output, like the
            // "STOP THE TAPE" prompt) this clamps to 0 so the initial
            // screen stays put until the player acknowledges it. If
            // _lastseen now sits beyond the document (the textstorage was
            // truncated by a game restart without the clear-window path
            // resetting it), treat it as 0 so the new game's initial
            // screen is held in place.
            CGFloat lastseenCap = (CGFloat)self->_lastseen;
            if (lastseenCap > NSMaxY(blockTextView.frame)) {
                lastseenCap = 0;
            }
            if (bottom > lastseenCap) {
                bottom = lastseenCap;
            }

            // Anchor to "last on-screen line at top, with one line of
            // overlap": when the doc fit fully in the viewport at the
            // keypress (docHAL <= V), the natural scroll target is one
            // lineHeight above the doc-as-of-keypress, so the last
            // pre-response line sits at the top of the new viewport
            // with the response stacked below. The standard
            // scroll-to-bottom target (newDocBottom - V + inset) doesn't
            // know about the keypress-time content boundary, and
            // typically lands further down — leaving the prior content
            // tightly cropped at the top instead of cleanly anchored.
            // Gated on docHAL <= viewportHeight so it only applies in
            // the doc-fit case; in "more"-prompt mode the unmodified
            // scroll-to-bottom is right.
            CGFloat docHAL = self->docHeightAtLastSeen;
            if (docHAL > 0 && docHAL <= viewportHeight) {
                CGFloat anchored = docHAL - lineHeight;
                if (anchored > currentPosition && bottom > anchored) {
                    bottom = anchored;
                }
            }
        }

		[self scrollToPosition:bottom animate:animate];
    });
}

// Scroll to an absolute Y position within the text view. Only scrolls
// downward (returns early if already past the target). When animation
// is enabled and the theme supports smooth scrolling, animates with a
// duration proportional to the distance (clamped to 0.3–1.0 seconds).
- (void)scrollToPosition:(CGFloat)position animate:(BOOL)animate {
    scrolling = NO;
    if (pauseScrolling) {
        return;
    }
    NSClipView* clipView = scrollview.contentView;
    NSRect newBounds = clipView.bounds;
    CGFloat diff = position - newBounds.origin.y;
    if (newBounds.origin.y > position) {
        return;
    }
    newBounds.origin.y = position;
    if (animate && self.theme.smoothScroll) {
        scrolling = YES;
        [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context){
            context.duration = MAX(0.002 * diff, 0.3);
            if (context.duration > 1)
                context.duration = 1;
            clipView.animator.boundsOrigin = newBounds.origin;
        } completionHandler:^{
            self->scrolling = NO;
        }];
    } else {
        clipView.bounds = newBounds;
    }
}

// Check if the scroll view is at or near the top (within one cell height).
- (BOOL)scrolledToTop {
    NSView *clipView = scrollview.contentView;
    if (!clipView) {
        return NO;
    }
    CGFloat diff = clipView.bounds.origin.y - _textview.textContainerInset.height;
    return (fabs(diff) < self.theme.bufferCellHeight);
}

// Scroll to the very top of the document.
- (void)scrollToTop {
    if (pauseScrolling)
        return;
    lastAtTop = YES;
    lastAtBottom = NO;

    NSPoint p = NSMakePoint(0, _textview.textContainerInset.height);

    [scrollview.contentView scrollToPoint:p];
}

@end
