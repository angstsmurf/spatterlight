//
//  ReadOnlyStore.m
//  Spatterlight
//

#import "ReadOnlyStore.h"
#import "Game.h"
#import "NSString+Categories.h"

@implementation ReadOnlyStore

+ (NSPersistentContainer *)persistentContainerWithLogName:(NSString *)logName {
    NSPersistentContainer *container = [[NSPersistentContainer alloc] initWithName:@"Spatterlight"];

    NSString *groupIdentifier =
    [[NSBundle mainBundle] objectForInfoDictionaryKey:@"GroupIdentifier"];

    NSURL *url = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:groupIdentifier];

    url = [url URLByAppendingPathComponent:@"Spatterlight.storedata" isDirectory:NO];

    NSPersistentStoreDescription *description = [[NSPersistentStoreDescription alloc] initWithURL:url];

    description.readOnly = YES;
    description.shouldMigrateStoreAutomatically = NO;

    container.persistentStoreDescriptions = @[ description ];

    [container loadPersistentStoresWithCompletionHandler:^(NSPersistentStoreDescription *aDescription, NSError *error) {
        if (error != nil) {
            NSLog(@"%@: Failed to load Core Data stack at %@: %@", logName, url.path, error);
        }
    }];

    return container;
}

+ (nullable Game *)gameMatching:(NSPredicate *)predicate
                      inContext:(NSManagedObjectContext *)context
                          error:(NSError **)error {
    NSFetchRequest *fetchRequest = [Game fetchRequest];
    fetchRequest.predicate = predicate;
    NSError *fetchError = nil;
    NSArray<Game *> *fetchedObjects = [context executeFetchRequest:fetchRequest error:&fetchError];
    if (fetchedObjects == nil) {
        NSLog(@"ReadOnlyStore: fetch for %@ failed: %@", predicate, fetchError);
        if (error)
            *error = fetchError;
    } else if (fetchedObjects.count > 1) {
        NSLog(@"ReadOnlyStore: Found %ld games matching %@", fetchedObjects.count, predicate);
    }
    return fetchedObjects.firstObject;
}

+ (nullable Game *)gameForURL:(NSURL *)url
                    inContext:(NSManagedObjectContext *)context
                      hashTag:(NSString **)hashTag
                        error:(NSError **)error {
    NSError *fetchError = nil;
    Game *game = [self gameMatching:[NSPredicate predicateWithFormat:@"path ==[c] %@", url.path]
                          inContext:context
                              error:&fetchError];
    if (fetchError) {
        if (error)
            *error = fetchError;
        return nil;
    }
    if (game)
        return game;

    NSString *signature = url.path.signatureFromFile;
    if (hashTag)
        *hashTag = signature;
    if (!signature.length)
        return nil;
    return [self gameMatching:[NSPredicate predicateWithFormat:@"hashTag ==[c] %@", signature]
                    inContext:context
                        error:NULL];
}

+ (void)checkCategoryMethods:(NSArray<NSArray *> *)entries target:(NSString *)targetName {
    for (NSArray *entry in entries) {
        Class cls = entry[0];
        if (![cls instancesRespondToSelector:NSSelectorFromString(entry[1])]) {
            NSLog(@"%@: -[%@ %@] is missing. Is %@ in the %@ target?", targetName, cls, entry[1], entry[2], targetName);
            NSAssert(NO, @"-[%@ %@] is missing. Add %@ to the %@ target.", cls, entry[1], entry[2], targetName);
        }
    }
}

@end
