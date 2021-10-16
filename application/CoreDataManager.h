//
//  CoreDataManager.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-06-27.
//
//

#import <Cocoa/Cocoa.h>


@interface CoreDataManager : NSObject

@property (strong, nonatomic) NSManagedObjectModel
*managedObjectModel;
@property (strong, nonatomic) NSPersistentStoreCoordinator
*persistentStoreCoordinator;
@property (strong, nonatomic) NSPersistentContainer
*persistentContainer;
@property (strong, nonatomic) NSManagedObjectContext
*mainManagedObjectContext;

- (instancetype)initWithModelName:(NSString *)aModelName;
- (NSManagedObjectContext *)privateChildManagedObjectContext;
- (void)saveChanges;
@end
