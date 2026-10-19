//
//  NSFont+Categories.h
//  Spatterlight
//
//  Created by Administrator on 2021-06-13.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSFont (Categories)

- (NSFont *)fontToFitWidth:(CGFloat)desiredWidth sampleText:(NSString *)text;
// This font, or a smaller one if the longest space-separated word of text
// would not fit in width: a title wraps between words but not inside them.
- (NSFont *)fontFittingLongestWordOf:(NSString *)text inWidth:(CGFloat)width;
- (CGFloat)widthForPointSize:(CGFloat)guess sampleText:(NSString *)text;
- (BOOL)hasNonBreakingSpaceGlyph;

@end

NS_ASSUME_NONNULL_END
