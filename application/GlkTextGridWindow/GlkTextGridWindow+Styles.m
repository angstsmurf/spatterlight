//
//  GlkTextGridWindow+Styles.m
//  Spatterlight
//
//  Theme styles, the background colour taken from the grid, and the
//  Beyond Zork graphical font.
//

#import "GlkTextGridWindowPrivate.h"
#import "GlkController.h"
#import "GlkController+BorderColor.h"
#import "GridTextView.h"
#import "InputTextField.h"
#import "Theme.h"
#import "GlkStyle.h"
#import "Game.h"
#import "ZColor.h"
#import "NSColor+integer.h"
#import "NSFont+Categories.h"

@implementation GlkTextGridWindow (Styles)

#pragma mark Colors and styles

- (GlkStyle *)themeStyle:(NSUInteger)style {
    return [self.theme valueForKey:gGridStyleNames[style]];
}

- (NSColor *)themeBackground {
    return self.theme.gridBackground;
}

- (int32_t)themeLinkStyle {
    return self.theme.gridLinkStyle;
}

- (void)prefsDidChange {
    NSRange selectedRange = _textview.selectedRange;

    GlkController *glkctl = self.glkctl;

    // Adjust terminators for Beyond Zork arrow keys hack
    if ((glkctl.gameID == kGameIsBeyondZork || [glkctl zVersion6])) {
        [self adjustBZTerminators:self.pendingTerminators];
        [self adjustBZTerminators:self.currentTerminators];
    }

    BOOL different = [self syncStyleFlagsWithTheme];

    NSMutableArray *newstyles = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
    for (NSUInteger i = 0; i < style_NUMSTYLES; i++) {
        NSDictionary *attributes = [self themeAttributesForStyle:i];
        if (attributes) {
            [newstyles addObject:attributes];
            if (!different && ![newstyles[i] isEqualToDictionary:styles[i]])
                different = YES;
        } else
            [newstyles addObject:[NSNull null]];
    }

    _textview.textContainerInset = NSMakeSize(self.theme.gridMarginX, self.theme.gridMarginY);

    if (different) {
        styles = newstyles;
        if (glkctl.usesFont3)
            [self createBeyondZorkStyle];

        /* reassign styles to attributedstrings */
        // We restyle a copy of the buffer rather than of the text storage.
        // The two differ whenever something is waiting to be flushed, and
        // then it is the buffer that is right: a setFrame: has already laid
        // it out for the new cols. Going back to the text storage here would
        // leave rows of the old width under the new cols, and the next
        // setFrame: would cut them in the wrong places. This happens when
        // autorestoring a game whose theme was changed while it was closed.
        NSAttributedString *source = _bufferTextStorage.length ? [_bufferTextStorage copy] : [textstorage copy];
        _bufferTextStorage = [source mutableCopy];
        dirty = YES;

        GlkTextGridWindow * __weak weakSelf = self;

        NSArray<NSDictionary *> __block *blockStyles = styles;

        [source
         enumerateAttributesInRange:NSMakeRange(0, source.length)
         options:0
         usingBlock:^(NSDictionary *attrs, NSRange range, BOOL *stop) {

            // First, we overwrite all attributes with those in our updated
            // styles array
            id styleobject = attrs[@"GlkStyle"];
            if (styleobject) {
                NSDictionary *blockattributes = blockStyles[(NSUInteger)[styleobject intValue]];
                [weakSelf.bufferTextStorage setAttributes:blockattributes range:range];
            }
            // Then, we re-add all the "non-Glk" style values we want to keep
            // (hyperlinks, Z-colors and reverse video)
            for (NSString *key in @[NSLinkAttributeName, @"ZColor", @"ReverseVideo"]) {
                id value = attrs[key];
                if (value)
                    [weakSelf.bufferTextStorage addAttribute:key value:value range:range];
            }
        }];

        if (self.theme.doStyles)
            _bufferTextStorage = [self applyZColorsAndThenReverse:_bufferTextStorage];
        else
            _bufferTextStorage = [self applyReverseOnly:_bufferTextStorage];


        // Now we can replace the text storage
        [textstorage setAttributedString:_bufferTextStorage];

        [self updateLinkTextAttributesOf:_textview];

        if (NSMaxRange(selectedRange) <= textstorage.length)
            _textview.selectedRange = selectedRange;
        [self recalcBackground];
        _textview.backgroundColor = _pendingBackgroundCol;

        if (line_request) {
            if (!_enteredTextSoFar)
                _enteredTextSoFar = @"";
            if (self.input) {
                _enteredTextSoFar = [self.input.stringValue copy];
                self.input = nil;
            }

            [self scheduleDeferredInitLine:_enteredTextSoFar delay:0];
        }

        // After the game has ended there is no interpreter to re-arrange
        // the windows, so stretch to the game view the way the autoresizing
        // mask would.
        if (!glkctl.isAlive) {
            NSRect frame = self.frame;

            if ((self.autoresizingMask & NSViewWidthSizable) == NSViewWidthSizable) {
                frame.size.width = glkctl.gameView.frame.size.width - frame.origin.x;
            }

            if ((self.autoresizingMask & NSViewHeightSizable) == NSViewHeightSizable) {
                frame.size.height = glkctl.gameView.frame.size.height - frame.origin.y;
            }
            self.frame = frame;
            [self checkForUglyBorder];
            [self flushDisplay];
        }
    }
}

- (void)recalcBackground {
    // The pending colour this computes is normally superseded by
    // checkForUglyBorder at the next flush. Make sure that flush does the
    // full work, so the colour that reaches the text view is the same one
    // as before the buffer-changed gate existed.
    dirty = YES;

    NSColor *bgcolor = styles[style_Normal][NSBackgroundColorAttributeName];
    GlkController *glkctl = self.glkctl;

    NSString *detectedFormat = glkctl.game.detectedFormat;
    if (!([detectedFormat isEqualToString:@"glulx"] || [detectedFormat isEqualToString:@"hugo"] || [detectedFormat isEqualToString:@"zcode"])) {
        bgcolor = styles[style_User1][NSBackgroundColorAttributeName];
    }

    if (self.theme.doStyles && bgnd > -1 && bgnd != zcolor_Default) {
        bgcolor = [NSColor colorFromInteger:bgnd];
    }

    if (!bgcolor)
        bgcolor = self.theme.gridBackground;

    if (transparent || bgnd == zcolor_Transparent)
        bgcolor = [NSColor clearColor];
    else
        [glkctl setBorderColor:bgcolor fromWindow:self];

    _pendingBackgroundCol = bgcolor;
    _textview.insertionPointColor = bgcolor;
}

- (void)setBgColor:(NSInteger)bc {
    bgnd = bc;

    [self recalcBackground];
}

- (void)checkForUglyBorder {
    // This checks whether all the text in the grid window uses the same background color attribute,
    // and if so, uses this color for the window background color.
    // It is complicated by the fact that some games have a status line in reverse video, but leaves
    // a single character at the end non-reversed. Also, our setFrame implementation inserts newline
    // charaters at the end of every row, and those may not have the right background color. So we
    // simply count the background color runs and check if one fills the window (which we fudge to
    // mean "length of textstorage minus two colums".)

    NSAttributedString __block *blockTextStorage = _bufferTextStorage;

    if (!blockTextStorage.length) {
        if (textstorage.length) {
            blockTextStorage = textstorage;
        } else return;
    }

    NSMutableDictionary<NSColor *, NSValue *> __block *colors = [[NSMutableDictionary alloc] init];

    [blockTextStorage
     enumerateAttribute:NSBackgroundColorAttributeName
     inRange:NSMakeRange(0, blockTextStorage.length)
     options:0
     usingBlock:^(NSColor *color, NSRange range, BOOL *stop) {
        if (color) {
            NSValue *rangeVal = colors[color];
            if (rangeVal) {
                NSRange storedRange = rangeVal.rangeValue;
                storedRange.length += range.length;
                colors[color] = [NSValue valueWithRange:storedRange];
            } else {
                colors[color] = [NSValue valueWithRange:range];
            }
        }
    }];

    NSColor *bgCol = nil;

    if (colors.count) {
        NSUInteger longest = 0;
        for (NSColor *col in colors.allKeys) {
            NSValue *rangeVal = colors[col];
            NSUInteger length = rangeVal.rangeValue.length;
            if (length > longest) {
                longest = length;
                bgCol = col;
            }
        }
        // Highlighted words (such as the yellow "menu" links in WITHOUT
        // Corporation) can add up to far more than two columns per row,
        // so also accept a color that fills most of the window as long as
        // it is the background of the first and last cell of every row,
        // which are the ones that border the margins.
        if (longest < blockTextStorage.length - rows * 2 &&
            (longest < blockTextStorage.length / 2 ||
             ![self color:bgCol isAtRowEdgesOf:blockTextStorage]))
            bgCol = nil;
    }

    if (!bgCol) {
        if (bgnd < 0 || !self.theme.doStyles) {
            bgCol = self.theme.gridBackground;
        }
        else {
            bgCol = [NSColor colorFromInteger:bgnd];
        }
    }
    _pendingBackgroundCol = bgCol;
}

- (BOOL)color:(NSColor *)color isAtRowEdgesOf:(NSAttributedString *)storage {
    // Each row is cols characters followed by a newline.
    NSUInteger rowLength = cols + 1;
    if (cols < 1 || storage.length < rowLength)
        return NO;
    for (NSUInteger start = 0; start + rowLength <= storage.length; start += rowLength) {
        NSColor *first = [storage attribute:NSBackgroundColorAttributeName atIndex:start effectiveRange:nil];
        NSColor *last = [storage attribute:NSBackgroundColorAttributeName atIndex:start + cols - 1 effectiveRange:nil];
        if (![first isEqual:color] || ![last isEqual:color])
            return NO;
    }
    return YES;
}

// With certain fonts, a line consisting of nothing but spaces "collapses": the
// text system gives it a zero-width used rect and draws none of it, so a blank
// reverse-video row is not painted at all. Replacing the row's first space with a
// non-breaking one is enough to stop that. (Idea originally from David
// Schweinsberg and Yazmin.) Still reproducible today -- of the fixed-pitch
// families installed here it hits Source Code Pro, PT Mono, Noto Sans Mono and
// our own bundled FreeFont3, while Courier, Monaco, Menlo, Courier New, Andale
// Mono and SF Mono are unaffected.
//
// Only rows that are *entirely* whitespace collapse: one ordinary glyph anywhere
// in the row and the text system paints the background across the full width
// again. So we decide this per row on the finished buffer, rather than on every
// string handed to printToWindow:. Doing it at print time stamped a non-breaking
// space into any string that merely began with one, which is how ADRIFT status
// lines picked up a stray U+00A0 in column 0 -- Scarier fills the whole bar with
// spaces in style_User1 before writing the room name over it, so the fill was
// substituted even though the finished row was never blank. In a font without a
// U+00A0 glyph that shows up as an "unprintable character" box.
//
// A game that prints its own non-breaking space in column 0 of a non-blank row
// gets it turned back into an ordinary space, which renders identically. Rows
// whose font has no U+00A0 glyph at all are left alone; see below.
//
// A quote box needs this as much as the grid it was cut out of: its padding
// rows are blank reverse video, and they come out of the *middle* of a grid
// row, so whatever protected that row is not part of them. Its rows are not
// cols wide, either -- setFrame: only estimates cols from the box's frame, and
// a merged side-by-side box may have rows of different lengths -- so there we
// go by the newlines that quotebox: put at the end of every row.
- (void)fixCollapsingSpaceRows {
    BOOL isQuoteBox = (_quoteboxSize.width > 0);
    if ((cols == 0 && !isQuoteBox) || _bufferTextStorage.length == 0)
        return;

    static NSCharacterSet *nonSpace = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        const unichar spaces[2] = {' ', 0xa0};
        nonSpace = [[NSCharacterSet characterSetWithCharactersInString:
                     [NSString stringWithCharacters:spaces length:2]] invertedSet];
    });

    const unichar nbsp = 0xa0;
    NSString *nbspString = [NSString stringWithCharacters:&nbsp length:1];
    NSString *string = _bufferTextStorage.string;
    NSUInteger length = string.length;

    NSUInteger next;
    for (NSUInteger start = 0; start < length; start = next) {
        NSRange row;
        if (isQuoteBox) {
            NSRange newline = [string rangeOfString:@"\n"
                                            options:0
                                              range:NSMakeRange(start, length - start)];
            NSUInteger end = (newline.location == NSNotFound) ? length : newline.location;
            row = NSMakeRange(start, end - start);
            next = end + 1;
            if (row.length == 0)
                continue;
        } else {
            row = NSMakeRange(start, MIN(cols, length - start));
            next = start + cols + 1;
            if (row.length == 0)
                break;
        }

        BOOL blank = ([string rangeOfCharacterFromSet:nonSpace
                                              options:0
                                                range:row].location == NSNotFound);
        BOOL substituted = ([string characterAtIndex:row.location] == nbsp);
        if (blank == substituted)
            continue;

        // A font with no U+00A0 glyph draws an "unprintable character" box in its
        // place, which is worse than the collapsed row we are trying to avoid, so
        // leave those rows as they are. (Undoing an earlier substitution is never
        // skipped: that is the direction that removes such a box.)
        if (blank) {
            NSFont *font = [_bufferTextStorage attribute:NSFontAttributeName
                                                 atIndex:row.location
                                          effectiveRange:NULL];
            if (font && !font.hasNonBreakingSpaceGlyph)
                continue;
        }

        // replaceCharactersInRange:withString: gives the new character the
        // attributes of the one it replaces, so the row's background run survives.
        [_bufferTextStorage replaceCharactersInRange:NSMakeRange(row.location, 1)
                                          withString:blank ? nbspString : @" "];
    }
}

#pragma mark Beyond Zork graphical font

- (void)createBeyondZorkStyle {
    CGFloat pointSize = self.theme.gridNormal.font.pointSize;
    NSFont *zorkFont = [NSFont fontWithName:@"FreeFont3" size:pointSize];

    if (!zorkFont) {
        NSLog(@"Error! No Zork Font Found!");
        return;
    }

    // First we find a point size that matches the width of the Normal style font
    NSString *normalTextSample = [self sampleStringWithString:@"The horizon is lost in the glare of morning upon the Great Sea. "];
    NSString *zorkTextSample = [self sampleStringWithString:@"/''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''0 "];

    CGFloat desiredWidth = [self.theme.gridNormal.font widthForPointSize:pointSize sampleText:normalTextSample];
    zorkFont = [zorkFont fontToFitWidth:desiredWidth sampleText:zorkTextSample];
    // Then we switch off anything that may cause gaps in the default BlockQuote style font
    NSMutableDictionary *beyondZorkStyle = [styles[style_Normal] mutableCopy];
    NSString *normalFontName = self.theme.gridNormal.font.fontName;
    BOOL isMonaco = ([normalFontName isEqualToString:@"Monaco"]);

    beyondZorkStyle[@"GlkStyle"] = @(style_BlockQuote);

    NSMutableParagraphStyle *para = [beyondZorkStyle[NSParagraphStyleAttributeName] mutableCopy];
    beyondZorkStyle[NSFontAttributeName] = zorkFont;

    // Create an NSAffineTransform that stretches the font to our cellHeight
    // This is where things usually go wrong, i.e. the result is too short or too tall
    NSAffineTransform *transform = [[NSAffineTransform alloc] init];
    [transform scaleBy:zorkFont.pointSize];
    CGFloat yscale = (self.theme.cellHeight + 0.5 + 0.1 * self.theme.bZAdjustment) / zorkFont.boundingRectForFont.size.height;
    if (isMonaco && self.glkctl.gameID == kGameIsJourney)
        yscale *= 1.2;
    [transform scaleXBy:1 yBy:yscale];

    zorkFont = [NSFont fontWithDescriptor:zorkFont.fontDescriptor textTransform:transform];

    if (!zorkFont)
        NSLog(@"Failed to create Zork Font!");

    beyondZorkStyle[NSFontAttributeName] = zorkFont;
    beyondZorkStyle[NSParagraphStyleAttributeName] = para;

    styles[style_BlockQuote] = beyondZorkStyle;
}

// Convenience method to create an exactly 100 characters wide string
// by repeating the input string.
- (NSString *)sampleStringWithString:(NSString *)str {
    if (!str.length)
        return nil;

    while (str.length < 100) {
        str = [str stringByAppendingString:str];
    }
    str = [str substringToIndex:99];
    return str;
}

@end
