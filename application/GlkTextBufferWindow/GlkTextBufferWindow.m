#include "glk_dummy_defs.h"

#import "GlkTextBufferWindow.h"
#import "GlkTextGridWindow.h"

#import "Constants.h"
#import "GlkController.h"
#import "BufferTextView.h"
#import "GridTextView.h"


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
    NSTextContainer *container;
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
}
@end


@implementation GlkTextBufferWindow

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithGlkController:(GlkController *)glkctl_ name:(NSInteger)name_ {

    self = [super initWithGlkController:glkctl_ name:name_];

    if (self) {
        NSUInteger i;

        NSDictionary *styleDict = nil;
        self.styleHints = [self deepCopyOfStyleHintsArray:self.glkctl.bufferStyleHints];

        styles = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
        echo = YES;

        _lastchar = '\n';

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

        container = [[NSTextContainer alloc]
                     initWithContainerSize:NSMakeSize(0, 10000000)];

        container.layoutManager = layoutmanager;
        [layoutmanager addTextContainer:container];

        _textview =
        [[BufferTextView alloc] initWithFrame:NSMakeRect(0, 0, 0, 10000000)
                                textContainer:container];

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

        _textview.textContainerInset = NSZeroSize;

        NSMutableDictionary *linkAttributes = [_textview.linkTextAttributes mutableCopy];

        [_textview enableCaret:nil];

        scrollview.accessibilityLabel = @"buffer scroll view";

        [self addSubview:scrollview];

    }

    return self;
}

- (void)setFrame:(NSRect)frame {
    GlkController *glkctl = self.glkctl;

    if (glkctl.curses && glkctl.quoteBoxes.count && glkctl.turns > 0) {
        // When we extend the height of the status
        // line in Curses in order to make the second line
        // visible when showing quote boxes, we also need to
        // reduce the height of the buffer window below it.
        // This is mainly to prevent the search bar from
        // being partially hidden under the status window.
        for (GlkTextGridWindow *grid in glkctl.contentView.subviews) {
            if ([grid isKindOfClass:[GlkTextGridWindow class]]) {
                frame.size.height = glkctl.contentView.frame.size.height - grid.pendingFrame.size.height;
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

#pragma mark Autorestore

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        _textview = [decoder decodeObjectOfClass:[BufferTextView class] forKey:@"textview"];
        layoutmanager = _textview.layoutManager;
        textstorage = _textview.textStorage;
        container = _textview.textContainer;
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
        _textview.editable = line_request;
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

    _pendingScrollRestore = YES;
    _pendingScroll = NO;
}

- (void)deferredScrollPosition:(id)sender {
    [self restoreScrollBarStyle];
    if (_restoredAtBottom) {
        [self scrollToBottomAnimated:NO];
    } else {
        if (_restoredLastVisible == 0)
            [self scrollToBottomAnimated:NO];
    }
    _pendingScrollRestore = NO;
}

- (void)restoreScrollBarStyle {
    if (scrollview) {
        scrollview.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        scrollview.scrollerStyle = NSScrollerStyleOverlay;
        scrollview.drawsBackground = YES;
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


#pragma mark Output

- (void)clear {
    _pendingClear = YES;
    storedNewline = nil;
    bufferTextstorage = [[NSMutableAttributedString alloc] init];
}

- (void)reallyClear {
    [_textview resetTextFinder];
    fence = 0;
    _lastseen = 0;
    _lastchar = '\n';
    _printPositionOnInput = 0;
    self.moveRanges = [[NSMutableArray alloc] init];
    moveRangeIndex = 0;
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
        if ([string characterAtIndex:length - i - 1] == '\n') {
            if (found) {
                break;
            } else {
                found = YES;
            }
        }
    }
    if (i < length)
        prompt = i;
    else {
        prompt = 0;
        // Found no newline
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
    self.moveRanges = [[NSMutableArray alloc] init];
}

- (void)putString:(NSString *)str style:(NSUInteger)stylevalue {

    if (!str.length) {
        NSLog(@"Null string!");
        return;
    }
//    if ([str characterAtIndex:(str.length - 1)] == '\0')
//        NSLog(@"Null terminated string!");
//    if ([str characterAtIndex:0] == '\0')
//        NSLog(@"Null prefixed string!");

    if (bufferTextstorage.length > 50000)
        bufferTextstorage = [bufferTextstorage attributedSubstringFromRange:NSMakeRange(25000, bufferTextstorage.length - 25000)].mutableCopy;

    if (line_request)
        NSLog(@"Printing to text buffer window during line request");

    [self printToWindow:str style:stylevalue];

    if (self.glkctl.deadCities && line_request && [[str substringFromIndex:str.length - 1] isEqualToString:@"\n"]) {
        // This is against the Glk spec but makes
        // hyperlinks in Dead Cities work.
        // Turn this off by disabling game specific hacks in preferences.
        NSString *line = [textstorage.string substringFromIndex:fence];
        [self sendInputLine:line withTerminator:0];
    }
}

// static const char *stylenames[] =
//{
//    "style_Normal", "style_Emphasized", "style_Preformatted", "style_Header",
//    "style_Subheader", "style_Alert", "style_Note", "style_BlockQuote",
//    "style_Input", "style_User1", "style_User2", "style_NUMSTYLES"
//};

- (void)printToWindow:(NSString *)str style:(NSUInteger)stylevalue {
//    NSLog(@"printToWindow:\"%@\" style:%s", str, stylenames[stylevalue]);

    if (self.glkctl.usesFont3 && str.length == 1 && stylevalue == style_BlockQuote) {
        NSDictionary *font3 = [self font3ToUnicode];
        NSString *newString = font3[str];
        if (newString) {
            str = newString;
            stylevalue = style_Normal;
        }
    }
    //    NSLog(@"\nPrinting %ld chars at position %ld with style %@", str.length, textstorage.length, gBufferStyleNames[stylevalue]);

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

    NSMutableDictionary *attributes = [styles[stylevalue] mutableCopy];

    if (self.currentHyperlink) {
        attributes[NSLinkAttributeName] = @(self.currentHyperlink);
    }

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

    line = [line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];

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

- (void)initLine:(NSString *)str maxLength:(NSUInteger)maxLength
{
    //    NSLog(@"initLine: %@ in: %ld", str, (long)self.name);
    [self flushDisplay];

    if (self.terminatorsPending) {
        self.currentTerminators = self.pendingTerminators;
        self.terminatorsPending = NO;
    }

    if (echo_toggle_pending) {
        echo_toggle_pending = NO;
        echo = !echo;
    }

    fence = textstorage.length;

    NSAttributedString *att = [[NSAttributedString alloc]
              initWithString:str
              attributes:_inputAttributes];

    [textstorage appendAttributedString:att];

    _textview.editable = YES;

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
        _textview.insertionPointColor = color;
    }
}

- (void)showInsertionPoint {
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

- (void)textView:(NSTextView *)view
     draggedCell:(id<NSTextAttachmentCell>)cell
          inRect:(NSRect)rect
           event:(NSEvent *)event
         atIndex:(NSUInteger)charIndex {
    _textview.selectedRange = NSMakeRange(0, 0);
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

    hyper_request = NO;
    return YES;
}

#pragma mark Scrolling

- (void)forceLayout{
    if (textstorage.length < 50000)
        [layoutmanager glyphRangeForTextContainer:container];
}

- (void)markLastSeen {
    NSRange glyphs;
    NSRect line;

    _printPositionOnInput = textstorage.length;
    if (fence > 0)
        _printPositionOnInput = fence;

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

- (void)restoreScroll:(id)sender {
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

    return;
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

    if (textstorage.length < 1000000)
        // first, force a layout so we have the correct textview frame
        [layoutmanager ensureLayoutForTextContainer:container];

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
    //    NSLog(@"GlkTextBufferWindow %ld: scrolledToBottom?", self.name);
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
    //    NSLog(@"GlkTextBufferWindow %ld scrollToBottom", self.name);
    lastAtTop = NO;
    lastAtBottom = YES;

    // first, force a layout so we have the correct textview frame
    [layoutmanager glyphRangeForTextContainer:container];
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

    clipView.bounds = newBounds;
}

- (void)scrollToTop {
    if (pauseScrolling)
        return;
//    NSLog(@"scrollToTop");
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
    return YES;
}

- (void)repeatLastMove:(id)sender {
    GlkController *glkctl = self.glkctl;
    if (glkctl.zmenu)
        [NSObject cancelPreviousPerformRequestsWithTarget:glkctl.zmenu];
    if (glkctl.form)
        [NSObject cancelPreviousPerformRequestsWithTarget:glkctl.form];

    NSString *str = @"";
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
}

- (void)speakNext {
    //    NSLog(@"GlkTextBufferWindow %ld speakNext:", self.name);
    [self setLastMove];
    if (!self.moveRanges.count)
    {
        return;
    }

    NSString *prefix = @"";

    if (moveRangeIndex < self.moveRanges.count - 1) {
        moveRangeIndex++;
    } else {
        prefix = @"At last move.\n";
        moveRangeIndex = self.moveRanges.count - 1;
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

- (NSDictionary <NSNumber *, NSTextAttachment *> *)attachmentsInRange:(NSRange)range withKeys:(NSArray * __autoreleasing *)keys {
    NSMutableDictionary <NSNumber *, NSTextAttachment *> __block *attachments = [NSMutableDictionary new];
    NSMutableArray __block *mutKeys = [NSMutableArray new];
    [textstorage
     enumerateAttribute:NSAttachmentAttributeName
     inRange:range
     options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired
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
