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
#include "geasfile.hh"
#include "reserved_words.hh"
#include "readfile.hh"
#include "geas-util.hh"
#include "geas-impl.hh"
#include "general.hh"
#include "istring.hh"

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

std::string GeasFile::name_key (const string &type, const string &name)
{
  return type + '\1' + lcase (name);
}

const GeasBlock *GeasFile::find_by_name (const string &type, const string &name,
					 const string &preferred_parent) const
{
  /* O(1) lookup via name_index instead of scanning every block of the type.
   * The candidate list is in definition order, so the first-match fallback and
   * the preferred-parent tie-break behave exactly as the old linear scan. */
  std::map<std::string, std::vector<size_t> >::const_iterator it =
    name_index.find (name_key (type, name));
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



  if (!has (obj_types, objname))
    {
      debug_print ("Checking nonexistent object <" + objname + "> for property <" + propname + ">");
      return false;
    }
  string objtype = (*obj_types.find(objname)).second;

  const GeasBlock *block = find_by_name (objtype, objname, preferred_parent);

  string not_prop = "not " + propname;
  std::string::size_type c1, c2;
  if (block == NULL)
    {
      gi->debug_print ("get_obj_property: no block for object '" + objname + "'");
      return false;
    }
  for (const string &line: block->data)
    {
      string tok = first_token (line, c1, c2);
      // SENSITIVE?
      if (tok == "type")
	{
	  tok = next_token (line, c1, c2);
	  if (is_param (tok))
	    get_type_property (param_contents(tok), propname, bool_rv, string_rv);
	  else
	    {
	      debug_print ("Expected parameter for type in " + line);
	    }
	}
      // SENSITIVE?
      else if (tok == "properties")
	{
	  tok = next_token (line, c1, c2);
	  if (!is_param(tok))
	    {
	      debug_print ("Expected param on line " + line);
	      continue;
	    }
	  vector<string> props = split_param (param_contents (tok));
	  for (const string &j: props)
	    {
	      std::string::size_type index;
	      if (j == propname)
		{
		  string_rv = "";
		  bool_rv = true;
		}
	      else if (j == not_prop)
		{
		  string_rv = "!";
		  bool_rv = false;
		}
	      else if ((index = j.find ('=')) != string::npos &&
		       (trim (j.substr (0, index)) == propname))
		{
		  string_rv = j.substr (index+1);
		  bool_rv = true;
		}
	    }
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
  for (const string &line: block->data)
    {
      std::string::size_type c1, c2;
      string tok = first_token (line, c1, c2);

      // SENSITIVE?
      if (tok == "type")
	{
	  tok = next_token (line, c1, c2);
	  if (is_param (tok))
	    get_type_property (param_contents(tok), propname, bool_rv, string_rv);
	}
      else if (line == propname)
	{
	  bool_rv = true;
	  string_rv = "";
	}
      else
	{
	  c1 = line.find ('=');
	  if (c1 != string::npos)
	    {
	      tok = trim (line.substr (0, c1));
	      if (tok == propname)
		{
		  string_rv = trim (line.substr (c1 + 1));
		  bool_rv = true;
		}
	    }
	}
    }
}
	      


bool GeasFile::obj_of_type (const string &objname, const string &typenamex) const
{
  if (!has (obj_types, objname))
    {
      debug_print ("Checking nonexistent obj <" + objname + "> for type <" +
		   typenamex + ">");
      return false;
    }
  string objtype = (*obj_types.find(objname)).second;

  const GeasBlock *block = find_by_name (objtype, objname);

  std::string::size_type c1=0, c2;
  if (block == NULL)
    {
      gi->debug_print ("obj_of_type: no block for object '" + objname + "'");
      return false;
    }
  for (const string &line: block->data)
    {
      string tok = first_token (line, c1, c2);
      // SENSITIVE?
      if (tok == "type")
	{
	  tok = next_token (line, c1, c2);
	  if (is_param (tok))
	    {
	      if (type_of_type (param_contents(tok), typenamex))
		return true;
	    }
	  else
	    {
	      debug_print ("Eg_o_p: xpected parameter for type in " + line);
	    }
	}
    }
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
  for (const string &line: block->data)
    {
      std::string::size_type c1=0, c2;
      string tok = first_token (line, c1, c2);
      // SENSITIVE?
      if (tok == "type")
	{
	  tok = next_token (line, c1, c2);
	  if (is_param (tok) && type_of_type (param_contents(tok), supertype))
	    return true;
	}
    }
  return false;
}



bool GeasFile::get_obj_action (const string &objname, const string &propname, string &string_rv, const string &preferred_parent) const
{
  GEAS_DBG << "g_o_a: Getting action <" << propname << "> of object <" << objname << ">\n";
  string_rv = "!";
  bool bool_rv = false;

  if (!has (obj_types, objname))
    {
      debug_print ("Checking nonexistent object <" + objname + "> for action <" + propname + ">.");
      return false;
    }
  string objtype = (*obj_types.find(objname)).second;

  //reserved_words *rw;

  const GeasBlock *block = find_by_name (objtype, objname, preferred_parent);
  string not_prop = "not " + propname;
  std::string::size_type c1, c2;
  for (const string &line: block->data)
    {
      string tok = first_token (line, c1, c2);
      // SENSITIVE?
      if (tok == "type")
	{
	  tok = next_token (line, c1, c2);
	  if (is_param (tok))
	    get_type_action (param_contents(tok), propname, bool_rv, string_rv);
	  else
	    {
	      gi->debug_print ("Expected parameter for type in " + line);
	    }
	}
      // SENSITIVE?
      else if (tok == "action")
	{
	  tok = next_token (line, c1, c2);
	  if (is_param(tok) && param_contents(tok) == propname)
	    {
	      if (c2 + 1 < line.length())
		string_rv = line.substr (c2 + 1);
	      else
		string_rv = "";
	      bool_rv = true;
	      GEAS_DBG << "   Action line, string_rv now <" << string_rv << ">\n";
	    }
	}
    }

  GEAS_DBG << "g_o_a: Ultimately returning value " << (bool_rv ? "true" : "false")  << ", with string <" << string_rv << ">\n\n";

  return bool_rv;
}

bool GeasFile::get_obj_default_action (const string &objname, string &string_rv) const
{
  if (!has (obj_types, objname))
    {
      debug_print ("Checking nonexistent object <" + objname + "> for default action.");
      return false;
    }
  const GeasBlock *block = find_by_name ((*obj_types.find(objname)).second, objname);
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
  for (const string &line: block->data)
    {
      std::string::size_type c1, c2;
      string tok = first_token (line, c1, c2);
      // SENSITIVE?
      if (tok == "action")
	{
	  tok = next_token (line, c1, c2);
	  if (is_param (tok) && param_contents(tok) == actname)
	    {
	      bool_rv = true;
	      string_rv = line.substr (c2);
	    }
	}
      // SENSITIVE?
      else if (tok == "type")
	{
	  tok = next_token (line, c1, c2);
	  if (is_param (tok))
	    get_type_action (param_contents(tok), actname, bool_rv, string_rv);
	}
    }
}
 
void GeasFile::register_block (const string &blockname, const string &blocktype)
{
  if (has (obj_types, blockname))
    {
      /* Quest allows the same object name in several rooms (e.g. a "Colony
       * Ship" object defined in three space rooms).  Don't abort the whole
       * game over it -- warn and keep the first registration. */
      debug_print ("Duplicate block name <" + blockname + "> of type <" +
		   blocktype + ">; keeping the existing <" +
		   obj_types[blockname] + ">.");
      return;
    }
  obj_types[blockname] = blocktype;
}

string GeasFile::static_svar_lookup (const string &varname) const
{
  //varname = lcase (varname);
  for (size_t i = 0; i < size("variable"); i ++)
    //if (blocks[i].lname == varname)
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
      //if (blocks[i].lname == varname)
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
