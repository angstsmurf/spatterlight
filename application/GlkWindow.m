#import "main.h"

@implementation GlkWindow

- (instancetype) initWithGlkController: (GlkController*)glkctl_ name: (NSInteger)name
{
    self = [super initWithFrame:NSZeroRect];

    if (self)
    {
        glkctl = glkctl_;
        _name = name;
        bgnd = 0xFFFFFF; // White
        _pendingTerminators = [[NSMutableDictionary alloc] initWithObjectsAndKeys:
                               @(NO), @keycode_Func1,
                               @(NO), @keycode_Func2,
                               @(NO), @keycode_Func3,
                               @(NO), @keycode_Func4,
                               @(NO), @keycode_Func5,
                               @(NO), @keycode_Func6,
                               @(NO), @keycode_Func7,
                               @(NO), @keycode_Func8,
                               @(NO), @keycode_Func9,
                               @(NO), @keycode_Func10,
                               @(NO), @keycode_Func11,
                               @(NO), @keycode_Func12,
                               @(NO), @keycode_Escape,
                               nil];
        currentTerminators = _pendingTerminators;
        _terminatorsPending = NO;
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

    /* set autoresizing for live resize. */
    /* the client should rearrange after it's finished. */
    /* flex the views connected to the right and bottom */
    /* keep the other views fixed in size */
    /* x and y separable */

    CGFloat border = Preferences.border;

    if (NSMaxX(thisframe) == floor(NSMaxX(mainframe) - border))
        rgt = 1;

    if (NSMaxY(thisframe) == floor(NSMaxY(mainframe) - border))
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
    super.frame = thisframe;
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
    // NSLog(@"grab focus in window %ld", self.name);
    [self.window makeFirstResponder: self];
    NSAccessibilityPostNotification( self, NSAccessibilityFocusedUIElementChangedNotification );
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

- (NSDictionary *) attributesFromStylevalue: (NSInteger)stylevalue
{
    NSInteger style = stylevalue & 0xff;
    NSInteger fg = (stylevalue >> 8) & 0xff;
    NSInteger bg = (stylevalue >> 16) & 0xff;

    if (fg || bg)
    {
        NSMutableDictionary *mutatt = [styles[style].attributes mutableCopy];
        mutatt[@"GlkStyle"] = @(stylevalue);
        if ([Preferences stylesEnabled])
        {
            if (fg)
                mutatt[NSForegroundColorAttributeName] = [Preferences foregroundColor: (int)(fg - 1)];
            if (bg)
                mutatt[NSBackgroundColorAttributeName] = [Preferences backgroundColor: (int)(bg - 1)];
        }
        return (NSDictionary *) mutatt;
    }
    else
    {
        return styles[style].attributes;
    }
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

- (void) setHyperlink: (NSUInteger)linkid;
{
    NSLog(@"hyperlink input in %@ not implemented", [self class]);
}

- (void) initHyperlink
{
    NSLog(@"hyperlink input in %@ not implemented", [self class]);
}

- (void) cancelHyperlink
{
    NSLog(@"hyperlink input in %@ not implemented", [self class]);
}

- (BOOL) hasLineRequest
{
    return NO;
}

#pragma mark Accessibility

- (BOOL)accessibilityIsIgnored {
	return NO;
}

@end
