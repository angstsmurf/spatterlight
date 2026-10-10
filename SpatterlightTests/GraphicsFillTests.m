//
//  GraphicsFillTests.m
//  SpatterlightTests
//
//  GlkGraphicsWindow draws FILLRECT batches straight into its bitmap canvas.
//  These tests check the result pixel by pixel against the Glk model of
//  glk_window_fill_rect: opaque fills applied in order, later ones on top,
//  clipped to the window, in top-left-origin window coordinates.
//

#import <XCTest/XCTest.h>
#import <CoreData/CoreData.h>

#import "GlkController.h"
#import "GlkGraphicsWindow.h"
#import "Theme.h"
#import "BuiltInThemes.h"
#import "CoreDataManager.h"
#import "NSColor+integer.h"

#include "glk.h"
#include "protocol.h"

static const NSInteger kWidth = 40;
static const NSInteger kHeight = 30;

@interface GraphicsFillTests : XCTestCase

@property (nonatomic, strong) CoreDataManager *coreDataManager;
@property (nonatomic, strong) NSManagedObjectContext *context;
@property (nonatomic, strong) Theme *theme;
// Windows hold their controller weakly, so the test keeps it alive.
@property (nonatomic, strong) GlkController *controller;

@end

@implementation GraphicsFillTests

- (void)setUp {
    [super setUp];

    // In-memory store, so the real library's Default theme is never touched.
    self.coreDataManager = [[CoreDataManager alloc] initInMemoryWithModelName:@"Spatterlight"];
    self.context = self.coreDataManager.mainManagedObjectContext;
    XCTAssertNotNil(self.context);

    self.theme = [BuiltInThemes createDefaultThemeInContext:self.context forceRebuild:YES];
    XCTAssertNotNil(self.theme);
}

- (void)tearDown {
    self.controller = nil;
    self.theme = nil;
    self.context = nil;
    self.coreDataManager = nil;
    [super tearDown];
}

#pragma mark - Helpers

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

// Read the window's image back as 0x00RRGGBB values, one per point, sampling
// the centre of each point's device pixels. Row 0 is the top, as in Glk.
- (NSArray<NSNumber *> *)pointColorsOfWindow:(GlkGraphicsWindow *)win {
    NSImage *image = win.image;
    CGImageRef cgImage = [image CGImageForProposedRect:NULL context:nil hints:nil];
    XCTAssert(cgImage != NULL);
    size_t pixelsWide = CGImageGetWidth(cgImage);
    size_t pixelsHigh = CGImageGetHeight(cgImage);
    CGFloat scale = (CGFloat)pixelsWide / image.size.width;

    // Copy into a context of the canvas's own format, so no conversion happens.
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef readback = CGBitmapContextCreate(NULL, pixelsWide, pixelsHigh, 8, 0, space,
                                                  kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(space);
    CGContextDrawImage(readback, CGRectMake(0, 0, pixelsWide, pixelsHigh), cgImage);
    const uint32_t *pixels = CGBitmapContextGetData(readback);
    size_t rowPixels = CGBitmapContextGetBytesPerRow(readback) / sizeof(uint32_t);

    NSMutableArray<NSNumber *> *colors = [NSMutableArray arrayWithCapacity:kWidth * kHeight];
    for (NSInteger y = 0; y < kHeight; y++) {
        for (NSInteger x = 0; x < kWidth; x++) {
            size_t px = (size_t)(((CGFloat)x + 0.5) * scale);
            size_t py = (size_t)(((CGFloat)y + 0.5) * scale);
            [colors addObject:@(pixels[py * rowPixels + px] & 0xFFFFFF)];
        }
    }
    CGContextRelease(readback);
    return colors;
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

    NSImage *red = [NSImage imageWithSize:NSMakeSize(10, 10) flipped:NO drawingHandler:^BOOL(NSRect dstRect) {
        [[NSColor colorWithDeviceRed:1 green:0 blue:0 alpha:1] setFill];
        NSRectFill(dstRect);
        return YES;
    }];
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

@end
