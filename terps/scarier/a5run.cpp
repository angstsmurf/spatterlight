/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- turn loop and action executor.  See a5run.h.
 *
 * A port of the player-turn half of clsUserSession: GetGeneralTask (walk tasks
 * in priority order, match command, resolve references, refine by restrictions)
 * + ExecuteActions / ExecuteSingleAction (the core action set).  Reference
 * resolution (InputMatchesObject/Character) is the in-scope name match, refined
 * by the task's restrictions; when a reference still names several in-scope
 * candidates it raises the "Which X?" disambiguation prompt and resolves the
 * pick on the next turn (clsUserSession DisplayAmbiguityQuestion /
 * ResolveAmbiguity / PossibleKeys).
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
#include "a5sexpr.h"
#include "a5text.h"

int a5run_trace = 0;

/* ------------------------------------------------------------ string builder */

typedef struct { char *p; size_t len, cap; } sb_t;
static void sb_init (sb_t *b) { b->p = NULL; b->len = b->cap = 0; }
static void
sb_puts (sb_t *b, const char *s)
{
  size_t n;
  if (s == NULL) return;
  n = strlen (s);
  if (b->len + n + 1 > b->cap)
    {
      size_t cap = b->cap ? b->cap : 128;
      while (cap < b->len + n + 1) cap *= 2;
      b->p = (char *) realloc (b->p, cap);
      b->cap = cap;
    }
  memcpy (b->p + b->len, s, n);
  b->len += n;
  b->p[b->len] = '\0';
}
static char *sb_take (sb_t *b) { return b->p ? b->p : strdup (""); }
/* FrankenDrift's Global.pSpace: before appending the next Display() chunk, the
   accumulator gets two trailing spaces -- UNLESS it is empty or already ends in
   a newline (vbLf).  This is what joins a task's completion message and a
   turn-based event's message onto the same line ("...milk.  The postman...")
   while a message that ends in a newline keeps the next one on a fresh line.
   Scarier formerly forced a '\n' after every message, so multi-message turns
   diverged from FD; call sb_pspace before each message instead. */
static void
sb_pspace (sb_t *b)
{
  if (b->len > 0 && b->p[b->len - 1] != '\n')
    sb_puts (b, "  ");
}
/* Splice `s` into the buffer at byte offset `pos`, shifting the tail right.
   Used to render a deferred room view (emit_look) at the position it was skipped
   once the After children have updated world state. */
static void
sb_insert (sb_t *b, size_t pos, const char *s)
{
  size_t n;
  if (s == NULL || (n = strlen (s)) == 0) return;
  if (pos > b->len) pos = b->len;
  if (b->len + n + 1 > b->cap)
    {
      size_t cap = b->cap ? b->cap : 128;
      while (cap < b->len + n + 1) cap *= 2;
      b->p = (char *) realloc (b->p, cap);
      b->cap = cap;
    }
  memmove (b->p + pos + n, b->p + pos, b->len - pos);
  memcpy (b->p + pos, s, n);
  b->len += n;
  b->p[b->len] = '\0';
}
/* True when `m` has visible content (FD's bHasOutput, approximated): a message
   that renders to nothing but whitespace produces no output and is skipped (the
   stock cl_PAtStartOp "page" task has an empty Before message, for instance).
   Messages with real text are emitted verbatim -- their own trailing newline,
   if any, drives the pSpace line/paragraph break before the next message. */
static int
msg_has_output (const char *m)
{
  if (m == NULL) return 0;
  for (; *m; m++)
    if (*m != '\n' && *m != '\r' && *m != ' ' && *m != '\t')
      return 1;
  return 0;
}

/* ----------------------------------------------------------------- run state */

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
};

static int
streq (const char *a, const char *b)
{
  return a != NULL && b != NULL && strcmp (a, b) == 0;
}

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
  run->defer_look = 0;
  run->look_pending = 0;
  run->look_pos = 0;
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
  delete run->order;
  delete run->events;
  delete run->walks;
  delete run->known_words;
  delete run->tasks_to_run;
  delete run;
}

/* ------------------------------------------------------------ tokenisation */

static std::vector<std::string>
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

static std::string
lower (const std::string &s)
{
  std::string o = s;
  for (char &c : o) c = (char) tolower ((unsigned char) c);
  return o;
}

/* ---------------------------------------------------- reference resolution */

/* Collect an entity's matchable words (article + prefix + names/descriptors). */
static void
collect_words (const char *article, const char *prefix,
               const char **names, int n_names,
               std::vector<std::string> &allowed, std::vector<std::string> &nouns)
{
  if (article && article[0])
    allowed.push_back (lower (article));
  for (auto &w : split_ws (prefix))
    allowed.push_back (lower (w));
  for (int i = 0; i < n_names; i++)
    for (auto &w : split_ws (names[i]))
      { allowed.push_back (lower (w)); nouns.push_back (lower (w)); }
}

/* The input words from index `i` onward must equal exactly one whole name. */
static int
name_match_tail (const std::vector<std::string> &in, size_t i,
                 const char **names, int n_names)
{
  std::vector<std::string> rest (in.begin () + i, in.end ());
  if (rest.empty ())
    return 0;
  for (int n = 0; n < n_names; n++)
    {
      std::vector<std::string> nw = split_ws (names[n]);
      if (nw.size () != rest.size ())
        continue;
      int eq = 1;
      for (size_t k = 0; k < nw.size (); k++)
        if (lower (nw[k]) != lower (rest[k])) { eq = 0; break; }
      if (eq)
        return 1;
    }
  return 0;
}

/* Match the `(prefix_pi )?...(name_alt)` tail of the regex with backtracking:
   each remaining prefix word is independently optional (in order), then one
   whole name must consume the rest.  Crucially this BACKTRACKS -- a prefix word
   that also equals a name (e.g. the ID pass's prefix "ID" vs its name "id")
   must be skippable so the bare noun still resolves, exactly as .NET's regex
   engine does for FrankenDrift's `(ID )?(id|pass)` pattern. */
static int
name_match_prefix (const std::vector<std::string> &in, size_t i,
                   const std::vector<std::string> &pfx, size_t pi,
                   const char **names, int n_names)
{
  if (pi == pfx.size ())
    return name_match_tail (in, i, names, n_names);
  /* option 1: skip this prefix word. */
  if (name_match_prefix (in, i, pfx, pi + 1, names, n_names))
    return 1;
  /* option 2: consume it if the next input word matches. */
  if (i < in.size () && lower (in[i]) == lower (pfx[pi]))
    if (name_match_prefix (in, i + 1, pfx, pi + 1, names, n_names))
      return 1;
  return 0;
}

/* Match `text` against an entity the way clsObject/clsCharacter's
   sRegularExpressionString does: `^(article |the )?(prefix_i )?...(name_alt)$` --
   an optional article, each prefix word independently optional in order, then
   exactly one whole name (a multi-word name like "key rack" must appear in full,
   unlike the looser word-set words_match used for clarifiers/AmbWord).  The
   prefix/name tail is matched with backtracking (name_match_prefix). */
static int
name_match (const char *article, const char *prefix,
            const char **names, int n_names, const std::string &text)
{
  std::vector<std::string> in = split_ws (text.c_str ());
  if (in.empty ())
    return 0;
  std::vector<std::string> pfx = split_ws (prefix);
  /* without the optional leading article. */
  if (name_match_prefix (in, 0, pfx, 0, names, n_names))
    return 1;
  /* with the optional leading article (the entity's own, or "the") consumed. */
  {
    std::string w = lower (in[0]);
    std::string art = article ? lower (article) : "";
    if (w == "the" || (!art.empty () && w == art))
      if (name_match_prefix (in, 1, pfx, 0, names, n_names))
        return 1;
  }
  return 0;
}

static int
words_match (const std::vector<std::string> &allowed,
             const std::vector<std::string> &nouns, const std::string &text)
{
  std::vector<std::string> in = split_ws (text.c_str ());
  int noun_hit = 0;
  if (in.empty ())
    return 0;
  for (auto &w : in)
    {
      std::string lw = lower (w);
      int ok = 0;
      for (auto &a : allowed) if (a == lw) { ok = 1; break; }
      if (!ok) return 0;
      for (auto &no : nouns) if (no == lw) { noun_hit = 1; break; }
    }
  return noun_hit;
}

/* Collect the matchable words of an object / character key, so the same
   word-set drives both the initial match and the cross-turn clarifier. */
static void
object_words (const a5_object_t *o,
              std::vector<std::string> &allowed, std::vector<std::string> &nouns)
{
  collect_words (o->article, o->prefix, o->names, o->n_names, allowed, nouns);
}
/* Is the character's *proper name* usable as a reference word yet?  Mirrors
   clsCharacter.sRegularExpressionString: the proper name is excluded once the
   game defines a "Known" character property and this character does not (yet)
   have it -- i.e. an unintroduced NPC is addressable only by descriptor
   ("woman"), not by name ("susan").  A character with no descriptors, or a game
   with no Known property, always exposes the proper name. */
static int
char_name_usable (a5_state_t *st, const a5_character_t *c)
{
  int game_has_known = 0, i;
  if (c->n_descriptors == 0)
    return 1;
  for (i = 0; i < st->adv->n_propdefs; i++)
    if (streq (st->adv->propdefs[i].key, "Known")
        && streq (st->adv->propdefs[i].property_of, "Characters"))
      { game_has_known = 1; break; }
  if (!game_has_known)
    return 1;
  if (a5_prop_find (c->props, c->n_props, "Known") != NULL)
    return 1;
  return a5state_entity_prop (st, c->key, "Known") != NULL;
}

static void
character_words (a5_state_t *st, const a5_character_t *c,
                 std::vector<std::string> &allowed, std::vector<std::string> &nouns)
{
  /* Article + prefix apply to every noun ("young woman", "the woman"); the
     proper name is exposed only once the character is Known (see above). */
  collect_words (c->article, c->prefix, c->descriptors, c->n_descriptors,
                 allowed, nouns);
  if (char_name_usable (st, c) && c->name != NULL)
    {
      const char *one_name[1];
      one_name[0] = c->name;
      collect_words (c->article, c->prefix, one_name, 1, allowed, nouns);
    }
}

/* All object keys whose names match `text` (the full any-scope match set, as
   clsUserSession.InputMatchesObject builds -- no scope filter), ordered
   visible-first then seen then the rest.  The ordering is only a default-pick
   hint; resolve_refine narrows the set by the Applicable/Visible/Seen tiers. */
static std::vector<const char *>
resolve_object_candidates (a5_state_t *st, const std::string &text)
{
  std::vector<const char *> vis, seen, rest;
  const char *ploc = a5state_player_location (st);
  for (int i = 0; i < st->adv->n_objects; i++)
    {
      const a5_object_t *o = &st->adv->objects[i];
      if (!name_match (o->article, o->prefix, o->names, o->n_names, text))
        continue;
      if (ploc != NULL && a5state_object_visible_at_location (st, i, ploc, 0))
        vis.push_back (o->key);
      else if (st->obj_seen != NULL && st->obj_seen[i])
        seen.push_back (o->key);   /* known but not here now */
      else
        rest.push_back (o->key);
    }
  std::vector<const char *> all;
  all.insert (all.end (), vis.begin (), vis.end ());
  all.insert (all.end (), seen.begin (), seen.end ());
  all.insert (all.end (), rest.begin (), rest.end ());
  return all;
}

/* clsObject.GuessPluralFromNoun (defined below; shared with emit_cantsee). */
static std::string guess_plural_from_noun (const std::string &n);

/* Like name_match, but the candidate nouns are the objects' *plural* forms, so
   a plural reference ("balls"/"boxes"/"knives") matches each object whose noun
   pluralises to it (clsObject.sRegularExpressionString(bPlural:=True), whose arl
   is arlPlurals).  Article + prefix stay as for the singular. */
static int
name_match_plural (const a5_object_t *o, const std::string &text)
{
  std::vector<std::string> plurals;
  std::vector<const char *> pp;
  for (int i = 0; i < o->n_names; i++)
    plurals.push_back (guess_plural_from_noun (lower (o->names[i])));
  for (auto &s : plurals) pp.push_back (s.c_str ());
  return name_match (o->article, o->prefix,
                     pp.empty () ? NULL : &pp[0], (int) pp.size (), text);
}

/* All character keys whose names match `text`, visible-first (full any-scope
   set, mirroring InputMatchesCharacter). */
static std::vector<const char *>
resolve_character_candidates (a5_state_t *st, const std::string &text)
{
  std::vector<const char *> vis, rest;
  const char *ploc = a5state_player_location (st);
  for (int i = 0; i < st->adv->n_characters; i++)
    {
      const a5_character_t *c = &st->adv->characters[i];
      /* The character's matchable names are its descriptors plus -- only once it
         is Known -- its proper name (clsCharacter.sRegularExpressionString). */
      std::vector<const char *> names;
      for (int d = 0; d < c->n_descriptors; d++)
        names.push_back (c->descriptors[d]);
      if (char_name_usable (st, c) && c->name != NULL)
        names.push_back (c->name);
      if (!name_match (c->article, c->prefix,
                       names.empty () ? NULL : &names[0],
                       (int) names.size (), text))
        continue;
      if (streq (st->char_loc[i], ploc))
        vis.push_back (c->key);
      else
        rest.push_back (c->key);
    }
  std::vector<const char *> all;
  all.insert (all.end (), vis.begin (), vis.end ());
  all.insert (all.end (), rest.begin (), rest.end ());
  return all;
}

/* PossibleKeys: narrow a candidate list to those entities every word of the
   clarifier input matches (clsUserSession.PossibleKeys).  "the" always matches. */
static std::vector<std::string>
possible_keys (a5_state_t *st, const std::vector<std::string> &keys,
               const std::string &input, char type)
{
  std::vector<std::string> out;
  std::vector<std::string> words = split_ws (input.c_str ());
  for (auto &k : keys)
    {
      std::vector<std::string> allowed, nouns;
      if (type == 'o')
        { int oi = a5state_object_index (st, k.c_str ());
          if (oi < 0) continue;
          object_words (&st->adv->objects[oi], allowed, nouns); }
      else
        { int ci = a5state_character_index (st, k.c_str ());
          if (ci < 0) continue;
          character_words (st, &st->adv->characters[ci], allowed, nouns); }
      int all = 1;
      for (auto &w : words)
        {
          std::string lw = lower (w);
          int found = (lw == "the");
          for (auto &a : allowed) if (a == lw) { found = 1; break; }
          if (!found) { all = 0; break; }
        }
      if (all) out.push_back (k);
    }
  return out;
}

/*
 * Bind a captured reference under its "Referenced..." aliases (the resolved key)
 * plus a parallel "<alias>$text" carrying the raw typed text (for BeExactText).
 */
static void
bind_reference (a5_state_t *st, const char *group, const char *value,
                const char *text)
{
  std::string g = group;
  std::string base = g;
  std::string num;
  while (!base.empty () && isdigit ((unsigned char) base.back ()))
    { num.insert (num.begin (), base.back ()); base.pop_back (); }

  std::string cap = base;
  if (!cap.empty ()) cap[0] = (char) toupper ((unsigned char) cap[0]);
  /* "objects"/"characters" -> singular "Object"/"Character" stem too */
  std::string stem = cap;
  if (stem.size () > 1 && stem.back () == 's' && num.empty ()
      && (base == "objects" || base == "characters"))
    stem = stem.substr (0, stem.size () - 1);

  auto bind = [&](const std::string &alias) {
    a5state_bind_ref (st, alias.c_str (), value);
    if (text != NULL)
      a5state_bind_ref (st, (alias + "$text").c_str (), text);
  };
  bind ("Referenced" + stem + num);
  /* The bare "Referenced<Stem>" is the *first* reference (%object% == %object1%);
     a higher index (%object2%..) must not clobber it -- FD keeps ReferencedObject
     pinned to ref 1 so 2-specific overrides (e.g. PutSomeDry: Gunpowder1 + Keg)
     resolve per index. */
  if (num.empty () || num == "1") bind ("Referenced" + stem);
  if (num == "1") bind ("Referenced" + stem + "1");
  if (base == "objects")    bind ("ReferencedObjects");
  if (base == "characters") bind ("ReferencedCharacters");

  /* Track whether the *singular* %object%/%object1% (resp. %character%) text
     token should resolve.  In FrankenDrift GetReference("ReferencedObject")
     resolves a reference only when its ReferenceMatch is "object1"
     (clsUserSession.vb:3990); a plural %objects% reference has ReferenceMatch
     "objects", so %object%/%object1% in a message render EMPTY even though the
     plural's first key stays bound for override-key matching and the
     ReferencedObjects restriction path.  E.g. Axe of Kolt's give task R1 message
     "There is nobody here to give %TheObject[%object%]% to!" must render the
     singular as nothing -> "...give nothing to!".  We keep the binding but mark
     the slot plural-derived so a5text suppresses the singular token. */
  if (base == "object" && (num.empty () || num == "1"))
    st->ref_object1_plural = 0;
  else if (base == "objects")
    st->ref_object1_plural = 1;
  if (base == "character" && (num.empty () || num == "1"))
    st->ref_character1_plural = 0;
  else if (base == "characters")
    st->ref_character1_plural = 1;
}

/* The "Which <word>?" noun: the first word of the typed reference text that
   names every candidate (clsUserSession.AmbWord, vb:2656).

   FAITHFUL QUIRK: FD compares each input word `sWord` to the candidates' whole
   `arlNames` / ProperName / descriptors **case-sensitively** (`sWord = sName`),
   and returns Nothing (rendered "") when no input word is a name of *every*
   candidate.  So `buy ale` -- which matches two objects, one named "Ale" and one
   "ale" -- finds no common case-exact name for the lowercase input word "ale"
   (the "Ale" object fails), so AmbWord is empty and the cantsee renders
   "You can't see any !".  We mirror both: case-sensitive whole-name comparison
   and an empty fallback (NOT the raw text). */
static std::string
amb_word (a5_state_t *st, const std::vector<std::string> &keys,
          const std::string &ref_text, char type)
{
  for (auto &w : split_ws (ref_text.c_str ()))
    {
      int in_all = 1;
      for (auto &k : keys)
        {
          int hit = 0;
          if (type == 'o')
            { int oi = a5state_object_index (st, k.c_str ());
              if (oi >= 0)
                for (int n = 0; n < st->adv->objects[oi].n_names && !hit; n++)
                  if (w == st->adv->objects[oi].names[n]) hit = 1; }
          else
            { int ci = a5state_character_index (st, k.c_str ());
              if (ci >= 0)
                { const a5_character_t *c = &st->adv->characters[ci];
                  if (c->name != NULL && w == c->name) hit = 1;
                  for (int d = 0; d < c->n_descriptors && !hit; d++)
                    if (w == c->descriptors[d]) hit = 1; } }
          if (!hit) { in_all = 0; break; }
        }
      if (in_all) return w;
    }
  return "";
}

/* clsObject.GuessPluralFromNoun: a naive English pluraliser (the quirky FRotZ
   one, "feet"->"feets" and all -- ported verbatim so the canned "You can't see
   any <plural>!" message byte-matches the Runner/FrankenDrift). */
static std::string
guess_plural_from_noun (const std::string &n)
{
  if (n.empty ())
    return n;
  static const char *same[] = { "deer","fish","cod","mackerel","trout","moose",
    "sheep","swine","aircraft","blues","cannon" };
  for (const char *s : same) if (n == s) return n;
  if (n == "ox")    return "oxen";
  if (n == "cow")   return "kine";
  if (n == "child") return "children";
  if (n == "foot")  return "feet";
  if (n == "goose") return "geese";
  if (n == "louse") return "lice";
  if (n == "mouse") return "mice";
  if (n == "tooth") return "teeth";
  size_t L = n.size ();
  if (L <= 2)
    return n + "s";
  std::string l3 = n.substr (L - 3), l2 = n.substr (L - 2), l1 = n.substr (L - 1);
  if (l3 == "man") return n.substr (0, L - 2) + "en";
  if (l3 == "ies") return n;
  if (l2 == "sh" || l2 == "ss" || l2 == "ch") return n + "es";
  if (l2 == "ge" || l2 == "se") return n + "s";
  if (l2 == "ex") return n.substr (0, L - 2) + "ices";
  if (l2 == "is") return n.substr (0, L - 2) + "es";
  if (l2 == "um") return n.substr (0, L - 2) + "a";
  if (l2 == "us") return n.substr (0, L - 2) + "i";
  const char *cons = "bcdfghjklmnpqrstvwxz";
  if (l1 == "f")
    {
      if (n == "dwarf" || n == "hoof" || n == "roof") return n + "s";
      return n.substr (0, L - 1) + "ves";
    }
  if (l1 == "o" && strchr (cons, n[L - 2]) != NULL) return n + "es";
  if (l1 == "x") return n + "es";
  if (l1 == "y" && strchr (cons, n[L - 2]) != NULL) return n.substr (0, L - 1) + "ies";
  if (l1 == "s") return n;
  return n + "s";
}

/* Is object `key` in the player's scope this turn (the visibility proxy
   resolve_object_candidates uses)? */
static int
obj_in_scope (a5_state_t *st, const char *key)
{
  const char *ploc = a5state_player_location (st);
  int i = a5state_object_index (st, key);
  return ploc != NULL && i >= 0
         && a5state_object_visible_at_location (st, i, ploc, 0);
}

/* Visible/seen predicates the refine tiers use (clsObject.IsVisibleTo /
   SeenBy and clsCharacter.CanSeeCharacter / SeenBy), keyed by entity key. */
static int
obj_visible (a5_state_t *st, const char *key) { return obj_in_scope (st, key); }

static int
obj_seen_p (a5_state_t *st, const char *key)
{
  int i = a5state_object_index (st, key);
  return i >= 0 && st->obj_seen != NULL && st->obj_seen[i];
}

static int
char_visible (a5_state_t *st, const char *key)
{
  int i = a5state_character_index (st, key);
  return i >= 0 && streq (st->char_loc[i], a5state_player_location (st));
}

static int
char_seen_p (a5_state_t *st, const char *key)
{
  int i = a5state_character_index (st, key);
  return i >= 0 && st->char_seen != NULL && st->char_seen[i];
}

/* -------------------------------------------- multiple-object references */

static std::string
trim_ws (const std::string &s)
{
  size_t a = s.find_first_not_of (" \t");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of (" \t");
  return s.substr (a, b - a + 1);
}

/*
 * InputMatchesObject (clsUserSession.vb:5378): every object whose name matches
 * `input` is recorded.  For a singular reference the matches accumulate into one
 * item (a candidate set, disambiguated later); for a *plural* reference each
 * matching object becomes its own item (so "take balls" grabs every ball).  In
 * the `excepts` pass the matching keys are removed from the items instead.
 * `items` is the per-item candidate-key list being built; returns true if any
 * object matched.
 */
static bool
match_object_one (a5_state_t *st, const std::string &input,
                  std::vector<std::vector<std::string>> &items,
                  int item_num, bool excepts, bool plural)
{
  bool result = false, added = false;
  if (item_num == 0 && plural)
    item_num = -1;
  for (int i = 0; i < st->adv->n_objects; i++)
    {
      const a5_object_t *o = &st->adv->objects[i];
      int hit = plural ? name_match_plural (o, input)
                       : name_match (o->article, o->prefix, o->names,
                                     o->n_names, input);
      if (!hit)
        continue;
      result = true;
      if (!excepts)
        {
          if (plural) item_num++;
          if (plural || !added)
            { if ((int) items.size () <= item_num) items.resize (item_num + 1); }
          added = true;
          if (item_num >= 0 && item_num < (int) items.size ())
            items[item_num].push_back (o->key);
        }
      else
        {
          for (auto &itm : items)
            itm.erase (std::remove (itm.begin (), itm.end (),
                                    std::string (o->key)), itm.end ());
        }
    }
  if (excepts)
    items.erase (std::remove_if (items.begin (), items.end (),
                  [] (const std::vector<std::string> &v) { return v.empty (); }),
                 items.end ());
  return result;
}

/* Every object the player has seen (clsObject.SeenBy / htblObjects.SeenBy) ->
   one single-candidate item each, in model order; the source of a bare "all". */
static void
expand_all_objects (a5_state_t *st, std::vector<std::vector<std::string>> &items)
{
  for (int i = 0; i < st->adv->n_objects; i++)
    if (st->obj_seen != NULL && st->obj_seen[i])
      items.push_back (std::vector<std::string> (1, st->adv->objects[i].key));
}

/*
 * InputMatchesObjects (clsUserSession.vb:5305): parse the plural-reference
 * grammar into per-item candidate sets -- "all" / "all <plural>",
 * "X and Y" / comma lists, and a trailing "... except/but/apart from ...".
 * `had_all` is set when a bare/refined "all" appeared (drives the FailOverride).
 * `hard_fail` is set when the input parsed as an explicit "X and Y" / comma
 * multi-reference but a *chunk* named no object: frankendrift's and-form path
 * (`If Not InputMatchesObject(...) Then Return False`, vb:5371) returns False
 * outright and -- unlike a single-noun no-match -- does NOT fall through to the
 * second-chance `HasObjectExistRestriction` fallback, so the task does not match
 * at all (-> "Sorry, I didn't understand that command.").
 * Returns true if the text resolved to at least one item.
 */
static bool
match_objects (a5_state_t *st, const std::string &input_raw,
               std::vector<std::vector<std::string>> &items,
               bool excepts, bool plural, int *had_all, int *hard_fail = NULL)
{
  std::string input = trim_ws (input_raw);
  if (input.empty ())
    return false;

  if (!plural)
    {
      /* Split off a trailing "... except|but|apart from <objects2>" (the regex
         is RightToLeft, so the *last* connector wins). */
      std::string head = input, excepts_text;
      static const char *conns[] = { " except ", " but ", " apart from ", NULL };
      std::string lin = lower (input);
      size_t best = std::string::npos, best_len = 0;
      for (int i = 0; conns[i]; i++)
        {
          size_t p = lin.rfind (conns[i]);
          if (p != std::string::npos && (best == std::string::npos || p > best))
            { best = p; best_len = strlen (conns[i]); }
        }
      if (best != std::string::npos)
        { head = trim_ws (input.substr (0, best));
          excepts_text = trim_ws (input.substr (best + best_len)); }

      std::string lhead = lower (head);
      bool head_ok = true;
      if (lhead == "all")
        { if (had_all) *had_all = 1; expand_all_objects (st, items); }
      else if (lhead.rfind ("all ", 0) == 0)
        { if (had_all) *had_all = 1;
          if (!match_objects (st, head.substr (4), items, false, true, had_all))
            head_ok = false; }                         /* GoTo NextCheck */
      else if (!head.empty ())
        { if (!match_objects (st, head, items, excepts, true, had_all))
            head_ok = false; }                         /* GoTo NextCheck */

      if (head_ok)
        {
          if (!excepts_text.empty ())
            match_objects (st, excepts_text, items, true, false, had_all);
          return true;
        }

      /* NextCheck: "(<csv>, )* <o2> and <o3>". */
      size_t andp = lin.rfind (" and ");
      if (andp != std::string::npos)
        {
          std::string left = trim_ws (input.substr (0, andp));
          std::string o3   = trim_ws (input.substr (andp + 5));
          int item_num = 0;
          /* split `left` on ", " into csv items + o2 */
          size_t start = 0;
          while (start <= left.size ())
            {
              size_t comma = left.find (", ", start);
              std::string tok = trim_ws (comma == std::string::npos
                                  ? left.substr (start)
                                  : left.substr (start, comma - start));
              if (!tok.empty ()
                  && !match_object_one (st, tok, items, item_num++, excepts, false))
                { if (hard_fail) *hard_fail = 1; return false; }
              if (comma == std::string::npos) break;
              start = comma + 2;
            }
          if (!match_object_one (st, o3, items, item_num++, excepts, false))
            { if (hard_fail) *hard_fail = 1; return false; }
          return true;
        }
    }

  return match_object_one (st, input, items, 0, excepts, plural);
}

/* Outcome of resolve_refine. */
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

/*
 * Resolve a matched command's references in scope, refining ambiguous
 * object/character references by the task's own restrictions, mirroring
 * clsUserSession.RefineMatchingPossibilitesUsingRestrictions + the post-refine
 * ambiguity check (GetGeneralTask).  Each reference is bound (first surviving
 * candidate) so restriction/action evaluation can read it.
 *
 *   RR_NOMATCH  a non-object/character reference failed to parse -> next task
 *   RR_OK       every reference resolved uniquely and restrictions pass
 *   RR_FAIL     restrictions fail for the (uniquely) resolved references
 *   RR_AMBIG    a reference still has >1 candidate, >=1 visible -> *amb (prompt)
 *   RR_CANTSEE  a reference still has >1 candidate, none visible -> *amb
 *   RR_NOREF    an object/character reference named nothing -> *amb (sNoRefTask:
 *               run the task with the reference unresolved so a `Must Exist`
 *               restriction surfaces "Sorry, I'm not sure which object ...")
 *
 * Candidates begin as the full any-scope name-match set (InputMatchesObject),
 * then a three-tier refine (Applicable / Visible / Seen), each tier resetting a
 * reference to its full original set when it empties -- a port of
 * clsUserSession.RefineMatchingPossibilitesUsingRestrictions + the post-refine
 * count check (GetGeneralTask / DisplayAmbiguityQuestion).
 *
 * `force_name`/`force_key`: when re-running a remembered task to resolve a
 * pending ambiguity, pin that reference to the chosen key.
 */

/* Pick the key that represents an item: the first currently-visible candidate,
   else the first seen, else simply the first (the visible-first default pick
   used throughout resolve_refine). */
static std::string
pick_item_key (a5_state_t *st, const std::vector<std::string> &cands)
{
  for (auto &k : cands) if (obj_visible (st, k.c_str ())) return k;
  for (auto &k : cands) if (obj_seen_p (st, k.c_str ())) return k;
  return cands.empty () ? std::string () : cands[0];
}

/* Re-order a candidate list visible-first, then seen, then the rest (the same
   default-pick ordering resolve_object_candidates produces), so a single-item
   %objects% reference refines and disambiguates exactly like a bare %object%. */
static std::vector<std::string>
order_visible_first (a5_state_t *st, const std::vector<std::string> &keys)
{
  std::vector<std::string> vis, seen, rest, out;
  for (auto &k : keys)
    {
      if (obj_visible (st, k.c_str ()))     vis.push_back (k);
      else if (obj_seen_p (st, k.c_str ())) seen.push_back (k);
      else                                  rest.push_back (k);
    }
  out.insert (out.end (), vis.begin (),  vis.end ());
  out.insert (out.end (), seen.begin (), seen.end ());
  out.insert (out.end (), rest.begin (), rest.end ());
  return out;
}

/*
 * Resolve the plural %objects% reference (the only reference whose base is
 * "objects") to a concrete item list -- InputMatchesObjects + the single-
 * reference case of RefineMatchingPossibilitesUsingRestrictions.  Parse the
 * all/and/comma/except grammar, then keep the items whose chosen key passes the
 * task's restrictions (with the other references already bound); if at least one
 * passes those are the items, else reset to the full list (so the task runs and
 * fails) and, when the command contained "all", arm the FailOverride.
 *
 * Stores the resolved keys in st->ref_items and binds ReferencedObject = the
 * first item and ReferencedObjects = the "key1|key2|..." pipe list (so the OO /
 * text engine renders the whole set, and the per-item action loop iterates).
 * Returns RR_OK (>=1 passing/forced item), RR_FAIL (items exist, none pass), or
 * RR_NOMATCH (the text named no object at all).
 */
static int
resolve_plural (a5_run_t *run, const a5_task_t *t, const std::string &text,
                amb_info *amb)
{
  a5_state_t *st = run->st;
  std::vector<std::vector<std::string>> items;
  int had_all = 0;

  if (!match_objects (st, text, items, false, false, &had_all))
    return RR_NOMATCH;

  /* Per-item ambiguity (clsUserSession.RefineMatchingPossibilitesUsingRestrictions
     + GetGeneralTask).  A single noun (no "and"/comma) that name-matches several
     objects collapses into one Item holding >1 MatchingPossibility -- e.g. the
     "leathers" chunk of `wear leathers and boots` matches two "riding leathers"
     objects, so its Item has Count 2.  (The bare-plural path -- "all", "all
     <plural>", or a plural-noun match -- instead spreads each object into its own
     single-possibility Item, so it is never ambiguous.)  FD refines every Item
     through the Applicable/Visible/Seen tiers, resetting the whole reference to
     its original Items whenever a tier empties *all* of them; an Item that is
     never reduced to a unique key stays Count>1, and GetGeneralTask then raises
     it as sAmbTask (DisplayAmbiguityQuestion -> "Which <noun>?" if any candidate
     is visible, else "You can't see any <plural>!").  So mirror the tiered refine
     over the per-item candidate sets and, if an Item survives ambiguous, surface
     it here -- the resolved/none-passed machinery below only runs when every Item
     is unique. */
  {
    std::vector<std::vector<std::string>> cur = items;

    /* Applicable: keep, per Item, the candidates that pass the restrictions with
       that single key bound (a key already kept in an earlier Item is not added
       again -- FD's global lAdded).  Items with no surviving candidate are
       dropped; if *every* Item is dropped, the whole reference resets to its
       original Items (so a Count>1 Item is preserved). */
    {
      std::vector<std::vector<std::string>> nr;
      std::vector<std::string> added;
      for (auto &item : cur)
        {
          std::vector<std::string> out;
          for (auto &k : item)
            {
              bind_reference (st, "objects", k.c_str (), text.c_str ());
              if (a5restr_pass (st, t->restrictions)
                  && std::find (added.begin (), added.end (), k) == added.end ())
                { out.push_back (k); added.push_back (k); }
            }
          if (!out.empty ()) nr.push_back (out);
        }
      cur = nr.empty () ? items : nr;
    }

    /* Visible, then Seen: drop non-visible / non-seen candidates per Item; reset
       to the original Items whenever a tier empties them all. */
    for (int tier = 0; tier < 2; tier++)
      {
        std::vector<std::vector<std::string>> nr;
        for (auto &item : cur)
          {
            std::vector<std::string> out;
            for (auto &k : item)
              if (tier == 0 ? obj_visible (st, k.c_str ())
                            : obj_seen_p  (st, k.c_str ()))
                out.push_back (k);
            if (!out.empty ()) nr.push_back (out);
          }
        cur = nr.empty () ? items : nr;
      }

    /* The first Item still holding >1 candidate is the ambiguity (FD scans Items
       in order; GetGeneralTask sets sAmbTask for the first Count>1). */
    for (auto &item : cur)
      if (item.size () > 1)
        {
          int any_vis = 0;
          for (auto &k : item)
            if (obj_visible (st, k.c_str ())) { any_vis = 1; break; }
          if (amb != NULL)
            { amb->ref_name = "objects"; amb->type = 'o';
              amb->ref_text = text; amb->keys = item;
              amb->second_chance = 0; }
          return any_vis ? RR_AMBIG : RR_CANTSEE;
        }
  }

  /* Choose one key per item; keep the items whose key passes the restrictions. */
  std::vector<std::string> chosen, all_keys;
  for (auto &cand : items)
    {
      std::string pick = pick_item_key (st, cand);
      if (pick.empty ()) continue;
      all_keys.push_back (pick);
      bind_reference (st, "objects", pick.c_str (), text.c_str ());
      if (a5restr_pass (st, t->restrictions))
        chosen.push_back (pick);
    }

  if (all_keys.empty ())
    {
      if (had_all && t->fail_override != NULL)
        { run->pending_failover = t->fail_override; return RR_FAIL; }
      return RR_NOMATCH;
    }

  int none_passed = chosen.empty ();
  if (none_passed)
    {
      /* No item passed.  A "get all"-style command shows the FailOverride.
         Otherwise the task runs with the whole (reset) reference set and its
         restrictions decide the message -- a genuine plural %objects% reference
         is NEVER an out-of-scope "You can't see any <plural>!" ambiguity: FD's
         InputMatchesObject spreads each matching object into its own Item with a
         single MatchingPossibility (clsUserSession.vb:5387-5391), so no Item ever
         holds >1 possibility and DisplayAmbiguityQuestion (which needs Count>1)
         is unreachable from a plural.  Instead GetGeneralTask runs the task and
         PassRestrictions surfaces the first failing restriction's message --
         e.g. Axe of Kolt's `give planks` (nobody present) -> the give task's
         "There is nobody here to give nothing to!", `take seeds` (never seen) ->
         "Sorry, I'm not sure which object you are trying to take.".  (The "You
         can't see any <plural>!" message proper comes only from a *singular*
         %object% reference whose one Item name-matched >1 objects -- the
         resolve_refine RR_CANTSEE path, e.g. `press button`.)

         The reset is FD's tiered fallback, not a blunt reset to the full set:
         when the Applicable tier empties the whole reference, FD resets to the
         full set and refines by Visible (each emptied single-possibility item is
         dropped, clsUserSession.vb:5848-5912); only if Visible empties *every*
         item does it reset again and refine by Seen; only if Seen empties
         everything too does the full set survive (vb:5914-5959).  So the
         surviving set is the Visible subset, else the Seen subset, else all --
         which is why `show documents` (travel documents in scope, a second
         out-of-scope "documents" part of distant cabinets) renders only the
         visible "travel documents" in the "take ... out of whatever it is in
         first" message, not both. */
      std::vector<std::string> vis, seen;
      for (auto &k : all_keys)
        {
          if (obj_visible (st, k.c_str ()))  vis.push_back (k);
          if (obj_seen_p  (st, k.c_str ()))  seen.push_back (k);
        }
      if (!vis.empty ())       chosen = vis;
      else if (!seen.empty ()) chosen = seen;
      else                     chosen = all_keys;
    }

  /* De-duplicate, preserving order (a noun and "all" may name an object twice). */
  std::vector<std::string> uniq;
  for (auto &k : chosen)
    if (std::find (uniq.begin (), uniq.end (), k) == uniq.end ())
      uniq.push_back (k);

  st->n_ref_items = 0;
  st->ref_items_type = 'o';
  std::string pipe;
  for (auto &k : uniq)
    {
      const a5_object_t *o = a5model_object (st->adv, k.c_str ());
      if (o == NULL) continue;                 /* stable model-key pointer */
      if (st->n_ref_items < A5_MAX_ITEMS)
        st->ref_items[st->n_ref_items++] = o->key;
      if (!pipe.empty ()) pipe += "|";
      pipe += o->key;
    }
  if (st->n_ref_items == 0)
    return RR_NOMATCH;

  bind_reference (st, "objects", st->ref_items[0], text.c_str ());
  a5state_bind_ref (st, "ReferencedObjects", pipe.c_str ());

  if (none_passed)
    {
      if (had_all && t->fail_override != NULL)
        run->pending_failover = t->fail_override;
      return RR_FAIL;
    }
  return RR_OK;
}

static int
resolve_refine (a5_run_t *run, const a5_task_t *t, const a5_match_t *m,
                const char *force_name, const char *force_key, amb_info *amb)
{
  a5_state_t *st = run->st;
  struct rref { std::string name, text; char type;
                std::vector<std::string> orig, keys; };
  std::vector<rref> refs;
  int have_noref = 0;
  rref noref_r;
  int plural_idx = -1;
  std::string plural_text;

  a5state_clear_refs (st);
  run->pending_failover = NULL;

  /* Pass 1: gather full any-scope candidate sets; bind the default of each. */
  for (int i = 0; i < m->n_refs; i++)
    {
      rref r;
      r.name = m->ref_name[i];
      r.text = m->ref_text[i];
      std::string base = r.name;
      while (!base.empty () && isdigit ((unsigned char) base.back ()))
        base.pop_back ();

      /* The plural %objects% reference.  Only a *genuine* multiple-object input
         ("all", "X and Y", a comma list, or a plural noun matching several
         objects) is deferred to resolve_plural; a single-object input is bound
         here as an ordinary object reference so the existing refine / ambiguity
         / "can't see any" / no-ref semantics are byte-identical to a bare
         %object% (clsUserSession.InputMatchesObjects yields one item in that
         case, which RefineMatchingPossibilitesUsingRestrictions then treats like
         any single reference). */
      if (base == "objects")
        {
          std::vector<std::vector<std::string>> items;
          int had_all = 0, hard_fail = 0;
          bool ok = match_objects (st, r.text, items, false, false, &had_all,
                                   &hard_fail);
          /* An explicit "X and Y" / comma list one of whose chunks named no
             object: frankendrift returns False without the second-chance
             existence fallback, so the command does not match this task at all
             (no sNoRefTask) -- e.g. `get fleetwind saddle and fleetwind bridle`,
             where neither chunk names an object, is "I didn't understand", not
             the take task's "...trying to take." no-reference message. */
          if (hard_fail)
            return RR_NOMATCH;
          /* A genuine multi-object input *or* any "all" command (even one that
             expands to zero/one seen object) goes through resolve_plural, so a
             "get all" with nothing takeable surfaces the task's FailOverride
             ("There is nothing worth taking here.") rather than the single-ref
             no-reference message. */
          if (ok && (items.size () > 1 || had_all))
            { plural_idx = i; plural_text = r.text; continue; }   /* multi/all */
          r.type = 'o';
          if (ok && items.size () == 1)
            r.orig = order_visible_first (st, items[0]);
          else
            { std::vector<const char *> c =
                resolve_object_candidates (st, r.text);
              for (const char *k : c) r.orig.push_back (k); }
          if (r.orig.empty ())
            {
              /* The reference names no object.  A task only "matches" such input
                 if it has an Object `Must Exist` restriction (clsUserSession's
                 second-chance pass, gated on HasObjectExistRestriction); else the
                 command simply does not match this task. */
              if (have_noref)
                continue;
              if (!a5restr_has_exist (t->restrictions, 'o'))
                return RR_NOMATCH;
              have_noref = 1; noref_r = r;
              continue;
            }
          r.keys = r.orig;
          bind_reference (st, r.name.c_str (), r.keys[0].c_str (),
                          r.text.c_str ());
          refs.push_back (r);
          continue;
        }

      if (base == "object" || base == "item")
        r.type = 'o';
      else if (base == "character" || base == "characters")
        r.type = 'c';
      else if (base == "direction")
        { const char *d = a5parse_canonical_direction (r.text.c_str ());
          if (d == NULL) return RR_NOMATCH;
          bind_reference (st, r.name.c_str (), d, r.text.c_str ());
          continue; }
      else /* text, number, location */
        { bind_reference (st, r.name.c_str (), r.text.c_str (), r.text.c_str ());
          continue; }

      if (force_name != NULL && r.name == force_name)
        { r.orig.assign (1, force_key); r.keys = r.orig; }
      else
        {
          std::vector<const char *> c = (r.type == 'o')
            ? resolve_object_candidates (st, r.text)
            : resolve_character_candidates (st, r.text);
          if (c.empty ())
            {
              /* The reference names no entity at all.  A task only "matches"
                 such input if it has a `Must Exist` restriction of this
                 reference's type (clsUserSession's second-chance pass, gated on
                 HasObjectExistRestriction / HasCharacterExistRestriction); then
                 it is remembered as the no-reference fallback (sNoRefTask) and
                 the reference is left unbound so the caller can surface its
                 Must-Exist message.  Without such a restriction the command does
                 not match this task at all. */
              if (have_noref)
                continue;
              if (!a5restr_has_exist (t->restrictions, r.type))
                return RR_NOMATCH;
              have_noref = 1; noref_r = r;
              continue;
            }
          for (const char *k : c) r.orig.push_back (k);
          r.keys = r.orig;
        }
      bind_reference (st, r.name.c_str (), r.keys[0].c_str (), r.text.c_str ());
      refs.push_back (r);
    }

  /* A reference matched nothing -> the whole task is deferred as the no-reference
     fallback (GetGeneralTask sNoRefTask).  But the unmatched reference does NOT
     preempt the *other* references' ambiguity resolution: in frankendrift an
     unmatched-but-Must-Exist reference is a second-chance match that adds a
     zero-Item NewReference (InputMatchesObject returns True via
     HasObjectExistRestriction but appends no Item), so the post-refine loop simply
     skips it while a *sibling* reference that is ambiguous-and-invisible still sets
     sAmbTask and wins ("You can't see any oil!" for `pour oil on me`, where object1
     "oil" is out-of-scope-ambiguous and object2 "me" names no object).  So fall
     through to the refine + ambiguity pass on the matched refs and only return the
     no-reference fallback below if none of them is ambiguous. */

  /* Pass 2: three-tier refinement (Applicable / Visible / Seen).  Each tier that
     empties a reference resets it to the full original set; a tier that leaves
     >1 falls through to the next.  Single-candidate references are already
     unique (clsUserSession's reset-on-empty makes refining them a no-op). */
  for (auto &r : refs)
    {
      if (r.keys.size () <= 1)
        continue;

      /* Tier 1: Applicable -- the candidates that pass the task's restrictions.
         frankendrift evaluates candidates in *model order* (the order
         InputMatchesObject/Character add them to MatchingPossibilities), and the
         last PassRestrictions call leaves sRestrictionText set to its deciding
         restriction's Message (st->restriction_text).  That carried text is what
         a later malformed-bracket task displays, so iterate in model order here
         even though the surviving set is kept in Scarier's visible-first order
         (the default-pick hint) -- only the *last evaluated* candidate, hence
         st->restriction_text, depends on this. */
      std::vector<std::string> eval_order = r.keys;
      std::stable_sort (eval_order.begin (), eval_order.end (),
        [&] (const std::string &a, const std::string &b) {
          int ia = (r.type == 'o') ? a5state_object_index (st, a.c_str ())
                                   : a5state_character_index (st, a.c_str ());
          int ib = (r.type == 'o') ? a5state_object_index (st, b.c_str ())
                                   : a5state_character_index (st, b.c_str ());
          return ia < ib;
        });
      std::vector<std::string> passset;
      for (auto &k : eval_order)
        {
          bind_reference (st, r.name.c_str (), k.c_str (), r.text.c_str ());
          if (a5restr_pass (st, t->restrictions))
            passset.push_back (k);
        }
      std::vector<std::string> pass;            /* survivors, visible-first */
      for (auto &k : r.keys)
        if (std::find (passset.begin (), passset.end (), k) != passset.end ())
          pass.push_back (k);
      int more;
      if (pass.empty ())          { r.keys = r.orig; more = 1; }
      else if (pass.size () > 1)  { r.keys = pass;   more = 1; }
      else                        { r.keys = pass;   more = 0; }

      /* Tier 2: Visible. */
      if (more)
        {
          std::vector<std::string> vis;
          for (auto &k : r.keys)
            if (r.type == 'o' ? obj_visible (st, k.c_str ())
                              : char_visible (st, k.c_str ()))
              vis.push_back (k);
          if (vis.empty ())          { r.keys = r.orig; more = 1; }
          else if (vis.size () > 1)  { r.keys = vis;    more = 1; }
          else                       { r.keys = vis;    more = 0; }
        }

      /* Tier 3: Seen. */
      if (more)
        {
          std::vector<std::string> sn;
          for (auto &k : r.keys)
            if (r.type == 'o' ? obj_seen_p (st, k.c_str ())
                              : char_seen_p (st, k.c_str ()))
              sn.push_back (k);
          r.keys = sn.empty () ? r.orig : sn;
        }

      bind_reference (st, r.name.c_str (), r.keys[0].c_str (), r.text.c_str ());
    }

  /* Is the unmatched reference genuinely *required* by this task -- i.e. is its
     `Must Exist` restriction actually evaluated within the BracketSequence?  If so
     the no-reference fallback wins outright (FD surfaces the task's Must-Exist
     message, e.g. `blow dart at <absent>` -> "Sorry, I'm not sure which object you
     are trying to blow.", `hang amulet on <absent>` -> "...trying to hang...").  If
     the Must Exist is truncated out of the bracket, the reference is optional and
     does NOT block the task, so a *sibling* reference's out-of-scope ambiguity
     surfaces instead (`pour oil on me`, whose pour task's `#A#A#` omits object2's
     Exist -> "You can't see any oil!"). */
  int noref_required = 0;
  if (have_noref)
    {
      std::string nm = noref_r.name, num;
      while (!nm.empty () && isdigit ((unsigned char) nm.back ()))
        { num.insert (num.begin (), nm.back ()); nm.pop_back (); }
      std::string alias = std::string ("Referenced")
        + (noref_r.type == 'c' ? "Character" : "Object") + num;
      noref_required =
        a5restr_exist_evaluated (t->restrictions, alias.c_str ());
    }
  if (noref_required)
    {
      /* The task has an unmatched but *required* reference, so in frankendrift
         it never matches in the first pass; it is found only in the
         second-chance (existence) pass.  If a *sibling* reference is itself
         ambiguous (>1 candidate), FD's second-chance pass sets sAmbTask for
         this task but a different task's clean 0-item Must-Exist failure
         (GetGeneralTask) wins.  So surface the sibling ambiguity flagged as
         `second_chance` -- the caller yields it to a clean no-reference
         fallback (e.g. `tie wire to girder`: TieObjectT's "wire" is out-of-
         scope-ambiguous and "girder" is the required unmatched ref, so the
         simpler `[tie/bind/secure] %object%` task's "...trying to
         tie/bind/secure." wins over TieObjectT's "...trying to tie.").  When
         every sibling resolves uniquely, the required no-reference fallback
         still wins outright (`blow dart at <absent>` -> "...trying to
         blow."). */
      for (auto &r : refs)
        if (r.keys.size () > 1)
          {
            int any_vis = 0;
            for (auto &k : r.keys)
              if (r.type == 'o' ? obj_visible (st, k.c_str ())
                                : char_visible (st, k.c_str ()))
                { any_vis = 1; break; }
            if (amb != NULL)
              { amb->ref_name = r.name; amb->type = r.type;
                amb->ref_text = r.text; amb->keys = r.keys;
                amb->second_chance = 1; }
            return any_vis ? RR_AMBIG : RR_CANTSEE;
          }
      if (amb != NULL)
        { amb->ref_name = noref_r.name; amb->type = noref_r.type;
          amb->ref_text = noref_r.text; amb->keys.clear (); }
      return RR_NOREF;
    }

  /* Post-refine: a reference left with >1 candidate is an ambiguity.  The Runner
     prompts "Which X?" when at least one candidate is currently visible, else it
     answers "You can't see any <plural>!" (DisplayAmbiguityQuestion bCanSeeAny).
     This precedes the (non-required) no-reference fallback below: a sibling
     reference's ambiguity (sAmbTask) wins over an unmatched optional reference. */
  for (auto &r : refs)
    if (r.keys.size () > 1)
      {
        int any_vis = 0;
        for (auto &k : r.keys)
          if (r.type == 'o' ? obj_visible (st, k.c_str ())
                            : char_visible (st, k.c_str ()))
            { any_vis = 1; break; }
        if (amb != NULL)
          { amb->ref_name = r.name; amb->type = r.type;
            amb->ref_text = r.text; amb->keys = r.keys;
            /* If this task ALSO had a reference that named nothing, it matched
               only via frankendrift's second-chance (existence) pass -- so its
               ambiguity is a second-chance sAmbTask, which a *different* task's
               clean no-reference / failing-with-output result (GetGeneralTask)
               beats.  `remove uniform from dummy`: TakeFromCh1 (`%objects% from
               %character%`) is ambiguous on "uniform" but its "dummy" names no
               character, so RemoveObjects' `[remove/take off] %objects%`
               no-reference message ("...you're referring to.") wins over the
               cantsee.  A pure first-pass ambiguity (no unmatched ref) stays
               second_chance=0 and preempts any no-reference fallback. */
            amb->second_chance = have_noref; }
        return any_vis ? RR_AMBIG : RR_CANTSEE;
      }

  /* No sibling reference was ambiguous: honour the deferred (optional)
     no-reference fallback (GetGeneralTask sNoRefTask). */
  if (have_noref)
    {
      if (amb != NULL)
        { amb->ref_name = noref_r.name; amb->type = noref_r.type;
          amb->ref_text = noref_r.text; amb->keys.clear (); }
      return RR_NOREF;
    }

  /* Bind the resolved singular references, then resolve the deferred plural
     %objects% reference against them. */
  for (auto &r : refs)
    bind_reference (st, r.name.c_str (), r.keys[0].c_str (), r.text.c_str ());

  if (plural_idx >= 0)
    {
      int pr = resolve_plural (run, t, plural_text, amb);
      if (pr == RR_NOMATCH)
        {
          /* The plural reference named no object (e.g. "take all" with nothing
             in scope).  As with a singular reference, the task only matches if
             it has an Object `Must Exist` restriction (so it can surface that
             message); otherwise the command does not match this task. */
          if (!a5restr_has_exist (t->restrictions, 'o'))
            return RR_NOMATCH;
          /* Treat like sNoRefTask: defer so a later task can claim. */
          if (amb != NULL)
            { amb->ref_name = "objects"; amb->type = 'o';
              amb->ref_text = plural_text; amb->keys.clear (); }
          return RR_NOREF;
        }
      if (pr != RR_OK)
        return pr;                  /* RR_FAIL / RR_CANTSEE (amb already set) */
    }

  /* Unique references: confirm the whole restriction set passes. */
  return a5restr_pass (st, t->restrictions) ? RR_OK : RR_FAIL;
}

/* ----------------------------------------------------------- action helpers */

/* The adventure's own (stable, DOM-aliased) pointer for an entity key. */
static const char *
canon_key (const a5_adventure_t *adv, const char *k)
{
  int i;
  if (k == NULL) return NULL;
  for (i = 0; i < adv->n_objects; i++)
    if (streq (adv->objects[i].key, k))    return adv->objects[i].key;
  for (i = 0; i < adv->n_characters; i++)
    if (streq (adv->characters[i].key, k)) return adv->characters[i].key;
  for (i = 0; i < adv->n_locations; i++)
    if (streq (adv->locations[i].key, k))  return adv->locations[i].key;
  for (i = 0; i < adv->n_groups; i++)
    if (streq (adv->groups[i].key, k))     return adv->groups[i].key;
  return NULL;
}

/*
 * Resolve an action operand to a key.  Crucially returns a *stable* pointer for
 * anything that may be stored in the world state (object/character locations):
 * the model's own key string, never a pointer into a temporary token or the
 * per-turn binding buffer (which would dangle).
 */
static const char *
act_key (a5_state_t *st, const char *tok)
{
  const char *b, *resolved, *c;
  if (tok == NULL) return NULL;
  if (streq (tok, "%Player%") || streq (tok, "Player"))
    resolved = "Player";
  else
    { b = a5state_lookup_ref (st, tok); resolved = b ? b : tok; }
  c = canon_key (st->adv, resolved);
  if (c != NULL)
    return c;
  if (streq (resolved, "Hidden"))
    return "Hidden";
  return resolved;   /* non-entity (direction/literal): used transiently only */
}

/* Apply a variable value string ("1", "%roller%", "RAND (1, 4)") -> number. */
static long
eval_num_value (a5_state_t *st, const char *raw)
{
  char *proc;
  long v;
  if (raw != NULL && strncmp (raw, "RAND", 4) == 0)
    {
      /* RAND (min, max): inclusive random in [min, max] via the shared
         erkyrath_random (frankendrift Global.Random(iMin, iMax)).  The data
         writes it as "RAND (1, 4)" (spaces optional). */
      long lo = 0, hi = 0;
      char *end;
      const char *p = raw + 4;
      while (*p && !(*p == '-' || (*p >= '0' && *p <= '9'))) p++;
      lo = strtol (p, &end, 10);
      p = end;
      while (*p && !(*p == '-' || (*p >= '0' && *p <= '9'))) p++;
      hi = (*p) ? strtol (p, NULL, 10) : lo;
      return a5rand_between (lo, hi);
    }
  proc = a5text_process (st, raw ? raw : "0");
  {
    bool ok = false;
    long e = a5_eval_arith (proc, &ok);
    if (ok) { free (proc); return e; }   /* arithmetic expression */
  }
  v = strtol (proc, NULL, 10);           /* fall back: leading integer */
  free (proc);
  return v;
}

/* Split a `Name = "value"` action body into name and (unquoted) value. */
static int
split_assignment (const std::string &body, std::string &name, std::string &value)
{
  size_t eq = body.find ('=');
  if (eq == std::string::npos)
    return 0;
  name = body.substr (0, eq);
  /* trim name */
  while (!name.empty () && isspace ((unsigned char) name.back ())) name.pop_back ();
  size_t a = name.find_first_not_of (" \t");
  name = (a == std::string::npos) ? "" : name.substr (a);
  /* value: strip spaces then surrounding quotes */
  std::string v = body.substr (eq + 1);
  a = v.find_first_not_of (" \t");
  if (a != std::string::npos) v = v.substr (a);
  while (!v.empty () && isspace ((unsigned char) v.back ())) v.pop_back ();
  if (v.size () >= 2 && v.front () == '"' && v.back () == '"')
    v = v.substr (1, v.size () - 2);
  value = v;
  return 1;
}

static void run_task (a5_run_t *run, const a5_task_t *t, int depth, sb_t *out);
static void run_action (a5_run_t *run, const char *kind, const char *body,
                        int depth, sb_t *out);
static void ev_on_task_completed (a5_run_t *run, const char *task_key, sb_t *out);
static void emit_look (a5_run_t *run, sb_t *out);
static std::string render_look_string (a5_run_t *run);
static void emit_completion (a5_run_t *run, const a5_xml_node_t *comp, sb_t *out);

/* Conversation type bits (clsAction.ConversationEnum). */
enum { A5_CONV_GREET = 1, A5_CONV_ASK = 2, A5_CONV_TELL = 4,
       A5_CONV_COMMAND = 8, A5_CONV_FAREWELL = 16 };
static void exec_conversation (a5_run_t *run, const char *char_key, int conv_type,
                               const char *subject, sb_t *out);

/* ----------------------------------------- specific-override task dispatch */

/* One resolved command reference: its general type and resolved entity key. */
struct ref_info { char type; const char *key; };   /* 'o'/'c'/'d'/'t'/'n'/'l' */

/* Map one command reference name ("objects", "object2", "character1", ...) to
   its (general type, resolved entity key) using this turn's bindings.  The bare
   "Referenced<Stem>" is reference 1; "Referenced<Stem>N" is the N-th, so a
   2-specific override keys each Specific off its own reference. */
static ref_info
ref_info_for_name (a5_state_t *st, const std::string &name)
{
  std::string base = name, num;
  while (!base.empty () && isdigit ((unsigned char) base.back ()))
    { num.insert (num.begin (), base.back ()); base.pop_back (); }
  ref_info ri; ri.type = 't'; ri.key = NULL;
  std::string stem;
  if (base == "object" || base == "objects" || base == "item")
    { stem = "Object"; ri.type = 'o'; }
  else if (base == "character" || base == "characters")
    { stem = "Character"; ri.type = 'c'; }
  else if (base == "direction") { stem = "Direction"; ri.type = 'd'; }
  else if (base == "number")    { stem = "Number"; ri.type = 'n'; }
  else if (base == "location")  { stem = "Location"; ri.type = 'l'; }
  else if (base == "text")      { stem = "Text"; ri.type = 't'; }
  else return ri;
  if (!num.empty () && num != "1")
    ri.key = a5state_lookup_ref (st, ("Referenced" + stem + num).c_str ());
  if (ri.key == NULL)
    ri.key = a5state_lookup_ref (st, ("Referenced" + stem).c_str ());
  return ri;
}

/* Recompute the ordered, resolved references for a matched command, using the
   bindings resolve_refine already established this turn. */
static std::vector<ref_info>
collect_refs (a5_state_t *st, const a5_match_t *m)
{
  std::vector<ref_info> v;
  for (int i = 0; i < m->n_refs; i++)
    v.push_back (ref_info_for_name (st, m->ref_name[i]));
  return v;
}

/* The ordered reference names (%objects%, %object2%, ...) in a task's first
   command, normalised to their binding base ("objects","object2"). */
static std::vector<std::string>
command_refs (const a5_task_t *t)
{
  std::vector<std::string> v;
  if (t->n_commands == 0) return v;
  const char *p = t->commands[0];
  while (*p)
    {
      if (*p == '%')
        {
          const char *q = p + 1;
          while (*q && *q != '%') q++;
          if (*q == '%')
            {
              std::string nm (p + 1, (size_t) (q - (p + 1)));
              std::string base = nm;
              /* singular %object% normalises to object1 only for matching, but
                 the binding base keeps "objects" plural distinct. */
              if (base == "object")    base = "object1";
              if (base == "character") base = "character1";
              if (base == "direction") base = "direction1";
              if (base == "number")    base = "number1";
              if (base == "text")      base = "text1";
              v.push_back (base);
              p = q + 1;
              continue;
            }
        }
      p++;
    }
  return v;
}

/* Build the resolved references for TASK from its first command's reference
   names and this turn's bindings -- used when re-dispatching a task via
   SetTasks Execute, where there is no a5_match_t to derive them from. */
static std::vector<ref_info>
refs_from_bindings (a5_state_t *st, const a5_task_t *t)
{
  std::vector<ref_info> v;
  for (auto &nm : command_refs (t))
    v.push_back (ref_info_for_name (st, nm));
  return v;
}

/* Evaluate a SetTasks "Execute (...)" argument to an entity KEY (not a display
   name): a plain %reference% yields its bound key; %ParentOf[..]% and the like
   evaluate via the text engine (which already returns keys). */
static char *
eval_arg_to_key (a5_state_t *st, const std::string &arg)
{
  std::string a = arg;
  size_t b = a.find_first_not_of (" \t");
  size_t e = a.find_last_not_of (" \t");
  a = (b == std::string::npos) ? "" : a.substr (b, e - b + 1);
  /* A bare entity key (no %...% at all, e.g. `vnl_Dial`) is already a key -- pass
     it through verbatim.  It must NOT go through a5text_process, whose display
     pipeline auto-capitalises the first letter ("vnl_Dial" -> "Vnl_Dial") and so
     corrupts the case-sensitive object key. */
  if (a.find ('%') == std::string::npos)
    return strdup (a.c_str ());
  if (a.size () >= 2 && a.front () == '%' && a.back () == '%'
      && a.find ('[') == std::string::npos)
    {
      std::string nm = a.substr (1, a.size () - 2);
      if (nm == "Player") return strdup ("Player");
      std::string lname = lower (nm);
      const char *k = NULL;
      if (lname == "objects")          k = a5state_lookup_ref (st, "ReferencedObjects");
      else if (lname.rfind ("object", 0) == 0)
        { std::string cap = "ReferencedObject"; if (lname.size () > 6) cap += lname.substr (6);
          k = a5state_lookup_ref (st, cap.c_str ()); if (!k) k = a5state_lookup_ref (st, "ReferencedObject"); }
      else if (lname.rfind ("character", 0) == 0)
        k = a5state_lookup_ref (st, "ReferencedCharacter");
      if (k != NULL) return strdup (k);
    }
  /* A function/OO arg (e.g. %PropertyValue[%object1%,LockKey]%) evaluates to an
     entity KEY -- run the substitution passes but NOT auto-capitalisation, which
     would turn "s_SkeletonKe" into "S_SkeletonKe" and break the case-sensitive
     lookup (same class of bug as the bare-key `vnl_Dial` case above). */
  return a5text_process_nocap (st, a.c_str ());
}

/* Resolve a Specific's key to a concrete key (handles %Player%/references). */
static const char *
specific_key (a5_state_t *st, const char *key)
{
  if (key == NULL || key[0] == '\0')
    return NULL;                                  /* "" = match any */
  if (streq (key, "%Player%"))
    return "Player";
  if (strchr (key, '%') != NULL)
    {
      const char *b = a5state_lookup_ref (st, key);   /* best-effort */
      return b ? b : key;
    }
  return key;
}

/* A Specific that constrains nothing (no Key elems, or a single empty Key) --
   FD's `Keys.Count = 0 OrElse (Keys.Count = 1 AndAlso Keys(0) = "")`. */
static int
spec_is_any (a5_state_t *st, const a5_specific_t *sp)
{
  if (sp->n_keys == 0) return 1;
  if (sp->n_keys == 1 && specific_key (st, sp->keys[0]) == NULL) return 1;
  return 0;
}

/* Match a resolved-specifics vector 1:1 against the references (one Specific per
   reference, each non-empty key matching its reference's resolved key). */
static int
match_specifics_vec (a5_state_t *st, const std::vector<const a5_specific_t *> &specs,
                     const std::vector<ref_info> &refs)
{
  if (specs.size () != refs.size ())
    return 0;
  for (size_t i = 0; i < specs.size (); i++)
    {
      const a5_specific_t *sp = specs[i];
      int matched = (sp->n_keys == 0);            /* no Key elems = match any */
      for (int k = 0; k < sp->n_keys; k++)
        {
          const char *want = specific_key (st, sp->keys[k]);
          if (want == NULL) { matched = 1; break; }       /* empty key = any */
          /* FD compares the Specific key to the reference's matched possibility
             case-insensitively (RefsMatchSpecifics, clsUserSession.vb:634).  For a
             Text specific (e.g. Grandpa's `say %text%` -> `vnl_SayKennedy`,
             key "kennedy") the reference resolves to the typed text, so match it
             case-insensitively; entity keys stay exact. */
          if (refs[i].key != NULL
              && (streq (want, refs[i].key)
                  || (refs[i].type == 't'
                      && lower (want) == lower (refs[i].key))))
            { matched = 1; break; }
        }
      if (!matched)
        return 0;
    }
  return 1;
}

/* RefsMatchSpecifics: the child's Specifics must line up 1:1 with the resolved
   references.  A *nested* override may carry FEWER specifics than the references
   (it only re-constrains some of them) -- FD (clsUserSession.vb:1060-1071) then
   expands the child against its parent: keep each parent specific that is itself
   constrained, and fill the parent's "any" slots from the child's specifics in
   order.  E.g. Jacaranda's `give tape to pirate`: Task49 (Object=tape) overrides
   Task48 (Object=any, Character=pirate) -> effective specifics [tape, pirate]. */
static int
refs_match_specifics (a5_state_t *st, const a5_task_t *child,
                      const a5_task_t *parent, const std::vector<ref_info> &refs)
{
  std::vector<const a5_specific_t *> specs;
  if (child->n_specifics == (int) refs.size ())
    {
      for (int i = 0; i < child->n_specifics; i++)
        specs.push_back (&child->specifics[i]);
    }
  else if (parent != NULL
           && child->n_specifics < parent->n_specifics
           && parent->n_specifics == (int) refs.size ())
    {
      int ichild = 0;
      for (int i = 0; i < parent->n_specifics; i++)
        {
          const a5_specific_t *psp = &parent->specifics[i];
          if (spec_is_any (st, psp) && ichild < child->n_specifics)
            specs.push_back (&child->specifics[ichild++]);
          else
            specs.push_back (psp);
        }
    }
  else
    return 0;
  return match_specifics_vec (st, specs, refs);
}

/* ----------------------------------------- per-command response aggregation */

/* FrankenDrift's clsUserSession collects every task completion / restriction-
   fail message of a single top-level command into two ordered, message-keyed
   maps (htblResponsesPass / htblResponsesFail) and flushes them once at the end
   (AttemptToExecuteTask, clsUserSession.vb:782-863).  Two behaviours fall out:

     * a plural %objects% reference is run one item at a time (ExecuteSubTasks,
       vb:695), so each item independently selects its Specific overrides; and
     * identical messages dedup, while an AggregateOutput task's raw template is
       the key so two items through the same task collapse to one entry whose
       %objects% list grows (AddResponse, vb:1295) -- e.g. "the tiara and the
       shoes".  A pass for an item drops that item's earlier fail message.

   Scarier mirrors this only on the plural path (run_general); the single-
   reference path keeps writing straight to `out`. */
struct resp_entry {
  bool is_pass;
  bool aggregate;                 /* key is comp ptr; render deferred to flush  */
  const a5_xml_node_t *comp;      /* aggregate: re-rendered at flush            */
  std::string rendered;           /* non-aggregate / fail: final text           */
  std::vector<std::string> obj_keys;  /* merged %objects% items (aggregate)     */
  std::string obj2;               /* %object2% snapshot (first occurrence)      */
  std::string item;               /* the single item this entry was made for    */
};
struct resp_map {
  std::vector<resp_entry> ents;
  std::string cur_item;           /* item currently being iterated              */
};

/* The "did this produce a response" probe used by the override scan: with a map
   active it is the entry count (out never grows), else the byte length of out. */
static size_t
sink_len (a5_run_t *run, sb_t *out)
{
  return run->resp != NULL ? run->resp->ents.size () : out->len;
}

/* Render a completion wrapper without retiring its DisplayOnce segments
   (FrankenDrift's bTestingOutput=True): used for the pre/post-action comparison
   that decides whether a Before message is pinned or deferred. */
static char *
render_comp_test (a5_state_t *st, const a5_xml_node_t *comp)
{
  int pm = st->marking_display; st->marking_display = 0;
  char *m = a5text_describe (st, comp);
  st->marking_display = pm;
  return m;
}

/* Insert a freshly-built entry, honouring a reserved position (the slot a
   "Before" message claimed before its actions ran -- clsUserSession's
   iResponsePosition / OrderedHashTable.Insert, vb:1189/1311). */
static void
resp_insert (resp_map *rm, resp_entry &e, int pos)
{
  if (pos >= 0 && (size_t) pos < rm->ents.size ())
    rm->ents.insert (rm->ents.begin () + pos, e);
  else
    rm->ents.push_back (e);
}

/* Add a completion message (from task t) as a response.  An AggregateOutput
   task defers its function replacement: the comp node is the dedup key, and a
   second item through the same task merges into the existing entry (its
   %objects% list grows).  A non-aggregate task renders eagerly at the current
   (single-item) state and dedups on the rendered text. */
static void
resp_add_comp (a5_run_t *run, const a5_task_t *t, const a5_xml_node_t *comp,
               bool is_pass, int pos = -1)
{
  resp_map *rm = run->resp;
  a5_state_t *st = run->st;
  bool aggregate = t != NULL && t->aggregate;
  const std::string &item = rm->cur_item;

  if (aggregate)
    {
      for (auto &e : rm->ents)
        if (e.aggregate && e.comp == comp && e.is_pass == is_pass)
          {
            if (!item.empty ()
                && std::find (e.obj_keys.begin (), e.obj_keys.end (), item)
                     == e.obj_keys.end ())
              e.obj_keys.push_back (item);
            return;
          }
      resp_entry e;
      e.is_pass = is_pass; e.aggregate = true; e.comp = comp;
      const char *o2 = a5state_lookup_ref (st, "ReferencedObject2");
      e.obj2 = o2 ? o2 : "";
      if (!item.empty ()) e.obj_keys.push_back (item);
      e.item = item;
      resp_insert (rm, e, pos);
    }
  else
    {
      int pm = st->marking_display; st->marking_display = 1;
      char *m = a5text_describe (st, comp);
      st->marking_display = pm;
      bool has = msg_has_output (m);
      std::string r = m ? m : ""; free (m);
      if (!has) return;
      for (auto &e : rm->ents)
        if (!e.aggregate && e.is_pass == is_pass && e.rendered == r) return;
      resp_entry e;
      e.is_pass = is_pass; e.aggregate = false; e.comp = NULL;
      e.rendered = r; e.item = item;
      resp_insert (rm, e, pos);
    }
}

/* Add an already-rendered string as a response (override-fail messages, room
   views, pinned Before messages).  Always non-aggregate; deduped on the text. */
static void
resp_add_text (a5_run_t *run, const char *text, bool is_pass, int pos = -1)
{
  resp_map *rm = run->resp;
  if (!msg_has_output (text)) return;
  std::string r = text;
  for (auto &e : rm->ents)
    if (!e.aggregate && e.is_pass == is_pass && e.rendered == r) return;
  resp_entry e;
  e.is_pass = is_pass; e.aggregate = false; e.comp = NULL;
  e.rendered = r; e.item = rm->cur_item;
  resp_insert (rm, e, pos);
}

/* Flush the map to `out`: pass responses first in insertion order (aggregate
   entries re-rendered over their merged %objects% set), then any fail response
   whose item was not covered by a pass (clsUserSession.vb:804-855). */
static void
resp_flush (a5_run_t *run, resp_map *rm, sb_t *out)
{
  a5_state_t *st = run->st;
  std::set<std::string> passed;
  for (auto &e : rm->ents)
    if (e.is_pass)
      {
        if (!e.item.empty ()) passed.insert (e.item);
        for (auto &k : e.obj_keys) passed.insert (k);
      }

  for (int phase = 0; phase < 2; phase++)
    for (auto &e : rm->ents)
      {
        if (e.is_pass != (phase == 0)) continue;
        if (!e.is_pass && !e.item.empty () && passed.count (e.item)) continue;

        std::string text;
        if (e.aggregate)
          {
            /* Rebind the merged %objects% set exactly as resolve_plural does so
               the completion renders its "a, b and c" list (a5text reads
               st->ref_items / the ReferencedObjects pipe). */
            std::string pipe;
            st->n_ref_items = 0;
            st->ref_items_type = 'o';
            for (auto &k : e.obj_keys)
              {
                const a5_object_t *o = a5model_object (st->adv, k.c_str ());
                if (o == NULL) continue;
                if (st->n_ref_items < A5_MAX_ITEMS)
                  st->ref_items[st->n_ref_items++] = o->key;
                if (!pipe.empty ()) pipe += "|";
                pipe += o->key;
              }
            bind_reference (st, "objects", pipe.c_str (), pipe.c_str ());
            a5state_bind_ref (st, "ReferencedObjects", pipe.c_str ());
            if (!e.obj2.empty ())
              bind_reference (st, "object2", e.obj2.c_str (), e.obj2.c_str ());
            int pm = st->marking_display; st->marking_display = 1;
            char *m = a5text_describe (st, e.comp);
            st->marking_display = pm;
            if (m != NULL) text = m;
            free (m);
          }
        else
          text = e.rendered;

        if (msg_has_output (text.c_str ()))
          { sb_pspace (out); sb_puts (out, text.c_str ()); }
      }
}

/* True when every Specific of a child override matches "any" (an empty key) --
   the "lazy" catch-all pattern (e.g. TakeObjectsFromOthersLazy, Key empty).
   Such an override that *fails* its restrictions simply declines the item and
   lets the general parent run; an object-specific override (Key=Ballgown) that
   fails instead asserts a per-object failure and suppresses the parent. */
static int
child_all_any (a5_state_t *st, const a5_task_t *child)
{
  if (child->n_specifics == 0) return 0;
  for (int i = 0; i < child->n_specifics; i++)
    if (!spec_is_any (st, &child->specifics[i])) return 0;
  return 1;
}

/*
 * Run PARENT honouring its Specific child overrides (clsTask Children +
 * SpecificOverrideType), recursively -- a faithful port of FD's
 * AttemptToExecuteTask -> AttemptToExecuteSubTask -> AttemptToExecuteTask.  A
 * child whose Specifics match REFS and whose own restrictions pass runs in place
 * of (Override) or before/after the parent's text+actions, and is itself
 * resolved for nested overrides.  Reusable from both the command-matched parent
 * (run_general) and a SetTasks-Execute re-dispatch.
 */
static void
execute_task_with_overrides (a5_run_t *run, const a5_task_t *parent,
                             const std::vector<ref_info> &refs, int depth,
                             sb_t *out)
{
  a5_state_t *st = run->st;
  int parent_text = 1, parent_actions = 1;
  std::vector<const a5_task_t *> after;

  /* A command-bearing General has override children; a Specific override
     (depth>0) is itself scanned for its OWN nested children -- e.g. GetAWooden
     overrides TakeObjectFromLocation, which overrides TakeObjects. */
  if ((parent->n_commands > 0 || depth > 0) && depth < 16)
    for (size_t oi = 0; oi < run->order->size (); oi++)
      {
        int cidx = (*run->order)[oi];
        const a5_task_t *child = &run->adv->tasks[cidx];
        if (!streq (child->type, "Specific")) continue;
        if (!streq (child->general_key, parent->key)) continue;
        if (!refs_match_specifics (st, child, parent, refs)) continue;
        /* clsUserSession.AttemptToExecuteTask returns False at entry for an
           already-completed non-repeatable task (vb:730), so such a child does
           not fire as an override -- the parent (or a later child) runs. */
        if (!child->repeatable && st->task_done[cidx]) continue;

        const char *ot = child->override_type;
        int is_after = ot != NULL && strncmp (ot, "After", 5) == 0;
        if (is_after)
          { after.push_back (child); continue; }

        /* clsUserSession.AttemptToExecuteSubTask iterates EVERY matching child in
           priority order (clsUserSession.vb:1056-1160), not just the first.  After
           running a child it keeps going unless that child claims the turn -- the
           bContinue rule (clsUserSession.vb:911-936): continue past a child that
           produced no output (or whose Continue is ContinueAlways), stop after one
           that did.  So Amazon's per-move travel-time override (Delay*, no output)
           runs and then falls through to Beforeplay (the "You move <dir>." +
           Date/Time override, with output), which then stops the scan.  Detect a
           child's output by the growth of `out`. */
        size_t len0 = sink_len (run, out);

        if (!a5restr_pass (st, child->restrictions))
          {
            /* A Specific task that matches the references but whose restrictions
               fail still claims the turn if it has a fail message: show it and
               suppress the parent.  In the default HighestPriorityTask mode a
               fail *with* output stops the scan; in HighestPriorityPassingTask
               mode, or with no output at all, scanning continues so a later
               override (or the parent) can run. */
            const a5_xml_node_t *fm =
              a5restr_fail_message (st, child->restrictions);
            /* On the plural map path, a "lazy" match-any override (empty
               Specific key) that fails simply declines this item -- its fail
               message is collected (and dropped later if the item passes the
               general task), but the parent still runs.  An object-specific
               override that fails asserts a per-object failure: show it and
               suppress the parent (clsUserSession's bShouldParentExecuteTasks).
               Mirrors why Amazon's bottle (lazy fail) is still picked up while
               RunBronwynn's ballgown (RemoveBall, Key=Ballgown) is not. */
            int lazy = run->resp != NULL && child_all_any (st, child);
            if (fm != NULL)
              {
                if (run->resp != NULL)
                  { char *f = a5text_describe (st, fm);
                    resp_add_text (run, f, false); free (f); }
                else
                  { char *fmsg = a5text_describe (st, fm);
                    sb_pspace (out); sb_puts (out, fmsg); free (fmsg); }
                if (!lazy) { parent_text = 0; parent_actions = 0; }
              }
            if (!lazy && sink_len (run, out) > len0
                && !child->continue_lower && !st->adv->hp_passing)
              break;                        /* fail with output: claims the turn */
            continue;                       /* no output (or HPPT): keep scanning */
          }

        /* Recurse so the child's own Specific overrides fire in its place
           (AttemptToExecuteSubTask -> recursive AttemptToExecuteTask). */
        execute_task_with_overrides (run, child, refs, depth + 1, out);
        st->task_done[cidx] = 1;        /* clsUserSession.vb:1193 task.Completed = True */
        /* A completing override child fires its own EventControls/WalkControls,
           exactly like the parent (FD's AttemptToExecuteSubTask -> recursive
           AttemptToExecuteTask runs the control loop for every task).  E.g. Stone
           of Wisdom's ring-knockout `s_AttackTheT` is a Specific override of
           AttackCharacterWithObject; only its completion fires StartTroll's
           `Stop Completion s_AttackTheT`, disarming the troll's death subevent. */
        ev_on_task_completed (run, child->key, out);
        if (ot == NULL || streq (ot, "Override"))
          { parent_text = 0; parent_actions = 0; }
        else if (streq (ot, "BeforeTextOnly"))      parent_actions = 0;
        else if (streq (ot, "BeforeActionsOnly"))   parent_text = 0;
        /* BeforeTextAndActions: parent still runs both (child ran first).
           FD (clsUserSession.vb:1106/1111): parent ACTIONS run for
           BeforeActionsOnly|BeforeTextAndActions; parent TEXT is output for
           BeforeTextOnly|BeforeTextAndActions.  So BeforeActionsOnly keeps the
           parent's actions but suppresses its completion message (Grandpa's
           `vnl_OpenGate1` "safe enough to open" override: the gate still opens,
           but the generic "You open the gate." is not shown). */

        /* Stop only when this passing child produced output (and isn't
           ContinueAlways); otherwise fall through to the next matching child. */
        if (sink_len (run, out) > len0 && !child->continue_lower)
          break;
      }

  /* FrankenDrift defers an AggregateOutput task's completion-message function
     replacement to final Display (clsUserSession.vb:1184/1210 replace eagerly
     only when NOT AggregateOutput).  So an aggregate parent's text reflects
     state changed by its AfterTextAndActions / AfterActionsOnly children, which
     run before Display.  Mirror that: when an aggregate parent with no actions
     of its own has After-children, run the children first, then render the
     parent text -- keeping the parent text ahead of the children's output.
     Stone of Wisdom's `x window` needs this: ExamineAnO (AfterTextAndActions)
     increments Windowcoun, and ExamineObjects' %object%.Description segments are
     gated on it, so the "movement"/"creatures" lines must reflect the post-
     increment count on the same turn FD shows them. */
  int defer_text = run->resp == NULL && parent_text && parent->aggregate
                   && parent->actions == NULL && !after.empty ();

  /* If the parent has After children that run actions on the direct path, defer
     its room view (Execute Look) so it renders at the post-child state, mirroring
     FD's AggregateOutput Look deferral (see emit_look). */
  int prev_defer_look = run->defer_look;
  int prev_look_pending = run->look_pending;
  size_t prev_look_pos = run->look_pos;
  int enable_look_defer = run->resp == NULL && !defer_text && !after.empty ();
  if (enable_look_defer) { run->defer_look = 1; run->look_pending = 0; }

  if (parent_text || parent_actions)
    {
      /* run_task emits both text and actions; gate via temporary copies. */
      if (!parent_text && parent_actions)
        {
          const a5_xml_node_t *act = parent->actions;
          if (act != NULL)
            for (const a5_xml_node_t *c = act->first_child; c; c = c->next)
              run_action (run, c->name, c->text, 0, out);
        }
      else if (parent_text && !defer_text)
        run_task (run, parent, 0, out);
    }

  run->defer_look = prev_defer_look;
  /* Only the frame that enabled deferral consumes the pending look; a nested
     non-enabling frame (e.g. MovePlayer, run via SetTasks Execute, which emits
     the actual Look) leaves it pending for the enabling parent to render after
     its After children run.  The Look's offset is into the enabling frame's
     `out`, which is the same sink threaded through the nested calls. */
  int look_deferred = 0;
  if (enable_look_defer)
    {
      if (run->look_pending) look_deferred = 1;
      run->look_pending = prev_look_pending;
      run->look_pos = prev_look_pos;
    }

  if (look_deferred)
    {
      /* The parent's room view was skipped.  Run the After children into a side
         buffer (their actions update what the room lists -- e.g. Grandpa's `go
         west` moves Buster into the western enclosure), render the view once at
         the resulting final state, then emit view-then-children so the room
         reflects the moves and the children's lines follow it.  Using a side
         buffer (rather than splicing) lets sb_pspace compute each separator
         against real preceding text -- e.g. Jacaranda's "...Exits are ... in.
         [pSpace]It is raining out here." */
      sb_t after_buf; sb_init (&after_buf);
      for (auto *child : after)
        if (a5restr_pass (st, child->restrictions))
          {
            run_task (run, child, 0, &after_buf);
            int ai = a5state_task_index (st, child->key);
            if (ai >= 0) st->task_done[ai] = 1;
            ev_on_task_completed (run, child->key, &after_buf);
          }
      std::string view = render_look_string (run);
      sb_pspace (out); sb_puts (out, view.c_str ());
      if (after_buf.len > 0) { sb_pspace (out); sb_puts (out, after_buf.p); }
      free (after_buf.p);
    }
  else if (defer_text)
    {
      /* Children run into a side buffer so the parent text can be spliced ahead
         of them once their actions have updated state. */
      sb_t after_buf; sb_init (&after_buf);
      for (auto *child : after)
        if (a5restr_pass (st, child->restrictions))
          {
            run_task (run, child, 0, &after_buf);
            int ai = a5state_task_index (st, child->key);
            if (ai >= 0) st->task_done[ai] = 1;
            ev_on_task_completed (run, child->key, &after_buf);
          }
      int self_ti = a5state_task_index (st, parent->key);
      if (self_ti >= 0) st->task_done[self_ti] = 1;
      const a5_xml_node_t *comp = a5xml_child (parent->node, "CompletionMessage");
      if (comp != NULL) emit_completion (run, comp, out);
      if (after_buf.len > 0) { sb_pspace (out); sb_puts (out, after_buf.p); }
      free (after_buf.p);
    }
  else
    for (auto *child : after)
      if (a5restr_pass (st, child->restrictions))
        {
          run_task (run, child, 0, out);
          int ai = a5state_task_index (st, child->key);
          if (ai >= 0) st->task_done[ai] = 1;   /* task.Completed = True */
          ev_on_task_completed (run, child->key, out);   /* fire its controls too */
        }
}

/* Run a matched General task (the directly command-matched parent), honouring
   its Specific child overrides. */
static void
run_general (a5_run_t *run, const a5_task_t *parent, const a5_match_t *m,
             sb_t *out)
{
  a5_state_t *st = run->st;

  /* A genuine plural %objects% command (resolve_plural bound ref_items this
     turn) is run one item at a time, FrankenDrift-style (ExecuteSubTasks):
     each item independently selects its Specific overrides, and the resulting
     completion / fail messages are aggregated (dedup + %objects% merge) and
     flushed once.  A single resolved item keeps the ordinary direct path so
     non-plural commands are byte-identical. */
  int objidx = -1;
  for (int i = 0; i < m->n_refs; i++)
    if (strcmp (m->ref_name[i], "objects") == 0) { objidx = i; break; }

  if (objidx >= 0 && st->n_ref_items > 1)
    {
      std::vector<const char *> items (st->ref_items,
                                       st->ref_items + st->n_ref_items);
      const char *rtext = m->ref_text[objidx];
      resp_map rm;
      run->resp = &rm;
      for (const char *it : items)
        {
          rm.cur_item = it;
          st->ref_items[0] = it;
          st->n_ref_items = 1;
          bind_reference (st, "objects", it, rtext);
          a5state_bind_ref (st, "ReferencedObjects", it);
          std::vector<ref_info> refs = collect_refs (st, m);
          execute_task_with_overrides (run, parent, refs, 0, out);
        }
      run->resp = NULL;
      resp_flush (run, &rm, out);
      return;
    }

  std::vector<ref_info> refs = collect_refs (st, m);
  execute_task_with_overrides (run, parent, refs, 0, out);
}

/* ------------------------------------------------------------ conversation */

/* Mark every character the player can currently see as "seen" (player-centric
   clsCharacter.HasSeenCharacter, set each turn by clsUserSession's
   PrepareForNextTurn).  The player is always considered self-seen. */
static void
update_seen (a5_state_t *st)
{
  const char *ploc = a5state_player_location (st);
  int i;
  if (st->char_seen == NULL)
    return;
  for (i = 0; i < st->adv->n_characters; i++)
    {
      if (streq (st->adv->characters[i].key, "Player"))
        { st->char_seen[i] = 1; continue; }
      if (ploc != NULL && streq (st->char_loc[i], ploc))
        st->char_seen[i] = 1;
    }
  /* Mark every object currently visible in the player's room as seen
     (clsCharacter.HasSeenObject), so HaveBeenSeenByCharacter persists once an
     object has been viewed. */
  if (st->obj_seen != NULL && ploc != NULL)
    for (i = 0; i < st->adv->n_objects; i++)
      if (a5state_object_visible_at_location (st, i, ploc, 0))
        st->obj_seen[i] = 1;
}

/* ContainsWord (clsUserSession.vb:3885): every space-separated word of `check`
   appears among the space-separated words of `sentence` (case-insensitive).

   FAITHFUL QUIRK: FD splits with VB `Split(x, " ")`, which KEEPS empty tokens.
   So an empty `check` (an empty-keyword Ask/Tell topic, e.g. RtC's "ask about
   igor" Topic6 with <Keywords/>) splits to the single check-word "", which is
   found only when the sentence also has an empty token -- i.e. a non-empty
   subject like "freeze" never matches an empty keyword.  C++ `split_ws` drops
   empties, so an empty keyword used to match EVERYTHING (Topic6 stole the turn
   from the keyworded "freeze" Topic7).  Mirror VB Split (split on a single
   space, keep empties) exactly. */
static int
conv_contains_word (const std::string &sentence, const std::string &check)
{
  auto vb_split = [] (const std::string &s) {
    std::vector<std::string> out;
    size_t start = 0;
    for (;;)
      {
        size_t sp = s.find (' ', start);
        if (sp == std::string::npos) { out.push_back (s.substr (start)); break; }
        out.push_back (s.substr (start, sp - start));
        start = sp + 1;
      }
    return out;
  };
  std::vector<std::string> words = vb_split (lower (sentence));
  for (auto &c : vb_split (check))
    {
      int found = 0;
      for (auto &w : words)
        if (w == c) { found = 1; break; }
      if (!found)
        return 0;
    }
  return 1;
}

static int
topic_has_children (const a5_character_t *ch, const char *key)
{
  int i;
  for (i = 0; i < ch->n_topics; i++)
    if (streq (ch->topics[i].parent_key, key))
      return 1;
  return 0;
}

/* Render a topic reply / message with the conversation character as the text
   context, trimmed and newline-terminated (one AddResponse). */
static void
emit_conv (a5_run_t *run, const char *rendered_owned, sb_t *out)
{
  char *m = (char *) rendered_owned;
  if (msg_has_output (m))
    { sb_pspace (out); sb_puts (out, m); }
  free (m);
}

/*
 * FindConversationNode (clsUserSession): the best topic of `ch` matching the
 * conversation type + subject at the current node.  Ask/Tell topics match by
 * keyword (most matches, then highest %); Command topics by command pattern;
 * Greet/Farewell topics just by restrictions.  When a candidate matches its
 * trigger but fails its restrictions, its restriction fail-message is captured
 * into *restr_text (last wins), so the caller can surface it as the default.
 */
static const a5_topic_t *
find_conv_node (a5_run_t *run, const a5_character_t *ch, int conv_type,
                const char *subject, std::string *restr_text)
{
  a5_state_t *st = run->st;
  int t = conv_type;
  int b_farewell = 0, b_command = 0, b_tell = 0, b_ask = 0, b_intro = 0;
  double best_pct = 0.0;
  int best_matches = 0;
  const a5_topic_t *best = NULL;
  int i;

  if (t >= A5_CONV_FAREWELL) { b_farewell = 1; t -= A5_CONV_FAREWELL; }
  if (t >= A5_CONV_COMMAND)  { b_command  = 1; t -= A5_CONV_COMMAND; }
  if (t >= A5_CONV_TELL)     { b_tell     = 1; t -= A5_CONV_TELL; }
  if (t >= A5_CONV_ASK)      { b_ask      = 1; t -= A5_CONV_ASK; }
  if (t >= A5_CONV_GREET)    { b_intro    = 1; t -= A5_CONV_GREET; }

  for (i = 0; i < ch->n_topics; i++)
    {
      const a5_topic_t *tp = &ch->topics[i];
      if (!(streq (tp->parent_key, "") || streq (tp->parent_key, st->conv_node)))
        continue;
      if (b_intro && !tp->is_intro)       continue;
      if (b_farewell && !tp->is_farewell) continue;
      if (b_command != tp->is_command)    continue;
      if (b_ask && !tp->is_ask)           continue;
      if (b_tell && !tp->is_tell)         continue;

      if (b_ask || b_tell)
        {
          std::vector<std::string> kws;
          { std::string cur; const char *p = tp->keywords;
            for (; p && *p; p++)
              { if (*p == ',') { kws.push_back (cur); cur.clear (); }
                else cur += *p; }
            kws.push_back (cur); }
          int matched = 0, low_priority = 0;
          for (auto &kw : kws)
            {
              std::string k = lower (kw);
              size_t a = k.find_first_not_of (" \t");
              size_t z = k.find_last_not_of (" \t");
              k = (a == std::string::npos) ? "" : k.substr (a, z - a + 1);
              if (conv_contains_word (subject, k) || k == "*")
                {
                  if (a5restr_pass (st, tp->restrictions))
                    matched++;
                  else if (restr_text != NULL)
                    { const a5_xml_node_t *fm =
                        a5restr_fail_message (st, tp->restrictions);
                      if (fm != NULL)
                        { char *t2 = a5text_describe (st, fm);
                          *restr_text = t2; free (t2); } }
                  if (k == "*") low_priority = 1;
                }
            }
          double pct = kws.empty () ? 0.0 : (double) matched / (double) kws.size ();
          if (low_priority && pct == 1.0) pct = 0.001;
          if (matched > best_matches
              || (matched == best_matches && pct > best_pct))
            { best = tp; best_pct = pct; best_matches = matched; }
        }
      else if (b_command)
        {
          a5_match_t m;
          if (a5parse_match_command (tp->keywords, subject, &m))
            {
              if (a5restr_pass (st, tp->restrictions))
                return tp;
              else if (restr_text != NULL)
                { const a5_xml_node_t *fm =
                    a5restr_fail_message (st, tp->restrictions);
                  if (fm != NULL)
                    { char *t2 = a5text_describe (st, fm);
                      *restr_text = t2; free (t2); } }
            }
        }
      else /* greet / farewell: no keyword match, just restrictions */
        {
          if (a5restr_pass (st, tp->restrictions))
            return tp;
          else if (restr_text != NULL)
            { const a5_xml_node_t *fm =
                a5restr_fail_message (st, tp->restrictions);
              if (fm != NULL)
                { char *t2 = a5text_describe (st, fm);
                  *restr_text = t2; free (t2); } }
        }
    }
  return best;
}

/*
 * ExecuteConversation (clsUserSession): drive a Greet/Ask/Tell/Command/Farewell
 * against a character -- implicit/explicit intro on first contact, then the
 * matching topic's reply + actions, else a default "doesn't understand" message
 * (or a topic restriction's fail text).
 */
static void
exec_conversation (a5_run_t *run, const char *char_key, int conv_type,
                   const char *subject, sb_t *out)
{
  a5_state_t *st = run->st;
  const a5_character_t *conv_ch;
  const a5_topic_t *topic = NULL;
  std::string restr_text;

  if (char_key == NULL || char_key[0] == '\0')
    return;

  /* Leaving a different conversation: implicit farewell, then reset. */
  if (st->conv_char[0] != '\0' && !streq (st->conv_char, char_key))
    {
      const a5_character_t *prev = a5model_character (st->adv, st->conv_char);
      if (prev != NULL)
        {
          const a5_topic_t *fw = find_conv_node (run, prev, conv_type, "", NULL);
          if (fw != NULL)
            {
              const char *pc = st->ctx_char; st->ctx_char = prev->key;
              emit_conv (run, a5text_describe (st, fw->conversation), out);
              st->ctx_char = pc;
            }
        }
      a5state_set_conv_char (st, "");
      a5state_set_conv_node (st, "");
    }

  conv_ch = a5model_character (st->adv, char_key);
  if (conv_ch == NULL)
    return;

  /* Not yet in a conversation: try an explicit then an implicit intro. */
  if (st->conv_char[0] == '\0')
    {
      const a5_topic_t *intro =
        find_conv_node (run, conv_ch, conv_type | A5_CONV_GREET, subject, NULL);
      if (intro == NULL)
        intro = find_conv_node (run, conv_ch, A5_CONV_GREET, "", NULL);
      if (intro != NULL)
        {
          const char *pc = st->ctx_char; st->ctx_char = conv_ch->key;
          emit_conv (run, a5text_describe (st, intro->conversation), out);
          st->ctx_char = pc;
          a5state_set_conv_node (st, intro->key);
          if (intro->is_ask || intro->is_tell || intro->is_command)
            {
              a5state_set_conv_char (st, char_key);
              if (intro->actions != NULL)
                for (const a5_xml_node_t *c = intro->actions->first_child;
                     c != NULL; c = c->next)
                  run_action (run, c->name, c->text, 0, out);
              return;
            }
        }
    }

  a5state_set_conv_char (st, char_key);

  if (conv_type == A5_CONV_COMMAND)
    topic = find_conv_node (run, conv_ch, conv_type | A5_CONV_FAREWELL,
                            subject, NULL);
  if (topic == NULL)
    topic = find_conv_node (run, conv_ch, conv_type, subject, &restr_text);
  else
    { a5state_set_conv_char (st, ""); a5state_set_conv_node (st, ""); }

  if (topic != NULL)
    {
      const char *pc = st->ctx_char; st->ctx_char = conv_ch->key;
      emit_conv (run, a5text_describe (st, topic->conversation), out);
      st->ctx_char = pc;
      if (topic_has_children (conv_ch, topic->key))
        a5state_set_conv_node (st, topic->key);
      else if (!topic->stay_in_node)
        a5state_set_conv_node (st, "");
      if (topic->actions != NULL)
        for (const a5_xml_node_t *c = topic->actions->first_child;
             c != NULL; c = c->next)
          run_action (run, c->name, c->text, 0, out);
    }
  else
    {
      std::string msg;
      a5state_set_conv_node (st, "");
      if (!restr_text.empty ())
        msg = restr_text;
      else
        {
          std::string verb = (conv_type == A5_CONV_COMMAND)
            ? "ignores you." : "doesn't appear to understand you.";
          msg = std::string ("%CharacterName[") + conv_ch->key + "]% " + verb;
        }
      emit_conv (run, a5text_process (st, msg.c_str ()), out);
    }
}

/* clsCharacter.Move (Player branch): when the Player moves into a new location,
   arm every System task whose <LocationTrigger> is that location and that may
   still run (Repeatable or not yet Completed), by enqueuing it onto
   qTasksToRun.  The queue is drained after the turn's task, before the event
   tick (ev_tick_all).  `old_loc` is where the Player was *before* the move. */
static void
enqueue_loc_trigger_tasks (a5_run_t *run, const char *old_loc, const char *new_loc)
{
  a5_state_t *st = run->st;
  int i;
  if (new_loc == NULL || streq (old_loc, new_loc))
    return;
  for (i = 0; i < st->adv->n_tasks; i++)
    {
      const a5_task_t *t = &st->adv->tasks[i];
      int ti;
      if (!streq (t->type, "System") || !streq (t->location_trigger, new_loc))
        continue;
      ti = a5state_task_index (st, t->key);
      if (!t->repeatable && ti >= 0 && st->task_done[ti])
        continue;
      run->tasks_to_run->push_back (t->key);
    }
}

/* clsCharacter.Move conversation maintenance (clsCharacter.vb:535-545): after the
   Player moves, if we are in a conversation whose partner is no longer at the
   Player's (new) location, show an implicit Farewell topic and end the
   conversation.  Without this, conv_char persists across rooms so `say %text%`
   keeps matching `Say` (BeInConversationWith) instead of falling to `SayLazy`
   (BeAloneWith) -- e.g. Stone of Wisdom's `say yes` to Pamba never routes
   `SayToCharacter (yes|%AloneWithChar%)`, and the weapon-stall trade fails. */
static void
clear_conv_if_partner_gone (a5_run_t *run, sb_t *out)
{
  a5_state_t *st = run->st;
  int pci;
  const char *ploc;
  if (st->conv_char == NULL || st->conv_char[0] == '\0')
    return;
  pci = a5state_character_index (st, st->conv_char);
  ploc = a5state_player_location (st);
  if (pci >= 0 && ploc != NULL && a5state_character_at_location (st, pci, ploc))
    return;                                   /* partner still here */
  /* partner gone -> implicit Farewell (if any), then end the conversation. */
  {
    const a5_character_t *prev = a5model_character (st->adv, st->conv_char);
    if (prev != NULL)
      {
        const a5_topic_t *fw =
          find_conv_node (run, prev, A5_CONV_FAREWELL, "", NULL);
        if (fw != NULL)
          {
            const char *pc = st->ctx_char; st->ctx_char = prev->key;
            emit_conv (run, a5text_describe (st, fw->conversation), out);
            st->ctx_char = pc;
          }
      }
  }
  a5state_set_conv_char (st, "");
  a5state_set_conv_node (st, "");
}

/* ----------------------------------------------------------- action: execute */

static void
run_action (a5_run_t *run, const char *kind, const char *body, int depth, sb_t *out)
{
  a5_state_t *st = run->st;
  std::vector<std::string> tk = split_ws (body);

  if (a5run_trace)
    fprintf (stderr, "    [action %s] %s\n", kind, body ? body : "");

  /* Multiple-object expansion (clsUserSession.ExecuteSingleAction's
     ReferencedObjects loop): when an operand names the plural %objects%
     reference and it resolved to a set, run this action once per item with the
     item's key substituted for "ReferencedObjects".  The recursive call sees a
     concrete key in place of the token, so it does not loop again. */
  if (st->n_ref_items > 0 && depth < 16)
    for (auto &w : tk)
      if (w == "ReferencedObjects")
        {
          for (int it = 0; it < st->n_ref_items; it++)
            {
              std::string b2;
              for (size_t wi = 0; wi < tk.size (); wi++)
                { if (wi) b2 += ' ';
                  b2 += (tk[wi] == "ReferencedObjects") ? st->ref_items[it]
                                                        : tk[wi]; }
              run_action (run, kind, b2.c_str (), depth + 1, out);
            }
          return;
        }

  if (streq (kind, "MoveObject"))
    {
      if (tk.size () < 4)
        return;
      /* clsUserSession.vb:1479 MoveObjectWhat: the source may be a single Object
         or an "Everything*" set (a group's members, everything held/worn by a
         character, inside/on an object, at a location, or with a property).
         tk[1] names the source entity; tk[2] is the destination kind; tk[3] the
         destination key.  Collect the affected object indices, then apply the
         same per-object move to each. */
      const std::string &what = tk[0];
      const char *srckey = act_key (st, tk[1].c_str ());
      const std::string &to = tk[2];
      const char *k2 = act_key (st, tk[3].c_str ());
      std::vector<int> targets;

      if (what == "Object")
        {
          int oi = a5state_object_index (st, srckey);
          if (oi >= 0) targets.push_back (oi);
        }
      else if (what == "EverythingInGroup")
        {
          for (int i = 0; i < st->adv->n_objects; i++)
            {
              const char *ok = st->adv->objects[i].key;
              int member = a5state_object_in_group (st, tk[1].c_str (), ok);
              for (int g = 0; !member && g < st->adv->n_groups; g++)
                if (streq (st->adv->groups[g].key, tk[1].c_str ()))
                  for (int m = 0; !member && m < st->adv->groups[g].n_members; m++)
                    if (streq (st->adv->groups[g].members[m], ok)) member = 1;
              if (member) targets.push_back (i);
            }
        }
      else if (what == "EverythingHeldBy" || what == "EverythingWornBy")
        {
          a5_owhere_t w = (what == "EverythingHeldBy") ? A5_OWHERE_HELD_BY
                                                       : A5_OWHERE_WORN_BY;
          for (int i = 0; i < st->adv->n_objects; i++)
            if (st->obj[i].where == w && streq (st->obj[i].key, srckey))
              targets.push_back (i);
        }
      else if (what == "EverythingInside" || what == "EverythingOn")
        {
          a5_owhere_t w = (what == "EverythingInside") ? A5_OWHERE_IN_OBJECT
                                                       : A5_OWHERE_ON_OBJECT;
          for (int i = 0; i < st->adv->n_objects; i++)
            if (st->obj[i].where == w && streq (st->obj[i].key, srckey))
              targets.push_back (i);
        }
      else if (what == "EverythingAtLocation")
        {
          for (int i = 0; i < st->adv->n_objects; i++)
            if (st->obj[i].where == A5_OWHERE_LOCATION
                && streq (st->obj[i].key, srckey))
              targets.push_back (i);
        }
      else if (what == "EverythingWithProperty")
        {
          for (int i = 0; i < st->adv->n_objects; i++)
            if (a5_prop_find (st->adv->objects[i].props,
                              st->adv->objects[i].n_props, tk[1].c_str ()))
              targets.push_back (i);
        }
      else
        return;

      for (size_t ti = 0; ti < targets.size (); ti++)
        {
          a5_objloc_t *L = &st->obj[targets[ti]];
          if (to == "ToCarriedBy")      { L->where = A5_OWHERE_HELD_BY;   L->key = k2; }
          else if (to == "ToWornBy")    { L->where = A5_OWHERE_WORN_BY;   L->key = k2; }
          else if (to == "OntoObject")  { L->where = A5_OWHERE_ON_OBJECT; L->key = k2; }
          else if (to == "InsideObject"){ L->where = A5_OWHERE_IN_OBJECT; L->key = k2; }
          else if (to == "ToPartOfObject"){ L->where = A5_OWHERE_PART_OBJECT; L->key = k2; }
          else if (to == "ToPartOfCharacter"){ L->where = A5_OWHERE_PART_CHAR; L->key = k2; }
          else if (to == "ToLocation")
            { if (streq (k2, "Hidden")) { L->where = A5_OWHERE_HIDDEN; L->key = NULL; }
              else { L->where = A5_OWHERE_LOCATION; L->key = k2; } }
          else if (to == "ToLocationGroup") { L->where = A5_OWHERE_LOCGROUP; L->key = k2; }
          else if (to == "ToSameLocationAs")
            {
              /* The target may be a character or an object (clsUserSession.vb:1570).
                 Character: place the object in the character's room (the common
                 "drop" case).  Object: copy the target object's full location
                 (where + key) — e.g. eating "four food rations" reveals the hidden
                 "three food rations" inside the same backpack. */
              int ci = a5state_character_index (st, k2);
              if (ci >= 0)
                {
                  const char *loc = st->char_loc[ci];
                  if (loc != NULL) { L->where = A5_OWHERE_LOCATION; L->key = loc; }
                  else             { L->where = A5_OWHERE_HIDDEN;   L->key = NULL; }
                }
              else
                {
                  int oj = a5state_object_index (st, k2);
                  if (oj >= 0) { L->where = st->obj[oj].where; L->key = st->obj[oj].key; }
                  else         { L->where = A5_OWHERE_HIDDEN;  L->key = NULL; }
                }
            }
        }
      return;
    }

  if (streq (kind, "AddObjectToGroup") || streq (kind, "RemoveObjectFromGroup"))
    {
      /* "Object <obj> ToGroup <grp>" / "Object <obj> FromGroup <grp>"
         (clsAction AddObjectToGroup/RemoveObjectFromGroup): runtime membership,
         e.g. the flashlight joins LightSources while switched on so the darkness
         override's "MustNot BeInSameLocationAsObject LightSources" sees it. */
      if (tk.size () < 4 || tk[0] != "Object")
        return;
      const char *obj = act_key (st, tk[1].c_str ());
      const char *grp = tk[3].c_str ();
      a5state_set_object_in_group (st, grp, obj, streq (kind, "AddObjectToGroup"));
      return;
    }

  if (streq (kind, "MoveCharacter"))
    {
      if (tk.size () < 3)
        return;
      /* The "who" set: a single Character, or one of FD's bulk source forms
         (clsAction.MoveCharacterWhoEnum, clsUserSession.vb:1689) -- e.g. the
         wand-teleport `EveryoneAtLocation Location33 ToLocation Location34`.
         Token layout is identical to MoveObject: tk[0]=who, tk[1]=who-key,
         tk[2]=to, tk[3]=to-key. */
      const std::string &who = tk[0];
      const char *whok = act_key (st, tk[1].c_str ());
      std::vector<int> cis;
      if (who == "Character")
        { int ci = a5state_character_index (st, whok); if (ci >= 0) cis.push_back (ci); }
      else if (who == "EveryoneAtLocation")
        {
          /* clsLocation.CharactersDirectlyInLocation(True): directly at the
             location (not on/in an object), Player included. */
          for (int i = 0; i < st->adv->n_characters; i++)
            if (streq (st->char_loc[i], whok) && st->char_onobj[i] == NULL)
              cis.push_back (i);
        }
      else if (who == "EveryoneInGroup")
        {
          for (int g = 0; g < st->adv->n_groups; g++)
            if (streq (st->adv->groups[g].key, whok))
              { for (int m = 0; m < st->adv->groups[g].n_members; m++)
                  { int ci = a5state_character_index (st, st->adv->groups[g].members[m]);
                    if (ci >= 0) cis.push_back (ci); }
                break; }
        }
      else if (who == "EveryoneInside" || who == "EveryoneOn")
        {
          char want_in = (who == "EveryoneInside") ? 1 : 0;
          for (int i = 0; i < st->adv->n_characters; i++)
            if (streq (st->char_onobj[i], whok) && st->char_in[i] == want_in)
              cis.push_back (i);
        }
      else if (who == "EveryoneWithProperty")
        {
          for (int i = 0; i < st->adv->n_characters; i++)
            if (a5_prop_find (st->adv->characters[i].props,
                              st->adv->characters[i].n_props, whok) != NULL
                || a5state_entity_prop (st, st->adv->characters[i].key, whok) != NULL)
              cis.push_back (i);
        }
      else
        return;
      if (cis.empty ())
        return;

      const std::string &to = tk[2];
      /* ToLocationGroup: FD computes dest.Key = group.RandomKey ONCE per action
         (clsUserSession.vb:1767), so the whole MoveCharacter draws a single
         RandomKey and sends every affected character to the SAME room -- e.g.
         Jacaranda's champagne `MoveCharacter %Player% ToLocationGroup Group7`. */
      const char *group_dest = NULL;
      if (to == "ToLocationGroup" && tk.size () >= 4)
        {
          const char *gk = act_key (st, tk[3].c_str ());
          for (int g = 0; g < st->adv->n_groups; g++)
            if (streq (st->adv->groups[g].key, gk)
                && st->adv->groups[g].n_members > 0)
              { group_dest = st->adv->groups[g].members
                  [a5rand_between (0, st->adv->groups[g].n_members - 1)];
                break; }
        }
      for (size_t ix = 0; ix < cis.size (); ix++)
        {
          int ci = cis[ix];
          const char *k1 = st->adv->characters[ci].key;
          if (to == "ToLocationGroup")
            {
              if (streq (k1, "Player"))
                enqueue_loc_trigger_tasks (run, st->char_loc[ci], group_dest);
              st->char_loc[ci] = group_dest;
              st->char_onobj[ci] = NULL;
              if (streq (k1, "Player"))
                clear_conv_if_partner_gone (run, out);
            }
          else if (to == "InDirection")
            {
              const char *dir = (tk.size () >= 4) ? act_key (st, tk[3].c_str ()) : NULL;
              const char *canon = a5parse_canonical_direction (dir);
              const char *here = st->char_loc[ci];
              const char *dest;
              if (canon == NULL) canon = dir;   /* already canonical */
              dest = here ? a5restr_exit_in_direction (st, here, canon, NULL) : NULL;
              if (dest != NULL)
                {
                  /* destination may be a location group -> first member */
                  if (a5model_location (st->adv, dest) == NULL)
                    {
                      for (int g = 0; g < st->adv->n_groups; g++)
                        if (streq (st->adv->groups[g].key, dest)
                            && st->adv->groups[g].n_members > 0)
                          { dest = st->adv->groups[g].members[0]; break; }
                    }
                  if (streq (k1, "Player"))
                    enqueue_loc_trigger_tasks (run, st->char_loc[ci], dest);
                  st->char_loc[ci] = dest;
                  st->char_onobj[ci] = NULL;   /* now "at location" */
                  if (streq (k1, "Player"))
                    clear_conv_if_partner_gone (run, out);
                }
            }
          else if (to == "ToLocation")
            {
              const char *k2 = (tk.size () >= 4) ? act_key (st, tk[3].c_str ()) : NULL;
              const char *new_loc = streq (k2, "Hidden") ? NULL : k2;
              if (streq (k1, "Player"))
                enqueue_loc_trigger_tasks (run, st->char_loc[ci], new_loc);
              st->char_loc[ci] = new_loc;
              st->char_onobj[ci] = NULL;       /* now "at location" */
              if (streq (k1, "Player"))
                clear_conv_if_partner_gone (run, out);
            }
          else if (to == "ToStandingOn" || to == "ToSittingOn" || to == "ToLyingOn")
            {
              const char *pos = (to == "ToSittingOn") ? "Sitting"
                              : (to == "ToLyingOn")   ? "Lying" : "Standing";
              const char *k2 = (tk.size () >= 4) ? act_key (st, tk[3].c_str ()) : NULL;
              free (st->char_position[ci]);
              st->char_position[ci] = strdup (pos);
              a5state_set_prop (st, k1, "CharacterPosition", pos);
              /* "TheFloor" means standing/sitting/lying at the location, not on an
                 object (clsCharacterLocation.ExistsWhere AtLocation vs OnObject). */
              if (k2 == NULL || streq (k2, "TheFloor"))
                st->char_onobj[ci] = NULL;
              else
                { st->char_onobj[ci] = k2; st->char_in[ci] = 0; }
            }
          else if (to == "ToSameLocationAs")
            {
              /* Place the character in the same place as the target character or
                 object (clsUserSession.vb:1777).  Character target: copy its
                 ExistWhere/Key (location + on/in-furniture) -- this is how Alan's
                 "follow" task `MoveCharacter Character8 ToSameLocationAs %Player%`
                 brings him along on a teleport.  Object target: the object's
                 containing location. */
              const char *k2 = (tk.size () >= 4) ? act_key (st, tk[3].c_str ()) : NULL;
              int ti = k2 ? a5state_character_index (st, k2) : -1;
              const char *old_loc = st->char_loc[ci];
              if (ti >= 0)
                {
                  st->char_loc[ci]   = st->char_loc[ti];
                  st->char_onobj[ci] = st->char_onobj[ti];
                  st->char_in[ci]    = st->char_in[ti];
                }
              else
                {
                  int oj = k2 ? a5state_object_index (st, k2) : -1;
                  const char *loc = NULL;
                  if (oj >= 0)
                    for (int li = 0; li < st->adv->n_locations; li++)
                      if (a5state_object_at_location (st, oj,
                                                      st->adv->locations[li].key, 1))
                        { loc = st->adv->locations[li].key; break; }
                  st->char_loc[ci] = loc;          /* NULL => Hidden */
                  st->char_onobj[ci] = NULL;
                }
              if (streq (k1, "Player"))
                {
                  enqueue_loc_trigger_tasks (run, old_loc, st->char_loc[ci]);
                  clear_conv_if_partner_gone (run, out);
                }
            }
          /* ToParentLocation / others: best-effort no-op for Phase 3 */
        }
      return;
    }

  if (streq (kind, "SetVariable") || streq (kind, "IncVariable")
      || streq (kind, "DecVariable"))
    {
      std::string name, value;
      int vi;
      if (!split_assignment (body, name, value))
        return;
      vi = a5state_variable_index (st, name.c_str ());
      if (vi < 0) return;
      if (streq (st->adv->variables[vi].type, "Text") && streq (kind, "SetVariable"))
        {
          char *proc = a5text_process (st, value.c_str ());
          free (st->var_text[vi]);
          st->var_text[vi] = proc;
        }
      else
        {
          long delta = eval_num_value (st, value.c_str ());
          if (streq (kind, "SetVariable"))      st->var_num[vi] = delta;
          else if (streq (kind, "IncVariable")) st->var_num[vi] += delta;
          else                                  st->var_num[vi] -= delta;
        }
      return;
    }

  if (streq (kind, "SetProperty"))
    {
      if (tk.size () < 3) return;
      const char *k1 = act_key (st, tk[0].c_str ());
      std::string val;
      for (size_t i = 2; i < tk.size (); i++)
        { if (i > 2) val += " "; val += tk[i]; }
      /* clsUserSession SetProperties: an Integer/Text/StateList/ValueList
         property value is an expression (EvaluateExpression), e.g.
         "PCASE(%text%)" for the player's typed name.  Evaluate it when it
         carries a %reference% (a plain key/state value has none and is stored
         verbatim, matching FD's Nothing->raw fallback). */
      if (val.find ('%') != std::string::npos)
        {
          char *ev = a5text_eval_expression (st, val.c_str ());
          if (ev != NULL && ev[0] != '\0') val = ev;
          free (ev);
        }
      a5state_set_prop (st, k1, tk[1].c_str (), val.c_str ());
      /* clsCharacter.ProperName tracks the CharacterProperName property
         (clsUserSession.vb:2080), so .Name/.ProperName render the new value. */
      if (tk[1] == "CharacterPosition")
        {
          int ci = a5state_character_index (st, k1);
          if (ci >= 0) { free (st->char_position[ci]); st->char_position[ci] = strdup (val.c_str ()); }
        }
      return;
    }

  if (streq (kind, "EndGame"))
    {
      st->game_over = 1;
      free (st->end_message);
      st->end_message = strdup (tk.empty () ? "" : tk[0].c_str ());
      return;
    }

  if (streq (kind, "SetTasks"))
    {
      if (tk.empty ()) return;
      if (tk[0] == "Execute" && tk.size () >= 2)
        {
          std::string key = tk[1];
          if (key == "Look")            /* built-in room view + the Look task's actions */
            {
              emit_look (run, out);
              /* FD's ExecuteTask(Look) is a full AttemptToExecuteTask, so the Look
                 task's own <Actions> run after the view -- e.g. Grandpa's Ranch
                 chains `Execute vnl_TutorialSt` (the location-gated tutorial lines)
                 off every room view via vnl_NameTyped's `Execute Look`.  emit_look
                 only renders the view, so run those actions here too.  (No-op for
                 every other corpus game: only Grandpa's Look carries actions.)

                 NOTE: FD also runs Look's Specific overrides here (e.g. Amazon's
                 `Beforeplay1` -> `Execute ts_tasCheckTime`, the startup "Date: ..."
                 line).  We deliberately do NOT, because PlayerMovement's action is
                 `Execute Look` *after* its own `Beforeplay` override already ran
                 ts_tasCheckTime; FD's per-turn response dedup (htblResponses keyed
                 by message text, clsUserSession.vb:783) collapses the two identical
                 "Date:" lines to one, but Scarier has no such dedup, so firing
                 Beforeplay1 here would double the date on every move.  The startup
                 date and the move-date dedup both await that aggregation layer. */
              const a5_task_t *lt = a5model_task (st->adv, "Look");
              if (lt != NULL && lt->actions != NULL && depth < 16)
                for (const a5_xml_node_t *c = lt->actions->first_child; c;
                     c = c->next)
                  run_action (run, c->name, c->text, depth + 1, out);
              return;
            }
          const a5_task_t *tt = a5model_task (st->adv, key.c_str ());
          if (tt != NULL && depth < 16)
            {
              /* Optional "(arg1|arg2|...)" reference passing: bind the sub-task's
                 command references to the evaluated argument keys, mirroring
                 ExecuteTask's parameter substitution (used by the stock
                 "take from others lazy" -> "take from others" re-dispatch). */
              std::string b = body ? body : "";
              size_t lp = b.find ('(');
              if (lp != std::string::npos && b.rfind (')') != std::string::npos)
                {
                  std::string argstr = b.substr (lp + 1, b.rfind (')') - lp - 1);
                  std::vector<std::string> args;
                  { std::string cur; for (char c : argstr)
                      { if (c == '|') { args.push_back (cur); cur.clear (); }
                        else cur += c; }
                    args.push_back (cur); }
                  std::vector<std::string> rnames = command_refs (tt);
                  for (size_t i = 0; i < args.size () && i < rnames.size (); i++)
                    {
                      char *val = eval_arg_to_key (st, args[i]);
                      bind_reference (st, rnames[i].c_str (), val, val);
                      free (val);
                    }
                }
              /* AttemptToExecuteTask: gate on Completed/Repeatable and the
                 task's own restrictions (with any refs just bound above) -- the
                 stock list-runner tasks (e.g. the bomb cascade) execute a list
                 of candidate tasks and rely on each one's restrictions to pick
                 the one that fires. */
              int tti = a5state_task_index (st, key.c_str ());
              if (tti >= 0 && st->task_done[tti] && !tt->repeatable)
                return;
              if (!a5restr_pass (st, tt->restrictions))
                return;
              /* FD's ExecuteTask is a full AttemptToExecuteTask, so the executed
                 task's own Specific overrides apply too -- e.g. the lazy
                 take-from-others re-dispatch lets PutSomeDry (the "skull" override
                 of TakeObjectsFromOthers) fire, moving Anno's gunpowder into the
                 held skull.  The downstream cannon puzzle reads it via the now
                 recursive BeHoldingObject (a5restr char_holds_object). */
              std::vector<ref_info> refs = refs_from_bindings (st, tt);
              execute_task_with_overrides (run, tt, refs, 0, out);
              if (tti >= 0)
                st->task_done[tti] = 1;
              ev_on_task_completed (run, key.c_str (), out);
            }
        }
      else if (tk[0] == "Unset" || tk[0] == "Clear")
        {
          if (tk.size () >= 2)
            { int ti = a5state_task_index (st, tk[1].c_str ()); if (ti >= 0) st->task_done[ti] = 0; }
        }
      return;
    }

  if (streq (kind, "Conversation"))
    {
      /* FileIO conversation-action parse: GREET/FAREWELL/ASK/TELL <key> [About|with
         '<subject>'], SAY '<subject>' to <key>, ENTERWITH/LEAVEWITH <key>.  The
         key (often ReferencedCharacter) and subject (often %text%) are resolved
         here, then ExecuteConversation runs. */
      if (tk.empty ())
        return;
      std::string verb = tk[0];
      for (char &c : verb) c = (char) toupper ((unsigned char) c);

      if (verb == "ENTERWITH")
        { if (tk.size () >= 2) a5state_set_conv_char (st, act_key (st, tk[1].c_str ())); return; }
      if (verb == "LEAVEWITH")
        { if (tk.size () >= 2 && streq (st->conv_char, act_key (st, tk[1].c_str ())))
            { a5state_set_conv_char (st, ""); a5state_set_conv_node (st, ""); }
          return; }

      int conv_type = 0;
      std::string keytok, subjraw;
      if (verb == "GREET" || verb == "FAREWELL")
        {
          conv_type = (verb == "GREET") ? A5_CONV_GREET : A5_CONV_FAREWELL;
          if (tk.size () >= 2) keytok = tk[1];
          for (size_t i = 3; i < tk.size (); i++)        /* tk[2] = "with" */
            { if (i > 3) subjraw += " "; subjraw += tk[i]; }
        }
      else if (verb == "ASK" || verb == "TELL")
        {
          conv_type = (verb == "ASK") ? A5_CONV_ASK : A5_CONV_TELL;
          if (tk.size () >= 2) keytok = tk[1];
          for (size_t i = 3; i < tk.size (); i++)        /* tk[2] = "About" */
            { if (i > 3) subjraw += " "; subjraw += tk[i]; }
        }
      else if (verb == "SAY")
        {
          conv_type = A5_CONV_COMMAND;
          if (tk.size () >= 1) keytok = tk[tk.size () - 1];   /* last = key */
          for (size_t i = 1; i + 2 < tk.size (); i++)         /* before "to <key>" */
            { if (i > 1) subjraw += " "; subjraw += tk[i]; }
        }
      else
        return;

      /* strip surrounding single quotes from the subject */
      if (subjraw.size () >= 1 && subjraw.front () == '\'')
        subjraw.erase (subjraw.begin ());
      if (subjraw.size () >= 1 && subjraw.back () == '\'')
        subjraw.pop_back ();

      const char *ckey = act_key (st, keytok.c_str ());
      char *subj = a5text_process (st, subjraw.c_str ());
      exec_conversation (run, ckey, conv_type, subj, out);
      free (subj);
      return;
    }

  if (streq (kind, "AddLocationToGroup") || streq (kind, "RemoveLocationFromGroup"))
    {
      /* "<what> <key1> [To|From]Group <grp>" (clsUserSession.vb:1917): build the
         location set from the selector, then add/remove each from the target
         group's runtime membership.  Jacaranda's day/night events run
         `AddLocationToGroup EverywhereInGroup Group2 ToGroup DarkLocations` (dusk)
         and the matching Remove (dawn), so every outdoor location inherits the
         DarkLocations ShortLocationDescription ("Everything is dark") at night. */
      if (tk.size () < 4)
        return;
      const std::string &what = tk[0];
      const char *k1  = tk[1].c_str ();
      const char *grp = tk[3].c_str ();
      int add = streq (kind, "AddLocationToGroup");
      std::vector<std::string> locs;
      if (what == "Location")
        locs.push_back (k1);
      else if (what == "LocationOf")
        {
          int ci = a5state_character_index (st, k1);
          if (ci >= 0)
            { const char *lk = st->char_loc[ci];
              if (lk != NULL && !streq (lk, "Hidden")) locs.push_back (lk); }
          else
            { int oi = a5state_object_index (st, k1);
              if (oi >= 0)
                for (int i = 0; i < st->adv->n_locations; i++)
                  if (a5state_object_at_location (st, oi, st->adv->locations[i].key, 1))
                    locs.push_back (st->adv->locations[i].key); }
        }
      else if (what == "EverywhereInGroup")
        {
          for (int g = 0; g < st->adv->n_groups; g++)
            if (streq (st->adv->groups[g].key, k1))
              { for (int m = 0; m < st->adv->groups[g].n_members; m++)
                  locs.push_back (st->adv->groups[g].members[m]);
                break; }
        }
      else if (what == "EverywhereWithProperty")
        {
          for (int i = 0; i < st->adv->n_locations; i++)
            { const a5_location_t *l = &st->adv->locations[i];
              if (a5_prop_find (l->props, l->n_props, k1) != NULL)
                locs.push_back (l->key); }
        }
      else
        return;
      for (size_t i = 0; i < locs.size (); i++)
        a5state_set_object_in_group (st, grp, locs[i].c_str (), add);
      return;
    }
}


/* ------------------------------------------------------------- run one task */

/* Render a completion message and emit it as one response: trailing whitespace
   (the newline the XML <Text> carries before </Text>) is trimmed so a "Before"
   message followed by a sub-task's output is not separated by a spurious blank
   line -- FrankenDrift trims each response the same way. */
static void
emit_completion (a5_run_t *run, const a5_xml_node_t *comp, sb_t *out)
{
  /* A completion message is real output, so its <DisplayOnce> segments must be
     retired after the first show (clsDescription.ToString marks Displayed when
     bTestingOutput is false) -- otherwise the first-visit segment shows every
     time.  Mirror a5text_view_location: render under marking_display.  Fixes
     Anno's MovingFrom5 ("step into the hotel" once, "step into the reception"
     thereafter). */
  int prev_mark = run->st->marking_display;
  run->st->marking_display = 1;
  char *m = a5text_describe (run->st, comp);
  run->st->marking_display = prev_mark;
  /* Append exactly as FD Display() does: pSpace-join to the running output, then
     the rendered text verbatim.  A whitespace-only message has no output (FD's
     bHasOutput) and is dropped; a real message keeps its own trailing newline so
     it forces a line/paragraph break before the next message, while one ending
     in text space-joins to the next. */
  if (msg_has_output (m)) { sb_pspace (out); sb_puts (out, m); }
  free (m);
}

/* Render the room view for FD's "Execute Look" (on movement / "look").  The
   stock Look task's CompletionMessage is "%Player%.Location.Description" (== the
   room view) followed by an optional restriction-gated darkness override -- a
   StartDescriptionWithThis description that, when the player is in a
   DarkLocations-group location with no LightSources object in scope, replaces
   the whole view with the game's "It is too dark..." line (clsDescription.ToString;
   the override lives in the Look task data, not the engine).  We render the room
   view exactly as before (a5text_view_location -- not back through the text
   pipeline, which would perturb whitespace in the common no-override case), then
   apply any further passing descriptions just as ToString would. */
static std::string
render_look_string (a5_run_t *run)
{
  a5_state_t *st = run->st;
  const a5_task_t *look = a5model_task (st->adv, "Look");
  const a5_xml_node_t *comp =
      look != NULL ? a5xml_child (look->node, "CompletionMessage") : NULL;
  char *view = a5text_view_location (st);
  std::string result = view ? view : "";
  free (view);

  if (comp != NULL)
    {
      int idx = 0;
      for (const a5_xml_node_t *c = comp->first_child; c; c = c->next)
        {
          const a5_xml_node_t *restr;
          const char *when, *raw;
          char *proc, *plain;
          if (strcmp (c->name, "Description") != 0)
            continue;
          if (idx++ == 0)
            continue;                          /* the room-view default */
          restr = a5xml_child (c, "Restrictions");
          if (restr != NULL && !a5restr_pass (st, restr))
            continue;
          when = a5xml_child_text (c, "DisplayWhen");
          raw  = a5xml_child_text (c, "Text");
          proc = a5text_process (st, raw ? raw : "");
          plain = a5text_render_plain (proc);
          free (proc);
          if (streq (when, "StartDescriptionWithThis"))
            result = plain;
          else                                  /* Append / StartAfterDefault */
            { if (!result.empty ()) result += "  "; result += plain; }
          free (plain);
        }
    }
  return result;
}

static void
emit_look (a5_run_t *run, sb_t *out)
{
  /* Defer the room view until the command's After children have run (FD's
     AggregateOutput Look render at final Display).  Record the insertion offset;
     execute_task_with_overrides renders and splices it once at final state. */
  if (run->defer_look && run->resp == NULL)
    {
      if (!run->look_pending)
        { run->look_pending = 1; run->look_pos = out->len; }
      return;
    }

  std::string result = render_look_string (run);
  if (run->resp != NULL)
    { resp_add_text (run, result.c_str (), true); return; }
  sb_pspace (out);
  sb_puts (out, result.c_str ());
}

static void
run_task (a5_run_t *run, const a5_task_t *t, int depth, sb_t *out)
{
  /* v5's FileIO.Load defaults a missing <MessageBeforeOrAfter> to Before (the
     class default After is overridden at load time, FileIO.vb:1618), so the
     completion message is evaluated and emitted *before* the actions run.  This
     is what makes TakeObjectsFromOthersLazy's "(from %objects%.Parent.Name)"
     render "(from the Table)" (parent still the source) ahead of the
     SetTasks-Execute sub-task's "You take ... from the Table." line. */
  const char *when = a5xml_child_text (t->node, "MessageBeforeOrAfter");
  int before = !streq (when, "After");
  const a5_xml_node_t *comp = a5xml_child (t->node, "CompletionMessage");
  const a5_xml_node_t *act = t->actions;
  int self_ti = a5state_task_index (run->st, t->key);

  if (a5run_trace)
    fprintf (stderr, "  [run task %s]\n", t->key ? t->key : "?");

  /* The built-in Look task's completion is the ADRIFT property expression
     "%Player%.Location.Description" plus a restriction-gated darkness override;
     render that CompletionMessage (emit_look) instead of the room view directly,
     so a dark location shows the game's "too dark" line. */
  if (streq (t->key, "Look"))
    {
      emit_look (run, out);
      if (self_ti >= 0) run->st->task_done[self_ti] = 1;
      if (t->actions != NULL)
        for (const a5_xml_node_t *c = t->actions->first_child; c; c = c->next)
          run_action (run, c->name, c->text, depth, out);
      return;
    }

  /* On the map path a "Before" message follows FrankenDrift's
     sBeforeActionsMessage logic (clsUserSession.vb:1176-1205): it claims its
     slot in the response order now, is rendered once before the actions, and
     after the actions is either pinned to that pre-action text (if the actions
     changed its rendering -- e.g. TakeObjectsFromOthersLazy's "(from
     %objects%.Parent.Name)", whose parent flips to the player once the take
     runs) or, for an unchanged AggregateOutput task, left deferred so a second
     item merges into its %objects% list. */
  char *resp_pre = NULL;
  int   resp_pos = -1;
  if (before && comp != NULL)
    {
      if (run->resp != NULL)
        { resp_pre = render_comp_test (run->st, comp);
          resp_pos = (int) run->resp->ents.size (); }
      else
        emit_completion (run, comp, out);
    }

  /* clsUserSession.vb:1193 sets `task.Completed = True` *before* ExecuteActions
     (vb:1194), so an action that runs or gates on this task's own completion sees
     it complete -- e.g. Anno's Translatin runs `SetTasks Execute SettingOff`, and
     SettingOff requires `Translatin Must BeComplete`.  (Callers also mark it after
     run_task; this earlier mark is the one FD relies on.) */
  if (self_ti >= 0)
    run->st->task_done[self_ti] = 1;

  if (act != NULL)
    {
      const a5_xml_node_t *c;
      for (c = act->first_child; c != NULL; c = c->next)
        run_action (run, c->name, c->text, depth, out);
    }

  if (before && comp != NULL && run->resp != NULL)
    {
      char *post = render_comp_test (run->st, comp);
      if (t->aggregate && resp_pre != NULL && post != NULL
          && strcmp (resp_pre, post) == 0)
        resp_add_comp (run, t, comp, true, resp_pos);      /* deferred + merge */
      else if (resp_pre != NULL)
        resp_add_text (run, resp_pre, true, resp_pos);     /* pinned pre-action */
      free (post);
    }
  free (resp_pre);

  if (!before && comp != NULL)
    {
      if (run->resp != NULL) resp_add_comp (run, t, comp, true);
      else                   emit_completion (run, comp, out);
    }
}

/* ------------------------------------------------------------------- events */

/* A faithful port of clsEvent's turn-based state machine: IncrementTimer /
   DoAnySubEvents / RunSubEvent / lStart / lStop, driven each turn by
   clsUserSession.TurnBasedStuff and on task completion by the EventControls.
   Only TurnBased events are handled (this engine has no real-time clock). */

static void ev_lstart (a5_run_t *run, int ei, int restart, sb_t *out);
static void ev_lstop  (a5_run_t *run, int ei, int run_subs, sb_t *out);
static void ev_do_subevents (a5_run_t *run, int ei, sb_t *out);
static void ev_on_task_completed (a5_run_t *run, const char *task_key, sb_t *out);
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
          && a5state_in_group_or_location (run->st, "Player", se->key))
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
static void
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
attempt_event_task (a5_run_t *run, const char *key, int depth, sb_t *out)
{
  a5_state_t *st = run->st;
  const a5_task_t *t = a5model_task (st->adv, key);
  int ti;
  if (t == NULL || depth > 16)
    return;
  ti = a5state_task_index (st, key);
  if (ti >= 0 && st->task_done[ti] && !t->repeatable)
    return;
  a5state_clear_refs (st);                 /* event tasks carry no command refs */
  if (!a5restr_pass (st, t->restrictions))
    return;
  run_task (run, t, depth + 1, out);
  if (ti >= 0)
    st->task_done[ti] = 1;
  ev_on_task_completed (run, key, out);
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
static void
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
static void
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

/* The player character's key (Type=Player), or "Player". */
static const char *
walk_player_key (a5_state_t *st)
{
  int i;
  for (i = 0; i < st->adv->n_characters; i++)
    if (streq (st->adv->characters[i].type, "Player"))
      return st->adv->characters[i].key;
  return "Player";
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
              && a5state_in_group_or_location (st, "Player", sw->only_apply_at))
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
   (capitalise the first letter, lower-case the rest), with pSpace's two spaces. */
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
  for (char &c : list) c = (char) tolower ((unsigned char) c);   /* ToProper rest */
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
   scan -- the first ambiguity (*have_amb/*amb/*amb_ti/*amb_ci; *amb_cantsee
   distinguishes the "can't see any" out-of-scope case from a "which?" prompt)
   and the first restriction-failure message (*have_fail/*fail_text). */
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
  raw = sb_take (out);
  fin = a5text_display_alr (run->st, raw);
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
  return fin;
}

char *
a5run_intro (a5_run_t *run)
{
  sb_t out;
  char *intro, *look;
  sb_init (&out);
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
  intro = a5text_describe (run->st, run->adv->introduction);
  if (intro[0]) { sb_puts (&out, intro); sb_puts (&out, "\n\n"); }
  free (intro);
  /* Show the start room only when <ShowFirstLocation> is set (the default);
     games like Six Silver Bullets clear it and reveal the room via a LOOK or
     the first move (clsUserSession game-start: gated on Adventure.ShowFirstRoom). */
  if (run->adv->show_first_location)
    {
      look = a5text_view_location (run->st);
      sb_puts (&out, look);
      free (look);
    }
  /* Start the Immediately events (clsUserSession init loop, after intro). */
  ev_init (run, &out);
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
    resolve_pronouns (run, in, &out);

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

/* Phase 5 save/restore (clsState / FileIO.SaveState|LoadState).  The v5 save is
   an XML game-state snapshot.  We serialise the full mutable runtime to a
   self-contained <SaveState> document and apply it back.  Entity arrays
   (objects, characters, variables, events, walks) are written in model order and
   restored positionally; the task/seen/override/displayed/look sets are sparse.
   Holder/location keys are re-interned to the model's stable strings on restore
   so they outlive the (freed) save buffer, mirroring how a5state computes them.

   Unlike FrankenDrift's format we also record the RNG state, so a restored game
   replays the identical deterministic sequence -- otherwise a fresh a5run would
   re-seed to 1234 and diverge. */

static void
sb_putc_ (sb_t *b, char c) { char t[2] = { c, '\0' }; sb_puts (b, t); }

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

char *
a5run_save (a5_run_t *run, size_t *out_len)
{
  sb_t b;
  a5_state_t *st;
  const a5_adventure_t *adv;
  int i, native;
  unsigned int rng[4];
  std::vector<const a5_xml_node_t *> dom;

  if (run == NULL)
    return NULL;
  st = run->st;
  adv = run->adv;
  sb_init (&b);
  sb_puts (&b, "<SaveState>\n");
  sb_elem_l (&b, "Version", 1);

  a5rand_get_state (&native, rng);
  sb_elem_l (&b, "RngNative", native);
  for (i = 0; i < 4; i++)
    sb_elem_l (&b, "Rng", (long) rng[i]);

  sb_elem_l (&b, "EventsRunning", run->events_running);
  sb_elem_l (&b, "GameOver", st->game_over);
  sb_elem_l (&b, "Turns", st->turns);
  sb_elem_l (&b, "EndDisplayed", st->end_displayed);
  if (st->end_message != NULL)
    sb_elem (&b, "EndMessage", st->end_message);
  if (st->conv_char != NULL && st->conv_char[0])
    sb_elem (&b, "ConvChar", st->conv_char);
  if (st->conv_node != NULL && st->conv_node[0])
    sb_elem (&b, "ConvNode", st->conv_node);
  if (st->s_it != NULL && st->s_it[0])
    sb_elem (&b, "ItRef", st->s_it);
  if (st->s_them != NULL && st->s_them[0])
    sb_elem (&b, "ThemRef", st->s_them);
  if (st->s_him != NULL && st->s_him[0])
    sb_elem (&b, "HimRef", st->s_him);
  if (st->s_her != NULL && st->s_her[0])
    sb_elem (&b, "HerRef", st->s_her);

  /* Objects (model order). */
  for (i = 0; i < adv->n_objects; i++)
    {
      sb_puts (&b, "<Object>\n");
      sb_elem_l (&b, "Where", (long) st->obj[i].where);
      if (st->obj[i].key != NULL && st->obj[i].key[0])
        sb_elem (&b, "Key", st->obj[i].key);
      sb_puts (&b, "</Object>\n");
    }

  /* Characters (model order). */
  for (i = 0; i < adv->n_characters; i++)
    {
      sb_puts (&b, "<Character>\n");
      if (st->char_loc[i] != NULL && st->char_loc[i][0])
        sb_elem (&b, "Loc", st->char_loc[i]);
      if (st->char_position[i] != NULL)
        sb_elem (&b, "Position", st->char_position[i]);
      if (st->char_onobj[i] != NULL && st->char_onobj[i][0])
        sb_elem (&b, "OnObj", st->char_onobj[i]);
      sb_elem_l (&b, "In", st->char_in ? st->char_in[i] : 0);
      sb_puts (&b, "</Character>\n");
    }

  /* Variables (model order). */
  for (i = 0; i < adv->n_variables; i++)
    {
      sb_puts (&b, "<Variable>\n");
      sb_elem_l (&b, "Num", st->var_num[i]);
      if (st->var_text[i] != NULL)
        sb_elem (&b, "Text", st->var_text[i]);
      sb_puts (&b, "</Variable>\n");
    }

  /* Completed tasks (sparse, by key). */
  for (i = 0; i < adv->n_tasks; i++)
    if (st->task_done[i])
      sb_elem (&b, "TaskDone", adv->tasks[i].key);

  /* Property overrides. */
  for (i = 0; i < st->n_ov; i++)
    {
      sb_puts (&b, "<PropOv>\n");
      sb_elem (&b, "Entity", st->ov[i].entity);
      sb_elem (&b, "Prop", st->ov[i].prop);
      sb_elem (&b, "Value", st->ov[i].value);
      sb_puts (&b, "</PropOv>\n");
    }

  /* "Seen" sets (sparse, by key). */
  if (st->obj_seen != NULL)
    for (i = 0; i < adv->n_objects; i++)
      if (st->obj_seen[i])
        sb_elem (&b, "ObjSeen", adv->objects[i].key);
  if (st->char_seen != NULL)
    for (i = 0; i < adv->n_characters; i++)
      if (st->char_seen[i])
        sb_elem (&b, "CharSeen", adv->characters[i].key);

  /* Events (model order). */
  for (i = 0; i < (int) run->events->size (); i++)
    {
      a5_event_rt &e = (*run->events)[i];
      size_t s;
      sb_puts (&b, "<Event>\n");
      sb_elem_l (&b, "Status", e.status);
      sb_elem_l (&b, "Length", e.length_value);
      sb_elem_l (&b, "Timer", e.timer_to_end);
      sb_elem_l (&b, "LastSeTime", e.last_se_time);
      sb_elem_l (&b, "LastSeIndex", e.last_se_index);
      sb_elem_l (&b, "JustStarted", e.just_started);
      sb_elem_l (&b, "NextCommand", e.next_command);
      sb_elem_l (&b, "WhenStart", e.when_start);
      sb_elem (&b, "Trigger", e.triggering_task.c_str ());
      for (s = 0; s < e.se_ft.size (); s++)
        sb_elem_l (&b, "SeFt", e.se_ft[s]);
      sb_puts (&b, "</Event>\n");
    }

  /* Walks (flattened order, as built by a5run_new). */
  for (i = 0; i < (int) run->walks->size (); i++)
    {
      a5_walk_rt &w = (*run->walks)[i];
      size_t s;
      sb_puts (&b, "<Walk>\n");
      sb_elem_l (&b, "Status", w.status);
      sb_elem_l (&b, "Length", w.length);
      sb_elem_l (&b, "Timer", w.timer_to_end);
      sb_elem_l (&b, "LastSwTime", w.last_sw_time);
      sb_elem_l (&b, "LastSwIndex", w.last_sw_index);
      sb_elem_l (&b, "JustStarted", w.just_started);
      sb_elem_l (&b, "NextCommand", w.next_command);
      sb_elem (&b, "Trigger", w.triggering_task.c_str ());
      for (s = 0; s < w.step_dur.size (); s++)
        sb_elem_l (&b, "StepDur", w.step_dur[s]);
      for (s = 0; s < w.sw_ft.size (); s++)
        sb_elem_l (&b, "SwFt", w.sw_ft[s]);
      for (s = 0; s < w.came_across.size (); s++)
        sb_elem_l (&b, "CameAcross", w.came_across[s]);
      sb_puts (&b, "</Walk>\n");
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
              { sb_elem_l (&b, "Displayed", (long) k); break; }
        }
    }

  /* SetLook stack. */
  for (i = 0; i < st->n_looks; i++)
    {
      sb_puts (&b, "<Look>\n");
      sb_elem (&b, "LocKey", st->looks[i].loc_key);
      sb_elem (&b, "Text", st->looks[i].text);
      sb_puts (&b, "</Look>\n");
    }

  sb_puts (&b, "</SaveState>\n");
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

int
a5run_restore (a5_run_t *run, const char *data, size_t len)
{
  a5_xml_doc_t *doc;
  a5_xml_node_t *root, *n;
  a5_state_t *st;
  const a5_adventure_t *adv;
  char *buf;
  unsigned int rng[4] = { 0, 0, 0, 0 };
  int native = 0, rng_i = 0, i;
  int obj_i = 0, char_i = 0, var_i = 0, ev_i = 0, wk_i = 0;
  std::vector<const a5_xml_node_t *> dom;

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
  if (root == NULL || !streq (root->name, "SaveState"))
    { a5xml_free (doc); return 0; }

  st = run->st;
  adv = run->adv;

  /* Reset the accumulating / sparse fields before applying. */
  for (i = 0; i < adv->n_tasks; i++)
    st->task_done[i] = 0;
  for (i = 0; i < st->n_ov; i++)
    { free (st->ov[i].entity); free (st->ov[i].prop); free (st->ov[i].value); }
  st->n_ov = 0;
  if (st->obj_seen != NULL)
    memset (st->obj_seen, 0, (size_t) adv->n_objects);
  if (st->char_seen != NULL)
    memset (st->char_seen, 0, (size_t) adv->n_characters);
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

  for (n = root->first_child; n != NULL; n = n->next)
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
      else if (streq (nm, "Object"))
        {
          if (obj_i < adv->n_objects)
            {
              st->obj[obj_i].where = (a5_owhere_t) child_long (n, "Where");
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
  a5xml_free (doc);
  return 1;
}
