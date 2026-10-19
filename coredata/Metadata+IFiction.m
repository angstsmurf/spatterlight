//
//  Metadata+IFiction.m
//  Spatterlight
//

#import "Metadata+IFiction.h"
#import "Ifid.h"
#import "IFStory.h"
#import "IFIdentification.h"
#import "IFBibliographic.h"
#import "IFCoverDescription.h"
#import "IFDB.h"

@implementation Metadata (IFiction)

- (void)addInfoFromStory:(IFStory *)story {
    IFIdentification *identification = story.identification;
    if (identification.bafn)
        self.bafn = identification.bafn;
    if (identification.ifids.count)
        [self addIfidStrings:identification.ifids];

    if (story.bibliographic)
        [self addInfoFromBibliographic:story.bibliographic];

    IFDB *ifdb = story.ifdb;
    if (ifdb.tuid.length)
        self.tuid = ifdb.tuid;
    if (ifdb.starRating.length)
        self.starRating = ifdb.starRating;
    if (ifdb.coverArtURL.length)
        self.coverArtURL = ifdb.coverArtURL;

    if (story.coverDescription.coverArtDescription.length)
        self.coverArtDescription = story.coverDescription.coverArtDescription;
}

- (void)addInfoFromBibliographic:(IFBibliographic *)bibliographic {
    if (bibliographic.title.length)
        self.title = bibliographic.title;
    if (bibliographic.author.length)
        self.author = bibliographic.author;
    if (bibliographic.language.length)
        self.language = bibliographic.language;
    if (bibliographic.languageAsWord.length)
        self.languageAsWord = bibliographic.languageAsWord;
    if (bibliographic.headline.length)
        self.headline = bibliographic.headline;
    if (bibliographic.blurb.length)
        self.blurb = bibliographic.blurb;
    if (bibliographic.firstPublished.length)
        self.firstpublished = bibliographic.firstPublished;
    if (bibliographic.firstPublishedDate)
        self.firstpublishedDate = bibliographic.firstPublishedDate;
    if (bibliographic.genre.length)
        self.genre = bibliographic.genre;
    if (bibliographic.group.length)
        self.group = bibliographic.group;
    if (bibliographic.series.length)
        self.series = bibliographic.series;
    if (bibliographic.seriesnumber.length)
        self.seriesnumber = bibliographic.seriesnumber;
    if (bibliographic.forgiveness.length)
        self.forgiveness = bibliographic.forgiveness;
    if (bibliographic.forgivenessNumeric)
        self.forgivenessNumeric = bibliographic.forgivenessNumeric;
}

- (void)addIfidStrings:(NSSet<NSString *> *)ifids {
    NSManagedObjectContext *context = self.managedObjectContext;
    if (context == nil)
        return;

    NSMutableSet<NSString *> *existing = [[self.ifids valueForKey:@"ifidString"] mutableCopy];

    for (NSString *ifid in ifids) {
        // Placeholders used by the info panel, never real IFIDs
        if ([ifid isEqualToString:@"DUMMYmanySelected"] || [ifid isEqualToString:@"DUMMYnoneSelected"])
            continue;
        if ([existing containsObject:ifid])
            continue;
        [existing addObject:ifid];

        // The library can contain any number of Ifid objects with identical ifidStrings,
        // as long as they are attached to different Metadata objects.
        Ifid *ifidObj = (Ifid *) [NSEntityDescription
                                  insertNewObjectForEntityForName:@"Ifid"
                                  inManagedObjectContext:context];
        ifidObj.ifidString = ifid;
        ifidObj.metadata = self;
    }
}

@end
