#import "Compatibility.h"
#import "CoreDataManager.h"
#import "Game.h"
#import "Theme.h"
#import "ThemeArrayController.h"
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

static BOOL smartquotes = YES;
static NSUInteger spaceformat = TAG_SPACES_GAME;
static NSUInteger zoomDirection = ZOOMRESET;

static NSFont *bufroman;
static NSFont *gridroman;
static NSFont *inputfont;

static NSColor *bufferbg, *bufferfg;
static NSColor *gridbg, *gridfg;
static NSColor *inputfg;

static NSColor *fgcolor[8];
static NSColor *bgcolor[8];

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
        theme = [Preferences createDefaultThemeInContext:managedObjectContext];
        if (!theme)
            NSLog(@"Preference readDefaults: Error! Could not create default theme!");
    } else theme = [fetchedObjects objectAtIndex:0];

    [self readSettingsFromTheme:theme];
}


+ (Theme *)createDefaultThemeInContext:(NSManagedObjectContext *)context {
    NSArray *fetchedObjects;
    NSError *error = nil;

    // First, check if ißt already exists
    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:context];

    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"name like[c] %@", @"Default"];
    fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];

    if (fetchedObjects && fetchedObjects.count)
        return [fetchedObjects objectAtIndex:0];
    else if (error != nil) {
        NSLog(@"Preferences createDefaultThemeInContext: %@", error);
        return nil;
    }

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

    gridbg = theme.gridBackground;
    gridfg = theme.gridNormal.color;
    bufferbg = theme.bufferBackground;
    bufferfg = theme.bufferNormal.color;
    inputfg = theme.bufInput.color;

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

    [self initFactoryDefaults];
    [self readDefaults];

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

+ (void)rebuildTextAttributes {

    [theme populateStyles];
    NSSize cellsize = [theme.gridNormal cellSize];
    theme.cellWidth = cellsize.width;
    theme.cellHeight = cellsize.height;

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
    [self changeThemeName:theme.name];

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
    disregardTableSelection = YES;

    self.windowFrameAutosaveName = @"PrefsWindow";
    self.window.delegate = self;
    themesTableView.autosaveName = @"ThemesTable";

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

    _scrollView.scrollerStyle = NSScrollerStyleOverlay;
    _scrollView.drawsBackground = YES;
    _scrollView.hasHorizontalScroller = NO;
    _scrollView.hasVerticalScroller = YES;
    _scrollView.verticalScroller.alphaValue = 100;
    _scrollView.autohidesScrollers = YES;
    _scrollView.borderType = NSNoBorder;

    [self changeThemeName:theme.name];
    [_addAndRemove setEnabled:theme.editable forSegment:1];
    [self performSelector:@selector(restoreThemeSelection:) withObject:theme afterDelay:0.1];
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
}

@synthesize defaultTheme = _defaultTheme;

- (Theme *)defaultTheme {
    if (_defaultTheme == nil) {
        NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
        fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:[self managedObjectContext]];
        fetchRequest.predicate = [NSPredicate predicateWithFormat:@"name like[c] %@", @"Default"];
        NSError *error = nil;
        NSArray *fetchedObjects = [_managedObjectContext executeFetchRequest:fetchRequest error:&error];

        if (fetchedObjects && fetchedObjects.count) {
            _defaultTheme = [fetchedObjects objectAtIndex:0];
        } else {
            if (error != nil)
                NSLog(@"Preferences defaultTheme: %@", error);
            _defaultTheme = [Preferences createDefaultThemeInContext:_managedObjectContext];
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

- (void)createDefaultThemes {

    Theme *darkTheme;
    NSArray *fetchedObjects;
    NSError *error;

    // First, check if they already exist

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];

    fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:self.managedObjectContext];
   
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
    darkTheme.editable = NO;

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

- (void)restoreThemeSelection:(id)sender {
    NSArray *themes = _arrayController.arrangedObjects;
    theme = sender;
    if (![themes containsObject:sender]) {
        NSLog(@"selected theme not found!");
        return;
    }

    NSUInteger row = [themes indexOfObject:theme];

    [_arrayController setSelectionIndex:row];
    themesTableView.allowsEmptySelection = NO;
    [themesTableView scrollRowToVisible:(NSInteger)row];
    disregardTableSelection = NO;
}

- (void)tableViewSelectionDidChange:(id)notification {
    NSTableView *tableView = [notification object];
    if (tableView == themesTableView) {
        if (_arrayController.selectedTheme == theme || disregardTableSelection == YES)
            return;
        theme = _arrayController.selectedTheme;
        // Send notification that theme has changed -- trigger configure events

        [self updatePrefsPanel];
        [self changeThemeName:theme.name];
        [_addAndRemove setEnabled:theme.editable forSegment:1];
        notification = [NSNotification notificationWithName:@"ThemeChanged" object:theme];
        [Preferences readSettingsFromTheme:theme];

        glktxtbuf.theme = theme;
        [glktxtbuf prefsDidChange];

//        NSLog(@"Preferences tableViewSelectionDidChange issued themeChanged notification with object %@", theme.name);
        [[NSNotificationCenter defaultCenter]
         postNotification:notification];

        return;
    }
    return;
}

- (void)changeThemeName:(NSString *)name {
    [[NSUserDefaults standardUserDefaults] setObject:name forKey:@"themeName"];
    _settingsThemeHeader.stringValue = [NSString stringWithFormat:@"Settings for theme %@", name];
}

- (BOOL)control:(NSControl *)control
  isValidObject:(id)obj {
    NSLog(@"control:isValidObject:");
    NSInteger result = [themesTableView rowForView:control];
    if (result != -1) {
        if ([obj isKindOfClass:[NSString class]]) {
            return [self notDuplicate:obj];
        }
    }
    return YES;
}

- (BOOL)notDuplicate:(NSString *)string {
    NSArray *themes = [_arrayController arrangedObjects];
    for (Theme *aTheme in themes) {
        if ([aTheme.name isEqualToString:string] && [themes indexOfObject:aTheme] != [themes indexOfObject:_arrayController.selectedTheme])
            return NO;
    }
    return YES;
}


- (BOOL)control:(NSControl *)control
textShouldBeginEditing:(NSText *)fieldEditor {
    NSLog(@"textShouldBeginEditing: %@", fieldEditor.string);
    return YES;
}

- (BOOL)control:(NSControl *)control
textShouldEndEditing:(NSText *)fieldEditor {
    NSLog(@"textShouldEndEditing: %@", fieldEditor.string);
    [self changeThemeName:fieldEditor.string];
    return YES;
}


- (void)controlTextDidEndEditing:(NSNotification *)obj {
    NSLog(@"Preferences controlTextDidEndEditing: %@", obj.object);
}

- (NSArray *)sortDescriptors {
    return @[[NSSortDescriptor sortDescriptorWithKey:@"editable" ascending:YES], [NSSortDescriptor sortDescriptorWithKey:@"name" ascending:YES selector:@selector(localizedStandardCompare:)]];
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

- (void)editNewEntry:(id)sender {
    NSInteger row = (NSInteger)[_arrayController selectionIndex];
    NSTableCellView* cellView = (NSTableCellView*)[themesTableView viewAtColumn:0 row:row makeIfNecessary:YES];
    if ([cellView.textField acceptsFirstResponder]) {
        [cellView.window makeFirstResponder:cellView.textField];
        [themesTableView scrollRowToVisible:(NSInteger)row];
    }

}

- (IBAction)clickedSegmentedControl:(id)sender {

    NSInteger row;
    NSTableCellView* cellView;
    
    NSInteger clickedSegment = [sender selectedSegment];
    NSInteger clickedSegmentTag = [[sender cell] tagForSegment:clickedSegment];
    switch (clickedSegmentTag) {
        case 0:
            row = (NSInteger)[_arrayController selectionIndex];
            cellView = (NSTableCellView*)[themesTableView viewAtColumn:0 row:row makeIfNecessary:YES];
            if ([self notDuplicate:cellView.textField.stringValue]) {
                [_arrayController add:sender];
                [self performSelector:@selector(editNewEntry:) withObject:nil afterDelay:0.1];
            } else NSBeep();
            break;
        case 1:
            if (!_arrayController.selectedTheme.editable) {
                NSBeep();
                break;
            }
            row = (NSInteger)[_arrayController selectionIndex] - 1;
            [_arrayController remove:sender];
            _arrayController.selectionIndex = (NSUInteger)row;
            break;
        case 2:
            //Should never happen
            break;
        default:
            break;
    }
}

#pragma mark User actions

- (IBAction)changeDefaultSize:(id)sender {
    if (sender == txtCols) {
        if (defscreenw == [sender intValue])
            return;
        [self cloneThemeIfNotEditable];
        defscreenw = [sender intValue];
        if (defscreenw < 5)
            defscreenw = 5;
        if (defscreenw > 200)
            defscreenw = 200;
        txtCols.intValue = defscreenw;
        theme.defaultCols = defscreenw;
    }
    if (sender == txtRows) {
        if (defscreenh == [sender intValue])
            return;
        [self cloneThemeIfNotEditable];
        defscreenh = [sender intValue];
        if (defscreenh < 5)
            defscreenh = 5;
        if (defscreenh > 200)
            defscreenh = 200;
        txtRows.intValue = defscreenh;
        theme.defaultRows = defscreenh;

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
        if ([theme.gridBackground isEqual:color])
            return;
        [self cloneThemeIfNotEditable];
        theme.gridBackground = color;
    } else if (sender == clrBufferFg) {
        key = @"bufferNormal";
    } else if (sender == clrBufferBg) {
        if ([theme.bufferBackground isEqual:color])
            return;
        [self cloneThemeIfNotEditable];
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

        if ([style.color isEqual:color])
            return;

        [self cloneThemeIfNotEditable];
        style.color = color;

    }

    [Preferences rebuildTextAttributes];

    if (sender == clrBufferFg) {
        NSLog(@"User changed text color to %@", theme.bufferNormal.color);
        for (NSString *str in gBufferStyleNames)
            NSLog(@"Text color of %@ is now %@", str, [[theme valueForKey:str] color]);
    }
}

- (IBAction)changeMargin:(id)sender  {
    NSString *key = nil;
    NSInteger val = 0;

    val = [sender intValue];

    if (sender == txtGridMargin) {
        if (theme.gridMarginX == val)
            return;
        [self cloneThemeIfNotEditable];
        key = @"GridMargin";
        theme.gridMarginX = val;
    }
    if (sender == txtBufferMargin) {
        if (theme.bufferMarginX == val)
            return;
        [self cloneThemeIfNotEditable];
        key = @"BufferMargin";
        theme.bufferMarginX = val;
    }

    if (key) {
        [Preferences rebuildTextAttributes];
    }
}

- (IBAction)changeLeading:(id)sender {
    if (theme.bufferNormal.lineSpacing == [sender floatValue])
        return;
    [self cloneThemeIfNotEditable];
    theme.bufferNormal.lineSpacing = [sender floatValue];
    [Preferences rebuildTextAttributes];
}

- (IBAction)changeSmartQuotes:(id)sender {
    if (theme.smartQuotes  == [sender state])
        return;
    [self cloneThemeIfNotEditable];
    theme.smartQuotes = [sender state];
    NSLog(@"pref: smart quotes changed to %d", smartquotes);
}

- (IBAction)changeSpaceFormatting:(id)sender {
    if (theme.spaceFormat  == [sender state])
        return;
    [self cloneThemeIfNotEditable];
    theme.spaceFormat = [sender state];
    NSLog(@"pref: space format changed to %d", theme.spaceFormat);
}

- (IBAction)changeEnableGraphics:(id)sender {
    if (theme.doGraphics  == [sender state])
        return;
    [self cloneThemeIfNotEditable];
    theme.doGraphics = [sender state];
    NSLog(@"pref: dographics changed to %d", theme.doGraphics);

    /* send notification that prefs have changed -- tell clients that graphics
     * are off limits */
    [[NSNotificationCenter defaultCenter]
     postNotificationName:@"PreferencesChanged"
     object:[Preferences currentTheme]];

    NSLog(@"Preferences changeEnableGraphics issued PreferencesChanged notification with object %@", [Preferences currentTheme].name);
}

- (IBAction)changeEnableSound:(id)sender {
    if (theme.doSound  == [sender state])
        return;
    [self cloneThemeIfNotEditable];
    theme.doSound = [sender state];
    NSLog(@"pref: dosound changed to %d", theme.doSound);

    /* send notification that prefs have changed -- tell clients that sound is
     * off limits */
    [[NSNotificationCenter defaultCenter]
     postNotificationName:@"PreferencesChanged"
     object:[Preferences currentTheme]];

    NSLog(@"Preferences changeEnableGraphics issued PreferencesChanged notification with object %@", [Preferences currentTheme].name);
}

- (IBAction)changeEnableStyles:(id)sender {
    if (theme.doStyles  == [sender state])
        return;
    [self cloneThemeIfNotEditable];
    theme.doStyles = [sender state];
    NSLog(@"pref: dostyles changed to %d", theme.doStyles);
    [Preferences rebuildTextAttributes];
}


- (IBAction)changeBorderSize:(id)sender {
    if (theme.border == [sender intValue])
        return;
    [self cloneThemeIfNotEditable];
    theme.border = [sender intValue];

    /* send notification that prefs have changed -- tell clients that border has
     * changed */
    [[NSNotificationCenter defaultCenter]
     postNotificationName:@"PreferencesChanged"
     object:[Preferences currentTheme]];

    NSLog(@"Preferences changeBorderSize issued PreferencesChanged notification with object %@", [Preferences currentTheme].name);
}

- (void)cloneThemeIfNotEditable {
    if (!theme.editable) {
        disregardTableSelection = YES;
        Theme *clonedTheme = theme.clone;
        clonedTheme.editable = YES;
        NSString *name = [theme.name stringByAppendingString:@" (modified)"];
        NSUInteger counter = 2;
        while ([_arrayController findThemeByName:name]) {
            name = [NSString stringWithFormat:@"%@ (modified) %ld", theme.name, counter++];
        }
        clonedTheme.name = name;
        theme = clonedTheme;
        [self changeThemeName:name];
        [_addAndRemove setEnabled:YES forSegment:1];
        [self performSelector:@selector(restoreThemeSelection:) withObject:theme afterDelay:0.1];
    }
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
    if (selfontp) {
        *selfontp = [fontManager convertFont:*selfontp];
    } else {
        NSLog(@"Error! Preferences changeFont called with no font selected");
        return;
    }

    if (selfontp == &gridroman) {
        if ([theme.gridNormal.font isEqual:gridroman])
            return;
        [self cloneThemeIfNotEditable];
        theme.gridNormal.font = gridroman;
        btnGridFont.title = fontToString(gridroman);
    } else if (selfontp == &bufroman) {
        if ([theme.bufferNormal.font isEqual:bufroman])
            return;
        [self cloneThemeIfNotEditable];
        theme.bufferNormal.font = bufroman;
        btnBufferFont.title = fontToString(bufroman);
    } else if (selfontp == &inputfont) {
        if ([theme.bufInput.font isEqual:inputfont])
            return;
        [self cloneThemeIfNotEditable];
        theme.bufInput.font = inputfont;
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
    if ([[NSColorPanel sharedColorPanel] isVisible])
        [[NSColorPanel sharedColorPanel] orderOut:self];
}

@end
