#import <Cocoa/Cocoa.h>

extern NSArray *gGameFileTypes;
extern NSArray *gDocFileTypes;
extern NSArray *gSaveFileTypes;
extern NSArray *gGridStyleNames;
extern NSArray *gBufferStyleNames;

extern NSPasteboardType PasteboardFileURLPromise,
PasteboardFilePromiseContent,
PasteboardFilePasteLocation;

extern NSDictionary *gExtMap;
extern NSDictionary *gFormatMap;

// NSUserDefaults keys used in more than one place. Defaults.plist has the
// default values of most of them.
extern NSString * const kShowThemePreviewKey;
extern NSString * const kThemeNameKey;
extern NSString * const kSelectedStyleKey;
extern NSString * const kSelectedGlkWindowTypeKey;
extern NSString * const kRecheckFrequencyKey;
extern NSString * const kRecheckForMissingKey;
extern NSString * const kOneThemeForAllKey;
extern NSString * const kLastThemesRebuildKey;
extern NSString * const kAdjustSizeKey;
extern NSString * const kImageReplacementKey;
extern NSString * const kSaveInGameDirectoryKey;
extern NSString * const kShowBezelsKey;
extern NSString * const kShowLibraryKey;
extern NSString * const kAddToLibraryKey;

// Set by the "Do not show this message again" box of an alert. The
// Preferences "Reset dialogs" button clears every key in AlertSuppressionKeys().
extern NSString * const kTerminationAlertSuppressionKey;
extern NSString * const kUseForAllAlertSuppressionKey;
extern NSString * const kOverwriteStylesAlertSuppressionKey;
extern NSString * const kAutorestoreAlertSuppressionKey;
extern NSString * const kCloseAlertSuppressionKey;
extern NSString * const kCommandScriptAlertSuppressionKey;
extern NSString * const kSaveFileAlertSuppressionKey;
extern NSString * const kImageComparisonSuppressionKey;
extern NSString * const kVerifyAlertSuppressionKey;
extern NSArray<NSString *> *AlertSuppressionKeys(void);

// Fills in the arrays, dictionaries and pasteboard types above. Called once,
// by AppDelegate -awakeFromNib.
extern void InitializeConstants(void);
