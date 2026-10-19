#import "Preferences.h"

#import "Preferences+Themes.h"
#import "Preferences+Window.h"
#import "Preferences+Fonts.h"
#import "Preferences+Zoom.h"
#import "Preferences+LibraryOrganiser.h"
#import "Preferences+Settings.h"

@class PreviewController, GlkStyle;

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
    NSUInteger restoreSelectionAttempts;
    CGFloat defaultWindowHeight;
    CGFloat restoredPreviewHeight;
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

@end
