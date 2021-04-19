//
//  BuiltInThemes.m
//  Spatterlight
//
//  Created by Administrator on 2021-04-19.
//

#import "BuiltInThemes.h"
#import "Theme.h"
#import "GlkStyle.h"
#import "NSColor+integer.h"
#import "Preferences.h"

@implementation BuiltInThemes

+ (void)checkThemesInContext:(NSManagedObjectContext *)context {
    //Loop through all attributes of theme and assign them to me
    NSDictionary *attributes = [[NSEntityDescription
                                 entityForName:@"Theme"
                                 inManagedObjectContext:context] attributesByName];

    NSError *error = nil;
    NSFetchRequest *fetchRequest = [NSFetchRequest new];
    fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:context];

    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"editable = NO"];
    NSArray *fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];

    if (!fetchedObjects || !fetchedObjects.count) {
        NSLog(@"No built-in themes found!");
    } else NSLog(@"%ld built-in themes found!", fetchedObjects.count);

    if (error != nil) {
        NSLog(@"BuiltInThemes checkThemesInContext: %@", error);
    }


    for (NSString *attr in attributes) {
        id value = [fetchedObjects[0] valueForKey:attr];
        BOOL same = YES;
        for (Theme *theme in fetchedObjects) {
            if (![[theme valueForKey:attr] isEqual:value]) {
                same = NO;
                break;
            }
        }

        if (same)
            NSLog(@"%@ (%@)", attr, value);
    }
}

+ (void)createBuiltInThemesInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {
    [BuiltInThemes createThemeFromDefaultsPlistInContext:context forceRebuild:force];
    [BuiltInThemes createDefaultThemeInContext:context forceRebuild:force];
    [BuiltInThemes createZoomThemeInContext:context forceRebuild:force];
    [BuiltInThemes createClassicSpatterlightThemeInContext:context forceRebuild:force];
    [BuiltInThemes createDOSThemeInContext:context forceRebuild:force];
    [BuiltInThemes createDOSBoxThemeInContext:context forceRebuild:force];
    [BuiltInThemes createLectroteThemeInContext:context forceRebuild:force];
    [BuiltInThemes createLectroteDarkThemeInContext:context forceRebuild:force];
    [BuiltInThemes createGargoyleThemeInContext:context forceRebuild:force];
    [BuiltInThemes createMontserratThemeInContext:context forceRebuild:force];
    //    [BuiltInThemes createSTThemeInContext:managedObjectContext forceRebuild:force];
    //    [BuiltInThemes checkThemesInContext:context];
}

+ (Theme *)createThemeFromDefaultsPlistInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {

    BOOL exists = NO;
    Theme *oldTheme = [BuiltInThemes findOrCreateTheme:@"Old settings" inContext:context alreadyExists:&exists];
    if (exists && !force)
        return oldTheme;

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *name;
    CGFloat size;

    oldTheme.defaultCols = [[defaults objectForKey:@"DefaultWidth"] intValue];
    oldTheme.defaultRows = [[defaults objectForKey:@"DefaultHeight"] intValue];

    oldTheme.smartQuotes = [[defaults objectForKey:@"SmartQuotes"] boolValue];
    oldTheme.spaceFormat = [[defaults objectForKey:@"SpaceFormat"] intValue];

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
    size = [[defaults objectForKey:@"GridFontSize"] doubleValue];
    oldTheme.gridNormal.font = [NSFont fontWithName:name size:size];

    if (!oldTheme.gridNormal.font) {
        NSLog(@"pref: failed to create grid font '%@'", name);
        oldTheme.gridNormal.font = [NSFont userFontOfSize:0];
    }

    oldTheme.gridBackground = [NSColor colorFromData:[defaults objectForKey:@"GridBackground"]];
    oldTheme.gridNormal.color = [NSColor colorFromData:[defaults objectForKey:@"GridForeground"]];

    name = [defaults objectForKey:@"BufferFontName"];
    size = [[defaults objectForKey:@"BufferFontSize"] doubleValue];
    oldTheme.bufferNormal.font = [NSFont fontWithName:name size:size];
    if (!oldTheme.bufferNormal.font) {
        NSLog(@"pref: failed to create buffer font '%@'", name);
        oldTheme.bufferNormal.font = [NSFont userFontOfSize:0];
    }
    oldTheme.bufferBackground = [NSColor colorFromData:[defaults objectForKey:@"BufferBackground"]];
    oldTheme.bufferNormal.color = [NSColor colorFromData:[defaults objectForKey:@"BufferForeground"]];

    name = [defaults objectForKey:@"InputFontName"];
    size = [[defaults objectForKey:@"InputFontSize"] doubleValue];
    oldTheme.bufInput.font = [NSFont fontWithName:name size:size];
    if (!oldTheme.bufInput.font) {
        NSLog(@"pref: failed to create input font '%@'", name);
        oldTheme.bufInput.font = [NSFont userFontOfSize:0];
    }

    oldTheme.bufInput.color = [NSColor colorFromData:[defaults objectForKey:@"InputColor"]];
    oldTheme.gridNormal.lineSpacing = 0;

    NSSize cellSize = [oldTheme.gridNormal cellSize];

    oldTheme.cellHeight = cellSize.height;
    oldTheme.cellWidth = cellSize.width;

    cellSize = [oldTheme.bufferNormal cellSize];

    oldTheme.bufferCellHeight = cellSize.height;
    oldTheme.bufferCellWidth = cellSize.width;

    [oldTheme populateStyles];

    oldTheme.defaultParent = [BuiltInThemes createDefaultThemeInContext:context forceRebuild:NO];

    return oldTheme;
}

+ (Theme *)createDefaultThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {

    BOOL exists = NO;
    Theme *defaultTheme = [BuiltInThemes findOrCreateTheme:@"Default" inContext:context alreadyExists:&exists];

    [defaultTheme resetCommonValues];

    if (exists && !force)
        return defaultTheme;

    defaultTheme.dashes = YES;
    defaultTheme.defaultRows = 50;
    defaultTheme.defaultCols = 80;
    defaultTheme.doStyles = YES;
    defaultTheme.smartQuotes = YES;
    defaultTheme.spaceFormat = TAG_SPACES_GAME;
    defaultTheme.border = 10;
    defaultTheme.bufferMarginX = 15;
    defaultTheme.bufferMarginY = 15;
    defaultTheme.gridMarginX = 5;
    defaultTheme.gridMarginY = 5;

    defaultTheme.winSpacingX = 0;
    defaultTheme.winSpacingY = 0;

    defaultTheme.gridBackground = [NSColor blackColor];
    defaultTheme.gridNormal.color = [NSColor colorWithCalibratedRed:0.847 green:0.847 blue:0.847 alpha:1.0];

    defaultTheme.bufferBackground = [NSColor whiteColor];

    defaultTheme.bufInput.color = [NSColor colorWithCalibratedRed:0.137 green:0.431 blue:0.145 alpha:1.0];

    defaultTheme.bufferNormal.lineSpacing = 1;

    [defaultTheme populateStyles];

    NSSize size = [defaultTheme.gridNormal cellSize];

    defaultTheme.cellHeight = size.height;
    defaultTheme.cellWidth = size.width;

    size = [defaultTheme.bufferNormal cellSize];

    defaultTheme.bufferCellHeight = size.height;
    defaultTheme.bufferCellWidth = size.width;

    return defaultTheme;
}

+ (Theme *)createClassicSpatterlightThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {

    BOOL exists = NO;
    Theme *classicTheme = [BuiltInThemes findOrCreateTheme:@"Spatterlight Classic" inContext:context alreadyExists:&exists];

    [classicTheme resetCommonValues];

    if (exists && !force)
        return classicTheme;

    classicTheme.dashes = YES;
    classicTheme.defaultRows = 30;
    classicTheme.defaultCols = 62;
    classicTheme.doStyles = YES;
    classicTheme.smartQuotes = YES;
    classicTheme.spaceFormat = TAG_SPACES_ONE;
    classicTheme.border = 0;
    classicTheme.bufferMarginX = 15;
    classicTheme.bufferMarginY = 15;
    classicTheme.gridMarginX = 5;
    classicTheme.gridMarginY = 5;

    classicTheme.winSpacingX = 0;
    classicTheme.winSpacingY = 0;

    classicTheme.gridBackground = [NSColor blackColor];
    classicTheme.gridNormal.color = [NSColor colorWithCalibratedRed:0.847 green:0.847 blue:0.847 alpha:1.0];
    classicTheme.bufferBackground = [NSColor whiteColor];

    classicTheme.bufInput.color = [NSColor colorWithCalibratedRed:0.137 green:0.431 blue:0.145 alpha:1.0];

    [classicTheme populateStyles];

    NSSize size = [classicTheme.gridNormal cellSize];

    classicTheme.cellHeight = size.height;
    classicTheme.cellWidth = size.width;

    size = [classicTheme.bufferNormal cellSize];

    classicTheme.bufferCellHeight = size.height;
    classicTheme.bufferCellWidth = size.width;

    return classicTheme;
}

+ (Theme *)createGargoyleThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {

    BOOL exists = NO;
    Theme *gargoyleTheme = [BuiltInThemes findOrCreateTheme:@"Gargoyle" inContext:context alreadyExists:&exists];

    [gargoyleTheme resetCommonValues];

    if (exists && !force)
        return gargoyleTheme;

    gargoyleTheme.dashes = YES;
    gargoyleTheme.defaultRows = 30;
    gargoyleTheme.defaultCols = 62;

    gargoyleTheme.doStyles = YES;
    gargoyleTheme.smartQuotes = YES;
    gargoyleTheme.spaceFormat = TAG_SPACES_GAME;
    gargoyleTheme.border = 20;
    gargoyleTheme.bufferMarginX = 3;
    gargoyleTheme.bufferMarginY = 7;
    gargoyleTheme.gridMarginX = 0;
    gargoyleTheme.gridMarginY = 3;

    gargoyleTheme.winSpacingX = 0;
    gargoyleTheme.winSpacingY = 0;

    gargoyleTheme.gridBackground = [NSColor whiteColor];
    gargoyleTheme.bufferBackground = [NSColor whiteColor];

    gargoyleTheme.bufferNormal.font = [NSFont fontWithName:@"Linux Libertine O" size:15.5];
    gargoyleTheme.bufferNormal.lineSpacing = 2;

    gargoyleTheme.bufInput.font = [[NSFontManager sharedFontManager] convertWeight:YES ofFont:gargoyleTheme.bufferNormal.font];
    gargoyleTheme.bufInput.color = [NSColor colorWithCalibratedRed:0.291 green:0.501 blue:0.192 alpha:1.0];

    gargoyleTheme.gridNormal.font = [NSFont fontWithName:@"Liberation Mono" size:12.5];
    gargoyleTheme.gridNormal.color = [NSColor colorWithCalibratedRed:0.376 green:0.376 blue:0.376 alpha:1.0];

    NSMutableDictionary *dict = [gargoyleTheme.gridNormal.attributeDict mutableCopy];
    dict[NSBaselineOffsetAttributeName] = @(-2);
    gargoyleTheme.gridNormal.attributeDict = dict;

    [gargoyleTheme populateStyles];

    gargoyleTheme.bufHead.font = [NSFont fontWithName:@"Linux Libertine O Bold" size:15.5];
    gargoyleTheme.bufHead.autogenerated = NO;
    gargoyleTheme.bufSubH.font = [NSFont fontWithName:@"Linux Libertine O Bold" size:15.5];
    gargoyleTheme.bufSubH.autogenerated = NO;

    NSSize size = [gargoyleTheme.gridNormal cellSize];

    gargoyleTheme.cellHeight = size.height;
    gargoyleTheme.cellWidth = size.width;

    size = [gargoyleTheme.bufferNormal cellSize];

    gargoyleTheme.bufferCellHeight = size.height;
    gargoyleTheme.bufferCellWidth = size.width;

    return gargoyleTheme;
}
+ (Theme *)createLectroteThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {
    BOOL exists = NO;
    Theme *lectroteTheme = [BuiltInThemes findOrCreateTheme:@"Lectrote" inContext:context alreadyExists:&exists];

    [lectroteTheme resetCommonValues];

    if (exists && !force)
        return lectroteTheme;

    lectroteTheme.dashes = YES;
    lectroteTheme.defaultRows = 40;
    lectroteTheme.defaultCols = 100;

    lectroteTheme.doStyles = YES;
    lectroteTheme.smartQuotes = YES;
    lectroteTheme.spaceFormat = TAG_SPACES_GAME;
    lectroteTheme.border = 20;
    lectroteTheme.bufferMarginX = 20;
    lectroteTheme.bufferMarginY = 15;
    lectroteTheme.gridMarginX = 15;
    lectroteTheme.gridMarginY = 6;

    lectroteTheme.winSpacingX = 0;
    lectroteTheme.winSpacingY = 10;

    lectroteTheme.gridBackground = [NSColor colorWithCalibratedRed:0.450844 green:0.325858 blue:0.205177 alpha:1];
    lectroteTheme.bufferBackground = [NSColor whiteColor];

    lectroteTheme.bufferNormal.font = [NSFont fontWithName:@"Lora" size:15];
    lectroteTheme.bufferNormal.lineSpacing = 3.2;

    lectroteTheme.bufInput.font = [[NSFontManager sharedFontManager] convertWeight:YES ofFont:lectroteTheme.bufferNormal.font];

    lectroteTheme.bufInput.color = [NSColor colorWithCalibratedRed:0.042041 green:0.333368 blue:0.011031 alpha:1];

    lectroteTheme.gridNormal.font = [NSFont fontWithName:@"Source Code Pro" size:14];
    lectroteTheme.gridNormal.color = [NSColor colorWithCalibratedRed:0.916565 green:0.902161 blue:0.839754 alpha:1];

    NSSize size = [lectroteTheme.gridNormal cellSize];

    lectroteTheme.cellHeight = size.height;
    lectroteTheme.cellWidth = size.width;

    size = [lectroteTheme.bufferNormal cellSize];

    lectroteTheme.bufferCellHeight = size.height;
    lectroteTheme.bufferCellWidth = size.width;

    [lectroteTheme populateStyles];

    return lectroteTheme;
}

+ (Theme *)createLectroteDarkThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {
    BOOL exists = NO;
    Theme *lectroteDarkTheme = [BuiltInThemes findOrCreateTheme:@"Lectrote Dark" inContext:context alreadyExists:&exists];

    [lectroteDarkTheme resetCommonValues];

    if (exists && !force)
        return lectroteDarkTheme;

    lectroteDarkTheme.dashes = YES;
    lectroteDarkTheme.defaultRows = 40;
    lectroteDarkTheme.defaultCols = 100;

    lectroteDarkTheme.doStyles = YES;
    lectroteDarkTheme.smartQuotes = YES;
    lectroteDarkTheme.spaceFormat = TAG_SPACES_GAME;
    lectroteDarkTheme.border = 20;
    lectroteDarkTheme.bufferMarginX = 20;
    lectroteDarkTheme.bufferMarginY = 15;
    lectroteDarkTheme.gridMarginX = 15;
    lectroteDarkTheme.gridMarginY = 6;

    lectroteDarkTheme.winSpacingX = 0;
    lectroteDarkTheme.winSpacingY = 10;

    lectroteDarkTheme.gridBackground = [NSColor colorWithCalibratedRed:0.991 green:0.957 blue:0.937 alpha:1];
    lectroteDarkTheme.bufferBackground = [NSColor blackColor];

    lectroteDarkTheme.gridNormal.font = [NSFont fontWithName:@"Source Code Pro" size:14];
    lectroteDarkTheme.gridNormal.color = [NSColor colorWithCalibratedRed:0.258 green:0.205 blue:0.145 alpha:1];
    lectroteDarkTheme.bufferNormal.font = [NSFont fontWithName:@"Lora" size:15];
    lectroteDarkTheme.bufferNormal.color = [NSColor colorWithCalibratedRed:0.991 green:0.957 blue:0.937 alpha:1];

    lectroteDarkTheme.bufferNormal.lineSpacing = 3.2;

    lectroteDarkTheme.bufInput.font = [[NSFontManager sharedFontManager] convertWeight:YES ofFont:lectroteDarkTheme.bufferNormal.font];
    lectroteDarkTheme.bufInput.color = [NSColor colorWithCalibratedRed:0.842 green:0.994 blue:0.820 alpha:1];

    NSSize size = [lectroteDarkTheme.gridNormal cellSize];

    lectroteDarkTheme.cellHeight = size.height;
    lectroteDarkTheme.cellWidth = size.width;

    size = [lectroteDarkTheme.bufferNormal cellSize];

    lectroteDarkTheme.bufferCellHeight = size.height;
    lectroteDarkTheme.bufferCellWidth = size.width;

    [lectroteDarkTheme populateStyles];

    return lectroteDarkTheme;
}

+ (Theme *)createZoomThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {
    BOOL exists = NO;
    Theme *zoomTheme = [BuiltInThemes findOrCreateTheme:@"Zoom" inContext:context alreadyExists:&exists];

    if (exists && !force)
        return zoomTheme;

    [zoomTheme resetCommonValues];

    zoomTheme.dashes = YES;
    zoomTheme.defaultRows = 50;
    zoomTheme.defaultCols = 92;

    zoomTheme.doStyles = YES;
    zoomTheme.smartQuotes = YES;
    zoomTheme.spaceFormat = TAG_SPACES_GAME;
    zoomTheme.border = 0;
    zoomTheme.bufferMarginX = 10;
    zoomTheme.bufferMarginY = 12;
    zoomTheme.gridMarginX = 0;
    zoomTheme.gridMarginY = 0;

    zoomTheme.winSpacingX = 0;
    zoomTheme.winSpacingY = 0;

    zoomTheme.gridBackground = [NSColor colorWithCalibratedRed:1 green:1 blue:0.8 alpha:1];
    zoomTheme.bufferBackground = [NSColor colorWithCalibratedRed:1 green:1 blue:0.8 alpha:1];

    zoomTheme.bufferNormal.font = [NSFont fontWithName:@"Gill Sans" size:12];

    NSFont *gillSansBold = [[NSFontManager sharedFontManager] convertFont:zoomTheme.bufferNormal.font toFace:@"GillSans-Bold"];

    zoomTheme.bufInput.font = [gillSansBold copy];

    zoomTheme.gridNormal.font = [NSFont fontWithName:@"Courier" size:12];
    zoomTheme.gridNormal.color = [NSColor blackColor];

    NSMutableDictionary *dict = [zoomTheme.gridNormal.attributeDict mutableCopy];
    dict[NSBaselineOffsetAttributeName] = @(-2);
    zoomTheme.gridNormal.attributeDict = dict;

    NSSize size = [zoomTheme.gridNormal cellSize];

    zoomTheme.cellHeight = size.height;
    zoomTheme.cellWidth = size.width;

    size = [zoomTheme.bufferNormal cellSize];

    zoomTheme.bufferCellHeight = size.height;
    zoomTheme.bufferCellWidth = size.width;

    [zoomTheme populateStyles];

    dict = zoomTheme.bufBlock.attributeDict.mutableCopy;

    NSMutableParagraphStyle *para = [dict[NSParagraphStyleAttributeName] mutableCopy];
    para.headIndent = 10;
    para.firstLineHeadIndent = 10;
    dict[NSParagraphStyleAttributeName] = para;
    zoomTheme.bufBlock.attributeDict = dict;
    zoomTheme.bufBlock.font = [[NSFontManager sharedFontManager] convertFont:zoomTheme.bufBlock.font toHaveTrait:NSBoldFontMask];
    zoomTheme.bufBlock.autogenerated = NO;

    zoomTheme.bufSubH.font = [[NSFontManager sharedFontManager] convertFont:gillSansBold toSize:13];
    //I'm sure this line can't be necessay, but every time I change it reverts to semibold
    zoomTheme.bufSubH.font = [[NSFontManager sharedFontManager] convertFont:zoomTheme.bufSubH.font toFace:@"GillSans-Bold"];
    zoomTheme.bufSubH.autogenerated = NO;

    zoomTheme.bufHead.font = [NSFont fontWithName:@"Gill Sans" size:16];
    dict = zoomTheme.bufHead.attributeDict.mutableCopy;
    para = [dict[NSParagraphStyleAttributeName] mutableCopy];
    para.alignment = NSCenterTextAlignment;
    dict[NSParagraphStyleAttributeName] = para;
    zoomTheme.bufHead.attributeDict = dict;
    zoomTheme.bufHead.autogenerated = NO;

    return zoomTheme;
}

+ (Theme *)createDOSThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {
    BOOL exists = NO;
    Theme *dosTheme = [BuiltInThemes findOrCreateTheme:@"MS-DOS" inContext:context alreadyExists:&exists];

    [dosTheme resetCommonValues];

    if (exists && !force)
        return dosTheme;

    dosTheme.dashes = NO;
    dosTheme.defaultRows = 24;
    dosTheme.defaultCols = 80;

    dosTheme.doStyles = YES;
    dosTheme.smartQuotes = NO;
    dosTheme.spaceFormat = TAG_SPACES_GAME;
    dosTheme.border = 0;
    dosTheme.bufferMarginX = 0;
    dosTheme.bufferMarginY = 0;
    dosTheme.gridMarginX = 0;
    dosTheme.gridMarginY = 0;

    dosTheme.winSpacingX = 0;
    dosTheme.winSpacingY = 0;

    dosTheme.gridBackground =  [NSColor blackColor];
    dosTheme.bufferBackground = [NSColor blackColor];

    dosTheme.gridNormal.font = [NSFont fontWithName:@"PxPlus IBM CGA-2y" size:18];
    dosTheme.gridNormal.color = [NSColor colorWithCalibratedRed:0.512756 green:0.512821  blue:0.512721 alpha:1];

    dosTheme.bufferNormal.font = [NSFont fontWithName:@"PxPlus IBM CGA-2y" size:18];
    dosTheme.bufferNormal.color = [NSColor colorWithCalibratedRed:0.512756 green:0.512821  blue:0.512721 alpha:1];
    dosTheme.bufInput.font = [NSFont fontWithName:@"PxPlus IBM CGA-2y" size:18];
    dosTheme.bufInput.color = [NSColor colorWithCalibratedRed:0.512756 green:0.512821  blue:0.512721 alpha:1];

    NSSize size = [dosTheme.gridNormal cellSize];

    dosTheme.cellHeight = size.height;
    dosTheme.cellWidth = size.width;

    size = [dosTheme.bufferNormal cellSize];

    dosTheme.bufferCellHeight = size.height;
    dosTheme.bufferCellWidth = size.width;

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

    return dosTheme;
}

+ (Theme *)createDOSBoxThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {
    BOOL exists = NO;
    Theme *dosBoxTheme = [BuiltInThemes findOrCreateTheme:@"DOSBox" inContext:context alreadyExists:&exists];

    [dosBoxTheme resetCommonValues];

    if (exists && !force)
        return dosBoxTheme;

    dosBoxTheme.dashes = NO;
    dosBoxTheme.defaultRows = 24;
    dosBoxTheme.defaultCols = 80;

    dosBoxTheme.doStyles = YES;
    dosBoxTheme.smartQuotes = NO;
    dosBoxTheme.spaceFormat = TAG_SPACES_GAME;
    dosBoxTheme.border = 0;
    dosBoxTheme.bufferMarginX = 0;
    dosBoxTheme.bufferMarginY = 0;
    dosBoxTheme.gridMarginX = 0;
    dosBoxTheme.gridMarginY = 0;

    dosBoxTheme.winSpacingX = 0;
    dosBoxTheme.winSpacingY = 0;

    dosBoxTheme.gridBackground = [NSColor colorWithCalibratedRed:0.008897 green:0  blue:0.633764 alpha:1];
    dosBoxTheme.bufferBackground = [NSColor colorWithCalibratedRed:0.008897 green:0  blue:0.633764 alpha:1];

    dosBoxTheme.gridNormal.font = [NSFont fontWithName:@"PxPlus VGA SquarePX" size:24];
    dosBoxTheme.gridNormal.color = [NSColor colorWithCalibratedRed:0.602654 green:0.602749  blue:0.602620 alpha:1];

    dosBoxTheme.bufferNormal.font = [NSFont fontWithName:@"PxPlus VGA SquarePX" size:24];
    dosBoxTheme.bufferNormal.color = [NSColor colorWithCalibratedRed:0.602654 green:0.602749  blue:0.602620 alpha:1];

    dosBoxTheme.bufInput.font = [NSFont fontWithName:@"PxPlus VGA SquarePX" size:24];
    dosBoxTheme.bufInput.color = [NSColor colorWithCalibratedRed:0.602654 green:0.602749  blue:0.602620 alpha:1];

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

    NSSize size = [dosBoxTheme.gridNormal cellSize];

    dosBoxTheme.cellHeight = size.height;
    dosBoxTheme.cellWidth = size.width;

    size = [dosBoxTheme.bufferNormal cellSize];

    dosBoxTheme.bufferCellHeight = size.height;
    dosBoxTheme.bufferCellWidth = size.width;

    return dosBoxTheme;
}

+ (Theme *)createSTThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {
    BOOL exists = NO;
    Theme *stTheme = [BuiltInThemes findOrCreateTheme:@"Atari ST" inContext:context alreadyExists:&exists];

    [stTheme resetCommonValues];

//    if (exists && !force)
//        return stTheme;

    stTheme.dashes = NO;
    stTheme.defaultRows = 24;
    stTheme.defaultCols = 80;


    stTheme.doStyles = YES;

    stTheme.smartQuotes = NO;
    stTheme.spaceFormat = TAG_SPACES_GAME;
    stTheme.border = 0;
    stTheme.bufferMarginX = 0;
    stTheme.bufferMarginY = 0;
    stTheme.gridMarginX = 0;
    stTheme.gridMarginY = 0;

    stTheme.winSpacingX = 0;
    stTheme.winSpacingY = 0;

    stTheme.gridBackground = [NSColor blackColor];
    stTheme.bufferBackground = [NSColor whiteColor];

    stTheme.gridNormal.font = [NSFont fontWithName:@"Atari ST 8x16 System Font" size:18];
    stTheme.gridNormal.color = [NSColor whiteColor];


    stTheme.bufferNormal.font = [NSFont fontWithName:@"Atari ST 8x16 System Font" size:18];

    stTheme.bufInput.font = [NSFont fontWithName:@"Atari ST 8x16 System Font" size:18];

    [stTheme populateStyles];

    stTheme.bufHead.font = [NSFont fontWithName:@"Atari ST 8x16 System Font Bold" size:18];
    stTheme.bufHead.autogenerated = NO;
    stTheme.bufSubH.font = [NSFont fontWithName:@"Atari ST 8x16 System Font Bold" size:18];
    stTheme.bufSubH.autogenerated = NO;
    stTheme.bufEmph.font = [NSFont fontWithName:@"Atari ST 8x16 System Font Bold" size:18];
    stTheme.bufEmph.autogenerated = NO;
    stTheme.gridEmph.font = [NSFont fontWithName:@"Atari ST 8x16 System Font Bold" size:18];
    stTheme.gridEmph.autogenerated = NO;

    NSSize size = [stTheme.gridNormal cellSize];

    stTheme.cellHeight = size.height;
    stTheme.cellWidth = size.width;

    size = [stTheme.bufferNormal cellSize];

    stTheme.bufferCellHeight = size.height;
    stTheme.bufferCellWidth = size.width;

    return stTheme;
}

+ (Theme *)createMontserratThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force {
    BOOL exists = NO;
    Theme *montserratTheme = [BuiltInThemes findOrCreateTheme:@"Montserrat" inContext:context alreadyExists:&exists];

    [montserratTheme resetCommonValues];

    if (exists && !force)
        return montserratTheme;

    montserratTheme.dashes = NO;
    montserratTheme.defaultRows = 50;
    montserratTheme.defaultCols = 65;
    montserratTheme.minRows = 5;
    montserratTheme.minCols = 32;
    montserratTheme.maxRows = 1000;
    montserratTheme.maxCols = 1000;

    montserratTheme.doStyles = NO;
    montserratTheme.smartQuotes = YES;
    montserratTheme.spaceFormat = TAG_SPACES_ONE;
    montserratTheme.border = 0;
    montserratTheme.bufferMarginX = 60;
    montserratTheme.bufferMarginY = 60;
    montserratTheme.gridMarginX = 40;
    montserratTheme.gridMarginY = 10;

    montserratTheme.winSpacingX = 0;
    montserratTheme.winSpacingY = 0;

    montserratTheme.gridBackground = [NSColor whiteColor];
    montserratTheme.bufferBackground = [NSColor whiteColor];

    montserratTheme.gridNormal.font = [NSFont fontWithName:@"PT Sans Narrow" size:15];
    montserratTheme.gridNormal.color = [NSColor blackColor];


    montserratTheme.bufferNormal.font = [NSFont fontWithName:@"Montserrat Regular" size:15];
    montserratTheme.bufferNormal.lineSpacing = 15;

    montserratTheme.bufInput.font = [NSFont fontWithName:@"Montserrat ExtraBold Italic" size:15];


    montserratTheme.bufBlock.font = [NSFont fontWithName:@"Montserrat ExtraLight Italic" size:15];

    [montserratTheme populateStyles];

    montserratTheme.bufHead.font = [NSFont fontWithName:@"Montserrat Black" size:30];
    montserratTheme.bufHead.lineSpacing = 0;

    NSMutableDictionary *dict = montserratTheme.bufHead.attributeDict.mutableCopy;

    NSMutableParagraphStyle *para = [dict[NSParagraphStyleAttributeName] mutableCopy];
    para.lineSpacing = 0;
    para.paragraphSpacing = 15;
    para.paragraphSpacingBefore = 0;
    para.maximumLineHeight = 30;
    dict[NSParagraphStyleAttributeName] = para;

    montserratTheme.bufHead.attributeDict = dict;

    montserratTheme.bufHead.autogenerated = NO;

    montserratTheme.bufSubH.font = [NSFont fontWithName:@"Montserrat ExtraBold" size:20];
    montserratTheme.bufSubH.lineSpacing = 10;

    dict = montserratTheme.bufSubH.attributeDict.mutableCopy;
    para = [dict[NSParagraphStyleAttributeName] mutableCopy];
    para.lineSpacing = 10;
    para.paragraphSpacing = 0;
    para.paragraphSpacingBefore = 0;
    para.maximumLineHeight = 30;
    dict[NSParagraphStyleAttributeName] = para;

    montserratTheme.bufSubH.attributeDict = dict;
    montserratTheme.bufSubH.autogenerated = NO;

    dict = montserratTheme.bufBlock.attributeDict.mutableCopy;

    para = [dict[NSParagraphStyleAttributeName] mutableCopy];
    para.headIndent = 10;
    para.firstLineHeadIndent = 10;
    para.alignment = NSJustifiedTextAlignment;
    dict[NSParagraphStyleAttributeName] = para;
    montserratTheme.bufBlock.attributeDict = dict;
    montserratTheme.bufBlock.autogenerated = NO;

    NSSize size = [montserratTheme.gridNormal cellSize];

    montserratTheme.cellHeight = size.height;
    montserratTheme.cellWidth = size.width;

    size = [montserratTheme.bufferNormal cellSize];

    montserratTheme.bufferCellHeight = size.height;
    montserratTheme.bufferCellWidth = size.width;

    return montserratTheme;
}

+ (Theme *)findOrCreateTheme:(NSString *)themeName inContext:(NSManagedObjectContext *)context alreadyExists:(BOOL *)existsFlagPointer {

    NSError *error = nil;
    *existsFlagPointer = NO;

    // First, check if it already exists
    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:context];

    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"name like[c] %@", themeName];
    NSArray *fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];

    if (fetchedObjects && fetchedObjects.count) {
        *existsFlagPointer = YES;
        return fetchedObjects[0];
    } else if (error != nil) {
        NSLog(@"BuiltInThemes findOrCreateTheme: %@", error);
        return nil;
    }

    Theme *newTheme = (Theme *) [NSEntityDescription
                                 insertNewObjectForEntityForName:@"Theme"
                                 inManagedObjectContext:context];

    newTheme.name = themeName;

    [newTheme populateStyles];

    return newTheme;
}

@end
