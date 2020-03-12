#import "Compatibility.h"
#import "CoreDataManager.h"
#import "Game.h"
#import "Metadata.h"
#import "Theme.h"
#import "ThemeArrayController.h"
#import "GlkStyle.h"
#import "LibController.h"
#import "NSString+Categories.h"

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

static kZoomDirectionType zoomDirection = ZOOMRESET;

//static NSColor *fgcolor[8];
//static NSColor *bgcolor[8];

static Theme *theme = nil;
static Preferences *prefs = nil;

/*
 * Some color utility functions
 */

//
//static NSColor *makehsb(CGFloat h, CGFloat s, CGFloat b) {
//    return [NSColor colorWithCalibratedHue:h
//                                saturation:s
//                                brightness:b
//                                     alpha:1.0];
//}

/*
 * Load and save defaults
 */

+ (void)initFactoryDefaults {
    NSString *filename = [[NSBundle mainBundle] pathForResource:@"Defaults"
                                                         ofType:@"plist"];
    NSMutableDictionary *defaults =
        [NSMutableDictionary dictionaryWithContentsOfFile:filename];

    defaults[@"GameDirectory"] = (@"~/Documents").stringByExpandingTildeInPath;
    defaults[@"SaveDirectory"] = (@"~/Documents").stringByExpandingTildeInPath;

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
    } else theme = fetchedObjects[0];

//    [self readSettingsFromTheme:theme];
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
        return fetchedObjects[0];
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

//+ (void)readSettingsFromTheme:(Theme *)theme {
//
//    if (!theme.gridNormal.font) {
//        NSLog(@"pref: Found no grid NSFont object in theme %@, creating default", theme.name);
//        theme.gridNormal.font = [NSFont userFixedPitchFontOfSize:0];
//    }
//
//    if (!theme.bufferNormal.font) {
//        NSLog(@"pref: Found no buffer NSFont object in theme %@, creating default", theme.name);
//        theme.bufferNormal.font = [NSFont userFontOfSize:0];
//    }
//
//    if (!theme.bufInput.font) {
//        NSLog(@"pref: Found no bufInput NSFont object in theme %@, creating default", theme.name);
//        theme.bufInput.font = [NSFont userFontOfSize:0];
//    }
//}

+ (void)changeCurrentGame:(Game *)game {
    if (prefs) {
        prefs.currentGame = game;
        [prefs restoreThemeSelection:theme];
    }
}

+ (void)initialize {

    [self initFactoryDefaults];
    [self readDefaults];

//    /* 0=black, 1=red, 2=green, 3=yellow, 4=blue, 5=magenta, 6=cyan, 7=white */
//
//    /* black */
//    bgcolor[0] = makehsb(0, 0, 0.2);
//    fgcolor[0] = makehsb(0, 0, 0.0);
//
//    /* white */
//    bgcolor[7] = makehsb(0, 0, 1.0);
//    fgcolor[7] = makehsb(0, 0, 0.8);
//
//    /* hues go from red, orange, yellow, green, cyan, blue, magenta, red */
//    /* foreground: 70% sat 30% bright */
//    /* background: 60% sat 90% bright */
//
//    bgcolor[1] = makehsb(0 / 360.0, 0.8, 0.4);   /* red */
//    bgcolor[2] = makehsb(120 / 360.0, 0.8, 0.4); /* green */
//    bgcolor[3] = makehsb(60 / 360.0, 0.8, 0.4);  /* yellow */
//    bgcolor[4] = makehsb(230 / 360.0, 0.8, 0.4); /* blue */
//    bgcolor[5] = makehsb(300 / 360.0, 0.8, 0.4); /* magenta */
//    bgcolor[6] = makehsb(180 / 360.0, 0.8, 0.4); /* cyan */
//
//    fgcolor[1] = makehsb(0 / 360.0, 0.8, 0.8);   /* red */
//    fgcolor[2] = makehsb(120 / 360.0, 0.8, 0.8); /* green */
//    fgcolor[3] = makehsb(60 / 360.0, 0.8, 0.8);  /* yellow */
//    fgcolor[4] = makehsb(230 / 360.0, 0.8, 0.8); /* blue */
//    fgcolor[5] = makehsb(300 / 360.0, 0.8, 0.8); /* magenta */
//    fgcolor[6] = makehsb(180 / 360.0, 0.8, 0.8); /* cyan */

    [self rebuildTextAttributes];
}


#pragma mark Global accessors

//+ (NSColor *)foregroundColor:(int)number {
//    if (number < 0 || number > 7)
//        return nil;
//    return fgcolor[number];
//}
//
//+ (NSColor *)backgroundColor:(int)number {
//    if (number < 0 || number > 7)
//        return nil;
//    return bgcolor[number];
//}

+ (BOOL)graphicsEnabled {
    return theme.doGraphics;
}

+ (BOOL)soundEnabled {
    return theme.doSound;
}

+ (BOOL)stylesEnabled {
    return theme.doStyles;
}

+ (BOOL)smartQuotes {
    return theme.smartQuotes;
}

+ (kSpacesFormatType)spaceFormat {
    return (kSpacesFormatType)theme.spaceFormat;
}

+ (kZoomDirectionType)zoomDirection {
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

    self.windowFrameAutosaveName = @"PrefsPanel";
    self.window.delegate = self;
    themesTableView.autosaveName = @"ThemesTable";

    if (!theme)
        theme = self.defaultTheme;

    // Sample text view
    glkcntrl = [[GlkController alloc] init];
    glkcntrl.theme = theme;
    glkcntrl.borderView = sampleTextBorderView;
    glkcntrl.contentView = sampleTextView;

    sampleTextBorderView.wantsLayer = YES;
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
    [_divider removeFromSuperview];

    NSRect sampleTextFrame;

    sampleTextFrame.origin = NSMakePoint(theme.border, theme.border);
    sampleTextFrame.size = NSMakeSize(sampleTextView.frame.size.width - theme.border * 2,
                                      sampleTextView.frame.size.height - theme.border * 2);

    glktxtbuf.frame = NSMakeRect(0, 0, sampleTextFrame.size.width,  sampleTextFrame.size.height);

    [glktxtbuf putString:@"Palace Gate\n" style:style_Subheader];
    [glktxtbuf putString:@"A tide of perambulators surges north along the crowded Broad Walk. "
                         @"Shaded glades stretch away to the northeast, "
                         @"and a hint of color marks the western edge of the Flower Walk. " style:style_Normal];

    [glktxtbuf putString:@"(Trinity, Brian Moriarty, Infocom, 1986)" style:style_Emphasized];

    [self fixScrollBar:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(notePreferencesChanged:)
                                                 name:@"PreferencesChanged"
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(noteManagedObjectContextDidChange:)
                                                 name:NSManagedObjectContextObjectsDidChangeNotification
                                               object:_managedObjectContext];

    _oneThemeForAll = [[NSUserDefaults standardUserDefaults] boolForKey:@"oneThemeForAll"];
    _themesHeader.stringValue = [self themeScopeTitle];

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
    [self performSelector:@selector(restoreThemeSelection:) withObject:theme afterDelay:0.1];
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
    btnSpaceFormat.state = (theme.spaceFormat == TAG_SPACES_ONE);

    btnEnableGraphics.state = theme.doGraphics;
    btnEnableSound.state = theme.doSound;
    btnEnableStyles.state = theme.doStyles;

    _btnOneThemeForAll.state = _oneThemeForAll;

    if ([[NSFontPanel sharedFontPanel] isVisible] && selectedFontButton)
        [self showFontPanel:selectedFontButton];
}

@synthesize currentGame = _currentGame;

- (void)setCurrentGame:(Game *)currentGame {
    _currentGame = currentGame;
    _themesHeader.stringValue = [self themeScopeTitle];
    if (currentGame == nil) {
        NSLog(@"Preferences currentGame was set to nil");
        return;
    }
    if (_currentGame.theme != theme) {
        [self restoreThemeSelection:_currentGame.theme];
    }
}

- (Game *)currentGame {
    return _currentGame;
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
            _defaultTheme = fetchedObjects[0];
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

    if (fetchedObjects[0] != self.defaultTheme) {
        NSLog(@"createDefaultThemes: something went wrong");
    } else
        NSLog(@"createDefaultThemes successful");
}

#pragma mark Preview

- (void)notePreferencesChanged:(NSNotification *)notify {
    NSLog(@"notePreferencesChanged:");

    // Change the theme of the sample text field
    glktxtbuf.theme = theme;
    glkcntrl.theme = theme;

    [glktxtbuf prefsDidChange];
    [_coreDataManager saveChanges];

    if (previewHidden)
        return;

    [self resizeWindowToHeight:[self previewHeight]];
}

- (void)resizeWindowToHeight:(CGFloat)height {
    NSWindow *prefsPanel= self.window;
    CGRect screenframe = prefsPanel.screen.visibleFrame;

    CGFloat oldheight = prefsPanel.frame.size.height;

    CGRect winrect = prefsPanel.frame;
    winrect.origin = prefsPanel.frame.origin;

    winrect.size.height = height;

    // If the entire text does not fit on screen, don't change height at all
    if (winrect.size.height > screenframe.size.height)
        winrect.size.height = oldheight;

    // When we reuse the window it will remember our last scroll position,
    // so we reset it here

    NSScrollView *scrollView = glktxtbuf.textview.enclosingScrollView;

    // Scroll the vertical scroller to top
    scrollView.verticalScroller.floatValue = 0;

    // Scroll the contentView to top
    [scrollView.contentView scrollToPoint:NSZeroPoint];

    CGFloat offset = winrect.size.height - oldheight;
    winrect.origin.y -= offset;

    // If window is partly off the screen, move it (just) inside
    if (NSMaxX(winrect) > NSMaxX(screenframe))
        winrect.origin.x = NSMaxX(screenframe) - winrect.size.width;

    if (NSMinY(winrect) < 0)
        winrect.origin.y = NSMinY(screenframe);

    Preferences * __unsafe_unretained weakSelf = self;

    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
         [[prefsPanel animator]
          setFrame:winrect
          display:YES];
     } completionHandler:^{
         //We need to reset the sampleTextBorderView here, otherwise some of it will still show when hiding the preview.
         NSRect newFrame = weakSelf.window.frame;
         sampleTextBorderView.frame = NSMakeRect(0, 0, newFrame.size.width, newFrame.size.height - kDefaultPrefWindowHeight);

         sampleTextView.frame = NSMakeRect(theme.border, theme.border, sampleTextBorderView.frame.size.width - theme.border * 2, sampleTextBorderView.frame.size.height - theme.border * 2);

         [_divider removeFromSuperview];
         if (!previewHidden) {
             _divider.frame = NSMakeRect(0, sampleTextBorderView.frame.size.height, newFrame.size.width, 1);
             [weakSelf.window.contentView addSubview:_divider];
         }

         [weakSelf performSelector:@selector(fixScrollBar:) withObject:nil afterDelay:0.1];
     }];
}

- (void)fixScrollBar:(id)sender {
    [glktxtbuf restoreScrollBarStyle];
    glktxtbuf.frame = NSMakeRect(0, 0, sampleTextView.frame.size.width, sampleTextView.frame.size.height);
    NSScrollView *scrollView = glktxtbuf.textview.enclosingScrollView;
    scrollView.frame = glktxtbuf.frame;
    [scrollView.contentView scrollToPoint:NSZeroPoint];
}


- (CGFloat)previewHeight {

    CGFloat proposedHeight = [self textHeight];

    CGFloat totalHeight = kDefaultPrefWindowHeight + proposedHeight + 2 * (theme.border + theme.bufferMarginY);
    CGRect screenframe = [NSScreen mainScreen].visibleFrame;

    if (totalHeight > screenframe.size.height) {
        totalHeight = screenframe.size.height;
    }
    return totalHeight;
}

- (CGFloat)textHeight {
    NSTextView *textview = [[NSTextView alloc] initWithFrame:glktxtbuf.textview.frame];
    if (textview == nil) {
        NSLog(@"Couldn't create textview!");
        return 0;
    }

    CGFloat padding = glktxtbuf.textview.textContainer.lineFragmentPadding;

    NSTextStorage *textStorage = [[NSTextStorage alloc] initWithAttributedString:[glktxtbuf.textview.textStorage copy]];
    CGFloat textWidth = textview.frame.size.width - padding + 1;
    NSTextContainer *textContainer = [[NSTextContainer alloc]
                                      initWithContainerSize:NSMakeSize(textWidth, FLT_MAX)];

    NSLayoutManager *layoutManager = [[NSLayoutManager alloc] init];
    [layoutManager addTextContainer:textContainer];
    [textStorage addLayoutManager:layoutManager];

    [layoutManager ensureLayoutForGlyphRange:NSMakeRange(0, textStorage.length)];

    CGRect proposedRect = [layoutManager usedRectForTextContainer:textContainer];
    return proposedRect.size.height;
}

- (void)noteManagedObjectContextDidChange:(NSNotification *)notify {
    NSArray *updatedObjects = (notify.userInfo)[NSUpdatedObjectsKey];

    if ([updatedObjects containsObject:theme]) {
        Preferences * __unsafe_unretained weakSelf = self;

        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf updatePrefsPanel];
            [[NSNotificationCenter defaultCenter]
             postNotification:[NSNotification notificationWithName:@"PreferencesChanged" object:theme]];
        });
    }
}

#pragma mark Themes Table View Magic

- (void)restoreThemeSelection:(id)sender {
    NSArray *themes = _arrayController.arrangedObjects;
    theme = sender;
    if (![themes containsObject:sender]) {
        NSLog(@"selected theme not found!");
        theme = self.defaultTheme;
        return;
    }

    NSUInteger row = [themes indexOfObject:theme];

    disregardTableSelection = NO;

    [_arrayController setSelectionIndex:row];
    themesTableView.allowsEmptySelection = NO;
    [themesTableView scrollRowToVisible:(NSInteger)row];
}

- (void)tableViewSelectionDidChange:(id)notification {
    NSTableView *tableView = [notification object];
    if (tableView == themesTableView) {
        NSLog(@"Preferences tableViewSelectionDidChange:%@", _arrayController.selectedTheme.name);
        if (disregardTableSelection == YES) {
            disregardTableSelection = NO;
            return;
        }
        theme = _arrayController.selectedTheme;
        // Send notification that theme has changed -- trigger configure events

        [self updatePrefsPanel];
        [self changeThemeName:theme.name];
        _btnRemove.enabled = theme.editable;
//        [Preferences readSettingsFromTheme:theme];

        if (_oneThemeForAll) {
            NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
            NSArray *fetchedObjects;
            NSError *error;
            fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:self.managedObjectContext];
            fetchRequest.includesPropertyValues = NO;
            fetchedObjects = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];
            [theme addGames:[NSSet setWithArray:fetchedObjects]];
        } else if (_currentGame) {
            _currentGame.theme = theme;
        }


        [[NSNotificationCenter defaultCenter]
         postNotification:[NSNotification notificationWithName:@"PreferencesChanged" object:theme]];
    }
    return;
}

- (void)changeThemeName:(NSString *)name {
    [[NSUserDefaults standardUserDefaults] setObject:name forKey:@"themeName"];
    _detailsHeader.stringValue = [NSString stringWithFormat:@"Settings for theme %@", name];
//    [_coreDataManager saveChanges];
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
textShouldEndEditing:(NSText *)fieldEditor {
    if ([self notDuplicate:fieldEditor.string] == NO) {
        [self showDuplicateThemeNameAlert:fieldEditor];
        return NO;
    }
    return YES;
}

- (void)showDuplicateThemeNameAlert:(NSText *)fieldEditor {
    NSAlert *anAlert = [[NSAlert alloc] init];
    anAlert.messageText =
    [NSString stringWithFormat:@"The theme name \"%@\" is already in use.", fieldEditor.string];
    anAlert.informativeText = @"Please enter another name.";
    [anAlert addButtonWithTitle:@"Okay"];
    [anAlert addButtonWithTitle:@"Discard Change"];

    [anAlert beginSheetModalForWindow:self.window
                        modalDelegate:self
                       didEndSelector:@selector(duplicateThemeNameAlertDidFinish:
                                                rc:ctx:)
                          contextInfo:(__bridge void *)fieldEditor];
}

- (void)duplicateThemeNameAlertDidFinish:(id)alert rc:(int)result ctx:(void *)ctx {
    if (result == NSAlertSecondButtonReturn) {
        ((__bridge NSText *)ctx).string = theme.name;
    }
}

- (void)controlTextDidEndEditing:(NSNotification *)notification {
    if ([notification.object isKindOfClass:[NSTextField class]]) {
        NSTextField *textfield = notification.object;
        [self changeThemeName:textfield.stringValue];
    }
}

- (NSArray *)sortDescriptors {
    return @[[NSSortDescriptor sortDescriptorWithKey:@"editable" ascending:YES],
             [NSSortDescriptor sortDescriptorWithKey:@"name" ascending:YES
                                            selector:@selector(localizedStandardCompare:)]];
}

#pragma mark -
#pragma mark Windows restoration

- (void)window:(NSWindow *)window willEncodeRestorableState:(NSCoder *)state {
    NSString *selectedfontString = nil;
    if (selectedFontButton)
        selectedfontString = selectedFontButton.identifier;
    [state encodeObject:selectedfontString forKey:@"selectedFont"];
    [state encodeBool:previewHidden forKey:@"previewHidden"];
}

- (void)window:(NSWindow *)window didDecodeRestorableState:(NSCoder *)state {
    NSString *selectedfontString = [state decodeObjectForKey:@"selectedFont"];
    if (selectedfontString != nil) {
        NSArray *fontsButtons = @[btnBufferFont, btnGridFont, btnInputFont];
        for (NSButton *button in fontsButtons) {
            if ([button.identifier isEqualToString:selectedfontString]) {
                selectedFontButton = button;
            }
        }
    }
    
    previewHidden = [state decodeBoolForKey:@"previewHidden"];
    if (previewHidden) {
        [self resizeWindowToHeight:kDefaultPrefWindowHeight];
    } else {
        [self resizeWindowToHeight:[self previewHeight]];
    }

}

- (NSUndoManager *)windowWillReturnUndoManager:(NSWindow *)window {
    //    NSLog(@"libctl: windowWillReturnUndoManager!")
    return _managedObjectContext.undoManager;
}

#pragma mark Action menu

@synthesize oneThemeForAll = _oneThemeForAll;

- (void)setOneThemeForAll:(BOOL)oneThemeForAll {
    _oneThemeForAll = oneThemeForAll;
    [[NSUserDefaults standardUserDefaults] setBool:_oneThemeForAll forKey:@"oneThemeForAll"];
    _themesHeader.stringValue = [self themeScopeTitle];
    if (oneThemeForAll) {
        _btnOneThemeForAll.state = NSOnState;
        NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
        NSError *error = nil;
        fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:self.managedObjectContext];
        fetchRequest.includesPropertyValues = NO;
        NSArray *fetchedObjects = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];
        theme.games = [NSSet setWithArray:fetchedObjects];
    } else {
        _btnOneThemeForAll.state = NSOffState;
    }

}

- (BOOL)oneThemeForAll {
    return _oneThemeForAll;
}

- (IBAction)clickedOneThemeForAll:(id)sender {
    if ([sender state] == 1) {
        if (![[NSUserDefaults standardUserDefaults] valueForKey:@"UseForAllAlertAlertSuppression"]) {
            NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
            NSError *error = nil;
            fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:self.managedObjectContext];
            fetchRequest.includesPropertyValues = NO;
            NSArray *fetchedObjects = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];
            NSUInteger numberOfGames = fetchedObjects.count;
            Theme *mostPopularTheme = nil;
            NSUInteger highestCount = 0;
            NSUInteger currentCount = 0;
            for (Theme *t in _arrayController.arrangedObjects) {
                currentCount = t.games.count;
                if (currentCount > highestCount) {
                    highestCount = t.games.count;
                    mostPopularTheme = t;
                    NSLog(@"Theme %@ has the highest number (%ld) of games using it so far", t.name, highestCount);
                }
            }
            if (highestCount < numberOfGames) {
                fetchRequest.predicate = [NSPredicate predicateWithFormat:@"theme != %@", mostPopularTheme];
                fetchedObjects = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];
                [self showUseForAllAlert:fetchedObjects];
                return;
            }
        }
    }

    self.oneThemeForAll = (BOOL)[sender state];
}

- (void)showUseForAllAlert:(NSArray *)games {
    NSAlert *anAlert = [[NSAlert alloc] init];
    anAlert.messageText =
    [NSString stringWithFormat:@"%@ %@ individual theme settings.", [NSString stringWithSummaryOf:games], (games.count == 1) ? @"has" : @"have"];
    anAlert.informativeText = [NSString stringWithFormat:@"Would you like to use theme %@ for all games?", theme.name];
    anAlert.showsSuppressionButton = YES;
    anAlert.suppressionButton.title = @"Do not show again.";
    [anAlert addButtonWithTitle:@"Okay"];
    [anAlert addButtonWithTitle:@"Cancel"];

    [anAlert beginSheetModalForWindow:self.window
                        modalDelegate:self
                       didEndSelector:@selector(useForAllAlertDidFinish:
                                                rc:ctx:)
                          contextInfo:NULL];
}

- (void)useForAllAlertDidFinish:(id)alert rc:(int)result ctx:(void *)ctx {

    NSAlert *anAlert = alert;
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    NSString *alertSuppressionKey = @"UseForAllAlertAlertSuppression";

    if (anAlert.suppressionButton.state == NSOnState) {
        // Suppress this alert from now on
        [defaults setBool:YES forKey:alertSuppressionKey];
    }

    if (result == NSAlertFirstButtonReturn) {
        self.oneThemeForAll = YES;
    } else {
        _btnOneThemeForAll.state = NSOffState;
    }
}

- (NSString *)themeScopeTitle {
    if (_oneThemeForAll) return @"Theme setting for all games";
    if ( _currentGame == nil)
        return @"No game is currently running";
    else
        return [@"Theme setting for game " stringByAppendingString:_currentGame.metadata.title];
}

- (IBAction)addTheme:(id)sender {
    NSInteger row = (NSInteger)[_arrayController selectionIndex];
    NSTableCellView *cellView = (NSTableCellView*)[themesTableView viewAtColumn:0 row:row makeIfNecessary:YES];
    if ([self notDuplicate:cellView.textField.stringValue]) {
        // For some reason, tableViewSelectionDidChange will be called twice here,
        // so we disregard the first call
        disregardTableSelection = YES;
        [_arrayController add:sender];
        [self performSelector:@selector(editNewEntry:) withObject:nil afterDelay:0.1];
    } else NSBeep();
}

- (IBAction)removeTheme:(id)sender {
    if (!_arrayController.selectedTheme.editable) {
        NSBeep();
        return;
    }
    NSSet *orphanedGames = _arrayController.selectedTheme.games;
    NSInteger row = (NSInteger)[_arrayController selectionIndex] - 1;
    [_arrayController remove:sender];
    _arrayController.selectionIndex = (NSUInteger)row;
    [_arrayController.selectedTheme addGames:orphanedGames];
}

- (IBAction)applyToSelected:(id)sender {
    [theme addGames:[NSSet setWithArray:_libcontroller.selectedGames]];
}

- (IBAction)selectUsingTheme:(id)sender {
    [_libcontroller selectGames:theme.games];
    NSLog(@"selected %ld games using theme %@", theme.games.count, theme.name);
}

- (IBAction)deleteUserThemes:(id)sender {
    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    NSArray *fetchedObjects;
    NSError *error;
    fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:self.managedObjectContext];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"editable == YES"];
    fetchedObjects = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];

    if (fetchedObjects == nil || fetchedObjects.count == 0) {
        return;
    }

    NSMutableSet *orphanedGames = [[NSMutableSet alloc] init];

    for (Theme *t in fetchedObjects) {
        [orphanedGames unionSet:t.games];
    }

    [_arrayController removeObjects:fetchedObjects];

    NSArray *remainingThemes = [_arrayController arrangedObjects];
    Theme *lastTheme = remainingThemes[remainingThemes.count - 1];
    NSLog(@"lastRemainingTheme: %@", lastTheme.name);
    [lastTheme addGames:orphanedGames];
    _arrayController.selectedObjects = @[lastTheme];
}

- (IBAction)togglePreview:(id)sender {
    if (!previewHidden) {
        [self resizeWindowToHeight:kDefaultPrefWindowHeight];
        previewHidden = YES;
    } else {
        previewHidden = NO;
        if (!previewHidden) {
            _divider.frame = NSMakeRect(0, sampleTextBorderView.frame.size.height, self.window.frame.size.width, 1);
            [self.window.contentView addSubview:_divider];
        }
        [self notePreferencesChanged:(NSNotification *)theme];
    }
}

- (IBAction)editNewEntry:(id)sender {
    NSInteger row = (NSInteger)[_arrayController selectionIndex];
    NSTableCellView* cellView = (NSTableCellView*)[themesTableView viewAtColumn:0 row:row makeIfNecessary:YES];
    if ([cellView.textField acceptsFirstResponder]) {
        [cellView.window makeFirstResponder:cellView.textField];
        [themesTableView scrollRowToVisible:(NSInteger)row];
    }
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    SEL action = menuItem.action;

    if (action == @selector(applyToSelected:)) {
        if (_oneThemeForAll || _libcontroller.selectedGames.count == 0) {
            return NO;
        } else {
            return YES;
        }
    }

    if (action == @selector(selectUsingTheme:))
        return (theme.games.count > 0);

    if (action == @selector(deleteUserThemes:)) {
        NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
        NSArray *fetchedObjects;
        NSError *error;
        fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:self.managedObjectContext];
        fetchRequest.predicate = [NSPredicate predicateWithFormat:@"editable == YES"];
        fetchRequest.includesPropertyValues = NO;
        fetchedObjects = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];

        if (fetchedObjects == nil || fetchedObjects.count == 0) {
            return NO;
        }
    }

    if (action == @selector(editNewEntry:))
        return theme.editable;

    if (action == @selector(togglePreview:))
    {
        NSString* title = previewHidden ? @"Show Preview" : @"Hide Preview";
        ((NSMenuItem*)menuItem).title = title;
    }
    
    return YES;
}

#pragma mark User actions

- (IBAction)changeDefaultSize:(id)sender {
    if (sender == txtCols) {
        if (theme.defaultCols == [sender intValue])
            return;
        [self cloneThemeIfNotEditable];
        theme.defaultCols  = [sender intValue];
        if (theme.defaultCols  < 5)
            theme.defaultCols  = 5;
        if (theme.defaultCols  > 200)
            theme.defaultCols  = 200;
        txtCols.intValue = theme.defaultCols ;
    }
    if (sender == txtRows) {
        if (theme.defaultRows == [sender intValue])
            return;
        [self cloneThemeIfNotEditable];
        theme.defaultRows  = [sender intValue];
        if (theme.defaultRows  < 5)
            theme.defaultRows  = 5;
        if (theme.defaultRows  > 200)
            theme.defaultRows  = 200;
        txtRows.intValue = theme.defaultRows ;
    }

    /* send notification that default size has changed -- resize all windows */
    NSNotification *notification = [NSNotification notificationWithName:@"DefaultSizeChanged" object:theme];
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
        theme.gridMarginY = val;
    }
    if (sender == txtBufferMargin) {
        if (theme.bufferMarginX == val)
            return;
        [self cloneThemeIfNotEditable];
        key = @"BufferMargin";
        theme.bufferMarginX = val;
        theme.bufferMarginY = val;
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
    NSLog(@"pref: smart quotes changed to %d", theme.smartQuotes);
}

- (IBAction)changeSpaceFormatting:(id)sender {
    if (theme.spaceFormat == [sender state])
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
    if (theme.doStyles == [sender state])
        return;
    [self cloneThemeIfNotEditable];
    theme.doStyles = [sender state];
    NSLog(@"pref: dostyles for theme %@ changed to %d", theme.name, theme.doStyles);
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
        _btnRemove.enabled = YES;
        [self performSelector:@selector(restoreThemeSelection:) withObject:theme afterDelay:0.1];
    }
}

#pragma mark Zoom

+ (void)zoomIn {
    zoomDirection = ZOOMRESET;
    NSFont *gridroman = theme.gridNormal.font;
    NSLog(@"zoomIn gridroman.pointSize = %f", gridroman.pointSize);

    if (gridroman.pointSize < 100) {
        zoomDirection = ZOOMIN;
        [self scale:(gridroman.pointSize + 1) / gridroman.pointSize];
    }
}

+ (void)zoomOut {
    NSLog(@"zoomOut");
    zoomDirection = ZOOMRESET;
    NSFont *gridroman = theme.gridNormal.font;
    if (gridroman.pointSize > 6) {
        zoomDirection = ZOOMOUT;
        [self scale:(gridroman.pointSize - 1) / gridroman.pointSize];
    }
}

+ (void)zoomToActualSize {
    NSLog(@"zoomToActualSize");
    zoomDirection = ZOOMRESET;
    [self scale:12 / theme.gridNormal.font.pointSize];
}

+ (void)scale:(CGFloat)scalefactor {
    NSLog(@"Preferences scale: %f", scalefactor);

    NSFont *gridroman = theme.gridNormal.font;
    NSFont *bufroman = theme.bufferNormal.font;
    NSFont *inputfont = theme.bufInput.font;


    if (scalefactor < 0)
        scalefactor = fabs(scalefactor);

    if ((scalefactor < 1.01 && scalefactor > 0.99) || scalefactor == 0.0)
//        scalefactor = 1.0;
        return;

    [prefs cloneThemeIfNotEditable];

    CGFloat fontSize;

    fontSize = gridroman.pointSize;
    fontSize *= scalefactor;
    if (fontSize > 0) {
        theme.gridNormal.font = [NSFont fontWithDescriptor:gridroman.fontDescriptor
                                          size:fontSize];
    }

    fontSize = bufroman.pointSize;
    fontSize *= scalefactor;
    if (fontSize > 0) {
        theme.bufferNormal.font = [NSFont fontWithDescriptor:bufroman.fontDescriptor
                                         size:fontSize];
    }

    fontSize = inputfont.pointSize;
    fontSize *= scalefactor;
    if (fontSize > 0) {
        theme.bufInput.font = [NSFont fontWithDescriptor:inputfont.fontDescriptor
                                          size:fontSize];
    }

    [Preferences rebuildTextAttributes];

    /* send notification that default size has changed -- resize all windows */
    [[NSNotificationCenter defaultCenter]
     postNotificationName:@"DefaultSizeChanged"
     object:theme];
}

- (void)updatePanelAfterZoom {
    btnGridFont.title = fontToString(theme.gridNormal.font);
    btnBufferFont.title = fontToString(theme.bufferNormal.font);
    btnInputFont.title = fontToString(theme.bufInput.font);
}

#pragma mark Font panel

- (IBAction)showFontPanel:(id)sender {

    selectedFontButton = sender;
    NSFont *selectedFont = nil;
    NSColor *selectedFontColor = nil;
    NSColor *selectedDocumentColor = nil;


    if (sender == btnGridFont) {
        selectedFont = theme.gridNormal.font;
        selectedFontColor = theme.gridNormal.color;
        selectedDocumentColor = theme.gridBackground;
    }
    if (sender == btnBufferFont) {
        selectedFont = theme.bufferNormal.font;
        selectedFontColor = theme.bufferNormal.color;
        selectedDocumentColor = theme.bufferBackground;
    }
    if (sender == btnInputFont) {
        selectedFont = theme.bufInput.font;
        selectedFontColor = theme.bufInput.color;
        selectedDocumentColor = theme.bufferBackground;
    }

    if (selectedFont) {
        NSDictionary *attr =
        @{@"NSColor" : selectedFontColor, @"NSDocumentBackgroundColor" : selectedDocumentColor};

        [self.window makeFirstResponder:self.window];

        [NSFontManager sharedFontManager].target = self;
        [NSFontPanel sharedFontPanel].delegate = self;
        [[NSFontPanel sharedFontPanel] makeKeyAndOrderFront:self];

        [[NSFontManager sharedFontManager] setSelectedAttributes:attr
                                                      isMultiple:NO];
        [[NSFontManager sharedFontManager] setSelectedFont:selectedFont
                                                isMultiple:NO];
    }
}



- (IBAction)changeFont:(id)fontManager {
    NSFont *newFont = nil;
    if (selectedFontButton) {
        newFont = [fontManager convertFont:[fontManager selectedFont]];
    } else {
        NSLog(@"Error! Preferences changeFont called with no font selected");
        return;
    }

    if (selectedFontButton == btnGridFont) {
        if ([theme.gridNormal.font isEqual:newFont])
            return;
        [self cloneThemeIfNotEditable];
        theme.gridNormal.font = newFont;
        btnGridFont.title = fontToString(newFont);
    } else if (selectedFontButton == btnBufferFont) {
        if ([theme.bufferNormal.font isEqual:newFont])
            return;
        [self cloneThemeIfNotEditable];
        theme.bufferNormal.font = newFont;
        btnBufferFont.title = fontToString(newFont);
    } else if (selectedFontButton == btnInputFont) {
        if ([theme.bufInput.font isEqual:newFont])
            return;
        [self cloneThemeIfNotEditable];
        theme.bufInput.font = newFont;
        btnInputFont.title = fontToString(newFont);
    }

    [Preferences rebuildTextAttributes];
}

// This is sent from the font panel when changing font style there

- (void)changeAttributes:(id)sender {
    NSLog(@"changeAttributes:%@", sender);

    NSDictionary *newAttributes = [sender convertAttributes:@{}];

    NSLog(@"changeAttributes: Keys in newAttributes:");
    for (NSString *key in newAttributes.allKeys) {
        NSLog(@" %@ : %@", key, newAttributes[key]);
    }

    //	"NSForegroundColorAttributeName"	"NSColor"
    //	"NSUnderlineStyleAttributeName"		"NSUnderline"
    //	"NSStrikethroughStyleAttributeName"	"NSStrikethrough"
    //	"NSUnderlineColorAttributeName"		"NSUnderlineColor"
    //	"NSStrikethroughColorAttributeName"	"NSStrikethroughColor"
    //	"NSShadowAttributeName"				"NSShadow"

    if (newAttributes[@"NSColor"]) {
        NSColorWell *colorWell = nil;
        NSFont *currentFont = [NSFontManager sharedFontManager].selectedFont;
        if (currentFont == theme.gridNormal.font)
            colorWell = clrGridFg;
        else if (currentFont == theme.bufferNormal.font)
            colorWell = clrBufferFg;
        else if (currentFont == theme.bufInput.font)
            colorWell = clrInputFg;
        colorWell.color = newAttributes[@"NSColor"];
        [self changeColor:colorWell];
    }
}

// This is sent from the font panel when changing background color there

- (void)changeDocumentBackgroundColor:(id)sender {
//    NSLog(@"changeDocumentBackgroundColor");

    NSColorWell *colorWell = nil;
    NSFont *currentFont = [NSFontManager sharedFontManager].selectedFont;
    if (currentFont == theme.gridNormal.font)
        colorWell = clrGridBg;
    else if (currentFont == theme.bufferNormal.font)
        colorWell = clrBufferBg;
    else if (currentFont == theme.bufInput.font)
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
