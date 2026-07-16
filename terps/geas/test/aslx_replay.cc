// aslx_replay.cc -- replay an oracle .cmd walkthrough script through the
// NATIVE aslx engine and emit a qvh-normalised transcript, for diffing
// against the frozen goldens in quest5-oracle/golden/. This is the native
// half of the milestone-6 ground-truth harness; the .cmd/.out pairs were
// produced by the QuestViva oracle (quest5-oracle/README.md).
//
//   make aslx_replay
//   ASLX_CORE=../aslx-core ./aslx_replay "<game.quest>" "golden/<Game>.cmd" \
//       | diff "quest5-oracle/golden/<Game>.out" -
//
// Mirrors quest5-oracle/Program.cs: same step grammar (menu:/answer:/assert:/
// label:/delay:/runtime:/event:/# comments; bare lines resolve pending menus
// and questions), same auto-advance of pending waits, same deterministic
// DrainTimers (after each step, tick pending SetTimeout timers by exactly
// their trigger delta; only self-destructing "timeout*" timers are drained so
// recurring authored timers never loop), same HTML-strip + blank-line-collapse
// normalisation, same "> cmd" echo for v520+ and final "[state=...]" line.
// NOT yet mirrored: the Wedged-vs-Finished distinction.
//
// Baseline at first light (2026-07-16, input-model commit): all 17 goldens
// replay end-to-end with 0-4 script errors; matching golden lines per game
// range from ~30% to ~70%, no game reaches Finished yet. First divergences
// cluster on: verb commands ("wear X", "break X") answered "I don't
// understand your command.", object resolution ("open door 24" -> "I can't
// see that."), and missing/displaced room-description output on `go`.
#include "aslx.cc"
#include "aslx-runtime.cc"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace aslx;

static std::string strip_html(std::string s) {
    std::string out;
    // <br> variants -> newline; every other tag removed.
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '<') {
            size_t close = s.find('>', i);
            if (close == std::string::npos) { out += s.substr(i); break; }
            std::string tag = s.substr(i + 1, close - i - 1);
            // qvh's Strip regex ("<br\\s*/?>") is case-SENSITIVE: an
            // uppercase <BR> is dropped by the generic tag stripper, no
            // newline. Mirror that exactly.
            if (tag == "br" || tag == "br/" || tag == "br /") out += '\n';
            i = close + 1;
        } else {
            out += s[i++];
        }
    }
    // Basic HTML entity decode (WebUtility.HtmlDecode subset).
    struct { const char *e; const char *r; } ents[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""},
        {"&#39;", "'"}, {"&apos;", "'"}, {"&nbsp;", " "},
    };
    for (auto &en : ents) {
        size_t pos = 0;
        std::string e = en.e;
        while ((pos = out.find(e, pos)) != std::string::npos) {
            out.replace(pos, e.size(), en.r);
            pos += 1;
        }
    }
    return out;
}

static std::string normalise(const std::string &s) {
    std::istringstream is(s);
    std::ostringstream os;
    std::string line;
    int blanks = 0;
    bool first = true;
    while (std::getline(is, line)) {
        while (!line.empty() &&
               (line.back() == ' ' || line.back() == '\t' || line.back() == '\r'))
            line.pop_back();
        if (line.empty()) { blanks++; continue; }
        if (!first && blanks > 0) os << "\n";
        os << line << "\n";
        blanks = 0;
        first = false;
    }
    return os.str();
}

static std::string nr_trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

int main(int argc, char **argv) {
    if (argc < 3) { std::cerr << "usage: nreplay <game> <script>\n"; return 2; }
    World w;
    const char *core = std::getenv("ASLX_CORE");
    if (!load_file(argv[1], w, core ? core : "terps/geas/aslx-core")) {
        std::cerr << "[fatal] load failed\n";
        return 3;
    }
    Interp in(w);
    std::string transcript;
    auto emit = [&](const std::string &html) {
        std::string t = strip_html(html);
        if (t.empty()) return;
        transcript += t;
        if (t.back() != '\n') transcript += '\n';
    };
    in.print = emit;
    auto line_out = [&](const std::string &s) { transcript += s + "\n"; };

    Context boot;
    in.begin_timers();  // BeginInternalAsync arms enabled timers before InitInterface
    try {
        if (w.find("InitInterface")) in.call_function("InitInterface", {}, &boot);
        if (w.find("StartGame")) in.call_function("StartGame", {}, &boot);
    } catch (const TurnSuspended &) {
        // synchronous `play sound` during boot: the rest of Begin is abandoned
    }
    in.drain_on_ready();
    auto auto_advance = [&]() {
        int guard = 0;
        while (!w.finished && in.pending_wait() && guard++ < 100) in.finish_wait();
    };
    auto_advance();
    // Oracle DrainTimers: pending_tick mirrors RequestNextTimerTick (the engine
    // reports it after every call; we poll next_timer_seconds() at the same
    // points). While a SetTimeout timer pends, tick exactly its trigger delta.
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

    bool echo = w.asl_version >= 520;
    std::ifstream script(argv[2]);
    std::vector<std::string> lines;
    std::string raw;
    while (std::getline(script, raw)) lines.push_back(raw);
    size_t li = 0;
    int steps = 0;

    // Resolve a script line to a menu option key: exact key, display text
    // (case-insensitive), or 1-based number. Empty return = no match.
    auto resolve_menu_key = [](const MenuData &m, std::string l) -> std::string {
        if (l.compare(0, 5, "menu:") == 0) l = nr_trim(l.substr(5));
        for (auto &kv : m.options)
            if (kv.first == l) return kv.first;
        for (auto &kv : m.options)
            if (rt_iequals(kv.second, l.c_str())) return kv.first;
        char *end = nullptr;
        long n = std::strtol(l.c_str(), &end, 10);
        if (end && *end == 0 && n >= 1 && (size_t)n <= m.options.size())
            return m.options[n - 1].first;
        return std::string();
    };

    // The EXPRESSION form of ShowMenu blocks mid-script for its answer
    // (ExpressionOwner.ShowMenu awaits); feed it the next script line, like
    // the oracle driver feeds its PendingMenu.
    in.menu_provider = [&](const MenuData &m, std::string &key) -> bool {
        while (li < lines.size()) {
            std::string cmd = nr_trim(lines[li++]);
            if (cmd.empty() || cmd[0] == '#') continue;
            steps++;
            std::string k = resolve_menu_key(m, cmd);
            if (k.empty()) {
                std::cerr << "[warn] step " << steps << ": '" << cmd
                          << "' is not a valid menu option; cancelling menu\n";
                return false;
            }
            key = k;
            return true;
        }
        return false;  // script exhausted
    };

    while (li < lines.size()) {
        if (w.finished) break;
        std::string cmd = nr_trim(lines[li++]);
        if (cmd.empty() || cmd[0] == '#') continue;
        steps++;
        if (const MenuData *m = in.pending_menu()) {
            std::string l = cmd;
            if (l.compare(0, 5, "menu:") == 0) l = nr_trim(l.substr(5));
            const std::string *key = nullptr;
            std::string keybuf;
            for (auto &kv : m->options)
                if (kv.first == l) { keybuf = kv.first; key = &keybuf; break; }
            if (!key) {
                for (auto &kv : m->options)
                    if (rt_iequals(kv.second, l.c_str())) { keybuf = kv.first; key = &keybuf; break; }
            }
            if (!key) {
                char *end = nullptr;
                long n = std::strtol(l.c_str(), &end, 10);
                if (end && *end == 0 && n >= 1 && (size_t)n <= m->options.size()) {
                    keybuf = m->options[n - 1].first;
                    key = &keybuf;
                }
            }
            if (!key)
                std::cerr << "[warn] step " << steps << ": '" << cmd
                          << "' is not a valid menu option; cancelling menu\n";
            in.set_menu_response(key);
        } else if (in.pending_question()) {
            std::string l = cmd;
            if (l.compare(0, 7, "answer:") == 0) l = nr_trim(l.substr(7));
            for (char &c : l) c = (char)std::tolower((unsigned char)c);
            in.set_question_response(l == "yes" || l == "y" || l == "true" || l == "1");
        } else if (cmd.compare(0, 7, "assert:") == 0) {
            std::string expr = cmd.substr(7);
            bool ok = false;
            try {
                ok = Interp::truthy(in.eval(expr, boot));
            } catch (const std::exception &err) {
                std::cerr << "  err: " << err.what() << "\n";
            }
            line_out(std::string("[assert ") + (ok ? "PASS" : "FAIL") + "] " + expr);
        } else if (cmd.compare(0, 6, "label:") == 0 ||
                   cmd.compare(0, 6, "delay:") == 0 ||
                   cmd.compare(0, 8, "runtime:") == 0) {
            // editor bookkeeping
        } else if (cmd.compare(0, 6, "event:") == 0) {
            std::string rest = cmd.substr(6);
            size_t sc = rest.find(';');
            std::string name = sc == std::string::npos ? rest : rest.substr(0, sc);
            std::string param = sc == std::string::npos ? "" : rest.substr(sc + 1);
            try {
                if (w.find(name)) in.call_function(name, {vstr(param)}, &boot);
            } catch (const TurnSuspended &) {
            }
        } else {
            if (echo) line_out("> " + cmd);
            in.send_command(cmd);
        }
        in.drain_on_ready();
        auto_advance();
        drain_timers();
    }

    line_out(std::string("[state=") + (w.finished ? "Finished" : "Running") + "]");
    std::cout << normalise(transcript);
    std::cerr << "[diag] steps=" << steps << " errors=" << w.errors.size()
              << " finished=" << w.finished << "\n";
    for (size_t i = 0; i < w.errors.size() && i < 12; ++i)
        std::cerr << "  err: " << w.errors[i] << "\n";
    return 0;
}
