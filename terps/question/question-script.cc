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

/* question-script.cc -- Script execution: apply_type, run_script (which
 * finds a statement's handler; the exec_<statement> handlers themselves are
 * in question-script-flow.cc, -world.cc and -output.cc), plus condition
 * evaluation (eval_cond/eval_conds).
 *
 * Part of question_implementation; question-runner.cc holds the rest of the
 * preamble and question-internal.hh what these units share. */

#include "question-internal.hh"

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

void question_implementation::apply_type (const string &obj, const string &typenm)
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

void question_implementation::run_script_as (const string &obj, const string &scr)
{
  string backup_object, garbage;
  backup_object = this_object;
  this_object = obj;
  run_script (scr, garbage);
  this_object = backup_object;
}

void question_implementation::run_script (const string &s)
{
  string garbage;
  run_script (s, garbage);
}

bool question_implementation::next_param (const string &line,
					  std::string::size_type &c1,
					  std::string::size_type &c2,
					  string &tok, const char *complaint)
{
  tok = next_token (line, c1, c2);
  if (is_param (tok))
    return true;
  gi->debug_print (complaint + line);
  return false;
}

void question_implementation::run_script (const string &s, string &rv)
{
  QUESTION_DBG << "Script line '" << s << "'\n";
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
      /* rv is passed down so a `return' inside a braced block still sets the
       * enclosing function's result ("if ... then { return <a> } else { ... }"). */
      if (brace2 >= brace1)
	run_script (s.substr (brace1, brace2 - brace1), rv);
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
   * statement.  The table maps every accepted spelling to its handler, an
   * exec_<statement> method in question-script-flow.cc, -world.cc or
   * -output.cc.  A handler returns false when it did not deal with the line,
   * which is reported as an unrecognised script just as an unknown keyword is. */
  typedef bool (question_implementation::*StmtFn)
    (const string &, string &, string, std::string::size_type,
     std::string::size_type);
  static const std::unordered_map<string, StmtFn> stmt_table = {
    {"action", &question_implementation::exec_action},
    {"add", &question_implementation::exec_add_remove},
    {"animate", &question_implementation::exec_animate},
    {"background", &question_implementation::exec_colour},
    {"choose", &question_implementation::exec_choose},
    {"clear", &question_implementation::exec_clear},
    {"clone", &question_implementation::exec_clone},
    {"close", &question_implementation::exec_open_close},
    {"conceal", &question_implementation::exec_reveal_conceal},
    {"concealchar", &question_implementation::exec_reveal_conceal},
    {"concealobject", &question_implementation::exec_reveal_conceal},
    {"create", &question_implementation::exec_create},
    {"debug", &question_implementation::exec_debug},
    {"dec", &question_implementation::exec_inc_dec},
    {"destroy", &question_implementation::exec_destroy},
    {"disconnect", &question_implementation::exec_disconnect},
    {"displaytext", &question_implementation::exec_displaytext},
    {"do", &question_implementation::exec_do},
    {"doaction", &question_implementation::exec_doaction},
    {"dontprocess", &question_implementation::exec_dontprocess},
    {"enter", &question_implementation::exec_enter},
    {"exec", &question_implementation::exec_exec},
    {"flag", &question_implementation::exec_flag},
    {"font", &question_implementation::exec_ignored},
    {"for", &question_implementation::exec_for},
    {"foreground", &question_implementation::exec_colour},
    {"give", &question_implementation::exec_give},
    {"goto", &question_implementation::exec_goto},
    {"helpclear", &question_implementation::exec_ignored},
    {"helpclose", &question_implementation::exec_ignored},
    {"helpdisplaytext", &question_implementation::exec_displaytext},
    {"helpmsg", &question_implementation::exec_msg},
    {"hide", &question_implementation::exec_hide},
    {"hidechar", &question_implementation::exec_hide},
    {"hideobject", &question_implementation::exec_hide},
    {"if", &question_implementation::exec_if},
    {"inc", &question_implementation::exec_inc_dec},
    {"lock", &question_implementation::exec_lock_unlock},
    {"lose", &question_implementation::exec_lose},
    {"mailto", &question_implementation::exec_ignored},
    {"modvolume", &question_implementation::exec_ignored},
    {"move", &question_implementation::exec_move},
    {"movechar", &question_implementation::exec_move},
    {"moveobject", &question_implementation::exec_move},
    {"msg", &question_implementation::exec_msg},
    {"msgto", &question_implementation::exec_ignored},
    {"nointro", &question_implementation::exec_nointro},
    {"open", &question_implementation::exec_open_close},
    {"outputoff", &question_implementation::exec_outputoff},
    {"outputon", &question_implementation::exec_outputon},
    {"panes", &question_implementation::exec_ignored},
    {"pause", &question_implementation::exec_pause},
    {"picture", &question_implementation::exec_picture},
    {"playerlose", &question_implementation::exec_playerwin_lose},
    {"playerwin", &question_implementation::exec_playerwin_lose},
    {"playmidi", &question_implementation::exec_play_sound},
    {"playmod", &question_implementation::exec_play_sound},
    {"playwav", &question_implementation::exec_play_sound},
    {"property", &question_implementation::exec_property},
    {"remove", &question_implementation::exec_add_remove},
    {"repeat", &question_implementation::exec_repeat},
    {"return", &question_implementation::exec_return},
    {"reveal", &question_implementation::exec_reveal_conceal},
    {"revealchar", &question_implementation::exec_reveal_conceal},
    {"revealobject", &question_implementation::exec_reveal_conceal},
    {"say", &question_implementation::exec_say},
    {"select", &question_implementation::exec_select},
    {"set", &question_implementation::exec_set},
    {"setstring", &question_implementation::exec_setvar},
    {"setvar", &question_implementation::exec_setvar},
    {"shell", &question_implementation::exec_ignored},
    {"shellexe", &question_implementation::exec_ignored},
    {"show", &question_implementation::exec_show},
    {"showchar", &question_implementation::exec_show},
    {"showobject", &question_implementation::exec_show},
    {"speak", &question_implementation::exec_speak},
    {"stop", &question_implementation::exec_stop},
    {"timeroff", &question_implementation::exec_timer},
    {"timeron", &question_implementation::exec_timer},
    {"type", &question_implementation::exec_type},
    {"unlock", &question_implementation::exec_lock_unlock},
    {"wait", &question_implementation::exec_wait},
    {"with", &question_implementation::exec_ignored}
  };
  auto stmt_it = stmt_table.find (tok);
  if (stmt_it != stmt_table.end ()
      && (this->*stmt_it->second) (s, rv, tok, c1, c2))
    return;
  gi->debug_print ("Unrecognized script " + s);
}

bool question_implementation::eval_conds (const string &s)
{
  QUESTION_DBG << "if (" + s + ")" << endl;

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

  QUESTION_DBG << "if (" << s << ") --> " << (rv ? "true" : "false") << endl;
  return rv;
}

/* One side of an "is <a;b>" or "is <a;!=;b>" comparison, as Quest reads it.
 * From ASL 3.91 on ExecuteIfIs runs both operands through ExpressionHandler and
 * uses the result whenever the operand is arithmetic all the way through
 * (V4Game.cs:7479-7494), so "is <%diceout%+1;%totaldice%>" compares 2 with 2
 * rather than the string "1+1" with the string "2".  Anything that is not
 * arithmetic fails the expression and is compared verbatim, which is what the
 * rest of the games rely on; older games never evaluate either side. */
std::string question_implementation::eval_is_operand (const string &s) const
{
  string out;
  if (asl_version_ >= 391 && eval_numeric_expr (s, out))
    return out;
  return s;
}

bool question_implementation::eval_cond (const string &s)
{
  std::string::size_type c1, c2;
  string tok = first_token (s, c1, c2);
  /* Condition keywords (not/got/here/is/property/real/type/...) are
   * case-insensitive; fold the dispatch token once (see run_script). */
  tok = lcase (tok);
  if (tok == "not")
    return !eval_cond (s.substr (c2));
  else if (tok == "action" || tok == "property")
    {
      bool action = (tok == "action");
      if (!next_param (s, c1, c2, tok, "expected parameter after property in "))
	return false;
      tok = eval_param (tok);
      std::string::size_type index = tok.find (';');
      if (index == string::npos)
	{
	  gi->debug_print ("Only one argument to property in " + s);
	  return false;
	}
      string obj = trim (tok.substr (0, index));
      string name = trim (tok.substr (index+1));
      return action ? has_obj_action (obj, name) : has_obj_property (obj, name);
    }
  else if (tok == "ask")
    {
      if (!next_param (s, c1, c2, tok, "expected parameter after ask in "))
	return false;
      tok = eval_param (tok);
      return gi->choose_yes_no (tok);
    }
  else if (tok == "exists" || tok == "real")
    {
      /* "real" asks only whether the object was ever defined; "exists" also
       * wants it placed and not hidden. */
      const string cond = tok;
      if (!next_param (s, c1, c2, tok,
		       cond == "real" ? "expected parameter after real in "
				      : "expected parameter after exists in "))
	return false;
      vector<string> args = split_param (eval_param (tok));
      bool do_report = false;
      for (uint i = 1; i < args.size(); i ++)
	/* CI + trimmed: Quest compares LCase(Trim(...)) == "report"
	 * (V4Game.cs:5905). */
	if (ci_equal (trim (args[i]), "report"))
	  do_report = true;
	else
	  gi->debug_print ("Got modifier " + args[i] + " after " + cond);
      if (const vector<size_t> *v = state.obj_records (args[0]))
	{
	  if (cond == "real")
	    return true;
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
	gi->debug_print (cond + " " + args[0] + " failed due to nonexistence");
      return false;
    }
  else if (tok == "flag")
    {
      if (!next_param (s, c1, c2, tok, "expected parameter after flag in "))
	return false;
      tok = trim (eval_param (tok));
      return has_obj_property ("game", tok);
    }
  else if (tok == "got")
    {
      if (!next_param (s, c1, c2, tok, "expected parameter after got in "))
	return false;
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
      if (!next_param (s, c1, c2, tok, "expected parameter after has in "))
	return false;
      return eval_has (eval_param (tok));
    }
  else if (tok == "here")
    {
      if (!next_param (s, c1, c2, tok, "expected parameter after here in "))
	return false;
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
      if (!next_param (s, c1, c2, tok, "expected parameter after is in "))
	return false;
      tok = eval_param (tok);
      /* ExecuteIfIs (V4Game.cs:7450-7578) splits the parameter into *fields* on
       * the first ';' and, if there is one, the second: with three fields the
       * middle one is the operator and everything after it is the right-hand
       * side; with two, the operator is "=".  The operator is then matched
       * whole, and anything but =, !=, gt, lt, gt= or lt= is an error.
       *
       * Question used to hunt for the operator as a *substring* of the whole
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
      QUESTION_DBG << "Comparing <" << val1 << "> " << op << " <" << val2 << ">\n";
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
  else if (tok == "type")
    {
      if (!next_param (s, c1, c2, tok, "Expected parameter after type in "))
	return false;
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
