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

#include <set>
#include <cstring>
#include "geasfile.hh"
#include "reserved_words.hh"
#include "readfile.hh"
#include "geas-util.hh"
#include "general.hh"
#include "istring.hh"
#include "geas-impl.hh"  // completes GeasInterface, used here via gi->

using namespace std;

reserved_words obj_tag_property ("look", "examine", "speak", "take", "alias", "prefix", "suffix", "detail", "displaytype", "gender", "article", "hidden", "invisible", (char *) NULL);



reserved_words room_tag_property ("look", "alias", "prefix", "indescription", "description", "north", "south", "east", "west", "northwest", "northeast", "southeast", "southwest", "up", "down", "out", (char *) NULL);

void GeasFile::debug_print (const string &s) const
{
  if (gi == NULL)
    {
      GEAS_DBG << s << endl;
    }
  else
    {
      gi->debug_print (s);
    }
}

void GeasFile::build_name_key (std::string &out, const string &type,
			      const string &name)
{
  /* Surrounding whitespace is not part of a name.  readfile.cc registers every
   * block under trim()'d name (see the comment there), so the lookup key has to
   * be trimmed too, or a game that spells the same name with a stray space in
   * one place and without it in another can never match its own definition.
   * Quest does neither -- it compares the raw parameter against the raw
   * definition, so both spellings have to agree exactly -- but since geas has
   * already folded the two spellings together in the definition table, folding
   * them here as well is what makes the two halves consistent.  "Something
   * 'Bout A Hex" needs it: the journal is `define object <Journal >' and the
   * knapsack hands it over with `give <Journal >', while `if got <Journal>'
   * elsewhere drops the space. */
  size_t beg = 0, end = name.size ();
  while (beg < end && isspace ((unsigned char) name[beg]))
    beg ++;
  while (end > beg && isspace ((unsigned char) name[end - 1]))
    end --;

  /* One sized write into the reused buffer rather than reserve + push_back per
   * char (which was a profile hotspot): resize once, then fill directly. */
  size_t tn = type.size (), nn = end - beg;
  out.resize (tn + 1 + nn);
  char *p = &out[0];
  std::memcpy (p, type.data (), tn);
  p[tn] = '\1';
  char *q = p + tn + 1;
  for (size_t i = 0; i < nn; i++)
    {
      unsigned char c = (unsigned char) name[beg + i];
      q[i] = (c >= 'A' && c <= 'Z') ? c + 32 : c;
    }
}

std::string GeasFile::name_key (const string &type, const string &name)
{
  /* Keep the same key bytes as build_name_key so load-time keys (readfile) and
   * lookup-time keys agree.  ASCII fold matches the old lcase() under C
   * locale. */
  std::string out;
  build_name_key (out, type, name);
  return out;
}

const GeasBlock *GeasFile::find_by_name (const string &type, const string &name,
					 const string &preferred_parent) const
{
  /* O(1) lookup via name_index instead of scanning every block of the type.
   * The candidate list is in definition order, so the first-match fallback and
   * the preferred-parent tie-break behave exactly as the old linear scan. */
  build_name_key (name_key_scratch, type, name);
  auto it = name_index.find (name_key_scratch);
  if (it == name_index.end ())
    return NULL;

  const GeasBlock *first = NULL;
  for (size_t idx: it->second)
    {
      const GeasBlock *b = &blocks[idx];
      if (first == NULL)
	first = b;
      /* When several blocks share this name, prefer the one defined in the
       * preferred room (so same-named objects in different rooms behave
       * per-room). */
      if (preferred_parent != "" && is_param (b->parent) &&
	  ci_equal (param_contents (b->parent), preferred_parent))
	return b;
    }
  return first;
}

const GeasBlock &GeasFile::block (const std::string &type, size_t index) const { 
  std::map<std::string, std::vector<size_t> >::const_iterator iter;
  iter = type_indecies.find(type);
  if (iter != type_indecies.end() && index < (*iter).second.size())
    return blocks[(*iter).second[index]];

  // Was assert(...) here, which aborts the interpreter whenever a game refers
  // to a block that wasn't parsed (the common geas crash). Return a shared
  // empty block instead: callers either compare .name (which won't match) or
  // iterate .data (which is empty), so they degrade gracefully. The reference
  // stays valid because the sentinel is static.
  static const GeasBlock empty_block;
  GEAS_DBG << "Unable to find type " << type << " at index " << index << "\n";
  return empty_block;
}


size_t GeasFile::size (const std::string &type) const {

  std::map<std::string, std::vector<size_t> >::const_iterator iter;
  iter = type_indecies.find(type);
  if (iter == type_indecies.end())
    {
      return 0;
    }
  return (*iter).second.size(); 
}


const GeasBlock::line_first &GeasFile::line_tok (const GeasBlock &b, size_t i) const
{
  /* Rebuild if `data` grew since (mirrors ensure_cached's cached_lines guard);
   * for the room blocks these scanners walk, `data` is done growing by the time
   * anyone navigates, so this builds exactly once. */
  if (b.first_tokens_lines != b.data.size ())
    {
      b.first_tokens.clear ();
      b.first_tokens.reserve (b.data.size ());
      std::string::size_type t1, t2;
      for (const std::string &l : b.data)
	{
	  std::string tok = first_token (l, t1, t2);
	  b.first_tokens.push_back ({std::move (tok), t2});
	}
      b.first_tokens_lines = b.data.size ();
    }
  return b.first_tokens[i];
}

void GeasFile::ensure_cached (const GeasBlock &b) const
{
  if (b.cache_built && b.cached_lines == b.data.size ())
    return;
  if (b.cache_built)
    {
      /* The block grew since the cache was built (see GeasBlock::cached_lines):
       * start over rather than append, so nothing is counted twice. */
      b.obj_prop.clear (); b.obj_act.clear ();
      b.type_prop.clear (); b.type_act.clear ();
      b.parent_types.clear ();
      b.commands.clear ();
      b.beforeturns.clear (); b.afterturns.clear ();
    }
  b.cache_built = true;
  b.cached_lines = b.data.size ();

  std::string::size_type c1, c2;
  for (const string &line : b.data)
    {
      string tok = first_token (line, c1, c2);

      /* Keyword compares here are CI: a `type` line and a raw (game-written)
       * properties/action line are read by Quest with the lowercasing
       * BeginsWith (GetPropertiesInType, V4Game.cs:6291-6328; InitialiseObject,
       * V4Game.Part2.cs:3378-3405). */
      if (ci_equal (tok, "type"))
	{
	  string p = next_token (line, c1, c2);
	  if (is_param (p))
	    {
	      string tn = param_contents (p);
	      b.parent_types.push_back (tn);
	      /* A `type` line means "inherit": object/type property and action
	       * lookups both recurse into the named type at this position. */
	      b.obj_prop.push_back  ({true, tn, "", false});
	      b.obj_act.push_back   ({true, tn, "", false});
	      b.type_prop.push_back ({true, tn, "", false});
	      b.type_act.push_back  ({true, tn, "", false});
	    }
	  /* Mirrors get_type_property/get_obj_property: a `type` first-token line
	   * never also contributes a bare/assigned property, so fall through. */
	  continue;
	}

      /* Per-turn block directives (room & game blocks).  These never overlap
       * with property/action/type lines, so handle and skip the rest.
       * All of these keywords are matched with the case-insensitive
       * BeginsWith (V4Game.cs:1094): "command " V4Game.Part2.cs:1215, the
       * "command"/"lib command" tags 2350-2375, "beforeturn "/"afterturn "
       * 1276/1281 (rooms) and 1387/1392 (game). */
      if (ci_equal (tok, "command") || ci_equal (tok, "lib"))
	{
	  bool is_lib = ci_equal (tok, "lib");
	  string htok = tok;
	  if (is_lib)   /* "lib command <...>": a library-supplied command */
	    htok = next_token (line, c1, c2);
	  if (ci_equal (htok, "command"))
	    {
	      string p = next_token (line, c1, c2);
	      if (is_param (p))
		{
		  GeasBlock::cmd_entry e;
		  e.patterns = split_param (param_contents (p));
		  e.script = (c2 + 1 < line.length ()) ? line.substr (c2 + 1) : "";
		  e.is_lib = is_lib;
		  b.commands.push_back (std::move (e));
		}
	      else
		debug_print ("Bad command line: " + line);
	    }
	  continue;
	}
      if (ci_equal (tok, "beforeturn") || ci_equal (tok, "afterturn"))
	{
	  /* Script starts after the keyword, or after a following "override"
	   * (also a case-insensitive BeginsWith: V4Game.Part2.cs:4205/4595). */
	  std::string::size_type scr_starts = c2;
	  bool ov = ci_equal (next_token (line, c1, c2), "override");
	  if (ov)
	    scr_starts = c2;
	  GeasBlock::hook_entry h {ov, line.substr (scr_starts)};
	  if (ci_equal (tok, "beforeturn"))
	    b.beforeturns.push_back (std::move (h));
	  else
	    b.afterturns.push_back (std::move (h));
	  continue;
	}

      /* Object-style property lines: `properties <a;b;c>` (read_into already
       * splits these one-per-line, but split_param keeps multi-item lines safe).
       * Each item is bare ("foo" -> true/""), negated ("not foo" -> false/"!"),
       * or assigned ("foo=bar" -> true/"bar").  We pre-classify here; the rare
       * pathological item (e.g. a literal property named "not foo") that the
       * original's query-dependent compares treated differently never occurs as
       * a real lookup. */
      if (ci_equal (tok, "properties"))
	{
	  string p = next_token (line, c1, c2);
	  if (is_param (p))
	    for (const string &j : split_param (param_contents (p)))
	      {
		std::string::size_type idx;
		GeasBlock::dir d;
		/* CI: BeginsWith "not " (AddToObjectProperties, V4Game.cs:4034). */
		if (starts_with_i (j, "not "))
		  d = {false, trim (j.substr (4)), "!", false};
		else if ((idx = j.find ('=')) != string::npos)
		  /* Quest trims both sides of the "=" (AddToObjectProperties,
		   * V4Game.cs:4021-4025), so "volume = 30" is the value 30, not
		   * " 30" -- which a game then prints, or does arithmetic on. */
		  d = {false, trim (j.substr (0, idx)),
		       trim (j.substr (idx + 1)), true};
		else
		  d = {false, j, "", true};
		b.obj_prop.push_back (d);
		/* Into the type-side list too: GetPropertiesInType accepts a
		 * "properties <...>" line inside a define type
		 * (V4Game.cs:6335-6337), accumulating it exactly like the bare
		 * lines below, and the object loader flattens the result onto
		 * the object (V4Game.Part2.cs:3388-3389).  Without this,
		 * get_type_property never saw such a line and the property was
		 * invisible through inheritance. */
		b.type_prop.push_back (d);
	      }
	}
      /* A tag line whose value never went through Quest's property list, and so
       * was never `;`-split: one property, the whole "name=value"
       * (readfile.cc's tag_property_splits). */
      else if (ci_equal (tok, "tagproperty"))
	{
	  string p = next_token (line, c1, c2);
	  std::string::size_type idx;
	  if (is_param (p) &&
	      (idx = param_contents (p).find ('=')) != string::npos)
	    {
	      string item = param_contents (p);
	      GeasBlock::dir d = {false, trim (item.substr (0, idx)),
				  trim (item.substr (idx + 1)), true};
	      b.obj_prop.push_back (d);
	      b.type_prop.push_back (d);
	    }
	}
      /* Action lines: object-style keeps the script after "<name> " (substr
       * c2+1); type-style keeps it from c2 -- matching the two original loops. */
      else if (ci_equal (tok, "action"))
	{
	  string p = next_token (line, c1, c2);
	  if (is_param (p))
	    {
	      string name = param_contents (p);
	      string script = (c2 + 1 < line.length ())
		? line.substr (c2 + 1) : "";
	      /* An action with nothing after the closing ">" is thrown away, not
	       * registered empty: AddToObjectActions logs "No script given for
	       * '<name>' action data" and returns (V4Game.cs:3934-3939).  The
	       * loader has already trimmed the line (V4Game.cs:1335), so "the
	       * first '>' is the last character" and "no script" are the same
	       * test.  Registering it shadowed the object's like-named
	       * `properties <verb = ...>` text, and the verb printed nothing --
	       * Sutekh Is Hiding In Your Priory has both on its television. */
	      if (trim (script).empty ())
		continue;
	      b.obj_act.push_back ({false, name, script, true});
	      b.type_act.push_back ({false, name, line.substr (c2), true});
	    }
	}

      /* Type-style property lines: a bare line is a flag named by the whole
       * line; a `name = value` line assigns.  (Only ever walked for blocks that
       * are actually `type`s; harmless extra entries on object blocks.) */
      std::string::size_type eq = line.find ('=');
      if (eq != string::npos)
	b.type_prop.push_back ({false, trim (line.substr (0, eq)),
				trim (line.substr (eq + 1)), true});
      else
	b.type_prop.push_back ({false, line, "", true});
    }
}

bool GeasFile::obj_has_property (const string &objname, const string &propname) const
{
  string tmp;
  return get_obj_property (objname, propname, tmp);
}

extern ostream &operator<< (ostream &, const set<string>&);

/**
 * Currently only works for actual objects, not rooms or the game
 */
set<string> GeasFile::get_obj_keys (const string &obj) const
{
  set<string> rv;
  get_obj_keys (obj, rv);
  return rv;
}

void GeasFile::get_obj_keys (const string &obj, set<string> &rv) const
{
  GEAS_DBG << "get_obj_keys (gf, <" << obj << ">)\n";
  //set<string> rv;

  std::string::size_type c1, c2;
  string tok, line;
  reserved_words *rw = NULL;

  const GeasBlock *gb = find_by_name ("object", obj);
  rw = &obj_tag_property;

  if (gb == NULL)
    {
      GEAS_DBG << "No such object found, aborting\n";
      return;
    }

  for (size_t i = 0; i < gb->data.size(); i ++)
    {
      line = gb->data[i];
      GEAS_DBG << "  handling line <" << line << ">\n";
      tok = first_token (line, c1, c2);
      /* CI keyword compares -- see ensure_cached. */
      if (ci_equal (tok, "properties"))
	{
	  tok = next_token (line, c1, c2);
	  if (is_param(tok))
	    {
	      vector<string> params = split_param (param_contents (tok));
	      for (const auto &j: params)
		{
		  GEAS_DBG << "   handling parameter <" << j << ">\n";
		  std::string::size_type k = j.find('=');
		  if (starts_with_i (j, "not "))
		    {
		      rv.insert (trim (j.substr(4)));
		      GEAS_DBG << "     adding <" << trim (j.substr(4))
			   << ">\n";
		    }
		  else if (k == string::npos)
		    {
		      rv.insert (j);
		      GEAS_DBG << "     adding <" << j << ">\n";
		    }
		  else
		    {
		      rv.insert (trim (j.substr(0, k)));
		      GEAS_DBG << "     adding <" << trim (j.substr(0, k))
			   << ">\n";
		    }
		}
	    }
	}
      /* An unsplit tag property (see ensure_cached): always "name=value". */
      else if (ci_equal (tok, "tagproperty"))
	{
	  tok = next_token (line, c1, c2);
	  std::string::size_type k;
	  if (is_param (tok) &&
	      (k = param_contents (tok).find ('=')) != string::npos)
	    rv.insert (trim (param_contents (tok).substr (0, k)));
	}
      else if (ci_equal (tok, "type"))
	{
	  tok = next_token (line, c1, c2);
	  if (is_param (tok))
	    get_type_keys (param_contents(tok), rv);
	}
      //else if (has (tag_property, tok) && tag_property[tok])
      else if (rw != NULL && rw->has(tok)) 
	{
	  string tok1 = next_token (line, c1, c2);
	  if (is_param (tok1))
	    rv.insert (tok);
        }
    }

  GEAS_DBG << "Returning (" << rv << ")\n";
}

void GeasFile::get_type_keys (const string &typen, set<string> &rv) const
{
  GEAS_DBG << "get_type_keys (" << typen << ", " << rv << ")\n";
  const GeasBlock* gb = find_by_name ("type", typen);
  if (gb == NULL)
    {
      GEAS_DBG << "  g_t_k: Nonexistent type\n";
      return;
    }
  string line, tok;
  std::string::size_type c1, c2;
  for (const string &line: gb->data)
    {
      tok = first_token (line, c1, c2);
      /* CI keyword compares -- see ensure_cached. */
      if (ci_equal (tok, "type"))
	{
	  tok = next_token (line, c1, c2);
	  if (is_param(tok))
	    {
	      get_type_keys (param_contents(tok), rv);
	      GEAS_DBG << "      g_t_k: Adding <" << tok << "> to rv: " << rv << "\n";
	    }
	}
      else if (ci_equal (tok, "action"))
	{
	  GEAS_DBG << "       action, skipping\n";
	}
      else
	{
	  std::string::size_type ch = line.find ('=');
	  if (ch != string::npos)
	    {
	      rv.insert (trim (line.substr (0, ch)));
	      GEAS_DBG << "      adding <" << trim (line.substr (0, ch)) << ">\n";
	    }
	}
    }
  GEAS_DBG << "Returning (" << rv << ")\n";
}

bool GeasFile::block_property (const GeasBlock &block, const string &propname, string &string_rv) const
{
  bool bool_rv = false;
  ensure_cached (block);
  /* One pass, in source order: last writer wins.  Quest reads a define block a
   * line at a time and applies each line where it stands, and a `type <...>`
   * line is a property source like any other -- it pushes the whole type
   * through the same AddToObjectProperties a `properties <...>` line uses, and
   * that overwrites an entry of the same name (InitialiseObject,
   * V4Game.Part2.cs:3398-3405; V4Game.cs:4042-4069).  So a type named *below* a
   * tag beats it, and only a tag below the type survives.  For the usual
   * type-first ordering this is the same answer as resolving the types first,
   * which is what geas used to do unconditionally; Operation: Sleepover writes
   * the other order eight times, and lost an adjective off each garment
   * ("a black sweater" for Quest's "a sweater"). */
  for (const GeasBlock::dir &d: block.obj_prop)
    if (d.is_type)
      get_type_property (d.a, propname, bool_rv, string_rv);
    else if (ci_equal (d.a, propname))
      {
	string_rv = d.b;
	bool_rv = d.bv;
      }
  GEAS_DBG << "g_o_p: Ultimately returning " << (bool_rv ? "true" : "false")
       << ", with string <" << string_rv << ">\n\n";
  return bool_rv;
}

bool GeasFile::get_obj_property (const string &objname, const string &propname, string &string_rv, const string &preferred_parent) const
{
  GEAS_DBG << "g_o_p: Getting prop <" << propname << "> of obj <" << objname << ">\n";
  string_rv = "!";

  const string *oti = obj_type_of (objname);
  if (oti == NULL)
    {
      debug_print ("Checking nonexistent object <" + objname + "> for property <" + propname + ">");
      return false;
    }
  const string &objtype = *oti;

  const GeasBlock *block = find_by_name (objtype, objname, preferred_parent);

  if (block == NULL)
    {
      gi->debug_print ("get_obj_property: no block for object '" + objname + "'");
      return false;
    }
  return block_property (*block, propname, string_rv);
}

bool GeasFile::get_room_property (const string &roomname, const string &propname, string &string_rv) const
{
  GEAS_DBG << "g_r_p: Getting prop <" << propname << "> of room <" << roomname << ">\n";
  string_rv = "!";
  const GeasBlock *block = find_by_name ("room", roomname);
  if (block == NULL)
    {
      debug_print ("Checking nonexistent room <" + roomname + "> for property <" + propname + ">");
      return false;
    }
  return block_property (*block, propname, string_rv);
}

void GeasFile::get_type_property (const string &typenamex, const string &propname, bool &bool_rv, string &string_rv) const
{
  const GeasBlock *block = find_by_name ("type", typenamex);
  if (block == NULL)
    {
      debug_print ("Object of nonexistent type " + typenamex);
      return;
    }
  ensure_cached (*block);
  for (const GeasBlock::dir &d: block->type_prop)
    {
      if (d.is_type)
	get_type_property (d.a, propname, bool_rv, string_rv);
      else if (ci_equal (d.a, propname))   /* Quest property names are case-insensitive */
	{
	  bool_rv = d.bv;
	  string_rv = d.b;
	}
    }
}
	      


void GeasFile::flatten_type (const string &typenamex,
			     std::vector<std::pair<std::string, std::string>> &props,
			     std::vector<std::pair<std::string, std::string>> &actions,
			     std::vector<std::string> &included, int depth) const
{
  /* A type may include itself through a cycle; GetPropertiesInType would
   * recurse forever there too, so any sane bound is fine. */
  if (depth > 32)
    return;
  const GeasBlock *block = find_by_name ("type", typenamex);
  if (block == NULL)
    {
      debug_print ("No such type '" + typenamex + "'");
      return;
    }
  ensure_cached (*block);
  included.push_back (typenamex);
  for (const GeasBlock::dir &d: block->type_prop)
    {
      if (d.is_type)
	flatten_type (d.a, props, actions, included, depth + 1);
      else
	props.push_back ({d.a, d.bv ? d.b : "!"});
    }
  for (const GeasBlock::dir &d: block->type_act)
    if (!d.is_type)
      actions.push_back ({d.a, d.b});
}

bool GeasFile::obj_of_type (const string &objname, const string &typenamex) const
{
  const string *oti = obj_type_of (objname);
  if (oti == NULL)
    {
      debug_print ("Checking nonexistent obj <" + objname + "> for type <" +
		   typenamex + ">");
      return false;
    }
  const string &objtype = *oti;

  const GeasBlock *block = find_by_name (objtype, objname);

  if (block == NULL)
    {
      gi->debug_print ("obj_of_type: no block for object '" + objname + "'");
      return false;
    }
  ensure_cached (*block);
  for (const string &tn: block->parent_types)
    if (type_of_type (tn, typenamex))
      return true;
  return false;
}


bool GeasFile::type_of_type (const string &subtype, const string &supertype) const
{
  if (ci_equal (subtype, supertype))
    {
      return true;
    }
  const GeasBlock *block = find_by_name ("type", subtype);
  if (block == NULL)
    {
      debug_print ("t_o_t: Nonexistent type " + subtype);
      return false;
    }
  ensure_cached (*block);
  for (const string &tn: block->parent_types)
    if (type_of_type (tn, supertype))
      return true;
  return false;
}



bool GeasFile::block_action (const GeasBlock &block, const string &actname, string &string_rv) const
{
  bool bool_rv = false;
  ensure_cached (block);
  /* One pass in source order, as in block_property: AddObjectAction overwrites
   * a like-named action just as AddToObjectProperties overwrites a property
   * (V4Game.cs:3784-3816), and the loader runs both from the same `type <...>`
   * line, so an `action <rub>` written above a type that also defines `rub`
   * loses to it. */
  for (const GeasBlock::dir &d: block.obj_act)
    if (d.is_type)
      get_type_action (d.a, actname, bool_rv, string_rv);
    else if (ci_equal (d.a, actname))
      {
	string_rv = d.b;
	bool_rv = true;
      }

  GEAS_DBG << "g_o_a: Ultimately returning value " << (bool_rv ? "true" : "false")  << ", with string <" << string_rv << ">\n\n";

  return bool_rv;
}

bool GeasFile::get_obj_action (const string &objname, const string &propname, string &string_rv, const string &preferred_parent) const
{
  GEAS_DBG << "g_o_a: Getting action <" << propname << "> of object <" << objname << ">\n";
  string_rv = "!";

  const string *oti = obj_type_of (objname);
  if (oti == NULL)
    {
      debug_print ("Checking nonexistent object <" + objname + "> for action <" + propname + ">.");
      return false;
    }
  const string &objtype = *oti;

  const GeasBlock *block = find_by_name (objtype, objname, preferred_parent);
  if (block == NULL)
    {
      gi->debug_print ("get_obj_action: no block for object '" + objname + "'");
      return false;
    }
  return block_action (*block, propname, string_rv);
}

bool GeasFile::get_room_action (const string &roomname, const string &actname, string &string_rv) const
{
  GEAS_DBG << "g_r_a: Getting action <" << actname << "> of room <" << roomname << ">\n";
  string_rv = "!";
  const GeasBlock *block = find_by_name ("room", roomname);
  if (block == NULL)
    {
      debug_print ("Checking nonexistent room <" + roomname + "> for action <" + actname + ">.");
      return false;
    }
  return block_action (*block, actname, string_rv);
}

bool GeasFile::get_obj_default_action (const string &objname, string &string_rv) const
{
  const string *oti = obj_type_of (objname);
  if (oti == NULL)
    {
      debug_print ("Checking nonexistent object <" + objname + "> for default action.");
      return false;
    }
  const GeasBlock *block = find_by_name (*oti, objname);
  if (block == NULL)
    return false;
  std::string::size_type c1, c2;
  /* Every line in a parsed object block is prefixed with a keyword
   * (alt/alias/properties/action/type/drop, plus a possible leading
   * "type <default>").  The one line with no such prefix is the anonymous
   * default-action script, e.g. "do <open the box>" or "msg <...>". */
  for (const string &line: block->data)
    {
      string tok = first_token (line, c1, c2);
      /* CI, so a mixed-case keyword line is never mistaken for the anonymous
       * default action. */
      if (tok == "" || ci_equal (tok, "alt") || ci_equal (tok, "alias") ||
	  ci_equal (tok, "properties") || ci_equal (tok, "tagproperty") ||
	  ci_equal (tok, "action") ||
	  ci_equal (tok, "type") || ci_equal (tok, "drop"))
	continue;
      string_rv = trim (line);
      return true;
    }
  return false;
}

void GeasFile::get_type_action (const string &typenamex, const string &actname, bool &bool_rv, string &string_rv) const
{
  const GeasBlock *block = find_by_name ("type", typenamex);
  if (block == NULL)
    {
      debug_print ("Object of nonexistent type " + typenamex);
      return;
    }
  ensure_cached (*block);
  for (const GeasBlock::dir &d: block->type_act)
    {
      if (d.is_type)
	get_type_action (d.a, actname, bool_rv, string_rv);
      else if (ci_equal (d.a, actname))   /* Quest action names are case-insensitive */
	{
	  bool_rv = true;
	  string_rv = d.b;
	}
    }
}
 
/* Fold to lower case in place, matching the ASCII fold build_name_key uses (and
 * lcase() under the C locale), without allocating a fresh string per call.
 * Surrounding whitespace is dropped for the same reason build_name_key drops it:
 * block names are registered trimmed, so keys derived from them must be too. */
static void fold_lower_into (std::string &out, const string &s)
{
  size_t beg = 0, end = s.size ();
  while (beg < end && isspace ((unsigned char) s[beg]))
    beg ++;
  while (end > beg && isspace ((unsigned char) s[end - 1]))
    end --;
  size_t n = end - beg;
  out.resize (n);
  for (size_t i = 0; i < n; i++)
    {
      unsigned char c = (unsigned char) s[beg + i];
      out[i] = (c >= 'A' && c <= 'Z') ? (char) (c + 32) : (char) c;
    }
}

const std::string *GeasFile::obj_type_of (const string &name) const
{
  fold_lower_into (obj_type_scratch, name);
  auto it = obj_types.find (obj_type_scratch);
  return it == obj_types.end () ? NULL : &it->second;
}

void GeasFile::register_block (const string &blockname, const string &blocktype)
{
  std::string key;
  fold_lower_into (key, blockname);
  auto it = obj_types.find (key);
  if (it != obj_types.end ())
    {
      /* A room and an object may legitimately share a name: Quest keeps rooms
       * in _rooms and objects in _objs, so "define room <box>" (the interior a
       * container's contents are parked in) coexists with "define object <box>"
       * (the box the player sees).  This map answers the question "what is
       * <name>?" for lookups that did not say, and those are object lookups --
       * the room half is asked for by name through find_by_name ("room", ...)
       * or get_room_property.  So let an object or character displace a room.
       *
       * "The Devil's Bargain" has five such pairs (box, desk, wallet,
       * briefcase, panel 579); with the room winning, examining the desk
       * printed the room's look text, the room prefix was pasted onto the
       * object in every listing, and "here <desk>" was false, so its whole
       * container library refused to hand anything over. */
      if ((blocktype == "object" || blocktype == "character") &&
	  it->second == "room")
	{
	  it->second = blocktype;
	  return;
	}
      /* Quest allows the same object name in several rooms (e.g. a "Colony
       * Ship" object defined in three space rooms).  Don't abort the whole
       * game over it -- warn and keep the first registration. */
      debug_print ("Duplicate block name <" + blockname + "> of type <" +
		   blocktype + ">; keeping the existing <" +
		   it->second + ">.");
      return;
    }
  obj_types[key] = blocktype;
}

void GeasFile::register_clone (const string &srcname, const string &newname)
{
  std::string srckey, newkey;
  fold_lower_into (srckey, srcname);
  fold_lower_into (newkey, newname);
  if (srckey == newkey || newkey == "")
    return;

  /* What is <newname>?  Whatever the source was.  Set before the definition
   * aliases so a cloned room still answers "room" to obj_type_of. */
  auto ot = obj_types.find (srckey);
  if (ot != obj_types.end () && obj_types.find (newkey) == obj_types.end ())
    obj_types[newkey] = ot->second;

  /* Point the new name at the source's definition block(s).  name_index keys
   * are "<blocktype>\1<lowercased name>", so every key whose name half is the
   * source gets a twin whose name half is the clone -- which covers the
   * object/character block and, for a cloned room, the room block too, without
   * this having to know which of them the game meant.  Collect first: inserting
   * into an unordered_map while iterating it may rehash.
   *
   * An existing definition of that name is left alone.  Quest appends the clone
   * to the end of _objs while GetObjectIdNoAlias returns the *first* match, so
   * a name that is already defined in the file keeps its own definition -- and
   * geas's obj_records is in the same definition order, so the record side
   * agrees. */
  std::vector<std::pair<std::string, std::vector<size_t> > > twins;
  for (const auto &e: name_index)
    {
      const std::string &k = e.first;
      if (k.size () > srckey.size () + 1 &&
	  k[k.size () - srckey.size () - 1] == '\1' &&
	  k.compare (k.size () - srckey.size (), srckey.size (), srckey) == 0)
	twins.push_back (std::make_pair
			 (k.substr (0, k.size () - srckey.size ()) + newkey,
			  e.second));
    }
  for (const auto &t: twins)
    if (name_index.find (t.first) == name_index.end ())
      name_index[t.first] = t.second;
}

string GeasFile::static_svar_lookup (const string &varname) const
{
  /* block("variable", i), not blocks[i]: size("variable") counts the variable
   * blocks, while blocks[] holds every block of every type in file order, so
   * indexing one by the other's count scanned unrelated blocks (typically the
   * game block and the first rooms) and never reached the real variables. */
  for (size_t i = 0; i < size("variable"); i ++)
    {
      const GeasBlock &vb = block ("variable", i);
      if (ci_equal (vb.name, varname))
      {
	string rv;
	string tok;
	std::string::size_type c1, c2;
	bool found_typeline = false;
	for (size_t j = 0; j < vb.data.size(); j ++)
	  {
	    string line = vb.data[j];
	    tok = first_token (line, c1, c2);
	    /* The keywords are CI (BeginsWith, V4Game.Part2.cs:704-731); the
	     * type *value* is not -- Quest compares the raw text (ibid. 707),
	     * so "type String" is an unrecognised type there and here. */
	    if (ci_equal (tok, "type"))
	      {
		tok = next_token (line, c1, c2);
		if (tok == "numeric")
		  throw string ("Trying to evaluate int var '" + varname +
				"' as string");
		if (tok != "string")
		  throw string ("Bad variable type " + tok);
		found_typeline = true;
	      }
	    else if (ci_equal (tok, "value"))
	      {
		tok = next_token (line, c1, c2);
		if (!is_param (tok))
		  throw string ("Expected param after value in " + line);
		rv = param_contents (tok);
	      }
	  }
	if (!found_typeline)
	  throw string (varname + " is a numeric variable");
	GEAS_DBG << "static_svar_lookup(" << varname << ") -> \"" << rv << "\"" << endl;
	return rv;
      }
    }
  debug_print ("Variable <" + varname + "> not found.");
  return "";
}

string GeasFile::static_ivar_lookup (const string &varname) const
{
  /* block("variable", i), not blocks[i] -- see static_svar_lookup. */
  for (size_t i = 0; i < size("variable"); i ++)
    {
      const GeasBlock &vb = block ("variable", i);
      if (ci_equal (vb.name, varname))
	{
	  string rv;
	  string tok;
	  std::string::size_type c1=0, c2;
	  for (const string &line: vb.data)
	    {
	      tok = first_token (line, c1, c2);
	      /* CI keywords, CS type value -- see static_svar_lookup. */
	      if (ci_equal (tok, "type"))
		{
		  tok = next_token (line, c1, c2);
		  if (tok == "string")
		    throw string ("Trying to evaluate string var '" + varname +
				  "' as numeric");
		  if (tok != "numeric")
		    throw string ("Bad variable type " + tok);
		}
	      else if (ci_equal (tok, "value"))
		{
		  tok = next_token (line, c1, c2);
		  if (!is_param (tok))
		    throw string ("Expected param after value in " + line);
		  rv = param_contents (tok);
		}
	    }
	  return rv;
	}
    }
  debug_print ("Variable <" + varname + "> not found");
  return "-32768";
}

string GeasFile::static_eval (const string &input) const
{
  string rv = "";
  for (size_t i = 0; i < input.length(); i ++)
    {
      if (input[i] == '#')
	{
	  size_t j;
	  for (j = i+1; j < input.length() && input[j] != '#'; j ++)
	    ;
	  /* An unmatched conversion character is not fatal in Quest: each pass of
	   * ConvertParameter (V4Game.cs:6697-6701) reports
	   * "Line parameter <...> has missing #" to the log -- a WarningError, so
	   * nothing the player sees -- and yields the string "<ERROR>" for the
	   * whole parameter, after which play continues normally.  Throwing here
	   * instead escaped all the way out to set_game's catch, which abandoned
	   * the rest of startup and left the game silent from the first turn on;
	   * "The Statue of Riddles" has a room described as "100% empty" and was
	   * unplayable for that one stray percent sign.
	   *
	   * Quest stores the literal text "<ERROR>" as the value instead, and this
	   * is the only chance to do the same: it resolves definition-level text
	   * once at load, exactly as we do here (SetUpRoomData calls GetParameter on
	   * a room's "look" line, V4Game.Part2.cs:1170-1172), and never re-converts
	   * it when the room is shown.  We cannot, though -- these results are
	   * re-emitted as "properties <name=value>" lines, and next_token
	   * (readfile.cc:56-62) ends a <...> token at the *first* '>', so "<ERROR>"
	   * would be read back as "<ERROR".  Leaving the text alone thus prints the
	   * raw "100% empty" where Quest prints "<ERROR>"; the runtime paths
	   * (eval_string, for msg and script parameters) do emit the marker, and
	   * those are the ones games actually build text with. */
	  if (j == input.length())
	    {
	      debug_print ("Line parameter <" + input + "> has missing #");
	      return input;
	    }
	  /* Two conversion characters in a row are a literal one: Quest's
	   * ConvertParameter has an explicit empty-name case that appends the
	   * character and moves on (V4Game.cs:6704-6708).  eval_string has it
	   * (geas-runner.cc:7895-7896); this, its load-time twin, did not, so a
	   * room aliased "Room ##1" -- RiddleRun's five are -- looked up a string
	   * variable with no name and came out as "Room 1". */
	  if (j == i + 1)
	    {
	      rv += "#";
	      i = j;
	      continue;
	    }
	  size_t k;
	  for (k = i + 1; k < j && input[k] != ':'; k ++)
	    ;
	  /* k < j means we found a ':', i.e. this is "#object:property#" rather
	   * than a plain "#variable#".  The test used to be "k == ':'", comparing
	   * the *index* against the character ':' (58), so this branch effectively
	   * never ran -- and every substring length inside it was left off by one,
	   * unnoticed.  Those are corrected below. */
	  if (k < j)
	    {
	      string objname;
	      if (input[i+1] == '(' && input[k-1] == ')')
		objname = static_svar_lookup (input.substr (i+2, k-i-3));
	      else
		objname = input.substr (i+1, k-i-1);
	      GEAS_DBG << "  objname == '" << objname << endl;
	      //rv += get_obj_property (objname, input.substr (k+1, j-k-2));
	      string tmp;
	      bool had_var;
	      
	      string objprop = input.substr (k+1, j-k-1);
	      GEAS_DBG << "  objprop == " << objprop << endl;
	      had_var = get_obj_property (objname, objprop, tmp);
	      rv += tmp;
	      if (!had_var)
		debug_print ("Requesting nonexistent property <" + objprop +
			     "> of object <" + objname + ">");
	    }
	  else
	    {
	      GEAS_DBG << "i == " << i << ", j == " << j << ", length is " << input.length() << endl;
	      GEAS_DBG << "Looking up static var " << input.substr (i+1, j-i-1) << endl;
	      rv += static_svar_lookup (input.substr (i+1, j-i-1));
	    }
	  i = j;
	}
      else if (input[i] == '%')
	{
	  size_t j;
	  /* Start past the opening '%': starting *at* it matched immediately, so the
	   * name came out empty, the substring length underflowed to a huge count,
	   * and i never advanced past the '%'. */
	  for (j = i + 1; j < input.length() && input[j] != '%'; j ++)
	    ;
	  if (j == input.length())
	    {
	      debug_print ("Line parameter <" + input + "> has missing %");
	      return input;
	    }
	  /* Same empty-name case as the '#' branch above -- and louder here,
	   * because a numeric variable that does not exist reads as "-32768",
	   * so "100%% sure" came out as "100-32768 sure". */
	  if (j == i + 1)
	    rv += "%";
	  else
	    rv += static_ivar_lookup (input.substr (i+1, j-i-1));
	  i = j;
	}
      /* The fourth conversion character.  GetParameter runs a dollar pass too
       * (ConvertType.Functions, V4Game.cs:1897), and we cannot: it calls user
       * functions, which need a running game, and this is load time.  The
       * empty-name case, though, needs no game at all -- a doubled dollar is a
       * literal one (V4Game.cs:6705-6708) -- and it is the half that reaches
       * the player, because a definition-level "$name$" is a rarity while
       * prices are not.  Pure Chaos describes a hoard as "It's about
       * $1000000."; the pair used to survive into the room description
       * verbatim. */
      else if (input.compare (i, 2, "$$") == 0)
	{
	  rv += "$";
	  i ++;
	}
      else
	rv += input[i];
    }
  if (rv != input)
    GEAS_DBG << "*** CHANGED ***\n";
  return rv;
}
