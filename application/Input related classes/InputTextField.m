//
//  InputTextField.m
//  Spatterlight
//
//  Created by Administrator on 2020-10-24.
//

#include "glk.h"

#import "GlkEvent.h"
#import "GlkTextGridWindow.h"
#import "InputTextField.h"

@implementation MyFieldEditor

- (void)keyDown:(NSEvent *)evt {
    NSString *str = evt.characters;
    unsigned ch = keycode_Unknown;
    if (str.length)
        ch = chartokeycode([str characterAtIndex:str.length - 1]);
    GlkWindow *glkWin = (GlkWindow *)((NSTextField *)self.delegate).delegate;
    if ([glkWin.currentTerminators[@(ch)] isEqual:@(YES)] || [glkWin hasCharRequest] || ((ch == keycode_Up || ch == keycode_Down) && ![glkWin hasCharRequest]))
        [glkWin keyDown:evt];
    else {
        [super keyDown:evt];
    }
}

- (void)mouseMoved:(NSEvent *)event {
    [[NSCursor IBeamCursor] set];
}

- (BOOL)isAccessibilityElement {
    return YES;
}

@end


/*
 * Extend NSTextField to ...
 *   - set insertion point color
 *   - set selection at end on becoming key
 */

@implementation InputTextField

- (instancetype)initWithFrame:(NSRect)frame maxLength:(NSUInteger)maxLength{
    self = [super initWithFrame:frame];
    if (self) {
        self.usesSingleLineMode = YES;
        self.editable = YES;
        self.bordered = NO;
        self.allowsEditingTextAttributes = NO;
        self.bezeled = NO;
        self.drawsBackground = NO;
        self.selectable = YES;
        self.cell.wraps = YES;
        self.cell.accessibilityLabel = NSLocalizedString(@"input text field cell", nil);
        self.cell.accessibilityElement = YES;

        self.accessibilityLabel = NSLocalizedString(@"input text field", nil);

        MyTextFormatter *inputFormatter =
        [[MyTextFormatter alloc] initWithMaxLength:maxLength];
        self.formatter = inputFormatter;

        NSTrackingArea *trackingArea = [[NSTrackingArea alloc] initWithRect:frame options: (NSTrackingActiveAlways | NSTrackingMouseEnteredAndExited) owner:self userInfo:nil];
        [self addTrackingArea:trackingArea];

        _fieldEditor = nil;
    }
    return self;
}


@synthesize fieldEditor = _fieldEditor;

- (MyFieldEditor *)fieldEditor {
    if (_fieldEditor == nil) {
        _fieldEditor = [[MyFieldEditor alloc] init];
        _fieldEditor.fieldEditor = YES;
        _fieldEditor.accessibilityLabel = NSLocalizedString(@"input", nil);
    }
    return _fieldEditor;
}

- (BOOL) becomeFirstResponder
{
    BOOL success = [super becomeFirstResponder];
    if( success )
    {
        // Strictly spoken, NSText (which currentEditor returns) doesn't
        // implement setInsertionPointColor:, but it's an NSTextView in practice.
        // But let's be paranoid, better show an invisible black-on-black cursor
        // than crash.
        MyFieldEditor* textField = (MyFieldEditor *)[self currentEditor];
        GlkTextGridWindow *gridWin = (GlkTextGridWindow *)self.delegate;
        textField.accessibilityParent = gridWin.textview;

        textField.delegate = self;
        if( [textField respondsToSelector: @selector(setInsertionPointColor:)] )
            [textField setInsertionPointColor:self.textColor];
        NSRange newRange = NSMakeRange(textField.string.length,0);
        if (!NSEqualRanges(newRange, textField.selectedRange))
            textField.selectedRange = newRange;
    }
    return success;
}

- (BOOL)isAccessibilityElement {
    return NO;
}

@end


// Custom formatter adapted from code by Jonathan Mitchell
// See
// https://stackoverflow.com/questions/827014/how-to-limit-nstextfield-text-length-and-keep-it-always-upper-case
//

@implementation MyTextFormatter

- (instancetype)init {
    if (self = [super init]) {
        self.maxLength = INT_MAX;
    }

    return self;
}

- (instancetype)initWithMaxLength:(NSUInteger)maxLength {
    if (self = [super init]) {
        self.maxLength = maxLength;
    }

    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        _maxLength = (NSUInteger)[decoder decodeIntegerForKey:@"maxLength"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeInteger:(NSInteger)_maxLength forKey:@"maxLength"];
}

#pragma mark -
#pragma mark Textual Representation of Cell Content

- (NSString *)stringForObjectValue:(id)object {
    NSString *stringValue = nil;
    if ([object isKindOfClass:[NSString class]]) {

        // A new NSString is perhaps not required here
        // but generically a new object would be generated
        stringValue = [NSString stringWithString:object];
    }

    return stringValue;
}

#pragma mark -
#pragma mark Object Equivalent to Textual Representation

- (BOOL)getObjectValue:(__autoreleasing id * _Nullable)object forString:(NSString *)string errorDescription:(NSString * __autoreleasing *)error
{
    BOOL valid = YES;

    // Be sure to generate a new object here or binding woe ensues
    // when continuously updating bindings are enabled.
    *object = [NSString stringWithString:string];

    return valid;
}

#pragma mark -
#pragma mark Dynamic Cell Editing

- (BOOL)isPartialStringValid:(NSString * __autoreleasing *)partialStringPtr
       proposedSelectedRange:(NSRangePointer)proposedSelRangePtr
              originalString:(NSString *)origString
       originalSelectedRange:(NSRange)origSelRange
            errorDescription:(NSString * __autoreleasing *)error
{
    BOOL valid = YES;
    NSString *proposedString = *partialStringPtr;
    if (proposedString.length > self.maxLength && self.maxLength) {

        // The original string has been modified by one or more characters (via
        // pasting). Either way compute how much of the proposed string can be
        // accommodated.
        NSUInteger origLength = origString.length;
        NSUInteger insertLength = self.maxLength - origLength;

        // If a range is selected then characters in that range will be removed
        // so adjust the insert length accordingly
        insertLength += origSelRange.length;

        // Get the string components
        NSString *prefix = [origString substringToIndex:origSelRange.location];
        NSString *suffix = [origString
                            substringFromIndex:origSelRange.location + origSelRange.length];
        NSString *insert = [proposedString
                            substringWithRange:NSMakeRange(origSelRange.location,
                                                           insertLength)];

        // Assemble the final string
        *partialStringPtr =
        [NSString stringWithFormat:@"%@%@%@", prefix, insert, suffix];

        // Fix-up the proposed selection range
        proposedSelRangePtr->location = origSelRange.location + insertLength;
        proposedSelRangePtr->length = 0;

        valid = NO;
    }

    if ([*partialStringPtr length] > _maxLength) {
        *partialStringPtr = [origString copy];
        *proposedSelRangePtr = origSelRange;
        return NO;
    }
    
    return valid;
}

@end
