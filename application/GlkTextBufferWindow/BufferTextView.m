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

- (void)mouseDown:(NSEvent *)theEvent {
    if (![(GlkTextBufferWindow *)self.delegate myMouseDown:theEvent])
        [super mouseDown:theEvent];
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
    // NSLog(@"MyTextView setFrameSize: %@ Old size: %@",
    // NSStringFromSize(newSize), NSStringFromSize(self.frame.size));

    newSize.height += _bottomPadding;
    [super setFrameSize:newSize];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    BOOL isValidItem = NO;
    BOOL waseditable = self.editable;
    self.editable = NO;
    GlkTextBufferWindow *delegate = (GlkTextBufferWindow *)self.delegate;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundeclared-selector"

    if (delegate.glkctl.previewDummy && menuItem.action != @selector(copy:) && menuItem.action != @selector(_lookUpDefiniteRangeInDictionaryFromMenu:) && menuItem.action != @selector(_searchWithGoogleFromMenu:))
        return NO;

#pragma clang diagnostic pop

    if (menuItem.action == @selector(cut:)) {
        if (self.selectedRange.length &&
            [delegate textView:self
                shouldChangeTextInRange:self.selectedRange
                      replacementString:nil])
            self.editable = waseditable;
    }

    else if (menuItem.action == @selector(paste:)) {
        if ([delegate textView:self
                shouldChangeTextInRange:self.selectedRange
                      replacementString:nil])
            self.editable = waseditable;
    }

    if (menuItem.action == @selector(performTextFinderAction:))
        isValidItem = [self.textFinder validateAction:menuItem.tag];

    // validate other menu items if needed
    // ...
    // and don't forget to call the superclass
    else {
        isValidItem = [super validateMenuItem:menuItem];
    }

    self.editable = waseditable;

    return isValidItem;
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

- (NSString *)accessibilityActionDescription:(NSString *)action {
    if (@available(macOS 10.13, *)) {
    } else {
    if ([action isEqualToString:@"Repeat last move"])
        return @"repeat the text output of the last move";
    if ([action isEqualToString:@"Speak move before"])
        return @"step backward through moves";
    if ([action isEqualToString:@"Speak move after"])
        return @"step forward through moves";
    if ([action isEqualToString:@"Speak status bar"])
        return @"read status bar text";
    }

    return [super accessibilityActionDescription:action];
}

- (NSArray *)accessibilityCustomActions API_AVAILABLE(macos(10.13)) {
    GlkTextBufferWindow *delegate = (GlkTextBufferWindow *)self.delegate;
    NSArray *actions = [delegate.glkctl accessibilityCustomActions];
    return actions;
}


- (NSArray *)accessibilityActionNames {
    NSMutableArray *result = [[super accessibilityActionNames] mutableCopy];

    if (@available(macOS 10.13, *)) {
    } else {
    [result addObjectsFromArray:@[
        @"Repeat last move", @"Speak move before", @"Speak move after",
        @"Speak status bar"
    ]];
    }
    return result;
}

- (void)accessibilityPerformAction:(NSString *)action {
    if (@available(macOS 10.13, *)) {
        [super accessibilityPerformAction:action];
    } else {
    GlkTextBufferWindow *delegate = (GlkTextBufferWindow *)self.delegate;

    if ([action isEqualToString:@"Repeat last move"])
        [delegate.glkctl speakMostRecent:nil];
    else if ([action isEqualToString:@"Speak move before"])
        [delegate.glkctl speakPrevious:nil];
    else if ([action isEqualToString:@"Speak move after"])
        [delegate.glkctl speakNext:nil];
    else if ([action isEqualToString:@"Speak status bar"])
        [delegate.glkctl speakStatus:nil];
    else
        [super accessibilityPerformAction:action];
    }
}

- (NSArray *)accessibilityCustomRotors  {
   return [((GlkTextBufferWindow *)self.delegate).glkctl createCustomRotors];
}

- (NSArray *)accessibilityChildren {
    NSArray *children = [super accessibilityChildren];

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
