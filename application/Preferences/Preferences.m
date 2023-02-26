#import "Preferences.h"
#import "AppDelegate.h"

#import "CoreDataManager.h"
#import "Game.h"
#import "Metadata.h"
#import "Theme.h"
#import "ThemeArrayController.h"
#import "GlkStyle.h"
#import "TableViewController.h"
#import "NSString+Categories.h"
#import "NSColor+integer.h"
#import "BufferTextView.h"
#import "Constants.h"
#import "BuiltInThemes.h"
#import "ParagraphPopOver.h"
#import "NotificationBezel.h"
#import "PreviewController.h"

#include "glk.h"

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
    [super changeFont:fontManager];
    _sender = fontManager;
    [[Preferences instance] changeFont:self];
}

- (void)changeAttributes:(id)sender {
    [super changeAttributes:sender];
    _sender = sender;
    [[Preferences instance] changeAttributes:self];
}

- (void)changeDocumentBackgroundColor:(id)sender {
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

    NSButton *selectedFontButton;

    BOOL disregardTableSelection;
    BOOL zooming;
    CGFloat previewTextHeight;
    CGFloat defaultWindowHeight;

    NSDictionary *catalinaSoundsToBigSur;
    NSDictionary *bigSurSoundsToCatalina;
}

@property (strong) IBOutlet PreviewController *previewController;
@property (strong) IBOutlet NSLayoutConstraint *previewHeight;

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

    // Rebuild default themes the first time a new Spatterlight version is run (or the preferences are deleted.)
    // Rebuilding is so quick that this may be overkill. Perhaps we should just rebuild on every run?
    BOOL forceRebuild = NO;
    NSString *appBuildString = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"];

    NSString *lastThemesRebuild = [defaults objectForKey:@"LastThemesRebuild"];

    if (![lastThemesRebuild isEqualToString:appBuildString]) {
        forceRebuild = YES;
        [defaults setObject:appBuildString forKey:@"LastThemesRebuild"];
    }

    // We may or may not have created the Default and Old themes already above.
    // Then these won't be recreated below.
    [BuiltInThemes createBuiltInThemesInContext:managedObjectContext forceRebuild:forceRebuild];
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
    if (!theme)
        theme = [Preferences instance].defaultTheme;
    return theme;
}

+ (Preferences *)instance {
    return prefs;
}


#pragma mark GlkStyle and attributed-string magic

+ (void)rebuildTextAttributes {
    [theme populateStyles];
    NSSize cellsize = theme.gridNormal.cellSize;
    theme.cellWidth = cellsize.width;
    theme.cellHeight = cellsize.height;
    cellsize = theme.bufferNormal.cellSize;
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
    [super windowDidLoad];

    self.window.delegate = self;

    disregardTableSelection = YES;

    NSRect winRect = [self.window frameRectForContentRect:NSMakeRect(0, 0,  kDefaultPrefWindowWidth, kDefaultPrefsLowerViewHeight)];

    defaultWindowHeight = NSHeight(winRect);

    if (self.window.minSize.height != defaultWindowHeight || self.window.minSize.width != kDefaultPrefWindowWidth) {
        NSSize minSize = self.window.minSize;
        minSize.height = defaultWindowHeight;
        minSize.width = kDefaultPrefWindowWidth;
        self.window.minSize = minSize;
    }

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    _previewShown = [defaults boolForKey:@"ShowThemePreview"];

    if (!_previewShown) {
        [self resizeWindowToHeight:defaultWindowHeight];
    } else {
        CGFloat restoredHeight = NSHeight(self.window.frame);

        // Hack to fix weird bug where a sliver of the preview window
        // keeps showing on restart
        if (restoredHeight < defaultWindowHeight + 10) {
            [self togglePreview:nil];
        } else {
            if (restoredHeight <= defaultWindowHeight)
                [self resizeWindowToHeight:[self calculatePreviewHeight]];
        }
    }

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

    _previewController.theme = theme;

    // Sample text view
    CGFloat sampleY = kDefaultPrefsLowerViewHeight + 1;
    NSRect newSampleFrame = NSMakeRect(0, sampleY, self.window.frame.size.width, ((NSView *)self.window.contentView).frame.size.height - sampleY);
    _sampleTextBorderView.frame = newSampleFrame;

    _divider.frame = NSMakeRect(0, kDefaultPrefsLowerViewHeight, self.window.frame.size.width, 1);
    _divider.autoresizingMask = NSViewMaxYMargin;

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(notePreferencesChanged:)
                                                 name:@"PreferencesChanged"
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(noteManagedObjectContextDidChange:)
                                                 name:NSManagedObjectContextObjectsDidChangeNotification
                                               object:_managedObjectContext];

    _oneThemeForAll = [defaults boolForKey:@"OneThemeForAll"];
    _themesHeader.stringValue = [self themeScopeTitle];

    _adjustSize = [defaults boolForKey:@"AdjustSize"];

    prefs = self;
    [self updatePrefsPanel];

    NSScrollView *scrollView = _scrollView;

    scrollView.scrollerStyle = NSScrollerStyleOverlay;
    scrollView.drawsBackground = YES;
    scrollView.hasHorizontalScroller = NO;
    scrollView.hasVerticalScroller = YES;
    scrollView.verticalScroller.alphaValue = 100;
    scrollView.autohidesScrollers = YES;
    scrollView.borderType = NSNoBorder;

    [self changeThemeName:theme.name];

    self.windowFrameAutosaveName = @"PrefsPanel";
    themesTableView.autosaveName = @"ThemesTable";

    [self performSelector:@selector(restoreThemeSelection:) withObject:theme afterDelay:0.1];
}

#pragma mark Update panels

- (void)updatePrefsPanel {
    if (!theme) {
        theme = _currentGame.theme;
    }
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

    _btnAutoBorderColor.state = theme.borderBehavior == kAutomatic ? NSOnState : NSOffState;
    _borderColorWell.enabled = (theme.borderBehavior == kUserOverride);
    if (theme.borderColor == nil)
        theme.borderColor = theme.bufferBackground;

    wint_t windowType = (wint_t)[defaults integerForKey:@"SelectedHyperlinkWindowType"];
    if (windowType != wintype_TextGrid && windowType != wintype_TextBuffer) {
        windowType = wintype_TextGrid;
        [defaults setInteger:windowType forKey:@"SelectedHyperlinkWindowType"];
    }

    [_hyperlinksPopup selectItemWithTag:windowType];

    switch (windowType) {
        case wintype_TextGrid:
            _btnUnderlineLinks.state = (theme.gridLinkStyle == NSUnderlineStyleNone) ? NSOffState : NSOnState;
            break;
        case wintype_TextBuffer:
            _btnUnderlineLinks.state = (theme.bufLinkStyle == NSUnderlineStyleNone) ? NSOffState : NSOnState;
            break;
        default:
            NSLog(@"Unhandled link window type");
            break;
    }

    _btnVOSpeakCommands.state = theme.vOSpeakCommand;
    [_vOMenuButton selectItemWithTag:theme.vOSpeakMenu];
    [_vOImagesButton selectItemWithTag:theme.vOSpeakImages];

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
    [_bZArrowsMenu selectItemWithTag:theme.bZTerminator];

    _zVersionTextField.stringValue = theme.zMachineLetter;

    _bZVerticalTextField.integerValue = theme.bZAdjustment;
    _bZVerticalStepper.integerValue = theme.bZAdjustment;

    _btnSmoothScroll.state = theme.smoothScroll;
    _btnAutosave.state = theme.autosave;
    _btnAutosaveOnTimer.state = theme.autosaveOnTimer;
    _btnAutosaveOnTimer.enabled = theme.autosave ? YES : NO;
    [_errorHandlingPopup selectItemWithTag:theme.errorHandling];

    _btnDeterminism.state = theme.determinism;
    _btnNoHacks.state = theme.nohacks ? NSOffState : NSOnState;

    [_coverImagePopup selectItemWithTag:theme.coverArtStyle];

    [_imageReplacePopup selectItemWithTag:[defaults integerForKey:@"ImageReplacement"]];

    _btnShowBezels.state = [defaults boolForKey:@"ShowBezels"] ? NSOnState : NSOffState;

    if (theme.minTimer != 0) {
        if (_timerSlider.integerValue != 1000.0 / theme.minTimer) {
            _timerSlider.integerValue = (long)(1000.0 / theme.minTimer);
        }
        if (_timerTextField.integerValue != (1000.0 / theme.minTimer)) {
            _timerTextField.integerValue = (long)(1000.0 / theme.minTimer);
        }
    }

    _libraryAtStartCheckbox.state = [defaults boolForKey:@"ShowLibrary"] ? NSOnState : NSOffState;
    _addToLibraryCheckbox.state = [defaults boolForKey:@"AddToLibrary"] ? NSOnState : NSOffState;
    _recheckMissingCheckbox.state = [defaults boolForKey:@"RecheckForMissing"] ? NSOnState : NSOffState;
    _recheckFrequencyTextfield.stringValue = [defaults stringForKey:@"RecheckFrequency"];
    _recheckFrequencyTextfield.enabled = _recheckMissingCheckbox.state ? YES : NO;

    [_palettePopup selectItemWithTag:theme.sAPalette];
    [_inventoryPopup selectItemWithTag:theme.sAInventory];
    _delaysCheckbox.state = theme.sADelays;
    _slowDrawCheckbox.state = theme.slowDrawing;

    if ([NSFontPanel sharedFontPanel].visible) {
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
    if (currentGame.theme != theme) {
        [self restoreThemeSelection:currentGame.theme];
    }
}

- (Game *)currentGame {
    return _currentGame;
}

@synthesize defaultTheme = _defaultTheme;

- (Theme *)defaultTheme {
    if (_defaultTheme == nil) {
        NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
        fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:self.managedObjectContext];
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
        _managedObjectContext = self.coreDataManager.mainManagedObjectContext;
    }
    return _managedObjectContext;
}

- (IBAction)rebuildDefaultThemes:(id)sender {
    [BuiltInThemes createBuiltInThemesInContext:_managedObjectContext forceRebuild:YES];
}

#pragma mark Preview

- (void)notePreferencesChanged:(NSNotification *)notify {
    // Change the theme of the sample text field
    [self.coreDataManager saveChanges];
}

- (NSSize)windowWillResize:(NSWindow *)window
                    toSize:(NSSize)frameSize {

    if (window != self.window) {
        return frameSize;
    }

    if (zooming) {
        zooming = NO;
        return frameSize;
    }

    if (frameSize.height <= defaultWindowHeight) {
        _previewShown = NO;
    } else {
        if (frameSize.height > self.window.frame.size.height) { // We are enlarging
            CGFloat maxHeight = [self calculatePreviewHeight] + 40;
            if (frameSize.height > maxHeight)
                frameSize.height = maxHeight;
        }
        _previewShown = YES;
    }

    [[NSUserDefaults standardUserDefaults] setBool:_previewShown forKey:@"ShowThemePreview"];

    return frameSize;
}

- (NSRect)windowWillUseStandardFrame:(NSWindow *)window
                        defaultFrame:(NSRect)newFrame {

    if (window != self.window)
        return newFrame;

    CGFloat newHeight;

    if (!_previewShown) {
        newHeight = defaultWindowHeight;
        zooming = YES;
    } else {
        newHeight = [self calculatePreviewHeight];
        _previewHeight.constant = newHeight;
    }

    NSRect currentFrame = window.frame;

    CGFloat diff = currentFrame.size.height - newHeight;
    currentFrame.origin.y += diff;
    currentFrame.size.height = newHeight;

    return currentFrame;
}

- (BOOL)windowShouldZoom:(NSWindow *)window toFrame:(NSRect)newFrame {
    if (window != self.window)
        return YES;
    if (!_previewShown && newFrame.size.height > defaultWindowHeight)
        return NO;

    return YES;
}

- (void)resizeWindowToHeight:(CGFloat)height {
    NSWindow *prefsPanel = self.window;

    CGFloat oldheight = NSHeight(prefsPanel.frame);

    if (ceil(height) == ceil(oldheight)) {
        return;
    }

    CGRect screenframe = prefsPanel.screen.visibleFrame;

    CGRect winrect = prefsPanel.frame;

    winrect.size.height = height;
    winrect.size.width = kDefaultPrefWindowWidth;

    // If the entire text does not fit on screen, don't change height at all
    if (winrect.size.height > screenframe.size.height)
        winrect.size.height = oldheight;

    CGFloat offset = winrect.size.height - oldheight;
    winrect.origin.y -= offset;

    // If window is partly off the screen, move it (just) inside
    if (NSMaxX(winrect) > NSMaxX(screenframe))
        winrect.origin.x = NSMaxX(screenframe) - winrect.size.width;

    if (NSMinY(winrect) < 0)
        winrect.origin.y = NSMinY(screenframe);

    Preferences * __weak weakSelf = self;

    CGFloat blockDefaultWindowHeight = defaultWindowHeight;
    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = 0.3;
         [[prefsPanel animator]
          setFrame:winrect
          display:YES];
        [_previewHeight.animator setConstant:NSHeight(weakSelf.window.frame) - blockDefaultWindowHeight];
        [_previewController.textHeight.animator setConstant:MIN(NSHeight(weakSelf.window.frame) - blockDefaultWindowHeight, [_previewController calculateHeight])];
     } completionHandler:^{}];
}

- (CGFloat)calculatePreviewHeight {

    CGFloat proposedHeight = [_previewController calculateHeight];

    CGFloat totalHeight = defaultWindowHeight + proposedHeight + 40; //2 * (theme.border + theme.bufferMarginY);
    CGRect screenframe = [NSScreen mainScreen].visibleFrame;

    if (totalHeight > screenframe.size.height) {
        totalHeight = screenframe.size.height;
    }
    return totalHeight;
}

- (void)noteManagedObjectContextDidChange:(NSNotification *)notify {
    NSSet *updatedObjects = (notify.userInfo)[NSUpdatedObjectsKey];
    NSSet *insertedObjects = (notify.userInfo)[NSInsertedObjectsKey];
    NSSet *refreshedObjects = (notify.userInfo)[NSRefreshedObjectsKey];
    NSSet *deletedObjects = (notify.userInfo)[NSDeletedObjectsKey];

    if (!updatedObjects)
        updatedObjects = [NSSet new];

    updatedObjects = [updatedObjects setByAddingObjectsFromSet:insertedObjects];
    updatedObjects = [updatedObjects setByAddingObjectsFromSet:refreshedObjects];
    updatedObjects = [updatedObjects setByAddingObjectsFromSet:deletedObjects];

    if ([updatedObjects containsObject:theme]) {
        Preferences * __weak weakSelf = self;

        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf updatePrefsPanel];
            [[NSNotificationCenter defaultCenter]
             postNotification:[NSNotification notificationWithName:@"PreferencesChanged" object:theme]];
        });
    }
}

#pragma mark Themes Table View Magic

- (void)restoreThemeSelection:(id)sender {
    ThemeArrayController *arrayController = _arrayController;
    if (arrayController.selectedTheme == sender) {
        return;
    }
    NSArray *themes = arrayController.arrangedObjects;
    theme = sender;
    if (![themes containsObject:sender]) {
        theme = themes.lastObject;
        return;
    }
    NSUInteger row = [themes indexOfObject:theme];

    disregardTableSelection = NO;

    arrayController.selectionIndex = row;
    themesTableView.allowsEmptySelection = NO;
    [themesTableView scrollRowToVisible:(NSInteger)row];
}

- (void)tableViewSelectionDidChange:(id)notification {
    NSTableView *tableView = [notification object];
    if (tableView == themesTableView) {
        if (disregardTableSelection == YES) {
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
    NSString *themeString = [NSString stringWithFormat:@"Settings for theme %@", name];
    _detailsHeader.stringValue = themeString;
    _miscHeader.stringValue = themeString;
    _stylesHeader.stringValue = themeString;
    _zcodeHeader.stringValue = themeString;
    _vOHeader.stringValue = themeString;
}

- (BOOL)notDuplicate:(NSString *)string {
    ThemeArrayController *arrayController = _arrayController;
    NSArray *themes = arrayController.arrangedObjects;
    for (Theme *aTheme in themes) {
        if ([aTheme.name isEqualToString:string] && [themes indexOfObject:aTheme] != [themes indexOfObject:arrayController.selectedTheme])
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
        if (![[NSUserDefaults standardUserDefaults] boolForKey:@"UseForAllAlertSuppression"]) {
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
    [NSString stringWithFormat:@"%@ %@ individual theme settings.", [NSString stringWithSummaryOfGames:games], (games.count == 1) ? @"has" : @"have"];
    anAlert.informativeText = [NSString stringWithFormat:@"Would you like to use theme %@ for all games?", theme.name];
    anAlert.showsSuppressionButton = YES;
    anAlert.suppressionButton.title = NSLocalizedString(@"Do not show again.", nil);
    [anAlert addButtonWithTitle:NSLocalizedString(@"Okay", nil)];
    [anAlert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

    Preferences * __weak weakSelf = self;
    [anAlert beginSheetModalForWindow:self.window completionHandler:^(NSInteger result){

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
    if (_currentGame == nil) {
        return NSLocalizedString(@"No game is currently running", nil);
    } else {
        if (!_currentGame.metadata.title.length)
            _currentGame.metadata.title = _currentGame.path.lastPathComponent;
        if (!_currentGame.metadata.title.length)
            return NSLocalizedString(@"Error: Current game has no metadata!", nil);
        return [NSLocalizedString(@"Theme setting for game ", nil) stringByAppendingString:_currentGame.metadata.title];
    }
}

- (IBAction)changeAdjustSize:(id)sender {
    _adjustSize = (BOOL)[sender state];
    [[NSUserDefaults standardUserDefaults] setBool:_adjustSize forKey:@"AdjustSize"];
}

- (IBAction)addTheme:(id)sender {
    ThemeArrayController *arrayController = _arrayController;
    NSUInteger row = arrayController.selectionIndex;
    if (row == NSNotFound)
        row = [arrayController.arrangedObjects count] - 1;
    if (row >= [arrayController.arrangedObjects count])
        row = 0;
    NSTableCellView *cellView = (NSTableCellView*)[themesTableView viewAtColumn:0 row:(NSInteger)row makeIfNecessary:YES];
    if ([self notDuplicate:cellView.textField.stringValue]) {
        // For some reason, tableViewSelectionDidChange will be called twice here,
        // so we disregard the first call
        disregardTableSelection = YES;
        [arrayController add:sender];
        [self performSelector:@selector(editNewEntry:) withObject:nil afterDelay:0.1];
    } else NSBeep();
}

- (IBAction)removeTheme:(id)sender {
    ThemeArrayController *arrayController = _arrayController;
    Theme *themeToRemove = arrayController.selectedTheme;
    if (!themeToRemove.editable) {
        NSBeep();
        return;
    }
    Theme *ancestor = themeToRemove.defaultParent;
    if (!ancestor)
        ancestor = [self findAncestorThemeOf:themeToRemove];
    NSSet *orphanedGames = themeToRemove.games;
    NSSet *orphanedThemes = themeToRemove.defaultChild;
    NSUInteger row = arrayController.selectionIndex - 1;
    if (row >= [arrayController.arrangedObjects count])
        row = 0;
    [arrayController remove:sender];
    arrayController.selectionIndex = row;
    if (!ancestor)
        ancestor = arrayController.selectedTheme;

    [ancestor addGames:orphanedGames];
    [ancestor addDefaultChild:orphanedThemes];
}

- (IBAction)applyToSelected:(id)sender {
    [theme addGames:[NSSet setWithArray:_libcontroller.selectedGames]];
}

- (IBAction)selectUsingTheme:(id)sender {
    [_libcontroller selectGames:theme.games];
}

- (IBAction)deleteUserThemes:(id)sender {
    ThemeArrayController *arrayController = _arrayController;
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
        if (t.games.count || t.defaultChild.count) {
            Theme *ancestor = [self findAncestorThemeOf:t];
            if (ancestor && !ancestor.editable) {
                [ancestor addGames:t.games];
                [ancestor addDefaultChild:t.defaultChild];
                if (t == theme) {
                    NSUInteger row = [arrayController.arrangedObjects indexOfObject:t];
                    arrayController.selectionIndex = row;
                }
            }
            [orphanedGames unionSet:t.games];
        }
    }

    [arrayController removeObjects:fetchedObjects];

    [theme addGames:orphanedGames];
    arrayController.selectedObjects = @[theme];
}

- (nullable Theme *)findAncestorThemeOf:(Theme *)t {
    NSRange modifiedRange = [t.name rangeOfString:@" (modified)"];
    NSString *baseName = @"";
    if (modifiedRange.location != NSNotFound && modifiedRange.location > 1) {
        baseName = [t.name substringToIndex:modifiedRange.location];
        Theme *newTheme = [_arrayController findThemeByName:baseName];
        if (newTheme != nil) {
            return newTheme;
        }
    }
    if (t.defaultParent != nil) {
        Theme *t2 = t;
        while (t2.defaultParent != nil) {
            t2 = t2.defaultParent;
        }
        return t2;
    }
    NSLog(@"Found no ancestor theme!");
    return nil;
}

- (IBAction)togglePreview:(id)sender {
    if (_previewShown) {
        [self resizeWindowToHeight:defaultWindowHeight];
        _previewShown = NO;
    } else {
        _previewShown = YES;
        [self resizeWindowToHeight:[self calculatePreviewHeight]];
    }
    [[NSUserDefaults standardUserDefaults] setBool:_previewShown forKey:@"ShowThemePreview"];
}

- (IBAction)editNewEntry:(id)sender {
    ThemeArrayController *arrayController = _arrayController;
    NSUInteger row = arrayController.selectionIndex;
    if (row == NSNotFound)
        row = [arrayController.arrangedObjects count] - 1;
    if (row >= [arrayController.arrangedObjects count])
        row = 0;

    NSTableCellView* cellView = (NSTableCellView*)[themesTableView viewAtColumn:0 row:(NSInteger)row makeIfNecessary:YES];
    if ((cellView.textField).acceptsFirstResponder) {
        [cellView.window makeFirstResponder:cellView.textField];
        [themesTableView scrollRowToVisible:(NSInteger)row];
    }
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    SEL action = menuItem.action;

    NSArray *selectedGames = _libcontroller.selectedGames;

    if (action == @selector(applyToSelected:)) {
        if (_oneThemeForAll || selectedGames.count == 0) {
            return NO;
        } else {
            for (Game *game in selectedGames) {
                if (game.theme != theme)
                    return YES;
            }
            return NO;
        }
    }

    if (action == @selector(selectUsingTheme:)) {
        if (theme.games.count == 0)
            return NO;
        for (Game *game in theme.games) {
            if ([selectedGames indexOfObject:game] == NSNotFound)
                return YES;
        }
        return NO;
    }

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
    Theme *themeToChange = nil;
    if (sender == txtCols) {
        if (theme.defaultCols == [sender intValue])
            return;
        themeToChange = [self cloneThemeIfNotEditable];
        themeToChange.defaultCols  = [sender intValue];
        if (themeToChange.defaultCols  < 5)
            themeToChange.defaultCols  = 5;
        if (themeToChange.defaultCols  > 200)
            themeToChange.defaultCols  = 200;
        txtCols.intValue = themeToChange.defaultCols ;
    }
    if (sender == txtRows) {
        if (theme.defaultRows == [sender intValue])
            return;
        themeToChange = [self cloneThemeIfNotEditable];
        themeToChange.defaultRows  = [sender intValue];
        if (themeToChange.defaultRows  < 5)
            themeToChange.defaultRows  = 5;
        if (themeToChange.defaultRows  > 200)
            themeToChange.defaultRows  = 200;
        txtRows.intValue = themeToChange.defaultRows ;
    }
    /* send notification that default size has changed -- resize all windows */
    NSNotification *notification = [NSNotification notificationWithName:@"DefaultSizeChanged" object:themeToChange];
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
        else if (_borderColorWell.active)
            sender = _borderColorWell;
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
    } else if (sender == _borderColorWell) {
        [self changeBorderColor:color];
        return;
    } else return;

    GlkStyle *style = nil;
    if (key) {
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
         postNotification:[NSNotification notificationWithName:@"PreferencesChanged" object:themeToChange]];
}

- (void)changeBorderColor:(NSColor *)color {
    if ([color isEqualToColor:theme.borderColor])
        return;

    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.borderColor = color;
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
    int32_t val = 0;
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
    NSInteger windowType = _windowTypePopup.selectedTag;
    [defaults setInteger:windowType forKey:@"SelectedGlkWindowType"];
    [defaults setInteger:_styleNamePopup.selectedTag forKey:@"SelectedStyle"];
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

- (IBAction)changeHyperlinkPopup:(id)sender {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSInteger windowType = _hyperlinksPopup.selectedTag;
    if ([defaults integerForKey:@"SelectedHyperlinkWindowType"] == windowType)
        return;
    [defaults setInteger:windowType forKey:@"SelectedHyperlinkWindowType"];
    switch (windowType) {
        case wintype_TextGrid:
            _btnUnderlineLinks.state = (theme.gridLinkStyle == NSUnderlineStyleNone) ? NSOffState : NSOnState;
            break;
        case wintype_TextBuffer:
            _btnUnderlineLinks.state = (theme.bufLinkStyle == NSUnderlineStyleNone) ? NSOffState : NSOnState;
            break;
        default:
            NSLog(@"Unhandled hyperlink window type");
            break;
    }
}

- (IBAction)changeUnderlineLinks:(id)sender {
    NSInteger windowType = _hyperlinksPopup.selectedTag;
    Theme *themeToChange;
    NSUnderlineStyle selectedStyle = (_btnUnderlineLinks.state == NSOnState) ? NSUnderlineStyleSingle : NSUnderlineStyleNone;
    switch (windowType) {
        case wintype_TextGrid:
            if (theme.gridLinkStyle == selectedStyle)
                return;
            themeToChange = [self cloneThemeIfNotEditable];
            themeToChange.gridLinkStyle = (int32_t)selectedStyle;
            break;
        case wintype_TextBuffer:
            if (theme.bufLinkStyle == selectedStyle)
                return;
            themeToChange = [self cloneThemeIfNotEditable];
            themeToChange.bufLinkStyle = (int32_t)selectedStyle;
            break;
        default:
            NSLog(@"Unhandled hyperlink window type");
            break;
    }
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
    themeToChange.gridMarginX = [sender intValue];
    _marginHorizontalGridTextField.integerValue = themeToChange.gridMarginX;
    _marginHorizontalGridStepper.integerValue = themeToChange.gridMarginX;
}

- (IBAction)changeGridVerticalMargin:(id)sender {
    if (theme.gridMarginY == [sender integerValue])
        return;
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.gridMarginY = [sender intValue];
    _marginVerticalGridTextField.integerValue = themeToChange.gridMarginY;
    _marginVerticalGridStepper.integerValue = themeToChange.gridMarginY;
}

- (IBAction)changeBufferHorizontalMargin:(id)sender {
    if (theme.bufferMarginX == [sender integerValue])
        return;
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.bufferMarginX = [sender intValue];
    _marginHorizontalBufferTextField.integerValue = themeToChange.bufferMarginX;
    _marginHorizontalBufferStepper.integerValue = themeToChange.bufferMarginX;
}
- (IBAction)changeBufferVerticalMargin:(id)sender {
    if (theme.bufferMarginY == [sender integerValue])
        return;
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.bufferMarginY = [sender intValue];
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
    themeToChange.bZAdjustment = [sender intValue];
    _bZVerticalStepper.integerValue = themeToChange.bZAdjustment;
    _bZVerticalTextField.integerValue = themeToChange.bZAdjustment;
}

- (IBAction)changeQuoteBoxCheckBox:(id)sender {
    [self changeBooleanAttribute:@"quoteBox" fromButton:sender];
}

#pragma mark Scott Adams menu

- (IBAction)changeScottAdamsPalette:(id)sender {
    [self changeMenuAttribute:@"sAPalette" fromPopUp:sender];
}

- (IBAction)changeScottAdamsInventory:(id)sender {
    [self changeMenuAttribute:@"sAInventory" fromPopUp:sender];
}

- (IBAction)changeScottAdamsDelay:(id)sender {
    [self changeBooleanAttribute:@"sADelays" fromButton:sender];
}

- (IBAction)changeScottAdamsSlowDraw:(id)sender {
    [self changeBooleanAttribute:@"slowDrawing" fromButton:sender];
}

#pragma mark Misc menu

- (IBAction)resetDialogs:(NSButton *)sender {
    NSUInteger counter = 0;
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSSet *keys = [[NSSet alloc] initWithObjects:@"TerminationAlertSuppression", @"UseForAllAlertSuppression", @"OverwriteStylesAlertSuppression", @"AutorestoreAlertSuppression", @"CloseAlertSuppression", @"CommandScriptAlertSuppression", @"SaveFileAlertSuppression", @"ImageComparisonSuppression", @"VerifyAlertSuppression", nil];

    for (NSString *key in keys) {
        if ([defaults boolForKey:key])
            counter++;
        [defaults setBool:NO forKey:key];
    }

    NotificationBezel *notification = [[NotificationBezel alloc] initWithScreen:self.window.screen];
    [notification showStandardWithText:[NSString stringWithFormat:@"%ld alert%@ reset", counter, counter == 1 ? @"" : @"s"]];
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

- (IBAction)changeShowCoverImage:(id)sender {
    [self changeMenuAttribute:@"coverArtStyle" fromPopUp:sender];
}

#pragma mark Global menu

- (IBAction)changeShowBezel:(id)sender {
    [[NSUserDefaults standardUserDefaults] setBool:([sender state] == NSOnState) forKey:@"ShowBezels"];
}

- (IBAction)changeImageReplacePopup:(NSPopUpButton *)sender {
    [[NSUserDefaults standardUserDefaults] setInteger:sender.selectedTag forKey:@"ImageReplacement"];
}

- (IBAction)changeShowLibrary:(id)sender {
    [[NSUserDefaults standardUserDefaults] setBool:([sender state] == NSOnState) forKey:@"ShowLibrary"];
}

- (IBAction)changeAddToLibrary:(id)sender {
    [[NSUserDefaults standardUserDefaults] setBool:([sender state] == NSOnState) forKey:@"AddToLibrary"];
}

- (IBAction)changeCheckMissing:(id)sender {
    [[NSUserDefaults standardUserDefaults] setBool:([sender state] == NSOnState) forKey:@"RecheckForMissing"];
    _recheckFrequencyTextfield.enabled = ([sender state] == NSOnState);
    if (_recheckMissingCheckbox.state == NSOnState) {
        [_libcontroller startVerifyTimer];
    } else {
        [_libcontroller stopVerifyTimer];
    }
}

- (IBAction)changeCheckFrequency:(id)sender {
    [[NSUserDefaults standardUserDefaults] setInteger:[sender intValue] forKey:@"RecheckFrequency"];
    _recheckFrequencyTextfield.integerValue = (NSInteger)round([sender floatValue]);
}

#pragma mark End of Global menu

- (IBAction)changeOverwriteStyles:(id)sender {
    if ([sender state] == NSOnState) {
        if (![[NSUserDefaults standardUserDefaults] boolForKey:@"OverwriteStylesAlertSuppression"]) {
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

    Preferences * __weak weakSelf = self;

    [anAlert beginSheetModalForWindow:self.window completionHandler:^(NSInteger result) {

        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

        NSString *alertSuppressionKey = @"OverwriteStylesAlertSuppression";

        if (anAlert.suppressionButton.state == NSOnState) {
            // Suppress this alert from now on
            [defaults setBool:YES forKey:alertSuppressionKey];
        }

        if (result == NSAlertFirstButtonReturn) {
            [weakSelf overWriteStyles];
        }
    }];
}

- (void)overWriteStyles {
    theme = [self cloneThemeIfNotEditable];
    NSUInteger counter = 0;
    for (GlkStyle *style in theme.allStyles) {
        if (!style.autogenerated) {
            counter++;
            style.autogenerated = YES;
        }
    }
    if (counter) {
        [theme populateStyles];
        [Preferences rebuildTextAttributes];
    }
    NotificationBezel *notification = [[NotificationBezel alloc] initWithScreen:self.window.screen];
    [notification showStandardWithText:[NSString stringWithFormat:@"%ld style%@ changed", counter, counter == 1 ? @"" : @"s"]];
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
- (IBAction)changeAutomaticBorderColor:(id)sender {
    if (theme.borderBehavior == ([sender state] == NSOffState))
        return;
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.borderBehavior = ([sender state] == NSOffState);
    if (themeToChange.borderBehavior == kUserOverride) {
        NSColorWell *colorWell = _borderColorWell;
        themeToChange.borderColor = colorWell.color;
        if ([NSColorPanel sharedColorPanel].visible)
            [colorWell activate:YES];
    }
}

- (Theme *)cloneThemeIfNotEditable {
    if (!theme.editable) {
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
        disregardTableSelection = YES;
        [self performSelector:@selector(restoreThemeSelection:) withObject:clonedTheme afterDelay:0.1];
        return clonedTheme;
    }
    return theme;
}

#pragma mark Zoom

+ (void)zoomIn {
    zoomDirection = ZOOMRESET;
    NSFont *gridroman = theme.gridNormal.font;
    if (gridroman.pointSize < 200) {
        zoomDirection = ZOOMIN;
        [self scale:(gridroman.pointSize + 1) / gridroman.pointSize];
    }
}

+ (void)zoomOut {
    zoomDirection = ZOOMRESET;
    NSFont *gridroman = theme.gridNormal.font;
    if (gridroman.pointSize > 6) {
        zoomDirection = ZOOMOUT;
        [self scale:(gridroman.pointSize - 1) / gridroman.pointSize];
    }
}

+ (void)zoomToActualSize {
    zoomDirection = ZOOMRESET;

    CGFloat scale = 12;
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
        NSMutableDictionary *dict = style.attributeDict.mutableCopy;
        if (dict[NSKernAttributeName]) {
            CGFloat newValue = ((NSNumber *)dict[NSKernAttributeName]).doubleValue * scalefactor;
            dict[NSKernAttributeName] = @(newValue);
        }
        if (dict[NSBaselineOffsetAttributeName]) {
            CGFloat newValue = ((NSNumber *)dict[NSBaselineOffsetAttributeName]).doubleValue * scalefactor;
            dict[NSBaselineOffsetAttributeName] = @(newValue);
        }
        NSMutableParagraphStyle *para = ((NSParagraphStyle *)dict[NSParagraphStyleAttributeName]).mutableCopy;
        para.lineSpacing = para.lineSpacing * scalefactor;
        para.paragraphSpacing = para.paragraphSpacing * scalefactor;
        para.paragraphSpacingBefore = para.paragraphSpacingBefore * scalefactor;
        para.headIndent = para.headIndent * scalefactor;
        para.tailIndent = para.tailIndent * scalefactor;
        para.firstLineHeadIndent = para.firstLineHeadIndent * scalefactor;
        para.maximumLineHeight = para.maximumLineHeight * scalefactor;
        para.minimumLineHeight = para.minimumLineHeight * scalefactor;

        dict[NSParagraphStyleAttributeName] = para;

        style.attributeDict = dict;
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

//    [self.window makeFirstResponder:self.window];

    [self.dummyTextView updateTextWithAttributes:selectedStyle.attributeDict];

    NSFontPanel *fontPanel = [NSFontPanel sharedFontPanel];
    if (fontPanel.delegate != self.dummyTextView || !fontPanel.visible) {
        fontPanel.delegate = self.dummyTextView;
        [fontPanel makeKeyAndOrderFront:self];
    }
    NSLog(@"fontPanel visible: %@", fontPanel.visible ? @"YES" : @"NO");
    NSLog(@"fontPanel exists: %@", [NSFontPanel sharedFontPanelExists] ? @"YES" : @"NO");
    NSLog(@"fontPanel.delegate == self.dummyTextView: %@", fontPanel.delegate == self.dummyTextView ? @"YES" : @"NO");

    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    fontManager.target = self.dummyTextView;
    [fontManager setSelectedFont:selectedFont isMultiple:NO];
    [fontManager setSelectedAttributes:attr isMultiple:NO];
}


- (IBAction)changeFont:(id)fontManager {
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
    if ([NSFontPanel sharedFontPanel].visible)
        [[NSFontPanel sharedFontPanel] orderOut:self];
    if ([NSColorPanel sharedColorPanel].visible)
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
