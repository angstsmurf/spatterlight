//
//  Tag.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-12.
//
//

#import "Tag.h"
#import "Metadata.h"


@implementation Tag

+ (NSFetchRequest<Tag *> *)fetchRequest {
	return [NSFetchRequest fetchRequestWithEntityName:@"Tag"];
}

@dynamic content;
@dynamic metadata;

@end
