#include "glk.h"

#import "AppDelegate.h"
#import "BufferTextView.h"
#import "BuiltInThemes.h"
#import "Constants.h"
#import "CoreDataManager.h"
#import "DummyTextView.h"
#import "Game.h"
#import "GlkController.h"
#import "GlkTextBufferWindow.h"
#import "GlkStyle.h"
#import "LibraryOrganizer.h"
#import "Metadata.h"
#import "NSAlert+Spatterlight.h"
#import "NotificationBezel.h"
#import "NSColor+integer.h"
#import "NSString+Categories.h"
#import "ParagraphPopOver.h"
#import "PreviewController.h"
#import "TableViewController.h"
#import "TableViewController+TableDelegate.h"
#import "TableViewController+LibraryManagement.h"
#import "Theme.h"
#import "ThemeArrayController.h"
#import "Fetches.h"

#import "PreferencesPrivate.h"

#import "DebugLog.h"

NSNotificationName const kPreferencesChangedNotification = @"PreferencesChanged";
NSNotificationName const kDefaultSizeChangedNotification = @"DefaultSizeChanged";
NSNotificationName const kBorderChangedNotification = @"BorderChanged";
NSNotificationName const kColorModeChangedNotification = @"ColorModeChanged";

@implementation Preferences

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
    NSString *name = [defaults objectForKey:kThemeNameKey];

    if (!name)
        name = @"Old settings";

    CoreDataManager *coreDataManager = ((AppDelegate*)NSApp.delegate).coreDataManager;

    NSManagedObjectContext *managedObjectContext = coreDataManager.mainManagedObjectContext;

    Theme *savedTheme = [Fetches findTheme:name inContext:managedObjectContext];
    if (!savedTheme) {
        NSLog(@"Preference readDefaults: Error! Saved theme %@ not found. Creating new default theme!", name);
        self.theme = [BuiltInThemes createThemeFromDefaultsPlistInContext:managedObjectContext];
        if (!self.theme)
            self.theme = [BuiltInThemes createDefaultThemeInContext:managedObjectContext forceRebuild:NO];
        if (!self.theme) {
            NSLog(@"Preference readDefaults: Error! Could not create default theme!");
        }
    } else self.theme = savedTheme;

    // Rebuild default themes the first time a new Spatterlight version is run (or the preferences are deleted.)
    // Rebuilding is so quick that this may be overkill. Perhaps we should just rebuild on every run?
    BOOL forceRebuild = NO;
    NSString *appBuildString = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"];

    NSString *lastThemesRebuild = [defaults objectForKey:kLastThemesRebuildKey];

    if (![lastThemesRebuild isEqualToString:appBuildString]) {
        forceRebuild = YES;
        [defaults setObject:appBuildString forKey:kLastThemesRebuildKey];
    }

    // We may or may not have created the Default and Old themes already above.
    // Then these won't be recreated below.
    [BuiltInThemes createBuiltInThemesInContext:managedObjectContext forceRebuild:forceRebuild];
}

+ (void)changeCurrentGlkController:(GlkController *)ctrl {
    Preferences *prefs = self.instance;
    if (prefs) {
        // Never read self.theme raw here. It can be nil if the built-in
        // themes could not be created at startup, or if an earlier call handed
        // restoreThemeSelection: an empty themes table. objectWithID: raises on
        // a nil object ID, so a nil theme used to abort the app on game launch.
        // currentTheme recreates a default theme when this happens.
        Theme *currentTheme = [Preferences currentTheme];

        // The preferences window works on the library's store. A game from
        // another store (a test's in-memory one) cannot be mapped into it --
        // objectWithID: raises -- so show the window as having no game.
        if (ctrl.game.managedObjectContext && currentTheme.managedObjectContext &&
            ctrl.game.managedObjectContext.persistentStoreCoordinator !=
            currentTheme.managedObjectContext.persistentStoreCoordinator)
            ctrl = nil;

        Game *game = nil;
        if (ctrl.game)
            game = [currentTheme.managedObjectContext objectWithID:ctrl.game.objectID];
        if (prefs.currentGame == game)
            return;
        if (ctrl == nil) {
            prefs.currentGame = nil;
        } else if (currentTheme) {
            prefs.currentGame = game;
            Theme *ctrlCtxTheme = [ctrl.game.managedObjectContext objectWithID:currentTheme.objectID];
            if (!ctrl.theme)
                ctrl.theme = ctrlCtxTheme;
            // ctrl.theme is still nil if the game has no context of its own.
            if (ctrl.theme)
                self.theme = [currentTheme.managedObjectContext objectWithID:ctrl.theme.objectID];
            if (!game.theme)
                game.theme = self.theme;
        } else {
            prefs.currentGame = game;
        }
        [prefs restoreThemeSelection:self.theme];
        prefs.themesHeader.stringValue = [prefs themeScopeTitle];
    }
}

+ (void)initialize {
    [self initFactoryDefaults];
    [self readDefaults];

    [self rebuildTextAttributes];
}

#pragma mark Global accessors

// Backing storage of the class properties. Only the accessors below use it.
static Theme *sTheme = nil;
static Preferences *sInstance = nil;
static kZoomDirectionType sZoomDirection = ZOOMRESET;

+ (Theme *)theme {
    return sTheme;
}

+ (void)setTheme:(Theme *)aTheme {
    sTheme = aTheme;
}

- (Theme *)theme {
    return sTheme;
}

- (void)setTheme:(Theme *)aTheme {
    sTheme = aTheme;
}

+ (kZoomDirectionType)zoomDirection {
    return sZoomDirection;
}

+ (void)setZoomDirection:(kZoomDirectionType)zoomDirection {
    sZoomDirection = zoomDirection;
}

+ (Theme *)currentTheme {
    if (!self.theme)
        self.theme = self.instance.defaultTheme;
    return self.theme;
}

+ (Preferences *)instance {
    return sInstance;
}

+ (void)setInstance:(Preferences *)instance {
    sInstance = instance;
}

#pragma mark GlkStyle and attributed-string magic

+ (void)rebuildTextAttributes {
    [self.theme populateStyles];
    NSSize cellsize = self.theme.gridNormal.cellSize;
    self.theme.cellWidth = cellsize.width;
    self.theme.cellHeight = cellsize.height;
    cellsize = self.theme.bufferNormal.cellSize;
    self.theme.bufferCellWidth = cellsize.width;
    self.theme.bufferCellHeight = cellsize.height;
}

- (void)dealloc {
    [self stopWaitingForThemesTable];
}

- (void)windowDidLoad {
    [super windowDidLoad];

    self.window.delegate = self;

    disregardTableSelection = YES;

    [self setUpZArrowsMenuItems];
    [self setUpBeepMenus];
    [self setUpThemeState];
    [self setUpSimpleSettings];

    Preferences.instance = self;

    [self setUpScrollView];

    [self changeThemeName:self.theme.name];

    NSToolbar *toolbar = [self setUpToolbar];

    _previewShown = [[NSUserDefaults standardUserDefaults] boolForKey:kShowThemePreviewKey];

    // The tool menu is also the contextual menu of the themes table, where
    // it applies to the clicked theme. Skip the hidden pull-down title item.
    NSMenu *contextMenu = [_actionButton.menu copy];
    [contextMenu removeItemAtIndex:0];
    themesTableView.menu = contextMenu;

    [self setUpWindowHeight];
    [self showSelectedPanelOfToolbar:toolbar];

    [self restoreThemeSelection:self.theme];

    self.windowFrameAutosaveName = @"PrefsPanel";
    themesTableView.autosaveName = @"ThemesTable";

    [self addObservers];
}

#pragma mark windowDidLoad steps

- (void)setUpZArrowsMenuItems {
    _standardZArrowsMenuItem.title = NSLocalizedString(@"↑ and ↓ work as in original", nil);
    _standardZArrowsMenuItem.toolTip = NSLocalizedString(@"↑ and ↓ navigate menus and status windows. \u2318↑ and \u2318↓ step through command history.", nil);
    _compromiseZArrowsMenuItem.title = NSLocalizedString(@"Replaced by \u2318↑ and \u2318↓", nil);
    _compromiseZArrowsMenuItem.toolTip = NSLocalizedString(@"\u2318↑ and \u2318↓ are used where the original uses ↑ and ↓. ↑ and ↓ step through command history as in other games.", nil);
    _strictZArrowsMenuItem.title = NSLocalizedString(@"↑↓ and ←→ work as in original", nil);
    _strictZArrowsMenuItem.toolTip = NSLocalizedString(@"↑ and ↓ navigate menus and status windows. \u2318↑ and \u2318↓ step through command history. ← and → don't do anything.", nil);
}

// On Big Sur and later the system alert sounds have new names
- (void)setUpBeepMenus {
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
}

- (void)setUpThemeState {
    if (!self.theme)
        self.theme = self.defaultTheme;

    _previewController.theme = self.theme;

    [self findDarkAndLightThemes];
    if (_lightTheme && [Preferences currentSystemMode] == kLightMode) {
        _lightOverrideActive = YES;
    } else if (_darkTheme && [Preferences currentSystemMode] == kDarkMode) {
        _darkOverrideActive = YES;
    }

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    _oneThemeForAll = [defaults boolForKey:kOneThemeForAllKey];
    _themesHeader.stringValue = [self themeScopeTitle];

    _adjustSize = [defaults boolForKey:kAdjustSizeKey];
}

- (void)setUpScrollView {
    _scrollView.scrollerStyle = NSScrollerStyleOverlay;
    _scrollView.drawsBackground = YES;
    _scrollView.hasHorizontalScroller = NO;
    _scrollView.hasVerticalScroller = YES;
    _scrollView.verticalScroller.alphaValue = 1;
    _scrollView.autohidesScrollers = YES;
    _scrollView.borderType = NSNoBorder;
}

- (NSToolbar *)setUpToolbar {
    themesPanel = @"themes";
    stylesPanel = @"styles";
    presentationPanel = @"presentation";
    formatPanel = @"format";
    voiceOverPanel = @"voiceOver";
    miscPanel = @"misc";
    globalPanel = @"global";

    itemIdentifierToViewDict = @{
        themesPanel:_themesView,
        stylesPanel:_stylesView,
        presentationPanel:_presentationView,
        formatPanel:_formatView,
        voiceOverPanel:_voiceOverView,
        miscPanel:_miscView,
        globalPanel:_globalView
    };

    NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier:@"prefsToolbar"];
    toolbar.delegate = self;
    toolbar.displayMode = NSToolbarDisplayModeIconAndLabel;
    toolbar.allowsUserCustomization = NO;
    if (@available(macOS 11.0, *)) {
        self.window.toolbarStyle = NSWindowToolbarStylePreference;
    }
    self.window.toolbar = toolbar;

    if (toolbar.selectedItemIdentifier == nil)
        toolbar.selectedItemIdentifier = themesPanel;

    return toolbar;
}

// Fits the window to the current panel, plus the preview if it is shown
- (void)setUpWindowHeight {
    if (!_belowView.subviews.count) {
        currentPanel = _themesView;
    } else {
        currentPanel = _belowView.subviews[0];
    }

    defaultWindowHeight = ceil(NSHeight([self.window frameRectForContentRect:NSMakeRect(0, 0,  kDefaultPrefWindowWidth, NSHeight(currentPanel.frame))]));

    if (!_previewShown) {
        _previewHeightConstraint.constant = 0;
        [self resizeWindowToHeight:defaultWindowHeight animate:NO];
    } else {
        CGFloat height = NSHeight(self.window.frame) - defaultWindowHeight;
        if (height <= 0)
            height = [_previewController calculateHeight] + 40;
        [self resizeWindowToHeight:height + defaultWindowHeight animate:NO];
        [_previewController fixScrollBar];
    }
}

- (void)showSelectedPanelOfToolbar:(NSToolbar *)toolbar {
    BOOL found = NO;
    for (NSToolbarItem *item in toolbar.items) {
        if ([item.itemIdentifier isEqual:toolbar.selectedItemIdentifier]) {
            [self switchToPanel:item resizePreview:YES];
            found = YES;
            break;
        }
    }

    if (!found)
        [self switchToPanel:toolbar.items[0] resizePreview:YES];
}

- (void)addObservers {
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(notePreferencesChanged:)
                                                 name:kPreferencesChangedNotification
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(noteManagedObjectContextDidChange:)
                                                 name:NSManagedObjectContextObjectsDidChangeNotification
                                               object:_managedObjectContext];

    [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(noteColorModeChanged:) name:@"AppleInterfaceThemeChangedNotification" object:nil];
}

#pragma mark Update panels

- (void)updatePrefsPanel {
    if (![self prepareThemeForPanel])
        return;

    [self updateSimpleSettings];
    [self updateThemesPane];
    [self updateStylesPane];
    [self updatePresentationPane];
    [self updateFormatPane];
    [self updateVoiceOverPane];
    [self updateMiscPane];
    [self updateGlobalPane];

    if ([NSFontPanel sharedFontPanel].visible) {
        if (!selectedFontButton)
            selectedFontButton = btnBufferFont;
        [self showFontPanel:selectedFontButton];
    }
}

// Picks the theme to show: falls back to the current game's theme, then the
// default theme. Returns NO, after rebuilding the built-in themes, if it has
// no grid colour even after populateStyles. Also repairs values the panes
// cannot show, so that the update<Pane>Pane methods only read the theme.
- (BOOL)prepareThemeForPanel {
    // A deleted theme has no context to create styles in, so populateStyles
    // below would crash on it.
    if (self.theme.isDeleted || !self.theme.managedObjectContext)
        self.theme = nil;
    if (!self.theme) {
        self.theme = _currentGame.theme;
    }
    if (!self.theme)
        self.theme = self.defaultTheme;
    if (!self.theme.gridNormal.attributeDict)
        [self.theme populateStyles];
    if (!self.theme.gridNormal.color) {
        NSLog(@"Preferences prepareThemeForPanel: Something is very wrong");
        [BuiltInThemes createBuiltInThemesInContext:_managedObjectContext forceRebuild:YES];
        return NO;
    }
    if (self.theme.borderColor == nil)
        self.theme.borderColor = self.theme.bufferBackground;
    // Due to a bug in earlier versions, theme.zMachineTerp might be 0.
    if (self.theme.zMachineTerp < 1 || self.theme.zMachineTerp > 11) {
        self.theme.zMachineTerp = 6; // Interpreter 6 means IBM PC.
    }
    return YES;
}

- (void)updateThemesPane {
    _btnOneThemeForAll.state = _oneThemeForAll;
    _btnAdjustSize.state = _adjustSize;

    _darkModeMenuItem.state = self.theme.hardDark;

    _lightModeMenuItem.state = self.theme.hardLight;
}

- (void)updateStylesPane {
    clrGridFg.color = self.theme.gridNormal.color;
    clrGridBg.color = self.theme.gridBackground;
    clrBufferFg.color = self.theme.bufferNormal.color;
    clrBufferBg.color = self.theme.bufferBackground;

    txtGridMargin.floatValue = (float)self.theme.gridMarginX;
    txtBufferMargin.floatValue = (float)self.theme.bufferMarginX;

    btnGridFont.title = fontToString(self.theme.gridNormal.font);
    btnBufferFont.title = fontToString(self.theme.bufferNormal.font);

    btnEnableStyles.state = self.theme.doStyles;

    _btnOverwriteStyles.enabled = self.theme.hasCustomStyles;

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [_windowTypePopup selectItemWithTag:
     [defaults integerForKey:kSelectedGlkWindowTypeKey]];
    [_styleNamePopup selectItemWithTag:
     [defaults integerForKey:kSelectedStyleKey]];

    GlkStyle *selectedStyle = [self selectedStyle];
    clrAnyFg.color = selectedStyle.color;
    btnAnyFont.title = fontToString(selectedStyle.font);
}

- (void)updatePresentationPane {
    txtCols.intValue = self.theme.defaultCols;
    txtRows.intValue = self.theme.defaultRows;

    txtBorder.intValue = self.theme.border;

    btnSpaceFormat.state = (self.theme.spaceFormat == TAG_SPACES_ONE);

    _btnAutoBorderColor.state = self.theme.borderBehavior == kAutomatic ? NSControlStateValueOn : NSControlStateValueOff;
    _borderColorWell.enabled = (self.theme.borderBehavior == kUserOverride);

    _btnUnderlineLinksGrid.state = (self.theme.gridLinkStyle == NSUnderlineStyleNone) ? NSControlStateValueOff : NSControlStateValueOn;
    _btnUnderlineLinksBuffer.state = (self.theme.bufLinkStyle == NSUnderlineStyleNone) ? NSControlStateValueOff : NSControlStateValueOn;

    NSInteger scrollbackLimit = (NSInteger)[GlkTextBufferWindow clampScrollbackLimit:self.theme.scrollbackLimit];
    _scrollbackLimitTextField.integerValue = scrollbackLimit;
    _scrollbackLimitStepper.integerValue = scrollbackLimit;
    _scrollbackUnitLabel.stringValue = (scrollbackLimit == 0) ? NSLocalizedString(@"= unlimited", nil) : NSLocalizedString(@"characters", nil);

    _hardDarkCheckbox.state = self.theme.hardDark;

    _hardLightCheckbox.state = self.theme.hardLight;
}

- (void)updateFormatPane {
    NSString *beepHigh = self.theme.beepHigh;
    NSString *beepLow = self.theme.beepLow;

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

    _zVersionTextField.stringValue = self.theme.zMachineLetter;

    _bZVerticalTextField.integerValue = self.theme.bZAdjustment;
    _bZVerticalStepper.integerValue = self.theme.bZAdjustment;
}

- (void)updateVoiceOverPane {
    _vODelaySlider.doubleValue = self.theme.vOHackDelay;
    _vODelayTextField.doubleValue = self.theme.vOHackDelay;
    _vODelayTextField.enabled = self.theme.vODelayOn;
    _vODelaySlider.enabled = self.theme.vODelayOn;
    _vODelaySlider.accessibilityValueDescription = [self secondsAccessibilityString];
    _vODelayCheckbox.state = self.theme.vODelayOn ? NSControlStateValueOn : NSControlStateValueOff;
}

- (void)updateMiscPane {
    _btnAutosave.state = self.theme.autosave;
    _btnAutosaveOnTimer.enabled = self.theme.autosave ? YES : NO;

    if (self.theme.minTimer != 0) {
        if (_timerSlider.doubleValue != floor(1000.0 / self.theme.minTimer)) {
            _timerSlider.doubleValue = floor(1000.0 / (CGFloat)self.theme.minTimer);
        }
        if (_timerTextField.doubleValue != floor(1000.0 / self.theme.minTimer)) {
            _timerTextField.doubleValue = floor(1000.0 / (CGFloat)self.theme.minTimer);
        }
        _timerSlider.accessibilityValueDescription = [NSString stringWithFormat:NSLocalizedString(@"%ld per second", nil), _timerSlider.integerValue];
    }
}

// Global settings come from NSUserDefaults, not the theme
- (void)updateGlobalPane {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    _recheckMissingCheckbox.state = [defaults boolForKey:kRecheckForMissingKey] ? NSControlStateValueOn : NSControlStateValueOff;
    _recheckFrequencyTextfield.stringValue = [defaults stringForKey:kRecheckFrequencyKey];
    _recheckFrequencyTextfield.enabled = _recheckMissingCheckbox.state ? YES : NO;

    [self updateLibraryOrganiserControls];
}

- (NSString *)secondsAccessibilityString {
    CGFloat fractional = _vODelaySlider.doubleValue - (CGFloat)_vODelaySlider.integerValue;
    if (fractional < 0.05 || fractional > 0.95) {
        return [NSString stringWithFormat:NSLocalizedString(@"%ld seconds", nil), (long)round(_vODelaySlider.doubleValue)];
    } else {
        return [NSString stringWithFormat:NSLocalizedString(@"%.1f seconds", nil), _vODelaySlider.doubleValue];
    }
}

- (void)showOkayCancelAlertWithMessage:(NSString *)message
                                  info:(NSString *)info
                        suppressionKey:(NSString *)suppressionKey
                               handler:(void (^)(BOOL okay))handler {
    NSAlert *anAlert = [NSAlert alertWithMessage:message
                                            info:info
                                         buttons:@[NSLocalizedString(@"Okay", nil), NSLocalizedString(@"Cancel", nil)]];
    [anAlert showSuppressionButtonWithTitle:NSLocalizedString(@"Do not show again.", nil)];
    [anAlert beginSheetModalForWindow:self.window
                   suppressingWithKey:suppressionKey
                    completionHandler:^(NSInteger result) {
        handler(result == NSAlertFirstButtonReturn);
    }];
}

- (GlkStyle *)selectedStyle {
    NSString *styleName = [self selectedStyleName];
    return [self.theme valueForKey:styleName];
}

- (NSString *)selectedStyleName {
    NSUInteger windowType = (NSUInteger)_windowTypePopup.selectedTag;
    NSUInteger styleValue = (NSUInteger)_styleNamePopup.selectedTag;
    return (windowType == wintype_TextGrid) ? gGridStyleNames[styleValue] : gBufferStyleNames[styleValue];
}

- (void)setCurrentGame:(Game *)currentGame {
    _currentGame = currentGame;
    _themesHeader.stringValue = [self themeScopeTitle];
    if (currentGame && currentGame.theme != self.theme && !(_darkOverrideActive || _lightOverrideActive)) {
        [self restoreThemeSelection:currentGame.theme];
    }
}

- (Theme *)defaultTheme {
    if (_defaultTheme == nil) {
        // Go through the accessor, not the ivar: this is the last-resort path
        // used by +currentTheme, and it may run before the nib bindings have
        // populated _managedObjectContext. A nil context here silently returns
        // a nil theme, which is what we are trying to recover from.
        NSManagedObjectContext *context = self.managedObjectContext;
        _defaultTheme = [Fetches findTheme:@"Default" inContext:context];
        if (!_defaultTheme)
            _defaultTheme = [BuiltInThemes createDefaultThemeInContext:context forceRebuild:NO];
    }
    return _defaultTheme;
}

@synthesize coreDataManager = _coreDataManager;

- (CoreDataManager *)coreDataManager {
    if (_coreDataManager == nil) {
        _coreDataManager = ((AppDelegate*)NSApp.delegate).coreDataManager;
    }
    return _coreDataManager;
}

@synthesize managedObjectContext = _managedObjectContext;

- (NSManagedObjectContext *)managedObjectContext {
    if (_managedObjectContext == nil) {
        _managedObjectContext = self.coreDataManager.mainManagedObjectContext;
    }
    return _managedObjectContext;
}

- (IBAction)rebuildDefaultThemes:(id)sender {
    [BuiltInThemes createBuiltInThemesInContext:_managedObjectContext forceRebuild:YES];
}

- (NSArray *)sortDescriptors {
    return @[[NSSortDescriptor sortDescriptorWithKey:@"editable" ascending:YES],
             [NSSortDescriptor sortDescriptorWithKey:@"name" ascending:YES
                                            selector:@selector(localizedStandardCompare:)]];
}

#pragma mark Action menu

- (void)setOneThemeForAll:(BOOL)oneThemeForAll {
    _oneThemeForAll = oneThemeForAll;
    [[NSUserDefaults standardUserDefaults] setBool:_oneThemeForAll forKey:kOneThemeForAllKey];
    _themesHeader.stringValue = [self themeScopeTitle];
    if (oneThemeForAll) {
        _btnOneThemeForAll.state = NSControlStateValueOn;
        self.theme.games = [NSSet setWithArray:[Fetches fetchAllGamesAsFaultsInContext:self.managedObjectContext]];
    } else {
        _btnOneThemeForAll.state = NSControlStateValueOff;
    }
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    SEL action = menuItem.action;

    NSArray *selectedGames = _libcontroller.selectedGames;

    if (action == @selector(applyToSelected:)) {
        if (_oneThemeForAll || selectedGames.count == 0) {
            return NO;
        }
    } else if (action == @selector(selectUsingTheme:)) {
        if ([self themeForMenuCommand:menuItem].games.count == 0 || _oneThemeForAll || _darkOverrideActive || _lightOverrideActive) {
            return NO;
        }
    } else if (action == @selector(deleteUserThemes:)) {
        if ([Fetches fetchUserThemesInContext:self.managedObjectContext].count == 0)
            return NO;
    } else if (action == @selector(editNewEntry:)) {
        return [self themeForMenuCommand:menuItem].editable;

    } else if (action == @selector(useInLightMode:) || action == @selector(useInDarkMode:)) {
        // These act on the selected theme, so in the contextual menu of
        // another row they only show that theme's state.
        Theme *target = [self themeForMenuCommand:menuItem];
        menuItem.state = (action == @selector(useInLightMode:)) ? target.hardLight : target.hardDark;
        return target == self.theme;

    } else if (action == @selector(togglePreview:)) {
        NSString* title = _previewShown ? NSLocalizedString(@"Hide Preview", nil) : NSLocalizedString(@"Show Preview", nil);
        ((NSMenuItem*)menuItem).title = title;
    } else if (action == @selector(clearLightDarkOverrides:)) {
        return (_darkTheme || _lightTheme);
    }

    return YES;
}

- (Theme *)cloneThemeIfNotEditable {
    if (!self.theme.editable) {
        Theme *clonedTheme = self.theme.clone;
        clonedTheme.editable = YES;
        NSString *name = [Theme unusedNameBasedOn:[self.theme.name stringByAppendingString:@" (modified)"]
                                         inContext:self.managedObjectContext];
        clonedTheme.name = name;
        [self changeThemeName:name];
        _btnRemove.enabled = YES;
        self.theme = clonedTheme;
        pendingSelectionTheme = clonedTheme;
        [self performSelector:@selector(restoreThemeSelection:) withObject:clonedTheme afterDelay:0.1];
        return clonedTheme;
    }
    return self.theme;
}

- (void)windowWillClose:(id)sender {
    if ([NSFontPanel sharedFontPanel].visible)
        [[NSFontPanel sharedFontPanel] orderOut:self];
    if ([NSColorPanel sharedColorPanel].visible)
        [[NSColorPanel sharedColorPanel] orderOut:self];
}

@synthesize dummyTextView = _dummyTextView;

// Created lazily: receives font panel changes on behalf of the Preferences window
- (DummyTextView *)dummyTextView {
    if (!_dummyTextView) {
        _dummyTextView = [[DummyTextView alloc] initWithFrame:NSMakeRect(0,0,200,500)];
    }
    return _dummyTextView;
}

@end
