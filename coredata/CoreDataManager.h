//
//  CoreDataManager.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-06-27.
//
//

#import <Cocoa/Cocoa.h>

@class MyCoreDataCoreSpotlightDelegate;

@interface CoreDataManager : NSObject

@property (strong, nonatomic) NSManagedObjectModel
*managedObjectModel;
@property (strong, nonatomic) NSPersistentStoreCoordinator
*persistentStoreCoordinator;
@property (strong, nonatomic) NSPersistentContainer
*persistentContainer;
@property (strong, nonatomic) NSManagedObjectContext
*mainManagedObjectContext;

@property MyCoreDataCoreSpotlightDelegate *spotlightDelegate;

- (instancetype)initWithModelName:(NSString *)aModelName;

//! A stack on a private in-memory store, for tests: no on-disk library, no
//! migration, no persistent history or Spotlight indexing. It shares the
//! process's one managed object model with every other CoreDataManager, so
//! entities never resolve against a second copy of the model.
- (instancetype)initInMemoryWithModelName:(NSString *)aModelName;

//! The store of the test library named by the SPATTERLIGHT_TEST_STORE
//! environment variable, in the temporary folder, or nil. The UI tests launch
//! the app with it, so that they work on a library of their own instead of the
//! user's, and never touch the Spotlight index. SPATTERLIGHT_TEST_STORE_FRESH=1
//! deletes the store first.
+ (nullable NSURL *)testStoreURL;

@property (NS_NONATOMIC_IOSONLY, readonly, strong) NSManagedObjectContext *privateChildManagedObjectContext;
- (void)saveChanges;

//! One-time repair for stores migrated by an early IfidToHashMigrationPolicy,
//! which could leave a game wearing another same-ifid game's filename as its
//! title (e.g. an Apple II .woz disk image titled "claymorge.z80"). Runs on the
//! main context and saves.
- (void)repairCrossedFilenameTitles;

//! The repair itself, factored out so it can run against any context (used by
//! the above and by tests). Returns the number of titles corrected; does not save.
+ (NSUInteger)repairCrossedFilenameTitlesInContext:(NSManagedObjectContext *)context;

@end
