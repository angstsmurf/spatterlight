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

@end
