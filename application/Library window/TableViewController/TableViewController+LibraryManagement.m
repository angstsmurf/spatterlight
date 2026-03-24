//
//  TableViewController+LibraryManagement.m
//  Spatterlight
//
//  Library-level operations (delete, prune, verify).
//

#import <CoreSpotlight/CoreSpotlight.h>

#import "TableViewController+LibraryManagement.h"
#import "TableViewController+Internal.h"
#import "TableViewController+GameActions.h"

#import "GlkController.h"
#import "Constants.h"

#import "NSString+Categories.h"
#import "NSManagedObjectContext+safeSave.h"

#import "Game.h"
#import "Metadata.h"
#import "Ifid.h"
#import "Image.h"
#import "Fetches.h"

#import "MissingFilesFinder.h"
#import "GameImporter.h"
#import "NotificationBezel.h"
#import "FolderAccess.h"

#import "CoreDataManager.h"
#import "InfoController.h"

#include "babel_handler.h"
#include "ifiction.h"
#include "treaty.h"

@implementation TableViewController (LibraryManagement)

+ (NSString *)ifidFromFile:(NSString *)path {
    NSString *result = @"";
    if (!path.length)
        return result;
    void *context = get_babel_ctx();
    if (context == NULL) {
        return result;
    }

    int rv = 0;
    char buf[TREATY_MINIMUM_EXTENT];

    char *format = babel_init_ctx((char *)path.fileSystemRepresentation, context);
    if (format && babel_get_authoritative_ctx(context)) {
        rv = babel_treaty_ctx(GET_STORY_FILE_IFID_SEL, buf, sizeof buf, context);
    }

    babel_release_ctx(context);
    free(context);

    if (rv <= 0) {
        return result;
    }

    result = @(buf);
    if (!result.length)
        return @"";
    else
        return result;
}

- (void)deleteGameMetaAndIfid:(Game *)game inContext:(NSManagedObjectContext *)context {
    // Don't delete games that are playing
    if (game.hashTag.length && self.gameSessions[game.hashTag]) {
        return;
    }
    NSSet *ifids = game.metadata.ifids.copy;
    for (Ifid *ifid in ifids) {
        [context deleteObject:ifid];
    }
    if (game.metadata)
        [context deleteObject:game.metadata];
    if (game)
        [context deleteObject:game];
}


- (void)deleteIfDuplicate:(NSString *)hashTag inContext:(NSManagedObjectContext *)context {
    if (!hashTag.length)
        return;
    NSError *error = nil;
    NSFetchRequest *fetchRequest = [Game fetchRequest];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"hashTag like[c] %@",hashTag];

    NSArray<Game *> *fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"Problem! %@",error);
    }

    if (fetchedObjects.count > 1) {
        Game *first = fetchedObjects.firstObject;
        for (Game *game in fetchedObjects) {
            if (game != first) {
                [self deleteGameMetaAndIfid:game inContext:context];
            }
        }
    }
}

- (IBAction)deleteLibrary:(id)sender {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:NSLocalizedString(@"Do you really want to delete the library?", nil)];
    [alert setInformativeText:NSLocalizedString(@"This will empty your game list and delete all metadata. The original game files will not be affected.", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Delete Library", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
    if (self.gameSessions.count) {
        alert.accessoryView = self.forceQuitView;
        self.forceQuitCheckBox.state = NSOffState;
    }

    NSModalResponse choice = [alert runModal];

    if (choice == NSAlertFirstButtonReturn) {

        [self cancel:nil];
        [self.coreDataManager saveChanges];
        BOOL forceQuit = self.forceQuitCheckBox.state == NSOnState;

        CSSearchableIndex *index = [CSSearchableIndex defaultSearchableIndex];
        [index deleteSearchableItemsWithDomainIdentifiers:@[@"net.ccxvii.spatterlight"] completionHandler:^(NSError *blockerror){
            if (blockerror) {
                NSLog(@"Deleting all searchable items failed: %@", blockerror);
            }
        }];

        NSArray *entitiesToDelete = @[@"Metadata", @"Game", @"Ifid", @"Image"];

        NSMutableSet<NSManagedObjectID *> *gamesToKeep = [[NSMutableSet alloc] initWithCapacity:self.gameSessions.count];
        NSMutableSet<NSManagedObjectID *> *metadataToKeep = [[NSMutableSet alloc] initWithCapacity:self.gameSessions.count];
        NSMutableSet<NSManagedObjectID *> *imagesToKeep = [[NSMutableSet alloc] initWithCapacity:self.gameSessions.count];
        NSMutableSet<NSManagedObjectID *> *ifidsToKeep = [[NSMutableSet alloc] init];

        for (GlkController *ctl in self.gameSessions.allValues) {
            if (forceQuit || !ctl.alive) {
                if (self.infoWindows[ctl.game.hashTag]) {
                    self.infoWindows[ctl.game.hashTag].inDeletion = YES;
                }
                [ctl.window close];
            } else {
                Game *game = ctl.game;
                [gamesToKeep addObject:game.objectID];
                [metadataToKeep addObject:game.metadata.objectID];
                if (game.metadata.cover)
                    [imagesToKeep addObject:game.metadata.cover.objectID];
                for (Ifid *ifid in game.metadata.ifids)
                    [ifidsToKeep addObject:ifid.objectID];
            }
        }

        if (forceQuit) {
            self.gameSessions = [NSMutableDictionary new];
        }

        NSManagedObjectContext *childContext = self.coreDataManager.privateChildManagedObjectContext;

        [childContext performBlock:^{
            for (NSString *entity in entitiesToDelete) {

                NSFetchRequest *fetchEntities = [[NSFetchRequest alloc] init];
                fetchEntities.entity = [NSEntityDescription entityForName:entity inManagedObjectContext:childContext];
                fetchEntities.resultType = NSManagedObjectIDResultType;
                fetchEntities.includesPropertyValues = NO; //only fetch the managedObjectID

                NSError *error = nil;
                NSArray *objectsToDelete = [childContext executeFetchRequest:fetchEntities error:&error];
                if (error)
                    NSLog(@"deleteLibrary: %@", error);

                NSMutableSet *set = [NSMutableSet setWithArray:objectsToDelete];

                if ([entity isEqualToString:@"Game"]) {
                    [set minusSet:gamesToKeep];
                } else if ([entity isEqualToString:@"Metadata"]) {
                    [set minusSet:metadataToKeep];
                } else if ([entity isEqualToString:@"Image"]) {
                    [set minusSet:imagesToKeep];
                } else if ([entity isEqualToString:@"Ifid"]) {
                    [set minusSet:ifidsToKeep];
                }

                for (NSManagedObjectID *objectID in set) {
                    NSManagedObject *obj = [childContext objectWithID:objectID];
                    if (obj)
                        [childContext deleteObject:obj];
                }
            }
            [childContext safeSave];
        }];
    }
}

- (IBAction)pruneLibrary:(id)sender {

    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = NSLocalizedString(@"Do you really want to prune the library?", nil);
    alert.informativeText = NSLocalizedString(@"Pruning will delete the information about games that are not in the library at the moment.", nil);
    [alert addButtonWithTitle:NSLocalizedString(@"Prune", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

    NSModalResponse choice = [alert runModal];

    if (choice == NSAlertFirstButtonReturn) {

        [self cancel:nil];

        NSArray *gameEntriesToDelete =
        [Fetches fetchObjects:@"Game" predicate:@"hidden == YES" inContext:self.managedObjectContext];
        NSUInteger counter = gameEntriesToDelete.count;
        for (Game *game in gameEntriesToDelete) {
            if (game.metadata)
                NSLog(@"pruneLibrary: Deleting Game object with title %@", game.metadata.title);
            [self.managedObjectContext deleteObject:game];
        }

        NSArray *metadataEntriesToDelete =
        [Fetches fetchObjects:@"Metadata" predicate:@"game == NIL" inContext:self.managedObjectContext];
        counter += metadataEntriesToDelete.count;

        for (Metadata *meta in metadataEntriesToDelete) {
            NSLog(@"pruneLibrary: Deleting Metadata object with title %@", meta.title);
            [self.managedObjectContext deleteObject:meta];
        }

        // Now we removed any orphaned images
        NSArray *imageEntriesToDelete = [Fetches fetchObjects:@"Image" predicate:@"ANY metadata == NIL" inContext:self.managedObjectContext];

        counter += imageEntriesToDelete.count;
        for (Image *img in imageEntriesToDelete) {
            NSLog(@"pruneLibrary: Deleting Image object with url %@", img.originalURL);
            [self.managedObjectContext deleteObject:img];
        }

        [self.coreDataManager saveChanges];

        // And then any orphaned ifids
        NSArray *ifidEntriesToDelete = [Fetches fetchObjects:@"Ifid" predicate:@"metadata == NIL" inContext:self.managedObjectContext];

        counter += ifidEntriesToDelete.count;
        for (Ifid *ifid in ifidEntriesToDelete) {
            [self.managedObjectContext deleteObject:ifid];
        }

        [self.coreDataManager saveChanges];

        NotificationBezel *notification = [[NotificationBezel alloc] initWithScreen:self.view.window.screen];
        [notification showStandardWithText:[NSString stringWithFormat:@"%ld entit%@ pruned", counter, counter == 1 ? @"y" : @"ies"]];

        [self.coreDataManager saveChanges];
    }
}

#pragma mark Check library for missing files

- (IBAction)verifyLibrary:(id)sender {
    if ([self verifyAlert] == NSAlertFirstButtonReturn) {
        self.verifyIsCancelled = NO;
        [self verifyInBackground:nil];
    }
}

- (NSModalResponse)verifyAlert {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    if ([defaults boolForKey:@"VerifyAlertSuppression"])
        return NSAlertFirstButtonReturn;

    NSAlert *alert = [NSAlert new];
    alert.messageText = NSLocalizedString(@"Verify Library", nil);
    alert.informativeText = NSLocalizedString(@"Check the library for missing game files.", nil);
    alert.showsSuppressionButton = YES;
    alert.suppressionButton.title = NSLocalizedString(@"Remember these choices", nil);

    self.verifyDeleteMissingCheckbox.state = [defaults boolForKey:@"DeleteMissing"];
    self.verifyReCheckbox.state = [defaults boolForKey:@"RecheckForMissing"];
    self.verifyFrequencyTextField.integerValue = [defaults integerForKey:@"RecheckFrequency"];

    alert.accessoryView = self.verifyView;
    [alert addButtonWithTitle:NSLocalizedString(@"Check", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
    NSModalResponse choice = [alert runModal];

    if (choice == NSAlertFirstButtonReturn) {
        [defaults setBool:alert.suppressionButton.state ? YES : NO forKey:@"VerifyAlertSuppression"];
        [defaults setBool:self.verifyReCheckbox.state ? YES : NO forKey:@"RecheckForMissing"];
        if (self.verifyReCheckbox.state)
            [self startVerifyTimer];
        else
            [self stopVerifyTimer];
        [defaults setBool:self.verifyDeleteMissingCheckbox.state ? YES : NO forKey:@"DeleteMissing"];
        [defaults setInteger:self.verifyFrequencyTextField.integerValue forKey:@"RecheckFrequency"];
    }
    return choice;
}

- (void)verifyInBackground:(nullable id)sender {
    if (self.verifyIsCancelled) {
        self.verifyIsCancelled = NO;
        return;
    }

    NSManagedObjectContext *childContext = self.coreDataManager.privateChildManagedObjectContext;
    childContext.undoManager = nil;

    TableViewController * __weak weakSelf = self;

    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"StopIndexing" object:nil]];

    [childContext performBlock:^{
        TableViewController *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        BOOL deleteMissing = [[NSUserDefaults standardUserDefaults] boolForKey:@"DeleteMissing"];
        NSArray *gameTableCopy = strongSelf.gameTableModel.copy;
        for (Game *gameInMain in gameTableCopy) {
            if (strongSelf.verifyIsCancelled) {
                strongSelf.verifyIsCancelled = NO;
                return;
            }
            Game *game = [childContext objectWithID:gameInMain.objectID];
            if (game) {
                NSURL *gameURL = game.urlForBookmark;

                NSString *path = nil;

                if (gameURL) {
                    path = gameURL.path;
                } else {
                    path = game.path;
                }

                // Check if the game is playable
                NSString *terp = gExtMap[path.pathExtension.lowercaseString];
                if (!terp)
                    terp = gFormatMap[game.detectedFormat];
                if (!terp) {
                    [strongSelf deleteGameMetaAndIfid:game inContext:childContext];
                }

                if (![[NSFileManager defaultManager] fileExistsAtPath:path]) {
                    if (deleteMissing) {
                        [strongSelf deleteGameMetaAndIfid:game inContext:childContext];
                    } else {
                        game.found = NO;
                    }
                } else {
                    game.found = YES;

                    if (!game.hashTag.length) {
                        [FolderAccess askForAccessToURL:[NSURL fileURLWithPath:path isDirectory:NO] andThenRunBlock:^{
                            NSString *ifid = [TableViewController ifidFromFile:path];
                            if (![ifid isEqualToString:game.ifid]) {
                                BOOL hidden = game.hidden;
                                [strongSelf deleteGameMetaAndIfid:game inContext:childContext];
                                GameImporter *importer = [[GameImporter alloc] initWithTableViewController:strongSelf];
                                [importer importGame:path inContext:childContext reportFailure:NO hide:hidden];
                            } else {
                                game.hashTag = [path signatureFromFile];
                                game.metadata.hashTag = game.hashTag;
                            }
                            [strongSelf deleteIfDuplicate:game.hashTag inContext:childContext];
                        }];
                    }
                }
                [strongSelf deleteIfDuplicate:game.hashTag inContext:childContext];
            }
        }
        [childContext safeSave];

        [[NSNotificationCenter defaultCenter]
         postNotification:[NSNotification notificationWithName:@"StartIndexing" object:nil]];
    }];
}

- (void)startVerifyTimer {
    if (self.verifyTimer)
        [self.verifyTimer invalidate];
    self.verifyIsCancelled = NO;
    NSInteger minutes = [[NSUserDefaults standardUserDefaults] integerForKey:@"RecheckFrequency"];
    NSTimeInterval seconds = minutes * 60;
    self.verifyTimer = [NSTimer scheduledTimerWithTimeInterval:seconds
                                                   target:self
                                                 selector:@selector(verifyInBackground:)
                                                 userInfo:nil
                                                  repeats:YES];
    self.verifyTimer.tolerance = 10;
}

- (void)stopVerifyTimer {
    if (self.verifyTimer)
        [self.verifyTimer invalidate];
    self.verifyIsCancelled = YES;
}

- (void)lookForMissingFile:(Game *)game {
    if (!game)
        return;
    MissingFilesFinder *look = [MissingFilesFinder new];
    [look lookForMissingFile:game libController:self];
}

@end
