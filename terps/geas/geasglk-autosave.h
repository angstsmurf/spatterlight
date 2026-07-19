/* geasglk-autosave.h

   Spatterlight autosave/autorestore for the geas terp -- both the classic
   Quest 4 frontend (geasglk.cc) and the native Quest 5 frontend
   (aslxglk.cc) -- modeled on the Bocfel implementation
   (bocfel-spatterlight/).

   Only the Spatterlight app build compiles geasglk-autosave.mm; the call
   sites in the frontends sit behind #ifdef SPATTERLIGHT so headless builds
   are unaffected.

   Both engines share the same file layout under
   ~/Library/Application Support/Spatterlight/Geas Files/Autosaves/(HASH)/:
   autosave.glksave holds the engine's own self-contained state
   serialization, autosave.plist the Glk library state (TempLibrary) with
   engine-specific extras appended by an archive hook.  A given game file
   only ever runs on one of the two engines, so the shared names never
   collide.
*/

#ifndef GEASGLK_AUTOSAVE_H
#define GEASGLK_AUTOSAVE_H

#include <cstdint>
#include <string>

class GeasRunner;

/* ---- shared -------------------------------------------------------------- */

/* True if a complete autosave (game state + Glk library plist) exists for
 * the current game and autosaving is enabled. */
bool geas_autosave_exists(void);

/* The shared per-prompt guard: autosaving enabled, and the last event was
 * one worth saving after (not init/arrange/redraw, and timer only when the
 * autosave-on-timer preference is on). */
bool geas_autosave_wanted(void);

/* Delete the autosave files (called after a failed restore, so the next
 * launch boots fresh instead of hitting the same failure again). */
void geas_autosave_discard(void);

/* ---- Quest 4 (geasglk.cc / GeasRunner) ----------------------------------- */

/* The Glk-facing globals in geasglk.cc that an autorestore must re-point
 * at the restored Glk objects, carried across the archive as the objects'
 * serialization tags. */
struct GeasGlkFrontendState {
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

/* Implemented in geasglk.cc, the owner of those globals. */
void geas_stash_frontend_state(GeasGlkFrontendState *st);
void geas_recover_frontend_state(const GeasGlkFrontendState *st);

/* Save the whole game state and the Glk library state, then ask the window
 * server to snapshot the GUI under the same tag.  Called at every top-level
 * command prompt, after the prompt is printed but before line input is
 * requested, so a restore re-enters cleanly by just re-requesting input. */
void geas_do_autosave(GeasRunner *gr);

/* Restore an autosaved session into a booted runner (set_game already run,
 * with its output suppressed and its prompts auto-answered -- the restored
 * state replaces all of that).  On success the Glk window list has been
 * replaced and the frontend globals re-pointed.  On failure the bad
 * autosave files have been deleted; the caller should reset and exit. */
bool geas_restore_autosave(GeasRunner *gr);

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
