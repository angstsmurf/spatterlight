//
//  ProgressiveMigrator.m
//  Spatterlight
//

#import "ProgressiveMigrator.h"

NSErrorDomain const ProgressiveMigrationErrorDomain = @"net.ccxvii.spatterlight.migration";

@implementation ProgressiveMigrator

// Every versioned model inside the .momd, used as the candidate set when
// looking for the next migration hop.
+ (NSArray<NSManagedObjectModel *> *)allVersionsOfModelNamed:(NSString *)modelName {
    NSMutableArray<NSManagedObjectModel *> *models = [NSMutableArray array];
    NSURL *momdURL = [[NSBundle mainBundle] URLForResource:modelName withExtension:@"momd"];
    if (!momdURL)
        return models;
    NSArray<NSURL *> *contents = [[NSFileManager defaultManager] contentsOfDirectoryAtURL:momdURL
                                                              includingPropertiesForKeys:nil
                                                                                 options:0
                                                                                   error:NULL];
    for (NSURL *url in contents) {
        if ([url.pathExtension isEqualToString:@"mom"]) {
            NSManagedObjectModel *model = [[NSManagedObjectModel alloc] initWithContentsOfURL:url];
            if (model)
                [models addObject:model];
        }
    }
    return models;
}

+ (BOOL)migrateStoreAtURL:(NSURL *)sourceURL
                   ofType:(NSString *)type
                  toModel:(NSManagedObjectModel *)finalModel
                modelName:(NSString *)modelName
                    error:(NSError * __autoreleasing *)error {
    NSArray<NSBundle *> *bundles = @[[NSBundle mainBundle]];
    NSArray<NSManagedObjectModel *> *allModels = [self allVersionsOfModelNamed:modelName];
    // A correct chain can never need more hops than there are model versions.
    NSUInteger maxSteps = allModels.count + 1;

    // Our real store is opened with persistent history tracking enabled. Once a
    // store has been opened with NSPersistentHistoryTrackingKey, every later
    // open must pass it too, or Core Data forces the store into read-only mode
    // (and logs a fault). The migrated temp store inherits that marker from the
    // source, so the migrate/replace operations below must carry the option as
    // well to stay consistent.
    NSDictionary *storeOptions = @{ NSPersistentHistoryTrackingKey: @YES };

    for (NSUInteger step = 0; step < maxSteps; step++) {
        NSDictionary *metadata = [NSPersistentStoreCoordinator metadataForPersistentStoreOfType:type
                                                                                            URL:sourceURL
                                                                                        options:nil
                                                                                          error:NULL];
        // No store at this URL yet (fresh install): nothing to migrate.
        if (!metadata)
            return YES;

        // Already at the final model: done.
        if ([finalModel isConfiguration:nil compatibleWithStoreMetadata:metadata])
            return YES;

        // Find the model that matches the existing store.
        NSManagedObjectModel *sourceModel = [NSManagedObjectModel mergedModelFromBundles:bundles
                                                                       forStoreMetadata:metadata];
        if (!sourceModel) {
            if (error)
                *error = [NSError errorWithDomain:ProgressiveMigrationErrorDomain
                                             code:1
                                         userInfo:@{NSLocalizedDescriptionKey: @"No bundled model matches the existing store; cannot migrate it."}];
            return NO;
        }

        // Pick the next hop: an explicit custom mapping to any later version if
        // one exists, otherwise an inferred mapping straight to the final model.
        NSManagedObjectModel *destinationModel = nil;
        NSMappingModel *mappingModel = nil;
        NSDictionary *sourceHashes = sourceModel.entityVersionHashesByName;
        for (NSManagedObjectModel *candidate in allModels) {
            if ([candidate.entityVersionHashesByName isEqualToDictionary:sourceHashes])
                continue; // same model as the source
            NSMappingModel *explicit = [NSMappingModel mappingModelFromBundles:bundles
                                                               forSourceModel:sourceModel
                                                            destinationModel:candidate];
            if (explicit) {
                destinationModel = candidate;
                mappingModel = explicit;
                break;
            }
        }
        if (!mappingModel) {
            destinationModel = finalModel;
            mappingModel = [NSMappingModel inferredMappingModelForSourceModel:sourceModel
                                                            destinationModel:finalModel
                                                                       error:error];
            if (!mappingModel)
                return NO;
        }

        // Migrate source -> destination into a temporary store.
        NSMigrationManager *manager = [[NSMigrationManager alloc] initWithSourceModel:sourceModel
                                                                    destinationModel:destinationModel];
        NSURL *tempURL = [[sourceURL URLByDeletingLastPathComponent]
                          URLByAppendingPathComponent:[NSString stringWithFormat:@"migration-%@.storedata",
                                                       [[NSProcessInfo processInfo] globallyUniqueString]]];
        if (![manager migrateStoreFromURL:sourceURL
                                     type:type
                                  options:storeOptions
                         withMappingModel:mappingModel
                         toDestinationURL:tempURL
                          destinationType:type
                       destinationOptions:storeOptions
                                    error:error]) {
            [[[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:destinationModel]
             destroyPersistentStoreAtURL:tempURL withType:type options:nil error:NULL];
            return NO;
        }

        // Atomically replace the original store with the migrated one.
        NSPersistentStoreCoordinator *replacer = [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:destinationModel];
        NSError *replaceError = nil;
        BOOL replaced = [replacer replacePersistentStoreAtURL:sourceURL
                                          destinationOptions:storeOptions
                                  withPersistentStoreFromURL:tempURL
                                               sourceOptions:storeOptions
                                                   storeType:type
                                                       error:&replaceError];
        [replacer destroyPersistentStoreAtURL:tempURL withType:type options:nil error:NULL];
        if (!replaced) {
            if (error)
                *error = replaceError;
            return NO;
        }
    }

    if (error)
        *error = [NSError errorWithDomain:ProgressiveMigrationErrorDomain
                                     code:2
                                 userInfo:@{NSLocalizedDescriptionKey: @"Migration did not reach the current model within the expected number of steps."}];
    return NO;
}

@end
