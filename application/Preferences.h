/*
 * Preferences is a combined singleton / window controller.
 */

#define TAG_SPACES_GAME 0
#define TAG_SPACES_ONE 1
#define TAG_SPACES_TWO 2

#define ZOOMRESET 0
#define ZOOMIN 1
#define ZOOMOUT 2

@class Theme, CoreDataManager, GlkHelperView, GlkController, GlkTextBufferWindow;

@interface Preferences : NSWindowController <NSWindowDelegate> {
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
    IBOutlet NSButton *btnUseScreenFonts;
    IBOutlet NSTableView *themesTableView;
    IBOutlet NSButton *btnUseThemeForAll;
    IBOutlet NSButton *btnUseThemeForRunning;
    IBOutlet NSButton *btnUseThemeForSelected;
    IBOutlet NSView *sampleTextBorderView;
    IBOutlet GlkHelperView *sampleTextView;

    GlkController *glkcntrl;
    GlkTextBufferWindow *glktxtbuf;

    NSFont *__strong *selfontp;
    NSColor *__strong *colorp;
    NSColor *__strong *colorp2;
}

@property (readonly) Theme *defaultTheme;
@property (readonly) CoreDataManager *coreDataManager;
@property (readonly) NSManagedObjectContext *managedObjectContext;

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
//- (IBAction)changeUseScreenFonts:(id)sender;

- (IBAction)addTheme:(id)sender;
- (IBAction)deleteTheme:(id)sender;

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

+ (NSColor *)foregroundColor:(int)number;
+ (NSColor *)backgroundColor:(int)number;

+ (float)lineHeight;
+ (float)charWidth;
+ (CGFloat)gridMargins;
+ (CGFloat)bufferMargins;
+ (CGFloat)border;
+ (CGFloat)leading;

+ (BOOL)stylesEnabled;
+ (BOOL)smartQuotes;
+ (NSUInteger)spaceFormat;
+ (NSUInteger)zoomDirection;

+ (BOOL)graphicsEnabled;
+ (BOOL)soundEnabled;
+ (BOOL)useScreenFonts;

//+ (NSDictionary *)attributesForGridStyle:(int)style;
//+ (NSDictionary *)attributesForBufferStyle:(int)style;

+ (Theme *)currentTheme;

+ (Preferences *)instance;

@property (strong) IBOutlet NSTextField *sampleTextfield;
@end
