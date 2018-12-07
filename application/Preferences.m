#import "main.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...) fprintf(stderr,"%s\n", [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@implementation Preferences

/*
 * Preference variables, all unpacked
 */

static int defscreenw = 80;
static int defscreenh = 24;
static float cellw = 5;
static float cellh = 5;

static int smartquotes = YES;
static int spaceformat = 0;
static int dographics = YES;
static int dosound = NO;
static int dostyles = NO;
static int usescreenfonts = NO;

static int gridmargin = 0;
static int buffermargin = 0;
static float leading = 0;		/* added to lineHeight */

static NSFont *bufroman;
static NSFont *gridroman;
static NSFont *inputfont;

static NSColor *bufferbg, *bufferfg;
static NSColor *gridbg, *gridfg;
static NSColor *inputfg;

static NSColor *fgcolor[8];
static NSColor *bgcolor[8];

static NSDictionary *bufferatts[style_NUMSTYLES];
static NSDictionary *gridatts[style_NUMSTYLES];

/*
 * Some color utility functions
 */

NSData *colorToData(NSColor *color)
{
    NSData *data;
    CGFloat r = 0, g = 0, b = 0, a = 0;
    unsigned char buf[3];

    color = [color colorUsingColorSpaceName: NSCalibratedRGBColorSpace];

    [color getRed:&r green:&g blue:&b alpha:&a];

    buf[0] = (int)(r * 255);
    buf[1] = (int)(g * 255);
    buf[2] = (int)(b * 255);

    data = [NSData dataWithBytes: buf length: 3];

    return data;
}

NSColor *dataToColor(NSData *data)
{
    NSColor *color;
    CGFloat r, g, b;
    const unsigned char *buf = [data bytes];

    if ([data length] < 3)
        r = g = b = 0;
    else
    {
        r = buf[0] / 255.0;
        g = buf[1] / 255.0;
        b = buf[2] / 255.0;
    }

    color = [NSColor colorWithCalibratedRed:r green:g blue:b alpha:1.0];

    return color;
}

static NSColor *makehsb(CGFloat h, CGFloat s, CGFloat b)
{
    return [NSColor colorWithCalibratedHue: h
                                 saturation: s
                                 brightness: b
                                      alpha: 1.0];
}

/*
 * Load and save defaults
 */

+ (void) initFactoryDefaults
{
    NSString *filename = [[NSBundle mainBundle] pathForResource: @"Defaults" ofType: @"plist"];
    NSMutableDictionary *defaults = [NSMutableDictionary dictionaryWithContentsOfFile: filename];

    defaults[@"GameDirectory"] = [@"~/Documents" stringByExpandingTildeInPath];
    defaults[@"SaveDirectory"] = [@"~/Documents" stringByExpandingTildeInPath];

    [[NSUserDefaults standardUserDefaults] registerDefaults: defaults];
}

+ (void) readDefaults
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *name;
    float size;

    defscreenw = [[defaults objectForKey: @"DefaultWidth"] intValue];
    defscreenh = [[defaults objectForKey: @"DefaultHeight"] intValue];

    smartquotes = [[defaults objectForKey: @"SmartQuotes"] intValue];
    spaceformat = [[defaults objectForKey: @"SpaceFormat"] intValue];

    dographics = [[defaults objectForKey: @"EnableGraphics"] intValue];
    dosound = [[defaults objectForKey: @"EnableSound"] intValue];
    dostyles = [[defaults objectForKey: @"EnableStyles"] intValue];
    usescreenfonts = [[defaults objectForKey: @"ScreenFonts"] intValue];

    gridbg = dataToColor([defaults objectForKey: @"GridBackground"]);
    gridfg = dataToColor([defaults objectForKey: @"GridForeground"]);
    bufferbg = dataToColor([defaults objectForKey: @"BufferBackground"]);
    bufferfg = dataToColor([defaults objectForKey: @"BufferForeground"]);
    inputfg = dataToColor([defaults objectForKey: @"InputColor"]);

    gridmargin = [[defaults objectForKey: @"GridMargin"] floatValue];
    buffermargin = [[defaults objectForKey: @"BufferMargin"] floatValue];

    leading = [[defaults objectForKey: @"Leading"] floatValue];

    name = [defaults objectForKey: @"GridFontName"];
    size = [[defaults objectForKey: @"GridFontSize"] floatValue];
    gridroman = [NSFont fontWithName: name size: size];
    if (!gridroman)
    {
        NSLog(@"pref: failed to create grid font '%@'", name);
        gridroman = [NSFont userFixedPitchFontOfSize: 0];
    }

    name = [defaults objectForKey: @"BufferFontName"];
    size = [[defaults objectForKey: @"BufferFontSize"] floatValue];
    bufroman = [NSFont fontWithName: name size: size];
    if (!bufroman)
    {
        NSLog(@"pref: failed to create buffer font '%@'", name);
        bufroman = [NSFont userFontOfSize: 0];
    }

    name = [defaults objectForKey: @"InputFontName"];
    size = [[defaults objectForKey: @"InputFontSize"] floatValue];
    inputfont = [NSFont fontWithName: name size: size];
    if (!inputfont)
    {
        NSLog(@"pref: failed to create input font '%@'", name);
        inputfont = [NSFont userFontOfSize: 0];
    }
}

+ (void) initialize
{
    NSInteger i;

    [self initFactoryDefaults];
    [self readDefaults];

    for (i = 0; i < style_NUMSTYLES; i++)
    {
        bufferatts[i] = nil;
        gridatts[i] = nil;
    }

    /* 0=black, 1=red, 2=green, 3=yellow, 4=blue, 5=magenta, 6=cyan, 7=white */

    /* black */
    bgcolor[0] = makehsb(0, 0, 0.2);
    fgcolor[0] = makehsb(0, 0, 0.0);

    /* white */
    bgcolor[7] = makehsb(0, 0, 1.0);
    fgcolor[7] = makehsb(0, 0, 0.8);

    /* hues go from red, orange, yellow, green, cyan, blue, magenta, red */
    /* foreground: 70% sat 30% bright */
    /* background: 60% sat 90% bright */

    bgcolor[1] = makehsb(  0/360.0, 0.8, 0.4); /* red */
    bgcolor[2] = makehsb(120/360.0, 0.8, 0.4); /* green */
    bgcolor[3] = makehsb( 60/360.0, 0.8, 0.4); /* yellow */
    bgcolor[4] = makehsb(230/360.0, 0.8, 0.4); /* blue */
    bgcolor[5] = makehsb(300/360.0, 0.8, 0.4); /* magenta */
    bgcolor[6] = makehsb(180/360.0, 0.8, 0.4); /* cyan */

    fgcolor[1] = makehsb(  0/360.0, 0.8, 0.8); /* red */
    fgcolor[2] = makehsb(120/360.0, 0.8, 0.8); /* green */
    fgcolor[3] = makehsb( 60/360.0, 0.8, 0.8); /* yellow */
    fgcolor[4] = makehsb(230/360.0, 0.8, 0.8); /* blue */
    fgcolor[5] = makehsb(300/360.0, 0.8, 0.8); /* magenta */
    fgcolor[6] = makehsb(180/360.0, 0.8, 0.8); /* cyan */

    [self rebuildTextAttributes];
}

/*
 * Global accessors
 */

+ (NSColor*) foregroundColor: (int)number
{
    if (number < 0 || number > 7)
        return nil;
    return fgcolor[number];
}

+ (NSColor*) backgroundColor: (int)number
{
    if (number < 0 || number > 7)
        return nil;
    return bgcolor[number];
}

+ (NSInteger) graphicsEnabled
{
    return dographics;
}

+ (NSInteger) soundEnabled
{
    return dosound;
}

+ (NSInteger) stylesEnabled
{
    return dostyles;
}

+ (NSInteger) useScreenFonts
{
    return usescreenfonts;
}

+ (NSInteger) smartQuotes
{
    return smartquotes;
}

+ (NSInteger) spaceFormat
{
    return spaceformat;
}

+ (float) lineHeight
{
    return cellh;
}

+ (float) charWidth
{
    return cellw;
}

+ (NSInteger) gridMargins
{
    return gridmargin;
}

+ (NSInteger) bufferMargins
{
    return buffermargin;
}

+ (float) leading
{
    return leading;
}

+ (NSSize) defaultWindowSize
{
    return NSMakeSize(ceil([self charWidth] * defscreenw + gridmargin * 2.0),
                      ceil([self lineHeight] * defscreenh + gridmargin * 2.0));
}

+ (NSColor*) gridBackground
{
    return gridbg;
}

+ (NSColor*) gridForeground;
{
    return gridfg;
}

+ (NSColor*) bufferBackground;
{
    return bufferbg;
}

+ (NSColor*) bufferForeground;
{
    return bufferfg;
}

+ (NSColor*) inputColor;
{
    return inputfg;
}


/*
 * Style and attributed-string magic
 */

+ (NSDictionary*) attributesForGridStyle: (int)style
{
    if (style < 0 || style >= style_NUMSTYLES)
        return nil;
    return gridatts[style];
}

+ (NSDictionary*) attributesForBufferStyle: (int)style
{
    if (style < 0 || style >= style_NUMSTYLES)
        return nil;
    return bufferatts[style];
}

+ (void) rebuildTextAttributes
{
    int style;
    NSFontManager *mgr = [NSFontManager sharedFontManager];
    NSMutableParagraphStyle *para;
    NSMutableDictionary *dict;
    NSFont *font;

    //NSLog(@"pref: rebuildTextAttributes()");

    /* make italic, bold, bolditalic font variants */

    NSFont *bufbold, *bufitalic, *bufbolditalic;
    NSFont *gridbold, *griditalic, *gridbolditalic;

    gridbold = [mgr convertWeight: YES ofFont: gridroman];
    griditalic = [mgr convertFont: gridroman toHaveTrait: NSItalicFontMask];
    gridbolditalic = [mgr convertFont: gridbold toHaveTrait: NSItalicFontMask];

    bufbold = [mgr convertWeight: YES ofFont: bufroman];
    bufitalic = [mgr convertFont: bufroman toHaveTrait: NSItalicFontMask];
    bufbolditalic = [mgr convertFont: bufbold toHaveTrait: NSItalicFontMask];

    /* update style attribute dictionaries */

    para = [[NSMutableParagraphStyle alloc] init];
    [para setParagraphStyle: [NSParagraphStyle defaultParagraphStyle]];
    [para setLineSpacing: leading];

    for (style = 0; style < style_NUMSTYLES; style++)
    {
        /*
         * Buffer windows
         */


        dict = [[NSMutableDictionary alloc] init];
        dict[@"GlkStyle"] = @(style);
        dict[NSParagraphStyleAttributeName] = para;

#if 0
        if (style == style_BlockQuote)
        {
            NSMutableParagraphStyle *mpara;
            float indent = [bufroman defaultLineHeightForFont] * 1.0;
            mpara = [[NSMutableParagraphStyle alloc] init];
            [mpara setParagraphStyle: para];
            [mpara setFirstLineHeadIndent: indent];
            [mpara setHeadIndent: indent];
            [mpara setTailIndent: -indent];
            [dict setObject: mpara forKey: NSParagraphStyleAttributeName];
            [mpara release];
        }
#endif

        if (style == style_Input)
            dict[NSForegroundColorAttributeName] = inputfg;
        else
            dict[NSForegroundColorAttributeName] = bufferfg;

        font = bufroman;
        switch (style)
        {
            case style_Emphasized: font = bufitalic; break;
            case style_Preformatted: font = gridroman; break;
            case style_Header: font = bufbold; break;
            case style_Subheader: font = bufbold; break;
            case style_Alert: font = bufbolditalic; break;
            case style_Input: font = inputfont; break;
        }
        dict[NSFontAttributeName] = font;

        bufferatts[style] = dict;

        /*
         * Grid windows
         */


        dict = [[NSMutableDictionary alloc] init];
        dict[@"GlkStyle"] = @(style);
        dict[NSParagraphStyleAttributeName] = para;
        dict[NSForegroundColorAttributeName] = gridfg;

        /* for our frotz quote-box hack */
        if (style == style_User1)
            dict[NSBackgroundColorAttributeName] = gridbg;

        font = gridroman;
        switch (style)
        {
            case style_Emphasized: font = griditalic; break;
            case style_Preformatted: font = gridroman; break;
            case style_Header: font = gridbold; break;
            case style_Subheader: font = gridbold; break;
            case style_Alert: font = gridbolditalic; break;
        }
        dict[NSFontAttributeName] = font;

        gridatts[style] = dict;
    }

    if (usescreenfonts)
        font = [gridroman screenFont];
    else
        font = [gridroman printerFont];

    cellw = [font advancementForGlyph:(NSGlyph) 'X'].width;


    NSLayoutManager *layoutManager = [[NSLayoutManager alloc] init];
    cellh = [layoutManager defaultLineHeightForFont:font] + leading;
    layoutManager = nil;
    //cellh = [font ascender] + [font descender] + [font leading] + leading;

    /* send notification that prefs have changed -- trigger configure events */
    [[NSNotificationCenter defaultCenter]
     postNotificationName: @"PreferencesChanged" object: nil];


}

/* ---------------------------------------------------------------------- */

/*
 * Instance -- controller for preference panel
 *
 */

NSString* fontToString(NSFont *font)
{
    return [NSString stringWithFormat: @"%@ %g", [font displayName], [font pointSize]];
}

- (void) windowDidLoad
{
    NSLog(@"pref: windowDidLoad()");

    [super windowDidLoad];

    [self setWindowFrameAutosaveName: @"PrefsWindow"];

    [clrGridFg setColor: gridfg];
    [clrGridBg setColor: gridbg];
    [clrBufferFg setColor: bufferfg];
    [clrBufferBg setColor: bufferbg];
    [clrInputFg setColor: inputfg];

    [txtGridMargin setFloatValue: gridmargin];
    [txtBufferMargin setFloatValue: buffermargin];
    [txtLeading setFloatValue: leading];

    [txtCols setIntValue: defscreenw];
    [txtRows setIntValue: defscreenh];

    [btnGridFont setTitle: fontToString(gridroman)];
    [btnBufferFont setTitle: fontToString(bufroman)];
    [btnInputFont setTitle: fontToString(inputfont)];

    [btnSmartQuotes setState: smartquotes];
    [btnSpaceFormat setState: spaceformat];

    [btnEnableGraphics setState: dographics];
    [btnEnableSound setState: dosound];
    [btnEnableStyles setState: dostyles];
    [btnUseScreenFonts setState: usescreenfonts];
}

- (IBAction) changeDefaultSize: (id)sender
{
    if (sender == txtCols)
    {
        defscreenw = [sender intValue];
        if (defscreenw < 5 || defscreenw > 200)
            defscreenw = 60;
        [[NSUserDefaults standardUserDefaults]
         setObject: @(defscreenw)
         forKey: @"DefaultWidth"];
    }
    if (sender == txtRows)
    {
        defscreenh = [sender intValue];
        if (defscreenh < 5 || defscreenh > 200)
            defscreenh = 24;
        [[NSUserDefaults standardUserDefaults]
         setObject: @(defscreenh)
         forKey: @"DefaultHeight"];
    }
}

- (IBAction) changeColor: (id)sender
{
    NSString *key = nil;
    colorp = nil;

    if (sender == clrGridFg) { key = @"GridForeground"; colorp = &gridfg; }
    if (sender == clrGridBg) { key = @"GridBackground"; colorp = &gridbg; }
    if (sender == clrBufferFg) { key = @"BufferForeground"; colorp = &bufferfg; }
    if (sender == clrBufferBg) { key = @"BufferBackground"; colorp = &bufferbg; }
    if (sender == clrInputFg) { key = @"InputColor"; colorp = &inputfg; }

    if (colorp)
    {
         *colorp = nil;
        *colorp = [sender color];

        [[NSUserDefaults standardUserDefaults] setObject: colorToData(*colorp) forKey: key];

        [Preferences rebuildTextAttributes];
    }
}

- (IBAction) changeMargin: (id)sender;
{
    NSString *key = nil;
    float val = 0.0;

    val = [sender floatValue];

    if (sender == txtGridMargin) { key = @"GridMargin"; gridmargin = val; }
    if (sender == txtBufferMargin) { key = @"BufferMargin"; buffermargin = val; }

    if (key)
    {
        [[NSUserDefaults standardUserDefaults] setObject: @(val) forKey: key];
        [Preferences rebuildTextAttributes];
    }
}

- (IBAction) changeLeading: (id)sender
{
    leading = [sender floatValue];
    [[NSUserDefaults standardUserDefaults] setObject: @(leading) forKey: @"Leading"];
    [Preferences rebuildTextAttributes];
}

#pragma mark - Font panel

- (IBAction) showFontPanel: (id)sender
{
    selfontp = nil;
    colorp = nil;
	colorp2 = nil;


    if (sender == btnGridFont) { selfontp = &gridroman; colorp = &gridfg; colorp2 = &gridbg; }
	if (sender == btnBufferFont) { selfontp = &bufroman;  colorp = &bufferfg; colorp2 = &bufferbg;  }
    if (sender == btnInputFont) { selfontp = &inputfont;  colorp = &inputfg; colorp2 = &bufferbg; }

    if (selfontp)
    {
        NSDictionary *attr = @{ @"NSColor" : *colorp, @"NSDocumentBackgroundColor" : *colorp2 };
        
        [self.window makeFirstResponder: self.window];

        [NSFontManager sharedFontManager].target = self;
		[[NSFontPanel sharedFontPanel] setDelegate:self];
		[[NSFontPanel sharedFontPanel] makeKeyAndOrderFront:self];

		[[NSFontManager sharedFontManager] setSelectedAttributes:attr isMultiple: NO];
		[[NSFontManager sharedFontManager] setSelectedFont: *selfontp isMultiple:NO];

    }
}

- (IBAction) changeFont: (id)fontManager
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    *selfontp = [fontManager convertFont: *selfontp];

    if (selfontp == &gridroman)
    {
        [defaults setObject: [gridroman fontName] forKey: @"GridFontName"];
        [defaults setObject: [NSNumber numberWithFloat: [gridroman pointSize]] forKey: @"GridFontSize"];
        [btnGridFont setTitle: fontToString(gridroman)];
    }

    if (selfontp == &bufroman)
    {
        [defaults setObject: [bufroman fontName] forKey: @"BufferFontName"];
        [defaults setObject: [NSNumber numberWithFloat: [bufroman pointSize]] forKey: @"BufferFontSize"];
        [btnBufferFont setTitle: fontToString(bufroman)];
    }

    if (selfontp == &inputfont)
    {
        [defaults setObject: [inputfont fontName] forKey: @"InputFontName"];
        [defaults setObject: [NSNumber numberWithFloat: [inputfont pointSize]] forKey: @"InputFontSize"];
        [btnInputFont setTitle: fontToString(inputfont)];
    }

    [Preferences rebuildTextAttributes];
}

// This is sent from the font panel when changing font style there

- (void) changeAttributes:(id)sender
{
    NSLog(@"changeAttributes:%@", sender);

    NSDictionary * newAttributes = [sender convertAttributes:@{}];

	NSLog(@"Keys in newAttributes:");
	for(NSString *key in [newAttributes allKeys]) {
		NSLog(@" %@ : %@",key, [newAttributes valueForKey: key]);
	}

//	"NSForegroundColorAttributeName"	"NSColor"
//	"NSUnderlineStyleAttributeName"		"NSUnderline"
//	"NSStrikethroughStyleAttributeName"	"NSStrikethrough"
//	"NSUnderlineColorAttributeName"		"NSUnderlineColor"
//	"NSStrikethroughColorAttributeName"	"NSStrikethroughColor"
//										"NSShadow"

    if ([newAttributes valueForKey: @"NSColor"])
	{
        NSColorWell *colorWell = nil;
        NSFont *currentFont = [NSFontManager sharedFontManager].selectedFont;
        if (currentFont == gridroman)
            colorWell = clrGridFg;
        else if (currentFont == bufroman)
            colorWell = clrBufferFg;
        else if (currentFont == inputfont)
            colorWell = clrInputFg;
        colorWell.color=[newAttributes valueForKey: @"NSColor"];
        [self changeColor:colorWell];
    }
}

// This is sent from the font panel when changing background color there

- (void)changeDocumentBackgroundColor:(id)sender
{
	NSLog(@"changeDocumentBackgroundColor");

	NSColorWell *colorWell = nil;
	NSFont *currentFont = [NSFontManager sharedFontManager].selectedFont;
	if (currentFont == gridroman)
		colorWell = clrGridBg;
	else if (currentFont == bufroman)
		colorWell = clrBufferBg;
	else if (currentFont == inputfont)
		colorWell = clrBufferBg;
	colorWell.color=[sender color];
	[self changeColor:colorWell];
}

- (NSUInteger)validModesForFontPanel:(NSFontPanel *)fontPanel
{
//	NSLog(@"validModesForFontPanel");

	return NSFontPanelFaceModeMask | NSFontPanelCollectionModeMask | NSFontPanelSizeModeMask | NSFontPanelTextColorEffectModeMask | NSFontPanelDocumentColorEffectModeMask;}

- (void) windowWillClose: (id)sender
{
	if ([[NSFontPanel sharedFontPanel] isVisible])
		[[NSFontPanel sharedFontPanel] orderOut:self];
}

- (IBAction) changeSmartQuotes: (id)sender
{
    smartquotes = [sender state];
    NSLog(@"pref: smart quotes changed to %d", smartquotes);
    [[NSUserDefaults standardUserDefaults]
     setObject: @(smartquotes)
     forKey: @"SmartQuotes"];
}

- (IBAction) changeSpaceFormatting: (id)sender
{
    spaceformat = [sender state];
    NSLog(@"pref: space format changed to %d", spaceformat);
    [[NSUserDefaults standardUserDefaults]
     setObject: @(spaceformat)
     forKey: @"SpaceFormat"];
}

- (IBAction) changeEnableGraphics: (id)sender
{
    dographics = [sender state];
    NSLog(@"pref: dographics changed to %d", dographics);
    [[NSUserDefaults standardUserDefaults]
     setObject: @(dographics)
     forKey: @"EnableGraphics"];

    /* send notification that prefs have changed -- tell clients that graphics are off limits */
    [[NSNotificationCenter defaultCenter]
     postNotificationName: @"PreferencesChanged" object: nil];
}

- (IBAction) changeEnableSound: (id)sender
{
    dosound = [sender state];
    NSLog(@"pref: dosound changed to %d", dosound);
    [[NSUserDefaults standardUserDefaults]
     setObject: @(dosound)
     forKey: @"EnableSound"];

    /* send notification that prefs have changed -- tell clients that sound is off limits */
    [[NSNotificationCenter defaultCenter]
     postNotificationName: @"PreferencesChanged" object: nil];
}

- (IBAction) changeEnableStyles: (id)sender
{
    dostyles = [sender state];
    NSLog(@"pref: dostyles changed to %d", dostyles);
    [[NSUserDefaults standardUserDefaults]
     setObject: @(dostyles)
     forKey: @"EnableStyles"];
    [Preferences rebuildTextAttributes];
}

- (IBAction) changeUseScreenFonts: (id)sender
{
    usescreenfonts = [sender state];
    NSLog(@"pref: usescreenfonts changed to %d", usescreenfonts);
    [[NSUserDefaults standardUserDefaults]
     setObject: @(usescreenfonts)
     forKey: @"ScreenFonts"];
    [Preferences rebuildTextAttributes];
}

@end
