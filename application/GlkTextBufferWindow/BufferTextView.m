//
//  BufferTextView.m
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import "BufferTextView.h"
#import "GlkTextBufferWindow.h"
#import "GlkController.h"
#import "MarginContainer.h"
#import "MarginImage.h"
#import "CommandScriptHandler.h"
#import "Preferences.h"
#import "Game.h"


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
    [(MarginContainer *)self.textContainer drawRect:rect];
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

    MarginImage *marginImage = [(MarginContainer *)self.textContainer marginImageAt:adjustedPoint];
    if (!marginImage) {
        if (![(GlkTextBufferWindow *)self.delegate myMouseDown:theEvent])
            [super mouseDown:theEvent];
    } else {

        NSEventMask eventMask = NSEventMaskLeftMouseDown | NSEventMaskLeftMouseDragged | NSEventMaskLeftMouseUp;
        NSTimeInterval timeout = NSEventDurationForever;

        CGFloat dragThreshold = 0.3;

        [self.window trackEventsMatchingMask:eventMask timeout:timeout mode:NSEventTrackingRunLoopMode handler:^(NSEvent * _Nullable event, BOOL * _Nonnull stop) {

            if (!event) { return; }

            if (event.type == NSEventTypeLeftMouseUp) {
                *stop = YES;
                if (mouseTime.timeIntervalSinceNow > -0.5) {
                    if (![(GlkTextBufferWindow *)self.delegate myMouseDown:theEvent])
                        [super mouseDown:theEvent];
                }
            } else if (event.type == NSEventTypeLeftMouseDragged) {
                NSPoint movedLocation = [self convertPoint:event.locationInWindow fromView: nil];
                if (ABS(movedLocation.x - location.x) > dragThreshold || ABS(movedLocation.y - location.y) > dragThreshold) {
                    *stop = YES;

                    GlkTextBufferWindow *delegate = (GlkTextBufferWindow *)self.delegate;
                    NSString *filename = delegate.glkctl.game.path.lastPathComponent.stringByDeletingPathExtension;

                    NSRect bounds = marginImage.bounds;
                    bounds.origin.x += self.textContainerInset.width;
                    bounds.origin.y += self.textContainerInset.height;

                    [marginImage dragMarginImageFrom:self event:event filename:filename rect:bounds];
                }
            }
        }];
    }
}

// Change mouse cursor when over margin images
- (void)mouseMoved:(NSEvent *)event {
    NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    point.x -= self.textContainerInset.width;
    point.y -= self.textContainerInset.height;
    MarginImage *marginImage = [(MarginContainer *)self.textContainer marginImageAt:point];
    if (marginImage) {
        [marginImage cursorUpdate];
        return;
    }
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
        [delegate.glkctl.commandScriptHandler startCommandScript:string inWindow:delegate];
        return;
    }
    self.selectedRange = [self adjustedRange];
    [super paste:sender];
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender {
    NSPasteboard *pboard = sender.draggingPasteboard;

    GlkTextBufferWindow *delegate = (GlkTextBufferWindow *)self.delegate;
    if ([delegate.glkctl.commandScriptHandler commandScriptInPasteboard:pboard fromWindow:delegate])
        return YES;
    else {
        BOOL result = [super performDragOperation:sender];
        NSRange editableRange = delegate.editableRange;
        if (self.selectedRange.location < editableRange.location)
            self.selectedRange = NSMakeRange(editableRange.location, self.selectedRange.length);
        return result;
    }
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

- (void)scrollWheel:(NSEvent *)event {
    GlkTextBufferWindow *delegate = (GlkTextBufferWindow *)self.delegate;
    [delegate scrollWheelchanged:(NSEvent *)event];
    [super scrollWheel:event];
}

- (void)changeAttributes:(id)sender {
    [(NSTextView *)[Preferences instance].dummyTextView changeAttributes:sender];
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

    for (MarginImage *mi in ((MarginContainer *)self.textContainer).marginImages) {
        children = [children arrayByAddingObject:mi];
        NSRect bounds = [mi boundsWithLayout:self.layoutManager];
        bounds = NSAccessibilityFrameInView(self, bounds);
        bounds.origin.x += self.textContainerInset.width;
        bounds.origin.y -= self.textContainerInset.height;

        mi.accessibilityFrame = bounds;
    }
    return children;
}

@end
