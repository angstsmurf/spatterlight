
/*
 * GlkTextGridWindow
 */

#import "GlkTextGridWindow.h"
#import "GlkTextBufferWindow.h"
#import "GlkController.h"
#import "Preferences.h"
#import "GlkEvent.h"

#import "InputTextField.h"
#import "InputHistory.h"
#import "GridTextView.h"
#import "BufferTextView.h"

#import "NSString+Categories.h"
#import "Theme.h"
#import "GlkStyle.h"
#import "Constants.h"
#import "Game.h"
#import "Metadata.h"
#import "ZColor.h"
#import "ZMenu.h"
#import "BureaucracyForm.h"

#import "NSColor+integer.h"
#import "NSFont+Categories.h"

#include "glkimp.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
    fprintf(stderr, "%s\n",                                                    \
            [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@interface GlkTextGridWindow () <NSSecureCoding, NSTextViewDelegate, NSTextStorageDelegate, NSTextFieldDelegate> {
    NSScrollView *scrollview;
    NSTextStorage *textstorage;
    NSLayoutManager *layoutmanager;
    NSTextContainer *container;
    NSUInteger rows, cols;
    NSUInteger xpos, ypos;
    NSUInteger maxInputLength;
    BOOL line_request;
    BOOL hyper_request;
    BOOL mouse_request;
    BOOL transparent;
    NSInteger terminator;
}
@end

@implementation GlkTextGridWindow

+ (BOOL) supportsSecureCoding {
    return YES;
}

+ (BOOL)isCompatibleWithResponsiveScrolling
{
    return YES;
}

- (instancetype)initWithGlkController:(GlkController *)glkctl_
                                 name:(NSInteger)name_ {
    self = [super initWithGlkController:glkctl_ name:name_];

    if (self) {
        // lines = [[NSMutableArray alloc] init];

        NSDictionary *styleDict = nil;

        self.styleHints = [self deepCopyOfStyleHintsArray:glkctl_.gridStyleHints];

        styles = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
        for (NSUInteger i = 0; i < style_NUMSTYLES; i++) {

            if (self.theme.doStyles) {
                styleDict = [((GlkStyle *)[self.theme valueForKey:gGridStyleNames[i]]) attributesWithHints:self.styleHints[i]];
            } else {
                styleDict = ((GlkStyle *)[self.theme valueForKey:gGridStyleNames[i]]).attributeDict;
            }

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

        layoutmanager = [[NSLayoutManager alloc] init];
        layoutmanager.backgroundLayoutEnabled = YES;

        [textstorage addLayoutManager:layoutmanager];

        container = [[NSTextContainer alloc]
                     initWithContainerSize:NSMakeSize(10000000, 10000000)];

        container.layoutManager = layoutmanager;
        [layoutmanager addTextContainer:container];

        _textview = [[GridTextView alloc] initWithFrame:NSMakeRect(0, 0, 0, 0)
                                           textContainer:container];

        _textview.minSize = NSMakeSize(0, 0);
        _textview.maxSize = NSMakeSize(10000000, 10000000);

        _textview.autoresizingMask = NSViewWidthSizable;

        container.textView = _textview;
        scrollview.documentView = _textview;

        /* now configure the text stuff */

        _textview.delegate = self;
        textstorage.delegate = self;
        _textview.textContainerInset =
            NSMakeSize(self.theme.gridMarginX, self.theme.gridMarginY);
        _textview.backgroundColor = self.theme.gridBackground;

        NSMutableDictionary *linkAttributes = [_textview.linkTextAttributes mutableCopy];
        linkAttributes[NSForegroundColorAttributeName] = styles[style_Normal][NSForegroundColorAttributeName];
        linkAttributes[NSUnderlineStyleAttributeName] = @(self.theme.gridLinkStyle);
        _textview.linkTextAttributes = linkAttributes;

        _textview.editable = NO;
        _textview.usesFontPanel = NO;
        _restoredSelection = NSMakeRange(0, 0);

        history = [[InputHistory alloc] init];

        [self addSubview:scrollview];
        [self recalcBackground];

        if (self.glkctl.usesFont3)
            [self createBeyondZorkStyle];

        underlineLinks = (self.theme.gridLinkStyle != NSUnderlineStyleNone);
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

        line_request = [decoder decodeBoolForKey:@"line_request"];
        hyper_request = [decoder decodeBoolForKey:@"hyper_request"];
        mouse_request = [decoder decodeBoolForKey:@"mouse_request"];

        rows = (NSUInteger)[decoder decodeIntegerForKey:@"rows"];
        cols = (NSUInteger)[decoder decodeIntegerForKey:@"cols"];
        xpos = (NSUInteger)[decoder decodeIntegerForKey:@"xpos"];
        ypos = (NSUInteger)[decoder decodeIntegerForKey:@"ypos"];

        _selectedRow = (NSUInteger)[decoder decodeIntegerForKey:@"selectedRow"];
        _selectedCol = (NSUInteger)[decoder decodeIntegerForKey:@"selectedCol"];
        _selectedString = [decoder decodeObjectOfClass:[NSString class] forKey:@"selectedString"];

        dirty = YES;
        transparent = [decoder decodeBoolForKey:@"transparent"];

        _restoredSelection =
        ((NSValue *)[decoder decodeObjectOfClass:[NSValue class] forKey:@"selectedRange"])
        .rangeValue;

        _pendingBackgroundCol = [decoder decodeObjectOfClass:[NSColor class] forKey:@"pendingBackgroundCol"];
        _bufferTextStorage = [decoder decodeObjectOfClass:[NSMutableAttributedString class] forKey:@"bufferTextStorage"];

        _enteredTextSoFar = [decoder decodeObjectOfClass:[NSString class] forKey:@"inputString"];
        maxInputLength = (NSUInteger)[decoder decodeIntForKey:@"maxInputLength"];

        _quoteboxSize = ((NSValue *)[decoder decodeObjectOfClass:[NSValue class] forKey:@"quoteboxSize"]).sizeValue;
        _quoteboxAddedOnPAC = [decoder decodeIntegerForKey:@"quoteboxAddedOnTurn"];
        _quoteboxVerticalOffset = (NSUInteger)[decoder decodeIntegerForKey:@"quoteboxVerticalOffset"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeObject:_textview forKey:@"_textview"];
    [encoder encodeBool:line_request forKey:@"line_request"];
    [encoder encodeBool:hyper_request forKey:@"hyper_request"];
    [encoder encodeBool:mouse_request forKey:@"mouse_request"];
    [encoder encodeInteger:(NSInteger)rows forKey:@"rows"];
    [encoder encodeInteger:(NSInteger)cols forKey:@"cols"];
    [encoder encodeInteger:(NSInteger)xpos forKey:@"xpos"];
    [encoder encodeInteger:(NSInteger)ypos forKey:@"ypos"];
    [encoder encodeBool:transparent forKey:@"transparent"];
    NSValue *rangeVal = [NSValue valueWithRange:_textview.selectedRange];
    [encoder encodeObject:rangeVal forKey:@"selectedRange"];

    [encoder encodeInteger:(NSInteger)(_textview.selectedRange.location / (cols + 1)) forKey:@"selectedRow"];
    [encoder encodeInteger:(NSInteger)(_textview.selectedRange.location % (cols + 1)) forKey:@"selectedCol"];
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

    [self flushDisplay];

    _restoredSelection = restoredWin.restoredSelection;
    _selectedRow = restoredWin.selectedRow;
    _selectedCol = restoredWin.selectedCol;
    _selectedString = restoredWin.selectedString;

    if (NSMaxRange(_restoredSelection) < textstorage.length && [[textstorage.string substringWithRange:_restoredSelection] isEqualToString:_selectedString]) {
        _textview.selectedRange = _restoredSelection;
    } else {
        _restoredSelection = NSMakeRange(_selectedCol + _selectedRow * (cols + 1), _restoredSelection.length);
        if (NSMaxRange(_restoredSelection) < textstorage.length && [[textstorage.string substringWithRange:_restoredSelection] isEqualToString:_selectedString]) {
            _textview.selectedRange = _restoredSelection;
        }
    }

    _enteredTextSoFar = restoredWin.enteredTextSoFar;

    if (line_request) {
        GlkTextGridWindow * __weak weakSelf = self;

        NSString __block *inputString = _enteredTextSoFar;
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf performSelector:@selector(deferredInitLine:) withObject:inputString afterDelay:0.5];
        });
    }
}

#pragma mark Colors and styles

- (void)prefsDidChange {
    NSDictionary *attributes;
    NSRange selectedRange = _textview.selectedRange;

    GlkController *glkctl = self.glkctl;

    // Adjust terminators for Beyond Zork arrow keys hack
    if ((glkctl.gameID == kGameIsBeyondZork || [glkctl zVersion6])) {
        [self adjustBZTerminators:self.pendingTerminators];
        [self adjustBZTerminators:self.currentTerminators];
    }

    NSUInteger i;

    NSMutableArray *newstyles = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
    BOOL different = NO;
    for (i = 0; i < style_NUMSTYLES; i++) {
        if (self.theme.doStyles) {
            // We're doing styles, so we call the current theme object with our hints array
            // in order to get an attributes dictionary
            attributes = [((GlkStyle *)[self.theme valueForKey:gGridStyleNames[i]]) attributesWithHints:self.styleHints[i]];
        } else {
            // We're not doing styles, so use the raw style attributes from
            // the theme object's attributeDict object
            attributes = ((GlkStyle *)[self.theme valueForKey:gGridStyleNames[i]]).attributeDict;
        }

        if (usingStyles != self.theme.doStyles) {
            different = YES;
            usingStyles = self.theme.doStyles;
        }

        if (underlineLinks != (self.theme.gridLinkStyle != NSUnderlineStyleNone)) {
            different = YES;
            underlineLinks = (self.theme.gridLinkStyle != NSUnderlineStyleNone);
        }

        if (attributes) {
            [newstyles addObject:attributes];
            if (!different && ![newstyles[i] isEqualToDictionary:styles[i]])
                different = YES;
        } else
            [newstyles addObject:[NSNull null]];
    }

    NSInteger marginX = self.theme.gridMarginX;
    NSInteger marginY = self.theme.gridMarginY;

    _textview.textContainerInset = NSMakeSize(marginX, marginY);

    if (different) {
        styles = newstyles;
        if (glkctl.usesFont3)
            [self createBeyondZorkStyle];

        /* reassign styles to attributedstrings */
        // We create a copy of the text storage
        _bufferTextStorage = [textstorage mutableCopy];

        GlkTextGridWindow * __weak weakSelf = self;

        NSArray<NSDictionary *> __block *blockStyles = styles;

        [textstorage
         enumerateAttributesInRange:NSMakeRange(0, textstorage.length)
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
            id hyperlink = attrs[NSLinkAttributeName];
            if (hyperlink) {
                [weakSelf.bufferTextStorage addAttribute:NSLinkAttributeName
                                                   value:hyperlink
                                                   range:range];
            }

            id zcolor = attrs[@"ZColor"];
            if (zcolor) {
                [weakSelf.bufferTextStorage addAttribute:@"ZColor"
                                                   value:zcolor
                                                   range:range];
            }

            id reverse = attrs[@"ReverseVideo"];
            if (reverse) {
                [weakSelf.bufferTextStorage addAttribute:@"ReverseVideo"
                                                   value:reverse
                                                   range:range];
            }

        }];

        if (self.theme.doStyles)
            _bufferTextStorage = [self applyZColorsAndThenReverse:_bufferTextStorage];
        else
            _bufferTextStorage = [self applyReverseOnly:_bufferTextStorage];

//        [_bufferTextStorage addAttribute:NSCursorAttributeName value:[NSCursor arrowCursor] range:NSMakeRange(0, _bufferTextStorage.length)];

        // Now we can replace the text storage
        [textstorage setAttributedString:_bufferTextStorage];

        NSMutableDictionary *linkAttributes = [_textview.linkTextAttributes mutableCopy];
        linkAttributes[NSUnderlineStyleAttributeName] = @(self.theme.gridLinkStyle);
        linkAttributes[NSForegroundColorAttributeName] = styles[style_Normal][NSForegroundColorAttributeName];
        _textview.linkTextAttributes = linkAttributes;

        _textview.selectedRange = selectedRange;
        dirty = NO;

        [self recalcBackground];
        _textview.backgroundColor = _pendingBackgroundCol;

        if (line_request) {
            if (!_enteredTextSoFar)
                _enteredTextSoFar = @"";
            if (self.input) {
                _enteredTextSoFar = [self.input.stringValue copy];
                self.input = nil;
            }

            dispatch_async(dispatch_get_main_queue(), ^{
                [self performSelector:@selector(deferredInitLine:) withObject:weakSelf.enteredTextSoFar afterDelay:0];
            });
        }

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

- (BOOL)allowsDocumentBackgroundColorChange {
    return YES;
}

- (void)recalcBackground {
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
        if (longest < blockTextStorage.length - rows * 2)
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

- (void)flushDisplay {
    // Making the textview temporarily editable
    // reduces flicker in Hugo Tetris, for some reason.
    _textview.editable = YES;
    NSRange selectedRange = _textview.selectedRange;
    NSString *selectedString = [textstorage.string substringWithRange:selectedRange];
    _selectedRow = selectedRange.location / (cols + 1);
    _selectedCol = selectedRange.location % (cols + 1);

    if (self.framePending) {
        if (!NSEqualRects(self.frame, self.pendingFrame)) {
            super.frame = self.pendingFrame;
        }
        if (!NSEqualRects(_textview.frame, self.bounds)) {
            _textview.frame = self.bounds;
        }

        self.framePending = NO;
    }

    if (!transparent)
        [self checkForUglyBorder];

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

    _restoredSelection = NSMakeRange(_selectedCol + _selectedRow * (cols + 1), _restoredSelection.length);
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

    NSUInteger r;

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

        _selectedRow = _restoredSelection.location / (cols + 1);
        _selectedCol = _restoredSelection.location % (cols + 1);

        if (newcols * self.theme.cellWidth > screensize.width || newrows * self.theme.cellHeight > screensize.height) {
            return;
        }

        NSMutableDictionary *attrDict = [styles[style_Normal] mutableCopy];

        if (cols == 0 || rows == 0)
            _bufferTextStorage = nil;

        if (!_bufferTextStorage || !_bufferTextStorage.length) {
            NSString *spaces = [[[NSString alloc] init]
                                stringByPaddingToLength:(NSUInteger)(rows * (cols + 1) - (cols > 1))
                                withString:@" "
                                startingAtIndex:0];
            _bufferTextStorage = [[NSTextStorage alloc]
                                  initWithString:spaces
                                  attributes:attrDict];
        }

        if (newcols < cols) {
            // Delete characters if the window has become narrower
            for (r = cols - 1; r < _bufferTextStorage.length; r += cols + 1) {
                if (r < (cols - newcols))
                    continue;
                NSRange deleteRange =
                NSMakeRange(r - (cols - newcols), cols - newcols);
                if (NSMaxRange(deleteRange) > _bufferTextStorage.length)
                    deleteRange =
                    NSMakeRange(r - (cols - newcols),
                                _bufferTextStorage.length - (r - (cols - newcols)));

                [_bufferTextStorage deleteCharactersInRange:deleteRange];
                r -= (cols - newcols);
            }
            // For some reason we must remove a couple of extra characters at the
            // end to avoid strays
            if (rows == 1 && cols > 1 &&
                _bufferTextStorage.length >= (cols - 2))
                [_bufferTextStorage
                 deleteCharactersInRange:NSMakeRange(cols - 2,
                                                     _bufferTextStorage.length -
                                                     (cols - 2))];
        } else if (newcols > cols) {
            // Pad with spaces if the window has become wider
            NSString *spaces =
            [[[NSString alloc] init] stringByPaddingToLength:newcols - cols
                                                  withString:@" "
                                             startingAtIndex:0];
            NSAttributedString *string;
            NSMutableDictionary *newDict;

            for (r = cols; r < _bufferTextStorage.length - 1; r += (cols + 1)) {

                newDict = [attrDict mutableCopy];

                // Check for the background color attribute of the line.
                // If we find one, we paint over the very last characters with it
                if (r > 2) {
                    NSDictionary *bgDict = [self findRowColorOfString:_bufferTextStorage index: r - cols rowLength:cols];
                    if (bgDict) {
                        [newDict addEntriesFromDictionary:bgDict];
                    }
                }

                string = [[NSAttributedString alloc]
                          initWithString:spaces
                          attributes:newDict];
                [_bufferTextStorage insertAttributedString:string atIndex:r];

                r += (newcols - cols);
            }
        }
        cols = newcols;
        rows = newrows;

        NSUInteger desiredLength =
        rows * (cols + 1) - 1; // -1 because we don't want a newline at the very end
        if (desiredLength < 1 || rows == 1)
            desiredLength = cols;
        if (_bufferTextStorage.length < desiredLength) {
            NSDictionary *bgDict = nil;
            if ((cols + 1) * (rows - 1) < _bufferTextStorage.length)
                bgDict = [self findRowColorOfString:_bufferTextStorage index:(cols + 1) * (rows - 1) rowLength:cols];
            NSString *spaces = [[[NSString alloc] init]
                                stringByPaddingToLength:desiredLength - _bufferTextStorage.length
                                withString:@" "
                                startingAtIndex:0];
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
        for (r = cols; r < _bufferTextStorage.length; r += cols + 1) {
            [_bufferTextStorage replaceCharactersInRange:NSMakeRange(r, 1)
                                    withAttributedString:newlinestring];
        }

        _restoredSelection = NSMakeRange(_selectedCol + _selectedRow * (cols + 1), _restoredSelection.length);

    }

    if (self.inLiveResize) {
        super.frame = frame;
        self.framePending = NO;
    }
    dirty = YES;
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
       && (r != ypos || abs((int)c - (int)xpos) > 1)) {
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

    rows = cols = xpos = ypos = 0;

    // Re-fill with spaces
    if (self.framePending) {
        self.frame = self.pendingFrame;
    } else {
        self.frame = self.frame;
    }

//    if (currentZColor && bgnd != currentZColor.bg) {
//        if (currentZColor.bg != zcolor_Current && currentZColor.bg != zcolor_Default) {
//            bgnd = currentZColor.bg;
//        }
//        [self recalcBackground];
//    }

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
    NSUInteger startpos;
    NSUInteger pos = 0;

    GlkController *glkctl = self.glkctl;

    NSUInteger textstoragelength = _bufferTextStorage.length;

    if (textstoragelength == 0) {
        if (textstorage.length) {
            _bufferTextStorage = [textstorage mutableCopy];
            textstoragelength = textstorage.length;
        } else return;
    }

    if (cols == 0 || rows == 0 || string.length == 0) {
        return;
    }

    // With certain fonts and sizes, strings containing only spaces will "collapse."
    // So if the first character is a space, we replace it with a &nbsp;
    if ([string hasPrefix:@" "]) {
        const unichar nbsp = 0xa0;
        NSString *nbspstring = [NSString stringWithCharacters:&nbsp length:1];
        string = [string stringByReplacingCharactersInRange:NSMakeRange(0, 1) withString:nbspstring];
    // Speak each letter when typing in Bureaucracy form
    } else if (glkctl.form
               && string.length == 1
               && _keyPressTimeStamp.timeIntervalSinceNow > -0.5
               && [_lastKeyPress caseInsensitiveCompare:string] == NSOrderedSame) {
        // Don't echo keys if speak command setting is off
        if (glkctl.theme.vOSpeakCommand) {
            [glkctl speakStringNow:[string lowercaseString]];
        }
        glkctl.form.dontSpeakField = YES;
    }

    if (xpos > cols) {
        ypos += xpos / cols;
        xpos = xpos % cols;
    }
    NSMutableDictionary *attrDict = [self getCurrentAttributesForStyle:stylevalue];

    startpos = self.indexOfPos;
    if (startpos > textstoragelength) {
        // We are outside window visible range!
        // Do nothing
        return;
    }

    if (ypos > rows) {
        NSLog(@"printToWindow: ypos outside visible range");
        return;
    }

    // Check for newlines in string to write
    NSUInteger x;
    for (x = 0; x < string.length; x++) {
        if ([string characterAtIndex:x] == '\n' ||
            [string characterAtIndex:x] == '\r') {
            [self printToWindow:[string substringToIndex:x] style:stylevalue];
            xpos = 0;
            ypos++;
            [self printToWindow:[string substringFromIndex:x + 1]
                          style:stylevalue];
            return;
        }
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

        // Update the x position (and the y position if necessary)
        xpos += amountToDraw;
        pos += amountToDraw;
        if (xpos >= cols - 1) {
            xpos = 0;
            ypos++;
        }
    }

    _hasNewText = YES;
    dirty = YES;
}

- (NSUInteger)indexOfPos {
    return ypos * (cols + 1) + xpos;
}

- (NSSize)currentSizeInChars {
    return NSMakeSize(cols, rows);
}

- (unichar)characterAtPoint:(NSPoint)point {
    NSSize size = [self currentSizeInChars];
    NSUInteger index = (NSUInteger)(point.y * (size.width + 1.0) + point.x);
    if (textstorage.length <= index)
        return 0;
    return [textstorage.string characterAtIndex:index];
}


#pragma mark Hyperlinks

- (void)initHyperlink {
    hyper_request = YES;
}

- (void)cancelHyperlink {
    hyper_request = NO;
}

- (BOOL)textView:_textview clickedOnLink:(id)link atIndex:(NSUInteger)charIndex {
    GlkEvent *gev;
    if (!hyper_request) {
        NSLog(@"txtgrid: No hyperlink request in window.");
        // City of Secrets has hyperlinks but no hyperlink
        // requests, covered by mouse requests.
        if(mouse_request) {
            NSPoint p;
            p.y = charIndex / (cols + 1);
            p.x = charIndex % (cols + 1);
            if (p.x >= 0 && p.y >= 0 && p.x < cols && p.y < rows) {
                gev = [[GlkEvent alloc] initMouseEvent:p forWindow:self.name];
                [self.glkctl queueEvent:gev];
                mouse_request = NO;
                return YES;
            }
        }
        return NO;
    }

    gev =
    [[GlkEvent alloc] initLinkEvent:((NSNumber *)link).unsignedIntegerValue
                          forWindow:self.name];
    [self.glkctl queueEvent:gev];

    hyper_request = NO;
    return YES;
}

#pragma mark Mouse input

- (void)initMouse {
    mouse_request = YES;
}

- (void)cancelMouse {
    mouse_request = NO;
}

- (BOOL)myMouseDown:(NSEvent *)theEvent {
    GlkEvent *gev;
    if (mouse_request) {
        [self.glkctl markLastSeen];

        NSPoint point, point_in_window;
        point_in_window = theEvent.locationInWindow;

        point_in_window = [_textview convertPoint:point_in_window fromView:nil];

        point_in_window.x -= _textview.textContainerInset.width;
        point_in_window.y -= _textview.textContainerInset.height;

        NSUInteger charIndex =
        [_textview.layoutManager characterIndexForPoint:point_in_window
                                       inTextContainer:container
              fractionOfDistanceBetweenInsertionPoints:nil];

        point.y = charIndex / (cols + 1);
        point.x = charIndex % (cols + 1);
        if (point.x >= cols)
            point.x = point_in_window.x / self.theme.cellWidth;
        if (point.x >= 0 && point.y >= 0 && point.x < cols && point.y < rows) {
            [self.glkctl markLastSeen];
            gev = [[GlkEvent alloc] initMouseEvent:point forWindow:self.name];
            [self.glkctl queueEvent:gev];
            mouse_request = NO;
            return YES;
        }
    }
    return NO;
}

#pragma mark Single key input

- (void)initChar {
    char_request = YES;
    dirty = YES;

    // Draw Bureaucracy form cursor
    if (self.glkctl.gameID == kGameIsBureaucracy) {
        self.currentReverseVideo = YES;
        [self putString:@" " style:style_Normal];
        self.currentReverseVideo = NO;
        xpos--;
    }
}

- (void)cancelChar {
    char_request = NO;
    dirty = YES;

    // Remove leftover "cursors"
    // when running command scripts in
    // Bureaucracy form
    if (self.glkctl.gameID == kGameIsBureaucracy && self.glkctl.commandScriptRunning) {
        self.currentReverseVideo = NO;
        [self putString:@" " style:style_Normal];
        xpos--;
    }
}

- (void)keyDown:(NSEvent *)evt {
    NSString *str = evt.characters;
    unsigned ch = keycode_Unknown;
    if (str.length)
        ch = chartokeycode([str characterAtIndex:str.length-1]);

    NSUInteger flags = evt.modifierFlags;

    GlkController *glkctl = self.glkctl;

    if ((flags & NSEventModifierFlagNumericPad) && glkctl.gameID != kGameIsBureaucracy && ch >= '0' && ch <= '9')
        ch = keycode_Pad0 - (ch - '0');

    GlkWindow *win;
    // pass on this key press to another GlkWindow if we are not expecting one
    if (!self.wantsFocus) {
        for (win in (glkctl.gwindows).allValues) {
            if (win != self && win.wantsFocus) {
                [win grabFocus];
                NSLog(@"GlkTextGridWindow %ld: Passing on keypress to %@ %ld", self.name, win.class, win.name);
                [win keyDown:evt];
                return;
            }
        }
    }

    // Stupid hack for Swedish keyboard
    if (char_request && glkctl.gameID == kGameIsBureaucracy && evt.keyCode == 30)
        ch = '^';

    if (char_request && ch != keycode_Unknown) {
        [glkctl markLastSeen];

        if (glkctl.gameID == kGameIsBureaucracy) {
            // Bureaucracy on Bocfel will try to convert these keycodes
            // to characters and then error out with
            // "fatal error: @print_char called with invalid character"
            // so we attempt to change them into something reasonable here.
            if (ch == keycode_Up || ch == keycode_Home || ch == keycode_PageUp)
                ch = '^';
            else if (ch == keycode_Down || ch == keycode_Tab || ch == keycode_End || ch == keycode_PageDown)
                ch = keycode_Return;
            else if (ch == keycode_Left || ch == keycode_Erase)
                ch = keycode_Delete;
            else if (ch == keycode_Right)
                ch = ' ';
            else if (ch == keycode_Escape || (ch >= keycode_Func12 && ch <= keycode_Func1))
                ch = keycode_Unknown;

            //For speaking input
            _lastKeyPress = [NSString stringWithCharacters:(unichar *)&ch length:1];
            _keyPressTimeStamp = [NSDate date];

            self.currentReverseVideo = NO;
            [self putString:@" " style:style_Normal];
            xpos--;
        }

        GlkEvent *gev = [[GlkEvent alloc] initCharEvent:ch forWindow:self.name];
        [glkctl queueEvent:gev];
        char_request = NO;
        dirty = YES;
        return;
    }

    if (line_request) {
        // Did player type enter or line terminator?
        if (ch == keycode_Return || [self.currentTerminators[@(ch)] isEqual:@(YES)]) {
            terminator = [self.currentTerminators[@(ch)] isEqual:@(YES)] ? ch : 0;

            if (glkctl.gameID == kGameIsBeyondZork || [glkctl zVersion6]) {
                if (terminator == keycode_Home) {
                    NSLog(@"Gridwin keyDown changed keycode_Home to keycode_Up");
                    terminator = keycode_Up;
                } else if (terminator == keycode_End) {
                    NSLog(@"Gridwin keyDown changed keycode_End to keycode_Down");
                    terminator = keycode_Down;
                }
            }

            [self.window makeFirstResponder:nil];
            if (ch != keycode_Return)
                [self typedEnter:nil];
            // Or ar we here because the input field lost focus?
            // (When the input field has focus, key events won't be sent here)
            // (unless it passes along line termination events)
        } else if (ch != keycode_Unknown && line_request) {
            if (!self.input)
                [self initLine:_enteredTextSoFar maxLength:maxInputLength];
            [self.window makeFirstResponder:self.input];

            if (ch == keycode_Up) {
                [self travelBackwardInHistory];
                return;
            }

            if (ch == keycode_Down) {
                [self travelForwardInHistory];
                return;
            }

            NSTextView* textField = (NSTextView*)self.input.fieldEditor;
            [textField keyDown:evt];
        }
    }
}

- (void)sendKeypress:(unsigned)ch {
    GlkEvent *gev = [[GlkEvent alloc] initCharEvent:ch forWindow:self.name];
    [self.glkctl queueEvent:gev];
    char_request = NO;
    _textview.editable = NO;
}

#pragma mark Line input

- (void)initLine:(NSString *)str maxLength:(NSUInteger)maxLength  {
    if (self.terminatorsPending) {
        self.currentTerminators = self.pendingTerminators;
        self.terminatorsPending = NO;
    }

    terminator = 0;

    if (xpos > cols || maxLength == 0) {
        return;
    }

    if (xpos + maxLength > cols + 1)
        maxLength = cols + 1 - xpos;

    line_request = YES;
    [history reset];
    maxInputLength = maxLength;

    NSRect bounds = self.bounds;
    NSInteger mx = (NSInteger)_textview.textContainerInset.width;
    NSInteger my = (NSInteger)_textview.textContainerInset.height;

    NSInteger x0 = (NSInteger)(NSMinX(bounds) + mx + container.lineFragmentPadding / 2);
    NSInteger y0 = (NSInteger)(NSMinY(bounds) + my);
    CGFloat lineHeight = self.theme.cellHeight;
    CGFloat charWidth = self.theme.cellWidth;

    if (cols == 0)
        cols = 1;

    if (ypos >= _bufferTextStorage.length / cols)
        ypos = _bufferTextStorage.length / cols - 1;

    NSRect caret;
    caret.origin.x = x0 + xpos * charWidth;
    caret.origin.y = y0 + ypos * lineHeight;
    caret.size.width = maxLength * charWidth + container.lineFragmentPadding;
    caret.size.height = lineHeight;

    self.input = [[InputTextField alloc] initWithFrame:caret maxLength:maxLength];
    self.input.action = @selector(typedEnter:);
    self.input.target = self;
    self.input.delegate = self;
    self.input.font = self.theme.gridInput.font;
    NSDictionary *firstCharDict = [_bufferTextStorage attributesAtIndex:0 effectiveRange:nil];
    self.input.textColor = firstCharDict[NSForegroundColorAttributeName];
    if (currentZColor && self.theme.doStyles)
        self.input.textColor = [NSColor colorFromInteger:currentZColor.fg];

    _enteredTextSoFar = str;

    NSAttributedString *attString = [[NSAttributedString alloc]
                                     initWithString:str
                                     attributes:firstCharDict];

    self.input.attributedStringValue = attString;

    MyTextFormatter *inputFormatter =
    [[MyTextFormatter alloc] initWithMaxLength:maxLength];
    self.input.formatter = inputFormatter;

    [_textview addSubview:self.input];

    [self performSelector:@selector(deferredGrabFocus:) withObject:str afterDelay:0.1];
}

- (void)deferredGrabFocus:(id)str {
    if (self.input) {
        [self.window makeFirstResponder:self.input];
    }
}

- (NSString *)cancelLine {
    line_request = NO;
    if (self.input) {
        NSString *str = self.input.stringValue;
        [self printToWindow:str style:style_Input];
        [self moveToColumn:0 row:ypos + 1];
        [self.input removeFromSuperview];
        self.input = nil;
        return str;
    }
    return @"";
}

- (void)typedEnter:(id)sender {
    line_request = NO;
    if (self.input) {
        [self.glkctl markLastSeen];

        NSString *str = self.input.stringValue;

        [self printToWindow:str style:style_Input];
        [self moveToColumn:0 row:ypos + 1];

        if (str.length > 0) {
            [history saveHistory:str];
        }

        str = [str scrubInvalidCharacters];

        GlkEvent *gev = [[GlkEvent alloc] initLineEvent:str
                                              forWindow:self.name terminator:terminator];
        [self.glkctl queueEvent:gev];
        [self.input removeFromSuperview];
        self.input = nil;
    }
}

- (void)sendCommandLine:(NSString *)command {
    self.input.stringValue = command;
    [self typedEnter:nil];
}


- (void)controlTextDidChange:(NSNotification *)obj {
    NSDictionary *dict = obj.userInfo;
    MyFieldEditor *fieldEditor = dict[@"NSFieldEditor"];
    _enteredTextSoFar = [fieldEditor.string copy];
}

- (void)deferredInitLine:(id)sender {
    NSArray *subviews = _textview.subviews;
    if (subviews) {
        for (NSView *view in subviews) {
            if ([view isKindOfClass:[NSTextField class]]) {
                self.input = (InputTextField *)view;
                [self.input.fieldEditor removeFromSuperview];
                [self.input removeFromSuperview];
                self.input = nil;
            }
        }
    }
    [self initLine:(NSString *)sender maxLength:maxInputLength];
}

- (NSUInteger)unputString:(NSString *)buf {

    NSUInteger endpos = self.indexOfPos;
    NSUInteger startpos = endpos - buf.length;

    if (endpos > _bufferTextStorage.length || startpos > endpos) {
        return 0;
    }

    NSString *stringToRemove = [_bufferTextStorage.string substringWithRange:NSMakeRange(startpos, buf.length)].uppercaseString;

    if ([stringToRemove isEqualToString:buf.uppercaseString]) {
        NSString *spaces = [[[NSString alloc] init]
                            stringByPaddingToLength:buf.length
                            withString:@" "
                            startingAtIndex:0];
        xpos -= buf.length;
        [self putString:spaces style:style_Normal];
        xpos -= buf.length;
    } else {
        NSLog(@"GlkTextGridWindow unputString: string \"%@\" not found!", buf);
        return 0;
    }
    return buf.length;
}

- (BOOL)hasLineRequest {
    return line_request;
}

#pragma mark Command history

- (void)travelBackwardInHistory {
    if (!self.input)
        return;
    NSString *cx;
    if (self.input.stringValue.length > 0)
        cx = self.input.stringValue;
    else
        cx = @"";
    cx = [history travelBackwardInHistory:cx];
    if (cx) {
        self.input.stringValue = cx;
        self.input.fieldEditor.selectedRange = NSMakeRange(cx.length, 0);
    }

    if (!cx.length) {
        if ([history empty])
            [self.glkctl speakStringNow:@"No commands entered"];
        else
            [self.glkctl speakStringNow:@"Start of command history"];
    }
}

- (void)travelForwardInHistory {
    if (!self.input)
        return;
    NSString *cx = [history travelForwardInHistory];
    if (cx) {
        self.input.stringValue = cx;
        self.input.fieldEditor.selectedRange = NSMakeRange(cx.length, 0);
    }

    if (!cx.length) {
        if ([history empty])
            [self.glkctl speakStringNow:@"No commands entered"];
        else
            [self.glkctl speakStringNow:@"Start of command history"];
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

#pragma mark Quote box

- (void)quotebox:(NSUInteger)linesToSkip {
    NSUInteger charactersToSkip = (linesToSkip + 1) * (cols + 1);
    NSUInteger __block changes = 0;
    NSUInteger __block width;
    NSUInteger __block height = 0;
    NSAttributedString __block *blockTextStorage = _bufferTextStorage;
    if (blockTextStorage.length < charactersToSkip)
        blockTextStorage = textstorage;
    if (blockTextStorage.length < charactersToSkip)
        return;

    NSRange quoteBoxRange = NSMakeRange(charactersToSkip, blockTextStorage.length - charactersToSkip);

    NSMutableAttributedString __block *quoteAttStr = [[NSMutableAttributedString alloc] init];

    [blockTextStorage
     enumerateAttribute:NSBackgroundColorAttributeName
     inRange:quoteBoxRange
     options:0
     usingBlock:^(id value, NSRange range, BOOL *stop) {
        changes++;
        // Add string between every other background change
        if ((CGFloat)changes / 2 == changes / 2) {
            [quoteAttStr appendAttributedString:[blockTextStorage attributedSubstringFromRange:range]];
            NSAttributedString *newline = [[NSAttributedString alloc] initWithString:@"\n"];
            [quoteAttStr appendAttributedString:newline];
            height++;
            width = range.length;
        }
    }];

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

        GlkTextGridWindow *box = [[GlkTextGridWindow alloc] initWithGlkController:glkctl name:-1];
        box.quoteboxSize = NSMakeSize(width, height);
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

- (void)quoteboxAdjustSize:(id)sender {

    GlkController *glkctl = self.glkctl;
    NSScrollView *quoteboxParent = _quoteboxParent;

    if (quoteboxParent == nil) {
        return;
    }
    NSTextView *textView = quoteboxParent.documentView;
    GlkTextBufferWindow *bufWin = (GlkTextBufferWindow *)textView.delegate;
    NSSize boxSize = NSMakeSize(ceil(self.theme.gridMarginX * 2 + (_quoteboxSize.width + 1) * self.theme.cellWidth), ceil(self.theme.gridMarginY * 2 + _quoteboxSize.height * self.theme.cellHeight));

    NSRect frame = self.frame;
    frame.size = boxSize;
    frame.origin.x = ceil((bufWin.frame.size.width - boxSize.width) / 2) - self.theme.cellWidth * (2 * (glkctl.gameID != kGameIsTrinity && self.theme.cellWidth == self.theme.bufferCellWidth) );
    frame.origin.y = ceil(quoteboxParent.contentView.frame.origin.y +
                          (_quoteboxVerticalOffset + 2 * (glkctl.gameID == kGameIsCurses)) * self.theme.cellHeight);

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
    self.bufferTextStorage = nil;
    [self flushDisplay];

    [quoteboxParent addFloatingSubview:self forAxis:NSEventGestureAxisVertical];

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = 0.5;
        self.animator.alphaValue = 1;
    } completionHandler:^{
        self.alphaValue = 1;
    }];
}

#pragma mark Accessibility

- (void)speakStatus {
    GlkController *glkctl = self.glkctl;
    if (glkctl.zmenu)
        [NSObject cancelPreviousPerformRequestsWithTarget:glkctl.zmenu];
    if (glkctl.form)
        [NSObject cancelPreviousPerformRequestsWithTarget:glkctl.form];
    [glkctl speakStringNow:textstorage.string];
}

- (BOOL)setLastMove {
    BOOL hadNewText = _hasNewText;
    _hasNewText = NO;
    return hadNewText;
}

- (void)repeatLastMove:(id)sender {
    if (!self.moveRanges || !self.moveRanges.count) {
        [self speakStatus];
        return;
    }

    moveRangeIndex = self.moveRanges.count - 1;
    NSString *str = [textstorage.string substringWithRangeValue:self.moveRanges.lastObject];

    if (!str.length && sender != self.glkctl) {
        [self.glkctl speakStringNow:@"No last move to speak"];
        return;
    }

    [self.glkctl speakStringNow:str];
}

- (void)speakPrevious {
    if (!self.moveRanges.count)
        return;
    NSString *prefix = @"";
    if (moveRangeIndex > 0) {
        moveRangeIndex--;
    } else {
        prefix = @"At first move.\n";
        moveRangeIndex = 0;
    }
    NSString *str = [prefix stringByAppendingString:[textstorage.string substringWithRangeValue:self.moveRanges[moveRangeIndex]]];
    [self.glkctl speakStringNow:str];
}

- (void)speakNext {
    if (!self.moveRanges.count)
    {
        return;
    }

    NSString *prefix = @"";

    if (moveRangeIndex < self.moveRanges.count - 1) {
        moveRangeIndex++;
    } else {
        prefix = @"At last move.\n";
        moveRangeIndex = self.moveRanges.count - 1;
    }

    NSString *str = [prefix stringByAppendingString:[textstorage.string substringWithRangeValue:self.moveRanges[moveRangeIndex]]];
    [self.glkctl speakStringNow:str];
}

- (NSArray *)links {
    NSMutableArray<NSValue *> __block *links = [[NSMutableArray alloc] init];
    [textstorage
     enumerateAttribute:NSLinkAttributeName
     inRange:NSMakeRange(0, textstorage.length)
     options:0
     usingBlock:^(id value, NSRange range, BOOL *stop) {
         if (!value) {
             return;
         }
         [links addObject:[NSValue valueWithRange:range]];
     }];
    return links;
}

@end
