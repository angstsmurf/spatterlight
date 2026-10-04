//
//  GridRestyleTests.m
//  SpatterlightTests
//
//  Pins what happens to a text grid when a game is autorestored with another
//  theme than the one it was autosaved with.
//
//  The case that broke was Beyond Zork: start it with the DOSBox theme, open
//  the DEFINE menu, close the window, give the game the Default theme and open
//  it again.  The menu came back with every row cut in the wrong place.
//
//  A grid keeps two copies of its text: the text storage on screen and a
//  buffer that printing and resizing work on, which a flush copies to the
//  screen.  setFrame: lays the buffer out for the new number of columns and
//  leaves the text storage alone.  During a restore the grid is resized without
//  a flush, and then the theme is swapped in.  -[GlkTextGridWindow
//  prefsDidChange] used to restyle a copy of the text storage and make that the
//  new buffer: text laid out for the old number of columns, in a grid that
//  already counted the new number.  The next setFrame: -- the interpreter
//  answering the arrange event -- cut the rows at the wrong column.
//
//  The test feeds a GlkController the requests bocfel sends, as QuoteBoxTests
//  does, and takes it through the real archive, restoreUI and
//  notePreferencesChanged code.  Only the interpreter process and the window
//  sizes it would ask for (SIZWIN) are stood in for.
//

#import <XCTest/XCTest.h>
#import <CoreData/CoreData.h>

#import "GlkController.h"
#import "GlkController_Private.h"
#import "GlkController+Autorestore.h"
#import "GlkController+GlkRequests.h"
#import "GlkWindow.h"
#import "GlkTextBufferWindow.h"
#import "GlkTextGridWindow.h"
#import "GridTextView.h"
#import "Theme.h"
#import "Game.h"
#import "GlkStyle.h"
#import "BuiltInThemes.h"
#import "CoreDataManager.h"

#include "glk.h"
#include "protocol.h"

// Not in BuiltInThemes.h.
@interface BuiltInThemes (GridRestyleTests)
+ (Theme *)createClassicSpatterlightThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force;
@end

// Window names, as bocfel numbers them.
static const int kStory = 0;
static const int kMenu = 1;

static const unichar kNBSP = 0xa0;

// The widths the game window has when it is autosaved and when it comes back.
static const CGFloat kSavedWidth = 700;
static const CGFloat kRestoredWidth = 730;

@interface GridRestyleTests : XCTestCase

@property (nonatomic, strong) NSManagedObjectContext *context;
// The theme the game is autosaved with, and the one it is given while closed.
@property (nonatomic, strong) Theme *savedTheme;
@property (nonatomic, strong) Theme *laterTheme;
@property (nonatomic, strong) Game *game;
// GlkWindow.glkctl and GlkController.game are weak, so the test owns these.
@property (nonatomic, strong) NSMutableArray<GlkController *> *controllers;

@end

@implementation GridRestyleTests

- (void)setUp {
    [super setUp];

    // Always an in-memory store: creating themes with forceRebuild would
    // rewrite the real library's.
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

    self.laterTheme = [BuiltInThemes createDefaultThemeInContext:self.context forceRebuild:YES];
    self.savedTheme = [BuiltInThemes createClassicSpatterlightThemeInContext:self.context forceRebuild:YES];
    XCTAssertNotNil(self.laterTheme);
    XCTAssertNotNil(self.savedTheme);

    // The two themes have to disagree about how many columns fit in a window,
    // or swapping them does not lay the grid out again.
    XCTAssertNotEqual([self columnsIn:kRestoredWidth theme:self.savedTheme],
                      [self columnsIn:kRestoredWidth theme:self.laterTheme]);
    // And the window has to come back with another number of columns than it
    // was saved with, which is what leaves the grid waiting for a flush.
    XCTAssertNotEqual([self columnsIn:kSavedWidth theme:self.savedTheme],
                      [self columnsIn:kRestoredWidth theme:self.savedTheme]);

    self.game = [NSEntityDescription insertNewObjectForEntityForName:@"Game"
                                              inManagedObjectContext:self.context];
    self.game.ifid = @"ZCODE-1-261004-0001";
    self.game.theme = self.savedTheme;

    self.controllers = [NSMutableArray array];
}

- (void)tearDown {
    for (GlkController *ctl in self.controllers)
        [NSObject cancelPreviousPerformRequestsWithTarget:ctl];
    self.controllers = nil;
    self.game = nil;
    self.savedTheme = nil;
    self.laterTheme = nil;
    self.context = nil;
    [super tearDown];
}

#pragma mark - A controller without an interpreter

// A GlkController wired the way runTerp wires one, minus the interpreter
// process. It gets a window (never shown) because GlkTextGridWindow's
// setFrame: will not lay out a grid without a screen to measure it against,
// and events meant for the interpreter go to /dev/null.
- (GlkController *)makeControllerWithTheme:(Theme *)theme {
    GlkController *ctl = [[GlkController alloc] init];
    ctl.theme = theme;
    [ctl setValue:self.game forKey:@"game"];

    NSMutableArray *nullarray = [NSMutableArray arrayWithCapacity:stylehint_NUMHINTS];
    for (NSInteger i = 0; i < stylehint_NUMHINTS; i++)
        [nullarray addObject:[NSNull null]];

    ctl.gridStyleHints = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
    ctl.bufferStyleHints = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
    for (NSInteger i = 0; i < style_NUMSTYLES; i++) {
        [ctl.gridStyleHints addObject:[nullarray mutableCopy]];
        [ctl.bufferStyleHints addObject:[nullarray mutableCopy]];
    }

    ctl.gwindows = [NSMutableDictionary dictionary];
    ctl.windowsToBeAdded = [NSMutableArray array];
    ctl.windowsToBeRemoved = [NSMutableArray array];
    ctl.queue = [NSMutableArray array];
    ctl->sendfh = [NSFileHandle fileHandleWithNullDevice];

    ctl.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 800, 600)
                                             styleMask:NSWindowStyleMaskTitled
                                               backing:NSBackingStoreBuffered
                                                 defer:YES];
    XCTAssertNotNil(ctl.window.screen, @"the test needs a screen to lay out text grids");
    ctl.gameView = [[GlkHelperView alloc] initWithFrame:ctl.window.contentView.bounds];
    [ctl.window.contentView addSubview:ctl.gameView];

    [self.controllers addObject:ctl];
    return ctl;
}

#pragma mark - Requests, as the interpreter sends them

- (int)send:(int)cmd to:(GlkController *)ctl a1:(int)a1 a2:(int)a2 a3:(int)a3 text:(NSString *)text {
    struct message request = { .cmd = cmd, .a1 = a1, .a2 = a2, .a3 = a3 };
    struct message reply = { 0 };
    NSMutableData *buffer = [NSMutableData dataWithLength:(text.length + 1) * sizeof(unichar)];
    if (text.length) {
        [text getCharacters:buffer.mutableBytes range:NSMakeRange(0, text.length)];
        request.len = text.length * sizeof(unichar);
    }
    [ctl handleRequest:&request reply:&reply buffer:buffer.mutableBytes];
    return reply.a1;
}

// The interpreter asks for the next event: the end of a turn.
- (void)nextEventIn:(GlkController *)ctl {
    [self send:NEXTEVENT to:ctl a1:1 a2:0 a3:0 text:nil];
}

// The number of columns setFrame: gives a grid this wide.
- (NSUInteger)columnsIn:(CGFloat)width theme:(Theme *)theme {
    return (NSUInteger)round((width - 2 * theme.gridMarginX - 10) / theme.cellWidth);
}

// Stands in for SIZWIN, which only works out a frame and sets it. The menu
// fills the game window, as Beyond Zork's does, with the story window left a
// sliver underneath.
- (void)sizeWindowsTo:(CGFloat)width theme:(Theme *)theme in:(GlkController *)ctl {
    CGFloat menuHeight = ceil([self menu].count * theme.cellHeight + 2 * theme.gridMarginY);
    ctl.gwindows[@(kMenu)].frame = NSMakeRect(0, 0, width, menuHeight);
    ctl.gwindows[@(kStory)].frame = NSMakeRect(0, menuHeight, width, 40);
}

#pragma mark - The menu

// Rows like those of Beyond Zork's DEFINE menu. None is blank, and none is
// longer than the narrowest grid in the test.
- (NSArray<NSString *> *)menu {
    return @[@"  Function Key Definitions",
             @"  F1  look around",
             @"  F2  inventory",
             @"  F3  status",
             @"  F4  examine",
             @"  F5  take",
             @"  F6  drop",
             @"  Restore Defaults",
             @"  Exit"];
}

- (void)drawMenuIn:(GlkController *)ctl {
    NSArray<NSString *> *menu = [self menu];
    for (NSUInteger i = 0; i < menu.count; i++) {
        [self send:MOVETO to:ctl a1:kMenu a2:0 a3:(int)i text:nil];
        [self send:PRINT to:ctl a1:kMenu a2:style_Normal a3:0 text:menu[i]];
    }
}

// The grid holds the menu, one item to a row, and every row is as long as the
// grid is wide.
- (void)assertMenuIsIntactIn:(GlkController *)ctl columns:(NSUInteger)columns {
    GlkTextGridWindow *grid = (GlkTextGridWindow *)ctl.gwindows[@(kMenu)];
    XCTAssertTrue([grid isKindOfClass:[GlkTextGridWindow class]]);

    NSString *nbsp = [NSString stringWithCharacters:&kNBSP length:1];
    NSString *text = [grid.textview.string stringByReplacingOccurrencesOfString:nbsp withString:@" "];
    if ([text hasSuffix:@"\n"])
        text = [text substringToIndex:text.length - 1];
    NSArray<NSString *> *rows = [text componentsSeparatedByString:@"\n"];

    NSArray<NSString *> *menu = [self menu];
    XCTAssertEqual(rows.count, menu.count);
    for (NSUInteger i = 0; i < MIN(rows.count, menu.count); i++) {
        XCTAssertEqual(rows[i].length, columns, @"row %lu", i);
        NSString *expected = [menu[i] stringByPaddingToLength:columns withString:@" " startingAtIndex:0];
        XCTAssertEqualObjects(rows[i], expected, @"row %lu", i);
    }
}

#pragma mark - Quitting and relaunching

// Archives a controller the way handleAutosave: and autoSaveOnExit do, and
// reads it back the way a relaunch does.
- (GlkController *)archivedCopyOf:(GlkController *)ctl {
    NSError *error = nil;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:ctl requiringSecureCoding:NO error:&error];
    XCTAssertNil(error);
    XCTAssertNotNil(data);

    NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
    XCTAssertNil(error);
    unarchiver.requiresSecureCoding = NO;
    GlkController *copy = [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
    [unarchiver finishDecoding];
    XCTAssertNotNil(copy);
    [self.controllers addObject:copy];
    return copy;
}

#pragma mark - The test

- (void)testMenuSurvivesAutorestoreWithAnotherTheme {
    Theme *savedTheme = self.savedTheme;
    Theme *laterTheme = self.laterTheme;

    // The game is played with the first theme, and autosaved with its menu up.
    GlkController *ctl = [self makeControllerWithTheme:savedTheme];
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextBuffer a2:kStory a3:0 text:nil], kStory);
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextGrid a2:kMenu a3:0 text:nil], kMenu);
    [self sizeWindowsTo:kSavedWidth theme:savedTheme in:ctl];
    [self drawMenuIn:ctl];
    [self nextEventIn:ctl];
    [self assertMenuIsIntactIn:ctl columns:[self columnsIn:kSavedWidth theme:savedTheme]];

    GlkController *restored = [self archivedCopyOf:ctl];
    GlkController *restoredLate = [self archivedCopyOf:ctl];
    XCTAssertEqualObjects(restoredLate.oldThemeName, savedTheme.name);

    // While it is closed, the game is given the other theme.
    self.game.theme = laterTheme;

    // The relaunch. runTerpWithAutorestore puts the autosaved theme back for
    // the time being, and stashes the one the game has now.
    ctl = [self makeControllerWithTheme:savedTheme];
    ctl.stashedTheme = laterTheme;
    ctl->restoredController = restored;
    ctl->restoredControllerLate = restoredLate;
    ctl->shouldRestoreUI = YES;
    ctl->autorestoring = YES;
    // restoreUI runs on the NEXTEVENT that finds eventcount at 2.
    ctl.eventcount = 2;

    // Bocfel restores its own state from the interpreter autosave. Then it
    // opens two windows for its scrollback history and closes both again,
    // before asking for input in the windows it had.
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextBuffer a2:kStory a3:0 text:nil], kStory);
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextGrid a2:kMenu a3:0 text:nil], kMenu);
    [self send:DELWIN to:ctl a1:kStory a2:0 a3:0 text:nil];
    [self send:DELWIN to:ctl a1:kMenu a2:0 a3:0 text:nil];
    [self nextEventIn:ctl];
    XCTAssertFalse(ctl->shouldRestoreUI, @"the first NEXTEVENT should have run restoreUI");

    // The game window does not come back quite as wide as it was, and the
    // grid is resized to fit before anything flushes it.
    [self sizeWindowsTo:kRestoredWidth theme:savedTheme in:ctl];

    // restoreUI defers postRestoreArrange:, which shows the game window and
    // the autorestore alert. Do the rest of what it does: swap in the stashed
    // theme, and run the preferences pass, which sends the interpreter an
    // arrange event to answer.
    [NSObject cancelPreviousPerformRequestsWithTarget:ctl
                                             selector:@selector(postRestoreArrange:)
                                               object:nil];
    ctl.theme = laterTheme;
    ctl.stashedTheme = nil;
    [ctl notePreferencesChanged:[NSNotification notificationWithName:@"PreferencesChanged"
                                                              object:ctl.theme]];
    ctl->autorestoring = NO;
    XCTAssertEqual(ctl.gwindows[@(kMenu)].theme, laterTheme);

    // The interpreter sizes its windows for the new theme, prints nothing,
    // and goes back to waiting.
    [self sizeWindowsTo:kRestoredWidth theme:laterTheme in:ctl];
    [self nextEventIn:ctl];

    [self assertMenuIsIntactIn:ctl columns:[self columnsIn:kRestoredWidth theme:laterTheme]];
}

@end
