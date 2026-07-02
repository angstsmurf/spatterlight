/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- events + walks turn-based runtime.
 *
 * A faithful port of clsEvent's / clsWalk's turn-based state machines
 * (IncrementTimer / DoAnySubEvents / lStart / lStop), driven each turn by
 * clsUserSession.TurnBasedStuff and on task completion by the EventControls.
 * Split out of a5run.cpp; the shared run struct and the ev_* entry points it
 * exposes to the core turn loop live in a5run_internal.h.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <set>
#include <string>
#include <vector>

#include "a5arith.h"
#include "a5expr.h"
#include "a5parse.h"
#include "a5rand.h"
#include "a5restr.h"
#include "a5run.h"
#include "a5run_internal.h"
#include "a5sb.h"
#include "a5sexpr.h"
#include "a5text.h"

/* A faithful port of clsEvent's turn-based state machine: IncrementTimer /
   DoAnySubEvents / RunSubEvent / lStart / lStop, driven each turn by
   clsUserSession.TurnBasedStuff and on task completion by the EventControls.
   Only TurnBased events are handled (this engine has no real-time clock). */

static void ev_lstart (a5_run_t *run, int ei, int restart, sb_t *out);
static void ev_lstop  (a5_run_t *run, int ei, int run_subs, sb_t *out);
static void ev_do_subevents (a5_run_t *run, int ei, sb_t *out);
static void attempt_event_task (a5_run_t *run, const char *key, int depth, sb_t *out);

/* Walk drivers (defined after the events; called from the same hook points). */
static void wk_on_task_completed (a5_run_t *run, const char *task_key);
static void wk_tick_all (a5_run_t *run, sb_t *out);
static void wk_init     (a5_run_t *run, sb_t *out);

static long ev_from_start (const a5_event_rt &rt) { return rt.length_value - rt.timer_to_end; }
static long ev_from_last  (const a5_event_rt &rt) { return ev_from_start (rt) - rt.last_se_time; }

/* The TimerToEndOfEvent property setter: assigning it can start a counted-down
   event or stop a running one (clsEvent.TimerToEndOfEvent Set). */
static void
ev_set_timer_to_end (a5_run_t *run, int ei, long value, sb_t *out)
{
  a5_event_rt &rt = (*run->events)[ei];
  rt.timer_to_end = value;
  if (rt.status == A5_EV_COUNTDOWN && ev_from_start (rt) == 0)
    ev_lstart (run, ei, 1, out);                  /* Start(True) */
  if (rt.status == A5_EV_RUNNING && rt.timer_to_end == 0)
    ev_lstop (run, ei, 1, out);                   /* lStop(True) */
}

static void
ev_run_subevent (a5_run_t *run, int ei, int sei, sb_t *out)
{
  const a5_event_t *e = &run->adv->events[ei];
  a5_event_rt &rt = (*run->events)[ei];
  const a5_subevent_t *se = &e->subevents[sei];

  switch (se->what)
    {
    case A5_SE_EXECTASK:
      if (se->key != NULL)
        attempt_event_task (run, se->key, 0, out);
      break;
    case A5_SE_UNSETTASK:
      if (se->key != NULL)
        { int ti = a5state_task_index (run->st, se->key);
          if (ti >= 0) run->st->task_done[ti] = 0; }
      break;
    case A5_SE_DISPLAY:
      /* clsEvent.RunSubEvent DisplayMessage: show only if the OnlyApplyAt gate
         (se->key) is set and the player is in that location/group. */
      if (se->description != NULL && se->key != NULL && se->key[0] != '\0'
          && a5state_in_group_or_location (run->st, a5state_player_key (run->st), se->key))
        { char *m = a5text_describe (run->st, se->description);
          if (msg_has_output (m)) { sb_pspace (out); sb_puts (out, m); }
          free (m); }
      break;
    case A5_SE_SETLOOK:
      /* clsEvent SetLook: push (OnlyApplyAt gate, rendered text) onto the look
         stack; a5text_view_location consults it (LookText). */
      if (se->description != NULL)
        { char *m = a5text_describe (run->st, se->description);
          a5state_push_look (run->st, se->key, m);
          free (m); }
      break;
    }
  rt.last_se_time  = ev_from_start (rt);
  rt.last_se_index = sei;
}

static void
ev_do_subevents (a5_run_t *run, int ei, sb_t *out)
{
  const a5_event_t *e = &run->adv->events[ei];
  a5_event_rt &rt = (*run->events)[ei];
  int i;
  if (rt.status != A5_EV_RUNNING)
    return;
  for (i = 0; i < e->n_subevents; i++)
    {
      const a5_subevent_t *se = &e->subevents[i];
      long ft = rt.se_ft[i];
      int go = 0;
      switch (se->when)
        {
        case A5_SE_FROM_START:
          if (ev_from_start (rt) == ft && ft <= rt.length_value
              && (ft > 0 || rt.when_start != 1))
            go = 1;
          break;
        case A5_SE_FROM_LAST:
          if (ev_from_last (rt) == ft
              && ((rt.last_se_index == -1 && i == 0)
                  || (i > 0 && rt.last_se_index == i - 1)))
            go = 1;
          break;
        case A5_SE_BEFORE_END:
          if (rt.timer_to_end == ft)
            go = 1;
          break;
        }
      if (go)
        ev_run_subevent (run, ei, i, out);
    }
}

static void
ev_lstart (a5_run_t *run, int ei, int restart, sb_t *out)
{
  const a5_event_t *e = &run->adv->events[ei];
  a5_event_rt &rt = (*run->events)[ei];
  int i;
  if (!(rt.status == A5_EV_NOTYET || rt.status == A5_EV_COUNTDOWN
        || rt.status == A5_EV_FINISHED
        || (rt.status == A5_EV_RUNNING && restart)))
    return;
  rt.status = A5_EV_RUNNING;
  rt.length_value = a5rand_between (e->length_from, e->length_to);  /* Length.Reset + first .Value */
  rt.last_se_index = -1;
  rt.last_se_time = 0;
  for (i = 0; i < e->n_subevents; i++)
    rt.se_ft[i] = a5rand_between (e->subevents[i].ft_from, e->subevents[i].ft_to);

  ev_set_timer_to_end (run, ei, rt.length_value, out);
  if (ev_from_start (rt) == 0)
    ev_do_subevents (run, ei, out);              /* 'after 0 turns' sub-events */

  if (rt.when_start == 1)                         /* Immediately -> BetweenXY  */
    rt.when_start = 2;
  rt.just_started = 1;
}

static void
ev_lstop (a5_run_t *run, int ei, int run_subs, sb_t *out)
{
  const a5_event_t *e = &run->adv->events[ei];
  a5_event_rt &rt = (*run->events)[ei];
  if (run_subs)
    ev_do_subevents (run, ei, out);
  if (rt.status == A5_EV_PAUSED)
    return;
  rt.status = A5_EV_FINISHED;
  if (e->repeating && rt.timer_to_end == 0)
    {
      if (rt.length_value > 0)
        {
          if (e->repeat_countdown)
            {
              long delay = a5rand_between (e->start_from, e->start_to);
              rt.status = A5_EV_COUNTDOWN;
              ev_set_timer_to_end (run, ei, delay + rt.length_value, out);
            }
          else
            ev_lstart (run, ei, 1, out);
        }
      /* else: zero-length repeating event -- don't restart (infinite loop) */
    }
}

static void
ev_increment (a5_run_t *run, int ei, sb_t *out)
{
  a5_event_rt &rt = (*run->events)[ei];

  if (rt.next_command != A5_CMD_NONE)
    {
      switch (rt.next_command)
        {
        case A5_CMD_START:  ev_lstart (run, ei, 0, out); break;
        case A5_CMD_STOP:   ev_lstop  (run, ei, 0, out); break;
        case A5_CMD_PAUSE:  if (rt.status == A5_EV_RUNNING) rt.status = A5_EV_PAUSED; break;
        case A5_CMD_RESUME: if (rt.status == A5_EV_PAUSED)  rt.status = A5_EV_RUNNING; break;
        }
      rt.next_command = A5_CMD_NONE;
      rt.triggering_task.clear ();
    }

  switch (rt.status)
    {
    case A5_EV_COUNTDOWN:
      ev_set_timer_to_end (run, ei, rt.timer_to_end - 1, out);
      break;
    case A5_EV_RUNNING:
      if (!rt.just_started)
        ev_set_timer_to_end (run, ei, rt.timer_to_end - 1, out);
      break;
    default:
      break;
    }

  if (!rt.just_started)
    ev_do_subevents (run, ei, out);
  rt.just_started = 0;
}

/* Defer a control's Start/Stop/etc. unless we are already inside an event tick
   (clsEvent.Start/Stop: NextCommand vs immediate). */
static void
ev_control (a5_run_t *run, int ei, int cmd, const char *task_key, sb_t *out)
{
  a5_event_rt &rt = (*run->events)[ei];
  int force = (cmd == A5_CMD_START) ? 0 : 0;  /* control triggers never force */
  if (run->events_running)
    {
      switch (cmd)
        {
        case A5_CMD_START:  ev_lstart (run, ei, 0, out); break;
        case A5_CMD_STOP:   ev_lstop  (run, ei, 0, out); break;
        case A5_CMD_PAUSE:  if (rt.status == A5_EV_RUNNING) rt.status = A5_EV_PAUSED; break;
        case A5_CMD_RESUME: if (rt.status == A5_EV_PAUSED)  rt.status = A5_EV_RUNNING; break;
        }
    }
  else
    rt.next_command = cmd;
  (void) force;
  rt.triggering_task = task_key ? task_key : "";
}

/* clsTask.Children(True): is `candidate` a (recursive) Specific-override
   descendant of `ancestor`?  Used by the control loop so a parent task does not
   re-trigger an event/walk control that one of its override children already
   triggered -- clsUserSession.vb:872/893
   `Not task.Children(True).Contains(e.sTriggeringTask)`.  (E.g. the parent
   AttackCharacterWithObject must NOT re-fire a control its override child
   s_AttackTheT already handled, which would shift Spectre's noon-bell event.) */
static int
task_is_descendant (const a5_adventure_t *adv, const char *ancestor,
                    const char *candidate, int depth)
{
  const a5_task_t *c;
  if (depth > 16 || candidate == NULL || ancestor == NULL)
    return 0;
  c = a5model_task (adv, candidate);
  if (c == NULL || !streq (c->type, "Specific") || c->general_key == NULL)
    return 0;
  if (streq (c->general_key, ancestor))
    return 1;
  return task_is_descendant (adv, ancestor, c->general_key, depth + 1);
}

/* clsUserSession: when a task completes (bPass), fire any EventControls. */
void
ev_on_task_completed (a5_run_t *run, const char *task_key, sb_t *out)
{
  int ei;
  if (task_key == NULL)
    return;
  /* clsUserSession loops walks then events: fire WalkControls first. */
  wk_on_task_completed (run, task_key);
  for (ei = 0; ei < run->adv->n_events; ei++)
    {
      const a5_event_t *e = &run->adv->events[ei];
      a5_event_rt &rt = (*run->events)[ei];
      int ci;
      for (ci = 0; ci < e->n_controls; ci++)
        {
          const a5_eventctrl_t *c = &e->controls[ci];
          if (!c->on_completion || !streq (c->task_key, task_key))
            continue;
          /* Guard against a task re-triggering the control it just triggered,
             and (FD's children check) against a parent re-firing a control one
             of its override descendants already triggered. */
          if (!rt.triggering_task.empty ()
              && (rt.triggering_task == task_key
                  || task_is_descendant (run->adv, task_key,
                                         rt.triggering_task.c_str (), 0)))
            continue;
          switch (c->control)
            {
            case A5_CTRL_START:   ev_control (run, ei, A5_CMD_START,  task_key, out); break;
            case A5_CTRL_STOP:    ev_control (run, ei, A5_CMD_STOP,   task_key, out); break;
            case A5_CTRL_SUSPEND: ev_control (run, ei, A5_CMD_PAUSE,  task_key, out); break;
            case A5_CTRL_RESUME:  ev_control (run, ei, A5_CMD_RESUME, task_key, out); break;
            }
        }
    }
}

/* Run a task fired by an event (clsUserSession.AttemptToExecuteTask, bEvent):
   restriction-checked, silent on failure, and itself a control trigger. */
static void
attempt_event_task_impl (a5_run_t *run, const char *key, int depth, sb_t *out)
{
  a5_state_t *st = run->st;
  const a5_task_t *t = a5model_task (st->adv, key);
  int ti;
  if (t == NULL || depth > 16)
    return;
  ti = a5state_task_index (st, key);
  if (ti >= 0 && st->task_done[ti] && !t->repeatable)
    return;

  /* FrankenDrift runs an event-fired task through the *same* AttemptToExecuteTask
     as a command: it copies the leftover command references
     (`CopyNewRefs(NewReferences)`) and `ExecuteSubTasks`-iterates them one item at
     a time.  After a plural `%objects%` command whose final response reference
     still holds N items (`NewReferences = refs` in the Display loop,
     clsUserSession.vb:868), that copy carries all N items, so the event task runs
     once PER item.  This is why Amazon's `get ammo and rifle` ticks
     `ts_tasIncrement` twice (+2 minutes) where `get crown and bottle` -- whose
     two takes leave a single-item final reference -- ticks once.  Scarier's
     `resp_flush` already leaves `st->ref_items` equal to FD's post-Display
     `NewReferences`, so iterate it here.  A 0/1-item leftover keeps the original
     single, refs-cleared run (the overwhelmingly common case stays byte-exact). */
  if (st->n_ref_items > 1)
    {
      int nleft = st->n_ref_items;
      char tchar = st->ref_items_type;
      const char *grp  = (tchar == 'c') ? "characters" : "objects";
      const char *rbnd = (tchar == 'c') ? "ReferencedCharacters" : "ReferencedObjects";
      std::vector<const char *> items (st->ref_items, st->ref_items + nleft);
      for (const char *it : items)
        {
          /* FD AttemptToExecuteSubTask ReDims NewReferences to this single item;
             mirror run_general's per-item bind so a task that *does* read its
             reference resolves the right one (the increment reads none). */
          a5state_clear_refs (st);
          st->ref_items[0] = it;
          st->n_ref_items = 1;
          st->ref_items_type = tchar;
          bind_reference (st, grp, it, it);
          a5state_bind_ref (st, rbnd, it);
          if (a5restr_pass (st, t->restrictions))
            run_task (run, t, depth + 1, out);
        }
      if (ti >= 0)
        st->task_done[ti] = 1;
      ev_on_task_completed (run, key, out);
      return;
    }

  a5state_clear_refs (st);                 /* event tasks carry no command refs */
  if (!a5restr_pass (st, t->restrictions))
    return;
  run_task (run, t, depth + 1, out);
  if (ti >= 0)
    st->task_done[ti] = 1;
  ev_on_task_completed (run, key, out);
}

/* Each event-fired task is its own AttemptToExecuteTask in FD, with a fresh
   htblResponsesPass -- so give each attempt_event_task its own dedup scope
   (save/restore the previous one for a nested event task).  Within one scope a
   completion message emitted twice via SetTasks-Execute (run_action, which shares
   the current scope, not a new attempt_event_task) shows once -- e.g. Pathway's
   Task5 "The End" banner, executed by both Task33 and its parent Task30. */
static void
attempt_event_task (a5_run_t *run, const char *key, int depth, sb_t *out)
{
  std::set<std::string> seen;
  exec_resp_scope escope;
  std::set<std::string> *prev = run->ev_seen;
  exec_resp_scope *prev_escope = run->exec_scope;
  run->ev_seen = &seen;
  run->exec_scope = &escope;
  attempt_event_task_impl (run, key, depth, out);
  run->ev_seen = prev;
  run->exec_scope = prev_escope;
  exec_scope_flush (run, &escope, out);
}

/* Drain clsAdventure.qTasksToRun: run each LocationTrigger-armed System task in
   FIFO order (clsUserSession.vb:3421 "While qTasksToRun.Count > 0").  A drained
   task may itself move the Player and arm further triggers, so loop until empty.
   Runs after the turn's command task and before TurnBasedStuff. */
static void
drain_tasks_to_run (a5_run_t *run, sb_t *out)
{
  size_t guard = 0;
  while (!run->tasks_to_run->empty () && guard++ < 256)
    {
      std::string key = run->tasks_to_run->front ();
      run->tasks_to_run->erase (run->tasks_to_run->begin ());
      attempt_event_task (run, key.c_str (), 0, out);
    }
}

/* clsUserSession.TurnBasedStuff: tick every turn-based event once. */
void
ev_tick_all (a5_run_t *run, sb_t *out)
{
  int ei;
  /* clsUserSession.ProcessTurn drains qTasksToRun after the command's task,
     before TurnBasedStuff; do the same here (ev_tick_all is the turn's only
     TurnBasedStuff entry). */
  drain_tasks_to_run (run, out);
  if (run->st->game_over)
    return;
  /* TurnBasedStuff: tick the walks first, then the turn-based events. */
  wk_tick_all (run, out);
  run->events_running = 1;
  for (ei = 0; ei < run->adv->n_events && !run->st->game_over; ei++)
    if (streq (run->adv->events[ei].type, "TurnBased")
        || run->adv->events[ei].type == NULL)
      ev_increment (run, ei, out);
  /* Separate loop: an event started by a later event must not also tick. */
  for (ei = 0; ei < run->adv->n_events; ei++)
    (*run->events)[ei].just_started = 0;
  run->events_running = 0;
}

/* Game start: set each event's initial status (clsUserSession init loop). */
void
ev_init (a5_run_t *run, sb_t *out)
{
  int ei;
  run->events_running = 1;
  for (ei = 0; ei < run->adv->n_events; ei++)
    {
      const a5_event_t *e = &run->adv->events[ei];
      a5_event_rt &rt = (*run->events)[ei];
      switch (rt.when_start)
        {
        case 3:                                  /* AfterATask */
          rt.status = A5_EV_NOTYET;
          break;
        case 2:                                  /* BetweenXandYTurns */
          rt.status = A5_EV_COUNTDOWN;
          /* FD: TimerToEndOfEvent = StartDelay.Value + Length.Value -- VB
             evaluates left-to-right, so StartDelay draws BEFORE Length.  Draw in
             that exact order or the two RNG values get swapped (desyncing the
             whole stream and every random timer downstream). */
          {
            long delay = a5rand_between (e->start_from, e->start_to);
            rt.length_value = a5rand_between (e->length_from, e->length_to);
            ev_set_timer_to_end (run, ei, delay + rt.length_value, out);
          }
          break;
        case 1:                                  /* Immediately */
          ev_lstart (run, ei, 0, out);
          break;
        }
    }
  for (ei = 0; ei < run->adv->n_events; ei++)
    (*run->events)[ei].just_started = 0;
  run->events_running = 0;
  /* Start the StartActive walks (clsUserSession game-start init loop). */
  wk_init (run, out);
}

/* =========================================================== walks (clsWalk) */

/* A faithful port of clsWalk's per-turn state machine (clsCharacter.vb:1175+):
   IncrementTimer / DoAnySteps / DoAnySubWalks / lStart / lStop, driven each turn
   by clsUserSession.TurnBasedStuff (before the events) and on task completion by
   the WalkControls.  Walks reuse the event Status/Command enums (no
   CountingDownToStart). */

static void wk_lstart   (a5_run_t *run, int wi, int restart, sb_t *out);
static void wk_lstop    (a5_run_t *run, int wi, int run_subs, int reached_end, sb_t *out);
static void wk_do_steps (a5_run_t *run, int wi, sb_t *out);
static void wk_do_subwalks (a5_run_t *run, int wi, sb_t *out);

static long wk_from_start (const a5_walk_rt &rt) { return rt.length - rt.timer_to_end; }
static long wk_from_last  (const a5_walk_rt &rt) { return wk_from_start (rt) - rt.last_sw_time; }

/* DirectionsEnum order (Global.vb:146) so DirectionTo is deterministic
   regardless of the XML <Movement> order. */
static const char *const DIR_ORDER[12] = {
  "North", "East", "South", "West", "Up", "Down", "In", "Out",
  "NorthEast", "SouthEast", "SouthWest", "NorthWest"
};

/* The raw Destination of `lockey` in canonical direction `dir`, ignoring route
   restrictions (clsLocation.arlDirections(d).LocationKey: adjacency uses the
   unconditional map), or NULL. */
static const char *
loc_raw_exit (a5_state_t *st, const char *lockey, const char *dir)
{
  const a5_location_t *l = lockey ? a5model_location (st->adv, lockey) : NULL;
  const a5_xml_node_t *c;
  if (l == NULL)
    return NULL;
  for (c = l->node->first_child; c != NULL; c = c->next)
    {
      const char *d, *dest;
      if (strcmp (c->name, "Movement") != 0)
        continue;
      d = a5xml_child_text (c, "Direction");
      if (!streq (d, dir))
        continue;
      dest = a5xml_child_text (c, "Destination");
      return (dest != NULL && dest[0] != '\0') ? dest : NULL;
    }
  return NULL;
}

/* clsLocation.IsAdjacent: any exit of `lockey` whose destination is `destkey`. */
static int
loc_is_adjacent (a5_state_t *st, const char *lockey, const char *destkey)
{
  int d;
  if (lockey == NULL || destkey == NULL)
    return 0;
  for (d = 0; d < 12; d++)
    if (streq (loc_raw_exit (st, lockey, DIR_ORDER[d]), destkey))
      return 1;
  return 0;
}

/* clsLocation.DirectionTo: prose direction from `fromkey` to `destkey`. */
static std::string
loc_direction_to (a5_state_t *st, const char *fromkey, const char *destkey)
{
  int d;
  if (streq (fromkey, destkey))
    return "not moved";
  for (d = 0; d < 12; d++)
    if (streq (loc_raw_exit (st, fromkey, DIR_ORDER[d]), destkey))
      switch (d)
        {
        case 0:  return "the north";
        case 1:  return "the east";
        case 2:  return "the south";
        case 3:  return "the west";
        case 4:  return "above";
        case 5:  return "below";
        case 6:  return "inside";
        case 7:  return "outside";
        case 8:  return "the north-east";
        case 9:  return "the south-east";
        case 10: return "the south-west";
        default: return "the north-west";
        }
  return "nowhere";
}

/* The current player character's key (clsAdventure.Player.Key) -- the Type=Player
   character at load, retargeted by a ToSwitchWith/BECOME. */
static const char *
walk_player_key (a5_state_t *st)
{
  return a5state_player_key (st);
}

/* clsCharacter.GetPropertyValue for a character property: runtime override if
   set, else the static value -- which for a rich Text property is stored as a
   <Description> (value_node) and must be rendered.  Returns a malloc'd string
   when the property is *present* (HasProperty), or NULL when absent so the
   caller can apply its default.  Mirrors FD's `If .HasProperty(k) Then s =
   .GetPropertyValue(k)`. */
static char *
char_prop_value (a5_state_t *st, const char *charkey, const char *propkey)
{
  const char *ov = a5state_entity_prop (st, charkey, propkey);
  if (ov != NULL)
    return strdup (ov);
  const a5_character_t *c = a5model_character (st->adv, charkey);
  if (c == NULL)
    return NULL;
  const a5_prop_t *p = a5_prop_find (c->props, c->n_props, propkey);
  if (p == NULL)
    return NULL;
  if (p->value_node != NULL)
    return a5text_eval_description (st, p->value_node);
  return strdup (p->value ? p->value : "");
}

/* The "ShowEnterExit" message a walking NPC shows when it enters or leaves the
   player's room (clsWalk.DoAnySteps).  Evaluated with the character's *current*
   (pre-move) location vs the player's, and the move destination `dest`. */
static void
wk_show_enter_exit (a5_run_t *run, int ci, const char *dest, sb_t *out)
{
  a5_state_t *st = run->st;
  const a5_character_t *c = &st->adv->characters[ci];
  const char *cloc = st->char_loc[ci];
  const char *ploc = a5state_player_location (st);

  /* The player's own walk never narrates this; only a ShowEnterExit NPC that is
     currently "at location" (not on/in an object). */
  if (ci == a5state_character_index (st, walk_player_key (st)))
    return;
  if (a5_prop_find (c->props, c->n_props, "ShowEnterExit") == NULL
      && a5state_entity_prop (st, c->key, "ShowEnterExit") == NULL)
    return;
  if (st->char_onobj[ci] != NULL || cloc == NULL)
    return;

  if (streq (cloc, ploc))                      /* leaving the player's room */
    {
      char *exits = char_prop_value (st, c->key, "CharExits");
      char *nm = a5text_char_proper_name (st, c->key);
      std::string s = std::string (nm) + " " + (exits ? exits : "exits");
      free (nm);
      free (exits);
      if (ploc != NULL && loc_is_adjacent (st, ploc, dest))
        {
          std::string dir = loc_direction_to (st, cloc, dest);
          if (dir != "nowhere")
            {
              s += (dir == "outside" || dir == "inside") ? " " : " to ";
              s += dir;
            }
        }
      s += ".";
      sb_pspace (out); sb_puts (out, s.c_str ());
    }
  else if (streq (dest, ploc))                 /* entering the player's room */
    {
      char *enters = char_prop_value (st, c->key, "CharEnters");
      char *nm = a5text_char_proper_name (st, c->key);
      std::string s = std::string (nm) + " " + (enters ? enters : "enters");
      free (nm);
      free (enters);
      if (ploc != NULL && loc_is_adjacent (st, ploc, cloc))
        {
          std::string dir = loc_direction_to (st, dest, cloc);
          if (dir != "nowhere")
            s += " from " + dir;
        }
      s += ".";
      sb_pspace (out); sb_puts (out, s.c_str ());
    }
}

/* clsWalk.DoAnySteps: when the from-start timer reaches a step's cumulative
   offset, move the character toward that step's destination (a location, a
   random adjacent member of a location group, or toward another character). */
static void
wk_do_steps (a5_run_t *run, int wi, sb_t *out)
{
  a5_state_t *st = run->st;
  a5_walk_rt &rt = (*run->walks)[wi];
  const a5_walk_t *wk = rt.walk;
  int ci = rt.char_index;
  long step_len = 0;
  int si;
  if (rt.status != A5_EV_RUNNING)
    return;
  for (si = 0; si < wk->n_steps; si++)
    {
      if (step_len == wk_from_start (rt))
        {
          const char *destkey = wk->steps[si].location;
          const char *cloc = st->char_loc[ci];
          const char *dest = NULL;                  /* sDestination */
          const a5_group_t *grp = NULL;
          int g, c2;
          if (streq (destkey, "%Player%"))
            destkey = walk_player_key (st);
          for (g = 0; g < st->adv->n_groups; g++)
            if (streq (st->adv->groups[g].key, destkey))
              { grp = &st->adv->groups[g]; break; }

          if (grp != NULL && grp->n_members > 0)
            {
              int has_adj = 0, m;
              if (cloc != NULL)
                for (m = 0; m < grp->n_members; m++)
                  if (loc_is_adjacent (st, cloc, grp->members[m]))
                    { has_adj = 1; break; }
              if (has_adj)
                {
                  int guard = 0;
                  while (dest == NULL && guard++ < 10000)
                    {
                      const char *poss =
                        grp->members[a5rand_between (0, grp->n_members - 1)];
                      if (cloc == NULL || loc_is_adjacent (st, cloc, poss))
                        dest = poss;
                    }
                }
              else
                dest = grp->members[a5rand_between (0, grp->n_members - 1)];
            }
          else if ((c2 = a5state_character_index (st, destkey)) >= 0)
            {
              /* Follow a character: only step in if it is in an adjacent room. */
              const char *c2loc = st->char_loc[c2];
              if (!streq (cloc, c2loc) && cloc != NULL && c2loc != NULL
                  && loc_is_adjacent (st, cloc, c2loc))
                dest = c2loc;
            }
          else
            dest = destkey;                          /* a literal location key */

          if (dest != NULL && (streq (dest, "Hidden")
                               || a5model_location (st->adv, dest) != NULL))
            {
              wk_show_enter_exit (run, ci, dest, out);
              st->char_loc[ci] = streq (dest, "Hidden") ? NULL : dest;
              st->char_onobj[ci] = NULL;             /* now "at location" */
            }
        }
      step_len += rt.step_dur[si];
    }
}

/* clsWalk.DoAnySubWalks: the sub-walk activities (DisplayMessage / ExecuteTask /
   UnsetTask), triggered by FromStart / FromLast / BeforeEnd offsets or by the
   ComesAcross transition (the walker entering the subject's room). */
static void
wk_do_subwalks (a5_run_t *run, int wi, sb_t *out)
{
  a5_state_t *st = run->st;
  a5_walk_rt &rt = (*run->walks)[wi];
  const a5_walk_t *wk = rt.walk;
  int ci = rt.char_index;
  int i;
  if (rt.status != A5_EV_RUNNING)
    return;
  for (i = 0; i < wk->n_subwalks; i++)
    {
      const a5_subwalk_t *sw = &wk->subwalks[i];
      long ft = rt.sw_ft[i];
      int go = 0;
      switch (sw->when)
        {
        case A5_SW_FROM_START:
          if (wk_from_start (rt) == ft && ft <= rt.length)
            go = 1;
          break;
        case A5_SW_FROM_LAST:
          if (wk_from_last (rt) == ft
              && ((rt.last_sw_index == -1 && i == 0)
                  || (i > 0 && rt.last_sw_index == i - 1)))
            go = 1;
          break;
        case A5_SW_BEFORE_END:
          if (rt.timer_to_end == ft)
            go = 1;
          break;
        case A5_SW_COMES_ACROSS:
          {
            const char *comekey = sw->come_key;
            int prev = rt.came_across[i], now, cc;
            if (streq (comekey, "%Player%"))
              comekey = walk_player_key (st);
            cc = a5state_character_index (st, comekey);
            now = (cc >= 0 && st->char_loc[ci] != NULL
                   && streq (st->char_loc[ci], st->char_loc[cc]));
            rt.came_across[i] = (char) now;
            if (!prev && now)
              go = 1;
          }
          break;
        }
      if (!go)
        continue;
      switch (sw->what)
        {
        case A5_SW_DISPLAY:
          if (sw->description != NULL && sw->only_apply_at != NULL
              && sw->only_apply_at[0] != '\0'
              && a5state_in_group_or_location (st, a5state_player_key (st), sw->only_apply_at))
            { char *m = a5text_describe (st, sw->description);
              if (msg_has_output (m)) { sb_pspace (out); sb_puts (out, m); }
              free (m); }
          break;
        case A5_SW_EXECTASK:
          if (sw->task_key != NULL)
            attempt_event_task (run, sw->task_key, 0, out);
          break;
        case A5_SW_UNSETTASK:
          if (sw->task_key != NULL)
            { int ti = a5state_task_index (st, sw->task_key);
              if (ti >= 0) st->task_done[ti] = 0; }
          break;
        }
      rt.last_sw_time  = wk_from_start (rt);
      rt.last_sw_index = i;
    }
}

/* TimerToEndOfWalk setter (clsWalk): hitting 0 while running stops the walk
   (running its sub-walks) and loops it if set to loop. */
static void
wk_set_timer (a5_run_t *run, int wi, long value, sb_t *out)
{
  a5_walk_rt &rt = (*run->walks)[wi];
  rt.timer_to_end = value;
  if (rt.status == A5_EV_RUNNING && rt.timer_to_end == 0)
    wk_lstop (run, wi, 1, 1, out);                /* lStop(True, True) */
}

static void
wk_lstart (a5_run_t *run, int wi, int restart, sb_t *out)
{
  a5_walk_rt &rt = (*run->walks)[wi];
  const a5_walk_t *wk = rt.walk;
  int i;
  if (!(rt.status == A5_EV_NOTYET || rt.status == A5_EV_FINISHED
        || (rt.status == A5_EV_RUNNING && restart)))
    return;
  rt.status = A5_EV_RUNNING;
  /* ResetLength: re-roll each step's duration; Length = their sum.  (clsWalk
     does NOT reset LastSubWalk / iLastSubWalkTime or the sub-walk offsets.) */
  rt.length = 0;
  for (i = 0; i < wk->n_steps; i++)
    {
      rt.step_dur[i] = a5rand_between (wk->steps[i].ft_from, wk->steps[i].ft_to);
      rt.length += rt.step_dur[i];
    }
  wk_set_timer (run, wi, rt.length, out);
  if (wk_from_start (rt) == 0)
    { wk_do_steps (run, wi, out); wk_do_subwalks (run, wi, out); }
  rt.just_started = 1;
}

static void
wk_lstop (a5_run_t *run, int wi, int run_subs, int reached_end, sb_t *out)
{
  a5_walk_rt &rt = (*run->walks)[wi];
  if (run_subs)
    wk_do_subwalks (run, wi, out);
  rt.status = A5_EV_FINISHED;
  if (rt.walk->loops && rt.timer_to_end == 0 && reached_end && rt.length > 0)
    wk_lstart (run, wi, 1, out);                  /* restart a looping walk */
}

static void
wk_increment (a5_run_t *run, int wi, sb_t *out)
{
  a5_walk_rt &rt = (*run->walks)[wi];
  if (rt.next_command != A5_CMD_NONE)
    {
      switch (rt.next_command)
        {
        case A5_CMD_START:   wk_lstart (run, wi, 0, out); break;
        case A5_CMD_STOP:    wk_lstop  (run, wi, 0, 0, out); break;
        case A5_CMD_PAUSE:   if (rt.status == A5_EV_RUNNING) rt.status = A5_EV_PAUSED; break;
        case A5_CMD_RESUME:  if (rt.status == A5_EV_PAUSED)  rt.status = A5_EV_RUNNING; break;
        case A5_CMD_RESTART: wk_lstart (run, wi, 1, out); break;
        }
      rt.next_command = A5_CMD_NONE;
      rt.triggering_task.clear ();
    }
  if (rt.status == A5_EV_RUNNING && !rt.just_started)
    wk_set_timer (run, wi, rt.timer_to_end - 1, out);
  if (!rt.just_started)
    { wk_do_steps (run, wi, out); wk_do_subwalks (run, wi, out); }
  rt.just_started = 0;
}

/* Defer a control's Start/Stop/etc. to the next IncrementTimer (clsWalk.Start /
   Stop / Pause / Resume all set NextCommand; a Start after a pending Stop becomes
   a Restart). */
static void
wk_control (a5_run_t *run, int wi, int ctrl, const char *task_key)
{
  a5_walk_rt &rt = (*run->walks)[wi];
  switch (ctrl)
    {
    case A5_CTRL_START:
      rt.next_command = (rt.next_command == A5_CMD_STOP) ? A5_CMD_RESTART
                                                         : A5_CMD_START;
      break;
    case A5_CTRL_STOP:    rt.next_command = A5_CMD_STOP;   break;
    case A5_CTRL_SUSPEND: rt.next_command = A5_CMD_PAUSE;  break;
    case A5_CTRL_RESUME:  rt.next_command = A5_CMD_RESUME; break;
    }
  rt.triggering_task = task_key ? task_key : "";
}

/* clsUserSession (bPass): fire any WalkControls keyed on a completing task.
   Runs before the EventControls (clsUserSession loops walks then events). */
static void
wk_on_task_completed (a5_run_t *run, const char *task_key)
{
  size_t wi;
  if (task_key == NULL)
    return;
  for (wi = 0; wi < run->walks->size (); wi++)
    {
      a5_walk_rt &rt = (*run->walks)[wi];
      const a5_walk_t *wk = rt.walk;
      int ci;
      for (ci = 0; ci < wk->n_controls; ci++)
        {
          const a5_eventctrl_t *c = &wk->controls[ci];
          if (!c->on_completion || !streq (c->task_key, task_key))
            continue;
          if (!rt.triggering_task.empty ()
              && (rt.triggering_task == task_key
                  || task_is_descendant (run->adv, task_key,
                                         rt.triggering_task.c_str (), 0)))
            continue;
          wk_control (run, (int) wi, c->control, task_key);
        }
    }
}

/* clsUserSession.TurnBasedStuff: tick every walk once (before the events). */
static void
wk_tick_all (a5_run_t *run, sb_t *out)
{
  size_t wi;
  if (run->st->game_over)
    return;
  for (wi = 0; wi < run->walks->size () && !run->st->game_over; wi++)
    wk_increment (run, (int) wi, out);
}

/* Game start: start the StartActive walks (clsUserSession init loop). */
static void
wk_init (a5_run_t *run, sb_t *out)
{
  size_t wi;
  for (wi = 0; wi < run->walks->size (); wi++)
    if ((*run->walks)[wi].walk->start_active)
      wk_lstart (run, (int) wi, 0, out);
  for (wi = 0; wi < run->walks->size (); wi++)
    (*run->walks)[wi].just_started = 0;
}
