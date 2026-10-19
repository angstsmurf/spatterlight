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

// The coordinator initializer and the start/stop calls are missing from SDKs
// up to 11.3, which the old-macOS build still uses.
+ (instancetype)exporterForStoreWithDescription:(NSPersistentStoreDescription *)description
                                    coordinator:(NSPersistentStoreCoordinator *)coordinator {
#if __MAC_OS_X_VERSION_MAX_ALLOWED > 110300
    return [[self alloc] initForStoreWithDescription:description coordinator:coordinator];
#else
    return [[self alloc] initForStoreWithDescription:description model:coordinator.managedObjectModel];
#endif
}

- (void)startIndexing {
#if __MAC_OS_X_VERSION_MAX_ALLOWED > 110300
    [self startSpotlightIndexing];
#endif
}

- (void)stopIndexing {
#if __MAC_OS_X_VERSION_MAX_ALLOWED > 110300
    [self stopSpotlightIndexing];
#endif
}

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

        attributeSet.thumbnailData = (NSData *)metadata.cover.data;

        attributeSet.artist = metadata.author;
        attributeSet.genre = metadata.genre;
        attributeSet.originalFormat = metadata.format;
        if (metadata.languageAsWord.length)
            attributeSet.languages = @[metadata.languageAsWord];

        NSMutableArray<NSString *> *ifidArray = [NSMutableArray arrayWithCapacity:metadata.ifids.count];
        for (Ifid *ifidobject in metadata.ifids) {
            if (ifidobject.ifidString.length)
                [ifidArray addObject:ifidobject.ifidString];
        }
        if (ifidArray.count)
            [attributeSet setValue:ifidArray forCustomKey:[MyCoreDataCoreSpotlightDelegate customKeyNamed:@"ifid" multiValued:YES]];

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

// CSCustomAttributeKeys are immutable, so make each one once instead of per
// keyword per object on every re-index.
+ (CSCustomAttributeKey *)customKeyNamed:(NSString *)name multiValued:(BOOL)multiValued {
    static NSMutableDictionary<NSString *, CSCustomAttributeKey *> *keys;
    @synchronized (self) {
        if (!keys)
            keys = [NSMutableDictionary new];
        CSCustomAttributeKey *key = keys[name];
        if (!key) {
            key = [[CSCustomAttributeKey alloc] initWithKeyName:[NSString stringWithFormat:@"net_ccxvii_spatterlight_%@", name]
                                                     searchable:YES
                                            searchableByDefault:YES
                                                         unique:YES
                                                    multiValued:multiValued];
            keys[name] = key;
        }
        return key;
    }
}

+ (void)addKeyword:(id)keyword toCustomAttribute:(NSString *)name inSet:(CSSearchableItemAttributeSet *)attributeSet {
    if (keyword == nil)
        return;
    [attributeSet setValue:keyword forCustomKey:[MyCoreDataCoreSpotlightDelegate customKeyNamed:name multiValued:NO]];
}

@end
