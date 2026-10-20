//
//  CoreDataManager.m
//  Spatterlight
//
//  https://cocoacasts.com/bring-your-own
//
//

#import "CoreDataManager.h"
#import "MyCoreDataCoreSpotlightDelegate.h"
#import "ProgressiveMigrator.h"
#import "StoreLocation.h"
#import "AppDelegate.h"
#import "Game.h"
#import "Metadata.h"

@interface CoreDataManager () {
    NSString *modelName;
    BOOL inMemory;
}

@property (nonatomic, strong) NSManagedObjectModel *managedObjectModel;
@property (nonatomic, strong) NSPersistentContainer *persistentContainer;
@property (nonatomic, strong) NSManagedObjectContext *mainManagedObjectContext;
@property (nonatomic, strong) MyCoreDataCoreSpotlightDelegate *spotlightDelegate;

@end

NSNotificationName const SpatterlightStartIndexingNotification = @"StartIndexing";
NSNotificationName const SpatterlightStopIndexingNotification = @"StopIndexing";

@implementation CoreDataManager

- (instancetype)initWithModelName:(NSString *)aModelName {
    self = [super init];
    if (self) {
        modelName = aModelName;
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(startIndexing:)
                                                     name:SpatterlightStartIndexingNotification
                                                   object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(stopIndexing:)
                                                     name:SpatterlightStopIndexingNotification
                                                   object:nil];
    }
    return self;
}

- (instancetype)initInMemoryWithModelName:(NSString *)aModelName {
    self = [self initWithModelName:aModelName];
    if (self)
        inMemory = YES;
    return self;
}

// The process's single copy of each managed object model. Two stacks built on
// separate copies of the same model (the app's library and a test's store, say)
// each claim the same NSManagedObject subclasses, and an object from one can
// then land in a relationship of the other ("desired type = Image; given type
// = Image").
+ (nullable NSManagedObjectModel *)sharedModelNamed:(NSString *)name {
    static NSMutableDictionary<NSString *, NSManagedObjectModel *> *models;
    @synchronized (self) {
        if (!models)
            models = [NSMutableDictionary new];
        NSManagedObjectModel *model = models[name];
        if (!model) {
            NSURL *modelURL = [[NSBundle mainBundle] URLForResource:name withExtension:@"momd"];
            if (!modelURL) {
                NSLog(@"Unable to Find Data Model!");
                return nil;
            }
            model = [[NSManagedObjectModel alloc] initWithContentsOfURL:modelURL];
            if (!model) {
                NSLog(@"Unable to Load Data Model!");
                return nil;
            }
            models[name] = model;
        }
        return model;
    }
}

// Creates if necessary and returns the managed object model for the application.
- (NSManagedObjectModel *)managedObjectModel
{
    if (!_managedObjectModel)
        _managedObjectModel = [CoreDataManager sharedModelNamed:modelName];
    return _managedObjectModel;
}

+ (nullable NSURL *)testStoreURL {
    static NSURL *url = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        NSDictionary<NSString *, NSString *> *environment = NSProcessInfo.processInfo.environment;
        NSString *name = environment[@"SPATTERLIGHT_TEST_STORE"];
        if (!name.length)
            return;
        NSFileManager *fileManager = [NSFileManager defaultManager];
        NSURL *folder = [[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES]
                         URLByAppendingPathComponent:@"Spatterlight test stores" isDirectory:YES];
        [fileManager createDirectoryAtURL:folder withIntermediateDirectories:YES attributes:nil error:NULL];
        url = [folder URLByAppendingPathComponent:[name stringByAppendingPathExtension:@"storedata"] isDirectory:NO];
        if ([environment[@"SPATTERLIGHT_TEST_STORE_FRESH"] isEqualToString:@"1"]) {
            for (NSString *suffix in @[@"", @"-shm", @"-wal"])
                [fileManager removeItemAtPath:[url.path stringByAppendingString:suffix] error:NULL];
        }
    });
    return url;
}

- (NSPersistentContainer *)persistentContainer
{
    if (_persistentContainer) {
        return _persistentContainer;
    }

    if (inMemory)
        return [self inMemoryPersistentContainer];

    StoreLocation *location = [StoreLocation currentLocation];
    if (!location)
        return nil;
    NSURL *targetURL = location.storeURL;

    _persistentContainer = [[NSPersistentContainer alloc] initWithName:modelName
                                                     managedObjectModel:self.managedObjectModel];

    // Chain through intermediate model versions before the container opens the
    // store, so custom mapping models (e.g. the IfidToHashMigrationPolicy that
    // runs at "Spatterlight 15") get a chance to run. Best effort: on failure we
    // fall through to the container's own automatic/inferred migration below.
    NSError *migrationError = nil;
    if (![ProgressiveMigrator migrateStoreAtURL:targetURL
                                         ofType:NSSQLiteStoreType
                                        toModel:_persistentContainer.managedObjectModel
                                      modelName:modelName
                                          error:&migrationError]) {
        NSLog(@"Progressive migration of %@ did not complete: %@. Falling back to automatic migration.", targetURL.path, migrationError);
    }

    NSPersistentStoreDescription *description = [[NSPersistentStoreDescription alloc] initWithURL:targetURL];

    description.shouldMigrateStoreAutomatically = YES;
    description.shouldInferMappingModelAutomatically = YES;

    // Under XCTest the app is only a host. Its Spotlight exporter cannot reach
    // the indexing helper there and floods the log with CDCS errors, and tests
    // have no business re-indexing the user's library anyway. Nor do the UI
    // tests, which run the app on a test store.
    BOOL runningTests = NSProcessInfo.processInfo.environment[@"XCTestConfigurationFilePath"] != nil
    || [CoreDataManager testStoreURL] != nil;

    if (@available(macOS 10.15, *)) {
        // History tracking stays on even under tests: the store has it, and
        // opening it without would make Core Data open it read-only.
        [description setOption:@YES forKey:NSPersistentHistoryTrackingKey];
        if (!runningTests) {
            _spotlightDelegate = [MyCoreDataCoreSpotlightDelegate exporterForStoreWithDescription:description
                                                                                     coordinator:_persistentContainer.persistentStoreCoordinator];
            [description setOption:_spotlightDelegate forKey:NSCoreDataCoreSpotlightExporter];
        }
        [description setOption:@YES forKey:NSPersistentStoreRemoteChangeNotificationPostOptionKey];
    }

    _persistentContainer.persistentStoreDescriptions = @[ description ];

    NSPersistentContainer *blockContainer = _persistentContainer;
    __block NSError *loadError = nil;

    // The store is added synchronously (the description's default), so the
    // handler has run by the time this returns.
    [blockContainer loadPersistentStoresWithCompletionHandler:^(NSPersistentStoreDescription *aDescription, NSError *error) {

        if (error != nil) {
            loadError = error;
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
        if (location.needsMove) {
            NSURL *groupURL = location.destinationURL;
            error = nil;
            NSPersistentStoreCoordinator *coordinator = blockContainer.persistentStoreCoordinator;
            NSPersistentStore *store = [coordinator persistentStoreForURL:targetURL];
            [coordinator migratePersistentStore:store toURL:groupURL options:@{ NSMigratePersistentStoresAutomaticallyOption: @(YES), NSInferMappingModelAutomaticallyOption: @(YES)} withType:NSSQLiteStoreType error:&error];
            // We could delete the old store here, but it doesn't really hurt to keep it,
            // and it helps to have a non-sandboxed store around for development purposes.
            // On failure the old store is still the one loaded, so keep using
            // it and try the move again at next launch.
            if (error != nil) {
                NSLog(@"Error during Core Data migration to group folder: %@, %@. Staying on the store at %@.", error, error.userInfo, targetURL.path);
            } else {
                NSLog(@"Successfully migrated store at %@ to %@", targetURL.absoluteString, groupURL.absoluteString);
            }
        }
    }];

    // Without a store the app has nothing to show. Say why once; the caller
    // (AppDelegate) quits on a nil container.
    if (loadError) {
        NSLog(@"Failed to load Core Data stack: %@", loadError);
        [NSApp presentError:loadError];
        _persistentContainer = nil;
        _spotlightDelegate = nil;
    }

    return _persistentContainer;
}

// The in-memory variant: nothing on disk, so no migration, folder access,
// persistent history or Spotlight exporter. The store loads synchronously.
- (NSPersistentContainer *)inMemoryPersistentContainer {
    NSPersistentContainer *container = [[NSPersistentContainer alloc] initWithName:modelName
                                                                  managedObjectModel:self.managedObjectModel];

    NSPersistentStoreDescription *description = [NSPersistentStoreDescription new];
    description.type = NSInMemoryStoreType;
    description.shouldAddStoreAsynchronously = NO;
    container.persistentStoreDescriptions = @[ description ];

    __block NSError *loadError = nil;
    [container loadPersistentStoresWithCompletionHandler:^(NSPersistentStoreDescription *aDescription, NSError *error) {
        loadError = error;
    }];
    if (loadError) {
        NSLog(@"Failed to load in-memory Core Data stack: %@", loadError);
        return nil;
    }

    NSUndoManager *undoManager = [[NSUndoManager alloc] init];
    undoManager.levelsOfUndo = 10;
    container.viewContext.undoManager = undoManager;
    container.viewContext.mergePolicy = NSMergeByPropertyObjectTrumpMergePolicy;
    container.viewContext.automaticallyMergesChangesFromParent = YES;

    _persistentContainer = container;
    return _persistentContainer;
}

- (NSManagedObjectContext *)mainManagedObjectContext
{
    if (_mainManagedObjectContext) {
        return _mainManagedObjectContext;
    }

    _mainManagedObjectContext = self.persistentContainer.viewContext;

    return _mainManagedObjectContext;
}

// One-time repair for stores migrated by an early version of
// IfidToHashMigrationPolicy, which could leave a game wearing another
// same-ifid game's filename as its title (e.g. an Apple II .woz disk image
// titled "claymorge.z80" after its Spectrum sibling). We detect a metadata
// whose title is *exactly the filename of a different game* and reset it to
// the game's own filename.
- (void)repairCrossedFilenameTitles {
    NSManagedObjectContext *context = self.mainManagedObjectContext;
    if (!context)
        return;

    [context performBlockAndWait:^{
        NSUInteger repaired = [CoreDataManager repairCrossedFilenameTitlesInContext:context];
        if (repaired) {
            NSLog(@"repairCrossedFilenameTitles: fixed %lu crossed title(s).", (unsigned long)repaired);
            NSError *saveError = nil;
            if (![context save:&saveError])
                NSLog(@"repairCrossedFilenameTitles: save failed: %@", saveError);
        }
    }];
}

+ (NSUInteger)repairCrossedFilenameTitlesInContext:(NSManagedObjectContext *)context {
    NSFetchRequest *request = [NSFetchRequest fetchRequestWithEntityName:@"Game"];
    NSError *error = nil;
    NSArray<Game *> *games = [context executeFetchRequest:request error:&error];
    if (!games.count)
        return 0;

    // filename (last path component) -> the games that actually own it
    NSMutableDictionary<NSString *, NSMutableArray<Game *> *> *gamesByFilename = [NSMutableDictionary new];
    for (Game *game in games) {
        NSString *filename = game.path.lastPathComponent;
        if (!filename.length)
            continue;
        NSMutableArray<Game *> *owners = gamesByFilename[filename];
        if (!owners) {
            owners = [NSMutableArray new];
            gamesByFilename[filename] = owners;
        }
        [owners addObject:game];
    }

    NSUInteger repaired = 0;
    for (Game *game in games) {
        NSString *title = game.metadata.title;
        if (!title.length)
            continue;
        // If this game's title is exactly some game's filename, and this game
        // is not among the owners of that filename, the title was crossed
        // during migration. Reset it to this game's own filename.
        NSArray<Game *> *owners = gamesByFilename[title];
        if (owners && ![owners containsObject:game]) {
            NSString *ownFilename = game.path.lastPathComponent;
            game.metadata.title = ownFilename.length ? ownFilename : @"Untitled";
            repaired++;
        }
    }
    return repaired;
}

- (void)saveChanges {
    NSManagedObjectContext *mainContext = _mainManagedObjectContext;

    [mainContext performBlock:^{
        NSError *error = nil;
        if (mainContext.hasChanges) {
            @try {
                if (![mainContext save:&error])
                    NSLog(@"Error while saving context: %@", error);
            } @catch (NSException *exception) {
                NSLog(@"Exception while saving managed object context: %@", exception);
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
    if (@available(macOS 10.15, *))
        [_spotlightDelegate startIndexing];
}

- (void)stopIndexing:(NSNotification *)notification {
    if (@available(macOS 10.15, *))
        [_spotlightDelegate stopIndexing];
}

@end
