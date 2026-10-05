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

#ifndef __question_state_hh
#define __question_state_hh

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <unordered_map>
#include "general.hh"

struct PropertyRecord
{
  std::string name, data;
  PropertyRecord (std::string in_name, std::string in_data) : name (in_name), data (in_data) {}
};

struct ObjectRecord
{
  std::string name, parent;
  bool hidden, invisible;
  /* True for the placeholder record every "define room" gets.  Quest keeps
     rooms in _rooms and objects in _objs, so a game may name a container room
     after the object the player sees ("define room <box>" beside "define
     object <box>"); Question keeps both in one vector, and a lookup by name has to
     be able to tell them apart.  Not serialized: rooms are the only records
     built with hidden and invisible both set, so a loaded state derives it. */
  bool is_room;
  
  //ObjectRecord (std::string in_name, std::string in_parent) : name (in_name), parent (in_parent), hidden (false), concealed (false) {}

};

struct ExitRecord
{
  std::string src, dest;
  ExitRecord (const std::string &in_src, const std::string &in_dest) : src(in_src), dest(in_dest) {}
};

struct TimerRecord
{
  std::string name;
  bool is_running;
  /* Quest counts a timer *up* and compares the count against the interval as
   * it stands on each tick -- "TimerTicks = TimerTicks + elapsedTime; if
   * (TimerTicks >= TimerInterval) { TimerTicks = 0; …}"
   * (V4Game.Part2.cs:126-131).  For a fixed interval that is the same clock as
   * counting down and re-arming, but `set interval' moves the interval under a
   * cycle already in flight, and only the count-up form lets the change take
   * effect within that cycle -- a shortened interval can fire at once off the
   * ticks already banked, and a lengthened one applies immediately rather than
   * from the cycle after next.  The Pilgrim's Progress steps its "sinking in
   * mire" timer down from 15 to 10 to 5 from inside its own action. */
  uint interval, elapsed;
  /* Quest's TimerType.BypassThisTurn: "don't trigger timer during the turn it
   * was first enabled" (SetTimerState, V4Game.Part2.cs:561-574).  Every
   * timeron/timeroff raises it, and the next tick spends it instead of
   * counting.  Not part of the save format -- Quest saves TimerTicks and not
   * this flag (V4Game.Part2.cs:344) -- so it defaults to false on load. */
  bool bypass = false;
};

/* A variable is an array of values under one name; a plain variable is the
 * array's element 0.  Quest keeps strings and numbers in two separate arrays,
 * and this is what their records share. */
template <class T>
struct VarRecord
{
protected:
  std::vector<T> data;
public:
  std::string name;

  VarRecord () {}
  VarRecord (const std::string &in_name, const T &val = T ()) : name (in_name) { set (0, val); }
  size_t size() const { return data.size(); }
  /* Highest defined index.  A default-constructed record holds no elements at
   * all, so guard the subtraction: an unsigned size() - 1 would wrap to
   * SIZE_MAX, which the serializer would then happily write out as this
   * array's upper bound. */
  size_t max() const { return size() ? size() - 1 : 0; }
  void set (size_t i, const T &val) { if (i >= size()) data.resize(i+1); data[i] = val; }
  void set (const T &val) { data[0] = val; }
};

struct SVarRecord : VarRecord<std::string>
{
  using VarRecord<std::string>::VarRecord;

  /* An index past the end reads as the empty string, not as a sentinel:
   * GetStringContents logs "Array index ... too big" and returns ""
   * (V4Game.Part2.cs:2637-2643).  It used to answer "!", which is a marker for
   * a missing *property*, and it went straight into game text -- Wizard's
   * spell menu printed "4) !" for each of its nine empty slots. */
  std::string get (size_t i) const { if (i < size()) return data[i]; return "";}
  std::string get() const { return data[0]; }
  /* Element i as a save file holds it. */
  std::string stored (size_t i) const { return get (i); }
};

/* Quest numeric variables are doubles; we store them as such so fractional
 * results (e.g. probabilities) survive.  get()/get(i) round to int for the
 * many callers that need an integer (array indices, loop bounds); getd()
 * exposes the raw double for formatting and float math. */
struct IVarRecord : VarRecord<double>
{
private:
  /* Clamped first: casting a double outside int's range is undefined. */
  static int as_int (double d)
  {
    if (!(d > -2147483648.5)) return d != d ? 0 : -2147483647 - 1;
    if (!(d < 2147483647.5)) return 2147483647;
    return (int) (d < 0 ? d - 0.5 : d + 0.5);
  }
public:
  using VarRecord<double>::VarRecord;
  using VarRecord<double>::set;

  void set (size_t i, int val) { set (i, (double) val); }
  void set (int val) { set ((double) val); }
  int get (size_t i) const { if (i < size()) return as_int (data[i]); else return -32767;}
  double getd (size_t i) const { if (i < size()) return data[i]; else return -32767.0;}
  int get() const { return as_int (data[0]); }
  double getd() const { return data[0]; }
  double stored (size_t i) const { return getd (i); }
};

struct QuestionFile;
class QuestionInterface;

/* Index of QuestionState::props by lower-cased object name (built lazily).
 * A *copy* deliberately starts empty and invalid: QuestionState is snapshotted
 * every turn for the undo stack, and deep-copying this map there is the bulk
 * of the index's cost.  A restored snapshot rebuilds the index on first use
 * (undos are rare).  A *move* keeps the built index. */
struct PropsIndex
{
  /* Probed only by find/[]/clear (never iterated in order), so unordered. */
  std::unordered_map<std::string, std::vector<size_t> > map;
  bool valid = false;
  std::string key_scratch;   /* reused buffer for lowercased lookup keys */

  /* The records held under `name` (folded to a key the same way the index was
     built -- see fold_lower_into), or nullptr if it has none.  Non-const
     because it writes the key into key_scratch; both indices are `mutable`
     members of QuestionState, so the const lookups can still call it. */
  const std::vector<size_t> *find (const std::string &name);

  PropsIndex () = default;
  PropsIndex (const PropsIndex &) {}
  PropsIndex &operator= (const PropsIndex &) { map.clear (); valid = false; return *this; }
  PropsIndex (PropsIndex &&) = default;
  PropsIndex &operator= (PropsIndex &&) = default;
};

/* A turn snapshot for the undo ring.  QuestionState::props is an append-only log
 * (records are only ever pushed and then shadowed by later ones with the same
 * name -- never modified or removed during play), so we do not copy the whole
 * log every turn: we record only its length and truncate back to it on undo.
 * That keeps per-turn undo cost flat no matter how long the game has run,
 * instead of growing with the property history.  Everything else here is small
 * and bounded (the object list grows only by `clone`; vars/exits/items grow
 * only slowly), so those are copied normally.  props_index is derived and rebuilt
 * lazily, so it is not stored.
 *
 * A RESTORE replaces the log wholesale, so a snapshot taken before it is no
 * longer a prefix of the live log.  The restore hands each such snapshot the
 * abandoned log in props_base (one shared copy for all of them), and undoing
 * to it rebuilds the log from there instead of truncating the live one. */
struct UndoState
{
  bool running = false;
  std::string location;
  size_t props_len = 0;
  std::shared_ptr<const std::vector<PropertyRecord>> props_base;
  std::vector<ObjectRecord> objs;
  std::vector<ExitRecord> exits;
  std::vector<TimerRecord> timers;
  std::vector<SVarRecord> svars;
  std::vector<IVarRecord> ivars;
  std::vector<std::string> items;
};

struct QuestionState
{
  //private:
  //std::auto_ptr<QuestionFile> gf;

public:
  bool running = false;
  std::string location;
  std::vector<PropertyRecord> props;
  /* Index of `props` by lower-cased object name so the runtime get_obj_property
   * / get_obj_action scans visit only one object's records.  Derived data, not
   * serialized; mutable so the const lookups can rebuild it lazily.  Kept in
   * sync with `props` by add_prop (). */
  mutable PropsIndex props_index;
  std::vector<ObjectRecord> objs;
  /* Index of `objs` by lower-cased object name, for the container/parent chain
   * walks (obj_parent, room_of, container_in_scope) that regen runs per object
   * per turn.  Records change fields but are never removed during play, and the
   * only thing that appends is `clone` (add_object, which invalidates the
   * index), so the indices stay valid; a copy or undo-restore starts it invalid
   * (see PropsIndex) and it is rebuilt on first use.  Duplicate names keep
   * definition order, matching the old first-match linear scans. */
  mutable PropsIndex objs_index;
  std::vector<ExitRecord> exits;
  std::vector<TimerRecord> timers;
  std::vector<SVarRecord> svars;
  std::vector<IVarRecord> ivars;
  /* Quest 2.x "items": inventory entries managed by give/lose/got that are
   * separate from world objects (a game may both give an item and hide the
   * like-named room object).  Held by display name. */
  std::vector<std::string> items;
  //std::map <std::string, std::string> obj_types;

  //void register_block (std::string blockname, std::string blocktype);

  QuestionState () {}
  //QuestionState (QuestionRunner &, const QuestionFile &);
  QuestionState (QuestionInterface &, const QuestionFile &);

  /* Append a runtime property/action record (the only way props grows during
   * play), keeping props_index in sync if it is currently built. */
  void add_prop (const std::string &name, const std::string &data);
  /* Append a world object during play (Quest's `clone`), invalidating
   * objs_index so the next lookup sees it. */
  void add_object (const ObjectRecord &o);
  /* (Re)build props_index if it is not valid (e.g. after a copy/load). */
  void ensure_props_index () const;
  /* The props records for `name` (newest last), or nullptr if it has none. */
  const std::vector<size_t> *prop_records (const std::string &name) const;

  /* (Re)build objs_index if it is not valid. */
  void ensure_objs_index () const;
  /* The objs records named `name`, or nullptr if none: objects and characters
     first (in definition order), then any like-named room -- see
     ensure_objs_index and ObjectRecord::is_room. */
  const std::vector<size_t> *obj_records (const std::string &name) const;

  /* Capture this state into an undo snapshot (records props by length only),
   * and restore from one (truncating props back to that length).  See
   * UndoState. */
  UndoState save_undo () const;
  void restore_undo (const UndoState &u);
};

/* Serialize/parse a whole undo history (snapshots oldest first) for the
 * Spatterlight autosave, so UNDO still works after an autorestore.  The
 * snapshots' props_len values are measured against the same append-only
 * props log the accompanying full state carries, so they stay valid across
 * the save/restore round trip. */
extern std::string serialize_undo_history (const std::vector<UndoState> &states);
extern bool deserialize_undo_history (const std::string &data,
                                      std::vector<UndoState> &states);

extern void save_game_to (const std::string &gamename, const std::string &savename, const QuestionState &gs);
/* Serialize/parse the full game state to/from a self-contained string, leaving
 * the actual file I/O to the host (Glk). */
extern std::string serialize_game (const std::string &gamename, const QuestionState &gs);
extern bool deserialize_game (const std::string &filedata, std::string &gamename, QuestionState &gs);

extern std::ostream &operator<< (std::ostream &o, const std::map <std::string, std::string> &m);
extern std::ostream &operator<< (std::ostream &o, const PropertyRecord &pr);
extern std::ostream &operator<< (std::ostream &o, const ObjectRecord &objr);
extern std::ostream &operator<< (std::ostream &o, const ExitRecord &er);
extern std::ostream &operator<< (std::ostream &o, const TimerRecord &tr);
extern std::ostream &operator<< (std::ostream &o, const SVarRecord &sr);
extern std::ostream &operator<< (std::ostream &o, const IVarRecord &ir);
extern std::ostream &operator<< (std::ostream &o, const QuestionState &gs);


#endif
