//
//  Font.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-06-25.
//
//

#import "Font.h"
#import "Settings.h"


@implementation Font

@dynamic baseline;
@dynamic bgcolor;
@dynamic color;
@dynamic leading;
@dynamic name;
@dynamic pitch;
@dynamic size;
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
