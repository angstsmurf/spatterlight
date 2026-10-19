//
//  GridWindowTests.m
//  SpatterlightTests
//
//  Pins the character grid that GlkTextGridWindow keeps in its buffer: rows of
//  cols cells, each row but the last followed by a newline, the cursor that
//  printing advances, and how the grid is cut and padded when the window
//  changes size. Also the input that is placed on the grid (line input, mouse
//  clicks) and the background colour taken from the grid's text.
//
//  The grid is driven directly, through the methods GlkController calls when
//  it handles PRINT, MOVETO, UNPRINT and the rest, on a controller without an
//  interpreter like the one in QuoteBoxTests.
//

#import "GlkControllerTestCase.h"

#import "GlkController.h"
#import "GlkController_Private.h"
#import "GlkController+GlkRequests.h"
#import "GlkWindow.h"
#import "GlkTextGridWindow.h"
#import "GridTextView.h"
#import "InputTextField.h"
#import "GlkEvent.h"
#import "Theme.h"
#import "Game.h"
#import "BuiltInThemes.h"

#include "glk.h"
#include "protocol.h"

// NEWWIN only creates the first unused id, so the only window is 0.
static const int kGrid = 0;
static const unichar kNBSP = 0xa0;

@interface GridWindowTests : GlkControllerTestCase
@end

@implementation GridWindowTests

- (void)setUp {
    [super setUp];
    [self makeGameWithIfid:@"ZCODE-1-261019-0000" theme:self.theme];
}

#pragma mark - A grid without an interpreter

// The frame SIZWIN would give a grid of this many cells.
- (NSRect)frameForColumns:(NSUInteger)columns rows:(NSUInteger)rows {
    Theme *theme = self.theme;
    CGFloat width = ceil(columns * theme.cellWidth + 2 * theme.gridMarginX + 10);
    CGFloat height = ceil(rows * theme.cellHeight + 2 * theme.gridMarginY);
    return NSMakeRect(0, 0, width, height);
}

- (GlkTextGridWindow *)gridWithColumns:(NSUInteger)columns rows:(NSUInteger)rows {
    GlkController *ctl = [self makeController];
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextGrid a2:kGrid a3:0], kGrid);
    GlkTextGridWindow *grid = (GlkTextGridWindow *)ctl.gwindows[@(kGrid)];
    XCTAssertTrue([grid isKindOfClass:[GlkTextGridWindow class]]);
    grid.frame = [self frameForColumns:columns rows:rows];
    [grid flushDisplay];
    XCTAssertEqual(grid.currentSizeInChars.width, (CGFloat)columns);
    XCTAssertEqual(grid.currentSizeInChars.height, (CGFloat)rows);
    return grid;
}

// The rows of the grid buffer, with the non-breaking spaces that keep blank
// rows from collapsing read as the spaces they stand in for.
- (NSArray<NSString *> *)rowsOf:(GlkTextGridWindow *)grid {
    NSString *nbsp = [NSString stringWithCharacters:&kNBSP length:1];
    NSString *text = [grid.bufferTextStorage.string stringByReplacingOccurrencesOfString:nbsp withString:@" "];
    return [text componentsSeparatedByString:@"\n"];
}

- (NSString *)row:(NSString *)text width:(NSUInteger)width {
    return [text stringByPaddingToLength:width withString:@" " startingAtIndex:0];
}

- (void)assertGrid:(GlkTextGridWindow *)grid is:(NSArray<NSString *> *)expected width:(NSUInteger)width {
    NSArray<NSString *> *rows = [self rowsOf:grid];
    XCTAssertEqual(rows.count, expected.count);
    for (NSUInteger i = 0; i < MIN(rows.count, expected.count); i++)
        XCTAssertEqualObjects(rows[i], [self row:expected[i] width:width], @"row %lu", i);
}

#pragma mark - Layout

- (void)testNewGridIsRowsOfSpacesSeparatedByNewlines {
    GlkTextGridWindow *grid = [self gridWithColumns:10 rows:3];
    XCTAssertEqual(grid.bufferTextStorage.length, 3UL * 11 - 1);
    [self assertGrid:grid is:@[@"", @"", @""] width:10];
}

- (void)testNarrowingCutsTheEndOfEveryRow {
    GlkTextGridWindow *grid = [self gridWithColumns:10 rows:3];
    for (NSUInteger r = 0; r < 3; r++) {
        [grid moveToColumn:0 row:r];
        [grid putString:@"abcdefghij" style:style_Normal];
    }
    grid.frame = [self frameForColumns:6 rows:3];
    [grid flushDisplay];
    XCTAssertEqual(grid.currentSizeInChars.width, 6);
    [self assertGrid:grid is:@[@"abcdef", @"abcdef", @"abcdef"] width:6];
}

- (void)testWideningPadsEveryRow {
    GlkTextGridWindow *grid = [self gridWithColumns:6 rows:3];
    for (NSUInteger r = 0; r < 3; r++) {
        [grid moveToColumn:0 row:r];
        [grid putString:@"abcdef" style:style_Normal];
    }
    grid.frame = [self frameForColumns:12 rows:3];
    [grid flushDisplay];
    XCTAssertEqual(grid.currentSizeInChars.width, 12);
    [self assertGrid:grid is:@[@"abcdef", @"abcdef", @"abcdef"] width:12];
}

- (void)testGrowingAndShrinkingHeightAddsAndDropsRows {
    GlkTextGridWindow *grid = [self gridWithColumns:8 rows:2];
    [grid moveToColumn:0 row:0];
    [grid putString:@"top" style:style_Normal];
    [grid moveToColumn:0 row:1];
    [grid putString:@"bottom" style:style_Normal];

    grid.frame = [self frameForColumns:8 rows:4];
    [grid flushDisplay];
    [self assertGrid:grid is:@[@"top", @"bottom", @"", @""] width:8];

    grid.frame = [self frameForColumns:8 rows:1];
    [grid flushDisplay];
    [self assertGrid:grid is:@[@"top"] width:8];
}

- (void)testClearBlanksTheGridAndHomesTheCursor {
    GlkTextGridWindow *grid = [self gridWithColumns:10 rows:2];
    [grid moveToColumn:3 row:1];
    [grid putString:@"text" style:style_Normal];
    [grid clear];
    [grid flushDisplay];
    [self assertGrid:grid is:@[@"", @""] width:10];
    XCTAssertEqual(grid.indexOfPos, 0UL);
}

#pragma mark - Printing

- (void)testPrintingWrapsAtTheRightEdge {
    GlkTextGridWindow *grid = [self gridWithColumns:10 rows:3];
    [grid moveToColumn:0 row:0];
    [grid putString:@"abcdefghijKLM" style:style_Normal];
    [self assertGrid:grid is:@[@"abcdefghij", @"KLM", @""] width:10];
    XCTAssertEqual(grid.indexOfPos, 11UL + 3);
}

- (void)testNewlineMovesToTheStartOfTheNextRow {
    GlkTextGridWindow *grid = [self gridWithColumns:10 rows:3];
    [grid moveToColumn:4 row:0];
    [grid putString:@"ab\ncd\ref" style:style_Normal];
    [self assertGrid:grid is:@[@"    ab", @"cd", @"ef"] width:10];
}

- (void)testAnEmptyLineLeavesABlankRow {
    GlkTextGridWindow *grid = [self gridWithColumns:10 rows:3];
    [grid moveToColumn:0 row:0];
    [grid putString:@"a\n\nb" style:style_Normal];
    [self assertGrid:grid is:@[@"a", @"", @"b"] width:10];
}

// \r and \n are each a newline, so \r\n moves down two rows.
- (void)testCarriageReturnLinefeedIsTwoNewlines {
    GlkTextGridWindow *grid = [self gridWithColumns:10 rows:3];
    [grid moveToColumn:0 row:0];
    [grid putString:@"a\r\nb" style:style_Normal];
    [self assertGrid:grid is:@[@"a", @"", @"b"] width:10];
}

// A row one short of the width is not followed by a blank row (Comprehend's
// room descriptions).
- (void)testNewlineAfterARowOneShortOfTheWidth {
    GlkTextGridWindow *grid = [self gridWithColumns:10 rows:3];
    [grid moveToColumn:0 row:0];
    [grid putString:@"abcdefghi\nX" style:style_Normal];
    [self assertGrid:grid is:@[@"abcdefghi", @"X", @""] width:10];
}

// A full row has already put the cursor on the next row, so a newline after
// it moves on once more.
- (void)testNewlineAfterAFullRow {
    GlkTextGridWindow *grid = [self gridWithColumns:10 rows:3];
    [grid moveToColumn:0 row:0];
    [grid putString:@"abcdefghij\nX" style:style_Normal];
    [self assertGrid:grid is:@[@"abcdefghij", @"", @"X"] width:10];
}

- (void)testPrintingOverwritesInPlace {
    GlkTextGridWindow *grid = [self gridWithColumns:10 rows:1];
    [grid moveToColumn:0 row:0];
    [grid putString:@"abcdefghij" style:style_Normal];
    [grid moveToColumn:3 row:0];
    [grid putString:@"XY" style:style_Normal];
    [self assertGrid:grid is:@[@"abcXYfghij"] width:10];
}

- (void)testPrintingOffTheBottomIsDropped {
    GlkTextGridWindow *grid = [self gridWithColumns:10 rows:2];
    [grid moveToColumn:0 row:1];
    [grid putString:@"0123456789abcdefghijklmno" style:style_Normal];
    XCTAssertEqual(grid.bufferTextStorage.length, 2UL * 11 - 1);
    [self assertGrid:grid is:@[@"", @"0123456789"] width:10];

    [grid moveToColumn:0 row:5];
    [grid putString:@"lost" style:style_Normal];
    XCTAssertEqual(grid.bufferTextStorage.length, 2UL * 11 - 1);
    [self assertGrid:grid is:@[@"", @"0123456789"] width:10];
}

- (void)testFlushShowsTheBufferInTheTextView {
    GlkTextGridWindow *grid = [self gridWithColumns:10 rows:2];
    [grid moveToColumn:2 row:1];
    [grid putString:@"shown" style:style_Normal];
    [grid flushDisplay];
    XCTAssertEqualObjects(grid.textview.string, grid.bufferTextStorage.string);
    XCTAssertEqual([grid characterAtPoint:NSMakePoint(2, 1)], 's');
    XCTAssertEqual([grid characterAtPoint:NSMakePoint(6, 1)], 'n');
    XCTAssertEqual([grid characterAtPoint:NSMakePoint(9, 5)], 0);
}

#pragma mark - Unprinting

- (void)testUnputStringErasesTheTextBeforeTheCursor {
    GlkTextGridWindow *grid = [self gridWithColumns:10 rows:2];
    [grid moveToColumn:2 row:1];
    [grid putString:@"look" style:style_Normal];
    XCTAssertEqual([grid unputString:@"LOOK"], 4UL);
    [self assertGrid:grid is:@[@"", @""] width:10];
    XCTAssertEqual(grid.indexOfPos, 11UL + 2);
}

- (void)testUnputStringThatDoesNotMatchLeavesTheGridAlone {
    GlkTextGridWindow *grid = [self gridWithColumns:10 rows:2];
    [grid moveToColumn:2 row:1];
    [grid putString:@"look" style:style_Normal];
    XCTAssertEqual([grid unputString:@"lock"], 0UL);
    XCTAssertEqual([grid unputString:@"a long string"], 0UL);
    [self assertGrid:grid is:@[@"", @"  look"] width:10];
    XCTAssertEqual(grid.indexOfPos, 11UL + 6);
}

- (void)testUnputStringDoesNotReachIntoThePreviousRow {
    GlkTextGridWindow *grid = [self gridWithColumns:10 rows:2];
    [grid moveToColumn:7 row:0];
    [grid putString:@"abc" style:style_Normal];
    [grid moveToColumn:2 row:1];
    [grid putString:@"de" style:style_Normal];
    XCTAssertEqual([grid unputString:@"bcde"], 0UL);
    XCTAssertEqual([grid unputString:@"c\nde"], 0UL);
    [self assertGrid:grid is:@[@"       abc", @"  de"] width:10];
    XCTAssertEqual(grid.indexOfPos, 11UL + 4);
}

#pragma mark - Line input

- (void)testLineInputFieldSitsAtTheCursor {
    GlkTextGridWindow *grid = [self gridWithColumns:20 rows:3];
    [grid moveToColumn:4 row:2];
    [grid initLine:@"pre" maxLength:100];
    InputTextField *input = grid.input;
    XCTAssertNotNil(input);
    XCTAssertEqualObjects(input.stringValue, @"pre");
    Theme *theme = self.theme;
    XCTAssertEqualWithAccuracy(NSMinY(input.frame), theme.gridMarginY + 2 * theme.cellHeight, 0.01);
    XCTAssertEqualWithAccuracy(NSWidth(input.frame) - grid.textview.textContainer.lineFragmentPadding,
                               (20 + 1 - 4) * theme.cellWidth, 0.01, @"clipped to the rest of the row");
}

- (void)testLineInputBelowTheGridIsMovedToTheLastRow {
    GlkTextGridWindow *grid = [self gridWithColumns:20 rows:3];
    [grid moveToColumn:0 row:7];
    [grid initLine:@"" maxLength:10];
    XCTAssertNotNil(grid.input);
    Theme *theme = self.theme;
    XCTAssertEqualWithAccuracy(NSMinY(grid.input.frame), theme.gridMarginY + 2 * theme.cellHeight, 0.01);
}

- (void)testLineInputOnAnEmptyBufferDoesNotThrow {
    GlkTextGridWindow *grid = [self gridWithColumns:20 rows:3];
    grid.bufferTextStorage = [[NSMutableAttributedString alloc] init];
    [grid moveToColumn:0 row:0];
    XCTAssertNoThrow([grid initLine:@"" maxLength:10]);
}

- (void)testEnteredLineIsPrintedOnTheGridAndQueued {
    GlkTextGridWindow *grid = [self gridWithColumns:20 rows:3];
    [grid moveToColumn:2 row:1];
    [grid initLine:@"" maxLength:10];
    GlkController *ctl = grid.glkctl;
    NSUInteger queued = ctl.queue.count;
    [grid sendCommandLine:@"north"];
    XCTAssertNil(grid.input);
    XCTAssertFalse(grid.hasLineRequest);
    [self assertGrid:grid is:@[@"", @"  north", @""] width:20];
    XCTAssertEqual(grid.indexOfPos, 2UL * 21, @"the cursor moves to the start of the next row");
    XCTAssertEqual(ctl.queue.count, queued + 1);
    XCTAssertEqual(((GlkEvent *)ctl.queue.lastObject).type, EVTLINE);
}

#pragma mark - Mouse input

// City of Secrets has hyperlinks but asks for mouse input rather than
// hyperlink input: a click on a link is sent as a click on its cell.
- (void)testLinkClickWithOnlyAMouseRequestIsSentAsAClick {
    GlkTextGridWindow *grid = [self gridWithColumns:10 rows:3];
    GlkController *ctl = grid.glkctl;
    NSUInteger queued = ctl.queue.count;

    XCTAssertFalse([grid textView:grid.textview clickedOnLink:@1 atIndex:11 + 3]);
    XCTAssertEqual(ctl.queue.count, queued, @"no request, nothing sent");

    [grid initMouse];
    XCTAssertFalse([grid textView:grid.textview clickedOnLink:@1 atIndex:10], @"the newline is not a cell");
    XCTAssertTrue([grid textView:grid.textview clickedOnLink:@1 atIndex:11 + 3]);
    XCTAssertEqual(ctl.queue.count, queued + 1);
    GlkEvent *click = ctl.queue.lastObject;
    XCTAssertEqual(click.type, EVTMOUSE);
    XCTAssertEqual(click.val1, 3);
    XCTAssertEqual(click.val2, 1);

    XCTAssertFalse([grid textView:grid.textview clickedOnLink:@1 atIndex:11 + 3], @"one click per request");
}

- (void)testLinkClickWithAHyperlinkRequestIsSentAsALink {
    GlkTextGridWindow *grid = [self gridWithColumns:10 rows:3];
    GlkController *ctl = grid.glkctl;
    NSUInteger queued = ctl.queue.count;
    [grid initHyperlink];
    XCTAssertTrue([grid textView:grid.textview clickedOnLink:@42 atIndex:5]);
    XCTAssertEqual(ctl.queue.count, queued + 1);
    XCTAssertEqual(((GlkEvent *)ctl.queue.lastObject).type, EVTHYPER);
    XCTAssertEqual(((GlkEvent *)ctl.queue.lastObject).val1, 42);
}

#pragma mark - Background

// A status line in reverse video that fills the grid gives the window its
// background colour, so the margins around it match.
- (void)testReverseVideoGridGivesTheBackgroundColour {
    GlkTextGridWindow *grid = [self gridWithColumns:20 rows:1];
    NSColor *before = grid.textview.backgroundColor;
    grid.currentReverseVideo = YES;
    [grid moveToColumn:0 row:0];
    [grid putString:[self row:@" Status" width:20] style:style_Normal];
    grid.currentReverseVideo = NO;
    [grid flushDisplay];
    NSColor *reverse = [grid.bufferTextStorage attribute:NSBackgroundColorAttributeName atIndex:0 effectiveRange:NULL];
    XCTAssertNotNil(reverse);
    XCTAssertNotEqualObjects(reverse, before);
    XCTAssertEqualObjects(grid.textview.backgroundColor, reverse);
}

#pragma mark - Autorestore

- (void)testArchiveKeepsTheGridAndCursor {
    GlkTextGridWindow *grid = [self gridWithColumns:12 rows:2];
    [grid moveToColumn:1 row:1];
    [grid putString:@"saved" style:style_Normal];
    [grid flushDisplay];

    NSError *error = nil;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:grid requiringSecureCoding:NO error:&error];
    XCTAssertNil(error);
    NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
    unarchiver.requiresSecureCoding = NO;
    GlkTextGridWindow *copy = [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
    [unarchiver finishDecoding];

    XCTAssertEqualObjects(copy.bufferTextStorage.string, grid.bufferTextStorage.string);
    XCTAssertEqual(copy.indexOfPos, grid.indexOfPos);
    XCTAssertEqual(copy.currentSizeInChars.width, 12);
    XCTAssertEqual(copy.currentSizeInChars.height, 2);
}

@end
