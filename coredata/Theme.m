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
@dynamic themeName;
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

    NSLog(@"Creating a clone of theme %@", self.themeName);
    
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
	NSDictionary *relationships = [NSEntityDescription
                                   entityForName:@"Theme"
                                   inManagedObjectContext:self.managedObjectContext].relationshipsByName;
	for (NSRelationshipDescription *rel in relationships)
	{
		NSString *keyName = [NSString stringWithFormat:@"%@",rel];
		if ([self valueForKey:keyName] != nil && [cloned valueForKey:keyName] == nil && ![keyName isEqualToString:@"games"])
		{
			//Clone it, and add clone to set
            //NSLog(@"Setting clone %@ to a clone of my %@", keyName, keyName);
			GlkStyle *clonedFont = [((GlkStyle *)[self valueForKey:keyName]) clone];
			[cloned setValue:clonedFont forKey:keyName];
			if ([clonedFont valueForKey:keyName] != cloned)
				NSLog(@"Error! Reciprocal relationship did not work as expected");
		}
	}
	return cloned;
}

- (void)populateStyles {

    NSFontManager *mgr = [NSFontManager sharedFontManager];

    if (!self.bufferNormal) {

        self.bufferNormal = (GlkStyle *) [NSEntityDescription
                                          insertNewObjectForEntityForName:@"GlkStyle"
                                          inManagedObjectContext:self.managedObjectContext];

        [self.bufferNormal createDefaultAttributeDictionary];
        NSLog(@"Created a new normal buffer style for theme %@", self.themeName);
    }


    if (!self.gridNormal) {
        self.gridNormal = [self.bufferNormal clone];
        [self.gridNormal createDefaultAttributeDictionary];
        NSLog(@"Created a new normal buffer style for theme %@", self.themeName);
    }

    for (NSInteger i = 0 ; i < style_NUMSTYLES ; i++)
	{
		[self setValue:[self.bufferNormal clone] forKey:[gBufferStyleNames objectAtIndex:i]];
        [self setValue:[self.gridNormal clone] forKey:[gGridStyleNames objectAtIndex:i]];

        [((GlkStyle *)[self valueForKey:[gBufferStyleNames objectAtIndex:i]]).attributeDict setObject:@(i) forKey:@"GlkStyle"];
        [((GlkStyle *)[self valueForKey:[gGridStyleNames objectAtIndex:i]]).attributeDict setObject:@(i) forKey:@"GlkStyle"];
	}

      /* make italic, bold, bolditalic font variants */

    NSFont *bufroman, *bufbold, *bufitalic, *bufbolditalic, *bufheader;
    NSFont *gridroman, *gridbold, *griditalic, *gridbolditalic;

    bufroman  = [self.bufferNormal.attributeDict objectForKey:NSFontAttributeName];
    gridroman = [self.gridNormal.attributeDict objectForKey:NSFontAttributeName];

    gridbold = [mgr convertWeight:YES ofFont:gridroman];
    griditalic = [mgr convertFont:gridroman toHaveTrait:NSItalicFontMask];
    gridbolditalic = [mgr convertFont:gridbold toHaveTrait:NSItalicFontMask];

    bufbold = [mgr convertWeight:YES ofFont:bufroman];
    bufitalic = [mgr convertFont:bufroman toHaveTrait:NSItalicFontMask];
    bufbolditalic = [mgr convertFont:bufbold toHaveTrait:NSItalicFontMask];
    bufheader = [mgr convertFont:bufbold toSize:bufbold.pointSize + 2];

    [self.bufInput.attributeDict setObject:[bufbold copy] forKey:NSFontAttributeName];
    [self.bufEmph.attributeDict setObject:[bufitalic copy] forKey:NSFontAttributeName];
    [self.bufHead.attributeDict setObject:[bufheader copy] forKey:NSFontAttributeName];
    [self.bufSubH.attributeDict setObject:[bufbold copy] forKey:NSFontAttributeName];
    [self.bufAlert.attributeDict setObject:[bufbolditalic copy] forKey:NSFontAttributeName];

    [self.gridInput.attributeDict setObject:[gridbold copy] forKey:NSFontAttributeName];
    [self.gridEmph.attributeDict setObject:[griditalic copy] forKey:NSFontAttributeName];
    [self.gridHead.attributeDict setObject:[gridbold copy] forKey:NSFontAttributeName];
    [self.gridSubH.attributeDict setObject:[gridbold copy] forKey:NSFontAttributeName];
    [self.gridAlert.attributeDict setObject:[gridbolditalic copy] forKey:NSFontAttributeName];

}

@end
