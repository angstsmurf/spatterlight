//
//  ImageHandlerTests.m
//  SpatterlightTests
//
//  application/Images on its own, without a game: the "last image" that
//  FINDIMAGE, LOADIMAGE and PURGEIMG leave for a draw request, lookups that
//  must not change it, Blorb Rect placeholders, PURGEIMG replacements as
//  Bocfel writes them (and which files the handler deletes, and when), and the frame
//  timing of an animated GIF.
//

#import <XCTest/XCTest.h>

#import "ImageHandler.h"
#import "ImageResource.h"
#import "XCTestCase+TemporaryDirectory.h"
#import "NSBitmapImageRep+TestImages.h"

@interface ImageHandlerTests : XCTestCase
@property NSURL *directory;
@end

@implementation ImageHandlerTests

- (void)setUp {
    _directory = [self makeTemporaryDirectory];
}

#pragma mark Helpers

- (NSString *)write:(NSData *)data named:(NSString *)name {
    NSString *path = [_directory URLByAppendingPathComponent:name].path;
    XCTAssertTrue([data writeToFile:path atomically:NO]);
    return path;
}

static void put16(NSMutableData *d, uint16_t v) {
    uint8_t b[2] = { (uint8_t)(v >> 8), (uint8_t)v };
    [d appendBytes:b length:2];
}

static void put32(NSMutableData *d, uint32_t v) {
    uint8_t b[4] = { (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v };
    [d appendBytes:b length:4];
}

// An RGBA TIFF laid out the way terps/bocfel/z6/writetiff.cpp writes one:
// header, pixels, then the IFD and its per-channel arrays.
static NSData *BocfelTIFF(uint16_t width, uint16_t height) {
    const uint32_t pixels = (uint32_t)width * height * 4;
    const uint32_t header = 8, entries = 15, ifd = 2 + entries * 12 + 4, array = 8;
    NSMutableData *d = [NSMutableData data];
    put16(d, 0x4d4d); put16(d, 42); put32(d, header + pixels);
    NSMutableData *rgba = [NSMutableData dataWithLength:pixels];
    memset(rgba.mutableBytes, 0x80, pixels);
    [d appendData:rgba];
    const uint32_t bps = header + pixels + ifd;
    void (^shortEntry)(uint16_t, uint16_t) = ^(uint16_t tag, uint16_t value) {
        put16(d, tag); put16(d, 3); put32(d, 1); put16(d, value); put16(d, 0);
    };
    void (^longEntry)(uint16_t, uint32_t) = ^(uint16_t tag, uint32_t value) {
        put16(d, tag); put16(d, 4); put32(d, 1); put32(d, value);
    };
    void (^arrayEntry)(uint16_t, uint32_t) = ^(uint16_t tag, uint32_t offset) {
        put16(d, tag); put16(d, 3); put32(d, 4); put32(d, offset);
    };
    put16(d, (uint16_t)entries);
    shortEntry(0x0100, width);
    shortEntry(0x0101, height);
    arrayEntry(0x0102, bps);
    shortEntry(0x0103, 1);
    shortEntry(0x0106, 2);
    longEntry(0x0111, header);
    shortEntry(0x0112, 1);
    shortEntry(0x0115, 4);
    shortEntry(0x0116, height);
    longEntry(0x0117, pixels);
    arrayEntry(0x0118, bps + array);
    arrayEntry(0x0119, bps + 2 * array);
    shortEntry(0x011c, 1);
    shortEntry(0x0152, 1);
    arrayEntry(0x0153, bps + 3 * array);
    put32(d, 0);
    // BitsPerSample, MinSampleValue, MaxSampleValue, SampleFormat.
    const uint16_t arrays[4] = { 8, 0, 0xff, 1 };
    for (int a = 0; a < 4; a++)
        for (int i = 0; i < 4; i++)
            put16(d, arrays[a]);
    return d;
}

- (NSString *)writePNGNamed:(NSString *)name width:(NSInteger)width height:(NSInteger)height {
    NSBitmapImageRep *rep = [NSBitmapImageRep blankRGBAWithWidth:width height:height];
    return [self write:[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] named:name];
}

- (void)load:(NSInteger)resno path:(NSString *)path into:(ImageHandler *)handler {
    NSNumber *size = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:NULL][NSFileSize];
    [handler handleLoadImageNumber:resno from:path offset:0 size:size.unsignedIntegerValue];
}

#pragma mark The last image

- (void)testLoadAndFindMakeTheImageCurrent {
    ImageHandler *handler = [ImageHandler new];
    [self load:1 path:[self writePNGNamed:@"a.png" width:10 height:20] into:handler];
    XCTAssertEqual(handler.lastimageresno, 1);
    XCTAssertEqual(handler.lastimage.size.width, 10);
    XCTAssertEqual(handler.lastimage.size.height, 20);

    [self load:2 path:[self writePNGNamed:@"b.png" width:30 height:40] into:handler];
    XCTAssertEqual(handler.lastimageresno, 2);

    XCTAssertTrue([handler handleFindImageNumber:1]);
    XCTAssertEqual(handler.lastimageresno, 1);
    XCTAssertEqual(handler.lastimage.size.width, 10);
}

// A failed find leaves nothing for a draw request, not the previous picture.
- (void)testFailedFindClearsTheLastImage {
    ImageHandler *handler = [ImageHandler new];
    [self load:1 path:[self writePNGNamed:@"a.png" width:10 height:20] into:handler];
    XCTAssertFalse([handler handleFindImageNumber:99]);
    XCTAssertNil(handler.lastimage);
    XCTAssertEqual(handler.lastimageresno, -1);
}

- (void)testLoadOfMissingFileLeavesNoLastImage {
    ImageHandler *handler = [ImageHandler new];
    [self load:1 path:[self writePNGNamed:@"a.png" width:10 height:20] into:handler];
    [handler handleLoadImageNumber:2 from:[_directory URLByAppendingPathComponent:@"nothing.png"].path offset:0 size:100];
    XCTAssertNil(handler.lastimage);
    XCTAssertEqual(handler.lastimageresno, -1);
}

- (void)testLookupsLeaveTheLastImageAlone {
    ImageHandler *handler = [ImageHandler new];
    NSString *path = [self writePNGNamed:@"a.png" width:10 height:20];
    [self load:1 path:path into:handler];
    [self load:2 path:[self writePNGNamed:@"b.png" width:30 height:40] into:handler];
    NSImage *current = handler.lastimage;

    XCTAssertEqual([handler imageForNumber:1].size.width, 10);
    XCTAssertEqualObjects([handler dataForImageNumber:1], [NSData dataWithContentsOfFile:path]);
    XCTAssertNil([handler animationForImageNumber:1]);
    XCTAssertNil([handler imageForNumber:99]);

    XCTAssertEqual(handler.lastimageresno, 2);
    XCTAssertEqual(handler.lastimage, current);
}

#pragma mark Placeholders

// A Blorb Rect chunk: a size and no pixels, read as two 32-bit values.
- (void)testRectPlaceholder {
    ImageHandler *handler = [ImageHandler new];
    const uint8_t rect[8] = { 0, 1, 0x02, 0x58, 0, 0, 0, 200 };   // 65536 + 600 by 200
    NSString *path = [self write:[NSData dataWithBytes:rect length:8] named:@"rect"];
    [handler handleLoadImageNumber:5 from:path offset:0 size:8];
    XCTAssertEqual(handler.lastimageresno, 5);
    XCTAssertEqualWithAccuracy(handler.lastimage.size.width, 66136, 0.1);
    XCTAssertEqualWithAccuracy(handler.lastimage.size.height, 200, 0.1);
    XCTAssertGreaterThan(handler.lastimage.size.width, 0);

    // Made again from the resource, as after the cache has let it go.
    ImageResource *resource = [[ImageResource alloc] initWithFilename:path offset:0 data:[NSData dataWithBytes:rect length:8]];
    XCTAssertEqualWithAccuracy([resource createImage].size.height, 200, 0.1);
}

- (void)testRectPlaceholderWithZeroSizeIsNotEmpty {
    const uint8_t rect[8] = { 0 };
    ImageResource *resource = [[ImageResource alloc] initWithFilename:@"" offset:0 data:[NSData dataWithBytes:rect length:8]];
    NSImage *image = [resource createImage];
    XCTAssertNotNil(image);
    XCTAssertGreaterThan(image.size.width, 0);
    XCTAssertGreaterThan(image.size.height, 0);
}

#pragma mark Purging

// The IFD of a Bocfel TIFF comes after the pixels, so a replacement that
// decodes was read whole.
- (void)testPurgeReplacementReadsTheWholeFile {
    NSData *tiff = BocfelTIFF(12, 7);
    XCTAssertFalse([NSBitmapImageRep imageRepsWithData:[tiff subdataWithRange:NSMakeRange(0, 12 * 7 * 4)]].count,
                   @"the pixel-sized prefix alone does not decode");

    ImageHandler *handler = [ImageHandler new];
    NSString *path = [self write:tiff named:@"r1.tiff"];
    [handler purgeImage:600 withReplacement:path];
    XCTAssertEqual(handler.lastimageresno, 600);
    XCTAssertEqual(handler.lastimage.size.width, 12);
    XCTAssertEqual(handler.lastimage.size.height, 7);
    XCTAssertTrue([handler handleFindImageNumber:600]);
}

// A replacement's file is the handler's to delete, and it goes as soon as
// the picture is decoded: the image is made from the data read, so it
// survives the file, a cache eviction included.
- (void)testPurgeDeletesTheReplacementFileOnceDecoded {
    ImageHandler *handler = [ImageHandler new];
    NSString *first = [self write:BocfelTIFF(4, 4) named:@"r1.tiff"];
    NSString *second = [self write:BocfelTIFF(8, 2) named:@"r2.tiff"];
    [handler purgeImage:600 withReplacement:first];
    XCTAssertFalse([[NSFileManager defaultManager] fileExistsAtPath:first]);
    [handler purgeImage:600 withReplacement:second];
    XCTAssertFalse([[NSFileManager defaultManager] fileExistsAtPath:second]);
    XCTAssertEqual(handler.lastimage.size.width, 8);
    XCTAssertEqual([handler dataForImageNumber:600].length, BocfelTIFF(8, 2).length);

    [handler purgeImage:600 withReplacement:nil];
    XCTAssertNil(handler.lastimage);
    XCTAssertEqual(handler.lastimageresno, -1);
    XCTAssertFalse([handler handleFindImageNumber:600]);
}

// A replacement that does not decode keeps its file, so a later lookup can
// try again, until its number is purged again. (glkimp deletes whatever is
// left in its temp folder when the interpreter exits.)
- (void)testUndecodedReplacementKeepsItsFileUntilPurged {
    NSString *junk = [self write:[@"not a picture" dataUsingEncoding:NSASCIIStringEncoding] named:@"junk.tiff"];
    ImageHandler *handler = [ImageHandler new];
    [handler purgeImage:600 withReplacement:junk];
    XCTAssertNil(handler.lastimage);
    XCTAssertTrue([[NSFileManager defaultManager] fileExistsAtPath:junk]);
    [handler purgeImage:600 withReplacement:nil];
    XCTAssertFalse([[NSFileManager defaultManager] fileExistsAtPath:junk]);
}

// A file the game declared with LOADIMAGE (a PIC file, a game's own
// resource file) is not the handler's to delete.
- (void)testPurgeKeepsLoadedFiles {
    ImageHandler *handler = [ImageHandler new];
    NSString *path = [self writePNGNamed:@"PIC3" width:10 height:20];
    [self load:3 path:path into:handler];
    [handler purgeImage:3 withReplacement:nil];
    XCTAssertTrue([[NSFileManager defaultManager] fileExistsAtPath:path]);
    XCTAssertFalse([handler handleFindImageNumber:3]);

    // A purge of a picture never declared is harmless.
    [handler purgeImage:4 withReplacement:nil];
}

#pragma mark Resources

- (void)testResourceReadsItsRange {
    NSMutableData *file = [NSMutableData dataWithLength:10];
    [file appendData:[NSData dataWithContentsOfFile:[self writePNGNamed:@"a.png" width:3 height:5]]];
    [file appendData:[NSMutableData dataWithLength:10]];
    NSString *path = [self write:file named:@"bundle"];
    ImageResource *resource = [[ImageResource alloc] initWithFilename:path offset:10 length:file.length - 20];
    XCTAssertTrue([resource loadWithError:NULL]);
    XCTAssertEqual(resource.data.length, file.length - 20);
    XCTAssertEqual([resource createImage].size.width, 3);

    ImageResource *rest = [[ImageResource alloc] initWithFilename:path offset:10 length:0];
    XCTAssertTrue([rest loadWithError:NULL]);
    XCTAssertEqual(rest.data.length, file.length - 10);
}

- (void)testResourceLoadFailures {
    NSError *error = nil;
    ImageResource *missing = [[ImageResource alloc] initWithFilename:[_directory URLByAppendingPathComponent:@"none"].path offset:0 length:4];
    XCTAssertFalse([missing loadWithError:&error]);
    XCTAssertNotNil(error);
    XCTAssertNil(missing.data);

    ImageResource *unnamed = [[ImageResource alloc] initWithFilename:@"" offset:0 length:4];
    XCTAssertFalse([unnamed loadWithError:NULL]);

    NSString *path = [self write:[NSMutableData dataWithLength:4] named:@"short"];
    ImageResource *pastEnd = [[ImageResource alloc] initWithFilename:path offset:100 length:4];
    XCTAssertFalse([pastEnd loadWithError:NULL]);
    XCTAssertNil([pastEnd createImage]);
}

#pragma mark Animation

- (void)testAnimationTiming {
    NSURL *url = [_directory URLByAppendingPathComponent:@"anim.gif"];
    // Delays 0.5 s, none (played as 0.1 s) and 0.2 s; two passes.
    NSArray<NSNumber *> *delays = @[@0.5, @0, @0.2];
    CGImageDestinationRef destination = CGImageDestinationCreateWithURL((__bridge CFURLRef)url, CFSTR("com.compuserve.gif"), delays.count, NULL);
    NSDictionary *gif = @{ (__bridge NSString *)kCGImagePropertyGIFDictionary :
                               @{ (__bridge NSString *)kCGImagePropertyGIFLoopCount : @2 } };
    CGImageDestinationSetProperties(destination, (__bridge CFDictionaryRef)gif);
    NSBitmapImageRep *rep = [NSBitmapImageRep blankRGBAWithWidth:4 height:4];
    for (NSNumber *delay in delays) {
        NSDictionary *frame = @{ (__bridge NSString *)kCGImagePropertyGIFDictionary :
                                     @{ (__bridge NSString *)kCGImagePropertyGIFDelayTime : delay,
                                        (__bridge NSString *)kCGImagePropertyGIFUnclampedDelayTime : delay } };
        CGImageDestinationAddImage(destination, rep.CGImage, (__bridge CFDictionaryRef)frame);
    }
    XCTAssertTrue(CGImageDestinationFinalize(destination));
    CFRelease(destination);

    ImageHandler *handler = [ImageHandler new];
    [self load:7 path:url.path into:handler];
    ImageAnimation *animation = [handler animationForImageNumber:7];
    XCTAssertNotNil(animation);
    XCTAssertEqual(animation, [handler animationForImageNumber:7], @"one instance per image");
    XCTAssertEqual(animation.frameCount, 3);
    XCTAssertEqualWithAccuracy(animation.duration, 0.8, 0.001);
    XCTAssertEqual(animation.loopCount, 2);

    NSTimeInterval next = 0;
    XCTAssertEqual([animation frameAtTime:0 untilNext:&next], 0);
    XCTAssertEqualWithAccuracy(next, 0.5, 0.001);
    XCTAssertEqual([animation frameAtTime:0.55 untilNext:&next], 1);
    XCTAssertEqualWithAccuracy(next, 0.05, 0.001);
    XCTAssertEqual([animation frameAtTime:0.8 + 0.65 untilNext:&next], 2, @"second pass");
    XCTAssertEqual([animation frameAtTime:-1 untilNext:NULL], 0);
    XCTAssertEqual([animation frameAtTime:1.7 untilNext:&next], 2, @"stops on the last frame");
    XCTAssertEqual(next, INFINITY);

    XCTAssertEqual([animation imageForFrame:1], animation.image);
    XCTAssertNil([animation imageForFrame:3]);
}

@end
