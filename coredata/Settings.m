//
//  Settings.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-06-25.
//
//

#import "Settings.h"
#import "Font.h"
#import "Game.h"


@implementation Settings

@dynamic dashes;
@dynamic defaultHeight;
@dynamic defaultWidth;
@dynamic doGraphics;
@dynamic doSound;
@dynamic doStyles;
@dynamic justify;
@dynamic smartQuotes;
@dynamic spaceFormat;
@dynamic wborder;
@dynamic wmargin;
@dynamic wpadding;
@dynamic bufAlert;
@dynamic bufBlock;
@dynamic bufEmph;
@dynamic bufferFont;
@dynamic bufHead;
@dynamic bufInput;
@dynamic bufNote;
@dynamic bufPre;
@dynamic bufSubH;
@dynamic bufUsr1;
@dynamic bufUsr2;
@dynamic gridAlert;
@dynamic gridBlock;
@dynamic gridEmph;
@dynamic gridFont;
@dynamic gridHead;
@dynamic gridInput;
@dynamic gridNote;
@dynamic gridPre;
@dynamic gridSubH;
@dynamic gridUsr1;
@dynamic gridUsr2;
@dynamic game;
@dynamic overrides;

- (Settings *)clone
{
	//create new object in data store
	Settings *cloned = [NSEntityDescription
						insertNewObjectForEntityForName:@"Settings"
						inManagedObjectContext:self.managedObjectContext];

	//loop through all attributes and assign then to the clone
	NSDictionary *attributes = [NSEntityDescription
                                entityForName:@"Settings"
                                inManagedObjectContext:self.managedObjectContext].attributesByName;

	for (NSString *attr in attributes)
	{
		if (![attr isEqualToString:@"isDefault"])
			[cloned setValue:[self valueForKey:attr] forKey:attr];
	}

	// Most settings will only use custom settings for bufferFont, gridFont and bufInput, so let's do them first.

	if (self.bufferFont)
		cloned.bufferFont = [self.bufferFont clone];

	if (self.gridFont)
		cloned.gridFont = [self.gridFont clone];

	if (self.bufInput)
		cloned.bufInput = [self.bufInput clone];

	NSArray *bufStyles = @[@"bufAlert", @"bufBlock", @"bufEmph", @"bufHead", @"bufNote", @"bufPre", @"bufSubH", @"bufUsr1", @"bufUsr2"];

	NSArray *gridStyles = @[@"gridAlert", @"gridBlock", @"gridEmph", @"gridHead", @"gridNote", @"gridPre", @"gridSubH", @"gridUsr1", @"gridUsr2", @"gridInput"];

	for (NSString *style in bufStyles)
	{
		if ([self valueForKey:style] == self.bufferFont)
			[cloned setValue:cloned.bufferFont forKey:style];
	}

	for (NSString *style in gridStyles)
	{
		if ([self valueForKey:style] == self.gridFont)
			[cloned setValue:cloned.gridFont forKey:style];
	}

	//Loop through all relationships, and clone them if nil in target.
	NSDictionary *relationships = [NSEntityDescription
                                   entityForName:@"Settings"
                                   inManagedObjectContext:self.managedObjectContext].relationshipsByName;
	for (NSRelationshipDescription *rel in relationships)
	{
		NSString *keyName = [NSString stringWithFormat:@"%@",rel];
		if ([self valueForKey:keyName] != nil && [cloned valueForKey:keyName] == nil && ![keyName isEqualToString:@"games"])
		{
			//Clone it, and add clone to set
			Font *clonedFont = [[self valueForKey:keyName] clone];
			[cloned setValue:clonedFont forKey:keyName];
			if ([clonedFont valueForKey:keyName] != cloned)
				NSLog(@"Error! Reciprocal relationship did not work as expected");
		}
	}
	return cloned;
}

@end
