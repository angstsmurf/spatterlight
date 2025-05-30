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

    XCUIApplication *app = [[XCUIApplication alloc] init];
    [app launch];

    XCUIElementQuery *menuBarsQuery = app.menuBars;

    XCUIElement *fileMenuBarItem = menuBarsQuery.menuBarItems[@"File"];
    [fileMenuBarItem click];
    [menuBarsQuery.menuItems[@"Delete Library…"] click];
    XCUIElement *alertDialog = app.dialogs.firstMatch;
    XCTAssert([alertDialog waitForExistenceWithTimeout:5]);
    XCUIElement *checkbox = alertDialog.checkBoxes[@"Force quit running games and delete them."];
    if (checkbox.exists && [checkbox.value isEqual:@0])
        [checkbox click];
    [alertDialog.buttons[@"Delete Library"] click];

    // In UI tests it’s important to set the initial state - such as interface orientation - required for your tests before they run. The setUp method is a good place to do this.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
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

    XCTAssert([openButton waitForExistenceWithTimeout:5]);
    [openButton click];
}

+ (void)typeURL:(NSURL *)url intoApp:(XCUIApplication *)app andPressButtonWithText:(NSString *)buttonText
{
    [UITests typeURL:url intoApp:app];

    XCUIElement *openButton = app.buttons[buttonText].firstMatch;

    XCTAssert([openButton waitForExistenceWithTimeout:5]);
    [openButton click];
}

+ (void)typeURL:(NSURL *)url intoFileDialog:(XCUIElement *)dialog {
    [dialog typeKey:@"g" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierShift];

    XCUIElement *sheet = dialog.sheets.firstMatch;
    if (!sheet.exists) {
        XCTAssert([sheet waitForExistenceWithTimeout:5]);
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

    XCTAssert([sheet waitForExistenceWithTimeout:5]);

    [sheet typeText:url.path];
    [sheet typeKey:XCUIKeyboardKeyEnter modifierFlags:XCUIKeyModifierNone];
}

+ (NSString *)transcriptFromFile:(NSString *)fileName {
    NSURL *url = [UITests transcriptWithFormat:@"Plain Text"];

    url = [url URLByAppendingPathComponent:fileName isDirectory:NO];

    NSError *error = nil;
    NSString *comparison = [NSString stringWithContentsOfURL:url encoding:NSUTF8StringEncoding error:&error];

    if (error)
        NSLog(@"Error: %@", error);

    return comparison;
}

+ (NSAttributedString *)attributedTranscriptFromFile:(NSString *)fileName {
    NSURL *url = [UITests transcriptWithFormat:@"Plain Text"];

    url = [url URLByAppendingPathComponent:fileName isDirectory:NO];

    NSError *error = nil;
    NSAttributedString *comparison = [[NSAttributedString alloc] initWithURL:url options:@{NSDocumentTypeDocumentAttribute:NSRTFTextDocumentType} documentAttributes:nil error:&error];

    if (error)
        NSLog(@"Error: %@", error);

    return comparison;
}

+ (NSURL *)transcriptWithFormat:(NSString *)format {

    XCUIApplication *app = [[XCUIApplication alloc] init];
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

    XCUIApplication *app = [[XCUIApplication alloc] init];
    NSURL *transcriptURL = [UITests tempDir];

    NSLog(@"Transcript is in directory %@", transcriptURL.path);

    XCUIElement *savePanel = gameWin.sheets.firstMatch;
    if (!savePanel.exists) {
        XCTAssert([savePanel waitForExistenceWithTimeout:5]);
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
    XCUIApplication *app = [[XCUIApplication alloc] init];

    [app.toolbars.buttons[@"Details"] click];
    [app.checkBoxes[@"Animate scrolling"] click];

    [app.toolbars.buttons[@"Misc"] click];
    [app.checkBoxes[@"Determinism"] click];

    [app typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [app typeKey:@"r" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierOption];
}

+ (void)selectTheme:(nullable NSString *)theme {
    if (!theme.length)
        theme = @"Default";
    XCUIApplication *app = [[XCUIApplication alloc] init];
    [app typeKey:@"," modifierFlags:XCUIKeyModifierCommand];
    XCUIElement *themesTab = app.toolbars.buttons[@"Themes"];
    if (themesTab.exists)
        [themesTab click];
    [app.tables.staticTexts[theme] click];
}

- (void)openCommandScript:(NSString *)name {
    XCUIApplication *app = [[XCUIApplication alloc] init];

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

    NSError *error = nil;
    NSString *facit = [NSString stringWithContentsOfURL:url encoding:NSUTF8StringEncoding error:&error];
    if (error)
        NSLog(@"Error: %@", error);
    return facit;
}

//- (void)testTetrisAutorestore {
//
//    XCUIApplication *app = [[XCUIApplication alloc] init];
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
//    [self waitForExpectations:@[expectation] timeout:5];
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
//    XCUIApplication *app = [[XCUIApplication alloc] init];
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
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElementQuery *menuBarsQuery = app.menuBars;

    XCUIElement *fileMenuBarItem = menuBarsQuery.menuBarItems[@"File"];
    if(![fileMenuBarItem waitForExistenceWithTimeout:5] || !fileMenuBarItem.hittable) {
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

        XCTAssert([saveDialog waitForExistenceWithTimeout:5]);

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

        XCUIElement *alert = app.sheets.firstMatch;
        if ([alert waitForExistenceWithTimeout:1]) {
            XCUIElement *replaceButton = alert.buttons[@"Replace"];
            if (replaceButton.exists)
                [replaceButton click];
            else if (alert.buttons.firstMatch.hittable) {
                [alert.buttons.firstMatch click];
            }
        }

        alert = app.dialogs[@"alert"];

        if ([alert waitForExistenceWithTimeout:1] && alert.buttons.firstMatch.hittable) {
            [alert.buttons.firstMatch click];
        }
    }
}

- (void)testMoreFileMenu {

    XCUIApplication *app = [[XCUIApplication alloc] init];

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
    XCTAssert([app waitForExistenceWithTimeout:5]);

    [menuBarsQuery.menuBarItems[@"Window"] click];
    [menuBarsQuery.menuItems[@"Interactive Fiction"] click];

    XCUIElement *libraryWindow = app/*@START_MENU_TOKEN@*/.windows[@"library"]/*[[".windows[@\"Interactive Fiction\"]",".windows[@\"library\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;

    XCUIElement *searchSearchField = app/*@START_MENU_TOKEN@*/.windows[@"library"].searchFields[@"Search"]/*[[".windows[@\"Interactive Fiction\"]",".splitGroups[@\"SplitViewTotal\"].searchFields[@\"Search\"]",".searchFields[@\"Search\"]",".windows[@\"library\"]"],[[[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0,0]]@END_MENU_TOKEN@*/ ;

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
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Play Game"]/*[[".menuBarItems[@\"File\"]",".menus.menuItems[@\"Play Game\"]",".menuItems[@\"Play Game\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    XCUIElement *gameWindow = app.windows[@"imagetest.gblorb"];
    [gameWindow click];
    [gameMenuBarItem click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Reset Game"]/*[[".menuBarItems[@\"File\"]",".menus.menuItems[@\"Reset Game\"]",".menuItems[@\"Reset Game\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [gameWindow click];
    [fileMenuBarItem click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Close Window"]/*[[".menuBarItems[@\"File\"]",".menus.menuItems[@\"Close Window\"]",".menuItems[@\"Close Window\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    libraryWindow = app/*@START_MENU_TOKEN@*/.windows[@"library"]/*[[".windows[@\"Interactive Fiction\"]",".windows[@\"library\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;
    XCTAssert([libraryWindow waitForExistenceWithTimeout:5]);
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
        XCTAssert([savePanel waitForExistenceWithTimeout:5]);
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
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElementQuery *menuBarsQuery = app.menuBars;

    [menuBarsQuery.menuItems[@"Import Metadata…"] click];

    XCUIElement *openDialog = app.sheets.firstMatch;
    XCTAssert([openDialog waitForExistenceWithTimeout:5]);

    NSURL *url = [testBundle URLForResource:@"test_stories"
                              withExtension:@"iFiction"
                               subdirectory:nil];

    [UITests typeURL:url intoFileDialog:openDialog andPressButtonWithText:@"Import"];
}

- (void)testAutosave {
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElement *libraryWindow = app/*@START_MENU_TOKEN@*/.windows[@"library"]/*[[".windows[@\"Interactive Fiction\"]",".windows[@\"library\"]"],[[[-1,1],[-1,0]]],[1]]@END_MENU_TOKEN@*/;

    XCUIElement *textField = [self addAndSelectGame:@"autosavetest.gblorb"];

    [textField doubleClick];

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
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"value CONTAINS 'Not a game.'"];
    XCTNSPredicateExpectation *expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView];

    [self waitForExpectations:@[expectation] timeout:5];

    XCUIElement *statusLine = [gameWindow.staticTexts elementBoundByIndex:0];

    CGRect windowRect = gameWindow.frame;
    CGRect scrollRect = scrollView.frame;
    CGRect statusRect = statusLine.frame;

    [textView typeText:@"run all\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];

    XCUIElement *gamesTable = libraryWindow/*@START_MENU_TOKEN@*/.tables[@"Games"]/*[[".splitGroups[@\"SplitViewTotal\"]",".scrollViews.tables[@\"Games\"]",".tables[@\"Games\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/;

    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];

    NSDate *date = [NSDate date];

    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"q"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"r"];

    NSTimeInterval interval1 = date.timeIntervalSinceNow;
    NSLog(@"Time taken: %f", interval1);

    date = [NSDate date];

    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];

    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"s"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];

    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"t"];

    NSTimeInterval interval2 = date.timeIntervalSinceNow;

    NSLog(@"Time taken: %f (accelerated) Previous unaccelerated time taken: %f Diff: %f", fabs(interval2), fabs(interval1), fabs(interval1) - fabs(interval2));
    // There might be a way to measure this, but this is not it.
    //    XCTAssert(fabs(interval1) > fabs(interval2));
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];

    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"This is a short sample text\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"This is a short sample text\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"Thıs is å shört sämplê tëxt\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"This is å shört sämplê tëxt\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"This is ä shört sämplê tëxt\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"This is ö shört sämplê tëxt\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"This is é shört sämplê tëxt\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];
    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [gamesTable typeKey:@"p" modifierFlags:XCUIKeyModifierCommand];
    [textView typeText:@"\r"];

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

    XCTAssert([comparison isEqualToString:facit]);

    NSLog(@"windowRect: %@ gameWindow.frame: %@", NSStringFromRect(windowRect), NSStringFromRect(gameWindow.frame));
    NSLog(@"scrollRect: %@ scrollView.frame: %@", NSStringFromRect(scrollRect), NSStringFromRect(scrollView.frame));
    NSLog(@"statusRect: %@ statusLine.frame: %@", NSStringFromRect(statusRect), NSStringFromRect(statusLine.frame));

    //    XCTAssert(NSEqualRects(windowRect, gameWindow.frame));
    //    XCTAssert(NSEqualRects(scrollRect, scrollView.frame));
    //    XCTAssert(NSEqualRects(statusRect, statusLine.frame));

    [textView typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    XCTAssert(!textView.exists);
}

- (void)testGlulxe {
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElement *libraryWindow = app/*@START_MENU_TOKEN@*/.windows[@"library"]/*[[".windows[@\"Interactive Fiction\"]",".windows[@\"library\"]"],[[[-1,1],[-1,0]]],[1]]@END_MENU_TOKEN@*/;

    XCUIElement *textField = [self addAndSelectGame:@"Transparent.gblorb"];

    [textField doubleClick];

    XCUIElement *gameWindow = app.windows[@"Transparent"];
    XCUIElement *scrollView = gameWindow.scrollViews[@"buffer scroll view"];
    XCUIElement *textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;

    XCUIElement *alertSheet = gameWindow.sheets[@"alert"];
    if ([alertSheet waitForExistenceWithTimeout:2]) {
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
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"value CONTAINS 'Adjust your volume.'"];
    XCTNSPredicateExpectation *expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView];

    [self waitForExpectations:@[expectation] timeout:5];

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
    NSError *error = nil;

    NSString *commandScript = [[NSString alloc] initWithContentsOfURL:url encoding:NSUTF8StringEncoding error:&error];

    if (error)
        NSLog(@"Error: %@", error);
    
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

    error = nil;

    transcriptURL = [transcriptURL URLByAppendingPathComponent:@"Transcript of Transparent.txt" isDirectory:NO];

    NSString *transcript = [NSString stringWithContentsOfURL:transcriptURL encoding:NSUTF8StringEncoding error:&error];

    if (error)
        NSLog(@"Error: %@", error);

    NSString *facit = [self comparisonTranscriptFor:@"Transparent"];

    XCTAssert([transcript isEqualToString:facit]);
}

- (void)testTADS3 {
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElement *licensewinWindow = app/*@START_MENU_TOKEN@*/.windows[@"licenseWin"]/*[[".windows[@\"Fonts License\"]",".windows[@\"licenseWin\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;
    if (licensewinWindow.exists) {
        XCUIElementQuery *menuBarsQuery = [[XCUIApplication alloc] init].menuBars;
        [menuBarsQuery.menuBarItems[@"Help"] click];
        [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"GNU Public License"]/*[[".menuBarItems[@\"Help\"]",".menus.menuItems[@\"GNU Public License\"]",".menuItems[@\"GNU Public License\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
        [app/*@START_MENU_TOKEN@*/.windows[@"licenseWin"]/*[[".windows[@\"GNU Public License\"]",".windows[@\"licenseWin\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/.buttons[@"Understood"] click];

    }

    XCUIElement *textField = [self addAndSelectGame:@"Elysium.t3"];

    [textField doubleClick];

    XCUIElement *gameWindow = app.windows[@"The Elysium Enigma"];
    XCUIElement *scrollView = [gameWindow.scrollViews elementBoundByIndex:0];
    XCUIElement *textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;

    [UITests turnOnDeterminism:@"Zoom"];

    // Wait for initial text to show
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"value CONTAINS 'The Elysium Enigma by Eric Eve; version 2.03 (2007-03-05)'"];
    XCTNSPredicateExpectation *expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView];

    [self waitForExpectations:@[expectation] timeout:5];

    [textView typeKey:@" " modifierFlags:XCUIKeyModifierNone];
    [textView typeText:@"script\r"];

    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"The Elysium Enigma"];

    NSString *facit = [self comparisonTranscriptFor:@"The Elysium Enigma"];

    XCUIElement *textView2 = [gameWindow.textViews elementBoundByIndex:1];

    predicate = [NSPredicate predicateWithFormat:@"value CONTAINS '30/422'"];
    expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView2];
    [self waitForExpectations:@[expectation] timeout:80];

    NSError *error = nil;

    transcriptURL = [transcriptURL URLByAppendingPathComponent:@"Transcript of The Elysium Enigma.txt" isDirectory:NO];

    NSString *transcript = [NSString stringWithContentsOfURL:transcriptURL encoding:NSUTF8StringEncoding error:&error];

    if (error)
        NSLog(@"Error: %@", error);

    XCTAssert([transcript isEqualToString:facit]);
}

- (void)testTADS2 {
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElement *textField = [self addAndSelectGame:@"tildeath.gam"];

    [textField doubleClick];

    XCUIElement *gameWindow = app.windows[@"tildeath.gam"];
    XCUIElement *scrollView = [gameWindow.scrollViews elementBoundByIndex:0];
    XCUIElement *textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;

    [UITests turnOnDeterminism:@"Default"];

    XCTAssert([textView waitForExistenceWithTimeout:1]);
    [textView typeText:@"script\r"];

    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"Till Death Makes a Monk-Fish out of Me"];

    NSString *facit = [self comparisonTranscriptFor:@"Till Death Makes a Monk-Fish out of Me"];

    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"value ENDSWITH 'Please enter RESTORE, RESTART, QUIT, UNDO or AMUSING:  >'"];
    XCTNSPredicateExpectation *expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView];
    [self waitForExpectations:@[expectation] timeout:80];

    transcriptURL = [transcriptURL URLByAppendingPathComponent:@"Transcript of tildeath.gam.txt" isDirectory:NO];

    NSError *error = nil;

    NSString *transcript = [NSString stringWithContentsOfURL:transcriptURL encoding:NSUTF8StringEncoding error:&error];

    if (error)
        NSLog(@"Error: %@", error);

    XCTAssert([transcript isEqualToString:facit]);
}

- (void)testAlan2 {
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElement *textField = [self addAndSelectGame:@"bugged.acd"];

    [textField doubleClick];

    XCUIElement *gameWindow = app.windows[@"Bugged"];
    if (!gameWindow.exists)
        gameWindow = app.windows[@"bugged.acd"];

    [UITests turnOnDeterminism:@"Default"];

    [gameWindow click];
    [self openCommandScript:@"Bugged"];

    NSString *facit = [self comparisonTranscriptFor:@"Bugged"];

    XCUIElement *textView2 = [gameWindow.staticTexts elementBoundByIndex:0];

    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"value CONTAINS '140 moves'"];
    XCTNSPredicateExpectation *expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView2];
    [self waitForExpectations:@[expectation] timeout:80];

    NSString *transcript = [UITests transcriptFromFile:@"bugged.txt"];

    XCTAssert([transcript isEqualToString:facit]);
}

- (void)testAlan3 {
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElement *textField = [self addAndSelectGame:@"00 Wyldkynd Project.a3c"];

    [textField doubleClick];

    XCUIElement *gameWindow = app.windows[@"The Wyldkynd Project"];
    if (!gameWindow.exists)
        gameWindow = app.windows[@"00 Wyldkynd Project.a3c"];

    [UITests turnOnDeterminism:@"Default"];
    [gameWindow click];
    [self openCommandScript:@"The Wyldkynd Project"];

    NSString *facit = [self comparisonTranscriptFor:@"The Wyldkynd Project"];

    XCUIElement *textView = [gameWindow.staticTexts elementBoundByIndex:0];

    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"value CONTAINS '175 moves'"];
    XCTNSPredicateExpectation *expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView];
    [self waitForExpectations:@[expectation] timeout:80];

    NSString *transcript = [UITests transcriptFromFile:@"00 Wyldkynd Project.txt"];

    XCTAssert([transcript isEqualToString:facit]);
}

- (void)testHugo {
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElement *textField = [self addAndSelectGame:@"guilty.hex"];

    [textField doubleClick];

    XCUIElement *gameWindow = app.windows[@"Guilty Bastards"];

    [UITests turnOnDeterminism:@"Default"];

    XCUIElement *staticText = [gameWindow.staticTexts elementBoundByIndex:0];

    // Wait for initial text to show
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"value CONTAINS 'One of these four'"];

    XCTNSPredicateExpectation *expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:staticText];

    [self waitForExpectations:@[expectation] timeout:5];

    [gameWindow typeKey:@" " modifierFlags:XCUIKeyModifierNone];

    XCUIElement *scrollView = [gameWindow.scrollViews elementBoundByIndex:1];
    XCUIElement *textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;

    [textView typeText:@"script\r"];

    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"Guilty Bastards"];

    NSString *facit = [self comparisonTranscriptFor:@"Guilty Bastards"];

    XCUIElement *textView2 = [gameWindow.staticTexts elementBoundByIndex:0];

    predicate = [NSPredicate predicateWithFormat:@"value CONTAINS '2:08 p.m.'"];
    expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView2];
    [self waitForExpectations:@[expectation] timeout:80];

    NSError *error = nil;

    transcriptURL = [transcriptURL URLByAppendingPathComponent:@"Transcript of Guilty Bastards.txt" isDirectory:NO];

    NSString *transcript = [NSString stringWithContentsOfURL:transcriptURL encoding:NSUTF8StringEncoding error:&error];

    if (error)
        NSLog(@"Error: %@", error);

    XCTAssert([transcript isEqualToString:facit]);
}

- (void)testAdvSys {
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElement *textField = [self addAndSelectGame:@"onehand.dat"];

    [textField doubleClick];

    XCUIElement *gameWindow = app.windows[@"onehand.dat"];
    XCUIElement *scrollView = [gameWindow.scrollViews elementBoundByIndex:0];
    XCUIElement *textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;

    [UITests turnOnDeterminism:@"DOSBox"];

    [gameWindow click];
    [self openCommandScript:@"The Sound of One Hand Clapping"];

    NSString *facit = [self comparisonTranscriptFor:@"The Sound of One Hand Clapping"];

    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"value ENDSWITH 'Do you want to try again? '"];
    XCTNSPredicateExpectation *expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView];
    [self waitForExpectations:@[expectation] timeout:80];

    NSString *transcript = [UITests transcriptFromFile:@"onehand.txt"];

    XCTAssert([transcript isEqualToString:facit]);
}

- (void)testAGiliTy {
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElement *textField = [self addAndSelectGame:@"AGT-03201-0000E16C.agx"];

    [textField doubleClick];

    XCUIElement *gameWindow = app.windows[@"Shades of Gray"];
    if (!gameWindow.exists)
        gameWindow = app.windows[@"AGT-03201-0000E16C.agx"];
    XCUIElement *scrollView = [gameWindow.scrollViews elementBoundByIndex:0];
    XCUIElement *textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;

    [UITests turnOnDeterminism:@"Default"];
    [textView click];

    // Wait for initial text to show
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"value ENDSWITH 'or <other> to start the game'"];
    XCTNSPredicateExpectation *expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView];

    [textView typeKey:XCUIKeyboardKeyDownArrow modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:XCUIKeyboardKeyDownArrow modifierFlags:XCUIKeyModifierCommand];

    [self waitForExpectations:@[expectation] timeout:5];

    [textView typeKey:@" " modifierFlags:XCUIKeyModifierNone];
    [textView typeKey:XCUIKeyboardKeyDownArrow modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:XCUIKeyboardKeyDownArrow modifierFlags:XCUIKeyModifierCommand];

    predicate = [NSPredicate predicateWithFormat:@"value BEGINSWITH 'You wake painfully,'"];
    expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView];
    [self waitForExpectations:@[expectation] timeout:5];

    [textView typeKey:@" " modifierFlags:XCUIKeyModifierNone];
    [textView typeKey:XCUIKeyboardKeyDownArrow modifierFlags:XCUIKeyModifierCommand];
    [textView typeKey:XCUIKeyboardKeyDownArrow modifierFlags:XCUIKeyModifierCommand];

    predicate = [NSPredicate predicateWithFormat:@"value BEGINSWITH 'You are running through a dark and dreadful wood.'"];
    expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView];
    [self waitForExpectations:@[expectation] timeout:5];
    [textView typeText:@"glk script on\r"];

    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"Shades of Gray"];

    NSString *facit = [self comparisonTranscriptFor:@"Shades of Gray"];

    XCUIElement *textView2 = [gameWindow.staticTexts elementBoundByIndex:0];

    predicate = [NSPredicate predicateWithFormat:@"value CONTAINS 'Moves: 654'"];
    expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView2];
    [self waitForExpectations:@[expectation] timeout:80];

    [textView typeText:@"glk script off\r"];

    NSError *error = nil;

    transcriptURL = [transcriptURL URLByAppendingPathComponent:@"Transcript of AGT-03201-0000E16C.agx.txt" isDirectory:NO];

    NSString *transcript = [NSString stringWithContentsOfURL:transcriptURL encoding:NSUTF8StringEncoding error:&error];

    if (error)
        NSLog(@"Error: %@", error);

    XCTAssert([transcript isEqualToString:facit]);
}

- (void)testLevel9 {
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElement *textField = [self addAndSelectGame:@"Q.L9"];

    [textField doubleClick];

    XCUIElement *gameWindow = app.windows[@"Q.L9"];
    XCUIElement *scrollView = [gameWindow.scrollViews elementBoundByIndex:0];
    XCUIElement *textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;

    [UITests turnOnDeterminism:@"Default"];
    XCTAssert([textView waitForExistenceWithTimeout:1]);

    [textView typeText:@"glk script on\r"];

    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"Level 9"];

    NSString *facit = [self comparisonTranscriptFor:@"Level 9"];

    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"value ENDSWITH 'Which do you want to do, RESTART or RESTORE? '"];
    XCTNSPredicateExpectation *expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView];
    [self waitForExpectations:@[expectation] timeout:80];

    [textView typeText:@"glk script off\r"];

    NSError *error = nil;

    transcriptURL = [transcriptURL URLByAppendingPathComponent:@"Transcript of Q.l9.txt" isDirectory:NO];

    NSString *transcript = [NSString stringWithContentsOfURL:transcriptURL encoding:NSUTF8StringEncoding error:&error];

    if (error)
        NSLog(@"Error: %@", error);

    XCTAssert([transcript isEqualToString:facit]);
}

- (void)testMagnetic {
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElement *textField = [self addAndSelectGame:@"mag.mag"];

    [textField doubleClick];

    XCUIElement *gameWindow = app.windows[@"mag.mag"];
    XCUIElement *scrollView = [gameWindow.scrollViews elementBoundByIndex:0];
    XCUIElement *textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;

    [UITests turnOnDeterminism:@"Default"];
    XCTAssert([textView waitForExistenceWithTimeout:1]);

    [textView typeText:@"glk script on\r"];

    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"Magnetic"];

    NSString *facit = [self comparisonTranscriptFor:@"Magnetic"];

    XCUIElement *textView2 = [gameWindow.staticTexts elementBoundByIndex:0];

    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"value CONTAINS '350/344'"];
    XCTNSPredicateExpectation *expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView2];
    [self waitForExpectations:@[expectation] timeout:80];

    NSError *error = nil;

    [textView typeText:@"glk script off\r"];

    transcriptURL = [transcriptURL URLByAppendingPathComponent:@"Transcript of mag.mag.txt" isDirectory:NO];

    NSString *transcript = [NSString stringWithContentsOfURL:transcriptURL encoding:NSUTF8StringEncoding error:&error];

    if (error)
        NSLog(@"Error: %@", error);

    XCTAssert([transcript isEqualToString:facit]);
}

- (void)testAdrift {
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElement *textField = [self addAndSelectGame:@"Hamper.taf"];

    [textField doubleClick];

    XCUIElement *gameWindow = app.windows[@"Hamper.taf"];
    XCUIElement *scrollView = [gameWindow.scrollViews elementBoundByIndex:0];
    XCUIElement *textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;

    [UITests turnOnDeterminism:@"Default"];

    [textView click];

    [app typeKey:@" " modifierFlags:XCUIKeyModifierNone];
    [app typeKey:@" " modifierFlags:XCUIKeyModifierNone];
    [app typeKey:@" " modifierFlags:XCUIKeyModifierNone];

    [textView typeText:@" glk script on\r"];

    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"To Hell in a Hamper"];

    NSString *facit = [self comparisonTranscriptFor:@"To Hell in a Hamper"];

    XCUIElement *textView2 = [gameWindow.staticTexts elementBoundByIndex:0];

    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"value CONTAINS 'ALTITUDE - 37,000 FEET'"];
    XCTNSPredicateExpectation *expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView2];
    [self waitForExpectations:@[expectation] timeout:80];

    [textView click];

    [textView typeKey:@"r" modifierFlags:XCUIKeyModifierNone];
    [textView click];
    [textView typeKey:@" " modifierFlags:XCUIKeyModifierNone];
    [textView click];
    [textView typeKey:@" " modifierFlags:XCUIKeyModifierNone];
    [textView click];
    [textView typeKey:@" " modifierFlags:XCUIKeyModifierNone];
    [textView click];
    [textView typeKey:@" " modifierFlags:XCUIKeyModifierNone];
    [textView typeText:@"glk script off\r"];

    NSError *error = nil;

    transcriptURL = [transcriptURL URLByAppendingPathComponent:@"Transcript of Hamper.taf.txt" isDirectory:NO];

    NSString *transcript = [NSString stringWithContentsOfURL:transcriptURL encoding:NSUTF8StringEncoding error:&error];

    if (error)
        NSLog(@"Error: %@", error);

    XCTAssert([transcript isEqualToString:facit]);
}

- (void)testCzech {
    XCUIElement *textField = [self addAndSelectGame:@"czech.z5"];

    [textField doubleClick];

    [UITests selectTheme:@"Default"];
    XCUIApplication *app = [[XCUIApplication alloc] init];

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

    NSURL *url = [testBundle URLForResource:@"Transcript of CZECH"
                              withExtension:@"rtf"
                               subdirectory:nil];

    NSError *error = nil;
    NSAttributedString *facit = [[NSAttributedString alloc] initWithURL:url options:@{NSDocumentTypeDocumentAttribute:NSRTFTextDocumentType} documentAttributes:nil error:&error];
    if (error)
        NSLog(@"Error: %@", error);

    NSURL *transcriptURL = [UITests transcriptWithFormat:@"Rich Text Format"];

    transcriptURL = [transcriptURL URLByAppendingPathComponent:@"czech.z5 (finished).rtf" isDirectory:NO];

    error = nil;
    NSAttributedString *comparison = [[NSAttributedString alloc] initWithURL:transcriptURL options:@{NSDocumentTypeDocumentAttribute:NSRTFTextDocumentType} documentAttributes:nil error:&error];
    if (error)
        NSLog(@"Error: %@", error);

    XCTAssert([comparison isEqual:facit]);
}

- (void)testPraxix {
    XCUIElement *textField = [self addAndSelectGame:@"praxix.z5"];

    [textField doubleClick];

    [UITests selectTheme:@"Default"];
    XCUIApplication *app = [[XCUIApplication alloc] init];
    [app typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [app typeKey:@"r" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierOption];

    XCUIElement *gameWindow = app.windows[@"praxix.z5"];
    XCUIElement *scrollView = [gameWindow.scrollViews elementBoundByIndex:0];
    XCUIElement *textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;
    XCTAssert([textView waitForExistenceWithTimeout:1]);
    [textView typeText:@"all\n"];

    NSURL *url;

    if (@available(macOS 12.0, *)) {
        url = [testBundle URLForResource:@"Transcript of Praxix"
                           withExtension:@"rtf"
                            subdirectory:nil];
    } else {
        url = [testBundle URLForResource:@"Transcript of Praxix old"
                           withExtension:@"rtf"
                            subdirectory:nil];
    }

    NSError *error = nil;
    NSAttributedString *facit = [[NSAttributedString alloc] initWithURL:url options:@{NSDocumentTypeDocumentAttribute:NSRTFTextDocumentType} documentAttributes:nil error:&error];
    if (error)
        NSLog(@"Error: %@", error);

    NSURL *transcriptURL = [UITests transcriptWithFormat:@"Rich Text Format"];

    transcriptURL = [transcriptURL URLByAppendingPathComponent:@"praxix.rtf" isDirectory:NO];

    error = nil;
    NSAttributedString *comparison = [[NSAttributedString alloc] initWithURL:transcriptURL options:@{NSDocumentTypeDocumentAttribute:NSRTFTextDocumentType} documentAttributes:nil error:&error];
    if (error)
        NSLog(@"Error: %@", error);

    XCTAssert([comparison isEqual:facit]);
}

- (void)testTerpEtude {
    XCUIElement *textField = [self addAndSelectGame:@"etude.z5"];

    [textField doubleClick];

    XCUIApplication *app = [[XCUIApplication alloc] init];
    XCUIElement *gameWindow = app.windows[@"etude.z5"];
    XCUIElement *alertSheet = gameWindow.sheets[@"alert"];
    if ([alertSheet waitForExistenceWithTimeout:2]) {
        [alertSheet.checkBoxes[@"Remember this choice."] click];
        [alertSheet.buttons[@"Continue"] click];
    }

    [UITests selectTheme:@"Default"];
    [app typeKey:@"w" modifierFlags:XCUIKeyModifierCommand];
    [app typeKey:@"r" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierOption];

    [gameWindow click];
    [self openCommandScript:@"TerpEtude"];

    NSURL *url;

    if (@available(macOS 12.0, *)) {
        url = [testBundle URLForResource:@"Transcript of TerpEtude"
                           withExtension:@"rtf"
                            subdirectory:nil];
    } else {
        url = [testBundle URLForResource:@"Transcript of TerpEtude old"
                           withExtension:@"rtf"
                            subdirectory:nil];
    }

    NSError *error = nil;
    NSAttributedString *facit = [[NSAttributedString alloc] initWithURL:url options:@{NSDocumentTypeDocumentAttribute:NSRTFTextDocumentType} documentAttributes:nil error:&error];
    if (error)
        NSLog(@"Error: %@", error);

    NSURL *transcriptURL = [UITests transcriptWithFormat:@"Rich Text Format"];

    transcriptURL = [transcriptURL URLByAppendingPathComponent:@"etude.rtf" isDirectory:NO];

    error = nil;
    NSAttributedString *comparison = [[NSAttributedString alloc] initWithURL:transcriptURL options:@{NSDocumentTypeDocumentAttribute:NSRTFTextDocumentType} documentAttributes:nil error:&error];
    if (error)
        NSLog(@"Error: %@", error);

    XCTAssert([comparison isEqual:facit]);
}

- (void)testJacl {
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElement *textField = [self addAndSelectGame:@"grail.j2"];

    [textField doubleClick];

    XCUIElement *gameWindow = app.windows[@"grail.j2"];
    XCUIElement *scrollView = [gameWindow.scrollViews elementBoundByIndex:0];
    XCUIElement *textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;

    [UITests turnOnDeterminism:@"Default"];

    [textView click];

    [textView typeText:@"transcript\r"];

    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"Unholy Grail"];

    NSString *facit = [self comparisonTranscriptFor:@"Unholy Grail"];

    gameWindow = app.windows[@"grail.j2 (finished)"];
    XCUIElement *textView2 = [gameWindow.staticTexts elementBoundByIndex:0];

    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"value BEGINSWITH ' Game Over '"];
    XCTNSPredicateExpectation *expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView2];
    [self waitForExpectations:@[expectation] timeout:80];

    transcriptURL = [transcriptURL URLByAppendingPathComponent:@"Transcript of grail.j2.txt" isDirectory:NO];

    NSError *error = nil;
    NSString *transcript = [NSString stringWithContentsOfURL:transcriptURL encoding:NSUTF8StringEncoding error:&error];

    if (error)
        NSLog(@"Error: %@", error);

    XCTAssert([transcript isEqualToString:facit]);
}

- (void)testScott {
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElement *textField = [self addAndSelectGame:@"adv01.dat"];

    [textField doubleClick];

    XCUIElement *gameWindow = app.windows[@"adv01.dat"];
    XCUIElement *scrollView = [gameWindow.scrollViews elementBoundByIndex:0];
    XCUIElement *textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;

    [UITests turnOnDeterminism:@"Default"];

    [textView click];
    [textView typeText:@"transcript\r"];
    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [self openCommandScript:@"ScottFree"];

    NSString *facit = [self comparisonTranscriptFor:@"adv01"];

    gameWindow = app.windows[@"adv01.dat (finished)"];
    textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"value CONTAINS 'transcript off'"];
    XCTNSPredicateExpectation *expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView];
    [self waitForExpectations:@[expectation] timeout:25];

    transcriptURL = [transcriptURL URLByAppendingPathComponent:@"Transcript of adv01.dat.txt" isDirectory:NO];

    NSError *error = nil;
    NSString *transcript = [NSString stringWithContentsOfURL:transcriptURL encoding:NSUTF8StringEncoding error:&error];

    if (error)
        NSLog(@"Error: %@", error);

    XCTAssert([transcript isEqualToString:facit]);
}

- (void)testTaylor {
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElement *textField = [self addAndSelectGame:@"tot.tay"];

    [textField doubleClick];

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

    NSString *facit = [self comparisonTranscriptFor:@"tot"];

    gameWindow = app.windows[@"tot.tay"];
    textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"value CONTAINS 'TRANSCRIPT OFF'"];
    XCTNSPredicateExpectation *expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView];
    [self waitForExpectations:@[expectation] timeout:25];

    transcriptURL = [transcriptURL URLByAppendingPathComponent:@"Transcript of tot.tay.txt" isDirectory:NO];

    NSError *error = nil;
    NSString *transcript = [NSString stringWithContentsOfURL:transcriptURL encoding:NSUTF8StringEncoding error:&error];

    if (error)
        NSLog(@"Error: %@", error);

    XCTAssert([transcript isEqualToString:facit]);
}

- (void)testPlus {
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElement *textField = [self addAndSelectGame:@"SPL13P.plus"];

    [textField doubleClick];

    XCUIElement *gameWindow = app.windows[@"SPL13P.plus"];
    XCUIElement *scrollView = [gameWindow.scrollViews elementBoundByIndex:0];
    XCUIElement *textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;

    [UITests turnOnDeterminism:@"Default"];

    [textView click];
    [textView typeText:@"transcript\r"];
    NSURL *transcriptURL = [UITests saveTranscriptInWindow:gameWindow];

    [textView click];
    [self openCommandScript:@"Plus"];

    NSString *facit = [self comparisonTranscriptFor:@"SPL13P"];

    gameWindow = app.windows[@"SPL13P.plus"];
    textView = [scrollView childrenMatchingType:XCUIElementTypeTextView].element;
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"value CONTAINS 'TRANSCRIPT OFF'"];
    XCTNSPredicateExpectation *expectation = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate object:textView];
    [self waitForExpectations:@[expectation] timeout:25];

    transcriptURL = [transcriptURL URLByAppendingPathComponent:@"Transcript of SPL13P.plus.txt" isDirectory:NO];

    NSError *error = nil;
    NSString *transcript = [NSString stringWithContentsOfURL:transcriptURL encoding:NSUTF8StringEncoding error:&error];

    if (error)
        NSLog(@"Error: %@", error);

    XCTAssert([transcript isEqualToString:facit]);
}


- (void)testEditMenu {
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElementQuery *menuBarsQuery = app.menuBars;

    XCUIElement *textField = [self addAndSelectGame:@"curses.z5"];
    [textField doubleClick];

    XCUIElement *gameWindow = app/*@START_MENU_TOKEN@*/.windows[@"gameWinZCODE-16-951024-4DE6"]/*[[".windows[@\"curses.z5\"]",".windows[@\"gameWinZCODE-16-951024-4DE6\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;

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
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElementQuery *menuBarsQuery = app.menuBars;

    XCUIElement *libraryWindow = app/*@START_MENU_TOKEN@*/.windows[@"library"]/*[[".windows[@\"Interactive Fiction\"]",".windows[@\"library\"]"],[[[-1,1],[-1,0]]],[1]]@END_MENU_TOKEN@*/;

    XCUIElement *textField = [self addAndSelectGame:@"curses.z5"];
    [textField rightClick];
    [libraryWindow/*@START_MENU_TOKEN@*/.tables[@"Games"].menuItems[@"Play"]/*[[".splitGroups[@\"SplitViewTotal\"]",".scrollViews.tables[@\"Games\"]",".menus.menuItems[@\"Play\"]",".menuItems[@\"Play\"]",".tables[@\"Games\"]"],[[[-1,4,2],[-1,1,2],[-1,0,1]],[[-1,4,2],[-1,1,2]],[[-1,3],[-1,2]]],[0,0]]@END_MENU_TOKEN@*/ click];
    [app/*@START_MENU_TOKEN@*/.windows[@"gameWinZCODE-16-951024-4DE6"]/*[[".windows[@\"Curses\"]",".windows[@\"gameWinZCODE-16-951024-4DE6\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/.buttons[XCUIIdentifierCloseWindow] click];

    [textField click];
    [textField rightClick];
    [libraryWindow/*@START_MENU_TOKEN@*/.tables[@"Games"].menuItems[@"Download Info"]/*[[".splitGroups[@\"SplitViewTotal\"]",".scrollViews.tables[@\"Games\"]",".menus.menuItems[@\"Download Info\"]",".menuItems[@\"Download Info\"]",".tables[@\"Games\"]"],[[[-1,4,2],[-1,1,2],[-1,0,1]],[[-1,4,2],[-1,1,2]],[[-1,3],[-1,2]]],[0,0]]@END_MENU_TOKEN@*/ click];

    XCUIElement *imageDialog = app.dialogs[@"alert"];

    BOOL result = [imageDialog waitForExistenceWithTimeout:5];

    if (result) {
        [imageDialog.buttons[@"Yes"] click];
    }

    [textField rightClick];
    [libraryWindow.tables[@"Games"].menuItems[@"Show Info"] click];

    XCUIElement *infoWin = app.windows[@"infoWinZCODE-16-951024-4DE6"];
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
    XCTAssert([alert waitForExistenceWithTimeout:5]);
    [alert.buttons[@"Delete"] click];

    image = [infoWin childrenMatchingType:XCUIElementTypeImage].firstMatch;

    [self forceClickElement:image];
    [infoWin.menuItems[@"Paste"] click];
    [self forceClickElement:image];
    [infoWin.menuItems[@"Filter"] click];
    [self forceClickElement:image];
    [infoWin.menuItems[@"Add description"] click];

    alert = app.dialogs.firstMatch;
    XCTAssert([alert waitForExistenceWithTimeout:5]);
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
    XCTAssert([app waitForExistenceWithTimeout:5]);
    [menuBarsQuery.menuBarItems[@"Window"] click];
    [menuBarsQuery.menuItems[@"Interactive Fiction"] click];
    [textField rightClick];
    [libraryWindow.tables[@"Games"].menuItems[@"Open Ifdb page"] click];
    [app terminate];
    // Quit and restart the app to get back from Finder
    [app launch];
    XCTAssert([app waitForExistenceWithTimeout:5]);
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


    infoWin = app.windows[@"infoWinGLULX-3-151126-1731D3B6"];
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
    XCTAssert([openDialog waitForExistenceWithTimeout:5]);
    [UITests typeURL:path intoFileDialog:openDialog andPressButtonWithText:@"Open"];

    [self forceClickElement:image];
    [infoWin.menuItems[@"Reload from Blorb"] click];
}

- (void)forceClickElement:(XCUIElement *)element {
    XCUICoordinate *coordinate = [element coordinateWithNormalizedOffset:CGVectorMake(0.1, 0.1)];
    [coordinate rightClick];
}

- (void)testPreferences {

    XCUIApplication *app = [[XCUIApplication alloc] init];

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

    [app/*@START_MENU_TOKEN@*/.checkBoxes[@"Game specific hacks"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.checkBoxes[@\"Game specific hacks\"]",".checkBoxes[@\"Game specific hacks\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [app/*@START_MENU_TOKEN@*/.checkBoxes[@"Determinism"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.checkBoxes[@\"Determinism\"]",".checkBoxes[@\"Determinism\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];

    [app/*@START_MENU_TOKEN@*/.toolbars.buttons[@"Themes"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.toolbars.buttons[@\"Themes\"]",".toolbars.buttons[@\"Themes\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    [app/*@START_MENU_TOKEN@*/.checkBoxes[@"Changes apply to all games in library"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.checkBoxes[@\"Changes apply to all games in library\"]",".checkBoxes[@\"Changes apply to all games in library\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];
    alert = app.sheets[@"alert"];
    if (alert.exists)
        [alert.buttons[@"Okay"] click];

    XCUIElement *actionMenuButton = app/*@START_MENU_TOKEN@*/.menuButtons[@"action"]/*[[".dialogs[@\"Preferences\"]",".tabGroups.menuButtons[@\"action\"]",".menuButtons[@\"action\"]",".dialogs[@\"preferences\"]"],[[[-1,2],[-1,1],[-1,3,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/;
    XCTAssert([actionMenuButton waitForExistenceWithTimeout:5]);
    [actionMenuButton click];
    [app/*@START_MENU_TOKEN@*/.menuItems[@"deleteUserThemes:"]/*[[".dialogs[@\"Preferences\"]",".tabGroups",".menuButtons[@\"action\"]",".menus",".menuItems[@\"Delete All User Themes\"]",".menuItems[@\"deleteUserThemes:\"]",".dialogs[@\"preferences\"]"],[[[-1,5],[-1,4],[-1,3,4],[-1,2,3],[-1,1,2],[-1,6,1],[-1,0,1]],[[-1,5],[-1,4],[-1,3,4],[-1,2,3],[-1,1,2]],[[-1,5],[-1,4],[-1,3,4],[-1,2,3]],[[-1,5],[-1,4],[-1,3,4]],[[-1,5],[-1,4]]],[0]]@END_MENU_TOKEN@*/ click];
    [actionMenuButton click];
    [app/*@START_MENU_TOKEN@*/.menuItems[@"rebuildDefaultThemes:"]/*[[".dialogs[@\"Preferences\"]",".tabGroups",".menuButtons[@\"action\"]",".menus",".menuItems[@\"Rebuild Default Themes\"]",".menuItems[@\"rebuildDefaultThemes:\"]",".dialogs[@\"preferences\"]"],[[[-1,5],[-1,4],[-1,3,4],[-1,2,3],[-1,1,2],[-1,6,1],[-1,0,1]],[[-1,5],[-1,4],[-1,3,4],[-1,2,3],[-1,1,2]],[[-1,5],[-1,4],[-1,3,4],[-1,2,3]],[[-1,5],[-1,4],[-1,3,4]],[[-1,5],[-1,4]]],[0]]@END_MENU_TOKEN@*/ click];
    [actionMenuButton click];
    [app/*@START_MENU_TOKEN@*/.menuItems[@"togglePreview:"]/*[[".dialogs[@\"Preferences\"]",".tabGroups",".menuButtons[@\"action\"]",".menus",".menuItems[@\"Show Preview\"]",".menuItems[@\"togglePreview:\"]",".dialogs[@\"preferences\"]"],[[[-1,5],[-1,4],[-1,3,4],[-1,2,3],[-1,1,2],[-1,6,1],[-1,0,1]],[[-1,5],[-1,4],[-1,3,4],[-1,2,3],[-1,1,2]],[[-1,5],[-1,4],[-1,3,4],[-1,2,3]],[[-1,5],[-1,4],[-1,3,4]],[[-1,5],[-1,4]]],[0]]@END_MENU_TOKEN@*/ click];
}

- (XCUIElement *)addAndSelectGame:(NSString *)game {

    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElement *libraryWindow = app/*@START_MENU_TOKEN@*/.windows[@"library"]/*[[".windows[@\"Interactive Fiction\"]",".windows[@\"library\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;

    NSString *gameName = game.stringByDeletingPathExtension;
    if (![self doesGameExist:gameName]) {

        XCUIElement *addButton = libraryWindow.toolbars.buttons[@"Add"];

        XCTAssert(addButton.exists);
        [addButton click];

        XCUIElement *openDialog = app.sheets.firstMatch;
        // Grab a reference to the Open button so we can click it later

        NSURL *url = [testBundle URLForResource:gameName
                                  withExtension:game.pathExtension
                                   subdirectory:nil];

        [UITests typeURL:url intoFileDialog:openDialog andPressButtonWithText:@"Add"];
    }

    XCUIElement *searchField = libraryWindow/*@START_MENU_TOKEN@*/.searchFields[@"Search"]/*[[".splitGroups[@\"SplitViewTotal\"].searchFields[@\"Search\"]",".searchFields[@\"Search\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;
    // Select search field
    [searchField click];

    XCUIElement *cancelButton = searchField.buttons[@"cancel"];
    if ([cancelButton waitForExistenceWithTimeout:10])
        [cancelButton click];
    else {
        [searchField doubleClick];
        [searchField typeText:@" "];
    }

    [searchField click];
    [searchField typeText:gameName];

    XCUIElement *gamesTableRow =
    [[libraryWindow/*@START_MENU_TOKEN@*/.tables[@"Games"]/*[[".splitGroups[@\"SplitViewTotal\"]",".scrollViews.tables[@\"Games\"]",".tables[@\"Games\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/.tableRows childrenMatchingType:XCUIElementTypeCell] elementBoundByIndex:0];
    [gamesTableRow click];
    return gamesTableRow;
}

- (BOOL)doesGameExist:(NSString *)game {
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElementQuery *menuBarsQuery = app.menuBars;

    [menuBarsQuery.menuBarItems[@"Window"] click];
    [menuBarsQuery.menuItems[@"Interactive Fiction"] click];

    XCUIElement *libraryWindow = app.windows[@"library"];

    XCUIElement *searchField = libraryWindow.searchFields.firstMatch;
    XCTAssert([searchField waitForExistenceWithTimeout:5]);
    XCUIElement *cancelButton = searchField.buttons[@"cancel"];
    if (cancelButton.exists)
        [cancelButton click];
    [searchField click];
    [searchField typeText:game];

    XCUIElement *gamesTable = libraryWindow/*@START_MENU_TOKEN@*/.tables[@"Games"]/*[[".splitGroups[@\"SplitViewTotal\"]",".scrollViews.tables[@\"Games\"]",".tables[@\"Games\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/;
    XCUIElement *textField = [[gamesTable.tableRows childrenMatchingType:XCUIElementTypeTextField] elementBoundByIndex:0];
    return (textField.exists && textField.hittable);
}


- (void)testLaunchPerformance {
    if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, watchOS 7.0, *)) {
        // This measures how long it takes to launch your application.
        [self measureWithMetrics:@[[[XCTApplicationLaunchMetric alloc] init]] block:^{
            [[[XCUIApplication alloc] init] launch];
        }];
    }
}

@end
