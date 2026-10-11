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


/* question-verbs.cc -- The built-in verb handlers: what a command means when
 * no command or verb of the game's own has claimed it.  try_match in
 * question-parse.cc tries them in order; each returns true once the command
 * is dealt with, whether or not it succeeded, and false to pass it on.
 *
 * Part of question_implementation; question-internal.hh holds what these
 * units share. */

#include "question-internal.hh"

using namespace std;

/* Take off / unwear a worn item.  Game-defined commands ran first (above), so
 * a game that scripts its own removal -- e.g. KQ5 refuses to take off the
 * cloak -- still wins; this is the fallback for clothing whose only handler is
 * a type's action <unwear> (which prints the message and resets the alias but,
 * like Quest's library, leaves clearing the worn flag to the command layer we
 * don't bundle).  Placed before the wear/verb table so "take off X" isn't read
 * as the verb "take". */
bool question_implementation::try_unwear (const string &cmd, bool is_internal)
{
  match_rv match;

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

  return false;
}

/* ---- Generic verb dispatch ---------------------------------------------
 * Quest games attach most verbs to objects through `action <verb>`,
 * `properties <verb=text>`, or (for opening things) the object's anonymous
 * default action.  Question previously hard-coded only look/examine/take/use/
 * give/drop/etc., so common verbs such as open/move/eat/drink/smell fell
 * through to "I don't understand your command".  This table maps the surface
 * phrases a player may type to the canonical key stored in the game file
 * (builtin_verbs, shared with the verb menu).
 * It is tried after explicit `command` definitions but before the bare
 * "look #@object#" handler below, so multi-word verbs like "look under" are
 * matched before "look" would swallow them. */
bool question_implementation::try_builtin_verb (const string &cmd, bool is_internal)
{
  match_rv match;

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
	     * DoAddRemove, from 4.10).  Question marked it seen before it even knew
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
		    /* Not one of Quest's branches: Question resolves a `define
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
		 * is no look-inside listing anywhere in the engine.  Question
		 * had one, in wording of its own ("Inside you see a and
		 * b.", "It is closed.", "It is empty."), and printed no
		 * description at all.
		 *
		 * "search" is Question's own third spelling and stays on this
		 * handler; the two corpus games that give it a meaning
		 * write `action <search>`, which the lookup above answers
		 * before ever reaching here. */
		do_look (obj);
	      }
	    else if (key == "wear")
	      {
		/* Quest's built-in clothing: wearing sets the "worn" property
		 * (games test `property <obj; worn>`) and holds the item.  Question
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
	     * Question ran above -- sets the "worn" property and holds the item, as in
	     * Quest's clothing library.  The library's own command layer (which set
	     * "worn") isn't bundled, so the type action alone wouldn't, yet games
	     * test `property <obj; worn>` (e.g. KQ5's cloak/amulet). */
	    if (key == "wear")
	      { set_obj_property (obj, "worn"); move (obj, "inventory"); }
	    return true;
	  }
  }

  return false;
}

/* "l <thing>" is "look <thing>" -- ExecCommand rewrites it before calling
 * ExecLook, and takes a bare "l" for "look" (V4Game.Part2.cs:4426-4429,
 * 4452-4455).  Realm of Chaos depends on it: its `kill' command execs
 * "l #enemy#" to run the monster's look script, which sets its stats. */
bool question_implementation::try_look (const string &cmd, bool is_internal)
{
  match_rv match;

  if ((match = match_command (cmd, "look at #@object#")) ||
      (match = match_command (cmd, "look #@object#")) ||
      (match = match_command (cmd, "l at #@object#")) ||
      (match = match_command (cmd, "l #@object#")))
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

  if ((match = match_command (cmd, "look")) || (match = match_command (cmd, "l")))
    {
      look();
      return true;
    }

  return false;
}

/* GIVE has two forms, and ExecGive tries them in this order: split on the
 * first " to " and, only if the input has none, on the first " the " -- with
 * the recipient in front of it and the item after (V4Game.Part2.cs:4618-4643).
 * "give the gatekeeper the document of approval" is the phrasing QDK's own
 * hint text hands to players, and in Tai & David's Amazing Maze
 * (TDAMAZINGMAZE.cas) it is the only way to hand over the document the gate
 * is locked behind.  Question knew the "X to Y" form only.
 *
 * The recursive matcher grows a binding from the shortest text up, so the
 * split it finds is the leftmost one -- the same place InStr would put it. */
bool question_implementation::try_give (const string &cmd, bool is_internal)
{
  match_rv match;

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
	  if (pronoun_refused_)
	    return true;
	  display_error ("noitem", item_b[0].var_text);
	  return true;
	}
      if (!dereference_vars (char_b, room_only, is_internal, true))
	{
	  if (pronoun_refused_)
	    return true;
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

  return false;
}

/* Quest's standard "remove" verb, e.g. "remove book of keys from odd book
 * shelf": from 3.91 it is the container half of "put", handled by the
 * *container*, not by the item (ExecAddRemove, V4Game.cs:2369-2624).  Without
 * a "from", the container is whatever the item's parent property says.
 *
 * Question keeps its own, older reading -- run the named object's own "remove"
 * action/property -- as a fallback, because that is the only way several
 * games in the corpus can be played at all: Shiversword Tales and Bear
 * Campsite hang `action <remove>` on the clothes and the tangled string the
 * player is told to remove, which real Quest would answer with "You can't
 * remove that." since neither is in a container. */
bool question_implementation::try_remove (const string &cmd, bool is_internal)
{
  match_rv match;

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

  return false;
}

bool question_implementation::try_use (const string &cmd, bool is_internal)
{
  match_rv match;

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
	  if (pronoun_refused_)
	    return true;
	  display_error ("noitem", item_b[0].var_text);
	  return true;
	}
      string script, first = item_b[0].var_text, second;
      {
	vector<string> room_only (1, state.location);
	second = get_obj_name (target_b[0].var_text, room_only, is_internal);
	string tlc = lcase (trim (target_b[0].var_text));
	if (tlc == "it" || tlc == "them" || tlc == "this" || tlc == "those" ||
	    tlc == "these" || tlc == "that" || tlc == "him" || tlc == "her")
	  {
	    /* A pronoun is resolved over both scopes at once. */
	    if (!dereference_vars (target_b, is_internal))
	      return true;
	    second = target_b[0].var_text;
	  }
	else if (second == "!")
	  {
	    if (!dereference_vars (target_b, inv_only, is_internal, true))
	      {
		if (pronoun_refused_)
		  return true;
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
       * (V4Game.Part2.cs:5439-5460).  Question used to call it "quest.use.object",
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
	  if (pronoun_refused_)
	    return true;
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

  return false;
}

/* Quest's standard "put #object# in/on #object#" (place into a container or
 * onto a surface): the target's add machinery decides what happens, and if
 * the item is actually moved into it (its parent becomes the target), the
 * closed-container scope rules then apply.  Games with richer logic define
 * their own `command <put ...>`, tried before this. */
bool question_implementation::try_put (const string &cmd, bool is_internal)
{
  match_rv match;

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

  return false;
}

bool question_implementation::try_take (const string &cmd, bool is_internal)
{
  match_rv match;
  string tok;

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
	  if (pronoun_refused_)
	    return true;
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
       * trenchcoat and overrides alreadytaken; Question used to run a whole take
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
	  QUESTION_DBG << "Running script '" << tok << "' for take " << object << endl;
	  run_script_as (object, tok);
	}
      else if (get_obj_property (object, "take", tok))
	{
	  QUESTION_DBG << "Found property '" << tok << "' for take " << object << endl;
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
	       * (ibid. 4614): Question prints a turn's blank line at the top of
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
	  move (object, "inventory");
	  /* A taken object is now in hand, so it's no longer hidden (it may have
	   * been a hidden item inside a container). */
	  set_obj_property (object, "not hidden");
	  /* Quest's ExecTake ends a text/default take with PlayerItem(item, true),
	   * which runs the object's GainScript (V4Game.Part2.cs:5205-5215,
	   * 6631-6634).  That is run_script_as inside dispatch_obj_verb, not
	   * run_script: the two-argument run_script is (script, &return value),
	   * so passing the object name as the first argument ran the *name* as
	   * a script and quietly dropped the gain script into the return
	   * value.  Enterprising in space hangs its entire Youhera chain off
	   * three bare `take` + `gain` objects and was unwinnable. */
	  dispatch_obj_verb (object, "gain");
	}
      else
	{
	  QUESTION_DBG << "No match found for take " << object << endl;
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

  return false;
}

bool question_implementation::try_drop (const string &cmd, bool is_internal)
{
  match_rv match;

  if ((match = match_command (cmd, "drop #@object#")))
    {
      /* The inventory alone: ExecDrop's one Disambiguate is scoped to it, and a
       * miss is NoItem from 3.91 and BadDrop below (V4Game.cs:5531-5556).  So a
       * noun shared by a carried object and one already on the floor drops the
       * carried one rather than asking which. */
      vector<string> inv_only (1, "inventory");
      if (!dereference_vars (match.bindings, inv_only, is_internal, true))
	{
	  if (pronoun_refused_)
	    return true;
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

      const QuestionBlock *gb = gf.find_by_name ("object", obj);
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

  return false;
}

bool question_implementation::try_speak (const string &cmd, bool is_internal)
{
  match_rv match;

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

  return false;
}

bool question_implementation::try_out (const string &cmd, bool)
{
  if (cmd == "exit" || cmd == "out" || cmd == "go out")
    {
      /* From 4.10 a movement command forgets what "it" was.  Quest clears
       * its referent for every version (V4Game.Part2.cs:4282-4388), but
       * below 4.10 the end of the turn puts it straight back (ibid.
       * 4624-4629); measured against the real runner at 2.80 to 4.10. */
      if (asl_version_ >= 410)
	last_object = "";
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
      const QuestionBlock *gb = gf.find_by_name ("room", state.location);
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

  return false;
}

bool question_implementation::try_direction (const string &cmd, bool)
{
  string tok;

  for (size_t i = 0; i < ARRAYSIZE(dir_names); i ++)
    if (cmd == dir_names[i] || cmd == "go " + dir_names[i] ||
	cmd == short_dir_names[i] || cmd == "go " + short_dir_names[i])
      {
	bool is_script = false;
	if (asl_version_ >= 410)
	  last_object = "";   /* as for "out" above */
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

  return false;
}

bool question_implementation::try_goto (const string &cmd, bool)
{
  match_rv match;

  if ((match = match_command (cmd, "go to #@room#")) ||
      (match = match_command (cmd, "go #@room#")))
    {
      if (match.bindings.size() != 1) { report_unsupported ("unexpected binding count for 'go to' command"); return true; }
      if (asl_version_ >= 410)
	last_object = "";   /* as for "out" above */
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

  return false;
}

bool question_implementation::try_inventory (const string &cmd, bool)
{
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

  return false;
}

/* Quest's built-in "wait" -- pass a turn.  Games that want a custom message
 * define `command <wait>` (tried earlier); this is the engine default. */
bool question_implementation::try_meta (const string &cmd, bool is_internal)
{
  match_rv match;

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
      const QuestionBlock *gb = gf.find_by_name ("game", "game");
      if (gb == NULL)
	return true;
      QUESTION_DBG << *gb << endl;
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
   * frontend -- which is why a typed SAVE never gets this far: questionglk
   * intercepts it (handle_saverestore_command) and so does the
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
