//
//  NSData+Categories.m
//  Spatterlight
//
//  Created by Administrator on 2021-05-24.
//

#import "NSData+Categories.h"

#import "NSBitmapImageRep+retro.h"
#import "NSImage+Categories.h"
#import "FileSignature.h"

@implementation NSData (Categories)

- (NSString *)sha256String {
    return [FileSignature sha256StringFromData:self];
}

- (NSString *)signature {
    return [FileSignature signatureFromData:self];
}

- (BOOL)isPlaceHolderImage {
    return [[FileSignature sha256StringFromData:self] isEqualToString:@"1F10EC3B41826AB7F51CF9392B37EFFC11F0961A17F83E8F2E0EF360FA0D79DC"];
}

- (BOOL)isPNG {
    BOOL result = NO;
    if (self.length > 4) {
        Byte *bytes4 = (Byte *)malloc(4);
        [self getBytes:bytes4 length:4];

        result = (bytes4[1] == 'P' && bytes4[2] == 'N' && bytes4[3] == 'G');
        free(bytes4);
    }
    return result;
}

- (BOOL)isJPEG {
    BOOL result = NO;
    if (self.length > 10) {
        Byte *bytes10 = (Byte *)malloc(10);
        [self getBytes:bytes10 length:10];
        result = (bytes10[6] == 'J' && bytes10[7] == 'F' && bytes10[8] == 'I' && bytes10[9] == 'F');
        free(bytes10);
    }
    return result;
}


#pragma mark Retro image data

+ (nullable NSData *)imageDataFromRetroURL:(NSURL *)url {
    NSBitmapImageRep *rep = [NSBitmapImageRep repFromURL:url];
    NSData *data = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    return data;
}
+ (nullable NSData *)imageDataFromNeoURL:(NSURL *)url {
    NSBitmapImageRep *rep = [NSBitmapImageRep repFromNeoURL:url];
    NSData *data = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    return data;
}
+ (nullable NSData *)imageDataFromMG1URL:(NSURL *)url {
    NSBitmapImageRep *rep = [NSBitmapImageRep repFromMG1URL:url];
    NSData *data = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    return data;
}

- (nullable NSData *)reduceImageDimensionsTo:(NSSize)size {
    NSImage *image = [[NSImage alloc] initWithData:self];
    if (!image.isValid)
        return nil;
    NSImageRep *imageRep = image.representations.firstObject;
    if (!imageRep)
        return nil;

    NSSize pixelSize = NSMakeSize(imageRep.pixelsWide,  imageRep.pixelsHigh);
    if (pixelSize.width <= size.width && pixelSize.height <= size.height)
        return self;

    CGFloat ratio = pixelSize.height / pixelSize.width;
    NSSize newImageSize = NSMakeSize(size.width, size.width * ratio);
    if (newImageSize.height > size.height) {
        newImageSize = NSMakeSize(size.height / ratio, size.height);
    }

    image = [image resizedToPixelDimensions:newImageSize];
    image.size = newImageSize;

    NSBitmapImageRep *bitmaprep = image.bitmapImageRepresentation;

    NSDictionary *props = @{ NSImageCompressionFactor: @(0.5) };
    return [NSBitmapImageRep representationOfImageRepsInArray:@[bitmaprep] usingType:NSBitmapImageFileTypeJPEG properties:props];
}

@end
