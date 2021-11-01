//
//  NSManagedObject+safeSave.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2020-10-27.
//
//

#import "NSManagedObjectContext+safeSave.h"

@implementation NSManagedObjectContext (safeSave)

- (void)safeSave {
    [self saveAndWait:NO];
}

- (void)safeSaveAndWait {
    [self saveAndWait:YES];
}

- (void)saveAndWait:(BOOL)wait {
    void (^saveBlock)(void) = ^void() {
        if (self.hasChanges) {
            NSError *error = nil;
            BOOL result = NO;
            @try {
                result = [self save:&error];
            } @catch (NSException *exception) {
                NSLog(@"Exception while saving managed object context");
            } @finally {
                if (!result || error)
                    NSLog(@"Error while saving context: %@", error);
            }
        }
    };

    if (wait)
        [self performBlockAndWait:saveBlock];
    else
        [self performBlock:saveBlock];
}

@end
