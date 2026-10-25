/*
 * Copyright (C) 2006  Mark J. Tilford
 * Copyright (C) 2021-2026  Petter Sjölund
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

#include "question-internal.hh"

using namespace std;

const string dir_names[] = {"north", "south", "east", "west", "northeast", "northwest", "southeast", "southwest", "up", "down", "out"};
const string short_dir_names[] = {"n", "s", "e", "w", "ne", "nw", "se", "sw", "u", "d", "out"};

const std::vector<verb_def> &builtin_verbs ()
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

QuestionRunner *QuestionRunner::get_runner (QuestionInterface *gi) {
  return new question_implementation (gi);
}

/* The rest of question_implementation is split by area into the other
 * question-*.cc units (vars, objects, rooms, session, parse, script,
 * functions, panes); question-internal.hh declares what they share. */

void question_implementation::tick_timers()
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
	  const QuestionBlock *gb = gf.find_by_name ("timer", tr.name);
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

/***************************************
 *                                     *
 *                                     *
 *                                     *
 * QuestionInterface related functions *
 *                                     *
 *                                     *
 *                                     *
 ***************************************/

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

/* Reads the formatting code whose bar is at s[i] into `style', leaving `i'
 * on the code's last character, and says what else the code asks of the
 * printer.  Shared by print_formatted and the style census
 * (question_style_scan), so the two cannot disagree about what a string
 * looks like.  FMT_LITERAL leaves `i' and `style' alone: the bar is not a
 * code after all and prints as itself. */
question_format_code
question_parse_format_code (const string &s, std::string::size_type &i,
			    QuestionFontStyle &style)
{
  /* Quest tries the two-character codes, then the one-character ones, then
   * |sNN, and if none matches it emits a literal "|" and carries on reading
   * from the character right after the bar, which is then ordinary text
   * (TextFormatter.cs:161-176).  This used to fall off the end of the
   * switch with the index already past the code character, so the loop's
   * own i++ ate it too: The Lazy Gun Cult's |K| sigil and |plaque| emphasis
   * both lost two characters a bar, which ran the surrounding words
   * together. */
  std::string::size_type k = i + 1;

  /* A bar with nothing after it is a code that matched nothing. */
  if (k == s.length())
    return FMT_LITERAL;

  /* The | codes are case-sensitive: Quest's TextFormatter switches on the
     raw one- and two-character code. */
  switch (s[k])
    {
    case 'u': style.is_underlined = true; break;
    case 'i': style.is_italic     = true; break;
    case 'b': style.is_bold       = true; break;
    case 'c':
      /* The clearing "|c" is the one that is not "|cb", "|cr", "|cl", "|cy"
       * or "|cg"; it is the bar and the "c" alone. */
      if (k + 1 < s.length())
	switch (s[k + 1])
	  {
	  case 'y': style.color = "#ffff00"; i = k + 1; return FMT_STYLE;
	  case 'g': style.color = "#00ff00"; i = k + 1; return FMT_STYLE;
	  case 'l': style.color = "#0000ff"; i = k + 1; return FMT_STYLE;
	  case 'r': style.color = "#ff0000"; i = k + 1; return FMT_STYLE;
	  case 'b': style.color = "";        i = k + 1; return FMT_STYLE;
	  }
      i = k;
      return FMT_CLEAR;

    case 's':
      {
	/* |sNN wants exactly two characters after the "s", and they have to
	 * parse as a number: Quest reads them with int.TryParse, under a
	 * guard that there are two of them (TextFormatter.cs:165-176).  A
	 * malformed one -- Uranus opens with a stray |s9| -- is not a code,
	 * and prints as itself.  Size 0 is the game's default size, which is
	 * what |s00 goes back to. */
	int newsize;
	if (k + 2 >= s.length ()
	    || !parse_two_char_int (s.substr (k + 1, 2), newsize))
	  return FMT_LITERAL;
	style.size = newsize > 0 ? newsize : 0;
	i = k + 2;
	return FMT_STYLE;
      }

    case 'j':
      /* Same as |sNN: "j" on its own is not one of Quest's one-character
       * codes, so a malformed |jX is a literal bar. */
      if (k + 1 == s.length())
	return FMT_LITERAL;
      if (s[k + 1] == 'l') style.justify = JUSTIFY_LEFT;
      else if (s[k + 1] == 'r') style.justify = JUSTIFY_RIGHT;
      else if (s[k + 1] == 'c') style.justify = JUSTIFY_CENTER;
      else
	return FMT_LITERAL;
      i = k + 1;
      return FMT_STYLE;

    case 'n':
      i = k;
      return FMT_NEWLINE;

    case 'w':
      i = k;
      return FMT_WAIT;

    case 'x':
      if (k + 1 == s.length())
	return FMT_LITERAL;
      if (s[k + 1] == 'b')
	style.is_bold = false;
      else if (s[k + 1] == 'u')
	style.is_underlined = false;
      else if (s[k + 1] == 'i')
	style.is_italic = false;
      else if (s[k + 1] == 'n' && k + 2 == s.length())
	{
	  i = k + 1;
	  return FMT_NO_NEWLINE;
	}
      else
	/* xb, xi and xu are the only two-character x codes; |xn is read off
	 * the end of the whole string before formatting starts
	 * (TextFormatter.cs:29-33), so one in the middle is not a code
	 * either. */
	return FMT_LITERAL;
      i = k + 1;
      return FMT_STYLE;

    default:
      QUESTION_DBG << "p_f: Fallthrough " << s[k] << std::endl;
      return FMT_LITERAL;
    }
  i = k;
  return FMT_STYLE;
}

QuestionResult QuestionInterface::print_formatted (const string &s, bool with_newline)
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
          switch (question_parse_format_code (s, i, cur_style))
            {
            case FMT_LITERAL:
	      /* The loop's i++ lands on the character after the bar, and
	       * the text branch prints it. */
	      print_normal ("|");
	      continue;

            case FMT_STYLE:
              break;

            case FMT_CLEAR:
	      /* The clearing "|c" flushes what has been printed so far
	       * before it wipes, exactly as "|w" does below
	       * (V4Game.Part2.cs:6761-6784). */
	      print_newline();
	      pending = false;
              clear_screen();
              break;

            case FMT_NEWLINE:
              print_newline();
              continue;

            case FMT_WAIT:
              /* The flush comes first and unconditionally: Quest's DoPrint runs
               * before DoWaitAsync, and it runs even with nothing accumulated,
               * so a string that opens with the code opens with a blank line. */
              print_newline();
              pending = false;
              wait_keypress("");
              /* A wait, and so a suspension: the turn's timers tick here
               * (see question_implementation::run_command). */
              if (runner != NULL)
                runner->turn_suspended();
              continue;

            case FMT_NO_NEWLINE:
              with_newline = false;
              continue;
            }
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

void QuestionInterface::set_default_font_size (const string &size)
{
  int n = parse_int (size);
  if (n > 0)
    default_size = n;
}

namespace {

/* What question_style_scan walks the game with.  Which strings reach the
 * screen is not knowable without running the game, so every <...> parameter
 * counts; the ones that never print are mostly plain names, which only add
 * to Normal's count, and Normal is always a style of its own. */
struct StyleScan
{
  const QuestionFile &gf;
  const QuestionStyleSink &sink;
  std::set<const QuestionBlock *> intro;

  void walk (const string &s, QuestionFontStyle &style, bool in_intro);
  void walk_params (const string &line, bool in_intro);
  void mark_intro (const string &type, const string &name,
		   std::vector<const QuestionBlock *> &todo);
};

}

void StyleScan::walk (const string &s, QuestionFontStyle &style, bool in_intro)
{
  std::string::size_type i, j;
  for (i = 0; i < s.length (); i ++)
    {
      if (s[i] == '|')
	{
	  if (question_parse_format_code (s, i, style) == FMT_LITERAL)
	    sink (style, "|", in_intro);
	  continue;
	}
      for (j = i; i < s.length () && s[i] != '|'; i ++)
	;
      sink (style, s.substr (j, i - j), in_intro);
      -- i;
    }
}

/* Each string starts from the plain style.  The printer's style carries over
 * from one string to the next, but a game that leaves a code open ends up
 * styling whatever happens to print next, which no census can predict. */
void StyleScan::walk_params (const string &line, bool in_intro)
{
  std::string::size_type open = 0, close;
  while ((open = line.find ('<', open)) != string::npos
	 && (close = line.find ('>', open + 1)) != string::npos)
    {
      QuestionFontStyle style;
      walk (line.substr (open + 1, close - open - 1), style, in_intro);
      open = close + 1;
    }
}

void StyleScan::mark_intro (const string &type, const string &name,
			    std::vector<const QuestionBlock *> &todo)
{
  const QuestionBlock *b = gf.find_by_name (type, name);
  if (b != NULL && intro.insert (b).second)
    todo.push_back (b);
}

bool question_style_scan (QuestionInterface *gi, const string &filename,
			  const QuestionStyleSink &sink, int &default_size)
{
  QuestionFile gf = read_question_file (gi, filename);
  default_size = 10;
  if (gf.blocks.size () == 0)
    return false;

  StyleScan scan { gf, sink, {} };
  std::string::size_type c1, c2;

  /* The introduction: the startscript, whatever procedures and text blocks
   * it reaches through `do' and `displaytext' (a braced startscript is one
   * of those, deinlined at load), and the <intro> text Quest shows after
   * it. */
  const QuestionBlock *game = gf.find_by_name ("game", "game");
  std::vector<const QuestionBlock *> todo;
  scan.mark_intro ("text", "intro", todo);
  if (game != NULL)
    for (const string &line: game->data)
      {
	string tok = first_token (line, c1, c2);
	if (ci_equal (tok, "default")
	    && ci_equal (next_token (line, c1, c2), "fontsize")
	    && is_param (tok = next_token (line, c1, c2)))
	  {
	    int n = parse_int (param_contents (tok));
	    if (n > 0)
	      default_size = n;
	  }
      }
  for (size_t k = 0; ; k ++)
    {
      const std::vector<string> *lines;
      if (k == 0)
	{
	  if (game == NULL)
	    continue;
	  lines = &game->data;
	}
      else if (k <= todo.size ())
	lines = &todo[k - 1]->data;
      else
	break;
      for (const string &line: *lines)
	{
	  if (k == 0 && !ci_equal (first_token (line, c1, c2), "startscript"))
	    continue;
	  for (const char *call: { "do <", "displaytext <" })
	    {
	      std::string::size_type at = 0;
	      while ((at = line.find (call, at)) != string::npos)
		{
		  std::string::size_type open = at + strlen (call) - 1;
		  std::string::size_type close = line.find ('>', open);
		  if (close == string::npos)
		    break;
		  if (at == 0 || isspace ((unsigned char) line[at - 1]))
		    scan.mark_intro (call[0] == 'd' && call[1] == 'o'
				     ? "procedure" : "text",
				     line.substr (open + 1, close - open - 1),
				     todo);
		  at = close;
		}
	    }
	}
    }

  for (const QuestionBlock &b: gf.blocks)
    {
      const bool in_intro = scan.intro.count (&b) != 0;
      if (b.blocktype == "synonyms")
	continue;
      if (b.blocktype == "text")
	{
	  /* displaytext prints the lines one at a time, and the style carries
	   * from each to the next. */
	  QuestionFontStyle style;
	  for (const string &line: b.data)
	    scan.walk (line, style, in_intro);
	  continue;
	}
      for (size_t n = 0; n < b.data.size (); n ++)
	{
	  bool line_intro = in_intro;
	  if (&b == game && n > 0)
	    line_intro = ci_equal (first_token (b.data[n], c1, c2),
				   "startscript");
	  if (&b == game && n == 0)
	    {
	      /* The game's name heads the banner set_game prints in italic. */
	      first_token (b.data[0], c1, c2);
	      next_token (b.data[0], c1, c2);
	      string tok = next_token (b.data[0], c1, c2);
	      if (is_param (tok))
		{
		  QuestionFontStyle style;
		  style.is_italic = true;
		  scan.walk (param_contents (tok), style, true);
		}
	      continue;
	    }
	  scan.walk_params (b.data[n], line_intro);
	}
    }
  return true;
}

void QuestionInterface::set_default_font (const string &font)
{
  default_font = font;
}

bool QuestionInterface::choose_yes_no (const string &question)
{
  vector<string> v;
  v.push_back ("Yes");
  v.push_back ("No");
  return (make_choice (question, v) == 0);
}
