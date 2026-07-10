/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- private shared declarations for the turn-loop
 * translation units (a5run.cpp and the a5run_*.cpp it was split into).  Holds
 * the runtime-state struct (a5_run_s) and the small types/helpers those files
 * pass between one another.  NOT a public interface -- host code uses a5run.h,
 * which keeps a5_run_t opaque.
 */

#ifndef SCARIER_A5RUN_INTERNAL_H
#define SCARIER_A5RUN_INTERNAL_H

#include <string.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "a5parse.h"
#include "a5run.h"
#include "a5sb.h"

/* Per-event runtime (clsEvent instance state).  Status mirrors
   clsEvent.StatusEnum; the timers mirror TimerToEndOfEvent / iLastSubEventTime /
   bJustStarted / NextCommand.  Random ranges (Length / sub-event offsets) are
   resolved per run via a5rand_between (a from==to range draws no RNG). */
enum { A5_EV_NOTYET = 0, A5_EV_RUNNING = 1, A5_EV_COUNTDOWN = 2,
       A5_EV_PAUSED = 3, A5_EV_FINISHED = 4 };
enum { A5_CMD_NONE = 0, A5_CMD_START = 1, A5_CMD_STOP = 2,
       A5_CMD_PAUSE = 3, A5_CMD_RESUME = 4, A5_CMD_RESTART = 5 };
struct a5_event_rt {
  int  status;
  long length_value;       /* resolved Length for this run                   */
  long timer_to_end;
  long last_se_time;       /* iLastSubEventTime                              */
  int  last_se_index;      /* -1 = none (LastSubEvent Is Nothing)            */
  int  just_started;
  int  next_command;
  int  when_start;         /* mutable: Immediately -> BetweenXY after start  */
  std::string triggering_task;
  std::vector<long> se_ft; /* resolved per-sub-event turn offset             */
};

/* Per-walk runtime (clsWalk instance state).  Walks share the event status /
   command enums; CountingDownToStart is unused (walks have no StartDelay).  The
   resolved step durations (Length) re-roll on each lStart; the sub-walk offsets
   are resolved once (clsWalk never resets them). */
struct a5_walk_rt {
  const a5_walk_t *walk;
  int  char_index;         /* owning character index in adv->characters       */
  int  status;
  long length;             /* sum of resolved step durations (Length)         */
  long timer_to_end;       /* TimerToEndOfWalk                                 */
  long last_sw_time;       /* iLastSubWalkTime                                 */
  int  last_sw_index;      /* -1 = none (LastSubWalk Is Nothing)              */
  int  just_started;
  int  next_command;
  std::string triggering_task;
  std::vector<long> step_dur;     /* resolved per-step duration               */
  std::vector<long> sw_ft;        /* resolved per-sub-walk offset             */
  std::vector<char> came_across;  /* per-sub-walk ComesAcross prev-same-loc   */
};

struct resp_map;   /* per-top-level-command response collector (htblResponses) */

struct a5_run_s {
  const a5_adventure_t *adv;
  a5_state_t *st;
  /* Embedded-media events captured from this turn's rendered text (a5text media
     sink); rebuilt each a5run_intro / a5run_input.  See a5run_media_* . */
  std::vector<a5_media_event_t> *media;
  std::vector<int> *order;   /* task indices, ascending priority */

  /* FrankenDrift's per-top-level-command response aggregation layer
     (clsUserSession htblResponsesPass/Fail).  Non-NULL only while a plural
     %objects% command iterates its items (run_general): completion + override-
     fail messages are collected here keyed by message/template, deduped and
     ref-merged, then flushed once at the end (mirroring AttemptToExecuteTask's
     final Display loop).  NULL on the single-reference path, so non-plural
     commands keep the byte-identical direct-to-`out` behaviour. */
  resp_map *resp;

  /* Event runtime, one slot per adv->events (clsEvent instances). */
  std::vector<a5_event_rt> *events;

  /* Walk runtime, one slot per character walk (flattened over all characters). */
  std::vector<a5_walk_rt> *walks;
  int events_running;        /* clsUserSession.bEventsRunning (immediate vs
                                deferred Start/Stop on control triggers)      */
  sb_t *event_out;           /* sink for sub-event output during a tick       */

  /* Pending general-reference disambiguation (clsUserSession sAmbTask /
     NewReferences).  When amb_active, the previous turn matched a task whose
     reference resolved to several in-scope candidates; the next input is first
     tried as a clarifier (narrowing amb_keys) before being re-parsed as a fresh
     command. */
  int amb_active;
  int amb_task_index;        /* the remembered task to re-run                  */
  int amb_command_index;     /* which of its command patterns matched          */
  std::string amb_input;     /* the original proper input (sLastProperInput)   */
  std::string amb_ref_name;  /* the ambiguous reference ("object1", ...)       */
  char amb_ref_type;         /* 'o' object / 'c' character                     */
  std::string amb_word;      /* the noun echoed in "Which <word>?"             */
  std::vector<std::string> amb_keys;   /* the surviving candidate keys         */

  /* clsUserSession.sRememberedVerb: a bare verb that matched a "verb %ref%"
     command but supplied no reference prompted "<Verb> what/who/where?"; the
     next input is re-tried as "<verb> <input>" (NotUnderstood, vb:3471). */
  std::string remembered_verb;

  /* Every word the game recognises (clsAdventure.listKnownWords), built lazily
     the first time an unmatched command needs the "I did not understand the
     word ..." check (clsUserSession.NotUnderstood). */
  std::set<std::string> *known_words;

  /* Set by resolve_refine when a "get all"-style command resolved its %objects%
     reference to no passing item: the matched task's <FailOverride> to show
     instead of a restriction message ("There is nothing worth taking here." --
     clsUserSession.vb:788, shown only when the input contains "all").  Cleared
     each resolve. */
  const a5_xml_node_t *pending_failover;

  /* Set by resolve_plural when a %objects% NOUN matched more than one object and
     none passed the restrictions: FrankenDrift then treats the multi-match as
     ambiguous and prefixes the task's failure with its "ReferencedObjects Must
     Exist" message ("Sorry, I'm not sure which object you are trying to take.").
     The RR_FAIL consumer prepends this ahead of the restriction message and
     clears it.  Empty when there is no such ambiguity. */
  std::string plural_amb_prefix;

  /* clsAdventure.qTasksToRun: System tasks armed by a Player move into their
     <LocationTrigger> (clsCharacter.Move); drained after the turn's task runs,
     before TurnBasedStuff (clsUserSession.vb:3421). */
  std::vector<std::string> *tasks_to_run;

  /* Deferred room-view render.  The stock Look task's CompletionMessage is
     "%Player%.Location.Description" with no <Aggregate> => AggregateOutput=True,
     so FD defers its function replacement to final Display (clsUserSession.vb:
     1184/1210): the room view reflects state changed by AfterText/Actions
     children that run after the parent's `Execute Look`.  When defer_look is set
     (a parent with After children runs on the direct path), emit_look records
     the insertion offset instead of rendering; the view is then rendered once --
     at final state -- and spliced in at that offset, ahead of the children's own
     output.  Grandpa's Ranch `go west`: vnl_GoWestBust (AfterTextAndActions)
     moves Buster, so the new room must list him "in the western enclosure". */
  int    defer_look;
  int    look_pending;
  size_t look_pos;

  /* Index of the task whose <Actions> are currently executing (clsUserSession
     ExecuteSingleAction's `task` param), or -1.  Used to gate the Score variable
     so a task can only modify Score once (clsTask.Scored, vb:2144). */
  int    cur_score_ti;

  /* When the Look dance's two test renders differ (a random pick in the room
     view moved between them -- clsUserSession.vb:1200), FD pins the response to
     the FIRST render instead of deferring the raw aggregate message to Display.
     This carries that pinned text to the deferred-splice site (owned; NULL =
     not pinned, render at final state as usual). */
  char  *look_pinned;

  /* Set only while the System <RunImmediately> startup tasks run (before the
     title).  emit_completion then uses FD's bHasOutput (markup-aware) instead of
     the plain whitespace test, so a title-music task's `<audio ...> ` keeps its
     trailing space to join onto the centred title.  Off everywhere else, so the
     per-turn completion-message emission is byte-identical to before. */
  int    immediate_emit;

  /* clsUserSession htblResponsesPass dedup for an event-fired task chain.  FD
     runs an event's ExecuteTask through AttemptToExecuteTask, which keys every
     completion message by its rendered text and shows each once (vb:783/1295).
     Non-NULL only while an outermost event task and its SetTasks-Execute subtree
     run (installed in attempt_event_task), so a task reached twice within one
     event contributes its message once -- e.g. Pathway to Destruction's Task5
     "The End" banner, executed by both Task33 and its parent Task30.  On the
     direct command path the plural/movement resp_map already covers this; on the
     single-command path there is never a same-turn duplicate. */
  std::set<std::string> *ev_seen;

  /* clsUserSession htblResponsesFail for a SetTasks-Execute'd task whose
     restrictions fail WITH a message.  FD's ExecuteTask is a full
     AttemptToExecuteTask: the failing restriction's sRestrictionText is
     AddResponse'd (bPass=False), deduped by text, and displayed at the
     response flush ONLY when no pass response carries the same reference
     items (the pass-cancels-fail rule, clsUserSession.vb:804-849) -- in
     either order, since the cancellation happens at flush.  Examples (TBN):
       * dark-room `get dirt`: cl_TakeCharac passes silently, both its
         `Execute cl_TakeObject/TakeObjects (%objects%)` fail on the
         LightSources restriction -> "It is too dark to find the dirt."
         shown once (dedup);
       * `get grapnel`: the take override passes for the hooked grapnel (and
         hides it), the follow-up `Execute TakeObjects` fails Visible on the
         SAME object -> cancelled (pass after fail: Grandpa's flashlight,
         where TakeFromLazy fails "not on or inside another object!" BEFORE
         the plain take passes).
     So the direct path buffers Execute-fail messages here (with the object
     keys they were evaluated against) and flushes the survivors at the end
     of the scope (run_general / attempt_event_task).  When the plural/
     movement resp_map is active it handles fail responses itself instead. */
  struct exec_resp_scope *exec_scope;

  /* Deferred-completion sink pointer for the currently running command.  On the
     single-reference path this points at `display_defers` (below); a random draw
     inside an AggregateOutput completion message -- either a `<#OneOf/Rand#>`
     expression or a `%var[rand()]%` model-variable index -- is rendered as a
     `\004<idx>\004` sentinel and its body pushed to the sink, then drawn + spliced
     back at end-of-turn Display (a5run_flush_display_defers).  NULL on the
     plural/movement (resp map) path and outside a command. */
  std::vector<std::string> *comp_defers;

  /* Persistent per-turn Display-deferral sink (owned; cleared each finish_turn).
     Holds AggregateOutput-completion draws whose evaluation FD defers to the
     final Display loop -- i.e. AFTER the command's task, the LocationTrigger
     drain (drain_tasks_to_run) and the event tick.  Skybreak's dock: StorylineL1's
     `After` completion ends in %DisplayLocation% (the `%flavorskybreak[rand]%`
     draws); FD holds those to Display so the Skybreak location-trigger task's
     `SidequestE = RAND(1,10)` draws first.  Flushing here (not at run_general's
     end) reproduces that interleave.  Each body is either a frozen `<#..#>` sexpr
     or, tagged with a leading \001, a raw `%var[rand()]%` token re-resolved at
     flush. */
  std::vector<std::string> *display_defers;

  /* Single-level undo: a serialised a5run_save snapshot of the state as it stood
     BEFORE the last processed turn (NULL until the first turn is snapshotted, and
     consumed on undo so a double-undo fails).  Taken by a5run_snapshot from the
     frontend just before a5run_input; a5run_undo restores it.  Mirrors the ADRIFT
     4 game->undo slot and FrankenDrift's one-deep undo. */
  char  *undo_blob;
  size_t undo_len;
};

/* One SetTasks-Execute response scope (see exec_scope above). */
struct exec_resp_scope {
  std::set<std::string> pass_refs;   /* object keys of pass responses w/ output */
  std::set<std::string> fail_seen;   /* text dedup (htblResponsesFail keying)   */
  /* Pass-response text dedup (htblResponsesPass keying): FD shows each
     distinct response once per command, so a command that Executes two tasks
     with the same completion text -- or `Execute Look` twice -- prints it
     once.  Euripides' `press on` runs cl_ToCrawler11 AND cl_ToCrawler12
     (identical journey paragraphs), each ending in `Execute Look`. */
  std::set<std::string> pass_seen;
  /* buffered fail messages, in order: text + the object keys bound when the
     restriction failed.  Empty key set = a ref-less fail: cancelled iff any pass
     response occurred this command (FD's bAllMatch stays True vs the first pass),
     else shown. */
  std::vector<std::pair<std::string, std::vector<std::string>>> fails;
  /* Specific-override restriction fails, buffered on the DIRECT (single-ref)
     path with the task's POSITIONAL reference vector.  FD's flush
     (clsUserSession.vb:804-834) drops a fail only when some pass response's
     refs match it POSITIONALLY over the fail's full length -- a 2-ref fail
     (`get ashes` re-dispatched as TakeObjectsFromOthers [ashes, firepit])
     survives a 1-ref pass ("(from the firepit)" note, [ashes]), while AoK's
     `show talisman` 1-ref fail [talisman] is cancelled by the 1-ref passing
     override [talisman]. */
  std::vector<std::pair<std::string, std::vector<std::string>>> ov_fails;
  /* Positional ref signatures of every task frame that produced pass output on
     the direct path (execute_task_with_overrides), for the ov_fails rule. */
  std::vector<std::vector<std::string>> pass_sigs;
};

static inline int
streq (const char *a, const char *b)
{
  return a != NULL && b != NULL && strcmp (a, b) == 0;
}

/* Outcome of resolve_refine (reference resolution -> disambiguation). */
enum { RR_NOMATCH = 0, RR_OK, RR_FAIL, RR_AMBIG, RR_CANTSEE, RR_NOREF };

/* What the caller needs to raise (or carry forward) a disambiguation prompt. */
struct amb_info {
  std::string ref_name;          /* "object1", "character1", ...              */
  char        type;              /* 'o' / 'c'                                 */
  std::string ref_text;          /* the captured reference text typed         */
  std::vector<std::string> keys; /* surviving candidate keys                  */
  /* This ambiguity arose only because the *same task* also has an unmatched
     but *required* (`Must Exist`) reference, so in frankendrift the task does
     not match in the first pass and is found only in the second-chance
     (existence) pass.  There it sets sAmbTask, but a *different* task's
     0-item Must-Exist failure (GetGeneralTask) wins over it.  So unlike a
     first-pass ambiguity (which preempts any sNoRefTask), a second-chance
     ambiguity must YIELD to a clean no-reference fallback. */
  int         second_chance = 0;
};

/* ------------------------------------------------ cross-file entry points ---
   Functions shared between the turn-loop translation units.  Grouped by the
   file that defines each; every other file reaches them through here. */

/* a5run.cpp (core turn loop) */
int  msg_has_output (const char *m);
int  fd_has_output (a5_state_t *st, const char *raw);
std::vector<std::string> split_ws (const char *s);
std::string lower (const std::string &s);

/* a5run_events.cpp (events + walks runtime) */
void ev_init          (a5_run_t *run, sb_t *out);
void ev_tick_all      (a5_run_t *run, sb_t *out);
void drain_tasks_to_run (a5_run_t *run, sb_t *out);
void a5run_flush_display_defers (a5_run_t *run, sb_t *out);
void ev_on_task_completed (a5_run_t *run, const char *task_key, sb_t *out);

/* a5run_ref.cpp (reference resolution + multiple-object references) */
void bind_reference (a5_state_t *st, const char *group, const char *value,
                     const char *text);
int  obj_in_scope     (a5_state_t *st, const char *key);
int  obj_visible      (a5_state_t *st, const char *key);
int  obj_seen_p       (a5_state_t *st, const char *key);
int  char_visible     (a5_state_t *st, const char *key);
int  char_seen_p      (a5_state_t *st, const char *key);
int  char_name_usable (a5_state_t *st, const a5_character_t *c);
std::string guess_plural_from_noun (const std::string &n);
std::string amb_word (a5_state_t *st, const std::vector<std::string> &keys,
                     const std::string &ref_text, char type);
std::vector<std::string> possible_keys (a5_state_t *st,
                     const std::vector<std::string> &keys,
                     const std::string &input, char type);
int  resolve_plural (a5_run_t *run, const a5_task_t *t, const std::string &text,
                     amb_info *amb);
int  resolve_refine (a5_run_t *run, const a5_task_t *t, const a5_match_t *m,
                     const char *force_name, const char *force_key,
                     amb_info *amb);

/* a5run_action.cpp (action helpers, task dispatch, response aggregation,
   conversation, action execution, run one task) */
void run_task (a5_run_t *run, const a5_task_t *t, int depth, sb_t *out);
void run_general (a5_run_t *run, const a5_task_t *parent, const a5_match_t *m,
                  sb_t *out);
void update_seen (a5_state_t *st);
int  conv_contains_word (const std::string &sentence, const std::string &check);
void exec_scope_flush (a5_run_t *run, struct exec_resp_scope *sc, sb_t *out);

#endif
