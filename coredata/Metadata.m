//
//  Metadata.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-13.
//
//

#import "Metadata.h"
#import "Game.h"
#import "Ifid.h"
#import "Image.h"
#import "Tag.h"
#import <CoreSpotlight/CoreSpotlight.h>


@implementation Metadata

+ (NSFetchRequest<Metadata *> *)fetchRequest {
	return [NSFetchRequest fetchRequestWithEntityName:@"Metadata"];
}

@dynamic author;
@dynamic bafn;
@dynamic blurb;
@dynamic coverArtDescription;
@dynamic coverArtIndex;
@dynamic coverArtURL;
@dynamic dateEdited;
@dynamic firstpublished;
@dynamic firstpublishedDate;
@dynamic forgiveness;
@dynamic forgivenessNumeric;
@dynamic format;
@dynamic genre;
@dynamic group;
@dynamic hashTag;
@dynamic headline;
@dynamic language;
@dynamic languageAsWord;
@dynamic lastModified;
@dynamic myRating;
@dynamic ratingCountTot;
@dynamic series;
@dynamic seriesnumber;
@dynamic source;
@dynamic starRating;
@dynamic title;
@dynamic tuid;
@dynamic userEdited;
@dynamic cover;
@dynamic game;
@dynamic ifids;
@dynamic tag;


//- (Ifid *)findOrCreateIfid:(NSString *)ifidstring {
//
//    Ifid *ifid = nil;
//
//    NSError *error = nil;
//    NSArray *fetchedObjects;
//
//    NSFetchRequest *fetchRequest = [Ifid fetchRequest];
//
//    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"ifidString like[c] %@",ifidstring];
//    fetchRequest.includesPropertyValues = YES;
//
//    fetchedObjects = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];
//
//    if (fetchedObjects == nil) {
//        NSLog(@"findOrInsertIfid: %@",error);
//    }
//
//    if (fetchedObjects.count > 1) {
//        NSLog(@"findOrInsertIfid: Found more than one entry with ifidString %@",ifidstring);
//    }
//
//    if (fetchedObjects == nil || fetchedObjects.count == 0) {
//        ifid = (Ifid *) [NSEntityDescription
//                         insertNewObjectForEntityForName:@"Ifid"
//                         inManagedObjectContext:self.managedObjectContext];
//        ifid.ifidString = ifidstring;
//
//    } else ifid = fetchedObjects[0];
//
//    [self addIfidsObject:ifid];
//    
//    return ifid;
//}

- (void)createIfid:(NSString *)ifidstring {
    for (Ifid *ifid in self.ifids) {
        if ([ifid.ifidString isEqualToString:ifidstring])
            return;
    }
    Ifid *ifid = (Ifid *) [NSEntityDescription
                         insertNewObjectForEntityForName:@"Ifid"
                         inManagedObjectContext:self.managedObjectContext];
        ifid.ifidString = ifidstring;

    [self addIfidsObject:ifid];
}

- (void)willSave {
    if (!self.deleted)
        return;
    CSSearchableIndex *index = [CSSearchableIndex defaultSearchableIndex];
    NSString *identifier = self.objectID.URIRepresentation.absoluteString;
    if (!identifier)
        return;
    [index deleteSearchableItemsWithIdentifiers:@[identifier]
                              completionHandler:^(NSError *blockerror){
        if (blockerror) {
            NSLog(@"Deleting searchable item for Metadata failed: %@", blockerror);
        }
    }];
}

@end
