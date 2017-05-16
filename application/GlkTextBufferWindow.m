/*
 * NSTextView associated with a glk window
 */

#import "main.h"

/* ------------------------------------------------------------ */

/*
 * Extend NSTextContainer to handle images in the margins,
 * with the text flowing around them like in HTML.
 *
 * TODO: flowbreaks
 */

@interface MarginImage : NSObject
{
    NSImage *image;
    int align;
    int pos;
    NSSize size;
    int brk;
    float brky;
    NSRect bounds;
    int recalc;
}

- (id) initWithImage: (NSImage*)animage align: (int)analign at: (int)apos size: (NSSize)asize;
- (NSImage*) image;
- (int) position;
- (int) alignment;
- (NSRect) boundsWithLayout: (NSLayoutManager*)layout;
- (int) flowBreakAt;
- (void) setFlowBreakAt: (int)fb;

@end

@implementation MarginImage

- (id) initWithImage: (NSImage*)animage align: (int)analign at: (int)apos size: (NSSize)asize;
{
    [super init];
    image = [animage retain];
    [image setFlipped: YES];
    align = analign;
    pos = apos;
    size = asize;
    brk = -1;
    bounds = NSZeroRect;
    recalc = YES;
    return self;
}

- (void) dealloc
{
    [image release];
    [super dealloc];
}

- (NSImage*) image { return image; }
- (int) position { return pos; }
- (int) alignment { return align; }
- (int) flowBreakAt { return brk; }
- (void) setFlowBreakAt: (int)fb { brk = fb; }

- (NSRect) boundsWithLayout: (NSLayoutManager*)layout
{
    NSRange ourglyph;
    NSRange ourline;
    NSRect theline;
    
    if (recalc)
    {
	recalc = NO;	/* don't infiniloop in here, settle for the first result */
	
	bounds = NSZeroRect;
	
	/* force layout and get position of anchor glyph */
	ourglyph = [layout glyphRangeForCharacterRange: NSMakeRange(pos, 1)
				  actualCharacterRange: &ourline];
	theline = [layout lineFragmentRectForGlyphAtIndex: ourglyph.location
					   effectiveRange: nil];
	
	/* set bounds to be at the same line as anchor but in left/right margin */
	if (align == imagealign_MarginRight)
	{
	    bounds = NSMakeRect(NSMaxX(theline) - size.width,
				theline.origin.y,
				size.width,
				size.height);
	}
	else
	{
	    bounds = NSMakeRect(theline.origin.x,
				theline.origin.y,
				size.width,
				size.height);
	}
	
	/* invalidate our fake layout *after* we set the bounds ... to avoid infiniloop */
	[layout invalidateLayoutForCharacterRange: ourline
					   isSoft: NO
			     actualCharacterRange: nil];	
    }
    
    return bounds;
}

- (void) uncacheBounds
{
    recalc = YES;
    bounds = NSZeroRect;
}

@end

/* ------------------------------------------------------------ */

@implementation MarginContainer

- (id) initWithContainerSize: (NSSize)size
{
    [super initWithContainerSize: size];
    
    margins = [[NSMutableArray alloc] init];
    
    return self;
}

- (void) dealloc
{
    [margins release];
    [super dealloc];
}

- (void) clearImages
{
    [margins release];
    margins = [[NSMutableArray alloc] init];
    [[self layoutManager] textContainerChangedGeometry: self];
}

- (void) invalidateLayout
{
    int count, i;
    
    count = [margins count];
    for (i = 0; i < count; i++)
	[[margins objectAtIndex: i] uncacheBounds];
    
    [[self layoutManager] textContainerChangedGeometry: self];
}

- (void) addImage: (NSImage*)image align: (int)align at: (int)top size: (NSSize)size
{
    MarginImage *mi = [[MarginImage alloc] initWithImage: image align: align at: top size: size];
    [margins addObject: mi];
    [mi release];
    [[self layoutManager] textContainerChangedGeometry: self];
}

- (void) flowBreakAt: (int)pos
{
    int count = [margins count];
    if (count)
    {
        MarginImage *mi = [margins objectAtIndex: count - 1];
        if ([mi flowBreakAt] < 0)
        {
            [mi setFlowBreakAt: pos];
            [[self layoutManager] textContainerChangedGeometry: self];
        }
    }
}

- (BOOL) isSimpleRectangularTextContainer
{
    return [margins count] == 0;
}

- (NSRect) lineFragmentRectForProposedRect: (NSRect) proposed
			    sweepDirection: (NSLineSweepDirection) sweepdir
			 movementDirection: (NSLineMovementDirection) movementdir
			     remainingRect: (NSRect*) remaining
{
    MarginImage *image;
    NSRect bounds;
    NSRect rect;
    NSPoint point;
    int brk;
    int count;
    int i;
    
    rect = [super lineFragmentRectForProposedRect: proposed
				   sweepDirection: sweepdir
				movementDirection: movementdir
				    remainingRect: remaining];
    
    count = [margins count];
    for (i = 0; i < count; i++)
    {
	image = [margins objectAtIndex: i];
	bounds = [image boundsWithLayout: [self layoutManager]];
	
	if (NSIntersectsRect(bounds, rect))
	{
	    if ([image alignment] == imagealign_MarginLeft)
		rect.origin.x += bounds.size.width;
	    rect.size.width -= bounds.size.width;
	}
    }
    
    return rect;
}

- (void) drawRect: (NSRect)rect
{
    NSSize inset = [[self textView] textContainerInset];
    MarginImage *image;
    NSSize size;
    NSRect bounds;
    
    int count;
    int i;
    
    count = [margins count];
    for (i = 0; i < count; i++)
    {
	image = [margins objectAtIndex: i];
	bounds = [image boundsWithLayout: [self layoutManager]];
	bounds.origin.x += inset.width;
	bounds.origin.y += inset.height;
	
	if (NSIntersectsRect(bounds, rect))
	{
	    size = [[image image] size];
	    [[image image] drawInRect: bounds
			     fromRect: NSMakeRect(0, 0, size.width, size.height)
			    operation: NSCompositeSourceOver
			     fraction: 1.0];		
	}
    }
}

@end

/* ------------------------------------------------------------ */

/*
 * Extend NSTextView to ...
 *   - call onKeyDown on our TextBuffer object
 *   - draw images with high quality interpolation
 */

@interface MyTextView : NSTextView
{
}
- (void) superKeyDown: (NSEvent*)evt;
@end

@implementation MyTextView

- (void) superKeyDown: (NSEvent*)evt
{
    [super keyDown: evt];
}

- (void) keyDown: (NSEvent*)evt
{
    id view = [self superview];
    while (view && ![view isKindOfClass: [GlkTextBufferWindow class]])
	view = [view superview];
    [(GlkTextBufferWindow*)view onKeyDown: evt];
}

- (void) drawRect: (NSRect)rect
{
    [[NSGraphicsContext currentContext] setImageInterpolation: NSImageInterpolationHigh];
    [super drawRect: rect];
    [(MarginContainer*)[self textContainer] drawRect: rect];
}

@end

/* ------------------------------------------------------------ */

/*
 * Controller for the various objects required in the NSText* mess.
 */

@implementation GlkTextBufferWindow

- (id) initWithGlkController: (GlkController*)glkctl_ name: (int)name_
{
    int margin = [Preferences bufferMargins];
    int i;
    
    [super initWithGlkController: glkctl_ name: name_];
    
    char_request = NO;
    line_request = NO;
    
    fence = 0;
    lastseen = 0;
    lastchar = '\n';
    
    for (i = 0; i < HISTORYLEN; i++)
	history[i] = nil;
    historypos = 0;
    historyfirst = 0;
    historypresent = 0;
    
    scrollview = [[NSScrollView alloc] initWithFrame: NSZeroRect];
    [scrollview setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [scrollview setHasHorizontalScroller: NO];
    [scrollview setHasVerticalScroller: YES];
    [scrollview setBorderType: NSNoBorder];
    
    /* construct text system manually */
    
    textstorage = [[NSTextStorage alloc] init];
    
    layoutmanager = [[NSLayoutManager alloc] init];
    [textstorage addLayoutManager: layoutmanager];
    
    container = [[MarginContainer alloc] initWithContainerSize: NSMakeSize(FLT_MAX, FLT_MAX)];
    [container setLayoutManager: layoutmanager];
    [layoutmanager addTextContainer: container];
    
    textview = [[MyTextView alloc] initWithFrame:NSMakeRect(0, 0, 0, 1000000) textContainer:container];

    [container setTextView: textview];
    
    [scrollview setDocumentView: textview];
    
    /* now configure the text stuff */
    
    [container setWidthTracksTextView: YES];
    [container setHeightTracksTextView: NO];
    
    [textview setHorizontallyResizable: NO];
    [textview setVerticallyResizable: YES];
    [textview setAutoresizingMask: NSViewWidthSizable];
    
    [textview setEditable: YES];
    [textview setAllowsUndo: NO];
    [textview setUsesFontPanel: NO];
    [textview setSmartInsertDeleteEnabled: NO];
    
    [textview setDelegate: self];
    [textstorage setDelegate: self];
    
    [textview setTextContainerInset: NSMakeSize(margin - 3, margin)];
    [textview setBackgroundColor: [Preferences bufferBackground]];
    [textview setInsertionPointColor: [Preferences bufferForeground]];
    
    // disabling screen fonts will force font smoothing and kerning.
    // using screen fonts will render ugly and uneven text and sometimes
    // even bitmapped fonts.
    [layoutmanager setUsesScreenFonts: [Preferences useScreenFonts]];
    
    [self addSubview: scrollview];
    
    return self;
}

- (void) dealloc
{
    int i;
    
    for (i = 0; i < HISTORYLEN; i++)
	[history[i] release];
    
    [textview release];
    [container release];
    [layoutmanager release];
    [textstorage release];
    [scrollview release];
    [super dealloc];
}

- (void) setBgColor: (int)bg
{
    [super setBgColor: bg];
    [self recalcBackground];
}

- (void) recalcBackground
{
    NSColor *bgcolor, *fgcolor;
    
    bgcolor = nil;
    fgcolor = nil;
    
    if ([Preferences stylesEnabled])
    {
	bgcolor = [[styles[style_Normal] attributes] objectForKey: NSBackgroundColorAttributeName];
	fgcolor = [[styles[style_Normal] attributes] objectForKey: NSForegroundColorAttributeName];
	
	if (bgnd != 0)
	{
	    bgcolor = [Preferences backgroundColor: bgnd - 1];
	    if (bgnd == 1) // black
		fgcolor = [Preferences foregroundColor: 7];
	    else
		fgcolor = [Preferences foregroundColor: 0];
	}
    }
	
    if (!bgcolor)
	bgcolor = [Preferences bufferBackground];
    if (!fgcolor)
	fgcolor = [Preferences bufferForeground];
    
    [textview setBackgroundColor: bgcolor];
    [textview setInsertionPointColor: fgcolor];
}

- (void) setStyle: (int)style windowType: (int)wintype enable: (int*)enable value:(int*)value
{
    [super setStyle:style windowType:wintype enable:enable value:value];
    [self recalcBackground];
}

- (void) prefsDidChange
{
    NSRange range, attrange;
    int x;
    
    [super prefsDidChange];
    
    int margin = [Preferences bufferMargins];
    [textview setTextContainerInset: NSMakeSize(margin - 3, margin)];
    [self recalcBackground];
    
    [textstorage removeAttribute: NSBackgroundColorAttributeName
			   range: NSMakeRange(0, [textstorage length])];

    /* reassign attribute dictionaries */
    x = 0;
    while (x < [textstorage length])
    {
	id styleobject = [textstorage attribute:@"GlkStyle" atIndex:x effectiveRange:&range];
	int stylevalue = [styleobject intValue];
	int style = stylevalue & 0xff;
	int fg = (stylevalue >> 8) & 0xff;
	int bg = (stylevalue >> 16) & 0xff;
	
	id image = [textstorage attribute: @"NSAttachment" atIndex:x effectiveRange:NULL];
	if (image)
	    [image retain];
	
	[textstorage setAttributes: [styles[style] attributes] range: range];
	
	if (fg || bg)
	{
	    [textstorage addAttribute: @"GlkStyle" value: [NSNumber numberWithInt: stylevalue] range: range];
	    if ([Preferences stylesEnabled])
	    {
		if (fg)
		    [textstorage addAttribute: NSForegroundColorAttributeName
					value: [Preferences foregroundColor: fg - 1]
					range: range];
		if (bg)
		    [textstorage addAttribute: NSBackgroundColorAttributeName
					value: [Preferences backgroundColor: bg - 1]
					range: range];
	    }
	}
	
	if (image)
	{
	    [textstorage addAttribute: @"NSAttachment"
				value: image
				range: NSMakeRange(x, 1)];
	    [image release];
	}
	
	x = range.location + range.length;
    }
    
    [layoutmanager setUsesScreenFonts: [Preferences useScreenFonts]];
}

- (void) setFrame: (NSRect)frame
{
    if (NSEqualRects(frame, [self frame]))
	return;
    [super setFrame: frame];
    [container invalidateLayout];
}

- (void) didEndSaveAsRTFPanel: (id)panel ret: (int)ret ctx: (void*)ctx
{
    if (ret == NSOKButton)
    {
	if ([textstorage containsAttachments])
	{
	    NSFileWrapper *wrapper;
	    wrapper = [textstorage RTFDFileWrapperFromRange: NSMakeRange(0, [textstorage length])
					 documentAttributes: nil];
	    [wrapper writeToFile: [panel filename]
		      atomically: NO
		 updateFilenames: NO];
	}
	else
	{
	    NSData *data;
	    data = [textstorage RTFFromRange: NSMakeRange(0, [textstorage length])
			  documentAttributes: nil];
	    [data writeToFile: [panel filename] atomically: NO];
	}
    }
}

- (void) saveAsRTF: (id)sender
{
    NSSavePanel *panel = [[NSSavePanel savePanel] retain];
    [panel setTitle: @"Save Scrollback"];
    [panel setNameFieldLabel: @"Save Text: "];
    [panel setRequiredFileType: [textstorage containsAttachments] ? @"rtfd" : @"rtf"];
    [panel beginSheetForDirectory: nil
			     file: nil
		   modalForWindow: [self window]
		    modalDelegate: self
		   didEndSelector: @selector(didEndSaveAsRTFPanel:ret:ctx:)
		      contextInfo: nil];
}

- (NSImage*) scaleImage: (NSImage*)src size: (NSSize)dstsize
{
    NSSize srcsize = [src size];
    NSImage *dst;
    
    if (NSEqualSizes(srcsize, dstsize))
	return src;
    
    dst = [[NSImage alloc] initWithSize: dstsize];
    [dst lockFocus];

    [[NSGraphicsContext currentContext] setImageInterpolation: NSImageInterpolationHigh];

    [src setFlipped: NO];
    
    [src drawInRect: NSMakeRect(0, 0, dstsize.width, dstsize.height)
	   fromRect: NSMakeRect(0, 0, srcsize.width, srcsize.height)
	  operation: NSCompositeSourceOver
	   fraction: 1.0];		

    [src setFlipped: YES];

    [dst unlockFocus];
    
    return [dst autorelease];
}

- (void) drawImage: (NSImage*)image val1: (int)align val2: (int)unused width: (int)w height: (int)h
{
    NSTextAttachment *att;
    NSFileWrapper *wrapper;
    NSData *tiffdata;
    NSAttributedString *attstr;
    
    if (w == 0)
	w = [image size].width;
    if (h == 0)
	h = [image size].height;
    
    if (align == imagealign_MarginLeft || align == imagealign_MarginRight)
    {
	//NSLog(@"adding image to margins");
	unichar uc[1];
	uc[0] = NSAttachmentCharacter;
	[[textstorage mutableString] appendString: [NSString stringWithCharacters: uc length: 1]];
	[container addImage: image align: align at: [textstorage length] - 1 size: NSMakeSize(w, h)];
	[self setNeedsDisplay: YES];
    }
    
    else
    {
	//NSLog(@"adding image to text");
	
	image = [self scaleImage: image size: NSMakeSize(w, h)];
	
	tiffdata = [image TIFFRepresentation];
	wrapper = [[[NSFileWrapper alloc] initRegularFileWithContents: tiffdata] autorelease];
	[wrapper setPreferredFilename: @"image.tiff"];
	att = [[[NSTextAttachment alloc] initWithFileWrapper: wrapper] autorelease];
	attstr = [NSAttributedString attributedStringWithAttachment: att];
	[textstorage appendAttributedString: attstr];
    }
}

- (void) flowBreak
{
    // NSLog(@"adding flowbreak");
    unichar uc[1];
    uc[0] = NSAttachmentCharacter;
    uc[0] = '\n';
    [[textstorage mutableString] appendString: [NSString stringWithCharacters: uc length: 1]];
    [container flowBreakAt: [textstorage length] - 1];
}

- (void) markLastSeen
{
    NSRange glyphs;
    NSRect line;
    
    glyphs = [layoutmanager glyphRangeForTextContainer: container];
    
    if (glyphs.length)
    {
	line = [layoutmanager lineFragmentRectForGlyphAtIndex: glyphs.location + glyphs.length - 1
					       effectiveRange: nil];
	
	lastseen = line.origin.y + line.size.height; // bottom of the line
    }
}

- (void) performScroll
{
    int bottom;
    
    // first, force a layout so we have the correct textview frame
    [layoutmanager glyphRangeForTextContainer: container];
    
    // then, get the bottom
    bottom = [textview frame].size.height;
    
    // scroll so rect from lastseen to bottom is visible
    //NSLog(@"scroll %d -> %d", lastseen, bottom);
    [textview scrollRectToVisible: NSMakeRect(0, lastseen, 0, bottom - lastseen)];
    
    //NSLog(@"perform scroll bottom = %d lastseen = %d", bottom, lastseen);
}

- (void) saveHistory: (NSString*)line
{
    if (history[historypresent])
    {
	[history[historypresent] release];
	history[historypresent] = nil;
    }

    history[historypresent] = [line retain];
    
    historypresent ++;
    if (historypresent >= HISTORYLEN)
	historypresent -= HISTORYLEN;
    
    if (historypresent == historyfirst)
    {
	historyfirst ++;
	if (historyfirst > HISTORYLEN)
	    historyfirst -= HISTORYLEN;
    }
    
    if (history[historypresent])
    {
	[history[historypresent] release];
	history[historypresent] = nil;
    }    
}

- (void) travelBackwardInHistory
{
    NSString *cx;
    
    if (historypos == historyfirst)
	return;
    
    if (historypos == historypresent)
    {
	/* save the edited line */
	if ([textstorage length] - fence > 0)
	    cx = [[textstorage string] substringWithRange: NSMakeRange(fence, [textstorage length] - fence)];
	else
	    cx = nil;
	if (history[historypos])
	    [history[historypos] release];
	history[historypos] = [cx retain];
    }
    
    historypos--;
    if (historypos < 0)
	historypos += HISTORYLEN;
    
    cx = history[historypos];
    if (!cx)
	cx = @"";
    
    [textstorage replaceCharactersInRange: NSMakeRange(fence, [textstorage length] - fence)
			       withString: cx];
}

- (void) travelForwardInHistory
{
    NSString *cx;

    if (historypos == historypresent)
	return;
    
    historypos++;
    if (historypos >= HISTORYLEN)
	historypos -= HISTORYLEN;
    
    cx = history[historypos];
    if (!cx)
	cx = @"";
    
    [textstorage replaceCharactersInRange: NSMakeRange(fence, [textstorage length] - fence)
			       withString: cx];
}

- (void) onKeyDown: (NSEvent*)evt
{
    GlkEvent *gev;
    NSString *str = [evt characters];
    unsigned ch = keycode_Unknown;
    if ([str length])
	ch = chartokeycode([str characterAtIndex: 0]);
    
    // if not scrolled to the bottom, pagedown or navigate scrolling on each key instead
    if (NSMaxY([textview visibleRect]) < NSMaxY([textview bounds]) - 5)
    {
	switch (ch)
	{
	    case keycode_PageUp:
	    case keycode_Delete:
		[textview scrollPageUp: nil];
		return;
	    case keycode_PageDown:
	    case ' ':
		[textview scrollPageDown: nil];
		return;
	    case keycode_Up:
		[textview scrollLineUp: nil];
		return;
	    case keycode_Down:
	    case keycode_Return:
		[textview scrollLineDown: nil];
		return;
	    default:
		[textview scrollRangeToVisible: NSMakeRange([textstorage length], 0)];
		break;
	}
    }
    
    if (char_request && ch != keycode_Unknown)
    {
	NSLog(@"char event from %d", name);
	
	[glkctl markLastSeen];
	
	gev = [[GlkEvent alloc] initCharEvent: ch forWindow: name];
	[glkctl queueEvent: gev];
	[gev release];
	
	char_request = NO;
	[textview setEditable: NO];
    }
    
    else if (line_request && ch == keycode_Return)
    {
	NSLog(@"line event from %d", name);
	
	[glkctl markLastSeen];
	
	NSString *line = [[textstorage string] substringWithRange: NSMakeRange(fence, [textstorage length] - fence)];
	[self putString: @"\n" style: style_Input]; // XXX arranger lastchar needs to be set
	lastchar = '\n';
	
	if ([line length] > 0)
	{
	    [self saveHistory: line];
	}
	
	gev = [[GlkEvent alloc] initLineEvent: line forWindow: name];
	[glkctl queueEvent: gev];
	[gev release];
	
	fence = [textstorage length];
	line_request = NO;
	[textview setEditable: NO];
    }
    
    else if (line_request && ch == keycode_Up)
    {
	[self travelBackwardInHistory];
    }
    
    else if (line_request && ch == keycode_Down)
    {
	[self travelForwardInHistory];
    }
    
    else
    {
	[(MyTextView*)textview superKeyDown: evt];
    }
}

- (BOOL) textView: (NSTextView*)textview_ clickedOnLink: (id)link atIndex: (unsigned)charIndex
{
    NSLog(@"txtbuf: clicked on link: %@", link);
}

- (void) grabFocus
{
    [[self window] makeFirstResponder: textview];
}

- (void) clear
{
    id att = [[NSAttributedString alloc] initWithString: @""];
    [textstorage setAttributedString: att];
    [att release];
    fence = 0;
    lastseen = 0;
    lastchar = '\n';
    [container clearImages];
}

- (void) clearScrollback: (id)sender
{
    NSString *string = [textstorage string];
    int length = [string length];
    int save_request = line_request;
    int prompt;
    int i;
    
    if (!line_request)
	fence = [string length];
    
    /* try to rescue prompt line */
    for (i = 0; i < length; i++)
    {
	if ([string characterAtIndex: length - i - 1] == '\n')
	    break;
    }
    if (i < length)
	prompt = i;
    else
	prompt = 0;
    
    line_request = NO;
    
    [textstorage replaceCharactersInRange: NSMakeRange(0, fence - prompt) withString: @""];
    lastseen = 0;
    lastchar = '\n';
    fence = prompt;
    
    line_request = save_request;
    
    [container clearImages];
}

- (void) putString:(NSString*)str style:(int)stylevalue
{
    NSAttributedString *attstr;
    NSDictionary *att;
    NSColor *col;

    int style = stylevalue & 0xff;
    int fg = (stylevalue >> 8) & 0xff;
    int bg = (stylevalue >> 16) & 0xff;
    
    if (fg || bg)
    {
	NSMutableDictionary *mutatt = [[[styles[style] attributes] mutableCopy] autorelease];
	//if (linkid)
	//    [mutatt setObject: [NSNumber numberWithInt: linkid] forKey: @"GlkLink"];
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
    
    attstr = [[NSAttributedString alloc] initWithString: str attributes: att];
    [textstorage appendAttributedString: attstr];
    [attstr release];
    
    lastchar = [str characterAtIndex: [str length] - 1];
}

- (int) lastchar
{
    return lastchar;
}

- (void) initChar
{
    //NSLog(@"init char in %d", name);
    
    // [glkctl performScroll];
    
    fence = [textstorage length];
    
    char_request = YES;
    [textview setEditable: YES];
    
    [textview setSelectedRange: NSMakeRange(fence, 0)];
}

- (void) cancelChar
{
    //NSLog(@"cancel char in %d", name);
    
    char_request = NO;
    [textview setEditable: NO];
}

- (void) initLine:(NSString*)str
{
    //NSLog(@"initLine: %@ in: %d", str, name);
    
    historypos = historypresent;
    
    // [glkctl performScroll];
    
    if (lastchar == '>' && [Preferences spaceFormat])
    {
	[self putString: @" " style: style_Normal];
    }
    
    fence = [textstorage length];
    
    id att = [[NSAttributedString alloc] initWithString: str
					     attributes: [styles[style_Input] attributes]];	
    [textstorage appendAttributedString: att];
    [att release];
    
    [textview setEditable: YES];
    line_request = YES;	
    
    [textview setSelectedRange: NSMakeRange([textstorage length], 0)];
}

- (NSString*) cancelLine
{
    NSString *str = [textstorage string];
    str = [str substringWithRange: NSMakeRange(fence, [str length] - fence)];
    [textview setEditable: NO];
    line_request = NO;
    lastchar = [str characterAtIndex: [str length] - 1];
    return str;
}

- (void) terpDidStop
{
    [textview setEditable: NO];
    // [self performScroll];
}

- (BOOL) wantsFocus
{
    return char_request || line_request;
}

- (BOOL) textView: (NSTextView*)aTextView
        shouldChangeTextInRange: (NSRange)range
	      replacementString: (id)repl
{
    if (line_request && range.location >= fence)
	return YES;
    return NO;
}

- (void) textStorageWillProcessEditing: (NSNotification*)note
{
    if (!line_request)
	return;
    
    if ([textstorage editedRange].location < fence)
	return;
    
    [textstorage setAttributes: [styles[style_Input] attributes]
			 range: [textstorage editedRange]];
}

- (NSRange) textView: (NSTextView *)aTextView
	willChangeSelectionFromCharacterRange: (NSRange)oldrange
    toCharacterRange:(NSRange)newrange
{
    if (line_request)
    {
	if (newrange.length == 0)
	    if (newrange.location < fence)
		newrange.location = fence;
    }
    else
    {
	if (newrange.length == 0)
	    newrange.location = [textstorage length];
    }
    return newrange;
}

@end
