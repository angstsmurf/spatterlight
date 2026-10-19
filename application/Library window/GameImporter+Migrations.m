//
//  GameImporter+Migrations.m
//  Spatterlight
//
//  One-time repairs of libraries made by earlier versions, run at launch.
//  Split out of GameImporter.m.
//

#import "GameImporter.h"

#import "Game.h"
#import "Metadata.h"
#import "Ifid.h"
#import "Fetches.h"
#import "FolderAccess.h"
#import "LibraryOrganizer.h"
#import "NSString+Categories.h"
#import "TableViewController.h"

NSString *const kAGTConvertedToDirectMigrationDone = @"AGTConvertedToDirectMigrationDone";
NSString *const kAGTMigratedHashRepairDone = @"AGTMigratedHashRepairDone";
NSString *const kOutdatedIfidMigrationDone = @"OutdatedIfidMigrationDone";

// Runs `block` with the path of the game's file while access to its folder is
// granted. Does not run it if the file can't be read.
static void WithReadableGameFile(Game *game, void (^block)(NSString *path)) {
    NSURL *url = game.urlForBookmark;
    NSString *path = url ? url.path : game.path;
    if (!path.length)
        return;
    NSURL *granted = [FolderAccess grantAccessToFile:[NSURL fileURLWithPath:path isDirectory:NO]];
    if ([NSFileManager.defaultManager isReadableFileAtPath:path])
        block(path);
    if (granted)
        [FolderAccess releaseBookmark:granted];
}

@implementation GameImporter (Migrations)

- (void)migrateConvertedAGTGamesInContext:(NSManagedObjectContext *)context {
    NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
    if ([defaults boolForKey:kAGTConvertedToDirectMigrationDone])
        return;

    NSFileManager *fm = NSFileManager.defaultManager;

    // Path of our container's Converted folder, used to recognise (and only
    // ever delete within) the old converted .agx files.
    NSString *convertedPath =
    [self.tableViewController.homepath URLByAppendingPathComponent:@"Converted" isDirectory:YES].absoluteURL.path;

    // AGT games still stored as a converted .agx (direct ones are .d$$).
    NSArray<Game *> *agtGames =
    [Fetches fetchObjects:@"Game" predicate:@"detectedFormat == \"agt\"" inContext:context];

    NSMutableArray<Game *> *candidates = [NSMutableArray array];
    for (Game *game in agtGames) {
        if ([game.path.pathExtension.lowercaseString isEqualToString:@"agx"])
            [candidates addObject:game];
    }

    if (!candidates.count) {
        [defaults setBool:YES forKey:kAGTConvertedToDirectMigrationDone];
        return;
    }

    // Build a map of IFID -> original .D$$ by scanning every folder we still
    // hold access to (these include the folders the games were imported from).
    NSMutableDictionary<NSString *, NSURL *> *ifidToDss = [NSMutableDictionary dictionary];
    for (NSURL *folder in [FolderAccess bookmarkedFolders]) {
        // A stale bookmark is stored again under the folder's new URL, so
        // access must be released through the URL restoreURL: returns
        NSURL *accessURL = [FolderAccess restoreURL:folder];
        if (!accessURL)
            continue;
        NSArray<NSURL *> *contents =
        [fm contentsOfDirectoryAtURL:accessURL
          includingPropertiesForKeys:nil
                             options:NSDirectoryEnumerationSkipsHiddenFiles
                               error:nil];
        for (NSURL *fileURL in contents) {
            if (![fileURL.pathExtension.lowercaseString isEqualToString:@"d$$"])
                continue;
            NSString *foundIfid = [GameImporter identifyAGTFile:fileURL.path];
            if (foundIfid && !ifidToDss[foundIfid])
                ifidToDss[foundIfid] = fileURL;
        }
        [FolderAccess releaseBookmark:accessURL];
    }

    for (Game *game in candidates) {
        NSURL *dssURL = ifidToDss[game.ifid];
        if (!dssURL || ![fm isReadableFileAtPath:dssURL.path])
            continue;  // original not found; leave this game on the AGX path

        // The library knows a game by the signature of its file, and this
        // game is about to change file. If the original was added on its own
        // in the meantime, leave this entry on the AGX path rather than end
        // up with two entries for one file.
        NSString *hash = dssURL.path.signatureFromFile;
        Game *existing = hash.length ? [Fetches fetchGameForHash:hash inContext:context] : nil;
        if (existing && existing != game)
            continue;

        NSString *oldAGX = game.path;
        [game bookmarkForPath:dssURL.path];
        game.found = YES;
        if (hash.length) {
            game.hashTag = hash;
            game.metadata.hashTag = hash;
        }

        // Remove the now-orphaned converted file (and its copied icon), but
        // only when it really lives inside our own container.
        if (convertedPath.length && [oldAGX hasPrefix:convertedPath]) {
            [fm removeItemAtPath:oldAGX error:nil];
            NSString *ico = [oldAGX.stringByDeletingPathExtension
                             stringByAppendingPathExtension:@"ico"];
            [fm removeItemAtPath:ico error:nil];
        }

        NSLog(@"AGT migration: re-pointed \"%@\" from %@ to %@",
              game.metadata.title, oldAGX.lastPathComponent, dssURL.path);
    }

    [defaults setBool:YES forKey:kAGTConvertedToDirectMigrationDone];
}

// The migration above used to re-point a game at its original .D$$ and leave
// its hash as it was: the signature of the converted .agx it had just deleted.
// The library finds games by hash, so such an entry was not recognised when
// its own file was added again, and a duplicate was created. This gives every
// AGT game played from a .D$$ the signature of that file.
- (void)repairHashesOfMigratedAGTGamesInContext:(NSManagedObjectContext *)context {
    NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
    if ([defaults boolForKey:kAGTMigratedHashRepairDone])
        return;

    NSArray<Game *> *agtGames =
    [Fetches fetchObjects:@"Game" predicate:@"detectedFormat == \"agt\"" inContext:context];

    // Games whose file we could not read this time. While there are any, the
    // repair runs again at next launch.
    NSUInteger unresolved = 0;

    for (Game *game in agtGames) {
        if (game.isDeleted ||
            ![game.path.pathExtension.lowercaseString isEqualToString:@"d$$"])
            continue;

        NSString __block *hash = nil;
        WithReadableGameFile(game, ^(NSString *path) {
            hash = path.signatureFromFile;
        });

        if (!hash.length) {
            unresolved++;
            continue;
        }
        if ([hash isEqualToString:game.hashTag])
            continue;

        // The duplicate has already been made. Leave both entries alone.
        Game *existing = [Fetches fetchGameForHash:hash inContext:context];
        if (existing && existing != game)
            continue;

        game.hashTag = hash;
        game.metadata.hashTag = hash;
    }

    if (!unresolved)
        [defaults setBool:YES forKey:kAGTMigratedHashRepairDone];
}

#pragma mark - IFID migration

// YES if `ifid` has a form babel no longer produces for a game of `format`:
// Quest files without an id of their own used to get the bare MD5 of the file
// (now QUEST-<MD5>), and the UUID:// marker of a Z-code or Glulx file used to
// be copied as found, up to the first slash, whatever it contained.
static BOOL IfidLooksOutdated(NSString *ifid, NSString *format) {
    static NSCharacterSet *notHex, *notIfid;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        notHex = [NSCharacterSet characterSetWithCharactersInString:
                  @"0123456789ABCDEFabcdef"].invertedSet;
        notIfid = [NSCharacterSet characterSetWithCharactersInString:
                   @"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-"].invertedSet;
    });

    if ([format hasPrefix:@"quest"])
        return ifid.length == 32 &&
            [ifid rangeOfCharacterFromSet:notHex].location == NSNotFound;

    // IFIDs built from the file header may hold a serial with small letters
    if ([ifid hasPrefix:@"ZCODE-"] || [ifid hasPrefix:@"GLULX-"])
        return NO;
    return [ifid rangeOfCharacterFromSet:notIfid].location != NSNotFound;
}

// What an outdated IFID turns into when the game file can't be read: the
// change babel would make to it if the file is still the one we imported.
// Returns nil when that can't be told without the file.
static NSString *UpdatedIfidWithoutFile(NSString *ifid, NSString *format) {
    if ([format hasPrefix:@"quest"])
        return [@"QUEST-" stringByAppendingString:ifid.uppercaseString];
    NSString *upper = ifid.uppercaseString;
    return IfidLooksOutdated(upper, format) ? nil : upper;
}

- (void)migrateOutdatedIfidsInContext:(NSManagedObjectContext *)context {
    NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
    if ([defaults boolForKey:kOutdatedIfidMigrationDone])
        return;

    NSArray<Game *> *games =
    [Fetches fetchObjects:@"Game"
                predicate:@"detectedFormat IN {\"quest4\", \"quest5\", \"zcode\", \"glulx\"}"
                inContext:context];

    // Games we could not settle this time. While there are any, the migration
    // runs again at next launch.
    NSUInteger unresolved = 0;

    for (Game *game in games) {
        NSString *oldIfid = game.ifid;
        if (game.isDeleted || !oldIfid.length ||
            !IfidLooksOutdated(oldIfid, game.detectedFormat))
            continue;

        // Ask babel, if we can get at the file.
        NSString __block *newIfid = nil;
        WithReadableGameFile(game, ^(NSString *path) {
            NSString *ifid = nil;
            [GameImporter identifyFile:path format:NULL ifid:&ifid];
            newIfid = ifid;
        });

        if (!newIfid)
            newIfid = UpdatedIfidWithoutFile(oldIfid, game.detectedFormat);
        if (!newIfid) {
            unresolved++;
            continue;
        }
        if ([newIfid isEqualToString:oldIfid])
            continue;

        game.ifid = newIfid;

        // The metadata's IFID record follows, so that matching imported
        // iFiction and IFDB lookups use the new one.
        Metadata *metadata = game.metadata;
        Ifid *oldRecord = nil;
        BOOL hasNew = NO;
        for (Ifid *record in metadata.ifids) {
            if ([record.ifidString isEqualToString:newIfid])
                hasNew = YES;
            else if ([record.ifidString caseInsensitiveCompare:oldIfid] == NSOrderedSame)
                oldRecord = record;
        }
        if (oldRecord && hasNew) {
            [metadata removeIfidsObject:oldRecord];
            [context deleteObject:oldRecord];
        } else if (oldRecord) {
            oldRecord.ifidString = newIfid;
        } else if (!hasNew) {
            [metadata createIfid:newIfid];
        }

        [[LibraryOrganizer sharedOrganizer] updateIdentityMarkerForGame:game
                                                       previousIdentity:oldIfid];

        NSLog(@"IFID migration: \"%@\" %@ -> %@", metadata.title, oldIfid, newIfid);
    }

    if (!unresolved)
        [defaults setBool:YES forKey:kOutdatedIfidMigrationDone];
}

@end
