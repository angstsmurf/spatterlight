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


/* question-script-flow.cc -- the statement handlers run_script dispatches to for
 * control flow, variables and the turn: if, for, repeat, do, exec, select,
 * set, wait, timers and the game-ending statements.
 *
 * Each takes the statement line (s), the enclosing function's result (rv),
 * the lower-cased keyword (tok) and the keyword's token bounds in s (c1, c2),
 * and returns true once the line is dealt with -- see run_script in
 * question-script.cc. */

#include "question-internal.hh"

using namespace std;

/* select */
bool question_implementation::exec_select (const string &s, string &,
					   string tok, std::string::size_type c1,
					   std::string::size_type c2)
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
      return true;
    }
  if (!next_param (s, c1, c2, tok, "Expected selector parameter in "))
    return true;
  string selector = eval_param (tok);
  tok = next_token (s, c1, c2);          // "do"
  if (!next_param (s, c1, c2, tok, "Expected case block in "))  // <!intproc>
    return true;
  string procname = param_contents (tok);
  /* O(1) name_index lookup instead of the case-insensitive linear scan the
     other procedure dispatch used to do (see run_procedure). */
  const QuestionBlock *casep = gf.find_by_name ("procedure", procname);
  if (casep == NULL)
    {
      gi->debug_print ("No case block " + procname + " found");
      return true;
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
	  return true;
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
	  return true;
	}
    }
  return true;
}

/* choose */
bool question_implementation::exec_choose (const string &s, string &,
					   string tok, std::string::size_type c1,
					   std::string::size_type c2)
{
  if (!next_param (s, c1, c2, tok, "Expected parameter after choose in "))
    return true;
  tok = eval_param (tok);
  const QuestionBlock *gb = gf.find_by_name ("selection", tok);
  if (gb == NULL)
    {
      gi->debug_print ("No selection called " + tok + " found");
      return true;
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
	  if (next_param (line, c1, c2, tok, "Expected parameter after info in "))
	    question = eval_param (tok);
	}
      else if (ci_equal (tok, "choice"))
	{
	  if (next_param (line, c1, c2, tok, "Expected parameter after choice in "))
	    {
	      choices.push_back (eval_param (tok));
	      actions.push_back (line.substr (c2));
	    }
	}
      else
	gi->debug_print ("Bad line " + line + " in selection");
    }
  if (choices.size() == 0)
    gi->debug_print ("No choices in selection " + gb->name);
  else
    run_script (actions[gi->make_choice (question, choices)]);
  return true;
}

/* do */
bool question_implementation::exec_do (const string &s, string &rv,
				       string tok, std::string::size_type c1,
				       std::string::size_type c2)
{
  /* Quest fetches the procedure name with GetParameter (V4Game.Part2.cs:5756),
   * which scans the whole line for the first <...> instead of demanding that
   * it be the very next token.  That matters for "do { ... }": de-inlining a
   * brace block keeps everything before the '{' and appends its own call, so
   * "if x then do {" becomes "if x then do do <!intprocN>" in Quest
   * (ConvertMultiLineSections, V4Game.cs:668) and identically in Question
   * (readfile.cc pass 3).  Requiring the parameter to follow "do" immediately
   * made Question silently drop every such block -- The Devil's Bargain writes
   * all of its nested conditionals that way, so e.g. the colour question in
   * its <Answers> procedure never printed its reply and never re-asked. */
  tok = next_param_token (s, c1, c2);
  if (tok == "")
    {
      gi->debug_print ("Expected parameter after do in " + s);
      return true;
    }
  string fname = eval_param (tok);
  std::string::size_type index = fname.find ('(');
  if (index != string::npos)
    {
      std::string::size_type index2 = fname.find (')');
      run_procedure (trim (fname.substr (0, index)),
		     split_f_args (fname.substr (index+1, index2-index-1)));
    }
  /* A multi-line brace block is hoisted into a "!intproc" procedure, and
   * from ASL 3.92 Quest runs those in the caller's own context
   * (ExecuteDo's useNewCtx, V4Game.cs:5767-5776), so a `return' inside
   * "if ... then {" still sets the enclosing function's result.  Any other
   * procedure gets a copied context and its `return' is lost. */
  else if (asl_version_ >= 392 && fname.compare (0, 8, "!intproc") == 0)
    {
      const QuestionBlock *proc = gf.find_by_name ("procedure", fname);
      if (proc == NULL)
	gi->debug_print ("No procedure " + fname + " found.");
      else
	for (uint j = 0; j < proc->data.size (); j ++)
	  run_script (proc->data[j], rv);
    }
  else
    run_procedure (fname);

  return true;
}

/* doaction */
bool question_implementation::exec_doaction (const string &s, string &,
					     string tok, std::string::size_type c1,
					     std::string::size_type c2)
{
  if (!next_param (s, c1, c2, tok, "Expected parameter after doaction in "))
    return true;
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
  return true;
}

/* dontprocess */
bool question_implementation::exec_dontprocess (const string &, string &,
						string, std::string::size_type,
						std::string::size_type)
{
  dont_process = true;
  return true;
}

/* enter */
bool question_implementation::exec_enter (const string &s, string &,
					  string tok, std::string::size_type c1,
					  std::string::size_type c2)
{
  if (!next_param (s, c1, c2, tok, "Expected parameter after enter in "))
    return true;
  tok = eval_param (tok);
  set_svar (tok, input_to_utf8 (gi->get_string()));
  return true;
}

/* exec */
bool question_implementation::exec_exec (const string &s, string &,
					 string tok, std::string::size_type c1,
					 std::string::size_type c2)
{
  if (!next_param (s, c1, c2, tok, "Expected parameter after exec in "))
    return true;
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
	  return true;
	}
      /* An empty command line is not an error and does not even end a
       * paragraph: ExecCommand returns on the spot, before the hooks, the
       * error path and its closing Print("") (ibid. 4137-4141).  "The Quest
       * to find The Dark Hills" execs one from a `choice` whose "no" branch
       * does nothing. */
      if (cmd == "")
	return true;
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
	return true;
      run_turn (tok, true, false);
    }
  /* Quest ends every ExecCommand with Print("") (V4Game.Part2.cs:4606), the
   * blank line that separates one turn's output from the next.  Question emits
   * that separator at the top of run_command instead, which comes to the
   * same thing for a command the player typed but leaves an exec'd one
   * butted up against whatever follows.  stdverbs.lib's splitter is built
   * entirely out of exec, so "north, then south" needs the break here. */
  print_newline ();
  return true;
}

/* flag */
bool question_implementation::exec_flag (const string &s, string &,
					 string tok, std::string::size_type c1,
					 std::string::size_type c2)
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
      return true;
    }
  string onoff = tok;

  tok = next_token (s, c1, c2);
  if (is_param (tok))
    set_obj_property ("game", (is_on ? "" : "not ") + eval_param (tok));
  else
    gi->debug_print ("Expected param after flag " + onoff + " in " + s);
  return true;
}

/* for */
bool question_implementation::exec_for (const string &s, string &,
					string tok, std::string::size_type c1,
					std::string::size_type c2)
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
       * or an exit.  Question keeps rooms in state.objs beside the objects (they
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
	      /* These loops index state.objs afresh on every pass: the body
	       * can clone or create an object, and the push_back that adds
	       * it may reallocate the vector under a range-for.  Objects
	       * added during the loop are not visited. */
	      const size_t nobjs = state.objs.size();
	      if (want_exit)
		{
		  for (size_t k = 0; k < nobjs; k ++)
		    if (state.objs[k].is_room)
		      for (const string &e:
			     exit_object_names (state.objs[k].name))
			{
			  set_svar ("quest.thing", e);
			  run_script (script);
			}
		  return true;
		}
	      for (size_t k = 0; k < nobjs; k ++)
		if (state.objs[k].is_room == want_room)
		  {
		    string name = state.objs[k].name;
		    QUESTION_DBG << "  quest.thing -> " + name + "\n";
		    set_svar ("quest.thing", name);
		    run_script (script);
		  }
	      return true;
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
		  return true;
		}
	      /* The place is a *room*, matched against ContainerRoom, so
	       * "for each object in <box>" iterates nothing at all while a
	       * box's contents turn up in their room's own loop.  Wizard's
	       * church turns on that: its DETECT MAGIC sweeps the room and
	       * has to reach the Yellow Bead inside the basket.  Quest
	       * compares lower-cased both sides, so the casing a move or a
	       * "parent" line happened to use does not matter. */
	      string container = lcase (tok);
	      const size_t nobjs = state.objs.size();
	      for (size_t k = 0; k < nobjs; k ++)
		if (state.objs[k].is_room == want_room &&
		    lcase (state.objs[k].parent) == container)
		  {
		    set_svar ("quest.thing", state.objs[k].name);
		    run_script (script);
		  }
	      return true;
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
	  return true;
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
	  return true;
	}
      /* Quest's FOR includes its end value and counts down on a negative step
       * (it is VB's For/Next).  The old strict "< endindex" dropped the final
       * iteration of every loop -- the bundled type library walks a string with
       * "for <n; 1; %lengthof%>", so its last character was never seen -- and
       * ran a negative step zero times. */
      /* The next value is computed wide, so a loop whose end is near
       * INT_MAX/INT_MIN stops there rather than overflowing and wrapping
       * round to start over (four billion passes, i.e. a hang). */
      for (set_ivar (varname, startindex);
	   step > 0 ? get_ivar (varname) <= endindex
		    : get_ivar (varname) >= endindex; )
	{
	  run_script (script);
	  long long next = (long long) get_ivar (varname) + step;
	  if (next > INT_MAX || next < INT_MIN)
	    break;
	  set_ivar (varname, (int) next);
	}
      return true;
    }

  return false;
}

/* if */
bool question_implementation::exec_if (const string &s, string &rv,
				       string tok, std::string::size_type c1,
				       std::string::size_type c2)
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
      return true;
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
  return true;
}

/* inc, dec */
bool question_implementation::exec_inc_dec (const string &s, string &,
					    string tok, std::string::size_type c1,
					    std::string::size_type c2)
{
  bool is_dec = (tok == "dec");
  if (!next_param (s, c1, c2, tok, "Expected parameter after inc in "))
    return true;
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
  return true;
}

/* pause */
bool question_implementation::exec_pause (const string &s, string &,
					  string tok, std::string::size_type c1,
					  std::string::size_type c2)
{
  if (!next_param (s, c1, c2, tok, "Expected parameter after pause in "))
    return true;
  int i = (int) eval_double (param_contents(tok));
  gi->pause (i);
  suspend_turn ();          /* PauseAsync suspends the turn, as `wait` does */
  return true;
}

/* playerlose, playerwin */
bool question_implementation::exec_playerwin_lose (const string &, string &,
						   string tok, std::string::size_type,
						   std::string::size_type)
{
  run_script (tok == "playerwin" ? "displaytext <win>"
				 : "displaytext <lose>");
  state.running = false;
  is_running_ = false;   /* end the game so the host stops prompting */
  return true;
}

/* repeat */
bool question_implementation::exec_repeat (const string &s, string &,
					   string tok, std::string::size_type c1,
					   std::string::size_type c2)
{
  tok = next_token (s, c1, c2);
  /* CI: ExecuteRepeat tests "while "/"until " with BeginsWith
   * (V4Game.cs:6002-6007). */
  if (!ci_equal (tok, "while") && !ci_equal (tok, "until"))
    {
      gi->debug_print ("Expected while or until after repeat in " + s);
      return true;
    }
  bool is_while = ci_equal (tok, "while");
  std::string::size_type start_cond = c2, end_cond = string::npos;
  /* Quest does not use a keyword to separate the condition from the loop
   * body: ExecuteRepeat (V4Game.cs:6018-6034) walks the '>' characters and
   * stops at the first one whose remainder does not continue the condition
   * with "and " or "or ".  Everything up to and including that '>' is the
   * condition, everything after it is the script.  Question used to look for a
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
      return true;
    }
  string cond = trim (s.substr (start_cond, end_cond - start_cond));
  string script = trim (s.substr (end_cond));
  QUESTION_DBG << "Interpreting '" << s << "' as ("
       << (is_while ? "WHILE" : "UNTIL") << ") ("
       << cond << ") {" << script << "}\n";
  while (state.running && eval_conds (cond) == is_while)
    run_script(script);
  return true;
}

/* return */
bool question_implementation::exec_return (const string &s, string &rv,
					   string tok, std::string::size_type c1,
					   std::string::size_type c2)
{
  if (next_param (s, c1, c2, tok, "Expected parameter after return in "))
    rv = eval_param(tok);
  return true;
}

/* set */
bool question_implementation::exec_set (const string &s, string &,
					string tok, std::string::size_type c1,
					std::string::size_type c2)
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
	  return true;
	}
      run_set_collectable (eval_param (tok));
      return true;
    }
  /* CI, like the type keywords below: ExecuteSet reads them all with
   * BeginsWith (V4Game.Part2.cs:6104-6141). */
  if (ci_equal (tok, "interval"))
    {
      if (!next_param (s, c1, c2, tok, "Expected param after set interval in "))
	return true;
      tok = eval_param (tok);
      std::string::size_type index = tok.find (';');
      if (index == string::npos)
	{
	  gi->debug_print ("No semicolon in param in " + s);
	  return true;
	}
      string timer_name = trim (tok.substr (0, index));
      uint time_val = parse_int (trim (tok.substr (index+1)));

      /* Timer names compare LCase'd (V4Game.Part2.cs:6120). */
      for (TimerRecord &i: state.timers)
	if (ci_equal (i.name, timer_name))
	  {
	    i.interval = time_val;
	    return true;
	  }
      gi->debug_print ("no interval named " + timer_name + " found!");
      return true;
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
      return true;
    }
  if (tok.find (';') == string::npos)
    {
      gi->debug_print ("Only one expression in set in " + s);
      return true;
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
      size_t vi;
      if (find_svar (varname, vi))
	vartype = "string";
      else if (find_ivar (varname, vi))
	vartype = "numeric";
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
      return true;
    }
  if (vartype == "")
    {
      gi->debug_print ("Undefined variable " + varname + " in " + s);
      return true;
    }
  if (vartype == "string")
    {
      set_svar (varname, trim_braces (trim (tok.substr(index+1))));
    }
  else
    {
      set_ivar (varname, eval_set_numeric (tok.substr (index+1)));
    }
  return true;
}

/* setstring, setvar */
bool question_implementation::exec_setvar (const string &s, string &,
					   string tok, std::string::size_type c1,
					   std::string::size_type c2)
{
  bool is_string = (tok == "setstring");
  if (!next_param (s, c1, c2, tok, "Expected parameter in "))
    return true;
  if (tok.find (';') == string::npos)
    {
      gi->debug_print ("Only one expression in set in " + s);
      return true;
    }
  tok = eval_param (tok);
  std::string::size_type index = tok.find (';');
  string varname = trim (tok.substr (0, index));
  if (is_string)
    set_svar (varname, trim_braces (trim (tok.substr (index+1))));
  else
    set_ivar (varname, eval_set_numeric (tok.substr (index+1)));
  return true;
}

/* stop */
bool question_implementation::exec_stop (const string &, string &,
					 string, std::string::size_type,
					 std::string::size_type)
{
  /* "stop" is FinishGame (StopType.Null) (V4Game.Part2.cs:5798-5801): the
   * game is over exactly as it is after playerwin/playerlose, only with no
   * win/lose text.  Clearing state.running alone left the host prompting
   * for commands that could no longer do anything. */
  state.running = false;
  is_running_ = false;
  return true;
}

/* timeron, timeroff */
bool question_implementation::exec_timer (const string &s, string &,
					  string tok, std::string::size_type c1,
					  std::string::size_type c2)
{
  bool running = (tok == "timeron");
  if (next_param (s, c1, c2, tok,
		  running ? "Expected parameter after timeron in "
			  : "Expected parameter after timeroff in "))
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
	     * than starting over.  Question used to reset timeleft here, which
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
	    return true;
	  }
      gi->debug_print ("No timer " + tok + " found");
    }
  return true;
}

/* wait */
bool question_implementation::exec_wait (const string &s, string &,
					 string tok, std::string::size_type c1,
					 std::string::size_type c2)
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
	  return true;
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
   * run_command).
   *
   * Nothing is printed after the keypress: Quest 4.1.5 neither clears
   * the screen nor leaves a blank line there, and the script's next msg
   * follows the wait line directly (Sim Political Career's opening,
   * checked against the real runner). */
  suspend_turn ();
  return true;
}
