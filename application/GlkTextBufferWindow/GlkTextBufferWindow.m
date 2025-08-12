#import "GlkTextBufferWindow.h"
#import "GlkTextGridWindow.h"
#import "GlkEvent.h"

#import "Constants.h"
#import "GlkController.h"
#import "NSString+Categories.h"
#import "NSColor+integer.h"
#import "Theme.h"
#import "Game.h"
#import "Metadata.h"
#import "GlkStyle.h"
#import "ZColor.h"
#import "InputHistory.h"
#import "ZMenu.h"
#import "MyAttachmentCell.h"
#import "MarginContainer.h"
#import "MarginImage.h"
#import "BufferTextView.h"
#import "GridTextView.h"
#import "InfocomV6MenuHandler.h"
#import "ImageHandler.h"
#include "glkimp.h"


#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                 \
fprintf(stderr, "%s\n",                                                    \
[[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String])
#else
#define NSLog(...)
#endif // DEBUG

/*
 * Controller for the various objects required in the NSText* mess.
 */

@interface GlkTextBufferWindow () <NSSecureCoding, NSTextViewDelegate, NSTextStorageDelegate> {
    NSScrollView *scrollview;
    NSLayoutManager *layoutmanager;
    MarginContainer *container;
    NSTextStorage *textstorage;
    NSMutableAttributedString *bufferTextstorage;

    BOOL line_request;
    BOOL hyper_request;

    BOOL echo_toggle_pending; /* if YES, line echo behavior will be inverted,
                               starting from the next line event*/
    BOOL echo; /* if NO, line input text will be deleted when entered */

    NSUInteger fence; /* for input line editing */

    CGFloat lastLineheight;

    NSAttributedString *storedNewline;

    // for temporarily storing scroll position
    NSUInteger lastVisible;
    CGFloat lastScrollOffset;
    BOOL lastAtBottom;
    BOOL lastAtTop;

    BOOL pauseScrolling;
    BOOL commandScriptWasRunning;

    BOOL scrolling;
    NSMutableArray<NSEvent *> *bufferedEvents;

    NSRange lastSpokenRange;
    NSString *lastSpokenString;
}
@end


@implementation GlkTextBufferWindow

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithGlkController:(GlkController *)glkctl_ name:(NSInteger)name_ {

    self = [super initWithGlkController:glkctl_ name:name_];

    if (self) {
        NSInteger marginX = self.theme.bufferMarginX;
        NSInteger marginY = self.theme.bufferMarginY;

        NSUInteger i;

        NSDictionary *styleDict = nil;
        self.styleHints = [self deepCopyOfStyleHintsArray:self.glkctl.bufferStyleHints];

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

        history = [[InputHistory alloc] init];

        self.moveRanges = [[NSMutableArray alloc] init];
        scrollview = [[NSScrollView alloc] initWithFrame:NSZeroRect];

        [self restoreScrollBarStyle];

        /* construct text system manually */

        textstorage = [[NSTextStorage alloc] init];
        bufferTextstorage = [textstorage mutableCopy];

        layoutmanager = [[NSLayoutManager alloc] init];
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

        _textview.editable = NO;
        _textview.minSize = NSMakeSize(1, 10000000);
        _textview.maxSize = NSMakeSize(10000000, 10000000);

        container.textView = _textview;

        scrollview.documentView = _textview;
        scrollview.contentView.copiesOnScroll = YES;
        scrollview.verticalScrollElasticity = NSScrollElasticityNone;

        /* now configure the text stuff */

        container.widthTracksTextView = YES;
        container.heightTracksTextView = NO;

        _textview.horizontallyResizable = NO;
        _textview.verticallyResizable = YES;

        _textview.autoresizingMask = NSViewWidthSizable;

        _textview.allowsImageEditing = NO;
        _textview.allowsUndo = NO;
        _textview.usesFontPanel = NO;
        _textview.usesFindBar = YES;
        _textview.incrementalSearchingEnabled = YES;

        _textview.smartInsertDeleteEnabled = NO;

        _textview.delegate = self;
        textstorage.delegate = self;

        _textview.textContainerInset = NSMakeSize(marginX, marginY);
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
    }

    return self;
}

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
        //        NSLog(@"Same frame as last frame, returning");
        return;
    }
    self.framePending = YES;
    self.pendingFrame = frame;

    if (self.inLiveResize)
        [self flushDisplay];
}

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

    if (self.framePending) {
        self.framePending = NO;
        if (!NSEqualRects(self.pendingFrame, self.frame)) {

            if ([container hasMarginImages])
                [container invalidateLayout:nil];

            if (NSMaxX(self.pendingFrame) > NSWidth(glkctl.gameView.bounds) && NSWidth(self.pendingFrame) > 10) {
                self.pendingFrame = NSMakeRect(self.pendingFrame.origin.x, self.pendingFrame.origin.y, NSWidth(glkctl.gameView.bounds) - self.pendingFrame.origin.x, self.pendingFrame.size.height);
            }

            super.frame = self.pendingFrame;
        }
    }

    if (_pendingClear) {
        [self reallyClear];
        [textstorage setAttributedString:bufferTextstorage];
    } else if (bufferTextstorage.length) {
        [textstorage appendAttributedString:bufferTextstorage];
    }

    bufferTextstorage = [[NSMutableAttributedString alloc] init];

    if (_pendingScroll) {
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
    if (_pendingEditable) {
        if (glkctl.commandScriptRunning) {
            [self scrollToBottomAnimated:NO];
        } else {
            _textview.editable = YES;
            _pendingEditable = NO;
        }
    }
}

- (void)scrollWheelchanged:(NSEvent *)event {
    if (self.glkctl.commandScriptRunning) {
        if (pauseScrolling && event.scrollingDeltaY < 0) {
            if (NSHeight(_textview.bounds) - NSMaxY(scrollview.contentView.bounds) < NSHeight(scrollview.contentView.bounds)) {
                // Scrollbar moved down close enough to bottom. Resume scrolling.
                pauseScrolling = NO;
                return;
            }
        }

        if (event.scrollingDeltaY > 0 ) {
            // Scrollbar moved up. Pause scrolling.
            pauseScrolling = YES;
        }
    }
}

- (void)grabFocus {
    BufferTextView *localTextView = _textview;
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.window makeFirstResponder:localTextView];
    });
    // NSLog(@"GlkTextBufferWindow %ld grabbed focus.", self.name);
}

- (BOOL)wantsFocus {
    return char_request || line_request;
}

- (void)terpDidStop {
    _textview.editable = NO;
    [self grabFocus];
    [self performScroll];
    [self flushDisplay];
}

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

#pragma mark Autorestore

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        _textview = [decoder decodeObjectOfClass:[BufferTextView class] forKey:@"textview"];
        layoutmanager = _textview.layoutManager;
        textstorage = _textview.textStorage;
        container = (MarginContainer *)_textview.textContainer;
        if (!layoutmanager)
            NSLog(@"layoutmanager nil!");
        if (!textstorage)
            NSLog(@"textstorage nil!");
        if (!container)
            NSLog(@"container nil!");
        scrollview = _textview.enclosingScrollView;
        if (!scrollview)
            NSLog(@"scrollview nil!");
        scrollview.accessibilityLabel = NSLocalizedString(@"buffer scroll view", nil);
        scrollview.documentView = _textview;
        _textview.delegate = self;
        textstorage.delegate = self;

        if (_textview.textStorage != textstorage)
            NSLog(@"Error! _textview.textStorage != textstorage");

        [self restoreScrollBarStyle];

        line_request = [decoder decodeBoolForKey:@"line_request"];
        _textview.editable = NO;
        hyper_request = [decoder decodeBoolForKey:@"hyper_request"];

        echo_toggle_pending = [decoder decodeBoolForKey:@"echo_toggle_pending"];
        echo = [decoder decodeBoolForKey:@"echo"];

        fence = (NSUInteger)[decoder decodeIntegerForKey:@"fence"];
        _printPositionOnInput = (NSUInteger)[decoder decodeIntegerForKey:@"printPositionOnInput"];
        _lastchar = (unichar)[decoder decodeIntegerForKey:@"lastchar"];
        _lastseen = [decoder decodeIntegerForKey:@"lastseen"];
        _restoredSelection =
        ((NSValue *)[decoder decodeObjectOfClass:[NSValue class] forKey:@"selectedRange"])
            .rangeValue;
        _textview.selectedRange = _restoredSelection;

        _restoredAtBottom = [decoder decodeBoolForKey:@"scrolledToBottom"];
        _restoredAtTop = [decoder decodeBoolForKey:@"scrolledToTop"];
        _restoredLastVisible = (NSUInteger)[decoder decodeIntegerForKey:@"lastVisible"];
        _restoredScrollOffset = [decoder decodeDoubleForKey:@"scrollOffset"];

        _textview.insertionPointColor =
        [decoder decodeObjectOfClass:[NSColor class] forKey:@"insertionPointColor"];
        _textview.shouldDrawCaret =
        [decoder decodeBoolForKey:@"shouldDrawCaret"];
        _restoredSearch = [decoder decodeObjectOfClass:[NSString class] forKey:@"searchString"];
        _restoredFindBarVisible = [decoder decodeBoolForKey:@"findBarVisible"];
        storedNewline = [decoder decodeObjectOfClass:[NSAttributedString class] forKey:@"storedNewline"];

        _pendingScroll = [decoder decodeBoolForKey:@"pendingScroll"];
        _pendingClear = [decoder decodeBoolForKey:@"pendingClear"];
        _pendingScrollRestore = NO;

        bufferTextstorage = [decoder decodeObjectOfClass:[NSMutableAttributedString class] forKey:@"bufferTextstorage"];

        _restoredInput = [decoder decodeObjectOfClass:[NSAttributedString class] forKey:@"inputString"];
        _quoteBox = [decoder decodeObjectOfClass:[GlkTextGridWindow class] forKey:@"quoteBox"];

        [self destroyTextFinder];

        pauseScrolling = NO;
        commandScriptWasRunning = NO;
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeObject:_textview forKey:@"textview"];
    NSValue *rangeVal = [NSValue valueWithRange:_textview.selectedRange];
    [encoder encodeObject:rangeVal forKey:@"selectedRange"];
    [encoder encodeBool:line_request forKey:@"line_request"];
    [encoder encodeBool:hyper_request forKey:@"hyper_request"];
    [encoder encodeBool:echo_toggle_pending forKey:@"echo_toggle_pending"];
    [encoder encodeBool:echo forKey:@"echo"];
    [encoder encodeInteger:(NSInteger)fence forKey:@"fence"];
    [encoder encodeInteger:(NSInteger)_printPositionOnInput forKey:@"printPositionOnInput"];
    [encoder encodeInteger:_lastchar forKey:@"lastchar"];
    [encoder encodeInteger:_lastseen forKey:@"lastseen"];

    [self storeScrollOffset];

    [encoder encodeBool:lastAtBottom forKey:@"scrolledToBottom"];
    [encoder encodeBool:lastAtTop forKey:@"scrolledToTop"];
    [encoder encodeInteger:(NSInteger)lastVisible forKey:@"lastVisible"];
    [encoder encodeDouble:lastScrollOffset forKey:@"scrollOffset"];

    [encoder encodeObject:_textview.insertionPointColor
                   forKey:@"insertionPointColor"];
    [encoder encodeBool:_textview.shouldDrawCaret forKey:@"shouldDrawCaret"];
    NSSearchField *searchField = [self findSearchFieldIn:self];
    if (searchField) {
        [encoder encodeObject:searchField.stringValue forKey:@"searchString"];
    }
    [encoder encodeBool:scrollview.findBarVisible forKey:@"findBarVisible"];
    [encoder encodeObject:storedNewline forKey:@"storedNewline"];

    [encoder encodeBool:_pendingScroll forKey:@"pendingScroll"];
    [encoder encodeBool:_pendingClear forKey:@"pendingClear"];
    [encoder encodeBool:_pendingScrollRestore forKey:@"pendingScrollRestore"];

    if (line_request && textstorage.length > fence) {
        NSAttributedString *input = [textstorage attributedSubstringFromRange:self.editableRange];
        [encoder encodeObject:input forKey:@"inputString"];
    }

    [encoder encodeObject:bufferTextstorage forKey:@"bufferTextstorage"];
    [encoder encodeObject:_quoteBox forKey:@"quoteBox"];
}

- (void)postRestoreAdjustments:(GlkWindow *)win {
    GlkTextBufferWindow *restoredWin = (GlkTextBufferWindow *)win;

    // No idea where these spurious storedNewlines come from
    if (line_request || self.glkctl.commandScriptRunning)
        storedNewline = nil;

    if (line_request && (restoredWin.restoredInput).length) {
        NSAttributedString *restoredInput = restoredWin.restoredInput;
        if (textstorage.length > fence) {
            // Delete any preloaded input
            [textstorage deleteCharactersInRange:self.editableRange];
        }
        [textstorage appendAttributedString:restoredInput];
    }

    _restoredSelection = restoredWin.restoredSelection;
    NSRange allText = NSMakeRange(0, textstorage.length + 1);
    _restoredSelection = NSIntersectionRange(allText, _restoredSelection);
    _textview.selectedRange = _restoredSelection;
    _textview.editable = line_request;

    _restoredFindBarVisible = restoredWin.restoredFindBarVisible;
    _restoredSearch = restoredWin.restoredSearch;

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void) {
        [self restoreTextFinder];
    });

    _restoredLastVisible = restoredWin.restoredLastVisible;
    _restoredScrollOffset = restoredWin.restoredScrollOffset;
    _restoredAtTop = restoredWin.restoredAtTop;
    _restoredAtBottom = restoredWin.restoredAtBottom;

    lastVisible = _restoredLastVisible;
    lastScrollOffset = _restoredScrollOffset;
    lastAtTop = _restoredAtTop;
    lastAtBottom = _restoredAtBottom;

    self.moveRanges = restoredWin.moveRanges;

    _pendingScrollRestore = YES;
    _pendingScroll = NO;

    _lastNewTextOnTurn = self.glkctl.turns;

    NSMutableDictionary<NSString *, MyAttachmentCell *> *attachmentCells = [[NSMutableDictionary alloc] initWithCapacity:container.marginImages.count];

    [textstorage
     enumerateAttribute:NSAttachmentAttributeName
     inRange:NSMakeRange(0, textstorage.length)
     options:0
     usingBlock:^(NSTextAttachment *attachment, NSRange range, BOOL *stop) {
        MyAttachmentCell *cell = (MyAttachmentCell *)attachment.attachmentCell;
        if (cell) {
            if (cell.glkImgAlign == imagealign_MarginLeft || cell.glkImgAlign == imagealign_MarginRight) {
                if (cell.marginImgUUID != nil)
                    attachmentCells[cell.marginImgUUID] = cell;
            }
            cell.pos = range.location;
        }
    }];

    for (MarginImage *img in container.marginImages) {
        img.container = container;
        img.accessibilityParent = _textview;
        MyAttachmentCell *cell = attachmentCells[img.uuid];
        if (cell) {
            cell.marginImage = img;
            img.pos = cell.pos;
            img.bounds = [img boundsWithLayout:layoutmanager];
            cell.accessibilityLabel = cell.customA11yLabel;
        }
    }

    if (!self.glkctl.inFullscreen || self.glkctl.startingInFullscreen)
        [self performSelector:@selector(deferredScrollPosition:) withObject:nil afterDelay:0.1];
    else
        [self performSelector:@selector(deferredScrollPosition:) withObject:nil afterDelay:0.5];
}

- (void)deferredScrollPosition:(id)sender {
    [self restoreScrollBarStyle];
    if (_restoredAtBottom) {
        [self scrollToBottomAnimated:NO];
    } else {
        if (_restoredLastVisible == 0)
            [self scrollToBottomAnimated:NO];
        else
            [self scrollToCharacter:_restoredLastVisible withOffset:_restoredScrollOffset animate:NO];
    }
    _pendingScrollRestore = NO;
}

- (void)restoreScrollBarStyle {
    if (scrollview) {
        scrollview.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        scrollview.scrollerStyle = NSScrollerStyleOverlay;
        scrollview.drawsBackground = YES;
        NSColor *bgcolor = styles[style_Normal][NSBackgroundColorAttributeName];
        if (!bgcolor)
            bgcolor = self.theme.bufferBackground;
        scrollview.backgroundColor = bgcolor;
        scrollview.hasHorizontalScroller = NO;
        scrollview.hasVerticalScroller = YES;
        scrollview.verticalScroller.alphaValue = 100;
        scrollview.autohidesScrollers = YES;
        scrollview.borderType = NSNoBorder;
    }
}


#pragma mark Colors and styles

- (BOOL)allowsDocumentBackgroundColorChange {
    return YES;
}

- (void)recalcBackground {
    NSColor *bgcolor = styles[style_Normal][NSBackgroundColorAttributeName];

    if (self.theme.doStyles && bgnd > -1 && bgnd != zcolor_Default) {
        bgcolor = [NSColor colorFromInteger:bgnd];
    }
    if (!bgcolor) {
        bgcolor = self.theme.bufferBackground;
    }
    _textview.backgroundColor = bgcolor;

    if (line_request)
        [self showInsertionPoint];
    [self.glkctl setBorderColor:bgcolor fromWindow:self];
}

- (void)setBgColor:(NSInteger)bc {
    bgnd = bc;
    [self recalcBackground];
}

- (void)prefsDidChange {
    NSDictionary *attributes;
    if (!_pendingScrollRestore) {
        [self storeScrollOffset];
    }

    GlkController *glkctl = self.glkctl;

    // Adjust terminators for Beyond Zork arrow keys hack
    if (glkctl.gameID == kGameIsBeyondZork || [glkctl zVersion6]) {
        [self adjustBZTerminators:self.pendingTerminators];
        [self adjustBZTerminators:self.currentTerminators];
    }

    // Preferences has changed, so first we must redo the styles library
    NSMutableArray *newstyles = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
    BOOL different = NO;
    for (NSUInteger i = 0; i < style_NUMSTYLES; i++) {
        if (self.theme.doStyles) {
            // We're doing styles, so we call the current theme object with our hints array
            // in order to get an attributes dictionary
            attributes = [((GlkStyle *)[self.theme valueForKey:gBufferStyleNames[i]]) attributesWithHints:self.styleHints[i]];
        } else {
            // We're not doing styles, so use the raw style attributes from
            // the theme object's attributeDict object
            attributes = ((GlkStyle *)[self.theme valueForKey:gBufferStyleNames[i]]).attributeDict;
        }

        if (usingStyles != self.theme.doStyles) {
            different = YES;
            usingStyles = self.theme.doStyles;
        }

        if (underlineLinks != (self.theme.bufLinkStyle != NSUnderlineStyleNone)) {
            different = YES;
            underlineLinks = (self.theme.bufLinkStyle != NSUnderlineStyleNone);
        }

        if (attributes) {
            [newstyles addObject:attributes];
            if (!different && ![newstyles[i] isEqualToDictionary:styles[i]])
                different = YES;
        } else
            [newstyles addObject:[NSNull null]];
    }

    NSInteger marginX = self.theme.bufferMarginX;
    NSInteger marginY = self.theme.bufferMarginY;

    BOOL marginHeightChanged = (marginY != _textview.textContainerInset.height);
    CGFloat heightDiff = marginY - _textview.textContainerInset.height;

    _textview.textContainerInset = NSMakeSize(marginX, marginY);

    // If the Y margin has changed, we must adjust the text view
    // here to make the scrollview aware of this, otherwise we might
    // not be able to scroll to the bottom.
    if (marginHeightChanged) {
        NSRect newTextviewFrame = _textview.frame;
        newTextviewFrame.size.height += heightDiff * 2;
        _textview.frame = newTextviewFrame;
    }

    // We can think of attributes as special characters in the mutable attributed
    // string called textstorage.
    // Here we iterate through the textstorage string to find them all.
    // We have to do it character by character instead of using
    // enumerateAttribute:inRange:options:usingBlock:
    // to make sure that no inline images, hyperlinks or zcolors
    // get lost when we update the Glk Styles.

    if (different) {
        styles = newstyles;
        [self recalcInputAttributes];

        if (glkctl.usesFont3) {
            [self createBeyondZorkStyle];
        }

        /* reassign styles to attributedstrings */
        NSMutableAttributedString *backingStorage = [textstorage mutableCopy];

        if (storedNewline) {
            if (!self.theme.doStyles) {
                NSMutableDictionary *newLineAttributes = [storedNewline attributesAtIndex:0 effectiveRange:nil].mutableCopy;
                ZColor *zcolor = newLineAttributes[@"ZColor"];
                if (zcolor && zcolor.bg != zcolor_Current && zcolor.bg != zcolor_Default && zcolor.bg != zcolor_Transparent) {
                    newLineAttributes[NSBackgroundColorAttributeName] = nil;
                    storedNewline = [[NSAttributedString alloc] initWithString:@"\n" attributes:newLineAttributes];
                }
            }
            [backingStorage appendAttributedString:storedNewline];
        }

        NSRange selectedRange = _textview.selectedRange;

        NSArray __block *blockStyles = styles;
        [textstorage
         enumerateAttributesInRange:NSMakeRange(0, textstorage.length)
         options:0
         usingBlock:^(NSDictionary *attrs, NSRange range, BOOL *stop) {

            // First, we overwrite all attributes with those in our updated
            // styles array
            id styleobject = attrs[@"GlkStyle"];
            if (styleobject) {
                NSDictionary *stylesAtt = blockStyles[(NSUInteger)[styleobject intValue]];
                [backingStorage setAttributes:stylesAtt range:range];
            }

            // Then, we re-add all the "non-Glk" style values we want to keep
            // (inline images, hyperlinks, Z-colors and reverse video)
            id image = attrs[NSAttachmentAttributeName];
            if (image) {
                [backingStorage addAttribute: NSAttachmentAttributeName
                                       value: image
                                       range: NSMakeRange(range.location, 1)];
                ((MyAttachmentCell *)((NSTextAttachment *)image).attachmentCell).attrstr = backingStorage;
            }

            id hyperlink = attrs[NSLinkAttributeName];
            if (hyperlink) {
                [backingStorage addAttribute:NSLinkAttributeName
                                       value:hyperlink
                                       range:range];
            }

            id zcolor = attrs[@"ZColor"];
            if (zcolor) {
                [backingStorage addAttribute:@"ZColor"
                                       value:zcolor
                                       range:range];
            }

            id reverse = attrs[@"ReverseVideo"];
            if (reverse) {
                [backingStorage addAttribute:@"ReverseVideo"
                                       value:reverse
                                       range:range];
            }
        }];

        backingStorage = [self applyZColorsAndThenReverse:backingStorage];

        if (storedNewline) {
            storedNewline = [[NSAttributedString alloc] initWithString:@"\n" attributes:[backingStorage attributesAtIndex:backingStorage.length - 1 effectiveRange:NULL]];
            [backingStorage deleteCharactersInRange:NSMakeRange(backingStorage.length - 1, 1)];
        }

        [textstorage setAttributedString:backingStorage];

        _textview.selectedRange = selectedRange;
    }

    if (self.glkctl.isAlive) {
        if (different) {
            // Set style for hyperlinks
            NSMutableDictionary *linkAttributes = [_textview.linkTextAttributes mutableCopy];
            linkAttributes[NSUnderlineStyleAttributeName] = @(self.theme.bufLinkStyle);
            linkAttributes[NSForegroundColorAttributeName] = styles[style_Normal][NSForegroundColorAttributeName];
            _textview.linkTextAttributes = linkAttributes;

            [self showInsertionPoint];
            lastLineheight = self.theme.bufferNormal.font.boundingRectForFont.size.height;
            [self recalcBackground];
            if ([container hasMarginImages])
                [container performSelector:@selector(invalidateLayout:) withObject:nil afterDelay:0.2];
        }
        if (!_pendingScrollRestore) {
            _pendingScrollRestore = YES;
            [self flushDisplay];
            [self performSelector:@selector(restoreScroll:) withObject:nil afterDelay:0.2];
        }
    } else {
        if (!glkctl.isAlive) {
            NSRect frame = self.frame;

            if ((self.autoresizingMask & NSViewWidthSizable) == NSViewWidthSizable) {
                frame.size.width = glkctl.gameView.frame.size.width - frame.origin.x;
            }

            if ((self.autoresizingMask & NSViewHeightSizable) == NSViewHeightSizable) {
                frame.size.height = glkctl.gameView.frame.size.height - frame.origin.y;
            }
            self.frame = frame;
        }
        [self flushDisplay];
        [self recalcBackground];
        [self restoreScrollBarStyle];
    }
}

#pragma mark Output

- (void)clear {
    _pendingClear = YES;
    storedNewline = nil;
    bufferTextstorage = [[NSMutableAttributedString alloc] init];
    if (currentZColor && currentZColor.bg != zcolor_Current)
        bgnd = currentZColor.bg;
    [self recalcBackground];
    [self resetLastSpokenString];
}

- (void)reallyClear {
    [_textview resetTextFinder];
    fence = 0;
    _lastseen = 0;
    _lastchar = '\n';
    _printPositionOnInput = 0;
    [container clearImages];

    self.moveRanges = [[NSMutableArray alloc] init];
    moveRangeIndex = 0;
    [container invalidateLayout:nil];
    _pendingClear = NO;
}

- (void)clearScrollback:(id)sender {
    [self flushDisplay];
    if (storedNewline) {
        [textstorage appendAttributedString:storedNewline];
    }
    storedNewline = nil;

    NSString *string = textstorage.string;
    NSUInteger length = string.length;
    BOOL save_request = line_request;

    [_textview resetTextFinder];

    NSUInteger prompt;
    NSUInteger i;

    NSUInteger charsAfterFence = length - fence;
    BOOL found = NO;

    // find the second newline from the end
    for (i = 0; i < length; i++) {
        if ([string characterAtIndex:length - i - 1] == '\n' || [string characterAtIndex:length - i - 1] == '\r') {
            if (found) {
                break;
            } else {
                found = YES;
            }
        }
    }
    if (i < length) {
        prompt = i;
    } else {
        // Found no newline
        if (length > 1000) {
            prompt = 0;
        } else {
            prompt = length;
        }
    }

    line_request = NO;

    [textstorage deleteCharactersInRange:NSMakeRange(0, length - prompt)];

    _lastseen = 0;
    _lastchar = '\n';
    _printPositionOnInput = 0;
    if (textstorage.length < charsAfterFence)
        fence = 0;
    else
        fence = textstorage.length - charsAfterFence;

    line_request = save_request;

    [container clearImages];
    for (NSView *view in _textview.subviews)
        if ([view isKindOfClass:[MarginImage class]])
            [view removeFromSuperview];
    self.moveRanges = [[NSMutableArray alloc] init];
}

- (void)putString:(NSString *)str style:(NSUInteger)stylevalue {

    if (!str.length) {
        NSLog(@"Null string!");
        return;
    }

    if (bufferTextstorage.length > 50000)
        bufferTextstorage = [bufferTextstorage attributedSubstringFromRange:NSMakeRange(25000, bufferTextstorage.length - 25000)].mutableCopy;

    if (line_request)
        NSLog(@"Printing to text buffer window during line request");

    [self printToWindow:str style:stylevalue];

    if (self.glkctl.gameID == kGameIsDeadCities && line_request && [[str substringFromIndex:str.length - 1] isEqualToString:@"\n"]) {
        // This is against the Glk spec but makes
        // hyperlinks in Dead Cities work.
        // Turn this off by disabling game specific hacks in preferences.
        NSString *line = [textstorage.string substringFromIndex:fence];
        [self sendInputLine:line withTerminator:0];
    }
}

- (void)printToWindow:(NSString *)str style:(NSUInteger)stylevalue {

    if (self.glkctl.usesFont3 && str.length == 1 && stylevalue == style_BlockQuote) {
        NSDictionary *font3 = [self font3ToUnicode];
        NSString *newString = font3[str];
        if (newString) {
            str = newString;
            stylevalue = style_Normal;
        }
    }

    // With certain fonts and sizes, strings containing only spaces will "collapse."
    // So if the first character is a space, we replace it with a &nbsp;
    if (stylevalue == style_Preformatted && [str hasPrefix:@" "]) {
        const unichar nbsp = 0xa0;
        NSString *nbspstring = [NSString stringWithCharacters:&nbsp length:1];
        str = [str stringByReplacingCharactersInRange:NSMakeRange(0, 1) withString:nbspstring];
    }

    // A lot of code to not print single newlines
    // at the bottom
    if (storedNewline) {
        [bufferTextstorage appendAttributedString:storedNewline];
        storedNewline = nil;
    }

    NSMutableDictionary *attributes = [self getCurrentAttributesForStyle:stylevalue];

    if (str.length > 1) {
        unichar c = [str characterAtIndex:str.length - 1];
        if (c == '\n') {
            storedNewline = [[NSAttributedString alloc]
                             initWithString:@"\n"
                             attributes:attributes];

            str = [str substringWithRange:NSMakeRange(0, str.length - 1)];
        }
    }
    _lastchar = [str characterAtIndex:str.length - 1];

    NSAttributedString *attstr = [[NSAttributedString alloc]
                                  initWithString:str
                                  attributes:attributes];

    [bufferTextstorage appendAttributedString:attstr];
    dirty = YES;
    if (!_pendingClear && textstorage.length && bufferTextstorage.length) {
        [textstorage appendAttributedString:bufferTextstorage];
        bufferTextstorage = [[NSMutableAttributedString alloc] init];
    }
}

- (NSUInteger)unputString:(NSString *)buf {
    [self flushDisplay];
    NSUInteger result = 0;
    NSUInteger initialLength = textstorage.length;
    NSString *stringToRemove = [textstorage.string substringFromIndex:textstorage.length - buf.length].uppercaseString;
    if ([stringToRemove isEqualToString:buf.uppercaseString]) {
        [textstorage deleteCharactersInRange:NSMakeRange(textstorage.length - buf.length, buf.length)];
        result = initialLength - textstorage.length;
    }
    return result;
}

- (void)echo:(BOOL)val {
    if ((!(val) && echo) || (val && !(echo))) // Do we need to toggle echo?
        echo_toggle_pending = YES;
}

#pragma mark Input

- (void)keyDown:(NSEvent *)evt {
    NSString *str = evt.characters;
    unsigned ch = keycode_Unknown;
    if (str.length)
        ch = chartokeycode([str characterAtIndex:str.length - 1]);

    NSUInteger flags = evt.modifierFlags;

    if ((flags & NSEventModifierFlagNumericPad) && ch >= '0' && ch <= '9')
        ch = keycode_Pad0 - (ch - '0');

    GlkWindow *win;

    GlkController *glkctl = self.glkctl;
    // pass on this key press to another GlkWindow if we are not expecting one
    if (!self.wantsFocus) {
        //        NSLog(@"%ld does not want focus", self.name);
        for (win in (glkctl.gwindows).allValues) {
            if (win != self && win.wantsFocus) {
                NSLog(@"GlkTextBufferWindow %ld: Passing on keypress to %@ %ld", self.name, win.class, win.name);
                [win grabFocus];
                [win keyDown:evt];
                return;
            }
        }
    }

    BOOL commandKeyOnly = ((flags & NSEventModifierFlagCommand) &&
                           !(flags & (NSEventModifierFlagOption | NSEventModifierFlagShift |
                                      NSEventModifierFlagControl | NSEventModifierFlagHelp)));
    BOOL optionKeyOnly = ((flags & NSEventModifierFlagOption) &&
                          !(flags & (NSEventModifierFlagCommand | NSEventModifierFlagShift |
                                     NSEventModifierFlagControl | NSEventModifierFlagHelp)));

    if (ch == keycode_Up) {
        if (optionKeyOnly)
            ch = keycode_PageUp;
        else if (commandKeyOnly)
            ch = keycode_Home;
    } else if (ch == keycode_Down) {
        if (optionKeyOnly)
            ch = keycode_PageDown;
        else if (commandKeyOnly)
            ch = keycode_End;

    } else if (([str isEqualToString:@"f"] || [str isEqualToString:@"F"]) &&
             commandKeyOnly) {
        if (!scrollview.findBarVisible) {
            _restoredFindBarVisible = YES;
            [self restoreTextFinder];
            return;
        }
    }

    NSNumber *key = @(ch);
    BOOL scrolled = NO;

    if (self.glkctl.commandScriptRunning)
        [self scrollToBottomAnimated:NO];

    if (!scrolling && !_pendingScroll && !self.scrolledToBottom && !self.glkctl.voiceOverActive && !self.glkctl.commandScriptRunning) {
        // Not scrolled to the bottom, pagedown or navigate scrolling on each key instead
        switch (ch) {
            case keycode_PageUp:
                [_textview scrollPageUp:nil];
                return;
            case keycode_PageDown:
            case ' ':
                [_textview scrollPageDown:nil];
                return;
            case keycode_Up:
                [_textview scrollLineUp:nil];
                return;
            case keycode_Down:
            case keycode_Return:
                [_textview scrollLineDown:nil];
                return;
            case keycode_End:
                [self scrollToBottomAnimated:YES];
                return;
            default:
                if (line_request && !flags) {
                    [self scrollToBottomAnimated:NO];
                } else
                    [self performScroll];
                // To fix scrolling in the Adrian Mole games
                scrolled = YES;
                break;
        }
    }

    if (char_request && ch != keycode_Unknown) {
        // To fix scrolling in the Adrian Mole games
        if (!scrolled)
            glkctl.shouldScrollOnCharEvent = YES;

        [self sendKeypress:ch];

    } else if (line_request && (ch == keycode_Return ||
                                [self.currentTerminators[key] isEqual:@(YES)])) {
        NSString *line = [textstorage.string substringFromIndex:fence];
        [self sendInputLine:line withTerminator:ch == keycode_Return ? 0 : key.unsignedIntegerValue];
        return;
    } else if (line_request && (ch == keycode_Up ||
                                // Use Home to travel backward in history when Beyond Zork eats up arrow
                                ((self.glkctl.gameID == kGameIsBeyondZork || [self.glkctl zVersion6]) && self.theme.bZTerminator != kBZArrowsSwapped && ch == keycode_Home))) {
        [self travelBackwardInHistory];
    } else if (line_request && (ch == keycode_Down ||
                                // Use End to travel forward in history when Beyond Zork eats down arrow
                                ((self.glkctl.gameID == kGameIsBeyondZork || [self.glkctl zVersion6]) && self.theme.bZTerminator != kBZArrowsSwapped && ch == keycode_End))) {
        [self travelForwardInHistory];
    } else if (line_request && ch == keycode_PageUp &&
               fence == textstorage.length) {
        [_textview scrollPageUp:nil];
        return;
    }

    else {
        if (line_request) {
            if ((ch == 'v' || ch == 'V') && commandKeyOnly && _textview.selectedRange.location < fence) {
                [glkctl.window makeFirstResponder:_textview];
                NSRange selectedRange = NSIntersectionRange(_textview.selectedRange, self.editableRange);
                if (selectedRange.location == NSNotFound || selectedRange.length == 0)
                    selectedRange = NSMakeRange(textstorage.length, 0);
                _textview.selectedRange = selectedRange;
                [_textview paste:nil];
                return;
            } else if (_textview.selectedRange.length != 0 && ch != keycode_Unknown && (flags & NSEventModifierFlagCommand) != NSEventModifierFlagCommand) {
                // Deselect text and move cursor to end to facilitate typing
                NSRange selectedRange = NSIntersectionRange(_textview.selectedRange, self.editableRange);
                if (selectedRange.location == NSNotFound || (selectedRange.length == 0))
                    selectedRange = NSMakeRange(textstorage.length, 0);
                _textview.selectedRange = selectedRange;
            }
        }

        if (self.window.firstResponder != _textview)
            [self.window makeFirstResponder:_textview];

        // If there is no input request, buffer keyDown events
        // and reissue them on the next line input request
        if (!self.wantsFocus) {
            if (!bufferedEvents)
                bufferedEvents = [NSMutableArray new];
            [bufferedEvents addObject:evt];
        }
        [_textview superKeyDown:evt];
    }
}


- (void)sendCommandLine:(NSString *)line {
    if (echo) {
        NSAttributedString *att = [[NSAttributedString alloc]
                                   initWithString:line
                                   attributes:_inputAttributes];
        [textstorage appendAttributedString:att];
    }
    [self sendInputLine:line withTerminator:0];
}


- (void)sendInputLine:(NSString *)line withTerminator:(NSUInteger)terminator {
    // NSLog(@"line event from %ld", (long)self.name);
    if (echo) {
        //        [textstorage
        //         addAttribute:NSCursorAttributeName value:[NSCursor arrowCursor] range:[self editableRange]];
        [self printToWindow:@"\n"
                      style:style_Normal]; // XXX arranger lastchar needs to be set
        _lastchar = '\n';
    } else {
        [textstorage
         deleteCharactersInRange:NSMakeRange(fence,
                                             textstorage.length -
                                             fence)]; // Don't echo
    }
    // input line

    if (line.length > 0) {
        [history saveHistory:line];
    }

    line = [line scrubInvalidCharacters];
    line = [line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];

    if (self.glkctl.gameID == kGameIsBeyondZork || [self.glkctl zVersion6]) {
        if (terminator == keycode_Home) {
            terminator = keycode_Up;
        } else if (terminator == keycode_End) {
            terminator = keycode_Down;
        }
    }

    GlkEvent *gev = [[GlkEvent alloc] initLineEvent:line forWindow:self.name terminator:(NSInteger)terminator];
    [self.glkctl queueEvent:gev];

    fence = textstorage.length;
    line_request = NO;
    [self hideInsertionPoint];
    _textview.editable = NO;
    [self flushDisplay];
    [_textview resetTextFinder];
    [self.glkctl markLastSeen];
}

- (void)initChar {
    //    NSLog(@"GlkTextbufferWindow %ld initChar", (long)self.name);
    char_request = YES;
    [self hideInsertionPoint];
}

- (void)cancelChar {
    // NSLog(@"cancel char in %d", name);
    char_request = NO;
}

- (void)sendKeypress:(unsigned)ch {
    [self.glkctl markLastSeen];

    GlkEvent *gev = [[GlkEvent alloc] initCharEvent:ch forWindow:self.name];
    [self.glkctl queueEvent:gev];

    char_request = NO;
    _textview.editable = NO;
}

- (void)initLine:(NSString *)str maxLength:(NSUInteger)maxLength
{
    //    NSLog(@"initLine: %@ in: %ld", str, (long)self.name);
    [self flushDisplay];

    [history reset];

    if (self.terminatorsPending) {
        self.currentTerminators = self.pendingTerminators;
        self.terminatorsPending = NO;
    }

    if (echo_toggle_pending) {
        echo_toggle_pending = NO;
        echo = !echo;
    }

    if (_lastchar == '>' && self.theme.spaceFormat) {
        [self printToWindow:@" " style:style_Normal];
        _lastchar = ' ';
    }

    fence = textstorage.length;

    [self recalcInputAttributes];

    NSAttributedString *att = [[NSAttributedString alloc]
              initWithString:str
              attributes:_inputAttributes];

    [textstorage appendAttributedString:att];
    _pendingEditable = YES;
//    _textview.editable = YES;

    line_request = YES;
    [self showInsertionPoint];

    _textview.selectedRange = NSMakeRange(textstorage.length, 0);
    if (bufferedEvents.count)  {
        NSMutableArray *copiedEvents = bufferedEvents.copy;
        for (NSEvent *event in copiedEvents) {
            [self keyDown:event];
        }
        bufferedEvents = [NSMutableArray new];
    }
}

- (void)recalcInputAttributes {
    _inputAttributes = [self getCurrentAttributesForStyle:style_Input];
}

- (NSString *)cancelLine {
    [self flushDisplay];
    [_textview resetTextFinder];

    NSString *str = textstorage.string;
    str = [str substringFromIndex:fence];
    if (echo) {
        [self printToWindow:@"\n" style:style_Input];
        _lastchar = '\n'; // [str characterAtIndex: str.length - 1];
    } else
        [textstorage
         deleteCharactersInRange:NSMakeRange(
                                             fence,
                                             textstorage.length -
                                             fence)]; // Don't echo input line

    _textview.editable = NO;
    line_request = NO;
    [self hideInsertionPoint];
    return str;
}

- (BOOL)hasLineRequest {
    return line_request;
}

#pragma mark Command history

- (void)travelBackwardInHistory {
    [self flushDisplay];
    NSString *cx;
    if (textstorage.length > fence) {
        cx = [textstorage.string substringFromIndex:fence];
    } else {
        cx = @"";
    }

    cx = [history travelBackwardInHistory:cx];

    if (cx) {
        [textstorage
         replaceCharactersInRange:self.editableRange
         withString:cx];
        [_textview resetTextFinder];
    }

    if (!cx.length) {
        if ([history empty])
            [self.glkctl speakStringNow:@"No commands entered"];
        else
            [self.glkctl speakStringNow:@"Start of command history"];
    }
}

- (void)travelForwardInHistory {
    NSString *cx = [history travelForwardInHistory];
    if (cx) {
        [self flushDisplay];
        [textstorage
         replaceCharactersInRange:self.editableRange
         withString:cx];
        [_textview resetTextFinder];
    }

    if (!cx.length) {
        if ([history empty])
            [self.glkctl speakStringNow:@"No commands entered"];
        else
            [self.glkctl speakStringNow:@"End of command history"];
    }
}

#pragma mark Beyond Zork font

- (void)createBeyondZorkStyle {
    CGFloat pointSize = ((NSFont *)(styles[style_Normal][NSFontAttributeName])).pointSize;
    NSFont *zorkFont = [NSFont fontWithName:@"FreeFont3" size:pointSize];
    if (!zorkFont) {
        NSLog(@"Error! No Zork Font Found!");
        return;
    }

    NSMutableDictionary *beyondZorkStyle = [styles[style_BlockQuote] mutableCopy];

    beyondZorkStyle[NSBaselineOffsetAttributeName] = @(0);

    beyondZorkStyle[NSFontAttributeName] = zorkFont;

    NSSize size = [@"6" sizeWithAttributes:beyondZorkStyle];
    NSSize wSize = [@"W" sizeWithAttributes:styles[style_Normal]];

    NSAffineTransform *transform = [[NSAffineTransform alloc] init];
    [transform scaleBy:pointSize];

    CGFloat xscale = wSize.width / size.width;
    if (xscale < 1) xscale = 1;
    CGFloat yscale = wSize.height / size.height;
    if (yscale < 1) yscale = 1;

    [transform scaleXBy:xscale yBy:yscale];
    NSFontDescriptor *descriptor = [NSFontDescriptor fontDescriptorWithName:@"FreeFont3" size:pointSize];
    zorkFont = [NSFont fontWithDescriptor:descriptor textTransform:transform];
    if (!zorkFont)
        NSLog(@"Failed to create Zork Font!");
    beyondZorkStyle[NSFontAttributeName] = zorkFont;
    NSMutableParagraphStyle *para = [beyondZorkStyle[NSParagraphStyleAttributeName] mutableCopy];
    para.lineSpacing = 0;
    para.paragraphSpacing = 0;
    para.paragraphSpacingBefore = 0;
    para.maximumLineHeight = [layoutmanager defaultLineHeightForFont:self.theme.bufferNormal.font];
    beyondZorkStyle[NSParagraphStyleAttributeName] = para;
    beyondZorkStyle[NSKernAttributeName] = @(-2);

    styles[style_BlockQuote] = beyondZorkStyle;
}

- (NSDictionary *)font3ToUnicode {
    return @{
        @"!" : @"←",
        @"\"" : @"→",
        @"\\" : @"↑",
        @"]" : @"↓",
        @"a" : @"ᚪ",
        @"b" : @"ᛒ",
        @"c" : @"ᛇ",
        @"d" : @"ᛞ",
        @"e" : @"ᛖ",
        @"f" : @"ᚠ",
        @"g" : @"ᚷ",
        @"h" : @"ᚻ",
        @"i" : @"ᛁ",
        @"j" : @"ᛄ",
        @"k" : @"ᛦ",
        @"l" : @"ᛚ",
        @"m" : @"ᛗ",
        @"n" : @"ᚾ",
        @"o" : @"ᚩ",
        @"p" : @"ᖾ",
        @"q" : @"ᚳ",
        @"r" : @"ᚱ",
        @"s" : @"ᛋ",
        @"t" : @"ᛏ",
        @"u" : @"ᚢ",
        @"v" : @"ᛠ",
        @"w" : @"ᚹ",
        @"x" : @"ᛉ",
        @"y" : @"ᚥ",
        @"z" : @"ᛟ"
    };
}

#pragma mark NSTextView customization

- (BOOL)textView:(NSTextView *)aTextView
shouldChangeTextInRange:(NSRange)range
replacementString:(id)repl {
    if (line_request && range.location >= fence) {
        _textview.shouldDrawCaret = YES;
        return YES;
    }
    if (line_request && range.location == fence - 1)
        _textview.shouldDrawCaret = YES;
    else
        _textview.shouldDrawCaret = NO;
    return NO;
}


- (void)textStorage:(NSTextStorage *)textStorage willProcessEditing:(NSTextStorageEditActions)editedMask range:(NSRange)editedRange changeInLength:(NSInteger)delta {
    if ((editedMask & NSTextStorageEditedCharacters) == 0)
        return;
    if (!line_request)
        return;

    if (textstorage.editedRange.location < fence)
        return;

    if (!_inputAttributes)
        [self recalcInputAttributes];

    [textstorage setAttributes:_inputAttributes
                         range:textstorage.editedRange];
}

- (NSRange)textView:(NSTextView *)aTextView willChangeSelectionFromCharacterRange:(NSRange)oldrange
   toCharacterRange:(NSRange)newrange {

    if (line_request) {
        if (newrange.length == 0)
            if (newrange.location < fence)
                newrange.location = fence;
    } else {
        if (newrange.length == 0)
            newrange.location = textstorage.length;
    }
    return newrange;
}

- (void)hideInsertionPoint {
    if (!line_request) {
        NSColor *color = _textview.backgroundColor;
        if (textstorage.length) {
            color = [textstorage attribute:NSBackgroundColorAttributeName atIndex:textstorage.length-1 effectiveRange:nil];
        }
        if (!color) {
            color = self.theme.bufferBackground;
        }
        _textview.insertionPointColor = color;
    }
}

- (void)showInsertionPoint {
    if (line_request) {
        NSColor *color = styles[style_Normal][NSForegroundColorAttributeName];
        if (currentZColor)
            color = [NSColor colorFromInteger: currentZColor.fg];
        if (textstorage.length && [color isEqualToColor:_textview.backgroundColor]) {
            if (fence <= textstorage.length && fence > 0)
                color = [textstorage attribute:NSForegroundColorAttributeName atIndex:fence - 1 effectiveRange:nil];
            else
                color = [textstorage attribute:NSForegroundColorAttributeName atIndex:0 effectiveRange:nil];
        }
        _textview.insertionPointColor = color;
    }
}

- (NSRange)editableRange {
    if (!line_request)
        return NSMakeRange(NSNotFound, 0);
    return NSMakeRange(fence, textstorage.length - fence);
}

// Assumes a fixed-width style is used,
// as in Tads status bars.
- (NSUInteger)numberOfColumns {
    NSRect frame = self.frame;
    if (self.framePending)
        frame = self.pendingFrame;
    CGFloat charwidth = [@"l" sizeWithAttributes:styles[style_Normal]].width;
    NSUInteger cols = (NSUInteger)round((frame.size.width -
                                         (_textview.textContainerInset.width + container.lineFragmentPadding) * 2) /
                                        charwidth);
    return cols;
}

- (NSUInteger)numberOfLines {
    [self flushDisplay];
    NSUInteger numberOfLines, index, numberOfGlyphs =
    layoutmanager.numberOfGlyphs;
    NSRange lineRange;
    for (numberOfLines = 0, index = 0; index < numberOfGlyphs; numberOfLines++){
        [layoutmanager lineFragmentRectForGlyphAtIndex:index
                                        effectiveRange:&lineRange];
        index = NSMaxRange(lineRange);
    }
    return numberOfLines;
}

#pragma mark Text finder

- (void)restoreTextFinder {
    BOOL waseditable = _textview.editable;
    _textview.editable = NO;
    _textview.usesFindBar = YES;

    NSTextFinder *newFinder = _textview.textFinder;
    newFinder.client = _textview;
    newFinder.findBarContainer = scrollview;
    newFinder.incrementalSearchingEnabled = YES;
    newFinder.incrementalSearchingShouldDimContentView = NO;

    if (_restoredFindBarVisible) {
        NSLog(@"Restoring textFinder");
        [newFinder performAction:NSTextFinderActionShowFindInterface];
        NSSearchField *searchField = [self findSearchFieldIn:self];
        if (searchField) {
            if (_restoredSearch)
                searchField.stringValue = _restoredSearch;
            [newFinder cancelFindIndicator];
            [self.glkctl.window makeFirstResponder:_textview];
            [searchField sendAction:searchField.action to:searchField.target];
        }
    }
    _textview.editable = waseditable;
}

- (void)destroyTextFinder {
    NSView *aView = [self findSearchFieldIn:scrollview];
    if (aView) {
        while (aView.superview != scrollview)
            aView = aView.superview;
        [aView removeFromSuperview];
        NSLog(@"Destroyed textFinder!");
    }
}

- (NSSearchField *)findSearchFieldIn:
(NSView *)theView // search the subviews for a view of class NSSearchField
{
    NSSearchField __block __weak *(^weak_findSearchField)(NSView *);

    NSSearchField * (^findSearchField)(NSView *);

    weak_findSearchField = findSearchField = ^(NSView *view) {
        if ([view isKindOfClass:[NSSearchField class]])
            return (NSSearchField *)view;
        NSSearchField __block *foundView = nil;
        [view.subviews enumerateObjectsUsingBlock:^(
                                                    NSView *subview, NSUInteger idx, BOOL *stop) {
                                                        foundView = weak_findSearchField(subview);
                                                        if (foundView)
                                                            *stop = YES;
                                                    }];
        return foundView;
    };

    return findSearchField(theView);
}

#pragma mark images

- (NSImage *)scaleImage:(NSImage *)src size:(NSSize)dstsize {
    NSSize srcsize = src.size;
    NSImage *dst;

    if (NSEqualSizes(srcsize, dstsize))
        return src;

    dst = [[NSImage alloc] initWithSize:dstsize];
    [dst lockFocus];

    [NSGraphicsContext currentContext].imageInterpolation =
    NSImageInterpolationHigh;

    [src drawInRect:NSMakeRect(0, 0, dstsize.width, dstsize.height)
           fromRect:NSMakeRect(0, 0, srcsize.width, srcsize.height)
          operation:NSCompositingOperationSourceOver
           fraction:1.0
     respectFlipped:YES
              hints:nil];

    [dst unlockFocus];

    return dst;
}

- (NSTextAttachment *)textAttachmenWithImage:(NSImage *)image alignment:(NSInteger)alignment index:(NSInteger)index position:(NSUInteger)position {
    NSTextAttachment *att = [[NSTextAttachment alloc] initWithData:nil ofType:nil];
    MyAttachmentCell *cell =
    [[MyAttachmentCell alloc] initImageCell:image
                               andAlignment:alignment
                                  andAttStr:textstorage
                                         at:position
                                      index:index];
    att.attachmentCell = cell;
    return att;
}

- (void)drawImage:(NSImage *)image
             val1:(NSInteger)alignment
             val2:(NSInteger)index
            width:(NSInteger)w
           height:(NSInteger)h
            style:(NSUInteger)style {
    [self flushDisplay];

    if (storedNewline) {
        [textstorage appendAttributedString:storedNewline];
        storedNewline = nil;
        _lastchar = '\n';
    }

    if (w == 0)
        w = (NSInteger)image.size.width;
    if (h == 0)
        h = (NSInteger)image.size.height;

    image = [self scaleImage:image size:NSMakeSize(w, h)];

    if (textstorage.length == 0 && (alignment == imagealign_MarginLeft || alignment == imagealign_MarginRight)) {
        [textstorage appendAttributedString:[[NSAttributedString alloc] initWithString:@"\u00AD" attributes:styles[style]]];
        _lastchar = '\n';
    }

    MyAttachmentCell *cell =
    [[MyAttachmentCell alloc] initImageCell:image
                               andAlignment:alignment
                                  andAttStr:textstorage
                                         at:textstorage.length
                                      index:index];

    if (alignment == imagealign_MarginLeft || alignment == imagealign_MarginRight) {
        if (_lastchar != '\n') {
            NSLog(@"lastchar is not line break. Do not add margin image.");
            return;
        }

        [container addImage:image alignment:alignment at:textstorage.length linkid:(NSUInteger)self.currentHyperlink];
        cell.marginImage = container.marginImages.lastObject;
        cell.marginImgUUID = cell.marginImage.uuid;
    }

    NSTextAttachment *att = [[NSTextAttachment alloc] initWithData:nil ofType:nil];
    att.attachmentCell = cell;
    NSAttributedString *attstr = [NSAttributedString
                                  attributedStringWithAttachment:att];

    [textstorage appendAttributedString:attstr];

    if (self.currentHyperlink) {
        [textstorage addAttribute:NSLinkAttributeName value:@(self.currentHyperlink) range:NSMakeRange(textstorage.length - 1, 1)];
    }

    [textstorage addAttributes:styles[style] range:NSMakeRange(textstorage.length - 1, 1)];
}

- (void)flowBreak {
    [self flushDisplay];
    [_textview resetTextFinder];

    // NSLog(@"adding flowbreak");
    unichar uc = NSAttachmentCharacter;
    [textstorage.mutableString appendString:[NSString stringWithCharacters:&uc
                                                                    length:1]];
    [container flowBreakAt:textstorage.length - 1];
}

- (void)textView:(NSTextView *)view
     draggedCell:(id<NSTextAttachmentCell>)cell
          inRect:(NSRect)rect
           event:(NSEvent *)event
         atIndex:(NSUInteger)charIndex {
    _textview.selectedRange = NSMakeRange(0, 0);
    if ([cell isKindOfClass:[MyAttachmentCell class]]) {
        MyAttachmentCell *attachment = (MyAttachmentCell *)cell;
        NSString *filename = self.glkctl.game.path.lastPathComponent.stringByDeletingPathExtension;
        [attachment dragTextAttachmentFrom:view event:event filename:filename inRect:rect];
    }
}

- (void)updateImageAttachmentsWithXScale:(CGFloat)xscale yScale:(CGFloat)yscale {

    if (xscale == 0 || yscale == 0)
        return;

    NSMutableDictionary<NSString *, MarginImage *> *marginImages = [[NSMutableDictionary alloc] initWithCapacity:container.marginImages.count];
    for (MarginImage *marginImage in container.marginImages) {
        marginImages[marginImage.uuid] = marginImage;
    }

    [textstorage
     enumerateAttribute:NSAttachmentAttributeName
     inRange:NSMakeRange(0, textstorage.length)
     options:0
     usingBlock:^(NSTextAttachment *value, NSRange subrange, BOOL *stop) {
        if (!value) {
            return;
        }
        MyAttachmentCell *cell = (MyAttachmentCell *)value.attachmentCell;

        NSImage *img = nil;
        BOOL imageIsMargin = (cell.glkImgAlign == imagealign_MarginLeft || cell.glkImgAlign == imagealign_MarginRight);

        if (cell && [self.glkctl.imageHandler handleFindImageNumber:cell.index]) {
            CGFloat blockXScale = xscale;
            CGFloat blockYScale = yscale;

            img = self.glkctl.imageHandler.lastimage;
            if (!imageIsMargin && cell.image && cell.image.size.width > scrollview.contentView.frame.size.width * 0.7) {
                CGFloat width = scrollview.contentView.frame.size.width;
                CGFloat factor = img.size.width * xscale / width;
                blockXScale *= factor;
                blockYScale *= factor;
            }
            img = [self scaleImage:img size:NSMakeSize(img.size.width * blockXScale, img.size.height * blockYScale)];
        } else {
            return;
        }

        // Replace non-margin inline images (alignment imagealign_InlineUp, imagealign_InlineDown, or imagealign_InlineCenter)
        if (!imageIsMargin) {
            NSTextAttachment *att = [self textAttachmenWithImage:img alignment:cell.glkImgAlign index:cell.index position:subrange.location];
            [textstorage addAttribute:NSAttachmentAttributeName value:att range:subrange];
            return;
        }

        MarginImage *mimg = marginImages[cell.marginImgUUID];
        if (mimg == nil) {
            NSLog(@"updateImageAttachments: Could not find margin image with uuid %@", cell.marginImgUUID);
            return;
        }

        [container.marginImages removeObject:mimg];
        [container addImage:img alignment:mimg.glkImgAlign at:subrange.location linkid:0];
        cell.marginImage = container.marginImages.lastObject;
        cell.marginImgUUID = cell.marginImage.uuid;
    }];
}

#pragma mark Hyperlinks

- (void)initHyperlink {
    hyper_request = YES;
    //    NSLog(@"txtbuf: hyperlink event requested");
}

- (void)cancelHyperlink {
    hyper_request = NO;
    //    NSLog(@"txtbuf: hyperlink event cancelled");
}

// Make margin image links clickable
- (BOOL)myMouseDown:(NSEvent *)theEvent {
    GlkEvent *gev;

    // Don't draw a caret right now, even if we clicked at the prompt
    [_textview temporarilyHideCaret];

    // NSLog(@"mouseDown in buffer window.");
    if (hyper_request) {
        [self.glkctl markLastSeen];

        NSPoint p;
        p = theEvent.locationInWindow;
        p = [_textview convertPoint:p fromView:nil];
        p.x -= _textview.textContainerInset.width;
        p.y -= _textview.textContainerInset.height;

        NSUInteger linkid = [container findHyperlinkAt:p];
        if (linkid) {
            NSLog(@"Clicked margin image hyperlink in buf at %g,%g", p.x, p.y);
            gev = [[GlkEvent alloc] initLinkEvent:linkid forWindow:self.name];
            [self.glkctl queueEvent:gev];
            hyper_request = NO;
            [self colderLightHack];
            return YES;
        }
    }
    return NO;
}

- (BOOL)textView:(NSTextView *)textview_
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
    //    NSLog(@"txtbuf: clicked on link: %@", link);

    if (!hyper_request) {
        NSLog(@"txtbuf: No hyperlink request in window.");
        //        return NO;
    }

    [self.glkctl markLastSeen];

    GlkEvent *gev =
    [[GlkEvent alloc] initLinkEvent:((NSNumber *)link).unsignedIntegerValue
                          forWindow:self.name];
    [self.glkctl queueEvent:gev];

    hyper_request = NO;
    [self colderLightHack];
    return YES;
}

- (void)colderLightHack {

    GlkController *glkctl = self.glkctl;
    // Send an arrange event to The Colder Light in order
    // to make it update its title bar
    if (glkctl.gameID == kGameIsAColderLight) {
        GlkEvent *gev = [[GlkEvent alloc] initArrangeWidth:(NSInteger)glkctl.gameView.frame.size.width
                                                    height:(NSInteger)glkctl.gameView.frame.size.height
                                                     theme:glkctl.theme
                                                     force:YES];
        [glkctl queueEvent:gev];
    }
}

#pragma mark Scrolling

- (void)forceLayout{
//    if (textstorage.length < 50000 && container.marginImages.count < 40 && !self.inLiveResize) {
//        if (!self.inLiveResize) {
//
//        //        [layoutmanager ensureLayoutForTextContainer:container];
//        NSUInteger length = MIN(textstorage.length , 1000);
//
//        [layoutmanager ensureLayoutForGlyphRange:NSMakeRange(textstorage.length - length, length)];
//    }

}

- (void)markLastSeen {
    NSRange glyphs;
    NSRect line;

    _printPositionOnInput = textstorage.length;
    if (fence > 0 && !char_request) {
        _printPositionOnInput = fence;
    }

    if (textstorage.length == 0) {
        _lastseen = 0;
        return;
    }

    glyphs = [layoutmanager glyphRangeForTextContainer:container];

    if (glyphs.length) {
        line = [layoutmanager
                lineFragmentRectForGlyphAtIndex:NSMaxRange(glyphs) - 1
                effectiveRange:nil];

        _lastseen = (NSInteger)ceil(NSMaxY(line)); // bottom of the line
        // NSLog(@"GlkTextBufferWindow: markLastSeen: %ld", (long)_lastseen);
    }
}

- (void)storeScrollOffset {
    //    NSLog(@"GlkTextBufferWindow %ld: storeScrollOffset", self.name);
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

    [self forceLayout];

    NSRect visibleRect = scrollview.documentVisibleRect;

    lastVisible = [layoutmanager characterIndexForPoint:NSMakePoint(NSMaxX(visibleRect),
                                                                    NSMaxY(visibleRect))
                                        inTextContainer:container
               fractionOfDistanceBetweenInsertionPoints:nil];

    if (lastVisible != 0)
        lastVisible--;
    if (lastVisible >= textstorage.length) {
        NSLog(@"lastCharacter index (%ld) is outside textstorage length (%ld)",
              lastVisible, textstorage.length);
        lastVisible = textstorage.length - 1;
    }

    NSRect lastRect =
    [layoutmanager lineFragmentRectForGlyphAtIndex:lastVisible
                                    effectiveRange:nil];

    lastScrollOffset = (NSMaxY(visibleRect) - NSMaxY(lastRect)) / self.theme.bufferCellHeight;

    if (isnan(lastScrollOffset) || isinf(lastScrollOffset))
        lastScrollOffset = 0;

    //    NSLog(@"lastScrollOffset: %f", lastScrollOffset);
    //    NSLog(@"lastScrollOffset as percentage of cell height: %f", (lastScrollOffset / self.theme.bufferCellHeight) * 100);
}

- (void)restoreScroll:(id)sender {
    if (_textview.inLiveResize)
        return;
    _pendingScrollRestore = NO;
    _pendingScroll = NO;
    //    NSLog(@"GlkTextBufferWindow %ld restoreScroll", self.name);
    //    NSLog(@"lastVisible: %ld lastScrollOffset:%f", lastVisible, lastScrollOffset);
    if (_textview.bounds.size.height <= scrollview.bounds.size.height) {
        //        NSLog(@"All of textview fits in scrollview. Returning without scrolling");
        if (_textview.bounds.size.height == scrollview.bounds.size.height) {
            _textview.frame = self.bounds;
        }
        return;
    }

    if (lastAtBottom) {
        [self scrollToBottomAnimated:NO];
        return;
    }

    if (lastAtTop) {
        [self scrollToTop];
        return;
    }

    if (!lastVisible) {
        return;
    }

    [self scrollToCharacter:lastVisible withOffset:lastScrollOffset animate:NO];
    return;
}

- (void)scrollToCharacter:(NSUInteger)character withOffset:(CGFloat)offset animate:(BOOL)animate {
//    NSLog(@"GlkTextBufferWindow %ld: scrollToCharacter %ld withOffset: %f", self.name, character, offset);

    CGFloat charHeight = self.theme.bufferCellHeight;
    if (pauseScrolling)
        return;

    NSRect line;

    if (character >= textstorage.length - 1 || !textstorage.length) {
        return;
    }

    offset = offset * charHeight;
    // first, force a layout so we have the correct textview frame
    [layoutmanager glyphRangeForTextContainer:container];

    line = [layoutmanager lineFragmentRectForGlyphAtIndex:character
                                           effectiveRange:nil];

    CGFloat charbottom = NSMaxY(line); // bottom of the line
    if (fabs(charbottom - NSHeight(scrollview.frame)) < charHeight && NSHeight(scrollview.frame) / charHeight > 3) {
        //        NSLog(@"scrollToCharacter: too close to the top!");
        [self scrollToTop];
        return;
    }
    charbottom = charbottom + offset;
    [self scrollToPosition:floor(charbottom - NSHeight(scrollview.frame)) animate:animate];
}

- (void)performScroll {
    if (_pendingScrollRestore)
        return;
    _pendingScroll = YES;
}

- (void)reallyPerformScroll {
    _pendingScroll = NO;
    self.glkctl.shouldScrollOnCharEvent = NO;

    if (pauseScrolling)
        return;

    if (!textstorage.length)
        return;

//    if (textstorage.length < 1000000)
//        // first, force a layout so we have the correct textview frame
//        [layoutmanager ensureLayoutForTextContainer:container];
    [self forceLayout];

    // then, get the bottom
    CGFloat bottom = NSHeight(_textview.frame);

    BOOL animate = !self.glkctl.commandScriptRunning;
    if (bottom - _lastseen > NSHeight(scrollview.frame)) {
        [self scrollToPosition:_lastseen animate:animate];
    } else {
        [self scrollToBottomAnimated:animate];
    }
}

- (BOOL)scrolledToBottom {
    NSView *clipView = scrollview.contentView;

    // At least the start screen of Kerkerkruip uses a buffer window
    // with height 0 to catch key input.
    if (!clipView || NSHeight(clipView.bounds) == 0) {
        return YES;
    }

    return (NSHeight(_textview.bounds) - NSMaxY(clipView.bounds) <
            2 + _textview.textContainerInset.height + _textview.bottomPadding);
}

- (void)scrollToBottomAnimated:(BOOL)animate {
    lastAtTop = NO;
    lastAtBottom = YES;

    // first, force a layout so we have the correct textview frame
    [self forceLayout];
    NSPoint newScrollOrigin = NSMakePoint(0, NSMaxY(_textview.frame) - NSHeight(scrollview.contentView.bounds));

    [self scrollToPosition:newScrollOrigin.y animate:animate];
}

- (void)scrollToPosition:(CGFloat)position animate:(BOOL)animate {
    if (pauseScrolling)
        return;
    NSClipView* clipView = scrollview.contentView;
    NSRect newBounds = clipView.bounds;
    CGFloat diff = position - newBounds.origin.y;
    if (newBounds.origin.y > position)
        return;
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

- (BOOL)scrolledToTop {
    NSView *clipView = scrollview.contentView;
    if (!clipView) {
        return NO;
    }
    CGFloat diff = clipView.bounds.origin.y;
    return (diff < self.theme.bufferCellHeight);
}

- (void)scrollToTop {
    if (pauseScrolling)
        return;
    lastAtTop = YES;
    lastAtBottom = NO;

    [scrollview.contentView scrollToPoint:NSZeroPoint];
}

#pragma mark Speech

- (BOOL)setLastMove {
    NSUInteger maxlength = textstorage.length;

    if (!maxlength) {
        self.moveRanges = [[NSMutableArray alloc] init];
        moveRangeIndex = 0;
        _printPositionOnInput = 0;
        return NO;
    }

    if (self.glkctl.showingInfocomV6Menu) {
        [self.glkctl.infocomV6MenuHandler updateMoveRanges:self];
        return YES;
    }

    NSRange allText = NSMakeRange(0, maxlength);
    NSRange currentMove = allText;

    if (self.moveRanges.lastObject) {
        NSRange lastMove = self.moveRanges.lastObject.rangeValue;
        if (NSMaxRange(lastMove) > maxlength) {
            // Removing last move object
            // (because it goes past the end)
            [self.moveRanges removeLastObject];
        } else if (lastMove.length == maxlength) {
            return NO;
        } else {
            if (lastMove.location == _printPositionOnInput && lastMove.length != maxlength - _printPositionOnInput) {
                // Removing last move object (because it does not go all
                // the way to the end)
                [self.moveRanges removeLastObject];
                currentMove = NSMakeRange(_printPositionOnInput, maxlength);
            } else {
                currentMove = NSMakeRange(NSMaxRange(lastMove),
                                          maxlength);
            }
        }
    }

    currentMove = NSIntersectionRange(allText, currentMove);
    if (currentMove.length == 0)
        return NO;
    NSString *string = [textstorage.string substringWithRange:currentMove];
    string = [string stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (string.length == 0)
        return NO;
    moveRangeIndex = self.moveRanges.count;
    [self.moveRanges addObject:[NSValue valueWithRange:currentMove]];
    _lastNewTextOnTurn = self.glkctl.turns;
    return YES;
}

- (NSString *)stringFromRangeVal:(NSValue *)val {
    NSRange range = val.rangeValue;
    NSAttributedString *attStr = [_textview accessibilityAttributedStringForRange:range];
    NSMutableString *string = attStr.string.mutableCopy;

    if (self.theme.vOSpeakImages != kVOImageNone) {
        // Look for image attachments and add their descriptions
        // according to settings.
        NSArray *keys;
        NSDictionary *attachments = [self attachmentsInRange:range withKeys:&keys];
        NSUInteger offset = 0;
        for (NSNumber *num in keys) {
            NSUInteger index = (NSUInteger)num.intValue - range.location + offset;
            MyAttachmentCell *cell = (MyAttachmentCell *)((NSTextAttachment *)attachments[num]).attachmentCell;
            NSString *label = cell.customA11yLabel;
            if (self.theme.vOSpeakImages != kVOImageWithDescriptionOnly || cell.hasDescription) {
                label = [NSString stringWithFormat:@"(%@)", label];
                [string insertString:label atIndex:index];
                offset += label.length;
            }
        }
    }

    // Strip command line if the speak command setting is off
    if (!self.glkctl.theme.vOSpeakCommand && range.location != 0 && !self.glkctl.showingInfocomV6Menu) {
        NSUInteger promptIndex = range.location - 1;
        if ([textstorage.string characterAtIndex:promptIndex] == '>' || (promptIndex > 0 && [textstorage.string characterAtIndex:promptIndex - 1] == '>')) {
            NSRange foundRange = [string rangeOfString:@"\n"];
            if (foundRange.location != NSNotFound) {
                string = [string substringFromIndex:foundRange.location].mutableCopy;
            }
        }
    }


    return string;
}

- (NSString *)lastMoveString {
    NSString *str = @"";

    if (self.moveRanges.count) {
        moveRangeIndex = self.moveRanges.count - 1;
        str = [self stringFromRangeVal:self.moveRanges.lastObject];
    }
    return str;
}

- (void)resetLastSpokenString {
    lastSpokenRange = NSMakeRange(0, 0);
    lastSpokenString = @"";
}

- (void)repeatLastMove:(id)sender {
    GlkController *glkctl = self.glkctl;
    if (glkctl.zmenu)
        [NSObject cancelPreviousPerformRequestsWithTarget:glkctl.zmenu];
    if (glkctl.form)
        [NSObject cancelPreviousPerformRequestsWithTarget:glkctl.form];

    NSString *str = [self lastMoveString];

    if (glkctl.quoteBoxes.count) {
        GlkTextGridWindow *box = glkctl.quoteBoxes.lastObject;

        str = [box.textview.string stringByAppendingString:str];
        str = [@"QUOTE: \n\n" stringByAppendingString:str];
    }

    // sender is only set to self.glkctl if repeatLastMove is called by
    // the GlkController after entering a command, or the window gets focus,
    // or VoiceOver is activated, i.e. not in response to the player
    // using the shortcut or the menu item.
    if (!str.length && sender != glkctl) {
        [glkctl speakStringNow:@"No last move to speak"];
        return;
    }

    // The GlkController might sometimes think there
    // is new text to speak after a key event even if there isn't,
    // so we perform an extra check for that here.
    if (sender == glkctl && NSEqualRanges(lastSpokenRange, self.moveRanges.lastObject.rangeValue) && [str isEqualToString:lastSpokenString]) {
        return;
    } else {
        lastSpokenRange = self.moveRanges.lastObject.rangeValue;
        lastSpokenString = str;
    }

    [glkctl speakString:str];
}

- (void)speakPrevious {
    //    NSLog(@"GlkTextBufferWindow %ld speakPrevious:", self.name);
    if (!self.moveRanges.count)
        return;
    NSString *prefix = @"";
    if (moveRangeIndex > 0) {
        moveRangeIndex--;
    } else {
        prefix = @"At first move.\n";
        moveRangeIndex = 0;
    }
    NSString *str = [prefix stringByAppendingString:[self stringFromRangeVal:self.moveRanges[moveRangeIndex]]];
    [self.glkctl speakStringNow:str];
}

- (void)speakNext {
    //    NSLog(@"GlkTextBufferWindow %ld speakNext:", self.name);
    [self setLastMove];
    if (!self.moveRanges.count) {
        return;
    }

    NSString *prefix = @"";

    if (moveRangeIndex < self.moveRanges.count - 1) {
        moveRangeIndex++;
    } else {
        prefix = @"At last move.\n";
        moveRangeIndex = self.moveRanges.count - 1;
    }

    NSString *str = [prefix stringByAppendingString:[self stringFromRangeVal:self.moveRanges[moveRangeIndex]]];
    [self.glkctl speakStringNow:str];
}

- (void)speakStatus {
    GlkController *glkctl = self.glkctl;
    if (glkctl.zmenu)
        [NSObject cancelPreviousPerformRequestsWithTarget:glkctl.zmenu];
    if (glkctl.form)
        [NSObject cancelPreviousPerformRequestsWithTarget:glkctl.form];
    [glkctl speakStringNow:textstorage.string];
}

- (void)movesRangesFromV6Menu:(NSArray<NSString *> *)menuStrings {
    self.moveRanges = [[NSMutableArray<NSValue *> alloc] initWithCapacity:menuStrings.count];
    moveRangeIndex = menuStrings.count - 1;
    [self flushDisplay];
    for (NSString *str in menuStrings) {
        NSRange range = [textstorage.string rangeOfString:str];
        if (range.location != NSNotFound) {
            [self.moveRanges addObject:[NSValue valueWithRange:range]];
        }
    }
}

#pragma mark Accessibility

- (BOOL)isAccessibilityElement {
    return NO;
}

- (NSArray <NSValue *> *)links {
    NSRange allText = NSMakeRange(0, textstorage.length);
    if (self.moveRanges.count < 2)
        return [self linksInRange:allText];
    NSMutableArray <NSValue *> *links = [[NSMutableArray alloc] init];

    // Make sure that no text after last moveRange slips through
    NSRange lastMoveRange = self.moveRanges.lastObject.rangeValue;
    NSRange stubRange = NSMakeRange(NSMaxRange(lastMoveRange), textstorage.length);
    stubRange = NSIntersectionRange(allText, stubRange);
    if (stubRange.length) {
        [links addObjectsFromArray:[self linksInRange:stubRange]];
    }

    for (NSValue *rangeVal in [self.moveRanges reverseObjectEnumerator])
    {
        // Print some info
        [links addObjectsFromArray:[self linksInRange:rangeVal.rangeValue]];
        if (links.count > 15)
            break;
    }
    if (links.count > 15)
        links.array = [links subarrayWithRange:NSMakeRange(0, 15)];
    return links;
}

- (NSArray <NSValue *> *)linksInRange:(NSRange)range {
    NSMutableArray <NSValue *> __block *links = [[NSMutableArray alloc] init];
    [textstorage
     enumerateAttribute:NSLinkAttributeName
     inRange:range
     options:0
     usingBlock:^(id value, NSRange subrange, BOOL *stop) {
        if (!value) {
            return;
        }
        [links addObject:[NSValue valueWithRange:subrange]];
    }];
    return links;
}

- (NSArray<NSValue *> *)images {
    if (self.theme.vOSpeakImages == kVOImageNone)
        return @[];
    NSArray<NSValue *> *images = [self imagesInRange:NSMakeRange(0,_textview.string.length)];
    return images;
}

- (NSArray<NSValue *> *)imagesInRange:(NSRange)range {
    NSMutableArray<NSValue *> __block *images = [NSMutableArray new];
    BOOL withDescOnly = (self.theme.vOSpeakImages == kVOImageWithDescriptionOnly);

    [textstorage
     enumerateAttribute:NSAttachmentAttributeName
     inRange:range
     options:0
     usingBlock:^(id value, NSRange subrange, BOOL *stop) {
        if (!value) {
            return;
        }
        MyAttachmentCell *cell = (MyAttachmentCell *)((NSTextAttachment *)value).attachmentCell;
        if (withDescOnly && cell.hasDescription == NO)
            return;
        if (subrange.length < 2 && (cell.alignment == imagealign_MarginRight || cell.alignment == imagealign_MarginLeft))
            subrange.length = 2;
        [images addObject:[NSValue valueWithRange:subrange]];
    }];

    return images;
}

- (NSDictionary <NSNumber *, NSTextAttachment *> *)attachmentsInRange:(NSRange)range withKeys:(NSArray * __autoreleasing *)keys {
    NSMutableDictionary <NSNumber *, NSTextAttachment *> __block *attachments = [NSMutableDictionary new];
    NSMutableArray __block *mutKeys = [NSMutableArray new];
    range = NSIntersectionRange(range, NSMakeRange(0, textstorage.length));
    [textstorage
     enumerateAttribute:NSAttachmentAttributeName
     inRange:range
     options:0
     usingBlock:^(id value, NSRange subrange, BOOL *stop) {
        if (!value) {
            return;
        }
        [mutKeys addObject:@(subrange.location)];
        attachments[mutKeys.lastObject] = value;
    }];
    *keys = mutKeys;
    return attachments;
}

@end
