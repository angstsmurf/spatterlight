#import "CoreDataManager.h"
#import "Game.h"
#import "Metadata.h"
#import "Theme.h"
#import "ThemeArrayController.h"
#import "GlkStyle.h"
#import "LibController.h"
#import "NSString+Categories.h"
#import "NSColor+integer.h"
#import "BufferTextView.h"
#import "main.h"
#import "BuiltInThemes.h"
#import "ParagraphPopOver.h"


#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
fprintf(stderr, "%s\n",                                                    \
[[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@interface DummyTextView : NSTextView <NSWindowDelegate>

- (void)updateTextWithAttributes:(NSDictionary *)attributes;

@property id sender;

@end

@implementation DummyTextView

- (void)updateTextWithAttributes:(NSDictionary *)attributes {
    NSAttributedString *attStr = [[NSAttributedString alloc] initWithString:NSLocalizedString(@"ABCabc1234fiffiflfflct", nil) attributes:attributes];
    [self.textStorage setAttributedString:attStr];
    self.selectedRange = NSMakeRange(0, self.string.length);
}

// These three are sent from the font panel

- (void)changeFont:(id)fontManager {
//    NSLog(@"DummyTextView: changeFont: %@", fontManager);
    [super changeFont:fontManager];
    _sender = fontManager;
    [[Preferences instance] changeFont:self];
}

- (void)changeAttributes:(id)sender {
//    NSLog(@"DummyTextView: changeAttributes:%@", sender);
    [super changeAttributes:sender];
    _sender = sender;
    [[Preferences instance] changeAttributes:self];
}

- (void)changeDocumentBackgroundColor:(id)sender {
//    NSLog(@"DummyTextView: changeDocumentBackgroundColor:%@", sender);
    [super changeDocumentBackgroundColor:sender];
    _sender = sender;
    [[Preferences instance] changeDocumentBackgroundColor:self];
}

@end

@interface Preferences () <NSWindowDelegate, NSControlTextEditingDelegate> {
    IBOutlet NSButton *btnAnyFont, *btnBufferFont, *btnGridFont;
    IBOutlet NSColorWell *clrAnyFg, *clrBufferFg, *clrGridFg;
    IBOutlet NSColorWell *clrBufferBg, *clrGridBg;
    IBOutlet NSTextField *txtBufferMargin, *txtGridMargin, *txtLeading;
    IBOutlet NSTextField *txtRows, *txtCols;
    IBOutlet NSTextField *txtBorder;
    IBOutlet NSButton *btnSmartQuotes;
    IBOutlet NSButton *btnSpaceFormat;
    IBOutlet NSButton *btnEnableGraphics;
    IBOutlet NSButton *btnEnableSound;
    IBOutlet NSButton *btnEnableStyles;
    IBOutlet NSTableView *themesTableView;
    IBOutlet GlkHelperView *sampleTextView;

    GlkController *glkcntrl;

    NSButton *selectedFontButton;

    BOOL disregardTableSelection;
    BOOL zooming;
    CGFloat previewTextHeight;
    NSString *lastSelectedTheme;

    NSDate *themeDuplicationTimestamp;
    Theme *lastDuplicatedTheme;

    NSDictionary *catalinaSoundsToBigSur;
    NSDictionary *bigSurSoundsToCatalina;
}
@end

@implementation Preferences

/*
 * Preference variables, all unpacked
 */

static kZoomDirectionType zoomDirection = ZOOMRESET;

static Theme *theme = nil;
static Preferences *prefs = nil;

/*
 * Load and save defaults
 */

+ (void)initFactoryDefaults {
    NSString *filename = [[NSBundle mainBundle] pathForResource:@"Defaults"
                                                         ofType:@"plist"];
    NSMutableDictionary *defaults =
    [NSMutableDictionary dictionaryWithContentsOfFile:filename];

    // Find user's Documents directory
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);

    defaults[@"GameDirectory"] = paths.firstObject;
    defaults[@"SaveDirectory"] = paths.firstObject;

    [[NSUserDefaults standardUserDefaults] registerDefaults:defaults];
}

+ (void)readDefaults {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *name = [defaults objectForKey:@"themeName"];

    if (!name)
        name = @"Old settings";

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
        theme = [BuiltInThemes createThemeFromDefaultsPlistInContext:managedObjectContext forceRebuild:NO];
        if (!theme)
            theme = [BuiltInThemes createDefaultThemeInContext:managedObjectContext forceRebuild:NO];
        if (!theme) {
            NSLog(@"Preference readDefaults: Error! Could not create default theme!");
        }
    } else theme = fetchedObjects[0];

    // We may or may not have created the Default and Old themes already above.
    // Then these won't be recreated below.
    [BuiltInThemes createBuiltInThemesInContext:managedObjectContext forceRebuild:NO];
}



+ (void)changeCurrentGame:(Game *)game {
    if (prefs) {
        prefs.currentGame = game;
        if (!game.theme)
            game.theme = theme;
        [prefs restoreThemeSelection:theme];
    }
}

+ (void)initialize {

    [self initFactoryDefaults];
    [self readDefaults];

    [self rebuildTextAttributes];
}


#pragma mark Global accessors

+ (kZoomDirectionType)zoomDirection {
    return zoomDirection;
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
    cellsize = [theme.bufferNormal cellSize];
    theme.bufferCellWidth = cellsize.width;
    theme.bufferCellHeight = cellsize.height;
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

    self.window.delegate = self;

    self.windowFrameAutosaveName = @"PrefsPanel";
    themesTableView.autosaveName = @"ThemesTable";

    disregardTableSelection = YES;

    if (self.window.minSize.height != kDefaultPrefWindowHeight || self.window.minSize.width != kDefaultPrefWindowWidth) {
        NSSize minSize = self.window.minSize;
        minSize.height = kDefaultPrefWindowHeight;
        minSize.width = kDefaultPrefWindowWidth;
        self.window.minSize = minSize;
    }

    _previewShown = [[NSUserDefaults standardUserDefaults] boolForKey:@"ShowThemePreview"];

    _standardZArrowsMenuItem.title = NSLocalizedString(@"↑ and ↓ work as in original", nil);
    _standardZArrowsMenuItem.toolTip = NSLocalizedString(@"↑ and ↓ navigate menus and status windows. \u2318↑ and \u2318↓ step through command history.", nil);
    _compromiseZArrowsMenuItem.title = NSLocalizedString(@"Replaced by \u2318↑ and \u2318↓", nil);
    _compromiseZArrowsMenuItem.toolTip = NSLocalizedString(@"\u2318↑ and \u2318↓ are used where the original uses ↑ and ↓. ↑ and ↓ step through command history as in other games.", nil);
    _strictZArrowsMenuItem.title = NSLocalizedString(@"↑↓ and ←→ work as in original", nil);
    _strictZArrowsMenuItem.toolTip = NSLocalizedString(@"↑ and ↓ navigate menus and status windows. \u2318↑ and \u2318↓ step through command history. ← and → don't do anything.", nil);

    if (@available(macOS 11, *)) {

        catalinaSoundsToBigSur = @{ @"Purr" : @"Pluck",
                                    @"Tink" : @"Boop",
                                    @"Blow" : @"Breeze",
                                    @"Pop" : @"Bubble",
                                    @"Glass" : @"Crystal",
                                    @"Funk" : @"Funky",
                                    @"Hero" : @"Heroine",
                                    @"Frog" : @"Jump",
                                    @"Basso" : @"Mezzo",
                                    @"Bottle" : @"Pebble",
                                    @"Morse" : @"Pong",
                                    @"Ping" : @"Sonar",
                                    @"Sosumi" : @"Sonumi",
                                    @"Submarine" : @"Submerge" };

        bigSurSoundsToCatalina = @{ @"Pluck" : @"Purr",
                                    @"Boop" : @"Tink",
                                    @"Breeze" : @"Blow",
                                    @"Bubble" : @"Pop",
                                    @"Crystal": @"Glass",
                                    @"Funky" : @"Funk",
                                    @"Heroine" : @"Hero",
                                    @"Jump" : @"Frog",
                                    @"Mezzo" : @"Basso",
                                    @"Pebble" : @"Bottle",
                                    @"Pong" : @"Morse",
                                    @"Sonar" : @"Ping",
                                    @"Sonumi" : @"Sosumi",
                                    @"Submerge" : @"Submarine" };

        for (NSString *key in catalinaSoundsToBigSur.allKeys) {
            NSMenuItem *menuItem = [_beepHighMenu itemWithTitle:key];
            menuItem.title = catalinaSoundsToBigSur[key];
            menuItem = [_beepLowMenu itemWithTitle:key];
            menuItem.title = catalinaSoundsToBigSur[key];
        }
    }



    if (!theme)
        theme = self.defaultTheme;

    // Sample text view
    glkcntrl = [[GlkController alloc] init];
    glkcntrl.theme = theme;
    glkcntrl.previewDummy = YES;
    glkcntrl.borderView = _sampleTextBorderView;
    glkcntrl.contentView = sampleTextView;
    glkcntrl.ignoreResizes = YES;
    sampleTextView.glkctrl = glkcntrl;

    _sampleTextBorderView.fillColor = theme.bufferBackground;
    NSRect newSampleFrame = NSMakeRect(20, 312, self.window.frame.size.width - 40, ((NSView *)self.window.contentView).frame.size.height - 312);
    sampleTextView.frame = newSampleFrame;
    _sampleTextBorderView.frame = newSampleFrame;

    _divider.frame = NSMakeRect(0, 311, self.window.frame.size.width, 1);
    _divider.autoresizingMask = NSViewMaxYMargin;

    NSMutableArray *nullarray = [NSMutableArray arrayWithCapacity:stylehint_NUMHINTS];

    NSInteger i;
    for (i = 0 ; i < stylehint_NUMHINTS ; i ++)
        [nullarray addObject:[NSNull null]];
    NSMutableArray *stylehHints = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
    for (i = 0 ; i < style_NUMSTYLES ; i ++) {
        [stylehHints addObject:[nullarray mutableCopy]];
    }

    glkcntrl.bufferStyleHints = stylehHints;

    _glktxtbuf = [[GlkTextBufferWindow alloc] initWithGlkController:glkcntrl name:1];

    _glktxtbuf.textview.editable = NO;
    [sampleTextView addSubview:_glktxtbuf];

    [_glktxtbuf putString:@"Palace Gate" style:style_Subheader];
    [_glktxtbuf putString:@" A tide of perambulators surges north along the crowded Broad Walk. "
                   style:style_Normal];
    [_glktxtbuf putString:@"(Trinity, Brian Moriarty, Infocom 1986)" style:style_Emphasized];

    previewTextHeight = [self textHeight];
    [self adjustPreview:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(notePreferencesChanged:)
                                                 name:@"PreferencesChanged"
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(noteManagedObjectContextDidChange:)
                                                 name:NSManagedObjectContextObjectsDidChangeNotification
                                               object:_managedObjectContext];

    _oneThemeForAll = [[NSUserDefaults standardUserDefaults] boolForKey:@"OneThemeForAll"];
    _themesHeader.stringValue = [self themeScopeTitle];

    _adjustSize = [[NSUserDefaults standardUserDefaults] boolForKey:@"AdjustSize"];

    prefs = self;
    [self updatePrefsPanel];

    _scrollView.scrollerStyle = NSScrollerStyleOverlay;
    _scrollView.drawsBackground = YES;
    _scrollView.hasHorizontalScroller = NO;
    _scrollView.hasVerticalScroller = YES;
    _scrollView.verticalScroller.alphaValue = 100;
    _scrollView.autohidesScrollers = YES;
    _scrollView.borderType = NSNoBorder;

    themeDuplicationTimestamp = [NSDate date];

    [self changeThemeName:theme.name];
    [self performSelector:@selector(restoreThemeSelection:) withObject:theme afterDelay:0.1];

    // If the application state was saved on an old version of Spatterlight, the preferences window
    // will be restored too narrow, so we fix it here. We need a delay in order to wait for system
    // windows restoration to finish.
    [self performSelector:@selector(restoreWindowSize:) withObject:theme afterDelay:0.1];
}

- (void)restoreWindowSize:(id)sender  {
    if (NSWidth(self.window.frame) != kDefaultPrefWindowWidth || !_previewShown) {
        _previewShown = NO;
        [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"ShowThemePreview"];
        [self resizeWindowToHeight:kDefaultPrefWindowHeight];
        sampleTextView.autoresizingMask = NSViewHeightSizable;
    }
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

    txtGridMargin.floatValue = theme.gridMarginX;
    txtBufferMargin.floatValue = theme.bufferMarginX;
    txtLeading.doubleValue = theme.bufferNormal.lineSpacing;

    txtCols.intValue = theme.defaultCols;
    txtRows.intValue = theme.defaultRows;

    txtBorder.intValue = theme.border;

    btnGridFont.title = fontToString(theme.gridNormal.font);
    btnBufferFont.title = fontToString(theme.bufferNormal.font);

    btnSmartQuotes.state = theme.smartQuotes;
    btnSpaceFormat.state = (theme.spaceFormat == TAG_SPACES_ONE);

    btnEnableGraphics.state = theme.doGraphics;
    btnEnableSound.state = theme.doSound;
    btnEnableStyles.state = theme.doStyles;

    _btnOverwriteStyles.enabled = theme.hasCustomStyles;
    _btnOverwriteStyles.state = ([_btnOverwriteStyles isEnabled] == NO);

    _btnOneThemeForAll.state = _oneThemeForAll;
    _btnAdjustSize.state = _adjustSize;

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [_windowTypePopup selectItemWithTag:
     [defaults integerForKey:@"SelectedGlkWindowType"]];
    [_styleNamePopup selectItemWithTag:
     [defaults integerForKey:@"SelectedStyle"]];

    GlkStyle *selectedStyle = [self selectedStyle];
    clrAnyFg.color = selectedStyle.color;
    btnAnyFont.title = fontToString(selectedStyle.font);

    _btnVOSpeakCommands.state = theme.vOSpeakCommand;
    [_vOMenuButton selectItemAtIndex:theme.vOSpeakMenu];
    [_vOImagesButton selectItemAtIndex:theme.vOSpeakImages];

    NSString *beepHigh = theme.beepHigh;
    NSString *beepLow = theme.beepLow;

    if (@available(macOS 11, *)) {
        NSString *newHigh = catalinaSoundsToBigSur[beepHigh];
        if (newHigh)
            beepHigh = newHigh;
        NSString *newLow = catalinaSoundsToBigSur[beepLow];
        if (newLow)
            beepLow = newLow;
    }

    [_beepHighMenu selectItemWithTitle:beepHigh];
    [_beepLowMenu selectItemWithTitle:beepLow];
    [_zterpMenu selectItemAtIndex:theme.zMachineTerp];
    [_bZArrowsMenu selectItemAtIndex:theme.bZTerminator];

    _zVersionTextField.stringValue = theme.zMachineLetter;

    _bZVerticalTextField.integerValue = theme.bZAdjustment;
    _bZVerticalStepper.integerValue = theme.bZAdjustment;

    _btnSmoothScroll.state = theme.smoothScroll;
    _btnAutosave.state = theme.autosave;
    _btnAutosaveOnTimer.state = theme.autosaveOnTimer;
    _btnAutosaveOnTimer.enabled = _btnAutosave.state ? YES : NO;
    [_errorHandlingPopup selectItemAtIndex:theme.errorHandling];

    _btnDeterminism.state = theme.determinism;
    _btnNoHacks.state = theme.nohacks ? NSOffState : NSOnState;

    [_imageReplacePopup selectItemWithTag:[defaults integerForKey:@"ImageReplacement"]];

    if (theme.minTimer != 0) {
        if (_timerSlider.integerValue != 1000.0 / theme.minTimer) {
            _timerSlider.integerValue = (long)(1000.0 / theme.minTimer);
        }
        if (_timerTextField.integerValue != (1000.0 / theme.minTimer)) {
            _timerTextField.integerValue = (long)(1000.0 / theme.minTimer);
        }
    }

    if ([[NSFontPanel sharedFontPanel] isVisible]) {
        if (!selectedFontButton)
            selectedFontButton = btnBufferFont;
        [self showFontPanel:selectedFontButton];
    }
}

- (GlkStyle *)selectedStyle {
    NSString *styleName = [self selectedStyleName];
    return [theme valueForKey:styleName];
}

- (NSString *)selectedStyleName {
    NSUInteger windowType = (NSUInteger)_windowTypePopup.selectedTag;
    NSUInteger styleValue = (NSUInteger)_styleNamePopup.selectedTag;
    return (windowType == wintype_TextGrid) ? gGridStyleNames[styleValue] : gBufferStyleNames[styleValue];
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
            _defaultTheme = [BuiltInThemes createDefaultThemeInContext:_managedObjectContext forceRebuild:NO];
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

- (IBAction)rebuildDefaultThemes:(id)sender {
    [BuiltInThemes createBuiltInThemesInContext:_managedObjectContext forceRebuild:YES];
}

#pragma mark Preview

- (void)notePreferencesChanged:(NSNotification *)notify {
    // Change the theme of the sample text field
    _glktxtbuf.theme = theme;
    glkcntrl.theme = theme;

    previewTextHeight = [self textHeight];

    _sampleTextBorderView.fillColor = theme.bufferBackground;

    [_glktxtbuf prefsDidChange];

    Preferences * __unsafe_unretained weakSelf = self;

    dispatch_async(dispatch_get_main_queue(), ^{
        [weakSelf.coreDataManager saveChanges];
    });

    if (!_previewShown)
        return;

    if (sampleTextView.frame.size.height < _sampleTextBorderView.frame.size.height) {
        [self adjustPreview:nil];
    }
    [self performSelector:@selector(adjustPreview:) withObject:nil afterDelay:0.1];
}

- (void)adjustPreview:(id)sender {
    NSRect previewFrame = [self.window.contentView frame];
    previewFrame.origin.y = kDefaultPrefsLowerViewHeight + 1; // Plus one to allow for divider line
    previewFrame.size.height = previewFrame.size.height - kDefaultPrefsLowerViewHeight - 1;
    _sampleTextBorderView.frame = previewFrame;

    previewTextHeight = [self textHeight];
    NSRect newSampleFrame = _sampleTextBorderView.bounds;

    newSampleFrame.origin = NSMakePoint(
                                        round((NSWidth([_sampleTextBorderView bounds]) - NSWidth([sampleTextView frame])) / 2),
                                        round((NSHeight([_sampleTextBorderView bounds]) - previewTextHeight) / 2)
                                        );
    if (newSampleFrame.origin.x < 0)
        newSampleFrame.origin.x = 0;
    if (newSampleFrame.origin.y < 0)
        newSampleFrame.origin.y = 0;

    newSampleFrame.size.width = _sampleTextBorderView.frame.size.width - 40;
    newSampleFrame.size.height = previewTextHeight;

    sampleTextView.autoresizingMask = NSViewMinYMargin | NSViewMaxYMargin | NSViewWidthSizable;

    if (newSampleFrame.size.height > _sampleTextBorderView.bounds.size.height) {
        newSampleFrame.size.height = _sampleTextBorderView.bounds.size.height;
    }

    NSTextView *textview = _glktxtbuf.textview;
    textview.textContainerInset = NSZeroSize;

    if (sampleTextView.frame.size.height < _glktxtbuf.textview.frame.size.height && _glktxtbuf.frame.size.height < _glktxtbuf.textview.frame.size.height && _glktxtbuf.textview.frame.size.height < _sampleTextBorderView.frame.size.height) {
        newSampleFrame.size.height = textview.frame.size.height;
    }

    sampleTextView.frame = newSampleFrame;
    _glktxtbuf.textview.enclosingScrollView.frame = sampleTextView.bounds;
    _glktxtbuf.frame = sampleTextView.bounds;

    _glktxtbuf.autoresizingMask = NSViewHeightSizable;
    _glktxtbuf.textview.enclosingScrollView.autoresizingMask = NSViewHeightSizable;
    [self scrollToTop:nil];
}

- (NSSize)windowWillResize:(NSWindow *)window
                    toSize:(NSSize)frameSize {

    if (window != self.window)
        return frameSize;

    if (frameSize.height > self.window.frame.size.height) { // We are enlarging
        NSRect previewFrame = _sampleTextBorderView.frame;
        previewFrame.origin.y = kDefaultPrefsLowerViewHeight + 1;
        _sampleTextBorderView.frame = previewFrame;
        if (sampleTextView.frame.size.height >= _sampleTextBorderView.frame.size.height) { // Preview fills superview
            if (sampleTextView.frame.size.height >= previewTextHeight) {
                sampleTextView.autoresizingMask = NSViewMinYMargin | NSViewMaxYMargin;
            } else sampleTextView.autoresizingMask = NSViewHeightSizable;
        } else {
            NSRect newFrame = sampleTextView.frame;

            [sampleTextView removeFromSuperview];

            if (sampleTextView.frame.size.height < _glktxtbuf.textview.frame.size.height && _glktxtbuf.frame.size.height < _glktxtbuf.textview.frame.size.height) {
                newFrame.size.height = _glktxtbuf.textview.frame.size.height;
                sampleTextView.frame = newFrame;
                _glktxtbuf.frame = sampleTextView.bounds;
                _glktxtbuf.textview.enclosingScrollView.frame = sampleTextView.bounds;
            }
            newFrame.origin.y = round((_sampleTextBorderView.bounds.size.height - newFrame.size.height) / 2);
            sampleTextView.frame = newFrame;

            [_sampleTextBorderView addSubview:sampleTextView];
        }
    }

    if (zooming) {
        zooming = NO;
        return frameSize;
    }

    if (frameSize.height <= kDefaultPrefWindowHeight) {
        _previewShown = NO;
    } else _previewShown = YES;

    [[NSUserDefaults standardUserDefaults] setBool:_previewShown forKey:@"ShowThemePreview"];

    return frameSize;
}

- (NSRect)windowWillUseStandardFrame:(NSWindow *)window
                        defaultFrame:(NSRect)newFrame {

    if (window != self.window)
        return newFrame;

    CGFloat newHeight;

    if (!_previewShown) {
        newHeight = kDefaultPrefWindowHeight;
        zooming = YES;
    } else {
        newHeight = [self previewHeight];
    }

    NSRect currentFrame = window.frame;

    CGFloat diff = currentFrame.size.height - newHeight;
    currentFrame.origin.y += diff;
    currentFrame.size.height = newHeight;

    return currentFrame;
};

- (BOOL)windowShouldZoom:(NSWindow *)window toFrame:(NSRect)newFrame {
    if (window != self.window)
        return YES;
    if (!_previewShown && newFrame.size.height > kDefaultPrefWindowHeight)
        return NO;
    if (_previewShown)
        [self performSelector:@selector(adjustPreview:) withObject:nil afterDelay:0.1];
    return YES;
}

- (void)resizeWindowToHeight:(CGFloat)height {
    NSWindow *prefsPanel = self.window;

    CGFloat oldheight = prefsPanel.frame.size.height;

    if (ceil(height) == ceil(oldheight)) {
        if (_previewShown) {
            [self performSelector:@selector(scrollToTop:) withObject:nil afterDelay:0.1];
        }
        return;
    }

    CGRect screenframe = prefsPanel.screen.visibleFrame;

    CGRect winrect = prefsPanel.frame;
    winrect.origin = prefsPanel.frame.origin;

    winrect.size.height = height;
    winrect.size.width = kDefaultPrefWindowWidth;

    // If the entire text does not fit on screen, don't change height at all
    if (winrect.size.height > screenframe.size.height)
        winrect.size.height = oldheight;

    // When we reuse the window it will remember our last scroll position,
    // so we reset it here

    NSScrollView *scrollView = _glktxtbuf.textview.enclosingScrollView;

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
    [self adjustPreview:nil];

    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
         [[prefsPanel animator]
          setFrame:winrect
          display:YES];
     } completionHandler:^{
         //We need to reset the _sampleTextBorderView here, otherwise some of it will still show when hiding the preview.
         NSRect newFrame = weakSelf.window.frame;
         weakSelf.sampleTextBorderView.frame = NSMakeRect(0, kDefaultPrefWindowHeight, newFrame.size.width, newFrame.size.height - kDefaultPrefWindowHeight);

         if (weakSelf.previewShown) {
             [weakSelf adjustPreview:nil];
             [weakSelf.glktxtbuf restoreScrollBarStyle];
         }
     }];
}

- (void)scrollToTop:(id)sender {
    if (_previewShown) {
        NSScrollView *scrollView = _glktxtbuf.textview.enclosingScrollView;
        scrollView.frame = _glktxtbuf.frame;
        [scrollView.contentView scrollToPoint:NSZeroPoint];
    }
}

- (CGFloat)previewHeight {

    CGFloat proposedHeight = [self textHeight];

    CGFloat totalHeight = kDefaultPrefWindowHeight + proposedHeight + 40; //2 * (theme.border + theme.bufferMarginY);
    CGRect screenframe = [NSScreen mainScreen].visibleFrame;

    if (totalHeight > screenframe.size.height) {
        totalHeight = screenframe.size.height;
    }
    return totalHeight;
}

- (CGFloat)textHeight {
    [_glktxtbuf flushDisplay];
    NSTextView *textview = [[NSTextView alloc] initWithFrame:_glktxtbuf.textview.frame];
    if (textview == nil) {
        NSLog(@"Couldn't create textview!");
        return 0;
    }

    NSTextStorage *textStorage = [[NSTextStorage alloc] initWithAttributedString:[_glktxtbuf.textview.textStorage copy]];
    CGFloat textWidth = textview.frame.size.width;
    NSTextContainer *textContainer = [[NSTextContainer alloc]
                                      initWithContainerSize:NSMakeSize(textWidth, FLT_MAX)];

    NSLayoutManager *layoutManager = [[NSLayoutManager alloc] init];
    [layoutManager addTextContainer:textContainer];
    [textStorage addLayoutManager:layoutManager];

    [layoutManager ensureLayoutForGlyphRange:NSMakeRange(0, textStorage.length)];

    CGRect proposedRect = [layoutManager usedRectForTextContainer:textContainer];
    return ceil(proposedRect.size.height);
}

- (void)noteManagedObjectContextDidChange:(NSNotification *)notify {
//    NSLog(@"noteManagedObjectContextDidChange: %@", theme.name);
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
    if (_arrayController.selectedTheme == sender) {
//        NSLog(@"restoreThemeSelection: selected theme already was %@. Returning", ((Theme *)sender).name);
        return;
    }
    NSArray *themes = _arrayController.arrangedObjects;
    theme = sender;
    if (![themes containsObject:sender]) {
        theme = themes.lastObject;
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
//        NSLog(@"Preferences tableViewSelectionDidChange:%@", _arrayController.selectedTheme.name);
        if (disregardTableSelection == YES) {
//            NSLog(@"Disregarding tableViewSelectionDidChange");
            disregardTableSelection = NO;
            return;
        }

        theme = _arrayController.selectedTheme;
        [self updatePrefsPanel];
        [self changeThemeName:theme.name];
        _btnRemove.enabled = theme.editable;

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

        // Send notification that theme has changed -- trigger configure events
        [[NSNotificationCenter defaultCenter]
         postNotification:[NSNotification notificationWithName:@"PreferencesChanged" object:theme]];
    }
    return;
}

- (void)changeThemeName:(NSString *)name {
    [[NSUserDefaults standardUserDefaults] setObject:name forKey:@"themeName"];
    _detailsHeader.stringValue = [NSString stringWithFormat:@"Settings for theme %@", name];
    _miscHeader.stringValue = _detailsHeader.stringValue;
    _zcodeHeader.stringValue = _detailsHeader.stringValue;
    _vOHeader.stringValue = _detailsHeader.stringValue;
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
    [NSString stringWithFormat:NSLocalizedString(@"The theme name \"%@\" is already in use.", nil), fieldEditor.string];
    anAlert.informativeText = NSLocalizedString(@"Please enter another name.", nil);
    [anAlert addButtonWithTitle:NSLocalizedString(@"Okay", nil)];
    [anAlert addButtonWithTitle:NSLocalizedString(@"Discard Change", nil)];

    [anAlert beginSheetModalForWindow:self.window completionHandler:^(NSInteger result){
        if (result == NSAlertSecondButtonReturn) {
            fieldEditor.string = theme.name;
        }
    }];
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
    [state encodeBool:_previewShown forKey:@"_previewShown"];
    [state encodeDouble:self.window.frame.size.height forKey:@"windowHeight"];
}

- (void)window:(NSWindow *)window didDecodeRestorableState:(NSCoder *)state {
    NSString *selectedfontString = [state decodeObjectOfClass:[NSString class] forKey:@"selectedFont"];
    if (selectedfontString != nil) {
        NSArray *fontsButtons = @[btnBufferFont, btnGridFont, btnAnyFont];
        for (NSButton *button in fontsButtons) {
            if ([button.identifier isEqualToString:selectedfontString]) {
                selectedFontButton = button;
            }
        }
    }
    _previewShown = [state decodeBoolForKey:@"_previewShown"];
    if (!_previewShown) {
        [self resizeWindowToHeight:kDefaultPrefWindowHeight];
    } else {
        CGFloat storedHeight = [state decodeDoubleForKey:@"windowHeight"];
        if (storedHeight > kDefaultPrefWindowHeight)
            [self resizeWindowToHeight:storedHeight];
        else
            [self resizeWindowToHeight:[self previewHeight]];
    }
}

- (NSUndoManager *)windowWillReturnUndoManager:(NSWindow *)window {
    return _managedObjectContext.undoManager;
}

#pragma mark Action menu

@synthesize oneThemeForAll = _oneThemeForAll;

- (void)setOneThemeForAll:(BOOL)oneThemeForAll {
    _oneThemeForAll = oneThemeForAll;
    [[NSUserDefaults standardUserDefaults] setBool:_oneThemeForAll forKey:@"OneThemeForAll"];
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
    if ([sender state] == NSOnState) {
        if (![[NSUserDefaults standardUserDefaults] valueForKey:@"UseForAllAlertSuppression"]) {
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
    self.oneThemeForAll = ([sender state] == NSOnState);
}

- (void)showUseForAllAlert:(NSArray *)games {
    NSAlert *anAlert = [[NSAlert alloc] init];
    anAlert.messageText =
    [NSString stringWithFormat:@"%@ %@ individual theme settings.", [NSString stringWithSummaryOf:games], (games.count == 1) ? @"has" : @"have"];
    anAlert.informativeText = [NSString stringWithFormat:@"Would you like to use theme %@ for all games?", theme.name];
    anAlert.showsSuppressionButton = YES;
    anAlert.suppressionButton.title = NSLocalizedString(@"Do not show again.", nil);
    [anAlert addButtonWithTitle:NSLocalizedString(@"Okay", nil)];
    [anAlert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

    Preferences * __unsafe_unretained weakSelf = self;
    [anAlert beginSheetModalForWindow:[self window] completionHandler:^(NSInteger result){

        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        NSString *alertSuppressionKey = @"UseForAllAlertSuppression";

        if (anAlert.suppressionButton.state == NSOnState) {
            // Suppress this alert from now on
            [defaults setBool:YES forKey:alertSuppressionKey];
        }
        if (result == NSAlertFirstButtonReturn) {
            weakSelf.oneThemeForAll = YES;
        } else {
            weakSelf.btnOneThemeForAll.state = NSOffState;
        }
    }];
}

- (NSString *)themeScopeTitle {
    if (_oneThemeForAll) return NSLocalizedString(@"Theme setting for all games", nil);
    if ( _currentGame == nil)
        return NSLocalizedString(@"No game is currently running", nil);
    else
        return [NSLocalizedString(@"Theme setting for game ", nil) stringByAppendingString:_currentGame.metadata.title];
}

- (IBAction)changeAdjustSize:(id)sender {
    _adjustSize = (BOOL)[sender state];
    [[NSUserDefaults standardUserDefaults] setBool:_adjustSize forKey:@"AdjustSize"];
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
    if (_previewShown) {
        [self resizeWindowToHeight:kDefaultPrefWindowHeight];
        _previewShown = NO;
    } else {
        _previewShown = YES;
        [self resizeWindowToHeight:[self previewHeight]];
    }
    [self performSelector:@selector(adjustPreview:) withObject:nil afterDelay:0.5];
    [[NSUserDefaults standardUserDefaults] setBool:_previewShown forKey:@"ShowThemePreview"];
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
        NSString* title = _previewShown ? NSLocalizedString(@"Hide Preview", nil) : NSLocalizedString(@"Show Preview", nil);
        ((NSMenuItem*)menuItem).title = title;
    }

    return YES;
}

#pragma mark User actions

- (void)changeBooleanAttribute:(NSString *)attribute fromButton:(NSButton *)button {
    if (((NSNumber *)[theme valueForKey:attribute]).intValue == button.state)
        return;
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    [themeToChange setValue:@(button.state) forKey:attribute];
}

- (void)changeMenuAttribute:(NSString *)attribute fromPopUp:(NSPopUpButton *)button {
    if (((NSNumber *)[theme valueForKey:attribute]).intValue == button.selectedTag)
        return;
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    [themeToChange setValue:@(button.selectedTag) forKey:attribute];
}

- (IBAction)changeDefaultSize:(id)sender {
    if (sender == txtCols) {
        if (theme.defaultCols == [sender intValue])
            return;
        theme = [self cloneThemeIfNotEditable];
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
        theme = [self cloneThemeIfNotEditable];
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
    Theme *themeToChange;
    NSColor *color = [sender color];
    if (!color) {
        NSLog(@"Preferences changeColor called with invalid color! %@ %@", sender, [sender color]);
        return;
    }

    if ([sender isKindOfClass:[NSColorPanel class]]) {
        if (clrGridFg.active)
            sender = clrGridFg;
        else if (clrGridBg.active)
            sender = clrGridBg;
        else if (clrBufferFg.active)
            sender = clrBufferFg;
        else if (clrBufferBg.active)
            sender = clrBufferBg;
        else if (clrAnyFg.active)
            sender = clrAnyFg;
    }

    if (sender == clrGridFg) {
        key = @"gridNormal";
        if ([self selectedStyle] == theme.gridNormal)
            clrAnyFg.color = color;
    } else if (sender == clrGridBg) {
        if ([theme.gridBackground isEqualToColor:color])
            return;
        themeToChange = [self cloneThemeIfNotEditable];
        themeToChange.gridBackground = color;
    } else if (sender == clrBufferFg) {
        key = @"bufferNormal";
        if ([self selectedStyle] == theme.bufferNormal)
            clrAnyFg.color = color;
    } else if (sender == clrBufferBg) {
        if ([theme.bufferBackground isEqualToColor:color])
            return;
        themeToChange = [self cloneThemeIfNotEditable];
        themeToChange.bufferBackground = color;
    } else if (sender == clrAnyFg) {
        key = [self selectedStyleName];
    } else return;

    GlkStyle *style = nil;
    if (key) {
        //NSLog(@"key: %@", key);
        style = [theme valueForKey:key];
        if ([style.color isEqualToColor:color])
            return;

        themeToChange = [self cloneThemeIfNotEditable];
        style = [themeToChange valueForKey:key];

        if (!style.attributeDict) {
            NSLog(@"Preferences changeColor called with invalid theme object!");
            return;
        }

        style.color = color;
    }

    style.autogenerated = NO;
    if (style == themeToChange.bufferNormal || style == themeToChange.gridNormal)
        [Preferences rebuildTextAttributes];
    else
        [[NSNotificationCenter defaultCenter]
         postNotification:[NSNotification notificationWithName:@"PreferencesChanged" object:theme]];
}

- (IBAction)swapColors:(id)sender {
    NSColor *tempCol;
    if (sender == _swapBufColBtn) {
        tempCol = clrBufferFg.color;
        clrBufferFg.color = clrBufferBg.color;
        clrBufferBg.color = tempCol;
        [self changeColor:clrBufferFg];
        [self changeColor:clrBufferBg];
    } else if (sender == _swapGridColBtn) {
        tempCol = clrGridFg.color;
        clrGridFg.color = clrGridBg.color;
        clrGridBg.color = tempCol;
        [self changeColor:clrGridFg];
        [self changeColor:clrGridBg];
    }
}

- (IBAction)changeMargin:(id)sender  {
    NSString *key = nil;
    NSInteger val = 0;
    Theme *themeToChange;
    val = [sender intValue];

    if (sender == txtGridMargin) {
        if (theme.gridMarginX == val)
            return;
        themeToChange = [self cloneThemeIfNotEditable];
        key = @"GridMargin";
        themeToChange.gridMarginX = val;
        themeToChange.gridMarginY = val;
    }
    if (sender == txtBufferMargin) {
        if (theme.bufferMarginX == val)
            return;
        themeToChange = [self cloneThemeIfNotEditable];
        key = @"BufferMargin";
        themeToChange.bufferMarginX = val;
        themeToChange.bufferMarginY = val;
    }

    if (key) {
        [Preferences rebuildTextAttributes];
    }
}

- (IBAction)changeSmartQuotes:(id)sender {
    [self changeBooleanAttribute:@"smartQuotes" fromButton:sender];
}

- (IBAction)changeSpaceFormatting:(id)sender {
    [self changeBooleanAttribute:@"spaceFormat" fromButton:sender];
}

- (IBAction)changeEnableGraphics:(id)sender {
    [self changeBooleanAttribute:@"doGraphics" fromButton:sender];
}

- (IBAction)changeEnableSound:(id)sender {
    [self changeBooleanAttribute:@"doSound" fromButton:sender];
}

- (IBAction)changeEnableStyles:(id)sender {
    [self changeBooleanAttribute:@"doStyles" fromButton:sender];
    [Preferences rebuildTextAttributes];
}

- (IBAction)changeStylePopup:(id)sender {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSInteger windowType = [_windowTypePopup selectedTag];
    [defaults setInteger:windowType forKey:@"SelectedGlkWindowType"];
    [defaults setInteger:[_styleNamePopup selectedTag] forKey:@"SelectedStyle"];
    GlkStyle *selectedStyle = [self selectedStyle];
    clrAnyFg.color = selectedStyle.color;
    btnAnyFont.title = fontToString(selectedStyle.font);
    selectedFontButton = btnAnyFont;
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    [self.dummyTextView updateTextWithAttributes:selectedStyle.attributeDict];
    NSMutableDictionary *convertedAttributes = selectedStyle.attributeDict.mutableCopy;

    convertedAttributes[@"NSDocumentBackgroundColor"] = (windowType == wintype_TextGrid) ?
    theme.gridBackground : theme.bufferBackground;
    [fontManager setSelectedFont:selectedStyle.font isMultiple:NO];
    [fontManager setSelectedAttributes:convertedAttributes isMultiple:NO];
}

#pragma mark Margin Popover

- (IBAction)showMarginPopover:(id)sender {
    _marginHorizontalGridTextField.integerValue = theme.gridMarginX;
    _marginHorizontalGridStepper.integerValue = theme.gridMarginX;
    
    _marginVerticalGridTextField.integerValue = theme.gridMarginY;
    _marginVerticalGridStepper.integerValue = theme.gridMarginY;

    _marginHorizontalBufferTextField.integerValue = theme.bufferMarginX;
    _marginHorizontalBufferStepper.integerValue = theme.bufferMarginX;

    _marginVerticalBufferTextField.integerValue = theme.bufferMarginY;
    _marginVerticalBufferStepper.integerValue = theme.bufferMarginY;

    [_marginsPopover showRelativeToRect:[sender bounds] ofView:sender preferredEdge:NSMaxYEdge];
}

- (IBAction)changeGridHorizontalMargin:(id)sender {
    if (theme.gridMarginX == [sender integerValue])
        return;
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.gridMarginX = [sender integerValue];
    _marginHorizontalGridTextField.integerValue = themeToChange.gridMarginX;
    _marginHorizontalGridStepper.integerValue = themeToChange.gridMarginX;
}

- (IBAction)changeGridVerticalMargin:(id)sender {
    if (theme.gridMarginY == [sender integerValue])
        return;
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.gridMarginY = [sender integerValue];
    _marginVerticalGridTextField.integerValue = themeToChange.gridMarginY;
    _marginVerticalGridStepper.integerValue = themeToChange.gridMarginY;
}

- (IBAction)changeBufferHorizontalMargin:(id)sender {
    if (theme.bufferMarginX == [sender integerValue])
        return;
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.bufferMarginX = [sender integerValue];
    _marginHorizontalBufferTextField.integerValue = themeToChange.bufferMarginX;
    _marginHorizontalBufferStepper.integerValue = themeToChange.bufferMarginX;
}
- (IBAction)changeBufferVerticalMargin:(id)sender {
    if (theme.bufferMarginY == [sender integerValue])
        return;
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.bufferMarginY = [sender integerValue];
    _marginVerticalBufferTextField.integerValue = themeToChange.bufferMarginY;
    _marginVerticalBufferStepper.integerValue = themeToChange.bufferMarginY;
}

- (IBAction)showParagraphPopOver:(id)sender {
    if (!_paragraphPopover)
        _paragraphPopover = [[ParagraphPopOver alloc] initWithNibName:@"ParagraphPopOver" bundle:nil];

    [_paragraphPopover presentViewController:_paragraphPopover asPopoverRelativeToRect:[sender frame] ofView:[sender superview] preferredEdge:NSMaxYEdge behavior:NSPopoverBehaviorTransient];
    [_paragraphPopover refreshForStyle:[self selectedStyle]];
}


#pragma mark VoiceOver menu

- (IBAction)changeVOSpeakCommands:(id)sender {
    [self changeBooleanAttribute:@"vOSpeakCommand" fromButton:sender];
}

- (IBAction)changeVOMenuMenu:(id)sender {
    if (theme.vOSpeakMenu == (int)[sender selectedTag])
        return;
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.vOSpeakMenu = (int)[sender selectedTag];
}

- (IBAction)changeVOImageMenu:(id)sender {
    if (theme.vOSpeakImages == (int)[sender selectedTag])
        return;
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.vOSpeakImages = (int)[sender selectedTag];
}

#pragma mark ZCode menu

- (IBAction)changeBeepHighMenu:(id)sender {
    NSString *title = [sender titleOfSelectedItem];
    NSSound *sound = [NSSound soundNamed:title];
    if (@available(macOS 11, *)) {
        if (!sound) {
            title = bigSurSoundsToCatalina[title];
            sound = [NSSound soundNamed:title];
        }
    }
    if (sound) {
        [sound stop];
        [sound play];
    }
    if ([theme.beepHigh isEqualToString:title])
        return;
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.beepHigh = title;
}

- (IBAction)changeBeepLowMenu:(id)sender {
    NSString *title = [sender titleOfSelectedItem];
    NSSound *sound = [NSSound soundNamed:title];
    if (@available(macOS 11, *)) {
        if (!sound) {
            title = bigSurSoundsToCatalina[title];
            sound = [NSSound soundNamed:title];
        }
    }
    if (sound) {
        [sound stop];
        [sound play];
    }

    if ([theme.beepLow isEqualToString:title])
        return;
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.beepLow = title;
}

- (IBAction)changeZterpMenu:(id)sender {
    [self changeMenuAttribute:@"zMachineTerp" fromPopUp:sender];
}

- (IBAction)changeBZArrowsMenu:(id)sender {
    [self changeMenuAttribute:@"bZTerminator" fromPopUp:sender];
}

- (IBAction)changeZVersion:(id)sender {
    if ([theme.zMachineLetter isEqualToString:[sender stringValue]]) {
        return;
    }
    if ([sender stringValue].length == 0) {
        _zVersionTextField.stringValue = theme.zMachineLetter;
        return;
    }
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.zMachineLetter = [sender stringValue];
}


- (IBAction)changeBZVerticalAdjustment:(id)sender {
    if (theme.bZAdjustment == [sender integerValue]) {
        return;
    }
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.bZAdjustment = [sender integerValue];
    _bZVerticalStepper.integerValue = themeToChange.bZAdjustment;
    _bZVerticalTextField.integerValue = themeToChange.bZAdjustment;
}

- (IBAction)changeQuoteBoxCheckBox:(id)sender {
    [self changeBooleanAttribute:@"quoteBox" fromButton:sender];
}

#pragma mark Misc menu

- (IBAction)resetDialogs:(NSButton *)sender {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setBool:NO forKey:@"TerminationAlertSuppression"];
    [defaults setBool:NO forKey:@"UseForAllAlertSuppression"];
    [defaults setBool:NO forKey:@"OverwriteStylesAlertSuppression"];
    [defaults setBool:NO forKey:@"AutorestoreAlertSuppression"];
    [defaults setBool:NO forKey:@"CloseAlertSuppression"];
    [defaults setBool:NO forKey:@"CommandScriptAlertSuppression"];
    [defaults setBool:NO forKey:@"SaveFileAlertSuppression"];
}

- (IBAction)changeSmoothScroll:(id)sender {
    [self changeBooleanAttribute:@"smoothScroll" fromButton:sender];
}

- (IBAction)changeAutosaveOnTimer:(id)sender {
    [self changeBooleanAttribute:@"autosaveOnTimer" fromButton:sender];
}

- (IBAction)changeAutosave:(id)sender {
    [self changeBooleanAttribute:@"autosave" fromButton:sender];
    _btnAutosaveOnTimer.enabled = ([sender state] == NSOnState);
}

- (IBAction)changeTimerSlider:(id)sender {
    if ([sender integerValue] == 0 || theme.minTimer == 1000.0 / [sender integerValue]) {
        return;
    }
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.minTimer = (1000.0 / [sender integerValue]);
    _timerTextField.integerValue = [sender integerValue];
    _timerSlider.integerValue = [sender integerValue];
}

- (IBAction)changeDeterminism:(id)sender {
    [self changeBooleanAttribute:@"determinism" fromButton:sender];
}

- (IBAction)changeNoHacks:(id)sender {
    if (theme.nohacks == ([sender state] == NSOffState))
        return;
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.nohacks = ([sender state] == NSOffState);
}

- (IBAction)changeErrorHandlingPopup:(id)sender {
    [self changeMenuAttribute:@"errorHandling" fromPopUp:sender];
}
- (IBAction)changeImageReplacePopup:(NSPopUpButton *)sender {
    if ([[NSUserDefaults standardUserDefaults] integerForKey:@"ImageReplacement"] == sender.selectedTag)
        return;

    [[NSUserDefaults standardUserDefaults]  setInteger:sender.selectedTag forKey:@"ImageReplacement"];
}

#pragma mark End of Misc menu

- (IBAction)changeOverwriteStyles:(id)sender {
    if ([sender state] == 1) {
        if (![[NSUserDefaults standardUserDefaults] valueForKey:@"OverwriteStylesAlertSuppression"]) {
            NSMutableArray *customStyles = [[NSMutableArray alloc] initWithCapacity:style_NUMSTYLES * 2];
            for (GlkStyle *style in theme.allStyles) {
                if (!style.autogenerated && style != theme.bufferNormal && style != theme.gridNormal) {
                    [customStyles addObject:style];
                }
            }
            if (customStyles.count) {
                [self showOverwriteStylesAlert:customStyles];
                return;
            }
        }
        [self overWriteStyles];
    }
}

- (void)showOverwriteStylesAlert:(NSArray *)styles {
    NSAlert *anAlert = [[NSAlert alloc] init];
    anAlert.messageText =
    [NSString stringWithFormat:@"This theme uses %ld custom %@.", styles.count, (styles.count == 1) ? @"style" : @"styles"];
    if (styles.count == 1)
        anAlert.informativeText = NSLocalizedString(@"Do you want to replace it with an autogenerated style?", nil);
    else
        anAlert.informativeText = NSLocalizedString(@"Do you want to replace them with autogenerated styles?", nil);

    anAlert.showsSuppressionButton = YES;
    anAlert.suppressionButton.title = NSLocalizedString(@"Do not show again.", nil);
    [anAlert addButtonWithTitle:NSLocalizedString(@"Okay", nil)];
    [anAlert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

    Preferences * __unsafe_unretained weakSelf = self;

    [anAlert beginSheetModalForWindow:[self window] completionHandler:^(NSInteger result) {

        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

        NSString *alertSuppressionKey = @"OverwriteStylesAlertSuppression";

        if (anAlert.suppressionButton.state == NSOnState) {
            // Suppress this alert from now on
            [defaults setBool:YES forKey:alertSuppressionKey];
        }

        if (result == NSAlertFirstButtonReturn) {
            [weakSelf overWriteStyles];
        } else {
            weakSelf.btnOverwriteStyles.state = NSOffState;
        }
    }];
}

- (void)overWriteStyles {
    theme = [self cloneThemeIfNotEditable];
    for (GlkStyle *style in theme.allStyles) {
        style.autogenerated = YES;
    }
    [theme populateStyles];
    [Preferences rebuildTextAttributes];
}

- (IBAction)changeBorderSize:(id)sender {
    if (theme.border == [sender intValue])
        return;
    theme = [self cloneThemeIfNotEditable];
    NSInteger oldBorder = theme.border;
    theme.border = [sender intValue];
    NSInteger diff = theme.border - oldBorder;
    [[NSNotificationCenter defaultCenter]
        postNotificationName:@"BorderChanged"
                      object:theme
     userInfo:@{@"diff":@(diff)}];
}

- (Theme *)cloneThemeIfNotEditable {
    if (!theme.editable) {
//        NSLog(@"Cloned theme %@", theme.name);
        if ([themeDuplicationTimestamp timeIntervalSinceNow] > -0.5 && lastDuplicatedTheme && lastDuplicatedTheme.editable) {
            return lastDuplicatedTheme;
        }

        Theme *clonedTheme = theme.clone;
        clonedTheme.editable = YES;
        NSString *name = [theme.name stringByAppendingString:@" (modified)"];
        NSUInteger counter = 2;
        while ([_arrayController findThemeByName:name]) {
            name = [NSString stringWithFormat:@"%@ (modified) %ld", theme.name, counter++];
        }
        clonedTheme.name = name;
        [self changeThemeName:name];
        _btnRemove.enabled = YES;
        theme = clonedTheme;
        lastDuplicatedTheme = clonedTheme;
        disregardTableSelection = YES;
        [self performSelector:@selector(restoreThemeSelection:) withObject:clonedTheme afterDelay:0.1];
        themeDuplicationTimestamp = [NSDate date];
        return clonedTheme;
    }
    return theme;
}

#pragma mark Zoom

+ (void)zoomIn {
    zoomDirection = ZOOMRESET;
    NSFont *gridroman = theme.gridNormal.font;
//    NSLog(@"zoomIn gridroman.pointSize = %f", gridroman.pointSize);

    if (gridroman.pointSize < 200) {
        zoomDirection = ZOOMIN;
        [self scale:(gridroman.pointSize + 1) / gridroman.pointSize];
    }
}

+ (void)zoomOut {
//    NSLog(@"zoomOut");
    zoomDirection = ZOOMRESET;
    NSFont *gridroman = theme.gridNormal.font;
    if (gridroman.pointSize > 6) {
        zoomDirection = ZOOMOUT;
        [self scale:(gridroman.pointSize - 1) / gridroman.pointSize];
    }
}

+ (void)zoomToActualSize {
//    NSLog(@"zoomToActualSize");
    zoomDirection = ZOOMRESET;

    CGFloat scale;
    Theme *parent = theme.defaultParent;
    while (parent.defaultParent)
        parent = parent.defaultParent;

    if (parent)
        scale = parent.gridNormal.font.pointSize;

    if (scale < 6)
        scale = 12;

    [self scale:scale / theme.gridNormal.font.pointSize];
}

+ (void)scale:(CGFloat)scalefactor {
//    NSLog(@"Preferences scale: %f", scalefactor);
    if (scalefactor < 0)
        scalefactor = fabs(scalefactor);

    [prefs cloneThemeIfNotEditable];

    CGFloat fontSize;

    for (GlkStyle *style in theme.allStyles) {
        fontSize = style.font.pointSize;
        fontSize *= scalefactor;
        if (fontSize > 0) {
            style.font = [NSFont fontWithDescriptor:style.font.fontDescriptor
                                               size:fontSize];
        }
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
    btnAnyFont.title = fontToString([self selectedStyle].font);
}

#pragma mark Font panel

- (IBAction)showFontPanel:(id)sender {
    selectedFontButton = sender;
    NSFont *selectedFont = nil;
    NSColor *selectedFontColor = nil;
    NSColor *selectedDocumentColor = nil;
    GlkStyle *selectedStyle = nil;

    selectedFont = theme.bufferNormal.font;
    selectedFontColor = theme.bufferNormal.color;
    selectedDocumentColor = theme.bufferBackground;
    selectedStyle = theme.bufferNormal;

    if (sender == btnGridFont) {
        selectedFont = theme.gridNormal.font;
        selectedFontColor = theme.gridNormal.color;
        selectedDocumentColor = theme.gridBackground;
        selectedStyle = theme.gridNormal;
    }
    if (sender == btnAnyFont) {
        selectedStyle = [self selectedStyle];
        selectedFont = selectedStyle.font;
        selectedFontColor = selectedStyle.color;
        NSInteger windowType = _windowTypePopup.selectedTag;
        selectedDocumentColor = (windowType == wintype_TextGrid) ?
        theme.gridBackground : theme.bufferBackground;
    }

    NSDictionary *attConvDict = @{ NSForegroundColorAttributeName: @"NSColor",
                                   NSUnderlineStyleAttributeName: @"NSUnderline",
                                   NSUnderlineColorAttributeName: @"NSUnderlineColor",
                                   NSStrikethroughStyleAttributeName: @"NSStrikethrough",
                                   NSStrikethroughColorAttributeName: @"NSStrikethroughColor",
                                   NSShadowAttributeName: @"NSShadow",
                                   NSLigatureAttributeName: @"NSLigature",
                                   NSFontAttributeName: @"NSFont" };

    NSMutableDictionary *attr = [NSMutableDictionary new];
    NSDictionary *oldAttr = selectedStyle.attributeDict;
    for (NSString *key in oldAttr.allKeys) {
        NSString *newKey = attConvDict[key];
        if (!newKey)
            newKey = key;
        attr[newKey] = oldAttr[key];
    }
    attr[@"NSColor"] = selectedFontColor;
    attr[@"Font"] = selectedFont;
    attr[@"NSDocumentBackgroundColor"] = selectedDocumentColor;

    [self.window makeFirstResponder:self.window];

    [self.dummyTextView updateTextWithAttributes:selectedStyle.attributeDict];

    NSFontPanel *fontPanel = [NSFontPanel sharedFontPanel];
    fontPanel.delegate = self.dummyTextView;
    if (!fontPanel.visible)
        [fontPanel makeKeyAndOrderFront:self];

    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    fontManager.target = self.dummyTextView;
    [fontManager setSelectedFont:selectedFont isMultiple:NO];
    [fontManager setSelectedAttributes:attr isMultiple:NO];
}


- (IBAction)changeFont:(id)fontManager {
//    NSLog(@"Prefs: changeFont: %@", fontManager);
    if (fontManager != self.dummyTextView) {
        [self.dummyTextView changeFont:fontManager];
        return;
    } else {
        fontManager = self.dummyTextView.sender;
    }
    NSFont *newFont = nil;
    if (selectedFontButton) {
        newFont = [fontManager convertFont:[fontManager selectedFont]];
    } else {
        NSLog(@"Error! Preferences changeFont called with no font selected");
        return;
    }

    Theme *themeToChange;
    if (selectedFontButton == btnGridFont) {
        if ([theme.gridNormal.font isEqual:newFont])
            return;
        themeToChange = [self cloneThemeIfNotEditable];
        themeToChange.gridNormal.font = newFont;
        btnGridFont.title = fontToString(newFont);
    } else if (selectedFontButton == btnBufferFont) {
        if ([themeToChange.bufferNormal.font isEqual:newFont])
            return;
        themeToChange = [self cloneThemeIfNotEditable];
        themeToChange.bufferNormal.font = newFont;
        btnBufferFont.title = fontToString(newFont);
    } else if (selectedFontButton == btnAnyFont) {
        themeToChange = [self cloneThemeIfNotEditable];
        GlkStyle *selectedStyle = [themeToChange valueForKey:[self selectedStyleName]];
        if ([selectedStyle.font isEqual:newFont])
            return;

        selectedStyle.autogenerated = NO;
        selectedStyle.font = newFont;
        btnAnyFont.title = fontToString(newFont);
    }

    [Preferences rebuildTextAttributes];
}

// This is sent from the font panel when changing font style there

- (void)changeAttributes:(id)sender {
    NSLog(@"Prefs: changeAttributes:%@", sender);

    if (sender != self.dummyTextView) {
        [self.dummyTextView changeAttributes:sender];
        return;
    } else {
        sender = self.dummyTextView.sender;
    }

    NSFontManager *fontManager = [NSFontManager sharedFontManager];

    GlkStyle *style = theme.bufferNormal;
    if (selectedFontButton == btnGridFont)
        style = theme.gridNormal;
    else if (selectedFontButton == btnAnyFont)
        style = [self selectedStyle];

    if (style) {
        NSDictionary *attDict = [self.dummyTextView.textStorage attributesAtIndex:0 effectiveRange:nil];

        [fontManager setSelectedFont:attDict[NSFontAttributeName] isMultiple:NO];
        [fontManager setSelectedAttributes:attDict isMultiple:NO];

        Theme *themeToChange = [self cloneThemeIfNotEditable];
        if (selectedFontButton == btnBufferFont)
            style = themeToChange.bufferNormal;
        else if (selectedFontButton == btnGridFont)
            style = themeToChange.gridNormal;
        else if (selectedFontButton == btnAnyFont) {
            style = [themeToChange valueForKey:[self selectedStyleName]];
        }
        style.attributeDict = attDict;
        style.autogenerated = NO;
    }

    NSDictionary *newAttributes = [sender convertAttributes:@{}];

    if (newAttributes[@"NSColor"]) {
        NSColorWell *colorWell = nil;
        if (style == theme.gridNormal)
            colorWell = clrGridFg;
        else if (style == theme.bufferNormal)
            colorWell = clrBufferFg;
        else if (style == [self selectedStyle])
            colorWell = clrAnyFg;
        if (colorWell) {
            colorWell.color = newAttributes[@"NSColor"];
            [self changeColor:colorWell];
        }
    }

    [Preferences rebuildTextAttributes];
}

// This is sent from the font panel when changing background color there

- (void)changeDocumentBackgroundColor:(id)sender {
    //    NSLog(@"changeDocumentBackgroundColor");

    if (sender != self.dummyTextView) {
        [self.dummyTextView changeDocumentBackgroundColor:sender];
        return;
    } else {
        sender = self.dummyTextView.sender;
    }

    GlkStyle *style = theme.bufferNormal;
    if (selectedFontButton == btnGridFont)
        style = theme.gridNormal;
    else if (selectedFontButton == btnAnyFont)
        style = [self selectedStyle];

    NSColorWell *colorWell = nil;
    if (style == theme.gridNormal)
        colorWell = clrGridBg;
    else if (style == theme.bufferNormal)
        colorWell = clrBufferBg;
    else if (style == [self selectedStyle]) {
        NSInteger windowType = _windowTypePopup.selectedTag;
        colorWell = (windowType == wintype_TextGrid) ? clrGridBg : clrBufferBg;
    }
    colorWell.color = [sender color];
    [self changeColor:colorWell];
}

- (NSFontPanelModeMask)validModesForFontPanel:(NSFontPanel *)fontPanel {
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

@synthesize dummyTextView = _dummyTextView;

- (DummyTextView *)dummyTextView {
    if (!_dummyTextView) {
        _dummyTextView = [[DummyTextView alloc] initWithFrame:NSMakeRect(0,0,200,500)];
    }
    return _dummyTextView;
}

- (void)setDummyTextView:(DummyTextView *)dummyTextView {
    _dummyTextView = dummyTextView;
}

@end
