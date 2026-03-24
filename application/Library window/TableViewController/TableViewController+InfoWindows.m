//
//  TableViewController+InfoWindows.m
//  Spatterlight
//
//  Info window management.
//

#import "TableViewController+InfoWindows.h"
#import "TableViewController+Internal.h"

#import "LibHelperTableView.h"
#import "AppDelegate.h"

#import "NSString+Categories.h"

#import "Game.h"

#import "InfoController.h"

@implementation TableViewController (InfoWindows)

#pragma mark Game info windows

- (IBAction)showGameInfo:(id)sender {
    NSIndexSet *rows = self.gameTableView.selectedRowIndexes;

    // If we clicked outside selected rows, only show info for clicked row
    if (self.gameTableView.clickedRow != -1 &&
        ![rows containsIndex:(NSUInteger)self.gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)self.gameTableView.clickedRow];

    NSUInteger i;
    NSUInteger counter = 0;
    for (i = rows.firstIndex; i != NSNotFound;
         i = [rows indexGreaterThanIndex:i]) {
        Game *game = self.gameTableModel[i];
        [self showInfoForGame:game toggle:![sender isKindOfClass:[NSMenuItem class]]];
        // Don't open more than 20 info windows at once
        if (counter++ > 20)
            break;
    }
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

- (void)closeAndOpenNextAbove:(InfoController *)infocontroller {
    NSUInteger index = [self.gameTableModel indexOfObject:infocontroller.game];
    if (index != NSNotFound && index > 0) {
        [self.gameTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:index - 1] byExtendingSelection:NO];
        [infocontroller.window performClose:nil];
        [self showInfoForGame:self.gameTableModel[index - 1] toggle:NO];
        [self.gameTableView scrollRowToVisible:(NSInteger)index - 1];
    }
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void) {
        [InfoController closeStrayInfoWindows];
    });
}

- (void)closeAndOpenNextBelow:(InfoController *)infocontroller {
    NSUInteger index = [self.gameTableModel indexOfObject:infocontroller.game];
    if (index != NSNotFound && index < self.gameTableModel.count - 1) {
        [self.gameTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:index + 1] byExtendingSelection:NO];
        [infocontroller.window performClose:nil];
        [self showInfoForGame:self.gameTableModel[index + 1] toggle:NO];
        [self.gameTableView scrollRowToVisible:(NSInteger)index + 1];
    }
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void) {
        [InfoController closeStrayInfoWindows];
    });
}

- (void)releaseInfoController:(InfoController *)infoctl {
    NSString *key = nil;
    for (InfoController *controller in self.infoWindows.allValues)
        if (controller == infoctl) {
            NSArray *temp = [self.infoWindows allKeysForObject:controller];
            key = temp[0];
            break;
        }
    if (key) {
        infoctl.window.delegate = nil;
        [self.infoWindows removeObjectForKey:key];
    }
}

@end
