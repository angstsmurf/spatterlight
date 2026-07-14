#import "GlkTextBufferWindowPrivate.h"

#import "GlkTextGridWindow.h"
#import "Constants.h"
#import "GlkController.h"
#import "Theme.h"
#import "Game.h"
#import "Metadata.h"
#import "GlkStyle.h"
#import "InputHistory.h"
#import "MarginContainer.h"
#import "BufferTextView.h"
#include "glkimp.h"

// In release builds, suppress NSLog entirely.
#ifndef DEBUG
#define NSLog(...)
#endif // DEBUG

/*
 * GlkTextBufferWindow is the Glk text buffer window type — a scrollable,
 * styled text view used for the main game text. It manages the full
 * NSText* stack (NSTextStorage → NSLayoutManager → MarginContainer →
 * BufferTextView → NSScrollView), handles line and character input requests,
 * maintains command history, supports inline and margin images, hyperlinks,
 * Z-machine color overrides, the find bar, VoiceOver speech, and autosave
 * serialization of all of the above.
 */

@implementation GlkTextBufferWindow

// These readonly properties are computed by getters in the category files
// (+Scrolling and +Input), so no ivars must be synthesized for them here.
@dynamic scrolledToBottom, editableRange, numberOfColumns, numberOfLines;

+ (BOOL) supportsSecureCoding {
    return YES;
}

// Initialize a new text buffer window. Builds the entire NSText* stack from
// scratch: NSTextStorage → NSLayoutManager → MarginContainer → BufferTextView,
// wrapped in an NSScrollView. Also initializes Glk styles from the current
// theme and style hints, command history, and link appearance.
- (instancetype)initWithGlkController:(GlkController *)glkctl_ name:(NSInteger)name_ {

    self = [super initWithGlkController:glkctl_ name:name_];

    if (self) {
        NSInteger marginX = self.theme.bufferMarginX;
        NSInteger marginY = self.theme.bufferMarginY;

        NSUInteger i;

        // Deep-copy the style hints so per-window hint changes don't affect others
        NSDictionary *styleDict = nil;
        self.styleHints = [GlkWindow deepCopyOfStyleHintsArray:self.glkctl.bufferStyleHints];

        // Build the styles array: one NSDictionary of text attributes per Glk style.
        // When doStyles is on, game-provided style hints are applied on top of the
        // theme defaults; otherwise, raw theme attributes are used directly.
        styles = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
        for (i = 0; i < style_NUMSTYLES; i++) {
            if (self.theme.doStyles) {
                styleDict = [((GlkStyle *)[self.theme valueForKey:gBufferStyleNames[i]]) attributesWithHints:self.styleHints[i]];
            } else {
                styleDict = ((GlkStyle *)[self.theme valueForKey:gBufferStyleNames[i]]).attributeDict;
            }
            if (!styleDict) {
                NSLog(@"GlkTextBufferWindow couldn't create style dict for style %ld", i);
                [styles addObject:[NSNull null]];
            } else {
                [styles addObject:styleDict];
            }
        }

        echo = YES;

        _lastchar = '\n';

        // We use lastLineheight to restore scroll position with sub-character size precision
        // after a resize
        lastLineheight = self.theme.bufferCellHeight;
        rewrapLastseenChar = NSNotFound;

        history = [[InputHistory alloc] init];

        self.moveRanges = [[NSMutableArray alloc] init];
        scrollview = [[NSScrollView alloc] initWithFrame:NSZeroRect];

        [self restoreScrollBarStyle];

        // Construct the NSText* stack manually rather than using IB, because
        // we need a custom MarginContainer that supports margin images and
        // flow breaks. The very large height (10M) allows unbounded vertical
        // growth as text accumulates.
        textstorage = [[NSTextStorage alloc] init];
        bufferTextstorage = [textstorage mutableCopy];

        layoutmanager = [[NSLayoutManager alloc] init];
        layoutmanager.delegate = self;
        layoutmanager.backgroundLayoutEnabled = YES;
        layoutmanager.allowsNonContiguousLayout = NO;
        [textstorage addLayoutManager:layoutmanager];

        container = [[MarginContainer alloc]
                     initWithContainerSize:NSMakeSize(0, 10000000)];

        container.layoutManager = layoutmanager;
        [layoutmanager addTextContainer:container];

        _textview =
        [[BufferTextView alloc] initWithFrame:NSMakeRect(0, 0, 0, 10000000)
                                textContainer:container];

        [self configureBufferTextView:_textview];
        _textview.editable = NO;

        container.textView = _textview;

        scrollview.documentView = _textview;
        scrollview.contentView.copiesOnScroll = YES;
        scrollview.verticalScrollElasticity = NSScrollElasticityNone;

        // The container tracks the text view's width (for word wrapping) but
        // not its height (so the text view grows vertically as text is added).
        container.widthTracksTextView = YES;
        container.heightTracksTextView = NO;

        _textview.delegate = self;
        textstorage.delegate = self;

        _textview.textContainerInset = NSMakeSize((CGFloat)marginX, (CGFloat)marginY);
        _textview.insertionPointColor = styles[style_Normal][NSForegroundColorAttributeName];

        NSMutableDictionary *linkAttributes = [_textview.linkTextAttributes mutableCopy];
        linkAttributes[NSUnderlineStyleAttributeName] = @(self.theme.bufLinkStyle);
        linkAttributes[NSForegroundColorAttributeName] = styles[style_Normal][NSForegroundColorAttributeName];
        _textview.linkTextAttributes = linkAttributes;

        [_textview enableCaret:nil];

        scrollview.accessibilityLabel = @"buffer scroll view";

        [self addSubview:scrollview];

        if (self.glkctl.usesFont3)
            [self createBeyondZorkStyle];

        underlineLinks = (self.theme.bufLinkStyle != NSUnderlineStyleNone);
        [self recalcBackground];
        scrollAdjustTimeStamp = [NSDate distantPast];
    }

    return self;
}

// Apply the standard BufferTextView configuration shared by the initial text
// view built in init and the replacement built during a background scrollback
// trim. Keeping it in one place stops the two construction paths from drifting
// apart. Instance-specific properties (frame, insets, colors, delegate,
// editable) are set by the caller.
- (void)configureBufferTextView:(BufferTextView *)textView {
    textView.minSize = NSMakeSize(1, 10000000);
    textView.maxSize = NSMakeSize(10000000, 10000000);
    textView.horizontallyResizable = NO;
    textView.verticallyResizable = YES;
    textView.autoresizingMask = NSViewWidthSizable;
    textView.allowsImageEditing = NO;
    textView.allowsUndo = NO;
    textView.usesFontPanel = NO;
    textView.usesFindBar = YES;
    textView.incrementalSearchingEnabled = YES;
    textView.smartInsertDeleteEnabled = NO;
}

// Append `toAppend` to the text storage, coalescing any pending echo-off delete
// into the same edit pass so the input replacement is a single paint rather
// than two (delete now, append milliseconds later). When echo is off and the
// player submits a line, the typed text is left in place and
// pendingEchoDeleteRange records what to delete; this folds that deletion into
// the next output append. Returns YES if a deferred delete was applied.
- (BOOL)coalescePendingEchoDeleteWithAppend:(NSAttributedString *)toAppend {
    if (pendingEchoDeleteRange.length
        && NSMaxRange(pendingEchoDeleteRange) <= textstorage.length) {
        [textstorage beginEditing];
        [textstorage deleteCharactersInRange:pendingEchoDeleteRange];
        [textstorage appendAttributedString:toAppend];
        [textstorage endEditing];
        pendingEchoDeleteRange = NSMakeRange(0, 0);
        fence = textstorage.length;
        return YES;
    }
    [textstorage appendAttributedString:toAppend];
    return NO;
}

// Apply a deferred echo-off delete if one is pending and still within the text
// storage, then clear the pending range unconditionally. Used by call sites
// (unputString, initLine, cancelLine) that need the ghost input gone before
// they read or manipulate the tail of the text. Returns YES if characters were
// actually deleted.
- (BOOL)applyPendingEchoDeleteIfValid {
    BOOL applied = NO;
    if (pendingEchoDeleteRange.length
        && NSMaxRange(pendingEchoDeleteRange) <= textstorage.length) {
        [textstorage deleteCharactersInRange:pendingEchoDeleteRange];
        applied = YES;
    }
    pendingEchoDeleteRange = NSMakeRange(0, 0);
    return applied;
}

// Override setFrame: to defer the actual frame change until flushDisplay.
// This batching prevents layout thrashing when multiple windows are resized
// in a single update cycle. Also includes a Curses-specific workaround to
// adjust the buffer window height when quote boxes expand the status line.
- (void)setFrame:(NSRect)frame {
    GlkController *glkctl = self.glkctl;

    if (glkctl.gameID == kGameIsCurses && glkctl.quoteBoxes.count && glkctl.turns > 0) {
        // When we extend the height of the status
        // line in Curses in order to make the second line
        // visible when showing quote boxes, we also need to
        // reduce the height of the buffer window below it.
        // This is mainly to prevent the search bar from
        // being partially hidden under the status window.
        for (GlkTextGridWindow *grid in glkctl.gameView.subviews) {
            if ([grid isKindOfClass:[GlkTextGridWindow class]]) {
                frame.size.height = glkctl.gameView.frame.size.height - grid.pendingFrame.size.height;
                frame.origin.y = NSMaxY(grid.pendingFrame);
                break;
            }
        }
    }

    if (self.framePending && NSEqualRects(self.pendingFrame, frame)) {
        // Same frame as last frame, returning
        return;
    }
    self.framePending = YES;
    self.pendingFrame = frame;

    if (self.inLiveResize)
        [self flushDisplay];
}

// Commit all pending changes to the display:
// 1. Apply any deferred frame change
// 2. If a clear is pending, replace textstorage with the buffered content
// 3. Otherwise, append buffered text to textstorage
// 4. Perform any pending scroll
// The hyphenation language is temporarily set to the game's language to
// ensure correct word breaking for non-English text.
- (void)flushDisplay {
    GlkController *glkctl = self.glkctl;
    NSString *language = glkctl.game.metadata.language;
    if (!language.length)
        language = @"en";
    NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
    NSString *hyphenationLanguage = [defaults valueForKey:@"NSHyphenationLanguage"];
    [NSUserDefaults.standardUserDefaults setValue:language forKey:@"NSHyphenationLanguage"];

    if (!bufferTextstorage)
        bufferTextstorage = [[NSMutableAttributedString alloc] init];

    // Capture whether the user is following output at the bottom *before* we
    // append new text (which would grow the document and make the check read
    // false). Used to gate scrollback trimming, which must not run while the
    // user has paged or scrolled up to read.
    BOOL wasAtBottom = self.scrolledToBottom;

    if (self.framePending) {
        self.framePending = NO;
        if (!NSEqualRects(self.pendingFrame, self.frame)) {

            if ([container hasMarginImages])
                [container invalidateLayout:nil];

            if (NSMaxX(self.pendingFrame) > NSWidth(glkctl.gameView.bounds) && NSWidth(self.pendingFrame) > 10) {
                self.pendingFrame = NSMakeRect(self.pendingFrame.origin.x, self.pendingFrame.origin.y, NSWidth(glkctl.gameView.bounds) - self.pendingFrame.origin.x, self.pendingFrame.size.height);
            }

            // A width change re-wraps the whole document, so every
            // y-coordinate captured in the old layout — the clip origin as
            // well as _lastseen — is invalid afterwards: content above the
            // viewport that wraps to more lines pushes everything below it
            // further down. Capture the position as a character anchor
            // before the old layout goes away, and schedule the same
            // uncapped restore used after theme changes and live resizes.
            // Without this, a game-driven arrange (e.g. ADRIFT opening its
            // map pane) leaves the auto-scroll caps aiming at old-layout
            // coordinates, which strands the view mid-scrollback when the
            // rewrap growth exceeds the caps (wide margins and large line
            // spacing make it large). Live resizes and fullscreen
            // transitions store/restore around the whole gesture instead.
            BOOL rewrapRestore = NSWidth(self.pendingFrame) != NSWidth(self.frame)
                && !_pendingScrollRestore && !self.inLiveResize
                && !glkctl.ignoreResizes && textstorage.length > 0;
            if (rewrapRestore) {
                [self storeScrollOffset];
                // _lastseen is a y-coordinate in the layout that is about to
                // go away; remember the character under it so restoreScroll
                // can recompute it once the new layout exists. Also note
                // whether an auto-scroll was pending: it will run against
                // old-layout coordinates and typically go nowhere, so the
                // restore re-runs it afterwards.
                if (_lastseen > 0) {
                    rewrapLastseenChar =
                        [layoutmanager characterIndexForPoint:NSMakePoint(0, (CGFloat)_lastseen - 1)
                                              inTextContainer:container
                     fractionOfDistanceBetweenInsertionPoints:nil];
                    if (rewrapLastseenChar >= textstorage.length)
                        rewrapLastseenChar = textstorage.length - 1;
                }
                rewrapFollowupScroll = _pendingScroll;
            }

            super.frame = self.pendingFrame;

            // If the viewport shrank while we were pinned to the bottom,
            // re-pin immediately. Without this, a live-resize shrink leaves
            // clipView.bounds.origin.y unchanged while the viewport height
            // decreases, so NSMaxY(clipView.bounds) no longer reaches the
            // document end and the last line disappears below the visible area.
            // Growing works without intervention: the clip view extending
            // downward already satisfies scrolledToBottom, so the existing
            // restoreScroll / layoutComplete path is not needed there.
            // (On a rewrap this is a stale-layout best effort that reduces
            // the visible jump; the scheduled restoreScroll finishes the job.)
            if (wasAtBottom && !self.scrolledToBottom) {
                NSClipView *clipView = scrollview.contentView;
                CGFloat viewportHeight = NSHeight(clipView.bounds);
                CGFloat newBottom = NSMaxY(_textview.frame) - viewportHeight
                                    + _textview.textContainerInset.height;
                if (newBottom > clipView.bounds.origin.y) {
                    [clipView scrollToPoint:NSMakePoint(clipView.bounds.origin.x, newBottom)];
                    [scrollview reflectScrolledClipView:clipView];
                }
                lastAtBottom = YES;
            }

            if (rewrapRestore) {
                _pendingScrollRestore = YES;
                [self performSelector:@selector(restoreScroll:) withObject:nil afterDelay:0.2];
            }
        }
    }

    if (_pendingClear) {
        [self reallyClear];
        [textstorage setAttributedString:bufferTextstorage];
        backgroundLayoutGeneration++;
        // A window clear starts a fresh page, so the viewport belongs at the
        // top. AppKit only auto-clamps the scroll origin back to the top when
        // the new content is shorter than the viewport; without this reset an
        // equally-tall (or taller) post-clear page — e.g. an ADRIFT title
        // screen's multi-screen "Playing Instructions" — inherits the previous
        // page's bottom scroll origin and opens scrolled to its end. (The
        // _lastseen == 0 early-return in reallyPerformScroll leaves the origin
        // untouched, so it can't correct this on its own.) Skipped during
        // autorestore, where restoreScroll re-applies the saved position.
        if (!_pendingScrollRestore)
            [self scrollToTop];
    } else if (bufferTextstorage.length) {
        [self coalescePendingEchoDeleteWithAppend:bufferTextstorage];
        backgroundLayoutGeneration++;
    }

    bufferTextstorage = [[NSMutableAttributedString alloc] init];

    [self trimScrollbackIfNeeded:wasAtBottom];

    if (_pendingScroll) {
        // Resume auto-scrolling once the user has scrolled back to the bottom.
        // wasAtBottom was captured before the append above, so it reflects
        // whether the user was following output rather than the just-grown
        // document (which always reads as "not at bottom" until we scroll).
        // This is the resume counterpart to scrollWheelchanged's pause and
        // covers both command-script and timer-driven output.
        if (pauseScrolling && wasAtBottom)
            pauseScrolling = NO;

        if (glkctl.commandScriptRunning) {
            if (!commandScriptWasRunning) {
                // A command script just started
                pauseScrolling = NO;
                commandScriptWasRunning = YES;
            }

            if (pauseScrolling) {
                _pendingScroll = NO;
                return;
            }
        } else if (commandScriptWasRunning) {
            // A command script was just switched off
            pauseScrolling = NO;
            commandScriptWasRunning = NO;
        }

        [self reallyPerformScroll];
    }
    [NSUserDefaults.standardUserDefaults setValue:hyphenationLanguage forKey:@"NSHyphenationLanguage"];
}

// Track scroll wheel activity while the game produces unattended output —
// command-script playback or a timer-driven (real-time/animated) game.
// Scrolling up pauses auto-scrolling so the user can read; scrolling back to
// the bottom resumes it.
//
// Called from -[BufferTextView scrollWheel:] *after* the event has been
// applied, so scrolledToBottom reflects the current position. Resume uses the
// strict scrolledToBottom test (not the old "within one viewport of the
// bottom" test), so the trackpad-momentum sub-events — whose deltas wobble in
// both directions — can no longer cancel the pause the instant it is set,
// which is what made scrolling up nearly impossible.
//
// The at-bottom test takes priority over the upward-delta pause: once the user
// has scrolled back down to the bottom, a trackpad rubber-band/momentum bounce
// emits a spurious upward delta there, which would otherwise re-arm the pause
// while the view is sitting on the input prompt — leaving auto-scroll stuck off
// after the next command. Being at the bottom always resumes (and never
// pauses).
- (void)scrollWheelchanged:(NSEvent *)event {
    if (self.glkctl.commandScriptRunning || self.glkctl.timerActive) {
        if (self.scrolledToBottom) {
            // At the very bottom. Resume scrolling, and never let a momentum
            // bounce's upward delta re-pause us here.
            pauseScrolling = NO;
        } else if (event.scrollingDeltaY > 0) {
            // Scrollbar moved up, away from the bottom. Pause scrolling.
            pauseScrolling = YES;
        }
    }
}

// Make this window's text view the first responder (keyboard focus).
- (void)grabFocus {
    BufferTextView *localTextView = _textview;
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.window makeFirstResponder:localTextView];
    });
}

// This window wants focus if it has an active character or line input request.
- (BOOL)wantsFocus {
    return char_request || line_request;
}

// Called when the interpreter process terminates. Locks the text view to
// read-only, grabs focus for accessibility, and performs a final scroll/flush.
- (void)terpDidStop {
    _textview.editable = NO;
    [self grabFocus];
    [self performScroll];
    [self flushDisplay];
}

// Insert blank lines at the beginning of the text storage. Used during
// autorestore to pad the text so that scroll position can be restored
// correctly (the restored content needs the same vertical extent).
- (void)padWithNewlines:(NSUInteger)lines {
    NSString *newlinestring = [[[NSString alloc] init]
                               stringByPaddingToLength:lines
                               withString:@"\n"
                               startingAtIndex:0];
    NSDictionary *attributes = [textstorage attributesAtIndex:0 effectiveRange:nil];
    NSAttributedString *attrStr = [[NSAttributedString alloc] initWithString:newlinestring attributes:attributes];
    [textstorage insertAttributedString:attrStr atIndex:0];
    if (self.moveRanges.count) {
        NSRange range = self.moveRanges.firstObject.rangeValue;
        range.location += lines;
        (self.moveRanges)[0] = [NSValue valueWithRange:range];
    }
    fence += lines;
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    return [_textview validateMenuItem:menuItem];
}

@end
