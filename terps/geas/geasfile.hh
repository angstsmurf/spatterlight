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

#ifndef __geasfile_hh
#define __geasfile_hh

#include "general.hh"
#include <vector>
#include <string>
#include <ostream>
#include <iostream>
#include <set>
#include <map>
#include <unordered_map>

class GeasInterface;

class reserved_words;

struct GeasBlock
{
  ////// lname == lowercase name
  ////// nname == normal name
  ////// parent == initial parent object (lowercased)
  // name == name
  // parent == initial parent object
  std::string blocktype, name, parent;
  std::vector<std::string> data;
  //GeasBlock (const std::vector<std::string> &, std::string, uint, bool);
  GeasBlock () {}

  /* Parsed cache of `data`, built lazily on first property/action lookup (see
   * GeasFile::ensure_cached).  The original getters re-tokenized every raw line
   * of `data` on every call, and those getters sit inside per-object loops, so a
   * single turn re-parsed the same text thousands of times.  Since `data` never
   * changes after load, we tokenize each block once into ordered directives and
   * the getters then walk these with no string parsing.
   *
   * A `dir` is either a type-recursion (is_type, the parent type's name in `a`)
   * or a value assignment (key in `a`, value in `b`, truthiness in `bv`).  Order
   * is preserved so the original "last assignment wins" semantics hold.  Object
   * blocks and type blocks parse properties/actions differently, hence the four
   * separate lists; a block is only ever read in one style, so the unused lists
   * cost a little build time but are never walked. */
  struct dir { bool is_type; std::string a, b; bool bv; };
  mutable bool cache_built = false;
  mutable std::vector<dir> obj_prop, obj_act, type_prop, type_act;
  mutable std::vector<std::string> parent_types;

  /* Per-turn block directives (room & game blocks): the command patterns and
   * beforeturn/afterturn hooks, parsed once so run_command/run_commands need
   * not re-tokenize every line of the room and game blocks on every turn (the
   * dominant tokenization cost once property/action lookups were cached).
   * `script` is the runnable tail of the line; for a command, `patterns` is the
   * split list of surface patterns to match. */
  struct cmd_entry { std::vector<std::string> patterns; std::string script; };
  struct hook_entry { bool is_override; std::string script; };
  mutable std::vector<cmd_entry> commands;
  mutable std::vector<hook_entry> beforeturns, afterturns;
};

struct GeasFile
{
  GeasInterface *gi;
  void debug_print (const std::string &s) const;

  //vector<GeasBlock> rooms, objects, textblocks, functions, procedures, types;
  //GeasBlock synonyms, game;
  std::vector <GeasBlock> blocks;

  //std::vector<GeasBlock> rooms, objects, textblocks, functions, procedures,
  //  types, synonyms, game, variables, timers, choices;
  /* Keyed by exact object name -> block type; only ever probed by find (never
   * iterated in order), so an unordered_map gives O(1) lookups instead of the
   * red-black-tree string compares that dominated the profile. */
  std::unordered_map <std::string, std::string> obj_types;
  std::map <std::string, std::vector<size_t> > type_indecies;

  /* Fast lookup for find_by_name: maps "<blocktype>\1<lowercased name>" to the
   * indices (into blocks) of every block with that type and name, in definition
   * order.  Populated alongside type_indecies as blocks are parsed, so
   * find_by_name no longer has to scan every block of a type. */
  std::unordered_map <std::string, std::vector<size_t> > name_index;
  static std::string name_key (const std::string &type, const std::string &name);
  /* Build a name_index key ("<type>\1<ascii-lowercased name>") into `out`,
   * reusing its capacity.  find_by_name uses this with a shared scratch buffer
   * (name_key_scratch) so the hot path allocates no temporary key string. */
  static void build_name_key (std::string &out, const std::string &type,
			      const std::string &name);
  mutable std::string name_key_scratch;

  void register_block (const std::string &blockname, const std::string &blocktype);

  const GeasBlock &block (const std::string &type, size_t index) const;
  size_t size (const std::string &type) const;

  /* Tokenize a block's raw `data` into its directive cache once (see GeasBlock).
   * No-op if already built; callable from const getters (cache is mutable). */
  void ensure_cached (const GeasBlock &b) const;

  void read_into (const std::vector<std::string>&, const std::string &, uint, bool, const reserved_words&, const reserved_words&);



  GeasFile () {}
  explicit GeasFile (const std::vector<std::string> &in_data,
		     GeasInterface *gi);

  bool obj_has_property (const std::string &objname, const std::string &propname) const;
  /* preferred_parent: when several blocks share this object name (Quest allows
   * the same name in different rooms), prefer the one defined in that room. */
  bool get_obj_property (const std::string &objname, const std::string &propname,
			 std::string &rv,
			 const std::string &preferred_parent = "") const;

  void get_type_property (const std::string &typenamex, const std::string &propname,
			  bool &, std::string &) const;
  bool obj_of_type (const std::string &object, const std::string &type) const;
  bool type_of_type (const std::string &subtype, const std::string &supertype) const;

  std::set<std::string> get_obj_keys (const std::string &) const;
  void get_obj_keys (const std::string &, std::set<std::string> &) const;
  void get_type_keys (const std::string &, std::set<std::string> &) const;

  bool obj_has_action (const std::string &objname, const std::string &propname) const;
  bool get_obj_action (const std::string &objname, const std::string &propname,
		       std::string &rv,
		       const std::string &preferred_parent = "") const;
  /* Returns the object's anonymous "default action" (the unlabelled script
   * block in the object definition), which Quest runs for the OPEN verb when
   * no explicit action/property handles it. */
  bool get_obj_default_action (const std::string &objname, std::string &rv) const;
  void get_type_action (const std::string &typenamex, const std::string &propname,
			bool &, std::string &) const;
  std::string static_eval (const std::string &) const;
  std::string static_ivar_lookup (const std::string &varname) const;
  std::string static_svar_lookup (const std::string &varname) const;

  const GeasBlock *find_by_name (const std::string &type, const std::string &name,
				 const std::string &preferred_parent = "") const;
};

extern std::ostream &operator << (std::ostream &, const GeasBlock &);
extern std::ostream &operator << (std::ostream &, const GeasFile &);



#endif
