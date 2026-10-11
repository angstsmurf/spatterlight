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
   terps.  This file adds the one plist hook that carries a frontend's state
   and the Quest 4 side's bookkeeping.

   Both engines write the same two things: autosave.glksave is a
   questglk::AutosaveRecord (engine state, undo history, and the replay
   record when the autosave was taken under a prompt of a running turn), and
   autosave.plist carries, next to the Glk library, one opaque blob in which
   the frontend keeps what it needs to pick up its windows again.  Record and
   blob are both written with questglk's blob_num/blob_str.
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
#include "questglk-common.hh"
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

/* ---- the pair on disk ---------------------------------------------------- */

/* The frontend's blob, on its way into or out of the plist. */
static std::string frontend_blob;

static void library_archive(TempLibrary *library, NSCoder *encoder)
{
    (void)library;
    [encoder encodeObject:[NSData dataWithBytes:frontend_blob.data()
                                         length:frontend_blob.size()]
                   forKey:@"question_frontend_state"];
}

static void library_unarchive(TempLibrary *library, NSCoder *decoder)
{
    (void)library;
    NSData *blob = [decoder decodeObjectOfClass:[NSData class]
                                         forKey:@"question_frontend_state"];
    frontend_blob = blob ? std::string((const char *)blob.bytes, (size_t)blob.length)
                         : std::string();
}

void question_autosave_write(const questglk::AutosaveRecord &rec,
                             const std::string &blob)
{
    std::string data = questglk::encode_autosave_record(rec);
    frontend_blob = blob;
    gli_autosave_write(storyfilename, data.data(), data.size(), library_archive);
    frontend_blob.clear();
}

bool question_autosave_read(questglk::AutosaveRecord *rec)
{
    void *data = NULL;
    size_t length = 0;
    if (!gli_autosave_read_game(storyfilename, &data, &length))
        return false;
    std::string filedata((const char *)data, length);
    free(data);
    return questglk::decode_autosave_record(filedata, rec);
}

/* The library unarchived by question_autosave_restore_library, held until
 * its late pass. */
static TempLibrary *pending_library = nil;

bool question_autosave_restore_library(std::string *frontend_blob_out)
{
    if (!gli_enable_autosave)
        return false;
    @autoreleasepool {
        frontend_blob.clear();
        TempLibrary *newlib = gli_autosave_load_library(storyfilename,
                                                        library_unarchive);
        if (!newlib)
            return false;
        [newlib updateFromLibrary];
        *frontend_blob_out = frontend_blob;
        frontend_blob.clear();
        pending_library = newlib;
    }
    return true;
}

void question_autosave_restore_library_late(void)
{
    @autoreleasepool {
        [pending_library updateFromLibraryLate];
        pending_library = nil;
    }
}

/* ---- Quest 4 (questionglk.cc / QuestionRunner) --------------------------- */

/* The Quest 4 frontend's blob.  The stamp is bumped on every layout change;
 * frontend_decode rejects anything else. */
static const char *const kFrontendBlobMagic = "QUESTIONGLK-AUTOSAVE 1";

static std::string frontend_encode(const QuestionGlkFrontendState &st)
{
    std::string b;
    questglk::blob_str(b, kFrontendBlobMagic);
    questglk::blob_num(b, st.mainwintag);
    questglk::blob_num(b, st.inputwintag);
    questglk::blob_num(b, st.bannerwintag);
    questglk::blob_num(b, st.objwintag);
    questglk::blob_num(b, st.gfxwintag);
    questglk::blob_num(b, st.transcripttag);
    questglk::blob_num(b, st.soundchanneltag);
    questglk::blob_num(b, st.use_objpane);
    questglk::blob_str(b, st.objwin_expanded);
    questglk::blob_num(b, st.sidebar_wide);
    questglk::blob_num(b, st.rng_usenative);
    for (int i = 0; i < 4; i++)
        questglk::blob_num(b, (long)st.rng_state[i]);
    return b;
}

static bool frontend_decode(const std::string &blob, QuestionGlkFrontendState *st)
{
    questglk::BlobReader r{blob};
    if (r.str() != kFrontendBlobMagic)
        return false;
    st->mainwintag = (int)r.num();
    st->inputwintag = (int)r.num();
    st->bannerwintag = (int)r.num();
    st->objwintag = (int)r.num();
    st->gfxwintag = (int)r.num();
    st->transcripttag = (int)r.num();
    st->soundchanneltag = (int)r.num();
    st->use_objpane = (int)r.num();
    st->objwin_expanded = r.str();
    st->sidebar_wide = (int)r.num();
    st->rng_usenative = (int)r.num();
    for (int i = 0; i < 4; i++)
        st->rng_state[i] = (uint32_t)r.num();
    return r.ok;
}

/* Swap in the saved Glk library and re-point the frontend at it. */
static bool restore_library_and_frontend(void)
{
    std::string blob;
    QuestionGlkFrontendState st;
    if (!question_autosave_restore_library(&blob))
        return false;
    bool ok = frontend_decode(blob, &st);
    if (ok)
        question_recover_frontend_state(&st);
    question_autosave_restore_library_late();
    return ok;
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
                               const std::vector<std::string> &answers,
                               bool at_wait)
{
    if (!(at_wait ? gli_enable_autosave != 0 : question_autosave_wanted())
        || !turn_start.valid)
        return;
    /* The windows as they are now (the menu on screen), but the RNG as it
     * was when the turn began. */
    QuestionGlkFrontendState st;
    question_stash_frontend_state(&st);
    st.rng_usenative = turn_start.rng_usenative;
    memcpy(st.rng_state, turn_start.rng_state, sizeof st.rng_state);
    questglk::AutosaveRecord rec;
    rec.state = turn_start.state;
    rec.undo = turn_start.undo;
    rec.replay = true;
    rec.command = command;
    rec.answers = answers;
    question_autosave_write(rec, frontend_encode(st));
}

void question_do_boot_autosave(const std::vector<std::string> &answers,
                               bool at_wait)
{
    if (!(at_wait ? gli_enable_autosave != 0 : question_autosave_wanted()))
        return;
    QuestionGlkFrontendState st;
    question_stash_frontend_state(&st);
    questglk::AutosaveRecord rec;
    rec.replay = true;
    rec.answers = answers;
    question_autosave_write(rec, frontend_encode(st));
}

bool question_autosave_take_boot_replay(std::vector<std::string> *answers)
{
    if (!gli_enable_autosave || !question_autosave_exists())
        return false;
    questglk::AutosaveRecord rec;
    if (!question_autosave_read(&rec) || !rec.boot_replay())
        return false;
    answers->swap(rec.answers);
    return true;
}

bool question_restore_boot_autosave(void)
{
    if (!restore_library_and_frontend()) {
        question_autosave_discard();
        return false;
    }
    turn_start.valid = false;
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
    questglk::AutosaveRecord rec;
    rec.state = gr->save_state(false);
    if (rec.state.empty())
        return;
    capture_turn_start(gr, rec.state);
    rec.undo = turn_start.undo;
    QuestionGlkFrontendState st;
    question_stash_frontend_state(&st);
    question_autosave_write(rec, frontend_encode(st));
}

bool question_restore_autosave(QuestionRunner *gr)
{
    if (!gli_enable_autosave)
        return false;
    /* An autosave that cannot be used is thrown away, so the next launch
     * does not trip over it again. */
    auto unusable = [](const char *why) {
        NSLog(@"question autorestore: %s", why);
        question_autosave_discard();
        return false;
    };

    questglk::AutosaveRecord rec;
    if (!question_autosave_read(&rec))
        return unusable("autosave was not readable.");
    if (!gr->load_state(rec.state, false))
        return unusable("saved game state was not usable.");
    /* After load_state: the undo snapshots reference the restored props
     * log.  A missing or unreadable history just means no UNDO past the
     * restore point. */
    if (!rec.undo.empty() && !gr->load_undo_history(rec.undo))
        NSLog(@"question autorestore: undo history was not usable (ignored).");

    if (!restore_library_and_frontend())
        return unusable("Glk library state was not usable.");

    /* The restored state is where the next turn starts from -- or, for a
     * menu autosave, where the replayed one does. */
    capture_turn_start(gr, rec.state);
    pending_replay = rec.replay;
    pending_replay_command.swap(rec.command);
    pending_replay_answers.swap(rec.answers);
    return true;
}
