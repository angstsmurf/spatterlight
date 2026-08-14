// aslx_linkcapture.cc -- replay an oracle .cmd walkthrough through the native
// aslx engine (exactly like aslx_replay.cc) but instead of a plain transcript,
// report, for every script command that clicks an on-screen link, the visible
// TEXT of that link. Command links are emitted by the Core text processor as
//   <a ... class="cmdlink ..." data-command="CMD">DISPLAY TEXT</a>
// (CoreOutput.aslx ProcessTextCommand_Command). We collect every such link as
// it scrolls past, and when the script sends CMD we match it to the most
// recent still-unspent link carrying that data-command and print its text.
#include "../../../quest5/aslx.cc"
#include "../../../quest5/aslx-runtime.cc"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace aslx;

// Minimal tag-stripper + entity decoder (subset, enough for Moquette's link
// text: it uses &mdash;/&eacute; etc). Mirrors aslx_replay.cc's strip_html.
static std::string strip_html(std::string s) {
    std::string out;
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '<') {
            size_t close = s.find('>', i);
            if (close == std::string::npos) { out += s.substr(i); break; }
            std::string tag = s.substr(i + 1, close - i - 1);
            if (tag == "br" || tag == "br/" || tag == "br /") out += '\n';
            i = close + 1;
        } else {
            out += s[i++];
        }
    }
    struct { const char *e; const char *r; } ents[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""},
        {"&#39;", "'"}, {"&apos;", "'"}, {"&nbsp;", " "},
        {"&mdash;", "\xe2\x80\x94"}, {"&ndash;", "\xe2\x80\x93"},
        {"&hellip;", "\xe2\x80\xa6"}, {"&middot;", "\xc2\xb7"},
        {"&lsquo;", "'"}, {"&rsquo;", "'"}, {"&ldquo;", "\""}, {"&rdquo;", "\""},
        {"&eacute;", "\xc3\xa9"},
    };
    for (auto &en : ents) {
        size_t pos = 0; std::string e = en.e;
        while ((pos = out.find(e, pos)) != std::string::npos) {
            out.replace(pos, e.size(), en.r); pos += 1;
        }
    }
    return out;
}

static std::string attr(const std::string &tag, const std::string &name) {
    size_t p = tag.find(name + "=\"");
    if (p == std::string::npos) return "";
    p += name.size() + 2;
    size_t q = tag.find('"', p);
    if (q == std::string::npos) return "";
    return tag.substr(p, q - p);
}

struct Link { std::string command; std::string text; bool spent = false; };
static std::vector<Link> g_links;

// Scan an emitted HTML chunk for <a ...>...</a> command links and append them.
static void collect_links(const std::string &html) {
    size_t i = 0;
    while ((i = html.find("<a", i)) != std::string::npos) {
        size_t close = html.find('>', i);
        if (close == std::string::npos) break;
        std::string opentag = html.substr(i, close - i + 1);
        size_t end = html.find("</a>", close);
        if (end == std::string::npos) break;
        std::string inner = html.substr(close + 1, end - close - 1);
        std::string cmd = attr(opentag, "data-command");
        // Only cmdlinks (game command links) carry data-command and matter here.
        if (!cmd.empty()) {
            Link l;
            l.command = cmd;
            l.text = strip_html(inner);
            g_links.push_back(l);
        }
        i = end + 4;
    }
}

static std::string nr_trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Find the most recent unspent link whose data-command equals cmd (case-insens).
static const Link *match_link(const std::string &cmd) {
    for (size_t k = g_links.size(); k-- > 0;) {
        if (g_links[k].spent) continue;
        if (rt_iequals(g_links[k].command.c_str(), cmd.c_str())) {
            g_links[k].spent = true;
            return &g_links[k];
        }
    }
    return nullptr;
}

int main(int argc, char **argv) {
    if (argc < 3) { std::cerr << "usage: linkcapture <game> <script>\n"; return 2; }
    World w;
    const char *core = std::getenv("ASLX_CORE");
    if (!load_file(argv[1], w, core ? core : "terps/geas/quest5/aslx-core")) {
        std::cerr << "[fatal] load failed\n"; return 3;
    }
    Interp in(w);
    // Capture raw HTML (with <a> tags intact) so we can harvest links.
    in.print = [&](const std::string &html) { collect_links(html); };
    in.play_sound = [](const std::string &, bool, bool) {};

    Context boot;
    in.begin_timers();
    try {
        if (w.find("InitInterface")) in.call_function("InitInterface", {}, &boot);
        if (w.find("StartGame")) in.call_function("StartGame", {}, &boot);
        in.update_lists();
    } catch (const TurnSuspended &) {}
    in.drain_on_ready();
    auto auto_advance = [&]() {
        int guard = 0;
        while (!w.finished && in.pending_wait() && guard++ < 100) in.finish_wait();
    };
    auto_advance();
    auto drain_timers = [&]() {
        int guard = 0;
        int pending_tick = in.next_timer_seconds();
        while (!w.finished && guard++ < 500 && in.has_enabled_timeout()) {
            int secs = pending_tick;
            in.tick(secs > 0 ? secs : 1);
            auto_advance();
            pending_tick = in.next_timer_seconds();
        }
    };
    drain_timers();

    std::ifstream script(argv[2]);
    std::vector<std::string> lines;
    std::string raw;
    while (std::getline(script, raw)) lines.push_back(raw);
    size_t li = 0;
    int steps = 0, clicks = 0;

    while (li < lines.size()) {
        if (w.finished) break;
        std::string cmd = nr_trim(lines[li++]);
        if (cmd.empty() || cmd[0] == '#') continue;
        // Skip harness directives that are not player input.
        if (cmd.compare(0, 6, "event:") == 0) {
            std::string rest = cmd.substr(6);
            size_t sc = rest.find(';');
            std::string name = sc == std::string::npos ? rest : rest.substr(0, sc);
            std::string param = sc == std::string::npos ? "" : rest.substr(sc + 1);
            in.send_event(name, param);
            in.drain_on_ready(); auto_advance(); drain_timers();
            continue;
        }
        if (cmd.compare(0, 6, "label:") == 0 || cmd.compare(0, 6, "delay:") == 0 ||
            cmd.compare(0, 8, "runtime:") == 0) continue;

        steps++;
        const Link *l = match_link(cmd);
        clicks++;
        if (l) {
            std::string disp = l->text;
            // Collapse internal whitespace/newlines for a one-line report.
            std::string flat;
            bool sp = false;
            for (char c : disp) {
                if (c == '\n' || c == '\t' || c == ' ') { sp = true; continue; }
                if (sp && !flat.empty()) flat += ' ';
                sp = false; flat += c;
            }
            std::cout << "[" << clicks << "] cmd=" << cmd
                      << "  |  link text: \"" << flat << "\"\n";
        } else {
            std::cout << "[" << clicks << "] cmd=" << cmd
                      << "  |  (typed command -- no matching on-screen link)\n";
        }
        in.send_command(cmd);
        in.drain_on_ready(); auto_advance(); drain_timers();
    }

    std::cerr << "[diag] steps=" << steps << " finished=" << w.finished
              << " errors=" << w.errors.size() << "\n";
    if (std::getenv("DUMP_LINKS")) {
        std::cerr << "--- all collected links (command => text) ---\n";
        for (auto &l : g_links) {
            std::string t;
            for (char c : l.text) t += (c == '\n' ? ' ' : c);
            std::cerr << "  " << l.command << " => \"" << t << "\"\n";
        }
    }
    return 0;
}
