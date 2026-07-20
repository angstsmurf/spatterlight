/* questglk-common.inc -- presentation helpers shared by the geas terp's two
   Glk frontends: geasglk.cc (the classic Quest 1-4 runner) and aslxglk.cc
   (the native Quest 5 engine).  Each frontend is its own translation unit in
   the same binary and includes this file textually, so everything here is
   `static` (internal linkage; no build-system changes).

   These are the pieces the Quest 5 frontend originally mirrored line for
   line from the classic one: the status banner, the right-hand pane and its
   divider, the transcript metaverb, the save-file prompts, and the small
   string/UTF-8 utilities.  The player-visible strings live here so the two
   frontends cannot drift apart.  Both engines' output is pinned by byte-diff
   regression suites; changes here must keep it byte-stable. */

#include <cctype>
#include <fstream>
#include <map>
#include <string>
#include <vector>

extern "C" {
#include "glk.h"
}

#ifdef SPATTERLIGHT
/* Spatterlight's by-name resource registration (glkimp).  Quest games in
 * both dialects reference image/sound files by name (e.g. "die.wav"), not by
 * numbered Blorb resource, so each file is pre-registered with the backend
 * under a synthetic resource number; glk_image_draw* / glk_schannel_play*
 * then short-circuit their PIC<n>/SND<n> lookup because win_findimage /
 * win_findsound report the number already loaded. */
extern "C" int  win_findimage (int resno);
extern "C" void win_loadimage (int resno, const char *filename, int offset, int reslen);
extern "C" int  win_findsound (int resno);
extern "C" void win_loadsound (int resno, char *filename, int offset, int reslen);
#endif

namespace questglk {

/* ---------------------------------------------------------------- strings */

static std::string
lower (std::string s)
{
    for (char &c : s)
        c = (char) tolower ((unsigned char) c);
    return s;
}

static std::string
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
static std::string
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
static glui32
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
static void
put_stream_utf8 (strid_t s, const std::string &str)
{
    for (size_t i = 0; i < str.size ();)
        glk_put_char_stream_uni (s, utf8_next_cp (str, i));
}

static size_t
utf8_cp_len (const std::string &s)
{
    size_t n = 0;
    for (size_t i = 0; i < s.size (); i++)
        if (((unsigned char) s[i] & 0xc0) != 0x80)
            n++;
    return n;
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

/* True when the whole string is well-formed UTF-8 (pure ASCII counts).  Used
 * to pick between byte (Latin-1) and codepoint-aware presentation for
 * classic-game text, whose file encoding is unknown: real Latin-1 text is
 * practically never valid UTF-8, so the check only flips a string to
 * codepoint mode when decoding it that way is right.  (Overlong forms and
 * surrogates are not rejected; this is an encoding sniff, not a validator.) */
[[maybe_unused]] static bool
utf8_valid (const std::string &s)
{
    for (size_t i = 0; i < s.size ();) {
        unsigned char c = (unsigned char) s[i];
        int len;
        if (c < 0x80) { i++; continue; }
        if ((c & 0xe0) == 0xc0) len = 2;
        else if ((c & 0xf0) == 0xe0) len = 3;
        else if ((c & 0xf8) == 0xf0) len = 4;
        else return false;
        if (i + len > s.size ())
            return false;
        for (int k = 1; k < len; k++)
            if (((unsigned char) s[i + k] & 0xc0) != 0x80)
                return false;
        i += len;
    }
    return true;
}

/* ------------------------------------------------------------------- echo */

/* Echo an accepted input line -- or a clicked hyperlink's command, run as if
 * typed -- to the current stream in the input style, ending the line.  Both
 * frontends echo manually (library line-input echo is off so a timer or a
 * click can cancel a pending request without leaving stray text). */
static void
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

/* ----------------------------------------------------------------- banner */

/* Draw the one-line status banner grid: the current room name left-aligned
 * at column 1, and the game's status variables right-aligned when both fit
 * (at least two blank cells between them, one before the right edge).
 * `utf8` selects codepoint-aware writes and measurement (Quest 5 text is
 * UTF-8); byte mode passes classic geas text through verbatim (those games
 * are typically Latin-1). */
static void
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
        glui32 slen = (glui32) (utf8 ? utf8_cp_len (status) : status.size ());
        glui32 rlen = (glui32) (utf8 ? utf8_cp_len (room) : room.size ());
        if (rlen + slen + 4 <= width) {
            glk_window_move_cursor (banner, width - slen - 1, 0);
            if (utf8)
                put_stream_utf8 (stream, status);
            else
                glk_put_string_stream (stream, (char *) status.c_str ());
        }
    }
}

/* -------------------------------------------------------------- side pane */

/* Open the right-hand pane: a 20%-proportional split of the main text
 * window, with a thin graphics window as its left child, drawn in the text
 * colour as a divider (see fill_side_divider).  No-op when already open; the
 * pane stays null when the host cannot split. */
static void
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
static void
close_side_pane_windows (winid_t *pane, winid_t *divider)
{
    if (*divider) { glk_window_close (*divider, nullptr); *divider = nullptr; }
    if (*pane)    { glk_window_close (*pane, nullptr);    *pane = nullptr; }
}

/* Paint the divider in the main window's text colour.  Graphics windows are
 * blanked on resize, so Arrange/Redraw events call this again. */
static void
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
static void
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
static void
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
static bool
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
static void
stop_single_sound (schanid_t *chan)
{
    if (*chan)
        glk_schannel_stop (*chan);
}

/* ------------------------------------------------------------- transcript */

/* Transcript metaverb matching: +1 = turn recording on, -1 = off, 0 = not a
 * transcript command (let it reach the game). */
static int
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
static void
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
enum { POSTGAME_UNDO = 1, POSTGAME_RESTORE, POSTGAME_RESTART, POSTGAME_QUIT };

static void
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
static int
post_game_menu_match (const std::string &raw)
{
    std::string w = lower (trim (raw));
    if (w == "1" || w == "u" || w == "undo")            return POSTGAME_UNDO;
    if (w == "2" || w == "restore" || w == "load")      return POSTGAME_RESTORE;
    if (w == "3" || w == "restart")                     return POSTGAME_RESTART;
    if (w == "4" || w == "q" || w == "quit")            return POSTGAME_QUIT;
    return 0;
}

static void
post_game_menu_reprompt (void)
{
    glk_put_string ((char *)
        "Please type 1, 2, 3 or 4 (or undo / restore / restart / quit).\n");
}

/* ------------------------------------------------------------- save files */

/* Prompt for a save file and write `data` (an engine snapshot) to it.  Each
 * engine does its own (de)serialising; only the Glk file plumbing and the
 * player-facing messages live here. */
static bool
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
static bool
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
/* Path-named resource -> stable Glk resource number, loading the whole file
 * into the backend on first use.  Returns 0 when the file cannot be opened
 * (the failed path keeps its number, so it is retried on the next request).
 * Used by the classic runner, whose resources are plain files; the Quest 5
 * frontend resolves .quest package entries itself. */
[[maybe_unused]] static int
register_path_resource (std::map<std::string, int> &ids,
                        const std::string &path, bool sound)
{
    auto it = ids.find (path);
    int resno = it == ids.end () ? (ids[path] = (int) ids.size () + 1)
                                 : it->second;
    if (!(sound ? win_findsound (resno) : win_findimage (resno))) {
        std::ifstream f (path.c_str (), std::ios::binary | std::ios::ate);
        if (!f.is_open ())
            return 0;
        int len = (int) f.tellg ();
        if (sound)
            win_loadsound (resno, (char *) path.c_str (), 0, len);
        else
            win_loadimage (resno, path.c_str (), 0, len);
    }
    return resno;
}
#endif /* SPATTERLIGHT */

} /* namespace questglk */
