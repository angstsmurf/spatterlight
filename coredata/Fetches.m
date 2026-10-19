//
//  Fetches.m
//  Spatterlight
//
//  Created by Administrator on 2025-09-05.
//

#import "Game.h"
#import "Metadata.h"
#import "Ifid.h"
#import "Theme.h"

#import "Fetches.h"

@implementation Fetches

// Runs request and returns the first match. A failed fetch and more than
// one match are logged (prefixed with caller): the callers look up keys that
// should be unique.
+ (nullable id)firstResultOfFetch:(NSFetchRequest *)request
                        inContext:(NSManagedObjectContext *)context
                           caller:(NSString *)caller {
    NSError *error = nil;
    NSArray *fetchedObjects = [context executeFetchRequest:request error:&error];
    if (fetchedObjects == nil)
        NSLog(@"%@: %@", caller, error);
    else if (fetchedObjects.count > 1)
        NSLog(@"%@: Found more than one match (%ld) for %@", caller, fetchedObjects.count, request.predicate);
    return fetchedObjects.firstObject;
}

+ (nullable Metadata *)fetchMetadataForHash:(NSString *)hash inContext:(NSManagedObjectContext *)context {
    if (!hash.length)
        return nil;
    NSFetchRequest *fetchRequest = [Metadata fetchRequest];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"hashTag ==[c] %@", hash];
    return [self firstResultOfFetch:fetchRequest inContext:context caller:@"fetchMetadataForHash"];
}

+ (nullable Game *)fetchGameForHash:(NSString *)hash inContext:(NSManagedObjectContext *)context {
    if (!hash.length)
        return nil;
    NSFetchRequest *fetchRequest = [Game fetchRequest];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"hashTag ==[c] %@", hash];
    return [self firstResultOfFetch:fetchRequest inContext:context caller:@"fetchGameForHash"];
}

+ (NSArray *)fetchObjects:(NSString *)entityName predicate:(nullable NSString *)predicate inContext:(NSManagedObjectContext *)context {

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    NSEntityDescription *entity = [NSEntityDescription entityForName:entityName inManagedObjectContext:context];
    fetchRequest.entity = entity;
    if (predicate)
        fetchRequest.predicate = [NSPredicate predicateWithFormat:predicate];

    NSError *error = nil;
    NSArray<NSManagedObject *> *fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"Problem! %@",error);
        fetchedObjects = @[];
    }

    return fetchedObjects;
}

+ (nullable NSArray<Game *> *)fetchGamesForIfid:(NSString *)ifid inContext:(NSManagedObjectContext *)context {
    if (!ifid.length)
        return @[];
    NSError *error = nil;
    NSArray *fetchedObjects;

    NSFetchRequest *fetchRequest = [Game fetchRequest];

    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"ifid ==[c] %@",ifid];

    fetchedObjects = (NSArray<Game *> *)[context executeFetchRequest:fetchRequest error:&error];
    if (error != nil) {
        NSLog(@"fetchGamesForIfid: %@",error);
    }

    return fetchedObjects;
}

+ (nullable NSSet<Metadata *> *)fetchMetadataForIfid:(NSString *)ifid inContext:(NSManagedObjectContext *)context {
    if (!ifid.length)
        return [NSSet set];
    NSError *error = nil;
    NSArray *fetchedObjects;

    NSFetchRequest *fetchRequest = [Ifid fetchRequest];

    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"ifidString ==[c] %@",ifid];

    fetchedObjects = (NSArray<Ifid *> *)[context executeFetchRequest:fetchRequest error:&error];
    if (error != nil) {
        NSLog(@"fetchMetadataForIfid: %@",error);
    }
    NSMutableSet<Metadata *> *metas = [[NSMutableSet alloc] init];
    for (Ifid *match in fetchedObjects) {
        // An orphaned Ifid (or one caught mid-migration) has no metadata.
        if (match.metadata)
            [metas addObject:match.metadata];
    }

    return metas;
}

+ (nullable Theme *)findTheme:(NSString *)name inContext:(NSManagedObjectContext *)context {
    if (!name.length)
        return nil;
    NSFetchRequest *fetchRequest = [Theme fetchRequest];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"name ==[c] %@", name];
    return [self firstResultOfFetch:fetchRequest inContext:context caller:@"findTheme"];
}

+ (NSArray<Game *> *)fetchAllGamesAsFaultsInContext:(NSManagedObjectContext *)context {
    NSFetchRequest *fetchRequest = [Game fetchRequest];
    fetchRequest.includesPropertyValues = NO;
    NSError *error = nil;
    NSArray<Game *> *games = [context executeFetchRequest:fetchRequest error:&error];
    if (!games)
        NSLog(@"fetchAllGamesAsFaultsInContext: %@", error);
    return games ?: @[];
}

+ (NSArray<Theme *> *)fetchUserThemesInContext:(NSManagedObjectContext *)context {
    return [self fetchObjects:@"Theme" predicate:@"editable == YES" inContext:context];
}

@end
