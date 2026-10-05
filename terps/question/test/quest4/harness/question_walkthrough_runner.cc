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

  Drives the Question core directly (no Glk) by unity-including the engine, the
  same trick QuestionRegressionTests.mm uses.  It feeds a plain command script
  (one input per line) to run_command (); menu numbers and free-text answers
  are served from the *same* queue by make_choice ()/get_string (), so a
  script reads exactly like what a player would type.

  Beyond plain playback it can reproduce two things a real session relies on:

    --tick        Tick the timers once per turn -- Quest's SendCommand with an
                  elapsedTime of one second, which the engine places at the
                  turn's first `wait`/`pause` or at its end.  A real session
                  ticks on the wall clock instead; one tick a turn is what
                  makes a transcript reproducible.  Required for games with
                  real-time/turn timers (e.g. World's End's dynamite fuse,
                  whose explosion reveals an item the rest of the game needs).

    --save-scum   On a turn that ends the game without the win marker (a
                  death), reload the pre-turn state and retry.  load_state ()
                  revives the game, so this "save-scums" random fights: each
                  lethal turn is retried with fresh RNG while damage already
                  dealt persists.

    --fight "c1|c2|...=marker"   Repeat the |-separated commands (cycled),
                  save-scumming, until `marker` appears in real output.  Use
                  for exploring a random fight before linearising it
                  (alternate vials at a cube; fire until a boss dies).
                  Repeatable.  No committed walkthrough uses this or
                  --save-scum any more: under a fixed seed the draw stream is
                  known in advance, so even World's End's two coin-flip
                  fights are spelled out turn for turn (see its script's
                  header), which is what lets its transcript be diffed and
                  oracle-compared like every other game's.

    --win "marker"   Success marker.  Sets the exit code (0 = seen) and the
                  reported WON state, and guards --save-scum from rewinding a
                  genuine winning turn.

    --echo        Echo the full transcript to stdout (otherwise only a short
                  summary, plus the tail, is printed).

  A script line of "[status]" is not passed to the game: it prints the status
  pane's current contents (get_status_vars ()) into the transcript instead.
  "[save]" snapshots the game as a player's SAVE would, and "[restore]" loads
  that snapshot back as a player's RESTORE would (beforesave/onload and all);
  "[restore foreign]" restores it with the current room renamed, as a save
  another game wrote would have it.
    --seed N      RNG seed (overrides $QUESTION_SEED).
    --max-reloads N   Per-turn save-scum reload cap (default 20000).
    --lenient-names   Keep the engine's player-facing name leniency (accepting
                  a declared name like `boat2' when it points at exactly one
                  object).  By default this tool sets QUESTION_STRICT_NAMES and
                  gets Quest's alias-only matching instead: a walkthrough
                  derived here has to replay in a real Quest, so it must never
                  lean on a noun Quest would refuse.  Playing interactively is
                  the opposite trade, which is why lenient is the engine's own
                  default and strict is this tool's.

  Exit status: 0 if a win marker was given and seen, or if no win marker was
  given and the whole script ran; 1 otherwise.

  Build:  make            (in this directory)
  Run:    ./question_walkthrough_runner [options] <game-file> <command-script>

  Question suppresses std::cerr during a run (see Logger in question-util.cc), so set
  QUESTION_LOGFILE=/path to capture this tool's traces if you add any.
*/

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "headless_common.hh"

namespace {

/* The input queue (g_queue) and the interface this one builds on come from
   headless_common.hh.  String helpers lcase () and trim () come from the
   Question core (question-util.cc / readfile.cc); trim () strips all isspace,
   so it also drops CR from CRLF. */

/* The runner's one save slot, for "[save]" / "[restore]". */
std::string g_saved;

class RunnerInterface : public HeadlessInterface
{
public:
  std::string log;            /* full transcript, for marker detection */
  bool echo = false;
  /* Put text in the transcript from outside the engine, for the runner's own
     meta-commands (see "[status]"). */
  void emit (const std::string &s) { print_normal (s); }

protected:
  QuestionResult print_normal (const std::string &s) override
  {
    log += s;
    if (echo)
      std::cout << s;
    return r_success;
  }

  QuestionResult print_newline () override
  {
    log += "\n";
    if (echo)
      std::cout << "\n";
    return r_success;
  }

  /* Silent by default -- the engine's diagnostics would swamp a transcript --
   * but set QUESTION_DEBUG=1 to see them on stderr when chasing a load problem. */
  void debug_print (const std::string &s) override
  {
    if (getenv ("QUESTION_DEBUG"))
      std::cerr << "[question] " << s << std::endl;
  }
  /* There is no key to press here, but the message that asks for one is real
   * output: the Glk frontend prints it (QuestionGlkInterface::wait_keypress,
   * questionglk.cc:1135-1141) and so does Quest, and a `wait <...>` whose message
   * runs to several lines through |n hid all of them from the transcript. */
  QuestionResult wait_keypress (const std::string &msg) override
  {
    if (!msg.empty ())
      print_formatted (msg);
    return r_success;
  }
  QuestionResult pause (int) override { return r_success; }
  /* Inherit QuestionInterface::clear_screen (), which emits a blank-line separator
   * in place of an actual screen wipe, so the transcript reflects it. */

  std::string get_string () override
  {
    std::string s;
    if (!next_input (s))
      return "";
    log += "[input] " + s + "\n";
    return s;
  }

  uint make_choice (const std::string &info,
		    std::vector<std::string> choices) override
  {
    /* Echo the menu, not just log it: a `choose` that prints nothing makes the
     * turn that triggered it look like a command with no output, and the choice
     * numbers are what a script has to supply -- the answer is read with atoi,
     * so "1" and not "left".  Numbering them here is what makes that visible. */
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
    return clamp_choice (c, choices.size ());
  }

  std::string get_file (const std::string &fn) const override
  {
    std::string s;
    if (!read_whole_file (fn, s))
      std::cerr << "[runner] cannot open " << fn << "\n";
    return s;
  }
};

RunnerInterface *gi = nullptr;
QuestionRunner *gr = nullptr;
bool opt_tick = false;
int opt_max_reloads = 20000;

/* Has `marker` appeared in real game output (not an echoed "> cmd" line)?
   Scanned incrementally per marker so save-scum retries stay O(total output). */
bool
seen (const std::string &marker)
{
  static std::map<std::string, std::pair<size_t, bool> > st;
  if (marker.empty ())
    return false;
  std::pair<size_t, bool> &e = st[marker];
  if (e.second)
    return true;
  std::string m = lcase (marker);
  while (e.first < gi->log.size ())
    {
      size_t nl = gi->log.find ('\n', e.first);
      if (nl == std::string::npos)
	break;                  /* only scan complete lines */
      std::string ln = gi->log.substr (e.first, nl - e.first);
      if (ln.rfind ("> ", 0) != 0 && lcase (ln).find (m) != std::string::npos)
	{
	  e.second = true;
	  return true;
	}
      e.first = nl + 1;
    }
  return e.second;
}

/* One turn: run the command, (optionally) ticking the timers once as part of
   it.  The engine places that tick where Quest's SendCommand does -- at the
   turn's first `wait`/`pause` if it has one, at its end otherwise. */
void
runturn (const std::string &cmd)
{
  gr->run_command (cmd, opt_tick);
}

/* Run one turn; if it ends the game without `win_marker` (a death, in either
   the command or the timer tick), reload the pre-turn state and retry. */
void
scummed (const std::string &cmd, const std::string &win_marker, bool do_scum)
{
  /* Transparent snapshots (run_hooks=false): these are checkpoints, not player
     saves, so they must not fire the game's beforesave/onload scripts (e.g.
     World's End's onload gotos the saved room, which would reset a fight). */
  /* Snapshotting is only needed to rewind a fatal turn when save-scumming.
     Skipping it otherwise avoids a full state serialization (and command-queue
     copy) on every single turn. */
  if (!do_scum || win_marker.empty ())
    {
      runturn (cmd);
      return;
    }
  std::string snap = gr->save_state (false);
  std::deque<std::string> q = g_queue;
  runturn (cmd);
  for (int r = 0; !gr->is_running () && !seen (win_marker)
		  && r < opt_max_reloads; r++)
    {
      gr->load_state (snap, false);
      g_queue = q;
      runturn (cmd);
    }
}

struct Fight
{
  std::vector<std::string> cmds;
  std::string marker;
};

/* Repeat a fight's commands (cycled), save-scumming, until its marker appears. */
bool
run_fight (const Fight &f)
{
  for (int i = 0; i < 400000 && !seen (f.marker); i++)
    scummed (f.cmds[i % f.cmds.size ()], f.marker, true);
  return seen (f.marker);
}

}  /* namespace */

int
main (int argc, char **argv)
{
  std::vector<Fight> fights;
  std::string win_marker;
  bool opt_scum = false, opt_echo = false, opt_lenient = false;
  int seed = getenv ("QUESTION_SEED") ? atoi (getenv ("QUESTION_SEED")) : 0;
  bool have_seed = getenv ("QUESTION_SEED");
  std::vector<std::string> pos;

  for (int i = 1; i < argc; i++)
    {
      std::string a = argv[i];
      auto need = [&] (const char *n) -> std::string {
	if (i + 1 >= argc)
	  {
	    std::cerr << "missing value for " << n << "\n";
	    exit (2);
	  }
	return argv[++i];
      };
      if (a == "--tick")
	opt_tick = true;
      else if (a == "--save-scum")
	opt_scum = true;
      else if (a == "--echo")
	opt_echo = true;
      else if (a == "--lenient-names")
	opt_lenient = true;
      else if (a == "--seed")
	{
	  seed = atoi (need ("--seed").c_str ());
	  have_seed = true;
	}
      else if (a == "--max-reloads")
	opt_max_reloads = atoi (need ("--max-reloads").c_str ());
      else if (a == "--win")
	win_marker = need ("--win");
      else if (a == "--fight")
	{
	  std::string spec = need ("--fight");
	  std::string::size_type eq = spec.rfind ('=');
	  if (eq == std::string::npos)
	    {
	      std::cerr << "--fight needs CMDS=MARKER\n";
	      return 2;
	    }
	  Fight f;
	  f.marker = trim (spec.substr (eq + 1));
	  std::istringstream is (spec.substr (0, eq));
	  std::string tok;
	  while (std::getline (is, tok, '|'))
	    {
	      tok = trim (tok);
	      if (!tok.empty ())
		f.cmds.push_back (tok);
	    }
	  if (f.cmds.empty () || f.marker.empty ())
	    {
	      std::cerr << "--fight needs CMDS=MARKER\n";
	      return 2;
	    }
	  fights.push_back (f);
	}
      else if (a == "-h" || a == "--help")
	{
	  std::cout << "usage: " << argv[0]
		    << " [--tick] [--save-scum] [--echo] [--seed N]\n"
		       "          [--lenient-names] [--max-reloads N]"
		       " [--win MARKER]\n"
		       "          [--fight \"c1|c2=MARKER\"]...\n"
		       "          <game-file> <command-script>\n";
	  return 0;
	}
      else if (a.rfind ("--", 0) == 0)
	{
	  std::cerr << "unknown option " << a << "\n";
	  return 2;
	}
      else
	pos.push_back (a);
    }

  if (pos.size () < 2)
    {
      std::cerr << "usage: " << argv[0]
		<< " [options] <game-file> <command-script>\n";
      return 2;
    }
  const std::string game = pos[0], script = pos[1];

  if (have_seed)
    {
      char buf[32];
      snprintf (buf, sizeof buf, "%d", seed);
      setenv ("QUESTION_SEED", buf, 1);
    }

  /* Quest's alias-only noun matching, unless --lenient-names asked for the
     engine's player-facing default (see lenient_names_ in question-impl.hh).
     The flag wins over an inherited environment either way. */
  if (opt_lenient)
    unsetenv ("QUESTION_STRICT_NAMES");
  else
    setenv ("QUESTION_STRICT_NAMES", "1", 1);

  /* Read the script.  A "raw" script is one clean input per line; a prose
     walkthrough may carry a header (skipped up to a "Start:" line, if any)
     and trailing "  (comment)" notes on command lines (stripped).  A raw
     script has neither, so both steps are no-ops for it. */
  std::ifstream sf (script.c_str ());
  if (!sf)
    {
      std::cerr << "cannot open script " << script << "\n";
      return 2;
    }
  std::vector<std::string> raw;
  std::string line;
  while (std::getline (sf, line))
    raw.push_back (line);

  size_t begin = 0;
  for (size_t i = 0; i < raw.size (); i++)
    if (lcase (trim (raw[i])).rfind ("start:", 0) == 0)
      {
	begin = i + 1;
	break;
      }
  for (size_t i = begin; i < raw.size (); i++)
    {
      std::string t = trim (raw[i]);
      if (t.empty ())
	continue;
      /* Strip a trailing "  (note)" walkthrough annotation.  Require TWO+ spaces
	 before the "(" (the alignment-padding convention): a single space can be
	 part of a real token, e.g. the Quest place "cottage (inside)" in sirloin3,
	 which "go to" must match exactly. */
      std::string::size_type p = t.find ("  (");
      if (p != std::string::npos)
	t = trim (t.substr (0, p));
      if (!t.empty ())
	g_queue.push_back (t);
    }

  gi = new RunnerInterface ();
  gi->echo = opt_echo;
  gr = QuestionRunner::get_runner (gi);   /* may consume a leading name/answer */
  bool exhausted = false;
  try
    {
      gr->set_game (game);
    }
  catch (const InputExhausted &)
    {
      exhausted = true;
    }
  if (!exhausted && !gr->is_running ())
    {
      std::cerr << "[runner] game failed to load\n";
      return 3;
    }

  /* Find the fight (if any) whose command list contains `c`. */
  auto find_fight = [&] (const std::string &c) -> const Fight * {
    for (const Fight &f : fights)
      for (const std::string &fc : f.cmds)
	if (fc == c)
	  return &f;
    return nullptr;
  };

  long turns = 0;
  try
    {
  while (!exhausted && gr->is_running () && !g_queue.empty ())
    {
      std::string c = g_queue.front ();
      g_queue.pop_front ();
      /* "[status]" is a runner meta-command, not a game command: it writes what
	 a frontend would be showing in the status pane into the transcript, so a
	 fixture can check it.  A real frontend refreshes that pane after every
	 turn; asking for it explicitly keeps it out of the transcripts that do
	 not care about it. */
      if (lcase (c) == "[status]")
	{
	  gi->emit ("[status]\n");
	  for (const std::string &sv : gr->get_status_vars ())
	    gi->emit (sv + "\n");
	  continue;
	}
      if (lcase (c) == "[save]")
	{
	  g_saved = gr->save_state ();
	  gi->emit ("\n[save]\n");
	  continue;
	}
      if (lcase (c) == "[restore]" || lcase (c) == "[restore foreign]")
	{
	  std::string data = g_saved;
	  if (lcase (c) != "[restore]")
	    {
	      std::string name;
	      QuestionState st;
	      if (deserialize_game (data, name, st))
		{
		  for (ObjectRecord &o: st.objs)
		    if (o.name == st.location)
		      o.name = "Elsewhere";
		  st.location = "Elsewhere";
		  data = serialize_game (name, st);
		}
	    }
	  gi->emit ("\n" + lcase (c) + "\n");
	  gi->emit (gr->load_state (data) ? "[restored]\n" : "[refused]\n");
	  continue;
	}
      if (const Fight *f = find_fight (c))
	{
	  run_fight (*f);
	  /* Consume any further scripted lines belonging to this fight. */
	  while (!g_queue.empty ())
	    {
	      bool in = false;
	      for (const std::string &fc : f->cmds)
		if (g_queue.front () == fc)
		  {
		    in = true;
		    break;
		  }
	      if (!in)
		break;
	      g_queue.pop_front ();
	    }
	  turns++;
	  continue;
	}
      scummed (c, win_marker, opt_scum);
      turns++;
    }
    }
  catch (const InputExhausted &)
    {
      exhausted = true;
    }
  if (exhausted)
    std::cerr << "[runner] input exhausted: the game kept asking for input the"
		 " script cannot answer\n";

  bool won = !win_marker.empty () && seen (win_marker);
  std::string tail = gi->log.size () > 1400
		     ? gi->log.substr (gi->log.size () - 1400) : gi->log;
  if (!opt_echo)
    std::cout << "--- transcript tail ---\n" << tail << "\n";
  std::cout << "[runner] turns=" << turns << " leftover=" << g_queue.size ()
	    << " running=" << (gr->is_running () ? "true" : "false");
  if (!win_marker.empty ())
    std::cout << " WON=" << (won ? "YES" : "no");
  std::cout << "\n";

  int rc = exhausted ? 1
	   : win_marker.empty () ? (g_queue.empty () ? 0 : 1) : (won ? 0 : 1);
  delete gr;
  delete gi;
  return rc;
}
