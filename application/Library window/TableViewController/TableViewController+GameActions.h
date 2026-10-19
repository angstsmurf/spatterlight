//
//  TableViewController+GameActions.h
//  Spatterlight
//
//  Contextual menu actions and validation.
//

#import "TableViewController.h"

@class Game;

@interface TableViewController (GameActions)

- (IBAction)play:(id)sender;
- (IBAction)revealGameInFinder:(id)sender;
- (IBAction)deleteGame:(id)sender;
- (IBAction)openIfdb:(id)sender;
- (IBAction)download:(id)sender;
- (IBAction)applyTheme:(id)sender;
- (IBAction)selectSameTheme:(id)sender;
- (IBAction)deleteSaves:(id)sender;
- (IBAction)like:(id)sender;
- (IBAction)dislike:(id)sender;
- (IBAction)cancel:(id)sender;

- (void)toggleLike:(int32_t)value forGames:(NSArray<Game *> *)games;

- (void)rebuildThemesSubmenu;

@end
