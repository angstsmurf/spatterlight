//
//  StyleMeasureTests.m
//  SpatterlightTests
//
//  Pins the app-side glk_style_measure semantics (issue #151 / PR #152) for
//  the one interpreter that mixes zcolors with glk_style_measure: bocfel.
//
//  Bocfel's Journey code measures style_Normal while zcolors may be active on
//  the measured window:
//    - screen.cpp measures the main buffer window's TextColor to fill the
//      graphics border;
//    - z6/journey.cpp measures a text grid's BackColor and passes it to
//      win_setbgnd.
//  A measure that folds in the live zcolor (or reverse video) makes those
//  answers depend on whatever colour happened to be in force at redraw time.
//  The contract tested here is Gargoyle's: glk_style_measure reports the
//  style table only -- theme attributes, plus game stylehints when the
//  "Games can set colors and styles" preference (theme.doStyles) is on --
//  never the transient zcolor/reverse state.  Scarier's colour-mode
//  detection (set a Normal colour hint, measure it back) relies on the same
//  contract, in both directions of the doStyles switch.
//

#import <XCTest/XCTest.h>
#import <CoreData/CoreData.h>

#import "GlkController.h"
#import "GlkWindow.h"
#import "GlkTextBufferWindow.h"
#import "GlkTextGridWindow.h"
#import "Theme.h"
#import "GlkStyle.h"
#import "BuiltInThemes.h"
#import "CoreDataManager.h"
#import "NSColor+integer.h"

#include "glk.h"

// handleStyleMeasureOnWin is not declared in GlkController+GlkRequests.h;
// it is the private worker behind the STYLEMEASURE protocol request.
@interface GlkController (StyleMeasureTesting)
- (BOOL)handleStyleMeasureOnWin:(GlkWindow *)gwindow
                          style:(NSUInteger)style
                           hint:(NSUInteger)hint
                         result:(NSInteger *)result;
@end

@interface StyleMeasureTests : XCTestCase

@property (nonatomic, strong) NSManagedObjectContext *context;
@property (nonatomic, strong) Theme *theme;

@end

@implementation StyleMeasureTests

- (void)setUp {
    [super setUp];

    // Always an in-memory store: createDefaultThemeInContext with
    // forceRebuild would rewrite the real library's Default theme.
    NSURL *modelURL = [[NSBundle bundleForClass:[CoreDataManager class]] URLForResource:@"Spatterlight" withExtension:@"momd"];
    NSManagedObjectModel *model = [[NSManagedObjectModel alloc] initWithContentsOfURL:modelURL];
    NSPersistentStoreCoordinator *coordinator = [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:model];
    NSError *error = nil;
    [coordinator addPersistentStoreWithType:NSInMemoryStoreType
                              configuration:nil
                                        URL:nil
                                    options:nil
                                      error:&error];
    XCTAssertNil(error);
    self.context = [[NSManagedObjectContext alloc] initWithConcurrencyType:NSMainQueueConcurrencyType];
    self.context.persistentStoreCoordinator = coordinator;

    self.theme = [BuiltInThemes createDefaultThemeInContext:self.context forceRebuild:YES];
    XCTAssertNotNil(self.theme);
    self.theme.doStyles = YES;
}

- (void)tearDown {
    self.theme = nil;
    self.context = nil;
    [super tearDown];
}

#pragma mark - Helpers

// A GlkController wired the way runTerp wires one, minus the interpreter
// process: theme set, and empty style_NUMSTYLES x stylehint_NUMHINTS hint
// matrices for both window types.
- (GlkController *)makeController {
    GlkController *ctl = [[GlkController alloc] init];
    ctl.theme = self.theme;

    NSMutableArray *nullarray = [NSMutableArray arrayWithCapacity:stylehint_NUMHINTS];
    for (NSInteger i = 0; i < stylehint_NUMHINTS; i++)
        [nullarray addObject:[NSNull null]];

    ctl.gridStyleHints = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
    ctl.bufferStyleHints = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
    for (NSInteger i = 0; i < style_NUMSTYLES; i++) {
        [ctl.gridStyleHints addObject:[nullarray mutableCopy]];
        [ctl.bufferStyleHints addObject:[nullarray mutableCopy]];
    }
    return ctl;
}

- (NSInteger)measureWindow:(GlkWindow *)win
                controller:(GlkController *)ctl
                     style:(NSUInteger)style
                      hint:(NSUInteger)hint {
    NSInteger result = -1;
    BOOL ok = [ctl handleStyleMeasureOnWin:win style:style hint:hint result:&result];
    XCTAssertTrue(ok, @"measure of style %lu hint %lu should succeed", style, hint);
    return result;
}

// The colour the style table itself holds for a style, with the same
// missing-background fallback handleStyleMeasureOnWin uses.
- (NSInteger)themeColorForBufferStyle:(GlkStyle *)style hint:(NSUInteger)hint {
    NSColor *color;
    if (hint == stylehint_TextColor)
        color = style.attributeDict[NSForegroundColorAttributeName];
    else
        color = style.attributeDict[NSBackgroundColorAttributeName] ?: self.theme.bufferBackground;
    XCTAssertNotNil(color);
    return color.integerColor;
}

- (NSInteger)themeColorForGridStyle:(GlkStyle *)style hint:(NSUInteger)hint {
    NSColor *color;
    if (hint == stylehint_TextColor)
        color = style.attributeDict[NSForegroundColorAttributeName];
    else
        color = style.attributeDict[NSBackgroundColorAttributeName] ?: self.theme.gridBackground;
    XCTAssertNotNil(color);
    return color.integerColor;
}

#pragma mark - Bocfel scenarios: zcolors active while measuring

// bocfel screen.cpp (Journey border): measure style_Normal TextColor on the
// main buffer window.  A Z-machine game has set colours, so a zcolor is in
// force on that window when the border is redrawn.  The border must get the
// style-table colour, not the transient zcolor.
- (void)testBufferMeasureTextColorIgnoresActiveZColor {
    GlkController *ctl = [self makeController];
    GlkTextBufferWindow *win = [[GlkTextBufferWindow alloc] initWithGlkController:ctl name:1];

    NSInteger zfg = 0x345678, zbg = 0x9abcde;
    [win setZColorText:zfg background:zbg];

    NSInteger expected = [self themeColorForBufferStyle:self.theme.bufferNormal
                                                   hint:stylehint_TextColor];
    XCTAssertNotEqual(expected, zfg, @"fixture colour must differ from the theme for the test to prove anything");

    NSInteger measured = [self measureWindow:win controller:ctl
                                       style:style_Normal hint:stylehint_TextColor];
    XCTAssertEqual(measured, expected,
                   @"measure must report the style table, not the active zcolor");
    XCTAssertNotEqual(measured, zfg);
}

// bocfel z6/journey.cpp: measure style_Normal BackColor on a text grid and
// hand it to win_setbgnd.  Same contract, grid flavour.
- (void)testGridMeasureBackColorIgnoresActiveZColor {
    GlkController *ctl = [self makeController];
    GlkTextGridWindow *win = [[GlkTextGridWindow alloc] initWithGlkController:ctl name:2];

    NSInteger zfg = 0x345678, zbg = 0x9abcde;
    [win setZColorText:zfg background:zbg];

    NSInteger expected = [self themeColorForGridStyle:self.theme.gridNormal
                                                 hint:stylehint_BackColor];
    XCTAssertNotEqual(expected, zbg);

    NSInteger measured = [self measureWindow:win controller:ctl
                                       style:style_Normal hint:stylehint_BackColor];
    XCTAssertEqual(measured, expected,
                   @"measure must report the style table, not the active zcolor");
    XCTAssertNotEqual(measured, zbg);
}

// A V6 game can have reverse video in force when a redraw measures.  Reverse
// swaps fg/bg on painted text; measure must not report the swap.
- (void)testBufferMeasureIgnoresReverseVideo {
    GlkController *ctl = [self makeController];
    GlkTextBufferWindow *win = [[GlkTextBufferWindow alloc] initWithGlkController:ctl name:1];

    win.currentReverseVideo = YES;

    NSInteger expectedFg = [self themeColorForBufferStyle:self.theme.bufferNormal
                                                     hint:stylehint_TextColor];
    NSInteger expectedBg = [self themeColorForBufferStyle:self.theme.bufferNormal
                                                     hint:stylehint_BackColor];
    XCTAssertNotEqual(expectedFg, expectedBg,
                      @"theme fg and bg must differ for the test to prove anything");

    NSInteger measured = [self measureWindow:win controller:ctl
                                       style:style_Normal hint:stylehint_TextColor];
    XCTAssertEqual(measured, expectedFg,
                   @"measure must report the style table, not reverse-video state");
}

#pragma mark - Stylehints and the doStyles preference

// With "Games can set colors and styles" on, a colour stylehint set before
// the window opens must be readable back through measure, exactly.  This is
// the round-trip scarier's colour-mode detection performs.
- (void)testMeasureReflectsColourHintsWhenStylesEnabled {
    GlkController *ctl = [self makeController];

    NSInteger hintFg = 0x123456, hintBg = 0x654321;
    ctl.bufferStyleHints[style_Normal][stylehint_TextColor] = @(hintFg);
    ctl.bufferStyleHints[style_Normal][stylehint_BackColor] = @(hintBg);

    GlkTextBufferWindow *win = [[GlkTextBufferWindow alloc] initWithGlkController:ctl name:1];

    XCTAssertEqual([self measureWindow:win controller:ctl
                                 style:style_Normal hint:stylehint_TextColor], hintFg,
                   @"a published colour hint must measure back exactly");
    XCTAssertEqual([self measureWindow:win controller:ctl
                                 style:style_Normal hint:stylehint_BackColor], hintBg);
}

// With the preference off, the same hints must NOT show through measure: the
// window is painted in the theme, and measure has to agree with the paint.
// (This is what lets an interpreter detect that its colours are ignored --
// issue #151.)
- (void)testMeasureIgnoresColourHintsWhenStylesDisabled {
    self.theme.doStyles = NO;

    GlkController *ctl = [self makeController];
    ctl.bufferStyleHints[style_Normal][stylehint_TextColor] = @(0x123456);
    ctl.bufferStyleHints[style_Normal][stylehint_BackColor] = @(0x654321);

    GlkTextBufferWindow *win = [[GlkTextBufferWindow alloc] initWithGlkController:ctl name:1];

    NSInteger expectedFg = [self themeColorForBufferStyle:self.theme.bufferNormal
                                                     hint:stylehint_TextColor];
    NSInteger expectedBg = [self themeColorForBufferStyle:self.theme.bufferNormal
                                                     hint:stylehint_BackColor];

    XCTAssertEqual([self measureWindow:win controller:ctl
                                 style:style_Normal hint:stylehint_TextColor], expectedFg,
                   @"with doStyles off, measure must report the theme, not the hint");
    XCTAssertEqual([self measureWindow:win controller:ctl
                                 style:style_Normal hint:stylehint_BackColor], expectedBg);
    XCTAssertNotEqual(expectedFg, 0x123456);
    XCTAssertNotEqual(expectedBg, 0x654321);
}

// With doStyles off AND a zcolor active (a colour-using Z-machine game played
// with the preference unchecked), measure still reports the theme.
- (void)testMeasureWithZColorAndStylesDisabled {
    self.theme.doStyles = NO;

    GlkController *ctl = [self makeController];
    GlkTextBufferWindow *win = [[GlkTextBufferWindow alloc] initWithGlkController:ctl name:1];
    [win setZColorText:0x345678 background:0x9abcde];

    NSInteger expected = [self themeColorForBufferStyle:self.theme.bufferNormal
                                                   hint:stylehint_TextColor];
    XCTAssertEqual([self measureWindow:win controller:ctl
                                 style:style_Normal hint:stylehint_TextColor], expected);
}

// The BackColor answer when the style itself carries no background: the
// window background fills in (bocfel's journey.cpp feeds this straight to
// win_setbgnd, so a nil here would paint the surround black).
- (void)testMeasureBackColorFallsBackToWindowBackground {
    GlkController *ctl = [self makeController];
    GlkTextBufferWindow *win = [[GlkTextBufferWindow alloc] initWithGlkController:ctl name:1];

    NSInteger measured = [self measureWindow:win controller:ctl
                                       style:style_Normal hint:stylehint_BackColor];
    NSInteger expected = [self themeColorForBufferStyle:self.theme.bufferNormal
                                                   hint:stylehint_BackColor];
    XCTAssertEqual(measured, expected);
}

@end
