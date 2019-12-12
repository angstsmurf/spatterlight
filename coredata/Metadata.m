//
//  Metadata.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-12.
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
@dynamic myRating;
@dynamic ratingCountTot;
@dynamic series;
@dynamic seriesnumber;
@dynamic source;
@dynamic starRating;
@dynamic title;
@dynamic tuid;
@dynamic userEdited;
@dynamic lastModified;
@dynamic cover;
@dynamic game;
@dynamic tag;
@dynamic ifids;


- (void)addIfidObject:(Ifid *)value {
    NSMutableSet* tempSet = [NSMutableSet setWithSet:self.ifids];
    [tempSet addObject:value];
    self.ifids = [NSSet setWithSet:tempSet];
}

- (void)addIfid:(NSSet *)values {
    NSMutableSet* tempSet = [NSMutableSet setWithSet:self.ifids];
    [tempSet unionSet:values];
    self.ifids = [NSSet setWithSet:tempSet];
}

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

    } else ifid = [fetchedObjects objectAtIndex:0];

    [self addIfidObject:ifid];

    return ifid;
}

@end
