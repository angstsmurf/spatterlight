#import "main.h"

@implementation GlkWindow

- (id) initWithGlkController: (GlkController*)glkctl_ name: (int)name_
{
    [super init];
    
    glkctl = glkctl_;
    name = name_;
    bgnd = 0;
    
    // NSLog(@"new window %d (%@)", name, [self class]);
    
    return self;
}

- (void) dealloc
{
    int i;
    for (i = 0; i < style_NUMSTYLES; i++)
 	[styles[i] release];
    [super dealloc];
}

- (void) setStyle: (int)style windowType: (int)wintype enable: (int*)enable value:(int*)value
{
    int i;

    if (styles[style])
    {
	[styles[style] release];
	styles[style] = 0;
    }

    styles[style] = [[GlkStyle alloc] initWithStyle: style
					 windowType: wintype
					     enable: enable
					      value: value];
}

- (BOOL) isOpaque
{
    return YES;
}

- (void) setFrame: (NSRect)thisframe
{
    NSRect mainframe = [[self superview] frame];
    int hmask, vmask;
    int rgt = 0;
    int bot = 0;
    
    [super setFrame: thisframe];
    
    /* set autoresizing for live resize. */
    /* the client should rearrange after it's finished. */
    /* flex the views connected to the right and bottom */
    /* keep the other views fixed in size */
    /* x and y separable */
    
    if (NSMaxX(thisframe) == NSMaxX(mainframe))
	rgt = 1;
    
    if (NSMaxY(thisframe) == NSMaxY(mainframe))
	bot = 1;
    
    if (rgt)
	hmask = NSViewWidthSizable;
    else
	hmask = NSViewMaxXMargin;
    
    if (bot)
	vmask = NSViewHeightSizable;
    else
	vmask = NSViewMaxYMargin;
    
    [self setAutoresizingMask: hmask | vmask];
}

- (void) prefsDidChange
{
    int i;
    for (i = 0; i < style_NUMSTYLES; i++)
	[styles[i] prefsDidChange];
}

- (void) terpDidStop
{
}

- (BOOL) wantsFocus
{
    return NO;
}

- (void) grabFocus
{
    // NSLog(@"grab focus in window %d", name);
    [[self window] makeFirstResponder: self];
}

- (void) flushDisplay
{
}

- (void) setBgColor: (int)bc
{
    bgnd = bc;
}

- (void) fillRects: (struct fillrect *)rects count: (int)n
{
    NSLog(@"fillrect in %@ not implemented", [self class]);
}

- (void) drawImage: (NSImage*)buf val1: (int)v1 val2: (int)v2 width: (int)w height: (int)h
{
    NSLog(@"drawimage in %@ not implemented", [self class]);
}

- (void) flowBreak
{
    NSLog(@"flowbreak in %@ not implemented", [self class]);
}

- (void) makeTransparent
{
    NSLog(@"makeTransparent in %@ not implemented", [self class]);
}

- (void) markLastSeen { }
- (void) performScroll { }

- (void) clear
{
    NSLog(@"clear in %@ not implemented", [self class]);
}

- (void) putString:(NSString*)buf style:(int)style
{
    NSLog(@"print in %@ not implemented", [self class]);
}

- (void) moveToColumn:(int)x row:(int)y
{
    NSLog(@"move cursor in %@ not implemented", [self class]);
}

- (void) initLine: (NSString*)buf
{
    NSLog(@"line input in %@ not implemented", [self class]);
}

- (NSString*) cancelLine
{
    return @"";
}

- (void) initChar
{
    NSLog(@"char input in %@ not implemented", [self class]);
}

- (void) cancelChar
{
}

- (void) initMouse
{
    NSLog(@"mouse input in %@ not implemented", [self class]);
}

- (void) cancelMouse
{
}

@end
