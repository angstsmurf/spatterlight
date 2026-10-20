//
//  BuiltInThemes.m
//  Spatterlight
//
//  Created by Administrator on 2021-04-19.
//

#import "BuiltInThemes.h"
#import "Theme.h"
#import "Fetches.h"
#import "GlkStyle.h"
#import "NSColor+integer.h"

// The layout values every built-in theme sets
typedef struct {
    int32_t rows, cols;
    BOOL dashes, doStyles, smartQuotes;
    kSpacesFormatType spaceFormat;
    int32_t border;
    int32_t bufferMarginX, bufferMarginY;
    int32_t gridMarginX, gridMarginY;
    int32_t winSpacingX, winSpacingY;
    NSUnderlineStyle gridLinkStyle;
} BuiltInLayout;

@implementation BuiltInThemes

#pragma mark Builder helpers

+ (void)applyLayout:(BuiltInLayout)layout toTheme:(Theme *)theme {
    theme.dashes = layout.dashes;
    theme.defaultRows = layout.rows;
    theme.defaultCols = layout.cols;
    theme.doStyles = layout.doStyles;
    theme.smartQuotes = layout.smartQuotes;
    theme.spaceFormat = layout.spaceFormat;
    theme.border = layout.border;
    theme.bufferMarginX = layout.bufferMarginX;
    theme.bufferMarginY = layout.bufferMarginY;
    theme.gridMarginX = layout.gridMarginX;
    theme.gridMarginY = layout.gridMarginY;
    theme.winSpacingX = layout.winSpacingX;
    theme.winSpacingY = layout.winSpacingY;
    theme.gridLinkStyle = (int32_t)layout.gridLinkStyle;
}

// Zero baseline offset and the given maximum line height (0 = none) on the
// grid normal style. Call before populateStyles, which derives the other
// grid styles from it.
+ (void)setGridNormalOfTheme:(Theme *)theme maximumLineHeight:(CGFloat)height {
    [theme.gridNormal updateAttributes:^(NSMutableDictionary *attributes) {
        attributes[NSBaselineOffsetAttributeName] = @(0);
    }];
    [theme.gridNormal updateParagraphStyle:^(NSMutableParagraphStyle *para) {
        para.maximumLineHeight = height;
    }];
}

// Copy the grid and buffer cell sizes out of the normal styles. Call last.
+ (void)refreshCellSizesOfTheme:(Theme *)theme {
    NSSize size = theme.gridNormal.cellSize;
    theme.cellHeight = size.height;
    theme.cellWidth = size.width;

    size = theme.bufferNormal.cellSize;
    theme.bufferCellHeight = size.height;
    theme.bufferCellWidth = size.width;
}

+ (NSFont *)boldVersionOf:(NSFont *)font {
    return [[NSFontManager sharedFontManager] convertWeight:YES ofFont:font];
}

#pragma mark Themes

+ (void)createBuiltInThemesInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {
    [BuiltInThemes createDefaultThemeInContext:context forceRebuild:force];
    [BuiltInThemes createZoomThemeInContext:context forceRebuild:force];
    [BuiltInThemes createClassicSpatterlightThemeInContext:context forceRebuild:force];
    [BuiltInThemes createDOSThemeInContext:context forceRebuild:force];
    [BuiltInThemes createDOSBoxThemeInContext:context forceRebuild:force];
    [BuiltInThemes createLectroteThemeInContext:context forceRebuild:force];
    [BuiltInThemes createLectroteDarkThemeInContext:context forceRebuild:force];
    [BuiltInThemes createGargoyleThemeInContext:context forceRebuild:force];
    [BuiltInThemes createMontserratThemeInContext:context forceRebuild:force];
    [BuiltInThemes createAutomapThemeInContext:context forceRebuild:force];
}

// A missing defaults key gives a nil name, which fontWithName:size: must not get.
// The callers fall back to another font when this returns nil.
static NSFont *fontNamedInDefaults(NSString *name, CGFloat size) {
    if (![name isKindOfClass:[NSString class]] || !name.length)
        return nil;
    return [NSFont fontWithName:name size:size];
}

// Imports the pre-themes preferences once, for a user with no saved theme
// (see Preferences readDefaults). Old settings is then the user's to edit or
// delete, so it is never rebuilt: a forced rebuild of the built-in themes
// used to overwrite their edits, recreate it after deletion, or delete it
// when the old defaults matched Spatterlight Classic.
+ (nullable Theme *)createThemeFromDefaultsPlistInContext:(NSManagedObjectContext *)context {
    BOOL exists = NO;
    Theme *oldTheme = [BuiltInThemes findOrCreateTheme:@"Old settings" inContext:context alreadyExists:&exists];
    if (exists)
        return oldTheme;

    // Built, not just found or created: a bare find-or-create here left a
    // blank Spatterlight Classic that the Classic builder below then took as
    // existing and skipped, unless the rebuild was forced
    Theme *defaultTheme = [BuiltInThemes createClassicSpatterlightThemeInContext:context forceRebuild:NO];

    BOOL foundDifference = NO;

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *name;
    CGFloat size;

    oldTheme.defaultCols = [[defaults objectForKey:@"DefaultWidth"] intValue];
    oldTheme.defaultRows = [[defaults objectForKey:@"DefaultHeight"] intValue];

    oldTheme.smartQuotes = [[defaults objectForKey:@"SmartQuotes"] boolValue];
    oldTheme.spaceFormat = (kSpacesFormatType)[[defaults objectForKey:@"SpaceFormat"] intValue];

    oldTheme.doGraphics = [[defaults objectForKey:@"EnableGraphics"] boolValue];
    oldTheme.doSound = [[defaults objectForKey:@"EnableSound"] boolValue];
    oldTheme.doStyles = [[defaults objectForKey:@"EnableStyles"] boolValue];

    oldTheme.gridMarginX = (int32_t)[[defaults objectForKey:@"GridMargin"] doubleValue];
    oldTheme.gridMarginY = oldTheme.gridMarginX;
    oldTheme.bufferMarginX = (int32_t)[[defaults objectForKey:@"BufferMargin"] doubleValue];
    oldTheme.bufferMarginY = oldTheme.bufferMarginX;

    oldTheme.border = (int32_t)[[defaults objectForKey:@"Border"] doubleValue];

    oldTheme.editable = YES;

    name = [defaults objectForKey:@"GridFontName"];

    if (![defaultTheme.gridNormal.font.fontName isEqualToString:name])
        foundDifference = YES;

    size = [[defaults objectForKey:@"GridFontSize"] doubleValue];
    if (defaultTheme.gridNormal.font.pointSize != size)
        foundDifference = YES;

    oldTheme.gridNormal.font = fontNamedInDefaults(name, size);

    if (!oldTheme.gridNormal.font) {
        NSLog(@"pref: failed to create grid font '%@'", name);
        oldTheme.gridNormal.font = [NSFont userFontOfSize:0];
    }

    oldTheme.gridBackground = [NSColor colorFromData:[defaults objectForKey:@"GridBackground"]];
    oldTheme.gridNormal.color = [NSColor colorFromData:[defaults objectForKey:@"GridForeground"]];

    name = [defaults objectForKey:@"BufferFontName"];

    if (![defaultTheme.bufferNormal.font.fontName isEqualToString:name])
        foundDifference = YES;

    size = [[defaults objectForKey:@"BufferFontSize"] doubleValue];

    if (defaultTheme.bufferNormal.font.pointSize != size)
        foundDifference = YES;

    oldTheme.bufferNormal.font = fontNamedInDefaults(name, size);
    if (!oldTheme.bufferNormal.font) {
        NSLog(@"pref: failed to create buffer font '%@'", name);
        oldTheme.bufferNormal.font = [NSFont userFontOfSize:0];
    }
    oldTheme.bufferBackground = [NSColor colorFromData:[defaults objectForKey:@"BufferBackground"]];
    oldTheme.bufferNormal.color = [NSColor colorFromData:[defaults objectForKey:@"BufferForeground"]];

    name = [defaults objectForKey:@"InputFontName"];
    if (![defaultTheme.bufInput.font.fontName isEqualToString:name]) {
        foundDifference = YES;
    }

    size = [[defaults objectForKey:@"InputFontSize"] doubleValue];
    if (defaultTheme.bufInput.font.pointSize != size)
        foundDifference = YES;

    oldTheme.bufInput.font = fontNamedInDefaults(name, size);

    NSFont *bufbold = [BuiltInThemes boldVersionOf:oldTheme.bufferNormal.font];

    if (!oldTheme.bufInput.font) {
        NSLog(@"pref: failed to create input font '%@'", name);
        oldTheme.bufInput.font = bufbold;
    }

    oldTheme.bufInput.color = [NSColor colorFromData:[defaults objectForKey:@"InputColor"]];
    oldTheme.bufInput.autogenerated = NO;

    oldTheme.gridNormal.lineSpacing = 0;

    [BuiltInThemes refreshCellSizesOfTheme:oldTheme];

    [oldTheme populateStyles];

    oldTheme.defaultParent = [BuiltInThemes createDefaultThemeInContext:context forceRebuild:NO];

    if (!foundDifference) {
        NSDictionary *attributesToDefaultsDict =
        @{ @"defaultCols": @"DefaultWidth",
           @"defaultRows": @"DefaultHeight",
           @"smartQuotes": @"SmartQuotes",
           @"spaceFormat": @"SpaceFormat",
           @"doGraphics": @"EnableGraphics",
           @"doSound": @"EnableSound",
           @"doStyles": @"EnableStyles",
           @"gridMarginX": @"GridMargin",
           @"bufferMarginX": @"BufferMargin",
           @"border": @"Border",
        };

        // Only the attributes above have a defaults key. Looking up the
        // others would pass a nil key to objectForKey:, which crashes
        // ("CFHash() called with NULL") on macOS 10.13 and earlier.
        for (NSString *attr in attributesToDefaultsDict) {
            id defaultsValue = [defaults objectForKey:attributesToDefaultsDict[attr]];

            if (defaultsValue == nil) {
                continue;
            }

            id value = [oldTheme valueForKey:attr];
            if (value == nil)
                value = [oldTheme.defaultParent valueForKey:attr];

            if ([value isKindOfClass:[NSColor class]]) {
                continue;
            }

            if (value && ![value isEqual:defaultsValue]) {
                if ([value isKindOfClass:[NSNumber class]] &&
                    ((NSNumber *)value).intValue == ((NSString *)defaultsValue).intValue) {
                    continue;
                }
                foundDifference = YES;
                break;
            }
        }
        if (!foundDifference) {
            [context deleteObject:oldTheme];
            return nil;
        }
    }
    return oldTheme;
}

+ (Theme *)createDefaultThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {

    BOOL exists = NO;
    Theme *defaultTheme = [BuiltInThemes findOrCreateTheme:@"Default" inContext:context alreadyExists:&exists];

    [defaultTheme resetCommonValues];

    if (exists && !force)
        return defaultTheme;

    defaultTheme.bZAdjustment = -2;
    [BuiltInThemes applyLayout:(BuiltInLayout){
        .rows = 50, .cols = 80,
        .dashes = YES, .doStyles = YES, .smartQuotes = YES,
        .spaceFormat = TAG_SPACES_GAME, .border = 0,
        .bufferMarginX = 15, .bufferMarginY = 15,
        .gridMarginX = 5, .gridMarginY = 5,
        .winSpacingX = 0, .winSpacingY = 0,
        .gridLinkStyle = NSUnderlineStyleSingle
    } toTheme:defaultTheme];

    defaultTheme.gridNormal.font = [NSFont fontWithName:@"Source Code Pro" size:11];
    defaultTheme.gridBackground = [NSColor blackColor];
    defaultTheme.gridNormal.color = [NSColor colorWithDeviceRed:0.847 green:0.847 blue:0.847 alpha:1.0];

    defaultTheme.bufferBackground = [NSColor whiteColor];

    defaultTheme.bufInput.color = [NSColor colorWithDeviceRed:0.137 green:0.431 blue:0.145 alpha:1.0];
    defaultTheme.bufInput.autogenerated = NO;

    defaultTheme.bufferNormal.lineSpacing = 1;

    [BuiltInThemes setGridNormalOfTheme:defaultTheme maximumLineHeight:0];

    [defaultTheme populateStyles];

    [BuiltInThemes refreshCellSizesOfTheme:defaultTheme];

    return defaultTheme;
}

+ (Theme *)createClassicSpatterlightThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {

    BOOL exists = NO;
    Theme *classicTheme = [BuiltInThemes findOrCreateTheme:@"Spatterlight Classic" inContext:context alreadyExists:&exists];

    [classicTheme resetCommonValues];

    if (exists && !force)
        return classicTheme;

    classicTheme.bZAdjustment = 0;

    [BuiltInThemes applyLayout:(BuiltInLayout){
        .rows = 50, .cols = 80,
        .dashes = NO, .doStyles = YES, .smartQuotes = YES,
        .spaceFormat = TAG_SPACES_ONE, .border = 0,
        .bufferMarginX = 15, .bufferMarginY = 15,
        .gridMarginX = 5, .gridMarginY = 5,
        .winSpacingX = 0, .winSpacingY = 0,
        .gridLinkStyle = NSUnderlineStyleSingle
    } toTheme:classicTheme];

    classicTheme.gridNormal.font = [NSFont fontWithName:@"Monaco" size:12];

    classicTheme.gridBackground = [NSColor blackColor];
    classicTheme.gridNormal.color = [NSColor colorWithDeviceRed:0.847 green:0.847 blue:0.847 alpha:1.0];

    [BuiltInThemes setGridNormalOfTheme:classicTheme maximumLineHeight:12];

    classicTheme.bufferNormal.font = [NSFont fontWithName:@"Helvetica" size:13];

    classicTheme.bufferBackground = [NSColor whiteColor];

    classicTheme.bufInput.font = [NSFont fontWithName:@"Helvetica-Bold" size:13];
    classicTheme.bufInput.color = [NSColor colorWithDeviceRed:0.137 green:0.431 blue:0.145 alpha:1.0];
    classicTheme.bufInput.autogenerated = NO;

    [classicTheme populateStyles];

    [BuiltInThemes refreshCellSizesOfTheme:classicTheme];

    return classicTheme;
}

+ (Theme *)createGargoyleThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {

    BOOL exists = NO;
    Theme *gargoyleTheme = [BuiltInThemes findOrCreateTheme:@"Gargoyle" inContext:context alreadyExists:&exists];

    [gargoyleTheme resetCommonValues];

    if (exists && !force)
        return gargoyleTheme;

    gargoyleTheme.bZAdjustment = 0;

    [BuiltInThemes applyLayout:(BuiltInLayout){
        .rows = 30, .cols = 62,
        .dashes = YES, .doStyles = YES, .smartQuotes = YES,
        .spaceFormat = TAG_SPACES_GAME, .border = 20,
        .bufferMarginX = 3, .bufferMarginY = 7,
        .gridMarginX = 0, .gridMarginY = 3,
        .winSpacingX = 0, .winSpacingY = 0,
        .gridLinkStyle = NSUnderlineStyleSingle
    } toTheme:gargoyleTheme];

    gargoyleTheme.gridBackground = [NSColor whiteColor];
    gargoyleTheme.bufferBackground = [NSColor whiteColor];

    gargoyleTheme.bufferNormal.font = [NSFont fontWithName:@"Linux Libertine O" size:15.5];
    gargoyleTheme.bufferNormal.lineSpacing = 2;

    gargoyleTheme.bufInput.font = [BuiltInThemes boldVersionOf:gargoyleTheme.bufferNormal.font];
    gargoyleTheme.bufInput.color = [NSColor colorWithDeviceRed:0.100 green:0.587 blue:0.275 alpha:1.0];
    gargoyleTheme.bufInput.autogenerated = NO;

    gargoyleTheme.gridNormal.font = [NSFont fontWithName:@"Liberation Mono" size:12.5];
    gargoyleTheme.gridNormal.color = [NSColor colorWithDeviceRed:0.376 green:0.376 blue:0.376 alpha:1.0];

    [BuiltInThemes setGridNormalOfTheme:gargoyleTheme maximumLineHeight:14];

    [gargoyleTheme populateStyles];

    gargoyleTheme.bufHead.font = [NSFont fontWithName:@"Linux Libertine O Bold" size:15.5];
    gargoyleTheme.bufHead.autogenerated = NO;
    gargoyleTheme.bufSubH.font = [NSFont fontWithName:@"Linux Libertine O Bold" size:15.5];
    gargoyleTheme.bufSubH.autogenerated = NO;

    [BuiltInThemes refreshCellSizesOfTheme:gargoyleTheme];

    return gargoyleTheme;
}

+ (Theme *)createLectroteThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {
    BOOL exists = NO;
    Theme *lectroteTheme = [BuiltInThemes findOrCreateTheme:@"Lectrote" inContext:context alreadyExists:&exists];

    [lectroteTheme resetCommonValues];

    if (exists && !force)
        return lectroteTheme;

    lectroteTheme.bZAdjustment = -4;

    [BuiltInThemes applyLayout:(BuiltInLayout){
        .rows = 40, .cols = 100,
        .dashes = YES, .doStyles = YES, .smartQuotes = YES,
        .spaceFormat = TAG_SPACES_GAME, .border = 20,
        .bufferMarginX = 20, .bufferMarginY = 15,
        .gridMarginX = 15, .gridMarginY = 6,
        .winSpacingX = 0, .winSpacingY = 10,
        .gridLinkStyle = NSUnderlineStyleSingle
    } toTheme:lectroteTheme];

    lectroteTheme.gridBackground = [NSColor colorWithDeviceRed:0.450844 green:0.325858 blue:0.205177 alpha:1];
    lectroteTheme.bufferBackground = [NSColor whiteColor];

    lectroteTheme.bufferNormal.font = [NSFont fontWithName:@"Lora" size:15];
    lectroteTheme.bufferNormal.lineSpacing = 3.2;

    lectroteTheme.bufInput.font = [BuiltInThemes boldVersionOf:lectroteTheme.bufferNormal.font];

    lectroteTheme.bufInput.color = [NSColor colorWithDeviceRed:0.042041 green:0.333368 blue:0.011031 alpha:1];
    lectroteTheme.bufInput.autogenerated = NO;

    lectroteTheme.gridNormal.font = [NSFont fontWithName:@"Source Code Pro" size:14];
    lectroteTheme.gridNormal.color = [NSColor colorWithDeviceRed:0.916565 green:0.902161 blue:0.839754 alpha:1];

    [BuiltInThemes setGridNormalOfTheme:lectroteTheme maximumLineHeight:0];

    [BuiltInThemes refreshCellSizesOfTheme:lectroteTheme];

    [lectroteTheme populateStyles];

    return lectroteTheme;
}

+ (Theme *)createLectroteDarkThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {
    BOOL exists = NO;
    Theme *lectroteDarkTheme = [BuiltInThemes findOrCreateTheme:@"Lectrote Dark" inContext:context alreadyExists:&exists];

    [lectroteDarkTheme resetCommonValues];

    if (exists && !force)
        return lectroteDarkTheme;

    lectroteDarkTheme.bZAdjustment = -4;

    [BuiltInThemes applyLayout:(BuiltInLayout){
        .rows = 40, .cols = 100,
        .dashes = YES, .doStyles = YES, .smartQuotes = YES,
        .spaceFormat = TAG_SPACES_GAME, .border = 20,
        .bufferMarginX = 20, .bufferMarginY = 15,
        .gridMarginX = 15, .gridMarginY = 6,
        .winSpacingX = 0, .winSpacingY = 10,
        .gridLinkStyle = NSUnderlineStyleSingle
    } toTheme:lectroteDarkTheme];

    lectroteDarkTheme.gridBackground = [NSColor colorWithDeviceRed:0.991 green:0.957 blue:0.937 alpha:1];
    lectroteDarkTheme.bufferBackground = [NSColor blackColor];

    lectroteDarkTheme.gridNormal.font = [NSFont fontWithName:@"Source Code Pro" size:14];
    lectroteDarkTheme.gridNormal.color = [NSColor colorWithDeviceRed:0.258 green:0.205 blue:0.145 alpha:1];
    lectroteDarkTheme.bufferNormal.font = [NSFont fontWithName:@"Lora" size:15];
    lectroteDarkTheme.bufferNormal.color = [NSColor colorWithDeviceRed:0.991 green:0.957 blue:0.937 alpha:1];

    lectroteDarkTheme.bufferNormal.lineSpacing = 3.2;

    lectroteDarkTheme.bufInput.font = [BuiltInThemes boldVersionOf:lectroteDarkTheme.bufferNormal.font];
    lectroteDarkTheme.bufInput.color = [NSColor colorWithDeviceRed:0.842 green:0.994 blue:0.820 alpha:1];
    lectroteDarkTheme.bufInput.autogenerated = NO;

    [BuiltInThemes setGridNormalOfTheme:lectroteDarkTheme maximumLineHeight:0];

    [BuiltInThemes refreshCellSizesOfTheme:lectroteDarkTheme];

    [lectroteDarkTheme populateStyles];

    return lectroteDarkTheme;
}

+ (Theme *)createZoomThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {
    BOOL exists = NO;
    Theme *zoomTheme = [BuiltInThemes findOrCreateTheme:@"Zoom" inContext:context alreadyExists:&exists];

    [zoomTheme resetCommonValues];

    if (exists && !force)
        return zoomTheme;

    zoomTheme.bZAdjustment = 0;

    [BuiltInThemes applyLayout:(BuiltInLayout){
        .rows = 50, .cols = 92,
        .dashes = YES, .doStyles = YES, .smartQuotes = YES,
        .spaceFormat = TAG_SPACES_GAME, .border = 0,
        .bufferMarginX = 10, .bufferMarginY = 12,
        .gridMarginX = 0, .gridMarginY = 0,
        .winSpacingX = 0, .winSpacingY = 0,
        .gridLinkStyle = NSUnderlineStyleSingle
    } toTheme:zoomTheme];

    zoomTheme.gridBackground = [NSColor colorWithDeviceRed:1 green:0.996 blue:0.820 alpha:1];
    zoomTheme.bufferBackground = [NSColor colorWithDeviceRed:1 green:0.996 blue:0.820 alpha:1];

    zoomTheme.bufferNormal.font = [NSFont fontWithName:@"Gill Sans" size:12];

    NSFont *gillSansBold = [[NSFontManager sharedFontManager] convertFont:zoomTheme.bufferNormal.font toFace:@"GillSans-Bold"];

    zoomTheme.bufInput.font = [gillSansBold copy];
    zoomTheme.bufInput.autogenerated = NO;

    zoomTheme.gridNormal.font = [NSFont fontWithName:@"Courier" size:12];
    zoomTheme.gridNormal.color = [NSColor blackColor];

    [BuiltInThemes setGridNormalOfTheme:zoomTheme maximumLineHeight:12];

    [BuiltInThemes refreshCellSizesOfTheme:zoomTheme];

    [zoomTheme populateStyles];

    [zoomTheme.bufBlock updateParagraphStyle:^(NSMutableParagraphStyle *para) {
        para.headIndent = 10;
        para.firstLineHeadIndent = 10;
    }];
    zoomTheme.bufBlock.font = [[NSFontManager sharedFontManager] convertFont:zoomTheme.bufBlock.font toHaveTrait:NSBoldFontMask];
    zoomTheme.bufBlock.autogenerated = NO;

    zoomTheme.bufSubH.font = [[NSFontManager sharedFontManager] convertFont:gillSansBold toSize:13];
    //I'm sure this line can't be necessay, but every time I change it reverts to semibold
    zoomTheme.bufSubH.font = [[NSFontManager sharedFontManager] convertFont:zoomTheme.bufSubH.font toFace:@"GillSans-Bold"];
    zoomTheme.bufSubH.autogenerated = NO;

    zoomTheme.bufHead.font = [NSFont fontWithName:@"Gill Sans" size:16];
    [zoomTheme.bufHead updateParagraphStyle:^(NSMutableParagraphStyle *para) {
        para.alignment = NSTextAlignmentCenter;
    }];
    zoomTheme.bufHead.autogenerated = NO;

    return zoomTheme;
}

+ (Theme *)createDOSThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {
    BOOL exists = NO;
    Theme *dosTheme = [BuiltInThemes findOrCreateTheme:@"MS-DOS" inContext:context alreadyExists:&exists];

    [dosTheme resetCommonValues];

    if (exists && !force)
        return dosTheme;

    dosTheme.bZAdjustment = 0;

    [BuiltInThemes applyLayout:(BuiltInLayout){
        .rows = 24, .cols = 80,
        .dashes = NO, .doStyles = YES, .smartQuotes = NO,
        .spaceFormat = TAG_SPACES_GAME, .border = 20,
        .bufferMarginX = 0, .bufferMarginY = 0,
        .gridMarginX = 0, .gridMarginY = 0,
        .winSpacingX = 0, .winSpacingY = 0,
        .gridLinkStyle = NSUnderlineStyleSingle
    } toTheme:dosTheme];

    dosTheme.gridBackground =  [NSColor blackColor];
    dosTheme.bufferBackground = [NSColor blackColor];

    dosTheme.gridNormal.font = [NSFont fontWithName:@"PxPlus IBM CGA-2y" size:18];
    dosTheme.gridNormal.color = [NSColor colorWithDeviceRed:0.512756 green:0.512821  blue:0.512721 alpha:1];

    dosTheme.bufferNormal.font = [NSFont fontWithName:@"PxPlus IBM CGA-2y" size:18];
    dosTheme.bufferNormal.color = [NSColor colorWithDeviceRed:0.512756 green:0.512821  blue:0.512721 alpha:1];
    dosTheme.bufInput.font = [NSFont fontWithName:@"PxPlus IBM CGA-2y" size:18];
    dosTheme.bufInput.color = [NSColor colorWithDeviceRed:0.512756 green:0.512821  blue:0.512721 alpha:1];
    dosTheme.bufInput.autogenerated = NO;

    [BuiltInThemes refreshCellSizesOfTheme:dosTheme];

    [dosTheme.gridNormal updateAttributes:^(NSMutableDictionary *attributes) {
        attributes[NSBaselineOffsetAttributeName] = @(0);
    }];
    [dosTheme.gridNormal updateParagraphStyle:^(NSMutableParagraphStyle *para) {
        para.lineSpacing = 0;
        para.paragraphSpacing = 0;
        para.paragraphSpacingBefore = 0;
        para.maximumLineHeight = 18.2;
    }];

    [dosTheme populateStyles];

    dosTheme.bufHead.font = [NSFont fontWithName:@"PxPlus IBM CGA-2y" size:18];
    dosTheme.bufHead.color = [NSColor whiteColor];
    dosTheme.bufHead.autogenerated = NO;
    dosTheme.bufSubH.font = [NSFont fontWithName:@"PxPlus IBM CGA-2y" size:18];
    dosTheme.bufSubH.color = [NSColor whiteColor];
    dosTheme.bufSubH.autogenerated = NO;
    dosTheme.bufEmph.font = [NSFont fontWithName:@"PxPlus IBM CGA-2y-UC" size:18];
    dosTheme.bufEmph.autogenerated = NO;
    dosTheme.gridEmph.font = [NSFont fontWithName:@"PxPlus IBM CGA-2y-UC" size:18];
    dosTheme.gridEmph.autogenerated = NO;
    dosTheme.gridSubH.color = [NSColor whiteColor];
    dosTheme.gridSubH.autogenerated = NO;

    return dosTheme;
}

+ (Theme *)createDOSBoxThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {
    BOOL exists = NO;
    Theme *dosBoxTheme = [BuiltInThemes findOrCreateTheme:@"DOSBox" inContext:context alreadyExists:&exists];

    [dosBoxTheme resetCommonValues];

    if (exists && !force)
        return dosBoxTheme;

    dosBoxTheme.bZAdjustment = -2;

    [BuiltInThemes applyLayout:(BuiltInLayout){
        .rows = 24, .cols = 80,
        .dashes = NO, .doStyles = YES, .smartQuotes = NO,
        .spaceFormat = TAG_SPACES_GAME, .border = 20,
        .bufferMarginX = 0, .bufferMarginY = 0,
        .gridMarginX = 0, .gridMarginY = 0,
        .winSpacingX = 0, .winSpacingY = 0,
        .gridLinkStyle = NSUnderlineStyleSingle
    } toTheme:dosBoxTheme];

    dosBoxTheme.gridBackground = [NSColor colorWithDeviceRed:0.008897 green:0  blue:0.633764 alpha:1];
    dosBoxTheme.bufferBackground = [NSColor colorWithDeviceRed:0.008897 green:0  blue:0.633764 alpha:1];

    dosBoxTheme.gridNormal.font = [NSFont fontWithName:@"PxPlus VGA SquarePX" size:24];
    dosBoxTheme.gridNormal.color = [NSColor colorWithDeviceRed:0.602654 green:0.602749  blue:0.602620 alpha:1];

    dosBoxTheme.bufferNormal.font = [NSFont fontWithName:@"PxPlus VGA SquarePX" size:24];
    dosBoxTheme.bufferNormal.color = [NSColor colorWithDeviceRed:0.602654 green:0.602749  blue:0.602620 alpha:1];

    dosBoxTheme.bufInput.font = [NSFont fontWithName:@"PxPlus VGA SquarePX" size:24];
    dosBoxTheme.bufInput.color = [NSColor colorWithDeviceRed:0.602654 green:0.602749  blue:0.602620 alpha:1];
    dosBoxTheme.bufInput.autogenerated = NO;

    [BuiltInThemes setGridNormalOfTheme:dosBoxTheme maximumLineHeight:0];

    [dosBoxTheme populateStyles];

    dosBoxTheme.bufHead.font = [NSFont fontWithName:@"PxPlus VGA SquarePX" size:24];
    dosBoxTheme.bufHead.color = [NSColor whiteColor];
    dosBoxTheme.bufHead.autogenerated = NO;
    dosBoxTheme.bufSubH.font = [NSFont fontWithName:@"PxPlus VGA SquarePX" size:24];
    dosBoxTheme.bufSubH.color = [NSColor whiteColor];
    dosBoxTheme.bufSubH.autogenerated = NO;
    dosBoxTheme.bufEmph.font = [NSFont fontWithName:@"PxPlus VGA SquarePX UC" size:24];
    dosBoxTheme.bufEmph.autogenerated = NO;
    dosBoxTheme.gridEmph.font = [NSFont fontWithName:@"PxPlus VGA SquarePX UC" size:24];
    dosBoxTheme.gridEmph.autogenerated = NO;
    dosBoxTheme.gridSubH.color = [NSColor whiteColor];
    dosBoxTheme.gridSubH.autogenerated = NO;

    [BuiltInThemes refreshCellSizesOfTheme:dosBoxTheme];

    return dosBoxTheme;
}

+ (Theme *)createMontserratThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {
    BOOL exists = NO;
    Theme *montserratTheme = [BuiltInThemes findOrCreateTheme:@"Montserrat" inContext:context alreadyExists:&exists];

    [montserratTheme resetCommonValues];

    if (exists && !force)
        return montserratTheme;

    [BuiltInThemes applyLayout:(BuiltInLayout){
        .rows = 50, .cols = 65,
        .dashes = NO, .doStyles = NO, .smartQuotes = YES,
        .spaceFormat = TAG_SPACES_ONE, .border = 0,
        .bufferMarginX = 60, .bufferMarginY = 60,
        .gridMarginX = 40, .gridMarginY = 10,
        .winSpacingX = 0, .winSpacingY = 0,
        .gridLinkStyle = NSUnderlineStyleSingle
    } toTheme:montserratTheme];

    montserratTheme.gridBackground = [NSColor whiteColor];
    montserratTheme.bufferBackground = [NSColor whiteColor];

    montserratTheme.gridNormal.font = [NSFont fontWithName:@"PT Sans Narrow" size:15];
    montserratTheme.gridNormal.color = [NSColor blackColor];

    montserratTheme.bufferNormal.font = [NSFont fontWithName:@"Montserrat Regular" size:15];
    montserratTheme.bufferNormal.lineSpacing = 15;

    montserratTheme.bufInput.font = [NSFont fontWithName:@"Montserrat ExtraBold Italic" size:15];
    montserratTheme.bufInput.autogenerated = NO;

    montserratTheme.bufBlock.font = [NSFont fontWithName:@"Montserrat ExtraLight Italic" size:15];
    montserratTheme.bufBlock.autogenerated = NO;

    [montserratTheme populateStyles];

    montserratTheme.bufHead.font = [NSFont fontWithName:@"Montserrat Black" size:30];
    montserratTheme.bufHead.lineSpacing = 0;

    [montserratTheme.bufHead updateAttributes:^(NSMutableDictionary *attributes) {
        attributes[NSBaselineOffsetAttributeName] = @(0);
    }];
    [montserratTheme.bufHead updateParagraphStyle:^(NSMutableParagraphStyle *para) {
        para.lineSpacing = 0;
        para.paragraphSpacing = 15;
        para.paragraphSpacingBefore = 0;
        para.maximumLineHeight = 30;
    }];
    montserratTheme.bufHead.autogenerated = NO;

    montserratTheme.bufSubH.font = [NSFont fontWithName:@"Montserrat ExtraBold" size:20];
    montserratTheme.bufSubH.lineSpacing = 10;

    [montserratTheme.bufSubH updateParagraphStyle:^(NSMutableParagraphStyle *para) {
        para.lineSpacing = 10;
        para.paragraphSpacing = 0;
        para.paragraphSpacingBefore = 0;
        para.maximumLineHeight = 30;
    }];
    montserratTheme.bufSubH.autogenerated = NO;

    [montserratTheme.bufBlock updateParagraphStyle:^(NSMutableParagraphStyle *para) {
        para.headIndent = 10;
        para.firstLineHeadIndent = 10;
        para.alignment = NSTextAlignmentJustified;
    }];
    montserratTheme.bufBlock.autogenerated = NO;

    [BuiltInThemes refreshCellSizesOfTheme:montserratTheme];

    return montserratTheme;
}

+ (Theme *)createAutomapThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {
    BOOL exists = NO;
    Theme *autoMapTheme = [BuiltInThemes findOrCreateTheme:@"Automap" inContext:context alreadyExists:&exists];

    [autoMapTheme resetCommonValues];

    if (exists && !force)
        return autoMapTheme;

    [BuiltInThemes applyLayout:(BuiltInLayout){
        .rows = 59, .cols = 80,
        .dashes = YES, .doStyles = YES, .smartQuotes = YES,
        .spaceFormat = TAG_SPACES_ONE, .border = 0,
        .bufferMarginX = 20, .bufferMarginY = 30,
        .gridMarginX = 20, .gridMarginY = 10,
        .winSpacingX = 0, .winSpacingY = 0,
        .gridLinkStyle = NSUnderlineStyleNone
    } toTheme:autoMapTheme];

    autoMapTheme.gridBackground = [NSColor whiteColor];
    autoMapTheme.bufferBackground = [NSColor whiteColor];

    autoMapTheme.gridNormal.font = [NSFont fontWithName:@"Source Code Pro" size:16];
    autoMapTheme.gridNormal.color = [NSColor blackColor];

    [autoMapTheme.gridNormal updateAttributes:^(NSMutableDictionary *attributes) {
        attributes[NSKernAttributeName] = @(0);
    }];
    [autoMapTheme.gridNormal updateParagraphStyle:^(NSMutableParagraphStyle *para) {
        para.maximumLineHeight = 16;
        para.lineSpacing = 0;
    }];

    autoMapTheme.bufferNormal.font = [NSFont fontWithName:@"Helvetica" size:13];

    [autoMapTheme populateStyles];

    [BuiltInThemes refreshCellSizesOfTheme:autoMapTheme];

    return autoMapTheme;
}

+ (Theme *)findOrCreateTheme:(NSString *)themeName inContext:(NSManagedObjectContext *)context alreadyExists:(BOOL *)existsFlagPointer {

    // First, check if it already exists
    Theme *existing = [Fetches findTheme:themeName inContext:context];
    *existsFlagPointer = (existing != nil);
    if (existing)
        return existing;

    Theme *newTheme = (Theme *) [NSEntityDescription
                                 insertNewObjectForEntityForName:@"Theme"
                                 inManagedObjectContext:context];

    newTheme.name = themeName;

    [newTheme populateStyles];

    return newTheme;
}

@end
