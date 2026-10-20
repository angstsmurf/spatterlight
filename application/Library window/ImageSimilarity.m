//
//  ImageSimilarity.m
//  Spatterlight
//

#import <Vision/Vision.h>

#import "ImageSimilarity.h"

// Vision feature print distance (revision 1) at or below which two images
// count as the same picture. Measured on 79 covers and pixel-art images with
// 14 variants of each (re-encoded, resized, pixel art upscaled, squashed,
// letterboxed, framed, cropped 5%, brightness, contrast, greyscale): this
// catches about 90% of the variants, while the closest pair of different
// pictures was 15.8 apart. Revision 2 has a different scale. Small pixel art
// that is halved or letterboxed can land above the threshold, and then the
// user is asked.
static const float kSamePictureDistance = 12;

@implementation ImageSimilarity

// Decodes the image and flattens it onto white, so that a transparent cover
// and an opaque copy of it look the same to Vision.
static CGImageRef CreateOpaqueImage(NSData *data) {
    CGImageSourceRef source = CGImageSourceCreateWithData((__bridge CFDataRef)data, NULL);
    if (!source)
        return NULL;
    CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, NULL);
    CFRelease(source);
    if (!image)
        return NULL;

    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(NULL, width, height, 8, 0, colorSpace, (CGBitmapInfo)kCGImageAlphaNoneSkipLast);
    CGColorSpaceRelease(colorSpace);
    if (!context) {
        CGImageRelease(image);
        return NULL;
    }
    CGRect rect = CGRectMake(0, 0, width, height);
    CGContextSetRGBFillColor(context, 1, 1, 1, 1);
    CGContextFillRect(context, rect);
    CGContextDrawImage(context, rect, image);
    CGImageRelease(image);
    CGImageRef opaque = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    return opaque;
}

static VNFeaturePrintObservation *FeaturePrint(NSData *data) API_AVAILABLE(macos(10.15)) {
    CGImageRef image = CreateOpaqueImage(data);
    if (!image)
        return nil;
    VNGenerateImageFeaturePrintRequest *request = [VNGenerateImageFeaturePrintRequest new];
    request.revision = VNGenerateImageFeaturePrintRequestRevision1;
    VNImageRequestHandler *handler = [[VNImageRequestHandler alloc] initWithCGImage:image options:@{}];
    NSError *error = nil;
    BOOL success = [handler performRequests:@[request] error:&error];
    CGImageRelease(image);
    if (!success) {
        NSLog(@"ImageSimilarity: could not make a feature print: %@", error);
        return nil;
    }
    return (VNFeaturePrintObservation *)request.results.firstObject;
}

+ (BOOL)imageData:(NSData *)imageA looksLike:(NSData *)imageB {
    if ([imageA isEqualToData:imageB])
        return YES;
    if (@available(macOS 10.15, *)) {
        VNFeaturePrintObservation *printA = FeaturePrint(imageA);
        VNFeaturePrintObservation *printB = FeaturePrint(imageB);
        if (!printA || !printB)
            return NO;
        float distance = 0;
        NSError *error = nil;
        if (![printA computeDistance:&distance toFeaturePrintObservation:printB error:&error]) {
            NSLog(@"ImageSimilarity: could not compare feature prints: %@", error);
            return NO;
        }
        NSLog(@"ImageSimilarity: distance %.2f", distance);
        return distance <= kSamePictureDistance;
    }
    return NO;
}

@end
