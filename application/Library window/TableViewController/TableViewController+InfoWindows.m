//
//  TableViewController+InfoWindows.m
//  Spatterlight
//
//  Info window management.
//

#import "TableViewController+InfoWindows.h"
#import "TableViewController+Internal.h"
#import "TableViewController+TableDelegate.h"

#import "LibHelperTableView.h"
#import "AppDelegate.h"

#import "NSString+Categories.h"

#import "Game.h"

#import "InfoController.h"

@implementation TableViewController (InfoWindows)

#pragma mark Game info windows

- (IBAction)showGameInfo:(id)sender {
    NSArray<Game *> *games = self.targetGames;
    if (games.count > kMaxInfoWindowsAtOnce)
        games = [games subarrayWithRange:NSMakeRange(0, kMaxInfoWindowsAtOnce)];
    for (Game *game in games)
        [self showInfoForGame:game toggle:![sender isKindOfClass:[NSMenuItem class]]];
}

- (void)showInfoForGame:(Game *)game toggle:(BOOL)toggle {
    InfoController *infoctl;

    NSString *hashTag = game.hashTag;
    if (hashTag.length == 0) {
        hashTag = game.path.signatureFromFile;
        game.hashTag = hashTag;
    }
    if (hashTag.length == 0) {
        NSLog(@"showInfoForGame: Could not create hash from game file!");
        return;
    }
    // First, we check if we have created this info window already
    infoctl = self.infoWindows[hashTag];

    if (!infoctl || infoctl.inDeletion) {
        infoctl = [[InfoController alloc] initWithGame:game];
        infoctl.libcontroller = self;
        self.infoWindows[hashTag] = infoctl;
        NSWindow *infoWindow = infoctl.window;
        infoWindow.restorable = YES;
        infoWindow.restorationClass = [AppDelegate class];
        infoWindow.identifier = [NSString stringWithFormat:@"infoWin%@", hashTag];

        NSRect targetFrame = infoctl.window.frame;
        NSRect visibleFrame = infoctl.window.screen.visibleFrame;
        if (targetFrame.origin.y < visibleFrame.origin.y)
            targetFrame.origin.y = visibleFrame.origin.y;

        [infoctl hideWindow];

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [infoctl animateIn:targetFrame];
        });
    } else if (toggle) {
        if (!infoctl.inAnimation)
            [infoctl.window performClose:nil];
    } else {
        [infoctl showWindow:nil];
    }
}

//! Closes the info window and opens one for the game the given number of
//! rows away (-1 above, 1 below), selecting its row.
- (void)closeInfoWindow:(InfoController *)infocontroller andOpenNextWithOffset:(NSInteger)offset {
    NSUInteger index = [self.gameTableModel indexOfObject:infocontroller.game];
    if (index != NSNotFound) {
        NSInteger next = (NSInteger)index + offset;
        Game *game = [self gameForRow:next];
        if (game) {
            [self.gameTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)next] byExtendingSelection:NO];
            [infocontroller.window performClose:nil];
            [self showInfoForGame:game toggle:NO];
            [self.gameTableView scrollRowToVisible:next];
        }
    }
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void) {
        [InfoController closeStrayInfoWindows];
    });
}

- (void)closeAndOpenNextAbove:(InfoController *)infocontroller {
    [self closeInfoWindow:infocontroller andOpenNextWithOffset:-1];
}

- (void)closeAndOpenNextBelow:(InfoController *)infocontroller {
    [self closeInfoWindow:infocontroller andOpenNextWithOffset:1];
}

- (void)releaseInfoController:(InfoController *)infoctl {
    NSString *key = [self.infoWindows allKeysForObject:infoctl].firstObject;
    if (key) {
        infoctl.window.delegate = nil;
        [self.infoWindows removeObjectForKey:key];
    }
}

@end
