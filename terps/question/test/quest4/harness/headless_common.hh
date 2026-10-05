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

/*
  headless_common.hh -- the prelude the Quest 4 harness programs share.

  It unity-includes the whole engine, the same trick QuestionRegressionTests.mm
  uses, so a program that includes it is a complete Question with no Glk in
  sight.  (QuestionInterface's out-of-line virtuals, and so its vtable, live in
  question-runner.cc: even a program that only wants the loader gets it all.)
  On top of that comes HeadlessInterface, the QuestionInterface every harness
  would otherwise write for itself: output dropped, files read from disk, and
  input served from g_queue.

  Include it once, from the program's one translation unit.
*/

#ifndef HEADLESS_COMMON_HH
#define HEADLESS_COMMON_HH

#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../../../question-util.cc"
#include "../../../istring.cc"
#include "../../../readfile.cc"
#include "../../../questionfile.cc"
#include "../../../question-state.cc"

/* The engine's `rand` script function is the one thing in a replay that is not
   fixed by the game and the script, and the transcripts in ../goldens are
   byte-compared -- so the draws have to be the same everywhere.  question-runner.cc
   draws from erkyrath_random() (common_utils/randomness.c), the same generator
   the Spatterlight build uses: seeded, it is xoshiro128**, a fixed algorithm
   that gives identical numbers on every platform and in the app.  The Makefile
   compiles randomness.c alongside each program; nothing here has to substitute
   a generator of its own. */
#include "../../../question-runner.cc"
#include "../../../question-vars.cc"
#include "../../../question-objects.cc"
#include "../../../question-rooms.cc"
#include "../../../question-session.cc"
#include "../../../question-parse.cc"
#include "../../../question-script.cc"
#include "../../../question-functions.cc"
#include "../../../question-panes.cc"

/* Shared input stream: commands plus the menu/free-text answers they prompt,
   served from the *same* queue by make_choice ()/get_string (), so a script
   reads exactly like what a player would type. */
inline std::deque<std::string> g_queue;

/* Thrown once the game has asked for input far more often than the script can
   answer.  A Quest game that validates its input ("How many players?" ... "Try
   again.") loops until it gets something it likes, and real Quest just blocks on
   the prompt; headless, an empty queue answers "" forever and the game spins,
   filling the transcript.  Bail out instead of running until the OS kills us. */
struct InputExhausted { };

inline std::string
dirname_of (const std::string &p)
{
  std::string::size_type s = p.find_last_of ('/');
  return s == std::string::npos ? std::string (".") : p.substr (0, s);
}

/* The whole of file `fn' in `out'; false if it cannot be opened. */
inline bool
read_whole_file (const std::string &fn, std::string &out)
{
  std::ifstream f (fn.c_str (), std::ios::binary);
  if (!f)
    return false;
  std::ostringstream ss;
  ss << f.rdbuf ();
  out = ss.str ();
  return true;
}

class HeadlessInterface : public QuestionInterface
{
public:
  /* Consecutive answers served from an empty queue; see InputExhausted.  The cap
     is generous because a game may legitimately keep asking after the script
     ends (an end-of-game "press any key", a final menu) without looping. */
  int starved = 0;
  static const int kMaxStarved = 200;

protected:
  QuestionResult print_normal (const std::string &) override { return r_success; }
  QuestionResult print_newline () override { return r_success; }
  void set_foreground (const std::string &) override { }
  void set_background (const std::string &) override { }

  /* The next scripted answer in `s', or false if the script has run dry. */
  bool next_input (std::string &s)
  {
    if (g_queue.empty ())
      {
	if (++starved > kMaxStarved)
	  throw InputExhausted ();
	return false;
      }
    starved = 0;
    s = g_queue.front ();
    g_queue.pop_front ();
    return true;
  }

  /* The 0-based index for scripted menu answer `c', which counts from 1 and
     is held to the `n' choices on offer. */
  static uint clamp_choice (int c, size_t n)
  {
    if (c < 1)
      c = 1;
    if ((size_t) c > n)
      c = (int) n;
    return (uint) c - 1;
  }

  std::string get_string () override
  {
    std::string s;
    next_input (s);
    return s;
  }

  /* A menu answer is read with atoi, so a script supplies "1" and not "left";
     with nothing left to supply, the first choice it is. */
  uint make_choice (const std::string &, std::vector<std::string> choices) override
  {
    std::string s;
    return clamp_choice (next_input (s) ? atoi (s.c_str ()) : 1, choices.size ());
  }

  std::string absolute_name (const std::string &rel,
			     const std::string &parent) const override
  {
    if (!rel.empty () && rel[0] == '/')
      return rel;
    return dirname_of (parent) + "/" + rel;
  }

  std::string get_file (const std::string &fn) const override
  {
    std::string s;
    read_whole_file (fn, s);
    return s;
  }
};

#endif
