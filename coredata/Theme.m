//
//  Theme.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2020-01-30.
//
//

#import "main.h"
#import "Theme.h"
#import "Game.h"
#import "GlkStyle.h"
#import "Interpreter.h"
#import "Theme.h"


@implementation Theme

@dynamic border;
@dynamic bufferBackground;
@dynamic bufferMarginX;
@dynamic bufferMarginY;
@dynamic cellHeight;
@dynamic cellWidth;
@dynamic dashes;
@dynamic defaultCols;
@dynamic defaultRows;
@dynamic doGraphics;
@dynamic doSound;
@dynamic doStyles;
@dynamic editable;
@dynamic gridBackground;
@dynamic gridMarginX;
@dynamic gridMarginY;
@dynamic justify;
@dynamic maxCols;
@dynamic maxRows;
@dynamic minCols;
@dynamic minRows;
@dynamic morePrompt;
@dynamic name;
@dynamic smartQuotes;
@dynamic spaceFormat;
@dynamic spacingColor;
@dynamic winSpacingX;
@dynamic winSpacingY;
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
@dynamic interpreter;
@dynamic overrides;
@dynamic darkTheme;
@dynamic lightTheme;

- (Theme *)clone {

    NSLog(@"Creating a clone of theme %@", self.name);

	//create new object in data store
	Theme *cloned = [NSEntityDescription
                     insertNewObjectForEntityForName:@"Theme"
                     inManagedObjectContext:self.managedObjectContext];

	[cloned copyAttributesFrom:self];

    NSString *trimmedString = [cloned.name substringFromIndex:(NSUInteger)MAX((int)[cloned.name length]-7, 0)]; //in case string is less than 7 characters long.
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
    BOOL bufInputExists = NO;

    if (!self.bufferNormal) {

        self.bufferNormal = (GlkStyle *) [NSEntityDescription
                                          insertNewObjectForEntityForName:@"GlkStyle"
                                          inManagedObjectContext:self.managedObjectContext];

        [self.bufferNormal createDefaultAttributeDictionary];
//        NSLog(@"Created a new normal buffer style for theme %@", self.name);
    }


    if (!self.gridNormal) {
        self.gridNormal = (GlkStyle *) [NSEntityDescription
                                        insertNewObjectForEntityForName:@"GlkStyle"
                                        inManagedObjectContext:self.managedObjectContext];
        [self.gridNormal createDefaultAttributeDictionary];
        NSSize size = [self.gridNormal cellSize];
        self.cellWidth = size.width;
        self.cellHeight = size.height;
//        NSLog(@"Created a new normal grid style for theme %@", self.name);
    }

    if (!self.bufInput) {
        self.bufInput = (GlkStyle *) [NSEntityDescription
                                      insertNewObjectForEntityForName:@"GlkStyle"
                                      inManagedObjectContext:self.managedObjectContext];

        [self.bufInput createDefaultAttributeDictionary];
        NSLog(@"Created a new buffer input style for theme %@", self.name);
    } else bufInputExists = YES;

    // We skip the first element (0), i.e. the Normal styles here
    for (NSUInteger i = 1 ; i < style_NUMSTYLES ; i++)
	{
        GlkStyle *style = [self valueForKey:gGridStyleNames[i]];
        // Delete all old GlkStyle objects that we do not want to keep
        if (style && style.autogenerated) {
            [self.managedObjectContext deleteObject:style];
            style = nil;
        }

        if (!style)
            style = [self.gridNormal clone] ;

        [self setValue:style forKey:gGridStyleNames[i]];
        [style setIndex:(NSInteger)i];

        style = [self valueForKey:gBufferStyleNames[i]];

        if (style && style.autogenerated) {
            if (i == style_Input)
                continue;
            // Delete all old GlkStyle objects that we do not want to keep
            [self.managedObjectContext deleteObject:style];
            style = nil ;
        }

        if (!style)
            style = [self.bufferNormal clone] ;

        [self setValue:style forKey:gBufferStyleNames[i]];
        [style setIndex:(NSInteger)i];
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
    bufFixed = [mgr convertFont:bufFixed toSize:bufroman.pointSize];

    // Do not replace input style there was an old one
    if (!bufInputExists)
        [self.bufInput setFont:[bufbold copy]];
    if (self.bufEmph.autogenerated)
        [self.bufEmph setFont:[bufitalic copy]];
    if (self.bufHead.autogenerated)
        [self.bufHead setFont:[bufheader copy]];
    if (self.bufSubH.autogenerated)
        [self.bufSubH setFont:[bufbold copy]];
    if (self.bufAlert.autogenerated)
        [self.bufAlert setFont:[bufbolditalic copy]];
    if (self.bufPre.autogenerated)
        [self.bufPre setFont:[bufFixed copy]];

    if (self.gridInput.autogenerated)
        [self.gridInput setFont:[gridbold copy]];
    if (self.gridEmph.autogenerated)
        [self.gridEmph setFont:[griditalic copy]];
    if (self.gridHead.autogenerated)
        [self.gridHead setFont:[gridbold copy]];
    if (self.gridSubH.autogenerated)
        [self.gridSubH setFont:[gridbold copy]];
    if (self.gridAlert.autogenerated)
        [self.gridAlert setFont:[gridbolditalic copy]];
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

- (BOOL)hasCustomStyles {
    NSArray *styles = self.allStyles;
    for (GlkStyle *style in styles) {
        if (!style.autogenerated)
            return YES;
    }
    return NO;
}

@end
