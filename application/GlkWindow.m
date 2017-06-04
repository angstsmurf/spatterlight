#import "main.h"

@implementation GlkWindow

- (instancetype) initWithFrame:(NSRect)frameRect
{
    return [self initWithGlkController:[[GlkController alloc] init] name:0];
}

- (instancetype) initWithCoder:(NSCoder *)coder
{
    return [self initWithGlkController:[[GlkController alloc] init] name:0];
}

- (instancetype) initWithGlkController: (GlkController*)glkctl_ name: (NSInteger)name
{
    self = [super initWithFrame:NSZeroRect];

    if (self)
    {
        glkctl = glkctl_;
        _name = name;
        //bgnd = 0xFFFFFF; // White
    }
    
    return self;
}

- (void) setStyle: (NSInteger)style windowType: (NSInteger)wintype enable: (NSInteger*)enable value:(NSInteger*)value
{
    if (styles[style])
    {
        styles[style] = 0;
    }
    
    styles[style] = [[GlkStyle alloc] initWithStyle: style
                                         windowType: wintype
                                             enable: enable
                                              value: value];
}

- (BOOL) getStyleVal: (NSInteger)style hint: (NSInteger)hint value:(NSInteger *)value
{
    GlkStyle *checkedStyle = styles[style];
    if(checkedStyle)
    {
        if ([checkedStyle valueForHint:hint value:value])
            return YES;
    }

    return NO;
}

- (BOOL) isOpaque
{
    return YES;
}

- (void) setFrame: (NSRect)thisframe
{
    NSRect mainframe = self.superview.frame;
    NSInteger hmask, vmask;
    NSInteger rgt = 0;
    NSInteger bot = 0;
    
    super.frame = thisframe;
    
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
    
    self.autoresizingMask = hmask | vmask;
}

- (void) prefsDidChange
{
    NSInteger i;
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
    [self.window makeFirstResponder: self];
}

- (void) flushDisplay
{
}

- (void) setBgColor: (NSInteger)bc
{
    NSLog(@"set background color in %@ not allowed", [self class]);
}

- (void) fillRects: (struct fillrect *)rects count: (NSInteger)n
{
    NSLog(@"fillrect in %@ not implemented", [self class]);
}

- (void) drawImage: (NSImage*)buf val1: (NSInteger)v1 val2: (NSInteger)v2 width: (NSInteger)w height: (NSInteger)h
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

- (void) putString:(NSString*)buf style:(NSInteger)style
{
    NSLog(@"print in %@ not implemented", [self class]);
}

- (void) moveToColumn:(NSInteger)x row:(NSInteger)y
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
