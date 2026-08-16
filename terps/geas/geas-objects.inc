// geas-objects.inc -- Objects: action and property lookups (runtime state over
// file defaults), the verb menus, moving objects, and containers --
// open/close, contents listing, reachability and visibility.
//
// Split out of geas-runner.cc, which #includes it (same single-TU layout
// as the quest5/aslx-*.inc units); the code is unchanged from the split.

bool geas_implementation::has_obj_action (const string &obj, const string &prop) const
{
  string tmp;
  return get_obj_action (obj, prop, tmp);
}

/* The runtime (state.props) half of an action lookup, shared by the object and
   room forms below: true if a "action <name> ..." record set at runtime answers
   the question, with the script in rv. */
bool geas_implementation::runtime_action (const string &objname, const string &actname,
					  string &rv) const
{
  string tok;
  std::string::size_type c1, c2;
  const vector<size_t> *recs = state.prop_records (objname);
  if (recs)
    /* Most-recently-set action wins: walk this object's records newest first. */
    for (auto ri = recs->rbegin (); ri != recs->rend (); ++ri)
      {
	const string &line = state.props[*ri].data;
	/* Internal record format -- geas itself writes "action <...>" (see
	 * set_obj_action), so the exact compare is right. */
	if (first_token (line, c1, c2) != "action")
	  continue;
	tok = next_token (line, c1, c2);
	/* Skip unless this runtime-set action's name matches the one requested.
	 * (The condition was inverted, returning a non-matching action.) */
	if (!is_param(tok) || !ci_equal (param_contents(tok), actname))
	  continue;
	rv = trim (line.substr (c2));
	GEAS_DBG << "  g_o_a: returning true, \"" << rv << "\".";
	return true;
      }
  return false;
}

bool geas_implementation::get_obj_action (const string &objname, const string &actname,
					  string &rv) const
{
  GEAS_DBG << "get_obj_action (" << objname << ", " << actname << ")\n";
  if (runtime_action (objname, actname, rv))
    return true;
  /* Prefer the block defined in the current room, so same-named objects in
   * different rooms keep their own actions. */
  return gf.get_obj_action (objname, actname, rv, state.location);
}

/* As get_obj_action, but for a name that is known to be a room -- the room
   script, its description action, its "use X" handler.  A room and an object
   may share a name (see GeasFile::register_block), and an unqualified lookup
   answers for the object. */
bool geas_implementation::get_room_action (const string &roomname, const string &actname,
					   string &rv) const
{
  GEAS_DBG << "get_room_action (" << roomname << ", " << actname << ")\n";
  if (runtime_action (roomname, actname, rv))
    return true;
  return gf.get_room_action (roomname, actname, rv);
}

/* Quest's FindLine over the game block (V4Game.Part2.cs:6191-6219): the first
   line whose first token is `tok` and whose first parameter matches `param`
   case-insensitively, with everything after that parameter handed back as the
   script.  Game-block lines are not rewritten into "action <...>" form by
   readfile, so they are invisible to get_obj_action and have to be walked here.
   Sub-define blocks inside the game block are skipped, as FindLine skips them. */
bool geas_implementation::find_game_block_line (const string &tok, const string &param,
						string &rv) const
{
  const GeasBlock *game = gf.find_by_name ("game", "game");
  if (game == NULL)
    return false;
  std::string::size_type c1, c2;
  int depth = 0;
  for (const string &line: game->data)
    {
      string first = first_token (line, c1, c2);
      if (first == "define")
	{ ++depth; continue; }
      if (first == "end")
	{ if (depth > 0) --depth; continue; }
      if (depth > 0 || !ci_equal (first, tok))
	continue;
      string ptok = next_token (line, c1, c2);
      if (!is_param (ptok) || !ci_equal (trim (param_contents (ptok)), param))
	continue;
      rv = trim (line.substr (c2));
      return true;
    }
  return false;
}

bool geas_implementation::has_obj_property (const string &obj, const string &prop) const
{
  string tmp;
  return get_obj_property (obj, prop, tmp);
}

bool geas_implementation::dispatch_obj_verb (const string &obj, const string &key)
{
  string script;
  if (get_obj_action (obj, key, script))
    {
      run_script_as (obj, script);
      return true;
    }
  if (get_obj_property (obj, key, script))
    {
      print_formatted (script);
      return true;
    }
  return false;
}

v2string geas_implementation::object_verbs (const string &obj) const
{
  /* Each entry is {menu label, command, "1" when the command is only a prefix
     the player still has to finish}; see GeasRunner::get_object_verbs. */
  v2string out;
  /* The name a typed command uses for this object -- the same printed alias
     the pane lists it under (see get_room_contents). */
  string alias;
  if (!get_obj_property (obj, "alias", alias))
    alias = obj;
  /* Capitalise the first letter so a stored key ("destroy", "listen to")
     reads as a menu label ("Destroy", "Listen to"), matching Quest.

     A few action keys are spellings of a verb the menu already lists under
     another name: the parser dispatches "look" and "look at" to the same
     `look` action, "speak"/"talk"/"talk to" to the same `speak` action, and
     "x" to `examine` (see run_commands).  An object that spells its handler
     the short way ("look msg <...>", which the loader stores as
     `action <look>`) would otherwise be listed twice -- The Mansion's dining
     room painting showed both "Look at" and "Look", "Speak to" and "Speak".
     Canonicalise to the label the menu already uses so add()'s duplicate
     check catches them. */
  auto label_of = [&] (const string &s) {
    string t = trim (s);
    if (ci_equal (t, "look"))                            return string ("Look at");
    if (ci_equal (t, "x"))                               return string ("Examine");
    if (ci_equal (t, "speak") || ci_equal (t, "talk") ||
	ci_equal (t, "talk to"))                         return string ("Speak to");
    /* The take/drop pair is one menu entry, whichever way it is spelled:
       a carried object offers Drop, one on the floor offers Take. */
    if (ci_equal (t, "take") || ci_equal (t, "get"))
      return string (is_held (obj) ? "Drop" : "Take");
    if (!t.empty ()) t[0] = std::toupper ((unsigned char) t[0]);
    return t;
  };
  /* Add one menu entry.  The command defaults to what the player would type
     to apply this verb to this object ("Look at" -> "Look at red herring");
     a verb needing a second noun passes its own prefix and more = true. */
  auto add = [&] (const string &label, const string &command = "",
		  bool more = false) {
    string t = trim (label);
    if (t == "")
      return;
    for (const vector<string> &e: out)
      if (!e.empty () && ci_equal (e[0], t))
	return;
    out.push_back ({t, command != "" ? command : t + " " + alias,
		    more ? "1" : ""});
  };
  /* Does the object define its own handler (action or property) for this key? */
  auto responds = [&] (const string &key) {
    string s;
    return get_obj_action (obj, key, s) || get_obj_property (obj, key, s);
  };

  /* Universal verbs the engine always handles for any object in scope. */
  add ("Look at");
  add ("Examine");
  add (is_held (obj) ? "Drop" : "Take");
  if (responds ("speak")) add ("Speak to");
  if (responds ("use"))   add ("Use");
  if (responds ("read"))  add ("Read");
  /* "remove <object>" is a command in its own right, and when the object is not
     inside a container it falls back on the object's own `remove` action or
     property (see the "remove #@object#" handler) -- which is how Shiversword
     Tales' clothes and Bear Campsite's tangled string come off.  It used to
     reach the menu only as a side-effect of harvesting the action list below.

     Two other things answer a bare responds("remove") and are not that.  A
     container's `remove` is the container half of "put", the handler for taking
     something back *out* of it, and says nothing about removing the container
     itself: "City of Blood" tags seven of its containers with a valueless
     `properties <remove>` and not one of them can be removed.  And a valueless
     property prints nothing at all, since dispatch_obj_verb formats the empty
     string, so it is no response either. */
  string rem;
  if ((get_obj_action (obj, "remove", rem) ||
       (get_obj_property (obj, "remove", rem) && trim (rem) != "")) &&
      !has_obj_property (obj, "container") && !has_obj_property (obj, "surface"))
    add ("Remove");

  /* Built-in multi-synonym verbs (the same table try_match dispatches through):
     list one when the object defines a matching action/property.  The container
     verbs also apply to any container/surface, as the engine handles those
     itself.  label_of turns the stored key into the menu label ("listen to" ->
     "Listen to"). */
  for (const verb_def &v: builtin_verbs ())
    if (responds (v.key))
      add (label_of (v.key));
  if (has_obj_property (obj, "container") || has_obj_property (obj, "surface"))
    { add ("Open"); add ("Close"); add ("Look in"); }

  /* Returns true when a verb phrase names a specific second object, e.g.
     "break with fire-axe", "use fire ax" or "give to guard".  Such entries
     are spoilery and omitted; the plain forms with no noun ("Use", "Give
     to...") are still shown when applicable. */
  auto has_indirect_object = [] (const string &v) {
    static const char *preps[] = { " with ", " using ", nullptr };
    for (const char **p = preps; *p; ++p)
      {
	string::size_type pos = v.find (*p);
	if (pos != string::npos && pos + strlen (*p) < v.size ())
	  return true;
      }
    /* "use <noun>" / "give <noun>" / "give to <noun>" — a verb followed
       directly by a named object, with or without a preposition.  (The
       "anything" catch-alls are handled by the caller, before this.) */
    static const char *verbs[] = { "use ", "give ", "give to ", nullptr };
    for (const char **w = verbs; *w; ++w)
      {
	size_t n = strlen (*w);
	if (v.size () > n && ci_equal (v.substr (0, n), *w))
	  return true;
      }
    return false;
  };
  /* An object's `action <verb>` list is NOT a source of menu verbs on its own.
     Quest only ever runs an action whose key the engine dispatches -- the
     universal verbs and the built-in table above, plus the second-noun handlers
     (use/give/put) -- or one named by a `verb`/`lib verb` declaration in the
     *game* block; ExecVerb (V4Game.cs:2952-2955) scans nowhere else.  So an
     author who writes `action <lift>` and no `verb <lift>` has written dead
     code: LIFT PAINTING is "I don't understand your command" in real Quest too.
     Harvesting the action list unconditionally put such a verb in the menu,
     where clicking it typed a command the parser then rejected -- "Escape from
     this house" advertised Lift on its painting, which is reachable only by
     TAKE.  The one exception is the GIVE catch-all below, which the engine does
     dispatch with no declaration.

     (Nothing is lost for a genuinely declared verb: the game-block loop that
     follows adds it, checking the same object for the same handler.) */
  const GeasBlock *ob = gf.find_by_name ("object", obj);
  if (ob != NULL)
    for (const string &line: ob->data)
      {
	std::string::size_type c1, c2;
	if (first_token (line, c1, c2) != "action")
	  continue;
	string nm = next_token (line, c1, c2);
	if (!is_param (nm))
	  continue;
	for (const string &v: split_param (param_contents (nm)))
	  /* "give to anything" is this object being handed to someone the
	     player has not named yet, so it makes an entry -- but only as the
	     start of a command.  (Its mirror, "give anything", is this object
	     *receiving* something; that command starts with the other object,
	     so there is nothing here to offer.  See the "give ... to ..."
	     handler in run_commands.) */
	  if (ci_equal (trim (v), "give to anything"))
	    add ("Give to...", "give " + alias + " to ", true);
      }

  /* Global `verb <name[;syn]> ...` declarations the object responds to. */
  const GeasBlock *game = gf.find_by_name ("game", "game");
  if (game != NULL)
    for (const string &line: game->data)
      {
	std::string::size_type d1, d2;
	string tok = first_token (line, d1, d2);
	if (tok == "lib")                       /* optional library-verb prefix */
	  tok = next_token (line, d1, d2);
	string names_tok;
	if (tok == "verb")
	  names_tok = next_token (line, d1, d2);
	else if (is_param (tok))                /* bare "<name> <script>" form */
	  names_tok = tok;
	else
	  continue;
	if (!is_param (names_tok))
	  continue;
	/* The action or property the verb looks for on the object is whatever
	   follows a colon, and the colon is taken out of the name list; with no
	   colon it is the first (or only) name (V4Game.cs:2958-2976, mirrored in
	   try_game_verb).  So "verb <hoist;lift:raise>" is typed as HOIST or
	   LIFT, labelled "Hoist", and looks for `action <raise>` -- checking
	   responds() against the typed name instead missed every colon form.
	   (try_game_verb reads the name list with eval_param, which is not const;
	   an unresolved #string# reference in a verb name only arises in the QDK
	   mistake that leaves the verb dead anyway, so take it as written.) */
	string names_str = param_contents (names_tok), key;
	std::string::size_type colon = names_str.find (':');
	if (colon != string::npos)
	  {
	    key = trim (names_str.substr (colon + 1));
	    names_str = trim (names_str.substr (0, colon));
	  }
	vector<string> names = split_param (names_str);
	if (names.empty () || trim (names[0]) == "")
	  continue;
	if (key == "")
	  key = trim (names[0]);
	if (responds (key) && !has_indirect_object (trim (names[0])))
	  add (label_of (names[0]));
      }

  return out;
}

/* The runtime (state.props) half of a property lookup, shared by the object and
   room forms below.  Returns true if a record settles the question, in which
   case `value` is the answer; false if the static block has to be consulted. */
bool geas_implementation::runtime_prop (const string &obj, const string &prop,
					bool &value, string &string_rv) const
{
  string is_prop = "properties " + prop;
  string not_prop = "properties not " + prop;
  const vector<size_t> *recs = state.prop_records (obj);
  if (recs)
    /* Most-recently-set value wins: walk this object's records newest first. */
    for (auto ri = recs->rbegin (); ri != recs->rend (); ++ri)
      {
	const string &dat = state.props[*ri].data;
	if (ci_equal (dat, not_prop))
	  {
	    string_rv = "!";
	    value = false;
	    return true;
	  }
	if (ci_equal (dat, is_prop))
	  {
	    string_rv = "";
	    value = true;
	    return true;
	  }
	std::string::size_type index = dat.find ('=');
	if (index != string::npos && ci_equal (trim (dat.substr (0, index)), is_prop))
	  {
	    /* Trim so "prop = val" (spaces around '=', as some games write) reads
	     * back the same as "prop=val"; otherwise the name carries a trailing
	     * space and never matches, and the value carries a leading one. */
	    string_rv = trim (dat.substr (index+1));
	    value = true;
	    return true;
	  }
      }
  return false;
}

bool geas_implementation::get_obj_property(const string &obj, const string &prop,
					   string &string_rv) const
{
  bool value;
  if (runtime_prop (obj, prop, value, string_rv))
    return value;
  return gf.get_obj_property (obj, prop, string_rv, state.location);
}

/* As get_obj_property, but for a name that is known to be a room -- see
   get_room_action. */
bool geas_implementation::get_room_property (const string &room, const string &prop,
					     string &string_rv) const
{
  bool value;
  if (runtime_prop (room, prop, value, string_rv))
    return value;
  return gf.get_room_property (room, prop, string_rv);
}

bool geas_implementation::has_room_property (const string &room, const string &prop) const
{
  string tmp;
  return get_room_property (room, prop, tmp);
}

void geas_implementation::set_obj_property (const string &obj, const string &prop)
{
  state.add_prop (obj, "properties " + prop);
  /* Recompute the pane when something that affects what is in scope changes: an
   * object's own visibility, or a container's open/closed/seen/transparency that
   * governs its contents. */
  string base = prop;
  if (base.compare (0, 4, "not ") == 0)
    base = base.substr (4);
  bool own_visibility = ci_equal (base, "hidden") || ci_equal (base, "invisible");
  /* A change to the container's own state re-derives its contents' hidden flags,
   * exactly where Quest calls UpdateVisibilityInContainers: opening or closing it
   * (DoOpenClose), looking at it (DoLook, which sets "seen"), and putting
   * something in or taking it out (DoAddRemove).  Note that the object's *own*
   * hidden flag must not sweep -- a script's "show <X>" would undo itself. */
  bool container_state =
    ci_equal (base, "open")      || ci_equal (base, "closed")     ||
    ci_equal (base, "opened")    || ci_equal (base, "seen")       ||
    ci_equal (base, "transparent") || ci_equal (base, "container") ||
    ci_equal (base, "surface");
  /* Only this object's own contents are re-derived: every runtime call Quest
   * makes passes OnlyParent, so a sweep triggered by looking at, opening or
   * filling one container cannot touch anything held by another.  That matters
   * for a container that is never opened at all: "Pyramid Of Terror" reveals its
   * goth with a bare "show <goth>" from the sarcophagus's own open script, and a
   * global sweep put her straight back out of scope the moment anything else set
   * a "seen" flag, so SPEAK TO GOTH and GIVE BOOK TO GOTH answered "You don't
   * see any goth." */
  if (container_state)
    update_container_visibility (obj);
  if (own_visibility || container_state)
    {
      gi->update_sidebars();
      objs_vars_dirty_ = true;
    }
}

/* Quest 2.x addressed an object as "name@room", and SetAvailability is where that
 * is taken apart: below ASL 281 it splits the string at the "@", matches the half
 * after it against the object's ContainerRoom and the half before it against the
 * object's name, and an object that is not in that room is simply not found
 * (V4Game.Part2.cs:2994-3068).  From 281 the form is gone -- the whole string is
 * compared to the object name, so "X@room" matches nothing at all.
 *
 * geas keys properties by object name alone, so the most this can do is say
 * whether the reference resolves and hand back the bare name.  That is enough for
 * what the games do with it: "Venus flytrap: Romantic Music" (musicvf1.cas, ASL
 * 210) picks up every one of its objects with `give <X>` followed by `hideobject
 * <X@#quest.currentroom#>` -- the 2.x idiom, where "give" adds to the item list
 * and the world object has to be hidden separately -- and geas, passing the whole
 * "X@room" string through as a name, hid nothing, so everything you took stayed
 * lying in the room as well.  `showchar <Linda@lodger's room>` failed the same
 * way, which kept the game's two characters off stage entirely.
 *
 * Deliberately left alone: with no "@" at all, Quest below 281 presumes the
 * current room, where geas searches for the name wherever it is.  Narrowing that
 * would change every 2.x game in the corpus rather than just the ones using the
 * "@" form, and nothing so far needs it. */
bool geas_implementation::resolve_at_room (string &name) const
{
  string::size_type at = name.find ('@');
  if (at == string::npos)
    return true;
  if (asl_version_ >= 281)
    return true;   /* not a reference Quest would split: let it fail as a name */

  string room = trim (name.substr (at + 1)), bare = trim (name.substr (0, at));
  for (const auto &o: state.objs)
    if (ci_equal (o.name, bare) && ci_equal (o.parent, room))
      {
	name = bare;
	return true;
      }
  gi->debug_print ("No object '" + bare + "' in '" + room + "'");
  return false;
}

void geas_implementation::set_obj_action (const string &obj, const string &act)
{
  state.add_prop (obj, "action " + act);
}

void geas_implementation::move (const string &obj, const string &dest, int depth)
{
  const vector<size_t> *v = state.obj_records (obj);
  if (v == NULL)
    {
      gi->debug_print ("Tried to move nonexistent object '" + obj +
		       "' to '" + dest + "'.");
      return;
    }
  state.objs[(*v)[0]].parent = dest;

  /* "If this object contains other objects, move them too" -- MoveThing's own
   * comment (V4Game.cs:6648-6656).  The contents are the objects whose "parent"
   * property names this one, and they follow it into the new room, which is how
   * picking up a bag puts everything in it in the inventory as well.  Quest
   * recurses with nothing to stop it if a game has made the chain a cycle; we
   * cap the depth like every other walk up the chain.  The names are collected
   * first because the recursion writes to state.objs. */
  if (asl_version_ >= 391 && depth < kMaxContainerDepth)
    {
      vector<string> contents;
      for (const auto &o: state.objs)
	if (ci_equal (obj_container (o.name), obj))
	  contents.push_back (o.name);
      for (const string &c: contents)
	move (c, dest, depth + 1);
    }

  if (depth == 0)
    {
      gi->update_sidebars();
      objs_vars_dirty_ = true;
    }
}

/* See the comment on the declaration in geas-impl.hh. */
void geas_implementation::do_add_remove (const string &child, const string &parent,
					 bool adding, bool discover_parent)
{
  if (adding)
    {
      /* Quest writes the container's definition name, never its alias. */
      set_obj_property (child, "parent=" + parent);
      /* "_objs[childId].ContainerRoom = _objs[parentId].ContainerRoom": the
       * child is now wherever the container is, and stays with it from here on
       * because MoveThing carries a container's contents along.  Note this is a
       * plain assignment, not a move -- it does not recurse, so something
       * already inside the child keeps whatever room it had until the next real
       * move.  Quest is the same. */
      if (const vector<size_t> *v = state.obj_records (child))
	{
	  state.objs[(*v)[0]].parent = container_room (parent);
	  gi->update_sidebars();
	  objs_vars_dirty_ = true;
	}
    }
  else
    /* Taking something out of a container clears the property and nothing else:
     * the object has not gone anywhere, so its room is already right. */
    set_obj_property (child, "not parent");
  /* From ASL 4.10 on, moving something into or out of a container also marks
   * the container "seen"; Quest's own comment explains why -- otherwise you
   * could LOOK AT the object you just put in and have disambiguation fail
   * (DoAddRemove, V4Game.cs:2125-2131).  It belongs here, in the one routine
   * every add and every parented remove goes through, rather than at the
   * individual call sites: Quest has exactly two places that set "seen", this
   * one and DoLook, and only this one is version-gated. */
  if (discover_parent && asl_version_ >= 410 && parent != "")
    set_obj_property (parent, "seen");
  /* And re-derive what the container's contents can reach -- the last thing
   * DoAddRemove does (V4Game.cs:2133).  set_obj_property has already done it if
   * the "seen" above fired, but that is version-gated and this is not. */
  update_container_visibility (parent);
}

string geas_implementation::get_obj_parent (const string &obj)
{
  if (const vector<size_t> *v = state.obj_records (obj))
    return state.objs[(*v)[0]].parent;
  gi->debug_print ("Tried to find parent of nonexistent object " + obj);
  return "";
}

bool geas_implementation::is_held (const string &name) const
{
  if (const vector<size_t> *v = state.obj_records (name))
    for (size_t idx: *v)
      if (ci_equal (state.objs[idx].parent, "inventory"))
	return true;
  for (const string &it: state.items)
    if (ci_equal (it, name))
      return true;
  /* No walk up the container chain here: from 3.91 what is in a carried
   * container is carried too, but that falls out of the container's own room
   * already, because MoveThing takes the contents with it (V4Game.cs:6648-6656)
   * and DoAddRemove starts them off in the container's room.  "TARDIS Escape"
   * is the game that notices -- it refused `take sonic screwdriver` as already
   * carried and then refused to *use* the screwdriver because the player did
   * not have one -- and it passes on the moved room alone. */
  return false;
}

bool geas_implementation::names_object_in_scope (const string &word) const
{
  for (const auto &o: state.objs)
    {
      if (!(ci_equal (o.parent, state.location) || ci_equal (o.parent, "inventory")))
	continue;
      if (has_obj_property (o.name, "hidden"))
	continue;
      /* exact match only (no partial), so we only protect a true object name */
      if (match_object (word, o.name, false, false))
	return true;
    }
  return false;
}

bool geas_implementation::container_is_open (const string &name) const
{
  /* Honour an explicit open/closed state -- set by the open/close verbs, or
   * declared with "opened"/"closed".  A container with no stated state defaults
   * to closed, as in Quest.  (The state flag is "opened" -- Quest's name --
   * rather than "open", so it does not collide with the "open" verb's own
   * action/property lookup.) */
  if (has_obj_property (name, "opened")) return true;
  if (has_obj_property (name, "closed")) return false;
  return false;
}

/* See the comment on the declaration in geas-impl.hh. */
bool geas_implementation::player_can_access (const string &obj, string &errmsg) const
{
  errmsg = "";
  /* Bounded like the other chain walks: a game can parent two containers into
   * each other, and Quest guards the same way -- it keeps the objects it has
   * already passed, logs "Looped object parents detected" on a repeat, and
   * answers "no access" with no message at all. */
  string cur = obj;
  for (int guard = 0; guard < kMaxContainerDepth; guard++)
    {
      string parent;
      if (!get_obj_property (cur, "parent", parent) || parent == "")
	return true;
      /* Note "opened", not container_is_open: this is Quest's own pair of
       * tests, and neither "closed" nor "transparent" is one of them -- you
       * can see into a transparent box and still not reach in. */
      if (!has_obj_property (parent, "surface") &&
	  !has_obj_property (parent, "opened"))
	{
	  errmsg = "inside closed " + displayed_name (parent);
	  return false;
	}
      cur = parent;
    }
  return false;
}

bool geas_implementation::container_in_scope (const string &name, const vector<string> &where) const
{
  /* Walk up the container chain iteratively, bounded by kMaxContainerDepth: a
   * game can make the chain a cycle (put the bag in the box and the box in the
   * bag), and recursing on the parent would then never bottom out.  See
   * room_of, which is bounded the same way. */
  string cur = name;
  for (int guard = 0; guard < kMaxContainerDepth; guard++)
    {
      const vector<size_t> *v = state.obj_records (cur);
      if (v == NULL)
	return false;
      const ObjectRecord *obj = &state.objs[(*v)[0]];
      bool is_surface = has_obj_property (obj->name, "surface");
      if ((!has_obj_property (obj->name, "container") && !is_surface) ||
	  has_obj_property (obj->name, "hidden"))
	return false;
      /* Reachability of the contents (Quest's rule): a surface's are always
       * reachable; a container's only when it is open or transparent (you can
       * see through a transparent container even while shut) AND it has been
       * "seen" -- discovered by looking at, opening, or putting into it -- so
       * the contents of an as-yet-undiscovered container are out of scope. */
      if (!is_surface)
	{
	  if (!container_is_open (obj->name) && !has_obj_property (obj->name, "transparent"))
	    return false;
	  if (!has_obj_property (obj->name, "seen"))
	    return false;
	}
      for (const auto &loc: where)
	if (loc == "game" || ci_equal (obj->parent, loc))
	  return true;
      /* the container may itself sit inside another open container, whose room
       * is the one that decides */
      cur = obj_container (obj->name);
      if (cur == "")
	return false;
    }
  return false;   /* ran off the depth cap: the chain must be a cycle */
}

string geas_implementation::container_room (const string &obj) const
{
  if (const vector<size_t> *v = state.obj_records (obj))
    return state.objs[(*v)[0]].parent;
  return "";
}

string geas_implementation::obj_container (const string &obj) const
{
  string parent;
  if (get_obj_property (obj, "parent", parent))
    return parent;
  return "";
}

bool geas_implementation::is_inside (const string &inner, const string &outer) const
{
  /* Would putting `inner` into `outer` nest a container inside itself?  True if
   * they are the same object, or if outer already sits (however deeply) within
   * inner.  Bounded like the other chain walks so an already-cyclic chain -- a
   * state an older save may hold -- cannot hang us. */
  if (ci_equal (inner, outer))
    return true;
  string p = obj_container (outer);
  for (int guard = 0; guard < kMaxContainerDepth && p != ""; guard++)
    {
      if (ci_equal (p, inner))
	return true;
      p = obj_container (p);
    }
  return false;
}

bool geas_implementation::content_available (const string &P) const
{
  /* "If the parent is a surface, then the contents are always available.
   * Otherwise, only if the parent has been seen, AND is either open or
   * transparent, then the contents are available." -- Quest's own comment on
   * the one line this is (V4Game.Part2.cs:7721).  No walk: a box shut inside
   * another box still says its contents are available, and the player is kept
   * out of them by player_can_access instead. */
  if (has_obj_property (P, "surface"))
    return true;
  if (!has_obj_property (P, "seen"))
    return false;
  return container_is_open (P) || has_obj_property (P, "transparent");
}

/* `only_parent` empty sweeps every contained object, which is what Quest does
 * once at the end of loading (V4Game.Part2.cs:3611).  Every *runtime* call names
 * the one container whose state just changed and updates only its direct
 * children (UpdateVisibilityInContainers' OnlyParent argument,
 * V4Game.Part2.cs:7650-7708); the container's own availability is not in
 * question there, only its contents'. */
void geas_implementation::update_container_visibility (const string &only_parent)
{
  if (sweeping)
    return;
  sweeping = true;
  /* "If object has a parent object" -- the *property*, not the room, which is
   * why nothing standing loose in a room is ever swept.  Whether a container's
   * contents are available cannot change during the sweep (only the children's
   * hidden flags are written) and many objects share a container, so compute
   * each one once. */
  std::unordered_map<string, bool> parent_memo;
  for (const auto &o: state.objs)
    {
      string p = obj_container (o.name);
      if (p == "")
	continue;
      if (only_parent != "" && !ci_equal (p, only_parent))
	continue;
      auto it = parent_memo.find (lcase (p));
      if (it == parent_memo.end ())
	it = parent_memo.emplace (lcase (p), content_available (p)).first;
      bool avail = it->second;
      bool hidden = has_obj_property (o.name, "hidden");
      /* Toggle via add_prop (not set_obj_property) so we don't recurse through
       * regen; only when it actually changes, to bound state.props growth. */
      if (avail && hidden)
	state.add_prop (o.name, "properties not hidden");
      else if (!avail && !hidden)
	state.add_prop (o.name, "properties hidden");
    }
  sweeping = false;
}

/* True if object `obj` is declared inside container `name` with a bare
 * "<name>" line (the line is a lone parameter and nothing else). */
bool geas_implementation::declared_inside (const string &obj, const string &name) const
{
  const GeasBlock *gb = gf.find_by_name ("object", obj);
  if (gb == NULL)
    return false;
  for (const string &line: gb->data)
    {
      std::string::size_type c1, c2;
      string tok = first_token (line, c1, c2);
      if (is_param (tok) && next_token (line, c1, c2) == "" &&
	  ci_equal (trim (param_contents (tok)), name))
	return true;
    }
  return false;
}

/* See the comment on the declaration in geas-impl.hh. */
string geas_implementation::list_contents (const string &name)
{
  string s;
  if (!has_obj_property (name, "container"))
    return "";

  /* Shut, and neither transparent nor a surface: the contents are not on show,
   * and all the object has to say is its "list closed" line, if it has one. */
  if (!container_is_open (name) && !has_obj_property (name, "transparent") &&
      !has_obj_property (name, "surface"))
    {
      if (get_obj_action (name, "list closed", s))
	{ run_script_as (name, s); return "<script>"; }
      if (get_obj_property (name, "list closed", s))
	return s;
      return "";
    }

  /* Quest's contents are the objects whose "parent" property names this one and
   * that are both Exists and Visible -- the pair geas keeps as the "hidden"
   * flag update_container_visibility derives, plus an authored "invisible". */
  vector<string> items;
  for (const auto &o: state.objs)
    if (ci_equal (obj_container (o.name), name) &&
	!has_obj_property (o.name, "hidden") &&
	!has_obj_property (o.name, "invisible"))
      {
	string disp, affix;
	if (!get_obj_property (o.name, "alias", disp))
	  disp = o.name;
	disp = "|b" + disp + "|xb";
	if (get_obj_property (o.name, "prefix", affix) && affix != "")
	  disp = affix + " " + disp;
	if (get_obj_property (o.name, "suffix", affix) && affix != "")
	  disp = disp + " " + affix;
	items.push_back (disp);
      }

  if (items.empty ())
    {
      if (get_obj_action (name, "list empty", s))
	{ run_script_as (name, s); return "<script>"; }
      if (get_obj_property (name, "list empty", s))
	return s;
      return "";
    }

  if (get_obj_action (name, "list", s))
    { run_script_as (name, s); return "<script>"; }
  /* No "list" property at all means the game wrote `list off` -- every object
   * has one otherwise (see the loader) -- and that says: never list me. */
  if (!get_obj_property (name, "list", s))
    return "";

  string header;
  bool show_items = true;
  if (s != "")
    {
      /* A header ending in a colon introduces the list; one that does not *is*
       * the whole answer, and the objects are not named at all. */
      if (s[s.length () - 1] == ':')
	header = s.substr (0, s.length () - 1) + " ";
      else
	{ header = s; show_items = false; }
    }
  else
    {
      /* The default header is built from the object's "article", which is "it"
       * unless the game says otherwise -- hence "It contains ...". */
      string art;
      if (!get_obj_property (name, "article", art) || art == "")
	art = "it";
      art[0] = std::toupper ((unsigned char) art[0]);
      header = art + " contains ";
    }

  if (show_items)
    for (size_t i = 0; i < items.size (); i++)
      header += (i == 0 ? "" :
		 (i + 1 == items.size () ? " and " : ", ")) + items[i];
  return header + ".";
}

/* See the comment on the declaration in geas-impl.hh. */
void geas_implementation::do_look (const string &obj, bool examine_error,
				   bool show_default)
{
  /* Looking at something discovers it, and re-derives what its contents can
   * reach -- which is why a container has to be looked at (or opened, which
   * looks at it) before the player can reach into it.  Quest only bothers below
   * DoLook's ASLVersion >= 391 gate, having no container model at all under it;
   * geas emulates one at every version and reads the same flag, so the flag is
   * set at every version too, as the look handler has always done. */
  set_obj_property (obj, "seen");

  bool found_look = dispatch_obj_verb (obj, "look");

  /* Worked out before the fallback error is printed, because it decides
   * whether there is one: an undescribed container answers with its contents
   * rather than with "Nothing out of the ordinary". */
  string contents;
  if (asl_version_ >= 391)
    contents = list_contents (obj);

  if (!found_look && show_default && contents == "")
    display_error (examine_error ? "defaultexamine" : "defaultlook", obj);
  if (contents != "" && contents != "<script>")
    print_formatted (contents);
}

bool geas_implementation::open_container (const string &name)
{
  /* A container's contents are either parented to it (the container_in_scope
   * model) or declared inside it with a bare "<container>" line (which we
   * un-hide).  Mark the container open so its contents come into scope, then
   * reveal/list both kinds. */
  bool is_container = has_obj_property (name, "container");
  vector<string> shown_names;
  for (const auto &o: state.objs)
    {
      bool inside = ci_equal (obj_container (o.name), name) ||
		    declared_inside (o.name, name);
      if (!inside)
	continue;
      is_container = true;
      if (has_obj_property (o.name, "hidden"))
	set_obj_property (o.name, "not hidden");
      string disp;
      if (!get_obj_property (o.name, "alias", disp))
	disp = o.name;
      shown_names.push_back (disp);
    }
  if (!is_container)
    return false;   /* not a container: caller falls back to "can't do that" */

  set_obj_property (name, "not closed");
  set_obj_property (name, "opened");

  string msg = "You open the " + name + ".";
  for (size_t i = 0; i < shown_names.size (); i++)
    msg += (i == 0 ? "  Inside you see " :
	    (i + 1 == shown_names.size () ? " and " : ", ")) + shown_names[i];
  if (!shown_names.empty ())
    msg += ".";
  print_formatted (msg);
  return true;
}

bool geas_implementation::close_container (const string &name)
{
  /* Mark the container closed -- its parent-based contents leave scope via
   * container_in_scope -- and re-hide any bare-line contents still inside (not
   * carried off by the player). */
  bool is_container = has_obj_property (name, "container");
  vector<string> bareline;
  for (const auto &o: state.objs)
    {
      if (ci_equal (obj_container (o.name), name))
	is_container = true;
      else if (declared_inside (o.name, name))
	{ is_container = true; bareline.push_back (o.name); }
    }
  if (!is_container)
    return false;

  set_obj_property (name, "not opened");
  set_obj_property (name, "closed");
  for (const string &c: bareline)
    if (!is_held (c))
      set_obj_property (c, "hidden");
  print_formatted ("You close the " + name + ".");
  return true;
}

/* See the comment on the declaration in geas-impl.hh. */
void geas_implementation::do_open_close (const string &name, bool opening,
					bool show_look)
{
  if (opening)
    {
      set_obj_property (name, "not closed");
      set_obj_property (name, "opened");
      /* geas also lets a game put an object in a container by writing a bare
       * "<container>" line in the *object's* block.  Those contents are
       * modelled with the hidden flag rather than a parent, so the sweep
       * set_obj_property runs cannot reach them; reveal them by hand. */
      for (const auto &o: state.objs)
	if (declared_inside (o.name, name) && has_obj_property (o.name, "hidden"))
	  set_obj_property (o.name, "not hidden");
      /* Quest shows the container here -- DoLook with the default description
       * suppressed, so an object with no `look` of its own stays quiet and only
       * its contents are announced -- and DoLook is what marks it "seen", from
       * 3.91 on.  Which is also why the contents come out *after* the "You open
       * it." the caller has already printed. */
      if (show_look)
	do_look (name, false, false);
    }
  else
    {
      set_obj_property (name, "not opened");
      set_obj_property (name, "closed");
      for (const auto &o: state.objs)
	if (declared_inside (o.name, name) && !is_held (o.name))
	  set_obj_property (o.name, "hidden");
    }
}

/* See the comment on the declaration in geas-impl.hh. */
bool geas_implementation::remove_from_container (const string &child,
						const string &parent)
{
  if (parent == "")
    {
      display_error ("cantremove", child);
      return false;
    }
  /* Quest asks for "container" and then for "surface or opened" (V4Game.cs:2500,
   * 2564): a `surface` object is given *both* tags when it is parsed
   * (V4Game.Part2.cs:3448-3454), so an altar you can lift a ruby off passes the
   * container test too, while a closed chest fails the second one and the
   * removal (and any take that asked for it) is refused. */
  if (!has_obj_property (parent, "container") &&
      !has_obj_property (parent, "surface"))
    {
      display_error ("cantremove", child);
      return false;
    }
  if (!has_obj_property (parent, "surface") && !container_is_open (parent))
    {
      display_error ("cantremove", child);
      return false;
    }

  string script;
  if (get_obj_action (parent, "remove", script))
    {
      /* An action is trusted to do the removal itself, with
       * `remove <#quest.remove.object.name#>` -- Barbarian's coffin resets the
       * pressure plate under the rock it hands back. */
      set_svar ("quest.remove.object.name", child);
      run_script_as (parent, script);
      return true;
    }
  if (get_obj_property (parent, "remove", script))
    {
      if (script != "")
	print_formatted (script);
      else
	display_error ("defaultremove", child);
      do_add_remove (child, parent, false);
      return true;
    }
  display_error ("cantremove", child);
  return false;
}
