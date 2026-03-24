//
//  TableViewController+InfoWindows.h
//  Spatterlight
//
//  Info window management.
//

#import "TableViewController.h"

@class Game;

@interface TableViewController (InfoWindows)

NS_ASSUME_NONNULL_BEGIN

- (IBAction)showGameInfo:(nullable id)sender;
- (void)showInfoForGame:(Game *)game toggle:(BOOL)toggle;

- (void)closeAndOpenNextAbove:(InfoController *)infocontroller;
- (void)closeAndOpenNextBelow:(InfoController *)infocontroller;
- (void)releaseInfoController:(InfoController *)infoctl;

NS_ASSUME_NONNULL_END

@end
