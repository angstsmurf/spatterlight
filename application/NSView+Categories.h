//
//  NSView+Categories.h
//  Spatterlight
//

#import <Cocoa/Cocoa.h>

#import "Theme.h"

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

// Caret shapes (issue #39), set per theme.

// The cell-wide rect for a block or underline caret at caretRect: the width
// of the character after the caret, or of a "0" at the end of a line. A caret
// that would stick out past the right edge moves to the start of the next
// line, as a terminal waits to wrap. Needs TextKit 1.
- (NSRect)caretShapeRect:(kCaretShapePrefsType)shape forCaretRect:(NSRect)caretRect;

// For drawInsertionPointInRect:color:turnedOn:. AppKit calls that right
// after drawRect:, in the same display pass, with the context still current
// and clipped to what it invalidated, so a block or underline is drawn then
// and there. Returns the rect the caret and the shape cover together, to keep
// for the next call and for widening invalidations; NSZeroRect when turned
// off, and for a line or no caret, which draw nothing here. The caller draws
// a line caret with super.
- (NSRect)drawCaretShape:(kCaretShapePrefsType)shape forCaretRect:(NSRect)caretRect color:(NSColor *)color turnedOn:(BOOL)flag lastShapeRect:(NSRect)lastShapeRect;

// For setNeedsDisplayInRect:avoidAdditionalLayout:. AppKit blinks and moves
// the caret by invalidating its own 1-point-wide rect, which would leave the
// rest of the shape behind: widen the rect to the last drawn shape when it
// touches it.
- (NSRect)rect:(NSRect)rect widenedToCaretShape:(NSRect)lastShapeRect;

@end

NS_ASSUME_NONNULL_END
