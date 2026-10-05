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

// aslx_drive.hh -- the .cmd script driver the native Quest 5 harness programs
// share (aslx_replay.cc, aslx_linkcapture.cc). It mirrors oracle/Program.cs:
// same step grammar (menu:/answer:/assert:/save:/tick:/label:/delay:/runtime:/
// event:/# comments; bare lines resolve pending menus and questions), same
// auto-advance of pending waits, same deterministic DrainTimers (after each
// step, tick pending SetTimeout timers by exactly their trigger delta; only
// self-destructing "timeout*" timers are drained so recurring authored timers
// never loop) or, under a `#!clock=N` header, the same typing clock (N seconds
// per typed command, nothing drained -- the model a real-time chase needs).
// What a program does with the game's output, and with each line the script
// types, is its own business: see Driver::on_typed and Driver::on_note.
#ifndef ASLX_DRIVE_HH
#define ASLX_DRIVE_HH

#include "../../../quest5/aslx-runtime-internal.hh"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace aslx_drive {

using namespace aslx;

inline std::string nr_trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Load the game with the Core library named by ASLX_CORE (default: the in-tree
// one, relative to the repository root).
inline bool load_game(const char *path, World &w) {
    const char *core = std::getenv("ASLX_CORE");
    if (load_file(path, w, core ? core : "terps/question/quest5/aslx-core"))
        return true;
    std::cerr << "[fatal] load failed\n";
    return false;
}

// Program.cs ResolveMenuKey: resolve a script line to a menu option key --
// exact key, display text (case-insensitive), or 1-based number.
inline bool resolve_menu_key(const MenuData &m, std::string l, std::string &key) {
    if (l.compare(0, 5, "menu:") == 0) l = nr_trim(l.substr(5));
    for (auto &kv : m.options)
        if (kv.first == l) { key = kv.first; return true; }
    for (auto &kv : m.options)
        if (rt_iequals(kv.second, l.c_str())) { key = kv.first; return true; }
    char *end = nullptr;
    long n = std::strtol(l.c_str(), &end, 10);
    if (end && *end == 0 && n >= 1 && (size_t)n <= m.options.size()) {
        key = m.options[n - 1].first;
        return true;
    }
    return false;
}

// Program.cs ParseYesNo.
inline bool parse_yes_no(std::string l) {
    if (l.compare(0, 7, "answer:") == 0) l = nr_trim(l.substr(7));
    for (char &c : l) c = (char)std::tolower((unsigned char)c);
    return l == "yes" || l == "y" || l == "true" || l == "1";
}

// Engine output as the oracle harness prints it: tags removed (a <br> is a
// line break) and character references decoded.
inline std::string strip_html(std::string s) {
    std::string out;
    // <br> variants -> newline; every other tag removed.
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '<') {
            size_t close = s.find('>', i);
            if (close == std::string::npos) { out += s.substr(i); break; }
            std::string tag = s.substr(i + 1, close - i - 1);
            // qvh's Strip regex ("<br\\s*/?>") is case-SENSITIVE: an
            // uppercase <BR> is dropped by the generic tag stripper, no
            // newline. Mirror that exactly -- including \s*, so A Stranger,
            // Unregarded's fly (`<br  />`, two spaces) gets its own line.
            if (tag.compare(0, 2, "br") == 0) {
                size_t j = 2;
                while (j < tag.size() && std::isspace((unsigned char)tag[j])) j++;
                if (j < tag.size() && tag[j] == '/') j++;
                if (j == tag.size()) out += '\n';
            }
            i = close + 1;
        } else {
            out += s[i++];
        }
    }
    // Basic HTML entity decode (WebUtility.HtmlDecode subset).
    struct { const char *e; const char *r; } ents[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""},
        // WebUtility.HtmlDecode: &nbsp; is U+00A0, NOT an ASCII space
        // (spondre's menu bullets are "&nbsp;&bull; ..." and its golden
        // carries the real no-break byte); it knows every named entity --
        // add them here as the corpus needs them.
        {"&#39;", "'"}, {"&apos;", "'"}, {"&nbsp;", "\xc2\xa0"},
        {"&bull;", "\xe2\x80\xa2"},
        // Typographic entities authors paste into hand-written HTML. Moquette
        // is written almost entirely in "&mdash;"-separated asides, so without
        // these the native transcript keeps the raw entity where the oracle
        // prints the character. Kept in step with aslxglk.cc's own table.
        {"&mdash;", "\xe2\x80\x94"}, {"&ndash;", "\xe2\x80\x93"},
        {"&hellip;", "\xe2\x80\xa6"}, {"&middot;", "\xc2\xb7"},
        {"&lsquo;", "\xe2\x80\x98"}, {"&rsquo;", "\xe2\x80\x99"},
        {"&ldquo;", "\xe2\x80\x9c"}, {"&rdquo;", "\xe2\x80\x9d"},
        {"&laquo;", "\xc2\xab"}, {"&raquo;", "\xc2\xbb"},
        {"&copy;", "\xc2\xa9"}, {"&reg;", "\xc2\xae"},
        {"&trade;", "\xe2\x84\xa2"}, {"&deg;", "\xc2\xb0"},
        {"&eacute;", "\xc3\xa9"},
    };
    for (auto &en : ents) {
        size_t pos = 0;
        std::string e = en.e;
        while ((pos = out.find(e, pos)) != std::string::npos) {
            out.replace(pos, e.size(), en.r);
            pos += 1;
        }
    }
    // Numeric character references, which HtmlDecode also expands: "&#NNN;" and
    // "&#xHH;". Woo Rebooted's completed-task list is built from "&#10004;"
    // (heavy check mark), so a named-entity table alone is not enough.
    std::string dec;
    for (size_t i = 0; i < out.size();) {
        if (out[i] == '&' && i + 2 < out.size() && out[i + 1] == '#') {
            size_t j = i + 2;
            int base = 10;
            if (j < out.size() && (out[j] == 'x' || out[j] == 'X')) { base = 16; ++j; }
            size_t start = j;
            unsigned long cp = 0;
            bool ok = true;
            for (; j < out.size() && out[j] != ';'; ++j) {
                int d;
                if (out[j] >= '0' && out[j] <= '9') d = out[j] - '0';
                else if (base == 16 && out[j] >= 'a' && out[j] <= 'f') d = out[j] - 'a' + 10;
                else if (base == 16 && out[j] >= 'A' && out[j] <= 'F') d = out[j] - 'A' + 10;
                else { ok = false; break; }
                cp = cp * base + (unsigned long)d;
                if (cp > 0x10FFFF) { ok = false; break; }
            }
            if (ok && j > start && j < out.size() && out[j] == ';') {
                if (cp < 0x80) {
                    dec += (char)cp;
                } else if (cp < 0x800) {
                    dec += (char)(0xC0 | (cp >> 6));
                    dec += (char)(0x80 | (cp & 0x3F));
                } else if (cp < 0x10000) {
                    dec += (char)(0xE0 | (cp >> 12));
                    dec += (char)(0x80 | ((cp >> 6) & 0x3F));
                    dec += (char)(0x80 | (cp & 0x3F));
                } else {
                    dec += (char)(0xF0 | (cp >> 18));
                    dec += (char)(0x80 | ((cp >> 12) & 0x3F));
                    dec += (char)(0x80 | ((cp >> 6) & 0x3F));
                    dec += (char)(0x80 | (cp & 0x3F));
                }
                i = j + 1;
                continue;
            }
        }
        dec += out[i++];
    }
    return dec;
}

class Driver {
public:
    // Called with each line the script types at the game -- a command, or the
    // text an expression-form GetInput asked for -- just before it is sent.
    std::function<void(const std::string &)> on_typed;
    // Called with each out-of-band transcript line ("[assert PASS] ...").
    std::function<void(const std::string &)> on_note;
    // The context the boot scripts ran in; `assert:` expressions share it.
    Context boot;
    // Script lines consumed so far, comments and blank lines aside.
    int steps = 0;

    // The script and its prompt providers go in BEFORE the boot: StartGame
    // can itself raise an expression-form Ask/ShowMenu/GetInput (The Day
    // the Sky Fell Down asks about sound effects), which the oracle feeds
    // from the first script line.
    Driver(World &world, Interp &interp, const char *game, const char *script)
        : w(world), in(interp), game_path(game) {
        std::ifstream f(script);
        std::string raw;
        while (std::getline(f, raw)) lines.push_back(raw);
        // qvh `#!errorlimit=N` directive: raise the script-error breaker
        // threshold for this game (legacy Quest had no breaker at all).
        // qvh `#!clock=N` directive: typing clock instead of DrainTimers --
        // each typed command costs N seconds (one tick after it settles),
        // nothing is drained. Games with a real-time chase need it; see
        // Program.cs SettleClock for the why.
        for (auto &l : lines) {
            if (l.rfind("#!errorlimit=", 0) == 0)
                in.set_max_script_errors((int)std::strtol(l.c_str() + 13, nullptr, 10));
            else if (l.rfind("#!clock=", 0) == 0) {
                long cs = std::strtol(l.c_str() + 8, nullptr, 10);
                if (cs > 0) clock_secs = (int)cs;
            }
        }

        // Headless audio: a no-op host hook that RETURNS means "playback
        // finished". Without it a synchronous `play sound` parks the rest of
        // the turn on the wait slot forever unless something later claims it
        // -- and when the sound is the last statement before the ending (HMS
        // Victory's "Eight bells.wav", Nearco II's "cantoninfa.mp3") nothing
        // ever does, so the win is lost. The oracle had the identical bug; its
        // HeadlessPlayer.PlaySoundAsync now flags IsWaiting for the same
        // reason. Both mirror QuestViva's own WebPlayer, which forces
        // synchronous=false + Runner.BeginWait() under a walkthrough runner.
        // Games whose parked tail IS claimed later (The Tree's `x tube`
        // holcast, I Contain Multitudes) are unaffected: resuming inline and
        // resuming at the next claim produce the same transcript when no
        // output sits between the two points.
        in.play_sound = [](const std::string &, bool, bool) {};

        // The EXPRESSION form of ShowMenu blocks mid-script for its answer
        // (ExpressionOwner.ShowMenu awaits); feed it the next script line,
        // like the oracle driver feeds its PendingMenu.
        in.menu_provider = [this](const MenuData &m, std::string &key) -> bool {
            std::string cmd;
            if (!next(cmd)) return false;  // script exhausted
            if (resolve_menu_key(m, cmd, key)) return true;
            warn_menu(cmd);
            return false;
        };

        // The EXPRESSION form of Ask blocks mid-script the same way; feed it
        // the next script line, accepting the same `answer:` grammar as a
        // pending `ask` statement (and a bare yes/no, per the
        // lenient-resolution rule).
        in.ask_provider = [this](const std::string &, bool &answer) -> bool {
            std::string cmd;
            if (!next(cmd)) return false;  // script exhausted
            answer = parse_yes_no(cmd);
            return true;
        };

        // The EXPRESSION form of GetInput blocks mid-script for the next
        // command line. The oracle driver feeds it its next script line
        // through SendCommand -- echoed "> line" like any command (the
        // engine's own echo never happens: the override branch bypasses
        // HandleCommand) -- so it counts as typed here too.
        in.input_provider = [this](std::string &text) -> bool {
            std::string cmd;
            if (!next(cmd)) return false;  // script exhausted
            if (on_typed) on_typed(cmd);
            text = cmd;
            return true;
        };
    }

    // Boot the game, the fresh way or -- once a save has been restored onto
    // it -- the saved-game way (WorldModel.BeginInternalAsync with
    // _loadedFromSaved: InitInterface only, no begin_timers/StartGame).
    void begin(bool restored = false) {
        if (!restored)
            in.begin_timers();  // BeginInternalAsync arms enabled timers before InitInterface
        try {
            if (w.find("InitInterface")) in.call_function("InitInterface", {}, &boot);
            if (!restored && w.find("StartGame")) in.call_function("StartGame", {}, &boot);
            // BeginInternalAsync ends with UpdateListsAsync -- with no
            // UpdateList subscriber that is just UpdateStatusAttributes,
            // exactly like qvh.
            in.update_lists();
        } catch (const TurnSuspended &) {
            // synchronous `play sound` during boot: the rest of Begin is abandoned
        }
        in.drain_on_ready();
        auto_advance();
        settle_clock(false);  // qvh: DrainTimers after Begin (no-op under the clock)
    }

    // Play the rest of the script, one step per line.
    void run() {
        std::string cmd;
        while (!w.finished && next(cmd)) {
            bool typed = false;
            if (const MenuData *m = in.pending_menu()) {
                std::string key;
                bool ok = resolve_menu_key(*m, cmd, key);
                if (!ok) warn_menu(cmd);
                in.set_menu_response(ok ? &key : nullptr);
            } else if (in.pending_question()) {
                in.set_question_response(parse_yes_no(cmd));
            } else if (cmd.compare(0, 5, "save:") == 0) {
                // Write a native .quest-save at this point (save-compat cross-test).
                std::string path = nr_trim(cmd.substr(5));
                std::ofstream of(path, std::ios::binary);
                of << in.save_game_native(game_path);
            } else if (cmd.compare(0, 7, "assert:") == 0) {
                std::string expr = cmd.substr(7);
                bool ok = false;
                try {
                    ok = Interp::truthy(in.eval(expr, boot));
                } catch (const std::exception &err) {
                    std::cerr << "  err: " << err.what() << "\n";
                }
                if (on_note)
                    on_note(std::string("[assert ") + (ok ? "PASS" : "FAIL") + "] " + expr);
            } else if (cmd.compare(0, 6, "label:") == 0 ||
                       cmd.compare(0, 6, "delay:") == 0 ||
                       cmd.compare(0, 8, "runtime:") == 0) {
                // editor bookkeeping
            } else if (cmd.compare(0, 6, "event:") == 0) {
                std::string rest = cmd.substr(6);
                size_t sc = rest.find(';');
                std::string name = sc == std::string::npos ? rest : rest.substr(0, sc);
                std::string param = sc == std::string::npos ? "" : rest.substr(sc + 1);
                // qvh: await world.SendEvent(name, param)
                in.send_event(name, param);
                typed = true;
            } else if (cmd.compare(0, 5, "tick:") == 0) {
                // qvh tick:N — deterministic real-time advance: tick the game
                // clock by exactly N seconds, firing due AUTHORED timers (the
                // script-driven counterpart of the interactive 1s JS interval).
                // Needed by games whose progression gates on authored one-shot
                // timers, which drain_timers deliberately leaves dormant.
                long tsecs = std::strtol(cmd.c_str() + 5, nullptr, 10);
                if (tsecs > 0) {
                    in.tick((int)tsecs);
                    auto_advance();
                }
            } else {
                if (on_typed) on_typed(cmd);
                in.send_command(cmd);
                typed = true;
            }
            in.drain_on_ready();
            auto_advance();
            settle_clock(typed);
        }
    }

private:
    World &w;
    Interp &in;
    std::string game_path;
    std::vector<std::string> lines;
    size_t li = 0;
    int clock_secs = 0;

    // The next script step, skipping blank lines and # comments.
    bool next(std::string &cmd) {
        while (li < lines.size()) {
            cmd = nr_trim(lines[li++]);
            if (cmd.empty() || cmd[0] == '#') continue;
            steps++;
            return true;
        }
        return false;
    }

    void warn_menu(const std::string &cmd) {
        std::cerr << "[warn] step " << steps << ": '" << cmd
                  << "' is not a valid menu option; cancelling menu\n";
    }

    void auto_advance() {
        int guard = 0;
        while (!w.finished && in.pending_wait() && guard++ < 100) in.finish_wait();
    }

    // Oracle DrainTimers: pending_tick mirrors RequestNextTimerTick (the engine
    // reports it after every call; we poll next_timer_seconds() at the same
    // points). While a SetTimeout timer pends, tick exactly its trigger delta.
    void drain_timers() {
        int guard = 0;
        int pending_tick = in.next_timer_seconds();
        while (!w.finished && guard++ < 500 && in.has_enabled_timeout()) {
            int secs = pending_tick;
            in.tick(secs > 0 ? secs : 1);
            auto_advance();
            pending_tick = in.next_timer_seconds();
        }
    }

    // Program.cs SettleClock: drain (default) or, under the typing clock, tick
    // exactly clock_secs once per typed command/event -- never for a menu or
    // question answer (they belong to the command that asked), nor for
    // save:/assert:/tick: bookkeeping.
    void settle_clock(bool typed) {
        if (clock_secs == 0) { drain_timers(); return; }
        if (!typed || w.finished) return;
        in.tick(clock_secs);
        auto_advance();
    }
};

}  // namespace aslx_drive

#endif
