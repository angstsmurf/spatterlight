//
//  InputTextField.m
//  Spatterlight
//
//  Created by Administrator on 2020-10-24.
//

#include "glk.h"

#import "GlkTextGridWindow.h"
#import "InputTextField.h"
#import "GlkController.h"
#import "GlkController+Speech.h"
#import "CommandScriptHandler.h"

@implementation MyFieldEditor

// Our delegate is the InputTextField, whose delegate is the GlkWindow.
// AppKit may hand this editor to another field while it is not installed
// on ours, so check both links.
- (nullable GlkWindow *)glkWindow {
    id field = self.delegate;
    if (![field isKindOfClass:[InputTextField class]])
        return nil;
    id win = ((InputTextField *)field).delegate;
    if (![win isKindOfClass:[GlkWindow class]])
        return nil;
    return win;
}

// Terminators, char requests and history travel are handled by the window.
- (BOOL)shouldForwardKey:(unsigned)ch toGlkWindow:(GlkWindow *)glkWin {
    return [glkWin.currentTerminators[@(ch)] isEqual:@(YES)] || glkWin.hasCharRequest || ch == keycode_Up || ch == keycode_Down;
}

- (void)keyDown:(NSEvent *)evt {
    unsigned ch = [GlkWindow keycodeForEvent:evt mapNumericPad:NO];
    GlkWindow *glkWin = self.glkWindow;
    if (glkWin && [self shouldForwardKey:ch toGlkWindow:glkWin])
        [glkWin keyDown:evt];
    else
        [super keyDown:evt];
}

- (void)mouseMoved:(NSEvent *)event {
    [[NSCursor IBeamCursor] set];
}

- (BOOL)isAccessibilityElement {
    return YES;
}

// VoiceOver collects custom rotors from the focused element only, and does not
// walk up accessibilityParent. While the player is at the command prompt this
// field editor is the focused element, so without vending the rotors here the
// player would see only the system rotors (Window spots, Misspelled words) and
// lose the Command history / Game windows rotors.
- (NSArray *)accessibilityCustomRotors {
    return self.glkWindow.glkctl.createCustomRotors ?: @[];
}

- (void)paste:(id)sender {
    NSString* string = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];

    if ([string rangeOfCharacterFromSet:[NSCharacterSet newlineCharacterSet]].location != NSNotFound) {
        GlkWindow *glkWin = self.glkWindow;
        if (glkWin) {
            [glkWin.glkctl.commandScriptHandler startCommandScript:string inWindow:glkWin];
            return;
        }
    }

    [super paste:sender];
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    NSPasteboard *pboard = sender.draggingPasteboard;

    GlkWindow *glkWin = self.glkWindow;
    if (glkWin && [glkWin.glkctl.commandScriptHandler commandScriptInPasteboard:pboard fromWindow:glkWin])
        return YES;
    return [super performDragOperation:sender];
}

@end


// The line input field of a grid window. Vends its own field editor (see
// GlkController windowWillReturnFieldEditor:), limits the input length with
// a MyTextFormatter, and on becoming first responder sets the insertion
// point colour, puts the caret at the end and makes the editor an
// accessibility child of the grid text view.

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
        id gridWin = self.delegate;
        if ([gridWin isKindOfClass:[GlkTextGridWindow class]])
            textField.accessibilityParent = ((GlkTextGridWindow *)gridWin).textview;

        textField.delegate = self;
        if( [textField respondsToSelector: @selector(setInsertionPointColor:)] )
            textField.insertionPointColor = self.textColor;
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
    return [self initWithMaxLength:INT_MAX];
}

- (instancetype)initWithMaxLength:(NSUInteger)maxLength {
    if (self = [super init]) {
        _maxLength = maxLength;
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
    if ([object isKindOfClass:[NSString class]])
        return [object copy];
    return nil;
}

#pragma mark -
#pragma mark Object Equivalent to Textual Representation

- (BOOL)getObjectValue:(__autoreleasing id * _Nullable)object forString:(NSString *)string errorDescription:(NSString * __autoreleasing *)error
{
    // Be sure to generate a new object here or binding woe ensues
    // when continuously updating bindings are enabled.
    *object = [NSString stringWithString:string];
    return YES;
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
    // Always allow deleting, even when the text is over the limit.
    if (proposedString.length <= origString.length)
        return YES;

    if (proposedString.length > _maxLength) {

        // The original string has been modified by one or more characters (via
        // pasting). Either way compute how much of the proposed string can be
        // accommodated. Characters in the selected range are replaced, so they
        // do not count. The field may already be over the limit, if its text
        // was set in code or maxLength was lowered.
        NSUInteger keptLength = origString.length - origSelRange.length;
        NSUInteger insertLength = _maxLength > keptLength ? _maxLength - keptLength : 0;

        // Don't cut a surrogate pair or a composed character sequence in two.
        NSUInteger insertEnd = origSelRange.location + insertLength;
        if (insertLength && insertEnd < proposedString.length) {
            NSUInteger sequenceStart = [proposedString rangeOfComposedCharacterSequenceAtIndex:insertEnd].location;
            if (sequenceStart < insertEnd)
                insertLength = sequenceStart > origSelRange.location ? sequenceStart - origSelRange.location : 0;
        }

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

    if ((*partialStringPtr).length > _maxLength) {
        *partialStringPtr = [origString copy];
        *proposedSelRangePtr = origSelRange;
        return NO;
    }
    
    return valid;
}

@end
