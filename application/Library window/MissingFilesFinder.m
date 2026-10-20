//
//  MissingFilesFinder.m
//  Spatterlight
//
//  Created by Administrator on 2021-05-22.
//

#import "Game.h"
#import "Metadata.h"
#import "NSString+Categories.h"
#import "TableViewController.h"
#import "GameLauncher.h"
#import "GameImporter.h"

#import "FolderAccess.h"
#import "Constants.h"

#import "MissingFilesFinder.h"
#import "NSAlert+Spatterlight.h"


extern NSArray *gGameFileTypes;

@implementation MissingFilesFinder

//! Looks for the old file under an ancestor folder it shares with the folder
//! the user found a game in. Starting from the file's innermost folder, each
//! folder name is looked up among the components of @c directory; if the file
//! exists at the same relative path below the matching folder, that path is
//! returned. Folders are compared by whole name, so "Games" does not match
//! "MyGames".
+ (nullable NSString *)pathForFile:(NSString *)oldPath underCommonAncestorWith:(NSString *)directory {
    NSArray<NSString *> *fileComponents = oldPath.pathComponents;
    NSArray<NSString *> *directoryComponents = directory.pathComponents;
    NSFileManager *fileManager = [NSFileManager defaultManager];

    // The last component is the file name itself. Index 0 is the root "/".
    for (NSInteger i = (NSInteger)fileComponents.count - 2; i > 0; i--) {
        NSString *component = fileComponents[(NSUInteger)i];
        NSUInteger j = [directoryComponents indexOfObject:component];
        if (j == NSNotFound || j == 0)
            continue;

        NSArray<NSString *> *beginning = [directoryComponents subarrayWithRange:NSMakeRange(0, j)];
        NSArray<NSString *> *end = [fileComponents subarrayWithRange:NSMakeRange((NSUInteger)i, fileComponents.count - (NSUInteger)i)];
        NSString *newPath = [NSString pathWithComponents:[beginning arrayByAddingObjectsFromArray:end]];

        if ([fileManager fileExistsAtPath:newPath])
            return newPath;
    }
    return nil;
}

- (void)lookForMissingFile:(Game *)game libController:(TableViewController *)libController {
    NSAlert *alert = [NSAlert alertWithMessage:NSLocalizedString(@"Cannot find the file.", nil)
                                          info:[NSString stringWithFormat: NSLocalizedString(@"The game file for \"%@\" could not be found at its original location. Do you want to look for it?", nil), game.metadata.title]
                                       buttons:@[ NSLocalizedString(@"Look for game", nil), NSLocalizedString(@"Cancel", nil), NSLocalizedString(@"Delete game", nil) ]];

    NSModalResponse choice = [alert runModal];

    if (choice == NSAlertFirstButtonReturn) {
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        [panel setAllowsMultipleSelection:NO];
        [panel setCanChooseDirectories:NO];
        panel.prompt = NSLocalizedString(@"Open", nil);

        NSDictionary *values = [NSURL resourceValuesForKeys:@[NSURLPathKey]
                                           fromBookmarkData:(NSData *)game.fileLocation];

        NSString *path = values[NSURLPathKey];

        if (!path)
            path = game.path;

        NSString *extension = path.pathExtension;
        if (extension)
            panel.allowedFileTypes = @[extension];

        NSModalResponse result = [panel runModal];
        if (result == NSModalResponseOK) {
            NSString *newPath = ((NSURL *)panel.URLs[0]).path;
            NSString *hash = newPath.signatureFromFile;
            if (hash.length) {
                if ([hash isEqualToString:game.hashTag]) {
                    [game bookmarkForPath:newPath];
                    game.found = YES;
                    NSString *parentDirectory = newPath.stringByDeletingLastPathComponent;
                    [self lookForMoreMissingFilesInFolder:parentDirectory context:game.managedObjectContext];
                } else {
                    NSAlert *anAlert = [NSAlert alertWithMessage:NSLocalizedString(@"Not a match.", nil)
                                                            info:[NSString stringWithFormat:NSLocalizedString(@"This file does not match the game \"%@.\"\nDo you want to replace it anyway?", nil), game.metadata.title]
                                                         buttons:@[ NSLocalizedString(@"Yes", nil), NSLocalizedString(@"Cancel", nil) ]];
                    anAlert.alertStyle = NSAlertStyleInformational;
                    NSInteger response = [anAlert runModal];
                    if (response == NSAlertFirstButtonReturn) {
                        if ([libController.gameLauncher importGame:newPath inContext:game.managedObjectContext reportFailure:YES hide:NO]) {
                            [game.managedObjectContext deleteObject:game];
                        }
                    }
                }
            }
        }
    } else if (choice == NSAlertThirdButtonReturn) {
        [game.managedObjectContext deleteObject:game];
    }
}

- (void)lookForMoreMissingFilesInFolder:(NSString *)directory context:(NSManagedObjectContext *)context {

    NSError *error = nil;
    NSArray *fetchedObjects;

    NSFetchRequest *fetchRequest = [Game fetchRequest];

    // We check all files in the directory, to see if they may
    // be other missing game files in our library

    // First we check all game files in directory, to see if they match
    // file names of games in library. This will automatically set the
    // found attribute of the corresponding game entities to appropriate values.
    NSURL *dirUrl = [NSURL fileURLWithPath:directory isDirectory:YES];
    NSArray *contentsOfDir = [[NSFileManager defaultManager] contentsOfDirectoryAtURL:dirUrl
                               includingPropertiesForKeys:@[NSURLNameKey]
                                                  options:NSDirectoryEnumerationSkipsHiddenFiles
                                                    error:nil];
    NSMutableArray<NSPredicate *> *filenamePredicates = [NSMutableArray new];
    for (NSURL *url in contentsOfDir) {
        NSString *name = nil;
        [url getResourceValue:&name forKey:NSURLNameKey error:nil];
        NSString *extension = name.pathExtension.lowercaseString;
        if (extension.length && [gGameFileTypes indexOfObject:extension] != NSNotFound) {
            NSString *filename = url.lastPathComponent;
            if (filename.length)
                [filenamePredicates addObject:[NSPredicate predicateWithFormat:@"path ENDSWITH[c] %@", filename]];
        }
    }

    // One fetch per batch of file names rather than one per file. The
    // batches keep the SQL expression well below SQLite's depth limit.
    const NSUInteger batchSize = 100;
    for (NSUInteger start = 0; start < filenamePredicates.count; start += batchSize) {
        NSRange range = NSMakeRange(start, MIN(batchSize, filenamePredicates.count - start));
        fetchRequest.predicate = [NSCompoundPredicate orPredicateWithSubpredicates:[filenamePredicates subarrayWithRange:range]];
        fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
        for (Game *game in fetchedObjects) {
            [game urlForBookmark];
        }
    }

    // Then we check all files with paths that contain adjacent directory names
    // This is mainly to catch all games in a downloaded IF-Comp directory tree
    NSArray<NSString *> *directoryComponents = directory.pathComponents;

    NSUInteger componentIndex = directoryComponents.count - 1;
    if (directoryComponents.count > 7) {
        componentIndex = directoryComponents.count - 3;
    } else if (directoryComponents.count > 3) {
        componentIndex = directoryComponents.count - 2;
    }
    NSString *commonDirectory = directoryComponents[componentIndex];

    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"path CONTAINS[cd] %@", commonDirectory];

    fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    for (Game *game in fetchedObjects) {
        [game urlForBookmark];
    }

    // The we get all missing files in library and check if they can be found here
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"found == NO"];

    fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"lookForMoreMissingFilesInFolder: %@",error);
    }

    if (fetchedObjects.count == 0) {
        return; //Found no missing files in library.
    }
    NSMutableDictionary<NSString *, Game *> *filenames = [[NSMutableDictionary alloc] initWithCapacity:fetchedObjects.count];

    for (Game *game in fetchedObjects) {
        NSDictionary *values = [NSURL resourceValuesForKeys:@[NSURLPathKey]
                             fromBookmarkData:(NSData *)game.fileLocation];
        NSString *filename = values[NSURLPathKey];
        if (!filename)
            filename = game.path;

        NSString *searchPath = [directory stringByAppendingPathComponent:filename.lastPathComponent];
        if ([[NSFileManager defaultManager] fileExistsAtPath:searchPath]) {
            filenames[searchPath] = game;
        } else {
            // If the missing game is not in the current directory, we check for a
            // common ancestor directory
            NSString *newPath = [MissingFilesFinder pathForFile:filename underCommonAncestorWith:directory];
            if (newPath)
                filenames[newPath] = game;
        }
    }

    if (!filenames.count)
        return;

    NSArray<NSString *> *matchedPaths = [filenames.allKeys sortedArrayUsingComparator:
                  ^NSComparisonResult(NSString * obj1, NSString * obj2){
        NSUInteger l1 = ((NSString *)obj1).length;
        NSUInteger l2 = ((NSString *)obj2).length;
        if (l1 > l2) {
            return (NSComparisonResult)NSOrderedDescending;
        }
        if (l1 < l2) {
            return (NSComparisonResult)NSOrderedAscending;
        }
        return (NSComparisonResult)NSOrderedSame;
    }];

    [FolderAccess askForAccessToURL:[NSURL fileURLWithPath:matchedPaths.firstObject] andThenRunBlock:^{
        // A file is only the missing game if it has the same signature, just
        // like the file the user picked by hand. An IFID is not enough: other
        // releases and other platform versions of a game share one. Entries
        // that never got a signature are matched by IFID and given one below.
        for (NSString *path in matchedPaths) {
            Game *candidate = filenames[path];
            BOOL match;
            if (candidate.hashTag.length)
                match = [path.signatureFromFile isEqualToString:candidate.hashTag];
            else {
                NSString *ifid = nil;
                [GameImporter identifyFile:path format:NULL ifid:&ifid];
                match = [ifid isEqualToString:candidate.ifid];
            }
            if (!match)
                filenames[path] = nil;
        }

        NSArray *matchedIfids = filenames.allValues;

        if (filenames.count > 0) {
            NSAlert *alert = [NSAlert alertWithMessage:[NSString stringWithFormat:NSLocalizedString(@"%@ %@ also in this folder.", nil), [NSString stringWithSummaryOfGames:matchedIfids], (matchedIfids.count > 1) ? NSLocalizedString(@"are", nil) : NSLocalizedString(@"is", nil)]
                                                  info:[NSString stringWithFormat:NSLocalizedString(@"Do you want to update %@ as well?", nil), (matchedIfids.count > 1) ? @"them" : @"it"]
                                               buttons:@[NSLocalizedString(@"Yes", nil), NSLocalizedString(@"Cancel", nil)]];
            alert.alertStyle = NSAlertStyleInformational;

            NSModalResponse choice = [alert runModal];
            if (choice == NSAlertFirstButtonReturn) {
                for (NSString *path in filenames) {
                    Game *game = filenames[path];
                    NSLog(@"Updating game %@ with new path %@", game.metadata.title, path);
                    [game bookmarkForPath:path];
                    game.found = YES;
                    if (!game.hashTag.length) {
                        game.hashTag = path.signatureFromFile;
                        game.metadata.hashTag = game.hashTag;
                    }
                }
            }
        }
    }];
}

@end
