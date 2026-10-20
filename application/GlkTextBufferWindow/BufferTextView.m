//
//  BufferTextView.m
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import "BufferTextView.h"
#import "GlkTextBufferWindow.h"
#import "GlkController.h"
#import "GlkController+Speech.h"
#import "MarginContainer.h"
#import "NSView+Categories.h"
#import "MarginImage.h"
#import "CommandScriptHandler.h"
#import "Preferences.h"
#import "Game.h"


@interface BufferTextView () <NSSecureCoding> {
    NSTimer *caretTimer;
    BOOL bottomPaddingUpdatePending;
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

// Let the buffer window handle the click (hyperlinks, input focus) and fall
// back to normal text view behaviour if it doesn't.
- (void)forwardMouseDown:(NSEvent *)theEvent {
    if (![(GlkTextBufferWindow *)self.delegate myMouseDown:theEvent])
        [super mouseDown:theEvent];
}

- (void)mouseDown:(NSEvent *)theEvent {
    [self temporarilyHideCaret];
    NSPoint adjustedPoint = [self containerPointForEvent:theEvent];

    MarginImage *marginImage = [(MarginContainer *)self.textContainer marginImageAt:adjustedPoint];
    if (!marginImage) {
        [self forwardMouseDown:theEvent];
    } else {
        [self trackClickOrDragFromEvent:theEvent click:^(NSEvent *upEvent) {
            [self forwardMouseDown:theEvent];
        } drag:^(NSEvent *dragEvent) {
            GlkTextBufferWindow *delegate = (GlkTextBufferWindow *)self.delegate;
            NSRect bounds = marginImage.bounds;
            bounds.origin.x += self.textContainerInset.width;
            bounds.origin.y += self.textContainerInset.height;

            [marginImage dragMarginImageFrom:self event:dragEvent filename:delegate.glkctl.game.fileBaseName rect:bounds];
        }];
    }
}

// Change mouse cursor when over margin images
- (void)mouseMoved:(NSEvent *)event {
    NSPoint point = [self containerPointForEvent:event];
    MarginImage *marginImage = [(MarginContainer *)self.textContainer marginImageAt:point];
    if (marginImage) {
        [marginImage cursorUpdate];
        return;
    }
    [super mouseMoved:event];
}

// Never draw a caret if the system doesn't want to. Super overrides
// glkTextBuffer.
- (BOOL)shouldDrawInsertionPoint {
    return super.shouldDrawInsertionPoint && _shouldDrawCaret;
}

- (void)enableCaret:(nullable id)sender {
    _shouldDrawCaret = YES;
}

// Hide the caret for a moment. Repeated calls extend the pause rather than
// stacking timers, so the caret comes back 0.05 s after the last call.
- (void)temporarilyHideCaret {
    _shouldDrawCaret = NO;
    [caretTimer invalidate];
    caretTimer = [NSTimer scheduledTimerWithTimeInterval:0.05
                                     target:self
                                   selector:@selector(enableCaret:)
                                   userInfo:nil
                                    repeats:NO];
}

- (void)setFrameSize:(NSSize)newSize {
    // The layout manager passes the height of the text. A caller that re-sets
    // the current frame (autoresizing, updateBottomPadding) passes the height
    // with the padding already added, so take it off again, or the padding
    // would pile up, or stay behind once the image no longer needs it.
    CGFloat textHeight = newSize.height;
    if (_bottomPadding > 0 && textHeight == NSHeight(self.frame))
        textHeight -= _bottomPadding;

    _bottomPadding = [self paddingForTextHeight:textHeight];
    newSize.height = textHeight + _bottomPadding;
    [super setFrameSize:newSize];
}

// How far the lowest margin image (plus the bottom margin) reaches below
// text of the given height.
- (CGFloat)paddingForTextHeight:(CGFloat)textHeight {
    MarginContainer *marginContainer = (MarginContainer *)self.textContainer;
    if (![marginContainer isKindOfClass:[MarginContainer class]] || !marginContainer.hasMarginImages)
        return 0;
    CGFloat imagesBottom = marginContainer.marginImagesBottom;
    if (imagesBottom <= 0)
        return 0;
    CGFloat needed = imagesBottom + self.textContainerOrigin.y + self.textContainerInset.height;
    return MAX(0, needed - textHeight);
}

- (void)updateBottomPadding {
    bottomPaddingUpdatePending = NO;
    NSSize size = self.frame.size;
    CGFloat textHeight = size.height - _bottomPadding;
    if (fabs([self paddingForTextHeight:textHeight] - _bottomPadding) < 0.5)
        return;
    [self setFrameSize:size];
}

- (void)bottomPaddingNeedsUpdate {
    if (bottomPaddingUpdatePending)
        return;
    CGFloat textHeight = NSHeight(self.frame) - _bottomPadding;
    if (fabs([self paddingForTextHeight:textHeight] - _bottomPadding) < 0.5)
        return;
    bottomPaddingUpdatePending = YES;
    dispatch_async(dispatch_get_main_queue(), ^{
        [self updateBottomPadding];
    });
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
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

    // Also validates performTextFinderAction:.
    return [super validateMenuItem:menuItem];
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
    // Let the scroll happen first, then notify the delegate, so it can test
    // the *updated* scroll position (e.g. to resume auto-scrolling once the
    // user has reached the bottom).
    CGFloat oldPosition = self.enclosingScrollView.contentView.bounds.origin.y;
    [super scrollWheel:event];
    [delegate scrollWheelchanged:event fromPosition:oldPosition];
}

// The Find bar reveals each match through this (BufferTextView is its
// NSTextFinderClient), and jumping to a match up in the scrollback is the
// user scrolling up as much as a wheel or paging key is: tell the delegate,
// so it can pause auto-scrolling while unattended output keeps arriving.
- (void)scrollRangeToVisible:(NSRange)range {
    GlkTextBufferWindow *delegate = (GlkTextBufferWindow *)self.delegate;
    CGFloat oldPosition = self.enclosingScrollView.contentView.bounds.origin.y;
    [super scrollRangeToVisible:range];
    [delegate userDidScrollUpFromPosition:oldPosition];
}

- (void)changeAttributes:(id)sender {
    [(NSTextView *)[Preferences instance].dummyTextView changeAttributes:sender];
}

#pragma mark Text Finder

// The finder must not run while the input line is editable.
- (void)performTextFinderAction:(id<NSValidatedUserInterfaceItem>)sender {
    BOOL waseditable = self.editable;
    self.editable = NO;
    [super performTextFinderAction:sender];
    self.editable = waseditable;
}

#pragma mark Accessibility

- (BOOL)isAccessibilityElement {
    return YES;
}

- (NSArray *)accessibilityCustomActions API_AVAILABLE(macos(10.13)) {
    GlkTextBufferWindow *delegate = (GlkTextBufferWindow *)self.delegate;
    NSArray *actions = (delegate.glkctl).accessibilityCustomActions;
    return actions;
}

- (NSArray *)accessibilityCustomRotors  {
    return (((GlkTextBufferWindow *)self.delegate).glkctl).createCustomRotors;
}

// Margin images compute their own accessibilityFrame on demand.
- (NSArray *)accessibilityChildren {
    NSArray *children = super.accessibilityChildren;
    NSArray<MarginImage *> *marginImages = ((MarginContainer *)self.textContainer).marginImages;
    if (marginImages.count)
        children = [children arrayByAddingObjectsFromArray:marginImages];
    return children;
}

@end
