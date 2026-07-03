/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- task execution.
 *
 * The action layer clsUserSession runs once a task is chosen: ExecuteActions /
 * ExecuteSingleAction (the core action set -- MoveObject/MoveCharacter,
 * Set/Inc/Dec, SetProperty, SetTasks Execute, conversation, EndGame), the
 * specific-override task dispatch, the per-top-level-command response
 * aggregation (htblResponses dedup/merge) and the run_task / run_general
 * drivers.  Split out of a5run.cpp; the shared run struct and the entry points
 * it exposes (run_task, run_general, update_seen, conv_contains_word) live in
 * a5run_internal.h.
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
  if (streq (tok, "%Player%"))
    resolved = a5state_player_key (st);  /* dynamic: whoever is the player now */
  else if (streq (tok, "Player"))
    resolved = "Player";                 /* literal model key of the player slot */
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
  /* Not plain arithmetic: the value may be a function call the arith evaluator
     doesn't know -- most importantly a *lowercase* rand(min,max) (the uppercase
     RAND fast path above only matches "RAND...").  Tingalan's Roller sets
     Randbetwee = "rand(1,9)", Witsroll = "rand(0,%playerwits%)" etc. every turn;
     without this they all evaluate to 0 (no draw) and the whole encounter/combat
     RNG is dead.  FD evaluates a SetVariable value through EvaluateExpression, so
     defer to the s-expression engine, which draws for rand()/oneof() via the same
     a5rand_between as the uppercase path.  %vars% are already substituted, so
     rand(0,%playerwits%) is now rand(0,N).  Only used when it yields a number, so
     a non-numeric junk value still falls through to the leading-integer strtol. */
  {
    char *sv = a5_eval_sexpr (proc);
    char *end = NULL;
    long e = strtol (sv, &end, 10);
    int got = (sv != end);
    free (sv);
    if (got) { free (proc); return e; }
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

static void run_action (a5_run_t *run, const char *kind, const char *body,
                        int depth, sb_t *out);
static void emit_look (a5_run_t *run, sb_t *out);
static std::string render_look_string (a5_run_t *run);
static void emit_completion (a5_run_t *run, const a5_xml_node_t *comp, sb_t *out);
static std::vector<std::string> current_obj_ref_keys (a5_state_t *st);

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
  /* The plural %objects%/%characters% slot resolves through its OWN alias.
     When the command also carries a singular %object% (`hide %objects% in
     %object%`), bind_reference deliberately does NOT alias the plural onto
     ReferencedObject (FD's GetReference keys ReferencedObject to the "object1"
     reference only, clsUserSession.vb:3990), so reading ReferencedObject here
     would yield the singular container -- Dwarf's `hide axe in vegetation`
     matched cl_HideObjInV1's [Sword, cl_Vegetation] specifics against
     [vegetation, vegetation] and missed.  ReferencedObjects holds the current
     item on both the per-item plural loop and the single-item direct path. */
  if (base == "objects")
    ri.key = a5state_lookup_ref (st, "ReferencedObjects");
  else if (base == "characters")
    ri.key = a5state_lookup_ref (st, "ReferencedCharacters");
  if (ri.key == NULL && !num.empty () && num != "1")
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
      if (nm == "Player") return strdup (a5state_player_key (st));
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
    return a5state_player_key (st);
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
  int r = match_specifics_vec (st, specs, refs);
  if (getenv ("A5DBG_SPEC") != NULL)
    {
      fprintf (stderr, "[spec] child=%s parent=%s r=%d refs:", child->key,
               parent ? parent->key : "-", r);
      for (auto &ri : refs)
        fprintf (stderr, " (%c,%s)", ri.type, ri.key ? ri.key : "NULL");
      fprintf (stderr, " specs:");
      for (auto *sp : specs)
        for (int k = 0; k < sp->n_keys; k++)
          fprintf (stderr, " [%s]", sp->keys[k]);
      fprintf (stderr, "\n");
    }
  return r;
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
/* Snapshot of the per-turn reference bindings, captured when a deferred
   (AggregateOutput) message is queued and restored before it is re-rendered at
   flush.  FrankenDrift stores the message's NewReferences alongside it and
   reassigns them at Display (clsUserSession.vb:851-854), so the deferred
   "%object%.Description"/"(to %character%)"/"%direction%" tokens resolve against
   the entity the command referenced -- not whatever the binding table holds by
   the end of the command. */
struct ref_snap {
  char ref_name[16][32];
  char ref_value[16][256];
  int  n_refbind;
  int  ref_object1_plural;
  int  ref_character1_plural;
};

struct resp_entry {
  bool is_pass;
  bool aggregate;                 /* key is comp ptr; render deferred to flush  */
  bool is_look;                   /* deferred room view: render_look_string()   */
  bool has_snap;                  /* snap holds the refs to restore at flush    */
  const a5_xml_node_t *comp;      /* aggregate: re-rendered at flush            */
  const a5_xml_node_t *fail_comp; /* fail: the restriction <Message> node       */
  std::string rendered;           /* non-aggregate / fail: final text           */
  std::vector<std::string> obj_keys;  /* merged %objects% items (aggregate)     */
  std::string obj2;               /* %object2% snapshot (first occurrence)      */
  std::string item;               /* the single item this entry was made for    */
  ref_snap snap;
  resp_entry () : is_pass (false), aggregate (false), is_look (false),
                  has_snap (false), comp (NULL), fail_comp (NULL) {}
};

/* Capture / restore the live reference-binding table (a5state_bind_ref store +
   the plural-derived flags), so a deferred message renders with the references
   the command had when it produced the message. */
static void
ref_snap_take (a5_state_t *st, ref_snap *s)
{
  memcpy (s->ref_name, st->ref_name, sizeof s->ref_name);
  memcpy (s->ref_value, st->ref_value, sizeof s->ref_value);
  s->n_refbind = st->n_refbind;
  s->ref_object1_plural = st->ref_object1_plural;
  s->ref_character1_plural = st->ref_character1_plural;
}
static void
ref_snap_restore (a5_state_t *st, const ref_snap *s)
{
  memcpy (st->ref_name, s->ref_name, sizeof s->ref_name);
  memcpy (st->ref_value, s->ref_value, sizeof s->ref_value);
  st->n_refbind = s->n_refbind;
  st->ref_object1_plural = s->ref_object1_plural;
  st->ref_character1_plural = s->ref_character1_plural;
}
struct resp_map {
  std::vector<resp_entry> ents;
  std::string cur_item;           /* item currently being iterated              */
  size_t nmut = 0;                /* count of real adds AND aggregate merges     */
};

/* The "did this produce a response" probe used by the override scan: with a map
   active it is the response-mutation count, else the byte length of out.  A count
   (not ents.size()) is required because an AggregateOutput task's second+ item
   MERGES into an existing entry (obj_keys grows, ents.size() does NOT): FD treats
   such a merging AddResponse as output that claims the turn (clsUserSession
   vb:1295), so the override scan must break on it too -- otherwise it falls
   through to a lower-priority sibling (e.g. AoS `put all in bag`: the 2nd item's
   merge into PutObjectI let the scan reach cl_PutAllInBa, whose `EverythingHeldBy
   Player` action then dumped the food/money into the bag). */
static size_t
sink_len (a5_run_t *run, sb_t *out)
{
  return run->resp != NULL ? run->resp->nmut : out->len;
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
  rm->nmut++;                     /* a new response is output (see sink_len) */
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
  const std::string &item = rm->cur_item;
  /* The AggregateOutput deferral (store the comp, re-render at flush over the
     merged %objects% list) only earns its keep inside a *plural* iteration, where
     a later item merges into the same entry.  In a single-reference command
     (cur_item empty -- e.g. movement's `Beforeplay` "You move north." After
     message, and `ts_tasCheckTime`'s "Date: ...") render eagerly and dedup on the
     text: re-rendering at flush is fragile (the `move[//s]` verb conjugation reads
     transient per-turn context that has changed by the end of the command), and
     two identical "Date:" lines still collapse via the text dedup. */
  bool aggregate = t != NULL && t->aggregate && !item.empty ();

  if (aggregate)
    {
      for (auto &e : rm->ents)
        if (e.aggregate && e.comp == comp && e.is_pass == is_pass)
          {
            if (!item.empty ()
                && std::find (e.obj_keys.begin (), e.obj_keys.end (), item)
                     == e.obj_keys.end ())
              { e.obj_keys.push_back (item); rm->nmut++; }  /* merge is output */
            return;
          }
      resp_entry e;
      e.is_pass = is_pass; e.aggregate = true; e.comp = comp;
      const char *o2 = a5state_lookup_ref (st, "ReferencedObject2");
      e.obj2 = o2 ? o2 : "";
      if (!item.empty ()) e.obj_keys.push_back (item);
      e.item = item;
      ref_snap_take (st, &e.snap); e.has_snap = true;
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

/* Add a restriction-fail message (the restriction's <Message> node `fm`) as a
   fail response, mirroring FrankenDrift's htblResponsesFail keyed by the raw
   message template (clsUserSession AddResponse, vb:1301-1307): a second item that
   fails the SAME restriction node merges its %objects% item into the existing
   entry instead of emitting a separate message, so at flush the template renders
   ONCE over the merged set.  AoS `put all in bag` re-fails the two nested coins on
   the general `Must BeHeldByCharacter %Player%`; FD produces the single "You are
   not carrying the gold gonks and the silver ginks." (which the game's
   cl_YouAreNotC1 ALR then blanks), where Scarier used to show two singular fails
   that no ALR could match.  A single-item fail (the common case) keeps its
   eagerly-rendered singular text, so every non-aggregating fail stays byte-
   identical to the old resp_add_text path. */
static void
resp_add_fail (a5_run_t *run, const a5_xml_node_t *fm)
{
  resp_map *rm = run->resp;
  a5_state_t *st = run->st;
  const std::string &item = rm->cur_item;

  /* Second+ item failing the SAME restriction node: merge its %objects% key
     (htblResponsesFail's ContainsKey branch, vb:1303-1307). */
  if (!item.empty ())
    for (auto &e : rm->ents)
      if (!e.is_pass && e.fail_comp == fm)
        {
          if (std::find (e.obj_keys.begin (), e.obj_keys.end (), item)
                == e.obj_keys.end ())
            { e.obj_keys.push_back (item); rm->nmut++; }   /* merge is output */
          return;
        }

  /* First occurrence: render eagerly at the failing item's state (identical to
     the old path) and text-dedup against the existing fails, so two DIFFERENT
     restrictions that render the same string still collapse. */
  char *f = a5text_describe (st, fm);
  if (!msg_has_output (f)) { free (f); return; }
  std::string r = f;
  free (f);
  for (auto &e : rm->ents)
    if (!e.aggregate && !e.is_pass && e.rendered == r) return;
  resp_entry e;
  e.is_pass = false; e.aggregate = false; e.comp = NULL; e.fail_comp = fm;
  e.rendered = r; e.item = item;
  if (!item.empty ()) e.obj_keys.push_back (item);
  ref_snap_take (st, &e.snap); e.has_snap = true;
  resp_insert (rm, e, -1);
}

/* Flush the map to `out`: pass responses first in insertion order (aggregate
   entries re-rendered over their merged %objects% set), then any fail response
   whose item was not covered by a pass (clsUserSession.vb:804-855). */
static void
resp_flush (a5_run_t *run, resp_map *rm, sb_t *out)
{
  a5_state_t *st = run->st;
  std::set<std::string> passed;
  std::set<std::string> look_seen;
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
        /* FD drops a fail whose every reference item also produced a pass
           (clsUserSession.vb:812-834).  A merged fail (obj_keys accumulated by
           resp_add_fail) is dropped only when ALL its items passed; a fail with no
           merged set falls back to the single-item check (the old resp_add_text
           path -- movement fails, SetTasks-Execute fails). */
        if (!e.is_pass)
          {
            if (!e.obj_keys.empty ())
              {
                bool all_passed = true;
                for (auto &k : e.obj_keys)
                  if (!passed.count (k)) { all_passed = false; break; }
                if (all_passed) continue;
              }
            else if (!e.item.empty () && passed.count (e.item))
              continue;
          }

        std::string text;
        if (e.is_look)
          {
            /* A deferred room view (Execute Look): rendered now, at the command's
               final state, mirroring FrankenDrift's AggregateOutput Look whose
               function replacement is deferred to Display.  So a move whose After
               children relocated an NPC lists it at its new spot. */
            text = render_look_string (run);
            /* FD keys the stock Look's AggregateOutput on its response template
               (htblResponsesPass), so a command that Executes Look more than once
               -- e.g. Bug Hunt's cl_ZMovePlaye, which runs Execute Look after the
               player moves AND again after the marine squad follows -- shows a
               single room view, not one per Execute.  Mirror the non-resp path's
               pass_seen dedup: collapse identical deferred views. */
            if (!look_seen.insert (text).second)
              continue;
          }
        else if (e.aggregate)
          {
            /* Restore the references this message was queued with, so its
               deferred %object%/%character%/%direction% tokens resolve as they
               did when the task ran (FD reassigns NewReferences at Display). */
            if (e.has_snap) ref_snap_restore (st, &e.snap);
            /* A genuine plural %objects% command merged several items into this
               entry: rebind the whole set (overriding the snapshot's singular
               object binding) so the completion renders its "a, b and c" list.
               A single-reference command has no merged items (obj_keys empty) and
               keeps the restored snapshot's singular binding untouched. */
            if (!e.obj_keys.empty ())
              {
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
              }
            int pm = st->marking_display; st->marking_display = 1;
            char *m = a5text_describe (st, e.comp);
            st->marking_display = pm;
            if (m != NULL) text = m;
            free (m);
          }
        else if (e.fail_comp != NULL && e.obj_keys.size () > 1)
          {
            /* A merged fail: re-render the restriction template ONCE over the
               combined %objects% set, so %TheObjects[%objects%]% lists every
               failing item (FD Display over the accumulated htblResponsesFail
               NewReferences).  The aggregate string is what a game ALR can match
               at the display boundary -- AoS "You are not carrying the gold gonks
               and the silver ginks." blanked by cl_YouAreNotC1.  A single-item
               fail (obj_keys.size()==1) keeps its eager render below, byte-exact. */
            if (e.has_snap) ref_snap_restore (st, &e.snap);
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
            char *m = a5text_describe (st, e.fail_comp);
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
                  resp_add_fail (run, fm);
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
  char *prev_look_pinned = run->look_pinned;
  int enable_look_defer = run->resp == NULL && !defer_text && !after.empty ();
  if (enable_look_defer)
    { run->defer_look = 1; run->look_pending = 0; run->look_pinned = NULL; }

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
  char *pinned_view = NULL;
  if (enable_look_defer)
    {
      if (run->look_pending) look_deferred = 1;
      pinned_view = run->look_pinned;          /* consumed below (or freed) */
      run->look_pending = prev_look_pending;
      run->look_pos = prev_look_pos;
      run->look_pinned = prev_look_pinned;
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
      /* A pinned view (the Look dance's two test renders differed -- a random
         pick moved) is emitted verbatim; otherwise render at the final state. */
      std::string view = pinned_view != NULL ? std::string (pinned_view)
                                             : render_look_string (run);
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
  free (pinned_view);
}

/* Run a matched General task (the directly command-matched parent), honouring
   its Specific child overrides. */
void
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
  int objidx = -1, diridx = -1;
  for (int i = 0; i < m->n_refs; i++)
    {
      if (strcmp (m->ref_name[i], "objects") == 0 && objidx < 0) objidx = i;
      if (strncmp (m->ref_name[i], "direction", 9) == 0) diridx = i;
    }

  /* A command runs under a per-command response map (FrankenDrift's
     htblResponsesPass/Fail, cleared at the top of AttemptToExecuteTask and flushed
     once at the end) in two cases.  (1) A genuine plural %objects% command
     iterates its items one at a time (ExecuteSubTasks), aggregating completion /
     fail messages.  (2) A *movement* command (a %direction% reference): its
     `Beforeplay` override and its `Execute Look` -> `Beforeplay1` both run
     `ts_tasCheckTime`, emitting the same "Date: ..." line, and the map dedups the
     two into one (clsUserSession.vb:783).  Other single-reference commands stay
     on the direct path -- they never produce a same-turn duplicate, and routing
     conversation / multi-restriction output through the map's pass-then-fail
     reorder would perturb their byte-exact ordering.  A SetTasks-Execute sub-task
     shares whichever map is active (it does not flush). */
  bool plural = (objidx >= 0 && st->n_ref_items > 1);
  bool movement = (diridx >= 0);
  /* An "all" command whose %objects% resolved to items of which NONE pass shows
     the general task's <FailOverride> ("You are not carrying anything."),
     discarding the per-item override fail messages -- FD's AttemptToExecuteTask
     finalize (clsUserSession.vb:789: htblResponsesPass.Count=0 AndAlso
     FailOverride<>"" AndAlso ContainsWord(sInput,"all") -> Display(FailOverride)).
     This must fire even when "all" collapses to a SINGLE surviving item: a
     genuine singular command (no "all") stays on the direct path and keeps its
     object-specific fail message (`put food in bag` -> "makes a mess"), but `put
     all in bag` with only the un-baggable food held must show the FailOverride,
     not the food's "makes a mess" (AoS `put all in bag` after the pouch/whetstone
     are already bagged).  Route it through the response map so the child fail
     messages buffer and can be replaced by the FailOverride at flush. */
  bool all_failover = (objidx >= 0 && parent->fail_override != NULL
                       && conv_contains_word (m->ref_text[objidx], "all"));
  bool use_map = plural || movement || all_failover;

  resp_map rm;
  if (use_map) run->resp = &rm;

  /* Per-command response scope for SetTasks-Execute'd tasks (FD's
     htblResponsesPass/Fail, live for the whole AttemptToExecuteTask): buffers
     Execute'd-task restriction-fail messages and cancels the ones whose
     objects also produced a pass response, at the flush below. */
  exec_resp_scope escope;
  exec_resp_scope *prev_escope = run->exec_scope;
  run->exec_scope = &escope;

  if (plural)
    {
      std::vector<const char *> items (st->ref_items,
                                       st->ref_items + st->n_ref_items);
      const char *rtext = m->ref_text[objidx];
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
    }
  else
    {
      std::vector<ref_info> refs = collect_refs (st, m);
      execute_task_with_overrides (run, parent, refs, 0, out);
    }

  run->exec_scope = prev_escope;
  if (use_map)
    {
      run->resp = NULL;
      /* No child override passed for an "all" command: replace the buffered
         per-item fail messages with the general task's FailOverride (FD's
         htblResponsesPass.Count=0 branch). */
      if (all_failover)
        {
          bool any_pass = false;
          for (auto &e : rm.ents)
            if (e.is_pass) { any_pass = true; break; }
          if (!any_pass)
            {
              char *fo = a5text_describe (st, parent->fail_override);
              rm.ents.clear ();
              rm.nmut = 0;
              if (msg_has_output (fo))
                {
                  resp_entry e;
                  e.is_pass = true;
                  e.rendered = fo ? fo : "";
                  rm.ents.push_back (e);
                  rm.nmut = 1;
                }
              free (fo);
            }
        }
      resp_flush (run, &rm, out);
    }
  exec_scope_flush (run, &escope, out);
}

/* Flush a SetTasks-Execute response scope: emit each buffered fail message
   (in order) unless every object it was evaluated against also produced a
   pass response -- FD's flush loop, clsUserSession.vb:804-849, where a fail
   whose reference items all match a pass's is dropped and the survivors are
   Display'd after the pass responses. */
void
exec_scope_flush (a5_run_t *run, exec_resp_scope *sc, sb_t *out)
{
  (void) run;
  for (auto &f : sc->fails)
    {
      bool cancelled = !f.second.empty ();
      for (auto &k : f.second)
        if (!sc->pass_refs.count (k)) { cancelled = false; break; }
      if (!cancelled)
        { sb_pspace (out); sb_puts (out, f.first.c_str ()); }
    }
  sc->fails.clear ();
}

/* ------------------------------------------------------------ conversation */

/* Mark every character the player can currently see as "seen" (player-centric
   clsCharacter.HasSeenCharacter, set each turn by clsUserSession's
   PrepareForNextTurn).  The player is always considered self-seen. */
void
update_seen (a5_state_t *st)
{
  const char *ploc = a5state_player_location (st);
  int i;
  if (st->char_seen == NULL)
    return;
  for (i = 0; i < st->adv->n_characters; i++)
    {
      if (streq (st->adv->characters[i].key, a5state_player_key (st)))
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
  /* Catch-all for player moves that bypass the MoveCharacter action (event
     moves, walks): the location the player ends the turn in has been seen. */
  a5state_mark_loc_seen (st, ploc);
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
int
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
emit_conv (const char *rendered_owned, sb_t *out)
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
              emit_conv (a5text_describe (st, fw->conversation), out);
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
          emit_conv (a5text_describe (st, intro->conversation), out);
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
      emit_conv (a5text_describe (st, topic->conversation), out);
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
      emit_conv (a5text_process (st, msg.c_str ()), out);
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
            emit_conv (a5text_describe (st, fw->conversation), out);
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
              if (streq (k1, a5state_player_key (st)))
                enqueue_loc_trigger_tasks (run, st->char_loc[ci], group_dest);
              st->char_loc[ci] = group_dest;
              st->char_onobj[ci] = NULL;
              if (streq (k1, a5state_player_key (st)))
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
                  if (streq (k1, a5state_player_key (st)))
                    enqueue_loc_trigger_tasks (run, st->char_loc[ci], dest);
                  st->char_loc[ci] = dest;
                  st->char_onobj[ci] = NULL;   /* now "at location" */
                  if (streq (k1, a5state_player_key (st)))
                    clear_conv_if_partner_gone (run, out);
                }
            }
          else if (to == "ToLocation")
            {
              const char *k2 = (tk.size () >= 4) ? act_key (st, tk[3].c_str ()) : NULL;
              const char *new_loc = streq (k2, "Hidden") ? NULL : k2;
              if (streq (k1, a5state_player_key (st)))
                enqueue_loc_trigger_tasks (run, st->char_loc[ci], new_loc);
              st->char_loc[ci] = new_loc;
              st->char_onobj[ci] = NULL;       /* now "at location" */
              if (streq (k1, a5state_player_key (st)))
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
          else if (to == "InsideObject" || to == "OntoObject")
            {
              /* clsUserSession MoveCharacterToEnum.InsideObject / OntoObject:
                 the character stays at its current location but is now in/on the
                 object (clsCharacterLocation.ExistsWhere InsideObject/OnObject).
                 char_in distinguishes the two so BeInsideObject / BeOnObject
                 read it back correctly -- e.g. FBA's `hide in niche`
                 (MoveCharacter Player InsideObject cl_Niche1) which the custodian
                 "goes past" check gates on. */
              const char *k2 = (tk.size () >= 4) ? act_key (st, tk[3].c_str ()) : NULL;
              if (k2 != NULL)
                { st->char_onobj[ci] = k2;
                  st->char_in[ci] = (to == "InsideObject") ? 1 : 0; }
            }
          else if (to == "ToParentLocation")
            {
              /* clsUserSession MoveCharacterToEnum.ToParentLocation: drop out of
                 the containing object back onto the floor of the SAME location
                 (clsCharacter.Move ... the character's location is unchanged, only
                 the on/in-object binding clears) -- FBA's `out` of the niche. */
              st->char_onobj[ci] = NULL;
              st->char_in[ci] = 0;
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
              if (streq (k1, a5state_player_key (st)))
                {
                  enqueue_loc_trigger_tasks (run, old_loc, st->char_loc[ci]);
                  clear_conv_if_partner_gone (run, out);
                }
            }
          else if (to == "ToSwitchWith")
            {
              /* clsUserSession MoveCharacterToEnum.ToSwitchWith: swap character k1
                 with k2.  When either is the current player, DON'T move anyone --
                 change which character IS the player (ADRIFT's BECOME <character>
                 mechanism).  The player viewpoint, %Player% resolution and scope
                 then follow the new character; the old player stays put, now an
                 NPC.  Otherwise, actually exchange the two characters' locations. */
              const char *k2 = (tk.size () >= 4) ? act_key (st, tk[3].c_str ()) : NULL;
              const char *pk = a5state_player_key (st);
              int bi = k2 ? a5state_character_index (st, k2) : -1;
              if (k2 == NULL || bi < 0)
                { /* unresolved target: no-op */ }
              else if (streq (k1, pk) || streq (k2, pk))
                {
                  const char *old_loc = a5state_player_location (st);
                  int new_ci = streq (k1, pk) ? bi : ci;
                  const char *new_player = st->adv->characters[new_ci].key;
                  st->player_key = new_player;   /* stable model key pointer */
                  /* HasSeenObject/-Location/-Character are per-character in FD:
                     the new viewpoint must not inherit the old player's
                     sightings (BugHunt: Jones must not "have seen" the elevator
                     sign that Davey saw). */
                  a5state_switch_seen (st, new_ci);
                  enqueue_loc_trigger_tasks (run, old_loc,
                                             a5state_player_location (st));
                  clear_conv_if_partner_gone (run, out);
                  a5state_mark_loc_seen (st, a5state_player_location (st));
                }
              else
                {
                  /* Neither is the player: exchange the two characters' places. */
                  const char *tloc = st->char_loc[bi];
                  const char *tonobj = st->char_onobj[bi];
                  char tin = st->char_in[bi];
                  st->char_loc[bi]   = st->char_loc[ci];
                  st->char_onobj[bi] = st->char_onobj[ci];
                  st->char_in[bi]    = st->char_in[ci];
                  st->char_loc[ci]   = tloc;
                  st->char_onobj[ci] = tonobj;
                  st->char_in[ci]    = tin;
                }
            }
          /* other MoveCharacterToEnum forms: best-effort no-op */

          /* clsCharacter.Move marks the destination location seen for the
             moving character; our set is player-centric (like obj_seen). */
          if (streq (k1, a5state_player_key (st)))
            a5state_mark_loc_seen (st, st->char_loc[ci]);
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
          /* FD evaluates the RHS as an expression, so a bare string-literal value
             unwraps to its contents.  ADRIFT writes such literals with their own
             quotes inside the assignment's delimiters (Playeraxe = ""an"" or
             = "'an'"); split_assignment stripped one outer layer, leaving "an" /
             'an'.  Strip a remaining fully-surrounding matched quote pair so
             %playeraxe% renders `an`, not `'an'`/`"an"` (Tingalan inventory).
             Only when it is a lone literal (no interior same-quote), so a
             concatenation like "a" & "b" is left for a5text_process untouched. */
          std::string tv = value;
          if (tv.size () >= 2 && (tv[0] == '"' || tv[0] == '\'')
              && tv[tv.size () - 1] == tv[0]
              && tv.find (tv[0], 1) == tv.size () - 1)
            tv = tv.substr (1, tv.size () - 2);
          /* Store WITHOUT the display-time auto-capitalisation: FD keeps the raw
             evaluated value (Playeraxe = "an") and only capitalises when it is
             rendered in sentence context.  a5text_process would cap the lone value
             at position 0 ("an" -> "An"), so %playeraxe% mid-sentence wrongly
             shows "You have An axe" instead of "an axe". */
          char *proc = a5text_process_nocap (st, tv.c_str ());
          free (st->var_text[vi]);
          st->var_text[vi] = proc;
        }
      else
        {
          /* clsUserSession.vb:2144: a task modifies the built-in Score at most
             once ever -- gated on clsTask.Scored, set on first Score change and
             never reset (persisted across save/restore).  Without this a
             repeatable scoring task, or one System scoring task Execute'd by two
             distinct non-repeatable tasks (FBA's cl_MakeLeash, reached via both
             `make a leash` cl_TieRopeToD1 and `tie rope to dog` cl_TieDogWith),
             re-awards its points.  Non-Score variables are never gated. */
          int is_score = streq (st->adv->variables[vi].key, "Score");
          if (is_score && run->cur_score_ti >= 0)
            {
              if (st->task_scored[run->cur_score_ti])
                return;                       /* already scored -- suppress */
              st->task_scored[run->cur_score_ti] = 1;
            }
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
              /* FD's ExecuteTask(Look) is a full AttemptToExecuteTask(Look): its
                 Specific overrides run before the view, then the room view
                 (Look's AggregateOutput CompletionMessage), then Look's own
                 <Actions>.  Routing through execute_task_with_overrides fires
                 both -- e.g. Amazon's `Beforeplay1` -> `Execute ts_tasCheckTime`
                 (the "Date: ..." line: startup, re-looks and per-move) AND
                 Grandpa's `vnl_TutorialSt` chain off Look's actions.

                 The per-move duplicate is real: PlayerMovement's `Beforeplay`
                 override already ran ts_tasCheckTime, so its `Execute Look` ->
                 `Beforeplay1` emits the same "Date: ..." a second time.  FD's
                 per-turn response dedup (htblResponses keyed by message text,
                 clsUserSession.vb:783) collapses the two -- which run_general's
                 movement response map now reproduces (the two Date lines share
                 ts_tasCheckTime's AggregateOutput comp node and merge to one). */
              const a5_task_t *lt = a5model_task (st->adv, "Look");
              if (lt != NULL && depth < 16)
                {
                  std::vector<ref_info> noref;
                  execute_task_with_overrides (run, lt, noref, depth, out);
                }
              else
                emit_look (run, out);
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
                      /* FD's SetTasks handler (clsUserSession.vb:2188) passes a
                         parameter that names the target task's own reference
                         (`sParam = sRef`) STRAIGHT THROUGH -- the live
                         clsNewReference object, items and sCommandReference
                         intact.  So `Execute cl_TakeObject (%objects%)` keeps
                         each item's original typed text ("all" under a bare
                         `get all`), which is what lets the executed task's
                         `MustNot BeExactText All` restriction fail silently per
                         item (ThingsThatGoBumpInTheNight's take chain).  Mirror
                         that by leaving the existing binding (key AND $text)
                         untouched.  A computed argument (%ParentOf[..]%, a
                         literal key) instead builds FD's UserDefinedRef, whose
                         fresh items carry NO sCommandReference -- bind an empty
                         typed text, not the key. */
                      std::string an = args[i];
                      { size_t b1 = an.find_first_not_of (" \t");
                        size_t e1 = an.find_last_not_of (" \t");
                        an = (b1 == std::string::npos)
                               ? "" : an.substr (b1, e1 - b1 + 1); }
                      if (an.size () >= 2 && an.front () == '%' && an.back () == '%')
                        {
                          std::string base = an.substr (1, an.size () - 2);
                          if (base == "object")    base = "object1";
                          if (base == "character") base = "character1";
                          if (base == "direction") base = "direction1";
                          if (base == "number")    base = "number1";
                          if (base == "text")      base = "text1";
                          if (base == rnames[i])
                            continue;                     /* pass the ref through */
                        }
                      char *val = eval_arg_to_key (st, args[i]);
                      bind_reference (st, rnames[i].c_str (), val, "");
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
                {
                  /* FD's ExecuteTask is a full AttemptToExecuteTask: a
                     restriction failure DISPLAYS the failing restriction's
                     message (AttemptToExecuteSubTask, clsUserSession.vb:1246
                     `sMessage = sRestrictionText` -> AddResponse bPass=False,
                     deduped by text in htblResponsesFail).  a5restr_pass just
                     left that node in st->restriction_text (NULL for a
                     message-less failure, e.g. `MustNot BeExactText All` under
                     a bare `get all` -- those stay silent).  TBN's dark-room
                     `get dirt` needs the emission: cl_TakeCharac passes but
                     both Execute'd take tasks fail on the LightSources
                     restriction with "It is too dark to find the dirt.". */
                  const a5_xml_node_t *fm = st->restriction_text;
                  if (fm != NULL)
                    {
                      char *fmsg = a5text_describe (st, fm);
                      if (msg_has_output (fmsg))
                        {
                          if (run->resp != NULL)
                            resp_add_text (run, fmsg, false);
                          else if (run->exec_scope != NULL)
                            {
                              /* Buffer for the end-of-scope flush, where FD's
                                 pass-cancels-fail rule is applied (a pass for
                                 the same objects can arrive before OR after
                                 this fail); dedup by text (htblResponsesFail
                                 keying). */
                              if (run->exec_scope->fail_seen.insert (fmsg).second)
                                run->exec_scope->fails.push_back
                                  (std::make_pair (std::string (fmsg),
                                                   current_obj_ref_keys (st)));
                            }
                          else
                            { sb_pspace (out); sb_puts (out, fmsg); }
                        }
                      free (fmsg);
                    }
                  return;
                }
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
      /* Resolve the source key through act_key so a `%Player%` / `%objectN%`
         operand maps to its entity key (FD's ReferenceControl expansion).  A
         group key (EverywhereInGroup) is not an entity, so act_key returns it
         verbatim -- harmless.  Without this `LocationOf %Player%` looked up a
         character literally named "%Player%" (none), so SixSilverBullets'
         `RemoveLocationFromGroup LocationOf %Player% FromGroup TimeTraps` removed
         nothing and The Hotel kept tolling the bell every turn. */
      const char *k1  = act_key (st, tk[1].c_str ());
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

/* The object keys currently bound as this task-chain's %objects%/%object%
   reference -- the ReferencedObjects pipe list when present, else the singular
   ReferencedObject1.  Used for FD's pass-cancels-fail response rule (see
   exec_pass_refs in a5run_internal.h). */
static std::vector<std::string>
current_obj_ref_keys (a5_state_t *st)
{
  std::vector<std::string> v;
  const char *p = a5state_lookup_ref (st, "ReferencedObjects");
  if (p != NULL && p[0] != '\0')
    {
      std::string cur;
      for (const char *q = p; ; q++)
        {
          if (*q == '|' || *q == '\0')
            { if (!cur.empty ()) v.push_back (cur); cur.clear ();
              if (*q == '\0') break; }
          else cur += *q;
        }
      return v;
    }
  p = a5state_lookup_ref (st, "ReferencedObject1");
  if (p != NULL && p[0] != '\0')
    v.push_back (p);
  return v;
}

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
  if (run->immediate_emit)
    {
      /* Startup RunImmediately path: render in two stages so the markup-bearing
         form (`proc`, what FD's Display sees) drives the output test, while the
         plain form is emitted.  FD's bHasOutput keeps a message when stripping
         tags leaves any character -- so a title-music task's `<audio ...> ` keeps
         its trailing space to join onto the title.  a5text_describe ==
         eval_description -> process -> render_plain. */
      char *raw   = a5text_eval_description (run->st, comp);
      char *proc  = a5text_process (run->st, raw);
      char *plain = a5text_render_plain (proc);
      run->st->marking_display = prev_mark;
      if (fd_has_output (run->st, proc)) { sb_pspace (out); sb_puts (out, plain); }
      free (raw); free (proc); free (plain);
      return;
    }
  int pre_alr_ink = 0;
  char *m = a5text_describe_ex (run->st, comp, &pre_alr_ink);
  run->st->marking_display = prev_mark;
  /* Append exactly as FD Display() does: pSpace-join to the running output, then
     the rendered text verbatim.  A whitespace-only message has no output (FD's
     bHasOutput) and is dropped; a real message keeps its own trailing newline so
     it forces a line/paragraph break before the next message, while one ending
     in text space-joins to the next. */
  if (msg_has_output (m))
    {
      /* Within an event-fired task chain, dedup identical completion messages
         (FD's htblResponsesPass, keyed by text): show the first, drop repeats.
         Keeps the first occurrence's position -- which, for a message emitted
         before a later <cls>, means it is wiped by that clear (Pathway's banner). */
      if (run->ev_seen != NULL)
        {
          if (run->ev_seen->count (m)) { free (m); return; }
          run->ev_seen->insert (m);
        }
      /* Per-command pass-response text dedup (FD's htblResponsesPass keying):
         a command that reaches the same completion text twice -- e.g. two
         Execute'd tasks with identical messages (Euripides' `press on` runs
         cl_ToCrawler11 AND cl_ToCrawler12, both the crawler journey) -- shows
         it once, keeping the first occurrence's position. */
      if (run->exec_scope != NULL
          && !run->exec_scope->pass_seen.insert (m).second)
        { free (m); return; }
      /* A pass response with output: record its bound object references for
         the pass-cancels-fail rule (FD keys every AddResponse with the
         subtask's reference keys and drops a fail whose refs all passed). */
      if (run->exec_scope != NULL)
        for (auto &k : current_obj_ref_keys (run->st))
          run->exec_scope->pass_refs.insert (k);
      sb_pspace (out); sb_puts (out, m);
    }
  else if (pre_alr_ink && m != NULL && m[0] != '\0')
    {
      /* The message had real text until a game ALR blanked it (Tribute maps
         the auto-note "(from the enormous mirror)" to an empty TextOverride).
         FD stores the response pre-ALR and only blanks it inside Display(),
         so its markup/newline skeleton still pSpace-joins the output and
         leaves an empty paragraph slot.  Emit the whitespace remainder
         verbatim to keep that slot. */
      sb_pspace (out); sb_puts (out, m);
    }
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

  if (run->resp != NULL)
    {
      /* Defer the room view to flush (final command state): FD's stock Look
         CompletionMessage is AggregateOutput, so its render is deferred to
         Display.  Recording a look entry (rather than rendering now) lets a
         command whose After children move things have the view reflect them. */
      resp_entry e;
      e.is_pass = true; e.is_look = true;
      e.item = run->resp->cur_item;
      resp_insert (run->resp, e, -1);
      return;
    }
  std::string result = render_look_string (run);
  sb_pspace (out);
  sb_puts (out, result.c_str ());
}

void
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
      /* FD executes the stock Look like any Before + AggregateOutput task
         (clsUserSession.vb:1164-1207): the message is test-rendered BEFORE and
         AFTER its actions (bTestingOutput=True; each render re-draws any
         <# OneOf #>/RAND in the room view), and when the two renders differ (a
         random pick moved between them) the response is PINNED to the first
         render.  Only when they agree does the raw aggregate message survive to
         be re-rendered once more at final Display -- which is the defer_look /
         is_look deferral emit_look implements.  Doing both test renders here
         keeps the xoshiro draw stream aligned with FD for random room views
         (LostLabyrinth's riding OneOf: 2 draws, first one shown); for the usual
         static view the two renders draw nothing and agree, so this is
         draw- and output-neutral. */
      int pm = run->st->marking_display;
      run->st->marking_display = 0;
      std::string e1 = render_look_string (run);
      run->st->marking_display = pm;

      /* The response slot is reserved BEFORE the actions run (vb:1189
         iResponsePosition), so any output the actions produce follows the
         room view -- Grandpa's tutorial lines land after the view. */
      int resp_pos = -1;
      int claimed_pending = 0;
      sb_t abuf; sb_init (&abuf);
      if (run->resp != NULL)
        resp_pos = (int) run->resp->ents.size ();
      else if (run->defer_look && !run->look_pending)
        { run->look_pending = 1; run->look_pos = out->len; claimed_pending = 1; }

      if (self_ti >= 0) run->st->task_done[self_ti] = 1;
      int saved_sti = run->cur_score_ti;
      run->cur_score_ti = self_ti;
      if (t->actions != NULL)
        for (const a5_xml_node_t *c = t->actions->first_child; c; c = c->next)
          run_action (run, c->name, c->text, depth,
                      (run->resp == NULL && !run->defer_look) ? &abuf : out);
      run->cur_score_ti = saved_sti;

      pm = run->st->marking_display;
      run->st->marking_display = 0;
      std::string e2 = render_look_string (run);
      run->st->marking_display = pm;

      if (e1 != e2)
        {
          /* Pinned to the pre-actions render (vb:1200 sMessage = sBefore...). */
          if (run->resp != NULL)
            resp_add_text (run, e1.c_str (), true, resp_pos);
          else if (run->defer_look)
            {
              if (claimed_pending)
                { free (run->look_pinned);
                  run->look_pinned = strdup (e1.c_str ()); }
            }
          else if (run->exec_scope == NULL
                   || run->exec_scope->pass_seen.insert (e1).second)
            { sb_pspace (out); sb_puts (out, e1.c_str ()); }
        }
      else if (run->resp != NULL)
        {
          resp_entry e;
          e.is_pass = true; e.is_look = true;
          e.item = run->resp->cur_item;
          resp_insert (run->resp, e, resp_pos);
        }
      else if (!run->defer_look)
        {
          /* The per-command response dedup applies to the room view too: FD
             keys the Look task's raw AggregateOutput template, so a command
             that Executes Look twice shows one view (Euripides `press on`). */
          std::string v = render_look_string (run);
          if (run->exec_scope == NULL
              || run->exec_scope->pass_seen.insert (v).second)
            { sb_pspace (out); sb_puts (out, v.c_str ()); }
        }
      /* defer_look + unchanged: the pending slot claimed above is rendered at
         the enabling frame's final state, exactly as before. */

      if (abuf.len > 0) { sb_pspace (out); sb_puts (out, abuf.p); }
      free (abuf.p);
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
        {
          resp_pre = render_comp_test (run->st, comp);
          resp_pos = (int) run->resp->ents.size ();
          /* FD renders task.CompletionMessage.ToString with bTestingOutput
             False BEFORE the actions (clsUserSession.vb:1167), retiring any
             <DisplayOnce> segment even though the response text itself is
             pinned/re-rendered later.  Mirror the marking for a non-aggregate
             task with a segment-selection eval AFTER the pre-action test
             render (so resp_pre keeps the first-time segment) -- eval only,
             no %function% replacement, so no RNG draws.  An aggregate comp is
             re-rendered at flush, where Scarier's segment choice must stay
             live, so it keeps the old behaviour.  Euripides' crevice squeeze
             (a movement override with a DisplayOnce first paragraph) showed
             the first-time text on every later squeeze without this. */
          if (!t->aggregate)
            {
              int pm = run->st->marking_display;
              run->st->marking_display = 1;
              char *mark = a5text_eval_description (run->st, comp);
              run->st->marking_display = pm;
              free (mark);
            }
        }
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
      int saved_sti = run->cur_score_ti;
      run->cur_score_ti = self_ti;
      const a5_xml_node_t *c;
      for (c = act->first_child; c != NULL; c = c->next)
        run_action (run, c->name, c->text, depth, out);
      run->cur_score_ti = saved_sti;
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

/* The events + walks turn-based runtime (ev_* / wk_*) lives in
   a5run_events.cpp; its ev_init / ev_tick_all / ev_on_task_completed
   entry points are declared in a5run_internal.h. */
