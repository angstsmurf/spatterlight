//
//  SettingsUITests.m
//  UITests
//
//  Tests for the Settings window. They run against a separate library,
//  UITests.storedata, so they never touch the user's games or themes. They
//  do share the preferences of the debug build, so they put back the ones
//  they change, like the Dark mode setting.
//

#import <XCTest/XCTest.h>

@interface SettingsUITests : XCTestCase
@end

@implementation SettingsUITests

- (void)setUp {
    self.continueAfterFailure = NO;
}

- (BOOL)waitForElement:(XCUIElement *)element toExistWithTimeout:(NSTimeInterval)timeout {
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"exists == true"];
    XCTNSPredicateExpectation *expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:element];
    return [XCTWaiter waitForExpectations:@[expectation] timeout:timeout] == XCTWaiterResultCompleted;
}

// Attaches the screenshot to the test result. Like other attachments, it is
// kept only when the test fails, unless the scheme says to keep them all.
- (void)attachScreenshot:(XCUIScreenshot *)screenshot named:(NSString *)name {
    XCTAttachment *attachment = [XCTAttachment attachmentWithScreenshot:screenshot];
    attachment.name = name;
    [self addAttachment:attachment];
}

- (void)launch:(XCUIApplication *)app {
    app.launchArguments = @[ @"-SpatterlightStoreFileName", @"UITests.storedata",
                             @"-ApplePersistenceIgnoreState", @"YES",
                             @"-AutorestoreAlertSuppression", @"YES",
                             @"-AlwaysAutorestore", @"NO" ];
    [app launch];
    [app activate];
}

- (XCUIElement *)openSettings:(XCUIApplication *)app {
    [app typeKey:@"," modifierFlags:XCUIKeyModifierCommand];
    XCUIElement *settings = app.dialogs[@"preferences"];
    XCTAssert([self waitForElement:settings toExistWithTimeout:10], @"The Settings window did not open");
    return settings;
}

- (XCUIElement *)launchAndOpenSettings:(XCUIApplication *)app {
    [self launch:app];
    return [self openSettings:app];
}

- (void)showTab:(NSString *)name inSettings:(XCUIElement *)settings {
    XCUIElement *tab = settings.toolbars.buttons[name];
    XCTAssert([self waitForElement:tab toExistWithTimeout:5], @"No %@ tab", name);
    [tab click];
    [NSThread sleepForTimeInterval:0.7];
}

- (void)showStylesTab:(XCUIElement *)settings {
    [self showTab:@"Glk Styles" inSettings:settings];
    XCTAssert([self waitForElement:settings.buttons[@"Un-customize styles"] toExistWithTimeout:5], @"The Glk Styles tab did not appear");
}

- (NSString *)themeNameOnStylesTab:(XCUIElement *)settings {
    NSString *prefix = @"Settings for theme ";
    XCUIElement *header = [settings.staticTexts matchingPredicate:[NSPredicate predicateWithFormat:@"value BEGINSWITH %@", prefix]].firstMatch;
    XCTAssert([self waitForElement:header toExistWithTimeout:5], @"No theme header on the Glk Styles tab");
    return [header.value substringFromIndex:prefix.length];
}

- (void)selectTheme:(NSString *)name inSettings:(XCUIElement *)settings {
    [self showTab:@"Themes" inSettings:settings];
    XCUIElement *table = settings.tables.firstMatch;
    // Built-in themes are static text, and editable ones text fields.
    XCUIElement *row = [table.tableRows containingPredicate:[NSPredicate predicateWithFormat:@"value == %@", name]].firstMatch;
    XCTAssert([self waitForElement:table toExistWithTimeout:5]);
    XCTAssert(row.exists, @"No theme named %@", name);
    if (row.hittable) {
        [row click];
    } else {
        // Scroll rows below the visible part into view. Rows with editable
        // names report that they are not hittable even then.
        XCUIElement *scrollView = [settings.scrollViews containingType:XCUIElementTypeTable identifier:nil].firstMatch;
        NSRect visible = scrollView.exists ? scrollView.frame : table.frame;
        for (NSInteger i = 0; i < 10 && NSMaxY(row.frame) > NSMaxY(visible); i++)
            [scrollView scrollByDeltaX:0 deltaY:-60];
        [[row coordinateWithNormalizedOffset:CGVectorMake(0.9, 0.5)] click];
        if (!row.isSelected)
            [self attachScreenshot:settings.screenshot named:@"select-failed"];
        XCTAssert(row.isSelected, @"Could not select the theme %@", name);
    }
    [NSThread sleepForTimeInterval:0.5];
}

- (XCUIElement *)darkModeCheckbox:(XCUIElement *)settings {
    XCUIElement *checkbox = settings.checkBoxes[@"Dark mode"];
    XCTAssert([self waitForElement:checkbox toExistWithTimeout:5], @"No Dark mode checkbox");
    return checkbox;
}

// Clicks the Dark mode checkbox. Tests that call this click it again when
// they end, so the appearance setting goes back to following the system.
- (void)toggleDarkMode:(XCUIElement *)settings {
    XCUIElement *checkbox = [self darkModeCheckbox:settings];
    [checkbox click];
    [NSThread sleepForTimeInterval:1];
}

// Opens a game with File > Open…, and returns its window. The path is
// relative to the root of the repository.
- (XCUIElement *)openGame:(NSString *)path inApp:(XCUIApplication *)app {
    [app.menuBars.menuBarItems[@"File"] click];
    [app.menuBars.menuItems[@"Open…"] click];
    NSString *root = @(__FILE__).stringByDeletingLastPathComponent.stringByDeletingLastPathComponent;
    NSURL *url = [NSURL fileURLWithPath:[root stringByAppendingPathComponent:path]];
    [app typeKey:@"g" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierShift];
    XCUIElement *sheet = app.sheets.firstMatch;
    XCTAssert([self waitForElement:sheet toExistWithTimeout:5], @"No Go to Folder sheet");
    [sheet typeText:url.path];
    [sheet typeKey:XCUIKeyboardKeyEnter modifierFlags:XCUIKeyModifierNone];
    [app typeKey:XCUIKeyboardKeyEnter modifierFlags:XCUIKeyModifierNone];

    XCUIElement *window = [app.windows matchingPredicate:[NSPredicate predicateWithFormat:@"identifier BEGINSWITH 'gameWin'"]].firstMatch;
    XCTAssert([self waitForElement:window toExistWithTimeout:10], @"The game window did not open");
    [NSThread sleepForTimeInterval:2];
    return window;
}

// Brings the window to the front, then attaches a screenshot of it. The Window
// menu works even when another window covers this one's title bar, where a
// click would land on the other window.
- (void)captureWindow:(XCUIElement *)window named:(NSString *)name {
    XCUIApplication *app = [[XCUIApplication alloc] init];
    XCUIElement *windowMenu = app.menuBars.menuBarItems[@"Window"];
    [windowMenu click];
    XCUIElement *item = window.title.length ? windowMenu.menuItems[window.title] : nil;
    if (item.exists) {
        [item click];
    } else {
        [app typeKey:XCUIKeyboardKeyEscape modifierFlags:XCUIKeyModifierNone];
        [[[window coordinateWithNormalizedOffset:CGVectorMake(0.5, 0)] coordinateWithOffset:CGVectorMake(0, 12)] click];
    }
    [NSThread sleepForTimeInterval:0.7];
    [self attachScreenshot:window.screenshot named:name];
}

// A game with a dark mood: a theme whose light colors are copied from its dark
// colors looks the same whether Spatterlight is in light or dark mode.
- (void)testCopyColors {
    XCUIApplication *app = [[XCUIApplication alloc] init];
    [self launch:app];
    XCUIElement *game = [self openGame:@"babel/test/bronze/Bronze.zblorb" inApp:app];
    XCUIElement *settings = [self openSettings:app];
    [self selectTheme:@"Lectrote" inSettings:settings];
    [self showStylesTab:settings];
    SettingsUITests * __weak weakSelf = self;
    __block BOOL darkModeToggled = NO, themeCreated = NO;
    [self addTeardownBlock:^{
        if (darkModeToggled)
            [weakSelf toggleDarkMode:settings];
        if (themeCreated) {
            // The created theme is still selected
            [weakSelf showTab:@"Themes" inSettings:settings];
            [settings.buttons[@"remove"] click];
            [NSThread sleepForTimeInterval:0.5];
        }
    }];

    XCUIElement *copyFromDark = settings.buttons[@"Replace with Dark"];
    XCUIElement *copyFromLight = settings.buttons[@"Replace with Light"];
    XCTAssert([self waitForElement:copyFromDark toExistWithTimeout:5], @"No Replace with Dark button");
    XCTAssert(copyFromDark.enabled && copyFromLight.enabled, @"Lectrote has different light and dark colors");
    [self captureWindow:settings named:@"1-styles"];
    [self captureWindow:game named:@"1-game"];

    [copyFromDark click];
    [NSThread sleepForTimeInterval:1];
    themeCreated = ![[self themeNameOnStylesTab:settings] isEqualToString:@"Lectrote"];
    XCTAssert(themeCreated, @"Changing a built-in theme should make a copy");
    XCTAssertFalse(copyFromDark.enabled || copyFromLight.enabled, @"Nothing to copy once the light and dark colors match");
    [self captureWindow:settings named:@"2-styles-copied"];
    [self captureWindow:game named:@"2-game-copied"];

    [self toggleDarkMode:settings];
    darkModeToggled = YES;
    [self captureWindow:settings named:@"3-styles-dark-mode"];
    [self captureWindow:game named:@"3-game-dark-mode"];

    // The copy is the user's own theme, so replacing its colors asks first
    [settings.buttons[@"Swap ink and paper colors"].firstMatch click];
    [NSThread sleepForTimeInterval:1];
    XCTAssert(copyFromLight.enabled, @"Swapping the light ink and paper should make the columns differ");
    [copyFromLight click];
    XCUIElement *confirm = settings.sheets.firstMatch;
    XCTAssert([self waitForElement:confirm toExistWithTimeout:5], @"No confirmation before replacing the colors of the user's own theme");
    [self attachScreenshot:settings.screenshot named:@"4-confirm"];
    [confirm.buttons[@"Cancel"] click];
    [NSThread sleepForTimeInterval:0.5];
    XCTAssert(copyFromLight.enabled, @"Cancel should leave the colors alone");
    [copyFromLight click];
    XCTAssert([self waitForElement:confirm toExistWithTimeout:5]);
    [confirm.buttons[@"Replace"] click];
    [NSThread sleepForTimeInterval:1];
    XCTAssertFalse(copyFromLight.enabled, @"Replace should make the columns match");
}

// The Dark mode checkbox switches the appearance, and switches it back.
- (void)testDarkModeCheckbox {
    XCUIApplication *app = [[XCUIApplication alloc] init];
    XCUIElement *settings = [self launchAndOpenSettings:app];
    [self selectTheme:@"Lectrote" inSettings:settings];
    [self showStylesTab:settings];
    XCUIElement *checkbox = [self darkModeCheckbox:settings];
    id original = checkbox.value;
    SettingsUITests * __weak weakSelf = self;
    [self addTeardownBlock:^{
        if (![checkbox.value isEqual:original])
            [weakSelf toggleDarkMode:settings];
    }];
    [self attachScreenshot:settings.screenshot named:@"styles"];

    [self toggleDarkMode:settings];
    XCTAssertNotEqualObjects(checkbox.value, original, @"The Dark mode checkbox did not change");
    [self attachScreenshot:settings.screenshot named:@"styles-other-appearance"];

    [self toggleDarkMode:settings];
    XCTAssertEqualObjects(checkbox.value, original, @"The Dark mode checkbox did not change back");
}

@end
