//
//  NSBitmapImageRep+TestImages.h
//  SpatterlightTests
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSBitmapImageRep (TestImages)

// A blank (transparent) 8-bit RGBA bitmap in device RGB.
+ (instancetype)blankRGBAWithWidth:(NSInteger)width height:(NSInteger)height;

@end

NS_ASSUME_NONNULL_END
