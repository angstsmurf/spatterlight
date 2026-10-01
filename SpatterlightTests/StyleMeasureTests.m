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
#import "Preferences+Appearance.h"

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

#pragma mark - Autorestore: measure must survive the archive round-trip

// Archive a window the way GlkController+Autorestore does (the GUI autosave
// archives the controller, windows included, with requiringSecureCoding:NO),
// and revive it the way restoreUI adopts one: assign glkctl and theme, keep
// the decoded styles/styleHints.
- (GlkWindow *)archiveWindow:(GlkWindow *)win reviveInto:(GlkController *)ctl {
    NSError *error = nil;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:win
                                         requiringSecureCoding:NO
                                                         error:&error];
    XCTAssertNil(error);
    XCTAssertNotNil(data);

    NSKeyedUnarchiver *unarchiver =
        [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
    XCTAssertNil(error);
    unarchiver.requiresSecureCoding = NO;
    GlkWindow *restored = [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
    [unarchiver finishDecoding];
    XCTAssertNotNil(restored);
    XCTAssertNotEqual(restored, win);

    // restoreUI's adoption of a restored window (GlkController+Autorestore.m).
    restored.glkctl = ctl;
    restored.theme = ctl.theme;
    return restored;
}

// Scarier's autorestore never re-publishes the game palette as stylehints:
// gsc_colour_set_normal_hints runs only on a "glk colour" toggle (or -c
// startup), and a restored session skips both.  gsc_colour_visible therefore
// depends on the ARCHIVED window's style table still answering measure with
// the palette after a relaunch, even though the new process's hint arrays
// are empty.  This is that dependency, pinned.
- (void)testMeasureSurvivesAutorestoreArchiveRoundTrip {
    GlkController *ctl = [self makeController];

    NSInteger hintFg = 0x123456, hintBg = 0x654321;
    ctl.bufferStyleHints[style_Normal][stylehint_TextColor] = @(hintFg);
    ctl.bufferStyleHints[style_Normal][stylehint_BackColor] = @(hintBg);

    GlkTextBufferWindow *win = [[GlkTextBufferWindow alloc] initWithGlkController:ctl name:1];
    XCTAssertEqual([self measureWindow:win controller:ctl
                                 style:style_Normal hint:stylehint_TextColor], hintFg);

    // The relaunched process: fresh controller, hint arrays all NSNull --
    // nothing has re-sent the palette.
    GlkController *relaunched = [self makeController];
    GlkWindow *restored = [self archiveWindow:win reviveInto:relaunched];

    XCTAssertEqual([self measureWindow:restored controller:relaunched
                                 style:style_Normal hint:stylehint_TextColor], hintFg,
                   @"an autorestored window must still measure the game palette");
    XCTAssertEqual([self measureWindow:restored controller:relaunched
                                 style:style_Normal hint:stylehint_BackColor], hintBg);
}

// The other direction: a session that ran with doStyles off archived
// theme-coloured styles, and a restored measure must keep reporting the
// theme -- so a restored gsc_colour_visible still says the palette is not
// visible, and scarier keeps drawing bar/map in the theme.
- (void)testMeasureAfterAutorestoreWithStylesDisabled {
    self.theme.doStyles = NO;

    GlkController *ctl = [self makeController];
    ctl.bufferStyleHints[style_Normal][stylehint_TextColor] = @(0x123456);
    ctl.bufferStyleHints[style_Normal][stylehint_BackColor] = @(0x654321);

    GlkTextBufferWindow *win = [[GlkTextBufferWindow alloc] initWithGlkController:ctl name:1];

    GlkController *relaunched = [self makeController];
    GlkWindow *restored = [self archiveWindow:win reviveInto:relaunched];

    NSInteger expectedFg = [self themeColorForBufferStyle:self.theme.bufferNormal
                                                     hint:stylehint_TextColor];
    XCTAssertEqual([self measureWindow:restored controller:relaunched
                                 style:style_Normal hint:stylehint_TextColor], expectedFg,
                   @"with doStyles off, a restored measure must still report the theme");
    XCTAssertNotEqual(expectedFg, 0x123456);
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

#pragma mark - Font trait measures (Weight / Oblique / Proportional / Size)

- (NSInteger)fontTraitValueForStyle:(GlkStyle *)style hint:(NSUInteger)hint {
    NSFont *font = style.attributeDict[NSFontAttributeName];
    XCTAssertNotNil(font);
    NSFontTraitMask traits = [[NSFontManager sharedFontManager] traitsOfFont:font];
    switch (hint) {
        case stylehint_Weight: {
            NSInteger weight = [[NSFontManager sharedFontManager] weightOfFont:font];
            if ((traits & NSBoldFontMask) || weight > 5)
                return 1;
            return weight < 5 ? -1 : 0;
        }
        case stylehint_Oblique:
            return (traits & NSItalicFontMask) ? 1 : 0;
        case stylehint_Proportional:
            return ((traits & NSFixedPitchFontMask) || font.isFixedPitch) ? 0 : 1;
        case stylehint_Size:
            return (NSInteger)llround(font.pointSize);
        default:
            XCTFail(@"unexpected hint %lu", (unsigned long)hint);
            return -1;
    }
}

// Theme-backed font traits must measure without a game stylehint (the gap the
// stylemeasure.ulx probe hit: weight/oblique/proportional used to FAIL).
- (void)testBufferMeasureFontTraitsFromTheme {
    GlkController *ctl = [self makeController];
    GlkTextBufferWindow *win = [[GlkTextBufferWindow alloc] initWithGlkController:ctl name:1];

    XCTAssertEqual([self measureWindow:win controller:ctl style:style_Normal hint:stylehint_Weight],
                   [self fontTraitValueForStyle:self.theme.bufferNormal hint:stylehint_Weight]);
    XCTAssertEqual([self measureWindow:win controller:ctl style:style_Normal hint:stylehint_Oblique],
                   [self fontTraitValueForStyle:self.theme.bufferNormal hint:stylehint_Oblique]);
    XCTAssertEqual([self measureWindow:win controller:ctl style:style_Normal hint:stylehint_Proportional],
                   [self fontTraitValueForStyle:self.theme.bufferNormal hint:stylehint_Proportional]);
    XCTAssertEqual([self measureWindow:win controller:ctl style:style_Normal hint:stylehint_Size],
                   [self fontTraitValueForStyle:self.theme.bufferNormal hint:stylehint_Size]);
    XCTAssertGreaterThan([self measureWindow:win controller:ctl style:style_Normal hint:stylehint_Size], 1,
                         @"Size must be a real point/CSS-px size, not the old stub value 1");

    // Preformatted is fixed-width in the default theme.
    XCTAssertEqual([self measureWindow:win controller:ctl style:style_Preformatted hint:stylehint_Proportional],
                   [self fontTraitValueForStyle:self.theme.bufPre hint:stylehint_Proportional]);
    XCTAssertEqual([self measureWindow:win controller:ctl style:style_Preformatted hint:stylehint_Proportional], 0);
}

// A theme font lighter than regular (Light, Thin, UltraLight) measures as the
// spec's -1, not as 0; regular stays 0 and bold stays 1.
- (void)testMeasureReportsLightFontAsMinusOne {
    NSFont *light = [NSFont fontWithName:@"HelveticaNeue-Light"
                                    size:self.theme.bufUsr1.font.pointSize];
    XCTAssertNotNil(light, @"fixture: HelveticaNeue-Light ships with macOS");
    self.theme.bufUsr1.font = light;

    GlkController *ctl = [self makeController];
    GlkTextBufferWindow *win = [[GlkTextBufferWindow alloc] initWithGlkController:ctl name:1];

    XCTAssertEqual([self measureWindow:win controller:ctl style:style_User1 hint:stylehint_Weight], -1);
    XCTAssertEqual([self measureWindow:win controller:ctl style:style_Normal hint:stylehint_Weight], 0);
    XCTAssertEqual([self measureWindow:win controller:ctl style:style_Input hint:stylehint_Weight], 1,
                   @"fixture: the default theme's Input style is bold");
}

// Families whose next step up from regular is Medium (Helvetica Neue, Avenir
// Next) never produce a bold-trait face for a Weight 1 hint; the measure must
// still report 1 for the heavier face the hint produced, and for a theme
// font that is Medium outright.
- (void)testMeasureReportsMediumWeightAsBold {
    CGFloat size = self.theme.bufUsr1.font.pointSize;
    NSFont *regular = [NSFont fontWithName:@"HelveticaNeue" size:size];
    NSFont *medium = [NSFont fontWithName:@"HelveticaNeue-Medium" size:size];
    XCTAssertNotNil(regular);
    XCTAssertNotNil(medium);
    XCTAssertFalse([[NSFontManager sharedFontManager] traitsOfFont:medium] & NSBoldFontMask,
                   @"fixture: Medium must not carry the bold trait, or the test proves nothing");

    self.theme.bufUsr1.font = regular;
    self.theme.bufUsr2.font = medium;

    GlkController *ctl = [self makeController];
    ctl.bufferStyleHints[style_User1][stylehint_Weight] = @(1);

    GlkTextBufferWindow *win = [[GlkTextBufferWindow alloc] initWithGlkController:ctl name:1];

    XCTAssertEqual([self measureWindow:win controller:ctl style:style_User1 hint:stylehint_Weight], 1,
                   @"Weight 1 hint on Helvetica Neue yields Medium, which must measure as bold");
    XCTAssertEqual([self measureWindow:win controller:ctl style:style_User2 hint:stylehint_Weight], 1,
                   @"a Medium theme font must measure as bold");
}

// A Size stylehint (±2 pt per step for buffers) must show through absolute measure.
- (void)testMeasureReflectsSizeHintWhenStylesEnabled {
    GlkController *ctl = [self makeController];
    ctl.bufferStyleHints[style_User1][stylehint_Size] = @(1);

    GlkTextBufferWindow *win = [[GlkTextBufferWindow alloc] initWithGlkController:ctl name:1];

    NSInteger base = [self fontTraitValueForStyle:self.theme.bufUsr1 hint:stylehint_Size];
    NSInteger measured = [self measureWindow:win controller:ctl style:style_User1 hint:stylehint_Size];
    XCTAssertEqual(measured, base + 2);
}
// With doStyles on, a Weight hint applied before the window opens must show
// through measure via the style table font (not only via getStyleVal).
- (void)testMeasureReflectsWeightHintWhenStylesEnabled {
    GlkController *ctl = [self makeController];
    ctl.bufferStyleHints[style_User1][stylehint_Weight] = @(1);

    GlkTextBufferWindow *win = [[GlkTextBufferWindow alloc] initWithGlkController:ctl name:1];

    XCTAssertEqual([self measureWindow:win controller:ctl style:style_User1 hint:stylehint_Weight], 1);
}

// With doStyles off, an unused Weight hint must not show through: measure the
// theme font, matching the colour-hint contract.
- (void)testMeasureIgnoresWeightHintWhenStylesDisabled {
    self.theme.doStyles = NO;

    GlkController *ctl = [self makeController];
    ctl.bufferStyleHints[style_User1][stylehint_Weight] = @(1);

    GlkTextBufferWindow *win = [[GlkTextBufferWindow alloc] initWithGlkController:ctl name:1];

    NSInteger expected = [self fontTraitValueForStyle:self.theme.bufUsr1 hint:stylehint_Weight];
    XCTAssertEqual([self measureWindow:win controller:ctl style:style_User1 hint:stylehint_Weight], expected);
    XCTAssertNotEqual(expected, 1,
                      @"fixture: theme User1 should not already be bold, or the test proves nothing");
}

@end

#pragma mark - Light and dark theme sides

// Not declared in BuiltInThemes.h.
@interface BuiltInThemes (ThemeSideTesting)
+ (Theme *)createDOSThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force;
+ (Theme *)createLectroteDarkThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force;
+ (Theme *)createGargoyleThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force;
@end

// The light and dark sides of a theme (Theme.m), the light/dark override
// rule (Preferences+Appearance.m) and the one-time move to theme sides.
@interface ThemeSideTests : XCTestCase

@property (nonatomic, strong) NSManagedObjectContext *context;

@end

@implementation ThemeSideTests

- (void)setUp {
    [super setUp];
    // In memory: the built-in themes are rebuilt here.
    NSURL *modelURL = [[NSBundle bundleForClass:[CoreDataManager class]] URLForResource:@"Spatterlight" withExtension:@"momd"];
    NSManagedObjectModel *model = [[NSManagedObjectModel alloc] initWithContentsOfURL:modelURL];
    NSPersistentStoreCoordinator *coordinator = [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:model];
    NSError *error = nil;
    [coordinator addPersistentStoreWithType:NSInMemoryStoreType configuration:nil URL:nil options:nil error:&error];
    XCTAssertNil(error);
    self.context = [[NSManagedObjectContext alloc] initWithConcurrencyType:NSMainQueueConcurrencyType];
    self.context.persistentStoreCoordinator = coordinator;
}

- (void)tearDown {
    self.context = nil;
    [super tearDown];
}

// Default, showing its light side whatever the machine is set to.
- (Theme *)automodeTheme {
    Theme *theme = [BuiltInThemes createDefaultThemeInContext:self.context forceRebuild:YES];
    XCTAssertNotNil(theme);
    [theme activateSideForDark:NO];
    return theme;
}

- (void)testBuiltInThemeHasSeparateSides {
    Theme *theme = [self automodeTheme];
    XCTAssertTrue(theme.hasSeparateSides);
    XCTAssertFalse(theme.sideIsDark);
    XCTAssertFalse(theme.sidesAreIdentical);
    XCTAssertTrue([theme.bufferBackground isEqualToColor:[NSColor whiteColor]]);
}

- (void)testActivateSideSwapsAndSwapsBack {
    Theme *theme = [self automodeTheme];
    NSInteger lightBackground = theme.bufferBackground.integerColor;
    NSInteger lightText = theme.bufferNormal.color.integerColor;

    [theme activateSideForDark:YES];
    XCTAssertTrue(theme.sideIsDark);
    XCTAssertTrue([theme.bufferBackground isEqualToColor:[NSColor blackColor]]);
    XCTAssertNotEqual(theme.bufferNormal.color.integerColor, lightText);

    // Asking for the side already showing changes nothing.
    NSData *stored = theme.inactiveSideData;
    [theme activateSideForDark:YES];
    XCTAssertEqualObjects(theme.inactiveSideData, stored);

    [theme activateSideForDark:NO];
    XCTAssertFalse(theme.sideIsDark);
    XCTAssertEqual(theme.bufferBackground.integerColor, lightBackground);
    XCTAssertEqual(theme.bufferNormal.color.integerColor, lightText);
}

// Editing the side that is showing leaves the other one alone. (This is what
// a font panel or color well edit in Preferences does.)
- (void)testEditingOneSideKeepsTheOther {
    Theme *theme = [self automodeTheme];
    [theme activateSideForDark:YES];
    NSColor *red = [NSColor colorWithSRGBRed:0.8 green:0.1 blue:0.1 alpha:1];
    theme.bufferNormal.color = red;

    [theme activateSideForDark:NO];
    XCTAssertFalse([theme.bufferNormal.color isEqualToColor:red]);

    [theme activateSideForDark:YES];
    XCTAssertTrue([theme.bufferNormal.color isEqualToColor:red]);
}

- (void)testActivateSidesInContextSkipsSingleSidedThemes {
    Theme *automode = [self automodeTheme];
    Theme *plain = [BuiltInThemes createDOSThemeInContext:self.context forceRebuild:YES];
    XCTAssertFalse(plain.hasSeparateSides);
    NSInteger plainBackground = plain.bufferBackground.integerColor;

    [Theme activateSidesForDark:YES inContext:self.context];
    XCTAssertTrue(automode.sideIsDark);
    XCTAssertTrue([automode.bufferBackground isEqualToColor:[NSColor blackColor]]);
    XCTAssertEqual(plain.bufferBackground.integerColor, plainBackground);
    XCTAssertFalse(plain.hasSeparateSides);
}

- (void)testSeparateAndDiscardSides {
    Theme *theme = [BuiltInThemes createDOSThemeInContext:self.context forceRebuild:YES];
    [theme separateSidesWithActiveDark:NO];
    XCTAssertTrue(theme.hasSeparateSides);
    XCTAssertTrue(theme.sidesAreIdentical);

    theme.bufferBackground = [NSColor whiteColor];
    XCTAssertFalse(theme.sidesAreIdentical);

    [theme discardInactiveSide];
    XCTAssertFalse(theme.hasSeparateSides);
    XCTAssertTrue(theme.sidesAreIdentical);
    XCTAssertTrue([theme.bufferBackground isEqualToColor:[NSColor whiteColor]]);
}

// The inactive side is archived; style attributes must come back intact.
- (void)testInactiveSideRoundTrips {
    Theme *theme = [self automodeTheme];
    NSDictionary *snapshot = [theme sideSnapshot];
    [theme setInactiveSide:snapshot activeIsDark:NO];
    NSDictionary *back = theme.inactiveSide;
    XCTAssertNotNil(back);
    NSColor *before = snapshot[@"styles"][@"bufferNormal"][@"attributeDict"][NSForegroundColorAttributeName];
    NSColor *after = back[@"styles"][@"bufferNormal"][@"attributeDict"][NSForegroundColorAttributeName];
    XCTAssertTrue([after isEqualToColor:before]);
    XCTAssertTrue([back[@"theme"][@"bufferBackground"] isEqualToColor:snapshot[@"theme"][@"bufferBackground"]]);
}

#pragma mark Game colors on the dark side

- (void)testBlackGameTextGetsTheLightBackgroundOnTheDarkSide {
    Theme *theme = [self automodeTheme];
    [theme activateSideForDark:YES];
    NSMutableDictionary *attributes = [@{ NSForegroundColorAttributeName: [NSColor blackColor] } mutableCopy];
    [theme fitGameColors:attributes gameForeground:YES gameBackground:NO lightStyle:nil grid:NO];
    XCTAssertTrue([attributes[NSBackgroundColorAttributeName] isEqualToColor:[NSColor whiteColor]]);
}

- (void)testReadableOrFullySetGameColorsAreLeftAlone {
    Theme *theme = [self automodeTheme];
    [theme activateSideForDark:YES];

    // Light text on the dark side already reads.
    NSMutableDictionary *readable = [@{ NSForegroundColorAttributeName: [NSColor yellowColor] } mutableCopy];
    [theme fitGameColors:readable gameForeground:YES gameBackground:NO lightStyle:nil grid:NO];
    XCTAssertNil(readable[NSBackgroundColorAttributeName]);

    // A game that set both colors chose the pair itself.
    NSMutableDictionary *both = [@{ NSForegroundColorAttributeName: [NSColor blackColor],
                                    NSBackgroundColorAttributeName: [NSColor blackColor] } mutableCopy];
    [theme fitGameColors:both gameForeground:YES gameBackground:YES lightStyle:nil grid:NO];
    XCTAssertTrue([both[NSForegroundColorAttributeName] isEqualToColor:[NSColor blackColor]]);
}

- (void)testSingleSidedThemeHasNoLightSideToFallBackOn {
    Theme *theme = [BuiltInThemes createDOSThemeInContext:self.context forceRebuild:YES];
    XCTAssertTrue([theme.bufferBackground isEqualToColor:[NSColor blackColor]]);
    NSMutableDictionary *attributes = [@{ NSForegroundColorAttributeName: [NSColor blackColor] } mutableCopy];
    [theme fitGameColors:attributes gameForeground:YES gameBackground:NO lightStyle:nil grid:NO];
    // The theme's own background is the only one there is, and it reads no
    // better, so the game's color is left as it is.
    XCTAssertNil(attributes[NSBackgroundColorAttributeName]);
    XCTAssertTrue([attributes[NSForegroundColorAttributeName] isEqualToColor:[NSColor blackColor]]);
}

// MS-DOS, DOSBox and Lectrote Dark are dark on both sides.
- (void)testDarkBuiltInThemesAreSingleSided {
    XCTAssertFalse([BuiltInThemes createDOSThemeInContext:self.context forceRebuild:YES].hasSeparateSides);
    XCTAssertFalse([BuiltInThemes createLectroteDarkThemeInContext:self.context forceRebuild:YES].hasSeparateSides);
}

// Rebuilding a theme while its dark side shows gives it fresh sides.
- (void)testRebuildingOnTheDarkSideKeepsBothSides {
    Theme *theme = [self automodeTheme];
    [theme activateSideForDark:YES];
    theme.bufferBackground = [NSColor redColor];
    [BuiltInThemes createDefaultThemeInContext:self.context forceRebuild:YES];
    [theme activateSideForDark:YES];
    XCTAssertTrue([theme.bufferBackground isEqualToColor:[NSColor blackColor]]);
    [theme activateSideForDark:NO];
    XCTAssertTrue([theme.bufferBackground isEqualToColor:[NSColor whiteColor]]);
}

#pragma mark The light/dark override

- (void)testOverrideIsDroppedOnceTheSystemMatchesIt {
    XCTAssertNil([Preferences override:@"dark" keptForSystemAppearance:kDarkAppearance]);
    XCTAssertNil([Preferences override:@"light" keptForSystemAppearance:kLightAppearance]);
    XCTAssertEqualObjects([Preferences override:@"dark" keptForSystemAppearance:kLightAppearance], @"dark");
    XCTAssertEqualObjects([Preferences override:@"light" keptForSystemAppearance:kDarkAppearance], @"light");
    XCTAssertNil([Preferences override:nil keptForSystemAppearance:kDarkAppearance]);
}

#pragma mark The move to theme sides

- (void)testMigrationToThemeSidesRunsOnce {
    NSString *key = @"SpatterlightThemeSidesV1";
    NSString *key2 = @"SpatterlightThemeSidesV2";
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    id saved = [defaults objectForKey:key];
    id saved2 = [defaults objectForKey:key2];
    [defaults removeObjectForKey:key];
    [defaults setBool:YES forKey:key2];

    Theme *lectroteDark = (Theme *)[NSEntityDescription insertNewObjectForEntityForName:@"Theme" inManagedObjectContext:self.context];
    lectroteDark.name = @"Lectrote Dark";
    lectroteDark.editable = YES;
    lectroteDark.defaultParent = [BuiltInThemes createDefaultThemeInContext:self.context forceRebuild:YES];

    XCTAssertTrue([Preferences migrateToThemeSidesIfNeededInContext:self.context]);
    XCTAssertFalse(lectroteDark.editable);
    XCTAssertNil(lectroteDark.defaultParent);

    lectroteDark.editable = YES;
    XCTAssertFalse([Preferences migrateToThemeSidesIfNeededInContext:self.context]);
    XCTAssertTrue(lectroteDark.editable);

    if (saved)
        [defaults setObject:saved forKey:key];
    else
        [defaults removeObjectForKey:key];
    if (saved2)
        [defaults setObject:saved2 forKey:key2];
    else
        [defaults removeObjectForKey:key2];
}

- (void)testAutomodeThemesAreReplacedByTheirNamesakes {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSArray<NSString *> *keys = @[ @"SpatterlightThemeSidesV1", @"SpatterlightThemeSidesV2", @"themeName" ];
    NSMutableDictionary *saved = [NSMutableDictionary new];
    for (NSString *key in keys) {
        id value = [defaults objectForKey:key];
        if (value)
            saved[key] = value;
    }
    [defaults setBool:YES forKey:@"SpatterlightThemeSidesV1"];
    [defaults removeObjectForKey:@"SpatterlightThemeSidesV2"];
    [defaults setObject:@"Gargoyle automode" forKey:@"themeName"];

    Theme *gargoyle = [BuiltInThemes createGargoyleThemeInContext:self.context forceRebuild:YES];
    Theme *automode = (Theme *)[NSEntityDescription insertNewObjectForEntityForName:@"Theme" inManagedObjectContext:self.context];
    automode.name = @"Gargoyle automode";
    automode.editable = NO;
    Theme *child = (Theme *)[NSEntityDescription insertNewObjectForEntityForName:@"Theme" inManagedObjectContext:self.context];
    child.name = @"Gargoyle automode (modified)";
    child.editable = YES;
    child.defaultParent = automode;

    XCTAssertTrue([Preferences migrateToThemeSidesIfNeededInContext:self.context]);
    NSFetchRequest *request = [Theme fetchRequest];
    request.predicate = [NSPredicate predicateWithFormat:@"name == %@", @"Gargoyle automode"];
    XCTAssertEqual([self.context countForFetchRequest:request error:nil], 0);
    XCTAssertEqual(child.defaultParent, gargoyle);
    XCTAssertEqualObjects([defaults stringForKey:@"themeName"], @"Gargoyle");
    XCTAssertFalse([Preferences migrateToThemeSidesIfNeededInContext:self.context]);

    for (NSString *key in keys) {
        if (saved[key])
            [defaults setObject:saved[key] forKey:key];
        else
            [defaults removeObjectForKey:key];
    }
}

@end
