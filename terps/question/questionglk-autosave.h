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

/* questionglk-autosave.h

   Spatterlight autosave/autorestore for the Question terp -- both the classic
   Quest 4 frontend (questionglk.cc) and the native Quest 5 frontend
   (aslxglk.cc) -- modeled on the Bocfel implementation
   (bocfel-spatterlight/).

   Only the Spatterlight app build compiles questionglk-autosave.mm; the call
   sites in the frontends sit behind #ifdef SPATTERLIGHT so headless builds
   are unaffected.

   Both engines share the same file layout under
   ~/Library/Application Support/Spatterlight/Quest Files/Autosaves/(HASH)/:
   autosave.glksave holds the engine's own self-contained state
   serialization, autosave.plist the Glk library state (TempLibrary) with
   engine-specific extras appended by an archive hook.  A given game file
   only ever runs on one of the two engines, so the shared names never
   collide.
*/

#ifndef QUESTIONGLK_AUTOSAVE_H
#define QUESTIONGLK_AUTOSAVE_H

#include <cstdint>
#include <string>
#include <vector>

class QuestionRunner;

/* ---- shared -------------------------------------------------------------- */

/* True if a complete autosave (game state + Glk library plist) exists for
 * the current game and autosaving is enabled. */
bool question_autosave_exists(void);

/* The shared per-prompt guard: autosaving enabled, and the last event was
 * one worth saving after (not init/arrange/redraw, and timer only when the
 * autosave-on-timer preference is on). */
bool question_autosave_wanted(void);

/* Delete the autosave files (called after a failed restore, so the next
 * launch boots fresh instead of hitting the same failure again). */
void question_autosave_discard(void);

/* ---- Quest 4 (questionglk.cc / QuestionRunner) --------------------------- */

/* The Glk-facing globals in questionglk.cc that an autorestore must re-point
 * at the restored Glk objects, carried across the archive as the objects'
 * serialization tags. */
struct QuestionGlkFrontendState {
    int mainwintag = 0;
    int inputwintag = 0;       /* == mainwintag unless a separate input window */
    int bannerwintag = 0;
    int objwintag = 0;         /* right-hand pane, 0 when closed */
    int gfxwintag = 0;         /* pane divider, 0 when closed */
    int transcripttag = 0;     /* open transcript file stream, if any */
    int soundchanneltag = 0;
    int use_objpane = 0;
    std::string objwin_expanded; /* object whose verb menu is unfolded in the pane */
    /* Exact RNG state (erkyrath_random detstate): which generator is active
     * plus the xoshiro words, so deterministic randomness continues across
     * an autorestore.  -1 = not recorded (an older autosave). */
    int rng_usenative = -1;
    uint32_t rng_state[4] = { 0, 0, 0, 0 };
};

/* Implemented in questionglk.cc, the owner of those globals. */
void question_stash_frontend_state(QuestionGlkFrontendState *st);
void question_recover_frontend_state(const QuestionGlkFrontendState *st);

/* Save the whole game state and the Glk library state, then ask the window
 * server to snapshot the GUI under the same tag.  Called at every top-level
 * command prompt, after the prompt is printed but before line input is
 * requested, so a restore re-enters cleanly by just re-requesting input. */
void question_do_autosave(QuestionRunner *gr);

/* Restore an autosaved session into a booted runner (set_game already run,
 * with its output suppressed and its prompts auto-answered -- the restored
 * state replaces all of that).  On success the Glk window list has been
 * replaced and the frontend globals re-pointed.  On failure the bad
 * autosave files have been deleted; the caller should reset and exit. */
bool question_restore_autosave(QuestionRunner *gr);

/* Autosave while the parser's "which one do you mean?" menu is open.  The
 * engine can't be serialized mid-parse, so the autosave instead holds the
 * state from the start of the turn plus a replay record: the command line
 * and the answers already given to earlier prompts in the same turn.  An
 * autorestore loads that state and re-runs the command silently, feeding it
 * the recorded answers, until it reaches the open menu again.
 *
 * The start-of-turn state is cached by question_do_autosave.  Call
 * question_turn_state_changed when the engine state moves on without one (a
 * timer counting down), and question_note_turn_start just before running a
 * command, which re-captures the state if the cache went stale. */
void question_note_turn_start(QuestionRunner *gr);
void question_turn_state_changed(void);

/* Write the menu autosave.  Called with the menu and its prompt on screen
 * and no input requested yet, like question_do_autosave. */
void question_do_menu_autosave(const std::string &command,
                               const std::vector<std::string> &answers);

/* After a successful question_restore_autosave: true (once) if the autosave
 * was taken at an open menu, handing back what to replay. */
bool question_autosave_take_replay(std::string *command,
                                   std::vector<std::string> *answers);

/* ---- Quest 5 (aslxglk.cc / aslx Interp) ---------------------------------- */

/* The aslx frontend lives in an anonymous namespace, so its state crosses
 * into the archive as an opaque blob it encodes/decodes itself (window
 * tags, transcript hyperlink actions, banner strings). */

/* Write engine state + library plist (with the blob) and win_autosave. */
void aslx_do_autosave_write(const std::string &engine_state,
                            const std::string &frontend_blob);

/* Read autosave.glksave into `out` (the engine applies it itself). */
bool aslx_autosave_read_game(std::string *out);

/* Unarchive autosave.plist, replace the live Glk object lists, and hand
 * back the frontend blob.  The library's "late" pass (file/resource stream
 * reopening, timer re-arm) is a separate call so the frontend can re-point
 * its globals in between, mirroring the Bocfel sequence. */
bool aslx_autosave_restore_library(std::string *frontend_blob_out);
void aslx_autosave_restore_library_late(void);

#endif
