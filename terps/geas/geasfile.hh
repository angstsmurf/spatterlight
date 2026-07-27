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
  /* How many lines of `data` the cache was built from.  `data` is normally done
   * growing before the first lookup, but not always: a definition-level
   * property whose text contains "#obj:property#" is resolved *while the block
   * is being read* (GeasFile::static_eval, as Quest resolves it in GetParameter
   * at load time), and that lookup would otherwise freeze a cache built from
   * the handful of lines read so far -- silently throwing away every property
   * of the object, including the one being defined. */
  mutable size_t cached_lines = 0;
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
  /* Object/room/procedure name -> block type.  Keyed by the ASCII-lowercased
   * name, because Quest's own name lookups are case-insensitive throughout
   * (GetRoomID, GetObjectIDNoAlias and friends all compare LCase to LCase), and
   * games do write a name with different capitalisation from its definition:
   * The Birthday Assignment defines `define room <EDock>` and reaches it with
   * `goto <Edock>`, which under an exact-match key found the room (find_by_name
   * folds case) but not its script, so the endgame silently did nothing.
   * Only ever probed by find (never iterated in order), so an unordered_map
   * gives O(1) lookups instead of the red-black-tree string compares that
   * dominated the profile. */
  std::unordered_map <std::string, std::string> obj_types;
  /* Fold `name` into the shared scratch buffer and look it up in obj_types;
   * NULL if there is no such block.  The scratch buffer keeps the hot property
   * and action lookups allocation-free, as build_name_key does for name_index. */
  const std::string *obj_type_of (const std::string &name) const;
  mutable std::string obj_type_scratch;
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
  /* Make `newname` resolve to `srcname`'s definition, for Quest's runtime
   * `clone <src; newname[; room]>` (ExecClone, V4Game.cs:4352-4400).  Quest
   * copies the whole ObjectType record, definition and all; geas keeps
   * definitions in the immutable GeasFile and only the mutable half of an
   * object in GeasState, so the clone shares the source's block instead of
   * duplicating it -- nothing ever writes to a block after load, and the
   * clone's own properties and actions land in GeasState::props under its new
   * name, so the two stay independent where it matters.  Aliasing also keeps
   * `blocks` from growing, which matters because callers hold
   * `const GeasBlock *` across script execution. */
  void register_clone (const std::string &srcname, const std::string &newname);

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
  /* The same lookup forced to the "room" namespace.  Quest keeps rooms and
   * objects in separate collections, so a game may name a container room after
   * the object the player sees (the standard "define room <box>" idiom); the
   * name alone therefore cannot say which of the two a lookup meant. */
  bool get_room_property (const std::string &roomname, const std::string &propname,
			  std::string &rv) const;

  void get_type_property (const std::string &typenamex, const std::string &propname,
			  bool &, std::string &) const;
  bool obj_of_type (const std::string &object, const std::string &type) const;
  bool type_of_type (const std::string &subtype, const std::string &supertype) const;

  std::set<std::string> get_obj_keys (const std::string &) const;
  void get_obj_keys (const std::string &, std::set<std::string> &) const;
  void get_type_keys (const std::string &, std::set<std::string> &) const;
  /* Flatten a type for the runtime `type <obj; typename>` statement: every
   * property (name, value -- "!" for a negated one) and action (name, script)
   * the type carries, recursing through included types in declaration order,
   * plus the names of every type visited.  Mirrors GetPropertiesInType and
   * ExecType's flattening (V4Game.cs:6257-6345, 4455-4476). */
  void flatten_type (const std::string &typenamex,
		     std::vector<std::pair<std::string, std::string>> &props,
		     std::vector<std::pair<std::string, std::string>> &actions,
		     std::vector<std::string> &included, int depth = 0) const;

  bool obj_has_action (const std::string &objname, const std::string &propname) const;
  bool get_obj_action (const std::string &objname, const std::string &propname,
		       std::string &rv,
		       const std::string &preferred_parent = "") const;
  /* Room-namespace counterpart of get_obj_action -- see get_room_property. */
  bool get_room_action (const std::string &roomname, const std::string &propname,
			std::string &rv) const;
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

private:
  /* The block-walking tails of get_obj_property / get_obj_action, once the
   * block has been resolved: inherited types first, then the block's own
   * entries, so a directly-defined one always wins. */
  bool block_property (const GeasBlock &block, const std::string &propname,
		       std::string &rv) const;
  bool block_action (const GeasBlock &block, const std::string &actname,
		     std::string &rv) const;
};

extern std::ostream &operator << (std::ostream &, const GeasBlock &);
extern std::ostream &operator << (std::ostream &, const GeasFile &);



#endif
