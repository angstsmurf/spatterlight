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

@end

NS_ASSUME_NONNULL_END
