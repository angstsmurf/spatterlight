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
#include <string>
#include <vector>

#include "a5parse.h"
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

struct a5_run_s {
  const a5_adventure_t *adv;
  a5_state_t *st;
  std::vector<int> *order;   /* task indices, ascending priority */

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
  run = new a5_run_s;
  run->adv = adv;
  run->st = a5state_new (adv);
  run->order = new std::vector<int>;
  run->amb_active = 0;
  run->amb_task_index = run->amb_command_index = -1;
  run->amb_ref_type = 'o';
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
static void
character_words (const a5_character_t *c,
                 std::vector<std::string> &allowed, std::vector<std::string> &nouns)
{
  const char *one_name[1];
  one_name[0] = c->name;
  collect_words (c->article, c->prefix, one_name, c->name ? 1 : 0, allowed, nouns);
  collect_words (NULL, NULL, c->descriptors, c->n_descriptors, allowed, nouns);
}

/* All object keys whose names match `text`, visible-to-the-player first; if any
   visible candidate matched, the non-visible ones are dropped (mirrors
   frankendrift's Visible-then-Seen scope narrowing).  The order is significant:
   the first entry is the default pick when no disambiguation is needed. */
static std::vector<const char *>
resolve_object_candidates (a5_state_t *st, const std::string &text)
{
  std::vector<const char *> vis, any;
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
    }
  return vis.empty () ? any : vis;
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
      character_words (c, allowed, nouns);
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
          character_words (&st->adv->characters[ci], allowed, nouns); }
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
              character_words (&st->adv->characters[ci], allowed, nouns); }
          int hit = 0;
          for (auto &n : nouns) if (n == lw) { hit = 1; break; }
          if (!hit) { in_all = 0; break; }
        }
      if (in_all) return lw;
    }
  return lower (ref_text);
}

/* Outcome of resolve_refine. */
enum { RR_NOMATCH = 0, RR_OK, RR_FAIL, RR_AMBIG };

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
      /* RAND(min,max): deterministic lower bound for reproducible runs
         (Phase 4 will route this through the shared erkyrath_random). */
      long lo = 0;
      const char *p = raw + 4;
      while (*p && (*p == ' ' || *p == '(')) p++;
      lo = strtol (p, NULL, 10);
      return lo;
    }
  proc = a5text_process (st, raw ? raw : "0");
  v = strtol (proc, NULL, 10);
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
          continue;                       /* failing child: parent still runs */

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
          dest = here ? a5restr_exit_in_direction (st, here, canon) : NULL;
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
            }
          return;
        }
      if (to == "ToLocation")
        {
          const char *k2 = (tk.size () >= 4) ? act_key (st, tk[3].c_str ()) : NULL;
          st->char_loc[ci] = streq (k2, "Hidden") ? NULL : k2;
          return;
        }
      if (to == "ToStandingOn" || to == "ToSittingOn" || to == "ToLyingOn")
        {
          const char *pos = (to == "ToSittingOn") ? "Sitting"
                          : (to == "ToLyingOn")   ? "Lying" : "Standing";
          free (st->char_position[ci]);
          st->char_position[ci] = strdup (pos);
          a5state_set_prop (st, k1, "CharacterPosition", pos);
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
              run_task (run, tt, depth + 1, out);
            }
        }
      else if (tk[0] == "Unset" || tk[0] == "Clear")
        {
          if (tk.size () >= 2)
            { int ti = a5state_task_index (st, tk[1].c_str ()); if (ti >= 0) st->task_done[ti] = 0; }
        }
      return;
    }

  /* AddLocationToGroup / RemoveLocationFromGroup / Conversation: Phase 4. */
}

/* ------------------------------------------------------------- run one task */

static void
run_task (a5_run_t *run, const a5_task_t *t, int depth, sb_t *out)
{
  const char *when = a5xml_child_text (t->node, "MessageBeforeOrAfter");
  int before = streq (when, "Before");
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
    {
      char *m = a5text_describe (run->st, comp);
      if (m[0]) { sb_puts (out, m); sb_puts (out, "\n"); }
      free (m);
    }

  if (act != NULL)
    {
      const a5_xml_node_t *c;
      for (c = act->first_child; c != NULL; c = c->next)
        run_action (run, c->name, c->text, depth, out);
    }

  if (!before && comp != NULL)
    {
      char *m = a5text_describe (run->st, comp);
      if (m[0]) { sb_puts (out, m); sb_puts (out, "\n"); }
      free (m);
    }
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
  return 1;
}

/* Scan general tasks for `in`.  Runs (and returns 1) on the first unique passing
   match.  Otherwise returns 0, recording -- for the caller to act on after the
   scan -- the first ambiguity (*have_amb/*amb/*amb_ti/*amb_ci) and the first
   restriction-failure message (*have_fail/*fail_text). */
static int
scan_tasks (a5_run_t *run, const std::string &in, sb_t *out,
            int *have_amb, amb_info *amb, int *amb_ti, int *amb_ci,
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
              return 1;
            }
          if (r == RR_AMBIG && !*have_amb)
            { *have_amb = 1; *amb = this_amb; *amb_ti = ti; *amb_ci = ci; }
          if (r == RR_FAIL && !*have_fail)
            {
              /* Render now, while this task's reference bindings are still live
                 (a later resolve_refine would clear them). */
              const a5_xml_node_t *fm = a5restr_fail_message (st, t->restrictions);
              if (fm != NULL)
                { char *fmsg = a5text_describe (st, fm);
                  *fail_text = fmsg; free (fmsg); *have_fail = 1; }
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
  look = a5text_view_location (run->st);
  sb_puts (&out, look);
  free (look);
  return sb_take (&out);
}

char *
a5run_input (a5_run_t *run, const char *line)
{
  a5_state_t *st = run->st;
  sb_t out;
  std::string in = lower (line ? line : "");
  std::string fail_text;
  int have_fail = 0, have_amb = 0, amb_ti = -1, amb_ci = -1;
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
        { run->amb_active = 0; return sb_take (&out); }
      if (narrowed.size () > 1)            /* a partial narrowing: keep progress */
        run->amb_keys = narrowed;
    }

  if (scan_tasks (run, in, &out, &have_amb, &amb, &amb_ti, &amb_ci,
                  &have_fail, &fail_text))
    { run->amb_active = 0; return sb_take (&out); }

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
    sb_puts (&out, fail_text.c_str ());
  else
    sb_puts (&out, "I don't understand.");
  return sb_take (&out);
}
