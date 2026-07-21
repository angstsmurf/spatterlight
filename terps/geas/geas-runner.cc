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
#include "geas-impl.hh"
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "general.hh"
#include "istring.hh"

#ifdef SPATTERLIGHT
/* Use the shared erkyrath_random() RNG (xoshiro128** when seeded, native
   otherwise), like scott/comprehend/plus/taylor.  The headless walkthrough
   runner has no Glk, so it keeps the C library rand() below. */
extern "C" {
#include "randomness.h"
}
extern "C" int gli_determinism;
#endif

class GeasInterface;

using namespace std;

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

bool geas_implementation::find_ivar (const string &name, size_t &rv) const
{
  for (size_t n = 0; n < state.ivars.size(); n ++)
    if (ci_equal (state.ivars[n].name, name))
      {
	rv = n;
	return true;
      }
  return false;
}

bool geas_implementation::find_svar (const string &name, size_t &rv) const
{
  for (size_t n = 0; n < state.svars.size(); n ++)
    if (ci_equal (state.svars[n].name, name))
      {
	rv = n;
	return true;
      }
  return false;
}

bool geas_implementation::split_var_index (const string &varname, const char *who,
					   string &base, size_t &index) const
{
  std::string::size_type i1 = varname.find ('[');
  if (i1 == string::npos)
    {
      base = varname;
      index = 0;
      return true;
    }
  if (varname[varname.length() - 1] != ']')
    {
      gi->debug_print (string (who) + ": Badly formatted name " + varname);
      return false;
    }
  base = varname.substr (0, i1);
  string indextext = trim (varname.substr (i1+1, varname.length() - i1 - 2));
  GEAS_DBG << who << " (" << varname << ") --> (" << base << ", " << indextext << ")\n";

  /* A subscript is either a decimal literal or the name of a numeric variable
   * holding one.  Anything else -- including an undefined variable, which reads
   * back as the -32767 sentinel -- is refused rather than passed on as a
   * subscript; see kMaxVarIndex. */
  long value;
  bool is_literal = !indextext.empty ();
  for (size_t c3 = 0; c3 < indextext.size (); c3 ++)
    if (indextext[c3] < '0' || indextext[c3] > '9')
      {
	is_literal = false;
	break;
      }
  value = is_literal ? parse_int (indextext) : get_ivar (indextext);
  if (value < 0 || (size_t) value > kMaxVarIndex)
    {
      gi->debug_print (string (who) + ": Bad index [" + indextext + "] in " + varname);
      return false;
    }
  index = (size_t) value;
  return true;
}

void geas_implementation::set_svar (const string &varname, const string &varval)
{
  GEAS_DBG << "set_svar (" << varname << ", " << varval << ")\n";
  string base;
  size_t index;
  if (split_var_index (varname, "set_svar", base, index))
    set_svar (base, index, varval);
}

void geas_implementation::set_svar (const string &varname, size_t index, const string &varval)
{
  size_t n, m;
  if (!find_svar (varname, n))
    {
      if (find_ivar (varname, m))
	{
	  gi->debug_print ("Defining " + varname + " as string variable when there is already a numeric variable of that name.");
	  return;
	}
      SVarRecord svr;
      svr.name = varname;
      n = state.svars.size();
      state.svars.push_back (svr);
    }
  state.svars[n].set(index, varval);
  if (index == 0)
    {
      for (size_t varn = 0; varn < gf.size("variable"); varn ++)
	{
	  const GeasBlock &go (gf.block ("variable", varn));
	  if (ci_equal (go.name, varname))
	    {
	      string script = "";
	      std::string::size_type c1, c2;
	      for (uint j = 0; j < go.data.size(); j ++)
		// SENSITIVE ?
		if (first_token (go.data[j], c1, c2) == "onchange")
		  script = trim (go.data[j].substr (c2 + 1));
	      if (script != "")
		run_script (script);
	    }
	}
    }
}

string geas_implementation::get_svar (const string &varname) const
{
  string base;
  size_t index;
  if (!split_var_index (varname, "get_svar", base, index))
    return "";
  return get_svar (base, index);
}
string geas_implementation::get_svar (const string &varname, size_t index) const
{
  for (const auto &i: state.svars)
    {
      if (ci_equal (i.name, varname))
	return i.get(index);
    }

  gi->debug_print ("get_svar (" + varname + ", " + string_int (index) + "): No such variable defined.");
  return "";
}

int geas_implementation::get_ivar (const string &varname) const
{
  string base;
  size_t index;
  if (!split_var_index (varname, "get_ivar", base, index))
    return -32767;
  return get_ivar (base, index);
}
int geas_implementation::get_ivar (const string &varname, size_t index) const
{
  for (const auto &i: state.ivars)
    if (ci_equal (i.name, varname))
      return i.get(index);
  gi->debug_print ("get_ivar: Tried to read undefined int '" + varname +
		   "' [" + string_int(index) + "]");
  return -32767;
}
double geas_implementation::get_dvar (const string &varname) const
{
  string base;
  size_t index;
  if (!split_var_index (varname, "get_dvar", base, index))
    return -32767.0;
  return get_dvar (base, index);
}
double geas_implementation::get_dvar (const string &varname, size_t index) const
{
  for (const auto &i: state.ivars)
    if (ci_equal (i.name, varname))
      return i.getd(index);
  return -32767.0;
}
void geas_implementation::set_ivar (const string &varname, int varval)
{
  set_ivar (varname, (double) varval);
}

void geas_implementation::set_ivar (const string &varname, double varval)
{
  string base;
  size_t index;
  if (split_var_index (varname, "set_ivar", base, index))
    set_ivar (base, index, varval);
}

void geas_implementation::set_ivar (const string &varname, size_t index, double varval)
{
  size_t n, m;
  if (!find_ivar (varname, n))
    {
      if (find_svar (varname, m))
	{
	  gi->debug_print ("Defining " + varname + " as numeric variable when there is already a string variable of that name.");
	  return;
	}
      IVarRecord ivr;
      ivr.name = varname;
      n = state.ivars.size();
      state.ivars.push_back (ivr);
    }
  state.ivars[n].set(index, varval);
  if (index == 0)
    {
      for (size_t varn = 0; varn < gf.size("variable"); varn ++)
	{
	  const GeasBlock &go (gf.block ("variable", varn));
	  if (ci_equal (go.name, varname))
	    {
	      string script = "";
	      std::string::size_type c1, c2;
	      for (uint j = 0; j < go.data.size(); j ++)
		// SENSITIVE?
		if (first_token (go.data[j], c1, c2) == "onchange")
		  script = trim (go.data[j].substr (c2 + 1));
	      if (script != "")
		run_script (script);
	    }
	}
    }
}



ostream &operator<< (ostream &o, const match_binding &mb)
{
  o << "MB['" << mb.var_name << "' == '" << mb.var_text << "' @ " 
    << mb.start << " to " << mb.end << "]";
  return o;
}

string match_binding::tostring ()
{
  ostringstream oss;
  oss << *this;
  return oss.str();
}

ostream &operator << (ostream &o, const set<string> &s)
{
  o << "{ ";
  for (set<string>::const_iterator i = s.begin(); i != s.end(); i ++)
    {
      if (i != s.begin())
	o << ", ";
      o << (*i);
    }
  o << " }";
  return o;
}

bool geas_implementation::has_obj_action (const string &obj, const string &prop) const
{
  string tmp;
  return get_obj_action (obj, prop, tmp);
}


bool geas_implementation::get_obj_action (const string &objname, const string &actname,
					  string &rv) const
{

  GEAS_DBG << "get_obj_action (" << objname << ", " << actname << ")\n";
  string tok;
  std::string::size_type c1, c2;
  const vector<size_t> *recs = state.prop_records (objname);
  if (recs)
    /* Most-recently-set action wins: walk this object's records newest first. */
    for (auto ri = recs->rbegin (); ri != recs->rend (); ++ri)
      {
	const string &line = state.props[*ri].data;
	// SENSITIVE?
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
  /* Prefer the block defined in the current room, so same-named objects in
   * different rooms keep their own actions. */
  return gf.get_obj_action (objname, actname, rv, state.location);
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

  /* The object's own explicit `action <verb>` definitions carry custom verbs
     the tables above don't know about.  Skip the engine-internal action keys
     that are never player-typed menu verbs. */
  auto is_skipped = [] (const string &key) {
    static const char *skip[] = { "gain", "lose", "description", "script" };
    for (const char *s: skip)
      if (ci_equal (key, s))
	return true;
    return false;
  };
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
	  {
	    string key = trim (v);
	    /* The two GIVE catch-alls (see the "give ... to ..." handler in
	       run_commands).  "give to anything" is this object being handed
	       over, so it makes a menu entry -- but only as the start of a
	       command, since the recipient is still missing.  "give anything"
	       is the mirror case, this object receiving something the player
	       has not named yet; that command starts with the *other* object,
	       so there is nothing here to offer.  Both would otherwise be
	       listed raw ("Give anything", "Give to anything"), reading as two
	       duplicate verbs that do nothing when clicked.  Checked before
	       has_indirect_object, which drops the named-recipient forms. */
	    if (ci_equal (key, "give anything"))
	      continue;
	    if (ci_equal (key, "give to anything"))
	      {
		add ("Give to...", "give " + alias + " to ", true);
		continue;
	      }
	    if (is_skipped (key) || has_indirect_object (key))
	      continue;
	    add (label_of (v));
	  }
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
	vector<string> names = split_param (param_contents (names_tok));
	if (names.empty () || trim (names[0]) == "")
	  continue;
	if (responds (trim (names[0])) && !has_indirect_object (trim (names[0])))
	  add (label_of (names[0]));
      }

  return out;
}

bool geas_implementation::get_obj_property(const string &obj, const string &prop,
					   string &string_rv) const
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
	    return false;
	  }
	if (ci_equal (dat, is_prop))
	  {
	    string_rv = "";
	    return true;
	  }
	std::string::size_type index = dat.find ('=');
	if (index != string::npos && ci_equal (trim (dat.substr (0, index)), is_prop))
	  {
	    /* Trim so "prop = val" (spaces around '=', as some games write) reads
	     * back the same as "prop=val"; otherwise the name carries a trailing
	     * space and never matches, and the value carries a leading one. */
	    string_rv = trim (dat.substr (index+1));
	    return true;
	  }
      }
  return gf.get_obj_property (obj, prop, string_rv, state.location);
}

void geas_implementation::set_obj_property (const string &obj, const string &prop)
{
  state.add_prop (obj, "properties " + prop);
  /* Recompute the pane (and container visibility) when something that affects
   * what is in scope changes: an object's own visibility, or a container's
   * open/closed/seen/transparency that governs its contents. */
  string base = prop;
  if (base.compare (0, 4, "not ") == 0)
    base = base.substr (4);
  if (ci_equal (base, "hidden")    || ci_equal (base, "invisible")  ||
      ci_equal (base, "open")      || ci_equal (base, "closed")     ||
      ci_equal (base, "opened")    || ci_equal (base, "seen")       ||
      ci_equal (base, "transparent") || ci_equal (base, "container") ||
      ci_equal (base, "surface"))
    {
      gi->update_sidebars();
      regen_var_objects();
    }
}

void geas_implementation::set_obj_action (const string &obj, const string &act)
{
  state.add_prop (obj, "action " + act);
}

void geas_implementation::move (const string &obj, const string &dest)
{
  for (auto &i: state.objs)
    if (ci_equal (i.name, obj))
      {
	i.parent = dest;
	gi->update_sidebars();
	regen_var_objects();
	return;
      }
  gi->debug_print ("Tried to move nonexistent object '" + obj + 
		   "' to '" + dest + "'.");
}

string geas_implementation::get_obj_parent (const string &obj)
{
  //obj = lcase (obj);
  for (const auto &i: state.objs)
    if (ci_equal (i.name, obj))
      return i.parent;
  gi->debug_print ("Tried to find parent of nonexistent object " + obj);
  return "";
}

bool geas_implementation::is_held (const string &name) const
{
  for (const auto &o: state.objs)
    if (ci_equal (o.name, name) && ci_equal (o.parent, "inventory"))
      return true;
  for (const string &it: state.items)
    if (ci_equal (it, name))
      return true;
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

bool geas_implementation::container_in_scope (const string &name, const vector<string> &where) const
{
  /* Walk up the container chain iteratively, bounded by kMaxContainerDepth: a
   * game can make the chain a cycle (put the bag in the box and the box in the
   * bag), and recursing on the parent would then never bottom out.  See
   * room_of, which is bounded the same way. */
  string cur = name;
  for (int guard = 0; guard < kMaxContainerDepth; guard++)
    {
      const ObjectRecord *obj = NULL;
      for (const auto &o: state.objs)
	if (ci_equal (o.name, cur))
	  {
	    obj = &o;
	    break;
	  }
      if (obj == NULL)
	return false;
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
      /* the container may itself sit inside another open container */
      cur = obj->parent;
    }
  return false;   /* ran off the depth cap: the chain must be a cycle */
}

string geas_implementation::obj_parent (const string &obj) const
{
  for (const auto &o: state.objs)
    if (ci_equal (o.name, obj))
      return o.parent;
  return "";
}

string geas_implementation::room_of (const string &obj) const
{
  string p = obj_parent (obj);
  for (int guard = 0; guard < kMaxContainerDepth && p != ""; guard++)
    {
      if (!has_obj_property (p, "container") && !has_obj_property (p, "surface"))
	return p;            /* p is a room/inventory, not a container */
      p = obj_parent (p);    /* p is a container -> keep walking up */
    }
  return p;
}

bool geas_implementation::is_inside (const string &inner, const string &outer) const
{
  /* Would putting `inner` into `outer` nest a container inside itself?  True if
   * they are the same object, or if outer already sits (however deeply) within
   * inner.  Bounded like the other chain walks so an already-cyclic chain -- a
   * state an older save may hold -- cannot hang us. */
  if (ci_equal (inner, outer))
    return true;
  string p = obj_parent (outer);
  for (int guard = 0; guard < kMaxContainerDepth && p != ""; guard++)
    {
      if (ci_equal (p, inner))
	return true;
      p = obj_parent (p);
    }
  return false;
}

bool geas_implementation::content_available (const string &P) const
{
  /* Iterative, bounded walk up the container chain: see container_in_scope. */
  string cur = P;
  for (int guard = 0; guard < kMaxContainerDepth; guard++)
    {
      bool surf = has_obj_property (cur, "surface");
      if (!surf && !has_obj_property (cur, "container"))
	return false;
      if (!surf)
	{
	  if (!container_is_open (cur) && !has_obj_property (cur, "transparent"))
	    return false;
	  if (!has_obj_property (cur, "seen"))
	    return false;
	}
      /* Is cur itself reachable?  If it sits directly in a room/inventory, it is
       * available unless it has been (authored-)hidden.  If it is nested inside
       * another container, its availability is that container's.  We deliberately
       * read the parent chain's *state* (not the sweep-managed hidden flags) so
       * the result is independent of the order objects are swept. */
      string pp = obj_parent (cur);
      if (pp == "" || (!has_obj_property (pp, "container") && !has_obj_property (pp, "surface")))
	return !has_obj_property (cur, "hidden");
      cur = pp;
    }
  return false;   /* ran off the depth cap: the chain must be a cycle */
}

void geas_implementation::update_container_visibility ()
{
  if (sweeping)
    return;
  sweeping = true;
  for (const auto &o: state.objs)
    {
      const string &p = o.parent;
      if (p == "" ||
	  (!has_obj_property (p, "container") && !has_obj_property (p, "surface")))
	continue;   /* only objects inside a container/surface are swept */
      bool avail = content_available (p);
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

vector<string> geas_implementation::container_contents (const string &name) const
{
  vector<string> rv;
  for (const auto &o: state.objs)
    if (ci_equal (o.parent, name) || declared_inside (o.name, name))
      {
	string disp;
	if (!get_obj_property (o.name, "alias", disp))
	  disp = o.name;
	rv.push_back (disp);
      }
  return rv;
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
      bool inside = ci_equal (o.parent, name) || declared_inside (o.name, name);
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
  set_obj_property (name, "seen");   /* opening it discovers it (seen gate) */

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
      if (ci_equal (o.parent, name))
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

void geas_implementation::goto_room (string room)
{
  state.location = room;
  regen_var_room();
  regen_var_dirs();
  regen_var_look();
  regen_var_objects();
  /* Quest's order (PlayGame): describe the room first, then run its entry
   * script.  If that script gotos another room, the nested goto describes the
   * destination -- so, unlike the old script-then-look order, there is no
   * trailing look() here to redescribe after a redirect. */
  look();
  string scr;
  if (get_obj_action (room, "script", scr))
    run_script_as (room, scr);
}

void geas_implementation::display_error (string errorname, string obj)
{
  GEAS_DBG << "display_error (" << errorname << ", " << obj << ")\n";
  if (obj != "")
    {
      string tmp;
      if (!get_obj_property (obj, "gender", tmp))
	tmp = "it";
      set_svar ("quest.error.gender", tmp);
      
      if (!get_obj_property (obj, "article", tmp))
	tmp = "it";
      set_svar ("quest.error.article", tmp);
     
      GEAS_DBG << "In erroring " << errorname << " / " << obj << ", qeg == "
	   << get_svar ("quest.error.gender") << ", qea == "
	   << get_svar ("quest.error.article") << endl;
      // TODO quest.error.charactername 
    }

  const GeasBlock *game = gf.find_by_name ("game", "game");
  if (game == NULL)
    {
      gi->debug_print ("display_error: no 'game' block found");
      return;
    }
  std::string::size_type c1, c2;
  for (const string &line: game->data)
    {
      string tok = first_token (line, c1, c2);
      // SENSITIVE?
      if (tok == "error")
	{
	  tok = next_token (line, c1, c2);
	  if (is_param (tok))
	    {
	      string text = param_contents(tok);
	      std::string::size_type index = text.find (';');
	      string errortype = trim (text.substr (0, index));
	      // SENSITIVE?
	      if (errortype == errorname)
		{
		  print_eval_p (trim (text.substr (index+1)));
		  return;
		}
	    }
	  else
	    gi->debug_print ("Bad error line: " + line);
	}
    }
  
  // ARE THESE SENSITIVE?
  if (errorname == "badcommand")
    print_eval ("I don't understand your command. Type HELP for a list of valid commands.");
  else if (errorname == "badgo")
    print_eval ("I don't understand your use of 'GO' - you must either GO in some direction, or GO TO a place.");
  else if (errorname == "badgive")
    print_eval ("You didn't say who you wanted to give that to.");
  else if (errorname == "badcharacter")
    print_eval ("I can't see anybody of that name here.");
  else if (errorname == "noitem")
    print_eval ("You don't have that.");
  else if (errorname == "itemunwanted")
    print_eval_p ("#quest.error.gender# doesn't want #quest.error.article#.");
  else if (errorname == "badlook")
    print_eval ("You didn't say what you wanted to look at.");
  else if (errorname == "badthing")
    print_eval ("I can't see that here.");
  else if (errorname == "defaultlook")
    print_eval ("Nothing out of the ordinary.");
  else if (errorname == "defaultspeak")
    print_eval_p ("#quest.error.gender# says nothing.");
  else if (errorname == "baditem")
    print_eval ("I can't see that anywhere.");
  else if (errorname == "defaulttake")
    print_eval ("You pick #quest.error.article# up.");
  else if (errorname == "baduse")
    print_eval ("You didn't say what you wanted to use that on.");
  else if (errorname == "defaultuse")
    print_eval ("You can't use that here.");
  else if (errorname == "defaultput")
    print_eval_p ("You can't put #quest.error.article# there.");
  else if (errorname == "defaultwait")
    print_eval ("Time passes...");
  else if (errorname == "defaultverb")
    print_eval ("You can't do that.");
  else if (errorname == "alreadyopen")
    print_eval ("It is already open.");
  else if (errorname == "alreadyclosed")
    print_eval ("It is already closed.");
  else if (errorname == "cantopen")
    print_eval ("You can't open that.");
  else if (errorname == "cantclose")
    print_eval ("You can't close that.");
  else if (errorname == "defaultout")
    print_eval ("There's nowhere you can go out to around here.");
  else if (errorname == "badplace")
    print_eval ("You can't go there.");
  else if (errorname == "defaultexamine")
    print_eval ("Nothing out of the ordinary.");
  else if (errorname == "badtake")
    print_eval ("You can't take #quest.error.article#.");
  else if (errorname == "cantdrop")
    print_eval ("You can't drop that here.");
  else if (errorname == "defaultdrop")
    print_eval ("You drop #quest.error.article#.");
  else if (errorname == "baddrop")
    print_eval ("You are not carrying such a thing.");
  else if (errorname == "badpronoun")
    print_eval ("I don't know what '#quest.error.pronoun#' you are referring to.");
  else if (errorname == "badexamine")
    print_eval ("You didn't say what you wanted to examine.");
  else
    gi->debug_print ("Bad error name " + errorname);
}

string geas_implementation::displayed_name (const string &obj) const
{
  string rv = obj, tmp;

  if (get_obj_property (obj, "alias", tmp))
    rv = tmp;
  else
    {
      for (const auto &i: gf.blocks)
	if (ci_equal (i.name, obj))
	  {
	    rv = i.name;
	    break;
	  }
    }
  return rv;
}

/* For each destination, give:
 * - printed name
 * - accepted name, with prefix
 * - accepted name, without prefix
 * - destination, internal format
 * - script (optional)
 */
vector<vector<string> > geas_implementation::get_places (const string &room)
{
  vector<vector<string> > rv;

  const GeasBlock *gb = gf.find_by_name ("room", room);
  if (gb == NULL)
    return rv;
  
  string line, tok;
  std::string::size_type c1, c2;
  for (const auto &line: gb->data)
    {
      tok = first_token (line, c1, c2);
      if (tok == "place")
	{
	  tok = next_token (line, c1, c2);
	  if (!is_param(tok))
	    {
	      gi->debug_print ("Expected parameter after 'place' in " + line);
	      continue;
	    }
	  string dest_param = eval_param (tok);
	  if (dest_param == "")
	    {
	      gi->debug_print ("Parameter empty in " + line);
	      continue;
	    }
	  std::string::size_type i = dest_param.find (';');
	  string dest, prefix = "";
	  if (i == string::npos)
	    dest = trim (dest_param);
	  else
	    {
	      dest = trim (dest_param.substr (i+1));
	      prefix = trim (dest_param.substr (0, i));
	    }
	  string displayed = displayed_name (dest);
	  string printed_dest = (prefix != "" ? prefix + " " : "") +
	    "|b" + displayed + "|xb";

	  vector<string> tmp;
	  tmp.push_back (printed_dest);
	  tmp.push_back (prefix + " " + displayed);
	  tmp.push_back (displayed);
	  tmp.push_back (dest);
	  string rest = trim (line.substr (c2));
	  if (rest != "")
	    tmp.push_back (rest);
	  rv.push_back (tmp);
	}
    }

  for (const auto &i: state.exits)
    {
      if (i.src != room)
	continue;
      line = i.dest;
      string tok = first_token (line, c1, c2);
      if (tok == "exit")
	{
	  tok = next_token (line, c1, c2);
	  if (!is_param(tok))
	    continue;
	  /* A directionless "place" exit is stored as "exit <src;dest>", so the
	   * token after "exit" is already the src;dest parameter.  (Directional
	   * dynamic exits, "exit <dir> <src;dest>", have a non-param direction
	   * token and were skipped by the !is_param check above.)  The previous
	   * code unconditionally consumed another token here, which mis-flagged
	   * every place exit as malformed. */
	  tok = param_contents(tok);
	  vector<string> args = split_param (tok);
	  if (args.size() != 2)
	    {
	      gi->debug_print ("Expected two arguments in " + tok);
	      continue;
	    }
	  if (args[0] != room) { report_unsupported ("exit source '" + args[0] + "' does not match room '" + room + "'"); continue; }
	  vector<string> tmp;
	  string displayed = displayed_name (args[1]);
	  tmp.push_back ("|b" + displayed + "|xb");
	  tmp.push_back (displayed);
	  tmp.push_back (displayed);
	  tmp.push_back (args[1]);
	  rv.push_back (tmp);
	}
      else if (tok == "destroy")
	{
	  tok = next_token (line, c1, c2);
	  if (tok != "exit") { report_unsupported ("expected 'exit' after 'destroy' in: " + line); continue; }
	  tok = next_token (line, c1, c2);

	  for (v2string::iterator j = rv.begin(); j != rv.end(); j ++)
	    if ((*j)[3] == tok)
	      {
		rv.erase(j);
		break;
	      }
	}
      
      
    }

  GEAS_DBG << "get_places (" << room << ") -> " << rv << "\n";
  return rv;
}

string geas_implementation::exit_dest (const string &room, const string &dir, bool *is_script) const
{
  std::string::size_type c1, c2;
  string tok;
  if (is_script != NULL)
    *is_script = false;
  for (auto i = state.exits.rbegin(); i != state.exits.rend(); i++)
    if (i->src == room)
      {
	const string &line = i->dest;
	GEAS_DBG << "Processing exit line '" << i->dest << "'\n";
	tok = first_token (line, c1, c2);
	GEAS_DBG << "   first tok is " << tok << " (vs. exit)\n";
	// SENSITIVE?
	/* A "noexit <dir>" record (pushed by disconnect) removes that exit;
	 * the latest record for the direction wins over an earlier create-exit
	 * or the static room exit below. */
	if (tok == "noexit")
	  {
	    if (next_token (line, c1, c2) == dir)
	      return "";
	    continue;
	  }
	if (tok != "exit")
	  continue;
	tok = next_token (line, c1, c2);
	GEAS_DBG << "   second tok is " << tok << " (vs. " << dir << ")\n";
	if (tok != dir)
	  continue;
	tok = next_token (line, c1, c2);
	GEAS_DBG << "   third tok is " << tok << " (expecting parameter)\n";
	if (!is_param (tok)) { report_unsupported ("malformed exit destination: " + line); continue; }
	vector<string> p = split_param (param_contents(tok));
	if (p.size() != 2) { report_unsupported ("unexpected exit parameters in: " + line); continue; }
	if (!ci_equal (p[0], room)) { report_unsupported ("exit source mismatch in: " + line); continue; }
	return p[1];
      }

  const GeasBlock *gb = gf.find_by_name ("room", room);
  if (gb == NULL)
    {
      gi->debug_print (string ("Trying to find exit <") + dir + 
		       "> of nonexistent room <" + room + ">.");
      return "";
    }
  // TODO: what's the priority on this?
  for (const string &line: gb->data) {
    string tok = first_token (line, c1, c2);
    if (tok == dir) {
      std::string::size_type line_start = c2;
      tok = next_token (line, c1, c2);
      /* "<dir> locked <dest; lockmessage>": a (initially locked) exit whose
       * destination is the first ;-separated field.  Locking only gates
       * traversal (handled by the caller), so return the destination here. */
      if (ci_equal (tok, "locked")) {
	tok = next_token (line, c1, c2);
	if (is_param (tok)) {
	  vector<string> p = split_param (param_contents (tok));
	  if (!p.empty ())
	    return trim (p[0]);
	}
	return "";
      }
      if (is_param (tok))
	return param_contents(tok);
      if (tok != "")
	{
	  if (is_script != NULL)
	    *is_script = true;
	  return trim (line.substr (line_start + 1));
	}
      return "";
    }
  }
  return "";
}

/* The lockmessage declared in "<dir> locked <dest; lockmessage>", or "". */
bool geas_implementation::exit_declared_locked (const string &room, const string &dir,
						string &message) const
{
  message = "";
  const GeasBlock *gb = gf.find_by_name ("room", room);
  if (gb == NULL)
    return false;
  std::string::size_type c1, c2;
  for (const string &line: gb->data) {
    if (first_token (line, c1, c2) != dir)
      continue;
    if (!ci_equal (next_token (line, c1, c2), "locked"))
      return false;
    string tok = next_token (line, c1, c2);
    if (is_param (tok)) {
      vector<string> p = split_param (param_contents (tok));
      if (p.size () >= 2)
	message = trim (p[1]);
    }
    return true;
  }
  return false;
}

/* The lockmessage declared in "<dir> locked <dest; lockmessage>", or "". */
string geas_implementation::exit_lock_message (const string &room, const string &dir) const
{
  string message;
  exit_declared_locked (room, dir, message);
  return message;
}

/* Is the (room, dir) exit currently locked?  An explicit lock/unlock wins
 * (stored as a "<room>;<dir>=locked|open" property on "!exitlock"); otherwise
 * it is locked iff the room declares it with the "locked" keyword. */
bool geas_implementation::exit_locked (const string &room, const string &dir)
{
  string val;
  if (get_obj_property ("!exitlock", lcase (room) + ";" + lcase (dir), val))
    return ci_equal (val, "locked");
  string message;
  return exit_declared_locked (room, dir, message);
}

void geas_implementation::look()
{
  string tmp;
  bool described = false;
  if (get_obj_action (state.location, "description", tmp))
    { run_script_as (state.location, tmp); described = true; }
  else if (get_obj_property (state.location, "description", tmp))
    { print_formatted (tmp); described = true; }
  else if (get_obj_action ("game", "description", tmp))
    { run_script_as ("game", tmp); described = true; }
  else if (get_obj_property ("game", "description", tmp))
    { print_formatted (tmp); described = true; }

  if (!described)
    {
      string in_desc;
      if (get_obj_property (state.location, "indescription", tmp))
	{
	  /* Quest's indescription: a trailing colon is replaced by a space, the
	   * room name and a full stop ("...west end of the:" becomes "...west
	   * end of the Entrance Hall.").  Without a trailing colon the text is
	   * printed verbatim, with no room name appended. */
	  in_desc = tmp;
	  if (!in_desc.empty () && in_desc[in_desc.size () - 1] == ':')
	    print_formatted (in_desc.substr (0, in_desc.size () - 1) + " "
			     + get_svar ("quest.formatroom") + ".");
	  else
	    print_formatted (in_desc);
	}
      else
	print_formatted (string ("You are in") + " "
			 + get_svar ("quest.formatroom"));
    }

  /* List the objects and characters present.  The default room display always
   * did this; do it after a custom description too -- otherwise a character such
   * as World's End's snowville "woman" is never mentioned and the player has no
   * way to know it is there.  Like the exits below, the original Quest runner
   * printed this inline in the main text ("There is a key here.") *and* mirrored
   * it in its objects pane, so print it in the main window even on a host that
   * provides such a pane (see has_objects_window); the pane stays a
   * supplementary copy. */
  regen_var_objects ();
  if ((tmp = get_svar ("quest.formatobjects")) != "")
    print_eval ("There is #quest.formatobjects# here.");

  /* List the available exits, after the objects.  The original Quest runner
   * printed these directly in the main text ("You can go north, south, east or
   * west.") *and* mirrored them in its compass/exits pane -- the pane was just a
   * set of clickable shortcuts, not a replacement for the inline line.  So
   * always print them in the main window, even on a host that also provides a
   * room-exits pane (see has_objects_window / get_room_exits); the pane stays a
   * supplementary copy.  Do this whether or not the room has a custom
   * description -- otherwise the player has no way to know which way to go out
   * of a custom-described room. */
  /* The *display* form, so an "out <the; town>" prefix shows up here ("You can
   * go out to the town."), as it does in the typelib's own version of this line
   * and in the "places" line below. */
  if (get_svar ("quest.doorways.out") != ""
      && (tmp = get_svar ("quest.doorways.out.display")) != "")
    print_formatted ("You can go out to " + tmp + ".");
  if ((tmp = get_svar ("quest.doorways.dirs")) != "")
    print_eval ("You can go #quest.doorways.dirs#.");
  if ((tmp = get_svar ("quest.doorways.places")) != "")
    print_formatted ("You can go to " + tmp + ".");

  if (!described)
    {
      if ((tmp = get_svar ("quest.lookdesc")) != "")
	print_formatted (tmp);
    }
}

bool geas_implementation::timer_will_fire ()
{
  /* tick_timers() runs a timer's action on the tick where its countdown has
   * already reached zero. */
  for (const auto &t: state.timers)
    if (t.is_running && t.timeleft == 0)
      return true;
  return false;
}

void geas_implementation::restart ()
{
  /* set_game re-reads and re-initialises everything from scratch (fresh
   * state, start room, intro, startscript). */
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
  regen_var_objects ();
  look ();
  return true;
}

std::string geas_implementation::get_location ()
{
  string name;
  if (!get_obj_property (state.location, "alias", name))
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

bool geas_implementation::load_state (const string &data, bool run_hooks)
{
  GeasState newstate;
  string gamename;
  if (!deserialize_game (data, gamename, newstate))
    return false;
  state = newstate;
  is_running_ = state.running = true;
  if (run_hooks)
    load_method_ = "loaded";   /* a real restore -> $loadmethod$ = "loaded" */
  /* Rebuild the cached views of the (now restored) current room. */
  regen_var_room ();
  regen_var_dirs ();
  regen_var_look ();
  regen_var_objects ();
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
   * leave rand() unseeded, so any random fight played out identically each time
   * -- which left World's End's final fight permanently unwinnable.  GEAS_SEED
   * overrides the seed for reproducible testing. */
  {
    const char *envseed = getenv ("GEAS_SEED");
#ifdef SPATTERLIGHT
    if (envseed)
      set_erkyrath_random ((glui32) atoi (envseed));
    else if (gli_determinism)
      set_erkyrath_random (1234);
    else
      set_erkyrath_random (0);
#else
    srand (envseed ? (unsigned) atoi (envseed) : (unsigned) time (nullptr));
#endif
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

      state = GeasState (*gi, gf);

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
	      // SENSITIVE?
	      if (tok == "fontname")
		{
		  tok = next_token (s, tok_start, tok_end);
		  if (!is_param (tok))
		    gi->debug_print (nonparam ("font name", s));
		  else
		    gi->set_default_font (param_contents(tok));
		}
	      // SENSITIVE?
	      else if (tok == "fontsize")
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
	      tok = next_token (s, tok_start, tok_end);
	      // SENSITIVE?
	      if (tok == "singleplayer")
		continue;
	      // SENSITIVE?
	      if (tok == "multiplayer")
		throw string ("Error: geas is single player only.");
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
      
      /* Quest "startitems <a; b; ...>": the player's initial inventory. */
      for (const auto &i: game.data)
	if (first_token (i, c1, c2) == "startitems")
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

      /* Run every startscript in order, not just the first: a game block may
	 carry more than one once a library has appended its own via `!addto game`
	 (e.g. the bundled type library's TLPstartup runs ahead of the game's own
	 startscript).  Stopping at the first would silently drop the game's
	 initialisation -- or the library's. */
      for (const auto &i: game.data)
	// SENSITIVE?
	if (first_token (i, c1, c2) == "startscript")
	  run_script_as ("game", i.substr (c2 + 1));

      run_script ("displaytext <intro>");

      regen_var_room ();
      regen_var_objects ();
      regen_var_dirs ();
      regen_var_look ();
      /* Quest enters the start room like any other (PlayGame): describe it,
       * then run its entry script -- which may immediately goto the real first
       * room (a "please wait" start room).  That nested goto describes the
       * destination, so there is no second describe here. */
      look();
      {
	string start_scr;
	if (get_obj_action (state.location, "script", start_scr))
	  run_script_as (state.location, start_scr);
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

void geas_implementation::regen_var_objects ()
{
  /* Bring container contents' hidden flags up to date first, so the pane (and
   * everything else that reads hidden) reflects open/closed/seen state. */
  update_container_visibility ();

  string tmp;
  vector <string> objs;
  for (const auto &i: state.objs)
    {
      /* List things in this room -- directly, or inside an open container/
       * surface here (room_of walks up the container chain). */
      if (ci_equal (room_of (i.name), state.location) &&
	  !get_obj_property (i.name, "hidden", tmp) &&
	  !get_obj_property (i.name, "invisible", tmp))
	  //!state.objs[i].hidden &&
	  //!state.objs[i].invisible)
	objs.push_back (i.name);
    }
  string qobjs = "", qfobjs = "";
  string objname, prefix, main, suffix, propval, print1, print2;
  for (uint i = 0; i < objs.size(); i ++)
    {
      objname = objs[i];
      if (!get_obj_property (objname, "alias", main))
	main = objname;
      print1 = main;
      print2 = "|b" + main + "|xb";
      if (get_obj_property (objname, "prefix", prefix))
	{
	  print1 = prefix + " " + print1;
	  print2 = prefix + " " + print2;
	}
      if (get_obj_property (objname, "suffix", suffix))
	{
	  print1 = print1 + " " + suffix;
	  print2 = print2 + " " + suffix;
	}
      qobjs = qobjs + print1;
      qfobjs = qfobjs + print2;
      if (i + 2 < objs.size())
	{
	  qobjs = qobjs + ", ";
	  qfobjs = qfobjs + ", ";
	}
      else if (i + 2 == objs.size())
	{
	  qobjs = qobjs + " and ";
	  qfobjs = qfobjs + " and ";
	}
    }
  set_svar ("quest.objects", qobjs);
  set_svar ("quest.formatobjects", qfobjs);
}

void geas_implementation::regen_var_room () 
{
  set_svar ("quest.currentroom", state.location);

  string tmp, formatroom;
  if (!get_obj_property (state.location, "alias", formatroom))
    formatroom = state.location;
  formatroom = "|cr" + formatroom + "|cb";
  if (get_obj_property (state.location, "prefix", tmp))
    formatroom = tmp + " " + formatroom;
  if (get_obj_property (state.location, "suffix", tmp))
    formatroom = formatroom + " " + tmp;  
  set_svar ("quest.formatroom", formatroom);

}


void geas_implementation::regen_var_look ()
{
  string look_tag;
  if (!get_obj_property (state.location, "look", look_tag))
    look_tag = "";
  set_svar ("quest.lookdesc", look_tag);
}


void geas_implementation::regen_var_dirs() 
{
  vector <string> dirs;
  // the -1 is so that it skips 'out'
  for (size_t i = 0; i < ARRAYSIZE (dir_names) - 1; i ++)
    if (exit_dest (state.location, dir_names[i]) != "")
      dirs.push_back (dir_names[i]);
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
  set_svar ("quest.doorways.dirs", exits);


   string out_dest = exit_dest (state.location, "out");
  if (out_dest == "")
    {
      set_svar ("quest.doorways.out", "");
      set_svar ("quest.doorways.out.display", "");
    }
  else
    {
      GEAS_DBG << "Updating quest.doorways.out; out_dest == {" << out_dest << "}";
      std::string::size_type i = out_dest.find (';');
      GEAS_DBG << ", i == " << i;
      string prefix = "";
      if (i != string::npos)
	{
	  /* "out <prefix; destination>" -- the prefix is everything before the
	   * semicolon, as in get_places (). */
	  prefix = trim (out_dest.substr (0, i));
	  out_dest = trim (out_dest.substr (i + 1));
	  GEAS_DBG << "; prefix == {" << prefix << "}, out_dest == {" << out_dest << "}";
	}
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
       * was not written for. */
      if (tmp == "")
	tmp = out_dest;
      tmp = (prefix != "" ? prefix + " " : "") + "|b" + tmp + "|xb";

      GEAS_DBG << ",    final value {" << tmp << "}" << endl;

      set_svar ("quest.doorways.out.display", tmp);
    }
   
  current_places = get_places (state.location);
  string printed_places = "";
  for (size_t i = 0; i < current_places.size(); i ++)
    {
      if (i == 0)
	printed_places = current_places[i][0];
      else if (i < current_places.size() - 1)
	printed_places = printed_places + ", " + current_places[i][0];
      else if (current_places.size() == 2)
	printed_places = printed_places + " or " + current_places[i][0];
      else
	printed_places = printed_places + ", or " + current_places[i][0];
    }
  set_svar ("quest.doorways.places", printed_places);
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

// TODO:  SENSITIVE???
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
      /* TODO: exactly in what order does it try synonyms?
       * Does it have to be flanked by whitespace?
       */
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
              if (first_token (line, c1, c2) == "game" &&
                  next_token (line, c1, c2) == "version" &&
                  is_param (tok = next_token (line, c1, c2)))
                {
                  banner += ", v";
                  banner += eval_param (tok);
                }
            }
  
          for (const string &line: gb->data)
            {
               if (first_token (line, c1, c2) == "game" &&
                   next_token (line, c1, c2) == "author" &&
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

void geas_implementation::run_command (const string &s1)
{
  string s = s1;
  /* if s == "restore" or "restart" or "quit" or "undo" */

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
  // TODO: does this get the original command, or the lowercased version?
  set_svar ("quest.originalcommand", s);
  s = substitute_synonyms (lcase(s));
  set_svar ("quest.command", s);

  /* "oops <correction>" (and the lenient "the <correction>") re-runs the last
   * command that failed on an unrecognised object word, with that word replaced
   * by the correction -- Quest's OOPS. */
  {
    string corr;
    bool is_oops = false;
    if (s.compare (0, 5, "oops ") == 0)
      { corr = trim (s.substr (5)); is_oops = true; }
    else if (oops_ready && s.compare (0, 4, "the ") == 0)
      { corr = trim (s.substr (4)); is_oops = true; }
    if (is_oops)
      {
	if (oops_ready && corr != "")
	  run_command (oops_before + corr + oops_after);
	else
	  print_formatted ("I don't understand your command. "
			   "Type HELP for a list of valid commands.");
	return;
      }
  }
  /* A fresh command: remember it for oops context, and clear any stale oops
   * state -- dereference_vars sets it again if this command fails on an
   * object word. */
  current_command = s;
  oops_ready = false;

  bool overridden = false;
  dont_process = false;

  /* beforeturn/afterturn hooks are pre-parsed per block (see GeasBlock); a room
   * override suppresses the game block's hooks (matching the original scans). */
  const GeasBlock *gb = gf.find_by_name ("room", state.location);
  if (gb != NULL)
    {
      gf.ensure_cached (*gb);
      for (const GeasBlock::hook_entry &h : gb->beforeturns)
	{
	  if (h.is_override)
	    overridden = true;
	  run_script_as (state.location, h.script);
	}
    }
  else
    gi->debug_print ("Unable to find block " + state.location + ".\n");

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
      if (try_match (s, false, false))
	{
	  /* TODO TODO */
	  // run after turn events ???
	}
      else
	display_error ("badcommand");
    }

  overridden = false;

  gb = gf.find_by_name ("room", state.location);
  if (gb != NULL)
    {
      gf.ensure_cached (*gb);
      for (const GeasBlock::hook_entry &h : gb->afterturns)
	{
	  if (h.is_override)
	    overridden = true;
	  run_script_as (state.location, h.script);
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

  if (state.running)
    { UndoState u = state.save_undo (); undo_buffer.push (u); }
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
      // SENSITIVE?
      if (ichar == input.length() || !c_equal_i (input[ichar], action[achar]))
	return match_rv ();
      ++ achar;
      ++ ichar;
    }
}

/* True if "text" occurs in "target" as a run of whole words, so a player can
 * name an object by part of its name: "rose" matches "Red Rose", "tondy"
 * matches "Head General Tondy", "board" matches "Notice board". */
static bool word_match (const string &text, const string &target)
{
  string x = lcase (trim (text)), t = lcase (target);
  if (x.empty())
    return false;
  std::string::size_type pos = 0;
  while ((pos = t.find (x, pos)) != string::npos)
    {
      bool left_ok = (pos == 0) || t[pos - 1] == ' ';
      std::string::size_type end = pos + x.length();
      bool right_ok = (end == t.length()) || t[end] == ' ';
      if (left_ok && right_ok)
	return true;
      ++ pos;
    }
  return false;
}

static bool match_object_alts (string text, const vector<string> &alts, bool is_internal)
{
  for (const string &i: alts)
    {
      GEAS_DBG << "m_o_a: Checking '" << text << "' v. alt '" << i << "'.\n";
      if (starts_with (text, i))
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


bool geas_implementation::match_object (const string &text, const string &name, bool is_internal, bool allow_partial) const
{
  GEAS_DBG << "* * * match_object (" << text << ", " << name << ", "
       << (is_internal ? "true" : "false") << ")\n";

  string alias, alt_list, prefix, suffix;

  if (is_internal && ci_equal (text, name)) return true;

  if (get_obj_property (name, "prefix", prefix) &&
      starts_with (text, prefix + " ") &&
      match_object (text.substr (prefix.length() + 1), name, false, allow_partial))
    return true;

  if (get_obj_property (name, "suffix", suffix) &&
      ends_with (text, " " + suffix) &&
      match_object (text.substr (0, text.length() - suffix.length() - 1), name, false, allow_partial))
    return true;

  if (!get_obj_property (name, "alias", alias))
    alias = name;
  if (ci_equal (text, alias))
    return true;
  /* Partial matching: accept any whole-word run of the alias or name, so the
   * player can refer to an object by part of its name ("rose" -> "Red Rose").
   * Only used as a fallback (see get_obj_name) so exact matches win. */
  if (allow_partial && (word_match (text, alias) || word_match (text, name)))
    return true;

  const GeasBlock *gb = gf.find_by_name ("object", name);
  if (gb != NULL)
    {
      std::string::size_type c1=0, c2;
      for (const string &line: gb->data)
	{
	  string tok = first_token (line, c1, c2);
	  // SENSITIVE?
	  if (tok == "alt")
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
  /* TODO */
  vector<string> where;
  where.push_back ("inventory");
  where.push_back (state.location);
  return dereference_vars (bindings, where, is_internal);
}

bool geas_implementation::dereference_vars (vector<match_binding> &bindings, const vector<string> &where, bool is_internal) const
{
  bool rv = true;
  for (auto &binding: bindings)
    if (binding.var_name[0] == '@')
      {
	/* Resolve pronouns ("it", "them", ...) to the last object referenced. */
	string lc = lcase (trim (binding.var_text));
	string obj_name;
	if (last_object != "" &&
	    (lc == "it" || lc == "them" || lc == "they" ||
	     lc == "him" || lc == "her"))
	  obj_name = last_object;
	else
	  obj_name = get_obj_name (binding.var_text, where, is_internal);
	if (obj_name == "!")
	  {
	    print_formatted ("You don't see any " + binding.var_text + ".");
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
   * matches. */
  for (int pass = 0; pass < 2 && objs.empty(); pass ++)
    {
      bool allow_partial = (pass == 1);
      for (size_t objnum = 0; objnum < state.objs.size(); objnum ++)
	{
	  bool is_used = false;
	  for (auto &loc: where)
	    {
	      // SENSITIVE?
	      if (loc == "game" || state.objs[objnum].parent == loc)
		is_used = true;
	    }
	  /* An object inside a reachable, open container is reachable too; its own
	   * "hidden" flag (meaning "inside the container") is then ignored. */
	  bool via_container = false;
	  if (!is_used &&
	      container_in_scope (state.objs[objnum].parent, where))
	    is_used = via_container = true;
	  if (is_used &&
	      (via_container || !has_obj_property (state.objs[objnum].name, "hidden")) &&
	      match_object (name, state.objs[objnum].name, is_internal, allow_partial))
	    {
	      string printed_name, tmp, oname = state.objs[objnum].name;
	      objs.push_back (oname);
	      if (!get_obj_property (oname, "alias", printed_name))
		printed_name = oname;
	      if (get_obj_property (oname, "detail", tmp))
		printed_name = tmp;
	      printed_objs.push_back (printed_name);
	    }
	}
    }
  GEAS_DBG << "objs == " << objs << ", printed_objs == " << printed_objs << "\n";
  if (objs.size() > 1)
    {
      uint num = 0;
      num = gi->make_choice ("Which " + name + " do you mean?", printed_objs);
			     
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


bool geas_implementation::run_commands (string cmd, const GeasBlock *room, bool is_internal)
{
  match_rv match;

  if (room == NULL)
    {
      gi->debug_print ("room is null\n");
      return false;
    }

  /* Commands and their split patterns are pre-parsed once (see GeasBlock /
   * ensure_cached); we no longer re-tokenize every line of the block each turn.
   * Order is preserved, so file-order priority (game commands before appended
   * lib commands) is unchanged. */
  gf.ensure_cached (*room);
  for (const GeasBlock::cmd_entry &e : room->commands)
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

bool geas_implementation::try_match (string cmd, bool is_internal, bool is_normal)
{

  string tok;
  match_rv match;

  if (!is_normal)
    {
      if (run_commands (cmd, gf.find_by_name ("room", state.location)) ||
	  run_commands (cmd, gf.find_by_name ("game", "game")))
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
	    /* Opening or closing a container discovers it (seen gate), even when
	     * the game scripts its own open/close action. */
	    if (key == "open" || key == "close")
	      set_obj_property (obj, "seen");
	    /* Prefer an action/property named by the typed verb (objects define
	     * e.g. action <search>, action <wear>), then the canonical key. */
	    if (get_obj_action (obj, ph, script) ||
		get_obj_action (obj, v.key, script))
	      run_script_as (obj, script);
	    else if (get_obj_property (obj, ph, script) ||
		     get_obj_property (obj, v.key, script))
	      print_formatted (script);
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
	    else if (key == "look in" &&
		     (has_obj_property (obj, "container") ||
		      has_obj_property (obj, "surface")))
	      {
		/* Look inside a container/surface with no custom handler: list its
		 * contents if reachable, else its "list closed" text.  Looking in
		 * discovers it (seen gate). */
		set_obj_property (obj, "seen");
		bool open_ = has_obj_property (obj, "surface") ||
		  container_is_open (obj) || has_obj_property (obj, "transparent");
		string lc;
		if (!open_ && get_obj_property (obj, "list closed", lc))
		  print_formatted (lc);
		else if (!open_)
		  print_formatted ("It is closed.");
		else
		  {
		    vector<string> c = container_contents (obj);
		    string m;
		    for (size_t i = 0; i < c.size (); i++)
		      m += (i == 0 ? "Inside you see " :
			    (i + 1 == c.size () ? " and " : ", ")) + c[i];
		    print_formatted (c.empty () ? "It is empty." : m + ".");
		  }
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
	    /* Opening/closing a container records its state even when a custom
	     * action handled the verb, so a scripted "open <msg>" still updates
	     * availability of the contents. */
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

      string object = match.bindings[0].var_text;

      set_obj_property (object, "seen");   /* discovered: gates container contents */
      if (!dispatch_obj_verb (object, "look"))
	display_error ("defaultlook", object);

      return true;
    }

  if ((match = match_command (cmd, "examine #@object#")) ||
      (match = match_command (cmd, "x #@object#")))
    {
      if (!dereference_vars (match.bindings, is_internal))
	return true;

      string object = match.bindings[0].var_text;
      set_obj_property (object, "seen");
      /* examine, falling back to look. */
      if (!dispatch_obj_verb (object, "examine") &&
	  !dispatch_obj_verb (object, "look"))
	display_error ("defaultexamine", object);
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

  if ((match = match_command (cmd, "give #@first# to #@second#")))
    {
      if (!dereference_vars (match.bindings, is_internal))
	return true;
      string script, first = match.bindings[0].var_text, second = match.bindings[1].var_text;
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
	  if (!get_obj_property (second, "gender", tmp))
	    tmp = "it";
	  set_svar ("quest.error.gender", tmp);
	  if (!get_obj_property (first, "article", tmp))
	    tmp = "it";
	  set_svar ("quest.error.article", tmp);
	  display_error ("itemunwanted");
	}
      return true;
    }
   
  /* Quest's standard "remove" verb, e.g. "remove book of keys from odd book
   * shelf".  Run the named object's "remove" action/property; for the
   * "X from Y" form the message is usually attached to the container Y (where
   * the game hangs it), so fall back to Y when X has no "remove" of its own. */
  if ((match = match_command (cmd, "remove #@first# from #@second#")))
    {
      if (!dereference_vars (match.bindings, is_internal))
	return true;
      string first = match.bindings[0].var_text,
	second = match.bindings[1].var_text;
      /* The "remove" message usually hangs on the container Y, so fall back
       * to it when the named item X has no "remove" of its own. */
      if (!dispatch_obj_verb (first, "remove") &&
	  !dispatch_obj_verb (second, "remove"))
	display_error ("defaultverb", first);
      return true;
    }
  if ((match = match_command (cmd, "remove from #@object#")) ||
      (match = match_command (cmd, "remove #@object#")))
    {
      if (!dereference_vars (match.bindings, is_internal))
	return true;
      string obj = match.bindings[0].var_text;
      if (!dispatch_obj_verb (obj, "remove"))
	display_error ("defaultverb", obj);
      return true;
    }

  if ((match = match_command (cmd, "use #@first# on #@second#")) ||
      (match = match_command (cmd, "use #@first# with #@second#")))
    {
      if (!dereference_vars (match.bindings, is_internal))
	return true;
      string script, first = match.bindings[0].var_text, second = match.bindings[1].var_text;
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
      else if (get_obj_action (second, "use anything", script))
	{
	  set_svar ("quest.use.object", first);
	  run_script_as (second, script);  /* was run_script(second, script),
	    which executed the object name instead of the action. */
	}
      else if (get_obj_action (first, "use on anything", script))
	{
	  set_svar ("quest.use.object", second);
	  run_script_as (first, script);
	}
      else
	display_error ("defaultuse");

      return true;
    }

  if ((match = match_command (cmd, "use #@first#")))
    {
      if (!dereference_vars (match.bindings, is_internal))
	return true;
      string tmp, obj = match.bindings[0].var_text;
      if (!is_held (obj))
	display_error ("noitem", obj);
      /* A "use <obj>" handler on the current room overrides the object's own
       * use action (Quest semantics): e.g. a key whose generic use says "no
       * door" still unlocks the door in the room that defines use <key>.
       * Mirror the "use X on Y" path, which already checks the target's
       * "use X" action. */
      else if (get_obj_action (state.location, "use " + obj, tmp))
	run_script_as (state.location, tmp);
      else if (!dispatch_obj_verb (obj, "use"))
	display_error ("defaultuse", obj);
      return true;
    }

  /* Quest's standard "put #object# in/on #object#" (place into a container or
   * onto a surface).  A target object can intercept with a `put <item>` (or a
   * catch-all `put anything`) action; otherwise, if it is a container/surface,
   * the item is actually moved into it (its parent becomes the target), so the
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
	string script, first = match.bindings[0].var_text,
	  second = match.bindings[1].var_text;
	if (!is_held (first))
	  display_error ("noitem", first);
	else if (get_obj_action (second, "put " + first, script))
	  run_script_as (second, script);
	else if (get_obj_action (second, "put anything", script))
	  {
	    set_svar ("quest.put.object", first);
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
	else if (has_obj_property (second, "surface") ||
		 (has_obj_property (second, "container") && container_is_open (second)))
	  {
	    /* Default: relocate the item into/onto the target (real
	     * containment), then report it.  Mark the target seen so the item
	     * just placed in it is immediately referenceable (the seen gate). */
	    move (first, second);
	    set_obj_property (second, "seen");
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
      if (!dereference_vars (match.bindings, is_internal))
	return true;

      string object = match.bindings[0].var_text;
      /* Already carrying it? (Some objects are placed straight into the
       * inventory by other actions, so a later "take" should not fail.) */
      if (is_held (object))
	{
	  print_formatted ("You already have it.");
	  return true;
	}
      if (get_obj_action (object, "take", tok))
	{
	  GEAS_DBG << "Running script '" << tok << "' for take " << object << endl;
	  run_script_as (object, tok);
	}
      else if (get_obj_property (object, "take", tok))
	{
	  GEAS_DBG << "Found property '" << tok << "' for take " << object << endl;
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
	    run_script (object, tmp);
	  else if (get_obj_property (object, "gain", tmp))
	    print_formatted (tmp);
	}
      else
	{
	  GEAS_DBG << "No match found for take " << object << endl;
	  // TODO set variable with object name
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
      if (!dereference_vars (match.bindings, is_internal))
	return true;
      string scr, obj = match.bindings[0].var_text;
      /* If the object is inside a container/surface, take it out first -- Quest
       * does this implicitly when you drop something still in a container. */
      {
	string p = obj_parent (obj);
	if (p != "" && !ci_equal (p, state.location) && !ci_equal (p, "inventory") &&
	    (has_obj_property (p, "container") || has_obj_property (p, "surface")))
	  {
	    string odisp, pdisp;
	    if (!get_obj_property (obj, "article", odisp)) odisp = obj;
	    if (!get_obj_property (p, "alias", pdisp)) pdisp = p;
	    print_formatted ("(first removing " + odisp + " from " + pdisp + ")");
	    move (obj, "inventory");
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
	      // SENSITIVE?
	      if (tok == "drop")
		{
		  script_begins = c2;
		  tok = next_token (line, c1, c2);
		  // SENSITIVE?
		  if (tok == "everywhere")
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
		  // SENSITIVE?
		  if (tok == "nowhere")
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
      string script;
      if (get_obj_action (obj, "speak", script))
	run_script_as (obj, script);
      else
	display_error ("defaultspeak", obj);
      return true;
    }
        
  if (cmd == "exit" || cmd == "out" || cmd == "go out") 
    {
      const GeasBlock *gb = gf.find_by_name ("room", state.location);
      if (gb == NULL) 
	{
	  gi->debug_print ("Bad room");
	  return true;
	}
      string line = "", tok;
      std::string::size_type c1, c2=0;
      // TODO: Use the first matching line or the last?
      for (size_t i = 0; i < gb->data.size(); i ++)
	{
	  if (first_token (gb->data[i], c1, c2) == "out")
	    line = gb->data[i];
	}


      if (line == "")
	display_error ("defaultout");
      else
	{
	  c1 = line.find ('<');
	  if (c1 != string::npos)
	    c2 = line.find ('>', c1);

	  if (c1 == string::npos || c2 == string::npos)
	    gi->debug_print ("Bad out line: " + line);
	  else
	    {
	      string tmp = trim (line.substr (c2 + 1));
	      if (tmp != "")
		run_script_as (state.location, tmp);
	      else
		{
		  tmp = line.substr (c1, c2-c1+1);
		  if (!is_param (tmp)) report_unsupported ("expected parameter in goto: " + line);
		  tmp = param_contents (tmp);
		  c1 = tmp.find (';');
		  if (c1 == string::npos)
		    goto_room (trim (tmp));
		  else
		    goto_room (trim (tmp.substr (c1+1)));
		}
	    }
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
	    // TODO Which display_error do I use?
	    print_formatted ("You can't go that way.");
	    return true;
	  }
	if (!is_script && exit_locked (state.location, dir_names[i]))
	  {
	    string lm = exit_lock_message (state.location, dir_names[i]);
	    print_formatted (lm != "" ? lm : "The way is locked.");
	    return true;
	  }
	if (is_script)
	  run_script_as (state.location, tok);
	else 
	  {
	    std::string::size_type index = tok.find (';');
	    if (index == string::npos)
	      goto_room (trim (tok));
	    else
	      goto_room (trim (tok.substr(index+1)));
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

  if (ci_equal (cmd, "inventory") || ci_equal (cmd, "i"))
    {
      vector<vector<string> > inv = get_inventory();
      if (inv.size() == 0)
	print_formatted ("You are carrying nothing.");
      else
	print_formatted ("You are carrying:");
      for (const auto &i: inv)
	{
	  print_normal (i[0]);
	  print_newline();
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
   * built-in multi-synonym verbs the object responds to, its own action
   * definitions, and any global `verb` declaration it handles. */
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
	  // SENSITIVE?
	  if (first_token (line, d1, d2) == "game" &&
	      next_token (line, d1, d2) == key &&
	      is_param (t = next_token (line, d1, d2)))
	    print_formatted (label + eval_param (t));
      };
      emit ("version",   "Version ");
      emit ("author",    "Author: ");
      emit ("copyright", "Copyright: ");
      emit ("info",      "");

      return true;
    }
  
  if (ci_equal (cmd, "quit"))
    {
      is_running_ = false;
      return true;
    }

  /* Game-scope verb declarations.  Quest writes these as either
   *   verb <name[;synonym;...]> <default script>     (asl source) or
   *   <name[;synonym;...]> <default script>           (compiled form),
   * optionally prefixed with "lib" for a library-supplied verb.  They
   * register custom verbs (e.g. "destroy", "mount") with a default response;
   * an object overrides via action <name> or properties <name=...>.  Checked
   * last, so the built-in verbs above keep their normal behaviour and this
   * only handles otherwise-unrecognised verbs. */
  {
    const GeasBlock *game = gf.find_by_name ("game", "game");
    if (game != NULL)
      for (const string &line: game->data)
	{
	  std::string::size_type d1, d2;
	  string tok = first_token (line, d1, d2);
	  if (tok == "lib")                  /* optional library-verb prefix */
	    tok = next_token (line, d1, d2);
	  string names_tok;
	  if (tok == "verb")
	    names_tok = next_token (line, d1, d2);
	  else if (is_param (tok))           /* bare "<name> <script>" form */
	    names_tok = tok;
	  else
	    continue;
	  if (!is_param (names_tok))
	    continue;
	  string deflt = trim (line.substr (d2));        /* default script */
	  vector<string> names = split_param (param_contents (names_tok));
	  if (names.empty() || trim (names[0]) == "")
	    continue;
	  string key = trim (names[0]);   /* property/action key (first name) */
	  for (const string &nm: names)
	    {
	      string verbname = trim (nm);
	      if (verbname == "")
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
		  else if (deflt != "")
		    run_script_as (obj, deflt);
		  return true;
		}
	    }
	}
  }

  return false;
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

  if (tok == "action")
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
  else if (tok == "animate")
    {
      return;   /* recognised, nothing to do -- not "unrecognised script" */
    }
  else if (tok == "background")
    {
      tok = next_token (s, c1, c2);
      if (is_param (tok))
	gi->set_background (eval_param (tok));
      else
	gi->debug_print ("Expected parameter after foreground in " + s);
      return;
    }
  else if (tok == "lock" || tok == "unlock")
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
  else if (tok == "select")
    {
      /* Quest "select case <expr> do <!intproc>": the reader deinlines the
       * select-case block into a procedure whose lines are
       *   case <v1;v2;...> <script>     (script may itself be "do <!intproc>")
       *   case else <script>
       * Run the script of the first case whose (semicolon-separated) value
       * equals the selector; "case else" matches anything. */
      tok = next_token (s, c1, c2);
      if (tok != "case")
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
      for (uint i = 0; i < gf.size ("procedure"); i ++)
	if (ci_equal (gf.block ("procedure", i).name, procname))
	  {
	    const GeasBlock &proc = gf.block ("procedure", i);
	    for (uint j = 0; j < proc.data.size (); j ++)
	      {
		std::string::size_type d1, d2;
		string line = proc.data[j];
		if (first_token (line, d1, d2) != "case")
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
      gi->debug_print ("No case block " + procname + " found");
      return;
    }
  else if (tok == "choose")
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
	  // SENSITIVE?
	  if (tok == "info")
	    {
	      tok = next_token (line, c1, c2);
	      if (is_param (tok))
		question = eval_param (tok);
	      else
		gi->debug_print ("Expected parameter after info in " + line);
	    }
	  // SENSITIVE?
	  else if (tok == "choice")
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
  else if (tok == "clear")
    {
      gi->clear_screen();
      return;
    }
  else if (tok == "clone")
    {
      /* TODO */
      return;   /* recognised, nothing to do -- not "unrecognised script" */
    }
  else if (tok == "create")
    {
      tok = next_token (s, c1, c2);
      // SENSITIVE?
      if (tok == "exit") // create exit
	{
	  string dir = "";

	  tok = next_token (s, c1, c2);
	  if (!is_param (tok))
	    {
	      dir = tok;
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
	  if (dir != "")
	    state.exits.push_back (ExitRecord (args[0], 
					       "exit "+dir+" <"+tok+">"));
	  else
	    state.exits.push_back (ExitRecord (args[0], "exit <" + tok + ">"));
	  regen_var_dirs();
	  return;
	}
      // SENSITIVE?
      else if (tok == "object") // create object
	{
	  /* TODO */
	}
      // SENSITIVE?
      else if (tok == "room") // create room
	{
	  /* TODO */
	}
      else
	gi->debug_print ("Bad create line " + s);
      return;
    }
  else if (tok == "debug")
    {
      tok = next_token (s, c1, c2);
      if (is_param (tok))
	gi->debug_print (eval_param(tok));
      else
	gi->debug_print ("Expected param after debug in " + s);
      return;
    }
  else if (tok == "destroy")
    {
      tok = next_token (s, c1, c2);
      if (tok != "exit")
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
  else if (tok == "disconnect")
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
      state.exits.push_back (ExitRecord (args[0], "noexit " + args[1]));
      regen_var_dirs ();
      return;
    }
  else if (tok == "displaytext")
    {
      tok = next_token (s, c1, c2);
      if (!is_param(tok))
	{
	  gi->debug_print ("Expected parameter after displaytext in " + s);
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
	    print_formatted (gb->data[i]);
	  print_newline();
	}
      else
	gi->debug_print ("No such text block " + tok);
      return;
    }
  else if (tok == "do")
    {
      tok = next_token (s, c1, c2);
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
  else if (tok == "doaction")
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
  else if (tok == "dontprocess")
    {
      dont_process = true;
      return;
    }
  else if (tok == "enter")
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected parameter after enter in " + s);
	  return;
	}
      tok = eval_param (tok);
      set_svar (tok, gi->get_string());
      return;
    }
  else if (tok == "exec")
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
	  string tmp = trim (tok.substr (index+1));
	  // SENSITIVE?
	  if (tmp == "normal")
	    {
	      try_match (trim (tok.substr (0, index)), true, true);
	    }
	  else
	    {
	      gi->debug_print ("Bad " + tmp + " in exec in " + s);
	      try_match (trim (tok.substr (0, index)), true, false);
	    }
	}
      else
	{
	  try_match (trim (tok.substr (0, index)), true, false);
	}
      return;
    }
  else if (tok == "flag")
    {
      tok = next_token (s, c1, c2);
      bool is_on;
      // SENSITIVE?
      if (tok == "on")
	is_on = true;
      // SENSITIVE?
      else if (tok == "off")
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
  else if (tok == "font")
    {
      /* TODO */
      return;   /* recognised, nothing to do -- not "unrecognised script" */
    }
  else if (tok == "for")
    {
      tok = next_token (s, c1, c2);
      // SENSITIVE?
      if (tok == "each")
	{
	  // SENSITIVE?
	  if (next_token (s, c1, c2) == "object" &&
	      next_token (s, c1, c2) == "in")
	    {
	      tok = next_token (s, c1, c2);
	      // SENSITIVE?
	      if (tok == "game")
		{
		  /* TODO: This will run over the game, rooms, and objects */
		  /* It should just do the objects. */
		  string script = s.substr (c2);
		  // Start at 1 to skip game
		  for (const auto &i: state.objs)
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
		  string script = s.substr (c2);
		  for (const auto &i: state.objs)
		    if (i.parent == tok)
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
  else if (tok == "foreground")
    {
      tok = next_token (s, c1, c2);
      if (is_param (tok))
	gi->set_foreground (eval_param (tok));
      else
	gi->debug_print ("Expected parameter after foreground in " + s);
      return;
    }
  else if (tok == "give")
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
       * separately from objects. */
      bool have_item = false;
      for (const string &it: state.items)
	if (ci_equal (it, tok)) { have_item = true; break; }
      if (!have_item)
	state.items.push_back (tok);
      /* If a real object of this name exists, also move it to the inventory
       * (3.x-style give) and run its gain action. */
      bool is_object = false;
      for (const auto &o: state.objs)
	if (ci_equal (o.name, tok)) { is_object = true; break; }
      if (is_object)
	{
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
  else if (tok == "goto")
    {
      tok = next_token (s, c1, c2);
      if (is_param(tok))
	goto_room (trim (eval_param (tok)));
      else
	gi->debug_print ("Expected parameter after goto in " + s);
      return;
    }
  else if (tok == "helpclear")
    {
      return;   /* recognised, nothing to do -- not "unrecognised script" */
    }
  else if (tok == "helpclose")
    {
      return;   /* recognised, nothing to do -- not "unrecognised script" */
    }
  else if (tok == "helpdisplaytext")
    {
      return;   /* recognised, nothing to do -- not "unrecognised script" */
    }
  else if (tok == "helpmsg")
    {
      return;   /* recognised, nothing to do -- not "unrecognised script" */
    }
  // SENSITIVE?
  /* "hideobject"/"hidechar" are the Quest 2.x spellings of "hide". */
  else if (tok == "hide" || tok == "hideobject" || tok == "hidechar")
    {
      tok = next_token (s, c1, c2);
      if (is_param(tok))
	{
	  string name = eval_param (tok);
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
	gi->debug_print ("Expected param after conceal in " + s);
      return;
    }
  // SENSITIVE?
  /* "showobject"/"showchar" are the Quest 2.x spellings of "show". */
  else if (tok == "show" || tok == "showobject" || tok == "showchar")
    {
      tok = next_token (s, c1, c2);
      if (is_param(tok))
	set_obj_property (eval_param (tok), "not hidden");
      else
	gi->debug_print ("Expected param after conceal in " + s);
      return;
    }
  else if (tok == "if")
    {
      std::string::size_type begin_cond = c2 + 1, end_cond, begin_then, end_then;

      do {
	tok = next_token (s, c1, c2);
	// SENSITIVE?
      } while (tok != "then" && tok != "");
      
      if (tok == "")
	{
	  gi->debug_print ("Expected then in if: " + s);
	  return;
	}
      end_cond = c1;
      string cond_str = s.substr (begin_cond, end_cond - begin_cond);

      begin_then = c2 + 1;
      int brace_count = 0;
      do {
	tok = next_token (s, c1, c2);
	for (uint i = 0; i < tok.length(); i ++)
	  if (tok[i] == '{')
	    brace_count ++;
	  else if (tok[i] == '}')
	    brace_count --;
	// SENSITIVE?
      } while (tok != "" && !(brace_count == 0 && tok == "else"));
      end_then = c1;

      
      if (eval_conds (cond_str))
	run_script (s.substr (begin_then, end_then - begin_then), rv);
      else if (c2 < s.length())
	run_script (s.substr (c2), rv);
      return;
    }
  else if (tok == "inc" || tok == "dec")
    {
      // SENSITIVE?
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
      if (is_dec)
	set_ivar (varname, get_dvar (varname) - diff);
      else
	set_ivar (varname, get_dvar (varname) + diff);
      return;
    }
  else if (tok == "lose")
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected parameter after lose in " + s);
	  return;
	}
      tok = eval_param (tok);

      /* Remove from the Quest 2.x item inventory. */
      for (auto it = state.items.begin(); it != state.items.end(); )
	if (ci_equal (*it, tok))
	  it = state.items.erase (it);
	else
	  ++ it;

      /* If a real object of this name was being carried, drop it to the room. */
      bool is_object = false;
      for (const auto &o: state.objs)
	if (ci_equal (o.name, tok)) { is_object = true; break; }
      if (is_object && ci_equal (get_obj_parent (tok), "inventory"))
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
  else if (tok == "mailto")
    {
      return;   /* recognised, nothing to do -- not "unrecognised script" */
    }
  else if (tok == "modvolume")
    {
      return;   /* recognised, nothing to do -- not "unrecognised script" */
    }
  // SENSITIVE?
  /* "movechar"/"moveobject" are the Quest 2.x spellings of "move". */
  else if (tok == "move" || tok == "movechar" || tok == "moveobject")
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
  else if (tok == "msg")
    {
      tok = next_token (s, c1, c2);
      if (is_param (tok))
	print_eval (param_contents(tok));
      else
	gi->debug_print ("Expected parameter after msg in " + s);
      return;
    }
  else if (tok == "msgto")
    {
      /* QNSO */
      return;   /* recognised, nothing to do -- not "unrecognised script" */
    }
  else if (tok == "outputoff")
    {
      outputting = false;
      return;
    }
  else if (tok == "outputon")
    {
      outputting = true;
      return;
    }
  else if (tok == "panes")
    {
      /* TODO */
      return;   /* recognised, nothing to do -- not "unrecognised script" */
    }
  else if (tok == "pause")
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Expected parameter after pause in " + s);;
	  return;
	}
      int i = (int) eval_double (param_contents(tok));
      gi->pause (i);
      return;
    }
  else if (tok == "picture")
    {
      /* picture <file[@WxH]>  -- display an image (the host loads the file
       * itself).  Quest allows an optional "@<width>x<height>" suffix giving
       * the display size; split it off so the host opens the real filename and
       * receives the requested resolution separately. */
      tok = next_token (s, c1, c2);
      if (is_param (tok))
	{
	  string spec = eval_param (tok), file = spec, res;
	  std::string::size_type at = spec.find ('@');
	  if (at != string::npos)
	    {
	      file = trim (spec.substr (0, at));
	      res = trim (spec.substr (at + 1));
	    }
	  gi->show_image (file, res, "");
	}
      return;
    }
  else if (tok == "playerlose")
    {
      run_script ("displaytext <lose>");
      state.running = false;
      is_running_ = false;   /* end the game so the host stops prompting */
      return;
    }
  else if (tok == "playerwin")
    {
      run_script ("displaytext <win>");
      state.running = false;
      is_running_ = false;   /* end the game so the host stops prompting */
      return;
    }
  // SENSITIVE?
  else if (tok == "playwav" || tok == "playmidi" || tok == "playmod")
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
      return;
    }
  else if (tok == "property")
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
  else if (tok == "repeat")
    {
      /* TODO TODO: assumes script is a "do ..." */
      tok = next_token (s, c1, c2);
      // SENSITIVE?
      if (tok != "while" && tok != "until")
	{
	  gi->debug_print ("Expected while or until after repeat in " + s);
	  return;
	}
      bool is_while = (tok == "while");
      std::string::size_type start_cond = c2, end_cond = string::npos;
      while ((tok = next_token (s, c1, c2)) != "")
	{
	  // SENSITIVE?
	  if (tok == "do")
	    {
	      end_cond = c1;
	      break; // TODO: Do I break here?
	    }
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
      while (eval_conds (cond) == is_while)
	run_script(script);
      return;
    }
  else if (tok == "return")
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
  else if (tok == "reveal" || tok == "revealobject" || tok == "revealchar")
    {
      tok = next_token (s, c1, c2);
      if (is_param(tok))
	set_obj_property (eval_param (tok), "not invisible");
      else
	gi->debug_print ("Expected param after reveal in " + s);
      return;
    }
  else if (tok == "conceal" || tok == "concealobject" || tok == "concealchar")
    {
      tok = next_token (s, c1, c2);
      if (is_param(tok))
	set_obj_property (eval_param (tok), "invisible");
      else
	gi->debug_print ("Expected param after conceal in " + s);
      return;
    }
  else if (tok == "say")
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
  else if (tok == "set")
    {
      string vartype = "";
      tok = next_token (s, c1, c2);
      // SENSITIVE?
      if (tok == "interval")
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

	  for (TimerRecord &i: state.timers)
	    if (i.name == timer_name)
	      {
		i.interval = time_val;
		return;
	      }
	  gi->debug_print ("no interval named " + timer_name + " found!");
	  return;
	}
      // SENSITIVE?
      if (tok == "string" || tok == "numeric")
	{
	  vartype = tok;
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
	  for (const auto &varn: state.ivars)
	    {
	      if (varn.name == varname)
		{
		  vartype = "numeric";
		  break;
		}
	    }
	  if (vartype == "")
	    {
	      for (const auto &varn: state.svars)
		{
		  if (varn.name == varname)
		    {
		      vartype = "string";
		      break;
		    }
		}
	    }
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
	  set_ivar (varname, eval_double(tok.substr (index+1)));
	}
      return;
    }
  else if (tok == "setstring")
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
  else if (tok == "setvar")
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
      set_ivar (varname, eval_double(tok.substr (index+1)));
      return;
    }
  else if (tok == "shell")
    {
      return;   /* recognised, nothing to do -- not "unrecognised script" */
    }
  else if (tok == "shellexe")
    {
      return;   /* recognised, nothing to do -- not "unrecognised script" */
    }
  else if (tok == "speak")
    {
      tok = next_token (s, c1, c2);
      if (is_param(tok))
	gi->speak (eval_param (tok));
      else
	gi->debug_print ("Expected param after speak in " + s);
      return;
    }
  else if (tok == "stop")
    {
      state.running = false;
      return;
    }
  else if (tok == "timeron" || tok == "timeroff")
    {
      // SENSITIVE?
      bool running = (tok == "timeron");
      //tok = lcase (next_token (s, c1, c2));
      tok = next_token (s, c1, c2);
      if (is_param (tok))
	{
	  tok = eval_param (tok);
	  for (auto &timer: state.timers)
	    if (timer.name == tok)
	      {
		if (running)
		  timer.timeleft = timer.interval;
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
  else if (tok == "type")
    {
      /* TODO */
      return;   /* recognised, nothing to do -- not "unrecognised script" */
    }
  else if (tok == "wait")
    {
      tok = next_token (s, c1, c2);
      if (tok != "")
	{
	  if (!is_param(tok))
	    {
	      gi->debug_print ("Expected parameter after wait in " + s);
	      return;
	    }
	  tok = eval_param (tok);
	}
      gi->wait_keypress (tok);
      /* Quest clears the screen once the player presses a key; clear_screen
       * emits a blank-line separator (see GeasInterface::clear_screen). */
      gi->clear_screen ();
      return;
    }
  else if (tok == "with")
    {
      // QNSO
      return;   /* recognised, nothing to do -- not "unrecognised script" */
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
	// SENSITIVE?
	if (args[i] == "report")
	  do_report = true;
	else
	  gi->debug_print ("Got modifier " + args[i] + " after exists");
      //args[0] = lcase (args[0]);
      for (uint i = 0; i < state.objs.size(); i ++)
	if (ci_equal (state.objs[i].name, args[0]))
	  /* An object "exists" (in the Quest sense the games rely on) only if it
	   * is placed AND not hidden.  A statically-defined object can sit in a
	   * room while flagged hidden until a show command reveals it; checking
	   * parent alone made such reveals dead code.  Note: only "hidden" is
	   * toggled by show/hide -- "invisible" (which merely suppresses room
	   * listing) must NOT gate existence, or objects revealed via show while
	   * still flagged invisible would wrongly read as nonexistent. */
	  return state.objs[i].parent != ""
	    && !has_obj_property (state.objs[i].name, "hidden");
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
      //tok = lcase (trim (eval_param (tok)));
      tok = trim (eval_param (tok));
      /* Quest 2.x item inventory. */
      for (const string &it: state.items)
	if (ci_equal (it, tok))
	  return true;
      for (uint i = 0; i < state.objs.size(); i ++)
	if (ci_equal (state.objs[i].name, tok))
	  return ci_equal (state.objs[i].parent, "inventory");
      gi->debug_print ("No object " + tok + " found while evaling " + s);
      return false;
    }
  else if (tok == "here")
    {
      tok = next_token (s, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("expected parameter after here in " + s);
	  return false;
	}
      //tok = lcase (trim (eval_param (tok)));
      tok = trim (eval_param (tok));
      for (uint i = 0; i < state.objs.size(); i ++)
	if (ci_equal (state.objs[i].name, tok))
	  /* "here" means present in this room AND not hidden -- present either
	   * directly or inside an open container/surface here (room_of walks the
	   * container chain).  An object removed with "hide" (e.g. a creature
	   * that has run off) is no longer here, even though its parent stands. */
	  return (ci_equal (room_of (state.objs[i].name), state.location) &&
		  !has_obj_property (state.objs[i].name, "hidden"));
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
      std::string::size_type index;
      // SENSITIVE?
      /* preprocess () rewrites "if (a <= b)" to the canonical "is <a;lt=;b>",
       * so the operator is always preceded by a ';' separator: the left-hand
       * side is substr (0, index - 1), dropping it.  The numeric branches below
       * can afford substr (0, index) because eval_double () stops at the
       * trailing ';', but a string compare cannot. */
      if ((index = tok.find ("!=;")) != string::npos)
	{
	  std::string::size_type index1 = index;
	  do
	    {
	      -- index1;
	    } while (index1 > 0 && tok[index1] != ';');

	  GEAS_DBG << "Comparing <" << trim_braces (trim (tok.substr (0, index1)))
	       << "> != <" << trim_braces (trim (tok.substr (index + 3)))
	       << ">\n";
	  return ci_notequal (trim_braces (trim (tok.substr (0, index - 1))),
			      trim_braces (trim (tok.substr (index + 3))));
	}
      if ((index = tok.find ("lt=;")) != string::npos)
	{
	  GEAS_DBG << "Comparing <" << trim_braces (trim (tok.substr (0, index)))
	       << "> < <" << trim_braces (trim (tok.substr (index + 4)))
	       << ">\n";
	  return eval_double (tok.substr (0, index - 1))
	    <= eval_double (tok.substr (index + 4));
	}
      if ((index = tok.find ("gt=;")) != string::npos)
	return eval_double (tok.substr (0, index))
	  >= eval_double (tok.substr (index + 4));
      if ((index = tok.find ("lt;")) != string::npos)
	return eval_double (tok.substr (0, index))
	  < eval_double (tok.substr (index + 3));
      if ((index = tok.find ("gt;")) != string::npos)
	return eval_double (tok.substr (0, index))
	  > eval_double (tok.substr (index + 3));
      if ((index = tok.find (";")) != string::npos)
	{
	  GEAS_DBG << "Comparing <" << trim_braces (trim (tok.substr (0, index)))
	       << "> == <" << trim_braces (trim (tok.substr (index + 1))) 
	       << ">\n";
	  return ci_equal (trim_braces (trim (tok.substr (0, index))),
			   trim_braces (trim (tok.substr (index + 1))));
	}
      gi->debug_print ("Bad is condition " + tok + " in " + s);
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
	// SENSITIVE?
	if (args[i] == "report")
	  do_report = true;
	else
	  gi->debug_print ("Got modifier " + args[i] + " after exists");
      //args[0] = lcase (args[0]);
      for (const auto &i: state.objs)
	if (ci_equal (i.name, args[0]))
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
      return gf.obj_of_type (args[0], args[1]);
    }

  gi->debug_print ("Bad condition " + s);
  return false;
}

void geas_implementation::run_procedure (const string &pname, vector<string> args)
{
  GEAS_DBG << "run_procedure " << pname << " (" << args << ")\n";
  vector<string> backup = function_args;
  function_args = args;
  run_procedure (pname);
  function_args = backup;
}

void geas_implementation::run_procedure (const string &pname)
{
  for (uint i = 0; i < gf.size ("procedure"); i ++)
    if (ci_equal (gf.block ("procedure", i).name, pname))
      {
	const GeasBlock &proc = gf.block ("procedure", i);
	for (uint j = 0; j < proc.data.size(); j ++)
	  {
	    run_script(proc.data[j]);
	  }
	return;
      }
  gi->debug_print ("No procedure " + pname + " found.");
}

string geas_implementation::run_function (const string &pname, vector<string> args)
{
  GEAS_DBG << "run_function (w/ args) " << pname << " (" << args << ")\n";
  /* Parameter is handled specially because it can't change the stack */
  // SENSITIVE?
  if (pname == "parameter")
    {
      if (args.size() != 1)
	{
	  gi->debug_print ("parameter called with " + string_int(args.size())
			   + " args");
	  return "";
	}
      uint num = parse_int (args[0]);
      if (0 < num && num <= function_args.size())
	{
	  GEAS_DBG << "   --> " << function_args[num-1] << "\n";
	  return function_args[num-1];
	}
      GEAS_DBG << "   --> too many arguments\n";
      return "";
    }
  vector<string> backup = function_args;
  function_args = args;
  for (size_t i = 0; i < args.size(); i ++)
    {
      set_svar ("quest.function.parameter." + string_int (i+1), args[i]);
    }
  string rv = run_function (pname);
  function_args = backup;
  return rv;
}

string geas_implementation::bad_arg_count (const string &fname)
{
  gi->debug_print ("Called " + fname + " with " + 
		   string_int(function_args.size()) + " arguments.");
  return "";
}

string geas_implementation::run_function (const string &pname)
{
  GEAS_DBG << "geas_implementation::run_function (" << pname << ", " << function_args << ")\n";
  //pname = lcase (pname);
  // SENSITIVE?
  if (pname == "getobjectname") 
    {
      if (function_args.size() == 0)
	return bad_arg_count (pname);
      vector<string> where;
      for (size_t i = 1; i < function_args.size(); i ++)
	{
	  where.push_back (function_args[i]);
	}
      if (where.size() == 0)
	{
	  where.push_back (state.location);
	  where.push_back ("inventory");
	}
      bool is_internal = false;
      return get_obj_name (function_args[0], where, is_internal);
    } 
  // SENSITIVE?
  else if (pname == "loadmethod")
    {
      /* "normal" for a fresh game, "loaded" once a save has been restored. */
      return load_method_;
    }
  // SENSITIVE?
  else if (pname == "locationof")
    { 
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      return get_obj_parent (function_args[0]);
    }
  // SENSITIVE?
  else if (pname == "objectproperty")
    {
      if (function_args.size() != 2)
	return bad_arg_count(pname);

      string rv;
      get_obj_property (function_args[0], function_args[1], rv);
      return rv;
    }
  // SENSITIVE?
  else if (pname == "timerstate")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      string timername = function_args[0];
      for (const auto &timer: state.timers)
	if (timer.name == timername)
	  return timer.is_running ? "1" : "0";
      return "!";
    }
  // SENSITIVE?
  else if (pname == "displayname")
    {
      if (function_args.size() != 1)
	return bad_arg_count (pname);
      
      return displayed_name (function_args[0]);
    }
  // SENSITIVE?
  else if (pname == "capfirst")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      return pcase (function_args[0]);
    }
  // SENSITIVE?
  else if (pname == "instr")
    {
      if (function_args.size() != 2 && function_args.size() != 3)
	return bad_arg_count(pname);

      std::string::size_type rv;
      if (function_args.size() == 2)
	rv = function_args[0].find (function_args[1]);
      else
	{
	  /* 3-arg form is instr(start, text, search); Quest's start is
	   * 1-based. */
	  int start = parse_int (function_args[0]);
	  if (start < 1)
	    start = 1;
	  rv = function_args[1].find (function_args[2], start - 1);
	}

      /* Quest's Instr is 1-based and returns 0 when the search string is not
       * found (it was returning npos, i.e. a huge bogus position). */
      if (rv == string::npos)
	return "0";
      else
	return string_int (rv + 1);
    }
  // SENSITIVE?
  else if (pname == "lcase")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      string rv = function_args[0];
      for (uint i = 0; i < rv.size(); i ++)
	rv[i] = tolower (rv[i]);
      return rv;
    }
  // SENSITIVE?
  else if (pname == "left")
    {
      if (function_args.size() != 2)
	return bad_arg_count(pname);
       
      uint i = parse_int (function_args[1]);
      if (i > function_args[0].length())
	return function_args[0];
      else
	return function_args[0].substr (0, i);
    }
  // SENSITIVE?
  else if (pname == "lengthof")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      return string_int (function_args[0].length());
    }
  // SENSITIVE?
  else if (pname == "mid")
    {
      /* Quest's Mid is 1-based (it is VB's), and its length argument is
       * optional: $mid(<s>; <start>)$ returns the rest of the string from
       * start.  The bundled type library uses both forms (TLFcontentFormat
       * scans a string with $mid(#s#;%n%;1)$ for n = 1..lengthof, then calls
       * the 2-argument form), so a 0-based start or a hard 3-argument
       * requirement silently corrupts its output. */
      if (function_args.size() != 2 && function_args.size() != 3)
	return bad_arg_count(pname);

      const string &str = function_args[0];
      int start = parse_int (function_args[1]);
      if (start < 1)
	start = 1;
      size_t pos = (size_t) (start - 1);
      if (pos >= str.length())
	return "";
      if (function_args.size() == 2)
	return str.substr (pos);
      int len = parse_int (function_args[2]);
      if (len <= 0)
	return "";
      if (pos + (size_t) len > str.length())
	return str.substr (pos);
      return str.substr (pos, (size_t) len);
    }
  // SENSITIVE?
  else if (pname == "right")
    {
      if (function_args.size() != 2)
	return bad_arg_count(pname);

      uint size = parse_int (function_args[1]);
      if (size > function_args[0].length())
	return function_args[0];
      return function_args[0].substr (function_args[0].length() - size);
    }
  else if (pname == "ubound")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      /* Highest defined index of an array variable (string or numeric).
       * Records carry index 0 plus every set index, so max() == size()-1. */
      for (const auto &i: state.svars)
	if (ci_equal (i.name, function_args[0]))
	  return string_int (i.max());
      for (const auto &i: state.ivars)
	if (ci_equal (i.name, function_args[0]))
	  return string_int (i.max());
      return "0";
    }
  // SENSITIVE?
  else if (pname == "round")
    {
      /* Quest "$round(<expr>; <decimals>)$": evaluate the (double) expression
       * and round to the given number of decimal places (default 0).  Format
       * without trailing zeros, matching .NET's double-to-string. */
      if (function_args.size () < 1)
	return bad_arg_count (pname);
      double v = eval_double (function_args[0]);
      int dec = (function_args.size () >= 2) ? parse_int (function_args[1]) : 0;
      if (dec < 0)
	dec = 0;
      double p = pow (10.0, dec);
      double r = (v < 0 ? -1.0 : 1.0) * floor (fabs (v) * p + 0.5) / p;
      char buf[64];
      snprintf (buf, sizeof buf, "%.*f", dec, r);
      string out = buf;
      if (dec > 0)
	{
	  std::string::size_type last = out.find_last_not_of ('0');
	  if (last != string::npos && out[last] == '.')
	    last--;
	  out = out.substr (0, last + 1);
	}
      return out;
    }
  // SENSITIVE?
  else if (pname == "ucase")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      string rv = function_args[0];
      for (uint i = 0; i < rv.length(); i ++)
	rv[i] = toupper (rv[i]);
      return rv;
    }
  // SENSITIVE?
  else if (pname == "rand")
    {
      if (function_args.size() != 2)
	return bad_arg_count(pname);
      /* Signed, and widened, on purpose.  The bounds were read into uints, so a
       * negative range ($rand(-3;3)$) wrapped into a huge positive result, and
       * a reversed one ($rand(1;%n%)$ with n == 0) made the modulus zero -- a
       * division by zero, i.e. undefined: observed as a garbage value at -O2,
       * but a SIGFPE is equally allowed.  A reversed pair is now read as the
       * range the game plainly meant. */
      long long lower = parse_int (function_args[0]),
	upper = parse_int (function_args[1]);
      if (upper < lower)
	{
	  long long tmp = lower;
	  lower = upper;
	  upper = tmp;
	}
      unsigned long long range = (unsigned long long) (upper - lower) + 1ULL;

      // TODO: change this to use the high order bits of the random # instead
#ifdef SPATTERLIGHT
      return string_int (lower + (long long) (erkyrath_random() % range));
#else
      return string_int (lower + (long long) (rand() % range));
#endif
    }
  // SENSITIVE?
  else if (pname == "speechenabled")
    {
      if (function_args.size() != 0)
	return bad_arg_count(pname);

      return "0";
      /* TODO: return 1 if speech is enabled */
    }
  // SENSITIVE?
  else if (pname == "symbol")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      // SENSITIVE?
      if (function_args[0] == "gt")
	return ">";
      // SENSITIVE?
      else if (function_args[0] == "lt")
	return "<";
      gi->debug_print ("Bad symbol argument: " + function_args[0]);
      return "";
    }
  // SENSITIVE?
  else if (pname == "numberparameters")
    return string_int (function_args.size());
  // SENSITIVE?
  else if (pname == "thisobject")
    return this_object;

  /* disconnectedby, id, name, removeformatting */

  string rv = "";
  
  for (uint i = 0; i < gf.size ("function"); i ++)
    if (ci_equal (gf.block ("function", i).name, pname))
      {
	const GeasBlock &proc = gf.block ("function", i);
	GEAS_DBG << "Running function " << proc << endl;
	for (uint j = 0; j < proc.data.size(); j ++)
	  {
	    GEAS_DBG << "  Running line #" << j << ": " << proc.data[j] << endl;
	    run_script(proc.data[j], rv);
	  }
	return rv;
      }
  gi->debug_print ("No function " + pname + " found.");
  return "";
}



v2string geas_implementation::get_inventory ()
{
  v2string rv = get_room_contents ("inventory");
  /* Append Quest 2.x items that have no real object of that name in the
   * inventory (a game may give a bare item, or give an item while hiding the
   * like-named *room* object).  If a matching inventory object exists we let it
   * speak for itself: a visible one is already in rv, and a hidden one means
   * the item was destroyed/removed (e.g. dropping a vase that "smashes" via
   * hide <Vase>), so the bare item must not linger. */
  for (const string &it: state.items)
    {
      bool shown = false;
      for (const auto &o: state.objs)
	if (ci_equal (o.name, it) && ci_equal (o.parent, "inventory"))
	  { shown = true; break; }
      if (!shown)
	{
	  vstring tmp;
	  tmp.push_back (it);
	  tmp.push_back ("object");
	  rv.push_back (tmp);
	}
    }
  return rv;
}

v2string geas_implementation::get_room_contents ()
{
  return get_room_contents (state.location);
}

v2string geas_implementation::get_room_contents (const string &room)
{
  v2string rv;
  string objname;
  for (const auto &i: state.objs)
    /* Things in this room -- directly or inside an open container/surface here
     * (room_of walks the container chain); the hidden check, kept current by
     * the visibility sweep, drops the contents of closed containers. */
    if (ci_equal (room_of (i.name), room))
      {
	objname = i.name;
	if (!has_obj_property (objname, "invisible") &&
	    !has_obj_property (objname, "hidden"))
	  {
	    vstring tmp;

	    string print_name, temp_str;
	    if (!get_obj_property (objname, "alias", print_name))
	      print_name = objname;
	    tmp.push_back (print_name);

	    string otype;
	    if (!get_obj_property (objname, "displaytype", otype))
	      otype = "object";
	    tmp.push_back (otype);
	    /* The internal name, so a host that lists this entry can ask for its
	       verb menu without re-resolving the printed alias (which may be
	       ambiguous, and would prompt). */
	    tmp.push_back (objname);
	    rv.push_back (tmp);
	  }
      }
  return rv;
}

v2string geas_implementation::get_object_verbs (const string &obj)
{
  /* The same list "verbs <object>" prints, for a host that shows the verb
     menu in its pane; the caller passes the internal name from a
     get_room_contents entry, so no object resolution happens here. */
  return object_verbs (obj);
}

v2string geas_implementation::get_room_exits ()
{
  v2string rv;

  /* Each exit is a {display label, click command} pair.  The label is what the
   * pane shows; the command is what a host runs when the label is clicked as a
   * hyperlink -- the same thing the player would type. */

  /* Directional exits, in the canonical order, skipping the trailing "out"
   * (handled separately below so it can show its destination place). */
  for (size_t i = 0; i + 1 < ARRAYSIZE (dir_names); i ++)
    if (exit_dest (state.location, dir_names[i]) != "")
      {
	string label = dir_names[i];
	if (!label.empty())
	  label[0] = toupper ((unsigned char) label[0]);
	rv.push_back ({label, dir_names[i]});
      }

  /* The "out" exit, with the place it leads to when one is named. */
  if (exit_dest (state.location, "out") != "")
    {
      string out = get_svar ("quest.doorways.out");
      rv.push_back ({out != "" ? "Out to " + out : string ("Out"), "out"});
    }

  /* Named "place" exits, listed by their displayed destination name.  Clicking
   * one navigates with "go to <target>", where the target matches what the
   * "go to" handler compares against: the destination room name for a scripted
   * place, or the displayed name for a plain one. */
  for (const auto &place: get_places (state.location))
    if (place.size() > 2 && place[2] != "")
      {
	bool scripted = (place.size () == 5);
	const string &target = scripted ? place[3] : place[2];
	rv.push_back ({place[2], "go to " + target});
      }

  return rv;
}

vstring geas_implementation::get_status_vars ()
{
  vstring rv;

  string tok, line;
  std::string::size_type c1, c2;

  for (size_t i = 0; i < gf.size("variable"); i ++)
    {
      const GeasBlock &gb = gf.block ("variable", i);
      
      bool nozero = false;
      string disp;
      bool is_numeric = true;

      GEAS_DBG << "g_s_v: " << gb << endl;

      for (const string &line: gb.data)
	{
	  GEAS_DBG << "  g_s_v:  " << line << endl;
	  tok = first_token (line, c1, c2);
	  // SENSITIVE?
	  if (tok == "display")
	    {
	      tok = next_token (line, c1, c2);

	      // SENSITIVE?
	      if (tok == "nozero")
		{
		  nozero = true;
		  tok = next_token (line, c1, c2);
		}
	      if (!is_param (tok))
		gi->debug_print ("Expected param after display: " + line);
	      else
		disp = tok;
	    }
	  // SENSITIVE?
	  else if (tok == "type")
	    {
	      tok = next_token (line, c1, c2);
	      // SENSITIVE?
	      if (tok == "string")
		is_numeric = false;
	    }
	}

      GEAS_DBG << "  g_s_v, block 2, tok == '" << tok << "'" << endl; 
      if (! (is_numeric && nozero && get_ivar (gb.name) == 0) && disp != "")
	{
	  disp = param_contents (disp);
	  string outval = "";
	  for (size_t j = 0; j < disp.length(); j ++)
	    if (disp[j] == '!')
	      {
		if (is_numeric)
		  outval = outval + string_int (get_ivar (gb.name));
		else
		  outval = outval + get_svar (gb.name);
	      }
	    else if (disp[j] == '*')
	      {
		size_t k;
		for (k = j + 1; k < disp.length() && disp[k] != '*'; k ++)
		  ;
		if (!is_numeric || get_ivar (gb.name) != 1)
		  outval = outval + disp.substr (j+1, k - j - 1);
		j = k;
	      }
	    else
	      outval = outval + disp[j];
	  rv.push_back (eval_string (outval));
	}
    }
  return rv;
}

vector<bool> geas_implementation::get_valid_exits()
{
  vector<bool> rv;
  GEAS_DBG << "Getting valid exits\n";
  rv.push_back (exit_dest (state.location, "northwest") != "");
  rv.push_back (exit_dest (state.location, "north") != "");
  rv.push_back (exit_dest (state.location, "northeast") != "");
  rv.push_back (exit_dest (state.location, "west") != "");
  rv.push_back (exit_dest (state.location, "out") != "");
  rv.push_back (exit_dest (state.location, "east") != "");
  rv.push_back (exit_dest (state.location, "southwest") != "");
  rv.push_back (exit_dest (state.location, "south") != "");
  rv.push_back (exit_dest (state.location, "southeast") != "");
  rv.push_back (exit_dest (state.location, "up") != "");
  rv.push_back (exit_dest (state.location, "down") != "");
  GEAS_DBG << "Done getting valid exits\n";

  return rv;
}

void geas_implementation::print_eval_p (const string &s)
{ print_formatted (pcase (eval_string (s))); }

void geas_implementation::print_eval (const string &s)
{ print_formatted (eval_string (s)); }

string geas_implementation::eval_string (const string &s)
{
  string rv;
  std::string::size_type i, j;
  bool do_print = (s.find('$') != string::npos);
  if (do_print) GEAS_DBG << "eval_string (" << s << ")\n";
  for (i = 0; i < s.length(); i ++)
    {
      if (i + 1 < s.length() && s[i] == '#' && s[i+1] == '@')
	{
	  for (j = i + 1; j < s.length() && s[j] != '#'; j ++)
	    ;
	  if (j == s.length())
	    {
	      gi->debug_print ("eval_string: Unmatched hash in " + s);
	      break;
	    }
	  rv = rv + displayed_name (get_svar (s.substr (i+2, j-i-2)));
	  i = j;
	}
      else if (s[i] == '#')
	{
	  for (j = i + 1; j < s.length() && s[j] != '#'; j ++)
	    ;
	  if (j == s.length())
	    {
	      gi->debug_print ("eval_string: Unmatched hash in " + s);
	      break;
	    }
	  std::string::size_type k;
	  for (k = i + 1; k < j && s[k] != ':'; k ++)
	    ;
	  if (k == j && j == i + 1)
	    rv += "#";
	  else if (k == j)
	    rv += get_svar (s.substr (i+1, j-i-1));
	  else
	    {
	      string propname = s.substr (k+1, j-k-1);
	      if (s[i+1] == '(')
		{
		  if (s[k-1] != ')')
		    {
		      gi->debug_print ("e_s: Missing paren in '" +
				       s.substr (i, j-i) + "' of '" + s + "'");
		      break;
		    }
		  string objvar = s.substr (i+2, k-i-3);
		  string objname = get_svar (objvar);
		  string tmp;
		  if (get_obj_property (objname, propname, tmp))
		    rv += tmp;
		  else
		    gi->debug_print ("e_s: Evaluating nonexistent object prop "
				     "{" + objname + "}:{" + propname + "}");
		}
	      else
		{
		  string objname = s.substr (i+1, k-i-1);
		  string tmp;
		  if (get_obj_property (objname, propname, tmp))
		    rv += tmp;
		  else
		    gi->debug_print ("e_s: Evaluating nonexistent var " + objname);
		}
	    }
	  i = j;
	}
      else if (s[i] == '%')
	{
	  for (j = i + 1; j < s.length() && s[j] != '%'; j ++)
	    ;
	  if (j == s.length())
	    {
	      gi->debug_print ("e_s: Unmatched %s in " + s);
	      break;
	    }
	  if (j == i + 1)
	    rv += "%";
	  else
	    rv += fmt_double (get_dvar (s.substr (i+1, j-i-1)));
	  i = j;
	}
      else if (s[i] == '$')
	{
	  std::string::size_type j = s.find ('$', i + 1);
	  if (j == string::npos)
	    {
	      gi->debug_print ("Unmatched $s in " + s);
	      return rv + s.substr (i);
	    }
	  string tmp1 = s.substr (i + 1, j - i - 1);
	  GEAS_DBG << "e_s: first substr was '" << tmp1 << "'\n";
	  string tmp = eval_string (tmp1);
	  GEAS_DBG << "e_s: eval substr " + s + "': '" + tmp + "'\n"; 

	  string func_eval;

	  std::string::size_type paren_open, paren_close;
	  if ((paren_open = tmp.find ('(')) == string::npos)
	    {
	      /* A bare $name$ user-function call takes no arguments, so reset
	       * function_args -- otherwise it inherits the leftover args of the
	       * previously evaluated function (e.g. $desertroom$ seeing the args
	       * of a prior $desertroom(x;y)$ and mis-reading $numberparameters$).
	       * Builtins like numberparameters/parameter instead inspect the
	       * enclosing procedure's args, so leave function_args intact. */
	      bool is_user_func = false;
	      for (uint k = 0; k < gf.size ("function"); k ++)
		if (ci_equal (gf.block ("function", k).name, tmp))
		  { is_user_func = true; break; }
	      if (is_user_func)
		func_eval = run_function (tmp, vector<string> ());
	      else
		func_eval = run_function (tmp);
	    }
	  else
	    {
	      paren_close = tmp.find (')', paren_open);
	      if (paren_close == string::npos)
		gi->debug_print ("No matching right paren in " + tmp);
	      else
		{
		  /* Trim so "$round (...)$" (space before the paren) dispatches. */
		  string f_name = trim (tmp.substr (0, paren_open));
		  string f_args = tmp.substr (paren_open + 1,
					      paren_close - paren_open - 1);
		  func_eval = run_function (f_name, split_f_args (f_args));
		}
	    }
	  rv = rv + func_eval;
	  i = j;
	}
      else
	rv += s[i];
    }


  return rv;
}

void geas_implementation::tick_timers()
{
  if (!state.running) 
    return;
  for (size_t i = 0; i < state.timers.size(); i ++)
    {
      TimerRecord &tr = state.timers[i];
      if (tr.is_running)
	{
	  if (tr.timeleft != 0)
	    tr.timeleft --;
	  else
	    {
	      /* Quest timers repeat every `interval` ticks until explicitly
	       * timeroff'd (a one-shot timer calls timeroff in its own action,
	       * directly or via a variable onchange).  Re-arm and keep running
	       * rather than stopping after the first firing -- otherwise
	       * counter timers (interval 1, action `dec <x>`) only fire once. */
	      tr.timeleft = tr.interval;
	      const GeasBlock *gb = gf.find_by_name ("timer", tr.name);
	      if (gb != NULL)
		{
		  std::string::size_type c1, c2;
		  for (const auto &line: gb->data)
		    {
		      string tok = first_token (line, c1, c2);
		      // SENSITIVE?
		      if (tok == "action")
			{
			  run_script (line.substr (c2));
			  break;
			}
		    }
		}
	    }
	}
    }
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


GeasResult GeasInterface::print_formatted (const string &s, bool with_newline)
{
  std::string::size_type i, j;


  for (i = 0; i < s.length(); i ++)
    {
      if (s[i] == '|')
        {
          // changed indicated whether cur_style has been changed
          // and update_style should be called at the end.
          // it is true unless cleared (by |n or |w).
          bool changed = true;
          j = i;
          i ++;
          if (i == s.length())
            continue;

	  // Are the | codes case-sensitive?
          switch (s[i])
            {
            case 'u': cur_style.is_underlined = true; break;
            case 'i': cur_style.is_italic     = true; break;
            case 'b': cur_style.is_bold       = true; break;
            case 'c':
              i ++;

              if (i == s.length()) { clear_screen(); break; }

              switch (s[i])
                {
                case 'y': cur_style.color = "#ffff00"; break;
                case 'g': cur_style.color = "#00ff00"; break;
                case 'l': cur_style.color = "#0000ff"; break;
                case 'r': cur_style.color = "#ff0000"; break;
                case 'b': cur_style.color = "";  break;

                default:
                  clear_screen();
                  --i;
                }
              break;

            case 's':
	      {
		i ++;
		if (i == s.length() || !(s[i] >= '0' && s[i] <= '9'))
		  continue;
		i ++;
		if (i == s.length() || !(s[i] >= '0' && s[i] <= '9'))
		  continue;
		
		int newsize = parse_int (s.substr(j, i-j));
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
                continue;
              if (s[i] == 'l') cur_style.justify = JUSTIFY_LEFT;
              else if (s[i] == 'r') cur_style.justify = JUSTIFY_RIGHT;
              else if (s[i] == 'c') cur_style.justify = JUSTIFY_CENTER;
              break;

            case 'n':
              print_newline();
              changed = false;
              break;

            case 'w':
              wait_keypress("");
              changed = false;
              break;

            case 'x':
              i ++;

              if (s[i] == 'b')
                cur_style.is_bold = false;
              else if (s[i] == 'u')
                cur_style.is_underlined = false;
              else if (s[i] == 'i')
                cur_style.is_italic = false;
              else if (s[i] == 'n' && i + 1 == s.length())
                changed = with_newline = false;
              break;

            default:
              GEAS_DBG << "p_f: Fallthrough " << s[i] << std::endl;
              changed = false;
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
  if (with_newline)
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
