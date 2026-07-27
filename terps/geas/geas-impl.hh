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
  /* The game's declared ASL version, as an integer ("400" for asl-version
   * <400>).  Quest keeps two parallel implementations of several features and
   * picks between them on this number, so it has to be known before the first
   * room is entered.  Defaults to the oldest version geas supports for a game
   * that declares none. */
  int asl_version_ = 311;
  /* Quest's "define options / abbreviations on|off", which controls the second,
   * looser pass of noun matching: with abbreviations on, Disambiguate falls back
   * to matching the typed word against a whole-word run of the object's name
   * ("paper" -> "Wall Paper") when nothing matched exactly.  Quest gates that
   * pass on ASLVersion >= 391 and on this option (Disambiguate,
   * V4Game.cs:4730; the option is read in SetUpOptions,
   * V4Game.Part2.cs:884-895, and defaults to on, V4Game.Part2.cs:6404).
   * Games do turn it off to keep a decorative scenery object from swallowing a
   * puzzle item's alias (Permanant Room has both `define object <Wall Paper>'
   * and a `Peice of Paper' aliased "paper"). */
  bool use_abbreviations_ = true;
  /* Quest displays the game block's <intro> text section automatically once the
   * startscript has run, unless the startscript executed "nointro" -- games that
   * print their own credits sequence use that to show <intro> exactly where they
   * want it with an explicit `displaytext <intro>` (The Devil's Bargain).  Quest
   * clears the flag in ExecuteScript and tests it after the startscripts
   * (V4Game.Part2.cs:6000-6003 and 7946/7987), so it is session state, not game
   * state: undo cannot take you back to before the intro. */
  bool auto_intro_ = true;
  /* Quest 2.x "collectables": named numbers with a display string, declared
   * once in the game block as
   *     collectables <Money dp=100 [! Dollars]; Score p=1000 [! Points]>
   * and shown in the status pane (SetUpCollectables, V4Game.Part2.cs:6838-6935).
   * "Type" is a range spec that clamps the value after every change ("%" tops
   * out at 100, "p" cannot go below 0, "10r50" bounds both ends) and a leading
   * "d" means "hide this line while the value is zero".
   *
   * These declarations never change, so they are session state rather than game
   * state; only the values move, and those live in the ordinary numeric
   * variables under the reserved name collectable_var() gives them, so that
   * save, restore and undo carry them with no extra plumbing (the same trick as
   * the "!exitlock" pseudo-object). */
  struct CollectableDef
  {
    std::string name, type, display;
    bool display_when_zero = true;
    double init = 0.0;
  };
  std::vector<CollectableDef> collectables;
  Logger logger;

public:
  geas_implementation (GeasInterface *in_gi)
     : GeasRunner (in_gi), undo_buffer (kUndoLevels), is_running_(true) {}
  //void set_game (std::string s);
  void set_game (const std::string &s);
  std::string save_state (bool run_hooks = true);
  bool load_state (const std::string &data, bool run_hooks = true);
  /* Re-register the GeasFile definition aliases of runtime-cloned objects after
   * a state has been loaded (see the "!clones" trail). */
  void restore_clones ();
  std::string save_undo_history ();
  bool load_undo_history (const std::string &data);
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
  bool try_game_verb (const std::string &cmd, bool is_internal);
  bool try_match (std::string s, bool, bool);
  match_rv match_command (const std::string &input, const std::string &action) const;
  match_rv match_command (const std::string &input, uint ichar,
			  const std::string &action, uint achar, match_rv rv) const;
  bool dereference_vars (std::vector<match_binding> &bindings, bool is_internal) const;
  /* quiet_notfound suppresses the generic "You don't see any X." for the commands
   * whose scope is narrow enough that Quest has its own wording for the miss
   * (ExecUse's "noitem"/"badthing"); the caller prints it instead. */
  bool dereference_vars (std::vector<match_binding>&, const std::vector<std::string>&, bool is_internal,
			 bool quiet_notfound = false) const;
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

  /* Split a variable reference into its base name and subscript: "x" is
   * (x, 0), "arr[3]" is (arr, 3), and "arr[n]" is (arr, value of n).  Returns
   * false, after a diagnostic naming `who`, when the reference is malformed or
   * the subscript is unusable; every accessor below shares this so the five
   * spellings of the same parsing cannot drift apart. */
  bool split_var_index (const std::string &varname, const char *who,
			std::string &base, size_t &index) const;
  /* Ceiling on a variable subscript.  Quest grows an array to whatever index is
   * written, so this only has to be far beyond any real game's use: its job is
   * to stop a nonsense subscript -- an undefined index variable reads back as
   * the -32767 "no such variable" sentinel, which as a size_t is ~1.8e19 --
   * from reaching a resize() that throws std::length_error and aborts the
   * interpreter mid-turn. */
  static constexpr size_t kMaxVarIndex = 100000;

  /* Flatten type `typenm` onto `obj` at runtime -- the shared half of the
   * `type <obj; typenm>` statement and of 4.10 `create object` (ExecType,
   * V4Game.cs:4455-4476).  Records "!type <name>" membership markers, which
   * eval_cond's `type` condition consults beside the static block. */
  void apply_type (const std::string &obj, const std::string &typenm);

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
  /* The same three lookups for a name that is known to name a room.  A room and
     an object may share a name -- Quest keeps the two in separate collections
     and an object's name wins the unqualified lookup, so anything asking about
     the player's location has to say so.  See GeasFile::register_block. */
  bool has_room_property (const std::string &room, const std::string &prop) const;
  bool get_room_property (const std::string &room, const std::string &prop,
			  std::string &rv) const;
  bool get_room_action (const std::string &room, const std::string &act,
			std::string &rv) const;
  /* A "<tok> <param>" line in the game block, with the script that follows it:
     the lines of that block never become actions, so they need their own
     lookup.  Quest's FindLine. */
  bool find_game_block_line (const std::string &tok, const std::string &param,
			     std::string &rv) const;
  /* The runtime-property/action halves of the lookups above, shared by both
     forms: true if a record set during play answers the question. */
  bool runtime_prop (const std::string &obj, const std::string &prop,
		     bool &value, std::string &rv) const;
  bool runtime_action (const std::string &obj, const std::string &act,
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
  v2string object_verbs (const std::string &obj) const;
  std::string exit_dest (const std::string &room, const std::string &dir, bool *is_act = NULL) const;
  /* The exit for <dir> exactly as the room block declares it, ignoring anything
     created or destroyed at run time.  Returns "" if the block declares none;
     sets is_script when what comes back is a script rather than a destination. */
  std::string declared_exit_dest (const GeasBlock *gb, const std::string &dir,
				  bool &is_script) const;
  void split_exit_dest (const std::string &raw, std::string &dest,
			std::string &prefix) const;
  std::string eval_is_operand (const std::string &s) const;
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
  /* The names Quest gives a room's exit objects, in the order they appear in
   * _objs -- what "for each exit in <room>" walks.  See run_script's "for
   * each" handler for the naming rules. */
  std::vector<std::string> exit_object_names (const std::string &room);
  /* The room's compass directions in the order their exits were created (the
   * order the room's tags name them, then any made during play), whether or not
   * they still lead anywhere.  Quest's _directions dictionary order. */
  std::vector<std::string> exit_dir_order (const std::string &room);

  void set_obj_property (const std::string &obj, const std::string &prop);
  /* Resolve the Quest 2.x "name@room" way of addressing an object, in place.
   * Returns false when the reference names a room the object is not in, which is
   * Quest's way of saying "no such object". */
  bool resolve_at_room (std::string &name) const;
  void set_obj_action (const std::string &obj, const std::string &act);
  void move (const std::string &obj, const std::string &dest);
  /* Quest's DoAddRemove (V4Game.cs:2113-2133): containment is recorded as the
   * child's "parent" *property* (holding the container's definition name, and
   * cleared with "not parent"), which games read back as #child:parent#.
   * Barbarian's pedestal will only swap the sack for the Eye of the East when
   * `is <#Pile of bones:parent#; Sack>` holds -- and springs a lethal trap
   * otherwise.  geas keeps containment in ObjectRecord::parent instead, so the
   * property has to be written alongside the move or such a check never
   * matches.  Only the container statements and verbs route through here:
   * Quest's MoveThing (rooms) leaves the property alone.  TAKE reaches it
   * indirectly, by running "remove <object>" first -- see the take handler. */
  void do_add_remove (const std::string &child, const std::string &parent,
		      bool adding);
  /* Takes room by value on purpose: a caller may pass a reference into
   * current_places, which regen_var_dirs() reallocates partway through. */
  bool room_exists (const std::string &room) const;
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
  std::string room_of_parent (const std::string &parent) const;
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
  void update_container_visibility (const std::string &only_parent = "");
  bool sweeping = false;   /* re-entry guard for update_container_visibility */
  /* regen_var_objects is deferred: mutators (move, set_obj_property, clone,
   * goto) raise this flag and the derived quest.objects/quest.formatobjects
   * strings are rebuilt only when one of them is actually read (see the flush
   * in get_svar).  They used to be rebuilt ~2.6x per turn, which profiling
   * showed dominating large games (regen_var_objects was 68% of KQ5's
   * inclusive time). */
  bool objs_vars_dirty_ = false;
  /* Default "open <container>" behaviour: mark the container open and reveal/
   * list its contents -- both objects declared inside with a bare "<container>"
   * line (which we un-hide) and objects parented to it.  Returns false if the
   * object is neither a container nor holds anything, so the caller can fall
   * back to "You can't do that". */
  bool open_container (const std::string &name);
  /* "close <container>": mark it closed (its contents leave scope) and re-hide
   * any bare-line contents still inside.  Returns false if not a container. */
  bool close_container (const std::string &name);
  /* Quest's half of "remove <child>" / "remove <child> from <parent>": the
   * *container* decides, through its "remove" action (which is handed the item
   * in quest.remove.object.name and is responsible for the removal itself) or
   * its "remove" text property (after which the engine clears the child's
   * parent).  ExecAddRemove, V4Game.cs:2578-2624.  Returns false if the
   * container refused -- not a container, closed, or no remove of its own -- so
   * a caller doing this on the player's behalf can abandon its own action. */
  bool remove_from_container (const std::string &child, const std::string &parent);

  void print_eval (const std::string &);
  void print_eval_p (const std::string &);
  std::string eval_string (const std::string &s);
  /* The #/%/$ substitution pass on its own, without the trailing inline-brace
   * evaluation -- Quest runs the brace pass once, on the finished parameter. */
  std::string eval_string_body (const std::string &s);
  std::string eval_inline_exprs (const std::string &s);
  /* Quest's GetParameter.  Below ASL 3.20 a parameter whose contents start with
   * "~Internal" is handed back completely unconverted (V4Game.cs:1884-1893):
   * those are the procedure names QDK generates for a .cas file, and the ~ that
   * begins them would otherwise open a collectable reference. */
  std::string eval_param (const std::string &s)
  {
    assert (is_param(s));
    std::string p = param_contents (s);
    if (asl_version_ < 320 && p.compare (0, 9, "~Internal") == 0)
      return p;
    return eval_string (p);
  }

  /* Collectables (see the CollectableDef comment above). */
  void set_up_collectables ();
  /* Quest looks a collectable up by an exact, case-sensitive name match
   * (ExecuteSetCollectable, V4Game.cs:6059-6066); only the untyped "set" uses a
   * case-insensitive search to pick which kind of variable is meant
   * (SetUnknownVariableType, V4Game.Part2.cs:612-618), hence the two lookups. */
  bool find_collectable (const std::string &name, size_t &index) const;
  bool find_collectable_ci (const std::string &name, size_t &index) const;
  /* The reserved numeric-variable name a collectable's value is kept in. */
  std::string collectable_var (const std::string &name) const
  { return "!collectable;" + name; }
  /* Value of a named collectable, 0 if there is no such collectable
   * (GetCollectableAmount, V4Game.Part2.cs:6221-6232). */
  double get_collectable (const std::string &name) const;
  /* Store a value, first clamping it to the type's range (CheckCollectable,
   * V4Game.Part2.cs:3968-4022). */
  void set_collectable (size_t index, double value);
  /* "set collectable <name; +N>" -- also every bare "set" below ASL 2.80. */
  void run_set_collectable (const std::string &param);
  /* "if has <name; +N>" (ExecuteIfHas, V4Game.cs:7395-7448). */
  bool eval_has (const std::string &param);
  /* One collectable's status-pane line, or "<null>" when it has none
   * (DisplayCollectableInfo, V4Game.Part2.cs:4024-4073). */
  std::string collectable_display (size_t index) const;


  void run_script_as (const std::string &, const std::string &);
  void run_script (const std::string &);
  void run_script (const std::string &, std::string &);
  /* Depth cap on nested script execution.  A game can write a script that
   * re-enters itself with no way out -- the classic case is a variable whose
   * "onchange" script assigns to that same variable, since Quest fires onchange
   * on every assignment, not just value-changing ones (SetNumericVariableContents,
   * V4Game.Part2.cs:526).  Two such clamps that disagree ("if too big, shrink"
   * plus "if too small, grow") oscillate forever.  Quest itself only guards
   * `exec` (ExecExec, V4Game.Part2.cs:2197, limit 500) and would smash its own
   * stack here; we cap every nesting path so a buggy game file cannot take the
   * interpreter down with it.  Quest's number and its wording are reused.
   *
   * Lower under AddressSanitizer: its redzones inflate run_script's frame
   * several-fold, so 500 nested frames overflow the sanitizer's stack before
   * this guard ever trips (fixtures/scriptdepth.asl found this).  Production
   * builds keep Quest's number. */
#if defined(__has_feature)
# if __has_feature(address_sanitizer)
  static constexpr int kMaxScriptDepth = 100;
# else
  static constexpr int kMaxScriptDepth = 500;
# endif
#else
  static constexpr int kMaxScriptDepth = 500;
#endif
  int script_depth = 0;
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
  virtual v2string get_object_verbs(const std::string &);
  virtual v2string get_room_exits();
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
