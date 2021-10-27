//
//  NSManagedObjectContext+safeSave.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2020-10-27.
//
//

#import <CoreData/CoreData.h>


@interface NSManagedObjectContext (safeSave)

- (void)safeSave;
- (void)safeSaveAndWait;

@end
