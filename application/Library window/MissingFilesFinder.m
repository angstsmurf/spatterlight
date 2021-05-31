//
//  LookForMissingFiles.m
//  Spatterlight
//
//  Created by Administrator on 2021-05-22.
//

#import "Game.h"
#import "Metadata.h"
#import "NSString+Categories.h"
#import "LibController.h"

#import "MissingFilesFinder.h"

#include "babel_handler.h"

extern NSArray *gGameFileTypes;

@implementation MissingFilesFinder

- (void)lookForMissingFile:(Game *)game libController:(LibController *)libController {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = NSLocalizedString(@"Cannot find the file.", nil);
    alert.informativeText = [NSString stringWithFormat: NSLocalizedString(@"The game file for \"%@\" could not be found at its original location. Do you want to look for it?", nil), game.metadata.title];
    [alert addButtonWithTitle:NSLocalizedString(@"Yes", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

    NSModalResponse choice = [alert runModal];

    if (choice == NSAlertFirstButtonReturn) {
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        [panel setAllowsMultipleSelection:NO];
        [panel setCanChooseDirectories:NO];
        panel.prompt = NSLocalizedString(@"Open", nil);

        NSDictionary *values = [NSURL resourceValuesForKeys:@[NSURLPathKey]
                                           fromBookmarkData:(NSData *)game.fileLocation];

        NSString *path = [values objectForKey:NSURLPathKey];

        if (!path)
            path = game.path;

        NSString *extension = path.pathExtension;
        if (extension)
            panel.allowedFileTypes = @[extension];

        NSModalResponse result = [panel runModal];
        if (result == NSModalResponseOK) {
            NSString *newPath = ((NSURL *)panel.URLs[0]).path;
            NSString *ifid = [self ifidFromFile:newPath];
            if (ifid && [ifid isEqualToString:game.ifid]) {
                [game bookmarkForPath:newPath];
                game.found = YES;
                [self lookForMoreMissingFilesInFolder:newPath.stringByDeletingLastPathComponent context:game.managedObjectContext];
            } else {
                if (ifid) {
                    NSAlert *anAlert = [[NSAlert alloc] init];
                    anAlert.alertStyle = NSInformationalAlertStyle;
                    anAlert.messageText = NSLocalizedString(@"Not a match.", nil);
                    anAlert.informativeText = [NSString stringWithFormat:NSLocalizedString(@"This file does not match the game \"%@.\"\nDo you want to replace it anyway?", nil), game.metadata.title];
                    [anAlert addButtonWithTitle:NSLocalizedString(@"Yes", nil)];
                    [anAlert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
                    NSInteger response = [anAlert runModal];
                    if (response == NSAlertFirstButtonReturn) {

                        if ([libController importGame:newPath inContext:game.managedObjectContext reportFailure:YES]) {
                            [game.managedObjectContext deleteObject:game];
                        }
                    }

                } else {
                    NSAlert *anAlert = [[NSAlert alloc] init];
                    anAlert.messageText = NSLocalizedString(@"Not a match.", nil);
                    anAlert.informativeText = [NSString stringWithFormat:NSLocalizedString(@"This file does not match the game \"%@.", nil), game.metadata.title];
                    [anAlert runModal];
                }
            }
        }
    }
}

- (NSString *)ifidFromFile:(NSString *)path {
    void *context = get_babel_ctx();
    char *format = babel_init_ctx((char*)path.UTF8String, context);
    if (!format || !babel_get_authoritative_ctx(context))
    {
        NSAlert *anAlert = [[NSAlert alloc] init];
        anAlert.messageText = NSLocalizedString(@"Unknown file format.", nil);
        anAlert.informativeText = NSLocalizedString(@"Babel can not identify the file format.", nil);
        [anAlert runModal];
        babel_release_ctx(context);
        return nil;
    }

    char buf[TREATY_MINIMUM_EXTENT];

    int rv = babel_treaty_ctx(GET_STORY_FILE_IFID_SEL, buf, sizeof buf, context);
    if (rv <= 0)
    {
        NSAlert *anAlert = [[NSAlert alloc] init];
        anAlert.messageText = NSLocalizedString(@"Fatal error.", nil);
        anAlert.informativeText = NSLocalizedString(@"Can not compute IFID from the file.", nil);
        [anAlert runModal];
        babel_release_ctx(context);
        return nil;
    }

    babel_release_ctx(context);
    return @(buf);
}

- (void)lookForMoreMissingFilesInFolder:(NSString *)directory context:(NSManagedObjectContext *)context {

    NSError *error = nil;
    NSArray *fetchedObjects;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];

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
    for (NSURL *url in contentsOfDir) {
        NSString *name = nil;
        [url getResourceValue:&name forKey:NSURLNameKey error:nil];
        NSString *extension = name.pathExtension;
        if (extension.length && [gGameFileTypes indexOfObject:extension] != NSNotFound) {
            NSString *ifid = [self ifidFromFile:url.path];
            if (ifid.length) {
                fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:context];
                fetchRequest.predicate = [NSPredicate predicateWithFormat:@"ifid like[c] %@",ifid];
                fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
                for (Game *game in fetchedObjects) {
                    [game urlForBookmark];
                }
            }
        }
    }

    // Then we check all files with paths that contain adjacent directory names
    // This is mainly to catch all games in a downloaded IF-Comp directory tree
    NSArray<NSString *> *directoryComponents = [directory pathComponents];

    NSUInteger componentIndex = directoryComponents.count - 1;
    if (directoryComponents.count > 7) {
        componentIndex = directoryComponents.count - 3;
    } else if (directoryComponents.count > 3) {
        componentIndex = directoryComponents.count - 2;
    }
    NSString *commonDirectory = directoryComponents[componentIndex];


    fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:context];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"path CONTAINS[cd] %@", commonDirectory];

    fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    for (Game *game in fetchedObjects) {
        [game urlForBookmark];
    }

    // The we get all missing files in library and check if they can be found here
    fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:context];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"found == NO"];

    fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"lookForMoreMissingFilesInFolder: %@",error);
    }

    if (fetchedObjects.count == 0) {
        return; //Found no missing files in library.
    }
    NSMutableDictionary *filenames = [[NSMutableDictionary alloc] initWithCapacity:fetchedObjects.count];
    NSDictionary *values;

    NSString *filename;
    for (Game *game in fetchedObjects) {
        values = [NSURL resourceValuesForKeys:@[NSURLPathKey]
                             fromBookmarkData:(NSData *)game.fileLocation];
        filename = [values objectForKey:NSURLPathKey];
        if (!filename)
            filename = game.path;

        NSString *searchPath = [directory stringByAppendingPathComponent:filename.lastPathComponent];
        if ([[NSFileManager defaultManager] fileExistsAtPath:searchPath] && [[self ifidFromFile:searchPath] isEqualToString:game.ifid]) {
            [filenames setObject:game forKey:searchPath];
        } else {
            // If the missing game is not in the current directory, we check for a
            // common ancestor directory
            NSArray<NSString *> *pathComponents = [filename.stringByDeletingLastPathComponent pathComponents];

            NSEnumerator *enumerator = [pathComponents reverseObjectEnumerator];
            NSString *component;
            NSRange foundRange, rangeInFileName;
            while (component = [enumerator nextObject]) {
                foundRange = [directory rangeOfString:component];
                if (foundRange.location == NSNotFound || foundRange.location == 0)
                    continue;

                rangeInFileName = [filename rangeOfString:component];
                if (rangeInFileName.location == NSNotFound)
                    continue;

                NSString *newPathBeginning = [directory substringToIndex:foundRange.location - 1];
                NSString *newPathEnd = [filename substringFromIndex:rangeInFileName.location];
                NSString *newPath = [newPathBeginning stringByAppendingPathComponent:newPathEnd];

                if ([[NSFileManager defaultManager] fileExistsAtPath:newPath] &&
                    [[self ifidFromFile:newPath] isEqualToString:game.ifid]) {
                    [filenames setObject:game forKey:newPath];
                    break;
                }
            }
        }
    }

    fetchedObjects = [filenames allValues];

    if (filenames.count > 0) {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.alertStyle = NSInformationalAlertStyle;
        alert.messageText = [NSString stringWithFormat:NSLocalizedString(@"%@ %@ also in this folder.", nil), [NSString stringWithSummaryOf:fetchedObjects], (fetchedObjects.count > 1) ? NSLocalizedString(@"are", nil) : NSLocalizedString(@"is", nil)];
        alert.informativeText = [NSString stringWithFormat:NSLocalizedString(@"Do you want to update %@ as well?", nil), (fetchedObjects.count > 1) ? @"them" : @"it"];
        [alert addButtonWithTitle:NSLocalizedString(@"Yes", nil)];
        [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

        NSModalResponse choice = [alert runModal];
        if (choice == NSAlertFirstButtonReturn) {
            for (filename in filenames) {
                Game *game = [filenames objectForKey:filename];
                NSLog(@"Updating game %@ with new path %@", game.metadata.title, filename);
                [game bookmarkForPath:filename];
                game.found = YES;
            }
        }
    }
}

@end
