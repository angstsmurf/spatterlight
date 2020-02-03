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

typedef enum kThemeSelectionScopeType : NSUInteger {
    USE_FOR_ALL,
    USE_FOR_ACTIVE,
    USE_FOR_SELECTED,
} kThemeSelectionScopeType;


@class Theme, CoreDataManager, GlkHelperView, GlkController, GlkTextBufferWindow, ThemeArrayController;

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
    IBOutlet NSButton *btnUseThemeForAll;
    IBOutlet NSButton *btnUseThemeForRunning;
    IBOutlet NSButton *btnUseThemeForSelected;
    IBOutlet NSView *sampleTextBorderView;
    IBOutlet GlkHelperView *sampleTextView;

    GlkController *glkcntrl;
    GlkTextBufferWindow *glktxtbuf;

    NSButton *selectedFontButton;

    BOOL disregardTableSelection;

    kThemeSelectionScopeType themeSelectionScope;
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

- (IBAction)clickedSegmentedControl:(id)sender;

- (IBAction)pushedUseThemeForAll:(id)sender;
- (IBAction)pushedUseThemeForRunning:(id)sender;
- (IBAction)pushedUseThemeForSelected:(id)sender;

- (void)createDefaultThemes;

+ (void)zoomIn;
+ (void)zoomOut;
+ (void)zoomToActualSize;
+ (void)scale:(CGFloat)scalefactor;
- (void)updatePanelAfterZoom;

+ (NSColor *)gridBackground;
+ (NSColor *)gridForeground;
+ (NSColor *)bufferBackground;
+ (NSColor *)bufferForeground;
+ (NSColor *)inputColor;

//+ (NSColor *)foregroundColor:(int)number;
//+ (NSColor *)backgroundColor:(int)number;

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
+ (BOOL)useScreenFonts;

+ (Theme *)currentTheme;

+ (Preferences *)instance;

@property (readonly) Theme *defaultTheme;
@property (readonly) CoreDataManager *coreDataManager;
@property (readonly) NSArray *sortDescriptors;

@property (readonly) NSManagedObjectContext *managedObjectContext;
@property (strong) IBOutlet NSSegmentedControl *addAndRemove;
@property (strong) IBOutlet ThemeArrayController *arrayController;
@property (strong) IBOutlet NSScrollView *scrollView;
@property (strong) IBOutlet NSTextFieldCell *settingsThemeHeader;
@property (strong) IBOutlet NSTextFieldCell *gamesScopeHeader;

@end
