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


/* question-script-output.cc -- the statement handlers run_script dispatches to for
 * what the player sees and hears: msg, say, pictures, sounds, colours, and the
 * statements Question recognises only to ignore.
 *
 * Each takes the statement line (s), the enclosing function's result (rv),
 * the lower-cased keyword (tok) and the keyword's token bounds in s (c1, c2),
 * and returns true once the line is dealt with -- see run_script in
 * question-script.cc. */

#include "question-internal.hh"

using namespace std;

/* font, helpclear, helpclose, mailto, modvolume, msgto, panes, shell, shellexe, with */
bool question_implementation::exec_ignored (const string &, string &,
					    string, std::string::size_type,
					    std::string::size_type)
{
  /* Recognised statements with nothing to do here -- not "unrecognised
   * script".  helpclear/helpclose really do nothing in Quest 5 either
   * (V4Game.Part2.cs:5782, 5988); mailto/modvolume/shell/shellexe drive
   * host facilities Question does not provide; msgto and with are multiplayer
   * (QNSO); font and panes set up the Quest window, which a Glk text
   * frontend lays out itself, so they are ignored on purpose. */
  return true;
}

/* animate */
bool question_implementation::exec_animate (const string &s, string &,
					    string tok, std::string::size_type c1,
					    std::string::size_type c2)
{
  /* `animate <file>` and `animate persist <file>` are pictures: both go to
   * ShowPicture, at every ASL version and with no gate of their own
   * (V4Game.Part2.cs:5850-5858), animation being a property of the file
   * rather than of the statement.  `persist' asked Quest to leave the
   * window open; there is no window here.  GetParameter reads from the
   * line's first '<' and never looks at what comes before it, so skipping
   * to that is all `persist' needs. */
  std::string::size_type lt = s.find ('<', c2);
  if (lt != string::npos)
    c2 = lt;
  tok = next_token (s, c1, c2);
  if (is_param (tok))
    show_picture (eval_param (tok), true);
  return true;
}

/* background, foreground */
bool question_implementation::exec_colour (const string &s, string &,
					   string tok, std::string::size_type c1,
					   std::string::size_type c2)
{
  bool background = (tok == "background");
  if (next_param (s, c1, c2, tok, "Expected parameter after foreground in "))
    {
      if (background)
	gi->set_background (eval_param (tok));
      else
	gi->set_foreground (eval_param (tok));
    }
  return true;
}

/* clear */
bool question_implementation::exec_clear (const string &, string &,
					  string, std::string::size_type,
					  std::string::size_type)
{
  gi->clear_screen();
  return true;
}

/* debug */
bool question_implementation::exec_debug (const string &s, string &,
					  string tok, std::string::size_type c1,
					  std::string::size_type c2)
{
  if (next_param (s, c1, c2, tok, "Expected param after debug in "))
    gi->debug_print (eval_param(tok));
  return true;
}

/* "helpdisplaytext" is displaytext for the same reason helpmsg is msg: with no
 * separate help window it goes through DisplayTextSection unchanged
 * (V4Game.Part2.cs:5972-5975). */
bool question_implementation::exec_displaytext (const string &s, string &,
						string tok, std::string::size_type c1,
						std::string::size_type c2)
{
  if (!next_param (s, c1, c2, tok,
		   tok == "displaytext"
		   ? "Expected parameter after displaytext in "
		   : "Expected parameter after helpdisplaytext in "))
    return true;
  const QuestionBlock *gb = gf.find_by_name ("text", param_contents(tok));
  if (gb != NULL)
    {
      /* Quest prints each line of the block with a single newline, then
       * ends the displaytext with one trailing blank line.  (print_formatted
       * already appends a newline by default, so a second print_newline per
       * line would double-space the block -- e.g. Mansion's <intro> blanks.)
       * For a single-line block this is identical to the old two-newline
       * behaviour, so <win>/<lose> spacing is unchanged. */
      for (size_t i = 0; i < gb->data.size(); i ++)
	/* From ASL 3.92 on each line goes through GetParameter first, so a
	   text block can carry variables: DisplayTextSection prints
	   GetParameter ("<" + line + ">") (V4Game.Part2.cs:4089-4096).
	   Riddle Run's <win> block greets the player by name, and printing
	   the line raw left "#playername#" in the winning text. */
	print_formatted (asl_version_ >= 392 ? eval_string (gb->data[i])
					     : gb->data[i]);
      print_newline();
    }
  else
    gi->debug_print ("No such text block " + tok);
  return true;
}

/* "helpmsg" is just "msg".  In Quest 4 it printed into the separate help
 * window; the Quest 5 player has no such window, so ExecuteScript sends it to
 * Print exactly like msg (V4Game.Part2.cs:5778-5781), and so does Question, which
 * likewise has one output stream.  It used to be a no-op here, which swallowed
 * ThunderClan Mystery 1's "***TYPER GET HERB***" prompt whole.  "helpclose"
 * and "helpclear" are the ones that really do nothing (ibid. 5782, 5988). */
bool question_implementation::exec_msg (const string &s, string &,
					string tok, std::string::size_type c1,
					std::string::size_type c2)
{
  const char *complaint = (tok == "msg")
    ? "Expected parameter after msg in "
    : "Expected parameter after helpmsg in ";
  /* Quest reads the message with GetParameter, which takes whatever lies
   * between the line's first '<' and its first '>' and never looks at what
   * comes before the '<' (V4Game.cs:1858-1874).  So a modifier word
   * between the statement and its parameter is simply invisible to it --
   * `nospeak`, the ASL keyword that suppresses the text-to-speech reading
   * of a message (Libraries/quest.dat:204), is the one games write.  Question
   * demanded the parameter as the very next token and dropped the whole
   * message when it was not, which lost King's Quest V's and The Former's
   * `msg nospeak <...>` lines. */
  std::string::size_type lt = s.find ('<', c2);
  if (lt != string::npos)
    c2 = lt;
  if (next_param (s, c1, c2, tok, complaint))
    print_eval (param_contents(tok));
  return true;
}

/* outputoff */
bool question_implementation::exec_outputoff (const string &, string &,
					      string, std::string::size_type,
					      std::string::size_type)
{
  outputting = false;
  return true;
}

/* outputon */
bool question_implementation::exec_outputon (const string &, string &,
					     string, std::string::size_type,
					     std::string::size_type)
{
  outputting = true;
  return true;
}

/* nointro */
bool question_implementation::exec_nointro (const string &, string &,
					    string, std::string::size_type,
					    std::string::size_type)
{
  /* Suppress the automatic <intro> display that would otherwise follow the
   * startscript (see auto_intro_). */
  auto_intro_ = false;
  return true;
}

/* picture */
bool question_implementation::exec_picture (const string &s, string &,
					    string tok, std::string::size_type c1,
					    std::string::size_type c2)
{
  /*   picture close                          -- close the picture window
   *   picture popup <file[@WxH][; caption]>  -- ASL >= 3.90
   *   picture <file[@WxH][; caption]>        -- ASL 2.82 to 3.89
   *   picture <file>                         -- ASL >= 3.90, among the text
   *
   * One if/else-if chain decides it (V4Game.Part2.cs:5836-5858).  `popup'
   * is a word only from 3.90 on, and before then a plain `picture' *is*
   * the popup, with the size and caption syntax that goes with it.  From
   * 3.90 the plain form draws among the text and splits nothing, so a
   * filename there keeps any '@' or ';' it happens to contain.  Below 2.82
   * `picture' is not a statement at all -- see exec_show for the spelling it
   * had then.  `picture close' closes a window Question never opened, so it
   * does nothing here, as it does nothing in QuestViva (ibid. 5836). */
  tok = next_token (s, c1, c2);
  if (ci_equal (tok, "close"))
    return true;
  bool popup = ci_equal (tok, "popup");
  if (popup)
    tok = next_token (s, c1, c2);
  if (is_param (tok) && asl_version_ >= 282)
    show_picture (eval_param (tok), popup || asl_version_ < 390);
  return true;
}

/* playwav, playmidi, playmod */
bool question_implementation::exec_play_sound (const string &s, string &,
					       string tok, std::string::size_type c1,
					       std::string::size_type c2)
{
  /* play{wav,midi,mod} <file>        -- play (music loops by default)
   *                    <file; loop>  -- force looping
   *                    <file; noloop>-- force play-once
   *                    <file; sync>  -- play it (synchronously, in Quest)
   *                    <>            -- stop all sounds
   * The host backend decodes wav/midi/mod transparently, so all three
   * route through the same play_sound path.  The filename is resolved
   * relative to the game file by the interface.  midi/mod are background
   * music and loop by default; wav plays once. */
  bool looped = (tok != "playwav");
  bool sync = false;
  tok = next_token (s, c1, c2);
  if (!is_param (tok))
    {
      gi->debug_print ("Expected parameter in '" + s + "'");
      return true;
    }
  vector<string> args = split_param (param_contents (tok));
  string fname = args.empty() ? "" : trim (args[0]);
  for (size_t i = 1; i < args.size(); i ++)
    {
      string flag = lcase (trim (args[i]));
      if (flag == "loop")
	looped = true;
      else if (flag == "noloop")
	looped = false;
      else if (flag == "sync")
	sync = true;
    }
  gi->play_sound (fname, looped, sync);
  /* A synchronous sound holds the turn until it has played out, and Quest
   * suspends for it too (V4Game.cs:7692-7697). */
  if (sync)
    suspend_turn ();
  return true;
}

/* say */
bool question_implementation::exec_say (const string &s, string &,
					string tok, std::string::size_type c1,
					std::string::size_type c2)
{
  if (next_param (s, c1, c2, tok, "Expected param after say in "))
    {
      tok = eval_param (tok);
      print_formatted ("\"" + tok + "\"");
    }
  return true;
}

/* speak */
bool question_implementation::exec_speak (const string &s, string &,
					  string tok, std::string::size_type c1,
					  std::string::size_type c2)
{
  if (next_param (s, c1, c2, tok, "Expected param after speak in "))
    gi->speak (eval_param (tok));
  return true;
}
