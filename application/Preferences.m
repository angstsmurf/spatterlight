#import "Compatibility.h"
#import "CoreDataManager.h"
#import "Game.h"
#import "Theme.h"
#import "GlkStyle.h"

#import "main.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
    fprintf(stderr, "%s\n",                                                    \
            [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@implementation Preferences

/*
 * Preference variables, all unpacked
 */

static int defscreenw = 80;
static int defscreenh = 24;
//static float cellw = 5;ny
//static float cellh = 5;

static BOOL smartquotes = YES;
static NSUInteger spaceformat = TAG_SPACES_GAME;
static NSUInteger zoomDirection = ZOOMRESET;
//static BOOL dographics = YES;
//static BOOL dosound = NO;
//static BOOL dostyles = NO;
//static BOOL usescreenfonts = NO;

//static CGFloat gridmargin = 0;
//static CGFloat buffermargin = 0;
//static CGFloat border = 0;
//
//static CGFloat leading = 0; /* added to lineHeight */

static NSFont *bufroman;
static NSFont *gridroman;
static NSFont *inputfont;

static NSColor *bufferbg, *bufferfg;
static NSColor *gridbg, *gridfg;
static NSColor *inputfg;

static NSColor *fgcolor[8];
static NSColor *bgcolor[8];

//static NSDictionary *bufferatts[style_NUMSTYLES];
//static NSDictionary *gridatts[style_NUMSTYLES];

static Theme *theme = nil;

static Preferences *prefs = nil;

/*
 * Some color utility functions
 */

NSData *colorToData(NSColor *color) {
    NSData *data;
    CGFloat r = 0, g = 0, b = 0, a = 0;
    unsigned char buf[3];

    color = [color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];

    [color getRed:&r green:&g blue:&b alpha:&a];

    buf[0] = (unsigned char)(r * 255);
    buf[1] = (unsigned char)(g * 255);
    buf[2] = (unsigned char)(b * 255);

    data = [NSData dataWithBytes:buf length:3];

    return data;
}

NSColor *dataToColor(NSData *data) {
    NSColor *color;
    CGFloat r, g, b;
    const unsigned char *buf = data.bytes;

    if (data.length < 3)
        r = g = b = 0;
    else {
        r = buf[0] / 255.0;
        g = buf[1] / 255.0;
        b = buf[2] / 255.0;
    }

    color = [NSColor colorWithCalibratedRed:r green:g blue:b alpha:1.0];

    return color;
}

static NSColor *makehsb(CGFloat h, CGFloat s, CGFloat b) {
    return [NSColor colorWithCalibratedHue:h
                                saturation:s
                                brightness:b
                                     alpha:1.0];
}

/*
 * Load and save defaults
 */

+ (void)initFactoryDefaults {
    NSString *filename = [[NSBundle mainBundle] pathForResource:@"Defaults"
                                                         ofType:@"plist"];
    NSMutableDictionary *defaults =
        [NSMutableDictionary dictionaryWithContentsOfFile:filename];

    [defaults setObject:(@"~/Documents").stringByExpandingTildeInPath
                 forKey:@"GameDirectory"];
    [defaults setObject:(@"~/Documents").stringByExpandingTildeInPath
                 forKey:@"SaveDirectory"];

    [[NSUserDefaults standardUserDefaults] registerDefaults:defaults];
}

+ (void)readDefaults {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *name;
//    float size;

    name = [defaults objectForKey:@"themeName"];

    if (!name)
        name = @"Default";

    CoreDataManager *coreDataManager = ((AppDelegate*)[NSApplication sharedApplication].delegate).coreDataManager;

    NSManagedObjectContext *managedObjectContext = coreDataManager.mainManagedObjectContext;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    NSArray *fetchedObjects;
    NSError *error;
    fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:managedObjectContext];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"name like[c] %@", name];
    fetchedObjects = [managedObjectContext executeFetchRequest:fetchRequest error:&error];

    if (fetchedObjects == nil || fetchedObjects.count == 0) {
        NSLog(@"Preference readDefaults: Error! Saved theme %@ not found. Creating new default theme!", name);
        theme = [self classCreateDefaultThemeInContext:managedObjectContext];
        if (!theme)
            NSLog(@"Preference readDefaults: Error! Could not create default theme!");
    } else theme = [fetchedObjects objectAtIndex:0];

    [self readSettingsFromTheme:theme];
          
//    defscreenw = [[defaults objectForKey:@"DefaultWidth"] intValue];
//    defscreenh = [[defaults objectForKey:@"DefaultHeight"] intValue];
//
//    smartquotes = [[defaults objectForKey:@"SmartQuotes"] boolValue];
//    spaceformat = [[defaults objectForKey:@"SpaceFormat"] intValue];
//
//    dographics = [[defaults objectForKey:@"EnableGraphics"] boolValue];
//    dosound = [[defaults objectForKey:@"EnableSound"] boolValue];
//    dostyles = [[defaults objectForKey:@"EnableStyles"] boolValue];
//    usescreenfonts = [[defaults objectForKey:@"ScreenFonts"] boolValue];
//
//    gridbg = dataToColor([defaults objectForKey:@"GridBackground"]);
//    gridfg = dataToColor([defaults objectForKey:@"GridForeground"]);
//    bufferbg = dataToColor([defaults objectForKey:@"BufferBackground"]);
//    bufferfg = dataToColor([defaults objectForKey:@"BufferForeground"]);
//    inputfg = dataToColor([defaults objectForKey:@"InputColor"]);
//
//    gridmargin = [[defaults objectForKey:@"GridMargin"] doubleValue];
//    buffermargin = [[defaults objectForKey:@"BufferMargin"] doubleValue];
//    border = [[defaults objectForKey:@"Border"] doubleValue];
//
//    leading = [[defaults objectForKey:@"Leading"] doubleValue];
//
//    name = [defaults objectForKey:@"GridFontName"];
//    size = [[defaults objectForKey:@"GridFontSize"] doubleValue];
//    gridroman = [NSFont fontWithName:name size:size];
//    if (!gridroman) {
//        NSLog(@"pref: failed to create grid font '%@'", name);
//        gridroman = [NSFont userFixedPitchFontOfSize:0];
//    }
//
//    name = [defaults objectForKey:@"BufferFontName"];
//    size = [[defaults objectForKey:@"BufferFontSize"] doubleValue];
//    bufroman = [NSFont fontWithName:name size:size];
//    if (!bufroman) {
//        NSLog(@"pref: failed to create buffer font '%@'", name);
//        bufroman = [NSFont userFontOfSize:0];
//    }
//
//    name = [defaults objectForKey:@"InputFontName"];
//    size = [[defaults objectForKey:@"InputFontSize"] doubleValue];
//    inputfont = [NSFont fontWithName:name size:size];
//    if (!inputfont) {
//        NSLog(@"pref: failed to create input font '%@'", name);
//        inputfont = [NSFont userFontOfSize:0];
//    }
}


+ (Theme *)classCreateDefaultThemeInContext:(NSManagedObjectContext *)context {
    Theme *defaultTheme = (Theme *) [NSEntityDescription
                                     insertNewObjectForEntityForName:@"Theme"
                                     inManagedObjectContext:context];

    defaultTheme.name = @"Default";
    defaultTheme.dashes = YES;
    defaultTheme.defaultRows = 100;
    defaultTheme.defaultCols = 80;
    defaultTheme.minRows = 5;
    defaultTheme.minCols = 32;
    defaultTheme.maxRows = 1000;
    defaultTheme.maxCols = 1000;
    defaultTheme.doGraphics = YES;
    defaultTheme.doSound = YES;
    defaultTheme.doStyles = YES;
    defaultTheme.justify = NO;
    defaultTheme.smartQuotes = YES;
    defaultTheme.spaceFormat = TAG_SPACES_GAME;
    defaultTheme.border = 10;
    defaultTheme.bufferMarginX = 5;
    defaultTheme.bufferMarginY = 5;
    defaultTheme.gridMarginX = 0;
    defaultTheme.gridMarginY = 0;

    defaultTheme.winSpacingX = 0;
    defaultTheme.winSpacingY = 0;

    defaultTheme.morePrompt = nil;
    defaultTheme.spacingColor = nil;

    defaultTheme.gridBackground = [NSColor whiteColor];
    defaultTheme.bufferBackground = [NSColor whiteColor];
    defaultTheme.editable = NO;

    [defaultTheme populateStyles];

    NSSize size = [defaultTheme.gridNormal cellSize];

    defaultTheme.cellHeight = size.height;
    defaultTheme.cellWidth = size.width;
    
    return defaultTheme;
}

+ (void)readSettingsFromTheme:(Theme *)theme {

    defscreenw = theme.defaultCols;
    defscreenh = theme.defaultRows;

    smartquotes = theme.smartQuotes;
    spaceformat = (NSUInteger)theme.spaceFormat;

//    dographics = theme.doGraphics;
//    dosound = theme.doSound;
//    dostyles = theme.doStyles;
//
//    usescreenfonts = NO;

    gridbg = theme.gridBackground;
    gridfg = theme.gridNormal.color;
    bufferbg = theme.bufferBackground;
    bufferfg = theme.bufferNormal.color;
    inputfg = theme.bufInput.color;

//    gridmargin = theme.gridMarginX;
//    buffermargin = theme.bufferMarginX;
//    border = theme.border;
//
//    leading = ((NSParagraphStyle *)[theme.bufferNormal.attributeDict objectForKey:NSParagraphStyleAttributeName]).lineSpacing;

    gridroman = theme.gridNormal.font;
    if (!gridroman) {
        NSLog(@"pref: Found no grid NSFont object in theme %@, creating default", theme.name);
        gridroman = [NSFont userFixedPitchFontOfSize:0];
        [theme.gridNormal setFont:gridroman];
    }

    bufroman = theme.bufferNormal.font;
    if (!bufroman) {
        NSLog(@"pref: Found no buffer NSFont object in theme %@, creating default", theme.name);
        bufroman = [NSFont userFontOfSize:0];
        theme.bufferNormal.font = bufroman;
    }

    inputfont = [theme.bufInput.attributeDict objectForKey:NSFontAttributeName];
    if (!inputfont) {
        NSLog(@"pref: Found no bufInput NSFont object in theme %@, creating default", theme.name);
        inputfont = [NSFont userFontOfSize:0];
        theme.bufInput.font = inputfont;
    }
}



+ (void)initialize {
//    NSInteger i;

    [self initFactoryDefaults];
    [self readDefaults];

//    for (i = 0; i < style_NUMSTYLES; i++) {
//        bufferatts[i] = nil;
//        gridatts[i] = nil;
//    }

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

    bgcolor[1] = makehsb(0 / 360.0, 0.8, 0.4);   /* red */
    bgcolor[2] = makehsb(120 / 360.0, 0.8, 0.4); /* green */
    bgcolor[3] = makehsb(60 / 360.0, 0.8, 0.4);  /* yellow */
    bgcolor[4] = makehsb(230 / 360.0, 0.8, 0.4); /* blue */
    bgcolor[5] = makehsb(300 / 360.0, 0.8, 0.4); /* magenta */
    bgcolor[6] = makehsb(180 / 360.0, 0.8, 0.4); /* cyan */

    fgcolor[1] = makehsb(0 / 360.0, 0.8, 0.8);   /* red */
    fgcolor[2] = makehsb(120 / 360.0, 0.8, 0.8); /* green */
    fgcolor[3] = makehsb(60 / 360.0, 0.8, 0.8);  /* yellow */
    fgcolor[4] = makehsb(230 / 360.0, 0.8, 0.8); /* blue */
    fgcolor[5] = makehsb(300 / 360.0, 0.8, 0.8); /* magenta */
    fgcolor[6] = makehsb(180 / 360.0, 0.8, 0.8); /* cyan */

    [self rebuildTextAttributes];
}


#pragma mark Global accessors

+ (NSColor *)foregroundColor:(int)number {
    if (number < 0 || number > 7)
        return nil;
    return fgcolor[number];
}

+ (NSColor *)backgroundColor:(int)number {
    if (number < 0 || number > 7)
        return nil;
    return bgcolor[number];
}

+ (BOOL)graphicsEnabled {
    return theme.doGraphics;
}

+ (BOOL)soundEnabled {
    return theme.doSound;
}

+ (BOOL)stylesEnabled {
    return theme.doStyles;
}

+ (BOOL)useScreenFonts {
    return NO;
}

+ (BOOL)smartQuotes {
    return theme.smartQuotes;
}

+ (NSUInteger)spaceFormat {
    return (NSUInteger)theme.spaceFormat;
}

+ (NSUInteger)zoomDirection {
    return zoomDirection;
}

+ (double)lineHeight {
    return theme.cellHeight;
}

+ (double)charWidth {
    return theme.cellWidth;;
}

+ (CGFloat)gridMargins {
    return theme.gridMarginX;
}

+ (CGFloat)bufferMargins {
    return theme.bufferMarginX;
}

+ (CGFloat)border {
    return theme.border;
}

+ (CGFloat)leading {
    return theme.bufferNormal.lineSpacing;
}

+ (NSColor *)gridBackground {
    return theme.gridBackground;
}

+ (NSColor *)gridForeground {
    return theme.gridNormal.color;
}

+ (NSColor *)bufferBackground {
    return theme.bufferBackground;
}

+ (NSColor *)bufferForeground {
    return theme.bufferNormal.color;
}

+ (NSColor *)inputColor {
    return theme.bufInput.color;
}

+ (Theme *)currentTheme {
    return theme;
}

+ (Preferences *)instance {
    return prefs;
}


#pragma mark GlkStyle and attributed-string magic

//+ (NSDictionary *)attributesForGridStyle:(int)style {
//    if (style < 0 || style >= style_NUMSTYLES)
//        return nil;
//    return gridatts[style];
//}
//
//+ (NSDictionary *)attributesForBufferStyle:(int)style {
//    if (style < 0 || style >= style_NUMSTYLES)
//        return nil;
//    return bufferatts[style];
//}

+ (void)rebuildTextAttributes {
//    int style;
//    NSFontManager *mgr = [NSFontManager sharedFontManager];
//    NSMutableParagraphStyle *para;
//    NSMutableDictionary *dict;
//    NSFont *font;
//    GlkStyle *glkstyle;

    [theme populateStyles];
    NSSize cellsize = [theme.gridNormal cellSize];
    theme.cellWidth = cellsize.width;
    theme.cellHeight = cellsize.height;

//    [theme.gridNormal.attributeDict  setObject:@(-1) forKey:NSBaselineOffsetAttributeName];

    // NSLog(@"pref: rebuildTextAttributes()");

    /* make italic, bold, bolditalic font variants */

    /* update style attribute dictionaries */

//    para.lineSpacing = leading;

//    for (style = 0; style < style_NUMSTYLES; style++) {
//        /*
//         * Buffer windows
//         */
//
//        dict = [[NSMutableDictionary alloc] init];
//        [dict setObject:@(style) forKey:@"GlkStyle"];
//        [dict setObject:para forKey:NSParagraphStyleAttributeName];

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

//        if (style == style_Input)
//            [dict setObject:inputfg forKey:NSForegroundColorAttributeName];
//        else
//            [dict setObject:bufferfg forKey:NSForegroundColorAttributeName];
//
//        switch (style) {
//            case style_Emphasized:
//                font = bufitalic;
//                break;
//            case style_Preformatted:
//                font = gridroman;
//                break;
//            case style_Header:
//                font = bufheader;
//                break;
//            case style_Subheader:
//                font = bufbold;
//                break;
//            case style_Alert:
//                font = bufbolditalic;
//                break;
//            case style_Input:
//                font = inputfont;
//                break;
//            default:
//                font = bufroman;
//                break;
//        }
//        [dict setObject:font forKey:NSFontAttributeName];

//        bufferatts[style] = dict;
//        glkstyle = [theme valueForKey:[gBufferStyleNames objectAtIndex:style]];
//        glkstyle.attributeDict = [dict mutableCopy];
//
//        /*
//         * Grid windows
//         */
//
//        dict = [[NSMutableDictionary alloc] init];
//        [dict setObject:@(style) forKey:@"GlkStyle"];
//        [dict setObject:para forKey:NSParagraphStyleAttributeName];
//        [dict setObject:gridfg forKey:NSForegroundColorAttributeName];
//
//        /* for our frotz quote-box hack */
////        if (style == style_User1)
////            [dict setObject:gridbg forKey:NSBackgroundColorAttributeName];
//
//        font = gridroman;
//        switch (style) {
//            case style_Emphasized:
//                font = griditalic;
//                break;
//            case style_Preformatted:
//                font = gridroman;
//                break;
//            case style_Header:
//                font = gridbold;
//                break;
//            case style_Subheader:
//                font = gridbold;
//                break;
//            case style_Alert:
//                font = gridbolditalic;
//                break;
//        }
//        [dict setObject:font forKey:NSFontAttributeName];
//
//        gridatts[style] = dict;
//        glkstyle = [theme valueForKey:[gGridStyleNames objectAtIndex:style]];
//        glkstyle.attributeDict = [dict mutableCopy];
//    }

//    if (usescreenfonts)
//        font = gridroman.screenFont;
//    else
//        font = gridroman.printerFont;

    // NSLog(@"[font advancementForGlyph:(NSGlyph)'X'].width:%f
    // font.maximumAdvancement.width:%f [@\"X\"
    // sizeWithAttributes:@{NSFontAttributeName: font}].width:%f", [font
    // advancementForGlyph:(NSGlyph) 'X'].width, font.maximumAdvancement.width,
    // [@"X" sizeWithAttributes:@{NSFontAttributeName: font}].width);

    // This is the only way I have found to get the correct width at all sizes
//    if (NSAppKitVersionNumber < NSAppKitVersionNumber10_8)
//        cellw = [@"X" sizeWithAttributes:@{NSFontAttributeName : font}].width;
//    else
//        cellw = [font advancementForGlyph:(NSGlyph)'X'].width;
//
//    NSLayoutManager *layoutManager = [[NSLayoutManager alloc] init];
//    cellh = [layoutManager defaultLineHeightForFont:font] + leading;
//    layoutManager = nil;
    // cellh = [font ascender] + [font descender] + [font leading] + leading;

    /* send notification that prefs have changed -- trigger configure events */

    NSNotification *notification = [NSNotification notificationWithName:@"PreferencesChanged" object:theme];

    //NSLog(@"Preferences rebuildTextAttributes issued PreferencesChanged notification with object %@", theme.name);
    [[NSNotificationCenter defaultCenter]
     postNotification:notification];
}

- (void)noteCurrentThemeDidChange:(NSNotification *)notification {
    theme = notification.object;
    // Send notification that theme has changed -- trigger configure events

    [self updatePrefsPanel];
    [[NSUserDefaults standardUserDefaults] setObject:theme.name forKey:@"themeName"];

    [Preferences readSettingsFromTheme:theme];

    glktxtbuf.theme = theme;
    [glktxtbuf prefsDidChange];
}

#pragma mark - Instance -- controller for preference panel

NSString *fontToString(NSFont *font) {
    if ((int)font.pointSize == font.pointSize)
        return [NSString stringWithFormat:@"%@ %.f", font.displayName,
                (float)font.pointSize];
    else
        return [NSString stringWithFormat:@"%@ %.1f", font.displayName,
                (float)font.pointSize];
}

- (void)windowDidLoad {
    //    NSLog(@"pref: windowDidLoad()");

    [super windowDidLoad];

    self.windowFrameAutosaveName = @"PrefsWindow";
    self.window.delegate = self;

    if (!theme)
        theme = [self defaultTheme];

    // Sample text view
    glkcntrl = [[GlkController alloc] init];
    glkcntrl.theme = theme;
    glkcntrl.borderView = sampleTextBorderView;
    glkcntrl.contentView = sampleTextView;

    [sampleTextBorderView setWantsLayer:YES];
    [glkcntrl setBorderColor:theme.bufferBackground];

    glktxtbuf = [[GlkTextBufferWindow alloc] initWithGlkController:glkcntrl name:1];

    NSMutableArray *nullarray = [NSMutableArray arrayWithCapacity:stylehint_NUMHINTS];

    NSInteger i;
    for (i = 0 ; i < stylehint_NUMHINTS ; i ++)
        [nullarray addObject:[NSNull null]];
    glktxtbuf.styleHints = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
    for (i = 0 ; i < style_NUMSTYLES ; i ++) {
        [glktxtbuf.styleHints addObject:[nullarray mutableCopy]];
    }

    [sampleTextView addSubview:glktxtbuf];

    NSRect sampleTextFrame;

    sampleTextFrame.origin = NSMakePoint(theme.border, theme.border);
    sampleTextFrame.size = NSMakeSize(sampleTextView.frame.size.width - theme.border * 2,
                                      sampleTextView.frame.size.height - theme.border * 2);

    glktxtbuf.frame = sampleTextFrame;

    [glktxtbuf putString:@"Palace Gate\n" style:style_Subheader];
    [glktxtbuf putString:@"A tide of perambulators surges north along the crowded Broad Walk. " style:style_Normal];
    [glktxtbuf putString:@"(From Trinity, Infocom 1986)" style:style_Emphasized];

    [glktxtbuf restoreScrollBarStyle];

    [[NSNotificationCenter defaultCenter]
     addObserver:self
     selector:@selector(notePreferencesChanged:)
     name:@"PreferencesChanged"
     object:nil];

    [[NSNotificationCenter defaultCenter]
     addObserver:self
     selector:@selector(noteCurrentThemeDidChange:)
     name:@"CurrentThemeChanged"
     object:nil];

    [Preferences readSettingsFromTheme:theme];

    prefs = self;
    [self updatePrefsPanel];
}

- (void)notePreferencesChanged:(NSNotification *)notify {
    NSLog(@"Preferences: notePreferencesChanged notification received");
    glktxtbuf.theme = theme;
    NSRect sampleTextFrame;
    sampleTextFrame.origin = NSMakePoint(theme.border, theme.border);
    sampleTextFrame.size = NSMakeSize(sampleTextView.frame.size.width - theme.border * 2,
                                      sampleTextView.frame.size.height - theme.border * 2);
    glktxtbuf.frame = sampleTextFrame;
    [glktxtbuf restoreScrollBarStyle];
    [glktxtbuf prefsDidChange];
}

- (void)updatePrefsPanel {
    if (!theme)
        theme = self.defaultTheme;
    if (!theme.gridNormal.attributeDict)
        [theme populateStyles];
    clrGridFg.color = theme.gridNormal.color;
    clrGridBg.color = theme.gridBackground;
    clrBufferFg.color = theme.bufferNormal.color;
    clrBufferBg.color = theme.bufferBackground;
    clrInputFg.color = theme.bufInput.color;

    txtGridMargin.floatValue = theme.gridMarginX;
    txtBufferMargin.floatValue = theme.bufferMarginX;
    txtLeading.doubleValue = theme.bufferNormal.lineSpacing;

    txtCols.intValue = theme.defaultCols;
    txtRows.intValue = theme.defaultRows;

    txtBorder.intValue = theme.border;

    btnGridFont.title = fontToString(theme.gridNormal.font);
    btnBufferFont.title = fontToString(theme.bufferNormal.font);
    btnInputFont.title = fontToString(theme.bufInput.font);

    btnSmartQuotes.state = theme.smartQuotes;
    btnSpaceFormat.state = theme.spaceFormat;

    btnEnableGraphics.state = theme.doGraphics;
    btnEnableSound.state = theme.doSound;
    btnEnableStyles.state = theme.doSound;
//    btnUseScreenFonts.state = usescreenfonts;

    NSArray *themes = [self themeTableArray];
    NSIndexSet *selection = [NSIndexSet indexSetWithIndex:[themes indexOfObject:theme]];
    if (selection && selection.count)
        [themesTableView selectRowIndexes:selection byExtendingSelection:NO];
}

@synthesize defaultTheme = _defaultTheme;

- (Theme *)defaultTheme {
    if (_defaultTheme == nil) {
        NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
        fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:[self managedObjectContext]];
        fetchRequest.predicate = [NSPredicate predicateWithFormat:@"name like[c] %@", @"Default"];
        NSError *error;
        NSArray *fetchedObjects = [_managedObjectContext executeFetchRequest:fetchRequest error:&error];

        if (fetchedObjects && fetchedObjects.count) {
            _defaultTheme = [fetchedObjects objectAtIndex:0];
        } else {
            _defaultTheme = [self createDefaultTheme];
        }
    }
    return _defaultTheme;
}

@synthesize coreDataManager = _coreDataManager;

- (CoreDataManager *)coreDataManager {
    if (_coreDataManager == nil) {
        _coreDataManager = ((AppDelegate*)[NSApplication sharedApplication].delegate).coreDataManager;
    }
    return _coreDataManager;
}

@synthesize managedObjectContext = _managedObjectContext;

- (NSManagedObjectContext *)managedObjectContext {
    if (_managedObjectContext == nil) {
        _managedObjectContext = [self coreDataManager].mainManagedObjectContext;
    }
    return _managedObjectContext;
}

- (Theme *)createDefaultTheme {
    Theme *defaultTheme = (Theme *) [NSEntityDescription
                              insertNewObjectForEntityForName:@"Theme"
                              inManagedObjectContext:_managedObjectContext];
    
    defaultTheme.name = @"Default";
    defaultTheme.dashes = YES;
    defaultTheme.defaultRows = 100;
    defaultTheme.defaultCols = 80;
    defaultTheme.minRows = 5;
    defaultTheme.minCols = 32;
    defaultTheme.maxRows = 1000;
    defaultTheme.maxCols = 1000;
    defaultTheme.doGraphics = YES;
    defaultTheme.doSound = YES;
    defaultTheme.doStyles = YES;
    defaultTheme.justify = NO;
    defaultTheme.smartQuotes = YES;
    defaultTheme.spaceFormat = TAG_SPACES_GAME;
    defaultTheme.border = 10;
    defaultTheme.bufferMarginX = 5;
    defaultTheme.bufferMarginY = 5;
    defaultTheme.gridMarginX = 0;
    defaultTheme.gridMarginY = 0;

    defaultTheme.winSpacingX = 0;
    defaultTheme.winSpacingY = 0;

    defaultTheme.morePrompt = nil;
    defaultTheme.spacingColor = nil;

    defaultTheme.gridBackground = [NSColor whiteColor];
    defaultTheme.bufferBackground = [NSColor whiteColor];
    defaultTheme.editable = NO;

    [defaultTheme populateStyles];

    NSSize size = [defaultTheme.gridNormal cellSize];

    defaultTheme.cellHeight = size.height;
    defaultTheme.cellWidth = size.width;

    return defaultTheme;
}

- (void)createDefaultThemes {

    Theme *darkTheme;

    NSArray *fetchedObjects;
    NSError *error;

    // First, check if they already exist
    // If they do, delete them

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];

    fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:self.managedObjectContext];
   
    // Reset theme Dark (whether it exists or not)

    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"name like[c] %@", @"Dark"];

    fetchedObjects = [_managedObjectContext executeFetchRequest:fetchRequest error:&error];

    if (fetchedObjects && fetchedObjects.count) {
        NSLog(@"Dark theme already exists!");
        return;
    } else {
        darkTheme = [self.defaultTheme clone];
        darkTheme.name = @"Dark";
    }

    darkTheme.gridBackground = [NSColor blackColor];
    darkTheme.bufferBackground = [NSColor blackColor];
    if (!darkTheme.bufferNormal.attributeDict)
        [darkTheme populateStyles];
    [darkTheme.bufferNormal setColor:[NSColor whiteColor]];
    [darkTheme.gridNormal setColor:[NSColor whiteColor]];
    [darkTheme.bufInput setColor:[NSColor redColor]];
    [darkTheme populateStyles];

    fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:_managedObjectContext];

    fetchRequest.predicate = nil;
    fetchedObjects = [_managedObjectContext executeFetchRequest:fetchRequest error:&error];

    for (Game *game in fetchedObjects)
        game.theme = self.defaultTheme;

    fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:_managedObjectContext];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"name like[c] %@", @"Default"];

    fetchedObjects = [_managedObjectContext executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"createDefaultThemes: %@",error);
    }

    if (fetchedObjects.count > 1)
    {
        NSLog(@"createDefaultThemes: Found more than one Theme object with name Default (total %ld)", fetchedObjects.count);
    }
    else if (fetchedObjects.count == 0)
    {
        NSLog(@"createDefaultThemes: Found no Ifid object with with name Default");
    }

    if ([fetchedObjects objectAtIndex:0] != self.defaultTheme) {
        NSLog(@"createDefaultThemes: something went wrong");
    } else
        NSLog(@"createDefaultThemes successful");
}

#pragma mark Themes Table View Magic

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    if (tableView == themesTableView) {
        NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
        NSArray *fetchedObjects;
        NSError *error;
        fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:self.managedObjectContext];
        fetchRequest.includesPropertyValues = NO;

        fetchedObjects = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];

        if (fetchedObjects == nil) {
            return 0;
        }

        return (NSInteger)fetchedObjects.count;
    }
    return 0;
}

- (id) tableView: (NSTableView*)tableView
objectValueForTableColumn: (NSTableColumn*)column
             row:(NSInteger)row
{
    if (tableView == themesTableView) {
        NSArray *fetchedObjects = [self themeTableArray];

        if (fetchedObjects == nil) {
            return 0;
        }

        Theme *t = [fetchedObjects objectAtIndex:(NSUInteger)row];
        return t.name;
    }
    return nil;
}

- (void)tableViewSelectionDidChange:(id)notification {
    NSTableView *tableView = [notification object];
    if (tableView == themesTableView) {
        NSIndexSet *rows = tableView.selectedRowIndexes;
        if (rows.count > 1) {
            NSLog(@"The user has somehow selected more than one theme in preferences window!?");
            return;
        }
        
        if (rows.count == 0) {
            // All selected themes were de-selected
            return;
        }

        NSArray *fetchedObjects = [self themeTableArray];

        if (fetchedObjects == nil) {
            return;
        }

        theme = [fetchedObjects objectAtIndex:rows.firstIndex];
        // Send notification that theme has changed -- trigger configure events

        [self updatePrefsPanel];
        [[NSUserDefaults standardUserDefaults] setObject:theme.name forKey:@"themeName"];

        notification = [NSNotification notificationWithName:@"ThemeChanged" object:theme];
        [Preferences readSettingsFromTheme:theme];

        glktxtbuf.theme = theme;
        [glktxtbuf prefsDidChange];

        NSLog(@"Preferences tableViewSelectionDidChange issued themeChanged notification with object %@", theme.name);
        [[NSNotificationCenter defaultCenter]
         postNotification:notification];

        return;
    }
    return;
}


-(BOOL)tableView:(NSTableView *)tableView
shouldEditTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)rowIndex {
    NSLog(@"Preferences tableView:shouldEditTableColumn:row:");
    if (tableView == themesTableView) {
        NSArray *themes = [self themeTableArray];
        if ([[themes objectAtIndex:(NSUInteger)rowIndex] editable]) {
            return YES;
        }
    }
    return NO;
}

- (void)controlTextDidEndEditing:(NSNotification *)notification {
    Theme *renamedTheme;
    if ([themesTableView rowForView:notification.object] != -1) {
        NSUInteger row = (NSUInteger)themesTableView.selectedRow;
        NSString *newName = ((NSTextField *)notification.object).stringValue;
        NSArray *themes = [self themeTableArray];
        if ([self findThemeByName:newName] == nil) {
            if (row < themes.count) {
                renamedTheme = [themes objectAtIndex:row];
                renamedTheme.name = newName;
            } else {
                // We deleted a theme while editing its name
                renamedTheme = theme;
            }
            [themesTableView reloadData];
            themes = [self themeTableArray];
            row = [themes indexOfObject:renamedTheme];
            NSIndexSet *set = [NSIndexSet indexSetWithIndex:row];
            [themesTableView selectRowIndexes:set byExtendingSelection:NO];
            [themesTableView scrollRowToVisible:(NSInteger)row];
        }
    }
}

- (BOOL)control:(NSControl *)control
  isValidObject:(id)obj {
    NSLog(@"control:isValidObject:");
    NSInteger result = [themesTableView rowForView:control];
    if (result != -1) {
        NSLog(@"control:isValidObject:obj is a view in themesTableView. Row:%ld", result);
        if ([obj isKindOfClass:[NSString class]]) {
            if ([self findThemeByName:(NSString *)obj] == nil) {
             return YES;
            } else {
                NSBeep();
                return NO;
            }
        }
    }
    return YES;
}

- (NSArray *)themeTableArray {
    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    NSArray *fetchedObjects;
    NSError *error = nil;
    fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:self.managedObjectContext];
    fetchRequest.sortDescriptors = @[[NSSortDescriptor sortDescriptorWithKey:@"name" ascending:YES]];
    fetchedObjects = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];
    if (error) {
        NSLog(@"Preferences themeTableArray: %@", error);
        return nil;
    }
    return fetchedObjects;
}

- (IBAction)pushedUseThemeForAll:(id)sender {
    btnUseThemeForAll.state = NSOnState;
    btnUseThemeForRunning.state = NSOffState;
    btnUseThemeForSelected.state = NSOffState;

}

- (IBAction)pushedUseThemeForRunning:(id)sender {
    btnUseThemeForAll.state = NSOffState;
    btnUseThemeForRunning.state = NSOnState;
    btnUseThemeForSelected.state = NSOffState;
}

- (IBAction)pushedUseThemeForSelected:(id)sender {
    btnUseThemeForAll.state = NSOffState;
    btnUseThemeForRunning.state = NSOffState;
    btnUseThemeForSelected.state = NSOnState;
}

- (IBAction)addTheme:(id)sender {
    Theme *newTheme = theme.clone;
    NSString *name = @"New theme";
    NSUInteger counter = 1;
    while ([self findThemeByName:name]) {
        name = [NSString stringWithFormat:@"New theme %ld", counter++];
    }
    newTheme.name = name;
    newTheme.editable = YES;
    theme = newTheme;
    [themesTableView reloadData];
    NSArray *themes = [self themeTableArray];
    NSUInteger row = [themes indexOfObject:theme];
    NSIndexSet *set = [NSIndexSet indexSetWithIndex:row];
    [themesTableView selectRowIndexes:set byExtendingSelection:NO];
    [themesTableView editColumn:0 row:(NSInteger)row withEvent:nil select:YES];
}

- (IBAction)deleteTheme:(id)sender {
    NSIndexSet *rows = themesTableView.selectedRowIndexes;
    if (!rows.count)
        //Should never happen
        return;

    NSArray *themes = [self themeTableArray];

    Theme *selectedTheme = [themes objectAtIndex:rows.firstIndex];
    if (selectedTheme == theme) {
        //Should always be true

        if (!selectedTheme.editable || themes.count == 1) {
            NSBeep();
            return;
        }

        if (selectedTheme.defaultParent) {
            NSLog(@"We are deleting theme %@, so switch to theme %@", theme.name, selectedTheme.defaultParent.name)
            theme = selectedTheme.defaultParent;
        } else {
            NSEnumerator *enumerator = [themes objectEnumerator];
            Theme *nextTheme;
            while (nextTheme = [enumerator nextObject]) {
                if (nextTheme != selectedTheme) {
                    theme = nextTheme;
                    break;
                }
            }
        }
        if (theme == selectedTheme) {
            NSLog(@"Error! Tried to delete last remaining theme!");
            NSBeep();
            return;
        }
        for (Theme *child in selectedTheme.defaultChild)
            child.defaultParent = theme;
        [self.managedObjectContext deleteObject:selectedTheme];
        [themesTableView reloadData];
        NSUInteger row = [themes indexOfObject:theme];
        NSIndexSet *set = [NSIndexSet indexSetWithIndex:row];
        [themesTableView selectRowIndexes:set byExtendingSelection:NO];
        [themesTableView scrollRowToVisible:(NSInteger)row];
        [self.window makeFirstResponder:themesTableView];
    }
}


- (Theme *)findThemeByName:(NSString *)name {

    if (!name)
        return nil;
    
    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    NSArray *fetchedObjects;
    NSError *error;
    fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:self.managedObjectContext];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"name like[c] %@", name];
    fetchRequest.includesPropertyValues = NO;
    fetchedObjects = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];

    if (fetchedObjects == nil || fetchedObjects.count == 0) {
        return nil;
    }

    return [fetchedObjects objectAtIndex:0];
}

#pragma mark User actions

- (IBAction)changeDefaultSize:(id)sender {
    if (sender == txtCols) {
        defscreenw = [sender intValue];
        if (defscreenw < 5)
            defscreenw = 5;
        if (defscreenw > 200)
            defscreenw = 200;
        txtCols.intValue = defscreenw;
        theme.defaultCols = defscreenw;
//        [[NSUserDefaults standardUserDefaults] setObject:@(defscreenw)
//                                                  forKey:@"DefaultWidth"];
    }
    if (sender == txtRows) {
        defscreenh = [sender intValue];
        if (defscreenh < 5)
            defscreenh = 5;
        if (defscreenh > 200)
            defscreenh = 200;
        txtRows.intValue = defscreenh;
        theme.defaultRows = defscreenh;
//        [[NSUserDefaults standardUserDefaults] setObject:@(defscreenh)
//                                                  forKey:@"DefaultHeight"];
    }

    /* send notification that default size has changed -- resize all windows */
    NSNotification *notification = [NSNotification notificationWithName:@"DefaultSizeChanged" object:[Preferences currentTheme]];
    [[NSNotificationCenter defaultCenter]
     postNotification:notification];
}

- (IBAction)changeColor:(id)sender {
    NSString *key = nil;

    NSColor *color = [sender color];
    if (!color) {
        NSLog(@"Preferences changeColor called with invalid color!");
        return;
    }

    if (sender == clrGridFg) {
       key = @"gridNormal";
    } else if (sender == clrGridBg) {
        theme.gridBackground = color;
    } else if (sender == clrBufferFg) {
        key = @"bufferNormal";
    } else if (sender == clrBufferBg) {
        theme.bufferBackground = color;
    } else if (sender == clrInputFg) {
        key = @"bufInput";
    } else return;

    if (key) {
        GlkStyle *style = [theme valueForKey:key];
        if (!style.attributeDict) {
            NSLog(@"Preferences changeColor called with invalid theme object!");
            return;
        }
        style.color=color;

//        [[NSUserDefaults standardUserDefaults] setObject:colorToData(*colorp)
//                                                  forKey:key];
    }

    [Preferences rebuildTextAttributes];

    if (sender == clrBufferFg) {
        NSLog(@"User changed text color to %@", theme.bufferNormal.color);
        for (NSString *str in gBufferStyleNames)
            NSLog(@"Text color of %@ is now %@", str, [[theme valueForKey:str] color]);
    }
}

- (IBAction)changeMargin:(id)sender;
{
    NSString *key = nil;
    NSInteger val = 0;

    val = [sender intValue];

    if (sender == txtGridMargin) {
        key = @"GridMargin";
        theme.gridMarginX = val;
    }
    if (sender == txtBufferMargin) {
        key = @"BufferMargin";
        theme.bufferMarginX = val;
    }

    if (key) {
//        [[NSUserDefaults standardUserDefaults] setObject:@(val) forKey:key];
        [Preferences rebuildTextAttributes];
    }
}

- (IBAction)changeLeading:(id)sender {
    theme.bufferNormal.lineSpacing = [sender floatValue];     
//    [[NSUserDefaults standardUserDefaults] setObject:@(leading)
//                                              forKey:@"Leading"];
    [Preferences rebuildTextAttributes];
}

- (IBAction)changeSmartQuotes:(id)sender {
    theme.smartQuotes = [sender state];
    NSLog(@"pref: smart quotes changed to %d", smartquotes);
//    [[NSUserDefaults standardUserDefaults] setObject:@(smartquotes)
//                                              forKey:@"SmartQuotes"];
}

- (IBAction)changeSpaceFormatting:(id)sender {
    theme.spaceFormat = [sender state];
    NSLog(@"pref: space format changed to %d", theme.spaceFormat);
//    [[NSUserDefaults standardUserDefaults] setObject:@(spaceformat)
//                                              forKey:@"SpaceFormat"];
}

- (IBAction)changeEnableGraphics:(id)sender {
    theme.doGraphics = [sender state];
    NSLog(@"pref: dographics changed to %d", theme.doGraphics);
//    [[NSUserDefaults standardUserDefaults] setObject:@(dographics)
//                                              forKey:@"EnableGraphics"];

    /* send notification that prefs have changed -- tell clients that graphics
     * are off limits */
    [[NSNotificationCenter defaultCenter]
     postNotificationName:@"PreferencesChanged"
     object:[Preferences currentTheme]];

    NSLog(@"Preferences changeEnableGraphics issued PreferencesChanged notification with object %@", [Preferences currentTheme].name);
}

- (IBAction)changeEnableSound:(id)sender {
    theme.doSound = [sender state];
    NSLog(@"pref: dosound changed to %d", theme.doSound);
//    [[NSUserDefaults standardUserDefaults] setObject:@(dosound)
//                                              forKey:@"EnableSound"];

    /* send notification that prefs have changed -- tell clients that sound is
     * off limits */
    [[NSNotificationCenter defaultCenter]
     postNotificationName:@"PreferencesChanged"
     object:[Preferences currentTheme]];

    NSLog(@"Preferences changeEnableGraphics issued PreferencesChanged notification with object %@", [Preferences currentTheme].name);
}

- (IBAction)changeEnableStyles:(id)sender {
    theme.doStyles = [sender state];
    NSLog(@"pref: dostyles changed to %d", theme.doStyles);
//    [[NSUserDefaults standardUserDefaults] setObject:@(dostyles)
//                                              forKey:@"EnableStyles"];
    [Preferences rebuildTextAttributes];
}

//- (IBAction)changeUseScreenFonts:(id)sender {
//    usescreenfonts = [sender state];
//    NSLog(@"pref: usescreenfonts changed to %d", usescreenfonts);
//    [[NSUserDefaults standardUserDefaults] setObject:@(usescreenfonts)
//                                              forKey:@"ScreenFonts"];
//    [Preferences rebuildTextAttributes];
//}

- (IBAction)changeBorderSize:(id)sender {
    theme.border = [sender intValue];
//    [[NSUserDefaults standardUserDefaults] setObject:@(border)
//                                              forKey:@"Border"];

    /* send notification that prefs have changed -- tell clients that border has
     * changed */
    [[NSNotificationCenter defaultCenter]
     postNotificationName:@"PreferencesChanged"
     object:[Preferences currentTheme]];

    NSLog(@"Preferences changeBorderSize issued PreferencesChanged notification with object %@", [Preferences currentTheme].name);
}

#pragma mark Zoom

+ (void)zoomIn {
    zoomDirection = ZOOMRESET;
    if (gridroman.pointSize < 100) {
        zoomDirection = ZOOMIN;
        [self scale:(gridroman.pointSize + 1) / gridroman.pointSize];
    }
}

+ (void)zoomOut {
    zoomDirection = ZOOMRESET;
    if (gridroman.pointSize > 6) {
        zoomDirection = ZOOMOUT;
        [self scale:(gridroman.pointSize - 1) / gridroman.pointSize];
    }
}

+ (void)zoomToActualSize {
    zoomDirection = ZOOMRESET;
    [self scale:12 / gridroman.pointSize];
}

+ (void)scale:(CGFloat)scalefactor {
    // NSLog(@"Preferences scale: %f", scalefactor);

    if (scalefactor < 0)
        scalefactor = fabs(scalefactor);

    if ((scalefactor < 1.01 && scalefactor > 0.99) || scalefactor == 0.0)
        scalefactor = 1.0;

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    CGFloat fontSize;

    fontSize = gridroman.pointSize;
    fontSize *= scalefactor;
    if (fontSize > 0) {
        gridroman = [NSFont fontWithDescriptor:gridroman.fontDescriptor
                                          size:fontSize];
        [defaults setObject:@(fontSize) forKey:@"GridFontSize"];
    }

    fontSize = bufroman.pointSize;
    fontSize *= scalefactor;
    if (fontSize > 0) {
        bufroman = [NSFont fontWithDescriptor:bufroman.fontDescriptor
                                         size:fontSize];
        [defaults setObject:@(fontSize) forKey:@"BufferFontSize"];
    }

    fontSize = inputfont.pointSize;
    fontSize *= scalefactor;
    if (fontSize > 0) {
        inputfont = [NSFont fontWithDescriptor:inputfont.fontDescriptor
                                          size:fontSize];
        [defaults setObject:@(fontSize) forKey:@"InputFontSize"];
    }

//    if (leading * scalefactor > 0) {
//        leading *= scalefactor;
//        [defaults setObject:@(leading) forKey:@"Leading"];
//    }
//
//    if (gridmargin * scalefactor > 0) {
//        gridmargin *= scalefactor;
//        [defaults setObject:@(gridmargin) forKey:@"GridMargin"];
//    }
//
//    if (buffermargin * scalefactor > 0) {
//        buffermargin *= scalefactor;
//        [defaults setObject:@(buffermargin) forKey:@"BufferMargin"];
//    }

//    if (border * scalefactor > 0) {
//        border *= scalefactor;
//        [defaults setObject:@(border) forKey:@"Border"];
//    }

    [Preferences rebuildTextAttributes];

    /* send notification that default size has changed -- resize all windows */
    [[NSNotificationCenter defaultCenter]
     postNotificationName:@"DefaultSizeChanged"
     object:nil];
}

- (void)updatePanelAfterZoom {
    btnGridFont.title = fontToString(gridroman);
    btnBufferFont.title = fontToString(bufroman);
    btnInputFont.title = fontToString(inputfont);
//    txtLeading.floatValue = leading;
//    txtGridMargin.floatValue = gridmargin;
//    txtBufferMargin.floatValue = buffermargin;
//    txtBorder.intValue = border;
}

#pragma mark Font panel

- (IBAction)showFontPanel:(id)sender {
    selfontp = nil;
    colorp = nil;
    colorp2 = nil;

    if (sender == btnGridFont) {
        selfontp = &gridroman;
        colorp = &gridfg;
        colorp2 = &gridbg;
    }
    if (sender == btnBufferFont) {
        selfontp = &bufroman;
        colorp = &bufferfg;
        colorp2 = &bufferbg;
    }
    if (sender == btnInputFont) {
        selfontp = &inputfont;
        colorp = &inputfg;
        colorp2 = &bufferbg;
    }

    if (selfontp) {
        NSDictionary *attr =
        @{@"NSColor" : *colorp, @"NSDocumentBackgroundColor" : *colorp2};

        [self.window makeFirstResponder:self.window];

        [NSFontManager sharedFontManager].target = self;
        [NSFontPanel sharedFontPanel].delegate = self;
        [[NSFontPanel sharedFontPanel] makeKeyAndOrderFront:self];

        [[NSFontManager sharedFontManager] setSelectedAttributes:attr
                                                      isMultiple:NO];
        [[NSFontManager sharedFontManager] setSelectedFont:*selfontp
                                                isMultiple:NO];
    }
}

- (IBAction)changeFont:(id)fontManager {
//    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    if (selfontp) {
        *selfontp = [fontManager convertFont:*selfontp];
    } else {
        NSLog(@"Error! Preferences changeFont called with no font selected");
        return;
    }

    if (selfontp == &gridroman) {
        [theme.gridNormal setFont:gridroman];
//        [defaults setObject:gridroman.fontName forKey:@"GridFontName"];
//        [defaults setObject:@(gridroman.pointSize) forKey:@"GridFontSize"];
        btnGridFont.title = fontToString(gridroman);
    }

    if (selfontp == &bufroman) {
        [theme.bufferNormal setFont:bufroman];
//
//        [defaults setObject:bufroman.fontName forKey:@"BufferFontName"];
//        [defaults setObject:@(bufroman.pointSize) forKey:@"BufferFontSize"];
        btnBufferFont.title = fontToString(bufroman);
    }

    if (selfontp == &inputfont) {
        [theme.bufInput setFont:inputfont];
//
//        [defaults setObject:inputfont.fontName forKey:@"InputFontName"];
//        [defaults setObject:@(inputfont.pointSize) forKey:@"InputFontSize"];
        btnInputFont.title = fontToString(inputfont);
    }

    [Preferences rebuildTextAttributes];
}

// This is sent from the font panel when changing font style there

- (void)changeAttributes:(id)sender {
    NSLog(@"changeAttributes:%@", sender);

    NSDictionary *newAttributes = [sender convertAttributes:@{}];

    NSLog(@"changeAttributes: Keys in newAttributes:");
    for (NSString *key in newAttributes.allKeys) {
        NSLog(@" %@ : %@", key, [newAttributes objectForKey:key]);
    }

    //	"NSForegroundColorAttributeName"	"NSColor"
    //	"NSUnderlineStyleAttributeName"		"NSUnderline"
    //	"NSStrikethroughStyleAttributeName"	"NSStrikethrough"
    //	"NSUnderlineColorAttributeName"		"NSUnderlineColor"
    //	"NSStrikethroughColorAttributeName"	"NSStrikethroughColor"
    //	"NSShadowAttributeName"				"NSShadow"

    if ([newAttributes objectForKey:@"NSColor"]) {
        NSColorWell *colorWell = nil;
        NSFont *currentFont = [NSFontManager sharedFontManager].selectedFont;
        if (currentFont == gridroman)
            colorWell = clrGridFg;
        else if (currentFont == bufroman)
            colorWell = clrBufferFg;
        else if (currentFont == inputfont)
            colorWell = clrInputFg;
        colorWell.color = [newAttributes objectForKey:@"NSColor"];
        [self changeColor:colorWell];
    }
}

// This is sent from the font panel when changing background color there

- (void)changeDocumentBackgroundColor:(id)sender {
//    NSLog(@"changeDocumentBackgroundColor");

    NSColorWell *colorWell = nil;
    NSFont *currentFont = [NSFontManager sharedFontManager].selectedFont;
    if (currentFont == gridroman)
        colorWell = clrGridBg;
    else if (currentFont == bufroman)
        colorWell = clrBufferBg;
    else if (currentFont == inputfont)
        colorWell = clrBufferBg;
    colorWell.color = [sender color];
    [self changeColor:colorWell];
}

- (NSUInteger)validModesForFontPanel:(NSFontPanel *)fontPanel {
    return NSFontPanelAllModesMask;
//    NSFontPanelFaceModeMask | NSFontPanelCollectionModeMask |
//    NSFontPanelSizeModeMask | NSFontPanelTextColorEffectModeMask |
//    NSFontPanelDocumentColorEffectModeMask;
}

- (void)windowWillClose:(id)sender {
    if ([[NSFontPanel sharedFontPanel] isVisible])
        [[NSFontPanel sharedFontPanel] orderOut:self];
}

@end
