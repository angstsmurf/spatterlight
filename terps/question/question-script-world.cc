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


/* question-script-world.cc -- the statement handlers run_script dispatches to for
 * the game world: creating, moving, hiding and destroying objects, rooms and
 * exits, and their properties, actions and types.
 *
 * Each takes the statement line (s), the enclosing function's result (rv),
 * the lower-cased keyword (tok) and the keyword's token bounds in s (c1, c2),
 * and returns true once the line is dealt with -- see run_script in
 * question-script.cc. */

#include "question-internal.hh"

using namespace std;

/* action */
bool question_implementation::exec_action (const string &s, string &,
					   string tok, std::string::size_type c1,
					   std::string::size_type c2)
{
  if (!next_param (s, c1, c2, tok, "Expected parameter after action in "))
    return true;
  tok = eval_param (tok);
  std::string::size_type index = tok.find (';');
  if (index == string::npos)
    {
      gi->debug_print ("Error: no semicolon in " + s);
      return true;
    }
  set_obj_action (trim (tok.substr (0, index)),
		  "<" + trim (tok.substr (index+1)) + "> "
		  + (c2 < s.length() ? s.substr (c2 + 1) : ""));
  return true;
}

/* type */
bool question_implementation::exec_type (const string &s, string &,
					 string tok, std::string::size_type c1,
					 std::string::size_type c2)
{
  /* Runtime `type <obj; typename>` (ExecType, V4Game.cs:4425-4476):
   * flatten the type's properties and actions onto the object now, and
   * remember the membership for `if type`.  The marker is a
   * "!type <name>" property, following the "!exitlock"/"!clones"
   * convention, so it rides through save/undo with the rest of the
   * state. */
  if (!next_param (s, c1, c2, tok, "Expected parameter after type in "))
    return true;
  vector<string> args = split_param (eval_param (tok));
  if (args.size () != 2 || trim (args[0]) == "" || trim (args[1]) == "")
    {
      gi->debug_print ("Expected <object; type> in " + s);
      return true;
    }
  string obj = trim (args[0]);
  if (state.obj_records (obj) == NULL)
    {
      gi->debug_print ("No such object in " + s);
      return true;
    }
  apply_type (obj, trim (args[1]));
  return true;
}

/* Quest's container statements, all ASL >= 3.91: "add <child; parent>" puts an
 * object into a container, "remove <child>" takes it back out, and
 * "open <container>" / "close <container>" set the open state silently -- no
 * message, no action or property lookup and no container check at all, unlike
 * the open/close *verbs* (ExecuteScript -> SetOpenClose -> DoOpenClose,
 * V4Game.cs:2240, and -> ExecAddRemoveScript -> DoAddRemove, V4Game.cs:2113).
 * Below 3.91 Quest has no such statements, so the line falls through to the
 * unrecognised-script path exactly as it would there. */
bool question_implementation::exec_add_remove (const string &s, string &,
					       string tok, std::string::size_type c1,
					       std::string::size_type c2)
{
  if (asl_version_ < 391)
    return false;   /* no such statement below 3.91: unrecognised */
  bool adding = (tok == "add");
  if (!next_param (s, c1, c2, tok,
		   adding ? "Expected parameter after add in "
			  : "Expected parameter after remove in "))
    return true;
  string arg = eval_param (tok);
  std::string::size_type index = arg.find (';');
  string child = trim (index == string::npos ? arg : arg.substr (0, index));
  if (state.obj_records (child) == NULL)
    {
      gi->debug_print ("Invalid child object name specified in " + s);
      return true;
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
	  return true;
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
	  return true;
	}
      do_add_remove (child, parent, adding);
    }
  /* No sweep here: do_add_remove has already re-derived the one container
   * Quest names (V4Game.cs:2133, 2698). */
  gi->update_sidebars ();
  return true;
}

/* open, close */
bool question_implementation::exec_open_close (const string &s, string &,
					       string tok, std::string::size_type c1,
					       std::string::size_type c2)
{
  if (asl_version_ < 391)
    return false;   /* no such statement below 3.91: unrecognised */
  bool opening = (tok == "open");
  if (!next_param (s, c1, c2, tok,
		   opening ? "Expected parameter after open in "
			   : "Expected parameter after close in "))
    return true;
  string name = trim (eval_param (tok));
  if (state.obj_records (name) == NULL)
    gi->debug_print ("Invalid object name specified in " + s);
  else
    set_obj_property (name, opening ? "opened" : "not opened");
  return true;
}

/* lock, unlock */
bool question_implementation::exec_lock_unlock (const string &s, string &,
						string tok, std::string::size_type c1,
						std::string::size_type c2)
{
  /* Quest "lock <room; dir>" / "unlock <room; dir>": toggle an exit's
   * locked state.  Recorded as a property on the synthetic "!exitlock"
   * object so it persists with state/undo; exit_locked() consults it. */
  bool locking = (tok == "lock");
  if (!next_param (s, c1, c2, tok,
		   locking ? "Expected <room; direction> after lock in "
			   : "Expected <room; direction> after unlock in "))
    return true;
  vector<string> p = split_param (eval_param (tok));
  if (p.size () < 2 || trim (p[0]) == "" || trim (p[1]) == "")
    {
      gi->debug_print ("Malformed exit in " + s);
      return true;
    }
  string key = lcase (trim (p[0])) + ";" + lcase (trim (p[1]));
  set_obj_property ("!exitlock", key + (locking ? "=locked" : "=open"));
  return true;
}

/* clone */
bool question_implementation::exec_clone (const string &s, string &,
					  string tok, std::string::size_type c1,
					  std::string::size_type c2)
{
  /* clone <src; newname[; room]> -- ExecClone, V4Game.cs:4352-4400.
   *
   * Quest appends a byte copy of the source's record to _objs, overwrites
   * its ObjectName with the new name and its ContainerRoom with the third
   * field (or, when there is none, with wherever the source itself is), and
   * calls UpdateObjectList.  Here the definition half of that copy is an
   * alias (QuestionFile::register_clone) and the mutable half is a new
   * ObjectRecord plus copies of whatever properties and actions the source
   * has picked up at runtime, which together are what Quest's copy carries.
   *
   * The corpus case is Barbarian (Wonderjudge, ASL 410): every purchase in
   * Turner's market is `clone <Tomato; Tomato%c%; Market>` followed by a
   * `give`, so with this a no-op the player paid and got nothing -- and the
   * cabbage that tames the horse is the first link of task 1. */
  if (!next_param (s, c1, c2, tok, "Expected param after clone in "))
    return true;
  vector<string> args = split_param (eval_param (tok));
  if (args.size() < 2)
    {
      gi->debug_print ("No new object name specified in " + s);
      return true;
    }
  string src = trim (args[0]), newname = trim (args[1]);
  const vector<size_t> *v = state.obj_records (src);
  if (v == NULL || v->empty())
    {
      gi->debug_print ("Tried to clone nonexistent object '" + src + "'");
      return true;
    }
  if (newname == "")
    {
      gi->debug_print ("No new object name specified in " + s);
      return true;
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
  /* Copied first: add_prop appends to the very list being walked when
   * the new name differs from the source only in case. */
  if (const vector<size_t> *pv = state.prop_records (src))
    for (size_t idx: vector<size_t> (*pv))
      state.add_prop (newname, state.props[idx].data);
  /* So a clone survives a save/restore: load_state replays these to
   * re-register the definition aliases, which live in the QuestionFile and are
   * therefore not part of the serialized state. */
  state.add_prop ("!clones", "properties " + lcase (newname) + "=" + src);

  update_container_visibility (dest);
  gi->update_sidebars();
  objs_vars_dirty_ = true;
  return true;
}

/* create */
bool question_implementation::exec_create (const string &s, string &,
					   string tok, std::string::size_type c1,
					   std::string::size_type c2)
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
	  return true;
	}
      tok = eval_param (tok);
      vector<string> args = split_param (tok);
      if (args.size() != 2)
	{
	  gi->debug_print ("Expected 2 elements in param in " + s);
	  return true;
	}
      /* Quest never lists the same "go to" exit twice.  Below ASL 4.10
       * ExecuteCreateExit looks the destination up in the room's Places list
       * first and, if it is already there, logs "Exit from 'X' to 'Y'
       * already exists" and does nothing at all (V4Game.cs:5385-5409).  From
       * 4.10 on the places live in a dictionary keyed by destination room, so
       * AddPlaceExit deletes the old entry before inserting the new one
       * (RoomExits.cs:46-56): the exit is replaced rather than duplicated,
       * and moves to the end of the listing.  Either way one destination
       * means one entry.  Question simply appended, so DJay32's "Metroid" Lite
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
		  return true;
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
      return true;
    }
  else if (ci_equal (tok, "object")) // create object
    {
      /* ExecuteCreate's object half (V4Game.cs:5278-5325): a fresh object
       * named by the first field, placed in the room named by the second
       * -- or nowhere at all when there is none.  Alias, gender and
       * article take the same defaults Question already falls back on, and
       * from 4.10 the `default` type's properties and actions are
       * flattened onto it exactly as a declared object gets them. */
      if (!next_param (s, c1, c2, tok, "Expected param after create object in "))
	return true;
      vector<string> args = split_param (eval_param (tok));
      string name = trim (args[0]);
      if (name == "")
	{
	  gi->debug_print ("No object name given in " + s);
	  return true;
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
      if (!next_param (s, c1, c2, tok, "Expected param after create room in "))
	return true;
      set_obj_property ("!createdroom", lcase (trim (eval_param (tok))));
    }
  else
    gi->debug_print ("Bad create line " + s);
  return true;
}

/* destroy */
bool question_implementation::exec_destroy (const string &s, string &,
					    string tok, std::string::size_type c1,
					    std::string::size_type c2)
{
  tok = next_token (s, c1, c2);
  /* CI: BeginsWith "destroy exit " (V4Game.Part2.cs). */
  if (!ci_equal (tok, "exit"))
    {
      gi->debug_print ("expected 'exit' after 'destroy' in " + s);
      return true;
    }
  if (!next_param (s, c1, c2, tok, "Expected param after 'destroy exit' in "))
    return true;
  string tok2 = eval_param (tok);
  vector<string> args = split_param (tok2);
  if (args.size() != 2)
    {
      gi->debug_print ("Expected two arguments in " + s);
      return true;
    }
  state.exits.push_back (ExitRecord (args[0], "destroy exit " + args[1]));
  regen_var_dirs();
  return true;
}

/* disconnect */
bool question_implementation::exec_disconnect (const string &s, string &,
					       string tok, std::string::size_type c1,
					       std::string::size_type c2)
{
  /* disconnect <room; direction> -- remove that exit (the inverse of
   * create exit <direction> <room; dest>).  Recorded as a "noexit" so it
   * overrides the static room exit and any earlier dynamic create-exit. */
  if (!next_param (s, c1, c2, tok, "Expected param after disconnect in "))
    return true;
  vector<string> args = split_param (eval_param (tok));
  if (args.size () != 2)
    {
      gi->debug_print ("Expected <room; direction> after disconnect in " + s);
      return true;
    }
  /* Folded like create exit's direction, so the record matches the
   * engine's lowercase direction names when read back. */
  state.exits.push_back (ExitRecord (args[0], "noexit " + lcase (trim (args[1]))));
  regen_var_dirs ();
  return true;
}

/* give */
bool question_implementation::exec_give (const string &s, string &,
					 string tok, std::string::size_type c1,
					 std::string::size_type c2)
{
  /* Quest takes the first <...> on the line whatever sits before it
   * (GetParameter, V4Game.Part2.cs:5796-5799), so QDK's long-hand `give
   * object <pretty hairdo>' in On Time's elf menu still hands it over. */
  tok = next_param_token (s, c1, c2);
  if (tok == "")
    {
      gi->debug_print ("Expected parameter after give in " + s);
      return true;
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
   * `define object <Journal >', but Question stores block names trimmed. */
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
      dispatch_obj_verb (tok, "gain");
    }
  gi->update_sidebars();
  return true;
}

/* goto */
bool question_implementation::exec_goto (const string &s, string &,
					 string tok, std::string::size_type c1,
					 std::string::size_type c2)
{
  if (next_param (s, c1, c2, tok, "Expected parameter after goto in "))
    goto_room (trim (eval_param (tok)));
  return true;
}

/* "hideobject"/"hidechar" are the Quest 2.x spellings of "hide". */
bool question_implementation::exec_hide (const string &s, string &,
					 string tok, std::string::size_type c1,
					 std::string::size_type c2)
{
  if (next_param (s, c1, c2, tok, "Expected param after hide in "))
    {
      string name = eval_param (tok);
      if (!resolve_at_room (name))
	return true;
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
  return true;
}

/* "showobject"/"showchar" are the Quest 2.x spellings of "show". */
bool question_implementation::exec_show (const string &s, string &,
					 string tok, std::string::size_type c1,
					 std::string::size_type c2)
{
  /* Below ASL 2.82 `show <...>' is the *picture* statement: the picture
   * branch of the dispatcher sits above the object one and claims the word
   * (V4Game.Part2.cs:5842 against 5904), so showing an object by that
   * spelling effectively begins at 2.82 even though its own gate reads
   * >= 2.81.  A game older than that says `showobject' or `showchar'. */
  if (tok == "show" && asl_version_ < 282)
    {
      std::string::size_type lt = s.find ('<', c2);
      if (lt != string::npos)
	c2 = lt;
      tok = next_token (s, c1, c2);
      if (is_param (tok))
	show_picture (eval_param (tok), true);
      return true;
    }
  if (next_param (s, c1, c2, tok, "Expected param after show in "))
    {
      string name = eval_param (tok);
      if (!resolve_at_room (name))
	return true;
      set_obj_property (name, "not hidden");
    }
  return true;
}

/* lose */
bool question_implementation::exec_lose (const string &s, string &,
					 string tok, std::string::size_type c1,
					 std::string::size_type c2)
{
  if (!next_param (s, c1, c2, tok, "Expected parameter after lose in "))
    return true;
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
   * am standing", not "drop it if I am holding it".  exec_give, its mirror
   * image, has never had such a test either. */
  bool is_object = (asl_version_ >= 280 && state.obj_records (tok) != NULL);
  if (is_object)
    {
      move (tok, state.location);
      dispatch_obj_verb (tok, "lose");
    }
  gi->update_sidebars();
  return true;
}

/* "movechar"/"moveobject" are the Quest 2.x spellings of "move". */
bool question_implementation::exec_move (const string &s, string &,
					 string tok, std::string::size_type c1,
					 std::string::size_type c2)
{
  if (!next_param (s, c1, c2, tok, "Expected parameter after move in "))
    return true;
  tok = eval_param (tok);
  std::string::size_type index = tok.find (';');
  if (index == string::npos)
    {
      gi->debug_print ("No semi in " + tok + " in " + s);
      return true;
    }
  move (trim (tok.substr (0, index)), trim (tok.substr (index + 1)));
  return true;
}

/* property */
bool question_implementation::exec_property (const string &s, string &,
					     string tok, std::string::size_type c1,
					     std::string::size_type c2)
{
  tok = next_token (s, c1, c2);
  if (!is_param(tok))
    {
      gi->debug_print ("Expected parameter in '" + s + "'");
      return true;
    }
  vector<string> args = split_param (eval_param (tok));
  for (size_t i = 1; i < args.size(); i ++)
    {
      string val = args[i];
      val = trim_braces(val);
      set_obj_property (args[0], val);
    }
  return true;
}

/* "reveal"/"conceal" (ASL>=281) and their "…object"/"…char" spellings
 * (ASL<281) toggle Quest's *visibility* flag (SetVisibility -> .Visible in
 * LegacyGame.vb), which gates only the room/inventory listing.  This is a
 * different axis from show/hide (SetAvailability -> .Exists), which gates
 * verb scope: DisambObjHere resolves a typed noun on .Exists alone and never
 * consults .Visible, so a concealed-but-existing object stays referenceable.
 * Hence these must map to the "invisible" property, NOT "hidden". */
bool question_implementation::exec_reveal_conceal (const string &s, string &,
						   string tok, std::string::size_type c1,
						   std::string::size_type c2)
{
  bool reveal = (tok.compare (0, 6, "reveal") == 0);
  if (next_param (s, c1, c2, tok,
		  reveal ? "Expected param after reveal in "
			 : "Expected param after conceal in "))
    set_obj_property (eval_param (tok), reveal ? "not invisible" : "invisible");
  return true;
}
