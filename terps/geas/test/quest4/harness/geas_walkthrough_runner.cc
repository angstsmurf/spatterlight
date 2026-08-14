/*
  geas_walkthrough_runner -- headless walkthrough/regression runner for geas.

  Drives the geas core directly (no Glk) by unity-including the engine, the
  same trick GeasRegressionTests.mm uses.  It feeds a plain command script
  (one input per line) to run_command (); menu numbers and free-text answers
  are served from the *same* queue by make_choice ()/get_string (), so a
  script reads exactly like what a player would type.

  Beyond plain playback it can reproduce two things a real session relies on:

    --tick        Call tick_timers () once per turn, like the geasglk frontend
                  does after every line of input.  Required for games with
                  real-time/turn timers (e.g. World's End's dynamite fuse,
                  whose explosion reveals an item the rest of the game needs).

    --save-scum   On a turn that ends the game without the win marker (a
                  death), reload the pre-turn state and retry.  load_state ()
                  revives the game, so this "save-scums" random fights: each
                  lethal turn is retried with fresh RNG while damage already
                  dealt persists.

    --fight "c1|c2|...=marker"   Repeat the |-separated commands (cycled),
                  save-scumming, until `marker` appears in real output.  Use
                  for fights the script can't spell out turn-by-turn because
                  they are random (alternate vials at a cube; fire until a
                  boss dies).  Repeatable.

    --win "marker"   Success marker.  Sets the exit code (0 = seen) and the
                  reported WON state, and guards --save-scum from rewinding a
                  genuine winning turn.

    --echo        Echo the full transcript to stdout (otherwise only a short
                  summary, plus the tail, is printed).

  A script line of "[status]" is not passed to the game: it prints the status
  pane's current contents (get_status_vars ()) into the transcript instead.
    --seed N      RNG seed (overrides $GEAS_SEED).
    --max-reloads N   Per-turn save-scum reload cap (default 20000).

  Exit status: 0 if a win marker was given and seen, or if no win marker was
  given and the whole script ran; 1 otherwise.

  Build:  make            (in this directory)
  Run:    ./geas_walkthrough_runner [options] <game-file> <command-script>

  geas suppresses std::cerr during a run (see Logger in geas-util.cc), so set
  GEAS_LOGFILE=/path to capture this tool's traces if you add any.
*/

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../../../geas-util.cc"
#include "../../../istring.cc"
#include "../../../readfile.cc"
#include "../../../geasfile.cc"
#include "../../../geas-state.cc"

/* Portable rand(): the transcripts in ../goldens are byte-compared, and the
   engine's `rand` script function is the one thing in a replay that is not
   fixed by the game and the script.  A seeded C library rand() is reproducible
   on one machine but not across C libraries -- glibc's generator is nothing
   like the BSD one -- so 14 of the corpus games would produce a different
   transcript, and 5 of them a different *outcome*, on Linux.
   Substituting the generator the walkthroughs were derived against keeps every
   platform on that sequence.  This is Park-Miller/Lehmer, x' = 16807x mod
   2^31-1, seeded directly, which is exactly what macOS rand() computes: over
   200000 draws from each of nine seeds the two agree value for value.
   Only geas-runner.cc calls rand(), and nothing below this point uses it. */
namespace {
uint32_t g_rand_state = 1;
inline void geas_srand (unsigned s) { g_rand_state = (uint32_t) s; }
inline int geas_rand ()
{
  g_rand_state = (uint32_t) (((uint64_t) g_rand_state * 16807u) % 2147483647u);
  return (int) g_rand_state;
}
}
#define srand geas_srand
#define rand geas_rand

#include "../../../geas-runner.cc"

namespace {

/* Shared input stream: commands plus the menu/free-text answers they prompt.
   String helpers lcase () and trim () come from the geas core (geas-util.cc /
   readfile.cc); trim () strips all isspace, so it also drops CR from CRLF. */
std::deque<std::string> g_queue;

/* Thrown once the game has asked for input far more often than the script can
   answer.  A Quest game that validates its input ("How many players?" ... "Try
   again.") loops until it gets something it likes, and real Quest just blocks on
   the prompt; headless, an empty queue answers "" forever and the game spins,
   filling the transcript.  Bail out instead of running until the OS kills us. */
struct InputExhausted { };

std::string
dirname_of (const std::string &p)
{
  std::string::size_type s = p.find_last_of ('/');
  return s == std::string::npos ? std::string (".") : p.substr (0, s);
}

class RunnerInterface : public GeasInterface
{
public:
  std::string log;            /* full transcript, for marker detection */
  bool echo = false;
  /* Put text in the transcript from outside the engine, for the runner's own
     meta-commands (see "[status]"). */
  void emit (const std::string &s) { print_normal (s); }
  /* Consecutive answers served from an empty queue; see InputExhausted.  The cap
     is generous because a game may legitimately keep asking after the script
     ends (an end-of-game "press any key", a final menu) without looping. */
  int starved = 0;
  static const int kMaxStarved = 200;

protected:
  GeasResult print_normal (const std::string &s) override
  {
    log += s;
    if (echo)
      std::cout << s;
    return r_success;
  }

  GeasResult print_newline () override
  {
    log += "\n";
    if (echo)
      std::cout << "\n";
    return r_success;
  }

  void set_foreground (const std::string &) override { }
  void set_background (const std::string &) override { }
  /* Silent by default -- the engine's diagnostics would swamp a transcript --
   * but set GEAS_DEBUG=1 to see them on stderr when chasing a load problem. */
  void debug_print (const std::string &s) override
  {
    if (getenv ("GEAS_DEBUG"))
      std::cerr << "[geas] " << s << std::endl;
  }
  GeasResult wait_keypress (const std::string &) override { return r_success; }
  GeasResult pause (int) override { return r_success; }
  /* Inherit GeasInterface::clear_screen (), which emits a blank-line separator
   * in place of an actual screen wipe, so the transcript reflects it. */

  std::string get_string () override
  {
    if (g_queue.empty ())
      {
	if (++starved > kMaxStarved)
	  throw InputExhausted ();
	return "";
      }
    starved = 0;
    std::string s = g_queue.front ();
    g_queue.pop_front ();
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
    if (g_queue.empty ())
      {
	if (++starved > kMaxStarved)
	  throw InputExhausted ();
      }
    else
      {
	starved = 0;
	std::string s = g_queue.front ();
	g_queue.pop_front ();
	emit ("[choice] " + s + "\n");
	c = atoi (s.c_str ());
      }
    if (c < 1)
      c = 1;
    if ((size_t) c > choices.size ())
      c = (int) choices.size ();
    return (uint) c - 1;
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
    std::ifstream f (fn.c_str (), std::ios::binary);
    if (!f)
      {
	std::cerr << "[runner] cannot open " << fn << "\n";
	return "";
      }
    std::ostringstream ss;
    ss << f.rdbuf ();
    return ss.str ();
  }
};

RunnerInterface *gi = nullptr;
GeasRunner *gr = nullptr;
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

/* One turn: run the command, then (optionally) tick timers once. */
void
runturn (const std::string &cmd)
{
  gr->run_command (cmd);
  if (opt_tick && gr->is_running ())
    gr->tick_timers ();
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
  bool opt_scum = false, opt_echo = false;
  int seed = getenv ("GEAS_SEED") ? atoi (getenv ("GEAS_SEED")) : 0;
  bool have_seed = getenv ("GEAS_SEED");
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
		       "          [--max-reloads N] [--win MARKER]"
		       " [--fight \"c1|c2=MARKER\"]...\n"
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
      setenv ("GEAS_SEED", buf, 1);
    }

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
  gr = GeasRunner::get_runner (gi);   /* may consume a leading name/answer */
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
