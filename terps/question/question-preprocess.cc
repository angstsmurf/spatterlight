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

/* question-preprocess.cc -- Everything between a game file's raw lines and the
 * block parser: !include expansion, including the libraries bundled with
 * Question, !addto folding, friendly-if conversion, comment stripping, brace
 * deinlining and else joining.  readfile.cc calls preprocess () and holds the
 * tokenizer this uses. */

#include <map>
#include <string>
#include <vector>
#include "builtin_libs.hh"
#include "general.hh"
#include "istring.hh"
#include "question-util.hh"
#include "QuestionRunner.hh"
#include "readfile.hh"

using namespace std;

/* The file an !include names, without its directory and in lower case: what
 * the bundled-library tests below compare.  A game may spell the path either
 * way round, so both separators count. */
static string include_basename (const string &name)
{
  std::string::size_type slash = name.find_last_of ("/\\");
  return lcase (slash == string::npos ? name : name.substr (slash + 1));
}

/* net.lib is Quest's multiplayer/networking library.  It ships with Quest
 * rather than with the games, so it is never on disk for us to load, and
 * nothing in it means anything to a single-player replay.  Recognise it by
 * name so we don't fruitlessly look for the file (which would print a
 * "Couldn't open ..." error) when a game !includes it. */
static bool is_builtin_quest_library (const string &name)
{
  return include_basename (name) == "net.lib";
}

/* stdverbs.lib, Quest's "Additional verbs" library, ships with Quest too, but
 * unlike net.lib it is all content: default responses for verbs the engine has
 * none for, a handful of extra commands, and the splitter that makes
 * "north, then east" two turns.  Splice in the bundled copy (see
 * question_builtin_stdverbs in stdverbs_builtin.cc) rather than dropping the
 * include, or a game that !includes it plays differently from Quest. */
static bool is_stdverbs (const string &name)
{
  return include_basename (name) == "stdverbs.lib";
}

/* A.G. Bampton's standard.lib (the Quest 2-era container library) also ships
 * with Quest rather than with games.  Quest 4 looks for a copy next to the
 * game first and falls back to its bundled one (V4Game.cs:ParseFile), and its
 * content changes play: its !addto synonyms block rewrites "get X" to
 * "take X" before any game command can match, and its LOOK AT override lists
 * a container's contents.  Splice in the bundled copy (see
 * question_builtin_standardlib in standardlib_builtin.cc) when no file is on
 * disk, or a game that !includes it plays differently from Quest. */
static bool is_standardlib (const string &name)
{
  return include_basename (name) == "standard.lib";
}

/* MaDbRiT's Type Library (typelib.qlb / typelib.lib) is another standard Quest
 * library that ships with Quest/QDK rather than with games, so it is never on
 * disk for us to load.  Unlike stdverbs/net, it supplies content Question does NOT
 * implement natively -- a set of object types (TLTscenery, TLTactor, TLTobject,
 * the clothing hierarchy, ...) with their default properties and action
 * overrides -- so instead of skipping it we splice in a bundled copy (see
 * question_builtin_typelib in typelib_builtin.cc). */
static bool is_typelib (const string &name)
{
  string base = include_basename (name);
  return base == "typelib.qlb" || base == "typelib.lib";
}

/* MaDbRiT's older extension library, q3ext.qlb, is the same story as his Type
 * Library: it shipped with Quest, and it is all content -- its own LOOK/TAKE/
 * DROP/GIVE/WEAR commands, synonyms and object types -- so a game that names
 * it plays a different game without it (see q3ext_builtin.cc). */
static bool is_q3ext (const string &name)
{
  return include_basename (name) == "q3ext.qlb";
}

/* A library's own asl-version, which decides how its !addto game lines merge
 * into the game block.  Quest reads it only when the file opens with
 * "!library"; a file that does not is a pre-3.x library and counts as 100, and
 * a !library with no version line counts as 200 (V4Game.cs:ParseFile). */
static int library_asl_version (const vector<string> &lines)
{
  std::string::size_type c1, c2;
  bool is_library = false;
  for (uint i = 0; i < lines.size(); i ++)
    {
      string tok = first_token (lines[i], c1, c2);
      if (tok == "")
	continue;
      is_library = (tok == "!library");
      break;
    }
  if (!is_library)
    return 100;
  for (uint i = 0; i < lines.size(); i ++)
    {
      std::string::size_type d1, d2;
      if (first_token (lines[i], d1, d2) != "!asl-version")
	continue;
      string p = next_token (lines[i], d1, d2);
      if (is_param (p))
	return atoi (param_contents (p).c_str());
    }
  return 200;
}

/* Prefix a library's !addto game lines with "lib " where Quest does.
 *
 * The prefix is what makes a library's contribution lose to the game's own:
 * ExecCommand runs the game's commands, then the game's verbs, then the
 * commands tagged "lib command", then the verbs tagged "lib verb"
 * (V4Game.Part2.cs:ExecCommand), and startscript is prefixed so a library's
 * initialisation runs before the game's (V4Game.cs:ParseFile).  Without it
 * stdverbs.lib's `command <ask #stdverbs.command#>` would answer "You get no
 * reply." to a game's own ASK command, and its `command <#stdverbs.command#.>`
 * splitter would swallow every command a player ended with a full stop.
 *
 * Lines already carrying the prefix belong to a library this one !included,
 * and were tagged by that library's own version; leave them alone. */
static void mark_library_addtos (vector<string> &lines, int libver)
{
  std::string::size_type c1, c2;
  bool in_addto_game = false;
  for (uint i = 0; i < lines.size(); i ++)
    {
      string tok = first_token (lines[i], c1, c2);
      if (tok == "!addto")
	{
	  in_addto_game = (next_token (lines[i], c1, c2) == "game");
	  continue;
	}
      if (tok == "!end")
	{
	  in_addto_game = false;
	  continue;
	}
      if (!in_addto_game || tok == "lib")
	continue;
      if ((libver >= 311 && tok == "startscript") ||
	  (libver >= 392 && (tok == "command" || tok == "verb")))
	lines[i] = "lib " + lines[i];
    }
}

static void handle_includes (const vector<string> &in_data_arg,
			     const string &filename, vector<string> &out_data,
			     QuestionInterface *gi, vector<string> &open);

/* Whether `name' -- already lower-cased, as `open' holds them -- is one of the
 * files whose !includes are being expanded right now. */
static bool being_included (const vector<string> &open, const string &name)
{
  return std::find (open.begin(), open.end(), name) != open.end();
}

/* Splice in a library bundled with Question, under the name `open' knows it
 * by.  It goes through the same include pipeline as a file would, so its own
 * directives are processed normally; one that is already being expanded is
 * passed over without comment. */
static void include_builtin (const char *name, const char *text,
			     vector<string> &out_data, QuestionInterface *gi,
			     vector<string> &open)
{
  if (!being_included (open, name))
    handle_includes (split_lines (text), name, out_data, gi, open);
}

/* `open` holds the files whose !includes are currently being expanded, from the
 * top-level game file down.  A file that !includes something already on that
 * list -- itself, most simply -- would otherwise recurse until the stack blows,
 * so we report the cycle and skip that one include instead. */
static void handle_includes (const vector<string> &in_data_arg, const string &filename, vector<string> &out_data, QuestionInterface *gi, vector<string> &open)
{
  string line, tok;
  std::string::size_type tok_start, tok_end;
  /* An empty `open` means this is the game file itself; anything below it is a
   * library, whose !addto game lines need Quest's "lib" tagging. */
  vector<string> in_data (in_data_arg);
  if (!open.empty ())
    mark_library_addtos (in_data, library_asl_version (in_data));
  open.push_back (lcase (filename));
  for (uint ln = 0; ln < in_data.size(); ln ++)
    {
      line = in_data[ln];
      tok = first_token (line, tok_start, tok_end);
      if (tok == "!include")
	{
	  tok = next_token (line, tok_start, tok_end);
	  if (!is_param(tok))
	    {
	      gi->debug_print ("Expected parameter after !include");
	      continue;
	    }
	  /* The networking library means nothing to a single-player replay; don't
	   * try to load it from disk. */
	  if (is_builtin_quest_library (param_contents (tok)))
	    continue;
	  /* The verb library isn't on disk either, but we do need its content:
	   * splice in the bundled copy (run through the same include pipeline so
	   * its own directives are processed normally). */
	  if (is_stdverbs (param_contents (tok)))
	    {
	      include_builtin ("stdverbs.lib", question_builtin_stdverbs, out_data,
			       gi, open);
	      continue;
	    }
	  /* Likewise the type library. */
	  if (is_typelib (param_contents (tok)))
	    {
	      include_builtin ("typelib.qlb", question_builtin_typelib, out_data,
			       gi, open);
	      continue;
	    }
	  /* The container library and MaDbRiT's q3ext.  Unlike the two above, a
	     game could ship its own copy, and Quest reads an adjacent file
	     before falling back to the bundled one (V4Game.cs:ParseFile); do
	     the same. */
	  if (is_standardlib (param_contents (tok)) ||
	      is_q3ext (param_contents (tok)))
	    {
	      bool q3ext = is_q3ext (param_contents (tok));
	      string libname = gi->absolute_name (param_contents (tok), filename);
	      string contents = gi->get_file (libname);
	      if (contents.empty ())
		{
		  if (q3ext)
		    include_builtin ("q3ext.qlb", question_builtin_q3ext, out_data,
				     gi, open);
		  else
		    include_builtin ("standard.lib", question_builtin_standardlib,
				     out_data, gi, open);
		}
	      else if (being_included (open, lcase (libname)))
		gi->debug_print ("Ignoring recursive !include of " + libname);
	      else
		handle_includes (split_lines (contents), libname, out_data, gi,
				 open);
	      continue;
	    }
	  string newname = gi->absolute_name (param_contents(tok), filename);
	  if (being_included (open, lcase (newname)))
	    {
	      gi->debug_print ("Ignoring recursive !include of " + newname);
	      continue;
	    }
	  handle_includes (split_lines (gi->get_file (newname)), newname, out_data, gi, open);
	}
      else if (tok == "!QDK")
	{
	  while (ln < in_data.size() &&
		 first_token (in_data[ln], tok_start, tok_end) != "!end")
	    ++ ln;
	}
      else
	out_data.push_back (line);
    }
  open.pop_back ();
}

/* preprocess, pass 0b: gather the body of every "!addto game|synonyms|type
 * <default>|<defaultroom>" ... "!end" section, keyed by the block it is
 * meant for. */
static void collect_addtos (const vector<string> &v2,
			    map <string, vector<string> > &addtos,
			    QuestionInterface *gi)
{
  std::string::size_type tok_start, tok_end;
  string tok;

  for (uint line = 0; line < v2.size(); line ++)
    {
      tok = first_token (v2[line], tok_start, tok_end);
      if (tok == "!addto")
	{
	  /* Quest's library loader knows exactly three forms (V4Game.cs:
	   * ParseFile): "!addto game", "!addto synonyms" and "!addto type
	   * <name>" for any type name, which it compares lowercased against
	   * the "define type <name>" line and whose block it creates when the
	   * game has none.  The key here is the "define" line's own tokens,
	   * which is what splice_addtos builds from the other side. */
	  tok = next_token (v2[line], tok_start, tok_end);
	  if (!(tok == "game" || tok == "synonyms" || tok == "type"))
	    {
	      gi->debug_print ("Error: Had addto for '" + tok + "'.");
	      continue;
	    }
	  if (tok == "type")
	    tok = tok + " " + lcase (next_token (v2[line], tok_start, tok_end));
	  ++ line; // skip !addto
	  while (line < v2.size() &&
 		 first_token (v2[line], tok_start, tok_end) != "!end")
	    addtos[tok].push_back (v2[line ++]);
	}
    }
}

/* preprocess, pass 0c: copy v2 to v without its !addto sections, splicing
 * each section's lines into the block it names; a section whose block never
 * turns up gets a block of its own at the end. */
static void splice_addtos (const vector<string> &v2,
			   map <string, vector<string> > &addtos,
			   vector<string> &v)
{
  std::string::size_type tok_start, tok_end;
  string tok;

  /* One entry per open "define" block: the addtos key whose lines are owed to
   * that block when it closes, or "" for a block nothing is owed to.  Quest
   * splices an !addto's lines in immediately *before* the block's "end define"
   * (V4Game.cs:ParseFile), not after its header, and that is not cosmetic:
   * within a block Quest matches commands and verbs in file order, so lines
   * put at the front would beat the author's own.  A key that no block in
   * the game answers to (a type the library adds outright) becomes a block
   * of its own after the last line, as Quest's AddLine does. */
  vector<string> owed;
  /* Inside a text or synonyms block only its "end define" counts: a line of
   * prose starting "define" would otherwise push an entry nothing pops. */
  bool in_text_body = false;
  for (uint line = 0; line < v2.size(); line ++)
    {
      if (in_text_body)
	{
	  if (!is_end_define (v2[line]))
	    {
	      v.push_back (v2[line]);
	      continue;
	    }
	  in_text_body = false;
	}
      else if (first_token (v2[line], tok_start, tok_end) == "define"
	       && is_start_textmode (v2[line]))
	in_text_body = true;
      tok = first_token (v2[line], tok_start, tok_end);
      if (tok == "!addto")
	{
	  while (line < v2.size() &&
		 first_token (v2[line], tok_start, tok_end) != "!end")
	    line ++;
	  continue;
	}
      if (tok == "define")
	{
	  string key = next_token (v2[line], tok_start, tok_end);
	  if (key == "type")
	    key = key + " " + lcase (next_token (v2[line], tok_start, tok_end));
	  v.push_back (v2[line]);
	  owed.push_back (addtos.find (key) != addtos.end() ? key : string ());
	  continue;
	}
      if (tok == "end" && next_token (v2[line], tok_start, tok_end) == "define"
	  && !owed.empty())
	{
	  string key = owed.back();
	  owed.pop_back();
	  if (key != "")
	    {
	      vector<string> &lines = addtos[key];
	      for (uint line2 = 0; line2 < lines.size(); line2 ++)
		v.push_back (lines[line2]);
	      addtos.erase (key);
	    }
	}
      v.push_back (v2[line]);
    }

  for (map<string, vector<string> >::iterator i = addtos.begin();
       i != addtos.end(); i ++)
    {
      v.push_back ("define " + i->first);
      for (uint j = 0; j < i->second.size(); j ++)
	v.push_back (i->second[j]);
      v.push_back ("end define");
    }
}

/* preprocess, pass 1: look for "if/and/or (.. <.. )" or the like and convert
 * the parenthesised group to "if/and/or is <..;lt;..>". */
static void convert_friendly_ifs (vector<string> &v)
{
  /* Both "!=" and "<>" mean not-equal.  The table is in Quest's search order
   * (V4Game.cs:ConvertFriendlyIfs): the first symbol of the list
   * that occurs anywhere in the group is the one the group is split at, so
   * "!=" beats "<>", and the two-character symbols are tried before the
   * one-character ones they contain. */
  static string comps[][2] =
    { { "!=", "!=;" },  { "<>", "!=;" },  { "<=", "lt=;" },  { ">=", "gt=;" },
      { "<",  "lt;" },  { ">",  "gt;" },  { "=",  "" } };

  std::string::size_type tok_start, tok_end;
  string tok;

  /* The words a parenthesised condition may follow are Quest's six:
   * ConvertFriendlyIfs (V4Game.cs:ConvertFriendlyIfs) looks for "if (", "until
   * (", "while (", "not (", "and (" and "or (".  "not" and "while" were missing
   * here, and their absence does not merely leave the condition unconverted:
   * the raw "<" that survives opens a <parameter> as far as the tokeniser is
   * concerned, so the search for "then" runs off the end of the line and the
   * whole if-statement is dropped -- neither branch runs.  Kingdom's
   * `if not ( %V1pop% <= 0 ) then do <V1riot>' was the game that showed it:
   * Village1 never rioted, never sent its leader with the tribute, and never
   * appeared in the season report at all, and `repeat while ( %p% < 3 )' did
   * not loop even once. */
  for (uint line = 0; line < v.size(); line ++)
    {
      tok_end = 0;
      while (tok_end < v[line].length())
	{
	  tok = next_token (v[line], tok_start, tok_end, false);
	  /* A while loop rather than an if: the word that follows one of these
	   * may be another of them ("if not (", "repeat while ("), and Quest,
	   * which looks for the six pairs as plain substrings, sees the inner
	   * one either way. */
	  while (tok == "if" || tok == "repeat" || tok == "until" ||
		 tok == "while" || tok == "not" ||
		 tok == "and" || tok == "or")
	    {
	      tok = next_token (v[line], tok_start, tok_end, true);
	      if (tok.length() > 2 && tok[0] == '(' &&
		  tok [tok.length() - 1] == ')')
		{
		  tok = tok.substr (1, tok.length() - 2);
		  string str = v[line];
		  std::string::size_type cmp_start;
		  for (uint cmp = 0; cmp < ARRAYSIZE(comps); cmp ++)
		    if ((cmp_start = tok.find(comps[cmp][0])) != string::npos)
		      {
			std::string::size_type cmp_end = cmp_start + comps[cmp][0].length();
			str = str.substr(0, tok_start) +
			  "is <" + trim (tok.substr(0, cmp_start)) + ";" +
			  comps[cmp][1] +
			  trim (tok.substr(cmp_end)) + ">" +
			    str.substr(tok_end);
			cmp = ARRAYSIZE(comps);
			v[line] = str;
			tok_end = tok_start; // old value of tok_end invalid
		      }
		  break;   /* the parenthesised group ends this condition */
		}
	    }
	}
    }
}

/* preprocess, pass 2: extract comments from non-text blocks. */
static void strip_comments (vector<string> &v)
{
  bool in_text_block = false;
  for (uint line = 0; line < v.size(); line ++)
    {
      if (!in_text_block && is_start_textmode (v[line]))
	in_text_block = true;
      else if (in_text_block && is_end_define (v[line]))
	in_text_block = false;
      else if (!in_text_block)
	{
	  std::string::size_type start_ch = 0, end_ch = 0;
	  while (start_ch < v[line].length())
	    if (next_token (v[line], start_ch, end_ch)[0] == '\'')
	      {
		v[line] = v[line].substr (0, start_ch);
		break;
	      }
	}
    }
}

/* preprocess, pass 3: deinline braces.  Reads v, appending the generated
 * !intproc procedures to it, and writes the main-line result to v2.  False
 * if a brace block never closes. */
static bool deinline_braces (vector<string> &v, vector<string> &v2)
{
  bool in_text_block = false;
  int int_proc_count = 0;
  for (uint line = 0; line < v.size(); line ++)
    {
      string str = v[line];
      if (!in_text_block && is_start_textmode (str))
	in_text_block = true;
      else if (in_text_block && is_end_define (str))
	in_text_block = false;
      else if (!is_balanced (str))
	{
	  /* This line opens a brace block that does not close on the same line.
	   * Extract the block into a procedure and replace it with a call.  The
	   * body runs from just after the first '{' up to the matching '}', which
	   * may sit mid-line (as in "...} else {").  Anything after that '}' is
	   * left in place to be processed normally, so that "} else {" and
	   * "} else msg <...>" split into separate statements rather than being
	   * miscounted as a balanced pair. */
	  std::string::size_type init_size = v.size();
	  int procnum = ++int_proc_count;
	  v.push_back ("define procedure <!intproc" + string_int (procnum) + ">");

	  std::string::size_type tmp_index = str.find ('{');
	  v2.push_back (trim (str.substr (0, tmp_index)) +
			" do <!intproc" + string_int (procnum) + "> ");

	  int depth = 1;
	  string rest = str.substr (tmp_index + 1);
	  uint cur = line;
	  bool closed = false;
	  string remainder;
	  for (;;)
	    {
	      std::string::size_type p = 0;
	      while (p < rest.length())
		{
		  if (rest[p] == '{')
		    ++ depth;
		  else if (rest[p] == '}' && -- depth == 0)
		    break;
		  ++ p;
		}
	      if (depth == 0 && p < rest.length())
		{
		  v.push_back (rest.substr (0, p));   /* body up to closing '}' */
		  remainder = rest.substr (p + 1);    /* e.g. " else { ... }"   */
		  closed = true;
		  break;
		}
	      v.push_back (rest);                     /* whole line is body     */
	      if (++ cur >= init_size)
		break;
	      rest = v[cur];
	    }
	  if (!closed)
	    {
	      report_error ("Braces Unbalanced");
	      return false;
	    }
	  v.push_back ("end define");

	  /* Reprocess the closing line in place, now holding only the remainder
	   * (often empty, or an "else ..." clause), and skip the consumed body. */
	  line = cur;
	  v[line] = remainder;
	  line --;
	  continue;
	  // The continue is to avoid the v2.push_back(...);
	  // this block pushes stuff onto v2 on its own.
	}
      v2.push_back (str);
    }
  return true;
}

/* preprocess, pass 4: trim lines, drop blank lines, combine elses. */
static void trim_and_join_elses (const vector<string> &v2, vector<string> &rv)
{
  bool in_text_block = false;
  for (uint line = 0; line < v2.size(); line ++)
    {
      string str = v2[line];
      if (!in_text_block && is_start_textmode (str))
	in_text_block = true;
      else if (in_text_block && is_end_define (str))
	in_text_block = false;
      else if (rv.size() > 0 && !in_text_block && get_token (str) == "else")
	{
	  rv[rv.size() - 1] = rv[rv.size() - 1] + " " + trim (str);
	  continue;
	}
      if (!in_text_block)
	str = trim (str);
      if (in_text_block || str != "")
	rv.push_back (str);
    }
}

bool preprocess (vector<string> v, const string &fname, vector<string> &rv,
		 QuestionInterface *gi)
{
  /* pass 0a: replace each !include with the text it names. */
  vector<string> v2, open_includes;
  handle_includes (v, fname, v2, gi, open_includes);
  v.clear();

  map <string, vector<string> > addtos;
  collect_addtos (v2, addtos, gi);
  splice_addtos (v2, addtos, v);
  v2.clear();

  convert_friendly_ifs (v);
  strip_comments (v);
  if (!deinline_braces (v, v2))
    return false;
  trim_and_join_elses (v2, rv);
  return true;
}
