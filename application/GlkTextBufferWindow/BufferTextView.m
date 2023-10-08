//
//  BufferTextView.m
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import "BufferTextView.h"
#import "GlkTextBufferWindow.h"
#import "GlkController.h"

@interface BufferTextView () <NSTextFinderClient, NSSecureCoding> {
    NSTextFinder *_textFinder;
}
@end

@implementation BufferTextView

+ (BOOL) supportsSecureCoding {
    return YES;
}

+ (BOOL)isCompatibleWithResponsiveScrolling {
    return YES;
}

- (BOOL)isOpaque {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        _bottomPadding = [decoder decodeDoubleForKey:@"bottomPadding"];
    }

    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeDouble:_bottomPadding forKey:@"bottomPadding"];
}

- (void)superKeyDown:(NSEvent *)evt {
    [super keyDown:evt];
}

- (void)keyDown:(NSEvent *)evt {
    [(GlkTextBufferWindow *)self.delegate keyDown:evt];
}

- (void)drawRect:(NSRect)rect {
    [NSGraphicsContext currentContext].imageInterpolation =
    NSImageInterpolationHigh;
    [super drawRect:rect];
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    return YES;
}

- (void)mouseDown:(NSEvent *)theEvent {
    NSDate *mouseTime = [NSDate date];

    [self temporarilyHideCaret];
    NSPoint __block location = [self convertPoint:theEvent.locationInWindow fromView:nil];
    NSPoint adjustedPoint = location;
    adjustedPoint.x -= self.textContainerInset.width;
    adjustedPoint.y -= self.textContainerInset.height;
    if (![(GlkTextBufferWindow *)self.delegate myMouseDown:theEvent])
        [super mouseDown:theEvent];
}

// Change mouse cursor when over margin images
- (void)mouseMoved:(NSEvent *)event {
    NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    point.x -= self.textContainerInset.width;
    point.y -= self.textContainerInset.height;
    [super mouseMoved:event];
}

- (BOOL)shouldDrawInsertionPoint {
    BOOL result = super.shouldDrawInsertionPoint;

    // Never draw a caret if the system doesn't want to. Super overrides
    // glkTextBuffer.
    if (result && !_shouldDrawCaret)
        result = _shouldDrawCaret;

    return result;
}

- (void)enableCaret:(nullable id)sender {
    _shouldDrawCaret = YES;
}

- (void)temporarilyHideCaret {
    _shouldDrawCaret = NO;
    [NSTimer scheduledTimerWithTimeInterval:0.05
                                     target:self
                                   selector:@selector(enableCaret:)
                                   userInfo:nil
                                    repeats:NO];
}

- (void)setFrameSize:(NSSize)newSize {
    // NSLog(@"BufferTextView setFrameSize: %@ Old size: %@",
    // NSStringFromSize(newSize), NSStringFromSize(self.frame.size));

    newSize.height += _bottomPadding;
    [super setFrameSize:newSize];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    BOOL isValidItem = NO;
    BOOL waseditable = self.editable;
    GlkTextBufferWindow *delegate = (GlkTextBufferWindow *)self.delegate;

    if (menuItem.action == @selector(changeLayoutOrientation:) || menuItem.action == NSSelectorFromString(@"addLinksInSelection:") || menuItem.action == NSSelectorFromString(@"replaceTextInSelection:") || menuItem.action == NSSelectorFromString(@"replaceQuotesInSelection:") || menuItem.action == NSSelectorFromString(@"replaceDashesInSelection:"))
        return NO;

    if (menuItem.action == @selector(cut:) || menuItem.action == @selector(delete:) || menuItem.action == @selector(paste:) || menuItem.action == @selector(uppercaseWord:) || menuItem.action == @selector(lowercaseWord:) || menuItem.action == @selector(capitalizeWord:)) {
        NSRange editableRange = delegate.editableRange;

        // If no line request, return NO
        if (editableRange.location == NSNotFound) {
            return NO;
        }

        // Allow paste if no selection
        if (menuItem.action == @selector(paste:) && editableRange.location != NSNotFound && editableRange.length == 0 && self.selectedRange.length == 0) {
            return waseditable;
        }

        // Allow all if selection partially inside editable range
        if (NSMaxRange(self.selectedRange) > editableRange.location) {
            return waseditable;
        }

        // Allow all if selection completely inside editable range
        if (self.selectedRange.location >= editableRange.location) {
            // Allow cut or delete only if selection has length
            if (menuItem.action == @selector(paste:)) {
                return waseditable;
            } else if (self.selectedRange.length) {
                return waseditable;
            }
        }

        return NO;
    }

    if (menuItem.action == @selector(performTextFinderAction:))
        isValidItem = [self.textFinder validateAction:menuItem.tag];

    // validate other menu items if needed
    // ...
    // and don't forget to call the superclass
    else {
        isValidItem = [super validateMenuItem:menuItem];
    }

    return isValidItem;
}

- (void)cut:(id)sender {
    self.selectedRange = [self adjustedRange];
    [super cut:sender];
}

- (void)delete:(id)sender {
    self.selectedRange = [self adjustedRange];
    [super delete:sender];
}

- (void)deleteBackward:(id)sender {
    self.selectedRange = [self adjustedRange];
    [super deleteBackward:sender];
}

- (void)uppercaseWord:(id)sender {
    self.selectedRange = [self adjustedRange];
    [super uppercaseWord:sender];
}

- (void)lowercaseWord:(id)sender {
    self.selectedRange = [self adjustedRange];
    [super lowercaseWord:sender];
}

- (void)capitalizeWord:(id)sender {
    self.selectedRange = [self adjustedRange];
    [super capitalizeWord:sender];
}

- (void)paste:(id)sender {
    GlkTextBufferWindow *delegate = (GlkTextBufferWindow *)self.delegate;

    NSString* string = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];

    if ([string rangeOfCharacterFromSet:[NSCharacterSet newlineCharacterSet]].location != NSNotFound) {
        return;
    }
    self.selectedRange = [self adjustedRange];
    [super paste:sender];
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender {
    NSPasteboard *pboard = sender.draggingPasteboard;

    GlkTextBufferWindow *delegate = (GlkTextBufferWindow *)self.delegate;
        BOOL result = [super performDragOperation:sender];
        NSRange editableRange = delegate.editableRange;
        if (self.selectedRange.location < editableRange.location)
            self.selectedRange = NSMakeRange(editableRange.location, self.selectedRange.length);
        return result;
}

- (NSRange)adjustedRange {
    GlkTextBufferWindow *delegate = (GlkTextBufferWindow *)self.delegate;
    NSRange selectedRange = self.selectedRange;
    if (!delegate.hasLineRequest)
        return selectedRange;

    NSRange editableRange = delegate.editableRange;

    if (selectedRange.location < editableRange.location && NSMaxRange(selectedRange) > editableRange.location) {
        return NSIntersectionRange(selectedRange, editableRange);
    } else {
        return selectedRange;
    }
}

- (void)changeAttributes:(id)sender {
}

#pragma mark Text Finder

- (NSTextFinder *)textFinder {
    // Create the text finder on demand
    if (_textFinder == nil) {
        _textFinder = [[NSTextFinder alloc] init];
        _textFinder.client = self;
        _textFinder.findBarContainer = self.enclosingScrollView;
        _textFinder.incrementalSearchingEnabled = YES;
        _textFinder.incrementalSearchingShouldDimContentView = NO;
    }

    return _textFinder;
}

- (void)resetTextFinder {
    if (_textFinder != nil) {
        [_textFinder noteClientStringWillChange];
    }
}

// This is where the commands are actually sent to the text finder
- (void)performTextFinderAction:(id<NSValidatedUserInterfaceItem>)sender {
    BOOL waseditable = self.editable;
    self.editable = NO;
    [self.textFinder performAction:sender.tag];
    self.editable = waseditable;
}

#pragma mark Accessibility

- (BOOL)isAccessibilityElement {
    return YES;
}

- (BOOL)becomeFirstResponder {
    GlkTextBufferWindow *bufWin = (GlkTextBufferWindow *)self.delegate;
    if (!bufWin.glkctl.mustBeQuiet && bufWin.moveRanges.count > 1) {
        [bufWin setLastMove];
        [bufWin performSelector:@selector(repeatLastMove:) withObject:nil afterDelay:0.1];
    }
    return [super becomeFirstResponder];
}

- (NSArray *)accessibilityCustomActions API_AVAILABLE(macos(10.13)) {
    GlkTextBufferWindow *delegate = (GlkTextBufferWindow *)self.delegate;
    NSArray *actions = (delegate.glkctl).accessibilityCustomActions;
    return actions;
}

- (NSArray *)accessibilityCustomRotors  {
    return (((GlkTextBufferWindow *)self.delegate).glkctl).createCustomRotors;
}

- (NSArray *)accessibilityChildren {
    NSArray *children = super.accessibilityChildren;
    return children;
}

@end
