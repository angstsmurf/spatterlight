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
    static const Byte magic[4] = { 0x89, 'P', 'N', 'G' };
    return self.length > sizeof(magic) && memcmp(self.bytes, magic, sizeof(magic)) == 0;
}

// Any JPEG starts with an SOI marker followed by another marker. Testing for
// "JFIF" at offset 6 missed Exif (camera) files.
- (BOOL)isJPEG {
    static const Byte magic[3] = { 0xFF, 0xD8, 0xFF };
    return self.length > sizeof(magic) && memcmp(self.bytes, magic, sizeof(magic)) == 0;
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

    NSSize pixelSize = NSMakeSize((CGFloat)imageRep.pixelsWide,  (CGFloat)imageRep.pixelsHigh);
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
