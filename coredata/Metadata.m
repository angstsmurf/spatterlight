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
    if (self.deleted)
        [self removeFromSpotlightIndex];
}

@end
