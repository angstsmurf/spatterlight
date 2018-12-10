//
//  Settings+CoreDataClass.m
//  Spatterlight
//
//  Created by Administrator on 2018-09-04.
//
//

#import "Settings.h"

@implementation Settings

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
