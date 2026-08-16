// geas-parse.inc -- Player input: synonym substitution, the turn loop
// (run_turn/run_command), command and object matching against defined
// commands, and the built-in verb handlers (try_match).
//
// Split out of geas-runner.cc, which #includes it (same single-TU layout
// as the quest5/aslx-*.inc units); the code is unchanged from the split.

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
 * geas keeps it and instead skips occurrences already sitting inside an
 * expansion -- such a synonym works here and is merely dead in Quest. */
string geas_implementation::substitute_synonyms (string s) const
{
  string orig = s;
  GEAS_DBG << "substitute_synonyms (" << s << ")\n";
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
  const GeasBlock *gb = gf.find_by_name ("synonyms", "");
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
  GEAS_DBG << "substitute_synonyms (" << orig << ") -> '" << s << "'\n";
  return s;
}

bool geas_implementation::is_running () const
{
  return is_running_;
}

std::string geas_implementation::get_banner ()
{
  string banner;
  const GeasBlock *gb = gf.find_by_name ("game", "game");
  if (gb)
    {
      string line = gb->data[0];
      std::string::size_type c1, c2;
      string tok = first_token (line, c1, c2);
      tok = next_token (line, c1, c2);
      tok = next_token (line, c1, c2);
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
 * read_geas_file transcodes the file from Windows-1252 to UTF-8, so a noun with
 * an accent in it is UTF-8 by the time it reaches the object table; input,
 * though, arrives from a non-Unicode Glk line request (Latin-1 bytes) or
 * straight out of a command-script file, and "pañuelo" typed as one byte per
 * letter would no longer match the object of that name.  The test is the same
 * one the loader uses, and for the same reason: a line that is already
 * well-formed UTF-8 -- which every pure-ASCII line is -- is left alone. */
static string input_to_utf8 (const string &s)
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
void geas_implementation::run_command (const string &s1, bool tick_this_turn)
{
  turn_tick_pending = tick_this_turn;
  run_command_body (s1);
  suspend_turn ();
}

void geas_implementation::suspend_turn ()
{
  if (!turn_tick_pending)
    return;
  turn_tick_pending = false;
  tick_timers ();
}

void geas_implementation::run_command_body (const string &s1)
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
void geas_implementation::run_turn (const string &input, bool is_internal,
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

  /* beforeturn/afterturn hooks are pre-parsed per block (see GeasBlock); a room
   * override suppresses the game block's hooks (matching the original scans). */
  const GeasBlock *gb = gf.find_by_name ("room", turn_start_location);
  if (gb != NULL)
    {
      gf.ensure_cached (*gb);
      for (const GeasBlock::hook_entry &h : gb->beforeturns)
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
	for (const GeasBlock::hook_entry &h : gb->beforeturns)
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
      for (const GeasBlock::hook_entry &h : gb->afterturns)
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
	for (const GeasBlock::hook_entry &h : gb->afterturns)
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

match_rv geas_implementation::match_command (const string &input, const string &action) const
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

match_rv geas_implementation::match_command (const string &input, uint ichar, const string &action, uint achar, match_rv rv) const
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
      GEAS_DBG << "m_o_a: Checking '" << text << "' v. alt '" << i << "'.\n";
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

bool geas_implementation::match_object (const string &text, const string &name, bool is_internal, bool allow_partial, bool internal_name_ok) const
{
  GEAS_DBG << "* * * match_object (" << text << ", " << name << ", "
       << (is_internal ? "true" : "false") << ")\n";

  string alias, alt_list, prefix, suffix;

  if (is_internal && ci_equal (text, name)) return true;

  /* The lenient internal-name arm (see lenient_names_ in geas-impl.hh): only
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
   * "boat", and USE OAR ON BOAT2 is "That doesn't work." in Quest.  geas
   * deliberately re-admits the declared name, but only through the
   * internal_name_ok arm above, on the last-resort unique-match pass. */
  if (allow_partial && word_match (text, alias, false))
    return true;

  const GeasBlock *gb = gf.find_by_name ("object", name);
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
		  GEAS_DBG << "  m_o: alt == " << alts << "\n";
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

bool geas_implementation::dereference_vars (vector<match_binding> &bindings, bool is_internal) const
{
  vector<string> where;
  where.push_back ("inventory");
  where.push_back (state.location);
  return dereference_vars (bindings, where, is_internal);
}

bool geas_implementation::dereference_vars (vector<match_binding> &bindings, const vector<string> &where, bool is_internal,
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
	const_cast<geas_implementation *> (this)->set_svar ("quest.lastobject", "");
	if (last_object != "" &&
	    (lc == "it" || lc == "them" || lc == "they" ||
	     lc == "him" || lc == "her"))
	  obj_name = last_object;
	else
	  obj_name = get_obj_name (binding.var_text, where, is_internal);
	if (obj_name == "!")
	  {
	    /* Disambiguate's failure path stores the unresolved noun in
	     * quest.error.object (V4Game.cs:4859-4860).  const_cast because
	     * this const path legitimately updates that error variable, like
	     * the oops state below (both are mutable-by-design). */
	    const_cast<geas_implementation *> (this)
	      ->set_svar ("quest.error.object", binding.var_text);
	    /* The miss is an overridable player error, not a fixed sentence:
	     * MatchCommand answers an unresolved #@object# with BadThing from
	     * ASL 3.91 and BadItem below it (V4Game.Part2.cs:2490-2500). */
	    if (!quiet_notfound)
	      const_cast<geas_implementation *> (this)
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
	    const_cast<geas_implementation *> (this)
	      ->set_svar ("quest.lastobject", obj_name);
	  }
      }
  return rv;
}

string geas_implementation::get_obj_name (const string &name, const vector<string> &where, bool is_internal) const
{
  vector<string> objs, printed_objs;
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
   * geas-impl.hh): when both of those find nothing, accept the object's
   * *declared* name -- the `define object <boat2>' identifier an alias has
   * replaced -- if it names exactly one object in scope.  Quest refuses those
   * outright; early geas always took them, on purpose, and the unique-match
   * rule is what makes the leniency safe (it can only ever turn a refusal
   * into a success).  The walkthrough runner sets GEAS_STRICT_NAMES to turn
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
	      match_object (name, state.objs[objnum].name, is_internal,
			    allow_partial, internal_name))
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
  GEAS_DBG << "objs == " << objs << ", printed_objs == " << printed_objs << "\n";
  if (objs.size() > 1)
    {
      uint num = 0;
      /* Quest's wording, which it prints into the main text as
       * "- |iPlease select which bed you mean:|xi" ahead of the menu and
       * echoes the chosen option back under as "- a bed" (V4Game.cs:4802-4803,
       * 4854).  The marker and the italics are that window's presentation of
       * a menu; here the caption is handed to the host, which lays the menu
       * out its own way (see GeasGlkInterface::make_choice). */
      num = gi->make_choice ("Please select which " + name + " you mean:",
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

void geas_implementation::set_vars (const vector<match_binding> &v)
{
  for (const auto &i: v)
    set_svar (i.var_name, i.var_text);
}

bool geas_implementation::run_commands (string cmd, const GeasBlock *room, bool is_internal, bool lib_pass)
{
  match_rv match;

  if (room == NULL)
    {
      gi->debug_print ("room is null\n");
      return false;
    }

  /* Commands and their split patterns are pre-parsed once (see GeasBlock /
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
  for (const GeasBlock::cmd_entry &e : room->commands)
    if (e.is_lib == lib_pass)
    for (const string &pat : e.patterns)
      if ((match = match_command (cmd, pat)))
	{
	  if (!dereference_vars (match.bindings, is_internal))
	    return false;
	  set_vars (match.bindings);
	  run_script_as (state.location, e.script);
	  return true;
	}

  return false;
}

bool geas_implementation::try_game_verb (const string &cmd, bool is_internal, bool lib_pass)
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
  const GeasBlock *game = gf.find_by_name ("game", "game");
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
	   * unevaluated instead made geas match "drop #@object#" as a two-noun
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

bool geas_implementation::try_match (string cmd, bool is_internal, bool is_normal)
{
  string tok;
  match_rv match;

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

  /* Take off / unwear a worn item.  Game-defined commands ran first (above), so
   * a game that scripts its own removal -- e.g. KQ5 refuses to take off the
   * cloak -- still wins; this is the fallback for clothing whose only handler is
   * a type's action <unwear> (which prints the message and resets the alias but,
   * like Quest's library, leaves clearing the worn flag to the command layer we
   * don't bundle).  Placed before the wear/verb table so "take off X" isn't read
   * as the verb "take". */
  for (const char *phrase : { "take off", "unwear", "doff" })
    if ((match = match_command (cmd, string (phrase) + " #@object#")))
      {
	if (!dereference_vars (match.bindings, is_internal))
	  return true;
	string obj = match.bindings[0].var_text, script;
	if (!has_obj_property (obj, "worn"))
	  print_formatted ("You aren't wearing it.");
	else
	  {
	    if (get_obj_action (obj, "unwear", script))
	      run_script_as (obj, script);   /* type action: message + alias reset */
	    else
	      {
		string um;
		if (get_obj_property (obj, "unwearmessage", um) && um != "")
		  print_formatted (um);
		else
		  print_formatted ("You take it off.");
	      }
	    set_obj_property (obj, "not worn");
	  }
	return true;
      }

  /* ---- Generic verb dispatch ---------------------------------------------
   * Quest games attach most verbs to objects through `action <verb>`,
   * `properties <verb=text>`, or (for opening things) the object's anonymous
   * default action.  geas previously hard-coded only look/examine/take/use/
   * give/drop/etc., so common verbs such as open/move/eat/drink/smell fell
   * through to "I don't understand your command".  This table maps the surface
   * phrases a player may type to the canonical key stored in the game file
   * (builtin_verbs, shared with the verb menu).
   * It is tried after explicit `command` definitions but before the bare
   * "look #@object#" handler below, so multi-word verbs like "look under" are
   * matched before "look" would swallow them. */
  {
    for (const verb_def &v: builtin_verbs ())
      for (const char *phrase: v.phrases)
	if ((match = match_command (cmd, string (phrase) + " #@object#")))
	  {
	    if (!dereference_vars (match.bindings, is_internal))
	      return true;
	    string obj = match.bindings[0].var_text, script;
	    string ph = phrase;   /* the actual verb the player typed */
	    string key = v.key;   /* the canonical verb it dispatches to */
	    /* "open" and "close" are their own routine in Quest, ExecOpenClose
	     * (V4Game.cs:2702-2847), and only from ASL 3.91 -- below that there
	     * is no container model at all and the words are left to the game
	     * (V4Game.Part2.cs:4448-4455).  Its order of tests is load-bearing,
	     * so follow it exactly:
	     *
	     *   not a container       -> cantopen/cantclose
	     *   already open/closed   -> alreadyopen/alreadyclosed
	     *   out of reach          -> cantopen/cantclose + the reason
	     *   action <open>         -> run it, and nothing else
	     *   no "open" property    -> cantopen/cantclose
	     *   otherwise             -> its text (or defaultopen/defaultclose)
	     *                            and then DoOpenClose
	     *
	     * Only that last branch changes any state, which is why Quest never
	     * marks an opened container "seen" on the way in: DoOpenClose sets
	     * "opened" and calls DoLook, and DoLook is one of the engine's two
	     * places that set "seen" (the other being the "add" half of
	     * DoAddRemove, from 4.10).  geas marked it seen before it even knew
	     * whether the object could be opened, so a *refused* open -- "It is
	     * already open." -- was enough to bring a hidden container's contents
	     * into scope, and The Blight of Elantria handed over a bronze key and
	     * a ruby that Quest keeps behind its `badthing` text. */
	    if ((key == "open" || key == "close") && asl_version_ >= 391)
	      {
		bool opening = (key == "open");
		const char *cant  = opening ? "cantopen" : "cantclose";
		if (!has_obj_property (obj, "container"))
		  display_error (cant, obj);
		else if (opening == has_obj_property (obj, "opened"))
		  display_error (opening ? "alreadyopen" : "alreadyclosed", obj);
		else
		  {
		    string why;
		    if (!player_can_access (obj, why))
		      display_error_info (cant, obj, why);
		    else if (get_obj_action (obj, key, script))
		      run_script_as (obj, script);
		    else if (get_obj_property (obj, key, script))
		      {
			/* A bare "open"/"close" line declares the container
			 * openable without giving a message; Quest then prints
			 * its defaultopen/defaultclose text, not a blank line. */
			if (script == "")
			  display_error (opening ? "defaultopen" : "defaultclose", obj);
			else
			  print_formatted (script);
			do_open_close (obj, opening, true);
		      }
		    /* Not one of Quest's branches: geas resolves a `define
		     * object` block's anonymous script line as a last-resort
		     * action, where Quest folds the game's `default` block into
		     * the object's own actions as it loads it.  Keep it in the
		     * place the old dispatch had it, below the property. */
		    else if (v.use_default && gf.get_obj_default_action (obj, script))
		      run_script_as (obj, script);
		    else
		      display_error (cant, obj);
		  }
		return true;
	      }
	    /* Prefer an action/property named by the typed verb (objects define
	     * e.g. action <search>, action <wear>), then the canonical key. */
	    if (get_obj_action (obj, ph, script) ||
		get_obj_action (obj, v.key, script))
	      run_script_as (obj, script);
	    else if (get_obj_property (obj, ph, script) ||
		     get_obj_property (obj, v.key, script))
	      {
		/* A bare "open"/"close" line in an object definition declares the
		 * container openable without giving a message.  Quest then prints its
		 * defaultopen/defaultclose text, not a blank line (ExecOpenClose,
		 * V4Game.cs:2830). */
		if (script == "" && (key == "open" || key == "close"))
		  display_error ("default" + key, obj);
		else
		  print_formatted (script);
	      }
	    else if (v.use_default && gf.get_obj_default_action (obj, script))
	      run_script_as (obj, script);
	    else if (key == "open")
	      {
		/* Validate as Quest does: already-open / can't-open. */
		if (has_obj_property (obj, "opened"))
		  display_error ("alreadyopen", obj);
		else if (open_container (obj))
		  ;   /* opened: state set + contents revealed */
		else
		  display_error ("cantopen", obj);
	      }
	    else if (key == "close")
	      {
		if (has_obj_property (obj, "closed"))
		  display_error ("alreadyclosed", obj);
		else if (close_container (obj))
		  ;   /* closed: state set + contents re-hidden */
		else
		  display_error ("cantclose", obj);
	      }
	    else if (key == "look in")
	      {
		/* "look in", "look inside" and "look on" are not verbs of
		 * their own in Quest: ExecLook strips whichever of the
		 * three the player typed and then describes the object
		 * exactly as "look at" does (V4Game.Part2.cs:4885-4922).
		 * From 3.91 that description ends in the object's contents,
		 * which is where a container's listing comes from -- there
		 * is no look-inside listing anywhere in the engine.  geas
		 * had one, in wording of its own ("Inside you see a and
		 * b.", "It is closed.", "It is empty."), and printed no
		 * description at all.
		 *
		 * "search" is geas's own third spelling and stays on this
		 * handler; the two corpus games that give it a meaning
		 * write `action <search>`, which the lookup above answers
		 * before ever reaching here. */
		do_look (obj);
	      }
	    else if (key == "wear")
	      {
		/* Quest's built-in clothing: wearing sets the "worn" property
		 * (games test `property <obj; worn>`) and holds the item.  geas
		 * has no clothing library and the type's action <wear> may live
		 * in an unavailable .qlb, so apply the effect here and print the
		 * object's wearmessage. */
		set_obj_property (obj, "worn");
		move (obj, "inventory");
		string wm;
		if (get_obj_property (obj, "wearmessage", wm) && wm != "")
		  print_formatted (wm);
		else
		  print_formatted ("You put it on.");
	      }
	    else
	      display_error ("defaultverb", obj);
	    /* Below ASL 3.91 only -- from 3.91 the open/close verbs return above,
	     * out of Quest's own ExecOpenClose.  Opening/closing a container
	     * records its state even when a custom action handled the verb, so a
	     * scripted "open <msg>" still updates availability of the contents. */
	    if (key == "open" && has_obj_property (obj, "container") &&
		!has_obj_property (obj, "opened"))
	      { set_obj_property (obj, "not closed"); set_obj_property (obj, "opened"); }
	    else if (key == "close" && has_obj_property (obj, "container") &&
		     !has_obj_property (obj, "closed"))
	      { set_obj_property (obj, "not opened"); set_obj_property (obj, "closed"); }
	    /* Wearing -- through any handler, including a type's action <wear> that
	     * geas ran above -- sets the "worn" property and holds the item, as in
	     * Quest's clothing library.  The library's own command layer (which set
	     * "worn") isn't bundled, so the type action alone wouldn't, yet games
	     * test `property <obj; worn>` (e.g. KQ5's cloak/amulet). */
	    if (key == "wear")
	      { set_obj_property (obj, "worn"); move (obj, "inventory"); }
	    return true;
	  }
  }

  if ((match = match_command (cmd, "look at #@object#")) ||
      (match = match_command (cmd, "look #@object#")))
    {
      if (!dereference_vars (match.bindings, is_internal))
	return true;

      /* ExecLook hands every spelling of the verb to DoLook, which is what
       * marks the object "seen" -- and, from 3.91, follows its description with
       * its contents. */
      do_look (match.bindings[0].var_text);
      return true;
    }

  if ((match = match_command (cmd, "examine #@object#")) ||
      (match = match_command (cmd, "x #@object#")))
    {
      if (!dereference_vars (match.bindings, is_internal))
	return true;

      /* ExecExamine answers with the object's own `examine` action or property
       * and stops there -- no "seen", and no contents listing.  Only an object
       * with nothing to say to EXAMINE falls through to DoLook, which is then
       * told to word its own failure as defaultexamine (V4Game.cs:5680-5717). */
      string object = match.bindings[0].var_text;
      if (!dispatch_obj_verb (object, "examine"))
	do_look (object, true);
      return true;
    }

  if ((match = match_command (cmd, "read #@object#")))
    {
      if (!dereference_vars (match.bindings, is_internal))
	return true;

      string object = match.bindings[0].var_text;
      set_obj_property (object, "seen");
      /* "read" dispatches to the object's read action/property if defined,
       * otherwise it behaves like examine and falls back to look.  Many Quest
       * games gate content (and progress flags) behind action <read>. */
      if (!dispatch_obj_verb (object, "read") &&
	  !dispatch_obj_verb (object, "examine") &&
	  !dispatch_obj_verb (object, "look"))
	display_error ("defaultexamine", object);
      return true;
    }

  if ((match = match_command (cmd, "look")))
    {
      look();
      return true;
    }

  /* GIVE has two forms, and ExecGive tries them in this order: split on the
   * first " to " and, only if the input has none, on the first " the " -- with
   * the recipient in front of it and the item after (V4Game.Part2.cs:4618-4643).
   * "give the gatekeeper the document of approval" is the phrasing QDK's own
   * hint text hands to players, and in Tai & David's Amazing Maze
   * (TDAMAZINGMAZE.cas) it is the only way to hand over the document the gate
   * is locked behind.  geas knew the "X to Y" form only.
   *
   * The recursive matcher grows a binding from the shortest text up, so the
   * split it finds is the leftmost one -- the same place InStr would put it. */
  bool give_reversed = false;
  if (!(match = match_command (cmd, "give #@first# to #@second#")))
    {
      match = match_command (cmd, "give #@second# the #@first#");
      give_reversed = true;
    }
  if (match)
    {
      /* Two blanks, two scopes, as in "use X on Y": the item is looked for in
       * the inventory and the recipient in the current room, and each miss has
       * its own refusal -- NoItem for the item, BadCharacter for the recipient
       * (ExecGive, V4Game.Part2.cs:4673-4735). */
      vector<match_binding>
	item_b (1, match.bindings[give_reversed ? 1 : 0]),
	char_b (1, match.bindings[give_reversed ? 0 : 1]);
      vector<string> inv_only (1, "inventory"), room_only (1, state.location);
      if (!dereference_vars (item_b, inv_only, is_internal, true))
	{
	  display_error ("noitem", item_b[0].var_text);
	  return true;
	}
      if (!dereference_vars (char_b, room_only, is_internal, true))
	{
	  display_error ("badcharacter", char_b[0].var_text);
	  return true;
	}
      string script, first = item_b[0].var_text, second = char_b[0].var_text;
      if (!is_held (first))
	display_error ("noitem", first);
      else if (get_obj_action (second, "give " + first, script))
	run_script_as (second, script);  /* run the action in the recipient's
	  context; the old run_script(second, script) wrongly executed the
	  object name "second" as the script (two-arg overload), so giving any
	  item to an NPC with a give <item> action did nothing. */
      else if (get_obj_action (first, "give to " + second, script))
	run_script_as (first, script);
      else if (get_obj_action (second, "give anything", script))
	{
	  set_svar ("quest.give.object.name", first);
	  run_script_as (second, script);
	}
      else if (get_obj_action (first, "give to anything", script))
	{
	  set_svar ("quest.give.object.name", second);
	  run_script_as (first, script);
	}
      else
	{
	  string tmp;
	  /* The refusal fills the quest.error.* family as ExecGive does
	   * (V4Game.Part2.cs:4788-4795, and 4835-4838 on the ASL2 side):
	   * charactername is the recipient's object name, gender and article
	   * come from the two objects. */
	  set_svar ("quest.error.charactername", second);
	  if (!get_obj_property (second, "gender", tmp))
	    tmp = "it";
	  set_svar ("quest.error.gender", pcase (tmp));   /* capitalised, ibid. 4790 */
	  if (!get_obj_property (first, "article", tmp))
	    tmp = "it";
	  set_svar ("quest.error.article", tmp);
	  display_error ("itemunwanted");
	}
      return true;
    }

  /* Quest's standard "remove" verb, e.g. "remove book of keys from odd book
   * shelf": from 3.91 it is the container half of "put", handled by the
   * *container*, not by the item (ExecAddRemove, V4Game.cs:2369-2624).  Without
   * a "from", the container is whatever the item's parent property says.
   *
   * geas keeps its own, older reading -- run the named object's own "remove"
   * action/property -- as a fallback, because that is the only way several
   * games in the corpus can be played at all: Shiversword Tales and Bear
   * Campsite hang `action <remove>` on the clothes and the tangled string the
   * player is told to remove, which real Quest would answer with "You can't
   * remove that." since neither is in a container. */
  if ((match = match_command (cmd, "remove #@first# from #@second#")))
    {
      if (!dereference_vars (match.bindings, is_internal))
	return true;
      string first = match.bindings[0].var_text,
	second = match.bindings[1].var_text;
      if (has_obj_property (second, "container") ||
	  has_obj_property (second, "surface"))
	remove_from_container (first, second);
      else if (!dispatch_obj_verb (first, "remove") &&
	       !dispatch_obj_verb (second, "remove"))
	display_error ("defaultverb", first);
      return true;
    }
  if ((match = match_command (cmd, "remove from #@object#")) ||
      (match = match_command (cmd, "remove #@object#")))
    {
      if (!dereference_vars (match.bindings, is_internal))
	return true;
      string obj = match.bindings[0].var_text, parent;
      if (get_obj_property (obj, "parent", parent) && parent != "")
	remove_from_container (obj, parent);
      else if (!dispatch_obj_verb (obj, "remove"))
	display_error ("defaultverb", obj);
      return true;
    }

  if ((match = match_command (cmd, "use #@first# on #@second#")) ||
      (match = match_command (cmd, "use #@first# with #@second#")))
    {
      /* Quest resolves the two blanks in different places, and not in the one
       * combined scope the other commands use:
       *
       *     id = await Disambiguate(useItem, "inventory", ctx);
       *     ...
       *     useOnObjectId = await Disambiguate(useOn, _currentRoom, ctx);
       *     if (useOnObjectId > 0) foundUseOnObject = true;
       *     else { useOnObjectId = await Disambiguate(useOn, "inventory", ctx); ... }
       *                                ' ExecUse, V4Game.Part2.cs:5289, 5376-5390
       *
       * The item comes from the inventory alone, and the thing it is used on from
       * the room first, falling back to the inventory only if the room has no
       * such object.  Searching both places at once instead turns a name that
       * exists in both into a disambiguation menu Quest never shows -- which
       * silently eats the next line of a walkthrough.  "Something 'Bout A Hex"
       * needs the item half: it pours three drinks that all answer to
       * "Jim-n-Ginger" and leaves the ones you have not been handed yet sitting
       * in the bar room, so USE JIM-N-GINGER has exactly one candidate in
       * Quest and three here. */
      vector<match_binding> item_b (match.bindings.begin (),
				    match.bindings.begin () + 1);
      vector<match_binding> target_b (match.bindings.begin () + 1,
				      match.bindings.end ());
      vector<string> inv_only (1, "inventory");
      if (!dereference_vars (item_b, inv_only, is_internal, true))
	{
	  display_error ("noitem", item_b[0].var_text);
	  return true;
	}
      string script, first = item_b[0].var_text, second;
      {
	vector<string> room_only (1, state.location);
	second = get_obj_name (target_b[0].var_text, room_only, is_internal);
	if (second == "!")
	  {
	    if (!dereference_vars (target_b, inv_only, is_internal, true))
	      {
		display_error ("badthing", target_b[0].var_text);
		return true;
	      }
	    second = target_b[0].var_text;
	  }
	else
	  last_object = second;   /* as dereference_vars would have done */
      }
      if (!is_held (first))
	display_error ("noitem", first);
      else if (get_obj_action (second, "use " + first, script))
	{
	  run_script_as (second, script);
	}
      else if (get_obj_action (first, "use on " + second, script))
	{
	  run_script_as (first, script);
	}
      /* The catch-all forms tell the script which object filled the blank
       * through a string variable, and its name is "quest.use.object.name" --
       * ExecUse sets exactly that, to the *other* object's name, in both cases
       * (V4Game.Part2.cs:5439-5460).  geas used to call it "quest.use.object",
       * which no game ever reads.  "ChristmaKwanzakkah" is the one that cares:
       * every crowd and every boss is a `use anything` whose script tests
       * `#(quest.use.object.name):weapon#` against a threshold, so with the
       * wrong name each weapon reads as having no power at all and the crowds
       * "ignore you completely" whatever you wave at them. */
      else if (get_obj_action (second, "use anything", script))
	{
	  set_svar ("quest.use.object.name", first);
	  run_script_as (second, script);  /* was run_script(second, script),
	    which executed the object name instead of the action. */
	}
      else if (get_obj_action (first, "use on anything", script))
	{
	  set_svar ("quest.use.object.name", second);
	  run_script_as (first, script);
	}
      else
	display_error ("defaultuse");

      return true;
    }

  if ((match = match_command (cmd, "use #@first#")))
    {
      /* Inventory only -- see the "use X on Y" branch above for why. */
      vector<string> inv_only (1, "inventory");
      if (!dereference_vars (match.bindings, inv_only, is_internal, true))
	{
	  display_error ("noitem", match.bindings[0].var_text);
	  return true;
	}
      string tmp, obj = match.bindings[0].var_text;
      if (!is_held (obj))
	display_error ("noitem", obj);
      /* A "use <obj>" handler on the current room overrides the object's own
       * use action: a key whose generic use says "no door" still unlocks the
       * door in the room that defines use <key>.  This mirrors the "use X on Y"
       * path, which already checks the target's "use X" action -- but Quest
       * dropped it in ASL 410:
       *
       *     if (ASLVersion < 410) { ... foundUseScript = true; ... }
       *     if (!foundUseScript) { useScript = _objs[id].Use; ... }
       *                                       ' ExecUse, V4Game.Part2.cs:5348-5368
       *
       * so from 410 on a room's bare "use <obj>" line is never consulted and the
       * object's own use action always runs.
       *
       * Below ASL 280 the whole thing is a different piece of code, and there an
       * object's own use action is not in the chain at all: the room's use list
       * is searched, then a "use <obj>" line in the *game* block, and a miss is
       * the defaultuse error (ExecUse, V4Game.Part2.cs:5472-5497).  The game
       * block is read straight out of the block for the same reason the
       * description fallback above is -- readfile does not rewrite game-block
       * lines into "action <...>" form, so get_obj_action never sees them.  Dom's
       * "MagicSword Part 1" is unwinnable without it: its Immune Potion, healing
       * Potion, Sword and Staff all declare their use scripts in the game block,
       * and DRINK IMMUNE POTION is `exec <use immune potion>`, so with no
       * fallback the potion cannot be drunk and Hallucination Forest -- which
       * kills anyone entering without %Immune% set -- is sealed off. */
      else if (asl_version_ < 410 &&
	       get_room_action (state.location, "use " + obj, tmp))
	run_script_as (state.location, tmp);
      else if (asl_version_ < 280)
	{
	  if (find_game_block_line ("use", obj, tmp))
	    run_script_as ("game", tmp);
	  else
	    display_error ("defaultuse", obj);
	}
      else if (!dispatch_obj_verb (obj, "use"))
	display_error ("defaultuse", obj);
      return true;
    }

  /* Quest's standard "put #object# in/on #object#" (place into a container or
   * onto a surface): the target's add machinery decides what happens, and if
   * the item is actually moved into it (its parent becomes the target), the
   * closed-container scope rules then apply.  Games with richer logic define
   * their own `command <put ...>`, tried before this. */
  {
    bool put_matched = false, put_on = false;
    if ((match = match_command (cmd, "put #@first# on #@second#")) ||
	(match = match_command (cmd, "put #@first# onto #@second#")))
      put_matched = put_on = true;
    else if ((match = match_command (cmd, "put #@first# in #@second#"))   ||
	     (match = match_command (cmd, "put #@first# into #@second#")) ||
	     (match = match_command (cmd, "put #@first# inside #@second#")))
      put_matched = true;
    if (put_matched)
      {
	if (!dereference_vars (match.bindings, is_internal))
	  return true;
	string script, addtext, first = match.bindings[0].var_text,
	  second = match.bindings[1].var_text;
	if (!is_held (first))
	  display_error ("noitem", first);
	/* A closed container refuses the put before any of its add machinery is
	 * consulted: Quest checks `surface || opened` and answers CantPut ahead
	 * of the action/property lookups below (ExecAddRemove,
	 * V4Game.cs:2563-2578; confirmed in the real 4.1.5 runner,
	 * putprobe.asl).  Barbarian's Sack is the game that cares -- its `add`
	 * script bags the pile of bones, but only an *opened* sack may take
	 * them, because the pedestal puzzle wants the bones sealed inside a
	 * closed sack and OPEN SACK / PUT / CLOSE SACK is the intended dance. */
	else if (has_obj_property (second, "container") &&
		 !has_obj_property (second, "surface") &&
		 !container_is_open (second))
	  display_error ("cantput");
	/* Quest's own container mechanic: the target's "add" action decides what
	 * happens, and learns which object it was handed through
	 * quest.add.object.name (DoAddRemove, V4Game.cs:2578-2597).  Such a script
	 * is responsible for the containment itself, with `add <child; parent>` --
	 * Shipwrecked's basket accepts only the *lit* lantern, and its script both
	 * moves it and raises the flag the tomb's archway checks. */
	else if (get_obj_action (second, "add", script))
	  {
	    set_svar ("quest.add.object.name", first);
	    run_script_as (second, script);
	  }
	else if (is_inside (first, second))
	  /* Refuse to nest a container inside itself: the parent chain is walked
	   * all over the engine (scope, visibility, the objects pane), and a cycle
	   * in it has no sane reading -- both objects would sit nowhere.  Quest's
	   * typelib never guards this (its own engine does not walk the chain), so
	   * there is no Quest wording to borrow; report it as an ordinary refused
	   * put, which keeps the vocabulary Quest does have and lets a game
	   * override the text with "error <defaultput; ...>" like any other. */
	  display_error ("defaultput", first);
	/* An "add <text>" *property* is the non-scripted half of the same mechanic:
	 * Quest prints the text and then performs the containment itself
	 * (V4Game.cs:2600-2636). */
	else if (get_obj_property (second, "add", addtext) && addtext != "")
	  {
	    print_formatted (addtext);
	    do_add_remove (first, second, true);
	  }
	else if (has_obj_property (second, "surface") ||
		 (has_obj_property (second, "container") && container_is_open (second)))
	  {
	    /* Default: relocate the item into/onto the target (real
	     * containment), then report it.  do_add_remove marks the target seen
	     * for us where Quest does, from ASL 4.10 on. */
	    do_add_remove (first, second, true);
	    string fdisp, sdisp;
	    if (!get_obj_property (first, "alias", fdisp))  fdisp = first;
	    if (!get_obj_property (second, "alias", sdisp)) sdisp = second;
	    print_formatted ("You put " + fdisp + (put_on ? " on " : " in ") + sdisp + ".");
	  }
	else
	  display_error ("defaultput", first);
	return true;
      }
  }

  if ((match = match_command (cmd, "take #@object#")) ||
      (match = match_command (cmd, "get #@object#")) ||
      (match = match_command (cmd, "pick up #@object#")))
    {
      /* ExecTake disambiguates in the current room and nowhere else
       * (V4Game.Part2.cs:5132), so a noun that names both something carried and
       * something lying here is the one lying here, with no menu.  Searching
       * both at once made The Wizard's fourth yellow bead -- the one just found
       * in the beds, with three already in hand -- a four-way question Quest
       * never asks, and the walkthrough's answer to it was read as the next
       * command.
       *
       * The inventory is consulted afterwards, only from 4.10, and only to pick
       * the wording of the refusal (ibid. 5143-5167). */
      vector<string> room_only (1, state.location);
      if (!dereference_vars (match.bindings, room_only, is_internal, true))
	{
	  string noun = match.bindings[0].var_text;
	  vector<match_binding> inv_b (match.bindings);
	  vector<string> inv_only (1, "inventory");
	  if (asl_version_ >= 410 &&
	      dereference_vars (inv_b, inv_only, is_internal, true))
	    display_error ("alreadytaken", inv_b[0].var_text);
	  else
	    display_error (asl_version_ >= 391 ? "badthing" : "baditem", noun);
	  return true;
	}

      string object = match.bindings[0].var_text;
      /* The same answer once more, for a name that reached this far while still
       * being in hand: a Quest 2.x item, which has no world object to scope and
       * so answers to its name from anywhere (see get_obj_name), or something
       * parented into a carried container, which "in hand" reaches into from
       * 3.91 on (see is_held).  TARDIS Escape parents a pocket to a worn
       * trenchcoat and overrides alreadytaken; geas used to run a whole take
       * turn, implied remove and all, on things the player already had. */
      if (is_held (object))
	{
	  display_error (asl_version_ >= 410   ? "alreadytaken"
			 : asl_version_ >= 391 ? "badthing"
			 :                       "baditem", object);
	  return true;
	}
      /* From 3.91 the object has to be reachable before anything else happens:
       * ExecTake asks PlayerCanAccessObject and, on a no, prints badtake with
       * the reason appended and returns without running any take script at all
       * (V4Game.Part2.cs:5176-5186).  We went straight to the script, so
       * Pyramid of Terror handed the player things out of shut containers. */
      if (asl_version_ >= 391)
	{
	  string why;
	  if (!player_can_access (object, why))
	    {
	      display_error_info ("badtake", object, why);
	      return true;
	    }
	}
      if (get_obj_action (object, "take", tok))
	{
	  GEAS_DBG << "Running script '" << tok << "' for take " << object << endl;
	  run_script_as (object, tok);
	}
      else if (get_obj_property (object, "take", tok))
	{
	  GEAS_DBG << "Found property '" << tok << "' for take " << object << endl;
	  /* Taking something out of a container removes it from that container
	   * first: Quest announces the implied step and then runs the "remove"
	   * command on the player's behalf, giving up on the take if the item is
	   * still parented afterwards (ExecTake, V4Game.Part2.cs:5178-5204).  Only
	   * a text/default take does this -- a scripted take is on its own -- and
	   * the container's own "remove" handling is what actually clears the
	   * parent property, so a chest that refuses (or is shut) keeps its
	   * contents.  QuestViva's ExecTake computes the parent and then never sets
	   * the isInContainer flag that guards this block, so nothing in the port
	   * ever reaches it; its ExecDrop shows the missing line
	   * (`if IsYes(GetObjectProperty("parent", ...)) then isInContainer = true`,
	   * V4Game.cs:5560-5570), and without the step Tim Hamilton's "The Things
	   * That Go Bump In The Night" cannot be finished -- its water pump wants
	   * four dead fuses out of the fuse box and a good one in, and its "add"
	   * script counts what is still parented to the box.  */
	  string parent;
	  if (get_obj_property (object, "parent", parent) && parent != "")
	    {
	      string art, pdisp;
	      if (!get_obj_property (object, "article", art))
		art = "it";
	      pdisp = displayed_name (parent);
	      print_formatted ("(first removing " + art + " from " + pdisp + ")");
	      /* The removal is a whole nested turn, not a call to the built-in
	       * handler: ExecTake hands "remove <name>" back to ExecCommand with
	       * AllowRealNamesInCommand set (V4Game.Part2.cs:5222-5223), so the
	       * game's own synonyms, beforeturn, `command' blocks and `verb' blocks
	       * all get a look at it before the built-in does, and the afterturn
	       * runs at the end of it.  A `verb <remove>' is what makes the
	       * difference visible, because a verb is looked up on the object being
	       * removed rather than on the container: Wizard's game block opens with
	       * `verb <remove> msg <You can't remove that.>' and hangs an
	       * `action <remove>' on its beakers, potions and beads, so its church
	       * offering basket refuses the Yellow Bead until a donation is made.
	       * Calling remove_from_container straight -- which is only the last of
	       * those steps, the container's own remove handling -- pocketed the
	       * bead anyway.  dontSetIt is the pronoun: an implied removal must not
	       * leave "it" pointing at the thing it removed (ibid. 4616-4622). */
	      const string saved_last_object = last_object;
	      run_turn ("remove " + object, true, false);
	      last_object = saved_last_object;
	      /* and the separator the nested ExecCommand ends with, as `exec' does
	       * (ibid. 4614): geas prints a turn's blank line at the top of
	       * run_command, so a turn run from inside one has to close itself. */
	      print_newline ();
	      string still;
	      if (get_obj_property (object, "parent", still) && still != "")
		return true;
	    }
	  if (tok != "")
	    print_formatted (tok);
	  else
	    display_error ("defaulttake", object);
	  string tmp;
	  move (object, "inventory");
	  /* A taken object is now in hand, so it's no longer hidden (it may have
	   * been a hidden item inside a container). */
	  set_obj_property (object, "not hidden");
	  if (get_obj_action (object, "gain", tmp))
	    /* Quest's ExecTake ends a text/default take with
		   * PlayerItem(item, true), which runs the object's GainScript
		   * (V4Game.Part2.cs:5205-5215, 6631-6634).  Note run_script_as, not
		   * run_script: the two-argument run_script is (script, &return value),
		   * so passing the object name as the first argument ran the *name* as
		   * a script and quietly dropped the gain script into the return
		   * value.  Enterprising in space hangs its entire Youhera chain off
		   * three bare `take` + `gain` objects and was unwinnable. */
		  run_script_as (object, tmp);
	  else if (get_obj_property (object, "gain", tmp))
	    print_formatted (tmp);
	}
      else
	{
	  GEAS_DBG << "No match found for take " << object << endl;
	  /* ExecTake sets only quest.error.article ahead of this refusal
	   * (V4Game.Part2.cs:5158), which display_error(obj) already covers. */
	  /* An object that can't be taken may carry a custom refusal message in a
	   * "noTake" property (the Quest type library sets one by default and lets
	   * objects override it, e.g. scenery); honour it before the generic
	   * badtake error. */
	  string notake;
	  if (get_obj_property (object, "noTake", notake) && notake != "")
	    print_formatted (notake);
	  else
	    display_error ("badtake", object);
	}
      return true;
    }

  if ((match = match_command (cmd, "drop #@object#")))
    {
      /* The inventory alone: ExecDrop's one Disambiguate is scoped to it, and a
       * miss is NoItem from 3.91 and BadDrop below (V4Game.cs:5531-5556).  So a
       * noun shared by a carried object and one already on the floor drops the
       * carried one rather than asking which. */
      vector<string> inv_only (1, "inventory");
      if (!dereference_vars (match.bindings, inv_only, is_internal, true))
	{
	  display_error (asl_version_ >= 391 ? "noitem" : "baddrop",
			 match.bindings[0].var_text);
	  return true;
	}
      string scr, obj = match.bindings[0].var_text;
      /* If the object is inside a container/surface, take it out first -- Quest
       * does this implicitly when you drop something still in a container. */
      {
	string p = obj_container (obj);
	if (p != "" &&
	    (has_obj_property (p, "container") || has_obj_property (p, "surface")))
	  {
	    string odisp, pdisp;
	    if (!get_obj_property (obj, "article", odisp)) odisp = obj;
	    if (!get_obj_property (p, "alias", pdisp)) pdisp = p;
	    print_formatted ("(first removing " + odisp + " from " + pdisp + ")");
	    do_add_remove (obj, p, false);
	  }
      }
      if (get_obj_action (obj, "drop", scr))
	{
	  run_script_as (obj, scr);
	  return true;
	}

      const GeasBlock *gb = gf.find_by_name ("object", obj);
      if (gb != NULL)
	{
	  string line, tok;
	  std::string::size_type c1, c2, script_begins;
	  for (uint i = 0; i < gb->data.size(); i ++)
	    {
	      line = gb->data[i];
	      tok = first_token (line, c1, c2);
	      /* CI, sub-keywords included: Quest reads all three with
	       * BeginsWith (ExecDrop, V4Game.cs:5577-5635). */
	      if (ci_equal (tok, "drop"))
		{
		  script_begins = c2;
		  tok = next_token (line, c1, c2);
		  if (ci_equal (tok, "everywhere"))
		    {
		      tok = next_token (line, c1, c2);
		      move (obj, state.location);
		      /* param_contents, not the raw token: print_eval does not strip
		       * the <angle brackets>, so the message was shown with them. */
		      if (is_param (tok))
			print_eval (param_contents (tok));
		      else
			gi->debug_print ("Expected param after drop everywhere in " + line);
		      return true;
		    }
		  if (ci_equal (tok, "nowhere"))
		    {
		      /* Advance to the message first: this tested is_param() on the
		       * keyword "nowhere" itself, which is never a param, so the
		       * refusal message was never printed. */
		      tok = next_token (line, c1, c2);
		      if (is_param (tok))
			print_eval (param_contents (tok));
		      else
			gi->debug_print ("Expected param after drop nowhere in " + line);
		      return true;
		    }
		  run_script_as (obj, line.substr (script_begins));
		  return true;
		}
	    }
	}
      move (obj, state.location);
      display_error ("defaultdrop", obj);
      return true;
    }

  if ((match = match_command (cmd, "speak to #@object#")) ||
      (match = match_command (cmd, "speak #@object#")) ||
      (match = match_command (cmd, "talk to #@object#")) ||
      (match = match_command (cmd, "talk #@object#")))
    {
      if (!dereference_vars (match.bindings, is_internal))
	return true;
      string obj = match.bindings[0].var_text;
      /* Action first, then property -- the order Quest's ExecSpeak uses (it
	 scans the object's actions for "speak", then its properties, and only
	 then falls back to the default message).  This used to look at actions
	 alone, so the common `speak <He jests with you.>` form -- which the
	 loader stores as a *property* -- printed "He says nothing." for every
	 character in a game.  Red Sauce Monday is all of them but one. */
      if (!dispatch_obj_verb (obj, "speak"))
	display_error ("defaultspeak", obj);
      return true;
    }

  if (cmd == "exit" || cmd == "out" || cmd == "go out")
    {
      /* "out" is the one direction Quest stores as both a destination and a
       * script: before 4.10 the room parser fills Out.Text from the parameter
       * *and* Out.Script from whatever follows the '>'
       * (V4Game.Part2.cs:1018-1030), and GoDirection runs the script when there
       * is one and treats the text as a room name when there is not
       * (ibid. 6324-6356).  A "create exit out <src; dest>" assigns only
       * Out.Text (V4Game.cs:5482-5485), so a static script still wins over it --
       * hence the room block is checked for a script first and exit_dest ()
       * consulted second.  exit_dest () returns the parameter, never a script,
       * for a pre-4.10 "out"; from 4.10 on "out" is an ordinary exit tag and it
       * may return either. */
      /* A lock beats both, as it does for every other direction: RoomExit.Go
       * tests IsLocked before it looks at the script (RoomExit.cs:240-259), and
       * "out" is parsed by the same code that gives north/south their "locked"
       * keyword (RoomExits.cs:143-147).  Only the directional loop below
       * checked this, so Exits of The World's `out locked <Skarro; ...>` -- the
       * one exit that is supposed to keep you off the Dalek homeworld until you
       * have a ship -- let the player walk straight through it. */
      if (exit_locked (state.location, "out"))
	{
	  string lm = exit_lock_message (state.location, "out");
	  if (lm != "")
	    print_formatted (lm);
	  else
	    display_error ("locked");
	  return true;
	}
      const GeasBlock *gb = gf.find_by_name ("room", state.location);
      if (gb == NULL)
	{
	  gi->debug_print ("Bad room");
	  return true;
	}
      string line = "", tok;
      std::string::size_type c1, c2=0;
      /* The last "out" line wins: each one overwrites Out in the loader. */
      for (size_t i = 0; i < gb->data.size(); i ++)
	{
	  if (first_token (gb->data[i], c1, c2) == "out")
	    line = gb->data[i];
	}

      if (line != "")
	{
	  c1 = line.find ('<');
	  c2 = (c1 == string::npos) ? string::npos : line.find ('>', c1);

	  if (c1 == string::npos || c2 == string::npos)
	    gi->debug_print ("Bad out line: " + line);
	  else
	    {
	      string script = trim (line.substr (c2 + 1));
	      if (script != "")
		{
		  run_script_as (state.location, script);
		  return true;
		}
	    }
	}

      /* No script, so this is a plain destination -- which may have been made
       * at run time, so it has to come from exit_dest ().  Reading the room
       * block alone left Riddle Run's doors, every one of them created by
       * "create exit out <Room n; Room n+1>", permanently shut. */
      bool is_script = false;
      tok = exit_dest (state.location, "out", &is_script);
      if (tok == "")
	/* PlayerError.DefaultOut is GoDirection's, and GoDirection is pre-4.10
	 * code.  From 4.10 on "out" is dispatched with every other exit by
	 * RoomExits.ExecuteGo, which cannot find one and answers with BadPlace,
	 * "You can't go there." (RoomExits.cs:291-296) -- the same refusal a
	 * missing "north" gets. */
	display_error (asl_version_ >= 410 ? "badplace" : "defaultout");
      else if (is_script)
	run_script_as (state.location, tok);
      else
	{
	  string dest, unused_prefix;
	  split_exit_dest (tok, dest, unused_prefix);
	  goto_room (dest);
	}
      return true;
    }

  for (size_t i = 0; i < ARRAYSIZE(dir_names); i ++)
    if (cmd == dir_names[i] || cmd == "go " + dir_names[i] ||
	cmd == short_dir_names[i] || cmd == "go " + short_dir_names[i])
      {
	bool is_script = false;
	if ((tok = exit_dest (state.location, dir_names[i], &is_script)) == "")
	  {
	    /* PlayerError.BadPlace in both eras: GoDirection's final else
	     * (V4Game.Part2.cs:6336) and 4.10's ExecuteGo (RoomExits.cs:295).
	     * Via display_error, so a game's "error <badplace; ...>" line
	     * overrides it, which the old hardcoded string never honoured. */
	    display_error ("badplace");
	    return true;
	  }
	/* The lock is tested before the script, not instead of it: RoomExit.Go
	 * refuses a locked exit whatever it carries (RoomExit.cs:240-259). */
	if (exit_locked (state.location, dir_names[i]))
	  {
	    string lm = exit_lock_message (state.location, dir_names[i]);
	    if (lm != "")
	      print_formatted (lm);
	    else
	      display_error ("locked");
	    return true;
	  }
	if (is_script)
	  run_script_as (state.location, tok);
	else
	  {
	    string dest, unused_prefix;
	    split_exit_dest (tok, dest, unused_prefix);
	    goto_room (dest);
	  }
	return true;
      }

  if ((match = match_command (cmd, "go to #@room#")) ||
      (match = match_command (cmd, "go #@room#")))
    {
      if (match.bindings.size() != 1) { report_unsupported ("unexpected binding count for 'go to' command"); return true; }
      string destination = match.bindings[0].var_text;
      /* Quest also strips a leading "the " and retries (GoToPlace). */
      string alt = destination;
      if (lcase (destination).rfind ("the ", 0) == 0)
	alt = trim (destination.substr (4));
      for (size_t i = 0; i < current_places.size (); i ++)
	{
	  bool scripted = (current_places[i].size () == 5);
	  /* Quest's PlaceExist compares against one target: the destination
	   * room name for a scripted place (e.g. `exec <go to ROOMNAME>`), or
	   * the room's display alias for a plain place. */
	  const string &target = scripted ? current_places[i][3]
					   : current_places[i][2];
	  if (ci_equal (destination, target) || ci_equal (alt, target))
	    {
	      /* Same test, in the same place, as for a directional exit: RoomExit
	       * .Go refuses a locked exit whatever it carries, script included
	       * (RoomExit.cs:240-259).  A place is keyed on its destination room
	       * -- see exit_declared_locked -- and can only be locked from 4.10,
	       * where a room's exits are exit objects. */
	      const string &dest = current_places[i][3];
	      if (asl_version_ >= 410 && exit_locked (state.location, dest))
		{
		  string lm = exit_lock_message (state.location, dest);
		  if (lm != "")
		    print_formatted (lm);
		  else
		    display_error ("locked");
		  return true;
		}
	      if (scripted)
		{
		  /* Copy first: the script may goto, which reallocates
		   * current_places out from under us. */
		  string scr = current_places[i][4];
		  run_script_as (state.location, scr);
		}
	      else
		goto_room (current_places[i][3]);
	      return true;
	    }
	}
      display_error ("badplace", destination);
      return true;
    }

  if (ci_equal (cmd, "inventory") || ci_equal (cmd, "inv") || ci_equal (cmd, "i"))
    {
      /* Quest prints "You are carrying:", a line break, and then the whole
	 inventory as one sentence: each entry is prefix + name + suffix, the
	 entries are joined with ", ", the last comma becomes " and", the first
	 letter is capitalised and the line is stopped with a full stop
	 (V4Game.Part2.cs:4496-4561).  An empty inventory is a sentence of its
	 own.  The last comma is found by scanning the built string, as Quest
	 does, so an object whose own name contains a comma moves the join.

	 The capitalisation is applied to the assembled string *including* its
	 markup, so when the first entry has no prefix the character it reaches
	 is the "|" of the name's own bold tag and the name itself stays as the
	 game spelt it.  Below 2.80 Quest lists bare items, with neither the
	 bold nor the affixes -- and there the capital does land on the name. */
      string inv;
      for (const auto &i: get_inventory ())
	{
	  string entry = i[0], prefix, suffix;
	  if (asl_version_ >= 280)
	    {
	      entry = "|b" + entry + "|xb";
	      /* A bare Quest 2.x item has no object record to carry affixes. */
	      if (i.size () > 2)
		{
		  if (get_obj_property (i[2], "prefix", prefix))
		    entry = prefix + " " + entry;
		  if (get_obj_property (i[2], "suffix", suffix))
		    entry = entry + " " + suffix;
		}
	    }
	  inv += entry + ", ";
	}
      if (inv == "")
	print_formatted ("You are not carrying anything.");
      else
	{
	  inv.erase (inv.size () - 2);
	  std::string::size_type lastcomma = inv.rfind (',');
	  if (lastcomma != string::npos)
	    inv = inv.substr (0, lastcomma) + " and" + inv.substr (lastcomma + 1);
	  print_formatted ("You are carrying:|n" + pcase (inv) + ".");
	}
      return true;
    }

  /* Quest's built-in "wait" -- pass a turn.  Games that want a custom message
   * define `command <wait>` (tried earlier); this is the engine default. */
  if (ci_equal (cmd, "wait"))
    {
      display_error ("defaultwait");
      return true;
    }

  /* "verbs <object>" -- list the verbs available for an object, mirroring the
   * right-click verb context menu in the original Windows Quest 4.  ASL4 has no
   * single per-object verb list, so object_verbs() synthesises one from the
   * sources the engine actually dispatches through: the universal verbs, the
   * built-in multi-synonym verbs the object responds to, and any global `verb`
   * declaration it handles.  (Quest's own pane menu is a fixed four-entry list
   * that ignores the object entirely -- _listVerbs, V4Game.Part2.cs:6458-6471 --
   * so this is deliberately richer, but it must never offer a verb the parser
   * would reject; see the note in object_verbs.) */
  if ((match = match_command (cmd, "verbs #@object#")))
    {
      if (!dereference_vars (match.bindings, is_internal))
	return true;
      v2string verbs = object_verbs (match.bindings[0].var_text);
      print_formatted ("You can:");
      for (const vector<string> &v: verbs)
	{
	  if (v.empty ())
	    continue;
	  print_normal (v[0]);
	  print_newline();
	}
      return true;
    }

  /* Bare "verbs" names no object to list, so say what the command wants
     rather than letting it fall through to "I don't understand", which is
     what the Quest 5 frontend's VERBS does. */
  if (ci_equal (cmd, "verbs"))
    {
      print_formatted ("Type VERBS followed by an object name, e.g. VERBS lamp.");
      return true;
    }

  if (ci_equal (cmd, "help"))
    {
      print_formatted ("|b|cl|s14Quest Quick Help|xb|cb|s00|n|n|cl|bMoving|xb|cb Press the direction buttons in the 'Compass' pane, or type |bGO NORTH|xb, |bSOUTH|xb, |bE|xb, etc. |xnTo go into a place, type |bGO TO ...|xb . To leave a place, type |bOUT, EXIT|xb or |bLEAVE|xb, or press the '|crOUT|cb' button.|n|cl|bObjects and Characters|xb|cb Use |bTAKE ...|xb, |bGIVE ... TO ...|xb, |bTALK|xb/|bSPEAK TO ...|xb, |bUSE ... ON|xb/|bWITH ...|xb, |bLOOK AT ...|xb, etc.|n|cl|bExit Quest|xb|cb Type |bQUIT|xb to leave Quest.|n|cl|bMisc|xb|cb Type |bABOUT|xb to get information on the current game. Type |b#HELP|xb to list the SAVE, RESTORE, UNDO and other system commands.");
      return true;
    }

  if (ci_equal (cmd, "about"))
    {
      const GeasBlock *gb = gf.find_by_name ("game", "game");
      if (gb == NULL)
	return true;
      GEAS_DBG << *gb << endl;
      string line, tok;
      std::string::size_type c1, c2;
      line = gb->data[0];
      tok = first_token (line, c1, c2); // game
      tok = next_token (line, c1, c2); // name
      tok = next_token (line, c1, c2); // <whatever>
      if (is_param (tok))
	print_formatted ("Game name: " + eval_param (tok));

      /* Print each "game <key> <param>" line under its label.  Kept as one
       * helper instead of four near-identical loops; the fixed call order
       * preserves the original Version/Author/Copyright/Info output order. */
      auto emit = [&] (const char *key, const string &label) {
	std::string::size_type d1, d2;
	string t;
	for (const string &line: gb->data)
	  /* CI: FindStatement/BeginsWith, as in get_banner. */
	  if (ci_equal (first_token (line, d1, d2), "game") &&
	      ci_equal (next_token (line, d1, d2), key) &&
	      is_param (t = next_token (line, d1, d2)))
	    print_formatted (label + eval_param (t));
      };
      /* "Version:", with the colon: ShowGameAbout prints |bVersion:|xb
       * (V4Game.Part2.cs:3648), like the other three labels. */
      emit ("version",   "Version: ");
      emit ("author",    "Author: ");
      emit ("copyright", "Copyright: ");
      emit ("info",      "");

      return true;
    }

  /* SAVE is a built-in command of the engine's, not of the game's
   * (V4Game.Part2.cs:4468), even though the dialog it opens belongs to the
   * frontend -- which is why a typed SAVE never gets this far: geasglk
   * intercepts it (handle_saverestore_command, geasglk.cc:316) and so does the
   * walkthrough runner's --save-scum.  What does get here is `exec <save;
   * normal>`, the "do you want to save?" that several corpus games offer from
   * a `choice`, and the engine has to recognise it or the turn ends in
   * "I don't understand your command". */
  if (ci_equal (cmd, "save"))
    return true;

  if (ci_equal (cmd, "quit"))
    {
      is_running_ = false;
      return true;
    }

  return false;
}
