//
//  CoreDataManager.m
//  Spatterlight
//
//  https://cocoacasts.com/bring-your-own
//
//

#import "CoreDataManager.h"
#import "MyCoreDataCoreSpotlightDelegate.h"

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

// Returns the persistent store coordinator for the application. This implementation creates and return a coordinator, having added the store for the application to it. (The directory for the store is created, if necessary.)
- (NSPersistentStoreCoordinator *)persistentStoreCoordinator
{
    if (_persistentStoreCoordinator) {
        return _persistentStoreCoordinator;
    }

    //    BOOL needDeleteOld  = false;

    NSManagedObjectModel *mom = [self managedObjectModel];
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

    NSPersistentStore *store = [coordinator addPersistentStoreWithType:NSSQLiteStoreType configuration:nil URL:targetURL options:options error:&error];

    if (!store) {
        [[NSApplication sharedApplication] presentError:error];
        return nil;
    }

    // do the migrate job from local store to a group store.
    if (needMigrate) {
        error = nil;
        NSManagedObjectContext *context = [[NSManagedObjectContext alloc] initWithConcurrencyType:NSMainQueueConcurrencyType];
        [context setPersistentStoreCoordinator:coordinator];
        [coordinator migratePersistentStore:store toURL:groupURL options:options withType:NSSQLiteStoreType error:&error];
        if (error != nil) {
            NSLog(@"Error during Core Data migration to group folder: %@, %@", error, [error userInfo]);
            abort();
        }
    }

    _persistentStoreCoordinator = coordinator;

    return _persistentStoreCoordinator;
}

- (NSURL *)storeURLNeedMigrate:(BOOL *)needMigrate groupURL:(NSURL * __autoreleasing *)groupURL{

    NSURL *oldURL = [[self applicationFilesDirectory] URLByAppendingPathComponent:@"Spatterlight.storedata"];

    NSString *groupIdentifier =
    [[NSBundle mainBundle] objectForInfoDictionaryKey:@"GroupIdentifier"];

    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *applicationFilesDirectory = [fileManager containerURLForSecurityApplicationGroupIdentifier:groupIdentifier];

    NSError *error = nil;

    NSDictionary *properties = [applicationFilesDirectory resourceValuesForKeys:@[NSURLIsDirectoryKey] error:&error];

    if (!properties) {
        BOOL ok = NO;
        if ([error code] == NSFileReadNoSuchFileError) {
            ok = [fileManager createDirectoryAtPath:[applicationFilesDirectory path] withIntermediateDirectories:YES attributes:nil error:&error];
        }
        if (!ok) {
            [[NSApplication sharedApplication] presentError:error];
            NSLog(@"Error: %@", error);
            return nil;
        }
    } else {
        if (![properties[NSURLIsDirectoryKey] boolValue]) {
            // Customize and localize this error.
            NSString *failureDescription = [NSString stringWithFormat:@"Expected a folder to store application data, found a file (%@).", [applicationFilesDirectory path]];

            NSMutableDictionary *dict = [NSMutableDictionary dictionary];
            [dict setValue:failureDescription forKey:NSLocalizedDescriptionKey];
            error = [NSError errorWithDomain:@"YOUR_ERROR_DOMAIN" code:101 userInfo:dict];

            [[NSApplication sharedApplication] presentError:error];
            return nil;
        }
    }

    *groupURL = [fileManager containerURLForSecurityApplicationGroupIdentifier:groupIdentifier];
    *groupURL = [*groupURL URLByAppendingPathComponent:@"Spatterlight.storedata"];

    NSURL *targetURL = oldURL;

//    if ([fileManager fileExistsAtPath:[groupURL path]]) {
//        needMigrate = NO;
//        //        if ([fileManager fileExistsAtPath:[oldURL path]]) {
//        //            needDeleteOld = YES;
//        //        }
//    } else if ([fileManager fileExistsAtPath:[oldURL path]]) {
//        targetURL = oldURL;
//        needMigrate = YES;
//    }

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

    _persistentContainer = [[NSPersistentContainer alloc] initWithName:modelName];

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
        }
        if (blockContainer.viewContext.undoManager == nil) {
            NSUndoManager *newManager = [[NSUndoManager alloc] init];
            [newManager setLevelsOfUndo:10];
            blockContainer.viewContext.undoManager = newManager;
        }

        blockContainer.viewContext.mergePolicy = NSMergeByPropertyObjectTrumpMergePolicy;
        blockContainer.viewContext.automaticallyMergesChangesFromParent = YES;

        // do the migrate job from local store to a group store.
        if (needMigrate) {
            error = nil;
            NSPersistentStoreCoordinator *coordinator = blockContainer.persistentStoreCoordinator;
            NSPersistentStore *store = [coordinator persistentStoreForURL:targetURL];
            [coordinator migratePersistentStore:store toURL:groupURL options:@{ NSMigratePersistentStoresAutomaticallyOption: @(YES), NSInferMappingModelAutomaticallyOption: @(YES)} withType:NSSQLiteStoreType error:&error];
            if (error != nil) {
                NSLog(@"Error during Core Data migration to group folder: %@, %@", error, [error userInfo]);
                abort();
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

    NSPersistentStoreCoordinator *coordinator = [self persistentStoreCoordinator];
    if (!coordinator) {
        NSMutableDictionary *dict = [NSMutableDictionary dictionary];
        [dict setValue:@"Failed to initialize the store" forKey:NSLocalizedDescriptionKey];
        [dict setValue:@"There was an error building up the data file." forKey:NSLocalizedFailureReasonErrorKey];
        NSError *error = [NSError errorWithDomain:@"YOUR_ERROR_DOMAIN" code:9999 userInfo:dict];
        [[NSApplication sharedApplication] presentError:error];
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

    if (@available(macOS 10.13, *)) {
        _mainManagedObjectContext = self.persistentContainer.viewContext;
    } else {
        _mainManagedObjectContext = [[NSManagedObjectContext alloc] initWithConcurrencyType:NSMainQueueConcurrencyType];

        [_mainManagedObjectContext setParentContext:[self privateManagedObjectContext]];

        if (_mainManagedObjectContext.undoManager == nil) {
            NSUndoManager *newManager = [[NSUndoManager alloc] init];
            [newManager setLevelsOfUndo:10];
            _mainManagedObjectContext.undoManager = newManager;
        }
    }

    return _mainManagedObjectContext;
}

// Returns the directory the application uses to store the Core Data store file. This code uses a directory named "Spatterlight" in the user's Application Support directory.
- (NSURL *)applicationFilesDirectory
{
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *appSupportURL = [[fileManager URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask] lastObject];
    return [appSupportURL URLByAppendingPathComponent:@"Spatterlight"];
}

- (void)saveChanges {
//    NSLog(@"CoreDataManagar saveChanges");
    NSManagedObjectContext *mainContext = _mainManagedObjectContext;

    if (@available(macOS 10.13, *)) {
        [mainContext performBlock:^{
            NSError *error = nil;
            if (mainContext.hasChanges) {
                [mainContext save:&error];
                if (error) {
                    NSLog(@"CoreDataManager saveMainContext error: %@", error);
                }
            }
        }];
    } else {
        [mainContext performBlockAndWait:^{
            NSError *error = nil;
            if (mainContext.hasChanges) {
                if (![mainContext save:&error]) {
                    NSLog(@"Unable to Save Changes of Main Managed Object Context! Error: %@", error);
                    if (error) {
                        [[NSApplication sharedApplication] presentError:error];
                    }
                } //else NSLog(@"Changes in _mainManagedObjectContext were saved");
                
            } //else NSLog(@"No changes to save in _mainManagedObjectContext");
            
        }];

        NSManagedObjectContext *privateContext = privateManagedObjectContext;

        [privateContext performBlock:^{
            BOOL result = NO;
            NSError *error = nil;
            if (privateContext.hasChanges) {
                @try {
                    result = [privateContext save:&error];
                    if (error)
                        NSLog(@"Error: %@", error);
                }
                @catch (NSException *ex) {
                    // Ususally because we have deleted the core data files
                    // while the program is running
                    NSLog(@"Unable to save changes in Private Managed Object Context!");
                    return;
                }
                
                if (!result) {
                    NSLog(@"Unable to Save Changes of Private Managed Object Context! Error:%@", error);
                }
                
            } //else NSLog(@"No changes to save in privateManagedObjectContext");
        }];
    }
}

- (NSManagedObjectContext *)privateChildManagedObjectContext {
    NSManagedObjectContext *managedObjectContext =
    [[NSManagedObjectContext alloc] initWithConcurrencyType:NSPrivateQueueConcurrencyType];
    managedObjectContext.parentContext = self.mainManagedObjectContext;

    return managedObjectContext;
}

- (void)startIndexing {
    if (@available(macOS 10.15, *)) {
        if (!_spotlightDelegate)
            return;

#if __MAC_OS_X_VERSION_MAX_ALLOWED > 110300
        [_spotlightDelegate startSpotlightIndexing];
#endif
    }
}

- (void)stopIndexing {
    if (@available(macOS 10.15, *)) {
        if (!_spotlightDelegate)
            return;
#if __MAC_OS_X_VERSION_MAX_ALLOWED > 110300
        [_spotlightDelegate stopSpotlightIndexing];
#endif
    }
}

@end
