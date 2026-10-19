//
//  NSBitmapImageRep+TestImages.m
//  SpatterlightTests
//

#import "NSBitmapImageRep+TestImages.h"

@implementation NSBitmapImageRep (TestImages)

+ (instancetype)blankRGBAWithWidth:(NSInteger)width height:(NSInteger)height {
    return [[self alloc] initWithBitmapDataPlanes:NULL
                                       pixelsWide:width
                                       pixelsHigh:height
                                    bitsPerSample:8
                                  samplesPerPixel:4
                                         hasAlpha:YES
                                         isPlanar:NO
                                   colorSpaceName:NSDeviceRGBColorSpace
                                      bytesPerRow:0
                                     bitsPerPixel:0];
}

@end
