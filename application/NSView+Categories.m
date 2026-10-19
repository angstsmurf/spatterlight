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

@end
