//
//  ZoomFactorTests.m
//  SpatterlightTests
//
//  The game window zoom (View > Zoom In/Out): a factor on the controller
//  that the windows apply to the theme's styles and to the images in buffer
//  windows, leaving the theme itself alone. It is kept on the game, and the
//  cell sizes that lay out the windows and go to the interpreter are the
//  zoomed ones.
//

#import "GlkControllerTestCase.h"
#import <ImageIO/ImageIO.h>

#import "GlkController.h"
#import "GlkController_Private.h"
#import "GlkController+GlkRequests.h"
#import "GlkWindow.h"
#import "GlkTextBufferWindow.h"
#import "BufferTextView.h"
#import "MarginContainer.h"
#import "MyAttachmentCell.h"
#import "MarginImage.h"
#import "ImageHandler.h"
#import "Theme.h"
#import "GlkStyle.h"
#import "Game.h"
#import "XCTestCase+TemporaryDirectory.h"

#include "glk.h"
#include "protocol.h"

static const int kStory = 0;
static const int kPicture = 5;
static const NSSize kPictureSize = { 20, 10 };

@interface ZoomFactorTests : GlkControllerTestCase
@end

@implementation ZoomFactorTests

- (void)setUp {
    [super setUp];
    [self makeGameWithIfid:@"ZCODE-1-261021-0000" theme:self.theme];
}

#pragma mark - Helpers

- (GlkController *)makeController {
    GlkController *ctl = [super makeController];
    ctl.imageHandler = [ImageHandler new];
    return ctl;
}

- (NSString *)writePicture {
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef bitmap = CGBitmapContextCreate(NULL, (size_t)kPictureSize.width, (size_t)kPictureSize.height, 8, 0, space,
                                                (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
    CGContextSetRGBFillColor(bitmap, 0.2, 0.2, 0.2, 1);
    CGContextFillRect(bitmap, CGRectMake(0, 0, kPictureSize.width, kPictureSize.height));
    CGImageRef image = CGBitmapContextCreateImage(bitmap);
    NSURL *url = [[self makeTemporaryDirectory] URLByAppendingPathComponent:@"picture.png"];
    CGImageDestinationRef destination = CGImageDestinationCreateWithURL((__bridge CFURLRef)url, CFSTR("public.png"), 1, NULL);
    CGImageDestinationAddImage(destination, image, NULL);
    XCTAssertTrue(CGImageDestinationFinalize(destination));
    CFRelease(destination);
    CGImageRelease(image);
    CGContextRelease(bitmap);
    CGColorSpaceRelease(space);
    return url.path;
}

- (void)print:(NSString *)text in:(GlkController *)ctl {
    [self send:PRINT to:ctl a1:kStory a2:style_Normal a3:0
          data:[text dataUsingEncoding:NSUTF16LittleEndianStringEncoding]];
}

- (void)drawPictureWithAlignment:(int)alignment in:(GlkController *)ctl {
    XCTAssertTrue([self send:FINDIMAGE to:ctl a1:kPicture a2:0 a3:0 data:nil].a1 != 0);
    struct drawrect rect = { .x = alignment, .y = 0, .style = style_Normal };
    [self send:DRAWIMAGE to:ctl a1:kStory a2:0 a3:0 data:[NSData dataWithBytes:&rect length:sizeof(rect)]];
}

// A story window with some text in it and the picture loaded
- (GlkController *)startGame {
    GlkController *ctl = [self makeController];
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextBuffer a2:kStory a3:0 data:nil].a1, kStory);
    ctl.gwindows[@(kStory)].frame = NSMakeRect(0, 0, 700, 500);

    NSString *path = [self writePicture];
    NSNumber *size = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:NULL][NSFileSize];
    [self send:LOADIMAGE to:ctl a1:kPicture a2:0 a3:size.intValue
          data:[path dataUsingEncoding:NSUTF8StringEncoding]];

    [self print:@"Zoom test.\n" in:ctl];
    return ctl;
}

- (GlkTextBufferWindow *)storyIn:(GlkController *)ctl {
    return (GlkTextBufferWindow *)ctl.gwindows[@(kStory)];
}

- (MyAttachmentCell *)imageCellIn:(GlkController *)ctl {
    NSTextStorage *text = [self storyIn:ctl].textview.textStorage;
    MyAttachmentCell __block *cell = nil;
    [text enumerateAttribute:NSAttachmentAttributeName
                     inRange:NSMakeRange(0, text.length)
                     options:0
                  usingBlock:^(NSTextAttachment *value, NSRange range, BOOL *stop) {
        if ([value.attachmentCell isKindOfClass:[MyAttachmentCell class]]) {
            cell = (MyAttachmentCell *)value.attachmentCell;
            *stop = YES;
        }
    }];
    return cell;
}

- (void)layOut:(GlkController *)ctl {
    NSTextView *textview = [self storyIn:ctl].textview;
    [textview.layoutManager ensureLayoutForTextContainer:textview.textContainer];
}

// The laid-out size of the first attachment in the story window
- (NSSize)attachmentSizeIn:(GlkController *)ctl {
    [self layOut:ctl];
    NSTextView *textview = [self storyIn:ctl].textview;
    NSTextStorage *text = textview.textStorage;
    NSUInteger __block index = NSNotFound;
    [text enumerateAttribute:NSAttachmentAttributeName
                     inRange:NSMakeRange(0, text.length)
                     options:0
                  usingBlock:^(id value, NSRange range, BOOL *stop) {
        if (value) {
            index = range.location;
            *stop = YES;
        }
    }];
    XCTAssertNotEqual(index, NSNotFound);
    NSLayoutManager *layout = textview.layoutManager;
    NSUInteger glyph = [layout glyphIndexForCharacterAtIndex:index];
    return [layout attachmentSizeForGlyphAtIndex:glyph];
}

- (CGFloat)pointSizeOfStoryTextIn:(GlkController *)ctl {
    [[self storyIn:ctl] flushDisplay];
    NSTextStorage *text = [self storyIn:ctl].textview.textStorage;
    return ((NSFont *)[text attribute:NSFontAttributeName atIndex:0 effectiveRange:NULL]).pointSize;
}

#pragma mark - Tests

// The text is restyled at the zoom, the theme is not touched, and the game
// keeps the factor
- (void)testZoomScalesTextNotTheme {
    GlkController *ctl = [self startGame];
    CGFloat themeSize = self.theme.bufferNormal.font.pointSize;
    CGFloat themeCellHeight = self.theme.cellHeight;
    XCTAssertEqualWithAccuracy([self pointSizeOfStoryTextIn:ctl], themeSize, 0.001);

    [ctl applyZoomFactor:2];

    XCTAssertEqual(ctl.zoomFactor, 2);
    XCTAssertEqual(self.game.zoomFactor, 2);
    XCTAssertEqualWithAccuracy([self pointSizeOfStoryTextIn:ctl], themeSize * 2, 0.001);
    XCTAssertEqualWithAccuracy(self.theme.bufferNormal.font.pointSize, themeSize, 0.001);
    XCTAssertEqual(self.theme.cellHeight, themeCellHeight);

    // Measured at the zoomed size, not multiplied
    NSSize zoomedCell = [GlkStyle cellSizeForAttributes:[GlkStyle attributes:self.theme.gridNormal.attributeDict scaledBy:2]];
    XCTAssertEqual(ctl.cellWidth, zoomedCell.width);
    XCTAssertEqual(ctl.cellHeight, zoomedCell.height);
    XCTAssertGreaterThan(ctl.bufferCellHeight, self.theme.bufferCellHeight);

    [ctl zoomToActualSize:nil];
    XCTAssertEqual(ctl.zoomFactor, 1);
    XCTAssertEqual(self.game.zoomFactor, 1);
    XCTAssertEqual(ctl.cellHeight, self.theme.cellHeight);
    XCTAssertEqualWithAccuracy([self pointSizeOfStoryTextIn:ctl], themeSize, 0.001);
}

// Zoom In and Zoom Out step the grid font by a point
- (void)testZoomInStepsOnePoint {
    GlkController *ctl = [self startGame];
    CGFloat gridSize = self.theme.gridNormal.font.pointSize;

    [ctl zoomIn:nil];
    [ctl zoomIn:nil];
    XCTAssertEqualWithAccuracy(gridSize * ctl.zoomFactor, gridSize + 2, 0.001);
    [ctl zoomOut:nil];
    XCTAssertEqualWithAccuracy(gridSize * ctl.zoomFactor, gridSize + 1, 0.001);
    XCTAssertEqualWithAccuracy(self.theme.gridNormal.font.pointSize, gridSize, 0.001);
}

// An inline image is laid out at its size times the zoom
- (void)testInlineImageFollowsZoom {
    GlkController *ctl = [self startGame];
    [self drawPictureWithAlignment:imagealign_InlineUp in:ctl];
    [self print:@"\nMore text.\n" in:ctl];

    NSSize size = [self attachmentSizeIn:ctl];
    XCTAssertEqualWithAccuracy(size.width, kPictureSize.width, 0.5);
    XCTAssertEqualWithAccuracy(size.height, kPictureSize.height, 0.5);

    [ctl applyZoomFactor:2];

    size = [self attachmentSizeIn:ctl];
    XCTAssertEqualWithAccuracy(size.width, kPictureSize.width * 2, 0.5);
    XCTAssertEqualWithAccuracy(size.height, kPictureSize.height * 2, 0.5);
    // The image itself is kept at its own size
    XCTAssertTrue(NSEqualSizes([self imageCellIn:ctl].image.size, kPictureSize));
}

// So is a margin image
- (void)testMarginImageFollowsZoom {
    GlkController *ctl = [self startGame];
    [self drawPictureWithAlignment:imagealign_MarginLeft in:ctl];
    [self print:@"Text flowing around the picture.\n" in:ctl];
    MarginImage *image = [self imageCellIn:ctl].marginImage;
    XCTAssertNotNil(image);

    [self layOut:ctl];
    XCTAssertTrue(NSEqualSizes(image.bounds.size, kPictureSize));

    [ctl applyZoomFactor:1.5];
    [self layOut:ctl];

    XCTAssertEqualWithAccuracy(image.bounds.size.width, kPictureSize.width * 1.5, 0.001);
    XCTAssertEqualWithAccuracy(image.bounds.size.height, kPictureSize.height * 1.5, 0.001);
    XCTAssertTrue(NSEqualSizes(image.image.size, kPictureSize));
}

// The container keeps its zoom through an autosave; one archived before
// there was a zoom comes back at 1
- (void)testContainerZoomSurvivesArchiving {
    MarginContainer *container = [[MarginContainer alloc] initWithContainerSize:NSMakeSize(100, 100)];
    XCTAssertEqual(container.zoomFactor, 1);
    container.zoomFactor = 1.25;

    NSError *error = nil;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:container requiringSecureCoding:YES error:&error];
    XCTAssertNotNil(data, @"%@", error);
    MarginContainer *restored = [NSKeyedUnarchiver unarchivedObjectOfClass:[MarginContainer class] fromData:data error:&error];
    XCTAssertNotNil(restored, @"%@", error);
    XCTAssertEqual(restored.zoomFactor, 1.25);

    container.zoomFactor = 0;
    XCTAssertEqual(container.zoomFactor, 1);
}

// A zoom of 0, as from a game in a library made before there was a zoom,
// is 1
- (void)testZeroZoomIsOne {
    GlkController *ctl = [self makeController];
    ctl.zoomFactor = 0;
    XCTAssertEqual(ctl.zoomFactor, 1);
    XCTAssertEqual(self.game.zoomFactor, 1);
}

@end
