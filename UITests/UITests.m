//
//  UITests.m
//  UITests
//
//  Created by Administrator on 2021-07-20.
//

#import <XCTest/XCTest.h>

@interface UITests : XCTestCase {
    NSBundle *testBundle;
}

@end

@implementation UITests

- (void)setUp {
    // Put setup code here. This method is called before the invocation of each test method in the class.

    testBundle = [NSBundle bundleForClass:UITests.class];

    // In UI tests it is usually best to stop immediately when a failure occurs.
    self.continueAfterFailure = NO;

    // Start every test on an empty library of its own
    XCUIApplication *app = [UITests app];
    app.launchEnvironment = @{ @"SPATTERLIGHT_TEST_STORE": @"UITests",
                               @"SPATTERLIGHT_TEST_STORE_FRESH": @"1" };
    [app launch];

    // In UI tests it’s important to set the initial state - such as interface orientation - required for your tests before they run. The setUp method is a good place to do this.
}

/// The app, set to use the UI tests' own library instead of the user's (see
/// CoreDataManager +testStoreURL). Launch it through this, relaunches too, or
/// it opens the user's library.
+ (XCUIApplication *)app {
    XCUIApplication *app = [[XCUIApplication alloc] init];
    app.launchEnvironment = @{ @"SPATTERLIGHT_TEST_STORE": @"UITests" };
    return app;
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
}

/// Waits for the element to match predicateFormat (which takes no arguments),
/// using XCTWaiter to avoid priority inversion between the User-interactive
/// QoS test thread and the Default QoS accessibility thread.
+ (BOOL)waitForElement:(XCUIElement *)element matching:(NSString *)predicateFormat timeout:(NSTimeInterval)timeout {
    NSPredicate *predicate = [NSPredicate predicateWithFormat:predicateFormat argumentArray:nil];
    XCTNSPredicateExpectation *expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:element];
    return [XCTWaiter waitForExpectations:@[expectation] timeout:timeout] == XCTWaiterResultCompleted;
}

+ (BOOL)waitForElement:(XCUIElement *)element toExistWithTimeout:(NSTimeInterval)timeout {
    return [self waitForElement:element matching:@"exists == true" timeout:timeout];
}

- (BOOL)waitForElement:(XCUIElement *)element toExistWithTimeout:(NSTimeInterval)timeout {
    return [UITests waitForElement:element toExistWithTimeout:timeout];
}

+ (BOOL)waitForElement:(XCUIElement *)element toBeHittableWithTimeout:(NSTimeInterval)timeout {
    return [self waitForElement:element matching:@"exists == true AND hittable == true" timeout:timeout];
}

- (BOOL)waitForElement:(XCUIElement *)element toBeHittableWithTimeout:(NSTimeInterval)timeout {
    return [UITests waitForElement:element toBeHittableWithTimeout:timeout];
}

/// The text view of the window's index'th scroll view.
+ (XCUIElement *)textViewOfWindow:(XCUIElement *)window scrollViewIndex:(NSUInteger)index {
    XCUIElement *scrollView = [window.scrollViews elementBoundByIndex:index];
    return [scrollView childrenMatchingType:XCUIElementTypeTextView].element;
}

+ (NSURL *)tempDir {
    NSURL *usersURL = [NSURL fileURLWithPath:@"/Users"
                                 isDirectory:YES];

    NSError *error = nil;

    NSURL *tempURL = [[NSFileManager defaultManager] URLForDirectory:NSItemReplacementDirectory
                                                            inDomain:NSUserDomainMask
                                                   appropriateForURL:usersURL
                                                              create:YES
                                                               error:&error];
    if (error)
        NSLog(@"tempDir: Error: %@", error);

    return tempURL;
}

+ (void)typeURL:(NSURL *)url intoFileDialog:(XCUIElement *)dialog andPressButtonWithText:(NSString *)buttonText
{
    [UITests typeURL:url intoFileDialog:dialog];

    XCUIElement *openButton = dialog.buttons[buttonText];

    XCTAssert([UITests waitForElement:openButton toExistWithTimeout:5]);
    [openButton click];
}

+ (void)typeURL:(NSURL *)url intoApp:(XCUIApplication *)app andPressButtonWithText:(NSString *)buttonText
{
    [UITests typeURL:url intoApp:app];
    [app typeKey:XCUIKeyboardKeyEnter modifierFlags:XCUIKeyModifierNone];
}

+ (void)typeURL:(NSURL *)url intoFileDialog:(XCUIElement *)dialog {
    [dialog typeKey:@"g" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierShift];

    XCUIElement *sheet = dialog.sheets.firstMatch;
    if (!sheet.exists) {
        XCTAssert([UITests waitForElement:sheet toExistWithTimeout:5]);
    }

    [sheet typeText:url.path];
    [sheet typeKey:XCUIKeyboardKeyEnter modifierFlags:XCUIKeyModifierNone];
}

+ (void)typeURL:(NSURL *)url intoApp:(XCUIApplication *)app {
    [app typeKey:@"g" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierShift];

    XCUIElement *sheet = app.sheets.firstMatch;
    if(![sheet exists]) {
        [app typeKey:@"g" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierShift];
    }

    XCTAssert([UITests waitForElement:sheet toExistWithTimeout:5]);

    [sheet typeText:url.path];
    [sheet typeKey:XCUIKeyboardKeyEnter modifierFlags:XCUIKeyModifierNone];
}

// Read errors are logged; the comparison that follows fails on the nil.
+ (NSString *)stringAtURL:(NSURL *)url {
    NSError *error = nil;
    NSString *string = [NSString stringWithContentsOfURL:url encoding:NSUTF8StringEncoding error:&error];
    if (error)
        NSLog(@"Error: %@", error);
    return string;
}

+ (NSAttributedString *)rtfAtURL:(NSURL *)url {
    NSError *error = nil;
    NSAttributedString *string = [[NSAttributedString alloc] initWithURL:url options:@{NSDocumentTypeDocumentAttribute:NSRTFTextDocumentType} documentAttributes:nil error:&error];
    if (error)
        NSLog(@"Error: %@", error);
    return string;
}

+ (NSString *)transcriptFromFile:(NSString *)fileName {
    NSURL *url = [UITests transcriptWithFormat:@"Plain Text"];
    return [UITests stringAtURL:[url URLByAppendingPathComponent:fileName isDirectory:NO]];
}

+ (NSAttributedString *)attributedTranscriptFromFile:(NSString *)fileName {
    NSURL *url = [UITests transcriptWithFormat:@"Plain Text"];
    return [UITests rtfAtURL:[url URLByAppendingPathComponent:fileName isDirectory:NO]];
}

+ (NSURL *)transcriptWithFormat:(NSString *)format {

    XCUIApplication *app = [UITests app];
    XCUIElementQuery *menuBarsQuery = app.menuBars;

    [menuBarsQuery.menuBarItems[@"File"] click];

    NSURL *url = [UITests tempDir];

    [menuBarsQuery.menuItems[@"Save Scrollback…"] click];

    XCUIElement *savePanel = app.sheets.firstMatch;

    NSLog(@"Transcript is in directory %@", url.path);

    [UITests typeURL:url intoFileDialog:savePanel];

    XCUIElement *popUp = [savePanel.popUpButtons elementMatchingType:XCUIElementTypePopUpButton identifier:@"saveFormatPopUp"];
    [popUp click];
    [popUp.menuItems[format] click];
    XCUIElement *saveButton = savePanel.buttons[@"Save"];
    [saveButton click];

    XCUIElement *alert = app.sheets.firstMatch;
    if (alert.exists) {
        [alert.buttons[@"Replace"] click];
    }

    return url;
}

+ (NSURL *)saveTranscriptInWindow:(XCUIElement *)gameWin {

    XCUIApplication *app = [UITests app];
    NSURL *transcriptURL = [UITests tempDir];

    NSLog(@"Transcript is in directory %@", transcriptURL.path);

    XCUIElement *savePanel = gameWin.sheets.firstMatch;
    if (!savePanel.exists) {
        XCTAssert([UITests waitForElement:savePanel toExistWithTimeout:5]);
    }

    [UITests typeURL:transcriptURL intoFileDialog:savePanel andPressButtonWithText:@"Save"];

    XCUIElement *alert = app.sheets.firstMatch;
    if (alert.exists) {
        [alert.buttons[@"Replace"] click];
    }
    return transcriptURL;
}

+ (void)turnOnDeterminism:(nullable NSString *)theme {
    [UITests selectTheme:theme];
    XCUIApplication *app = [UITests app];

    [app.toolbars.buttons[@"Details"] click];
    [app.checkBoxes[@"Animate scrolling"] click];

    // Tests save the whole scrollback as their transcript, so turn off
    // scrollback trimming (0 = unlimited)
    XCUIElement *scrollbackLimitField = app.textFields[@"scrollbackLimitField"];
    [scrollbackLimitField click];
    [scrollbackLimitField typeKey:@"a" modifierFlags:XCUIKeyModifierCommand];
    [scrollbackLimitField typeText:@"0\r"];

    [app.toolbars.buttons[@"Misc"] click];
    [app.checkBoxes[@"Determinism"] click];

    [app typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [app typeKey:@"r" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierOption];
}

+ (void)selectTheme:(nullable NSString *)theme {
    if (!theme.length)
        theme = @"Default";
    XCUIApplication *app = [UITests app];
    [app typeKey:@"," modifierFlags:XCUIKeyModifierCommand];
    XCUIElement *themesTab = app.toolbars.buttons[@"Themes"];
    if (themesTab.exists)
        [themesTab click];
    [app.tables.staticTexts[theme] click];
}

- (void)openCommandScript:(NSString *)name {
    XCUIApplication *app = [UITests app];

    [app typeKey:@"o" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierCommand];

    name = [NSString stringWithFormat:@"%@ command script", name];

    NSURL *url = [testBundle URLForResource:name
                              withExtension:@"txt"
                               subdirectory:nil];

    [UITests typeURL:url intoApp:app andPressButtonWithText:@"Open"];

    XCUIElement *alert = app.dialogs[@"alert"];
    if (alert.exists) {
        [alert.checkBoxes.firstMatch click];
        [alert.buttons[@"Okay"] click];
    }
}

- (NSString *)comparisonTranscriptFor:(NSString *)name {
    name = [NSString stringWithFormat:@"Transcript of %@", name];

    NSURL *url = [testBundle URLForResource:name
                              withExtension:@"txt"
                               subdirectory:nil];
    return [UITests stringAtURL:url];
}

// The transcript saved as fileName in directory, compared with the
// "Transcript of <facitName>" in the test bundle.
- (void)assertTranscriptIn:(NSURL *)directory named:(NSString *)fileName equalsFacitFor:(NSString *)facitName {
    NSString *transcript = [UITests stringAtURL:[directory URLByAppendingPathComponent:fileName isDirectory:NO]];
    XCTAssert([transcript isEqualToString:[self comparisonTranscriptFor:facitName]]);
}

// The same for a Rich Text transcript, compared with "<facitName>.rtf".
- (void)assertRTFTranscriptIn:(NSURL *)directory named:(NSString *)fileName equalsFacitFor:(NSString *)facitName {
    NSURL *url = [testBundle URLForResource:facitName
                              withExtension:@"rtf"
                               subdirectory:nil];
    NSAttributedString *facit = [UITests rtfAtURL:url];
    NSAttributedString *comparison = [UITests rtfAtURL:[directory URLByAppendingPathComponent:fileName isDirectory:NO]];
    XCTAssert([comparison isEqual:facit]);
}

//- (void)testTetrisAutorestore {
//
//    XCUIApplication *app = [UITests app];
//
//    XCUIElement *textField = [self addAndSelectGame:@"freefall.z5"];
//    [textField doubleClick];
//
//    XCUIElement *gamewin = app/*@START_MENU_TOKEN@*/.windows[@"gameWinZCODE-2-951111-2084"]/*[[".windows[@\"Tetris\"]",".windows[@\"gameWinZCODE-2-951111-2084\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;
//
//    XCUIElement *alertSheet = gamewin.sheets[@"alert"];
//    if ([alertSheet waitForExistenceWithTimeout:5]) {
//        [alertSheet.checkBoxes[@"Remember this choice."] click];
//        [alertSheet.buttons[@"Continue"] click];
//    }
//
//    XCUIElement *lowerScrollView = gamewin.scrollViews[@"buffer scroll view"];
//
//
//    XCUIElement *lowerTextView = [lowerScrollView childrenMatchingType:XCUIElementTypeTextView].element;
//
//    XCUIElement *upperTextView = [gamewin.staticTexts elementBoundByIndex:0];
//
//    // Reset any autorestored game running
//    [lowerTextView typeKey:@"r" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierOption];
//
//    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"value CONTAINS 'You wake up.'"];
//    XCTNSPredicateExpectation *expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:lowerTextView];
//
//    XCTAssertEqual([XCTWaiter waitForExpectations:@[expectation] timeout:5], XCTWaiterResultCompleted);
//
//    // Start the tiles falling
//    [lowerTextView typeKey:@" " modifierFlags:XCUIKeyModifierNone];
//
//    CGRect textRect = lowerTextView.frame;
//    CGRect scrollRect = lowerScrollView.frame;
//    CGRect statusRect = upperTextView.frame;
//
//    XCTAssertEqual(textRect.origin.y, scrollRect.origin.y,
//                   @"textView origin doesn't match scrollView origin");
//
//    [gamewin.buttons[XCUIIdentifierCloseWindow] click];
//    [textField doubleClick];
//
//    XCTAssert(NSEqualRects(textRect, lowerTextView.frame));
//    NSLog(@"Lower text view rects match: %@", NSStringFromRect(textRect));
//    XCTAssert(NSEqualRects(scrollRect, lowerScrollView.frame));
//    NSLog(@"Lower scroll view rects match: %@", NSStringFromRect(scrollRect));
//    XCTAssert(NSEqualRects(statusRect, upperTextView.frame));
//    NSLog(@"Status window view rects match: %@", NSStringFromRect(statusRect));
//}
//
//- (void)testImportGame {
//    XCUIApplication *app = [UITests app];
//
//    XCUIElement *libraryWindow = app.windows[@"library"];
//    XCUIElement *foundElement = [self addAndSelectGame:@"freefall.z5"];
//    [foundElement typeKey:XCUIKeyboardKeyDelete modifierFlags:XCUIKeyModifierCommand];
//
//    XCUIElement *alertDialog = app.dialogs[@"alert"];
//    if (alertDialog.exists)
//        [alertDialog.buttons[@"Delete"] click];
//
//    XCTAssert(!foundElement.exists);
//
//    foundElement = [self addAndSelectGame:@"freefall.z5"];
//    [foundElement doubleClick];
//    XCUIElement *gamewin = app/*@START_MENU_TOKEN@*/.windows[@"gameWinZCODE-2-951111-2084"]/*[[".windows[@\"Tetris\"]",".windows[@\"gameWinZCODE-2-951111-2084\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;
//    if (!gamewin.exists)
//        gamewin = app.windows[@"freefall.z5"];
//    if (!gamewin.exists)
//        gamewin = app.windows[@"Tetris"];
//    XCTAssert(gamewin.exists);
//
//    [app typeKey:@"1" modifierFlags:XCUIKeyModifierCommand];
//
//    XCUIElement *searchSearchField = libraryWindow/*@START_MENU_TOKEN@*/.searchFields[@"Search"]/*[[".splitGroups[@\"SplitViewTotal\"].searchFields[@\"Search\"]",".searchFields[@\"Search\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;
//    XCUIElement *cancelButton = searchSearchField.buttons[@"cancel"];
//    if (cancelButton.exists)
//        [cancelButton click];
//    [searchSearchField click];
//}

- (void)testFileMenu {
    XCUIApplication *app = [UITests app];

    XCUIElementQuery *menuBarsQuery = app.menuBars;

    XCUIElement *fileMenuBarItem = menuBarsQuery.menuBarItems[@"File"];
    if (![self waitForElement:fileMenuBarItem toBeHittableWithTimeout:5]) {
        XCUIElement *gamewinzcode169510244de6Window = app/*@START_MENU_TOKEN@*/.windows[@"gameWinZCODE-16-951024-4DE6"]/*[[".windows[@\"Curses\"]",".windows[@\"gameWinZCODE-16-951024-4DE6\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;
        [[gamewinzcode169510244de6Window.scrollViews[@"buffer scroll view"] childrenMatchingType:XCUIElementTypeTextView].element typeKey:@"f" modifierFlags:(XCUIKeyModifierCommand | XCUIKeyModifierControl)];
        fileMenuBarItem = menuBarsQuery.menuBarItems[@"File"];
    }
    [fileMenuBarItem click];
    [menuBarsQuery.menuItems[@"Open…"] click];

    NSURL *url = [testBundle URLForResource:@"curses"
                              withExtension:@"z5"
                               subdirectory:nil];

    [UITests typeURL:url intoApp:app andPressButtonWithText:@"Open"];

    NSArray *menuItemTitles = @[@"Edited Entries", @"Games in Library", @"Complete Database"];

    for (NSString *menuItem in menuItemTitles) {

        [menuBarsQuery.menuItems[@"Export Metadata…"] click];

        XCUIElement *saveDialog = app.sheets.firstMatch;

        XCTAssert([self waitForElement:saveDialog toExistWithTimeout:5]);

        NSURL *path = url.URLByDeletingLastPathComponent;

        [UITests typeURL:path intoFileDialog:saveDialog];

        saveDialog = app.sheets.firstMatch;

        XCUIElement *popup;

        // We try all three strings until one matches
        for (NSString *popUpString in menuItemTitles) {
            popup = saveDialog.popUpButtons[popUpString];
            if (popup.exists)
                break;
        }

        [popup click];

        [saveDialog.menuItems[menuItem] click];

        XCUIElement *exportButton = saveDialog.buttons[@"Export"];
        [exportButton click];

        // When the target file already exists, a "file exists" confirmation
        // is presented as a second sheet stacked on top of the save panel.
        // app.sheets.firstMatch resolves to the save panel rather than the
        // alert, so scope the query to the save-panel sheet and let the
        // recursive buttons query reach the Replace button in the nested
        // alert sheet. This also avoids the duplicate Replace button that
        // lives on the Touch Bar. The alert button reports itself as not
        // hittable, so wait for it to merely exist (click derives a
        // coordinate from its frame). Use a generous timeout; otherwise the
        // undismissed alert keeps the save panel open and derails the next
        // iteration.
        XCUIElement *replaceButton = app.sheets[@"save-panel"].buttons[@"Replace"];
        if ([self waitForElement:replaceButton toExistWithTimeout:5]) {
            [replaceButton click];
        }

        // An export may cause a "No metadata exported" alert.
        // Dismiss it before the next iteration; otherwise it stays modal and
        // the next "Export Metadata…" never opens a fresh save panel.
        XCUIElement *alert = app.dialogs[@"alert"];
        if ([self waitForElement:alert.buttons.firstMatch toBeHittableWithTimeout:5]) {
            [alert.buttons.firstMatch click];
        }
    }
}

- (void)testMoreFileMenu {

    XCUIApplication *app = [UITests app];

    XCUIElementQuery *menuBarsQuery = app.menuBars;

    XCUIElement *fileMenuBarItem = menuBarsQuery.menuBarItems[@"File"];
    XCUIElement *gameMenuBarItem = menuBarsQuery.menuBarItems[@"Game"];
    [fileMenuBarItem click];
    [menuBarsQuery.menuItems[@"Open…"] click];

    NSURL *url = [testBundle URLForResource:@"imagetest"
                              withExtension:@"gblorb"
                               subdirectory:nil];

    [UITests typeURL:url intoApp:app andPressButtonWithText:@"Open"];

    [gameMenuBarItem click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Reveal in Finder"]/*[[".menuBarItems[@\"File\"]",".menus.menuItems[@\"Reveal in Finder\"]",".menuItems[@\"Reveal in Finder\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];

    [app terminate];
    // Quit and restart the app to get back from Finder
    [app launch];
    XCTAssert([self waitForElement:app toExistWithTimeout:5]);

    [menuBarsQuery.menuBarItems[@"Window"] click];
    [menuBarsQuery.menuItems[@"Interactive Fiction"] click];

    XCUIElement *libraryWindow = app/*@START_MENU_TOKEN@*/.windows[@"library"]/*[[".windows[@\"Interactive Fiction\"]",".windows[@\"library\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;

    XCUIElement *searchSearchField = libraryWindow.searchFields.firstMatch;

    XCUIElement *cancelButton = searchSearchField.buttons[@"cancel"];
    if (cancelButton.exists)
        [cancelButton click];
    [searchSearchField click];
    [searchSearchField typeText:@"image"];

    [[[libraryWindow/*@START_MENU_TOKEN@*/.tables[@"Games"]/*[[".splitGroups[@\"SplitViewTotal\"]",".scrollViews.tables[@\"Games\"]",".tables[@\"Games\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/.tableRows childrenMatchingType:XCUIElementTypeCell] elementBoundByIndex:0] click];

    [gameMenuBarItem.menuItems[@"Show Info…"] click];
    [app typeText:@" "];
    [[[libraryWindow/*@START_MENU_TOKEN@*/.tables[@"Games"]/*[[".splitGroups[@\"SplitViewTotal\"]",".scrollViews.tables[@\"Games\"]",".tables[@\"Games\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/.tableRows childrenMatchingType:XCUIElementTypeCell] elementBoundByIndex:0] click];
    [gameMenuBarItem click];
    [menuBarsQuery.menuItems[@"Play Game"] click];

    [menuBarsQuery.menuBarItems[@"Window"] click];

    [/*@START_MENU_TOKEN@*/[[app.menuBarItems matchingIdentifier:@"_NS:381"].menuItems matchingIdentifier:@"makeKeyAndOrderFront:"].firstMatch/*[["app","[[",".menuItems matchingIdentifier:@\"makeKeyAndOrderFront:\"] elementBoundByIndex:0]",".menus matchingIdentifier:@\"_NS:383\"].menuItems matchingIdentifier:@\"imagetest.gblorb\"].firstMatch",".menuBarItems matchingIdentifier:@\"_NS:381\"].menuItems matchingIdentifier:@\"makeKeyAndOrderFront:\"].firstMatch"],[[[-1,0,1]],[[1,4],[1,3],[1,2]]],[0,0]]@END_MENU_TOKEN@*/ click];
    
    XCUIElement *gameWindow = app.windows[@"imagetest.gblorb"];
    [XCUIElement performWithKeyModifiers:XCUIKeyModifierOption block:^{
        [gameWindow.buttons[XCUIIdentifierFullScreenWindow] click];
    }];
    [gameMenuBarItem click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Reset Game"]/*[[".menuBarItems[@\"File\"]",".menus.menuItems[@\"Reset Game\"]",".menuItems[@\"Reset Game\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [fileMenuBarItem click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Close Window"]/*[[".menuBarItems[@\"File\"]",".menus.menuItems[@\"Close Window\"]",".menuItems[@\"Close Window\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    libraryWindow = app/*@START_MENU_TOKEN@*/.windows[@"library"]/*[[".windows[@\"Interactive Fiction\"]",".windows[@\"library\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;
    XCTAssert([self waitForElement:libraryWindow toExistWithTimeout:5]);
    [libraryWindow.toolbars.buttons[@"Play"] click];
    [fileMenuBarItem click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Clear Scrollback"]/*[[".menuBarItems[@\"File\"]",".menus.menuItems[@\"Clear Scrollback\"]",".menuItems[@\"Clear Scrollback\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];

    XCUIElement *gamewin = app.windows[@"imagetest.gblorb"];

    XCUIElement *textView = [gamewin.scrollViews[@"buffer scroll view"] childrenMatchingType:XCUIElementTypeTextView].element;
    [textView click];
    [textView typeText:@"image\r"];
    [textView typeText:@"image left link\r"];

    [textView typeText:@"image right\r"];

    NSArray *menuItemTitles = @[@"Rich Text Format with images", @"Rich Text Format without images", @"Plain Text"];

    for (NSString *menuItem in menuItemTitles) {
        [fileMenuBarItem click];
        [menuBarsQuery.menuItems[@"Save Scrollback…"] click];
        XCUIElement *savePanel = gamewin.sheets[@"save-panel"];
        XCTAssert([self waitForElement:savePanel toExistWithTimeout:5]);
       [[savePanel.popUpButtons elementMatchingType:XCUIElementTypePopUpButton identifier:@"saveFormatPopUp"] click];

        [savePanel.menuItems[menuItem] click];
        XCUIElement *saveButton = savePanel.buttons[@"Save"];
        [saveButton click];

        XCUIElement *alert = app.sheets.firstMatch;
        if (alert.exists) {
            [alert.buttons[@"Replace"] click];
        }
    }

    [fileMenuBarItem click];
    [XCUIElement performWithKeyModifiers:XCUIKeyModifierOption block:^{
        [menuBarsQuery.menuItems[@"Prune Library…"] click];
    }];

    XCUIElement *alertDialog = app.dialogs[@"alert"];
    [alertDialog.buttons[@"Prune"] click];
    [fileMenuBarItem click];
    [menuBarsQuery.menuItems[@"Delete Library…"] click];
    [app/*@START_MENU_TOKEN@*/.checkBoxes[@"Force quit running games and delete them."]/*[[".dialogs[@\"alert\"].checkBoxes[@\"Force quit running games and delete them.\"]",".checkBoxes[@\"Force quit running games and delete them.\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/ click];
    [alertDialog.buttons[@"Delete Library"] click];
    [menuBarsQuery.menuBarItems[@"Edit"] click];
    [menuBarsQuery.menuItems[@"Undo"] click];
}

- (void)testImportMetadata {
    XCUIApplication *app = [UITests app];

    XCUIElementQuery *menuBarsQuery = app.menuBars;

    [menuBarsQuery.menuItems[@"Import Metadata…"] click];

    XCUIElement *openDialog = app.sheets.firstMatch;
    XCTAssert([self waitForElement:openDialog toExistWithTimeout:5]);

    NSURL *url = [testBundle URLForResource:@"test_stories"
                              withExtension:@"iFiction"
                               subdirectory:nil];

    [UITests typeURL:url intoFileDialog:openDialog andPressButtonWithText:@"Import"];
}

/// Polls @c textView's text until it stops changing, i.e. the game has finished
/// printing (or re-rendering after a restore) and is blocking on input. Returns
/// once the value is unchanged for two consecutive samples, or after a timeout.
- (void)waitForTextViewToSettle:(XCUIElement *)textView {
    // Once the game finishes, its window is retitled "… (finished)" and no longer
    // matches the running-game query, so the text view stops resolving. There is
    // nothing left to settle in that case; bail out rather than let a snapshot of
    // a missing element throw.
    if (!textView.exists)
        return;
    NSString *current = textView.value;
    NSInteger stableSamples = 0;
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:5];
    while (deadline.timeIntervalSinceNow > 0) {
        [NSThread sleepForTimeInterval:0.2];
        if (!textView.exists)
            return;
        NSString *previous = current;
        current = textView.value;
        if (current.length > 0 && [current isEqualToString:previous]) {
            if (++stableSamples >= 2)
                return;
        } else {
            stableSamples = 0;
        }
    }
}

/// Returns YES once "run all" has finished and the game is back at its top-level
/// ">" prompt (or the window is already gone). Distinguishes that prompt from the
/// "Kill and hit a key>", "Kill and enter a line>" and "Wait for a timer event>"
/// checkpoint prompts, so the driver stops while the game is still running —
/// before any extra input makes it exit, which would leave nothing to save.
- (BOOL)runAllDidCompleteInTextView:(XCUIElement *)textView {
    if (!textView.exists)
        return YES;
    [self waitForTextViewToSettle:textView];
    if (!textView.exists)
        return YES;
    NSString *value = [textView.value stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (![value hasSuffix:@">"])
        return NO;
    return !([value hasSuffix:@"key>"] || [value hasSuffix:@"line>"] || [value hasSuffix:@"event>"]);
}

/// Closes the current game window, replays the selected game so it autorestores
/// from autosave, and waits for the text view to reappear before typing @c text.
/// Waiting for the text to settle before closing and after reopening keeps the
/// kill/restore round-trip in step with the game: it stops us autosaving a
/// mid-output state, or typing before the restored scrollback has re-rendered,
/// either of which would drop a checkpoint and truncate the transcript.
- (void)closeAndReopenGame:(XCUIElement *)textView
                gamesTable:(XCUIElement *)gamesTable
                   andType:(NSString *)text {
    // The run can complete in fewer cycles than the script provides. Once the
    // game has finished, its window is retitled "… (finished)" and the running-
    // game text view no longer resolves; there is nothing left to drive, so skip
    // and let any surplus trailing cycles be harmless no-ops.
    if (!textView.exists)
        return;

    // Wait until the game has finished printing and is blocking on input, so the
    // autosave captures a settled prompt rather than a state mid-output.
    [self waitForTextViewToSettle:textView];

    // The game may have reached its end while settling; if so, stop here too.
    if (!textView.exists)
        return;

    // Close the game window and wait for it to disappear.
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    XCTAssertTrue([UITests waitForElement:textView matching:@"exists == false" timeout:5],
                   @"Game window did not close");

    // Replay the selected game, autorestoring from autosave, and wait for the
    // text view to come back and finish re-rendering before typing into it.
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    XCTAssertTrue([self waitForElement:textView toExistWithTimeout:5],
                  @"Game window did not reopen from autosave");
    [self waitForTextViewToSettle:textView];
    [self scrollTextViewToBottom:textView];

    if (text.length)
        [textView typeText:text];
}

/// In a small window, the text printed since the last key press can overflow
/// the buffer view. Spatterlight then holds the rest back behind an invisible
/// "more" prompt, and the next key only pages down instead of reaching the game,
/// which puts the test out of phase with the checkpoints. Scroll the view to
/// the bottom with the scroll wheel instead, which never reaches the game.
- (void)scrollTextViewToBottom:(XCUIElement *)textView {
    XCUIElement *scrollView = [UITests app].windows.scrollViews[@"buffer scroll view"];
    for (NSInteger i = 0; i < 10 && textView.exists; i++) {
        // The text view's frame covers the whole document, so its bottom edge
        // lies below the scroll view's by the height of the text not shown
        // (it is a few points above it when at the bottom).
        CGFloat hidden = NSMaxY(textView.frame) - NSMaxY(scrollView.frame);
        if (hidden <= 0)
            return;
        [scrollView scrollByDeltaX:0 deltaY:-(hidden + 100)];
        [NSThread sleepForTimeInterval:0.3];
    }
}

- (void)testAutosave {
    XCUIApplication *app = [UITests app];

    XCUIElement *libraryWindow = app/*@START_MENU_TOKEN@*/.windows[@"library"]/*[[".windows[@\"Interactive Fiction\"]",".windows[@\"library\"]"],[[[-1,1],[-1,0]]],[1]]@END_MENU_TOKEN@*/;

    [self playGame:@"autosavetest.gblorb"];

    XCUIElement *gameWindow = app/*@START_MENU_TOKEN@*/.windows[@"autosavetest.gblorb"]/*[[".windows[@\"autosavetest.gblorb\"]",".windows[@\"gameWinGLULX-1-160227-AB0E83B\"]"],[[[-1,1],[-1,0]]],[1]]@END_MENU_TOKEN@*/;
    XCUIElement *scrollView = gameWindow.scrollViews[@"buffer scroll view"];

    XCUIElement *textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;

    XCUIElement *alertSheet = gameWindow.sheets[@"alert"];
    if (alertSheet.exists) {
        [alertSheet.checkBoxes[@"Remember this choice."] click];
        [alertSheet.buttons[@"Continue"] click];
    }

    [app/*@START_MENU_TOKEN@*/.windows[@"library"].tables[@"Games"]/*[[".windows[@\"Interactive Fiction\"]",".splitGroups[@\"SplitViewTotal\"]",".scrollViews.tables[@\"Games\"]",".tables[@\"Games\"]",".windows[@\"library\"]"],[[[-1,4,1],[-1,0,1]],[[-1,3],[-1,2],[-1,1,2]],[[-1,3],[-1,2]]],[0,0]]@END_MENU_TOKEN@*/ typeKey:@"," modifierFlags:XCUIKeyModifierCommand];
    XCUIElement *themesTab = app.toolbars.buttons[@"Themes"];
    if (themesTab.exists)
        [themesTab click];
    [app/*@START_MENU_TOKEN@*/.tables.staticTexts[@"Zoom"]/*[[".dialogs[@\"Preferences\"]",".tabGroups",".scrollViews.tables",".tableRows",".cells.staticTexts[@\"Zoom\"]",".staticTexts[@\"Zoom\"]",".tables",".dialogs[@\"preferences\"]"],[[[-1,6,3],[-1,2,3],[-1,1,2],[-1,7,1],[-1,0,1]],[[-1,6,3],[-1,2,3],[-1,1,2]],[[-1,6,3],[-1,2,3]],[[-1,5],[-1,4],[-1,3,4]],[[-1,5],[-1,4]]],[0,0]]@END_MENU_TOKEN@*/ click];
    [app.toolbars.buttons[@"Details"] click];
    [app/*@START_MENU_TOKEN@*/.checkBoxes[@"Animate scrolling"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.checkBoxes[@\"Animate scrolling\"]",".checkBoxes[@\"Animate scrolling\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [app typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];

    // Reset any autorestored game running
    [textView typeKey:@"r" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierOption];

    // Wait for initial text to show
    XCTAssertTrue([UITests waitForElement:textView matching:@"value CONTAINS 'Not a game.'" timeout:5]);

    XCUIElement *statusLine = [gameWindow.staticTexts elementBoundByIndex:0];

    CGRect windowRect = gameWindow.frame;
    CGRect scrollRect = scrollView.frame;
    CGRect statusRect = statusLine.frame;

    [textView typeText:@"run all\r"];

    XCUIElement *gamesTable = libraryWindow/*@START_MENU_TOKEN@*/.tables[@"Games"]/*[[".splitGroups[@\"SplitViewTotal\"]",".scrollViews.tables[@\"Games\"]",".tables[@\"Games\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/;

    // Repeatedly close the game and reopen it from autosave, checking that the
    // state is restored each time. Each helper call waits for the window to
    // close and the text view to reappear before returning, so the sequence
    // stays deterministic instead of racing the asynchronous restore.
    for (NSInteger i = 0; i < 9; i++)
        [self closeAndReopenGame:textView gamesTable:gamesTable andType:@"\r"];

    NSDate *date = [NSDate date];

    [self closeAndReopenGame:textView gamesTable:gamesTable andType:@"q"];
    [self closeAndReopenGame:textView gamesTable:gamesTable andType:@"r"];

    NSTimeInterval interval1 = date.timeIntervalSinceNow;
    NSLog(@"Time taken: %f", interval1);

    date = [NSDate date];

    [self closeAndReopenGame:textView gamesTable:gamesTable andType:@"s"];
    [self closeAndReopenGame:textView gamesTable:gamesTable andType:@"t"];

    NSTimeInterval interval2 = date.timeIntervalSinceNow;

    NSLog(@"Time taken: %f (accelerated) Previous unaccelerated time taken: %f Diff: %f", fabs(interval2), fabs(interval1), fabs(interval1) - fabs(interval2));
    // There might be a way to measure this, but this is not it.
    //    XCTAssert(fabs(interval1) > fabs(interval2));
    for (NSInteger i = 0; i < 6; i++)
        [self closeAndReopenGame:textView gamesTable:gamesTable andType:@"\r"];

    NSArray<NSString *> *sampleTexts = @[
        @"This is a short sample text\r",
        @"This is a short sample text\r",
        @"Thıs is å shört sämplê tëxt\r",
        @"This is å shört sämplê tëxt\r",
        @"This is ä shört sämplê tëxt\r",
        @"This is ö shört sämplê tëxt\r",
        @"This is é shört sämplê tëxt\r",
    ];
    for (NSString *sample in sampleTexts)
        [self closeAndReopenGame:textView gamesTable:gamesTable andType:sample];

    // Drive the remaining checkpoints, but stop the moment the game returns to
    // its top-level prompt: reaching it means the run is complete, and stopping
    // while the game is still running (rather than pressing on until it exits)
    // keeps the final scrollback intact and saveable. The cap is just a safety
    // net in case completion is never detected.
    for (NSInteger i = 0; i < 21 && ![self runAllDidCompleteInTextView:textView]; i++)
        [self closeAndReopenGame:textView gamesTable:gamesTable andType:@"\r"];

    // Let the final response finish rendering before saving the scrollback,
    // otherwise the saved transcript is cut off before the end of the run.
    [self waitForTextViewToSettle:textView];

    NSString *comparison = [UITests transcriptFromFile:@"autosavetest.txt"];

    NSString *facit = [self comparisonTranscriptFor:@"autosavetest"];

    NSError *error = nil;

    // Replace the parts that vary randomly
    NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:@"(?<=Array: 0!=)(.*)(?=\n\nPassed.)" options:0 error:&error];

    if (error)
        NSLog(@"Error: %@", error);

    facit = [regex stringByReplacingMatchesInString:facit options:0 range:NSMakeRange(0, facit.length) withTemplate:@"X"];
    comparison = [regex stringByReplacingMatchesInString:comparison options:0 range:NSMakeRange(0, comparison.length) withTemplate:@"X"];
    regex = [NSRegularExpression regularExpressionWithPattern:@"(?<=Mainwin parent: )(.*)(?=, rock=0\n)" options:NSRegularExpressionDotMatchesLineSeparators error:&error];
    facit = [regex stringByReplacingMatchesInString:facit options:0 range:NSMakeRange(0, facit.length) withTemplate:@"X"];
    comparison = [regex stringByReplacingMatchesInString:comparison options:0 range:NSMakeRange(0, comparison.length) withTemplate:@"X"];

    // "(Deleting existing file: …)" is only printed when the file happens to
    // survive from a previous run, so it is absent on a clean run. Strip these
    // lines from both sides so the comparison doesn't depend on leftover files.
    regex = [NSRegularExpression regularExpressionWithPattern:@"\\(Deleting existing file:[^\n]*\\)\n" options:0 error:&error];
    facit = [regex stringByReplacingMatchesInString:facit options:0 range:NSMakeRange(0, facit.length) withTemplate:@""];
    comparison = [regex stringByReplacingMatchesInString:comparison options:0 range:NSMakeRange(0, comparison.length) withTemplate:@""];

    XCTAssert([comparison isEqualToString:facit]);

    NSLog(@"windowRect: %@ gameWindow.frame: %@", NSStringFromRect(windowRect), NSStringFromRect(gameWindow.frame));
    NSLog(@"scrollRect: %@ scrollView.frame: %@", NSStringFromRect(scrollRect), NSStringFromRect(scrollView.frame));
    NSLog(@"statusRect: %@ statusLine.frame: %@", NSStringFromRect(statusRect), NSStringFromRect(statusLine.frame));

    //    XCTAssert(NSEqualRects(windowRect, gameWindow.frame));
    //    XCTAssert(NSEqualRects(scrollRect, scrollView.frame));
    //    XCTAssert(NSEqualRects(statusRect, statusLine.frame));

    // If the run completed, the game has already finished and its window has been
    // retitled, so the running-game text view no longer resolves. Only send the
    // closing Cmd-W when it is still around.
    if (textView.exists)
        [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    XCTAssert(!textView.exists);
}

- (void)testGlulxe {
    XCUIApplication *app = [UITests app];

    XCUIElement *libraryWindow = app/*@START_MENU_TOKEN@*/.windows[@"library"]/*[[".windows[@\"Interactive Fiction\"]",".windows[@\"library\"]"],[[[-1,1],[-1,0]]],[1]]@END_MENU_TOKEN@*/;

    [self playGame:@"Transparent.gblorb"];

    XCUIElement *gameWindow = app.windows[@"Transparent"];
    XCUIElement *scrollView = gameWindow.scrollViews[@"buffer scroll view"];
    XCUIElement *textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;

    XCUIElement *alertSheet = gameWindow.sheets[@"alert"];
    if ([self waitForElement:alertSheet toExistWithTimeout:2]) {
        [alertSheet.checkBoxes[@"Remember this choice."] click];
        [alertSheet.buttons[@"Continue"] click];
    }

    [app/*@START_MENU_TOKEN@*/.windows[@"library"].tables[@"Games"]/*[[".windows[@\"Interactive Fiction\"]",".splitGroups[@\"SplitViewTotal\"]",".scrollViews.tables[@\"Games\"]",".tables[@\"Games\"]",".windows[@\"library\"]"],[[[-1,4,1],[-1,0,1]],[[-1,3],[-1,2],[-1,1,2]],[[-1,3],[-1,2]]],[0,0]]@END_MENU_TOKEN@*/ typeKey:@"," modifierFlags:XCUIKeyModifierCommand];
    XCUIElement *themesTab = app.toolbars.buttons[@"Themes"];
    if (themesTab.exists)
        [themesTab click];
    [app.tables.staticTexts[@"Lectrote Dark"] click];
    [app.toolbars.buttons[@"Details"] click];
    [app.checkBoxes[@"Animate scrolling"] click];
    [app.toolbars.buttons[@"Misc"] click];
    [app.checkBoxes[@"Determinism"] click];

    XCUIElement *preferences = app.dialogs[@"preferences"];
    XCUIElement *timerField = [preferences childrenMatchingType:XCUIElementTypeTextField].element;
    [timerField click];
    [timerField doubleClick];
    [timerField typeText:@"1000\r"];

    [app typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];

    // Reset any autorestored game running
    [textView typeKey:@"r" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierOption];

    // Wait for initial text to show
    XCTAssertTrue([UITests waitForElement:textView matching:@"value CONTAINS 'Adjust your volume.'" timeout:5]);

    [textView typeText:@"y\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];

    XCUIElement *gamesTable = libraryWindow/*@START_MENU_TOKEN@*/.tables[@"Games"]/*[[".splitGroups[@\"SplitViewTotal\"]",".scrollViews.tables[@\"Games\"]",".tables[@\"Games\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/;

    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"y\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:@" " modifierFlags:XCUIKeyModifierNone];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:@" " modifierFlags:XCUIKeyModifierNone];
    [textView typeKey:@" " modifierFlags:XCUIKeyModifierNone];
    [textView typeText:@"transcript\r"];

    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];

    NSURL *url = [testBundle URLForResource:@"Transparent command script"
                              withExtension:@"txt"
                               subdirectory:nil];
    NSString *commandScript = [UITests stringAtURL:url];

    NSArray<NSString *> *commandArray = [commandScript componentsSeparatedByString:@"\n"];
    for (NSString *command in commandArray) {
        NSLog(@"Command: %@", command);
        NSString *returnString = [command stringByAppendingString:@"\n"];
        [textView typeKey:@" " modifierFlags:XCUIKeyModifierNone];
        [textView typeKey:XCUIKeyboardKeyDelete modifierFlags:XCUIKeyModifierNone];
        [textView typeText:returnString];
        [textView typeKey:@" " modifierFlags:XCUIKeyModifierNone];
        [textView typeKey:XCUIKeyboardKeyDelete modifierFlags:XCUIKeyModifierNone];
        [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
        [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    }

    [self assertTranscriptIn:transcriptURL named:@"Transcript of Transparent.txt" equalsFacitFor:@"Transparent"];
}

- (void)testTADS3 {
    XCUIApplication *app = [UITests app];

    XCUIElement *licensewinWindow = app/*@START_MENU_TOKEN@*/.windows[@"licenseWin"]/*[[".windows[@\"Fonts License\"]",".windows[@\"licenseWin\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;
    if (licensewinWindow.exists) {
        XCUIElementQuery *menuBarsQuery = [UITests app].menuBars;
        [menuBarsQuery.menuBarItems[@"Help"] click];
        [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"GNU Public License"]/*[[".menuBarItems[@\"Help\"]",".menus.menuItems[@\"GNU Public License\"]",".menuItems[@\"GNU Public License\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
        [app/*@START_MENU_TOKEN@*/.windows[@"licenseWin"]/*[[".windows[@\"GNU Public License\"]",".windows[@\"licenseWin\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/.buttons[@"Understood"] click];

    }

    [self playGame:@"Elysium.t3"];

    XCUIElement *gameWindow = app.windows[@"The Elysium Enigma"];
    XCUIElement *textView = [UITests textViewOfWindow:gameWindow scrollViewIndex:0];

    [UITests turnOnDeterminism:@"Zoom"];

    // Wait for initial text to show
    XCTAssertTrue([UITests waitForElement:textView matching:@"value CONTAINS 'The Elysium Enigma by Eric Eve; version 2.03 (2007-03-05)'" timeout:5]);

    [textView typeKey:@" " modifierFlags:XCUIKeyModifierNone];
    [textView typeText:@"script\r"];

    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"The Elysium Enigma"];

    XCUIElement *textView2 = [gameWindow.textViews elementBoundByIndex:1];

    XCTAssertTrue([UITests waitForElement:textView2 matching:@"value CONTAINS '30/422'" timeout:80]);

    [self assertTranscriptIn:transcriptURL named:@"Transcript of The Elysium Enigma.txt" equalsFacitFor:@"The Elysium Enigma"];
}

- (void)testTADS2 {
    XCUIApplication *app = [UITests app];

    XCUIElement *gameWindow = [self playGame:@"tildeath.gam"];
    XCUIElement *textView = [UITests textViewOfWindow:gameWindow scrollViewIndex:0];

    [UITests turnOnDeterminism:@"Default"];

    XCTAssert([self waitForElement:textView toExistWithTimeout:10]);
    [textView typeText:@"script\r"];

    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"Till Death Makes a Monk-Fish out of Me"];

    XCTAssertTrue([UITests waitForElement:textView matching:@"value ENDSWITH 'Please enter RESTORE, RESTART, QUIT, UNDO or AMUSING:  >'" timeout:80]);

    [self assertTranscriptIn:transcriptURL named:@"Transcript of tildeath.gam.txt" equalsFacitFor:@"Till Death Makes a Monk-Fish out of Me"];
}

- (void)testAlan2 {
    XCUIApplication *app = [UITests app];

    [self playGame:@"bugged.acd"];

    XCUIElement *gameWindow = app.windows[@"Bugged"];
    if (!gameWindow.exists)
        gameWindow = app.windows[@"bugged.acd"];

    [UITests turnOnDeterminism:@"Default"];

    [gameWindow click];
    [self openCommandScript:@"Bugged"];

    NSString *facit = [self comparisonTranscriptFor:@"Bugged"];

    XCUIElement *textView2 = [gameWindow.staticTexts elementBoundByIndex:0];

    XCTAssertTrue([UITests waitForElement:textView2 matching:@"value CONTAINS '140 moves'" timeout:80]);

    NSString *transcript = [UITests transcriptFromFile:@"bugged.txt"];

    XCTAssert([transcript isEqualToString:facit]);
}

- (void)testAlan3 {
    XCUIApplication *app = [UITests app];

    [self playGame:@"00 Wyldkynd Project.a3c"];

    XCUIElement *gameWindow = app.windows[@"The Wyldkynd Project"];
    if (!gameWindow.exists)
        gameWindow = app.windows[@"00 Wyldkynd Project.a3c"];

    [UITests turnOnDeterminism:@"Default"];
    [gameWindow click];
    [self openCommandScript:@"The Wyldkynd Project"];

    NSString *facit = [self comparisonTranscriptFor:@"The Wyldkynd Project"];

    XCUIElement *textView = [gameWindow.staticTexts elementBoundByIndex:0];

    XCTAssertTrue([UITests waitForElement:textView matching:@"value CONTAINS '175 moves'" timeout:80]);

    NSString *transcript = [UITests transcriptFromFile:@"00 Wyldkynd Project.txt"];

    XCTAssert([transcript isEqualToString:facit]);
}

- (void)testHugo {
    XCUIApplication *app = [UITests app];

    [self playGame:@"guilty.hex"];

    XCUIElement *gameWindow = app.windows[@"Guilty Bastards"];

    [UITests turnOnDeterminism:@"Default"];

    XCUIElement *staticText = [gameWindow.staticTexts elementBoundByIndex:0];

    // Wait for initial text to show
    XCTAssertTrue([UITests waitForElement:staticText matching:@"value CONTAINS 'One of these four'" timeout:5]);

    [gameWindow typeKey:@" " modifierFlags:XCUIKeyModifierNone];

    XCUIElement *textView = [UITests textViewOfWindow:gameWindow scrollViewIndex:1];

    [textView typeText:@"script\r"];

    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"Guilty Bastards"];

    XCUIElement *textView2 = [gameWindow.staticTexts elementBoundByIndex:0];

    XCTAssertTrue([UITests waitForElement:textView2 matching:@"value CONTAINS '2:10 p.m.'" timeout:80]);

    [self assertTranscriptIn:transcriptURL named:@"Transcript of Guilty Bastards.txt" equalsFacitFor:@"Guilty Bastards"];
}

- (void)testAdvSys {
    XCUIApplication *app = [UITests app];

    [self playGame:@"onehand.dat"];

    XCUIElement *gameWindow = app.windows[@"onehand.dat"];
    XCUIElement *textView = [UITests textViewOfWindow:gameWindow scrollViewIndex:0];

    [UITests turnOnDeterminism:@"DOSBox"];

    [gameWindow click];
    [self openCommandScript:@"The Sound of One Hand Clapping"];

    NSString *facit = [self comparisonTranscriptFor:@"The Sound of One Hand Clapping"];

    XCTAssertTrue([UITests waitForElement:textView matching:@"value ENDSWITH 'Do you want to try again? '" timeout:80]);

    NSString *transcript = [UITests transcriptFromFile:@"onehand.txt"];

    XCTAssert([transcript isEqualToString:facit]);
}

- (void)testAGiliTy {
    XCUIApplication *app = [UITests app];

    [self playGame:@"AGT-03201-0000E16C.agx"];

    XCUIElement *gameWindow = app.windows[@"Shades of Gray"];
    if (!gameWindow.exists)
        gameWindow = app.windows[@"AGT-03201-0000E16C.agx"];
    XCUIElement *textView = [UITests textViewOfWindow:gameWindow scrollViewIndex:0];

    [UITests turnOnDeterminism:@"Default"];
    [textView click];

    // Wait for initial text to show
    [textView typeKey:XCUIKeyboardKeyDownArrow modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:XCUIKeyboardKeyDownArrow modifierFlags:XCUIKeyModifierCommand];

    XCTAssertTrue([UITests waitForElement:textView matching:@"value ENDSWITH 'or <other> to start the game'" timeout:5]);

    [textView typeKey:@" " modifierFlags:XCUIKeyModifierNone];
    [textView typeKey:XCUIKeyboardKeyDownArrow modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:XCUIKeyboardKeyDownArrow modifierFlags:XCUIKeyModifierCommand];

    XCTAssertTrue([UITests waitForElement:textView matching:@"value BEGINSWITH 'You wake painfully,'" timeout:5]);

    [textView typeKey:@" " modifierFlags:XCUIKeyModifierNone];
    [textView typeKey:XCUIKeyboardKeyDownArrow modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:XCUIKeyboardKeyDownArrow modifierFlags:XCUIKeyModifierCommand];

    XCTAssertTrue([UITests waitForElement:textView matching:@"value BEGINSWITH 'You are running through a dark and dreadful wood.'" timeout:5]);
    [textView typeText:@"glk script on\r"];

    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"Shades of Gray"];

    XCUIElement *textView2 = [gameWindow.staticTexts elementBoundByIndex:0];

    XCTAssertTrue([UITests waitForElement:textView2 matching:@"value CONTAINS 'Moves: 661'" timeout:80]);

    [textView typeText:@"glk script off\r"];

    [self assertTranscriptIn:transcriptURL named:@"Transcript of AGT-03201-0000E16C.agx.txt" equalsFacitFor:@"Shades of Gray"];
}

- (void)testLevel9 {
    XCUIApplication *app = [UITests app];

    XCUIElement *gameWindow = [self playGame:@"Q.L9"];
    XCUIElement *textView = [UITests textViewOfWindow:gameWindow scrollViewIndex:0];

    [UITests turnOnDeterminism:@"Default"];
    XCTAssert([self waitForElement:textView toExistWithTimeout:10]);

    [textView typeText:@"glk script on\r"];

    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"Level 9"];

    XCTAssertTrue([UITests waitForElement:textView matching:@"value ENDSWITH 'Which do you want to do, RESTART or RESTORE? '" timeout:80]);

    [textView typeText:@"glk script off\r"];

    [self assertTranscriptIn:transcriptURL named:@"Transcript of Q.l9.txt" equalsFacitFor:@"Level 9"];
}

- (void)testMagnetic {
    XCUIApplication *app = [UITests app];

    XCUIElement *gameWindow = [self playGame:@"mag.mag"];
    XCUIElement *textView = [UITests textViewOfWindow:gameWindow scrollViewIndex:0];

    [UITests turnOnDeterminism:@"Default"];
    XCTAssert([self waitForElement:textView toExistWithTimeout:10]);

    [textView typeText:@"glk script on\r"];

    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"Magnetic"];

    XCUIElement *textView2 = [gameWindow.staticTexts elementBoundByIndex:0];

    XCTAssertTrue([UITests waitForElement:textView2 matching:@"value CONTAINS '350/344'" timeout:80]);

    [textView typeText:@"glk script off\r"];

    [self assertTranscriptIn:transcriptURL named:@"Transcript of mag.mag.txt" equalsFacitFor:@"Magnetic"];
}

- (void)testAdrift {
    XCUIApplication *app = [UITests app];

    [self playGame:@"Hamper.taf"];

    XCUIElement *gameWindow = app.windows[@"To Hell in a Hamper"];
    XCUIElement *textView = [UITests textViewOfWindow:gameWindow scrollViewIndex:0];

    [UITests turnOnDeterminism:@"Default"];

    [textView click];

    [app typeKey:@" " modifierFlags:XCUIKeyModifierNone];
    [app typeKey:@" " modifierFlags:XCUIKeyModifierNone];
    [app typeKey:@" " modifierFlags:XCUIKeyModifierNone];

    [textView typeText:@" glk script on\r"];

    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"To Hell in a Hamper"];

    XCUIElement *textView2 = [gameWindow.staticTexts elementBoundByIndex:0];

    XCTAssertTrue([UITests waitForElement:textView2 matching:@"value CONTAINS 'ALTITUDE - 37,000 FEET'" timeout:80]);

    [textView click];

    [textView typeKey:@"r" modifierFlags:XCUIKeyModifierNone];

    // Scarier autosaves at each "Press any key" before it asks for the key,
    // and a key typed meanwhile is lost, so keep pressing until the restarted
    // game reaches its prompt
    for (NSUInteger i = 0; i < 10 && ![UITests waitForElement:textView matching:@"value ENDSWITH '>'" timeout:2]; i++) {
        [textView click];
        [textView typeKey:@" " modifierFlags:XCUIKeyModifierNone];
    }
    [textView typeText:@"glk script off\r"];

    [self assertTranscriptIn:transcriptURL named:@"Transcript of To Hell in a Hamper.txt" equalsFacitFor:@"To Hell in a Hamper"];
}

- (void)testAdriftPKGirl {
    XCUIApplication *app = [UITests app];

    [self playGame:@"the_pk_girl.taf"];

    XCUIElement *gameWindow = app.windows[@"The PK Girl"];
    XCUIElement *textView = [UITests textViewOfWindow:gameWindow scrollViewIndex:0];

    [UITests turnOnDeterminism:@"Default"];

    [textView click];

    // The PK Girl shows a title screen and then waits at a line prompt
    // ("Press Enter to begin the game"). Spaces do not submit a line, so wait
    // for the large game to finish loading, then submit a dummy line (a lone
    // space, which the game discards) to advance to the game proper before
    // turning the transcript on.
    XCTAssertTrue([UITests waitForElement:textView matching:@"value CONTAINS 'Press Enter to begin'" timeout:30]);

    [textView typeText:@" \r"];
    [textView typeText:@" glk script on\r"];

    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"The PK Girl"];

    // The command script plays through the Chapter 1 kidnapping and into
    // Chapter 3, ending at the "Front of Monika's House" room. Waiting for that
    // (distinctive, end-only) room name tells us the whole script has finished.
    XCTAssertTrue([UITests waitForElement:textView matching:@"value CONTAINS 'Front of Monika'" timeout:180]);

    [textView click];
    [textView typeText:@"glk script off\r"];

    [self assertTranscriptIn:transcriptURL named:@"Transcript of The PK Girl.txt" equalsFacitFor:@"The PK Girl"];
}

- (void)testCzech {
    [self playGame:@"czech.z5"];

    [UITests selectTheme:@"Default"];
    XCUIApplication *app = [UITests app];

    [app/*@START_MENU_TOKEN@*/.toolbars.buttons[@"Details"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.toolbars.buttons[@\"Details\"]",".toolbars.buttons[@\"Details\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];

    XCUIElement *preferences = app.dialogs[@"preferences"];
    XCUIElement *field = [[preferences childrenMatchingType:XCUIElementTypeTextField] elementBoundByIndex:0];
    NSString *value = field.value;
    [field doubleClick];
    [field typeKey:XCUIKeyboardKeyDelete modifierFlags:XCUIKeyModifierNone];
    [field typeText:@"70\r"];
    [field doubleClick];
    [field typeText:value];
    [field typeKey:XCUIKeyboardKeyEnter modifierFlags:XCUIKeyModifierNone];

    [app typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [app typeKey:@"r" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierOption];

    NSURL *transcriptURL = [UITests transcriptWithFormat:@"Rich Text Format"];
    [self assertRTFTranscriptIn:transcriptURL named:@"czech.z5 (finished).rtf" equalsFacitFor:@"Transcript of CZECH"];
}

- (void)testPraxix {
    [self playGame:@"praxix.z5"];

    [UITests selectTheme:@"Default"];
    XCUIApplication *app = [UITests app];
    [app typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [app typeKey:@"r" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierOption];

    XCUIElement *gameWindow = app.windows[@"praxix.z5"];
    XCUIElement *textView = [UITests textViewOfWindow:gameWindow scrollViewIndex:0];
    XCTAssert([self waitForElement:textView toExistWithTimeout:10]);
    [textView typeText:@"all\n"];

    NSString *facitName;

    if (@available(macOS 12.0, *)) {
        facitName = @"Transcript of Praxix";
    } else {
        facitName = @"Transcript of Praxix old";
    }

    NSURL *transcriptURL = [UITests transcriptWithFormat:@"Rich Text Format"];
    [self assertRTFTranscriptIn:transcriptURL named:@"praxix.rtf" equalsFacitFor:facitName];
}

- (void)testTerpEtude {
    [self playGame:@"etude.z5"];

    XCUIApplication *app = [UITests app];
    XCUIElement *gameWindow = app.windows[@"etude.z5"];
    XCUIElement *alertSheet = gameWindow.sheets[@"alert"];
    if ([self waitForElement:alertSheet toExistWithTimeout:2]) {
        [alertSheet.checkBoxes[@"Remember this choice."] click];
        [alertSheet.buttons[@"Continue"] click];
    }

    [UITests selectTheme:@"Default"];
    [app typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [app typeKey:@"r" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierOption];

    [gameWindow click];
    [self openCommandScript:@"TerpEtude"];

    NSString *facitName;

    if (@available(macOS 12.0, *)) {
        facitName = @"Transcript of TerpEtude";
    } else {
        facitName = @"Transcript of TerpEtude old";
    }

    NSURL *transcriptURL = [UITests transcriptWithFormat:@"Rich Text Format"];
    [self assertRTFTranscriptIn:transcriptURL named:@"etude.rtf" equalsFacitFor:facitName];
}

- (void)testJacl {
    XCUIApplication *app = [UITests app];

    [self playGame:@"grail.j2"];

    XCUIElement *gameWindow = app.windows[@"grail.j2"];
    XCUIElement *textView = [UITests textViewOfWindow:gameWindow scrollViewIndex:0];

    [UITests turnOnDeterminism:@"Default"];

    [textView click];

    [textView typeText:@"transcript\r"];

    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"Unholy Grail"];

    gameWindow = app.windows[@"grail.j2 (finished)"];
    XCUIElement *textView2 = [gameWindow.staticTexts elementBoundByIndex:0];

    XCTAssertTrue([UITests waitForElement:textView2 matching:@"value BEGINSWITH ' Game Over '" timeout:80]);

    [self assertTranscriptIn:transcriptURL named:@"Transcript of grail.j2.txt" equalsFacitFor:@"Unholy Grail"];
}

- (void)testScott {
    XCUIApplication *app = [UITests app];

    [self playGame:@"adv01.dat"];

    XCUIElement *gameWindow = app.windows[@"adv01.dat"];
    XCUIElement *scrollView = [gameWindow.scrollViews elementBoundByIndex:0];
    XCUIElement *textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;

    [UITests turnOnDeterminism:@"Default"];

    [textView click];
    [textView typeText:@"transcript\r"];
    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"ScottFree"];

    gameWindow = app.windows[@"adv01.dat (finished)"];
    textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;
    XCTAssertTrue([UITests waitForElement:textView matching:@"value CONTAINS 'transcript off'" timeout:25]);

    [self assertTranscriptIn:transcriptURL named:@"Transcript of adv01.dat.txt" equalsFacitFor:@"adv01"];
}

- (void)testTaylor {
    XCUIApplication *app = [UITests app];

    [self playGame:@"tot.tay"];

    XCUIElement *gameWindow = app.windows[@"tot.tay"];
    if (![gameWindow exists])
        gameWindow = app.windows[@"Temple of Terror"];
    XCUIElement *scrollView = [gameWindow.scrollViews elementBoundByIndex:0];
    XCUIElement *textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;

    [UITests turnOnDeterminism:@"Default"];

    [textView click];
    [textView typeText:@"transcript\r"];
    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"TaylorMade"];

    gameWindow = app.windows[@"tot.tay"];
    textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;
    XCTAssertTrue([UITests waitForElement:textView matching:@"value CONTAINS 'TRANSCRIPT OFF'" timeout:25]);

    [self assertTranscriptIn:transcriptURL named:@"Transcript of tot.tay.txt" equalsFacitFor:@"tot"];
}

- (void)testPlus {
    XCUIApplication *app = [UITests app];

    [self playGame:@"SPL13P.plus"];

    XCUIElement *gameWindow = app.windows[@"SPL13P.plus"];
    XCUIElement *scrollView = [gameWindow.scrollViews elementBoundByIndex:0];
    XCUIElement *textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;

    [UITests turnOnDeterminism:@"Default"];

    [textView click];
    [textView typeText:@"transcript\r"];
    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [textView click];
    [self openCommandScript:@"Plus"];

    gameWindow = app.windows[@"SPL13P.plus"];
    textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;
    XCTAssertTrue([UITests waitForElement:textView matching:@"value CONTAINS 'TRANSCRIPT OFF'" timeout:25]);

    [self assertTranscriptIn:transcriptURL named:@"Transcript of SPL13P.plus.txt" equalsFacitFor:@"SPL13P"];
}


- (void)testEditMenu {
    XCUIApplication *app = [UITests app];

    XCUIElementQuery *menuBarsQuery = app.menuBars;

    [self playGame:@"curses.z5"];

    XCUIElement *gameWindow = app.windows[@"curses.z5"];

    XCUIElement *scrollView = gameWindow.scrollViews[@"buffer scroll view"];

    XCUIElement *textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;

    // Reset any autorestored game running
    [textView typeKey:@"r" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierOption];

    [textView typeText:@" "];
    [textView typeText:@" "];

    [textView typeText:@" i"];

    XCUIElement *editMenuBarItem = menuBarsQuery.menuBarItems[@"Edit"];
    [editMenuBarItem click];

    [editMenuBarItem click];
    [menuBarsQuery.menuItems[@"Select All"] click];

    [editMenuBarItem click];

    XCUIElement *cutMenuItem = menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Cut"]/*[[".menuBarItems[@\"Edit\"]",".menus.menuItems[@\"Cut\"]",".menuItems[@\"Cut\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/;
    [cutMenuItem click];

    [editMenuBarItem click];

    XCUIElement *pasteMenuItem = menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Paste"]/*[[".menuBarItems[@\"Edit\"]",".menus.menuItems[@\"Paste\"]",".menuItems[@\"Paste\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/;
    [pasteMenuItem click];
    [textView typeText:@"\r"];
    [editMenuBarItem click];
    [pasteMenuItem click];
    [editMenuBarItem click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Select All"]/*[[".menuBarItems[@\"Edit\"]",".menus.menuItems[@\"Select All\"]",".menuItems[@\"Select All\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [editMenuBarItem click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Delete"]/*[[".menuBarItems[@\"Edit\"]",".menus.menuItems[@\"Delete\"]",".menuItems[@\"Delete\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [editMenuBarItem click];
    [menuBarsQuery.menuItems[@"Find…"] click];
    [gameWindow/*@START_MENU_TOKEN@*/.searchFields[@"Find"]/*[[".scrollViews[@\"buffer scroll view\"].searchFields[@\"Find\"]",".searchFields[@\"Find\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/ typeText:@"attic\r"];
    [editMenuBarItem click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Start Speaking"]/*[[".menuBarItems[@\"Edit\"]",".menuItems[@\"Speech\"]",".menus.menuItems[@\"Start Speaking\"]",".menuItems[@\"Start Speaking\"]"],[[[-1,3],[-1,2],[-1,1,2],[-1,0,1]],[[-1,3],[-1,2],[-1,1,2]],[[-1,3],[-1,2]]],[0]]@END_MENU_TOKEN@*/ click];
    [editMenuBarItem click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"startDictation:"]/*[[".menuBarItems[@\"Edit\"]",".menus",".menuItems[@\"Start Dictation\\U2026\"]",".menuItems[@\"startDictation:\"]"],[[[-1,3],[-1,2],[-1,1,2],[-1,0,1]],[[-1,3],[-1,2],[-1,1,2]],[[-1,3],[-1,2]]],[0]]@END_MENU_TOKEN@*/ click];
    [editMenuBarItem click];

    XCUIElement *orderfrontcharacterpaletteMenuItem = menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"orderFrontCharacterPalette:"]/*[[".menuBarItems[@\"Edit\"]",".menus",".menuItems[@\"Emoji & Symbols\"]",".menuItems[@\"orderFrontCharacterPalette:\"]"],[[[-1,3],[-1,2],[-1,1,2],[-1,0,1]],[[-1,3],[-1,2],[-1,1,2]],[[-1,3],[-1,2]]],[0]]@END_MENU_TOKEN@*/;
    [orderfrontcharacterpaletteMenuItem click];

    [textView click];
    [textView typeKey:@"a" modifierFlags:XCUIKeyModifierCommand];
    [textView rightClick];
    XCUIElement *copyMenuItem = gameWindow.textViews.menuItems[@"Copy"];
    [copyMenuItem click];
    [textView typeText:@"xyzzy"];
    [textView typeKey:@"a" modifierFlags:XCUIKeyModifierCommand];
    [textView rightClick];
    XCUIElement *pasteMenuItem2 = gameWindow.textViews.menuItems[@"Paste"];
    [pasteMenuItem2 click];
    [textView doubleClick];
    [textView click];
    [textView typeText:@"xyzzy"];
    [textView typeKey:@"a" modifierFlags:XCUIKeyModifierCommand];
    [textView rightClick];
    [gameWindow/*@START_MENU_TOKEN@*/.textViews.menuItems[@"Cut"]/*[[".scrollViews[@\"buffer scroll view\"].textViews",".menus.menuItems[@\"Cut\"]",".menuItems[@\"Cut\"]",".textViews"],[[[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0,0]]@END_MENU_TOKEN@*/ click];
    [textView typeText:@" "];
    [textView typeKey:@"a" modifierFlags:XCUIKeyModifierCommand];
    [textView rightClick];
    pasteMenuItem2 = gameWindow.textViews.menuItems[@"Paste"];
    [pasteMenuItem2 click];
    [textView typeText:@"\r"];

    XCUIElement *closeButton = scrollView/*@START_MENU_TOKEN@*/.buttons[@"Done"]/*[[".scrollViews[@\"buffer scroll view\"].buttons[@\"Done\"]",".buttons[@\"Done\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;
    if (closeButton.exists && closeButton.hittable)
        [closeButton click];

    [textView click];
    [textView typeText:@"xyzzy"];

    [editMenuBarItem click];
    XCUIElement *jumpToSelectionMenuItem = menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Jump to Selection"]/*[[".menuBarItems[@\"Edit\"]",".menuItems[@\"Find\"]",".menus.menuItems[@\"Jump to Selection\"]",".menuItems[@\"Jump to Selection\"]"],[[[-1,3],[-1,2],[-1,1,2],[-1,0,1]],[[-1,3],[-1,2],[-1,1,2]],[[-1,3],[-1,2]]],[0]]@END_MENU_TOKEN@*/;
    [jumpToSelectionMenuItem click];
    [editMenuBarItem click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Find Next"]/*[[".menuBarItems[@\"Edit\"]",".menuItems[@\"Find\"]",".menus.menuItems[@\"Find Next\"]",".menuItems[@\"Find Next\"]"],[[[-1,3],[-1,2],[-1,1,2],[-1,0,1]],[[-1,3],[-1,2],[-1,1,2]],[[-1,3],[-1,2]]],[0]]@END_MENU_TOKEN@*/ click];
    [editMenuBarItem click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Find Previous"]/*[[".menuBarItems[@\"Edit\"]",".menuItems[@\"Find\"]",".menus.menuItems[@\"Find Previous\"]",".menuItems[@\"Find Previous\"]"],[[[-1,3],[-1,2],[-1,1,2],[-1,0,1]],[[-1,3],[-1,2],[-1,1,2]],[[-1,3],[-1,2]]],[0]]@END_MENU_TOKEN@*/ click];
    [editMenuBarItem click];
    [jumpToSelectionMenuItem click];


    XCUIElement *viewMenuBarItem = menuBarsQuery.menuBarItems[@"View"];
    [viewMenuBarItem click];

    XCUIElement *zoomInMenuItem = menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Zoom In"]/*[[".menuBarItems[@\"View\"]",".menus.menuItems[@\"Zoom In\"]",".menuItems[@\"Zoom In\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/;
    [zoomInMenuItem click];
    [viewMenuBarItem click];
    [zoomInMenuItem click];
    [viewMenuBarItem click];
    [zoomInMenuItem click];
    [viewMenuBarItem click];
    [zoomInMenuItem click];
    [viewMenuBarItem click];
    [zoomInMenuItem click];

    [textView typeKey:@"+" modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:@"+" modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:@"+" modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:@"+" modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:@"+" modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:@"+" modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:@"+" modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:@"+" modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:@"+" modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:@"+" modifierFlags:XCUIKeyModifierCommand];
    [viewMenuBarItem click];

    XCUIElement *zoomOutMenuItem = menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Zoom Out"]/*[[".menuBarItems[@\"View\"]",".menus.menuItems[@\"Zoom Out\"]",".menuItems[@\"Zoom Out\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/;
    [zoomOutMenuItem click];
    [viewMenuBarItem click];
    [zoomOutMenuItem click];
    [textView typeKey:@"-" modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:@"-" modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:@"-" modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:@"-" modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:@"-" modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:@"-" modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:@"-" modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:@"-" modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:@"-" modifierFlags:XCUIKeyModifierCommand];
    [viewMenuBarItem click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Actual Size"]/*[[".menuBarItems[@\"View\"]",".menus.menuItems[@\"Actual Size\"]",".menuItems[@\"Actual Size\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [viewMenuBarItem click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Enter Full Screen"]/*[[".menuBarItems[@\"View\"]",".menus.menuItems[@\"Enter Full Screen\"]",".menuItems[@\"Enter Full Screen\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [textView typeKey:@"f" modifierFlags:(XCUIKeyModifierCommand | XCUIKeyModifierControl)];

    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];

    XCUIElementQuery *menuBarsQuery2 = app.menuBars;
    XCUIElement *helpMenuBarItem = menuBarsQuery2.menuBarItems[@"Help"];
    [helpMenuBarItem click];

    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"GNU Public License"]/*[[".menuBarItems[@\"Help\"]",".menus.menuItems[@\"GNU Public License\"]",".menuItems[@\"GNU Public License\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [helpMenuBarItem click];

    XCUIElement *glulxeLicenseMenuItem = menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Glulxe License"]/*[[".menuBarItems[@\"Help\"]",".menus.menuItems[@\"Glulxe License\"]",".menuItems[@\"Glulxe License\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/;
    [glulxeLicenseMenuItem click];
    [helpMenuBarItem click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Hugo License"]/*[[".menuBarItems[@\"Help\"]",".menus.menuItems[@\"Hugo License\"]",".menuItems[@\"Hugo License\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [helpMenuBarItem click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Alan License"]/*[[".menuBarItems[@\"Help\"]",".menus.menuItems[@\"Alan License\"]",".menuItems[@\"Alan License\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [helpMenuBarItem click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Fonts License"]/*[[".menuBarItems[@\"Help\"]",".menus.menuItems[@\"Fonts License\"]",".menuItems[@\"Fonts License\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];

    XCUIElement *licensewinWindow = app/*@START_MENU_TOKEN@*/.windows[@"licenseWin"]/*[[".windows[@\"Fonts License\"]",".windows[@\"licenseWin\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;
    [licensewinWindow.buttons[@"Copy to Clipboard"] click];
    [licensewinWindow.buttons[@"Understood"] click];
}

- (void)testContextualMenus {
    XCUIApplication *app = [UITests app];

    XCUIElementQuery *menuBarsQuery = app.menuBars;

    XCUIElement *libraryWindow = app/*@START_MENU_TOKEN@*/.windows[@"library"]/*[[".windows[@\"Interactive Fiction\"]",".windows[@\"library\"]"],[[[-1,1],[-1,0]]],[1]]@END_MENU_TOKEN@*/;

    XCUIElement *textField = [self addAndSelectGame:@"curses.z5"];
    [textField rightClick];
    [libraryWindow/*@START_MENU_TOKEN@*/.tables[@"Games"].menuItems[@"Play"]/*[[".splitGroups[@\"SplitViewTotal\"]",".scrollViews.tables[@\"Games\"]",".menus.menuItems[@\"Play\"]",".menuItems[@\"Play\"]",".tables[@\"Games\"]"],[[[-1,4,2],[-1,1,2],[-1,0,1]],[[-1,4,2],[-1,1,2]],[[-1,3],[-1,2]]],[0,0]]@END_MENU_TOKEN@*/ click];
    [app.windows[@"curses.z5"].buttons[XCUIIdentifierCloseWindow] click];

    [textField click];
    [textField rightClick];
    [libraryWindow/*@START_MENU_TOKEN@*/.tables[@"Games"].menuItems[@"Download Info"]/*[[".splitGroups[@\"SplitViewTotal\"]",".scrollViews.tables[@\"Games\"]",".menus.menuItems[@\"Download Info\"]",".menuItems[@\"Download Info\"]",".tables[@\"Games\"]"],[[[-1,4,2],[-1,1,2],[-1,0,1]],[[-1,4,2],[-1,1,2]],[[-1,3],[-1,2]]],[0,0]]@END_MENU_TOKEN@*/ click];

    XCUIElement *imageDialog = app.dialogs[@"alert"];

    BOOL result = [self waitForElement:imageDialog toExistWithTimeout:5];

    if (result) {
        [imageDialog.buttons[@"Yes"] click];
    }

    [textField rightClick];
    [libraryWindow.tables[@"Games"].menuItems[@"Show Info"] click];

    XCUIElement *infoWin = app.windows[@"Curses Info"];
    [menuBarsQuery.menuBarItems[@"Window"] click];
    [menuBarsQuery.menuItems[@"Curses Info"] click];
    XCUIElement *image = [[infoWin childrenMatchingType:XCUIElementTypeImage] matchingIdentifier:@"coverImageView"].firstMatch;
    [self forceClickElement:image];
    [infoWin.menuItems[@"saveImage:"] click];

    XCUIElement *saveDialog = app.sheets.firstMatch;
    XCUIElement *saveButton = saveDialog.buttons[@"Save"];
    [saveButton click];

    XCUIElement *alert = app.sheets.firstMatch;
    if (alert.exists) {
        [alert.buttons[@"Replace"] click];
    }

    [self forceClickElement:image];
    [infoWin.menuItems[@"Cut"] click];
    [self forceClickElement:image];
    [infoWin.menuItems[@"Paste"] click];
    [self forceClickElement:image];
    [infoWin.menuItems[@"Copy"] click];
    [self forceClickElement:image];
    [infoWin.menuItems[@"Delete"] click];

    alert = app.dialogs.firstMatch;
    XCTAssert([self waitForElement:alert toExistWithTimeout:5]);
    [alert.buttons[@"Delete"] click];

    image = [infoWin childrenMatchingType:XCUIElementTypeImage].firstMatch;

    [self forceClickElement:image];
    [infoWin.menuItems[@"Paste"] click];
    [self forceClickElement:image];
    [infoWin.menuItems[@"Filter"] click];
    [self forceClickElement:image];
    [infoWin.menuItems[@"Add description"] click];

    alert = app.dialogs.firstMatch;
    XCTAssert([self waitForElement:alert toExistWithTimeout:5]);
    [alert.textFields.firstMatch typeText:@"Some ancient, broken, rocks with a subway map of Paris in front."];
    [alert.buttons[@"Okay"] click];

    [infoWin.buttons[XCUIIdentifierCloseWindow] click];

    [textField rightClick];
    [libraryWindow/*@START_MENU_TOKEN@*/.tables[@"Games"].menuItems[@"Lectrote Dark"]/*[[".splitGroups[@\"SplitViewTotal\"]",".scrollViews.tables[@\"Games\"]",".menuItems[@\"Apply Theme\"]",".menus.menuItems[@\"Lectrote Dark\"]",".menuItems[@\"Lectrote Dark\"]",".tables[@\"Games\"]"],[[[-1,5,2],[-1,1,2],[-1,0,1]],[[-1,5,2],[-1,1,2]],[[-1,4],[-1,3],[-1,2,3]],[[-1,4],[-1,3]]],[0,0]]@END_MENU_TOKEN@*/ click];
    [textField rightClick];
    [libraryWindow.tables[@"Games"].menuItems[@"Lectrote"] click];
    [textField rightClick];
    [libraryWindow.tables[@"Games"].menuItems[@"Spatterlight Classic"] click];
    [textField rightClick];
    [libraryWindow.tables[@"Games"].menuItems[@"Zoom"] click];
    [textField rightClick];
    [libraryWindow/*@START_MENU_TOKEN@*/.tables[@"Games"].menuItems[@"Select Same Theme"]/*[[".splitGroups[@\"SplitViewTotal\"]",".scrollViews.tables[@\"Games\"]",".menus.menuItems[@\"Select Same Theme\"]",".menuItems[@\"Select Same Theme\"]",".tables[@\"Games\"]"],[[[-1,4,2],[-1,1,2],[-1,0,1]],[[-1,4,2],[-1,1,2]],[[-1,3],[-1,2]]],[0,0]]@END_MENU_TOKEN@*/ click];
    [textField rightClick];
    [libraryWindow.tables[@"Games"].menuItems[@"Reveal in Finder"] click];
    [app terminate];
    // Quit and restart the app to get back from Finder
    [app launch];
    XCTAssert([self waitForElement:app toExistWithTimeout:5]);
    [menuBarsQuery.menuBarItems[@"Window"] click];
    [menuBarsQuery.menuItems[@"Interactive Fiction"] click];
    [textField rightClick];
    [libraryWindow.tables[@"Games"].menuItems[@"Open Ifdb page"] click];
    [app terminate];
    // Quit and restart the app to get back from Finder
    [app launch];
    XCTAssert([self waitForElement:app toExistWithTimeout:5]);
    [menuBarsQuery.menuBarItems[@"Window"] click];
    [menuBarsQuery.menuItems[@"Interactive Fiction"] click];
    [textField rightClick];
    [libraryWindow.tables[@"Games"].menuItems[@"Delete Autosaves"] click];


    [textField rightClick];
    [libraryWindow.tables[@"Games"].menuItems[@"Delete Game"] click];

    [menuBarsQuery.menuBarItems[@"Edit"] click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Undo"]/*[[".menuBarItems[@\"Edit\"]",".menus.menuItems[@\"Undo\"]",".menuItems[@\"Undo\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];

    [menuBarsQuery.menuBarItems[@"File"] click];
    XCUIElement *cursesItem = menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Open Recent"].menuItems[@"curses.z5"]/*[[".menuBarItems[@\"File\"]",".menuItems[@\"Open Recent\"]",".menus.menuItems[@\"curses.z5\"]",".menuItems[@\"curses.z5\"]",".menus.menuItems[@\"Open Recent\"]"],[[[-1,1,2],[-1,4,2],[-1,0,1]],[[-1,3],[-1,2],[-1,1,2]],[[-1,3],[-1,2]]],[0,0]]@END_MENU_TOKEN@*/;
    if (cursesItem.exists)
        [cursesItem click];

    XCUIElement *fileMenuBarItem = menuBarsQuery.menuBarItems[@"File"];
    [fileMenuBarItem click];
    [menuBarsQuery.menuItems[@"Open…"] click];

    NSURL *url = [testBundle URLForResource:@"imagetest"
                              withExtension:@"gblorb"
                               subdirectory:nil];

    [UITests typeURL:url intoApp:app andPressButtonWithText:@"Open"];

    [menuBarsQuery.menuBarItems[@"Window"] click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Interactive Fiction"]/*[[".menuBarItems[@\"Window\"]",".menus.menuItems[@\"Interactive Fiction\"]",".menuItems[@\"Interactive Fiction\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];

    XCUIElement *searchField = libraryWindow.searchFields.firstMatch;

    [searchField click];
    
    XCUIElement *cancelButton = searchField.buttons[@"cancel"];
    if (cancelButton.exists)
        [cancelButton click];
    [searchField typeText:@"imagetest"];

    [textField click];

    [libraryWindow.toolbars.buttons[@"Show info"] click];


    infoWin = app.windows[@"imagetest.gblorb Info"];
    [menuBarsQuery.menuBarItems[@"Window"] click];
    [menuBarsQuery.menuItems[@"imagetest.gblorb Info"] click];
    [infoWin click];
    image = [[[infoWin childrenMatchingType:XCUIElementTypeAny] matchingIdentifier:@"coverImageView"] elementBoundByIndex:0];
    [self forceClickElement:image];
    [infoWin.menuItems[@"Select Image File…"] click];

    url = [testBundle URLForResource:@"imagetest"
                       withExtension:@"gblorb"
                        subdirectory:nil];

    NSURL *path = url.URLByDeletingLastPathComponent;
    path = [path URLByAppendingPathComponent:@"curses.png" isDirectory:NO];

    XCUIElement *openDialog = app.sheets.firstMatch;
    XCTAssert([self waitForElement:openDialog toExistWithTimeout:5]);
    [UITests typeURL:path intoFileDialog:openDialog andPressButtonWithText:@"Open"];

    [self forceClickElement:image];
    [infoWin.menuItems[@"Reload from Blorb"] click];
}

- (void)forceClickElement:(XCUIElement *)element {
    XCUICoordinate *coordinate = [element coordinateWithNormalizedOffset:CGVectorMake(0.1, 0.1)];
    [coordinate rightClick];
}

- (void)testPreferences {

    XCUIApplication *app = [UITests app];

    [self addAndSelectGame:@"curses.z5"];

    XCUIElement *libraryWindow = app/*@START_MENU_TOKEN@*/.windows[@"library"]/*[[".windows[@\"Interactive Fiction\"]",".windows[@\"library\"]"],[[[-1,1],[-1,0]]],[1]]@END_MENU_TOKEN@*/;
    XCUIElement *playButton = libraryWindow.toolbars.buttons[@"Play"];
    [playButton click];

    [app typeKey:@"," modifierFlags:XCUIKeyModifierCommand];

    XCUIElement *preferences = app.dialogs[@"preferences"];
    XCUIElement *themesTab = app.toolbars.buttons[@"Themes"];
    [themesTab click];

    XCUIElementQuery *tables = preferences.tables;


    [tables.staticTexts[@"Default"] click];
    [tables.staticTexts[@"DOSBox"] click];
    [tables.staticTexts[@"Gargoyle"] click];
    [tables.staticTexts[@"Lectrote"] click];
    [tables.staticTexts[@"Lectrote Dark"] click];
    [tables.staticTexts[@"Montserrat"] click];
    [tables.staticTexts[@"MS-DOS"] click];
    [tables.staticTexts[@"Spatterlight Classic"] click];
    [tables.staticTexts[@"Zoom"] click];

    [preferences.toolbars.buttons[@"Glk Styles"] click];

    XCUIElementQuery *tabGroupsQuery = app.groups;

    XCUIElementQuery *buttonQuery = [tabGroupsQuery childrenMatchingType:XCUIElementTypeButton] ;

    [[buttonQuery elementBoundByIndex:0] click];

    XCUIElement *xcuiClosewindowButton = app.windows[@"Fonts"].buttons[XCUIIdentifierCloseWindow];
    [xcuiClosewindowButton click];
    [[buttonQuery elementBoundByIndex:1] click];
    [[[tabGroupsQuery childrenMatchingType:XCUIElementTypePopUpButton] elementBoundByIndex:0] click];
    [app.menuItems[@"Buffer"] click];
    [[[tabGroupsQuery childrenMatchingType:XCUIElementTypePopUpButton] elementBoundByIndex:1] click];
    [app.menuItems[@"Normal"] click];
    [[buttonQuery elementBoundByIndex:4] click];
    [xcuiClosewindowButton click];
    [[[tabGroupsQuery childrenMatchingType:XCUIElementTypeColorWell] elementBoundByIndex:4] click];
    [app.windows[@"Colors"].toolbars.buttons[@"Pencils"] click];
    [app.windows[@"Colors"]/*@START_MENU_TOKEN@*/.radioButtons[@"Lavender"]/*[[".splitGroups",".radioGroups[@\"Pencils\"].radioButtons[@\"Lavender\"]",".radioButtons[@\"Lavender\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [app.windows[@"Colors"].buttons[XCUIIdentifierCloseWindow] click];

    [[[[tabGroupsQuery childrenMatchingType:XCUIElementTypeButton] matchingIdentifier:@"action"] elementBoundByIndex:1] click];

    XCUIElementQuery *popoversQuery = /*@START_MENU_TOKEN@*/app.popovers/*[["app",".dialogs[@\"Preferences\"]",".tabGroups","[",".buttons matchingIdentifier:@\"action\"].popovers",".popovers",".dialogs[@\"preferences\"]"],[[[-1,0,1]],[[-1,5],[3,4],[-1,2,3],[-1,6,2],[-1,1,2]],[[-1,5],[3,4],[-1,2,3]],[[-1,5],[3,4]]],[0,0]]@END_MENU_TOKEN@*/;
    XCUIElement *textField = [[[popoversQuery childrenMatchingType:XCUIElementTypeTextField] matchingIdentifier:@"0"] elementBoundByIndex:0];
    [textField typeText:@"10"];

    XCUIElement *stepper = [[popoversQuery childrenMatchingType:XCUIElementTypeStepper] elementBoundByIndex:2];
    XCUIElement *incrementArrow = [stepper childrenMatchingType:XCUIElementTypeIncrementArrow].element;
    [incrementArrow click];
    
    XCUIElement *decrementArrow = [stepper childrenMatchingType:XCUIElementTypeDecrementArrow].element;
    [decrementArrow click];
    [incrementArrow click];
    [incrementArrow click];
    [incrementArrow click];
    [incrementArrow click];
    
    XCUIElement *textField2 = [[[popoversQuery childrenMatchingType:XCUIElementTypeTextField] matchingIdentifier:@"0"] elementBoundByIndex:1];
    [textField2 click];
    [textField2 typeText:@"1\r"];

    XCUIElement *incrementArrow2 = [[[popoversQuery childrenMatchingType:XCUIElementTypeStepper] elementBoundByIndex:0] childrenMatchingType:XCUIElementTypeIncrementArrow].element;
    [incrementArrow2 click];
    [incrementArrow2 click];
    [textField click];
    [incrementArrow click];
    [textField click];
    [textField typeText:@"10"];
    [incrementArrow click];
    [decrementArrow click];

    XCUIElement *textField3 = [[[popoversQuery childrenMatchingType:XCUIElementTypeTextField] matchingIdentifier:@"0"] elementBoundByIndex:2];
    [textField3 click];
    [textField3 typeText:@"10\r"];

    XCUIElement *incrementArrow3 = [[[popoversQuery childrenMatchingType:XCUIElementTypeStepper] elementBoundByIndex:3] childrenMatchingType:XCUIElementTypeIncrementArrow].element;
    [incrementArrow3 click];
    [incrementArrow3 click];
    
    XCUIElement *textField4 = [[[popoversQuery childrenMatchingType:XCUIElementTypeTextField] matchingIdentifier:@"0"] elementBoundByIndex:3];
    [textField4 click];
    [textField4 typeText:@"10\r"];
    [[[[popoversQuery childrenMatchingType:XCUIElementTypeStepper] elementBoundByIndex:1] childrenMatchingType:XCUIElementTypeIncrementArrow].element click];
    [textField click];
    [textField typeText:@"12"];
    [incrementArrow click];
    [decrementArrow click];

    XCUIElement *stylesTab = app.toolbars.buttons[@"Glk Styles"];
    [app/*@START_MENU_TOKEN@*/.toolbars.buttons[@"Themes"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.toolbars.buttons[@\"Themes\"]",".toolbars.buttons[@\"Themes\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [stylesTab click];

    XCUIElement *actionButton = [[[tabGroupsQuery childrenMatchingType:XCUIElementTypeButton] matchingIdentifier:@"action"] elementBoundByIndex:0];

    [actionButton click];

    textField = [[[app/*@START_MENU_TOKEN@*/.popovers/*[[".dialogs[@\"Preferences\"]",".tabGroups.popovers",".popovers",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ childrenMatchingType:XCUIElementTypeTextField] matchingIdentifier:@"0"] elementBoundByIndex:0];
    [textField click];
    [textField typeText:@"10\r"];

    incrementArrow = [[[popoversQuery childrenMatchingType:XCUIElementTypeStepper] elementBoundByIndex:4] childrenMatchingType:XCUIElementTypeIncrementArrow].element;
    [incrementArrow click];
    [incrementArrow click];
    
    incrementArrow2 = [[[popoversQuery childrenMatchingType:XCUIElementTypeStepper] elementBoundByIndex:10] childrenMatchingType:XCUIElementTypeIncrementArrow].element;
    [incrementArrow2 click];
    [incrementArrow2 doubleClick];
    [incrementArrow2 click];
    
    incrementArrow3 = [[[popoversQuery childrenMatchingType:XCUIElementTypeStepper] elementBoundByIndex:0] childrenMatchingType:XCUIElementTypeIncrementArrow].element;
    [incrementArrow3 click];
    [incrementArrow3 click];
    
    XCUIElement *incrementArrow4 = [[[popoversQuery childrenMatchingType:XCUIElementTypeStepper] elementBoundByIndex:5] childrenMatchingType:XCUIElementTypeIncrementArrow].element;
    [incrementArrow4 click];
    [incrementArrow4 click];
    
    XCUIElement *incrementArrow5 = [[[popoversQuery childrenMatchingType:XCUIElementTypeStepper] elementBoundByIndex:1] childrenMatchingType:XCUIElementTypeIncrementArrow].element;
    [incrementArrow5 click];
    [incrementArrow5 click];
    [incrementArrow5 click];
    [incrementArrow5 click];

    XCUIElement *incrementArrow6 = [[[popoversQuery childrenMatchingType:XCUIElementTypeStepper] elementBoundByIndex:3] childrenMatchingType:XCUIElementTypeIncrementArrow].element;
    [incrementArrow6 click];
    [incrementArrow6 click];
    [incrementArrow6 click];
    [incrementArrow6 click];
    [incrementArrow6 click];
    [incrementArrow6 click];
    [incrementArrow6 click];
    [incrementArrow6 click];
    [incrementArrow6 click];
    [incrementArrow6 click];
    [[[popoversQuery childrenMatchingType:XCUIElementTypeCheckBox] elementBoundByIndex:3] click];
    [incrementArrow6 click];
    [incrementArrow6 click];
    
    XCUIElement *incrementArrow7 = [[[popoversQuery childrenMatchingType:XCUIElementTypeStepper] elementBoundByIndex:2] childrenMatchingType:XCUIElementTypeIncrementArrow].element;
    [incrementArrow7 click];
    [incrementArrow7 click];
    [incrementArrow7 click];
    [incrementArrow7 click];
    [incrementArrow7 click];
    [incrementArrow7 click];
    [incrementArrow7 click];
    [incrementArrow7 click];
    
    XCUIElement *incrementArrow8 = [[[popoversQuery childrenMatchingType:XCUIElementTypeStepper] elementBoundByIndex:7] childrenMatchingType:XCUIElementTypeIncrementArrow].element;
    [incrementArrow8 click];
    [incrementArrow8 click];
    
    XCUIElement *incrementArrow9 = [[[popoversQuery childrenMatchingType:XCUIElementTypeStepper] elementBoundByIndex:6] childrenMatchingType:XCUIElementTypeIncrementArrow].element;
    [incrementArrow9 click];
    [incrementArrow9 click];

    XCUIElement *incrementArrow10 = [[[popoversQuery childrenMatchingType:XCUIElementTypeStepper] elementBoundByIndex:8] childrenMatchingType:XCUIElementTypeIncrementArrow].element;
    [incrementArrow10 click];
    [incrementArrow10 click];
    [incrementArrow10 click];
    [incrementArrow10 click];
    [incrementArrow10 click];
    [incrementArrow10 click];
    [incrementArrow10 click];
    [incrementArrow10 click];
    [incrementArrow10 click];
    [[[[popoversQuery childrenMatchingType:XCUIElementTypeStepper] elementBoundByIndex:9] childrenMatchingType:XCUIElementTypeIncrementArrow].element click];

    [actionButton click];
    [app/*@START_MENU_TOKEN@*/.buttons[@"Un-customize styles"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.buttons[@\"Un-customize styles\"]",".buttons[@\"Un-customize styles\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    XCUIElement *alert = app/*@START_MENU_TOKEN@*/.sheets[@"alert"]/*[[".dialogs[@\"Preferences\"].sheets[@\"alert\"]",".dialogs[@\"preferences\"].sheets[@\"alert\"]",".sheets[@\"alert\"]"],[[[-1,2],[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;
    if (alert.exists)
        [alert.buttons[@"Okay"] click];
    [app/*@START_MENU_TOKEN@*/.toolbars.buttons[@"Details"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.toolbars.buttons[@\"Details\"]",".toolbars.buttons[@\"Details\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [app/*@START_MENU_TOKEN@*/.checkBoxes[@"Enable sound"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.checkBoxes[@\"Enable sound\"]",".checkBoxes[@\"Enable sound\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [app/*@START_MENU_TOKEN@*/.checkBoxes[@"Enable graphics"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.checkBoxes[@\"Enable graphics\"]",".checkBoxes[@\"Enable graphics\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [app/*@START_MENU_TOKEN@*/.checkBoxes[@"Smart quotes"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.checkBoxes[@\"Smart quotes\"]",".checkBoxes[@\"Smart quotes\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [app/*@START_MENU_TOKEN@*/.checkBoxes[@"No double spaces"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.checkBoxes[@\"No double spaces\"]",".checkBoxes[@\"No double spaces\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];

    textField = [[preferences childrenMatchingType:XCUIElementTypeTextField] elementBoundByIndex:0];
    [textField doubleClick];
    [textField typeText:@"40\r"];
    
    textField2 = [[preferences childrenMatchingType:XCUIElementTypeTextField] elementBoundByIndex:1];
    [textField2 doubleClick];
    [textField2 typeText:@"200\r"];

    XCUIElement *textField5 = [[preferences childrenMatchingType:XCUIElementTypeTextField] elementBoundByIndex:2];
    [textField5 click];
    [textField5 typeText:@"1\r"];

    XCUIElement *colorWell = [preferences childrenMatchingType:XCUIElementTypeColorWell].element;
    [colorWell click];
    [app/*@START_MENU_TOKEN@*/.checkBoxes[@"Automatic"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.checkBoxes[@\"Automatic\"]",".checkBoxes[@\"Automatic\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [colorWell click];
    [app.windows[@"Colors"]/*@START_MENU_TOKEN@*/.radioButtons[@"Licorice"]/*[[".splitGroups",".radioGroups[@\"Pencils\"].radioButtons[@\"Licorice\"]",".radioButtons[@\"Licorice\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [app.windows[@"Colors"].buttons[XCUIIdentifierCloseWindow] click];

    textField3 = [[preferences childrenMatchingType:XCUIElementTypeTextField] elementBoundByIndex:2];
    [textField3 click];
    [textField3 typeText:@"0\r"];
    [textField3 typeText:@"1000\r"];

    [[[preferences childrenMatchingType:XCUIElementTypePopUpButton] element] click];
    [app/*@START_MENU_TOKEN@*/.menuItems[@"Show and wait for key"]/*[[".dialogs[@\"Preferences\"]",".tabGroups",".popUpButtons",".menus.menuItems[@\"Show and wait for key\"]",".menuItems[@\"Show and wait for key\"]",".dialogs[@\"preferences\"]"],[[[-1,4],[-1,3],[-1,2,3],[-1,1,2],[-1,5,1],[-1,0,1]],[[-1,4],[-1,3],[-1,2,3],[-1,1,2]],[[-1,4],[-1,3],[-1,2,3]],[[-1,4],[-1,3]]],[0]]@END_MENU_TOKEN@*/ click];
    [app.checkBoxes[@"Animate scrolling"] click];

    XCUIElement *formatTab = app.toolbars.buttons[@"Format"];
    [formatTab click];

    [[app.popUpButtons elementMatchingType:XCUIElementTypePopUpButton identifier:@"arrowKeysPopup"] click];
    [app.menuItems[@"Replaced by \u2318↑ and \u2318↓"] click];

    [[app.popUpButtons elementMatchingType:XCUIElementTypePopUpButton identifier:@"highBeepPopup"] click];
    [app/*@START_MENU_TOKEN@*/.menuItems[@"Funky"]/*[[".dialogs[@\"Preferences\"]",".tabGroups",".popUpButtons[@\"arrowKeysPopup\"]",".menus.menuItems[@\"Funky\"]",".menuItems[@\"Funky\"]",".dialogs[@\"preferences\"]"],[[[-1,4],[-1,3],[-1,2,3],[-1,1,2],[-1,5,1],[-1,0,1]],[[-1,4],[-1,3],[-1,2,3],[-1,1,2]],[[-1,4],[-1,3],[-1,2,3]],[[-1,4],[-1,3]]],[0]]@END_MENU_TOKEN@*/ click];

    [[app.popUpButtons elementMatchingType:XCUIElementTypePopUpButton identifier:@"lowBeepPopup"] click];
    [app/*@START_MENU_TOKEN@*/.menuItems[@"Bubble"]/*[[".dialogs[@\"Preferences\"]",".tabGroups",".popUpButtons",".menus.menuItems[@\"Bubble\"]",".menuItems[@\"Bubble\"]",".dialogs[@\"preferences\"]"],[[[-1,4],[-1,3],[-1,2,3],[-1,1,2],[-1,5,1],[-1,0,1]],[[-1,4],[-1,3],[-1,2,3],[-1,1,2]],[[-1,4],[-1,3],[-1,2,3]],[[-1,4],[-1,3]]],[0]]@END_MENU_TOKEN@*/ click];

    [[app.popUpButtons elementMatchingType:XCUIElementTypePopUpButton identifier:@"systemPopup"] click];

    [app/*@START_MENU_TOKEN@*/.menuItems[@"1. DECSystem-20"]/*[[".dialogs[@\"Preferences\"]",".tabGroups",".popUpButtons",".menus.menuItems[@\"1. DECSystem-20\"]",".menuItems[@\"1. DECSystem-20\"]",".dialogs[@\"preferences\"]"],[[[-1,4],[-1,3],[-1,2,3],[-1,1,2],[-1,5,1],[-1,0,1]],[[-1,4],[-1,3],[-1,2,3],[-1,1,2]],[[-1,4],[-1,3],[-1,2,3]],[[-1,4],[-1,3]]],[0]]@END_MENU_TOKEN@*/ click];

    textField = [[preferences childrenMatchingType:XCUIElementTypeTextField] elementBoundByIndex:0];
    [textField click];
    [textField typeText:@"ÅÖL\r"];

    textField2 = [[preferences childrenMatchingType:XCUIElementTypeTextField] elementBoundByIndex:1];
    [textField2 click];
    [textField2 typeText:@"1000\r"];
    [textField2 typeKey:XCUIKeyboardKeyDelete modifierFlags:XCUIKeyModifierNone];
    [textField2 typeText:@"\r"];
    [textField2 typeKey:XCUIKeyboardKeyDelete modifierFlags:XCUIKeyModifierNone];
    [textField2 typeText:@"\r"];
    [app/*@START_MENU_TOKEN@*/.checkBoxes[@"Fancy quote boxes"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.checkBoxes[@\"Fancy quote boxes\"]",".checkBoxes[@\"Fancy quote boxes\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];

    [app/*@START_MENU_TOKEN@*/.toolbars.buttons[@"VoiceOver"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.toolbars.buttons[@\"VoiceOver\"]",".toolbars.buttons[@\"VoiceOver\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [[app.popUpButtons elementMatchingType:XCUIElementTypePopUpButton identifier:@"speakMenuLinesPopup"] click];
    [app/*@START_MENU_TOKEN@*/.menuItems[@"Text, index, and total"]/*[[".dialogs[@\"Preferences\"]",".tabGroups",".popUpButtons",".menus.menuItems[@\"Text, index, and total\"]",".menuItems[@\"Text, index, and total\"]",".dialogs[@\"preferences\"]"],[[[-1,4],[-1,3],[-1,2,3],[-1,1,2],[-1,5,1],[-1,0,1]],[[-1,4],[-1,3],[-1,2,3],[-1,1,2]],[[-1,4],[-1,3],[-1,2,3]],[[-1,4],[-1,3]]],[0]]@END_MENU_TOKEN@*/ click];

    [[app.popUpButtons elementMatchingType:XCUIElementTypePopUpButton identifier:@"speakImagesPopup"] click];
    [app/*@START_MENU_TOKEN@*/.menuItems[@"All"]/*[[".dialogs[@\"Preferences\"]",".tabGroups",".popUpButtons",".menus.menuItems[@\"All\"]",".menuItems[@\"All\"]",".dialogs[@\"preferences\"]"],[[[-1,4],[-1,3],[-1,2,3],[-1,1,2],[-1,5,1],[-1,0,1]],[[-1,4],[-1,3],[-1,2,3],[-1,1,2]],[[-1,4],[-1,3],[-1,2,3]],[[-1,4],[-1,3]]],[0]]@END_MENU_TOKEN@*/ click];
    [app/*@START_MENU_TOKEN@*/.checkBoxes[@"Speak commands"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.checkBoxes[@\"Speak commands\"]",".checkBoxes[@\"Speak commands\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [app/*@START_MENU_TOKEN@*/.toolbars.buttons[@"Misc"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.toolbars.buttons[@\"Misc\"]",".toolbars.buttons[@\"Misc\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];

    textField4 = [preferences childrenMatchingType:XCUIElementTypeTextField].element;
    [textField4 doubleClick];
    [textField4 typeText:@"5000\r"];
    [textField4 typeKey:XCUIKeyboardKeyDelete modifierFlags:XCUIKeyModifierNone];
    [textField4 typeKey:XCUIKeyboardKeyDelete modifierFlags:XCUIKeyModifierNone];
    [textField4 typeText:@"\r"];

    [[[preferences childrenMatchingType:XCUIElementTypePopUpButton] element] click];
    [app.menuItems[@"Ignore errors"] click];

    XCUIElement *autosaveAndAutorestoreCheckBox = app/*@START_MENU_TOKEN@*/.checkBoxes[@"Autosave and autorestore"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.checkBoxes[@\"Autosave and autorestore\"]",".checkBoxes[@\"Autosave and autorestore\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/;
    [autosaveAndAutorestoreCheckBox click];
    [autosaveAndAutorestoreCheckBox click];

    XCUIElement *autosaveOnTimerEventsCheckBox = app/*@START_MENU_TOKEN@*/.checkBoxes[@"Autosave on timer events"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.checkBoxes[@\"Autosave on timer events\"]",".checkBoxes[@\"Autosave on timer events\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/;
    [autosaveOnTimerEventsCheckBox click];
    [autosaveOnTimerEventsCheckBox click];

    [app/*@START_MENU_TOKEN@*/.checkBoxes[@"Disable game-specific hacks"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.checkBoxes[@\"Disable game-specific hacks\"]",".checkBoxes[@\"Disable game-specific hacks\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [app/*@START_MENU_TOKEN@*/.checkBoxes[@"Determinism"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.checkBoxes[@\"Determinism\"]",".checkBoxes[@\"Determinism\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];

    [app/*@START_MENU_TOKEN@*/.toolbars.buttons[@"Themes"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.toolbars.buttons[@\"Themes\"]",".toolbars.buttons[@\"Themes\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [app/*@START_MENU_TOKEN@*/.checkBoxes[@"Changes apply to all games in library"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.checkBoxes[@\"Changes apply to all games in library\"]",".checkBoxes[@\"Changes apply to all games in library\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    alert = app.sheets[@"alert"];
    if (alert.exists)
        [alert.buttons[@"Okay"] click];

    XCUIElement *actionMenuButton = app/*@START_MENU_TOKEN@*/.menuButtons[@"action"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.menuButtons[@\"action\"]",".menuButtons[@\"action\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/;
    XCTAssert([self waitForElement:actionMenuButton toExistWithTimeout:5]);
    [actionMenuButton click];
    [app/*@START_MENU_TOKEN@*/.menuItems[@"deleteUserThemes:"]/*[[".dialogs[@\"Preferences\"]",".tabGroups",".menuButtons[@\"action\"]",".menus",".menuItems[@\"Delete All User Themes\"]",".menuItems[@\"deleteUserThemes:\"]",".dialogs[@\"preferences\"]"],[[[-1,5],[-1,4],[-1,3,4],[-1,2,3],[-1,1,2],[-1,6,1],[-1,0,1]],[[-1,5],[-1,4],[-1,3,4],[-1,2,3],[-1,1,2]],[[-1,5],[-1,4],[-1,3,4],[-1,2,3]],[[-1,5],[-1,4],[-1,3,4]],[[-1,5],[-1,4]]],[0]]@END_MENU_TOKEN@*/ click];
    [actionMenuButton click];
    [app/*@START_MENU_TOKEN@*/.menuItems[@"rebuildDefaultThemes:"]/*[[".dialogs[@\"Preferences\"]",".tabGroups",".menuButtons[@\"action\"]",".menus",".menuItems[@\"Rebuild Default Themes\"]",".menuItems[@\"rebuildDefaultThemes:\"]",".dialogs[@\"preferences\"]"],[[[-1,5],[-1,4],[-1,3,4],[-1,2,3],[-1,1,2],[-1,6,1],[-1,0,1]],[[-1,5],[-1,4],[-1,3,4],[-1,2,3],[-1,1,2]],[[-1,5],[-1,4],[-1,3,4],[-1,2,3]],[[-1,5],[-1,4],[-1,3,4]],[[-1,5],[-1,4]]],[0]]@END_MENU_TOKEN@*/ click];
    [actionMenuButton click];
    [app/*@START_MENU_TOKEN@*/.menuItems[@"togglePreview:"]/*[[".dialogs[@\"Preferences\"]",".tabGroups",".menuButtons[@\"action\"]",".menus",".menuItems[@\"Show Preview\"]",".menuItems[@\"togglePreview:\"]",".dialogs[@\"preferences\"]"],[[[-1,5],[-1,4],[-1,3,4],[-1,2,3],[-1,1,2],[-1,6,1],[-1,0,1]],[[-1,5],[-1,4],[-1,3,4],[-1,2,3],[-1,1,2]],[[-1,5],[-1,4],[-1,3,4],[-1,2,3]],[[-1,5],[-1,4],[-1,3,4]],[[-1,5],[-1,4]]],[0]]@END_MENU_TOKEN@*/ click];
}

/// Relaunches the app on a fresh test library holding only the game, which
/// the app adds and selects at launch (see TableViewController
/// -addTestGames). This is much faster than adding it through the open panel
/// and finding it with the search field. Call it first thing in a test, as
/// the relaunch loses all earlier state.
- (XCUIElement *)addAndSelectGame:(NSString *)game {
    XCUIApplication *app = [self relaunchWithGame:game environmentKey:@"SPATTERLIGHT_TEST_ADD_GAMES"];

    XCUIElement *gamesTableRow = [UITests firstGameRowOf:app];
    XCTAssert([self waitForElement:gamesTableRow toBeHittableWithTimeout:30]);
    return gamesTableRow;
}

/// As addAndSelectGame:, but the app also starts the game, as when it is
/// double-clicked in the library. The game stays selected in the library.
/// Returns the game window.
- (XCUIElement *)playGame:(NSString *)game {
    XCUIApplication *app = [self relaunchWithGame:game environmentKey:@"SPATTERLIGHT_TEST_PLAY_GAME"];
    // Only games with autosave get the window identifier gameWin<hash> (see
    // GameLauncher), so wait for any window that is not one of the app's own
    NSPredicate *notAppWindow = [NSPredicate predicateWithFormat:@"NOT (identifier IN {'library', 'preferences', 'licenseWin'}) AND NOT (identifier BEGINSWITH 'infoWin')"];
    XCUIElement *gameWindow = [app.windows matchingPredicate:notAppWindow].firstMatch;
    XCTAssert([self waitForElement:gameWindow toExistWithTimeout:30]);
    return gameWindow;
}

- (XCUIApplication *)relaunchWithGame:(NSString *)game environmentKey:(NSString *)key {
    XCUIApplication *app = [UITests app];

    NSURL *url = [testBundle URLForResource:game.stringByDeletingPathExtension
                              withExtension:game.pathExtension
                               subdirectory:nil];
    XCTAssertNotNil(url, @"%@ is missing from the test bundle", game);

    [app terminate];
    app.launchEnvironment = @{ @"SPATTERLIGHT_TEST_STORE": @"UITests",
                               @"SPATTERLIGHT_TEST_STORE_FRESH": @"1",
                               key: url.path };
    [app launch];

    // The library window may have been closed when the app last quit
    XCUIElement *libraryWindow = app.windows[@"library"];
    if (![self waitForElement:libraryWindow toExistWithTimeout:5]) {
        [app.menuBars.menuBarItems[@"Window"] click];
        [app.menuBars.menuItems[@"Interactive Fiction"] click];
    }
    return app;
}

+ (XCUIElement *)firstGameRowOf:(XCUIApplication *)app {
    XCUIElement *gamesTable = app.windows[@"library"].tables[@"Games"];
    return [[gamesTable.tableRows childrenMatchingType:XCUIElementTypeCell] elementBoundByIndex:0];
}


- (void)testLaunchPerformance {
    if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, watchOS 7.0, *)) {
        // This measures how long it takes to launch your application.
        [self measureWithMetrics:@[[[XCTApplicationLaunchMetric alloc] init]] block:^{
            [[UITests app] launch];
        }];
    }
}

@end
