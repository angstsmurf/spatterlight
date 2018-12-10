/*
 * Preferences is a combined singleton / window controller.
 */

#define TAG_SPACES_GAME 0
#define TAG_SPACES_ONE 1
#define TAG_SPACES_TWO 2

#import "Settings.h"
#import "Game.h"
#import "Font.h"


@interface Preferences : NSWindowController <NSWindowDelegate>
{

    IBOutlet NSButton *btnInputFont, *btnBufferFont, *btnGridFont;
    IBOutlet NSColorWell *clrInputFg, *clrBufferFg, *clrGridFg;
    IBOutlet NSColorWell *clrBufferBg, *clrGridBg;
    IBOutlet NSTextField *txtBufferMargin, *txtGridMargin, *txtLeading;
    IBOutlet NSTextField *txtRows, *txtCols;
    IBOutlet NSButton *btnSmartQuotes;
    IBOutlet NSButton *btnSpaceFormat;
    IBOutlet NSButton *btnEnableGraphics;
    IBOutlet NSButton *btnEnableSound;
    IBOutlet NSButton *btnEnableStyles;
    IBOutlet NSButton *btnUseScreenFonts;
    NSFont * __strong *selfontp;
    NSColor * __strong *colorp;
	NSColor * __strong *colorp2;
}

+ (void) rebuildTextAttributes;

+ (NSSize) defaultWindowSize;

+ (void) changePreferences: (Settings *)settings forGame: (Game *)game;
+ (void) savePreferences;
+ (void) nilCurrentSettings;

- (void) updatePreferencePanel;


+ (void) changeDefaultSize: (NSSize)size forSettings: (Settings *)setting;

- (IBAction) changeColor: (id)sender;
- (IBAction) showFontPanel: (id)sender;
- (IBAction) changeFont: (id)sender;
- (IBAction) changeMargin: (id)sender;
- (IBAction) changeLeading: (id)sender;
- (IBAction) changeSmartQuotes: (id)sender;
- (IBAction) changeSpaceFormatting: (id)sender;
- (IBAction) changeEnableGraphics: (id)sender;
- (IBAction) changeEnableSound: (id)sender;
- (IBAction) changeEnableStyles: (id)sender;

+ (instancetype)sharedInstance;
- (instancetype)init NS_UNAVAILABLE NS_DESIGNATED_INITIALIZER;

//- (void) setColor:(NSColor *)col forAttribute:(NSString *)attr;
- (void)changeAttributes:(id)sender;
- (NSFontPanelModeMask)validModesForFontPanel:(NSFontPanel *)fontPanel;


//- (IBAction) changeUseScreenFonts: (id)sender;

+ (NSColor*) gridBackground;
+ (NSColor*) gridForeground;
+ (NSColor*) bufferBackground;
+ (NSColor*) bufferForeground;
+ (NSColor*) inputColor;


+ (NSColor*) foregroundColor: (int)number;
+ (NSColor*) backgroundColor: (int)number;

+ (float) lineHeight;
+ (float) charWidth;
+ (NSInteger) gridMargins;
+ (NSInteger) bufferMargins;
+ (float) leading;

+ (NSInteger) stylesEnabled;
+ (NSInteger) smartQuotes;
+ (NSInteger) spaceFormat;

+ (NSInteger) graphicsEnabled;
+ (NSInteger) soundEnabled;
+ (NSInteger) useScreenFonts;

+ (NSDictionary*) attributesForGridStyle: (int)style;
+ (NSDictionary*) attributesForBufferStyle: (int)style;

+ (Settings *) currentSettings;
+ (Settings *) defaultSettings;

+ (Game *) currentGame;

+ (NSManagedObjectContext*) managedObjectContext;

@end
