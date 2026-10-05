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

// aslx_replay.cc -- replay an oracle .cmd walkthrough script through the
// NATIVE aslx engine and emit a qvh-normalised transcript, for diffing
// against the frozen goldens in ../goldens/. This is the native half of the
// milestone-6 ground-truth harness; the .cmd/.txt pairs were produced by the
// QuestViva oracle (oracle/README.md).
//
//   make aslx_replay
//   ASLX_CORE=../../../quest5/aslx-core ./aslx_replay "<game.quest>" \
//       "../goldens/<Game>.cmd" | diff "../goldens/<Game>.txt" -
//
// Mirrors oracle/Program.cs: the step grammar, the auto-advance of pending
// waits and the timer model are aslx_drive.hh's (shared with
// aslx_linkcapture.cc); here are the same HTML-strip + blank-line-collapse
// normalisation, same "> cmd" echo for v520+ and final "[state=...]" line,
// same Wedged-vs-Finished distinction (a Finished forced by the 20-error
// breaker reports as Wedged, via Interp::script_errors_fatal()).
//
// Baseline at first light (2026-07-16, input-model commit) was ~30-70%
// matching lines and 0 games finishing; by end of day 16 of 17 goldens
// replay BYTE-IDENTICAL (incl. Whitefield's Wedged error-cascade and Bony
// King's depth-cap death spiral). The one remaining diff, I Contain
// Multitudes, is a DOCUMENTED QuestViva artifact: its pending
// synchronous-sound TCS leak means the oracle never shows the ending; the
// native engine ends the game like real Quest. Do not chase.
//
// 2026-07-17: corpus grown to 25 goldens (the six formerly hints-only games
// + the PDF-only Jenny Lee, each driven by a curated override); 24 of 25
// replay byte-identical (ICM = the same documented artifact). The batch cost
// three engine/harness fixes: GetExitByLink + dictionary ListCount (Hawk),
// deferred else-if condition compile (Serpent's Eye's literal `else if ()`),
// and .NET-TrimEnd-equivalent Unicode whitespace stripping here (Hawk's
// literal trailing U+00A0s). Then 26 goldens (The Deer Trail, an override
// rendering the author's "Direction to <room>" shorthand into parser
// commands); it replayed byte-identical with no engine changes — 25 of 26.
#include "aslx_drive.hh"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace aslx;

static std::string normalise(const std::string &s) {
    std::istringstream is(s);
    std::ostringstream os;
    std::string line;
    int blanks = 0;
    bool first = true;
    // qvh trims with .NET TrimEnd(), whose default set is char.IsWhiteSpace --
    // Unicode whitespace, not just ASCII. The one non-ASCII member that occurs
    // in practice is U+00A0 (Hawk the Hunter has literal trailing nbsp before
    // a <br/>); strip the common UTF-8 whitespace sequences too.
    auto strip_trailing_ws = [](std::string &l) {
        for (;;) {
            if (!l.empty() && (l.back() == ' ' || l.back() == '\t' ||
                               l.back() == '\r' || l.back() == '\v' ||
                               l.back() == '\f')) {
                l.pop_back();
                continue;
            }
            if (l.size() >= 2 && (unsigned char)l[l.size() - 2] == 0xC2 &&
                ((unsigned char)l.back() == 0xA0 ||    // U+00A0 nbsp
                 (unsigned char)l.back() == 0x85)) {   // U+0085 NEL
                l.erase(l.size() - 2);
                continue;
            }
            if (l.size() >= 3 && (unsigned char)l[l.size() - 3] == 0xE2 &&
                (unsigned char)l[l.size() - 2] == 0x80 &&
                (((unsigned char)l.back() >= 0x80 &&
                  (unsigned char)l.back() <= 0x8A) ||  // U+2000..U+200A
                 (unsigned char)l.back() == 0xA8 ||    // U+2028
                 (unsigned char)l.back() == 0xA9 ||    // U+2029
                 (unsigned char)l.back() == 0xAF)) {   // U+202F
                l.erase(l.size() - 3);
                continue;
            }
            if (l.size() >= 3 && (unsigned char)l[l.size() - 3] == 0xE3 &&
                (unsigned char)l[l.size() - 2] == 0x80 &&
                (unsigned char)l.back() == 0x80) {     // U+3000
                l.erase(l.size() - 3);
                continue;
            }
            break;
        }
    };
    while (std::getline(is, line)) {
        strip_trailing_ws(line);
        if (line.empty()) { blanks++; continue; }
        if (!first && blanks > 0) os << "\n";
        os << line << "\n";
        blanks = 0;
        first = false;
    }
    return os.str();
}

int main(int argc, char **argv) {
    if (argc < 3) { std::cerr << "usage: nreplay <game> <script>\n"; return 2; }
    World w;
    if (!aslx_drive::load_game(argv[1], w)) return 3;
    Interp in(w);
    std::string transcript;
    // GetFileData reads the .quest package entry, or the file beside a bare
    // .aslx (as aslxglk's resource_bytes does).
    std::string package;
    {
        std::ifstream f(argv[1], std::ios::binary);
        package.assign(std::istreambuf_iterator<char>(f), {});
    }
    std::vector<ZipEntryInfo> entries;
    bool is_package = zip_list_entries((const uint8_t *)package.data(),
                                       package.size(), entries);
    std::string story_dir = argv[1];
    story_dir = story_dir.find('/') == std::string::npos
        ? "." : story_dir.substr(0, story_dir.rfind('/'));
    in.resource_provider = [&](const std::string &name, std::string &out) {
        if (is_package) {
            const ZipEntryInfo *e = zip_find_entry(entries, name);
            return e && zip_extract_entry((const uint8_t *)package.data(),
                                          package.size(), *e, out);
        }
        std::ifstream f(story_dir + "/" + name, std::ios::binary);
        if (!f) return false;
        out.assign(std::istreambuf_iterator<char>(f), {});
        return true;
    };
    // ASLX_RAW=1: mirror every print's raw HTML to stderr, for chasing a
    // transcript diff back to the markup that produced it.
    static const bool raw_trace = std::getenv("ASLX_RAW") != nullptr;
    auto emit = [&](const std::string &html) {
        if (raw_trace) fprintf(stderr, "RAW[%s]\n", html.c_str());
        std::string t = aslx_drive::strip_html(html);
        if (t.empty()) return;
        transcript += t;
        if (t.back() != '\n') transcript += '\n';
    };
    in.print = emit;
    // ASLX_GRID_TRACE=1: dump the grid-map paint commands (CoreGrid.aslx ->
    // grid.js) to stderr, so what the Glk map pane is asked to draw can be
    // checked without running the app. Installing the hook also switches the
    // JS.* bridge onto its grid vocabulary, so it stays off by default.
    if (std::getenv("ASLX_GRID_TRACE")) {
        in.grid_draw = [](const GridDraw &g) {
            fprintf(stderr, "GRID op=%d x=%g y=%g x2=%g y2=%g z=%d w=%g h=%g "
                    "border=%s fill=%s text=%s\n", (int) g.op, g.x, g.y,
                    g.x2, g.y2, g.z, g.w, g.h, g.border.c_str(),
                    g.fill.c_str(), g.text.c_str());
        };
    }
    auto line_out = [&](const std::string &s) { transcript += s + "\n"; };

    // ASLX_RESTORE=<file>: apply a saved game onto the freshly-loaded original
    // and boot the saved-game way (WorldModel.BeginInternalAsync with
    // _loadedFromSaved -- InitInterface only, no begin_timers/StartGame). Used
    // by the native<->QuestViva save-compat cross-test; the save may be one this
    // engine wrote OR one QuestViva wrote (native format).
    bool restored = false;
    if (const char *rf = std::getenv("ASLX_RESTORE")) {
        std::ifstream sf(rf, std::ios::binary);
        std::stringstream ss; ss << sf.rdbuf();
        std::string data = ss.str();
        std::string err;
        restored = in.restore_game(data, err);
        if (!restored) { std::cerr << "[fatal] restore failed: " << err << "\n"; return 4; }
    }

    // The driver goes in BEFORE the boot: it installs the prompt providers
    // StartGame may already need. The oracle echoes what the script types
    // from v520 on.
    aslx_drive::Driver drive(w, in, argv[1], argv[2]);
    bool echo = w.asl_version >= 520;
    drive.on_typed = [&](const std::string &cmd) {
        if (echo) line_out("> " + cmd);
    };
    drive.on_note = line_out;
    drive.begin(restored);
    drive.run();

    // A Finished reached only because the 20-error breaker fired is reported
    // as Wedged, exactly like qvh's reflection probe on _scriptErrorsFatal.
    line_out(std::string("[state=") +
             (in.script_errors_fatal() ? "Wedged"
              : w.finished             ? "Finished"
                                       : "Running") +
             "]");
    std::cout << normalise(transcript);
    std::cerr << "[diag] steps=" << drive.steps << " errors=" << w.errors.size()
              << " finished=" << w.finished << "\n";
    for (size_t i = 0; i < w.errors.size() && i < 500; ++i)
        std::cerr << "  err: " << w.errors[i] << "\n";
    return 0;
}
