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
  question_walkthrough_runner -- headless walkthrough/regression runner for Question.


/*
  legacy_runner -- plays a Quest 4 walkthrough on the legacy Geas 0.5 engine.

  compare.py extracts the engine from git (513b0f4ff, the last commit before
  the 2026 rework), points its $rand at erkyrath_random and builds this file
  against that copy.  Nothing in Question is linked in.

  It follows question_walkthrough_runner's script rules, so that the two engines
  get the same input: the header is skipped up to a "Start:" line, a trailing
  "  (note)" is stripped, and commands, menu numbers and free-text answers come
  from one queue.  --tick calls tick_timers after each command, as Geas has no
  way to place the tick inside a turn the way Question does.

  Usage:  legacy_runner [--tick] [--seed N] [--echo] [--win MARKER] <game> <script>
  Exit:   0 win marker seen (or none given), 1 not seen, 2 usage, 3 did not load.
*/

#include <cstdint>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include "randomness.h"
}

#include "geas-util.cc"
#include "istring.cc"
#include "readfile.cc"
#include "geasfile.cc"
#include "geas-state.cc"
#include "geas-runner.cc"

namespace {

std::deque<std::string> g_queue;

/* A game that validates its input loops for as long as the answers are empty. */
struct InputExhausted { };

class LegacyInterface : public GeasInterface
{
public:
  std::string log;
  bool echo = false;

  void emit (const std::string &s)
  {
    log += s;
    if (echo)
      std::cout << s;
  }

  GeasResult print_normal (const std::string &s) override
  {
    emit (s);
    return r_success;
  }

  GeasResult print_newline () override
  {
    emit ("\n");
    return r_success;
  }

  void debug_print (const std::string &) override { }
  void set_foreground (const std::string &) override { }
  void set_background (const std::string &) override { }
  GeasResult wait_keypress (const std::string &msg) override
  {
    if (!msg.empty ())
      print_formatted (msg);
    return r_success;
  }
  GeasResult pause (int) override { return r_success; }

  std::string absolute_name (const std::string &rel,
			     const std::string &parent) const override
  {
    if (!rel.empty () && rel[0] == '/')
      return rel;
    std::string::size_type s = parent.find_last_of ('/');
    return (s == std::string::npos ? std::string (".") : parent.substr (0, s))
	   + "/" + rel;
  }

  std::string get_file (const std::string &fn) const override
  {
    std::ifstream f (fn.c_str (), std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf ();
    return ss.str ();
  }

  std::string get_string () override
  {
    std::string s;
    next_input (s);
    log += "[input] " + s + "\n";
    return s;
  }

  uint make_choice (const std::string &info,
		    std::vector<std::string> choices) override
  {
    emit (info + "\n");
    for (size_t i = 0; i < choices.size (); i ++)
      {
	std::ostringstream ss;
	ss << (i + 1) << ") " << choices[i] << "\n";
	emit (ss.str ());
      }
    int c = 1;
    std::string s;
    if (next_input (s))
      {
	emit ("[choice] " + s + "\n");
	c = atoi (s.c_str ());
      }
    if (c < 1)
      c = 1;
    if ((size_t) c > choices.size ())
      c = (int) choices.size ();
    return (uint) c - 1;
  }

private:
  int starved = 0;

  bool next_input (std::string &s)
  {
    if (g_queue.empty ())
      {
	if (++starved > 200)
	  throw InputExhausted ();
	return false;
      }
    starved = 0;
    s = g_queue.front ();
    g_queue.pop_front ();
    return true;
  }
};

std::string
lower (std::string s)
{
  for (char &c : s)
    c = (char) tolower ((unsigned char) c);
  return s;
}

std::string
strip (const std::string &s)
{
  static const char *const ws = " \t\r\n\f\v";
  std::string::size_type a = s.find_first_not_of (ws);
  if (a == std::string::npos)
    return "";
  return s.substr (a, s.find_last_not_of (ws) - a + 1);
}

/* The marker on a whole line of output that is not an echoed "> command". */
bool
marker_seen (const std::string &log, const std::string &marker)
{
  std::string m = lower (marker);
  std::string::size_type at = 0, nl;
  while ((nl = log.find ('\n', at)) != std::string::npos)
    {
      std::string ln = log.substr (at, nl - at);
      if (ln.rfind ("> ", 0) != 0 && lower (ln).find (m) != std::string::npos)
	return true;
      at = nl + 1;
    }
  return false;
}

bool
read_script (const std::string &path)
{
  std::ifstream f (path.c_str ());
  if (!f)
    return false;
  std::vector<std::string> raw;
  std::string line;
  while (std::getline (f, line))
    raw.push_back (line);
  size_t begin = 0;
  for (size_t i = 0; i < raw.size (); i++)
    if (lower (strip (raw[i])).rfind ("start:", 0) == 0)
      {
	begin = i + 1;
	break;
      }
  for (size_t i = begin; i < raw.size (); i++)
    {
      std::string t = strip (raw[i]);
      std::string::size_type p = t.find ("  (");
      if (p != std::string::npos)
	t = strip (t.substr (0, p));
      if (!t.empty ())
	g_queue.push_back (t);
    }
  return true;
}

}  /* namespace */

int
main (int argc, char **argv)
{
  bool tick = false, echo = false;
  uint32_t seed = 1;
  std::string win;
  std::vector<std::string> pos;
  for (int i = 1; i < argc; i++)
    {
      std::string a = argv[i];
      if (a == "--tick")
	tick = true;
      else if (a == "--echo")
	echo = true;
      else if (a == "--seed" && i + 1 < argc)
	seed = (uint32_t) atoi (argv[++i]);
      else if (a == "--win" && i + 1 < argc)
	win = argv[++i];
      else
	pos.push_back (a);
    }
  if (pos.size () != 2 || !read_script (pos[1]))
    {
      std::cerr << "usage: " << argv[0]
		<< " [--tick] [--seed N] [--echo] [--win MARKER] <game> <script>\n";
      return 2;
    }

  set_erkyrath_random (seed);

  LegacyInterface gi;
  gi.echo = echo;
  GeasRunner *gr = GeasRunner::get_runner (&gi);
  try
    {
      gr->set_game (pos[0]);
      if (!gr->is_running ())
	return 3;
      while (gr->is_running () && !g_queue.empty ())
	{
	  std::string c = g_queue.front ();
	  g_queue.pop_front ();
	  gr->run_command (c);
	  if (tick)
	    gr->tick_timers ();
	}
    }
  catch (const InputExhausted &)
    {
    }
  std::cout.flush ();
  return win.empty () || marker_seen (gi.log, win) ? 0 : 1;
}
