/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- turn loop core.  See a5run.h.
 *
 * A port of the player-turn half of clsUserSession: GetGeneralTask (walk tasks
 * in priority order, match command, resolve references, refine by restrictions)
 * plus the per-turn driver (a5run_input), run state, status-line / media
 * accessors, the "Which X?" disambiguation prompt (DisplayAmbiguityQuestion /
 * ResolveAmbiguity) and save/restore.  The turn loop was split across sibling
 * translation units; the shared run struct and cross-file entry points live in
 * a5run_internal.h:
 *
 *   a5run_ref.cpp     reference resolution (InputMatchesObject/Character) and
 *                     multiple-object (%objects%) references
 *   a5run_action.cpp  ExecuteActions / ExecuteSingleAction, task dispatch,
 *                     response aggregation, conversation, run_task/run_general
 *   a5run_events.cpp  the clsEvent / clsWalk turn-based runtime
 *   a5sb.cpp          the output buffer (sb_t) shared by all of the above
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
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

int a5run_trace = 0;

/* -------------------------------------------------------------- message tests */

/* True when `m` has visible content (FD's bHasOutput, approximated): a message
   that renders to nothing but whitespace produces no output and is skipped (the
   stock cl_PAtStartOp "page" task has an empty Before message, for instance).
   Messages with real text are emitted verbatim -- their own trailing newline,
   if any, drives the pSpace line/paragraph break before the next message. */
int
msg_has_output (const char *m)
{
  if (m == NULL) return 0;
  for (; *m; m++)
    if (*m != '\n' && *m != '\r' && *m != ' ' && *m != '\t')
      return 1;
  return 0;
}

/* A faithful port of clsUserSession.bHasOutput (vb:1272), applied to a message
   string with its markup STILL EMBEDDED (the value FD's Display() sees, before
   the final HTML->plain pass).  Unlike msg_has_output (which inspects the
   already-plain text and treats whitespace as nothing), FD keeps a message when
   stripping all <...> tags leaves ANY character behind -- including a bare space.
   This is why Amazon's title music task `<audio play src="...">` followed by a
   space counts as output: StripCarats leaves " ", so the space joins onto the
   centred title (giving its leading indent).  When stripping leaves nothing, FD
   keeps the message only if it carries a known formatting/media tag, or the whole
   string is an ALR (Text Override) trigger key. */
int
fd_has_output (a5_state_t *st, const char *raw)
{
  const char *p;
  int i;
  if (raw == NULL || raw[0] == '\0')
    return 0;
  /* StripCarats: is there any character outside a <...> tag? */
  for (p = raw; *p; )
    {
      if (*p == '<')
        { while (*p && *p != '>') p++; if (*p == '>') p++; }
      else
        return 1;                       /* a non-tag char survives -> output */
    }
  /* All tags: FD keeps it only for a recognised formatting/media tag. */
  {
    static const char *const known[] = {
      "<br>", "<center>", "<centre>", "<i>", "</i>", "<b>", "</b>", "<u>",
      "</u>", "<c>", "</c>", "<font", "</font>", "<right>", "</right>",
      "<left>", "</left>", "<del>", "<wait", "<cls>", "<img " };
    std::string low;
    for (p = raw; *p; p++)
      low += (char) tolower ((unsigned char) *p);
    for (size_t k = 0; k < sizeof known / sizeof known[0]; k++)
      if (low.find (known[k]) != std::string::npos)
        return 1;
  }
  /* An ALR may rewrite the whole tag-only string into something visible. */
  for (i = 0; i < st->adv->n_alrs; i++)
    if (st->adv->alrs[i].old_text != NULL
        && strcmp (st->adv->alrs[i].old_text, raw) == 0)
      return 1;
  return 0;
}

/* The run-state struct (a5_run_s), the per-event/walk runtime records, the
   status/command enums and streq live in a5run_internal.h, shared with the
   a5run_*.cpp files split out of this one. */

/* The live run, for the a5expr event-property hook (event timers live here, not
   in a5_state).  Set in a5run_new; single-run headless harness, so a bare static
   is sufficient (mirrors the single global a5sexpr_rng_hook). */
static a5_run_t *g_event_hook_run = NULL;

/* a5expr_event_prop_hook: Event.Position -> TimerFromStartOfEvent
   (Length.Value - TimerToEndOfEvent), Event.Length -> Length.Value. */
static int
a5run_event_prop (const char *event_key, const char *prop, long *out)
{
  a5_run_t *run = g_event_hook_run;
  if (run == NULL || event_key == NULL) return 0;
  for (int i = 0; i < run->adv->n_events; i++)
    if (streq (run->adv->events[i].key, event_key))
      {
        const a5_event_rt &rt = (*run->events)[i];
        if (streq (prop, "Position")) { *out = rt.length_value - rt.timer_to_end; return 1; }
        if (streq (prop, "Length"))   { *out = rt.length_value; return 1; }
        return 0;
      }
  return 0;
}

a5_state_t *
a5run_state (a5_run_t *run) { return run->st; }

int
a5run_is_over (a5_run_t *run) { return run->st->game_over; }

/* ----------------------------------------------------- status-line accessors */

/* The player's current location short description (room NAME) as plain text,
   caller frees; NULL if the location is unknown. */
char *
a5run_location_name (a5_run_t *run)
{
  const char *loc = a5state_player_location (run->st);
  if (loc == NULL)
    return NULL;
  return a5text_location_short_plain (run->st, loc);
}

/* Adventure.Score / MaxScore (FD reads htblVariables("Score"/"MaxScore"); see
   emit_endgame).  0 when the game defines no such variable. */
long
a5run_score (a5_run_t *run)
{
  int i = a5state_variable_index (run->st, "Score");
  return i >= 0 ? run->st->var_num[i] : 0;
}

long
a5run_maxscore (a5_run_t *run)
{
  int i = a5state_variable_index (run->st, "MaxScore");
  return i >= 0 ? run->st->var_num[i] : 0;
}

int
a5run_turns (a5_run_t *run) { return run->st->turns; }

/* ------------------------------------------------------------- media capture */

/* a5text media sink: resolve the src path to a Blorb resource number (via the
   game's <FileMappings>) and record the event on the run's per-turn list. */
static void
a5run_media_cb (void *ctx, int kind, const char *src, int channel, int loop)
{
  a5_run_t *run = (a5_run_t *) ctx;
  a5_media_event_t ev;
  ev.kind = kind;
  ev.channel = channel;
  ev.loop = loop;
  ev.number = (src != NULL) ? a5model_resource_for_file (run->adv, src) : -1;
  /* A description may be rendered more than once internally during a turn; drop
     an image already recorded this turn so it is not shown twice. */
  if (kind == A5_MEDIA_IMAGE)
    for (size_t i = 0; i < run->media->size (); i++)
      if ((*run->media)[i].kind == A5_MEDIA_IMAGE
          && (*run->media)[i].number == ev.number)
        return;
  run->media->push_back (ev);
}

/* Install/remove the media sink around a turn's text generation. */
static void
a5run_media_begin (a5_run_t *run)
{
  run->media->clear ();
  a5text_set_media_sink (a5run_media_cb, run);
}

static void
a5run_media_end (void)
{
  a5text_set_media_sink (NULL, NULL);
}

int
a5run_media_count (a5_run_t *run) { return (int) run->media->size (); }

const a5_media_event_t *
a5run_media_get (a5_run_t *run, int i)
{
  if (i < 0 || (size_t) i >= run->media->size ())
    return NULL;
  return &(*run->media)[i];
}

a5_run_t *
a5run_new (const a5_adventure_t *adv)
{
  a5_run_t *run;
  int i;
  if (adv == NULL)
    return NULL;
  /* Deterministic RNG sequence per game (seed 1234), like the other Scarier
     headless engines; routes RAND through the shared erkyrath_random.  When the
     v5 path is wired into Spatterlight this should honour the determinism /
     native-seed toggle, as scutils.cpp does for the v4 engine. */
  a5rand_seed (1234u);
  /* Let embedded <#...#> expressions (OneOf/Either/Rand) draw from the same RNG
     stream as the rest of the engine -- a5sexpr is otherwise RNG-less. */
  a5sexpr_rng_hook = a5rand_between;
  /* Let `Event.Position`/`Event.Length` OO-expressions read the live event
     timers (e.g. JacarandaJim's rain `<# If(RAND(8)=1, ...) #>` gated on
     `Event12.Position > 0`). */
  a5expr_event_prop_hook = a5run_event_prop;
  /* Install the game's localized direction synonyms (the localization
     subsystem); English when the <Direction*> fields are absent.  Resets the
     parser's direction table, so every game starts from a known state. */
  a5parse_set_directions (adv->dir_re);
  run = new a5_run_s;
  run->adv = adv;
  run->st = a5state_new (adv);
  run->media = new std::vector<a5_media_event_t>;
  run->order = new std::vector<int>;
  run->events = new std::vector<a5_event_rt> (adv->n_events);
  run->events_running = 0;
  run->event_out = NULL;
  for (i = 0; i < adv->n_events; i++)
    {
      a5_event_rt &rt = (*run->events)[i];
      rt.status = A5_EV_NOTYET;
      rt.length_value = 0;
      rt.timer_to_end = 0;
      rt.last_se_time = 0;
      rt.last_se_index = -1;
      rt.just_started = 0;
      rt.next_command = A5_CMD_NONE;
      rt.when_start = adv->events[i].when_start;
      rt.se_ft.assign ((size_t) adv->events[i].n_subevents, 0);
    }
  /* Flatten every character's walks into one runtime list (clsWalk instances).
     Sub-walk offsets resolve once here (clsWalk never resets them); step
     durations re-roll on each lStart. */
  run->walks = new std::vector<a5_walk_rt>;
  for (i = 0; i < adv->n_characters; i++)
    for (int w = 0; w < adv->characters[i].n_walks; w++)
      {
        const a5_walk_t *wk = &adv->characters[i].walks[w];
        a5_walk_rt rt;
        rt.walk = wk;
        rt.char_index = i;
        rt.status = A5_EV_NOTYET;
        rt.length = rt.timer_to_end = rt.last_sw_time = 0;
        rt.last_sw_index = -1;
        rt.just_started = 0;
        rt.next_command = A5_CMD_NONE;
        rt.step_dur.assign ((size_t) wk->n_steps, 0);
        rt.sw_ft.assign ((size_t) wk->n_subwalks, 0);
        rt.came_across.assign ((size_t) wk->n_subwalks, 0);
        for (int s = 0; s < wk->n_subwalks; s++)
          rt.sw_ft[s] = a5rand_between (wk->subwalks[s].ft_from,
                                        wk->subwalks[s].ft_to);
        run->walks->push_back (rt);
      }
  run->amb_active = 0;
  run->amb_task_index = run->amb_command_index = -1;
  run->amb_ref_type = 'o';
  run->known_words = NULL;
  run->resp = NULL;
  run->ev_seen = NULL;
  run->exec_scope = NULL;
  run->defer_look = 0;
  run->look_pending = 0;
  run->look_pos = 0;
  run->cur_score_ti = -1;
  run->look_pinned = NULL;
  run->tasks_to_run = new std::vector<std::string>;
  for (i = 0; i < adv->n_tasks; i++)
    run->order->push_back (i);
  /* TaskSorter: ascending Priority (lower value checked first). */
  std::stable_sort (run->order->begin (), run->order->end (),
                    [adv](int a, int b) {
                      return adv->tasks[a].priority < adv->tasks[b].priority;
                    });
  g_event_hook_run = run;
  return run;
}

void
a5run_free (a5_run_t *run)
{
  if (run == NULL)
    return;
  a5state_free (run->st);
  free (run->look_pinned);
  delete run->media;
  delete run->order;
  delete run->events;
  delete run->walks;
  delete run->known_words;
  delete run->tasks_to_run;
  delete run;
}

/* ------------------------------------------------------------ tokenisation */

std::vector<std::string>
split_ws (const char *s)
{
  std::vector<std::string> v;
  std::string cur;
  for (const char *p = s ? s : ""; ; p++)
    {
      if (*p == '\0' || isspace ((unsigned char) *p))
        { if (!cur.empty ()) { v.push_back (cur); cur.clear (); } if (*p == '\0') break; }
      else
        cur += *p;
    }
  return v;
}

std::string
lower (const std::string &s)
{
  std::string o = s;
  for (char &c : o) c = (char) tolower ((unsigned char) c);
  return o;
}

/* Reference resolution + multiple-object references live in
   a5run_ref.cpp; their entry points (resolve_refine, resolve_plural,
   *_visible / *_seen_p, possible_keys, bind_reference, ...) are
   declared in a5run_internal.h. */

/* Task execution -- action helpers, specific-override dispatch, response
   aggregation, conversation, ExecuteSingleAction and the run_task /
   run_general drivers -- lives in a5run_action.cpp; its entry points are
   declared in a5run_internal.h. */

/* ------------------------------------------------------ disambiguation prompt */

/* An entity's definite display name ("the Yellow Note" / a character's name). */
static std::string
entity_def_name (a5_state_t *st, const std::string &key, char type)
{
  if (type == 'o')
    {
      int oi = a5state_object_index (st, key.c_str ());
      if (oi >= 0)
        { char *n = a5text_object_name (&st->adv->objects[oi], A5_ART_DEFINITE);
          std::string s = n ? n : ""; free (n); return s; }
    }
  else
    {
      int ci = a5state_character_index (st, key.c_str ());
      if (ci >= 0 && st->adv->characters[ci].name)
        return st->adv->characters[ci].name;
    }
  return key;
}

/* "Which <word>?  The a, the b or the c."  Mirrors DisplayAmbiguityQuestion:
   the candidate list uses definite names joined ", "/" or ", run through ToProper
   with its default bStrict=False (capitalise only the first letter, leave the
   rest as-is so e.g. "PC door" keeps its casing), with pSpace's two spaces. */
static std::string
build_amb_prompt (a5_state_t *st, const std::string &word,
                  const std::vector<std::string> &keys, char type)
{
  std::string list;
  for (size_t i = 0; i < keys.size (); i++)
    {
      if (i > 0)
        list += (i + 1 == keys.size ()) ? " or " : ", ";
      list += entity_def_name (st, keys[i], type);
    }
  /* ToProper(bStrict=False): capitalise only the first character; the remainder
     keeps its original casing (clsUserSession.ToProper, FrankenDrift). */
  if (!list.empty ()) list[0] = (char) toupper ((unsigned char) list[0]);
  return "Which " + word + "?  " + list + ".";
}

/* Re-run the remembered task with its ambiguous reference pinned to `chosen`;
   returns 1 if it ran (RR_OK). */
static int
run_remembered (a5_run_t *run, const char *chosen, sb_t *out)
{
  a5_state_t *st = run->st;
  if (run->amb_task_index < 0)
    return 0;
  const a5_task_t *t = &run->adv->tasks[run->amb_task_index];
  a5_match_t m;
  if (!a5parse_match_command (t->commands[run->amb_command_index],
                              run->amb_input.c_str (), &m))
    return 0;
  if (resolve_refine (run, t, &m, run->amb_ref_name.c_str (), chosen, NULL) != RR_OK)
    return 0;
  run_general (run, t, &m, out);
  st->task_done[run->amb_task_index] = 1;
  ev_on_task_completed (run, t->key, out);
  return 1;
}

/* Emit the "You can't see any <plural>!" message for an out-of-scope object
   reference (clsUserSession.DisplayAmbiguityQuestion, bCanSeeAny=False). */
static void
emit_cantsee (a5_state_t *st, const amb_info *amb, sb_t *out)
{
  std::string w = amb_word (st, amb->keys, amb->ref_text, amb->type);
  /* Characters use the bare AmbWord (DisplayAmbiguityQuestion does not
     pluralise the character case). */
  if (amb->type == 'c')
    {
      sb_puts (out, "You can't see any ");
      sb_puts (out, w.c_str ());
      sb_puts (out, "!");
      return;
    }
  /* clsObject.IsPlural == (Article == "some"): a mass/plural object
     ("some firewood") keeps the bare noun; others are pluralised. */
  int any_plural = 0;
  for (auto &k : amb->keys)
    {
      int ki = a5state_object_index (st, k.c_str ());
      if (ki >= 0 && streq (st->adv->objects[ki].article, "some"))
        { any_plural = 1; break; }
    }
  std::string pl = any_plural ? w : guess_plural_from_noun (w);
  sb_puts (out, "You can't see any ");
  sb_puts (out, pl.c_str ());
  sb_puts (out, "!");
}

static int noref_has_output (a5_run_t *run, int ti, int ci,
                             const std::string &in);

/* Scan general tasks for `in`.  Runs (and returns 1) on the first unique passing
   match.  Otherwise returns 0, recording -- for the caller to act on after the
   scan -- the first ambiguity (via have_amb/amb/amb_ti/amb_ci; amb_cantsee
   distinguishes the "can't see any" out-of-scope case from a "which?" prompt)
   and the first restriction-failure message (via have_fail/fail_text). */
static int
scan_tasks (a5_run_t *run, const std::string &in, sb_t *out,
            int *have_amb, amb_info *amb, int *amb_ti, int *amb_ci,
            int *amb_cantsee,
            int *have_fail, std::string *fail_text,
            int *have_noref, int *noref_ti, int *noref_ci)
{
  a5_state_t *st = run->st;
  int fail_priority = 0;
  /* When a <Continue>ContinueAlways task runs, FrankenDrift continues task
     selection via EvaluateInput(Priority+1) -> GetGeneralTask with
     iMinimumPriority = iPriorityFail = that task's Priority + 1.  So on the
     continuation only *strictly higher* priority tasks are eligible, and a
     LowPriority task is skipped unless its priority equals the floor (the
     `Not (LowPriority AndAlso Priority > iPriorityFail)` guard,
     clsUserSession.vb:5981).  E.g. RunBronwynn's GetOffBeforeMoving (50432,
     ContinueAlways) prints "...get off the bed first?" and the LowPriority
     PlayerMovement (50434) is then NOT run, so the player stays put. */
  int cont_active = 0;
  long cont_floor = 0;        /* = the continue task's Priority (min = floor+1) */
  for (size_t oi = 0; oi < run->order->size (); oi++)
    {
      int ti = (*run->order)[oi];
      const a5_task_t *t = &run->adv->tasks[ti];
      if (!streq (t->type, "General"))
        continue;
      if (st->task_done[ti] && !t->repeatable)
        continue;
      if (cont_active)
        {
          if (t->priority <= cont_floor)              /* below iMinimumPriority */
            continue;
          if (t->low_priority && t->priority > cont_floor + 1) /* iPriorityFail */
            continue;
        }

      /* HighestPriorityTask mode: a recorded failing-with-output task claims the
         turn over any *lower*-priority task (higher Priority value).  But an
         *equal*-priority task that passes still overrides it -- frankendrift's
         GetGeneralTask reaches such a passing task first when its (unstable)
         priority sort happens to order it ahead of the failing one (e.g. the
         library "say %text%" pair: the "Say" fallback needs an active
         conversation and fails with a message, while the equal-priority
         "SayLazy" passes when alone with a character and prints "(to <char>)").
         So keep scanning within the same priority band, but stop the moment we
         descend to a strictly lower priority: emit the recorded message. */
      if (*have_fail && !st->adv->hp_passing && t->priority > fail_priority)
        return 0;

      /* HighestPriorityPassingTask: once a task fails *with output*, FD sets
         iPriorityFail to its Priority, so the loop-top guard
         `Not (LowPriority AndAlso Priority > iPriorityFail)`
         (clsUserSession.vb:5981) then skips any LowPriority task above that
         floor.  This is what keeps Six Silver Bullets' turn timer aligned when
         the highest-priority failing task wins (below): a LowPriority failing
         task above the floor must not be recorded (and so claim/tick the turn)
         when FD would have skipped it outright. */
      if (st->adv->hp_passing && *have_fail
          && t->low_priority && t->priority > fail_priority)
        continue;

      for (int ci = 0; ci < t->n_commands; ci++)
        {
          a5_match_t m;
          amb_info this_amb;
          if (!a5parse_match_command (t->commands[ci], in.c_str (), &m))
            continue;
          int r = resolve_refine (run, t, &m, NULL, NULL, &this_amb);
          if (r == RR_NOMATCH)
            continue;
          if (a5run_trace)
            fprintf (stderr, "[match task %s cmd \"%s\" -> %d]\n",
                     t->key, t->commands[ci], r);
          if (r == RR_OK)
            {
              run_general (run, t, &m, out);
              st->task_done[ti] = 1;
              ev_on_task_completed (run, t->key, out);
              /* <Continue>ContinueAlways: run lower-priority matching tasks too
                 (clsUserSession GetGeneralTask -> EvaluateInput(Priority+1)).
                 E.g. GetOffBeforeMoving prints "(getting off X first)" then lets
                 the actual movement task run. */
              if (t->continue_lower)
                {
                  /* EvaluateInput(Priority+1): only strictly-higher-priority
                     tasks remain eligible, with the LowPriority/iPriorityFail
                     guard.  A second ContinueAlways task raises the floor. */
                  if (!cont_active || t->priority > cont_floor)
                    { cont_active = 1; cont_floor = t->priority; }
                  break;          /* stop trying this task's commands; keep scanning */
                }
              return 1;
            }
          if (r == RR_CANTSEE
              && (!*have_amb || (amb->second_chance && !this_amb.second_chance)))
            {
              /* "You can't see any <plural>!" -- an out-of-scope object
                 reference.  Like a "which?" ambiguity (sAmbTask), this does NOT
                 claim the turn: a lower-priority task that uniquely resolves or
                 fails its restrictions *with output* (e.g. ExamineCharacter
                 resolving the same noun to an NPC and reporting "You can't see
                 the woman!") overrides it.  Mirrors GetGeneralTask, where
                 sAmbTask is shown only when no later task claims sTaskKey.  Record
                 the first such reference and keep scanning.  A genuine first-pass
                 ambiguity replaces an earlier *second-chance* one (FD finds all
                 first-pass results before the second-chance pass). */
              *have_amb = 1; *amb_cantsee = 1; *amb = this_amb;
              *amb_ti = ti; *amb_ci = ci;
              continue;
            }
          if (r == RR_NOREF)
            {
              /* A reference named nothing: remember this task as the
                 no-reference fallback (clsUserSession.GetGeneralTask sNoRefTask).
                 Keep scanning -- a later task may resolve uniquely or fail with
                 output; only if nothing claims does the caller run this task
                 with the reference unresolved.

                 These are FD's second-chance pass tasks (matched only via
                 HasObjectExistRestriction, with zero-Item references): there they
                 fail their `Must Exist` restriction *with output*, so the same
                 TaskExecution ordering applies.  Under HighestPriorityPassingTask
                 the highest-priority such task that fails *with output* wins
                 (clsUserSession.vb:6076, gated on `sRestrictionText <> ""`) --
                 run->order is ascending, so overwrite each time a higher task
                 would produce a message; under the default HighestPriorityTask the
                 first (lowest-priority) one is kept.  E.g. Spectre's `remove
                 bricks` matches both RemoveObjects (P50736, "...you're referring
                 to.") and RemoveObje (P50749, "...trying to remove."); FD surfaces
                 the latter.  The output gate keeps Axe of Kolt's `examine
                 <unknown noun>` on its "You see no such thing." rather than
                 yielding to a higher refless task that fails silently. */
              if (!*have_noref)
                { *have_noref = 1; *noref_ti = ti; *noref_ci = ci; }
              else if (st->adv->hp_passing
                       && noref_has_output (run, ti, ci, in))
                { *noref_ti = ti; *noref_ci = ci; }
              continue;
            }
          if (r == RR_AMBIG
              && (!*have_amb || (amb->second_chance && !this_amb.second_chance)))
            { *have_amb = 1; *amb_cantsee = 0; *amb = this_amb;
              *amb_ti = ti; *amb_ci = ci; }
          if (r == RR_FAIL)
            {
              /* A "get all"-style command whose %objects% resolved to no passing
                 item: the task's FailOverride claims the turn ("There is nothing
                 worth taking here.") -- clsUserSession.vb:788, shown when no
                 response passed and the input contained "all". */
              if (run->pending_failover != NULL)
                {
                  char *fo = a5text_describe (st, run->pending_failover);
                  run->pending_failover = NULL;
                  if (msg_has_output (fo)) { sb_pspace (out); sb_puts (out, fo); }
                  free (fo);
                  return 1;
                }
              /* The task matched the command but its restrictions fail.  If a
                 failing restriction has output, this is a candidate fail message
                 (clsUserSession.GetGeneralTask: sRestrictionText = the failing
                 restriction's Message; refless and reference-bearing tasks are
                 treated identically -- htblCompleteableGeneralTasks has no
                 HasReferences/IsCompleteable filter; that lives only in the
                 background-thread predictor).  The TaskExecution mode decides
                 whether it claims the turn now:
                   - HighestPriorityTask (the v5 default): the highest-priority
                     command-matching task that fails *with output* claims the
                     turn -- show its message, no lower task runs.
                   - HighestPriorityPassingTask: it does NOT claim; record the
                     first (highest-priority) such message and keep scanning for
                     a lower-priority *passing* task, which overrides it.  Six
                     Silver Bullets sets this, so its refless "LOOK" peephole
                     task falls through to the real Look. */
              const a5_xml_node_t *fm = a5restr_fail_message (st, t->restrictions);
              if (fm == NULL)
                /* The task's own restrictions named no failing message (e.g. a
                   malformed BracketSequence whose evaluation calls no single
                   restriction).  frankendrift then still has sRestrictionText
                   set to the *previous* command's leftover message, so the task
                   fails *with output* and claims the turn -- ticking
                   TurnBasedStuff.  Anno 1700's reference-free OpeningHid ("##A#")
                   matches "open closet", fails, and inherits the prior turn's
                   "%CharacterName% can't see %TheObject[%object%]%." (rendered
                   with its empty references as "You can't see nothing."). */
                fm = st->restriction_text;
              if (fm != NULL)
                {
                  char *fmsg = a5text_describe (st, fm);
                  if (fmsg[0])
                    {
                      /* Record the first (lowest-Priority, since run->order is
                         ascending) failing-with-output task and keep scanning.
                         Under HighestPriorityPassingTask any later passing task
                         (any priority) overrides it; under the default
                         HighestPriorityTask only an equal-priority passing task
                         does (enforced by the loop-top guard, which stops once a
                         strictly lower priority is reached).

                         Under HighestPriorityPassingTask FD reassigns
                         GetGeneralTask for *every* failing-with-output task
                         (clsUserSession.vb:6076), so the *highest*-priority such
                         task wins -- run->order is ascending, so overwrite each
                         time.  The LowPriority/iPriorityFail guard at the loop top
                         (above) already excludes the LowPriority tasks FD skips
                         once a fail floor is set, which keeps Six Silver Bullets'
                         turn timer aligned (a bare keep-highest without that guard
                         ticked early).  Under the default HighestPriorityTask the
                         loop-top guard stops the scan once a strictly lower
                         priority is reached, so keeping the first within the band
                         is correct there. */
                      if (!*have_fail || st->adv->hp_passing)
                        { *have_fail = 1; *fail_text = fmsg; fail_priority = t->priority; }
                    }
                  free (fmsg);
                }
            }
          /* command matched but did not resolve to a unique pass: keep scanning */
        }
    }
  /* A <Continue>ContinueAlways task already ran and claimed this turn; the
     continuation (FD's EvaluateInput at iMinimumPriority>0) found no further
     task, but unlike a first-pass empty result it must NOT print "I didn't
     understand" (that path is gated on iMinimumPriority=0).  So report the
     turn as handled. */
  if (cont_active)
    return 1;
  return 0;
}

/* --------------------------------------------------------------- public turn */

/* Finalise a turn: take the assembled output and run it through the Display()
   ALR boundary (clsUserSession.Display -> Global.ReplaceALRs), so the engine's
   stock literals are translated for a non-English game.  A no-op (bar a copy)
   when the game has no ALRs. */
/* Append the end-of-game block (clsUserSession.CheckEndOfGame): the win/lose
   banner, the optional EndGameText (WinningText), the score line when MaxScore>0,
   and the restart prompt.  Emitted once, the first turn game_over is set, before
   `turns` is bumped for the ending command -- so the score shows the count of
   commands processed *before* this one (FD displays during Process, ahead of its
   per-command Adventure.Turns increment). */
static void
emit_endgame (a5_run_t *run, sb_t *out)
{
  a5_state_t *st = run->st;
  const char *es = st->end_message;     /* "Win" / "Lose" / "Neutral" */
  long score = 0, maxscore = 0;

  /* FrankenDrift's CheckEndOfGame Displays the banner through pSpace, so a
     paragraph break always separates it from the turn's preceding output.  A
     game that ends via a turn-based event (e.g. StarshipQuest's hyperspace
     death, MagneticMoon's "took too long" timer) emits its death text with a
     single trailing newline -- without this the banner would abut it, whereas a
     command-ending game's last response already ends in a blank line.  Top up
     the trailing newlines to a blank-line separator. */
  if (out->len > 0)
    {
      size_t k = out->len;
      int nl = 0;
      while (k > 0 && (out->p[k - 1] == '\n' || out->p[k - 1] == '\r'
                       || out->p[k - 1] == ' ' || out->p[k - 1] == '\t'))
        {
          if (out->p[k - 1] == '\n') nl++;
          k--;
        }
      if (nl < 2)
        sb_puts (out, nl == 0 ? "\n\n" : "\n");
    }

  if (es != NULL && streq (es, "Win"))
    sb_puts (out, "*** You have won ***\n");
  else if (es != NULL && streq (es, "Lose"))
    sb_puts (out, "*** You have lost ***\n");
  /* Neutral (and an unset/unknown enum): no banner, like FD. */

  if (run->adv->end_game_text != NULL)
    {
      char *wt = a5text_describe (st, run->adv->end_game_text);
      if (wt[0]) { sb_puts (out, wt); sb_puts (out, "\n"); }
      free (wt);
    }

  /* FD reads Adventure.Score / MaxScore = htblVariables("Score"/"MaxScore"),
     which is keyed by the variable KEY, not its (user-facing) Name.  Magnetic
     Moon's score variable has key "Score" but Name "__Points_Scored", so a
     by-Name lookup misses it.  Look up by key (index) to match FD. */
  {
    int svi = a5state_variable_index (st, "Score");
    if (svi >= 0) score = st->var_num[svi];
    int mvi = a5state_variable_index (st, "MaxScore");
    if (mvi >= 0) maxscore = st->var_num[mvi];
  }
  if (maxscore > 0)
    {
      char line[160];
      snprintf (line, sizeof line,
                "In that game you scored %ld out of a possible %ld, in %d turns.\n\n",
                score, maxscore, st->turns - 1);
      sb_puts (out, line);
    }

  sb_puts (out, "Would you like to restart, restore a saved game, quit or "
                "undo the last command?\n\n");
  st->end_displayed = 1;
}

static char *
finish_turn (a5_run_t *run, sb_t *out)
{
  char *raw, *fin;
  size_t n;
  if (run->st->game_over && !run->st->end_displayed)
    emit_endgame (run, out);
  /* Honour any <cls> relayed by the plain renderer (A5_CLS_MARK).  A command
     turn is a single FD commit (its output accumulates in sOutputText and flushes
     once, clsUserSession.vb:3766), so a <cls> anywhere in the turn wipes the whole
     turn's prior output -- e.g. Pathway to Destruction's win narrative, whose
     embedded <cls> drops the move message, room view and "The End" banner.  (The
     multi-commit intro resolves its own segments before calling finish_turn, so
     no markers reach here from a5run_intro.) */
  sb_resolve_cls (out, 0);
  raw = sb_take (out);
  fin = a5text_display_alr (run->st, raw);   /* may render ALR <img>/<audio> too */
  free (raw);
  /* Normalise only the very end of the turn: FD's pSpace model leaves a message
     ending in trailing spaces or a paragraph break, and FD then appends its own
     end-of-turn vbCrLf pair.  The interior pSpace joins are what matter for
     conformance; the turn's tail is cosmetic (the diff harness collapses it).
     Trim trailing whitespace/newlines and re-add a single '\n' so each turn ends
     the same shape it always did -- the dump harness supplies the blank line. */
  n = strlen (fin);
  while (n > 0 && (fin[n - 1] == '\n' || fin[n - 1] == '\r'
                   || fin[n - 1] == ' ' || fin[n - 1] == '\t'))
    n--;
  if (n > 0)
    {
      fin = (char *) realloc (fin, n + 2);
      fin[n] = '\n';
      fin[n + 1] = '\0';
    }
  else if (fin != NULL)
    fin[0] = '\0';
  a5run_media_end ();
  return fin;
}

/* Run the System tasks flagged <RunImmediately> once at game start, in file
   order, exactly as clsUserSession's init loop does (vb:209-216) -- before the
   title is shown.  Their completion-message output (e.g. a title-music task's
   audio markup + trailing space) accumulates in the same buffer so it joins onto
   the title via pSpace.  Events are not yet initialised at this point (FD starts
   them after the intro), so no event-completion hooks fire here. */
static void
run_immediate_tasks (a5_run_t *run, sb_t *out)
{
  a5_state_t *st = run->st;
  int i;
  run->immediate_emit = 1;
  for (i = 0; i < run->adv->n_tasks; i++)
    {
      const a5_task_t *t = &run->adv->tasks[i];
      int ti;
      if (!t->run_immediately || !streq (t->type, "System"))
        continue;
      ti = a5state_task_index (st, t->key);
      if (ti >= 0 && st->task_done[ti] && !t->repeatable)
        continue;
      if (!a5restr_pass (st, t->restrictions))
        continue;
      run_task (run, t, 0, out);
    }
  run->immediate_emit = 0;
}

char *
a5run_intro (a5_run_t *run)
{
  sb_t out;
  char *intro, *look;
  /* FD renders the intro as several separate commits (Adventure-Upgrade prompt,
     the RunImmediately output + title at vb:226, the Introduction at vb:227, the
     room view at vb:229), so a <cls> in one unit clears only that unit's buffer
     -- e.g. an Introduction that opens with <cls> must NOT wipe the already-
     committed title (Achtung Panzer).  Resolve each commit unit's markers before
     the next is appended; `cf` is the current unit's start offset. */
  size_t cf = 0;
  sb_init (&out);
  a5run_media_begin (run);
  /*
   * "Adventure Upgrade" auto-correct prompt (FileIO.vb:634).  When the file
   * format version is older than 5.0.26 and any task carries an "#A#O#"
   * (AND-then-OR) BracketSequence, FD asks once at load whether to auto-correct
   * those tasks.  Our headless ground truth (FrankenDrift.Headless) answers the
   * question only when the next script line is literally yes/no; RtC's first
   * script line is a real command, so FD answers NO -- the correction is NOT
   * applied (and Scarier, like FD-no, reads the BracketSequence verbatim, so the
   * restriction logic already matches).  But FD still EMITS the question prose
   * before the intro, so we must too.  We never apply CorrectBracketSequence
   * (matching FD's NO answer for this corpus).
   */
  if (run->adv->version != NULL && atof (run->adv->version) < 5.000026)
    {
      int ti;
      for (ti = 0; ti < run->adv->n_tasks; ti++)
        {
          const a5_xml_node_t *r = run->adv->tasks[ti].restrictions;
          const char *bs = r ? a5xml_child_text (r, "BracketSequence") : NULL;
          if (bs != NULL && strstr (bs, "#A#O#") != NULL)
            {
              sb_puts (&out,
                "Adventure Upgrade\n"
                "There was a logic correction in version 5.0.26 which means OR "
                "restrictions after AND restrictions were not evaluated.  Would "
                "you like to auto-correct these tasks?\n\n"
                "You may not wish to do so if you have already used brackets "
                "around any OR restrictions following AND restrictions.\n\n");
              break;
            }
        }
    }
  /* System <RunImmediately> tasks run before the title (FD clsUserSession init,
     vb:209-216); a title-music task's audio markup leaves a trailing space that
     joins onto the centred title. */
  sb_resolve_cls (&out, cf); cf = out.len;   /* commit: Adventure-Upgrade prompt */
  run_immediate_tasks (run, &out);
  /* The centred title: FD's Display("<c>" & Adventure.Title & "</c>" & vbCrLf)
     at vb:226, emitted through the same buffer so the RunImmediately output above
     joins onto it via pSpace.  An empty title emits nothing (FD's "<c></c>"
     renders to a bare blank that the turn-tail normalisation drops); a non-empty
     one space-joins to any preceding RunImmediately output, then the intro's own
     leading break follows. */
  if (run->adv->title != NULL && run->adv->title[0] != '\0')
    { sb_pspace (&out); sb_puts (&out, run->adv->title); sb_puts (&out, "\n"); }
  sb_resolve_cls (&out, cf); cf = out.len;   /* commit: RunImmediately + title */
  intro = a5text_describe (run->st, run->adv->introduction);
  /* No paragraph break after an intro whose last visible act is a <cls> (DDF's
     multi-page intro ends "...<waitkey><cls>"): the break would land AFTER the
     wipe marker and survive the resolve below as two stray blank lines at the
     top of the cleared screen, which FD never shows. */
  if (intro[0])
    {
      sb_puts (&out, intro);
      if (intro[strlen (intro) - 1] != A5_CLS_MARK)
        sb_puts (&out, "\n\n");
    }
  free (intro);
  sb_resolve_cls (&out, cf); cf = out.len;   /* commit: Introduction (vb:227) */
  /* Show the start room only when <ShowFirstLocation> is set (the default);
     games like Six Silver Bullets clear it and reveal the room via a LOOK or
     the first move (clsUserSession game-start: gated on Adventure.ShowFirstRoom). */
  if (run->adv->show_first_location)
    {
      look = a5text_view_location (run->st);
      sb_puts (&out, look);
      free (look);
    }
  sb_resolve_cls (&out, cf); cf = out.len;   /* commit: room view (vb:229) */
  /* Start the Immediately events (clsUserSession init loop, after intro). */
  ev_init (run, &out);
  sb_resolve_cls (&out, cf); cf = out.len;   /* commit: start-of-game events */
  update_seen (run->st);
  return finish_turn (run, &out);
}

/* Seed the known-words list exactly as clsUserSession.NotUnderstood does, lazily
   (the first time an unmatched command needs the word-validity check).

   FAITHFUL QUIRK: FD stores command verbs, object/character articles, prefixes,
   names and descriptors in listKnownWords with their ORIGINAL case (only the
   character ProperName is `.ToLower`-ed; vb:3489-3531), and the validity check
   compares the *lowercased* input word case-sensitively (`List.Contains`).  So a
   capitalised model word like the object name "Crystal" or prefix "Zykon" is NOT
   a known word for the lowercase input "crystal"/"zykon" -- e.g. Revenge's
   `insert crystal in bracket` reports `I did not understand the word "crystal".`
   (the object is named "Crystal").  We preserve case here to reproduce it; the
   lookup in not_understood lowercases the input word, mirroring FD's lowercase
   sInput vs mixed-case known words. */
static void
build_known_words (a5_run_t *run)
{
  const a5_adventure_t *adv = run->adv;
  std::set<std::string> *kw = new std::set<std::string>;
  int i, j;

  /* Verbs: every word of every General task command, with the pattern
     punctuation ([]{}/) blanked to spaces (NotUnderstood does the same). */
  for (i = 0; i < adv->n_tasks; i++)
    {
      if (!streq (adv->tasks[i].type, "General"))
        continue;
      for (j = 0; j < adv->tasks[i].n_commands; j++)
        {
          std::string c = adv->tasks[i].commands[j];
          for (char &ch : c)
            if (ch == '[' || ch == ']' || ch == '{' || ch == '}' || ch == '/')
              ch = ' ';
          for (auto &w : split_ws (c.c_str ()))
            kw->insert (w);
        }
    }
  /* Nouns: each object's article + prefix words + names (original case). */
  for (i = 0; i < adv->n_objects; i++)
    {
      const a5_object_t *o = &adv->objects[i];
      if (o->article && o->article[0]) kw->insert (o->article);
      for (auto &w : split_ws (o->prefix)) kw->insert (w);
      for (j = 0; j < o->n_names; j++)
        for (auto &w : split_ws (o->names[j])) kw->insert (w);
    }
  /* Characters: article + prefix words + descriptors (original case) + proper
     name (the one field FD lowercases, vb:3522). */
  for (i = 0; i < adv->n_characters; i++)
    {
      const a5_character_t *c = &adv->characters[i];
      if (c->article && c->article[0]) kw->insert (c->article);
      for (auto &w : split_ws (c->prefix)) kw->insert (w);
      for (j = 0; j < c->n_descriptors; j++)
        for (auto &w : split_ws (c->descriptors[j])) kw->insert (w);
      if (c->name && c->name[0]) kw->insert (lower (c->name));
    }
  /* Adverbs: every direction word (sDirectionsRE split on "|"). */
  {
    std::string dr = a5parse_directions_re (), cur;
    for (size_t k = 0; k <= dr.size (); k++)
      {
        if (k == dr.size () || dr[k] == '|')
          { if (!cur.empty ()) kw->insert (cur); cur.clear (); }
        else
          cur += dr[k];
      }
  }
  kw->insert ("and");
  run->known_words = kw;
}

/* clsUserSession.SystemTasks: the built-in verbs handled outside the task
   engine, tried after task matching fails but before NotUnderstood.  Only the
   one this corpus exercises is implemented -- "wait"/"z" prints "Time passes..."
   and advances WaitTurns turns of TurnBasedStuff.  Returns 1 if handled. */
static int
system_command (a5_run_t *run, const std::string &in, sb_t *out)
{
  if (in == "wait" || in == "z")
    {
      int n = run->adv->wait_turns;
      sb_puts (out, "Time passes...");
      for (int i = 0; i < n; i++)
        ev_tick_all (run, out);
      return 1;
    }
  return 0;
}

/* The clsUserSession.NotUnderstood ladder, reached when no task ran, no task
   failed with output, and there is no pending disambiguation: first reject the
   earliest unrecognised input word ("I did not understand the word ..."), then
   fall back to the adventure's catch-all message. */
static void
not_understood (a5_run_t *run, const std::string &in, sb_t *out)
{
  if (run->known_words == NULL)
    build_known_words (run);

  for (auto &w : split_ws (in.c_str ()))
    if (run->known_words->find (lower (w)) == run->known_words->end ())
      {
        sb_puts (out, "I did not understand the word \"");
        sb_puts (out, w.c_str ());
        sb_puts (out, "\".");
        return;
      }

  /* The user entered just a verb that matches a "verb %ref%" command: prompt for
     the missing reference ("<Verb> what?/who?") and remember the verb so the next
     input is re-tried as "<verb> <input>" (NotUnderstood, vb:3543-3581).  Only a
     single-word input.

     FAITHFUL QUIRK: FrankenDrift (and the original Runner) test the *normalised*
     command, where every bare singular reference has been suffixed with "1"
     (%object% -> %object1%, %character% -> %character1%, %direction% ->
     %direction1%; FileIO.vb:647).  The object/character branch checks for the
     substring "%object"/"%character" (no trailing %), which still matches
     %object1%/%character1%; but the direction branch checks for "%direction%"
     WITH the trailing %, which no longer matches %direction1%.  So the "where?"
     prompt is effectively dead code in the Runner -- a bare movement verb falls
     through to the catch-all.  We reproduce that exactly by normalising the
     command before the same three Contains checks. */
  if (in.find (' ') == std::string::npos)
    {
      for (size_t oi = 0; oi < run->order->size (); oi++)
        {
          const a5_task_t *t = &run->adv->tasks[(*run->order)[oi]];
          if (!streq (t->type, "General") || t->n_commands == 0)
            continue;
          const char *cmd0 = t->commands[0];
          /* arlCommands(0) must contain the verb and be a multi-word pattern. */
          if (strstr (cmd0, in.c_str ()) == NULL || strchr (cmd0, ' ') == NULL)
            continue;
          std::string norm = cmd0;          /* normalise bare singular refs */
          static const char *bare[] = { "%object%", "%character%", "%direction%",
                                        "%number%", "%text%" };
          static const char *num[]  = { "%object1%", "%character1%", "%direction1%",
                                        "%number1%", "%text1%" };
          for (int b = 0; b < 5; b++)
            { size_t p; const std::string from = bare[b], to = num[b];
              while ((p = norm.find (from)) != std::string::npos)
                norm.replace (p, from.size (), to); }
          const char *q = NULL;
          if (norm.find ("%object") != std::string::npos)
            q = "what?";
          else if (norm.find ("%character") != std::string::npos)
            q = "who?";
          else if (norm.find ("%direction%") != std::string::npos)
            q = "where?";             /* never true: %direction1%, see above */
          else
            continue;
          /* Confirm a command pattern of this task actually matches verb+dummy. */
          int hit = 0;
          for (int ci = 0; ci < t->n_commands && !hit; ci++)
            { a5_match_t m;
              if (a5parse_match_command (t->commands[ci],
                                         (in + " sdkfjdslkj").c_str (), &m))
                hit = 1; }
          if (!hit)
            continue;
          run->remembered_verb = in;
          {
            std::string vp = in;            /* PCase(verb) */
            if (!vp.empty ()) vp[0] = (char) toupper ((unsigned char) vp[0]);
            sb_puts (out, vp.c_str ());
            sb_puts (out, " ");
            sb_puts (out, q);
          }
          return;
        }
    }

  /* The input names an object the player has seen and can see now, but no task
     accepted it: "I don't understand what you want to do with the X."
     (NotUnderstood's seen-noun branch, vb:3584-3609).  First seen+visible match
     wins, objects before characters (FD's loop order).

     FAITHFUL QUIRK: FD matches the entity's sRegularExpressionString *unanchored*
     against the whole input (`re.IsMatch(sInput)`, no `^...$`), so a noun
     alternative need only appear as a SUBSTRING -- FD's player descriptor "me"
     matches inside "hel<me>t", so `put helmet ... ` reports "...do with Mike
     Erlin.".  Since the regex's article/prefix groups are all optional, IsMatch
     reduces to: does any noun alternative occur as a substring of the lowercased
     input?  The noun set is the object's names, or the character's descriptors
     plus its proper name when usable -- the proper name as a WHOLE string ("mike
     erlin" must appear contiguously, unlike the word-split set used elsewhere),
     so `say erlin ...` does NOT match the player and falls through to
     NotUnderstood, matching FD. */
  {
    a5_state_t *st = run->st;
    std::string lin = lower (in);
    auto noun_substr = [&] (const std::vector<std::string> &nouns) {
      for (auto &n : nouns)
        if (!n.empty () && lin.find (n) != std::string::npos)
          return 1;
      return 0;
    };
    for (int oi = 0; oi < st->adv->n_objects; oi++)
      {
        const a5_object_t *o = &st->adv->objects[oi];
        if (st->obj_seen == NULL || !st->obj_seen[oi]
            || !obj_in_scope (st, o->key))
          continue;
        std::vector<std::string> nouns;
        for (int n = 0; n < o->n_names; n++)
          nouns.push_back (lower (o->names[n]));
        /* FD's `If arl.Count = 0 Then sRE &= "|"` fudge: a nameless object's
           empty alternation matches anywhere -> always IsMatch. */
        int hit = (o->n_names == 0) ? 1 : noun_substr (nouns);
        if (hit)
          {
            char *nm = a5text_object_name (o, A5_ART_DEFINITE);
            sb_puts (out, "I don't understand what you want to do with ");
            sb_puts (out, nm);
            sb_puts (out, ".");
            free (nm);
            return;
          }
      }

    /* Likewise a seen+visible character: "...what you want to do with <Name>."
       (NotUnderstood's character branch, vb:3597). */
    for (int ci = 0; ci < st->adv->n_characters; ci++)
      {
        const a5_character_t *c = &st->adv->characters[ci];
        if (!char_seen_p (st, c->key) || !char_visible (st, c->key))
          continue;
        std::vector<std::string> nouns;
        for (int d = 0; d < c->n_descriptors; d++)
          nouns.push_back (lower (c->descriptors[d]));
        if (char_name_usable (st, c) && c->name != NULL)
          nouns.push_back (lower (c->name));
        if (noun_substr (nouns))
          {
            char *nm = a5text_character_known_name (st, c, 0);
            sb_puts (out, "I don't understand what you want to do with ");
            sb_puts (out, nm);
            sb_puts (out, ".");
            free (nm);
            return;
          }
      }
  }

  /* All words known but nothing matched: Adventure.NotUnderstood (FileIO.vb:850
     defaults a missing <NotUnderstood> to this string). */
  sb_puts (out, "Sorry, I didn't understand that command.");
}

/* Would run_noref produce a non-empty message for this task?  Mirrors FD's
   `sRestrictionText <> ""` gate: a second-chance Must-Exist task only updates
   GetGeneralTask when its failing restriction has output.  Used to decide
   whether a higher-priority no-reference task may override a recorded one under
   HighestPriorityPassingTask (so an examine task whose noun is unknown keeps its
   "You see no such thing." rather than yielding to a higher refless task that
   fails silently and drops the turn to NotUnderstood). */
static int
noref_has_output (a5_run_t *run, int ti, int ci, const std::string &in)
{
  a5_state_t *st = run->st;
  const a5_task_t *t = &run->adv->tasks[ti];
  a5_match_t m;
  if (!a5parse_match_command (t->commands[ci], in.c_str (), &m))
    return 0;
  resolve_refine (run, t, &m, NULL, NULL, NULL);
  const a5_xml_node_t *fm = a5restr_fail_message (st, t->restrictions);
  if (fm == NULL)
    return 0;
  char *fmsg = a5text_describe (st, fm);
  int has = fmsg[0] != '\0';
  free (fmsg);
  return has;
}

/* Run the deferred sNoRefTask (clsUserSession.GetGeneralTask, GetGeneralTask =
   sNoRefTask): re-bind the resolvable references and surface the task's
   restriction message for the unresolved one ("Sorry, I'm not sure which object
   you are trying to <verb>.").  Returns 1 if it produced output. */
static int
run_noref (a5_run_t *run, int ti, int ci, const std::string &in, sb_t *out)
{
  a5_state_t *st = run->st;
  const a5_task_t *t = &run->adv->tasks[ti];
  a5_match_t m;
  if (!a5parse_match_command (t->commands[ci], in.c_str (), &m))
    return 0;
  resolve_refine (run, t, &m, NULL, NULL, NULL);   /* binds the resolvable refs */
  const a5_xml_node_t *fm = a5restr_fail_message (st, t->restrictions);
  if (fm == NULL)
    return 0;
  char *fmsg = a5text_describe (st, fm);
  int has = fmsg[0] != '\0';
  if (has)
    sb_puts (out, fmsg);
  free (fmsg);
  return has;
}

/* clsUserSession.ReplaceWord: replace whole-word occurrences of `find` in `text`
   with `repl` (word boundaries are spaces / string ends; case-sensitive on the
   already-lowercased input, exactly as FD). */
static void
replace_word (std::string &text, const std::string &find, const std::string &repl)
{
  /* " find " -> " repl " (interior) */
  std::string from = " " + find + " ", to = " " + repl + " ";
  size_t pos = 0;
  while ((pos = text.find (from, pos)) != std::string::npos)
    { text.replace (pos, from.size (), to); pos += to.size (); }
  /* leading / trailing / whole */
  if (text.size () > find.size () + 1
      && text.compare (0, find.size () + 1, find + " ") == 0)
    text = repl + " " + text.substr (find.size () + 1);
  if (text.size () > find.size () + 1
      && text.compare (text.size () - find.size () - 1, find.size () + 1, " " + find) == 0)
    text = text.substr (0, text.size () - find.size () - 1) + " " + repl;
  if (text == find)
    text = repl;
}

/*
 * clsUserSession.GrabIt: after the player's command words are known, work out
 * what "it"/"them"/"him"/"her" should refer to *next* turn.  Scan the input
 * words against in-scope (visible, then seen) objects' names and characters'
 * proper-name/descriptors; an unambiguous single match in a pronoun class sets
 * that class's stored display name.  Objects use the definite full name,
 * characters their (definite) known name.  "them" considers only plural objects;
 * characters split into him/her/it by Gender.  A >1 match is narrowed by a
 * prefix word appearing in the input (FD's second pass); still-ambiguous classes
 * keep the previous value.
 */
static void
grab_it (a5_run_t *run, const std::string &in)
{
  a5_state_t *st = run->st;
  std::vector<std::string> words = split_ws (lower (in).c_str ());
  std::string new_it, new_them, new_him, new_her;
  std::vector<const char *> it_keys, them_keys, him_keys, her_keys;
  int scope;

  auto has_word = [&] (const std::string &w) {
    for (auto &x : words) if (x == w) return 1;
    return 0;
  };
  auto add_uniq = [] (std::vector<const char *> &v, const char *k) {
    for (auto *e : v) if (streq (e, k)) return;
    v.push_back (k);
  };

  /* Visible first, then seen (eScope.Visible To eScope.Seen). */
  for (scope = 0; scope < 2; scope++)
    {
      int oi, ci;
      for (oi = 0; oi < st->adv->n_objects; oi++)
        {
          const a5_object_t *o = &st->adv->objects[oi];
          int inscope = (scope == 0) ? obj_visible (st, o->key)
                                     : (!obj_visible (st, o->key) && obj_seen_p (st, o->key));
          if (!inscope)
            continue;
          int is_plural = (o->article != NULL && lower (o->article) == "some");
          for (int n = 0; n < o->n_names; n++)
            if (has_word (lower (o->names[n])))
              {
                if (new_it.empty ()) add_uniq (it_keys, o->key);
                if (new_them.empty () && is_plural) add_uniq (them_keys, o->key);
              }
        }
      for (ci = 0; ci < st->adv->n_characters; ci++)
        {
          const a5_character_t *c = &st->adv->characters[ci];
          int inscope = (scope == 0) ? char_visible (st, c->key)
                                     : (!char_visible (st, c->key) && char_seen_p (st, c->key));
          if (!inscope)
            continue;
          int matched = 0;
          if (c->name != NULL && has_word (lower (c->name)))
            matched = 1;
          for (int d = 0; !matched && d < c->n_descriptors; d++)
            if (has_word (lower (c->descriptors[d])))
              matched = 1;
          if (!matched)
            continue;
          const char *g = a5state_entity_prop (st, c->key, "Gender");
          if (g != NULL && lower (g) == "male")
            add_uniq (him_keys, c->key);
          else if (g != NULL && lower (g) == "female")
            add_uniq (her_keys, c->key);
          else
            add_uniq (it_keys, c->key);
        }

      /* Resolve each class that is now unambiguous (or narrowable by prefix). */
      struct { std::string *out; std::vector<const char *> *keys; int definite; } cls[] = {
        { &new_it,   &it_keys,   1 },
        { &new_them, &them_keys, 1 },
        { &new_him,  &him_keys,  1 },
        { &new_her,  &her_keys,  1 },
      };
      for (auto &cl : cls)
        {
          if (!cl.out->empty ())
            continue;
          std::vector<const char *> *ks = cl.keys;
          const char *chosen = NULL;
          if (ks->size () == 1)
            chosen = (*ks)[0];
          else if (ks->size () > 1)
            {
              /* Second pass: keep candidates whose prefix word is in the input. */
              const char *only = NULL;
              int n = 0;
              for (auto *k : *ks)
                {
                  const a5_object_t *o = a5model_object (st->adv, k);
                  const a5_character_t *c = a5model_character (st->adv, k);
                  const char *prefix = o ? o->prefix : (c ? c->prefix : NULL);
                  int hit = 0;
                  for (auto &pw : split_ws (prefix))
                    if (has_word (lower (pw))) { hit = 1; break; }
                  if (hit) { only = k; n++; }
                }
              if (n == 1)
                chosen = only;
            }
          if (chosen != NULL)
            {
              const a5_object_t *o = a5model_object (st->adv, chosen);
              const a5_character_t *c = a5model_character (st->adv, chosen);
              char *nm = o ? a5text_object_name (o, A5_ART_DEFINITE)
                           : (c ? a5text_character_known_name (st, c, cl.definite)
                                : strdup (chosen));
              *cl.out = nm ? nm : "";
              free (nm);
            }
        }
    }

  if (!new_it.empty ())   { free (st->s_it);   st->s_it = strdup (new_it.c_str ()); }
  if (!new_them.empty ()) { free (st->s_them); st->s_them = strdup (new_them.c_str ()); }
  if (!new_him.empty ())  { free (st->s_him);  st->s_him = strdup (new_him.c_str ()); }
  if (!new_her.empty ())  { free (st->s_her);  st->s_her = strdup (new_her.c_str ()); }

  /* Defaults so a later "it"/etc. with no referent prints FD's placeholder. */
  if (st->s_it[0]   == '\0') { free (st->s_it);   st->s_it   = strdup ("Absolutely Nothing"); }
  if (st->s_them[0] == '\0') { free (st->s_them); st->s_them = strdup ("Absolutely Nothing"); }
  if (st->s_him[0]  == '\0') { free (st->s_him);  st->s_him  = strdup ("No male"); }
  if (st->s_her[0]  == '\0') { free (st->s_her);  st->s_her  = strdup ("No female"); }
}

/* Substitute "it"/"them"/"him"/"her" in the player's input with the last
   referenced object/character name, emitting FD's "(name)" echo line, then
   recompute the referents for next turn (clsUserSession.EvaluateInput). */
static void
resolve_pronouns (a5_run_t *run, std::string &in, sb_t *out)
{
  a5_state_t *st = run->st;
  struct { const char *word; char *val; } pr[] = {
    { "it",   st->s_it },
    { "them", st->s_them },
    { "him",  st->s_him },
    { "her",  st->s_her },
  };
  for (auto &p : pr)
    if (conv_contains_word (in, p.word) && p.val != NULL && p.val[0])
      {
        sb_puts (out, "(");
        sb_puts (out, p.val);
        sb_puts (out, ")\n");
        replace_word (in, p.word, lower (p.val));
      }
  grab_it (run, in);
}

/* <Synonym> word/phrase substitution (clsUserSession.EvaluateInput's synonym
   loop, run right after GrabIt): for each synonym, for each of its <From>
   phrases, if every word of the phrase appears in the input (ContainsWord),
   replace the phrase with its <To> word (ReplaceWord).  Later synonyms see
   earlier synonyms' replacements, exactly as FD's single `sInput` threaded
   through the loop. */
static void
resolve_synonyms (a5_run_t *run, std::string &in)
{
  const a5_adventure_t *adv = run->adv;
  int i, j;
  for (i = 0; i < adv->n_synonyms; i++)
    {
      const a5_synonym_t *syn = &adv->synonyms[i];
      if (syn->to == NULL)
        continue;
      for (j = 0; j < syn->n_from; j++)
        {
          if (syn->from[j] == NULL)
            continue;
          std::string from = lower (syn->from[j]);
          if (conv_contains_word (in, from))
            replace_word (in, from, lower (syn->to));
        }
    }
}

char *
a5run_input (a5_run_t *run, const char *line)
{
  a5_state_t *st = run->st;
  sb_t out;
  std::string in = lower (line ? line : "");
  std::string fail_text;
  int have_fail = 0, have_amb = 0, amb_ti = -1, amb_ci = -1, amb_cantsee = 0;
  int have_noref = 0, noref_ti = -1, noref_ci = -1;
  amb_info amb;

  sb_init (&out);
  a5run_media_begin (run);
  /* trim */
  {
    size_t a = in.find_first_not_of (" \t");
    size_t b = in.find_last_not_of (" \t");
    in = (a == std::string::npos) ? "" : in.substr (a, b - a + 1);
  }
  if (a5run_trace)
    fprintf (stderr, "\n=== INPUT: \"%s\" ===\n", in.c_str ());
  if (in.empty ())
    return finish_turn (run, &out);

  /* This command claims a turn (clsAdventure.Turns, bumped once per processed
     command by the Runner).  The end-of-game score line shows the count *before*
     this command, so emit_endgame reads turns-1; bump here, after the blank-line
     skip, so empty input does not count.  A remembered-verb retry decrements
     first (below) to keep one user command == one turn. */
  st->turns++;

  /* Consume any remembered verb for this turn (sRememberedVerb): captured here so
     a turn that does anything naturally clears it (clsUserSession clears it on a
     successful task); a system command re-sets it (FD keeps it across SystemTasks)
     and the NotUnderstood fallback re-tries the input as "<verb> <input>". */
  std::string rverb = run->remembered_verb;
  run->remembered_verb.clear ();

  /* Refresh the player's "seen" set from where things ended up last turn
     (clsUserSession.PrepareForNextTurn), so HaveSeenCharacter / "characters
     must be seen" gates reflect everyone visible by now. */
  update_seen (st);

  /* Resolve "it"/"them"/"him"/"her" to the last-referenced entity (echoing the
     "(name)" line) and recompute the referents for next turn, before the input
     is parsed -- clsUserSession.EvaluateInput.  Skipped while resolving a pending
     disambiguation clarifier, which is not a fresh command. */
  if (!run->amb_active)
    {
      resolve_pronouns (run, in, &out);
      resolve_synonyms (run, in);
    }

  /* Pending disambiguation (clsUserSession.ResolveAmbiguity): try `in` as a
     clarifier first -- if it narrows the remembered candidates to exactly one,
     re-run the remembered task with that pick.  Otherwise fall through and let
     `in` be parsed as a fresh command; only if nothing runs do we re-prompt. */
  int resolving_amb = 0;
  long amb_narrowed = -1;
  if (run->amb_active)
    {
      std::vector<std::string> narrowed =
        possible_keys (st, run->amb_keys, in, run->amb_ref_type);
      if (narrowed.size () == 1
          && run_remembered (run, narrowed[0].c_str (), &out))
        { run->amb_active = 0; ev_tick_all (run, &out); return finish_turn (run, &out); }
      /* Not resolved to one.  FD keeps sAmbTask set while trying the new input as
         a fresh command (GetGeneralTask sets sAmbTask only when it is Nothing,
         and the second-chance noref pass runs only when sAmbTask Is Nothing), so
         a fresh ambiguity / noref CANNOT override the remembered ambiguity --
         only a passing or failing-with-output task can claim the turn.  After
         that, DisplayAmbiguityQuestion re-runs on the remembered reference now
         narrowed by PossibleKeys: Count 0 -> "Sorry, that does not clarify the
         ambiguity."; Count >1 -> "Which X?". */
      resolving_amb = 1;
      amb_narrowed = (long) narrowed.size ();
      if (narrowed.size () > 1)            /* a partial narrowing: keep progress */
        run->amb_keys = narrowed;
    }

  if (scan_tasks (run, in, &out, &have_amb, &amb, &amb_ti, &amb_ci,
                  &amb_cantsee, &have_fail, &fail_text,
                  &have_noref, &noref_ti, &noref_ci))
    { run->amb_active = 0; ev_tick_all (run, &out); return finish_turn (run, &out); }

  if (have_fail)
    {
      /* A HighestPriorityPassingTask game: no task passed, so show the
         highest-priority failing task's recorded message.  In frankendrift this
         sets GetGeneralTask (non-Nothing), so it preempts the sNoRefTask and the
         ambiguity display.  It is non-empty output, so the turn advances. */
      run->amb_active = 0;
      sb_puts (&out, fail_text.c_str ());
      ev_tick_all (run, &out);
      return finish_turn (run, &out);
    }

  if (resolving_amb)
    {
      /* The new input neither resolved the ambiguity nor ran a fresh task.  Show
         the remembered ambiguity's DisplayAmbiguityQuestion result (the fresh
         command's own ambiguity/noref is suppressed, as FD leaves sAmbTask set
         throughout resolution).  A clarifier matching none of the candidates
         narrows the reference to zero -> "Sorry, that does not clarify the
         ambiguity." (clsUserSession.vb:2780); a still-ambiguous clarifier
         (Count >1) re-asks "Which X?". */
      if (amb_narrowed == 0)
        {
          run->amb_active = 0;
          sb_puts (&out, "Sorry, that does not clarify the ambiguity.\n");
        }
      else
        sb_puts (&out, build_amb_prompt (st, run->amb_word, run->amb_keys,
                                         run->amb_ref_type).c_str ());
      return finish_turn (run, &out);
    }

  if (have_amb && amb_cantsee && !amb.second_chance)
    {
      /* No later task claimed the turn: emit the deferred "You can't see any
         <plural>!" (the out-of-scope reference is a dead end, not a pending
         prompt).  This does NOT tick TurnBasedStuff: in FrankenDrift an
         unresolvable reference leaves GetGeneralTask returning Nothing, so
         ProcessTurn takes the `sTaskKey Is Nothing` branch (clsUserSession.vb
         :3436) which shows the message but never calls TurnBasedStuff -- unlike
         a failing-but-matched task (the noref/have_fail paths), which does tick.
         Ticking here drifts turn-based event countdowns early (StarshipQuest's
         hyperspace timer fired ~8 turns too soon).

         A cantsee preempts the sNoRefTask below: in FD a cantsee/ambiguity is a
         *first-pass* result (the reference name-matched >1 objects) that sets
         sAmbTask, whereas the exist-restriction sNoRefTask is only found in the
         *second-chance* pass -- and that pass runs solely when sAmbTask Is
         Nothing (GetGeneralTask, clsUserSession.vb:5965 recursion guard).  So a
         command like "get flashlight from backpack", where TakeObjectsFromOthers
         (%objects% from %object2%) name-resolves "flashlight" to several unseen
         objects (cantsee) but GetObjectC (%objects%) swallows the whole
         "flashlight from backpack" as one unmatched reference (a Must-Exist
         noref), must show "You can't see any flashlights!" -- not the noref's
         "I'm not sure which object...".  Likewise for a visible ambiguity below. */
      run->amb_active = 0;
      emit_cantsee (st, &amb, &out);
      return finish_turn (run, &out);
    }

  if (have_amb && !amb.second_chance)
    {
      /* Remember the ambiguity and prompt; the next turn resolves it.  Like the
         cantsee above, a first-pass ambiguity preempts the second-chance
         sNoRefTask below (FD enters the exist pass only when sAmbTask Is
         Nothing). */
      run->amb_active = 1;
      run->amb_task_index = amb_ti;
      run->amb_command_index = amb_ci;
      run->amb_input = in;
      run->amb_ref_name = amb.ref_name;
      run->amb_ref_type = amb.type;
      run->amb_word = amb_word (st, amb.keys, amb.ref_text, amb.type);
      run->amb_keys = amb.keys;
      sb_puts (&out,
               build_amb_prompt (st, run->amb_word, amb.keys, amb.type).c_str ());
      return finish_turn (run, &out);
    }

  if (have_noref && run_noref (run, noref_ti, noref_ci, in, &out))
    {
      /* sNoRefTask: a command matched but a reference named nothing.  Running
         the task with the reference unresolved surfaced its `Must Exist`
         message.  Reached only when no task produced a cantsee/ambiguity above
         -- in FD the sNoRefTask is the result of the second-chance (existence)
         pass, which GetGeneralTask runs only if the first pass left sAmbTask
         Nothing (clsUserSession.vb:5965).  It does claim the turn and ticks. */
      run->amb_active = 0;
      ev_tick_all (run, &out);
      return finish_turn (run, &out);
    }

  if (have_amb)
    {
      /* A *second-chance* ambiguity: the task it came from matched only because
         it also had an unmatched required reference, so FD found it in the
         second-chance pass (sAmbTask).  It loses to a clean sNoRefTask above;
         only when none claimed the turn does it surface here (FD shows sAmbTask
         when GetGeneralTask is Nothing).  Same cantsee-vs-prompt split as the
         first-pass case above. */
      if (amb_cantsee)
        {
          run->amb_active = 0;
          emit_cantsee (st, &amb, &out);
          return finish_turn (run, &out);
        }
      run->amb_active = 1;
      run->amb_task_index = amb_ti;
      run->amb_command_index = amb_ci;
      run->amb_input = in;
      run->amb_ref_name = amb.ref_name;
      run->amb_ref_type = amb.type;
      run->amb_word = amb_word (st, amb.keys, amb.ref_text, amb.type);
      run->amb_keys = amb.keys;
      sb_puts (&out,
               build_amb_prompt (st, run->amb_word, amb.keys, amb.type).c_str ());
      return finish_turn (run, &out);
    }

  if (run->amb_active)
    {
      /* A clarifier that neither resolved nor ran a fresh command: re-prompt. */
      sb_puts (&out, build_amb_prompt (st, run->amb_word, run->amb_keys,
                                       run->amb_ref_type).c_str ());
      return finish_turn (run, &out);
    }

  if (system_command (run, in, &out))
    { run->amb_active = 0; run->remembered_verb = rverb; }   /* SystemTasks keeps it */
  else if (!rverb.empty ())
    {
      /* Remembered-verb retry: re-run the whole turn as "<verb> <input>".  The
         retried turn always produces output (its own NotUnderstood at worst), so
         it claims the turn (FD: "If sOutputText <> '' Then Exit Sub"). */
      run->amb_active = 0;
      free (out.p);
      st->turns--;            /* the retry re-bumps it; one user command == one turn */
      return a5run_input (run, (rverb + " " + in).c_str ());
    }
  else
    not_understood (run, in, &out);
  return finish_turn (run, &out);
}

/* ============================================================ save / restore */

/* Phase 5 save/restore (clsState / FileIO.SaveState|LoadState).
 *
 * The on-disk save is FrankenDrift-compatible: a `<Game>` document keyed by
 * entity <Key> (FileIO.SaveState / LoadState schema, TODO_a5_frankendrift_save_
 * compat.md).  A FrankenDrift (or official ADRIFT 5 Runner) save restores into
 * Scarier, and a Scarier save loads in FrankenDrift.
 *
 * To keep Scarier<->Scarier round-trips (single-level undo, deterministic
 * replay) lossless -- FrankenDrift's schema omits the RNG state and the full
 * event/walk internals -- every Scarier save also carries a <ScarierExt> child
 * of <Game> holding the complete native snapshot (the old <SaveState> body).
 * FrankenDrift's reader queries only the tags it knows via SelectNodes("/Game/
 * <tag>") and silently ignores <ScarierExt>, so the extension is invisible to
 * it.  On restore we prefer <ScarierExt> when present (lossless); a foreign save
 * without it is read from the FrankenDrift fields (RNG then re-seeds to 1234).
 *
 * A pre-existing raw (uncompressed) `<SaveState>` file -- the format Scarier
 * wrote before this change -- still restores through the legacy body path. */

static void
xml_esc (sb_t *b, const char *s)
{
  const char *p;
  if (s == NULL)
    return;
  for (p = s; *p; p++)
    switch (*p)
      {
      case '&': sb_puts (b, "&amp;"); break;
      case '<': sb_puts (b, "&lt;");  break;
      case '>': sb_puts (b, "&gt;");  break;
      case '"': sb_puts (b, "&quot;"); break;
      default:  sb_putc_ (b, *p);     break;
      }
}

static void
sb_elem (sb_t *b, const char *tag, const char *val)
{
  sb_puts (b, "<"); sb_puts (b, tag); sb_puts (b, ">");
  xml_esc (b, val);
  sb_puts (b, "</"); sb_puts (b, tag); sb_puts (b, ">\n");
}

static void
sb_elem_l (sb_t *b, const char *tag, long v)
{
  char num[32];
  snprintf (num, sizeof num, "%ld", v);
  sb_elem (b, tag, num);
}

/* Pre-order list of every node in the game DOM, so a <DisplayOnce> segment
   (tracked by node pointer) has a stable index across a save/restore that
   re-parses an identical game file. */
static void
collect_dom_nodes (const a5_xml_node_t *n,
                   std::vector<const a5_xml_node_t *> &out)
{
  for (; n != NULL; n = n->next)
    {
      out.push_back (n);
      collect_dom_nodes (n->first_child, out);
    }
}

/* Resolve a holder/location key to the model's own stable string pointer (so it
   survives the save buffer being freed).  Keys are globally unique in ADRIFT. */
static const char *
intern_key (const a5_adventure_t *adv, const char *key)
{
  int i;
  const a5_object_t *o;
  const a5_location_t *l;
  const a5_character_t *c;
  if (key == NULL || key[0] == '\0')
    return NULL;
  if ((o = a5model_object (adv, key)) != NULL)    return o->key;
  if ((l = a5model_location (adv, key)) != NULL)  return l->key;
  if ((c = a5model_character (adv, key)) != NULL) return c->key;
  for (i = 0; i < adv->n_groups; i++)
    if (streq (adv->groups[i].key, key))
      return adv->groups[i].key;
  return NULL;
}

/* ---------------------------------------------------------- Scarier body ----
   The full-fidelity native snapshot: RNG, every entity's runtime fields, event
   and walk internals, the sparse seen/override/displayed/look sets.  Emitted
   verbatim inside <ScarierExt> (and historically as the bare <SaveState> root);
   read back by restore_scarier_body.  Entity arrays are positional (model
   order); the sparse sets are keyed.  This is what makes undo and Scarier<->
   Scarier restore byte-for-byte deterministic despite FrankenDrift's lossier
   schema. */
static void
save_scarier_body (sb_t *b, a5_run_t *run)
{
  a5_state_t *st = run->st;
  const a5_adventure_t *adv = run->adv;
  std::vector<const a5_xml_node_t *> dom;
  int i, native;
  unsigned int rng[4];

  sb_elem_l (b, "Version", 1);

  a5rand_get_state (&native, rng);
  sb_elem_l (b, "RngNative", native);
  for (i = 0; i < 4; i++)
    sb_elem_l (b, "Rng", (long) rng[i]);

  sb_elem_l (b, "EventsRunning", run->events_running);
  sb_elem_l (b, "GameOver", st->game_over);
  sb_elem_l (b, "Turns", st->turns);
  sb_elem_l (b, "EndDisplayed", st->end_displayed);
  if (st->end_message != NULL)
    sb_elem (b, "EndMessage", st->end_message);
  if (st->conv_char != NULL && st->conv_char[0])
    sb_elem (b, "ConvChar", st->conv_char);
  if (st->conv_node != NULL && st->conv_node[0])
    sb_elem (b, "ConvNode", st->conv_node);
  if (st->s_it != NULL && st->s_it[0])
    sb_elem (b, "ItRef", st->s_it);
  if (st->s_them != NULL && st->s_them[0])
    sb_elem (b, "ThemRef", st->s_them);
  if (st->s_him != NULL && st->s_him[0])
    sb_elem (b, "HimRef", st->s_him);
  if (st->s_her != NULL && st->s_her[0])
    sb_elem (b, "HerRef", st->s_her);

  /* Objects (model order). */
  for (i = 0; i < adv->n_objects; i++)
    {
      sb_puts (b, "<Object>\n");
      sb_elem_l (b, "Where", (long) st->obj[i].where);
      sb_elem_l (b, "Static", st->obj[i].is_static);
      if (st->obj[i].key != NULL && st->obj[i].key[0])
        sb_elem (b, "Key", st->obj[i].key);
      sb_puts (b, "</Object>\n");
    }

  /* Characters (model order). */
  for (i = 0; i < adv->n_characters; i++)
    {
      sb_puts (b, "<Character>\n");
      if (st->char_loc[i] != NULL && st->char_loc[i][0])
        sb_elem (b, "Loc", st->char_loc[i]);
      if (st->char_position[i] != NULL)
        sb_elem (b, "Position", st->char_position[i]);
      if (st->char_onobj[i] != NULL && st->char_onobj[i][0])
        sb_elem (b, "OnObj", st->char_onobj[i]);
      sb_elem_l (b, "In", st->char_in ? st->char_in[i] : 0);
      sb_puts (b, "</Character>\n");
    }

  /* Player character key (clsAdventure.Player.Key; a ToSwitchWith/BECOME
     retargets it away from the literal "Player"). */
  if (st->player_key != NULL && st->player_key[0])
    sb_elem (b, "PlayerKey", st->player_key);

  /* Variables (model order). */
  for (i = 0; i < adv->n_variables; i++)
    {
      sb_puts (b, "<Variable>\n");
      sb_elem_l (b, "Num", st->var_num[i]);
      if (st->var_text[i] != NULL)
        sb_elem (b, "Text", st->var_text[i]);
      sb_puts (b, "</Variable>\n");
    }

  /* Completed tasks (sparse, by key). */
  for (i = 0; i < adv->n_tasks; i++)
    if (st->task_done[i])
      sb_elem (b, "TaskDone", adv->tasks[i].key);

  /* Property overrides. */
  for (i = 0; i < st->n_ov; i++)
    {
      sb_puts (b, "<PropOv>\n");
      sb_elem (b, "Entity", st->ov[i].entity);
      sb_elem (b, "Prop", st->ov[i].prop);
      sb_elem (b, "Value", st->ov[i].value);
      sb_puts (b, "</PropOv>\n");
    }

  /* "Seen" sets (sparse, by key). */
  if (st->obj_seen != NULL)
    for (i = 0; i < adv->n_objects; i++)
      if (st->obj_seen[i])
        sb_elem (b, "ObjSeen", adv->objects[i].key);
  if (st->char_seen != NULL)
    for (i = 0; i < adv->n_characters; i++)
      if (st->char_seen[i])
        sb_elem (b, "CharSeen", adv->characters[i].key);
  if (st->loc_seen != NULL)
    for (i = 0; i < adv->n_locations; i++)
      if (st->loc_seen[i])
        sb_elem (b, "LocSeen", adv->locations[i].key);

  /* Per-character seen sets for any OTHER viewpoint a BECOME game has switched
     away from (the active player's set is the top-level ObjSeen/CharSeen/LocSeen
     above).  Mirrors FD's per-character clsCharacterState.lSeenKeys so a saved
     BugHunt restores each marine's own sightings.  Empty for a lone-"Player"
     game. */
  for (i = 0; i < adv->n_characters; i++)
    {
      int j;
      char *os = st->seen_stash_obj  ? st->seen_stash_obj[i]  : NULL;
      char *cs = st->seen_stash_char ? st->seen_stash_char[i] : NULL;
      char *ls = st->seen_stash_loc  ? st->seen_stash_loc[i]  : NULL;
      if (i == st->seen_active_ci || (os == NULL && cs == NULL && ls == NULL))
        continue;
      sb_puts (b, "<SeenChar>\n");
      sb_elem (b, "Key", adv->characters[i].key);
      if (os != NULL)
        for (j = 0; j < adv->n_objects; j++)
          if (os[j]) sb_elem (b, "ObjSeen", adv->objects[j].key);
      if (cs != NULL)
        for (j = 0; j < adv->n_characters; j++)
          if (cs[j]) sb_elem (b, "CharSeen", adv->characters[j].key);
      if (ls != NULL)
        for (j = 0; j < adv->n_locations; j++)
          if (ls[j]) sb_elem (b, "LocSeen", adv->locations[j].key);
      sb_puts (b, "</SeenChar>\n");
    }

  /* Events (model order). */
  for (i = 0; i < (int) run->events->size (); i++)
    {
      a5_event_rt &e = (*run->events)[i];
      size_t s;
      sb_puts (b, "<Event>\n");
      sb_elem_l (b, "Status", e.status);
      sb_elem_l (b, "Length", e.length_value);
      sb_elem_l (b, "Timer", e.timer_to_end);
      sb_elem_l (b, "LastSeTime", e.last_se_time);
      sb_elem_l (b, "LastSeIndex", e.last_se_index);
      sb_elem_l (b, "JustStarted", e.just_started);
      sb_elem_l (b, "NextCommand", e.next_command);
      sb_elem_l (b, "WhenStart", e.when_start);
      sb_elem (b, "Trigger", e.triggering_task.c_str ());
      for (s = 0; s < e.se_ft.size (); s++)
        sb_elem_l (b, "SeFt", e.se_ft[s]);
      sb_puts (b, "</Event>\n");
    }

  /* Walks (flattened order, as built by a5run_new). */
  for (i = 0; i < (int) run->walks->size (); i++)
    {
      a5_walk_rt &w = (*run->walks)[i];
      size_t s;
      sb_puts (b, "<Walk>\n");
      sb_elem_l (b, "Status", w.status);
      sb_elem_l (b, "Length", w.length);
      sb_elem_l (b, "Timer", w.timer_to_end);
      sb_elem_l (b, "LastSwTime", w.last_sw_time);
      sb_elem_l (b, "LastSwIndex", w.last_sw_index);
      sb_elem_l (b, "JustStarted", w.just_started);
      sb_elem_l (b, "NextCommand", w.next_command);
      sb_elem (b, "Trigger", w.triggering_task.c_str ());
      for (s = 0; s < w.step_dur.size (); s++)
        sb_elem_l (b, "StepDur", w.step_dur[s]);
      for (s = 0; s < w.sw_ft.size (); s++)
        sb_elem_l (b, "SwFt", w.sw_ft[s]);
      for (s = 0; s < w.came_across.size (); s++)
        sb_elem_l (b, "CameAcross", w.came_across[s]);
      sb_puts (b, "</Walk>\n");
    }

  /* Displayed <DisplayOnce> segments (by DOM pre-order index). */
  if (st->n_disp_once > 0)
    {
      int j;
      collect_dom_nodes (a5xml_root (adv->doc), dom);
      for (j = 0; j < st->n_disp_once; j++)
        {
          size_t k;
          for (k = 0; k < dom.size (); k++)
            if (dom[k] == st->disp_once[j])
              { sb_elem_l (b, "Displayed", (long) k); break; }
        }
    }

  /* SetLook stack. */
  for (i = 0; i < st->n_looks; i++)
    {
      sb_puts (b, "<Look>\n");
      sb_elem (b, "LocKey", st->looks[i].loc_key);
      sb_elem (b, "Text", st->looks[i].text);
      sb_puts (b, "</Look>\n");
    }
}

/* ----------------------------------------------------- FrankenDrift <Game> ---
   The interop schema.  Maps a5_state_t onto FrankenDrift's clsGameState fields,
   keyed by entity <Key>, with the where-fields written as their .ToString enum
   names.  See TODO_a5_frankendrift_save_compat.md for the full mapping. */

/* Split a5_objloc_t onto FD's two where-enums + LocationKey (SaveState @83).
   *dyn / *stat are NULL when at their default (Hidden / NoRooms, so FD omits
   them); *lkey is "" when no key applies. */
static void
fd_object_location (const a5_objloc_t *l, const char **dyn, const char **stat,
                    const char **lkey)
{
  *dyn = NULL;
  *stat = NULL;
  *lkey = "";
  switch (l->where)
    {
    case A5_OWHERE_NONE:
    case A5_OWHERE_HIDDEN:
      break;
    case A5_OWHERE_ALLROOMS:
      *stat = "AllRooms";
      break;
    case A5_OWHERE_LOCATION:
      if (l->is_static) *stat = "SingleLocation";
      else              *dyn  = "InLocation";
      *lkey = l->key ? l->key : "";
      break;
    case A5_OWHERE_LOCGROUP:
      *stat = "LocationGroup";
      *lkey = l->key ? l->key : "";
      break;
    case A5_OWHERE_ON_OBJECT:
      *dyn = "OnObject";
      *lkey = l->key ? l->key : "";
      break;
    case A5_OWHERE_IN_OBJECT:
      *dyn = "InObject";
      *lkey = l->key ? l->key : "";
      break;
    case A5_OWHERE_HELD_BY:
      *dyn = "HeldByCharacter";
      *lkey = l->key ? l->key : "";
      break;
    case A5_OWHERE_WORN_BY:
      *dyn = "WornByCharacter";
      *lkey = l->key ? l->key : "";
      break;
    case A5_OWHERE_PART_OBJECT:
      *stat = "PartOfObject";
      *lkey = l->key ? l->key : "";
      break;
    case A5_OWHERE_PART_CHAR:
      *stat = "PartOfCharacter";
      *lkey = l->key ? l->key : "";
      break;
    }
}

/* clsEvent.StatusEnum name for the runtime status int. */
static const char *
fd_event_status (int status)
{
  switch (status)
    {
    case A5_EV_NOTYET:    return "NotYetStarted";
    case A5_EV_RUNNING:   return "Running";
    case A5_EV_COUNTDOWN: return "CountingDownToStart";
    case A5_EV_PAUSED:    return "Paused";
    case A5_EV_FINISHED:  return "Finished";
    default:              return "NotYetStarted";
    }
}

/* clsWalk.StatusEnum name (no CountingDownToStart -- walks have no StartDelay). */
static const char *
fd_walk_status (int status)
{
  switch (status)
    {
    case A5_EV_NOTYET:   return "NotYetStarted";
    case A5_EV_RUNNING:  return "Running";
    case A5_EV_PAUSED:   return "Paused";
    case A5_EV_FINISHED: return "Finished";
    default:             return "NotYetStarted";
    }
}

/* Emit <Property> children for an entity: its base (model) properties with the
   runtime-effective value, plus any override that added a property not in the
   base list.  FrankenDrift's RestoreState *removes* any current property absent
   from the state, so a partial list would strip the entity -- we emit the full
   effective set to be faithful. */
static void
fd_write_props (sb_t *b, a5_state_t *st, const char *entkey,
                const a5_prop_t *props, int n_props)
{
  int i, j;

  for (i = 0; i < n_props; i++)
    {
      const char *val = a5state_entity_prop (st, entkey, props[i].key);
      if (val == NULL)
        val = props[i].value;   /* rich <Value> block: no scalar -> "" */
      sb_puts (b, "<Property>\n");
      sb_elem (b, "Key", props[i].key);
      sb_elem (b, "Value", val ? val : "");
      sb_puts (b, "</Property>\n");
    }
  for (j = 0; j < st->n_ov; j++)
    {
      int is_base = 0;
      if (!streq (st->ov[j].entity, entkey))
        continue;
      for (i = 0; i < n_props; i++)
        if (streq (props[i].key, st->ov[j].prop)) { is_base = 1; break; }
      if (is_base)
        continue;
      sb_puts (b, "<Property>\n");
      sb_elem (b, "Key", st->ov[j].prop);
      sb_elem (b, "Value", st->ov[j].value ? st->ov[j].value : "");
      sb_puts (b, "</Property>\n");
    }
}

static void
save_fd_game (sb_t *b, a5_run_t *run)
{
  a5_state_t *st = run->st;
  const a5_adventure_t *adv = run->adv;
  const char *player = a5state_player_key (st);
  int i, w;

  /* Locations: property overrides only (base props are static; FD recomputes
     inherited ones).  Emit a <Location> only when it has a runtime override so
     FD does not strip its (unlisted) actual properties. */
  for (i = 0; i < adv->n_locations; i++)
    {
      int has_ov = 0, j;
      for (j = 0; j < st->n_ov; j++)
        if (streq (st->ov[j].entity, adv->locations[i].key)) { has_ov = 1; break; }
      if (!has_ov)
        continue;
      sb_puts (b, "<Location>\n");
      sb_elem (b, "Key", adv->locations[i].key);
      fd_write_props (b, st, adv->locations[i].key,
                      adv->locations[i].props, adv->locations[i].n_props);
      sb_puts (b, "</Location>\n");
    }

  /* Objects. */
  for (i = 0; i < adv->n_objects; i++)
    {
      const char *dyn, *stat, *lkey;
      /* <Key> is the object's own identity; the holder/location it sits in is
         the LocationKey (st->obj[i].key, resolved by fd_object_location). */
      fd_object_location (&st->obj[i], &dyn, &stat, &lkey);
      sb_puts (b, "<Object>\n");
      sb_elem (b, "Key", adv->objects[i].key);
      if (dyn != NULL)
        sb_elem (b, "DynamicExistWhere", dyn);
      if (stat != NULL)
        sb_elem (b, "StaticExistWhere", stat);
      sb_elem (b, "LocationKey", lkey);
      fd_write_props (b, st, adv->objects[i].key,
                      adv->objects[i].props, adv->objects[i].n_props);
      sb_puts (b, "</Object>\n");
    }

  /* Tasks (Completed; Scarier has no per-task Scored flag -> FD defaults it
     false).  Emitted for every task, matching FD's writer. */
  for (i = 0; i < adv->n_tasks; i++)
    {
      sb_puts (b, "<Task>\n");
      sb_elem (b, "Key", adv->tasks[i].key);
      sb_elem (b, "Completed", st->task_done[i] ? "True" : "False");
      sb_elem (b, "Scored", st->task_scored[i] ? "True" : "False");
      sb_puts (b, "</Task>\n");
    }

  /* Events. */
  for (i = 0; i < adv->n_events && i < (int) run->events->size (); i++)
    {
      a5_event_rt &e = (*run->events)[i];
      /* Scarier's last_se_index is -1 for "no sub-event has run"; FrankenDrift
         has no such sentinel and would do SubEvents(-1) (its only guard is
         Length > index, which -1 passes) -> IndexOutOfRange.  Emit a value >=
         the sub-event count so FD's guard fails and it leaves LastSubEvent as
         Nothing, matching the -1 meaning. */
      long sei = e.last_se_index < 0 ? adv->events[i].n_subevents
                                     : e.last_se_index;
      sb_puts (b, "<Event>\n");
      sb_elem (b, "Key", adv->events[i].key);
      sb_elem (b, "Status", fd_event_status (e.status));
      sb_elem_l (b, "Timer", e.timer_to_end);
      sb_elem_l (b, "SubEventTime", e.last_se_time);
      sb_elem_l (b, "SubEventIndex", sei);
      sb_puts (b, "</Event>\n");
    }

  /* Characters. */
  for (i = 0; i < adv->n_characters; i++)
    {
      const char *key = adv->characters[i].key;
      const char *where = NULL, *lkey = NULL;
      const char *pos = st->char_position[i];
      if (st->char_onobj[i] != NULL && st->char_onobj[i][0])
        {
          where = (st->char_in && st->char_in[i]) ? "InObject" : "OnObject";
          lkey = st->char_onobj[i];
        }
      else if (st->char_loc[i] != NULL && st->char_loc[i][0])
        {
          where = "AtLocation";
          lkey = st->char_loc[i];
        }
      /* else: Hidden -- FD omits ExistWhere/LocationKey. */

      sb_puts (b, "<Character>\n");
      sb_elem (b, "Key", key);
      if (where != NULL)
        sb_elem (b, "ExistWhere", where);
      if (pos != NULL && !streq (pos, "Standing"))
        sb_elem (b, "Position", pos);
      if (lkey != NULL && lkey[0])
        sb_elem (b, "LocationKey", lkey);

      /* Walks owned by this character, in flattened order. */
      for (w = 0; w < (int) run->walks->size (); w++)
        {
          a5_walk_rt &wk = (*run->walks)[w];
          if (wk.char_index != i)
            continue;
          sb_puts (b, "<Walk>\n");
          sb_elem (b, "Status", fd_walk_status (wk.status));
          sb_elem_l (b, "Timer", wk.timer_to_end);
          sb_puts (b, "</Walk>\n");
        }

      /* Seen keys: Scarier tracks only the player's, so emit them under the
         player character (FD stores per-character; NPC seen-sets are untracked). */
      if (streq (key, player))
        {
          int k;
          if (st->obj_seen != NULL)
            for (k = 0; k < adv->n_objects; k++)
              if (st->obj_seen[k])
                sb_elem (b, "Seen", adv->objects[k].key);
          if (st->char_seen != NULL)
            for (k = 0; k < adv->n_characters; k++)
              if (st->char_seen[k])
                sb_elem (b, "Seen", adv->characters[k].key);
          if (st->loc_seen != NULL)
            for (k = 0; k < adv->n_locations; k++)
              if (st->loc_seen[k])
                sb_elem (b, "Seen", adv->locations[k].key);
        }

      fd_write_props (b, st, key,
                      adv->characters[i].props, adv->characters[i].n_props);
      sb_puts (b, "</Character>\n");
    }

  /* Variables (single-valued -> Value_0). */
  for (i = 0; i < adv->n_variables; i++)
    {
      int numeric = !streq (adv->variables[i].type, "Text");
      sb_puts (b, "<Variable>\n");
      sb_elem (b, "Key", adv->variables[i].key);
      if (numeric)
        {
          if (st->var_num[i] != 0)
            sb_elem_l (b, "Value_0", st->var_num[i]);
        }
      else if (st->var_text[i] != NULL && st->var_text[i][0])
        sb_elem (b, "Value_0", st->var_text[i]);
      sb_puts (b, "</Variable>\n");
    }

  /* Groups: effective membership.  For Objects groups the runtime override
     layer can add/remove members; other group types are static (model list). */
  for (i = 0; i < adv->n_groups; i++)
    {
      int j;
      sb_puts (b, "<Group>\n");
      sb_elem (b, "Key", adv->groups[i].key);
      if (streq (adv->groups[i].type, "Objects"))
        {
          for (j = 0; j < adv->n_objects; j++)
            if (a5state_object_in_group (st, adv->groups[i].key,
                                         adv->objects[j].key))
              sb_elem (b, "Member", adv->objects[j].key);
        }
      else
        {
          for (j = 0; j < adv->groups[i].n_members; j++)
            sb_elem (b, "Member", adv->groups[i].members[j]);
        }
      sb_puts (b, "</Group>\n");
    }

  sb_elem_l (b, "Turns", st->turns);
}

char *
a5run_save (a5_run_t *run, size_t *out_len)
{
  sb_t b;

  if (run == NULL)
    return NULL;
  sb_init (&b);
  sb_puts (&b, "<Game>\n");
  save_fd_game (&b, run);
  /* Scarier-private, full-fidelity snapshot; FrankenDrift's LoadState ignores
     any <Game> child it does not query, so <ScarierExt> is invisible to it. */
  sb_puts (&b, "<ScarierExt>\n");
  save_scarier_body (&b, run);
  sb_puts (&b, "</ScarierExt>\n");
  sb_puts (&b, "</Game>\n");
  if (out_len != NULL)
    *out_len = b.len;
  return sb_take (&b);
}

/* strtol on a child's text (0 when absent). */
static long
child_long (const a5_xml_node_t *n, const char *name)
{
  const char *t = a5xml_child_text (n, name);
  return t != NULL ? strtol (t, NULL, 10) : 0;
}

/* Zero/clear the accumulating + sparse state before applying a save (shared by
   both the native and the FrankenDrift readers). */
static void
restore_reset (a5_run_t *run)
{
  a5_state_t *st = run->st;
  const a5_adventure_t *adv = run->adv;
  int i;

  for (i = 0; i < adv->n_tasks; i++)
    { st->task_done[i] = 0; st->task_scored[i] = 0; }
  for (i = 0; i < st->n_ov; i++)
    { free (st->ov[i].entity); free (st->ov[i].prop); free (st->ov[i].value); }
  st->n_ov = 0;
  if (st->obj_seen != NULL)
    memset (st->obj_seen, 0, (size_t) adv->n_objects);
  if (st->char_seen != NULL)
    memset (st->char_seen, 0, (size_t) adv->n_characters);
  if (st->loc_seen != NULL)
    memset (st->loc_seen, 0, (size_t) adv->n_locations);
  /* Drop the per-character BECOME seen-stash and re-home the active arrays on
     the starting player (restore/restart rebuilds the player-centric set). */
  for (i = 0; i < adv->n_characters; i++)
    {
      if (st->seen_stash_obj  != NULL) { free (st->seen_stash_obj[i]);  st->seen_stash_obj[i]  = NULL; }
      if (st->seen_stash_char != NULL) { free (st->seen_stash_char[i]); st->seen_stash_char[i] = NULL; }
      if (st->seen_stash_loc  != NULL) { free (st->seen_stash_loc[i]);  st->seen_stash_loc[i]  = NULL; }
    }
  st->seen_active_ci = a5state_character_index (st, a5state_player_key (st));
  st->n_disp_once = 0;
  for (i = 0; i < st->n_looks; i++)
    { free (st->looks[i].loc_key); free (st->looks[i].text); }
  st->n_looks = 0;
  free (st->end_message);
  st->end_message = NULL;
  st->game_over = 0;
  st->turns = 0;
  st->end_displayed = 0;
  free (st->s_it);   st->s_it   = strdup ("");
  free (st->s_them); st->s_them = strdup ("");
  free (st->s_him);  st->s_him  = strdup ("");
  free (st->s_her);  st->s_her  = strdup ("");
}

/* Apply the full-fidelity Scarier snapshot from `container` (the <ScarierExt>
   node of a <Game> save, or the root of a legacy raw <SaveState> file). */
static void
restore_scarier_body (a5_run_t *run, const a5_xml_node_t *container)
{
  a5_state_t *st = run->st;
  const a5_adventure_t *adv = run->adv;
  const a5_xml_node_t *n;
  unsigned int rng[4] = { 0, 0, 0, 0 };
  int native = 0, rng_i = 0;
  int obj_i = 0, char_i = 0, var_i = 0, ev_i = 0, wk_i = 0;
  std::vector<const a5_xml_node_t *> dom;

  restore_reset (run);

  for (n = container->first_child; n != NULL; n = n->next)
    {
      const char *nm = n->name;
      if (streq (nm, "RngNative"))
        native = (int) strtol (n->text ? n->text : "0", NULL, 10);
      else if (streq (nm, "Rng"))
        { if (rng_i < 4) rng[rng_i++] = (unsigned int) strtoul (n->text ? n->text : "0", NULL, 10); }
      else if (streq (nm, "EventsRunning"))
        run->events_running = (int) strtol (n->text ? n->text : "0", NULL, 10);
      else if (streq (nm, "GameOver"))
        st->game_over = (int) strtol (n->text ? n->text : "0", NULL, 10);
      else if (streq (nm, "Turns"))
        st->turns = (int) strtol (n->text ? n->text : "0", NULL, 10);
      else if (streq (nm, "EndDisplayed"))
        st->end_displayed = (int) strtol (n->text ? n->text : "0", NULL, 10);
      else if (streq (nm, "EndMessage"))
        st->end_message = strdup (n->text ? n->text : "");
      else if (streq (nm, "ConvChar"))
        a5state_set_conv_char (st, n->text ? n->text : "");
      else if (streq (nm, "ConvNode"))
        a5state_set_conv_node (st, n->text ? n->text : "");
      else if (streq (nm, "ItRef"))
        { free (st->s_it);   st->s_it   = strdup (n->text ? n->text : ""); }
      else if (streq (nm, "ThemRef"))
        { free (st->s_them); st->s_them = strdup (n->text ? n->text : ""); }
      else if (streq (nm, "HimRef"))
        { free (st->s_him);  st->s_him  = strdup (n->text ? n->text : ""); }
      else if (streq (nm, "HerRef"))
        { free (st->s_her);  st->s_her  = strdup (n->text ? n->text : ""); }
      else if (streq (nm, "PlayerKey"))
        {
          const char *pk = intern_key (adv, n->text);
          if (pk != NULL)
            {
              st->player_key = pk;
              st->seen_active_ci = a5state_character_index (st, pk);
            }
        }
      else if (streq (nm, "Object"))
        {
          if (obj_i < adv->n_objects)
            {
              const char *stat = a5xml_child_text (n, "Static");
              st->obj[obj_i].where = (a5_owhere_t) child_long (n, "Where");
              if (stat != NULL)
                st->obj[obj_i].is_static = (int) strtol (stat, NULL, 10);
              st->obj[obj_i].key = intern_key (adv, a5xml_child_text (n, "Key"));
              obj_i++;
            }
        }
      else if (streq (nm, "Character"))
        {
          if (char_i < adv->n_characters)
            {
              const char *loc = a5xml_child_text (n, "Loc");
              const char *pos = a5xml_child_text (n, "Position");
              const char *onobj = a5xml_child_text (n, "OnObj");
              st->char_loc[char_i] = intern_key (adv, loc);
              free (st->char_position[char_i]);
              st->char_position[char_i] = strdup (pos ? pos : "Standing");
              st->char_onobj[char_i] = intern_key (adv, onobj);
              if (st->char_in != NULL)
                st->char_in[char_i] = (char) child_long (n, "In");
              char_i++;
            }
        }
      else if (streq (nm, "Variable"))
        {
          if (var_i < adv->n_variables)
            {
              const char *txt = a5xml_child_text (n, "Text");
              st->var_num[var_i] = child_long (n, "Num");
              free (st->var_text[var_i]);
              st->var_text[var_i] = txt != NULL ? strdup (txt) : NULL;
              var_i++;
            }
        }
      else if (streq (nm, "TaskDone"))
        {
          int ti = a5state_task_index (st, n->text ? n->text : "");
          if (ti >= 0)
            st->task_done[ti] = 1;
        }
      else if (streq (nm, "PropOv"))
        {
          const char *e = a5xml_child_text (n, "Entity");
          const char *p = a5xml_child_text (n, "Prop");
          const char *v = a5xml_child_text (n, "Value");
          if (e != NULL && p != NULL)
            a5state_set_prop (st, e, p, v ? v : "");
        }
      else if (streq (nm, "ObjSeen"))
        {
          int oi = a5state_object_index (st, n->text ? n->text : "");
          if (oi >= 0 && st->obj_seen != NULL)
            st->obj_seen[oi] = 1;
        }
      else if (streq (nm, "CharSeen"))
        {
          int ci = a5state_character_index (st, n->text ? n->text : "");
          if (ci >= 0 && st->char_seen != NULL)
            st->char_seen[ci] = 1;
        }
      else if (streq (nm, "LocSeen"))
        a5state_mark_loc_seen (st, n->text ? n->text : "");
      else if (streq (nm, "SeenChar"))
        {
          /* A non-active viewpoint's per-character seen set (BECOME games).
             Populate its stash slot, or the active arrays if it is (unexpectedly)
             the current player. */
          int ci = a5state_character_index (st, a5xml_child_text (n, "Key"));
          if (ci >= 0)
            {
              const a5_xml_node_t *c;
              int active = (ci == st->seen_active_ci);
              char *os = st->obj_seen, *cs = st->char_seen, *ls = st->loc_seen;
              if (!active)
                {
                  if (st->seen_stash_obj[ci]  == NULL) st->seen_stash_obj[ci]  = (char *) calloc ((size_t) adv->n_objects, 1);
                  if (st->seen_stash_char[ci] == NULL) st->seen_stash_char[ci] = (char *) calloc ((size_t) adv->n_characters, 1);
                  if (st->seen_stash_loc[ci]  == NULL) st->seen_stash_loc[ci]  = (char *) calloc ((size_t) adv->n_locations, 1);
                  os = st->seen_stash_obj[ci]; cs = st->seen_stash_char[ci]; ls = st->seen_stash_loc[ci];
                }
              for (c = n->first_child; c != NULL; c = c->next)
                {
                  if (streq (c->name, "ObjSeen"))
                    { int oi = a5state_object_index (st, c->text ? c->text : ""); if (oi >= 0 && os) os[oi] = 1; }
                  else if (streq (c->name, "CharSeen"))
                    { int xi = a5state_character_index (st, c->text ? c->text : ""); if (xi >= 0 && cs) cs[xi] = 1; }
                  else if (streq (c->name, "LocSeen"))
                    { int li = a5state_location_index (st, c->text ? c->text : ""); if (li >= 0 && ls) ls[li] = 1; }
                }
            }
        }
      else if (streq (nm, "Event"))
        {
          if (ev_i < (int) run->events->size ())
            {
              a5_event_rt &e = (*run->events)[ev_i];
              const a5_xml_node_t *c;
              const char *trg = a5xml_child_text (n, "Trigger");
              int s = 0;
              e.status       = (int)  child_long (n, "Status");
              e.length_value =        child_long (n, "Length");
              e.timer_to_end =        child_long (n, "Timer");
              e.last_se_time =        child_long (n, "LastSeTime");
              e.last_se_index = (int) child_long (n, "LastSeIndex");
              e.just_started = (int)  child_long (n, "JustStarted");
              e.next_command = (int)  child_long (n, "NextCommand");
              e.when_start   = (int)  child_long (n, "WhenStart");
              e.triggering_task = trg ? trg : "";
              for (c = n->first_child; c != NULL; c = c->next)
                if (streq (c->name, "SeFt") && s < (int) e.se_ft.size ())
                  e.se_ft[s++] = strtol (c->text ? c->text : "0", NULL, 10);
              ev_i++;
            }
        }
      else if (streq (nm, "Walk"))
        {
          if (wk_i < (int) run->walks->size ())
            {
              a5_walk_rt &w = (*run->walks)[wk_i];
              const a5_xml_node_t *c;
              const char *trg = a5xml_child_text (n, "Trigger");
              int sd = 0, sf = 0, ca = 0;
              w.status       = (int) child_long (n, "Status");
              w.length       =       child_long (n, "Length");
              w.timer_to_end =       child_long (n, "Timer");
              w.last_sw_time =       child_long (n, "LastSwTime");
              w.last_sw_index = (int) child_long (n, "LastSwIndex");
              w.just_started = (int) child_long (n, "JustStarted");
              w.next_command = (int) child_long (n, "NextCommand");
              w.triggering_task = trg ? trg : "";
              for (c = n->first_child; c != NULL; c = c->next)
                {
                  if (streq (c->name, "StepDur") && sd < (int) w.step_dur.size ())
                    w.step_dur[sd++] = strtol (c->text ? c->text : "0", NULL, 10);
                  else if (streq (c->name, "SwFt") && sf < (int) w.sw_ft.size ())
                    w.sw_ft[sf++] = strtol (c->text ? c->text : "0", NULL, 10);
                  else if (streq (c->name, "CameAcross") && ca < (int) w.came_across.size ())
                    w.came_across[ca++] = (char) strtol (c->text ? c->text : "0", NULL, 10);
                }
              wk_i++;
            }
        }
      else if (streq (nm, "Displayed"))
        {
          long k = strtol (n->text ? n->text : "-1", NULL, 10);
          if (dom.empty ())
            collect_dom_nodes (a5xml_root (adv->doc), dom);
          if (k >= 0 && k < (long) dom.size ())
            a5state_disp_once_mark (st, dom[(size_t) k]);
        }
      else if (streq (nm, "Look"))
        {
          a5state_push_look (st, a5xml_child_text (n, "LocKey"),
                             a5xml_child_text (n, "Text"));
        }
    }

  a5rand_set_state (native, rng);
}

/* ------------------------------------------------ FrankenDrift <Game> reader ---
   Applied only to a foreign save (a FrankenDrift / ADRIFT 5 Runner file with no
   <ScarierExt>).  Keyed lookups, tolerant of missing/extra entities; the RNG
   re-seeds to 1234 (FD saves carry none), so a game that draws randomness right
   after such a restore diverges -- documented in the TODO. */

static a5_owhere_t
fd_decode_object_where (const char *dyn, const char *stat, int *is_static)
{
  *is_static = 0;
  if (stat != NULL && !streq (stat, "NoRooms"))
    {
      *is_static = 1;
      if (streq (stat, "SingleLocation"))  return A5_OWHERE_LOCATION;
      if (streq (stat, "AllRooms"))        return A5_OWHERE_ALLROOMS;
      if (streq (stat, "LocationGroup"))   return A5_OWHERE_LOCGROUP;
      if (streq (stat, "PartOfObject"))    return A5_OWHERE_PART_OBJECT;
      if (streq (stat, "PartOfCharacter")) return A5_OWHERE_PART_CHAR;
      return A5_OWHERE_HIDDEN;
    }
  if (dyn != NULL)
    {
      if (streq (dyn, "InLocation"))       return A5_OWHERE_LOCATION;
      if (streq (dyn, "OnObject"))         return A5_OWHERE_ON_OBJECT;
      if (streq (dyn, "InObject"))         return A5_OWHERE_IN_OBJECT;
      if (streq (dyn, "HeldByCharacter"))  return A5_OWHERE_HELD_BY;
      if (streq (dyn, "WornByCharacter"))  return A5_OWHERE_WORN_BY;
    }
  return A5_OWHERE_HIDDEN;
}

static int
fd_decode_event_status (const char *s)
{
  if (s == NULL)                            return A5_EV_NOTYET;
  if (streq (s, "Running"))                 return A5_EV_RUNNING;
  if (streq (s, "CountingDownToStart"))     return A5_EV_COUNTDOWN;
  if (streq (s, "Paused"))                  return A5_EV_PAUSED;
  if (streq (s, "Finished"))                return A5_EV_FINISHED;
  return A5_EV_NOTYET;
}

static int
fd_decode_walk_status (const char *s)
{
  if (s == NULL)              return A5_EV_NOTYET;
  if (streq (s, "Running"))   return A5_EV_RUNNING;
  if (streq (s, "Paused"))    return A5_EV_PAUSED;
  if (streq (s, "Finished"))  return A5_EV_FINISHED;
  return A5_EV_NOTYET;
}

static int
fd_event_index (const a5_adventure_t *adv, const char *key)
{
  int i;
  if (key == NULL)
    return -1;
  for (i = 0; i < adv->n_events; i++)
    if (streq (adv->events[i].key, key))
      return i;
  return -1;
}

static void
restore_fd_game (a5_run_t *run, const a5_xml_node_t *root)
{
  a5_state_t *st = run->st;
  const a5_adventure_t *adv = run->adv;
  const char *player = a5state_player_key (st);
  const a5_xml_node_t *n, *c;

  restore_reset (run);

  for (n = root->first_child; n != NULL; n = n->next)
    {
      const char *nm = n->name;
      if (streq (nm, "Turns"))
        st->turns = (int) strtol (n->text ? n->text : "0", NULL, 10);
      else if (streq (nm, "Location"))
        {
          const char *lk = a5xml_child_text (n, "Key");
          if (lk != NULL)
            for (c = n->first_child; c != NULL; c = c->next)
              if (streq (c->name, "Property"))
                {
                  const char *pk = a5xml_child_text (c, "Key");
                  const char *pv = a5xml_child_text (c, "Value");
                  if (pk != NULL)
                    a5state_set_prop (st, lk, pk, pv ? pv : "");
                }
        }
      else if (streq (nm, "Object"))
        {
          int oi = a5state_object_index (st, a5xml_child_text (n, "Key"));
          if (oi >= 0)
            {
              int is_static = 0;
              a5_owhere_t w = fd_decode_object_where (
                  a5xml_child_text (n, "DynamicExistWhere"),
                  a5xml_child_text (n, "StaticExistWhere"), &is_static);
              st->obj[oi].where = w;
              st->obj[oi].is_static = is_static;
              if (w == A5_OWHERE_NONE || w == A5_OWHERE_HIDDEN
                  || w == A5_OWHERE_ALLROOMS)
                st->obj[oi].key = NULL;
              else
                st->obj[oi].key =
                  intern_key (adv, a5xml_child_text (n, "LocationKey"));
              for (c = n->first_child; c != NULL; c = c->next)
                if (streq (c->name, "Property"))
                  {
                    const char *pk = a5xml_child_text (c, "Key");
                    const char *pv = a5xml_child_text (c, "Value");
                    if (pk != NULL)
                      a5state_set_prop (st, adv->objects[oi].key, pk,
                                        pv ? pv : "");
                  }
            }
        }
      else if (streq (nm, "Task"))
        {
          int ti = a5state_task_index (st, a5xml_child_text (n, "Key"));
          if (ti >= 0)
            {
              st->task_done[ti] = a5xml_bool (a5xml_child_text (n, "Completed"));
              st->task_scored[ti] = a5xml_bool (a5xml_child_text (n, "Scored"));
            }
        }
      else if (streq (nm, "Event"))
        {
          int ei = fd_event_index (adv, a5xml_child_text (n, "Key"));
          if (ei >= 0 && ei < (int) run->events->size ())
            {
              a5_event_rt &e = (*run->events)[ei];
              int sei = (int) child_long (n, "SubEventIndex");
              e.status = fd_decode_event_status (a5xml_child_text (n, "Status"));
              e.timer_to_end = child_long (n, "Timer");
              e.last_se_time = child_long (n, "SubEventTime");
              /* Our own writer emits count for "none"; also clamp any index at
                 or past the sub-event list back to Scarier's -1 sentinel. */
              e.last_se_index = (sei >= 0 && sei < adv->events[ei].n_subevents)
                                  ? sei : -1;
            }
        }
      else if (streq (nm, "Character"))
        {
          int ci = a5state_character_index (st, a5xml_child_text (n, "Key"));
          if (ci >= 0)
            {
              const char *where = a5xml_child_text (n, "ExistWhere");
              const char *pos   = a5xml_child_text (n, "Position");
              const char *lkey  = a5xml_child_text (n, "LocationKey");
              int wslot = 0;

              st->char_loc[ci] = NULL;
              st->char_onobj[ci] = NULL;
              if (st->char_in != NULL)
                st->char_in[ci] = 0;
              if (where == NULL || streq (where, "Hidden")
                  || streq (where, "Uninitialised"))
                { /* hidden: no placement */ }
              else if (streq (where, "AtLocation")
                       || streq (where, "OnCharacter"))
                st->char_loc[ci] = intern_key (adv, lkey);
              else if (streq (where, "OnObject"))
                st->char_onobj[ci] = intern_key (adv, lkey);
              else if (streq (where, "InObject"))
                {
                  st->char_onobj[ci] = intern_key (adv, lkey);
                  if (st->char_in != NULL)
                    st->char_in[ci] = 1;
                }
              free (st->char_position[ci]);
              st->char_position[ci] = strdup (pos ? pos : "Standing");

              /* Walks: nested <Walk> in order map onto this character's slots. */
              for (c = n->first_child; c != NULL; c = c->next)
                if (streq (c->name, "Walk"))
                  {
                    int found = -1, count = 0, w;
                    for (w = 0; w < (int) run->walks->size (); w++)
                      if ((*run->walks)[w].char_index == ci)
                        { if (count == wslot) { found = w; break; } count++; }
                    if (found >= 0)
                      {
                        a5_walk_rt &wk = (*run->walks)[found];
                        wk.status =
                          fd_decode_walk_status (a5xml_child_text (c, "Status"));
                        wk.timer_to_end = child_long (c, "Timer");
                      }
                    wslot++;
                  }

              /* Seen keys (player only -- Scarier tracks no NPC seen-sets). */
              if (streq (adv->characters[ci].key, player))
                for (c = n->first_child; c != NULL; c = c->next)
                  if (streq (c->name, "Seen"))
                    {
                      const char *k = c->text ? c->text : "";
                      int oi = a5state_object_index (st, k);
                      int cc = a5state_character_index (st, k);
                      if (oi >= 0 && st->obj_seen != NULL)
                        st->obj_seen[oi] = 1;
                      else if (cc >= 0 && st->char_seen != NULL)
                        st->char_seen[cc] = 1;
                      else
                        a5state_mark_loc_seen (st, k);
                    }

              for (c = n->first_child; c != NULL; c = c->next)
                if (streq (c->name, "Property"))
                  {
                    const char *pk = a5xml_child_text (c, "Key");
                    const char *pv = a5xml_child_text (c, "Value");
                    if (pk != NULL)
                      a5state_set_prop (st, adv->characters[ci].key, pk,
                                        pv ? pv : "");
                  }
            }
        }
      else if (streq (nm, "Variable"))
        {
          int vi = a5state_variable_index (st, a5xml_child_text (n, "Key"));
          if (vi >= 0)
            {
              const char *v0 = a5xml_child_text (n, "Value_0");
              if (!streq (adv->variables[vi].type, "Text"))
                st->var_num[vi] = v0 != NULL ? strtol (v0, NULL, 10) : 0;
              else
                {
                  free (st->var_text[vi]);
                  st->var_text[vi] = strdup (v0 ? v0 : "");
                }
            }
        }
      else if (streq (nm, "Group"))
        {
          const char *gk = a5xml_child_text (n, "Key");
          int gi;
          for (gi = 0; gi < adv->n_groups; gi++)
            if (streq (adv->groups[gi].key, gk))
              break;
          if (gi < adv->n_groups && streq (adv->groups[gi].type, "Objects"))
            {
              int j;
              /* Effective membership = the listed <Member> set (add/remove vs
                 the model list via the runtime override layer). */
              for (j = 0; j < adv->n_objects; j++)
                {
                  const char *ok = adv->objects[j].key;
                  int listed = 0;
                  for (c = n->first_child; c != NULL; c = c->next)
                    if (streq (c->name, "Member") && streq (c->text, ok))
                      { listed = 1; break; }
                  if (listed != a5state_object_in_group (st, gk, ok))
                    a5state_set_object_in_group (st, gk, ok, listed);
                }
            }
        }
    }
  /* No RNG in a FrankenDrift save: the run keeps its current (fresh: 1234) seed. */
}

int
a5run_restore (a5_run_t *run, const char *data, size_t len)
{
  a5_xml_doc_t *doc;
  a5_xml_node_t *root;
  char *buf;

  if (run == NULL || data == NULL)
    return 0;
  buf = (char *) malloc (len + 1);
  if (buf == NULL)
    return 0;
  memcpy (buf, data, len);
  buf[len] = '\0';
  doc = a5xml_parse (buf, (uint32_t) len);
  if (doc == NULL)
    { free (buf); return 0; }
  root = a5xml_root (doc);
  if (root == NULL)
    { a5xml_free (doc); return 0; }

  if (streq (root->name, "SaveState"))
    {
      /* Legacy raw Scarier save (pre-FrankenDrift-interop). */
      restore_scarier_body (run, root);
    }
  else if (streq (root->name, "Game"))
    {
      const a5_xml_node_t *ext = a5xml_child (root, "ScarierExt");
      if (ext != NULL)
        restore_scarier_body (run, ext);   /* our own save: full fidelity */
      else
        restore_fd_game (run, root);       /* a foreign FrankenDrift save */
    }
  else
    { a5xml_free (doc); return 0; }

  a5xml_free (doc);
  return 1;
}
