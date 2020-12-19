
/*
 * GlkTextGridWindow
 */

#import "InputTextField.h"
#import "InputHistory.h"

#import "Compatibility.h"
#import "NSString+Categories.h"
#import "Theme.h"
#import "GlkStyle.h"
#import "main.h"
#import "Game.h"
#import "Metadata.h"
#import "ZColor.h"
#import "ZMenu.h"

#import "NSColor+integer.h"

#include "glkimp.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
    fprintf(stderr, "%s\n",                                                    \
            [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif


/*
 * Extend NSTextView to ...
 *   - call keyDown and mouseDown on our GlkTextGridWindow object
 */

@implementation MyGridTextView

- (void)keyDown:(NSEvent *)theEvent {
    [(GlkTextGridWindow *)self.delegate keyDown:theEvent];
}

- (void)mouseDown:(NSEvent *)theEvent {
    if (![(GlkTextGridWindow *)self.delegate myMouseDown:theEvent])
        [super mouseDown:theEvent];
}

- (NSArray *)accessibilityCustomRotors  {
   return [((GlkTextGridWindow *)self.delegate).glkctl createCustomRotors];
}

 - (NSArray *)accessibilityChildren {
    NSArray *children = [super accessibilityChildren];
    InputTextField *input = ((GlkTextGridWindow *)self.delegate).input;
    if (input) {
        MyFieldEditor *fieldEditor = (((GlkTextGridWindow *)self.delegate).input.fieldEditor);
        if (fieldEditor) {
            if ([children indexOfObject:fieldEditor] == NSNotFound)
                children = [children arrayByAddingObject:fieldEditor];
        }
    }
    return children;
}

- (NSArray *)accessibilityCustomActions API_AVAILABLE(macos(10.13)) {
    GlkTextGridWindow *delegate = (GlkTextGridWindow *)self.delegate;
    NSArray *actions = [delegate.glkctl accessibilityCustomActions];
    return actions;
}

@end


@implementation GlkTextGridWindow

- (instancetype)initWithGlkController:(GlkController *)glkctl_
                                 name:(NSInteger)name_ {
    self = [super initWithGlkController:glkctl_ name:name_];

    if (self) {
        // lines = [[NSMutableArray alloc] init];
        
        NSDictionary *styleDict = nil;

        self.styleHints = [self deepCopyOfStyleHintsArray:self.glkctl.gridStyleHints];

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

        _textview = [[MyGridTextView alloc] initWithFrame:NSMakeRect(0, 0, 0, 0)
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
        _textview.linkTextAttributes = linkAttributes;

        _textview.editable = NO;
        _textview.usesFontPanel = NO;
        _restoredSelection = NSMakeRange(0, 0);

        history = [[InputHistory alloc] init];

        [self addSubview:scrollview];
        [self recalcBackground];

        if (self.glkctl.usesFont3)
            [self createBeyondZorkStyle];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        _textview = [decoder decodeObjectForKey:@"_textview"];

        layoutmanager = _textview.layoutManager;
        textstorage = _textview.textStorage;
        if (!textstorage)
            NSLog(@"Error! textstorage is nil!");
        _bufferTextStorage = [textstorage mutableCopy];
        container = (MarginContainer *)_textview.textContainer;

        _textview.delegate = self;
        _textview.insertionPointColor = self.theme.gridBackground;
        textstorage.delegate = self;
        scrollview = _textview.enclosingScrollView;
        scrollview.documentView = _textview;
        scrollview.accessibilityElement = NO;

        line_request = [decoder decodeBoolForKey:@"line_request"];
        hyper_request = [decoder decodeBoolForKey:@"hyper_request"];
        mouse_request = [decoder decodeBoolForKey:@"mouse_request"];

        rows = (NSUInteger)[decoder decodeIntegerForKey:@"rows"];
        cols = (NSUInteger)[decoder decodeIntegerForKey:@"cols"];
        xpos = (NSUInteger)[decoder decodeIntegerForKey:@"xpos"];
        ypos = (NSUInteger)[decoder decodeIntegerForKey:@"ypos"];

        history = [decoder decodeObjectForKey:@"history"];

        _selectedRow = (NSUInteger)[decoder decodeIntegerForKey:@"selectedRow"];
        _selectedCol = (NSUInteger)[decoder decodeIntegerForKey:@"selectedCol"];
        _selectedString = [decoder decodeObjectForKey:@"selectedString"];

        dirty = YES;
        transparent = [decoder decodeBoolForKey:@"transparent"];

        _restoredSelection =
        ((NSValue *)[decoder decodeObjectForKey:@"selectedRange"])
        .rangeValue;

        _pendingBackgroundCol = [decoder decodeObjectForKey:@"pendingBackgroundCol"];
        _bufferTextStorage = [decoder decodeObjectForKey:@"bufferTextStorage"];

        _enteredTextSoFar = [decoder decodeObjectForKey:@"inputString"];
        maxInputLength = (NSUInteger)[decoder decodeIntForKey:@"maxInputLength"];
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
    [encoder encodeObject:history forKey:@"history"];
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
    dirty = YES;
}

- (void)postRestoreAdjustments:(GlkWindow *)win {

    GlkTextGridWindow *restoredWin = (GlkTextGridWindow *)win;

    if (restoredWin.framePending)
        self.frame = restoredWin.pendingFrame;
    else
        self.frame = restoredWin.frame;

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
        GlkTextGridWindow * __unsafe_unretained weakSelf = self;
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf performSelector:@selector(deferredInitLine:) withObject:weakSelf.enteredTextSoFar afterDelay:0.5];
        });
    }
}

#pragma mark Colors and styles

- (void)prefsDidChange {
    //    NSLog(@"GlkTextGridWindow %ld prefsDidChange", self.name);

    NSDictionary *attributes;
    NSRange selectedRange = _textview.selectedRange;

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

        if (_usingStyles != self.theme.doStyles) {
            different = YES;
            _usingStyles = self.theme.doStyles;
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
        if (self.glkctl.usesFont3)
            [self createBeyondZorkStyle];

        NSUInteger textstoragelength = textstorage.length;

        /* reassign styles to attributedstrings */
        // We create a copy of the text storage
        _bufferTextStorage = [textstorage mutableCopy];

        GlkTextGridWindow * __unsafe_unretained weakSelf = self;

        __block NSArray *blockStyles = styles;

        [textstorage
         enumerateAttributesInRange:NSMakeRange(0, textstoragelength)
         options:0
         usingBlock:^(NSDictionary *attrs, NSRange range, BOOL *stop) {

            // First, we overwrite all attributes with those in our updated
            // styles array
            id styleobject = attrs[@"GlkStyle"];
            if (styleobject) {
                NSDictionary *blockattributes = blockStyles[(NSUInteger)[styleobject intValue]];
                [weakSelf.bufferTextStorage setAttributes:blockattributes range:range];
            } else NSLog(@"No GlkStyle for range %@!", NSStringFromRange(range));

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

        [_bufferTextStorage addAttribute:NSCursorAttributeName value:[NSCursor arrowCursor] range:NSMakeRange(0, _bufferTextStorage.length)];

        // Now we can replace the text storager
        [textstorage setAttributedString:_bufferTextStorage];

        NSMutableDictionary *linkAttributes = [_textview.linkTextAttributes mutableCopy];
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

        if (!self.glkctl.isAlive) {
            NSRect frame = self.frame;

            if ((self.autoresizingMask & NSViewWidthSizable) == NSViewWidthSizable) {
                frame.size.width = self.glkctl.contentView.frame.size.width - frame.origin.x;
            }

            if ((self.autoresizingMask & NSViewHeightSizable) == NSViewHeightSizable) {
                frame.size.height = self.glkctl.contentView.frame.size.height - frame.origin.y;
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

    if (!([self.glkctl.game.detectedFormat isEqualToString:@"glulx"] || [self.glkctl.game.detectedFormat isEqualToString:@"hugo"] || [self.glkctl.game.detectedFormat isEqualToString:@"zcode"])) {
        bgcolor = styles[style_User1][NSBackgroundColorAttributeName];
    }

    if (self.theme.doStyles && bgnd > -1) {
        bgcolor = [NSColor colorFromInteger:bgnd];
    }

    if (!bgcolor)
        bgcolor = self.theme.gridBackground;

    if (transparent)
        bgcolor = [NSColor clearColor];
    else
        [self.glkctl setBorderColor:bgcolor fromWindow:self];

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
    // charaters at the end of every row, at those may not have the right background color. So we have
    // to check on a row by row basis.

    if (!_bufferTextStorage.length) {
        if (textstorage.length) {
            _bufferTextStorage = textstorage;
        } else return;
    }

    __block NSColor *bgCol;
    __block NSUInteger blocCols = cols;
    __block NSAttributedString *blockTextStorage = _bufferTextStorage;

    [blockTextStorage
     enumerateAttribute:NSBackgroundColorAttributeName
     inRange:NSMakeRange(0, blockTextStorage.length)
     options:0
     usingBlock:^(id value, NSRange range, BOOL *stop) {

        bgCol = (NSColor *)value;
        if (bgCol && range.length + 2 > blocCols) {
            if (NSMaxRange(range) > blockTextStorage.length - 3)
                *stop = YES;
        } else {
            bgCol = nil;
        }
    }];

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
    _textview.editable = YES;
    NSRange selectedRange = _textview.selectedRange;
    NSString *selectedString = [textstorage.string substringWithRange:selectedRange];
    _selectedRow = selectedRange.location / (cols + 1);
    _selectedCol = selectedRange.location % (cols + 1);

    if (self.framePending) {
        if (!NSEqualRects(self.frame, self.pendingFrame)) {
            [super setFrame:self.pendingFrame];
        }
        if (!NSEqualRects(_textview.frame, self.frame)) {
            _textview.frame = self.frame;
        }

        self.framePending = NO;
    }

    if (!transparent)
        [self checkForUglyBorder];

    if (_pendingBackgroundCol && ![_pendingBackgroundCol isEqualToColor:_textview.backgroundColor]) {
        _textview.backgroundColor = _pendingBackgroundCol;
    }
    _pendingBackgroundCol = nil;

    if (_bufferTextStorage)
        [textstorage setAttributedString:_bufferTextStorage];
    else {
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

    [super flushDisplay];
    _textview.editable = NO;
}

#pragma mark Printing, moving, resizing

- (void)setFrame:(NSRect)frame {
    if (self.glkctl.ignoreResizes)
        return;

    NSUInteger r;

    self.framePending = YES;
    self.pendingFrame = frame;

    if (frame.size.height == 0) {
        return;
    }

    NSSize screensize = self.glkctl.window.screen.visibleFrame.size;
    if (frame.size.height > screensize.height)
        frame.size.height = self.glkctl.contentView.frame.size.height;

    NSUInteger newcols = (NSUInteger)round((frame.size.width -
                                            (_textview.textContainerInset.width + container.lineFragmentPadding) * 2) /
                                           self.theme.cellWidth);

    NSUInteger newrows = (NSUInteger)round((frame.size.height + self.theme.gridNormal.lineSpacing // We cut off the lowest linespaceing gap in order to center text vertically
                                            - (_textview.textContainerInset.height * 2) ) /
                                           self.theme.cellHeight);

    if ((NSInteger)newcols < 1)
        newcols = 1;
    if ((NSInteger)newrows < 1)
        newrows = 1;

    if (self.inLiveResize && newcols < cols)
        return;

    if (_restoredSelection.length == 0)
        _restoredSelection = _textview.selectedRange;

    _selectedRow = _restoredSelection.location / (cols + 1);
    _selectedCol = _restoredSelection.location % (cols + 1);

    if (newcols * self.theme.cellWidth > screensize.width || newrows * self.theme.cellHeight > screensize.height) {
        NSLog(@"GlkTextGridWindow setFrame error! newcols (%ld) * theme.cellwith (%f) = %f. screensize.width:%f newrows (%ld) * theme.cellheight (%f) = %f. screensize.height:%f Returning.", newcols, self.theme.cellWidth, newcols * self.theme.cellWidth, screensize.width, newrows, self.theme.cellHeight, newrows * self.theme.cellHeight, screensize.height);
        return;
    }

    NSMutableDictionary *attrDict = [styles[style_Normal] mutableCopy];

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
    for (r = cols; r < _bufferTextStorage.length; r += cols + 1)
        [_bufferTextStorage replaceCharactersInRange:NSMakeRange(r, 1)
                                withAttributedString:newlinestring];

    _restoredSelection = NSMakeRange(_selectedCol + _selectedRow * (cols + 1), _restoredSelection.length);


    if ([self inLiveResize]) {
        [super setFrame:frame];
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
    xpos = c;
    ypos = r;
}

- (void)clear {
    NSRange selectedRange = _textview.selectedRange;
    if (!_bufferTextStorage.length)
        return;
    _bufferTextStorage = [[NSMutableAttributedString alloc]
                          initWithString:@""];

    rows = cols = xpos = ypos = 0;

    // Re-fill with spaces
    if (self.framePending) {
        [self setFrame:self.pendingFrame];
    } else
        [self setFrame:self.frame];

//    if (currentZColor && bgnd != currentZColor.bg) {
//        if (currentZColor.bg != zcolor_Current && currentZColor.bg != zcolor_Default) {
//            bgnd = currentZColor.bg;
//        }
//        [self recalcBackground];
//    }

    if (NSMaxRange(selectedRange) > _textview.textStorage.length) {
        if (_textview.textStorage.length)
            selectedRange = NSMakeRange(_textview.textStorage.length - 1, 0);
        else
            selectedRange = NSMakeRange(0, 0);
    }
    _textview.selectedRange = selectedRange;
}

- (void)putString:(NSString *)string style:(NSUInteger)stylevalue {
    if (line_request)
        NSLog(@"Printing to text grid window during line request");

    //    if (char_request)
    //        NSLog(@"Printing to text grid window during character request");

    [self printToWindow:string style:stylevalue];
}

- (void)printToWindow:(NSString *)string style:(NSUInteger)stylevalue {
    NSUInteger length = string.length;
    NSUInteger startpos;
    NSUInteger pos = 0;

    NSUInteger textstoragelength = _bufferTextStorage.length;

    if (textstoragelength == 0) {
        if (textstorage.length) {
            _bufferTextStorage = [textstorage mutableCopy];
            textstoragelength = textstorage.length;
        } else return;
    }

    if (cols == 0 || rows == 0 || length == 0)
        return;

    // With certain fonts and sizes, strings containing only spaces will "collapse."
    // So if the first character is a space, we replace it with a &nbsp;
    if ([string hasPrefix:@" "]) {
        const unichar nbsp = 0xa0;
        NSString *nbspstring = [NSString stringWithCharacters:&nbsp length:1];
        string = [string stringByReplacingCharactersInRange:NSMakeRange(0, 1) withString:nbspstring];
    }

    if (xpos > cols) {
        ypos += (xpos / cols);
        xpos = (xpos % cols);
    }
    NSMutableDictionary *attrDict = [styles[stylevalue] mutableCopy];

    if (!attrDict)
        NSLog(@"GlkTextGridWindow printToWindow: ERROR! Style dictionary nil!");

    startpos = ypos * (cols + 1) + xpos;
    if (startpos > textstoragelength) {
        // We are outside window visible range!
        // Do nothing
        NSLog(@"Printed outside grid window visible range!");
        return;
    }

    if (currentZColor) {
        attrDict[@"ZColor"] = currentZColor;
        if (self.theme.doStyles) {
            if ([self.styleHints[stylevalue][stylehint_ReverseColor] isEqualTo:@(1)]) {
                attrDict = [currentZColor reversedAttributes:attrDict];
                //  If the style has the reverseColor hint set, we apply the zcolors in reverse
            } else {
                attrDict = [currentZColor coloredAttributes:attrDict];
                // Otherwise we apply the zcolors normally");
            }
        }
    }

    if (self.currentReverseVideo) {
        attrDict[@"ReverseVideo"] = @(YES);
        if (!self.theme.doStyles || [self.styleHints[stylevalue][stylehint_ReverseColor] isNotEqualTo:@(1)]) {
            // If the current colours are not already reversed by stylehint_ReverseColor,
            // we reverse the colours here
            attrDict = [self reversedAttributes:attrDict background:self.theme.gridBackground];
        }
    }

    if (self.currentHyperlink) {
        attrDict[NSLinkAttributeName] = @(self.currentHyperlink);
    }

    if (ypos > rows) {
        NSLog(@"printToWindow: ypos outside visible range");
        return;
    }

    // Check for newlines in string to write
    NSUInteger x;
    for (x = 0; x < length; x++) {
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
    while (pos < length) {
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

        if ((cols + 1) * ypos + xpos + amountToDraw > textstoragelength)
            amountToDraw = textstoragelength - ((cols + 1) * ypos + xpos) + 1;

        if (amountToDraw < 1)
            break;

        NSRange replaceRange = NSMakeRange((cols + 1) * ypos + xpos, amountToDraw);
        if (NSMaxRange(replaceRange) > textstoragelength) {
            if ((cols + 1) * ypos + xpos > textstoragelength)
                return;
            else {
                NSUInteger diff = NSMaxRange(replaceRange) - textstoragelength;
                amountToDraw -= diff;
                if (!amountToDraw)
                    return;
                replaceRange = NSMakeRange((cols + 1) * ypos + xpos, amountToDraw);
            }
        }

        // "Draw" the characters
        NSAttributedString *partString = [[NSAttributedString alloc]
                                          initWithString:[string substringWithRange:
                                                          NSMakeRange(pos, amountToDraw)]
                                          attributes:attrDict];
        [_bufferTextStorage
         replaceCharactersInRange:replaceRange withAttributedString:partString];
        
        //        NSLog(@"Replaced characters in range %@ with '%@'",
        //        NSStringFromRange(replaceRange), partString.string);

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

#pragma mark Hyperlinks

- (void)initHyperlink {
    hyper_request = YES;
    // NSLog(@"txtgrid: hyperlink event requested");
}

- (void)cancelHyperlink {
    hyper_request = NO;
    NSLog(@"txtgrid: hyperlink event cancelled");
}

- (BOOL)textView:_textview clickedOnLink:(id)link atIndex:(NSUInteger)charIndex {
    NSLog(@"txtgrid: clicked on link: %@", link);
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
    //    NSLog(@"mousedown in grid window %ld", self.name);

    if (mouse_request) {
        [self.glkctl markLastSeen];

        NSPoint p;
        p = theEvent.locationInWindow;

        p = [_textview convertPoint:p fromView:nil];

        p.x -= _textview.textContainerInset.width;
        p.y -= _textview.textContainerInset.height;

        NSUInteger charIndex =
        [_textview.layoutManager characterIndexForPoint:p
                                       inTextContainer:container
              fractionOfDistanceBetweenInsertionPoints:nil];
        // NSLog(@"Clicked on char index %ld, which is '%@'.", charIndex,
        // [textstorage.string substringWithRange:NSMakeRange(charIndex, 1)]);

        p.y = charIndex / (cols + 1);
        p.x = charIndex % (cols + 1);

        // NSLog(@"p.x: %f p.y: %f", p.x, p.y);

        if (p.x >= 0 && p.y >= 0 && p.x < cols && p.y < rows) {
            if (mouse_request) {
                gev = [[GlkEvent alloc] initMouseEvent:p forWindow:self.name];
                [self.glkctl queueEvent:gev];
                mouse_request = NO;
                return YES;
            }
        }
    } else {
        //                NSLog(@"No hyperlink request or mouse request in grid window %ld", self.name);
        [super mouseDown:theEvent];
    }
    return NO;
}

#pragma mark Single key input

- (void)initChar {
    // NSLog(@"init char in %ld", (long)self.name);
    char_request = YES;
    dirty = YES;

    // Draw Buraucracy intro form cursor
    if (self.glkctl.bureaucracy) {
        self.currentReverseVideo = YES;
        [self putString:@" " style:style_Normal];
        self.currentReverseVideo = NO;
        xpos--;
    }
}

- (void)cancelChar {
    // NSLog(@"cancel char in %ld", self.name);
    char_request = NO;
    dirty = YES;
}

- (void)keyDown:(NSEvent *)evt {
    NSString *str = evt.characters;
    unsigned ch = keycode_Unknown;
    if (str.length)
        ch = chartokeycode([str characterAtIndex:str.length-1]);

    NSUInteger flags = evt.modifierFlags;

    if ((flags & NSEventModifierFlagNumericPad) && !self.glkctl.bureaucracy && ch >= '0' && ch <= '9')
        ch = keycode_Pad0 - (ch - '0');

    GlkWindow *win;
    // pass on this key press to another GlkWindow if we are not expecting one
    if (!self.wantsFocus)
        for (win in [self.glkctl.gwindows allValues]) {
            if (win != self && win.wantsFocus) {
                [win grabFocus];
                NSLog(@"GlkTextGridWindow: Passing on keypress");
                [win keyDown:evt];
                return;
            }
        }

    if (char_request && ch != keycode_Unknown) {
        [self.glkctl markLastSeen];

        if (self.glkctl.bureaucracy) {
            // Bureacracy on Bocfel will try to convert these keycodes
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
            self.currentReverseVideo = NO;
            [self putString:@" " style:style_Normal];
            xpos--;
        }

        // NSLog(@"char event from %ld", self.name);
        GlkEvent *gev = [[GlkEvent alloc] initCharEvent:ch forWindow:self.name];
        [self.glkctl queueEvent:gev];
        char_request = NO;
        dirty = YES;
        return;
    }

    if (line_request) {
        // Did player type enter or line terminator?
        if (ch == keycode_Return || [self.currentTerminators[@(ch)] isEqual:@(YES)]) {
            terminator = [self.currentTerminators[@(ch)] isEqual:@(YES)] ? ch : 0;
            [self.window makeFirstResponder:nil];
            if (ch != keycode_Return)
                [self typedEnter:nil];
            // Or ar we here because the input field lost focus?
            // (When the input field has focus, key events won't be sent here)
            // (unless it passes on line termination events)
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

    NSTrackingArea *trackingArea = [[NSTrackingArea alloc] initWithRect:caret options: (NSTrackingActiveAlways | NSTrackingMouseEnteredAndExited) owner:self.input userInfo:nil];
    [self.input addTrackingArea:trackingArea];

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

- (void)unputString:(NSString *)buf {

    NSUInteger endpos = ypos * (cols + 1) + xpos;
    NSUInteger startpos = endpos - buf.length;

    if (endpos > _bufferTextStorage.length || startpos > endpos) {
        return;
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
    }
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
}

- (void)travelForwardInHistory {
    if (!self.input)
        return;
    NSString *cx = [history travelForwardInHistory];
    if (cx) {
        self.input.stringValue = cx;
        self.input.fieldEditor.selectedRange = NSMakeRange(cx.length, 0);
    }
}

#pragma mark ZColors

- (void)setZColorText:(NSInteger)fg background:(NSInteger)bg {

    if (currentZColor && !(currentZColor.fg == fg && currentZColor.bg == bg)) {
        // If there already was a previous active zcolor, we
        // deactivate it here, unless it uses the same colors as the new one.
        currentZColor = nil;
    }

    if (!currentZColor && !(fg == zcolor_Default && bg == zcolor_Default)) {
        currentZColor =
        [[ZColor alloc] initWithText:fg background:bg];
        //        NSLog(@"Started a new ZColor run";
    }

}

- (NSMutableAttributedString *)applyZColorsAndThenReverse:(NSMutableAttributedString *)attStr {
    NSUInteger textstoragelength = attStr.length;

    GlkTextGridWindow * __unsafe_unretained weakSelf = self;

    [attStr
     enumerateAttribute:@"ZColor"
     inRange:NSMakeRange(0, textstoragelength)
     options:0
     usingBlock:^(id value, NSRange range, BOOL *stop) {
         if (!value) {
             return;
         }
         ZColor *z = value;
         [attStr
          enumerateAttributesInRange:range
          options:0
          usingBlock:^(NSDictionary *dict, NSRange range2, BOOL *stop2) {
              NSUInteger stylevalue = (NSUInteger)((NSNumber *)dict[@"GlkStyle"]).integerValue;
              NSMutableDictionary *mutDict = [dict mutableCopy];
              if ([weakSelf.styleHints[stylevalue][stylehint_ReverseColor] isEqualTo:@(1)]) {
                  // Style has stylehint_ReverseColor set,
                  // so we apply Zcolor with reversed attributes
                  mutDict = [z reversedAttributes:mutDict];
              } else {
                  // Apply Zcolor normally
                  mutDict = [z coloredAttributes:mutDict];
              }
              [attStr addAttributes:mutDict range:range2];
          }];
     }];

    [attStr
     enumerateAttribute:@"ReverseVideo"
     inRange:NSMakeRange(0, textstoragelength)
     options:0
     usingBlock:^(id value, NSRange range, BOOL *stop) {
         if (!value) {
             return;
         }
         [attStr
          enumerateAttributesInRange:range
          options:0
          usingBlock:^(NSDictionary *dict, NSRange range2, BOOL *stop2) {
              NSUInteger stylevalue = (NSUInteger)((NSNumber *)dict[@"GlkStyle"]).integerValue;
              BOOL zcolorValue = (dict[@"ZColor"] != nil);
              // We only apply reversed attributes if they were not already set to reverse by the
              // ZColor check iteration above (because the style hint ReverseColor was active)
              if (!([weakSelf.styleHints[stylevalue][stylehint_ReverseColor] isEqualTo:@(1)] && !zcolorValue))  {
                  //NSLog(@"Applying reverse video at %@. ZColor at this range is %@.", NSStringFromRange(range), dict[@"ZColor"]);
                  NSMutableDictionary *mutDict = [dict mutableCopy];
                  mutDict = [weakSelf reversedAttributes:mutDict background:self.theme.gridBackground];
                  [attStr addAttributes:mutDict range:range2];
              }
          }];
     }];
    
    return attStr;
}

- (NSMutableAttributedString *)applyReverseOnly:(NSMutableAttributedString *)attStr {
    NSUInteger textstoragelength = attStr.length;

    GlkTextGridWindow * __unsafe_unretained weakSelf = self;

    [attStr
     enumerateAttribute:@"ReverseVideo"
     inRange:NSMakeRange(0, textstoragelength)
     options:0
     usingBlock:^(id value, NSRange range, BOOL *stop) {
         if (!value) {
             return;
         }
         [attStr
          enumerateAttributesInRange:range
          options:0
          usingBlock:^(NSDictionary *dict, NSRange range2, BOOL *stop2) {
                  NSMutableDictionary *mutDict = [dict mutableCopy];
                  mutDict = [weakSelf reversedAttributes:mutDict background:self.theme.gridBackground];
                  [attStr addAttributes:mutDict range:range2];
          }];
     }];

    return attStr;
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

    CGFloat desiredWidth = [self widthForPointSize:pointSize baseFont:self.theme.gridNormal.font sampleText:normalTextSample];
    zorkFont = [self fontToFitWidth:desiredWidth baseFont:zorkFont sampleText:zorkTextSample];
    // Then we switch off anything that may cause gaps in the default BlockQuote style font
    NSMutableDictionary *beyondZorkStyle = [styles[style_Normal] mutableCopy];
    NSString *normalFontName = self.theme.gridNormal.font.fontName;
    BOOL isMonaco = ([normalFontName isEqualToString:@"Monaco"]);
    
    beyondZorkStyle[@"GlkStyle"] = @(style_BlockQuote);

    NSMutableParagraphStyle *para = [beyondZorkStyle[NSParagraphStyleAttributeName] mutableCopy];
//    para.lineSpacing = 0;
//    para.paragraphSpacing = 0;
//    para.paragraphSpacingBefore = 0;

//    beyondZorkStyle[NSParagraphStyleAttributeName] = para;
    beyondZorkStyle[NSFontAttributeName] = zorkFont;

    // Create an NSAffineTransform that stretches the font to our cellHeight
    // This is where things usually go wrong, i.e. the result is too short or too tall
    NSAffineTransform *transform = [[NSAffineTransform alloc] init];
    [transform scaleBy:zorkFont.pointSize];
    CGFloat yscale = (self.theme.cellHeight + 0.5) / [zorkFont boundingRectForFont].size.height;
    if (isMonaco)
        yscale *= 1.1;
    [transform scaleXBy:1 yBy:yscale];

    zorkFont = [NSFont fontWithDescriptor:zorkFont.fontDescriptor textTransform:transform];

    if (!zorkFont)
        NSLog(@"Failed to create Zork Font!");

    beyondZorkStyle[NSFontAttributeName] = zorkFont;

    para.maximumLineHeight = self.theme.cellHeight;
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

// Find a font size to match a certain width in points.
// This is really only necessary on 10.7, which can't give exact character width for fonts,
// but I think it give slightly better results (less gaps) on recent systems as well.

- (NSFont *)fontToFitWidth:(CGFloat)desiredWidth baseFont:(NSFont *)font sampleText:(NSString *)text {
    {
        if (!text.length || desiredWidth < 2) {
            return font;
        }

        CGFloat guess;
        CGFloat guessWidth;

        guess = font.pointSize;
        if (guess > 1 && guess < 1000) { guess = 50; }

        guessWidth = [self widthForPointSize:guess baseFont:font sampleText:text];

        if (guessWidth == desiredWidth) {
            return [NSFont fontWithDescriptor:font.fontDescriptor size:guess];
        }

        NSInteger iterations = 4;

        while(iterations > 0) {
            guess = guess * ( desiredWidth / guessWidth );
            guessWidth = [self widthForPointSize:guess baseFont:font sampleText:text];

            if (guessWidth == desiredWidth)
            {
                return [NSFont fontWithDescriptor:font.fontDescriptor size:guess];
            }

            iterations -= 1;
        }
        return [NSFont fontWithDescriptor:font.fontDescriptor size:guess];
    }
}

- (CGFloat)widthForPointSize:(CGFloat)guess baseFont:(NSFont *)font sampleText:(NSString *)text {
    NSFont *newFont = [NSFont fontWithDescriptor:font.fontDescriptor size:guess];
    NSMutableDictionary *dic = [NSMutableDictionary dictionary];
    [dic setObject:newFont forKey:NSFontAttributeName];
    CGFloat textWidth = [text sizeWithAttributes:dic].width;
    return textWidth;
}

#pragma mark Accessibility

- (BOOL)isAccessibilityElement {
    return NO;
}

- (void)speakStatus {
    if (self.glkctl.zmenu)
        [NSObject cancelPreviousPerformRequestsWithTarget:self.glkctl.zmenu];
    NSDictionary *announcementInfo = @{
        NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh),
        NSAccessibilityAnnouncementKey : textstorage.string
    };
    NSAccessibilityPostNotificationWithUserInfo(
                                                [NSApp mainWindow],
                                                NSAccessibilityAnnouncementRequestedNotification, announcementInfo);
}

- (BOOL)setLastMove {
    BOOL hadNewText = _hasNewText;
    _hasNewText = NO;
    return hadNewText;
}

- (void)speakMostRecent {
    [self speakStatus];
}

- (void)deferredSpeakMostRecent:(id)sender {
    [self speakMostRecent];
}

- (NSArray *)links {
    __block NSMutableArray *links = [[NSMutableArray alloc] init];
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
