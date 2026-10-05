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

/* Profiling driver: replay a Quest 4 walkthrough in-process N times so a
   sampler has a single long-lived target.  Not a test; built on demand.

     make quest4/harness/question_profile_loop
     QUESTION_SEED=1 ./question_profile_loop <iterations> <game-file> <command-script>

   It reuses the engine exactly as question_walkthrough_runner does (same includes,
   same minimal QuestionInterface), minus the runner's fight/scumming/marker logic:
   every scripted line is fed straight to run_command.  Games that need those
   extras will diverge, but the per-turn engine work being profiled is the same.
*/

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "headless_common.hh"

namespace {

class RunnerInterface : public HeadlessInterface
{
protected:
  void debug_print (const std::string &) override { }
  QuestionResult wait_keypress (const std::string &) override { return r_success; }
  QuestionResult pause (int) override { return r_success; }
};

} // namespace

int main (int argc, char **argv)
{
  if (argc < 4)
    {
      std::cerr << "usage: " << argv[0] << " <iterations> <game> <script>\n";
      return 2;
    }
  long iters = atol (argv[1]);
  std::string game = argv[2], script = argv[3];

  std::ifstream sf (script.c_str ());
  if (!sf) { std::cerr << "cannot open script " << script << "\n"; return 2; }
  std::vector<std::string> cmds;
  std::string line;
  while (std::getline (sf, line))
    {
      std::string t = trim (line);
      if (t.empty ()) continue;
      std::string::size_type p = t.find ("  (");
      if (p != std::string::npos) t = trim (t.substr (0, p));
      if (!t.empty ()) cmds.push_back (t);
    }

  setenv ("QUESTION_STRICT_NAMES", "1", 1);

  long total_turns = 0;
  for (long it = 0; it < iters; it++)
    {
      RunnerInterface gi;
      QuestionRunner *gr = QuestionRunner::get_runner (&gi);
      g_queue.clear ();
      for (const std::string &c : cmds) g_queue.push_back (c);
      try
        {
          gr->set_game (game);
          while (gr->is_running () && !g_queue.empty ())
            {
              std::string c = g_queue.front (); g_queue.pop_front ();
              gr->run_command (c);
              total_turns++;
            }
        }
      catch (const InputExhausted &) { }
      delete gr;
    }
  std::cout << "iterations=" << iters << " total_turns=" << total_turns << "\n";
  return 0;
}
