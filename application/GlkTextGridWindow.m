/*
 * GlkTextGridWindow --
 * We keep an array of attributed strings
 * as our text grid.
 */

#import "main.h"

@implementation GlkTextGridWindow

- (id) initWithGlkController: (GlkController*)glkctl_ name: (int)name_
{
    [super initWithGlkController: glkctl_ name: name_];

    lines = [[NSMutableArray alloc] init];
    input = nil;
    
    rows = 0;
    cols = 0;
    xpos = 0;
    ypos = 0;
    
    line_request = NO;
    char_request = NO;
    mouse_request = NO;
    
    transparent = NO;
    
    return self;
}

- (void) dealloc
{
    if (input)
	[input release];
    [lines release];
    [super dealloc];
}

- (BOOL) isFlipped
{
    return YES;
}

- (void) prefsDidChange
{
    NSRange range;
    int i;
    
    [super prefsDidChange];
    
    /* reassign styles to attributedstrings */
    for (i = 0; i < [lines count]; i++)
    {
	NSMutableAttributedString *line = [lines objectAtIndex: i];
	int x = 0;
	while (x < [line length])
	{
	    id styleobject = [line attribute:@"GlkStyle" atIndex:x effectiveRange:&range];
	    int stylevalue = [styleobject intValue];
	    int style = stylevalue & 0xff;
	    int fg = (stylevalue >> 8) & 0xff;
	    int bg = (stylevalue >> 16) & 0xff;
	    
	    [line setAttributes: [styles[style] attributes] range: range];
	    if (fg || bg)
	    {
		[line addAttribute: @"GlkStyle" value: [NSNumber numberWithInt: stylevalue] range: range];
		if ([Preferences stylesEnabled])
		{
		    if (fg)
			[line addAttribute: NSForegroundColorAttributeName
					    value: [Preferences foregroundColor: fg - 1]
					    range: range];
		    if (bg)
			[line addAttribute: NSBackgroundColorAttributeName
					    value: [Preferences backgroundColor: bg - 1]
					    range: range];
		}
	    }

	    x = range.location + range.length;
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
    return char_request;
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
    NSRange glyphRange;
    NSPoint layoutLocation;
    NSRect textRect;
    
    int m = [Preferences gridMargins];
    if (transparent)
	m = 0;
    
    float x0 = NSMinX(bounds) + m;
    float y0 = NSMinY(bounds) + m;
    
    if (!transparent)
    {
	NSColor *color;
	
	color = nil;
	
	if ([Preferences stylesEnabled])
	{
	    color = [[styles[style_Normal] attributes] objectForKey: NSBackgroundColorAttributeName];
	    if (bgnd != 0)
		color = [Preferences backgroundColor: bgnd - 1];
	}
	
	if (!color)
	    color = [Preferences gridBackground];
	
	[color set];

	NSRectFill(rect);
    }
    
    int y;
    int lineHeight = [Preferences lineHeight];
    int charWidth = [Preferences charWidth];
    
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

	[textStorage setAttributedString: [lines objectAtIndex: y]];
	NSRange glyphRange = [textLayout glyphRangeForTextContainer: textContainer];
	NSPoint layoutLocation = [textLayout locationForGlyphAtIndex: 0];
	NSRect	textRect = [textLayout boundingRectForGlyphRange: glyphRange inTextContainer: textContainer];
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
    
    [textContainer release];
    [textLayout release];
    [textStorage release];
}

- (void) setFrame: (NSRect)frame
{
    int r;
    
    [super setFrame: frame];
    
    int newcols = frame.size.width / [Preferences charWidth];
    int newrows = frame.size.height / [Preferences lineHeight];
    
    if (newcols == cols && newrows == rows)
	return;
    
    cols = newcols;
    rows = newrows;
    
    for (r = [lines count]; r < rows; r++)
    {
	[lines addObject: [[[NSMutableAttributedString alloc] init] autorelease]];
    }
    
    // Remove old lines
    if ([lines count] > rows)
    {
	[lines removeObjectsInRange: NSMakeRange(rows, [lines count] - rows)];
    }
    
    // Size lines
    for (r = 0; r < rows; r++)
    {
	NSMutableAttributedString* line = [lines objectAtIndex: r];
	
	if ([line length] < cols)
	{
	    // Add spaces to the end (not sure about the attributes to use)
	    int amountToAdd = cols - [line length];
	    unichar* spaces = malloc(sizeof(unichar)*amountToAdd);
	    int c;
	    
	    for (c = 0; c < amountToAdd; c++)
	    {
		spaces[c] = ' ';
	    }
	    
	    NSAttributedString* string = [[NSAttributedString alloc]
				initWithString: [NSString stringWithCharacters: spaces length: amountToAdd]
				    attributes: [styles[style_Normal] attributes]];
	    
	    [line appendAttributedString: string];
	    [string release];
	    free(spaces);
	}
	else if ([line length] > cols)
	{
	    [line deleteCharactersInRange: NSMakeRange(cols, [line length] - cols)];
	}
    }
    
    dirty = YES;
}

- (void) moveToColumn:(int)c row:(int)r
{
    xpos = c;
    ypos = r;
}

- (void) clear
{
    [lines release];
    lines = nil;
    lines = [[NSMutableArray alloc] init];
    
    rows = cols = 0;
    xpos = ypos = 0;
    
    [self setFrame: [self frame]];
}

- (void) putString: (NSString*)string style: (int)stylevalue
{
    int length = [string length];
    int pos = 0;
    NSDictionary *att;
    NSColor *col;
    int style;
    int fg;
    int bg;
    
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
    
    style = stylevalue & 0xff;
    fg = (stylevalue >> 8) & 0xff;
    bg = (stylevalue >> 16) & 0xff;
    
    if (fg || bg)
    {
	NSMutableDictionary *mutatt = [[[styles[style] attributes] mutableCopy] autorelease];
	[mutatt setObject: [NSNumber numberWithInt: stylevalue] forKey: @"GlkStyle"];
	if ([Preferences stylesEnabled])
	{
	    if (fg)
		[mutatt setObject: [Preferences foregroundColor: fg - 1] forKey: NSForegroundColorAttributeName];
	    if (bg)
		[mutatt setObject: [Preferences backgroundColor: bg - 1] forKey: NSBackgroundColorAttributeName];
	}
	att = mutatt;
    }
    else
    {
	att = [styles[style] attributes];
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
	int amountToDraw = cols - xpos;
	if (amountToDraw > [string length] - pos)
	    amountToDraw = [string length] - pos;
	
	// "Draw" the characters
	NSAttributedString* partString = [[NSAttributedString alloc]
			initWithString: [string substringWithRange: NSMakeRange(pos, amountToDraw)]
			    attributes: att];
	[[lines objectAtIndex: ypos] replaceCharactersInRange: NSMakeRange(xpos, amountToDraw)
					 withAttributedString: partString];
	[partString release];
	
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

- (void) mouseDown: (NSEvent*)theEvent
{
    GlkEvent *gev;
    
    if (mouse_request && [theEvent clickCount] == 2)
    {
	[glkctl markLastSeen];
	
	NSPoint p;
	p = [theEvent locationInWindow];
	p = [self convertPoint: p fromView: nil];
	p.x = (p.x - [Preferences gridMargins]) / [Preferences charWidth];
	p.y = (p.y - [Preferences gridMargins]) / [Preferences lineHeight];
	if (p.x >= 0 && p.y >= 0 && p.x < cols && p.y < rows)
	{
	    // NSLog(@"mousedown in buf at %g,%g", p.x, p.y);
	    gev = [[GlkEvent alloc] initMouseEvent: p forWindow: name];
	    [glkctl queueEvent: gev];
	    [gev release];
	    mouse_request = NO;
	}
    }
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
    if (![self wantsFocus])
        for (int i = 0; i < MAXWIN; i++)
        {
            win = [glkctl windowWithNum:i];
            if (i != self->name && win && [win wantsFocus])
            {
                NSLog(@"Passing on keypress");
                if ([win isKindOfClass: [GlkTextBufferWindow class]])
                    [win onKeyDown:evt];
                else
                    [win keyDown:evt];
                [win grabFocus];
                return;
            }
        }

    if (char_request && ch != keycode_Unknown)
    {
	[glkctl markLastSeen];

	//NSLog(@"char event from %d", name);
	GlkEvent *gev = [[GlkEvent alloc] initCharEvent: ch forWindow: name];
	[glkctl queueEvent: gev];
	[gev release];
	char_request = NO;
	dirty = YES;
	return;
    }
}

- (void) initLine:(NSString*)str
{
    NSRect bounds = [self bounds];
    int m = [Preferences gridMargins];
    if (transparent)
	m = 0;
    
    int x0 = NSMinX(bounds) + m;
    int y0 = NSMaxY(bounds) - m;
    int lineHeight = [Preferences lineHeight];
    float charWidth = [Preferences charWidth];

    if (ypos >= [lines count])
	ypos = [lines count] - 1;

    NSRect caret;
    caret.origin.x = x0 + xpos * charWidth;
    caret.origin.y = y0 - (ypos + 1) * lineHeight;
    caret.size.width = 20 * charWidth;
    caret.size.height = lineHeight;
    
    NSLog(@"grid initLine: %@ in: %d", str, name);
  
    input = [[NSTextField alloc] initWithFrame: caret];
    [input setEditable: YES];
    [input setBordered: NO];
    [input setStringValue: str];
    [input setAction: @selector(typedEnter:)];
    [input setTarget: self];
    
    [self addSubview: input];
    [[self window] makeFirstResponder: input];
    
    line_request = YES;	
}

- (NSString*) cancelLine
{
    line_request = NO;
    if (input)
    {
	NSString *str = [[input stringValue] retain];
	[self putString: str style: style_Input];
	[input removeFromSuperview];
	[input release];
	input = nil;
	return [str autorelease];
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
	GlkEvent *gev = [[GlkEvent alloc] initLineEvent: str forWindow: name];
	[glkctl queueEvent: gev];
	[gev release];
	[input removeFromSuperview];
	[input release];
	input = nil;
    }    
}

@end
