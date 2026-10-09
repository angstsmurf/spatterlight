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

/* questglk-common.cc -- presentation helpers shared by the Question terp's two
   Glk frontends: questionglk.cc (the classic Quest 1-4 runner) and aslxglk.cc
   (the native Quest 5 engine).  Both include questglk-common.hh; everything
   they share lives in namespace questglk.

   These are the pieces the Quest 5 frontend originally mirrored line for
   line from the classic one: the status banner, the right-hand pane and its
   divider, the transcript metaverb, the save-file prompts, and the small
   string/UTF-8 utilities.  The player-visible strings live here so the two
   frontends cannot drift apart.  Both engines' output is pinned by byte-diff
   regression suites; changes here must keep it byte-stable. */

#include "questglk-common.hh"

#include <cctype>
#include <string>
#include <vector>

#ifdef SPATTERLIGHT
extern "C" {
#include "gi_blorb.h"
}
#endif

namespace questglk {

/* ---------------------------------------------------------------- strings */

std::string
lower (std::string s)
{
    for (char &c : s)
        c = (char) tolower ((unsigned char) c);
    return s;
}

std::string
trim (const std::string &s)
{
    std::string::size_type a = s.find_first_not_of (" \t\r\n");
    if (a == std::string::npos)
        return "";
    std::string::size_type b = s.find_last_not_of (" \t\r\n");
    return s.substr (a, b - a + 1);
}

/* Uppercase the leading ASCII letter (Core's CapFirst, input[..1].ToUpper()):
 * room names and pane labels read better capitalized.  Only the leading
 * ASCII letter is touched -- a UTF-8 lead byte (>= 0x80) or non-letter first
 * character is left alone, so accented or symbol-first names pass through
 * unchanged. */
std::string
cap_first (std::string s)
{
    if (!s.empty () && (unsigned char) s[0] < 0x80)
        s[0] = (char) toupper ((unsigned char) s[0]);
    return s;
}

/* ------------------------------------------------------------------ UTF-8 */

/* Decode one UTF-8 sequence starting at s[i] and advance i past it.  A
 * truncated tail falls back to the single byte value (a Latin-1-ish
 * passthrough, the historical behaviour of every copy of this loop). */
glui32
utf8_next_cp (const std::string &s, size_t &i)
{
    unsigned char c = (unsigned char) s[i];
    glui32 cp = c;
    int len = 1;
    if (c >= 0xf0) len = 4;
    else if (c >= 0xe0) len = 3;
    else if (c >= 0xc0) len = 2;
    if (len > 1 && i + len <= s.size ()) {
        cp = c & (0x7f >> len);
        for (int k = 1; k < len; k++)
            cp = (cp << 6) | ((unsigned char) s[i + k] & 0x3f);
    }
    i += len;
    return cp;
}

/* Write a UTF-8 string to a byte stream per-codepoint so accents survive
 * (the byte-stream API is Latin-1). */
void
put_stream_utf8 (strid_t s, const std::string &str)
{
    for (size_t i = 0; i < str.size ();)
        glk_put_char_stream_uni (s, utf8_next_cp (str, i));
}

size_t
utf8_cp_len (const std::string &s)
{
    size_t n = 0;
    for (size_t i = 0; i < s.size (); i++)
        if (((unsigned char) s[i] & 0xc0) != 0x80)
            n++;
    return n;
}

/* The last `n` characters of `s` -- codepoints when `utf8`, bytes otherwise
 * -- or the whole string when it is already that short.  Used to cut a status
 * line down to the cells the banner has left; splitting by codepoint keeps a
 * multi-byte character from being sliced in half. */
std::string
tail_chars (const std::string &s, size_t n, bool utf8)
{
    if (!utf8)
        return s.size () <= n ? s : s.substr (s.size () - n);
    if (utf8_cp_len (s) <= n)
        return s;
    size_t i = s.size (), got = 0;
    while (i > 0 && got < n) {
        i--;
        while (i > 0 && ((unsigned char) s[i] & 0xc0) == 0x80)
            i--;
        got++;
    }
    return s.substr (i);
}

#ifdef SPATTERLIGHT
/* Take `s` back off the end of `win`, but only if it is still exactly what
 * is there: garglk_unput_string_count_uni is a case-insensitive TAIL compare
 * (GlkTextBufferWindow+Output.m unputString) that removes nothing unless the
 * whole string matches.  Returns the count removed, 0 if the window has
 * moved on -- so every caller degrades to "leave the text alone".
 *
 * It retracts from the CURRENT output stream (glkimp/stream.c), which during
 * an engine callback need not be `win`, so point it there and put the old
 * stream back. */
static glui32
unput_window_tail (winid_t win, const std::u32string &s)
{
    if (s.empty () || !win)
        return 0;
    strid_t saved = glk_stream_get_current ();
    glk_set_window (win);
    std::vector<glui32> buf (s.begin (), s.end ());
    buf.push_back (0);
    glui32 got = garglk_unput_string_count_uni (buf.data ());
    glk_stream_set_current (saved);
    return got;
}
#endif

/* Take `s` back off the end of `win`, but only when the whole of it came
 * back: a partial match means the window has moved on and the caller has to
 * leave the text alone.  Always false where there is no unput at all
 * (CheapGlk), so a caller that degrades gracefully needs no #ifdef of its
 * own. */
bool
unput_tail_exact (winid_t win, const std::u32string &s)
{
#ifdef SPATTERLIGHT
    return unput_window_tail (win, s) == s.size ();
#else
    (void) win;
    (void) s;
    return false;
#endif
}

/* ------------------------------------------------------------------- echo */

/* Echo an accepted input line -- or a clicked hyperlink's command, run as if
 * typed -- to the current stream in the input style, ending the line.  Both
 * frontends echo manually (library line-input echo is off so a timer or a
 * click can cancel a pending request without leaving stray text). */
void
echo_input_line (const std::string &line, bool utf8)
{
    glk_set_style (style_Input);
    if (utf8)
        put_stream_utf8 (glk_stream_get_current (), line);
    else
        glk_put_string ((char *) line.c_str ());
    glk_set_style (style_Normal);
    glk_put_char ('\n');
}

/* ------------------------------------------------------------------ style */

/* The Glk style standing in for a combination of bold, italic and underline.
 * Glk styles are defined before the window opens, so mid-game a frontend can
 * only pick the closest one: bold italic is Alert, italic Emphasized, bold
 * Subheader, and underline -- which loses to both -- User2. */
glui32
glk_style_for (bool bold, bool italic, bool underlined)
{
    if (bold && italic)
        return style_Alert;
    if (italic)
        return style_Emphasized;
    if (bold)
        return style_Subheader;
    if (underlined)
        return style_User2;
    return style_Normal;
}

/* ----------------------------------------------------------------- banner */

/* Marker put in front of a status line that was cut down to fit: U+2026,
 * a horizontal ellipsis, one cell wide. */
static const glui32 STATUS_ELLIPSIS = 0x2026;

/* The longest tail of `status` that fits in `avail` cells, without the blanks
 * it would start with when the cut lands in a gap. */
std::string
status_tail (const std::string &status, size_t avail, bool utf8)
{
    std::string tail = tail_chars (status, avail, utf8);
    std::string::size_type a = tail.find_first_not_of (' ');
    return a == std::string::npos ? std::string () : tail.substr (a);
}

/* Draw the one-line status banner grid: the current room name left-aligned
 * at column 1, and the game's status variables right-aligned (at least one
 * blank cell between them, one before the right edge).  The room name is
 * never shortened; a status too long for the cells left over is truncated
 * from the LEFT, keeping the fields at the end of the line -- the whole
 * status stays readable with the STATUS metaverb.  What survives follows the
 * room name directly, with only an ellipsis between the two and no blanks
 * around it.
 * `utf8` selects codepoint-aware writes and measurement (Quest 5 text is
 * UTF-8); byte mode passes classic Question text through verbatim (those games
 * are typically Latin-1). */
void
draw_status_banner (winid_t banner, const std::string &room,
                    const std::string &status, bool utf8)
{
    if (!banner)
        return;
    glk_window_clear (banner);
    strid_t stream = glk_window_get_stream (banner);
    glk_set_style_stream (stream, style_User1);
    glui32 width;
    glk_window_get_size (banner, &width, nullptr);
    for (glui32 i = 0; i < width; i++)
        glk_put_char_stream (stream, ' ');

    glk_window_move_cursor (banner, 1, 0);
    if (utf8)
        put_stream_utf8 (stream, room);
    else
        glk_put_string_stream (stream, (char *) room.c_str ());

    if (!status.empty ()) {
        glui32 rlen = (glui32) (utf8 ? utf8_cp_len (room) : room.size ());
        glui32 slen = (glui32) (utf8 ? utf8_cp_len (status) : status.size ());
        /* Cells past the room name, less the one before the right edge. */
        glui32 avail = (width > rlen + 2) ? width - rlen - 2 : 0;
        std::string shown = status;
        if (slen + 1 <= avail) {
            /* Fits with a blank to spare after the room name. */
            glk_window_move_cursor (banner, width - slen - 1, 0);
        } else {
            /* The ellipsis takes one cell; nothing is drawn unless at least
             * one character of the status fits behind it. */
            if (avail < 2)
                return;
            shown = status_tail (status, avail - 1, utf8);
            if (shown.empty ())
                return;
            glk_window_move_cursor (banner, 1 + rlen, 0);
            glk_put_char_stream_uni (stream, STATUS_ELLIPSIS);
        }
        if (utf8)
            put_stream_utf8 (stream, shown);
        else
            glk_put_string_stream (stream, (char *) shown.c_str ());
    }
}

/* Where the status belongs: true for the side pane (under a "Status" header,
 * one field per line), false for the banner.  It leaves the banner once less
 * than three quarters of it would survive draw_status_banner's cut, and --
 * `in_pane`, the answer last time -- comes back only when all of it fits, so
 * a window dragged across the threshold does not flicker between the two.
 * The frontends ask on every banner redraw, which covers a resize and a
 * change of the status alike; one that has no pane to offer does not ask. */
bool
status_wants_pane (winid_t banner, const std::string &room,
                   const std::string &status, bool utf8, bool in_pane)
{
    if (!banner)
        return false;
    glui32 width;
    glk_window_get_size (banner, &width, nullptr);
    return status_leaves_banner (width, room, status, utf8, in_pane);
}

/* The rule itself, for a banner `width` cells wide. */
bool
status_leaves_banner (size_t width, const std::string &room,
                      const std::string &status, bool utf8, bool in_pane)
{
    if (status.empty ())
        return false;
    size_t rlen = utf8 ? utf8_cp_len (room) : room.size ();
    size_t slen = utf8 ? utf8_cp_len (status) : status.size ();
    /* The same arithmetic as draw_status_banner. */
    size_t avail = (width > rlen + 2) ? width - rlen - 2 : 0;
    if (slen + 1 <= avail)
        return false;
    if (in_pane)
        return true;
    std::string shown;
    if (avail >= 2)
        shown = status_tail (status, avail - 1, utf8);
    size_t fits = utf8 ? utf8_cp_len (shown) : shown.size ();
    return fits * 4 < slen * 3;
}

/* ----------------------------------------------------------------- status */

/* STATUS metaverb matching.  The banner is a single grid line and drops the
 * head of a status that does not fit, so this is how the player reads the
 * whole thing.  '#status' is accepted too, for a game that claims the plain
 * word for itself. */
bool
match_status_command (const std::string &raw)
{
    std::string c = lower (trim (raw));
    return c == "status" || c == "#status";
}

/* Print the banner's status text in full, one field per line when it holds
 * more than one -- fields are joined with " | " by both frontends, and a
 * status long enough to need this command practically always has several. */
void
print_status_report (const std::string &status, bool utf8)
{
    std::string s = trim (status);
    if (s.empty ()) {
        glk_put_string ((char *) "\nThis game does not put anything in the"
                                 " status bar.\n");
        return;
    }
    glk_put_char ('\n');
    for (std::string::size_type pos = 0; pos <= s.size ();) {
        std::string::size_type sep = s.find (" | ", pos);
        std::string field = trim (sep == std::string::npos
                                  ? s.substr (pos) : s.substr (pos, sep - pos));
        if (!field.empty ()) {
            if (utf8)
                put_stream_utf8 (glk_stream_get_current (), field);
            else
                glk_put_string ((char *) field.c_str ());
            glk_put_char ('\n');
        }
        if (sep == std::string::npos)
            break;
        pos = sep + 3;
    }
}

/* ------------------------------------------------------- save / restore */

/* SAVE metaverb matching.  Saving is a UI action in both reference players
 * (a menu item, not a typed command), so the frontends own the wording. */
bool
match_save_command (const std::string &raw)
{
    std::string c = lower (trim (raw));
    return c == "save" || c == "save game";
}

/* RESTORE metaverb matching.  Bare "load" is the one form a game's own
 * question ("get input") might plausibly be answered with, so pass `asking`
 * while one is pending and it yields to the game; the unambiguous forms are
 * honoured whenever they are typed. */
bool
match_restore_command (const std::string &raw, bool asking)
{
    std::string c = lower (trim (raw));
    if (c == "load")
        return !asking;
    return c == "restore" || c == "restore game" || c == "load game";
}

/* ---------------------------------------------------------- system help */

/* #HELP metaverb matching.  The listing below is already shared; without
 * this its trigger was the one part each frontend still spelled out for
 * itself.  '#commands' and 'metaverbs' are accepted because the list has
 * been called both, and neither word is plausible as game input. */
bool
match_help_command (const std::string &raw)
{
    std::string c = lower (trim (raw));
    return c == "#help" || c == "#commands" || c == "metaverbs";
}

/* The #HELP listing: the system commands that work in any game, whether the
 * engine or the frontend handles them.  Most of the list is the same for both
 * dialects, so the layout and the shared rows live here and each frontend
 * passes only the rows that differ; an empty string omits a row entirely.
 *
 *   quit_rows   the QUIT row (Question takes "q" as well; Quest 5 does not)
 *   oops_rows   drawn before VERBS -- question-runner has OOPS, Quest 5 has not
 *   verbs_rows  the VERBS row(s), which say different things about the menu
 *   about_rows  drawn after STATUS -- question-runner has ABOUT, Quest 5 has not
 *
 * Each argument is one or more complete "  NAME  text\n" rows, indented to
 * the same columns as the shared ones below. */
void
print_system_commands (const char *quit_rows, const char *oops_rows,
                       const char *verbs_rows, const char *about_rows)
{
    glk_put_string ((char *)
        "\nThese system commands work in any game, whether Quest itself or"
        " this interpreter handles them:\n"
        "\n"
        "  SAVE              Save the whole game to a file.\n"
        "  RESTORE  (LOAD)   Restore a previously saved game.\n"
        "  RESTART           Start the game over from the beginning.\n"
        "  UNDO              Take back the last turn.\n");
    glk_put_string ((char *) quit_rows);
    glk_put_string ((char *)
        "\n"
        "  SCRIPT            Start recording the game text to a file.\n"
        "  SCRIPT OFF        Stop recording the transcript.\n"
        "\n");
    glk_put_string ((char *) oops_rows);
    glk_put_string ((char *) verbs_rows);
    glk_put_string ((char *)
        "  STATUS            Show the status bar's text in full, for when it\n"
        "                    is too long to fit beside the room name.\n");
    glk_put_string ((char *) about_rows);
    glk_put_string ((char *)
        "  HELP              Show the game's own in-game help.\n"
        "  #HELP             Show this list of system commands.\n");
}

/* -------------------------------------------------------------- side pane */

/* The pane's section headings.  Quest 5 games may localize them (the
 * InventoryLabel / PlacesObjectsLabel / CompassLabel templates); these are
 * that engine's fallbacks and the classic runner's only wording. */
const char PANE_INVENTORY[] = "Inventory";
const char PANE_PLACES_OBJECTS[] = "Places and Objects";
const char PANE_COMPASS[] = "Compass";
const char PANE_STATUS[] = "Status";

/* Open the right-hand pane: a 20%-proportional split of the main text
 * window, with a thin graphics window as its left child, drawn in the text
 * colour as a divider (see fill_side_divider).  No-op when already open; the
 * pane stays null when the host cannot split. */
void
open_side_pane_windows (winid_t mainwin, winid_t *pane, winid_t *divider)
{
    if (*pane)
        return;
    *pane = glk_window_open (mainwin, winmethod_Right | winmethod_Proportional,
                             20, wintype_TextBuffer, 0);
    if (*pane && glk_gestalt (gestalt_Graphics, 0))
        *divider = glk_window_open (*pane, winmethod_Left | winmethod_Fixed,
                                    2, wintype_Graphics, 0);
}

/* Close the pane so the main window reclaims the width.  The divider is a
 * child split of the pane, so close it first. */
void
close_side_pane_windows (winid_t *pane, winid_t *divider)
{
    if (*divider) { glk_window_close (*divider, nullptr); *divider = nullptr; }
    if (*pane)    { glk_window_close (*pane, nullptr);    *pane = nullptr; }
}

/* Paint the divider in the main window's text colour.  Graphics windows are
 * blanked on resize, so Arrange/Redraw events call this again. */
void
fill_side_divider (winid_t mainwin, winid_t divider)
{
    if (!divider)
        return;
    glui32 color;
    if (!glk_style_measure (mainwin, style_Normal, stylehint_TextColor, &color))
        color = 0;   /* fall back to black */
    glui32 w = 0, h = 0;
    glk_window_get_size (divider, &w, &h);
    glk_window_fill_rect (divider, color, 0, 0, w, h);
}

/* Write a pane section header on its own line, in the subheader style. */
void
put_pane_header (strid_t s, const std::string &header, bool utf8)
{
    glk_set_style_stream (s, style_Subheader);
    if (utf8)
        put_stream_utf8 (s, header);
    else
        glk_put_string_stream (s, (char *) header.c_str ());
    glk_put_char_stream (s, '\n');
    glk_set_style_stream (s, style_Normal);
}

/* Write one pane entry on its own line.  A non-zero linkval makes the label
 * a hyperlink; each frontend keeps its own table mapping link values back to
 * click commands. */
void
put_pane_link (strid_t s, const std::string &label, glui32 linkval, bool utf8)
{
    if (linkval)
        glk_set_hyperlink_stream (s, linkval);
    if (utf8)
        put_stream_utf8 (s, label);
    else
        glk_put_string_stream (s, (char *) label.c_str ());
    if (linkval)
        glk_set_hyperlink_stream (s, 0);
    glk_put_char_stream (s, '\n');
}

/* ------------------------------------------------------------------ sound */

/* Quest (in both dialects) plays one sound at a time -- like the reference
 * players' single audio element, a new sound replaces the one playing.  Play
 * `resno` on the frontend's single channel (lazily created into *chan),
 * looped = repeat forever; a non-zero notify requests a finish notification
 * (the Quest 5 synchronous play).  False when the channel is unavailable
 * (sound disabled or unsupported) or the sound would not start. */
bool
play_single_sound (schanid_t *chan, glui32 resno, bool looped, glui32 notify)
{
    if (!*chan)
        *chan = glk_schannel_create (0);
    if (!*chan)
        return false;
    glk_schannel_stop (*chan);
    return glk_schannel_play_ext (*chan, resno,
                                  looped ? 0xffffffff : 1, notify) != 0;
}

/* Stop whatever is playing (a no-op before the first sound). */
void
stop_single_sound (schanid_t *chan)
{
    if (*chan)
        glk_schannel_stop (*chan);
}

/* ------------------------------------------------------------- transcript */

/* Transcript metaverb matching: +1 = turn recording on, -1 = off, 0 = not a
 * transcript command (let it reach the game). */
int
match_transcript_command (const std::string &raw)
{
    std::string c = lower (trim (raw));
    if (c == "transcript" || c == "transcript on" ||
        c == "script" || c == "script on")
        return 1;
    if (c == "transcript off" || c == "script off" ||
        c == "notranscript" || c == "noscript" || c == "unscript")
        return -1;
    return 0;
}

/* Toggle transcript recording on `win`, keeping the open stream in *slot: a
 * running transcript is wired as the window's echo stream, so every line
 * printed there is copied to the file. */
void
toggle_transcript (int on, winid_t win, strid_t *slot)
{
    if (on > 0) {
        if (*slot) {
            glk_put_string ((char *) "A transcript is already being recorded.\n");
            return;
        }
        frefid_t fref = glk_fileref_create_by_prompt (
            fileusage_Transcript | fileusage_TextMode, filemode_Write, 0);
        if (!fref) {
            glk_put_string ((char *) "Transcript cancelled.\n");
            return;
        }
        *slot = glk_stream_open_file (fref, filemode_Write, 0);
        glk_fileref_destroy (fref);
        if (!*slot) {
            glk_put_string ((char *) "Could not open the transcript file.\n");
            return;
        }
        glk_window_set_echo_stream (win, *slot);
        glk_put_string ((char *) "Transcript on: game text is now being saved to a file.\n");
    } else {
        if (!*slot) {
            glk_put_string ((char *) "No transcript is being recorded.\n");
            return;
        }
        glk_put_string ((char *) "Transcript off.\n");
        glk_window_set_echo_stream (win, nullptr);
        glk_stream_close (*slot, nullptr);
        *slot = nullptr;
    }
}

/* --------------------------------------------------------- end-of-story */

/* The menu offered once the story is over.  Both frontends print the same
 * four choices in the same words; only acting on the answer differs, so the
 * caller owns the read loop and these supply the text and the matching.
 *
 * The choices are numbered because RESTART and RESTORE share a first letter:
 * abbreviating either is a trap (guess wrong and the save you meant to load
 * is gone), so a lone "r" is not accepted and the numbers give everything a
 * short form. */
/* The last thing a session prints, whether the player typed QUIT mid-game or
 * chose it at the menu above.  The leading blank line separates it from
 * whatever the game said last. */
const char QUIT_FAREWELL[] = "\nThanks for playing. Goodbye!\n";

/* The menu's UNDO offer, refused.  What makes undo unavailable differs
 * between the engines -- and so does where each frontend notices it -- but
 * the player is told the same thing either way. */
const char NOTHING_TO_UNDO[] = "There is nothing to undo.\n";

void
post_game_menu_print (void)
{
    glk_put_string ((char *) "\nThe story has ended.  You can:\n");
    glk_put_string ((char *) "  1. UNDO the last turn\n");
    glk_put_string ((char *) "  2. RESTORE a saved game\n");
    glk_put_string ((char *) "  3. RESTART\n");
    glk_put_string ((char *) "  4. QUIT\n");
}

/* Match an answer to the menu: a POSTGAME_* choice, or 0 for anything else
 * (including a bare "r", which is why restart is not matched by letter). */
int
post_game_menu_match (const std::string &raw)
{
    std::string w = lower (trim (raw));
    if (w == "1" || w == "u" || w == "undo")            return POSTGAME_UNDO;
    if (w == "2" || w == "restore" || w == "load")      return POSTGAME_RESTORE;
    if (w == "3" || w == "restart")                     return POSTGAME_RESTART;
    if (w == "4" || w == "q" || w == "quit")            return POSTGAME_QUIT;
    return 0;
}

void
post_game_menu_reprompt (void)
{
    glk_put_string ((char *)
        "Please type 1, 2, 3 or 4 (or undo / restore / restart / quit).\n");
}

/* ------------------------------------------------------------- save files */

/* Prompt for a save file and write `data` (an engine snapshot) to it.  Each
 * engine does its own (de)serialising; only the Glk file plumbing and the
 * player-facing messages live here. */
bool
prompt_write_save (const std::string &data)
{
    glui32 usage = fileusage_SavedGame | fileusage_BinaryMode;
    frefid_t fref = glk_fileref_create_by_prompt (usage, filemode_Write, 0);
    if (!fref) { glk_put_string ((char *) "Save cancelled.\n"); return false; }
    strid_t str = glk_stream_open_file (fref, filemode_Write, 0);
    glk_fileref_destroy (fref);
    if (!str) { glk_put_string ((char *) "Could not open the save file.\n"); return false; }
    glk_put_buffer_stream (str, (char *) data.data (), (glui32) data.size ());
    glk_stream_close (str, nullptr);
    glk_put_string ((char *) "Game saved.\n");
    return true;
}

/* Prompt for a save file and read it whole into `data`.  Validating the
 * bytes is the caller's job (each engine has its own snapshot format). */
bool
prompt_read_save (std::string &data)
{
    glui32 usage = fileusage_SavedGame | fileusage_BinaryMode;
    frefid_t fref = glk_fileref_create_by_prompt (usage, filemode_Read, 0);
    if (!fref) { glk_put_string ((char *) "Restore cancelled.\n"); return false; }
    strid_t str = glk_stream_open_file (fref, filemode_Read, 0);
    glk_fileref_destroy (fref);
    if (!str) { glk_put_string ((char *) "Could not open the save file.\n"); return false; }
    data.clear ();
    char buf[4096];
    glui32 n;
    while ((n = glk_get_buffer_stream (str, buf, sizeof buf)) > 0)
        data.append (buf, n);
    glk_stream_close (str, nullptr);
    return true;
}

/* -------------------------------------------------------------- resources */

#ifdef SPATTERLIGHT
/* A whole file, by path, as a Glk resource: glkimp's
 * gli_add_resource_from_path gives it a resource number (the same one for
 * the same file every time) and loads it into the backend on first use.
 * Returns 0 when the file cannot be had.  Used by the classic runner, whose
 * resources are plain files; the Quest 5 frontend resolves .quest package
 * entries itself. */
int
register_path_resource (const std::string &path, bool sound)
{
    return (int) gli_add_resource_from_path (sound ? giblorb_ID_Snd
                                                   : giblorb_ID_Pict,
                                             path.c_str (), 0, 0);
}

#else /* !SPATTERLIGHT */

/* No by-name registration without the glkimp backend, so nothing can be
 * loaded: callers see the same 0 a missing file gives them and report the
 * sound or image as unsupported.  Defined rather than #ifdef'd at each call
 * site so the frontends compile against a plain Glk (CheapGlk) too. */
int
register_path_resource (const std::string &, bool)
{
    return 0;
}

#endif /* SPATTERLIGHT */

} /* namespace questglk */
