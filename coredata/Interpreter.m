//
//  Interpreter.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-12.
//
//

#import "Interpreter.h"
#import "Theme.h"


@implementation Interpreter

+ (NSFetchRequest<Interpreter *> *)fetchRequest {
	return [NSFetchRequest fetchRequestWithEntityName:@"Interpreter"];
}

@dynamic name;
@dynamic setting;

@end
