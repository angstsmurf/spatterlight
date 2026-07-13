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

#ifndef __geas_impl_hh
#define __geas_impl_hh

#include "GeasRunner.hh"
#include "geas-state.hh"
#include "LimitStack.hh"
#include "general.hh"

struct match_binding
{
  std::string var_name;
  std::string var_text;
  uint start, end;
  //operator std::string();
  std::string tostring();
  match_binding (std::string vn, uint i) : var_name (vn), start (i) {}
  void set (std::string vt, uint i) { var_text = vt; end = i; }
};

extern std::ostream &operator<< (std::ostream &, const match_binding &);


struct match_rv
{
  bool success;
  std::vector<match_binding> bindings;
  //match_rv (bool b, const std::vector<std::string> &v) : success(b), bindings(v) {}
  match_rv () : success(false) {}
  match_rv (bool b, const match_rv &rv) : success(b), bindings (rv.bindings) {}
  operator bool () { return success; }
};

extern std::ostream &operator<< (std::ostream &o, const match_rv &rv);
/*
  inline ostream &operator<< (ostream &o, const match_rv &rv) 
{
  //o << "match_rv {" << (rv.success ? "TRUE" : "FALSE") << ": " << rv.bindings << "}"; 
  o << "match_rv {" << (rv.success ? "TRUE" : "FALSE") << ": [";
  //o << rv.bindings.size();
  //o << rv.bindings;
  for (uint i = 0; i < rv.bindings.size(); i ++)
    o << rv.bindings[i] << ", ";
  o << "]}"; 
  return o; 
}
*/

class geas_implementation : public GeasRunner
{
  //GeasInterface *gi;
  GeasFile gf;
  //bool running;
  /* `outputting` gates every print path, and `dont_process` the command
   * dispatch, but both are only assigned once a game has loaded: a game that
   * fails to parse would leave them indeterminate while the interface is still
   * live.  Default to the values a fresh game gets. */
  bool dont_process = false, outputting = true;
  /* Depth of the undo history.  GeasState snapshots are large (a full copy of
   * the world is taken every turn), so this is a deliberately modest fixed cap
   * rather than unlimited undo.  LimitStack is a ring buffer holding this many
   * slots, one of which is the sentinel, so the player gets kUndoLevels - 1
   * actual undos.  Defined once here and used at both the initial-construction
   * and RESTART sites so the two cannot drift apart. */
  static constexpr unsigned kUndoLevels = 20;
  LimitStack <UndoState> undo_buffer;
  std::vector <std::string> function_args;
  std::string this_object;
  /* Most recently referenced object, used to resolve pronouns ("it", etc.).
   * Mutable because object resolution happens in const helpers. */
  mutable std::string last_object;
  /* Support for the "oops <correction>" command: the command currently being
   * processed, and the split of the last command that failed on an unrecognised
   * object word (the parts before/after that word), set in the const
   * dereference_vars -- hence mutable. */
  std::string current_command;
  mutable std::string oops_before, oops_after;
  mutable bool oops_ready = false;
  v2string current_places;
  bool is_running_;
  std::string story_filename;   /* used as the save-file's game tag */
  /* How this session started, for Quest's $loadmethod$: "normal" for a fresh
   * game, "loaded" once a saved game has been restored.  A session property
   * (not part of GeasState), so undo does not change it. */
  std::string load_method_ = "normal";
  Logger logger;

public:
  geas_implementation (GeasInterface *in_gi)
     : GeasRunner (in_gi), undo_buffer (kUndoLevels), is_running_(true) {}
  //void set_game (std::string s);
  void set_game (const std::string &s);
  std::string save_state (bool run_hooks = true);
  bool load_state (const std::string &data, bool run_hooks = true);
  /* Run every "<keyword> <script>" line in the game block (e.g. beforesave /
   * onload) in file order, in the game's context. */
  void run_game_event (const std::string &keyword);
  std::string get_location ();
  void restart ();
  bool undo ();
  bool timer_will_fire ();
  bool has_timers () { return !state.timers.empty (); }

  bool is_running () const;
  std::string get_banner ();
  void run_command (const std::string &);
  bool try_match (std::string s, bool, bool);
  match_rv match_command (const std::string &input, const std::string &action) const;
  match_rv match_command (const std::string &input, uint ichar,
			  const std::string &action, uint achar, match_rv rv) const;
  bool dereference_vars (std::vector<match_binding> &bindings, bool is_internal) const;
  bool dereference_vars (std::vector<match_binding>&, const std::vector<std::string>&, bool is_internal) const;
  bool match_object (const std::string &text, const std::string &name, bool is_internal = false, bool allow_partial = true) const;
  void set_vars (const std::vector<match_binding> &v);
  bool run_commands (std::string, const GeasBlock *, bool is_internal = false);

  void display_error (std::string errorname, std::string object = "");

  std::string substitute_synonyms (std::string) const;

  void set_svar (const std::string &, const std::string &);
  void set_svar (const std::string &, size_t, const std::string &);
  void set_ivar (const std::string &, int);
  void set_ivar (const std::string &, double);
  void set_ivar (const std::string &, size_t, double);

  std::string get_svar (const std::string &) const;
  std::string get_svar (const std::string &, size_t) const;
  int get_ivar (const std::string &) const;
  int get_ivar (const std::string &, size_t) const;
  /* Raw double value of a numeric variable (get_ivar rounds to int). */
  double get_dvar (const std::string &) const;
  double get_dvar (const std::string &, size_t) const;

  bool find_ivar (const std::string &, size_t &) const;
  bool find_svar (const std::string &, size_t &) const;

  void regen_var_look ();
  void regen_var_dirs ();
  void regen_var_objects ();
  void regen_var_room ();

  void look();

  std::string displayed_name (const std::string &object) const;
  //std::string get_obj_name (const std::vector<std::string> &args) const;
  std::string get_obj_name (const std::string &name, const std::vector<std::string> &where, bool is_internal) const;

  bool has_obj_property (const std::string &objname, const std::string &propname) const;
  bool get_obj_property (const std::string &objname, const std::string &propname,
			 std::string &rv) const;
  bool has_obj_action (const std::string &obj, const std::string &prop) const;
  bool get_obj_action (const std::string &objname, const std::string &actname,
		       std::string &rv) const;
  /* Run obj's `action <key>` (as a script) or, failing that, print its
     `properties <key=...>`; returns true if either existed.  Captures the
     action-then-property idiom the look/examine/read/remove handlers share. */
  bool dispatch_obj_verb (const std::string &obj, const std::string &key);
  /* Collect, in display order and de-duplicated, the verbs that mean something
     for this object: the universal engine verbs, the built-in multi-synonym
     verbs it responds to, its own `action <verb>` definitions, and any global
     `verb` declaration it handles.  Backs the "verbs <object>" command, which
     mirrors Quest 4's right-click verb context menu. */
  std::vector<std::string> object_verbs (const std::string &obj) const;
  std::string exit_dest (const std::string &room, const std::string &dir, bool *is_act = NULL) const;
  /* Quest locked exits ("<dir> locked <dest; lockmessage>", plus lock/unlock
   * commands).  exit_lock_message returns the declared message (or "").  An
   * exit is locked if explicitly toggled (a property on the synthetic
   * "!exitlock" object) or, absent that, declared locked in the room data. */
  bool exit_locked (const std::string &room, const std::string &dir);
  std::string exit_lock_message (const std::string &room, const std::string &dir) const;
  /* True if the room statically declares "<dir> locked <dest; msg>"; sets
   * message to that (possibly empty) lockmessage.  Shared by the two above. */
  bool exit_declared_locked (const std::string &room, const std::string &dir,
			     std::string &message) const;
  std::vector<std::vector<std::string> > get_places (const std::string &room);

  void set_obj_property (const std::string &obj, const std::string &prop);
  void set_obj_action (const std::string &obj, const std::string &act);
  void move (const std::string &obj, const std::string &dest);
  /* Takes room by value on purpose: a caller may pass a reference into
   * current_places, which regen_var_dirs() reallocates partway through. */
  void goto_room (std::string room);
  std::string get_obj_parent (const std::string &obj);
  /* True if the player is carrying this -- either an object in the inventory or
   * a Quest 2.x item (which has no world object). */
  bool is_held (const std::string &name) const;
  /* True if some non-hidden object in scope is named exactly by this word
   * (used so a synonym doesn't shadow a real object of that name). */
  bool names_object_in_scope (const std::string &word) const;
  /* True if "name" is a reachable, non-hidden, *open* container object (so its
   * contents are reachable too). */
  bool container_in_scope (const std::string &name, const std::vector<std::string> &where) const;
  /* A container's open/closed state: open if opened at runtime or declared
   * "opened", closed if closed at runtime, and (per Quest) closed by default. */
  bool container_is_open (const std::string &name) const;
  /* True if object `obj` is declared inside container `name` (bare "<name>"). */
  bool declared_inside (const std::string &obj, const std::string &name) const;
  /* Display names of the objects currently inside container/surface `name`. */
  std::vector<std::string> container_contents (const std::string &name) const;
  /* Parent object name of `obj` ("" if unplaced). */
  std::string obj_parent (const std::string &obj) const;
  /* The room/inventory an object ultimately sits in, walking up through any
   * containers/surfaces it is nested in. */
  std::string room_of (const std::string &obj) const;
  /* Depth cap on every walk up the object parent chain.  A game can create a
   * cycle in that chain (put the bag in the box, then the box in the bag), so
   * an unbounded walk is a hang / stack overflow, not a theoretical concern. */
  static constexpr int kMaxContainerDepth = 64;
  /* True if `outer` is `inner` itself or already nested inside it -- i.e. moving
   * inner into outer would make the parent chain a cycle. */
  bool is_inside (const std::string &inner, const std::string &outer) const;
  /* Whether the interior of container/surface P currently makes its contents
   * available (open|transparent & seen, or a surface; and P itself reachable). */
  bool content_available (const std::string &P) const;
  /* Quest's UpdateVisibilityInContainers: set each contained object's hidden
   * flag from its container's state, so pane/here/exists/resolution agree. */
  void update_container_visibility ();
  bool sweeping = false;   /* re-entry guard for update_container_visibility */
  /* Default "open <container>" behaviour: mark the container open and reveal/
   * list its contents -- both objects declared inside with a bare "<container>"
   * line (which we un-hide) and objects parented to it.  Returns false if the
   * object is neither a container nor holds anything, so the caller can fall
   * back to "You can't do that". */
  bool open_container (const std::string &name);
  /* "close <container>": mark it closed (its contents leave scope) and re-hide
   * any bare-line contents still inside.  Returns false if not a container. */
  bool close_container (const std::string &name);

  void print_eval (const std::string &);
  void print_eval_p (const std::string &);
  std::string eval_string (const std::string &s);
  std::string eval_param (const std::string &s) { assert (is_param(s)); return eval_string (param_contents(s)); }


  void run_script_as (const std::string &, const std::string &);
  void run_script (const std::string &);
  void run_script (const std::string &, std::string &);
  void run_procedure (const std::string &);
  void run_procedure (const std::string &, std::vector<std::string> args);
  std::string run_function (const std::string &);
  std::string run_function (const std::string &, std::vector<std::string> args);
  std::string bad_arg_count (const std::string &);

  bool eval_conds (const std::string &);
  bool eval_cond (const std::string &);
  GeasState state;

  virtual void tick_timers();
  virtual v2string get_inventory();
  virtual v2string get_room_contents();
  v2string get_room_contents(const std::string &);
  virtual vstring get_room_exits();
  virtual vstring get_status_vars();
  virtual std::vector<bool> get_valid_exits();


  inline void print_formatted (const std::string &s) const { if (outputting) gi->print_formatted(s); }
  inline void print_normal (const std::string &s) const { if (outputting) gi->print_normal(s); }
  inline void print_newline () const { if (outputting) gi->print_newline(); }

  // Report an unsupported or malformed-game-data condition without aborting.
  // These sites used to be assert()s, which crash the interpreter (asserts are
  // live even in release builds here). We always log on the debug channel, and
  // surface a notice to the player when we are in an output phase, so a stuck
  // game is at least explicable rather than a hard crash.
  inline void report_unsupported (const std::string &msg) const {
    gi->debug_print (msg);
    if (outputting)
      gi->print_normal ("\n[geas: " + msg + "]\n");
  }

  /*
  inline void print_formatted (std::string s) const {
    if (outputting)
      gi->print_formatted(s); 
    else
      gi->print_formatted ("{{" + s + "}}");
  }
  inline void print_normal (std::string s) const
  {
    if (outputting)
      gi->print_normal (s);
    else
      gi->print_normal("{{" + s + "}}");
  }
  inline void print_newline() const { 
    if (outputting)
      gi->print_newline();
    else
      gi->print_normal ("{{|n}}");
  }
  */
};

#endif
