//
//  OSImageHashing.m
//  CocoaImageHashing
//
//  These files are adapted from CocoaImageHashing by Andreas Meingas.
//  See https://github.com/ameingast/cocoaimagehashing for the complete library.
//
//  Created by Andreas Meingast on 10/10/15.
//  Copyright © 2015 Andreas Meingast. All rights reserved.
//

#import "OSImageHashing.h"
#import "OSCategories.h"
#import "OSFastGraphics.h"


static const SInt64 OSHashTypeError = -1;
static const NSUInteger OSPHashImageSideInPixels = 32;

@implementation OSImageHashing

+ (instancetype)sharedInstance
{
    static id instance;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
      instance = [self new];
    });
    return instance;
}

- (SInt64)hashImageData:(NSData *)imageData
{
    NSAssert(imageData, @"Image data must not be null");
    NSData *pixels = [imageData RGBABitmapDataForResizedImageWithWidth:OSPHashImageSideInPixels
                                     andHeight:OSPHashImageSideInPixels];
    if (!pixels) {
        return OSHashTypeError;
    }
    double greyscalePixels[OSPHashImageSideInPixels][OSPHashImageSideInPixels] = {{0.0}};
    double dctPixels[OSPHashImageSideInPixels][OSPHashImageSideInPixels] = {{0.0}};
    greyscale_pixels_rgba_32_32([pixels bytes], greyscalePixels);
    fast_dct_rgba_32_32(greyscalePixels, dctPixels);
    double dctAverage = fast_avg_no_first_el_rgba_8_8(dctPixels);
    SInt64 result = phash_rgba_8_8(dctPixels, dctAverage);
    return result;
}

- (BOOL)compareImageData:(NSData *)leftHandImageData
                      to:(NSData *)rightHandImageData
{
    NSAssert(leftHandImageData, @"Left hand image data must not be null");
    NSAssert(rightHandImageData, @"Right hand image data must not be null");
    SInt64 leftHandImageDataHash = [self hashImageData:leftHandImageData];
    SInt64 rightHandImageDataHash = [self hashImageData:rightHandImageData];
    if (leftHandImageDataHash == OSHashTypeError || rightHandImageDataHash == OSHashTypeError) {
        return NO;
    }

    // Only identical hashes are close enough for our purposes
    return leftHandImageDataHash == rightHandImageDataHash;
}


@end
