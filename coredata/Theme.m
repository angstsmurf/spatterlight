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
@dynamic cellHeight;
@dynamic cellWidth;
@dynamic comprehendGraphicsType;
@dynamic coverArtStyle;
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
@dynamic hardDark;
@dynamic hardLight;
@dynamic hardLightOrDark;
@dynamic minTimer;
@dynamic morePrompt;
@dynamic name;
@dynamic nohacks;
@dynamic quoteBox;
@dynamic sADelays;
@dynamic sADisplayStyle;
@dynamic sAInventory;
@dynamic sAPalette;
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
@dynamic darkTheme;
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
@dynamic lightTheme;

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

    self.hardDark = NO;
    self.hardLight = NO;
    self.hardLightOrDark = NO;

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
        bufFixed = [NSFont fontWithName:@"Source Code Pro" size:bufroman.pointSize];
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

    self.minTimer = 5;
    self.smoothScroll = YES;
    self.vOSpeakCommand = 1;
    self.vOSpeakImages = kVOImageWithDescriptionOnly;
    self.vOSpeakMenu = kVOMenuTextOnly;
    self.vODelayOn = YES;
    self.vOHackDelay = 4.0;
    self.zMachineLetter = @"S";
    self.zMachineTerp = 4; // Amiga
    self.nohacks = NO;
    self.determinism = NO;
    self.errorHandling = IGNORE_ERRORS;
    self.borderBehavior = kAutomatic;
    if (!self.borderColor)
        self.borderColor = NSColor.whiteColor;
    self.bufLinkStyle = NSUnderlineStyleSingle;
    self.bufLinkColor = nil;
    self.gridLinkColor = nil;
    self.slowDrawing = YES;
}

- (BOOL)hasCustomStyles {
    NSArray *styles = self.allStyles;
    for (GlkStyle *style in styles) {
        if (!style.autogenerated)
            return YES;
    }
    return NO;
}

@end
