
/*
 * GlkTextGridWindow
 */

#include "glk_dummy_defs.h"

#import "GlkTextGridWindow.h"
#import "GlkTextBufferWindow.h"
#import "GlkController.h"

#import "GridTextView.h"
#import "BufferTextView.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
    fprintf(stderr, "%s\n",                                                    \
            [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@interface GlkTextGridWindow () <NSSecureCoding, NSTextViewDelegate, NSTextStorageDelegate, NSTextFieldDelegate> {
    NSScrollView *scrollview;
    NSTextStorage *textstorage;
    NSLayoutManager *layoutmanager;
    NSTextContainer *container;
    NSUInteger rows, cols;
    NSUInteger xpos, ypos;
    NSUInteger maxInputLength;
    BOOL line_request;
    BOOL hyper_request;
    BOOL mouse_request;
    BOOL transparent;

    NSInteger terminator;
}
@end

@implementation GlkTextGridWindow

+ (BOOL) supportsSecureCoding {
    return YES;
}

+ (BOOL)isCompatibleWithResponsiveScrolling
{
    return YES;
}

- (instancetype)initWithGlkController:(GlkController *)glkctl_
                                 name:(NSInteger)name_ {
    self = [super initWithGlkController:glkctl_ name:name_];

    if (self) {
        // lines = [[NSMutableArray alloc] init];
        
        NSDictionary *styleDict = nil;

        self.styleHints = [self deepCopyOfStyleHintsArray:glkctl_.gridStyleHints];

        styles = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];

        /* construct text system manually */

        scrollview = [[NSScrollView alloc] initWithFrame:NSZeroRect];
        scrollview.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        scrollview.hasHorizontalScroller = NO;
        scrollview.hasVerticalScroller = NO;
        scrollview.horizontalScrollElasticity = NSScrollElasticityNone;
        scrollview.verticalScrollElasticity = NSScrollElasticityNone;
        scrollview.borderType = NSNoBorder;
        scrollview.drawsBackground = NO;
        scrollview.accessibilityElement = NO;

        textstorage = [[NSTextStorage alloc] init];
        _bufferTextStorage = [[NSMutableAttributedString alloc] init];

        layoutmanager = [[NSLayoutManager alloc] init];
        layoutmanager.backgroundLayoutEnabled = YES;

        [textstorage addLayoutManager:layoutmanager];

        container = [[NSTextContainer alloc]
                     initWithContainerSize:NSMakeSize(10000000, 10000000)];

        container.layoutManager = layoutmanager;
        [layoutmanager addTextContainer:container];

        _textview = [[GridTextView alloc] initWithFrame:NSMakeRect(0, 0, 0, 0)
                                           textContainer:container];

        _textview.minSize = NSMakeSize(0, 0);
        _textview.maxSize = NSMakeSize(10000000, 10000000);

        _textview.autoresizingMask = NSViewWidthSizable;

        container.textView = _textview;
        scrollview.documentView = _textview;

        /* now configure the text stuff */

        _textview.delegate = self;
        textstorage.delegate = self;
        _textview.textContainerInset =
            NSZeroSize;
        _textview.editable = NO;
        _textview.usesFontPanel = NO;
        _restoredSelection = NSMakeRange(0, 0);

        [self addSubview:scrollview];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        _textview = [decoder decodeObjectOfClass:[GridTextView class] forKey:@"_textview"];

        layoutmanager = _textview.layoutManager;
        textstorage = _textview.textStorage;
        if (!textstorage)
            NSLog(@"Error! textstorage is nil!");
        _bufferTextStorage = [textstorage mutableCopy];
        _textview.delegate = self;
        textstorage.delegate = self;
        scrollview = _textview.enclosingScrollView;
        scrollview.documentView = _textview;
        scrollview.accessibilityElement = NO;
        container = _textview.textContainer;

        line_request = [decoder decodeBoolForKey:@"line_request"];
        hyper_request = [decoder decodeBoolForKey:@"hyper_request"];
        mouse_request = [decoder decodeBoolForKey:@"mouse_request"];

        rows = (NSUInteger)[decoder decodeIntegerForKey:@"rows"];
        cols = (NSUInteger)[decoder decodeIntegerForKey:@"cols"];
        xpos = (NSUInteger)[decoder decodeIntegerForKey:@"xpos"];
        ypos = (NSUInteger)[decoder decodeIntegerForKey:@"ypos"];

        _selectedRow = (NSUInteger)[decoder decodeIntegerForKey:@"selectedRow"];
        _selectedCol = (NSUInteger)[decoder decodeIntegerForKey:@"selectedCol"];
        _selectedString = [decoder decodeObjectOfClass:[NSString class] forKey:@"selectedString"];

        dirty = YES;
        transparent = [decoder decodeBoolForKey:@"transparent"];

        _restoredSelection =
        ((NSValue *)[decoder decodeObjectOfClass:[NSValue class] forKey:@"selectedRange"])
        .rangeValue;

        _pendingBackgroundCol = [decoder decodeObjectOfClass:[NSColor class] forKey:@"pendingBackgroundCol"];
        _bufferTextStorage = [decoder decodeObjectOfClass:[NSMutableAttributedString class] forKey:@"bufferTextStorage"];

        _enteredTextSoFar = [decoder decodeObjectOfClass:[NSString class] forKey:@"inputString"];
        maxInputLength = (NSUInteger)[decoder decodeIntForKey:@"maxInputLength"];

        _quoteboxSize = ((NSValue *)[decoder decodeObjectOfClass:[NSValue class] forKey:@"quoteboxSize"]).sizeValue;
        _quoteboxAddedOnTurn = [decoder decodeIntegerForKey:@"quoteboxAddedOnTurn"];
        _quoteboxVerticalOffset = (NSUInteger)[decoder decodeIntegerForKey:@"quoteboxVerticalOffset"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeObject:_textview forKey:@"_textview"];
    [encoder encodeBool:line_request forKey:@"line_request"];
    [encoder encodeBool:hyper_request forKey:@"hyper_request"];
    [encoder encodeBool:mouse_request forKey:@"mouse_request"];
    [encoder encodeInteger:(NSInteger)rows forKey:@"rows"];
    [encoder encodeInteger:(NSInteger)cols forKey:@"cols"];
    [encoder encodeInteger:(NSInteger)xpos forKey:@"xpos"];
    [encoder encodeInteger:(NSInteger)ypos forKey:@"ypos"];
    [encoder encodeBool:transparent forKey:@"transparent"];
    NSValue *rangeVal = [NSValue valueWithRange:_textview.selectedRange];
    [encoder encodeObject:rangeVal forKey:@"selectedRange"];

    [encoder encodeInteger:(NSInteger)(_textview.selectedRange.location / (cols + 1)) forKey:@"selectedRow"];
    [encoder encodeInteger:(NSInteger)(_textview.selectedRange.location % (cols + 1)) forKey:@"selectedCol"];
    [encoder encodeObject:[textstorage.string substringWithRange:_textview.selectedRange] forKey:@"selectedString"];

    [encoder encodeInteger:(NSInteger)maxInputLength forKey:@"maxInputLength"];
    [encoder encodeObject:_pendingBackgroundCol forKey:@"pendingBackgroundCol"];
    [encoder encodeObject: _bufferTextStorage forKey:@"bufferTextStorage"];

    [encoder encodeObject:@(_quoteboxSize) forKey:@"quoteboxSize"];
    [encoder encodeInteger:_quoteboxAddedOnTurn forKey:@"quoteboxAddedOnTurn"];
    [encoder encodeInteger:(NSInteger)_quoteboxVerticalOffset forKey:@"quoteboxVerticalOffset"];
}

- (BOOL)wantsFocus {
    return char_request || line_request;
}

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)isOpaque {
    return !transparent;
}

- (void)makeTransparent {
    transparent = YES;
    dirty = YES;
}

- (void)postRestoreAdjustments:(GlkWindow *)win {

    GlkTextGridWindow *restoredWin = (GlkTextGridWindow *)win;
    line_request = [restoredWin hasLineRequest];

    if (restoredWin.bufferTextStorage)
        _bufferTextStorage = restoredWin.bufferTextStorage;

    [self flushDisplay];

    _restoredSelection = restoredWin.restoredSelection;
    _selectedRow = restoredWin.selectedRow;
    _selectedCol = restoredWin.selectedCol;
    _selectedString = restoredWin.selectedString;

    if (NSMaxRange(_restoredSelection) < textstorage.length && [[textstorage.string substringWithRange:_restoredSelection] isEqualToString:_selectedString]) {
        _textview.selectedRange = _restoredSelection;
    } else {
        _restoredSelection = NSMakeRange(_selectedCol + _selectedRow * (cols + 1), _restoredSelection.length);
        if (NSMaxRange(_restoredSelection) < textstorage.length && [[textstorage.string substringWithRange:_restoredSelection] isEqualToString:_selectedString]) {
            _textview.selectedRange = _restoredSelection;
        }
    }

    _enteredTextSoFar = restoredWin.enteredTextSoFar;

    if (line_request) {
        GlkTextGridWindow * __weak weakSelf = self;

        NSString __block *inputString = _enteredTextSoFar;
    }
}

#pragma mark Colors and styles

- (BOOL)allowsDocumentBackgroundColorChange {
    return YES;
}

- (void)setBgColor:(NSInteger)bc {
    bgnd = bc;
}

- (void)flushDisplay {
    _textview.editable = YES;
    NSRange selectedRange = _textview.selectedRange;
    NSString *selectedString = [textstorage.string substringWithRange:selectedRange];
    _selectedRow = selectedRange.location / (cols + 1);
    _selectedCol = selectedRange.location % (cols + 1);

    if (self.framePending) {
        if (!NSEqualRects(self.frame, self.pendingFrame)) {
            super.frame = self.pendingFrame;
        }
        if (!NSEqualRects(_textview.frame, self.bounds)) {
            _textview.frame = self.bounds;
        }

        self.framePending = NO;
    }

    _pendingBackgroundCol = nil;

    if (_bufferTextStorage) {
        NSRange selection = _textview.selectedRange;
        [textstorage setAttributedString:_bufferTextStorage];
        if (NSMaxRange(selection) <= _bufferTextStorage.length)
            _textview.selectedRange = selection;
    } else {
        _bufferTextStorage = [textstorage mutableCopy];
    }

    _restoredSelection = NSMakeRange(_selectedCol + _selectedRow * (cols + 1), _restoredSelection.length);
    if (NSMaxRange(_restoredSelection) <= _bufferTextStorage.length) {
        NSString *newSelectedString = [_bufferTextStorage.string substringWithRange:_restoredSelection];
        if ([newSelectedString isEqualToString:selectedString]) {
           _textview.selectedRange = _restoredSelection;
        }
    }
    _restoredSelection = _textview.selectedRange;
    _textview.editable = NO;
}

#pragma mark Printing, moving, resizing

- (NSDictionary *)findRowColorOfString:(NSMutableAttributedString *)attString index:(NSUInteger)index rowLength:(NSUInteger)rowLength {
    if (transparent)
        return nil;
    if (index > attString.length)
        index = attString.length - rowLength;
    if (index > attString.length)
        return nil;
    NSRange rangeToCheck = NSMakeRange(index, rowLength);
    if (NSMaxRange(rangeToCheck) > attString.length) {
        rangeToCheck.length = attString.length - rangeToCheck.location;
        rowLength = rangeToCheck.length;
    }
    NSRange range;
    NSColor *bg = [attString attribute:NSBackgroundColorAttributeName atIndex:index longestEffectiveRange:&range inRange:rangeToCheck];
    if (rowLength < 2)
        rowLength = 2;
    if (bg && range.length > rowLength - 2) {
        NSMutableDictionary *attrDict = [[NSMutableDictionary alloc] init];
        attrDict[NSBackgroundColorAttributeName] = bg;
        NSValue *reverse = [attString attribute:@"ReverseVideo" atIndex:index effectiveRange:nil];
        if (reverse)
            attrDict[@"ReverseVideo"] = reverse;
        ZColor *zCol = [attString attribute:@"ZColor" atIndex:index effectiveRange:nil];
        if (zCol)
            attrDict[@"ZColor"] = zCol;
        return attrDict;
    }

    return nil;
}

- (void)moveToColumn:(NSUInteger)c row:(NSUInteger)r {
    xpos = c;
    ypos = r;
}

- (void)clear {
    NSRange selectedRange = _textview.selectedRange;
    if (!_bufferTextStorage.length)
        return;
    _bufferTextStorage = [[NSMutableAttributedString alloc]
                          initWithString:@""];

    rows = cols = xpos = ypos = 0;

    // Re-fill with spaces
    if (self.framePending) {
        self.frame = self.pendingFrame;
    } else
        self.frame = self.frame;

//    if (currentZColor && bgnd != currentZColor.bg) {
//        if (currentZColor.bg != zcolor_Current && currentZColor.bg != zcolor_Default) {
//            bgnd = currentZColor.bg;
//        }
//        [self recalcBackground];
//    }

    if (NSMaxRange(selectedRange) > _textview.textStorage.length) {
        if (_textview.textStorage.length)
            selectedRange = NSMakeRange(_textview.textStorage.length - 1, 0);
        else
            selectedRange = NSMakeRange(0, 0);
    }
    _textview.selectedRange = selectedRange;
}

- (void)putString:(NSString *)string style:(NSUInteger)stylevalue {
    if (line_request)
        NSLog(@"Printing to text grid window during line request");

    [self printToWindow:string style:stylevalue];
}

- (void)printToWindow:(NSString *)string style:(NSUInteger)stylevalue {
    NSUInteger length = string.length;
    NSUInteger startpos;
    NSUInteger pos = 0;

    GlkController *glkctl = self.glkctl;

    NSUInteger textstoragelength = _bufferTextStorage.length;

    if (textstoragelength == 0) {
        if (textstorage.length) {
            _bufferTextStorage = [textstorage mutableCopy];
            textstoragelength = textstorage.length;
        } else return;
    }

    if (cols == 0 || rows == 0 || length == 0)
        return;

    // With certain fonts and sizes, strings containing only spaces will "collapse."
    // So if the first character is a space, we replace it with a &nbsp;
    if ([string hasPrefix:@" "]) {
        const unichar nbsp = 0xa0;
        NSString *nbspstring = [NSString stringWithCharacters:&nbsp length:1];
        string = [string stringByReplacingCharactersInRange:NSMakeRange(0, 1) withString:nbspstring];
    }
    if (xpos > cols) {
        ypos += (xpos / cols);
        xpos = (xpos % cols);
    }
    NSMutableDictionary *attrDict = [styles[stylevalue] mutableCopy];

    if (!attrDict)
        NSLog(@"GlkTextGridWindow printToWindow: ERROR! Style dictionary nil!");

    startpos = self.indexOfPos;
    if (startpos > textstoragelength) {
        // We are outside window visible range!
        // Do nothing
        NSLog(@"Printed outside grid window visible range! (%@)", string);
        return;
    }

    if (self.currentHyperlink) {
        attrDict[NSLinkAttributeName] = @(self.currentHyperlink);
    }

    if (ypos > rows) {
        NSLog(@"printToWindow: ypos outside visible range");
        return;
    }

    // Check for newlines in string to write
    NSUInteger x;
    for (x = 0; x < length; x++) {
        if ([string characterAtIndex:x] == '\n' ||
            [string characterAtIndex:x] == '\r') {
            [self printToWindow:[string substringToIndex:x] style:stylevalue];
            xpos = 0;
            ypos++;
            [self printToWindow:[string substringFromIndex:x + 1]
                          style:stylevalue];
            return;
        }
    }

    // Write this string
    while (pos < length) {
        // Can't write if we've fallen off the end of the window
        if (((NSInteger)cols > -1 && ypos > textstoragelength / (cols + 1) ) || ypos > rows)
            break;

        // Can only write a certain number of characters
        if (xpos >= cols) {
            xpos = 0;
            ypos++;
            continue;
        }

        // Get the number of characters to write
        NSUInteger amountToDraw = cols - xpos;
        if (amountToDraw > string.length - pos) {
            amountToDraw = string.length - pos;
        }

        if (self.indexOfPos + amountToDraw > textstoragelength)
            amountToDraw = textstoragelength - self.indexOfPos + 1;

        if (amountToDraw < 1)
            break;

        NSRange replaceRange = NSMakeRange(self.indexOfPos, amountToDraw);
        if (NSMaxRange(replaceRange) > textstoragelength) {
            if (self.indexOfPos > textstoragelength)
                return;
            else {
                NSUInteger diff = NSMaxRange(replaceRange) - textstoragelength;
                amountToDraw -= diff;
                if (!amountToDraw)
                    return;
                replaceRange = NSMakeRange(self.indexOfPos, amountToDraw);
            }
        }

        // "Draw" the characters
        NSAttributedString *partString = [[NSAttributedString alloc]
                                          initWithString:[string substringWithRange:
                                                          NSMakeRange(pos, amountToDraw)]
                                          attributes:attrDict];
        [_bufferTextStorage
         replaceCharactersInRange:replaceRange withAttributedString:partString];

        // Update the x position (and the y position if necessary)
        xpos += amountToDraw;
        pos += amountToDraw;
        if (xpos >= cols - 1) {
            xpos = 0;
            ypos++;
        }
    }

    _hasNewText = YES;
    dirty = YES;
}

- (NSUInteger)indexOfPos {
    return ypos * (cols + 1) + xpos;
}

- (NSSize)currentSizeInChars {
    return NSMakeSize(cols, rows);
}

#pragma mark Hyperlinks

- (void)initHyperlink {
    hyper_request = YES;
}

- (void)cancelHyperlink {
    hyper_request = NO;
}

- (BOOL)textView:_textview clickedOnLink:(id)link atIndex:(NSUInteger)charIndex {
    hyper_request = NO;
    return YES;
}

#pragma mark Mouse input

- (void)initMouse {
    mouse_request = YES;
}

- (void)cancelMouse {
    mouse_request = NO;
}

- (BOOL)myMouseDown:(NSEvent *)theEvent {
    GlkEvent *gev;
    if (mouse_request) {
        [self.glkctl markLastSeen];

        NSPoint p;
        p = theEvent.locationInWindow;

        p = [_textview convertPoint:p fromView:nil];

        p.x -= _textview.textContainerInset.width;
        p.y -= _textview.textContainerInset.height;

        NSUInteger charIndex =
        [_textview.layoutManager characterIndexForPoint:p
                                       inTextContainer:container
              fractionOfDistanceBetweenInsertionPoints:nil];

        p.y = charIndex / (cols + 1);
        p.x = charIndex % (cols + 1);

        if (p.x >= 0 && p.y >= 0 && p.x < cols && p.y < rows) {
        }
    }
    return NO;
}

#pragma mark Single key input

- (void)initChar {
    char_request = YES;
    dirty = YES;

    // Draw Bureaucracy form cursor
    if (self.glkctl.bureaucracy) {
        self.currentReverseVideo = YES;
        [self putString:@" " style:style_Normal];
        self.currentReverseVideo = NO;
        xpos--;
    }
}

- (void)cancelChar {
    char_request = NO;
    dirty = YES;

    // Remove leftover "cursors"
    // when running command scripts in
    // Bureaucracy form
    if (self.glkctl.bureaucracy && self.glkctl.commandScriptRunning) {
        self.currentReverseVideo = NO;
        [self putString:@" " style:style_Normal];
        xpos--;
    }
}


- (NSString *)cancelLine {
    line_request = NO;
    return @"";
}

- (NSUInteger)unputString:(NSString *)buf {

    NSUInteger endpos = self.indexOfPos;
    NSUInteger startpos = endpos - buf.length;

    if (endpos > _bufferTextStorage.length || startpos > endpos) {
        return 0;
    }

    NSString *stringToRemove = [_bufferTextStorage.string substringWithRange:NSMakeRange(startpos, buf.length)].uppercaseString;

    if ([stringToRemove isEqualToString:buf.uppercaseString]) {
        NSString *spaces = [[[NSString alloc] init]
                            stringByPaddingToLength:buf.length
                            withString:@" "
                            startingAtIndex:0];
        xpos -= buf.length;
        [self putString:spaces style:style_Normal];
        xpos -= buf.length;
    } else {
        NSLog(@"GlkTextGridWindow unputString: string \"%@\" not found!", buf);
        return 0;
    }
    return buf.length;
}

- (BOOL)hasLineRequest {
    return line_request;
}

@end
