//
//  BufferImageRescaleTests.m
//  SpatterlightTests
//
//  Pins how the images in a text buffer window are rescaled when the
//  interpreter sends REFRESH with a scale, as Bocfel does for its V6 games
//  when the player switches graphics format.
//
//  -[GlkTextBufferWindow updateImageAttachmentsWithXScale:yScale:] fetches
//  every image in the buffer from the image handler again and replaces it
//  with a copy at the new scale. It has to fetch the image by its resource
//  number. Each attachment cell also carries an index, the val2 argument of
//  the draw call, and the rescale used to look images up by that instead:
//  that only worked because Bocfel passes the picture number there.
//
//  The tests feed a GlkController the requests an interpreter sends, through
//  handleRequest:reply:buffer:, with small image files written to a temporary
//  directory standing in for the game's pictures.
//

#import "GlkControllerTestCase.h"
#import <ImageIO/ImageIO.h>

#import "GlkController.h"
#import "GlkController_Private.h"
#import "GlkController+GlkRequests.h"
#import "GlkWindow.h"
#import "GlkTextBufferWindow.h"
#import "BufferTextView.h"
#import "MyAttachmentCell.h"
#import "MarginImage.h"
#import "ImageHandler.h"
#import "Theme.h"
#import "Game.h"
#import "BuiltInThemes.h"
#import "XCTestCase+TemporaryDirectory.h"
#import "NSBitmapImageRep+TestImages.h"

#include "glk.h"
#include "protocol.h"

static const int kStory = 0;

// The pictures. kDecoy is never drawn: it is there to be found by a lookup
// that goes by draw index instead of resource number.
static const int kPicture = 5;
static const int kAnimated = 6;
static const int kDecoy = 9;

static const NSSize kPictureSize = { 20, 10 };
static const NSSize kAnimatedSize = { 16, 12 };
static const NSSize kDecoySize = { 30, 30 };

@interface BufferImageRescaleTests : GlkControllerTestCase

@property (nonatomic, strong) NSURL *imageDirectory;

@end

@implementation BufferImageRescaleTests

- (void)setUp {
    [super setUp];
    [self makeGameWithIfid:@"ZCODE-1-261008-0000" theme:self.theme];

    self.imageDirectory = [self makeTemporaryDirectory];
}

#pragma mark - A controller without an interpreter

// A controller without an interpreter, with an image handler to load the
// pictures into.
- (GlkController *)makeController {
    GlkController *ctl = [super makeController];
    ctl.imageHandler = [ImageHandler new];
    return ctl;
}

#pragma mark - Pictures

- (CGImageRef)newImageOfSize:(NSSize)size gray:(CGFloat)gray CF_RETURNS_RETAINED {
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef bitmap = CGBitmapContextCreate(NULL, (size_t)size.width, (size_t)size.height, 8, 0, space,
                                                (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
    CGContextSetRGBFillColor(bitmap, gray, gray, gray, 1);
    CGContextFillRect(bitmap, CGRectMake(0, 0, size.width, size.height));
    CGImageRef image = CGBitmapContextCreateImage(bitmap);
    CGContextRelease(bitmap);
    CGColorSpaceRelease(space);
    return image;
}

// Writes an image file with one frame per entry in grays, and returns its
// path. More than one frame makes an animated GIF that loops forever.
- (NSString *)writeImageNamed:(NSString *)name size:(NSSize)size grays:(NSArray<NSNumber *> *)grays {
    BOOL animated = grays.count > 1;
    NSURL *url = [self.imageDirectory URLByAppendingPathComponent:name];
    CGImageDestinationRef destination =
    CGImageDestinationCreateWithURL((__bridge CFURLRef)url,
                                    animated ? CFSTR("com.compuserve.gif") : CFSTR("public.png"),
                                    grays.count, NULL);
    XCTAssertTrue(destination != NULL);
    if (animated) {
        NSDictionary *properties = @{ (__bridge NSString *)kCGImagePropertyGIFDictionary :
                                          @{ (__bridge NSString *)kCGImagePropertyGIFLoopCount : @0 } };
        CGImageDestinationSetProperties(destination, (__bridge CFDictionaryRef)properties);
    }
    for (NSNumber *gray in grays) {
        CGImageRef frame = [self newImageOfSize:size gray:gray.doubleValue];
        NSDictionary *properties = animated ?
        @{ (__bridge NSString *)kCGImagePropertyGIFDictionary :
               @{ (__bridge NSString *)kCGImagePropertyGIFDelayTime : @0.5 } } : nil;
        CGImageDestinationAddImage(destination, frame, (__bridge CFDictionaryRef)properties);
        CGImageRelease(frame);
    }
    XCTAssertTrue(CGImageDestinationFinalize(destination));
    CFRelease(destination);
    return url.path;
}

#pragma mark - Requests, as the interpreter sends them

- (void)print:(NSString *)text in:(GlkController *)ctl {
    [self send:PRINT to:ctl a1:kStory a2:style_Normal a3:0
          data:[text dataUsingEncoding:NSUTF16LittleEndianStringEncoding]];
}

- (void)loadImage:(int)resno path:(NSString *)path in:(GlkController *)ctl {
    NSNumber *size = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:NULL][NSFileSize];
    XCTAssertGreaterThan(size.intValue, 0);
    [self send:LOADIMAGE to:ctl a1:resno a2:0 a3:size.intValue
          data:[path dataUsingEncoding:NSUTF8StringEncoding]];
}

- (BOOL)findImage:(int)resno in:(GlkController *)ctl {
    return [self send:FINDIMAGE to:ctl a1:resno a2:0 a3:0 data:nil].a1 != 0;
}

// The size of the image the interpreter found last, which is the one a
// draw request would draw.
- (NSSize)sizeOfLastImageIn:(GlkController *)ctl {
    struct message reply = [self send:SIZEIMAGE to:ctl a1:0 a2:0 a3:0 data:nil];
    return NSMakeSize(reply.a1, reply.a2);
}

// glk_image_draw(): find the image, then draw it at its own size. val2 is
// passed along untouched and means nothing in a buffer window.
- (void)drawImage:(int)resno alignment:(int)alignment val2:(int)val2 in:(GlkController *)ctl {
    XCTAssertTrue([self findImage:resno in:ctl]);
    struct drawrect rect = { .x = alignment, .y = val2, .style = style_Normal };
    [self send:DRAWIMAGE to:ctl a1:kStory a2:0 a3:0 data:[NSData dataWithBytes:&rect length:sizeof(rect)]];
}

// What Bocfel sends when the graphics format changes.
- (void)refreshWithScale:(CGFloat)scale in:(GlkController *)ctl {
    [self send:REFRESH to:ctl a1:kStory a2:(int)(scale * 1000) a3:(int)(scale * 1000) data:nil];
}

#pragma mark - A session of the game

// A story window with some text in it, and the three pictures loaded.
- (GlkController *)startGame {
    GlkController *ctl = [self makeController];
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextBuffer a2:kStory a3:0 data:nil].a1, kStory);
    ctl.gwindows[@(kStory)].frame = NSMakeRect(0, 0, 700, 500);

    [self loadImage:kPicture path:[self writeImageNamed:@"picture.png" size:kPictureSize grays:@[@0.2]] in:ctl];
    [self loadImage:kAnimated path:[self writeImageNamed:@"animated.gif" size:kAnimatedSize grays:@[@0.2, @0.8]] in:ctl];
    [self loadImage:kDecoy path:[self writeImageNamed:@"decoy.png" size:kDecoySize grays:@[@0.5]] in:ctl];

    [self print:@"Image rescale test.\n" in:ctl];
    return ctl;
}

- (NSTextStorage *)storyTextIn:(GlkController *)ctl {
    return ((GlkTextBufferWindow *)ctl.gwindows[@(kStory)]).textview.textStorage;
}

// The attachment cells in the story window, in buffer order.
- (NSArray<MyAttachmentCell *> *)imageCellsIn:(GlkController *)ctl {
    NSMutableArray<MyAttachmentCell *> *cells = [NSMutableArray array];
    NSTextStorage *text = [self storyTextIn:ctl];
    [text enumerateAttribute:NSAttachmentAttributeName
                     inRange:NSMakeRange(0, text.length)
                     options:0
                  usingBlock:^(NSTextAttachment *value, NSRange range, BOOL *stop) {
        if ([value.attachmentCell isKindOfClass:[MyAttachmentCell class]])
            [cells addObject:(MyAttachmentCell *)value.attachmentCell];
    }];
    return cells;
}

- (void)assertSize:(NSSize)size equals:(NSSize)expected scaledBy:(CGFloat)scale message:(NSString *)message {
    XCTAssertEqualWithAccuracy(size.width, expected.width * scale, 0.5, @"%@", message);
    XCTAssertEqualWithAccuracy(size.height, expected.height * scale, 0.5, @"%@", message);
}

#pragma mark - Tests

// An ordinary glk_image_draw() leaves val2 at zero, so the cell has nothing
// but the resource number to find its image by.
- (void)testInlineImageIsRescaled {
    GlkController *ctl = [self startGame];
    [self drawImage:kPicture alignment:imagealign_InlineUp val2:0 in:ctl];
    [self print:@"\nMore text.\n" in:ctl];

    MyAttachmentCell *cell = [self imageCellsIn:ctl].firstObject;
    XCTAssertNotNil(cell);
    XCTAssertEqual(cell.imageNumber, kPicture);
    [self assertSize:cell.image.size equals:kPictureSize scaledBy:1 message:@"as drawn"];

    [self refreshWithScale:2 in:ctl];

    NSArray<MyAttachmentCell *> *cells = [self imageCellsIn:ctl];
    XCTAssertEqual(cells.count, 1UL);
    XCTAssertEqual(cells.firstObject.imageNumber, kPicture, @"the new cell must still know its resource number");
    [self assertSize:cells.firstObject.image.size equals:kPictureSize scaledBy:2 message:@"after REFRESH at 2x"];

    // And again, from the original rather than from the scaled copy.
    [self refreshWithScale:3 in:ctl];
    [self assertSize:[self imageCellsIn:ctl].firstObject.image.size equals:kPictureSize scaledBy:3
             message:@"after REFRESH at 3x"];
}

// A draw index that happens to be the number of another picture must not
// make the rescale swap that picture in.
- (void)testRescaleGoesByResourceNumberNotDrawIndex {
    GlkController *ctl = [self startGame];
    [self drawImage:kPicture alignment:imagealign_InlineUp val2:kDecoy in:ctl];
    [self print:@"\n" in:ctl];

    MyAttachmentCell *cell = [self imageCellsIn:ctl].firstObject;
    XCTAssertEqual(cell.index, kDecoy);
    XCTAssertEqual(cell.imageNumber, kPicture);

    [self refreshWithScale:2 in:ctl];

    [self assertSize:[self imageCellsIn:ctl].firstObject.image.size equals:kPictureSize scaledBy:2
             message:@"the picture that was drawn, not the one numbered like the draw index"];
}

// Margin images are replaced in the container's list; their cell stays.
- (void)testMarginImageIsRescaled {
    GlkController *ctl = [self startGame];
    [self drawImage:kPicture alignment:imagealign_MarginLeft val2:kDecoy in:ctl];
    [self print:@"Text flowing around the picture.\n" in:ctl];

    MyAttachmentCell *cell = [self imageCellsIn:ctl].firstObject;
    XCTAssertNotNil(cell.marginImage);
    XCTAssertEqual(cell.imageNumber, kPicture);
    [self assertSize:cell.marginImage.image.size equals:kPictureSize scaledBy:1 message:@"as drawn"];

    [self refreshWithScale:2 in:ctl];

    NSArray<MyAttachmentCell *> *cells = [self imageCellsIn:ctl];
    XCTAssertEqual(cells.count, 1UL);
    XCTAssertNotNil(cells.firstObject.marginImage);
    XCTAssertEqualObjects(cells.firstObject.marginImgUUID, cells.firstObject.marginImage.uuid);
    [self assertSize:cells.firstObject.marginImage.image.size equals:kPictureSize scaledBy:2
             message:@"after REFRESH at 2x"];
}

// A cell from an autosave made before cells stored their resource number
// has only its index to go by. That is right for Bocfel, which passes the
// picture number as val2.
- (void)testCellWithoutResourceNumberFallsBackOnIndex {
    GlkController *ctl = [self startGame];
    [self drawImage:kPicture alignment:imagealign_InlineUp val2:kPicture in:ctl];
    [self drawImage:kPicture alignment:imagealign_InlineUp val2:1000 in:ctl];
    [self print:@"\n" in:ctl];

    NSArray<MyAttachmentCell *> *cells = [self imageCellsIn:ctl];
    XCTAssertEqual(cells.count, 2UL);
    for (MyAttachmentCell *cell in cells)
        cell.imageNumber = -1;

    [self refreshWithScale:2 in:ctl];

    cells = [self imageCellsIn:ctl];
    XCTAssertEqual(cells.count, 2UL);
    [self assertSize:cells[0].image.size equals:kPictureSize scaledBy:2 message:@"found by its index"];
    // No picture 1000: the cell is left as it was.
    [self assertSize:cells[1].image.size equals:kPictureSize scaledBy:1 message:@"nothing to find"];
}

// The resource number is archived with the cell.
- (void)testResourceNumberSurvivesArchiving {
    GlkController *ctl = [self startGame];
    [self drawImage:kPicture alignment:imagealign_InlineUp val2:kDecoy in:ctl];

    NSError *error = nil;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:[self imageCellsIn:ctl].firstObject
                                         requiringSecureCoding:NO
                                                         error:&error];
    XCTAssertNil(error);
    NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
    XCTAssertNil(error);
    unarchiver.requiresSecureCoding = NO;
    MyAttachmentCell *copy = [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
    [unarchiver finishDecoding];

    XCTAssertTrue([copy isKindOfClass:[MyAttachmentCell class]]);
    XCTAssertEqual(copy.imageNumber, kPicture);
    XCTAssertEqual(copy.index, kDecoy);
}

// Rescaling fetches every picture in the buffer from the image handler,
// which makes each of them the "last image" in turn. The one the
// interpreter found last has to be back in place afterwards, because a draw
// request draws whatever the last image is.
- (void)testRescaleLeavesLastFoundImageAlone {
    GlkController *ctl = [self startGame];
    [self drawImage:kPicture alignment:imagealign_InlineUp val2:0 in:ctl];
    [self print:@"\n" in:ctl];

    XCTAssertTrue([self findImage:kDecoy in:ctl]);
    NSImage *found = ctl.imageHandler.lastimage;
    XCTAssertNotNil(found);

    [self refreshWithScale:2 in:ctl];

    // The refresh did fetch the picture in the buffer...
    [self assertSize:[self imageCellsIn:ctl].firstObject.image.size equals:kPictureSize scaledBy:2
             message:@"after REFRESH at 2x"];
    // ...and put the interpreter's image back.
    XCTAssertEqual(ctl.imageHandler.lastimageresno, kDecoy);
    XCTAssertEqual(ctl.imageHandler.lastimage, found);
    [self assertSize:[self sizeOfLastImageIn:ctl] equals:kDecoySize scaledBy:1 message:@"SIZEIMAGE"];

    // A draw without a new find, then, draws the decoy.
    struct drawrect rect = { .x = imagealign_InlineUp, .style = style_Normal };
    [self send:DRAWIMAGE to:ctl a1:kStory a2:0 a3:0 data:[NSData dataWithBytes:&rect length:sizeof(rect)]];
    MyAttachmentCell *drawn = [self imageCellsIn:ctl].lastObject;
    XCTAssertEqual(drawn.imageNumber, kDecoy);
    [self assertSize:drawn.image.size equals:kDecoySize scaledBy:1 message:@"drawn after the refresh"];
}

// An inline image gets a new cell when it is rescaled. An animated one
// must carry on from where it was instead of starting over or stopping.
- (void)testRescaleKeepsAnimationPlaying {
    GlkController *ctl = [self startGame];
    [self drawImage:kAnimated alignment:imagealign_InlineUp val2:0 in:ctl];
    [self print:@"\n" in:ctl];

    MyAttachmentCell *cell = [self imageCellsIn:ctl].firstObject;
    ImageAnimation *animation = cell.animation;
    XCTAssertNotNil(animation, @"a two frame GIF drawn inline should be animated");
    XCTAssertEqual(animation.frameCount, 2UL);
    XCTAssertEqual(animation, [ctl.imageHandler animationForImageNumber:kAnimated]);
    XCTAssertNil([ctl.imageHandler animationForImageNumber:kPicture], @"a PNG is not an animation");

    // As the animation timer leaves a cell that is part way through.
    cell.animationStart = 1234.5;
    cell.animationFrame = 1;

    [self refreshWithScale:2 in:ctl];

    MyAttachmentCell *rescaled = [self imageCellsIn:ctl].firstObject;
    XCTAssertNotEqual(rescaled, cell, @"inline images are given a new cell");
    [self assertSize:rescaled.image.size equals:kAnimatedSize scaledBy:2 message:@"after REFRESH at 2x"];
    XCTAssertEqual(rescaled.imageNumber, kAnimated);
    XCTAssertEqual(rescaled.animation, animation);
    XCTAssertTrue(rescaled.animationResolved);
    XCTAssertEqual(rescaled.animationStart, 1234.5);
    XCTAssertEqual(rescaled.animationFrame, 1UL);
}

// The gray level at the middle of image, drawn the way a cell draws it.
- (CGFloat)grayDrawnBy:(NSImage *)image {
    NSBitmapImageRep *rep = [NSBitmapImageRep blankRGBAWithWidth:(NSInteger)kAnimatedSize.width
                                                              height:(NSInteger)kAnimatedSize.height];
    [NSGraphicsContext saveGraphicsState];
    NSGraphicsContext.currentContext = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    [image drawInRect:NSMakeRect(0, 0, kAnimatedSize.width, kAnimatedSize.height)
             fromRect:NSZeroRect
            operation:NSCompositingOperationCopy
             fraction:1.0
       respectFlipped:YES
                hints:nil];
    [NSGraphicsContext restoreGraphicsState];
    NSColor *color = [rep colorAtX:(NSInteger)kAnimatedSize.width / 2 y:(NSInteger)kAnimatedSize.height / 2];
    return color.redComponent;
}

// The animation is the cached image itself, with its bitmap rep switched
// between frames. Switching must change what the image draws, or every cell
// would show the first frame for ever.
- (void)testAnimationIsTheCachedImage {
    GlkController *ctl = [self startGame];
    XCTAssertTrue([self findImage:kAnimated in:ctl]);
    NSImage *cached = ctl.imageHandler.lastimage;

    ImageAnimation *animation = [ctl.imageHandler animationForImageNumber:kAnimated];
    XCTAssertNotNil(animation);
    XCTAssertEqual(animation.image, cached, @"no second copy of the picture");
    XCTAssertEqual(animation.frameCount, 2UL);
    XCTAssertEqual(animation.loopCount, 0UL, @"the test GIF loops for ever");
    XCTAssertEqualWithAccuracy(animation.duration, 1.0, 0.01, @"two frames of half a second");

    XCTAssertEqualWithAccuracy([self grayDrawnBy:[animation imageForFrame:0]], 0.2, 0.02);
    XCTAssertEqualWithAccuracy([self grayDrawnBy:[animation imageForFrame:1]], 0.8, 0.02);
    XCTAssertEqualWithAccuracy([self grayDrawnBy:[animation imageForFrame:0]], 0.2, 0.02, @"and back");
    XCTAssertNil([animation imageForFrame:2]);

    // Asking again, or after the handler has moved on to another image,
    // gives the same animation.
    XCTAssertTrue([self findImage:kPicture in:ctl]);
    XCTAssertEqual([ctl.imageHandler animationForImageNumber:kAnimated], animation);
    XCTAssertEqual(ctl.imageHandler.lastimageresno, kPicture, @"asking does not change the last image");
}

@end
