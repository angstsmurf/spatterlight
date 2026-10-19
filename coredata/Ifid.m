//
//  Ifid.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-12.
//
//

#import "Ifid.h"
#import "Metadata.h"


@implementation Ifid

+ (NSFetchRequest<Ifid *> *)fetchRequest {
	return [NSFetchRequest fetchRequestWithEntityName:@"Ifid"];
}

@dynamic ifidString;
@dynamic metadata;

@end

