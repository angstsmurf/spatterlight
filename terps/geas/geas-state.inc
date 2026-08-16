// geas-state.inc -- Whole-game state: restart, undo, save and load, game
// start-up (set_game), and the regen_var_* refreshes that rebuild the
// variable-backed object and exit lists after state changes.
//
// Split out of geas-runner.cc, which #includes it (same single-TU layout
// as the quest5/aslx-*.inc units); the code is unchanged from the split.

bool geas_implementation::timer_will_fire ()
{
  /* tick_timers() counts a timer up and fires it in the same tick, so the next
   * tick is the firing one once one more would reach the interval.  A timer
   * whose bypass is still up spends that tick doing nothing. */
  for (const auto &t: state.timers)
    if (t.is_running && !t.bypass && t.elapsed + 1 >= t.interval)
      return true;
  return false;
}

void geas_implementation::restart ()
{
  /* set_game re-reads and re-initialises everything from scratch (fresh
   * state, start room, intro, startscript).  is_running_ has to be raised
   * *before* that, because run_script is a no-op once the game has finished
   * (see the guard there) and restarting after a death or a win is legal. */
  is_running_ = true;
  set_game (story_filename);
  is_running_ = (gf.blocks.size() != 0);
}

bool geas_implementation::undo ()
{
  if (is_running_)
    {
      /* Normal undo: the last command pushed its state, so drop it and
       * restore the one before. */
      if (undo_buffer.size() < 2)
	return false;
      undo_buffer.pop();
    }
  else
    {
      /* After the game ended (death/win): the fatal turn was never pushed, so
       * the top of the buffer is already the last good state.  Resume play. */
      if (undo_buffer.is_empty())
	return false;
      is_running_ = true;
    }
  state.restore_undo (undo_buffer.peek());
  state.running = true;
  print_formatted ("Undone.");
  /* Rebuild the cached views of the restored state and redescribe the room. */
  regen_var_room ();
  regen_var_dirs ();
  regen_var_look ();
  objs_vars_dirty_ = true;
  look ();
  return true;
}

std::string geas_implementation::get_location ()
{
  string name;
  if (!get_room_property (state.location, "alias", name))
    name = state.location;
  return name;
}

void geas_implementation::run_game_event (const string &keyword)
{
  const GeasBlock *gb = gf.find_by_name ("game", "game");
  if (gb == NULL)
    return;
  std::string::size_type c1, c2;
  for (const string &line: gb->data)
    if (first_token (line, c1, c2) == keyword)
      run_script_as ("game", line.substr (c2));
}

std::string geas_implementation::save_state (bool run_hooks)
{
  /* Quest's "beforesave" runs just before the state is written, letting the
   * game stash data (e.g. the player's location) into saved variables/objects.
   * Skipped for transparent internal snapshots (run_hooks == false). */
  if (run_hooks)
    run_game_event ("beforesave");
  state.running = is_running_;
  return serialize_game (story_filename, state);
}

/* Re-register the definition alias of every object `clone` has made this game.
 * The records themselves are serialized with the rest of the state, but a
 * clone's *definition* is an alias in the GeasFile (see register_clone), which
 * is built from the story file alone -- so without this a restored clone would
 * come back as a nameless record with none of the source's look text, verbs or
 * properties.  The trail is the "!clones" pseudo-object's property log, written
 * by the clone handler and carried in the save like any other property. */
void geas_implementation::restore_clones ()
{
  const vector<size_t> *v = state.prop_records ("!clones");
  if (v == NULL)
    return;
  for (size_t idx: *v)
    {
      const string &data = state.props[idx].data;
      if (data.compare (0, 11, "properties ") != 0)
	continue;
      string body = data.substr (11);
      /* "newname=source": split on the last '=', since a source name may
       * contain one and a name we wrote ourselves will not end with one. */
      string::size_type eq = body.rfind ('=');
      if (eq == string::npos)
	continue;
      gf.register_clone (body.substr (eq + 1), body.substr (0, eq));
    }
}

bool geas_implementation::load_state (const string &data, bool run_hooks)
{
  GeasState newstate;
  string gamename;
  if (!deserialize_game (data, gamename, newstate))
    return false;
  state = newstate;
  is_running_ = state.running = true;
  restore_clones ();
  if (run_hooks)
    load_method_ = "loaded";   /* a real restore -> $loadmethod$ = "loaded" */
  /* Rebuild the cached views of the (now restored) current room. */
  regen_var_room ();
  regen_var_dirs ();
  regen_var_look ();
  objs_vars_dirty_ = true;
  /* Quest's "onload" runs after the state is restored, letting the game rebuild
   * whatever it stashed in beforesave (it may even goto the saved room).
   * Skipped for transparent internal snapshots (run_hooks == false). */
  if (run_hooks)
    run_game_event ("onload");
  look ();
  return true;
}

std::string geas_implementation::save_undo_history ()
{
  return serialize_undo_history (undo_buffer.contents ());
}

bool geas_implementation::load_undo_history (const string &data)
{
  std::vector<UndoState> states;
  if (!deserialize_undo_history (data, states))
    return false;
  /* Rebuild the ring by pushing oldest-first, exactly as play would have. */
  undo_buffer = LimitStack<UndoState> (kUndoLevels);
  for (UndoState &u : states)
    undo_buffer.push (u);
  return true;
}

void geas_implementation::set_game (const string &s)
{
  GEAS_DBG << "set_game (...)\n";
  story_filename = s;
  load_method_ = "normal";   /* a fresh game (reset on restart) */
  /* Seed the RNG once per game.  Real Quest randomises every run; geas used to
   * leave the generator unseeded, so any random fight played out identically
   * each time -- which left World's End's final fight permanently unwinnable.
   * Seed 0 means "true" randomness; GEAS_SEED overrides it for reproducible
   * testing, and so does the app's determinism mode. */
  {
    const char *envseed = getenv ("GEAS_SEED");
    if (envseed)
      set_erkyrath_random ((glui32) atoi (envseed));
#ifdef SPATTERLIGHT
    else if (gli_determinism)
      set_erkyrath_random (1234);
#endif
    else
      set_erkyrath_random (0);
  }
  try
    {
      gf = read_geas_file (gi, s);
      if (gf.blocks.size() == 0) {
        is_running_ = false;
	return;
      }
      std::string::size_type tok_start, tok_end;
      outputting = true;

      /* Learn the ASL version before anything can move the player: the game
       * block is walked again below for its other directives, but exits are
       * read version-dependently (see split_exit_dest) and a startscript can
       * already send us through one. */
      asl_version_ = 311;
      for (const auto &vline: gf.block ("game", 0).data)
	{
	  string vtok = lcase (first_token (vline, tok_start, tok_end));
	  if (vtok != "asl-version")
	    continue;
	  vtok = next_token (vline, tok_start, tok_end);
	  if (is_param (vtok))
	    asl_version_ = parse_int (param_contents (vtok));
	  break;
	}

      /* "define options" is a nameless block, like "define synonyms".  Only
       * "abbreviations" is acted on here; "panes" and "debug" are host-side
       * settings geas has no equivalent for. */
      if (const GeasBlock *opts = gf.find_by_name ("options", ""))
	for (const string &oline: opts->data)
	  {
	    string otok = lcase (first_token (oline, tok_start, tok_end));
	    if (otok == "abbreviations")
	      use_abbreviations_ =
		lcase (trim (next_token (oline, tok_start, tok_end))) != "off";
	  }

      state = GeasState (*gi, gf);

      /* The initial value of a variable is a *parameter*, not a literal.
       * SetUpDisplayVariables runs each `value <...>` through GetParameter as
       * it walks the game block (V4Game.Part2.cs:744), so a "#var#" or "%var%"
       * naming a variable defined earlier is substituted, and a "$rand(a;b)$"
       * is rolled -- once each, in definition order, before anything else in
       * the game has run.  GeasState stores the text raw, which left Kingdom's
       * 23 `value <$rand(...)$>` village statistics all sitting at zero and put
       * the RNG stream one draw per variable out of step with Quest's from the
       * first turn.
       *
       * It is one pass in source order, and each variable comes into existence
       * as it is reached, so a value can only see the ones above it: a
       * reference to a variable defined further down reads one that does not
       * exist yet -- "" for a string, Quest's undefined-numeric -32767 for a
       * number.  Hence the wipe first: GeasState has already created all of
       * them from the raw text, and leaving those in place would let a
       * not-yet-defined variable answer for itself.
       *
       * Rebuilt records rather than assignments, for the same reason:
       * set_svar/set_ivar would fire the variable's `onchange` script, and
       * Quest's setup does not run those. */
      for (size_t vi = 0; vi < gf.size ("variable"); vi ++)
	{
	  const string &vname = gf.block ("variable", vi).name;
	  for (size_t k = state.ivars.size (); k-- > 0; )
	    if (ci_equal (state.ivars[k].name, vname))
	      state.ivars.erase (state.ivars.begin () + k);
	  for (size_t k = state.svars.size (); k-- > 0; )
	    if (ci_equal (state.svars[k].name, vname))
	      state.svars.erase (state.svars.begin () + k);
	}
      for (size_t vi = 0; vi < gf.size ("variable"); vi ++)
	{
	  const GeasBlock &vb = gf.block ("variable", vi);
	  string raw;
	  bool is_numeric = true;
	  for (const string &vline: vb.data)
	    {
	      /* CI keywords, CS type value -- see static_svar_lookup. */
	      string vtok = first_token (vline, tok_start, tok_end);
	      if (ci_equal (vtok, "value"))
		{
		  vtok = next_token (vline, tok_start, tok_end);
		  if (is_param (vtok))
		    raw = param_contents (vtok);
		}
	      else if (ci_equal (vtok, "type"))
		is_numeric =
		  (next_token (vline, tok_start, tok_end) != "string");
	    }
	  string val = (raw.find_first_of ("#%$") == string::npos)
		       ? raw : eval_string (raw);
	  if (is_numeric)
	    {
	      IVarRecord iv;
	      iv.name = vb.name;
	      iv.set ((size_t) 0, strtod (val.c_str (), NULL));
	      state.ivars.push_back (iv);
	    }
	  else
	    {
	      SVarRecord sv;
	      sv.name = vb.name;
	      sv.set (0, val);
	      state.svars.push_back (sv);
	    }
	}

      state.running = true;

      /* Print the game's title/version/author banner once, here at the very
       * start, so it appears before the game's own opening output (intro text,
       * a start-room "please select an option" menu, etc.). */
      {
        string banner = get_banner ();
        if (!banner.empty ())
          print_formatted ("|n|i" + banner + "|xi|n");
      }

      auto gfData = gf.block("game", 0).data;
      for (auto &gline: gfData)
	{
	  const string &s = gline;
	  string tok = first_token (s, tok_start, tok_end);
	  /* Game-block directive keywords are case-insensitive (see run_script). */
	  tok = lcase (tok);
	  if (tok == "asl-version")
	    {
	      string ver = next_token (s, tok_start, tok_end);
	      if (!is_param(ver))
		{
		  gi->debug_print ("Version " + s + " has invalid version " +
				   ver);
		  continue;
		}
	      int vernum = parse_int (param_contents (ver));
	      if (vernum < 311 || vernum > 353)
		gi->debug_print ("Warning: Geas only supports ASL "
				 " versions 3.11 to 3.53");
	    }
	  else if (tok == "background")
	    {
	      tok = next_token (s, tok_start, tok_end);
	      if (!is_param (tok))
		gi->debug_print (nonparam ("background color", s));
	      else
		gi->set_background (param_contents(tok));
	    }
	  else if (tok == "default")
	    {
	      tok = next_token (s, tok_start, tok_end);
	      /* Sub-keywords are CI too: Quest matches the whole "default
	       * fontname" phrase with BeginsWith (V4Game.Part2.cs:661-669). */
	      if (ci_equal (tok, "fontname"))
		{
		  tok = next_token (s, tok_start, tok_end);
		  if (!is_param (tok))
		    gi->debug_print (nonparam ("font name", s));
		  else
		    gi->set_default_font (param_contents(tok));
		}
	      else if (ci_equal (tok, "fontsize"))
		{
		  tok = next_token (s, tok_start, tok_end);
		  if (!is_param (tok))
		    gi->debug_print (nonparam("font size", s));
		  else
		    gi->set_default_font_size (param_contents(tok));
		}
	    }
	  else if (tok == "foreground")
	    {
	      tok = next_token (s, tok_start, tok_end);
	      if (!is_param (tok))
		gi->debug_print (nonparam ("foreground color", s));
	      else
		gi->set_foreground (param_contents(tok));
	    }
	  else if (tok == "gametype")
	    {
	      /* Quest itself never reads this declaration: the only place
		 "gametype"/"multiplayer" appear in the runner is the keyword
		 table (quest.dat:98,100), and the game block is scanned solely
		 for "start " when play begins (V4Game.Part2.cc / .Part2.cs:
		 7992-7999).  A multiplayer game therefore loads and runs as an
		 ordinary single-player game from its declared start room.  We
		 used to throw here, which aborted the rest of this loop before
		 it reached "start" and left the player in room "" -- the game
		 came up with no location, listing every room as an object in
		 scope.  Ignore the declaration instead; the multiplayer-only
		 features (chat, challenges) simply never happen.  */
	      tok = next_token (s, tok_start, tok_end);
	      if (ci_equal (tok, "singleplayer") || ci_equal (tok, "multiplayer"))
		continue;
	      gi->debug_print ("Unexpected game type " + s);
	    }
	  else if (tok == "nodebug")
	    {
	    }
	  else if (tok == "start")
	    {
 	      tok = next_token (s, tok_start, tok_end);
	      if (!is_param (tok))
		gi->debug_print (nonparam ("start room", s));
	      else
		{
		  state.location = param_contents (tok);
		}
	    }
	}

      const GeasBlock &game = gf.block ("game", 0);
      GEAS_DBG << gf << endl;
      std::string::size_type c1, c2;
      string tok;

      /* Derive the hidden flags of everything parented inside a container once,
         before the game runs: Quest does this at the end of loading and then only
         on container events (see regen_var_objects). */
      update_container_visibility ();

      /* Read the game block's "collectables" declaration and seed the values,
	 before anything -- a startscript, a start-room entry script -- can read
	 or change one. */
      set_up_collectables ();

      /* Read the game block's "items"/"possitems" declaration, which below ASL
	 2.80 is the whole of the inventory -- so it has to be in place before
	 "startitems" can flag anything in it as held. */
      set_up_items ();

      /* Quest "startitems <a; b; ...>": the player's initial inventory. */
      for (const auto &i: game.data)
	if (ci_equal (first_token (i, c1, c2), "startitems"))
	  {
	    tok = next_token (i, c1, c2);
	    if (is_param (tok))
	      for (const string &it: split_param (param_contents (tok)))
		if (trim (it) != "")
		  run_script ("give <" + trim (it) + ">");
	    break;
	  }

      /* Quest runs the startscript first and prints the intro text afterwards
	 (e.g. Mansion's startscript pauses on "Press any key" before its
	 "*** THE MANSION ***" intro is shown). */

      /* Run every startscript, not just the first: a game block may carry more
	 than one once a library has appended its own via `!addto game` (e.g. the
	 bundled type library's TLPstartup, which has to run before the game's own
	 startscript or the types are not set up when it looks at them).  Stopping
	 at the first would silently drop the game's initialisation -- or the
	 library's.

	 From ASL 3.11 the library ones ("lib startscript", tagged in
	 mark_library_addtos) all run first and in *reverse* order, the innermost
	 library initialising first; below 3.11 the tag means nothing and they run
	 in place with the rest (V4Game.Part2.cs:7967-7997). */
      if (asl_version_ >= 311)
	for (auto i = game.data.rbegin(); i != game.data.rend(); ++ i)
	  if (ci_equal (first_token (*i, c1, c2), "lib") &&
	      ci_equal (next_token (*i, c1, c2), "startscript"))
	    run_script_as ("game", i->substr (c2 + 1));
      for (const auto &i: game.data)
	{
	  /* CI: Quest matches "startscript " with BeginsWith
	   * (V4Game.Part2.cs:7971). */
	  string tok = first_token (i, c1, c2);
	  if (ci_equal (tok, "lib") && asl_version_ < 311)
	    tok = next_token (i, c1, c2);
	  if (ci_equal (tok, "startscript"))
	    run_script_as ("game", i.substr (c2 + 1));
	}

      /* ... unless the startscript asked for no intro, in which case it has
	 (probably) displayed <intro> itself at the point it wanted it. */
      if (auto_intro_)
	run_script ("displaytext <intro>");

      regen_var_room ();
      objs_vars_dirty_ = true;
      regen_var_dirs ();
      regen_var_look ();
      /* Quest enters the start room like any other (PlayGame): describe it,
       * then run its entry script -- which may immediately goto the real first
       * room (a "please wait" start room).  That nested goto describes the
       * destination, so there is no second describe here. */
      /* The start room is entered through PlayGame as well, so it gets the same
       * "visited" stamp on the same version-dependent side of its script (see
       * goto_room). */
      if (asl_version_ >= 391 && asl_version_ < 410)
	set_obj_property (state.location, "visited");
      look();
      {
	string start_room = state.location, start_scr;
	if (get_room_action (start_room, "script", start_scr))
	  run_script_as (start_room, start_scr);
	if (asl_version_ >= 410 && !has_room_property (start_room, "visited"))
	  set_obj_property (start_room, "visited");
      }
      /* Start a fresh undo history (RESTART reuses this object) and seed it
       * with the opening state so the first command can be undone too. */
      undo_buffer = LimitStack<UndoState> (kUndoLevels);
      if (state.running)
	{ UndoState u = state.save_undo (); undo_buffer.push (u); }
    }
  catch (string s)
    {
      GEAS_DBG << s << endl;
      gi->debug_print (s);
    }
  GEAS_DBG << "s_g: done with set_game (...)\n\n";
}

void geas_implementation::regen_var_objects (bool room_display)
{
  /* Cleared before the rebuild, not after, so an onchange script hung on one
   * of the two variables cannot re-enter the flush. */
  objs_vars_dirty_ = false;
  /* No container-visibility sweep here.  Quest re-derives the contents' hidden
   * flags only when the container itself changes -- on open/close, on look at,
   * on put/take, and once when the game loads (UpdateVisibilityInContainers,
   * V4Game.Part2.cs:7650 and its four callers) -- so a "show <X>" aimed at
   * something parented inside a shut container sticks, and the object stays
   * referrable: Disambiguate scopes on the hidden flag and the object's room
   * alone, never on the container chain (DisambObjHere, V4Game.cs:4303).  Two
   * games in the corpus reveal a parented object that way and nothing else ever
   * opens the container: Christmas Day's Knife and Darkness's Hammer.  Sweeping
   * here (i.e. after every turn) hid them again on the next redraw.  See
   * set_obj_property and move for the sweep's real call sites. */
  string tmp;
  vector <string> objs, chars;
  for (const auto &i: state.objs)
    {
      /* List things in this room.  That is the object's own ContainerRoom, and
       * nothing else: what is inside an open container here has that same room
       * (put there by DoAddRemove, kept there by MoveThing), and what is inside
       * a shut one is filtered out by the hidden flag below.  This record's
       * own field, never a lookup by name -- two objects may share a name in
       * different rooms (The Hobbit defines a "ring" in three of them). */
      if (ci_equal (i.parent, state.location) &&
	  !get_obj_property (i.name, "hidden", tmp) &&
	  !get_obj_property (i.name, "invisible", tmp))
	{
	  /* Below 2.80 Quest keeps characters in a table of their own and
	   * gives them a line of their own, so one never turns up among the
	   * objects (ShowRoomInfoV2 walks _chars and then _objs,
	   * V4Game.Part2.cs:1824-1886; the `define character' branch of the
	   * loader that fills _chars is itself gated on ASLVersion <= 280,
	   * ibid. 3571).  From 2.80 on there is no such table and a character
	   * is an object like any other -- which is how geas stores them at
	   * every version, so the split has to happen here.  The two gates really
	   * do differ by one: at exactly 2.80 the loader still fills _chars but
	   * the room description is ShowRoomInfoV2's successor, which never reads
	   * it, so a character there is listed nowhere at all. */
	  if (asl_version_ <= 280 && gf.find_by_name ("character", i.name) != NULL)
	    chars.push_back (i.name);
	  else
	    objs.push_back (i.name);
	}
    }
  /* Two lists out of one walk.  quest.formatobjects is the sentence's own text
   * -- bold names, the suffix, and " and " before the last item -- and
   * quest.objects is the plain one the game is meant to reuse: prefix and name,
   * joined with ", " and nothing else.  It has no suffix and never has an
   * " and ", in any of the three routines that write it (ShowRoomInfo,
   * V4Game.Part2.cs:3799-3822; ShowRoomInfoV2, ibid. 1884-1896; UpdateObjectList,
   * ibid. 7521-7550).  Nearco prints "#quest.objects#" itself and so reads the
   * comma list where geas used to hand it "un hueso, una piedra and una roca".
   *
   * The " and " in quest.formatobjects is the room display's alone.  The pane
   * refresh rebuilds the pair after every show, hide, create, move and take
   * (SetAvailability, ibid. 3087-3088, and eleven other callers) and joins with
   * ", " throughout, so a script that changes what is visible and then prints
   * "#quest.formatobjects#" reads an unpunctuated list.  Sleepover's <bed2>
   * description does exactly that: two `show' lines, then the list. */
  string qobjs = "", qfobjs = "";
  string objname, prefix, main, suffix, propval, plain, print2;
  for (uint i = 0; i < objs.size(); i ++)
    {
      objname = objs[i];
      if (!get_obj_property (objname, "alias", main))
	main = objname;
      plain = main;
      print2 = "|b" + main + "|xb";
      if (get_obj_property (objname, "prefix", prefix))
	{
	  plain = prefix + " " + plain;
	  print2 = prefix + " " + print2;
	}
      if (get_obj_property (objname, "suffix", suffix))
	print2 = print2 + " " + suffix;
      qobjs = qobjs + plain;
      qfobjs = qfobjs + print2;
      if (i + 1 < objs.size())
	{
	  qobjs = qobjs + ", ";
	  qfobjs = qfobjs + (room_display && i + 2 == objs.size() ? " and " : ", ");
	}
    }
  set_svar ("quest.objects", qobjs);
  set_svar ("quest.formatobjects", qfobjs);

  /* The pre-2.80 characters line.  quest.characters gets the comma-joined
   * list, without the " and " the printed sentence gets, because Quest sets
   * the variable before it rewrites the last comma (V4Game.Part2.cs:1836-1856)
   * -- and gets "" when there is nobody here, where the line itself still
   * prints. */
  characters_display_ = "";
  if (asl_version_ < 280)
    {
      string qchars = "";
      for (size_t i = 0; i < chars.size (); i ++)
	{
	  if (!get_obj_property (chars[i], "alias", main))
	    main = chars[i];
	  print2 = "|b" + main + "|xb";
	  if (get_obj_property (chars[i], "prefix", prefix))
	    print2 = prefix + " " + print2;
	  if (get_obj_property (chars[i], "suffix", suffix))
	    print2 = print2 + " " + suffix;
	  qchars = qchars + (i > 0 ? ", " : "") + print2;
	}
      set_svar ("quest.characters", qchars);
      if (qchars == "")
	characters_display_ = "There is nobody here.";
      else
	{
	  std::string::size_type lastcomma = qchars.rfind (',');
	  if (lastcomma != string::npos)
	    qchars = trim (qchars.substr (0, lastcomma)) + " and "
		     + trim (qchars.substr (lastcomma + 1));
	  characters_display_ = "You can see " + qchars + " here.";
	}
    }
}

void geas_implementation::regen_var_room ()
{
  set_svar ("quest.currentroom", state.location);

  string tmp, formatroom;
  if (!get_room_property (state.location, "alias", formatroom))
    {
      /* Quest displays _rooms[id].RoomName -- the spelling used by `define room`
       * -- not whatever casing the goto happened to use (ShowRoomInfo,
       * V4Game.Part2.cs:3709-3712).  The Birthday Assignment's `goto <Edock>`
       * into `define room <EDock>` is the case in point.  A room made at
       * runtime by `create room` has no block, so fall back to the name given. */
      const GeasBlock *gb = gf.find_by_name ("room", state.location);
      formatroom = (gb != NULL && gb->name != "") ? gb->name : state.location;
    }
  formatroom = "|cr" + formatroom + "|cb";
  if (get_room_property (state.location, "prefix", tmp))
    formatroom = tmp + " " + formatroom;
  /* No suffix: Quest has a Suffix field on objects and on characters, and none
   * on a room -- ShowRoomInfo builds the displayed name out of the alias and
   * the prefix and nothing else (V4Game.Part2.cs:3724-3742).  A `suffix' tag on
   * a room is read into the property table like any other and never displayed.
   * See finding 73. */
  set_svar ("quest.formatroom", formatroom);

}

void geas_implementation::regen_var_look ()
{
  /* Below 2.80 the look text is read straight off the room block's source and
   * printed from there: ShowRoomInfoV2 walks the block for the first line
   * beginning "look" outside any nested define and takes GetParameter of it
   * (V4Game.Part2.cs:2157-2183).  Two things follow.  The line's *first*
   * parameter is the text whether or not the line is a script, so a `look msg
   * <...>' reads here as the message it prints -- everywhere else that line is
   * an action and has no text at all.  And there is no quest.lookdesc: the
   * variable is written by the 2.80 display alone (ibid. 3897), so a 2.x game
   * that prints "#quest.lookdesc#" gets an empty string, and geas keeps its own
   * copy for the display instead. */
  if (asl_version_ < 280)
    {
      look_desc_ = "";
      const GeasBlock *gb = gf.find_by_name ("room", state.location);
      if (gb != NULL)
	for (const string &line: gb->data)
	  {
	    std::string::size_type d1, d2;
	    string tok = first_token (line, d1, d2);
	    /* A `look msg <...>'-style line survives readfile's rewriting whole,
	     * while a plain `look <...>' has already become the room's "look"
	     * property -- so both forms have to be recognised, and in the order
	     * they were written, to take the first of them as Quest does. */
	    if (tok == "look")
	      {
		while ((tok = next_token (line, d1, d2)) != "" && !is_param (tok))
		  ;
		if (is_param (tok))
		  look_desc_ = eval_param (tok);
		break;
	      }
	    if (ci_equal (tok, "properties") || ci_equal (tok, "tagproperty"))
	      {
		tok = next_token (line, d1, d2);
		if (!is_param (tok))
		  continue;
		string pc = param_contents (tok);
		std::string::size_type eq = pc.find ('=');
		if (eq != string::npos && ci_equal (pc.substr (0, eq), "look"))
		  {
		    if (!get_room_property (state.location, "look", look_desc_))
		      look_desc_ = "";
		    break;
		  }
	      }
	  }
      return;
    }

  string look_tag;
  if (!get_room_property (state.location, "look", look_tag))
    look_tag = "";
  set_svar ("quest.lookdesc", look_tag);
}

void geas_implementation::regen_var_dirs()
{
  /* From 4.10 the three older doorway variables are not written at all.
   * UpdateDoorways hands the whole job to GetAvailableDirectionsDescription and
   * skips the block that sets ".dirs" and the "out" pair
   * (V4Game.Part2.cs:7207-7211), and ShowRoomInfo's places half is inside its
   * own `if ASLVersion < 410` (ibid. 3838-3874).  So a 4.10 game that prints
   * one of them gets an empty string: Nearco's "Puedes ir al: |b#quest.
   * doorways.dirs#|xb" is a bare label in Quest, and geas filled it with all
   * eight compass points.  quest.doorways is the mirror image -- set only from
   * 4.10 (see below) -- so exactly one of the two forms is live at a time. */
  const bool old_vars = (asl_version_ < 410);
  vector <string> dirs;
  if (asl_version_ >= 280)
    {
      /* 2.80 to 4.09: the fixed compass order, straight out of UpdateDoorways'
       * ten `if` statements (V4Game.Part2.cs:7229-7286).  The -1 skips "out",
       * which gets its own line below. */
      for (size_t i = 0; i < ARRAYSIZE (dir_names) - 1; i ++)
	if (exit_dest (state.location, dir_names[i]) != "")
	  dirs.push_back (dir_names[i]);
    }
  else
    {
      /* Below 2.80 there is no room object to read the exits off: ShowRoomInfoV2
       * walks the room block's own lines and appends each direction as it meets
       * it (V4Game.Part2.cs:1935-1980), so the line follows the order the author
       * wrote the tags in.  QDK writes its diagonals NW, NE, SW, SE, which is
       * why the fixed order showed up as a swapped pair -- Fade To White (210)
       * listed "northeast, northwest, southeast or southwest" where Quest says
       * "northwest, northeast, southwest or southeast", and Koww (200) had its
       * two exits the wrong way round.
       *
       * That loop also has no branch for "up" or "down": below 2.80 the two of
       * them are walkable but never named in the line. */
      for (const string &dir: exit_dir_order (state.location))
	if (dir != "up" && dir != "down" && dir != "out"
	    && exit_dest (state.location, dir) != "")
	  dirs.push_back (dir);
    }
  string exits = "";
  if (dirs.size() == 1)
    exits = "|b" + dirs[0] + "|xb";
  else if (dirs.size() > 1)
    {
      for (size_t i = 0; i < dirs.size(); i ++)
	{
	  exits = exits + "|b" + dirs[i] + "|xb";
	  if (i < dirs.size() - 2)
	    exits = exits + ", ";
	  else if (i == dirs.size() - 2)
	    exits = exits + " or ";
	}
    }
  if (old_vars)
    set_svar ("quest.doorways.dirs", exits);

  /* Below 2.80 the "out" line is not built from an exit at all.  ShowRoomInfoV2
   * walks the room block's own source lines, takes GetParameter of the last one
   * beginning "out" -- whatever that tag was written for -- looks the result up
   * as a room name to find an alias, and prints it plain
   * (V4Game.Part2.cs:1939-1942, 2004-2025).  So a *scripted* out advertises
   * whatever its first parameter happens to be: Fade to White's `out msg <You
   * need to get up first.>' really does print "You can go out to You need to
   * get up first..", where 2.80's UpdateDoorways would have printed no line at
   * all.  There is no prefix split and no bolding here either; both arrived
   * with 2.80. */
  if (asl_version_ < 280)
    {
      string doorways = "";
      const GeasBlock *gb = gf.find_by_name ("room", state.location);
      if (gb != NULL)
	for (const string &line: gb->data)
	  {
	    std::string::size_type d1, d2;
	    if (first_token (line, d1, d2) != "out")
	      continue;
	    /* GetParameter reads the line's first <...>, so the token to take is
	     * the first parameter after the keyword and not the first token. */
	    string tok;
	    while ((tok = next_token (line, d1, d2)) != "" && !is_param (tok))
	      ;
	    doorways = is_param (tok) ? eval_param (tok) : "";
	  }
      string out_display = "", out_alias;
      if (doorways != "")
	out_display = (get_room_property (doorways, "alias", out_alias)
		       && out_alias != "") ? out_alias : doorways;
      set_svar ("quest.doorways.out", out_display);
      set_svar ("quest.doorways.out.display", out_display);
    }

  string out_dest = (asl_version_ < 280)
    ? "" : exit_dest (state.location, "out");
  /* Whatever the "out" tag was written for, the line UpdateDoorways prints is
   * built from Out.Text -- its first parameter -- and nothing else
   * (V4Game.Part2.cs:7218-7233).  A scripted out is no exception, because before
   * 4.10 the loader fills Out.Text from every out line, script or not (see
   * declared_exit_dest): an "out { ... }" block, which readfile de-inlines into
   * "out do <!intprocNNN>" exactly as Quest's own ConvertMultiLineSections does,
   * really does advertise "You can go out to !intprocNNN.".  (From ASL 410 on
   * Quest folds "out" into the single directions line instead, via
   * RoomExits.GetAvailableDirectionsDescription; geas still prints the pre-410
   * two-line form -- and old_vars keeps this block off those games anyway.) */
  if (!old_vars || asl_version_ < 280)
    ;   /* the "out" pair belongs to the pre-4.10 block too, and below 2.80 it
	 * has been written already */
  else if (out_dest == "")
    {
      set_svar ("quest.doorways.out", "");
      set_svar ("quest.doorways.out.display", "");
    }
  else
    {
      GEAS_DBG << "Updating quest.doorways.out; out_dest == {" << out_dest << "}";
      /* "out <prefix; destination>" before ASL 4.10, "out <destination;
       * lockmessage>" from 4.10 on -- see split_exit_dest. */
      string prefix, raw_out = out_dest;
      split_exit_dest (raw_out, out_dest, prefix);
      GEAS_DBG << "; prefix == {" << prefix << "}, out_dest == {" << out_dest << "}";
      GEAS_DBG << "  quest.doorways.out == {" << out_dest << "}";
      set_svar ("quest.doorways.out", out_dest);
      GEAS_DBG << endl;

      string tmp = displayed_name (out_dest);

      GEAS_DBG << ", tmp == {" << tmp << "}";

      /* The prefix is the article/preposition the author put in front of the
       * destination ("out <the; town>" -> "the |btown|xb"), so it goes outside
       * the bold, exactly as get_places () renders a place's prefix.  It used to
       * be dropped whenever the destination had a display name -- i.e. always,
       * for a room that exists -- which left it visible only in the one case it
       * was not written for.
       *
       * An aliased destination below ASL 3.60 loses it, though: UpdateDoorways
       * keeps "prefix name" together only while there is no alias to replace
       * the name with, and reattaches the prefix to an alias from 3.60 on
       * (V4Game.Part2.cs:7294-7305).  King's Quest V (350) goes `out <the;
       * small country inn -outdoors>' to an aliased room, and Quest's line is
       * "You can go out to small country inn -outdoors." */
      string out_alias;
      if (tmp == "")
	tmp = out_dest;
      else if (asl_version_ < 360
	       && get_obj_property (out_dest, "alias", out_alias)
	       && out_alias != "")
	prefix = "";
      tmp = (prefix != "" ? prefix + " " : "") + "|b" + tmp + "|xb";

      GEAS_DBG << ",    final value {" << tmp << "}" << endl;

      set_svar ("quest.doorways.out.display", tmp);
    }

  current_places = get_places (state.location);
  string printed_places = "";
  for (size_t i = 0; i < current_places.size(); i ++)
    {
      /* Quest builds the list with ", " after every entry, strips the trailing
       * one and then turns the last remaining comma into " or" -- so from 2.80
       * the separator before the final place is " or ", with no comma of its
       * own however many places there are (V4Game.Part2.cs:3845-3868).  The
       * pre-2.80 display keeps the comma: it splits at Left(places, lastComma)
       * rather than lastComma - 1 (ibid. 2073-2085), which is where geas's
       * Oxford comma came from -- it just applied it at every version. */
      if (i == 0)
	printed_places = current_places[i][0];
      else if (i + 1 < current_places.size())
	printed_places = printed_places + ", " + current_places[i][0];
      else if (asl_version_ >= 280)
	printed_places = printed_places + " or " + current_places[i][0];
      else
	printed_places = printed_places + ", or " + current_places[i][0];
    }
  if (old_vars)
    set_svar ("quest.doorways.places", printed_places);

  /* "quest.doorways" -- the ASL 4.10 single folded list of everywhere you can go
   * from here, which is what a 4.10 game writes its own exits line from.  It is
   * set by RoomExits.GetAvailableDirectionsDescription (RoomExits.cs:303-353),
   * called from UpdateDoorways only when ASLVersion >= 410 (V4Game.Part2.cs:
   * 7191-7194) -- below 4.10 the variable is never assigned at all, so leave it
   * alone there.  (The three ".dirs"/".out"/".places" variables are the other way
   * round: 4.10 stops setting those -- see old_vars above.)
   *
   * Every exit that still exists goes in one list, the compass ones first --
   * AllExits() walks the directions dictionary before the places dictionary
   * (RoomExits.cs:545-573) -- in the order the room's tags created them, with
   * "out" among them rather than on a line of its own.  Two games in the corpus
   * would print a bare "You can go ." without this: Tim Hamilton's "Barbarian"
   * and his "The Things That Go Bump In The Night" both replace the room display
   * with a game-block description ending in `msg <You can go #quest.doorways#.>`,
   * gated on a "for each exit in" loop having found something. */
  if (asl_version_ >= 410)
    {
      vector<string> parts;
      for (const string &dir: exit_dir_order (state.location))
	if (exit_dest (state.location, dir) != "")
	  parts.push_back ("|b" + dir + "|xb");
      /* A place is the one kind of exit that keeps a destination in the line:
       * GetDirectionNameDisplay returns "to [prefix ]|bname|xb" for it, and
       * the prefix is the *destination room's* (RoomExits.cs:512-528, set from
       * _rooms[GetRoomId()].Prefix by RoomExit.cs:231) -- not the one written
       * on the "place <the; X>" line, which is the pre-4.10 rule. */
      for (const auto &p: current_places)
	{
	  string entry = "|b" + p[2] + "|xb", roomprefix;
	  if (get_room_property (p[3], "prefix", roomprefix) && roomprefix != "")
	    entry = roomprefix + " " + entry;
	  parts.push_back ("to " + entry);
	}
      /* Quest's own joining: ", " between all but the last two, " or " before
       * the last.  (Its counter is one ahead of the index, so a single exit gets
       * no separator at all and two are joined with " or ".) */
      string doorways = "";
      for (size_t i = 0; i < parts.size (); i ++)
	{
	  doorways += parts[i];
	  if (i + 2 < parts.size ())
	    doorways += ", ";
	  else if (i + 2 == parts.size ())
	    doorways += " or ";
	}
      set_svar ("quest.doorways", doorways);
    }
}

/* "take off", "pick up", "look at", ... -- phrasal verbs whose leading word is a
 * common synonym target.  A single-word synonym (e.g. KQ5's "take = get") must
 * not rewrite the lead word of one of these, or "take off cloak" turns into
 * "get off cloak" and never matches the take-off command/verb. */
static bool is_phrasal_verb_lead (const string &phrase)
{
  static const char *phrasals[] =
    { "take off", "put on", "put in", "put down", "pick up", "get off",
      "look at", "look in", "look inside", "look under", "look behind",
      "listen to", "sit on", "sit in", "turn on", "turn off",
      "switch on", "switch off", "give to", "take from" };
  for (const char *p : phrasals)
    if (phrase == p)
      return true;
  return false;
}
