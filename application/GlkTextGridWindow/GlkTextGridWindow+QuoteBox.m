//
//  GlkTextGridWindow+QuoteBox.m
//  Spatterlight
//
//  Infocom quote boxes: the grid window that is lifted out of the status
//  area and floated over the story, and the merging of side-by-side boxes.
//

#import "GlkTextGridWindowPrivate.h"
#import "GlkTextBufferWindow.h"
#import "BufferTextView.h"
#import "GlkController.h"
#import "GridTextView.h"
#import "NSString+Categories.h"
#import "Theme.h"

// A quote box cut out of the grid: the grid column it starts at, its size in
// cells, and its rows, each followed by a newline.
@interface QuoteBoxPart : NSObject
@property NSUInteger column;
@property NSUInteger width;
@property NSUInteger height;
@property NSAttributedString *content;
@end

@implementation QuoteBoxPart
@end

@implementation GlkTextGridWindow (QuoteBox)

#pragma mark Quote box

- (void)quotebox:(NSUInteger)linesToSkip {
    NSUInteger charactersToSkip = (linesToSkip + 1) * (cols + 1);
    NSAttributedString *blockTextStorage = _bufferTextStorage;
    if (blockTextStorage.length < charactersToSkip)
        blockTextStorage = textstorage;
    if (blockTextStorage.length < charactersToSkip)
        return;

    QuoteBoxPart *part = [self quoteBoxPartInStorage:blockTextStorage fromIndex:charactersToSkip];
    if (!part)
        return; // No quote box present.

    GlkController *glkctl = self.glkctl;
    GlkTextBufferWindow *lowerView = self.quoteBoxStoryWindow;

    if (glkctl.theme.zMachineNoErrWin) {
        [lowerView putString:part.content.string style:style_Preformatted];
    }

    if (!glkctl.theme.quoteBox)
        return;

    if (!glkctl.quoteBoxes)
        glkctl.quoteBoxes = [[NSMutableArray alloc] init];

    // A box whose quoteboxAddedOnPAC is still 0 was created during this same
    // turn and not yet committed at the turn boundary. Trinity draws two
    // boxes side by side (the Clarke/Lebling "magic/technology" pair) with
    // two back-to-back SPLIT cycles, producing two quotebox: calls in one
    // request batch. Merge the second into the first as a single wide box
    // with a transparent gap in the middle, so they appear side by side
    // rather than the second replacing the first.
    GlkTextGridWindow *previous = glkctl.quoteBoxes.lastObject;
    if (previous && previous.quoteboxAddedOnPAC == 0) {
        // Transparent template for the gap and any padding cells: the grid's
        // normal style minus its background colour.
        NSMutableDictionary *gapAttrs = [[blockTextStorage attributesAtIndex:charactersToSkip effectiveRange:NULL] mutableCopy];
        [gapAttrs removeObjectForKey:NSBackgroundColorAttributeName];
        [previous mergeSideBySideQuoteBox:part gapAttributes:gapAttrs];
    } else {
        [self showQuoteBox:part linesDown:linesToSkip over:lowerView];
    }
    glkctl.numberOfPrintsAndClears = 0;
}

// Cut the quote box out of the grid rows that start at index start.
//
// The "normal" (transparent) background is whatever the left margin uses.
// Any cell whose background differs from it belongs to a quote box. We work
// row by row rather than by background-colour runs so that two side-by-side
// boxes in the same rows collapse into a single wide box with a transparent
// gap in the middle, instead of being mistaken for two stacked rows.
- (QuoteBoxPart *)quoteBoxPartInStorage:(NSAttributedString *)storage fromIndex:(NSUInteger)start {
    id normalBg = [storage attribute:NSBackgroundColorAttributeName
                             atIndex:start
                      effectiveRange:NULL];

    BOOL (^isBoxCell)(NSUInteger) = ^BOOL(NSUInteger idx) {
        id bg = [storage attribute:NSBackgroundColorAttributeName
                           atIndex:idx
                    effectiveRange:NULL];
        if (!bg)
            return (normalBg != nil);
        return ![bg isEqual:normalBg];
    };

    NSRange span = [self quoteBoxSpanInStorage:storage fromIndex:start isBoxCell:isBoxCell];
    if (span.location == NSNotFound)
        return nil;

    // Emit every row that contains box cells, copying the full span
    // (including any transparent gap between two boxes) with its original
    // attributes, so the inverse-video backgrounds and the transparent gap
    // are both preserved.
    NSMutableAttributedString *content = [[NSMutableAttributedString alloc] init];
    NSAttributedString *newline = [[NSAttributedString alloc] initWithString:@"\n"];
    NSUInteger height = 0;
    for (NSUInteger rowStart = start;
         rowStart + cols <= storage.length;
         rowStart += cols + 1) {
        BOOL hasBox = NO;
        for (NSUInteger col = span.location; col < NSMaxRange(span); col++) {
            if (isBoxCell(rowStart + col)) {
                hasBox = YES;
                break;
            }
        }
        if (!hasBox)
            continue;
        NSRange r = NSMakeRange(rowStart + span.location, span.length);
        [content appendAttributedString:[storage attributedSubstringFromRange:r]];
        [content appendAttributedString:newline];
        height++;
    }

    QuoteBoxPart *part = [[QuoteBoxPart alloc] init];
    part.column = span.location;
    part.width = span.length;
    part.height = height;
    part.content = content;
    return part;
}

// The columns from the leftmost to the rightmost box cell in any row, so all
// rows stay left-aligned to the same column and any gap between two boxes is
// included. NSNotFound if there are no box cells.
- (NSRange)quoteBoxSpanInStorage:(NSAttributedString *)storage
                       fromIndex:(NSUInteger)start
                       isBoxCell:(BOOL (^)(NSUInteger))isBoxCell {
    NSUInteger first = NSUIntegerMax;
    NSUInteger last = 0;
    for (NSUInteger rowStart = start;
         rowStart + cols <= storage.length;
         rowStart += cols + 1) {
        for (NSUInteger col = 0; col < cols; col++) {
            if (isBoxCell(rowStart + col)) {
                if (col < first)
                    first = col;
                if (col > last)
                    last = col;
            }
        }
    }
    if (first == NSUIntegerMax)
        return NSMakeRange(NSNotFound, 0);
    return NSMakeRange(first, last - first + 1);
}

// The story window the box floats over: the last text buffer window.
- (GlkTextBufferWindow *)quoteBoxStoryWindow {
    GlkTextBufferWindow *lowerView = nil;
    for (GlkWindow *win in self.glkctl.gwindows.allValues) {
        if ([win isKindOfClass:[GlkTextBufferWindow class]])
            lowerView = (GlkTextBufferWindow *)win;
    }
    return lowerView;
}

// Make a floating window for the box and fade it in over the story window,
// linesDown rows below the top.
- (void)showQuoteBox:(QuoteBoxPart *)part
           linesDown:(NSUInteger)linesDown
                over:(GlkTextBufferWindow *)lowerView {
    GlkController *glkctl = self.glkctl;
    GlkTextGridWindow *box = [[GlkTextGridWindow alloc] initWithGlkController:glkctl name:-1];
    box.quoteboxSize = NSMakeSize((CGFloat)part.width, (CGFloat)part.height);
    box.quoteboxColumn = part.column;
    [box makeTransparent];

    [box.textview.textStorage setAttributedString:part.content];

    box.alphaValue = 0;

    [glkctl.quoteBoxes addObject:box];
    lowerView.quoteBox = box;
    box.quoteboxVerticalOffset = linesDown;
    box.quoteboxAddedOnPAC = 0;
    box.quoteboxParent = lowerView.textview.enclosingScrollView;
    [box performSelector:@selector(quoteboxAdjustSize:) withObject:nil afterDelay:0.2];
}

// Lay this (already-pending) quote box and a second same-turn box out side by
// side into one wide box, preserving each box's grid column so the gap between
// them stays transparent. The box already has a quoteboxAdjustSize: scheduled,
// which will pick up the new size and content.
- (void)mergeSideBySideQuoteBox:(QuoteBoxPart *)other gapAttributes:(NSDictionary *)gapAttrs {
    QuoteBoxPart *mine = [[QuoteBoxPart alloc] init];
    mine.column = self.quoteboxColumn;
    mine.width = (NSUInteger)self.quoteboxSize.width;
    mine.height = (NSUInteger)self.quoteboxSize.height;
    mine.content = [self.textview.textStorage copy];

    // Order the two boxes left-to-right.
    QuoteBoxPart *left = mine.column <= other.column ? mine : other;
    QuoteBoxPart *right = left == mine ? other : mine;

    NSArray<NSAttributedString *> *leftRows = [self quoteBoxRowsOfString:left.content];
    NSArray<NSAttributedString *> *rightRows = [self quoteBoxRowsOfString:right.content];

    NSUInteger gap = (right.column > left.column + left.width) ? right.column - (left.column + left.width) : 0;
    NSUInteger combinedWidth = (right.column + right.width) - left.column;
    NSUInteger combinedHeight = MAX(left.height, right.height);

    NSMutableAttributedString *combined = [[NSMutableAttributedString alloc] init];
    NSAttributedString *newline = [[NSAttributedString alloc] initWithString:@"\n" attributes:gapAttrs];

    for (NSUInteger row = 0; row < combinedHeight; row++) {
        NSAttributedString *leftRow = (row < leftRows.count) ? leftRows[row] : nil;
        if (leftRow)
            [combined appendAttributedString:leftRow];
        NSUInteger leftLen = leftRow ? leftRow.length : 0;
        if (leftLen < left.width)
            [combined appendAttributedString:[self quoteBoxSpaces:left.width - leftLen attributes:gapAttrs]];

        [combined appendAttributedString:[self quoteBoxSpaces:gap attributes:gapAttrs]];

        NSAttributedString *rightRow = (row < rightRows.count) ? rightRows[row] : nil;
        if (rightRow)
            [combined appendAttributedString:rightRow];

        [combined appendAttributedString:newline];
    }

    self.quoteboxColumn = left.column;
    self.quoteboxSize = NSMakeSize((CGFloat)combinedWidth, (CGFloat)combinedHeight);
    [self.textview.textStorage setAttributedString:combined];

    // Keep each column's text separate for accessibility. The on-screen storage
    // above interleaves them (every line is leftRow + gap + rightRow), which
    // makes VoiceOver read the two poems alternating line by line. offset/width
    // are in cells from the merged box's left edge (which is left.column).
    self.quoteboxColumns = @[
        @{ @"value": [self quoteBoxColumnTextFromRows:leftRows],
           @"offset": @(0),
           @"width": @(left.width) },
        @{ @"value": [self quoteBoxColumnTextFromRows:rightRows],
           @"offset": @(right.column - left.column),
           @"width": @(right.width) }
    ];
    self.quoteboxAXChildren = nil;
}

// Join a column's rows into one plain string, trimming trailing whitespace from
// each line (the rows may carry padding/centring spaces that VoiceOver would
// otherwise read out).
- (NSString *)quoteBoxColumnTextFromRows:(NSArray<NSAttributedString *> *)rows {
    NSCharacterSet *whitespace = [NSCharacterSet whitespaceCharacterSet];
    NSMutableArray<NSString *> *lines = [[NSMutableArray alloc] init];
    for (NSAttributedString *row in rows) {
        NSString *line = row.string;
        NSUInteger end = line.length;
        while (end > 0 && [whitespace characterIsMember:[line characterAtIndex:end - 1]])
            end--;
        [lines addObject:[line substringToIndex:end]];
    }
    return [lines componentsJoinedByString:@"\n"];
}

- (NSString *)deinterleavedQuoteboxString {
    NSMutableArray<NSString *> *parts = [[NSMutableArray alloc] init];
    for (NSDictionary *col in _quoteboxColumns) {
        NSString *value = col[@"value"];
        if (value.length)
            [parts addObject:value];
    }
    return [parts componentsJoinedByString:@"\n\n"];
}

- (NSString *)speakableString {
    return _quoteboxColumns.count ? self.deinterleavedQuoteboxString : textstorage.string;
}

// Split a quote box's attributed string (rows separated by newlines) into one
// attributed string per row, dropping the newlines.
- (NSArray<NSAttributedString *> *)quoteBoxRowsOfString:(NSAttributedString *)string {
    NSMutableArray<NSAttributedString *> *rows = [[NSMutableArray alloc] init];
    NSString *str = string.string;
    NSUInteger length = str.length;
    NSUInteger start = 0;
    for (NSUInteger i = 0; i < length; i++) {
        if ([str characterAtIndex:i] == '\n') {
            [rows addObject:[string attributedSubstringFromRange:NSMakeRange(start, i - start)]];
            start = i + 1;
        }
    }
    if (start < length)
        [rows addObject:[string attributedSubstringFromRange:NSMakeRange(start, length - start)]];
    return rows;
}

- (NSAttributedString *)quoteBoxSpaces:(NSUInteger)count attributes:(NSDictionary *)attributes {
    NSString *spaces = [NSString spacesOfLength:count];
    return [[NSAttributedString alloc] initWithString:spaces attributes:attributes];
}

- (void)quoteboxAdjustSize:(id)sender {

    GlkController *glkctl = self.glkctl;
    NSScrollView *quoteboxParent = _quoteboxParent;

    if (quoteboxParent == nil) {
        return;
    }
    NSTextView *textView = quoteboxParent.documentView;
    GlkTextBufferWindow *bufWin = (GlkTextBufferWindow *)textView.delegate;
    NSSize boxSize = NSMakeSize(ceil(self.theme.gridMarginX * 2 + (_quoteboxSize.width + 1) * self.theme.cellWidth), ceil(self.theme.gridMarginY * 2 + _quoteboxSize.height * self.theme.cellHeight));

    // The buffer window defers its frame changes until it is flushed, and
    // nothing is flushed while we are autorestoring. Centre the box in the
    // frame the window is about to get, or the box shows up hanging off the
    // left edge of a window that is still zero points wide.
    CGFloat bufWinWidth = bufWin.framePending ? NSWidth(bufWin.pendingFrame) : NSWidth(bufWin.frame);

    NSRect frame = self.frame;
    frame.size = boxSize;
    frame.origin.x = ceil((bufWinWidth - boxSize.width) / 2);
    // When the grid and buffer cells are the same width, the box goes two
    // cells left of centre, except in Trinity.
    if (glkctl.gameID != kGameIsTrinity && self.theme.cellWidth == self.theme.bufferCellWidth)
        frame.origin.x -= 2 * self.theme.cellWidth;

    NSUInteger rowsDown = _quoteboxVerticalOffset;
    // Curses has a two-row status line.
    if (glkctl.gameID == kGameIsCurses)
        rowsDown += 2;
    frame.origin.y = ceil(quoteboxParent.contentView.frame.origin.y + rowsDown * self.theme.cellHeight);

    // Push down buffer window text with newlines if the quote box covers text the player has not read yet.
    if (bufWin.moveRanges.count < 2 && (!bufWin.moveRanges || NSMaxRange(bufWin.moveRanges.lastObject.rangeValue) >= bufWin.textview.string.length)) {
        NSString *word = bufWin.textview.textStorage.words.firstObject.string;
        if (word) {
            NSRange range = [bufWin.textview.textStorage.string rangeOfString:word];
            NSLayoutManager *layoutManager = bufWin.textview.layoutManager;
            NSRect rect = [layoutManager boundingRectForGlyphRange:range inTextContainer:bufWin.textview.textContainer];
            CGFloat diff = rect.origin.y - NSMaxY(frame);
            if (diff < 0) {
                diff = -diff;
                NSUInteger newlines = (NSUInteger)ceil(diff / self.theme.bufferCellHeight) + 1;
                [bufWin padWithNewlines:newlines];
            }
        }
    }
    self.frame = frame;
    // setFrame: has just laid out a blank grid in the buffer. Throw that away
    // and have flushDisplay work on the text quotebox: gave us instead, so
    // that the blank rows of the box are kept from collapsing.
    _bufferTextStorage = [textstorage mutableCopy];
    dirty = YES;
    [self flushDisplay];

    [quoteboxParent addFloatingSubview:self forAxis:NSEventGestureAxisVertical];

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = 0.5;
        self.animator.alphaValue = 1;
    } completionHandler:^{
        self.alphaValue = 1;
    }];
}

@end
