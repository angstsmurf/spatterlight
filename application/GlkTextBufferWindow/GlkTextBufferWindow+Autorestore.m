#import "GlkTextBufferWindowPrivate.h"

#import "GlkController.h"
#import "GlkTextGridWindow.h"
#import "Theme.h"
#import "MarginContainer.h"
#import "MarginImage.h"
#import "MyAttachmentCell.h"
#import "BufferTextView.h"
#include "glkimp.h"

// In release builds, suppress NSLog entirely.
#ifndef DEBUG
#define NSLog(...)
#endif // DEBUG

@implementation GlkTextBufferWindow (Autorestore)

// Deserialize a text buffer window from an autosave archive. Reconstructs the
// text system from the archived BufferTextView (which carries its text storage,
// layout manager, and container), then restores all input state, scroll position,
// find bar state, and pending operations.
- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        rewrapLastseenChar = NSNotFound;
        _textview = [decoder decodeObjectOfClass:[BufferTextView class] forKey:@"textview"];
        layoutmanager = _textview.layoutManager;
        textstorage = _textview.textStorage;
        container = (MarginContainer *)_textview.textContainer;
        if (!layoutmanager)
            NSLog(@"layoutmanager nil!");
        layoutmanager.delegate = self;
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
        pendingEchoDeleteRange.location = (NSUInteger)[decoder decodeIntegerForKey:@"pendingEchoDeleteLocation"];
        pendingEchoDeleteRange.length = (NSUInteger)[decoder decodeIntegerForKey:@"pendingEchoDeleteLength"];
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

// Serialize all window state for autosave. Captures the text view (which
// includes the full text storage), input request state, fence position,
// scroll position, caret state, find bar state, and any in-progress input.
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
    [encoder encodeInteger:(NSInteger)pendingEchoDeleteRange.location forKey:@"pendingEchoDeleteLocation"];
    [encoder encodeInteger:(NSInteger)pendingEchoDeleteRange.length forKey:@"pendingEchoDeleteLength"];
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

// Apply final adjustments after the autorestore process completes. This is
// called once the window has been re-created by the interpreter and the
// restored state from the archive can be transferred into the live window.
// Handles restoring input text, selection, find bar, scroll position, move
// ranges (for VoiceOver navigation), and margin image attachment linkage.
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
    // An in-game restore in the saved session can bake misaligned (mid-word)
    // move boundaries into the autosave; setLastMove's snap only fixes moves
    // recorded live, not these. Realign them against the reconstructed text.
    [self repairRestoredMoveRanges];

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

    layoutmanager.delegate = self;
    scrollAdjustTimeStamp = [NSDate date];
    // Leave _pendingScrollRestore set until the scheduled restoreScroll: below
    // actually runs (it clears the flag for the nil sender). Setting the
    // layoutmanager delegate above, and the text layout that follows, trigger
    // layoutComplete callbacks that re-enter restoreScroll: with a non-nil
    // sender before then. Keeping the flag set marks those callbacks as part of
    // this restore, so they scroll uncapped to the saved position instead of
    // being treated as live game output and capped to _lastseen — which is 0
    // (or a stale y-coordinate) on a fresh restore, pinning the window to the top.

    [self restoreScrollBarStyle];
    if (!self.glkctl.inFullscreen || self.glkctl.startingInFullscreen)
        [self performSelector:@selector(restoreScroll:) withObject:nil afterDelay:0.1];
    else
        [self performSelector:@selector(restoreScroll:) withObject:nil afterDelay:0.5];
}

// Configure the scroll view to use overlay scrollers with the buffer
// window's background color. Called during init and after preference changes.
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


#pragma mark Text finder

// Recreate the find bar after autorestore. Temporarily disables editing
// (required for NSTextFinder setup), configures the finder to use the
// scroll view as its container, and re-populates the search field with
// the previously saved search string if the find bar was visible.
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

// Remove the find bar from the scroll view hierarchy. Walks up from
// the search field to find its top-level container within the scroll
// view, then removes it.
- (void)destroyTextFinder {
    NSView *aView = [self findSearchFieldIn:scrollview];
    if (aView) {
        while (aView.superview != scrollview)
            aView = aView.superview;
        [aView removeFromSuperview];
    }
}

// Recursively search the view hierarchy for an NSSearchField.
// Uses a weak/strong block pattern to allow recursive block invocation.
- (NSSearchField *)findSearchFieldIn:
(NSView *)theView
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

@end
