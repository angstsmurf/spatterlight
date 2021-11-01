//
//  MyCoreDataCoreSpotlightDelegate.m
//  Spatterlight
//
//  Created by Administrator on 2021-10-11.
//
#import <Cocoa/Cocoa.h>
#import <CoreData/CoreData.h>
#import <CoreSpotlight/CoreSpotlight.h>

#import "MyCoreDataCoreSpotlightDelegate.h"
#import "Image.h"
#import "Metadata.h"
#import "Game.h"
#import "Ifid.h"
#import "NSImage+Categories.h"


@implementation MyCoreDataCoreSpotlightDelegate

/* CoreSpotlight domain identifer; default is the store's identifier */
- (NSString *)domainIdentifier {
    return @"net.ccxvii.spatterlight";
}

/* CoreSpotlight index name; default nil */
- (nullable NSString *)indexName {
    return @"spatterlight-index";
}

+ (NSString *)UTIFromFormat:(NSString *)format {
    return [NSString stringWithFormat:@"public.%@", format];
}

- (nullable CSSearchableItemAttributeSet *)attributeSetForObject:(NSManagedObject*)object {
    CSSearchableItemAttributeSet *attributeSet;

    if ([object isKindOfClass:[Image class]]) {
        Image *image = (Image *)object;
        if (!image.imageDescription.length)
            return nil;
        if (!image.metadata.count || !image.metadata.anyObject.ifids.count)
            return nil;
        attributeSet = [[CSSearchableItemAttributeSet alloc] initWithItemContentType:@"public.image"];
        attributeSet.displayName = [NSString stringWithFormat:@"Cover image of %@", image.metadata.anyObject.title];
        attributeSet.contentDescription = image.imageDescription;
    } else if ([object isKindOfClass:[Metadata class]]) {
        Metadata *metadata = (Metadata *)object;
        if (!metadata.format.length)
            return nil;
        attributeSet = [[CSSearchableItemAttributeSet alloc] initWithItemContentType:[MyCoreDataCoreSpotlightDelegate UTIFromFormat:metadata.format]];
        if (!metadata.title.length)
            return nil;
        attributeSet.displayName = metadata.title;
        NSInteger rating = metadata.starRating.integerValue;
        if (!rating)
            rating = metadata.myRating.integerValue;
        if (rating)
            attributeSet.rating = @(rating);
        attributeSet.contentDescription = metadata.blurb;

        attributeSet.artist = metadata.author;
        attributeSet.genre = metadata.genre;
        attributeSet.originalFormat = metadata.format;
        if (metadata.languageAsWord.length)
            attributeSet.languages = @[metadata.languageAsWord];

        CSCustomAttributeKey *ifids = [[CSCustomAttributeKey alloc] initWithKeyName:@"net_ccxvii_spatterlight_ifid"
                                                                         searchable:YES
                                                                searchableByDefault:YES
                                                                             unique:YES
                                                                        multiValued:YES];

        NSArray<NSString *> *ifidArray = nil;
        for (Ifid *ifidobject in metadata.ifids) {
            if (ifidArray == nil) {
                ifidArray = @[ifidobject.ifidString];
            } else {
                ifidArray = [ifidArray arrayByAddingObject:ifidobject.ifidString];
            }
        }
        if (ifidArray)
            [attributeSet setValue:ifidArray forCustomKey:ifids];

        [MyCoreDataCoreSpotlightDelegate addKeyword:metadata.group
            toCustomAttribute:@"group"
            inSet:attributeSet];
        [MyCoreDataCoreSpotlightDelegate addKeyword:metadata.forgiveness toCustomAttribute:@"forgiveness" inSet:attributeSet];
        [MyCoreDataCoreSpotlightDelegate addKeyword:metadata.series toCustomAttribute:@"series" inSet:attributeSet];
        [MyCoreDataCoreSpotlightDelegate addKeyword:metadata.firstpublished toCustomAttribute:@"year" inSet:attributeSet];
        [MyCoreDataCoreSpotlightDelegate addKeyword:metadata.seriesnumber toCustomAttribute:@"indexInSeries" inSet:attributeSet];
        [MyCoreDataCoreSpotlightDelegate addKeyword:metadata.languageAsWord toCustomAttribute:@"language" inSet:attributeSet];
        [MyCoreDataCoreSpotlightDelegate addKeyword:metadata.headline toCustomAttribute:@"headline" inSet:attributeSet];
    } else {
        return nil;
    }
    return attributeSet;
}

+ (void)addKeyword:(id)keyword toCustomAttribute:(NSString *)name inSet:(CSSearchableItemAttributeSet *)attributeSet {
    if (keyword == nil)
        return;
    CSCustomAttributeKey *customKey =
    [[CSCustomAttributeKey alloc] initWithKeyName:[NSString stringWithFormat:@"net_ccxvii_spatterlight_%@", name]
                                       searchable:YES
                              searchableByDefault:YES
                                           unique:YES
                                      multiValued:NO];
    [attributeSet setValue:keyword forCustomKey:customKey];
}

@end
