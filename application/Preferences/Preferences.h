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

typedef enum kVOImagePrefsType : int32_t {
    kVOImageWithDescriptionOnly,
    kVOImageNone,
    kVOImageAll
} kVOImagePrefsType;

typedef enum kBZArrowsPrefsType : int32_t {
    kBZArrowsCompromise,
    kBZArrowsSwapped,
    kBZArrowsOriginal,
} kBZArrowsPrefsType;


@class Theme, Game, CoreDataManager, GlkHelperView, GlkController, GlkTextBufferWindow, ThemeArrayController, LibController, DummyTextView, ParagraphPopOver;

@interface Preferences : NSWindowController <NSWindowDelegate, NSControlTextEditingDelegate>

+ (void)rebuildTextAttributes;

- (Theme *)cloneThemeIfNotEditable;

#pragma mark Themes menu

@property (strong) IBOutlet NSButton *btnAdjustSize;

#pragma mark Action menu

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
@property (strong) GlkTextBufferWindow *glktxtbuf;
@property (strong) IBOutlet NSBox *sampleTextBorderView;

@property (readonly) Theme *defaultTheme;
@property (readonly) CoreDataManager *coreDataManager;
@property (readonly) NSArray *sortDescriptors;
@property (readonly) NSManagedObjectContext *managedObjectContext;
@property (weak) Game *currentGame;
@property BOOL oneThemeForAll;
@property BOOL adjustSize;
@property (weak) LibController *libcontroller;

@property BOOL inMagnification;

@property (weak) IBOutlet ThemeArrayController *arrayController;
@property (weak) IBOutlet NSScrollView *scrollView;

@property (weak) IBOutlet NSTextFieldCell *detailsHeader;
@property (weak) IBOutlet NSTextFieldCell *themesHeader;
@property (weak) IBOutlet NSTextFieldCell *miscHeader;
@property (weak) IBOutlet NSTextFieldCell *vOHeader;
@property (weak) IBOutlet NSTextFieldCell *zcodeHeader;

@property (weak) IBOutlet NSButton *btnOneThemeForAll;

@property (weak) IBOutlet NSPopUpButton *actionButton;

@property (weak) IBOutlet NSButton *btnAdd;
@property (weak) IBOutlet NSButton *btnRemove;
@property (weak) IBOutlet NSBox *divider;

@property (weak) IBOutlet NSButton *btnOverwriteStyles;
@property (weak) IBOutlet NSButton *swapGridColBtn;
@property (weak) IBOutlet NSButton *swapBufColBtn;

@property (weak) IBOutlet NSPopUpButton *vOMenuButton;
@property (weak) IBOutlet NSPopUpButton *vOImagesButton;
@property (weak) IBOutlet NSButton *btnVOSpeakCommands;

@property (weak) IBOutlet NSPopUpButton *beepHighMenu;
@property (weak) IBOutlet NSPopUpButton *beepLowMenu;
@property (weak) IBOutlet NSPopUpButton *zterpMenu;
@property (weak) IBOutlet NSPopUpButton *bZArrowsMenu;
@property (weak) IBOutlet NSTextField *zVersionTextField;
@property (weak) IBOutlet NSTextField *bZVerticalTextField;
@property (weak) IBOutlet NSStepper *bZVerticalStepper;

@property (weak) IBOutlet NSButton *btnAutosave;
@property (weak) IBOutlet NSButton *btnSmoothScroll;
@property (weak) IBOutlet NSButton *btnAutosaveOnTimer;

@property (weak) IBOutlet NSSlider *timerSlider;
@property (weak) IBOutlet NSTextField *timerTextField;

@property (weak) IBOutlet NSMenuItem *standardZArrowsMenuItem;
@property (weak) IBOutlet NSMenuItem *compromiseZArrowsMenuItem;
@property (weak) IBOutlet NSMenuItem *strictZArrowsMenuItem;
@property (weak) IBOutlet NSPopUpButton *windowTypePopup;
@property (weak) IBOutlet NSPopUpButton *styleNamePopup;
@property (weak) IBOutlet NSButton *quoteBoxCheckBox;

@property DummyTextView *dummyTextView;

- (void)changeAttributes:(id)sender;
- (void)changeDocumentBackgroundColor:(id)sender;

@property (strong) IBOutlet NSPopover *marginsPopover;

@property (weak) IBOutlet NSTextField *marginHorizontalGridTextField;
@property (weak) IBOutlet NSStepper *marginHorizontalGridStepper;
@property (weak) IBOutlet NSTextField *marginVerticalGridTextField;
@property (weak) IBOutlet NSStepper *marginVerticalGridStepper;
@property (weak) IBOutlet NSTextField *marginHorizontalBufferTextField;
@property (weak) IBOutlet NSStepper *marginHorizontalBufferStepper;
@property (weak) IBOutlet NSTextField *marginVerticalBufferTextField;
@property (weak) IBOutlet NSStepper *marginVerticalBufferStepper;

@property ParagraphPopOver *paragraphPopover;

@end
