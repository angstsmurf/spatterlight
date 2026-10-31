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

#include <algorithm>
#include <vector>
#include <map>
#include <string>
#include <iostream>
#include "readfile.hh"
#include "question-util.hh"
#include "istring.hh"
#include "reserved_words.hh"
#include "QuestionRunner.hh"
#include "general.hh"
#include "builtin_libs.hh"

using namespace std;


string next_token (const string &full, std::string::size_type &tok_start, std::string::size_type &tok_end, bool cvt_paren)
{
  tok_start = tok_end;
  while (tok_start < full.length() && isspace (full[tok_start]))
    {
      ++ tok_start;
    }
  if (tok_start >= full.length())
    {
      tok_start = tok_end = full.length();
      return "";
    }
  tok_end = tok_start + 1;
  if (full[tok_start] == '{' || full[tok_start] == '}')
    {
      /* brace is a token by itself */;
    }
  else if (full[tok_start] == '<')
    {
      while (tok_end < full.length() && full [tok_end] != '>')
	++ tok_end;
      if (full[tok_end] == '>')
	++ tok_end;
    }
  else if (cvt_paren && full[tok_start] == '(')
    {
      uint depth = 1;
      do {
	if (full[tok_end] == '(')
	  {
	    ++ depth;
	  }
	else if (full[tok_end] == ')')
	  {
	    -- depth;
	  }
	++ tok_end;
      } while (tok_end < full.length() && depth > 0);
    }
  else
    {
      while (tok_end < full.length() && !isspace (full[tok_end]))
	{
	  ++ tok_end;
	}
    }
  return full.substr (tok_start, tok_end - tok_start);
}

string first_token (const string &s, std::string::size_type &t_start, std::string::size_type &t_end)
{
  t_end = 0;
  return next_token (s, t_start, t_end);
}

string nth_token (const string &s, int n)
{
  std::string::size_type x1, x2 = 0;
  string rv;
  do
    {
      rv = next_token (s, x1, x2);
    }
  while (-- n > 0);
  return rv;
}

string get_token (const string &s, bool cvt_paren)
{
  std::string::size_type x1, x2 = 0;
  return next_token (s, x1, x2, cvt_paren);
}

bool find_token (const string &s, const string &tok, std::string::size_type &tok_start, std::string::size_type &tok_end, bool cvt_paren)
{
  std::string::size_type copy_start, copy_end;
  copy_end = tok_end;

  do
    {
      string tmp = next_token (s, copy_start, copy_end, cvt_paren);
      if (tmp == tok)
	{
	  tok_start = copy_start;
	  tok_end = copy_end;
	  return true;
	}
    }
  while (copy_end < s.size());

  return false;
}

static bool is_define (const string &s)
{
  /* CI: Quest spots block headers with the lowercasing BeginsWith
   * ("define ", V4Game.cs:CheckSections). */
  return ci_equal (get_token (s), "define");
}


bool is_start_textmode (const string &s)
{
  std::string::size_type start_char, end_char = 0;
  if (!ci_equal (next_token (s, start_char, end_char), "define")) return false;
  string tmp = next_token (s, start_char, end_char);
  /* CI: BeginsWith "define text" / "define synonyms"
   * (V4Game.cs:CheckSections). */
  return ci_equal (tmp, "text") || ci_equal (tmp, "synonyms");
}

bool is_end_define (const string &s)
{
  std::string::size_type start_char, end_char = 0;
  /* Case-sensitive, as in Quest: the section scan closes a block only on
   * Trim(line) == "end define", raw (V4Game.cs:CheckSections). */
  return (next_token (s, start_char, end_char) == "end" &&
	  next_token (s, start_char, end_char) == "define");
}






static reserved_words dir_tag_property ("north", "south", "east", "west", "northwest", "northeast", "southeast", "southwest", "up", "down", "out", (char *) NULL);

/* Whether a tag line's value is fed through Quest's property list -- and so
 * cut off at its first semicolon -- at this ASL version.
 *
 * A tag like `look <...>` is read into a field of the room or object record
 * first; only afterwards, and only from the version that introduced the
 * behaviour, is the same text pushed through AddToObjectProperties as
 * "look=<value>", which splits a `;`-separated list
 * (V4Game.cs:AddToObjectProperties). Below that version the field is all there
 * is, and the field holds the whole value: what the player reads still has its
 * semicolons in it.
 *
 * Every one of a room's tags crossed over at 3.50
 * (V4Game.Part2.cs:SetUpRoomData), and an object's descriptive ones at 3.11
 * (InitialiseObject, ibid. SetUpCharObjectInfo).  An object's container tags
 * -- `open`, `close`, `add`, `remove`, `parent` -- were properties from the
 * start and are cut at every version, because what those print is the property
 * (ExecOpenClose reads it with GetObjectProperty, V4Game.cs:ExecOpenClose).
 *
 * `take` is the one tag that is a property and yet never cut: ExecTake prints
 * the field, `Print(t.Data)` (V4Game.Part2.cs:ExecTake), and below 3.11 it
 * does not even do that -- it goes back to the source line and runs what
 * follows the keyword (ibid. ExecTake).  The truncated "take" property is
 * still written, but nothing reads it for its text.
 *
 * Question has no field/property split: a tag becomes a property whatever the
 * version, or exits, aliases and prefixes would stop working below 3.50.  So
 * what the gate decides here is only whether the value is split, which is the
 * half of the difference a game can see.  Magic Sword (2.17) is the corpus
 * game that needs it: its training room's `look` names two rooms in one
 * sentence each, "...replicas of enemys; using weapons.|n And...", and unsplit
 * it stops after the first eight words. */
static bool tag_property_splits (const string &blocktype, const string &tag,
				 int version)
{
  if (blocktype == "room")
    return version >= 350;
  if (tag == "take")
    return false;
  static reserved_words late_object_tags
    ("look", "examine", "speak", "displaytype", "alias", "prefix", "suffix",
     "detail", "gender", "article", (char *) NULL);
  return !late_object_tags[tag] || version >= 311;
}

/* Rewrite an object's `use' or `give' line as the action it declares.  `verb'
 * is the keyword and `prep' the preposition that turns the action round
 * ("on" for use, "to" for give); `rest' is the token after the keyword, which
 * [t1, t2) spans.  The five spellings come out as
 *
 *     use on <thing> ...     ->  action <use on thing> ...
 *     use on anything ...    ->  action <use on anything> ...
 *     use <thing> ...        ->  action <use thing> ...
 *     use anything ...       ->  action <use anything> ...
 *     use ...                ->  action <use> ...
 *
 * and a preposition with neither after it marks the line an error. */
static void rewrite_use_give (string &line, const string &verb,
			      const char *prep, string rest,
			      std::string::size_type t1,
			      std::string::size_type t2)
{
  string lhs = "action <" + verb + " ";
  if (ci_equal (rest, prep))
    {
      rest = next_token (line, t1, t2);
      string rhs = line.substr (t2);
      if (ci_equal (rest, "anything"))
	line = lhs + prep + " anything> " + rhs;
      else if (is_param (rest))
	line = lhs + prep + " " + trim (param_contents (rest)) + "> " + rhs;
      else
	{
	  QUESTION_DBG << "Error handling '" << line << "'" << endl;
	  line = "ERROR: " + line;
	}
    }
  else if (ci_equal (rest, "anything"))
    line = lhs + "anything> " + line.substr (t2);
  else if (is_param (rest))
    line = lhs + trim (param_contents (rest)) + "> " + line.substr (t2);
  else
    line = "action <" + verb + "> " + line.substr (t1);
}

/* read_into: one line of a block's own body (not of a block nested in it),
 * rewritten into the canonical "properties <..>" / "action <..> .." forms the
 * rest of Question reads, and appended to out_data -- as several lines, one,
 * or none. */
void QuestionFile::read_block_line (string line, const string &blocktype,
				  const reserved_words &props,
				  const reserved_words &actions,
				  vector<string> &out_data)
{
  std::string::size_type t1, t2;
  string tok = first_token (line, t1, t2);
  std::string::size_type tokend = t2;
  string rest = next_token (line, t1, t2);
  bool drop_line = false;
  /* Quest recognises a keyword with something after it via the
   * lowercasing BeginsWith ("alias ", "use ", "north ", ...), but a
   * bare-word line with a raw Trim(line) == "hidden" compare
   * (InitialiseObject, V4Game.Part2.cs:SetUpCharObjectInfo).  So fold the token
   * whenever the line carries more, and compare it raw when the
   * keyword is the whole line. */
  string ltok = (rest == "") ? tok : lcase (tok);

  if (props[ltok] && dir_tag_property[ltok])
    {
      /* Store the direction folded: the exit machinery in question-runner
       * compares these first words against its lowercase names. */
      out_data.push_back (ltok + line.substr (tokend));
    }

  /* Quest keeps a bare "open"/"close" line, and one whose whole argument is
     a plain <message>, as a *property*; only a scripted form ("open msg
     <...>", "open { ... }") becomes an action.  ExecOpenClose reads that
     property both to decide whether the container may be opened at all and
     for the text to print (V4Game.Part2.cs:SetUpCharObjectInfo,
     V4Game.cs:ExecOpenClose). Checked ahead of the generic handling below,
     where "open"/"close" appear in `actions` and so would become an empty
     action that opens the container while printing nothing -- or, for the
     <message> form, a line Question recognises as neither property nor action
     and drops. */
  if (actions[ltok] && (ltok == "open" || ltok == "close") &&
      (rest == "" || is_param (rest)))
    {
      line = "properties <" + ltok +
	(rest == "" ? "" : "=" + param_contents (rest)) + ">";
    }
  /* Container listing lines.  Quest keeps the <text> form of `list`,
     `list empty` and `list closed` as a property of that name and the
     scripted form as an action; "list empty off"/"list closed off" are the
     defaults and record nothing, while "list off" records the property
     "not list" to suppress listing altogether (V4Game.cs:ProcessListInfo,
     reached from the container branch of the object loader,
     V4Game.Part2.cs:SetUpCharObjectInfo).  "list" is in neither the props nor
     the actions word list, so it needs this case: the raw line would become a
     *type* property named after the whole line, leaving the "list closed" text
     unreachable and `property <X; list empty>` always false.  Games use that condition to ask
     "is this container empty?", because Quest's `property` condition only
     tests whether the property exists (ExecuteIfProperty,
     V4Game.cs:ExecuteRepeat) and a container that is not empty prints its
     contents instead.  "Shipwrecked" is unwinnable without it: the raft's last
     piece is `use <crate2>`, guarded by `if property <crate2; list empty>`. */
  else if (ltok == "list")
    {
      string propname = "list";
      if (ci_equal (rest, "empty") || ci_equal (rest, "closed"))
	{
	  propname = "list " + lcase (rest);
	  rest = next_token (line, t1, t2);
	}
      if (ci_equal (rest, "off"))
	{
	  if (propname == "list")
	    line = "properties <not list>";
	  else
	    drop_line = true;
	}
      else if (is_param (rest))
	line = "properties <" + propname + "=" + param_contents (rest) + ">";
      else if (rest == "")
	drop_line = true;
      else
	line = "action <" + propname + "> " + line.substr (t1);
    }
  else if (props[tok] && rest == "")
    {
      /* tok, not ltok: a bare-word flag line is case-sensitive in
       * Quest (see the ltok comment above). */
      line = "properties <" + tok + ">";

      /* Quest's object loader treats the two spellings of "hidden"
	 differently, and this rewrite is exactly what makes them
	 indistinguishable afterwards, so record which one we saw while we
	 still can.  Only a bare "hidden" line raises the loader's local
	 `hidden' flag (V4Game.Part2.cs:SetUpCharObjectInfo); "properties
	 <hidden>" and a hidden inherited from a type merely add the property
	 (V4Game.Part2.cs:SetUpCharObjectInfo), and when the definition ends the
	 loader undoes their effect on visibility with
	 "if (!hidden) o.Exists = true" (V4Game.Part2.cs:SetUpCharObjectInfo).
	 So only the bare tag hides an object at load time; the property still
	 hides it if something *writes* it later at runtime
	 (V4Game.cs:AddToObjectProperties).  Revenge of the Shadow Masters
	 depends on this: its Mug o' Grog is "properties <hidden; ...>" and has
	 to be on sale in the bar from turn one, and the grog gates the whole
	 second half of the game.  QuestionState's constructor reads the marker
	 back; types are excluded because a type's hidden goes through the
	 property path in Quest too.  */
      if (tok == "hidden" && blocktype != "type")
	line = "properties <hidden; !hiddentag>";

      /* A bare "surface" line declares a container as well: Quest's object
	 loader answers it with two AddToObjectProperties calls, "container"
	 and then "surface" (V4Game.Part2.cs:SetUpCharObjectInfo).  That
	 matters because ListContents refuses outright anything without the
	 "container" property (V4Game.cs:ListContents), so a surface that was
	 only ever tagged "surface" lists nothing at all -- which is how
	 Barbarian's Mantle came to swallow the Vase standing on it, and the
	 Wizard's bar-top the bottle of sludge.  Only the bare line does this: a
	 "properties <surface>" adds just the one property, and so does a
	 surface inherited from a type, whose lines Quest folds straight into
	 a property string (V4Game.cs:GetPropertiesInType).
	 Below ASL 3.91 the loader's surface branch is empty, so a bare
	 line records neither property and the whole line goes. */
      if (tok == "surface" && blocktype != "type")
	{
	  if (asl_version >= 391)
	    line = "properties <surface; container>";
	  else
	    drop_line = true;
	}
    }
  else if (props[ltok] && is_param(rest))
    {
      /* "tagproperty" is the same thing as "properties" with the
       * `;`-splitting left out -- see tag_property_splits, and
       * QuestionFile::ensure_cached, which files it as the one property it
       * names. */
      string kw = tag_property_splits (blocktype, ltok, asl_version)
	? "properties <" : "tagproperty <";
      line = kw + ltok + "=" + param_contents(rest) + ">";
    }
  /* The use/give forms and their on/to/anything sub-keywords are all CI:
   * AddToUseInfo/AddToGiveInfo read them with BeginsWith
   * (V4Game.cs:AddToUseInfo). */
  else if (actions[ltok] &&
	   (ltok == "use" || ltok == "give" || !is_param(rest)))
    {
      /* The partner object's name is trimmed here for the same reason the
       * `define` header above trims its own.  Quest trims neither: both
       * ObjectName and UseData().UseObject are raw GetParameter results
       * (V4Game.Part2.cs:SetUpCharObjectInfo, V4Game.cs:AddToUseInfo), so its
       * two spellings of a padded name agree and the use succeeds.  Question
       * trimmed only the declaration, so an object written "<Warrior >" was
       * registered as "Warrior" while its partner's handler was filed under
       * "use on Warrior " and get_obj_action missed -- Londe Perplex's
       * Rage lost its whole scripted attack that way.  */
      if (ltok == "use")
	rewrite_use_give (line, "use", "on", rest, t1, t2);
      else if (ltok == "give")
	rewrite_use_give (line, "give", "to", rest, t1, t2);
      else
	{
	  line = "action <" + ltok + "> " + line.substr (t1);
	}
    }
  //else

  /* Recalculating tok because the line might have been rewritten.  In
   * the walkthrough corpus a "properties"/"tagproperty" line only ever
   * turns up here in an object, room, character, type or game block
   * (59,020 lines, none anywhere else), so the split below needs no
   * check on the block type. */
  tok = first_token (line, t1, t2);
  if (ci_equal (tok, "properties"))
    {
      rest = next_token (line, t1, t2);
      if (is_param (rest))
	{
	  vstring items = split_param (param_contents (rest));
	  for (const auto &i: items)
	    {
	      out_data.push_back ("properties <" +
				  static_eval(i) + ">");
	    }
	}
      else
	{
	  out_data.push_back ("ERROR " + line);
	}
    }
  /* One property, semicolons and all -- but otherwise the same line the
   * split form emits, static_eval included. */
  else if (ci_equal (tok, "tagproperty"))
    {
      rest = next_token (line, t1, t2);
      if (is_param (rest))
	out_data.push_back ("tagproperty <" +
			    static_eval (param_contents (rest)) + ">");
      else
	out_data.push_back ("ERROR " + line);
    }
  else if (!drop_line)
    {
      out_data.push_back(line);
    }
}

void QuestionFile::read_into (const vector<string> &in_data,
			  const string &in_parent, uint cur_line, bool recurse,
			  const reserved_words &props,
			  const reserved_words &actions)
{
  size_t blocknum = blocks.size();
  blocks.push_back (QuestionBlock());
  QuestionBlock &out_block = blocks[blocknum];

  vector<string> &out_data = out_block.data;
  out_block.parent = in_parent;
  std::string::size_type t1, t2;
  string line = in_data[cur_line];
  string token = first_token (line, t1, t2);
  // Was assert(token == "define"); a malformed file would abort the whole
  // interpreter. Log and proceed so loading degrades rather than crashing.
  if (!ci_equal (token, "define"))
    QUESTION_DBG << "readfile: expected 'define' but got '" << token << "' in: " << line << endl;
  /* The block type is folded: Quest finds blocks of nearly every kind with the
   * lowercasing BeginsWith ("define room ", "define type ", "define timer ",
   * ...), so "define Room" is a room there.  (Its procedure/function/
   * selection/text lookups go through a case-sensitive dictionary instead
   * (V4Game.cs:DefineBlockParam) -- Question is deliberately laxer
   * there, as it already is about those blocks' names.) */
  string blocktype = out_block.blocktype = lcase (next_token (line, t1, t2));
  type_indecies[blocktype].push_back (blocknum);
  string name = next_token (line, t1, t2); // "<itemname>", or the like

  if (blocktype == "game")
    {
      out_block.name = "game";
      out_data.push_back ("game name " + name);
    }
  else if (is_param(name))
    {
      /* Trim the name: a stray space in "define room <Foo >" would otherwise
       * leave the block registered as "Foo " while every exit destination is
       * trimmed to "Foo" (see question-runner.cc), so find_by_name never matches
       * and the room becomes an exit-less dead-end. */
      out_block.name = trim (param_contents(name));
    }
  else if (name != "")
    {
      throw string ("Expected parameter; " + name + " found instead.");
    }

  /* Only the block types that name a thing in the game world go into the
   * "what is <name>?" map; procedures, functions, texts and the rest are
   * looked up by (type, name) through name_index below.  Names are not unique
   * even among these: across the walkthrough corpus 30 characters, 27 objects
   * and one room repeat a name already defined for their type, and
   * register_block keeps the first. */
  if (blocktype == "room" || blocktype == "object" || blocktype == "game" ||
      blocktype == "character")
    {
      register_block (out_block.name, blocktype);
    }

  /* Index this block by (type, name) for fast find_by_name lookups.  Done in
   * lockstep with type_indecies above so find_by_name sees exactly the same
   * (incrementally growing) set of blocks it did when it scanned linearly. */
  name_index[name_key (blocktype, out_block.name)].push_back (blocknum);

  if (blocktype == "room" && find_by_name ("type", "defaultroom"))
    {
      out_data.push_back ("type <defaultroom>");
    }
  if (blocktype == "object" && find_by_name ("type", "default"))
    {
      out_data.push_back ("type <default>");
    }
  /* From ASL 3.91 every object is created with the "list" property already set,
     right after the `default` type has been folded in and before its own
     definition lines are read (V4Game.Part2.cs:SetUpCharObjectInfo).  That
     property is what ListContents asks about before it will list anything, so
     the default is what makes an ordinary container describe its contents at
     all -- and `list off`, which the loader below records as "not list", is
     how a game opts out.  Pushed here, in Quest's own order, so a `list
     <header>` line of the object's own still overrides it. */
  if (blocktype == "object" && asl_version >= 391)
    {
      out_data.push_back ("properties <list>");
    }

  cur_line ++;
  uint depth = 1;
  while (cur_line < in_data.size() && depth > 0)
    {
      line = in_data[cur_line];
      if (recurse && is_define (line))
	{
	  ++ depth;
	}
      else if (is_end_define (in_data[cur_line]))
	{
	  -- depth;
	}
      else if (depth == 1)
	{
	  read_block_line (line, blocktype, props, actions, out_data);
	}
      cur_line ++;
    }
}

QuestionFile::QuestionFile (const vector<string> &v, QuestionInterface *_gi) : gi(_gi)
{
  uint depth = 0;

  /* The version has to be known before the first block is read, because
   * read_into's tag rewriting is version-dependent (tag_property_splits).
   * Quest reads the line out of the game block (V4Game.cs:ParseFile); the
   * scan here is over the raw file, which reaches the same line -- the only
   * "asl-version" a game file has is the game block's, a library's being
   * spelt "!asl-version". */
  for (const string &vline: v)
    {
      std::string::size_type c1, c2;
      if (!ci_equal (first_token (vline, c1, c2), "asl-version"))
	continue;
      string p = next_token (vline, c1, c2);
      if (is_param (p))
	asl_version = atoi (param_contents (p).c_str());
      break;
    }

  string parentname, parenttype;

  static string pass_names[] =
    {"game", "type", "room", "variable", "object", "character", "procedure",
     "function", "selection", "synonyms", "text", "timer",
     /* "define options" is a nameless block like "define synonyms"; without a
	pass for it the whole block was dropped, so "abbreviations off" -- the
	only line in it Question acts on -- could never be seen
	(V4Game.Part2.cs:SetUpOptions). */
     "options"};

  reserved_words recursive_passes ("game", "room", (char*) NULL),
    object_passes ("game", "room", "objects", (char*) NULL);


  for (size_t pass = 0; pass < sizeof (pass_names) / sizeof (*pass_names);
       pass ++)
    {
      string this_pass = pass_names[pass];
      bool recursive = recursive_passes[this_pass];

      reserved_words actions ((char *) NULL), props ((char *) NULL);
      if (this_pass == "room")
	{
	  props = reserved_words ("look", "alias", "prefix", "indescription", "description", "north", "south", "east", "west", "northeast", "northwest", "southeast", "southwest", "out", "up", "down", (char *) NULL);
	  actions = reserved_words ("use", "description", "script", "north", "south", "east", "west", "northeast", "northwest", "southeast", "southwest", "out", "up", "down", (char *) NULL);
	}
      else if (this_pass == "object" || this_pass == "character")
	{
	  /* "add" belongs in both lists for the same reason "remove" does:
	   * Quest's
	     container mechanic looks for an *action* called "add" on the
	     container first and falls back to a *property* of that name
	     (DoAddRemove, V4Game.cs:ExecAddRemove), so `add { ... }` / `add
	     msg <...>` is a script and `add <text>` / a bare `add` is a
	     property.  In neither list, an `add` block would be emitted as a
	     raw line that nothing reads, which silently disables every
	     scripted container in the game.  "Shipwrecked" is unwinnable
	     without it: its lit lantern only reaches the tomb by way of `put
	     lantern in basket`, and the basket's `add` script is what sets the
	     flag the tomb doors check. */
	  props = reserved_words ("look", "examine", "speak", "take", "alias", "prefix", "suffix", "detail", "displaytype", "gender", "article", "hidden", "invisible", "container", "surface", "transparent", "seen", "opened", "closed", "add", "remove", (char *) NULL);
	  actions = reserved_words ("look", "examine", "speak", "take", "gain", "lose", "use", "give", "open", "close", "add", "remove", (char *) NULL);
	}

      depth = 0;
      /* The body of a text or synonyms block is prose, not structure: a line
       * in it that happens to start with "Define" must not open a block, or
       * depth never gets back to 0 and every later block is dropped.  Pass 2
       * of the preprocessor skips these bodies the same way. */
      bool in_text_block = false;
      for (uint i = 0; i < v.size(); i ++)
	{
	  if (in_text_block)
	    {
	      if (is_end_define (v[i]))
		{
		  in_text_block = false;
		  -- depth;
		}
	      continue;
	    }
	  if (is_define(v[i]))
	    {
	      ++ depth;
	      if (is_start_textmode (v[i]))
		in_text_block = true;
	      /* Folded, like read_into's blocktype. */
	      string blocktype = lcase (nth_token (v[i], 2));
	      if (depth == 1)
		{
		  parenttype = blocktype;
		  parentname = nth_token (v[i], 3);

		  if (blocktype == this_pass)
		    read_into (v, "", i, recursive, props, actions);
		}
	      else if (depth == 2 && blocktype == this_pass)
		{
		  if ((this_pass == "object" || this_pass == "character") &&
		      parenttype == "room")
		    read_into (v, parentname, i, false, props, actions);
		  else if (this_pass == "variable" && parenttype == "game")
		    read_into (v, "", i, false, props, actions);
		}
	    }
	  else if (is_end_define (v[i]))
	    {
	      /* An "end define" with no block open.  Quest's section scan
	       * starts at each line beginning "define", counts its way to the
	       * matching "end define" and then jumps the loop counter past it
	       * (V4Game.cs:ParseFile), so a stray one at top level is a line
	       * the scan simply steps over: the sections after it are still
	       * found.  The only code that objects is CheckSections, which
	       * calls it a fatal error and refuses the file -- except that
	       * Quest skips that check outright for three games it names in
	       * its own source, "bargain.cas", "easymoney.asl" and
	       * "musicvf1.cas" (V4Game.cs:V4Game/ParseFile).
	       *
	       * "Venus flytrap: Romantic Music" (musicvf1.cas) is one of them: it
	       * closes `define procedure <noclothesdie>` twice.  Letting depth go
	       * negative here meant that from that line on `depth == 1` was never
	       * true again, so every later top-level block was dropped -- two thirds
	       * of the procedures, all of the copied-in standard library, and the win
	       * text -- and the game loaded into a state where no command did
	       * anything at all. */
	      if (depth > 0)
		-- depth;
	      else
		QUESTION_DBG << "readfile: 'end define' with no block open, ignoring: "
			 << v[i] << endl;
	    }
	}
    }

}

static bool decompile (const string &data, int cas_version, vector<string> &rv);


QuestionFile read_question_file (QuestionInterface *gi, const string &filename)
{
  string file_contents = gi->get_file (filename);

  if (file_contents == "")
    {
      return QuestionFile();
    }

  vector<string> data;
  bool success;

  QUESTION_DBG << "Header is '" << file_contents.substr (0, 7) << "'.\n";
  /* Three compiled-game container versions exist, and Quest reads all three
     with one decompiler (V4Game.cs:LoadCASFile): QCGF001 is the Quest 2.x
     era format, 002 adds three more text-mode block types, and 003 appends a
     resource catalogue. */
  int cas_version = 0;
  if (file_contents.size() > 8 && file_contents.compare (0, 4, "QCGF") == 0)
    {
      if (file_contents.compare (4, 3, "001") == 0)      cas_version = 1;
      else if (file_contents.compare (4, 3, "002") == 0) cas_version = 2;
      else if (file_contents.compare (4, 3, "003") == 0) cas_version = 3;
    }
  if (cas_version != 0)
    {
      QUESTION_DBG << "Decompiling (CAS v" << cas_version << ")\n";
      success = decompile (file_contents, cas_version, data);
    }
  else
    {
      QUESTION_DBG << "Preprocessing\n";
      success = preprocess (split_lines (file_contents), filename, data, gi);
    }

  QUESTION_DBG << "File load was " << (success ? "success" : "failure") << endl;

  if (success)
    {
      /* One transcoding pass over the whole game, after the .cas decoder (which
	 works on raw bytes) and after !include has pulled every other file in.
	 Quest reads its files through VB's Chr(), which is code page 1252 on the
	 Windows these games were written for, so that is the encoding the text
	 is in; passed straight to Glk, the bytes would be read as Latin-1 and every
	 ©, £, dash and curly quote would come out wrong.
	 Skipped when the file is already well-formed UTF-8: a game saved that
	 way by a later author would only be double-encoded by the conversion.
	 (Quest itself has no such escape hatch and would mangle such a file --
	 this is one of the few places Question is deliberately kinder than the
	 original.)  The decision is taken for the game as a whole, since a mixed
	 file is a corrupt file either way.  */
      bool utf8 = true;
      for (const string &line: data)
	if (!text_is_utf8 (line))
	  {
	    utf8 = false;
	    break;
	  }
      if (!utf8)
	for (string &line: data)
	  line = cp1252_to_utf8 (line);
      return QuestionFile (data, gi);
    }

  gi->debug_print ("Unable to read file " + filename);
  return QuestionFile();
}


ostream &operator << (ostream &o, const QuestionBlock &gb)
{
  o << "Block " << gb.blocktype << " '" << gb.name;
  if (gb.parent != "")
    {
      o << "' and parent '" << gb.parent;
    }
  o << "'\n";
  for (uint i = 0; i < gb.data.size(); i ++)
    {
      o << "    " << gb.data[i] << "\n";
    }
  o << "\n";
  return o;
}

void print_vblock (ostream &o, string blockname, const vector<QuestionBlock> &blocks)
{
  o << blockname << ":\n";
  for (const auto &i: blocks)
    {
      o << "  " << i << "\n";
    }
  o << "\n";
}

ostream &operator << (ostream &o, const QuestionFile &gf)
{
  o << "Question File\n";
  for (map<string, vector<size_t> >::const_iterator i = gf.type_indecies.begin();
       i != gf.type_indecies.end(); i ++)
    {
      o << "Blocks of type " << (*i).first << "\n";
      for (uint j = 0; j < (*i).second.size(); j ++)
	{
	  o << gf.blocks[(*i).second[j]];
	}
      o << "\n";
    }

  o << endl;
  return o;
}





/* The CAS keyword table, byte value -> ASL keyword, transcribed from the real
   Quest's own quest.dat (QuestViva Legacy/Libraries/quest.dat, read by
   LoadCASKeywords).  Slots 0, 10 and 252-255 hold quest.dat's "!"-prefixed
   control codes (!null, !quote, !startcat, !endcat, !unknown, !cr) rather than
   keywords; decompile() recognises those by byte value, so they stay empty here.

   A keyword missing from this table is silently *deleted* from its line when a
   .cas game is decompiled.  Without slots 96 (verb) and up, containers, surfaces,
   verbs, menus and lock/unlock would all be dead for every compiled game while
   working fine in the .asl equivalent. */
static const string compilation_tokens[256] =
{"", "game", "procedure", "room", "object", "character", "text",
 "selection", "define", "end", "", "asl-version", "game", "version",
 "author", "copyright", "info", "start", "possitems", "startitems",
 "prefix", "look", "out", "gender", "speak", "take", "alias", "place",
 "east", "north", "west", "south", "give", "hideobject", "hidechar",
 "showobject", "showchar", "collectable", "collecatbles", "command", "use",
 "hidden", "script", "font", "default", "fontname", "fontsize",
 "startscript", "nointro", "indescription", "description", "function",
 "setvar", "for", "error", "synonyms", "beforeturn", "afterturn",
 "invisible", "nodebug", "suffix", "startin", "northeast", "northwest",
 "southeast", "southwest", "items", "examine", "detail", "drop",
 "everywhere", "nowhere", "on", "anything", "article", "gain", "properties",
 "type", "action", "displaytype", "override", "enabled", "disabled",
 "variable", "value", "display", "nozero", "onchange", "timer", "alt",
 "lib", "up", "down", "gametype", "singleplayer", "multiplayer", "verb",
 "menu", "container", "surface", "transparent", "opened", "parent", "open",
 "close", "add", "remove", "list", "empty", "closed", "options",
 "abbreviations", "locked", "", "", "", "", "", "", "", "", "", "", "", "",
 "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
 "", "", "", "", "", "", "do", "if", "got", "then", "else", "has", "say",
 "playwav", "lose", "msg", "not", "playerlose", "playerwin", "ask", "goto",
 "set", "show", "choice", "choose", "is", "setstring", "displaytext",
 "exec", "pause", "clear", "debug", "enter", "movechar", "moveobject",
 "revealchar", "revealobject", "concealchar", "concealobject", "mailto",
 "and", "or", "outputoff", "outputon", "here", "playmidi", "drop",
 "helpmsg", "helpdisplaytext", "helpclear", "helpclose", "hide", "show",
 "move", "conceal", "reveal", "numeric", "string", "collectable",
 "property", "create", "exit", "doaction", "close", "each", "in", "repeat",
 "while", "until", "timeron", "timeroff", "stop", "panes", "on", "off",
 "return", "playmod", "modvolume", "clone", "shellexe", "background",
 "foreground", "wait", "picture", "nospeak", "animate", "persist", "inc",
 "dec", "flag", "dontprocess", "destroy", "beforesave", "onload", "playmp3",
 "extract", "shell", "popup", "select", "case", "lock", "unlock", "", "",
 "", "", "", "", "", "", "", ""
};


bool decompile (const string &s, int cas_version, vector<string> &rv)
{
  string cur_line, tok;
  uint expect_text = 0, obfus = 0;
  unsigned char ch;

  for (uint i = 8; i < s.length(); i ++)
    {
      ch = s[i];
      /* CAS v3 appends a resource catalogue (!startcat .. !endcat) followed by
	 the packed resource data.  Quest stops scanning script the moment it
	 meets !startcat at a token position (V4Game.cs:LoadCASFile) --
	 everything past it is binary, not script -- and so do we.  The test
	 has to be at a token position: inside a quoted string, a text block or
	 an !unknown run the same byte is just data. */
      if (cas_version >= 3 && ch == 252 && obfus == 0 && expect_text != 2)
	break;
      if (obfus == 1 && ch == 0)
	{
	  cur_line += "> ";
	  obfus = 0;
	}
      else if (obfus == 1)
	{
	  cur_line += char (255 - ch);
	}
      else if (obfus == 2 && ch == 254)
	{
	  obfus = 0;
	  cur_line += " ";
	}
      else if (obfus == 2)
	{
	  cur_line += ch;
	}
      else if (expect_text == 2)
	{
	  if (ch == 253)
	    {
	      expect_text = 0;
	      rv.push_back (cur_line);
	      cur_line = "";
	    }
	  else if (ch == 0)
	    {
	      rv.push_back (cur_line);
	      cur_line = "";
	    }
	  else
	    {
	      cur_line += char (255 - ch);
	    }
	}
      else if (obfus == 0 && ch == 10)
	{
	  cur_line += "<";
	  obfus = 1;
	}
      else if (obfus == 0 && ch == 254)
	{
	  obfus = 2;
	}
      else if (ch == 255)
	{
	  if (expect_text == 1)
	    {
	      expect_text = 2;
	    }
	  rv.push_back (cur_line);
	  cur_line = "";
	}
      else
	{
	  tok = compilation_tokens[ch];
	  /* Blocks whose body is stored as raw obfuscated text rather than as
	     tokens.  "define text" is text-mode in every version; synonyms,
	     type and menu only from v2 on (V4Game.cs:LoadCASFile), since v1
	     predates them. */
	  if (cur_line == "define " &&
	      (tok == "text" ||
	       (cas_version >= 2 &&
		(tok == "synonyms" || tok == "type" || tok == "menu"))))
	    {
	      expect_text = 1;
	    }
	  cur_line += tok + " ";
	}
    }
  rv.push_back (cur_line);

  for (size_t i = 0; i < rv.size(); i ++)
  {
    QUESTION_DBG << "rv[" << i << "]: " << rv[i] << "\n";
  }

  return true;
}








vector<string> tokenize (const string &s)
{
  std::string::size_type tok_start, tok_end;
  string tok;
  vector<string> rv;
  tok_end = 0;
  while (tok_end + 1 <= s.length())
    rv.push_back (next_token (s, tok_start, tok_end));
  return rv;
}

void report_error (const string &s)
{
  QUESTION_DBG << s << endl;
  throw s;
}

vector<string> split_lines (const string &data)
{
  vector <string> rv;
  string tmp;
  uint i = 0;
  while (i < data.length())
    {

      if (data[i] == '\n' || data[i] == '\r')
	{
	  if (tmp.size() > 0 && tmp[tmp.size() - 1] == '_')
	    {
	      tmp.erase (tmp.size() - 1);
	      /* Re-check emptiness: a line consisting of nothing but the "_"
	       * continuation marker leaves nothing behind, and tmp.size() - 1
	       * would wrap to SIZE_MAX. */
	      if (tmp.size() > 0 && tmp[tmp.size() - 1] == '_')
		tmp.erase (tmp.size() - 1);
	      if (i < data.length() && data[i] == '\r' && data[i+1] == '\n')
		++ i;
	      ++ i;
	      while (i < data.length() && data[i] != '\r' &&
		     data[i] != '\n' && isspace(data[i]))
		{
		  ++ i;
		}
	      -- i;
	    }
	  else
	    {
	      rv.push_back (tmp);
	      tmp = "";
	      if (i < data.length() && data[i] == '\r' && data[i+1] == '\n')
		++ i;
	    }
	}
      else
	tmp += data[i];
      ++ i;
    }
  if (tmp != "")
    rv.push_back (tmp);
  return rv;
}

void show_tokenize (string /* s */)
{
}

void say_push (const vector<string> & /* v */)
{
}






string trim (const string &s, trim_modes trim_mode)
{
  std::string::size_type i, j;
  for (i = 0; i < s.length() && isspace (s[i]); i ++)
    ;
  if (i == s.length()) return "";
  if ((trim_mode == TRIM_UNDERSCORE   &&  s[i] == '_') ||
      (trim_mode == TRIM_BRACE        &&  s[i] == '['))
    ++ i;
  if (i == s.length()) return "";
  for (j = s.length() - 1; isspace (s[j]); j --)
    ;
  if ((trim_mode == TRIM_UNDERSCORE && i < s.length() && s[j] == '_') ||
      (trim_mode == TRIM_BRACE && i < s.length()      && s[j] == ']'))
    -- j;
  return s.substr (i, j-i+1);
}

/* bool is_balanced (string)
 *
 * Decides whether the string has balanced braces (and needn't be deinlined)
 * - Track the nesting depth, starting at first lbrace to end of line
 * - If it is ever at depth 0 before the end, it's balanced
 * - Otherwise, it's unbalanced
 */
bool is_balanced (string str)
{
  std::string::size_type index = str.find ('{');
  if (index == string::npos)
    return true;
  int depth;
  for (depth = 1, index ++;  depth > 0 && index < str.length();  index ++)
    if (str[index] == '{')
      ++ depth;
    else if (str[index] == '}')
      -- depth;
  return depth == 0;
}

void show_find (string s, char ch)
{
  QUESTION_DBG << "Finding '" << ch << "' in '" << s << "': " << s.find(ch)+1 << endl;
}

void show_trim (string s)
{
  QUESTION_DBG << "Trimming '" << s << "': spaces (" << trim (s)
       << "), underscores (" << trim (s, TRIM_UNDERSCORE)
       << "), braces (" << trim (s, TRIM_BRACE) << ").\n";

}

template<class T> vector<T> &operator<< (vector<T> &v, T val)
{
  v.push_back (val);
  return v;
}

template<class T> class makevector {
  vector<T> dat;
public:
  makevector<T> &operator << (T it) { dat.push_back(it); return *this; }
  operator vector<T>() { return dat; }
};

vector <string> split (string s, char ch) {
  std::string::size_type i = 0, j;
  vector<string> rv;
  do
    {
      j = s.find (ch, i);
      if (i != j)
	rv.push_back (s.substr (i, j-i));
      i = j + 1;
    }
  while (j < s.length());

  return rv;
}

