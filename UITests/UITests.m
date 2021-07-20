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

    // In UI tests it’s important to set the initial state - such as interface orientation - required for your tests before they run. The setUp method is a good place to do this.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
}

- (void)testExample {
    // UI tests must launch the application that they test.
    XCUIApplication *app = [[XCUIApplication alloc] init];
    [app launch];

    // Use recording to get started writing UI tests.
    // Use XCTAssert and related functions to verify your tests produce the correct results.
}

- (void)testTetrisLowerWindow {


    XCUIApplication *app = [[XCUIApplication alloc] init];



    XCUIElementQuery *menuBarsQuery = app.menuBars;
    [menuBarsQuery.menuBarItems[@"Window"] click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Interactive Fiction"]/*[[".menuBarItems[@\"Window\"]",".menus.menuItems[@\"Interactive Fiction\"]",".menuItems[@\"Interactive Fiction\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];

    XCUIElement *libraryWindow = app/*@START_MENU_TOKEN@*/.windows[@"library"]/*[[".windows[@\"Interactive Fiction\"]",".windows[@\"library\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;

//        NSURL *url = [testBundle URLForResource:@"freefall"
//                                  withExtension:@"z5"
//                                   subdirectory:nil];


    XCUIElement *searchSearchField = libraryWindow/*@START_MENU_TOKEN@*/.searchFields[@"Search"]/*[[".splitGroups[@\"SplitViewTotal\"].searchFields[@\"Search\"]",".searchFields[@\"Search\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;
    // Select search field
    [searchSearchField click];

    // Select all previous text
    [searchSearchField typeKey:@"a" modifierFlags:XCUIKeyModifierCommand];
    // Delete it
    [searchSearchField typeKey:XCUIKeyboardKeyDelete modifierFlags:XCUIKeyModifierNone];

    [searchSearchField typeText:@"tetris zcode"];
    [[[libraryWindow/*@START_MENU_TOKEN@*/.tables[@"Games"]/*[[".splitGroups[@\"SplitViewTotal\"]",".scrollViews.tables[@\"Games\"]",".tables[@\"Games\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/.tableRows childrenMatchingType:XCUIElementTypeTextField] elementBoundByIndex:0] doubleClick];

    XCUIElement *gamewin = app/*@START_MENU_TOKEN@*/.windows[@"gameWinZCODE-2-951111-2084"]/*[[".windows[@\"Tetris\"]",".windows[@\"gameWinZCODE-2-951111-2084\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;
    XCUIElement *lowerScrollView = gamewin.scrollViews[@"buffer scroll view"];


    XCUIElement *lowerTextView = [lowerScrollView childrenMatchingType:XCUIElementTypeTextView].element;
    // Reset any autorestored game running
    [lowerTextView typeKey:@"r" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierOption];

    // Start the tiles falling
    [lowerTextView typeText:@"\r"];

    CGRect textRect = lowerTextView.frame;
    CGRect scrollRect = lowerScrollView.frame;
    XCTAssertEqual(textRect.origin.y, scrollRect.origin.y,
                   @"textView origin doesn't match scrollView origin");

    [gamewin.buttons[XCUIIdentifierCloseWindow] click];
}

- (void)testImportGame {
    XCUIApplication *app = [[XCUIApplication alloc] init];

    XCUIElementQuery *windowsQuery = app.windows;

    XCUIElement *addButton = windowsQuery.buttons[@"Add"];
    XCTAssert(addButton.exists);
    [addButton click];

    XCUIElement *openDialog = app.sheets.firstMatch;
    // Grab a reference to the Open button so we can click it later
    XCUIElement *openButton = openDialog.buttons[@"Add"];
    XCTAssert(openButton.exists);
    [app typeKey:@"g" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierShift];

    XCUIElement *sheet = openDialog.sheets.firstMatch;

    XCUIElement *goButton = openDialog.buttons[@"Go"];
    XCUIElement *input = sheet.comboBoxes.firstMatch;
    XCTAssert(goButton.exists);
    XCTAssert(input.exists);

    NSURL *url = [testBundle URLForResource:@"freefall"
                              withExtension:@"z5"
                               subdirectory:nil];

    [input typeText:url.path];
    [goButton click];
    [openButton click];

    XCUIElementQuery *menuBarsQuery = app.menuBars;

    [menuBarsQuery.menuBarItems[@"Window"] click];
    [menuBarsQuery/*@START_MENU_TOKEN@*/.menuItems[@"Interactive Fiction"]/*[[".menuBarItems[@\"Window\"]",".menus.menuItems[@\"Interactive Fiction\"]",".menuItems[@\"Interactive Fiction\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/ click];

    XCUIElement *libraryWindow = app/*@START_MENU_TOKEN@*/.windows[@"library"]/*[[".windows[@\"Interactive Fiction\"]",".windows[@\"library\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;


    XCUIElement *searchSearchField = libraryWindow/*@START_MENU_TOKEN@*/.searchFields[@"Search"]/*[[".splitGroups[@\"SplitViewTotal\"].searchFields[@\"Search\"]",".searchFields[@\"Search\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;
    // Select search field
    [searchSearchField click];

    // Select all previous text
    [searchSearchField typeKey:@"a" modifierFlags:XCUIKeyModifierCommand];
    // Delete it
    [searchSearchField typeKey:XCUIKeyboardKeyDelete modifierFlags:XCUIKeyModifierNone];

    [searchSearchField typeText:@"tetris zcode"];

    XCUIElement *foundElement = [[libraryWindow/*@START_MENU_TOKEN@*/.tables[@"Games"]/*[[".splitGroups[@\"SplitViewTotal\"]",".scrollViews.tables[@\"Games\"]",".tables[@\"Games\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/.tableRows childrenMatchingType:XCUIElementTypeTextField] elementBoundByIndex:0] ;
    [foundElement click];
    [foundElement typeKey:XCUIKeyboardKeyDelete modifierFlags:XCUIKeyModifierCommand];

    XCTAssert(!foundElement.exists);

    [addButton click];

    openDialog = app.sheets.firstMatch;

    openButton = openDialog.buttons[@"Add"];
    XCTAssert(openButton.exists);
    [app typeKey:@"g" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierShift];

    sheet = openDialog.sheets.firstMatch;

    goButton = openDialog.buttons[@"Go"];
    input = sheet.comboBoxes.firstMatch;
    XCTAssert(goButton.exists);
    XCTAssert(input.exists);

    [input typeText:url.path];
    [goButton click];

    [openButton click];

    // Select search field
    [searchSearchField click];

    // Select all previous text
    [searchSearchField typeKey:@"a" modifierFlags:XCUIKeyModifierCommand];
    // Delete it
    [searchSearchField typeKey:XCUIKeyboardKeyDelete modifierFlags:XCUIKeyModifierNone];

    [searchSearchField typeText:@"tetris zcode"];
    [[[libraryWindow/*@START_MENU_TOKEN@*/.tables[@"Games"]/*[[".splitGroups[@\"SplitViewTotal\"]",".scrollViews.tables[@\"Games\"]",".tables[@\"Games\"]"],[[[-1,2],[-1,1],[-1,0,1]],[[-1,2],[-1,1]]],[0]]@END_MENU_TOKEN@*/.tableRows childrenMatchingType:XCUIElementTypeTextField] elementBoundByIndex:0] doubleClick];
    XCUIElement *gamewin = app/*@START_MENU_TOKEN@*/.windows[@"gameWinZCODE-2-951111-2084"]/*[[".windows[@\"Tetris\"]",".windows[@\"gameWinZCODE-2-951111-2084\"]"],[[[-1,1],[-1,0]]],[0]]@END_MENU_TOKEN@*/;
    XCTAssert(gamewin.exists);
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
