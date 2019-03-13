#import "main.h"
#import "Compatibility.h"

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

static CGFloat gridmargin = 0;
static CGFloat buffermargin = 0;
static CGFloat border = 0;

static CGFloat leading = 0;		/* added to lineHeight */

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

static Preferences *prefs = nil;

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
    const unsigned char *buf = data.bytes;

    if (data.length < 3)
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

    defaults[@"GameDirectory"] = (@"~/Documents").stringByExpandingTildeInPath;
    defaults[@"SaveDirectory"] = (@"~/Documents").stringByExpandingTildeInPath;

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

    gridmargin = [[defaults objectForKey: @"GridMargin"] doubleValue];
    buffermargin = [[defaults objectForKey: @"BufferMargin"] doubleValue];
    border = [[defaults objectForKey: @"Border"] doubleValue];

    leading = [[defaults objectForKey: @"Leading"] doubleValue];

    name = [defaults objectForKey: @"GridFontName"];
    size = [[defaults objectForKey: @"GridFontSize"] doubleValue];
    gridroman = [NSFont fontWithName: name size: size];
    if (!gridroman)
    {
        NSLog(@"pref: failed to create grid font '%@'", name);
        gridroman = [NSFont userFixedPitchFontOfSize: 0];
    }

    name = [defaults objectForKey: @"BufferFontName"];
    size = [[defaults objectForKey: @"BufferFontSize"] doubleValue];
    bufroman = [NSFont fontWithName: name size: size];
    if (!bufroman)
    {
        NSLog(@"pref: failed to create buffer font '%@'", name);
        bufroman = [NSFont userFontOfSize: 0];
    }

    name = [defaults objectForKey: @"InputFontName"];
    size = [[defaults objectForKey: @"InputFontSize"] doubleValue];
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

+ (CGFloat) gridMargins
{
    return gridmargin;
}

+ (CGFloat) bufferMargins
{
    return buffermargin;
}

+ (CGFloat) border
{
    return border;
}

+ (CGFloat) leading
{
    return leading;
}

+ (NSSize) defaultWindowSize
{
    NSInteger width = ceil(self.charWidth * defscreenw + (gridmargin + border + 4) * 2.0);
    NSInteger height = ceil(self.lineHeight * defscreenh + (gridmargin + border + 4) * 2.0);

    CGRect screenframe = (NSScreen.mainScreen).visibleFrame;

    if (width > screenframe.size.width) width = screenframe.size.width;
    if (height > screenframe.size.height) height = screenframe.size.height;

    return NSMakeSize(width, height);
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

+ (Preferences *) instance;
{
	return prefs;
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

    NSFont *bufbold, *bufitalic, *bufbolditalic, *bufheader;
    NSFont *gridbold, *griditalic, *gridbolditalic;

    gridbold = [mgr convertWeight: YES ofFont: gridroman];
    griditalic = [mgr convertFont: gridroman toHaveTrait: NSItalicFontMask];
    gridbolditalic = [mgr convertFont: gridbold toHaveTrait: NSItalicFontMask];

    bufbold = [mgr convertWeight: YES ofFont: bufroman];
    bufitalic = [mgr convertFont: bufroman toHaveTrait: NSItalicFontMask];
    bufbolditalic = [mgr convertFont: bufbold toHaveTrait: NSItalicFontMask];
    bufheader = [mgr convertFont: bufbold toSize: bufbold.pointSize + 2];

    /* update style attribute dictionaries */

    para = [[NSMutableParagraphStyle alloc] init];
    [para setParagraphStyle: [NSParagraphStyle defaultParagraphStyle]];
    para.lineSpacing = leading;

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
            case style_Header: font = bufheader; break;
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
        font = gridroman.screenFont;
    else
        font = gridroman.printerFont;

    //NSLog(@"[font advancementForGlyph:(NSGlyph)'X'].width:%f font.maximumAdvancement.width:%f [@\"X\" sizeWithAttributes:@{NSFontAttributeName: font}].width:%f", [font advancementForGlyph:(NSGlyph) 'X'].width, font.maximumAdvancement.width, [@"X" sizeWithAttributes:@{NSFontAttributeName: font}].width);

    //This is the only way I have found to get the correct width at all sizes
    if (NSAppKitVersionNumber < NSAppKitVersionNumber10_8)
        cellw = [@"X" sizeWithAttributes:@{NSFontAttributeName: font}].width;
    else
        cellw = [font advancementForGlyph:(NSGlyph)'X'].width;

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
    if ((int)font.pointSize == font.pointSize)
        return [NSString stringWithFormat: @"%@ %.f", font.displayName, (float)font.pointSize];
    else
        return [NSString stringWithFormat: @"%@ %.1f", font.displayName, (float)font.pointSize];
}

- (void) windowDidLoad
{
    NSLog(@"pref: windowDidLoad()");

    [super windowDidLoad];

    self.windowFrameAutosaveName = @"PrefsWindow";

	prefs = self;

    clrGridFg.color = gridfg;
    clrGridBg.color = gridbg;
    clrBufferFg.color = bufferfg;
    clrBufferBg.color = bufferbg;
    clrInputFg.color = inputfg;

    txtGridMargin.floatValue = gridmargin;
    txtBufferMargin.floatValue = buffermargin;
    txtLeading.floatValue = leading;

    txtCols.intValue = defscreenw;
    txtRows.intValue = defscreenh;

    txtBorder.intValue = border;

    btnGridFont.title = fontToString(gridroman);
    btnBufferFont.title = fontToString(bufroman);
    btnInputFont.title = fontToString(inputfont);

    btnSmartQuotes.state = smartquotes;
    btnSpaceFormat.state = spaceformat;

    btnEnableGraphics.state = dographics;
    btnEnableSound.state = dosound;
    btnEnableStyles.state = dostyles;
    btnUseScreenFonts.state = usescreenfonts;
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

	/* send notification that default size has changed -- resize all windows */
	[[NSNotificationCenter defaultCenter]
	 postNotificationName: @"DefaultSizeChanged" object: nil];
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

- (IBAction) changeBorderSize: (id)sender
{
    border = [sender floatValue];
    NSLog(@"pref: border width changed to %f", border);
    [[NSUserDefaults standardUserDefaults] setObject: @(border) forKey: @"Border"];

    /* send notification that prefs have changed -- tell clients that border has changed */
    [[NSNotificationCenter defaultCenter]
     postNotificationName: @"PreferencesChanged" object: nil];
}

#pragma mark - Zoom

+ (void) zoomIn
{
	if (gridroman.pointSize < 100)
	{
        [self scale:(gridroman.pointSize + 1) / gridroman.pointSize];
	}
}

+ (void) zoomOut
{
	if (gridroman.pointSize > 6)
	{
        [self scale:(gridroman.pointSize - 1)/ gridroman.pointSize];
	}
}

+ (void) zoomToActualSize
{
    [self scale:12 / gridroman.pointSize];
}

+ (void)scale:(CGFloat)scalefactor
{
	//NSLog(@"Preferences scale: %f", scalefactor);

    if (scalefactor < 0)
        scalefactor = fabs(scalefactor);

    if ((scalefactor < 1.01 && scalefactor > 0.99) || scalefactor == 0.0)
        scalefactor = 1.0;

	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	CGFloat fontSize;

    fontSize = gridroman.pointSize;
	fontSize *= scalefactor;
    if (fontSize > 0)
	{
        gridroman = [NSFont fontWithDescriptor:gridroman.fontDescriptor size:fontSize];
        [defaults setObject: @(fontSize) forKey: @"GridFontSize"];
    }

	fontSize = bufroman.pointSize;
    fontSize *= scalefactor;
    if (fontSize > 0)
	{
        bufroman = [NSFont fontWithDescriptor:bufroman.fontDescriptor size:fontSize];
        [defaults setObject: @(fontSize) forKey: @"BufferFontSize"];
    }

	fontSize = inputfont.pointSize;
    fontSize *= scalefactor;
    if (fontSize > 0)
	{
        inputfont = [NSFont fontWithDescriptor:inputfont.fontDescriptor size:fontSize];
        [defaults setObject: @(fontSize) forKey: @"InputFontSize"];
    }

	if (leading * scalefactor > 0)
	{
		leading *= scalefactor;
		[defaults setObject: @(leading) forKey: @"Leading"];
	}

	if (gridmargin * scalefactor > 0)
	{
		gridmargin *= scalefactor;
		[defaults setObject: @(gridmargin) forKey: @"GridMargin"];
	}

	if (buffermargin * scalefactor > 0)
	{
		buffermargin *= scalefactor;
		[defaults setObject: @(buffermargin) forKey: @"BufferMargin"];
	}

	if (border * scalefactor > 0)
	{
		border *= scalefactor;
		[defaults setObject: @(border) forKey: @"Border"];
	}

	[Preferences rebuildTextAttributes];

	/* send notification that default size has changed -- resize all windows */
	[[NSNotificationCenter defaultCenter]
	 postNotificationName: @"DefaultSizeChanged" object: nil];
}

- (void) updatePanelAfterZoom
{
	btnGridFont.title = fontToString(gridroman);
	btnBufferFont.title = fontToString(bufroman);
	btnInputFont.title = fontToString(inputfont);
	txtLeading.floatValue = leading;
	txtGridMargin.floatValue = gridmargin;
	txtBufferMargin.floatValue = buffermargin;
	txtBorder.intValue = border;
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
		[NSFontPanel sharedFontPanel].delegate = self;
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
		[defaults setObject: gridroman.fontName forKey: @"GridFontName"];
		[defaults setObject: @(gridroman.pointSize) forKey: @"GridFontSize"];
		btnGridFont.title = fontToString(gridroman);
	}

	if (selfontp == &bufroman)
	{
		[defaults setObject: bufroman.fontName forKey: @"BufferFontName"];
		[defaults setObject: @(bufroman.pointSize) forKey: @"BufferFontSize"];
		btnBufferFont.title = fontToString(bufroman);
	}

	if (selfontp == &inputfont)
	{
		[defaults setObject: inputfont.fontName forKey: @"InputFontName"];
		[defaults setObject: @(inputfont.pointSize) forKey: @"InputFontSize"];
		btnInputFont.title = fontToString(inputfont);
	}

	[Preferences rebuildTextAttributes];
}

// This is sent from the font panel when changing font style there

- (void) changeAttributes:(id)sender
{
	NSLog(@"changeAttributes:%@", sender);

	NSDictionary * newAttributes = [sender convertAttributes:@{}];

	NSLog(@"Keys in newAttributes:");
	for(NSString *key in newAttributes.allKeys) {
		NSLog(@" %@ : %@",key, [newAttributes valueForKey: key]);
	}

	//	"NSForegroundColorAttributeName"	"NSColor"
	//	"NSUnderlineStyleAttributeName"		"NSUnderline"
	//	"NSStrikethroughStyleAttributeName"	"NSStrikethrough"
	//	"NSUnderlineColorAttributeName"		"NSUnderlineColor"
	//	"NSStrikethroughColorAttributeName"	"NSStrikethroughColor"
	//	"NSShadowAttributeName"				"NSShadow"

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

	return NSFontPanelFaceModeMask | NSFontPanelCollectionModeMask | NSFontPanelSizeModeMask | NSFontPanelTextColorEffectModeMask | NSFontPanelDocumentColorEffectModeMask;
}

- (void) windowWillClose: (id)sender
{
	if ([NSFontPanel.sharedFontPanel isVisible])
		[NSFontPanel.sharedFontPanel orderOut:self];
}

@end
