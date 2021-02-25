/*
 * Preferences is a combined singleton / window controller.
 */

typedef enum kSpacesFormatType : NSUInteger {
    TAG_SPACES_GAME,
    TAG_SPACES_ONE,
    TAG_SPACES_TWO
} kSpacesFormatType;

typedef enum kZoomDirectionType : NSUInteger {
    ZOOMRESET,
    ZOOMIN,
    ZOOMOUT
} kZoomDirectionType;

typedef enum kDefaultPrefWindowSize : NSUInteger {
    kDefaultPrefWindowWidth = 516,
    kDefaultPrefWindowHeight = 330,
    kDefaultPrefsLowerViewHeight = 311
} kDefaultPrefWindowSize;

typedef enum kVOMenuPrefsType : int32_t {
    kVOMenuNone,
    kVOMenuTextOnly,
    kVOMenuIndex,
    kVOMenuTotal
} kVOMenuPrefsType;

typedef enum kBZArrowsPrefsType : int32_t {
    kBZArrowsCompromise,
    kBZArrowsSwapped,
    kBZArrowsOriginal,
} kBZArrowsPrefsType;


@class Theme, Game, CoreDataManager, GlkHelperView, GlkController, GlkTextBufferWindow, ThemeArrayController, LibController;

@interface Preferences : NSWindowController <NSWindowDelegate, NSControlTextEditingDelegate>

+ (void)rebuildTextAttributes;

- (IBAction)changeColor:(id)sender;
- (IBAction)showFontPanel:(id)sender;
- (IBAction)changeFont:(id)sender;
- (IBAction)changeMargin:(id)sender;
- (IBAction)changeLeading:(id)sender;
- (IBAction)changeSmartQuotes:(id)sender;
- (IBAction)changeSpaceFormatting:(id)sender;
- (IBAction)changeDefaultSize:(id)sender;
- (IBAction)changeBorderSize:(id)sender;
- (IBAction)changeEnableGraphics:(id)sender;
- (IBAction)changeEnableSound:(id)sender;
- (IBAction)changeEnableStyles:(id)sender;
- (IBAction)changeOverwriteStyles:(id)sender;
- (IBAction)swapColors:(id)sender;

- (IBAction)changeVOSpeakCommands:(id)sender;

#pragma mark Themes menu
- (IBAction)addTheme:(id)sender;
- (IBAction)removeTheme:(id)sender;
- (IBAction)clickedOneThemeForAll:(id)sender;

@property (strong) IBOutlet NSButton *btnAdjustSize;
- (IBAction)changeAdjustSize:(id)sender;

#pragma mark Action menu
- (IBAction)applyToSelected:(id)sender;
- (IBAction)selectUsingTheme:(id)sender;
- (IBAction)deleteUserThemes:(id)sender;
- (IBAction)togglePreview:(id)sender;
- (IBAction)editNewEntry:(id)sender;

- (void)createDefaultThemes;
- (void)restoreThemeSelection:(id)sender;

+ (void)zoomIn;
+ (void)zoomOut;
+ (void)zoomToActualSize;
+ (void)scale:(CGFloat)scalefactor;
- (void)updatePanelAfterZoom;

#pragma mark Global accessors

+ (NSColor *)gridBackground;
+ (NSColor *)gridForeground;
+ (NSColor *)bufferBackground;
+ (NSColor *)bufferForeground;
+ (NSColor *)inputColor;

+ (double)lineHeight;
+ (double)charWidth;
+ (CGFloat)gridMargins;
+ (CGFloat)bufferMargins;
+ (CGFloat)border;
+ (CGFloat)leading;

+ (BOOL)stylesEnabled;
+ (BOOL)smartQuotes;
+ (kSpacesFormatType)spaceFormat;
+ (kZoomDirectionType)zoomDirection;

+ (BOOL)graphicsEnabled;
+ (BOOL)soundEnabled;

+ (Theme *)currentTheme;

+ (Preferences *)instance;

+ (void)changeCurrentGame:(Game *)game;

@property BOOL previewShown;
@property GlkTextBufferWindow *glktxtbuf;
@property IBOutlet NSBox *sampleTextBorderView;

@property (readonly) Theme *defaultTheme;
@property (readonly) CoreDataManager *coreDataManager;
@property (readonly) NSArray *sortDescriptors;
@property (readonly) NSManagedObjectContext *managedObjectContext;
@property Game *currentGame;
@property BOOL oneThemeForAll;
@property BOOL adjustSize;
@property LibController *libcontroller;

@property (strong) IBOutlet ThemeArrayController *arrayController;
@property (strong) IBOutlet NSScrollView *scrollView;

@property (strong) IBOutlet NSTextFieldCell *detailsHeader;
@property (strong) IBOutlet NSTextFieldCell *themesHeader;
@property (strong) IBOutlet NSTextFieldCell *miscHeader;
@property (strong) IBOutlet NSTextFieldCell *vOHeader;
@property (strong) IBOutlet NSTextFieldCell *zcodeHeader;

@property (strong) IBOutlet NSButton *btnOneThemeForAll;

@property (strong) IBOutlet NSPopUpButton *actionButton;

@property (strong) IBOutlet NSButton *btnAdd;
@property (strong) IBOutlet NSButton *btnRemove;
@property (strong) IBOutlet NSBox *divider;

@property (strong) IBOutlet NSButton *btnOverwriteStyles;
@property (strong) IBOutlet NSButton *swapGridColBtn;
@property (strong) IBOutlet NSButton *swapBufColBtn;

@property (strong) IBOutlet NSPopUpButton *vOMenuButton;
- (IBAction)changeVOMenuMenu:(id)sender;

@property (strong) IBOutlet NSButton *btnVOSpeakCommands;

@property (strong) IBOutlet NSPopUpButton *beepHighMenu;
- (IBAction)changeBeepHighMenu:(id)sender;
@property (strong) IBOutlet NSPopUpButton *beepLowMenu;
- (IBAction)changeBeepLowMenu:(id)sender;

@property (strong) IBOutlet NSPopUpButton *zterpMenu;
- (IBAction)changeZterpMenu:(id)sender;
@property (strong) IBOutlet NSPopUpButton *bZArrowsMenu;
- (IBAction)changeBZArrowsMenu:(id)sender;

@property (weak) IBOutlet NSTextField *zVersionTextField;
- (IBAction)changeZVersion:(id)sender;

@property (weak) IBOutlet NSTextField *bZVerticalTextField;
- (IBAction)changeBZVerticalTextField:(id)sender;

@property (weak) IBOutlet NSStepper *bZVerticalStepper;
- (IBAction)changeBZVerticalStepper:(id)sender;

@property (weak) IBOutlet NSButton *btnAutosave;
- (IBAction)changeAutosave:(id)sender;

@property (weak) IBOutlet NSButton *btnAutosaveOnTimer;
- (IBAction)changeAutosaveOnTimer:(id)sender;

- (IBAction)resetDialogs:(NSButton *)sender;

@property (weak) IBOutlet NSSlider *timerSlider;
- (IBAction)changeTimerSlider:(id)sender;

@property (weak) IBOutlet NSTextField *timerTextField;
- (IBAction)changeTimerTextField:(id)sender;

@property (weak) IBOutlet NSMenuItem *standardZArrowsMenuItem;
@property (weak) IBOutlet NSMenuItem *compromiseZArrowsMenuItem;
@property (weak) IBOutlet NSMenuItem *strictZArrowsMenuItem;

@end
