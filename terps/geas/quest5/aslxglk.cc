// aslxglk.cc -- Glk frontend for the native Quest 5 (.aslx / .quest) engine.
//
// This is the "second engine sharing the Glk frontend" of TODO-quest5.md:
// geasglk.cc's glk_main sniffs the story file and hands Quest 5 games to
// aslx_glk_main() below, while .asl/.cas keep going to the classic runner.
//
// The interaction model is the QuestViva pending-callback port (§3): the
// engine never blocks mid-script -- `get input` / `show menu` / `ask` / `wait`
// register callbacks and the prompt loop here asks the engine what it is
// waiting for (pending_menu / pending_question / pending_wait / else a
// command line) and resolves it with the next player input.  The expression
// forms of ShowMenu and Ask are the genuinely synchronous prompts (both AWAIT
// mid-expression); they get a nested input loop via Interp::menu_provider and
// Interp::ask_provider.
//
// Output arrives as Quest's constrained HTML subset (Core's OutputText emits
// <span style=".."> text </span><br/> chunks; game text adds <b>/<i>/<u> and
// {command:}/{object:} links).  render_html() maps it onto Glk styles and
// hyperlinks: <br/> is the newline, bold/italic/underline come from both the
// tags and the span-style CSS, cmdlinks become Glk hyperlinks whose click
// sends their data-command (or fires their ASLEvent) as if typed.  Everything
// else -- fonts, colours, alignment -- is deliberately dropped (§4: don't
// write a browser).  Images and sounds are presentation-milestone work.
//
// Like the test drivers, this unity-includes the loader + runtime: their
// file-local helpers stay shared, and this is the only aslx translation unit
// in the geas binary.

#include "aslx.cc"
#include "aslx-runtime.cc"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

extern "C" {
#include "glk.h"
#ifdef SPATTERLIGHT
/* Full window/stream structs: autosave needs the serialization tags.
 * fileref.h: the per-session temp directory staged sounds must live in so
 * the app can replay them after an autorestore. */
#include "glkimp.h"
#include "fileref.h"
#endif
}

#ifdef SPATTERLIGHT
/* Spatterlight autosave/autorestore (geasglk-autosave.mm). */
#include "../geasglk-autosave.h"
#endif

/* Presentation helpers shared with the classic Quest 1-4 frontend
 * (geasglk.cc): the status banner, side pane + divider, transcript metaverb,
 * save-file prompts, string/UTF-8 utilities and resource registration. */
#include "../questglk-common.inc"

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <libgen.h>
#endif
#include <unistd.h>

/* Spatterlight's "slow draw / real-time delays" preference (glkimp).  When it
 * is off -- as in the deterministic import tests -- the wall-clock timer
 * heartbeat is suppressed, exactly like the classic geas loop. */
extern "C" int gli_sa_delays;

namespace {

using namespace aslx;

/* The Glk timer interval currently requested (ms, 0 = off) and the ms
 * accumulated toward the next whole engine second while the interval runs
 * faster than 1s (the map marker glide -- update_timer_request,
 * timer_event_second).  File-scope globals so the blocking do_pause hook,
 * which overrides the timer with its own one-shot interval, can reset them
 * and let the next update_timer_request re-arm cleanly. */
glui32 g_timer_ms = 0;
int g_timer_frac_ms = 0;

/* Shared frontend helpers (questglk-common.inc).  Using-declarations rather
 * than a using-directive: aslx.cc has a file-local trim of its own, and a
 * name declared here shadows it instead of colliding with it. */
using questglk::close_side_pane_windows;
using questglk::draw_status_banner;
using questglk::echo_input_line;
using questglk::fill_side_divider;
using questglk::lower;
using questglk::match_status_command;
using questglk::match_transcript_command;
using questglk::open_side_pane_windows;
using questglk::play_single_sound;
using questglk::post_game_menu_match;
using questglk::post_game_menu_print;
using questglk::post_game_menu_reprompt;
using questglk::POSTGAME_QUIT;
using questglk::POSTGAME_RESTART;
using questglk::POSTGAME_RESTORE;
using questglk::POSTGAME_UNDO;
using questglk::print_status_report;
using questglk::prompt_read_save;

/* Core's CapFirst is .NET ToUpper on the first CHARACTER, not the first
 * ASCII byte -- Greek pane labels ("τόξο") and room names must capitalize
 * ("Τόξο") exactly as under QuestViva.  questglk::cap_first stays ASCII-only
 * for the Quest 4 frontend (geas strings are Latin-1-ish, passthrough there);
 * here the engine's .NET-invariant simple case map is in this TU already. */
static std::string cap_first(const std::string &s)
{
    return aslx::utf8_case(s, true, true);
}
using questglk::prompt_write_save;
using questglk::put_pane_header;
using questglk::put_pane_link;
using questglk::put_stream_utf8;
using questglk::stop_single_sound;
using questglk::toggle_transcript;
using questglk::trim;
using questglk::utf8_cp_len;
using questglk::utf8_next_cp;

winid_t gwin = nullptr;      /* main text buffer */
winid_t gbanner = nullptr;   /* one-line status grid: room name */
strid_t gtranscript = nullptr;
bool g_manual_echo = false;  /* the HOST echoes accepted input lines */
bool g_echo_control = false; /* library supports glk_set_echo_line_event */
bool g_hyperlinks = false;
std::string g_room_name;

/* Prompt-first presentation.  The reference player has a separate input box
 * ("Type here..."), so no prompt is ever printed and the GAME side echoes
 * each accepted command into the transcript itself ("> cmd" -- Core's
 * game.echocommand, or the engine for pre-v520).  In a Glk buffer we want a
 * standard "> " prompt at the input point instead, so when the library lets
 * us control line-input echo we print the prompt up front, echo the accepted
 * line ourselves, and swallow the game-side echo (g_swallow) so the command
 * does not appear twice.  Echo-incapable libraries (CheapGlk: the smoke
 * harness) keep the reference presentation, byte-stable for the harness. */
bool g_prompt_first = false;
/* Is the reference player's command box showing?  Core hides it when
 * game.showcommandbar is off, and GamebookCore hides it for good: a gamebook
 * has no parser at all, it is played by clicking the links on each page.
 * While it is hidden we print no prompt and request no line input, so the
 * page reads as a page -- exactly what the reference player shows.  Only a
 * `get input` re-shows it (GamebookCore's GetInput does so explicitly). */
bool g_command_bar = true;
/* Set by JS.HookClicks: the game has bound a handler to the whole output area
 * that turns ANY click there into a command (spondre's glue.js sends
 * "(Unknown)", a token no verb matches, so the game's catch-all response
 * runs).  A Glk text buffer delivers no such click -- only hyperlinks are
 * clickable -- so the visible "click to continue" link stands in for it: with
 * the hook installed and no wait to release, clicking runs a turn.  Without
 * that, spondre's title has no way forward but typing a command at a screen
 * that offers nothing to type at. */
bool g_click_hook = false;
/* The command a hook-driven click sends.  The text is not inspected by the
 * games using this idiom -- verified on spondre, where "(Unknown)",
 * "xyzzyplugh" and "look" all produce byte-identical output at the title, so
 * what matters is only that a turn runs. */
const char *const kClickHookCommand = "(Unknown)";
int g_swallow = 0;              /* 2 = leading blank + "> cmd" outstanding,
                                   1 = blank swallowed, "> cmd" outstanding */
std::string g_swallow_cmd;      /* the trimmed command the echo must match */

/* Click actions, one per Glk hyperlink value (index + 1).  A cmdlink carries
 * either a literal command (data-command / data-elementid) or an
 * onclick="ASLEvent('Func','param')" pair (Core's ShowMenu options). */
struct LinkAction {
    std::string command;        /* send_command() this */
    std::string event_func;     /* or call_function(event_func, {event_param}) */
    std::string event_param;
    std::string toggle_key;     /* pane object: expand/collapse its verb list */
    bool end_wait = false;      /* onclick="endWait()": release a pending wait */
    bool inner_text = false;    /* command is the element's own text (filled in
                                 * by the renderer, which has the content) */

    bool live() const {
        return !command.empty() || !event_func.empty() || end_wait;
    }
};
std::vector<LinkAction> g_links;
/* Link values at or below this are spent: JS.disableAllCommandLinks (Gamebook
 * Core, when a page choice has been taken) retires every cmdlink printed so
 * far, so an earlier page's options cannot be clicked again.  A high-water
 * mark rather than a clear, because the values are indices into g_links and
 * the already-printed links keep theirs -- clearing would hand an old link on
 * screen the action of whichever new one reused its number. */
size_t g_links_spent = 0;

/* Right-hand pane: Quest's Inventory / "Places and Objects" / compass lists
 * (WorldModel.UpdateListsAsync -> the update_list host hook), shown the way
 * the classic runner shows its objwin -- opened only when there is something
 * to list, with a thin graphics divider, closed otherwise. */
winid_t gobjwin = nullptr;
winid_t gdivider = nullptr;
bool g_use_objpane = false;          /* host can split; probed at startup */
std::vector<ListData> g_pane_inv, g_pane_places, g_pane_exits;
bool g_pane_dirty = false;           /* lists changed since the last redraw */
std::vector<LinkAction> g_pane_links;   /* rebuilt on every pane redraw */
/* Element name of the pane object whose verb list is currently unfolded, or
 * empty.  QuestViva pops a verb menu when an object name is clicked; a Glk
 * pane has nowhere to pop one, so the verbs appear as an indented list under
 * the name, and clicking the name again folds it away.  One at a time, like
 * the reference menu.  Survives redraws, so the list stays open across the
 * turns its own verbs run. */
std::string g_pane_expanded;
/* Pane hyperlink values live above the main-window link ids, which only ever
 * grow one per rendered cmdlink and never get near this. */
const glui32 kPaneLinkBase = 0x40000000;
/* JS.updateStatus payload flattened for the banner ("Score: 3 | Health: 90%"),
 * shown right-aligned next to the room name like the classic runner. */
std::string g_status_line;
/* JS.updateLocation payload -- Core's own "where am I" string, preferred over
 * resolving game.pov.parent by hand (empty for old pre-JS games). */
std::string g_location_line;

/* Inline image drawing (defined in the resources section below): draw the
 * named game resource in the main window, original size but never wider
 * than the window. False when unresolvable or images are unsupported. */
bool draw_image_inline(const std::string &name);
/* Inline <audio> playback (defined alongside it): start the first of `srcs`
 * that resolves, on the same single channel as `play sound`. */
bool play_audio_inline(const std::vector<std::string> &srcs, bool loop);
void panel_arrange();

/* Pane redraw (defined after the HTML renderer): repaints the side pane from
 * the stored lists when they changed; safe to call anywhere no line input is
 * pending. */
void redraw_side_pane(Interp &in);
void fill_pane_divider();

/* The grid map (game.gridmap): a graphics band across the top of the screen,
 * drawn from CoreGrid.aslx's paint stream via the grid_draw host hook. */
#include "aslxglk-map.inc"

#ifdef SPATTERLIGHT
/* Spatterlight autosave.  aslx_do_autosave (defined with the rest of the
 * autosave glue before run_session) runs at the parser prompt whenever
 * g_autosave_interp is set -- read_line calls it after printing the prompt
 * and again after state-changing timer ticks, so the snapshot always ends
 * at "waiting for a command". */
bool g_autorestore_pending = false;   /* an autosave exists; restore at boot */
bool g_autorestore_reentry = false;   /* first prompt after it: no reprint */
std::string g_autorestore_panel;      /* frame picture to re-establish */
/* RNG streams captured at autosave time, applied just before play resumes
 * (AFTER InitInterface's own draws) so the restored prompt's randomness is
 * exactly the saved one. */
std::vector<std::pair<std::string, std::array<uint32_t, 4>>> g_autorestore_rngs;
Interp *g_autosave_interp = nullptr;
void aslx_do_autosave(Interp &in);
#endif

void update_timer_request(Interp &in);

/* One evtype_Timer arrived.  While the map marker glides, the shared timer
 * runs at GM_ANIM_TICK_MS: advance the glide (graphics-window output only,
 * so it is legal even with a line request pending) and report whether a
 * whole engine second has also elapsed; on the plain 1-second heartbeat
 * every event is an engine second. */
bool timer_event_second(Interp &in)
{
    if (g_timer_ms == 0 || g_timer_ms >= 1000)
        return true;
    grid_map_anim_tick();
    redraw_grid_map();
    g_timer_frac_ms += (int) g_timer_ms;
    bool due = g_timer_frac_ms >= 1000;
    if (due)
        g_timer_frac_ms -= 1000;
    update_timer_request(in);  /* glide over -> heartbeat cadence, or off */
    return due;
}

/* ---------------------------------------------------------------- output -- */

/* While an output section is being recorded this captures every character
 * render_html writes into the main window, so HideOutputSection can hand the
 * exact text back to garglk_unput_string_count_uni (a case-insensitive TAIL
 * match -- see g_out_log). */
std::u32string *g_tee = nullptr;

/* Bumped by draw_image_inline; render_html compares it across a chunk to see
 * whether that chunk put an unmatchable attachment into the window. */
size_t g_images_drawn = 0;

void put_uni_char(glui32 c)
{
    glk_put_char_uni(c);
    if (g_tee)
        g_tee->push_back((char32_t) c);
}

void put_uni_string(const std::string &utf8)
{
    for (size_t i = 0; i < utf8.size();)
        put_uni_char(utf8_next_cp(utf8, i));
}

/* Current inline style, tracked as nesting counters so tags may overlap.
 * Mapped onto the closest Glk style, like the classic runner's set_style. */
int g_bold = 0, g_italic = 0, g_under = 0;

void apply_style()
{
    glui32 st;
    if (g_bold && g_italic) st = style_Alert;
    else if (g_italic)      st = style_Emphasized;
    else if (g_bold)        st = style_Subheader;
    else if (g_under)       st = style_User2;
    else                    st = style_Normal;
    glk_set_style(st);
}

/* What one <span>/<a> contributed to the counters, so its close undoes it. */
struct StylePush { int b = 0, i = 0, u = 0; };

/* Value of attr="..." (or attr='...') inside a raw tag body, or "". */
std::string tag_attr(const std::string &tag, const std::string &attr)
{
    std::string t = lower(tag);
    size_t pos = 0;
    while ((pos = t.find(attr, pos)) != std::string::npos) {
        /* Whole-attribute match only: `src` must not hit inside `data-src`.
         * The char before the name must not be part of an attribute name. */
        if (pos > 0 && (std::isalnum((unsigned char)t[pos - 1]) ||
                        t[pos - 1] == '-' || t[pos - 1] == '_')) {
            pos += attr.size();
            continue;
        }
        size_t eq = pos + attr.size();
        while (eq < t.size() && (t[eq] == ' ' || t[eq] == '\t')) eq++;
        if (eq >= t.size() || t[eq] != '=') { pos = eq; continue; }
        eq++;
        while (eq < t.size() && (t[eq] == ' ' || t[eq] == '\t')) eq++;
        if (eq >= t.size() || (t[eq] != '"' && t[eq] != '\'')) { pos = eq; continue; }
        char q = t[eq];
        size_t end = tag.find(q, eq + 1);
        if (end == std::string::npos) return "";
        return tag.substr(eq + 1, end - eq - 1);   /* original case */
    }
    return "";
}

/* True if a boolean attribute (`autoplay`, `loop`) is present on a raw tag.
 * tag_attr only sees the attr="value" form, and HTML boolean attributes are
 * normally bare.  This walks the attribute list properly, skipping quoted
 * values, so L Too's <audio autoplay onplay="this.autoplay=''"> is not read
 * as carrying a second autoplay from inside the onplay script. */
bool tag_flag(const std::string &tag, const std::string &attr)
{
    std::string t = lower(tag);
    size_t i = 0;
    while (i < t.size() && !isspace((unsigned char) t[i]))
        i++;                                    /* skip the tag name */
    while (i < t.size()) {
        while (i < t.size() &&
               (isspace((unsigned char) t[i]) || t[i] == '/'))
            i++;
        size_t start = i;
        while (i < t.size() && !isspace((unsigned char) t[i]) &&
               t[i] != '=' && t[i] != '/')
            i++;
        std::string name = t.substr(start, i - start);
        while (i < t.size() && isspace((unsigned char) t[i]))
            i++;
        if (i < t.size() && t[i] == '=') {       /* skip the value */
            i++;
            while (i < t.size() && isspace((unsigned char) t[i]))
                i++;
            if (i < t.size() && (t[i] == '"' || t[i] == '\'')) {
                char q = t[i++];
                while (i < t.size() && t[i] != q)
                    i++;
                if (i < t.size())
                    i++;
            } else {
                while (i < t.size() && !isspace((unsigned char) t[i]))
                    i++;
            }
        }
        if (name == attr)
            return true;
    }
    return false;
}

/* Style contribution of a span-style CSS string ("font-weight:bold;..."). */
StylePush css_style(const std::string &css)
{
    StylePush p;
    std::string c = lower(css);
    if (c.find("font-weight:bold") != std::string::npos ||
        c.find("font-weight: bold") != std::string::npos)
        p.b = 1;
    if (c.find("font-style:italic") != std::string::npos ||
        c.find("font-style: italic") != std::string::npos)
        p.i = 1;
    if (c.find("text-decoration:underline") != std::string::npos ||
        c.find("text-decoration: underline") != std::string::npos)
        p.u = 1;
    return p;
}

void push_style(const StylePush &p)
{
    g_bold += p.b; g_italic += p.i; g_under += p.u;
    apply_style();
}

void pop_style(const StylePush &p)
{
    g_bold -= p.b; g_italic -= p.i; g_under -= p.u;
    if (g_bold < 0) g_bold = 0;
    if (g_italic < 0) g_italic = 0;
    if (g_under < 0) g_under = 0;
    apply_style();
}

std::string decode_entities(const std::string &s);

/* Extract the click action of an <a> or <span> tag, if we understand it. */
LinkAction link_action(const std::string &tag)
{
    LinkAction a;
    a.command = tag_attr(tag, "data-command");
    if (a.command.empty()) {
        std::string oc = tag_attr(tag, "onclick");
        /* Core's ShowMenu options: onclick="ASLEvent('Func','param')". */
        size_t at = oc.find("ASLEvent(");
        if (at != std::string::npos) {
            size_t q1 = oc.find('\'', at);
            size_t q2 = q1 == std::string::npos ? q1 : oc.find('\'', q1 + 1);
            size_t q3 = q2 == std::string::npos ? q2 : oc.find('\'', q2 + 1);
            size_t q4 = q3 == std::string::npos ? q3 : oc.find('\'', q3 + 1);
            if (q4 != std::string::npos) {
                a.event_func = oc.substr(q1 + 1, q2 - q1 - 1);
                a.event_param = oc.substr(q3 + 1, q4 - q3 - 1);
            }
        }
        /* The web player's own command bridge, which games call directly from
         * hand-written markup (spondre's keyword spans, and 8 other corpus
         * games): sendCommand("literal") runs that command, while
         * sendCommand(this.innerText || this.textContent) runs whatever the
         * element says -- the renderer fills that in, having the content. */
        size_t sc = oc.find("sendCommand(");
        if (a.event_func.empty() && sc != std::string::npos) {
            size_t p = sc + strlen("sendCommand(");
            while (p < oc.size() && isspace((unsigned char) oc[p])) p++;
            if (p < oc.size() && (oc[p] == '"' || oc[p] == '\'')) {
                size_t end = oc.find(oc[p], p + 1);
                if (end != std::string::npos)
                    a.command = decode_entities(oc.substr(p + 1, end - p - 1));
            } else if (oc.compare(p, 5, "this.") == 0) {
                a.inner_text = true;
            }
        }
        /* onclick="endWait()": the page's "click to continue". */
        if (oc.find("endWait(") != std::string::npos)
            a.end_wait = true;
    }
    if (a.command.empty() && a.event_func.empty()) {
        /* An object link (class="cmdlink elementmenu") pops a verb menu in the
         * JS UI; the closest single-command equivalent is examining it. */
        std::string cls = tag_attr(tag, "class");
        std::string elem = tag_attr(tag, "data-elementid");
        if (cls.find("elementmenu") != std::string::npos && !elem.empty())
            a.command = std::string("look at ") + elem;
    }
    return a;
}

void utf8_append(std::string &out, glui32 cp)
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

/* Decode one HTML entity starting at s[i] ('&').  Returns the codepoint and
 * advances i past it, or returns '&' advancing 1 if not an entity. */
glui32 decode_entity(const std::string &s, size_t &i)
{
    size_t semi = s.find(';', i);
    if (semi == std::string::npos || semi - i > 10) { i++; return '&'; }
    std::string e = s.substr(i + 1, semi - i - 1);
    glui32 cp = 0;
    if (!e.empty() && e[0] == '#') {
        if (e.size() > 1 && (e[1] == 'x' || e[1] == 'X'))
            cp = (glui32) strtoul(e.c_str() + 2, nullptr, 16);
        else
            cp = (glui32) strtoul(e.c_str() + 1, nullptr, 10);
        if (!cp) { i++; return '&'; }
        i = semi + 1;
        return cp;
    }
    /* Games hand-write HTML, so the typographic entities an author pastes in
     * belong here alongside the markup ones -- an unknown name renders as a
     * literal "&bull;" mid-sentence. */
    struct { const char *name; glui32 cp; } ents[] = {
        {"amp", '&'}, {"lt", '<'}, {"gt", '>'}, {"quot", '"'},
        {"apos", '\''}, {"nbsp", ' '}, {"mdash", 0x2014}, {"ndash", 0x2013},
        {"hellip", 0x2026}, {"copy", 0xa9}, {"eacute", 0xe9},
        {"bull", 0x2022}, {"middot", 0xb7}, {"deg", 0xb0},
        {"lsquo", 0x2018}, {"rsquo", 0x2019},
        {"ldquo", 0x201c}, {"rdquo", 0x201d},
        {"laquo", 0xab}, {"raquo", 0xbb},
        {"reg", 0xae}, {"trade", 0x2122},
    };
    for (auto &en : ents)
        if (e == en.name) { i = semi + 1; return en.cp; }
    i++;
    return '&';
}

/* Entity-decode a whole attribute value (an <img src> may carry &amp;). */
std::string decode_entities(const std::string &s)
{
    std::string out;
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '&')
            utf8_append(out, decode_entity(s, i));
        else
            out += s[i++];
    }
    return out;
}

std::string plain_text(const std::string &html, const char *brsep);

/* Case-insensitive find, for tag names in raw markup. */
size_t find_ci(const std::string &hay, const std::string &needle, size_t from)
{
    if (needle.size() > hay.size()) return std::string::npos;
    for (size_t i = from; i + needle.size() <= hay.size(); i++) {
        size_t j = 0;
        while (j < needle.size() &&
               tolower((unsigned char) hay[i + j]) ==
               tolower((unsigned char) needle[j]))
            j++;
        if (j == needle.size()) return i;
    }
    return std::string::npos;
}

/* The text content of the element whose opening tag ends at `pos`, with
 * nested same-name tags respected -- what a browser's this.innerText would
 * give an onclick="sendCommand(this.innerText)" handler. */
std::string element_inner_text(const std::string &html, size_t pos,
                               const std::string &name)
{
    std::string inner;
    int depth = 1;
    for (size_t i = pos; i < html.size();) {
        if (html[i] != '<') { inner += html[i++]; continue; }
        size_t close = html.find('>', i);
        if (close == std::string::npos) break;
        std::string tag = html.substr(i + 1, close - i - 1);
        bool closing = !tag.empty() && tag[0] == '/';
        std::string tn = lower(tag.substr(closing ? 1 : 0));
        size_t sp = tn.find_first_of(" \t\r\n/");
        if (sp != std::string::npos) tn.erase(sp);
        if (tn == name) {
            if (closing && --depth == 0) break;
            if (!closing && tag[tag.size() - 1] != '/') depth++;
        }
        inner += html.substr(i, close - i + 1);
        i = close + 1;
    }
    return trim(plain_text(inner, " "));
}

/* The candidate files of the <audio> element whose opening tag `open_tag`
 * ends at `pos`, in document order: the element's own src, then every nested
 * <source src>.  *end is left just past the matching </audio> -- or at the end
 * of the chunk when the game never closed it -- so the caller can skip the
 * whole element, fallback text included. */
std::vector<std::string> audio_sources(const std::string &html, size_t pos,
                                       const std::string &open_tag, size_t *end)
{
    std::vector<std::string> srcs;
    std::string own = decode_entities(tag_attr(open_tag, "src"));
    if (!own.empty())
        srcs.push_back(own);
    int depth = 1;
    size_t i = pos;
    while (i < html.size()) {
        if (html[i] != '<') { i++; continue; }
        size_t close = html.find('>', i);
        if (close == std::string::npos) { i = html.size(); break; }
        std::string tag = html.substr(i + 1, close - i - 1);
        bool closing = !tag.empty() && tag[0] == '/';
        std::string tn = lower(tag.substr(closing ? 1 : 0));
        size_t sp = tn.find_first_of(" \t\r\n/");
        if (sp != std::string::npos) tn.erase(sp);
        if (tn == "audio") {
            if (closing && --depth == 0) { i = close + 1; break; }
            if (!closing && tag[tag.size() - 1] != '/') depth++;
        } else if (tn == "source" && !closing && depth == 1) {
            std::string s = decode_entities(tag_attr(tag, "src"));
            if (!s.empty())
                srcs.push_back(s);
        }
        i = close + 1;
    }
    *end = i;
    return srcs;
}

/* While g_swallow is armed (a send_command whose prompt and echo the host
 * already printed), watch for the game side's own echo of the command --
 * msg("")'s blank line then "&gt; cmd" (Core's echocommand), or the bare
 * "> cmd" print of pre-540 engines -- and drop it.  Any other chunk disarms
 * and prints normally (a game with a custom HandleCommand), giving back the
 * provisionally swallowed blank.  True = the chunk was the echo. */
bool swallow_echo_chunk(const std::string &html)
{
    std::string t = trim(plain_text(html, " "));
    if (t == "> " + g_swallow_cmd) {
        g_swallow = 0;
        return true;
    }
    if (g_swallow == 2 && t.empty()) {
        g_swallow = 1;
        return true;
    }
    if (g_swallow == 1)
        put_uni_char('\n');     /* not the echo after all: restore the blank */
    g_swallow = 0;
    return false;
}

/* Remove <style>/<script> elements, and the whitespace run on either side of
 * them, from a chunk.  These are not text: a browser consumes them silently,
 * while games embed whole stylesheets inline (spondre opens with ~30 lines of
 * CSS).  Taking the surrounding whitespace too keeps the blank gap the markup
 * indentation would otherwise leave behind. */
std::string strip_hidden_elements(const std::string &html)
{
    static const char *kHidden[] = { "style", "script" };
    std::string out = html;
    for (const char *name : kHidden) {
        std::string open = std::string("<") + name;
        for (size_t at = find_ci(out, open, 0); at != std::string::npos;
             at = find_ci(out, open, at)) {
            /* "<style" must be the whole tag name, not a prefix. */
            size_t after = at + open.size();
            if (after < out.size() && !isspace((unsigned char) out[after]) &&
                out[after] != '>' && out[after] != '/') {
                at = after;
                continue;
            }
            size_t end = find_ci(out, std::string("</") + name, at);
            if (end != std::string::npos) {
                size_t gt = out.find('>', end);
                end = gt == std::string::npos ? out.size() : gt + 1;
            } else {
                end = out.size();   /* unterminated: drop the rest */
            }
            while (at > 0 && isspace((unsigned char) out[at - 1])) at--;
            while (end < out.size() && isspace((unsigned char) out[end])) end++;
            out.erase(at, end - at);
        }
    }
    return out;
}

/* ------------------------------------------------------------- retract -- */

std::u32string u32_from_utf8(const std::string &s)
{
    std::u32string out;
    for (size_t i = 0; i < s.size();)
        out.push_back((char32_t) utf8_next_cp(s, i));
    return out;
}

/* Take `s` back off the end of the transcript window, but only if it is
 * still exactly what is there (questglk::unput_window_tail, the tail-match
 * retract shared with the classic frontend).  Returns the count removed, 0
 * if the window has moved on -- so every caller degrades to "leave the text
 * alone". */
size_t unput_exact(const std::u32string &s)
{
#ifdef SPATTERLIGHT
    glui32 got = questglk::unput_window_tail(gwin, s);
    return got == s.size() ? s.size() : 0;
#else
    (void) s;                   /* CheapGlk has no unput */
    return 0;
#endif
}

/* ------------------------------------------------------ output sections -- */

/* The reference player wraps JS.StartOutputSection..EndOutputSection output in
 * a <div> it can later hide.  A Glk buffer cannot hide anything, but
 * garglk_unput_string_count_uni retracts text that still matches the END of
 * the window (GlkTextBufferWindow+Output.m:221, a case-insensitive tail
 * compare returning the count removed).  A section is rarely the tail when it
 * is hidden -- GamebookCore prints the chosen option's label BEFORE DoPage
 * hides the options block above it -- so hiding means: retract everything from
 * the section's first chunk to the end of the window, then replay the chunks
 * that came after the section closed.
 *
 * That needs a record of what each chunk actually wrote, which is what g_tee
 * collects.  Anything that fails the tail match (a prompt or host echo landed
 * in between, an image attachment sits inside the range) leaves the transcript
 * exactly as it is today: the retract is best-effort, never destructive. */

struct OutChunk {
    std::string html;       /* raw payload, for replay through render_html */
    std::u32string text;    /* exactly what it wrote into the window */
    bool image = false;     /* drew an image: an attachment char we cannot match */
};

std::vector<OutChunk> g_out_log;

struct OutSection {
    std::string name;
    size_t start = 0;                       /* first chunk index */
    size_t end = (size_t) -1;               /* one past the last, -1 while open */
};

std::vector<OutSection> g_sections;

/* Sections are only ever hidden a page or two after they close, but nothing
 * says so -- bound the log rather than grow it for the length of a game. */
const size_t kMaxSections = 8;
/* win_unprint copies into the 64 KB message buffer as uint16_t
 * (glkimp/connect.c:220), so a retract longer than that would overrun it. */
const size_t kMaxUnput = 16384;

void render_html(const std::string &raw);

/* A hide can only ever retract kMaxUnput chars, so chunks past this many are
 * dead weight -- without the cap a game sitting inside <= kMaxSections (or one
 * never-closed section) records every print for the whole session and
 * re-serializes the lot into every autosave. */
const size_t kMaxLogChunks = 4096;

/* Drop the sections (and the chunks they cover) that can no longer be hidden. */
void trim_out_log()
{
    if (g_sections.size() > kMaxSections) {
        size_t drop = g_sections.size() - kMaxSections;
        size_t cut = g_sections[drop].start;
        g_sections.erase(g_sections.begin(), g_sections.begin() + drop);
        if (cut > 0) {
            g_out_log.erase(g_out_log.begin(), g_out_log.begin() + cut);
            for (OutSection &s : g_sections) {
                s.start -= cut;
                if (s.end != (size_t) -1)
                    s.end -= cut;
            }
        }
    }
    /* Bound the chunk log itself too.  A section clamped here retracts only
     * what remains -- the same best-effort bound kMaxUnput already imposes. */
    if (g_out_log.size() > kMaxLogChunks) {
        size_t cut = g_out_log.size() - kMaxLogChunks;
        g_out_log.erase(g_out_log.begin(), g_out_log.begin() + cut);
        while (!g_sections.empty() && g_sections.front().end != (size_t) -1 &&
               g_sections.front().end <= cut)
            g_sections.erase(g_sections.begin());
        for (OutSection &s : g_sections) {
            s.start = s.start > cut ? s.start - cut : 0;
            if (s.end != (size_t) -1)
                s.end = s.end > cut ? s.end - cut : 0;
        }
    }
}

void start_output_section(const std::string &name)
{
    OutSection s;
    s.name = name;
    s.start = g_out_log.size();
    g_sections.push_back(s);
    trim_out_log();
}

void end_output_section(const std::string &name)
{
    for (size_t i = g_sections.size(); i-- > 0;)
        if (g_sections[i].name == name && g_sections[i].end == (size_t) -1) {
            g_sections[i].end = g_out_log.size();
            return;
        }
}

void hide_output_section(const std::string &name)
{
#ifdef SPATTERLIGHT
    size_t at = (size_t) -1;
    for (size_t i = g_sections.size(); i-- > 0;)
        if (g_sections[i].name == name) { at = i; break; }
    if (at == (size_t) -1)
        return;
    const size_t start = g_sections[at].start;
    size_t end = g_sections[at].end;
    if (end == (size_t) -1 || end > g_out_log.size())
        end = g_out_log.size();
    if (start >= g_out_log.size())
        return;

    /* Everything from the section's first chunk to the end of the window has
     * to come off, since only a suffix can be retracted. */
    std::u32string tail;
    for (size_t i = start; i < g_out_log.size(); i++) {
        if (g_out_log[i].image)
            return;             /* an attachment char will not tail-match */
        tail += g_out_log[i].text;
        if (tail.size() > kMaxUnput)
            return;
    }
    if (tail.empty())
        return;

    /* The buffer window holds a trailing newline back rather than show a blank
     * line at the prompt (GlkTextBufferWindow+Output.m:187, storedNewline), so
     * the text storage lags this log by up to one newline.  Try the exact tail
     * first, then without that newline. */
    size_t took = unput_exact(tail);
    if (!took && !tail.empty() && tail.back() == U'\n')
        took = unput_exact(tail.substr(0, tail.size() - 1));
    if (!took)
        return;                 /* tail moved on: leave the transcript alone */

    /* Put back what followed the section.  Replaying through render_html
     * re-logs the chunks at their new positions and rebuilds their styles and
     * hyperlinks, so the g_links indices the replayed anchors get are fresh. */
    std::vector<OutChunk> replay(g_out_log.begin() + end, g_out_log.end());
    g_out_log.erase(g_out_log.begin() + start, g_out_log.end());
    for (size_t i = g_sections.size(); i-- > 0;)
        if (g_sections[i].start >= start)
            g_sections.erase(g_sections.begin() + i);
    for (const OutChunk &c : replay)
        render_html(c.html);
#else
    (void) name;                /* CheapGlk has no unput: nothing to hide */
#endif
}

/* Render one HTML chunk (a JS.addText payload) into the main window. */
void render_html_inner(const std::string &raw)
{
    const std::string html =
        find_ci(raw, "<style", 0) != std::string::npos ||
        find_ci(raw, "<script", 0) != std::string::npos
            ? strip_hidden_elements(raw) : raw;
    std::vector<StylePush> spans;   /* open <span>s */
    std::vector<StylePush> anchors; /* open <a>s */
    std::vector<char> span_link;    /* did that <span> open a hyperlink? */
    for (size_t i = 0; i < html.size();) {
        char ch = html[i];
        if (ch == '<') {
            size_t close = html.find('>', i);
            if (close == std::string::npos) break;
            std::string tag = html.substr(i + 1, close - i - 1);
            i = close + 1;
            /* tag name, lowercased, without leading '/' or trailing '/'. */
            bool closing = !tag.empty() && tag[0] == '/';
            std::string name = lower(tag.substr(closing ? 1 : 0));
            size_t sp = name.find_first_of(" \t\r\n/");
            if (sp != std::string::npos) name.erase(sp);

            if (name == "br") {
                put_uni_char('\n');     /* teed: see g_tee */
            } else if (name == "b" || name == "strong") {
                StylePush p; p.b = 1;
                closing ? pop_style(p) : push_style(p);
            } else if (name == "i" || name == "em") {
                StylePush p; p.i = 1;
                closing ? pop_style(p) : push_style(p);
            } else if (name == "u") {
                StylePush p; p.u = 1;
                closing ? pop_style(p) : push_style(p);
            } else if (name == "span") {
                if (!closing) {
                    StylePush p = css_style(tag_attr(tag, "style"));
                    /* Games hang clicks off bare spans too (spondre's
                     * keywords), so a span carries a link action like an
                     * anchor does. */
                    LinkAction act = link_action(tag);
                    if (act.inner_text)
                        act.command = element_inner_text(html, i, "span");
                    bool opened = false;
                    if (act.live() && g_hyperlinks) {
                        g_links.push_back(act);
                        glk_set_hyperlink((glui32) g_links.size());
                        opened = true;
                    } else if (act.live()) {
                        p.u = 1;   /* no hyperlink support: underline only */
                    }
                    spans.push_back(p);
                    span_link.push_back(opened ? 1 : 0);
                    push_style(p);
                } else if (!spans.empty()) {
                    if (span_link.back() && g_hyperlinks)
                        glk_set_hyperlink(0);
                    span_link.pop_back();
                    pop_style(spans.back());
                    spans.pop_back();
                }
            } else if (name == "a") {
                if (!closing) {
                    LinkAction act = link_action(tag);
                    if (act.inner_text)
                        act.command = element_inner_text(html, i, "a");
                    StylePush p;
                    bool live = act.live();
                    if (live && g_hyperlinks) {
                        g_links.push_back(act);
                        glk_set_hyperlink((glui32) g_links.size());
                    } else if (live) {
                        p.u = 1;   /* no hyperlink support: underline only */
                    }
                    anchors.push_back(p);
                    push_style(p);
                } else if (!anchors.empty()) {
                    if (g_hyperlinks)
                        glk_set_hyperlink(0);
                    pop_style(anchors.back());
                    anchors.pop_back();
                }
            } else if (name == "img") {
                /* Core's `picture` and the {img:} text-processor command
                 * both emit <img src="filename"/> (GetFileURL returns the
                 * filename as-is in this engine); games also hand-write the
                 * tag around GetFileUrl(). Unresolvable images are dropped
                 * like any other unknown tag. */
                if (!closing) {
                    std::string src = decode_entities(tag_attr(tag, "src"));
                    if (!src.empty())
                        draw_image_inline(src);
                }
            } else if (name == "audio") {
                /* Games hand-write their own sound effects rather than call
                 * Core's `play sound`: L Too's play_sound and Night House both
                 * emit <audio autoplay><source src="x.ogg"/><source
                 * src="x.mp3"/>Your browser does not support the audio
                 * tag.</audio>.  Play the first source that resolves, the same
                 * choice a browser makes over the sources it can decode.
                 *
                 * The element is skipped whole either way.  Its text is
                 * FALLBACK content -- shown only by a browser too old to know
                 * the tag, never alongside playback -- so printing it (which is
                 * what dropping the tag but keeping its children used to do)
                 * was always wrong here, whether or not the sound starts. */
                if (!closing) {
                    size_t end = i;
                    std::vector<std::string> srcs =
                        audio_sources(html, i, tag, &end);
                    if (tag_flag(tag, "autoplay"))
                        play_audio_inline(srcs, tag_flag(tag, "loop"));
                    i = end;
                }
            }
            /* every other tag (font, div, table...) is dropped: §4 says
             * best-effort subset, not a browser. */
        } else if (ch == '&') {
            put_uni_char(decode_entity(html, i));
        } else if (ch == '\r') {
            i++;
        } else {
            /* UTF-8 sequence or plain char (raw newlines kept, as in the
             * oracle transcripts). */
            put_uni_char(utf8_next_cp(html, i));
        }
    }
    /* Unbalanced opens (rare, malformed game HTML): restore the base style. */
    if (!spans.empty() || !anchors.empty()) {
        g_bold = g_italic = g_under = 0;
        if (g_hyperlinks && !anchors.empty())
            glk_set_hyperlink(0);
        apply_style();
    }
}

/* Render a chunk, recording what it wrote while any section is live so
 * HideOutputSection can retract it later.  With no section open (every game
 * that never calls StartOutputSection, and the headless harness) this is
 * render_html_inner plus one branch. */
void render_html(const std::string &raw)
{
    if (g_sections.empty()) {
        if (g_swallow && swallow_echo_chunk(raw))
            return;
        render_html_inner(raw);
        return;
    }
    OutChunk c;
    std::u32string *saved = g_tee;   /* replay re-enters through here */
    g_tee = &c.text;
    const size_t images = g_images_drawn;
    /* The swallow runs INSIDE the tee: when it decides a provisionally
     * swallowed blank was not the echo after all it puts that newline back,
     * and the recorded text has to account for it or the retract below stops
     * matching the window. */
    const bool swallowed = g_swallow && swallow_echo_chunk(raw);
    if (!swallowed)
        render_html_inner(raw);
    g_tee = saved;
    c.image = g_images_drawn != images;
    /* A swallowed chunk's own html must never be replayed -- the swallow is
     * spent, so re-rendering it would print the echo this time. All it can
     * leave behind is that restored newline. */
    c.html = swallowed ? "<br/>" : raw;
    if (!swallowed || !c.text.empty()) {
        g_out_log.push_back(c);
        trim_out_log();
    }
}

/* ---------------------------------------------------------------- banner -- */

/* Flatten a scrap of game HTML to plain text for the banner / pane labels:
 * tags stripped, entities decoded.  Newlines (from <br/>) become the given
 * separator. */
std::string plain_text(const std::string &html, const char *brsep = " ")
{
    std::string out;
    bool intag = false;
    for (size_t i = 0; i < html.size(); i++) {
        char c = html[i];
        if (intag) {
            if (c == '>') intag = false;
            continue;
        }
        if (c == '<') {
            /* <br> variants become the separator */
            std::string t = lower(html.substr(i, 5));
            if (t.compare(0, 3, "<br") == 0 && !out.empty())
                out += brsep;
            intag = true;
            continue;
        }
        out += c;
    }
    return decode_entities(out);
}

/* Current room: what Core last sent through JS.updateLocation
 * (CapFirst(GetDisplayName(game.pov.parent)) -- the reference player's
 * location bar).  Old v500-era games embed pre-JS libraries that never call
 * it, so fall back to resolving game.pov.parent -- or the conventional
 * `player` object, Core's own StartGame fallback, since those games predate
 * game.pov too.  Purely cosmetic; any resolution failure blanks the banner. */
void update_banner(Interp &in)
{
    World &w = in.world();
    std::string room = g_location_line;
    if (room.empty()) {
        Element *game = nullptr;
        for (Element *r : w.roots)
            if (r->kind == ElemKind::Game) { game = r; break; }
        const Value *pov = game ? in.resolve_field(game, "pov") : nullptr;
        Element *pl = pov && pov->type == Value::Type::ObjectRef
                          ? w.find(pov->str) : nullptr;
        if (!pl)
            pl = w.find("player");
        const Value *par = pl ? in.resolve_field(pl, "parent") : nullptr;
        Element *rm = par && par->type == Value::Type::ObjectRef
                          ? w.find(par->str) : nullptr;
        if (rm) {
            const Value *alias = in.resolve_field(rm, "alias");
            room = alias && alias->type == Value::Type::String ? alias->str
                                                               : rm->name;
        }
        room = cap_first(room);
    }
    g_room_name = plain_text(room);

    /* Room name left, status attributes (JS.updateStatus: "Score: 3 |
     * Health: 90%") right-aligned when they fit, in codepoint-aware UTF-8
     * mode. */
    draw_status_banner(gbanner, g_room_name, g_status_line, true);
}

/* ------------------------------------------------------------- side pane -- */

/* Open the right-hand pane (and its divider) if the host supports splits.
 * The same arrangement as the classic runner's objwin (shared code). */
void ensure_pane_open()
{
    if (gobjwin || !g_use_objpane)
        return;
    open_side_pane_windows(gwin, &gobjwin, &gdivider);
    fill_pane_divider();
}

/* Close the pane so the main window reclaims the width. */
void close_side_pane()
{
    close_side_pane_windows(&gobjwin, &gdivider);
}

/* Paint the divider in the main text colour.  Graphics windows are blanked
 * on resize, so Arrange/Redraw events call this again. */
void fill_pane_divider()
{
    fill_side_divider(gwin, gdivider);
}

/* A registered [template] text, later registration winning (like the loader's
 * replace_templates). */
std::string template_text_or(World &w, const char *name, const char *fallback)
{
    for (auto it = w.templates.rbegin(); it != w.templates.rend(); ++it)
        if (it->first == name)
            return it->second;
    return fallback;
}

/* game.compassdirections (localized at load time via the [CompassN]...
 * templates): the reference UI filters these aliases out of "Places and
 * Objects" so they only appear in the compass. */
std::vector<std::string> compass_directions(Interp &in)
{
    std::vector<std::string> dirs;
    Element *game = nullptr;
    for (Element *r : in.world().roots)
        if (r->kind == ElemKind::Game) { game = r; break; }
    const Value *v = game ? in.resolve_field(game, "compassdirections") : nullptr;
    if (v && v->list_store)
        for (const Value &e : *v->list_store)
            if (e.type == Value::Type::String)
                dirs.push_back(lower(e.str));
    return dirs;
}

/* Repaint the side pane from the stored lists when they changed.  The pane is
 * shown only while it has something to list (Quest's panes are permanent, but
 * an empty sidebar is dead width in a Glk layout -- the classic runner's
 * objwin behaves the same way).  Each item is a hyperlink: objects run their
 * first display verb ("Look at hat"), exits go their direction. */
void redraw_side_pane(Interp &in)
{
    if (!g_use_objpane || !g_pane_dirty)
        return;
    g_pane_dirty = false;

    std::vector<std::string> dirs = compass_directions(in);
    auto is_dir = [&](const ListData &d) {
        std::string a = lower(d.display_alias);
        for (const std::string &x : dirs)
            if (x == a)
                return true;
        return false;
    };
    std::vector<const ListData *> inv, places, exits;
    for (const ListData &d : g_pane_inv)
        inv.push_back(&d);
    for (const ListData &d : g_pane_places)
        if (!is_dir(d))
            places.push_back(&d);
    for (const ListData &d : g_pane_exits)
        if (is_dir(d))
            exits.push_back(&d);

    if (inv.empty() && places.empty() && exits.empty()) {
        close_side_pane();
        return;
    }
    ensure_pane_open();
    if (!gobjwin) {
        /* The host cannot split after all; stop asking the engine for lists. */
        g_use_objpane = false;
        return;
    }

    glk_window_clear(gobjwin);
    g_pane_links.clear();
    strid_t s = glk_window_get_stream(gobjwin);
    bool first = true;

    auto section = [&](const std::string &header,
                       const std::vector<const ListData *> &items,
                       bool exit_links) {
        if (items.empty())
            return;
        if (!first)
            glk_put_char_stream(s, '\n');
        first = false;
        put_pane_header(s, header, true);
        for (const ListData *d : items) {
            std::string label = cap_first(plain_text(d->text));
            if (label.empty())
                label = cap_first(d->display_alias);
            glui32 linkval = 0;
            if (g_hyperlinks) {
                LinkAction act;
                /* A direction runs itself; an object unfolds its verb list
                 * in place, the way QuestViva pops a verb menu on a click. */
                if (exit_links)
                    act.command = d->display_alias;
                else
                    act.toggle_key = d->element_name;
                g_pane_links.push_back(act);
                linkval = kPaneLinkBase + (glui32) g_pane_links.size() - 1;
            }
            put_pane_link(s, label, linkval, true);
            if (exit_links || d->element_name != g_pane_expanded)
                continue;
            /* The unfolded verb menu: one indented link per verb, each
             * running the command the reference menu item would ("Look at
             * hat") -- the same phrasing VERBS lists. */
            for (const std::string &verb : d->verbs) {
                std::string vtext = plain_text(verb);
                if (vtext.empty())
                    continue;
                glui32 vlink = 0;
                if (g_hyperlinks) {
                    LinkAction act;
                    act.command = vtext + " " + d->display_alias;
                    g_pane_links.push_back(act);
                    vlink = kPaneLinkBase + (glui32) g_pane_links.size() - 1;
                }
                glk_put_string_stream(s, (char *) "    ");
                put_pane_link(s, cap_first(vtext), vlink, true);
            }
        }
    };

    World &w = in.world();
    section(template_text_or(w, "InventoryLabel", "Inventory"), inv, false);
    section(template_text_or(w, "PlacesObjectsLabel", "Places and Objects"),
            places, false);
    section(template_text_or(w, "CompassLabel", "Compass"), exits, true);
    fill_pane_divider();
}

/* ----------------------------------------------------------------- input -- */

/* What ended an input request. */
enum class InEnd {
    Line,       /* text is the entered line */
    Command,    /* text is a hyperlink's command, to run as if typed */
    Event,      /* an ASLEvent hyperlink already ran; state may have changed */
    State,      /* a timer changed engine state (menu/question/wait/finish) */
};

struct InResult {
    InEnd kind = InEnd::Line;
    std::string text;
};

std::string utf8_from_uni(const glui32 *buf, glui32 n)
{
    std::string out;
    for (glui32 i = 0; i < n; i++)
        utf8_append(out, buf[i]);
    return out;
}

void echo_input(const std::string &line)
{
    echo_input_line(line, true);
}

/* Echo a host-owned metaverb (RESTORE, TRANSCRIPT) the way Core's
 * echocommand records a real command -- blank line, "> cmd" -- since the
 * game side never sees it and parser-bound lines are otherwise not
 * host-echoed. */
void echo_metaverb(const std::string &cmd)
{
    glk_put_string((char *) "\n> ");
    echo_input(cmd);
}

/* Fire an ASLEvent hyperlink (Core menu options): WorldModel.SendEventCore,
 * which also runs FinishTurn (v540..579) and refreshes the panes -- exactly
 * what the reference player does with an onclick.  send_event catches its
 * own throws. */
void run_asl_event(Interp &in, const LinkAction &act)
{
    in.send_event(act.event_func, act.event_param);
    in.drain_on_ready();
}

/* JS callback completions queued by the js_event_bridge hook: (js function,
 * its last argument).  Fired between turns, never mid-script. */
std::vector<std::pair<std::string, std::string> > g_js_events;

/* Run the JS completion callbacks a game's own <js> animations would have
 * fired from setTimeout (see Interp::js_event_bridge).  Only an argument that
 * really names a <function> is fired -- anything else is data that happened to
 * ride along in the last slot, and firing it would invent behaviour.  One
 * completion legitimately chains into the next (spondre's title fades in three
 * stages), so this drains until quiet, with a cap in case a game builds a
 * cycle the browser would have broken with real elapsed time. */
void fire_js_events(Interp &in)
{
    const int kMaxChain = 16;
    for (int fired = 0; !g_js_events.empty(); ) {
        std::pair<std::string, std::string> ev = g_js_events.front();
        g_js_events.erase(g_js_events.begin());
        /* "Func" or "Func param": the split the games' own JS does before
         * calling ASLEvent(args[0], args[1]). */
        std::string func = trim(ev.second), param;
        size_t sp = func.find(' ');
        if (sp != std::string::npos) {
            param = trim(func.substr(sp + 1));
            func.erase(sp);
        }
        if (func.empty())
            continue;
        Element *h = in.world().find(func);
        if (!h || h->kind != ElemKind::Function)
            continue;
        if (++fired > kMaxChain) {
            /* Not silently: past the cap a game is either cycling or doing
             * something this bridge models badly, and the callbacks dropped
             * here are real work the browser would have run.  Says what was
             * refused, so the truncation is diagnosable from the log rather
             * than showing up as a game that mysteriously stops advancing. */
            fprintf(stderr,
                    "JS callback chain exceeded %d in one turn: dropped '%s'"
                    " and %zu more still queued\n",
                    kMaxChain, func.c_str(), g_js_events.size());
            g_js_events.clear();
            break;
        }
        in.send_event(func, param);
        in.drain_on_ready();
    }
}

/* True when the engine is waiting for something other than a command line --
 * the prompt loop must re-dispatch. */
bool engine_state_pending(Interp &in)
{
    return in.pending_menu() || in.pending_question() || in.pending_wait() ||
           in.world().finished;
}

/* Scope guard: reroute engine prints so the first output emitted while a
 * prompt sits on the screen retracts that stale prompt first -- an
 * interrupting timer then reads as a clean run of text with a single prompt
 * at the bottom, exactly like the reference player, instead of a bare "> "
 * stranded above every tick.  broke tells the caller a reprint of the prompt
 * is needed.
 *
 * The retract is attempted only on the first output, after the caller has
 * cancelled the line request: with echo control the cancel already removed
 * any typed text (GlkTextBufferWindow+Input.m cancelLine), so the prompt
 * really is the window tail and unput_exact removes it whole.  Whenever it
 * does not (the window moved on, no echo control so the typed text is still
 * on screen, CheapGlk's no-op unput), the fallback is the old behaviour:
 * break the line so the output lands cleanly under the stale prompt.  Typed
 * text survives either way -- the caller re-requests input with the
 * cancelled line as preload (ce.val1). */
struct PromptBreak {
    Interp &in;
    const char *prompt;
    bool broke = false;
    std::function<void(const std::string &)> saved;
    PromptBreak(Interp &i, const char *p) : in(i), prompt(p) {
        if (!prompt) return;
        saved = in.print;  /* restore the PRIOR handler on exit -- a nested
                            * redirect must not be stomped back to render_html */
        in.print = [this](const std::string &h) {
            if (!broke && !h.empty()) {
                if (!unput_exact(u32_from_utf8(prompt)))
                    glk_put_char('\n');
                broke = true;
            }
            render_html(h);
        };
    }
    ~PromptBreak() {
        if (prompt) in.print = saved;
    }
};

/* Request one line of input.  `prompt` is printed first (and reprinted after
 * any engine output that interrupts the request); pass nullptr when the
 * caller printed its own or none applies.  `on_screen` = the prompt text is
 * already at the end of the window (an autorestored snapshot ends with it),
 * so it is not printed on entry but still describes what an interrupting
 * tick must retract or reprint -- passing nullptr instead would leave
 * PromptBreak blind and a tick's output would land ON the prompt line with
 * no prompt reprinted after it.  `echo` = the HOST echoes the
 * accepted line (host-owned prompts, and every prompt-first line).  It is
 * false only for legacy-mode parser commands, which the GAME side echoes
 * itself -- Core's game.echocommand prints "> cmd" into the transcript (the
 * engine does it for pre-v520 games), exactly like the reference player,
 * whose input box leaves no other record.  Library echo is off either way
 * (when controllable), so the typed line is atomically replaced by whichever
 * echo applies.  Timer events tick the engine; if a tick opens a prompt or
 * ends the game the request is cancelled and InEnd::State returned. */
InResult read_line(Interp &in, bool echo, const char *prompt = nullptr,
                   bool want_line = true, bool on_screen = false)
{
    /* Per-frame, NOT static: a timer tick below can open an expression-form
     * ShowMenu/Ask -> nested read_line, which with a shared buffer would
     * clobber the player's half-typed line (cancelled into buf, re-requested
     * with ce.val1 preload).  Every return path completes or cancels the
     * request, so the request never outlives the frame. */
    glui32 buf[256];
    /* Links are the only other way in, so a library without them always gets
     * the line request (CheapGlk: the smoke harness). */
    if (!g_hyperlinks)
        want_line = true;
    if (prompt && !on_screen)
        glk_put_string((char *) prompt);
    if (g_echo_control)
        glk_set_echo_line_event(gwin, 0);
#ifdef SPATTERLIGHT
    /* Parser prompt: autosave after the prompt is printed (so the GUI
     * snapshot ends with it) and before input is requested (so the saved
     * windows carry no pending request and a restore re-enters here). */
    if (g_autosave_interp)
        aslx_do_autosave(*g_autosave_interp);
#endif
    if (want_line)
        glk_request_line_event_uni(gwin, buf, 255, 0);
    if (g_hyperlinks) {
        glk_request_hyperlink_event(gwin);
        if (gobjwin)
            glk_request_hyperlink_event(gobjwin);
    }
    for (;;) {
        event_t ev;
        glk_select(&ev);
        switch (ev.type) {
        case evtype_LineInput:
            if (ev.win == gwin) {
                std::string line = utf8_from_uni(buf, ev.val1);
                if (g_manual_echo && echo)
                    echo_input(line);
                return {InEnd::Line, line};
            }
            break;
        case evtype_Hyperlink:
            if (ev.win == gwin && ev.val1 > g_links_spent &&
                ev.val1 <= g_links.size()) {
                event_t ce;
                if (want_line)
                    glk_cancel_line_event(gwin, &ce);
                const LinkAction &act = g_links[ev.val1 - 1];
                if (act.end_wait) {
                    /* "click to continue".  A pending wait is what the
                     * browser's endWait() releases, so release it (this arm
                     * is rarely taken: the prompt loop tests pending_wait
                     * before the line branch, so a click during a wait is
                     * usually consumed by read_keypress). */
                    if (in.pending_wait()) {
                        PromptBreak pb(in, prompt);
                        in.finish_wait();
                        in.drain_on_ready();
                        return {InEnd::Event, ""};
                    }
                    /* No wait: in the browser endWait() does nothing and the
                     * click bubbles to the game's own click-anywhere handler,
                     * which sends a command -- that, not endWait, is what
                     * moves spondre off its title.  Stand in for it when the
                     * game installed such a handler.  No echo: a click is its
                     * own visible act (see the line-echo note below). */
                    if (g_click_hook)
                        return {InEnd::Command, kClickHookCommand};
                    return {InEnd::Event, ""};
                }
                if (!act.command.empty()) {
                    if (g_manual_echo && echo)
                        echo_input(act.command);
                    return {InEnd::Command, act.command};
                }
                {
                    /* Event output must not land on the prompt line. */
                    PromptBreak pb(in, prompt);
                    run_asl_event(in, act);
                }
                return {InEnd::Event, ""};
            }
            if (ev.win == gobjwin && ev.val1 >= kPaneLinkBase &&
                ev.val1 - kPaneLinkBase < g_pane_links.size()) {
                /* By value: the toggle path rebuilds g_pane_links. */
                const LinkAction act = g_pane_links[ev.val1 - kPaneLinkBase];
                if (!act.toggle_key.empty()) {
                    /* An object name: fold its verb list open or shut and
                     * repaint the pane.  No turn passes, so the line request
                     * in the main window stays exactly as it was. */
                    g_pane_expanded =
                        g_pane_expanded == act.toggle_key ? "" : act.toggle_key;
                    g_pane_dirty = true;
                    redraw_side_pane(in);
                    if (g_hyperlinks) {
                        glk_request_hyperlink_event(gwin);
                        if (gobjwin)
                            glk_request_hyperlink_event(gobjwin);
                    }
                    break;
                }
                /* A verb or a direction: run its command as if typed. */
                event_t ce;
                if (want_line)
                    glk_cancel_line_event(gwin, &ce);
                if (g_manual_echo && echo)
                    echo_input(act.command);
                return {InEnd::Command, act.command};
            }
            if (g_hyperlinks) {
                glk_request_hyperlink_event(gwin);
                if (gobjwin)
                    glk_request_hyperlink_event(gobjwin);
            }
            break;
        case evtype_Timer: {
            /* A fast tick advancing the map glide leaves the pending line
             * request alone; only a whole engine second runs the tick
             * machinery below. */
            if (!timer_event_second(in))
                break;
            /* Cancel first: Glk forbids output with a live line request.
             * With echo off the cancel is invisible. */
            event_t ce;
            ce.val1 = 0;
            if (want_line)
                glk_cancel_line_event(gwin, &ce);
            bool reprompt;
            {
                PromptBreak pb(in, prompt);
                in.tick(1);
                in.drain_on_ready();
                reprompt = pb.broke;
            }
            update_banner(in);
            redraw_side_pane(in);
            redraw_grid_map();
            update_timer_request(in);  /* a timer script may have moved the
                                        * player and started a glide */
            if (engine_state_pending(in))
                return {InEnd::State, ""};
            if (reprompt)
                glk_put_string((char *) prompt);
#ifdef SPATTERLIGHT
            /* The tick changed game state while the prompt was up; refresh
             * the autosave (it skips itself when the autosave-on-timer
             * preference is off). */
            if (g_autosave_interp)
                aslx_do_autosave(*g_autosave_interp);
#endif
            if (want_line)
                glk_request_line_event_uni(gwin, buf, 255, ce.val1);
            if (g_hyperlinks) {
                glk_request_hyperlink_event(gwin);
                if (gobjwin)
                    glk_request_hyperlink_event(gobjwin);
            }
            break;
        }
        case evtype_Arrange:
        case evtype_Redraw:
            update_banner(in);
            fill_pane_divider();
            grid_map_arrange();
            panel_arrange();
            break;
        }
    }
}

/* One keypress (a pending `wait`).  Timer events keep ticking.  A click on a
 * hyperlink -- in the main window or the side pane -- counts as the keypress
 * that dismisses the wait, matching the reference player where any input
 * continues past "Press any key to continue". */
void read_keypress(Interp &in)
{
    glk_request_char_event(gwin);
    if (g_hyperlinks) {
        glk_request_hyperlink_event(gwin);
        if (gobjwin)
            glk_request_hyperlink_event(gobjwin);
    }
    for (;;) {
        event_t ev;
        glk_select(&ev);
        if (ev.type == evtype_CharInput && ev.win == gwin) {
            if (g_hyperlinks) {
                glk_cancel_hyperlink_event(gwin);
                if (gobjwin)
                    glk_cancel_hyperlink_event(gobjwin);
            }
            return;
        }
        if (ev.type == evtype_Hyperlink &&
            (ev.win == gwin || ev.win == gobjwin)) {
            glk_cancel_char_event(gwin);
            if (g_hyperlinks) {
                glk_cancel_hyperlink_event(gwin);
                if (gobjwin)
                    glk_cancel_hyperlink_event(gobjwin);
            }
            return;
        }
        if (ev.type == evtype_Timer) {
            if (!timer_event_second(in))
                continue;
            in.tick(1);
            update_banner(in);
            redraw_side_pane(in);
            redraw_grid_map();
            update_timer_request(in);
            if (in.world().finished || !in.pending_wait()) {
                glk_cancel_char_event(gwin);
                if (g_hyperlinks) {
                    glk_cancel_hyperlink_event(gwin);
                    if (gobjwin)
                        glk_cancel_hyperlink_event(gobjwin);
                }
                return;
            }
            /* Clearing the pane drops its hyperlink request; re-arm it. */
            if (g_hyperlinks && gobjwin)
                glk_request_hyperlink_event(gobjwin);
        }
        if (ev.type == evtype_Arrange || ev.type == evtype_Redraw) {
            update_banner(in);
            fill_pane_divider();
            grid_map_arrange();
        }
    }
}

bool iequal(const std::string &a, const std::string &b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (tolower((unsigned char) a[i]) != tolower((unsigned char) b[i]))
            return false;
    return true;
}

/* Present a MenuData and return the selected key, or false for cancel.
 * Shared by the script-command prompt (set_menu_response) and the
 * expression-form provider (menu_provider), which is why it does not touch
 * the pending slot itself. */
bool run_menu_ui(Interp &in, const MenuData &m, std::string &key)
{
    /* Through in.print, not render_html: with a PromptBreak active (a menu
     * opened by a timer tick) the first output must retract the stale prompt
     * first, exactly like every other engine print. */
    if (!m.caption.empty()) {
        in.print(m.caption);
        glk_put_char('\n');
    }
    for (size_t i = 0; i < m.options.size(); i++) {
        char num[16];
        snprintf(num, sizeof num, "%zu: ", i + 1);
        in.print(num + m.options[i].second);
        glk_put_char('\n');
    }
    for (;;) {
        char pr[64];
        snprintf(pr, sizeof pr, "\nChoose [1-%zu]%s> ", m.options.size(),
                 m.allow_cancel ? " (or nothing to cancel)" : "");
        InResult r = read_line(in, true, pr);
        if (r.kind == InEnd::State)
            return false;                     /* timer ended the world */
        if (r.kind == InEnd::Event)
            continue;
        std::string l = trim(r.text);
        if (l.empty() && m.allow_cancel)
            return false;
        /* number, exact key, or display text (the replayer's resolution) */
        char *end = nullptr;
        long n = strtol(l.c_str(), &end, 10);
        if (end && *end == 0 && n >= 1 && (size_t) n <= m.options.size()) {
            key = m.options[n - 1].first;
            return true;
        }
        for (auto &kv : m.options)
            if (kv.first == l || iequal(kv.second, l)) {
                key = kv.first;
                return true;
            }
        glk_put_string((char *) "Please choose one of the numbered options.\n");
    }
}

/* Present a yes/no question and return the answer.  The engine never prints
 * the caption (ShowQuestion is host UI), so render it here.  Shared by the
 * `ask` script-command prompt (set_question_response) and the expression-form
 * provider (ask_provider), which is why it does not touch the pending slot.
 * Returns false when the world ended under the prompt (no answer to give). */
bool run_question_ui(Interp &in, const std::string &q, bool &answer)
{
    in.print(q);  /* through the prompt-retract path; see run_menu_ui */
    for (;;) {
        InResult r = read_line(in, true, " (yes/no) > ");
        if (r.kind == InEnd::State)
            return false;                     /* timer ended the world */
        if (r.kind != InEnd::Line)
            continue;
        std::string a = lower(trim(r.text));
        if (a == "yes" || a == "y") { answer = true;  return true; }
        if (a == "no"  || a == "n") { answer = false; return true; }
        glk_put_string((char *) "Please answer YES or NO.");
    }
}

/* ---------------------------------------------------------- save/restore -- */

std::string core_dir_path();   /* defined in the core section below */

const char *g_storyfile = nullptr;

/* Write the engine snapshot to a player-chosen file. Runs mid-turn, from the
 * request (RequestSave) hook -- Core's `save` command -- so there is never a
 * pending line request here. */
void do_save_ui(Interp &in)
{
    prompt_write_save(in.save_game(g_storyfile ? g_storyfile : ""));
}

/* Prompt for a save file and read it, validating it against a scratch reload
 * of the game (so a wrong-game or corrupt save never tears down the running
 * session). Returns true with `data` filled when the caller should reboot
 * the session from it. */
bool do_restore_ui(std::string &data)
{
    if (!prompt_read_save(data))
        return false;

    /* Accept both our v1 snapshot and a native Quest/QuestViva `.quest-save`
     * (the interoperable format -- a save exported from the desktop Quest
     * player or QuestViva imports here). restore_game auto-detects which. */
    if (!Interp::is_save_data(data.data(), data.size()) &&
        !Interp::is_native_save_data(data.data(), data.size())) {
        glk_put_string((char *) "Sorry, that does not look like a saved game"
                                " for this story.\n");
        return false;
    }
    /* Full validation: apply it to a scratch world before committing. */
    World probe_w;
    std::string err = "could not reload the game";
    if (load_file(g_storyfile ? g_storyfile : "", probe_w, core_dir_path())) {
        Interp probe(probe_w);
        probe.print = [](const std::string &) {};
        if (probe.restore_game(data, err))
            return true;
    }
    glk_put_string((char *) "Sorry, that save cannot be restored (");
    put_uni_string(err);
    glk_put_string((char *) ").\n");
    return false;
}

/* The RESTORE metaverb. Quest has no restore command of its own (loading is
 * a UI action, like the classic desktop player's menu), so the frontend owns
 * it; `save` stays with Core's own command, which lands in request_save. */
bool is_restore_command(const std::string &raw, bool asking)
{
    std::string c = lower(trim(raw));
    /* Bare "load" is the one form a game's own `get input` question might
     * plausibly be answered with, so it yields to a pending question; the
     * unambiguous forms are honoured whenever they are typed. */
    if (c == "load")
        return !asking;
    return c == "restore" || c == "restore game" || c == "load game";
}

/* ------------------------------------------------------------- metaverbs -- */

/* #HELP: list the system commands, the way the classic runner's #HELP does for
 * Quest 4.  Only some of them are the frontend's -- RESTORE, the transcript
 * pair, VERBS and #HELP itself; SAVE, RESTART, UNDO and QUIT are Core's own
 * commands.  What they have in common, and what the player wants from this
 * list, is that they work in any game rather than being one game's invention,
 * so the list names them all and the heading claims no more than that.
 *
 * Kept behind the '#' so it never shadows the game's own HELP (Core's
 * DefaultHelp template, which the game may localize or replace); the
 * plain-HELP hint below points players at it. */
bool handle_help_command(const std::string &raw)
{
    std::string c = lower(trim(raw));
    if (c != "#help" && c != "#commands" && c != "metaverbs")
        return false;
    if (!g_prompt_first)        /* prompt-first: the host already echoed it */
        echo_metaverb(raw);

    glk_put_string((char *)
        "\nThese system commands work in any game, whether Quest itself or"
        " this interpreter handles them:\n"
        "\n"
        "  SAVE              Save the whole game to a file.\n"
        "  RESTORE  (LOAD)   Restore a previously saved game.\n"
        "  RESTART           Start the game over from the beginning.\n"
        "  UNDO              Take back the last turn.\n"
        "  QUIT              Stop playing and leave Quest.\n"
        "\n"
        "  SCRIPT   (TRANSCRIPT)       Start recording the game text to a file.\n"
        "  SCRIPT OFF  (UNSCRIPT)      Stop recording the transcript.\n"
        "\n"
        "  VERBS <object>    List the actions available for an object, the\n"
        "                    same menu clicking its name pops up.\n"
        "  STATUS            Show the status bar's text in full, for when it\n"
        "                    is too long to fit beside the room name.\n"
        "  HELP              Show the game's own in-game help.\n"
        "  #HELP             Show this list of system commands.\n");
    return true;
}

/* After a plain HELP, point the player at #HELP -- the game's own help never
 * mentions the commands this port adds.  English only: HELP itself is matched
 * by the game's (possibly localized) command pattern, so a game in another
 * language reaches its help by a word we would not recognize here, and saying
 * nothing beats attaching an English footnote to Greek help text. */
void hint_system_commands(const std::string &raw, bool parser_game)
{
    /* Only on a parser turn: a gamebook has no HELP command at all (the line
     * would only ever follow "no page named 'help'"), and under a `get input`
     * the word is the game's answer, not a command. */
    if (!parser_game || lower(trim(raw)) != "help")
        return;
    glk_put_string((char *)
        "\n(Type #HELP for the SAVE, RESTORE, UNDO and other system"
        " commands.)\n");
}

/* STATUS: print the status attributes in full.  The banner is one grid line
 * and cuts a status too long for it down from the left, so this is how the
 * player reads the fields that did not fit.  Shared wording and formatting
 * with the classic runner. */
bool handle_status_command(const std::string &raw)
{
    if (!match_status_command(raw))
        return false;
    if (!g_prompt_first)        /* prompt-first: the host already echoed it */
        echo_metaverb(raw);
    print_status_report(g_status_line, true);
    return true;
}

/* Transcript recording, same commands (and shared code) as the classic
 * runner. */
bool handle_transcript_command(const std::string &raw)
{
    int on = match_transcript_command(raw);
    if (!on)
        return false;
    if (!g_prompt_first)        /* prompt-first: the host already echoed it */
        echo_metaverb(raw);
    toggle_transcript(on, gwin, &gtranscript);
    return true;
}

/* VERBS <object>: list the verb menu a QuestViva object-name hyperlink would
 * pop up for that object -- its displayverbs, or inventoryverbs when carried.
 * The reference player only exposes these by clicking a name; this makes the
 * same list typable.  A pure front-end convenience, handled like the other
 * metaverbs (it never reaches the engine's parser). */
bool handle_verbs_command(Interp &in, const std::string &raw)
{
    std::string t = trim(raw);
    std::string c = lower(t);
    if (c != "verbs" && c.rfind("verbs ", 0) != 0)
        return false;
    if (!g_prompt_first)        /* prompt-first: the host already echoed it */
        echo_metaverb(raw);

    strid_t s = glk_window_get_stream(gwin);

    /* The object name, minus a leading article the parser would also drop. */
    std::string want = lower(trim(t.substr(5)));
    if (want.rfind("the ", 0) == 0)      want = trim(want.substr(4));
    else if (want.rfind("an ", 0) == 0)  want = trim(want.substr(3));
    else if (want.rfind("a ", 0) == 0)   want = trim(want.substr(2));

    if (want.empty()) {
        glk_put_string((char *) "Type VERBS followed by an object name, "
                                "e.g. VERBS lamp.\n");
        return true;
    }

    /* Match a currently-clickable object by the alias a typed command uses
     * (or its pane label), inventory winning a clash -- see verb_menu_objects. */
    const ListData *match = nullptr;
    std::vector<ListData> objs = in.verb_menu_objects();
    for (const ListData &d : objs) {
        if (lower(trim(d.display_alias)) == want ||
            lower(trim(plain_text(d.text))) == want) {
            match = &d;
            break;
        }
    }

    if (!match) {
        /* The game's own localized template ("Δεν βλέπω κάτι τέτοιο."), run
         * through Core's ASL text processor when this Core has one -- the
         * template may carry {if:}/{random:} directives (Dream Pieces). */
        std::string t = template_text_or(in.world(), "UnresolvedObject",
                                         "You can't see any such thing.");
        Element *pt = in.world().find("ProcessText");
        if (pt && pt->kind == ElemKind::Function) {
            Value v = in.call_function("ProcessText", {vstr(t)}, nullptr);
            if (v.type == Value::Type::String)
                t = v.str;
        } else {
            /* Pre-580 Cores have no ProcessText wrapper; their OutputText
             * calls ProcessTextSection(text, data) with a fresh dictionary
             * carrying "fulltext" -- do the same. */
            pt = in.world().find("ProcessTextSection");
            if (pt && pt->kind == ElemKind::Function) {
                Value data;
                data.type = Value::Type::StringDict;
                data.dict().push_back({"fulltext", vstr(t)});
                Value v = in.call_function("ProcessTextSection",
                                           {vstr(t), data}, nullptr);
                if (v.type == Value::Type::String)
                    t = v.str;
            }
        }
        put_stream_utf8(s, plain_text(t) + "\n");
        return true;
    }

    /* The reference UI pops a bare verb menu on an object-name click -- no
     * prose around it -- and English framing sits wrongly in a translated
     * game ("Verbs for τόξο: εξέτασε, πάρε").  Print the label and the verb
     * list uncommented; an (unlikely) empty list is just the label. */
    std::string alias = plain_text(match->display_alias);
    put_stream_utf8(s, cap_first(alias));
    for (size_t i = 0; i < match->verbs.size(); i++) {
        put_stream_utf8(s, i ? ", " : ": ");
        put_stream_utf8(s, plain_text(match->verbs[i]));
    }
    put_stream_utf8(s, ".\n");
    return true;
}

/* ------------------------------------------------------------------ core -- */

/* Locate the bundled Core libraries: $ASLX_CORE, else next to the terp
 * binary (../Resources/aslx-core inside the app bundle; ./aslx-core for
 * development builds). */
std::string core_dir_path()
{
    const char *env = getenv("ASLX_CORE");
    if (env && *env)
        return env;
#ifdef __APPLE__
    char exe[4096];
    uint32_t size = sizeof exe;
    if (_NSGetExecutablePath(exe, &size) == 0) {
        char real[4096];
        if (realpath(exe, real)) {
            std::string dir = real;
            size_t slash = dir.rfind('/');
            if (slash != std::string::npos)
                dir.erase(slash);
            std::string cands[] = {
                dir + "/../Resources/aslx-core",   /* app bundle */
                dir + "/aslx-core",                /* dev layout */
            };
            for (const std::string &c : cands)
                if (access((c + "/Core.aslx").c_str(), R_OK) == 0)
                    return c;
        }
    }
#endif
    return "";
}

/* ------------------------------------------------------------- resources -- */

/* The .quest package's entry table, listed once at startup. Empty for raw
 * .aslx games, whose resources live next to the game file instead
 * (QuestViva's FileDirectoryGameDataProvider). */
std::vector<ZipEntryInfo> g_package_entries;
bool g_is_package = false;
std::string g_story_dir;

void init_resources(const char *storyfile)
{
    g_package_entries.clear();
    g_is_package = false;
    g_story_dir.clear();
    if (!storyfile)
        return;
    std::string path = storyfile;
    size_t slash = path.find_last_of("/\\");
    g_story_dir = slash == std::string::npos ? "." : path.substr(0, slash);
    std::string buf;
    if (!slurp_file(path, buf) || buf.size() < 4 ||
        memcmp(buf.data(), "PK\x03\x04", 4) != 0)
        return;
    g_is_package = zip_list_entries((const uint8_t *) buf.data(), buf.size(),
                                    g_package_entries);
}

/* Read one byte range of a file (a zip entry's payload). */
bool read_file_range(const std::string &path, size_t offset, size_t len,
                     std::string &out)
{
    FILE *f = fopen(path.c_str(), "rb");
    if (!f)
        return false;
    out.resize(len);
    bool ok = fseek(f, (long) offset, SEEK_SET) == 0 &&
              fread(&out[0], 1, len, f) == len;
    fclose(f);
    return ok;
}

/* Bytes of a game resource by name: a package entry (exact match first,
 * then case-insensitive -- QuestViva's resource table is OrdinalIgnoreCase)
 * or a file next to a raw .aslx game. ".." is rejected like
 * WorldModel.GetExternalUrlAsync does. */
bool resource_bytes(const std::string &name, std::string &out)
{
    if (name.empty() || name.find("..") != std::string::npos)
        return false;
    if (g_is_package) {
        const ZipEntryInfo *e = zip_find_entry(g_package_entries, name);
        if (!e)
            return false;
        std::string comp;
        if (!read_file_range(g_storyfile ? g_storyfile : "", e->offset,
                             e->comp_size, comp))
            return false;
        if (e->method == 0) { out = std::move(comp); return true; }
        if (e->method != 8)
            return false;
        return inflate_raw((const uint8_t *) comp.data(), comp.size(),
                           e->raw_size, out);
    }
    if (g_story_dir.empty())
        return false;
    return slurp_file(g_story_dir + "/" + name, out);
}

#ifdef SPATTERLIGHT
/* Quest's image and sound files are external and arbitrarily named, so
 * register each with the backend under a private resource number (the
 * win_find / win_load pre-registration shared with the classic runner --
 * see questglk-common.inc) and let glk_image_draw* / glk_schannel_play*
 * short-circuit their PIC<n>/SND<n> blorb lookup. */

/* filename (case-folded) -> registered resource number; 0 = known-failed. */
std::map<std::string, glui32> g_image_ids;
std::map<std::string, glui32> g_sound_ids;
int g_image_next_id = 1;
int g_sound_next_id = 1;

/* Stage bytes in a temporary file for the backend to load.  An IMAGE file
 * is only needed until the load round-trips (see load_media_resource),
 * after which it is unlinked.  A SOUND file must outlive the session: the
 * app re-reads it from a bookmark when it resumes an interrupted sound
 * after an autorestore (SoundHandler restartAll), so it is staged in
 * glkimp's per-session temp directory -- swept on a normal glk_exit but
 * deliberately preserved on an autosave-quit, exactly like the temp files
 * backing open file streams -- and never unlinked here. */
bool stage_temp_file(const std::string &bytes, std::string &path,
                     bool keep_for_session = false)
{
    std::string t;
#ifdef SPATTERLIGHT
    if (keep_for_session) {
        gettempdir();
        if (tempdir[0])
            t = tempdir;
    }
#else
    (void) keep_for_session;
#endif
    if (t.empty()) {
        const char *tmpdir = getenv("TMPDIR");
        t = (tmpdir && *tmpdir) ? tmpdir : "/tmp";
    }
    if (t.back() != '/')
        t += '/';
    t += "aslximgXXXXXX";
    std::vector<char> buf(t.begin(), t.end());
    buf.push_back('\0');
    int fd = mkstemp(buf.data());
    if (fd < 0)
        return false;
    bool ok = write(fd, bytes.data(), bytes.size()) == (ssize_t) bytes.size();
    close(fd);
    path.assign(buf.data());
    if (!ok) {
        unlink(path.c_str());
        path.clear();
        return false;
    }
    return true;
}

/* Resolve + register one image or sound resource. Returns its resource
 * number, or 0. A stored package entry is read by the app straight from the
 * .quest at its byte offset; a deflated entry (the common case -- Quest's
 * packager deflates everything) or an adjacent file goes through
 * resource_bytes and a temp file. The verification round-trip to the app
 * (glk_image_get_info / win_findsound) both confirms the resource really
 * decoded and means the temp file has been consumed. */
glui32 load_media_resource(const std::string &name, bool sound)
{
    glui32 id = 0;
    std::string temp_path;
    int &next_id = sound ? g_sound_next_id : g_image_next_id;

    const ZipEntryInfo *e =
        g_is_package && name.find("..") == std::string::npos
            ? zip_find_entry(g_package_entries, name) : nullptr;
    if (e && e->method == 0 && g_storyfile) {
        id = (glui32) (next_id++);
        if (sound)
            win_loadsound((int) id, (char *) g_storyfile, (int) e->offset,
                          (int) e->comp_size);
        else
            win_loadimage((int) id, g_storyfile, (int) e->offset,
                          (int) e->comp_size);
    } else {
        std::string bytes;
        if (!resource_bytes(name, bytes) || bytes.empty())
            return 0;
        if (!stage_temp_file(bytes, temp_path, /*keep_for_session=*/sound))
            return 0;
        id = (glui32) (next_id++);
        if (sound)
            win_loadsound((int) id, (char *) temp_path.c_str(), 0,
                          (int) bytes.size());
        else
            win_loadimage((int) id, temp_path.c_str(), 0, (int) bytes.size());
    }

    glui32 w, h;
    bool ok = sound ? win_findsound((int) id) != 0
                    : glk_image_get_info(id, &w, &h) != 0;
    if (!ok)
        id = 0;
    /* Images are consumed by the round-trip; staged sounds stay for the
     * app's autorestore restart (glkimp's tempdir sweep owns them). */
    if (!temp_path.empty() && !sound)
        unlink(temp_path.c_str());
    return id;
}

/* Cached name -> resource-number lookup (failures cached as 0, so an
 * unresolvable file is only chased once). */
glui32 cached_media_id(std::map<std::string, glui32> &ids,
                       const std::string &name, bool sound)
{
    auto it = ids.find(lower(name));
    if (it != ids.end())
        return it->second;
    glui32 id = load_media_resource(name, sound);
    ids[lower(name)] = id;
    return id;
}
#endif /* SPATTERLIGHT */

/* One sound channel, like the reference player's single <audio> element: a
 * new `play sound` replaces the current one, `stop sound` silences it
 * (shared single-channel semantics -- see questglk-common.inc). */
schanid_t g_schannel = nullptr;

void stop_sound_ui()
{
    stop_single_sound(&g_schannel);
}

/* A hand-written <audio> element's sound effect (see the renderer's audio
 * branch).  Takes the first source that resolves, like a browser taking the
 * first <source> it can decode -- L Too offers .ogg then .mp3, and the app
 * plays both.  Never blocks: an autoplaying element is asynchronous, unlike
 * the synchronous `play sound` above.  True when playback started. */
bool play_audio_inline(const std::vector<std::string> &srcs, bool loop)
{
#ifdef SPATTERLIGHT
    if (!glk_gestalt(gestalt_Sound2, 0))
        return false;
    for (const std::string &src : srcs) {
        glui32 id = cached_media_id(g_sound_ids, src, true);
        if (id && play_single_sound(&g_schannel, id, loop, 0))
            return true;
    }
#else
    /* No way to register sounds by name outside Spatterlight's Glk. */
    (void) srcs; (void) loop;
#endif
    return false;
}

/* Block until the playing sound reports finished -- the synchronous
 * `play sound`, whose turn resumes only when the sound ends (QuestViva's
 * awaited wait slot). A keypress skips: the sound is stopped and the turn
 * resumes at once (also the safety valve if the notification never comes).
 * Timer events are deliberately ignored -- the turn is suspended, engine
 * ticks resume at prompt level. */
void wait_for_sound(Interp &in, glui32 id)
{
    glk_request_char_event(gwin);
    for (;;) {
        event_t ev;
        glk_select(&ev);
        if (ev.type == evtype_SoundNotify && ev.val1 == id) {
            glk_cancel_char_event(gwin);
            return;
        }
        if (ev.type == evtype_CharInput && ev.win == gwin) {
            stop_sound_ui();
            return;
        }
        if (ev.type == evtype_Arrange || ev.type == evtype_Redraw) {
            update_banner(in);
            grid_map_arrange();
        }
    }
}

/* The play_sound host hook. Degrades to an instantly-finished sound whenever
 * it cannot really play (sound disabled, unresolvable resource): the turn
 * always continues -- only the hook-less headless engine keeps the oracle's
 * abandoned-turn semantics. A looping synchronous sound never finishes, so
 * it plays without blocking; deterministic mode (gli_sa_delays off) never
 * blocks, like the timer heartbeat. */
void play_sound_ui(Interp &in, const std::string &name, bool sync, bool loop)
{
#ifdef SPATTERLIGHT
    if (!glk_gestalt(gestalt_Sound2, 0))
        return;

    glui32 id = cached_media_id(g_sound_ids, name, true);
    if (!id)
        return;

    bool block = sync && !loop && gli_sa_delays;
    if (!play_single_sound(&g_schannel, id, loop, block ? 1 : 0))
        return;
    if (block)
        wait_for_sound(in, id);
#else
    /* No way to register sounds by name outside Spatterlight's Glk. */
    (void) in; (void) name; (void) sync; (void) loop;
#endif
}

/* The do_wait host hook (pre-v540 `request (Wait)` -- Core's WaitForKeyPress).
 * Block mid-script until the player presses a key or clicks a link, exactly
 * like the reference player's "press any key to continue". Deterministic mode
 * (gli_sa_delays off, as in the import tests and the smoke harness) never
 * blocks -- the request just returns, keeping transcripts wall-clock free. No
 * game-time tick happens: a Quest wait is real-world only. */
void do_wait_ui(Interp &in)
{
    if (!gli_sa_delays)
        return;
    glk_request_char_event(gwin);
    if (g_hyperlinks) {
        glk_request_hyperlink_event(gwin);
        if (gobjwin)
            glk_request_hyperlink_event(gobjwin);
    }
    for (;;) {
        event_t ev;
        glk_select(&ev);
        if ((ev.type == evtype_CharInput && ev.win == gwin) ||
            (ev.type == evtype_Hyperlink &&
             (ev.win == gwin || ev.win == gobjwin))) {
            glk_cancel_char_event(gwin);
            if (g_hyperlinks) {
                glk_cancel_hyperlink_event(gwin);
                if (gobjwin)
                    glk_cancel_hyperlink_event(gobjwin);
            }
            return;
        }
        if (ev.type == evtype_Arrange || ev.type == evtype_Redraw) {
            update_banner(in);
            fill_pane_divider();
            grid_map_arrange();
        }
    }
}

/* The do_pause host hook (pre-v550 `request (Pause, ms)` -- Core's Pause).
 * Block mid-script for `ms` milliseconds using a one-shot timer; a keypress
 * skips it (the safety valve). Deterministic mode never blocks. The
 * prompt-level timer is overridden here and torn down afterwards --
 * g_timer_ms is reset so the next update_timer_request re-arms it. */
void do_pause_ui(Interp &in, int ms)
{
    if (!gli_sa_delays || ms <= 0)
        return;
    if (grid_map_animating()) {
        /* The one-shot pause owns the timer; finish the glide at once. */
        grid_map_anim_snap();
        redraw_grid_map();
    }
    glk_request_timer_events((glui32) ms);
    glk_request_char_event(gwin);
    for (;;) {
        event_t ev;
        glk_select(&ev);
        if (ev.type == evtype_Timer) {
            glk_cancel_char_event(gwin);
            break;
        }
        if (ev.type == evtype_CharInput && ev.win == gwin)
            break;
        if (ev.type == evtype_Arrange || ev.type == evtype_Redraw) {
            update_banner(in);
            fill_pane_divider();
            grid_map_arrange();
        }
    }
    glk_request_timer_events(0);
    g_timer_ms = 0;
}

bool draw_image_inline(const std::string &name)
{
#ifdef SPATTERLIGHT
    if (!glk_gestalt(gestalt_Graphics, 0) ||
        !glk_gestalt(gestalt_DrawImage, wintype_TextBuffer))
        return false;

    glui32 id = cached_media_id(g_image_ids, name, false);
    if (!id)
        return false;

    /* An image is an attachment character in the window's text storage, which
     * no unput string can match -- a chunk that drew one pins its output
     * section (see hide_output_section). */
    g_images_drawn++;

    /* Original size with the aspect ratio preserved, but never wider than
     * the window -- re-resolved on every resize (Glk 0.7.6
     * glk_image_draw_scaled_ext). Baseline-aligned like an HTML <img>;
     * Core's OutputText supplies the surrounding <br/>s. */
    return glk_image_draw_scaled_ext(gwin, id, imagealign_InlineUp, 0,
                                     0, 0x10000,
                                     imagerule_WidthOrig | imagerule_AspectRatio,
                                     0x10000) != 0;
#else
    /* No way to register images by name outside Spatterlight's Glk. */
    (void) name;
    return false;
#endif
}

/* The frame picture (JS.setPanelContents) -------------------------------
 *
 * Quest's picture frame is a PERSISTENT panel above the transcript, not a
 * picture in it: Core sets it from the room's `picture` attribute on every
 * room change (OnEnterRoom -> SetFramePicture) and re-sets it at boot and
 * restore (InitInterface). The reference player's #gamePanel is
 * `position: sticky; top: 32px`, full width, contents centred, hidden when
 * empty, with the image capped at half the window height
 * (playercore.js: `(window.innerHeight - 30) * 0.5`) -- a sibling of
 * #gridPanel, which is why this gets the same treatment as the grid map.
 *
 * Drawn inline instead, a room picture ate most of the viewport, pushed the
 * room description below the fold, and then scrolled away for good, since a
 * re-set of the same file is deduped (The Shack: every room has art).
 *
 * Hosts without graphics (CheapGlk, the smoke harness) keep the old inline
 * path, so their transcripts are unchanged. */
winid_t gpanelwin = nullptr;    /* the band, open only while a picture is set */
std::string g_panel_last;       /* case-folded filename currently in the frame */
glui32 g_panel_id = 0;          /* its Glk image id */

bool panel_band_available()
{
#ifdef SPATTERLIGHT
    /* The band needs the by-name image registry (g_image_ids), which only
     * Spatterlight's Glk provides -- see cached_media_id. */
    return glk_gestalt(gestalt_Graphics, 0) &&
           glk_gestalt(gestalt_DrawImage, wintype_Graphics);
#else
    return false;
#endif
}

/* The band's own background: the buffer's style_Normal background, so the
 * letterbox around a picture that does not fill the band matches the text
 * below it under any theme (Glk offers no way to read a window's background
 * directly -- probe the style hint, as the Comprehend runner does). */
void panel_set_background()
{
    if (!gpanelwin)
        return;
    glui32 bg;
    if (!glk_style_measure(gwin, style_Normal, stylehint_BackColor, &bg))
        bg = 0x00FFFFFF;
    glk_window_set_background_color(gpanelwin, bg);
}

void panel_close()
{
    if (gpanelwin) {
        glk_window_close(gpanelwin, nullptr);
        gpanelwin = nullptr;
    }
    g_panel_id = 0;
    g_panel_last.clear();
}

/* Size the band to the picture and repaint it.  Re-run on every arrange, so
 * the picture re-fits when the window is resized. */
void panel_relayout()
{
    if (!gpanelwin || !g_panel_id)
        return;
    glui32 iw = 0, ih = 0;
    if (!glk_image_get_info(g_panel_id, &iw, &ih) || !iw || !ih)
        return;

    /* Glk hands out no window-pixel metric, so probe: parked at 50% the
     * band's own height IS the reference's max-height, and its width is the
     * full content width. */
    winid_t parent = glk_window_get_parent(gpanelwin);
    if (parent)
        glk_window_set_arrangement(parent,
                                   winmethod_Above | winmethod_Proportional,
                                   50, gpanelwin);
    glui32 bw = 0, maxh = 0;
    glk_window_get_size(gpanelwin, &bw, &maxh);
    if (!bw || !maxh)
        return;

    /* CSS max-width/max-height only ever shrink -- never blow a small
     * picture up to fill the band. */
    double scale = 1.0;
    if (iw > bw)
        scale = (double) bw / (double) iw;
    if ((double) ih * scale > (double) maxh)
        scale = (double) maxh / (double) ih;
    glui32 dw = (glui32) ((double) iw * scale);
    glui32 dh = (glui32) ((double) ih * scale);
    if (!dw || !dh)
        return;

    if (parent)
        glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                                   dh, gpanelwin);
    glk_window_get_size(gpanelwin, &bw, &maxh);
    panel_set_background();     /* a theme change arrives as an arrange */
    glk_window_clear(gpanelwin);
    glk_image_draw_scaled(gpanelwin, g_panel_id,
                          (glsi32) (bw > dw ? (bw - dw) / 2 : 0), 0, dw, dh);
}

void panel_arrange()
{
    if (gpanelwin)
        panel_relayout();
}

void panel_contents(const std::string &html)
{
    std::string src = decode_entities(tag_attr(html, "src"));
    if (src.empty()) {          /* ClearFramePicture (or non-img contents) */
        if (panel_band_available())
            panel_close();
        else
            g_panel_last.clear();
        return;
    }
    if (lower(src) == g_panel_last)
        return;

    if (!panel_band_available()) {
        /* No band: the old inline draw, so a picture is at least seen. */
        if (draw_image_inline(src)) {
            g_panel_last = lower(src);
            glk_put_char('\n');
        }
        return;
    }

#ifdef SPATTERLIGHT
    glui32 id = cached_media_id(g_image_ids, src, false);
#else
    glui32 id = 0;
#endif
    if (!id)                    /* unresolvable: leave the frame as it was */
        return;
    g_panel_id = id;
    g_panel_last = lower(src);
    if (!gpanelwin) {
        gpanelwin = glk_window_open(glk_window_get_root(),
                                    winmethod_Above | winmethod_Proportional,
                                    50, wintype_Graphics, 0);
        if (!gpanelwin) {       /* refused the split: fall back to inline */
            g_panel_id = 0;
            if (draw_image_inline(src))
                glk_put_char('\n');
            return;
        }
        panel_set_background();
    }
    panel_relayout();
}

/* Print any warnings the engine queued since the last check ("'play sound'
 * is not supported yet" etc.), bracketed and emphasized. */
void flush_warnings(World &w, size_t &seen)
{
    for (; seen < w.warnings.size(); seen++) {
        glk_set_style(style_Emphasized);
        glk_put_string((char *) "[");
        put_uni_string(w.warnings[seen]);
        glk_put_string((char *) "]\n");
        glk_set_style(style_Normal);
    }
}

/* How a session ended, and what the next one boots from.  Resume never leaves
 * run_session: it is the post-game menu telling the turn loop to carry on. */
enum class SessionEnd { Quit, Restart, Restore, Resume };

/* Undo a turn after the story ended, putting the player back in play.
 *
 * QuestViva simply stops when a game finishes -- there is no undo to reach,
 * because there is no prompt left to type it at.  The classic runner has
 * offered a post-death undo for as long as it has had the menu, and it is the
 * one choice there that does not throw the session away, so the two ports now
 * agree on offering it.  Everything below the frontend is Core's own undo:
 * rollback_transaction is what the `undo` command's UndoScript runs, prints
 * the game's own UndoTurn template, and honours whatever the game logged.
 * Only clearing `finished` is ours -- `finish` sets a runtime flag, not an
 * element field, so no undo action recorded it. */
bool post_game_undo(Interp &in)
{
    /* A game stopped by the script-error breaker has not "ended" so much as
     * broken; rolling one turn back would just run into the same wall. */
    if (in.script_errors_fatal() || !in.undo_available()) {
        glk_put_string((char *) "There is nothing to undo.\n");
        return false;
    }
    Context ctx;
    in.rollback_transaction(ctx);
    in.world().finished = false;
    in.drain_on_ready();
    return true;
}

/* After the game ends: undo, restore, restart or quit -- the same four
 * choices, in the same words, as the classic runner's menu (the text and the
 * matching are shared, in questglk-common.inc). */
SessionEnd post_game_menu(Interp &in, std::string &restore_data)
{
    post_game_menu_print();
    for (;;) {
        InResult r = read_line(in, true, "> ");
        if (r.kind != InEnd::Line)
            continue;
        switch (post_game_menu_match(r.text)) {
        case POSTGAME_UNDO:
            if (post_game_undo(in))
                return SessionEnd::Resume;
            break;              /* nothing to undo; ask again */
        case POSTGAME_RESTORE:
            if (do_restore_ui(restore_data))
                return SessionEnd::Restore;
            break;              /* cancelled or unreadable; ask again */
        case POSTGAME_RESTART:
            return SessionEnd::Restart;
        case POSTGAME_QUIT:
            glk_put_string((char *) "\nThanks for playing. Goodbye!\n");
            return SessionEnd::Quit;
        default:
            post_game_menu_reprompt();
            break;
        }
    }
}

/* Arm the shared Glk timer to the fastest cadence anyone needs: the map
 * marker glide's fast tick while one is in flight, else the 1-second
 * heartbeat while any engine timer is enabled, else off.  Gated on the
 * real-time preference like the classic runner, so the deterministic import
 * tests see no wall-clock events (a glide never starts without it either --
 * grid_map_command snaps). */
void update_timer_request(Interp &in)
{
    bool heart = gli_sa_delays && in.next_timer_seconds() > 0;
    glui32 want = grid_map_animating() ? GM_ANIM_TICK_MS
                : heart ? 1000 : 0;
    if (want != g_timer_ms) {
        glk_request_timer_events(want);
        if (g_timer_ms == 0)
            g_timer_frac_ms = 0;  /* fresh cadence, fresh second */
        g_timer_ms = want;
    }
}

/* Run one full game session, booting fresh or from a validated snapshot
 * (which do_restore_ui already probe-applied). `restore_data` is consumed on
 * entry and re-filled when the session ends with a RESTORE. */
#ifdef SPATTERLIGHT

/* ---- Spatterlight autosave ---------------------------------------------- */

/* The frontend state blob: a tiny length-prefixed text format carried inside
 * the library plist.  Window/stream/channel identities cross as glkimp
 * serialization tags.  The transcript hyperlink actions (g_links) must
 * survive because the restored transcript still shows their links; the map,
 * the panes and the banner are repainted from restored ENGINE state on the
 * way into the session (InitInterface re-runs Grid_Redraw and the panel,
 * update_lists refills the pane and status), so none of that is saved. */

void blob_num(std::string &s, long v)
{
    s += std::to_string(v);
    s += '\n';
}

void blob_str(std::string &s, const std::string &v)
{
    blob_num(s, (long) v.size());
    s += v;
    s += '\n';
}

/* The chunk log's recorded text is UTF-32 (what went to the window); the blob
 * is bytes. */
std::string utf8_from_u32(const std::u32string &s)
{
    std::vector<glui32> cps(s.begin(), s.end());
    return utf8_from_uni(cps.empty() ? nullptr : cps.data(), (glui32) cps.size());
}

struct BlobReader {
    const std::string &s;
    size_t pos = 0;
    bool ok = true;

    long num()
    {
        if (!ok)
            return 0;
        size_t e = s.find('\n', pos);
        if (e == std::string::npos) {
            ok = false;
            return 0;
        }
        long v = atol(s.c_str() + pos);
        pos = e + 1;
        return v;
    }
    std::string str()
    {
        long n = num();
        if (!ok || n < 0 || pos + (size_t) n + 1 > s.size()) {
            ok = false;
            return "";
        }
        std::string v = s.substr(pos, (size_t) n);
        pos += (size_t) n + 1;
        return v;
    }
};

/* Bumped to 2 when the output-section log joined the blob: a version-1 blob
 * has no such fields, and an autorestore that mis-parses is worse than one
 * that is discarded for a fresh start. */
const char *const kAslxBlobMagic = "ASLXGLK-AUTOSAVE 2";

std::string aslx_encode_frontend(Interp &in)
{
    std::string b;
    blob_str(b, kAslxBlobMagic);
    blob_num(b, gwin ? gwin->tag : 0);
    blob_num(b, gbanner ? gbanner->tag : 0);
    blob_num(b, gobjwin ? gobjwin->tag : 0);
    blob_num(b, gdivider ? gdivider->tag : 0);
    blob_num(b, gpanelwin ? gpanelwin->tag : 0);
    blob_num(b, gmapwin ? gmapwin->tag : 0);
    blob_num(b, gtranscript ? gtranscript->tag : 0);
    blob_num(b, g_schannel ? g_schannel->tag : 0);
    blob_num(b, (long) g_links_spent);
    blob_num(b, (long) g_links.size());
    /* Spent links keep their slot (window link ids index into g_links) but
     * their payloads are never consulted again -- serialize them empty so an
     * autosave's size is bounded by the LIVE tail, not the whole session. */
    for (size_t i = 0; i < g_links.size(); i++) {
        const LinkAction &l = g_links[i];
        const bool spent = i < g_links_spent;
        blob_str(b, spent ? std::string() : l.command);
        blob_str(b, spent ? std::string() : l.event_func);
        blob_str(b, spent ? std::string() : l.event_param);
        blob_str(b, spent ? std::string() : l.toggle_key);
        blob_num(b, !spent && l.end_wait ? 1 : 0);
        blob_num(b, !spent && l.inner_text ? 1 : 0);
    }
    blob_str(b, g_pane_expanded);
    blob_str(b, g_status_line);
    blob_str(b, g_location_line);
    blob_str(b, g_panel_last);
    /* The exact RNG streams (fallback + per compiled expression), so a
     * deterministic session's randomness continues across an autorestore. */
    std::vector<std::pair<std::string, std::array<uint32_t, 4>>> rngs;
    in.capture_rng_streams(rngs);
    blob_num(b, (long) rngs.size());
    for (const auto &entry : rngs) {
        blob_str(b, entry.first);
        for (int i = 0; i < 4; i++)
            blob_num(b, (long) entry.second[i]);
    }
    /* Output sections and the chunk log behind them.  The restored transcript
     * still shows the text they cover, so without these the first hide after
     * an autorestore -- which for a gamebook is the very next click -- would
     * find no section and leave the previous page's options on screen.  The
     * recorded text goes over as UTF-8. */
    blob_num(b, (long) g_out_log.size());
    for (const OutChunk &c : g_out_log) {
        blob_str(b, c.html);
        blob_str(b, utf8_from_u32(c.text));
        blob_num(b, c.image ? 1 : 0);
    }
    blob_num(b, (long) g_sections.size());
    for (const OutSection &s : g_sections) {
        blob_str(b, s.name);
        blob_num(b, (long) s.start);
        blob_num(b, s.end == (size_t) -1 ? -1 : (long) s.end);
    }
    return b;
}

bool aslx_recover_frontend(const std::string &blob)
{
    BlobReader r{blob};
    if (r.str() != kAslxBlobMagic)
        return false;
    gwin = gli_window_for_tag((int) r.num());
    gbanner = gli_window_for_tag((int) r.num());
    gobjwin = gli_window_for_tag((int) r.num());
    gdivider = gli_window_for_tag((int) r.num());
    gpanelwin = gli_window_for_tag((int) r.num());
    gmapwin = gli_window_for_tag((int) r.num());
    gtranscript = gli_stream_for_tag((int) r.num());
    g_schannel = gli_schan_for_tag((int) r.num());
    g_links_spent = (size_t) r.num();
    long nlinks = r.num();
    g_links.clear();
    for (long i = 0; i < nlinks && r.ok; i++) {
        LinkAction l;
        l.command = r.str();
        l.event_func = r.str();
        l.event_param = r.str();
        l.toggle_key = r.str();
        l.end_wait = r.num() != 0;
        l.inner_text = r.num() != 0;
        g_links.push_back(l);
    }
    g_pane_expanded = r.str();
    g_status_line = r.str();
    g_location_line = r.str();
    g_autorestore_panel = r.str();
    g_autorestore_rngs.clear();
    long nrngs = r.num();
    for (long i = 0; i < nrngs && r.ok; i++) {
        std::string src = r.str();
        std::array<uint32_t, 4> s;
        for (int j = 0; j < 4; j++)
            s[(size_t) j] = (uint32_t) r.num();
        g_autorestore_rngs.emplace_back(src, s);
    }
    g_out_log.clear();
    long nchunks = r.num();
    for (long i = 0; i < nchunks && r.ok; i++) {
        OutChunk c;
        c.html = r.str();
        c.text = u32_from_utf8(r.str());
        c.image = r.num() != 0;
        g_out_log.push_back(c);
    }
    g_sections.clear();
    long nsecs = r.num();
    for (long i = 0; i < nsecs && r.ok; i++) {
        OutSection s;
        s.name = r.str();
        s.start = (size_t) r.num();
        long e = r.num();
        s.end = e < 0 ? (size_t) -1 : (size_t) e;
        g_sections.push_back(s);
    }
    if (!r.ok || !gwin)
        return false;

    /* The redraw paths dedup against "what is already on screen"; void
     * those keys so the post-restore repaints actually repaint. */
    g_panel_last.clear();
    g_panel_id = 0;
    gm_screen_drop();
    g_pane_dirty = true;
    /* Sounds are NOT resumed from here: the app archives its sound channels
     * (playing sound, loop count, pause/volume state) in its own GUI
     * snapshot and replays them itself (SoundHandler restartAll) -- and it
     * REPLACES its sound handler wholesale during its restore, so anything
     * the terp started before that point would keep playing as an orphan no
     * stop can ever reach.  The terp's only job is keeping the staged sound
     * files alive across the restart (see stage_temp_file). */
    return true;
}

/* Re-establish the frame picture after an autorestore, unless the game's
 * own InitInterface already set one. */
void panel_restore_picture()
{
    std::string src = g_autorestore_panel;
    g_autorestore_panel.clear();
    if (src.empty() || g_panel_id || !gpanelwin || !panel_band_available())
        return;
    glui32 id = cached_media_id(g_image_ids, src, false);
    if (!id)
        return;
    g_panel_id = id;
    g_panel_last = src;
    panel_relayout();
}

/* Autosave at the parser prompt: the engine snapshot (the same v1 format
 * SAVE writes), the frontend blob, the Glk library plist, and the GUI
 * snapshot request, in that order. */
void aslx_do_autosave(Interp &in)
{
    if (!geas_autosave_wanted())
        return;
    std::string engine_state = in.save_game(g_storyfile ? g_storyfile : "");
    if (engine_state.empty())
        return;
    aslx_do_autosave_write(engine_state, aslx_encode_frontend(in));
}

#endif /* SPATTERLIGHT */

SessionEnd run_session(const char *storyfile, std::string &restore_data)
{
    World w;
    if (!load_file(storyfile, w, core_dir_path())) {
        glk_put_string((char *) "Sorry, this Quest 5 game could not be loaded:\n");
        for (const std::string &e : w.errors) {
            glk_put_string((char *) "  ");
            put_uni_string(e);
            glk_put_char('\n');
        }
        return SessionEnd::Quit;
    }

    Interp in(w);
    in.print = render_html;
    in.menu_provider = [&in](const MenuData &m, std::string &key) -> bool {
        return run_menu_ui(in, m, key);
    };
    in.ask_provider = [&in](const std::string &q, bool &answer) -> bool {
        return run_question_ui(in, q, answer);
    };
    in.request_save = [&in] { do_save_ui(in); };
    /* Core's `restart` command (via JS.eval window.location.reload).  The
     * turn finishes normally -- under a browser reload the server-side turn
     * also runs to completion -- and the session ends at the next loop
     * check, rebooting through the same teardown as the post-game menu. */
    bool restart_requested = false;
    in.request_restart = [&restart_requested] { restart_requested = true; };
    /* Pane lists (UpdateListsAsync): only subscribe when this Glk could open
     * a side pane at startup -- an unset hook skips the whole scope
     * computation, like QuestViva with no UpdateList listener.  The hook just
     * snapshots; redraw_side_pane repaints at the next safe point. */
    if (g_use_objpane) {
        in.update_list = [](const char *type, const std::vector<ListData> &items) {
            if (strcmp(type, "inventory") == 0)
                g_pane_inv = items;
            else if (strcmp(type, "placesobjects") == 0)
                g_pane_places = items;
            else if (strcmp(type, "exits") == 0)
                g_pane_exits = items;
            g_pane_dirty = true;
        };
    }
    /* Status attributes land in the banner, next to the room name. */
    in.update_status = [](const std::string &html) {
        g_status_line = plain_text(html, " | ");
    };
    in.update_location = [](const std::string &text) {
        g_location_line = plain_text(text);
    };
    /* The command box's visibility -- see g_command_bar. */
    in.show_command_bar = [](bool shown) { g_command_bar = shown; };
    /* A taken page choice retires the options above it -- see g_links_spent.
     * Called before the new page prints, so the links it is about to add are
     * above the mark and stay live. */
    in.disable_command_links = [] {
        /* A spent link is never dispatched again (val1 > g_links_spent), so
         * its payload strings are dead -- release them, or a long gamebook
         * session grows the vector's heap without bound. */
        for (size_t i = g_links_spent; i < g_links.size(); i++)
            g_links[i] = LinkAction{};
        g_links_spent = g_links.size();
    };
    /* Output sections -- the reference player's hideable <div>s, retracted
     * here with garglk_unput_string_count_uni. */
    in.start_output_section = [](const std::string &n) { start_output_section(n); };
    in.end_output_section = [](const std::string &n) { end_output_section(n); };
    in.hide_output_section = [](const std::string &n) { hide_output_section(n); };
    /* Script errors go to stderr, not the page -- the reference web player
     * keeps them in the browser's JavaScript console, off the transcript. The
     * engine still logs every one (Interp::errors()), and headless output is
     * untouched: only a host that installs this hook diverts them. */
    /* The JS callback bridge: queue only -- fire_js_events runs them between
     * turns, so a callback never lands in the middle of the script that
     * triggered it (the browser's setTimeout would not either). */
    in.js_event_bridge = [](const std::string &fn, const std::string &arg) {
        /* JS.HookClicks: the game has installed a click-anywhere handler --
         * see g_click_hook. */
        if (fn == "HookClicks")
            g_click_hook = true;
        else if (!arg.empty())
            g_js_events.push_back(std::make_pair(fn, arg));
    };
    in.script_error = [](const std::string &message) {
        fprintf(stderr, "%s\n", message.c_str());
    };
    /* The grid map needs only filled rectangles, so plain gestalt_Graphics
     * gates it.  Unset (CheapGlk: the smoke harness), the engine never even
     * evaluates the paint arguments, keeping headless transcripts intact. */
    if (glk_gestalt(gestalt_Graphics, 0))
        in.grid_draw = grid_map_command;
    /* Pre-540 `picture` shows the image through the UI directly (no <img>
     * print), and JS.setPanelContents is the picture frame. Only claim them
     * when this Glk can actually draw -- unset, the engine keeps its
     * one-time "not supported" warning / silent ignore. */
    if (glk_gestalt(gestalt_Graphics, 0) &&
        glk_gestalt(gestalt_DrawImage, wintype_TextBuffer)) {
        in.show_picture = [](const std::string &filename) {
            if (draw_image_inline(filename))
                glk_put_char('\n');
        };
        g_panel_last.clear();   /* a fresh session redraws its frame */
        in.set_panel_contents = panel_contents;
    }
#ifdef SPATTERLIGHT
    /* Sounds play through a Glk sound channel; a synchronous `play sound`
     * blocks in the hook until the finish notification (or a keypress).
     * Installed unconditionally so an unplayable sound "finishes" instantly
     * and the turn always continues -- only the hook-less headless engine
     * (the oracle-parity harnesses) keeps the abandoned-turn semantics. */
    in.play_sound = [&in](const std::string &f, bool sync, bool loop) {
        play_sound_ui(in, f, sync, loop);
    };
    in.stop_sound = stop_sound_ui;
#endif

    /* The pre-v540/v550 blocking `request (Wait)` / `request (Pause, ms)`
     * (Core's WaitForKeyPress / Pause). Standard Glk keypress + timer events,
     * so wired on every host; both self-suppress in deterministic mode, so the
     * smoke harness never blocks. */
    in.do_wait = [&in] { do_wait_ui(in); };
    in.do_pause = [&in](int ms) { do_pause_ui(in, ms); };

#ifdef GLK_MODULE_GARGLKTEXT
    {
        /* Game names may carry markup ("<b><i> DRACULA </b></i>"); the
         * window title wants plain text.  game_name itself must stay raw --
         * it is the save-state identity. */
        std::string title = trim(plain_text(w.game_name));
        if (!title.empty())
            garglk_set_story_title(title.c_str());
    }
#endif

    size_t warnings_seen = 0;

    /* Saved-game boot, mirroring WorldModel.BeginInternalAsync with
     * _loadedFromSaved: timers are not re-armed, InitInterface re-runs,
     * StartGame does not, and the reference's no-transcript fallback message
     * is printed. */
    bool restored = false;
    bool autorestored = false;
    if (!restore_data.empty()) {
        std::string err;
        restored = in.restore_game(restore_data, err);
        restore_data.clear();
        if (!restored) {
            /* Should not happen -- the probe validated it -- but never boot
             * half-applied state silently. */
            glk_put_string((char *) "Restore failed (");
            put_uni_string(err);
            glk_put_string((char *) "); starting a new game.\n\n");
            return SessionEnd::Restart;
        }
    }
#ifdef SPATTERLIGHT
    /* A Spatterlight autosave: the engine state first (restore_game overlays
     * the snapshot onto this freshly loaded world -- silent), then the Glk
     * library from the plist, then the frontend globals from the blob.  The
     * boot then continues down the saved-game path: InitInterface re-runs
     * (repainting the grid map and panes from restored engine state),
     * StartGame and begin_timers are skipped.  The app restores the window
     * contents from its own GUI snapshot. */
    else if (g_autorestore_pending) {
        g_autorestore_pending = false;
        std::string engine_state, blob, err;
        bool ok = aslx_autosave_read_game(&engine_state) &&
                  in.restore_game(engine_state, err) &&
                  aslx_autosave_restore_library(&blob) &&
                  aslx_recover_frontend(blob);
        if (!ok) {
            /* Bad autosave: delete it and restart in a fresh process rather
             * than continuing from half-applied state. */
            geas_autosave_discard();
            win_reset();
            exit(0);
        }
        aslx_autosave_restore_library_late();
        /* The library snapshot re-arms the Glk timer interval that was in
         * force when the autosave was taken (TempLibrary restores
         * gtimerinterval) -- typically the map glide's 33 ms cadence, since
         * the snapshot is taken at the prompt right after a move started a
         * glide.  This process's mirror of that interval starts at 0, where
         * timer_event_second reads EVERY tick as a whole engine second: the
         * prompt loop would then cancel and re-request line input 30 times a
         * second, and Spatterlight scrolls the buffer to the bottom on each
         * request -- the window fights any attempt to scroll back.  Cancel
         * the restored interval and let update_timer_request below arm the
         * cadence this session actually needs. */
        glk_request_timer_events(0);
        g_timer_ms = 0;
        g_timer_frac_ms = 0;
        restored = true;
        autorestored = true;
        g_autorestore_reentry = true;
    }
#endif

    Context boot;
    if (!restored)
        in.begin_timers();
    try {
        if (w.find("InitInterface")) in.call_function("InitInterface", {}, &boot);
        if (!restored && w.find("StartGame"))
            in.call_function("StartGame", {}, &boot);
        /* BeginInternalAsync ends with UpdateListsAsync: first pane fill +
         * status attributes (a park above skips it, like the await chain). */
        in.update_lists();
    } catch (const TurnSuspended &) {
        /* synchronous `play sound` during boot: rest of Begin abandoned */
    }
    if (restored && !autorestored) {
        glk_put_string((char *) "Loaded saved game\n");
        update_banner(in);
    }
#ifdef SPATTERLIGHT
    if (autorestored)
        panel_restore_picture();
#endif
    in.drain_on_ready();
    fire_js_events(in);
    flush_warnings(w, warnings_seen);
    update_banner(in);
    redraw_side_pane(in);
    redraw_grid_map();
    update_timer_request(in);

#ifdef SPATTERLIGHT
    /* Put every RNG stream back where the autosave left it, now that the
     * boot (InitInterface, list refills, repaints) has taken its own draws
     * -- so the restored prompt's randomness is exactly the saved one. */
    if (!g_autorestore_rngs.empty()) {
        in.restore_rng_streams(g_autorestore_rngs);
        g_autorestore_rngs.clear();
    }
#endif

    /* Turn loop, then the end-of-story menu -- which may undo back into play,
     * in which case the whole thing runs again. */
    bool fatal_reported = false;
    for (;;) {
    while (!w.finished) {
        if (restart_requested)
            return SessionEnd::Restart;
        if (const MenuData *m = in.pending_menu()) {
            std::string key;
            bool picked = run_menu_ui(in, *m, key);
            in.set_menu_response(picked ? &key : nullptr);
        } else if (const std::string *q = in.pending_question()) {
            bool answer = false;
            if (run_question_ui(in, *q, answer))
                in.set_question_response(answer);
        } else if (in.pending_wait()) {
            read_keypress(in);
            if (in.pending_wait())
                in.finish_wait();
        } else {
            /* Parser-bound lines.  Prompt-first (g_prompt_first): print a
             * standard "> " prompt at the input point, host-echo the accepted
             * line, and arm g_swallow so the game side's own echo of the
             * command ("> cmd" -- Core's echocommand, or the engine for
             * pre-v520) is dropped instead of doubling it.  Legacy mode (no
             * line-input echo control, e.g. CheapGlk): no prompt, no host
             * echo -- the game-side echo is the only record, like the
             * reference player, whose input box leaves no other trace.  A
             * pending `get input` is host-owned either way: the engine
             * consumes the line without any game-side echo. */
            bool host_owned = in.command_override();
            /* With the command box hidden (a gamebook, or any game with
             * showcommandbar off) the page and its links are the interface,
             * so no prompt is drawn.  A line is still accepted, though: the
             * reference player backs a hidden box with a click hook this
             * engine cannot emulate (spondre's glue.js sends a command for
             * ANY click in the output, which is the only way past its title),
             * and a host that both hides the prompt and refuses a line leaves
             * no way in at all.  Hiding the box must cost the prompt, not the
             * player's last input path.  A `get input` is host-owned and
             * re-shows the box, so it prompts as usual. */
            bool bar_shown = g_command_bar || host_owned;
            bool prompted = (g_prompt_first || host_owned) && bar_shown;
            const char *prompt = prompted ? "\n> " : nullptr;
            bool prompt_on_screen = false;
#ifdef SPATTERLIGHT
            /* The first prompt after an autorestore is already on screen --
             * the GUI snapshot was taken just after it was printed.  Keep
             * the prompt text (an interrupting tick must still know what to
             * retract and reprint); just skip printing it again. */
            if (g_autorestore_reentry) {
                g_autorestore_reentry = false;
                prompt_on_screen = prompt != nullptr;
            }
            /* Autosave while the parser prompt is up.  Not for a pending
             * `get input`: its callback is not in the engine snapshot. */
            if (!host_owned)
                g_autosave_interp = &in;
#endif
            InResult r = read_line(in, prompted, prompt,
                                   /*want_line=*/true, prompt_on_screen);
#ifdef SPATTERLIGHT
            g_autosave_interp = nullptr;
#endif
            if (r.kind == InEnd::State || r.kind == InEnd::Event)
                { /* re-dispatch on the new engine state */ }
            else {
                std::string cmd = trim(r.text);
                if (cmd.empty())
                    continue;
                if (is_restore_command(cmd, host_owned)) {
                    if (!g_prompt_first)
                        echo_metaverb(cmd);
                    if (do_restore_ui(restore_data))
                        return SessionEnd::Restore;
                } else if (!handle_transcript_command(cmd) &&
                           !handle_help_command(cmd) &&
                           /* Not under a `get input`: unlike the metaverbs
                            * above, STATUS is a plain word a game may well be
                            * waiting to hear as an answer. */
                           !(!host_owned && handle_status_command(cmd)) &&
                           !handle_verbs_command(in, cmd)) {
                    /* Arm the echo swallow unconditionally: whether the game
                     * side echoes cannot be predicted from game.echocommand,
                     * because a game may carry its own copy of Core's
                     * HandleCommand that echoes regardless (First Times, ASL
                     * 520, sets no echocommand yet still prints "> cmd").
                     * Detecting the echo as it arrives covers both -- an
                     * unechoed turn just disarms on its first chunk. */
                    if (g_prompt_first && !host_owned) {
                        /* A line typed with no prompt drawn (hidden box) has
                         * nothing on screen to show for it, and the swallow
                         * below drops the game's echo too -- so record it
                         * here.  A CLICKED link needs no such record: the
                         * link itself is the visible act, and echoing it
                         * would put "> Page2" through a gamebook that is
                         * meant to read as prose. */
                        if (!bar_shown && r.kind == InEnd::Line)
                            echo_metaverb(cmd);
                        g_swallow = 2;
                        g_swallow_cmd = cmd;
                    }
                    in.send_command(cmd);
                    g_swallow = 0;
                    /* Drain first: the hint must land after the game's help
                     * text, not in front of output still queued behind it. */
                    in.drain_on_ready();
                    /* Only for a real parser turn: with the command box hidden
                     * (gamebook) HELP is not a command, and a `get input`
                     * override makes it an answer to the game's question. */
                    hint_system_commands(cmd, g_command_bar && !host_owned);
                }
            }
        }
        in.drain_on_ready();
        fire_js_events(in);
        flush_warnings(w, warnings_seen);
        update_banner(in);
        redraw_side_pane(in);
        redraw_grid_map();
        update_timer_request(in);
    }

    if (in.script_errors_fatal() && !fatal_reported) {
        fatal_reported = true;
        glk_set_style(style_Emphasized);
        glk_put_string((char *) "\n[The game has stopped because too many script"
                                " errors occurred.]\n");
        glk_set_style(style_Normal);
    }
    /* finish + reload in one turn: the browser reload wins over any menu. */
    if (restart_requested)
        return SessionEnd::Restart;
    SessionEnd ending = post_game_menu(in, restore_data);
    if (ending != SessionEnd::Resume)
        return ending;

    /* Undone back into play: repaint what the turn loop keeps current, since
     * the rolled-back turn may have moved the player or changed the lists. */
    fire_js_events(in);
    flush_warnings(w, warnings_seen);
    update_banner(in);
    redraw_side_pane(in);
    redraw_grid_map();
    update_timer_request(in);
    }
}

}  // namespace

/* ------------------------------------------------------------- entrypoints */

/* Sniff: a PK zip containing game.aslx, or XML whose root element is <asl>.
 * Cheap enough to run on every geas start (only the zip case reads the whole
 * file, and only after the 4-byte magic matched). */
extern "C" int aslx_is_quest5_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    unsigned char head[2048];
    size_t n = fread(head, 1, sizeof head, f);
    if (n >= 4 && memcmp(head, "PK\x03\x04", 4) == 0) {
        /* .quest package: claim it only if game.aslx is really inside.  A
         * failed ftell (-1) would otherwise become a (size_t)-1 vector. */
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (len <= 0) {
            fclose(f);
            return 0;
        }
        std::vector<uint8_t> buf((size_t) len);
        size_t got = fread(buf.data(), 1, (size_t) len, f);
        fclose(f);
        std::string out;
        return got == (size_t) len &&
               aslx::extract_game_aslx(buf.data(), got, out);
    }
    fclose(f);
    /* Raw .aslx: skip BOM/whitespace, require markup, then find the <asl
     * root tag (possibly after <?xml?> and comments). */
    size_t i = 0;
    if (n >= 3 && head[0] == 0xef && head[1] == 0xbb && head[2] == 0xbf)
        i = 3;
    while (i < n && isspace(head[i]))
        i++;
    if (i >= n || head[i] != '<')
        return 0;
    for (; i + 4 < n; i++)
        if (memcmp(head + i, "<asl", 4) == 0 &&
            (head[i + 4] == ' ' || head[i + 4] == '>' ||
             head[i + 4] == '\t' || head[i + 4] == '\r' || head[i + 4] == '\n'))
            return 1;
    return 0;
}

extern "C" void aslx_glk_main(const char *storyfile)
{
    gwin = glk_window_open(0, 0, 0, wintype_TextBuffer, 1);
    if (!gwin)
        return;
    glk_set_window(gwin);

    glk_stylehint_set(wintype_TextGrid, style_User1, stylehint_ReverseColor, 1);
    gbanner = glk_window_open(gwin, winmethod_Above | winmethod_Fixed, 1,
                              wintype_TextGrid, 0);

    /* Prompt-first needs the library to suppress line-input echo (the host
     * echoes instead).  Without it (CheapGlk) keep the reference player's
     * game-side-echo presentation -- the smoke harness's transcripts depend
     * on it.  ASLXGLK_PROMPT_FIRST=1 forces the new mode there for testing:
     * under CheapGlk the typed line never reaches the output stream, so
     * manual echo is right for a piped harness too. */
    g_echo_control = glk_gestalt(gestalt_LineInputEcho, 0) != 0;
    g_manual_echo = g_echo_control ||
                    getenv("ASLXGLK_PROMPT_FIRST") != nullptr;
    g_prompt_first = g_manual_echo;
    g_hyperlinks = glk_gestalt(gestalt_Hyperlinks, 0) &&
                   glk_gestalt(gestalt_HyperlinkInput, wintype_TextBuffer);

    /* Probe for side-pane support (CheapGlk can't split), then close it:
     * redraw_side_pane manages the pane dynamically, and the engine only
     * computes the pane lists when the probe succeeded. */
    g_use_objpane = true;
    ensure_pane_open();
    g_use_objpane = (gobjwin != nullptr);
    close_side_pane();

    g_storyfile = storyfile;
    init_resources(storyfile);
    std::string restore_data;
#ifdef SPATTERLIGHT
    /* A complete autosave means this launch is an autorestore; run_session
     * applies it on its way into the first session. */
    g_autorestore_pending = geas_autosave_exists();
#endif
    while (run_session(storyfile, restore_data) != SessionEnd::Quit) {
        /* RESTART/RESTORE: fresh world, fresh screen (QuestViva reloads from
         * disk; a restore then applies the snapshot in run_session), and no
         * sound survives the old session (a looping one would keep playing
         * over the new game). */
        stop_sound_ui();
        glk_window_clear(gwin);
        g_links.clear();
        g_links_spent = 0;
        /* The cleared window holds nothing to retract. */
        g_out_log.clear();
        g_sections.clear();
        g_bold = g_italic = g_under = 0;
        apply_style();
        /* No pane content survives the session either. */
        close_side_pane();
        grid_map_reset();
        panel_close();          /* the frame picture does not outlive a session */
        g_pane_inv.clear();
        g_pane_places.clear();
        g_pane_exits.clear();
        g_pane_links.clear();
        g_pane_expanded.clear();
        g_pane_dirty = false;
        g_status_line.clear();
        g_location_line.clear();
        /* The reloaded game declares its own command box and click handler at
         * InitInterface. */
        g_command_bar = true;
        g_click_hook = false;
        g_js_events.clear();
    }
}
