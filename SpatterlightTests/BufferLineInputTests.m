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

- (GlkTextBufferWindow *)unarchive:(NSData *)data {
    NSError *error = nil;
    NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
    unarchiver.requiresSecureCoding = NO;
    GlkTextBufferWindow *restored = [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
    XCTAssertNotNil(restored, @"%@", error);
    return restored;
}

// The archive with one key dropped from every object in it.
- (NSData *)archive:(NSData *)data withoutKey:(NSString *)key {
    NSError *error = nil;
    NSMutableDictionary *plist = [NSPropertyListSerialization propertyListWithData:data options:NSPropertyListMutableContainersAndLeaves format:NULL error:&error];
    XCTAssertNotNil(plist, @"%@", error);
    for (id object in plist[@"$objects"]) {
        if ([object isKindOfClass:[NSMutableDictionary class]])
            [object removeObjectForKey:key];
    }
    NSData *stripped = [NSPropertyListSerialization dataWithPropertyList:plist format:NSPropertyListBinaryFormat_v1_0 options:0 error:&error];
    XCTAssertNotNil(stripped, @"%@", error);
    return stripped;
}

// An autosave taken mid-line-request, with text typed at the prompt, keeps
// that text out of the restored window's text: it comes back as the
// restored input, which the live line request appends behind its own
// fence. Scarier up to 1.5.5 wrote such archives when it answered the
// quit-time autosave request, and the typed text fenced in as game output
// made its first characters undeletable after the restore.
- (void)testTypedInputInAnAutosaveComesBackAsRestoredInput {
    GlkController *ctl = [self makeController];
    GlkTextBufferWindow *story = [self startGameIn:ctl];

    [self print:@"Intro.\n>" in:ctl];
    [self requestLineIn:ctl];
    [story flushDisplay];
    [story.textview.textStorage appendAttributedString:[[NSAttributedString alloc] initWithString:@"abc"]];
    story.textview.selectedRange = NSMakeRange(story.textview.textStorage.length, 0);

    NSError *error = nil;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:story requiringSecureCoding:NO error:&error];
    XCTAssertNotNil(data, @"%@", error);

    GlkTextBufferWindow *restored = [self unarchive:data];
    XCTAssertEqualObjects(restored.textview.textStorage.string, @"Intro.\n>");
    XCTAssertEqualObjects([[restored valueForKey:@"restoredInput"] string], @"abc");
    XCTAssertEqual([[restored valueForKey:@"fence"] unsignedIntegerValue], (NSUInteger)8);
    // The caret stays where it was, after the input, for when it is back.
    XCTAssertEqual([[restored valueForKey:@"restoredSelection"] rangeValue].location, (NSUInteger)11);
    XCTAssertEqual(restored.textview.selectedRange.location, (NSUInteger)8);

    // The same archive without its inputString, as one from before that
    // key was written: the typed text alone must do.
    restored = [self unarchive:[self archive:data withoutKey:@"inputString"]];
    XCTAssertEqualObjects(restored.textview.textStorage.string, @"Intro.\n>");
    XCTAssertEqualObjects([[restored valueForKey:@"restoredInput"] string], @"abc");
}

// The restored window, adopted into a new session: the interpreter's line
// request fences the prompt alone, and the input sits behind it once, all
// of it deletable.
- (void)testRestoredInputIsAppendedOnceBehindTheNewFence {
    GlkController *ctl = [self makeController];
    GlkTextBufferWindow *story = [self startGameIn:ctl];

    [self print:@"Intro.\n>" in:ctl];
    [self requestLineIn:ctl];
    [story flushDisplay];
    [story.textview.textStorage appendAttributedString:[[NSAttributedString alloc] initWithString:@"abc"]];

    NSError *error = nil;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:story requiringSecureCoding:NO error:&error];
    GlkTextBufferWindow *restored = [self unarchive:data];

    // Adopt it the way adoptRestoredWindow: does, and let the interpreter
    // re-request the line as it does on restart.
    ctl.gwindows[@(kStory)] = restored;
    restored.glkctl = ctl;
    restored.theme = self.theme;
    [self requestLineIn:ctl];
    [restored postRestoreAdjustments:restored];

    XCTAssertEqualObjects([self textIn:restored], @"Intro.\n>abc");
    XCTAssertEqual([[restored valueForKey:@"fence"] unsignedIntegerValue], (NSUInteger)8);
    XCTAssertEqual(restored.textview.selectedRange.location, (NSUInteger)11);
    XCTAssertEqual(restored.editableRange.location, (NSUInteger)8);
    XCTAssertEqual(restored.editableRange.length, (NSUInteger)3);
    // Deleting the first typed character is allowed: it is past the fence.
    XCTAssertTrue([restored textView:restored.textview shouldChangeTextInRange:NSMakeRange(8, 1) replacementString:@""]);
    XCTAssertFalse([restored textView:restored.textview shouldChangeTextInRange:NSMakeRange(7, 1) replacementString:@""]);
}

@end
