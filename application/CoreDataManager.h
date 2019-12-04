//
//  CoreDataManager.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-06-27.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>


@interface CoreDataManager : NSObject {
    NSString *modelName;
    NSManagedObjectContext *privateManagedObjectContext;
}


@property (strong, nonatomic) NSManagedObjectModel
*managedObjectModel;
@property (strong, nonatomic) NSPersistentStoreCoordinator
*persistentStoreCoordinator;
@property (strong, nonatomic) NSManagedObjectContext
*mainManagedObjectContext;

- (instancetype)initWithModelName:(NSString *)aModelName;
- (NSManagedObjectContext *)privateChildManagedObjectContext;
- (void)saveChanges;
@end
