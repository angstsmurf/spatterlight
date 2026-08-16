/***************************************************************************
 *                                                                         *
 * Copyright (C) 2006 by Mark J. Tilford                                   *
 *                                                                         *
 * This file is part of Geas.                                              *
 *                                                                         *
 * Geas is free software; you can redistribute it and/or modify            *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 2 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * Geas is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with Geas; if not, write to the Free Software                     *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *                                                                         *
 ***************************************************************************/

#include "GeasRunner.hh"
#include "readfile.hh"
#include "geas-state.hh"
#include "geas-util.hh"
#include <set>
#include <unordered_map>
#include "geas-impl.hh"
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "general.hh"
#include "istring.hh"

/* Use the shared erkyrath_random() RNG (xoshiro128** when seeded, native
   otherwise), like scott/comprehend/plus/taylor.  The headless walkthrough
   runner links common_utils/randomness.c too, so a seeded run draws the same
   numbers there as in the app -- and, xoshiro128** being a fixed algorithm,
   the same numbers on any platform.  That is what lets the corpus transcripts
   in test/quest4/goldens be diffed at all. */
extern "C" {
#include "randomness.h"
}
#ifdef SPATTERLIGHT
extern "C" int gli_determinism;
#endif

class GeasInterface;

using namespace std;

/* Bumps a nesting counter for as long as it is alive, so every early `return`
   out of run_script still unwinds it.  See kMaxScriptDepth. */
namespace {
  struct ScriptDepth {
    int &d;
    explicit ScriptDepth (int &depth) : d (depth) { ++ d; }
    ~ScriptDepth () { -- d; }
    ScriptDepth (const ScriptDepth &) = delete;
    ScriptDepth &operator= (const ScriptDepth &) = delete;
  };
}

static const string dir_names[] = {"north", "south", "east", "west", "northeast", "northwest", "southeast", "southwest", "up", "down", "out"};
static const string short_dir_names[] = {"n", "s", "e", "w", "ne", "nw", "se", "sw", "u", "d", "out"};

/* The verbs the engine dispatches itself, beyond the universal ones.
 * `key` is the action/property name a game file stores; `phrases` are the
 * surface forms a player may type for it; `use_default` marks a verb that falls
 * back to the object's anonymous default action when nothing else handles it.
 *
 * Shared by the two places that must agree about them: try_match, which
 * dispatches a typed command, and object_verbs, which lists the verbs an object
 * responds to.  They used to keep separate tables, so a verb added to one was
 * silently missing from the other. */
struct verb_def { const char *key; std::vector<const char *> phrases; bool use_default; };

static const std::vector<verb_def> &builtin_verbs ()
{
  static const std::vector<verb_def> table =
    {
      { "open",       { "open" },                                   true  },
      { "close",      { "close", "shut" },                          false },
      { "move",       { "move", "push", "pull", "slide", "shove" }, false },
      { "eat",        { "eat", "chew", "taste", "bite" },           false },
      { "drink",      { "drink", "sip" },                           false },
      { "smell",      { "smell", "sniff" },                         false },
      { "touch",      { "touch", "feel", "rub" },                   false },
      { "listen to",  { "listen to", "listen" },                    false },
      { "look under", { "look under", "look beneath" },             false },
      { "look in",    { "look in", "look inside", "search" },       false },
      { "sit on",     { "sit on", "sit in", "sit" },                false },
      { "hit",        { "hit", "kick", "punch", "break", "smash" }, false },
      { "kiss",       { "kiss" },                                   false },
      { "burn",       { "burn" },                                   false },
      { "kill",       { "kill" },                                   false },
      { "wear",       { "wear", "put on", "don" },                  false },
      { "turn on",    { "turn on", "switch on" },                   false },
      { "turn off",   { "turn off", "switch off" },                 false },
    };
  return table;
}

GeasRunner *GeasRunner::get_runner (GeasInterface *gi) {
  return new geas_implementation (gi);
}

/* The rest of geas_implementation, split by area into single-TU sections (the
 * same layout as quest5's aslx-*.inc units).  Each file's header says what it
 * holds; the order is the order the code sat in when this was one file. */
#include "geas-vars.inc"
#include "geas-objects.inc"
#include "geas-rooms.inc"
#include "geas-state.inc"
#include "geas-parse.inc"
#include "geas-script.inc"
#include "geas-functions.inc"
#include "geas-panes.inc"

void geas_implementation::tick_timers()
{
  if (!state.running)
    return;

  /* Quest's Tick (V4Game.Part2.cs:91-129) walks the timers collecting the
   * actions that came due into a list, and only runs them once the walk is
   * over.  That ordering matters: a timer whose action does timeron/timeroff
   * cannot change whether the timers after it in the list count down on this
   * same tick, and a timer already collected still runs even if a later
   * action turns it off.  So collect first, run afterwards. */
  std::vector<string> due;

  for (size_t i = 0; i < state.timers.size(); i ++)
    {
      TimerRecord &tr = state.timers[i];
      if (!tr.is_running)
	continue;
      if (tr.bypass)
	{
	  /* Spent, not counted -- the turn a timer is switched on or off does
	   * not advance it (SetTimerState, V4Game.Part2.cs:561-574). */
	  tr.bypass = false;
	  continue;
	}
      tr.elapsed ++;
      if (tr.elapsed >= tr.interval)
	{
	  /* Quest timers repeat every `interval` ticks until explicitly
	   * timeroff'd (a one-shot timer calls timeroff in its own action,
	   * directly or via a variable onchange).  Zero the count and keep
	   * running rather than stopping after the first firing -- otherwise
	   * counter timers (interval 1, action `dec <x>`) only fire once.
	   * An interval of 0 lands here every tick, as in Quest, where
	   * TimerTicks >= 0 is true immediately.
	   *
	   * The count is zeroed *before* the action runs, and the comparison
	   * above reads the interval as it stands on the tick, which is what
	   * lets `set interval' reach the cycle already in flight -- from
	   * inside the timer's own action and from outside it alike
	   * (V4Game.Part2.cs:126-131). */
	  tr.elapsed = 0;
	  const GeasBlock *gb = gf.find_by_name ("timer", tr.name);
	  if (gb != NULL)
	    {
	      std::string::size_type c1, c2;
	      string action;
	      bool have_action = false;
	      for (const auto &line: gb->data)
		{
		  string tok = first_token (line, c1, c2);
		  /* CI: BeginsWith "action " -- and the LAST action line wins,
		   * since SetUpTimers overwrites TimerAction on each one
		   * (V4Game.Part2.cs:1337-1340). */
		  if (ci_equal (tok, "action"))
		    {
		      action = line.substr (c2);
		      have_action = true;
		    }
		}
	      if (have_action)
		due.push_back (action);
	    }
	}
    }

  for (const auto &script: due)
    run_script (script);
}

/***********************************
 *                                 *
 *                                 *
 *                                 *
 * GeasInterface related functions *
 *                                 *
 *                                 *
 *                                 *
 ***********************************/

/* The size in a |sNN code: Quest reads the two characters after the "s" with
 * int.TryParse (TextFormatter.cs:170), which takes an optional sign and then
 * digits, and nothing else -- so "09" and "-1" are sizes and "9|" is not.  The
 * old code parsed "|s" plus the first digit instead of the two digits, so it
 * never read a size at all; nothing in a transcript shows the difference,
 * since the size lands in a Glk style rather than in the text.
 *
 * int.TryParse's default is NumberStyles.Integer, which also skips whitespace
 * at either end, and that half is visible: ChristmaKwanzakkah's title banner
 * ends `|cr! |s0 |cb |xb', whose two characters after the "s" are "0" and a
 * space.  Quest reads that as a size and eats the code; a stricter reader
 * calls it malformed and prints a literal "|s0" in the middle of the game's
 * first line (finding 71). */
static bool parse_two_char_int (const string &s, int &rv)
{
  size_t b = 0, e = s.length ();
  while (b < e && isspace ((unsigned char) s[b]))
    b ++;
  while (e > b && isspace ((unsigned char) s[e - 1]))
    e --;
  string t = s.substr (b, e - b);
  if (t.empty ())
    return false;
  size_t d = (t[0] == '-' || t[0] == '+') ? 1 : 0;
  for (size_t k = d; k < t.length (); k ++)
    if (!(t[k] >= '0' && t[k] <= '9'))
      return false;
  if (d == t.length ())
    return false;
  rv = parse_int (t.substr (d));
  if (t[0] == '-')
    rv = -rv;
  return true;
}

GeasResult GeasInterface::print_formatted (const string &s, bool with_newline)
{
  std::string::size_type i, j;

  /* Quest prints a string in pieces rather than all at once: Print walks it a
   * character at a time, accumulating into `printString', and a "|w" or a
   * clearing "|c" hands what has accumulated to DoPrint and starts a fresh
   * string (V4Game.Part2.cs:6745-6784).  Every DoPrint ends its line, so the
   * codes are paragraph breaks as well as pauses -- the text on either side of
   * one is never on the same line.  The closing DoPrint is the only guarded
   * one, `If printString <> "" ' (ibid. 6794-6797), so a string that ends with
   * one of the two codes has already had its last line ended and does not get
   * another.  That is what `pending' is: printString <> "".  It starts true so
   * that Print's own empty-string case, a bare DoPrint("") (ibid. 6742-6745),
   * still ends a line here. */
  bool pending = true;

  for (i = 0; i < s.length(); i ++)
    {
      /* Anything but the two flushing codes below is a character appended to
       * printString -- the formatting codes among them included, since Quest
       * hands those to DoPrint too and lets TextFormatter eat them there. */
      pending = true;
      if (s[i] == '|')
        {
          // changed indicated whether cur_style has been changed
          // and update_style should be called at the end.
          // it is true unless cleared (by |n or |w).
          bool changed = true;
	  /* Set by any arm that turns out not to be a code after all.  Quest
	   * tries the two-character codes, then the one-character ones, then
	   * |sNN, and if none matches it emits a literal "|" and carries on
	   * reading from the character right after the bar, which is then
	   * ordinary text (TextFormatter.cs:161-176).  This used to fall off
	   * the end of the switch with the index already past the code
	   * character, so the loop's own i++ ate it too: The Lazy Gun Cult's
	   * |K| sigil and |plaque| emphasis both lost two characters a bar,
	   * which ran the surrounding words together. */
	  bool literal_bar = false;
          j = i;
          i ++;
          if (i == s.length())
            {
	      /* A bar with nothing after it is a code that matched nothing. */
	      print_normal ("|");
              continue;
            }

	  /* The | codes are case-sensitive: Quest's TextFormatter switches on
	     the raw one- and two-character code. */
          switch (s[i])
            {
            case 'u': cur_style.is_underlined = true; break;
            case 'i': cur_style.is_italic     = true; break;
            case 'b': cur_style.is_bold       = true; break;
            case 'c':
              i ++;

	      /* The clearing "|c" -- the one that is not "|cb", "|cr", "|cl",
	       * "|cy" or "|cg" -- flushes what has been printed so far before it
	       * wipes, exactly as "|w" does above (V4Game.Part2.cs:6761-6784). */
              if (i == s.length())
		{ print_newline(); pending = false; clear_screen(); break; }

              switch (s[i])
                {
                case 'y': cur_style.color = "#ffff00"; break;
                case 'g': cur_style.color = "#00ff00"; break;
                case 'l': cur_style.color = "#0000ff"; break;
                case 'r': cur_style.color = "#ff0000"; break;
                case 'b': cur_style.color = "";  break;

                default:
		  print_newline();
		  pending = false;
                  clear_screen();
                  --i;
                }
              break;

            case 's':
	      {
		/* |sNN wants exactly two characters after the "s", and they
		 * have to parse as a number: Quest reads them with
		 * int.TryParse, under a guard that there are two of them
		 * (TextFormatter.cs:165-176).  A malformed one -- Uranus opens
		 * with a stray |s9| -- is not a code, and prints as itself. */
		int newsize;
		if (i + 2 >= s.length ()
		    || !parse_two_char_int (s.substr (i + 1, 2), newsize))
		  {
		    literal_bar = true;
		    break;
		  }
		i += 2;
		if (newsize > 0)
		  cur_style.size = newsize;
		else
		  cur_style.size = default_size;
	      }
	      break;

            case 'j':
              i ++;

              if (i == s.length() ||
                  !(s[i] == 'l' || s[i] == 'c' || s[i] == 'r'))
                {
		  /* Same as |sNN: "j" on its own is not one of Quest's
		   * one-character codes, so a malformed |jX is a literal
		   * bar. */
		  literal_bar = true;
		  break;
		}
              if (s[i] == 'l') cur_style.justify = JUSTIFY_LEFT;
              else if (s[i] == 'r') cur_style.justify = JUSTIFY_RIGHT;
              else if (s[i] == 'c') cur_style.justify = JUSTIFY_CENTER;
              break;

            case 'n':
              print_newline();
              changed = false;
              break;

            case 'w':
              /* The flush comes first and unconditionally: Quest's DoPrint runs
               * before DoWaitAsync, and it runs even with nothing accumulated,
               * so a string that opens with the code opens with a blank line. */
              print_newline();
              pending = false;
              wait_keypress("");
              /* A wait, and so a suspension: the turn's timers tick here
               * (see geas_implementation::run_command). */
              if (runner != NULL)
                runner->turn_suspended();
              changed = false;
              break;

            case 'x':
              i ++;

	      if (i == s.length())
		literal_bar = true;
              else if (s[i] == 'b')
                cur_style.is_bold = false;
              else if (s[i] == 'u')
                cur_style.is_underlined = false;
              else if (s[i] == 'i')
                cur_style.is_italic = false;
              else if (s[i] == 'n' && i + 1 == s.length())
                changed = with_newline = false;
	      else
		/* xb, xi and xu are the only two-character x codes; |xn is
		 * read off the end of the whole string before formatting
		 * starts (TextFormatter.cs:29-33), so one in the middle is not
		 * a code either. */
		literal_bar = true;
              break;

            default:
              GEAS_DBG << "p_f: Fallthrough " << s[i] << std::endl;
              changed = false;
	      literal_bar = true;
            }
	  if (literal_bar)
	    {
	      print_normal ("|");
	      /* Resume at the bar, so the loop's i++ lands on the character
	       * after it and the text branch prints it. */
	      i = j;
	      continue;
	    }
          if (changed)
            update_style();
        }
      else
        {
          for (j = i; i != s.length() && s[i] != '|'; i ++)
            ;
          print_normal (s.substr (j, i-j));
          if (s[i] == '|')
            -- i;
        }
    }
  if (with_newline && pending)
    print_newline();
  return r_success;
}

void GeasInterface::set_default_font_size (const string &size)
{
  default_size = parse_int (size);
}

void GeasInterface::set_default_font (const string &font)
{
  default_font = font;
}

bool GeasInterface::choose_yes_no (const string &question)
{
  vector<string> v;
  v.push_back ("Yes");
  v.push_back ("No");
  return (make_choice (question, v) == 0);
}
