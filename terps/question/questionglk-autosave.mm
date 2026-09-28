/*
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* questionglk-autosave.mm

   Spatterlight autosave/autorestore for the Question terp -- the classic
   Quest 4 frontend (questionglk.cc) and the native Quest 5 frontend
   (aslxglk.cc) -- adapted from the Bocfel implementation
   (bocfel-spatterlight/), which is in turn adapted from Andrew Plotkin's
   IosGlk autosave design.

   At every top-level command prompt the engine's self-contained state
   serialization goes into

     ~/Library/Application Support/Spatterlight/Quest Files/Autosaves/(HASH)/autosave.glksave

   and the Glk library state into autosave.plist in the same directory.
   Both are written to temp names first and only then renamed into place,
   so a failed or interrupted write leaves the previous good pair intact;
   if the second rename fails, the first is rolled back.  (Only a crash
   in the instant between the two renames can still split the pair.)
   win_autosave() then tells the window server to snapshot the GUI under
   the same tag.
*/

extern "C" {
#include "glk.h"
#include "glkimp.h"
#include "fileref.h"
}

#include <string>

#include "QuestionRunner.hh"
#include "questionglk-autosave.h"

#import "TempLibrary.h"

extern "C" const char *storyfilename;   /* defined in questionglkterm.c */

/* ---- shared file plumbing ------------------------------------------------ */

static NSString *autosave_dirname(void)
{
    if (autosavedir == NULL)
        getautosavedir(const_cast<char *>(storyfilename));
    if (autosavedir == NULL)
        return nil;
    NSString *dirname = [[NSFileManager defaultManager]
        stringWithFileSystemRepresentation:autosavedir length:strlen(autosavedir)];
    if (!dirname.length)
        return nil;
    return dirname;
}

bool question_autosave_exists(void)
{
    if (!gli_enable_autosave)
        return false;
    @autoreleasepool {
        NSString *dirname = autosave_dirname();
        if (!dirname)
            return false;
        NSString *gamepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];
        NSString *libpath = [dirname stringByAppendingPathComponent:@"autosave.plist"];
        NSFileManager *fileManager = [NSFileManager defaultManager];
        if (![fileManager fileExistsAtPath:gamepath])
            return false;
        if (![fileManager fileExistsAtPath:libpath]) {
            /* A glksave with no plist can't be restored; delete it so it
             * does not cause trouble later. */
            [fileManager removeItemAtPath:gamepath error:nil];
            return false;
        }
        return true;
    }
}

bool question_autosave_wanted(void)
{
    if (!gli_enable_autosave)
        return false;
    /* Match Bocfel: no autosave before the first real event, after mere
     * rearrange/redraw wakeups, or on timer events when the timer pref is
     * off. */
    if ((int)lasteventtype == -1 || lasteventtype == evtype_Arrange ||
        lasteventtype == evtype_Redraw ||
        (lasteventtype == evtype_Timer && !gli_enable_autosave_on_timer))
        return false;
    return true;
}

void question_autosave_discard(void)
{
    @autoreleasepool {
        NSString *dirname = autosave_dirname();
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

    [fileManager removeItemAtPath:bakpath error:nil];
    [fileManager moveItemAtPath:finalpath toPath:bakpath error:nil];

    NSError *error = nil;
    if (![fileManager moveItemAtPath:tmppath toPath:finalpath error:&error]) {
        NSLog(@"question autosave: could not move %@ to final position: %@", tmpname, error);
        /* Put the old file back so the previous autosave stays usable. */
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

/* Write the engine state and the Glk library plist (with the given archive
 * hook appending engine-specific extras), then ask the window server to
 * snapshot the GUI under the same tag.  The per-prompt guards
 * (question_autosave_wanted) are the caller's job. */
static void write_autosave_pair(const std::string &engine_state,
                                void (*archive_hook)(TempLibrary *, NSCoder *))
{
    /* Unconditionally, NOT `if (autosavedir == NULL)`: getautosavedir resolves
     * the directory NAME without creating it, and the boot-time
     * question_autosave_exists / aslx autorestore probe calls it -- so autosavedir
     * is already non-null here and the guard skipped the one call that makes
     * the directory, leaving every write to fail with mktemp errno 2.  The
     * real app pre-creates the directory, which is why this stayed latent;
     * anything else driving the terp (test/glkdrive.py), or a user whose
     * autosave folder was removed, hits it immediately.
     * createDirectoryAtURL:withIntermediateDirectories:YES is idempotent. */
    create_autosavedir(const_cast<char *>(storyfilename));

    @autoreleasepool {
        NSString *dirname = autosave_dirname();
        if (!dirname) {
            win_showerror("Could not create autosave directory name.");
            return;
        }

        NSFileManager *fileManager = [NSFileManager defaultManager];
        NSString *tmpgamepath = [dirname stringByAppendingPathComponent:@"autosave-tmp.glksave"];
        NSString *tmplibpath = [dirname stringByAppendingPathComponent:@"autosave-tmp.plist"];

        /* 1. The game state, to its temp name. */
        NSData *gamedata = [NSData dataWithBytes:engine_state.data()
                                          length:engine_state.size()];
        NSError *error = nil;
        if (![gamedata writeToFile:tmpgamepath options:NSDataWritingAtomic error:&error]) {
            NSLog(@"question autosave: game state write failed: %@", error);
            return;
        }

        /* 2. The Glk library state, to its temp name. */
        TempLibrary *library = [[TempLibrary alloc] init];

        [TempLibrary setExtraArchiveHook:archive_hook];
        NSError *archiveError = nil;
        NSData *archiveData = [NSKeyedArchiver archivedDataWithRootObject:library
                                                    requiringSecureCoding:NO
                                                                    error:&archiveError];
        [TempLibrary setExtraArchiveHook:nil];

        if (!archiveData) {
            NSLog(@"question autosave: library serialize failed: %@", archiveError);
            [fileManager removeItemAtPath:tmpgamepath error:nil];
            return;
        }
        if (![archiveData writeToFile:tmplibpath options:NSDataWritingAtomic error:&archiveError]) {
            NSLog(@"question autosave: library write failed: %@", archiveError);
            [fileManager removeItemAtPath:tmpgamepath error:nil];
            return;
        }

        /* 3. Both written: rename them into place as a pair.  If the plist
         * cannot follow the glksave, take the glksave back too, so the
         * files on disk never mix two turns. */
        if (!move_into_place(dirname, @"autosave-tmp.glksave",
                             @"autosave.glksave", @"autosave-bak.glksave")) {
            [fileManager removeItemAtPath:tmplibpath error:nil];
            return;
        }
        if (!move_into_place(dirname, @"autosave-tmp.plist",
                             @"autosave.plist", @"autosave-bak.plist")) {
            roll_back(dirname, @"autosave.glksave", @"autosave-bak.glksave");
            [fileManager removeItemAtPath:tmplibpath error:nil];
            return;
        }

        /* 4. Have the window server snapshot the GUI under the same tag. */
        win_autosave(library.autosaveTag);
    }
}

/* Unarchive autosave.plist with the given hook and replace the live Glk
 * object lists.  Returns the library (retained in a static for the "late"
 * pass) or nil. */
static TempLibrary *pending_library = nil;

static TempLibrary *restore_library(void (*unarchive_hook)(TempLibrary *, NSCoder *))
{
    NSString *dirname = autosave_dirname();
    if (!dirname)
        return nil;
    NSString *libpath = [dirname stringByAppendingPathComponent:@"autosave.plist"];

    NSError *error = nil;
    TempLibrary *newlib = nil;
    NSData *libdata = [NSData dataWithContentsOfFile:libpath options:0 error:&error];
    if (libdata) {
        NSKeyedUnarchiver *unarchiver =
            [[NSKeyedUnarchiver alloc] initForReadingFromData:libdata error:&error];
        if (unarchiver) {
            unarchiver.requiresSecureCoding = NO;
            [TempLibrary setExtraUnarchiveHook:unarchive_hook];
            newlib = (TempLibrary *)[unarchiver decodeTopLevelObjectForKey:NSKeyedArchiveRootObjectKey
                                                                     error:&error];
            [TempLibrary setExtraUnarchiveHook:nil];
            [unarchiver finishDecoding];
        }
    }
    if (!newlib) {
        NSLog(@"question autorestore: could not restore library state: %@", error);
        return nil;
    }
    [newlib updateFromLibrary];
    return newlib;
}

/* ---- Quest 4 (questionglk.cc / QuestionRunner) --------------------------- */

static QuestionGlkFrontendState frontend_state;

/* The archive keys keep their pre-rename "geas_" prefix: they are read back
 * from autosaves already on disk. */
static void question_library_archive(TempLibrary *library, NSCoder *encoder)
{
    (void)library;
    [encoder encodeInt32:frontend_state.mainwintag forKey:@"geas_mainwintag"];
    [encoder encodeInt32:frontend_state.inputwintag forKey:@"geas_inputwintag"];
    [encoder encodeInt32:frontend_state.bannerwintag forKey:@"geas_bannerwintag"];
    [encoder encodeInt32:frontend_state.objwintag forKey:@"geas_objwintag"];
    [encoder encodeInt32:frontend_state.gfxwintag forKey:@"geas_gfxwintag"];
    [encoder encodeInt32:frontend_state.transcripttag forKey:@"geas_transcripttag"];
    [encoder encodeInt32:frontend_state.soundchanneltag forKey:@"geas_soundchanneltag"];
    [encoder encodeInt32:frontend_state.use_objpane forKey:@"geas_use_objpane"];
    [encoder encodeObject:@(frontend_state.objwin_expanded.c_str())
                   forKey:@"geas_objwin_expanded"];
    [encoder encodeInt32:frontend_state.rng_usenative forKey:@"geas_rng_usenative"];
    for (int i = 0; i < 4; i++)
        [encoder encodeInt32:(int32_t)frontend_state.rng_state[i]
                      forKey:[NSString stringWithFormat:@"geas_rng_state%d", i]];
}

static void question_library_unarchive(TempLibrary *library, NSCoder *decoder)
{
    (void)library;
    frontend_state.mainwintag = [decoder decodeInt32ForKey:@"geas_mainwintag"];
    frontend_state.inputwintag = [decoder decodeInt32ForKey:@"geas_inputwintag"];
    frontend_state.bannerwintag = [decoder decodeInt32ForKey:@"geas_bannerwintag"];
    frontend_state.objwintag = [decoder decodeInt32ForKey:@"geas_objwintag"];
    frontend_state.gfxwintag = [decoder decodeInt32ForKey:@"geas_gfxwintag"];
    frontend_state.transcripttag = [decoder decodeInt32ForKey:@"geas_transcripttag"];
    frontend_state.soundchanneltag = [decoder decodeInt32ForKey:@"geas_soundchanneltag"];
    frontend_state.use_objpane = [decoder decodeInt32ForKey:@"geas_use_objpane"];
    NSString *expanded = [decoder decodeObjectOfClass:[NSString class]
                                               forKey:@"geas_objwin_expanded"];
    frontend_state.objwin_expanded = expanded ? std::string(expanded.UTF8String)
                                              : std::string();
    frontend_state.rng_usenative =
        [decoder containsValueForKey:@"geas_rng_usenative"]
            ? [decoder decodeInt32ForKey:@"geas_rng_usenative"] : -1;
    for (int i = 0; i < 4; i++)
        frontend_state.rng_state[i] = (uint32_t)[decoder
            decodeInt32ForKey:[NSString stringWithFormat:@"geas_rng_state%d", i]];
}

/* The Quest 4 autosave.glksave is a small container: the engine's full
 * state serialization plus the undo history (so UNDO still works across an
 * autorestore, as Bocfel carries its save stacks in its autosave).  Both
 * parts are length-prefixed because the QUEST300 body reads to end-of-
 * buffer.  A file without the container magic is a bare engine state (an
 * autosave from before the container existed).  Being on disk, the magic
 * keeps its pre-rename "GEAS" prefix. */
static const char *const kQuestionContainerMagic = "GEASAUTO1\n";

static std::string container_wrap(const std::string &engine_state,
                                  const std::string &undo_history)
{
    std::string out = kQuestionContainerMagic;
    out += std::to_string(engine_state.size());
    out += '\n';
    out += engine_state;
    out += std::to_string(undo_history.size());
    out += '\n';
    out += undo_history;
    return out;
}

static bool container_split(const std::string &data, std::string *engine_state,
                            std::string *undo_history)
{
    const std::string magic = kQuestionContainerMagic;
    if (data.compare(0, magic.size(), magic) != 0) {
        *engine_state = data;   /* legacy: the whole file is the engine state */
        undo_history->clear();
        return true;
    }
    size_t pos = magic.size();
    for (std::string *part : { engine_state, undo_history }) {
        size_t eol = data.find('\n', pos);
        if (eol == std::string::npos)
            return false;
        unsigned long len = strtoul(data.c_str() + pos, nullptr, 10);
        pos = eol + 1;
        if (len > data.size() - pos)
            return false;
        part->assign(data, pos, len);
        pos += len;
    }
    return true;
}

void question_do_autosave(QuestionRunner *gr)
{
    if (!question_autosave_wanted())
        return;
    std::string data = gr->save_state(false);
    if (data.empty())
        return;
    question_stash_frontend_state(&frontend_state);
    write_autosave_pair(container_wrap(data, gr->save_undo_history()),
                        question_library_archive);
}

bool question_restore_autosave(QuestionRunner *gr)
{
    if (!gli_enable_autosave)
        return false;
    @autoreleasepool {
        NSString *dirname = autosave_dirname();
        if (!dirname)
            return false;
        NSString *gamepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];

        NSError *error = nil;
        NSData *gamedata = [NSData dataWithContentsOfFile:gamepath options:0 error:&error];
        if (!gamedata) {
            NSLog(@"question autorestore: could not read game state: %@", error);
            question_autosave_discard();
            return false;
        }

        std::string filedata((const char *)gamedata.bytes, (size_t)gamedata.length);
        std::string data, undo_history;
        if (!container_split(filedata, &data, &undo_history)) {
            NSLog(@"question autorestore: autosave container was malformed.");
            question_autosave_discard();
            return false;
        }
        if (!gr->load_state(data, false)) {
            NSLog(@"question autorestore: saved game state was not usable.");
            question_autosave_discard();
            return false;
        }
        /* After load_state: the undo snapshots reference the restored props
         * log.  A missing or unreadable history just means no UNDO past the
         * restore point. */
        if (!undo_history.empty() && !gr->load_undo_history(undo_history))
            NSLog(@"question autorestore: undo history was not usable (ignored).");

        TempLibrary *newlib = restore_library(question_library_unarchive);
        if (!newlib) {
            question_autosave_discard();
            return false;
        }
        question_recover_frontend_state(&frontend_state);
        [newlib updateFromLibraryLate];
    }
    return true;
}

/* ---- Quest 5 (aslxglk.cc / aslx Interp) ---------------------------------- */

/* The aslx frontend's state crosses the archive as an opaque blob it
 * encodes and decodes itself. */
static std::string aslx_frontend_blob;

static void aslx_library_archive(TempLibrary *library, NSCoder *encoder)
{
    (void)library;
    [encoder encodeObject:[NSData dataWithBytes:aslx_frontend_blob.data()
                                         length:aslx_frontend_blob.size()]
                   forKey:@"aslx_frontend_state"];
}

static void aslx_library_unarchive(TempLibrary *library, NSCoder *decoder)
{
    (void)library;
    NSData *blob = [decoder decodeObjectOfClass:[NSData class]
                                         forKey:@"aslx_frontend_state"];
    aslx_frontend_blob = blob ? std::string((const char *)blob.bytes, (size_t)blob.length)
                              : std::string();
}

void aslx_do_autosave_write(const std::string &engine_state,
                            const std::string &frontend_blob)
{
    aslx_frontend_blob = frontend_blob;
    write_autosave_pair(engine_state, aslx_library_archive);
    aslx_frontend_blob.clear();
}

bool aslx_autosave_read_game(std::string *out)
{
    @autoreleasepool {
        NSString *dirname = autosave_dirname();
        if (!dirname)
            return false;
        NSString *gamepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];
        NSError *error = nil;
        NSData *gamedata = [NSData dataWithContentsOfFile:gamepath options:0 error:&error];
        if (!gamedata) {
            NSLog(@"aslx autorestore: could not read game state: %@", error);
            return false;
        }
        out->assign((const char *)gamedata.bytes, (size_t)gamedata.length);
        return true;
    }
}

bool aslx_autosave_restore_library(std::string *frontend_blob_out)
{
    if (!gli_enable_autosave)
        return false;
    @autoreleasepool {
        aslx_frontend_blob.clear();
        TempLibrary *newlib = restore_library(aslx_library_unarchive);
        if (!newlib)
            return false;
        *frontend_blob_out = aslx_frontend_blob;
        aslx_frontend_blob.clear();
        pending_library = newlib;
    }
    return true;
}

void aslx_autosave_restore_library_late(void)
{
    @autoreleasepool {
        [pending_library updateFromLibraryLate];
        pending_library = nil;
    }
}
