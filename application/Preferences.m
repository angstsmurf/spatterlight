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

static Settings *currentSettings;

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
    CGFloat r, g, b, a;
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
    NSLog(@"readDefaults: space format changed to %d. Current game is %@" ,spaceformat, [currentSettings.games anyObject].metadata.title);


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

    if (currentSettings)
        [self changePreferences:currentSettings];
    
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
    {
        NSLog(@"Preferences backgroundColor called with an illegal color number of %d", number);
        return nil;
    }
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
    if (currentSettings.width && currentSettings.height)
        return NSMakeSize(currentSettings.width, currentSettings.height);
    else  return NSMakeSize (ceil([self charWidth] * defscreenw + gridmargin * 2.0),
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

+ (Settings *) currentSettings
{
    return currentSettings;
}

+ (void) rebuildTextAttributes
{
    int style;
    NSFontManager *mgr = [NSFontManager sharedFontManager];
    NSMutableParagraphStyle *para;
    NSMutableDictionary *dict;
    NSFont *font;
    
    NSLog(@"pref: rebuildTextAttributes()");
    
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
    para.lineSpacing = leading;
    
    for (style = 0; style < style_NUMSTYLES; style++)
    {
        /*
         * Buffer windows
         */
        
        
        dict = [[NSMutableDictionary alloc] init];
        dict[@"GlkStyle"] = @(style);
        dict[NSParagraphStyleAttributeName] = para;

//        if (style == style_BlockQuote)
//        {
//            NSMutableParagraphStyle *mpara;
//            float indent = [bufroman defaultLineHeightForFont] * 1.0;
//            mpara = [[NSMutableParagraphStyle alloc] init];
//            [mpara setParagraphStyle: para];
//            [mpara setFirstLineHeadIndent: indent];
//            [mpara setHeadIndent: indent];
//            [mpara setTailIndent: -indent];
//            [dict setObject: mpara forKey: NSParagraphStyleAttributeName];
//            [mpara release];
//        }
        
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
        font = gridroman.screenFont;
    else
        font = gridroman.printerFont;
    
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
    return [NSString stringWithFormat: @"%@ %g", font.displayName, font.pointSize];
}

- (void) windowDidLoad
{
    NSLog(@"pref: windowDidLoad()");
    
    [super windowDidLoad];
    
    self.windowFrameAutosaveName = @"PrefsWindow";

    if (currentSettings)
    {
        NSLog(@"windowDidLoad:Change current settings to those of %@", [currentSettings.games anyObject].metadata.title);
        [Preferences changePreferences:currentSettings];
        [Preferences rebuildTextAttributes];
    }
    else [self updatePreferencePanel];


//    btnUseScreenFonts.state = usescreenfonts;
}

- (void) updatePreferencePanel
{
    clrGridFg.color = gridfg;
    clrGridBg.color = gridbg;
    clrBufferFg.color = bufferfg;
    clrBufferBg.color = bufferbg;
    clrInputFg.color = inputfg;

    txtGridMargin.floatValue = gridmargin;
    txtBufferMargin.floatValue = buffermargin;
    txtLeading.floatValue = leading;

    txtCols.floatValue = defscreenw;
    txtRows.floatValue = defscreenh;

    btnGridFont.title = fontToString(gridroman);
    btnBufferFont.title = fontToString(bufroman);
    btnInputFont.title = fontToString(inputfont);

    btnSmartQuotes.state = smartquotes;
    btnSpaceFormat.state = spaceformat;

    btnEnableGraphics.state = dographics;
    btnEnableSound.state = dosound;
    btnEnableStyles.state = dostyles;
}

+ (void) changePreferences: (Settings *)settings
{
    NSLog(@"changePreferences for %@." , [settings.games anyObject].metadata.title);

    if (settings != currentSettings)
    {
        if (currentSettings)
            [Preferences savePreferences];
        currentSettings = settings;
    }
    if (settings.gridFont.color)
        gridfg = dataToColor((NSData *)settings.gridFont.color);
    if (settings.gridFont.bgcolor)
        gridbg = dataToColor((NSData *)settings.gridFont.bgcolor);
    if (settings.bufferFont.color)
        bufferfg = dataToColor((NSData *)settings.bufferFont.color);

    if (settings.bufferFont.bgcolor)
        bufferbg = dataToColor((NSData *)settings.bufferFont.bgcolor);
    if (settings.bufInput.color)
        inputfg = dataToColor((NSData *)settings.bufInput.color);

    if (settings.tmargin != -1)
    {
        //gridmargin = settings.tmargin;
        buffermargin = settings.tmargin;
        NSLog(@"changePreferences: Changed current text margins to %f", settings.tmargin);

    }

//    if (settings.bufferFont.leading)
        leading = settings.bufferFont.leading;

    if (settings.width)
        defscreenw = settings.width;
    if (settings.height)
        defscreenh = settings.height;

    if (settings.bufferFont.name && settings.bufferFont.size)
    {
        bufroman = [NSFont fontWithName:settings.bufferFont.name size:settings.bufferFont.size];
        NSLog(@"changePreferences: Changed current buffer font to %f pt %@", currentSettings.bufferFont.size, currentSettings.bufferFont.name);
    }
    if (settings.gridFont.name && settings.gridFont.size)
        gridroman = [NSFont fontWithName:settings.gridFont.name size:settings.gridFont.size];
    if (settings.bufInput.name  && settings.bufInput.size)
        inputfont = [NSFont fontWithName:settings.bufInput.name size:settings.bufInput.size];


    if (settings.smartquotes != -1)
        smartquotes = settings.smartquotes;
    if (settings.spaceformat != -1)
    {
        spaceformat = settings.spaceformat;
    }
    if (settings.dographics != -1)
        dographics = settings.dographics;
    if (settings.dosound != -1)
        dosound = settings.dosound;
    if (settings.dostyles != -1)
        dostyles = settings.dostyles;

    [Preferences rebuildTextAttributes];
    [(AppDelegate *)[[NSApplication sharedApplication] delegate] updatePreferencePanel];
    [Preferences savePreferences];
}

+ (void) savePreferences
{
    if (!currentSettings)
        return;

    NSLog(@"savePreferences for %@." , [currentSettings.games anyObject].metadata.title);

    currentSettings.gridFont.color = colorToData(gridfg);
    currentSettings.gridFont.bgcolor = colorToData(gridbg);
    currentSettings.bufferFont.color = colorToData(bufferfg);
    currentSettings.bufferFont.bgcolor = colorToData(bufferbg);
    currentSettings.bufInput.color = colorToData(inputfg);

    currentSettings.tmargin = buffermargin;

    currentSettings.bufferFont.leading = leading;
    currentSettings.width = defscreenw;
    currentSettings.height = defscreenh;

    currentSettings.bufferFont.name = bufroman.fontName;
    currentSettings.bufferFont.size = bufroman.pointSize;

    currentSettings.gridFont.name = gridroman.fontName;
    currentSettings.gridFont.size = gridroman.pointSize;

    currentSettings.bufInput.name = inputfont.fontName;
    currentSettings.bufInput.size = inputfont.pointSize;

    currentSettings.smartquotes = smartquotes;
    currentSettings.spaceformat = spaceformat;
    currentSettings.dographics = dographics;
    currentSettings.dosound = dosound;
    currentSettings.dostyles = dostyles;

    NSError *error = nil;
    if (![((AppDelegate*)[[NSApplication sharedApplication] delegate]).persistentContainer.viewContext save:&error]) {
        NSLog(@"There's a problem: %@", error);
    }

}

+ (void) nilCurrentSettings
{
    currentSettings = nil;
    [Preferences readDefaults];
}


+ (void) changeDefaultSize: (NSSize)size forSettings: (Settings *)setting
{
    if (currentSettings != setting)
    {
        [Preferences changePreferences:setting];
    }
    currentSettings.width = size.width;
    currentSettings.height = size.height;

    defscreenw = size.width;
    defscreenh = size.height;


//    if (sender == txtCols)
//    {
//        defscreenw = [sender intValue];
//        if (defscreenw < 5 || defscreenw > 200)
//            defscreenw = 60;
        [[NSUserDefaults standardUserDefaults]
         setObject: @(defscreenw)
         forKey: @"DefaultWidth"];
//    }
//    if (sender == txtRows)
//    {
//        defscreenh = [sender intValue];
//        if (defscreenh < 5 || defscreenh > 200)
//            defscreenh = 24;
        [[NSUserDefaults standardUserDefaults]
         setObject: @(defscreenh)
         forKey: @"DefaultHeight"];
//    }
}

- (IBAction) changeColor: (id)sender
{
    NSString *key = nil;
    NSData *col = colorToData([sender color]);

    colorp = nil;
    
    if (sender == clrGridFg)
    {   currentSettings.gridFont.color = col; key = @"GridForeground"; colorp = &gridfg; }
    if (sender == clrGridBg)
    { currentSettings.gridFont.bgcolor = col; key = @"GridBackground"; colorp = &gridbg; }

    if (sender == clrBufferFg)
    { currentSettings.bufferFont.color = col; key = @"BufferForeground"; colorp = &bufferfg; }

    if (sender == clrBufferBg)
    {    currentSettings.bufferFont.bgcolor = col; key = @"BufferBackground"; colorp = &bufferbg; }

    if (sender == clrInputFg)
    {  currentSettings.bufInput.color = col; key = @"InputColor"; colorp = &inputfg; }

    //[Preferences rebuildTextAttributes];


    if (colorp)
    {
        *colorp = nil;
        *colorp = [sender color];
//        
        [[NSUserDefaults standardUserDefaults] setObject: colorToData(*colorp) forKey: key];
//        
        [Preferences rebuildTextAttributes];
    }
}

- (IBAction) changeMargin: (id)sender;
{
    NSString *key = nil;
    float val = 0.0;
    
    val = [sender floatValue];

    currentSettings.tmargin = val;
//    [Preferences rebuildTextAttributes];


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

    currentSettings.bufferFont.leading = leading;

    [[NSUserDefaults standardUserDefaults] setObject: @(leading) forKey: @"Leading"];
    [Preferences rebuildTextAttributes];
}

- (IBAction) showFontPanel: (id)sender
{
    selfontp = nil;
    colorp = nil;
    
    if (sender == btnGridFont) { selfontp = &gridroman; colorp = &gridfg; }
    if (sender == btnBufferFont) { selfontp = &bufroman;  colorp = &bufferfg; }
    if (sender == btnInputFont) { selfontp = &inputfont;  colorp = &inputfg; }
    
    if (selfontp)
    {
        [self.window makeFirstResponder: self.window];

        //NSDictionary *attr = [NSDictionary dictionaryWithObject:bufferfg forKey:@"NSForegroundColorAttributeName"];
        NSDictionary *attr = [NSDictionary dictionaryWithObject:*colorp forKey:@"NSColor"];


        [NSFontManager sharedFontManager].target = self;
        [[NSFontManager sharedFontManager] setSelectedFont: *selfontp isMultiple:NO];
        [[NSFontManager sharedFontManager] orderFrontFontPanel: self];
        [[NSFontManager sharedFontManager] setSelectedAttributes:attr isMultiple: NO];

    }
}

- (IBAction) changeFont: (id)fontManager
{
    NSLog(@"changeFont");

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    *selfontp = [fontManager convertFont: *selfontp];
    
    if (selfontp == &gridroman)
    {
        [defaults setObject: gridroman.fontName forKey: @"GridFontName"];
        [defaults setObject: @(gridroman.pointSize) forKey: @"GridFontSize"];
        btnGridFont.title = fontToString(gridroman);

        currentSettings.gridFont.name = gridroman.fontName;
        currentSettings.gridFont.size = gridroman.pointSize;
       // currentSettings.bufferFont.color = gridroman.fo

    }

    if (selfontp == &bufroman)
    {
        currentSettings.bufferFont.name = bufroman.fontName;
        currentSettings.bufferFont.size = bufroman.pointSize;

        NSLog(@"changeFont: Changed current buffer font to %f pt %@", currentSettings.bufferFont.size, currentSettings.bufferFont.name);


        [defaults setObject: bufroman.fontName forKey: @"BufferFontName"];
        [defaults setObject: @(bufroman.pointSize) forKey: @"BufferFontSize"];

        btnBufferFont.title = fontToString(bufroman);
    }
    
    if (selfontp == &inputfont)
    {
        currentSettings.bufInput.name = inputfont.fontName;
        currentSettings.bufInput.size = inputfont.pointSize;

        [defaults setObject: inputfont.fontName forKey: @"InputFontName"];
        [defaults setObject: @(inputfont.pointSize) forKey: @"InputFontSize"];
        btnInputFont.title = fontToString(inputfont);
    }
    
    [Preferences rebuildTextAttributes];
}

- (void)changeAttributes:(id)sender
{
    NSLog(@"changeAttributes:%@", sender);
    NSDictionary * newAttributes = [sender convertAttributes:@{}];
    if (newAttributes[@"NSColor"]) {
        NSColorWell *colorWell = nil;
        NSFont *currentFont = [NSFontManager sharedFontManager].selectedFont;
        if (currentFont == gridroman)
            colorWell = clrGridFg;
        else if (currentFont == bufroman)
            colorWell = clrBufferFg;
        else if (currentFont == inputfont)
            colorWell = clrInputFg;
        colorWell.color=newAttributes[@"NSColor"];
        [self changeColor:colorWell];
    }

}

- (IBAction) changeSmartQuotes: (id)sender
{
    smartquotes = [sender state];
    NSLog(@"pref: smart quotes changed to %d", smartquotes);
    [[NSUserDefaults standardUserDefaults]
     setObject: @(smartquotes)
     forKey: @"SmartQuotes"];
     currentSettings.smartquotes = smartquotes;
//   [[NSNotificationCenter defaultCenter]
//     postNotificationName: @"PreferencesChanged" object: nil];
}

- (IBAction) changeSpaceFormatting: (id)sender
{
    spaceformat = [sender state];


    [[NSUserDefaults standardUserDefaults]
     setObject: @(spaceformat)
     forKey: @"SpaceFormat"];
    currentSettings.spaceformat = spaceformat;

    NSLog(@"pref: IBAction: space format changed to %d. Current game is %@" ,currentSettings.spaceformat, [currentSettings.games anyObject].metadata.title);
//    [[NSNotificationCenter defaultCenter]
//     postNotificationName: @"PreferencesChanged" object: nil];

}

- (IBAction) changeEnableGraphics: (id)sender
{
    dographics = [sender state];
    NSLog(@"pref: dographics changed to %d", dographics);
    [[NSUserDefaults standardUserDefaults]
     setObject: @(dographics)
     forKey: @"EnableGraphics"];
    currentSettings.dographics = dographics;

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
    currentSettings.dosound = dosound;


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
    currentSettings.dostyles = dostyles;
    [Preferences rebuildTextAttributes];
}

//- (IBAction) changeUseScreenFonts: (id)sender
//{
//    usescreenfonts = [sender state];
//    NSLog(@"pref: usescreenfonts changed to %d", usescreenfonts);
//    [[NSUserDefaults standardUserDefaults]
//     setObject: @(usescreenfonts)
//     forKey: @"ScreenFonts"];
//    [Preferences rebuildTextAttributes];
//}

@end
