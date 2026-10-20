//
//  Constants.m
//  Spatterlight
//
//  Definitions of the globals declared in Constants.h.
//

#import "Constants.h"

NSArray *gGameFileTypes;
NSArray *gDocFileTypes;
NSArray *gSaveFileTypes;

NSArray *gBufferStyleNames;
NSArray *gGridStyleNames;

NSDictionary *gExtMap;
NSDictionary *gFormatMap;

NSPasteboardType PasteboardFileURLPromise,
PasteboardFilePromiseContent,
PasteboardFilePasteLocation;

NSString * const kShowThemePreviewKey = @"ShowThemePreview";
NSString * const kThemeNameKey = @"themeName";
NSString * const kSelectedStyleKey = @"SelectedStyle";
NSString * const kSelectedGlkWindowTypeKey = @"SelectedGlkWindowType";
NSString * const kRecheckFrequencyKey = @"RecheckFrequency";
NSString * const kRecheckForMissingKey = @"RecheckForMissing";
NSString * const kOneThemeForAllKey = @"OneThemeForAll";
NSString * const kLastThemesRebuildKey = @"LastThemesRebuild";
NSString * const kAdjustSizeKey = @"AdjustSize";
NSString * const kImageReplacementKey = @"ImageReplacement";
NSString * const kSaveInGameDirectoryKey = @"SaveInGameDirectory";
NSString * const kShowBezelsKey = @"ShowBezels";
NSString * const kShowLibraryKey = @"ShowLibrary";
NSString * const kAddToLibraryKey = @"AddToLibrary";
NSString * const kTerminationAlertSuppressionKey = @"TerminationAlertSuppression";
NSString * const kUseForAllAlertSuppressionKey = @"UseForAllAlertSuppression";
NSString * const kOverwriteStylesAlertSuppressionKey = @"OverwriteStylesAlertSuppression";
NSString * const kAutorestoreAlertSuppressionKey = @"AutorestoreAlertSuppression";
NSString * const kCloseAlertSuppressionKey = @"CloseAlertSuppression";
NSString * const kCommandScriptAlertSuppressionKey = @"CommandScriptAlertSuppression";
NSString * const kSaveFileAlertSuppressionKey = @"SaveFileAlertSuppression";
NSString * const kImageComparisonSuppressionKey = @"ImageComparisonSuppression";
NSString * const kVerifyAlertSuppressionKey = @"VerifyAlertSuppression";

NSArray<NSString *> *AlertSuppressionKeys(void) {
    return @[ kTerminationAlertSuppressionKey, kUseForAllAlertSuppressionKey, kOverwriteStylesAlertSuppressionKey, kAutorestoreAlertSuppressionKey, kCloseAlertSuppressionKey, kCommandScriptAlertSuppressionKey, kSaveFileAlertSuppressionKey, kImageComparisonSuppressionKey, kVerifyAlertSuppressionKey ];
}

void InitializeConstants(void) {
    gGameFileTypes = @[
        @"d$$", @"dat", @"sna",  @"z80",  @"tap",   @"tzx",   @"advsys",
        @"l9",  @"mag", @"a3c",  @"acd",  @"agx",   @"gam",   @"t3",
        @"hex", @"taf", @"z1",   @"z2",   @"z3",    @"z4",    @"z5",
        @"z6",  @"z7",  @"z8",   @"cas",  @"asl",   @"saga",  @"jacl",
        @"j2",  @"ulx", @"blb",  @"zlb",  @"blorb", @"glb",   @"gblorb",
        @"d64", @"t64", @"fiad", @"dsk",  @"quill", @"zblorb",@"atr",
        @"st",  @"msa", @"woz",  @"sag",  @"plus",  @"plu",   @"tay",
        @"taylor", @"rpk", @"cart", @"zip", @"acx", @"exe",
        @"quest", @"aslx"
    ];

    gDocFileTypes = @[@"rtf", @"rtfd", @"html", @"doc", @"docx", @"odt", @"xml", @"webarchive", @"txt"];

    gSaveFileTypes = @[@"glksave", @"sav", @"qut", @"qzl"];

    // To map the Glk style indices onto our Core Data relation names
    gBufferStyleNames = @[
                       @"bufferNormal",
                       @"bufEmph",
                       @"bufPre",
                       @"bufHead",
                       @"bufSubH",
                       @"bufAlert",
                       @"bufNote",
                       @"bufBlock",
                       @"bufInput",
                       @"bufUsr1",
                       @"bufUsr2",
                       ];

    gGridStyleNames = @[
                       @"gridNormal",
                       @"gridEmph",
                       @"gridPre",
                       @"gridHead",
                       @"gridSubH",
                       @"gridAlert",
                       @"gridNote",
                       @"gridBlock",
                       @"gridInput",
                       @"gridUsr1",
                       @"gridUsr2",
                       ];

    gExtMap = @{@"acd" : @"alan2", @"a3c" : @"alan3", @"d$$" : @"agility"};

    gFormatMap = @{
                   @"adrift" : @"scarier",
                   @"advsys" : @"advsys",
                   @"agt" : @"agility",
                   @"archetype" : @"archetype",
                   @"comprehend" : @"comprehend",
                   @"glulx" : @"glulxe",
                   @"hugo" : @"hugo",
                   @"level9" : @"level9",
                   @"magscrolls" : @"magnetic",
                   @"quill" : @"unquill",
                   @"tads2" : @"tadsr",
                   @"tads3" : @"tadsr",
                   @"zcode" : @"bocfel",
                   @"quest4" : @"question",
                   @"quest5" : @"question",
                   @"jacl" : @"jacl",
                   @"sagaplus" : @"plus",
                   @"scott" : @"scott",
                   @"taylor" : @"taylor"
                   };

    PasteboardFileURLPromise = (NSPasteboardType)kPasteboardTypeFileURLPromise;
    PasteboardFilePromiseContent = (NSPasteboardType)kPasteboardTypeFilePromiseContent;
    PasteboardFilePasteLocation = (NSPasteboardType)@"com.apple.pastelocation";
}
