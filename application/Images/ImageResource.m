//
//  ImageResource.m
//  Spatterlight
//

#import "ImageResource.h"
#import "DebugLog.h"
#import "NSData+Categories.h"

@implementation ImageResource

- (instancetype)initWithFilename:(NSString *)filename offset:(NSUInteger)offset length:(NSUInteger)length {
    self = [super init];
    if (self) {
        _filename = [filename copy];
        _offset = offset;
        _length = length;
    }
    return self;
}

- (instancetype)initWithFilename:(NSString *)filename offset:(NSUInteger)offset data:(NSData *)data {
    self = [self initWithFilename:filename offset:offset length:data.length];
    if (self)
        _data = data;
    return self;
}

- (BOOL)loadWithError:(NSError * __autoreleasing *)outError {
    NSError *error = nil;
    NSData *data = nil;
    if (_filename.length)
        data = [NSData dataFromFileURL:[NSURL fileURLWithPath:_filename isDirectory:NO] offset:_offset length:_length error:&error];

    if (data.length == 0) {
        NSString *message = [NSString stringWithFormat:@"Could not read image resource from file %@, length %lu, offset %lu", _filename, (unsigned long)_length, (unsigned long)_offset];
        NSLog(@"%@: %@", message, error);
        if (outError)
            *outError = error ?: [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError
                                                 userInfo:@{ NSLocalizedDescriptionKey: message }];
        return NO;
    }
    // A short read is kept: createImage may still decode it, or fall back on
    // the whole file.
    if (_length && data.length < _length)
        NSLog(@"Image resource in %@ at offset %lu is %lu bytes, expected %lu", _filename, (unsigned long)_offset, (unsigned long)data.length, (unsigned long)_length);

    _data = data;
    return YES;
}

// A Blorb Rect chunk: a picture with a size and no content, as two 32-bit
// big-endian values, width then height.
static const NSUInteger kPlaceholderLength = 8;

- (nullable NSImage *)placeholderImage {
    const unsigned char *b = _data.bytes;
    uint32_t width = (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
    uint32_t height = (uint32_t)b[4] << 24 | (uint32_t)b[5] << 16 | (uint32_t)b[6] << 8 | b[7];
    // An NSImage with a zero dimension is taken to have no size at all, so
    // both are nudged off zero.
    return [[NSImage alloc] initWithSize:NSMakeSize((CGFloat)width + 0.01, (CGFloat)height + 0.01)];
}

- (nullable NSImage *)createImage {
    if (!_data)
        return nil;
    if (_data.length == kPlaceholderLength)
        return [self placeholderImage];

    NSArray<NSImageRep *> *reps = [NSBitmapImageRep imageRepsWithData:_data];
    if (reps.count == 0 && _filename.length && _offset == 0) {
        // A standalone image file: NSImage reads some formats from a file
        // that NSBitmapImageRep does not decode from the same bytes in
        // memory (b6655bd29).
        reps = [[NSImage alloc] initWithContentsOfFile:_filename].representations;
    }
    if (reps.count == 0)
        return nil;

    // Sized in pixels, whatever the resolution the file declares: Glk
    // measures pictures in pixels.
    NSImageRep *rep = reps[0];
    NSSize size = NSMakeSize((CGFloat)rep.pixelsWide, (CGFloat)rep.pixelsHigh);
    if (size.height == 0 || size.width == 0) {
        NSLog(@"ImageResource: image size is zero!");
        return nil;
    }

    NSImage *img = [[NSImage alloc] initWithSize:size];
    [img addRepresentations:reps];
    return img;
}

@end
