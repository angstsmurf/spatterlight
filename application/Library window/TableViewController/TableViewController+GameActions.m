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
#import "TableViewController+TableDelegate.h"

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
#import "IFDB.h"

#import "GameLauncher.h"
#import "MetadataHandler.h"
#import "NSAlert+Spatterlight.h"

@implementation TableViewController (GameActions)

#pragma mark Contextual menu

- (IBAction)play:(id)sender {
    NSInteger rowidx;

    if (self.gameTableView.clickedRow != -1)
        rowidx = self.gameTableView.clickedRow;
    else
        rowidx = self.gameTableView.selectedRow;

    Game *game = [self gameForRow:rowidx];
    if (game)
        [self.gameLauncher playGame:game restorationHandler:nil];
}

- (IBAction)revealGameInFinder:(id)sender {
    for (Game *game in self.targetGames) {
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

    NSMutableArray<Game *> *running = [NSMutableArray new];

    for (Game *game in self.targetGames) {
        if (self.gameSessions[game.hashTag] == nil) {
            [self.managedObjectContext deleteObject:game];
        } else {
            [running addObject:game];
        }
    }

    NSUInteger count = running.count;
    if (count) {
        NSAlert *alert = [NSAlert alertWithMessage:NSLocalizedString(@"Are you sure?", nil)
                                              info:[NSString stringWithFormat:@"%@ %@ currently in use. Do you want to close and delete %@?", [NSString stringWithSummaryOfGames:running], (count == 1) ? @"is" : @"are", (count == 1) ? @"it" : @"them"]
                                           buttons:@[NSLocalizedString(@"Delete", nil), NSLocalizedString(@"Cancel", nil)]];

        if ([alert runModal] == NSAlertFirstButtonReturn) {
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
    for (Game *game in self.targetGames) {
        NSURL *url = [IFDB viewGameURLForTUID:game.metadata.tuid IFID:game.ifid iFiction:NO];
        if (url)
            [[NSWorkspace sharedWorkspace] openURL:url];
    }
}

- (IBAction)download:(id)sender {
    NSArray<Game *> *games = self.targetGames;
    if (games.count > 0)
        [self.metadataHandler downloadMetadataForGames:games];
}

- (IBAction)applyTheme:(id)sender {
    NSString *name = ((NSMenuItem *)sender).title;

    Theme *theme = [Fetches findTheme:name inContext:self.managedObjectContext];

    if (!theme) {
        NSLog(@"applyTheme: found no theme with name %@", name);
        return;
    }

    NSSet *games = [NSSet setWithArray:self.targetGames];
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
     postNotification:[NSNotification notificationWithName:kPreferencesChangedNotification object:theme]];
}

- (IBAction)selectSameTheme:(id)sender {
    Game *selectedGame = self.targetGames.firstObject;
    if (!selectedGame)
        return;

    NSSet *gamesWithTheme = selectedGame.theme.games;

    NSIndexSet *matchingIndexes = [self.gameTableModel indexesOfObjectsPassingTest:^BOOL(Game *obj, NSUInteger idx, BOOL *stop) {
        return [gamesWithTheme containsObject:obj];
    }];

    [self.gameTableView selectRowIndexes:matchingIndexes byExtendingSelection:NO];
}

- (IBAction)deleteSaves:(id)sender {
    for (Game *game in self.targetGames) {
        GlkController *gctl = self.gameSessions[game.hashTag];

        if (!gctl) {
            gctl = [[GlkController alloc] init];
        }
        [gctl deleteAutosaveFilesForGame:game];
    }
}

//! Like (1) or dislike (2) the games, or clear it if all of them already
//! have that value.
- (void)toggleLike:(int32_t)value forGames:(NSArray<Game *> *)games {
    BOOL allSet = YES;
    for (Game *game in games) {
        if (game.like != value) {
            allSet = NO;
            break;
        }
    }
    for (Game *game in games)
        game.like = allSet ? 0 : value;
}

- (IBAction)like:(id)sender {
    [self toggleLike:1 forGames:self.targetGames];
}

- (IBAction)dislike:(id)sender {
    [self toggleLike:2 forGames:self.targetGames];
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

    NSFetchRequest *fetchRequest = [Theme fetchRequest];
    fetchRequest.sortDescriptors = @[[NSSortDescriptor sortDescriptorWithKey:@"editable" ascending:YES], [NSSortDescriptor sortDescriptorWithKey:@"name" ascending:YES selector:@selector(localizedStandardCompare:)]];
    NSError *error = nil;

    NSArray<Theme *> *themes = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];
    if (themes == nil) {
        NSLog(@"Problem! %@",error);
    }

    // One menu for the contextual menu, one for the main menu
    NSMenu *(^makeMenu)(void) = ^{
        NSMenu *menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Apply Theme", nil)];
        menu.showsStateColumn = YES;
        for (Theme *theme in themes)
            [menu addItemWithTitle:theme.name action:@selector(applyTheme:) keyEquivalent:@""];
        return menu;
    };

    self.themesSubMenu.submenu = makeMenu();
    self.mainThemesSubMenu.submenu = makeMenu();
}

#pragma mark Menu validation

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    SEL action = menuItem.action;
    NSArray<Game *> *selection = self.targetGames;
    NSUInteger count = selection.count;

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
                item.state = NSControlStateValueOff;
            }
            for (NSMenuItem *item in self.mainThemesSubMenu.submenu.itemArray) {
                item.state = NSControlStateValueOff;
            }
            self.enabledThemeItem = nil;
        }

        if (count > 0 && count < 10000) {
            NSMutableSet<NSString *> *themeNamesToSelect = [NSMutableSet new];
            for (Game *game in selection) {
                NSString *name = game.theme.name;
                if (name)
                    [themeNamesToSelect addObject:name];
            }
            NSControlStateValue state = NSControlStateValueOn;
            if (themeNamesToSelect.count > 1)
                state = NSControlStateValueMixed;
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
        return (menuItem.tag == NSTextFinderActionShowFindInterface);
    }

    if (action == @selector(addGamesToLibrary:))
        return !self.currentlyAddingGames;

    if (action == @selector(showGameInfo:))
        return (count > 0 && count <= kMaxInfoWindowsAtOnce);

    if (action == @selector(revealGameInFinder:))
        return count > 0 && count < 10;

    if (action == @selector(play:))
        return count == 1;

    if (action == @selector(selectSameTheme:)) {
        // Check if all selected games use the same theme
        Theme *selectionTheme = selection.firstObject.theme;
        for (Game *game in selection) {
            if (game.theme != selectionTheme)
                return NO;
        }
    }

    if (action == @selector(deleteSaves:)) {
        for (Game *game in selection) {
            if (game.autosaved) return YES;
        }
        return NO;
    }

    if (action == @selector(like:) || action == @selector(dislike:)) {
        int32_t value = (action == @selector(like:)) ? 1 : 2;
        NSUInteger matching = 0;
        for (Game *game in selection) {
            if (game.like == value)
                matching++;
        }
        if (value == 1)
            menuItem.title = matching ? NSLocalizedString(@"Liked", nil) : NSLocalizedString(@"Like", nil);
        else
            menuItem.title = matching ? NSLocalizedString(@"Disliked", nil) : NSLocalizedString(@"Dislike", nil);
        if (matching == 0)
            menuItem.state = NSControlStateValueOff;
        else if (matching == count)
            menuItem.state = NSControlStateValueOn;
        else
            menuItem.state = NSControlStateValueMixed;
    }
    return YES;
}

- (BOOL)validateToolbarItem:(NSToolbarItem *)item {
    BOOL enable = YES;
    NSUInteger count = self.selectedGames.count;
    if ([item.itemIdentifier isEqual:@"showInfo"] ) {
        return (count > 0 && count <= kMaxInfoWindowsAtOnce);
    } else if ([item.itemIdentifier isEqual:@"playGame"]) {
        enable = (count == 1);
    } else if ([item.itemIdentifier isEqual:@"addToLibrary"]) {
        enable = (self.currentlyAddingGames == NO);
    }
    return enable;
}

@end
