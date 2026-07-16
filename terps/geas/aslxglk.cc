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
// form of ShowMenu is the one genuinely synchronous prompt; it gets a nested
// input loop via Interp::menu_provider.
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
#include <cstring>
#include <map>
#include <string>
#include <vector>

extern "C" {
#include "glk.h"
}

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

winid_t gwin = nullptr;      /* main text buffer */
winid_t gbanner = nullptr;   /* one-line status grid: room name */
strid_t gtranscript = nullptr;
bool g_manual_echo = false;
bool g_hyperlinks = false;
std::string g_room_name;

/* Click actions, one per Glk hyperlink value (index + 1).  A cmdlink carries
 * either a literal command (data-command / data-elementid) or an
 * onclick="ASLEvent('Func','param')" pair (Core's ShowMenu options). */
struct LinkAction {
    std::string command;        /* send_command() this */
    std::string event_func;     /* or call_function(event_func, {event_param}) */
    std::string event_param;
};
std::vector<LinkAction> g_links;

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

/* Pane redraw (defined after the HTML renderer): repaints the side pane from
 * the stored lists when they changed; safe to call anywhere no line input is
 * pending. */
void redraw_side_pane(Interp &in);
void fill_pane_divider();

/* ---------------------------------------------------------------- output -- */

void put_uni_char(glui32 c)
{
    glk_put_char_uni(c);
}

void put_uni_string(const std::string &utf8)
{
    for (size_t i = 0; i < utf8.size();) {
        unsigned char c = (unsigned char) utf8[i];
        glui32 cp = c;
        int len = 1;
        if (c >= 0xf0) len = 4;
        else if (c >= 0xe0) len = 3;
        else if (c >= 0xc0) len = 2;
        if (len > 1 && i + len <= utf8.size()) {
            cp = c & (0x7f >> len);
            for (int k = 1; k < len; k++)
                cp = (cp << 6) | ((unsigned char) utf8[i + k] & 0x3f);
        }
        put_uni_char(cp);
        i += len;
    }
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

std::string lower(std::string s)
{
    for (char &c : s) c = (char) tolower((unsigned char) c);
    return s;
}

/* Value of attr="..." (or attr='...') inside a raw tag body, or "". */
std::string tag_attr(const std::string &tag, const std::string &attr)
{
    std::string t = lower(tag);
    size_t pos = 0;
    while ((pos = t.find(attr, pos)) != std::string::npos) {
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

/* Extract the click action of an <a> tag, if we understand it. */
LinkAction link_action(const std::string &tag)
{
    LinkAction a;
    a.command = tag_attr(tag, "data-command");
    if (a.command.empty()) {
        /* Core's ShowMenu options: onclick="ASLEvent('Func','param')". */
        std::string oc = tag_attr(tag, "onclick");
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
    struct { const char *name; glui32 cp; } ents[] = {
        {"amp", '&'}, {"lt", '<'}, {"gt", '>'}, {"quot", '"'},
        {"apos", '\''}, {"nbsp", ' '}, {"mdash", 0x2014}, {"ndash", 0x2013},
        {"hellip", 0x2026}, {"copy", 0xa9}, {"eacute", 0xe9},
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

/* Render one HTML chunk (a JS.addText payload) into the main window. */
void render_html(const std::string &html)
{
    std::vector<StylePush> spans;   /* open <span>s */
    std::vector<StylePush> anchors; /* open <a>s */
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
                glk_put_char('\n');
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
                    spans.push_back(p);
                    push_style(p);
                } else if (!spans.empty()) {
                    pop_style(spans.back());
                    spans.pop_back();
                }
            } else if (name == "a") {
                if (!closing) {
                    LinkAction act = link_action(tag);
                    StylePush p;
                    bool live = !act.command.empty() || !act.event_func.empty();
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
            }
            /* every other tag (font, div, table...) is dropped: §4 says
             * best-effort subset, not a browser.  Sounds are still
             * presentation-milestone work. */
        } else if (ch == '&') {
            put_uni_char(decode_entity(html, i));
        } else if (ch == '\r') {
            i++;
        } else {
            /* UTF-8 sequence or plain char (raw newlines kept, as in the
             * oracle transcripts). */
            unsigned char c = (unsigned char) ch;
            glui32 cp = c;
            int len = 1;
            if (c >= 0xf0) len = 4;
            else if (c >= 0xe0) len = 3;
            else if (c >= 0xc0) len = 2;
            if (len > 1 && i + len <= html.size()) {
                cp = c & (0x7f >> len);
                for (int k = 1; k < len; k++)
                    cp = (cp << 6) | ((unsigned char) html[i + k] & 0x3f);
            }
            put_uni_char(cp);
            i += len;
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

/* ---------------------------------------------------------------- banner -- */

/* Write a UTF-8 string to a byte stream per-codepoint so accents survive
 * (the byte-stream API is Latin-1). */
void put_stream_utf8(strid_t s, const std::string &str)
{
    for (size_t i = 0; i < str.size();) {
        unsigned char c = (unsigned char) str[i];
        glui32 cp = c;
        int len = 1;
        if (c >= 0xf0) len = 4;
        else if (c >= 0xe0) len = 3;
        else if (c >= 0xc0) len = 2;
        if (len > 1 && i + len <= str.size()) {
            cp = c & (0x7f >> len);
            for (int k = 1; k < len; k++)
                cp = (cp << 6) | ((unsigned char) str[i + k] & 0x3f);
        }
        glk_put_char_stream_uni(s, cp);
        i += len;
    }
}

size_t utf8_cp_len(const std::string &s)
{
    size_t n = 0;
    for (size_t i = 0; i < s.size(); i++)
        if (((unsigned char) s[i] & 0xc0) != 0x80)
            n++;
    return n;
}

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
            if (r->elem_type == "game") { game = r; break; }
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
        if (!room.empty())
            room[0] = (char) toupper((unsigned char) room[0]);
    }
    g_room_name = plain_text(room);

    if (!gbanner)
        return;
    glk_window_clear(gbanner);
    strid_t s = glk_window_get_stream(gbanner);
    glk_set_style_stream(s, style_User1);
    glui32 width;
    glk_window_get_size(gbanner, &width, nullptr);
    for (glui32 x = 0; x < width; x++)
        glk_put_char_stream(s, ' ');
    glk_window_move_cursor(gbanner, 1, 0);
    put_stream_utf8(s, g_room_name);
    /* Status attributes (JS.updateStatus: "Score: 3 | Health: 90%"),
     * right-aligned after the room name when they fit. */
    if (!g_status_line.empty()) {
        size_t len = utf8_cp_len(g_status_line);
        if (utf8_cp_len(g_room_name) + len + 4 < width) {
            glk_window_move_cursor(gbanner, width - (glui32) len - 1, 0);
            put_stream_utf8(s, g_status_line);
        }
    }
}

/* ------------------------------------------------------------- side pane -- */

/* Open the right-hand pane (and its divider) if the host supports splits.
 * Mirrors the classic runner's objwin arrangement: 20% of the main text's
 * width, with a 2px graphics divider drawn in the text colour. */
void ensure_pane_open()
{
    if (gobjwin || !g_use_objpane)
        return;
    gobjwin = glk_window_open(gwin, winmethod_Right | winmethod_Proportional,
                              20, wintype_TextBuffer, 0);
    if (gobjwin && glk_gestalt(gestalt_Graphics, 0))
        gdivider = glk_window_open(gobjwin, winmethod_Left | winmethod_Fixed,
                                   2, wintype_Graphics, 0);
    fill_pane_divider();
}

/* Close the pane so the main window reclaims the width.  The divider is a
 * child split of the pane, so close it first. */
void close_side_pane()
{
    if (gdivider) { glk_window_close(gdivider, nullptr); gdivider = nullptr; }
    if (gobjwin)  { glk_window_close(gobjwin, nullptr);  gobjwin = nullptr; }
}

/* Paint the divider in the main text colour.  Graphics windows are blanked
 * on resize, so Arrange/Redraw events call this again. */
void fill_pane_divider()
{
    if (!gdivider)
        return;
    glui32 color;
    if (!glk_style_measure(gwin, style_Normal, stylehint_TextColor, &color))
        color = 0;
    glui32 w = 0, h = 0;
    glk_window_get_size(gdivider, &w, &h);
    glk_window_fill_rect(gdivider, color, 0, 0, w, h);
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
        if (r->elem_type == "game") { game = r; break; }
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
        glk_set_style_stream(s, style_Subheader);
        put_stream_utf8(s, header);
        glk_put_char_stream(s, '\n');
        glk_set_style_stream(s, style_Normal);
        for (const ListData *d : items) {
            std::string label = plain_text(d->text);
            if (label.empty())
                label = d->display_alias;
            if (g_hyperlinks) {
                LinkAction act;
                act.command = exit_links
                    ? d->display_alias
                    : (d->verbs.empty() ? std::string("look at")
                                        : plain_text(d->verbs[0])) +
                          " " + d->display_alias;
                g_pane_links.push_back(act);
                glk_set_hyperlink_stream(
                    s, kPaneLinkBase + (glui32) g_pane_links.size() - 1);
            }
            put_stream_utf8(s, label);
            if (g_hyperlinks)
                glk_set_hyperlink_stream(s, 0);
            glk_put_char_stream(s, '\n');
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
    for (glui32 i = 0; i < n; i++) {
        glui32 cp = buf[i];
        if (cp < 0x80) out += (char) cp;
        else if (cp < 0x800) {
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
    return out;
}

void echo_input(const std::string &line)
{
    glk_set_style(style_Input);
    put_uni_string(line);
    glk_set_style(style_Normal);
    glk_put_char('\n');
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

/* True when the engine is waiting for something other than a command line --
 * the prompt loop must re-dispatch. */
bool engine_state_pending(Interp &in)
{
    return in.pending_menu() || in.pending_question() || in.pending_wait() ||
           in.world().finished;
}

/* Request one line of input.  `echo` = the HOST echoes the accepted line
 * (host-owned prompts: get input, menus, yes/no, the post-game menu).  It is
 * false for parser-bound commands, which the GAME side echoes itself --
 * Core's game.echocommand prints "> cmd" into the transcript (the engine
 * does it for pre-v520 games), exactly like the reference player, whose
 * input box leaves no other record.  Library echo is off either way (when
 * controllable), so the typed line is atomically replaced by whichever echo
 * applies.  Timer events tick the engine; if a tick opens a prompt or ends
 * the game the request is cancelled and InEnd::State returned. */
InResult read_line(Interp &in, bool echo)
{
    static glui32 buf[256];
    if (g_manual_echo)
        glk_set_echo_line_event(gwin, 0);
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
            if (ev.win == gwin && ev.val1 >= 1 && ev.val1 <= g_links.size()) {
                event_t ce;
                glk_cancel_line_event(gwin, &ce);
                const LinkAction &act = g_links[ev.val1 - 1];
                if (!act.command.empty()) {
                    if (g_manual_echo && echo)
                        echo_input(act.command);
                    return {InEnd::Command, act.command};
                }
                run_asl_event(in, act);
                return {InEnd::Event, ""};
            }
            if (ev.win == gobjwin && ev.val1 >= kPaneLinkBase &&
                ev.val1 - kPaneLinkBase < g_pane_links.size()) {
                /* A side-pane item: run its command as if typed. */
                event_t ce;
                glk_cancel_line_event(gwin, &ce);
                const LinkAction &act = g_pane_links[ev.val1 - kPaneLinkBase];
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
            /* Cancel first: Glk forbids output with a live line request.
             * With echo off the cancel is invisible. */
            event_t ce;
            glk_cancel_line_event(gwin, &ce);
            in.tick(1);
            in.drain_on_ready();
            update_banner(in);
            redraw_side_pane(in);
            if (engine_state_pending(in))
                return {InEnd::State, ""};
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
            break;
        }
    }
}

/* One keypress (a pending `wait`).  Timer events keep ticking. */
void read_keypress(Interp &in)
{
    glk_request_char_event(gwin);
    for (;;) {
        event_t ev;
        glk_select(&ev);
        if (ev.type == evtype_CharInput && ev.win == gwin)
            return;
        if (ev.type == evtype_Timer) {
            in.tick(1);
            update_banner(in);
            redraw_side_pane(in);
            if (in.world().finished || !in.pending_wait()) {
                glk_cancel_char_event(gwin);
                return;
            }
        }
        if (ev.type == evtype_Arrange || ev.type == evtype_Redraw) {
            update_banner(in);
            fill_pane_divider();
        }
    }
}

std::string trim(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
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
    if (!m.caption.empty()) {
        render_html(m.caption);
        glk_put_char('\n');
    }
    for (size_t i = 0; i < m.options.size(); i++) {
        char num[16];
        snprintf(num, sizeof num, "%zu: ", i + 1);
        glk_put_string((char *) num);
        render_html(m.options[i].second);
        glk_put_char('\n');
    }
    for (;;) {
        char pr[64];
        snprintf(pr, sizeof pr, "\nChoose [1-%zu]%s> ", m.options.size(),
                 m.allow_cancel ? " (or nothing to cancel)" : "");
        glk_put_string(pr);
        InResult r = read_line(in, true);
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

/* ---------------------------------------------------------- save/restore -- */

std::string core_dir_path();   /* defined in the core section below */

const char *g_storyfile = nullptr;

/* Write the engine snapshot to a player-chosen file. Runs mid-turn, from the
 * request (RequestSave) hook -- Core's `save` command -- so there is never a
 * pending line request here. */
void do_save_ui(Interp &in)
{
    glui32 usage = fileusage_SavedGame | fileusage_BinaryMode;
    frefid_t fref = glk_fileref_create_by_prompt(usage, filemode_Write, 0);
    if (!fref) { glk_put_string((char *) "Save cancelled.\n"); return; }
    strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
    glk_fileref_destroy(fref);
    if (!str) {
        glk_put_string((char *) "Could not open the save file.\n");
        return;
    }
    std::string data = in.save_game(g_storyfile ? g_storyfile : "");
    glk_put_buffer_stream(str, (char *) data.data(), (glui32) data.size());
    glk_stream_close(str, nullptr);
    glk_put_string((char *) "Game saved.\n");
}

/* Prompt for a save file and read it, validating it against a scratch reload
 * of the game (so a wrong-game or corrupt save never tears down the running
 * session). Returns true with `data` filled when the caller should reboot
 * the session from it. */
bool do_restore_ui(std::string &data)
{
    glui32 usage = fileusage_SavedGame | fileusage_BinaryMode;
    frefid_t fref = glk_fileref_create_by_prompt(usage, filemode_Read, 0);
    if (!fref) { glk_put_string((char *) "Restore cancelled.\n"); return false; }
    strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
    glk_fileref_destroy(fref);
    if (!str) {
        glk_put_string((char *) "Could not open the save file.\n");
        return false;
    }
    data.clear();
    char buf[4096];
    glui32 n;
    while ((n = glk_get_buffer_stream(str, buf, sizeof buf)) > 0)
        data.append(buf, n);
    glk_stream_close(str, nullptr);

    if (!Interp::is_save_data(data.data(), data.size())) {
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
bool is_restore_command(const std::string &raw)
{
    std::string c = lower(trim(raw));
    return c == "restore" || c == "restore game" || c == "load game";
}

/* ------------------------------------------------------------- metaverbs -- */

/* Transcript recording, same commands as the classic runner. */
bool handle_transcript_command(const std::string &raw)
{
    std::string c = lower(trim(raw));
    bool on  = (c == "transcript" || c == "transcript on" ||
                c == "script" || c == "script on");
    bool off = (c == "transcript off" || c == "script off" ||
                c == "notranscript" || c == "noscript" || c == "unscript");
    if (!on && !off)
        return false;
    echo_metaverb(raw);
    if (on) {
        if (gtranscript) {
            glk_put_string((char *) "A transcript is already being recorded.\n");
            return true;
        }
        frefid_t fref = glk_fileref_create_by_prompt(
            fileusage_Transcript | fileusage_TextMode, filemode_Write, 0);
        if (!fref) {
            glk_put_string((char *) "Transcript cancelled.\n");
            return true;
        }
        gtranscript = glk_stream_open_file(fref, filemode_Write, 0);
        glk_fileref_destroy(fref);
        if (!gtranscript) {
            glk_put_string((char *) "Could not open the transcript file.\n");
            return true;
        }
        glk_window_set_echo_stream(gwin, gtranscript);
        glk_put_string((char *) "Transcript on: game text is now being saved to a file.\n");
    } else {
        if (!gtranscript) {
            glk_put_string((char *) "No transcript is being recorded.\n");
            return true;
        }
        glk_put_string((char *) "Transcript off.\n");
        glk_window_set_echo_stream(gwin, nullptr);
        glk_stream_close(gtranscript, nullptr);
        gtranscript = nullptr;
    }
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
/* Like the classic runner's show_image (geasglk.cc): Quest image files are
 * external and arbitrarily named, so register each with the backend under a
 * private resource number and let glk_image_draw* short-circuit its
 * PIC<n>/blorb lookup via win_findimage. */
extern "C" int  win_findimage(int resno);
extern "C" void win_loadimage(int resno, const char *filename, int offset, int reslen);

/* filename (case-folded) -> registered resource number; 0 = known-failed. */
std::map<std::string, glui32> g_image_ids;
int g_image_next_id = 1;

/* Stage bytes in a temporary file for the backend to load. The file is only
 * needed until the load round-trips (the caller's glk_image_get_info), after
 * which it is unlinked. */
bool stage_temp_file(const std::string &bytes, std::string &path)
{
    const char *tmpdir = getenv("TMPDIR");
    std::string t = (tmpdir && *tmpdir) ? tmpdir : "/tmp";
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

/* Resolve + register one image resource. Returns its resource number, or 0.
 * A stored package entry is read by the app straight from the .quest at its
 * byte offset; a deflated entry (the common case -- Quest's packager
 * deflates everything) or an adjacent file goes through resource_bytes. */
glui32 load_image_resource(const std::string &name)
{
    glui32 id = 0;
    std::string temp_path;

    const ZipEntryInfo *e =
        g_is_package && name.find("..") == std::string::npos
            ? zip_find_entry(g_package_entries, name) : nullptr;
    if (e && e->method == 0 && g_storyfile) {
        id = (glui32) (g_image_next_id++);
        win_loadimage((int) id, g_storyfile, (int) e->offset,
                      (int) e->comp_size);
    } else {
        std::string bytes;
        if (!resource_bytes(name, bytes) || bytes.empty())
            return 0;
        if (!stage_temp_file(bytes, temp_path))
            return 0;
        id = (glui32) (g_image_next_id++);
        win_loadimage((int) id, temp_path.c_str(), 0, (int) bytes.size());
    }

    /* glk_image_get_info round-trips to the app, which both confirms the
     * image really decoded and means the temp file has been consumed. */
    glui32 w, h;
    if (!glk_image_get_info(id, &w, &h))
        id = 0;
    if (!temp_path.empty())
        unlink(temp_path.c_str());
    return id;
}
#endif /* SPATTERLIGHT */

#ifdef SPATTERLIGHT
/* Sounds, the same pattern as images: register each game sound file with the
 * backend under a private resource number so glk_schannel_play* finds it via
 * win_findsound. */
extern "C" int  win_findsound(int resno);
extern "C" void win_loadsound(int resno, char *filename, int offset, int reslen);

/* filename (case-folded) -> registered resource number; 0 = known-failed. */
std::map<std::string, glui32> g_sound_ids;
int g_sound_next_id = 1;

/* Resolve + register one sound resource, mirroring load_image_resource. The
 * app loads the data as soon as it receives LOADSOUND, and win_findsound
 * round-trips (the glk_image_get_info analog): it both confirms the resource
 * really loaded and means a temp file has been consumed. */
glui32 load_sound_resource(const std::string &name)
{
    glui32 id = 0;
    std::string temp_path;

    const ZipEntryInfo *e =
        g_is_package && name.find("..") == std::string::npos
            ? zip_find_entry(g_package_entries, name) : nullptr;
    if (e && e->method == 0 && g_storyfile) {
        id = (glui32) (g_sound_next_id++);
        win_loadsound((int) id, (char *) g_storyfile, (int) e->offset,
                      (int) e->comp_size);
    } else {
        std::string bytes;
        if (!resource_bytes(name, bytes) || bytes.empty())
            return 0;
        if (!stage_temp_file(bytes, temp_path))
            return 0;
        id = (glui32) (g_sound_next_id++);
        win_loadsound((int) id, (char *) temp_path.c_str(), 0,
                      (int) bytes.size());
    }

    if (!win_findsound((int) id))
        id = 0;
    if (!temp_path.empty())
        unlink(temp_path.c_str());
    return id;
}
#endif /* SPATTERLIGHT */

/* One sound channel, like the reference player's single <audio> element: a
 * new `play sound` replaces the current one, `stop sound` silences it. */
schanid_t g_schannel = nullptr;

void stop_sound_ui()
{
    if (g_schannel)
        glk_schannel_stop(g_schannel);
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
        if (ev.type == evtype_Arrange || ev.type == evtype_Redraw)
            update_banner(in);
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

    glui32 id;
    auto it = g_sound_ids.find(lower(name));
    if (it != g_sound_ids.end()) {
        id = it->second;
    } else {
        id = load_sound_resource(name);
        g_sound_ids[lower(name)] = id;   /* 0 = known-unresolvable */
    }
    if (!id)
        return;

    if (!g_schannel)
        g_schannel = glk_schannel_create(0);
    if (!g_schannel)
        return;

    bool block = sync && !loop && gli_sa_delays;
    if (!glk_schannel_play_ext(g_schannel, id, loop ? 0xffffffff : 1,
                               block ? 1 : 0))
        return;
    if (block)
        wait_for_sound(in, id);
#else
    /* No way to register sounds by name outside Spatterlight's Glk. */
    (void) in; (void) name; (void) sync; (void) loop;
#endif
}

bool draw_image_inline(const std::string &name)
{
#ifdef SPATTERLIGHT
    if (!glk_gestalt(gestalt_Graphics, 0) ||
        !glk_gestalt(gestalt_DrawImage, wintype_TextBuffer))
        return false;

    glui32 id;
    auto it = g_image_ids.find(lower(name));
    if (it != g_image_ids.end()) {
        id = it->second;
    } else {
        id = load_image_resource(name);
        g_image_ids[lower(name)] = id;   /* 0 = known-unresolvable */
    }
    if (!id)
        return false;

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

/* Frame-picture redirect (JS.setPanelContents): Quest's picture frame is a
 * persistent panel above the transcript -- Core sets it from the room's
 * `picture` attribute on every room change (OnEnterRoom -> SetFramePicture)
 * and re-sets it at boot/restore (InitInterface). With no panel, draw the
 * picture inline when it CHANGES, the way Scarier folds ADRIFT's graphics
 * into the buffer: room art appears on entry, classic illustrated-IF style,
 * and consecutive re-sets of the same file stay quiet. */
std::string g_panel_last;   /* case-folded filename currently "in the frame" */

void panel_contents(const std::string &html)
{
    std::string src = decode_entities(tag_attr(html, "src"));
    if (src.empty()) {          /* ClearFramePicture (or non-img contents) */
        g_panel_last.clear();
        return;
    }
    if (lower(src) == g_panel_last)
        return;
    if (draw_image_inline(src)) {
        g_panel_last = lower(src);
        glk_put_char('\n');
    }
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

/* How a session ended, and what the next one boots from. */
enum class SessionEnd { Quit, Restart, Restore };

/* After the game ends: restart, restore or quit.  (In-game UNDO is Core's own
 * command; there is no post-game undo, as in QuestViva.) */
SessionEnd post_game_menu(Interp &in, std::string &restore_data)
{
    glk_put_string((char *) "\nThe story has ended.  You can RESTART, RESTORE"
                            " a saved game, or QUIT.\n");
    for (;;) {
        glk_put_string((char *) "> ");
        InResult r = read_line(in, true);
        if (r.kind != InEnd::Line)
            continue;
        std::string w = lower(trim(r.text));
        if (w == "restart" || w == "r")
            return SessionEnd::Restart;
        if (w == "restore" || w == "load") {
            if (do_restore_ui(restore_data))
                return SessionEnd::Restore;
            continue;
        }
        if (w == "quit" || w == "q") {
            glk_put_string((char *) "\nThanks for playing. Goodbye!\n");
            return SessionEnd::Quit;
        }
        glk_put_string((char *) "Please type RESTART, RESTORE or QUIT.\n");
    }
}

/* Arm/disarm the 1-second heartbeat to match whether any timer is enabled.
 * Gated on the real-time preference like the classic runner, so the
 * deterministic import tests see no wall-clock events. */
void update_timer_request(Interp &in)
{
    static bool armed = false;
    bool want = gli_sa_delays && in.next_timer_seconds() > 0;
    if (want != armed) {
        glk_request_timer_events(want ? 1000 : 0);
        armed = want;
    }
}

/* Run one full game session, booting fresh or from a validated snapshot
 * (which do_restore_ui already probe-applied). `restore_data` is consumed on
 * entry and re-filled when the session ends with a RESTORE. */
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
    in.request_save = [&in] { do_save_ui(in); };
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

#ifdef GLK_MODULE_GARGLKTEXT
    if (!w.game_name.empty())
        garglk_set_story_title(w.game_name.c_str());
#endif

    size_t warnings_seen = 0;

    /* Saved-game boot, mirroring WorldModel.BeginInternalAsync with
     * _loadedFromSaved: timers are not re-armed, InitInterface re-runs,
     * StartGame does not, and the reference's no-transcript fallback message
     * is printed. */
    bool restored = false;
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
    if (restored) {
        glk_put_string((char *) "Loaded saved game\n");
        update_banner(in);
    }
    in.drain_on_ready();
    flush_warnings(w, warnings_seen);
    update_banner(in);
    redraw_side_pane(in);
    update_timer_request(in);

    while (!w.finished) {
        if (const MenuData *m = in.pending_menu()) {
            std::string key;
            bool picked = run_menu_ui(in, *m, key);
            in.set_menu_response(picked ? &key : nullptr);
        } else if (const std::string *q = in.pending_question()) {
            /* The engine does not print the caption (ShowQuestion is host
             * UI); render it with a yes/no prompt. */
            render_html(*q);
            for (;;) {
                glk_put_string((char *) " (yes/no) > ");
                InResult r = read_line(in, true);
                if (r.kind == InEnd::State) break;
                if (r.kind != InEnd::Line) continue;
                std::string a = lower(trim(r.text));
                if (a == "yes" || a == "y") { in.set_question_response(true); break; }
                if (a == "no" || a == "n")  { in.set_question_response(false); break; }
                glk_put_string((char *) "Please answer YES or NO.");
            }
        } else if (in.pending_wait()) {
            read_keypress(in);
            if (in.pending_wait())
                in.finish_wait();
        } else {
            /* Parser-bound lines get no printed prompt and no host echo: the
             * game side echoes them itself ("> cmd" -- Core's echocommand,
             * or the engine for pre-v520), so printing our own would double
             * every command.  A pending `get input` is the exception: the
             * engine consumes the line without any game-side echo, so it is
             * host-prompted and host-echoed. */
            bool host_owned = in.command_override();
            if (host_owned)
                glk_put_string((char *) "\n> ");
            InResult r = read_line(in, host_owned);
            if (r.kind == InEnd::State || r.kind == InEnd::Event)
                { /* re-dispatch on the new engine state */ }
            else {
                std::string cmd = trim(r.text);
                if (cmd.empty())
                    continue;
                if (is_restore_command(cmd)) {
                    echo_metaverb(cmd);
                    if (do_restore_ui(restore_data))
                        return SessionEnd::Restore;
                } else if (!handle_transcript_command(cmd)) {
                    in.send_command(cmd);
                }
            }
        }
        in.drain_on_ready();
        flush_warnings(w, warnings_seen);
        update_banner(in);
        redraw_side_pane(in);
        update_timer_request(in);
    }

    if (in.script_errors_fatal()) {
        glk_set_style(style_Emphasized);
        glk_put_string((char *) "\n[The game has stopped because too many script"
                                " errors occurred.]\n");
        glk_set_style(style_Normal);
    }
    return post_game_menu(in, restore_data);
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
        /* .quest package: claim it only if game.aslx is really inside. */
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
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

    g_manual_echo = glk_gestalt(gestalt_LineInputEcho, 0) != 0;
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
    while (run_session(storyfile, restore_data) != SessionEnd::Quit) {
        /* RESTART/RESTORE: fresh world, fresh screen (QuestViva reloads from
         * disk; a restore then applies the snapshot in run_session), and no
         * sound survives the old session (a looping one would keep playing
         * over the new game). */
        stop_sound_ui();
        glk_window_clear(gwin);
        g_links.clear();
        g_bold = g_italic = g_under = 0;
        apply_style();
        /* No pane content survives the session either. */
        close_side_pane();
        g_pane_inv.clear();
        g_pane_places.clear();
        g_pane_exits.clear();
        g_pane_links.clear();
        g_pane_dirty = false;
        g_status_line.clear();
        g_location_line.clear();
    }
}
