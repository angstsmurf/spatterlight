//
//  TableViewController+LibraryManagement.h
//  Spatterlight
//
//  Library-level operations (delete, prune, verify).
//

#import "TableViewController.h"

NS_ASSUME_NONNULL_BEGIN

@interface TableViewController (LibraryManagement)

+ (NSString *)ifidFromFile:(NSString *)path;

- (IBAction)deleteLibrary:(id)sender;
- (IBAction)pruneLibrary:(id)sender;
- (IBAction)verifyLibrary:(id)sender;

- (void)verifyInBackground:(nullable id)sender;
- (void)startVerifyTimer;
- (void)stopVerifyTimer;

- (void)lookForMissingFile:(Game *)game;
- (void)deleteGameMetaAndIfid:(Game *)game inContext:(NSManagedObjectContext *)context;
- (void)deleteIfDuplicate:(NSString *)hashTag inContext:(NSManagedObjectContext *)context;

@end
NS_ASSUME_NONNULL_END

