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


@class Theme, Game, CoreDataManager, GlkHelperView, GlkController, GlkTextBufferWindow, ThemeArrayController, TableViewController, DummyTextView, ParagraphPopOver, NSPersistentContainer;

@interface Preferences : NSWindowController <NSWindowDelegate, NSControlTextEditingDelegate, NSToolbarDelegate>

+ (void)rebuildTextAttributes;

@property (NS_NONATOMIC_IOSONLY, readonly, strong) Theme *cloneThemeIfNotEditable;

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

+ (kZoomDirectionType)zoomDirection;

+ (Theme *)currentTheme;

+ (Preferences *)instance;

+ (void)changeCurrentGlkController:(GlkController *)ctrl;
- (void)updatePrefsPanel;

+ (kModeType)currentSystemMode;

@property BOOL previewShown;
@property (strong) IBOutlet NSView *sampleTextBorderView;

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

@property (weak) IBOutlet ThemeArrayController *arrayController;
@property (weak) IBOutlet NSScrollView *scrollView;

@property (weak) IBOutlet NSTextFieldCell *themesHeader;
@property (weak) IBOutlet NSTextFieldCell *detailsHeader;
@property (weak) IBOutlet NSTextFieldCell *miscHeader;
@property (weak) IBOutlet NSTextFieldCell *vOHeader;
@property (weak) IBOutlet NSTextFieldCell *zcodeHeader;
@property (weak) IBOutlet NSTextFieldCell *stylesHeader;

@property (weak) IBOutlet NSButton *btnOneThemeForAll;

@property (weak) IBOutlet NSPopUpButton *actionButton;

@property (weak) IBOutlet NSButton *btnAdd;
@property (weak) IBOutlet NSButton *btnRemove;
@property (weak) IBOutlet NSBox *divider;

@property (strong) IBOutlet NSMenuItem *lightModeMenuItem;
@property (strong) IBOutlet NSMenuItem *darkModeMenuItem;

@property (weak) IBOutlet NSButton *btnOverwriteStyles;
@property (weak) IBOutlet NSButton *swapGridColBtn;
@property (weak) IBOutlet NSButton *swapBufColBtn;

@property (weak) IBOutlet NSPopUpButton *vOMenuButton;
@property (weak) IBOutlet NSPopUpButton *vOImagesButton;
@property (weak) IBOutlet NSButton *btnVOSpeakCommands;
@property (weak) IBOutlet NSSlider *vODelaySlider;
@property (weak) IBOutlet NSButton *vODelayCheckbox;
@property (weak) IBOutlet NSTextField *vODelayTextField;

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

@property (weak) IBOutlet NSPopUpButton *z6GraphicsPopup;
@property (weak) IBOutlet NSButton *z6ColorizeCheckBox;
@property (weak) IBOutlet NSButton *z6Sim16ColoursCheckBox;

@property (weak) IBOutlet NSButton *btnNoHacks;
@property (weak) IBOutlet NSButton *btnDeterminism;
@property (weak) IBOutlet NSPopUpButton *errorHandlingPopup;
@property (weak) IBOutlet NSPopUpButton *imageReplacePopup;
@property (weak) IBOutlet NSPopUpButton *coverImagePopup;

@property (weak) IBOutlet NSButton *btnAutoBorderColor;
@property (weak) IBOutlet NSColorWell *borderColorWell;
@property (weak) IBOutlet NSButton *btnShowBezels;

@property (weak) IBOutlet NSButton *btnUnderlineLinksGrid;
@property (weak) IBOutlet NSButton *btnUnderlineLinksBuffer;

@property (weak) IBOutlet NSButton *libraryAtStartCheckbox;
@property (weak) IBOutlet NSButton *addToLibraryCheckbox;
@property (weak) IBOutlet NSButton *recheckMissingCheckbox;
@property (weak) IBOutlet NSTextField *recheckFrequencyTextfield;
@property (weak) IBOutlet NSButton *saveInGameDirCheckbox;

@property (weak) IBOutlet NSPopUpButton *palettePopup;
@property (weak) IBOutlet NSPopUpButton *inventoryPopup;
@property (weak) IBOutlet NSButton *delaysCheckbox;
@property (weak) IBOutlet NSButton *slowDrawCheckbox;
@property (strong) IBOutlet NSButton *hardDarkCheckbox;
@property (strong) IBOutlet NSButton *hardLightCheckbox;
@property (strong) IBOutlet NSButton *scottAdamsFlickerCheckbox;

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
