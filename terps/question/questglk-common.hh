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

/* questglk-common.hh -- presentation helpers shared by the Question terp's two
   Glk frontends: questionglk.cc (the classic Quest 1-4 runner) and aslxglk.cc
   (the native Quest 5 engine).  Defined in questglk-common.cc, where each
   helper is documented. */

#ifndef QUESTION_QUESTGLK_COMMON_HH
#define QUESTION_QUESTGLK_COMMON_HH

#include <string>
#include <vector>

extern "C" {
#include "glk.h"
}

#include "qstr.hh"

#ifdef SPATTERLIGHT
/* Spatterlight's by-path resource registration (glkimp's fileresource.c).
 * Quest games in both dialects reference image/sound files by name (e.g.
 * "die.wav"), not by numbered Blorb resource, so each file is registered
 * with the backend under a resource number of glkimp's choosing;
 * glk_image_draw* / glk_schannel_play* then find it already loaded. */
extern "C" {
glui32 gli_add_resource_from_path (glui32 usage, const char *path,
                                   glui32 offset, glui32 length);
int  win_findsound (int resno);
}
#endif

namespace questglk {

/* ---------------------------------------------------------------- strings */

/* The shared helpers (qstr.hh) under the names the frontends know them by. */
inline std::string lower (std::string s) { return qstr::ascii_lower (std::move (s)); }
using qstr::trim;
std::string cap_first (std::string s);

/* ------------------------------------------------------------------ UTF-8 */

glui32 utf8_next_cp (const std::string &s, size_t &i);
void utf8_append (std::string &out, glui32 cp);
std::string utf8_from_uni (const glui32 *buf, size_t n);
void put_stream_utf8 (strid_t s, const std::string &str);
size_t utf8_cp_len (const std::string &s);
std::string tail_chars (const std::string &s, size_t n, bool utf8);
bool unput_tail_exact (winid_t win, const std::u32string &s);

/* ------------------------------------------------------------------ input */

void echo_input_line (const std::string &line, bool utf8);
glui32 cancel_line_request (winid_t win);

/* ------------------------------------------------------------------ style */

glui32 glk_style_for (bool bold, bool italic, bool underlined);

/* ----------------------------------------------------------------- banner */

std::string status_tail (const std::string &status, size_t avail, bool utf8);
void draw_status_banner (winid_t banner, const std::string &room,
                         const std::string &status, bool utf8);
bool status_leaves_banner (size_t width, const std::string &room,
                           const std::string &status, bool utf8,
                           bool in_pane);
bool status_wants_pane (winid_t banner, const std::string &room,
                        const std::string &status, bool utf8, bool in_pane);

/* ----------------------------------------------------------------- status */

bool match_status_command (const std::string &raw);
void print_status_report (const std::string &status, bool utf8);

/* ------------------------------------------------------- save / restore */

bool match_save_command (const std::string &raw);
bool match_restore_command (const std::string &raw, bool asking = false);

/* ---------------------------------------------------------- system help */

bool match_help_command (const std::string &raw);
void print_system_commands (const char *quit_rows, const char *oops_rows,
                            const char *verbs_rows, const char *about_rows);

/* -------------------------------------------------------------- side pane */

extern const char PANE_INVENTORY[];
extern const char PANE_PLACES_OBJECTS[];
extern const char PANE_COMPASS[];
extern const char PANE_STATUS[];

bool side_pane_is_wide ();
void set_side_pane_wide (bool wide, winid_t pane, winid_t divider);
int match_sidebar_command (const std::string &raw);
void change_side_pane_width (int which, bool supported, winid_t pane,
                             winid_t divider);
void open_side_pane_windows (winid_t mainwin, winid_t *pane, winid_t *divider);
void close_side_pane_windows (winid_t *pane, winid_t *divider);
void fill_side_divider (winid_t mainwin, winid_t divider);
void put_pane_header (strid_t s, const std::string &header, bool utf8);
void put_pane_link (strid_t s, const std::string &label, glui32 linkval,
                    bool utf8);

/* ------------------------------------------------------------------ sound */

bool play_single_sound (schanid_t *chan, glui32 resno, bool looped,
                        glui32 notify);
void stop_single_sound (schanid_t *chan);

/* ------------------------------------------------------------- transcript */

int match_transcript_command (const std::string &raw);
void toggle_transcript (int on, winid_t win, strid_t *slot);

/* ---------------------------------------------------------- event waits */

/* Block until `done` accepts an event, and return that event.  A resize or
 * redraw meanwhile goes to `rearrange`; every other event is dropped, timer
 * ticks included -- these are the waits during which the game stands still.
 * Requesting the input to wait on, and cancelling whatever is left pending
 * afterwards, stay with the caller: which requests are live differs at every
 * site. */
template <class Done, class Rearrange>
event_t
wait_for_event (Done done, Rearrange rearrange)
{
    for (;;) {
        event_t ev;
        glk_select (&ev);
        if (done (ev))
            return ev;
        if (ev.type == evtype_Arrange || ev.type == evtype_Redraw)
            rearrange ();
    }
}

/* --------------------------------------------------------- end-of-story */

/* The choices of the menu offered once the story is over (see
 * post_game_menu_print). */
enum { POSTGAME_UNDO = 1, POSTGAME_RESTORE, POSTGAME_RESTART, POSTGAME_QUIT };

extern const char QUIT_FAREWELL[];
extern const char NOTHING_TO_UNDO[];

void post_game_menu_print (void);
int post_game_menu_match (const std::string &raw);
void post_game_menu_reprompt (void);

/* ------------------------------------------------------------- save files */

bool prompt_write_save (const std::string &data);
bool prompt_read_save (std::string &data);

/* -------------------------------------------------------------- resources */

int register_path_resource (const std::string &path, bool sound,
                            glui32 offset = 0, glui32 length = 0,
                            bool verify = false);

/* --------------------------------------------------------------- autosave */

/* The one encoding everything a Question autosave writes goes through, for
 * both engines: a number is its decimal digits and a newline, a string is
 * its byte length as a number, the bytes, and a newline.  Strings may hold
 * anything, newlines and NULs included. */
void blob_num (std::string &s, long v);
void blob_str (std::string &s, const std::string &v);

/* Reads the same back.  A field that is missing or cut short clears `ok`
 * and every read after it returns 0 or the empty string, so a caller reads
 * the whole layout and tests `ok` once at the end. */
struct BlobReader {
  const std::string &s;
  size_t pos = 0;
  bool ok = true;

  long num ();
  std::string str ();
  bool at_end () const { return pos >= s.size (); }
};

/* What autosave.glksave holds, whichever engine wrote it.
 *
 * An autosave at the command prompt is the engine's state and its undo
 * history.  One taken while a turn has a prompt of its own open -- a menu, a
 * question, a pause for a key -- cannot be: the engine is in the middle of a
 * script.  It holds the state the turn *started* from plus the replay
 * record, the command and every answer given since, and the autorestore
 * runs the turn again in silence, feeding it those answers, until the
 * prompt is open again.  The game's start script counts as a turn with no
 * command and no state before it, so an autosave under one of its prompts
 * has an empty `state` and the boot itself is what gets replayed.
 *
 * What an answer looks like is the frontend's business; the record only
 * carries them. */
struct AutosaveRecord {
  std::string state;                  /* the engine's own serialization */
  std::string undo;                   /* ...and its undo history */
  bool replay = false;
  std::string command;                /* empty when the boot is the turn */
  std::vector<std::string> answers;

  bool boot_replay () const { return replay && state.empty (); }
};

std::string encode_autosave_record (const AutosaveRecord &rec);
/* False for anything that is not a whole record of this format, which is
 * how an autosave from an older build reads. */
bool decode_autosave_record (const std::string &data, AutosaveRecord *rec);

} /* namespace questglk */

#endif /* QUESTION_QUESTGLK_COMMON_HH */
