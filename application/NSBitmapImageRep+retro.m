//
//  NSBitmapImageRep+retro.m
//  Spatterlight
//
//  Created by Administrator on 2021-06-06.
//

#include "spat-mg1.h"
#include "neo.h"

#import "NSBitmapImageRep+retro.h"

@implementation NSBitmapImageRep (retro)

+ (NSBitmapImageRep *)repFromURL:(NSURL *)url {
    if ([url.lastPathComponent.lowercaseString isEqualToString:@"screen.dat"])
        return [NSBitmapImageRep repFromNeoURL:url];

    NSString *extension = url.pathExtension.lowercaseString;
    if ([extension isEqualToString:@"neo"]) {
        return [NSBitmapImageRep repFromNeoURL:url];
    } else if ([extension isEqualToString:@"mg1"]) {
        return [NSBitmapImageRep repFromMG1URL:url];
    }
    return nil;
}

// Wraps the RGB pixels of a decoded picture. The z_image itself still
// belongs to the decoder, which the caller ends afterwards.
static NSBitmapImageRep *SPRepFromZImage(z_image *zimg) {
    if (zimg == NULL || zimg->width < 1 || zimg->height < 1)
        return nil;

    // 3 bytes (rgb, no alpha) for each pixel
    NSUInteger bytesPerPixel = 3;
    NSUInteger bitsPerComponent = (NSUInteger)zimg->bits_per_sample;
    NSUInteger bitsPerPixel = bytesPerPixel * bitsPerComponent;

    NSUInteger bytesPerRow = (NSUInteger)(zimg->width * bytesPerPixel);

    // TODO: better colorspace for NEO and MG1 files?
    CGColorSpaceRef deviceColorSpace = CGColorSpaceCreateDeviceRGB();

    NSData *data =
    [NSData dataWithBytes:zimg->data length:(NSUInteger)(zimg->width * zimg->height * bytesPerPixel)];

    CGDataProviderRef cgDataProvider = CGDataProviderCreateWithCFData((__bridge CFDataRef)data);

    CGImageRef img = CGImageCreate(zimg->width, zimg->height, bitsPerComponent, bitsPerPixel, bytesPerRow, deviceColorSpace, kCGBitmapByteOrderDefault, cgDataProvider, nil, NO, kCGRenderingIntentDefault);

    NSBitmapImageRep *rep = nil;
    if (img) {
        rep = [[NSBitmapImageRep alloc]
               initWithCGImage:img];
        CFRelease(img);
    }

    CFRelease(deviceColorSpace);
    CFRelease(cgDataProvider);

    return rep;
}

+ (NSBitmapImageRep *)repFromNeoURL:(NSURL *)url {
    const char *filename = url.fileSystemRepresentation;

    NSBitmapImageRep *rep = SPRepFromZImage(get_neo_picture(filename));

    end_neo_graphics();

    return rep;
}

+ (NSBitmapImageRep *)repFromMG1URL:(NSURL *)url {

    const char *filename = url.fileSystemRepresentation;
    if (init_mg1_graphics(filename) != 0)
        return nil;
    uint16_t numImages = get_number_of_mg1_images();
    if (numImages < 1) {
        end_mg1_graphics();
        return nil;
    }

    uint16_t *pictureNumberArray = get_mg1_picture_numbers();
    if (pictureNumberArray == NULL) {
        end_mg1_graphics();
        return nil;
    }

    z_image *zimg = get_mg1_picture(pictureNumberArray[0]);
    free(pictureNumberArray);

    NSBitmapImageRep *rep = SPRepFromZImage(zimg);

    end_mg1_graphics();

    return rep;
}

@end
