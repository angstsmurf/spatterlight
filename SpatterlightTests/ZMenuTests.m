//
//  ZMenuTests.m
//  SpatterlightTests
//
//  Menu detection fixtures, one per rule in ZMenu: the generic " = "
//  instructions, Beyond Zork, Anchorhead, Vespers, The Unforgotten, Hugo,
//  TADS 3, Thaumistry and Junior Arithmancer, plus the ways a window is
//  turned down. Each fixture puts hand-written text in real Glk windows on
//  a controller without an interpreter and asks ZMenu (and so
//  ZMenuDetector) to find the menu.
//

#import "GlkControllerTestCase.h"

#import "GlkController.h"
#import "GlkController_Private.h"
#import "GlkController+GlkRequests.h"
#import "GlkController+Speech.h"
#import "InfocomV6MenuHandler.h"
#import "BureaucracyForm.h"
#import "GlkWindow.h"
#import "GlkTextGridWindow.h"
#import "GlkTextBufferWindow.h"
#import "GridTextView.h"
#import "BufferTextView.h"
#import "Game.h"
#import "ZMenu.h"
#import "ZMenuDetector.h"

#include "glk.h"
#include "protocol.h"

@interface ZMenu (ZMenuTests)
@property ZMenuDetection *detection;
@property NSUInteger selectedLine;
@end

// A window for -detect:…: its type, text and frame.
@interface ZMenuTestWindow : NSObject
@property glui32 type;
@property NSAttributedString *text;
@property NSRect frame;
@end

@implementation ZMenuTestWindow
@end

@interface ZMenuTests : GlkControllerTestCase
@end

@implementation ZMenuTests

- (void)setUp {
    [super setUp];
    [self makeGameWithIfid:@"ZCODE-1-261020-0000" theme:self.theme];
}

#pragma mark - Building the windows

// Not the colour of any theme's background.
static NSDictionary *Highlight(void) {
    return @{ NSBackgroundColorAttributeName : [NSColor colorWithSRGBRed:0.2 green:0.4 blue:0.8 alpha:1],
              NSForegroundColorAttributeName : NSColor.whiteColor };
}

// Plain text with the first occurrence of each string in `highlighted`
// shown in reverse video.
- (NSAttributedString *)text:(NSString *)plain highlight:(NSArray<NSString *> *)highlighted {
    NSMutableAttributedString *text = [[NSMutableAttributedString alloc] initWithString:plain];
    for (NSString *part in highlighted) {
        NSRange range = [plain rangeOfString:part];
        XCTAssertNotEqual(range.location, (NSUInteger)NSNotFound, @"%@", part);
        [text addAttributes:Highlight() range:range];
    }
    return text;
}

- (NSAttributedString *)text:(NSString *)plain {
    return [self text:plain highlight:@[]];
}

- (ZMenuTestWindow *)window:(glui32)type text:(NSAttributedString *)text frame:(NSRect)frame {
    ZMenuTestWindow *win = [ZMenuTestWindow new];
    win.type = type;
    win.text = text;
    win.frame = frame;
    return win;
}

// A status window at the top and a story window below it.
- (ZMenuTestWindow *)grid:(NSAttributedString *)text {
    return [self window:wintype_TextGrid text:text frame:NSMakeRect(0, 0, 600, 200)];
}

- (ZMenuTestWindow *)buffer:(NSAttributedString *)text {
    return [self window:wintype_TextBuffer text:text frame:NSMakeRect(0, 200, 600, 300)];
}

- (GlkController *)controllerForGame:(kGameIdentity)gameID format:(NSString *)format windows:(NSArray<ZMenuTestWindow *> *)windows {
    self.game.detectedFormat = format;
    GlkController *ctl = [self makeController];
    ctl.gameID = gameID;
    int name = 0;
    for (ZMenuTestWindow *spec in windows) {
        XCTAssertEqual([self send:NEWWIN to:ctl a1:(int)spec.type a2:name a3:0], name);
        GlkWindow *win = ctl.gwindows[@(name)];
        win.frame = spec.frame;
        [win flushDisplay];
        // Replaced after the flush, which would otherwise lay out the grid
        // again from its own buffer.
        NSTextStorage *storage = spec.type == wintype_TextGrid ? ((GlkTextGridWindow *)win).textview.textStorage : ((GlkTextBufferWindow *)win).textview.textStorage;
        [storage setAttributedString:spec.text];
        name++;
    }
    return ctl;
}

- (nullable ZMenu *)detect:(NSArray<ZMenuTestWindow *> *)windows game:(kGameIdentity)gameID format:(NSString *)format {
    GlkController *ctl = [self controllerForGame:gameID format:format windows:windows];
    ZMenu *menu = [[ZMenu alloc] initWithGlkController:ctl];
    ctl.zmenu = menu;
    return [menu detectMenu] ? menu : nil;
}

// The menu lines, trimmed.
- (NSArray<NSString *> *)linesOf:(ZMenu *)menu {
    NSMutableArray *lines = [NSMutableArray new];
    for (NSUInteger i = 0; i < menu.detection.lines.count; i++)
        [lines addObject:[[menu.detection stringForLine:i] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet]];
    return lines;
}

- (NSString *)fullLineOf:(ZMenu *)menu {
    return [menu menuLineStringWithTitle:YES index:YES total:YES instructions:YES];
}

#pragma mark - Inform " = " instructions

- (NSString *)informMenu {
    return @"             About the game\n"
    @" N = next subject    P = previous\n"
    @" RETURN = read subject    Q = resume game\n"
    @"\n"
    @"  Introduction\n"
    @"  Credits\n"
    @"  Hints\n";
}

// The instruction lines are dropped from the cluster and the blank line
// after them is not a menu line.
- (void)testInformMenuInReverseVideo {
    ZMenu *menu = [self detect:@[[self grid:[self text:self.informMenu highlight:@[@"  Credits"]]]] game:kGameIsGeneric format:@"zcode"];
    XCTAssertNotNil(menu);
    XCTAssertEqualObjects([self linesOf:menu], (@[@"Introduction", @"Credits", @"Hints"]));
    XCTAssertEqual(menu.selectedLine, 1u);
    XCTAssertEqualObjects([self fullLineOf:menu],
                          @"We are in a menu, \"About the game.\"\n"
                          @"  Credits\n.\nMenu item 2 of 3.\n"
                          @"N: \"next subject\".\nP: \"previous\".\nReturn: \"read subject\".\nQ: \"resume game\".\n");
}

- (void)testGlulxMenuWithLeadingMarker {
    NSString *plain = @" N = next    P = previous\n\n   Introduction\n > Credits\n   Hints\n";
    ZMenu *menu = [self detect:@[[self grid:[self text:plain]]] game:kGameIsGeneric format:@"glulx"];
    XCTAssertNotNil(menu);
    XCTAssertEqualObjects([self linesOf:menu], (@[@"Introduction", @"> Credits", @"Hints"]));
    XCTAssertEqual(menu.selectedLine, 1u);
    // The marker is not spoken. The grid has no title line.
    XCTAssertEqualObjects([self fullLineOf:menu], @"We are in a menu.\nCredits\n.\nMenu item 2 of 3.\nN: \"next\".\nP: \"previous\".\n");
}

- (void)testTrailingAsteriskMarksTheSelection {
    NSString *plain = @" N = next    P = previous\n\n   Introduction\n   Credits *\n   Hints\n";
    ZMenu *menu = [self detect:@[[self grid:[self text:plain]]] game:kGameIsGeneric format:@"zcode"];
    XCTAssertNotNil(menu);
    XCTAssertEqual(menu.selectedLine, 1u);
}

// With no marker and no change of background, a single line in another
// colour is the selected one.
- (void)testUniqueForegroundColourMarksTheSelection {
    NSString *plain = @" N = next    P = previous\n\n   Introduction\n   Credits\n   Hints\n";
    NSMutableAttributedString *text = [[self text:plain] mutableCopy];
    [text addAttribute:NSForegroundColorAttributeName value:NSColor.blueColor range:NSMakeRange(0, text.length)];
    [text addAttribute:NSForegroundColorAttributeName value:NSColor.redColor range:[plain rangeOfString:@"Hints"]];
    ZMenu *menu = [self detect:@[[self grid:text]] game:kGameIsGeneric format:@"zcode"];
    XCTAssertNotNil(menu);
    XCTAssertEqual(menu.selectedLine, 2u);
}

- (void)testMenuItemTextComesFromTheMenuWindowAndTitleFromTheTopWindow {
    NSString *status = @" Help menu\n";
    NSString *story = @" N = next    P = previous\n\n   Introduction\n > Credits\n";
    ZMenu *menu = [self detect:@[[self grid:[self text:status]], [self buffer:[self text:story]]] game:kGameIsGeneric format:@"zcode"];
    XCTAssertNotNil(menu);
    XCTAssertEqualObjects([self linesOf:menu], (@[@"Introduction", @"> Credits"]));
    XCTAssertTrue([[self fullLineOf:menu] hasPrefix:@"We are in a menu, \"Help menu.\"\nCredits\n.\nMenu item 2 of 2.\n"]);
}

#pragma mark - Ways a window is turned down

- (void)testNoInstructionsIsNotAMenu {
    NSString *plain = @"\n   Introduction\n > Credits\n   Hints\n";
    XCTAssertNil([self detect:@[[self grid:[self text:plain]]] game:kGameIsGeneric format:@"zcode"]);
}

- (void)testNoSelectionIsNotAMenu {
    NSString *plain = @" N = next    P = previous\n\n   Introduction\n   Credits\n   Hints\n";
    XCTAssertNil([self detect:@[[self grid:[self text:plain]]] game:kGameIsGeneric format:@"zcode"]);
}

- (void)testBlankWindowsAreNotAMenu {
    NSArray *windows = @[[self grid:[self text:@"\n\n\n\n"]], [self buffer:[self text:@""]]];
    XCTAssertNil([self detect:windows game:kGameIsGeneric format:@"zcode"]);
}

- (void)testMoreThan25LinesIsNotAMenu {
    NSMutableString *plain = [@" N = next    P = previous\n\n" mutableCopy];
    for (NSUInteger i = 0; i < 26; i++)
        [plain appendFormat:@"  %@ Item %lu\n", i == 3 ? @">" : @" ", i];
    XCTAssertNil([self detect:@[[self buffer:[self text:plain]]] game:kGameIsGeneric format:@"zcode"]);
}

- (void)testHiddenWindowIsNotSearched {
    ZMenuTestWindow *win = [self grid:[self text:@" N = next    P = previous\n\n   Introduction\n > Credits\n"]];
    win.frame = NSMakeRect(0, 0, 600, 0);
    XCTAssertNil([self detect:@[win] game:kGameIsGeneric format:@"zcode"]);
}

- (void)testOtherFormatsHaveNoMenus {
    NSString *plain = @" N = next    P = previous\n\n   Introduction\n > Credits\n";
    XCTAssertNil([self detect:@[[self grid:[self text:plain]]] game:kGameIsGeneric format:@"tads2"]);
}

- (void)testJuniorArithmancerHasNoMenus {
    XCTAssertNil([self detect:@[[self grid:[self text:self.informMenu highlight:@[@"  Credits"]]]] game:kGameIsJuniorArithmancer format:@"glulx"]);
}

// An Infocom V6 menu reported by the interpreter is spoken instead of any
// menu in the window text, and keeps its own "spoken" flag.
- (void)testInfocomV6MenuTakesPrecedence {
    GlkController *ctl = [self controllerForGame:kGameIsGeneric format:@"zcode" windows:@[[self buffer:[self text:self.informMenu highlight:@[@"  Credits"]]]]];
    InfocomV6MenuHandler *handler = ctl.infocomV6MenuHandler;
    [handler handleMenuItemOfType:kV6MenuTypeTopic index:0 total:1 text:(char *)"Hints" length:5];
    ZMenu *menu = [[ZMenu alloc] initWithGlkController:ctl];
    ctl.zmenu = menu;
    XCTAssertTrue([menu detectMenu]);
    XCTAssertEqual(ctl.currentMenu, (id<VoiceOverMenu>)handler);
    XCTAssertEqualObjects([ctl.currentMenu menuLineStringWithTitle:NO index:YES total:YES instructions:NO], @"Hints.\nMenu item 1 of 1.\n");

    [handler handleMenuItemOfType:kV6MenuSelectionChanged index:0 total:0 text:NULL length:0];
    XCTAssertTrue(handler.haveSpokenMenu);
    XCTAssertFalse(menu.haveSpokenMenu);

    [handler handleMenuItemOfType:kV6MenuExited index:0 total:0 text:NULL length:0];
    XCTAssertFalse(ctl.showingInfocomV6Menu);
    XCTAssertEqual(ctl.currentMenu, (id<VoiceOverMenu>)menu);
}

// Detecting a Bureaucracy form only reads the window; the caller adds the
// current field to the moves.
- (void)testBureaucracyFormDetectionLeavesTheMovesAlone {
    NSMutableAttributedString *text = [NSMutableAttributedString new];
    NSArray *runs = @[@"  ", @"|", @" ", @"  DEPOSIT SLIP  ", @" ", @"|", @" ", @"|", @"Please fill in the form", @"\n"];
    NSArray *colors = @[NSColor.blackColor, NSColor.whiteColor];
    [runs enumerateObjectsUsingBlock:^(NSString *run, NSUInteger i, BOOL *stop) {
        [text appendAttributedString:[[NSAttributedString alloc] initWithString:run attributes:@{NSBackgroundColorAttributeName : colors[i % 2]}]];
    }];
    [text appendAttributedString:[[NSAttributedString alloc] initWithString:@"Last name: \nFirst name: \nMiddle initial: \nAmount of deposit: $ \nFrom illegal activity? (y/n): \nIf yes, which one: "]];
    GlkController *ctl = [self controllerForGame:kGameIsBureaucracy format:@"zcode" windows:@[[self grid:text]]];
    GlkTextGridWindow *grid = (GlkTextGridWindow *)ctl.gwindows[@0];
    grid.moveRanges = nil;

    BureaucracyForm *form = [[BureaucracyForm alloc] initWithGlkController:ctl];
    XCTAssertTrue([form detectForm]);
    XCTAssertTrue([form detectForm]);
    XCTAssertEqual(grid.moveRanges.count, 0u);

    [form addCurrentFieldToMoves];
    [form addCurrentFieldToMoves];
    XCTAssertEqual(grid.moveRanges.count, 1u);
}

#pragma mark - Per-game rules

- (void)testBeyondZorkArrowKeys {
    NSString *plain = @"      Begin using a preset character\n"
    @"      Create your own character\n"
    @"      Quit\n"
    @"\n"
    @" Use the ↑ and ↓ keys to select, then press RETURN. >\n";
    NSAttributedString *text = [self text:plain highlight:@[@"Create your own character"]];
    ZMenu *menu = [self detect:@[[self grid:[self text:@"\n"]], [self buffer:text]] game:kGameIsBeyondZork format:@"zcode"];
    XCTAssertNotNil(menu);
    XCTAssertEqualObjects([self linesOf:menu], (@[@"Begin using a preset character", @"Create your own character", @"Quit"]));
    XCTAssertEqual(menu.selectedLine, 1u);
    // The title is made up from the first line, and the arrows are named.
    XCTAssertEqualObjects([self fullLineOf:menu],
                          @"We are in a menu, \"Character Setup.\"\n"
                          @"      Create your own character\n.\nMenu item 2 of 3.\n"
                          @"Use the UP ARROW and DOWN ARROW keys to select, then press RETURN. \n");
}

- (void)testBeyondZorkFunctionKeyDefinitions {
    NSString *plain = @"       Function Key Definitions\n"
    @"\n"
    @"  F1   look\n"
    @"  F2   inventory\n"
    @"  F3   status\n"
    @"\n"
    @"  Restore Defaults\n"
    @"  Exit\n";
    NSAttributedString *text = [self text:plain highlight:@[@"F2   inventory"]];
    ZMenu *menu = [self detect:@[[self grid:text]] game:kGameIsBeyondZork format:@"zcode"];
    XCTAssertNotNil(menu);
    XCTAssertEqualObjects([self linesOf:menu], (@[@"F1   look", @"F2   inventory", @"F3   status", @"", @"Restore Defaults", @"Exit", @"Restore Defaults", @"Exit"]));
    XCTAssertEqual(menu.selectedLine, 1u);
}

- (void)testAnchorhead {
    NSString *status = @" [press BACKSPACE to return to the game]\n";
    NSString *story = @"\n   Introduction\n > Credits\n   Hints\n";
    ZMenu *menu = [self detect:@[[self grid:[self text:status]], [self buffer:[self text:story]]] game:kGameIsAnchorheadOriginal format:@"zcode"];
    XCTAssertNotNil(menu);
    XCTAssertEqualObjects([self linesOf:menu], (@[@"Introduction", @"> Credits", @"Hints"]));
    XCTAssertEqual(menu.selectedLine, 1u);
    XCTAssertTrue([[self fullLineOf:menu] hasSuffix:@"Menu item 2 of 3.\nBackspace: \"to return to the game\".\n"]);
}

// Instructions below the menu in the same window are taken as menu lines.
- (void)testInstructionsBelowTheMenuAreMenuLines {
    NSString *plain = @"\n   Introduction\n > Credits\n   Hints\n\n [press BACKSPACE to return to the game]\n";
    ZMenu *menu = [self detect:@[[self buffer:[self text:plain]]] game:kGameIsAnchorheadOriginal format:@"zcode"];
    XCTAssertNotNil(menu);
    XCTAssertEqualObjects([self linesOf:menu], (@[@"Introduction", @"> Credits", @"Hints", @"", @"[press BACKSPACE to return to the game]"]));
}

// No spaces around "=", and a first line that is not part of the menu.
- (void)testVespers {
    NSString *status = @" P=Go up  Q=Quit\n";
    NSString *story = @"Help\n"
    @"   Introduction\n"
    @" > Credits\n"
    @"   Hints\n";
    ZMenu *menu = [self detect:@[[self grid:[self text:status]], [self buffer:[self text:story]]] game:kGameIsVespers format:@"zcode"];
    XCTAssertNotNil(menu);
    XCTAssertEqualObjects([self linesOf:menu], (@[@"Introduction", @"> Credits", @"Hints"]));
    XCTAssertEqual(menu.selectedLine, 1u);
    XCTAssertTrue([[self fullLineOf:menu] hasSuffix:@"P: \"Go up\".\nQ: \"Quit\".\n"]);
}

// No instructions: the title is what tells it is a menu. The blank fourth
// line of the menu is skipped.
- (void)testTheUnforgotten {
    NSString *status = @"   Unforgotten\n   By Quintin Pan\n";
    NSString *story = @"   Begin\n"
    @"   About\n"
    @"   Credits\n"
    @"\n"
    @"   Quit\n";
    NSAttributedString *text = [self text:story highlight:@[@"About"]];
    ZMenu *menu = [self detect:@[[self grid:[self text:status]], [self buffer:text]] game:kGameIsGeneric format:@"zcode"];
    XCTAssertNotNil(menu);
    XCTAssertEqualObjects([self linesOf:menu], (@[@"Begin", @"About", @"Credits", @"Quit"]));
    XCTAssertEqual(menu.selectedLine, 1u);
}

// The title in the menu window is cut from the menu, which then has no
// fourth line to skip.
- (void)testTheUnforgottenTitleInTheMenuWindow {
    NSString *plain = @"   Unforgotten\n"
    @"   By Quintin Pan\n"
    @"\n"
    @"   Begin\n"
    @"   About\n"
    @"   Quit\n";
    NSAttributedString *text = [self text:plain highlight:@[@"About"]];
    ZMenu *menu = [self detect:@[[self buffer:text]] game:kGameIsGeneric format:@"zcode"];
    XCTAssertNotNil(menu);
    XCTAssertEqualObjects([self linesOf:menu], (@[@"Begin", @"About", @"Quit"]));
    XCTAssertEqual(menu.selectedLine, 1u);
}

- (void)testHugo {
    NSString *plain = @" [N]ext item  [P]revious  [Enter] to select\n\n   Introduction\n > Credits\n   Hints\n";
    ZMenu *menu = [self detect:@[[self grid:[self text:plain]]] game:kGameIsGeneric format:@"hugo"];
    XCTAssertNotNil(menu);
    XCTAssertEqualObjects([self linesOf:menu], (@[@"Introduction", @"> Credits", @"Hints"]));
    XCTAssertEqual(menu.selectedLine, 1u);
    XCTAssertTrue([[self fullLineOf:menu] hasSuffix:@"Next Item: \"N\".\nPrevious: \"P\".\nTo Select: \"Enter\".\n"]);
}

- (void)testTads3 {
    NSString *plain = @"N=Next  P=Previous  Q=Quit\n\n> Introduction\n> Credits\n> Hints\n";
    NSAttributedString *text = [self text:plain highlight:@[@"> Credits"]];
    ZMenu *menu = [self detect:@[[self grid:text]] game:kGameIsGeneric format:@"tads3"];
    XCTAssertNotNil(menu);
    XCTAssertEqualObjects([self linesOf:menu], (@[@"> Introduction", @"> Credits", @"> Hints"]));
    XCTAssertEqual(menu.selectedLine, 1u);
    XCTAssertTrue([[self fullLineOf:menu] hasSuffix:@"Credits\n.\nMenu item 2 of 3.\nN: \"Next\".\nP: \"Previous\".\nQ: \"Quit\".\n"]);
}

// The '>' of unselected lines is hidden by drawing it in the background
// colour, so the selected line is the one where the two differ.
- (void)testTads3WithTwoItems {
    NSString *plain = @"N=Next  P=Previous\n\n> Introduction\n> Credits\n";
    NSMutableAttributedString *text = [[self text:plain] mutableCopy];
    [text addAttributes:@{ NSBackgroundColorAttributeName : NSColor.whiteColor, NSForegroundColorAttributeName : NSColor.whiteColor } range:NSMakeRange(0, text.length)];
    [text addAttribute:NSForegroundColorAttributeName value:NSColor.blackColor range:[plain rangeOfString:@"> Credits"]];
    ZMenu *menu = [self detect:@[[self grid:text]] game:kGameIsGeneric format:@"tads3"];
    XCTAssertNotNil(menu);
    XCTAssertEqualObjects([self linesOf:menu], (@[@"> Introduction", @"> Credits"]));
    XCTAssertEqual(menu.selectedLine, 1u);
}

// In TADS 3, a two-line menu with a highlighted line picks the other one.
- (void)testTads3WithTwoHighlightedItemsPicksTheOther {
    NSString *plain = @"N=Next  P=Previous\n\n> Introduction\n> Credits\n";
    ZMenu *menu = [self detect:@[[self grid:[self text:plain highlight:@[@"> Credits"]]]] game:kGameIsGeneric format:@"tads3"];
    XCTAssertNotNil(menu);
    XCTAssertEqual(menu.selectedLine, 0u);
}

// Bullets, no instructions, and three windows: a one-line title, the menu
// and a hidden one.
- (void)testThaumistry {
    NSString *plain = @"• Introduction\n• Credits\n• Hints\n";
    NSAttributedString *menuText = [self text:plain highlight:@[@"• Credits"]];
    NSArray *windows = @[[self grid:[self text:@"Charm's Way"]],
                         [self buffer:menuText],
                         [self window:wintype_TextBuffer text:[self text:@"hidden\n"] frame:NSMakeRect(0, 500, 600, 0)]];
    ZMenu *menu = [self detect:windows game:kGameIsThaumistry format:@"tads3"];
    XCTAssertNotNil(menu);
    XCTAssertEqualObjects([self linesOf:menu], (@[@"• Introduction", @"• Credits", @"• Hints"]));
    XCTAssertEqual(menu.selectedLine, 1u);
    XCTAssertEqualObjects([self fullLineOf:menu], @"We are in a menu, \"Charm's Way.\"\nCredits\n.\nMenu item 2 of 3.\n");
}

- (void)testThaumistryNeedsItsWindowLayout {
    NSString *plain = @"• Introduction\n• Credits\n• Hints\n";
    NSAttributedString *menuText = [self text:plain highlight:@[@"• Credits"]];
    NSArray *windows = @[[self grid:[self text:@"Charm's Way"]], [self buffer:menuText]];
    XCTAssertNil([self detect:windows game:kGameIsThaumistry format:@"tads3"]);
}

@end
