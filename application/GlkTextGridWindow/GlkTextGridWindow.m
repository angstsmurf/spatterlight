//
//  GlkTextGridWindow.m
//  Spatterlight
//
//  The text grid window: the cell buffer, printing and cursor movement,
//  resizing, and saving and restoring. The rest is in the categories.
//

#import "GlkTextGridWindowPrivate.h"
#import "GlkController.h"
#import "GlkController+Speech.h"
#import "Preferences.h"

#import "InputTextField.h"
#import "InputHistory.h"
#import "GridTextView.h"

#import "NSString+Categories.h"
#import "Theme.h"
#import "GlkStyle.h"
#import "ZColor.h"
#import "BureaucracyForm.h"

#import "NSColor+integer.h"

// Large enough that the text container and view never wrap or clip a row.
static const CGFloat kUnlimitedTextSize = 10000000;

@implementation GlkTextGridWindow

@synthesize bufferTextStorage = _bufferTextStorage;
// GlkWindow implements -textview (nil), so it is not auto-synthesized here.
@synthesize textview = _textview;

+ (BOOL) supportsSecureCoding {
    return YES;
}

// Anyone handing us a new buffer from outside (autorestore, the tests)
// gets the next flush to take it on board.
- (void)setBufferTextStorage:(NSMutableAttributedString *)bufferTextStorage {
    _bufferTextStorage = bufferTextStorage;
    dirty = YES;
}

+ (BOOL)isCompatibleWithResponsiveScrolling
{
    return YES;
}

- (instancetype)initWithGlkController:(GlkController *)glkctl_
                                 name:(NSInteger)name_ {
    self = [super initWithGlkController:glkctl_ name:name_];

    if (self) {

        self.styleHints = [GlkWindow deepCopyOfStyleHintsArray:glkctl_.gridStyleHints];

        styles = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
        for (NSUInteger i = 0; i < style_NUMSTYLES; i++) {
            NSDictionary *styleDict = [self themeAttributesForStyle:i];
            if (!styleDict) {
                NSLog(@"GlkTextGridWindow couldn't create style dict for style %ld", i);
                [styles addObject:[NSNull null]];
            } else {
                [styles addObject:styleDict];
            }
        }

        /* construct text system manually */

        scrollview = [[NSScrollView alloc] initWithFrame:NSZeroRect];
        scrollview.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        scrollview.hasHorizontalScroller = NO;
        scrollview.hasVerticalScroller = NO;
        scrollview.horizontalScrollElasticity = NSScrollElasticityNone;
        scrollview.verticalScrollElasticity = NSScrollElasticityNone;
        scrollview.borderType = NSNoBorder;
        scrollview.drawsBackground = NO;
        scrollview.accessibilityElement = NO;

        textstorage = [[NSTextStorage alloc] init];
        _bufferTextStorage = [[NSMutableAttributedString alloc] init];
        dirty = YES;

        layoutmanager = [[NSLayoutManager alloc] init];
        layoutmanager.backgroundLayoutEnabled = YES;

        [textstorage addLayoutManager:layoutmanager];

        container = [[NSTextContainer alloc]
                     initWithContainerSize:NSMakeSize(kUnlimitedTextSize, kUnlimitedTextSize)];

        container.layoutManager = layoutmanager;
        [layoutmanager addTextContainer:container];

        _textview = [[GridTextView alloc] initWithFrame:NSMakeRect(0, 0, 0, 0)
                                           textContainer:container];

        _textview.minSize = NSMakeSize(0, 0);
        _textview.maxSize = NSMakeSize(kUnlimitedTextSize, kUnlimitedTextSize);

        _textview.autoresizingMask = NSViewWidthSizable;

        container.textView = _textview;
        scrollview.documentView = _textview;

        /* now configure the text stuff */

        _textview.delegate = self;
        textstorage.delegate = self;
        _textview.textContainerInset =
            NSMakeSize(self.theme.gridMarginX, self.theme.gridMarginY);
        _textview.backgroundColor = self.theme.gridBackground;

        [self updateLinkTextAttributesOf:_textview];

        _textview.editable = NO;
        _textview.usesFontPanel = NO;
        _restoredSelection = NSMakeRange(0, 0);

        history = [[InputHistory alloc] init];

        [self addSubview:scrollview];
        [self recalcBackground];

        if (self.glkctl.usesFont3)
            [self createBeyondZorkStyle];

        underlineLinks = (self.themeLinkStyle != NSUnderlineStyleNone);
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        _textview = [decoder decodeObjectOfClass:[GridTextView class] forKey:@"_textview"];

        layoutmanager = _textview.layoutManager;
        textstorage = _textview.textStorage;
        if (!textstorage)
            NSLog(@"Error! textstorage is nil!");
        _bufferTextStorage = [textstorage mutableCopy];
        _textview.delegate = self;
        _textview.insertionPointColor = self.theme.gridBackground;
        textstorage.delegate = self;
        scrollview = _textview.enclosingScrollView;
        scrollview.documentView = _textview;
        scrollview.accessibilityElement = NO;
        container = _textview.textContainer;

        rows = (NSUInteger)[decoder decodeIntegerForKey:@"rows"];
        cols = (NSUInteger)[decoder decodeIntegerForKey:@"cols"];
        xpos = (NSUInteger)[decoder decodeIntegerForKey:@"xpos"];
        ypos = (NSUInteger)[decoder decodeIntegerForKey:@"ypos"];

        _selectedRow = (NSUInteger)[decoder decodeIntegerForKey:@"selectedRow"];
        _selectedCol = (NSUInteger)[decoder decodeIntegerForKey:@"selectedCol"];
        _selectedString = [decoder decodeObjectOfClass:[NSString class] forKey:@"selectedString"];

        transparent = [decoder decodeBoolForKey:@"transparent"];

        _restoredSelection =
        ((NSValue *)[decoder decodeObjectOfClass:[NSValue class] forKey:@"selectedRange"])
        .rangeValue;

        _pendingBackgroundCol = [decoder decodeObjectOfClass:[NSColor class] forKey:@"pendingBackgroundCol"];
        _bufferTextStorage = [decoder decodeObjectOfClass:[NSMutableAttributedString class] forKey:@"bufferTextStorage"];
        dirty = YES;

        _enteredTextSoFar = [decoder decodeObjectOfClass:[NSString class] forKey:@"inputString"];
        maxInputLength = (NSUInteger)[decoder decodeIntForKey:@"maxInputLength"];

        _quoteboxSize = ((NSValue *)[decoder decodeObjectOfClass:[NSValue class] forKey:@"quoteboxSize"]).sizeValue;
        _quoteboxAddedOnPAC = [decoder decodeIntegerForKey:@"quoteboxAddedOnTurn"];
        _quoteboxVerticalOffset = (NSUInteger)[decoder decodeIntegerForKey:@"quoteboxVerticalOffset"];
        _quoteboxColumn = (NSUInteger)[decoder decodeIntegerForKey:@"quoteboxColumn"];
        // De-interleaved columns of a merged side-by-side quote box, so a
        // restored box still reads as two separate blocks in VoiceOver rather
        // than interleaving the two columns line by line.
        NSSet *columnClasses = [NSSet setWithObjects:[NSArray class], [NSDictionary class], [NSString class], [NSNumber class], nil];
        _quoteboxColumns = [decoder decodeObjectOfClasses:columnClasses forKey:@"quoteboxColumns"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeObject:_textview forKey:@"_textview"];
    [encoder encodeInteger:(NSInteger)rows forKey:@"rows"];
    [encoder encodeInteger:(NSInteger)cols forKey:@"cols"];
    [encoder encodeInteger:(NSInteger)xpos forKey:@"xpos"];
    [encoder encodeInteger:(NSInteger)ypos forKey:@"ypos"];
    [encoder encodeBool:transparent forKey:@"transparent"];
    NSValue *rangeVal = [NSValue valueWithRange:_textview.selectedRange];
    [encoder encodeObject:rangeVal forKey:@"selectedRange"];

    [encoder encodeInteger:(NSInteger)[self rowOfIndex:_textview.selectedRange.location] forKey:@"selectedRow"];
    [encoder encodeInteger:(NSInteger)[self columnOfIndex:_textview.selectedRange.location] forKey:@"selectedCol"];
    [encoder encodeObject:[textstorage.string substringWithRange:_textview.selectedRange] forKey:@"selectedString"];

    MyFieldEditor *fieldEditor = self.input.fieldEditor;
    if (fieldEditor && fieldEditor.textStorage.string.length) {
        [encoder encodeObject:fieldEditor.textStorage.string forKey:@"inputString"];
    } else {
        [encoder encodeObject:_enteredTextSoFar forKey:@"inputString"];
    }
    [encoder encodeInteger:(NSInteger)maxInputLength forKey:@"maxInputLength"];
    [encoder encodeObject:_pendingBackgroundCol forKey:@"pendingBackgroundCol"];
    [encoder encodeObject: _bufferTextStorage forKey:@"bufferTextStorage"];

    [encoder encodeObject:@(_quoteboxSize) forKey:@"quoteboxSize"];
    [encoder encodeInteger:_quoteboxAddedOnPAC forKey:@"quoteboxAddedOnTurn"];
    [encoder encodeInteger:(NSInteger)_quoteboxVerticalOffset forKey:@"quoteboxVerticalOffset"];
    [encoder encodeInteger:(NSInteger)_quoteboxColumn forKey:@"quoteboxColumn"];
    [encoder encodeObject:_quoteboxColumns forKey:@"quoteboxColumns"];
}

- (BOOL)wantsFocus {
    return char_request || line_request;
}

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)isOpaque {
    return !transparent;
}

- (void)makeTransparent {
    transparent = YES;
    [self recalcBackground];
    dirty = YES;
}

- (void)postRestoreAdjustments:(GlkWindow *)win {

    GlkTextGridWindow *restoredWin = (GlkTextGridWindow *)win;
    line_request = [restoredWin hasLineRequest];

    if (restoredWin.bufferTextStorage)
        _bufferTextStorage = restoredWin.bufferTextStorage;
    dirty = YES;

    [self flushDisplay];

    _restoredSelection = restoredWin.restoredSelection;
    _selectedRow = restoredWin.selectedRow;
    _selectedCol = restoredWin.selectedCol;
    _selectedString = restoredWin.selectedString;

    if (NSMaxRange(_restoredSelection) <= textstorage.length && [[textstorage.string substringWithRange:_restoredSelection] isEqualToString:_selectedString]) {
        _textview.selectedRange = _restoredSelection;
    } else {
        _restoredSelection = [self selectionAtRememberedCell];
        if (NSMaxRange(_restoredSelection) <= textstorage.length && [[textstorage.string substringWithRange:_restoredSelection] isEqualToString:_selectedString]) {
            _textview.selectedRange = _restoredSelection;
        }
    }

    _enteredTextSoFar = restoredWin.enteredTextSoFar;

    if (line_request)
        [self scheduleDeferredInitLine:_enteredTextSoFar delay:0.5];
}

#pragma mark Display

- (void)applyPendingFrame {
    if (!self.framePending)
        return;
    if (!NSEqualRects(self.frame, self.pendingFrame)) {
        super.frame = self.pendingFrame;
    }
    if (!NSEqualRects(_textview.frame, self.bounds)) {
        _textview.frame = self.bounds;
    }
    self.framePending = NO;
}

- (void)flushDisplay {
    // Nothing printed, resized, restyled or recoloured since the last flush:
    // the text storage already shows the buffer, so skip the background scan,
    // the blank-row fix and the replacement that would re-lay-out and repaint
    // the whole grid. A pending frame is still applied, as setFrame: leaves
    // one behind without touching the buffer during live resize and for
    // frames that would not fit the screen, and the selection is still
    // recorded, as setFrame: reads it.
    if (!dirty && _bufferTextStorage) {
        [self applyPendingFrame];
        if (_pendingBackgroundCol) {
            _textview.backgroundColor = _pendingBackgroundCol;
        }
        _pendingBackgroundCol = nil;
        _restoredSelection = _textview.selectedRange;
        return;
    }
    dirty = NO;

    // Making the textview temporarily editable
    // reduces flicker in Hugo Tetris, for some reason.
    _textview.editable = YES;
    NSRange selectedRange = _textview.selectedRange;
    NSString *selectedString = [textstorage.string substringWithRange:selectedRange];
    [self rememberSelectionCell:selectedRange.location];

    [self applyPendingFrame];

    if (!transparent)
        [self checkForUglyBorder];

    [self fixCollapsingSpaceRows];

    if (_pendingBackgroundCol) {
        _textview.backgroundColor = _pendingBackgroundCol;
    }
    _pendingBackgroundCol = nil;

    if (_bufferTextStorage) {
        NSRange selection = _textview.selectedRange;
        [textstorage setAttributedString:_bufferTextStorage];
        if (NSMaxRange(selection) <= _bufferTextStorage.length)
            _textview.selectedRange = selection;
    } else {
        _bufferTextStorage = [textstorage mutableCopy];
    }

    _restoredSelection = [self selectionAtRememberedCell];
    if (NSMaxRange(_restoredSelection) <= _bufferTextStorage.length) {
        NSString *newSelectedString = [_bufferTextStorage.string substringWithRange:_restoredSelection];
        if ([newSelectedString isEqualToString:selectedString]) {
            _textview.selectedRange = _restoredSelection;
        }
    }
    _restoredSelection = _textview.selectedRange;
    _textview.editable = NO;
}

#pragma mark Printing, moving, resizing

- (void)setFrame:(NSRect)frame {
    GlkController *glkctl = self.glkctl;

    if (glkctl.ignoreResizes) {
        return;
    }

    self.framePending = YES;
    self.pendingFrame = frame;

    if (frame.size.height == 0) {
        return;
    }

    NSSize screensize = NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX);
    if (glkctl) {
        screensize = glkctl.window.screen.visibleFrame.size;
        if (frame.size.height > screensize.height)
            frame.size.height = glkctl.gameView.frame.size.height;
    }

    _textview.textContainerInset =
    NSMakeSize(self.theme.gridMarginX, self.theme.gridMarginY);

    if (self.theme.cellWidth == 0 || self.theme.cellHeight == 0) {
        return;
    }

    CGFloat margins = (_textview.textContainerInset.width + container.lineFragmentPadding) * 2;
    if (margins > frame.size.width)
        margins = 0;
    NSUInteger newcols = (NSUInteger)round((frame.size.width - margins) /
                                           self.theme.cellWidth);

    CGFloat containerInsetHeight = _textview.textContainerInset.height * 2;
    if (containerInsetHeight > frame.size.height)
        containerInsetHeight = 0;
    NSUInteger newrows = (NSUInteger)round((frame.size.height
                                            - containerInsetHeight) /
                                           self.theme.cellHeight);

    if (newrows == 0 && frame.size.height > 0)
        newrows = 1;
    if (newcols == 0 && frame.size.width > 0)
        newcols = 1;

    // Because our quote box hack assumes that the status line is 1 row
    // and Curses status line has 2 rows, we need a Curses-specific hack
    // to prevent the lower line from being cut off.
    if (newrows == 1 && glkctl.gameID == kGameIsCurses && glkctl.quoteBoxes.count && glkctl.turns > 0) {
        newrows = 2;
        frame.size.height += self.theme.cellHeight;
        self.pendingFrame = frame;
    }

    // Don't cut off lines while zooming
    if ([Preferences instance].inMagnification && newrows < rows)
        return;

    if (_restoredSelection.length == 0)
        _restoredSelection = _textview.selectedRange;

    if ((NSInteger)newcols > 0 && (NSInteger)newrows > 0) {

        if (self.inLiveResize && newcols < cols) {
            return;
        }

        [self rememberSelectionCell:_restoredSelection.location];

        if ((CGFloat)newcols * self.theme.cellWidth > screensize.width || (CGFloat)newrows * self.theme.cellHeight > screensize.height) {
            return;
        }

        NSMutableDictionary *attrDict = [styles[style_Normal] mutableCopy];
        dirty = YES;

        if (cols == 0 || rows == 0)
            _bufferTextStorage = nil;

        if (!_bufferTextStorage || !_bufferTextStorage.length) {
            NSString *spaces = [NSString spacesOfLength:(NSUInteger)(rows * (cols + 1) - (cols > 1))];
            _bufferTextStorage = [[NSTextStorage alloc]
                                  initWithString:spaces
                                  attributes:attrDict];
        }

        if (newcols < cols)
            [self narrowBufferTo:newcols];
        else if (newcols > cols)
            [self widenBufferTo:newcols attributes:attrDict];
        cols = newcols;
        rows = newrows;
        [self fitBufferToRowsWithAttributes:attrDict];

        _restoredSelection = [self selectionAtRememberedCell];

    }

    if (self.inLiveResize) {
        super.frame = frame;
        self.framePending = NO;
    }
}

// Delete characters if the window has become narrower: the last
// cols - newcols cells of each row, which end at the row's newline
// (or, on the last row, at the end of the buffer).
- (void)narrowBufferTo:(NSUInteger)newcols {
    for (NSUInteger r = cols; r <= _bufferTextStorage.length; r += cols + 1) {
        [_bufferTextStorage deleteCharactersInRange:NSMakeRange(r - (cols - newcols), cols - newcols)];
        r -= (cols - newcols);
    }
}

// Pad each row with spaces if the window has become wider.
- (void)widenBufferTo:(NSUInteger)newcols attributes:(NSDictionary *)attrDict {
    NSString *spaces = [NSString spacesOfLength:newcols - cols];

    for (NSUInteger r = cols; r < _bufferTextStorage.length - 1; r += (cols + 1)) {

        NSMutableDictionary *newDict = [attrDict mutableCopy];

        // Check for the background color attribute of the line.
        // If we find one, we paint over the very last characters with it
        if (r > 2) {
            NSDictionary *bgDict = [self findRowColorOfString:_bufferTextStorage index: r - cols rowLength:cols];
            if (bgDict) {
                [newDict addEntriesFromDictionary:bgDict];
            }
        }

        NSAttributedString *string = [[NSAttributedString alloc]
                                      initWithString:spaces
                                      attributes:newDict];
        [_bufferTextStorage insertAttributedString:string atIndex:r];

        r += (newcols - cols);
    }
}

// Pad or cut the buffer to rows * cols cells, then put a newline at the end
// of every row but the last.
- (void)fitBufferToRowsWithAttributes:(NSDictionary *)attrDict {
    NSUInteger desiredLength =
    rows * (cols + 1) - 1; // -1 because we don't want a newline at the very end
    if (desiredLength < 1 || rows == 1)
        desiredLength = cols;
    if (_bufferTextStorage.length < desiredLength) {
        NSDictionary *bgDict = nil;
        if ((cols + 1) * (rows - 1) < _bufferTextStorage.length)
            bgDict = [self findRowColorOfString:_bufferTextStorage index:(cols + 1) * (rows - 1) rowLength:cols];
        NSString *spaces = [NSString spacesOfLength:desiredLength - _bufferTextStorage.length];
        NSAttributedString *string = [[NSAttributedString alloc]
                                      initWithString:spaces
                                      attributes:attrDict];
        [_bufferTextStorage appendAttributedString:string];

        if (bgDict)
            [_bufferTextStorage addAttributes:bgDict range:NSMakeRange(_bufferTextStorage.length-cols, cols)];

    } else if (_bufferTextStorage.length > desiredLength)
        [_bufferTextStorage
         deleteCharactersInRange:NSMakeRange(desiredLength,
                                             _bufferTextStorage.length -
                                             desiredLength)];

    NSAttributedString *newlinestring = [[NSAttributedString alloc]
                                         initWithString:@"\n"
                                         attributes:attrDict];

    // Instert a newline character at the end of each line to avoid reflow
    // during live resize. (We carefully have to print around these in the
    // printToWindow method)
    for (NSUInteger r = cols; r < _bufferTextStorage.length; r += cols + 1) {
        [_bufferTextStorage replaceCharactersInRange:NSMakeRange(r, 1)
                                withAttributedString:newlinestring];
    }
}

- (NSDictionary *)findRowColorOfString:(NSMutableAttributedString *)attString index:(NSUInteger)index rowLength:(NSUInteger)rowLength {
    if (transparent)
        return nil;
    if (index > attString.length)
        index = attString.length - rowLength;
    if (index > attString.length)
        return nil;
    NSRange rangeToCheck = NSMakeRange(index, rowLength);
    if (NSMaxRange(rangeToCheck) > attString.length) {
        rangeToCheck.length = attString.length - rangeToCheck.location;
        rowLength = rangeToCheck.length;
    }
    NSRange range;
    NSColor *bg = [attString attribute:NSBackgroundColorAttributeName atIndex:index longestEffectiveRange:&range inRange:rangeToCheck];
    if (rowLength < 2)
        rowLength = 2;
    if (bg && range.length > rowLength - 2) {
        NSMutableDictionary *attrDict = [[NSMutableDictionary alloc] init];
        attrDict[NSBackgroundColorAttributeName] = bg;
        NSValue *reverse = [attString attribute:@"ReverseVideo" atIndex:index effectiveRange:nil];
        if (reverse)
            attrDict[@"ReverseVideo"] = reverse;
        ZColor *zCol = [attString attribute:@"ZColor" atIndex:index effectiveRange:nil];
        if (zCol)
            attrDict[@"ZColor"] = zCol;
        return attrDict;
    }

    return nil;
}

- (void)moveToColumn:(NSUInteger)c row:(NSUInteger)r {
    //For Bureaucracy form accessibility
    if (self.glkctl.form
       && (r != ypos || labs((NSInteger)c - (NSInteger)xpos) > 1)) {
        xpos = c;
        ypos = r;
        [self.glkctl.form movedFromField];
    }

    xpos = c;
    ypos = r;
}

- (void)clear {
    _hasNewText = NO;
    NSRange selectedRange = _textview.selectedRange;
    if (!_bufferTextStorage.length)
        return;
    _bufferTextStorage = [[NSMutableAttributedString alloc]
                          initWithString:@""];
    dirty = YES;

    rows = cols = xpos = ypos = 0;

    // Re-fill with spaces
    if (self.framePending) {
        self.frame = self.pendingFrame;
    } else {
        self.frame = self.frame;
    }

    NSDictionary *attributes = [self getCurrentAttributesForStyle:style_Normal];
    NSColor *background = attributes[NSBackgroundColorAttributeName];
    if (background && bgnd != background.integerColor) {
        bgnd = background.integerColor;
        [self recalcBackground];
    }

    if (NSMaxRange(selectedRange) > _textview.textStorage.length) {
        if (_textview.textStorage.length) {
            selectedRange = NSMakeRange(_textview.textStorage.length - 1, 0);
        } else {
            selectedRange = NSMakeRange(0, 0);
        }
    }
    _textview.selectedRange = selectedRange;
}

- (void)putString:(NSString *)string style:(NSUInteger)stylevalue {
    if (line_request)
        NSLog(@"Printing to text grid window during line request");

    [self printToWindow:string style:stylevalue];
}

- (void)printToWindow:(NSString *)string style:(NSUInteger)stylevalue {
    GlkController *glkctl = self.glkctl;

    if (_bufferTextStorage.length == 0) {
        if (textstorage.length) {
            _bufferTextStorage = [textstorage mutableCopy];
            dirty = YES;
        } else return;
    }

    if (cols == 0 || rows == 0 || string.length == 0) {
        return;
    }

    // Speak each letter when typing in Bureaucracy form. (Strings starting with a
    // space never reached this branch before fixCollapsingSpaceRows took over the
    // collapsing-space workaround, so keep excluding them.)
    if (![string hasPrefix:@" "]
        && glkctl.form
        && string.length == 1
        && _keyPressTimeStamp.timeIntervalSinceNow > -0.5
        && [_lastKeyPress caseInsensitiveCompare:string] == NSOrderedSame) {
        // Don't echo keys if speak command setting is off
        if (glkctl.theme.vOSpeakCommand) {
            [glkctl speakStringNow:[string lowercaseString]];
        }
        glkctl.form.dontSpeakField = YES;
    }

    NSMutableDictionary *attrDict = [self getCurrentAttributesForStyle:stylevalue];

    // \n and \r each start a new row.
    NSCharacterSet *newlines = [NSCharacterSet characterSetWithCharactersInString:@"\n\r"];
    NSArray<NSString *> *lines = [string componentsSeparatedByCharactersInSet:newlines];
    for (NSUInteger i = 0; i < lines.count; i++) {
        if (i > 0) {
            xpos = 0;
            ypos++;
        }
        [self printLine:lines[i] attributes:attrDict];
    }
}

// Writes text without newlines at the cursor, wrapping at the right edge, and
// leaves the cursor after it. Whatever falls below the last row is dropped.
- (void)printLine:(NSString *)string attributes:(NSDictionary *)attrDict {
    NSUInteger pos = 0;
    NSUInteger textstoragelength = _bufferTextStorage.length;

    if (string.length == 0)
        return;

    if (xpos > cols) {
        ypos += xpos / cols;
        xpos = xpos % cols;
    }

    if (self.indexOfPos > textstoragelength) {
        // We are outside window visible range!
        // Do nothing
        return;
    }

    if (ypos > rows) {
        NSLog(@"printToWindow: ypos outside visible range");
        return;
    }

    // Write this string
    while (pos < string.length) {
        // Can't write if we've fallen off the end of the window
        if (((NSInteger)cols > -1 && ypos > textstoragelength / (cols + 1) ) || ypos > rows)
            break;

        // Can only write a certain number of characters
        if (xpos >= cols) {
            xpos = 0;
            ypos++;
            continue;
        }

        // Get the number of characters to write
        NSUInteger amountToDraw = cols - xpos;
        if (amountToDraw > string.length - pos) {
            amountToDraw = string.length - pos;
        }

        if (self.indexOfPos + amountToDraw > textstoragelength)
            amountToDraw = textstoragelength - self.indexOfPos + 1;

        if (amountToDraw < 1)
            break;

        NSRange replaceRange = NSMakeRange(self.indexOfPos, amountToDraw);
        if (NSMaxRange(replaceRange) > textstoragelength) {
            if (self.indexOfPos > textstoragelength)
                return;
            else {
                NSUInteger diff = NSMaxRange(replaceRange) - textstoragelength;
                amountToDraw -= diff;
                if (!amountToDraw)
                    return;
                replaceRange = NSMakeRange(self.indexOfPos, amountToDraw);
            }
        }

        // "Draw" the characters
        NSAttributedString *partString = [[NSAttributedString alloc]
                                          initWithString:[string substringWithRange:
                                                          NSMakeRange(pos, amountToDraw)]
                                          attributes:attrDict];
        [_bufferTextStorage
         replaceCharactersInRange:replaceRange withAttributedString:partString];

        // Update the x position (and the y position if necessary). Advance to
        // the next row only once the cursor has actually filled the last column
        // (xpos == cols); using cols - 1 here wrapped one column early, so a row
        // exactly cols - 1 long was followed by a spurious blank row when the
        // caller then printed a newline (e.g. Comprehend's room descriptions).
        xpos += amountToDraw;
        pos += amountToDraw;
        if (xpos >= cols) {
            xpos = 0;
            ypos++;
        }
    }

    _hasNewText = YES;
    dirty = YES;
}

- (NSUInteger)unputString:(NSString *)buf {

    // Only the text just before the cursor on its own row can be unprinted,
    // and xpos must not wrap when it is moved back.
    NSUInteger endpos = self.indexOfPos;
    if (buf.length > xpos || endpos > _bufferTextStorage.length) {
        return 0;
    }
    NSUInteger startpos = endpos - buf.length;

    NSString *stringToRemove = [_bufferTextStorage.string substringWithRange:NSMakeRange(startpos, buf.length)].uppercaseString;

    if ([stringToRemove isEqualToString:buf.uppercaseString]) {
        NSString *spaces = [NSString spacesOfLength:buf.length];
        xpos -= buf.length;
        [self putString:spaces style:style_Normal];
        xpos -= buf.length;
    } else {
        NSLog(@"GlkTextGridWindow unputString: string \"%@\" not found!", buf);
        return 0;
    }
    return buf.length;
}

#pragma mark Cells

// Each row of the buffer is cols cells and, on every row but the last, a
// newline, so a row starts every cols + 1 characters.
- (NSUInteger)indexOfColumn:(NSUInteger)column row:(NSUInteger)row {
    return row * (cols + 1) + column;
}

- (NSUInteger)rowOfIndex:(NSUInteger)index {
    return index / (cols + 1);
}

- (NSUInteger)columnOfIndex:(NSUInteger)index {
    return index % (cols + 1);
}

// The selection is remembered by its cell, so that it can be found again
// after the grid has changed width.
- (void)rememberSelectionCell:(NSUInteger)index {
    _selectedRow = [self rowOfIndex:index];
    _selectedCol = [self columnOfIndex:index];
}

- (NSRange)selectionAtRememberedCell {
    return NSMakeRange([self indexOfColumn:_selectedCol row:_selectedRow], _restoredSelection.length);
}

- (NSUInteger)indexOfPos {
    return [self indexOfColumn:xpos row:ypos];
}

- (NSSize)currentSizeInChars {
    return NSMakeSize((CGFloat)cols, (CGFloat)rows);
}

// The character shown in the cell at point.x, point.y (a column and row, not
// view coordinates), or 0 outside the grid.
- (unichar)characterAtPoint:(NSPoint)point {
    if (point.x < 0 || point.y < 0 || point.x >= cols || point.y >= rows)
        return 0;
    NSUInteger index = [self indexOfColumn:(NSUInteger)point.x row:(NSUInteger)point.y];
    if (textstorage.length <= index)
        return 0;
    return [textstorage.string characterAtIndex:index];
}

@end
