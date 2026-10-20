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
- (void)selectGamesWithHashes:(NSArray *)hashes scroll:(BOOL)shouldscroll clearSearchIfHidden:(BOOL)clearSearch;
- (NSRect)rectForLineWithHash:(NSString *)hashTag;

- (void)updateWindowSubtitle;

//! The game in a row of the table, or nil when there is no such row
- (nullable Game *)gameForRow:(NSInteger)row;
- (nullable Game *)gameForView:(NSView *)view;
//! The rows a menu action applies to: the clicked row, if the user
//! right-clicked outside the selection, otherwise the selected rows.
@property (readonly) NSIndexSet *targetRowIndexes;
@property (readonly) NSArray<Game *> *targetGames;

- (nullable NSString *)imageNameForGame:(Game *)game description:(NSString * _Nullable __autoreleasing * _Nullable)description;

- (void)noteManagedObjectContextDidChange:(NSNotification *)notification;

@end

NS_ASSUME_NONNULL_END
