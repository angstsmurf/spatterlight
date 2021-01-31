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


@implementation Metadata

@dynamic author;
@dynamic averageRating;
@dynamic bafn;
@dynamic blurb;
@dynamic coverArtURL;
@dynamic coverArtDescription;
@dynamic dateEdited;
@dynamic firstpublished;
@dynamic firstpublishedDate;
@dynamic forgiveness;
@dynamic forgivenessNumeric;
@dynamic format;
@dynamic genre;
@dynamic group;
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
@dynamic games;
@dynamic ifids;
@dynamic tag;


- (Ifid *)findOrCreateIfid:(NSString *)ifidstring {

    Ifid *ifid = nil;

    NSError *error = nil;
    NSArray *fetchedObjects;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];

    fetchRequest.entity = [NSEntityDescription entityForName:@"Ifid" inManagedObjectContext:self.managedObjectContext];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"ifidString like[c] %@",ifidstring];
    fetchRequest.includesPropertyValues = YES;

    fetchedObjects = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];

    if (fetchedObjects == nil) {
        NSLog(@"findOrInsertIfid: %@",error);
    }

    if (fetchedObjects.count > 1) {
        NSLog(@"findOrInsertIfid: Found more than one entry with ifidString %@",ifidstring);
    }

    if (fetchedObjects == nil || fetchedObjects.count == 0) {
        ifid = (Ifid *) [NSEntityDescription
                         insertNewObjectForEntityForName:@"Ifid"
                         inManagedObjectContext:self.managedObjectContext];
        ifid.ifidString = ifidstring;

    } else ifid = fetchedObjects[0];

    [self addIfidsObject:ifid];
    
    return ifid;
}

@end
