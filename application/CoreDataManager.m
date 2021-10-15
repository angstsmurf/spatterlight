//
//  CoreDataManager.m
//  Spatterlight
//
//  https://cocoacasts.com/bring-your-own
//
//

#import "CoreDataManager.h"

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

    BOOL needMigrate = false;
    //    BOOL needDeleteOld  = false;

    NSURL *oldURL = [[self applicationFilesDirectory] URLByAppendingPathComponent:@"Spatterlight.storedata"];

    NSManagedObjectModel *mom = [self managedObjectModel];
    if (!mom) {
        NSLog(@"%@:%@ ERROR! No model to generate a store from!", [self class], NSStringFromSelector(_cmd));
        return nil;
    }

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

    NSURL *groupURL = [fileManager containerURLForSecurityApplicationGroupIdentifier:groupIdentifier];
    groupURL = [groupURL URLByAppendingPathComponent:@"Spatterlight.storedata"];

    NSURL *targetURL = groupURL;

    if ([fileManager fileExistsAtPath:[groupURL path]]) {
        needMigrate = NO;
        //        if ([fileManager fileExistsAtPath:[oldURL path]]) {
        //            needDeleteOld = YES;
        //        }
    } else if ([fileManager fileExistsAtPath:[oldURL path]]) {
        targetURL = oldURL;
        needMigrate = YES;
    }

    NSPersistentStoreCoordinator *coordinator = [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:mom];

    NSDictionary *options = @{ NSMigratePersistentStoresAutomaticallyOption: @(YES),
                               NSInferMappingModelAutomaticallyOption: @(YES) };

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
    _mainManagedObjectContext = [[NSManagedObjectContext alloc] initWithConcurrencyType:NSMainQueueConcurrencyType];

    [_mainManagedObjectContext setParentContext:[self privateManagedObjectContext]];

    if (_mainManagedObjectContext.undoManager == nil) {
        NSUndoManager *newManager = [[NSUndoManager alloc] init];
        [newManager setLevelsOfUndo:10];
        _mainManagedObjectContext.undoManager = newManager;
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

    [_mainManagedObjectContext performBlockAndWait:^{
        NSError *error;
        if (_mainManagedObjectContext.hasChanges) {
            if (![_mainManagedObjectContext save:&error]) {
                NSLog(@"Unable to Save Changes of Main Managed Object Context! Error: %@", error);
                if (error) {
                    [[NSApplication sharedApplication] presentError:error];
                }
            } //else NSLog(@"Changes in _mainManagedObjectContext were saved");

        } //else NSLog(@"No changes to save in _mainManagedObjectContext");

    }];

    CoreDataManager * __unsafe_unretained weakSelf = self;

    [privateManagedObjectContext performBlock:^{
        BOOL result = NO;
        NSError *error = nil;
        if (weakSelf->privateManagedObjectContext.hasChanges) {
            @try {
                result = [weakSelf->privateManagedObjectContext save:&error];
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

- (NSManagedObjectContext *)privateChildManagedObjectContext {
    // Initialize Managed Object Context
    NSManagedObjectContext *managedObjectContext = [[NSManagedObjectContext alloc] initWithConcurrencyType:NSPrivateQueueConcurrencyType];
    // Configure Managed Object Context
    managedObjectContext.parentContext = self.mainManagedObjectContext;
    
    return managedObjectContext;
}

@end
