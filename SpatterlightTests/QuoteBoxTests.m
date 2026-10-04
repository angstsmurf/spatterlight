//
//  QuoteBoxTests.m
//  SpatterlightTests
//
//  Pins the life of a Z-machine quote box: how it is painted, that it comes
//  back after an autorestore, and that it fades out once the game moves on.
//
//  Bocfel draws a quotation as a block of reverse video in the (temporarily
//  enlarged) status window and then sends QUOTEBOX.  -[GlkTextGridWindow
//  quotebox:] cuts the block out of the grid into a floating window of its
//  own, which is archived with the buffer window it floats over and adopted
//  again by restoreUI.  At every NEXTEVENT the controller compares the number
//  of PRINT and CLRWIN requests seen so far with the number at the end of the
//  turn the box appeared on, and fades the box out when the game has printed
//  since.
//
//  Painting: the padding rows of a box are nothing but spaces in reverse
//  video.  With several grid fonts -- the default Source Code Pro among them
//  -- the text system does not paint the background of a line that holds only
//  spaces, so unless the first cell of the row is a non-breaking space the
//  box comes out striped: text rows on a solid background, the rows between
//  them see-through.  (The grid itself is protected by fixCollapsingSpaceRows;
//  a quote box row comes out of the *middle* of a grid row and needs its own
//  protection.)
//
//  The tests feed a GlkController the requests bocfel sends, through
//  handleRequest:reply:buffer:, and take it through the real archive,
//  restoreUI and notePreferencesChanged code.  Only the interpreter process
//  and the window sizes it would ask for (SIZWIN) are stood in for.
//

#import <XCTest/XCTest.h>
#import <CoreData/CoreData.h>

#import "GlkController.h"
#import "GlkController_Private.h"
#import "GlkController+Autorestore.h"
#import "GlkController+GlkRequests.h"
#import "GlkWindow.h"
#import "GlkTextBufferWindow.h"
#import "BufferTextView.h"
#import "GlkTextGridWindow.h"
#import "GridTextView.h"
#import "Theme.h"
#import "Game.h"
#import "GlkStyle.h"
#import "BuiltInThemes.h"
#import "CoreDataManager.h"

#include "glk.h"
#include "protocol.h"

// The box bocfel draws for
//     box "Beware the Jabberwock, my son!" "" "-- Lewis Carroll";
// on an 80 column screen: five rows of 34 cells at column 31, starting on row
// 3 of the status window, the text indented two cells.  Rows 0, 2 and 4 of the
// box are blank.
static const NSUInteger kScreenColumns = 80;
static const NSUInteger kBoxColumn = 31;
static const NSUInteger kBoxWidth = 34;
static const NSUInteger kBoxFirstRow = 3;
static const NSUInteger kBoxRows = 5;
static const NSUInteger kLinesToSkip = 1;

// Window names, as bocfel numbers them.
static const int kStory = 0;
static const int kStatus = 1;

static const unichar kNBSP = 0xa0;

@interface QuoteBoxTests : XCTestCase

@property (nonatomic, strong) NSManagedObjectContext *context;
@property (nonatomic, strong) Theme *theme;
@property (nonatomic, strong) Game *game;
// GlkWindow.glkctl and GlkController.game are weak, so the test owns these.
@property (nonatomic, strong) NSMutableArray<GlkController *> *controllers;

@end

@implementation QuoteBoxTests

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
    self.theme.quoteBox = YES;

    // The default grid font is one of those that collapse rows of spaces. If
    // it ever stops being one, the painting checks below still hold, they just
    // no longer prove anything about the workaround.
    XCTAssertEqualObjects(self.theme.gridNormal.font.familyName, @"Source Code Pro");

    self.game = [NSEntityDescription insertNewObjectForEntityForName:@"Game"
                                              inManagedObjectContext:self.context];
    self.game.ifid = @"ZCODE-1-261004-0000";
    self.game.theme = self.theme;

    self.controllers = [NSMutableArray array];
}

- (void)tearDown {
    for (GlkController *ctl in self.controllers) {
        [NSObject cancelPreviousPerformRequestsWithTarget:ctl];
        for (GlkTextGridWindow *box in ctl.quoteBoxes)
            [NSObject cancelPreviousPerformRequestsWithTarget:box];
    }
    self.controllers = nil;
    self.game = nil;
    self.theme = nil;
    self.context = nil;
    [super tearDown];
}

#pragma mark - A controller without an interpreter

// A GlkController wired the way runTerp wires one, minus the interpreter
// process. It gets a window (never shown) because GlkTextGridWindow's
// setFrame: will not lay out a grid without a screen to measure it against,
// and events meant for the interpreter go to /dev/null.
- (GlkController *)makeController {
    GlkController *ctl = [[GlkController alloc] init];
    ctl.theme = self.theme;
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

- (void)print:(NSString *)text style:(int)style window:(int)window in:(GlkController *)ctl {
    [self send:PRINT to:ctl a1:window a2:style a3:0 text:text];
}

- (void)moveToColumn:(int)column row:(int)row in:(GlkController *)ctl {
    [self send:MOVETO to:ctl a1:kStatus a2:column a3:row text:nil];
}

- (void)setReverse:(BOOL)reverse in:(GlkController *)ctl {
    [self send:SETREVERSE to:ctl a1:kStatus a2:reverse a3:0 text:nil];
}

// The interpreter asks for the next event: the end of a turn.
- (void)nextEventIn:(GlkController *)ctl {
    [self send:NEXTEVENT to:ctl a1:1 a2:0 a3:0 text:nil];
}

- (NSString *)spaces:(NSUInteger)count {
    return [@"" stringByPaddingToLength:count withString:@" " startingAtIndex:0];
}

// Stands in for SIZWIN, which only works out a frame and sets it.
- (void)sizeStatusTo:(NSUInteger)rows in:(GlkController *)ctl {
    Theme *theme = self.theme;
    CGFloat width = ceil(kScreenColumns * theme.cellWidth + 2 * theme.gridMarginX + 10);
    CGFloat statusHeight = ceil(rows * theme.cellHeight + 2 * theme.gridMarginY);
    ctl.gwindows[@(kStatus)].frame = NSMakeRect(0, 0, width, statusHeight);
    ctl.gwindows[@(kStory)].frame = NSMakeRect(0, statusHeight, width, 500 - statusHeight);
}

- (void)drawStatusLineIn:(GlkController *)ctl {
    [self moveToColumn:0 row:0 in:ctl];
    [self setReverse:YES in:ctl];
    [self print:[self spaces:kScreenColumns] style:style_Normal window:kStatus in:ctl];
    [self moveToColumn:1 row:0 in:ctl];
    [self print:@"Quote box test" style:style_Normal window:kStatus in:ctl];
    [self setReverse:NO in:ctl];
}

#pragma mark - A session of the game

- (void)openWindowsIn:(GlkController *)ctl {
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextBuffer a2:kStory a3:0 text:nil], kStory);
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextGrid a2:kStatus a3:0 text:nil], kStatus);
    [self sizeStatusTo:1 in:ctl];
}

// The game starts: a story window and a one row status window, the opening
// text, and the first prompt.
- (void)startGameIn:(GlkController *)ctl {
    [self openWindowsIn:ctl];
    [self drawStatusLineIn:ctl];
    [self print:@"Quote box test.\nType BOX for a quotation, QUIT to stop.\n\n>"
          style:style_Normal window:kStory in:ctl];
    [self nextEventIn:ctl];
}

// The player types BOX. The turn is left where bocfel sends AUTOSAVE: the
// box is up and the next prompt printed, but the interpreter has not yet
// asked for input. Returns the floating quote box.
- (GlkTextGridWindow *)showQuoteBoxIn:(GlkController *)ctl {
    [self print:@"box\n" style:style_Input window:kStory in:ctl];
    [self print:@"A quotation appears.\n" style:style_Normal window:kStory in:ctl];

    // The status window is split tall enough to hold the box. Every row of
    // the box is first filled with spaces, then any text is printed over the
    // fill.
    [self sizeStatusTo:kBoxFirstRow + kBoxRows + 1 in:ctl];
    [self setReverse:YES in:ctl];
    NSArray<NSString *> *quote = @[@"", @"Beware the Jabberwock, my son!", @"", @"-- Lewis Carroll", @""];
    XCTAssertEqual(quote.count, kBoxRows);
    for (NSUInteger i = 0; i < kBoxRows; i++) {
        int row = (int)(kBoxFirstRow + i);
        [self moveToColumn:(int)kBoxColumn row:row in:ctl];
        [self print:[self spaces:kBoxWidth] style:style_Normal window:kStatus in:ctl];
        [self moveToColumn:(int)kBoxColumn + 2 row:row in:ctl];
        if (quote[i].length)
            [self print:quote[i] style:style_Normal window:kStatus in:ctl];
    }
    [self setReverse:NO in:ctl];

    [self send:QUOTEBOX to:ctl a1:kStatus a2:(int)kLinesToSkip a3:0 text:nil];

    [self sizeStatusTo:1 in:ctl];
    [self moveToColumn:0 row:0 in:ctl];
    [self print:@"\n>" style:style_Normal window:kStory in:ctl];

    XCTAssertEqual(ctl.quoteBoxes.count, 1UL, @"quotebox: should have found the reverse video block");
    GlkTextGridWindow *box = ctl.quoteBoxes.lastObject;
    XCTAssertEqual((NSUInteger)box.quoteboxSize.width, kBoxWidth);
    XCTAssertEqual((NSUInteger)box.quoteboxSize.height, kBoxRows);
    XCTAssertEqual(box.quoteboxColumn, kBoxColumn);
    return box;
}

// The player types anything else: an ordinary turn that redraws the status
// line and prints a reply and the next prompt. Like showQuoteBoxIn:, it stops
// short of asking for input.
- (void)takeOrdinaryTurnIn:(GlkController *)ctl {
    [self print:@"look\n" style:style_Input window:kStory in:ctl];
    [self drawStatusLineIn:ctl];
    [self print:@"Turn taken.\n\n>" style:style_Normal window:kStory in:ctl];
}

// The interpreter answers an arrange or preferences event: it sizes its
// windows again, prints nothing, and goes back to waiting.
- (void)answerArrangeEventIn:(GlkController *)ctl {
    [self sizeStatusTo:1 in:ctl];
    [self moveToColumn:0 row:0 in:ctl];
    [self nextEventIn:ctl];
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

// A relaunch of the game from the two GUI autosaves: autosave-GUI.plist,
// normally written when the interpreter sends AUTOSAVE, and the late one
// written when the game window closes. Returns the new controller, waiting
// for input.
- (GlkController *)relaunchFromAutosave:(GlkController *)restored late:(GlkController *)restoredLate {
    GlkController *ctl = [self makeController];
    ctl->restoredController = restored;
    ctl->restoredControllerLate = restoredLate;
    ctl->shouldRestoreUI = YES;
    ctl->autorestoring = YES;
    // restoreUI runs on the NEXTEVENT that finds eventcount at 2.
    ctl.eventcount = 2;

    // Bocfel restores its own state from the interpreter autosave. Then it
    // opens two windows, plays its scrollback history into the first -- the
    // quotation is in there in a plain text form -- and closes both again,
    // before asking for input in the windows it had.
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextBuffer a2:kStory a3:0 text:nil], kStory);
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextGrid a2:kStatus a3:0 text:nil], kStatus);
    [self print:@"Quote box test.\nType BOX for a quotation, QUIT to stop.\n\n>"
          style:style_Normal window:kStory in:ctl];
    [self print:@"box\n" style:style_Input window:kStory in:ctl];
    [self print:@"A quotation appears.\n" style:style_Normal window:kStory in:ctl];
    [self print:@"[ Beware the Jabberwock, my son!\n  \n  -- Lewis Carroll]\n\n\n>"
          style:style_Normal window:kStory in:ctl];
    [self send:DELWIN to:ctl a1:kStory a2:0 a3:0 text:nil];
    [self send:DELWIN to:ctl a1:kStatus a2:0 a3:0 text:nil];
    [self nextEventIn:ctl];
    XCTAssertFalse(ctl->shouldRestoreUI, @"the first NEXTEVENT should have run restoreUI");

    // restoreUI defers postRestoreArrange:, which shows the game window and
    // the autorestore alert. Do the rest of what it does: the preferences
    // pass, which sends the interpreter an arrange event to answer.
    [NSObject cancelPreviousPerformRequestsWithTarget:ctl
                                             selector:@selector(postRestoreArrange:)
                                               object:nil];
    [ctl notePreferencesChanged:[NSNotification notificationWithName:@"PreferencesChanged"
                                                              object:ctl.theme]];
    ctl->autorestoring = NO;
    [self answerArrangeEventIn:ctl];
    return ctl;
}

// Quits a game that has just sent AUTOSAVE (see showQuoteBoxIn:) and
// relaunches it.
- (GlkController *)quitAndRelaunch:(GlkController *)ctl {
    GlkController *restored = [self archivedCopyOf:ctl];
    [self nextEventIn:ctl];
    for (GlkTextGridWindow *box in ctl.quoteBoxes)
        [self waitForBoxToAppear:box];
    GlkController *restoredLate = [self archivedCopyOf:ctl];
    return [self relaunchFromAutosave:restored late:restoredLate];
}

#pragma mark - Looking at the box

// Lets the delayed quoteboxAdjustSize: that quotebox: and
// notePreferencesChanged: schedule run, and its fade-in finish.
- (void)waitForBoxToAppear:(GlkTextGridWindow *)box {
    NSDate *timeout = [NSDate dateWithTimeIntervalSinceNow:5];
    while ((box.superview == nil || box.alphaValue < 1) && timeout.timeIntervalSinceNow > 0) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                 beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
    }
    XCTAssertNotNil(box.superview, @"the quote box was never put on screen");
    XCTAssertEqual(box.alphaValue, 1, @"the quote box never faded in");
    XCTAssertFalse(box.hidden);
}

- (void)waitForBoxToDisappear:(GlkTextGridWindow *)box {
    NSDate *timeout = [NSDate dateWithTimeIntervalSinceNow:5];
    while (box.superview != nil && timeout.timeIntervalSinceNow > 0) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                 beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
    }
    XCTAssertNil(box.superview, @"the quote box was never taken off screen");
    XCTAssertTrue(box.hidden);
}

// The controller still shows this box over the story window, and is not
// fading it out.
- (void)assertBox:(GlkTextGridWindow *)box isShownIn:(GlkController *)ctl {
    GlkTextBufferWindow *story = (GlkTextBufferWindow *)ctl.gwindows[@(kStory)];
    XCTAssertTrue([story isKindOfClass:[GlkTextBufferWindow class]]);
    XCTAssertEqual(ctl.quoteBoxes.count, 1UL);
    XCTAssertEqual(ctl.quoteBoxes.lastObject, box);
    XCTAssertEqual(story.quoteBox, box);
    XCTAssertEqual(box.glkctl, ctl);
    XCTAssertEqual(box.quoteboxParent, story.textview.enclosingScrollView);
    XCTAssertTrue([box isDescendantOf:story], @"the box floats over the story window");
    XCTAssertFalse(box.hidden);
    XCTAssertEqual(box.alphaValue, 1);
}

// The controller has let go of the box, and it fades out and leaves the
// screen.
- (void)assertBox:(GlkTextGridWindow *)box fadesOutIn:(GlkController *)ctl {
    GlkTextBufferWindow *story = (GlkTextBufferWindow *)ctl.gwindows[@(kStory)];
    XCTAssertEqual(ctl.quoteBoxes.count, 0UL, @"the controller should have let go of the quote box");
    XCTAssertNil(story.quoteBox);
    XCTAssertNil(box.quoteboxParent);
    [self waitForBoxToDisappear:box];
    XCTAssertFalse([box isDescendantOf:story]);
}

- (NSArray<NSString *> *)rowsOf:(GlkTextGridWindow *)box {
    NSString *string = box.textview.string;
    if ([string hasSuffix:@"\n"])
        string = [string substringToIndex:string.length - 1];
    return [string componentsSeparatedByString:@"\n"];
}

// Swaps every non-breaking space in the box for an ordinary one, on screen
// and in the buffer, which is how a box without the workaround looks.
- (void)removeProtectionFrom:(GlkTextGridWindow *)box {
    NSString *nbsp = [NSString stringWithCharacters:&kNBSP length:1];
    for (NSMutableAttributedString *storage in @[box.textview.textStorage, box.bufferTextStorage]) {
        NSRange range;
        while ((range = [storage.string rangeOfString:nbsp]).location != NSNotFound)
            [storage replaceCharactersInRange:range withString:@" "];
    }
    box.textview.needsDisplay = YES;
}

// The stored text: every blank row leads with a non-breaking space that still
// carries the box's background, and the rows with text are left as printed.
- (void)assertRowsAreProtectedIn:(GlkTextGridWindow *)box {
    NSArray<NSString *> *rows = [self rowsOf:box];
    XCTAssertEqual(rows.count, kBoxRows);
    if (rows.count != kBoxRows)
        return;

    NSString *nbsp = [NSString stringWithCharacters:&kNBSP length:1];
    NSString *blank = [nbsp stringByAppendingString:[self spaces:kBoxWidth - 1]];
    XCTAssertEqualObjects(rows[0], blank, @"blank top row must lead with a non-breaking space");
    XCTAssertEqualObjects(rows[1], @"  Beware the Jabberwock, my son!  ");
    XCTAssertEqualObjects(rows[2], blank, @"blank middle row must lead with a non-breaking space");
    XCTAssertEqualObjects(rows[3], @"  -- Lewis Carroll                ");
    XCTAssertEqualObjects(rows[4], blank, @"blank bottom row must lead with a non-breaking space");

    NSTextStorage *storage = box.textview.textStorage;
    NSColor *textRowBackground = [storage attribute:NSBackgroundColorAttributeName
                                            atIndex:kBoxWidth + 1
                                     effectiveRange:NULL];
    XCTAssertNotNil(textRowBackground, @"box cells are reverse video and so have a background");
    for (NSUInteger row = 0; row < kBoxRows; row += 2) {
        NSColor *background = [storage attribute:NSBackgroundColorAttributeName
                                         atIndex:row * (kBoxWidth + 1)
                                  effectiveRange:NULL];
        XCTAssertEqualObjects(background, textRowBackground,
                              @"the substituted cell of row %lu lost its background", row);
    }
}

// What is painted: the middle of every row of the box, blank or not, shows
// the same solid background as a padding cell next to the text.
- (void)assertRowsArePaintedIn:(GlkTextGridWindow *)box {
    [box layoutSubtreeIfNeeded];
    NSRect bounds = box.bounds;
    XCTAssertGreaterThan(NSWidth(bounds), 0);
    XCTAssertGreaterThan(NSHeight(bounds), 0);
    if (NSIsEmptyRect(bounds))
        return;

    NSBitmapImageRep *rep = [box bitmapImageRepForCachingDisplayInRect:bounds];
    [box cacheDisplayInRect:bounds toBitmapImageRep:rep];

    NSTextView *textview = box.textview;
    NSLayoutManager *layoutManager = textview.layoutManager;
    CGFloat scale = (CGFloat)rep.pixelsWide / NSWidth(bounds);

    // The centre of a cell of the box, as a colour of the bitmap.
    NSColor * (^cellColor)(NSUInteger, NSUInteger) = ^NSColor *(NSUInteger row, NSUInteger column) {
        NSUInteger index = row * (kBoxWidth + 1) + column;
        NSRange glyphs = [layoutManager glyphRangeForCharacterRange:NSMakeRange(index, 1)
                                               actualCharacterRange:NULL];
        NSRect rect = [layoutManager boundingRectForGlyphRange:glyphs
                                               inTextContainer:textview.textContainer];
        NSPoint origin = textview.textContainerOrigin;
        NSPoint centre = NSMakePoint(NSMidX(rect) + origin.x, NSMidY(rect) + origin.y);
        centre = [box convertPoint:centre fromView:textview];
        // Both the view and the bitmap have their origin at the top left.
        NSColor *color = [rep colorAtX:(NSInteger)floor(centre.x * scale)
                                     y:(NSInteger)floor(centre.y * scale)];
        return [color colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
    };

    // Column 0 of a text row is padding: reverse video with nothing drawn on it.
    NSColor *expected = cellColor(1, 0);
    XCTAssertNotNil(expected);
    XCTAssertGreaterThan(expected.alphaComponent, 0.99, @"the padding of a text row should be opaque");

    for (NSUInteger row = 0; row < kBoxRows; row += 2) {
        for (NSNumber *column in @[@1, @(kBoxWidth / 2), @(kBoxWidth - 2)]) {
            NSColor *color = cellColor(row, column.unsignedIntegerValue);
            BOOL same = color != nil
                && fabs(color.redComponent - expected.redComponent) < 0.02
                && fabs(color.greenComponent - expected.greenComponent) < 0.02
                && fabs(color.blueComponent - expected.blueComponent) < 0.02
                && fabs(color.alphaComponent - expected.alphaComponent) < 0.02;
            XCTAssertTrue(same, @"blank row %lu is not painted at column %@: %@, expected %@",
                          row, column, color, expected);
        }
    }
}

#pragma mark - A live quote box

- (void)testBlankRowsOfLiveQuoteBoxAreKeptFromCollapsing {
    GlkController *ctl = [self makeController];
    [self startGameIn:ctl];
    GlkTextGridWindow *box = [self showQuoteBoxIn:ctl];
    [self nextEventIn:ctl];
    [self waitForBoxToAppear:box];

    [self assertBox:box isShownIn:ctl];
    [self assertRowsAreProtectedIn:box];
    [self assertRowsArePaintedIn:box];
}

// The painting check has to be able to fail: the same box with its
// non-breaking spaces taken out must come out striped. If this test fails,
// the text system has stopped collapsing rows of spaces in the default grid
// font, and the painting assertions in this file no longer prove that the
// workaround is in place (the assertions on the stored text still do).
- (void)testUnprotectedBlankRowsDoCollapse {
    GlkController *ctl = [self makeController];
    [self startGameIn:ctl];
    GlkTextGridWindow *box = [self showQuoteBoxIn:ctl];
    [self nextEventIn:ctl];
    [self waitForBoxToAppear:box];
    [self removeProtectionFrom:box];

    XCTExpectFailure(@"rows of plain spaces lose their background in Source Code Pro");
    [self assertRowsArePaintedIn:box];
}

// The box stays up for as long as the game prints nothing -- the interpreter
// answering a window resize, say -- and goes away at the end of the next turn
// that prints.
- (void)testLiveQuoteBoxFadesOutAfterNextTurn {
    GlkController *ctl = [self makeController];
    [self startGameIn:ctl];
    GlkTextGridWindow *box = [self showQuoteBoxIn:ctl];
    [self nextEventIn:ctl];
    [self waitForBoxToAppear:box];

    [self answerArrangeEventIn:ctl];
    [self answerArrangeEventIn:ctl];
    [self assertBox:box isShownIn:ctl];

    [self takeOrdinaryTurnIn:ctl];
    [self assertBox:box isShownIn:ctl];
    [self nextEventIn:ctl];
    [self assertBox:box fadesOutIn:ctl];
}

#pragma mark - Autorestore

// Quit with a quotation on screen and relaunch: the box is adopted by the new
// controller, put back over the story window, and painted solid. The
// scrollback history that bocfel prints while restoring must not count as the
// game having moved on.
- (void)testQuoteBoxSurvivesAutorestore {
    GlkController *ctl = [self makeController];
    [self startGameIn:ctl];
    GlkTextGridWindow *box = [self showQuoteBoxIn:ctl];

    GlkController *relaunched = [self quitAndRelaunch:ctl];

    XCTAssertEqual(relaunched.quoteBoxes.count, 1UL, @"restoreUI should adopt the archived quote box");
    GlkTextGridWindow *restoredBox = relaunched.quoteBoxes.lastObject;
    XCTAssertNotNil(restoredBox);
    XCTAssertNotEqual(restoredBox, box, @"the restored box is the unarchived one");
    [self waitForBoxToAppear:restoredBox];

    [self assertBox:restoredBox isShownIn:relaunched];
    XCTAssertEqual((NSUInteger)restoredBox.quoteboxSize.width, kBoxWidth);
    XCTAssertEqual((NSUInteger)restoredBox.quoteboxSize.height, kBoxRows);
    [self assertRowsAreProtectedIn:restoredBox];
    [self assertRowsArePaintedIn:restoredBox];
}

// A restored box behaves like a live one from there on: it stays while
// nothing is printed and fades out after the next turn.
- (void)testRestoredQuoteBoxFadesOutAfterNextTurn {
    GlkController *ctl = [self makeController];
    [self startGameIn:ctl];
    [self showQuoteBoxIn:ctl];

    GlkController *relaunched = [self quitAndRelaunch:ctl];
    GlkTextGridWindow *box = relaunched.quoteBoxes.lastObject;
    XCTAssertNotNil(box);
    [self waitForBoxToAppear:box];

    [self answerArrangeEventIn:relaunched];
    [self assertBox:box isShownIn:relaunched];

    [self takeOrdinaryTurnIn:relaunched];
    [self assertBox:box isShownIn:relaunched];
    [self nextEventIn:relaunched];
    [self assertBox:box fadesOutIn:relaunched];
}

// The GUI autosave is written again whenever the preferences change, which
// may well be after the turn with the quotation has ended; and when it is
// missing the late autosave takes its place. Either way the archived box has
// already been stamped with the print count of the session that is gone. The
// box must be restored all the same, and still last exactly one turn.
- (void)testQuoteBoxRestoredFromAutosaveWrittenAfterEndOfTurn {
    GlkController *ctl = [self makeController];
    [self startGameIn:ctl];
    GlkTextGridWindow *box = [self showQuoteBoxIn:ctl];
    [self nextEventIn:ctl];
    [self waitForBoxToAppear:box];

    GlkController *relaunched = [self relaunchFromAutosave:[self archivedCopyOf:ctl]
                                                      late:[self archivedCopyOf:ctl]];

    XCTAssertEqual(relaunched.quoteBoxes.count, 1UL, @"restoreUI should adopt the archived quote box");
    GlkTextGridWindow *restoredBox = relaunched.quoteBoxes.lastObject;
    [self waitForBoxToAppear:restoredBox];
    [self assertBox:restoredBox isShownIn:relaunched];
    [self assertRowsAreProtectedIn:restoredBox];
    [self assertRowsArePaintedIn:restoredBox];

    [self answerArrangeEventIn:relaunched];
    [self assertBox:restoredBox isShownIn:relaunched];

    [self takeOrdinaryTurnIn:relaunched];
    [self nextEventIn:relaunched];
    [self assertBox:restoredBox fadesOutIn:relaunched];
}

// A game that opens with a quotation, as Curses does, closed before the first
// command: there is no interpreter autosave yet, so on relaunch the game
// starts over and puts up the box again, and only the UI is restored around
// it. Nothing is flushed during an autorestore, so the story window still has
// no size -- its frame is pending -- when the box works out where it goes. It
// must be centred in the frame the window is about to get, not hang off the
// left edge of a window zero points wide.
- (void)testQuoteBoxAtGameStartIsCentredWhenOnlyTheUIIsRestored {
    GlkController *ctl = [self makeController];
    [self openWindowsIn:ctl];
    GlkTextGridWindow *box = [self showQuoteBoxIn:ctl];
    [self nextEventIn:ctl];
    [self waitForBoxToAppear:box];
    NSRect centred = box.frame;
    XCTAssertGreaterThan(NSMinX(centred), 0);
    GlkController *restoredLate = [self archivedCopyOf:ctl];

    GlkController *relaunched = [self makeController];
    relaunched->restoredControllerLate = restoredLate;
    relaunched->restoreUIOnly = YES;
    relaunched->shouldRestoreUI = YES;
    relaunched->autorestoring = YES;
    relaunched.eventcount = 2;
    [self openWindowsIn:relaunched];
    GlkTextGridWindow *newBox = [self showQuoteBoxIn:relaunched];
    [self nextEventIn:relaunched];
    XCTAssertFalse(relaunched->shouldRestoreUI, @"the first NEXTEVENT should have run restoreUI");
    [NSObject cancelPreviousPerformRequestsWithTarget:relaunched
                                             selector:@selector(postRestoreArrange:)
                                               object:nil];

    GlkWindow *story = relaunched.gwindows[@(kStory)];
    XCTAssertTrue(story.framePending, @"the story window should not have been laid out yet");
    XCTAssertEqual(NSWidth(story.frame), 0);

    [self waitForBoxToAppear:newBox];
    XCTAssertEqual(NSMinX(newBox.frame), NSMinX(centred));
    XCTAssertEqual(NSWidth(newBox.frame), NSWidth(centred));
    [self assertRowsAreProtectedIn:newBox];
}

// Autosaves written by builds that did not protect quote boxes hold the box
// with ordinary spaces throughout. Restoring one of those repairs it.
- (void)testUnprotectedQuoteBoxInOldAutosaveIsRepairedOnRestore {
    GlkController *ctl = [self makeController];
    [self startGameIn:ctl];
    GlkTextGridWindow *box = [self showQuoteBoxIn:ctl];
    [self nextEventIn:ctl];
    [self waitForBoxToAppear:box];
    [self removeProtectionFrom:box];
    XCTAssertFalse([box.textview.string containsString:[NSString stringWithCharacters:&kNBSP length:1]]);

    GlkController *relaunched = [self relaunchFromAutosave:[self archivedCopyOf:ctl]
                                                      late:[self archivedCopyOf:ctl]];

    XCTAssertEqual(relaunched.quoteBoxes.count, 1UL);
    GlkTextGridWindow *restoredBox = relaunched.quoteBoxes.lastObject;
    [self waitForBoxToAppear:restoredBox];

    [self assertRowsAreProtectedIn:restoredBox];
    [self assertRowsArePaintedIn:restoredBox];
}

// A quotation is only shown for the turn it appeared on. If the game went on
// printing after the box was put up, the autosave is from a later turn and the
// box must not come back.
- (void)testQuoteBoxFromAnEarlierTurnIsNotRestored {
    GlkController *ctl = [self makeController];
    [self startGameIn:ctl];
    GlkTextGridWindow *box = [self showQuoteBoxIn:ctl];
    [self nextEventIn:ctl];
    [self waitForBoxToAppear:box];
    [self takeOrdinaryTurnIn:ctl];

    GlkController *relaunched = [self quitAndRelaunch:ctl];

    XCTAssertEqual(relaunched.quoteBoxes.count, 0UL, @"a stale quote box must not be adopted");
    GlkTextBufferWindow *story = (GlkTextBufferWindow *)relaunched.gwindows[@(kStory)];
    XCTAssertTrue([story isKindOfClass:[GlkTextBufferWindow class]]);
    for (NSView *view in story.textview.enclosingScrollView.subviews)
        XCTAssertFalse([view isKindOfClass:[GlkTextGridWindow class]] && !view.hidden,
                       @"no quote box should be left floating over the story");
}

@end
