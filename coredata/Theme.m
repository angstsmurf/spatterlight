//
//  Theme.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-29.
//
//

#import "Theme.h"
#import "main.h"
#import "Game.h"
#import "GlkStyle.h"
#import "Interpreter.h"

@implementation Theme

@dynamic border;
@dynamic bufferBackground;
@dynamic bufferMarginX;
@dynamic bufferMarginY;
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
@dynamic smartQuotes;
@dynamic spaceFormat;
@dynamic spacingColor;
@dynamic name;
@dynamic winSpacingX;
@dynamic winSpacingY;
@dynamic cellWidth;
@dynamic cellHeight;
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
@dynamic gridNormal;
@dynamic gridHead;
@dynamic gridInput;
@dynamic gridNote;
@dynamic gridPre;
@dynamic gridSubH;
@dynamic gridUsr1;
@dynamic gridUsr2;
@dynamic interpreter;
@dynamic overrides;

- (Theme *)clone {

    NSLog(@"Creating a clone of theme %@", self.name);
    
	//create new object in data store
	Theme *cloned = [NSEntityDescription
                     insertNewObjectForEntityForName:@"Theme"
                     inManagedObjectContext:self.managedObjectContext];

	//Loop through all attributes and assign then to the clone
	NSDictionary *attributes = [NSEntityDescription
                                entityForName:@"Theme"
                                inManagedObjectContext:self.managedObjectContext].attributesByName;

	for (NSString *attr in attributes) {
        //NSLog(@"Setting clone %@ to %@", attr, [self valueForKey:attr]);
        [cloned setValue:[self valueForKey:attr] forKey:attr];
	}

    //Loop through all relationships, and clone them if nil in target.
//    NSDictionary *relationships = [NSEntityDescription
//                                   entityForName:@"Theme"
//                                   inManagedObjectContext:self.managedObjectContext].relationshipsByName;
	//for (NSRelationshipDescription *rel in relationships)

    NSString *keyName;
    GlkStyle *clonedStyle;
    for (NSUInteger i = 0; i < style_NUMSTYLES; i++) {
        //Clone it, and add clone to set
        //NSLog(@"Setting clone %@ to a clone of my %@", keyName, keyName);
        keyName = [gBufferStyleNames objectAtIndex:i];
        clonedStyle = [(GlkStyle * )[self valueForKey:keyName] clone];
        [cloned setValue:clonedStyle forKey:keyName];
        if ([clonedStyle valueForKey:keyName] != cloned)
            NSLog(@"Error! Reciprocal relationship did not work as expected");
        keyName = [gGridStyleNames objectAtIndex:i];
        clonedStyle = [(GlkStyle * )[self valueForKey:keyName] clone];
        [cloned setValue:clonedStyle forKey:keyName];
        if ([clonedStyle valueForKey:keyName] != cloned)
            NSLog(@"Error! Reciprocal relationship did not work as expected");
	}

    cloned.name = [cloned.name stringByAppendingString:@" cloned"];
    cloned.defaultParent = self;
	return cloned;
}

- (void)populateStyles {

    NSFontManager *mgr = [NSFontManager sharedFontManager];
    BOOL bufInputExists = NO;

    if (!self.bufferNormal) {

        self.bufferNormal = (GlkStyle *) [NSEntityDescription
                                          insertNewObjectForEntityForName:@"GlkStyle"
                                          inManagedObjectContext:self.managedObjectContext];

        [self.bufferNormal createDefaultAttributeDictionary];
        NSLog(@"Created a new normal buffer style for theme %@", self.name);
    }


    if (!self.gridNormal) {
        self.gridNormal = (GlkStyle *) [NSEntityDescription
                                        insertNewObjectForEntityForName:@"GlkStyle"
                                        inManagedObjectContext:self.managedObjectContext];
        [self.gridNormal createDefaultAttributeDictionary];
        NSSize size = [self.gridNormal cellSize];
        self.cellWidth = size.width;
        self.cellHeight = size.height;
        NSLog(@"Created a new normal buffer style for theme %@", self.name);
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
        // Delete all old GlkStyle objects that we do not want to keep
        if ([self valueForKey:[gGridStyleNames objectAtIndex:i]])
            [self.managedObjectContext deleteObject:[self valueForKey:[gGridStyleNames objectAtIndex:i]]];
        
        [self setValue:[self.gridNormal clone] forKey:[gGridStyleNames objectAtIndex:i]];
        [[self valueForKey:[gGridStyleNames objectAtIndex:i]] setIndex:(NSInteger)i];

        if ([self valueForKey:[gBufferStyleNames objectAtIndex:i]]) {
            if (i == style_Input)
                continue;
            // Delete all old GlkStyle objects that we do not want to keep
            [self.managedObjectContext deleteObject:[self valueForKey:[gBufferStyleNames objectAtIndex:i]]];
        }

        [self setValue:[self.bufferNormal clone] forKey:[gBufferStyleNames objectAtIndex:i]];
        [[self valueForKey:[gBufferStyleNames objectAtIndex:i]] setIndex:(NSInteger)i];
	}

      /* make italic, bold, bolditalic font variants */

    NSFont *bufroman, *bufbold, *bufitalic, *bufbolditalic, *bufheader;
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

    // Do not replace input style there was an old one
    if (!bufInputExists)
        [self.bufInput setFont:[bufbold copy]];
    [self.bufEmph setFont:[bufitalic copy]];
    [self.bufHead setFont:[bufheader copy]];
    [self.bufSubH setFont:[bufbold copy]];
    [self.bufAlert setFont:[bufbolditalic copy]];

    [self.gridInput setFont:[gridbold copy]];
    [self.gridEmph setFont:[griditalic copy]];
    [self.gridHead setFont:[gridbold copy]];
    [self.gridSubH setFont:[gridbold copy]];
    [self.gridAlert setFont:[gridbolditalic copy]];
}

@end
