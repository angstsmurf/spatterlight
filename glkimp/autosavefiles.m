/* autosavefiles.m -- see autosavefiles.h. */

#import <Foundation/Foundation.h>

#include <stdlib.h>
#include <string.h>

#include "glkimp.h"
#include "fileref.h"
#include "autosavefiles.h"

#import "TempLibrary.h"

static NSString *autosave_dirname(const char *gamepath)
{
    if (autosavedir == NULL)
        getautosavedir((char *)gamepath);
    if (autosavedir == NULL)
        return nil;
    NSString *dirname = [[NSFileManager defaultManager]
        stringWithFileSystemRepresentation:autosavedir length:strlen(autosavedir)];
    if (!dirname.length)
        return nil;
    return dirname;
}

bool gli_autosave_exists(const char *gamepath)
{
    if (!gli_enable_autosave)
        return false;
    @autoreleasepool {
        NSString *dirname = autosave_dirname(gamepath);
        if (!dirname)
            return false;
        NSString *gamefile = [dirname stringByAppendingPathComponent:@"autosave.glksave"];
        NSString *libfile = [dirname stringByAppendingPathComponent:@"autosave.plist"];
        NSFileManager *fileManager = [NSFileManager defaultManager];
        if (![fileManager fileExistsAtPath:gamefile])
            return false;
        if (![fileManager fileExistsAtPath:libfile]) {
            /* A glksave with no plist can't be restored; delete it so it
             * does not cause trouble later. */
            [fileManager removeItemAtPath:gamefile error:nil];
            return false;
        }
        return true;
    }
}

bool gli_autosave_wanted(void)
{
    if (!gli_enable_autosave)
        return false;
    if ((int)lasteventtype == -1 || lasteventtype == evtype_Arrange ||
        lasteventtype == evtype_Redraw ||
        (lasteventtype == evtype_Timer && !gli_enable_autosave_on_timer))
        return false;
    return true;
}

void gli_autosave_discard(const char *gamepath)
{
    @autoreleasepool {
        NSString *dirname = autosave_dirname(gamepath);
        if (!dirname)
            return;
        NSFileManager *fileManager = [NSFileManager defaultManager];
        for (NSString *name in @[ @"autosave.glksave", @"autosave.plist",
                                  @"autosave-bak.glksave", @"autosave-bak.plist" ])
            [fileManager removeItemAtPath:[dirname stringByAppendingPathComponent:name]
                                    error:nil];
    }
}

/* Move any current "final" file to its -bak name and the freshly written
 * temp file into the final position. */
static bool move_into_place(NSString *dirname, NSString *tmpname,
                            NSString *finalname, NSString *bakname)
{
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSString *tmppath = [dirname stringByAppendingPathComponent:tmpname];
    NSString *finalpath = [dirname stringByAppendingPathComponent:finalname];
    NSString *bakpath = [dirname stringByAppendingPathComponent:bakname];

    /* With no final file to back up, the replace below would leave a
     * stale -bak from an earlier turn for roll_back to find. */
    [fileManager removeItemAtPath:bakpath error:nil];

    /* The backup must outlive the call: roll_back needs it if the other
     * file of the pair cannot follow. */
    NSError *error = nil;
    if (![fileManager replaceItemAtURL:[NSURL fileURLWithPath:finalpath isDirectory:NO]
                         withItemAtURL:[NSURL fileURLWithPath:tmppath isDirectory:NO]
                        backupItemName:bakname
                               options:NSFileManagerItemReplacementWithoutDeletingBackupItem
                      resultingItemURL:nil
                                 error:&error]) {
        NSLog(@"autosave: could not move %@ to final position: %@", tmpname, error);
        /* Put the old file back so the previous autosave stays usable.
         * This fails harmlessly if the final file was never moved away. */
        [fileManager moveItemAtPath:bakpath toPath:finalpath error:nil];
        return false;
    }
    return true;
}

/* Undo a move_into_place that succeeded: put the -bak file back as the
 * final one, so the pair on disk is the previous turn's again. */
static void roll_back(NSString *dirname, NSString *finalname, NSString *bakname)
{
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSString *finalpath = [dirname stringByAppendingPathComponent:finalname];
    NSString *bakpath = [dirname stringByAppendingPathComponent:bakname];

    [fileManager removeItemAtPath:finalpath error:nil];
    [fileManager moveItemAtPath:bakpath toPath:finalpath error:nil];
}

bool gli_autosave_write(const char *gamepath, const void *data, size_t length,
                        gli_autosave_hook archive_hook)
{
    /* Unconditional: an earlier gli_autosave_exists() probe may already
     * have resolved the directory NAME (getautosavedir) without creating
     * the directory itself.  createDirectoryAtURL is a no-op when it
     * already exists. */
    create_autosavedir((char *)gamepath);

    @autoreleasepool {
        NSString *dirname = autosave_dirname(gamepath);
        if (!dirname) {
            win_showerror("Could not create autosave directory name.");
            return false;
        }

        /* 1. The game state, to its temp name. */
        NSString *tmpgamepath = [dirname stringByAppendingPathComponent:@"autosave-tmp.glksave"];
        NSData *gamedata = [NSData dataWithBytes:data length:length];
        NSError *error = nil;
        if (![gamedata writeToFile:tmpgamepath options:0 error:&error]) {
            NSLog(@"autosave: game state write failed: %@", error);
            [[NSFileManager defaultManager] removeItemAtPath:tmpgamepath error:nil];
            return false;
        }
    }
    return gli_autosave_commit(gamepath, archive_hook);
}

bool gli_autosave_commit(const char *gamepath, gli_autosave_hook archive_hook)
{
    @autoreleasepool {
        NSString *dirname = autosave_dirname(gamepath);
        if (!dirname) {
            win_showerror("Could not create autosave directory name.");
            return false;
        }

        NSFileManager *fileManager = [NSFileManager defaultManager];
        NSString *tmpgamepath = [dirname stringByAppendingPathComponent:@"autosave-tmp.glksave"];
        NSString *tmplibpath = [dirname stringByAppendingPathComponent:@"autosave-tmp.plist"];

        /* 2. The Glk library state, with the terp's extras appended, to
         * its temp name. */
        TempLibrary *library = [[TempLibrary alloc] init];

        NSError *error = nil;
        [TempLibrary setExtraArchiveHook:archive_hook];
        NSData *archiveData = [NSKeyedArchiver archivedDataWithRootObject:library
                                                    requiringSecureCoding:NO
                                                                    error:&error];
        [TempLibrary setExtraArchiveHook:NULL];

        if (!archiveData) {
            NSLog(@"autosave: library serialize failed: %@", error);
            [fileManager removeItemAtPath:tmpgamepath error:nil];
            return false;
        }
        if (![archiveData writeToFile:tmplibpath options:0 error:&error]) {
            NSLog(@"autosave: library write failed: %@", error);
            [fileManager removeItemAtPath:tmplibpath error:nil];
            [fileManager removeItemAtPath:tmpgamepath error:nil];
            return false;
        }

        /* 3. Both written: rename them into place as a pair.  If the plist
         * cannot follow the glksave, take the glksave back too, so the
         * files on disk never mix two turns. */
        if (!move_into_place(dirname, @"autosave-tmp.glksave",
                             @"autosave.glksave", @"autosave-bak.glksave")) {
            [fileManager removeItemAtPath:tmplibpath error:nil];
            return false;
        }
        if (!move_into_place(dirname, @"autosave-tmp.plist",
                             @"autosave.plist", @"autosave-bak.plist")) {
            roll_back(dirname, @"autosave.glksave", @"autosave-bak.glksave");
            [fileManager removeItemAtPath:tmplibpath error:nil];
            return false;
        }

        /* 4. Have the window server snapshot the GUI under the same tag. */
        win_autosave(library.autosaveTag);
    }
    return true;
}

void gli_autosave_discard_tmp(const char *gamepath)
{
    @autoreleasepool {
        NSString *dirname = autosave_dirname(gamepath);
        if (!dirname)
            return;
        [[NSFileManager defaultManager]
            removeItemAtPath:[dirname stringByAppendingPathComponent:@"autosave-tmp.glksave"]
                       error:nil];
    }
}

bool gli_autosave_read_game(const char *gamepath, void **data, size_t *length)
{
    @autoreleasepool {
        NSString *dirname = autosave_dirname(gamepath);
        if (!dirname)
            return false;
        NSString *gamefile = [dirname stringByAppendingPathComponent:@"autosave.glksave"];
        NSError *error = nil;
        NSData *gamedata = [NSData dataWithContentsOfFile:gamefile options:0 error:&error];
        if (!gamedata) {
            NSLog(@"autorestore: could not read game state: %@", error);
            return false;
        }
        /* malloc(0) may return NULL; never hand that back as success. */
        void *buffer = malloc(gamedata.length ? gamedata.length : 1);
        if (!buffer)
            return false;
        memcpy(buffer, gamedata.bytes, gamedata.length);
        *data = buffer;
        *length = gamedata.length;
        return true;
    }
}

TempLibrary *gli_autosave_load_library(const char *gamepath,
                                       gli_autosave_hook unarchive_hook)
{
    if (!gli_enable_autosave)
        return nil;
    NSString *dirname = autosave_dirname(gamepath);
    if (!dirname)
        return nil;
    NSString *libfile = [dirname stringByAppendingPathComponent:@"autosave.plist"];

    NSError *error = nil;
    TempLibrary *newlib = nil;
    NSData *libdata = [NSData dataWithContentsOfFile:libfile options:0 error:&error];
    if (libdata) {
        NSKeyedUnarchiver *unarchiver =
            [[NSKeyedUnarchiver alloc] initForReadingFromData:libdata error:&error];
        if (unarchiver) {
            unarchiver.requiresSecureCoding = NO;
            [TempLibrary setExtraUnarchiveHook:unarchive_hook];
            newlib = (TempLibrary *)[unarchiver decodeTopLevelObjectForKey:NSKeyedArchiveRootObjectKey
                                                                     error:&error];
            [TempLibrary setExtraUnarchiveHook:NULL];
            [unarchiver finishDecoding];
        }
    }
    if (!newlib)
        NSLog(@"autorestore: could not restore library state: %@", error);
    return newlib;
}
