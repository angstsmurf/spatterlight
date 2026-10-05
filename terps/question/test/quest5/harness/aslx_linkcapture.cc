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

// aslx_linkcapture.cc -- replay an oracle .cmd walkthrough through the native
// aslx engine (with aslx_replay.cc's driver, aslx_drive.hh) but instead of a
// plain transcript,
// report, for every script command that clicks an on-screen link, the visible
// TEXT of that link. Command links are emitted by the Core text processor as
//   <a ... class="cmdlink ..." data-command="CMD">DISPLAY TEXT</a>
// (CoreOutput.aslx ProcessTextCommand_Command). We collect every such link as
// it scrolls past, and when the script sends CMD we match it to the most
// recent still-unspent link carrying that data-command and print its text.
#include "aslx_drive.hh"

#include <iostream>
#include <string>
#include <vector>

using namespace aslx;

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
            l.text = aslx_drive::strip_html(inner);
            g_links.push_back(l);
        }
        i = end + 4;
    }
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
    if (!aslx_drive::load_game(argv[1], w)) return 3;
    Interp in(w);
    // Capture raw HTML (with <a> tags intact) so we can harvest links.
    in.print = [&](const std::string &html) { collect_links(html); };

    aslx_drive::Driver drive(w, in, argv[1], argv[2]);
    int clicks = 0;
    drive.on_typed = [&](const std::string &cmd) {
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
    };
    drive.begin();
    drive.run();

    std::cerr << "[diag] steps=" << clicks << " finished=" << w.finished
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
