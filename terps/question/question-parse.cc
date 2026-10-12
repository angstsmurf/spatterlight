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

/* question-parse.cc -- Player input: synonym substitution, the turn loop
 * (run_turn/run_command), command and object matching against defined
 * commands, and try_match, which hands what the game did not claim to the
 * built-in verb handlers of question-verbs.cc.
 *
 * Part of question_implementation; question-runner.cc holds the rest of the
 * preamble and question-internal.hh what these units share. */

#include "question-internal.hh"

using namespace std;

/* Synonym matching follows Quest (V4Game.Part2.cs:4143-4164): the typed command
 * is lowercased before this runs, and each synonym's left-hand word is searched
 * for verbatim -- case-sensitively, whitespace-flanked -- within it, in file
 * order, left to right, repeatedly.  So a capitalised synonym word in the game
 * file never fires, in Quest or here.
 *
 * The order is settled by SetUpSynonyms (V4Game.Part2.cs:1277-1313): one
 * record per left-hand word, file line by file line, words left to right
 * within a line -- the same order the nested loops below walk.  One divergence
 * is deliberate: Quest refuses at load any synonym whose word appears whole
 * inside its own replacement ("pole = flag pole" is logged and dropped), where
 * Question keeps it and instead skips occurrences already sitting inside an
 * expansion -- such a synonym works here and is merely dead in Quest. */
string question_implementation::substitute_synonyms (string s) const
{
  string orig = s;
  QUESTION_DBG << "substitute_synonyms (" << s << ")\n";
  /* A bare movement command takes priority over any game synonym, so the
   * direction abbreviations (n, s, e, ...) always work even when a game maps
   * one of those letters to something else (World's End maps "s" to "space
   * suit"). */
  {
    string t = trim (s);
    for (size_t i = 0; i < ARRAYSIZE (dir_names); i ++)
      if (t == dir_names[i] || t == short_dir_names[i] ||
	  t == "go " + dir_names[i] || t == "go " + short_dir_names[i])
	return s;
  }
  const QuestionBlock *gb = gf.find_by_name ("synonyms", "");
  if (gb != NULL)
    {
      for (auto &line: gb->data)
	{
	  std::string::size_type index = line.find ('=');
	  if (index == string::npos)
	    continue;
	  vector<string> words = split_param (line.substr (0, index));
	  string rhs = trim (line.substr (index + 1));
	  if (rhs == "")
	    continue;
	  for (size_t j = 0; j < words.size(); j ++)
	    {
	      string lhs = words[j];
	      if (lhs == "")
		continue;
	      /* Don't let a synonym shadow a real object: if the word exactly
	       * names an object currently in scope, leave it alone.  (E.g. the
	       * synonym "pole = flag pole" must not rewrite "pole" once a "pole"
	       * object is present.) */
	      /* Cheapest test first: most synonyms are not in the command at all. */
	      if (s.find (lhs) == string::npos)
		continue;
	      if (names_object_in_scope (lhs))
		continue;
	      std::string::size_type k = 0;
	      while ((k = s.find (lhs, k)) != string::npos)
		{
		  std::string::size_type end_index = k + lhs.length();
		  if ((k == 0 || s[k-1] == ' ') &&
		      (end_index == s.length() || s[end_index] == ' '))
		    {
		      /* Don't rewrite the lead word of a phrasal verb (e.g. "take" in
		       * "take off"): it's a distinct verb matched whole downstream. */
		      {
			std::string::size_type ws = end_index;
			while (ws < s.length() && s[ws] == ' ') ++ ws;
			std::string::size_type we = ws;
			while (we < s.length() && s[we] != ' ') ++ we;
			if (we > ws &&
			    is_phrasal_verb_lead (lhs + " " + s.substr (ws, we - ws)))
			  { k = end_index; continue; }
		      }
		      /* Don't expand the word if it's already part of a complete,
		       * word-bounded occurrence of its own expansion -- otherwise
		       * "flag pole" (pole = flag pole) becomes "flag flag pole". */
		      bool already = false;
		      for (std::string::size_type p = 0;
			   (p = s.find (rhs, p)) != string::npos; p ++)
			{
			  std::string::size_type pe = p + rhs.length();
			  if ((p == 0 || s[p-1] == ' ') &&
			      (pe == s.length() || s[pe] == ' ') &&
			      p <= k && end_index <= pe)
			    { already = true; break; }
			}
		      if (already)
			{
			  k = end_index;
			  continue;
			}
		      s = s.substr (0, k) + rhs + s.substr (k + lhs.length());
		      k = k + rhs.length();
		    }
		  else
		    k ++;
		}
	    }
	}
    }
  QUESTION_DBG << "substitute_synonyms (" << orig << ") -> '" << s << "'\n";
  return s;
}

bool question_implementation::is_running () const
{
  return is_running_;
}

std::string question_implementation::get_banner ()
{
  string banner;
  const QuestionBlock *gb = gf.find_by_name ("game", "game");
  if (gb)
    {
      string first = gb->data[0];
      std::string::size_type c1, c2;
      string tok = first_token (first, c1, c2);
      tok = next_token (first, c1, c2);
      tok = next_token (first, c1, c2);
      if (is_param (tok))
        {
          banner = eval_param (tok);

          for (const string &line: gb->data)
            {
              /* CI: Quest reads these with FindStatement ("game version" etc.),
               * which matches with BeginsWith (V4Game.Part2.cs:3616-3619). */
              if (ci_equal (first_token (line, c1, c2), "game") &&
                  ci_equal (next_token (line, c1, c2), "version") &&
                  is_param (tok = next_token (line, c1, c2)))
                {
                  banner += ", v";
                  banner += eval_param (tok);
                }
            }

          for (const string &line: gb->data)
            {
               if (ci_equal (first_token (line, c1, c2), "game") &&
                   ci_equal (next_token (line, c1, c2), "author") &&
                   is_param (tok = next_token (line, c1, c2)))
                {
                  banner += " | ";
                  banner += eval_param (tok);
                }
            }
        }
    }
  return banner;
}

/* Bring a line the player typed into the same encoding as the game text.
 * read_question_file transcodes the file from Windows-1252 to UTF-8, so a noun with
 * an accent in it is UTF-8 by the time it reaches the object table; input,
 * though, arrives from a non-Unicode Glk line request (Latin-1 bytes) or
 * straight out of a command-script file, and "pañuelo" typed as one byte per
 * letter would no longer match the object of that name.  The test is the same
 * one the loader uses, and for the same reason: a line that is already
 * well-formed UTF-8 -- which every pure-ASCII line is -- is left alone. */
string input_to_utf8 (const string &s)
{
  return text_is_utf8 (s) ? s : cp1252_to_utf8 (s);
}

/* Quest's SendCommand starts the turn, waits for it to *suspend*, and only then
 * ticks the timers (V4Game.Part2.cs:78-88):
 *
 *     _turnSuspendedTcs = new TaskCompletionSource();
 *     _ = HandleCommandAsync(command);
 *     await _turnSuspendedTcs.Task;
 *     if (elapsedTime > 0) await Tick(elapsedTime);
 *
 * A turn suspends when it finishes -- but also at its first `wait` or `pause`,
 * which is precisely what those do (DoWaitAsync/PauseAsync, V4Game.cs:3674-3680
 * and 6663-6669, both SignalTurnSuspended before awaiting the player).  So in a
 * turn that waits for a keypress the timers fire *at the wait*, and the rest of
 * the turn's script runs after them.  Only the first one counts: the turn's
 * TaskCompletionSource is created once per SendCommand and TrySetResult on an
 * already-completed one does nothing.
 *
 * The quieter half of the same rule is that Tick skips a timer on the turn it
 * was armed (BypassThisTurn, ibid. 118-123, which tick_timers has too), so a
 * `timeron` *after* the turn's wait has already missed that turn's Tick and
 * spends its bypass on the next one instead -- the timer starts a whole turn
 * later than it would if the tick came at the end of the turn.  King's Quest V
 * is built out of exactly this: its rooms arm their cut-scene countdowns from a
 * script that opens with `wait <press any key>`.
 *
 * tick_this_turn is the host's `elapsedTime > 0`: the walkthrough harness's
 * --tick, one tick per turn.  The Glk frontend leaves it false and drives
 * tick_timers from a real-time heartbeat instead, as Quest 4 itself does. */
void question_implementation::run_command (const string &s1, bool tick_this_turn)
{
  turn_tick_pending = tick_this_turn;
  run_command_body (s1);
  suspend_turn ();
}

void question_implementation::suspend_turn ()
{
  if (!turn_tick_pending)
    return;
  turn_tick_pending = false;
  tick_timers ();
}

void question_implementation::run_command_body (const string &s1)
{
  string s = input_to_utf8 (s1);

  if (!is_running_)
    return;

  print_newline();
  print_normal("> " + s);
  print_newline();

  if (s == "dump status")
    {
      ostringstream oss;
      oss << state;
      print_normal (oss.str());
      return;
    }

  if (s == "undo")
    {
      if (!undo())
	print_formatted ("(No more undo information available!)");
      return;
    }

  if (!state.running)
    return;

  /* "oops <correction>" (and the lenient "the <correction>") re-runs the last
   * command that failed on an unrecognised object word, with that word replaced
   * by the correction -- Quest's OOPS.  Tested on the command run_turn would
   * dispatch, synonyms and all. */
  {
    const string cooked = substitute_synonyms (lcase (s));
    string corr;
    bool is_oops = false;
    if (cooked.compare (0, 5, "oops ") == 0)
      { corr = trim (cooked.substr (5)); is_oops = true; }
    else if (oops_ready && cooked.compare (0, 4, "the ") == 0)
      { corr = trim (cooked.substr (4)); is_oops = true; }
    if (is_oops)
      {
	set_svar ("quest.originalcommand", lcase (s));
	set_svar ("quest.command", cooked);
	if (oops_ready && corr != "")
	  run_command_body (oops_before + corr + oops_after);
	else
	  print_formatted ("I don't understand your command. "
			   "Type HELP for a list of valid commands.");
	return;
      }
  }
  run_turn (s, false, false);

  if (state.running)
    { UndoState u = state.save_undo (); undo_buffer.push (u); }
}

/* One pass of Quest's ExecCommand: synonyms, the beforeturn hooks, the command
 * itself, the afterturn hooks (V4Game.Part2.cs:4127-4608).  The player's own
 * commands come through run_command_body, which adds the echo and the undo
 * snapshot around this; `exec' re-enters here instead, because `exec' is not a
 * dispatch shortcut -- ExecuteExec hands its parameter to the whole of
 * ExecCommand (ibid. 2229-2251), so an exec'd command gets its own beforeturn
 * and its own afterturn, and a turn built out of exec really does run the
 * afterturn twice.  Magic Sword (217) needs that: its `command <use #obj# on
 * #char#> exec <use #obj# on #char#; normal>' doubles the afterturn on every
 * attack, and the afterturn is what counts the Oggy-Waggi plant's regrowth
 * timer down, so the plant sprouts a turn earlier than one afterturn a turn
 * would have it.
 *
 * is_normal is ExecCommand's runUserCommand turned round: `exec <cmd; normal>'
 * asks for the built-in handling of cmd with the game's own commands passed
 * over. */
void question_implementation::run_turn (const string &input, bool is_internal,
				    bool is_normal)
{
  /* Quest lowercases the input before storing it, so quest.originalcommand is
   * "original" only in that it precedes synonym substitution
   * (V4Game.Part2.cs:4143-4145). */
  set_svar ("quest.originalcommand", lcase (input));
  string s = substitute_synonyms (lcase (input));
  set_svar ("quest.command", s);

  /* A fresh command: remember it for oops context, and clear any stale oops
   * state -- dereference_vars sets it again if this command fails on an
   * object word. */
  current_command = s;
  oops_ready = false;

  bool overridden = false;
  /* dontprocess is Quest's ctx.DontProcessCommand, a per-call flag
   * (V4Game.Part2.cs:4219); an exec'd turn must not lose the outer turn's. */
  const bool outer_dont_process = dont_process;
  dont_process = false;

  /* Quest reads the room index *once*, before the command is dispatched
   * (V4Game.ExecCommand, V4Game.Part2.cs:4116), and uses that same index for
   * both the beforeturn hook (4177-4188) and the afterturn hook (4567-4581).
   * So the afterturn that runs at the end of a turn belongs to the room you
   * started the turn in -- a room you walk into does not get its afterturn
   * until the turn *after* you arrive.  "Blight of Elantria" is built on this:
   * the Ice Queen's throne room kills you from its afterturn unless she is
   * already dead, which leaves exactly one turn inside to swing the warhammer
   * at her.  Looking the block up from state.location after the move instead
   * would kill the player on arrival and make the game unwinnable. */
  const string turn_start_location = state.location;

  /* beforeturn/afterturn hooks are pre-parsed per block (see QuestionBlock); a room
   * override suppresses the game block's hooks (matching the original scans). */
  const QuestionBlock *gb = gf.find_by_name ("room", turn_start_location);
  if (gb != NULL)
    {
      gf.ensure_cached (*gb);
      for (const QuestionBlock::hook_entry &h : gb->beforeturns)
	{
	  if (h.is_override)
	    overridden = true;
	  run_script_as (turn_start_location, h.script);
	}
    }
  else
    gi->debug_print ("Unable to find block " + turn_start_location + ".\n");

  if (!overridden) {
    gb = gf.find_by_name ("game", "game");
    if (gb != NULL)
      {
	gf.ensure_cached (*gb);
	for (const QuestionBlock::hook_entry &h : gb->beforeturns)
	  run_script_as ("game", h.script);   /* game override does not suppress */
      }
    else
      gi->debug_print ("Unable to find block game.\n");
  }

  if (!dont_process)
    {
      if (!try_match (s, is_internal, is_normal))
	display_error ("badcommand");
    }

  overridden = false;

  gb = gf.find_by_name ("room", turn_start_location);
  if (gb != NULL)
    {
      gf.ensure_cached (*gb);
      for (const QuestionBlock::hook_entry &h : gb->afterturns)
	{
	  if (h.is_override)
	    overridden = true;
	  run_script_as (turn_start_location, h.script);
	}
    }
  if (!overridden) {
    gb = gf.find_by_name ("game", "game");
    if (gb != NULL)
      {
	gf.ensure_cached (*gb);
	for (const QuestionBlock::hook_entry &h : gb->afterturns)
	  run_script_as ("game", h.script);   /* game override does not suppress */
      }
  }

  dont_process = outer_dont_process;
}

ostream &operator<< (ostream &o, const match_rv &rv)
{
  o << "match_rv {" << (rv.success ? "TRUE" : "FALSE") << ": [";
  o << rv.bindings;
  o << "]}";
  return o;
}

match_rv question_implementation::match_command (const string &input, const string &action) const
{
  /* A command pattern with an unpaired '#' (a typo in the game's own source,
   * which Quest tolerates) makes the recursive matcher throw.  Treat such a
   * pattern as simply not matching rather than letting the exception abort the
   * whole interpreter. */
  try
    {
      return match_command (input, 0, action, 0, match_rv ());
    }
  catch (const string &err)
    {
      gi->debug_print ("Skipping malformed command pattern: " + err);
      return match_rv ();
    }
}

match_rv question_implementation::match_command (const string &input, uint ichar, const string &action, uint achar, match_rv rv) const
{
  for (;;)
    {
      if (achar == action.length())
	{
	  return match_rv (ichar == input.length(), rv);
	}
      if (action[achar] == '#')
	{

	  achar ++;
	  string varname;
	  while (achar != action.length() && action[achar] != '#')
	    {
	      varname += action[achar];
	      achar ++;
	    }
	  if (achar == action.length())
	    throw string ("Unpaired hashes in command string " + action);
	  size_t index = rv.bindings.size();
	  rv.bindings.push_back (match_binding (varname, ichar));
	  achar ++;
	  varname = "";
	  rv.bindings[index].set (varname, ichar);
	  while (ichar < input.length())
	    {
	      match_rv tmp = match_command (input, ichar, action, achar, rv);
	      if (tmp.success)
		return tmp;
	      varname += input[ichar];
	      ichar ++;
	      rv.bindings[index].set (varname, ichar);
	    }
	  return match_rv (achar == action.length(), rv);
	}
      if (ichar == input.length() || !c_equal_i (input[ichar], action[achar]))
	return match_rv ();
      ++ achar;
      ++ ichar;
    }
}

/* True if "text" occurs in "target" as a run of whole words, so a player can
 * name an object by part of its name: "rose" matches "Red Rose", "tondy"
 * matches "Head General Tondy", "board" matches "Notice board". */
/* need_right_boundary=false is Quest's abbreviation test, which is
 * `InStr(" " + thisName, " " + name)` (V4Game.cs:4759): the typed noun has to
 * start a word of the object's name, but it need not finish one.  So "old
 * jacket" reaches an object actually called `old jacket.', which is how
 * "Where's my cell phone" expects its jacket to be taken -- stdverbs.lib's
 * full-stop splitter hands the take the noun with the stop shaved off. */
static bool word_match (const string &text, const string &target,
			bool need_right_boundary = true)
{
  string x = lcase (trim (text)), t = lcase (target);
  if (x.empty())
    return false;
  std::string::size_type pos = 0;
  while ((pos = t.find (x, pos)) != string::npos)
    {
      bool left_ok = (pos == 0) || t[pos - 1] == ' ';
      std::string::size_type end = pos + x.length();
      bool right_ok = !need_right_boundary || (end == t.length()) || t[end] == ' ';
      if (left_ok && right_ok)
	return true;
      ++ pos;
    }
  return false;
}

/* Quest compares a typed noun against each alt name case-insensitively
 * (Disambiguate lowercases both sides, V4Game.cs:4717-4731), so the comparison
 * here has to be case-insensitive too: `alt <Man>' on The Mansion's man in a
 * black suit is capitalised, and a case-sensitive test made EXAMINE MAN and
 * USE FIRE AX ON MAN miss him entirely.  That went unnoticed for as long as the
 * loose whole-word pass in get_obj_name ran for every game, because word_match
 * *is* case-insensitive and quietly picked the object up on the second pass;
 * once that pass was correctly gated on ASLVersion >= 391 and `abbreviations',
 * every pre-391 game with a capitalised alt lost its alt names. */
static bool match_object_alts (string text, const vector<string> &alts, bool is_internal)
{
  for (const string &i: alts)
    {
      QUESTION_DBG << "m_o_a: Checking '" << text << "' v. alt '" << i << "'.\n";
      if (starts_with_i (text, i))
	{
	  std::string::size_type len = i.length();
	  if (text.length() == len)
	    return true;
	  if (text.length() > len  &&  text[len] == ' '  &&
	      match_object_alts (text.substr (len+1), alts, is_internal))
	    return true;
	}
    }
  return false;
}

bool question_implementation::match_object (const string &text, const string &name, bool is_internal, bool allow_partial, bool internal_name_ok) const
{
  QUESTION_DBG << "* * * match_object (" << text << ", " << name << ", "
       << (is_internal ? "true" : "false") << ")\n";

  string alias, alt_list, prefix, suffix;

  if (is_internal && ci_equal (text, name)) return true;

  /* The lenient internal-name arm (see lenient_names_ in question-impl.hh): only
   * get_obj_name's last-resort pass sets internal_name_ok, after the exact and
   * partial alias passes have both come up empty, and it keeps the result only
   * when exactly one object answers.  The word-run half rides the same
   * allow_partial gate as the alias's, so a game that turned abbreviations off
   * gets the exact declared name and nothing looser. */
  if (internal_name_ok &&
      (ci_equal (text, name) ||
       (allow_partial && word_match (text, name, false))))
    return true;

  if (get_obj_property (name, "prefix", prefix) &&
      starts_with (text, prefix + " ") &&
      match_object (text.substr (prefix.length() + 1), name, false, allow_partial, internal_name_ok))
    return true;

  if (get_obj_property (name, "suffix", suffix) &&
      ends_with (text, " " + suffix) &&
      match_object (text.substr (0, text.length() - suffix.length() - 1), name, false, allow_partial, internal_name_ok))
    return true;

  if (!get_obj_property (name, "alias", alias))
    alias = name;
  if (ci_equal (text, alias))
    return true;
  /* Partial matching: accept any whole-word run of the alias, so the player
   * can refer to an object by part of its name ("rose" -> "Red Rose").  Only
   * used as a fallback (see get_obj_name) so exact matches win.  The alias
   * only, never the definition name: Quest's abbreviation pass reads
   * ObjectAlias (V4Game.cs:4738-4760), which is *initialised* to the name and
   * *replaced* by `alias' -- so an aliased object's definition name is not
   * typeable at all.  Blight of Elantria's island boat is `boat2' aliased
   * "boat", and USE OAR ON BOAT2 is "That doesn't work." in Quest.  Question
   * deliberately re-admits the declared name, but only through the
   * internal_name_ok arm above, on the last-resort unique-match pass. */
  if (allow_partial && word_match (text, alias, false))
    return true;

  const QuestionBlock *gb = gf.find_by_name ("object", name);
  if (gb != NULL)
    {
      std::string::size_type c1=0, c2;
      for (const string &line: gb->data)
	{
	  string tok = first_token (line, c1, c2);
	  /* CI: BeginsWith "alt " (V4Game.Part2.cs:3287). */
	  if (ci_equal (tok, "alt"))
	    {
	      tok = next_token (line, c1, c2);
	      if (!is_param (tok))
		gi->debug_print ("Expected param after alt in " + line);
	      else
		{
		  vector<string> alts = split_param (param_contents(tok));
		  QUESTION_DBG << "  m_o: alt == " << alts << "\n";
		  if (match_object_alts (text, alts, is_internal))
		    return true;
		  if (allow_partial)
		    for (const string &a: alts)
		      if (word_match (text, a))
			return true;
		  return false;
		}
	    }
	}
    }

  return false;
}

bool question_implementation::dereference_vars (vector<match_binding> &bindings, bool is_internal) const
{
  vector<string> where;
  where.push_back ("inventory");
  where.push_back (state.location);
  return dereference_vars (bindings, where, is_internal);
}

bool question_implementation::dereference_vars (vector<match_binding> &bindings, const vector<string> &where, bool is_internal,
					   bool quiet_notfound) const
{
  bool rv = true;
  for (auto &binding: bindings)
    if (binding.var_name[0] == '@')
      {
	/* Resolve pronouns ("it", "them", ...) to the last object referenced. */
	string lc = lcase (trim (binding.var_text));
	string obj_name;
	/* Quest clears quest.lastobject on entry to Disambiguate and writes the
	 * resolved name into it before returning (V4Game.cs:4623, 4646-4689),
	 * so a fallback message printed after the object was found can name it.
	 * stdverbs.lib is built entirely out of that: every one of its verb
	 * responses is "You can't <verb> #(quest.lastobject):article#.". */
	const_cast<question_implementation *> (this)->set_svar ("quest.lastobject", "");
	/* Disambiguate's pronouns, and the three kinds it sorts them into
	 * (V4Game.cs:4653-4695). */
	bool is_pronoun = lc == "it" || lc == "them" || lc == "this" ||
			  lc == "those" || lc == "these" || lc == "that" ||
			  lc == "him" || lc == "her";
	/* A DELIBERATE deviation: an object in scope that is itself called by
	 * the pronoun wins over the pronoun.  Disambiguate tests for "it",
	 * "them", "him" and "her" before it looks at a single object
	 * (V4Game.cs:4655-4697), so in Quest such an object can never be
	 * referred to at all -- Bob's Adventure names its beggar `him' and its
	 * lady `Her', and every verb the author wrote for them answers "I don't
	 * know what 'him' you are referring to."  The author plainly meant the
	 * object, and nothing can depend on it being unreachable.  Only an exact
	 * and unique match on the name the player sees counts, so a pronoun is
	 * never taken away from a game that uses it as one. */
	string named;
	if (is_pronoun)
	  {
	    int found = 0;
	    for (const auto &o: state.objs)
	      {
		bool here = false;
		for (const string &loc: where)
		  if (loc == "game" || ci_equal (o.parent, loc))
		    here = true;
		string shown;
		if (!get_obj_property (o.name, "alias", shown))
		  shown = o.name;
		if (here && !has_obj_property (o.name, "hidden") &&
		    ci_equal (trim (shown), lc))
		  {
		    named = o.name;
		    found ++;
		  }
	      }
	    if (found != 1)
	      named = "";
	  }
	bool bad_pronoun = false;
	if (named != "")
	  obj_name = named;
	else if (is_pronoun)
	  {
	    /* A pronoun stands for the last object only while that object is
	     * still in scope and of the pronoun's kind -- "him" for an object
	     * whose article is him, "her" likewise, everything else for the
	     * rest (V4Game.cs:4773-4791).  Otherwise the answer is BadPronoun,
	     * "I don't know what 'it' you are referring to.", and nothing
	     * more: Disambiguate returns -2, which its callers take as "already
	     * answered" (V4Game.cs:4653-4695). */
	    const_cast<question_implementation *> (this)
	      ->set_svar ("quest.error.pronoun", lc);
	    bool ok = last_object != "" &&
		      !has_obj_property (last_object, "hidden");
	    if (ok)
	      {
		string article, parent;
		get_obj_property (last_object, "article", article);
		string kind = lcase (trim (article));
		if (kind != "him" && kind != "her")
		  kind = "it";
		ok = kind == (lc == "him" || lc == "her" ? lc : string ("it"));
		bool here = false;
		for (const auto &o: state.objs)
		  if (ci_equal (o.name, last_object))
		    for (const string &loc: where)
		      if (loc == "game" || ci_equal (o.parent, loc))
			here = true;
		ok = ok && here;
	      }
	    if (ok)
	      obj_name = last_object;
	    else
	      {
		/* Said once, and even to a caller that words its own
		 * refusals: those check pronoun_refused_ and say no more. */
		bad_pronoun = true;
		if (!pronoun_refused_)
		  const_cast<question_implementation *> (this)
		    ->display_error ("badpronoun");
		pronoun_refused_ = true;
		rv = false;
	      }
	  }
	else
	  obj_name = get_obj_name (binding.var_text, where, is_internal);
	if (bad_pronoun)
	  ;
	else if (obj_name == "!")
	  {
	    /* Disambiguate's failure path stores the unresolved noun in
	     * quest.error.object (V4Game.cs:4859-4860).  const_cast because
	     * this const path legitimately updates that error variable, like
	     * the oops state below (both are mutable-by-design). */
	    const_cast<question_implementation *> (this)
	      ->set_svar ("quest.error.object", binding.var_text);
	    /* The miss is an overridable player error, not a fixed sentence:
	     * MatchCommand answers an unresolved #@object# with BadThing from
	     * ASL 3.91 and BadItem below it (V4Game.Part2.cs:2490-2500). */
	    if (!quiet_notfound)
	      const_cast<question_implementation *> (this)
		->display_error (asl_version_ >= 391 ? "badthing" : "baditem");
	    /* Record the first unrecognised object word (its position in the
	     * command) so a following "oops <correction>" can rebuild and re-run
	     * the command with the word replaced. */
	    if (!is_internal && !oops_ready)
	      {
		size_t b = binding.start < current_command.size () ? binding.start
		                                                   : current_command.size ();
		size_t e = binding.end < current_command.size () ? binding.end
		                                                 : current_command.size ();
		oops_before = current_command.substr (0, b);
		oops_after = current_command.substr (e);
		oops_ready = true;
	      }
	    rv = false;
	  }
	else
	  {
	    binding.var_text = obj_name;
	    binding.var_name = binding.var_name.substr (1);
	    last_object = obj_name;   /* remember for a later pronoun */
	    const_cast<question_implementation *> (this)
	      ->set_svar ("quest.lastobject", obj_name);
	  }
      }
  return rv;
}

string question_implementation::get_obj_name (const string &typed, const vector<string> &where, bool is_internal) const
{
  vector<string> objs, printed_objs;
  /* Disambiguate drops a leading "the " from the noun before it looks at any
   * object, and its exact pass then takes a name that is either the noun or
   * "the " + the noun (V4Game.cs:4699-4720) -- so LOOK AT THE RAT finds `rat',
   * and both THE BRIDGE and BRIDGE find an object aliased "The Bridge".  The
   * abbreviation pass sees the stripped noun and nothing else. */
  string name = typed;
  if (lcase (name).compare (0, 4, "the ") == 0)
    name = name.substr (4);
  /* Collect objects in scope whose name matches.  Two passes: first exact
   * (name/alias/alt), and only if nothing matches exactly do we allow partial
   * whole-word matches.  This keeps exact names unambiguous ("ice" matching an
   * alt "ice") while still letting "rose" find "Red Rose" when nothing else
   * matches.  Quest runs the same two passes and gates the second one on
   * ASLVersion >= 391 and on "define options / abbreviations" (Disambiguate,
   * V4Game.cs:4724-4767), so honour both: the loose pass is not merely a
   * nicety, it decides which of two like-named objects a puzzle sees, and
   * Permanant Room turns it off precisely so that "use pen on paper" reaches
   * the held `Peice of Paper' (aliased "paper") instead of stopping at the
   * scenery `Wall Paper' in the room.
   *
   * One deliberate divergence: we run the loose pass below 391 too, but only
   * accept it when it names exactly one object.  Half the corpus predates 391,
   * and in those games roughly 27 nouns per object list are words sitting
   * inside a name that Quest simply refuses -- a worn necklace whose alias
   * TypeLib has rewritten to `Necklace [worn]' stops answering to "necklace"
   * (DromBennacht).  Unique-match-only is what makes that safe to hand back:
   * it can turn a refusal into a success, but it can never ask a
   * disambiguation question Quest would not have asked, and never pick a
   * different object than Quest picked -- and ambiguity is the whole failure
   * mode, which is why Permanant Room above needed an escape hatch that a
   * pre-391 author had no reason to write.
   *
   * And a third pass, another DELIBERATE deviation (lenient_names_ in
   * question-impl.hh): when both of those find nothing, accept the object's
   * *declared* name -- the `define object <boat2>' identifier an alias has
   * replaced -- if it names exactly one object in scope.  Quest refuses those
   * outright; early Question always took them, on purpose, and the unique-match
   * rule is what makes the leniency safe (it can only ever turn a refusal
   * into a success).  The walkthrough runner sets QUESTION_STRICT_NAMES to turn
   * this pass off, so derived walkthroughs stay replayable in a real Quest. */
  bool try_partial = use_abbreviations_;
  bool unique_only = asl_version_ < 391;
  for (int pass = 0; pass < 3 && objs.empty(); pass ++)
    {
      bool internal_name = (pass == 2);
      if (pass == 1 && !try_partial)
	continue;
      if (internal_name && !lenient_names_)
	break;
      bool allow_partial = internal_name ? try_partial : (pass == 1);
      for (size_t objnum = 0; objnum < state.objs.size(); objnum ++)
	{
	  bool is_used = false;
	  for (auto &loc: where)
	    {
	      /* Quest scopes a typed noun on the object's room and its hidden flag
	       * alone: DisambObjHere matches ContainerRoom against the room (and
	       * the inventory) and tests Exists, which is what the hidden property
	       * sets (V4Game.cs:4303-4350, 4165-4178).  Something parented inside a
	       * container keeps the room it was defined in, so the container chain
	       * only reaches scope through those hidden flags -- which the engine
	       * re-derives on container events, not continuously (see
	       * update_container_visibility).  Hence the room alone, rather than a
	       * reachability test on the container: a game that reveals a parented
	       * object with "show" and never opens the container (Darkness's
	       * Hammer, Christmas Day's Knife) still leaves it referrable, as Quest
	       * does. */
	      if (loc == "game" || ci_equal (state.objs[objnum].parent, loc))
		is_used = true;
	    }
	  if (is_used &&
	      !has_obj_property (state.objs[objnum].name, "hidden") &&
	      (match_object (name, state.objs[objnum].name, is_internal,
			     allow_partial, internal_name) ||
	       (pass == 0 &&
		match_object ("the " + name, state.objs[objnum].name, false,
			      false, false))))
	    {
	      string printed_name, tmp, oname = state.objs[objnum].name;
	      objs.push_back (oname);
	      /* What the disambiguation menu calls this object: its `detail', or
	       * failing that its alias with the `prefix' in front (V4Game.cs:
	       * 4810-4820 -- ObjectType.Prefix is stored with the trailing space
	       * already on it, Part2:3273).  A prefix is only ever an article, so
	       * it is what tells two same-named objects apart in the list:
	       * "a bed" / "bed" / "a rickety bed" for three beds carrying,
	       * respectively, a prefix, nothing, and a detail. */
	      if (!get_obj_property (oname, "alias", printed_name))
		printed_name = oname;
	      if (get_obj_property (oname, "prefix", tmp) && tmp != "")
		printed_name = tmp + " " + printed_name;
	      if (get_obj_property (oname, "detail", tmp))
		printed_name = tmp;
	      printed_objs.push_back (printed_name);
	    }
	}
      /* An ambiguous loose match below 391 is dropped rather than put to the
	 player: Quest found nothing here, so the answer stays "nothing".  The
	 internal-name pass is unique-only at every version -- ambiguity is
	 exactly the doubt that forfeits the leniency. */
      if (objs.size () > 1 &&
	  ((allow_partial && unique_only) || internal_name))
	{
	  objs.clear ();
	  printed_objs.clear ();
	}
    }
  QUESTION_DBG << "objs == " << objs << ", printed_objs == " << printed_objs << "\n";
  if (objs.size() > 1)
    {
      uint num = 0;
      /* Quest's wording, which it prints into the main text as
       * "- |iPlease select which bed you mean:|xi" ahead of the menu and
       * echoes the chosen option back under as "- a bed" (V4Game.cs:4802-4803,
       * 4854).  The marker and the italics are that window's presentation of
       * a menu; here the caption is handed to the host, which lays the menu
       * out its own way (see QuestionGlkInterface::make_choice). */
      num = gi->choose_object ("Please select which " + name + " you mean:",
			       printed_objs);

      return objs[num];
    }
  if (objs.size() == 1)
    return objs[0];
  /* Quest 2.x items have no world object; resolve a held item by name so it can
   * be used in commands ("give milk to zeke", "use pitchfork on haystack"). */
  for (const string &it: state.items)
    if (ci_equal (it, name) || word_match (name, it))
      return it;
  return "!";
}

void question_implementation::set_vars (const vector<match_binding> &v)
{
  for (const auto &i: v)
    set_svar (i.var_name, i.var_text);
}

bool question_implementation::run_commands (string cmd, const QuestionBlock *room, bool is_internal, bool lib_pass)
{
  match_rv match;

  if (room == NULL)
    {
      gi->debug_print ("room is null\n");
      return false;
    }

  /* Commands and their split patterns are pre-parsed once (see QuestionBlock /
   * ensure_cached); we no longer re-tokenize every line of the block each turn.
   * Order within a pass is file order.
   *
   * The two passes are Quest's: ExecCommand tries the game's own commands, then
   * its verbs, then the commands a library contributed, then the library's
   * verbs (V4Game.Part2.cs:4224-4241).  stdverbs.lib is why that matters --
   * its `command <#stdverbs.command#.>` matches literally anything ending in a
   * full stop, so on one pass it would eat commands the game means to handle
   * itself. */
  gf.ensure_cached (*room);
  for (const QuestionBlock::cmd_entry &e : room->commands)
    if (e.is_lib == lib_pass)
    for (const string &pat : e.patterns)
      if ((match = match_command (cmd, pat)))
	{
	  /* A command whose pattern fits has claimed the input even when its
	   * #@object# names nothing: ExecUserCommand sets foundCommand on the
	   * pattern alone and returns it whether or not GetCommandParameters
	   * succeeds (V4Game.Part2.cs:2405-2413).  Returning false here let the
	   * input fall through to the built-in verbs, so Hungry Goblin's
	   * `command <eat #@food#>` answered EAT XYZZY with the error twice. */
	  if (!dereference_vars (match.bindings, is_internal))
	    return true;
	  set_vars (match.bindings);
	  run_script_as (state.location, e.script);
	  return true;
	}

  return false;
}

bool question_implementation::try_game_verb (const string &cmd, bool is_internal, bool lib_pass)
{
  match_rv match;

  /* Game-scope verb declarations (see GameVerbLine for the syntax).  They
   * register custom verbs (e.g. "destroy", "mount") with a default response; an
   * object overrides via action <name> or properties <name=...>.
   *
   * These run before every built-in: ExecCommand tries user commands, then
   * ExecVerb, then library commands and library verbs, and only reaches its own
   * "speak to"/"use "/movement handling if none of them claimed the input
   * (V4Game.Part2.cs:4205-4240).  So a game that declares "verb <Use>" takes over
   * USE entirely -- which is how Operation Rising Star is won, its Card Reader
   * carrying "action <Use> playerwin" for a card the player never has -- and a
   * QDK-generated "verb <take>" is why games like Magic World give their takeable
   * objects an explicit "action <take>". */
  const QuestionBlock *game = gf.find_by_name ("game", "game");
  if (game != NULL)
    for (const string &line: game->data)
	{
	  GameVerbLine v;
	  if (!parse_game_verb_line (line, v))
	    continue;
	  if (v.is_lib != lib_pass)
	    continue;
	  /* ExecVerb reads the name list with GetParameter, so #string#, %numeric%
	   * and $function$ references inside it are substituted before any matching
	   * happens (V4Game.cs:2946).  That matters for a mistake QDK invites: an
	   * author who types the *command* form into the verb box ends up with
	   * "verb <drop #@object#>", and Quest turns that into the unmatchable name
	   * "drop " -- there is no string variable "object", so the reference
	   * collapses to nothing and leaves the trailing space that
	   * BeginsWith (cmd, name + " ") can never satisfy.  The verb is dead and
	   * DROP falls through to Quest's own handling.  Reading the name
	   * unevaluated instead made Question match "drop #@object#" as a two-noun
	   * pattern, so in "Pyramid Of Terror" DROP GINGERBREAD MAN answered
	   * "Consider it dropped." and never ran the gingerbread man's own drop
	   * script -- the script that unlocks the only way into the second half of
	   * the game. */
	  vector<string> names;
	  string key;
	  split_verb_names (eval_param (v.names_tok), names, key);
	  if (names.empty() || trim (names[0]) == "")
	    continue;
	  for (const string &verbname: names)
	    {
	      if (trim (verbname) == "")
		continue;
	      if ((match = match_command (cmd, verbname + " #@object#")))
		{
		  if (!dereference_vars (match.bindings, is_internal))
		    return true;
		  string obj = match.bindings[0].var_text, script;
		  if (get_obj_action (obj, key, script))
		    run_script_as (obj, script);
		  /* A DELIBERATE deviation hides in this lookup.  QDK lets a verb
		   * be named with a trailing space, `verb <Waste the >', and
		   * writes the same name into the object: `properties <Waste
		   * the =He falls.>'.  Quest keeps the space on the verb and
		   * loses it from the property name, so the two never meet and
		   * the verb's default answers instead (ExecVerb,
		   * V4Game.cs:2971, 3051-3058) -- Bob's Adventure's gangster.
		   * Here the property is found and the author's text printed. */
		  else if (get_obj_property (obj, key, script))
		    print_formatted (script);
		  else if (v.deflt != "")
		    run_script_as (obj, v.deflt);
		  return true;
		}
	    }
	}

  return false;
}

bool question_implementation::try_match (string cmd, bool is_internal, bool is_normal)
{
  pronoun_refused_ = false;

  if (!is_normal)
    {
      /* is_internal rides along: Quest's AllowRealNamesInCommand lives in the
       * ctx that ExecCommand hands ExecUserCommand, so a `command' block hit
       * from an exec'd or implied turn resolves its `#@...#' blanks with real
       * names allowed too (V4Game.Part2.cs:2228-2231, 4232-4242).  Things That
       * Go Bump's `command <remove #@ob#>' is the witness: the implied removal
       * of TAKE DUSTY FUSE hands it "remove fuse3", and `fuse3' is a real name
       * its alias has replaced. */
      if (run_commands (cmd, gf.find_by_name ("room", state.location), is_internal) ||
	  run_commands (cmd, gf.find_by_name ("game", "game"), is_internal))
	return true;
      /* Verbs are next, ahead of every built-in and inside the same
       * runUserCommand gate the user commands above are (see try_game_verb). */
      if (try_game_verb (cmd, is_internal))
	return true;
      /* Then, and only then, whatever a library contributed: its commands, then
       * its verbs.  Both lose to everything above (V4Game.Part2.cs:4224-4241). */
      if (run_commands (cmd, gf.find_by_name ("game", "game"), is_internal, true))
	return true;
      if (try_game_verb (cmd, is_internal, true))
	return true;
    }

  /* Quest treats LEAVE (and the informal LOOK OUT) as synonyms for OUT/EXIT,
   * i.e. leaving the current place -- the HELP text already advertises LEAVE.
   * Normalise to "out" here, after any game-defined command has had priority
   * and before the "look #@object#" handler below would swallow "look out". */
  if (cmd == "leave" || cmd == "look out")
    cmd = "out";

  /* The built-in verbs, in the order Quest tries them -- which matters: "take
   * off" has to be seen before "take", and the multi-word verbs of the table
   * before the bare "look" that would swallow them.  Each is in
   * question-verbs.cc and returns true once the command is dealt with. */
  return try_unwear (cmd, is_internal)
    || try_builtin_verb (cmd, is_internal)
    || try_look (cmd, is_internal)
    || try_give (cmd, is_internal)
    || try_remove (cmd, is_internal)
    || try_use (cmd, is_internal)
    || try_put (cmd, is_internal)
    || try_take (cmd, is_internal)
    || try_drop (cmd, is_internal)
    || try_speak (cmd, is_internal)
    || try_out (cmd, is_internal)
    || try_direction (cmd, is_internal)
    || try_goto (cmd, is_internal)
    || try_inventory (cmd, is_internal)
    || try_meta (cmd, is_internal);
}
