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

static BOOL voProtoLog(void);

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
    if (![[GlkTextBufferWindow voProtoMode] isEqualToString:@"focushold"])
        [self clearVoFocusedMoveElement];
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
    if (voProtoLog())
        NSLog(@"AX scrollRangeToVisible %@\n%@", NSStringFromRange(range), [NSThread.callStackSymbols subarrayWithRange:NSMakeRange(1, MIN(8, NSThread.callStackSymbols.count - 1))]);
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

- (id)accessibilityFocusedUIElement {
    id el = self.voFocusedMoveElement;
    if (el)
        return el;
    return super.accessibilityFocusedUIElement;
}

- (void)clearVoFocusedMoveElement {
    if (self.voFocusedMoveElement) {
        self.voFocusedMoveElement = nil;
        // "focusquiet"/"focusarm": take the focus back without telling
        // VoiceOver, so it keeps tracking the text area while the player types.
        if ([[GlkTextBufferWindow voProtoMode] isEqualToString:@"focus"])
            NSAccessibilityPostNotification(self, NSAccessibilityFocusedUIElementChangedNotification);
    }
    self.voValueIsLastMove = NO;
}

// Prototype instrumentation: log which text APIs VoiceOver reads, when
// SPATTERLIGHT_VO_PROTO_LOG is set.
static BOOL voProtoLog(void) {
    static BOOL on; static dispatch_once_t once;
    dispatch_once(&once, ^{ on = NSProcessInfo.processInfo.environment[@"SPATTERLIGHT_VO_PROTO_LOG"] != nil; });
    return on;
}
#define VOLOG(fmt, ...) do { if (voProtoLog()) NSLog(@"AX %s " fmt, __FUNCTION__, ##__VA_ARGS__); } while (0)

// Prototype instrumentation: who scrolls the text view while VoiceOver reads.
- (void)viewDidMoveToSuperview {
    [super viewDidMoveToSuperview];
    if (!voProtoLog())
        return;
    NSClipView *clip = self.enclosingScrollView.contentView;
    if (!clip)
        return;
    [NSNotificationCenter.defaultCenter addObserverForName:NSViewBoundsDidChangeNotification object:clip queue:nil usingBlock:^(NSNotification *note) {
        NSArray *stack = NSThread.callStackSymbols;
        NSLog(@"AX clip bounds now %@\n%@", NSStringFromRect(clip.bounds), [stack subarrayWithRange:NSMakeRange(0, MIN(14, stack.count))]);
    }];
}

#pragma mark - "tail" modes: the accessible text starts at the last move

// VoiceOver reads a text area from its first character whenever its cursor
// lands on it, and scrolls there. In the "tail" modes the text area tells
// VoiceOver that its text starts where the last move starts, so landing on
// it reads the last move (and the input line), and the scrollback stays
// where it is. Older moves are reached through the move elements and the
// command history rotor. All character indexes VoiceOver sees are offset
// by voTailOffset; the methods below translate both ways.

- (NSUInteger)voTailOffset {
    if (![GlkTextBufferWindow voProtoEnabled] || ![[GlkTextBufferWindow voProtoMode] containsString:@"tail"])
        return 0;
    NSValue *last = ((GlkTextBufferWindow *)self.delegate).moveRanges.lastObject;
    if (!last)
        return 0;
    return MIN(last.rangeValue.location, self.textStorage.length);
}

static NSRange voToVirtual(NSRange real, NSUInteger off) {
    if (real.location == NSNotFound || off == 0)
        return real;
    if (NSMaxRange(real) <= off)
        return NSMakeRange(0, 0);
    if (real.location < off)
        return NSMakeRange(0, NSMaxRange(real) - off);
    return NSMakeRange(real.location - off, real.length);
}

static NSRange voToReal(NSRange virt, NSUInteger off) {
    if (virt.location == NSNotFound)
        return virt;
    return NSMakeRange(virt.location + off, virt.length);
}

// The line number of the first accessible character, in the full text.
- (NSInteger)voTailLine {
    NSUInteger off = self.voTailOffset;
    return off ? [super accessibilityLineForIndex:(NSInteger)off] : 0;
}

// For our own callers, which work in full-text ranges.
- (NSAttributedString *)accessibilityAttributedStringForFullTextRange:(NSRange)range {
    return [super accessibilityAttributedStringForRange:range];
}

// VoiceOver scrolls its cursor's element into view by its frame, and the
// text view's frame is the whole document, so it scrolled to the top.
// Report the frame of the accessible tail instead.
- (NSRect)accessibilityFrame {
    NSUInteger off = self.voTailOffset;
    if (!off)
        return super.accessibilityFrame;
    NSRect frame = [super accessibilityFrameForRange:NSMakeRange(off, self.textStorage.length - off)];
    VOLOG("-> %@", NSStringFromRect(frame));
    return NSIsEmptyRect(frame) ? super.accessibilityFrame : frame;
}

- (NSString *)accessibilityStringForRange:(NSRange)range {
    VOLOG("%@", NSStringFromRange(range));
    return [super accessibilityStringForRange:voToReal(range, self.voTailOffset)];
}
- (NSAttributedString *)accessibilityAttributedStringForRange:(NSRange)range {
    VOLOG("%@", NSStringFromRange(range));
    return [super accessibilityAttributedStringForRange:voToReal(range, self.voTailOffset)];
}
- (NSData *)accessibilityRTFForRange:(NSRange)range {
    VOLOG("%@", NSStringFromRange(range));
    return [super accessibilityRTFForRange:voToReal(range, self.voTailOffset)];
}
- (NSRange)accessibilityVisibleCharacterRange {
    NSRange r = voToVirtual(super.accessibilityVisibleCharacterRange, self.voTailOffset);
    VOLOG("-> %@", NSStringFromRange(r));
    return r;
}
- (void)setAccessibilityVisibleCharacterRange:(NSRange)range {
    VOLOG("%@", NSStringFromRange(range));
    [super setAccessibilityVisibleCharacterRange:voToReal(range, self.voTailOffset)];
}
- (NSInteger)accessibilityNumberOfCharacters {
    NSInteger n = super.accessibilityNumberOfCharacters - (NSInteger)self.voTailOffset;
    VOLOG("-> %ld", (long)n);
    return MAX(n, 0);
}
- (NSInteger)accessibilityLineForIndex:(NSInteger)index {
    VOLOG("%ld", (long)index);
    NSUInteger off = self.voTailOffset;
    if (!off)
        return [super accessibilityLineForIndex:index];
    return MAX([super accessibilityLineForIndex:index + (NSInteger)off] - self.voTailLine, 0);
}
- (NSRange)accessibilityRangeForLine:(NSInteger)line {
    VOLOG("%ld", (long)line);
    NSUInteger off = self.voTailOffset;
    if (!off)
        return [super accessibilityRangeForLine:line];
    return voToVirtual([super accessibilityRangeForLine:line + self.voTailLine], off);
}
- (NSInteger)accessibilityInsertionPointLineNumber {
    NSInteger n = super.accessibilityInsertionPointLineNumber;
    if (self.voTailOffset)
        n = MAX(n - self.voTailLine, 0);
    VOLOG("-> %ld", (long)n);
    return n;
}
- (NSRange)accessibilitySelectedTextRange {
    NSRange r = voToVirtual(super.accessibilitySelectedTextRange, self.voTailOffset);
    VOLOG("-> %@", NSStringFromRange(r));
    return r;
}
- (void)setAccessibilitySelectedTextRange:(NSRange)range {
    VOLOG("%@", NSStringFromRange(range));
    [super setAccessibilitySelectedTextRange:voToReal(range, self.voTailOffset)];
}
- (NSArray<NSValue *> *)accessibilitySelectedTextRanges {
    VOLOG("");
    NSUInteger off = self.voTailOffset;
    if (!off)
        return super.accessibilitySelectedTextRanges;
    NSMutableArray *ranges = [NSMutableArray new];
    for (NSValue *v in super.accessibilitySelectedTextRanges)
        [ranges addObject:[NSValue valueWithRange:voToVirtual(v.rangeValue, off)]];
    return ranges;
}
- (void)setAccessibilitySelectedTextRanges:(NSArray<NSValue *> *)ranges {
    VOLOG("%@", ranges);
    NSUInteger off = self.voTailOffset;
    if (off) {
        NSMutableArray *real = [NSMutableArray new];
        for (NSValue *v in ranges)
            [real addObject:[NSValue valueWithRange:voToReal(v.rangeValue, off)]];
        ranges = real;
    }
    [super setAccessibilitySelectedTextRanges:ranges];
}
- (NSRange)accessibilityRangeForIndex:(NSInteger)index {
    VOLOG("%ld", (long)index);
    NSUInteger off = self.voTailOffset;
    return voToVirtual([super accessibilityRangeForIndex:index + (NSInteger)off], off);
}
- (NSRange)accessibilityRangeForPosition:(NSPoint)point {
    VOLOG("%@", NSStringFromPoint(point));
    return voToVirtual([super accessibilityRangeForPosition:point], self.voTailOffset);
}
- (NSRange)accessibilityStyleRangeForIndex:(NSInteger)index {
    VOLOG("%ld", (long)index);
    NSUInteger off = self.voTailOffset;
    return voToVirtual([super accessibilityStyleRangeForIndex:index + (NSInteger)off], off);
}
- (NSRect)accessibilityFrameForRange:(NSRange)range {
    VOLOG("%@", NSStringFromRange(range));
    return [super accessibilityFrameForRange:voToReal(range, self.voTailOffset)];
}
- (NSString *)accessibilitySelectedText {
    VOLOG("");
    return super.accessibilitySelectedText;
}
- (NSRange)accessibilitySharedCharacterRange {
    VOLOG("");
    return voToVirtual(super.accessibilitySharedCharacterRange, self.voTailOffset);
}

- (id)accessibilityValue {
    VOLOG("(flag %d)", self.voValueIsLastMove);
    // "...value" modes: the text area's value is the last move, so VoiceOver
    // reads that (and not "edit text" alone, or the whole buffer) when its
    // cursor is moved onto the text area.
    if (self.voValueIsLastMove ||
        ([GlkTextBufferWindow voProtoEnabled] && [[GlkTextBufferWindow voProtoMode] hasSuffix:@"value"])) {
        NSString *last = ((GlkTextBufferWindow *)self.delegate).lastMoveString;
        if (last.length)
            return last;
    }
    id value = super.accessibilityValue;
    NSUInteger off = self.voTailOffset;
    if (off && [value isKindOfClass:[NSString class]] && off <= ((NSString *)value).length)
        return [(NSString *)value substringFromIndex:off];
    return value;
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
    NSArray *moves = ((GlkTextBufferWindow *)self.delegate).moveAccessibilityElements;
    if (moves.count)
        children = [children arrayByAddingObjectsFromArray:moves];
    NSArray<MarginImage *> *marginImages = ((MarginContainer *)self.textContainer).marginImages;
    if (marginImages.count)
        children = [children arrayByAddingObjectsFromArray:marginImages];
    return children;
}

@end
