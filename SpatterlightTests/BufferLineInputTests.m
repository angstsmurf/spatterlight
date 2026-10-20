//
//  BufferLineInputTests.m
//  SpatterlightTests
//
//  Pins where line input starts in a buffer window after text that ends in
//  a newline.
//
//  printToWindow holds a trailing newline back (storedNewline) so that a
//  window waiting for a key has no blank line at the bottom. A line request
//  must take it out first: the input belongs on the line after it. Left
//  held back, the input joined the line above, and the newline landed after
//  the input when the next text was printed, as a blank line. An autosave
//  taken during the line request also archived the held newline, which is
//  where the "spurious storedNewlines" that postRestoreAdjustments: drops
//  came from.
//
//  The tests feed a GlkController the requests an interpreter sends, through
//  handleRequest:reply:buffer:.
//

#import "GlkControllerTestCase.h"

#import "GlkController.h"
#import "GlkWindow.h"
#import "GlkTextBufferWindow.h"
#import "GlkTextBufferWindow+Input.h"
#import "BufferTextView.h"
#import "Theme.h"

#include "glk.h"
#include "protocol.h"

static const int kStory = 0;

@interface BufferLineInputTests : GlkControllerTestCase
@end

@implementation BufferLineInputTests

- (void)setUp {
    [super setUp];
    [self makeGameWithIfid:@"ZCODE-1-261020-0002" theme:self.theme];
}

#pragma mark - Requests, as the interpreter sends them

- (GlkTextBufferWindow *)startGameIn:(GlkController *)ctl {
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextBuffer a2:kStory a3:0], kStory);
    return (GlkTextBufferWindow *)ctl.gwindows[@(kStory)];
}

- (void)print:(NSString *)text in:(GlkController *)ctl {
    [self send:PRINT to:ctl a1:kStory a2:style_Normal a3:0 text:text];
}

- (void)requestLineIn:(GlkController *)ctl {
    [self send:INITLINE to:ctl a1:kStory a2:80 a3:0 text:@""];
}

// The player types a line and presses return.
- (void)type:(NSString *)line in:(GlkTextBufferWindow *)story {
    [story.textview.textStorage appendAttributedString:[[NSAttributedString alloc] initWithString:line]];
    [story sendInputLine:line withTerminator:0];
}

- (NSString *)textIn:(GlkTextBufferWindow *)story {
    [story flushDisplay];
    return story.textview.textStorage.string;
}

#pragma mark - Tests

// The input starts on the line after the question.
- (void)testLineInputStartsAfterAHeldBackNewline {
    GlkController *ctl = [self makeController];
    GlkTextBufferWindow *story = [self startGameIn:ctl];

    [self print:@"Intro.\nName?\n" in:ctl];
    [self requestLineIn:ctl];

    XCTAssertEqualObjects([self textIn:story], @"Intro.\nName?\n");
    XCTAssertEqual(story.textview.selectedRange.location, story.textview.textStorage.length,
                   @"the caret must be at the start of the new line");
}

// The reply follows the input with no blank line in between.
- (void)testNoBlankLineAfterTheInput {
    GlkController *ctl = [self makeController];
    GlkTextBufferWindow *story = [self startGameIn:ctl];

    [self print:@"Intro.\nName?\n" in:ctl];
    [self requestLineIn:ctl];
    [self type:@"Bob" in:story];
    [self print:@"Hi Bob.\n" in:ctl];

    XCTAssertEqualObjects([self textIn:story], @"Intro.\nName?\nBob\nHi Bob.");
}

// A prompt on the same line as the input is unchanged.
- (void)testPromptOnTheInputLineIsUnchanged {
    GlkController *ctl = [self makeController];
    GlkTextBufferWindow *story = [self startGameIn:ctl];

    [self print:@"A room.\n\n>" in:ctl];
    [self requestLineIn:ctl];
    [self type:@"look" in:story];
    [self print:@"A room.\n" in:ctl];

    NSString *expected = self.theme.spaceFormat ? @"A room.\n\n> look\nA room." : @"A room.\n\n>look\nA room.";
    XCTAssertEqualObjects([self textIn:story], expected);
}

// A window waiting for a key keeps the newline held back, so there is no
// blank line at the bottom.
- (void)testCharInputKeepsTheNewlineHeldBack {
    GlkController *ctl = [self makeController];
    GlkTextBufferWindow *story = [self startGameIn:ctl];

    [self print:@"Press a key.\n" in:ctl];
    [self send:INITCHAR to:ctl a1:kStory a2:0 a3:0];

    XCTAssertEqualObjects([self textIn:story], @"Press a key.");
}

// An autosave taken while the line is requested holds the newline in the
// text, not held back.
- (void)testAutosaveDuringALineRequestHasTheNewlineInTheText {
    GlkController *ctl = [self makeController];
    GlkTextBufferWindow *story = [self startGameIn:ctl];

    [self print:@"Intro.\nName?\n" in:ctl];
    [self requestLineIn:ctl];
    [story flushDisplay];

    NSError *error = nil;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:story requiringSecureCoding:NO error:&error];
    XCTAssertNotNil(data, @"%@", error);
    NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
    unarchiver.requiresSecureCoding = NO;
    GlkTextBufferWindow *restored = [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
    XCTAssertNotNil(restored, @"%@", error);

    XCTAssertEqualObjects(restored.textview.textStorage.string, @"Intro.\nName?\n");
    XCTAssertNil([restored valueForKey:@"storedNewline"]);
}

@end
