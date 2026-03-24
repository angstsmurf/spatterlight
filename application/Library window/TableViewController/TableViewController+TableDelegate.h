//
//  TableViewController+TableDelegate.h
//  Spatterlight
//
//  Table view data source/delegate, sorting, searching, editing, selection.
//

#import "TableViewController.h"

NS_ASSUME_NONNULL_BEGIN

@class Game;

@interface TableViewController (TableDelegate)

- (IBAction)searchForGames:(nullable id)sender;
- (IBAction)toggleColumn:(id)sender;

- (void)updateTableViews;
- (void)selectGames:(NSSet *)games;
- (void)selectGamesWithHashes:(NSArray *)hashes scroll:(BOOL)shouldscroll;
- (NSRect)rectForLineWithHash:(NSString *)hashTag;

- (nullable NSString *)imageNameForGame:(Game *)game description:(NSString * _Nullable __autoreleasing * _Nullable)description;

- (void)mouseOverChangedFromRow:(NSInteger)lastRow toRow:(NSInteger)currentRow;
- (void)enableClickToRenameAfterDelay;

- (void)noteManagedObjectContextDidChange:(NSNotification *)notification;

NS_ASSUME_NONNULL_END

@end
