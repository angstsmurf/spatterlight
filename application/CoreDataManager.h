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

@end
