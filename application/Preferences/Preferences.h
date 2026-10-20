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

typedef NS_ENUM(NSUInteger, kModeType) {
    kDarkMode,
    kLightMode
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

// The preferences window controller, once its window has loaded
@property (class, readonly) Preferences *instance;

+ (void)changeCurrentGlkController:(GlkController *)ctrl;
- (void)updatePrefsPanel;

+ (kModeType)currentSystemMode;

@property (nonatomic) Theme *defaultTheme ;
@property Theme *darkTheme;
@property Theme *lightTheme;
@property BOOL darkOverrideActive;
@property BOOL lightOverrideActive;

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

// The text view the font panel targets while the panel edits a theme style
@property (nonatomic, readonly) DummyTextView *dummyTextView;

- (void)changeAttributes:(id)sender;
- (void)changeDocumentBackgroundColor:(id)sender;

@end
