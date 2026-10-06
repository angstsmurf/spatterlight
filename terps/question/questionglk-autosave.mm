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

   The files themselves -- writing the pair, renaming it into place,
   reading it back -- are glkimp's (autosavefiles.m), shared with the other
   terps.  This file adds the plist hooks that carry each frontend's state,
   the Quest 4 container and its replay record.
*/

extern "C" {
#include "glk.h"
#include "glkimp.h"
}

#include <cstdlib>
#include <cstring>
#include <string>

#include "autosavefiles.h"

#include "QuestionRunner.hh"
#include "questionglk-autosave.h"

#import "TempLibrary.h"

extern "C" const char *storyfilename;   /* defined in questionglkterm.c */

/* ---- file plumbing: glkimp/autosavefiles.m ------------------------------- */

bool question_autosave_exists(void)
{
    return gli_autosave_exists(storyfilename);
}

bool question_autosave_wanted(void)
{
    return gli_autosave_wanted();
}

void question_autosave_discard(void)
{
    gli_autosave_discard(storyfilename);
}

static bool read_autosave_game(std::string *out)
{
    void *data = NULL;
    size_t length = 0;
    if (!gli_autosave_read_game(storyfilename, &data, &length))
        return false;
    out->assign((const char *)data, length);
    free(data);
    return true;
}

/* Unarchive autosave.plist with the given hook and replace the live Glk
 * object lists.  Returns the library or nil; the caller runs its "late"
 * pass once its own globals point at the restored objects. */
static TempLibrary *restore_library(void (*unarchive_hook)(TempLibrary *, NSCoder *))
{
    TempLibrary *newlib = gli_autosave_load_library(storyfilename, unarchive_hook);
    [newlib updateFromLibrary];
    return newlib;
}

/* The aslx frontend's library, held between aslx_autosave_restore_library
 * and its late pass. */
static TempLibrary *pending_library = nil;

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
 * buffer.  An autosave taken at an open menu or question appends a third
 * part, the replay record (see question_do_menu_autosave); a turn-prompt
 * autosave has none.  One taken at a question the startscript asked has
 * the replay record and an empty engine state (question_do_boot_autosave).  A file without the container magic is a bare engine
 * state (an autosave from before the container existed).  Being on disk,
 * the magic keeps its pre-rename "GEAS" prefix. */
static const char *const kQuestionContainerMagic = "GEASAUTO1\n";

static void put_part(std::string &out, const std::string &part)
{
    out += std::to_string(part.size());
    out += '\n';
    out += part;
}

/* Read one length-prefixed part at `pos`, advancing it. */
static bool get_part(const std::string &data, size_t &pos, std::string *part)
{
    size_t eol = data.find('\n', pos);
    if (eol == std::string::npos)
        return false;
    unsigned long len = strtoul(data.c_str() + pos, nullptr, 10);
    pos = eol + 1;
    if (len > data.size() - pos)
        return false;
    part->assign(data, pos, len);
    pos += len;
    return true;
}

static std::string container_wrap(const std::string &engine_state,
                                  const std::string &undo_history,
                                  const std::string &replay = std::string())
{
    std::string out = kQuestionContainerMagic;
    put_part(out, engine_state);
    put_part(out, undo_history);
    if (!replay.empty())
        put_part(out, replay);
    return out;
}

static bool container_split(const std::string &data, std::string *engine_state,
                            std::string *undo_history, std::string *replay)
{
    const std::string magic = kQuestionContainerMagic;
    replay->clear();
    if (data.compare(0, magic.size(), magic) != 0) {
        *engine_state = data;   /* legacy: the whole file is the engine state */
        undo_history->clear();
        return true;
    }
    size_t pos = magic.size();
    if (!get_part(data, pos, engine_state) || !get_part(data, pos, undo_history))
        return false;
    return pos == data.size() || get_part(data, pos, replay);
}

/* The replay record: the command line, then each answer, all length-
 * prefixed. */
static std::string replay_encode(const std::string &command,
                                 const std::vector<std::string> &answers)
{
    std::string out;
    put_part(out, command);
    for (const std::string &a : answers)
        put_part(out, a);
    return out;
}

static bool replay_decode(const std::string &data, std::string *command,
                          std::vector<std::string> *answers)
{
    size_t pos = 0;
    answers->clear();
    if (!get_part(data, pos, command))
        return false;
    while (pos < data.size()) {
        std::string a;
        if (!get_part(data, pos, &a))
            return false;
        answers->push_back(a);
    }
    return true;
}

/* The state at the start of the current turn, which a menu autosave saves in
 * place of the (unserializable) mid-parse state: engine state, undo history
 * and the RNG position, since the replay draws the same numbers again. */
static struct {
    bool valid = false;
    std::string state;
    std::string undo;
    int rng_usenative = -1;
    uint32_t rng_state[4] = { 0, 0, 0, 0 };
} turn_start;

static void capture_turn_start(QuestionRunner *gr, const std::string &state)
{
    QuestionGlkFrontendState st;
    question_stash_frontend_state(&st);
    turn_start.state = state;
    turn_start.undo = gr->save_undo_history();
    turn_start.rng_usenative = st.rng_usenative;
    memcpy(turn_start.rng_state, st.rng_state, sizeof turn_start.rng_state);
    turn_start.valid = !state.empty();
}

void question_note_turn_start(QuestionRunner *gr)
{
    if (gli_enable_autosave && !turn_start.valid)
        capture_turn_start(gr, gr->save_state(false));
}

void question_turn_state_changed(void)
{
    turn_start.valid = false;
}

void question_do_menu_autosave(const std::string &command,
                               const std::vector<std::string> &answers)
{
    if (!question_autosave_wanted() || !turn_start.valid)
        return;
    /* The windows as they are now (the menu on screen), but the RNG as it
     * was when the turn began. */
    question_stash_frontend_state(&frontend_state);
    frontend_state.rng_usenative = turn_start.rng_usenative;
    memcpy(frontend_state.rng_state, turn_start.rng_state,
           sizeof frontend_state.rng_state);
    std::string container = container_wrap(turn_start.state, turn_start.undo,
                                           replay_encode(command, answers));
    gli_autosave_write(storyfilename, container.data(), container.size(),
                       question_library_archive);
}

void question_do_boot_autosave(const std::vector<std::string> &answers)
{
    if (!question_autosave_wanted())
        return;
    question_stash_frontend_state(&frontend_state);
    std::string container = container_wrap(std::string(), std::string(),
                                           replay_encode(std::string(), answers));
    gli_autosave_write(storyfilename, container.data(), container.size(),
                       question_library_archive);
}

bool question_autosave_take_boot_replay(std::vector<std::string> *answers)
{
    if (!gli_enable_autosave || !question_autosave_exists())
        return false;
    std::string filedata, data, undo_history, replay, command;
    return read_autosave_game(&filedata)
        && container_split(filedata, &data, &undo_history, &replay)
        && data.empty() && !replay.empty()
        && replay_decode(replay, &command, answers);
}

bool question_restore_boot_autosave(void)
{
    @autoreleasepool {
        TempLibrary *newlib = restore_library(question_library_unarchive);
        if (!newlib) {
            question_autosave_discard();
            return false;
        }
        question_recover_frontend_state(&frontend_state);
        [newlib updateFromLibraryLate];
        turn_start.valid = false;
    }
    return true;
}

static bool pending_replay = false;
static std::string pending_replay_command;
static std::vector<std::string> pending_replay_answers;

bool question_autosave_take_replay(std::string *command,
                                   std::vector<std::string> *answers)
{
    if (!pending_replay)
        return false;
    pending_replay = false;
    command->swap(pending_replay_command);
    answers->swap(pending_replay_answers);
    pending_replay_command.clear();
    pending_replay_answers.clear();
    return true;
}

void question_do_autosave(QuestionRunner *gr)
{
    if (!question_autosave_wanted()) {
        /* Not saved, so the next command captures its own start state. */
        turn_start.valid = false;
        return;
    }
    std::string data = gr->save_state(false);
    if (data.empty())
        return;
    capture_turn_start(gr, data);
    question_stash_frontend_state(&frontend_state);
    std::string container = container_wrap(data, turn_start.undo);
    gli_autosave_write(storyfilename, container.data(), container.size(),
                       question_library_archive);
}

bool question_restore_autosave(QuestionRunner *gr)
{
    if (!gli_enable_autosave)
        return false;
    @autoreleasepool {
        /* An autosave that cannot be used is thrown away, so the next
         * launch does not trip over it again. */
        auto unusable = [](const char *why) {
            if (why)
                NSLog(@"question autorestore: %s", why);
            question_autosave_discard();
            return false;
        };

        std::string filedata;
        if (!read_autosave_game(&filedata))
            return unusable(NULL);

        std::string data, undo_history, replay;
        if (!container_split(filedata, &data, &undo_history, &replay))
            return unusable("autosave container was malformed.");
        if (!gr->load_state(data, false))
            return unusable("saved game state was not usable.");
        /* After load_state: the undo snapshots reference the restored props
         * log.  A missing or unreadable history just means no UNDO past the
         * restore point. */
        if (!undo_history.empty() && !gr->load_undo_history(undo_history))
            NSLog(@"question autorestore: undo history was not usable (ignored).");

        TempLibrary *newlib = restore_library(question_library_unarchive);
        if (!newlib)
            return unusable(NULL);
        question_recover_frontend_state(&frontend_state);
        [newlib updateFromLibraryLate];

        /* The restored state is where the next turn starts from -- or, for
         * a menu autosave, where the replayed one does. */
        capture_turn_start(gr, data);
        pending_replay = !replay.empty() &&
            replay_decode(replay, &pending_replay_command,
                          &pending_replay_answers);
        if (!replay.empty() && !pending_replay)
            NSLog(@"question autorestore: replay record was malformed (ignored).");
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
    gli_autosave_write(storyfilename, engine_state.data(), engine_state.size(),
                       aslx_library_archive);
    aslx_frontend_blob.clear();
}

bool aslx_autosave_read_game(std::string *out)
{
    return read_autosave_game(out);
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
