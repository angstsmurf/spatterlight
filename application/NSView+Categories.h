//
//  NSView+Categories.h
//  Spatterlight
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSView (Categories)

// The first NSSearchField in this view's hierarchy (depth first, the view
// itself included), such as the one in an NSTextFinder find bar.
@property (readonly, nullable) NSSearchField *firstSearchField;

// Tracks a left-button press that began with mouseDown. Releasing within half
// a second calls click with the mouse-up event. Moving more than 3 points
// first calls drag with the drag event instead, so a click that wobbles a
// pixel is still a click. Either block may be nil.
- (void)trackClickOrDragFromEvent:(NSEvent *)mouseDown
                            click:(nullable void (^)(NSEvent *upEvent))click
                             drag:(nullable void (^)(NSEvent *dragEvent))drag;

@end

@interface NSTextView (Categories)

// The event's location in text container coordinates, as the layout manager
// and the text container expect it.
- (NSPoint)containerPointForEvent:(NSEvent *)event;

// The ranges of the NSLinkAttributeName runs within range of the text storage.
- (NSArray<NSValue *> *)linkRangesInRange:(NSRange)range;

@end

NS_ASSUME_NONNULL_END
