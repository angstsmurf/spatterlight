#import "Preferences.h"

#import "Preferences+Themes.h"
#import "Preferences+Window.h"
#import "Preferences+Fonts.h"
#import "Preferences+Zoom.h"
#import "Preferences+LibraryOrganiser.h"
#import "Preferences+Settings.h"

@class PreviewController, GlkStyle, ThemeArrayController, ParagraphPopOver;

/*
 * Class extension shared by Preferences.m and its category files
 * (+Themes, +Window, +Fonts, +Zoom, +LibraryOrganiser, +Settings).
 * Not for use outside the Preferences implementation.
 */

@interface Preferences () <NSWindowDelegate, NSControlTextEditingDelegate> {
    IBOutlet NSButton *btnAnyFont, *btnBufferFont, *btnGridFont;
    IBOutlet NSColorWell *clrAnyFg, *clrBufferFg, *clrGridFg;
    IBOutlet NSColorWell *clrBufferBg, *clrGridBg;
    IBOutlet NSTextField *txtBufferMargin, *txtGridMargin;
    IBOutlet NSTextField *txtRows, *txtCols;
    IBOutlet NSTextField *txtBorder;
    IBOutlet NSButton *btnSmartQuotes;
    IBOutlet NSButton *btnSpaceFormat;
    IBOutlet NSButton *btnEnableGraphics;
    IBOutlet NSButton *btnEnableSound;
    IBOutlet NSButton *btnEnableStyles;
    IBOutlet NSTableView *themesTableView;

    NSDictionary<NSToolbarItemIdentifier, NSView *> *itemIdentifierToViewDict;

    NSButton *selectedFontButton;

    BOOL disregardTableSelection;
    BOOL ignoreTableSelectionChanges;
    // A clone of a non-editable theme that restoreThemeSelection: has not
    // selected in the themes table yet.
    __weak Theme *pendingSelectionTheme;
    // The theme restoreThemeSelection: selects once the themes array
    // controller has fetched its content
    __weak Theme *themeAwaitingTable;
    BOOL observingArrangedThemes;
    CGFloat defaultWindowHeight;
    CGFloat restoredPreviewHeight;
    // Bumped by every resizeWindowToHeight:animate:, so that the completion
    // of a superseded animation does not hide the preview
    NSUInteger resizeGeneration;
    NSView *currentPanel;

    NSDictionary *catalinaSoundsToBigSur;
    NSDictionary *bigSurSoundsToCatalina;

    NSToolbarItemIdentifier themesPanel;
    NSToolbarItemIdentifier stylesPanel;
    NSToolbarItemIdentifier presentationPanel;
    NSToolbarItemIdentifier formatPanel;
    NSToolbarItemIdentifier voiceOverPanel;
    NSToolbarItemIdentifier miscPanel;
    NSToolbarItemIdentifier globalPanel;

    // Backing ivars of properties the category files access directly;
    // synthesized ivars are only visible in Preferences.m.
    BOOL _adjustSize;
    __weak ThemeArrayController *_arrayController;
    __weak NSStepper *_bZVerticalStepper;
    __weak NSTextField *_bZVerticalTextField;
    NSLayoutConstraint *_belowHeightConstraint;
    NSView *_belowView;
    __weak NSColorWell *_borderColorWell;
    NSLayoutConstraint *_bottomConstraint;
    __weak NSButton *_btnAutosaveOnTimer;
    __weak NSButton *_btnRemove;
    __weak NSButton *_btnUnderlineLinksGrid;
    NSButton *_chooseLibraryButton;
    __weak Game *_currentGame;
    NSMenuItem *_darkModeMenuItem;
    BOOL _darkOverrideActive;
    Theme *_darkTheme;
    __weak NSTextFieldCell *_detailsHeader;
    __weak NSButton *_hardDarkCheckbox;
    __weak NSButton *_hardLightCheckbox;
    __weak NSButton *_keepOrganisedCheckbox;
    __weak TableViewController *_libcontroller;
    NSTextField *_libraryLocationField;
    NSMenuItem *_lightModeMenuItem;
    BOOL _lightOverrideActive;
    Theme *_lightTheme;
    NSManagedObjectContext *_managedObjectContext;
    __weak NSStepper *_marginHorizontalBufferStepper;
    __weak NSTextField *_marginHorizontalBufferTextField;
    __weak NSStepper *_marginHorizontalGridStepper;
    __weak NSTextField *_marginHorizontalGridTextField;
    __weak NSStepper *_marginVerticalBufferStepper;
    __weak NSTextField *_marginVerticalBufferTextField;
    __weak NSStepper *_marginVerticalGridStepper;
    __weak NSTextField *_marginVerticalGridTextField;
    NSPopover *_marginsPopover;
    __weak NSTextFieldCell *_miscHeader;
    BOOL _oneThemeForAll;
    ParagraphPopOver *_paragraphPopover;
    PreviewController *_previewController;
    NSLayoutConstraint *_previewHeightConstraint;
    BOOL _previewShown;
    __weak NSTextField *_recheckFrequencyTextfield;
    __weak NSButton *_recheckMissingCheckbox;
    NSView *_sampleTextBorderView;
    __weak NSStepper *_scrollbackLimitStepper;
    __weak NSTextField *_scrollbackLimitTextField;
    __weak NSTextField *_scrollbackUnitLabel;
    __weak NSPopUpButton *_styleNamePopup;
    __weak NSTextFieldCell *_stylesHeader;
    __weak NSButton *_swapBufColBtn;
    __weak NSButton *_swapGridColBtn;
    __weak NSTextFieldCell *_themesHeader;
    __weak NSSlider *_timerSlider;
    __weak NSTextField *_timerTextField;
    __weak NSButton *_vODelayCheckbox;
    __weak NSSlider *_vODelaySlider;
    __weak NSTextField *_vODelayTextField;
    __weak NSTextFieldCell *_vOHeader;
    __weak NSPopUpButton *_windowTypePopup;
    __weak NSTextField *_zVersionTextField;
    __weak NSTextFieldCell *_zcodeHeader;
}

// The theme the panel shows and edits. It is set before the panel exists,
// so it is a class property; the instance property forwards to it, which
// lets self.theme work in class and instance methods alike.
@property (class) Theme *theme;
@property Theme *theme;

@property (class, readwrite) kZoomDirectionType zoomDirection;
@property (class, readwrite) Preferences *instance;

@property (strong) IBOutlet PreviewController *previewController;
@property (strong) IBOutlet NSLayoutConstraint *previewHeightConstraint;

@property (strong) IBOutlet NSButton *btnAdjustSize;

@property BOOL previewShown;
@property (strong) IBOutlet NSView *sampleTextBorderView;
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

@property (weak) IBOutlet NSTextField *scrollbackLimitTextField;
@property (weak) IBOutlet NSStepper *scrollbackLimitStepper;
@property (weak) IBOutlet NSTextField *scrollbackUnitLabel;

@property (weak) IBOutlet NSSlider *timerSlider;
@property (weak) IBOutlet NSTextField *timerTextField;

@property (weak) IBOutlet NSMenuItem *standardZArrowsMenuItem;
@property (weak) IBOutlet NSMenuItem *compromiseZArrowsMenuItem;
@property (weak) IBOutlet NSMenuItem *strictZArrowsMenuItem;
@property (weak) IBOutlet NSPopUpButton *windowTypePopup;
@property (weak) IBOutlet NSPopUpButton *styleNamePopup;
@property (weak) IBOutlet NSButton *quoteBoxCheckBox;

@property (weak) IBOutlet NSPopUpButton *comprehendGraphicsPopup;
@property (weak) IBOutlet NSPopUpButton *z6GraphicsPopup;
@property (weak) IBOutlet NSButton *z6ColorizeCheckBox;

@property (weak) IBOutlet NSButton *btnNoHacks;
@property (weak) IBOutlet NSButton *btnDeterminism;
@property (weak) IBOutlet NSPopUpButton *errorHandlingPopup;
@property (weak) IBOutlet NSPopUpButton *imageReplacePopup;
@property (weak) IBOutlet NSPopUpButton *coverImagePopup;

@property (weak) IBOutlet NSButton *btnAutoBorderColor;
@property (weak) IBOutlet NSButton *btnShowBezels;

@property (weak) IBOutlet NSButton *btnUnderlineLinksGrid;
@property (weak) IBOutlet NSButton *btnUnderlineLinksBuffer;

@property (weak) IBOutlet NSButton *libraryAtStartCheckbox;
@property (weak) IBOutlet NSButton *recheckMissingCheckbox;
@property (weak) IBOutlet NSTextField *recheckFrequencyTextfield;
@property (weak) IBOutlet NSButton *saveInGameDirCheckbox;
@property (weak) IBOutlet NSButton *keepOrganisedCheckbox;

@property (weak) IBOutlet NSPopUpButton *palettePopup;
@property (weak) IBOutlet NSPopUpButton *inventoryPopup;
@property (weak) IBOutlet NSButton *delaysCheckbox;
@property (weak) IBOutlet NSButton *slowDrawCheckbox;
@property (weak) IBOutlet NSButton *hardDarkCheckbox;
@property (weak) IBOutlet NSButton *hardLightCheckbox;
@property (weak) IBOutlet NSButton *scottAdamsFlickerCheckbox;
@property (weak) IBOutlet NSButton *zMachineNoErrWinCheckbox;

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

@property (strong) IBOutlet NSView *themesView;
@property (strong) IBOutlet NSView *stylesView;
@property (strong) IBOutlet NSView *presentationView;
@property (strong) IBOutlet NSView *formatView;
@property (strong) IBOutlet NSView *voiceOverView;
@property (strong) IBOutlet NSView *miscView;
@property (strong) IBOutlet NSView *globalView;

@property (strong) IBOutlet NSView *belowView;
@property (strong) IBOutlet NSLayoutConstraint *belowHeightConstraint;
@property (strong) IBOutlet NSLayoutConstraint *bottomConstraint;

// "Keep games organised" controls on the Global settings pane
@property (strong) IBOutlet NSTextField *libraryLocationField;
@property (strong) IBOutlet NSButton *chooseLibraryButton;
@property (weak) NSOpenPanel *libraryLocationPanel;
@property (strong) NSURL *libraryChooserOriginalRoot;

// Implemented in Preferences.m
- (GlkStyle *)selectedStyle;
- (NSString *)selectedStyleName;
- (NSString *)secondsAccessibilityString;

// An Okay/Cancel sheet on the window with a "Do not show again." box.
// A ticked box sets suppressionKey in the user defaults; the caller checks it
// before asking. handler gets YES for Okay.
- (void)showOkayCancelAlertWithMessage:(NSString *)message
                                  info:(NSString *)info
                        suppressionKey:(NSString *)suppressionKey
                               handler:(void (^)(BOOL okay))handler;

@end
