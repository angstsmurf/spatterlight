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

+ (nullable Metadata *)fetchMetadataForHash:(NSString *)hash inContext:(NSManagedObjectContext *)context {
    NSError *error = nil;
    NSArray *fetchedObjects;

    NSFetchRequest *fetchRequest = [Metadata fetchRequest];

    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"hashTag like[c] %@",hash];

    fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"fetchMetadataForHash: %@",error);
    }

    if (fetchedObjects.count > 1) {
        NSLog(@"fetchMetadataForHash: Found more than one has object with hashTag %@",hash);
    } else if (fetchedObjects.count == 0) {
        return nil;
    }

    return (Metadata *)fetchedObjects[0];
}

+ (nullable Game *)fetchGameForHash:(NSString *)hash inContext:(NSManagedObjectContext *)context {
    NSError *error = nil;
    NSArray *fetchedObjects;

    NSFetchRequest *fetchRequest = [Game fetchRequest];

    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"hashTag like[c] %@",hash];

    fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"Problem! %@",error);
    }

    if (fetchedObjects.count > 1)
    {
        NSLog(@"Found more than one entry (%ld) with hashTag %@", fetchedObjects.count, hash);
    }
    else if (fetchedObjects.count == 0)
    {
        return nil;
    }

    return fetchedObjects.firstObject;
}

+ (NSArray *)fetchObjects:(NSString *)entityName predicate:(nullable NSString *)predicate inContext:(NSManagedObjectContext *)context {

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    NSEntityDescription *entity = [NSEntityDescription entityForName:entityName inManagedObjectContext:context];
    fetchRequest.entity = entity;
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
    NSError *error = nil;
    NSArray *fetchedObjects;

    NSFetchRequest *fetchRequest = [Game fetchRequest];

    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"ifid like[c] %@",ifid];

    fetchedObjects = (NSArray<Game *> *)[context executeFetchRequest:fetchRequest error:&error];
    if (error != nil) {
        NSLog(@"fetchGamesForIfid: %@",error);
    }

    return fetchedObjects;
}

+ (nullable NSSet<Metadata *> *)fetchMetadataForIfid:(NSString *)ifid inContext:(NSManagedObjectContext *)context {
    NSError *error = nil;
    NSArray *fetchedObjects;

    NSFetchRequest *fetchRequest = [Ifid fetchRequest];

    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"ifidString like[c] %@",ifid];

    fetchedObjects = (NSArray<Ifid *> *)[context executeFetchRequest:fetchRequest error:&error];
    if (error != nil) {
        NSLog(@"fetchMetadataForIfid: %@",error);
    }
    NSMutableSet<Metadata *> *metas = [[NSMutableSet alloc] init];
    for (Ifid *match in fetchedObjects) {
        [metas addObject:match.metadata];
    }

    return metas;
}

+ (nullable Theme *)findTheme:(NSString *)name inContext:(NSManagedObjectContext *)context {

    NSError *error = nil;

    NSFetchRequest *fetchRequest = [Theme fetchRequest];

    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"name like[c] %@", name];

    NSArray *fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"findTheme: inContext: %@",error);
    }

    if (fetchedObjects.count > 1)
    {
        NSLog(@"findTheme: inContext: Found more than one Theme object with name %@ (total %ld)",name, fetchedObjects.count);
    } else if (fetchedObjects.count == 0) {
        NSLog(@"findTheme: inContext: Found no Ifid object with with name %@", name);
        return nil;
    }

    return fetchedObjects[0];
}

@end
