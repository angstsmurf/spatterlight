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
  /* One sized write into the reused buffer rather than reserve + push_back per
   * char (which was a profile hotspot): resize once, then fill directly. */
  size_t tn = type.size (), nn = name.size ();
  out.resize (tn + 1 + nn);
  char *p = &out[0];
  std::memcpy (p, type.data (), tn);
  p[tn] = '\1';
  char *q = p + tn + 1;
  for (size_t i = 0; i < nn; i++)
    {
      unsigned char c = (unsigned char) name[i];
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

  // SENSITIVE?
  //std::map<std::string, std::vector<int>, CI_LESS>::const_iterator iter;
  std::map<std::string, std::vector<size_t> >::const_iterator iter;
  iter = type_indecies.find(type);
  if (iter == type_indecies.end())
    {
      return 0;
    }
  return (*iter).second.size(); 
}


void GeasFile::ensure_cached (const GeasBlock &b) const
{
  if (b.cache_built)
    return;
  b.cache_built = true;

  std::string::size_type c1, c2;
  for (const string &line : b.data)
    {
      string tok = first_token (line, c1, c2);

      if (tok == "type")
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
       * with property/action/type lines, so handle and skip the rest. */
      if (tok == "command" || tok == "lib")
	{
	  string htok = tok;
	  if (htok == "lib")   /* "lib command <...>": a library-supplied command */
	    htok = next_token (line, c1, c2);
	  if (htok == "command")
	    {
	      string p = next_token (line, c1, c2);
	      if (is_param (p))
		{
		  GeasBlock::cmd_entry e;
		  e.patterns = split_param (param_contents (p));
		  e.script = (c2 + 1 < line.length ()) ? line.substr (c2 + 1) : "";
		  b.commands.push_back (std::move (e));
		}
	      else
		debug_print ("Bad command line: " + line);
	    }
	  continue;
	}
      if (tok == "beforeturn" || tok == "afterturn")
	{
	  /* Script starts after the keyword, or after a following "override". */
	  std::string::size_type scr_starts = c2;
	  bool ov = (next_token (line, c1, c2) == "override");
	  if (ov)
	    scr_starts = c2;
	  GeasBlock::hook_entry h {ov, line.substr (scr_starts)};
	  if (tok == "beforeturn")
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
      if (tok == "properties")
	{
	  string p = next_token (line, c1, c2);
	  if (is_param (p))
	    for (const string &j : split_param (param_contents (p)))
	      {
		std::string::size_type idx;
		if (starts_with (j, "not "))
		  b.obj_prop.push_back ({false, trim (j.substr (4)), "!", false});
		else if ((idx = j.find ('=')) != string::npos)
		  b.obj_prop.push_back ({false, trim (j.substr (0, idx)),
					 j.substr (idx + 1), true});
		else
		  b.obj_prop.push_back ({false, j, "", true});
	      }
	}
      /* Action lines: object-style keeps the script after "<name> " (substr
       * c2+1); type-style keeps it from c2 -- matching the two original loops. */
      else if (tok == "action")
	{
	  string p = next_token (line, c1, c2);
	  if (is_param (p))
	    {
	      string name = param_contents (p);
	      b.obj_act.push_back ({false, name,
				    (c2 + 1 < line.length ()) ? line.substr (c2 + 1) : "",
				    true});
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
//set<string, CI_LESS> GeasFile::get_obj_keys (string obj) const
set<string> GeasFile::get_obj_keys (const string &obj) const
{ 
  //set<string, CI_LESS> rv;
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
      // SENSITIVE?
      if (tok == "properties")
	{
	  tok = next_token (line, c1, c2);
	  if (is_param(tok))
	    {
	      vector<string> params = split_param (param_contents (tok));
	      for (const auto &j: params)
		{
		  GEAS_DBG << "   handling parameter <" << j << ">\n";
		  std::string::size_type k = j.find('=');
		  // SENSITIVE?
		  if (starts_with (j, "not "))
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
      // SENSITIVE?
      else if (tok == "type")
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
      // SENSISTIVE?
      if (tok == "type")
	{
	  tok = next_token (line, c1, c2);
	  if (is_param(tok))
	    {
	      get_type_keys (param_contents(tok), rv);
	      GEAS_DBG << "      g_t_k: Adding <" << tok << "> to rv: " << rv << "\n";
	    }
	}
      // SENSITIVE?
      else if (tok == "action")
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

bool GeasFile::get_obj_property (const string &objname, const string &propname, string &string_rv, const string &preferred_parent) const
{
  GEAS_DBG << "g_o_p: Getting prop <" << propname << "> of obj <" << objname << ">\n";
  string_rv = "!";
  bool bool_rv = false;



  auto oti = obj_types.find (objname);
  if (oti == obj_types.end ())
    {
      debug_print ("Checking nonexistent object <" + objname + "> for property <" + propname + ">");
      return false;
    }
  const string &objtype = oti->second;

  const GeasBlock *block = find_by_name (objtype, objname, preferred_parent);

  if (block == NULL)
    {
      gi->debug_print ("get_obj_property: no block for object '" + objname + "'");
      return false;
    }
  ensure_cached (*block);
  for (const GeasBlock::dir &d: block->obj_prop)
    {
      if (d.is_type)
	get_type_property (d.a, propname, bool_rv, string_rv);
      else if (d.a == propname)
	{
	  string_rv = d.b;
	  bool_rv = d.bv;
	}
    }
  GEAS_DBG << "g_o_p: Ultimately returning " << (bool_rv ? "true" : "false")
       << ", with string <" << string_rv << ">\n\n";
  return bool_rv;
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
      else if (d.a == propname)
	{
	  bool_rv = d.bv;
	  string_rv = d.b;
	}
    }
}
	      


bool GeasFile::obj_of_type (const string &objname, const string &typenamex) const
{
  auto oti = obj_types.find (objname);
  if (oti == obj_types.end ())
    {
      debug_print ("Checking nonexistent obj <" + objname + "> for type <" +
		   typenamex + ">");
      return false;
    }
  const string &objtype = oti->second;

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



bool GeasFile::get_obj_action (const string &objname, const string &propname, string &string_rv, const string &preferred_parent) const
{
  GEAS_DBG << "g_o_a: Getting action <" << propname << "> of object <" << objname << ">\n";
  string_rv = "!";
  bool bool_rv = false;

  auto oti = obj_types.find (objname);
  if (oti == obj_types.end ())
    {
      debug_print ("Checking nonexistent object <" + objname + "> for action <" + propname + ">.");
      return false;
    }
  const string &objtype = oti->second;

  //reserved_words *rw;

  const GeasBlock *block = find_by_name (objtype, objname, preferred_parent);
  if (block == NULL)
    {
      gi->debug_print ("get_obj_action: no block for object '" + objname + "'");
      return false;
    }
  ensure_cached (*block);
  for (const GeasBlock::dir &d: block->obj_act)
    {
      if (d.is_type)
	get_type_action (d.a, propname, bool_rv, string_rv);
      else if (d.a == propname)
	{
	  string_rv = d.b;
	  bool_rv = true;
	}
    }

  GEAS_DBG << "g_o_a: Ultimately returning value " << (bool_rv ? "true" : "false")  << ", with string <" << string_rv << ">\n\n";

  return bool_rv;
}

bool GeasFile::get_obj_default_action (const string &objname, string &string_rv) const
{
  auto oti = obj_types.find (objname);
  if (oti == obj_types.end ())
    {
      debug_print ("Checking nonexistent object <" + objname + "> for default action.");
      return false;
    }
  const GeasBlock *block = find_by_name (oti->second, objname);
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
      if (tok == "" || tok == "alt" || tok == "alias" || tok == "properties" ||
	  tok == "action" || tok == "type" || tok == "drop")
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
      else if (d.a == actname)
	{
	  bool_rv = true;
	  string_rv = d.b;
	}
    }
}
 
void GeasFile::register_block (const string &blockname, const string &blocktype)
{
  auto it = obj_types.find (blockname);
  if (it != obj_types.end ())
    {
      /* Quest allows the same object name in several rooms (e.g. a "Colony
       * Ship" object defined in three space rooms).  Don't abort the whole
       * game over it -- warn and keep the first registration. */
      debug_print ("Duplicate block name <" + blockname + "> of type <" +
		   blocktype + ">; keeping the existing <" +
		   it->second + ">.");
      return;
    }
  obj_types[blockname] = blocktype;
}

string GeasFile::static_svar_lookup (const string &varname) const
{
  //varname = lcase (varname);
  for (size_t i = 0; i < size("variable"); i ++)
    if (ci_equal (blocks[i].name, varname))
      {
	string rv;
	string tok;
	std::string::size_type c1, c2;
	bool found_typeline = false;
	for (size_t j = 0; j < blocks[i].data.size(); j ++)
	  {
	    string line = blocks[i].data[j];
	    tok = first_token (line, c1, c2);
	    // SENSITIVE?
	    if (tok == "type")
	      {
		tok = next_token (line, c1, c2);
		// SENSITIVE?
		if (tok == "numeric")
		  throw string ("Trying to evaluate int var '" + varname + 
				"' as string");
		// SENSITIVE?
		if (tok != "string")
		  throw string ("Bad variable type " + tok);
		found_typeline = true;
	      }
	    // SENSITIVE?
	    else if (tok == "value")
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
  debug_print ("Variable <" + varname + "> not found.");
  return "";
}

string GeasFile::static_ivar_lookup (const string &varname) const
{
  //varname = lcase (varname);
  for (size_t i = 0; i < size("variable"); i ++)
    {
      if (ci_equal (blocks[i].name, varname))
	{
	  string rv;
	  string tok;
	  std::string::size_type c1=0, c2;
	  for (const string &line: blocks[i].data)
	    {
	      tok = first_token (line, c1, c2);
	      // SENSITIVE?
	      if (tok == "type")
		{
		  tok = next_token (line, c1, c2);
		  // SENSITIVE?
		  if (tok == "string")
		    throw string ("Trying to evaluate string var '" + varname + 
				  "' as numeric");
		  // SENSITIVE?
		  if (tok != "numeric")
		    throw string ("Bad variable type " + tok);
		}
	      // SENSITIVE?
	      else if (tok == "value")
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
	  if (j == input.length())
	    throw string ("Error processing '" + input + "', odd hashes");
	  size_t k;
	  for (k = i + 1; k < j && input[k] != ':'; k ++)
	    ;
	  if (k == ':')
	    {
	      string objname;
	      if (input[i+1] == '(' && input[k-1] == ')')
		objname = static_svar_lookup (input.substr (i+2, k-i-4));
	      else
		objname = input.substr (i+1, k-i-2);
	      GEAS_DBG << "  objname == '" << objname << endl;
	      //rv += get_obj_property (objname, input.substr (k+1, j-k-2));
	      string tmp;
	      bool had_var;
	      
	      string objprop = input.substr (k+1, j-k-2);
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
	  for (j = i; j < input.length() && input[j] != '%'; j ++)
	    ;
	  if (j == input.length())
	    throw string ("Error processing '" + input + "', unmatched %");
	  rv += static_ivar_lookup (input.substr (i+1, j-i-2));
	  i = j;
	}
      else
	rv += input[i];
    }
  if (rv != input)
    GEAS_DBG << "*** CHANGED ***\n";
  return rv;
}
