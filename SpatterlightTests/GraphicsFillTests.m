//
//  GraphicsFillTests.m
//  SpatterlightTests
//
//  GlkGraphicsWindow draws FILLRECT batches straight into its bitmap canvas.
//  These tests check the result pixel by pixel against the Glk model of
//  glk_window_fill_rect: opaque fills applied in order, later ones on top,
//  clipped to the window, in top-left-origin window coordinates.
//

#import "GlkControllerTestCase.h"

#import "GlkController.h"
#import "GlkGraphicsWindow.h"
#import "Theme.h"
#import "NSColor+integer.h"
#import "SubImage.h"

#include "glk.h"
#include "protocol.h"

static const NSInteger kWidth = 40;
static const NSInteger kHeight = 30;

@interface GraphicsFillTests : GlkControllerTestCase

// Windows hold their controller weakly, so the test keeps it alive.
@property (nonatomic, strong) GlkController *controller;

@end

@implementation GraphicsFillTests

- (void)tearDown {
    self.controller = nil;
    [super tearDown];
}

#pragma mark - Helpers

// Through NSKeyedArchiver and back, as autosave and autorestore do.
- (GlkGraphicsWindow *)archivedAndRestored:(GlkGraphicsWindow *)win {
    NSError *error = nil;
    NSData *archive = [NSKeyedArchiver archivedDataWithRootObject:win requiringSecureCoding:NO error:&error];
    XCTAssertNil(error);
    NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:archive error:&error];
    XCTAssertNil(error);
    unarchiver.requiresSecureCoding = NO;
    GlkGraphicsWindow *restored = [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
    [unarchiver finishDecoding];
    return restored;
}

- (GlkGraphicsWindow *)makeWindow {
    if (!self.controller) {
        self.controller = [[GlkController alloc] init];
        self.controller.theme = self.theme;
    }
    GlkGraphicsWindow *win = [[GlkGraphicsWindow alloc] initWithGlkController:self.controller name:1];
    win.frame = NSMakeRect(0, 0, kWidth, kHeight);
    XCTAssertNotNil(win.image, @"Sizing the window should create its canvas");
    return win;
}

// Copies the window's image into a context of the canvas's own format, so no
// conversion happens, and hands block its pixels as 0xAARRGGBB (premultiplied)
// and a function returning the one at the centre of a point's device pixels.
// y counts from the top, as in Glk.
- (void)readPixelsOfWindow:(GlkGraphicsWindow *)win
                     using:(void (^)(uint32_t (^pixelAt)(NSInteger x, NSInteger y)))block {
    NSImage *image = win.image;
    CGImageRef cgImage = [image CGImageForProposedRect:NULL context:nil hints:nil];
    XCTAssert(cgImage != NULL);
    size_t pixelsWide = CGImageGetWidth(cgImage);
    size_t pixelsHigh = CGImageGetHeight(cgImage);
    CGFloat scale = (CGFloat)pixelsWide / image.size.width;
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef readback = CGBitmapContextCreate(NULL, pixelsWide, pixelsHigh, 8, 0, space,
                                                  kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(space);
    CGContextSetBlendMode(readback, kCGBlendModeCopy);
    CGContextDrawImage(readback, CGRectMake(0, 0, pixelsWide, pixelsHigh), cgImage);
    const uint32_t *pixels = CGBitmapContextGetData(readback);
    size_t rowPixels = CGBitmapContextGetBytesPerRow(readback) / sizeof(uint32_t);
    block(^uint32_t(NSInteger x, NSInteger y) {
        size_t px = (size_t)(((CGFloat)x + 0.5) * scale);
        size_t py = (size_t)(((CGFloat)y + 0.5) * scale);
        return pixels[py * rowPixels + px];
    });
    CGContextRelease(readback);
}

// The window's image as 0x00RRGGBB values, one per point.
- (NSArray<NSNumber *> *)pointColorsOfWindow:(GlkGraphicsWindow *)win {
    NSMutableArray<NSNumber *> *colors = [NSMutableArray arrayWithCapacity:kWidth * kHeight];
    [self readPixelsOfWindow:win using:^(uint32_t (^pixelAt)(NSInteger, NSInteger)) {
        for (NSInteger y = 0; y < kHeight; y++)
            for (NSInteger x = 0; x < kWidth; x++)
                [colors addObject:@(pixelAt(x, y) & 0xFFFFFF)];
    }];
    return colors;
}

// One point of the window's image as 0xAARRGGBB (premultiplied).
- (uint32_t)pixelOfWindow:(GlkGraphicsWindow *)win x:(NSInteger)x y:(NSInteger)y {
    __block uint32_t pixel = 0;
    [self readPixelsOfWindow:win using:^(uint32_t (^pixelAt)(NSInteger, NSInteger)) {
        pixel = pixelAt(x, y);
    }];
    return pixel;
}

- (NSImage *)solidImageOfSize:(NSSize)size red:(CGFloat)r green:(CGFloat)g blue:(CGFloat)b {
    return [NSImage imageWithSize:size flipped:NO drawingHandler:^BOOL(NSRect dstRect) {
        [[NSColor colorWithDeviceRed:r green:g blue:b alpha:1] setFill];
        NSRectFill(dstRect);
        return YES;
    }];
}

- (NSArray<SubImage *> *)subImagesOfWindow:(GlkGraphicsWindow *)win {
    return [win.accessibilityChildren filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(id object, NSDictionary *bindings) {
        return [object isKindOfClass:[SubImage class]];
    }]];
}

// The Glk model: start from the background, paint each rect in order.
- (NSArray<NSNumber *> *)expectedColorsForRects:(const struct fillrect *)rects
                                          count:(NSInteger)count
                                     background:(uint32_t)background
                                   defaultColor:(uint32_t)defaultColor {
    NSMutableArray<NSNumber *> *colors = [NSMutableArray arrayWithCapacity:kWidth * kHeight];
    for (NSInteger i = 0; i < kWidth * kHeight; i++)
        [colors addObject:@(background)];
    for (NSInteger i = 0; i < count; i++) {
        const struct fillrect *r = &rects[i];
        uint32_t color = (r->color == zcolor_Default) ? defaultColor : (r->color & 0xFFFFFF);
        for (NSInteger y = MAX(0, r->y); y < MIN(kHeight, r->y + r->h); y++)
            for (NSInteger x = MAX(0, r->x); x < MIN(kWidth, r->x + r->w); x++)
                colors[(NSUInteger)(y * kWidth + x)] = @(color);
    }
    return colors;
}

- (void)assertWindow:(GlkGraphicsWindow *)win matches:(NSArray<NSNumber *> *)expected {
    NSArray<NSNumber *> *actual = [self pointColorsOfWindow:win];
    NSUInteger mismatches = 0;
    for (NSUInteger i = 0; i < expected.count; i++) {
        if (![actual[i] isEqualToNumber:expected[i]]) {
            if (mismatches++ < 5)
                XCTFail(@"Pixel (%lu, %lu): got 0x%06lx, expected 0x%06lx",
                        i % (NSUInteger)kWidth, i / (NSUInteger)kWidth,
                        actual[i].unsignedLongValue, expected[i].unsignedLongValue);
        }
    }
    XCTAssertEqual(mismatches, 0u);
}

#pragma mark - Tests

// Overlapping fills of different colours: the later one must win where they
// overlap, and the fill must not reorder or merge rects by colour.
- (void)testOverlappingFillsApplyInOrder {
    GlkGraphicsWindow *win = [self makeWindow];
    struct fillrect rects[] = {
        { 0xFF0000, 2, 2, 20, 10 },
        { 0x00FF00, 10, 5, 20, 10 },   // over the red one
        { 0xFF0000, 15, 8, 3, 3 },     // red again, on top of the green
        { 0x0000FF, 0, 20, 40, 1 },    // one-pixel-high span
        { 0x123456, 5, 22, 1, 6 },     // one-pixel-wide column
    };
    NSInteger count = (NSInteger)(sizeof rects / sizeof rects[0]);
    [win fillRects:rects count:count];

    [self assertWindow:win matches:[self expectedColorsForRects:rects count:count
                                                     background:0xFFFFFF
                                                   defaultColor:0]];
}

// Rects partly or wholly outside the window are clipped, and empty rects
// draw nothing.
- (void)testFillsAreClippedAndEmptyRectsIgnored {
    GlkGraphicsWindow *win = [self makeWindow];
    struct fillrect rects[] = {
        { 0x112233, -5, -5, 10, 10 },          // top-left corner, partly outside
        { 0x445566, 35, 25, 20, 20 },          // bottom-right corner, partly outside
        { 0x778899, 100, 100, 5, 5 },          // entirely outside
        { 0xAABBCC, 10, 10, 0, 5 },            // zero width
        { 0xDDEEFF, 10, 10, 5, 0 },            // zero height
    };
    NSInteger count = (NSInteger)(sizeof rects / sizeof rects[0]);
    [win fillRects:rects count:count];

    [self assertWindow:win matches:[self expectedColorsForRects:rects count:count
                                                     background:0xFFFFFF
                                                   defaultColor:0]];
}

// zcolor_Default is resolved against the theme's buffer background at draw time.
- (void)testDefaultColorUsesThemeBackground {
    GlkGraphicsWindow *win = [self makeWindow];
    uint32_t themeBackground = (uint32_t)self.theme.bufferBackground.integerColor;
    struct fillrect rects[] = {
        { 0x000000, 0, 0, 40, 30 },
        { zcolor_Default, 4, 4, 12, 12 },
    };
    NSInteger count = (NSInteger)(sizeof rects / sizeof rects[0]);
    [win fillRects:rects count:count];

    [self assertWindow:win matches:[self expectedColorsForRects:rects count:count
                                                     background:0xFFFFFF
                                                   defaultColor:themeBackground]];
}

// A run of alternating one-pixel spans, the shape a run-length-encoded map
// redraw arrives in.
- (void)testAlternatingSpansLikeAMapRedraw {
    GlkGraphicsWindow *win = [self makeWindow];
    NSMutableData *data = [NSMutableData data];
    for (short y = 0; y < kHeight; y++) {
        for (short x = 0; x < kWidth; x += 2) {
            struct fillrect r = { ((y + x) % 4 == 0) ? 0x202020 : 0xC0C0C0, x, y, 2, 1 };
            [data appendBytes:&r length:sizeof r];
        }
    }
    NSInteger count = (NSInteger)(data.length / sizeof(struct fillrect));
    [win fillRects:data.mutableBytes count:count];

    [self assertWindow:win matches:[self expectedColorsForRects:data.bytes count:count
                                                     background:0xFFFFFF
                                                   defaultColor:0]];
}

// Fills and image draws share one surface, in request order: an image drawn
// after a fill covers it, and a fill after the image covers that.
- (void)testFillsAndImageDrawsKeepRequestOrder {
    GlkGraphicsWindow *win = [self makeWindow];

    struct fillrect first[] = { { 0x00FF00, 0, 0, 40, 30 } };
    [win fillRects:first count:1];

    NSImage *red = [self solidImageOfSize:NSMakeSize(10, 10) red:1 green:0 blue:0];
    [win drawImage:red val1:5 val2:5 width:10 height:10 imagerule:0 maxwidth:0 style:style_Normal];

    struct fillrect last[] = { { 0x0000FF, 10, 10, 10, 10 } };
    [win fillRects:last count:1];

    struct fillrect model[] = {
        { 0x00FF00, 0, 0, 40, 30 },
        { 0xFF0000, 5, 5, 10, 10 },
        { 0x0000FF, 10, 10, 10, 10 },
    };
    [self assertWindow:win matches:[self expectedColorsForRects:model count:3
                                                     background:0xFFFFFF
                                                   defaultColor:0]];
}

// clear() repaints the whole canvas in the window background.
- (void)testClearFillsWithBackground {
    GlkGraphicsWindow *win = [self makeWindow];
    struct fillrect rects[] = { { 0x336699, 0, 0, 40, 30 } };
    [win fillRects:rects count:1];
    [win clear];

    [self assertWindow:win matches:[self expectedColorsForRects:NULL count:0
                                                     background:0xFFFFFF
                                                   defaultColor:0]];
}

// The canvas survives archiving as an NSImage, as autosave needs.
- (void)testCanvasRoundTripsThroughArchiving {
    GlkGraphicsWindow *win = [self makeWindow];
    struct fillrect rects[] = {
        { 0xFF8800, 3, 4, 17, 9 },
        { 0x0088FF, 20, 15, 15, 10 },
    };
    [win fillRects:rects count:2];

    NSError *error = nil;
    NSData *archive = [NSKeyedArchiver archivedDataWithRootObject:win.image requiringSecureCoding:YES error:&error];
    XCTAssertNil(error);
    NSImage *decoded = [NSKeyedUnarchiver unarchivedObjectOfClass:[NSImage class] fromData:archive error:&error];
    XCTAssertNil(error);

    GlkGraphicsWindow *restored = [self makeWindow];
    restored.image = decoded;
    [self assertWindow:restored matches:[self expectedColorsForRects:rects count:2
                                                          background:0xFFFFFF
                                                        defaultColor:0]];
}

// Resizing keeps the old contents anchored at the top left, as Glk
// coordinates are, and paints the new area in the background.
- (void)testResizeKeepsContentsAtTopLeft {
    GlkGraphicsWindow *win = [self makeWindow];
    struct fillrect rects[] = {
        { 0xFF0000, 0, 0, 5, 5 },
        { 0x00FF00, 35, 25, 5, 5 },
    };
    [win fillRects:rects count:2];

    win.frame = NSMakeRect(0, 0, kWidth + 20, kHeight + 20);
    XCTAssertEqualObjects([NSValue valueWithSize:win.image.size],
                          [NSValue valueWithSize:NSMakeSize(kWidth + 20, kHeight + 20)]);
    [self assertWindow:win matches:[self expectedColorsForRects:rects count:2
                                                     background:0xFFFFFF
                                                   defaultColor:0]];
    XCTAssertEqual([self pixelOfWindow:win x:kWidth + 10 y:kHeight + 10], 0xFFFFFFFFu);
}

// A transparent window is resized like any other: same contents at the top
// left, and the new area stays clear rather than the canvas keeping its old
// size.
- (void)testTransparentResizeKeepsContentsAndLeavesNewAreaClear {
    GlkGraphicsWindow *win = [self makeWindow];
    [win makeTransparent];
    [win clear];
    XCTAssertEqual([self pixelOfWindow:win x:20 y:15], 0u);
    struct fillrect rects[] = { { 0x0000FF, 2, 2, 6, 6 } };
    [win fillRects:rects count:1];

    win.frame = NSMakeRect(0, 0, kWidth + 20, kHeight + 20);
    XCTAssertEqualObjects([NSValue valueWithSize:win.image.size],
                          [NSValue valueWithSize:NSMakeSize(kWidth + 20, kHeight + 20)]);
    XCTAssertEqual([self pixelOfWindow:win x:4 y:4], 0xFF0000FFu);
    XCTAssertEqual([self pixelOfWindow:win x:20 y:15], 0u);
    XCTAssertEqual([self pixelOfWindow:win x:kWidth + 10 y:kHeight + 10], 0u);
}

// Described images stay with the pixels they describe when the window is
// resized, and a later draw over them still prunes them.
- (void)testResizeMovesSubImagesWithContents {
    // No "Image" placeholder once the described one is pruned.
    self.theme.vOSpeakImages = kVOImageWithDescriptionOnly;
    GlkGraphicsWindow *win = [self makeWindow];
    NSImage *red = [self solidImageOfSize:NSMakeSize(10, 10) red:1 green:0 blue:0];
    red.accessibilityDescription = @"red square";
    [win drawImage:red val1:5 val2:5 width:10 height:10 imagerule:0 maxwidth:0 style:style_Normal];

    win.frame = NSMakeRect(0, 0, kWidth, kHeight + 20);
    NSArray<SubImage *> *images = [self subImagesOfWindow:win];
    XCTAssertEqual(images.count, 1u);
    // Glk (5, 5, 10, 10) in a 50-point-high window, bottom-left origin.
    XCTAssertTrue(NSEqualRects(images.firstObject.frameRect, NSMakeRect(5, 35, 10, 10)));

    struct fillrect cover[] = { { 0x000000, 0, 0, 20, 20 } };
    [win fillRects:cover count:1];
    XCTAssertEqual([self subImagesOfWindow:win].count, 0u);
}

// The window's label follows the described images still on show, and falls
// back to the "Image" placeholder, or nothing, when they are gone.
- (void)testAccessibilityLabelFollowsImages {
    self.theme.vOSpeakImages = kVOImageAll;
    GlkGraphicsWindow *win = [self makeWindow];
    XCTAssertNil(win.accessibilityLabel);

    NSImage *red = [self solidImageOfSize:NSMakeSize(10, 10) red:1 green:0 blue:0];
    red.accessibilityDescription = @"red square";
    [win drawImage:red val1:5 val2:5 width:10 height:10 imagerule:0 maxwidth:0 style:style_Normal];
    XCTAssertEqualObjects(win.accessibilityLabel, @"red square");

    struct fillrect cover[] = { { 0x000000, 0, 0, 20, 20 } };
    [win fillRects:cover count:1];
    XCTAssertEqualObjects(win.accessibilityLabel, NSLocalizedString(@"Image", nil));

    [win drawImage:red val1:5 val2:5 width:10 height:10 imagerule:0 maxwidth:0 style:style_Normal];
    [win clear];
    XCTAssertNil(win.accessibilityLabel);
}

// glk_image_draw_scaled_ext: WidthRatio is a fraction of the window width,
// AspectRatio a multiple of the height the width implies.
- (void)testImageRuleWidthRatioAndAspectRatio {
    GlkGraphicsWindow *win = [self makeWindow];
    NSImage *red = [self solidImageOfSize:NSMakeSize(10, 5) red:1 green:0 blue:0];
    // Half the 40-point width = 20 wide; aspect 1:2 at 1.0 = 10 high.
    [win drawImage:red val1:0 val2:0 width:0x8000 height:0x10000
         imagerule:imagerule_WidthRatio | imagerule_AspectRatio maxwidth:0 style:style_Normal];

    struct fillrect model[] = { { 0xFF0000, 0, 0, 20, 10 } };
    [self assertWindow:win matches:[self expectedColorsForRects:model count:1
                                                     background:0xFFFFFF
                                                   defaultColor:0]];
}

// Autosave archives the whole window: the described images come back with
// the window as their accessibility parent.
- (void)testWindowArchiveRestoresSubImages {
    GlkGraphicsWindow *win = [self makeWindow];
    NSImage *red = [self solidImageOfSize:NSMakeSize(10, 10) red:1 green:0 blue:0];
    red.accessibilityDescription = @"red square";
    [win drawImage:red val1:5 val2:5 width:10 height:10 imagerule:0 maxwidth:0 style:style_Normal];
    NSString *uuid = [self subImagesOfWindow:win].firstObject.uuid;
    XCTAssertNotNil(uuid);

    GlkGraphicsWindow *restored = [self archivedAndRestored:win];
    XCTAssertNotNil(restored);

    NSArray<SubImage *> *images = [self subImagesOfWindow:restored];
    XCTAssertEqual(images.count, 1u);
    XCTAssertEqualObjects(images.firstObject.uuid, uuid);
    XCTAssertEqualObjects(images.firstObject.accessibilityLabel, @"red square");
    XCTAssertEqualObjects(images.firstObject.accessibilityRole, NSAccessibilityImageRole);
    XCTAssertEqual(images.firstObject.accessibilityParent, restored);
}

// The whole window, as an autosave stores it: the canvas goes through PNG
// and comes back pixel for pixel at its point size.
- (void)testWindowArchiveKeepsCanvasPixels {
    GlkGraphicsWindow *win = [self makeWindow];
    struct fillrect rects[] = {
        { 0xFF8800, 3, 4, 17, 9 },
        { 0x0088FF, 20, 15, 15, 10 },
        { 0x123456, 0, 29, 40, 1 },
    };
    [win fillRects:rects count:3];

    GlkGraphicsWindow *restored = [self archivedAndRestored:win];
    XCTAssertTrue(NSEqualSizes(restored.image.size, NSMakeSize(kWidth, kHeight)));
    [self assertWindow:restored matches:[self expectedColorsForRects:rects count:3
                                                          background:0xFFFFFF
                                                        defaultColor:0]];
}

@end
