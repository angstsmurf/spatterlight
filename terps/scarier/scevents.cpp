/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/*
 * Module notes:
 *
 * o Event pause and resume tasks need more testing.
 */

#include <assert.h>
#include <stdlib.h>

#include <vector>

#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"


/* Trace flag, set before running. */
static scr_bool evt_trace = FALSE;


/*
 * Cached per-event definition properties.
 *
 * evt_tick_events() visits every event every turn, and before caching each
 * visit re-read the same immutable event definition fields (starter, pauser,
 * resumer, object moves, restart times, room list) from the bundle -- after
 * the round of task/parser/serializer caches these reads were the largest
 * remaining prop_get() consumer.  Remember each field the first time it is
 * read.  As elsewhere (sctasks.cpp task cache), caching is per field and
 * lazy: the first touch goes through the same fatal-checking prop_get_*
 * wrapper the uncached code used, so a malformed game fails identically, and
 * a field never read before is never read now.  The cache tracks a single
 * game; gs_destroy() calls evt_forget_game().
 */
enum
{
  EVT_STARTER_TYPE, EVT_TASK_NUM, EVT_PAUSE_TASK, EVT_PAUSER_COMPLETED,
  EVT_RESUME_TASK, EVT_RESUMER_COMPLETED, EVT_OBJ1, EVT_OBJ1_DEST,
  EVT_TIME1, EVT_TIME2, EVT_OBJ2, EVT_OBJ2_DEST, EVT_OBJ3, EVT_OBJ3_DEST,
  EVT_TASK_AFFECTED, EVT_TASK_FINISHED, EVT_RESTART_TYPE, EVT_START_TIME,
  EVT_END_TIME, EVT_PREF_TIME1, EVT_PREF_TIME2, EVT_WHERE_TYPE,
  EVT_WHERE_ROOM, EVT_FIELD_COUNT
};

enum
{ EVT_CACHE_UNKNOWN = 0, EVT_CACHE_FALSE = 1, EVT_CACHE_TRUE = 2 };

typedef struct
{
  scr_uint known;                     /* bitmask over the field enum */
  scr_int value[EVT_FIELD_COUNT];
  std::vector<scr_byte> where_rooms;  /* per-room tri-state, sized lazily */
} scr_event_props_t;

static const void *evt_cache_game = NULL;
static std::vector<scr_event_props_t> evt_cache;
static scr_int evt_cache_version = 0;  /* bundle "Version", 0 = unknown */

/*
 * evt_cache_entry()
 *
 * Return the cache entry for an event, resetting the cache if it was built
 * for a different game.
 */
static scr_event_props_t *
evt_cache_entry (scr_gameref_t game, scr_int event)
{
  if (evt_cache_game != game)
    {
      scr_event_props_t initial;

      initial.known = 0;
      evt_cache.assign (gs_event_count (game), initial);
      evt_cache_version = 0;
      evt_cache_game = game;
    }
  return &evt_cache[event];
}

/*
 * evt_forget_game()
 *
 * Drop any event property cache built for the given game.  Called from
 * gs_destroy() so a stale cache can never outlive its game.
 */
void
evt_forget_game (const void *game)
{
  if (evt_cache_game == game)
    {
      evt_cache_game = NULL;
      evt_cache.clear ();
      evt_cache_version = 0;
    }
}

/*
 * evt_cached_integer()
 * evt_cached_boolean()
 * evt_cached_where_integer()
 * evt_cached_where_room_boolean()
 *
 * Lazily cached reads of immutable "Events" bundle fields: a named integer
 * or boolean directly under the event, a named integer under the event's
 * "Where" room list, and one room's membership boolean in that list.
 */
static scr_int
evt_cached_integer (scr_gameref_t game, scr_int event, scr_int field,
                    const scr_char *name)
{
  scr_event_props_t *cached = evt_cache_entry (game, event);

  if (!(cached->known & ((scr_uint) 1 << field)))
    {
      const scr_prop_setref_t bundle = gs_get_bundle (game);

      cached->value[field] = prop_get_indexed_integer (bundle, "Events", event,
                                                       name);
      cached->known |= (scr_uint) 1 << field;
    }
  return cached->value[field];
}

static scr_bool
evt_cached_boolean (scr_gameref_t game, scr_int event, scr_int field,
                    const scr_char *name)
{
  scr_event_props_t *cached = evt_cache_entry (game, event);

  if (!(cached->known & ((scr_uint) 1 << field)))
    {
      const scr_prop_setref_t bundle = gs_get_bundle (game);

      cached->value[field] = prop_get_indexed_boolean (bundle, "Events", event,
                                                       name);
      cached->known |= (scr_uint) 1 << field;
    }
  return (scr_bool) cached->value[field];
}

static scr_int
evt_cached_where_integer (scr_gameref_t game, scr_int event, scr_int field,
                          const scr_char *name)
{
  scr_event_props_t *cached = evt_cache_entry (game, event);

  if (!(cached->known & ((scr_uint) 1 << field)))
    {
      const scr_prop_setref_t bundle = gs_get_bundle (game);
      scr_vartype_t vt_key[4];

      vt_key[0].string = "Events";
      vt_key[1].integer = event;
      vt_key[2].string = "Where";
      vt_key[3].string = name;
      cached->value[field] = prop_get_integer (bundle, "I<-siss", vt_key);
      cached->known |= (scr_uint) 1 << field;
    }
  return cached->value[field];
}

static scr_bool
evt_cached_where_room_boolean (scr_gameref_t game, scr_int event, scr_int room)
{
  scr_event_props_t *cached = evt_cache_entry (game, event);
  scr_vartype_t vt_key[5];
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_bool result;

  vt_key[0].string = "Events";
  vt_key[1].integer = event;
  vt_key[2].string = "Where";
  vt_key[3].string = "Rooms";
  vt_key[4].integer = room;

  /* An out-of-range room can't be cached; take the uncached path so it
   * behaves exactly as before. */
  if (room < 0 || room >= gs_room_count (game))
    return prop_get_boolean (bundle, "B<-sissi", vt_key);

  if (cached->where_rooms.empty ())
    cached->where_rooms.assign (gs_room_count (game), EVT_CACHE_UNKNOWN);

  if (cached->where_rooms[room] == EVT_CACHE_UNKNOWN)
    cached->where_rooms[room] = prop_get_boolean (bundle, "B<-sissi", vt_key)
                                ? EVT_CACHE_TRUE : EVT_CACHE_FALSE;

  return cached->where_rooms[room] == EVT_CACHE_TRUE;
}


/*
 * evt_any_task_in_state()
 *
 * Return TRUE if any task at all matches the given completion state.
 */
static scr_bool
evt_any_task_in_state (scr_gameref_t game, scr_bool state)
{
  scr_int task;

  /* Scan tasks for any whose completion matches input. */
  for (task = 0; task < gs_task_count (game); task++)
    {
      if (gs_task_done (game, task) == state)
        return TRUE;
    }

  /* No tasks matched. */
  return FALSE;
}


/*
 * evt_can_see_event_in_room()
 * evt_can_see_event()
 *
 * Return TRUE if the given room is one the event's text shows in.
 *
 * The room matters, and it is not always the player's.  run400's room lister
 * (`viewroom`, Proc_19_63_472CA4) takes the room to describe as an argument
 * and tests the event's list against *that*: the loop at loc_472B34 reads the
 * event's state byte (var_198(74) = 1, running) and then indexes the event's
 * room array with `arg_C - 1` -- the argument, not the player -- before it
 * appends the LookText at loc_472B7F.  A task with ShowRoomDesc set names the
 * room it displays and prints it *before* its own actions run (see
 * task_show_room_desc() and the "ShowRoomDesc prints BEFORE the actions"
 * note), so at that moment the player is still standing wherever the task
 * found them.  Gating on gs_playerroom() there spliced the room the player
 * was *leaving* into the description of the room they were being shown.
 *
 * Measured 2026-08-25 on goldilocks.taf, run400, Adrift_1_goldilocks.txt,
 * turn 243: the escape from the flooding cellar shows the hall, and the
 * Runner prints no porridge line with it, because event 4 [Cellar fills with
 * porridge] lists rooms 11-13 (cellar, dark passage, dungeon) and the hall is
 * room 1.  Scarier printed it.
 *
 * The tick paths keep the player's room, which is what they are asking about.
 */
scr_bool
evt_can_see_event_in_room (scr_gameref_t game, scr_int event, scr_int room)
{
  scr_int type;

  /* Check room list for the event and return it. */
  type = evt_cached_where_integer (game, event, EVT_WHERE_TYPE, "Type");
  switch (type)
    {
    case ROOMLIST_NO_ROOMS:
      return FALSE;
    case ROOMLIST_ALL_ROOMS:
      return TRUE;

    case ROOMLIST_ONE_ROOM:
      return evt_cached_where_integer (game, event, EVT_WHERE_ROOM, "Room")
             == room;

    case ROOMLIST_SOME_ROOMS:
      return evt_cached_where_room_boolean (game, event, room);

    default:
      scr_fatal ("evt_can_see_event: invalid type, %ld\n", type);
      return FALSE;
    }
}

scr_bool
evt_can_see_event (scr_gameref_t game, scr_int event)
{
  return evt_can_see_event_in_room (game, event, gs_playerroom (game));
}


/*
 * evt_move_object()
 *
 * Move an object from within an event.
 */
static void
evt_move_object (scr_gameref_t game, scr_int object, scr_int destination)
{
  /* Ignore negative values of object. */
  if (object >= 0)
    {
      if (evt_trace)
        {
          scr_trace ("Event: moving object %ld to room %ld\n",
                    object, destination);
        }

      /*
       * Move object depending on destination.  The Runner's event mover
       * never touches the carried-load totals -- an event-placed object
       * weighs nothing towards the player's limits, and one spirited out
       * of the player's hands stays counted (measured live in run400,
       * RUNNER_TESTS_TODO.md section 9; its totals are only ever written
       * by the take/drop handlers and the task mover) -- so the position
       * tracker is suspended for the move.
       */
      gs_set_carried_suspend (game, TRUE);
      switch (destination)
        {
        case -1:               /* Hidden. */
          gs_object_make_hidden (game, object);
          break;

        case 0:                /* Held by player. */
          gs_object_player_get (game, object);
          break;

        case 1:                /* Same room as player. */
          gs_object_to_room (game, object, gs_playerroom (game));
          break;

        default:
          if (destination < gs_room_count (game) + 2)
            gs_object_to_room (game, object, destination - 2);
          else
            {
              scr_int roomgroup, room;

              roomgroup = destination - gs_room_count (game) - 2;
              room = lib_random_roomgroup_member (game, roomgroup);
              if (room >= 0)     /* Empty group: leave the object in place. */
                gs_object_to_room (game, object, room);
            }
          break;
        }
      gs_set_carried_suspend (game, FALSE);

      /*
       * If static, mark as no longer unmoved.
       *
       * This is the only place a static object moves.  The task action mover
       * refuses them outright (see task_move_object), so an event is the sole
       * route by which one can reach the player's hands -- measured live in
       * run400, RUNNER_TESTS_TODO.md section 9.  A static that gets there is
       * listed by "inventory", but the Runner does not otherwise count it as
       * held: it weighs nothing towards the player's limits and cannot be
       * dropped, which is what obj_get_size/obj_get_weight's zero preserves.
       */
      if (obj_is_static (game, object))
        gs_set_object_static_unmoved (game, object, FALSE);

      /*
       * An object an event drops into the room the player is standing in is
       * seen at once, with no lister involved.  run400's event mover ends on
       * exactly that test -- @00456124 compares the object's freshly written
       * location field against the player-room global and stamps the seen
       * byte -- and it is the only reveal on the path, since an event that
       * places an object mid-turn prints no room description.
       *
       * The comparison is against the object's own location, not its
       * container's: an event that posts something into a closed box in the
       * player's room leaves it unseen until the box is opened.
       *
       * The stamp is 4.0's alone.  run390's mover (448B94-448CE3, inside
       * checkevent 448EB8) writes the location (22), the static room-presence
       * array (24) and the parent (42), and simply falls off the end -- no
       * player-room compare, no write to the 3.9 seen byte (44) on any
       * branch.  Measured live on cleft.taf (3.90, Adrift_3_cleft.txt): the
       * klaxon event ends with the player standing in the Loading bay it
       * delivers the packing case to, and the very next commands get
       * "You can't open that." / "Take what?" -- the case stays
       * unreferenceable until something lists it.
       */
      if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_400
          && gs_object_position (game, object) == gs_playerroom (game) + 1)
        gs_set_object_seen (game, object, TRUE);
    }
}


/*
 * evt_taf_version()
 *
 * Return the game's TAF version.  It is immutable, so it is read once per
 * game and cached (evt_cache_entry synchronizes the cache, including the
 * version slot, to this game).
 */
static scr_int
evt_taf_version (scr_gameref_t game, scr_int event)
{
  scr_int version;

  evt_cache_entry (game, event);
  version = evt_cache_version;
  if (version == 0)
    {
      version = prop_get_taf_version (gs_get_bundle (game));
      evt_cache_version = version;
    }
  return version;
}


static void evt_start_event (scr_gameref_t game, scr_int event,
                             scr_bool silent);

/*
 * evt_fixup_v390_v380_immediate_restart()
 *
 * Versions 3.9 and 3.8 differ from version 4.0 on immediate restart: run390
 * re-arms the event WITHOUT printing its StartText, where run400 prints it
 * every time round.  Both give the restarted event its full authored length.
 *
 * Arbitrated live 2026-08-04 (RUNNER_TESTS_TODO.md section 8), probe
 * test/adrift4/harness/make_39_evtimeprobe.py against run390.exe and the EV9 twin against
 * run400.exe:
 *
 *   run390, Time1=Time2=5, RestartType=1, StarterType 1: "E FINISH." on turns
 *     5, 10, 15 -- period 5, and no StartText on either restart.
 *   run390, Time1=Time2=1, StarterType 1, 2 and 3 (variants b, c, e): the
 *     FinishText every turn, the StartText only on the very first start.
 *   run390, variant f -- the exact "Priest Coughs" shape, StartText plus
 *     LookText and no FinishText: one StartText at the trigger, then silence,
 *     and the LookText only in an explicit `look`.
 *   run400, the same event in a 4.0 taf: "E FINISH.  E START." every 5 turns.
 *
 * Only the immediate restart is silent.  Variant d (RestartType=2, restart
 * after a delay) prints its StartText on every re-arm in run390 too, because
 * that path goes back through ES_WAITING and the normal start.
 *
 * `silent` suppresses the StartText alone: Obj1 still moves and the start
 * resource still plays.  Neither of those halves has been probed on a 3.9
 * restart -- what is measured is the text.
 */
static scr_bool
evt_fixup_v390_v380_immediate_restart (scr_gameref_t game, scr_int event)
{
  const scr_int version = evt_taf_version (game, event);

  if (version < TAF_VERSION_400)
    {
      if (evt_trace)
        scr_trace ("Event: applying 3.9/3.8 restart fixup\n");

      /*
       * Re-arm silently.  The length roll belongs to evt_start_event();
       * rolling a second one here would churn the RNG stream on every restart,
       * and taking a turn off the clock -- which SCARE did, and which the
       * 2026-08-04 pass kept -- makes the period one short of what run390
       * measures.
       */
      evt_start_event (game, event, TRUE);
    }

  /* Return TRUE if we applied the fixup. */
  return version < TAF_VERSION_400;
}


/*
 * evt_start_event()
 *
 * Change an event from WAITING to RUNNING.  With `silent`, everything happens
 * except the StartText: that is the game-load start, where the real Runners
 * print into a screen the intro then clears (see evt_start_load_events()).
 */
static void
evt_start_event (scr_gameref_t game, scr_int event, scr_bool silent)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];
  scr_int time1, time2, obj1, obj1dest;

  if (evt_trace)
    scr_trace ("Event: starting event %ld\n", event);

  /* If event is visible, print its start text. */
  if (evt_can_see_event (game, event))
    {
      const scr_char *starttext;

      /* Get and print start text. */
      vt_key[0].string = "Events";
      vt_key[1].integer = event;
      vt_key[2].string = "StartText";
      starttext = prop_get_string (bundle, "S<-sis", vt_key);
      if (!scr_strempty (starttext) && !silent)
        {
          pf_buffer_paragraph_line (filter, starttext);
        }

      /* Handle any associated resource. */
      vt_key[2].string = "Res";
      vt_key[3].integer = 0;
      res_handle_resource (game, "sisi", vt_key);
    }

  /* Move event object to destination. */
  obj1 = evt_cached_integer (game, event, EVT_OBJ1, "Obj1") - 1;
  obj1dest = evt_cached_integer (game, event, EVT_OBJ1_DEST, "Obj1Dest") - 1;
  evt_move_object (game, obj1, obj1dest);

  /* Set the event's state and time. */
  gs_set_event_state (game, event, ES_RUNNING);

  time1 = evt_cached_integer (game, event, EVT_TIME1, "Time1");
  time2 = evt_cached_integer (game, event, EVT_TIME2, "Time2");

  /*
   * An immediate-start event rolled at load by run_runner_load_draws()
   * (Runner-compatible RNG mode) carries its length in the stash; use it up
   * rather than rolling a second time.
   */
  if (gs_event_loadtime (game, event) >= 0)
    {
      gs_set_event_time (game, event, gs_event_loadtime (game, event));
      gs_set_event_loadtime (game, event, -1);
    }
  else
    gs_set_event_time (game, event, scr_randomint_exclusive (time1, time2));

  if (evt_trace)
    scr_trace ("Event: start event handling done, %ld\n", event);
}


/*
 * evt_get_starter_type()
 *
 * Return the starter type for an event.
 */
static scr_int
evt_get_starter_type (scr_gameref_t game, scr_int event)
{
  return evt_cached_integer (game, event, EVT_STARTER_TYPE, "StarterType");
}


/*
 * evt_is_zero_length()
 *
 * TRUE for an event authored with no duration at all, Time1 == Time2 == 0.
 * Such an event behaves quite unlike a one-turn event in the real Runners --
 * see the "parks" comment in evt_tick_event() -- so it is worth a name.
 *
 * The test is on the AUTHORED length, not on the rolled one: a length rolled
 * from a range that happens to include zero was never probed, and the 3.9/3.8
 * immediate-restart fixup deliberately runs an event with one turn already
 * spent, which would otherwise be mistaken for a parked event.
 */
static scr_bool
evt_is_zero_length (scr_gameref_t game, scr_int event)
{
  return evt_cached_integer (game, event, EVT_TIME1, "Time1") == 0
         && evt_cached_integer (game, event, EVT_TIME2, "Time2") == 0;
}


static void evt_tick_event_and_settle (scr_gameref_t game, scr_int event);
static scr_bool evt_has_starter_task (scr_gameref_t game, scr_int event);

/*
 * evt_finish_event()
 *
 * Move an event to FINISHED, or restart it.
 */
static void
evt_finish_event (scr_gameref_t game, scr_int event)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];
  scr_int obj2, obj2dest, obj3, obj3dest;
  scr_int task, startertype, restarttype;
  scr_bool taskfinished;

  if (evt_trace)
    scr_trace ("Event: finishing event %ld\n", event);

  /* Set up invariant parts of the key. */
  vt_key[0].string = "Events";
  vt_key[1].integer = event;

  /* If event is visible, print its finish text. */
  if (evt_can_see_event (game, event))
    {
      const scr_char *finishtext;

      /* Get and print finish text. */
      vt_key[2].string = "FinishText";
      finishtext = prop_get_string (bundle, "S<-sis", vt_key);
      if (!scr_strempty (finishtext))
        {
          pf_buffer_paragraph_line (filter, finishtext);
        }

      /* Handle any associated resource. */
      vt_key[2].string = "Res";
      vt_key[3].integer = 4;
      res_handle_resource (game, "sisi", vt_key);
    }

  /* Move event objects to destination. */
  obj2 = evt_cached_integer (game, event, EVT_OBJ2, "Obj2") - 1;
  obj2dest = evt_cached_integer (game, event, EVT_OBJ2_DEST, "Obj2Dest") - 1;
  evt_move_object (game, obj2, obj2dest);

  obj3 = evt_cached_integer (game, event, EVT_OBJ3, "Obj3") - 1;
  obj3dest = evt_cached_integer (game, event, EVT_OBJ3_DEST, "Obj3Dest") - 1;
  evt_move_object (game, obj3, obj3dest);

  /* See if there is an affected task. */
  task = evt_cached_integer (game, event, EVT_TASK_AFFECTED, "TaskAffected")
         - 1;
  if (task >= 0 && task < gs_task_count (game))
    {
      taskfinished = evt_cached_boolean (game, event, EVT_TASK_FINISHED,
                                         "TaskFinished");
      if (taskfinished)
        {
          /*
           * The event marks the affected task incomplete.  The reference
           * Runner does this by clearing the task's completion flag directly
           * (verified in the ADRIFT 3.9 Runner's checkevent: the "task
           * finished" branch stores 0 into the affected task's completed
           * field).  It is not a task "reverse": no reverse message is shown,
           * and neither the task's Reversible flag nor its "where the task can
           * be run" room list is consulted.  Gating it like a reverse wrongly
           * left non-reversible tasks completed -- e.g. in "Lair of the
           * CyberCow" the "De-Uncle 2 Freedom" event could not clear "put
           * fairy in robot", so the cellar exit that task seals never
           * reopened and the player stayed trapped after saying "uncle".
           */
          gs_set_task_done (game, task, FALSE);
          if (evt_trace)
            scr_trace ("Event: event cleared task %ld\n", task);
        }
      else
        {
          /*
           * run380 43A728 (run400 alike): when the affected task is to be
           * RUN and is not yet complete, the event's own starter-task
           * snapshot is zeroed first.  It only matters when the affected
           * task is also this event's starter task, and only until the end
           * of the pass overwrites it -- i.e. for the lower-index recheck
           * below and for a same-pass restart -- but it is what the Runner
           * does.
           */
          if (!gs_task_done (game, task))
            gs_set_event_taskstate (game, event, FALSE);
        }

      if (taskfinished)
        {
          /* Nothing more: the flag is cleared, the task is not run. */
        }
      else if (evt_taf_version (game, event) < TAF_VERSION_400)
        {
          /*
           * The 3.9 Runner dispatches the task by its command text through
           * the task matcher rather than running it by index: a runnable
           * `*` wildcard task earlier in the list steals the execution, and
           * a restricted match is passed over silently, its FailMessage
           * unprinted.  The dispatch is not gated on the affected task's
           * own runnability -- a wildcard can fire even when the affected
           * task could not run here.  See run_event_task() and
           * RUNNER_TESTS_TODO.md section 2; "thetest" depends on the
           * stealing.
           */
          if (evt_trace)
            scr_trace ("Event: event dispatching task %ld forwards\n", task);

          run_event_task (game, task);
        }
      else if (task_can_run_task_directional (game, task, TRUE))
        {
          /*
           * The 4.0 Runner runs the affected task directly: no wildcard
           * interception, and failing restrictions print their FailMessage
           * (which task_run_task does) -- Shadowpeak's ambient bell/rat
           * lines are exactly such prints.  Both halves verified live
           * against run390/run400 with the same gen400-converted probe;
           * see RUNNER_TESTS_TODO.md section 2.
           */
          if (evt_trace)
            scr_trace ("Event: event running task %ld forwards\n", task);

          run_task_run_by_index (game, task);
        }
      else if (gs_task_done (game, task) && task_where_allows_run (game, task)
               && game->is_running)
        {
          /*
           * A completed task cannot run again, but run400's by-index runner
           * (Proc_19_21_45FB78) walks the restrictions (455C60) BEFORE it
           * looks at the task's done and repeatable bytes (45FA38), so a
           * failing restriction still prints its FailMessage.  Called from an
           * event (arg 2 = 1) a passing one prints nothing, not the
           * RepeatText.  "Riding Home" pins it
           * (runner_transcripts/riding_home.txt): event 8 keeps running the
           * completed "Samantha calls" task 104, and the Runner prints its
           * "Erica and Krystal continue their conversation" FailMessage on
           * a later `wait`.
           */
          const scr_char *fail_message;
          scr_bool restrictions_passed;

          if (evt_trace)
            scr_trace ("Event: event checking completed task %ld\n", task);

          if (restr_eval_task_restrictions (game, task, &restrictions_passed,
                                            &fail_message)
              && !restrictions_passed && fail_message)
            pf_buffer_paragraph_line (filter, fail_message);
        }
      else
        {
          if (evt_trace)
            scr_trace ("Event: event can't run task %ld forwards\n", task);
        }

      /*
       * Both Runners' checkevent (run390 448EB8 at 448D99, run400 470754 at
       * 47059C) follow the affected task with a loop over every event of a
       * LOWER index whose starter TaskNum is that task, calling checkevent on
       * each one again -- while the game is still running -- so an event
       * that this finish starts is not left to the next tick merely because
       * the ordered pass had already been past it.  Vardock Bates pins it:
       * "Movimiento Barcelona-Museo" (event 2) finishes on the first
       * `esperar` outside the airport, runs the arrival task, and "Mordedura
       * Taxista" (event 1, started by that task) prints its StartText in the
       * SAME turn (Adrift_1_vardock_bates.txt, twice).  Without this loop
       * Scarier printed it a turn later.  Events of a higher index need no
       * help: the pass reaches them after the task has completed.
       */
      if (!taskfinished)
        {
          scr_int other;

          for (other = 0; other < event; other++)
            {
              if (evt_has_starter_task (game, other)
                  && evt_cached_integer (game, other, EVT_TASK_NUM, "TaskNum")
                         == task + 1
                  && run_is_running (game))
                {
                  if (evt_trace)
                    scr_trace ("Event: re-checking event %ld started by"
                               " task %ld\n", other, task);

                  evt_tick_event_and_settle (game, other);
                }
            }
        }
    }

  /* Handle possible restart. */
  restarttype = evt_cached_integer (game, event, EVT_RESTART_TYPE,
                                    "RestartType");

  /*
   * A ZERO-length event that restarts "after a delay" does not come back at
   * all when its start is immediate or a task -- probed live 2026-08-02 in
   * run400 (probe EV5 events H2 and H3, make_arena_probe.py) and in run390
   * (make_39_fwprobe.py variant b): the affected task fires once and then
   * nothing, no matter how often the starter task is re-run afterwards, and
   * no LookText appears in the room description in between, so the event is
   * not sitting in a running state either.  Re-arming it here instead made
   * the affected task fire EVERY turn, which is how TheADRIFTProject's
   * "#Pill Check" ran a turn early.
   *
   * Restart-immediately is deliberately NOT gated: run400 really does start
   * such an event again -- EV5's H1 printed its StartText a second time and
   * showed its LookText in every later room description -- it just never
   * finishes again, which evt_tick_event() handles by parking it.
   */
  if (restarttype == 2
      && (evt_get_starter_type (game, event) == 1
          || evt_get_starter_type (game, event) == 3)
      && evt_is_zero_length (game, event))
    {
      if (evt_trace)
        scr_trace ("Event: zero-length event %ld will not restart\n", event);

      gs_set_event_state (game, event, ES_FINISHED);
      gs_set_event_time (game, event, 0);
      restarttype = -1;
    }

  switch (restarttype)
    {
    case -1:                   /* Zero-length one-shot, handled above. */
      break;

    case 0:                    /* Don't restart. */
      startertype = evt_get_starter_type (game, event);
      switch (startertype)
        {
        case 1:                /* Immediate. */
        case 2:                /* Random delay. */
          gs_set_event_state (game, event, ES_FINISHED);
          gs_set_event_time (game, event, 0);
          break;

        case 3:                /* After task. */
          /*
           * run380 43A868 (run390/run400 alike): `If TaskNum > 0 Then state
           * = 2 Else state = 3`.  Awaiting again, so that the event can run
           * once more if its starter task is undone and redone -- the
           * snapshot in evt_starter_task_may_start() keeps it from simply
           * cycling while the task stays complete.
           */
          if (evt_cached_integer (game, event, EVT_TASK_NUM, "TaskNum") > 0)
            gs_set_event_state (game, event, ES_AWAITING);
          else
            gs_set_event_state (game, event, ES_FINISHED);
          gs_set_event_time (game, event, 0);
          break;

        default:
          scr_fatal ("evt_finish_event:"
                    " unknown value for starter type, %ld\n", startertype);
        }
      break;

    case 1:                    /* Restart immediately. */
      if (evt_fixup_v390_v380_immediate_restart (game, event))
        break;
      else
        evt_start_event (game, event, FALSE);
      break;

    case 2:                    /* Restart after delay. */
      startertype = evt_get_starter_type (game, event);
      switch (startertype)
        {
        case 1:                /* Immediate. */
          if (evt_fixup_v390_v380_immediate_restart (game, event))
            break;
          else
            evt_start_event (game, event, FALSE);
          break;

        case 2:                /* Random delay. */
          {
            scr_int start, end;

            gs_set_event_state (game, event, ES_WAITING);
            start = evt_cached_integer (game, event, EVT_START_TIME,
                                        "StartTime");
            end = evt_cached_integer (game, event, EVT_END_TIME, "EndTime");
            gs_set_event_time (game, event,
                               scr_randomint_exclusive (start, end));
            break;
          }

        case 3:                /* After task. */
          gs_set_event_state (game, event, ES_AWAITING);
          gs_set_event_time (game, event, 0);
          break;

        default:
          scr_fatal ("evt_finish_event: unknown StarterType\n");
        }
      break;

    default:
      scr_fatal ("evt_finish_event: unknown RestartType\n");
    }

  if (evt_trace)
    scr_trace ("Event: finish event handling done, %ld\n", event);
}


/*
 * evt_has_starter_task()
 * evt_starter_task_is_complete()
 * evt_pauser_task_is_complete()
 * evt_resumer_task_is_complete()
 *
 * Return the status of start, pause and resume states of an event.
 */
static scr_bool
evt_has_starter_task (scr_gameref_t game, scr_int event)
{
  scr_int startertype;

  startertype = evt_get_starter_type (game, event);
  return startertype == 3;
}

static scr_bool
evt_starter_task_is_complete (scr_gameref_t game, scr_int event)
{
  scr_int task;
  scr_bool start;

  task = evt_cached_integer (game, event, EVT_TASK_NUM, "TaskNum");

  /*
   * A "start after task" event whose TaskNum is 0 (the Generator's blank
   * task) means "after any task" only in 4.0: run400's checkevent (470754)
   * loops over every task at 46FD93 when TaskNum = 0 and starts the event as
   * soon as one is done.  run370 (431B8D), run380 (439E15) and run390
   * (4483BD) guard the awaiting-state start with `TaskNum > 0` and have no
   * such loop, so there the event never starts at all.  Measured on
   * wrecked.taf (3.80, event 25 "stamp ticket", TaskNum 0): run380x drew four
   * Rnd on the first turn where scarier drew five -- the fifth was this
   * event's length roll -- and its task 74 never ran.
   */
  start = FALSE;
  if (task == 0)
    {
      if (evt_taf_version (game, event) >= TAF_VERSION_400
          && evt_any_task_in_state (game, TRUE))
        start = TRUE;
    }
  else if (task > 0 && task - 1 < gs_task_count (game))
    {
      if (gs_task_done (game, task - 1))
        start = TRUE;
    }

  return start;
}


/*
 * evt_starter_task_may_start()
 * evt_starter_task_reverts()
 * evt_snapshot_starter_tasks()
 *
 * The Runner's starter-task tests are EDGE-triggered, not level-triggered.
 * Every event carries a snapshot of its starter task's completed flag
 * (gs_event_taskstate), rewritten by a second loop at the end of each
 * events() pass -- run380 events() 425094 after its checkevent loop, run390
 * 42C57C, run400's driver at 4492A8 -- and checkevent tests the live flag
 * against it:
 *
 *   awaiting (state 2): start iff completed = 1 And snapshot = 0
 *                        (run380 439E3A, run400 46FDF4);
 *   running  (state 1): back to awaiting iff completed = 0 And snapshot = 1
 *                        (run380 439EBB, run400 46FE80).
 *
 * So a task that is undone and redone BETWEEN two pass ends restarts the
 * event, and one that merely stays complete does not.  wrecked.taf (3.80)
 * pins the first half: "after throw #2" (event 36) is started by "#boris
 * finds player" (task 112), and its finish un-does that very task; Boris's
 * walk re-completes it before the next tick, so run380x restarted the event
 * -- one length roll -- on each of four consecutive ticks of one `wait`,
 * where SCARIER, waiting to SEE the task incomplete at a tick, rolled once.
 * Runner 15 draws that turn, SCARIER 11, and every rain and train after it
 * shifted.
 *
 * TaskNum 0 has no snapshot in any Runner (their second loop is guarded by
 * TaskNum > 0), so the 4.0 "after any task" rule stays level-triggered.
 */
static scr_bool
evt_starter_task_may_start (scr_gameref_t game, scr_int event)
{
  scr_int task;

  if (!evt_starter_task_is_complete (game, event))
    return FALSE;

  task = evt_cached_integer (game, event, EVT_TASK_NUM, "TaskNum");
  if (task > 0 && gs_event_taskstate (game, event))
    return FALSE;

  return TRUE;
}

static scr_bool
evt_starter_task_reverts (scr_gameref_t game, scr_int event)
{
  scr_int task;

  task = evt_cached_integer (game, event, EVT_TASK_NUM, "TaskNum");
  if (task <= 0)
    return FALSE;

  return !evt_starter_task_is_complete (game, event)
         && gs_event_taskstate (game, event);
}

static void
evt_snapshot_starter_tasks (scr_gameref_t game)
{
  scr_int event;

  for (event = 0; event < gs_event_count (game); event++)
    {
      scr_int task;

      if (!evt_has_starter_task (game, event))
        continue;

      task = evt_cached_integer (game, event, EVT_TASK_NUM, "TaskNum");
      if (task > 0 && task - 1 < gs_task_count (game))
        {
          scr_bool done = gs_task_done (game, task - 1);

          if (evt_trace && done != gs_event_taskstate (game, event))
            scr_trace ("Event: event %ld starter task snapshot -> %d\n",
                       event, done);

          gs_set_event_taskstate (game, event, done);
        }
    }
}

static scr_bool
evt_pauser_task_is_complete (scr_gameref_t game, scr_int event)
{
  scr_int pausetask;
  scr_bool completed, pause;

  pausetask = evt_cached_integer (game, event, EVT_PAUSE_TASK, "PauseTask");
  completed = !evt_cached_boolean (game, event, EVT_PAUSER_COMPLETED,
                                   "PauserCompleted");

  pause = FALSE;
  if (pausetask == 1)
    {
      if (evt_any_task_in_state (game, completed))
        pause = TRUE;
    }
  else if (pausetask > 1 && pausetask - 2 < gs_task_count (game))
    {
      if (completed == gs_task_done (game, pausetask - 2))
        pause = TRUE;
    }

  return pause;
}

static scr_bool
evt_resumer_task_is_complete (scr_gameref_t game, scr_int event)
{
  scr_int resumetask;
  scr_bool completed, resume;

  resumetask = evt_cached_integer (game, event, EVT_RESUME_TASK,
                                   "ResumeTask");
  completed = !evt_cached_boolean (game, event, EVT_RESUMER_COMPLETED,
                                   "ResumerCompleted");

  resume = FALSE;
  if (resumetask == 1)
    {
      if (evt_any_task_in_state (game, completed))
        resume = TRUE;
    }
  else if (resumetask > 1 && resumetask - 2 < gs_task_count (game))
    {
      if (completed == gs_task_done (game, resumetask - 2))
        resume = TRUE;
    }

  return resume;
}


/*
 * evt_handle_preftime_notifications()
 *
 * Print messages and handle resources for the event where we're in mid-event
 * and getting close to some number of turns from its end.
 */
static void
evt_handle_preftime_notifications (scr_gameref_t game, scr_int event)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];
  scr_int preftime1, preftime2;
  const scr_char *preftext;

  vt_key[0].string = "Events";
  vt_key[1].integer = event;

  preftime1 = evt_cached_integer (game, event, EVT_PREF_TIME1, "PrefTime1");
  if (preftime1 == gs_event_time (game, event))
    {
      vt_key[2].string = "PrefText1";
      preftext = prop_get_string (bundle, "S<-sis", vt_key);
      if (!scr_strempty (preftext))
        {
          pf_buffer_paragraph_line (filter, preftext);
        }

      vt_key[2].string = "Res";
      vt_key[3].integer = 2;
      res_handle_resource (game, "sisi", vt_key);
    }

  preftime2 = evt_cached_integer (game, event, EVT_PREF_TIME2, "PrefTime2");
  if (preftime2 == gs_event_time (game, event))
    {
      vt_key[2].string = "PrefText2";
      preftext = prop_get_string (bundle, "S<-sis", vt_key);
      if (!scr_strempty (preftext))
        {
          pf_buffer_paragraph_line (filter, preftext);
        }

      vt_key[2].string = "Res";
      vt_key[3].integer = 3;
      res_handle_resource (game, "sisi", vt_key);
    }
}


/*
 * evt_tick_event()
 * evt_tick_event_and_settle()
 *
 * Attempt to advance an event by one turn.  The second form is one event's
 * share of evt_tick_events(): tick, and tick once more if that moved the
 * event from paused or waiting into running.
 */
static void
evt_tick_event (scr_gameref_t game, scr_int event)
{
  if (evt_trace)
    {
      scr_trace ("Event: ticking event %ld: state %ld, time %ld\n", event,
                gs_event_state (game, event), gs_event_time (game, event));
    }

  /* Handle call based on current event state. */
  switch (gs_event_state (game, event))
    {
    case ES_WAITING:
      {
        if (evt_trace)
          scr_trace ("Event: ticking waiting event %ld\n", event);

        /*
         * Because we also tick an event that goes from waiting to running,
         * events started here will tick through RUNNING too, and have their
         * time decremented.  To get around this, so that the timer for one-
         * shot events doesn't look one lower than it should after this
         * transition, we need to set the initial time for events that start
         * as soon as the game starts to one greater than that set by
         * evt_start_time().  Here's the hack to do that; if the event starts
         * immediately, its time will already be zero, even before decrement,
         * which is how we tell which events to apply this hack to.
         *
         * Inelegant, but it yields the Runner's timer values, and the
         * walkthrough corpus validates it.
         */
        if (gs_event_time (game, event) == 0)
          {
            evt_start_event (game, event, FALSE);

            /* If the event time was set to zero, finish immediately. */
            if (gs_event_time (game, event) <= 0)
              evt_finish_event (game, event);
            else
              gs_set_event_time (game, event, gs_event_time (game, event) + 1);
            break;
          }

        /*
         * Decrement the event's time, and if it goes to zero, start running
         * the event.
         */
        gs_decrement_event_time (game, event);

        if (gs_event_time (game, event) <= 0)
          {
            evt_start_event (game, event, FALSE);

            /*
             * If the event time was set to zero, finish immediately -- unless
             * the event has no length at all, in which case it PARKS: started
             * but never finishing.  A zero-length event reached off a running
             * clock behaves quite differently from one reached at game start
             * or off a starter task, both of which finish on the spot.  Probed
             * live in run400 2026-08-02 (probe EV4): the event printed its
             * StartText on the turn the delay expired, then showed its LookText
             * in every later room description and never printed its FinishText
             * or ran its affected task.  Del Sol's "physics distraction 3" is
             * exactly this shape.
             */
            if (gs_event_time (game, event) <= 0
                && !evt_is_zero_length (game, event))
              evt_finish_event (game, event);
          }
      }
      break;

    case ES_RUNNING:
      {
        if (evt_trace)
          scr_trace ("Event: ticking running event %ld\n", event);

        /*
         * Re-check the starter task; if it's no longer completed, we need
         * to set the event back to waiting on task.
         */
        if (evt_has_starter_task (game, event))
          {
            if (evt_starter_task_reverts (game, event))
              {
                if (evt_trace)
                  scr_trace ("Event: starter task not complete\n");

                gs_set_event_state (game, event, ES_AWAITING);
                gs_set_event_time (game, event, 0);
                break;
              }
          }

        /*
         * run400's running block -- everything from the pause test down to
         * the finish test -- runs at most once per turn per event: it is
         * entered on `state = running And ticked = 0' and sets ticked at
         * once (46FF48), and the command processor clears the flag at the
         * top of each typed line (48A2EA) and after every events() pass
         * (48AC40).  The revert test above is outside the block.  An event
         * already ticked this turn by an out-of-order checkevent -- the
         * execute-task action's immediate check, or a finishing event's
         * lower-index recheck -- is therefore skipped by the ordered pass.
         * run380 (43A135) and run390 (448714) enter their running blocks
         * on the state alone, and tick such an event twice.
         *
         * "SS Whore" (4.0, Adrift_304_sswhore.txt) pins it: event 12
         * (Time 1) is started, mid-pass, by an execute-task action inside
         * event 11's finish, and its own immediate check decrements it to
         * the roll; the ordered pass then reaches it and must not finish
         * it a turn early.
         */
        if (evt_taf_version (game, event) >= TAF_VERSION_400
            && !getenv ("SCR_TMP_NOTICKED"))
          {
            if (gs_event_ticked (game, event))
              {
                if (evt_trace)
                  scr_trace ("Event: event %ld already ticked this turn\n",
                             event);
                break;
              }
            gs_set_event_ticked (game, event, TRUE);
          }

        /* If the pauser has completed, but resumer not, pause this event. */
        if (evt_pauser_task_is_complete (game, event)
            && !evt_resumer_task_is_complete (game, event))
          {
            if (evt_trace)
              scr_trace ("Event: pause complete\n");

            gs_set_event_state (game, event, ES_PAUSED);
            break;
          }

        /*
         * A zero-length event that got here is parked: it was started off a
         * clock or by an immediate restart, and in the real Runner it stays
         * running for the rest of the game -- its LookText keeps appearing in
         * the room description, and its end never arrives.  Leave the clock
         * alone, or the decrement below would take it negative and "finish"
         * an event the Runner never finishes.
         *
         * Only a clock of zero parks.  A start that kept run400's +1 (the
         * "started after its tick this turn" case in ES_AWAITING) holds 1,
         * and run400's next running block takes it to 0 and finishes the
         * event like any other.  "Riding Home" pins it
         * (runner_transcripts/riding_home.txt): event 9 (zero-length,
         * starter "You get off the bus") is started and finished by the
         * execute-task check on the bus-stop turn, printing task 118's 90%
         * FailMessage.  The finish zeroed its snapshot (470548, task 118
         * stays incomplete), so the ordered pass starts it again with the
         * +1 kept, and the Runner prints the 90% line a second time after
         * `enter home`.
         */
        if (evt_is_zero_length (game, event)
            && gs_event_time (game, event) <= 0)
          {
            if (evt_trace)
              scr_trace ("Event: zero-length event %ld is parked\n", event);
            break;
          }

        /*
         * Decrement the event's time, and print any notifications for a set
         * number of turns from the event end.
         */
        gs_decrement_event_time (game, event);

        if (evt_can_see_event (game, event))
          evt_handle_preftime_notifications (game, event);

        /* If the time goes to zero, finish running the event. */
        if (gs_event_time (game, event) <= 0)
          evt_finish_event (game, event);
      }
      break;

    case ES_AWAITING:
      {
        if (evt_trace)
          scr_trace ("Event: ticking awaiting event %ld\n", event);

        /*
         * Check the starter task.  If it's completed -- and was not already
         * complete at the end of the last pass, see evt_starter_task_may_start
         * -- start running the event.
         */
        if (evt_starter_task_may_start (game, event))
          {
            scr_bool already_ticked;

            /*
             * run400 sets the clock to the roll PLUS ONE at the start
             * (46FE49) and relies on the running block that follows in the
             * same checkevent call to take the 1 back.  When the start
             * comes from an out-of-order checkevent -- a finishing event's
             * lower-index recheck, or the execute-task action's immediate
             * check -- AFTER the ordered pass has already ticked this event
             * this turn, the running block is closed (46FF48, byte 196 is
             * set) and the +1 survives: the event ends one turn later than
             * its roll.  "Glum Fiddle" pins it (Adrift_1080_Glum_Fiddle.txt,
             * seed 1234): "Move Glum to Swamp" (event 0, Time 4, started by
             * task 36) finishes on the third `wait`, is restarted the same
             * turn by event 5's finish recheck, and the Runner prints "Glum
             * suddenly turns and heads south" again on the FIFTH turn after
             * (`take tray`), not the fourth.  Nothing else of the running
             * block runs either -- no pause test, no notification, no
             * finish -- so the start is all that happens here.
             */
            already_ticked = evt_taf_version (game, event) >= TAF_VERSION_400
                             && gs_event_ticked (game, event);

            evt_start_event (game, event, FALSE);

            if (already_ticked)
              {
                if (evt_trace)
                  scr_trace ("Event: event %ld started after its tick this"
                             " turn, clock %ld + 1\n", event,
                             gs_event_time (game, event));

                gs_set_event_time (game, event,
                                   gs_event_time (game, event) + 1);
                break;
              }

            /*
             * The Runner's start turn falls straight into the running block
             * (see above) and so marks the event ticked for this turn.
             */
            gs_set_event_ticked (game, event, TRUE);

            /*
             * If the pauser has completed, but resumer not, immediately
             * also pause this event.  The Runner tests this before the
             * start turn's decrement and leaves checkevent() there, so a
             * pause on the start turn suppresses the notification and
             * finish checks below.
             */
            if (evt_pauser_task_is_complete (game, event)
                && !evt_resumer_task_is_complete (game, event))
              {
                if (evt_trace)
                  scr_trace ("Event: pause complete, immediate pause\n");

                gs_set_event_state (game, event, ES_PAUSED);
                break;
              }

            /*
             * The start turn is also a tick.  checkevent() is one straight
             * run of state tests, so an event that goes from "awaiting" to
             * "running" here falls into the running block in the very same
             * call: it prints its StartText, then decrements and runs the
             * two pref-time notifications and the end-of-event test.  The
             * Runner compensates by setting the clock to the roll plus one
             * (run370 431BF0, run380 439E78, run390 448428, run400 46FE49
             * all add the 1; only the task-started path does), so the +1
             * and the decrement cancel and the event still ends `roll'
             * turns after it started -- which is why our start-turn-does-
             * not-tick model has always given the right end time.  What it
             * cannot give is a notification whose PrefTime equals the whole
             * rolled length: the Runner compares the post-decrement clock,
             * i.e. the roll itself, on the start turn, and we never
             * compared anything on the start turn at all.
             *
             * Only this transition needs the block: evt_tick_events()
             * re-ticks an event that has just gone from waiting or paused
             * to running, so those paths have always had their start
             * turn's tick, and the ES_WAITING immediate-start hack's +1 is
             * there to compensate for that re-tick.  Do not re-tick here as
             * well -- our clock already holds the roll, which is the value
             * the Runner only reaches after its start-turn decrement.
             *
             * Measured in run400 under Wine, Orient_Express.taf, transcript
             * Adrift_36_orient_express.txt (2026-08-25).  Turn 43 `use
             * phone' starts event 2 [Phone rings] (Time1 = 1, Time2 = 8,
             * PrefTime1 = 2) and the Runner prints its StartText and its
             * PrefText1 on that one turn; the player leaves the event's
             * single room next turn, so we printed the PrefText1 never.
             * Turn 46 `give card to habibo' is the same shape with event 3
             * [Driveby Shooting] (PrefTime1 = 3).
             */
            if (evt_can_see_event (game, event))
              evt_handle_preftime_notifications (game, event);

            /* If the event time was set to zero, finish immediately. */
            if (gs_event_time (game, event) <= 0)
              evt_finish_event (game, event);
          }
      }
      break;

    case ES_FINISHED:
      {
        if (evt_trace)
          scr_trace ("Event: ticking finished event %ld\n", event);

        /*
         * Nothing to do.  The Runners' checkevent has no code at all for
         * their finished state (3): only an event with no starter task, or
         * an "after any task" (TaskNum 0) one, ever lands here, because a
         * task-started event that finishes without restarting goes back to
         * AWAITING instead (run380 43A868: `If TaskNum > 0 Then state = 2
         * Else state = 3`), where the snapshot rule decides whether it runs
         * again -- see evt_starter_task_may_start().  SCARIER used to park
         * every one-shot here and revive it on seeing the starter task
         * incomplete, which missed an undo-and-redo within one turn.
         */
      }
      break;

    case ES_PAUSED:
      {
        if (evt_trace)
          scr_trace ("Event: ticking paused event %ld\n", event);

        /* If the resumer has completed, resume this event. */
        if (evt_resumer_task_is_complete (game, event))
          {
            if (evt_trace)
              scr_trace ("Event: resume complete\n");

            gs_set_event_state (game, event, ES_RUNNING);
            break;
          }
      }
      break;

    default:
      scr_fatal ("evt_tick: invalid event state\n");
    }

  if (evt_trace)
    {
      scr_trace ("Event: after ticking event %ld: state %ld, time %ld\n", event,
                gs_event_state (game, event), gs_event_time (game, event));
    }
}


/*
 * evt_tick_events()
 *
 * Attempt to advance each event by one turn.
 */
void
evt_tick_events (scr_gameref_t game)
{
  scr_int event;

  /*
   * Tick all events.  If an event transitions into a running state from a
   * paused or waiting state, tick that event again.
   */
  for (event = 0; event < gs_event_count (game); event++)
    evt_tick_event_and_settle (game, event);

  /* The Runner's second loop: note every starter task's completion. */
  evt_snapshot_starter_tasks (game);

  /* run400 48AC40: every event may be ticked again next turn. */
  evt_clear_ticked_events (game);
}


/*
 * evt_clear_ticked_events()
 *
 * Clear every event's "ticked this turn" flag; run400 does this at the top
 * of each typed line (48A2EA) and after each events() pass (48AC40).  The
 * flag is only ever set for 4.0 games, so no gate is needed here.
 */
void
evt_clear_ticked_events (scr_gameref_t game)
{
  scr_int event;

  for (event = 0; event < gs_event_count (game); event++)
    gs_set_event_ticked (game, event, FALSE);
}

static void
evt_tick_event_and_settle (scr_gameref_t game, scr_int event)
{
  scr_int prior_state, state;

  /* Note current state, and tick event forwards. */
  prior_state = gs_event_state (game, event);
  evt_tick_event (game, event);

  /*
   * If the event went from paused or waiting to running, tick again.
   * This looks dodgy, and probably is, but it does keep timers correct
   * by only re-ticking events that have transitioned from non-running
   * states to a running one, and not already-running events.  This is
   * in effect just adding a bit of turn processing to a tick that would
   * otherwise change state alone; a bit of laziness, in other words.
   */
  state = gs_event_state (game, event);
  if (state == ES_RUNNING
      && (prior_state == ES_PAUSED || prior_state == ES_WAITING))
    evt_tick_event (game, event);
}


/*
 * evt_check_events_started_by_task()
 *
 * The 4.0 Runner's "execute task" action does more than run the task.  Its
 * execute_action (48E860, type 5 at 48D588) runs the named task through
 * task_dispatch_filter (45FB78) and then, at 48D5DE-48D638, loops over
 * every event: `If events(i).TaskNum - 1 = task And gamestate = 0 Then
 * checkevent(i)`.  So an event started by a task that an ACTION executes
 * advances at once, inside the action, wherever that happens -- in the
 * player's own command, or in the middle of the events pass when a
 * finishing event's TaskAffected executes the task.  The loop runs whether
 * or not the filter let the task run, and TaskNum 0 never matches.
 *
 * It matters because of the snapshot rule (evt_snapshot_starter_tasks): a
 * task completed by a finishing event's actions AFTER the pass has been past
 * the events it starts would otherwise be snapshotted as "already complete"
 * at the end of that pass, and those events would never start at all.
 * "SS Whore" (4.0) pins it: "Messenger Arrives After Sex" (event 9) and
 * "Oberst Drink Waiting" (event 8) are started by task 137, which only ever
 * runs as an execute-task action of task 261, the TaskAffected of event 12
 * (Adrift_304_sswhore.txt: the knock at the double doors after `wait`).
 *
 * The Runner skips the loop when execute_action's flag argument is 0, which
 * happens only for the two library-internal dispatches in its take (47C747)
 * and drop (46FADB) handlers; every other path -- typed commands, events'
 * TaskAffected, nested execute-task actions, inventory -- passes 1.  That
 * corner is not modelled.  run380 and run390 have no such loop (checkevent
 * is called only from their events() pass and the finish recheck), so this
 * is gated on 4.0.
 */
void
evt_check_events_started_by_task (scr_gameref_t game, scr_int task)
{
  scr_int event;

  if (gs_event_count (game) == 0
      || evt_taf_version (game, 0) < TAF_VERSION_400)
    return;

  for (event = 0; event < gs_event_count (game); event++)
    {
      if (!run_is_running (game))
        break;

      if (evt_has_starter_task (game, event)
          && evt_cached_integer (game, event, EVT_TASK_NUM, "TaskNum") - 1
                 == task)
        {
          if (evt_trace)
            scr_trace ("Event: checking event %ld after execute-task"
                       " action ran task %ld\n", event, task);

          evt_tick_event_and_settle (game, event);
        }
    }
}


/*
 * evt_start_load_events()
 * evt_finish_load_events()
 *
 * The two halves of an immediate event's game-load start, called either side
 * of the opening room description (scrunner.cpp).
 *
 * Both Runners start a StarterType=1 event while the game is still loading,
 * BEFORE the first room description is printed -- probed live 2026-08-02 in
 * run400 (probe EV6 in test/adrift4/harness/make_arena_probe.py) and run390
 * (make_39_fwprobe.py variant "e"), a plain length-3 immediate event carrying
 * all three texts.  Two things follow, and both are visible:
 *
 *   - its LookText is part of the OPENING room description, because the event
 *     is already running when that description is composed;
 *   - its StartText is never seen at all, having been printed into the screen
 *     the intro then clears.
 *
 * Everything else about the event is unchanged, including when it ends: the
 * probe's length-3 event finished on the third command turn in both Runners
 * and in SCARIER.  So this is purely a matter of moving the start earlier and
 * dropping its text -- hence the `silent` start here, and the +1 that leaves
 * the following startup tick's decrement landing on the rolled length.
 *
 * The finish half stays BELOW the description: a zero-length immediate event
 * prints its FinishText, runs its TaskAffected and (RestartType=1) restarts
 * with a visible StartText, all after the room text -- run400 probe EV5's
 * turn 0 reads "A bare arena.  H1 LOOK.  H2 LOOK.  H1 FINISH.  H1 TASK.  H1
 * START. ...".  Which is why this is two calls and not one.
 */
void
evt_start_load_events (scr_gameref_t game)
{
  scr_int event;

  for (event = 0; event < gs_event_count (game); event++)
    {
      if (gs_event_state (game, event) != ES_WAITING
          || gs_event_time (game, event) != 0)
        continue;

      evt_start_event (game, event, TRUE);

      /*
       * The same compensation the tick's immediate-start branch applies (see
       * evt_tick_event()): the startup tick that follows decrements a running
       * event once, so a length-N event has to leave the load carrying N+1.
       * A zero-length event keeps its zero and is finished below.
       *
       * Only where that tick exists, which is 3.90 and 4.00: run380 and
       * run370 never call events() before the first command (see the
       * startup block in scrunner.cpp), and their load code hands an
       * immediate event its bare rolled length, so the first command's
       * decrement is the first one it ever sees.
       */
      if (gs_event_time (game, event) > 0
          && evt_taf_version (game, event) >= TAF_VERSION_390)
        gs_set_event_time (game, event, gs_event_time (game, event) + 1);
    }
}

void
evt_finish_load_events (scr_gameref_t game)
{
  scr_int event;

  /*
   * The only events that can be RUNNING with no time left at this point are
   * the zero-length ones evt_start_load_events() just started -- nothing else
   * has ticked yet.  (A restart puts one back into RUNNING at zero, but at an
   * index this loop has already passed, so it parks as the Runner's does
   * rather than finishing twice.)
   */
  for (event = 0; event < gs_event_count (game); event++)
    {
      if (gs_event_state (game, event) == ES_RUNNING
          && gs_event_time (game, event) <= 0)
        evt_finish_event (game, event);
    }
}


/*
 * evt_debug_trace()
 *
 * Set event tracing on/off.
 */
void
evt_debug_trace (scr_bool flag)
{
  evt_trace = flag;
}
