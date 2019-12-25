//
//  CoreDataManager.m
//  Spatterlight
//
//  https://cocoacasts.com/bring-your-own
//
//

#import "CoreDataManager.h"

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

    NSManagedObjectModel *mom = [self managedObjectModel];
    if (!mom) {
        NSLog(@"%@:%@ No model to generate a store from", [self class], NSStringFromSelector(_cmd));
        return nil;
    }

    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *applicationFilesDirectory = [self applicationFilesDirectory];
    NSError *error = nil;

    NSDictionary *properties = [applicationFilesDirectory resourceValuesForKeys:@[NSURLIsDirectoryKey] error:&error];

    if (!properties) {
        BOOL ok = NO;
        if ([error code] == NSFileReadNoSuchFileError) {
            ok = [fileManager createDirectoryAtPath:[applicationFilesDirectory path] withIntermediateDirectories:YES attributes:nil error:&error];
        }
        if (!ok) {
            [[NSApplication sharedApplication] presentError:error];
            return nil;
        }
    } else {
        if (![[properties objectForKey:NSURLIsDirectoryKey] boolValue]) {
            // Customize and localize this error.
            NSString *failureDescription = [NSString stringWithFormat:@"Expected a folder to store application data, found a file (%@).", [applicationFilesDirectory path]];

            NSMutableDictionary *dict = [NSMutableDictionary dictionary];
            [dict setValue:failureDescription forKey:NSLocalizedDescriptionKey];
            error = [NSError errorWithDomain:@"YOUR_ERROR_DOMAIN" code:101 userInfo:dict];

            [[NSApplication sharedApplication] presentError:error];
            return nil;
        }
    }

    NSString *storeFileName = [modelName stringByAppendingString:@".storedata"];

    NSURL *url = [applicationFilesDirectory URLByAppendingPathComponent:storeFileName];
    NSPersistentStoreCoordinator *coordinator = [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:mom];
    if (![coordinator addPersistentStoreWithType:NSSQLiteStoreType configuration:nil URL:url options:nil error:&error]) {
        [[NSApplication sharedApplication] presentError:error];
        return nil;
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
    [privateManagedObjectContext setPersistentStoreCoordinator:coordinator];

    return privateManagedObjectContext;
}

- (NSManagedObjectContext *)mainManagedObjectContext
{
    if (_mainManagedObjectContext) {
        return _mainManagedObjectContext;
    }
    _mainManagedObjectContext = [[NSManagedObjectContext alloc] initWithConcurrencyType:NSMainQueueConcurrencyType];
    
    [_mainManagedObjectContext setParentContext:[self privateManagedObjectContext]];
    
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
    NSLog(@"CoreDataManagar saveChanges");
    [_mainManagedObjectContext performBlockAndWait:^{
        NSError *error;
        if (_mainManagedObjectContext.hasChanges) {
            if (![_mainManagedObjectContext save:&error]) {
                NSLog(@"Unable to Save Changes of Main Managed Object Context!");
                if (error) {
                    [[NSApplication sharedApplication] presentError:error];
                }
            } else NSLog(@"Changes in _mainManagedObjectContext were saved");

        } else NSLog(@"No changes to save in _mainManagedObjectContext");

    }];

    [privateManagedObjectContext performBlock:^{
        BOOL result;
        NSError *error;
        if (privateManagedObjectContext.hasChanges) {
            @try {
                result = [privateManagedObjectContext save:&error];
            }
            @catch (NSException *ex) {
                // Ususally because we have deleted the core data files
                // while the program is running
                NSLog(@"Unable to save changes in Private Managed Object Context!");
                return;
            }

            if (!result) {
                NSLog(@"Unable to Save Changes of Private Managed Object Context! Error:%@", error);
                if (error) {
                    [[NSApplication sharedApplication] presentError:error];
                } else NSLog(@"Changes in privateManagedObjectContext were saved");
            }

        } else NSLog(@"No changes to save in privateManagedObjectContext");
    }];
}

- (NSManagedObjectContext *)privateChildManagedObjectContext {
    // Initialize Managed Object Context
    NSManagedObjectContext *managedObjectContext = [[NSManagedObjectContext alloc] initWithConcurrencyType:NSPrivateQueueConcurrencyType];
    // Configure Managed Object Context
    [managedObjectContext setParentContext:_mainManagedObjectContext];

    return managedObjectContext;
}

@end
