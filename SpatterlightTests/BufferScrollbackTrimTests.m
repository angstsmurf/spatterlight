//
//  BufferScrollbackTrimTests.m
//  SpatterlightTests
//
//  Pins when -[GlkTextBufferWindow trimScrollbackIfNeeded:] trims the
//  scrollback.
//
//  A front trim shifts every glyph below the cut, so the whole text has to
//  be laid out again. Under a player who has scrolled up to read, that would
//  move or blank the view, so the text grows untrimmed until the player is
//  back at the bottom. (A background trim that swapped in a laid-out copy
//  was removed: it dropped the margin pictures, and lost text printed while
//  the copy was being laid out.)
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

// With a limit of 2000 characters a trim keeps about 1333, and starts above
// 5333.
static const int32_t kLimit = 2000;

@interface BufferScrollbackTrimTests : GlkControllerTestCase
@end

@implementation BufferScrollbackTrimTests

- (void)setUp {
    [super setUp];
    self.theme.smoothScroll = NO;
    self.theme.scrollbackLimit = kLimit;
    [self makeGameWithIfid:@"ZCODE-1-261020-0003" theme:self.theme];
}

#pragma mark - Requests, as the interpreter sends them

- (void)printLinesFrom:(NSUInteger)first to:(NSUInteger)last in:(GlkController *)ctl {
    NSMutableString *text = [NSMutableString string];
    for (NSUInteger i = first; i <= last; i++)
        [text appendFormat:@"Line %lu.\n", i];
    [self send:PRINT to:ctl a1:kStory a2:style_Normal a3:0 text:text];
}

#pragma mark - A session of the game

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

- (NSClipView *)clipViewIn:(GlkController *)ctl {
    return [self storyIn:ctl].textview.enclosingScrollView.contentView;
}

// A story window below the trim threshold, scrolled up to read the start.
- (GlkController *)startGameScrolledUp {
    GlkController *ctl = [self makeController];
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextBuffer a2:kStory a3:0], kStory);
    ctl.gwindows[@(kStory)].frame = NSMakeRect(0, 0, 700, 500);

    [self printLinesFrom:1 to:300 in:ctl];
    [self settle:ctl];
    GlkTextBufferWindow *story = [self storyIn:ctl];
    XCTAssertLessThan(story.textview.textStorage.length,
                      [GlkTextBufferWindow scrollbackTrimThresholdForLimit:kLimit]);

    NSClipView *clipView = [self clipViewIn:ctl];
    [clipView setBoundsOrigin:NSMakePoint(0, 600)];
    [story.textview.enclosingScrollView reflectScrolledClipView:clipView];
    [self runFor:0.1];
    XCTAssertFalse(story.scrolledToBottom);
    return ctl;
}

#pragma mark - Tests

// Text printed while the player reads further up is kept, and the view does
// not move, however far past the limit the text grows.
- (void)testNoTrimWhileScrolledUp {
    GlkController *ctl = [self startGameScrolledUp];
    GlkTextBufferWindow *story = [self storyIn:ctl];
    NSString *before = story.textview.textStorage.string.copy;

    [self printLinesFrom:301 to:1200 in:ctl];
    [self settle:ctl];

    NSString *after = story.textview.textStorage.string;
    XCTAssertGreaterThan(after.length, (NSUInteger)(4 * kLimit));
    XCTAssertTrue([after hasPrefix:before], @"nothing may be trimmed");
    XCTAssertEqual(NSMinY([self clipViewIn:ctl].bounds), 600);
}

// The first flush after the player is back at the bottom trims the text.
- (void)testTrimOnceBackAtTheBottom {
    GlkController *ctl = [self startGameScrolledUp];
    GlkTextBufferWindow *story = [self storyIn:ctl];
    [self printLinesFrom:301 to:1200 in:ctl];
    [self settle:ctl];
    NSString *full = story.textview.textStorage.string.copy;

    [story scrollToBottomAnimated:NO];
    [self runFor:0.1];
    [self printLinesFrom:1201 to:1201 in:ctl];
    [self settle:ctl];

    NSString *trimmed = story.textview.textStorage.string;
    XCTAssertLessThanOrEqual(trimmed.length, [GlkTextBufferWindow scrollbackTrimThresholdForLimit:kLimit]);
    XCTAssertTrue([trimmed hasPrefix:@"Line "], @"the cut must be at the start of a paragraph");
    XCTAssertTrue([trimmed hasSuffix:@"Line 1201."]);
    NSString *expected = [full stringByAppendingString:@"\nLine 1201."];
    XCTAssertTrue([expected hasSuffix:trimmed]);
}

@end
