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
#import "Interpreter.h"
#import "Theme.h"

#include "glk.h"
#include "glkimp.h"

@implementation Theme

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
@dynamic coverArtStyle;
@dynamic cursorShape;
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
@dynamic slowDrawing;
@dynamic smartQuotes;
@dynamic smoothScroll;
@dynamic spaceFormat;
@dynamic spacingColor;
@dynamic winSpacingX;
@dynamic winSpacingY;
@dynamic zMachineTerp;
@dynamic zMachineLetter;
@dynamic vOHackDelay;
@dynamic vODelayOn;
@dynamic vOSpeakCommand;
@dynamic vOSpeakImages;
@dynamic vOSpeakMenu;
@dynamic z6GraphicsType;
@dynamic z6Colorize1Bit;
@dynamic z6Simulate16Color;
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
@dynamic interpreter;
@dynamic lightTheme;
@dynamic overrides;

- (Theme *)clone {

    NSLog(@"Creating a clone of theme %@", self.name);

	//create new object in data store
	Theme *cloned = [NSEntityDescription
                     insertNewObjectForEntityForName:@"Theme"
                     inManagedObjectContext:self.managedObjectContext];

	[cloned copyAttributesFrom:self];

    NSString *trimmedString = [cloned.name substringFromIndex:(NSUInteger)MAX((int)[cloned.name length] - 7, 0)]; //in case string is less than 7 characters long.
    if ([trimmedString isEqualToString:@" cloned"])
        cloned.name = [cloned.name stringByAppendingString:@" 2"];
    else
        cloned.name = [cloned.name stringByAppendingString:@" cloned"];

    cloned.defaultParent = self;
	return cloned;
}

- (void)copyAttributesFrom:(Theme *)theme {
    //Loop through all attributes of theme and assign them to me
	NSDictionary *attributes = [NSEntityDescription
                                entityForName:@"Theme"
                                inManagedObjectContext:self.managedObjectContext].attributesByName;

	for (NSString *attr in attributes) {
        //NSLog(@"Setting my %@ to %@", attr, [theme valueForKey:attr]);
        [self setValue:[theme valueForKey:attr] forKey:attr];
	}

    self.editable = YES;

    //Loop through all relationships, and clone them if nil in target.

    NSString *keyName;
    GlkStyle *clonedStyle;
    for (NSUInteger i = 0; i < style_NUMSTYLES; i++) {
        //Clone it, and add clone to set
        keyName = gBufferStyleNames[i];
        clonedStyle = [(GlkStyle * )[theme valueForKey:keyName] clone];
        //NSLog(@"Setting my %@ to a clone of the %@ of %@", keyName, keyName, theme.name);
        [self setValue:clonedStyle forKey:keyName];
        if ([clonedStyle valueForKey:keyName] != self)
            NSLog(@"Error! Reciprocal relationship did not work as expected");
        keyName = gGridStyleNames[i];
        clonedStyle = [(GlkStyle * )[theme valueForKey:keyName] clone];
//        NSLog(@"Setting my %@ to a clone of the %@ of %@", keyName, keyName, theme.name);
        [self setValue:clonedStyle forKey:keyName];
        if ([clonedStyle valueForKey:keyName] != self)
            NSLog(@"Error! Reciprocal relationship did not work as expected");
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

    NSParagraphStyle *parastyle = self.gridNormal.attributeDict[NSParagraphStyleAttributeName];
    if (parastyle && parastyle.maximumLineHeight == 0) {
        NSMutableParagraphStyle *mutable = parastyle.mutableCopy;
        mutable.maximumLineHeight = self.cellHeight;
        NSMutableDictionary *mutAtt = self.gridNormal.attributeDict.mutableCopy;
        mutAtt[NSParagraphStyleAttributeName] = mutable;
        self.gridNormal.attributeDict = mutAtt;
    }

    // We skip the first element (0), i.e. the Normal styles here
    for (NSUInteger i = 1; i < style_NUMSTYLES; i++)
	{
        GlkStyle *style = [self valueForKey:gGridStyleNames[i]];
        // Delete all old GlkStyle objects that we do not want to keep
        if (style && style.autogenerated) {
            [self.managedObjectContext deleteObject:style];
            style = nil;
        }

        if (!style) {
            style = self.gridNormal.clone;
            style.autogenerated = YES;
        }

        [self setValue:style forKey:gGridStyleNames[i]];
        style.index = (NSInteger)i;

        style = [self valueForKey:gBufferStyleNames[i]];

        if (style && style.autogenerated) {
            // Delete all old GlkStyle objects that we do not want to keep
            [self.managedObjectContext deleteObject:style];
            style = nil ;
        }

        if (!style) {
            style = self.bufferNormal.clone;
            style.autogenerated = YES;
        }

        [self setValue:style forKey:gBufferStyleNames[i]];
        style.index = (NSInteger)i;
	}

    /* make italic, bold, bolditalic font variants */

    NSFont *bufroman, *bufbold, *bufitalic, *bufbolditalic, *bufheader, *bufFixed;
    NSFont *gridroman, *gridbold, *griditalic, *gridbolditalic;

    bufroman  = self.bufferNormal.font;
    gridroman = self.gridNormal.font;

    gridbold = [mgr convertWeight:YES ofFont:gridroman];
    griditalic = [mgr convertFont:gridroman toHaveTrait:NSItalicFontMask];
    gridbolditalic = [mgr convertFont:gridbold toHaveTrait:NSItalicFontMask];

    bufbold = [mgr convertWeight:YES ofFont:bufroman];
    bufitalic = [mgr convertFont:bufroman toHaveTrait:NSItalicFontMask];
    bufbolditalic = [mgr convertFont:bufbold toHaveTrait:NSItalicFontMask];
    bufheader = [mgr convertFont:bufbold toSize:bufbold.pointSize + 2];
    bufFixed = [mgr convertFont:gridroman toHaveTrait:NSFixedPitchFontMask];

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

    if (self.bufInput.autogenerated)
        (self.bufInput).font = [bufbold copy];
    if (self.bufEmph.autogenerated)
        (self.bufEmph).font = [bufitalic copy];
    if (self.bufHead.autogenerated)
        (self.bufHead).font = [bufheader copy];
    if (self.bufSubH.autogenerated)
        (self.bufSubH).font = [bufbold copy];
    if (self.bufAlert.autogenerated)
        (self.bufAlert).font = [bufbolditalic copy];
    if (self.bufPre.autogenerated)
        (self.bufPre).font = [bufFixed copy];

    if (self.gridInput.autogenerated)
        (self.gridInput).font = [gridbold copy];
    if (self.gridEmph.autogenerated)
        (self.gridEmph).font = [griditalic copy];
    if (self.gridHead.autogenerated)
        (self.gridHead).font = [gridbold copy];
    if (self.gridSubH.autogenerated)
        (self.gridSubH).font = [gridbold copy];
    if (self.gridAlert.autogenerated)
        (self.gridAlert).font = [gridbolditalic copy];
}

- (NSArray *)allStyles {
    return @[ self.bufAlert,
              self.bufBlock,
              self.bufEmph,
              self.bufferNormal,
              self.bufHead,
              self.bufInput,
              self.bufNote,
              self.bufPre,
              self.bufSubH,
              self.bufUsr1,
              self.bufUsr2,
              self.gridAlert,
              self.gridBlock,
              self.gridEmph,
              self.gridHead,
              self.gridInput,
              self.gridNormal,
              self.gridNote,
              self.gridPre,
              self.gridSubH,
              self.gridUsr1,
              self.gridUsr2 ];
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
    self.bZTerminator = kBZArrowsCompromise;
    self.doGraphics = YES;
    self.doSound = YES;
    self.editable = NO;

    self.minTimer = 5;
    self.smoothScroll = YES;
    self.vOSpeakCommand = YES;
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

// These values are not currently used

//    self.maxCols = 1000;
//    self.maxRows = 1000;
//    self.minCols = 32;
//    self.minRows = 5;
//    self.cursorShape = 0;
//    self.imageSizing = NO;
//    self.justify = NO;
//    self.vOExtraElements = NO;
//    self.vOSpeakInputType = 0;

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
