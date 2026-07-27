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

/* Bumps a nesting counter for as long as it is alive, so every early `return`
   out of run_script still unwinds it.  See kMaxScriptDepth. */
namespace {
  struct ScriptDepth {
    int &d;
    explicit ScriptDepth (int &depth) : d (depth) { ++ d; }
    ~ScriptDepth () { -- d; }
    ScriptDepth (const ScriptDepth &) = delete;
    ScriptDepth &operator= (const ScriptDepth &) = delete;
  };
}

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
  size_t n;
  if (!find_svar (varname, n))
    {
      /* Quest keeps string and numeric variables in two independent arrays
	 (_stringVariable / _numericVariable), so the same name can name one of
	 each and "#x#" and "%x%" then read different values.  Games rely on it:
	 "enter <players>" fills the string, and "set numeric <players;
	 #players#>" converts it (SetNumericVariableContents,
	 V4Game.Part2.cs:477-536, which never consults the string array).  geas
	 used to refuse the second definition and leave %players% undefined. */
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
  size_t n;
  if (!find_ivar (varname, n))
    {
      /* A string variable of the same name is no obstacle -- see set_svar. */
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
      regen_var_objects();
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

void geas_implementation::move (const string &obj, const string &dest)
{
  if (const vector<size_t> *v = state.obj_records (obj))
    {
      string was = state.objs[(*v)[0]].parent;
      state.objs[(*v)[0]].parent = dest;
      /* Only the two containers this move touches: DoAddRemove sweeps the parent
       * the object went into, and the "remove" path the one it came out of
       * (V4Game.cs:2133, 2698).  See update_container_visibility. */
      if (was != "" && !ci_equal (was, dest))
	update_container_visibility (was);
      update_container_visibility (dest);
      gi->update_sidebars();
      regen_var_objects();
      return;
    }
  gi->debug_print ("Tried to move nonexistent object '" + obj +
		   "' to '" + dest + "'.");
}

/* See the comment on the declaration in geas-impl.hh. */
void geas_implementation::do_add_remove (const string &child, const string &parent,
					 bool adding)
{
  if (adding)
    {
      /* Quest writes the container's definition name, never its alias. */
      set_obj_property (child, "parent=" + parent);
      move (child, parent);
    }
  else
    {
      set_obj_property (child, "not parent");
      move (child, room_of_parent (get_obj_parent (child)));
    }
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
      /* the container may itself sit inside another open container */
      cur = obj->parent;
    }
  return false;   /* ran off the depth cap: the chain must be a cycle */
}

string geas_implementation::obj_parent (const string &obj) const
{
  if (const vector<size_t> *v = state.obj_records (obj))
    return state.objs[(*v)[0]].parent;
  return "";
}

string geas_implementation::room_of (const string &obj) const
{
  return room_of_parent (obj_parent (obj));
}

/* room_of for a parent that is already in hand.  Two objects may share a name
 * (Uranus has a "Your Ship" in two rooms), and obj_parent then answers for
 * whichever was defined first, so anything walking the chain on behalf of a
 * particular object record has to start from that record's own parent. */
string geas_implementation::room_of_parent (const string &parent) const
{
  string p = parent;
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
  for (const auto &o: state.objs)
    {
      const string &p = o.parent;
      if (p == "" ||
	  (!has_obj_property (p, "container") && !has_obj_property (p, "surface")))
	continue;   /* only objects inside a container/surface are swept */
      if (only_parent != "" && !ci_equal (p, only_parent))
	continue;
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
      do_add_remove (child, "", false);
      return true;
    }
  display_error ("cantremove", child);
  return false;
}

/* Is `room` somewhere the player can actually stand?  Quest's GetRoomID scans
 * _rooms for a RoomName match (V4Game.cs:6348-6365) -- the room's *alias* is
 * never consulted -- and "create room" appends a new, empty entry to that same
 * array (ExecuteCreate, V4Game.cs:5244-5276), which is why runtime-created
 * rooms count too. */
bool geas_implementation::room_exists (const string &room) const
{
  return gf.find_by_name ("room", room) != NULL ||
         has_obj_property ("!createdroom", lcase (room));
}

void geas_implementation::goto_room (string room)
{
  /* PlayGame refuses a room that does not exist: it logs "No such room 'X'" and
   * returns without touching _currentRoom (V4Game.Part2.cs:6674-6684), so the
   * player stays where they were.  geas used to move anyway, which invented a
   * phantom room -- and since rooms and objects share the property table, a
   * name that belongs to an object rendered that object's prefix and look text
   * as the room description.  "pure chaos" (game.asl) does `goto <bed>` when
   * `bed` is an object and the room it means only carries `alias <bed>`, and
   * ended up describing the bed as though it were a place. */
  if (!room_exists (room))
    {
      gi->debug_print ("No such room '" + room + "'");
      return;
    }
  state.location = room;
  regen_var_room();
  regen_var_dirs();
  regen_var_look();
  regen_var_objects();
  /* PlayGame stamps the room object with a "visited" property, which games test
   * with `property <room; visited>` (V4Game.Part2.cs:6688-6708).  Which side of
   * the room's own entry script that happens on is version-dependent: 391..409
   * mark it on the way in, 410 and later only after the script has run, so a
   * 410 room script still sees its own room as unvisited the first time.  The
   * Pilgrims Progress (ASL 400) counts on the early form -- its nine-room field
   * maze only produces Evangelist once every other field is `visited`, and the
   * check lives in the script of the room you walk back into.
   *
   * Quest overwrites the property in place rather than appending a second copy
   * (AddToObjectProperties, V4Game.cs:4041-4059), so only stamp it once -- a
   * long game would otherwise pile up one property record per step taken. */
  if (asl_version_ >= 391 && asl_version_ < 410 && !has_room_property (room, "visited"))
    set_obj_property (room, "visited");
  /* Quest's order (PlayGame): describe the room first, then run its entry
   * script.  If that script gotos another room, the nested goto describes the
   * destination -- so, unlike the old script-then-look order, there is no
   * trailing look() here to redescribe after a redirect. */
  look();
  string scr;
  if (get_room_action (room, "script", scr))
    run_script_as (room, scr);
  if (asl_version_ >= 410 && !has_room_property (room, "visited"))
    set_obj_property (room, "visited");
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
  /* PlayerError.CantRemove / PlayerError.DefaultRemove, the two messages the
   * container half of "remove" falls back on (V4Game.Part2.cs:449-451). */
  else if (errorname == "cantremove")
    print_eval ("You can't remove that.");
  else if (errorname == "defaultremove")
    print_eval ("Done.");
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
  else if (errorname == "defaultopen")
    print_eval ("You open it.");
  else if (errorname == "defaultclose")
    print_eval ("You close it.");
  else if (errorname == "locked")
    /* PlayerError.Locked, the message a locked exit prints when it declares no
     * lockmessage of its own (V4Game.Part2.cs:452). */
    print_eval ("The exit is locked.");
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
	  string rest = trim (line.substr (c2));
	  /* The destination room's alias replaces its name only for a *plain*
	   * place: both the listing and the name the player has to type are
	   * gated on the place having no script (GetGoToExits,
	   * V4Game.Part2.cs:7790-7810, and PlaceExist, ibid. 6568-6578).  So a
	   * scripted `place <elevatorroom> { ... }` shows -- and answers to --
	   * "elevatorroom" even when that room is aliased "elevator", which is
	   * what World's End's weapon store does.  geas used to alias both
	   * kinds, so the listing advertised a name that GO TO then rejected. */
	  string displayed = (rest == "") ? displayed_name (dest) : dest;
	  string printed_dest = (prefix != "" ? prefix + " " : "") +
	    "|b" + displayed + "|xb";

	  vector<string> tmp;
	  tmp.push_back (printed_dest);
	  tmp.push_back (prefix + " " + displayed);
	  tmp.push_back (displayed);
	  tmp.push_back (dest);
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
	  /* The rest of the record is the destination room's *name*, spaces and
	   * all -- "destroy exit Nickel Building" -- so it cannot be read with
	   * next_token, which stops at the first space.  Reading only "Nickel"
	   * matched nothing, and every "destroy exit <room; multi word place>"
	   * silently did nothing: "The Things That Go Bump In The Night"
	   * destroys four of them in its startscript, so the Nickel Building
	   * still listed the cargo door it had just shut (twice over, once the
	   * door opener created it again) and the padlocked Work Shed could be
	   * walked into before its chain was cut.  Quest matches the name in
	   * full, and case-sensitively: at 4.10 FindExit looks the place up in
	   * the room's places dictionary (V4Game.Part2.cs:7885-7891), and below
	   * that DestroyExit compares PlaceName with = (V4Game.cs:3632-3641). */
	  string dest_name = trim (line.substr (c2));

	  for (v2string::iterator j = rv.begin(); j != rv.end(); j ++)
	    if ((*j)[3] == dest_name)
	      {
		rv.erase(j);
		break;
	      }
	}
      
      
    }

  GEAS_DBG << "get_places (" << room << ") -> " << rv << "\n";
  return rv;
}

/* An exit in Quest 4.10 is an object like any other: RoomExit's constructor
 * appends a record to _objs with IsExit set (RoomExit.cs:16-27) and names it
 * "<room>.<direction>" for a compass exit, or "<room>.exitN" -- N counted per
 * room, from the room object's own quest.lastexitid property -- for a
 * directionless "place" (UpdateObjectName, RoomExit.cs:172-232).  The objects
 * are created as the room's exit tags are read, in source order, so that is the
 * order "for each exit in <room>" walks them.
 *
 * Two things follow from exits being ordinary object records, and both matter:
 *
 *  - a destroyed exit is still in the list.  DestroyExit hands the exit to
 *    RoomExits.RemoveExit, which only clears the object's Exists flag
 *    (RoomExits.cs:575-591), and ExecForEach filters on IsRoom/IsExit alone --
 *    it never looks at Exists (V4Game.cs:5029-5040).  Tim Hamilton's "The Things
 *    That Go Bump In The Night" is built on this: its game-block description
 *    counts the exits with "for each exit in <#quest.currentroom#> inc
 *    <exitcheck>" and prints "You can go nowhere." when the count is zero, and
 *    the one room it wants to say that about (the Nickel Building, before the
 *    cargo door opens) gets there with a "dec <exitcheck>" *inside* the same
 *    loop -- which only ever cancels out if the two exits it destroyed in the
 *    startscript are still being counted.
 *
 *  - re-creating a compass exit does not add a second object.  SetDirection
 *    reuses the existing RoomExit for a direction the room has already had
 *    (RoomExits.cs:20-33), with a comment saying as much, so "create exit north"
 *    after "destroy exit north" leaves the count alone.  A *place* exit is not
 *    reused -- AddPlaceExit builds a fresh RoomExit and merely marks the old one
 *    non-existent -- so repeatedly destroying and re-creating one place would
 *    keep growing Quest's list.  Nothing in the corpus does that, and geas keeps
 *    no history of destroyed places, so places are deduplicated by destination
 *    here. */
/* One of the eleven direction tags?  "out" counts: Quest gives it a Direction of
 * its own, so it is an exit object like the compass points and takes part in the
 * 4.10 folded exits line (RoomExits.AddExitFromTag, RoomExits.cs:57-115). */
static bool is_dir_name (const string &t)
{
  for (size_t i = 0; i < ARRAYSIZE (dir_names); i ++)
    if (t == dir_names[i])
      return true;
  return false;
}

static bool ci_contains (const vector<string> &v, const string &t)
{
  for (const string &i: v)
    if (ci_equal (i, t))
      return true;
  return false;
}

vector<string> geas_implementation::exit_dir_order (const string &room)
{
  vector<string> rv;
  const GeasBlock *gb = gf.find_by_name ("room", room);
  std::string::size_type c1, c2;
  if (gb != NULL)
    for (const string &line: gb->data)
      {
	string tok = first_token (line, c1, c2);
	if (is_dir_name (tok) && !ci_contains (rv, tok))
	  rv.push_back (tok);
      }
  /* "create exit <dir> <room; dest>" for a direction the room never declared
   * adds a new exit object, and so a new entry at the end. */
  for (const auto &e: state.exits)
    {
      if (!ci_equal (e.src, room))
	continue;
      string tok = first_token (e.dest, c1, c2);
      if (tok != "exit")
	continue;
      tok = next_token (e.dest, c1, c2);
      if (is_dir_name (tok) && !ci_contains (rv, tok))
	rv.push_back (tok);
    }
  return rv;
}

vector<string> geas_implementation::exit_object_names (const string &room)
{
  vector<string> rv;
  const GeasBlock *gb = gf.find_by_name ("room", room);
  /* Quest names the exit after the room object, which carries the spelling used
   * by "define room" rather than whatever casing a goto used. */
  string rname = (gb != NULL && gb->name != "") ? gb->name : room;

  vector<string> seen_dirs, seen_places;
  int place_id = 0;

  std::string::size_type c1, c2;
  if (gb != NULL)
    for (const string &line: gb->data)
      {
	string tok = first_token (line, c1, c2);
	if (is_dir_name (tok))
	  {
	    if (!ci_contains (seen_dirs, tok))
	      {
		seen_dirs.push_back (tok);
		rv.push_back (rname + "." + tok);
	      }
	  }
	else if (tok == "place")
	  {
	    tok = next_token (line, c1, c2);
	    if (!is_param (tok))
	      continue;
	    string dest_param = eval_param (tok);
	    std::string::size_type i = dest_param.find (';');
	    string dest = trim (i == string::npos ? dest_param
				: dest_param.substr (i + 1));
	    if (dest != "" && !ci_contains (seen_places, dest))
	      {
		seen_places.push_back (dest);
		rv.push_back (rname + ".exit" + string_int (++place_id));
	      }
	  }
      }

  /* Then the exits made during play, in the order they were made.  "noexit" and
   * "destroy exit" records take nothing away: the object outlives both. */
  for (const auto &e: state.exits)
    {
      if (!ci_equal (e.src, room))
	continue;
      const string &line = e.dest;
      string tok = first_token (line, c1, c2);
      if (tok != "exit")
	continue;
      tok = next_token (line, c1, c2);
      if (is_dir_name (tok))
	{
	  if (!ci_contains (seen_dirs, tok))
	    {
	      seen_dirs.push_back (tok);
	      rv.push_back (rname + "." + tok);
	    }
	  continue;
	}
      /* Directionless: "exit <src; dest>", so the token after "exit" is already
       * the parameter. */
      if (!is_param (tok))
	continue;
      vector<string> p = split_param (param_contents (tok));
      if (p.size () != 2)
	continue;
      if (!ci_contains (seen_places, p[1]))
	{
	  seen_places.push_back (p[1]);
	  rv.push_back (rname + ".exit" + string_int (++place_id));
	}
    }

  return rv;
}

string geas_implementation::exit_dest (const string &room, const string &dir, bool *is_script) const
{
  std::string::size_type c1, c2;
  string tok;
  if (is_script != NULL)
    *is_script = false;

  string dyn_dest;
  bool have_dyn = false;
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
	/* From ASL 4.10 on, "destroy exit <room; north>" takes a *directional*
	 * exit away too, not just a "go to" place: DestroyExit hands the exit to
	 * RoomExits.RemoveExit, which clears the exit object's Exists flag
	 * whichever kind it is (V4Game.cs:3595-3606, RoomExits.cs:575-591).
	 * Before 4.10 the same statement only ever searched the room's *places*
	 * list, so a direction name matched nothing and the statement did nothing
	 * at all (V4Game.cs:3608-3654) -- hence the version test.  geas only ever
	 * read these records in get_places (), so a destroyed direction stayed
	 * walkable and stayed in the "You can go ..." list.  Tim Hamilton's "The
	 * Maze" is the game that cares: its one room is a 15x20 hedge maze held in
	 * four strings of y/n, and every move runs a procedure that destroys and
	 * re-creates the four compass exits to match the walls around the new
	 * square.  Without this the maze has no walls at all.  A place exit's
	 * record names a room rather than a direction, so it falls through to
	 * get_places (). */
	if (tok == "destroy")
	  {
	    if (asl_version_ >= 410
		&& next_token (line, c1, c2) == "exit"
		&& ci_equal (next_token (line, c1, c2), dir))
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
	dyn_dest = p[1];
	have_dyn = true;
	break;
      }

  const GeasBlock *gb = gf.find_by_name ("room", room);
  if (gb == NULL)
    {
      if (!have_dyn)
	gi->debug_print (string ("Trying to find exit <") + dir +
			 "> of nonexistent room <" + room + ">.");
      return dyn_dest;
    }

  /* From ASL 4.10 on a direction's exit is one long-lived object whose script is
   * set once and never cleared: "create exit <dir> <room; dest>" reuses the
   * existing object (SetDirection, RoomExits.cs:20-33), assigns the new
   * destination, and only calls SetScript if the new tag brings a script of its
   * own -- and Go checks for a script before the destination
   * (RoomExit.cs:240-259).  So a declared script exit survives being destroyed
   * and re-created with a plain destination, and still runs.  That is precisely
   * what "The Maze" relies on: its four exits are scripts that move the player
   * by adjusting %xpos% and %ypos%, and the re-creating procedure passes them
   * "<Maze; Maze>", which would otherwise re-enter the room with the coordinates
   * untouched.
   *
   * Before 4.10 there are no exit objects: "create exit north <room; dest>"
   * assigns the room's North field directly and stamps its type back to Text
   * (V4Game.cs:5429-5450), wiping whatever script the block declared.  Half a
   * dozen games in the corpus depend on that -- "Bear Campsite" (ASL 400)
   * declares `south { msg <a Grizzly Bear blocks your path> ... }` and opens the
   * way out with `create exit south <Campsite; Freedom>` once the bear has
   * choked on the fish -- so the dynamic destination has to win there. */
  bool declared_is_script = false;
  string declared = declared_exit_dest (gb, dir, declared_is_script);
  if (declared_is_script && (asl_version_ >= 410 || !have_dyn))
    {
      if (is_script != NULL)
	*is_script = true;
      return declared;
    }
  if (have_dyn)
    return dyn_dest;
  return declared;
}

string geas_implementation::declared_exit_dest (const GeasBlock *gb,
						const string &dir,
						bool &is_script) const
{
  std::string::size_type c1, c2;
  is_script = false;
  // TODO: what's the priority on this?
  for (const string &line: gb->data) {
    string tok = first_token (line, c1, c2);
    if (tok == dir) {
      std::string::size_type line_start = c2;
      tok = next_token (line, c1, c2);
      /* "<dir> locked <dest; lockmessage>": a (initially locked) exit whose
       * destination is the first ;-separated field.  Locking only gates
       * traversal (handled by the caller), so return the destination here.
       *
       * A locked exit that carries a *script* is different: it has no
       * destination at all, and its parameter -- if it has one -- is the lock
       * message (RoomExits.cs:186-191).  Reading that field as a room name
       * would send the player to a room named after the refusal text. */
      if (ci_equal (tok, "locked")) {
	std::string::size_type after_lock = c2;
	tok = next_token (line, c1, c2);
	if (is_param (tok)) {
	  string rest = trim (line.substr (c2));
	  if (rest != "") {
	    is_script = true;
	    return rest;
	  }
	  vector<string> p = split_param (param_contents (tok));
	  if (!p.empty ())
	    return trim (p[0]);
	  return "";
	}
	if (tok != "") {
	  is_script = true;
	  return trim (line.substr (after_lock));
	}
	return "";
      }
      if (is_param (tok))
	return param_contents(tok);
      if (tok != "")
	{
	  is_script = true;
	  return trim (line.substr (line_start + 1));
	}
      return "";
    }
  }
  return "";
}

/* Split the parameter of a directional exit -- "north <...>", and "out <...>" --
 * into the room it leads to and the prefix printed ahead of that room's name.
 *
 * The two ASL generations read a two-field parameter the other way round.  Up to
 * 3.53 GoDirection enters everything *after* the semicolon, so "out <the; town>"
 * means prefix "the" plus destination "town" (V4Game.Part2.cs:6319-6326).  From
 * 4.10 on the exit tag is parsed by AddExitFromTag, which takes the *first*
 * field as the destination and the second as a lock message -- inert unless the
 * tag also says "locked" (RoomExits.cs:186-191); a prefix, if any, comes from
 * the destination room's own "prefix" line instead (RoomExit.cs:231). */
void geas_implementation::split_exit_dest (const string &raw, string &dest,
					   string &prefix) const
{
  prefix = "";
  std::string::size_type i = raw.find (';');
  if (i == string::npos)
    {
      dest = trim (raw);
      return;
    }
  if (asl_version_ >= 410)
    {
      dest = trim (raw.substr (0, i));
      return;
    }
  prefix = trim (raw.substr (0, i));
  dest = trim (raw.substr (i + 1));
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
      /* A directional exit that runs a script has no destination to spend the
       * first field on, so the whole parameter is the lock message
       * (RoomExits.cs:186-191).  (A *place* exit keeps its destination there
       * even when scripted, but geas never locks places.) */
      if (trim (line.substr (c2)) != "") {
	if (!p.empty ())
	  message = trim (p[0]);
      }
      else if (p.size () >= 2)
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
  if (get_room_action (state.location, "description", tmp))
    { run_script_as (state.location, tmp); described = true; }
  else if (get_room_property (state.location, "description", tmp))
    { print_formatted (tmp); described = true; }
  else
    {
      /* A room with no description tag of its own falls back to a "description"
       * line in the game block, which stands in for every such room
       * (V4Game.Part2.cs:3899-3921).  It is read straight out of the block: a
       * parameter is text, anything else is a script.  This cannot go through
       * get_obj_action/get_obj_property, because the game block's lines are not
       * rewritten into "action <...>"/"properties <...>" form the way a room's
       * are (readfile.cc:415-420 lists no reserved words for the game pass), so
       * those lookups always came back empty.  Tim Hamilton's "The Maze" is
       * built entirely on this: it has one room, and the game-block description
       * is a `select case <%xpos%>` that prints "Entrance to maze", "Center of
       * maze", "Exit" or plain "Hedge Maze" from the player's coordinates. */
      const GeasBlock *game = gf.find_by_name ("game", "game");
      if (game != NULL)
	{
	  std::string::size_type c1, c2;
	  for (const string &line: game->data)
	    if (first_token (line, c1, c2) == "description")
	      {
		string rest = trim (line.substr (c2));
		if (is_param (rest))
		  print_formatted (param_contents (rest));
		else
		  run_script_as ("game", rest);
		described = true;
		break;
	      }
	}
    }

  if (!described)
    {
      string in_desc;
      if (get_room_property (state.location, "indescription", tmp))
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
  /* tick_timers() counts a timer down and fires it in the same tick, so the
   * next tick is the firing one once the countdown is down to its last step.
   * A timer whose bypass is still up spends that tick doing nothing. */
  for (const auto &t: state.timers)
    if (t.is_running && !t.bypass && t.timeleft <= 1)
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
  regen_var_objects ();
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
	      // SENSITIVE?
	      if (tok == "singleplayer" || tok == "multiplayer")
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

      /* ... unless the startscript asked for no intro, in which case it has
	 (probably) displayed <intro> itself at the point it wanted it. */
      if (auto_intro_)
	run_script ("displaytext <intro>");

      regen_var_room ();
      regen_var_objects ();
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

void geas_implementation::regen_var_objects ()
{
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
  vector <string> objs;
  for (const auto &i: state.objs)
    {
      /* List things in this room -- directly, or inside an open container/
       * surface here (room_of walks up the container chain). */
      /* Start from this record's own parent rather than from its name: two
       * objects may share a name in different rooms (The Hobbit defines a
       * "ring" in three of them), and room_of would then answer for
       * whichever was defined first, listing every one of them in that one
       * room and none of them anywhere else. */
      if (ci_equal (room_of_parent (i.parent), state.location) &&
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
  if (get_room_property (state.location, "suffix", tmp))
    formatroom = formatroom + " " + tmp;  
  set_svar ("quest.formatroom", formatroom);

}


void geas_implementation::regen_var_look ()
{
  string look_tag;
  if (!get_room_property (state.location, "look", look_tag))
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
  /* An "out { ... }" script is not a destination: readfile de-inlines the block
   * into "out do <!intprocNNN>", and a room name never contains an angle
   * bracket.  Quest keeps the two apart at the source level -- Out.Text stays
   * empty for a scripted exit, so ShowRoomInfo (V4Game.Part2.cs:7196-7212,
   * 7275-7313) prints no "You can go out to ..." line at all -- while geas used
   * to paste the script text into the line ("You can go out to do
   * <!intproc242>." on Shipwrecked's sail boat).  (From ASL 410 on Quest folds
   * "out" into the single directions line instead, via
   * RoomExits.GetAvailableDirectionsDescription; geas still prints the
   * pre-410 two-line form.) */
  if (out_dest.find ('<') != string::npos)
    out_dest = "";
  if (out_dest == "")
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

  /* "quest.doorways" -- the ASL 4.10 single folded list of everywhere you can go
   * from here, which is what a 4.10 game writes its own exits line from.  It is
   * set by RoomExits.GetAvailableDirectionsDescription (RoomExits.cs:303-353),
   * called from UpdateDoorways only when ASLVersion >= 410 (V4Game.Part2.cs:
   * 7191-7194) -- below 4.10 the variable is never assigned at all, so leave it
   * alone there.  (The three ".dirs"/".out"/".places" variables are the other way
   * round: 4.10 stops setting those.  geas keeps setting them, because it still
   * prints the pre-4.10 two-line form of the exits itself; see above.)
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
      for (const auto &p: current_places)
	parts.push_back (p[0]);
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
      if (try_match (s, false, false))
	{
	  /* TODO TODO */
	  // run after turn events ???
	}
      else
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
	if (last_object != "" &&
	    (lc == "it" || lc == "them" || lc == "they" ||
	     lc == "him" || lc == "her"))
	  obj_name = last_object;
	else
	  obj_name = get_obj_name (binding.var_text, where, is_internal);
	if (obj_name == "!")
	  {
	    if (!quiet_notfound)
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
   * matches.  Quest runs the same two passes and gates the second one on
   * ASLVersion >= 391 and on "define options / abbreviations" (Disambiguate,
   * V4Game.cs:4724-4767), so honour both: the loose pass is not merely a
   * nicety, it decides which of two like-named objects a puzzle sees, and
   * Permanant Room turns it off precisely so that "use pen on paper" reaches
   * the held `Peice of Paper' (aliased "paper") instead of stopping at the
   * scenery `Wall Paper' in the room. */
  bool try_partial = use_abbreviations_ && asl_version_ >= 391;
  for (int pass = 0; pass < (try_partial ? 2 : 1) && objs.empty(); pass ++)
    {
      bool allow_partial = (pass == 1);
      for (size_t objnum = 0; objnum < state.objs.size(); objnum ++)
	{
	  bool is_used = false;
	  for (auto &loc: where)
	    {
	      // SENSITIVE?
	      /* Quest scopes a typed noun on the object's room and its hidden flag
	       * alone: DisambObjHere matches ContainerRoom against the room (and
	       * the inventory) and tests Exists, which is what the hidden property
	       * sets (V4Game.cs:4303-4350, 4165-4178).  Something parented inside a
	       * container keeps the room it was defined in, so the container chain
	       * only reaches scope through those hidden flags -- which the engine
	       * re-derives on container events, not continuously (see
	       * update_container_visibility).  Hence room_of, which walks up the
	       * chain, rather than a reachability test on the container itself: a
	       * game that reveals a parented object with "show" and never opens the
	       * container (Darkness's Hammer, Christmas Day's Knife) still leaves it
	       * referrable, as Quest does. */
	      if (loc == "game" || state.objs[objnum].parent == loc ||
		  ci_equal (room_of_parent (state.objs[objnum].parent), loc))
		is_used = true;
	    }
	  if (is_used &&
	      !has_obj_property (state.objs[objnum].name, "hidden") &&
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

bool geas_implementation::try_game_verb (const string &cmd, bool is_internal)
{
  match_rv match;

  /* Game-scope verb declarations.  Quest writes these as either
   *   verb <name[;synonym;...]> <default script>     (asl source) or
   *   <name[;synonym;...]> <default script>           (compiled form),
   * optionally prefixed with "lib" for a library-supplied verb.  They register
   * custom verbs (e.g. "destroy", "mount") with a default response; an object
   * overrides via action <name> or properties <name=...>.
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
	  /* The action/property the verb looks for on the object is whatever
	   * follows a colon, and the colon is taken out of the name list; with no
	   * colon it is the first (or only) name (V4Game.cs:2958-2976).
	   *
	   * ExecVerb reads the name list with GetParameter, so #string#, %numeric%
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
	  string names_str = eval_param (names_tok), key;
	  std::string::size_type colon = names_str.find (':');
	  if (colon != string::npos)
	    {
	      key = trim (names_str.substr (colon + 1));
	      names_str = trim (names_str.substr (0, colon));
	    }
	  /* Quest walks the name list with a do-while that trims every segment it
	   * splits off at a semicolon but leaves the *last* one (the whole
	   * parameter, when there is no semicolon at all) exactly as written
	   * (V4Game.cs:2978-3003).  A verb declared "verb <use >" is therefore
	   * matched as BeginsWith (cmd, "use  ") -- two spaces -- and can never
	   * fire.  QDK 4.1.5 emits such names whenever the author leaves a
	   * trailing space in the verb box, and games rely on the resulting dead
	   * verbs without knowing it: "final of social studies.cas" declares
	   * "verb <use >" ahead of "verb <use key in>", and only because the
	   * former is unmatchable does USE KEY IN CLOSED DOOR reach the door's
	   * "action <use key in>" and open the one locked door in the game.
	   * Trimming here made "use " swallow every USE in the file.
	   *
	   * The same line splits on ';' alone, so the "verb <pick up; take, take>"
	   * that QDK writes when an author types a comma registers "pick up" and
	   * "take, take", not "take" -- which is why split_param's semicolon-only
	   * split is right and why TAKE in that game falls through to the later
	   * "verb <take>". */
	  vector<string> names;
	  for (string rest = names_str;;)
	    {
	      std::string::size_type scp = rest.find (';');
	      if (scp == string::npos)
		{
		  names.push_back (rest);
		  break;
		}
	      names.push_back (trim (rest.substr (0, scp)));
	      rest = trim (rest.substr (scp + 1));
	      if (rest == "")
		break;
	    }
	  if (names.empty() || trim (names[0]) == "")
	    continue;
	  if (key == "")
	    key = trim (names[0]);
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
		  else if (deflt != "")
		    run_script_as (obj, deflt);
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
      if (run_commands (cmd, gf.find_by_name ("room", state.location)) ||
	  run_commands (cmd, gf.find_by_name ("game", "game")))
	return true;
      /* Verbs are next, ahead of every built-in and inside the same
       * runUserCommand gate the user commands above are (see try_game_verb). */
      if (try_game_verb (cmd, is_internal))
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
		    if (!c.empty ())
		      print_formatted (m + ".");
		    else
		      {
			/* An empty container answers with its "list empty" script or
			   text if it has one (ObjectContents, V4Game.cs:3425-3431). */
			string le;
			if (get_obj_action (obj, "list empty", le))
			  run_script_as (obj, le);
			else if (get_obj_property (obj, "list empty", le))
			  print_formatted (le);
			else
			  print_formatted ("It is empty.");
		      }
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
      if (!dereference_vars (match.bindings, is_internal))
	return true;
      string script,
	first = match.bindings[give_reversed ? 1 : 0].var_text,
	second = match.bindings[give_reversed ? 0 : 1].var_text;
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
	string script, addtext, first = match.bindings[0].var_text,
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
	    set_obj_property (second, "seen");
	  }
	else if (has_obj_property (second, "surface") ||
		 (has_obj_property (second, "container") && container_is_open (second)))
	  {
	    /* Default: relocate the item into/onto the target (real
	     * containment), then report it.  Mark the target seen so the item
	     * just placed in it is immediately referenceable (the seen gate). */
	    do_add_remove (first, second, true);
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
	      remove_from_container (object, parent);
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
       * script: the room parser fills Out.Text from the parameter *and*
       * Out.Script from whatever follows the '>' (V4Game.Part2.cs:999-1001),
       * and GoDirection runs the script when there is one (6302-6313).  A
       * "create exit out <src; dest>" assigns only Out.Text (V4Game.cs:5485),
       * so a static script still wins over it -- hence the room block is
       * checked for a script first and exit_dest () consulted second. */
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
	display_error ("defaultout");
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
	    // TODO Which display_error do I use?
	    print_formatted ("You can't go that way.");
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
  /* Quest's container statements, all ASL >= 3.91: "add <child; parent>" puts an
   * object into a container, "remove <child>" takes it back out, and
   * "open <container>" / "close <container>" set the open state silently -- no
   * message, no action or property lookup and no container check at all, unlike
   * the open/close *verbs* (ExecuteScript -> SetOpenClose -> DoOpenClose,
   * V4Game.cs:2240, and -> ExecAddRemoveScript -> DoAddRemove, V4Game.cs:2113).
   * Below 3.91 Quest has no such statements, so the line falls through to the
   * unrecognised-script path exactly as it would there. */
  else if ((tok == "add" || tok == "remove") && asl_version_ >= 391)
    {
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
      if (adding)
	{
	  if (index == string::npos)
	    {
	      gi->debug_print ("No parent specified in " + s);
	      return;
	    }
	  string parent = trim (arg.substr (index + 1));
	  if (state.obj_records (parent) == NULL)
	    {
	      gi->debug_print ("Invalid parent object name specified in " + s);
	      return;
	    }
	  do_add_remove (child, parent, true);
	  /* From ASL 4.10 on, putting something into a container also marks the
	   * container "seen"; Quest's own comment explains why -- otherwise you
	   * could LOOK AT the object just added and have disambiguation fail. */
	  if (asl_version_ >= 410)
	    set_obj_property (parent, "seen");
	}
      else
	/* Quest clears the object's "parent" property and leaves its
	 * ContainerRoom alone, so it ends up loose in whatever room the
	 * container it came out of was in. */
	do_add_remove (child, "", false);
      /* No sweep here: move() has already re-derived both the container the
       * object left and the one it entered, which is exactly the pair DoAddRemove
       * touches (V4Game.cs:2133, 2698). */
      gi->update_sidebars ();
      return;
    }
  else if ((tok == "open" || tok == "close") && asl_version_ >= 391)
    {
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
      /* Two fields: the clone lands in the source's *room*, not in the
       * container the source may be sitting in -- ContainerRoom is the room, and
       * geas's room_of walks the container chain to the same place. */
      string dest = (args.size() > 2) ? trim (args[2]) : room_of (src);

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
      regen_var_objects();
      return;
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
      // SENSITIVE?
      else if (tok == "object") // create object
	{
	  /* TODO */
	}
      // SENSITIVE?
      else if (tok == "room") // create room
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
  /* "helpdisplaytext" is displaytext for the same reason helpmsg is msg: with no
   * separate help window it goes through DisplayTextSection unchanged
   * (V4Game.Part2.cs:5972-5975). */
  else if (tok == "displaytext" || tok == "helpdisplaytext")
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
  else if (tok == "do")
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
	  /* Quest's ExecForEach takes three kinds of thing -- "object", "exit"
	   * and "room" -- and serves all three from the one _objs array, picking
	   * out the records whose IsRoom and IsExit flags match what was asked
	   * for (V4Game.cs:4969-5042).  So "for each object in" never sees a room
	   * or an exit.  geas keeps rooms in state.objs beside the objects (they
	   * are the records with is_room set) and does not model exits as objects
	   * at all, so the three cases split up here.  Only "object" used to be
	   * implemented, and it walked the rooms too. */
	  tok = next_token (s, c1, c2);
	  // SENSITIVE?
	  bool want_room = (tok == "room"), want_exit = (tok == "exit");
	  // SENSITIVE?
	  if ((tok == "object" || want_room || want_exit) &&
	      next_token (s, c1, c2) == "in")
	    {
	      tok = next_token (s, c1, c2);
	      string script = s.substr (c2);
	      // SENSITIVE?
	      if (tok == "game")
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
		  /* Quest compares lower-cased both sides, so the casing a move
		   * or a "parent" line happened to use does not matter. */
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
       * separately from objects.  Only below ASL 2.80, though: from there on
       * PlayerItem ignores the item table entirely and just moves the *object*
       * of that name into "inventory" (V4Game.Part2.cs:6594-6647), so keeping a
       * parallel item would outlive the object -- a 3.x game that gives the
       * stone and later drops it would still list it and still pass
       * "if got <stone>". */
      if (asl_version_ < 280)
	{
	  bool have_item = false;
	  for (const string &it: state.items)
	    if (ci_equal (it, tok)) { have_item = true; break; }
	  if (!have_item)
	    state.items.push_back (tok);
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
  // SENSITIVE?
  /* "hideobject"/"hidechar" are the Quest 2.x spellings of "hide". */
  else if (tok == "hide" || tok == "hideobject" || tok == "hidechar")
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
	gi->debug_print ("Expected param after conceal in " + s);
      return;
    }
  // SENSITIVE?
  /* "showobject"/"showchar" are the Quest 2.x spellings of "show". */
  else if (tok == "show" || tok == "showobject" || tok == "showchar")
    {
      tok = next_token (s, c1, c2);
      if (is_param(tok))
	{
	  string name = eval_param (tok);
	  if (!resolve_at_room (name))
	    return;
	  set_obj_property (name, "not hidden");
	}
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

      /* The mirror image of give: only from ASL 2.80 on does losing an item mean
       * moving the object of that name into the current room (V4Game.Part2.cs:
       * 6636-6647).  Below that, items and objects are separate tables and
       * "lose" is a flag on the item alone. */
      bool is_object = (asl_version_ >= 280 && state.obj_records (tok) != NULL);
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
  /* "helpmsg" is just "msg".  In Quest 4 it printed into the separate help
   * window; the Quest 5 player has no such window, so ExecuteScript sends it to
   * Print exactly like msg (V4Game.Part2.cs:5778-5781), and so does geas, which
   * likewise has one output stream.  It used to be a no-op here, which swallowed
   * ThunderClan Mystery 1's "***TYPER GET HERB***" prompt whole.  "helpclose"
   * and "helpclear" are the ones that really do nothing (ibid. 5782, 5988). */
  else if (tok == "msg" || tok == "helpmsg")
    {
      string stmt = tok;
      tok = next_token (s, c1, c2);
      if (is_param (tok))
	print_eval (param_contents(tok));
      else
	gi->debug_print ("Expected parameter after " + stmt + " in " + s);
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
  else if (tok == "nointro")
    {
      /* Suppress the automatic <intro> display that would otherwise follow the
       * startscript (see auto_intro_). */
      auto_intro_ = false;
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
      tok = next_token (s, c1, c2);
      // SENSITIVE?
      if (tok != "while" && tok != "until")
	{
	  gi->debug_print ("Expected while or until after repeat in " + s);
	  return;
	}
      bool is_while = (tok == "while");
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
      if (tok == "string" || tok == "numeric" || tok == "collectable")
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
      /* "stop" is FinishGame (StopType.Null) (V4Game.Part2.cs:5798-5801): the
       * game is over exactly as it is after playerwin/playerlose, only with no
       * win/lose text.  Clearing state.running alone left the host prompting
       * for commands that could no longer do anything. */
      state.running = false;
      is_running_ = false;
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
	// SENSITIVE?
	if (args[i] == "report")
	  do_report = true;
	else
	  gi->debug_print ("Got modifier " + args[i] + " after exists");
      //args[0] = lcase (args[0]);
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
      //tok = lcase (trim (eval_param (tok)));
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
	  /* room_of_parent walks the container chain, which is how Quest's
	   * separate ContainerRoom field behaves: DoAddRemove copies the
	   * container's ContainerRoom onto whatever is put inside it
	   * (V4Game.cs:2117-2121), so something in a carried container still
	   * counts as got -- as `here` already does below.  "Shipwrecked" turns
	   * on this: its box of matches only survives the swim to the third
	   * island inside the jar of honey, and the barrel of gunpowder that
	   * clears the rubble is lit by `if not got <Box of matches>`. */
	  return ci_equal (room_of_parent (o.parent), "inventory")
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
      //tok = lcase (trim (eval_param (tok)));
      tok = trim (eval_param (tok));
      if (const vector<size_t> *v = state.obj_records (tok))
	{
	  const ObjectRecord &o = state.objs[(*v)[0]];
	  /* "here" means present in this room AND not hidden -- present either
	   * directly or inside an open container/surface here (room_of walks the
	   * container chain).  An object removed with "hide" (e.g. a creature
	   * that has run off) is no longer here, even though its parent stands. */
	  return (ci_equal (room_of_parent (o.parent), state.location) &&
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
	// SENSITIVE?
	if (args[i] == "report")
	  do_report = true;
	else
	  gi->debug_print ("Got modifier " + args[i] + " after exists");
      //args[0] = lcase (args[0]);
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
  /* Quest's deprecated "gettag": the parameter of the first line in the named
   * ROOM block that begins with the given statement, with nested define blocks
   * skipped (FindStatement, V4Game.Part2.cs:6164-6189).  It is room-scoped even
   * for names that are also objects -- "gettag" predates the object namespace
   * (V4Game.cs:6896-6900) -- and a name with no room block yields "", because
   * DefineBlockParam hands back an empty block whose line range is empty.
   *
   * "The Devil's Bargain" ships a hand-rolled standard library that leans on it
   * throughout: a container is a room whose contents the player is walked into
   * with a silent goto, so its listing prefix is read back with
   * $gettag(<container>;prefix)$, and $gettag(<name>;look)$ returning the
   * literal "object" or "character" is how the library tells the two apart. */
  else if (pname == "gettag")
    {
      if (function_args.size() != 2)
	return bad_arg_count(pname);
      string room = trim (function_args[0]), tag = trim (function_args[1]);
      const GeasBlock *gb = gf.find_by_name ("room", room);
      if (gb == NULL)
	return "";
      /* readfile.cc rewrites a room's recognised tags as it reads them, so the
       * raw "prefix <...>" line is gone by now and the value has to come back
       * out of the parsed property (and action) lists instead.  Quest returns
       * the *first* matching line where these return the last assignment, which
       * only differs for a block that states the same tag twice. */
      string rv;
      if (gf.get_room_property (room, tag, rv) || rv != "!")
	return eval_string (rv);
      if (gf.get_room_action (room, tag, rv))
	return eval_string (rv);
      /* Anything readfile.cc did not recognise is still in the block verbatim
       * (a room's "suffix", say), so fall back to Quest's own line scan. */
      for (const string &line: gb->data)
	{
	  string l = lcase (trim (line));
	  if (l.compare (0, tag.length (), lcase (tag)) != 0)
	    continue;
	  /* GetParameter: from the first '<' to the first '>' (V4Game.cs:1865),
	   * then the usual #/%/$ substitutions. */
	  string::size_type lt = line.find ('<'), gt = line.find ('>');
	  if (lt == string::npos || gt == string::npos || gt < lt)
	    return "";
	  return eval_string (line.substr (lt + 1, gt - lt - 1));
	}
      return "";
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
   * hide <Vase>), so the bare item must not linger.  state.items is only
   * populated below ASL 2.80 -- see the "give" script command. */
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
     * (room_of_parent walks the container chain, starting from this record's
     * own parent so that two objects sharing a name are not both credited to
     * whichever room the first was defined in -- see regen_var_objects); the
     * hidden check, kept current by the visibility sweep, drops the contents of
     * closed containers. */
    if (ci_equal (room_of_parent (i.parent), room))
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

namespace {
  /* VB's Val(): the value of the leading numeric part of the string, 0 if it
   * does not start with a number. */
  double vb_val (const string &s)
  { return atof (trim (s).c_str ()); }

  /* VB's IsNumeric(): true when the whole string is a number.  Quest uses it to
   * decide whether a collectable operand is a literal or the name of another
   * collectable. */
  bool vb_is_numeric (const string &s)
  {
    string t = trim (s);
    if (t == "")
      return false;
    const char *p = t.c_str ();
    char *end = NULL;
    strtod (p, &end);
    return end != NULL && *end == '\0';
  }
}

void geas_implementation::set_up_collectables ()
{
  collectables.clear ();
  std::string::size_type c1, c2;

  for (const string &line: gf.block ("game", 0).data)
    {
      if (lcase (first_token (line, c1, c2)) != "collectables")
	continue;
      string tok = next_token (line, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print (nonparam ("collectables", line));
	  continue;
	}
      /* A second "collectables" line replaces the first rather than adding to
	 it: SetUpCollectables restarts its counter inside the loop over the game
	 block (V4Game.Part2.cs:6866). */
      collectables.clear ();
      /* Read with no variable substitution -- SetUpCollectables passes
	 convertStringVariables = false, since this runs before the game does. */
      string s = trim (param_contents (tok));
      if (s == "")
	continue;

      for (std::string::size_type pos = 0; ; )
	{
	  /* Items are separated by the first comma after the item's start, or by
	     a semicolon if the declaration has no commas at all -- Quest looks
	     for the comma first and only then for the semicolon, so a comma
	     inside a display string does split the list. */
	  std::string::size_type sep = s.find (',', pos + 1);
	  if (sep == string::npos)
	    sep = s.find (';', pos + 1);
	  bool last = (sep == string::npos);
	  if (last)
	    sep = s.length ();
	  string info = trim (s.substr (pos, sep - pos));

	  std::string::size_type sp1 = info.find (' ');
	  std::string::size_type ep = info.find ('=');
	  if (sp1 == string::npos || ep == string::npos || ep < sp1)
	    gi->debug_print ("Malformed collectable '" + info + "' in " + line);
	  else
	    {
	      CollectableDef cd;
	      cd.name = trim (info.substr (0, sp1));
	      std::string::size_type sp2 = info.find (' ', ep);
	      if (sp2 == string::npos)
		sp2 = info.length ();
	      cd.type = trim (info.substr (sp1 + 1, ep - sp1 - 1));
	      cd.init = vb_val (info.substr (ep + 1, sp2 - ep - 1));
	      /* A "d" in front of the type means "leave the line out of the
		 status pane while the value is zero". */
	      if (cd.type.length () > 0 && cd.type[0] == 'd')
		{
		  cd.type = cd.type.substr (1);
		  cd.display_when_zero = false;
		}
	      std::string::size_type obp = info.find ('[');
	      std::string::size_type cbp = info.find (']');
	      if (obp == string::npos || cbp == string::npos || cbp < obp)
		cd.display = "<def>";
	      else
		cd.display = trim (info.substr (obp + 1, cbp - obp - 1));
	      collectables.push_back (cd);
	    }

	  if (last)
	    break;
	  pos = sep + 1;
	}
    }

  for (const CollectableDef &cd: collectables)
    set_ivar (collectable_var (cd.name), (size_t) 0, cd.init);
}

bool geas_implementation::find_collectable (const string &name,
					    size_t &index) const
{
  for (size_t i = 0; i < collectables.size (); i ++)
    if (collectables[i].name == name)
      {
	index = i;
	return true;
      }
  return false;
}

bool geas_implementation::find_collectable_ci (const string &name,
					       size_t &index) const
{
  for (size_t i = 0; i < collectables.size (); i ++)
    if (ci_equal (collectables[i].name, name))
      {
	index = i;
	return true;
      }
  return false;
}

double geas_implementation::get_collectable (const string &name) const
{
  size_t index, n;
  if (find_collectable (name, index)
      && find_ivar (collectable_var (collectables[index].name), n))
    return state.ivars[n].getd (0);
  return 0.0;
}

void geas_implementation::set_collectable (size_t index, double value)
{
  const string &type = collectables[index].type;

  /* CheckCollectable: keep the value inside the range its type declares, so a
     game does not have to guard every "set <Health; +10>" itself. */
  if (type == "%" && value > 100.0)
    value = 100.0;
  if ((type == "%" || type == "p") && value < 0.0)
    value = 0.0;

  std::string::size_type rpos = type.find ('r');
  if (rpos != string::npos)
    {
      if (rpos == 0)
	{
	  /* "r50" -- maximum only.  Quest reads the number with
	     Mid(type, Len(type) - 1), a slip that happens to be right for a
	     two-digit bound and wrong for any other; it is reproduced here
	     because games were written against what it does. */
	  if (type.length () >= 2)
	    {
	      double max = vb_val (type.substr (type.length () - 2));
	      if (value > max)
		value = max;
	    }
	}
      else if (rpos == type.length () - 1)
	{
	  double min = vb_val (type.substr (0, type.length () - 1));
	  if (value < min)
	    value = min;
	}
      else
	{
	  double min = vb_val (type.substr (0, rpos));
	  double max = vb_val (type.substr (rpos + 1));
	  if (value > max)
	    value = max;
	  if (value < min)
	    value = min;
	}
    }

  set_ivar (collectable_var (collectables[index].name), (size_t) 0, value);
}

void geas_implementation::run_set_collectable (const string &param)
{
  std::string::size_type scp = param.find (';');
  if (scp == string::npos)
    {
      gi->debug_print ("No such collectable '" + param + "'");
      return;
    }
  string name = trim (param.substr (0, scp));
  string newval = trim (param.substr (scp + 1));

  size_t index;
  if (!find_collectable (name, index))
    {
      gi->debug_print ("No such collectable '" + param + "'");
      return;
    }

  string op = newval.substr (0, 1);
  string operand = trim (newval.substr (op.length ()));
  double val = vb_is_numeric (operand) ? vb_val (operand)
					: get_collectable (operand);
  double now = get_collectable (collectables[index].name);

  if (op == "+")
    set_collectable (index, now + val);
  else if (op == "-")
    set_collectable (index, now - val);
  else if (op == "=")
    set_collectable (index, val);
}

bool geas_implementation::eval_has (const string &param)
{
  std::string::size_type scp = param.find (';');
  if (scp == string::npos)
    {
      gi->debug_print ("No such collectable in " + param);
      return false;
    }
  string name = trim (param.substr (0, scp));
  string newval = trim (param.substr (scp + 1));

  size_t index;
  if (!find_collectable (name, index))
    {
      gi->debug_print ("No such collectable in " + param);
      return false;
    }

  string op = newval.substr (0, 1);
  string operand = trim (newval.substr (op.length ()));
  double check = vb_is_numeric (operand) ? vb_val (operand)
					 : get_collectable (operand);
  double now = get_collectable (collectables[index].name);

  /* "+" is strictly greater and "-" strictly less -- "if has <Money; +4999>"
     wants 5000, not 4999.  Anything else (including the "has <x;1>" that games
     write when they mean "= 1") is simply false. */
  if (op == "+")
    return now > check;
  if (op == "-")
    return now < check;
  if (op == "=")
    return now == check;
  return false;
}

string geas_implementation::collectable_display (size_t index) const
{
  const CollectableDef &cd = collectables[index];
  double value = get_collectable (cd.name);
  string display;

  if (cd.display == "<def>")
    display = "You have " + fmt_double (value) + " " + cd.name;
  else if (cd.display == "")
    display = "<null>";
  else
    {
      std::string::size_type ep = cd.display.find ('!');
      if (ep == string::npos)
	display = cd.display;
      else
	display = cd.display.substr (0, ep) + fmt_double (value)
	  + cd.display.substr (ep + 1);

      /* "*s*" is the plural: dropped when the value is exactly one. */
      std::string::size_type star = display.find ('*');
      if (star != string::npos)
	{
	  std::string::size_type star2 = display.find ('*', star + 1);
	  if (star2 == string::npos)
	    star2 = display.length ();
	  string between = display.substr (star + 1, star2 - star - 1);
	  display = display.substr (0, star)
	    + (value != 1.0 ? between : string (""))
	    + (star2 < display.length () ? display.substr (star2 + 1) : string (""));
	}
    }

  if (value == 0.0 && !cd.display_when_zero)
    display = "<null>";
  return display;
}

vstring geas_implementation::get_status_vars ()
{
  vstring rv;

  /* Below ASL 2.84 the status pane is the collectables, one line each, and
     "status <...>" variables do not exist yet (UpdateItems,
     V4Game.Part2.cs:7398-7420). */
  if (asl_version_ < 284 && !collectables.empty ())
    {
      for (size_t i = 0; i < collectables.size (); i ++)
	{
	  string line = collectable_display (i);
	  if (line != "<null>")
	    rv.push_back (line);
	}
      return rv;
    }

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

/* Quest's GetParameter finishes by evaluating any inline {expression} in the
   parameter it just built -- "msg <2 + 2 = {2+2}>" prints 4 -- with "{{" as the
   escape for a literal brace (EvaluateInlineExpressions, V4Game.cs:2258-2320,
   gated on ASL 3.91).  It applies to every parameter, so games use it to do
   arithmetic on object properties: YOU ARE A TIGER's whiskey shrinks with
   "property <jack_daniels; volume = { #jack_daniels:volume# - 10 }>", and geas,
   which stored the braces verbatim, could never empty the bottle.

   One deliberate difference: Quest replaces the *whole* parameter with the
   string "<ERROR>" when the expression does not evaluate, and its arithmetic is
   not ours -- so a brace geas cannot evaluate is left exactly as it was found
   rather than destroying the surrounding text. */
string geas_implementation::eval_inline_exprs (const string &s)
{
  if (asl_version_ < 391 || s.find ('{') == string::npos)
    return s;

  string rv;
  for (std::string::size_type i = 0; i < s.length(); i ++)
    {
      if (s[i] != '{')
	{
	  rv += s[i];
	  continue;
	}
      if (i + 1 < s.length() && s[i+1] == '{')     /* "{{" is a literal "{" */
	{
	  rv += '{';
	  i ++;
	  continue;
	}
      std::string::size_type close = s.find ('}', i + 1);
      if (close == string::npos)
	{
	  gi->debug_print ("eval_inline_exprs: expected } in '" + s + "'");
	  rv += s.substr (i);
	  break;
	}
      string expr = trim (s.substr (i + 1, close - i - 1)), val;
      char *num_end = NULL;
      if (expr != "")
	strtod (expr.c_str (), &num_end);
      if (eval_numeric_expr (expr, val))
	rv += val;
      else if (num_end != NULL && *num_end == '\0')
	rv += expr;      /* no operators: Quest hands the element back as it is */
      else
	{
	  gi->debug_print ("eval_inline_exprs: not an expression: '" + expr + "'");
	  rv += s.substr (i, close - i + 1);
	}
      i = close;
    }
  /* As Quest does after its own pass, "}}" collapses to "}". */
  for (std::string::size_type i = 0; (i = rv.find ("}}", i)) != string::npos; i ++)
    rv.erase (i, 1);
  return rv;
}

string geas_implementation::eval_string (const string &s)
{
  return eval_inline_exprs (eval_string_body (s));
}

string geas_implementation::eval_string_body (const string &s)
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
	  /* Quest replaces the *entire* parameter with "<ERROR>" when a conversion
	   * character has no partner (ConvertParameter, V4Game.cs:6697-6701), and
	   * logs it as a WarningError the player never sees.  Truncating at the
	   * stray character instead silently dropped the rest of the text. */
	  if (j == s.length())
	    {
	      gi->debug_print ("Line parameter <" + s + "> has missing #");
	      return "<ERROR>";
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
	      gi->debug_print ("Line parameter <" + s + "> has missing %");
	      return "<ERROR>";
	    }
	  if (j == i + 1)
	    rv += "%";
	  else
	    rv += fmt_double (get_dvar (s.substr (i+1, j-i-1)));
	  i = j;
	}
      /* "~name~" is a collectable's value.  The syntax belongs to Quest 2.x and
	 was dropped in ASL 3.20, from which version a ~ is just a ~
	 (ConvertParameter, V4Game.cs:1884-1893 and 6723-6726). */
      else if (s[i] == '~' && asl_version_ < 320)
	{
	  for (j = i + 1; j < s.length() && s[j] != '~'; j ++)
	    ;
	  if (j == s.length())
	    {
	      gi->debug_print ("Line parameter <" + s + "> has missing ~");
	      return "<ERROR>";
	    }
	  if (j == i + 1)
	    rv += "~";
	  else
	    rv += fmt_double (get_collectable (s.substr (i+1, j-i-1)));
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
	  string tmp = eval_string_body (tmp1);
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
	      /* The *last* right paren, as Quest's DoFunction does with
	       * InStrRev (V4Game.cs:6745): the argument list has already had its
	       * #vars# and %vars% substituted, so a value carrying parens of its
	       * own must not end it early.  Barbarian's unwield command computes
	       * $left(#command1#; %length1%)$ over "Battle axe (wielded)", and
	       * stopping at the first ')' cut the arg list down to one argument,
	       * so the function returned nothing and the unwielded weapon was
	       * never handed back. */
	      paren_close = tmp.rfind (')');
	      if (paren_close == string::npos || paren_close < paren_open)
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
      if (tr.timeleft != 0)
	tr.timeleft --;
      if (tr.timeleft == 0)
	{
	  /* Quest timers repeat every `interval` ticks until explicitly
	   * timeroff'd (a one-shot timer calls timeroff in its own action,
	   * directly or via a variable onchange).  Re-arm and keep running
	   * rather than stopping after the first firing -- otherwise
	   * counter timers (interval 1, action `dec <x>`) only fire once.
	   * An interval of 0 lands here every tick, as in Quest, where
	   * TimerTicks >= 0 is true immediately. */
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
		      due.push_back (line.substr (c2));
		      break;
		    }
		}
	    }
	}
    }

  for (const auto &script: due)
    run_script (script);
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
