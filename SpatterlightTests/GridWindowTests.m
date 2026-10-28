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
#import "GlkStyle.h"
#import "BuiltInThemes.h"
#import "NSColor+integer.h"

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

// The upper window of a Z-machine game closes by shrinking to zero height
// (Bocfel close_upper_window). Glk's rule for a grid that shrinks is that
// the vanished rows are thrown away, so it comes back blank.
- (void)testCollapsingToNoRowsDropsTheText {
    GlkTextGridWindow *grid = [self gridWithColumns:8 rows:2];
    [grid moveToColumn:0 row:0];
    [grid putString:@"status" style:style_Normal];
    [grid flushDisplay];

    NSRect collapsed = [self frameForColumns:8 rows:2];
    collapsed.size.height = 0;
    grid.frame = collapsed;
    [grid flushDisplay];
    XCTAssertEqual(grid.currentSizeInChars.height, 0.0);
    [grid putString:@"lost" style:style_Normal];

    grid.frame = [self frameForColumns:8 rows:2];
    [grid flushDisplay];
    [self assertGrid:grid is:@[@"", @""] width:8];
    XCTAssertEqual(grid.indexOfPos, 0UL);
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

#pragma mark - Width in cells

// Every built-in theme, with its cell sizes set, sorted by name.
- (NSArray<Theme *> *)builtInThemes {
    [BuiltInThemes createBuiltInThemesInContext:self.context forceRebuild:NO];
    NSFetchRequest *request = [Theme fetchRequest];
    request.sortDescriptors = @[ [NSSortDescriptor sortDescriptorWithKey:@"name" ascending:YES] ];
    NSError *error = nil;
    NSArray<Theme *> *themes = [self.context executeFetchRequest:request error:&error];
    XCTAssertNil(error);
    XCTAssertGreaterThanOrEqual(themes.count, 10UL);
    return themes;
}

- (GlkTextGridWindow *)gridInTheme:(Theme *)theme {
    GlkController *ctl = [self makeControllerWithTheme:theme];
    XCTAssertEqual([self send:NEWWIN to:ctl a1:wintype_TextGrid a2:kGrid a3:0], kGrid);
    GlkTextGridWindow *grid = (GlkTextGridWindow *)ctl.gwindows[@(kGrid)];
    XCTAssertTrue([grid isKindOfClass:[GlkTextGridWindow class]]);
    return grid;
}

// What glkimp's glk_window_get_size answers for a grid this wide: its
// ggridmarginx is the theme's grid margin plus the line fragment padding.
- (NSUInteger)glkColumnsInWidth:(CGFloat)width theme:(Theme *)theme {
    return gli_cells_in(width - 2 * (theme.gridMarginX + 5), theme.cellWidth);
}

// A game wraps and pads its grid lines at the width glk_window_get_size
// gives it, so the app's grid must have exactly that many columns, in every
// theme and at every width, including the fractional cell widths of real
// fonts (Helvetica 13's "W" is 12.27).
- (void)testGridColumnsAreTheColumnsGlkCounts {
    for (Theme *theme in [self builtInThemes]) {
        GlkTextGridWindow *grid = [self gridInTheme:theme];
        XCTAssertGreaterThan(theme.cellWidth, 0, @"%@", theme.name);
        XCTAssertGreaterThan(theme.cellHeight, 0, @"%@", theme.name);
        CGFloat minimum = 2 * (theme.gridMarginX + 5) + theme.cellWidth;
        CGFloat height = gli_cells_span(3, theme.cellHeight) + 2 * theme.gridMarginY;
        for (CGFloat width = ceil(minimum); width <= 1200; width += 1) {
            grid.frame = NSMakeRect(0, 0, width, height);
            [grid flushDisplay];
            NSUInteger expected = [self glkColumnsInWidth:width theme:theme];
            XCTAssertEqual(grid.currentSizeInChars.width, (CGFloat)expected,
                           @"%@ (cell %f, margin %ld) at width %f", theme.name,
                           theme.cellWidth, (long)theme.gridMarginX, width);
            XCTAssertEqual(grid.currentSizeInChars.height, 3.0, @"%@ at width %f", theme.name, width);
            if (grid.currentSizeInChars.width != (CGFloat)expected)
                break;
        }
    }
}

- (void)testGridRowsAreTheRowsGlkCounts {
    for (Theme *theme in [self builtInThemes]) {
        GlkTextGridWindow *grid = [self gridInTheme:theme];
        CGFloat width = gli_cells_span(20, theme.cellWidth) + 2 * (theme.gridMarginX + 5);
        CGFloat minimum = 2 * theme.gridMarginY + theme.cellHeight;
        for (CGFloat height = ceil(minimum); height <= 600; height += 1) {
            grid.frame = NSMakeRect(0, 0, width, height);
            [grid flushDisplay];
            NSUInteger expected = gli_cells_in(height - 2 * theme.gridMarginY, theme.cellHeight);
            XCTAssertEqual(grid.currentSizeInChars.height, (CGFloat)expected,
                           @"%@ (cell height %f) at height %f", theme.name, theme.cellHeight, height);
            XCTAssertEqual(grid.currentSizeInChars.width, 20.0, @"%@ at height %f", theme.name, height);
            if (grid.currentSizeInChars.height != (CGFloat)expected)
                break;
        }
    }
}

// A window split off to hold n columns (glkimp's gli_cells_span plus the
// margins) has n, and a full row of them is drawn inside the text view,
// not clipped at the right edge.
- (void)testAFullRowFitsInTheGrid {
    for (Theme *theme in [self builtInThemes]) {
        GlkTextGridWindow *grid = [self gridInTheme:theme];
        CGFloat height = gli_cells_span(2, theme.cellHeight) + 2 * theme.gridMarginY;
        for (NSUInteger n = 1; n <= 100; n++) {
            CGFloat width = gli_cells_span((unsigned)n, theme.cellWidth) + 2 * (theme.gridMarginX + 5);
            grid.frame = NSMakeRect(0, 0, width, height);
            [grid flushDisplay];
            XCTAssertEqual(grid.currentSizeInChars.width, (CGFloat)n, @"%@ at %lu columns", theme.name, n);
            if (grid.currentSizeInChars.width != (CGFloat)n)
                break;

            [grid moveToColumn:0 row:0];
            [grid putString:[@"" stringByPaddingToLength:n withString:@"W" startingAtIndex:0] style:style_Normal];
            [grid flushDisplay];

            NSTextView *textview = grid.textview;
            NSLayoutManager *layout = textview.layoutManager;
            NSTextContainer *container = textview.textContainer;
            [layout ensureLayoutForTextContainer:container];
            // The advances of the row and the line fragment padding on both
            // sides (a glyph's ink, Monaco's "W" say, can hang over its cell)
            NSRect used = [layout usedRectForTextContainer:container];
            CGFloat right = NSMaxX(used) + textview.textContainerOrigin.x;
            XCTAssertLessThanOrEqual(right, width - theme.gridMarginX + 0.01,
                                     @"%@ at %lu columns", theme.name, n);
            // Still on the first line: the row did not wrap
            NSUInteger lastGlyph = [layout glyphIndexForCharacterAtIndex:n - 1];
            NSRect line = [layout lineFragmentRectForGlyphAtIndex:lastGlyph effectiveRange:NULL];
            XCTAssertEqual(NSMinY(line), 0.0, @"%@ at %lu columns", theme.name, n);
        }
    }
}

#pragma mark - Monaco

// Monaco's metrics have tripped up the grid before (GlkStyle cellSize once
// special-cased its height, or Hugo status bars stayed blank), and it is the
// Spatterlight Classic grid font. These sizes include the fractional ones
// that zooming gives.
- (NSArray<NSNumber *> *)monacoSizes {
    NSMutableArray<NSNumber *> *sizes = [NSMutableArray array];
    for (CGFloat size = 8; size <= 24; size += 0.5)
        [sizes addObject:@(size)];
    [sizes addObjectsFromArray:@[ @(9.6), @(10.8), @(13.2), @(14.4), @(15.6) ]];
    return sizes;
}

// self.theme with a Monaco grid font, the way the Classic theme sets it up:
// zero baseline offset, and a maximum line height of the point size (as
// Classic has) or none.
- (Theme *)monacoThemeOfSize:(CGFloat)size cappedLineHeight:(BOOL)capped {
    Theme *theme = self.theme;
    NSFont *monaco = [NSFont fontWithName:@"Monaco" size:size];
    XCTAssertNotNil(monaco);
    theme.gridNormal.font = monaco;
    [theme.gridNormal updateAttributes:^(NSMutableDictionary *attributes) {
        attributes[NSBaselineOffsetAttributeName] = @(0);
    }];
    [theme.gridNormal updateParagraphStyle:^(NSMutableParagraphStyle *para) {
        para.maximumLineHeight = capped ? size : 0;
    }];
    [theme populateStyles];
    NSSize cell = theme.gridNormal.cellSize;
    theme.cellWidth = cell.width;
    theme.cellHeight = cell.height;
    XCTAssertEqualObjects(theme.gridNormal.font.fontName, @"Monaco");
    XCTAssertGreaterThan(theme.cellWidth, 0);
    XCTAssertGreaterThan(theme.cellHeight, 0);
    return theme;
}

- (void)testMonacoGridColumnsAndRowsAreTheOnesGlkCounts {
    for (NSNumber *size in [self monacoSizes]) {
        for (int capped = 0; capped <= 1; capped++) {
            Theme *theme = [self monacoThemeOfSize:size.doubleValue cappedLineHeight:capped];
            GlkTextGridWindow *grid = [self gridInTheme:theme];
            NSString *what = [NSString stringWithFormat:@"Monaco %@%@ (cell %f x %f)", size,
                              capped ? @" capped" : @"", theme.cellWidth, theme.cellHeight];

            CGFloat height = gli_cells_span(3, theme.cellHeight) + 2 * theme.gridMarginY;
            CGFloat minimum = 2 * (theme.gridMarginX + 5) + theme.cellWidth;
            for (CGFloat width = ceil(minimum); width <= 700; width += 1) {
                grid.frame = NSMakeRect(0, 0, width, height);
                [grid flushDisplay];
                NSUInteger expected = [self glkColumnsInWidth:width theme:theme];
                XCTAssertEqual(grid.currentSizeInChars.width, (CGFloat)expected, @"%@ at width %f", what, width);
                XCTAssertEqual(grid.currentSizeInChars.height, 3.0, @"%@ at width %f", what, width);
                if (grid.currentSizeInChars.width != (CGFloat)expected)
                    break;
            }

            CGFloat width = gli_cells_span(20, theme.cellWidth) + 2 * (theme.gridMarginX + 5);
            minimum = 2 * theme.gridMarginY + theme.cellHeight;
            for (CGFloat h = ceil(minimum); h <= 400; h += 1) {
                grid.frame = NSMakeRect(0, 0, width, h);
                [grid flushDisplay];
                NSUInteger expected = gli_cells_in(h - 2 * theme.gridMarginY, theme.cellHeight);
                XCTAssertEqual(grid.currentSizeInChars.height, (CGFloat)expected, @"%@ at height %f", what, h);
                XCTAssertEqual(grid.currentSizeInChars.width, 20.0, @"%@ at height %f", what, h);
                if (grid.currentSizeInChars.height != (CGFloat)expected)
                    break;
            }
        }
    }
}

// A grid split off for n columns and one row per Glk style, every row full
// of "W" in its style: the rows lie a cell apart, none wraps, and the text
// stays inside the margins. Bold or oblique Monaco is synthesized, so the
// styles must keep the plain advance.
- (void)testMonacoFullGridInEveryStyleFitsItsCells {
    for (NSNumber *size in [self monacoSizes]) {
        for (int capped = 0; capped <= 1; capped++) {
            Theme *theme = [self monacoThemeOfSize:size.doubleValue cappedLineHeight:capped];
            GlkTextGridWindow *grid = [self gridInTheme:theme];
            NSString *what = [NSString stringWithFormat:@"Monaco %@%@ (cell %f x %f)", size,
                              capped ? @" capped" : @"", theme.cellWidth, theme.cellHeight];
            const NSUInteger rows = style_NUMSTYLES;

            for (NSUInteger n = 1; n <= 81; n += 4) {
                CGFloat width = gli_cells_span((unsigned)n, theme.cellWidth) + 2 * (theme.gridMarginX + 5);
                CGFloat height = gli_cells_span((unsigned)rows, theme.cellHeight) + 2 * theme.gridMarginY;
                grid.frame = NSMakeRect(0, 0, width, height);
                [grid flushDisplay];
                XCTAssertEqual(grid.currentSizeInChars.width, (CGFloat)n, @"%@ at %lu columns", what, n);
                XCTAssertEqual(grid.currentSizeInChars.height, (CGFloat)rows, @"%@ at %lu columns", what, n);
                if (grid.currentSizeInChars.width != (CGFloat)n || grid.currentSizeInChars.height != (CGFloat)rows)
                    break;

                NSString *row = [@"" stringByPaddingToLength:n withString:@"W" startingAtIndex:0];
                for (NSUInteger r = 0; r < rows; r++) {
                    [grid moveToColumn:0 row:r];
                    [grid putString:row style:(NSUInteger)r];
                }
                [grid flushDisplay];

                NSTextView *textview = grid.textview;
                NSLayoutManager *layout = textview.layoutManager;
                NSTextContainer *container = textview.textContainer;
                [layout ensureLayoutForTextContainer:container];
                NSPoint origin = textview.textContainerOrigin;
                NSRect used = [layout usedRectForTextContainer:container];
                XCTAssertLessThanOrEqual(NSMaxX(used) + origin.x, width - theme.gridMarginX + 0.01,
                                         @"%@ at %lu columns", what, n);
                XCTAssertLessThanOrEqual(NSMaxY(used) + origin.y, height - theme.gridMarginY + 0.01,
                                         @"%@ at %lu columns", what, n);
                for (NSUInteger r = 0; r < rows; r++) {
                    NSUInteger first = [layout glyphIndexForCharacterAtIndex:r * (n + 1)];
                    NSUInteger last = [layout glyphIndexForCharacterAtIndex:r * (n + 1) + n - 1];
                    NSRect line = [layout lineFragmentRectForGlyphAtIndex:first effectiveRange:NULL];
                    NSRect lastLine = [layout lineFragmentRectForGlyphAtIndex:last effectiveRange:NULL];
                    XCTAssertEqualWithAccuracy(NSMinY(line), r * theme.cellHeight, 0.01,
                                               @"%@ at %lu columns: row %lu (style %lu)", what, n, r, r);
                    XCTAssertEqual(NSMinY(lastLine), NSMinY(line),
                                   @"%@ at %lu columns: row %lu (style %lu) wrapped", what, n, r, r);
                    NSRect rowUsed = [layout lineFragmentUsedRectForGlyphAtIndex:first effectiveRange:NULL];
                    XCTAssertLessThanOrEqual(NSMaxX(rowUsed), gli_cells_span((unsigned)n, theme.cellWidth) + 10 + 0.01,
                                             @"%@ at %lu columns: row %lu (style %lu) is wider than its cells", what, n, r, r);
                }
            }
        }
    }
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

// Hugo pads its status line to two or three cells short of the grid (see
// checkForUglyBorder), and the grid may have been cleared while another zcolor
// was current, as it is when Colossal Cave leaves its black-on-white help menu.
// The colour that covers all but those cells still wins over the stale clear
// colour, so no strip in the old colour shows at the end of the status line.
- (void)testColourShortOfTheRightEdgeStillGivesTheBackgroundColour {
    GlkTextGridWindow *grid = [self gridWithColumns:20 rows:2];
    [grid setBgColor:0xffffff];
    [grid setZColorText:0xffffff background:0x00007f];
    for (NSUInteger row = 0; row < 2; row++) {
        [grid moveToColumn:0 row:row];
        [grid putString:[self row:@"At End Of Road" width:17] style:style_Normal];
    }
    [grid flushDisplay];
    NSColor *blue = [grid.bufferTextStorage attribute:NSBackgroundColorAttributeName atIndex:0 effectiveRange:NULL];
    XCTAssertEqualObjects(blue, [NSColor colorFromInteger:0x00007f]);
    XCTAssertEqualObjects(grid.textview.backgroundColor, blue);
}

// A colour that stops further short than that is not the grid's background.
- (void)testColourCoveringHalfARowDoesNotGiveTheBackgroundColour {
    GlkTextGridWindow *grid = [self gridWithColumns:20 rows:1];
    [grid setBgColor:0xffffff];
    [grid setZColorText:0xffffff background:0x00007f];
    [grid moveToColumn:0 row:0];
    [grid putString:[self row:@"Status" width:12] style:style_Normal];
    [grid flushDisplay];
    XCTAssertEqualObjects(grid.textview.backgroundColor, [NSColor colorFromInteger:0xffffff]);
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
