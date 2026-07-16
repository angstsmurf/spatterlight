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
            }
            /* every other tag (img, font, div, table...) is dropped: §4 says
             * best-effort subset, not a browser.  Images/sounds are the
             * presentation milestone. */
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

/* Current room = game.pov.parent, its alias if it has one.  Purely cosmetic;
 * any resolution failure just blanks the banner. */
void update_banner(Interp &in)
{
    World &w = in.world();
    std::string room;
    Element *game = nullptr;
    for (Element *r : w.roots)
        if (r->elem_type == "game") { game = r; break; }
    if (game) {
        const Value *pov = in.resolve_field(game, "pov");
        Element *pl = pov && pov->type == Value::Type::ObjectRef
                          ? w.find(pov->str) : nullptr;
        const Value *par = pl ? in.resolve_field(pl, "parent") : nullptr;
        Element *rm = par && par->type == Value::Type::ObjectRef
                          ? w.find(par->str) : nullptr;
        if (rm) {
            const Value *alias = in.resolve_field(rm, "alias");
            room = alias && alias->type == Value::Type::String ? alias->str
                                                               : rm->name;
        }
    }
    if (!room.empty())
        room[0] = (char) toupper((unsigned char) room[0]);
    g_room_name = room;

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
    /* The byte-stream API is Latin-1; write per-char so accents survive. */
    for (size_t i = 0; i < g_room_name.size();) {
        unsigned char c = (unsigned char) g_room_name[i];
        glui32 cp = c;
        int len = 1;
        if (c >= 0xf0) len = 4;
        else if (c >= 0xe0) len = 3;
        else if (c >= 0xc0) len = 2;
        if (len > 1 && i + len <= g_room_name.size()) {
            cp = c & (0x7f >> len);
            for (int k = 1; k < len; k++)
                cp = (cp << 6) | ((unsigned char) g_room_name[i + k] & 0x3f);
        }
        glk_put_char_stream_uni(s, cp);
        i += len;
    }
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

/* Fire an ASLEvent hyperlink (Core menu options): call the named function
 * with its single parameter, as WebPlayer's ASLEvent bridge does. */
void run_asl_event(Interp &in, const LinkAction &act)
{
    Context ctx;
    try {
        if (in.world().find(act.event_func))
            in.call_function(act.event_func, {vstr(act.event_param)}, &ctx);
    } catch (const TurnSuspended &) {
        /* synchronous `play sound`: rest of the handler abandoned */
    } catch (const std::exception &) {
        /* reported at the script boundary already */
    }
    in.drain_on_ready();
}

/* True when the engine is waiting for something other than a command line --
 * the prompt loop must re-dispatch. */
bool engine_state_pending(Interp &in)
{
    return in.pending_menu() || in.pending_question() || in.pending_wait() ||
           in.world().finished;
}

/* Request one line of input (echo handled here).  `echo` is false for
 * pre-v520 games, whose engine echoes the command itself.  Timer events tick
 * the engine; if a tick opens a prompt or ends the game the request is
 * cancelled and InEnd::State returned. */
InResult read_line(Interp &in, bool echo)
{
    static glui32 buf[256];
    if (g_manual_echo)
        glk_set_echo_line_event(gwin, echo ? 0 : 1);
    glk_request_line_event_uni(gwin, buf, 255, 0);
    if (g_hyperlinks)
        glk_request_hyperlink_event(gwin);
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
            if (g_hyperlinks)
                glk_request_hyperlink_event(gwin);
            break;
        case evtype_Timer: {
            /* Cancel first: Glk forbids output with a live line request.
             * With echo off the cancel is invisible. */
            event_t ce;
            glk_cancel_line_event(gwin, &ce);
            in.tick(1);
            in.drain_on_ready();
            update_banner(in);
            if (engine_state_pending(in))
                return {InEnd::State, ""};
            glk_request_line_event_uni(gwin, buf, 255, ce.val1);
            if (g_hyperlinks)
                glk_request_hyperlink_event(gwin);
            break;
        }
        case evtype_Arrange:
        case evtype_Redraw:
            update_banner(in);
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
            if (in.world().finished || !in.pending_wait()) {
                glk_cancel_char_event(gwin);
                return;
            }
        }
        if (ev.type == evtype_Arrange || ev.type == evtype_Redraw)
            update_banner(in);
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

#ifdef GLK_MODULE_GARGLKTEXT
    if (!w.game_name.empty())
        garglk_set_story_title(w.game_name.c_str());
#endif

    size_t warnings_seen = 0;
    bool host_echo = w.asl_version >= 520;   /* pre-520: the engine echoes */

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
            glk_put_string((char *) "\n> ");
            InResult r = read_line(in, host_echo);
            if (r.kind == InEnd::State || r.kind == InEnd::Event)
                { /* re-dispatch on the new engine state */ }
            else {
                std::string cmd = trim(r.text);
                if (cmd.empty())
                    continue;
                if (is_restore_command(cmd)) {
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

    g_storyfile = storyfile;
    std::string restore_data;
    while (run_session(storyfile, restore_data) != SessionEnd::Quit) {
        /* RESTART/RESTORE: fresh world, fresh screen (QuestViva reloads from
         * disk; a restore then applies the snapshot in run_session). */
        glk_window_clear(gwin);
        g_links.clear();
        g_bold = g_italic = g_under = 0;
        apply_style();
    }
}
