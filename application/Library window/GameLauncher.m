//
//  GameLauncher.m
//  Spatterlight
//
//  Extracted from TableViewController.m
//

#import <BlorbFramework/Blorb.h>

#import "GameLauncher.h"

#import "TableViewController.h"
#import "TableViewController+Internal.h"
#import "TableViewController+TableDelegate.h"
#import "TableViewController+LibraryManagement.h"

#import "LibHelperTableView.h"
#import "AppDelegate.h"
#import "LibController.h"
#import "InfoController.h"
#import "GlkController.h"
#import "GlkController+Autorestore.h"
#import "Preferences.h"
#import "Constants.h"

#import "NSString+Categories.h"

#import "Game.h"
#import "Metadata.h"
#import "Image.h"
#import "Ifid.h"
#import "Fetches.h"

#import "GameImporter.h"
#import "MissingFilesFinder.h"
#import "CommandScriptHandler.h"
#import "FolderAccess.h"

#import "CoreDataManager.h"
#import "OpenGameOperation.h"

@implementation GameLauncher

- (instancetype)initWithTableViewController:(TableViewController *)tableViewController {
    self = [super init];
    if (self) {
        _tableViewController = tableViewController;
    }
    return self;
}

#pragma mark - Command Scripts

- (void)runCommandsFromFile:(NSString *)filename {
    GlkController *gctl = [self activeGlkController];
    if (gctl) {
        [gctl.commandScriptHandler runCommandsFromFile:filename];
    }
}

- (void)restoreFromSaveFile:(NSString *)filename {
    GlkController *gctl = [self activeGlkController];
    if (gctl) {
        [gctl.commandScriptHandler restoreFromSaveFile:filename];
    }
}

- (nullable GlkController *)activeGlkController {
    TableViewController *tvc = self.tableViewController;
    Game *game = [Preferences instance].currentGame;
    if (!game)
        return nil;
    GlkController *gctl = tvc.gameSessions[game.hashTag];
    if (!gctl.alive) {
        // If the current game is finished, look for another
        gctl = nil;
        for (GlkController *g in tvc.gameSessions.allValues) {
            if (g.alive) {
                gctl = g;
                break;
            }
        }
    }
    return gctl;
}

- (BOOL)hasActiveGames {
    TableViewController *tvc = self.tableViewController;
    for (GlkController *gctl in tvc.gameSessions.allValues)
        if (gctl.alive)
            return YES;
    return NO;
}

#pragma mark - Playing Games

- (nullable NSWindow *)playGameWithHash:(NSString *)hash restorationHandler:(void (^)(NSWindow *, NSError *))completionHandler  {
    TableViewController *tvc = self.tableViewController;
    Game *game = [Fetches fetchGameForHash:hash inContext:tvc.managedObjectContext];
    if (!game) return nil;
    return [self playGame:game restorationHandler:completionHandler];
}

- (nullable NSWindow *)playGame:(Game *)game restorationHandler:(nullable void (^)(NSWindow *, NSError *))completionHandler {

    TableViewController *tvc = self.tableViewController;

    NSURL *url = game.urlForBookmark;
    NSString *path = url.path;
    NSString *terp;
    GlkController *gctl = tvc.gameSessions[game.hashTag];

    BOOL systemWindowRestoration = (completionHandler != nil);

    NSFileManager *fileManager = [NSFileManager defaultManager];

    if (gctl) {
        NSLog(@"A game with this hash is already in session");
        [gctl.window makeKeyAndOrderFront:nil];
        return gctl.window;
    }

    if (![fileManager fileExistsAtPath:path]) {
        game.found = NO;
        if (!systemWindowRestoration)
            [tvc lookForMissingFile:game];
        return nil;
    } else game.found = YES;

    //Check if the file is in trash
    NSError *error = nil;
    NSURL *trashUrl = [fileManager URLForDirectory:NSTrashDirectory inDomain:NSUserDomainMask appropriateForURL:[NSURL fileURLWithPath:path isDirectory:NO] create:NO error:&error];
    if (error)
        NSLog(@"Error getting Trash path: %@", error);
    if (trashUrl && [path hasPrefix:trashUrl.path]) {
        if (!systemWindowRestoration) {
            NSAlert *alert = [[NSAlert alloc] init];
            alert.messageText = NSLocalizedString(@"This game is in Trash.", nil);
            alert.informativeText = NSLocalizedString(@"You won't be able to play the game until you move the file out of there.", nil);
            [alert addButtonWithTitle:NSLocalizedString(@"Okay", nil)];
            [alert addButtonWithTitle:NSLocalizedString(@"Delete entry", nil)];
            NSInteger choice = [alert runModal];
            if (choice == NSAlertSecondButtonReturn) {
                [game.managedObjectContext deleteObject:game];
            }
        }
        return nil;
    }

    if (!game.detectedFormat) {
        game.detectedFormat = game.metadata.format;
    }

    terp = gExtMap[path.pathExtension.lowercaseString];

    if (!terp)
        terp = gFormatMap[game.detectedFormat];

    if (!terp) {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = NSLocalizedString(@"Cannot play the file.", nil);
        alert.informativeText = NSLocalizedString(@"The game is not in a recognized file format; cannot find a suitable interpreter.", nil);
        [alert runModal];
        return nil;
    }

    gctl = [[GlkController alloc] initWithWindowNibName:@"GameWindow"];
    if ([terp isEqualToString:@"glulxe"] || [terp isEqualToString:@"fizmo"] || [terp isEqualToString:@"bocfel"]) {
        gctl.window.restorable = YES;
        gctl.window.restorationClass = [AppDelegate class];
        gctl.window.identifier = [NSString stringWithFormat:@"gameWin%@", game.hashTag];
    } else {
        gctl.window.restorable = NO;
    }

    if (game.metadata.title.length)
        gctl.window.title = game.metadata.title;
    else
        NSLog(@"Game has no metadata?");

    TableViewController __weak *weakTVC = tvc;

    GlkController __block *blockgctl = gctl;
    Game __block *blockGame = game;
    NSString __block *hashTag = game.hashTag;

    [gctl askForAccessToURL:url showDialog:!systemWindowRestoration andThenRunBlock:^{
        OpenGameOperation *operation = [[OpenGameOperation alloc] initWithURL:url completionHandler:^(NSData * _Nullable newData, NSURL * _Nullable newURL) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [blockgctl.slowReadAlert.window close];
                blockgctl.slowReadAlert = nil;

                if (hashTag.length == 0) {
                    hashTag = game.path.signatureFromFile;
                }

                if (newData.length == 0 || hashTag.length == 0) {
                    NSAlert *alert = [[NSAlert alloc] init];
                    alert.messageText = NSLocalizedString(@"Failed to open the game.", nil);
                    alert.informativeText = [NSString stringWithFormat:NSLocalizedString(@"File access to \"%@\" was not permitted.", nil), newURL.lastPathComponent];
                    [alert runModal];
                } else {
                    game.hashTag = hashTag;
                    weakTVC.gameSessions[hashTag] = blockgctl;
                    blockGame.lastPlayed = [NSDate date];
                    blockgctl.gameData = newData;
                    blockgctl.gameFileURL = newURL;

                    [blockgctl runTerp:terp withGame:blockGame reset:NO restorationHandler:completionHandler];

                    [((AppDelegate *)NSApp.delegate)
                     addToRecents:@[ newURL ]];

                    if ([Blorb isBlorbURL:newURL]) {
                        if (!blockGame.metadata.cover || [Blorb isBlorbURL:[NSURL fileURLWithPath:blockGame.metadata.cover.originalURL]]) {
                            Blorb *blorb = [[Blorb alloc] initWithData:newData];
                            GameImporter *importer = [[GameImporter alloc] initWithTableViewController:weakTVC];
                            blockGame.metadata.coverArtURL = newURL.absoluteString;
                            [importer updateImageFromBlorb:blorb inGame:blockGame];
                        }
                    }
                }
            });
        }];

        operation.queuePriority = NSOperationQueuePriorityVeryHigh;
        operation.qualityOfService = NSQualityOfServiceUserInteractive;
        [weakTVC.openGameQueue addOperation:operation];

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            if (gctl.gameData == nil && !systemWindowRestoration) {
                gctl.slowReadAlert = [[NSAlert alloc] init];
                NSAlert __weak *alert = gctl.slowReadAlert;
                alert.messageText =
                [NSString stringWithFormat:NSLocalizedString(@"The game \"%@\" is taking a long time to load.", nil), blockGame.metadata.title];
                [alert addButtonWithTitle:NSLocalizedString(@"Cancel loading", nil)];

                [alert beginSheetModalForWindow:gctl.window completionHandler:^(NSModalResponse result){
                    if (result == NSAlertFirstButtonReturn) {
                        [operation cancel];
                    }
                }];
            }
        });
    }];

    return gctl.window;
}

#pragma mark - Import and Play

- (nullable NSWindow *)importAndPlayGame:(NSString *)path {
    BOOL hide = ![[NSUserDefaults standardUserDefaults] boolForKey:@"AddToLibrary"];
    Game *game = [self importGame:path inContext:self.tableViewController.managedObjectContext reportFailure:YES hide:hide];
    if (game) {
        return [self selectAndPlayGame:game];
    }
    return nil;
}

- (NSWindow *)selectAndPlayGame:(Game *)game {
    TableViewController *tvc = self.tableViewController;
    NSWindow *result = [self playGame:game restorationHandler:nil];
    NSUInteger gameIndex = [tvc.gameTableModel indexOfObject:game];
    if (gameIndex >= (NSUInteger)tvc.gameTableView.numberOfRows) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.6 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void) {
            [tvc selectGames:[NSSet setWithObject:game]];
            [tvc.gameTableView scrollRowToVisible:(NSInteger)[tvc.gameTableModel indexOfObject:game]];
        });
    } else {
        [tvc selectGames:[NSSet setWithObject:game]];
        [tvc.gameTableView scrollRowToVisible:(NSInteger)gameIndex];
    }
    return result;
}

- (Game *)importGame:(NSString*)path inContext:(NSManagedObjectContext *)context reportFailure:(BOOL)report hide:(BOOL)hide {
    TableViewController *tvc = self.tableViewController;
    GameImporter *importer = [[GameImporter alloc] initWithTableViewController:tvc];
    Game *result = [importer importGame:path inContext:context reportFailure:report hide:hide];
    if (result && !result.metadata.cover)
        [importer lookForImagesForGame:result];
    return result;
}

@end
