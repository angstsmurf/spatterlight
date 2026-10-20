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
#import "NSAlert+Spatterlight.h"


@implementation TableViewController (LibraryManagement)

- (NSSet<NSString *> *)runningHashTags {
    NSAssert(NSThread.isMainThread, @"gameSessions is main-thread state");
    return [NSSet setWithArray:self.gameSessions.allKeys];
}

- (void)deleteGameMetaAndIfid:(Game *)game inContext:(NSManagedObjectContext *)context sparing:(NSSet<NSString *> *)runningHashTags {
    // Don't delete games that are playing
    if (game.hashTag.length && [runningHashTags containsObject:game.hashTag]) {
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


// The order in which games sharing a hashTag are ranked when all but one are
// pruned: the survivor is the most recently played game (falling back to
// visible, earliest-added for never-played duplicates).
static NSArray<NSSortDescriptor *> *duplicateSurvivorOrder(void) {
    return @[
        // Most recently played first; nil (never played) sorts last under descending.
        [NSSortDescriptor sortDescriptorWithKey:@"lastPlayed" ascending:NO],
        [NSSortDescriptor sortDescriptorWithKey:@"hidden" ascending:YES],
        [NSSortDescriptor sortDescriptorWithKey:@"added" ascending:YES]
    ];
}

- (void)deleteIfDuplicate:(NSString *)hashTag inContext:(NSManagedObjectContext *)context sparing:(NSSet<NSString *> *)runningHashTags {
    if (!hashTag.length)
        return;
    NSError *error = nil;
    NSFetchRequest *fetchRequest = [Game fetchRequest];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"hashTag ==[c] %@",hashTag];
    fetchRequest.sortDescriptors = duplicateSurvivorOrder();

    NSArray<Game *> *fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"Problem! %@",error);
    }

    if (fetchedObjects.count > 1) {
        Game *first = fetchedObjects.firstObject;
        for (Game *game in fetchedObjects) {
            if (game != first) {
                [self deleteGameMetaAndIfid:game inContext:context sparing:runningHashTags];
            }
        }
    }
}

// Single grouped pass over *every* Game (including hidden ones) that prunes
// any games sharing a hashTag, keeping one survivor per hash. The fetch is
// sorted by duplicateSurvivorOrder() within each hash group.
// Games whose hashTag is in runningHashTags (currently playing) are never
// deleted, so an active session is never affected.
- (void)pruneDuplicateGamesInContext:(NSManagedObjectContext *)context sparing:(NSSet<NSString *> *)runningHashTags {
    NSError *error = nil;
    NSFetchRequest *fetchRequest = [Game fetchRequest];
    fetchRequest.sortDescriptors = [@[
        [NSSortDescriptor sortDescriptorWithKey:@"hashTag" ascending:YES selector:@selector(caseInsensitiveCompare:)]]
                                    arrayByAddingObjectsFromArray:duplicateSurvivorOrder()];

    NSArray<Game *> *games = [context executeFetchRequest:fetchRequest error:&error];
    if (!games) {
        NSLog(@"pruneDuplicateGamesInContext: %@", error);
        return;
    }

    NSString *survivorHash = nil;
    NSUInteger pruned = 0;
    for (Game *game in games) {
        NSString *hash = game.hashTag;
        // Games without a hash can't be reliably matched; leave them alone.
        if (!hash.length) {
            survivorHash = nil;
            continue;
        }
        if (survivorHash && [hash caseInsensitiveCompare:survivorHash] == NSOrderedSame) {
            // Same hash as the survivor we already kept: this is a duplicate.
            [self deleteGameMetaAndIfid:game inContext:context sparing:runningHashTags];
            pruned++;
        } else {
            // First game of a new hash group: keep it as the survivor.
            survivorHash = hash;
        }
    }
    if (pruned)
        NSLog(@"pruneDuplicateGamesInContext: pruned %lu duplicate game(s)", (unsigned long)pruned);
}

- (IBAction)deleteLibrary:(id)sender {
    NSAlert *alert = [NSAlert alertWithMessage:NSLocalizedString(@"Do you really want to delete the library?", nil)
                                          info:NSLocalizedString(@"This will empty your game list and delete all metadata. The original game files will not be affected.", nil)
                                       buttons:@[NSLocalizedString(@"Delete Library", nil), NSLocalizedString(@"Cancel", nil)]];
    if (self.gameSessions.count) {
        alert.accessoryView = self.forceQuitView;
        self.forceQuitCheckBox.state = NSControlStateValueOff;
    }

    NSModalResponse choice = [alert runModal];

    if (choice == NSAlertFirstButtonReturn) {

        [self cancel:nil];
        [self.coreDataManager saveChanges];
        BOOL forceQuit = self.forceQuitCheckBox.state == NSControlStateValueOn;

        // The Spotlight index holds the user's library, not a test store's
        if (![CoreDataManager testStoreURL]) {
            CSSearchableIndex *index = [CSSearchableIndex defaultSearchableIndex];
            [index deleteSearchableItemsWithDomainIdentifiers:@[@"net.ccxvii.spatterlight"] completionHandler:^(NSError *blockerror){
                if (blockerror) {
                    NSLog(@"Deleting all searchable items failed: %@", blockerror);
                }
            }];
        }

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

    NSAlert *alert = [NSAlert alertWithMessage:NSLocalizedString(@"Do you really want to prune the library?", nil)
                                          info:NSLocalizedString(@"Pruning will delete the information about games that are not in the library at the moment.", nil)
                                       buttons:@[NSLocalizedString(@"Prune", nil), NSLocalizedString(@"Cancel", nil)]];

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

    if ([defaults boolForKey:kVerifyAlertSuppressionKey])
        return NSAlertFirstButtonReturn;

    NSAlert *alert = [NSAlert new];
    alert.messageText = NSLocalizedString(@"Verify Library", nil);
    alert.informativeText = NSLocalizedString(@"Check the library for missing game files.", nil);
    alert.showsSuppressionButton = YES;
    alert.suppressionButton.title = NSLocalizedString(@"Remember these choices", nil);

    self.verifyDeleteMissingCheckbox.state = [defaults boolForKey:@"DeleteMissing"];
    self.verifyReCheckbox.state = [defaults boolForKey:kRecheckForMissingKey];
    self.verifyFrequencyTextField.integerValue = [defaults integerForKey:kRecheckFrequencyKey];

    alert.accessoryView = self.verifyView;
    [alert addButtonWithTitle:NSLocalizedString(@"Check", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
    NSModalResponse choice = [alert runModal];

    if (choice == NSAlertFirstButtonReturn) {
        [defaults setBool:alert.suppressionButton.state == NSControlStateValueOn forKey:kVerifyAlertSuppressionKey];
        [defaults setBool:self.verifyReCheckbox.state == NSControlStateValueOn forKey:kRecheckForMissingKey];
        if (self.verifyReCheckbox.state)
            [self startVerifyTimer];
        else
            [self stopVerifyTimer];
        [defaults setBool:self.verifyDeleteMissingCheckbox.state == NSControlStateValueOn forKey:@"DeleteMissing"];
        [defaults setInteger:self.verifyFrequencyTextField.integerValue forKey:kRecheckFrequencyKey];
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

    // gameTableModel and gameSessions are main-thread state: snapshot them
    // here rather than reading them from the child context's queue.
    NSArray<NSManagedObjectID *> *gameIDs = [self.gameTableModel valueForKey:@"objectID"];
    NSSet<NSString *> *running = self.runningHashTags;

    TableViewController * __weak weakSelf = self;

    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:SpatterlightStopIndexingNotification object:nil]];

    [childContext performBlock:^{
        TableViewController *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        // Prune duplicate games (same hashTag) across the whole library before
        // the per-game verification loop below, which only walks visible games.
        [strongSelf pruneDuplicateGamesInContext:childContext sparing:running];

        BOOL deleteMissing = [[NSUserDefaults standardUserDefaults] boolForKey:@"DeleteMissing"];
        NSMutableDictionary<NSManagedObjectID *, NSString *> *unhashed = [NSMutableDictionary new];
        for (NSManagedObjectID *gameID in gameIDs) {
            if (strongSelf.verifyIsCancelled) {
                strongSelf.verifyIsCancelled = NO;
                return;
            }
            Game *game = (Game *)[childContext existingObjectWithID:gameID error:NULL];
            if (!game || game.isDeleted)
                continue;

            NSURL *gameURL = game.urlForBookmark;
            NSString *path = gameURL ? gameURL.path : game.path;

            // Check if the game is playable
            NSString *terp = gExtMap[path.pathExtension.lowercaseString];
            if (!terp)
                terp = gFormatMap[game.detectedFormat];
            if (!terp) {
                [strongSelf deleteGameMetaAndIfid:game inContext:childContext sparing:running];
                continue;
            }

            if (![[NSFileManager defaultManager] fileExistsAtPath:path]) {
                if (deleteMissing) {
                    [strongSelf deleteGameMetaAndIfid:game inContext:childContext sparing:running];
                } else {
                    game.found = NO;
                }
            } else {
                game.found = YES;
                if (!game.hashTag.length)
                    unhashed[gameID] = path;
            }
        }
        [childContext safeSave];

        // Hashing a game may need the user to grant access to its folder,
        // which puts up an open panel: that has to happen on the main thread.
        if (unhashed.count) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [weakSelf hashGames:unhashed inContext:childContext];
            });
        }

        [[NSNotificationCenter defaultCenter]
         postNotification:[NSNotification notificationWithName:SpatterlightStartIndexingNotification object:nil]];
    }];
}

//! Gives the games in `paths` (game objectID → file path) the hashTag they
//! are missing, asking for folder access first where needed. A game whose
//! file now has a different IFID is re-imported instead.
- (void)hashGames:(NSDictionary<NSManagedObjectID *, NSString *> *)paths inContext:(NSManagedObjectContext *)childContext {
    TableViewController * __weak weakSelf = self;
    [paths enumerateKeysAndObjectsUsingBlock:^(NSManagedObjectID *gameID, NSString *path, BOOL *stop) {
        [FolderAccess askForAccessToURL:[NSURL fileURLWithPath:path isDirectory:NO] andThenRunBlock:^{
            TableViewController *strongSelf = weakSelf;
            if (!strongSelf)
                return;
            NSSet<NSString *> *running = strongSelf.runningHashTags;
            [childContext performBlock:^{
                Game *game = (Game *)[childContext existingObjectWithID:gameID error:NULL];
                if (!game || game.isDeleted || game.hashTag.length)
                    return;
                NSString *ifid = nil;
                [GameImporter identifyFile:path format:NULL ifid:&ifid];
                if (![ifid isEqualToString:game.ifid]) {
                    BOOL hidden = game.hidden;
                    [strongSelf deleteGameMetaAndIfid:game inContext:childContext sparing:running];
                    GameImporter *importer = [[GameImporter alloc] initWithTableViewController:strongSelf];
                    [importer importGame:path inContext:childContext reportFailure:NO hide:hidden];
                } else {
                    game.hashTag = [path signatureFromFile];
                    game.metadata.hashTag = game.hashTag;
                    [strongSelf deleteIfDuplicate:game.hashTag inContext:childContext sparing:running];
                }
                [childContext safeSave];
            }];
        }];
    }];
}

- (void)migrateConvertedAGTGames {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults boolForKey:kAGTConvertedToDirectMigrationDone] &&
        [defaults boolForKey:kAGTMigratedHashRepairDone])
        return;

    NSManagedObjectContext *childContext = self.coreDataManager.privateChildManagedObjectContext;
    childContext.undoManager = nil;

    TableViewController * __weak weakSelf = self;
    [childContext performBlock:^{
        TableViewController *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        GameImporter *importer = [[GameImporter alloc] initWithTableViewController:strongSelf];
        [importer migrateConvertedAGTGamesInContext:childContext];
        [importer repairHashesOfMigratedAGTGamesInContext:childContext];
        [childContext safeSave];
    }];
}

- (void)migrateOutdatedIfids {
    if ([[NSUserDefaults standardUserDefaults] boolForKey:kOutdatedIfidMigrationDone])
        return;

    NSManagedObjectContext *childContext = self.coreDataManager.privateChildManagedObjectContext;
    childContext.undoManager = nil;

    TableViewController * __weak weakSelf = self;
    [childContext performBlock:^{
        TableViewController *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        GameImporter *importer = [[GameImporter alloc] initWithTableViewController:strongSelf];
        [importer migrateOutdatedIfidsInContext:childContext];
        [childContext safeSave];
    }];
}

- (void)startVerifyTimer {
    if (self.verifyTimer)
        [self.verifyTimer invalidate];
    self.verifyIsCancelled = NO;
    NSInteger minutes = [[NSUserDefaults standardUserDefaults] integerForKey:kRecheckFrequencyKey];
    // A zero or negative frequency would make the timer fire continuously
    NSTimeInterval seconds = (NSTimeInterval)MAX(minutes, 1) * 60;
    self.verifyTimer = [NSTimer scheduledTimerWithTimeInterval:seconds
                                                   target:self
                                                 selector:@selector(verifyInBackground:)
                                                 userInfo:nil
                                                  repeats:YES];
    self.verifyTimer.tolerance = 10;
}

- (void)stopVerifyTimer {
    [self.verifyTimer invalidate];
    self.verifyTimer = nil;
    self.verifyIsCancelled = YES;
}

- (void)lookForMissingFile:(Game *)game {
    if (!game)
        return;
    MissingFilesFinder *look = [MissingFilesFinder new];
    [look lookForMissingFile:game libController:self];
}

@end
