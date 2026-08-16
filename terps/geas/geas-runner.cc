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
#include <unordered_map>
#include "geas-impl.hh"
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "general.hh"
#include "istring.hh"

/* Use the shared erkyrath_random() RNG (xoshiro128** when seeded, native
   otherwise), like scott/comprehend/plus/taylor.  The headless walkthrough
   runner links common_utils/randomness.c too, so a seeded run draws the same
   numbers there as in the app -- and, xoshiro128** being a fixed algorithm,
   the same numbers on any platform.  That is what lets the corpus transcripts
   in test/quest4/goldens be diffed at all. */
extern "C" {
#include "randomness.h"
}
#ifdef SPATTERLIGHT
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

/* Fire the onchange script of the variable block named for the changed
 * variable, if any.  The LAST "onchange" line in the block wins, since Quest's
 * loader overwrites the field on each one. */
void geas_implementation::run_onchange_script (const string &varname)
{
  for (size_t varn = 0; varn < gf.size("variable"); varn ++)
    {
      const GeasBlock &go (gf.block ("variable", varn));
      if (ci_equal (go.name, varname))
	{
	  string script = "";
	  std::string::size_type c1, c2;
	  for (uint j = 0; j < go.data.size(); j ++)
	    /* CI: Quest reads the block's keywords with BeginsWith, which
	     * lowercases (V4Game.Part2.cs:715). */
	    if (ci_equal (first_token (go.data[j], c1, c2), "onchange"))
	      script = trim (go.data[j].substr (c2 + 1));
	  if (script != "")
	    run_script (script);
	}
    }
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
    run_onchange_script (varname);
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
  /* Deferred regen: quest.objects/quest.formatobjects are rebuilt only when
   * one of them is read.  const_cast because the rebuild writes the two svars;
   * every reader observes exactly what eager regen would have shown it. */
  if (objs_vars_dirty_ && (ci_equal (varname, "quest.objects") ||
			   ci_equal (varname, "quest.formatobjects")))
    const_cast<geas_implementation *> (this)->regen_var_objects ();
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
    run_onchange_script (varname);
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
  objs_vars_dirty_ = true;
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

/* Quest keeps its player-error messages in a single array indexed by the
 * PlayerError enum (V4Game.Types.cs:287-326).  SetDefaultPlayerErrorMessages
 * fills it with the built-in texts (V4Game.Part2.cs:425-471) and
 * SetUpUserDefinedPlayerErrors then walks the game block overwriting entries
 * from "error <name; text>" lines (ibid. 1392-1625) -- so the LAST matching
 * line in the block wins, not the first.  error_table is that array in enum
 * order.  "id" is the name geas's own call sites pass to display_error; the
 * name a game writes in ASL is not always the same word (Quest spells
 * DefaultTake "defaultake"), which asl_error_index sorts out.  An entry marked
 * pcase is printed through print_eval_p, which capitalises the first letter
 * after substitution -- the message may open with a #quest.error.gender#
 * pronoun. */
namespace {

enum player_error {
  PE_BadCommand, PE_BadGo, PE_BadGive, PE_BadCharacter, PE_NoItem,
  PE_ItemUnwanted, PE_BadLook, PE_BadThing, PE_DefaultLook, PE_DefaultSpeak,
  PE_BadItem, PE_DefaultTake, PE_BadUse, PE_DefaultUse, PE_DefaultOut,
  PE_BadPlace, PE_BadExamine, PE_DefaultExamine, PE_BadTake, PE_CantDrop,
  PE_DefaultDrop, PE_BadDrop, PE_BadPronoun, PE_AlreadyOpen, PE_AlreadyClosed,
  PE_CantOpen, PE_CantClose, PE_DefaultOpen, PE_DefaultClose, PE_BadPut,
  PE_CantPut, PE_DefaultPut, PE_CantRemove, PE_AlreadyPut, PE_DefaultRemove,
  PE_Locked, PE_DefaultWait, PE_AlreadyTaken, PE_COUNT
};

struct error_entry { const char *id; const char *text; bool pcase; };

const error_entry error_table[] = {
  { "badcommand",     "I don't understand your command. Type HELP for a list of valid commands.", false },
  { "badgo",          "I don't understand your use of 'GO' - you must either GO in some direction, or GO TO a place.", false },
  { "badgive",        "You didn't say who you wanted to give that to.", false },
  { "badcharacter",   "I can't see anybody of that name here.",       false },
  { "noitem",         "You don't have that.",                         false },
  { "itemunwanted",   "#quest.error.gender# doesn't want #quest.error.article#.", true },
  { "badlook",        "You didn't say what you wanted to look at.",   false },
  { "badthing",       "I can't see that here.",                       false },
  { "defaultlook",    "Nothing out of the ordinary.",                 false },
  { "defaultspeak",   "#quest.error.gender# says nothing.",           true },
  { "baditem",        "I can't see that anywhere.",                   false },
  { "defaulttake",    "You pick #quest.error.article# up.",           false },
  { "baduse",         "You didn't say what you wanted to use that on.", false },
  { "defaultuse",     "You can't use that here.",                     false },
  { "defaultout",     "There's nowhere you can go out to around here.", false },
  { "badplace",       "You can't go there.",                          false },
  { "badexamine",     "You didn't say what you wanted to examine.",   false },
  { "defaultexamine", "Nothing out of the ordinary.",                 false },
  { "badtake",        "You can't take #quest.error.article#.",        false },
  { "cantdrop",       "You can't drop that here.",                    false },
  { "defaultdrop",    "You drop #quest.error.article#.",              false },
  { "baddrop",        "You are not carrying such a thing.",           false },
  { "badpronoun",     "I don't know what '#quest.error.pronoun#' you are referring to.", false },
  { "alreadyopen",    "It is already open.",                          false },
  { "alreadyclosed",  "It is already closed.",                        false },
  { "cantopen",       "You can't open that.",                         false },
  { "cantclose",      "You can't close that.",                        false },
  { "defaultopen",    "You open it.",                                 false },
  { "defaultclose",   "You close it.",                                false },
  { "badput",         "You didn't specify what you wanted to put #quest.error.article# on or in.", true },
  { "cantput",        "You can't put that there.",                    false },
  { "defaultput",     "Done.",                                        false },
  { "cantremove",     "You can't remove that.",                       false },
  { "alreadyput",     "It is already there.",                         false },
  { "defaultremove",  "Done.",                                        false },
  { "locked",         "The exit is locked.",                          false },
  { "defaultwait",    "Press a key to continue...",                   false },
  { "alreadytaken",   "You already have that.",                       false },
};
static_assert (sizeof error_table / sizeof error_table[0] == PE_COUNT,
	       "error_table must cover every PlayerError");

/* Quest's switch on the error name (V4Game.Part2.cs:1410-1610).  The name is
 * compared un-lowercased and un-trimmed, and -- the part that bites -- an
 * unrecognised name leaves currentError at 0, so the line overwrites
 * BadCommand instead of being ignored. */
int asl_error_index (const string &name, int asl_version)
{
  if (name == "defaultake")
    return asl_version <= 280 ? PE_BadTake : PE_DefaultTake;
  if (name == "badexamine")
    return asl_version >= 310 ? PE_BadExamine : PE_BadCommand;
  /* "defaulttake" -- the spelling geas's own call sites use -- is not a name
   * Quest's switch knows; a game that writes it is redefining badcommand. */
  if (name == "defaulttake")
    return PE_BadCommand;
  for (int i = 0; i < PE_COUNT; ++i)
    if (name == error_table[i].id)
      return i;
  return PE_BadCommand;
}

}

void geas_implementation::init_error_messages ()
{
  for (int i = 0; i < PE_COUNT; ++i)
    {
      error_messages_.push_back (error_table[i].text);
      error_pcase_.push_back (error_table[i].pcase);
    }

  const GeasBlock *game = gf.find_by_name ("game", "game");
  if (game == NULL)
    {
      gi->debug_print ("display_error: no 'game' block found");
      return;
    }
  bool examine_customised = false;
  std::string::size_type c1, c2;
  for (const string &line: game->data)
    {
      /* The keyword is CI -- Quest scans for it with BeginsWith
       * (SetUpUserDefinedPlayerErrors, V4Game.Part2.cs:1404). */
      string tok = first_token (line, c1, c2);
      if (!ci_equal (tok, "error"))
	continue;
      tok = next_token (line, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Bad error line: " + line);
	  continue;
	}
      string text = param_contents (tok);
      std::string::size_type index = text.find (';');
      if (index == string::npos)
	continue;
      /* Only the message is trimmed: Quest takes the name as
       * Left(errorInfo, scp - 1) with no Trim, so "error < badthing; ...>"
       * matches nothing and lands on badcommand. */
      int idx = asl_error_index (text.substr (0, index), asl_version_);
      string msg = trim (text.substr (index + 1));
      if (idx == PE_DefaultExamine)
	examine_customised = true;
      error_messages_[idx] = msg;
      error_pcase_[idx] = true;
      /* Setting the default look message sets the default examine message too,
       * unless an "error <defaultexamine; ...>" line has already been seen. */
      if (idx == PE_DefaultLook && !examine_customised)
	{
	  error_messages_[PE_DefaultExamine] = msg;
	  error_pcase_[PE_DefaultExamine] = true;
	}
    }
}

void geas_implementation::display_error (string errorname, string obj)
{
  display_error_info (errorname, obj, "");
}

void geas_implementation::display_error_info (string errorname, string obj,
					      const string &extra)
{
  GEAS_DBG << "display_error (" << errorname << ", " << obj << ")\n";
  if (obj != "")
    {
      string tmp;
      if (!get_obj_property (obj, "gender", tmp))
	tmp = "it";
      /* Capitalised: every quest.error.gender Quest writes goes through
       * UCase(Left(...)) or GetGender(..., capitalise: true)
       * (V4Game.Part2.cs:4790-4792, 5046-5047, 5094), so a custom message
       * reading it mid-sentence sees "He", not "he". */
      set_svar ("quest.error.gender", pcase (tmp));

      if (!get_obj_property (obj, "article", tmp))
	tmp = "it";
      set_svar ("quest.error.article", tmp);

      GEAS_DBG << "In erroring " << errorname << " / " << obj << ", qeg == "
	   << get_svar ("quest.error.gender") << ", qea == "
	   << get_svar ("quest.error.article") << endl;
      /* quest.error.charactername is deliberately not set here: Quest fills it
       * only in ExecGive's refusal path, so geas does too (see the give
       * handler in try_match). */
    }

  /* Quest has no error of this name: geas's generic verb dispatch needs a
   * refusal of its own, and a game's "error <defaultverb; ...>" line lands on
   * badcommand (asl_error_index), so this text is not overridable. */
  if (errorname == "defaultverb")
    {
      print_eval ("You can't do that.");
      return;
    }

  if (error_messages_.empty ())
    init_error_messages ();

  for (int i = 0; i < PE_COUNT; ++i)
    if (errorname == error_table[i].id)
      {
	string msg = eval_string (error_messages_[i]);
	if (error_pcase_[i])
	  msg = pcase (msg);
	if (extra != "")
	  {
	    if (msg.length () > 0 && msg[msg.length () - 1] == '.')
	      msg = msg.substr (0, msg.length () - 1);
	    msg = msg + " - " + extra + ".";
	  }
	print_formatted (msg);
	return;
      }
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

/* The parameter token of a "place [locked] <...>" line, with the optional
 * keyword consumed and `locked` set from it.
 *
 * AddExitFromTag strips a leading "locked " after the direction word, and
 * "place" is one of the direction words it accepts (RoomExits.cs:120-149), so a
 * place locks exactly as a compass direction does.  That parser is 4.10's: the
 * older loader hands the whole line to GetParameter, which scans for the "<" and
 * so walks straight past the keyword without noticing it (V4Game.Part2.cs:
 * 1216-1234).  The keyword is therefore eaten at every version -- otherwise the
 * line reads as malformed and the place disappears -- but only marks the exit
 * locked from 4.10 on, and below that the parameter keeps its older
 * "prefix; dest" reading, which is what Quest makes of it there. */
static string place_tag_param (const string &line, std::string::size_type &c1,
			       std::string::size_type &c2, int version,
			       bool &locked)
{
  string tok = next_token (line, c1, c2);
  locked = false;
  if (ci_equal (tok, "locked"))
    {
      locked = (version >= 410);
      tok = next_token (line, c1, c2);
    }
  return tok;
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

  std::string::size_type c1, c2;
  for (const auto &line: gb->data)
    {
      string tok = first_token (line, c1, c2);
      if (tok == "place")
	{
	  bool locked;
	  tok = place_tag_param (line, c1, c2, asl_version_, locked);
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
	  /* A locked place is still advertised in the exits line: locking gates
	   * the crossing and nothing else (RoomExit.Go, RoomExit.cs:240-259), so
	   * `locked` is of no interest here -- the "go to" handler asks
	   * exit_locked() about it.  From 4.10 the second field of the parameter
	   * is that exit's lock message rather than a prefix, which is why the
	   * split goes through split_exit_dest. */
	  string dest, prefix;
	  split_exit_dest (dest_param, dest, prefix);
	  string rest = trim (line.substr (c2));
	  /* The destination room's alias replaces its name only for a *plain*
	   * place, and only from ASL 3.11 up.  Both halves of that condition are
	   * one `&' in Quest, and it guards the listing (GetGoToExits,
	   * V4Game.Part2.cs:7790-7818) and the name the player has to type
	   * (PlaceExist, ibid. 6568-6594) alike:
	   *
	   *   if ((ASLVersion >= 311) & string.IsNullOrEmpty(Places[i].Script))
	   *
	   * So a scripted `place <elevatorroom> { ... }' shows -- and answers to --
	   * "elevatorroom" even when that room is aliased "elevator", which is what
	   * World's End's weapon store does; and below 3.11 a place is named by the
	   * tag whatever the destination is aliased, which is what saves Space, its
	   * <Science Lab> being a room its author aliased "Sciece Lab".  geas used
	   * to alias every kind at every version, so the listing advertised a name
	   * that GO TO then rejected.
	   *
	   * The room the player is *in* is unaffected: an alias always names that
	   * one (see regen_var_room). */
	  string displayed = (rest == "" && asl_version_ >= 311)
	    ? displayed_name (dest) : dest;
	  string printed_dest = (prefix != "" ? prefix + " " : "") +
	    "|b" + displayed + "|xb";
	  /* Below 2.80 the printed form is cut out of the tag's parameter by
	   * hand.  Quest's own cut has a fencepost error in it: ShowRoomInfoV2
	   * takes the prefix as Trim(Left(place, InStr(place, ";") - 1)) but the
	   * name as Right(place, Len(place) - (InStr(place, ";") + 1)), one
	   * character short, because that +1 assumes a space after the semicolon
	   * (V4Game.Part2.cs:1988-1999).  The usual `place <The; Library>'
	   * spelling has one and comes out right; "Magic Sword" writes `place
	   * <Your Training room;Training Room 1>' without one, and Quest really
	   * does offer to take the player to "Your Training room raining Room
	   * 1".  geas splits properly instead -- a DELIBERATE deviation: the
	   * mangled name is one Quest's own PlaceExist would accept from the
	   * player (it splits the parameter properly, ibid. 6568-6594, which is
	   * why "go to training room 1" works even there), so printing it whole
	   * only ever helps.  The oracle sweep carries the resulting Magic Sword
	   * diff as `deliberate:' -- see FINDINGS.md.  A parameter with no
	   * semicolon at all is printed whole, unTrimmed (ibid. 2000-2003). */
	  if (asl_version_ < 280)
	    {
	      std::string::size_type semi = dest_param.find (';');
	      if (semi == string::npos)
		printed_dest = "|b" + dest_param + "|xb";
	      else
		printed_dest = trim (dest_param.substr (0, semi)) + " |b"
		  + trim (dest_param.substr (semi + 1)) + "|xb";
	    }

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
      const string &line = i.dest;
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
	  /* A created place lands in the same Places array the tags fill, so
	   * GetGoToExits gates it on the version in just the same way. */
	  string displayed = (asl_version_ >= 311) ? displayed_name (args[1])
						  : args[1];
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
	    bool locked;
	    tok = place_tag_param (line, c1, c2, asl_version_, locked);
	    if (!is_param (tok))
	      continue;
	    string dest, prefix;
	    split_exit_dest (eval_param (tok), dest, prefix);
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

/* The LAST declaration of a direction wins: the pre-4.10 loader assigns the
 * room's field afresh on every "north " line (V4Game.Part2.cs:1042-1044),
 * and 4.10's AddExitFromTag re-uses and re-assigns the direction's existing
 * exit object (SetDirection, RoomExits.cs:20-33), so a later line overwrites
 * an earlier one either way.
 *
 * ...among the lines Quest reads at all: BeginsWith ("south ") needs the
 * trailing space, so a *bare* direction line assigns nothing and must be
 * skipped here too -- Lovesong's hall carries three stray "south" lines
 * under its real "south <yard>", and taking the last of those would wall
 * the house off. */
static const string *last_dir_decl (const GeasBlock *gb, const string &dir)
{
  std::string::size_type c1, c2;
  const string *decl = NULL;
  for (const string &l: gb->data)
    if (first_token (l, c1, c2) == dir && next_token (l, c1, c2) != "")
      decl = &l;
  return decl;
}

string geas_implementation::declared_exit_dest (const GeasBlock *gb,
						const string &dir,
						bool &is_script) const
{
  std::string::size_type c1, c2;
  is_script = false;
  const string *decl = last_dir_decl (gb, dir);
  if (decl == NULL)
    return "";
  const string &line = *decl;
  first_token (line, c1, c2);   /* just to set c2, the end of the keyword */
  std::string::size_type line_start = c2;
  string tok = next_token (line, c1, c2);
  /* Before 4.10 "out" is not read like the other directions.  Every other
   * direction goes through GetTextOrScript, which decides between the two by
   * looking at the line; "out" is cut in two at its first '>' and both
   * halves are kept -- Out.Text is GetParameter, the contents of the first
   * <...>, and Out.Script is everything after that '>'
   * (V4Game.Part2.cs:1018-1030).  So the parameter is always a destination,
   * whatever keyword stands in front of it: "out msg <You need to get up
   * first.>" leaves Out.Script empty, and GoDirection then hands the message
   * to PlayGame as a room name (ibid. 6324-6356), which prints nothing
   * because no room is called that.  UpdateDoorways advertises Out.Text
   * either way (ibid. 7218-7233), so the room still says "You can go out to
   * You need to get up first..".  The script half is the caller's business.
   *
   * From 4.10 on AddExitFromTag parses "out" with all the rest and a leading
   * keyword really does make the tag a script. */
  if (asl_version_ < 410 && ci_equal (dir, "out"))
    {
      while (tok != "" && !is_param (tok))
	tok = next_token (line, c1, c2);
      return is_param (tok) ? param_contents (tok) : "";
    }
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
  /* The last declaration wins, skipping bare direction lines -- see
   * last_dir_decl. */
  const string *decl = last_dir_decl (gb, dir);
  if (decl != NULL) {
    const string &line = *decl;
    first_token (line, c1, c2);
    if (!ci_equal (next_token (line, c1, c2), "locked"))
      return false;
    string tok = next_token (line, c1, c2);
    if (is_param (tok)) {
      vector<string> p = split_param (param_contents (tok));
      /* A directional exit that runs a script has no destination to spend the
       * first field on, so the whole parameter is the lock message
       * (RoomExits.cs:186-191).  A *place* exit keeps its destination there even
       * when scripted -- see the place scan below. */
      if (trim (line.substr (c2)) != "") {
	if (!p.empty ())
	  message = trim (p[0]);
      }
      else if (p.size () >= 2)
	message = trim (p[1]);
    }
    return true;
  }

  /* A directionless exit has no direction word to key on, so it answers to its
   * destination room's name, which is what FindExit looks a place up by
   * (V4Game.Part2.cs:7877-7913).  Only from 4.10: below it a room's places are
   * plain records with nothing to lock, and "lock" cannot reach them either. */
  if (asl_version_ >= 410)
    {
      bool found = false;
      for (const string &l: gb->data)
	{
	  if (first_token (l, c1, c2) != "place")
	    continue;
	  bool lk;
	  string tok = place_tag_param (l, c1, c2, asl_version_, lk);
	  if (!is_param (tok))
	    continue;
	  vector<string> p = split_param (param_contents (tok));
	  if (p.empty () || !ci_equal (trim (p[0]), dir))
	    continue;
	  /* The last declaration wins, as above. */
	  found = lk;
	  message = (lk && p.size () >= 2) ? trim (p[1]) : "";
	}
      if (found)
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

/* The right-hand side of a `set numeric <var; ...>`, evaluated as the game's own
 * ASL version does it.  From 3.91 ExecSetVar hands the text to
 * ExpressionHandler, which is the full precedence-respecting evaluator geas has
 * always used; below that it is one binary operation and nothing more
 * (V4Game.cs:7272-7343 -- see eval_double_pre391), so Kingdom's `set numeric
 * <x; 10*3/4>` is thirty there and seven and a half here. */
double geas_implementation::eval_set_numeric (const string &expr)
{
  if (asl_version_ >= 391)
    return eval_double (expr);
  /* ExecSetVar trims the text before it looks for the operator, and the search
   * for a "-" starts at the second character: with the space that follows the
   * semicolon still on the front, " -8-3" would split at the minus sign of the
   * negative number and answer -8 instead of -11. */
  bool div_by_zero = false;
  double rv = eval_double_pre391 (trim (expr), div_by_zero);
  if (div_by_zero)
    gi->debug_print ("Division by zero - The result of this operation has been "
		     "set to zero.");
  return rv;
}

void geas_implementation::look()
{
  string tmp;
  bool described = false;
  /* The look text is read fresh at every room display, not cached from the last
   * move: ShowRoomInfo asks GetObjectProperty("look", ...) each time it runs and
   * writes quest.lookdesc from it, falling back to the room's own tag
   * (V4Game.Part2.cs:3879-3893).  geas only refreshed it on arrival, so a script
   * that did `property <room; look=...>` while the player stood there left the
   * old text on screen until they walked out and back in -- Beam's !intproc116
   * rewrites the Inside Machine that way.  Quest computes it here, before the
   * description tag is found, so a description that changes the property still
   * gets the text as it was when the display began.
   *
   * The room's displayed name is the same story: ShowRoomInfo assembles alias,
   * prefix and suffix a few hundred lines above that (ibid. 3709-3730), so a
   * `property <room; alias=...>` shows on the very next display too. */
  regen_var_room ();
  regen_var_look ();
  /* The objects pair too, and eagerly, because this is the one writer that
   * punctuates: ShowRoomInfo fills quest.objects and quest.formatobjects a
   * couple of hundred lines above the point where it looks for a description
   * tag (V4Game.Part2.cs:3799-3830 against 3924-3956), so a described room sets
   * them just as an undescribed one does, and a description that prints
   * "#quest.formatobjects#" without disturbing anything first gets the " and ".
   * One that does disturb something -- a `show', a `create' -- raises the dirty
   * flag again and the next read rebuilds the pane's comma list, which is what
   * Quest's UpdateObjectList would have left behind. */
  regen_var_objects (true);
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
      bool has_in_desc = get_room_property (state.location, "indescription",
					    tmp);
      /* From ASL 3.50 the loader mirrors a room's tags into the object-property
       * table as well as into the room record, and AddToObjectProperties trims
       * whatever follows the "=" (V4Game.cs:4025) before writing it back over
       * InDescription (V4Game.cs:4111-4118).  So from 3.50 on the stored value
       * has no outer whitespace, and one made of nothing but spaces is stored
       * as the empty string.  TARDIS Escape's Console Room writes
       * `indescription < >` and depends on that without meaning to. */
      if (has_in_desc && asl_version_ >= 350)
	tmp = trim (tmp);
      /* What Quest tests is whether the stored text is empty, not whether the
       * tag was there (ShowRoomInfo, V4Game.Part2.cs:3746), so an `indescription
       * <>` takes the default line below. */
      if (has_in_desc && !tmp.empty ())
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
	/* The sentence ends in a full stop: "You are in " + roomDisplayName +
	 * "." (ShowRoomInfo, V4Game.Part2.cs:3764; the pre-2.80 display builds
	 * the same line at 2050). */
	print_formatted (string ("You are in") + " "
			 + get_svar ("quest.formatroom") + ".");
    }

  /* The object list and the exit lines belong to the *default* room display, and
   * a description tag replaces that display whole: ShowRoomInfo builds "You are
   * in ...", the object list and the three exit lines into one roomDisplayText
   * and prints it only when no description tag was found, then runs the tag
   * instead (V4Game.Part2.cs:3924-3956; the pre-2.80 path does the same at
   * 2088-2117).  A description that wants any of it prints it itself, which is
   * exactly what "Barbarian" and "The Maze" do -- both end their description
   * with `msg <You can go #quest.doorways#.>`, and geas adding its own copy gave
   * them the line twice.
   *
   * quest.formatobjects and the quest.doorways family are readable by game
   * script whether or not this prints them; the objects pair was regenerated
   * above, before the description tag ran. */
  if (!described)
    {
      /* The original Quest runner printed both of these inline in the main text
       * ("There is a key here.", "You can go north, south, east or west.") *and*
       * mirrored them in its objects and compass panes -- the panes were
       * clickable shortcuts, not a replacement for the lines.  So print them in
       * the main window even on a host that provides such panes (see
       * has_objects_window / get_room_exits); a pane stays a supplementary
       * copy. */
      tmp = get_svar ("quest.formatobjects");
      if (asl_version_ < 280)
	print_formatted (characters_display_);
      if (tmp != "")
	print_eval ("There is #quest.formatobjects# here.");

      /* The exits, after the objects.  From ASL 4.10 they are one folded line:
       * UpdateDoorways hands the whole job to
       * RoomExits.GetAvailableDirectionsDescription, which lists compass
       * exits (with "out" among them as a bare word) and places together in
       * the order the room's tags created them (V4Game.Part2.cs:7207-7211,
       * RoomExits.cs:304-353).  quest.doorways holds the list without the
       * sentence around it. */
      if (asl_version_ >= 410)
	{
	  if ((tmp = get_svar ("quest.doorways")) != "")
	    print_formatted ("You can go " + tmp + ".");
	}
      else
	{
	  /* From 2.80 the places line is part of roomDisplayText and the
	   * out/directions string is printed after it, so places come first
	   * (V4Game.Part2.cs:3838-3874, 3948-3955).  Below 2.80 the three are
	   * all part of roomDisplayText, in the other order (ShowRoomInfoV2,
	   * ibid. 2022, 2056, 2092) -- which is the order geas used at every
	   * version.
	   *
	   * The out and directions sentences are also one *line* from 2.80 on,
	   * not two: UpdateDoorways returns them as a single string with a space
	   * between (V4Game.Part2.cs:7308-7312, 7353) and ShowRoomInfo prints
	   * that string with one Print (ibid. 3951-3954).  Below 2.80 each ends
	   * in its own vbCrLf, so there they really are separate lines (ibid.
	   * 2022, 2056).  2.80 bolds the word "out" too, where the older path
	   * leaves it plain; both bold the directions, which is where
	   * quest.doorways.dirs already carries its own markup. */
	  bool places_first = (asl_version_ >= 280);
	  if (places_first && (tmp = get_svar ("quest.doorways.places")) != "")
	    print_formatted ("You can go to " + tmp + ".");
	  /* The *display* form of "out", so an "out <the; town>" prefix shows
	   * up here ("You can go out to the town."), as it does in the
	   * typelib's own version of this line and in the "places" line. */
	  string out_line = "", dirs_line = "";
	  if (get_svar ("quest.doorways.out") != ""
	      && (tmp = get_svar ("quest.doorways.out.display")) != "")
	    out_line = places_first ? "You can go |bout|xb to " + tmp + "."
				    : "You can go out to " + tmp + ".";
	  if ((tmp = get_svar ("quest.doorways.dirs")) != "")
	    dirs_line = "You can go " + tmp + ".";
	  if (places_first && out_line != "" && dirs_line != "")
	    print_formatted (out_line + " " + dirs_line);
	  else
	    {
	      if (out_line != "")
		print_formatted (out_line);
	      if (dirs_line != "")
		print_formatted (dirs_line);
	    }
	  if (!places_first && (tmp = get_svar ("quest.doorways.places")) != "")
	    print_formatted ("You can go to " + tmp + ".");
	}
    }

  /* ...and the punctuated pair lives no longer than the display that made it.
   * ShowRoomInfo's last act, after the default display has been printed or the
   * description tag has run, is a call to UpdateObjectList
   * (V4Game.Part2.cs:3974; ShowRoomInfoV2 does the same at 2156), which rebuilds
   * both strings as the pane's flat comma list.  So the " and " is readable from
   * inside a description tag and from nowhere else: a game that reads
   * quest.formatobjects on any later turn gets the comma list, even though
   * nothing has moved since.  Raising the flag rather than regenerating here
   * keeps that off the cost of walking into a room. */
  objs_vars_dirty_ = true;

  /* The room's "look" text is the one part of this that a description tag does
   * not always replace.  Below ASL 3.10 it is printed after the tag as well as
   * after the default display, and only from 3.10 on is a described room left to
   * print it itself: showLookText starts true and is cleared by
   * `descTagExist And ASLVersion >= 310` (V4Game.Part2.cs:3881-3927, 3958-3963).
   *
   * Below 2.80 there is no quest.lookdesc to read it back out of -- see
   * regen_var_look -- so the text comes from the copy that made. */
  if ((!described || asl_version_ < 310)
      && (tmp = (asl_version_ < 280) ? look_desc_
				     : get_svar ("quest.lookdesc")) != "")
    print_formatted (tmp);
}

/* Display one picture.  See the declaration for which tags land here with
 * `popup' set. */
void geas_implementation::show_picture (const string &spec, bool popup)
{
  string file = spec, res, caption;

  if (popup)
    {
      /* ShowPicture (V4Game.Part2.cs:3664-3690) takes the caption off first
       * and then looks for the size in what is left, so
       * "map.jpg@640x480; The Shire" yields all three -- and a ';' anywhere
       * after an '@' belongs to the caption, not to the size. */
      std::string::size_type semi = file.find (';');
      if (semi != string::npos)
	{
	  caption = trim (file.substr (semi + 1));
	  file = trim (file.substr (0, semi));
	}
      std::string::size_type at = file.find ('@');
      if (at != string::npos)
	{
	  res = trim (file.substr (at + 1));
	  file = trim (file.substr (0, at));
	}
    }

  /* Quest 4 used the caption as the title bar of the picture window rather
   * than as game text -- "If a caption is specified, that caption is used for
   * the window title, rather than the default 'Picture'" (Quest 4.1.5's own
   * quest.chm, script-script.htm).  Neither QuestViva nor geas has a window to
   * title: both draw the image among the text, so the only place left for the
   * author's caption is with it.  QuestViva prints it ahead of the image
   * (ibid. 3686) and geas follows, so that the caption survives a host that
   * cannot show pictures at all -- which is also what keeps the two
   * transcripts together. */
  if (caption != "")
    print_formatted (caption);
  gi->show_image (file, res, caption);
}

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
	  bool is_lib = (tok == "lib");      /* optional library-verb prefix */
	  if (is_lib)
	    tok = next_token (line, d1, d2);
	  if (is_lib != lib_pass)
	    continue;
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
      for (uint i = 0; i < gf.size ("procedure"); i ++)
	if (ci_equal (gf.block ("procedure", i).name, procname))
	  {
	    const GeasBlock &proc = gf.block ("procedure", i);
	    for (uint j = 0; j < proc.data.size (); j ++)
	      {
		std::string::size_type d1, d2;
		string line = proc.data[j];
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
      gi->debug_print ("No case block " + procname + " found");
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

/* $removeformatting(...)$: strip Quest's own "|" markup, code by code
 * (RemoveFormatting, V4Game.cs:121-220).  The table is the original's, in the
 * original's order -- the one-letter codes are tested before the "x" ones they
 * share a letter with, "|s" eats three characters (a point size) and every
 * colour is two.
 *
 * The stale-length quirk is the original's too: `len` is declared outside the
 * loop and only assigned by a branch that matches, so an unrecognised code
 * removes as many characters as the last recognised one did, not one.  It is
 * reproduced because a game written against Quest was written against that. */
static string remove_formatting (const string &s)
{
  static const struct { const char *code; int len; } codes[] =
    {{"b", 1}, {"xb", 2}, {"u", 1}, {"xu", 2}, {"i", 1}, {"xi", 2},
     {"cr", 2}, {"cb", 2}, {"cl", 2}, {"cy", 2}, {"cg", 2},
     {"n", 1}, {"xn", 2}, {"s", 3}, {"jc", 2}, {"jl", 2}, {"jr", 2},
     {"w", 1}, {"c", 1}};

  string rv = s;
  int len = 0;
  for (;;)
    {
      std::string::size_type pos = rv.find ('|');
      if (pos == string::npos)
	return rv;
      /* Mid(s, pos + 1, 3): the three characters after the bar, or fewer at
       * the end of the string. */
      string code = rv.substr (pos + 1, 3);
      for (const auto &c: codes)
	if (code.rfind (c.code, 0) == 0)
	  {
	    len = c.len;
	    break;
	  }
      if (len == 0)
	len = 1;
      rv = rv.substr (0, pos) + (pos + len + 1 <= rv.length ()
				 ? rv.substr (pos + len + 1) : "");
    }
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
  /* Built-in function names are case-sensitive, as in Quest: DoInternalFunction
   * compares each name with == on the un-lowercased string (V4Game.cs:6862 ff.),
   * so $UCase(...)$ is "No such function" there and here.  (geas is laxer than
   * Quest for *user-defined* names below -- Quest's block dictionary is
   * case-sensitive (DefineBlockParam, V4Game.cs:1195-1233), geas matches them
   * with ci_equal.) */
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
  else if (pname == "locationof")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      return get_obj_parent (function_args[0]);
    }
  else if (pname == "objectproperty")
    {
      if (function_args.size() != 2)
	return bad_arg_count(pname);

      string rv;
      get_obj_property (function_args[0], function_args[1], rv);
      return rv;
    }
  else if (pname == "timerstate")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      string timername = function_args[0];
      /* Timer names CI, like timeron/timeroff (V4Game.cs:7087). */
      for (const auto &timer: state.timers)
	if (ci_equal (timer.name, timername))
	  return timer.is_running ? "1" : "0";
      return "!";
    }
  else if (pname == "displayname")
    {
      if (function_args.size() != 1)
	return bad_arg_count (pname);

      return displayed_name (function_args[0]);
    }
  else if (pname == "capfirst")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      return pcase (function_args[0]);
    }
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
  else if (pname == "lcase")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      return lcase (function_args[0]);
    }
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
  else if (pname == "lengthof")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      return string_int (function_args[0].length());
    }
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
  else if (pname == "ucase")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      return ucase (function_args[0]);
    }
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

      /* Quest maps a float draw onto the range -- Int(NextDouble() * (b-a+1))
       * + a over .NET's Random (DoInternalFunction "rand",
       * V4Game.cs:6982-6988) -- so no mapping here can reproduce its values
       * and games cannot depend on specific draws.  The modulo mapping is kept
       * deliberately: the seeded walkthrough corpus was derived against this
       * exact draw sequence, and xoshiro128** has full-quality low bits, so
       * the classic low-bits worry does not apply. */
      return string_int (lower + (long long) (erkyrath_random() % range));
    }
  else if (pname == "speechenabled")
    {
      if (function_args.size() != 0)
	return bad_arg_count(pname);

      /* Deliberately 0 where QuestViva hardcodes "1" (V4Game.cs:7189):
       * GeasInterface::speak is r_not_supported in every host, so a game
       * branching on this would route its text into a no-op. */
      return "0";
    }
  else if (pname == "symbol")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      if (function_args[0] == "gt")
	return ">";
      else if (function_args[0] == "lt")
	return "<";
      gi->debug_print ("Bad symbol argument: " + function_args[0]);
      return "";
    }
  else if (pname == "numberparameters")
    return string_int (function_args.size());
  /* "thisobject" and "objectname" are the same thing -- the calling object's
   * real name -- and "thisobjectname" is its alias (V4Game.cs:7175-7183),
   * which Quest fills in with the name itself for an object that has none,
   * so it is displayed_name here. */
  else if (pname == "thisobject" || pname == "objectname")
    return this_object;
  else if (pname == "thisobjectname")
    return displayed_name (this_object);
  else if (pname == "timerinterval")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      /* Timer names CI and the miss value "!", as in timerstate above; the
       * leading space is VB's Str() and is Quest's too -- "ubound" trims its
       * result and this one does not (V4Game.cs:7118-7130). */
      for (const auto &timer: state.timers)
	if (ci_equal (timer.name, function_args[0]))
	  return " " + string_int (timer.interval);
      gi->debug_print ("No such timer '" + function_args[0] + "'");
      return "!";
    }
  else if (pname == "removeformatting")
    {
      if (function_args.size() != 1)
	return bad_arg_count(pname);

      return remove_formatting (function_args[0]);
    }
  /* An exit *object*, which is a 4.10 notion geas has no model for: its exits
   * are still the direction properties of a room.  Quest hands back the exit's
   * object name, or "" when there is none (V4Game.cs:7196-7205); "" is the
   * closer of the two answers to give, and below 4.10 the name is not a
   * function at all and falls through to the [ERROR] below. */
  else if (pname == "findexit" && asl_version_ >= 410)
    {
      gi->debug_print ("findexit is not supported");
      return "";
    }

  /* disconnectedby, id, name */

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
  /* A name that is neither a built-in nor a `define function` block prints a
   * marker rather than nothing: DoFunction logs "No such function" and returns
   * the literal "[ERROR]" (V4Game.cs:6763-6771).  Returning "" instead closed
   * the text up over the hole, which hid the author's typo from anyone reading
   * a transcript -- and hid it from this port's own corpus diffs. */
  gi->debug_print ("No function " + pname + " found.");
  return "[ERROR]";
}

v2string geas_implementation::get_inventory ()
{
  /* Below ASL 2.80 the inventory *is* the item table, and objects have nothing
   * to do with it: both the "inventory" command and the inventory pane walk
   * _items from 1 to _numberItems and take every entry whose Got flag is set
   * (V4Game.Part2.cs:4526-4535 and 7409-7417).  So the order is the order the
   * game block's `items <...>' line declares, not the order the player picked
   * things up in, and an object that happens to be filed under "inventory" is
   * not listed at all. */
  if (asl_version_ < 280)
    {
      v2string rv;
      for (const string &decl: item_table_)
	{
	  bool held = false;
	  for (const string &it: state.items)
	    if (it == decl) { held = true; break; }
	  if (held)
	    {
	      vstring tmp;
	      tmp.push_back (decl);
	      tmp.push_back ("object");
	      rv.push_back (tmp);
	    }
	}
      return rv;
    }
  return get_room_contents ("inventory");
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
    /* Things filed under this room -- which includes whatever is inside a
     * container here, since a container's contents share its room.  This
     * record's own field, so that two objects sharing a name are not both
     * credited to whichever room the first was defined in (see
     * regen_var_objects); the hidden check, kept current by the visibility
     * sweep, drops the contents of closed containers. */
    if (ci_equal (i.parent, room))
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

  /* The "out" exit, with the place it leads to when one is named.  Split here
   * rather than read back from quest.doorways.out: that variable is a pre-4.10
   * one and a 4.10 game never sets it (see regen_var_dirs), while this pane is
   * ours and is drawn at every version. */
  if (exit_dest (state.location, "out") != "")
    {
      string out, prefix, raw = exit_dest (state.location, "out");
      if (raw.find ('<') != string::npos)
	raw = "";   /* an "out { ... }" script is not a destination */
      split_exit_dest (raw, out, prefix);
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

void geas_implementation::set_up_items ()
{
  item_table_.clear ();
  std::string::size_type c1, c2;
  /* Quest declares its "have we reached the end of the list?" flag once, ahead
     of the loop over the game block, and never resets it (SetUpItemArrays,
     V4Game.Part2.cs:6963).  So a second items/possitems line contributes only
     its first entry before the do/while falls straight out again -- a VB6 bug,
     but one a game could be relying on, so keep it. */
  bool last_item = false;

  for (const string &line: gf.block ("game", 0).data)
    {
      string keyword = lcase (first_token (line, c1, c2));
      /* Two spellings of the same declaration, both read into the same table
	 and both appended to it: unlike the collectables, a second line adds
	 to the list rather than replacing it. */
      if (keyword != "items" && keyword != "possitems")
	continue;
      string tok = next_token (line, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print (nonparam (keyword, line));
	  continue;
	}
      /* Read with no variable substitution, as the collectables are: this runs
	 before the game does. */
      string s = trim (param_contents (tok));
      if (s == "")
	continue;

      for (std::string::size_type pos = 0; ; )
	{
	  /* The same separator rule as the collectables: the first comma after
	     the entry's start, or a semicolon when the rest of the line holds
	     no comma at all.  King of Wallwood Wood writes its possitems with
	     commas, everything else in the corpus with semicolons. */
	  std::string::size_type sep = s.find (',', pos + 1);
	  if (sep == string::npos)
	    sep = s.find (';', pos + 1);
	  if (sep == string::npos)
	    {
	      sep = s.length ();
	      last_item = true;
	    }
	  item_table_.push_back (trim (s.substr (pos, sep - pos)));
	  pos = sep + 1;
	  if (last_item)
	    break;
	}
    }
}

const string *geas_implementation::find_item (const string &name) const
{
  for (const string &it: item_table_)
    if (it == name)
      return &it;
  return NULL;
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
	  /* The block's keywords are CI (BeginsWith, V4Game.Part2.cs:704-728);
	   * the "type string" *value* is not -- Quest compares the raw text
	   * (ibid. 707), so "type String" is an unrecognised type there and
	   * here. */
	  if (ci_equal (tok, "display"))
	    {
	      tok = next_token (line, c1, c2);

	      if (ci_equal (tok, "nozero"))
		{
		  nozero = true;
		  tok = next_token (line, c1, c2);
		}
	      if (!is_param (tok))
		gi->debug_print ("Expected param after display: " + line);
	      else
		disp = tok;
	    }
	  else if (ci_equal (tok, "type"))
	    {
	      tok = next_token (line, c1, c2);
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
	      /* A property the object hasn't got expands to "!", not to nothing:
	       * GetStringContents reaches GetObjectProperty with its default
	       * logError = true, which logs the miss and returns "!"
	       * (V4Game.Part2.cs:2581, V4Game.cs:6249-6252).  stdverbs.lib makes
	       * this visible everywhere -- an object with no `article` answers
	       * "You can't buy !.". */
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
		    {
		      gi->debug_print ("e_s: Evaluating nonexistent object prop "
				       "{" + objname + "}:{" + propname + "}");
		      rv += "!";
		    }
		}
	      else
		{
		  string objname = s.substr (i+1, k-i-1);
		  string tmp;
		  if (get_obj_property (objname, propname, tmp))
		    rv += tmp;
		  else
		    {
		      gi->debug_print ("e_s: Evaluating nonexistent var " + objname);
		      rv += "!";
		    }
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
	  /* Quest applies ConvertParameter once per conversion character, with
	   * one body for all four, so an unpartnered '$' replaces the whole
	   * parameter with "<ERROR>" exactly as an unpartnered '#', '%' or '~'
	   * does (V4Game.cs:6697-6701).  Passing the rest of the string through
	   * instead was the one arm left over. */
	  if (j == string::npos)
	    {
	      gi->debug_print ("Line parameter <" + s + "> has missing $");
	      return "<ERROR>";
	    }
	  /* "$$" is the escape for a literal dollar, the same rule the other three
	   * conversion characters follow: ConvertParameter takes the text between
	   * the pair, finds it empty and appends the character itself
	   * (V4Game.cs:6705-6708).  Evaluating the empty name as a function
	   * instead swallowed the pair whole -- Pure Chaos writes its prices as
	   * "$$1000000" and the player was shown "1000000". */
	  if (j == i + 1)
	    {
	      rv += "$";
	      i = j;
	      continue;
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
      tr.elapsed ++;
      if (tr.elapsed >= tr.interval)
	{
	  /* Quest timers repeat every `interval` ticks until explicitly
	   * timeroff'd (a one-shot timer calls timeroff in its own action,
	   * directly or via a variable onchange).  Zero the count and keep
	   * running rather than stopping after the first firing -- otherwise
	   * counter timers (interval 1, action `dec <x>`) only fire once.
	   * An interval of 0 lands here every tick, as in Quest, where
	   * TimerTicks >= 0 is true immediately.
	   *
	   * The count is zeroed *before* the action runs, and the comparison
	   * above reads the interval as it stands on the tick, which is what
	   * lets `set interval' reach the cycle already in flight -- from
	   * inside the timer's own action and from outside it alike
	   * (V4Game.Part2.cs:126-131). */
	  tr.elapsed = 0;
	  const GeasBlock *gb = gf.find_by_name ("timer", tr.name);
	  if (gb != NULL)
	    {
	      std::string::size_type c1, c2;
	      string action;
	      bool have_action = false;
	      for (const auto &line: gb->data)
		{
		  string tok = first_token (line, c1, c2);
		  /* CI: BeginsWith "action " -- and the LAST action line wins,
		   * since SetUpTimers overwrites TimerAction on each one
		   * (V4Game.Part2.cs:1337-1340). */
		  if (ci_equal (tok, "action"))
		    {
		      action = line.substr (c2);
		      have_action = true;
		    }
		}
	      if (have_action)
		due.push_back (action);
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

/* The size in a |sNN code: Quest reads the two characters after the "s" with
 * int.TryParse (TextFormatter.cs:170), which takes an optional sign and then
 * digits, and nothing else -- so "09" and "-1" are sizes and "9|" is not.  The
 * old code parsed "|s" plus the first digit instead of the two digits, so it
 * never read a size at all; nothing in a transcript shows the difference,
 * since the size lands in a Glk style rather than in the text.
 *
 * int.TryParse's default is NumberStyles.Integer, which also skips whitespace
 * at either end, and that half is visible: ChristmaKwanzakkah's title banner
 * ends `|cr! |s0 |cb |xb', whose two characters after the "s" are "0" and a
 * space.  Quest reads that as a size and eats the code; a stricter reader
 * calls it malformed and prints a literal "|s0" in the middle of the game's
 * first line (finding 71). */
static bool parse_two_char_int (const string &s, int &rv)
{
  size_t b = 0, e = s.length ();
  while (b < e && isspace ((unsigned char) s[b]))
    b ++;
  while (e > b && isspace ((unsigned char) s[e - 1]))
    e --;
  string t = s.substr (b, e - b);
  if (t.empty ())
    return false;
  size_t d = (t[0] == '-' || t[0] == '+') ? 1 : 0;
  for (size_t k = d; k < t.length (); k ++)
    if (!(t[k] >= '0' && t[k] <= '9'))
      return false;
  if (d == t.length ())
    return false;
  rv = parse_int (t.substr (d));
  if (t[0] == '-')
    rv = -rv;
  return true;
}

GeasResult GeasInterface::print_formatted (const string &s, bool with_newline)
{
  std::string::size_type i, j;

  /* Quest prints a string in pieces rather than all at once: Print walks it a
   * character at a time, accumulating into `printString', and a "|w" or a
   * clearing "|c" hands what has accumulated to DoPrint and starts a fresh
   * string (V4Game.Part2.cs:6745-6784).  Every DoPrint ends its line, so the
   * codes are paragraph breaks as well as pauses -- the text on either side of
   * one is never on the same line.  The closing DoPrint is the only guarded
   * one, `If printString <> "" ' (ibid. 6794-6797), so a string that ends with
   * one of the two codes has already had its last line ended and does not get
   * another.  That is what `pending' is: printString <> "".  It starts true so
   * that Print's own empty-string case, a bare DoPrint("") (ibid. 6742-6745),
   * still ends a line here. */
  bool pending = true;

  for (i = 0; i < s.length(); i ++)
    {
      /* Anything but the two flushing codes below is a character appended to
       * printString -- the formatting codes among them included, since Quest
       * hands those to DoPrint too and lets TextFormatter eat them there. */
      pending = true;
      if (s[i] == '|')
        {
          // changed indicated whether cur_style has been changed
          // and update_style should be called at the end.
          // it is true unless cleared (by |n or |w).
          bool changed = true;
	  /* Set by any arm that turns out not to be a code after all.  Quest
	   * tries the two-character codes, then the one-character ones, then
	   * |sNN, and if none matches it emits a literal "|" and carries on
	   * reading from the character right after the bar, which is then
	   * ordinary text (TextFormatter.cs:161-176).  This used to fall off
	   * the end of the switch with the index already past the code
	   * character, so the loop's own i++ ate it too: The Lazy Gun Cult's
	   * |K| sigil and |plaque| emphasis both lost two characters a bar,
	   * which ran the surrounding words together. */
	  bool literal_bar = false;
          j = i;
          i ++;
          if (i == s.length())
            {
	      /* A bar with nothing after it is a code that matched nothing. */
	      print_normal ("|");
              continue;
            }

	  /* The | codes are case-sensitive: Quest's TextFormatter switches on
	     the raw one- and two-character code. */
          switch (s[i])
            {
            case 'u': cur_style.is_underlined = true; break;
            case 'i': cur_style.is_italic     = true; break;
            case 'b': cur_style.is_bold       = true; break;
            case 'c':
              i ++;

	      /* The clearing "|c" -- the one that is not "|cb", "|cr", "|cl",
	       * "|cy" or "|cg" -- flushes what has been printed so far before it
	       * wipes, exactly as "|w" does above (V4Game.Part2.cs:6761-6784). */
              if (i == s.length())
		{ print_newline(); pending = false; clear_screen(); break; }

              switch (s[i])
                {
                case 'y': cur_style.color = "#ffff00"; break;
                case 'g': cur_style.color = "#00ff00"; break;
                case 'l': cur_style.color = "#0000ff"; break;
                case 'r': cur_style.color = "#ff0000"; break;
                case 'b': cur_style.color = "";  break;

                default:
		  print_newline();
		  pending = false;
                  clear_screen();
                  --i;
                }
              break;

            case 's':
	      {
		/* |sNN wants exactly two characters after the "s", and they
		 * have to parse as a number: Quest reads them with
		 * int.TryParse, under a guard that there are two of them
		 * (TextFormatter.cs:165-176).  A malformed one -- Uranus opens
		 * with a stray |s9| -- is not a code, and prints as itself. */
		int newsize;
		if (i + 2 >= s.length ()
		    || !parse_two_char_int (s.substr (i + 1, 2), newsize))
		  {
		    literal_bar = true;
		    break;
		  }
		i += 2;
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
                {
		  /* Same as |sNN: "j" on its own is not one of Quest's
		   * one-character codes, so a malformed |jX is a literal
		   * bar. */
		  literal_bar = true;
		  break;
		}
              if (s[i] == 'l') cur_style.justify = JUSTIFY_LEFT;
              else if (s[i] == 'r') cur_style.justify = JUSTIFY_RIGHT;
              else if (s[i] == 'c') cur_style.justify = JUSTIFY_CENTER;
              break;

            case 'n':
              print_newline();
              changed = false;
              break;

            case 'w':
              /* The flush comes first and unconditionally: Quest's DoPrint runs
               * before DoWaitAsync, and it runs even with nothing accumulated,
               * so a string that opens with the code opens with a blank line. */
              print_newline();
              pending = false;
              wait_keypress("");
              /* A wait, and so a suspension: the turn's timers tick here
               * (see geas_implementation::run_command). */
              if (runner != NULL)
                runner->turn_suspended();
              changed = false;
              break;

            case 'x':
              i ++;

	      if (i == s.length())
		literal_bar = true;
              else if (s[i] == 'b')
                cur_style.is_bold = false;
              else if (s[i] == 'u')
                cur_style.is_underlined = false;
              else if (s[i] == 'i')
                cur_style.is_italic = false;
              else if (s[i] == 'n' && i + 1 == s.length())
                changed = with_newline = false;
	      else
		/* xb, xi and xu are the only two-character x codes; |xn is
		 * read off the end of the whole string before formatting
		 * starts (TextFormatter.cs:29-33), so one in the middle is not
		 * a code either. */
		literal_bar = true;
              break;

            default:
              GEAS_DBG << "p_f: Fallthrough " << s[i] << std::endl;
              changed = false;
	      literal_bar = true;
            }
	  if (literal_bar)
	    {
	      print_normal ("|");
	      /* Resume at the bar, so the loop's i++ lands on the character
	       * after it and the text branch prints it. */
	      i = j;
	      continue;
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
  if (with_newline && pending)
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
