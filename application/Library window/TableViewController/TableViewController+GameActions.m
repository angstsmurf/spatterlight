//
//  TableViewController+GameActions.m
//  Spatterlight
//
//  Contextual menu actions and validation.
//

#import "TableViewController+GameActions.h"
#import "TableViewController+Internal.h"
#import "TableViewController+LibraryManagement.h"
#import "TableViewController+DragDrop.h"
#import "TableViewController+InfoWindows.h"

#import "LibHelperTableView.h"
#import "LibController.h"
#import "GlkController.h"
#import "GlkController+Autorestore.h"
#import "Preferences.h"
#import "Constants.h"

#import "NSString+Categories.h"

#import "Game.h"
#import "Metadata.h"
#import "Theme.h"
#import "Fetches.h"

#import "GameLauncher.h"
#import "MetadataHandler.h"
#import "FolderAccess.h"
#import "MissingFilesFinder.h"

@implementation TableViewController (GameActions)

#pragma mark Contextual menu

- (IBAction)play:(id)sender {
    NSInteger rowidx;

    if (self.gameTableView.clickedRow != -1)
        rowidx = self.gameTableView.clickedRow;
    else
        rowidx = self.gameTableView.selectedRow;

    if (rowidx >= 0) {
        [self.gameLauncher playGame:self.gameTableModel[(NSUInteger)rowidx] restorationHandler:nil];
    }
}

- (IBAction)revealGameInFinder:(id)sender {
    NSIndexSet *rows = self.gameTableView.selectedRowIndexes;

    // If we clicked outside selected rows, only reveal game in clicked row
    if (self.gameTableView.clickedRow != -1 &&
        ![rows containsIndex:(NSUInteger)self.gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)self.gameTableView.clickedRow];

    NSUInteger i;
    for (i = rows.firstIndex; i != NSNotFound;
         i = [rows indexGreaterThanIndex:i]) {
        Game *game = self.gameTableModel[i];
        NSURL *url = game.urlForBookmark;
        if (![[NSFileManager defaultManager] fileExistsAtPath:url.path]) {
            game.found = NO;
            [self lookForMissingFile:game];
            return;
        } else game.found = YES;
        [[NSWorkspace sharedWorkspace] selectFile:url.path
                         inFileViewerRootedAtPath:@""];
    }
}

- (IBAction)deleteGame:(id)sender {
    if (self.currentlyAddingGames) {
        NSBeep();
        return;
    }
    NSIndexSet *rows = self.gameTableView.selectedRowIndexes;

    // If we clicked outside selected rows, only delete game in clicked row
    if (self.gameTableView.clickedRow != -1 &&
        ![rows containsIndex:(NSUInteger)self.gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)self.gameTableView.clickedRow];

    NSMutableArray __block *running = [NSMutableArray new];
    Game __block *game;

    TableViewController * __weak weakSelf = self;

    [rows
     enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        game = weakSelf.gameTableModel[idx];
        if (weakSelf.gameSessions[game.ifid] == nil) {
            [weakSelf.managedObjectContext deleteObject:game];
        } else {
            [running addObject:game];
        }
    }];

    NSUInteger count = running.count;
    if (count) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:NSLocalizedString(@"Are you sure?", nil)];
        alert.informativeText = [NSString stringWithFormat:@"%@ %@ currently in use. Do you want to close and delete %@?", [NSString stringWithSummaryOfGames:running], (count == 1) ? @"is" : @"are", (count == 1) ? @"it" : @"them"];
        [alert addButtonWithTitle:NSLocalizedString(@"Delete", nil)];
        [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

        NSInteger choice = [alert runModal];

        if (choice == NSAlertFirstButtonReturn) {
            for (Game *toDelete in running) {
                [self.gameSessions[toDelete.hashTag].window close];
                [self.managedObjectContext deleteObject:toDelete];
            }
        }
    }
}

- (IBAction)delete:(id)sender {
    [self deleteGame:sender];
}

- (IBAction)openIfdb:(id)sender {

    NSIndexSet *rows = self.gameTableView.selectedRowIndexes;

    if ((self.gameTableView.clickedRow != -1) && ![self.gameTableView isRowSelected:self.gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)self.gameTableView.clickedRow];

    Game __block *game;
    NSString __block *urlString;

    TableViewController * __weak weakSelf = self;

    [rows
     enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        game = weakSelf.gameTableModel[idx];
        if (game.metadata.tuid)
            urlString = [@"https://ifdb.tads.org/viewgame?id=" stringByAppendingString:game.metadata.tuid];
        else
            urlString = [@"https://ifdb.tads.org/viewgame?ifid=" stringByAppendingString:game.ifid];
        [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString: urlString]];
    }];
}

- (IBAction)download:(id)sender {
    NSIndexSet *rows = self.gameTableView.selectedRowIndexes;

    if ((self.gameTableView.clickedRow != -1) && ![self.gameTableView isRowSelected:self.gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)self.gameTableView.clickedRow];

    if (rows.count > 0)
    {
        [self.metadataHandler downloadMetadataForGames:[self.gameTableModel objectsAtIndexes:rows]];
    }
}

- (IBAction)applyTheme:(id)sender {

    NSIndexSet *rows = self.gameTableView.selectedRowIndexes;

    if ((self.gameTableView.clickedRow != -1) && ![self.gameTableView isRowSelected:self.gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)self.gameTableView.clickedRow];

    NSString *name = ((NSMenuItem *)sender).title;

    Theme *theme = [Fetches findTheme:name inContext:self.managedObjectContext];

    if (!theme) {
        NSLog(@"applyTheme: found no theme with name %@", name);
        return;
    }

    NSSet *games = [NSSet setWithArray:[self.gameTableModel objectsAtIndexes:rows]];
    [theme addGames:games];

    Preferences *prefwin = [Preferences instance];
    if (prefwin) {
        // Unless we are changing the theme of all games simultaneously,
        // uncheck the "Changes apply to all games" box.
        if (games.count < self.gameTableModel.count) {
            prefwin.oneThemeForAll = NO;
        }
        if ([games containsObject:prefwin.currentGame])
            [prefwin restoreThemeSelection:theme];
    }

    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"PreferencesChanged" object:theme]];
}

- (IBAction)selectSameTheme:(id)sender {
    NSIndexSet *rows = self.gameTableView.selectedRowIndexes;

    if ((self.gameTableView.clickedRow != -1) && ![self.gameTableView isRowSelected:self.gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)self.gameTableView.clickedRow];

    Game *selectedGame = self.gameTableModel[rows.firstIndex];

    NSSet *gamesWithTheme = selectedGame.theme.games;

    NSIndexSet *matchingIndexes = [self.gameTableModel indexesOfObjectsPassingTest:^BOOL(Game *obj, NSUInteger idx, BOOL *stop) {
        return [gamesWithTheme containsObject:obj];
    }];

    [self.gameTableView selectRowIndexes:matchingIndexes byExtendingSelection:NO];
}

- (IBAction)deleteSaves:(id)sender {
    NSIndexSet *rows = self.gameTableView.selectedRowIndexes;
    TableViewController * __weak weakSelf = self;

    // If we clicked outside selected rows, only show info for clicked row
    if (self.gameTableView.clickedRow != -1 &&
        ![rows containsIndex:(NSUInteger)self.gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)self.gameTableView.clickedRow];

    [rows
     enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        Game *game = weakSelf.gameTableModel[idx];
        GlkController *gctl = weakSelf.gameSessions[game.ifid];

        if (!gctl) {
            gctl = [[GlkController alloc] init];
        }
        [gctl deleteAutosaveFilesForGame:game];
    }];
}

- (IBAction)like:(id)sender {
    NSIndexSet *rows = self.gameTableView.selectedRowIndexes;
    TableViewController * __weak weakSelf = self;

    // If we clicked outside selected rows, only show info for clicked row
    if (self.gameTableView.clickedRow != -1 &&
        ![rows containsIndex:(NSUInteger)self.gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)self.gameTableView.clickedRow];

    __block NSUInteger count = 0;
    [rows
     enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        Game *game = weakSelf.gameTableModel[idx];
        if (game.like == 1)
            count++;
    }];

    BOOL allLiked = (count == rows.count);

    // If all are liked: unlike all. If only some or none are liked: like all
    [rows
     enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        Game *game = weakSelf.gameTableModel[idx];
        if (allLiked)
            game.like = 0;
        else
            game.like = 1;
    }];
}

- (IBAction)dislike:(id)sender {
    NSIndexSet *rows = self.gameTableView.selectedRowIndexes;
    TableViewController * __weak weakSelf = self;

    // If we clicked outside selected rows, only show info for clicked row
    if (self.gameTableView.clickedRow != -1 &&
        ![rows containsIndex:(NSUInteger)self.gameTableView.clickedRow])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)self.gameTableView.clickedRow];

    __block NSUInteger count = 0;
    [rows
     enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        Game *game = weakSelf.gameTableModel[idx];
        if (game.like == 2)
            count++;
    }];

    BOOL allDisliked = (count == rows.count);

    // If all are disliked: undislike all. If only some or none are disliked: dislike all

    [rows
     enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        Game *game = weakSelf.gameTableModel[idx];
        if (allDisliked)
            game.like = 0;
        else
            game.like = 2;
    }];
}

- (IBAction)cancel:(id)sender {
    self.currentlyAddingGames = NO;
    self.downloadWasCancelled = YES;
    self.verifyIsCancelled = YES;
    [self.alertQueue cancelAllOperations];
    [self.downloadQueue cancelAllOperations];
    [self.openGameQueue cancelAllOperations];
    [self endImporting];
}

#pragma mark Themes submenu

- (void)rebuildThemesSubmenu {

    self.enabledThemeItem = nil;
    NSMenu *themesMenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Apply Theme", nil)];

    themesMenu.showsStateColumn = YES;

    NSFetchRequest *fetchRequest = [Theme fetchRequest];
    fetchRequest.sortDescriptors = @[[NSSortDescriptor sortDescriptorWithKey:@"editable" ascending:YES], [NSSortDescriptor sortDescriptorWithKey:@"name" ascending:YES selector:@selector(localizedStandardCompare:)]];
    NSError *error = nil;

    NSArray *themes = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];
    if (themes == nil) {
        NSLog(@"Problem! %@",error);
    }

    NSMenu *themesMenu2 = themesMenu.copy;

    for (Theme *theme in themes) {
        [themesMenu addItemWithTitle:theme.name action:@selector(applyTheme:) keyEquivalent:@""];
        [themesMenu2 addItemWithTitle:theme.name action:@selector(applyTheme:) keyEquivalent:@""];
    }

    self.themesSubMenu.submenu = themesMenu;
    self.mainThemesSubMenu.submenu = themesMenu2;
}

#pragma mark Menu validation

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    SEL action = menuItem.action;
    NSInteger count = self.gameTableView.numberOfSelectedRows;
    NSIndexSet *rows = self.gameTableView.selectedRowIndexes;

    if (self.gameTableView.clickedRow != -1 &&
        (![rows containsIndex:(NSUInteger)self.gameTableView.clickedRow])) {
        count = 1;
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)self.gameTableView.clickedRow];
    }

    if (count == 0 &&
        (action == @selector(delete:) ||
         action == @selector(deleteGame:) ||
         action == @selector(openIfdb:) ||
         action == @selector(applyTheme:) ||
         action == @selector(selectSameTheme:) ||
         action == @selector(like:) ||
         action == @selector(download:) ||
         action == @selector(deleteSaves:) ||
         action == @selector(dislike:))) {
        return NO;
    }

    if (action == @selector(delete:) || action == @selector(deleteGame:)) {
        return !self.currentlyAddingGames;
    }

    if (action == @selector(applyTheme:)) {
        if ([Preferences instance].darkOverrideActive || [Preferences instance].lightOverrideActive)
            return NO;

        if (self.enabledThemeItem != nil) {
            for (NSMenuItem *item in self.themesSubMenu.submenu.itemArray) {
                item.state = NSOffState;
            }
            for (NSMenuItem *item in self.mainThemesSubMenu.submenu.itemArray) {
                item.state = NSOffState;
            }
            self.enabledThemeItem = nil;
        }

        if (count > 0 && count < 10000) {
            NSMutableSet<NSString *> __block *themeNamesToSelect = [NSMutableSet new];
            [rows enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
                NSString *name = self.gameTableModel[idx].theme.name;
                if (name)
                    [themeNamesToSelect addObject:name];
            }];
            NSControlStateValue state = NSOnState;
            if (themeNamesToSelect.count > 1)
                state = NSMixedState;
            for (NSString *name in themeNamesToSelect) {
                self.enabledThemeItem = [self.mainThemesSubMenu.submenu itemWithTitle:name];
                if (self.enabledThemeItem)
                    self.enabledThemeItem.state = state;
                self.enabledThemeItem = [self.themesSubMenu.submenu itemWithTitle:name];
                if (self.enabledThemeItem)
                    self.enabledThemeItem.state = state;
            }
        }
    }

    if (action == @selector(performFindPanelAction:)) {
        if (menuItem.tag == NSTextFinderActionShowFindInterface)
            return YES;
        else
            return NO;
    }

    if (action == @selector(addGamesToLibrary:))
        return !self.currentlyAddingGames;

    if (action == @selector(showGameInfo:)
        || action == @selector(reset:))
        return (count > 0 && count < 20);

    if (action == @selector(revealGameInFinder:))
        return count > 0 && count < 10;

    if (action == @selector(play:))
        return count == 1;

    if (action == @selector(selectSameTheme:)) {
        // Check if all selected games use the same theme
        NSArray *selection = [self.gameTableModel objectsAtIndexes:rows];
        Theme *selectionTheme = nil;
        for (Game *game in selection) {
            if (selectionTheme == nil) {
                selectionTheme = game.theme;
            } else {
                if (game.theme != selectionTheme) {
                    return NO;
                }
            }
        }
    }

    if (action == @selector(deleteSaves:)) {
        NSArray *selection = [self.gameTableModel objectsAtIndexes:rows];
        for (Game *game in selection) {
            if (game.autosaved) return YES;
        }
        return NO;
    }

    if (action == @selector(like:) || action == @selector(dislike:)) {
        BOOL like = (action == @selector(like:));
        NSArray *selection = [self.gameTableModel objectsAtIndexes:rows];
        int likecount[3] = { 0,0,0 };
        for (Game *game in selection) {
            likecount[game.like]++;
        }
        if (like) {
            if (likecount[1] > 0) {
                menuItem.title = NSLocalizedString(@"Liked", nil);
                if (likecount[0] > 0 || likecount[2] > 0) {
                    menuItem.state = NSMixedState;
                } else {
                    menuItem.state = NSOnState;
                }
            } else {
                menuItem.title = NSLocalizedString(@"Like", nil);
                menuItem.state = NSOffState;
            }
        } else {
            if (likecount[2] > 0) {
                menuItem.title = NSLocalizedString(@"Disliked", nil);
                if (likecount[0] > 0 || likecount[1] > 0) {
                    menuItem.state = NSMixedState;
                } else {
                    menuItem.state = NSOnState;
                }
            } else {
                menuItem.title = NSLocalizedString(@"Dislike", nil);
                menuItem.state = NSOffState;
            }
        }
    }
    return YES;
}

- (BOOL)validateToolbarItem:(NSToolbarItem *)item {
    BOOL enable = YES;
    NSUInteger count = self.selectedGames.count;
    if ([item.itemIdentifier isEqual:@"showInfo"] ) {
        return (count > 0 && count < 20);
    } else if ([item.itemIdentifier isEqual:@"playGame"]) {
        enable = (count == 1);
    } else if ([item.itemIdentifier isEqual:@"addToLibrary"]) {
        enable = (self.currentlyAddingGames == NO);
    }
    return enable;
}

@end
