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
        // No group container Core Data store exists, but one in
        // the non-sandboxed Application Support directory, so we try to migrate it.
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
            BOOL result = [mainContext save:&error];
            if (!result || error) {
                NSLog(@"CoreDataManager saveMainContext error: %@", error);
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
