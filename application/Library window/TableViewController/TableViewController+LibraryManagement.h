//
//  TableViewController+LibraryManagement.h
//  Spatterlight
//
//  Library-level operations (delete, prune, verify).
//

#import "TableViewController.h"

NS_ASSUME_NONNULL_BEGIN

@interface TableViewController (LibraryManagement)


- (IBAction)deleteLibrary:(id)sender;
- (IBAction)pruneLibrary:(id)sender;
- (IBAction)verifyLibrary:(id)sender;

- (void)verifyInBackground:(nullable id)sender;
- (void)startVerifyTimer;

- (void)migrateConvertedAGTGames;
- (void)migrateOutdatedIfids;
- (void)stopVerifyTimer;

- (void)lookForMissingFile:(Game *)game;
//! The hashTags of the games being played, which the methods below never
//! delete. Main thread only: snapshot it before going to a child context.
@property (readonly) NSSet<NSString *> *runningHashTags;

- (void)deleteGameMetaAndIfid:(Game *)game inContext:(NSManagedObjectContext *)context sparing:(NSSet<NSString *> *)runningHashTags;
- (void)deleteIfDuplicate:(NSString *)hashTag inContext:(NSManagedObjectContext *)context sparing:(NSSet<NSString *> *)runningHashTags;
- (void)pruneDuplicateGamesInContext:(NSManagedObjectContext *)context sparing:(NSSet<NSString *> *)runningHashTags;

@end
NS_ASSUME_NONNULL_END

