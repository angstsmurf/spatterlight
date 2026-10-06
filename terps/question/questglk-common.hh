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

extern "C" {
#include "glk.h"
}

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

std::string lower (std::string s);
std::string trim (const std::string &s);
std::string cap_first (std::string s);

/* ------------------------------------------------------------------ UTF-8 */

glui32 utf8_next_cp (const std::string &s, size_t &i);
void put_stream_utf8 (strid_t s, const std::string &str);
size_t utf8_cp_len (const std::string &s);
std::string tail_chars (const std::string &s, size_t n, bool utf8);
bool unput_tail_exact (winid_t win, const std::u32string &s);

/* ------------------------------------------------------------------- echo */

void echo_input_line (const std::string &line, bool utf8);

/* ------------------------------------------------------------------ style */

glui32 glk_style_for (bool bold, bool italic, bool underlined);

/* ----------------------------------------------------------------- banner */

std::string status_tail (const std::string &status, size_t avail, bool utf8);
void draw_status_banner (winid_t banner, const std::string &room,
                         const std::string &status, bool utf8);

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

int register_path_resource (const std::string &path, bool sound);

} /* namespace questglk */

#endif /* QUESTION_QUESTGLK_COMMON_HH */
