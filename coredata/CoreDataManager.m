//
//  CoreDataManager.m
//  Spatterlight
//
//  https://cocoacasts.com/bring-your-own
//
//

#import "CoreDataManager.h"
#import "MyCoreDataCoreSpotlightDelegate.h"
#import "FolderAccess.h"
#import "AppDelegate.h"

@interface CoreDataManager () {
    NSString *modelName;
    NSManagedObjectContext *privateManagedObjectContext;
}
@end

@implementation CoreDataManager

- (instancetype)initWithModelName:(NSString *)aModelName {
    self = [super init];
    if (self) {
        modelName = aModelName;
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(startIndexing:)
                                                     name:@"StartIndexing"
                                                   object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(stopIndexing:)
                                                     name:@"StopIndexing"
                                                   object:nil];
    }
    return self;
}

// Creates if necessary and returns the managed object model for the application.
- (NSManagedObjectModel *)managedObjectModel
{
    if (_managedObjectModel) {
        return _managedObjectModel;
    }

    NSURL *modelURL = [[NSBundle mainBundle] URLForResource:modelName withExtension:@"momd"];

    if (!modelURL) {
        NSLog(@"Unable to Find Data Model!");
        return nil;
    }

    _managedObjectModel = [[NSManagedObjectModel alloc] initWithContentsOfURL:modelURL];

    if (!_managedObjectModel) {
        NSLog(@"Unable to Load Data Model!");
        return nil;
    }

    return _managedObjectModel;
}

// Every versioned model inside the .momd, used as the candidate set when
// looking for the next migration hop.
- (NSArray<NSManagedObjectModel *> *)allModelVersions {
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

// Progressively (step by step) migrate the store at sourceURL up to finalModel,
// chaining through intermediate model versions instead of doing a single
// source->destination hop. At each step we prefer an explicit (custom) mapping
// model to any later version - this is what lets the IfidToHashMigrationPolicy
// run when an old store passes through "Spatterlight 15" - and otherwise infer
// a mapping straight to the final model. Each step migrates into a temporary
// store which then atomically replaces the original, so a failure never
// corrupts the source store. Returns YES if the store ends up compatible with
// finalModel (including the no-op case where it already was), NO on failure.
- (BOOL)progressivelyMigrateStoreAtURL:(NSURL *)sourceURL
                                ofType:(NSString *)type
                               toModel:(NSManagedObjectModel *)finalModel
                                 error:(NSError * __autoreleasing *)error {
    NSArray<NSBundle *> *bundles = @[[NSBundle mainBundle]];
    NSArray<NSManagedObjectModel *> *allModels = [self allModelVersions];
    // A correct chain can never need more hops than there are model versions.
    NSUInteger maxSteps = allModels.count + 1;

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
                *error = [NSError errorWithDomain:@"net.ccxvii.spatterlight.migration"
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
                                  options:nil
                         withMappingModel:mappingModel
                         toDestinationURL:tempURL
                          destinationType:type
                       destinationOptions:nil
                                    error:error]) {
            [[[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:destinationModel]
             destroyPersistentStoreAtURL:tempURL withType:type options:nil error:NULL];
            return NO;
        }

        // Atomically replace the original store with the migrated one.
        NSPersistentStoreCoordinator *replacer = [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:destinationModel];
        NSError *replaceError = nil;
        BOOL replaced = [replacer replacePersistentStoreAtURL:sourceURL
                                          destinationOptions:nil
                                  withPersistentStoreFromURL:tempURL
                                               sourceOptions:nil
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
        *error = [NSError errorWithDomain:@"net.ccxvii.spatterlight.migration"
                                     code:2
                                 userInfo:@{NSLocalizedDescriptionKey: @"Migration did not reach the current model within the expected number of steps."}];
    return NO;
}

// Returns the persistent store coordinator for the application. This implementation creates and return a coordinator, having added the store for the application to it. (The directory for the store is created, if necessary.)
- (NSPersistentStoreCoordinator *)persistentStoreCoordinator
{
    if (_persistentStoreCoordinator) {
        return _persistentStoreCoordinator;
    }

    //    BOOL needDeleteOld  = false;

    NSManagedObjectModel *mom = self.managedObjectModel;
    if (!mom) {
        NSLog(@"%@:%@ ERROR! No model to generate a store from!", [self class], NSStringFromSelector(_cmd));
        return nil;
    }

    NSPersistentStoreCoordinator *coordinator = [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:mom];

    NSDictionary *options = @{ NSMigratePersistentStoresAutomaticallyOption: @(YES),
                               NSInferMappingModelAutomaticallyOption: @(YES) };

    NSURL *groupURL = nil;
    BOOL needMigrate = NO;

    NSURL *targetURL = [self storeURLNeedMigrate:&needMigrate groupURL:&groupURL];

    NSError *error = nil;

    // Chain through intermediate model versions first, so custom mapping models
    // (e.g. the IfidToHashMigrationPolicy that runs at "Spatterlight 15") get a
    // chance to run. Best effort: if it fails, fall through to the standard
    // automatic/inferred migration below, which still opens the store.
    if (![self progressivelyMigrateStoreAtURL:targetURL ofType:NSSQLiteStoreType toModel:mom error:&error]) {
        NSLog(@"Progressive migration of %@ did not complete: %@. Falling back to automatic migration.", targetURL.path, error);
        error = nil;
    }

    NSPersistentStore *store = [coordinator addPersistentStoreWithType:NSSQLiteStoreType configuration:nil URL:targetURL options:options error:&error];

    if (!store) {
        [NSApp presentError:error];
        return nil;
    }

    // do the migrate job from local store to a group store.
    if (needMigrate) {
        error = nil;
        NSManagedObjectContext *context = [[NSManagedObjectContext alloc] initWithConcurrencyType:NSMainQueueConcurrencyType];
        context.persistentStoreCoordinator = coordinator;
        [coordinator migratePersistentStore:store toURL:groupURL options:options withType:NSSQLiteStoreType error:&error];
        if (error != nil) {
            NSLog(@"Error during Core Data migration to group folder: %@, %@", error, error.userInfo);
            abort();
        }
    }

    _persistentStoreCoordinator = coordinator;

    return _persistentStoreCoordinator;
}

- (NSURL *)storeURLNeedMigrate:(BOOL *)needMigrate groupURL:(NSURL * __autoreleasing *)groupURL{

    NSURL *oldURL = [[self applicationFilesDirectory] URLByAppendingPathComponent:@"Spatterlight.storedata" isDirectory:NO];

    NSURL *applicationFilesDirectory;
    NSFileManager *fileManager = [NSFileManager defaultManager];

    NSString *teamPrefix =
    [[NSBundle mainBundle] objectForInfoDictionaryKey:@"TeamPrefix"];
    NSString *groupIdentifier =
    [[NSBundle mainBundle] objectForInfoDictionaryKey:@"GroupIdentifier"];

    if (teamPrefix.length) {
        applicationFilesDirectory = [fileManager containerURLForSecurityApplicationGroupIdentifier:groupIdentifier];
    } else {
        applicationFilesDirectory = [[fileManager URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:nil] URLByAppendingPathComponent:@"Spatterlight" isDirectory:YES];
    }

    NSError *error = nil;

    NSDictionary *properties = [applicationFilesDirectory resourceValuesForKeys:@[NSURLIsDirectoryKey] error:&error];

    if (!properties) {
        BOOL ok = NO;
        if (error.code == NSFileReadNoSuchFileError) {
            ok = [fileManager createDirectoryAtPath:applicationFilesDirectory.path withIntermediateDirectories:YES attributes:nil error:&error];
        }
        if (!ok) {
            [NSApp presentError:error];
            NSLog(@"Error: %@", error);
            return nil;
        }
    } else {
        if (![properties[NSURLIsDirectoryKey] boolValue]) {
            NSString *failureDescription = [NSString stringWithFormat:@"Expected a folder to store application data, found a file (%@).", applicationFilesDirectory.path];

            NSMutableDictionary *dict = [NSMutableDictionary dictionary];
            [dict setValue:failureDescription forKey:NSLocalizedDescriptionKey];
            error = [NSError errorWithDomain:@"YOUR_ERROR_DOMAIN" code:101 userInfo:dict];

            [NSApp presentError:error];
            return nil;
        }
    }

    *groupURL = [applicationFilesDirectory URLByAppendingPathComponent:@"Spatterlight.storedata" isDirectory:NO];

    NSURL *targetURL = *groupURL;

    if ([fileManager fileExistsAtPath:(*groupURL).path]) {
        // The Core Data store already exists where we want it
        *needMigrate = NO;
    } else if ([fileManager fileExistsAtPath:oldURL.path]) {
        // No group container Core Data store exists, but there
        // is one in the non-sandboxed Application Support directory,
        // so we try to migrate it.
        targetURL = oldURL;
        *needMigrate = YES;
    }

    return targetURL;
}

- (NSPersistentContainer *)persistentContainer
{
    if (_persistentContainer) {
        return _persistentContainer;
    }

    NSURL *groupURL = nil;
    BOOL needMigrate = NO;

    NSURL *targetURL = [self storeURLNeedMigrate:&needMigrate groupURL:&groupURL];

    if (needMigrate) {
        [FolderAccess askForAccessToURL:targetURL andThenRunBlock:^{}];
        if (![[NSFileManager defaultManager] isReadableFileAtPath:targetURL.path]) {
            needMigrate = NO;
            targetURL = groupURL;
        }
    }

    _persistentContainer = [[NSPersistentContainer alloc] initWithName:modelName];

    // Chain through intermediate model versions before the container opens the
    // store, so custom mapping models (e.g. the IfidToHashMigrationPolicy that
    // runs at "Spatterlight 15") get a chance to run. Best effort: on failure we
    // fall through to the container's own automatic/inferred migration below.
    NSError *migrationError = nil;
    if (![self progressivelyMigrateStoreAtURL:targetURL
                                       ofType:NSSQLiteStoreType
                                      toModel:_persistentContainer.managedObjectModel
                                        error:&migrationError]) {
        NSLog(@"Progressive migration of %@ did not complete: %@. Falling back to automatic migration.", targetURL.path, migrationError);
    }

    NSPersistentStoreDescription *description = [[NSPersistentStoreDescription alloc] initWithURL:targetURL];

    description.shouldMigrateStoreAutomatically = YES;
    description.shouldInferMappingModelAutomatically = YES;

    if (@available(macOS 10.15, *)) {
        [description setOption:@YES forKey:NSPersistentHistoryTrackingKey];
#if __MAC_OS_X_VERSION_MAX_ALLOWED > 110300
        _spotlightDelegate = [[MyCoreDataCoreSpotlightDelegate alloc] initForStoreWithDescription:description coordinator:_persistentContainer.persistentStoreCoordinator];
#else
        _spotlightDelegate = [[MyCoreDataCoreSpotlightDelegate alloc] initForStoreWithDescription:description model:_persistentContainer.managedObjectModel];
#endif

        [description setOption:_spotlightDelegate forKey:NSCoreDataCoreSpotlightExporter];
        [description setOption:@YES forKey:NSPersistentStoreRemoteChangeNotificationPostOptionKey];
    }

    _persistentContainer.persistentStoreDescriptions = @[ description ];

    NSPersistentContainer *blockContainer = _persistentContainer;

    [blockContainer loadPersistentStoresWithCompletionHandler:^(NSPersistentStoreDescription *aDescription, NSError *error) {

        if (error != nil) {
            NSLog(@"Failed to load Core Data stack: %@", error);
            return;
        }

        if (blockContainer.viewContext.undoManager == nil) {
            NSUndoManager *newManager = [[NSUndoManager alloc] init];
            newManager.levelsOfUndo = 10;
            blockContainer.viewContext.undoManager = newManager;
        }

        blockContainer.viewContext.mergePolicy = NSMergeByPropertyObjectTrumpMergePolicy;
        blockContainer.viewContext.automaticallyMergesChangesFromParent = YES;

        // If needed, migrate to a group store.
        if (needMigrate) {
            error = nil;
            NSPersistentStoreCoordinator *coordinator = blockContainer.persistentStoreCoordinator;
            NSPersistentStore *store = [coordinator persistentStoreForURL:targetURL];
            [coordinator migratePersistentStore:store toURL:groupURL options:@{ NSMigratePersistentStoresAutomaticallyOption: @(YES), NSInferMappingModelAutomaticallyOption: @(YES)} withType:NSSQLiteStoreType error:&error];
            // We could delete the old store here, but it doesn't really hurt to keep it,
            // and it helps to have a non-sandboxed store around for development purposes.
            if (error != nil) {
                NSLog(@"Error during Core Data migration to group folder: %@, %@", error, error.userInfo);
                abort();
            } else {
                NSLog(@"Successfully migrated store at %@ to %@", targetURL.absoluteString, groupURL.absoluteString);
            }
        }
    }];

    return _persistentContainer;
}

// Returns the managed object context for the application (which is already bound to the persistent store coordinator for the application.)
- (NSManagedObjectContext *)privateManagedObjectContext
{
    if (privateManagedObjectContext) {
        return privateManagedObjectContext;
    }

    NSPersistentStoreCoordinator *coordinator = self.persistentStoreCoordinator;
    if (!coordinator) {
        NSMutableDictionary *dict = [NSMutableDictionary dictionary];
        [dict setValue:@"Failed to initialize the store" forKey:NSLocalizedDescriptionKey];
        [dict setValue:@"There was an error building up the data file." forKey:NSLocalizedFailureReasonErrorKey];
        NSError *error = [NSError errorWithDomain:@"YOUR_ERROR_DOMAIN" code:9999 userInfo:dict];
        [NSApp presentError:error];
        exit(0);
    }
    privateManagedObjectContext = [[NSManagedObjectContext alloc] initWithConcurrencyType:NSPrivateQueueConcurrencyType];
    privateManagedObjectContext.persistentStoreCoordinator = coordinator;

    return privateManagedObjectContext;
}

- (NSManagedObjectContext *)mainManagedObjectContext
{
    if (_mainManagedObjectContext) {
        return _mainManagedObjectContext;
    }

    _mainManagedObjectContext = self.persistentContainer.viewContext;

    return _mainManagedObjectContext;
}

// Returns the directory the application uses to store the Core Data store file. This code uses a directory named "Spatterlight" in the user's Application Support directory.
// In a sandboxed app, the system will always return the path to the Application Support directory
// in ~/Library/Group Containers, so we use this hack to get the one we want instead.

- (NSURL *)applicationFilesDirectory
{
    NSString *homeString = NSHomeDirectory();
    NSArray *pathComponents = homeString.pathComponents;
    pathComponents = [pathComponents subarrayWithRange:NSMakeRange(0, 3)];
    homeString = [[NSString pathWithComponents:pathComponents] stringByAppendingString:@"/Library/Application Support/Spatterlight/"];
    return [NSURL fileURLWithPath:homeString isDirectory:YES];
}

- (void)saveChanges {
//    NSLog(@"CoreDataManagar saveChanges");
    NSManagedObjectContext *mainContext = _mainManagedObjectContext;

    [mainContext performBlock:^{
        NSError *error = nil;
        if (mainContext.hasChanges) {
            BOOL result = NO;
            @try {
                result = [mainContext save:&error];
            } @catch (NSException *exception) {
                NSLog(@"Exception while saving managed object context: %@", exception);
            } @finally {
                if (!result || error)
                    NSLog(@"Error while saving context: %@", error);
            }
        }
    }];
}

- (NSManagedObjectContext *)privateChildManagedObjectContext {
    NSManagedObjectContext *managedObjectContext =
    [[NSManagedObjectContext alloc] initWithConcurrencyType:NSPrivateQueueConcurrencyType];
    managedObjectContext.parentContext = self.mainManagedObjectContext;

    return managedObjectContext;
}

- (void)startIndexing:(NSNotification *)notification {
    if (@available(macOS 10.15, *)) {
        if (!_spotlightDelegate)
            return;
#if __MAC_OS_X_VERSION_MAX_ALLOWED > 110300
        [_spotlightDelegate startSpotlightIndexing];
#endif
    }
}

- (void)stopIndexing:(NSNotification *)notification {
    if (@available(macOS 10.15, *)) {
        if (!_spotlightDelegate)
            return;
#if __MAC_OS_X_VERSION_MAX_ALLOWED > 110300
        [_spotlightDelegate stopSpotlightIndexing];
#endif
    }
}

@end
