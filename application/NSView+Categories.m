//
//  NSView+Categories.m
//  Spatterlight
//

#import "NSView+Categories.h"

@implementation NSView (Categories)

- (nullable NSSearchField *)firstSearchField {
    if ([self isKindOfClass:[NSSearchField class]])
        return (NSSearchField *)self;
    for (NSView *subview in self.subviews) {
        NSSearchField *found = subview.firstSearchField;
        if (found)
            return found;
    }
    return nil;
}

- (void)trackClickOrDragFromEvent:(NSEvent *)mouseDown
                            click:(nullable void (^)(NSEvent *upEvent))click
                             drag:(nullable void (^)(NSEvent *dragEvent))drag {
    const CGFloat dragThreshold = 3;
    NSDate *mouseTime = [NSDate date];
    NSPoint location = [self convertPoint:mouseDown.locationInWindow fromView:nil];
    NSEventMask eventMask = NSEventMaskLeftMouseDown | NSEventMaskLeftMouseDragged | NSEventMaskLeftMouseUp;

    [self.window trackEventsMatchingMask:eventMask timeout:NSEventDurationForever mode:NSEventTrackingRunLoopMode handler:^(NSEvent * _Nullable event, BOOL * _Nonnull stop) {
        if (!event)
            return;

        if (event.type == NSEventTypeLeftMouseUp) {
            *stop = YES;
            if (mouseTime.timeIntervalSinceNow > -0.5 && click)
                click(event);
        } else if (event.type == NSEventTypeLeftMouseDragged) {
            NSPoint movedLocation = [self convertPoint:event.locationInWindow fromView:nil];
            if (ABS(movedLocation.x - location.x) > dragThreshold || ABS(movedLocation.y - location.y) > dragThreshold) {
                *stop = YES;
                if (drag)
                    drag(event);
            }
        }
    }];
}

@end

@implementation NSTextView (Categories)

- (NSPoint)containerPointForEvent:(NSEvent *)event {
    NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    point.x -= self.textContainerInset.width;
    point.y -= self.textContainerInset.height;
    return point;
}

- (NSArray<NSValue *> *)linkRangesInRange:(NSRange)range {
    NSMutableArray<NSValue *> *links = [[NSMutableArray alloc] init];
    [self.textStorage enumerateAttribute:NSLinkAttributeName
                                 inRange:range
                                 options:0
                              usingBlock:^(id value, NSRange subrange, BOOL *stop) {
        if (value)
            [links addObject:[NSValue valueWithRange:subrange]];
    }];
    return links;
}

typedef NS_ENUM(NSInteger, CaretShape) {
    kCaretLine,
    kCaretBlock,
    kCaretUnderline,
};

static CaretShape caretShape(void) {
    static CaretShape shape;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        const char *env = getenv("SPATTERLIGHT_CARET");
        if (env && !strcmp(env, "block"))
            shape = kCaretBlock;
        else if (env && !strcmp(env, "underline"))
            shape = kCaretUnderline;
    });
    return shape;
}

+ (BOOL)drawsCaretShape {
    return caretShape() != kCaretLine;
}

- (NSRect)caretShapeRectForCaretRect:(NSRect)caretRect {
    NSRect rect = caretRect;
    NSString *string = self.string;
    NSUInteger index = self.selectedRange.location;
    NSLayoutManager *layoutManager = self.layoutManager;
    CGFloat width = 0;
    if (index < string.length && [string characterAtIndex:index] != '\n') {
        NSRange glyphs = [layoutManager glyphRangeForCharacterRange:NSMakeRange(index, 1) actualCharacterRange:NULL];
        width = [layoutManager boundingRectForGlyphRange:glyphs inTextContainer:self.textContainer].size.width;
    }
    if (width < 1) {
        NSFont *font = self.typingAttributes[NSFontAttributeName] ?: self.font;
        width = [@"0" sizeWithAttributes:@{ NSFontAttributeName: font ?: [NSFont userFixedPitchFontOfSize:0] }].width;
    }
    rect.size.width = ceil(width);

    NSTextContainer *container = self.textContainer;
    CGFloat left = self.textContainerOrigin.x + container.lineFragmentPadding;
    CGFloat right = self.textContainerOrigin.x + container.size.width - container.lineFragmentPadding;
    if (NSMaxX(rect) > right + 1) {
        rect.origin.x = left;
        rect.origin.y += rect.size.height;
    }

    if (caretShape() == kCaretUnderline) {
        CGFloat thickness = MAX(2, round(rect.size.height / 8));
        rect.origin.y = NSMaxY(rect) - thickness;
        rect.size.height = thickness;
    }
    return rect;
}

- (void)drawCaretShapeInRect:(NSRect)shapeRect color:(NSColor *)color {
    [NSGraphicsContext saveGraphicsState];
    NSRectClip(shapeRect);
    [color set];
    NSRectFill(shapeRect);

    // Redraw the character under a block in the background colour.
    NSString *string = self.string;
    NSUInteger index = self.selectedRange.location;
    if (caretShape() == kCaretBlock && index < string.length && [string characterAtIndex:index] != '\n') {
        NSLayoutManager *layoutManager = self.layoutManager;
        NSUInteger glyph = [layoutManager glyphIndexForCharacterAtIndex:index];
        NSRect fragment = [layoutManager lineFragmentRectForGlyphAtIndex:glyph effectiveRange:NULL];
        NSPoint location = [layoutManager locationForGlyphAtIndex:glyph];
        NSPoint origin = self.textContainerOrigin;
        NSMutableDictionary *attributes = [[self.textStorage attributesAtIndex:index effectiveRange:NULL] mutableCopy];
        NSColor *background = attributes[NSBackgroundColorAttributeName] ?: self.backgroundColor;
        attributes[NSForegroundColorAttributeName] = background;
        [attributes removeObjectForKey:NSBackgroundColorAttributeName];
        NSAttributedString *character = [[NSAttributedString alloc] initWithString:[string substringWithRange:NSMakeRange(index, 1)] attributes:attributes];
        // Without NSStringDrawingUsesLineFragmentOrigin the rect origin is the baseline.
        [character drawWithRect:NSMakeRect(origin.x + fragment.origin.x + location.x, origin.y + fragment.origin.y + location.y, 0, 0) options:0];
    }
    [NSGraphicsContext restoreGraphicsState];
}

@end
