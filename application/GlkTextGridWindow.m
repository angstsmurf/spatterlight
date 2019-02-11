
/*
 * GlkTextGridWindow
 */

#import "main.h"
#import "GlkHyperlink.h"
#import "Compatibility.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...) fprintf(stderr,"%s\n", [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@interface MyGridTextView : NSTextView
{
    GlkTextGridWindow * glkTextGrid;
}

- (instancetype) initWithFrame:(NSRect)rect textContainer:(NSTextContainer *)container textGrid: (GlkTextGridWindow *)textbuffer;
- (void) superKeyDown: (NSEvent*)evt;

@end

/*
 * Extend NSTextView to ...
 *   - call onKeyDown and mouseDown on our GlkTextGridWindow object
 */

@implementation MyGridTextView

- (instancetype) initWithFrame:(NSRect)rect textContainer:(NSTextContainer *)container textGrid: (GlkTextGridWindow *)textbuffer
{
    self = [super initWithFrame:rect textContainer:container];
    if (self)
    {
        glkTextGrid = textbuffer;
    }
    return self;
}

- (void) superKeyDown: (NSEvent*)evt
{
    [super keyDown: evt];
}

- (void) keyDown: (NSEvent*)evt
{
    [glkTextGrid keyDown: evt];
}

- (void) mouseDown: (NSEvent*)theEvent
{
    if (![glkTextGrid myMouseDown:theEvent])
        [super mouseDown:theEvent];
}

@end


//Custom formatter adapted from code by Jonathan Mitchell
//See https://stackoverflow.com/questions/827014/how-to-limit-nstextfield-text-length-and-keep-it-always-upper-case
//
@interface MyTextFormatter : NSFormatter {}

- (id)initWithMaxLength: (NSInteger)alength;

@property NSInteger maxLength;

@end

@implementation MyTextFormatter

- (id)init
{
    if(self = [super init]){
        self.maxLength = INT_MAX;
    }

    return self;
}

- (id)initWithMaxLength: (NSInteger)alength
{
    if(self = [super init]){
        self.maxLength = alength;
    }

    return self;
}

#pragma mark -
#pragma mark Textual Representation of Cell Content

- (NSString *)stringForObjectValue:(id)object
{
    NSString *stringValue = nil;
    if ([object isKindOfClass:[NSString class]]) {

        // A new NSString is perhaps not required here
        // but generically a new object would be generated
        stringValue = [NSString stringWithString:object];
    }

    return stringValue;
}

- (BOOL)getObjectValue:(id *)object forString:(NSString *)string errorDescription:(NSString **)error
{
    BOOL valid = YES;

    *object = [NSString stringWithString:string];

    return valid;
}

- (BOOL)isPartialStringValid:(NSString **)partialStringPtr
       proposedSelectedRange:(NSRangePointer)proposedSelRangePtr
              originalString:(NSString *)origString
       originalSelectedRange:(NSRange)origSelRange
            errorDescription:(NSString **)error
{
    BOOL valid = YES;

    NSString *proposedString = *partialStringPtr;
    if ((NSInteger)proposedString.length > self.maxLength) {

        // The original string has been modified by one or more characters (via pasting).
        // Either way compute how much of the proposed string can be accommodated.
        NSInteger origLength = origString.length;
        NSInteger insertLength = self.maxLength - origLength;

        // If a range is selected then characters in that range will be removed
        // so adjust the insert length accordingly
        insertLength += origSelRange.length;

        // Get the string components
        NSString *prefix = [origString substringToIndex:origSelRange.location];
        NSString *suffix = [origString substringFromIndex:origSelRange.location + origSelRange.length];
        NSString *insert = [proposedString substringWithRange:NSMakeRange(origSelRange.location, insertLength)];

        // Assemble the final string
        *partialStringPtr = [NSString stringWithFormat:@"%@%@%@", prefix, insert, suffix];

        // Fix-up the proposed selection range
        proposedSelRangePtr->location = origSelRange.location + insertLength;
        proposedSelRangePtr->length = 0;
        
        valid = NO;
    }
    
    return valid;
}

@end


@implementation GlkTextGridWindow

- (instancetype) initWithGlkController: (GlkController*)glkctl_ name: (NSInteger)name_
{
    self = [super initWithGlkController: glkctl_ name: name_];

    if (self)
    {
        //lines = [[NSMutableArray alloc] init];
        input = nil;

        rows = 0;
        cols = 0;
        xpos = 0;
        ypos = 0;

        line_request = NO;
        char_request = NO;
        mouse_request = NO;
        hyper_request = NO;

        hyperlinks = [[NSMutableArray alloc] init];
        currentHyperlink = nil;

        transparent = NO;

        /* construct text system manually */

        NSScrollView *scrollview = [[NSScrollView alloc] initWithFrame: NSZeroRect];
        scrollview.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        [scrollview setHasHorizontalScroller: NO];
        [scrollview setHasVerticalScroller: NO];
        scrollview.horizontalScrollElasticity = NSScrollElasticityNone;
        scrollview.verticalScrollElasticity = NSScrollElasticityNone;

        scrollview.borderType = NSNoBorder;

        textstorage = [[NSTextStorage alloc] init];

        layoutmanager = [[NSLayoutManager alloc] init];
        [textstorage addLayoutManager: layoutmanager];

        container = [[NSTextContainer alloc] initWithContainerSize: NSMakeSize(10000000, 10000000)];

        container.layoutManager = layoutmanager;
        [layoutmanager addTextContainer: container];

        textview = [[MyGridTextView alloc] initWithFrame:NSMakeRect(0, 0, 0, 0) textContainer:container textGrid:self];

        textview.minSize = NSMakeSize(0, 0);
        textview.maxSize = NSMakeSize(10000000, 10000000);

        container.textView = textview;
        scrollview.documentView = textview;

        /* now configure the text stuff */

        textview.delegate = self;
        textstorage.delegate = self;
        textview.textContainerInset = NSMakeSize([Preferences gridMargins], [Preferences gridMargins]);

        [textview setEditable:NO];
        [textview setUsesFontPanel: NO];

        [self addSubview: scrollview];
        [self recalcBackground];
    }
    return self;
}

- (BOOL) isFlipped
{
    return YES;
}

- (void) setStyle: (NSInteger)style windowType: (NSInteger)wintype enable: (NSInteger*)enable value:(NSInteger*)value
{
    [super setStyle:style windowType:wintype enable:enable value:value];
    [self recalcBackground];
}

- (void) prefsDidChange
{
    NSRange range = NSMakeRange(0, 0);
    NSRange linkrange= NSMakeRange(0, 0);

    int i;

    [super prefsDidChange];

    NSInteger margin = [Preferences gridMargins];
    textview.textContainerInset = NSMakeSize(margin, margin);

    [textstorage removeAttribute: NSBackgroundColorAttributeName
                           range: NSMakeRange(0, textstorage.length)];

    /* reassign styles to attributedstrings */
    for (i = 0; i < rows; i++)
    {
        NSRange lineRange = NSMakeRange(i * cols, cols);
        if (i * cols + cols > (long)textstorage.length)
            lineRange = NSMakeRange(i * cols, textstorage.length - i * cols);
        NSMutableAttributedString *line = [[textstorage attributedSubstringFromRange:lineRange] mutableCopy];
        NSUInteger x = 0;
        while (x < line.length)
        {
            id styleobject = [line attribute:@"GlkStyle" atIndex:x effectiveRange:&range];

            NSDictionary * attributes = [self attributesFromStylevalue:[styleobject intValue]];

            id hyperlink = [line attribute: NSLinkAttributeName atIndex:x effectiveRange:&linkrange];

            [line setAttributes: attributes range: range];

            if (hyperlink)
            {
                [line addAttribute: NSLinkAttributeName
                             value: hyperlink
                             range: linkrange];
            }

            x = (int)(range.location + range.length);

        }

        [textstorage replaceCharactersInRange:lineRange withAttributedString:line];
    }

    [self recalcBackground];
    [self setNeedsDisplay: YES];
    dirty = NO;
}

- (BOOL) isOpaque
{
    return !transparent;
}

- (void) makeTransparent
{
    transparent = YES;
    dirty = YES;
}

- (void) flushDisplay
{
    if (dirty)
        [self setNeedsDisplay: YES];
    dirty = NO;
}

- (BOOL) allowsDocumentBackgroundColorChange { return YES; }

- (void)changeDocumentBackgroundColor:(id)sender
{
    NSLog(@"changeDocumentBackgroundColor");
}

- (void) recalcBackground
{
    NSColor *bgcolor, *fgcolor;

    bgcolor = nil;
    fgcolor = nil;

    if ([Preferences stylesEnabled])
    {
        bgcolor = styles[style_Normal].attributes[NSBackgroundColorAttributeName];
        fgcolor = styles[style_Normal].attributes[NSForegroundColorAttributeName];
    }

    if (!bgcolor)
        bgcolor = [Preferences gridBackground];
    if (!fgcolor)
        fgcolor = [Preferences gridForeground];

    textview.backgroundColor = bgcolor;
    textview.insertionPointColor = fgcolor;
}

- (BOOL) wantsFocus
{
    return char_request || line_request;
}

- (BOOL) acceptsFirstResponder
{
    return YES;
}

- (BOOL) becomeFirstResponder
{
    if (char_request)
    {
        [self setNeedsDisplay: YES];
        dirty = NO;
    }
    return [super becomeFirstResponder];
}

- (BOOL) resignFirstResponder
{
    if (char_request)
    {
        [self setNeedsDisplay: YES];
        dirty = NO;
    }
    return [super resignFirstResponder];
}

- (void) setFrame: (NSRect)frame
{
    NSUInteger r;

    super.frame = frame;

    NSInteger newcols = ceil((frame.size.width - (textview.textContainerInset.width + container.lineFragmentPadding) * 2) / Preferences.charWidth);
    NSInteger newrows = ceil((frame.size.height + Preferences.leading - (textview.textContainerInset.width) * 2) / Preferences.lineHeight);

    if (newcols == cols && newrows == rows && NSEqualRects(textview.frame, frame))
        return;

    if (newcols < 0)
        newcols = 0;
    if (newrows < 0)
        newrows = 0;

    NSMutableAttributedString *backingStorage = [textstorage mutableCopy];
    if (newcols < cols)
    {
        // Delete characters if the window has become narrower
        for (r = cols - 1; r < backingStorage.length; r += cols + 1)
        {
            NSRange deleteRange = NSMakeRange(r - (cols - newcols), cols - newcols);
            if (r - (cols - newcols) < 0)
                continue;
            if (NSMaxRange(deleteRange) > backingStorage.length)
                deleteRange = NSMakeRange(r - (cols - newcols), backingStorage.length - (r - (cols - newcols)));
            
            [backingStorage deleteCharactersInRange:deleteRange];
            r -= (cols - newcols);
        }
        // For some reason we must remove a couple of extra characters at the end to avoid strays
        if (rows == 1 && (NSInteger)backingStorage.length >= (cols - 2))
            [backingStorage deleteCharactersInRange:NSMakeRange(cols - 2, backingStorage.length - (cols - 2))];
    }
    else if (newcols > cols)
    {
        // Pad with spaces if the window has become wider
        NSString *spaces = [[[NSString alloc] init] stringByPaddingToLength: newcols - cols withString:@" " startingAtIndex:0];
        NSAttributedString* string = [[NSAttributedString alloc]
                                      initWithString: spaces
                                      attributes: [self attributesFromStylevalue:style_Normal]];

        for (r = cols ; r < backingStorage.length - 1; r += (cols + 1))
        {
            [backingStorage insertAttributedString:string atIndex:r];
            r += (newcols - cols);
        }
    }
    cols = newcols;
    rows = newrows;

    NSInteger desiredLength = rows * (cols + 1) - 1; // -1 because we don't want a newline at the end
    if (desiredLength < 1 || rows == 1)
        desiredLength = cols;
    if ((NSInteger)backingStorage.length < desiredLength)
    {
        NSString *spaces = [[[NSString alloc] init] stringByPaddingToLength: desiredLength - backingStorage.length withString:@" " startingAtIndex:0];
        NSAttributedString* string = [[NSAttributedString alloc]
                                  initWithString: spaces
                                  attributes: styles[style_Normal].attributes];
        [backingStorage appendAttributedString:string];
    }
    else if ((NSInteger)backingStorage.length > desiredLength)
        [backingStorage deleteCharactersInRange: NSMakeRange(desiredLength, backingStorage.length - desiredLength)];

    NSAttributedString* newlinestring = [[NSAttributedString alloc]
                                         initWithString: @"\n"
                                         attributes: styles[style_Normal].attributes];

    // Instert a newline character at the end of each line to avoid reflow during live resize.
    // (We carefully have to print around these in the printToWindow method)
    for (r = cols; r < backingStorage.length; r += cols + 1)
        [backingStorage replaceCharactersInRange:NSMakeRange(r, 1) withAttributedString: newlinestring];

    [textstorage setAttributedString:backingStorage];
    textview.frame = frame;
    [self recalcBackground];
    dirty = YES;
}

- (void) moveToColumn:(NSInteger)c row:(NSInteger)r
{
    xpos = c;
    ypos = r;
}

- (void) clear
{
    [textstorage setAttributedString:[[NSMutableAttributedString alloc] initWithString:@""]];
    hyperlinks = nil;
    hyperlinks = [[NSMutableArray alloc] init];

    rows = cols = 0;
    xpos = ypos = 0;

    self.frame = self.frame;
}

- (void) putString: (NSString*)string style: (NSInteger)stylevalue
{
    if (line_request)
        NSLog(@"Printing to text grid window during line request");

    if (char_request)
        NSLog(@"Printing to text grid window during character request");

    [self printToWindow: string style: stylevalue];
}

- (void) printToWindow: (NSString*)string style: (NSInteger)stylevalue
{
    NSUInteger length = string.length;
    NSUInteger pos = 0;
    NSDictionary *att = [self attributesFromStylevalue:stylevalue];

//    NSLog(@"textGrid printToWindow: '%@' (style %ld)", string, stylevalue);
//    NSLog(@"cols: %ld rows: %ld", cols, rows);
//    NSLog(@"xpos: %ld ypos: %ld", xpos, ypos);
//
//    NSLog(@"self.frame.size.width: %f",self.frame.size.width);

    NSString *firstline = [textstorage.string substringWithRange:NSMakeRange(0, cols)];
    CGSize stringSize = [firstline sizeWithAttributes:[self attributesFromStylevalue:stylevalue]];

    if (stringSize.width > self.frame.size.width)
    {
        NSLog(@"ERROR! First line has somehow become too wide!!!!");
        NSLog(@"First line of text storage: '%@'", firstline);
        NSLog(@"Width of first line: %f", stringSize.width);
        NSLog(@"Width of text grid window: %f", self.frame.size.width);
    }

    // Check for newlines
    NSUInteger x;
    for (x = 0; x < length; x++)
    {
        if ([string characterAtIndex: x] == '\n' || [string characterAtIndex: x] == '\r')
        {
            [self printToWindow: [string substringToIndex: x] style: stylevalue];
            xpos = 0;
            ypos++;
            [self printToWindow: [string substringFromIndex: x + 1] style: stylevalue];
            return;
        }
    }

    // Write this string
    while (pos < length)
    {
        // Can't write if we've fallen off the end of the window
        if (ypos > (NSInteger)textstorage.length / (cols + 1) || ypos > rows)
            break;

        // Can only write a certain number of characters
        if (xpos >= cols)
        {
            xpos = 0;
            ypos++;
            continue;
        }

        // Get the number of characters to write
        NSInteger amountToDraw = cols - xpos;
        if (amountToDraw > (NSInteger)(string.length - pos))
        {
            amountToDraw = string.length - pos;
        }

        if (cols * ypos + xpos + amountToDraw > (NSInteger)textstorage.length)
            amountToDraw = textstorage.length - (cols * ypos + xpos) + 1;

        if (amountToDraw < 1)
            break;

        // "Draw" the characters
        NSAttributedString* partString = [[NSAttributedString alloc]
                                          initWithString: [string substringWithRange: NSMakeRange(pos, amountToDraw)]
                                          attributes: att];

        [textstorage replaceCharactersInRange: NSMakeRange((cols + 1) * ypos + xpos, amountToDraw) withAttributedString: partString];
//        NSLog(@"Replaced characters in range %@ with '%@'", NSStringFromRange(NSMakeRange((cols + 1) * ypos + xpos, amountToDraw)), partString.string);

        // Update the x position (and the y position if necessary)
        xpos += amountToDraw;
        pos += amountToDraw;
        if (xpos >= cols - 1)
        {
            xpos = 0;
            ypos++;
        }
    }

    dirty = YES;
}

- (void) initMouse
{
    mouse_request = YES;
}

- (void) cancelMouse
{
    mouse_request = NO;
}

- (void) setHyperlink:(NSUInteger)linkid
{
    //NSLog(@"txtgrid: hyperlink %ld set", (long)linkid);

    NSInteger length = ypos * (cols + 1) + xpos;

    if (currentHyperlink && currentHyperlink.index != linkid)
    {
//        NSLog(@"There is a preliminary hyperlink, with index %ld", currentHyperlink.index);
        if ((NSInteger)currentHyperlink.startpos >= length)
        {
//            NSLog(@"The preliminary hyperlink started at the very end of grid window text, so it was deleted to avoid a zero-length link. currentHyperlink.startpos == %ld, length == %ld", currentHyperlink.startpos, length);
            currentHyperlink = nil;
        }
        else
        {
            currentHyperlink.range = NSMakeRange(currentHyperlink.startpos, length - currentHyperlink.startpos);
            [hyperlinks addObject:currentHyperlink];
            NSNumber *link = @(currentHyperlink.index);

            [textstorage addAttribute:NSLinkAttributeName value:link range:currentHyperlink.range];
            
            dirty = YES;
            currentHyperlink = nil;
        }
    }
    if (!currentHyperlink && linkid)
    {
        currentHyperlink = [[GlkHyperlink alloc] initWithIndex:linkid andPos:length];
        //NSLog(@"New preliminary hyperlink started at position %ld, with link index %ld", currentHyperlink.startpos,linkid);
    }
}

- (void) initHyperlink
{
    hyper_request = YES;
    //NSLog(@"txtgrid: hyperlink event requested");
}

- (void) cancelHyperlink
{
    hyper_request = NO;
    NSLog(@"txtgrid: hyperlink event cancelled");
}

- (BOOL) textView: textview clickedOnLink: (id)link atIndex: (NSUInteger)charIndex
{
    NSLog(@"txtgrid: clicked on link: %@", link);

    if (!hyper_request)
    {
        NSLog(@"txtgrid: No hyperlink request in window.");
        return NO;
    }

    GlkEvent *gev = [[GlkEvent alloc] initLinkEvent:((NSNumber *)link).unsignedIntegerValue forWindow: self.name];
    [glkctl queueEvent: gev];

    hyper_request = NO;
    return YES;
}

- (BOOL) myMouseDown: (NSEvent*)theEvent
{
    GlkEvent *gev;
    //GlkHyperlink *hyp;
    NSLog(@"mousedown in grid window");

    if (mouse_request)
    {
        [glkctl markLastSeen];

        NSPoint p;
        p = theEvent.locationInWindow;
        
        p = [textview convertPoint: p fromView: nil];

        p.x -= textview.textContainerInset.width;
        p.y -= textview.textContainerInset.height;

        NSUInteger charIndex = [textview.layoutManager
                                characterIndexForPoint:p
                                inTextContainer:container
                                fractionOfDistanceBetweenInsertionPoints:nil];
        //NSLog(@"Clicked on char index %ld, which is '%@'.", charIndex, [textstorage.string substringWithRange:NSMakeRange(charIndex, 1)]);

        p.y = charIndex / (cols + 1);
        p.x = charIndex % (cols + 1);

        //NSLog(@"p.x: %f p.y: %f", p.x, p.y);

        if (p.x >= 0 && p.y >= 0 && p.x < cols && p.y < rows)
        {
            if (mouse_request)
            {
                gev = [[GlkEvent alloc] initMouseEvent: p forWindow: self.name];
                [glkctl queueEvent: gev];
                mouse_request = NO;
                return YES;
            }
        }
    }
    else
    {
        //NSLog(@"No hyperlink request or mouse request in grid window");
        [super mouseDown:theEvent];
    }
    return NO;
}

- (void) initChar
{
    //NSLog(@"init char in %ld", (long)self.name);
    char_request = YES;
    dirty = YES;
    [self speakStatus:nil];
}

- (void) cancelChar
{
    //NSLog(@"cancel char in %ld", self.name);
    char_request = NO;
    dirty = YES;
}

- (void) keyDown: (NSEvent*)evt
{
    NSString *str = evt.characters;
    unsigned ch = keycode_Unknown;
    if (str.length)
        ch = chartokeycode([str characterAtIndex: 0]);

    GlkWindow *win;
    // pass on this key press to another GlkWindow if we are not expecting one
    if (!self.wantsFocus)
        for (int i = 0; i < MAXWIN; i++)
        {
            win = [glkctl windowWithNum:i];
            if (i != self.name && win && win.wantsFocus)
            {
                [win grabFocus];
                NSLog(@"Passing on keypress");
                if ([win isKindOfClass: [GlkTextBufferWindow class]])
                {
                    [(GlkTextBufferWindow *)win onKeyDown:evt];
                }
                else
                    [win keyDown:evt];
                return;
            }
        }

    if (char_request && ch != keycode_Unknown)
    {
        [glkctl markLastSeen];

        //NSLog(@"char event from %ld", self.name);
        GlkEvent *gev = [[GlkEvent alloc] initCharEvent: ch forWindow: self.name];
        [glkctl queueEvent: gev];
        char_request = NO;
        dirty = YES;
        return;
    }

    NSNumber *key = @(ch);

    if (line_request && (ch == keycode_Return || [currentTerminators[key] isEqual: @(YES)]))
        [self typedEnter: nil];
}

- (void) initLine:(NSString*)str
{
    if (self.terminatorsPending)
    {
        currentTerminators = self.pendingTerminators;
        self.terminatorsPending = NO;
    }

    NSRect bounds = self.bounds;
    NSInteger m = [Preferences gridMargins];
    if (transparent)
        m = 0;

    NSInteger x0 = NSMinX(bounds) + m + container.lineFragmentPadding;
    NSInteger y0 = NSMinY(bounds) + m;
    NSInteger lineHeight = [Preferences lineHeight];
    float charWidth = [Preferences charWidth];

    if (ypos >= (NSInteger)textstorage.length / cols)
        ypos = textstorage.length / cols - 1;

    NSRect caret;
    caret.origin.x = x0 + xpos * charWidth;
    caret.origin.y = y0 + ypos * lineHeight;
    caret.size.width = NSMaxX(bounds) - m * 2 - caret.origin.x; //20 * charWidth;
    caret.size.height = lineHeight;

    NSLog(@"grid initLine: %@ in: %ld", str, (long)self.name);

    input = [[NSTextField alloc] initWithFrame: caret];
    input.editable = YES;
    input.bordered = NO;
    input.action = @selector(typedEnter:);
    input.target = self;
    input.allowsEditingTextAttributes = YES;
    input.bezeled = NO;
    input.drawsBackground = NO;
    input.selectable = YES;

    [input.cell setWraps:YES];

    if (str.length == 0)
        str = @" ";

    NSAttributedString *attString = [[NSAttributedString alloc] initWithString:str attributes:[self attributesFromStylevalue:style_Input]];

    input.attributedStringValue = attString;

    MyTextFormatter *inputFormatter = [[MyTextFormatter alloc] initWithMaxLength:cols - xpos];
    input.formatter = inputFormatter;

    [textview addSubview: input];
    [self.window makeFirstResponder: input];

    line_request = YES;
}

- (NSString*) cancelLine
{
    line_request = NO;
    if (input)
    {
        NSString *str = input.stringValue;
        [self printToWindow: str style: style_Input];
        [input removeFromSuperview];
        input = nil;
        return str;
    }
    return @"";
}

- (void) typedEnter: (id)sender
{
    line_request = NO;
    if (input)
    {
        [glkctl markLastSeen];

        NSString *str = input.stringValue;
        [self printToWindow: str style: style_Input];
        GlkEvent *gev = [[GlkEvent alloc] initLineEvent: str forWindow: self.name];
        [glkctl queueEvent: gev];
        [input removeFromSuperview];
        input = nil;
    }
}

# pragma mark Accessibility

- (NSArray*) accessibilityAttributeNames {
    NSMutableArray* result = [[super accessibilityAttributeNames] mutableCopy];
    if (!result) result = [[NSMutableArray alloc] init];

    [result addObjectsFromArray:@[NSAccessibilityContentsAttribute]];

    //    NSLog(@"GlkTextGridWindow: accessibilityAttributeNames: %@ ", result);

    return result;
}

- (id)accessibilityAttributeValue:(NSString *)attribute
{
    //NSLog(@"GlkTextGridWindow: accessibilityAttributeValue: %@",attribute);
    if ([attribute isEqualToString: NSAccessibilityContentsAttribute]) {
        return textview;
    } else if ([attribute isEqualToString: NSAccessibilityRoleDescriptionAttribute]) {
        return [NSString stringWithFormat: @"Status window%@%@%@. %@", line_request?@", waiting for commands":@"", char_request?@", waiting for a key press":@"", hyper_request?@", waiting for a hyperlink click":@"", [textview accessibilityAttributeValue:NSAccessibilityValueAttribute]];
    } else if ([attribute isEqualToString: NSAccessibilityFocusedAttribute]) {
        //return (id)NO;
        return [NSNumber numberWithBool: self.window.firstResponder == self ||
                self.window.firstResponder == textview];
    } else if ([attribute isEqualToString: NSAccessibilityFocusedUIElementAttribute]) {
        return self.accessibilityFocusedUIElement;
    } else if ([attribute isEqualToString: NSAccessibilityChildrenAttribute]) {
        return @[textview];
    }

    return [super accessibilityAttributeValue: attribute];
}

- (id)accessibilityFocusedUIElement {
    return textview;
}

- (IBAction)speakStatus:(id)sender
{
	if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_9)
	{
		NSDictionary *announcementInfo = @{
										   NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh),
										   NSAccessibilityAnnouncementKey : textstorage.string
										   };
		NSAccessibilityPostNotificationWithUserInfo([NSApp mainWindow], NSAccessibilityAnnouncementRequestedNotification, announcementInfo);
	}
}

@end
