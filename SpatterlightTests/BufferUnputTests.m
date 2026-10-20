//
//  BufferUnputTests.m
//  SpatterlightTests
//
//  Pins what -[GlkTextBufferWindow unputString:] (the UNPRINT request,
//  garglk_unput_string) does to the read boundary.
//
//  _lastseen is the y-coordinate down to which the player has seen the text,
//  recorded by markLastSeen at a keypress, and the auto-scroll pages from it.
//  A tail delete can leave it below the end of the shortened text. Left
//  there, the text printed next would count as read down to the old
//  boundary and be paged past, so the delete pulls the boundary up to the
//  new end of the text.
//
//  The tests feed a GlkController the requests an interpreter sends, through
//  handleRequest:reply:buffer:.
//

#import "GlkControllerTestCase.h"

#import "GlkController.h"
#import "GlkWindow.h"
#import "GlkTextBufferWindow.h"
#import "GlkTextBufferWindow+Scrolling.h"
#import "BufferTextView.h"
#import "Theme.h"

#include "glk.h"
#include "protocol.h"

static const int kStory = 0;

@interface BufferUnputTests : GlkControllerTestCase
@end

@implementation BufferUnputTests

- (void)setUp {
    [super setUp];
    self.theme.smoothScroll = NO;
    [self makeGameWithIfid:@"ZCODE-1-261020-0001" theme:self.theme];
}

#pragma mark - Requests, as the interpreter sends them

- (void)print:(NSString *)text in:(GlkController *)ctl {
    [self send:PRINT to:ctl a1:kStory a2:style_Normal a3:0 text:text];
}

- (int)unprint:(NSString *)text in:(GlkController *)ctl {
    return [self send:UNPRINT to:ctl a1:kStory a2:0 a3:0 text:text];
}

#pragma mark - A session of the game

- (NSString *)linesFrom:(NSUInteger)first to:(NSUInteger)last {
    NSMutableString *text = [NSMutableString string];
    for (NSUInteger i = first; i <= last; i++)
        [text appendFormat:@"Line %lu.\n", i];
    return text;
}

// The tail the tests take back: the last ten lines and the prompt. It must
// not end in a newline, as printToWindow holds a trailing newline back until
// the next print, so it is not in the text storage to match. (The callers in
// the interpreters only take back prompts and preloaded input.)
- (NSString *)tail {
    return [[self linesFrom:51 to:60] stringByAppendingString:@">"];
}

// A story window with more text than fits in it, scrolled to the bottom, and
// a keypress that marks all of it as read.
- (GlkController *)startGame {
    GlkController *ctl = [self makeController];
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextBuffer a2:kStory a3:0], kStory);
    ctl.gwindows[@(kStory)].frame = NSMakeRect(0, 0, 700, 500);

    [self print:[[self linesFrom:1 to:60] stringByAppendingString:@">"] in:ctl];
    [self settle:ctl];
    [[self storyIn:ctl] scrollToBottomAnimated:NO];
    [self runFor:0.3];
    [ctl markLastSeen];
    XCTAssertGreaterThan([self storyIn:ctl].lastseen, 0);
    return ctl;
}

- (GlkTextBufferWindow *)storyIn:(GlkController *)ctl {
    return (GlkTextBufferWindow *)ctl.gwindows[@(kStory)];
}

- (void)runFor:(NSTimeInterval)seconds {
    NSDate *until = [NSDate dateWithTimeIntervalSinceNow:seconds];
    while (until.timeIntervalSinceNow > 0)
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:until];
}

- (void)settle:(GlkController *)ctl {
    GlkTextBufferWindow *story = [self storyIn:ctl];
    [story flushDisplay];
    BufferTextView *textview = story.textview;
    [textview.layoutManager ensureLayoutForTextContainer:textview.textContainer];
    [self runFor:0.3];
}

// The bottom of the last line of text, in the coordinates of _lastseen.
- (CGFloat)textBottomIn:(GlkController *)ctl {
    NSLayoutManager *layout = [self storyIn:ctl].textview.layoutManager;
    NSRange glyphs = [layout glyphRangeForTextContainer:[self storyIn:ctl].textview.textContainer];
    return ceil(NSMaxY([layout lineFragmentRectForGlyphAtIndex:NSMaxRange(glyphs) - 1 effectiveRange:nil]));
}

#pragma mark - Tests

// Taking the last lines back pulls the read boundary up to the new end of
// the text.
- (void)testUnprintPullsTheReadBoundaryUp {
    GlkController *ctl = [self startGame];
    GlkTextBufferWindow *story = [self storyIn:ctl];

    NSString *tail = self.tail;
    XCTAssertEqual([self unprint:tail in:ctl], (int)tail.length);
    [self settle:ctl];

    CGFloat bottom = [self textBottomIn:ctl];
    XCTAssertLessThanOrEqual(story.lastseen, bottom, @"the read boundary must not be below the text");
    XCTAssertGreaterThan(story.lastseen, bottom - 2 * self.theme.bufferCellHeight,
                         @"the text that is left was read, all the way down");
}

// The text printed after the delete is unread, so the auto-scroll must not
// page past its start.
- (void)testOutputAfterAnUnprintIsNotPagedPast {
    GlkController *ctl = [self startGame];
    GlkTextBufferWindow *story = [self storyIn:ctl];
    NSClipView *clipView = story.textview.enclosingScrollView.contentView;

    XCTAssertEqual([self unprint:self.tail in:ctl], (int)self.tail.length);
    [self settle:ctl];
    CGFloat firstUnread = [self textBottomIn:ctl] + story.textview.textContainerOrigin.y;

    [self print:[self linesFrom:100 to:180] in:ctl];
    [self settle:ctl];
    [story reallyPerformScroll];
    [self runFor:0.3];

    XCTAssertLessThanOrEqual(NSMinY(clipView.bounds), firstUnread + 0.5,
                             @"the first unread line must not be scrolled out of the top");
    XCTAssertGreaterThan(NSMaxY(clipView.bounds), firstUnread,
                         @"the window must scroll on to the new text");
}

// A delete that leaves the text shorter than the boundary, with nothing
// printed yet, must not switch the auto-scroll off: a boundary past the end
// of the text is taken for a restarted game and reset to the top.
- (void)testAutoScrollStillWorksAfterAnUnprint {
    GlkController *ctl = [self startGame];
    GlkTextBufferWindow *story = [self storyIn:ctl];
    NSClipView *clipView = story.textview.enclosingScrollView.contentView;

    XCTAssertEqual([self unprint:self.tail in:ctl], (int)self.tail.length);
    [self settle:ctl];
    [story reallyPerformScroll];
    [self runFor:0.3];
    XCTAssertGreaterThan(story.lastseen, 0, @"the read boundary must survive");

    CGFloat before = NSMinY(clipView.bounds);
    [self print:@"A short reply.\n>" in:ctl];
    [self settle:ctl];
    [story reallyPerformScroll];
    [self runFor:0.3];

    XCTAssertGreaterThanOrEqual(NSMinY(clipView.bounds), before);
    XCTAssertEqualWithAccuracy(NSMaxY(clipView.bounds), NSHeight(story.textview.frame), 0.5,
                               @"a short reply must be scrolled into view");
}

@end
