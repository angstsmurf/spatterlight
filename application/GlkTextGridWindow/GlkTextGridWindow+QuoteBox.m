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

@implementation GlkTextGridWindow (QuoteBox)

#pragma mark Quote box

- (void)quotebox:(NSUInteger)linesToSkip {
    NSUInteger charactersToSkip = (linesToSkip + 1) * (cols + 1);
    NSUInteger width = 0;
    NSUInteger height = 0;
    NSAttributedString *blockTextStorage = _bufferTextStorage;
    if (blockTextStorage.length < charactersToSkip)
        blockTextStorage = textstorage;
    if (blockTextStorage.length < charactersToSkip)
        return;

    NSUInteger rowStride = cols + 1;

    // The "normal" (transparent) background is whatever the left margin uses.
    // Any cell whose background differs from it belongs to a quote box. We work
    // row by row rather than by background-colour runs so that two side-by-side
    // boxes — as Trinity draws for the Clarke/Lebling "magic/technology" pair
    // (PLTABLE 22 at left margins 9 and 49) — collapse into a single wide box
    // with a transparent gap in the middle, instead of being mistaken for two
    // stacked rows.
    id normalBg = [blockTextStorage attribute:NSBackgroundColorAttributeName
                                      atIndex:charactersToSkip
                               effectiveRange:NULL];

    BOOL (^isBoxCell)(NSUInteger) = ^BOOL(NSUInteger idx) {
        id bg = [blockTextStorage attribute:NSBackgroundColorAttributeName
                                    atIndex:idx
                             effectiveRange:NULL];
        if (!bg)
            return (normalBg != nil);
        return ![bg isEqual:normalBg];
    };

    // First pass: find the horizontal extent of the box cells across every row,
    // so all rows stay left-aligned to the same column and any inter-box gap is
    // included in the span.
    NSUInteger globalFirst = NSUIntegerMax;
    NSUInteger globalLast = 0;
    for (NSUInteger rowStart = charactersToSkip;
         rowStart + cols <= blockTextStorage.length;
         rowStart += rowStride) {
        for (NSUInteger col = 0; col < cols; col++) {
            if (isBoxCell(rowStart + col)) {
                if (col < globalFirst)
                    globalFirst = col;
                if (col > globalLast)
                    globalLast = col;
            }
        }
    }

    if (globalFirst == NSUIntegerMax)
        return; // No quote box present.

    width = globalLast - globalFirst + 1;

    NSMutableAttributedString *quoteAttStr = [[NSMutableAttributedString alloc] init];
    NSAttributedString *newline = [[NSAttributedString alloc] initWithString:@"\n"];

    // Second pass: emit every row that contains box cells, copying the full
    // span (including any transparent gap between two boxes) with its original
    // attributes, so the inverse-video backgrounds and the transparent gap are
    // both preserved.
    for (NSUInteger rowStart = charactersToSkip;
         rowStart + cols <= blockTextStorage.length;
         rowStart += rowStride) {
        BOOL hasBox = NO;
        for (NSUInteger col = globalFirst; col <= globalLast; col++) {
            if (isBoxCell(rowStart + col)) {
                hasBox = YES;
                break;
            }
        }
        if (!hasBox)
            continue;
        NSRange r = NSMakeRange(rowStart + globalFirst, width);
        [quoteAttStr appendAttributedString:[blockTextStorage attributedSubstringFromRange:r]];
        [quoteAttStr appendAttributedString:newline];
        height++;
    }

    GlkController *glkctl = self.glkctl;

    GlkTextBufferWindow *lowerView;

    for (GlkWindow *win in glkctl.gwindows.allValues) {
        if ([win isKindOfClass:[GlkTextBufferWindow class]])
            lowerView = (GlkTextBufferWindow *)win;
    }

    NSTextView *superView = lowerView.textview;

    if (glkctl.theme.zMachineNoErrWin) {
        [lowerView putString:quoteAttStr.string style:style_Preformatted];
    }

    if (glkctl.theme.quoteBox) {
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
            [previous mergeSideBySideQuoteBoxAtColumn:globalFirst
                                                width:width
                                               height:height
                                              content:quoteAttStr
                                        gapAttributes:gapAttrs];
            glkctl.numberOfPrintsAndClears = 0;
            return;
        }

        GlkTextGridWindow *box = [[GlkTextGridWindow alloc] initWithGlkController:glkctl name:-1];
        box.quoteboxSize = NSMakeSize((CGFloat)width, (CGFloat)height);
        box.quoteboxColumn = globalFirst;
        [box makeTransparent];

        [box.textview.textStorage setAttributedString:quoteAttStr];

        box.alphaValue = 0;

        [glkctl.quoteBoxes addObject:box];
        lowerView.quoteBox = box;
        box.quoteboxVerticalOffset = linesToSkip;
        box.quoteboxAddedOnPAC = 0;
        glkctl.numberOfPrintsAndClears = 0;
        box.quoteboxParent = superView.enclosingScrollView;
        [box performSelector:@selector(quoteboxAdjustSize:) withObject:nil afterDelay:0.2];
    }
}

// Lay this (already-pending) quote box and a second same-turn box out side by
// side into one wide box, preserving each box's grid column so the gap between
// them stays transparent. The box already has a quoteboxAdjustSize: scheduled,
// which will pick up the new size and content.
- (void)mergeSideBySideQuoteBoxAtColumn:(NSUInteger)otherColumn
                                  width:(NSUInteger)otherWidth
                                 height:(NSUInteger)otherHeight
                                content:(NSAttributedString *)otherContent
                          gapAttributes:(NSDictionary *)gapAttrs {
    NSUInteger selfColumn = self.quoteboxColumn;
    NSUInteger selfWidth = (NSUInteger)self.quoteboxSize.width;
    NSUInteger selfHeight = (NSUInteger)self.quoteboxSize.height;
    NSAttributedString *selfContent = self.textview.textStorage;

    // Order the two boxes left-to-right.
    NSUInteger leftColumn, leftWidth, leftHeight, rightColumn, rightWidth, rightHeight;
    NSAttributedString *leftContent, *rightContent;
    if (selfColumn <= otherColumn) {
        leftColumn = selfColumn; leftWidth = selfWidth; leftHeight = selfHeight; leftContent = selfContent;
        rightColumn = otherColumn; rightWidth = otherWidth; rightHeight = otherHeight; rightContent = otherContent;
    } else {
        leftColumn = otherColumn; leftWidth = otherWidth; leftHeight = otherHeight; leftContent = otherContent;
        rightColumn = selfColumn; rightWidth = selfWidth; rightHeight = selfHeight; rightContent = selfContent;
    }

    NSArray<NSAttributedString *> *leftRows = [self quoteBoxRowsOfString:leftContent];
    NSArray<NSAttributedString *> *rightRows = [self quoteBoxRowsOfString:rightContent];

    NSUInteger gap = (rightColumn > leftColumn + leftWidth) ? rightColumn - (leftColumn + leftWidth) : 0;
    NSUInteger combinedWidth = (rightColumn + rightWidth) - leftColumn;
    NSUInteger combinedHeight = MAX(leftHeight, rightHeight);

    NSMutableAttributedString *combined = [[NSMutableAttributedString alloc] init];
    NSAttributedString *newline = [[NSAttributedString alloc] initWithString:@"\n" attributes:gapAttrs];

    for (NSUInteger row = 0; row < combinedHeight; row++) {
        NSAttributedString *leftRow = (row < leftRows.count) ? leftRows[row] : nil;
        if (leftRow)
            [combined appendAttributedString:leftRow];
        NSUInteger leftLen = leftRow ? leftRow.length : 0;
        if (leftLen < leftWidth)
            [combined appendAttributedString:[self quoteBoxSpaces:leftWidth - leftLen attributes:gapAttrs]];

        [combined appendAttributedString:[self quoteBoxSpaces:gap attributes:gapAttrs]];

        NSAttributedString *rightRow = (row < rightRows.count) ? rightRows[row] : nil;
        if (rightRow)
            [combined appendAttributedString:rightRow];

        [combined appendAttributedString:newline];
    }

    self.quoteboxColumn = leftColumn;
    self.quoteboxSize = NSMakeSize((CGFloat)combinedWidth, (CGFloat)combinedHeight);
    [self.textview.textStorage setAttributedString:combined];

    // Keep each column's text separate for accessibility. The on-screen storage
    // above interleaves them (every line is leftRow + gap + rightRow), which
    // makes VoiceOver read the two poems alternating line by line. offset/width
    // are in cells from the merged box's left edge (which is leftColumn).
    self.quoteboxColumns = @[
        @{ @"value": [self quoteBoxColumnTextFromRows:leftRows],
           @"offset": @(0),
           @"width": @(leftWidth) },
        @{ @"value": [self quoteBoxColumnTextFromRows:rightRows],
           @"offset": @(rightColumn - leftColumn),
           @"width": @(rightWidth) }
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
