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
- (CGFloat)widthForPointSize:(CGFloat)guess sampleText:(NSString *)text;

@end

NS_ASSUME_NONNULL_END
