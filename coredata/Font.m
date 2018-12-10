//
//  Font+CoreDataClass.m
//  Spatterlight
//
//  Created by Administrator on 2018-09-04.
//
//

#import "Font.h"

@implementation Font

- (Font *)clone
{
	// Create new object in data store
	Font *cloned = [NSEntityDescription
					insertNewObjectForEntityForName:@"Font"
					inManagedObjectContext:self.managedObjectContext];

	// Loop through all attributes and assign them to the clone
	NSDictionary *attributes = [NSEntityDescription
								 entityForName:@"Font"
								 inManagedObjectContext:self.managedObjectContext].attributesByName;

	for (NSString *attr in attributes) {
		[cloned setValue:[self valueForKey:attr] forKey:attr];
	}
	return cloned;
}

@end
