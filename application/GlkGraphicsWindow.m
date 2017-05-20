#import "main.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...) fprintf(stderr,"%s\n", [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@implementation GlkGraphicsWindow

- (id) initWithGlkController: (GlkController*)glkctl_ name: (int)name_
{
    [super initWithGlkController: glkctl_ name: name_];
    
    image = [[NSImage alloc] initWithSize: NSZeroSize];

    mouse_request = NO;
    
    transparent = NO;
    
    return self;
}

- (void) dealloc
{
    [image release];
    [super dealloc];
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

- (void) drawRect: (NSRect)rect
{
    NSRect bounds = [self bounds];
    
    if (!transparent)
    {
	[[NSColor whiteColor] set];
	NSRectFill(rect);
    }
    
    [image drawAtPoint: bounds.origin
	      fromRect: NSMakeRect(0, 0, bounds.size.width, bounds.size.height)
	     operation: NSCompositeSourceOver
	      fraction: 1.0];
}

- (void) setFrame: (NSRect)frame
{
    int w, h;
    
    if (NSEqualRects(frame, [self frame]))
	return;
    
    [super setFrame: frame];
    
    w = frame.size.width;
    h = frame.size.height;
    
    if (w == 0 || h == 0)
	return;
    
    [image setSize: NSMakeSize(w, h)];
    [image recache];
    
    dirty = YES;
}

- (void) fillRects: (struct fillrect *)rects count: (int)count
{
    NSBitmapImageRep *bitmap;
    NSSize size;
    int x, y;
    int i;
    
    size = [image size];
    
    if (size.width == 0 || size.height == 0)
	return;
    
    bitmap = [[NSBitmapImageRep alloc]
		initWithBitmapDataPlanes: NULL
			      pixelsWide: size.width
			      pixelsHigh: size.height
			   bitsPerSample: 8
			 samplesPerPixel: 4
				hasAlpha: YES
				isPlanar: NO
			  colorSpaceName: NSCalibratedRGBColorSpace
			     bytesPerRow: 0
			    bitsPerPixel: 32];
    
    [bitmap setSize: size];
    
    unsigned char *pd = [bitmap bitmapData];
    int ps = [bitmap bytesPerRow];
    int pw = [bitmap pixelsWide];
    int ph = [bitmap pixelsHigh];
    
    memset(pd, 0x00, ps * ph);
    
    for (i = 0; i < count; i++)
    {
	unsigned char ca = ((rects[i].color >> 24) & 0xff);
	unsigned char cr = ((rects[i].color >> 16) & 0xff);
	unsigned char cg = ((rects[i].color >> 8) & 0xff);
	unsigned char cb = ((rects[i].color >> 0) & 0xff);
	
	int rx0 = rects[i].x;
	int ry0 = rects[i].y;
	int rx1 = rx0 + rects[i].w;
	int ry1 = ry0 + rects[i].h;
	
	if (ry0 < 0) ry0 = 0;
	if (ry1 < 0) ry1 = 0;
	if (rx0 < 0) rx0 = 0;
	if (rx1 < 0) rx1 = 0;
	
	if (ry0 > ph) ry0 = ph;
	if (ry1 > ph) ry1 = ph;
	if (rx0 > pw) rx0 = pw;
	if (rx1 > pw) rx1 = pw;
	
	for (y = ry0; y < ry1; y++)
	{
	    unsigned char *p = pd + (y * ps) + (rx0 * 4);
	    for (x = rx0; x < rx1; x++)
	    {
		*p++ = cr;
		*p++ = cg;
		*p++ = cb;
		*p++ = ca;
	    }
	}
    }
    
    [image lockFocus];
    {
	NSImage *tmp = [[NSImage alloc] initWithSize: size];
	[tmp addRepresentation: bitmap];
	[tmp drawAtPoint: NSZeroPoint
		fromRect: NSMakeRect(0, 0, size.width, size.height)
	       operation: NSCompositeSourceOver
		fraction: 1.0];		
	[tmp release];
    }
    [image unlockFocus];
    
    [bitmap release];
    
    dirty = YES;
}

- (NSRect) florpCoords: (NSRect) r
{
    NSRect res = r;
    NSSize size = [image size];
    res.origin.y = size.height - res.origin.y - res.size.height;
    return res;
}

- (void) drawImage: (NSImage*)src val1: (int)x val2: (int)y width: (int)w height: (int)h
{
    NSSize srcsize = [src size];
    
    if (w == 0)
	w = srcsize.width;
    if (h == 0)
	h = srcsize.height;
    
    //NSLog(@"  drawimage in gfx x=%d y=%d w=%d h=%d\n", x, y, w, h);
    
    [image lockFocus];
    
    [[NSGraphicsContext currentContext] setImageInterpolation: NSImageInterpolationHigh];
    
    [src drawInRect: [self florpCoords: NSMakeRect(x, y, w, h)]
	   fromRect: NSMakeRect(0, 0, srcsize.width, srcsize.height)
	  operation: NSCompositeSourceOver
	   fraction: 1.0];
    
    [image unlockFocus];
    
    dirty = YES;
}

- (void) flushDisplay
{
    if (dirty)
	[self setNeedsDisplay: YES];
    dirty = NO;
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
    if (mouse_request && [theEvent clickCount] == 1)
    {
	[glkctl markLastSeen];

	NSPoint p;
	p = [theEvent locationInWindow];
	p = [self convertPoint: p fromView: nil];
	p.y = [self frame].size.height - p.y;
	//NSLog(@"mousedown in gfx at %g,%g", p.x, p.y);
	GlkEvent *gev = [[GlkEvent alloc] initMouseEvent: p forWindow: name];
	[glkctl queueEvent: gev];
	[gev release];
	mouse_request = NO;
    }
}

- (BOOL) wantsFocus
{
    return char_request;
}

- (BOOL) acceptsFirstResponder
{
    return YES;
}

- (void) initChar
{
    //NSLog(@"init char in %d", name);
    char_request = YES;
}

- (void) cancelChar
{
    //NSLog(@"cancel char in %d", name);
    char_request = NO;
}

- (void) keyDown: (NSEvent*)evt
{
    NSString *str = [evt characters];
    unsigned ch = keycode_Unknown;
    if ([str length])
	ch = chartokeycode([str characterAtIndex: 0]);
    
    if (char_request && ch != keycode_Unknown)
    {
	[glkctl markLastSeen];
	
	//NSLog(@"char event from %d", name);
	GlkEvent *gev = [[GlkEvent alloc] initCharEvent: ch forWindow: name];
	[glkctl queueEvent: gev];
	[gev release];
	char_request = NO;
	return;
    }
}

@end
