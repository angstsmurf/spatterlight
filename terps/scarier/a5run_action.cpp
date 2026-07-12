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
  if (raw != NULL && strncmp (raw, "RAND", 4) == 0)
    {
      /* RAND (min, max): inclusive random in [min, max] via the shared
         erkyrath_random (the Adrift 5 runner Global.Random(iMin, iMax)).  The data
         writes it as "RAND (1, 4)" (spaces optional).  A bound may itself be a
         %variable% (LostCoastlines' item valuation writes `RAND (0, %valuator%)`);
         the runner resolves it via ReplaceFunctions before Global.Random, so substitute
         %tokens% first -- otherwise the bound parses as 0 and the whole
         age/decoration/aura draw collapses to a no-op RAND(0,0). */
      long lo = 0, hi = 0;
      char *end;
      char *sub = (strchr (raw, '%') != NULL)
                    ? a5text_process_noalr (st, raw) : NULL;
      const char *rr = sub ? sub : raw;
      const char *p = rr + 4;
      while (*p && !(*p == '-' || (*p >= '0' && *p <= '9'))) p++;
      lo = strtol (p, &end, 10);
      p = end;
      while (*p && !(*p == '-' || (*p >= '0' && *p <= '9'))) p++;
      hi = (*p) ? strtol (p, NULL, 10) : lo;
      free (sub);
      return a5rand_between (lo, hi);
    }
  /* NO ALR pass here: the runner evaluates action values via EvaluateExpression
     (ReplaceFunctions only); ReplaceALRs is Display-time.  GFS's display ALR
     "17000" -> "1.700.0" must not turn IncVariable cl_Money = "1700000"
     into 1. */
  proc = a5text_process_noalr (st, raw ? raw : "0");
  /* The runner evaluates a SetVariable value through EvaluateExpression
     (clsVariable) and reads the numeric result as VB Val(): full arithmetic
     (+ - * / ^ mod, with runner precedence and away-from-zero division) plus
     function calls -- min/max/if and the rand()/oneof() draws.  a5_eval_sexpr
     IS that evaluator; it supersedes the old integer-only a5arith fast path,
     whose ^ handled precedence/associativity differently from the runner
     (clsVariable reduces ^ on run 2, the +/- tier, left-to-right -- see
     FrankenDrift clsVariable.vb).  %vars% are already substituted, so Tingalan's
     Roller "rand(0,%playerwits%)" is now rand(0,N) and draws via the same
     a5rand_between as the uppercase RAND path.

     The value is Val() of the *result* -- its leading integer, else 0.  So a
     non-numeric value ("Object3") reads as 0, as does an out-of-range result
     the runner guards with SafeInt(Val(...)): a division by zero evaluates to
     "\xe2\x88\x9e" (Val 0, the former a5arith div/mod-by-zero guard), not the
     leading digit of the raw "5/0". */
  {
    char *sv = a5_eval_sexpr (proc);
    long e = strtol (sv, NULL, 10);      /* Val(): leading integer, else 0 */
    free (sv);
    free (proc);
    return e;
  }
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

static std::string
lower_copy (const std::string &s)
{
  std::string r = s;
  for (char &c : r) c = (char) tolower ((unsigned char) c);
  return r;
}

/* True if `v` is, after trimming, a single top-level call "name(...)" spanning
   the whole string -- not e.g. "f(a) & g(b)" or a plain literal.  `name` is
   set to the (unlowered) identifier. */
static bool
a5_bare_function_call (const std::string &v, std::string &name)
{
  size_t i = 0, n = v.size ();
  while (i < n && isspace ((unsigned char) v[i])) i++;
  size_t start = i;
  while (i < n && (isalpha ((unsigned char) v[i]) || v[i] == '_')) i++;
  size_t name_end = i;
  /* The runner's tokenizer strips redundant spaces before parsing, so "RAND (a, b)"
     (LostCoastlines writes its RAND assignments with a space) is the same call
     as "RAND(a,b)".  Skip any gap between the identifier and its '('. */
  while (i < n && isspace ((unsigned char) v[i])) i++;
  if (name_end == start || i >= n || v[i] != '(')
    return false;
  name = v.substr (start, name_end - start);
  size_t j = n;
  while (j > start && isspace ((unsigned char) v[j - 1])) j--;
  if (j == 0 || v[j - 1] != ')')
    return false;
  int depth = 0;
  for (size_t k = i; k < j; k++)
    {
      if (v[k] == '(') depth++;
      else if (v[k] == ')') { depth--; if (depth == 0 && k != j - 1) return false; }
    }
  return depth == 0;
}

static void run_action (a5_run_t *run, const char *kind, const char *body,
                        int depth, sb_t *out);
static void emit_look (a5_run_t *run, sb_t *out);
static std::string render_look_string (a5_run_t *run);
static void emit_completion (a5_run_t *run, const a5_xml_node_t *comp, sb_t *out);
static void emit_message_body (a5_run_t *run, char *m, int pre_alr_ink, sb_t *out);
static std::vector<std::string> current_obj_ref_keys (a5_state_t *st);

/* Render the room view as REAL output -- marking_display=1 so its <DisplayOnce>
   segments retire -- restoring the previous marking flag afterwards.  The common
   wrapper around render_look_string at every final-state Display site (and, via
   a5run_look, the frontend's post-UNDO room redisplay). */
std::string
render_look_marked (a5_run_t *run)
{
  int pm = run->st->marking_display;
  run->st->marking_display = 1;
  std::string v = render_look_string (run);
  run->st->marking_display = pm;
  return v;
}

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
     ReferencedObject (the runner's GetReference keys ReferencedObject to the "object1"
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

/* Normalise a bare singular reference name to its "...1" indexed form
   (FileIO.vb:647: %object% -> %object1%, etc.).  Plural "objects"/"characters"
   and already-indexed names are left unchanged. */
static void
normalize_singular_ref (std::string &base)
{
  if (base == "object")    base = "object1";
  if (base == "character") base = "character1";
  if (base == "direction") base = "direction1";
  if (base == "number")    base = "number1";
  if (base == "text")      base = "text1";
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
              std::string base (p + 1, (size_t) (q - (p + 1)));
              /* singular %object% normalises to object1 only for matching, but
                 the binding base keeps "objects" plural distinct. */
              normalize_singular_ref (base);
              v.push_back (base);
              p = q + 1;
              continue;
            }
        }
      p++;
    }
  return v;
}

/* clsUserSession.RefsMatchSpecifics / GetAlternateRef: a Specific child's
   Specifics are always defined in the order of the parent's FIRST command, but
   the input may have matched a later command whose references appear in a
   different order (SayToCharacter: `say %text% to %character%` vs `say to
   %character% %text%`).  Reorder the resolved refs into first-command order --
   a no-op when the first command matched -- so the 1:1 Specific matching in
   refs_match_specifics lines up.  When the matched command's reference names
   are not a permutation of the first command's (the runner's GetAlternateRef falls
   back to the unmapped index there), keep the matched order. */
static std::vector<ref_info>
collect_refs_ordered (a5_state_t *st, const a5_match_t *m, const a5_task_t *t)
{
  std::vector<ref_info> v = collect_refs (st, m);
  std::vector<std::string> first = command_refs (t);
  if ((int) first.size () != m->n_refs)
    return v;
  std::vector<ref_info> w;
  std::vector<char> used ((size_t) m->n_refs, 0);
  for (const std::string &nm : first)
    {
      int found = -1;
      for (int i = 0; i < m->n_refs; i++)
        if (!used[i] && nm == m->ref_name[i]) { found = i; break; }
      if (found < 0)
        return v;
      used[found] = 1;
      w.push_back (v[found]);
    }
  return w;
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

/* A bare `Group.<Property>` SetTasks-Execute argument is an OO reference that
   the Adrift 5 runner resolves (ReplaceOO -> ReplaceOOProperty, Global.vb:1597/930) to
   the group's ordered member LIST filtered to members that carry <Property> --
   e.g. `Objects1.StaticOrDynamic`, where the mandatory StateList property is on
   every object, yields all 125 member keys.  The runner packs those keys as the Items of
   a single reference so AttemptToExecuteTask -> ExecuteSubTasks runs the executed
   General task once per member, binding its reference to each key in turn (this
   is how the world-generation creation tasks value every object/secret/story).

   Fill OUT with the member keys in arlMembers order and return true when ARG is
   such a reference; return false (OUT untouched by the caller) otherwise so the
   argument falls back to the ordinary single-key resolution. */
static bool
group_prop_member_keys (a5_state_t *st, const std::string &arg,
                        std::vector<std::string> &out)
{
  out.clear ();
  size_t b = arg.find_first_not_of (" \t");
  if (b == std::string::npos) return false;
  size_t e = arg.find_last_not_of (" \t");
  std::string a = arg.substr (b, e - b + 1);
  /* A bare OO reference carries no %tokens% (those go through the text engine)
     and is a single-level `Group.Property`. */
  if (a.find ('%') != std::string::npos) return false;
  size_t dot = a.find ('.');
  if (dot == std::string::npos || dot == 0 || dot + 1 >= a.size ())
    return false;
  std::string grp = a.substr (0, dot);
  std::string prop = a.substr (dot + 1);
  if (prop.find ('.') != std::string::npos) return false;

  const a5_adventure_t *adv = st->adv;
  int gi;
  for (gi = 0; gi < adv->n_groups; gi++)
    if (streq (adv->groups[gi].key, grp.c_str ())) break;
  if (gi >= adv->n_groups) return false;

  const a5_propdef_t *pd = a5model_propdef (adv, prop.c_str ());
  if (pd == NULL) return false;
  /* A mandatory property (the runner's clsProperty.Mandatory) is added to every
     applicable item at load, so clsItem.HasProperty is always true for it --
     StaticOrDynamic is the canonical case.  Otherwise keep the members that
     HAVE the property (clsItem.HasProperty), presence not value -- a valueless
     SelectionOnly marker (AlienDiver's `BlankCards.ObjectIsAC` = the available
     blank cards) counts even though a5state_entity_prop returns NULL for it. */
  const char *mnd = pd->node ? a5xml_child_text (pd->node, "Mandatory") : NULL;
  int mandatory = (mnd != NULL && strcmp (mnd, "0") != 0);

  int n = a5state_group_count (st, grp.c_str ());
  for (int i = 0; i < n; i++)
    {
      const char *mk = a5state_group_member_at (st, grp.c_str (), i);
      if (mk == NULL) continue;
      if (mandatory || a5state_entity_has_prop (st, mk, prop.c_str ()))
        out.push_back (mk);
    }
  /* The runner still builds one (empty-keyed) Item for an empty resolution, so the
     executed task runs once with an unbound reference (its BeInGroup restriction
     then fails silently).  Preserve that single no-op run. */
  if (out.empty ())
    out.push_back ("");
  return true;
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
   the runner's `Keys.Count = 0 OrElse (Keys.Count = 1 AndAlso Keys(0) = "")`. */
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
          /* The runner compares the Specific key to the reference's matched possibility
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
   (it only re-constrains some of them) -- the runner (clsUserSession.vb:1060-1071) then
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

/* The Adrift 5 runner's clsUserSession collects every task completion / restriction-
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
   flush.  The Adrift 5 runner stores the message's NewReferences alongside it and
   reassigns them at Display (clsUserSession.vb:851-854), so the deferred
   "%object%.Description"/"(to %character%)"/"%direction%" tokens resolve against
   the entity the command referenced -- not whatever the binding table holds by
   the end of the command. */
struct ref_snap {
  char ref_name[16][32];
  char ref_value[16][A5_REFVAL_LEN];
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
   MERGES into an existing entry (obj_keys grows, ents.size() does NOT): the runner treats
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
   (the Adrift 5 runner's bTestingOutput=True): used for the pre/post-action comparison
   that decides whether a Before message is pinned or deferred. */
static char *
render_comp_test (a5_state_t *st, const a5_xml_node_t *comp)
{
  int pm = st->marking_display; st->marking_display = 0;
  char *m = a5text_describe (st, comp);
  st->marking_display = pm;
  return m;
}

/* Does this CompletionMessage bear a text-changing function -- an embedded
   <#...#> expression (OneOf/Either/...) or a RAND(..) inside a %function% or
   array reference?  Only such messages can draw RNG when rendered; run_task
   uses this to route a Before message through the runner-faithful
   render/actions/render interleave while plain static messages keep the flat
   fast path. */
static int
comp_bears_function (const a5_xml_node_t *n)
{
  if (n == NULL)
    return 0;
  if (n->text != NULL
      && (strstr (n->text, "<#") != NULL
          || strstr (n->text, "RAND(") != NULL))
    return 1;
  for (const a5_xml_node_t *c = n->first_child; c != NULL; c = c->next)
    if (comp_bears_function (c))
      return 1;
  return 0;
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

  /* The runner keys an AggregateOutput response by its RAW template -- AddResponse
     receives task.CompletionMessage.ToString UNexpanded (the OO chain and
     %functions% only resolve at Display).  The stock Look's response key is
     the literal "%Player%.Location.Description", so a movement task that both
     Executes Look AND carries that same completion (Murder Most Foul's custom
     PlayerMovement1) MERGES into the Look response: one room view, rendered
     once at Display -- which also means its DisplayOnce segments retire once
     and its ALR-borne %scor% reads that commit's state.  Mirror the merge:
     an aggregate single-reference completion whose raw template assembles to
     the same text as the pending is_look entry's (the Look task's raw
     CompletionMessage) is folded into it. */
  if (t != NULL && t->aggregate && item.empty ())
    for (auto &e : rm->ents)
      if (e.is_look && e.is_pass == is_pass)
        {
          const a5_task_t *look = a5model_task (st->adv, "Look");
          const a5_xml_node_t *lcomp =
              look != NULL ? a5xml_child (look->node, "CompletionMessage") : NULL;
          bool same = false;
          if (lcomp != NULL)
            {
              int pm = st->marking_display; st->marking_display = 0;
              char *rawc = a5text_eval_description (st, comp);
              char *rawl = a5text_eval_description (st, lcomp);
              st->marking_display = pm;
              same = rawc != NULL && rawl != NULL && streq (rawc, rawl);
              free (rawc); free (rawl);
            }
          if (same) { rm->nmut++; return; }   /* merge is output (sink_len) */
          break;
        }

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
      int raw_nonblank = 0, pre_alr_ink = 0;
      char *m = a5text_describe_ex (st, comp, &pre_alr_ink, &raw_nonblank);
      st->marking_display = pm;
      /* The runner's AddResponse output test (bHasOutput) sees the message BEFORE the
         trailing-whitespace trim, so a whitespace-only "\n" completion counts
         as task output -- it stops an After-children scan (The Salvage's
         per-move station-known task suppressing the fuel child) even though
         nothing visible renders. */
      if (raw_nonblank)
        run->task_raw_output = 1;
      bool has = msg_has_output (m);
      std::string r = m ? m : ""; free (m);
      /* A message whose plain render is ONLY stripped-tag sentinels but which
         had visible text before the ALR pass -- WW2 Elevator Escape's
         "(standing up first)" -> ALR -> "<del>", leaving
         "<font..><del><font..>" -- passes the runner's AddResponse gate (bHasOutput on
         the stored PRE-ALR text) and is Displayed as a markup skeleton, so
         the runner's raw buffer ends in '>' and the NEXT message pSpace-joins with two
         spaces.  Keep such an entry: the mark bytes survive to the sb, where
         sb_pspace treats them as a non-newline tail; finish_turn strips them.
         A marks-only render with NO pre-ALR ink (Amazon's ts_tasSunset "<>"
         off-hours completion) fails the runner's gate and is dropped as before. */
      if (!has && !(pre_alr_ink && !r.empty ())) return;
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
   fail response, mirroring the Adrift 5 runner's htblResponsesFail keyed by the raw
   message template (clsUserSession AddResponse, vb:1301-1307): a second item that
   fails the SAME restriction node merges its %objects% item into the existing
   entry instead of emitting a separate message, so at flush the template renders
   ONCE over the merged set.  AoS `put all in bag` re-fails the two nested coins on
   the general `Must BeHeldByCharacter %Player%`; The runner produces the single "You are
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

/* Rebind %objects%/ReferencedObjects to KEYS (skipping unknown keys, capped at
   A5_MAX_ITEMS).  Shared by the three resp_flush re-render sites that render a
   merged completion or fail over a plural %objects% set. */
static void
rebind_objects (a5_state_t *st, const std::vector<std::string> &keys)
{
  std::string pipe;
  st->n_ref_items = 0;
  st->ref_items_type = 'o';
  for (auto &k : keys)
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

  /* The runner sets NewReferences = refs only for the messages it actually Displays
     (htblResponses, which its AddResponse bHasOutput gate never adds an
     empty-output response to).  So the leftover reference the post-command event
     tasks iterate is the LAST *displayed* message's refs, NOT the last response
     entry's.  Track the last output-producing aggregate entry's refs here and
     re-apply them after the loop; an entry that renders to nothing (e.g. Book of
     Jax's `put all in bag` re-putting items already inside the bag -- their
     cl_PutObjInBa completions are empty) must not become the leftover set, or a
     per-turn TurnBased event (cl_TurnsTaken1) would tick once per silently-moved
     item instead of once. */
  std::vector<std::string> lo_keys;   /* last displayed aggregate's obj_keys   */
  std::string lo_obj2;
  int lo_have = 0;                    /* any entry produced output this flush   */
  int lo_multi = 0;                   /* last displayed entry was plural (>1)   */

  for (int phase = 0; phase < 2; phase++)
    for (auto &e : rm->ents)
      {
        if (e.is_pass != (phase == 0)) continue;
        /* The runner drops a fail whose every reference item also produced a pass
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
               final state, mirroring the Adrift 5 runner's AggregateOutput Look whose
               function replacement is deferred to Display.  So a move whose After
               children relocated an NPC lists it at its new spot.  Real output:
               retire DisplayOnce segments (view_location_impl honours the
               ambient marking flag). */
            text = render_look_marked (run);
            /* The runner keys the stock Look's AggregateOutput on its response template
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
               did when the task ran (the runner reassigns NewReferences at Display). */
            if (e.has_snap) ref_snap_restore (st, &e.snap);
            /* A genuine plural %objects% command merged several items into this
               entry: rebind the whole set (overriding the snapshot's singular
               object binding) so the completion renders its "a, b and c" list.
               A single-reference command has no merged items (obj_keys empty) and
               keeps the restored snapshot's singular binding untouched. */
            if (!e.obj_keys.empty ())
              {
                rebind_objects (st, e.obj_keys);
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
               failing item (the runner Display over the accumulated htblResponsesFail
               NewReferences).  The aggregate string is what a game ALR can match
               at the display boundary -- AoS "You are not carrying the gold gonks
               and the silver ginks." blanked by cl_YouAreNotC1.  A single-item
               fail (obj_keys.size()==1) keeps its eager render below, byte-exact. */
            if (e.has_snap) ref_snap_restore (st, &e.snap);
            rebind_objects (st, e.obj_keys);
            char *m = a5text_describe (st, e.fail_comp);
            if (m != NULL) text = m;
            free (m);
          }
        else
          text = e.rendered;

        /* Marks-only entries (see resp_add_comp) are emitted too: the runner Displayed
           the markup skeleton, so it space-joins here and leaves the buffer
           mid-line for the next response's pSpace. */
        if (msg_has_output (text.c_str ()) || !text.empty ())
          {
            sb_pspace (out); sb_puts (out, text.c_str ());
            /* This message is Displayed, so it is the running NewReferences (the runner
               reassigns per Displayed message).  An aggregate carrying a merged
               %objects% set makes the leftover plural; any other displayed message
               (a single-item completion, a movement line, a room view) leaves it
               singular -- the event tasks then run once. */
            lo_have = 1;
            if (e.aggregate && e.obj_keys.size () > 1)
              { lo_keys = e.obj_keys; lo_obj2 = e.obj2; lo_multi = 1; }
            else
              { lo_keys.clear (); lo_obj2.clear (); lo_multi = 0; }
          }
      }

  /* Re-apply the last Displayed message's references as the runner's post-Display
     NewReferences (see the note above the loop).  Only a plural leftover matters
     to attempt_event_task's per-item iteration; a singular last message collapses
     n_ref_items so the event runs once. */
  if (lo_have)
    {
      if (lo_multi)
        {
          rebind_objects (st, lo_keys);
          if (!lo_obj2.empty ())
            bind_reference (st, "object2", lo_obj2.c_str (), lo_obj2.c_str ());
        }
      else if (st->n_ref_items > 1)
        st->n_ref_items = 1;
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

/* clsTask.Children sorts a parent's Specific overrides with
   List(Of String).Sort(TaskPrioritySortComparer) (clsTask.vb:328-345) -- .NET's
   UNSTABLE introspective sort, keyed on Priority alone.  When two passing
   overrides tie on priority the one that runs is decided by where introsort
   happens to leave them, so being faithful means porting the algorithm
   byte-for-byte (.NET ArraySortHelper<T>.IntrospectiveSort with a comparer,
   IntrosortSizeThreshold 16; dotnet/runtime ArraySortHelper.cs).  E.g. Fortress
   of Fear has two 'talk to baker' Overrides of Task66 both at priority 1211 --
   Task58 (2023 revision: +3 score, "baker,") and Task1803 (2015: no score,
   "baker, ") -- and the runner runs Task1803 because introsort orders the tie
   [Task2536, Task1803, Task58].  A stable sort keeps XML order and runs Task58:
   wrong text AND a +3 score drift.  The sort input is the parent's children in
   XML load order (TaskHashTable = Dictionary preserves insertion order), with
   completed non-repeatable tasks filtered out BEFORE sorting (the Children
   property's bIncludeCompleted=False filter) -- the filter changes the array
   introsort sees, so it must precede the sort exactly as in the runner. */

static int
net_cmp_pri (const a5_adventure_t *adv, int a, int b)
{
  long pa = adv->tasks[a].priority, pb = adv->tasks[b].priority;
  return pa < pb ? -1 : pa > pb ? 1 : 0;
}

static void
net_swap_if_greater (const a5_adventure_t *adv, int *k, int i, int j)
{
  if (net_cmp_pri (adv, k[i], k[j]) > 0)
    { int t = k[i]; k[i] = k[j]; k[j] = t; }
}

static void
net_insertion_sort (const a5_adventure_t *adv, int *k, int n)
{
  for (int i = 0; i < n - 1; i++)
    {
      int t = k[i + 1];
      int j = i;
      while (j >= 0 && net_cmp_pri (adv, t, k[j]) < 0)
        { k[j + 1] = k[j]; j--; }
      k[j + 1] = t;
    }
}

static void
net_down_heap (const a5_adventure_t *adv, int *k, int i, int n)
{
  int d = k[i - 1];
  while (i <= n / 2)
    {
      int child = 2 * i;
      if (child < n && net_cmp_pri (adv, k[child - 1], k[child]) < 0)
        child++;
      if (!(net_cmp_pri (adv, d, k[child - 1]) < 0))
        break;
      k[i - 1] = k[child - 1];
      i = child;
    }
  k[i - 1] = d;
}

static void
net_heap_sort (const a5_adventure_t *adv, int *k, int n)
{
  for (int i = n / 2; i >= 1; i--)
    net_down_heap (adv, k, i, n);
  for (int i = n; i > 1; i--)
    {
      int t = k[0]; k[0] = k[i - 1]; k[i - 1] = t;
      net_down_heap (adv, k, 1, i - 1);
    }
}

static int
net_pick_pivot_and_partition (const a5_adventure_t *adv, int *k, int n)
{
  int hi = n - 1;
  int middle = hi >> 1;
  net_swap_if_greater (adv, k, 0, middle);
  net_swap_if_greater (adv, k, 0, hi);
  net_swap_if_greater (adv, k, middle, hi);
  int pivot = k[middle];
  { int t = k[middle]; k[middle] = k[hi - 1]; k[hi - 1] = t; }
  int left = 0, right = hi - 1;
  while (left < right)
    {
      while (net_cmp_pri (adv, k[++left], pivot) < 0) ;
      while (net_cmp_pri (adv, pivot, k[--right]) < 0) ;
      if (left >= right)
        break;
      { int t = k[left]; k[left] = k[right]; k[right] = t; }
    }
  if (left != hi - 1)
    { int t = k[left]; k[left] = k[hi - 1]; k[hi - 1] = t; }
  return left;
}

static void
net_intro_sort (const a5_adventure_t *adv, int *k, int n, int depth_limit)
{
  int partition_size = n;
  while (partition_size > 1)
    {
      if (partition_size <= 16)
        {
          if (partition_size == 2)
            { net_swap_if_greater (adv, k, 0, 1); return; }
          if (partition_size == 3)
            {
              net_swap_if_greater (adv, k, 0, 1);
              net_swap_if_greater (adv, k, 0, 2);
              net_swap_if_greater (adv, k, 1, 2);
              return;
            }
          net_insertion_sort (adv, k, partition_size);
          return;
        }
      if (depth_limit == 0)
        { net_heap_sort (adv, k, partition_size); return; }
      depth_limit--;
      int p = net_pick_pivot_and_partition (adv, k, partition_size);
      net_intro_sort (adv, k + p + 1, partition_size - (p + 1), depth_limit);
      partition_size = p;
    }
}

static void
net_introspective_sort (const a5_adventure_t *adv, std::vector<int> &keys)
{
  int n = (int) keys.size ();
  if (n <= 1)
    return;
  int log2n = 0;
  for (unsigned u = (unsigned) n; u > 1; u >>= 1)
    log2n++;
  net_intro_sort (adv, &keys[0], n, 2 * (log2n + 1));
}

/* Run the After-override children into `buf` in priority order, stopping after
   the first that produces output (unless ContinueAlways) -- the runner's bContinue
   rule.  Shared by the look-deferred and defer-text branches of
   execute_task_with_overrides, which each run the children into a side buffer. */
static void
run_after_children (a5_run_t *run, const std::vector<const a5_task_t *> &after,
                    sb_t *buf)
{
  a5_state_t *st = run->st;
  for (auto *child : after)
    if (a5restr_pass (st, child->restrictions))
      {
        size_t alen0 = buf->len;
        run->task_raw_output = 0;
        run_task (run, child, 0, buf);
        int ai = a5state_task_index (st, child->key);
        if (ai >= 0) st->task_done[ai] = 1;
        ev_on_task_completed (run, child->key, buf);
        if ((buf->len > alen0 || run->task_raw_output)
            && !child->continue_lower) break;
      }
}

/*
 * Run PARENT honouring its Specific child overrides (clsTask Children +
 * SpecificOverrideType), recursively -- a faithful port of the runner's
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
  if (getenv ("A5_TRACE_TASK"))
    fprintf (stderr, "TASK %-14s @draw %ld depth=%d\n",
             parent->key ? parent->key : "?", a5rand_draw_count, depth);
  int parent_text = 1, parent_actions = 1;
  int any_child_fail_output = 0;    /* The runner's bAnyChildHasOutput (vb:1053/1141) */
  std::vector<const a5_task_t *> after;
  size_t frame_len0 = sink_len (run, out);

  /* A command-bearing General has override children; a Specific override
     (depth>0) is itself scanned for its OWN nested children -- e.g. GetAWooden
     overrides TakeObjectFromLocation, which overrides TakeObjects.  Build the
     child list the way clsTask.Children does: the parent's Specific tasks in
     XML load order, completed non-repeatable ones filtered out (the property's
     bIncludeCompleted=False gate -- the runner's vb:730 entry check never sees them),
     then .NET-introsorted by Priority so equal-priority ties land exactly
     where the runner's unstable sort puts them (see net_introspective_sort above). */
  std::vector<int> kids;
  if ((parent->n_commands > 0 || depth > 0) && depth < 16)
    {
      for (int i = 0; i < run->adv->n_tasks; i++)
        {
          const a5_task_t *t = &run->adv->tasks[i];
          if (!streq (t->type, "Specific")) continue;
          if (t->general_key == NULL || !streq (t->general_key, parent->key))
            continue;
          if (!t->repeatable && st->task_done[i]) continue;
          kids.push_back (i);
        }
      net_introspective_sort (run->adv, kids);
    }
  for (size_t oi = 0; oi < kids.size (); oi++)
      {
        int cidx = kids[oi];
        const a5_task_t *child = &run->adv->tasks[cidx];
        if (!refs_match_specifics (st, child, parent, refs)) continue;

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
            /* The runner's bPassingOnly abort: once an earlier sibling child has failed
               WITH output (bAnyChildHasOutput, set only in the fail branch,
               vb:1141), every later child is attempted with bPassingOnly=True.
               A refs-matching child that then FAILS its restrictions is silent
               (the fail-message block is gated `If Not bPassingOnly`, vb:1244)
               and AttemptToExecuteTask returns early ("If bPassingOnly AndAlso
               Not bPass Then Return False", before the vb:911 bContinue rules)
               -- so bContinueExecutingTasks stays False and the child scan
               stops dead (vb:1146 Exit For).  The parent keeps whatever
               suppression earlier children set; later siblings (even a passing
               one) never run.  FoF `ask clockmaker about dials` after the
               clock explanation: Task2932 (7480) fails with "I have already
               explained...", then Task2949 (7495) matches and fails silently,
               aborting the scan BEFORE the wearily catch-all Task2950 (7496,
               +2 score) is reached -- the runner shows only Task2932's message. */
            if (any_child_fail_output)
              break;
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
            int fail_claims = 0;
            if (fm != NULL)
              {
                if (run->resp != NULL)
                  { resp_add_fail (run, fm); any_child_fail_output = 1; }
                else
                  {
                    char *fmsg = a5text_describe (st, fm);
                    /* The runner buffers this restriction text in htblResponsesFail and
                       flushes it after the passes; a later sibling override that
                       PASSES with output for the same reference cancels it
                       (clsUserSession.vb:804-834).  Buffer it in the command's
                       exec scope instead of writing to `out`, so AoK's `show
                       talisman` drops Task236's "Right idea - wrong location!"
                       once Task1427 passes.  When nothing passes, the scope
                       flush at the end of run_general still shows it. */
                    if (msg_has_output (fmsg))
                      {
                        fail_claims = 1;
                        any_child_fail_output = 1;
                        if (run->exec_scope != NULL)
                          {
                            if (run->exec_scope->fail_seen.insert (fmsg).second)
                              {
                                std::vector<std::string> pos;
                                for (auto &ri : refs)
                                  pos.push_back (ri.key ? ri.key : "");
                                run->exec_scope->ov_fails.push_back
                                  (std::make_pair (std::string (fmsg), pos));
                              }
                          }
                        else
                          { sb_pspace (out); sb_puts (out, fmsg); }
                      }
                    free (fmsg);
                  }
                if (!lazy) { parent_text = 0; parent_actions = 0; }
              }
            if (!lazy && (sink_len (run, out) > len0 || fail_claims)
                && !child->continue_lower && !st->adv->hp_passing)
              break;                        /* fail with output: claims the turn */
            continue;                       /* no output (or HPPT): keep scanning */
          }

        /* Recurse so the child's own Specific overrides fire in its place
           (AttemptToExecuteSubTask -> recursive AttemptToExecuteTask). */
        size_t ovf0 = run->exec_scope != NULL
                      ? run->exec_scope->ov_fails.size () : 0;
        execute_task_with_overrides (run, child, refs, depth + 1, out);
        st->task_done[cidx] = 1;        /* clsUserSession.vb:1193 task.Completed = True */
        /* A completing override child fires its own EventControls/WalkControls,
           exactly like the parent (the runner's AttemptToExecuteSubTask -> recursive
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
           the runner (clsUserSession.vb:1106/1111): parent ACTIONS run for
           BeforeActionsOnly|BeforeTextAndActions; parent TEXT is output for
           BeforeTextOnly|BeforeTextAndActions.  So BeforeActionsOnly keeps the
           parent's actions but suppresses its completion message (Grandpa's
           `vnl_OpenGate1` "safe enough to open" override: the gate still opens,
           but the generic "You open the gate." is not shown). */

        /* Stop only when this passing child produced output (and isn't
           ContinueAlways); otherwise fall through to the next matching child.
           A nested override that failed WITH a buffered message (ov_fails grew
           during the recursion) counts as output too: the runner's bContinue rule sees
           the htblResponsesFail entry, so the sibling scan stops and a later
           sibling's own restriction fail is never shown.  Temperamentum `get
           mirror`: TakeObjectFromLocation's GetALargeM buffers "You can't get
           at the mirror while the little girl is standing there." -- without
           this stop the scan fell through to TakeObjectsFromOthers, appending
           its "The large mirror is not on or inside another object!".  Gated
           on !hp_passing like the direct-fail claim above. */
        int subtree_fail_output = run->exec_scope != NULL
                                  && run->exec_scope->ov_fails.size () > ovf0;
        if ((sink_len (run, out) > len0
             || (subtree_fail_output && !st->adv->hp_passing))
            && !child->continue_lower)
          break;
      }

  /* The Adrift 5 runner defers an AggregateOutput task's completion-message function
     replacement to final Display (clsUserSession.vb:1184/1210 replace eagerly
     only when NOT AggregateOutput).  So an aggregate parent's text reflects
     state changed by its AfterTextAndActions / AfterActionsOnly children, which
     run before Display.  Mirror that: when an aggregate parent with no actions
     of its own has After-children, run the children first, then render the
     parent text -- keeping the parent text ahead of the children's output.
     Stone of Wisdom's `x window` needs this: ExamineAnO (AfterTextAndActions)
     increments Windowcoun, and ExamineObjects' %object%.Description segments are
     gated on it, so the "movement"/"creatures" lines must reflect the post-
     increment count on the same turn the runner shows them. */
  int defer_text = run->resp == NULL && parent_text && parent->aggregate
                   && parent->actions == NULL && !after.empty ();

  /* If the parent has After children that run actions on the direct path, defer
     its room view (Execute Look) so it renders at the post-child state, mirroring
     the runner's AggregateOutput Look deferral (see emit_look). */
  int prev_defer_look = run->defer_look;
  int prev_look_pending = run->look_pending;
  size_t prev_look_pos = run->look_pos;
  char *prev_look_pinned = run->look_pinned;
  int enable_look_defer = run->resp == NULL && !defer_text && !after.empty ();
  if (enable_look_defer)
    { run->defer_look = 1; run->look_pending = 0; run->look_pinned = NULL; }

  /* The runner's per-item AttemptToExecuteTask evaluates the task's restrictions at
     EXECUTION time (ExecuteSubTasks leaf), so an earlier item's actions can
     fail a later item of the same plural command -- Magnetic Moon's `put all
     in box`: once the (worn-then-held) backpack has been moved inside the
     box, the grapnel/cable/glue/camera nested in it are no longer
     BeHeldByCharacter and the runner refuses them ("You are not carrying ...!",
     items merged into one fail response).  Scarier's match phase refined the
     item set against the PRE-command state, so re-check the parent's
     restrictions here, on the plural per-item path only (cur_item set) --
     the singular/movement paths keep their single match-time evaluation and
     draw stream. */
  if ((parent_text || parent_actions) && depth == 0 && run->resp != NULL
      && !run->resp->cur_item.empty ()
      && !a5restr_pass (st, parent->restrictions))
    {
      const a5_xml_node_t *fm = st->restriction_text;
      if (fm != NULL)
        resp_add_fail (run, fm);
      parent_text = parent_actions = 0;
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
      run_after_children (run, after, &after_buf);
      /* A pinned view (the Look dance's two test renders differed -- a random
         pick moved) is emitted verbatim; otherwise render at the final state
         (real output, so <DisplayOnce> segments retire). */
      std::string view;
      if (pinned_view != NULL)
        view = std::string (pinned_view);
      else
        view = render_look_marked (run);
      /* The runner's response map holds ONE slot for the stock Look's raw template:
         a second Execute Look in the same command (The Salvage's request
         docking Looks at the docked pad, then on the bridge) re-renders that
         slot rather than appending -- first slot's position, last text. */
      exec_resp_scope *sc = run->exec_scope;
      if (sc != NULL && sc->look_off >= 0 && sc->look_sink == out
          && (size_t) sc->look_off + sc->look_len <= out->len)
        {
          sb_splice (out, (size_t) sc->look_off, sc->look_len, view.c_str ());
          sc->look_len = view.size ();
        }
      else
        {
          sb_pspace (out);
          if (sc != NULL)
            { sc->look_sink = out; sc->look_off = (long) out->len;
              sc->look_len = view.size (); }
          sb_puts (out, view.c_str ());
        }
      if (after_buf.len > 0) { sb_pspace (out); sb_puts (out, after_buf.p); }
      free (after_buf.p);
    }
  else if (defer_text)
    {
      /* Children run into a side buffer so the parent text can be spliced ahead
         of them once their actions have updated state. */
      sb_t after_buf; sb_init (&after_buf);
      run_after_children (run, after, &after_buf);
      int self_ti = a5state_task_index (st, parent->key);
      if (self_ti >= 0) st->task_done[self_ti] = 1;
      const a5_xml_node_t *comp = a5xml_child (parent->node, "CompletionMessage");
      /* The stock aggregate Look on the direct path: the runner's response map holds
         ONE slot keyed on its raw template, rendered at final Display -- after
         the LocationTrigger drain and event tick.  So (1) a command's SECOND
         Execute Look contributes nothing (one view, first slot's position,
         final state's text -- The Salvage's request docking Looks at the
         docked pad, then again on the bridge), and (2) the view reflects
         drain-time moves (McKinney relocating to the Airlock during `board
         avadora`).  Emit a display-defer sentinel; a5run_flush_display_defers
         renders the view into it at Display. */
      exec_resp_scope *sc = run->exec_scope;
      if (streq (parent->key, "Look") && parent->aggregate && sc != NULL
          && run->display_defers != NULL)
        {
          if (!(sc->look_off >= 0 && sc->look_sink == out))
            {
              sb_pspace (out);
              sc->look_sink = out;
              sc->look_off = (long) out->len;
              char mark[24];
              snprintf (mark, sizeof mark, "\004%d\004",
                        (int) run->display_defers->size ());
              sb_puts (out, mark);
              sc->look_len = strlen (mark);
              run->display_defers->push_back ("\002");
            }
        }
      else if (comp != NULL)
        emit_completion (run, comp, out);
      if (after_buf.len > 0) { sb_pspace (out); sb_puts (out, after_buf.p); }
      free (after_buf.p);
    }
  else
    for (auto *child : after)
      if (a5restr_pass (st, child->restrictions))
        {
          /* The runner's bContinue rule (AttemptToExecuteSubTask): stop the child scan
             once a passing child produces output, unless it is ContinueAlways.
             Two After overrides of the same general+spec that both pass (Galen's
             Quest `put yellow gem in ceiling receptacle` matches PutAYellow AND
             PutYellowG1, both +2) must not both fire -- only the higher-priority
             PutAYellow does, so the score tracks the runner (40, not 42). */
          size_t olen0 = sink_len (run, out);
          run->task_raw_output = 0;
          run_task (run, child, 0, out);
          int ai = a5state_task_index (st, child->key);
          if (ai >= 0) st->task_done[ai] = 1;   /* task.Completed = True */
          ev_on_task_completed (run, child->key, out);   /* fire its controls too */
          if ((sink_len (run, out) > olen0 || run->task_raw_output)
              && !child->continue_lower) break;
        }
      else
        {
          /* An After child is evaluated AFTER the parent's actions ran, so its
             restrictions see the post-action state, and a failure shows its
             restriction message like any child fail (vb:1244; the parent's own
             output has already been Displayed, so no cancel applies).  Head
             Case's `open rear doors`: the AfterTextAndActions Daza8Task24
             ("MustNot BeInState Open" -> "They are already open.") always
             fails against the freshly-opened doors, and the runner prints "You open
             the van rear doors.  They are already open." */
          const a5_xml_node_t *fm =
            a5restr_fail_message (st, child->restrictions);
          if (fm != NULL)
            {
              char *fmsg = a5text_describe (st, fm);
              if (msg_has_output (fmsg))
                {
                  sb_pspace (out);
                  sb_puts (out, fmsg);
                  free (fmsg);
                  break;               /* fail with output claims the scan */
                }
              free (fmsg);
            }
        }
  free (pinned_view);

  /* This frame produced pass output on the direct path: record its POSITIONAL
     reference signature (the runner keys every htblResponsesPass entry with the
     subtask's NewReferences) for the exec_scope ov_fails cancel rule. */
  if (run->resp == NULL && run->exec_scope != NULL
      && sink_len (run, out) > frame_len0)
    {
      std::vector<std::string> sig;
      for (auto &ri : refs)
        sig.push_back (ri.key ? ri.key : "");
      run->exec_scope->pass_sigs.push_back (sig);
    }
}

/* Run a matched General task (the directly command-matched parent), honouring
   its Specific child overrides. */
void
run_general (a5_run_t *run, const a5_task_t *parent, const a5_match_t *m,
             sb_t *out)
{
  a5_state_t *st = run->st;

  /* A genuine plural %objects% command (resolve_plural bound ref_items this
     turn) is run one item at a time, the-Adrift-5-runner-style (ExecuteSubTasks):
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

  /* A command runs under a per-command response map (the Adrift 5 runner's
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
     discarding the per-item override fail messages -- the runner's AttemptToExecuteTask
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

  /* Per-command response scope for SetTasks-Execute'd tasks (the runner's
     htblResponsesPass/Fail, live for the whole AttemptToExecuteTask): buffers
     Execute'd-task restriction-fail messages and cancels the ones whose
     objects also produced a pass response, at the flush below. */
  exec_resp_scope escope;
  exec_resp_scope *prev_escope = run->exec_scope;
  run->exec_scope = &escope;

  /* Deferred-completion sink for this command: an AggregateOutput completion's
     random draw (a `<#..#>` expression or a `%var[rand]%` index) is held in the
     run-level display_defers sink and drawn at end-of-turn Display
     (a5run_flush_display_defers), after the command's task, the LocationTrigger
     drain and the event tick -- matching the runner's Display-time ReplaceExpressions.
     Only armed on the single-reference (resp==NULL) path; the plural/movement map
     re-renders aggregate completions at its own flush. */
  std::vector<std::string> *prev_defers = run->comp_defers;
  run->comp_defers = use_map ? prev_defers : run->display_defers;

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
          std::vector<ref_info> refs = collect_refs_ordered (st, m, parent);
          execute_task_with_overrides (run, parent, refs, 0, out);
        }
    }
  else
    {
      std::vector<ref_info> refs = collect_refs_ordered (st, m, parent);
      execute_task_with_overrides (run, parent, refs, 0, out);
    }

  run->exec_scope = prev_escope;
  if (use_map)
    {
      run->resp = NULL;
      /* No child override passed for an "all" command: replace the buffered
         per-item fail messages with the general task's FailOverride (the runner's
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

  /* Deferred completion draws accumulated in run->display_defers are NOT flushed
     here: the runner holds them to the final Display loop, after the LocationTrigger
     drain and event tick.  a5run_flush_display_defers (called from finish_turn)
     draws + splices them then. */
  run->comp_defers = prev_defers;
}

/* End-of-turn Display flush (the runner's ReplaceALRs->ReplaceExpressions loop): draw
   each deferred AggregateOutput-completion body in emit order (== the runner htblResponses
   order) and splice the value into its `\004<idx>\004` sentinel slot in `out`.
   A body tagged with a leading \001 is a raw `%var[rand()]%` token re-resolved
   through the text pipeline (drawing its index); an untagged body is a frozen
   `<#..#>` sexpr reduced by a5_eval_sexpr (drawing its OneOf/Rand). */
void
a5run_flush_display_defers (a5_run_t *run, sb_t *out)
{
  std::vector<std::string> *sink = run->display_defers;
  if (sink == NULL || sink->empty ())
    return;
  void *prev_ed = run->st->expr_defer;
  run->st->expr_defer = NULL;                 /* no re-deferral during the draw */
  for (size_t k = 0; k < sink->size (); k++)
    {
      const std::string &body = (*sink)[k];
      char *val;
      if (body == "\002")
        {
          /* Deferred stock-Look room view: render at Display state (real
             output -- DisplayOnce segments retire). */
          std::string v = render_look_marked (run);
          val = strdup (v.c_str ());
        }
      else
        val = (!body.empty () && body[0] == '\001')
                ? a5text_process_noalr (run->st, body.c_str () + 1)
                : a5_eval_sexpr (body.c_str ());
      char mark[24];
      int ml = snprintf (mark, sizeof mark, "\004%d\004", (int) k);
      char *at = out->p != NULL ? strstr (out->p, mark) : NULL;
      if (at != NULL)
        {
          size_t pos = (size_t) (at - out->p);
          std::string tail (out->p + pos + ml, out->len - pos - (size_t) ml);
          out->len = pos;
          out->p[pos] = '\0';
          sb_puts (out, val ? val : "");
          sb_puts (out, tail.c_str ());
        }
      free (val);
    }
  run->st->expr_defer = prev_ed;
  sink->clear ();
}

/* Flush a SetTasks-Execute response scope: emit each buffered fail message
   (in order) unless every object it was evaluated against also produced a
   pass response -- the runner's flush loop, clsUserSession.vb:804-849, where a fail
   whose reference items all match a pass's is dropped and the survivors are
   Display'd after the pass responses. */
void
exec_scope_flush (a5_run_t *run, exec_resp_scope *sc, sb_t *out)
{
  (void) run;
  for (auto &f : sc->fails)
    {
      bool cancelled;
      if (f.second.empty ())
        /* No bound references: the runner's pass-cancels-fail loop (clsUserSession.vb:804)
           leaves bAllMatch True against the first pass message, because its per-ref
           check `For iRef = 0 To refsFail.Length-1` iterates 0 To -1 and never runs.
           So a ref-less Execute'd-task fail is cancelled by the mere presence of ANY
           pass response this command -- yet shown when there were none.  Tingalan's
           Search task passes with "You find nothing here", which cancels the
           Execute'd encounter's ref-less "...something follows..." restriction fail;
           the runner stays silent where Scarier used to leak the extra paragraph. */
        cancelled = !sc->pass_seen.empty ();
      else
        {
          cancelled = true;
          for (auto &k : f.second)
            if (!sc->pass_refs.count (k)) { cancelled = false; break; }
        }
      if (!cancelled)
        { sb_pspace (out); sb_puts (out, f.first.c_str ()); }
    }
  sc->fails.clear ();

  /* Specific-override fails buffered on the direct path: the runner's positional
     cancel (clsUserSession.vb:812-834) -- the fail is dropped only when some
     pass response's reference vector matches it position-for-position over the
     fail's FULL length (a pass with fewer refs cannot cancel: refsPass.Length
     <= iRef => bAllMatch False).  A ref-less fail keeps the bAllMatch-True
     quirk: cancelled by the mere presence of any pass response. */
  for (auto &f : sc->ov_fails)
    {
      bool cancelled;
      if (f.second.empty ())
        cancelled = !sc->pass_seen.empty () || !sc->pass_sigs.empty ();
      else
        {
          cancelled = false;
          for (auto &sig : sc->pass_sigs)
            {
              bool all = true;
              for (size_t i = 0; i < f.second.size (); i++)
                if (f.second[i].empty () || i >= sig.size ()
                    || sig[i] != f.second[i])
                  { all = false; break; }
              if (all) { cancelled = true; break; }
            }
        }
      if (!cancelled)
        { sb_pspace (out); sb_puts (out, f.first.c_str ()); }
    }
  sc->ov_fails.clear ();
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
      /* Resolve through the on/in-object chain, not char_loc alone -- a
         character whose model start is "On Object" (GFS's Grandpa on his
         rocking chair) has char_loc == NULL and is located via the
         furniture. */
      if (ploc != NULL && a5state_character_at_location (st, i, ploc))
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

/* Mark the destination location, its objects and its visible characters as seen
   the MOMENT the player arrives (clsCharacter.Move, clsCharacter.vb:524-533):
   the runner updates HasSeenObject/-Character/-Location at move time -- BEFORE the room
   render and BEFORE the movement's AfterTextAndActions overrides run -- so a
   same-turn `Must HaveBeenSeenByCharacter %Player%` restriction driven by such an
   override already sees the arrival.  (update_seen alone only refreshes at turn
   boundaries, which lags a discovery that a movement-override task tests in the
   same turn -- e.g. Alien Diver's ResetRollC -> CheckIfPla1 setting Shipfound as
   soon as the player walks onto the crashed ship.)  Player-centric, like the rest
   of our obj_seen/char_seen sets; only marks on an AtLocation arrival, matching
   the runner's `ToWhere.ExistWhere = AtLocation` gate. */
void
mark_player_arrival_seen (a5_state_t *st, const char *loc)
{
  int i;
  a5state_mark_loc_seen (st, loc);
  if (loc == NULL)
    return;
  if (st->obj_seen != NULL)
    for (i = 0; i < st->adv->n_objects; i++)
      if (a5state_object_visible_at_location (st, i, loc, 0))
        st->obj_seen[i] = 1;
  if (st->char_seen != NULL)
    for (i = 0; i < st->adv->n_characters; i++)
      if (!streq (st->adv->characters[i].key, a5state_player_key (st))
          && a5state_character_at_location (st, i, loc))
        st->char_seen[i] = 1;
}

/* ContainsWord (clsUserSession.vb:3885): every space-separated word of `check`
   appears among the space-separated words of `sentence` (case-insensitive).

   FAITHFUL QUIRK: the runner splits with VB `Split(x, " ")`, which KEEPS empty tokens.
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

/* Render a topic's <Conversation> with `ctx_key` as the text context and the
   intro flag set (the runner's conversation-reply emit), restoring both around
   the one AddResponse.  Shared by the intro / topic-reply / implicit-farewell
   paths of exec_conversation and clear_conv_if_partner_gone. */
static void
emit_topic_conv (a5_run_t *run, const char *ctx_key,
                 const a5_xml_node_t *conversation, sb_t *out)
{
  a5_state_t *st = run->st;
  const char *pc = st->ctx_char;
  int pia = st->intro_active;
  st->ctx_char = ctx_key;
  st->intro_active = 1;
  emit_conv (a5text_describe (st, conversation), out);
  st->intro_active = pia;
  st->ctx_char = pc;
}

/* On a topic whose trigger matched but whose restrictions failed, capture the
   restriction's fail-message text into *restr_text (last wins), so the caller can
   surface it as the "doesn't understand" default.  A no-op when restr_text is
   NULL (the callers that do not want the default). */
static void
capture_restr_text (a5_state_t *st, const a5_xml_node_t *restrictions,
                    std::string *restr_text)
{
  if (restr_text == NULL)
    return;
  const a5_xml_node_t *fm = a5restr_fail_message (st, restrictions);
  if (fm != NULL)
    { char *t2 = a5text_describe (st, fm); *restr_text = t2; free (t2); }
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
  if (t >= A5_CONV_GREET)    b_intro    = 1;   /* last bit: no further t use */

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
                  else
                    capture_restr_text (st, tp->restrictions, restr_text);
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
              else
                capture_restr_text (st, tp->restrictions, restr_text);
            }
        }
      else /* greet / farewell: no keyword match, just restrictions */
        {
          if (a5restr_pass (st, tp->restrictions))
            return tp;
          else
            capture_restr_text (st, tp->restrictions, restr_text);
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
            emit_topic_conv (run, prev->key, fw->conversation, out);
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
          emit_topic_conv (run, conv_ch->key, intro->conversation, out);
          a5state_set_conv_node (st, intro->key);
          if (intro->is_ask || intro->is_tell || intro->is_command)
            {
              a5state_set_conv_char (st, char_key);
              if (intro->actions != NULL)
                {
                  /* The runner runs topic actions through the ActionArrayList
                     ExecuteActions overload, which passes task = Nothing to
                     ExecuteSingleAction -- so they carry NO owning task even
                     though a library Say/Ask task triggered the conversation
                     (and a topic's Score changes never land; see the Score
                     gate in the variable actions). */
                  int saved_sti = run->cur_score_ti;
                  run->cur_score_ti = -1;
                  for (const a5_xml_node_t *c = intro->actions->first_child;
                       c != NULL; c = c->next)
                    run_action (run, c->name, c->text, 0, out);
                  run->cur_score_ti = saved_sti;
                }
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
      emit_topic_conv (run, conv_ch->key, topic->conversation, out);
      if (topic_has_children (conv_ch, topic->key))
        a5state_set_conv_node (st, topic->key);
      else if (!topic->stay_in_node)
        a5state_set_conv_node (st, "");
      if (topic->actions != NULL)
        {
          /* task = Nothing for topic actions, as above. */
          int saved_sti = run->cur_score_ti;
          run->cur_score_ti = -1;
          for (const a5_xml_node_t *c = topic->actions->first_child;
               c != NULL; c = c->next)
            run_action (run, c->name, c->text, 0, out);
          run->cur_score_ti = saved_sti;
        }
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
      {
        int pia = st->intro_active; st->intro_active = 1;
        emit_conv (a5text_process (st, msg.c_str ()), out);
        st->intro_active = pia;
      }
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
          emit_topic_conv (run, prev->key, fw->conversation, out);
      }
  }
  a5state_set_conv_char (st, "");
  a5state_set_conv_node (st, "");
}

/* ----------------------------------------------------------- action: execute */

/* ---- run_action per-kind handlers (extracted from run_action) ---- */

static void
act_move_object (a5_run_t *run, const char * /*kind*/,
                 const std::vector<std::string> &tk, const char * /*body*/,
                 int /*depth*/, sb_t * /*out*/)
{
  a5_state_t *st = run->st;
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
      /* Merged (runtime + static) test, matching clsObject.HasProperty: a
         property added at runtime via SetProperty must be seen here, exactly as
         the character EveryoneWithProperty branch already does. */
      for (int i = 0; i < st->adv->n_objects; i++)
        if (a5state_entity_has_prop (st, st->adv->objects[i].key, tk[1].c_str ()))
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
      else if (to == "ToLocationGroup")
        {
          /* The runner MoveObject->ToLocationGroup (clsUserSession.vb:1560): a STATIC
             object occupies the WHOLE group; a DYNAMIC object lands in ONE
             random member room (group.RandomKey).  AlienDiver scatters the
             crashed ship / sea creatures this way -- Seacreatur4 (dynamic) to
             a random ocean room, then the ship ToSameLocationAs it -- so the
             ship "must be located" and exit-ship can map back to a single
             room.  Setting a dynamic object to the whole group left the ship
             nowhere-in-particular and broke exit-ship. */
          if (L->is_static) { L->where = A5_OWHERE_LOCGROUP; L->key = k2; }
          else
            {
              int n = a5state_group_count (st, k2);
              if (n > 0)
                {
                  const char *m = a5state_group_member_at
                    (st, k2, a5rand_between (0, n - 1));
                  const a5_location_t *ld = a5model_location (st->adv, m);
                  L->where = A5_OWHERE_LOCATION; L->key = ld ? ld->key : m;
                }
              else { L->where = A5_OWHERE_HIDDEN; L->key = NULL; }
            }
        }
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

static void
act_object_group (a5_run_t *run, const char *kind,
                  const std::vector<std::string> &tk, const char * /*body*/,
                  int /*depth*/, sb_t * /*out*/)
{
  a5_state_t *st = run->st;
  /* "Object <obj> ToGroup <grp>" / "Object <obj> FromGroup <grp>"
     (clsAction AddObjectToGroup/RemoveObjectFromGroup): runtime membership,
     e.g. the flashlight joins LightSources while switched on so the darkness
     override's "MustNot BeInSameLocationAsObject LightSources" sees it.
     The bulk "EverythingWithProperty <prop> ToGroup <grp>" form sweeps every
     object carrying <prop> (The Salvage's startup seeds its Salvageable
     group this way before fanning placement out over the members). */
  if (tk.size () < 4)
    return;
  int add = streq (kind, "AddObjectToGroup");
  if (tk[0] == "EverythingWithProperty")
    {
      /* Merged (runtime + static) test -- see the MoveObject branch. */
      for (int i = 0; i < st->adv->n_objects; i++)
        if (a5state_entity_has_prop (st, st->adv->objects[i].key, tk[1].c_str ()))
          a5state_set_object_in_group (st, tk[3].c_str (),
                                       st->adv->objects[i].key, add);
      return;
    }
  if (tk[0] != "Object")
    return;
  const char *obj = act_key (st, tk[1].c_str ());
  const char *grp = tk[3].c_str ();
  a5state_set_object_in_group (st, grp, obj, add);
  return;
}

static void
act_move_character (a5_run_t *run, const char * /*kind*/,
                    const std::vector<std::string> &tk, const char * /*body*/,
                    int /*depth*/, sb_t *out)
{
  a5_state_t *st = run->st;
  if (tk.size () < 3)
    return;
  /* The "who" set: a single Character, or one of the runner's bulk source forms
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
      int n = a5state_group_count (st, whok);
      for (int m = 0; m < n; m++)
        { int ci = a5state_character_index (st,
                      a5state_group_member_at (st, whok, m));
          if (ci >= 0) cis.push_back (ci); }
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
  /* ToLocationGroup: the runner computes dest.Key = group.RandomKey ONCE per action
     (clsUserSession.vb:1767), so the whole MoveCharacter draws a single
     RandomKey and sends every affected character to the SAME room -- e.g.
     Jacaranda's champagne `MoveCharacter %Player% ToLocationGroup Group7`. */
  const char *group_dest = NULL;
  if (to == "ToLocationGroup" && tk.size () >= 4)
    {
      /* Read the group's LIVE membership (the runner arlMembers), not the static
         <Member> list: procedural games (Skybreak) populate the destination
         group at runtime via AddLocationToGroup right before the jump, so a
         static read finds 0 members and the player lands nowhere. */
      const char *gk = act_key (st, tk[3].c_str ());
      int n = a5state_group_count (st, gk);
      if (n > 0)
        {
          const char *m = a5state_group_member_at (st, gk,
                                                   a5rand_between (0, n - 1));
          /* Canonicalise to the stable model location key: the gm entry is
             an owned copy that is freed when the group is cleared later this
             turn (the post-jump EverywhereInGroup sweep), so storing it in
             char_loc would dangle. */
          const a5_location_t *ld = a5model_location (st->adv, m);
          group_dest = ld ? ld->key : m;
        }
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
          dest = here ? a5restr_exit_in_direction (st, k1, here, canon, NULL)
                      : NULL;
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
            {
              st->char_onobj[ci] = k2; st->char_in[ci] = 0;
              /* The runner keys the character's location off the furniture
                 (ExistsWhere OnObject -> clsCharacterLocation.LocationKey
                 follows the object), so seating someone on a chair in
                 ANOTHER room moves them there -- GFS's John Boom "invites
                 you in" seats the player on the living-room cosy chair.
                 Scarier stores the room explicitly: sync it. */
              int oj = a5state_object_index (st, k2);
              const char *loc = NULL;
              if (oj >= 0)
                {
                  /* A static object placed at a location GROUP exists at
                     many rooms (AoK's cell floor, Group74): keep the
                     character where it is when the furniture is present
                     there too, else the first-match scan below teleports
                     the player out of the cell on `lie down`. */
                  if (st->char_loc[ci] != NULL
                      && a5state_object_at_location (st, oj, st->char_loc[ci], 1))
                    loc = st->char_loc[ci];
                  else
                    for (int li = 0; li < st->adv->n_locations; li++)
                      if (a5state_object_at_location (st, oj,
                                                      st->adv->locations[li].key, 1))
                        { loc = st->adv->locations[li].key; break; }
                }
              if (loc != NULL && !streq (loc, st->char_loc[ci]))
                {
                  if (streq (k1, a5state_player_key (st)))
                    enqueue_loc_trigger_tasks (run, st->char_loc[ci], loc);
                  st->char_loc[ci] = loc;
                  if (streq (k1, a5state_player_key (st)))
                    clear_conv_if_partner_gone (run, out);
                }
            }
        }
      else if (to == "InsideObject" || to == "OntoObject")
        {
          /* clsUserSession MoveCharacterToEnum.InsideObject / OntoObject:
             the character is now in/on the object
             (clsCharacterLocation.ExistsWhere InsideObject/OnObject).
             char_in distinguishes the two so BeInsideObject / BeOnObject
             read it back correctly -- e.g. FBA's `hide in niche`
             (MoveCharacter Player InsideObject cl_Niche1) which the custodian
             "goes past" check gates on. */
          const char *k2 = (tk.size () >= 4) ? act_key (st, tk[3].c_str ()) : NULL;
          if (k2 != NULL)
            {
              st->char_onobj[ci] = k2;
              st->char_in[ci] = (to == "InsideObject") ? 1 : 0;
              /* The runner keys the character's location off the container
                 (clsCharacterLocation.LocationKey follows the object), so
                 putting someone inside an object in ANOTHER room moves them
                 there -- Axe of Kolt sends Grat from his burrow (Loc112)
                 into the loo hut (Object358 @Loc107); leaving char_loc at
                 the burrow made the Caught-By-Grat death (gated on him
                 being AT Loc112) fire while he was in the loo.  Sync it,
                 like the sit-on-furniture branch above. */
              const char *loc = NULL;
              /* Prefer the character's current room when the container is
                 present there (multi-location statics), like the
                 sit-on-furniture sync above. */
              if (st->char_loc[ci] != NULL
                  && a5state_object_key_at_location (st, k2, st->char_loc[ci], 0))
                loc = st->char_loc[ci];
              else
                for (int li = 0; li < st->adv->n_locations; li++)
                  if (a5state_object_key_at_location (st, k2,
                                                      st->adv->locations[li].key, 0))
                    { loc = st->adv->locations[li].key; break; }
              if (loc != NULL && (st->char_loc[ci] == NULL
                                  || !streq (loc, st->char_loc[ci])))
                {
                  if (streq (k1, a5state_player_key (st)))
                    enqueue_loc_trigger_tasks (run, st->char_loc[ci], loc);
                  st->char_loc[ci] = loc;
                  if (streq (k1, a5state_player_key (st)))
                    clear_conv_if_partner_gone (run, out);
                }
            }
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
              /* HasSeenObject/-Location/-Character are per-character in the runner:
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

      /* clsCharacter.Move marks the destination location -- and, on an
         AtLocation arrival, its objects and visible characters -- seen for
         the moving character at move time; our set is player-centric (like
         obj_seen).  See mark_player_arrival_seen. */
      if (streq (k1, a5state_player_key (st)))
        {
          if (st->char_onobj[ci] == NULL)
            mark_player_arrival_seen (st, st->char_loc[ci]);
          else
            a5state_mark_loc_seen (st, st->char_loc[ci]);
        }
    }
  return;
}

static void
act_set_variable (a5_run_t *run, const char *kind,
                  const std::vector<std::string> & /*tk*/, const char *body,
                  int /*depth*/, sb_t * /*out*/)
{
  a5_state_t *st = run->st;
  std::string name, value, idxstr;
  int vi;
  if (!split_assignment (body, name, value))
    return;
  /* The runner's action parser splits the LHS at '[' (FileIO.vb: sKey1 =
     sElements(0).Split("[")(0), sKey2 = the bracketed index).  The index is
     ignored for a Length-1 variable (clsUserSession.vb:2135, IntValue
     defaults to 1) -- AoK's push-doors task writes
     "Variable330[Hidden] = ""1""" and the lookup must see Variable330.
     For an ArrayLength>1 variable it selects the element to assign
     (WW2 Elevator Escape's cl_Buttonarra[cl_Variable1] = "1"). */
  size_t br = name.find ('[');
  if (br != std::string::npos)
    {
      size_t cb = name.find (']', br);
      idxstr = name.substr (br + 1, (cb == std::string::npos)
                                        ? std::string::npos : cb - br - 1);
      name.erase (br);
      while (!name.empty () && isspace ((unsigned char) name.back ()))
        name.pop_back ();
    }
  vi = a5state_variable_index (st, name.c_str ());
  if (vi < 0) return;
  /* Resolve the element index like clsUserSession.vb:2136-2144: a
     "ReferencedNumber" prefix reads the parser's number reference, a
     numeric literal is used as-is, anything else is a variable KEY whose
     (element-1) value is the index. */
  long elem_idx = 1;
  if (st->adv->variables[vi].array_length > 1 && !idxstr.empty ())
    {
      if (idxstr.compare (0, 16, "ReferencedNumber") == 0)
        {
          const char *rn = a5state_lookup_ref (st, "ReferencedNumber");
          elem_idx = rn != NULL ? strtol (rn, NULL, 10) : 0;
        }
      else if (idxstr.find_first_not_of ("0123456789 -") == std::string::npos)
        elem_idx = strtol (idxstr.c_str (), NULL, 10);
      else
        {
          int ivi = a5state_variable_index (st, idxstr.c_str ());
          elem_idx = ivi >= 0 ? a5state_var_get_elem (st, ivi, 1) : 0;
        }
    }
  if (streq (st->adv->variables[vi].type, "Text") && streq (kind, "SetVariable"))
    {
      /* The runner evaluates the RHS as an expression, so a bare string-literal value
         unwraps to its contents.  ADRIFT writes such literals with their own
         quotes inside the assignment's delimiters (Playeraxe = ""an"" or
         = "'an'"); split_assignment stripped one outer layer, leaving "an" /
         'an'.  Strip a remaining fully-surrounding matched quote pair so
         %playeraxe% renders `an`, not `'an'`/`"an"` (Tingalan inventory).
         Only when it is a lone literal (no interior same-quote), so a
         concatenation like "a" & "b" is left for a5text_process untouched. */
      std::string tv = value;
      if (getenv ("A5_TRACE_SETVAR"))
        fprintf (stderr, "[SETVAR] %s = <<%s>>\n", name.c_str (), tv.c_str ());
      /* A bare top-level function call ("UCASE(%text%)", Skybreak's
         Playername1 assignment) is a real expression, not literal text --
         route it through the full evaluator so %text% resolves and
         quotes (expr_substitute's bExpression quoting) before UCASE runs,
         instead of falling into the plain %ref%-substitution below, which
         left the literal "UCASE(...)" wrapper in the stored string. */
      std::string call_name;
      if (a5_bare_function_call (tv, call_name) && a5sexpr_is_function (lower_copy (call_name)))
        {
          char *ev = a5text_eval_expression (st, tv.c_str ());
          free (st->var_text[vi]);
          st->var_text[vi] = ev;
          return;
        }
      /* A string-concatenation expression -- a `+` operator outside any
         quoted literal, joining %functions%/%vars%/"literals" (the runner's
         SetToExpression is uniform: `+` on non-numeric operands concatenates,
         and its bExpression ReplaceFunctions drops the quotes around each
         substituted value).  LostCoastlines builds every generated place name
         this way: `%names[%pointer%]%+%space%+%types[%locationtype%]%`
         (%space% = " ") -> "cabal plain", and `"The"+%space%+...+"of"+...`.
         The plain %ref%-substitution path below would leave the literal `+`
         and the quote-stripped " " as `cabal+ +plain`. */
      {
        int in_q = 0;  char qc = 0;  int has_plus = 0;
        for (char c : tv)
          {
            if (in_q) { if (c == qc) in_q = 0; }
            else if (c == '"' || c == '\'') { in_q = 1; qc = c; }
            else if (c == '+') { has_plus = 1; break; }
          }
        if (has_plus)
          {
            char *ev = a5text_eval_expression (st, tv.c_str ());
            free (st->var_text[vi]);
            st->var_text[vi] = ev;
            return;
          }
      }
      /* The runner evaluates the RHS through SetToExpression, whose
         ReplaceFunctions (bExpression=True) substitutes every TEXT
         variable reference WITH surrounding quotes (Global.vb:1977).
         When the RHS is itself a single quoted literal, that nested
         quoting splits the literal and leaves a bare word token, so
         GetToken/classification throws "Bad token", the exception is
         swallowed (clsVariable.vb SetToExpression's blanket Try) and the
         assignment NEVER LANDS: the variable silently keeps its old
         value.  the virtual human sets Variable28 = "that dream %s/he%
         had" this way and the runner's %human_thinking% stays empty for the rest
         of the game.  Numeric variables substitute as bare digits and
         survive, so only a Text-variable reference kills the assignment.
         split_assignment already peeled the literal's quotes off `value`,
         so re-inspect the raw RHS for the quoted-literal shape. */
      {
        /* Which string does the runner's SetToExpression actually see?  Files
           newer than 5.0.32 (dFileVersion > 5.0000321, FileIO.vb:426)
           store the expression wrapped in one EXTRA quote layer that the runner
           peels at load -- split_assignment already mirrored that peel,
           so the runner's view is `tv`.  Older files (the virtual human,
           5.000017) are stored verbatim and the runner does NOT peel, so the runner's
           view is the raw RHS, quotes and all. */
        std::string rhs;
        double fver = (st->adv->version != NULL) ? atof (st->adv->version) : 0;
        if (fver > 5.0000321)
          rhs = tv;
        else
          {
            rhs = body;
            size_t eqp = rhs.find ('=');
            rhs = (eqp == std::string::npos) ? "" : rhs.substr (eqp + 1);
            size_t b = rhs.find_first_not_of (" \t");
            rhs = (b == std::string::npos) ? "" : rhs.substr (b);
            while (!rhs.empty () && isspace ((unsigned char) rhs.back ()))
              rhs.pop_back ();
          }
        if (rhs.size () >= 2 && (rhs[0] == '"' || rhs[0] == '\'')
            && rhs[rhs.size () - 1] == rhs[0]
            && rhs.find (rhs[0], 1) == rhs.size () - 1)
          {
            size_t sp = 1;
            while ((sp = rhs.find ('%', sp)) != std::string::npos
                   && sp + 1 < rhs.size () - 1)
              {
                size_t ep = rhs.find ('%', sp + 1);
                if (ep == std::string::npos || ep >= rhs.size () - 1)
                  break;
                std::string ref = rhs.substr (sp + 1, ep - sp - 1);
                int rvi;
                for (rvi = 0; rvi < st->adv->n_variables; rvi++)
                  if (st->adv->variables[rvi].name != NULL
                      && strcasecmp (st->adv->variables[rvi].name,
                                     ref.c_str ()) == 0)
                    break;                 /* The runner matches by NAME, any case */
                if (rvi < st->adv->n_variables
                    && streq (st->adv->variables[rvi].type, "Text"))
                  {
                    if (getenv ("A5_TRACE_SETVAR"))
                      fprintf (stderr, "[SETVAR-DROP] %s = <<%s>>\n",
                               name.c_str (), rhs.c_str ());
                    return;                /* The runner: bad expression, no-op */
                  }
                sp = ep + 1;
              }
          }
      }
      if (tv.size () >= 2 && (tv[0] == '"' || tv[0] == '\'')
          && tv[tv.size () - 1] == tv[0]
          && tv.find (tv[0], 1) == tv.size () - 1)
        tv = tv.substr (1, tv.size () - 2);
      /* Store WITHOUT the display-time auto-capitalisation: the runner keeps the raw
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
         re-awards its points.  Non-Score variables are never gated.

         The guard's other half: the runner only touches Score at all when a task
         is executing (`var.Key <> "Score" OrElse task IsNot Nothing`) --
         actions run from a conversation TOPIC or an event pass task =
         Nothing, so their Score changes silently never land (Thy
         Balconyman's intro/lips/haircut topics all IncVariable Score). */
      int is_score = streq (st->adv->variables[vi].key, "Score");
      if (is_score && run->cur_score_ti < 0)
        return;                           /* no owning task -- Score frozen */
      if (is_score)
        {
          if (st->task_scored[run->cur_score_ti])
            return;                       /* already scored -- suppress */
          st->task_scored[run->cur_score_ti] = 1;
        }
      long delta = eval_num_value (st, value.c_str ());
      /* A5_DEBUG_SCORE=<varkey> also traces a game's CUSTOM score
         variable (e.g. Murder Most Foul counts points in "Scor1"),
         without the once-only Score gate (debug print only).  */
      {
        const char *dbg = getenv ("A5_DEBUG_SCORE");
        if (dbg && !is_score && streq (st->adv->variables[vi].key, dbg))
          fprintf (stderr, "[score] turn=%d task=%s %s %s %ld (now %ld)\n",
                   st->turns,
                   run->cur_score_ti >= 0 ? st->adv->tasks[run->cur_score_ti].key : "?",
                   st->adv->variables[vi].key,
                   kind, delta, st->var_num[vi] + (streq (kind, "IncVariable") ? delta : 0));
      }
      if (is_score && getenv ("A5_DEBUG_SCORE"))
        fprintf (stderr, "[score] turn=%d task=%s %s %ld (now %ld)\n",
                 st->turns,
                 run->cur_score_ti >= 0 ? st->adv->tasks[run->cur_score_ti].key : "?",
                 kind, delta, st->var_num[vi] + (streq (kind, "IncVariable") ? delta : 0));
      /* The runner stores through SetToExpression(sExpr, iIndex): Inc/Dec build
         "%name% +/- value", and %name% on an array variable reads element
         1 (GetToken's bare var- token), so the result lands at elem_idx
         but the BASE is always element 1 (== the var_num mirror). */
      long newval;
      if (streq (kind, "SetVariable"))      newval = delta;
      else if (streq (kind, "IncVariable")) newval = st->var_num[vi] + delta;
      else                                  newval = st->var_num[vi] - delta;
      a5state_var_set_elem (st, vi, elem_idx, newval);
    }
  return;
}

static void
act_set_property (a5_run_t *run, const char * /*kind*/,
                  const std::vector<std::string> &tk, const char * /*body*/,
                  int /*depth*/, sb_t * /*out*/)
{
  a5_state_t *st = run->st;
  if (tk.size () < 3) return;
  const char *k1 = act_key (st, tk[0].c_str ());
  std::string val;
  for (size_t i = 2; i < tk.size (); i++)
    { if (i > 2) val += " "; val += tk[i]; }
  /* clsUserSession SetProperties: an Integer/Text/StateList/ValueList
     property value is an expression run through EvaluateExpression
     (clsUserSession.vb:2010) -- e.g. "PCASE(%text%)" for the player's typed
     name, or "RAND (25, 50)" for a randomised item Value.  Key-typed
     properties (Object/Character/LocationKey) and SelectionOnly are stored
     verbatim (the runner's non-evaluating branches).

     the runner only reaches that EvaluateExpression on the branch where the property
     ALREADY EXISTS on the entity (`prop IsNot Nothing`); when a property is
     being freshly ADDED, only SelectionOnly/ObjectKey are handled and an
     Integer/Text/... value is left at its clone default -- never evaluated.
     So gate the value-type evaluation on current presence: force it for a
     present value-typed property (this is what draws the RNG for
     LostCoastlines' `SetProperty <obj> Value RAND(a,b)` world-gen block),
     and otherwise fall back to the %reference% heuristic so a bare
     key/state value -- or a value on a not-yet-present property (Illumina) --
     is stored raw (the runner's add-branch / Nothing->raw behaviour). */
  const a5_propdef_t *pd = a5model_propdef (st->adv, tk[1].c_str ());
  const char *pt = pd ? pd->type : NULL;
  int value_type = pt != NULL
                   && (streq (pt, "Integer") || streq (pt, "Text")
                       || streq (pt, "StateList") || streq (pt, "ValueList"));
  int has_prop = a5state_entity_prop (st, k1, tk[1].c_str ()) != NULL;
  /* The runner's SetProperties add-branch is asymmetric (clsUserSession.vb): an ABSENT
     property is cloned-and-added freely for OBJECTS (vb:1989), but for a
     CHARACTER only a SelectionOnly-<Selected> (vb:2044) or CharacterProperName
     (vb:2036) is added -- a value-typed property (Integer/Text/StateList/
     ValueList) set to a plain value on an absent slot is DebugPrinted and
     ignored (vb:2056).  Galen's Quest shrinks the player with `SetProperty
     %Player% MaxBulk 90`, but the player has no MaxBulk by default, so in the runner
     the property is never added and carry stays unlimited -- which is why the
     729-bulk iron key remains takeable while shrunk inside the miniature
     castle (where `cast grow on me` is blocked).  Mirror the no-op. */
  if (!has_prop && value_type
      && a5state_character_index (st, k1) >= 0
      && a5state_object_index (st, k1) < 0
      && strcmp (tk[1].c_str (), "CharacterProperName") != 0)
    return;
  if ((value_type && has_prop) || val.find ('%') != std::string::npos)
    {
      char *ev = a5text_eval_expression (st, val.c_str ());
      if (ev != NULL && ev[0] != '\0') val = ev;
      free (ev);
    }
  /* A key-typed property (ObjectKey/CharacterKey/LocationKey) set to a bare
     reference sentinel -- "ReferencedObject", "ReferencedCharacter1", ... or
     "Player" -- must be resolved to the currently-bound entity KEY, the way
     the runner stores the resolved key (clsUserSession.SetProperties key branch) and
     not the sentinel word.  LMK's `equip %object%` runs `SetProperty %Player%
     EquippedWe ReferencedObject`; left raw, EquippedWe holds the literal
     "ReferencedObject" and never equals the weapon key the combat
     restrictions test (Knife / s_Sword / Nothing), so NO hit-or-miss subtask
     fires and `kill squid` falls through to NotUnderstood.  A literal key or
     "Nothing" is not a per-turn ref binding, so lookup returns NULL and the
     value is kept verbatim. */
  {
    const char *kt = pd ? pd->type : NULL;
    int is_key_prop = kt != NULL
                      && (streq (kt, "ObjectKey") || streq (kt, "CharacterKey")
                          || streq (kt, "LocationKey"));
    if (is_key_prop && val.find ('%') == std::string::npos
        && val != "Nothing")
      {
        const char *rk = (val == "Player")
                         ? a5state_player_key (st)
                         : a5state_lookup_ref (st, val.c_str ());
        if (rk != NULL && rk[0] != '\0') val = rk;
      }
  }
  a5state_set_prop (st, k1, tk[1].c_str (), val.c_str ());
  /* The runner derives an object's location live from its DynamicLocation property, so
     `SetProperty <obj> DynamicLocation Hidden` (Galen's Quest hides the meteor
     once the fountain has swallowed it, via GiveMeteor) immediately relocates
     it.  Scarier caches location in st->obj[], so mirror MoveObject's mapping
     here or the object lingers stale (a phantom meteorite in the player's
     hands).  Only DynamicLocation drives relocation; the keyed variants read
     their companion key property (runtime override, else the static value). */
  if (tk[1] == "DynamicLocation")
    {
      int oi = a5state_object_index (st, k1);
      if (oi >= 0)
        {
          a5_objloc_t *L = &st->obj[oi];
          const char *w = val.c_str (), *kp = NULL;
          if (streq (w, "Hidden"))           { L->where = A5_OWHERE_HIDDEN;      L->key = NULL; }
          else if (streq (w, "Held By Character")) { L->where = A5_OWHERE_HELD_BY;   kp = "HeldByWho"; }
          else if (streq (w, "Worn By Character")) { L->where = A5_OWHERE_WORN_BY;   kp = "WornByWho"; }
          else if (streq (w, "In Location"))       { L->where = A5_OWHERE_LOCATION;  kp = "InLocation"; }
          else if (streq (w, "Inside Object"))     { L->where = A5_OWHERE_IN_OBJECT; kp = "InsideWhat"; }
          else if (streq (w, "On Object"))         { L->where = A5_OWHERE_ON_OBJECT; kp = "OnWhat"; }
          if (kp != NULL)
            {
              const char *kv = a5state_entity_prop (st, k1, kp);
              if (kv == NULL)
                {
                  const a5_object_t *mo = a5model_object (st->adv, k1);
                  for (int pi = 0; mo != NULL && pi < mo->n_props; pi++)
                    if (streq (mo->props[pi].key, kp))
                      { kv = mo->props[pi].value; break; }
                }
              if (streq (kv, "%Player%")) kv = st->player_key;
              L->key = kv;
            }
        }
    }
  /* clsCharacter.ProperName tracks the CharacterProperName property
     (clsUserSession.vb:2080), so .Name/.ProperName render the new value. */
  if (tk[1] == "CharacterPosition")
    {
      int ci = a5state_character_index (st, k1);
      if (ci >= 0) { free (st->char_position[ci]); st->char_position[ci] = strdup (val.c_str ()); }
    }
  return;
}

static void
act_end_game (a5_run_t *run, const char * /*kind*/,
              const std::vector<std::string> &tk, const char * /*body*/,
              int /*depth*/, sb_t * /*out*/)
{
  a5_state_t *st = run->st;
  st->game_over = 1;
  free (st->end_message);
  st->end_message = strdup (tk.empty () ? "" : tk[0].c_str ());
  return;
}

static void
act_time (a5_run_t *run, const char * /*kind*/,
          const std::vector<std::string> &tk, const char * /*body*/,
          int /*depth*/, sb_t *out)
{
  a5_state_t *st = run->st;
  /* clsUserSession Time action (`Skip "N" turns`, vb:2357): Display the
     buffered pass responses, then run TurnBasedStuff N times, then clear
     the response buffer.  Lost Children's WaitZ task advances the
     Flight-1 countdown 3 extra turns per `z`. */
  long n = 0;
  if (tk.size () >= 2)
    {
      std::string sv = tk[1];
      if (sv.size () >= 2 && sv.front () == '"' && sv.back () == '"')
        sv = sv.substr (1, sv.size () - 2);
      char *endp = NULL;
      n = strtol (sv.c_str (), &endp, 10);
      if (endp == sv.c_str ())        /* not a literal: EvaluateExpression */
        {
          char *ev = a5text_eval_expression (st, sv.c_str ());
          if (ev != NULL) { n = strtol (ev, NULL, 10); free (ev); }
        }
    }
  if (run->resp != NULL)
    {
      /* The runner flushes htblResponsesPass before the ticks so the command's own
         message precedes any event output, then Clear()s it (keep nmut --
         it is the "did the task produce output" history probe). */
      resp_flush (run, run->resp, out);
      run->resp->ents.clear ();
    }
  for (long i = 0; i < n && !st->game_over; i++)
    ev_tick_all (run, out);
  return;
}

static void
act_set_tasks (a5_run_t *run, const char * /*kind*/,
               const std::vector<std::string> &tk, const char *body,
               int depth, sb_t *out)
{
  a5_state_t *st = run->st;
  if (tk.empty ()) return;
  /* "FOR Loop = a TO b : Execute <key> : NEXT Loop" (FileIO.vb:365-374:
     IntValue = element 3, sPropertyValue = element 5, the Execute starts
     at element 7).  clsUserSession's SetTasks case runs the plain Execute
     b-a+1 times; the loop counter itself is not visible to the executed
     task (only SetVariable-in-a-FOR substitutes %Loop%). */
  if (tk[0] == "FOR" && tk.size () >= 9 && tk[4] == "TO" && tk[6] == ":")
    {
      long from = strtol (tk[3].c_str (), NULL, 10);
      long to   = strtol (tk[5].c_str (), NULL, 10);
      std::string inner;
      for (size_t i = 7; i < tk.size (); i++)
        {
          if (tk[i] == ":")             /* ": NEXT Loop" tail */
            break;
          if (!inner.empty ()) inner += " ";
          inner += tk[i];
        }
      if (!inner.empty () && depth < 16)
        for (long l = from; l <= to && !st->game_over; l++)
          run_action (run, "SetTasks", inner.c_str (), depth + 1, out);
      return;
    }
  if (tk[0] == "Execute" && tk.size () >= 2)
    {
      std::string key = tk[1];
      if (key == "Look")            /* built-in room view + the Look task's actions */
        {
          /* The runner's ExecuteTask(Look) is a full AttemptToExecuteTask(Look): its
             Specific overrides run before the view, then the room view
             (Look's AggregateOutput CompletionMessage), then Look's own
             <Actions>.  Routing through execute_task_with_overrides fires
             both -- e.g. Amazon's `Beforeplay1` -> `Execute ts_tasCheckTime`
             (the "Date: ..." line: startup, re-looks and per-move) AND
             Grandpa's `vnl_TutorialSt` chain off Look's actions.

             The per-move duplicate is real: PlayerMovement's `Beforeplay`
             override already ran ts_tasCheckTime, so its `Execute Look` ->
             `Beforeplay1` emits the same "Date: ..." a second time.  The runner's
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
          std::vector<std::string> args;
          bool have_args = false;
          {
            size_t lp = b.find ('(');
            size_t rp = b.rfind (')');
            if (lp != std::string::npos && rp != std::string::npos && rp > lp)
              {
                have_args = true;
                std::string argstr = b.substr (lp + 1, rp - lp - 1);
                std::string cur;
                for (char c : argstr)
                  { if (c == '|') { args.push_back (cur); cur.clear (); }
                    else cur += c; }
                args.push_back (cur);
              }
          }
          std::vector<std::string> rnames = command_refs (tt);

          /* A bare `Group.<Property>` argument (e.g. `Objects1.StaticOrDynamic`)
             is an OO reference that the runner expands to the group's member LIST and
             runs the executed task once per member (ExecuteSubTasks iterating
             the reference's items).  Detect it and iterate; a plain argument
             leaves giter < 0 and runs the task exactly once, as before. */
          int giter = -1;
          std::vector<std::string> gmembers;
          for (size_t i = 0; i < args.size (); i++)
            if (group_prop_member_keys (st, args[i], gmembers))
              { giter = (int) i; break; }

          int tti = a5state_task_index (st, key.c_str ());
          bool ran_any = false;

          /* The Completed/Repeatable gate is the runner's AttemptToExecuteTask entry
             test, evaluated ONCE per Execute -- so a non-repeatable task run
             over a PLURAL group reference still fires its actions for every
             member (the runner's ExecuteSubTasks iterates the items inside one task
             execution; Completed flips only afterwards).  Snapshot the flag
             at entry and gate on that, or a task that marks itself done on
             member 0 (execute_task_with_overrides) would skip members 1..n --
             AlienDiver's ShowCardsP3 (Repeatable=0, `Unset` then Execute'd
             over the crafted cards to stamp each a unique CardId) then stamped
             only the first card, so `pc`'s CardId==%number% matched every held
             card and over-awarded colored fragments. */
          bool tt_done_at_entry = (tti >= 0 && st->task_done[tti]);

          /* One execution of the target task with the references bound from
             the current `args` (the group member substituted into args[giter]
             on each pass).  Returns via early-out on a Completed/Repeatable or
             restriction gate -- the runner's AttemptToExecuteSubTask per item. */
          auto run_one = [&] (void) {
              if (have_args)
                {
                  /* The runner (clsUserSession.vb:2169-2298) builds a FRESH reference
                     array (ReferencesNew) for the sub-task and only swaps it in
                     (`NewReferences = ReferencesNew`) AFTER every parameter has
                     been resolved -- so each computed parameter's OO chain is
                     evaluated against the PARENT's live references, never
                     against an earlier parameter's freshly-bound value.  Mirror
                     that with a two-phase pass: resolve every non-passthrough
                     argument against the current (parent) state FIRST, then
                     apply all the binds.  A single-pass bind would let e.g.
                     AlienDiver's `Execute CraftACard8
                     (%Player%.Location.Objects.ObjectIsAT|%object%)` read the
                     cube that arg0 just stamped onto ReferencedObject1 when
                     resolving arg1's `%object%`, mis-binding ReferencedObject2
                     to the cube instead of the parent's crafted card. */
                  std::vector<std::pair<size_t, std::string> > deferred;
                  for (size_t i = 0; i < args.size () && i < rnames.size (); i++)
                    {
                      /* The runner's SetTasks handler (clsUserSession.vb:2188) passes a
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
                         literal key, or an expanded group member) instead builds
                         the runner's UserDefinedRef, whose fresh items carry NO
                         sCommandReference -- bind an empty typed text. */
                      std::string an = args[i];
                      { size_t b1 = an.find_first_not_of (" \t");
                        size_t e1 = an.find_last_not_of (" \t");
                        an = (b1 == std::string::npos)
                               ? "" : an.substr (b1, e1 - b1 + 1); }
                      if (an.size () >= 2 && an.front () == '%' && an.back () == '%')
                        {
                          std::string base = an.substr (1, an.size () - 2);
                          normalize_singular_ref (base);
                          if (base == rnames[i])
                            continue;               /* pass the ref through */
                        }
                      char *val = eval_arg_to_key (st, args[i]);
                      deferred.push_back (std::make_pair (i, std::string (val)));
                      free (val);
                    }
                  for (size_t d = 0; d < deferred.size (); d++)
                    bind_reference (st, rnames[deferred[d].first].c_str (),
                                    deferred[d].second.c_str (), "");
                }
              /* AttemptToExecuteTask: gate on Completed/Repeatable and the
                 task's own restrictions (with any refs just bound above) --
                 the stock list-runner tasks (e.g. the bomb cascade) execute a
                 list of candidate tasks and rely on each one's restrictions to
                 pick the one that fires. */
              if (tt_done_at_entry && !tt->repeatable)
                return;
              if (!a5restr_pass (st, tt->restrictions))
                {
                  /* The runner's ExecuteTask is a full AttemptToExecuteTask: a
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
                              /* Buffer for the end-of-scope flush, where the runner's
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
              /* The runner's ExecuteTask is a full AttemptToExecuteTask, so the
                 executed task's own Specific overrides apply too -- e.g. the
                 lazy take-from-others re-dispatch lets PutSomeDry (the "skull"
                 override of TakeObjectsFromOthers) fire, moving Anno's
                 gunpowder into the held skull.  The downstream cannon puzzle
                 reads it via the now recursive BeHoldingObject. */
              std::vector<ref_info> refs = refs_from_bindings (st, tt);
              execute_task_with_overrides (run, tt, refs, 0, out);
              ran_any = true;
          };

          /* The runner wraps each SetTasks Execute in `oExistingRefs = NewReferences`
             ... `NewReferences = oExistingRefs` (clsUserSession.vb:2172/2310),
             so the sub-task's reference bindings are LOCAL to that one Execute
             and the parent's references are restored afterwards.  AlienDiver's
             craft chain relies on this: CraftACard2 runs `Execute CraftACard5`,
             `Execute CraftACard8`, ... in sequence, each `(...ObjectIsAT |
             %object%)`.  The first Execute binds object1 (and thus the bare
             ReferencedObject alias) to the cube, and without a restore the NEXT
             sibling's `%object%` would read that cube instead of the parent's
             crafted card -- mis-imprinting CraftedCar onto the cube and losing
             the play-match bonus.  Snapshot the parent bindings here, restore
             them before each group member and once after the whole Execute. */
          ref_snap parent_refs;
          ref_snap_take (st, &parent_refs);
          if (giter >= 0)
            {
              /* Iterate the executed task once per resolved group member,
                 substituting each member key into the group argument slot.
                 A prior action in this same task may have already ended the
                 game (e.g. I Summon Thee's win task runs `EndGame Win`
                 BEFORE `Execute CheckEscap (Everything.StaticOrDynamic)`,
                 which tallies Objects Destroyed): the runner keeps iterating every
                 member of a sub-task regardless -- game-over is only checked
                 back in the main loop.  So break only on a game-over NEWLY
                 triggered by a member's own execution, not one inherited
                 from before the loop, or the tally stops after member 0.
                 Restore the parent bindings before each member so every
                 member's non-iterated args resolve against the parent (the runner
                 fixes them once in ReferencesNew and reuses across items). */
              bool was_over = st->game_over;
              std::string saved = args[giter];
              for (const std::string &mk : gmembers)
                {
                  ref_snap_restore (st, &parent_refs);
                  args[giter] = mk;
                  run_one ();
                  if (st->game_over && !was_over)
                    break;
                }
              args[giter] = saved;
            }
          else
            run_one ();
          ref_snap_restore (st, &parent_refs);

          /* Mark done + fire completion controls once per AttemptToExecuteTask
             (the runner flips task.Completed once), and only when the task actually
             ran -- a wholly-failing Execute leaves it uncompleted, as before. */
          if (ran_any)
            {
              if (tti >= 0)
                st->task_done[tti] = 1;
              ev_on_task_completed (run, key.c_str (), out);
            }
        }
    }
  else if (tk[0] == "Unset" || tk[0] == "Clear")
    {
      if (tk.size () >= 2)
        { int ti = a5state_task_index (st, tk[1].c_str ()); if (ti >= 0) st->task_done[ti] = 0; }
    }
  return;
}

static void
act_conversation (a5_run_t *run, const char * /*kind*/,
                  const std::vector<std::string> &tk, const char * /*body*/,
                  int /*depth*/, sb_t *out)
{
  a5_state_t *st = run->st;
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

static void
act_location_group (a5_run_t *run, const char *kind,
                    const std::vector<std::string> &tk, const char * /*body*/,
                    int /*depth*/, sb_t * /*out*/)
{
  a5_state_t *st = run->st;
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
     operand maps to its entity key (the runner's ReferenceControl expansion).  A
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
      /* Live members (the runner arlMembers): the group-clear that follows each
         Skybreak jump is `RemoveLocationFromGroup EverywhereInGroup G
         FromGroup G`, which must see the runtime-added members to empty the
         group -- a static read leaves them and the group accretes forever. */
      int n = a5state_group_count (st, k1);
      for (int m = 0; m < n; m++)
        locs.push_back (a5state_group_member_at (st, k1, m));
    }
  else if (what == "EverywhereWithProperty")
    {
      /* Merged (runtime + static, incl. group-inherited) test -- see the
         MoveObject branch. */
      for (int i = 0; i < st->adv->n_locations; i++)
        { const a5_location_t *l = &st->adv->locations[i];
          if (a5state_entity_has_prop (st, l->key, k1))
            locs.push_back (l->key); }
    }
  else
    return;
  for (size_t i = 0; i < locs.size (); i++)
    a5state_set_object_in_group (st, grp, locs[i].c_str (), add);
  return;
}

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
    { act_move_object (run, kind, tk, body, depth, out); return; }
  if (streq (kind, "AddObjectToGroup") || streq (kind, "RemoveObjectFromGroup"))
    { act_object_group (run, kind, tk, body, depth, out); return; }
  if (streq (kind, "MoveCharacter"))
    { act_move_character (run, kind, tk, body, depth, out); return; }
  if (streq (kind, "SetVariable") || streq (kind, "IncVariable")
      || streq (kind, "DecVariable"))
    { act_set_variable (run, kind, tk, body, depth, out); return; }
  if (streq (kind, "SetProperty"))
    { act_set_property (run, kind, tk, body, depth, out); return; }
  if (streq (kind, "EndGame"))
    { act_end_game (run, kind, tk, body, depth, out); return; }
  if (streq (kind, "Time"))
    { act_time (run, kind, tk, body, depth, out); return; }
  if (streq (kind, "SetTasks"))
    { act_set_tasks (run, kind, tk, body, depth, out); return; }
  if (streq (kind, "Conversation"))
    { act_conversation (run, kind, tk, body, depth, out); return; }
  if (streq (kind, "AddLocationToGroup") || streq (kind, "RemoveLocationFromGroup"))
    { act_location_group (run, kind, tk, body, depth, out); return; }
}


/* ------------------------------------------------------------- run one task */

/* The object keys currently bound as this task-chain's %objects%/%object%
   reference -- the ReferencedObjects pipe list when present, else the singular
   ReferencedObject1.  Used for the runner's pass-cancels-fail response rule (see
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
   line -- the Adrift 5 runner trims each response the same way. */
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
         form (`proc`, what the runner's Display sees) drives the output test, while the
         plain form is emitted.  The runner's bHasOutput keeps a message when stripping
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
  int raw_nonblank = 0;
  char *m = a5text_describe_ex (run->st, comp, &pre_alr_ink, &raw_nonblank);
  run->st->marking_display = prev_mark;
  if (raw_nonblank)
    run->task_raw_output = 1;
  emit_message_body (run, m, pre_alr_ink, out);
}

/* Append an already-rendered completion message `m` (ownership taken; freed here)
   exactly as the runner Display() does: pSpace-join to the running output, then the
   rendered text verbatim.  `m` is already-plain, so msg_has_output is the faithful
   bHasOutput here: it keeps a whitespace-only message (e.g. a `<Text> </Text>`
   completion), which then space-joins to the next response -- that is how the runner
   renders the leading indent before a search-triggered encounter title (Tingalan's
   Search " " completion -> Execute encounter). */
static void
emit_message_body (a5_run_t *run, char *m, int pre_alr_ink, sb_t *out)
{
  if (msg_has_output (m))
    {
      /* Within an event-fired task chain, dedup identical completion messages
         (the runner's htblResponsesPass, keyed by text): show the first, drop repeats.
         Keeps the first occurrence's position -- which, for a message emitted
         before a later <cls>, means it is wiped by that clear (Pathway's banner). */
      if (run->ev_seen != NULL)
        {
          if (run->ev_seen->count (m)) { free (m); return; }
          run->ev_seen->insert (m);
        }
      /* Per-command pass-response text dedup (the runner's htblResponsesPass keying):
         a command that reaches the same completion text twice -- e.g. two
         Execute'd tasks with identical messages (Euripides' `press on` runs
         cl_ToCrawler11 AND cl_ToCrawler12, both the crawler journey) -- shows
         it once, keeping the first occurrence's position. */
      if (run->exec_scope != NULL
          && !run->exec_scope->pass_seen.insert (m).second)
        { free (m); return; }
      /* A pass response with output: record its bound object references for
         the pass-cancels-fail rule (the runner keys every AddResponse with the
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
         the runner stores the response pre-ALR and only blanks it inside Display(),
         so its markup/newline skeleton still pSpace-joins the output and
         leaves an empty paragraph slot.  Emit the whitespace remainder
         verbatim to keep that slot. */
      sb_pspace (out); sb_puts (out, m);
    }
  free (m);
}

/* Render the room view for the runner's "Execute Look" (on movement / "look").  The
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
  /* The room view IS the CompletionMessage's default (Me(0) ==
     "%Player%.Location.Description").  A StartAfterDefaultDescription segment
     rebuilds from this default (the runner clsDescription.ToString / eval_desc_into),
     discarding any StartDescriptionWithThis override that fired before it -- so
     Magor's fire-lit main chamber/library keep the room view even though the
     Look task's restricted "It is too dark..." override passes (its restrictions
     do pass; the following "Remember ... LUMINO" StartAfterDefault segment resets
     the buffer to default+hint). */
  std::string default_view = result;
  free (view);

  if (comp != NULL)
    {
      int idx = 0;
      for (const a5_xml_node_t *c = comp->first_child; c; c = c->next)
        {
          const a5_xml_node_t *restr;
          const char *when, *raw;
          char *proc, *plain;
          int once;
          if (strcmp (c->name, "Description") != 0)
            continue;
          if (idx++ == 0)
            continue;                          /* the room-view default */
          /* A <DisplayOnce> segment already shown is suppressed (mirrors
             eval_desc_into / clsDescription.ToString: "If Not sd.DisplayOnce
             OrElse Not sd.Displayed").  Without this the Look aggregate's
             first-time hints -- e.g. Book of Jax's "(Don't forget ... use the
             LUMINO spell.)" -- reappeared on every room view where the runner shows them
             once. */
          once = a5xml_bool (a5xml_child_text (c, "DisplayOnce"));
          if (once && a5state_disp_once_seen (st, c))
            continue;
          restr = a5xml_child (c, "Restrictions");
          if (restr != NULL && !a5restr_pass (st, restr))
            continue;
          /* Retire the segment only on real output, not a bTestingOutput peek
             (the two pre/post-action test renders set marking_display=0; the
             final Display render sets it to 1). */
          if (once && st->marking_display)
            a5state_disp_once_mark (st, c);
          when = a5xml_child_text (c, "DisplayWhen");
          raw  = a5xml_child_text (c, "Text");
          proc = a5text_process (st, raw ? raw : "");
          plain = a5text_render_plain (proc);
          free (proc);
          if (streq (when, "StartDescriptionWithThis"))
            result = plain;
          else if (streq (when, "StartAfterDefaultDescription"))
            {                                   /* rebuild from the default view */
              result = default_view;
              if (!result.empty ()) result += "  ";
              result += plain;
            }
          else                                  /* AppendToPreviousDescription */
            { if (!result.empty ()) result += "  "; result += plain; }
          free (plain);
        }
    }
  return result;
}

static void
emit_look (a5_run_t *run, sb_t *out)
{
  /* Defer the room view until the command's After children have run (the runner's
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
      /* Defer the room view to flush (final command state): the runner's stock Look
         CompletionMessage is AggregateOutput, so its render is deferred to
         Display.  Recording a look entry (rather than rendering now) lets a
         command whose After children move things have the view reflect them. */
      resp_entry e;
      e.is_pass = true; e.is_look = true;
      e.item = run->resp->cur_item;
      resp_insert (run->resp, e, -1);
      return;
    }
  /* Real output: retire any <DisplayOnce> completion segments this view shows. */
  std::string result = render_look_marked (run);
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
      /* The runner executes the stock Look like any Before + AggregateOutput task
         (clsUserSession.vb:1164-1207): the message is test-rendered BEFORE and
         AFTER its actions (bTestingOutput=True; each render re-draws any
         <# OneOf #>/RAND in the room view), and when the two renders differ (a
         random pick moved between them) the response is PINNED to the first
         render.  Only when they agree does the raw aggregate message survive to
         be re-rendered once more at final Display -- which is the defer_look /
         is_look deferral emit_look implements.  Doing both test renders here
         keeps the xoshiro draw stream aligned with the runner for random room views
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
          if (!t->aggregate)
            {
              /* A NON-aggregate stock Look (this game's library version --
                 e.g. Murder Most Foul) renders its response text at task
                 time in the runner (AddResponse keys the rendered text), so the view
                 must NOT be deferred to the flush: a LocationTrigger System
                 task that fires after the move (the corridor's +2 servant
                 scene) would otherwise leak its score into the view's SCORE
                 header, which the runner renders pre-award. */
              std::string v = render_look_marked (run);
              resp_add_text (run, v.c_str (), true, resp_pos);
            }
          else
            {
              resp_entry e;
              e.is_pass = true; e.is_look = true;
              e.item = run->resp->cur_item;
              resp_insert (run->resp, e, resp_pos);
            }
        }
      else if (!run->defer_look)
        {
          /* The per-command response dedup applies to the room view too: the runner
             keys the Look task's raw AggregateOutput template, so a command
             that Executes Look twice shows one view (Euripides `press on`).
             Real output: retire any <DisplayOnce> completion segments. */
          std::string v = render_look_marked (run);
          if (run->exec_scope == NULL
              || run->exec_scope->pass_seen.insert (v).second)
            {
              exec_resp_scope *sc = run->exec_scope;
              if (sc != NULL && t->aggregate && sc->look_off >= 0
                  && sc->look_sink == out
                  && (size_t) sc->look_off + sc->look_len <= out->len)
                {
                  /* Second aggregate view this command, DIFFERENT text (the
                     player moved between the Executes): the runner's response map has
                     ONE slot for the Look template, so the later render
                     replaces the earlier view in place (first slot's position,
                     last text). */
                  sb_splice (out, (size_t) sc->look_off, sc->look_len,
                             v.c_str ());
                  sc->look_len = v.size ();
                }
              else
                {
                  sb_pspace (out);
                  if (sc != NULL && t->aggregate)
                    { sc->look_sink = out; sc->look_off = (long) out->len; }
                  sb_puts (out, v.c_str ());
                  if (sc != NULL && t->aggregate)
                    sc->look_len = v.size ();
                }
            }
        }
      else if (!t->aggregate && claimed_pending)
        {
          /* Direct-path deferral (the outer task's Execute Look splice) of a
             NON-aggregate Look: pin the view rendered NOW, at Execute Look
             time, so later actions/system tasks can't retro-date the SCORE
             header (the runner renders the non-aggregate response eagerly). */
          std::string v = render_look_marked (run);
          free (run->look_pinned);
          run->look_pinned = strdup (v.c_str ());
        }
      /* defer_look + unchanged (aggregate): the pending slot claimed above is
         rendered at the enabling frame's final state, exactly as before. */

      if (abuf.len > 0) { sb_pspace (out); sb_puts (out, abuf.p); }
      free (abuf.p);
      return;
    }

  /* On the map path a "Before" message follows the Adrift 5 runner's
     sBeforeActionsMessage logic (clsUserSession.vb:1176-1205): it claims its
     slot in the response order now, is rendered once before the actions, and
     after the actions is either pinned to that pre-action text (if the actions
     changed its rendering -- e.g. TakeObjectsFromOthersLazy's "(from
     %objects%.Parent.Name)", whose parent flips to the player once the take
     runs) or, for an unchanged AggregateOutput task, left deferred so a second
     item merges into its %objects% list. */
  char *resp_pre = NULL;
  int   resp_pos = -1;
  char *flat_raw = NULL;         /* flat-path frozen Before template + snapshot */
  char *flat_pre = NULL;
  int   flat_pre_ink = 0;
  int   flat_inter = 0;
  sb_t  flat_abuf; sb_init (&flat_abuf);
  if (before && comp != NULL)
    {
      if (run->resp != NULL)
        {
          resp_pre = render_comp_test (run->st, comp);
          resp_pos = (int) run->resp->ents.size ();
          /* The runner renders task.CompletionMessage.ToString with bTestingOutput
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
      else if (!comp_bears_function (comp) || run->defer_look)
        {
          /* The runner (FileIO.vb:1618 defaults a missing <MessageBeforeOrAfter> to
             Before) renders a Before completion message up to THREE times
             (clsUserSession.vb:1176-1205): a pre-action snapshot
             `sBeforeActionsMessage`, a post-action compare, and a finalize
             `sMessage = ReplaceExpressions(ReplaceFunctions(sMessage))`.  A message
             bearing a text-changing function (`RAND(..)`, `<#OneOf#>`) draws on
             each render, so the runner draws 3× and shows the finalize when the first two
             renders agree, or draws 2× and pins the first when they differ.
             Scarier's flat resp==NULL buffer emitted once -- desyncing the xoshiro
             stream on e.g. Tingalan's `read book of ancient lore`, whose
             CompletionMessage is `This book contains %booksoflore[RAND(1,25)]%`
             (the runner drew RAND(1,25) 3× and showed array index 3; Scarier drew once and
             showed index 1, diverging every downstream encounter roll).  A static
             completion (no `%function%`) renders identically every time and draws
             nothing, so the two extra renders are inert for the corpus's plain
             messages -- this fast path keeps them byte-stable.  (defer_look also
             stays here: the interleaved path below buffers action output, which
             would break look_pos indexing into `out`.) */
          char *e1 = render_comp_test (run->st, comp);
          char *e2 = render_comp_test (run->st, comp);
          if (e1 != NULL && e2 != NULL && strcmp (e1, e2) != 0)
            {
              /* First two renders differ: the runner pins the pre-action snapshot and the
                 finalize replace is a no-op on the already-plain text (no 3rd
                 draw). */
              free (e2);
              emit_message_body (run, e1, 0, out);
            }
          else
            {
              free (e1);
              free (e2);
              emit_completion (run, comp, out);   /* finalize: 3rd render + display */
            }
        }
      else
        {
          /* The message draws AND this task's actions may draw too (AoK's
             Task999 magpie miss: message = <# Either(...) #>, actions =
             Execute Task998 = Variable206 RAND(1,100)).  Evaluating the
             compare/finalize before the actions (as above) would put the
             Either draws before the roll, while the runner's real order is render1 ->
             actions -> render2 (-> finalize) -- that misordering desynced the
             whole xoshiro stream from the magpie chase onward.  Interleave:
             freeze the template and take the pre-action snapshot now, run the
             actions into a side buffer below (the message still precedes their
             output, vb:1189 iResponsePosition), compare after them.

             The template is frozen via eval_description ONCE, exactly like
             the runner's `sMessage = task.CompletionMessage.ToString` (segment
             selection + DisplayOnce retirement happen HERE, vb:1167 -- real
             render, not bTestingOutput): the pre/post/finalize renders rework
             that string with the function/expression pass only.  Re-selecting
             segments per render instead broke Spectre of Castle Coris: the
             banish task's actions hide the Spectre, so a live re-select
             dropped the Either-bearing segment from the post-action compare
             and its draw vanished from the stream. */
          int pm = run->st->marking_display;
          run->st->marking_display = 1;
          flat_raw = a5text_eval_description (run->st, comp);
          run->st->marking_display = 0;
          flat_pre = a5text_process_frozen (run->st, flat_raw, &flat_pre_ink);
          run->st->marking_display = pm;
          flat_inter = 1;
        }
    }

  /* clsUserSession.vb:1193 sets `task.Completed = True` *before* ExecuteActions
     (vb:1194), so an action that runs or gates on this task's own completion sees
     it complete -- e.g. Anno's Translatin runs `SetTasks Execute SettingOff`, and
     SettingOff requires `Translatin Must BeComplete`.  (Callers also mark it after
     run_task; this earlier mark is the one the runner relies on.) */
  if (self_ti >= 0)
    run->st->task_done[self_ti] = 1;

  /* The runner's stock Look is an AggregateOutput task, so its room view is deferred to
     the command's final Display -- a later action in THIS task's list that moves
     an object out of the room is reflected in the view even though the view is
     positioned ahead of that action's output.  FoF's `give pail to baker` runs
     Task317 (Baker Bakes Bread), whose action list does `Execute Look` and THEN
     `Execute Task2572` (Farrier Takes Pail, hiding the pail); The runner shows the bakery
     WITHOUT the pail followed by the farrier's "picks up the pail" line.  When an
     `Execute Look` is not the last action here, defer it (its slot is recorded at
     look_pos) and splice the final-state view back in after the remaining actions
     have run.  The last-action case (the usual "move then look") is unchanged. */
  int local_defer_look = 0;
  int prev_dl = run->defer_look, prev_lp = run->look_pending;
  size_t prev_lpos = run->look_pos;
  char *prev_lpin = run->look_pinned;
  if (act != NULL && !flat_inter && run->resp == NULL && !run->defer_look)
    {
      for (const a5_xml_node_t *c = act->first_child; c != NULL; c = c->next)
        if (streq (c->name, "SetTasks") && c->next != NULL
            && streq (c->text, "Execute Look"))
          { local_defer_look = 1; break; }
      if (local_defer_look)
        { run->defer_look = 1; run->look_pending = 0; run->look_pinned = NULL; }
    }

  if (act != NULL)
    {
      int saved_sti = run->cur_score_ti;
      run->cur_score_ti = self_ti;
      const a5_xml_node_t *c;
      for (c = act->first_child; c != NULL; c = c->next)
        run_action (run, c->name, c->text, depth, flat_inter ? &flat_abuf : out);
      run->cur_score_ti = saved_sti;
    }

  if (local_defer_look)
    {
      int did_defer = run->look_pending;
      size_t lpos = run->look_pos;
      char *lpin = run->look_pinned;
      run->defer_look = prev_dl;
      run->look_pending = prev_lp;
      run->look_pos = prev_lpos;
      run->look_pinned = prev_lpin;
      if (did_defer && lpos <= (size_t) out->len)
        {
          std::string view;
          if (lpin != NULL)
            view = lpin;                       /* pinned pre-action render */
          else
            view = render_look_marked (run);   /* final-state room view */
          /* Splice the view at its recorded slot: head | view | tail, where tail
             (the output the remaining actions produced) already carries its own
             leading pSpace separator. */
          std::string tail (out->p + lpos, out->len - lpos);
          out->len = lpos;
          if (out->p) out->p[lpos] = '\0';
          sb_pspace (out);
          sb_puts (out, view.c_str ());
          sb_puts (out, tail.c_str ());
        }
      free (lpin);
    }

  if (flat_inter)
    {
      /* Post-action compare + finalize (clsUserSession.vb:1200-1205), on the
         FROZEN template: pin the pre-action snapshot when the renders differ
         (the runner displays the test render; no third draw), else render once more
         for display (the finalize draw, real marking). */
      int pm = run->st->marking_display;
      run->st->marking_display = 0;
      char *post = a5text_process_frozen (run->st, flat_raw, NULL);
      run->st->marking_display = pm;
      if (flat_pre != NULL && post != NULL && strcmp (flat_pre, post) != 0)
        { emit_message_body (run, flat_pre, flat_pre_ink, out); flat_pre = NULL; }
      else
        {
          int fin_ink = 0;
          pm = run->st->marking_display;
          run->st->marking_display = 1;
          char *fin = a5text_process_frozen (run->st, flat_raw, &fin_ink);
          run->st->marking_display = pm;
          emit_message_body (run, fin, fin_ink, out);
        }
      free (post);
    }
  free (flat_pre);
  free (flat_raw);
  if (flat_abuf.len > 0) { sb_pspace (out); sb_puts (out, flat_abuf.p); }
  free (flat_abuf.p);

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
      if (run->resp != NULL)
        resp_add_comp (run, t, comp, true);
      else if (run->comp_defers != NULL && t->aggregate)
        {
          /* AggregateOutput completion on the eager command path: render the
             static skeleton now (so its position and pSpace separators match),
             but hold any random `<#..#>` draw to the end-of-command flush --
             the runner's AggregateOutput responses run ReplaceExpressions at Display,
             after the following actions' draws.  If the message renders empty
             (or is deduped away), roll back any recorded expressions so no
             orphan draw fires at flush. */
          size_t sink0 = run->comp_defers->size ();
          size_t out0 = out->len;
          run->st->expr_defer = run->comp_defers;
          emit_completion (run, comp, out);
          run->st->expr_defer = NULL;
          if (out->len == out0)
            run->comp_defers->resize (sink0);
        }
      else
        emit_completion (run, comp, out);
    }
}

/* The events + walks turn-based runtime (ev_* / wk_*) lives in
   a5run_events.cpp; its ev_init / ev_tick_all / ev_on_task_completed
   entry points are declared in a5run_internal.h. */
