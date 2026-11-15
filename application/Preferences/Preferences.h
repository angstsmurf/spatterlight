/*
 * Preferences is a combined singleton / window controller.
 */

#import <AppKit/AppKit.h>

typedef NS_ENUM(NSUInteger, kZoomDirectionType) {
    ZOOMRESET,
    ZOOMIN,
    ZOOMOUT
};

typedef enum kDefaultPrefWindowSize : NSUInteger {
    kDefaultPrefWindowWidth = 560
} kDefaultPrefWindowSize;

typedef NS_ENUM(int32_t, kImageReplacementPrefsType) {
    kAlwaysReplace,
    kNeverReplace,
    kAskIfReplace,
};

typedef NS_ENUM(NSUInteger, kAppearanceType) {
    kDarkAppearance,
    kLightAppearance
};

// Notifications posted when theme settings change. The object is the
// affected Theme (nil for ColorModeChanged).
extern NSNotificationName const kPreferencesChangedNotification;
extern NSNotificationName const kDefaultSizeChangedNotification;
extern NSNotificationName const kBorderChangedNotification;
extern NSNotificationName const kColorModeChangedNotification;

@class Theme, Game, CoreDataManager, GlkHelperView, GlkController, GlkTextBufferWindow, TableViewController, DummyTextView, NSPersistentContainer;

@interface Preferences : NSWindowController <NSWindowDelegate, NSControlTextEditingDelegate, NSToolbarDelegate>

+ (void)rebuildTextAttributes;

@property (NS_NONATOMIC_IOSONLY, readonly, strong) Theme *cloneThemeIfNotEditable;
/// Like cloneThemeIfNotEditable, but does not give the theme a separate side
/// for the current mode: for changes that apply to both sides.
@property (NS_NONATOMIC_IOSONLY, readonly, strong) Theme *cloneThemeIfNotEditableKeepingSides;

#pragma mark Themes

- (void)restoreThemeSelection:(Theme *)aTheme;

+ (void)zoomIn;
+ (void)zoomOut;
+ (void)zoomToActualSize;
+ (void)scale:(CGFloat)scalefactor;
- (void)updatePanelAfterZoom;

#pragma mark Global accessors

@property (class, readonly) kZoomDirectionType zoomDirection;

// The theme the panel shows and edits; the default theme if there is none
@property (class, readonly) Theme *currentTheme;
/// NO for the built-in themes that the current mode hides: Lectrote Dark
/// in Fabulich mode.
+ (BOOL)themeIsVisibleInCurrentMode:(Theme *)theme;

// The preferences window controller, once its window has loaded
@property (class, readonly) Preferences *instance;

+ (void)changeCurrentGlkController:(GlkController *)ctrl;
- (void)updatePrefsPanel;

/// Override if set, else system. Used for theme light/dark color resolution.
+ (kAppearanceType)resolvedAppearance;

@property (nonatomic) Theme *defaultTheme ;

@property (readonly) CoreDataManager *coreDataManager;
@property (readonly) NSArray *sortDescriptors;
@property (readonly) NSManagedObjectContext *managedObjectContext;
@property (nonatomic, weak) Game *currentGame;
@property (nonatomic) BOOL oneThemeForAll;
@property BOOL adjustSize;
@property (weak) TableViewController *libcontroller;

@property BOOL inMagnification;

// Outlets used outside the Preferences implementation
@property (weak) IBOutlet NSColorWell *borderColorWell;
@property (weak) IBOutlet NSButton *addToLibraryCheckbox;

/// The light/dark mode symbols beside the header of each tab (Preferences+Appearance).
@property (nonatomic, strong) NSArray<NSControl *> *appearanceToggleButtons;

/// Enters or leaves Fabulich mode, as the button in the Global panel does.
- (void)setFabulichModeEnabled:(BOOL)on;

// The text view the font panel targets while the panel edits a theme style
@property (nonatomic, readonly) DummyTextView *dummyTextView;

- (void)changeAttributes:(id)sender;
- (void)changeDocumentBackgroundColor:(id)sender;

@end
