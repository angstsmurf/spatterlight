//
//  OSCategories.m
//  CocoaImageHashing
//
//  These files are adapted from CocoaImageHashing by Andreas Meingas.
//  See https://github.com/ameingast/cocoaimagehashing for the complete library.
//
//  Created by Andreas Meingast on 10/10/15.
//  Copyright © 2015 Andreas Meingast. All rights reserved.
//

#import "OSCategories.h"

#import <libkern/OSAtomic.h>

#define OS_ALIGN(x, multiple) ({ __typeof__(x) m = (multiple) - 1; ((x) + m) & ~m; })

#pragma mark - NSData Category

OS_INLINE OS_ALWAYS_INLINE NSUInteger OSBytesPerRowForWidth(NSUInteger width)
{
    return (width == 8) ? 32 : OS_ALIGN(4 * width, 64);
}


@implementation NSData (CocoaImageHashing)

- (NSData *)RGBABitmapDataForResizedImageWithWidth:(NSUInteger)width
                                         andHeight:(NSUInteger)height
{
    NSBitmapImageRep *sourceImageRep = [NSBitmapImageRep imageRepWithData:self];
    if (!sourceImageRep) {
        return nil;
    }
    NSBitmapImageRep *imageRep = [NSBitmapImageRep imageRepFrom:sourceImageRep
                                                  scaledToWidth:width
                                                 scaledToHeight:height
                                             usingInterpolation:NSImageInterpolationHigh];
    if (!imageRep) {
        return nil;
    }
    unsigned char *pixels = [imageRep bitmapData];
    NSData *result = [NSData dataWithBytes:pixels
                                    length:OSBytesPerRowForWidth(width) * height];
    return result;
}

@end


#pragma mark - NSBitmapImageRep Category


@implementation NSBitmapImageRep (CocoaImageHashing)

+ (NSBitmapImageRep *)imageRepFrom:(NSBitmapImageRep *)sourceImageRep
                     scaledToWidth:(NSUInteger)width
                    scaledToHeight:(NSUInteger)height
                usingInterpolation:(NSImageInterpolation)imageInterpolation
{
    NSBitmapImageRep *imageRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                                                         pixelsWide:(NSInteger)width
                                                                         pixelsHigh:(NSInteger)height
                                                                      bitsPerSample:8
                                                                    samplesPerPixel:4
                                                                           hasAlpha:YES
                                                                           isPlanar:NO
                                                                     colorSpaceName:NSDeviceRGBColorSpace
                                                                        bytesPerRow:(NSInteger)OSBytesPerRowForWidth(width)
                                                                       bitsPerPixel:0];
    [NSGraphicsContext saveGraphicsState];
    NSGraphicsContext *context = [NSGraphicsContext graphicsContextWithBitmapImageRep:imageRep];
    context.imageInterpolation = imageInterpolation;
    [NSGraphicsContext setCurrentContext:context];
    [sourceImageRep drawInRect:NSMakeRect(0, 0, width, height)];
    [context flushGraphics];
    [NSGraphicsContext restoreGraphicsState];
    [imageRep setSize:NSMakeSize(width, height)];
    return imageRep;
}

@end
