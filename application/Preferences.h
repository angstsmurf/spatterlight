/*
 * Preferences is a combined singleton / window controller.
 */

typedef enum kSpacesFormatType : NSUInteger {
    TAG_SPACES_GAME,
    TAG_SPACES_ONE,
    TAG_SPACES_TWO,
} kSpacesFormatType;

typedef enum kZoomDirectionType : NSUInteger {
    ZOOMRESET,
    ZOOMIN,
    ZOOMOUT,
} kZoomDirectionType;

typedef enum kDefaultPrefWindowSize : NSUInteger {
    kDefaultPrefWindowWidth = 516,
    kDefaultPrefWindowHeight = 330,
    kDefaultPrefsLowerViewHeight = 311
} kDefaultPrefWindowSize;


@class Theme, Game, CoreDataManager, GlkHelperView, GlkController, GlkTextBufferWindow, ThemeArrayController, LibController;

@interface Preferences : NSWindowController <NSWindowDelegate, NSControlTextEditingDelegate> {
    IBOutlet NSButton *btnInputFont, *btnBufferFont, *btnGridFont;
    IBOutlet NSColorWell *clrInputFg, *clrBufferFg, *clrGridFg;
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
    IBOutlet NSBox *sampleTextBorderView;
    IBOutlet GlkHelperView *sampleTextView;

    GlkController *glkcntrl;
    GlkTextBufferWindow *glktxtbuf;

    NSButton *selectedFontButton;

    BOOL disregardTableSelection;
    BOOL previewHidden;
    CGFloat previewTextHeight;
}


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

- (IBAction)addTheme:(id)sender;
- (IBAction)removeTheme:(id)sender;
- (IBAction)clickedOneThemeForAll:(id)sender;
- (IBAction)clickedAppliesToSelected:(id)sender;

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

@property (readonly) Theme *defaultTheme;
@property (readonly) CoreDataManager *coreDataManager;
@property (readonly) NSArray *sortDescriptors;
@property (readonly) NSManagedObjectContext *managedObjectContext;
@property Game *currentGame;
@property BOOL oneThemeForAll;
@property LibController *libcontroller;

@property (strong) IBOutlet ThemeArrayController *arrayController;
@property (strong) IBOutlet NSScrollView *scrollView;

@property (strong) IBOutlet NSTextFieldCell *detailsHeader;
@property (strong) IBOutlet NSTextFieldCell *themesHeader;
@property (strong) IBOutlet NSButton *btnOneThemeForAll;
@property (strong) IBOutlet NSButton *btnAppliesToSelected;

@property (strong) IBOutlet NSPopUpButton *actionButton;

@property (strong) IBOutlet NSButton *btnAdd;
@property (strong) IBOutlet NSButton *btnRemove;
@property (strong) IBOutlet NSBox *divider;

@property (strong) IBOutlet NSButton *btnOverwriteStyles;

@end
