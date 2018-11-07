
/*
 * GlkTextGridWindow --
 * We keep an array of attributed strings
 * as our text grid.
 */

#import "main.h"
#import "GlkHyperlink.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...) fprintf(stderr,"%s\n", [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@implementation GlkTextGridWindow

- (instancetype) initWithGlkController: (GlkController*)glkctl_ name: (NSInteger)name_
{
    self = [super initWithGlkController: glkctl_ name: name_];

    if (self)
    {
        lines = [[NSMutableArray alloc] init];
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
    }
    return self;
}


- (BOOL) isFlipped
{
    return YES;
}

- (void) prefsDidChange
{
	NSRange range = NSMakeRange(0, 0);
	NSRange linkrange= NSMakeRange(0, 0);

	int i;

	[super prefsDidChange];

    /* reassign styles to attributedstrings */
    for (i = 0; i < [lines count]; i++)
    {
        NSMutableAttributedString *line = lines[i];
        int x = 0;
        while (x < [line length])
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
	}

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

- (void) drawRect: (NSRect)rect
{
    NSRect bounds = [self bounds];
    NSColor *color;

    //NSRange glyphRange;
    //NSPoint layoutLocation;
    //NSRect textRect;

    NSInteger m = [Preferences gridMargins];
    if (transparent)
        m = 0;

    float x0 = NSMinX(bounds) + m;
    float y0 = NSMinY(bounds) + m;

    if (!transparent)
    {

        color = nil;

        if ([Preferences stylesEnabled])
        {
            color = [styles[style_Normal] attributes][NSBackgroundColorAttributeName];
        }

        if (!color)
            color = [Preferences gridBackground];

        [color set];

        NSRectFill(bounds);
    }

    NSInteger y;
    NSInteger lineHeight = [Preferences lineHeight];
    //NSInteger charWidth = [Preferences charWidth];

    NSTextStorage *textStorage = [[NSTextStorage alloc] init];
    NSLayoutManager *textLayout = [[NSLayoutManager alloc] init];
    NSTextContainer *textContainer = [[NSTextContainer alloc] init];

    [textLayout addTextContainer: textContainer];
    [textStorage addLayoutManager: textLayout];
    [textLayout setUsesScreenFonts: [Preferences useScreenFonts]];

    /* draw from bottom up because solid backgrounds overdraw descenders... */
    for (y = [lines count] - 1; y >= 0; y--)
    {
        // [attstr drawAtPoint: NSMakePoint(x0, y0 + y * lineHeight)]; -- if it were only this simple.

        [textStorage setAttributedString: lines[y]];
        NSRange glyphRange = [textLayout glyphRangeForTextContainer: textContainer];
        NSPoint layoutLocation = [textLayout locationForGlyphAtIndex: 0];
        //NSRect	textRect = [textLayout boundingRectForGlyphRange: glyphRange inTextContainer: textContainer];
        [textLayout drawBackgroundForGlyphRange: glyphRange
                                        atPoint: NSMakePoint(x0 - layoutLocation.x, y0 + y * lineHeight)];
        [textLayout drawGlyphsForGlyphRange: glyphRange
                                    atPoint: NSMakePoint(x0 - layoutLocation.x, y0 + y * lineHeight)];

#if 0
        if (ypos == y && char_request && [[self window] firstResponder] == self)
        {
            NSRect caret;
            NSPoint caretLocation = [textLayout locationForGlyphAtIndex: xpos];
            caret.origin.x = (int)(x0 - layoutLocation.x + caretLocation.x + 0.5);
            caret.origin.y = y0 + ypos * lineHeight;
            caret.size.width = 1;
            caret.size.height = lineHeight;
            [[Preferences gridForeground] set];
            NSRectFill(caret);
        }
#endif
    }

    //Hack to cover stray characters that sometimes get
    //stuck outside the window margin
    NSRect frame = rect;
    frame.size.width = [Preferences gridMargins];

    frame.origin.x = NSMaxX(self.bounds) - [Preferences gridMargins];

    [color set];

    NSRectFill(frame);
}

- (void) setFrame: (NSRect)frame
{
    NSInteger r;

    [super setFrame: frame];

    NSInteger newcols = floor (frame.size.width / [Preferences charWidth]) - 1;
    NSInteger newrows = frame.size.height / [Preferences lineHeight];

    if (newcols == cols && newrows == rows)
        return;

    cols = newcols;
    rows = newrows;

    for (r = [lines count]; r < rows; r++)
    {
        [lines addObject: [[NSMutableAttributedString alloc] init]];
    }

    // Remove old lines
    if ([lines count] > rows)
    {
        [lines removeObjectsInRange: NSMakeRange(rows, [lines count] - rows)];
    }

    // Size lines
    for (r = 0; r < rows; r++)
    {
        NSMutableAttributedString* line = lines[r];

        if ([line length] < cols)
        {
            // Add spaces to the end (not sure about the attributes to use)
            NSInteger amountToAdd = cols - [line length];
            unichar* spaces = malloc(sizeof(unichar)*amountToAdd);
            NSInteger c;

            for (c = 0; c < amountToAdd; c++)
            {
                spaces[c] = ' ';
            }

            NSAttributedString* string = [[NSAttributedString alloc]
                                          initWithString: [NSString stringWithCharacters: spaces length: amountToAdd]
                                          attributes: styles[style_Normal].attributes];

            [line appendAttributedString: string];
            free(spaces);
        }
        else if ([line length] > cols)
        {
            [line deleteCharactersInRange: NSMakeRange(cols, [line length] - cols)];
        }
    }

    dirty = YES;
}

- (void) moveToColumn:(NSInteger)c row:(NSInteger)r
{
    xpos = c;
    ypos = r;
}

- (void) clear
{
    lines = nil;
    lines = [[NSMutableArray alloc] init];

	hyperlinks = nil;
	hyperlinks = [[NSMutableArray alloc] init];

    rows = cols = 0;
    xpos = ypos = 0;

    [self setFrame: [self frame]];
}

- (void) putString: (NSString*)string style: (NSInteger)stylevalue
{
    NSInteger length = [string length];
    NSInteger pos = 0;
    NSDictionary *att = [self attributesFromStylevalue:stylevalue];
    //NSColor *col;

    // Check for newlines
    int x;
    for (x = 0; x < length; x++)
    {
        if ([string characterAtIndex: x] == '\n' || [string characterAtIndex: x] == '\r')
        {
            [self putString: [string substringToIndex: x] style: stylevalue];
            xpos = 0;
            ypos++;
            [self putString: [string substringFromIndex: x + 1] style: stylevalue];
            return;
        }
    }

    // Write this string
    while (pos < length)
    {
        // Can't write if we've fallen off the end of the window
        if (ypos >= [lines count] || ypos > rows)
            break;

        // Can only write a certain number of characters
        if (xpos >= cols)
        {
            xpos = 0;
            ypos ++;
            continue;
        }

        // Get the number of characters to write
        NSInteger amountToDraw = cols - xpos;
        if (amountToDraw > [string length] - pos)
            amountToDraw = [string length] - pos;

        // "Draw" the characters
        NSAttributedString* partString = [[NSAttributedString alloc]
                                          initWithString: [string substringWithRange: NSMakeRange(pos, amountToDraw)]
                                          attributes: att];

        [lines[ypos] replaceCharactersInRange: NSMakeRange(xpos, amountToDraw) withAttributedString: partString];

        dirty = YES;

        // Update the x position (and the y position if necessary)
        xpos += amountToDraw;
        pos += amountToDraw;
        if (xpos >= cols)
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

- (void) setHyperlink:(NSInteger)linkid
{
	NSLog(@"txtgrid: hyperlink %ld set", (long)linkid);

	NSUInteger length = ypos * cols + xpos;

	if (currentHyperlink && currentHyperlink.index != linkid)
	{
//		NSLog(@"There is a preliminary hyperlink, with index %ld", currentHyperlink.index);
		if (currentHyperlink.startpos >= length)
		{
//			NSLog(@"The preliminary hyperlink started at the end of current input, so it was deleted. currentHyperlink.startpos == %ld, length == %ld", currentHyperlink.startpos, length);
			currentHyperlink = nil;
		}
		else
		{
			currentHyperlink.range = NSMakeRange(currentHyperlink.startpos, length - currentHyperlink.startpos);

			[hyperlinks addObject:currentHyperlink];

			NSNumber *link = @(currentHyperlink.index);

			NSInteger pos = currentHyperlink.startpos;
			NSInteger currentx = currentHyperlink.startpos % cols;
			NSInteger currenty = currentHyperlink.startpos / cols;

			while (pos < length)
			{
				// Can't write if we've fallen off the end of the window
				if (currenty >= lines.count || currenty > rows)
					break;

				// Can only write a certain number of characters
				if (currentx >= cols)
				{
					currentx = 0;
					currenty ++;
					continue;
				}

				// Get the number of characters to write
				NSInteger amountToDraw = cols - currentx;
				if (amountToDraw > length - pos)
					amountToDraw = length - pos;

				// Make characters hyperlink
				[lines[currenty] addAttribute:NSLinkAttributeName value:link range:NSMakeRange(currentx, amountToDraw)];
                [lines[currenty] addAttribute:NSUnderlineStyleAttributeName value:@(NSUnderlineStyleSingle) range:NSMakeRange(currentx, amountToDraw)];

				dirty = YES;

				// Update the x position (and the y position if necessary)
				currentx += amountToDraw;
				pos += amountToDraw;
				if (currentx >= cols)
				{
					currentx = 0;
					currenty++;
				}
			}
			currentHyperlink = nil;
		}
	}
	if (!currentHyperlink && linkid)
	{
		currentHyperlink = [[GlkHyperlink alloc] initWithIndex:linkid andPos:length];
//		NSLog(@"New preliminary hyperlink started at position %ld, with link index %ld", currentHyperlink.startpos,linkid);

	}
}

- (void) initHyperlink
{
	hyper_request = YES;
//	NSLog(@"txtgrid: hyperlink event requested");

}

- (void) cancelHyperlink
{
	hyper_request = NO;
//	NSLog(@"txtgrid: hyperlink event cancelled");

}

//- (BOOL) textView: (NSTextView*)textview_ clickedOnLink: (id)link atIndex: (NSUInteger)charIndex
//{
//	NSLog(@"txtgrid: clicked on link: %@", link);
//
//	if (!hyper_request)
//	{
//		NSLog(@"txtgrid: No hyperlink request in window.");
//		return NO;
//	}
//
//	GlkEvent *gev = [[GlkEvent alloc] initLinkEvent:((NSNumber *)link).unsignedIntegerValue forWindow: self.name];
//	[glkctl queueEvent: gev];
//
//	hyper_request = NO;
//	[textview_ setEditable: YES];
//	return NO;
//}

- (void) mouseDown: (NSEvent*)theEvent
{
    GlkEvent *gev;
	GlkHyperlink *hyp;
	NSLog(@"mousedown in grid window");

    if ((mouse_request || hyper_request)) // && theEvent.clickCount == 2)
    {
        [glkctl markLastSeen];

        NSPoint p;
        p = [theEvent locationInWindow];
        p = [self convertPoint: p fromView: nil];
        p.x = (p.x - [Preferences gridMargins]) / [Preferences charWidth];
        p.y = (p.y - [Preferences gridMargins]) / [Preferences lineHeight];
        if (p.x >= 0 && p.y >= 0 && p.x < cols && p.y < rows)
        {
			if (hyper_request)
			{
				for (hyp in hyperlinks)
				{
					if (NSLocationInRange(floor(p.y) * cols + floor(p.x), hyp.range))
					{
						gev = [[GlkEvent alloc] initLinkEvent:hyp.index forWindow:self.name];
						[glkctl queueEvent: gev];
						hyper_request = NO;
						return;
					}
				}
			}

			if (mouse_request) //&& theEvent.clickCount == 2)
			{
				gev = [[GlkEvent alloc] initMouseEvent: p forWindow: self.name];
				[glkctl queueEvent: gev];
				mouse_request = NO;
			}
        }
	} else { NSLog(@"No hyperlink request or mouse request in grid window");
		[super mouseDown:theEvent]; }
}

- (void) initChar
{
    //NSLog(@"init char in %d", name);
    char_request = YES;
    dirty = YES;
}

- (void) cancelChar
{
    //NSLog(@"cancel char in %d", name);
    char_request = NO;
    dirty = YES;
}

- (void) keyDown: (NSEvent*)evt
{
    NSString *str = [evt characters];
    unsigned ch = keycode_Unknown;
    if ([str length])
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

        //NSLog(@"char event from %d", name);
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

    NSRect bounds = [self bounds];
    NSInteger m = [Preferences gridMargins];
    if (transparent)
        m = 0;

    NSInteger x0 = NSMinX(bounds) + m;
    NSInteger y0 = NSMinY(bounds) + m;
    NSInteger lineHeight = [Preferences lineHeight];
    float charWidth = [Preferences charWidth];

    if (ypos >= [lines count])
        ypos = [lines count] - 1;

    NSRect caret;
    caret.origin.x = x0 + xpos * charWidth;
    caret.origin.y = y0 + ypos * lineHeight;
    caret.size.width = 20 * charWidth;
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

	[[input cell] setWraps:YES];
	[[input cell] setScrollable:YES];

	if (str.length == 0)
		str = @" ";

	NSAttributedString *attString = [[NSAttributedString alloc] initWithString:str attributes:[self attributesFromStylevalue:style_Input]];

	input.attributedStringValue = attString;

    [self addSubview: input];
    [[self window] makeFirstResponder: input];

    line_request = YES;
}

- (NSString*) cancelLine
{
    line_request = NO;
    if (input)
    {
        NSString *str = [input stringValue];
        [self putString: str style: style_Input];
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

        NSString *str = [input stringValue];
        [self putString: str style: style_Input];
        GlkEvent *gev = [[GlkEvent alloc] initLineEvent: str forWindow: self.name];
        [glkctl queueEvent: gev];
        [input removeFromSuperview];
        input = nil;
    }
}

@end
