// geas-script.inc -- Script execution: apply_type, run_script and the
// statement handlers for every ASL script keyword, plus condition evaluation
// (eval_cond/eval_conds).
//
// Split out of geas-runner.cc, which #includes it (same single-TU layout
// as the quest5/aslx-*.inc units); the code is unchanged from the split.

void geas_implementation::apply_type (const string &obj, const string &typenm)
{
  std::vector<std::pair<string, string>> tprops, tactions;
  std::vector<string> included;
  gf.flatten_type (typenm, tprops, tactions, included);
  for (const auto &p: tprops)
    {
      if (p.second == "!")
	set_obj_property (obj, "not " + p.first);
      else if (p.second == "")
	set_obj_property (obj, p.first);
      else
	set_obj_property (obj, p.first + "=" + p.second);
    }
  for (const auto &a: tactions)
    set_obj_action (obj, "<" + a.first + ">" + a.second);
  /* ExecType records the named type even when no block of that name exists
   * (TypesIncluded is appended before the flatten), plus every type the
   * flatten visited. */
  set_obj_property (obj, "!type " + lcase (typenm));
  for (const auto &t: included)
    set_obj_property (obj, "!type " + lcase (t));
}

void geas_implementation::run_script_as (const string &obj, const string &scr)
{
  string backup_object, garbage;
  backup_object = this_object;
  this_object = obj;
  run_script (scr, garbage);
  this_object = backup_object;
}

void geas_implementation::run_script (const string &s)
{
  string garbage;
  run_script (s, garbage);
}

void geas_implementation::run_script (const string &s, string &rv)
{
  GEAS_DBG << "Script line '" << s << "'\n";
  string tok;
  std::string::size_type c1, c2;

  /* See kMaxScriptDepth: a self-re-entering script would otherwise overflow the
   * stack and kill the process. */
  if (script_depth >= kMaxScriptDepth)
    {
      gi->debug_print ("Out of stack space running '" + s + "' - infinite loop?");
      return;
    }
  ScriptDepth depth_guard (script_depth);

  /* Quest's ExecuteScript returns immediately for every line once the game has
   * finished (V4Game.Part2.cs:5695-5698, the `if (_gameFinished) return;` right
   * after the empty-line check).  So playerwin / playerlose / stop don't just
   * end the game, they abandon the rest of the enclosing script and everything
   * that would have run after it.  Games do lean on this: Ponyville.asl:1752
   * puts a bare `stop` in the middle of the chapter-2 ending and follows it
   * with more msg text and a playerwin that real Quest never reaches. */
  if (!is_running_)
    return;

  tok = first_token (s, c1, c2);

  if (tok == "") return;

  if (tok[0] == '{')
    {
      std::string::size_type brace1 = c1 + 1, brace2;
      for (brace2 = s.length() - 1; brace2 >= brace1 && s[brace2] != '}'; brace2 --)
	;
      if (brace2 >= brace1)
	run_script (s.substr (brace1, brace2 - brace1));
      else
	gi->debug_print ("Unterminated brace block in " + s);
      return;
    }

  /* ASL statement keywords are case-insensitive.  Fold the dispatch token once
   * so every top-level `tok == "..."` below matches regardless of case; branch
   * arguments are re-read with next_token, and the unrecognised-statement path
   * logs the original line, so only the keyword comparison is affected. */
  tok = lcase (tok);

  /* One hash probe instead of up to ~60 string compares per executed
   * statement.  The table maps every accepted spelling to its case; the case
   * bodies are the old else-if branches unchanged, and `break` falls through
   * to the "Unrecognized script" report exactly as falling off a branch did. */
  enum Stmt {
    st_unknown, st_action, st_animate, st_font, st_helpclear, st_helpclose,
    st_mailto, st_modvolume, st_msgto, st_panes, st_shell, st_shellexe,
    st_with, st_type, st_add, st_remove, st_open, st_close, st_background,
    st_lock, st_unlock, st_select, st_choose, st_clear, st_clone, st_create,
    st_debug, st_destroy, st_disconnect, st_displaytext, st_helpdisplaytext,
    st_do, st_doaction, st_dontprocess, st_enter, st_exec, st_flag, st_for,
    st_foreground, st_give, st_goto, st_hide, st_hideobject, st_hidechar,
    st_show, st_showobject, st_showchar, st_if, st_inc, st_dec, st_lose,
    st_move, st_movechar, st_moveobject, st_msg, st_helpmsg, st_outputoff,
    st_outputon, st_nointro, st_pause, st_picture, st_playerlose,
    st_playerwin, st_playwav, st_playmidi, st_playmod, st_property,
    st_repeat, st_return, st_reveal, st_revealobject, st_revealchar,
    st_conceal, st_concealobject, st_concealchar, st_say, st_set,
    st_setstring, st_setvar, st_speak, st_stop, st_timeron, st_timeroff,
    st_wait
  };
  static const std::unordered_map<string, Stmt> stmt_table = {
    {"action", st_action}, {"animate", st_animate}, {"font", st_font},
    {"helpclear", st_helpclear}, {"helpclose", st_helpclose},
    {"mailto", st_mailto}, {"modvolume", st_modvolume}, {"msgto", st_msgto},
    {"panes", st_panes}, {"shell", st_shell}, {"shellexe", st_shellexe},
    {"with", st_with}, {"type", st_type}, {"add", st_add},
    {"remove", st_remove}, {"open", st_open}, {"close", st_close},
    {"background", st_background}, {"lock", st_lock}, {"unlock", st_unlock},
    {"select", st_select}, {"choose", st_choose}, {"clear", st_clear},
    {"clone", st_clone}, {"create", st_create}, {"debug", st_debug},
    {"destroy", st_destroy}, {"disconnect", st_disconnect},
    {"displaytext", st_displaytext}, {"helpdisplaytext", st_helpdisplaytext},
    {"do", st_do}, {"doaction", st_doaction}, {"dontprocess", st_dontprocess},
    {"enter", st_enter}, {"exec", st_exec}, {"flag", st_flag}, {"for", st_for},
    {"foreground", st_foreground}, {"give", st_give}, {"goto", st_goto},
    {"hide", st_hide}, {"hideobject", st_hideobject}, {"hidechar", st_hidechar},
    {"show", st_show}, {"showobject", st_showobject}, {"showchar", st_showchar},
    {"if", st_if}, {"inc", st_inc}, {"dec", st_dec}, {"lose", st_lose},
    {"move", st_move}, {"movechar", st_movechar}, {"moveobject", st_moveobject},
    {"msg", st_msg}, {"helpmsg", st_helpmsg}, {"outputoff", st_outputoff},
    {"outputon", st_outputon}, {"nointro", st_nointro}, {"pause", st_pause},
    {"picture", st_picture}, {"playerlose", st_playerlose},
    {"playerwin", st_playerwin}, {"playwav", st_playwav},
    {"playmidi", st_playmidi}, {"playmod", st_playmod},
    {"property", st_property}, {"repeat", st_repeat}, {"return", st_return},
    {"reveal", st_reveal}, {"revealobject", st_revealobject},
    {"revealchar", st_revealchar}, {"conceal", st_conceal},
    {"concealobject", st_concealobject}, {"concealchar", st_concealchar},
    {"say", st_say}, {"set", st_set}, {"setstring", st_setstring},
    {"setvar", st_setvar}, {"speak", st_speak}, {"stop", st_stop},
    {"timeron", st_timeron}, {"timeroff", st_timeroff}, {"wait", st_wait}
  };
  auto stmt_it = stmt_table.find (tok);
  switch (stmt_it == stmt_table.end () ? st_unknown : stmt_it->second)
    {
  case st_action:
    {
      tok = next_token (s, c1, c2);
      if (!is_param(tok))
	{
	  gi->debug_print ("Expected parameter after action in " + s);
	  return;
	}
      tok = eval_param (tok);
      std::string::size_type index = tok.find (';');
      if (index == string::npos)
	{
	  gi->debug_print ("Error: no semicolon in " + s);
	  return;
	}
      set_obj_action (trim (tok.substr (0, index)),
		      "<" + trim (tok.substr (index+1)) + "> " + s.substr (c2 + 1));
      return;
    }
    break;
  case st_font: case st_helpclear: case st_helpclose:
  case st_mailto: case st_modvolume: case st_msgto: case st_panes:
  case st_shell: case st_shellexe: case st_with:
    {
      /* Recognised statements with nothing to do here -- not "unrecognised
       * script".  helpclear/helpclose really do nothing in Quest 5 either
       * (V4Game.Part2.cs:5782, 5988); mailto/modvolume/shell/shellexe drive
       * host facilities geas does not provide; msgto and with are multiplayer
       * (QNSO); font and panes are TODO. */
      return;
    }
    break;
  case st_animate:
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
      return;
    }
    break;
  case st_type:
    {
      /* Runtime `type <obj; typename>` (ExecType, V4Game.cs:4425-4476):
       * flatten the type's properties and actions onto the object now, and
       * remember the membership for `if type`.  The marker is a
       * "!type <name>" property, following the "!exitlock"/"!clones"
       * convention, so it rides through save/undo with the rest of the
       * state. */
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected parameter after type in " + s);
	  return;
	}
      vector<string> args = split_param (eval_param (tok));
      if (args.size () != 2 || trim (args[0]) == "" || trim (args[1]) == "")
	{
	  gi->debug_print ("Expected <object; type> in " + s);
	  return;
	}
      string obj = trim (args[0]);
      if (state.obj_records (obj) == NULL)
	{
	  gi->debug_print ("No such object in " + s);
	  return;
	}
      apply_type (obj, trim (args[1]));
      return;
    }
  /* Quest's container statements, all ASL >= 3.91: "add <child; parent>" puts an
   * object into a container, "remove <child>" takes it back out, and
   * "open <container>" / "close <container>" set the open state silently -- no
   * message, no action or property lookup and no container check at all, unlike
   * the open/close *verbs* (ExecuteScript -> SetOpenClose -> DoOpenClose,
   * V4Game.cs:2240, and -> ExecAddRemoveScript -> DoAddRemove, V4Game.cs:2113).
   * Below 3.91 Quest has no such statements, so the line falls through to the
   * unrecognised-script path exactly as it would there. */
    break;
  case st_add: case st_remove:
    {
      if (asl_version_ < 391)
	break;   /* no such statement below 3.91: fall out as unrecognised */
      bool adding = (tok == "add");
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print (string ("Expected parameter after ") +
			   (adding ? "add" : "remove") + " in " + s);
	  return;
	}
      string arg = eval_param (tok);
      std::string::size_type index = arg.find (';');
      string child = trim (index == string::npos ? arg : arg.substr (0, index));
      if (state.obj_records (child) == NULL)
	{
	  gi->debug_print ("Invalid child object name specified in " + s);
	  return;
	}
      /* "add" must name a parent; "remove" may, and the two spellings do
       * different things.  Either way the object's ContainerRoom is left alone:
       * taking something out of a box leaves it loose in whatever room the box
       * was in. */
      if (index == string::npos)
	{
	  if (adding)
	    {
	      gi->debug_print ("No parent specified in " + s);
	      return;
	    }
	  /* A parentless "remove" is the one containment change that never
	   * reaches DoAddRemove.  It clears the property itself and then sweeps
	   * with the name of object number zero -- ExecAddRemoveScript never
	   * assigns parentId on this branch (V4Game.cs:2695-2699) -- so the name
	   * is empty and the sweep is the whole-game one.  See removenoparent:
	   * this is what puts Pyramid Of Terror's goth out of scope for good. */
	  do_add_remove (child, "", false, false);
	}
      else
	{
	  string parent = trim (arg.substr (index + 1));
	  if (state.obj_records (parent) == NULL)
	    {
	      gi->debug_print ("Invalid parent object name specified in " + s);
	      return;
	    }
	  do_add_remove (child, parent, adding);
	}
      /* No sweep here: do_add_remove has already re-derived the one container
       * Quest names (V4Game.cs:2133, 2698). */
      gi->update_sidebars ();
      return;
    }
    break;
  case st_open: case st_close:
    {
      if (asl_version_ < 391)
	break;   /* no such statement below 3.91: fall out as unrecognised */
      bool opening = (tok == "open");
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print (string ("Expected parameter after ") +
			   (opening ? "open" : "close") + " in " + s);
	  return;
	}
      string name = trim (eval_param (tok));
      if (state.obj_records (name) == NULL)
	gi->debug_print ("Invalid object name specified in " + s);
      else
	set_obj_property (name, opening ? "opened" : "not opened");
      return;
    }
    break;
  case st_background:
    {
      tok = next_token (s, c1, c2);
      if (is_param (tok))
	gi->set_background (eval_param (tok));
      else
	gi->debug_print ("Expected parameter after foreground in " + s);
      return;
    }
    break;
  case st_lock: case st_unlock:
    {
      /* Quest "lock <room; dir>" / "unlock <room; dir>": toggle an exit's
       * locked state.  Recorded as a property on the synthetic "!exitlock"
       * object so it persists with state/undo; exit_locked() consults it. */
      bool locking = (tok == "lock");
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected <room; direction> after " +
			   string (locking ? "lock" : "unlock") + " in " + s);
	  return;
	}
      vector<string> p = split_param (eval_param (tok));
      if (p.size () < 2 || trim (p[0]) == "" || trim (p[1]) == "")
	{
	  gi->debug_print ("Malformed exit in " + s);
	  return;
	}
      string key = lcase (trim (p[0])) + ";" + lcase (trim (p[1]));
      set_obj_property ("!exitlock", key + (locking ? "=locked" : "=open"));
      return;
    }
    break;
  case st_select:
    {
      /* Quest "select case <expr> do <!intproc>": the reader deinlines the
       * select-case block into a procedure whose lines are
       *   case <v1;v2;...> <script>     (script may itself be "do <!intproc>")
       *   case else <script>
       * Run the script of the first case whose (semicolon-separated) value
       * equals the selector; "case else" matches anything. */
      tok = next_token (s, c1, c2);
      /* CI: BeginsWith "select case " / "case " (V4Game.cs:2877). */
      if (!ci_equal (tok, "case"))
	{
	  gi->debug_print ("Expected 'case' after 'select' in " + s);
	  return;
	}
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected selector parameter in " + s);
	  return;
	}
      string selector = eval_param (tok);
      tok = next_token (s, c1, c2);          // "do"
      tok = next_token (s, c1, c2);          // <!intproc>
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected case block in " + s);
	  return;
	}
      string procname = param_contents (tok);
      /* O(1) name_index lookup instead of the case-insensitive linear scan the
         other procedure dispatch used to do (see run_procedure). */
      const GeasBlock *casep = gf.find_by_name ("procedure", procname);
      if (casep == NULL)
	{
	  gi->debug_print ("No case block " + procname + " found");
	  return;
	}
      for (uint j = 0; j < casep->data.size (); j ++)
	{
	  std::string::size_type d1, d2;
	  string line = casep->data[j];
	  if (!ci_equal (first_token (line, d1, d2), "case"))
	    continue;
	  string t2 = next_token (line, d1, d2);
	  if (ci_equal (t2, "else"))
	    {
	      run_script (trim (line.substr (d2)));
	      return;
	    }
	  if (!is_param (t2))
	    continue;
	  bool matched = false;
	  for (const string &val : split_param (param_contents (t2)))
	    if (ci_equal (trim (val), selector))
	      { matched = true; break; }
	  if (matched)
	    {
	      run_script (trim (line.substr (d2)));
	      return;
	    }
	}
      return;
    }
    break;
  case st_choose:
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected parameter after choose in " + s);
	  return;
	}
      tok = eval_param (tok);
      const GeasBlock *gb = gf.find_by_name ("selection", tok);
      if (gb == NULL)
	{
	  gi->debug_print ("No selection called " + tok + " found");
	  return;
	}
      string question, line;
      vector<string> choices, actions;
      for (size_t ln = 0; ln < gb->data.size(); ln ++)
	{
	  line = gb->data[ln];
	  tok = first_token (line, c1, c2);
	  /* Both keywords CI: FindStatement / BeginsWith (SetUpChoiceForm,
	   * V4Game.Part2.cs:627-634). */
	  if (ci_equal (tok, "info"))
	    {
	      tok = next_token (line, c1, c2);
	      if (is_param (tok))
		question = eval_param (tok);
	      else
		gi->debug_print ("Expected parameter after info in " + line);
	    }
	  else if (ci_equal (tok, "choice"))
	    {
	      tok = next_token (line, c1, c2);
	      if (is_param (tok))
		{
		  choices.push_back (eval_param (tok));
		  actions.push_back (line.substr (c2));
		}
	      else
		gi->debug_print ("Expected parameter after choice in " + line);
	    }
	  else
	    gi->debug_print ("Bad line " + line + " in selection");
	}
      if (choices.size() == 0)
	gi->debug_print ("No choices in selection " + gb->name);
      else
	run_script (actions[gi->make_choice (question, choices)]);
      return;
    }
    break;
  case st_clear:
    {
      gi->clear_screen();
      return;
    }
    break;
  case st_clone:
    {
      /* clone <src; newname[; room]> -- ExecClone, V4Game.cs:4352-4400.
       *
       * Quest appends a byte copy of the source's record to _objs, overwrites
       * its ObjectName with the new name and its ContainerRoom with the third
       * field (or, when there is none, with wherever the source itself is), and
       * calls UpdateObjectList.  Here the definition half of that copy is an
       * alias (GeasFile::register_clone) and the mutable half is a new
       * ObjectRecord plus copies of whatever properties and actions the source
       * has picked up at runtime, which together are what Quest's copy carries.
       *
       * The corpus case is Barbarian (Wonderjudge, ASL 410): every purchase in
       * Turner's market is `clone <Tomato; Tomato%c%; Market>` followed by a
       * `give`, so with this a no-op the player paid and got nothing -- and the
       * cabbage that tames the horse is the first link of task 1. */
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected param after clone in " + s);
	  return;
	}
      vector<string> args = split_param (eval_param (tok));
      if (args.size() < 2)
	{
	  gi->debug_print ("No new object name specified in " + s);
	  return;
	}
      string src = trim (args[0]), newname = trim (args[1]);
      const vector<size_t> *v = state.obj_records (src);
      if (v == NULL || v->empty())
	{
	  gi->debug_print ("Tried to clone nonexistent object '" + src + "'");
	  return;
	}
      if (newname == "")
	{
	  gi->debug_print ("No new object name specified in " + s);
	  return;
	}
      const ObjectRecord &srcobj = state.objs[(*v)[0]];
      /* The clone lands in the source's ContainerRoom -- the room, which is not
       * the same question as which container it is in.  The "parent" property
       * comes along with the rest of the properties copied below, so a clone of
       * something in a box is in that box too. */
      string dest = (args.size() > 2) ? trim (args[2]) : container_room (src);

      ObjectRecord data;
      data.name = newname;
      data.parent = dest;
      data.hidden = srcobj.hidden;
      data.invisible = srcobj.invisible;
      data.is_room = srcobj.is_room;
      state.add_object (data);
      gf.register_clone (src, newname);
      /* Quest's record copy takes the source's runtime Properties and Actions
       * too, so a source that has been changed since load clones as it is now,
       * not as it was written. */
      if (const vector<size_t> *pv = state.prop_records (src))
	for (size_t idx: *pv)
	  state.add_prop (newname, state.props[idx].data);
      /* So a clone survives a save/restore: load_state replays these to
       * re-register the definition aliases, which live in the GeasFile and are
       * therefore not part of the serialized state. */
      state.add_prop ("!clones", "properties " + lcase (newname) + "=" + src);

      update_container_visibility (dest);
      gi->update_sidebars();
      objs_vars_dirty_ = true;
      return;
    }
    break;
  case st_create:
    {
      tok = next_token (s, c1, c2);
      /* The created kind is CI: ExecuteCreate tests "room "/"exit " with
       * BeginsWith (V4Game.cs:5240-5244), as ExecuteCreateExit does the
       * direction names (V4Game.cs:5432 ff.). */
      if (ci_equal (tok, "exit")) // create exit
	{
	  string dir = "";

	  tok = next_token (s, c1, c2);
	  if (!is_param (tok))
	    {
	      /* Folded, so the stored record matches the engine's own
	       * lowercase direction names wherever it is read back. */
	      dir = lcase (tok);
	      tok = next_token (s, c1, c2);
	    }

	  if (!is_param (tok))
	    {
	      gi->debug_print ("Expected param after create exit in " + s);
	      return;
	    }
	  tok = eval_param (tok);
	  vector<string> args = split_param (tok);
	  if (args.size() != 2)
	    {
	      gi->debug_print ("Expected 2 elements in param in " + s);
	      return;
	    }
	  /* Quest never lists the same "go to" exit twice.  Below ASL 4.10
	   * ExecuteCreateExit looks the destination up in the room's Places list
	   * first and, if it is already there, logs "Exit from 'X' to 'Y'
	   * already exists" and does nothing at all (V4Game.cs:5385-5409).  From
	   * 4.10 on the places live in a dictionary keyed by destination room, so
	   * AddPlaceExit deletes the old entry before inserting the new one
	   * (RoomExits.cs:46-56): the exit is replaced rather than duplicated,
	   * and moves to the end of the listing.  Either way one destination
	   * means one entry.  geas simply appended, so DJay32's "Metroid" Lite
	   * (ASL 400), whose Boardroom East does "create exit <Hallway; Hidden
	   * Secrets>" every time you walk into the hallway, wound up saying "You
	   * can go to Piperoom, Boardroom, Storage Room, or Storage Room.".
	   * Below 4.10 the comparison is LCase'd; the 4.10 dictionary key is
	   * case-sensitive.  Replaying the destroy-exit record is how the 4.10
	   * side removes a place it does not own, and it works for statically
	   * declared places too because get_places () replays these records in
	   * order. */
	  if (dir == "")
	    for (const auto &place: get_places (args[0]))
	      if (asl_version_ >= 410 ? place[3] == args[1]
		  : lcase (place[3]) == lcase (args[1]))
		{
		  if (asl_version_ < 410)
		    {
		      gi->debug_print ("Exit from '" + args[0] + "' to '" +
				       args[1] + "' already exists");
		      return;
		    }
		  state.exits.push_back (ExitRecord (args[0],
						     "destroy exit " + args[1]));
		  break;
		}

	  if (dir != "")
	    state.exits.push_back (ExitRecord (args[0],
					       "exit "+dir+" <"+tok+">"));
	  else
	    state.exits.push_back (ExitRecord (args[0], "exit <" + tok + ">"));
	  regen_var_dirs();
	  return;
	}
      else if (ci_equal (tok, "object")) // create object
	{
	  /* ExecuteCreate's object half (V4Game.cs:5278-5325): a fresh object
	   * named by the first field, placed in the room named by the second
	   * -- or nowhere at all when there is none.  Alias, gender and
	   * article take the same defaults geas already falls back on, and
	   * from 4.10 the `default` type's properties and actions are
	   * flattened onto it exactly as a declared object gets them. */
	  tok = next_token (s, c1, c2);
	  if (!is_param (tok))
	    {
	      gi->debug_print ("Expected param after create object in " + s);
	      return;
	    }
	  vector<string> args = split_param (eval_param (tok));
	  string name = trim (args[0]);
	  if (name == "")
	    {
	      gi->debug_print ("No object name given in " + s);
	      return;
	    }
	  ObjectRecord data;
	  data.name = name;
	  data.parent = (args.size () > 1) ? trim (args[1]) : "";
	  data.hidden = false;
	  data.invisible = false;
	  data.is_room = false;
	  state.add_object (data);
	  if (asl_version_ >= 410 && gf.find_by_name ("type", "default"))
	    apply_type (name, "default");
	  gi->update_sidebars ();
	  objs_vars_dirty_ = true;
	}
      else if (ci_equal (tok, "room")) // create room
	{
	  /* Quest appends an empty room to _rooms -- no description, no exits
	   * (ExecuteCreate, V4Game.cs:5244-5276) -- so the only thing that
	   * changes for the player is that GetRoomID now finds the name and
	   * "goto" and "create exit" can reach it.  Recording the name is
	   * therefore the whole of it here; the room's contents, if the game
	   * gives it any, arrive as ordinary property and exit records.  (A room
	   * of that name declared in the file wins either way: GetRoomID returns
	   * the first match, and find_by_name is checked first.) */
	  tok = next_token (s, c1, c2);
	  if (!is_param (tok))
	    {
	      gi->debug_print ("Expected param after create room in " + s);
	      return;
	    }
	  set_obj_property ("!createdroom", lcase (trim (eval_param (tok))));
	}
      else
	gi->debug_print ("Bad create line " + s);
      return;
    }
    break;
  case st_debug:
    {
      tok = next_token (s, c1, c2);
      if (is_param (tok))
	gi->debug_print (eval_param(tok));
      else
	gi->debug_print ("Expected param after debug in " + s);
      return;
    }
    break;
  case st_destroy:
    {
      tok = next_token (s, c1, c2);
      /* CI: BeginsWith "destroy exit " (V4Game.Part2.cs). */
      if (!ci_equal (tok, "exit"))
	{
	  gi->debug_print ("expected 'exit' after 'destroy' in " + s);
	  return;
	}
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected param after 'destroy exit' in " + s);
	  return;
	}
      string tok2 = eval_param (tok);
      vector<string> args = split_param (tok2);
      if (args.size() != 2)
	{
	  gi->debug_print ("Expected two arguments in " + s);
	  return;
	}
      state.exits.push_back (ExitRecord (args[0], "destroy exit " + args[1]));
      regen_var_dirs();
      return;
    }
    break;
  case st_disconnect:
    {
      /* disconnect <room; direction> -- remove that exit (the inverse of
       * create exit <direction> <room; dest>).  Recorded as a "noexit" so it
       * overrides the static room exit and any earlier dynamic create-exit. */
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
        {
          gi->debug_print ("Expected param after disconnect in " + s);
          return;
        }
      vector<string> args = split_param (eval_param (tok));
      if (args.size () != 2)
        {
          gi->debug_print ("Expected <room; direction> after disconnect in " + s);
          return;
        }
      /* Folded like create exit's direction, so the record matches the
       * engine's lowercase direction names when read back. */
      state.exits.push_back (ExitRecord (args[0], "noexit " + lcase (trim (args[1]))));
      regen_var_dirs ();
      return;
    }
  /* "helpdisplaytext" is displaytext for the same reason helpmsg is msg: with no
   * separate help window it goes through DisplayTextSection unchanged
   * (V4Game.Part2.cs:5972-5975). */
    break;
  case st_displaytext: case st_helpdisplaytext:
    {
      string stmt = tok;
      tok = next_token (s, c1, c2);
      if (!is_param(tok))
	{
	  gi->debug_print ("Expected parameter after " + stmt + " in " + s);
	  return;
	}
      const GeasBlock *gb = gf.find_by_name ("text", param_contents(tok));
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
      return;
    }
    break;
  case st_do:
    {
      /* Quest fetches the procedure name with GetParameter (V4Game.Part2.cs:5756),
       * which scans the whole line for the first <...> instead of demanding that
       * it be the very next token.  That matters for "do { ... }": de-inlining a
       * brace block keeps everything before the '{' and appends its own call, so
       * "if x then do {" becomes "if x then do do <!intprocN>" in Quest
       * (ConvertMultiLineSections, V4Game.cs:668) and identically in geas
       * (readfile.cc pass 3).  Requiring the parameter to follow "do" immediately
       * made geas silently drop every such block -- The Devil's Bargain writes
       * all of its nested conditionals that way, so e.g. the colour question in
       * its <Answers> procedure never printed its reply and never re-asked. */
      while ((tok = next_token (s, c1, c2)) != "" && !is_param (tok))
	;
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected parameter after do in " + s);
	  return;
	}
      string fname = eval_param (tok);
      std::string::size_type index = fname.find ('(');
      if (index != string::npos)
	{
	  std::string::size_type index2 = fname.find (')');
	  run_procedure (trim (fname.substr (0, index)),
			 split_f_args (fname.substr (index+1, index2-index-1)));
	}
      else
	run_procedure (fname);

      return;
    }
    break;
  case st_doaction:
    {
      tok = next_token (s, c1, c2);
      if (!is_param(tok))
	{
	  gi->debug_print ("Expected parameter after doaction in " + s);
	  return;
	}
      string line = eval_param (tok);
      std::string::size_type index = line.find (';');
      string obj = trim (line.substr (0, index));
      string act = trim (line.substr (index + 1));
      string old_object = this_object;
      this_object = obj;
      if (get_obj_action (obj, act, tok))
	run_script_as (obj, tok);
      else
	gi->debug_print ("No action defined for " + obj + " // " + act);
      this_object = old_object;
      return;
    }
    break;
  case st_dontprocess:
    {
      dont_process = true;
      return;
    }
    break;
  case st_enter:
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected parameter after enter in " + s);
	  return;
	}
      tok = eval_param (tok);
      set_svar (tok, input_to_utf8 (gi->get_string()));
      return;
    }
    break;
  case st_exec:
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected parameter after exec in " + s);
	  return;
	}
      tok = eval_param (tok);
      std::string::size_type index = tok.find (';');
      if (index != string::npos)
	{
	  /* Both halves are trimmed, and the modifier compared case-sensitively
	   * with == (V4Game.Part2.cs:2243-2253). */
	  string cmd = trim (tok.substr (0, index));
	  string tmp = trim (tok.substr (index+1));
	  if (tmp != "normal")
	    {
	      /* Quest logs the bad modifier and runs nothing at all: the else
	       * arm of ExecExec never reaches ExecCommand (ibid. 2251-2254), so
	       * there is no turn and no separator either. */
	      gi->debug_print ("Bad " + tmp + " in exec in " + s);
	      return;
	    }
	  /* An empty command line is not an error and does not even end a
	   * paragraph: ExecCommand returns on the spot, before the hooks, the
	   * error path and its closing Print("") (ibid. 4137-4141).  "The Quest
	   * to find The Dark Hills" execs one from a `choice` whose "no" branch
	   * does nothing. */
	  if (cmd == "")
	    return;
	  run_turn (cmd, true, true);
	}
      else
	{
	  /* Not trimmed, unlike the two halves of the `;` form: ExecExec hands
	   * this arm the parameter as it stands (ibid. 2231), and a command of
	   * nothing but spaces is not the empty one that returns early -- Quest
	   * runs the turn and answers the error.
	   *
	   * An exec'd command that matches nothing gets the same answer a typed
	   * one does: ExecuteExec hands the line to the whole of ExecCommand,
	   * error path included (ibid. 2229, 4574).  stdverbs.lib needs it --
	   * its splitter turns "speak to mr. cake" into "speak to mr" plus a
	   * stray "cake", and Quest answers the stray half.  And the whole of
	   * ExecCommand means the turn hooks as well, so the exec'd command runs
	   * its own beforeturn and afterturn (see run_turn). */
	  if (tok == "")
	    return;
	  run_turn (tok, true, false);
	}
      /* Quest ends every ExecCommand with Print("") (V4Game.Part2.cs:4606), the
       * blank line that separates one turn's output from the next.  Geas emits
       * that separator at the top of run_command instead, which comes to the
       * same thing for a command the player typed but leaves an exec'd one
       * butted up against whatever follows.  stdverbs.lib's splitter is built
       * entirely out of exec, so "north, then south" needs the break here. */
      print_newline ();
      return;
    }
    break;
  case st_flag:
    {
      tok = next_token (s, c1, c2);
      bool is_on;
      /* CI: ExecuteFlag tests "on "/"off " with BeginsWith
       * (V4Game.cs:3686-3690). */
      if (ci_equal (tok, "on"))
	is_on = true;
      else if (ci_equal (tok, "off"))
	is_on = false;
      else
	{
	  gi->debug_print ("Expected 'on' or 'off' after flag in " + s);
	  return;
	}
      string onoff = tok;

      tok = next_token (s, c1, c2);
      if (is_param (tok))
	set_obj_property ("game", (is_on ? "" : "not ") + eval_param (tok));
      else
	gi->debug_print ("Expected param after flag " + onoff + " in " + s);
      return;
    }
    break;
  case st_for:
    {
      tok = next_token (s, c1, c2);
      /* "each" and every sub-keyword below are CI: ExecForEach reads them
       * all with BeginsWith (V4Game.cs:7212, 4975-5017). */
      if (ci_equal (tok, "each"))
	{
	  /* Quest's ExecForEach takes three kinds of thing -- "object", "exit"
	   * and "room" -- and serves all three from the one _objs array, picking
	   * out the records whose IsRoom and IsExit flags match what was asked
	   * for (V4Game.cs:4969-5042).  So "for each object in" never sees a room
	   * or an exit.  geas keeps rooms in state.objs beside the objects (they
	   * are the records with is_room set) and does not model exits as objects
	   * at all, so the three cases split up here.  Only "object" used to be
	   * implemented, and it walked the rooms too. */
	  tok = next_token (s, c1, c2);
	  bool want_room = ci_equal (tok, "room"), want_exit = ci_equal (tok, "exit");
	  if ((ci_equal (tok, "object") || want_room || want_exit) &&
	      ci_equal (next_token (s, c1, c2), "in"))
	    {
	      tok = next_token (s, c1, c2);
	      string script = s.substr (c2);
	      if (ci_equal (tok, "game"))
		{
		  /* "in game" leaves inLocation empty, which matches every
		   * container -- including none, so the "game" pseudo-object
		   * (state.objs[0], Quest's _objs[1]) is walked as well. */
		  if (want_exit)
		    {
		      for (const auto &i: state.objs)
			if (i.is_room)
			  for (const string &e: exit_object_names (i.name))
			    {
			      set_svar ("quest.thing", e);
			      run_script (script);
			    }
		      return;
		    }
		  for (const auto &i: state.objs)
		    if (i.is_room == want_room)
		      {
			GEAS_DBG << "  quest.thing -> " + i.name + "\n";
			set_svar ("quest.thing", i.name);
			run_script (script);
		      }
		  return;
		}
	      else if (is_param (tok))
		{
		  tok = trim (eval_param (tok));
		  if (want_exit)
		    {
		      for (const string &e: exit_object_names (tok))
			{
			  set_svar ("quest.thing", e);
			  run_script (script);
			}
		      return;
		    }
		  /* The place is a *room*, matched against ContainerRoom, so
		   * "for each object in <box>" iterates nothing at all while a
		   * box's contents turn up in their room's own loop.  Wizard's
		   * church turns on that: its DETECT MAGIC sweeps the room and
		   * has to reach the Yellow Bead inside the basket.  Quest
		   * compares lower-cased both sides, so the casing a move or a
		   * "parent" line happened to use does not matter. */
		  string container = lcase (tok);
		  for (const auto &i: state.objs)
		    if (i.is_room == want_room && lcase (i.parent) == container)
		      {
			set_svar ("quest.thing", i.name);
			run_script (script);
		      }
		  return;
		}
	    }
	}
      else if (is_param (tok))
	{
	  vector<string> args = split_param (eval_param (tok));
	  if (args.size() < 3)
	    {
	      /* args[1] / args[2] used to be read unconditionally, so a truncated
	       * "for <i; 5>" indexed off the end of the vector. */
	      gi->debug_print ("Expected <variable; start; end[; step]> in " + s);
	      return;
	    }
	  string varname = args[0];
	  string script = s.substr (c2);
	  int startindex = parse_int (args[1]);
	  int endindex = parse_int (args[2]);
	  int step = 1;
	  if (args.size() > 3)
	    step = parse_int (args[3]);
	  if (step == 0)
	    {
	      gi->debug_print ("Zero step would loop forever in " + s);
	      return;
	    }
	  /* Quest's FOR includes its end value and counts down on a negative step
	   * (it is VB's For/Next).  The old strict "< endindex" dropped the final
	   * iteration of every loop -- the bundled type library walks a string with
	   * "for <n; 1; %lengthof%>", so its last character was never seen -- and
	   * ran a negative step zero times. */
	  for (set_ivar (varname, startindex);
	       step > 0 ? get_ivar (varname) <= endindex
			: get_ivar (varname) >= endindex;
	       set_ivar (varname, get_ivar (varname) + step))
	    run_script (script);
	  return;
	}

    }
    break;
  case st_foreground:
    {
      tok = next_token (s, c1, c2);
      if (is_param (tok))
	gi->set_foreground (eval_param (tok));
      else
	gi->debug_print ("Expected parameter after foreground in " + s);
      return;
    }
    break;
  case st_give:
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected parameter after give in " + s);
	  return;
	}
      tok = eval_param (tok);
      /* Add to the Quest 2.x item inventory (deduplicated).  A game may give an
       * item and then hide the like-named room object, so items are tracked
       * separately from objects.  Only below ASL 2.80, though: from there on
       * PlayerItem ignores the item table entirely and just moves the *object*
       * of that name into "inventory" (V4Game.Part2.cs:6594-6647), so keeping a
       * parallel item would outlive the object -- a 3.x game that gives the
       * stone and later drops it would still list it and still pass
       * "if got <stone>". */
      if (asl_version_ < 280)
	{
	  /* Below 2.80 "give" sets the Got flag of a *declared* item and does
	   * nothing else: PlayerItem walks the table for an exact, case-
	   * sensitive name match and simply falls off the end when there is
	   * none (V4Game.Part2.cs:6681-6690).  So a game can only ever hand
	   * over something its `items' line named, and the spelling stored is
	   * the table's, not the give's -- which is what the inventory prints.
	   * The Hobbit's "give <Goblin knife>" and "give <ring>" name nothing
	   * in its table and hand over nothing, in Quest and now here. */
	  if (const string *decl = find_item (tok))
	    {
	      bool have_item = false;
	      for (const string &it: state.items)
		if (it == *decl) { have_item = true; break; }
	      if (!have_item)
		state.items.push_back (*decl);
	    }
	}
      /* From ASL 2.80 on there is no item table to speak of: PlayerItem looks up
       * the *object* of that name and moves it to "inventory", running its gain
       * script (V4Game.Part2.cs:6604-6647).  Below 2.80 it only sets the Got
       * flag on the item and never touches an object, even one with exactly the
       * same name (ibid. 6658-6669), so an object must not follow the item into
       * the inventory here: The Dream Weaver (Bad Omen, ASL 217) takes the Hooka
       * with "give <Hooka>  hideobject <Hooka>", the 2.x idiom of handing over
       * the item and hiding the like-named scenery object it was sitting as, and
       * moving the object first made the hide look like a held object being
       * destroyed -- so the hooka vanished and the game was unwinnable from its
       * second puzzle on. */
      /* obj_records rather than a scan over state.objs so the name is matched
       * the same trimmed way everything else matches it: "Something 'Bout A Hex"
       * hands the journal over as `give <Journal >', and the object is
       * `define object <Journal >', but geas stores block names trimmed. */
      bool is_object = (asl_version_ >= 280 && state.obj_records (tok) != NULL);
      if (is_object)
	{
	  /* "Unset parent information, if any" -- taking something into the
	   * inventory takes it out of whatever container it was in, from ASL
	   * 391 on (PlayerItem, V4Game.Part2.cs:6646-6652).  It has to happen
	   * before the move, because move drags a container's contents along
	   * with it: Gathered in Darkness opens its radio with `show
	   * <Batteries>  give <Batteries>  move <Radio; INVENT HOLD>', and the
	   * batteries would follow the radio out of the world -- and the
	   * flashlight would never light -- if they were still its children.
	   * Note the bare property write: this is not DoAddRemove, so no
	   * container visibility sweep follows. */
	  if (asl_version_ >= 391)
	    set_obj_property (tok, "not parent");
	  move (tok, "inventory");
	  string tmp;
	  if (get_obj_action (tok, "gain", tmp))
	    run_script_as (tok, tmp);
	  else if (get_obj_property (tok, "gain", tmp))
	    print_formatted (tmp);
	}
      gi->update_sidebars();
      return;
    }
    break;
  case st_goto:
    {
      tok = next_token (s, c1, c2);
      if (is_param(tok))
	goto_room (trim (eval_param (tok)));
      else
	gi->debug_print ("Expected parameter after goto in " + s);
      return;
    }
  /* "hideobject"/"hidechar" are the Quest 2.x spellings of "hide". */
    break;
  case st_hide: case st_hideobject: case st_hidechar:
    {
      tok = next_token (s, c1, c2);
      if (is_param(tok))
	{
	  string name = eval_param (tok);
	  if (!resolve_at_room (name))
	    return;
	  set_obj_property (name, "hidden");
	  /* Hiding a *held* object removes it for good (e.g. a vase that
	   * "smashes" on drop, or a key consumed by a use action): also drop it
	   * from the Quest-2.x item list, so it stops counting as held and the
	   * item-name fallback in dereference_vars no longer resolves it for
	   * take/examine.  A hidden *room* object keeps its item (the
	   * give-an-item-then-hide-the-like-named-room-object case). */
	  for (const auto &o: state.objs)
	    if (ci_equal (o.name, name) && ci_equal (o.parent, "inventory"))
	      {
		for (auto i = state.items.begin (); i != state.items.end (); )
		  if (ci_equal (*i, name)) i = state.items.erase (i); else ++ i;
		break;
	      }
	}
      else
	gi->debug_print ("Expected param after hide in " + s);
      return;
    }
  /* "showobject"/"showchar" are the Quest 2.x spellings of "show". */
    break;
  case st_show: case st_showobject: case st_showchar:
    {
      /* Below ASL 2.82 `show <...>' is the *picture* statement: the picture
       * branch of the dispatcher sits above the object one and claims the word
       * (V4Game.Part2.cs:5842 against 5904), so showing an object by that
       * spelling effectively begins at 2.82 even though its own gate reads
       * >= 2.81.  A game older than that says `showobject' or `showchar'. */
      if (stmt_it->second == st_show && asl_version_ < 282)
	{
	  std::string::size_type lt = s.find ('<', c2);
	  if (lt != string::npos)
	    c2 = lt;
	  tok = next_token (s, c1, c2);
	  if (is_param (tok))
	    show_picture (eval_param (tok), true);
	  return;
	}
      tok = next_token (s, c1, c2);
      if (is_param(tok))
	{
	  string name = eval_param (tok);
	  if (!resolve_at_room (name))
	    return;
	  set_obj_property (name, "not hidden");
	}
      else
	gi->debug_print ("Expected param after show in " + s);
      return;
    }
    break;
  case st_if:
    {
      std::string::size_type begin_cond = c2 + 1, end_cond, begin_then, end_then;

      /* "then" -- and "else" below -- is matched case-sensitively, as in
       * Quest: ExecuteIf locates both with a plain InStr
       * (V4Game.Part2.cs:5631, 5644). */
      do {
	tok = next_token (s, c1, c2);
      } while (tok != "then" && tok != "");

      if (tok == "")
	{
	  gi->debug_print ("Expected then in if: " + s);
	  return;
	}
      end_cond = c1;
      string cond_str = s.substr (begin_cond, end_cond - begin_cond);

      /* The then-branch starts immediately after the "then" token, not one
       * character further on: c2 is already the index just past the 'n', and
       * ExecuteIf likewise takes everything from InStr("then") + 4 onwards
       * (V4Game.Part2.cs:5641).  The old "c2 + 1" only happened to work because
       * a space usually follows "then"; when "then" ends the line -- an author
       * writing
       *
       *     if got <sunglasses> then
       *     msg <...>
       *
       * inside a brace block, which readfile's de-inlining pass splits into two
       * separate statements exactly as Quest's ConvertMultiLineSections does --
       * c2 equals s.length() and the substr below threw std::out_of_range,
       * aborting the process.  Quest instead hands ExecuteScript an empty
       * then-script, does nothing, and runs the next line unconditionally.
       * "Pyramid Of Terror" crashed on USE CANE for this reason. */
      begin_then = c2;
      int brace_count = 0;
      do {
	tok = next_token (s, c1, c2);
	for (uint i = 0; i < tok.length(); i ++)
	  if (tok[i] == '{')
	    brace_count ++;
	  else if (tok[i] == '}')
	    brace_count --;
      } while (tok != "" && !(brace_count == 0 && tok == "else"));
      end_then = c1;

      if (eval_conds (cond_str))
	run_script (s.substr (begin_then, end_then - begin_then), rv);
      else if (c2 < s.length())
	run_script (s.substr (c2), rv);
      return;
    }
    break;
  case st_inc: case st_dec:
    {
      bool is_dec = (tok == "dec");
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected parameter after inc in " + s);
	  return;
	}
      tok = eval_param (tok);
      double diff;
      std::string::size_type index = tok.find (';');
      string varname;
      if (index == string::npos)
	{
	  varname = trim (tok);
	  diff = 1;
	}
      else
	{
	  varname = trim (tok.substr (0, index));
	  diff = eval_double (tok.substr (index+1));
	}
      /* Quest reads the current value with GetNumericContents, whose "no such
       * variable" answer is -32767, and then clamps anything at or below -32766
       * to zero before applying the change (ExecuteIncDec, V4Game.cs:3705), so
       * inc/dec on a variable the game never declared counts from 0 and creates
       * it.  Without the clamp "inc <degrees turned; 90>" leaves -32677 and
       * every "case <180>" past it is dead -- which is exactly what happened to
       * the spin scoring in "Skate ur @SS off" (fixtures/undefnum.asl). */
      double cur = get_dvar (varname);
      if (cur <= -32766.0)
	cur = 0;
      set_ivar (varname, is_dec ? cur - diff : cur + diff);
      return;
    }
    break;
  case st_lose:
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected parameter after lose in " + s);
	  return;
	}
      tok = eval_param (tok);

      /* Clear the Got flag of the Quest 2.x item, matched the same exact,
       * case-sensitive way "give" sets it (PlayerItem, V4Game.Part2.cs:
       * 6681-6690).  A miscased name therefore takes nothing away, and two of
       * The Dream Weaver's procedures are written that way: "lose <hooka>" and
       * "lose <good nugget>" against an `items <Good Nugget; Hooka; ...>'
       * declaration match nothing at all, so in Quest the player keeps both.
       * (Only a script line can hit this.  A name the player typed has been
       * lowercased by the time a command's parameter reaches here, so it can
       * only ever match a table entry that is lowercase already.) */
      if (const string *decl = find_item (tok))
	{
	  for (auto it = state.items.begin(); it != state.items.end(); )
	    {
	      if (*it == *decl)
		it = state.items.erase (it);
	      else
		++ it;
	    }
	}

      /* The mirror image of give: only from ASL 2.80 on does losing an item mean
       * moving the object of that name into the current room (V4Game.Part2.cs:
       * 6636-6647).  Below that, items and objects are separate tables and
       * "lose" is a flag on the item alone. */
      /* Wherever the object was: Quest's PlayerItem moves it into the current
       * room and runs its lose script without looking at where it came from
       * (V4Game.Part2.cs:6652-6660), so "lose" means "put this object where I
       * am standing", not "drop it if I am holding it".  st_give, its mirror
       * image, has never had such a test either. */
      bool is_object = (asl_version_ >= 280 && state.obj_records (tok) != NULL);
      if (is_object)
	{
	  move (tok, state.location);
	  string tmp;
	  if (get_obj_action (tok, "lose", tmp))
	    run_script_as (tok, tmp);
	  else if (get_obj_property (tok, "lose", tmp))
	    print_formatted (tmp);
	}
      gi->update_sidebars();
      return;
    }
  /* "movechar"/"moveobject" are the Quest 2.x spellings of "move". */
    break;
  case st_move: case st_movechar: case st_moveobject:
    {
      tok = next_token (s, c1, c2);
      if (!is_param(tok))
	{
	  gi->debug_print ("Expected parameter after move in " + s);
	  return;
	}
      tok = eval_param (tok);
      std::string::size_type index = tok.find (';');
      if (index == string::npos)
	{
	  gi->debug_print ("No semi in " + tok + " in " + s);
	  return;
	}
      move (trim (tok.substr (0, index)), trim (tok.substr (index + 1)));
      return;
    }
  /* "helpmsg" is just "msg".  In Quest 4 it printed into the separate help
   * window; the Quest 5 player has no such window, so ExecuteScript sends it to
   * Print exactly like msg (V4Game.Part2.cs:5778-5781), and so does geas, which
   * likewise has one output stream.  It used to be a no-op here, which swallowed
   * ThunderClan Mystery 1's "***TYPER GET HERB***" prompt whole.  "helpclose"
   * and "helpclear" are the ones that really do nothing (ibid. 5782, 5988). */
    break;
  case st_msg: case st_helpmsg:
    {
      string stmt = tok;
      /* Quest reads the message with GetParameter, which takes whatever lies
       * between the line's first '<' and its first '>' and never looks at what
       * comes before the '<' (V4Game.cs:1858-1874).  So a modifier word
       * between the statement and its parameter is simply invisible to it --
       * `nospeak`, the ASL keyword that suppresses the text-to-speech reading
       * of a message (Libraries/quest.dat:204), is the one games write.  geas
       * demanded the parameter as the very next token and dropped the whole
       * message when it was not, which lost King's Quest V's and The Former's
       * `msg nospeak <...>` lines. */
      std::string::size_type lt = s.find ('<', c2);
      if (lt != string::npos)
	c2 = lt;
      tok = next_token (s, c1, c2);
      if (is_param (tok))
	print_eval (param_contents(tok));
      else
	gi->debug_print ("Expected parameter after " + stmt + " in " + s);
      return;
    }
    break;
  case st_outputoff:
    {
      outputting = false;
      return;
    }
    break;
  case st_outputon:
    {
      outputting = true;
      return;
    }
    break;
  case st_nointro:
    {
      /* Suppress the automatic <intro> display that would otherwise follow the
       * startscript (see auto_intro_). */
      auto_intro_ = false;
      return;
    }
    break;
  case st_pause:
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected parameter after pause in " + s);
	  return;
	}
      int i = (int) eval_double (param_contents(tok));
      gi->pause (i);
      suspend_turn ();          /* PauseAsync suspends the turn, as `wait` does */
      return;
    }
    break;
  case st_picture:
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
       * `picture' is not a statement at all -- see st_show for the spelling it
       * had then.  `picture close' closes a window geas never opened, so it
       * does nothing here, as it does nothing in QuestViva (ibid. 5836). */
      tok = next_token (s, c1, c2);
      if (ci_equal (tok, "close"))
	return;
      bool popup = ci_equal (tok, "popup");
      if (popup)
	tok = next_token (s, c1, c2);
      if (is_param (tok) && asl_version_ >= 282)
	show_picture (eval_param (tok), popup || asl_version_ < 390);
      return;
    }
    break;
  case st_playerlose:
    {
      run_script ("displaytext <lose>");
      state.running = false;
      is_running_ = false;   /* end the game so the host stops prompting */
      return;
    }
    break;
  case st_playerwin:
    {
      run_script ("displaytext <win>");
      state.running = false;
      is_running_ = false;   /* end the game so the host stops prompting */
      return;
    }
    break;
  case st_playwav: case st_playmidi: case st_playmod:
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
	  return;
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
      return;
    }
    break;
  case st_property:
    {
      tok = next_token (s, c1, c2);
      if (!is_param(tok))
	{
	  gi->debug_print ("Expected parameter in '" + s + "'");
	  return;
	}
      vector<string> args = split_param (eval_param (tok));
      for (size_t i = 1; i < args.size(); i ++)
	{
	  string val = args[i];
	  val = trim_braces(val);
	  set_obj_property (args[0], val);
	}
      return;
    }
    break;
  case st_repeat:
    {
      tok = next_token (s, c1, c2);
      /* CI: ExecuteRepeat tests "while "/"until " with BeginsWith
       * (V4Game.cs:6002-6007). */
      if (!ci_equal (tok, "while") && !ci_equal (tok, "until"))
	{
	  gi->debug_print ("Expected while or until after repeat in " + s);
	  return;
	}
      bool is_while = ci_equal (tok, "while");
      std::string::size_type start_cond = c2, end_cond = string::npos;
      /* Quest does not use a keyword to separate the condition from the loop
       * body: ExecuteRepeat (V4Game.cs:6018-6034) walks the '>' characters and
       * stops at the first one whose remainder does not continue the condition
       * with "and " or "or ".  Everything up to and including that '>' is the
       * condition, everything after it is the script.  geas used to look for a
       * literal "do" instead, which happens to give the same answer for the
       * common "repeat until flag <f> do <proc>" shape but silently dropped the
       * whole loop for anything else -- "Shipwrecked" drives its colored-hole
       * puzzle with "repeat until flag <stoneb1> choose <coloredholes>", which
       * left the rod doing nothing at all and the game unfinishable.
       */
      for (std::string::size_type p = start_cond;
	   (p = s.find ('>', p)) != string::npos; ++p)
	{
	  string rest = trim (s.substr (p + 1));
	  if (rest == "")
	    break;
	  if (rest.compare (0, 4, "and ") == 0 || rest.compare (0, 3, "or ") == 0)
	    continue;
	  end_cond = p + 1;
	  break;
	}
      if (end_cond == string::npos)
	{
	  gi->debug_print ("No script found after condition in " + s);
	  return;
	}
      string cond = trim (s.substr (start_cond, end_cond - start_cond));
      string script = trim (s.substr (end_cond));
      GEAS_DBG << "Interpreting '" << s << "' as ("
	   << (is_while ? "WHILE" : "UNTIL") << ") ("
	   << cond << ") {" << script << "}\n";
      while (state.running && eval_conds (cond) == is_while)
	run_script(script);
      return;
    }
    break;
  case st_return:
    {
      tok = next_token (s, c1, c2);
      if (is_param(tok))
	rv = eval_param(tok);
      else
	gi->debug_print ("Expected parameter after return in " + s);
      return;
    }
  /* "reveal"/"conceal" (ASL>=281) and their "…object"/"…char" spellings
   * (ASL<281) toggle Quest's *visibility* flag (SetVisibility -> .Visible in
   * LegacyGame.vb), which gates only the room/inventory listing.  This is a
   * different axis from show/hide (SetAvailability -> .Exists), which gates
   * verb scope: DisambObjHere resolves a typed noun on .Exists alone and never
   * consults .Visible, so a concealed-but-existing object stays referenceable.
   * Hence these must map to the "invisible" property, NOT "hidden". */
    break;
  case st_reveal: case st_revealobject: case st_revealchar:
    {
      tok = next_token (s, c1, c2);
      if (is_param(tok))
	set_obj_property (eval_param (tok), "not invisible");
      else
	gi->debug_print ("Expected param after reveal in " + s);
      return;
    }
    break;
  case st_conceal: case st_concealobject: case st_concealchar:
    {
      tok = next_token (s, c1, c2);
      if (is_param(tok))
	set_obj_property (eval_param (tok), "invisible");
      else
	gi->debug_print ("Expected param after conceal in " + s);
      return;
    }
    break;
  case st_say:
    {
      tok = next_token (s, c1, c2);
      if (is_param (tok))
	{
	  tok = eval_param (tok);
	  print_formatted ("\"" + tok + "\"");
	}
      else
	gi->debug_print ("Expected param after say in " + s);
      return;
    }
    break;
  case st_set:
    {
      string vartype = "";
      tok = next_token (s, c1, c2);
      /* Below ASL 2.80 "set" means one thing only: a collectable.  Numeric and
	 string variables did not exist yet, so ExecuteSet does not even look at
	 the keyword that follows (V4Game.Part2.cs:6157-6160). */
      if (asl_version_ < 280)
	{
	  /* GetParameter reads the first <...> in the line whatever precedes
	     it, so "set interval <t; 5>" is a (failing) collectable set too. */
	  while (tok != "" && !is_param (tok))
	    tok = next_token (s, c1, c2);
	  if (!is_param (tok))
	    {
	      gi->debug_print ("Expected parameter in " + s);
	      return;
	    }
	  run_set_collectable (eval_param (tok));
	  return;
	}
      /* CI, like the type keywords below: ExecuteSet reads them all with
       * BeginsWith (V4Game.Part2.cs:6104-6141). */
      if (ci_equal (tok, "interval"))
	{
	  tok = next_token (s, c1, c2);
	  if (!is_param (tok))
	    {
	      gi->debug_print ("Expected param after set interval in " + s);
	      return;
	    }
	  tok = eval_param (tok);
	  std::string::size_type index = tok.find (';');
	  if (index == string::npos)
	    {
	      gi->debug_print ("No semicolon in param in " + s);
	      return;
	    }
	  string timer_name = trim (tok.substr (0, index));
	  uint time_val = parse_int (trim (tok.substr (index+1)));

	  /* Timer names compare LCase'd (V4Game.Part2.cs:6120). */
	  for (TimerRecord &i: state.timers)
	    if (ci_equal (i.name, timer_name))
	      {
		i.interval = time_val;
		return;
	      }
	  gi->debug_print ("no interval named " + timer_name + " found!");
	  return;
	}
      if (ci_equal (tok, "string") || ci_equal (tok, "numeric") ||
	  ci_equal (tok, "collectable"))
	{
	  vartype = lcase (tok);
	  tok = next_token (s, c1, c2);
	}
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected parameter in " + s);
	  return;
	}
      if (tok.find (';') == string::npos)
	{
	  gi->debug_print ("Only one expression in set in " + s);
	  return;
	}
      tok = eval_param (tok);
      std::string::size_type index = tok.find (';');
      string varname = trim (tok.substr (0, index));
      if (vartype == "")
	{
	  /* An untyped "set" finds its type by name.  Quest scans the string
	     variables first and only then the numeric ones (SetUnknownVariableType,
	     V4Game.Part2.cs:592-608), which decides the case where a name has
	     been used for both -- see set_svar.  Both scans compare with LCase on
	     each side, so the casing the game happens to type does not matter;
	     Zombies Attack.cas leans on that, resetting its "Chance factor" (made
	     by the startscript's `set numeric <Chance factor; 0>`) with `set
	     <chance factor; 0>` before every random roll.  Missing the variable
	     there would leave the counter to accumulate and make every one of the
	     game's chance rolls fail. */
	  for (const auto &varn: state.svars)
	    {
	      if (ci_equal (varn.name, varname))
		{
		  vartype = "string";
		  break;
		}
	    }
	  if (vartype == "")
	    {
	      for (const auto &varn: state.ivars)
		{
		  if (ci_equal (varn.name, varname))
		    {
		      vartype = "numeric";
		      break;
		    }
		}
	    }
	  /* Collectables are searched last, so a variable of the same name
	     shadows one (SetUnknownVariableType, V4Game.Part2.cs:612-618).  This
	     search is case-insensitive too, but the set it dispatches to is not
	     (run_set_collectable uses find_collectable), so a collectable reached
	     by a name of the wrong case is then reported as nonexistent -- which
	     is what Quest does too. */
	  if (vartype == "")
	    {
	      size_t ci;
	      if (find_collectable_ci (varname, ci))
		vartype = "collectable";
	    }
	}
      if (vartype == "collectable")
	{
	  run_set_collectable (tok);
	  return;
	}
      if (vartype == "")
	{
	  gi->debug_print ("Undefined variable " + varname + " in " + s);
	  return;
	}
      if (vartype == "string")
	{
	  set_svar (varname, trim_braces (trim (tok.substr(index+1))));
	}
      else
	{
	  set_ivar (varname, eval_set_numeric (tok.substr (index+1)));
	}
      return;
    }
    break;
  case st_setstring:
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected parameter in " + s);
	  return;
	}
      if (tok.find (';') == string::npos)
	{
	  gi->debug_print ("Only one expression in set in " + s);
	  return;
	}
      tok = eval_param (tok);
      std::string::size_type index = tok.find (';');
      string varname = trim (tok.substr (0, index));
      set_svar (varname, trim_braces (trim (tok.substr (index+1))));
      return;
    }
    break;
  case st_setvar:
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected parameter in " + s);
	  return;
	}
      if (tok.find (';') == string::npos)
	{
	  gi->debug_print ("Only one expression in set in " + s);
	  return;
	}
      tok = eval_param (tok);
      std::string::size_type index = tok.find (';');
      string varname = trim (tok.substr (0, index));
      set_ivar (varname, eval_set_numeric (tok.substr (index+1)));
      return;
    }
    break;
  case st_speak:
    {
      tok = next_token (s, c1, c2);
      if (is_param(tok))
	gi->speak (eval_param (tok));
      else
	gi->debug_print ("Expected param after speak in " + s);
      return;
    }
    break;
  case st_stop:
    {
      /* "stop" is FinishGame (StopType.Null) (V4Game.Part2.cs:5798-5801): the
       * game is over exactly as it is after playerwin/playerlose, only with no
       * win/lose text.  Clearing state.running alone left the host prompting
       * for commands that could no longer do anything. */
      state.running = false;
      is_running_ = false;
      return;
    }
    break;
  case st_timeron: case st_timeroff:
    {
      bool running = (tok == "timeron");
      tok = next_token (s, c1, c2);
      if (is_param (tok))
	{
	  tok = eval_param (tok);
	  /* Timer names are matched case-insensitively: SetTimerState LCases
	   * both sides (V4Game.Part2.cs:565). */
	  for (auto &timer: state.timers)
	    if (ci_equal (timer.name, tok))
	      {
		/* Quest's SetTimerState only flips TimerActive and raises
		 * BypassThisTurn; it never touches TimerTicks
		 * (V4Game.Part2.cs:561-574).  So timeron does NOT restart the
		 * countdown -- a running timer keeps counting from where it
		 * was, and a timer switched off and on again resumes rather
		 * than starting over.  geas used to reset timeleft here, which
		 * deadlocks any pair of timers where a short one re-arms a
		 * longer one: in "Something 'Bout A Hex" the Louisville
		 * sightseeing drive is `christchurch' (interval 2) arming
		 * `christchurch2' (interval 3), and only christchurch2 can turn
		 * christchurch off, so with a resetting timeron christchurch2
		 * was re-armed before it could ever reach its interval and the
		 * chain that opens the way to the old building never advanced
		 * past its first step. */
		timer.bypass = true;
		timer.is_running = running;
		return;
	      }
	  gi->debug_print ("No timer " + tok + " found");
	  return;
	}
      gi->debug_print (string ("Expected parameter after timer") +
		       (running ? "on" : "off") + " in " + s);
      return;
    }
    break;
  case st_wait:
    {
      tok = next_token (s, c1, c2);
      /* Quest tests the *unevaluated* rest of the line (ExecuteWaitAsync,
       * V4Game.cs:6103-6118), so `wait <>' counts as carrying a message and
       * prints it -- nothing -- where a `wait' with no parameter at all falls
       * through to the default.  Prison Break and MetroidLite both open with a
       * `wait <>' and neither wants the default text. */
      bool bare_wait = (tok == "");
      if (!bare_wait)
	{
	  if (!is_param(tok))
	    {
	      gi->debug_print ("Expected parameter after wait in " + s);
	      return;
	    }
	  tok = eval_param (tok);
	}
      /* And a bare `wait' is not silent.  From ASL 4.10 it prints the
       * overridable `defaultwait' error message; below 4.10 it prints a fixed
       * string that no `error <defaultwait; ...>' can reach, preceded by a
       * blank line.  Only the `wait' statement does this -- the `|w' code
       * waits with no message of its own (Part2:6747-6753), which is why the
       * message is built here and not in wait_keypress. */
      if (bare_wait)
	{
	  if (asl_version_ >= 410)
	    display_error ("defaultwait");
	  else
	    print_formatted ("|nPress a key to continue...");
	}
      gi->wait_keypress (tok);
      /* The turn suspends here, so this is where its timers tick (see
       * run_command).  Before the clear below, because in Quest the tick
       * happens while the game still sits at the keypress. */
      suspend_turn ();
      /* Quest clears the screen once the player presses a key; clear_screen
       * emits a blank-line separator (see GeasInterface::clear_screen). */
      gi->clear_screen ();
      return;
    }
    break;
  default:
    break;
    }
  gi->debug_print ("Unrecognized script " + s);
}

bool geas_implementation::eval_conds (const string &s)
{
  GEAS_DBG << "if (" + s + ")" << endl;

  std::string::size_type c1, c2;
  string tok = first_token (s, c1, c2);

  if (tok == "") return true;

  /* A port of Quest's ExecuteConditions (LegacyGame V4Game.cs): split the list
   * at its "and"/"or" joiners, then fold the conditions LEFT to right, each
   * combined with the joiner that *precedes* it (the first against an initial
   * true).  So "A and B or C" is "(A and B) or C".  The old code searched the
   * whole string for "and" and recursed on the remainder, which grouped it the
   * other way round -- "A and (B or C)" -- and disagreed with Quest whenever a
   * condition list mixed the two joiners.
   *
   * Two behaviours of the original are deliberately kept, because games were
   * written against the engine that had them:
   *  - at each split Quest looks for the next "and" anywhere ahead and only
   *    falls back to "or" when there is none, so an "or" standing before a
   *    later "and" is swallowed into the preceding condition's text and never
   *    evaluated: "A or B and C" means "A and C";
   *  - every condition is evaluated, even once the result is settled: the fold
   *    is VB's non-short-circuiting And/Or, and a condition may prompt the
   *    player (`ask <...>`).
   * Joiners inside a <parameter> are invisible here, as next_token returns a
   * whole "<...>" as one token -- Quest blanks them with ObliterateParameters.
   */
  vector<std::string::size_type> op_start, op_end;
  vector<bool> op_is_and;
  for (; tok != ""; tok = next_token (s, c1, c2))
    if (tok == "and" || tok == "or")
      {
	op_start.push_back (c1);
	op_end.push_back (c2);
	op_is_and.push_back (tok == "and");
      }

  bool rv = true;
  bool joiner_is_and = true;      /* Quest's operations(0) = "AND" */
  std::string::size_type pos = 0;
  size_t next_op = 0;
  for (;;)
    {
      /* The next "and" anywhere ahead, else the next joiner of any kind. */
      size_t use = next_op;
      while (use < op_is_and.size () && !op_is_and[use])
	use ++;
      if (use >= op_is_and.size ())
	use = next_op;
      bool is_final = (use >= op_start.size ());

      string cond = is_final ? s.substr (pos)
			     : s.substr (pos, op_start[use] - pos);
      bool this_rv = eval_cond (cond);
      rv = joiner_is_and ? (this_rv && rv) : (this_rv || rv);

      if (is_final)
	break;
      joiner_is_and = op_is_and[use];
      pos = op_end[use];
      next_op = use + 1;
    }

  GEAS_DBG << "if (" << s << ") --> " << (rv ? "true" : "false") << endl;
  return rv;
}

/* One side of an "is <a;b>" or "is <a;!=;b>" comparison, as Quest reads it.
 * From ASL 3.91 on ExecuteIfIs runs both operands through ExpressionHandler and
 * uses the result whenever the operand is arithmetic all the way through
 * (V4Game.cs:7479-7494), so "is <%diceout%+1;%totaldice%>" compares 2 with 2
 * rather than the string "1+1" with the string "2".  Anything that is not
 * arithmetic fails the expression and is compared verbatim, which is what the
 * rest of the games rely on; older games never evaluate either side. */
std::string geas_implementation::eval_is_operand (const string &s) const
{
  string out;
  if (asl_version_ >= 391 && eval_numeric_expr (s, out))
    return out;
  return s;
}

bool geas_implementation::eval_cond (const string &s)
{
  std::string::size_type c1, c2;
  string tok = first_token (s, c1, c2);
  /* Condition keywords (not/got/here/is/property/real/type/...) are
   * case-insensitive; fold the dispatch token once (see run_script). */
  tok = lcase (tok);
  if (tok == "not")
    return !eval_cond (s.substr (c2));
  else if (tok == "action")
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("expected parameter after property in " + s);
	  return false;
	}
      tok = eval_param (tok);
      std::string::size_type index = tok.find (';');
      if (index == string::npos)
	{
	  gi->debug_print ("Only one argument to property in " + s);
	  return false;
	}
      string obj = trim (tok.substr (0, index));
      string act = trim (tok.substr (index+1));
      return has_obj_action (obj, act);
    }
  else if (tok == "ask")
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("expected parameter after ask in " + s);
	  return false;
	}
      tok = eval_param (tok);
      return gi->choose_yes_no (tok);
    }
  else if (tok == "exists")
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("expected parameter after exists in " + s);
	  return false;
	}
      vector<string> args = split_param (eval_param (tok));
      bool do_report = false;
      for (uint i = 1; i < args.size(); i ++)
	/* CI + trimmed: Quest compares LCase(Trim(...)) == "report"
	 * (V4Game.cs:5905). */
	if (ci_equal (trim (args[i]), "report"))
	  do_report = true;
	else
	  gi->debug_print ("Got modifier " + args[i] + " after exists");
      if (const vector<size_t> *v = state.obj_records (args[0]))
	{
	  const ObjectRecord &o = state.objs[(*v)[0]];
	  /* An object "exists" (in the Quest sense the games rely on) only if it
	   * is placed AND not hidden.  A statically-defined object can sit in a
	   * room while flagged hidden until a show command reveals it; checking
	   * parent alone made such reveals dead code.  Note: only "hidden" is
	   * toggled by show/hide -- "invisible" (which merely suppresses room
	   * listing) must NOT gate existence, or objects revealed via show while
	   * still flagged invisible would wrongly read as nonexistent. */
	  return o.parent != "" && !has_obj_property (o.name, "hidden");
	}
      if (do_report)
	gi->debug_print ("exists " + args[0] + " failed due to nonexistence");
      return false;
    }
  else if (tok == "flag")
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("expected parameter after flag in " + s);
	  return false;
	}
      tok = trim (eval_param (tok));
      return has_obj_property ("game", tok);
    }
  else if (tok == "got")
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("expected parameter after got in " + s);
	  return false;
	}
      tok = trim (eval_param (tok));
      /* Quest 2.x item inventory -- consulted only below ASL 2.80, since
       * ExecuteIfGot looks at nothing but the object's room from there on
       * (V4Game.cs:7370-7382). */
      if (asl_version_ < 280)
	for (const string &it: state.items)
	  if (ci_equal (it, tok))
	    return true;
      if (const vector<size_t> *v = state.obj_records (tok))
	{
	  const ObjectRecord &o = state.objs[(*v)[0]];
	  /* "(ContainerRoom == "inventory") & Exists" -- being in the inventory
	   * is not enough, the object must also not have been hidden
	   * (V4Game.cs:7370-7378).  Hiding a held object leaves it filed under
	   * "inventory" but clears Exists, so Quest stops reporting it as got,
	   * exactly as the inventory listing stops showing it.  This matters for
	   * the 2.x "give the item, hide the object" idiom the other way round:
	   * from 2.80 on give/lose really do move the object, so a game that
	   * hides what it just gave you has taken it back again. */
	  /* The room field alone, with no walk up the container chain: something
	   * in a carried container is already filed under "inventory", because
	   * DoAddRemove copies the container's ContainerRoom onto whatever is put
	   * inside it (V4Game.cs:2117-2121) and MoveThing carries the contents
	   * along afterwards.  "Shipwrecked" turns on this: its box of matches
	   * only survives the swim to the third island inside the jar of honey,
	   * and the barrel of gunpowder that clears the rubble is lit by
	   * `if not got <Box of matches>`. */
	  return ci_equal (o.parent, "inventory")
	    && (asl_version_ < 280 || !has_obj_property (o.name, "hidden"));
	}
      gi->debug_print ("No object " + tok + " found while evaling " + s);
      return false;
    }
  else if (tok == "has")
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("expected parameter after has in " + s);
	  return false;
	}
      return eval_has (eval_param (tok));
    }
  else if (tok == "here")
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("expected parameter after here in " + s);
	  return false;
	}
      tok = trim (eval_param (tok));
      if (const vector<size_t> *v = state.obj_records (tok))
	{
	  const ObjectRecord &o = state.objs[(*v)[0]];
	  /* "here" means filed under this room AND not hidden (ExecuteIfHere,
	   * V4Game.cs:5880-5891).  What is inside a container here has the same
	   * room, so it is here too; an object removed with "hide" (e.g. a
	   * creature that has run off) is not, even though its container stands. */
	  return (ci_equal (o.parent, state.location) &&
		  !has_obj_property (o.name, "hidden"));
	}
      gi->debug_print ("No object " + tok + " found while evaling " + s);
      return false;
    }
  else if (tok == "is")
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("expected parameter after is in " + s);
	  return false;
	}
      tok = eval_param (tok);
      /* ExecuteIfIs (V4Game.cs:7450-7578) splits the parameter into *fields* on
       * the first ';' and, if there is one, the second: with three fields the
       * middle one is the operator and everything after it is the right-hand
       * side; with two, the operator is "=".  The operator is then matched
       * whole, and anything but =, !=, gt, lt, gt= or lt= is an error.
       *
       * geas used to hunt for the operator as a *substring* of the whole
       * parameter instead, which quietly misread any operand with those two
       * letters in it just before a separator: `is <#quest.thing#;bolt>` found
       * the "lt;" inside "bolt;" and compared "bo" with "bolt" numerically,
       * i.e. 0 < 0.  (preprocess () rewrites "if (a <= b)" into the canonical
       * three-field "is <a;lt=;b>", so that form arrives here too.) */
      std::string::size_type scp = tok.find (';');
      if (scp == string::npos)
	{
	  gi->debug_print ("Expected second parameter in 'is " + tok + "'");
	  return false;
	}
      std::string::size_type scp2 = tok.find (';', scp + 1);
      string op = "=", val1 = trim_braces (trim (tok.substr (0, scp))), val2;
      if (scp2 == string::npos)
	val2 = trim_braces (trim (tok.substr (scp + 1)));
      else
	{
	  op = trim (tok.substr (scp + 1, scp2 - scp - 1));
	  val2 = trim_braces (trim (tok.substr (scp2 + 1)));
	}
      val1 = eval_is_operand (val1);
      val2 = eval_is_operand (val2);
      GEAS_DBG << "Comparing <" << val1 << "> " << op << " <" << val2 << ">\n";
      if (op == "=")
	return ci_equal (val1, val2);
      if (op == "!=")
	return ci_notequal (val1, val2);
      /* Quest compares these with Val(), which reads the leading number and
       * calls the rest zero -- as eval_double does. */
      if (op == "gt")
	return eval_double (val1) > eval_double (val2);
      if (op == "lt")
	return eval_double (val1) < eval_double (val2);
      if (op == "gt=")
	return eval_double (val1) >= eval_double (val2);
      if (op == "lt=")
	return eval_double (val1) <= eval_double (val2);
      gi->debug_print ("Unrecognised comparison condition in 'is " + tok + "'");
      return false;
    }
  else if (tok == "property")
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("expected parameter after property in " + s);
	  return false;
	}
      tok = eval_param (tok);
      std::string::size_type index = tok.find (';');
      if (index == string::npos)
	{
	  gi->debug_print ("Only one argument to property in " + s);
	  return false;
	}
      string obj = trim (tok.substr (0, index));
      string prop = trim (tok.substr (index+1));
      return has_obj_property (obj, prop);
    }
  else if (tok == "real")
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("expected parameter after real in " + s);
	  return false;
	}
      vector<string> args = split_param (eval_param (tok));
      bool do_report = false;
      for (uint i = 1; i < args.size(); i ++)
	if (ci_equal (trim (args[i]), "report"))
	  do_report = true;
	else
	  gi->debug_print ("Got modifier " + args[i] + " after real");
      if (state.obj_records (args[0]) != NULL)
	return true;
      if (do_report)
	gi->debug_print ("real " + args[0] + " failed due to nonexistence");
      return false;
    }
  else if (tok == "type")
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected parameter after type in " + s);
	  return false;
	}
      vector<string> args = split_param (eval_param(tok));
      if (args.size() != 2)
	{
	  gi->debug_print ("Expected two parameters to type in " + s);
	  return false;
	}
      /* A membership added at runtime by the `type` statement is a
       * "!type <name>" property -- ExecType appends to TypesIncluded, which
       * is the list ExecuteIfType reads. */
      return gf.obj_of_type (args[0], args[1]) ||
	     has_obj_property (args[0], "!type " + lcase (trim (args[1])));
    }

  gi->debug_print ("Bad condition " + s);
  return false;
}
