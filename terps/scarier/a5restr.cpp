/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- restriction evaluation.  See a5restr.h.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a5parse.h"
#include "a5rand.h"
#include "a5restr.h"
#include "a5text.h"

int a5restr_trace = 0;

/* CBool-ish (Global.SafeBool): "true"/"false" by name, else a numeric string is
   true iff non-zero; anything else is false. */
static int
safe_bool (const char *s)
{
  char *end;
  double d;
  if (s == NULL) return 0;
  while (*s == ' ') s++;
  if (strcasecmp (s, "true") == 0)  return 1;
  if (strcasecmp (s, "false") == 0) return 0;
  d = strtod (s, &end);
  while (*end == ' ') end++;
  return (end != s && *end == '\0') ? (d != 0.0) : 0;
}

#define ANYOBJECT     "AnyObject"
#define NOOBJECT      "NoObject"
#define ANYCHARACTER  "AnyCharacter"
#define NOCHARACTER   "NoCharacter"
#define PLAYERLOCATION "PlayerLocation"

/* ---------------------------------------------------------- one restriction */

typedef struct {
  const char *type;   /* Object / Location / Character / Variable / Task / ...  */
  char *buf;          /* owned tokenised copy of the spec; key1/op/key2 alias it */
  const char *raw;    /* the original (un-tokenised) spec text -- stable DOM     */
  const char *key1;
  int must_not;
  const char *op;
  const char *key2;   /* may be "" */
} a5_restr_t;

/* Split "key1 Must|MustNot Op key2..." in place.  buf is owned by the restr. */
static void
parse_spec (a5_restr_t *r, const char *spec)
{
  char *p, *tok;
  r->buf = spec ? strdup (spec) : strdup ("");
  r->raw = spec;
  r->key1 = "";
  r->op = "";
  r->key2 = "";
  r->must_not = 0;

  p = r->buf;
  /* key1 */
  while (*p == ' ') p++;
  tok = p;
  while (*p && *p != ' ') p++;
  if (*p) *p++ = '\0';
  r->key1 = tok;
  /* Must / MustNot */
  while (*p == ' ') p++;
  tok = p;
  while (*p && *p != ' ') p++;
  if (*p) *p++ = '\0';
  if (strcmp (tok, "MustNot") == 0)
    r->must_not = 1;
  /* operator */
  while (*p == ' ') p++;
  tok = p;
  while (*p && *p != ' ') p++;
  if (*p) *p++ = '\0';
  r->op = tok;
  /* remainder = key2 (may contain spaces) */
  while (*p == ' ') p++;
  r->key2 = p;
}

/* The type element of a <Restriction> is its first child named a known type. */
static const a5_xml_node_t *
restr_type_node (const a5_xml_node_t *restriction)
{
  static const char *types[] = {
    "Object", "Location", "Character", "Variable", "Task",
    "Item", "Property", "Direction", "Expression", NULL
  };
  const a5_xml_node_t *c;
  for (c = restriction->first_child; c != NULL; c = c->next)
    {
      int i;
      for (i = 0; types[i] != NULL; i++)
        if (strcmp (c->name, types[i]) == 0)
          return c;
    }
  return NULL;
}

/* ------------------------------------------------------------- value helpers */

static int
streq (const char *a, const char *b)
{
  return a != NULL && b != NULL && strcmp (a, b) == 0;
}

static const char *
resolve_key (a5_state_t *st, const char *k)
{
  const char *bound;
  if (streq (k, "%Player%"))
    return a5state_player_key (st);
  /* A per-turn binding (ReferencedObject2, ReferencedDirection, ...) wins. */
  bound = a5state_lookup_ref (st, k);
  /* FD's GetReference("ReferencedObjects") carries NO ReferenceMatch condition
     (clsUserSession.vb:3990): it answers from the first Object-type reference
     in the command whatever its slot, so a restriction keyed "ReferencedObjects"
     also resolves when the command captured a singular %object% (LostLabyrinth's
     "sheath sword" PutObjInSc: `ReferencedObjects Must BeObject Sword`).  Same
     for ReferencedCharacters (vb:4019).  Resolved at lookup (not bound at
     capture) so failed match attempts earlier in the turn can't leak a stale
     alias into a later task's restrictions. */
  if (bound == NULL && streq (k, "ReferencedObjects"))
    bound = a5state_lookup_ref (st, "ReferencedObject");
  if (bound == NULL && streq (k, "ReferencedCharacters"))
    bound = a5state_lookup_ref (st, "ReferencedCharacter");
  if (bound != NULL)
    {
      /* A piped multi-object binding (ReferencedObjects = "k1|k2|...", set by
         resolve_plural so the text engine can render the whole set) resolves,
         for restriction purposes, to its FIRST item -- mirroring FD's
         GetReference("ReferencedObjects"), which returns Items(0).  Per-item
         narrowing already happened in RefineMatchingPossibilitesUsingRestrictions;
         the final PassRestrictions only ever checks the first item. */
      const char *bar = strchr (bound, '|');
      if (bar != NULL)
        {
          char tmp[128];
          size_t n = (size_t) (bar - bound);
          if (n < sizeof tmp)
            {
              memcpy (tmp, bound, n);
              tmp[n] = '\0';
              const a5_object_t *o = a5model_object (st->adv, tmp);
              if (o != NULL) return o->key;   /* stable model-key pointer */
            }
        }
      return bound;
    }
  if (streq (k, "ReferencedCharacter"))
    return a5state_player_key (st);
  return k;
}

/* Evaluate key2 as a number: a variable key's value, a `RAND(min,max)`
   expression, else an integer literal.  An expression RHS is wrapped in single
   quotes in the model (FD's clsRestriction summary quotes it; the stored
   StringValue is the bare expression), and FD's PassSingleRestriction evaluates
   it via EvaluateExpression -> clsVariable.SetToExpression, which calls
   Global.Random(min,max) for a RAND() token (clsUserSession.vb:4486).  So
   SixSilverBullets' TimeTrapsT `Roller Must BeEqualTo 'RAND(1,16)'` draws one
   random per evaluation -- a draw Scarier previously skipped (strtol of the
   quoted string yields 0, so the chime never fired and the RAND stream desynced
   from FD, mistiming the sniper/bomb events). */
static long
num_value (a5_state_t *st, const char *k)
{
  int vi;
  const char *s = k;
  char unq[64];
  if (s != NULL && s[0] == '\'')
    {
      /* strip surrounding single quotes into a small scratch buffer */
      size_t len = strlen (s);
      if (len >= 2 && s[len - 1] == '\'' && len - 2 < sizeof unq)
        { memcpy (unq, s + 1, len - 2); unq[len - 2] = '\0'; s = unq; }
    }
  if (s != NULL && strncmp (s, "RAND", 4) == 0)
    {
      long lo = 0, hi = 0;
      char *end;
      const char *p = s + 4;
      while (*p && !(*p == '-' || (*p >= '0' && *p <= '9'))) p++;
      lo = strtol (p, &end, 10);
      p = end;
      while (*p && !(*p == '-' || (*p >= '0' && *p <= '9'))) p++;
      hi = (*p) ? strtol (p, NULL, 10) : lo;
      return a5rand_between (lo, hi);
    }
  vi = a5state_variable_index (st, s);
  if (vi >= 0)
    return st->var_num[vi];
  return strtol (s != NULL ? s : "0", NULL, 10);
}

static const char *
obj_prop (const a5_object_t *o, const char *key)
{
  const a5_prop_t *p = a5_prop_find (o->props, o->n_props, key);
  return p ? p->value : NULL;
}

/* clsCharacter.IsHoldingObject (clsCharacter.vb:895): an object counts as held
   by `charkey` (or ANYCHARACTER) when it is directly HeldByCharacter, OR is
   inside/on an object that is (recursively) held.  So the silver key inside the
   carried jewelry box is "held" and unlocks the door. */
static int
is_holding_object (a5_state_t *st, int oi, const char *charkey)
{
  if (oi < 0) return 0;
  switch (st->obj[oi].where)
    {
    case A5_OWHERE_HELD_BY:
      return streq (charkey, ANYCHARACTER) || streq (st->obj[oi].key, charkey);
    case A5_OWHERE_IN_OBJECT:
    case A5_OWHERE_ON_OBJECT:
      return is_holding_object (st, a5state_object_index (st, st->obj[oi].key),
                                charkey);
    default:
      return 0;
    }
}

/* ------------------------------------------------------ object sub-evaluator */

/* Does an object "have" a property, mirroring clsObject.HasProperty over the
   merged (static + runtime) property table?  A runtime SetProperty override
   wins (an `<Unselected>` value means the SelectionOnly property was removed,
   per clsUserSession.vb:2058 -- e.g. `SetProperty Flashlight Purchased
   Selected` from a "buy" task); otherwise the model's static prop set.  The
   static layer alone is not enough: SelectionOnly props such as Purchased are
   only ever added at runtime. */
static int
obj_has_property (const a5_state_t *st, const char *okey, const char *propkey)
{
  int i;
  for (i = 0; i < st->n_ov; i++)
    if (streq (st->ov[i].entity, okey) && streq (st->ov[i].prop, propkey))
      return strstr (st->ov[i].value, "Unselected") == NULL;
  for (i = 0; i < st->adv->n_objects; i++)
    if (streq (st->adv->objects[i].key, okey))
      return a5_prop_find (st->adv->objects[i].props,
                           st->adv->objects[i].n_props, propkey) != NULL;
  return 0;
}

static int
pass_object (a5_state_t *st, a5_restr_t *r)
{
  const char *k1 = resolve_key (st, r->key1);
  const char *k2 = resolve_key (st, r->key2);
  int oi = a5state_object_index (st, k1);

  if (streq (r->op, "BeAtLocation"))
    {
      if (streq (k1, NOOBJECT) || streq (k1, ANYOBJECT))
        {
          int i, any = 0;
          for (i = 0; i < st->adv->n_objects; i++)
            if (a5state_object_at_location (st, i, k2, 0)) { any = 1; break; }
          return streq (k1, ANYOBJECT) ? any : !any;
        }
      return a5state_object_at_location (st, oi, k2, 0);
    }
  if (streq (r->op, "BeOnObject") || streq (r->op, "BeInsideObject"))
    {
      a5_owhere_t want = streq (r->op, "BeOnObject")
                           ? A5_OWHERE_ON_OBJECT : A5_OWHERE_IN_OBJECT;
      if (streq (k1, ANYOBJECT) || streq (k1, NOOBJECT))
        {
          int i, any = 0;
          for (i = 0; i < st->adv->n_objects; i++)
            if (st->obj[i].where == want && streq (st->obj[i].key, k2))
              { any = 1; break; }
          return streq (k1, ANYOBJECT) ? any : !any;
        }
      if (oi < 0) return 0;
      if (streq (k2, ANYOBJECT))
        return st->obj[oi].where == want;
      return st->obj[oi].where == want && streq (st->obj[oi].key, k2);
    }
  if (streq (r->op, "BeHeldByCharacter") || streq (r->op, "BeWornByCharacter"))
    {
      a5_owhere_t want = streq (r->op, "BeHeldByCharacter")
                           ? A5_OWHERE_HELD_BY : A5_OWHERE_WORN_BY;
      if (streq (k1, ANYOBJECT) || streq (k1, NOOBJECT))
        {
          int i, any = 0;
          for (i = 0; i < st->adv->n_objects; i++)
            if (st->obj[i].where == want
                && (streq (k2, ANYCHARACTER) || streq (st->obj[i].key, k2)))
              { any = 1; break; }
          return streq (k1, ANYOBJECT) ? any : !any;
        }
      if (oi < 0) return 0;
      /* BeHeldByCharacter recurses through held containers (FD's
         IsHoldingObject); BeWornByCharacter is a direct check. */
      if (want == A5_OWHERE_HELD_BY)
        return is_holding_object (st, oi, k2);
      if (streq (k2, ANYCHARACTER))
        return st->obj[oi].where == want;
      return st->obj[oi].where == want && streq (st->obj[oi].key, k2);
    }
  if (streq (r->op, "BePartOfCharacter"))
    {
      /* clsUserSession.vb:4291: object's where == PartOfCharacter and its
         holder key == k2 (a specific character).  A Hidden/elsewhere object
         fails -- this gates MI/TBN's "handfire winks out" System task. */
      if (oi < 0) return 0;
      return st->obj[oi].where == A5_OWHERE_PART_CHAR && streq (st->obj[oi].key, k2);
    }
  if (streq (r->op, "BePartOfObject"))
    {
      /* clsUserSession.vb:4306: where == PartOfObject; k2 == ANYOBJECT checks
         the where alone, else also the parent object key. */
      if (oi < 0) return 0;
      if (streq (k2, ANYOBJECT))
        return st->obj[oi].where == A5_OWHERE_PART_OBJECT;
      return st->obj[oi].where == A5_OWHERE_PART_OBJECT && streq (st->obj[oi].key, k2);
    }
  if (streq (r->op, "BeHidden"))
    return oi >= 0 && st->obj[oi].where == A5_OWHERE_HIDDEN;
  if (streq (r->op, "Exist"))
    {
      if (streq (k1, NOOBJECT)) return st->adv->n_objects == 0;
      if (streq (k1, ANYOBJECT)) return st->adv->n_objects > 0;
      return oi >= 0;
    }
  if (streq (r->op, "HaveProperty"))
    {
      if (streq (k1, ANYOBJECT) || streq (k1, NOOBJECT))
        {
          int i, any = 0;
          for (i = 0; i < st->adv->n_objects; i++)
            if (obj_has_property (st, st->adv->objects[i].key, k2)) { any = 1; break; }
          return streq (k1, ANYOBJECT) ? any : !any;
        }
      return oi >= 0 && obj_has_property (st, k1, k2);
    }
  if (streq (r->op, "BeInState"))
    {
      int i;
      if (oi < 0) return 0;
      /* Consult the runtime override layer so SetProperty (e.g. OpenStatus) is
         reflected; fall back to the model's static value.  Mirror FD's
         BeInState (clsUserSession.vb:4253): only a StateList property whose
         AppendToProperty is empty counts -- an appended pseudo-state property
         (e.g. LockStatus, which appends "Locked" onto OpenStatus) is skipped,
         so unlocking (OpenStatus -> Closed) clears the "Locked" state even
         though the child LockStatus property still reads "Locked". */
      for (i = 0; i < st->adv->objects[oi].n_props; i++)
        {
          const char *pk = st->adv->objects[oi].props[i].key;
          const a5_propdef_t *pd = a5model_propdef (st->adv, pk);
          if (pd != NULL)
            {
              if (pd->type != NULL && !streq (pd->type, "StateList"))
                continue;
              if (pd->append_to != NULL && pd->append_to[0] != '\0')
                continue;
            }
          const char *val = a5state_entity_prop (st, k1, pk);
          if (streq (val, k2))
            return 1;
        }
      return 0;
    }
  if (streq (r->op, "BeInGroup"))
    return a5state_object_in_group (st, k2, k1);
  if (streq (r->op, "BeExactText"))
    {
      /* True if the player's typed reference text equals k2 (e.g. "All"). */
      char alias[64];
      const char *typed;
      snprintf (alias, sizeof alias, "%s$text", r->key1);
      typed = a5state_lookup_ref (st, alias);
      if (typed == NULL)
        return 0;
      while (*typed && *k2 && tolower ((unsigned char) *typed) == tolower ((unsigned char) *k2))
        { typed++; k2++; }
      return *typed == '\0' && *k2 == '\0';
    }
  if (streq (r->op, "BeObject"))
    return streq (k1, k2);
  if (streq (r->op, "BeVisibleToCharacter"))
    {
      /* FD's CanSeeObject (clsCharacter.vb:772) compares BoundVisible keys, so
         an object inside a closed opaque container is not visible. */
      int ci = a5state_character_index (st, k2);
      const char *cloc = (ci >= 0) ? st->char_loc[ci] : NULL;
      return oi >= 0 && cloc != NULL
             && a5state_object_visible_at_location (st, oi, cloc, 0);
    }
  if (streq (r->op, "HaveBeenSeenByCharacter"))
    {
      /* Player-centric seen set (clsCharacter.HasSeenObject); a non-player
         observer falls back to "seen" so its tasks aren't over-suppressed. */
      if (k2 != NULL && !streq (k2, a5state_player_key (st)))
        return 1;
      return oi >= 0 && st->obj_seen != NULL && st->obj_seen[oi];
    }
  return 1;                   /* unknown operator: don't suppress text */
}

/* --------------------------------------------------- location sub-evaluator */

static int
pass_location (a5_state_t *st, a5_restr_t *r)
{
  const char *k1 = r->key1;
  const char *here = a5state_player_location (st);
  const char *loc = streq (k1, PLAYERLOCATION) ? here : k1;

  if (streq (r->op, "BeLocation"))
    return streq (loc, r->key2);
  if (streq (r->op, "BeInGroup"))
    {
      int i;
      for (i = 0; i < st->adv->n_groups; i++)
        if (streq (st->adv->groups[i].key, r->key2))
          {
            int m;
            for (m = 0; m < st->adv->groups[i].n_members; m++)
              if (streq (st->adv->groups[i].members[m], loc))
                return 1;
            return 0;
          }
      return 0;
    }
  if (streq (r->op, "HaveProperty"))
    {
      const a5_location_t *l;
      if (loc == NULL) return 0;
      l = a5model_location (st->adv, loc);
      return l != NULL && a5_prop_find (l->props, l->n_props, r->key2) != NULL;
    }
  if (streq (r->op, "Exist"))
    return 1;
  if (streq (r->op, "HaveBeenSeenByCharacter"))
    {
      /* Player-centric seen set (clsCharacter.HasSeenLocation, set on every
         player move + the start location); a non-player observer falls back
         to "seen" -- the same compromise as the object handler above. */
      const char *ch = resolve_key (st, r->key2);
      int li;
      if (ch != NULL && ch[0] != '\0' && !streq (ch, a5state_player_key (st)))
        return 1;
      li = a5state_location_index (st, loc);
      return li >= 0 && st->loc_seen != NULL && st->loc_seen[li];
    }
  return 1;
}

/* ------------------------------------------------------------ exit lookup */

const char *
a5restr_exit_in_direction (a5_state_t *st, const char *lockey, const char *dir,
                           const a5_xml_node_t **blocked_msg)
{
  const a5_location_t *l;
  const a5_xml_node_t *c;
  if (lockey == NULL || dir == NULL)
    return NULL;
  l = a5model_location (st->adv, lockey);
  if (l == NULL)
    return NULL;
  for (c = l->node->first_child; c != NULL; c = c->next)
    {
      const char *d, *dest;
      const a5_xml_node_t *mr;
      if (strcmp (c->name, "Movement") != 0)
        continue;
      d = a5xml_child_text (c, "Direction");
      if (!streq (d, dir))
        continue;
      mr = a5xml_child (c, "Restrictions");
      if (mr != NULL && !a5restr_pass (st, mr))
        {                               /* route blocked by its restriction */
          if (blocked_msg != NULL)
            *blocked_msg = a5restr_fail_message (st, mr);
          continue;
        }
      dest = a5xml_child_text (c, "Destination");
      return (dest != NULL && dest[0] != '\0') ? dest : NULL;
    }
  return NULL;
}

/* -------------------------------------------------- character sub-evaluator */

/* AloneWithChar (clsCharacter.AloneWithChar): the single other character in the
   same location as `charkey`, or NULL if there are zero or more than one. */
static const char *
alone_with_char (a5_state_t *st, const char *charkey)
{
  int ci = a5state_character_index (st, charkey);
  const char *cloc = (ci >= 0) ? st->char_loc[ci] : NULL;
  const char *found = NULL;
  int count = 0, i;
  if (cloc == NULL)
    return NULL;
  for (i = 0; i < st->adv->n_characters; i++)
    {
      if (i == ci)
        continue;
      if (streq (st->char_loc[i], cloc))
        { count++; found = st->adv->characters[i].key; }
      if (count > 1)
        return NULL;
    }
  return (count == 1) ? found : NULL;
}

/* Does character `charkey` hold (where==HELD_BY) or wear (WORN_BY) any object? */
static int
char_holds_any (a5_state_t *st, const char *charkey, a5_owhere_t where)
{
  int i;
  for (i = 0; i < st->adv->n_objects; i++)
    if (st->obj[i].where == where && streq (st->obj[i].key, charkey))
      return 1;
  return 0;
}

/* clsCharacter.IsHoldingObject(sObKey): true if CHARKEY holds OBJKEY, counting
   objects nested inside containers/on supporters the character (transitively)
   holds -- InObject/OnObject recurse on the parent, HeldByCharacter terminates
   (clsCharacter.vb:895).  WornByCharacter does NOT count as held, so the
   recursion stops (returns 0) at a worn parent.  bDirectly disables the recursion
   (the directly-held-only check). */
static int
char_holds_object (a5_state_t *st, const char *charkey, const char *objkey,
                   int directly)
{
  int oi = a5state_object_index (st, objkey);
  if (oi < 0)
    return 0;
  a5_owhere_t w = st->obj[oi].where;
  const char *k = st->obj[oi].key;
  if (w == A5_OWHERE_HELD_BY)
    return k != NULL && (streq (k, charkey)
                         || (streq (k, "%Player%")
                             && streq (charkey, a5state_player_key (st))));
  if (!directly && (w == A5_OWHERE_IN_OBJECT || w == A5_OWHERE_ON_OBJECT))
    return k != NULL && char_holds_object (st, charkey, k, 0);
  return 0;
}

/* Does a character "have" a property, mirroring clsCharacter.HasProperty over
   the merged (static + runtime) property table?  A runtime SetProperty override
   wins (an `<Unselected>` value means the SelectionOnly property was removed,
   per clsUserSession.vb:2058); otherwise the model's static prop set. */
static int
char_has_property (const a5_state_t *st, const char *ckey, const char *propkey)
{
  int i;
  for (i = 0; i < st->n_ov; i++)
    if (streq (st->ov[i].entity, ckey) && streq (st->ov[i].prop, propkey))
      return strstr (st->ov[i].value, "Unselected") == NULL;
  const a5_character_t *c = a5model_character (st->adv, ckey);
  return c != NULL && a5_prop_find (c->props, c->n_props, propkey) != NULL;
}

static int
pass_character (a5_state_t *st, a5_restr_t *r)
{
  const char *k1 = resolve_key (st, r->key1);
  const char *k2 = resolve_key (st, r->key2);
  int ci = a5state_character_index (st, k1);
  const char *cloc = (ci >= 0) ? st->char_loc[ci] : NULL;

  if (streq (r->op, "BeAtLocation"))
    return streq (cloc, k2);
  if (streq (r->op, "BeOfGender"))
    {
      /* clsCharacter.Gender / clsRestriction.CharacterEnum.BeOfGender: an
         exact match against the Mandatory StateList "Gender" property
         (Male/Female/Unknown).  k2 is the literal state name (resolve_key is
         a no-op on it -- no leading '%' and no matching per-turn ref
         binding).  Without this case the operator fell through to the
         best-effort `return 1`, so a gender-gated branch (e.g. Pathway to
         Destruction's
         `Player Must BeOfGender Female` swapping in an alternate character
         name/pronoun set) always fired even when the player's Gender is still
         its unset "Unknown" default -- headless runs never see the
         PopUpChoice gender prompt that would otherwise set it. */
      const char *g = a5state_entity_prop (st, k1, "Gender");
      return g != NULL && strcasecmp (g, k2) == 0;
    }
  if (streq (r->op, "BeHoldingObject"))
    {
      /* ANYOBJECT -> holding at least one object (clsCharacter.IsHoldingObject:
         "If sObKey = ANYOBJECT Then Return HeldObjects.Count > 0"). */
      if (streq (k2, ANYOBJECT))
        return char_holds_any (st, k1, A5_OWHERE_HELD_BY);
      /* A specific object counts as held if it is directly held OR nested in a
         container/supporter the character holds (clsCharacter.IsHoldingObject is
         recursive) -- e.g. Anno's gunpowder poured into the held skull still
         satisfies "Player Must BeHoldingObject Gunpowder1". */
      return char_holds_object (st, k1, k2, 0);
    }
  if (streq (r->op, "BeWearingObject"))
    {
      if (streq (k2, ANYOBJECT))     /* WornObjects.Count > 0 */
        return char_holds_any (st, k1, A5_OWHERE_WORN_BY);
      int oi = a5state_object_index (st, k2);
      return oi >= 0 && st->obj[oi].where == A5_OWHERE_WORN_BY
             && streq (st->obj[oi].key, k1);
    }
  if (streq (r->op, "BeInSameLocationAsObject"))
    {
      int oi;
      if (cloc == NULL)
        return 0;
      /* k2 may be an object GROUP (e.g. LightSources): true if any member is in
         scope, mirroring clsCharacter.CanSeeObject's group expansion (the
         darkness override's "MustNot BeInSameLocationAsObject LightSources"
         must see a held torch). */
      for (int gi = 0; gi < st->adv->n_groups; gi++)
        if (streq (st->adv->groups[gi].key, k2))
          {
            /* Effective membership = static <Member>s plus runtime adds minus
               removes; scan every object so runtime-added members (e.g. a lit
               flashlight in LightSources) count too. */
            for (int oi = 0; oi < st->adv->n_objects; oi++)
              if (a5state_object_in_group (st, k2, st->adv->objects[oi].key)
                  && a5state_object_visible_at_location (st, oi, cloc, 0))
                return 1;
            return 0;
          }
      oi = a5state_object_index (st, k2);
      return oi >= 0 && a5state_object_visible_at_location (st, oi, cloc, 0);
    }
  if (streq (r->op, "BeInSameLocationAsCharacter"))
    {
      /* clsUserSession.vb:4681-4698.  clsCharacter.Location.LocationKey resolves a
         seated/carried character to its room, so the comparison must walk an
         on/in-object character (e.g. Anne seated on Chair1) through its carrier
         rather than read the raw char_loc (NULL for a non-"At Location"
         character).  `same_room` does that resolution for either operand (prefer
         the recorded room, else scan locations for the on/in-object case). */
      auto same_room = [&] (int a, int b) -> int {
        if (a < 0 || b < 0)
          return 0;
        const char *la = (st->char_loc != NULL) ? st->char_loc[a] : NULL;
        if (la != NULL)
          return a5state_character_at_location (st, b, la);
        for (int L = 0; L < st->adv->n_locations; L++)
          { const char *lk = st->adv->locations[L].key;
            if (a5state_character_at_location (st, a, lk)
                && a5state_character_at_location (st, b, lk))
              return 1; }
        return 0;
      };
      int c2 = a5state_character_index (st, k2);
      /* k1 = ANYCHARACTER: r = Not htblCharacters(k2).IsAlone -- some *other*
         character shares k2's room (vb:4684). */
      if (streq (k1, ANYCHARACTER))
        {
          if (c2 < 0)
            return 0;
          for (int s = 0; s < st->adv->n_characters; s++)
            if (s != c2 && same_room (s, c2))
              return 1;
          return 0;
        }
      /* k1 = specific, k2 = ANYCHARACTER: any *other* character co-located with
         k1 (vb:4687-4694) -- e.g. Revenge's `Player Must
         BeInSameLocationAsCharacter AnyCharacter` ("There's nobody here to show
         anything to!"), true when the customs official is present. */
      if (streq (k2, ANYCHARACTER))
        {
          if (ci < 0)
            return 0;
          for (int c = 0; c < st->adv->n_characters; c++)
            if (c != ci && same_room (c, ci))
              return 1;
          return 0;
        }
      /* k1, k2 both specific: identical rooms (vb:4696). */
      return same_room (ci, c2);
    }
  if (streq (r->op, "BeCharacter"))
    {
      /* clsRestriction.CharacterEnum.BeCharacter (clsUserSession.vb:4579): the
         subject character k1 is a specific character (ANYCHARACTER = any) -- an
         identity test on the resolved keys (mirrors the object `BeObject`).  Without
         this case the operator fell through to the best-effort `return 1`, so e.g.
         RunBronwynn's `ExamineCha1` ("Examine Me", `ReferencedCharacter Must
         BeCharacter Player`) wrongly fired for any examined character (x horse ->
         Fleetwind), showing its description where FD's higher-priority
         ExamineCharacter fails the HaveSeen check with "You see no such thing." */
      if (streq (k1, ANYCHARACTER))
        return 1;
      return streq (k1, k2);
    }
  if (streq (r->op, "BeVisibleToCharacter"))
    {
      /* clsRestriction.CharacterEnum.BeVisibleToCharacter (clsUserSession.vb:4700):
         true when the subject character k1 (ANYCHARACTER = any) can be seen by the
         observer k2 -- clsCharacter.CanSeeCharacter -> IsVisibleTo: the two share a
         BoundVisible location and the subject is not Hidden.  Reduce to "the subject
         is at the observer's location" (a5state_character_at_location resolves an
         on/in-object subject through its carrier and treats Hidden as not-present).
         Without this case the operator fell through to the best-effort `return 1`,
         so e.g. Grandpa's `vnl_JustTalk` ("talk" near Molly) wrongly fired when
         Molly was elsewhere, swallowing the turn. */
      const char *obs_loc;
      if (k2 == NULL || streq (k2, a5state_player_key (st)))
        obs_loc = a5state_player_location (st);
      else if (streq (k2, ANYCHARACTER))
        obs_loc = NULL;
      else
        { int oc = a5state_character_index (st, k2);
          obs_loc = (oc >= 0) ? st->char_loc[oc] : NULL; }
      if (k2 != NULL && streq (k2, ANYCHARACTER))
        {
          /* any observer: the subject (or any subject) co-located with any *other*
             character. */
          for (int s = 0; s < st->adv->n_characters; s++)
            {
              if (!streq (k1, ANYCHARACTER) && !streq (st->adv->characters[s].key, k1))
                continue;
              for (int o = 0; o < st->adv->n_characters; o++)
                if (o != s && st->char_loc[o] != NULL
                    && a5state_character_at_location (st, s, st->char_loc[o]))
                  return 1;
            }
          return 0;
        }
      if (obs_loc == NULL)
        return 0;
      if (streq (k1, ANYCHARACTER))
        {
          for (int s = 0; s < st->adv->n_characters; s++)
            if (a5state_character_at_location (st, s, obs_loc))
              return 1;
          return 0;
        }
      return a5state_character_at_location (st, ci, obs_loc);
    }
  if (streq (r->op, "BeWithinLocationGroup"))
    {
      /* Membership must consult the runtime override layer, not just the static
         model members: SixSilverBullets' TimeTraps1 fires the bell then runs
         `RemoveLocationFromGroup LocationOf %Player% FromGroup TimeTraps`, so a
         once-tolled location leaves the group and must not re-toll.  Likewise
         AddLocationToGroup (Jacaranda's dark-locations) must count. */
      return cloc != NULL && a5state_object_in_group (st, k2, cloc);
    }
  if (streq (r->op, "HaveProperty"))
    {
      /* clsCharacter.HasProperty (clsItem.vb:386) consults the *merged*
         property table, so a property added at runtime via SetProperty
         (e.g. `SetProperty Susan Known <Selected>`, clsUserSession.vb:2042)
         must count -- not just the static model props.  FD's
         SetPropertyValue(Boolean) removes a SelectionOnly property when set
         to <Unselected>, so an override carrying that sentinel reads as
         absent (mirroring the display-name "Selected" check at a5text.cpp). */
      if (streq (k1, ANYCHARACTER))
        {
          for (int s = 0; s < st->adv->n_characters; s++)
            if (char_has_property (st, st->adv->characters[s].key, k2))
              return 1;
          return 0;
        }
      return ci >= 0 && char_has_property (st, k1, k2);
    }
  if (streq (r->op, "Exist"))
    return ci >= 0;
  if (streq (r->op, "HaveRouteInDirection"))
    {
      const char *dir = a5parse_canonical_direction (k2);
      const char *cl = streq (k1, a5state_player_key (st))
                         ? a5state_player_location (st) : cloc;
      const a5_xml_node_t *blocked = NULL;
      const char *exit;
      if (dir == NULL)
        dir = k2;             /* already canonical (e.g. bound ReferencedDirection) */
      /* sRouteError: clear, then capture the blocked exit's own message (if the
         exit exists but is restriction-gated) so the deciding fail message
         override below can prefer it over the generic "no route" text. */
      st->route_error = NULL;
      exit = (cl != NULL) ? a5restr_exit_in_direction (st, cl, dir, &blocked) : NULL;
      if (exit == NULL && blocked != NULL)
        st->route_error = blocked;
      return exit != NULL;
    }
  if (streq (r->op, "BeInPosition"))
    {
      const char *pos = (ci >= 0 && st->char_position) ? st->char_position[ci] : NULL;
      return streq (pos, k2);
    }
  /* On/in object, optionally constrained by stance.  Mirrors clsUserSession's
     BeOnObject / BeInsideObject / Be{Standing,Sitting,Lying}OnObject cases:
     ANYOBJECT -> just the ExistWhere; THEFLOOR / else -> object-key compare. */
  if (streq (r->op, "BeOnObject") || streq (r->op, "BeInsideObject")
      || streq (r->op, "BeStandingOnObject") || streq (r->op, "BeSittingOnObject")
      || streq (r->op, "BeLyingOnObject"))
    {
      const char *onobj = (ci >= 0 && st->char_onobj) ? st->char_onobj[ci] : NULL;
      int inside = (ci >= 0 && st->char_in) ? st->char_in[ci] : 0;
      const char *want_pos =
          streq (r->op, "BeStandingOnObject") ? "Standing"
        : streq (r->op, "BeSittingOnObject")  ? "Sitting"
        : streq (r->op, "BeLyingOnObject")    ? "Lying" : NULL;
      int want_in = streq (r->op, "BeInsideObject");
      const char *pos = (ci >= 0 && st->char_position) ? st->char_position[ci] : NULL;
      if (ci < 0) return 0;
      if (want_pos != NULL && !streq (pos, want_pos))
        return 0;
      if (streq (k2, ANYOBJECT))
        return onobj != NULL && (inside ? 1 : 0) == want_in;
      /* THEFLOOR and any concrete object key fall here: must be on/in that key. */
      return onobj != NULL && (inside ? 1 : 0) == want_in && streq (onobj, k2);
    }
  /* Conversation / acquaintance operators (clsUserSession character cases). */
  if (streq (r->op, "BeInConversationWith"))
    {
      if (streq (k2, ANYCHARACTER))
        return st->conv_char != NULL && st->conv_char[0] != '\0';
      return streq (st->conv_char, k2);
    }
  if (streq (r->op, "HaveSeenCharacter"))
    {
      /* key1 is the observer (typically Player), key2 the target.  We track the
         player's "seen" set; a non-player observer is treated as having seen
         (best-effort, no shipped restriction needs the general case). */
      int c2 = a5state_character_index (st, k2);
      if (c2 < 0)
        return 0;
      if (!streq (k1, a5state_player_key (st)))
        return 1;
      return st->char_seen != NULL && st->char_seen[c2];
    }
  if (streq (r->op, "BeAloneWith"))
    {
      const char *aw = alone_with_char (st, k1);
      if (streq (k2, ANYCHARACTER))
        return aw != NULL;
      return streq (aw, k2);
    }
  if (streq (r->op, "BeAlone"))
    {
      int i;
      if (cloc == NULL)
        return 1;
      for (i = 0; i < st->adv->n_characters; i++)
        if (i != ci && streq (st->char_loc[i], cloc))
          return 0;
      return 1;
    }
  if (streq (r->op, "BeOnCharacter"))
    {
      /* clsCharacterLocation.ExistsWhereEnum.OnCharacter: the character is
         sitting/standing on *another character*.  char_onobj holds whatever the
         character is on; it names a character here (vs an object). */
      const char *onk = (ci >= 0 && st->char_onobj) ? st->char_onobj[ci] : NULL;
      if (onk == NULL || a5state_character_index (st, onk) < 0)
        return 0;
      if (streq (k2, ANYCHARACTER))
        return 1;
      return streq (onk, k2);
    }
  return 1;                   /* other character ops: best-effort pass */
}

/* --------------------------------------------------- variable sub-evaluator */

/* A restriction's comparison value as a string (FileIO.LoadRestrictions +
   EvaluateExpression): a quoted literal is de-quoted then expression-evaluated
   (so `"" the ""` -> `" the "` -> ` the `); a bare token naming a variable
   yields that variable's value; otherwise the token is expression-evaluated. */
static char *
restr_text_value (a5_state_t *st, const char *key2)
{
  size_t n;
  if (key2 == NULL)
    return strdup ("");
  n = strlen (key2);
  if (n >= 2 && ((key2[0] == '"' && key2[n - 1] == '"')
              || (key2[0] == '\'' && key2[n - 1] == '\'')))
    {
      char *inner = strndup (key2 + 1, n - 2);
      char *v = a5text_eval_expression (st, inner);
      free (inner);
      return v;
    }
  {
    int vj = a5state_variable_index (st, key2);
    if (vj >= 0 && st->var_text[vj] != NULL)
      return strdup (st->var_text[vj]);
  }
  return a5text_eval_expression (st, key2);
}

static int
str_compare_op (const char *op, const char *cur, const char *rhs)
{
  int cmp = strcmp (cur, rhs);
  if (streq (op, "BeEqualTo"))               return cmp == 0;
  if (streq (op, "BeGreaterThan"))           return cmp > 0;
  if (streq (op, "BeGreaterThanOrEqualTo"))  return cmp >= 0;
  if (streq (op, "BeLessThan"))              return cmp < 0;
  if (streq (op, "BeLessThanOrEqualTo"))     return cmp <= 0;
  if (streq (op, "BeContain"))               return strstr (cur, rhs) != NULL;
  return 1;
}

static int
pass_variable (a5_state_t *st, a5_restr_t *r)
{
  int vi;
  const a5_variable_t *v;

  /* ReferencedText[n] / ReferencedNumber[n]: the parser-matched text/number,
     not a user variable (clsUserSession.vb:4459-4470).  These are the values
     the stock library tasks test against %text%/%number%. */
  if (strncmp (r->key1, "ReferencedText", 14) == 0)
    {
      const char *cur = a5state_lookup_ref (st, r->key1);
      char *rhs;
      int res;
      if (cur == NULL) cur = a5state_lookup_ref (st, "ReferencedText");
      if (cur == NULL) cur = "";
      rhs = restr_text_value (st, r->key2);
      res = str_compare_op (r->op, cur, rhs);
      free (rhs);
      return res;
    }
  if (strncmp (r->key1, "ReferencedNumber", 16) == 0)
    {
      const char *cur_s = a5state_lookup_ref (st, r->key1);
      char *rhs_s;
      long cur, rhs;
      if (cur_s == NULL) cur_s = a5state_lookup_ref (st, "ReferencedNumber");
      cur = cur_s ? strtol (cur_s, NULL, 10) : 0;
      rhs_s = restr_text_value (st, r->key2);
      rhs = strtol (rhs_s, NULL, 10);
      free (rhs_s);
      if (streq (r->op, "BeEqualTo"))               return cur == rhs;
      if (streq (r->op, "BeGreaterThan"))           return cur > rhs;
      if (streq (r->op, "BeGreaterThanOrEqualTo"))  return cur >= rhs;
      if (streq (r->op, "BeLessThan"))              return cur < rhs;
      if (streq (r->op, "BeLessThanOrEqualTo"))     return cur <= rhs;
      return 1;
    }

  vi = a5state_variable_index (st, r->key1);
  if (vi < 0)
    return 1;
  v = &st->adv->variables[vi];

  if (streq (v->type, "Text"))
    {
      const char *cur = st->var_text[vi] ? st->var_text[vi] : "";
      int vj = a5state_variable_index (st, r->key2);
      const char *rhs = (vj >= 0 && st->var_text[vj]) ? st->var_text[vj] : r->key2;
      int cmp = strcmp (cur, rhs);
      if (streq (r->op, "BeEqualTo"))            return cmp == 0;
      if (streq (r->op, "BeGreaterThan"))        return cmp > 0;
      if (streq (r->op, "BeGreaterThanOrEqualTo")) return cmp >= 0;
      if (streq (r->op, "BeLessThan"))           return cmp < 0;
      if (streq (r->op, "BeLessThanOrEqualTo"))  return cmp <= 0;
      if (streq (r->op, "BeContain"))            return strstr (cur, rhs) != NULL;
      return 1;
    }
  else
    {
      long cur = st->var_num[vi];
      long rhs = num_value (st, r->key2);
      if (streq (r->op, "BeEqualTo"))            return cur == rhs;
      if (streq (r->op, "BeGreaterThan"))        return cur > rhs;
      if (streq (r->op, "BeGreaterThanOrEqualTo")) return cur >= rhs;
      if (streq (r->op, "BeLessThan"))           return cur < rhs;
      if (streq (r->op, "BeLessThanOrEqualTo"))  return cur <= rhs;
      return 1;
    }
}

/* --------------------------------------------------- property sub-evaluator */

static int
is_clean_int (const char *s)
{
  if (s == NULL || *s == '\0') return 0;
  if (*s == '-' || *s == '+') s++;
  if (*s == '\0') return 0;
  for (; *s; s++)
    if (!isdigit ((unsigned char) *s)) return 0;
  return 1;
}

/*
 * Port of clsUserSession.PassSingleRestriction's Property case (clsUserSession
 * .vb:5060).  The spec is "<propKey> <itemRef> Must|MustNot <Op> <value>" -- it
 * carries an extra item token that parse_spec (built for the one-key Object/
 * Character layout) cannot represent, so re-tokenise the raw text here.
 *
 * An item lacking the property fails the test (bItemContainsProperty=False).
 * EqualTo compares the (override-aware) property value against the resolved
 * value as a string -- covering StaticOrDynamic / OpenStatus / state lists /
 * integer flags / object-key properties.  The numeric inequality operators are
 * evaluated only when the value is a plain integer; an expression value
 * (%Player%-relative weight/bulk sums) is not yet computed, so the restriction
 * passes (the lenient default, matching the previous always-pass stub).
 */
static int
pass_property (a5_state_t *st, a5_restr_t *r)
{
  char *buf = strdup (r->raw ? r->raw : "");
  char *p = buf, *propkey, *item, *musttok, *op, *value;
  int must_not, rr;
  const char *itemkey, *pv;

#define NEXT_TOK(dst)                                                          \
  do { while (*p == ' ') p++; (dst) = p;                                       \
       while (*p && *p != ' ') p++; if (*p) *p++ = '\0'; } while (0)
  NEXT_TOK (propkey);
  NEXT_TOK (item);
  NEXT_TOK (musttok);
  NEXT_TOK (op);
  while (*p == ' ') p++;
  value = p;
#undef NEXT_TOK

  must_not = (strcmp (musttok, "MustNot") == 0);
  itemkey = resolve_key (st, item);

  if (strcmp (op, "EqualTo") == 0)
    {
      const char *rhs = resolve_key (st, value);
      pv = a5state_entity_prop (st, itemkey, propkey);
      if (pv == NULL && strcmp (propkey, "StaticOrDynamic") == 0)
        pv = "Static";          /* the StaticOrDynamic default for an object */
      rr = (pv != NULL) && streq (pv, rhs);
    }
  else if (is_clean_int (value))
    {
      long lv, rv = strtol (value, NULL, 10);
      pv = a5state_entity_prop (st, itemkey, propkey);
      lv = pv ? strtol (pv, NULL, 10) : 0;
      if (strcmp (op, "GreaterThanOrEqualTo") == 0)      rr = lv >= rv;
      else if (strcmp (op, "GreaterThan") == 0)          rr = lv > rv;
      else if (strcmp (op, "LessThanOrEqualTo") == 0)    rr = lv <= rv;
      else if (strcmp (op, "LessThan") == 0)             rr = lv < rv;
      else                                               rr = 1;
    }
  else
    rr = 1;                     /* expression value not yet evaluated: pass */

  if (a5restr_trace)
    fprintf (stderr, "    [restr Property: %s %s %s%s %s] -> %d\n", propkey,
             item, must_not ? "MustNot " : "", op, value, must_not ? !rr : rr);
  free (buf);
  return must_not ? !rr : rr;
}

/* ---------------------------------------------------------- single + block */

static int
pass_single (a5_state_t *st, a5_restr_t *r)
{
  int result;
  if (streq (r->type, "Object"))         result = pass_object (st, r);
  else if (streq (r->type, "Location"))  result = pass_location (st, r);
  else if (streq (r->type, "Character")) result = pass_character (st, r);
  else if (streq (r->type, "Variable"))  result = pass_variable (st, r);
  else if (streq (r->type, "Property"))  return pass_property (st, r);
  else if (streq (r->type, "Task"))
    {
      int ti = a5state_task_index (st, resolve_key (st, r->key1));
      result = ti >= 0 && st->task_done[ti];
    }
  else if (streq (r->type, "Expression"))
    {
      /* clsUserSession.vb:5165: r = SafeBool(EvaluateExpression(StringValue)).
         The whole element text is the expression (parse_spec's key1/op/key2
         split is meaningless here -- use the un-tokenised raw). */
      char *v = a5text_eval_expression (st, r->raw);
      result = safe_bool (v);
      free (v);
    }
  else if (streq (r->type, "Direction"))
    {
      /* clsUserSession.vb:5161: r = (sKey1 == ReferencedDirection).
         The element text is "<Must|MustNot> Be<Dir>" (FileIO.vb:611-615:
         eMust=token0, sKey1=token1 with the leading "Be" trimmed).  parse_spec's
         generic key1/op split mishandles this, so re-tokenise r->raw here and
         return directly (the trailing must_not negation below would double-flip
         since parse_spec couldn't see the "MustNot"). */
      char tok0[32] = "", tok1[32] = "";
      const char *raw = r->raw ? r->raw : "";
      const char *want, *cur;
      int mnot, rr;
      sscanf (raw, "%31s %31s", tok0, tok1);
      mnot = (strcmp (tok0, "MustNot") == 0);
      want = (strncmp (tok1, "Be", 2) == 0) ? tok1 + 2 : tok1;
      cur = a5state_lookup_ref (st, "ReferencedDirection");
      rr = (cur != NULL && strcmp (want, cur) == 0);
      result = mnot ? !rr : rr;
      if (a5restr_trace)
        fprintf (stderr, "    [restr Direction: %s%s vs %s] -> %d\n",
                 mnot ? "MustNot " : "Must ", want, cur ? cur : "(none)",
                 result);
      return result;
    }
  else
    result = 1;               /* Item: Phase 3-4 */

  if (r->must_not)
    result = !result;
  if (a5restr_trace)
    fprintf (stderr, "    [restr %s: %s %s%s %s] -> %d\n", r->type, r->key1,
             r->must_not ? "MustNot " : "", r->op, r->key2, result);
  return result;
}

/*
 * Ported EvaluateRestrictionBlock (clsUserSession.vb:5190): consume '#' tokens
 * from `block` against rs[0..n) left-to-right, AND binding tighter than OR.
 * *idx tracks the next restriction; advances past short-circuited #'s.
 */
static int count_hashes (const char *s)
{
  int n = 0;
  for (; *s; s++) if (*s == '#') n++;
  return n;
}

/* Normalise "A#O" -> "(...A#)O..." so AND binds before OR (as frankendrift). */
static char *
normalise_block (const char *in)
{
  char *s = strdup (in);
  char *hit;
  while ((hit = strstr (s, "A#O")) != NULL)
    {
      int i = (int) (hit - s);
      /* find last '(' before i, insert '(' after it (or at start) */
      int j = -1, k;
      for (k = 0; k < i; k++)
        if (s[k] == '(') j = k;
      j += 1;
      /* build: left[0,j) + "(" + mid[j,i) + "A#)O" + right[i+3..] */
      {
        int len = (int) strlen (s);
        char *out = (char *) malloc ((size_t) len + 8);
        int p = 0, q;
        for (q = 0; q < j; q++) out[p++] = s[q];
        out[p++] = '(';
        for (q = j; q < i; q++) out[p++] = s[q];
        out[p++] = 'A'; out[p++] = '#'; out[p++] = ')'; out[p++] = 'O';
        for (q = i + 3; q < len; q++) out[p++] = s[q];
        out[p] = '\0';
        free (s);
        s = out;
      }
    }
  return s;
}

/*
 * Port of EvaluateRestrictionBlock: normalise at entry, evaluate the leading
 * term (a bracketed sub-block or a single '#'), then recurse on the remainder.
 * AND short-circuits false, OR short-circuits true; *idx advances over every
 * '#' actually consumed (including short-circuited ones).
 *
 * `last_fail` mirrors frankendrift's sRestrictionText side effect: every single
 * restriction actually evaluated clears it on a pass and sets it to its own
 * index on a fail, so after the (short-circuiting) walk it names the failing
 * restriction on the deciding path -- the one whose Message the Runner shows.
 * Short-circuited '#'s are skipped (no PassSingleRestriction call), so they do
 * not touch it.  `nodes[i]` is the XML <Restriction> for rs[i].
 */
static int
eval_block (a5_state_t *st, a5_restr_t *rs, const a5_xml_node_t **nodes, int n,
            const char *block, int *idx, int *last_fail)
{
  char *norm = normalise_block (block);
  const char *rest;       /* remainder after the leading term + operator */
  char op;
  int first;
  int result;

  if (norm[0] == '(')
    {
      int depth = 1, off = 1;
      int sublen;
      char *sub;
      while (norm[off] && depth > 0)
        {
          if (norm[off] == '(') depth++;
          else if (norm[off] == ')') depth--;
          off++;
        }
      sublen = off - 2;
      if (sublen < 0) sublen = 0;
      sub = (char *) malloc ((size_t) sublen + 1);
      memcpy (sub, norm + 1, (size_t) sublen);
      sub[sublen] = '\0';
      first = eval_block (st, rs, nodes, n, sub, idx, last_fail);
      free (sub);
      op = norm[off];
      rest = (op == '\0') ? NULL : norm + off + 1;
    }
  else if (norm[0] == '#')
    {
      op = norm[1];
      if (op != '\0' && op != 'A' && op != 'O')
        {
          /* frankendrift's inner "Case Else": a '#' followed by neither an
             operator nor end-of-block (e.g. the spurious leading '#' of a
             malformed "##A#").  EvaluateRestrictionBlock takes Case Else and
             falls off the end -> default False, WITHOUT calling
             PassSingleRestriction.  So the restriction is never evaluated and
             sRestrictionText (here *last_fail) is left untouched -- preserving
             any carried value (the basis for Anno's reference-free OpeningHid
             failing *with* the previous command's leftover message). */
          (*idx)++;             /* FD still runs iRestNum += 1 */
          free (norm);
          return 0;
        }
      {
        int my = (*idx)++;
        first = (my < n) ? pass_single (st, &rs[my]) : 1;
        if (my < n)
          *last_fail = first ? -1 : my; /* clear on pass, record on fail */
      }
      rest = (op == '\0') ? NULL : norm + 2;
    }
  else
    {
      free (norm);
      /* frankendrift's EvaluateRestrictionBlock hits "Case Else" (a block that
         starts with neither '(' nor '#') and falls off the end with no Return,
         so the Boolean function yields its default False -- a malformed bracket
         sequence FAILS, it does not pass. */
      return 0;
    }

  if (rest == NULL)
    result = first;
  else if (op == 'A')
    {
      if (!first) { *idx += count_hashes (rest); result = 0; }
      else result = eval_block (st, rs, nodes, n, rest, idx, last_fail);
    }
  else if (op == 'O')
    {
      if (first) { *idx += count_hashes (rest); result = 1; }
      else result = eval_block (st, rs, nodes, n, rest, idx, last_fail);
    }
  else
    /* The character after the leading term is neither 'A' nor 'O' (e.g. the
       extra '#' in the malformed "##A#").  frankendrift's inner Select Case
       takes "Case Else" and falls off the end -> default False.  Some games
       (Anno 1700's OpeningClo2) ship such sequences expecting the task to FAIL,
       so the engine moves on to the general task. */
    result = 0;

  free (norm);
  return result;
}

/*
 * Shared core for a5restr_pass / a5restr_fail_message: parse the restriction
 * specs, build the (square-bracket-expanded) BracketSequence and evaluate it.
 * Returns the boolean result; when `fail_node_out` is non-NULL it is set to the
 * <Restriction> on the deciding failing path (frankendrift's sRestrictionText
 * source) -- NULL if the block passes or no single restriction failed.
 */
static int
eval_restrictions (a5_state_t *st, const a5_xml_node_t *restrictions,
                   const a5_xml_node_t **fail_node_out)
{
  a5_restr_t *rs;
  const a5_xml_node_t **nodes;
  const a5_xml_node_t *c;
  const char *bracket;
  int n, i, idx = 0, result, last_fail = -2;   /* -2 = no restriction evaluated */
  char *normbrackets = NULL;

  if (fail_node_out != NULL)
    *fail_node_out = NULL;
  if (restrictions == NULL)
    return 1;
  n = a5xml_count (restrictions, "Restriction");
  if (n == 0)
    return 1;

  rs = (a5_restr_t *) calloc ((size_t) n, sizeof *rs);
  nodes = (const a5_xml_node_t **) calloc ((size_t) n, sizeof *nodes);
  i = 0;
  for (c = restrictions->first_child; c != NULL; c = c->next)
    if (strcmp (c->name, "Restriction") == 0)
      {
        const a5_xml_node_t *tn = restr_type_node (c);
        rs[i].type = tn ? tn->name : "";
        parse_spec (&rs[i], tn ? tn->text : "");
        nodes[i] = c;
        i++;
      }

  bracket = a5xml_child_text (restrictions, "BracketSequence");
  if (bracket == NULL || bracket[0] == '\0')
    {
      /* default: AND all restrictions -> "#A#A#..." */
      int sz = n * 2;
      normbrackets = (char *) malloc ((size_t) sz + 1);
      {
        int p = 0, j;
        for (j = 0; j < n; j++)
          {
            if (j) normbrackets[p++] = 'A';
            normbrackets[p++] = '#';
          }
        normbrackets[p] = '\0';
      }
      bracket = normbrackets;
    }

  /*
   * Expand the square-bracket grouping ADRIFT uses: '[' -> "((", ']' -> "))"
   * (FileIO.vb:640).  This is not a stray-bracket cleanup -- the doubling is what
   * keeps sequences such as "#A#A#A#A[#A#A#A#A#)O#)A#A#" balanced.
   */
  {
    char *b = (char *) malloc (strlen (bracket) * 2 + 1);
    char *w = b;
    const char *q;
    for (q = bracket; *q; q++)
      {
        if (*q == '[')      { *w++ = '('; *w++ = '('; }
        else if (*q == ']') { *w++ = ')'; *w++ = ')'; }
        else                  *w++ = *q;
      }
    *w = '\0';
    result = eval_block (st, rs, nodes, n, b, &idx, &last_fail);
    free (b);
  }

  /* frankendrift's sRestrictionText side effect (clsUserSession.vb:5176/5181):
     every PassSingleRestriction on the deciding path clears it on a pass and
     sets it to the restriction's Message on a fail; a call that evaluates *no*
     single restriction (malformed bracket -> last_fail still -2) leaves it
     untouched, so the carried value from a previous command survives.  Persists
     across commands -- never reset per turn. */
  if (last_fail == -1)
    st->restriction_text = NULL;                 /* deciding restriction passed */
  else if (last_fail >= 0 && last_fail < n)
    st->restriction_text = a5xml_child (nodes[last_fail], "Message");

  if (fail_node_out != NULL && !result && last_fail >= 0 && last_fail < n)
    {
      *fail_node_out = nodes[last_fail];
      /* sRouteError override: keep st->route_error set only when the deciding
         failing restriction is the HaveRouteInDirection one whose exit was
         restriction-blocked (PassSingleRestriction: "If sRouteError <>
         sRestrictionText AndAlso sRouteError <> ''").  The getter then prefers
         it over the restriction's generic "There is no route..." message. */
      if (!(st->route_error != NULL
            && streq (rs[last_fail].type, "Character")
            && streq (rs[last_fail].op, "HaveRouteInDirection")))
        st->route_error = NULL;
    }

  free (normbrackets);
  for (i = 0; i < n; i++)
    free (rs[i].buf);
  free (rs);
  free (nodes);
  return result;
}

const a5_xml_node_t *
a5restr_fail_message (a5_state_t *st, const a5_xml_node_t *restrictions)
{
  const a5_xml_node_t *fail_node = NULL;
  st->route_error = NULL;
  eval_restrictions (st, restrictions, &fail_node);
  if (st->route_error != NULL)
    return st->route_error;             /* blocked-exit message wins */
  return fail_node ? a5xml_child (fail_node, "Message") : NULL;
}

int
a5restr_pass (a5_state_t *st, const a5_xml_node_t *restrictions)
{
  return eval_restrictions (st, restrictions, NULL);
}

/* Does a `<ref_alias> Must Exist` restriction (e.g. "ReferencedObject2") fall
 * *within* the task's evaluated BracketSequence?  ADRIFT evaluates only the first
 * M restrictions, where M is the number of '#' placeholders in the bracket (the
 * restrictions list may be longer than the bracket -- a truncated/malformed bracket
 * simply leaves the trailing restrictions unevaluated, exactly as eval_block does).
 * Used to decide whether an unmatched reference is genuinely *required*: if its
 * Must Exist is never evaluated, the task does not actually need it, so a sibling
 * reference's out-of-scope ambiguity ("You can't see any oil!") surfaces instead of
 * this reference's no-reference fallback ("Sorry, I'm not sure which object...").
 * Mirrors the pour task (`#A#A#`, object2's Exist truncated -> cantsee) vs the blow
 * / hang tasks (object2's Exist inside the bracket -> no-reference message). */
int
a5restr_exist_evaluated (const a5_xml_node_t *restrictions, const char *ref_alias)
{
  const a5_xml_node_t *c;
  const char *bracket;
  int n = 0, M, i;

  if (restrictions == NULL || ref_alias == NULL)
    return 0;
  for (c = restrictions->first_child; c != NULL; c = c->next)
    if (strcmp (c->name, "Restriction") == 0)
      n++;
  bracket = a5xml_child_text (restrictions, "BracketSequence");
  if (bracket == NULL || bracket[0] == '\0')
    M = n;                       /* default bracket ANDs every restriction */
  else
    { M = 0; for (const char *q = bracket; *q; q++) if (*q == '#') M++; }

  i = 0;
  for (c = restrictions->first_child; c != NULL; c = c->next)
    {
      const a5_xml_node_t *tn;
      a5_restr_t r;
      int hit;
      if (strcmp (c->name, "Restriction") != 0)
        continue;
      if (i >= M)
        break;                   /* beyond the evaluated bracket prefix */
      tn = restr_type_node (c);
      if (tn != NULL)
        {
          parse_spec (&r, tn->text);
          hit = (strcmp (r.key1, ref_alias) == 0 && streq (r.op, "Exist"));
          free (r.buf);
          if (hit)
            return 1;
        }
      i++;
    }
  return 0;
}

int
a5restr_has_exist (const a5_xml_node_t *restrictions, char type)
{
  const char *want_type = (type == 'c') ? "Character" : "Object";
  const a5_xml_node_t *c;

  if (restrictions == NULL)
    return 0;
  for (c = restrictions->first_child; c != NULL; c = c->next)
    {
      const a5_xml_node_t *tn;
      a5_restr_t r;
      int hit;
      if (strcmp (c->name, "Restriction") != 0)
        continue;
      tn = restr_type_node (c);
      if (tn == NULL || strcmp (tn->name, want_type) != 0)
        continue;
      parse_spec (&r, tn->text);
      hit = streq (r.op, "Exist");
      free (r.buf);
      if (hit)
        return 1;
    }
  return 0;
}
