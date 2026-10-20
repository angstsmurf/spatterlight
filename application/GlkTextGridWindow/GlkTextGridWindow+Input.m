//
//  GlkTextGridWindow+Input.m
//  Spatterlight
//
//  Hyperlink, mouse, character and line input, and the command history.
//

#import "GlkTextGridWindowPrivate.h"
#import "GlkController.h"
#import "GlkController+InterpreterGlue.h"
#import "GlkController+Speech.h"
#import "GlkEvent.h"
#import "InputTextField.h"
#import "InputHistory.h"
#import "GridTextView.h"
#import "NSString+Categories.h"
#import "Theme.h"
#import "GlkStyle.h"
#import "ZColor.h"
#import "NSColor+integer.h"
#import "NSView+Categories.h"

@implementation GlkTextGridWindow (Input)

#pragma mark Hyperlinks

- (BOOL)textView:(NSTextView *)textView clickedOnLink:(id)link atIndex:(NSUInteger)charIndex {
    if (!hyper_request) {
        NSLog(@"txtgrid: No hyperlink request in window.");
        // City of Secrets has hyperlinks but no hyperlink
        // requests, covered by mouse requests.
        if (mouse_request)
            return [self sendMouseEventAtColumn:[self columnOfIndex:charIndex] row:[self rowOfIndex:charIndex]];
        return NO;
    }

    GlkEvent *gev =
    [[GlkEvent alloc] initLinkEvent:((NSNumber *)link).unsignedIntegerValue
                          forWindow:self.name];
    [self.glkctl queueEvent:gev];

    hyper_request = NO;
    return YES;
}

#pragma mark Mouse input

// Queues a mouse event for the cell at column, row and ends the mouse request,
// if the cell is on the grid.
- (BOOL)sendMouseEventAtColumn:(CGFloat)column row:(CGFloat)row {
    if (column < 0 || row < 0 || column >= (CGFloat)cols || row >= (CGFloat)rows)
        return NO;
    [self sendMouseEventAtPoint:NSMakePoint(column, row)];
    return YES;
}

- (BOOL)myMouseDown:(NSEvent *)theEvent {
    if (mouse_request) {
        NSPoint point;
        NSPoint point_in_window = [_textview containerPointForEvent:theEvent];

        NSUInteger charIndex =
        [_textview.layoutManager characterIndexForPoint:point_in_window
                                       inTextContainer:container
              fractionOfDistanceBetweenInsertionPoints:nil];

        point.y = (CGFloat)[self rowOfIndex:charIndex];
        point.x = (CGFloat)[self columnOfIndex:charIndex];
        if (point.x >= (CGFloat)cols)
            point.x = point_in_window.x / self.theme.cellWidth;
        return [self sendMouseEventAtColumn:point.x row:point.y];
    }
    return NO;
}

#pragma mark Single key input

// Bureaucracy's forms have no cursor of their own, so one is drawn as a
// reverse-video space at the cursor, and erased with a plain one. The cursor
// position is left where it was.
- (void)drawBureaucracyCursor:(BOOL)visible {
    self.currentReverseVideo = visible;
    [self putString:@" " style:style_Normal];
    self.currentReverseVideo = NO;
    xpos--;
}

- (void)initChar {
    char_request = YES;
    dirty = YES;

    // Draw Bureaucracy form cursor
    if (self.glkctl.gameID == kGameIsBureaucracy)
        [self drawBureaucracyCursor:YES];
}

- (void)cancelChar {
    char_request = NO;
    dirty = YES;

    // Remove leftover "cursors"
    // when running command scripts in
    // Bureaucracy form
    if (self.glkctl.gameID == kGameIsBureaucracy && self.glkctl.commandScriptRunning)
        [self drawBureaucracyCursor:NO];
}

- (void)keyDown:(NSEvent *)evt {
    GlkController *glkctl = self.glkctl;
    // Bureaucracy's forms read keypad digits as plain digits
    unsigned ch = [GlkWindow keycodeForEvent:evt mapNumericPad:glkctl.gameID != kGameIsBureaucracy];
    NSUInteger flags = evt.modifierFlags;

    if ([self forwardKeyDownToFocusedWindow:evt])
        return;

    // Stupid hack for Swedish keyboard
    if (char_request && glkctl.gameID == kGameIsBureaucracy && evt.keyCode == 30)
        ch = '^';

    if (char_request && ch != keycode_Unknown) {
        [glkctl markLastSeen];

        if (glkctl.gameID == kGameIsBureaucracy) {
            // Bureaucracy on Bocfel will try to convert these keycodes
            // to characters and then error out with
            // "fatal error: @print_char called with invalid character"
            // so we attempt to change them into something reasonable here.
            if (ch == keycode_Up || ch == keycode_Home || ch == keycode_PageUp)
                ch = '^';
            else if (ch == keycode_Down || ch == keycode_Tab || ch == keycode_End || ch == keycode_PageDown)
                ch = keycode_Return;
            else if (ch == keycode_Left || ch == keycode_Erase)
                ch = keycode_Delete;
            else if (ch == keycode_Right)
                ch = ' ';
            else if (ch == keycode_Escape || (ch >= keycode_Func12 && ch <= keycode_Func1))
                ch = keycode_Unknown;

            //For speaking input
            _lastKeyPress = [NSString stringWithFormat:@"%C", (unichar)ch];
            _keyPressTimeStamp = [NSDate date];

            [self drawBureaucracyCursor:NO];
        }

        GlkEvent *gev = [[GlkEvent alloc] initCharEvent:ch forWindow:self.name];
        [glkctl queueEvent:gev];
        char_request = NO;
        dirty = YES;
        return;
    }

    if (line_request) {
        // Did player type enter or line terminator?
        if (ch == keycode_Return || [self.currentTerminators[@(ch)] isEqual:@(YES)]) {
            terminator = [self.currentTerminators[@(ch)] isEqual:@(YES)] ? ch : 0;

            if (glkctl.gameID == kGameIsBeyondZork || [glkctl zVersion6]) {
                if (terminator == keycode_Home) {
                    NSLog(@"Gridwin keyDown changed keycode_Home to keycode_Up");
                    terminator = keycode_Up;
                } else if (terminator == keycode_End) {
                    NSLog(@"Gridwin keyDown changed keycode_End to keycode_Down");
                    terminator = keycode_Down;
                }
            }

            [self.window makeFirstResponder:nil];
            if (ch != keycode_Return)
                [self typedEnter:nil];
            // Or ar we here because the input field lost focus?
            // (When the input field has focus, key events won't be sent here)
            // (unless it passes along line termination events)
        } else if (ch != keycode_Unknown && line_request) {
            if (!self.input)
                [self initLine:_enteredTextSoFar maxLength:maxInputLength];
            [self.window makeFirstResponder:self.input];

            if (ch == keycode_Up) {
                [self travelInHistoryForward:NO];
                return;
            }

            if (ch == keycode_Down) {
                [self travelInHistoryForward:YES];
                return;
            }

            NSTextView* textField = (NSTextView*)self.input.fieldEditor;
            [textField keyDown:evt];
        }
    }
}

- (void)sendKeypress:(unsigned)ch {
    [super sendKeypress:ch];
    _textview.editable = NO;
}

#pragma mark Line input

- (void)initLine:(NSString *)str maxLength:(NSUInteger)maxLength  {
    if (self.terminatorsPending) {
        self.currentTerminators = self.pendingTerminators;
        self.terminatorsPending = NO;
    }

    terminator = 0;

    if (xpos > cols || maxLength == 0) {
        return;
    }

    if (xpos + maxLength > cols + 1)
        maxLength = cols + 1 - xpos;

    line_request = YES;
    [history reset];
    maxInputLength = maxLength;

    NSRect bounds = self.bounds;
    CGFloat mx = _textview.textContainerInset.width;
    CGFloat my = _textview.textContainerInset.height;

    CGFloat x0 = (NSMinX(bounds) + mx + container.lineFragmentPadding / 2);
    CGFloat y0 = (NSMinY(bounds) + my);
    CGFloat lineHeight = self.theme.cellHeight;
    CGFloat charWidth = self.theme.cellWidth;

    // Input below the grid goes on the last row.
    if (rows && ypos >= rows)
        ypos = rows - 1;

    NSRect caret;
    caret.origin.x = x0 + (CGFloat)xpos * charWidth;
    caret.origin.y = y0 + (CGFloat)ypos * lineHeight;
    caret.size.width = (CGFloat)maxLength * charWidth + container.lineFragmentPadding;
    caret.size.height = lineHeight;

    self.input = [[InputTextField alloc] initWithFrame:caret maxLength:maxLength];
    self.input.action = @selector(typedEnter:);
    self.input.target = self;
    self.input.delegate = self;
    self.input.font = self.theme.gridInput.font;
    NSDictionary *firstCharDict = nil;
    if (_bufferTextStorage.length)
        firstCharDict = [_bufferTextStorage attributesAtIndex:0 effectiveRange:nil];
    else if ([styles[style_Normal] isKindOfClass:[NSDictionary class]])
        firstCharDict = styles[style_Normal];
    self.input.textColor = firstCharDict[NSForegroundColorAttributeName];
    if (currentZColor && self.theme.doStyles)
        self.input.textColor = [NSColor colorFromInteger:currentZColor.fg];

    _enteredTextSoFar = str;

    NSAttributedString *attString = [[NSAttributedString alloc]
                                     initWithString:str
                                     attributes:firstCharDict];

    self.input.attributedStringValue = attString;

    [_textview addSubview:self.input];

    [self performSelector:@selector(deferredGrabFocus:) withObject:str afterDelay:0.1];
}

- (void)deferredGrabFocus:(id)str {
    if (self.input) {
        [self.window makeFirstResponder:self.input];
    }
}

- (NSString *)cancelLine {
    line_request = NO;
    if (self.input) {
        NSString *str = self.input.stringValue;
        [self printToWindow:str style:style_Input];
        [self moveToColumn:0 row:ypos + 1];
        [self.input removeFromSuperview];
        self.input = nil;
        return str;
    }
    return @"";
}

- (void)typedEnter:(id)sender {
    line_request = NO;
    if (self.input) {
        [self.glkctl markLastSeen];

        NSString *str = self.input.stringValue;

        [self printToWindow:str style:style_Input];
        [self moveToColumn:0 row:ypos + 1];

        if (str.length > 0) {
            [history saveHistory:str];
        }

        str = [str scrubInvalidCharacters];

        GlkEvent *gev = [[GlkEvent alloc] initLineEvent:str
                                              forWindow:self.name terminator:terminator];
        [self.glkctl queueEvent:gev];
        [self.input removeFromSuperview];
        self.input = nil;
    }
}

- (void)sendCommandLine:(NSString *)command {
    self.input.stringValue = command;
    [self typedEnter:nil];
}

- (void)controlTextDidChange:(NSNotification *)obj {
    NSDictionary *dict = obj.userInfo;
    MyFieldEditor *fieldEditor = dict[@"NSFieldEditor"];
    _enteredTextSoFar = [fieldEditor.string copy];
}

// Re-creates the line input field once the view has settled, after an
// autorestore or a theme change. Only the latest request is kept. The hop to
// the main queue is for callers off the main thread, whose run loop would
// never fire the delayed perform.
- (void)scheduleDeferredInitLine:(NSString *)text delay:(NSTimeInterval)delay {
    GlkTextGridWindow * __weak weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        GlkTextGridWindow *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        [NSObject cancelPreviousPerformRequestsWithTarget:strongSelf selector:@selector(deferredInitLine:) object:strongSelf->deferredLineText];
        strongSelf->deferredLineText = text;
        [strongSelf performSelector:@selector(deferredInitLine:) withObject:text afterDelay:delay];
    });
}

- (void)deferredInitLine:(id)sender {
    deferredLineText = nil;
    NSArray *subviews = _textview.subviews;
    if (subviews) {
        for (NSView *view in subviews) {
            if ([view isKindOfClass:[NSTextField class]]) {
                self.input = (InputTextField *)view;
                [self.input.fieldEditor removeFromSuperview];
                [self.input removeFromSuperview];
                self.input = nil;
            }
        }
    }
    [self initLine:(NSString *)sender maxLength:maxInputLength];
}

- (BOOL)hasLineRequest {
    return line_request;
}

#pragma mark Command history

- (NSString *)historyInputText {
    if (!self.input)
        return nil;
    return self.input.stringValue ?: @"";
}

- (void)replaceInputWithHistoryEntry:(NSString *)entry {
    self.input.stringValue = entry;
    self.input.fieldEditor.selectedRange = NSMakeRange(entry.length, 0);
}

@end
