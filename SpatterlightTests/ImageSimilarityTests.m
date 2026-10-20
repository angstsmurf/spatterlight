//
//  ImageSimilarityTests.m
//  SpatterlightTests
//
//  ImageSimilarity on the covers of the Infocom Blorbs in Supporting Files:
//  each cover must look like re-encoded and flattened copies of itself, and
//  unlike the other covers.
//

#import <XCTest/XCTest.h>
#import <BlorbFramework/BlorbFramework.h>

#import "ImageSimilarity.h"

@interface ImageSimilarityTests : XCTestCase
@property NSArray<NSData *> *covers;
@end

@implementation ImageSimilarityTests

- (void)setUp {
    NSBundle *bundle = [NSBundle bundleForClass:[self class]];
    NSMutableArray *covers = [NSMutableArray new];
    for (NSString *name in @[ @"arthur", @"journey", @"shogun", @"zork0" ]) {
        NSData *cover = [Blorb coverImageDataAtURL:[bundle URLForResource:name withExtension:@"zblorb"]];
        XCTAssertNotNil(cover, @"%@.zblorb should have a cover", name);
        if (cover)
            [covers addObject:cover];
    }
    _covers = covers;
}

// Draws the cover into a canvas of its own size with a 10% margin filled
// with background, and encodes it as JPEG, or as PNG when the background is
// transparent.
static NSData *Redraw(NSData *data, NSColor *background) {
    NSBitmapImageRep *source = [NSBitmapImageRep imageRepWithData:data];
    NSInteger width = source.pixelsWide, height = source.pixelsHigh;
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL pixelsWide:width pixelsHigh:height bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:0 bitsPerPixel:0];
    [NSGraphicsContext saveGraphicsState];
    NSGraphicsContext.currentContext = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    NSGraphicsContext.currentContext.imageInterpolation = NSImageInterpolationHigh;
    [background setFill];
    NSRectFill(NSMakeRect(0, 0, width, height));
    [source drawInRect:NSInsetRect(NSMakeRect(0, 0, width, height), width / 10, height / 10) fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1 respectFlipped:NO hints:nil];
    [NSGraphicsContext restoreGraphicsState];
    if (background.alphaComponent == 0)
        return [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    return [rep representationUsingType:NSBitmapImageFileTypeJPEG properties:@{ NSImageCompressionFactor: @0.7 }];
}

- (void)testIdenticalDataLooksAlike {
    for (NSData *cover in _covers)
        XCTAssertTrue([ImageSimilarity imageData:cover looksLike:[cover copy]]);
}

// These covers are small pixel art, which Vision tells apart from resized or
// letterboxed copies of itself (so the user gets asked), but not from a
// re-encoded one.
- (void)testReencodedCoverLooksAlike {
    if (@available(macOS 10.15, *)) {
        for (NSData *cover in _covers) {
            NSBitmapImageRep *rep = [NSBitmapImageRep imageRepWithData:cover];
            NSData *jpeg = [rep representationUsingType:NSBitmapImageFileTypeJPEG properties:@{ NSImageCompressionFactor: @0.7 }];
            XCTAssertTrue([ImageSimilarity imageData:cover looksLike:jpeg]);
        }
    }
}

// A transparent margin must look like a white one.
- (void)testTransparencyIsFlattenedOntoWhite {
    if (@available(macOS 10.15, *)) {
        for (NSData *cover in _covers)
            XCTAssertTrue([ImageSimilarity imageData:Redraw(cover, NSColor.clearColor) looksLike:Redraw(cover, NSColor.whiteColor)]);
    }
}

- (void)testDifferentCoversDoNotLookAlike {
    XCTAssertEqual(_covers.count, 4u);
    for (NSUInteger i = 0; i < _covers.count; i++)
        for (NSUInteger j = i + 1; j < _covers.count; j++)
            XCTAssertFalse([ImageSimilarity imageData:_covers[i] looksLike:_covers[j]], @"covers %lu and %lu", i, j);
}

- (void)testUnreadableDataDoesNotLookLikeACover {
    NSData *junk = [@"not an image" dataUsingEncoding:NSUTF8StringEncoding];
    XCTAssertFalse([ImageSimilarity imageData:_covers.firstObject looksLike:junk]);
}

@end
