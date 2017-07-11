//
//  Game+CoreDataProperties.m
//  Spatterlight
//
//  Created by Administrator on 2017-06-16.
//
//

#import "Game+CoreDataProperties.h"

@implementation Game (CoreDataProperties)

+ (NSFetchRequest<Game *> *)fetchRequest {
	return [[NSFetchRequest alloc] initWithEntityName:@"Game"];
}

@dynamic added;
@dynamic fileLocation;
@dynamic found;
@dynamic group;
@dynamic lastPlayed;
@dynamic row;
@dynamic thumbnail;
@dynamic infoWindow;
@dynamic interpreter;
@dynamic metadata;
@dynamic setting;

@end
