//
//  BufferBottomPaddingTests.m
//  SpatterlightTests
//
//  Pins the bottom padding of a text buffer window: the extra height that
//  -[BufferTextView setFrameSize:] adds to the frame when a margin image
//  reaches further down than the text, so that the image can be scrolled
//  into view. And how the scroll code treats that padding.
//
//  The padding is worked out from the margin image bounds, which are only
//  current once layout is done, so it is settled when layout completes
//  (layoutManager:didCompleteLayoutForTextContainer:atEnd:). It must go away
//  once the text reaches past the image, and must not pile up when something
//  sets the frame to its own size.
//
//  Scrolling to the bottom goes to the end of the padded frame, unless that
//  would push the last line of text, where the input caret is, off the top
//  of the window: then it stops with the last line at the top. A window
//  scrolled up from there, cutting the image off, or partway into the bottom
//  margin, is not "at the bottom", and a scroll restore brings it back where
//  it was rather than all the way down.
//
//  The tests feed a GlkController the requests an interpreter sends, through
//  handleRequest:reply:buffer:, with a tall image file written to a temporary
//  directory standing in for the game's picture.
//

#import "GlkControllerTestCase.h"
#import <ImageIO/ImageIO.h>

#import "GlkController.h"
#import "GlkController+GlkRequests.h"
#import "GlkWindow.h"
#import "GlkTextBufferWindow.h"
#import "GlkTextBufferWindow+Scrolling.h"
#import "BufferTextView.h"
#import "MarginContainer.h"
#import "MarginImage.h"
#import "ImageHandler.h"
#import "Theme.h"
#import "XCTestCase+TemporaryDirectory.h"

#include "glk.h"
#include "protocol.h"

static const int kStory = 0;
static const int kPicture = 5;

// The story window, smaller than the controller window it sits in.
static const NSRect kStoryFrame = { { 0, 0 }, { 700, 500 } };

@interface BufferBottomPaddingTests : GlkControllerTestCase

@property (nonatomic, strong) NSURL *imageDirectory;

@end

@implementation BufferBottomPaddingTests

- (void)setUp {
    [super setUp];
    self.theme.smoothScroll = NO;
    [self makeGameWithIfid:@"ZCODE-1-261020-0000" theme:self.theme];

    self.imageDirectory = [self makeTemporaryDirectory];
}

#pragma mark - A controller without an interpreter

- (GlkController *)makeController {
    GlkController *ctl = [super makeController];
    ctl.imageHandler = [ImageHandler new];
    return ctl;
}

// A grey PNG of the given size. Returns its path.
- (NSString *)writeImageOfSize:(NSSize)size {
    NSURL *url = [self.imageDirectory URLByAppendingPathComponent:@"picture.png"];
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef bitmap = CGBitmapContextCreate(NULL, (size_t)size.width, (size_t)size.height, 8, 0, space,
                                                (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
    CGContextSetRGBFillColor(bitmap, 0.5, 0.5, 0.5, 1);
    CGContextFillRect(bitmap, CGRectMake(0, 0, size.width, size.height));
    CGImageRef image = CGBitmapContextCreateImage(bitmap);
    CGImageDestinationRef destination =
    CGImageDestinationCreateWithURL((__bridge CFURLRef)url, CFSTR("public.png"), 1, NULL);
    CGImageDestinationAddImage(destination, image, NULL);
    XCTAssertTrue(CGImageDestinationFinalize(destination));
    CFRelease(destination);
    CGImageRelease(image);
    CGContextRelease(bitmap);
    CGColorSpaceRelease(space);
    return url.path;
}

#pragma mark - Requests, as the interpreter sends them

- (void)print:(NSString *)text in:(GlkController *)ctl {
    [self send:PRINT to:ctl a1:kStory a2:style_Normal a3:0 text:text];
}

- (void)loadPictureOfSize:(NSSize)size in:(GlkController *)ctl {
    NSString *path = [self writeImageOfSize:size];
    NSNumber *length = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:NULL][NSFileSize];
    [self send:LOADIMAGE to:ctl a1:kPicture a2:0 a3:length.intValue
          data:[path dataUsingEncoding:NSUTF8StringEncoding]];
}

- (void)drawPictureIn:(GlkController *)ctl {
    XCTAssertNotEqual([self send:FINDIMAGE to:ctl a1:kPicture a2:0 a3:0], 0);
    struct drawrect rect = { .x = imagealign_MarginLeft, .y = 0, .style = style_Normal };
    [self send:DRAWIMAGE to:ctl a1:kStory a2:0 a3:0 data:[NSData dataWithBytes:&rect length:sizeof(rect)]];
}

#pragma mark - A session of the game

// A story window with a line of text, a picture of the given size in the
// left margin, and the given number of lines of text flowing beside it.
- (GlkController *)startGameWithPicture:(NSSize)size lines:(NSUInteger)lines {
    GlkController *ctl = [self makeController];
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextBuffer a2:kStory a3:0], kStory);
    ctl.gwindows[@(kStory)].frame = kStoryFrame;

    [self loadPictureOfSize:size in:ctl];
    [self print:@"Bottom padding test.\n" in:ctl];
    [self drawPictureIn:ctl];
    [self printLines:lines in:ctl];
    [self settle:ctl];
    return ctl;
}

- (void)printLines:(NSUInteger)lines in:(GlkController *)ctl {
    NSMutableString *text = [NSMutableString string];
    for (NSUInteger i = 1; i <= lines; i++)
        [text appendFormat:@"Line %lu.\n", i];
    [self print:text in:ctl];
}

- (GlkTextBufferWindow *)storyIn:(GlkController *)ctl {
    return (GlkTextBufferWindow *)ctl.gwindows[@(kStory)];
}

- (void)runFor:(NSTimeInterval)seconds {
    NSDate *until = [NSDate dateWithTimeIntervalSinceNow:seconds];
    while (until.timeIntervalSinceNow > 0)
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:until];
}

// Applies a pending frame, lays out all the text, which settles the
// padding, and lets anything that was dispatched to the main queue run.
- (void)settle:(GlkController *)ctl {
    GlkTextBufferWindow *story = [self storyIn:ctl];
    [story flushDisplay];
    BufferTextView *textview = story.textview;
    [textview.layoutManager ensureLayoutForTextContainer:textview.textContainer];
    [self runFor:0.3];
}

#pragma mark - Measurements

- (NSClipView *)clipViewIn:(GlkController *)ctl {
    return [self storyIn:ctl].textview.enclosingScrollView.contentView;
}

- (MarginImage *)pictureIn:(GlkController *)ctl {
    MarginContainer *container = (MarginContainer *)[self storyIn:ctl].textview.textContainer;
    XCTAssertTrue([container isKindOfClass:[MarginContainer class]]);
    XCTAssertEqual(container.marginImages.count, 1UL);
    return container.marginImages.firstObject;
}

// Where the frame must end for the picture to fit: the bottom of the picture
// in the view, plus the bottom margin.
- (CGFloat)pictureEndIn:(GlkController *)ctl {
    BufferTextView *textview = [self storyIn:ctl].textview;
    return NSMaxY([self pictureIn:ctl].bounds) + textview.textContainerOrigin.y
        + textview.textContainerInset.height;
}

// Where the frame must end for the text to fit.
- (CGFloat)textEndIn:(GlkController *)ctl {
    BufferTextView *textview = [self storyIn:ctl].textview;
    NSRect used = [textview.layoutManager usedRectForTextContainer:textview.textContainer];
    return NSMaxY(used) + textview.textContainerOrigin.y + textview.textContainerInset.height;
}

// The top of the last line of text in the view, which is where the scroll
// code reckons the input caret is.
- (CGFloat)lastLineTopIn:(GlkController *)ctl {
    BufferTextView *textview = [self storyIn:ctl].textview;
    return NSHeight(textview.frame) - textview.bottomPadding
        - textview.textContainerInset.height - self.theme.bufferCellHeight;
}

- (void)scrollTo:(CGFloat)y in:(GlkController *)ctl {
    NSClipView *clipView = [self clipViewIn:ctl];
    [clipView scrollToPoint:NSMakePoint(0, y)];
    [clipView.enclosingScrollView reflectScrolledClipView:clipView];
    XCTAssertEqualWithAccuracy(NSMinY(clipView.bounds), y, 0.5, @"the clip view must take the position");
}

- (void)scrollToBottomIn:(GlkController *)ctl {
    [[self storyIn:ctl] scrollToBottomAnimated:NO];
    [self runFor:0.3];
}

#pragma mark - The padding

// A picture that hangs below the text pads the frame down to its bottom.
- (void)testLowMarginImagePadsTheFrame {
    GlkController *ctl = [self startGameWithPicture:NSMakeSize(100, 800) lines:2];
    BufferTextView *textview = [self storyIn:ctl].textview;

    XCTAssertGreaterThan(NSHeight([self pictureIn:ctl].bounds), 0, @"the picture must be laid out");
    XCTAssertGreaterThan([self pictureEndIn:ctl], [self textEndIn:ctl], @"the picture must hang below the text");
    XCTAssertGreaterThan(textview.bottomPadding, 0);
    XCTAssertEqualWithAccuracy(NSHeight(textview.frame), [self pictureEndIn:ctl], 0.5,
                               @"the frame must end below the picture");
}

// Once the text reaches below the picture, the padding goes, and leaves no
// extra space behind.
- (void)testPaddingGoesWhenTheTextPassesTheImage {
    GlkController *ctl = [self startGameWithPicture:NSMakeSize(100, 800) lines:2];
    BufferTextView *textview = [self storyIn:ctl].textview;
    XCTAssertGreaterThan(textview.bottomPadding, 0);

    [self printLines:60 in:ctl];
    [self settle:ctl];

    XCTAssertGreaterThan([self textEndIn:ctl], [self pictureEndIn:ctl], @"the text must reach below the picture");
    XCTAssertEqual(textview.bottomPadding, 0);
    XCTAssertEqualWithAccuracy(NSHeight(textview.frame), [self textEndIn:ctl], 0.5,
                               @"no space may be left below the text");
}

// Setting the frame to its own size, as autoresizing does, keeps the
// padding as it is rather than adding it again.
- (void)testSettingTheSameFrameKeepsThePadding {
    GlkController *ctl = [self startGameWithPicture:NSMakeSize(100, 800) lines:2];
    BufferTextView *textview = [self storyIn:ctl].textview;
    CGFloat padding = textview.bottomPadding;
    NSSize size = textview.frame.size;
    XCTAssertGreaterThan(padding, 0);

    [textview setFrameSize:size];
    [textview setFrameSize:textview.frame.size];
    textview.frame = textview.frame;
    [textview updateBottomPadding];
    [self runFor:0.1];

    XCTAssertEqualWithAccuracy(textview.bottomPadding, padding, 0.5);
    XCTAssertEqualWithAccuracy(NSHeight(textview.frame), size.height, 0.5);
}

// A narrower window rewraps the text beside the picture into more lines,
// which takes up some of the padding. It must still end below the picture.
- (void)testPaddingFollowsARewrap {
    // More text than fits in the window: while the text is shorter than
    // that, the frame is the height of the window and the padding measures
    // from there.
    GlkController *ctl = [self startGameWithPicture:NSMakeSize(100, 1200) lines:30];
    BufferTextView *textview = [self storyIn:ctl].textview;
    for (int i = 0; i < 3; i++)
        [self print:@"A long line of text that will wrap once the window gets narrow enough for it to.\n" in:ctl];
    [self settle:ctl];
    CGFloat padding = textview.bottomPadding;
    XCTAssertGreaterThan([self textEndIn:ctl], NSHeight([self clipViewIn:ctl].bounds));
    XCTAssertGreaterThan(padding, 0);
    XCTAssertEqualWithAccuracy(NSHeight(textview.frame), [self pictureEndIn:ctl], 0.5);

    ctl.gwindows[@(kStory)].frame = NSMakeRect(0, 0, 250, 500);
    [self settle:ctl];

    XCTAssertLessThan(textview.bottomPadding, padding, @"the rewrapped text must take up some of the padding");
    CGFloat expected = MAX([self pictureEndIn:ctl], [self textEndIn:ctl]);
    XCTAssertEqualWithAccuracy(NSHeight(textview.frame), expected, 0.5);

    ctl.gwindows[@(kStory)].frame = kStoryFrame;
    [self settle:ctl];
    XCTAssertEqualWithAccuracy(textview.bottomPadding, padding, 0.5);
    XCTAssertEqualWithAccuracy(NSHeight(textview.frame), [self pictureEndIn:ctl], 0.5,
                               @"back at full width, the frame must end below the picture again");
}

#pragma mark - Scrolling to the bottom

// When the bottom of the picture fits in the window together with the last
// line of text, scrolling to the bottom shows the whole of the picture.
- (void)testScrollToBottomShowsTheImageWhenTheCaretStaysVisible {
    // Enough text to overflow the window, and a picture reaching a little
    // below it.
    GlkController *ctl = [self startGameWithPicture:NSMakeSize(100, 900) lines:40];
    BufferTextView *textview = [self storyIn:ctl].textview;
    NSClipView *clipView = [self clipViewIn:ctl];
    XCTAssertGreaterThan([self textEndIn:ctl], NSHeight(clipView.bounds), @"the text must overflow the window");
    XCTAssertGreaterThan(textview.bottomPadding, 0);
    XCTAssertLessThan(NSHeight(textview.frame) - NSHeight(clipView.bounds), [self lastLineTopIn:ctl],
                      @"the end of the picture must fit in the window together with the last line");

    [self scrollTo:0 in:ctl];
    [self scrollToBottomIn:ctl];

    XCTAssertEqualWithAccuracy(NSMaxY(clipView.bounds), NSHeight(textview.frame), 0.5,
                               @"the window must scroll to the end of the padded frame");
    XCTAssertTrue([self storyIn:ctl].atBottomScrollTarget);
}

// When the picture reaches so far below the text that showing all of it
// would push the last line out of the top of the window, scrolling to the
// bottom stops with the last line at the top.
- (void)testScrollToBottomKeepsTheLastLineInView {
    GlkController *ctl = [self startGameWithPicture:NSMakeSize(100, 2000) lines:40];
    BufferTextView *textview = [self storyIn:ctl].textview;
    NSClipView *clipView = [self clipViewIn:ctl];
    XCTAssertGreaterThan(textview.bottomPadding, NSHeight(clipView.bounds));

    [self scrollTo:0 in:ctl];
    [self scrollToBottomIn:ctl];

    CGFloat lastLineTop = [self lastLineTopIn:ctl];
    XCTAssertEqualWithAccuracy(NSMinY(clipView.bounds), lastLineTop, 0.5,
                               @"the window must stop with the last line at the top");
    XCTAssertLessThanOrEqual(lastLineTop + self.theme.bufferCellHeight, NSMaxY(clipView.bounds),
                             @"the last line of text must be in view");
    XCTAssertLessThan(NSMaxY(clipView.bounds), NSHeight(textview.frame),
                      @"and the window must not have gone on to the end of the picture");
    XCTAssertTrue([self storyIn:ctl].atBottomScrollTarget);

    // And a second scroll to the bottom must not take it further.
    [self scrollToBottomIn:ctl];
    XCTAssertEqualWithAccuracy(NSMinY(clipView.bounds), lastLineTop, 0.5);
}

#pragma mark - Storing and restoring the scroll position

// Scrolls to the bottom, stores the scroll position from y, then scrolls to
// the top and restores it.
- (CGFloat)restoredPositionFrom:(CGFloat)y in:(GlkController *)ctl {
    GlkTextBufferWindow *story = [self storyIn:ctl];
    [self scrollTo:y in:ctl];
    XCTAssertFalse(story.atBottomScrollTarget, @"%.1f is not at the bottom", y);
    [story storeScrollOffset];

    [self scrollTo:0 in:ctl];
    [story restoreScroll:nil];
    [self runFor:0.3];
    return NSMinY([self clipViewIn:ctl].bounds);
}

// A window scrolled up so that the bottom of the picture is cut off comes
// back where it was, not scrolled down to the end of the picture.
- (void)testScrollPositionCuttingTheImageOffIsRestored {
    GlkController *ctl = [self startGameWithPicture:NSMakeSize(100, 1000) lines:40];
    BufferTextView *textview = [self storyIn:ctl].textview;
    NSClipView *clipView = [self clipViewIn:ctl];
    XCTAssertGreaterThan(textview.bottomPadding, 0);

    [self scrollToBottomIn:ctl];
    CGFloat bottom = NSMinY(clipView.bounds);
    CGFloat cut = bottom - textview.bottomPadding / 2;
    XCTAssertGreaterThan(NSMaxY(clipView.bounds) - textview.bottomPadding / 2, [self textEndIn:ctl] - 1,
                         @"the position must show the end of the text and cut the picture off");

    XCTAssertEqualWithAccuracy([self restoredPositionFrom:cut in:ctl], cut, self.theme.bufferCellHeight);
}

// The same for a window scrolled halfway into the bottom margin below the
// text, with no picture to pad it.
- (void)testScrollPositionInTheBottomMarginIsRestored {
    GlkController *ctl = [self startGameWithPicture:NSMakeSize(100, 100) lines:60];
    BufferTextView *textview = [self storyIn:ctl].textview;
    NSClipView *clipView = [self clipViewIn:ctl];
    XCTAssertEqual(textview.bottomPadding, 0);
    CGFloat inset = textview.textContainerInset.height;
    XCTAssertGreaterThanOrEqual(inset, 8, @"the bottom margin must be wide enough to stop halfway into");

    [self scrollToBottomIn:ctl];
    XCTAssertEqualWithAccuracy(NSMaxY(clipView.bounds), NSHeight(textview.frame), 0.5);
    CGFloat halfway = NSMinY(clipView.bounds) - inset / 2;

    XCTAssertEqualWithAccuracy([self restoredPositionFrom:halfway in:ctl], halfway, 2);
}

// A window left at the bottom target of a padded frame (last line at the
// top) is at the bottom, and is restored to it.
- (void)testScrollPositionAtTheLastLineIsRestoredToTheBottom {
    GlkController *ctl = [self startGameWithPicture:NSMakeSize(100, 2000) lines:40];
    GlkTextBufferWindow *story = [self storyIn:ctl];
    NSClipView *clipView = [self clipViewIn:ctl];

    [self scrollToBottomIn:ctl];
    CGFloat bottom = NSMinY(clipView.bounds);
    XCTAssertTrue(story.atBottomScrollTarget);
    [story storeScrollOffset];

    [self scrollTo:0 in:ctl];
    [story restoreScroll:nil];
    [self runFor:0.3];
    XCTAssertEqualWithAccuracy(NSMinY(clipView.bounds), bottom, 0.5);
}

@end
