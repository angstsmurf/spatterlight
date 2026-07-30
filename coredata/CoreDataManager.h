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
