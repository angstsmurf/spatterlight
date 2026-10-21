//
//  RotorHandlerTests.m
//  SpatterlightTests
//
//  Pins what the VoiceOver rotors find in the text of grid and buffer
//  windows. RotorHandler reaches the text through -[GlkWindow textview],
//  which the subclasses must still synthesize: GlkWindow implements it
//  (returning nil), and a base-class getter silently stops a subclass's
//  property from being auto-synthesized.
//

#import "GlkControllerTestCase.h"

#import "GlkController.h"
#import "GlkWindow.h"
#import "GlkTextGridWindow.h"
#import "GlkTextBufferWindow.h"
#import "GlkTextBufferWindow+Input.h"
#import "GlkGraphicsWindow.h"
#import "BufferTextView.h"
#import "RotorHandler.h"
#import "Theme.h"

#include "glk.h"
#include "protocol.h"

static const int kStory = 0;
static const int kStatus = 1;

@interface RotorHandlerTests : GlkControllerTestCase
@end

@implementation RotorHandlerTests

- (void)setUp {
    [super setUp];
    self.theme.smoothScroll = NO;
    [self makeGameWithIfid:@"ZCODE-1-261020-0002" theme:self.theme];
}

// A status grid over a story buffer, as most games have.
- (GlkController *)startGame {
    GlkController *ctl = [self makeController];
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextBuffer a2:kStory a3:0], kStory);
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextGrid a2:kStatus a3:0], kStatus);
    ctl.gwindows[@(kStory)].frame = NSMakeRect(0, 0, 700, 400);
    ctl.gwindows[@(kStatus)].frame = NSMakeRect(0, 400, 700, 40);
    [ctl.gwindows[@(kStatus)] flushDisplay];
    return ctl;
}

- (void)print:(NSString *)text in:(int)win of:(GlkController *)ctl {
    [self send:PRINT to:ctl a1:win a2:style_Normal a3:0 text:text];
}

- (void)flush:(GlkController *)ctl {
    for (GlkWindow *win in ctl.gwindows.allValues)
        [win flushDisplay];
}

- (RotorHandler *)rotorsFor:(GlkController *)ctl {
    RotorHandler *handler = [RotorHandler new];
    handler.glkctl = ctl;
    return handler;
}

- (NSString *)textOf:(NSAccessibilityCustomRotorItemResult *)result {
    NSTextView *textview = (NSTextView *)result.targetElement;
    return [textview.string substringWithRange:result.targetRange];
}

// One update cycle, as GlkController's flushDisplay runs it with VoiceOver
// off: flush every window and record the latest move. The interpreter
// causes one at every glk_select, so a timer event or a window resize has
// one happen while the player is still typing.
- (void)cycle:(GlkController *)ctl {
    for (GlkWindow *win in ctl.gwindows.allValues) {
        [win flushDisplay];
        if (![win isKindOfClass:[GlkGraphicsWindow class]])
            [win setLastMove];
    }
}

- (GlkTextBufferWindow *)storyOf:(GlkController *)ctl {
    return (GlkTextBufferWindow *)ctl.gwindows[@(kStory)];
}

- (void)requestLineIn:(GlkController *)ctl {
    [self send:INITLINE to:ctl a1:kStory a2:80 a3:0 text:@""];
}

// The player types some more of the line, without sending it.
- (void)keyIn:(NSString *)text in:(GlkController *)ctl {
    [[self storyOf:ctl].textview.textStorage appendAttributedString:[[NSAttributedString alloc] initWithString:text]];
}

// The player presses backspace `count` times.
- (void)backspace:(NSUInteger)count in:(GlkController *)ctl {
    NSTextStorage *storage = [self storyOf:ctl].textview.textStorage;
    [storage deleteCharactersInRange:NSMakeRange(storage.length - count, count)];
}

// The player presses return on the line typed so far.
- (void)returnIn:(GlkController *)ctl {
    GlkTextBufferWindow *story = [self storyOf:ctl];
    NSString *line = [story.textview.textStorage.string substringFromIndex:story.editableRange.location];
    [story sendInputLine:line withTerminator:0];
}

// Every item of the command history rotor, newest first, as VoiceOver
// collects them: Next from no current item until there is nothing left.
- (NSArray<NSString *> *)commandHistoryOf:(GlkController *)ctl {
    RotorHandler *rotors = [self rotorsFor:ctl];
    NSAccessibilityCustomRotor *rotor = [[NSAccessibilityCustomRotor alloc] initWithLabel:@"Command history" itemSearchDelegate:rotors];
    NSAccessibilityCustomRotorSearchParameters *search = [NSAccessibilityCustomRotorSearchParameters new];
    search.searchDirection = NSAccessibilityCustomRotorSearchDirectionNext;
    NSMutableArray<NSString *> *items = [NSMutableArray array];
    for (NSUInteger i = 0; i < 50; i++) {
        NSAccessibilityCustomRotorItemResult *result = [rotors commandHistoryRotor:rotor resultForSearchParameters:search];
        if (!result)
            break;
        [items addObject:[self textOf:result]];
        search.currentItem = result;
    }
    return items;
}

#pragma mark - Tests

- (void)testGridAndBufferWindowsHaveTextViews {
    GlkController *ctl = [self startGame];
    XCTAssertNotNil(ctl.gwindows[@(kStory)].textview);
    XCTAssertNotNil(ctl.gwindows[@(kStatus)].textview);
    XCTAssertEqual(ctl.gwindows[@(kStory)].textview, ((GlkTextBufferWindow *)ctl.gwindows[@(kStory)]).textview);
    XCTAssertEqual(ctl.gwindows[@(kStatus)].textview, ((GlkTextGridWindow *)ctl.gwindows[@(kStatus)]).textview);
}

- (void)testTextSearchFindsTextInEitherWindow {
    GlkController *ctl = [self startGame];
    [self print:@"Lamp Room" in:kStatus of:ctl];
    [self print:@"You see a brass lamp here.\n>" in:kStory of:ctl];
    [self flush:ctl];
    RotorHandler *rotors = [self rotorsFor:ctl];

    NSAccessibilityCustomRotorItemResult *result =
    [rotors textSearchResultForString:@"room" fromRange:NSMakeRange(0, 0)
                            direction:NSAccessibilityCustomRotorSearchDirectionNext];
    XCTAssertEqual(result.targetElement, ctl.gwindows[@(kStatus)].textview);
    XCTAssertEqualObjects([self textOf:result], @"Room");

    result = [rotors textSearchResultForString:@"BRASS" fromRange:NSMakeRange(0, 0)
                                     direction:NSAccessibilityCustomRotorSearchDirectionNext];
    XCTAssertEqual(result.targetElement, ctl.gwindows[@(kStory)].textview);
    XCTAssertEqualObjects([self textOf:result], @"brass");

    XCTAssertNil([rotors textSearchResultForString:@"sword" fromRange:NSMakeRange(0, 0)
                                         direction:NSAccessibilityCustomRotorSearchDirectionNext]);
}

- (void)testLinksRotorStepsThroughTheLinksOfTheBuffer {
    GlkController *ctl = [self startGame];
    [self print:@"Go " in:kStory of:ctl];
    [self send:SETLINK to:ctl a1:kStory a2:1 a3:0];
    [self print:@"north" in:kStory of:ctl];
    [self send:SETLINK to:ctl a1:kStory a2:0 a3:0];
    [self print:@" or " in:kStory of:ctl];
    [self send:SETLINK to:ctl a1:kStory a2:2 a3:0];
    [self print:@"south" in:kStory of:ctl];
    [self send:SETLINK to:ctl a1:kStory a2:0 a3:0];
    [self print:@".\n>" in:kStory of:ctl];
    [self flush:ctl];
    RotorHandler *rotors = [self rotorsFor:ctl];
    NSAccessibilityCustomRotor *rotor = [[NSAccessibilityCustomRotor alloc] initWithRotorType:NSAccessibilityCustomRotorTypeLink itemSearchDelegate:rotors];

    NSAccessibilityCustomRotorSearchParameters *search = [NSAccessibilityCustomRotorSearchParameters new];
    search.searchDirection = NSAccessibilityCustomRotorSearchDirectionNext;
    NSMutableArray<NSString *> *labels = [NSMutableArray array];
    for (NSUInteger i = 0; i < 2; i++) {
        NSAccessibilityCustomRotorItemResult *result = [rotors linksRotor:rotor resultForSearchParameters:search];
        XCTAssertNotNil(result);
        if (!result)
            break;
        XCTAssertEqual(result.targetElement, ctl.gwindows[@(kStory)].textview);
        XCTAssertEqualObjects(result.customLabel, [self textOf:result]);
        [labels addObject:result.customLabel];
        search.currentItem = result;
    }
    XCTAssertEqualObjects(labels, (@[@"north", @"south"]));
    // Past the last link there is nothing, and VoiceOver starts over.
    XCTAssertNil([rotors linksRotor:rotor resultForSearchParameters:search]);

    search.searchDirection = NSAccessibilityCustomRotorSearchDirectionPrevious;
    XCTAssertEqualObjects([rotors linksRotor:rotor resultForSearchParameters:search].customLabel, @"north");

    search.currentItem = nil;
    search.searchDirection = NSAccessibilityCustomRotorSearchDirectionNext;
    search.filterString = @"sou";
    XCTAssertEqualObjects([rotors linksRotor:rotor resultForSearchParameters:search].customLabel, @"south");
}

// Issue #26: the command history rotor got out of step with the text. A
// move recorded while the player was still typing (every timer or resize
// update cycle does that) ended inside the command, so the next move began
// mid-word: "x Closed." instead of "close mailbox Closed.". And once the
// player had made the line shorter, the move ran past the end of the text,
// was dropped, and the whole transcript was recorded over again as one
// move. The line being typed belongs to no move until it is sent.
- (void)testCommandHistoryIgnoresTheLineBeingTyped {
    GlkController *ctl = [self startGame];
    [self print:@"West of House\n>" in:kStory of:ctl];
    [self requestLineIn:ctl];
    [self cycle:ctl];

    [self keyIn:@"close mailbo" in:ctl];
    [self cycle:ctl]; // A timer fires.
    [self keyIn:@"x" in:ctl];
    [self cycle:ctl];
    [self backspace:4 in:ctl];
    [self cycle:ctl];
    [self keyIn:@"lbox" in:ctl];
    [self returnIn:ctl];
    [self print:@"Closed.\n>" in:kStory of:ctl];
    [self requestLineIn:ctl];
    [self cycle:ctl];

    NSString *prompt = self.theme.spaceFormat ? @"> " : @">";
    NSArray *expected = @[[@"close mailbox\nClosed.\n" stringByAppendingString:prompt],
                          [@"West of House\n" stringByAppendingString:prompt]];
    XCTAssertEqualObjects([self commandHistoryOf:ctl], expected);

    // The moves tile the text: each starts where the one before ended.
    GlkTextBufferWindow *story = [self storyOf:ctl];
    NSUInteger end = 0;
    for (NSValue *value in story.moveRanges) {
        XCTAssertEqual(value.rangeValue.location, end);
        end = NSMaxRange(value.rangeValue);
    }
    XCTAssertEqual(end, story.editableRange.location, @"the last move ends where the input starts");
}

// With echo off, the line stays on screen after it is sent until the next
// output deletes it. It belongs to no move either: a move recorded over it
// would end inside the response that replaces it.
- (void)testCommandHistoryIgnoresASentLineWaitingForItsEchoOffDelete {
    GlkController *ctl = [self startGame];
    [self send:SETECHO to:ctl a1:kStory a2:0 a3:0];
    [self print:@"Password?\n" in:kStory of:ctl];
    [self requestLineIn:ctl];
    [self cycle:ctl];

    [self keyIn:@"xyzzy" in:ctl];
    [self returnIn:ctl];
    [self cycle:ctl]; // Nothing printed yet: the line is still on screen.
    // A reply shorter than the line it replaces.
    [self print:@"No.\n" in:kStory of:ctl];
    [self requestLineIn:ctl];
    [self cycle:ctl];

    XCTAssertEqualObjects([self commandHistoryOf:ctl], (@[@"No.\n", @"Password?\n"]));
}

// The newest move grows while the game prints more of it. VoiceOver hands
// back the range it showed for an item, which for the newest one may be the
// shorter range from before that; the rotor must step on from it, not start
// over at the newest item every time.
- (void)testCommandHistoryStepsOnFromAStaleNewestItem {
    GlkController *ctl = [self startGame];
    [self print:@"West of House\n" in:kStory of:ctl];
    [self send:INITCHAR to:ctl a1:kStory a2:0 a3:0];
    [self cycle:ctl];
    [[self storyOf:ctl] sendKeypress:' '];
    [self print:@"You are here." in:kStory of:ctl];
    [self cycle:ctl];

    RotorHandler *rotors = [self rotorsFor:ctl];
    NSAccessibilityCustomRotor *rotor = [[NSAccessibilityCustomRotor alloc] initWithLabel:@"Command history" itemSearchDelegate:rotors];
    NSAccessibilityCustomRotorSearchParameters *search = [NSAccessibilityCustomRotorSearchParameters new];
    search.searchDirection = NSAccessibilityCustomRotorSearchDirectionNext;
    NSAccessibilityCustomRotorItemResult *newest = [rotors commandHistoryRotor:rotor resultForSearchParameters:search];
    XCTAssertEqualObjects([self textOf:newest], @"\nYou are here.");

    [self print:@" A bell rings." in:kStory of:ctl];
    [self cycle:ctl];
    XCTAssertEqualObjects([self commandHistoryOf:ctl], (@[@"\nYou are here. A bell rings.", @"West of House"]));

    search.currentItem = newest;
    XCTAssertEqualObjects([self textOf:[rotors commandHistoryRotor:rotor resultForSearchParameters:search]], @"West of House");
}

@end
