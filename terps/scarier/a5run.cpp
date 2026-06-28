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
#include "a5parse.h"
#include "a5rand.h"
#include "a5restr.h"
#include "a5run.h"
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

struct a5_run_s {
  const a5_adventure_t *adv;
  a5_state_t *st;
  std::vector<int> *order;   /* task indices, ascending priority */

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

  /* Every word the game recognises (clsAdventure.listKnownWords), built lazily
     the first time an unmatched command needs the "I did not understand the
     word ..." check (clsUserSession.NotUnderstood). */
  std::set<std::string> *known_words;
};

static int
streq (const char *a, const char *b)
{
  return a != NULL && b != NULL && strcmp (a, b) == 0;
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
  for (i = 0; i < adv->n_tasks; i++)
    run->order->push_back (i);
  /* TaskSorter: ascending Priority (lower value checked first). */
  std::stable_sort (run->order->begin (), run->order->end (),
                    [adv](int a, int b) {
                      return adv->tasks[a].priority < adv->tasks[b].priority;
                    });
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

/* All object keys whose names match `text`, visible-to-the-player first; if any
   visible candidate matched, the non-visible ones are dropped (mirrors
   frankendrift's Visible-then-Seen scope narrowing).  The order is significant:
   the first entry is the default pick when no disambiguation is needed. */
static std::vector<const char *>
resolve_object_candidates (a5_state_t *st, const std::string &text)
{
  std::vector<const char *> vis, seen, any;
  const char *ploc = a5state_player_location (st);
  for (int i = 0; i < st->adv->n_objects; i++)
    {
      const a5_object_t *o = &st->adv->objects[i];
      std::vector<std::string> allowed, nouns;
      object_words (o, allowed, nouns);
      if (!words_match (allowed, nouns, text))
        continue;
      any.push_back (o->key);
      if (ploc != NULL && a5state_object_at_location (st, i, ploc, 0))
        vis.push_back (o->key);
      else if (st->obj_seen != NULL && st->obj_seen[i])
        seen.push_back (o->key);   /* known but not here now */
    }
  /* Scope preference: visible, else previously-seen, else any name-match.  A
     seen-but-elsewhere object resolves to itself (examine -> "You can't see the
     X."); only when nothing is visible *or* seen does the reference stay a bag
     of never-encountered objects ("You can't see any <plural>!"). */
  if (!vis.empty ())  return vis;
  if (!seen.empty ()) return seen;
  return any;
}

static std::vector<const char *>
resolve_character_candidates (a5_state_t *st, const std::string &text)
{
  std::vector<const char *> vis, any;
  const char *ploc = a5state_player_location (st);
  for (int i = 0; i < st->adv->n_characters; i++)
    {
      const a5_character_t *c = &st->adv->characters[i];
      std::vector<std::string> allowed, nouns;
      character_words (st, c, allowed, nouns);
      if (!words_match (allowed, nouns, text))
        continue;
      any.push_back (c->key);
      if (streq (st->char_loc[i], ploc))
        vis.push_back (c->key);
    }
  return vis.empty () ? any : vis;
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
  bind ("Referenced" + stem);
  if (num == "1") bind ("Referenced" + stem + "1");
  if (base == "objects")    bind ("ReferencedObjects");
  if (base == "characters") bind ("ReferencedCharacters");
}

/* The "Which <word>?" noun: the first word of the typed reference text that
   names every candidate (clsUserSession.AmbWord); falls back to the raw text. */
static std::string
amb_word (a5_state_t *st, const std::vector<std::string> &keys,
          const std::string &ref_text, char type)
{
  for (auto &w : split_ws (ref_text.c_str ()))
    {
      std::string lw = lower (w);
      int in_all = 1;
      for (auto &k : keys)
        {
          std::vector<std::string> allowed, nouns;
          if (type == 'o')
            { int oi = a5state_object_index (st, k.c_str ());
              if (oi < 0) { in_all = 0; break; }
              object_words (&st->adv->objects[oi], allowed, nouns); }
          else
            { int ci = a5state_character_index (st, k.c_str ());
              if (ci < 0) { in_all = 0; break; }
              character_words (st, &st->adv->characters[ci], allowed, nouns); }
          int hit = 0;
          for (auto &n : nouns) if (n == lw) { hit = 1; break; }
          if (!hit) { in_all = 0; break; }
        }
      if (in_all) return lw;
    }
  return lower (ref_text);
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
  return ploc != NULL && i >= 0 && a5state_object_at_location (st, i, ploc, 0);
}

/* Outcome of resolve_refine. */
enum { RR_NOMATCH = 0, RR_OK, RR_FAIL, RR_AMBIG, RR_CANTSEE };

/* What the caller needs to raise (or carry forward) a disambiguation prompt. */
struct amb_info {
  std::string ref_name;          /* "object1", "character1", ...              */
  char        type;              /* 'o' / 'c'                                 */
  std::string ref_text;          /* the captured reference text typed         */
  std::vector<std::string> keys; /* surviving candidate keys                  */
};

/*
 * Resolve a matched command's references in scope, refining ambiguous
 * object/character references by the task's own restrictions, mirroring
 * clsUserSession.RefineMatchingPossibilitesUsingRestrictions + the post-refine
 * ambiguity check (GetGeneralTask).  Each reference is bound (first surviving
 * candidate) so restriction/action evaluation can read it.
 *
 *   RR_NOMATCH  a reference named nothing in scope -> try the next task
 *   RR_OK       every reference resolved uniquely and restrictions pass
 *   RR_FAIL     restrictions fail for the (uniquely) resolved references
 *   RR_AMBIG    a reference still has >1 candidate -> *amb describes it
 *
 * `force_name`/`force_key`: when re-running a remembered task to resolve a
 * pending ambiguity, pin that reference to the chosen key.
 */
static int
resolve_refine (a5_run_t *run, const a5_task_t *t, const a5_match_t *m,
                const char *force_name, const char *force_key, amb_info *amb)
{
  a5_state_t *st = run->st;
  struct rref { std::string name, text; char type; std::vector<std::string> keys; };
  std::vector<rref> refs;

  a5state_clear_refs (st);

  /* Pass 1: gather candidates and bind the default (first) of each. */
  for (int i = 0; i < m->n_refs; i++)
    {
      rref r;
      r.name = m->ref_name[i];
      r.text = m->ref_text[i];
      std::string base = r.name;
      while (!base.empty () && isdigit ((unsigned char) base.back ()))
        base.pop_back ();

      if (base == "object" || base == "objects" || base == "item")
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
        r.keys.assign (1, force_key);
      else
        {
          std::vector<const char *> c = (r.type == 'o')
            ? resolve_object_candidates (st, r.text)
            : resolve_character_candidates (st, r.text);
          if (c.empty ())
            return RR_NOMATCH;
          /* An object noun that names several objects, none of them in scope:
             the Runner answers "You can't see any <plural>!" rather than running
             the task (clsUserSession.DisplayAmbiguityQuestion, bCanSeeAny=False
             on MatchingPossibilities.Count>1).  Surface it so the caller can emit
             that message; single-match / any-visible cases proceed normally and
             let the task's own restriction message render. */
          if (r.type == 'o' && c.size () > 1 && amb != NULL)
            {
              int any_vis = 0, any_seen = 0;
              for (const char *k : c)
                {
                  if (obj_in_scope (st, k)) any_vis = 1;
                  int ki = a5state_object_index (st, k);
                  if (ki >= 0 && st->obj_seen != NULL && st->obj_seen[ki])
                    any_seen = 1;
                }
              if (!any_vis && !any_seen)
                {
                  amb->ref_name = r.name; amb->type = 'o'; amb->ref_text = r.text;
                  amb->keys.clear ();
                  for (const char *k : c) amb->keys.push_back (k);
                  return RR_CANTSEE;
                }
            }
          for (const char *k : c) r.keys.push_back (k);
        }
      bind_reference (st, r.name.c_str (), r.keys[0].c_str (), r.text.c_str ());
      refs.push_back (r);
    }

  /* Pass 2: refine each ambiguous reference by the task's restrictions, keeping
     only candidates that pass (others bound to their current first).  Mirrors
     RefineMatchingPossibilitesUsingRestrictions for the single-ambiguous-ref
     case; interacting multi-ref ambiguity is approximated independently. */
  for (auto &r : refs)
    {
      if (r.keys.size () <= 1)
        continue;
      std::vector<std::string> pass;
      for (auto &k : r.keys)
        {
          bind_reference (st, r.name.c_str (), k.c_str (), r.text.c_str ());
          if (a5restr_pass (st, t->restrictions))
            pass.push_back (k);
        }
      if (pass.empty ())
        {
          /* restrictions eliminate every candidate: a genuine failure.  Bind
             the first so the fail message renders against a real entity. */
          bind_reference (st, r.name.c_str (), r.keys[0].c_str (), r.text.c_str ());
          return RR_FAIL;
        }
      r.keys.swap (pass);
      bind_reference (st, r.name.c_str (), r.keys[0].c_str (), r.text.c_str ());
    }

  /* A still-ambiguous reference -> prompt. */
  for (auto &r : refs)
    if (r.keys.size () > 1)
      {
        if (amb != NULL)
          { amb->ref_name = r.name; amb->type = r.type;
            amb->ref_text = r.text; amb->keys = r.keys; }
        return RR_AMBIG;
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

/* Conversation type bits (clsAction.ConversationEnum). */
enum { A5_CONV_GREET = 1, A5_CONV_ASK = 2, A5_CONV_TELL = 4,
       A5_CONV_COMMAND = 8, A5_CONV_FAREWELL = 16 };
static void exec_conversation (a5_run_t *run, const char *char_key, int conv_type,
                               const char *subject, sb_t *out);

/* ----------------------------------------- specific-override task dispatch */

/* One resolved command reference: its general type and resolved entity key. */
struct ref_info { char type; const char *key; };   /* 'o'/'c'/'d'/'t'/'n'/'l' */

/* Recompute the ordered, resolved references for a matched command, using the
   bindings resolve_refine already established this turn. */
static std::vector<ref_info>
collect_refs (a5_state_t *st, const a5_match_t *m)
{
  std::vector<ref_info> v;
  for (int i = 0; i < m->n_refs; i++)
    {
      std::string name = m->ref_name[i];
      std::string base = name;
      while (!base.empty () && isdigit ((unsigned char) base.back ()))
        base.pop_back ();
      ref_info ri; ri.type = 't'; ri.key = NULL;
      if (base == "object" || base == "objects" || base == "item")
        { ri.type = 'o';
          ri.key = a5state_lookup_ref (st, base == "objects"
                     ? "ReferencedObjects" : "ReferencedObject"); }
      else if (base == "character" || base == "characters")
        { ri.type = 'c'; ri.key = a5state_lookup_ref (st, "ReferencedCharacter"); }
      else if (base == "direction")
        { ri.type = 'd'; ri.key = a5state_lookup_ref (st, "ReferencedDirection"); }
      else if (base == "number")
        { ri.type = 'n'; ri.key = a5state_lookup_ref (st, "ReferencedNumber"); }
      else if (base == "location")
        { ri.type = 'l'; ri.key = a5state_lookup_ref (st, "ReferencedLocation"); }
      v.push_back (ri);
    }
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
  return a5text_process (st, a.c_str ());
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

/* RefsMatchSpecifics: the child's Specifics must line up 1:1 with the resolved
   references, each non-empty key matching its reference's resolved key. */
static int
refs_match_specifics (a5_state_t *st, const a5_task_t *child,
                      const std::vector<ref_info> &refs)
{
  if (child->n_specifics != (int) refs.size ())
    return 0;
  for (int i = 0; i < child->n_specifics; i++)
    {
      const a5_specific_t *sp = &child->specifics[i];
      int matched = (sp->n_keys == 0);            /* no Key elems = match any */
      for (int k = 0; k < sp->n_keys; k++)
        {
          const char *want = specific_key (st, sp->keys[k]);
          if (want == NULL) { matched = 1; break; }       /* empty key = any */
          if (refs[i].key != NULL && streq (want, refs[i].key))
            { matched = 1; break; }
        }
      if (!matched)
        return 0;
    }
  return 1;
}

/*
 * Run a matched General task, honouring Specific child overrides (clsTask
 * Children + SpecificOverrideType).  A child whose Specifics match the resolved
 * references and whose own restrictions pass runs in place of (Override) or
 * before/after the parent's text+actions.
 */
static void
run_general (a5_run_t *run, const a5_task_t *parent, const a5_match_t *m,
             sb_t *out)
{
  a5_state_t *st = run->st;
  std::vector<ref_info> refs = collect_refs (st, m);
  int parent_text = 1, parent_actions = 1;
  std::vector<const a5_task_t *> after;

  if (parent->n_commands > 0)            /* only reference-bearing parents have children */
    for (size_t oi = 0; oi < run->order->size (); oi++)
      {
        const a5_task_t *child = &run->adv->tasks[(*run->order)[oi]];
        if (!streq (child->type, "Specific")) continue;
        if (!streq (child->general_key, parent->key)) continue;
        if (!refs_match_specifics (st, child, refs)) continue;

        const char *ot = child->override_type;
        int is_after = ot != NULL && strncmp (ot, "After", 5) == 0;
        if (is_after)
          { after.push_back (child); continue; }

        if (!a5restr_pass (st, child->restrictions))
          {
            /* A Specific task is a first-class, higher-priority task in
               frankendrift (its command is the parent's with the specifics
               substituted), so when it matches the references but its
               restrictions fail it still claims the turn: if it has a fail
               message, show it and suppress the parent (the default move/etc.);
               if it has no output, fall through and let the parent run (the
               HighestPriorityPassingTask "fails but no output -> continue"
               rule). */
            const a5_xml_node_t *fm =
              a5restr_fail_message (st, child->restrictions);
            if (fm != NULL)
              {
                char *fmsg = a5text_describe (st, fm);
                sb_puts (out, fmsg);
                free (fmsg);
                parent_text = 0;
                parent_actions = 0;
                break;
              }
            continue;                     /* no output: parent still runs */
          }

        run_task (run, child, 0, out);
        if (ot == NULL || streq (ot, "Override"))
          { parent_text = 0; parent_actions = 0; }
        else if (streq (ot, "BeforeTextOnly"))      parent_text = 0;
        else if (streq (ot, "BeforeActionsOnly"))   parent_actions = 0;
        /* BeforeTextAndActions: parent still runs both (child ran first) */
        break;                            /* one override is enough for v1 */
      }

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
      else if (parent_text)
        run_task (run, parent, 0, out);
    }

  for (auto *child : after)
    if (a5restr_pass (st, child->restrictions))
      run_task (run, child, 0, out);
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
      if (a5state_object_at_location (st, i, ploc, 0))
        st->obj_seen[i] = 1;
}

/* ContainsWord (clsUserSession): every space-separated word of `check` appears
   among the space-separated words of `sentence` (case-insensitive). */
static int
conv_contains_word (const std::string &sentence, const std::string &check)
{
  std::vector<std::string> words = split_ws (lower (sentence).c_str ());
  for (auto &c : split_ws (check.c_str ()))
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
  size_t n = m ? strlen (m) : 0;
  while (n > 0 && (m[n - 1] == '\n' || m[n - 1] == '\r'
                   || m[n - 1] == ' ' || m[n - 1] == '\t'))
    m[--n] = '\0';
  if (m != NULL && m[0])
    { sb_puts (out, m); sb_puts (out, "\n"); }
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

/* ----------------------------------------------------------- action: execute */

static void
run_action (a5_run_t *run, const char *kind, const char *body, int depth, sb_t *out)
{
  a5_state_t *st = run->st;
  std::vector<std::string> tk = split_ws (body);

  if (a5run_trace)
    fprintf (stderr, "    [action %s] %s\n", kind, body ? body : "");

  if (streq (kind, "MoveObject"))
    {
      if (tk.size () < 4 || tk[0] != "Object")
        return;
      const char *k1 = act_key (st, tk[1].c_str ());
      const std::string &to = tk[2];
      const char *k2 = act_key (st, tk[3].c_str ());
      int oi = a5state_object_index (st, k1);
      if (oi < 0) return;
      a5_objloc_t *L = &st->obj[oi];
      if (to == "ToCarriedBy")      { L->where = A5_OWHERE_HELD_BY;   L->key = k2; }
      else if (to == "ToWornBy")    { L->where = A5_OWHERE_WORN_BY;   L->key = k2; }
      else if (to == "OntoObject")  { L->where = A5_OWHERE_ON_OBJECT; L->key = k2; }
      else if (to == "InsideObject"){ L->where = A5_OWHERE_IN_OBJECT; L->key = k2; }
      else if (to == "ToLocation")
        { if (streq (k2, "Hidden")) { L->where = A5_OWHERE_HIDDEN; L->key = NULL; }
          else { L->where = A5_OWHERE_LOCATION; L->key = k2; } }
      else if (to == "ToLocationGroup") { L->where = A5_OWHERE_LOCGROUP; L->key = k2; }
      else if (to == "ToSameLocationAs")
        {
          /* drop: place the object directly in the target character's room. */
          int ci = a5state_character_index (st, k2);
          const char *loc = (ci >= 0) ? st->char_loc[ci] : NULL;
          if (loc != NULL) { L->where = A5_OWHERE_LOCATION; L->key = loc; }
          else             { L->where = A5_OWHERE_HIDDEN;   L->key = NULL; }
        }
      return;
    }

  if (streq (kind, "MoveCharacter"))
    {
      if (tk.size () < 3 || tk[0] != "Character")
        return;
      const char *k1 = act_key (st, tk[1].c_str ());
      const std::string &to = tk[2];
      int ci = a5state_character_index (st, k1);
      if (ci < 0) return;
      if (to == "InDirection")
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
              st->char_loc[ci] = dest;
              st->char_onobj[ci] = NULL;   /* now "at location" */
            }
          return;
        }
      if (to == "ToLocation")
        {
          const char *k2 = (tk.size () >= 4) ? act_key (st, tk[3].c_str ()) : NULL;
          st->char_loc[ci] = streq (k2, "Hidden") ? NULL : k2;
          st->char_onobj[ci] = NULL;       /* now "at location" */
          return;
        }
      if (to == "ToStandingOn" || to == "ToSittingOn" || to == "ToLyingOn")
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
          return;
        }
      /* ToParentLocation / others: best-effort no-op for Phase 3 */
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
      a5state_set_prop (st, k1, tk[1].c_str (), val.c_str ());
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
          if (key == "Look")            /* built-in room view */
            {
              char *v = a5text_view_location (st);
              sb_puts (out, v); sb_puts (out, "\n");
              free (v);
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
              run_task (run, tt, depth + 1, out);
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

  /* AddLocationToGroup / RemoveLocationFromGroup: later phases. */
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
  size_t n = strlen (m);
  while (n > 0 && (m[n - 1] == '\n' || m[n - 1] == '\r'
                   || m[n - 1] == ' ' || m[n - 1] == '\t'))
    m[--n] = '\0';
  if (m[0]) { sb_puts (out, m); sb_puts (out, "\n"); }
  free (m);
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

  if (a5run_trace)
    fprintf (stderr, "  [run task %s]\n", t->key ? t->key : "?");

  /* The built-in Look task's completion is the ADRIFT property expression
     "%Player%.Location.Description"; render the room view directly instead. */
  if (streq (t->key, "Look"))
    {
      char *v = a5text_view_location (run->st);
      sb_puts (out, v); sb_puts (out, "\n");
      free (v);
      if (t->actions != NULL)
        for (const a5_xml_node_t *c = t->actions->first_child; c; c = c->next)
          run_action (run, c->name, c->text, depth, out);
      return;
    }

  if (before && comp != NULL)
    emit_completion (run, comp, out);

  if (act != NULL)
    {
      const a5_xml_node_t *c;
      for (c = act->first_child; c != NULL; c = c->next)
        run_action (run, c->name, c->text, depth, out);
    }

  if (!before && comp != NULL)
    emit_completion (run, comp, out);
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
          if (m[0]) { sb_puts (out, m); sb_puts (out, "\n"); }
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
          /* Guard against a task re-triggering the control it just triggered. */
          if (!rt.triggering_task.empty () && rt.triggering_task == task_key)
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

/* clsUserSession.TurnBasedStuff: tick every turn-based event once. */
static void
ev_tick_all (a5_run_t *run, sb_t *out)
{
  int ei;
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
          rt.length_value = a5rand_between (e->length_from, e->length_to);
          ev_set_timer_to_end (run, ei,
                               a5rand_between (e->start_from, e->start_to)
                                 + rt.length_value, out);
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
      const char *exits = a5state_entity_prop (st, c->key, "CharExits");
      char *nm = a5text_char_proper_name (st, c->key);
      std::string s = std::string (nm) + " " + (exits ? exits : "exits");
      free (nm);
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
      sb_puts (out, s.c_str ()); sb_puts (out, "\n");
    }
  else if (streq (dest, ploc))                 /* entering the player's room */
    {
      const char *enters = a5state_entity_prop (st, c->key, "CharEnters");
      char *nm = a5text_char_proper_name (st, c->key);
      std::string s = std::string (nm) + " " + (enters ? enters : "enters");
      free (nm);
      if (ploc != NULL && loc_is_adjacent (st, ploc, cloc))
        {
          std::string dir = loc_direction_to (st, dest, cloc);
          if (dir != "nowhere")
            s += " from " + dir;
        }
      s += ".";
      sb_puts (out, s.c_str ()); sb_puts (out, "\n");
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
              if (m[0]) { sb_puts (out, m); sb_puts (out, "\n"); }
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
          if (!rt.triggering_task.empty () && rt.triggering_task == task_key)
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
  std::string w = amb_word (st, amb->keys, amb->ref_text, 'o');
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

/* Scan general tasks for `in`.  Runs (and returns 1) on the first unique passing
   match.  Otherwise returns 0, recording -- for the caller to act on after the
   scan -- the first ambiguity (*have_amb/*amb/*amb_ti/*amb_ci; *amb_cantsee
   distinguishes the "can't see any" out-of-scope case from a "which?" prompt)
   and the first restriction-failure message (*have_fail/*fail_text). */
static int
scan_tasks (a5_run_t *run, const std::string &in, sb_t *out,
            int *have_amb, amb_info *amb, int *amb_ti, int *amb_ci,
            int *amb_cantsee,
            int *have_fail, std::string *fail_text)
{
  a5_state_t *st = run->st;
  for (size_t oi = 0; oi < run->order->size (); oi++)
    {
      int ti = (*run->order)[oi];
      const a5_task_t *t = &run->adv->tasks[ti];
      if (!streq (t->type, "General"))
        continue;
      if (st->task_done[ti] && !t->repeatable)
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
                break;          /* stop trying this task's commands; keep scanning */
              return 1;
            }
          if (r == RR_CANTSEE && !*have_amb)
            {
              /* "You can't see any <plural>!" -- an out-of-scope object
                 reference.  Like a "which?" ambiguity (sAmbTask), this does NOT
                 claim the turn: a lower-priority task that uniquely resolves or
                 fails its restrictions *with output* (e.g. ExamineCharacter
                 resolving the same noun to an NPC and reporting "You can't see
                 the woman!") overrides it.  Mirrors GetGeneralTask, where
                 sAmbTask is shown only when no later task claims sTaskKey.  Record
                 the first such reference and keep scanning. */
              *have_amb = 1; *amb_cantsee = 1; *amb = this_amb;
              *amb_ti = ti; *amb_ci = ci;
              continue;
            }
          if (r == RR_AMBIG && !*have_amb)
            { *have_amb = 1; *amb = this_amb; *amb_ti = ti; *amb_ci = ci; }
          if (r == RR_FAIL)
            {
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
              if (fm != NULL)
                {
                  char *fmsg = a5text_describe (st, fm);
                  if (fmsg[0])
                    {
                      if (!st->adv->hp_passing)
                        { sb_puts (out, fmsg); free (fmsg); return 1; }
                      if (!*have_fail)
                        { *have_fail = 1; *fail_text = fmsg; }
                    }
                  free (fmsg);
                }
            }
          /* command matched but did not resolve to a unique pass: keep scanning */
        }
    }
  return 0;
}

/* --------------------------------------------------------------- public turn */

char *
a5run_intro (a5_run_t *run)
{
  sb_t out;
  char *intro, *look;
  sb_init (&out);
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
  return sb_take (&out);
}

/* Seed the known-words list exactly as clsUserSession.NotUnderstood does, lazily
   (the first time an unmatched command needs the word-validity check). */
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
            kw->insert (lower (w));
        }
    }
  /* Nouns: each object's article + prefix words + names. */
  for (i = 0; i < adv->n_objects; i++)
    {
      const a5_object_t *o = &adv->objects[i];
      if (o->article && o->article[0]) kw->insert (lower (o->article));
      for (auto &w : split_ws (o->prefix)) kw->insert (lower (w));
      for (j = 0; j < o->n_names; j++)
        for (auto &w : split_ws (o->names[j])) kw->insert (lower (w));
    }
  /* Characters: article + prefix words + descriptors + proper name. */
  for (i = 0; i < adv->n_characters; i++)
    {
      const a5_character_t *c = &adv->characters[i];
      if (c->article && c->article[0]) kw->insert (lower (c->article));
      for (auto &w : split_ws (c->prefix)) kw->insert (lower (w));
      for (j = 0; j < c->n_descriptors; j++)
        for (auto &w : split_ws (c->descriptors[j])) kw->insert (lower (w));
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

  /* The input names an object the player has seen and can see now, but no task
     accepted it: "I don't understand what you want to do with the X."
     (NotUnderstood's seen-noun branch).  First seen+visible match wins. */
  {
    a5_state_t *st = run->st;
    std::vector<std::string> words = split_ws (in.c_str ());
    for (int oi = 0; oi < st->adv->n_objects; oi++)
      {
        const a5_object_t *o = &st->adv->objects[oi];
        if (st->obj_seen == NULL || !st->obj_seen[oi]
            || !obj_in_scope (st, o->key))
          continue;
        std::vector<std::string> allowed, nouns;
        object_words (o, allowed, nouns);
        int hit = 0;
        for (auto &w : words)
          { std::string lw = lower (w);
            for (auto &n : nouns) if (n == lw) { hit = 1; break; }
            if (hit) break; }
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
  }

  /* All words known but nothing matched: Adventure.NotUnderstood (FileIO.vb:850
     defaults a missing <NotUnderstood> to this string). */
  sb_puts (out, "Sorry, I didn't understand that command.");
}

char *
a5run_input (a5_run_t *run, const char *line)
{
  a5_state_t *st = run->st;
  sb_t out;
  std::string in = lower (line ? line : "");
  std::string fail_text;
  int have_fail = 0, have_amb = 0, amb_ti = -1, amb_ci = -1, amb_cantsee = 0;
  amb_info amb;

  sb_init (&out);
  /* trim */
  {
    size_t a = in.find_first_not_of (" \t");
    size_t b = in.find_last_not_of (" \t");
    in = (a == std::string::npos) ? "" : in.substr (a, b - a + 1);
  }
  if (in.empty ())
    return sb_take (&out);

  /* Refresh the player's "seen" set from where things ended up last turn
     (clsUserSession.PrepareForNextTurn), so HaveSeenCharacter / "characters
     must be seen" gates reflect everyone visible by now. */
  update_seen (st);

  /* Pending disambiguation (clsUserSession.ResolveAmbiguity): try `in` as a
     clarifier first -- if it narrows the remembered candidates to exactly one,
     re-run the remembered task with that pick.  Otherwise fall through and let
     `in` be parsed as a fresh command; only if nothing runs do we re-prompt. */
  if (run->amb_active)
    {
      std::vector<std::string> narrowed =
        possible_keys (st, run->amb_keys, in, run->amb_ref_type);
      if (narrowed.size () == 1
          && run_remembered (run, narrowed[0].c_str (), &out))
        { run->amb_active = 0; ev_tick_all (run, &out); return sb_take (&out); }
      if (narrowed.size () > 1)            /* a partial narrowing: keep progress */
        run->amb_keys = narrowed;
    }

  if (scan_tasks (run, in, &out, &have_amb, &amb, &amb_ti, &amb_ci,
                  &amb_cantsee, &have_fail, &fail_text))
    { run->amb_active = 0; ev_tick_all (run, &out); return sb_take (&out); }

  if (have_amb && amb_cantsee)
    {
      /* No later task claimed the turn: emit the deferred "You can't see any
         <plural>!" and advance the turn (the out-of-scope reference is a dead
         end, not a pending prompt). */
      run->amb_active = 0;
      emit_cantsee (st, &amb, &out);
      ev_tick_all (run, &out);
      return sb_take (&out);
    }

  if (have_amb)
    {
      /* Remember the ambiguity and prompt; the next turn resolves it. */
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
      return sb_take (&out);
    }

  if (run->amb_active)
    {
      /* A clarifier that neither resolved nor ran a fresh command: re-prompt. */
      sb_puts (&out, build_amb_prompt (st, run->amb_word, run->amb_keys,
                                       run->amb_ref_type).c_str ());
      return sb_take (&out);
    }

  if (have_fail)
    {
      /* A HighestPriorityPassingTask game: no task passed, so show the
         highest-priority failing task's recorded message.  It is non-empty
         output, so the turn advances (clsUserSession: sOutputText <> "" =>
         TurnBasedStuff). */
      run->amb_active = 0;
      sb_puts (&out, fail_text.c_str ());
      ev_tick_all (run, &out);
    }
  else if (system_command (run, in, &out))
    { run->amb_active = 0; }
  else
    not_understood (run, in, &out);
  return sb_take (&out);
}
