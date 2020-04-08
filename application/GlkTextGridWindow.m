
/*
 * GlkTextGridWindow
 */

#import "Compatibility.h"
#import "GlkHyperlink.h"
#import "NSString+Categories.h"
#import "Theme.h"
#import "GlkStyle.h"
#import "main.h"
#import "Game.h"
#import "Metadata.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
    fprintf(stderr, "%s\n",                                                    \
            [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@interface MyGridTextView : NSTextView

- (void)superKeyDown:(NSEvent *)evt;

@end

/*
 * Extend NSTextView to ...
 *   - call onKeyDown and mouseDown on our GlkTextGridWindow object
 */

@implementation MyGridTextView

- (void)superKeyDown:(NSEvent *)evt {
    [super keyDown:evt];
}

- (void)keyDown:(NSEvent *)evt {
    [(GlkTextGridWindow *)self.delegate keyDown:evt];
}

- (void)mouseDown:(NSEvent *)theEvent {
    if (![(GlkTextGridWindow *)self.delegate myMouseDown:theEvent])
        [super mouseDown:theEvent];
}

@end

// Custom formatter adapted from code by Jonathan Mitchell
// See
// https://stackoverflow.com/questions/827014/how-to-limit-nstextfield-text-length-and-keep-it-always-upper-case
//
@interface MyTextFormatter : NSFormatter {
}

- (id)initWithMaxLength:(NSUInteger)alength;

@property NSUInteger maxLength;

@end

@implementation MyTextFormatter

- (id)init {
    if (self = [super init]) {
        self.maxLength = INT_MAX;
    }

    return self;
}

- (id)initWithMaxLength:(NSUInteger)alength {
    if (self = [super init]) {
        self.maxLength = alength;
    }

    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        _maxLength = (NSUInteger)[decoder decodeIntegerForKey:@"maxLength"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeInteger:(NSInteger)_maxLength forKey:@"maxLength"];
}

#pragma mark -
#pragma mark Textual Representation of Cell Content

- (NSString *)stringForObjectValue:(id)object {
    NSString *stringValue = nil;
    if ([object isKindOfClass:[NSString class]]) {

        // A new NSString is perhaps not required here
        // but generically a new object would be generated
        stringValue = [NSString stringWithString:object];
    }

    return stringValue;
}

- (BOOL)getObjectValue:(id __autoreleasing *)object
             forString:(NSString *)string
      errorDescription:(NSString * __autoreleasing *)error {
    BOOL valid = YES;

    *object = [NSString stringWithString:string];

    return valid;
}

- (BOOL)isPartialStringValid:(NSString * __autoreleasing *)partialStringPtr
       proposedSelectedRange:(NSRangePointer)proposedSelRangePtr
              originalString:(NSString *)origString
       originalSelectedRange:(NSRange)origSelRange
            errorDescription:(NSString * __autoreleasing *)error {
    BOOL valid = YES;

    NSString *proposedString = *partialStringPtr;
    if (proposedString.length > self.maxLength) {

        // The original string has been modified by one or more characters (via
        // pasting). Either way compute how much of the proposed string can be
        // accommodated.
        NSUInteger origLength = origString.length;
        NSUInteger insertLength = self.maxLength - origLength;

        // If a range is selected then characters in that range will be removed
        // so adjust the insert length accordingly
        insertLength += origSelRange.length;

        // Get the string components
        NSString *prefix = [origString substringToIndex:origSelRange.location];
        NSString *suffix = [origString
                            substringFromIndex:origSelRange.location + origSelRange.length];
        NSString *insert = [proposedString
                            substringWithRange:NSMakeRange(origSelRange.location,
                                                           insertLength)];

        // Assemble the final string
        *partialStringPtr =
        [NSString stringWithFormat:@"%@%@%@", prefix, insert, suffix];

        // Fix-up the proposed selection range
        proposedSelRangePtr->location = origSelRange.location + insertLength;
        proposedSelRangePtr->length = 0;

        valid = NO;
    }

    return valid;
}

@end

@implementation GlkTextGridWindow

- (instancetype)initWithGlkController:(GlkController *)glkctl_
                                 name:(NSInteger)name_ {
    self = [super initWithGlkController:glkctl_ name:name_];

    if (self) {
        // lines = [[NSMutableArray alloc] init];
        
        NSDictionary *styleDict = nil;

        self.styleHints = self.glkctl.gridStyleHints;
        styles = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
        for (NSUInteger i = 0; i < style_NUMSTYLES; i++) {

            if (self.theme.doStyles) {
                 styleDict = [((GlkStyle *)[self.theme valueForKey:gGridStyleNames[i]]) attributesWithHints:(self.styleHints)[i]];
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

        hyperlinks = [[NSMutableArray alloc] init];

        /* construct text system manually */

        scrollview = [[NSScrollView alloc] initWithFrame:NSZeroRect];
        scrollview.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        scrollview.hasHorizontalScroller = NO;
        scrollview.hasVerticalScroller = NO;
        scrollview.horizontalScrollElasticity = NSScrollElasticityNone;
        scrollview.verticalScrollElasticity = NSScrollElasticityNone;

        scrollview.borderType = NSNoBorder;

        textstorage = [[NSTextStorage alloc] init];

        layoutmanager = [[NSLayoutManager alloc] init];
        [textstorage addLayoutManager:layoutmanager];

        container = [[NSTextContainer alloc]
                     initWithContainerSize:NSMakeSize(10000000, 10000000)];

        container.layoutManager = layoutmanager;
        [layoutmanager addTextContainer:container];

        textview = [[MyGridTextView alloc] initWithFrame:NSMakeRect(0, 0, 0, 0)
                                           textContainer:container];

        textview.minSize = NSMakeSize(0, 0);
        textview.maxSize = NSMakeSize(10000000, 10000000);

        container.textView = textview;
        scrollview.documentView = textview;

        /* now configure the text stuff */

        textview.delegate = self;
        textstorage.delegate = self;
        textview.textContainerInset =
        NSMakeSize(self.theme.gridMarginX, self.theme.gridMarginY);

        NSMutableDictionary *linkAttributes = [textview.linkTextAttributes mutableCopy];
        linkAttributes[NSForegroundColorAttributeName] = styles[style_Normal][NSForegroundColorAttributeName];
        textview.linkTextAttributes = linkAttributes;

        textview.editable = NO;
        textview.usesFontPanel = NO;
        _restoredSelection = NSMakeRange(0, 0);

        [self addSubview:scrollview];
        [self recalcBackground];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        textview = [decoder decodeObjectForKey:@"textview"];

        layoutmanager = textview.layoutManager;
        textstorage = textview.textStorage;
        if (!textstorage)
            NSLog(@"Error! textstorage is nil!");
        container = (MarginContainer *)textview.textContainer;

        textview.delegate = self;
        textview.insertionPointColor = self.theme.gridBackground;
        textstorage.delegate = self;
        scrollview = textview.enclosingScrollView;
        scrollview.documentView = textview;

        line_request = [decoder decodeBoolForKey:@"line_request"];
        hyper_request = [decoder decodeBoolForKey:@"hyper_request"];
        mouse_request = [decoder decodeBoolForKey:@"mouse_request"];

        rows = (NSUInteger)[decoder decodeIntegerForKey:@"rows"];
        cols = (NSUInteger)[decoder decodeIntegerForKey:@"cols"];
        xpos = (NSUInteger)[decoder decodeIntegerForKey:@"xpos"];
        ypos = (NSUInteger)[decoder decodeIntegerForKey:@"ypos"];

        dirty = YES;
        transparent = [decoder decodeBoolForKey:@"transparent"];
        _restoredSelection =
        ((NSValue *)[decoder decodeObjectForKey:@"selectedRange"])
        .rangeValue;
        textview.selectedRange = _restoredSelection;
        if (line_request) {
            NSArray *subviews = textview.subviews;
            for (NSView *view in subviews) {
                if ([view isKindOfClass:[NSTextField class]]) {
                    input = (NSTextField *)view;
                    [input removeFromSuperview];
                    NSLog(@"Found old input textfield!");
                }
            }

            NSString *inputString = [decoder decodeObjectForKey:@"inputString"];

            dispatch_async(dispatch_get_main_queue(), ^{
                [self performSelector:@selector(deferredInitLine:) withObject:inputString afterDelay:0.5];
            });
        }
    }
    return self;
}

- (void)deferredInitLine:(id)sender {
    [self initLine:(NSString *)sender];
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeObject:textview forKey:@"textview"];
    [encoder encodeObject:input forKey:@"input"];
    [encoder encodeBool:line_request forKey:@"line_request"];
    [encoder encodeBool:hyper_request forKey:@"hyper_request"];
    [encoder encodeBool:mouse_request forKey:@"mouse_request"];
    [encoder encodeInteger:(NSInteger)rows forKey:@"rows"];
    [encoder encodeInteger:(NSInteger)cols forKey:@"cols"];
    [encoder encodeInteger:(NSInteger)xpos forKey:@"xpos"];
    [encoder encodeInteger:(NSInteger)ypos forKey:@"ypos"];
    [encoder encodeBool:transparent forKey:@"transparent"];
    NSValue *rangeVal = [NSValue valueWithRange:textview.selectedRange];
    [encoder encodeObject:rangeVal forKey:@"selectedRange"];
    if (fieldEditor) {
        [encoder encodeObject:fieldEditor.textStorage.string forKey:@"inputString"];
    }
}

- (BOOL)isFlipped {
    return YES;
}

- (void)prefsDidChange {
//    NSLog(@"GlkTextGridWindow %ld prefsDidChange", self.name);
    NSRange range = NSMakeRange(0, 0);
    NSRange linkrange = NSMakeRange(0, 0);
    NSRange selectedRange = textview.selectedRange;

    NSUInteger i;

    styles = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];

    for (i = 0; i < style_NUMSTYLES; i++) {
        if (self.theme.doStyles) {
            [styles addObject:[((GlkStyle *)[self.theme valueForKey:gGridStyleNames[i]]) attributesWithHints:(self.styleHints)[i]]];
        } else {
            [styles addObject:((GlkStyle *)[self.theme valueForKey:gGridStyleNames[i]]).attributeDict];
        }
    }

    NSInteger marginX = self.theme.gridMarginX;
    NSInteger marginY = self.theme.gridMarginY;

    textview.textContainerInset = NSMakeSize(marginX, marginY);

    [textstorage removeAttribute:NSBackgroundColorAttributeName
                           range:NSMakeRange(0, textstorage.length)];

    /* reassign styles to attributedstrings */
    for (i = 0; i < rows; i++) {
        NSRange lineRange = NSMakeRange(i * cols, cols);
        if (i * cols + cols > textstorage.length)
            lineRange = NSMakeRange(i * cols, textstorage.length - i * cols);
        NSMutableAttributedString *line =
        [[textstorage attributedSubstringFromRange:lineRange] mutableCopy];
        NSUInteger x = 0;
        while (x < line.length) {
            id styleobject = [line attribute:@"GlkStyle"
                                     atIndex:x
                              effectiveRange:&range];

            NSDictionary *attributes =
            styles[(NSUInteger)[styleobject intValue]];

            id hyperlink = [line attribute:NSLinkAttributeName
                                   atIndex:x
                            effectiveRange:&linkrange];

            [line setAttributes:attributes range:range];

            if (hyperlink) {
                [line addAttribute:NSLinkAttributeName
                             value:hyperlink
                             range:linkrange];
            }

            x = range.location + range.length;
        }

        [textstorage replaceCharactersInRange:lineRange
                         withAttributedString:line];
    }

    NSMutableDictionary *linkAttributes = [textview.linkTextAttributes mutableCopy];
    linkAttributes[NSForegroundColorAttributeName] = styles[style_Normal][NSForegroundColorAttributeName];
    textview.linkTextAttributes = linkAttributes;

//    NSLog(@"prefsDidChange: selected range was %@, restored to %@",
//          NSStringFromRange(textview.selectedRange),
//          NSStringFromRange(selectedRange));
    textview.selectedRange = selectedRange;
    [self setNeedsDisplay:YES];
    dirty = NO;

    [self recalcBackground];
}

- (BOOL)isOpaque {
    return !transparent;
}

- (void)makeTransparent {
    transparent = YES;
    dirty = YES;
}

- (void)flushDisplay {
    if (dirty)
        [self setNeedsDisplay:YES];
    dirty = NO;
}

- (BOOL)allowsDocumentBackgroundColorChange {
    return YES;
}

- (void)changeDocumentBackgroundColor:(id)sender {
    NSLog(@"changeDocumentBackgroundColor");
}


- (void)recalcBackground {
    NSColor *bgcolor;
    bgcolor = nil;

    if (self.theme.doStyles) {
        NSDictionary *attributes = [self.theme.gridNormal attributesWithHints:(self.styleHints)[style_Normal]];
        bgcolor = attributes[NSBackgroundColorAttributeName];
        if ([self.styleHints[stylehint_ReverseColor] isNotEqualTo:[NSNull null]] && !([self.glkctl.game.metadata.format isEqualToString:@"glulx"] || [self.glkctl.game.metadata.format isEqualToString:@"hugo"]))
        {   // Hack to make status bars look okay in other interpreters than Glulxe.
            // Need to find out what is really going on here.
            bgcolor = attributes[NSForegroundColorAttributeName];
        }
    }

    if (!bgcolor)
        bgcolor = self.theme.gridBackground;

    textview.backgroundColor = bgcolor;
    textview.insertionPointColor = bgcolor;
}

- (BOOL)wantsFocus {
    return char_request || line_request;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)becomeFirstResponder {
    if (char_request) {
        [self setNeedsDisplay:YES];
        dirty = NO;
    }
    return [super becomeFirstResponder];
}

- (BOOL)resignFirstResponder {
    if (char_request) {
        [self setNeedsDisplay:YES];
        dirty = NO;
    }
    return [super resignFirstResponder];
}

- (void)setFrame:(NSRect)frame {

    if (self.glkctl.ignoreResizes)
        return;
    NSUInteger r;
    NSRange selectedRange = textview.selectedRange;
    if (self.inLiveResize)
        _restoredSelection = NSMakeRange(0, 0);
    if (!NSEqualRanges(_restoredSelection, selectedRange))
        selectedRange = _restoredSelection;

    super.frame = frame;
    NSInteger newcols =
    (NSInteger)round((frame.size.width - (textview.textContainerInset.width + container.lineFragmentPadding
          ) * 2) /
         self.theme.cellWidth);

    NSInteger newrows = (NSInteger)round((frame.size.height + self.theme.gridNormal.lineSpacing
                              - (textview.textContainerInset.height * 2) ) /
                             self.theme.cellHeight);

//    NSLog(@"GlkTextGridWindow setFrame: newcols: %ld newrows: %ld", newcols, newrows);
    if (newcols == (NSInteger)cols && newrows == (NSInteger)rows) {
        //&& NSEqualRects(textview.frame, frame)) {
        //        NSLog(@"GlkTextGridWindow %ld setFrame: new frame same as old frame. "
        //              @"Skipping.", self.name);

        if ( NSEqualRects(textview.frame, frame))
            return;
    } else {
        //        NSLog(@"GlkTextGridWindow %ld setFrame: old cols:%ld new cols:%ld old rows:%ld new rows:%ld", self.name, cols, newcols, rows, newrows);
        //        NSLog(@"Rebuilding grid window!");
    }

    if (newcols < 1)
        newcols = 1;
    if (newrows < 1)
        newrows = 1;

    NSSize screensize = self.glkctl.window.screen.visibleFrame.size;
    if (newcols * self.theme.cellWidth > screensize.width || newrows * self.theme.cellHeight > screensize.height) {
        NSLog(@"GlkTextGridWindow setFrame error! newcols (%ld) * theme.cellwith (%f) = %f. newrows (%ld) * theme.cellheight (%f) = %f. Returning.", newcols, self.theme.cellWidth, newcols * self.theme.cellWidth, newrows, self.theme.cellHeight, newrows * self.theme.cellHeight);
        return;
    }

    NSMutableAttributedString *backingStorage = [textstorage mutableCopy];

    if (!backingStorage) {
        NSString *spaces = [[[NSString alloc] init]
                            stringByPaddingToLength:(NSUInteger)(rows * (cols + 1) - (cols > 1))
                            withString:@" "
                            startingAtIndex:0];
        backingStorage = [[NSTextStorage alloc]
                          initWithString:spaces
                          attributes:styles[style_Normal]];
    }
    if (newcols < cols) {
        // Delete characters if the window has become narrower
        for (r = cols - 1; r < backingStorage.length; r += cols + 1) {
            if (r < (cols - newcols))
                continue;
            NSRange deleteRange =
            NSMakeRange(r - (cols - newcols), cols - newcols);
            if (NSMaxRange(deleteRange) > backingStorage.length)
                deleteRange =
                NSMakeRange(r - (cols - newcols),
                            backingStorage.length - (r - (cols - newcols)));

            [backingStorage deleteCharactersInRange:deleteRange];
            r -= (cols - newcols);
        }
        // For some reason we must remove a couple of extra characters at the
        // end to avoid strays
        if (rows == 1 && cols > 1 &&
            backingStorage.length >= (cols - 2))
            [backingStorage
             deleteCharactersInRange:NSMakeRange(cols - 2,
                                                 backingStorage.length -
                                                 (cols - 2))];
    } else if (newcols > cols) {
        // Pad with spaces if the window has become wider
        NSString *spaces =
        [[[NSString alloc] init] stringByPaddingToLength:newcols - cols
                                              withString:@" "
                                         startingAtIndex:0];
        NSAttributedString *string = [[NSAttributedString alloc]
                                      initWithString:spaces
                                      attributes:styles[style_Normal]];

        for (r = cols; r < backingStorage.length - 1; r += (cols + 1)) {
            [backingStorage insertAttributedString:string atIndex:r];
            r += (newcols - cols);
        }
    }
    cols = newcols;
    rows = newrows;

    NSUInteger desiredLength =
    rows * (cols + 1) - 1; // -1 because we don't want a newline at the end
    if (desiredLength < 1 || rows == 1)
        desiredLength = cols;
    if (backingStorage.length < desiredLength) {
        NSString *spaces = [[[NSString alloc] init]
                            stringByPaddingToLength:desiredLength - backingStorage.length
                            withString:@" "
                            startingAtIndex:0];
        NSAttributedString *string = [[NSAttributedString alloc]
                                      initWithString:spaces
                                      attributes:styles[style_Normal]];
        [backingStorage appendAttributedString:string];
    } else if (backingStorage.length > desiredLength)
        [backingStorage
         deleteCharactersInRange:NSMakeRange(desiredLength,
                                             backingStorage.length -
                                             desiredLength)];

    NSAttributedString *newlinestring = [[NSAttributedString alloc]
                                         initWithString:@"\n"
                                         attributes:styles[style_Normal]];

    // Instert a newline character at the end of each line to avoid reflow
    // during live resize. (We carefully have to print around these in the
    // printToWindow method)
    for (r = cols; r < backingStorage.length; r += cols + 1)
        [backingStorage replaceCharactersInRange:NSMakeRange(r, 1)
                            withAttributedString:newlinestring];

    [textstorage setAttributedString:backingStorage];
    textview.frame = frame;
    [self recalcBackground];
    // NSLog(@"setFrame: selected range was %@, trying to restore to %@",
    // NSStringFromRange(textview.selectedRange),
    // NSStringFromRange(selectedRange));
    if (NSMaxRange(selectedRange) >= textstorage.length)
        selectedRange = NSMakeRange(textstorage.length - 1, 0);
    textview.selectedRange = selectedRange;
    // NSLog(@"Result: textview.selectedRange = %@",
    // NSStringFromRange(textview.selectedRange));
    _restoredSelection = selectedRange;
    dirty = YES;
}

- (void)restoreSelection {
    // NSLog(@"restoreSelection: selected range was %@, restored to %@",
    // NSStringFromRange(textview.selectedRange),
    // NSStringFromRange(_restoredSelection));
    textview.selectedRange = _restoredSelection;
}

- (void)moveToColumn:(NSUInteger)c row:(NSUInteger)r {
    xpos = c;
    ypos = r;
}

- (void)clear {
    NSRange selectedRange = textview.selectedRange;
    [textstorage setAttributedString:[[NSMutableAttributedString alloc]
                                      initWithString:@""]];
    hyperlinks = nil;
    hyperlinks = [[NSMutableArray alloc] init];

    rows = cols = 0;
    xpos = ypos = 0;

    if (self.frame.size.width > 0 && self.frame.size.height > 0)
        [self setFrame:self.frame];
    if (NSMaxRange(selectedRange) <= textview.textStorage.length)
        textview.selectedRange = selectedRange;
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
    NSUInteger pos = 0;
    NSDictionary *att = styles[stylevalue];
    NSRange selectedRange = textview.selectedRange;

//    NSLog(@"textGrid printToWindow: '%@' (style %ld)", string, stylevalue);
//    NSLog(@"cols: %ld rows: %ld", cols, rows);
//    NSLog(@"xpos:%ld ypos: %ld", xpos, ypos);
//
//    NSLog(@"self.frame.size.width: %f",self.frame.size.width);

    NSMutableDictionary *attrDict = styles[stylevalue];

    if (!attrDict)
        NSLog(@"GlkTextGridWindow printToWindow: ERROR! Style dictionary nil!");

    NSString *firstline =
    [textstorage.string substringWithRange:NSMakeRange(0, cols)];
    CGSize stringSize = [firstline
                         sizeWithAttributes:attrDict];

    if (stringSize.width > self.frame.size.width) {
        NSLog(@"ERROR! First line has somehow become too wide!!!!");
        NSLog(@"First line of text storage: '%@'", firstline);
        NSLog(@"Width of first line: %f", stringSize.width);
        NSLog(@"Width of text grid window: %f", self.frame.size.width);
    }

    // Check for newlines
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
        if ((cols > -1 && ypos > (NSInteger)textstorage.length / (cols + 1) ) || ypos > rows)
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

        if ((cols + 1) * ypos + xpos + amountToDraw > textstorage.length)
            amountToDraw = textstorage.length - ((cols + 1) * ypos + xpos) + 1;

        if (amountToDraw < 1)
            break;

        // "Draw" the characters
        NSAttributedString *partString = [[NSAttributedString alloc]
                                          initWithString:[string substringWithRange:NSMakeRange(pos,
                                                                                                amountToDraw)]
                                          attributes:att];

        [textstorage
         replaceCharactersInRange:NSMakeRange((cols + 1) * ypos + xpos,
                                              amountToDraw)
         withAttributedString:partString];
        
//        NSLog(@"Replaced characters in range %@ with '%@'",
//        NSStringFromRange(NSMakeRange((cols + 1) * ypos + xpos,
//        amountToDraw)), partString.string);

        // Update the x position (and the y position if necessary)
        xpos += amountToDraw;
        pos += amountToDraw;
        if (xpos >= cols - 1) {
            xpos = 0;
            ypos++;
        }
    }

    // NSLog(@"printToWindow: selected range was %@, restored to %@",
    // NSStringFromRange(textview.selectedRange),
    // NSStringFromRange(selectedRange));
    textview.selectedRange = selectedRange;
    dirty = YES;
}

- (void)initMouse {
    mouse_request = YES;
}

- (void)cancelMouse {
    mouse_request = NO;
}

- (void)setHyperlink:(NSUInteger)linkid {
    // NSLog(@"txtgrid: hyperlink %ld set", (long)linkid);

    NSUInteger length = ypos * (cols + 1) + xpos;

    if (currentHyperlink && currentHyperlink.index != linkid) {
        //        NSLog(@"There is a preliminary hyperlink, with index %ld",
        //        currentHyperlink.index);
        if (currentHyperlink.startpos >= length) {
            //            NSLog(@"The preliminary hyperlink started at the very
            //            end of grid window text, so it was deleted to avoid a
            //            zero-length link. currentHyperlink.startpos == %ld,
            //            length == %ld", currentHyperlink.startpos, length);
            currentHyperlink = nil;
        } else {
            currentHyperlink.range = NSMakeRange(
                                                 currentHyperlink.startpos, length - currentHyperlink.startpos);
            [hyperlinks addObject:currentHyperlink];
            NSNumber *link = @(currentHyperlink.index);

            [textstorage addAttribute:NSLinkAttributeName
                                value:link
                                range:currentHyperlink.range];

            dirty = YES;
            currentHyperlink = nil;
        }
    }
    if (!currentHyperlink && linkid) {
        currentHyperlink = [[GlkHyperlink alloc] initWithIndex:linkid
                                                        andPos:length];
        // NSLog(@"New preliminary hyperlink started at position %ld, with link
        // index %ld", currentHyperlink.startpos,linkid);
    }
}

- (void)initHyperlink {
    hyper_request = YES;
    // NSLog(@"txtgrid: hyperlink event requested");
}

- (void)cancelHyperlink {
    hyper_request = NO;
    NSLog(@"txtgrid: hyperlink event cancelled");
}

- (BOOL)textView:textview clickedOnLink:(id)link atIndex:(NSUInteger)charIndex {
    NSLog(@"txtgrid: clicked on link: %@", link);
    self.glkctl.shouldScrollOnInputEvent = YES;
    if (!hyper_request) {
        NSLog(@"txtgrid: No hyperlink request in window.");
        return NO;
    }

    GlkEvent *gev =
    [[GlkEvent alloc] initLinkEvent:((NSNumber *)link).unsignedIntegerValue
                          forWindow:self.name];
    [self.glkctl queueEvent:gev];

    hyper_request = NO;
    return YES;
}

- (BOOL)myMouseDown:(NSEvent *)theEvent {
    GlkEvent *gev;
//    NSLog(@"mousedown in grid window %ld", self.name);

    if (mouse_request) {
        self.glkctl.shouldScrollOnInputEvent = YES;
        [self.glkctl markLastSeen];

        NSPoint p;
        p = theEvent.locationInWindow;

        p = [textview convertPoint:p fromView:nil];

        p.x -= textview.textContainerInset.width;
        p.y -= textview.textContainerInset.height;

        NSUInteger charIndex =
        [textview.layoutManager characterIndexForPoint:p
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

- (void)initChar {
    // NSLog(@"init char in %ld", (long)self.name);
    char_request = YES;
    dirty = YES;
    [self speakStatus:nil];
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
        ch = chartokeycode([str characterAtIndex:0]);

    GlkWindow *win;
    // pass on this key press to another GlkWindow if we are not expecting one
    if (!self.wantsFocus)
        for (win in [self.glkctl.gwindows allValues]) {
            if (win != self && win.wantsFocus) {
                [win grabFocus];
                NSLog(@"GlkTextGridWindow: Passing on keypress");
                if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
                    [(GlkTextBufferWindow *)win onKeyDown:evt];
                } else
                    [win keyDown:evt];
                return;
            }
        }

    if (char_request && ch != keycode_Unknown) {
        [self.glkctl markLastSeen];

        // NSLog(@"char event from %ld", self.name);
        GlkEvent *gev = [[GlkEvent alloc] initCharEvent:ch forWindow:self.name];
        [self.glkctl queueEvent:gev];
        char_request = NO;
        dirty = YES;
        return;
    }

    NSNumber *key = @(ch);

    if (line_request &&
        (ch == keycode_Return ||
         [currentTerminators[key] isEqual:@(YES)]))
        [[input window] makeFirstResponder:nil];
}

- (void)initLine:(NSString *)str {
    if (self.terminatorsPending) {
        currentTerminators = self.pendingTerminators;
        self.terminatorsPending = NO;
    }

    NSRect bounds = self.bounds;
    NSInteger mx = (NSInteger)textview.textContainerInset.width;
    NSInteger my = (NSInteger)textview.textContainerInset.height;

    //    What is this supposed to do?
    //    Quotebox, I guess?
    //    if (transparent)
    //        mx = my = 0;

    NSInteger x0 = (NSInteger)(NSMinX(bounds) + mx + container.lineFragmentPadding);
    NSInteger y0 = (NSInteger)(NSMinY(bounds) + my);
    CGFloat lineHeight = [Preferences lineHeight];
    CGFloat charWidth = [Preferences charWidth];

    if (ypos >= textstorage.length / cols)
        ypos = textstorage.length / cols - 1;

    NSRect caret;
    caret.origin.x = x0 + xpos * charWidth;
    caret.origin.y = y0 + ypos * lineHeight;
    caret.size.width =
    NSMaxX(bounds) - mx * 2 - caret.origin.x; // 20 * charWidth;
    caret.size.height = lineHeight;

    NSLog(@"grid initLine: %@ in: %ld", str, (long)self.name);

    input = [[NSTextField alloc] initWithFrame:caret];
    input.editable = YES;
    input.bordered = NO;
    input.action = @selector(typedEnter:);
    input.target = self;
    input.allowsEditingTextAttributes = YES;
    input.bezeled = NO;
    input.drawsBackground = NO;
    input.selectable = YES;
    input.delegate = self;

    [input.cell setWraps:YES];

    if (str.length == 0)
        str = @" ";

    NSAttributedString *attString = [[NSAttributedString alloc]
                                     initWithString:str
                                     attributes:styles[style_Input]];

    input.attributedStringValue = attString;

    MyTextFormatter *inputFormatter =
    [[MyTextFormatter alloc] initWithMaxLength:cols - xpos];
    input.formatter = inputFormatter;

    [textview addSubview:input];

    [self performSelector:@selector(deferredGrabFocus:) withObject:input afterDelay:0.1];

    line_request = YES;
}

- (void)deferredGrabFocus:(id)sender {
    [self.window makeFirstResponder:input];
}

- (NSString *)cancelLine {
    line_request = NO;
    if (input) {
        NSString *str = input.stringValue;
        [self printToWindow:str style:style_Input];
        [input removeFromSuperview];
        input = nil;
        return str;
    }
    return @"";
}

- (void)typedEnter:(id)sender {
    line_request = NO;
    if (input) {
        [self.glkctl markLastSeen];
        self.glkctl.shouldScrollOnInputEvent = YES;

        NSString *str = input.stringValue;
        [self printToWindow:str style:style_Input];
        str = [str scrubInvalidCharacters];
        GlkEvent *gev = [[GlkEvent alloc] initLineEvent:str
                                              forWindow:self.name];
        [self.glkctl queueEvent:gev];
        [input removeFromSuperview];
        input = nil;
    }
}

- (NSRange)textView:(NSTextView *)aTextView
willChangeSelectionFromCharacterRange:(NSRange)oldrange
   toCharacterRange:(NSRange)newrange {
    if (textstorage.length >= NSMaxRange(_restoredSelection))
        _restoredSelection = newrange;
    return newrange;
}

- (void)controlTextDidChange:(NSNotification *)obj {
    NSDictionary *dict = obj.userInfo;
    fieldEditor = dict[@"NSFieldEditor"];
}

#pragma mark Accessibility

- (NSArray *)accessibilityAttributeNames {
    NSMutableArray *result = [[super accessibilityAttributeNames] mutableCopy];
    if (!result)
        result = [[NSMutableArray alloc] init];

    [result addObjectsFromArray:@[ NSAccessibilityContentsAttribute ]];

    //    NSLog(@"GlkTextGridWindow: accessibilityAttributeNames: %@ ", result);

    return result;
}

- (id)accessibilityAttributeValue:(NSString *)attribute {
    NSResponder *firstResponder = self.window.firstResponder;
    // NSLog(@"GlkTextGridWindow: accessibilityAttributeValue: %@",attribute);
    if ([attribute isEqualToString:NSAccessibilityContentsAttribute]) {
        return textview;
    } else if ([attribute
                isEqualToString:NSAccessibilityRoleDescriptionAttribute]) {
        return [NSString
                stringWithFormat:
                @"Status window%@%@%@. %@",
                line_request ? @", waiting for commands" : @"",
                char_request ? @", waiting for a key press" : @"",
                hyper_request ? @", waiting for a hyperlink click" : @"",
                [textview
                 accessibilityAttributeValue:NSAccessibilityValueAttribute]];
    } else if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
        // return (id)NO;
        return [NSNumber numberWithBool:firstResponder == self ||
                firstResponder == textview];
    } else if ([attribute
                isEqualToString:NSAccessibilityFocusedUIElementAttribute]) {
        return self.accessibilityFocusedUIElement;
    } else if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
        return @[ textview ];
    }

    return [super accessibilityAttributeValue:attribute];
}

- (id)accessibilityFocusedUIElement {
    return textview;
}

- (IBAction)speakStatus:(id)sender {
    if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_9) {
        NSDictionary *announcementInfo = @{
                                           NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh),
                                           NSAccessibilityAnnouncementKey : textstorage.string
                                           };
        NSAccessibilityPostNotificationWithUserInfo(
                                                    [NSApp mainWindow],
                                                    NSAccessibilityAnnouncementRequestedNotification, announcementInfo);
    }
}

@end
