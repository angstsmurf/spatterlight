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

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#ifdef SPATTERLIGHT
extern "C" {
#include "gi_blorb.h"
}
#endif

namespace questglk {

/* ---------------------------------------------------------------- strings */

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

/* Append one code point to a UTF-8 string. */
void
utf8_append (std::string &out, glui32 cp)
{
    if (cp < 0x80) {
        out += (char) cp;
    } else if (cp < 0x800) {
        out += (char) (0xc0 | (cp >> 6));
        out += (char) (0x80 | (cp & 0x3f));
    } else if (cp < 0x10000) {
        out += (char) (0xe0 | (cp >> 12));
        out += (char) (0x80 | ((cp >> 6) & 0x3f));
        out += (char) (0x80 | (cp & 0x3f));
    } else {
        out += (char) (0xf0 | (cp >> 18));
        out += (char) (0x80 | ((cp >> 12) & 0x3f));
        out += (char) (0x80 | ((cp >> 6) & 0x3f));
        out += (char) (0x80 | (cp & 0x3f));
    }
}

/* A buffer of code points, as a Glk Unicode line request fills one in, as
 * the UTF-8 both engines keep their text in. */
std::string
utf8_from_uni (const glui32 *buf, size_t n)
{
    std::string out;
    for (size_t i = 0; i < n; i++)
        utf8_append (out, buf[i]);
    return out;
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

/* ------------------------------------------------------------------ input */

/* Cancel the line request live in `win` and return how many characters had
 * been typed into its buffer.  Nothing may print to a window while a request
 * is live in it, so whatever interrupts one -- a timer, a hyperlink, an
 * autosave write -- cancels first, and re-requests afterwards with that many
 * characters preloaded, which puts the half-typed command back. */
glui32
cancel_line_request (winid_t win)
{
    event_t ce;
    ce.val1 = 0;
    glk_cancel_line_event (win, &ce);
    return ce.val1;
}

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

/* Glk styles don't combine, so each run of text prints in the one style that
 * stands for its whole attribute state.  Which states get a style of their
 * own is decided per game, before the main window opens, the way Scarier
 * does it (terps/scarier/glk/os_glk_styles.cpp): the frontend counts the
 * states the game's text uses (a StyleCensus), the states the old fixed
 * mapping already gave a home keep it, and the rest are handed the styles
 * the game leaves free, hinted to look like the state they stand for.  Text
 * in a state without a style falls back to the nearest state that has one,
 * by dropping attributes, never adding them. */

/* Two points to a step, truncating toward the default, so a point either
 * side of it is still the default size; the clamp is the range a Size hint
 * can sensibly ask for.  Alignment is one state; right wins over centre. */
StyleCombo
style_combo (bool bold, bool italic, bool underlined, bool monospaced,
             bool center, bool right, double points, double default_points)
{
    StyleCombo combo = 0;
    if (bold)
        combo |= COMBO_BOLD;
    if (italic || underlined)
        combo |= COMBO_ITALIC;
    if (monospaced)
        combo |= COMBO_MONOSPACED;
    if (right)
        combo |= COMBO_RIGHT;
    else if (center)
        combo |= COMBO_CENTER;

    /* The nearest step of two points; a size exactly between two steps
     * (an odd Quest 4 size) takes the one nearer the default. */
    double half_steps = (points - default_points) / 2;
    int step = (int) (fabs (half_steps) + 0.5 - 1e-9);
    if (half_steps < 0)
      step = -step;
    step = std::max<int> (STYLE_MIN_STEP, std::min<int> (STYLE_MAX_STEP, step));
    combo |= ((StyleCombo) step << COMBO_SIZE_SHIFT) & COMBO_SIZE_MASK;
    return combo;
}

int
style_combo_step (StyleCombo combo)
{
    return (signed char) ((combo & COMBO_SIZE_MASK) >> COMBO_SIZE_SHIFT);
}

/* The state spelled out for a report: "bold+centre size+3". */
std::string
style_combo_describe (StyleCombo combo)
{
    static const struct { StyleCombo bit; const char *name; } names[] = {
        { COMBO_BOLD, "bold" }, { COMBO_ITALIC, "italic" },
        { COMBO_MONOSPACED, "mono" }, { COMBO_CENTER, "centre" },
        { COMBO_RIGHT, "right" }
    };
    std::string out;
    for (const auto &n : names)
        if (combo & n.bit)
            out += (out.empty () ? "" : "+") + std::string (n.name);
    int step = style_combo_step (combo);
    if (step != 0) {
        char buf[16];
        snprintf (buf, sizeof buf, "%ssize%+d", out.empty () ? "" : " ", step);
        out += buf;
    }
    return out.empty () ? "plain" : out;
}

const char *
style_name (int style)
{
    static const char *const names[STYLE_COUNT] = {
        "Normal", "Emphasized", "Preformatted", "Header", "Subheader", "Alert",
        "Note", "BlockQuote", "Input", "User1", "User2"
    };
    return style >= 0 && style < STYLE_COUNT ? names[style] : "?";
}

/* Counted in visible characters, so one big title weighs what its words do
 * rather than what its markup does. */
void
StyleCensus::count (StyleCombo combo, const std::string &utf8, bool intro)
{
    unsigned long visible = 0;
    for (unsigned char c : utf8)
        if ((c & 0xc0) != 0x80 && c != ' ' && c != '\t' && c != '\n' &&
            c != '\r')
            visible++;
    if (visible == 0)
        return;
    for (StyleCensusRow &row : rows_)
        if (row.combo == combo) {
            row.characters += visible;
            if (intro)
                row.intro_characters += visible;
            return;
        }
    rows_.push_back ({ combo, visible, intro ? visible : 0 });
}

/* The title and introduction are what a player sees first, so a character
 * there counts twenty times; ties go to the simpler state, so a run of
 * sized italic doesn't take a style ahead of the plain size. */
static const unsigned long STYLE_INTRO_WEIGHT = 20;

static unsigned long
style_row_weight (const StyleCensusRow &row)
{
    return row.characters + STYLE_INTRO_WEIGHT * row.intro_characters;
}

static unsigned
style_combo_bits (StyleCombo combo)
{
    unsigned bits = style_combo_step (combo) != 0;
    for (combo &= COMBO_FLAGS; combo != 0; combo &= combo - 1)
        bits++;
    return bits;
}

std::vector<StyleCensusRow>
StyleCensus::ranked () const
{
    std::vector<StyleCensusRow> rows = rows_;
    std::sort (rows.begin (), rows.end (),
               [] (const StyleCensusRow &a, const StyleCensusRow &b) {
                   unsigned long aw = style_row_weight (a);
                   unsigned long bw = style_row_weight (b);
                   if (aw != bw)
                       return aw > bw;
                   unsigned abits = style_combo_bits (a.combo);
                   unsigned bbits = style_combo_bits (b.combo);
                   if (abits != bbits)
                       return abits < bbits;
                   return a.combo < b.combo;
               });
    return rows;
}

/* The assignment the old fixed mapping amounts to, with homes for the
 * alignments it never printed. */
void
style_table_natural (StyleTable &table)
{
    static const struct { int style; StyleCombo combo; } naturals[] = {
        { style_Normal, 0 },
        { style_Emphasized, COMBO_ITALIC },
        { style_Preformatted, COMBO_MONOSPACED },
        { style_Subheader, COMBO_BOLD },
        { style_Alert, COMBO_BOLD | COMBO_ITALIC },
        { style_Note, COMBO_RIGHT },
        { style_User1, COMBO_CENTER },
        { style_User2, COMBO_CENTER | COMBO_BOLD }
    };
    for (int i = 0; i < STYLE_COUNT; i++) {
        table.assigned[i] = false;
        table.natural[i] = true;
        table.combo[i] = 0;
    }
    for (const auto &n : naturals) {
        table.assigned[n.style] = true;
        table.combo[n.style] = n.combo;
    }
}

static int
style_find (const StyleTable &table, StyleCombo combo)
{
    for (int style = 0; style < STYLE_COUNT; style++)
        if (table.assigned[style] && table.combo[style] == combo)
            return style;
    return -1;
}

/* A weight aligned text counts against the state it would evict. */
static const unsigned long STYLE_ALIGNMENT_WEIGHT = 4;

/* What a frontend that ignores hints draws a style in: Spatterlight with
 * game styles off and Gargoyle's defaults agree that Header is bold, Alert
 * bold italic and the rest plain.  Size and alignment are lost either way. */
static StyleCombo
style_default_look (int style)
{
    switch (style) {
    case style_Header: return COMBO_BOLD;
    case style_Alert: return COMBO_BOLD | COMBO_ITALIC;
    default: return 0;
    }
}

/* How far a state drawn in a spare's default look is from itself.  An
 * attribute the look adds costs more than one it loses, as the fallback
 * chain never adds one either. */
static unsigned
style_spare_mismatch (int style, StyleCombo combo)
{
    StyleCombo look = style_default_look (style);
    StyleCombo want = combo & (COMBO_BOLD | COMBO_ITALIC);
    return 2 * style_combo_bits (look & ~want) + style_combo_bits (want & ~look);
}

/* Start from the natural table, then give each ranked state without a style
 * of its own a spare: Header and BlockQuote, which neither frontend prints,
 * and the natural styles whose state the game never shows.  Emphasized,
 * Preformatted and Subheader are never spares -- the frontends print their
 * own notes, command lists and pane headings in them -- and Normal and Input
 * are what they are.
 *
 * Each state takes the spare whose default look is nearest its own, so a
 * frontend that ignores the hints still draws a bold title bold (Header)
 * and a plain one plain (BlockQuote, User1...), losing only the centring
 * and the size.  Ties go in the order above.
 *
 * Handing out User1, User2 or Note can leave an alignment the game uses
 * with no style at all (a game whose centred text is all bold and sized
 * shows no plain centre), and losing the alignment moves text where losing
 * a size or slant only restyles it.  So then the last state handed a spare
 * gives it up to the plain alignment, provided the aligned text weighs at
 * least a quarter of that state's. */
void
style_assign (const std::vector<StyleCensusRow> &ranked, StyleTable &table)
{
    static const int natural_spares[] = { style_User1, style_User2,
                                          style_Note, style_Alert };
    static const struct { StyleCombo combo; int style; } alignments[] = {
        { COMBO_CENTER, style_User1 }, { COMBO_RIGHT, style_Note }
    };

    style_table_natural (table);

    std::vector<int> pool = { style_Header, style_BlockQuote };
    for (int style : natural_spares) {
        bool used = false;
        for (const StyleCensusRow &row : ranked)
            if (row.combo == table.combo[style])
                used = true;
        if (!used)
            pool.push_back (style);
    }

    /* The spares handed out, in order. */
    std::vector<int> handed;
    for (size_t i = 0; i < ranked.size () && !pool.empty (); i++) {
        if (style_find (table, ranked[i].combo) >= 0)
            continue;
        size_t best = 0;
        for (size_t p = 1; p < pool.size (); p++)
            if (style_spare_mismatch (pool[p], ranked[i].combo)
                < style_spare_mismatch (pool[best], ranked[i].combo))
                best = p;
        int style = pool[best];
        pool.erase (pool.begin () + (long) best);
        handed.push_back (style);
        table.assigned[style] = true;
        table.natural[style] = false;
        table.combo[style] = ranked[i].combo;
    }

    for (const auto &a : alignments) {
        if (handed.empty ())
            break;
        int style;
        for (style = 0; style < STYLE_COUNT; style++)
            if (table.assigned[style] && (table.combo[style] & a.combo))
                break;
        if (style < STYLE_COUNT)
            continue;
        unsigned long aligned = 0, evicted = 0;
        for (const StyleCensusRow &row : ranked) {
            if (row.combo & a.combo)
                aligned += style_row_weight (row);
            if (row.combo == table.combo[handed.back ()])
                evicted = style_row_weight (row);
        }
        if (aligned == 0 || STYLE_ALIGNMENT_WEIGHT * aligned < evicted)
            continue;

        /* The last state handed a spare loses it.  If the alignment's own
         * natural style went to a state ranked above that one, the state
         * moves to the freed spare and the natural style takes its old
         * hints back. */
        style = handed.back ();
        handed.pop_back ();
        for (int &earlier : handed)
            if (earlier == a.style) {
                table.combo[style] = table.combo[a.style];
                earlier = style;
                style = a.style;
            }
        table.combo[style] = a.combo;
        table.natural[style] = style == a.style;
    }
}

/* Each try drops attributes and none adds one, so text never shows an
 * attribute it didn't ask for.  The tries go by what they cost: italic and
 * bold one each, then the alignment, worth more than both together since
 * losing it moves the text, then the face, worth more than all of those.
 * Under each set of attributes the size steps toward the default, so a size
 * the game uses a little of lands on a near size it uses more.  Normal is
 * always assigned, so the walk ends there at the latest. */
int
style_lookup (const StyleTable &table, StyleCombo combo)
{
    static const struct { StyleCombo flags; unsigned cost; } drops[] = {
        { COMBO_ITALIC, 1 }, { COMBO_BOLD, 1 },
        { COMBO_CENTER | COMBO_RIGHT, 3 }, { COMBO_MONOSPACED, 6 }
    };
    static const size_t DROPS = sizeof drops / sizeof drops[0];
    static const unsigned DROP_SETS = 1u << DROPS, MAX_COST = 11;
    const StyleCombo flags = combo & COMBO_FLAGS;
    const int size = style_combo_step (combo);

    for (unsigned cost = 0; cost <= MAX_COST; cost++)
        for (unsigned set = 0; set < DROP_SETS; set++) {
            StyleCombo kept = flags;
            unsigned set_cost = 0;
            for (size_t i = 0; i < DROPS; i++)
                if (set & (1u << i)) {
                    kept &= ~drops[i].flags;
                    set_cost += drops[i].cost;
                }
            if (set_cost != cost)
                continue;
            for (int step = size; ; step += (step < 0) - (step > 0)) {
                int style = style_find (
                    table, kept | (((StyleCombo) step << COMBO_SIZE_SHIFT) &
                                   COMBO_SIZE_MASK));
                if (style >= 0)
                    return style;
                if (step == 0)
                    break;
            }
        }
    return style_Normal;
}

static StyleTable &
installed_table ()
{
    static StyleTable table = [] {
        StyleTable t;
        style_table_natural (t);
        return t;
    } ();
    return table;
}

void
style_install (const StyleTable &table)
{
    installed_table () = table;
}

const StyleTable &
style_installed ()
{
    return installed_table ();
}

/* The natural alignments get their justification; Header and Subheader are
 * pinned left so a theme that centres them doesn't centre bold text.  A
 * style the game's assignment took over gets every hint that shapes it, so
 * nothing of the theme's own look for it (a centred Header, an indented
 * BlockQuote) survives.  Size is relative to the style's own font, which in
 * most themes is the normal one.  Hints apply per window type, so the
 * buffer side panes see them too; they print only in Normal and Subheader,
 * which keep their looks. */
void
style_set_hints ()
{
    const StyleTable &table = installed_table ();

    if (table.natural[style_User1])
        glk_stylehint_set (wintype_TextBuffer, style_User1,
                           stylehint_Justification, stylehint_just_Centered);
    if (table.natural[style_User2]) {
        glk_stylehint_set (wintype_TextBuffer, style_User2,
                           stylehint_Justification, stylehint_just_Centered);
        glk_stylehint_set (wintype_TextBuffer, style_User2, stylehint_Weight, 1);
    }
    if (table.natural[style_Note])
        glk_stylehint_set (wintype_TextBuffer, style_Note,
                           stylehint_Justification, stylehint_just_RightFlush);
    if (table.natural[style_Header])
        glk_stylehint_set (wintype_TextBuffer, style_Header,
                           stylehint_Justification, stylehint_just_LeftFlush);
    glk_stylehint_set (wintype_TextBuffer, style_Subheader,
                       stylehint_Justification, stylehint_just_LeftFlush);

    for (glui32 style = 0; style < style_NUMSTYLES; style++) {
        if (table.natural[style])
            continue;
        StyleCombo combo = table.combo[style];
        glk_stylehint_set (wintype_TextBuffer, style, stylehint_Weight,
                           (combo & COMBO_BOLD) ? 1 : 0);
        glk_stylehint_set (wintype_TextBuffer, style, stylehint_Oblique,
                           (combo & COMBO_ITALIC) ? 1 : 0);
        glk_stylehint_set (wintype_TextBuffer, style, stylehint_Proportional,
                           (combo & COMBO_MONOSPACED) ? 0 : 1);
        glk_stylehint_set (wintype_TextBuffer, style, stylehint_Justification,
                           (combo & COMBO_CENTER) ? stylehint_just_Centered
                           : (combo & COMBO_RIGHT) ? stylehint_just_RightFlush
                           : stylehint_just_LeftFlush);
        glk_stylehint_set (wintype_TextBuffer, style, stylehint_Size,
                           (glsi32) style_combo_step (combo));
        glk_stylehint_set (wintype_TextBuffer, style, stylehint_Indentation, 0);
        glk_stylehint_set (wintype_TextBuffer, style,
                           stylehint_ParaIndentation, 0);
    }

    glk_stylehint_set (wintype_TextGrid, style_User1, stylehint_ReverseColor, 1);
}

glui32
quest_style_for (StyleCombo combo)
{
    return (glui32) style_lookup (installed_table (), combo);
}

/* ----------------------------------------------------------------- colour */

/* The CSS3 extended colour keywords: what a browser, and so Quest 5's own
 * player, accepts for a colour name. */
static const struct { const char *name; glui32 rgb; } kCssColours[] = {
    {"aliceblue", 0xF0F8FF}, {"antiquewhite", 0xFAEBD7}, {"aqua", 0x00FFFF},
    {"aquamarine", 0x7FFFD4}, {"azure", 0xF0FFFF}, {"beige", 0xF5F5DC},
    {"bisque", 0xFFE4C4}, {"black", 0x000000}, {"blanchedalmond", 0xFFEBCD},
    {"blue", 0x0000FF}, {"blueviolet", 0x8A2BE2}, {"brown", 0xA52A2A},
    {"burlywood", 0xDEB887}, {"cadetblue", 0x5F9EA0}, {"chartreuse", 0x7FFF00},
    {"chocolate", 0xD2691E}, {"coral", 0xFF7F50},
    {"cornflowerblue", 0x6495ED}, {"cornsilk", 0xFFF8DC},
    {"crimson", 0xDC143C}, {"cyan", 0x00FFFF}, {"darkblue", 0x00008B},
    {"darkcyan", 0x008B8B}, {"darkgoldenrod", 0xB8860B},
    {"darkgray", 0xA9A9A9}, {"darkgreen", 0x006400}, {"darkgrey", 0xA9A9A9},
    {"darkkhaki", 0xBDB76B}, {"darkmagenta", 0x8B008B},
    {"darkolivegreen", 0x556B2F}, {"darkorange", 0xFF8C00},
    {"darkorchid", 0x9932CC}, {"darkred", 0x8B0000},
    {"darksalmon", 0xE9967A}, {"darkseagreen", 0x8FBC8F},
    {"darkslateblue", 0x483D8B}, {"darkslategray", 0x2F4F4F},
    {"darkslategrey", 0x2F4F4F}, {"darkturquoise", 0x00CED1},
    {"darkviolet", 0x9400D3}, {"deeppink", 0xFF1493},
    {"deepskyblue", 0x00BFFF}, {"dimgray", 0x696969}, {"dimgrey", 0x696969},
    {"dodgerblue", 0x1E90FF}, {"firebrick", 0xB22222},
    {"floralwhite", 0xFFFAF0}, {"forestgreen", 0x228B22},
    {"fuchsia", 0xFF00FF}, {"gainsboro", 0xDCDCDC}, {"ghostwhite", 0xF8F8FF},
    {"gold", 0xFFD700}, {"goldenrod", 0xDAA520}, {"gray", 0x808080},
    {"green", 0x008000}, {"greenyellow", 0xADFF2F}, {"grey", 0x808080},
    {"honeydew", 0xF0FFF0}, {"hotpink", 0xFF69B4}, {"indianred", 0xCD5C5C},
    {"indigo", 0x4B0082}, {"ivory", 0xFFFFF0}, {"khaki", 0xF0E68C},
    {"lavender", 0xE6E6FA}, {"lavenderblush", 0xFFF0F5},
    {"lawngreen", 0x7CFC00}, {"lemonchiffon", 0xFFFACD},
    {"lightblue", 0xADD8E6}, {"lightcoral", 0xF08080},
    {"lightcyan", 0xE0FFFF}, {"lightgoldenrodyellow", 0xFAFAD2},
    {"lightgray", 0xD3D3D3}, {"lightgreen", 0x90EE90},
    {"lightgrey", 0xD3D3D3}, {"lightpink", 0xFFB6C1},
    {"lightsalmon", 0xFFA07A}, {"lightseagreen", 0x20B2AA},
    {"lightskyblue", 0x87CEFA}, {"lightslategray", 0x778899},
    {"lightslategrey", 0x778899}, {"lightsteelblue", 0xB0C4DE},
    {"lightyellow", 0xFFFFE0}, {"lime", 0x00FF00}, {"limegreen", 0x32CD32},
    {"linen", 0xFAF0E6}, {"magenta", 0xFF00FF}, {"maroon", 0x800000},
    {"mediumaquamarine", 0x66CDAA}, {"mediumblue", 0x0000CD},
    {"mediumorchid", 0xBA55D3}, {"mediumpurple", 0x9370DB},
    {"mediumseagreen", 0x3CB371}, {"mediumslateblue", 0x7B68EE},
    {"mediumspringgreen", 0x00FA9A}, {"mediumturquoise", 0x48D1CC},
    {"mediumvioletred", 0xC71585}, {"midnightblue", 0x191970},
    {"mintcream", 0xF5FFFA}, {"mistyrose", 0xFFE4E1}, {"moccasin", 0xFFE4B5},
    {"navajowhite", 0xFFDEAD}, {"navy", 0x000080}, {"oldlace", 0xFDF5E6},
    {"olive", 0x808000}, {"olivedrab", 0x6B8E23}, {"orange", 0xFFA500},
    {"orangered", 0xFF4500}, {"orchid", 0xDA70D6},
    {"palegoldenrod", 0xEEE8AA}, {"palegreen", 0x98FB98},
    {"paleturquoise", 0xAFEEEE}, {"palevioletred", 0xDB7093},
    {"papayawhip", 0xFFEFD5}, {"peachpuff", 0xFFDAB9}, {"peru", 0xCD853F},
    {"pink", 0xFFC0CB}, {"plum", 0xDDA0DD}, {"powderblue", 0xB0E0E6},
    {"purple", 0x800080}, {"rebeccapurple", 0x663399}, {"red", 0xFF0000},
    {"rosybrown", 0xBC8F8F}, {"royalblue", 0x4169E1},
    {"saddlebrown", 0x8B4513}, {"salmon", 0xFA8072},
    {"sandybrown", 0xF4A460}, {"seagreen", 0x2E8B57}, {"seashell", 0xFFF5EE},
    {"sienna", 0xA0522D}, {"silver", 0xC0C0C0}, {"skyblue", 0x87CEEB},
    {"slateblue", 0x6A5ACD}, {"slategray", 0x708090},
    {"slategrey", 0x708090}, {"snow", 0xFFFAFA}, {"springgreen", 0x00FF7F},
    {"steelblue", 0x4682B4}, {"tan", 0xD2B48C}, {"teal", 0x008080},
    {"thistle", 0xD8BFD8}, {"tomato", 0xFF6347}, {"turquoise", 0x40E0D0},
    {"violet", 0xEE82EE}, {"wheat", 0xF5DEB3}, {"white", 0xFFFFFF},
    {"whitesmoke", 0xF5F5F5}, {"yellow", 0xFFFF00},
    {"yellowgreen", 0x9ACD32},
};

static int
hex_digit (char c)
{
    return c >= '0' && c <= '9' ? c - '0'
         : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

/* Three or six hex digits as a colour; COLOUR_NONE for anything else.
 * `s' is lowercased. */
static glui32
hex_colour (const std::string &s)
{
    if (s.size () != 3 && s.size () != 6)
        return COLOUR_NONE;
    glui32 v = 0;
    for (char c : s) {
        int d = hex_digit (c);
        if (d < 0)
            return COLOUR_NONE;
        v = (v << 4) | (glui32) d;
    }
    if (s.size () == 3) {  /* #rgb -> #rrggbb */
        glui32 r = (v >> 8) & 0xf, g = (v >> 4) & 0xf, b = v & 0xf;
        return (r * 0x11 << 16) | (g * 0x11 << 8) | (b * 0x11);
    }
    return v;
}

/* A CSS colour -- a keyword, #rgb, #rrggbb or rgb(r, g, b) -- as 0xRRGGBB,
 * or COLOUR_NONE for anything a browser would ignore ("inherit" and
 * "transparent" among them: neither names a colour of its own). */
glui32
css_colour (const std::string &spec)
{
    std::string s;
    for (char c : spec)
        if (!isspace ((unsigned char) c))
            s += (char) tolower ((unsigned char) c);
    if (s.empty ())
        return COLOUR_NONE;
    if (s[0] == '#')
        return hex_colour (s.substr (1));
    if (s.compare (0, 4, "rgb(") == 0 || s.compare (0, 5, "rgba(") == 0) {
        size_t at = s.find ('(') + 1;
        glui32 v = 0;
        for (int n = 0; n < 3; n++) {
            char *end;
            long c = strtol (s.c_str () + at, &end, 10);
            size_t used = (size_t) (end - (s.c_str () + at));
            if (used == 0)
                return COLOUR_NONE;
            at += used;
            if (at < s.size () && s[at] == '%') {
                c = c * 255 / 100;
                at++;
            }
            v = (v << 8) | (glui32) std::min (std::max (c, 0L), 255L);
            if (n < 2 && (at >= s.size () || s[at++] != ','))
                return COLOUR_NONE;
        }
        return v;
    }
    for (const auto &e : kCssColours)
        if (s == e.name)
            return e.rgb;
    return COLOUR_NONE;
}

/* A Quest 4 `background' or `foreground' colour, read the way Quest's own
 * GetHTMLColour reads it (V4Game.cs:3532): the empty colour and "0" are the
 * default (`default_rgb'), six names have values of their own, and anything
 * else is handed to the page as "#" and the text, which a browser takes
 * only as three or six hex digits.  COLOUR_NONE for the rest -- "navy"
 * becomes "#navy", which the page ignores. */
glui32
quest4_colour (const std::string &spec, glui32 default_rgb)
{
    std::string s = lower (trim (spec));
    if (s.empty () || s == "0")
        return default_rgb;
    static const struct { const char *name; glui32 rgb; } names[] = {
        {"white", 0xFFFFFF}, {"black", 0x000000}, {"blue", 0x0000FF},
        {"yellow", 0xFFFF00}, {"red", 0xFF0000}, {"green", 0x00FF00},
    };
    for (const auto &e : names)
        if (s == e.name)
            return e.rgb;
    if (s[0] == '#')
        s.erase (0, 1);
    return hex_colour (s);
}

/* Whether text in one colour can be read against another: the WCAG
 * contrast ratio of the two relative luminances, each lifted by 0.05,
 * against a 3:1 floor, the threshold for large or decorative text.
 *
 * Luminance squares each channel rather than applying the sRGB curve, which
 * keeps it in integers (on a 255*255 scale) and changes no verdict over the
 * common colours: red, green, blue, teal, navy and grey are legible on
 * white, while white, silver, yellow, lime, cyan, orange and pink are not.
 * Scarier's gsc_colour_legible, unchanged. */
static glui32
colour_luminance (glui32 rgb)
{
    const glui32 r = (rgb >> 16) & 0xff, g = (rgb >> 8) & 0xff, b = rgb & 0xff;
    return (2126 * r * r + 7152 * g * g + 722 * b * b) / 10000;
}

bool
colour_legible (glui32 fg, glui32 bg)
{
    /* 0.05 of the 255*255 scale. */
    static const glui32 FLARE = 3251;
    const glui32 one = colour_luminance (fg) + FLARE,
                 two = colour_luminance (bg) + FLARE;
    return std::max (one, two) >= 3 * std::min (one, two);
}

ColourMode g_colour;

/* The background `win' draws Normal text on, as the library reports it, or
 * white when it cannot say: an interpreter that answers nothing is likeliest
 * to be showing the light page Glk defaults to. */
glui32
measure_background (winid_t win)
{
    glui32 bg;
    if (win && glk_style_measure (win, style_Normal, stylehint_BackColor, &bg))
        return bg;
    return 0xFFFFFF;
}

/* The main window's Normal colours, as the library measures them: the
 * palette while its hints are honoured, the theme while they are ignored.
 * Cached, since under Spatterlight each measure is a round trip to the
 * application; the answer only changes when the mode toggles or the
 * library's style preference flips, which arrives as an Arrange event, and
 * both reset the cache.  -1 not measured, 0 cannot measure, 1 measured. */
static int g_measure_state = -1;
static glui32 g_measured_fg, g_measured_bg;

bool
normal_measure (winid_t main, glui32 *fg, glui32 *bg)
{
    if (g_measure_state < 0) {
        g_measure_state = main
            && glk_style_measure (main, style_Normal, stylehint_TextColor,
                                  &g_measured_fg)
            && glk_style_measure (main, style_Normal, stylehint_BackColor,
                                  &g_measured_bg);
    }
    if (g_measure_state == 0)
        return false;
    *fg = g_measured_fg;
    *bg = g_measured_bg;
    return true;
}

void
normal_measure_reset ()
{
    g_measure_state = -1;
}

/* Whether the mode is on *and* the library is drawing the palette.
 * Spatterlight's "Games can set colors and styles" and Gargoyle's stylehint
 * preference both leave the windows in the theme while accepting every
 * hint and zcolor; then a colour standing in for a style (the status bar's
 * inversion) has to give way to the style.  A library that cannot measure
 * is believed. */
bool
colour_visible (winid_t main)
{
    if (!g_colour.on)
        return false;
    glui32 fg, bg;
    if (!normal_measure (main, &fg, &bg))
        return true;
    return fg == g_colour.hint_fg && bg == g_colour.hint_bg;
}

/* Publish or withdraw the palette as Normal hints, for the windows opened
 * after: libraries snapshot hints when a window opens. */
void
colour_set_normal_hints (bool on)
{
    g_colour.hint_fg = on ? g_colour.foreground : COLOUR_NONE;
    g_colour.hint_bg = on ? g_colour.background : COLOUR_NONE;
    for (glui32 type : {wintype_TextBuffer, wintype_TextGrid}) {
        if (on) {
            glk_stylehint_set (type, style_Normal, stylehint_TextColor,
                               (glsi32) g_colour.foreground);
            glk_stylehint_set (type, style_Normal, stylehint_BackColor,
                               (glsi32) g_colour.background);
        } else {
            glk_stylehint_clear (type, style_Normal, stylehint_TextColor);
            glk_stylehint_clear (type, style_Normal, stylehint_BackColor);
        }
    }
}

/* The half of starting in colour that comes before the first window opens:
 * the hints, so the windows snapshot them.  Turning the mode on once they
 * exist is the frontend's (its colour_on). */
void
colour_startup_prepare ()
{
#ifdef QUESTGLK_HAVE_ZCOLORS
    if (g_colour.startup) {
        colour_set_normal_hints (true);
        g_colour.hints_preapplied = true;
    }
#endif
}

/* Set the colours later text written to `win' comes out in: `fg', or the
 * game's foreground for COLOUR_NONE, over the game's background.  Does
 * nothing while the mode is off, so callers call it unconditionally. */
void
colour_apply ([[maybe_unused]] winid_t win, [[maybe_unused]] glui32 fg,
              [[maybe_unused]] winid_t main)
{
#ifdef QUESTGLK_HAVE_ZCOLORS
    if (!g_colour.on || !win)
        return;
    if (fg == COLOUR_NONE)
        fg = g_colour.foreground;
    if (win == main)
        g_colour.main_fg = fg;
    garglk_set_zcolors_stream (glk_window_get_stream (win), fg,
                               g_colour.background);
#endif
}

/* Take a text window into the palette, or hand it back to the theme.  Both
 * end in a clear: a window keeps the background its last clear gave it, and
 * nothing else repaints it.  Handing back takes three steps -- drop to
 * Default so no zcolor is live, clear in the theme's measured background,
 * and drop to Default again so later text follows the theme. */
void
colour_repaint ([[maybe_unused]] winid_t win, [[maybe_unused]] bool on)
{
#ifdef QUESTGLK_HAVE_ZCOLORS
    if (!win)
        return;
    strid_t stream = glk_window_get_stream (win);
    if (on) {
        garglk_set_zcolors_stream (stream, g_colour.foreground,
                                   g_colour.background);
        glk_window_clear (win);
    } else {
        glui32 bg;
        garglk_set_zcolors_stream (stream, zcolor_Default, zcolor_Default);
        if (glk_style_measure (win, style_Normal, stylehint_BackColor, &bg))
            garglk_set_zcolors_stream (stream, zcolor_Default, bg);
        glk_window_clear (win);
        garglk_set_zcolors_stream (stream, zcolor_Default, zcolor_Default);
    }
#endif
}

bool
colour_toggle_begin (bool on)
{
    g_colour.on = on;
    bool preapplied = g_colour.hints_preapplied && on;
    if (!preapplied)
        colour_set_normal_hints (on);
    normal_measure_reset ();
    bool rebuild = g_colour.main_win && !preapplied;
    g_colour.hints_preapplied = false;
    g_colour.main_fg = COLOUR_NONE;
    return rebuild;
}

void
colour_set_background (glui32 bg, bool main_empty, glui32 span_fg)
{
    if (bg == COLOUR_NONE)
        return;
    g_colour.background = bg;
    if (!g_colour.on)
        return;
    if (main_empty)
        colour_repaint (g_colour.main_win, true);
    colour_apply (g_colour.main_win, span_fg, g_colour.main_win);
}

void
colour_blob_put (std::string &b)
{
    blob_num (b, g_colour.on ? 1 : 0);
    blob_num (b, (long) g_colour.background);
    blob_num (b, (long) g_colour.foreground);
    blob_num (b, (long) g_colour.link);
    blob_num (b, (long) g_colour.main_fg);
}

void
colour_blob_get (BlobReader &r)
{
    bool on = r.num () != 0;
    g_colour.background = (glui32) r.num ();
    g_colour.foreground = (glui32) r.num ();
    g_colour.link = (glui32) r.num ();
    g_colour.main_fg = (glui32) r.num ();
    g_colour.on = on;
    g_colour.startup = g_colour.hints_preapplied = false;
    colour_set_normal_hints (on);
    normal_measure_reset ();
}

void
colour_restored ()
{
    colour_apply (g_colour.main_win, g_colour.main_fg, g_colour.main_win);
}

void
colour_status_begin ([[maybe_unused]] strid_t stream)
{
#ifdef QUESTGLK_HAVE_ZCOLORS
    if (colour_visible (g_colour.main_win)) {
        glk_set_style_stream (stream, style_Normal);
        garglk_set_zcolors_stream (stream, g_colour.background,
                                   g_colour.foreground);
        return;
    }
#endif
    glk_set_style_stream (stream, style_User1);
}

void
colour_status_end ()
{
    if (g_colour.on)
        colour_apply (g_colour.main_win, g_colour.main_fg, g_colour.main_win);
}

int
match_colour_command (const std::string &raw)
{
    std::string c = lower (trim (raw));
    if (!c.empty () && c[0] == '#')
        c.erase (0, 1);
    for (const char *word : {"colours", "colors", "colour", "color"}) {
        std::string w = word;
        if (c == w)
            return kColourReport;
        if (c == w + " on")
            return kColourOn;
        if (c == w + " off")
            return kColourOff;
    }
    return 0;
}

void
run_colour_command ([[maybe_unused]] int which,
                    [[maybe_unused]] void (*set) (bool on))
{
#ifdef QUESTGLK_HAVE_ZCOLORS
    if (which != kColourReport && (which == kColourOn) != g_colour.on)
        set (which == kColourOn);
    if (g_colour.on)
        glk_put_string ((char *) "\nThe game's own colours are on: text is"
                        " drawn in the colours it was written for.  COLOURS"
                        " OFF hands it back to the interpreter's theme.\n");
    else
        glk_put_string ((char *) "\nThe game's own colours are off: text"
                        " follows the interpreter's theme.  COLOURS ON draws"
                        " it in the colours the game asks for.\n");
#else
    glk_put_string ((char *) "\nThe game's own colours are not available"
                    " with this interpreter.\n");
#endif
}

/* Rules after the first only matter to the census; one firing per rule and
 * per detail is plenty there. */
void
ColourDetect::fire (const std::string &rule, const std::string &detail)
{
    for (const Fired &f : fired)
        if (f.rule == rule && f.detail == detail)
            return;
    if (fired.size () < 16)
        fired.push_back ({ rule, detail });
}

/* A `define game' colour other than Quest's white page and black text says
 * the game was written for its own page.  Values the real runner would hand
 * the browser as junk (`#navy') count for nothing. */
void
ColourDetect::quest4_header (const std::string &bg, const std::string &fg)
{
    game_bg = quest4_colour (bg, 0xFFFFFF);
    game_fg = quest4_colour (fg, 0x000000);
    if (game_bg == COLOUR_NONE)
        game_bg = 0xFFFFFF;
    if (game_fg == COLOUR_NONE)
        game_fg = 0x000000;
    if (game_bg != 0xFFFFFF)
        fire ("header", "background <" + bg + ">");
    if (game_fg != 0x000000)
        fire ("header", "foreground <" + fg + ">");
}

/* So does a background statement that would repaint the page in play,
 * wherever it is: the scan cannot tell whether it runs.  A foreground one
 * only colours the text after it, so it is judged as coloured text.  One
 * that names the declared colour again changes nothing. */
void
ColourDetect::quest4_statement (bool background, const std::string &value,
                                const std::string &block)
{
    glui32 c = quest4_colour (value, background ? 0xFFFFFF : 0x000000);
    if (c == COLOUR_NONE || c == (background ? game_bg : game_fg))
        return;
    std::string what = std::string (background ? "background" : "foreground")
                       + " <" + value + "> in " + block;
    if (background)
        fire ("statement", what);
    else
        span (c, what);
}

/* Quest 5's Core defaults are the same page with blue links; a theme_*
 * type or the game's own attributes changing the page or its text is the
 * header rule again.  Parsed, so that `white' and `#FFFFFF' are the same.
 * A link colour of its own (often black, to hide the links) is drawn in
 * colour mode but does not ask for it: the page is still Quest's. */
void
ColourDetect::quest5_defaults (const std::string &bg, const std::string &fg,
                               const std::string &link)
{
    static const struct { glui32 def; const char *name; } slot[] = {
        {0xFFFFFF, "defaultbackground"}, {0x000000, "defaultforeground"},
        {0x0000FF, "defaultlinkforeground"},
    };
    const std::string *spec[] = { &bg, &fg, &link };
    glui32 *out[] = { &game_bg, &game_fg, &game_link };
    for (int k = 0; k < 3; k++) {
        glui32 c = css_colour (*spec[k]);
        *out[k] = c == COLOUR_NONE ? slot[k].def : c;
        if (k < 2 && *out[k] != slot[k].def)
            fire ("header", std::string (slot[k].name) + " " + *spec[k]);
    }
}

/* A call that changes the colours in play, judged like the Quest 4
 * statements: a background one fires, a foreground one with a literal is
 * coloured text.  Core's own, which hand it the defaults (InitInterface),
 * ask for nothing new; nor does a literal naming the colour the game already
 * has.  An argument that is not a literal could be anything, so it fires. */
void
ColourDetect::quest5_call (const std::string &call, bool background,
                           const std::string &arg, const std::string &where)
{
    std::string a = trim (arg);
    if (lower (a).find ("game.default") != std::string::npos)
        return;
    if (a.size () >= 2 && a[0] == '"' && a[a.size () - 1] == '"') {
        glui32 c = css_colour (a.substr (1, a.size () - 2));
        if (c == COLOUR_NONE || c == (background ? game_bg : game_fg))
            return;
        if (!background) {
            span (c, call + " (" + a + ") in " + where);
            return;
        }
    }
    fire ("call", call + " (" + a + ") in " + where);
}

void
ColourDetect::span (glui32 fg, const std::string &text)
{
    if (fg == COLOUR_NONE)
        return;
    for (const auto &s : spans)
        if (s.first == fg)
            return;
    std::string sample = text.substr (0, 40);
    for (char &c : sample)
        if (c == '\n' || c == '\t')
            c = ' ';
    spans.push_back ({ fg, sample });
}

bool
ColourDetect::needs_theme () const
{
    const char *env = getenv ("QUESTION_COLOUR_CENSUS");
    return !spans.empty () && (!on () || (env && *env));
}

void
ColourDetect::judge (glui32 theme)
{
    theme_bg = theme;
    for (const auto &s : spans)
        if (colour_legible (s.first, game_bg)
            && !colour_legible (s.first, theme_bg)) {
            char hex[8];
            snprintf (hex, sizeof hex, "%06X", (unsigned) s.first);
            fire ("span", std::string ("#") + hex + " \"" + s.second + "\"");
        }
}

void
ColourDetect::census (const char *game) const
{
    const char *env = getenv ("QUESTION_COLOUR_CENSUS");
    if (!env || !*env)
        return;
    char theme[8] = "-";
    if (theme_bg != COLOUR_NONE)
        snprintf (theme, sizeof theme, "#%06X", (unsigned) theme_bg);
    fprintf (stderr, "colour-census: %s theme=%s game=#%06X spans=%zu %s\n",
             game ? game : "-", theme, (unsigned) game_bg, spans.size (),
             on () ? "ON" : "off");
    for (const Fired &f : fired)
        fprintf (stderr, "colour-census:   %s: %s\n", f.rule.c_str (),
                 f.detail.c_str ());
}

ColourDetect g_colour_detect;

void
colour_detect_settle (winid_t main, const char *game, bool autorestoring)
{
    ColourDetect &d = g_colour_detect;
    if (!autorestoring && d.needs_theme ()) {
        const char *env = getenv ("QUESTION_COLOUR_THEME");
        glui32 forced = env ? css_colour (env) : COLOUR_NONE;
        d.judge (forced != COLOUR_NONE ? forced : measure_background (main));
    }
    d.census (game);
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
draw_status_row (winid_t banner, const std::string &room,
                 const std::string &status, bool utf8)
{
    /* The colours first, so the clear paints the banner in them. */
    strid_t stream = glk_window_get_stream (banner);
    colour_status_begin (stream);
    glk_window_clear (banner);
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

/* The banner, then the main window's colours put back. */
void
draw_status_banner (winid_t banner, const std::string &room,
                    const std::string &status, bool utf8)
{
    if (!banner)
        return;
    draw_status_row (banner, room, status, utf8);
    colour_status_end ();
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
 * the same columns as the shared ones below.
 *
 * The rows are a table laid out with spaces, so they are printed in the
 * fixed-width style; in a proportional font the second column wanders. */
void
print_system_commands (const char *quit_rows, const char *oops_rows,
                       const char *verbs_rows, const char *about_rows)
{
    glk_put_string ((char *)
        "\nThese system commands work in any game, whether Quest itself or"
        " this interpreter handles them:\n"
        "\n");
    glk_set_style (style_Preformatted);
    glk_put_string ((char *)
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
        "  SIDEBAR WIDE      Make the pane beside the text wider.\n"
        "  SIDEBAR NORMAL    Give the pane its usual width back.\n"
        "  COLOURS ON / OFF  Draw the text in the game's own colours, or in\n"
        "                    the interpreter's theme.\n"
        "  HELP              Show the game's own in-game help.\n"
        "  #HELP             Show this list of system commands.\n");
    glk_set_style (style_Normal);
}

/* -------------------------------------------------------------- side pane */

/* The pane's section headings.  Quest 5 games may localize them (the
 * InventoryLabel / PlacesObjectsLabel / CompassLabel templates); these are
 * that engine's fallbacks and the classic runner's only wording. */
const char PANE_INVENTORY[] = "Inventory";
const char PANE_PLACES_OBJECTS[] = "Places and Objects";
const char PANE_COMPASS[] = "Compass";
const char PANE_STATUS[] = "Status";

/* The pane's share of the width, in percent: 20 as a rule, twice that after
 * SIDEBAR WIDE.  The player's choice rather than the game's, so it outlives
 * the pane itself, which closes whenever it has nothing to list. */
static bool side_pane_wide = false;

static glui32
side_pane_percent ()
{
    return side_pane_wide ? 35 : 20;
}

bool
side_pane_is_wide ()
{
    return side_pane_wide;
}

/* Set the pane's width, resizing it on the spot if it is open.  The split
 * that carries the width is the one the pane was opened with; the divider
 * was split off the pane afterwards and so sits one pair window below it. */
void
set_side_pane_wide (bool wide, winid_t pane, winid_t divider)
{
    side_pane_wide = wide;
    if (!pane)
        return;
    winid_t split = glk_window_get_parent (pane);
    if (split && divider)
        split = glk_window_get_parent (split);
    if (split)
        glk_window_set_arrangement (split,
                                    winmethod_Right | winmethod_Proportional,
                                    side_pane_percent (), nullptr);
}

/* SIDEBAR metaverb matching: +1 = make the pane wide, -1 = back to its
 * usual width, kSidebarOn/Off/Auto = show it, hide it, or leave it to the
 * game (Quest 5 only: a Quest 4 game cannot hide its pane), 0 = not a
 * sidebar command (let it reach the game). */
int
match_sidebar_command (const std::string &raw)
{
    std::string c = lower (trim (raw));
    if (!c.empty () && c[0] == '#')
        c.erase (0, 1);
    if (c == "sidebar wide")
        return 1;
    if (c == "sidebar normal" || c == "sidebar narrow")
        return -1;
    if (c == "sidebar" || c == "sidebar on")
        return kSidebarOn;
    if (c == "sidebar off")
        return kSidebarOff;
    if (c == "sidebar auto")
        return kSidebarAuto;
    return 0;
}

/* Act on a SIDEBAR command and say what came of it.  `supported` is whether
 * the host can show a pane at all; one that is merely closed for want of
 * anything to list takes the new width when it next opens. */
void
change_side_pane_width (int which, bool supported, winid_t pane,
                        winid_t divider)
{
    if (!supported) {
        glk_put_string ((char *) "There is no sidebar to resize.\n");
        return;
    }
    bool wide = which > 0;
    if (wide == side_pane_wide) {
        glk_put_string ((char *) (wide ? "The sidebar is already wide.\n"
                                       : "The sidebar already has its usual"
                                         " width.\n"));
        return;
    }
    set_side_pane_wide (wide, pane, divider);
    glk_put_string ((char *) (wide ? "The sidebar is now wide.\n"
                                   : "The sidebar is back to its usual"
                                     " width.\n"));
}

/* Open the right-hand pane: a proportional split of the main text window
 * (see side_pane_percent), with a thin graphics window as its left child,
 * drawn in the text colour as a divider (see fill_side_divider).  No-op when
 * already open; the pane stays null when the host cannot split. */
void
open_side_pane_windows (winid_t mainwin, winid_t *pane, winid_t *divider)
{
    if (*pane)
        return;
    *pane = glk_window_open (mainwin, winmethod_Right | winmethod_Proportional,
                             side_pane_percent (), wintype_TextBuffer, 0);
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
/* A file by path, or `length` bytes of it at `offset` (both 0 for the whole
 * file), as a Glk resource: glkimp's gli_add_resource_from_path gives it a
 * resource number (the same one for the same file every time) and loads it
 * into the backend on first use.  Returns 0 when the file cannot be had.
 * The classic runner's resources are plain files; the Quest 5 frontend
 * resolves .quest package entries itself and hands over what it found.
 *
 * `verify` asks the app whether the resource really decoded, and returns 0
 * if not.  That round-trip also means the app has read the file, so a
 * caller that staged the bytes in a temporary file may remove it. */
int
register_path_resource (const std::string &path, bool sound, glui32 offset,
                        glui32 length, bool verify)
{
    glui32 id = gli_add_resource_from_path (sound ? giblorb_ID_Snd
                                                  : giblorb_ID_Pict,
                                            path.c_str (), offset, length);
    if (id && verify) {
        glui32 w, h;
        if (!(sound ? win_findsound ((int) id) != 0
                    : glk_image_get_info (id, &w, &h) != 0))
            id = 0;
    }
    return (int) id;
}

#else /* !SPATTERLIGHT */

/* No by-name registration without the glkimp backend, so nothing can be
 * loaded: callers see the same 0 a missing file gives them and report the
 * sound or image as unsupported.  Defined rather than #ifdef'd at each call
 * site so the frontends compile against a plain Glk (CheapGlk) too. */
int
register_path_resource (const std::string &, bool, glui32, glui32, bool)
{
    return 0;
}

#endif /* SPATTERLIGHT */

/* --------------------------------------------------------------- autosave */

void
blob_num (std::string &s, long v)
{
    s += std::to_string (v);
    s += '\n';
}

void
blob_str (std::string &s, const std::string &v)
{
    blob_num (s, (long) v.size ());
    s += v;
    s += '\n';
}

long
BlobReader::num ()
{
    if (!ok)
        return 0;
    size_t e = s.find ('\n', pos);
    if (e == std::string::npos) {
        ok = false;
        return 0;
    }
    long v = atol (s.c_str () + pos);
    pos = e + 1;
    return v;
}

std::string
BlobReader::str ()
{
    long n = num ();
    if (!ok || n < 0 || (size_t) n >= s.size () - pos) {
        ok = false;
        return "";
    }
    std::string v = s.substr (pos, (size_t) n);
    pos += (size_t) n + 1;
    return v;
}

/* Stamped on autosave.glksave and bumped whenever the layout below changes:
 * an autosave that will not read is discarded for a fresh start, which is
 * better than one that reads wrongly. */
static const char AUTOSAVE_RECORD_MAGIC[] = "QUESTION-AUTOSAVE 1";

std::string
encode_autosave_record (const AutosaveRecord &rec)
{
    std::string out;
    blob_str (out, AUTOSAVE_RECORD_MAGIC);
    blob_str (out, rec.state);
    blob_str (out, rec.undo);
    blob_num (out, rec.replay ? 1 : 0);
    if (rec.replay) {
        blob_str (out, rec.command);
        blob_num (out, (long) rec.answers.size ());
        for (const std::string &a : rec.answers)
            blob_str (out, a);
    }
    return out;
}

bool
decode_autosave_record (const std::string &data, AutosaveRecord *rec)
{
    BlobReader r{data};
    *rec = AutosaveRecord ();
    if (r.str () != AUTOSAVE_RECORD_MAGIC)
        return false;
    rec->state = r.str ();
    rec->undo = r.str ();
    rec->replay = r.num () != 0;
    if (rec->replay) {
        rec->command = r.str ();
        long n = r.num ();
        for (long i = 0; i < n && r.ok; i++)
            rec->answers.push_back (r.str ());
    }
    /* A record with neither a state nor a replay has nothing to restore. */
    return r.ok && r.at_end () && (rec->replay || !rec->state.empty ());
}

} /* namespace questglk */
