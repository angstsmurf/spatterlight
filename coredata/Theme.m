//
//  Theme.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2020-10-04.
//
//

#import "Constants.h"
#import "Theme.h"
#import "Game.h"
#import "GlkStyle.h"
#import "Fetches.h"
#import "AttributeDictionaryTransformer.h"

#include <math.h>
#import <objc/runtime.h>

#include "glk.h"
#include "glkimp.h"

@implementation Theme

+ (NSFetchRequest<Theme *> *)fetchRequest {
	return [NSFetchRequest fetchRequestWithEntityName:@"Theme"];
}

@dynamic autosave;
@dynamic autosaveOnTimer;
@dynamic beepHigh;
@dynamic beepLow;
@dynamic border;
@dynamic borderBehavior;
@dynamic borderColor;
@dynamic bufferBackground;
@dynamic bufferCellHeight;
@dynamic bufferCellWidth;
@dynamic bufferMarginX;
@dynamic bufferMarginY;
@dynamic bufLinkColor;
@dynamic bufLinkStyle;
@dynamic bZAdjustment;
@dynamic bZTerminator;
@dynamic caretShape;
@dynamic cellHeight;
@dynamic cellWidth;
@dynamic comprehendGraphicsType;
@dynamic coverArtStyle;
@dynamic crtGlow;
@dynamic crtScanlines;
@dynamic dashes;
@dynamic defaultCols;
@dynamic defaultRows;
@dynamic determinism;
@dynamic doGraphics;
@dynamic doSound;
@dynamic doStyles;
@dynamic editable;
@dynamic errorHandling;
@dynamic flicker;
@dynamic gridBackground;
@dynamic gridLinkColor;
@dynamic gridLinkStyle;
@dynamic gridMarginX;
@dynamic gridMarginY;
@dynamic htmlTads;
@dynamic inactiveSideData;
@dynamic minTimer;
@dynamic morePrompt;
@dynamic name;
@dynamic nohacks;
@dynamic quoteBox;
@dynamic sADelays;
@dynamic sADisplayStyle;
@dynamic sAInventory;
@dynamic sAPalette;
@dynamic sideIsDark;
@dynamic scrollbackLimit;
@dynamic slowDrawing;
@dynamic smartQuotes;
@dynamic smoothScroll;
@dynamic spaceFormat;
@dynamic spacingColor;
@dynamic vODelayOn;
@dynamic vOHackDelay;
@dynamic vOSpeakCommand;
@dynamic vOSpeakImages;
@dynamic vOSpeakMenu;
@dynamic winSpacingX;
@dynamic winSpacingY;
@dynamic z6Colorize1Bit;
@dynamic z6GraphicsType;
@dynamic zMachineLetter;
@dynamic zMachineNoErrWin;
@dynamic zMachineTerp;
@dynamic bufAlert;
@dynamic bufBlock;
@dynamic bufEmph;
@dynamic bufferNormal;
@dynamic bufHead;
@dynamic bufInput;
@dynamic bufNote;
@dynamic bufPre;
@dynamic bufSubH;
@dynamic bufUsr1;
@dynamic bufUsr2;
@dynamic defaultChild;
@dynamic defaultParent;
@dynamic games;
@dynamic gridAlert;
@dynamic gridBlock;
@dynamic gridEmph;
@dynamic gridHead;
@dynamic gridInput;
@dynamic gridNormal;
@dynamic gridNote;
@dynamic gridPre;
@dynamic gridSubH;
@dynamic gridUsr1;
@dynamic gridUsr2;

+ (NSString *)unusedNameBasedOn:(NSString *)name inContext:(NSManagedObjectContext *)context {
    NSString *candidate = name;
    NSUInteger counter = 2;
    while ([Fetches findTheme:candidate inContext:context])
        candidate = [NSString stringWithFormat:@"%@ %lu", name, (unsigned long)counter++];
    return candidate;
}

- (nullable Theme *)ancestor {
    NSRange modifiedRange = [self.name rangeOfString:@" (modified)"];
    if (modifiedRange.location != NSNotFound && modifiedRange.location > 1) {
        NSString *baseName = [self.name substringToIndex:modifiedRange.location];
        Theme *named = [Fetches findTheme:baseName inContext:self.managedObjectContext];
        if (named != nil)
            return named;
    }
    Theme *root = self.defaultParent;
    while (root.defaultParent != nil)
        root = root.defaultParent;
    if (root == nil)
        NSLog(@"Found no ancestor theme!");
    return root;
}

- (Theme *)clone {
	//create new object in data store
	Theme *cloned = [NSEntityDescription
                     insertNewObjectForEntityForName:@"Theme"
                     inManagedObjectContext:self.managedObjectContext];

	[cloned copyAttributesFrom:self];
    cloned.defaultParent = self;
	return cloned;
}

- (void)copyAttributesFrom:(Theme *)theme {
    //Loop through all attributes of theme and assign them to me
	NSDictionary *attributes = [NSEntityDescription
                                entityForName:@"Theme"
                                inManagedObjectContext:self.managedObjectContext].attributesByName;

	for (NSString *attr in attributes) {
        [self setValue:[theme valueForKey:attr] forKey:attr];
	}

    self.editable = YES;

    //Loop through all relationships, and clone them if nil in target.

    for (NSUInteger i = 0; i < style_NUMSTYLES; i++) {
        for (NSString *keyName in @[gBufferStyleNames[i], gGridStyleNames[i]]) {
            GlkStyle *clonedStyle = [(GlkStyle *)[theme valueForKey:keyName] clone];
            [self setValue:clonedStyle forKey:keyName];
            if ([clonedStyle valueForKey:keyName] != self)
                NSLog(@"copyAttributesFrom error! Reciprocal relationship did not work as expected");
        }
	}
}

- (void)populateStyles {

    NSFontManager *mgr = [NSFontManager sharedFontManager];
    NSSize size;

    if (!self.bufferNormal) {
        self.bufferNormal = (GlkStyle *) [NSEntityDescription
                                          insertNewObjectForEntityForName:@"GlkStyle"
                                          inManagedObjectContext:self.managedObjectContext];

        [self.bufferNormal createDefaultAttributeDictionary];
        size = self.bufferNormal.cellSize;
        self.bufferCellWidth = size.width;
        self.bufferCellHeight = size.height;
    }


    if (!self.gridNormal || self.cellHeight == 0 || self.cellWidth == 0) {
        self.gridNormal = (GlkStyle *) [NSEntityDescription
                                        insertNewObjectForEntityForName:@"GlkStyle"
                                        inManagedObjectContext:self.managedObjectContext];
        [self.gridNormal createDefaultAttributeDictionary];
        size = self.gridNormal.cellSize;
        self.cellWidth = size.width;
        self.cellHeight = size.height;
    }

    // We skip the first element (0), i.e. the Normal styles here
    for (NSUInteger i = 1; i < style_NUMSTYLES; i++) {
        [self replaceAutogeneratedStyleForKey:gGridStyleNames[i] withCloneOf:self.gridNormal index:i];
        [self replaceAutogeneratedStyleForKey:gBufferStyleNames[i] withCloneOf:self.bufferNormal index:i];
    }

    /* make italic, bold, bolditalic font variants */

    NSFont *bufroman  = self.bufferNormal.font;
    NSFont *gridroman = self.gridNormal.font;

    NSFont *gridbold = [mgr convertWeight:YES ofFont:gridroman];
    NSFont *griditalic = [mgr convertFont:gridroman toHaveTrait:NSItalicFontMask];
    NSFont *gridbolditalic = [mgr convertFont:gridbold toHaveTrait:NSItalicFontMask];

    NSFont *bufbold = [mgr convertWeight:YES ofFont:bufroman];
    NSFont *bufitalic = [mgr convertFont:bufroman toHaveTrait:NSItalicFontMask];
    NSFont *bufbolditalic = [mgr convertFont:bufbold toHaveTrait:NSItalicFontMask];
    NSFont *bufheader = [mgr convertFont:bufbold toSize:bufbold.pointSize + 2];
    NSFont *bufFixed = [mgr convertFont:gridroman toHaveTrait:NSFixedPitchFontMask];

    // If this does not work, we hack it to use Source Code Pro
    if (([mgr traitsOfFont:bufFixed] & NSFixedPitchFontMask) != NSFixedPitchFontMask) {
        bufFixed = [NSFont fontWithName:@"SourceCodePro-Regular" size:bufroman.pointSize];
        NSFontTraitMask mask = [mgr traitsOfFont:bufroman];
        if (mask & NSItalicFontMask)
            bufFixed = [mgr convertFont:bufFixed toHaveTrait:NSItalicFontMask];
        if (mask & NSBoldFontMask)
            bufFixed = [mgr convertFont:bufFixed toHaveTrait:NSBoldFontMask];
    }
    bufFixed = [mgr convertFont:bufFixed toSize:bufroman.pointSize];

    // Only autogenerated styles get the variants: the user may have changed
    // the others. The styles not listed keep the font of their Normal style.
    NSDictionary<NSString *, NSFont *> *variants = @{
        @"bufInput": bufbold,
        @"bufEmph": bufitalic,
        @"bufHead": bufheader,
        @"bufSubH": bufbold,
        @"bufAlert": bufbolditalic,
        @"bufPre": bufFixed,

        @"gridInput": gridbold,
        @"gridEmph": griditalic,
        @"gridHead": gridbold,
        @"gridSubH": gridbold,
        @"gridAlert": gridbolditalic
    };

    for (NSString *key in variants) {
        GlkStyle *style = [self valueForKey:key];
        if (style.autogenerated)
            style.font = [variants[key] copy];
    }
}

// Deletes the style at key if it was autogenerated, and gives the theme a
// fresh autogenerated clone of normal there unless the user's own style is
// still in place.
- (void)replaceAutogeneratedStyleForKey:(NSString *)key withCloneOf:(GlkStyle *)normal index:(NSUInteger)index {
    GlkStyle *style = [self valueForKey:key];
    if (style && style.autogenerated) {
        [self.managedObjectContext deleteObject:style];
        style = nil;
    }

    if (!style) {
        style = normal.clone;
        style.autogenerated = YES;
    }

    [self setValue:style forKey:key];
    style.index = (NSInteger)index;
}

- (NSArray *)allStyles {
    // Skips missing styles: a theme whose styles have not been populated yet
    // would otherwise raise when building the array.
    NSMutableArray<GlkStyle *> *styles = [NSMutableArray arrayWithCapacity:style_NUMSTYLES * 2];
    for (NSUInteger i = 0; i < style_NUMSTYLES; i++) {
        for (NSString *key in @[gBufferStyleNames[i], gGridStyleNames[i]]) {
            GlkStyle *style = [self valueForKey:key];
            if (style)
                [styles addObject:style];
        }
    }
    return styles;
}

// These are values that should be the same in all
// the built-in themes, but may be off due to upgrading
// from and older version with themes that were created
// with different Core Data models and defaults.

// We update them here in all built-in themes every time
// Spatterlight is started.
- (void)resetCommonValues {
    self.autosave = YES;
    self.autosaveOnTimer = YES;
    self.bZTerminator = kBZArrowsSwapped;
    self.doGraphics = YES;
    self.doSound = YES;
    self.editable = NO;
    self.htmlTads = YES;

    self.minTimer = 1;
    self.smoothScroll = YES;
    self.vOSpeakCommand = 1;
    self.vOSpeakImages = kVOImageWithDescriptionOnly;
    self.vOSpeakMenu = kVOMenuTextOnly;
    self.vODelayOn = YES;
    self.vOHackDelay = 0;
    self.zMachineLetter = @"S";
    self.zMachineTerp = 4; // Amiga
    self.nohacks = NO;
    self.determinism = NO;
    self.errorHandling = IGNORE_ERRORS;
    self.borderBehavior = kAutomatic;
    if (!self.borderColor)
        self.borderColor = NSColor.whiteColor;
    self.scrollbackLimit = 0;
    self.bufLinkStyle = NSUnderlineStyleSingle;
    self.bufLinkColor = nil;
    self.gridLinkColor = nil;
    self.slowDrawing = YES;
    self.sADelays = YES;
}

- (BOOL)hasCustomStyles {
    NSArray *styles = self.allStyles;
    for (GlkStyle *style in styles) {
        if (!style.autogenerated)
            return YES;
    }
    return NO;
}

#pragma mark Light and dark sides

// Theme attributes that belong to a side, in addition to the styles.
static NSArray<NSString *> *sideThemeKeys(void) {
    return @[ @"bufferBackground", @"gridBackground", @"borderColor", @"spacingColor",
              @"bufLinkColor", @"gridLinkColor",
              @"bufferMarginX", @"bufferMarginY", @"gridMarginX", @"gridMarginY",
              @"cellWidth", @"cellHeight", @"bufferCellWidth", @"bufferCellHeight",
              @"doStyles" ];
}

// A side snapshot is a dictionary with the keys "theme" (attribute name -> value)
// and "styles" (style name -> dictionary with "attributeDict" and "autogenerated").
- (NSDictionary *)sideSnapshot {
    NSMutableDictionary *themeValues = [NSMutableDictionary new];
    for (NSString *key in sideThemeKeys()) {
        id value = [self valueForKey:key];
        if (value)
            themeValues[key] = value;
    }

    NSMutableDictionary *styles = [NSMutableDictionary new];
    for (NSUInteger i = 0; i < style_NUMSTYLES; i++) {
        for (NSString *name in @[ gBufferStyleNames[i], gGridStyleNames[i] ]) {
            GlkStyle *style = [self valueForKey:name];
            if (!style)
                continue;
            NSMutableDictionary *entry = [NSMutableDictionary new];
            if (style.attributeDict)
                entry[@"attributeDict"] = style.attributeDict;
            entry[@"autogenerated"] = @(style.autogenerated);
            styles[name] = entry;
        }
    }
    return @{ @"theme": themeValues, @"styles": styles };
}

- (void)applySideSnapshot:(NSDictionary *)side {
    NSDictionary *themeValues = side[@"theme"];
    for (NSString *key in sideThemeKeys()) {
        id value = themeValues[key];
        if (value || ![[self valueForKey:key] isKindOfClass:[NSNumber class]])
            [self setValue:value forKey:key];
    }

    NSDictionary *styles = side[@"styles"];
    for (NSString *name in styles) {
        GlkStyle *style = [self valueForKey:name];
        if (!style) {
            style = (GlkStyle *) [NSEntityDescription
                                  insertNewObjectForEntityForName:@"GlkStyle"
                                  inManagedObjectContext:self.managedObjectContext];
            [self setValue:style forKey:name];
        }
        NSDictionary *entry = styles[name];
        style.attributeDict = entry[@"attributeDict"];
        style.autogenerated = [entry[@"autogenerated"] boolValue];
    }
}

// Style attribute dictionaries are stored as data made by
// AttributeDictionaryTransformer, since they may hold things like
// cursors and shadows that it knows how to archive.
- (nullable NSDictionary *)inactiveSide {
    NSData *data = self.inactiveSideData;
    if (!data)
        return nil;
    NSError *error = nil;
    NSSet *classes = [NSSet setWithArray:@[ [NSDictionary class], [NSString class], [NSNumber class], [NSColor class], [NSData class] ]];
    NSDictionary *archived = [NSKeyedUnarchiver unarchivedObjectOfClasses:classes fromData:data error:&error];
    if (!archived) {
        NSLog(@"Theme %@: could not read inactive side: %@", self.name, error);
        return nil;
    }
    AttributeDictionaryTransformer *transformer = [AttributeDictionaryTransformer new];
    NSMutableDictionary *styles = [NSMutableDictionary new];
    NSDictionary *archivedStyles = archived[@"styles"];
    for (NSString *name in archivedStyles) {
        NSMutableDictionary *entry = [archivedStyles[name] mutableCopy];
        if (entry[@"attributeDict"])
            entry[@"attributeDict"] = [transformer transformedValue:entry[@"attributeDict"]];
        styles[name] = entry;
    }
    return @{ @"theme": archived[@"theme"] ?: @{}, @"styles": styles };
}

- (void)setInactiveSide:(nullable NSDictionary *)side activeIsDark:(BOOL)activeIsDark {
    if (!side) {
        self.inactiveSideData = nil;
        return;
    }
    AttributeDictionaryTransformer *transformer = [AttributeDictionaryTransformer new];
    NSMutableDictionary *styles = [NSMutableDictionary new];
    NSDictionary *sideStyles = side[@"styles"];
    for (NSString *name in sideStyles) {
        NSMutableDictionary *entry = [sideStyles[name] mutableCopy];
        if (entry[@"attributeDict"])
            entry[@"attributeDict"] = [transformer reverseTransformedValue:entry[@"attributeDict"]];
        styles[name] = entry;
    }
    NSError *error = nil;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:@{ @"theme": side[@"theme"] ?: @{}, @"styles": styles }
                                         requiringSecureCoding:YES error:&error];
    if (!data) {
        NSLog(@"Theme %@: could not store inactive side: %@", self.name, error);
        return;
    }
    self.inactiveSideData = data;
    self.sideIsDark = activeIsDark;
}

- (BOOL)hasSeparateSides {
    return self.inactiveSideData != nil;
}

- (void)separateSidesWithActiveDark:(BOOL)activeIsDark {
    if (self.hasSeparateSides)
        return;
    [self setInactiveSide:[self sideSnapshot] activeIsDark:activeIsDark];
}

- (void)discardInactiveSide {
    self.inactiveSideData = nil;
}

- (BOOL)sidesAreIdentical {
    NSDictionary *inactive = self.inactiveSide;
    if (!inactive)
        return YES;
    NSDictionary *active = [self sideSnapshot];
    if (![active[@"theme"] isEqual:inactive[@"theme"]])
        return NO;
    NSDictionary *activeStyles = active[@"styles"];
    NSDictionary *inactiveStyles = inactive[@"styles"];
    for (NSString *name in activeStyles) {
        NSDictionary *a = activeStyles[name];
        NSDictionary *b = inactiveStyles[name];
        if (!b || [a[@"autogenerated"] boolValue] != [b[@"autogenerated"] boolValue])
            return NO;
        NSDictionary *dictA = a[@"attributeDict"];
        NSDictionary *dictB = b[@"attributeDict"];
        if (dictA != dictB && ![dictA isEqual:dictB])
            return NO;
    }
    return YES;
}

- (void)activateSideForDark:(BOOL)dark {
    if (!self.hasSeparateSides || self.sideIsDark == dark)
        return;
    NSDictionary *inactive = self.inactiveSide;
    if (!inactive)
        return;
    NSDictionary *active = [self sideSnapshot];
    [self applySideSnapshot:inactive];
    [self setInactiveSide:active activeIsDark:dark];
}

+ (void)activateSidesForDark:(BOOL)dark inContext:(NSManagedObjectContext *)context {
    if (!context)
        return;
    NSFetchRequest *fetchRequest = [Theme fetchRequest];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"inactiveSideData != nil AND sideIsDark != %@", @(dark)];
    NSError *error = nil;
    NSArray<Theme *> *themes = [context executeFetchRequest:fetchRequest error:&error];
    if (error)
        NSLog(@"Theme activateSidesForDark: %@", error);
    for (Theme *theme in themes)
        [theme activateSideForDark:dark];
}

#pragma mark Game colors against the light side

static char SPLightSideCacheKey;

// The light side as a sideSnapshot, when it is the stored inactive side.
// nil when the theme's own attributes hold the light side, or there is only
// one. Cached against inactiveSideData, since game colors are fitted for
// every run of colored text.
- (nullable NSDictionary *)storedLightSide {
    if (!self.sideIsDark)
        return nil;
    NSData *data = self.inactiveSideData;
    if (!data)
        return nil;
    NSArray *cache = objc_getAssociatedObject(self, &SPLightSideCacheKey);
    if (cache.count == 2 && cache[0] == data)
        return cache[1];
    NSDictionary *side = self.inactiveSide;
    if (!side)
        return nil;
    objc_setAssociatedObject(self, &SPLightSideCacheKey, @[ data, side ], OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    return side;
}

- (nullable NSColor *)lightSideBackgroundForGrid:(BOOL)grid {
    NSString *key = grid ? @"gridBackground" : @"bufferBackground";
    NSDictionary *side = [self storedLightSide];
    if (side)
        return side[@"theme"][key];
    return [self valueForKey:key];
}

- (nullable NSColor *)lightSideColorOfStyle:(NSString *)name {
    NSDictionary *side = [self storedLightSide];
    if (side)
        return side[@"styles"][name][@"attributeDict"][NSForegroundColorAttributeName];
    return ((GlkStyle *)[self valueForKey:name]).color;
}

// WCAG relative luminance, or -1 for a color with no RGB form.
static CGFloat SPRelativeLuminance(NSColor *color) {
    NSColor *rgb = [color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
    if (!rgb)
        return -1;
    CGFloat channels[3] = { rgb.redComponent, rgb.greenComponent, rgb.blueComponent };
    for (NSUInteger i = 0; i < 3; i++) {
        CGFloat c = channels[i];
        channels[i] = (c <= 0.03928) ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
    }
    return 0.2126 * channels[0] + 0.7152 * channels[1] + 0.0722 * channels[2];
}

// WCAG contrast ratio, 1 to 21. A pair that cannot be measured counts as
// readable, so that it is left alone.
static CGFloat SPContrastRatio(NSColor *a, NSColor *b) {
    CGFloat la = SPRelativeLuminance(a), lb = SPRelativeLuminance(b);
    if (la < 0 || lb < 0)
        return 21;
    return (MAX(la, lb) + 0.05) / (MIN(la, lb) + 0.05);
}

// Below this a pair is taken to be unreadable: WCAG's minimum for large text
// and interface elements. Black or navy text on a dark background falls well
// under it, most colors a game picks for emphasis do not.
static const CGFloat kSPMinGameColorContrast = 3.0;

- (void)fitGameColors:(NSMutableDictionary *)attributes
       gameForeground:(BOOL)gameForeground
       gameBackground:(BOOL)gameBackground
           lightStyle:(NSString *)lightStyle
                 grid:(BOOL)grid {
    if (gameForeground == gameBackground)
        return;

    NSColor *fg = attributes[NSForegroundColorAttributeName];
    NSColor *bg = attributes[NSBackgroundColorAttributeName];
    if (!bg)
        bg = grid ? self.gridBackground : self.bufferBackground;
    if (!fg || !bg)
        return;

    CGFloat contrast = SPContrastRatio(fg, bg);
    if (contrast >= kSPMinGameColorContrast)
        return;

    if (gameForeground) {
        NSColor *designed = [self lightSideBackgroundForGrid:grid];
        if (designed && SPContrastRatio(fg, designed) > contrast)
            attributes[NSBackgroundColorAttributeName] = designed;
    } else {
        NSColor *designed = [self lightSideColorOfStyle:lightStyle ?: (grid ? @"gridNormal" : @"bufferNormal")];
        if (designed && SPContrastRatio(designed, bg) > contrast)
            attributes[NSForegroundColorAttributeName] = designed;
    }
}

@end
