//
//  NSData+Categories.m
//  Spatterlight
//
//  Created by Administrator on 2021-05-24.
//

#import <CommonCrypto/CommonDigest.h>

#import "NSData+Categories.h"

#import "NSBitmapImageRep+retro.h"
#import "NSImage+Categories.h"

@implementation NSData (MD5)

- (NSString *)md5String {
    unsigned char resultCString[CC_MD5_DIGEST_LENGTH];
    CC_MD5(self.bytes, (unsigned int)self.length, resultCString);
    
    return [NSString stringWithFormat:
            @"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
            resultCString[0], resultCString[1], resultCString[2], resultCString[3],
            resultCString[4], resultCString[5], resultCString[6], resultCString[7],
            resultCString[8], resultCString[9], resultCString[10], resultCString[11],
            resultCString[12], resultCString[13], resultCString[14], resultCString[15]
            ];
}

- (NSString *)sha256String {
    unsigned char resultCString[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(self.bytes, (unsigned int)self.length, resultCString);

    return [NSString stringWithFormat:
                @"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
            resultCString[0], resultCString[1], resultCString[2], resultCString[3],
            resultCString[4], resultCString[5], resultCString[6], resultCString[7],
            resultCString[8], resultCString[9], resultCString[10], resultCString[11],
            resultCString[12], resultCString[13], resultCString[14], resultCString[15],
            resultCString[16], resultCString[17], resultCString[18], resultCString[19],
            resultCString[20], resultCString[21], resultCString[22], resultCString[23],
            resultCString[24], resultCString[25], resultCString[26], resultCString[27],
            resultCString[28], resultCString[29], resultCString[30], resultCString[31]
    ];
}

- (BOOL)isPlaceHolderImage {
    return [self.md5String isEqualToString:@"1855A829466C301CD8113475E8E643BC"];
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
    NSData *data = [rep representationUsingType:NSBitmapImageFileTypeBMP properties:@{}];
    return data;
}
+ (nullable NSData *)imageDataFromNeoURL:(NSURL *)url {
    NSBitmapImageRep *rep = [NSBitmapImageRep repFromNeoURL:url];
    NSData *data = [rep representationUsingType:NSBitmapImageFileTypeBMP properties:@{}];
    return data;
}
+ (nullable NSData *)imageDataFromMG1URL:(NSURL *)url {
    NSBitmapImageRep *rep = [NSBitmapImageRep repFromMG1URL:url];
    NSData *data = [rep representationUsingType:NSBitmapImageFileTypeBMP properties:@{}];
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

    NSBitmapImageRep *bitmaprep = [image bitmapImageRepresentation];

    NSDictionary *props = @{ NSImageCompressionFactor: @(0.5) };
    return [NSBitmapImageRep representationOfImageRepsInArray:@[bitmaprep] usingType:NSBitmapImageFileTypeJPEG properties:props];
}

@end
