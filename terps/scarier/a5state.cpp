/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- runtime state (Phase 2).  See a5state.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a5state.h"

/* ----------------------------------------------------------------- helpers */

static const char *
obj_prop (const a5_object_t *o, const char *key)
{
  const a5_prop_t *p = a5_prop_find (o->props, o->n_props, key);
  return p ? p->value : NULL;
}

static int
obj_has_prop (const a5_object_t *o, const char *key)
{
  return a5_prop_find (o->props, o->n_props, key) != NULL;
}

static const char *
chr_prop (const a5_character_t *c, const char *key)
{
  const a5_prop_t *p = a5_prop_find (c->props, c->n_props, key);
  return p ? p->value : NULL;
}

static int
streq (const char *a, const char *b)
{
  return a != NULL && b != NULL && strcmp (a, b) == 0;
}

/* Decode an object's location property block into an a5_objloc_t. */
static void
compute_objloc (const a5_object_t *o, a5_objloc_t *loc)
{
  const char *sd = obj_prop (o, "StaticOrDynamic");

  loc->where = A5_OWHERE_NONE;
  loc->key = NULL;
  loc->is_static = !streq (sd, "Dynamic");   /* default Static */

  if (!loc->is_static && obj_has_prop (o, "DynamicLocation"))
    {
      const char *dl = obj_prop (o, "DynamicLocation");
      if (streq (dl, "Held By Character"))
        { loc->where = A5_OWHERE_HELD_BY;   loc->key = obj_prop (o, "HeldByWho"); }
      else if (streq (dl, "Hidden"))
        { loc->where = A5_OWHERE_HIDDEN; }
      else if (streq (dl, "In Location"))
        { loc->where = A5_OWHERE_LOCATION;  loc->key = obj_prop (o, "InLocation"); }
      else if (streq (dl, "Inside Object"))
        { loc->where = A5_OWHERE_IN_OBJECT; loc->key = obj_prop (o, "InsideWhat"); }
      else if (streq (dl, "On Object"))
        { loc->where = A5_OWHERE_ON_OBJECT; loc->key = obj_prop (o, "OnWhat"); }
      else if (streq (dl, "Worn By Character"))
        { loc->where = A5_OWHERE_WORN_BY;   loc->key = obj_prop (o, "WornByWho"); }
    }

  /* A holder key may be stored as the literal variable "%Player%" rather than
     the resolved player key (frankendrift resolves it to Adventure.Player.Key
     in every accessor, e.g. clsObjectLocation.Key getter, clsObject.vb:1058).
     The engine uses "Player" as the player key throughout, so normalise here —
     otherwise e.g. a backpack with WornByWho=%Player% is dropped from ListWorn. */
  if (streq (loc->key, "%Player%"))
    loc->key = "Player";
  else if (loc->is_static && obj_has_prop (o, "StaticLocation"))
    {
      const char *sl = obj_prop (o, "StaticLocation");
      if (streq (sl, "Nowhere") || streq (sl, "Hidden"))
        { loc->where = A5_OWHERE_HIDDEN; }
      else if (streq (sl, "Single Location"))
        { loc->where = A5_OWHERE_LOCATION;  loc->key = obj_prop (o, "AtLocation"); }
      else if (streq (sl, "Location Group"))
        { loc->where = A5_OWHERE_LOCGROUP;  loc->key = obj_prop (o, "AtLocationGroup"); }
      else if (streq (sl, "Everywhere"))
        { loc->where = A5_OWHERE_ALLROOMS; }
      else if (streq (sl, "Part of Character"))
        { loc->where = A5_OWHERE_PART_CHAR; loc->key = obj_prop (o, "PartOfWho"); }
      else if (streq (sl, "Part of Object"))
        { loc->where = A5_OWHERE_PART_OBJECT; loc->key = obj_prop (o, "PartOfWhat"); }
    }
}

/* ------------------------------------------------------------------- public */

a5_state_t *
a5state_new (const a5_adventure_t *adv)
{
  a5_state_t *st;
  int i;

  if (adv == NULL)
    return NULL;
  st = (a5_state_t *) calloc (1, sizeof *st);
  if (st == NULL)
    return NULL;
  st->adv = adv;

  if (adv->n_objects > 0)
    {
      st->obj = (a5_objloc_t *) calloc ((size_t) adv->n_objects, sizeof *st->obj);
      for (i = 0; i < adv->n_objects; i++)
        compute_objloc (&adv->objects[i], &st->obj[i]);
    }

  if (adv->n_characters > 0)
    {
      st->char_loc = (const char **) calloc ((size_t) adv->n_characters,
                                             sizeof *st->char_loc);
      st->char_position = (char **) calloc ((size_t) adv->n_characters,
                                            sizeof *st->char_position);
      st->char_onobj = (const char **) calloc ((size_t) adv->n_characters,
                                               sizeof *st->char_onobj);
      st->char_in = (char *) calloc ((size_t) adv->n_characters, 1);
      for (i = 0; i < adv->n_characters; i++)
        {
          const a5_character_t *c = &adv->characters[i];
          const char *cl = chr_prop (c, "CharacterLocation");
          const char *pos = chr_prop (c, "CharacterPosition");
          /* clsCharacterLocation.ExistWhere / Key decode.  "At Location" stores
             the location in char_loc; "On Object"/"In Object" store the carrier
             object in char_onobj (char_in distinguishes inside vs on the surface)
             and leave char_loc NULL -- the effective location is resolved through
             the object (a5state_character_at_location).  "On Character"/"Hidden"
             stay NULL (treated as not-at-any-location). */
          if (cl == NULL || streq (cl, "At Location"))
            st->char_loc[i] = chr_prop (c, "CharacterAtLocation");
          else if (streq (cl, "On Object"))
            st->char_onobj[i] = chr_prop (c, "CharOnWhat");
          else if (streq (cl, "In Object"))
            { st->char_onobj[i] = chr_prop (c, "CharInsideWhat");
              if (st->char_in != NULL) st->char_in[i] = 1; }
          st->char_position[i] = strdup (pos ? pos : "Standing");
        }
      st->char_seen = (char *) calloc ((size_t) adv->n_characters, 1);
      st->obj_seen = (char *) calloc ((size_t) adv->n_objects, 1);
    }

  st->conv_char = strdup ("");
  st->conv_node = strdup ("");
  st->ctx_char = NULL;

  st->s_it = strdup ("");
  st->s_them = strdup ("");
  st->s_him = strdup ("");
  st->s_her = strdup ("");

  if (adv->n_variables > 0)
    {
      st->var_num = (long *) calloc ((size_t) adv->n_variables, sizeof *st->var_num);
      st->var_text = (char **) calloc ((size_t) adv->n_variables, sizeof *st->var_text);
      for (i = 0; i < adv->n_variables; i++)
        {
          const a5_variable_t *v = &adv->variables[i];
          if (streq (v->type, "Text"))
            st->var_text[i] = v->initial ? strdup (v->initial) : strdup ("");
          else
            st->var_num[i] = v->initial ? strtol (v->initial, NULL, 10) : 0;
        }
    }

  if (adv->n_tasks > 0)
    st->task_done = (char *) calloc ((size_t) adv->n_tasks, 1);

  return st;
}

void
a5state_free (a5_state_t *st)
{
  int i;
  if (st == NULL)
    return;
  if (st->var_text != NULL)
    for (i = 0; i < st->adv->n_variables; i++)
      free (st->var_text[i]);
  if (st->char_position != NULL)
    for (i = 0; i < st->adv->n_characters; i++)
      free (st->char_position[i]);
  for (i = 0; i < st->n_ov; i++)
    {
      free (st->ov[i].entity);
      free (st->ov[i].prop);
      free (st->ov[i].value);
    }
  free (st->ov);
  free (st->end_message);
  free (st->obj);
  free ((void *) st->char_loc);
  free (st->char_position);
  free (st->char_seen);
  free (st->obj_seen);
  free (st->conv_char);
  free (st->conv_node);
  free (st->s_it);
  free (st->s_them);
  free (st->s_him);
  free (st->s_her);
  free ((void *) st->char_onobj);
  free (st->char_in);
  free (st->var_num);
  free (st->var_text);
  free (st->task_done);
  free (st->disp_once);
  for (i = 0; i < st->n_looks; i++)
    { free (st->looks[i].loc_key); free (st->looks[i].text); }
  free (st->looks);
  free (st);
}

/* ----------------------------------------------------- group/location gate */

int
a5state_in_group_or_location (const a5_state_t *st, const char *charkey,
                              const char *key)
{
  const char *cloc;
  int ci, i;
  if (key == NULL || key[0] == '\0')
    return 0;
  ci = a5state_character_index (st, charkey ? charkey : "Player");
  cloc = (ci >= 0) ? st->char_loc[ci] : NULL;
  if (cloc == NULL)
    return 0;
  /* A location key: the character is at exactly that location. */
  if (a5model_location (st->adv, key) != NULL)
    return streq (cloc, key);
  /* A group key: the character's location is one of its members. */
  for (i = 0; i < st->adv->n_groups; i++)
    if (streq (st->adv->groups[i].key, key))
      {
        const a5_group_t *g = &st->adv->groups[i];
        int m;
        for (m = 0; m < g->n_members; m++)
          if (streq (g->members[m], cloc))
            return 1;
        return 0;
      }
  return 0;
}

/* -------------------------------------------------------- SetLook look stack */

void
a5state_push_look (a5_state_t *st, const char *loc_key, const char *text)
{
  if (st->n_looks == st->cap_looks)
    {
      st->cap_looks = st->cap_looks ? st->cap_looks * 2 : 4;
      st->looks = (a5_looktext_t *)
        realloc (st->looks, (size_t) st->cap_looks * sizeof *st->looks);
    }
  st->looks[st->n_looks].loc_key = strdup (loc_key ? loc_key : "");
  st->looks[st->n_looks].text    = strdup (text ? text : "");
  st->n_looks++;
}

const char *
a5state_player_look (const a5_state_t *st)
{
  int i;
  /* LIFO: the most recently pushed matching look text wins. */
  for (i = st->n_looks - 1; i >= 0; i--)
    if (a5state_in_group_or_location (st, "Player", st->looks[i].loc_key))
      return st->looks[i].text;
  return NULL;
}

/* ----------------------------------------------------------- conversation */

void
a5state_set_conv_char (a5_state_t *st, const char *key)
{
  free (st->conv_char);
  st->conv_char = strdup (key ? key : "");
}

void
a5state_set_conv_node (a5_state_t *st, const char *key)
{
  free (st->conv_node);
  st->conv_node = strdup (key ? key : "");
}

/* ------------------------------------------------------------ DisplayOnce */

int
a5state_disp_once_seen (const a5_state_t *st, const void *node)
{
  int i;
  for (i = 0; i < st->n_disp_once; i++)
    if (st->disp_once[i] == node)
      return 1;
  return 0;
}

void
a5state_disp_once_mark (a5_state_t *st, const void *node)
{
  if (node == NULL || a5state_disp_once_seen (st, node))
    return;
  if (st->n_disp_once >= st->cap_disp_once)
    {
      int nc = st->cap_disp_once ? st->cap_disp_once * 2 : 8;
      st->disp_once = (const void **) realloc (st->disp_once,
                                               (size_t) nc * sizeof *st->disp_once);
      st->cap_disp_once = nc;
    }
  st->disp_once[st->n_disp_once++] = node;
}

/* ------------------------------------------------------- reference bindings */

void
a5state_clear_refs (a5_state_t *st)
{
  st->n_refbind = 0;
  st->n_ref_items = 0;
  st->ref_items_type = 'o';
  st->ref_object1_plural = 0;
  st->ref_character1_plural = 0;
}

void
a5state_bind_ref (a5_state_t *st, const char *name, const char *value)
{
  int i;
  if (name == NULL || value == NULL)
    return;
  for (i = 0; i < st->n_refbind; i++)
    if (streq (st->ref_name[i], name))
      {
        snprintf (st->ref_value[i], sizeof st->ref_value[i], "%s", value);
        return;
      }
  if (st->n_refbind >= (int) (sizeof st->ref_name / sizeof st->ref_name[0]))
    return;
  snprintf (st->ref_name[st->n_refbind], sizeof st->ref_name[0], "%s", name);
  snprintf (st->ref_value[st->n_refbind], sizeof st->ref_value[0], "%s", value);
  st->n_refbind++;
}

const char *
a5state_lookup_ref (const a5_state_t *st, const char *name)
{
  int i;
  if (name == NULL)
    return NULL;
  for (i = 0; i < st->n_refbind; i++)
    if (streq (st->ref_name[i], name))
      return st->ref_value[i];
  return NULL;
}

/* ------------------------------------------------------- property overrides */

const char *
a5state_entity_prop (const a5_state_t *st, const char *entkey, const char *propkey)
{
  int i;
  const a5_object_t *o;
  const a5_character_t *c;
  const a5_location_t *l;
  const a5_prop_t *p;

  if (entkey == NULL || propkey == NULL)
    return NULL;
  for (i = 0; i < st->n_ov; i++)
    if (streq (st->ov[i].entity, entkey) && streq (st->ov[i].prop, propkey))
      return st->ov[i].value;

  /* Fall back to the model's static value. */
  o = a5model_object (st->adv, entkey);
  if (o != NULL && (p = a5_prop_find (o->props, o->n_props, propkey)) != NULL)
    return p->value;
  c = a5model_character (st->adv, entkey);
  if (c != NULL && (p = a5_prop_find (c->props, c->n_props, propkey)) != NULL)
    return p->value;
  l = a5model_location (st->adv, entkey);
  if (l != NULL && (p = a5_prop_find (l->props, l->n_props, propkey)) != NULL)
    return p->value;
  return NULL;
}

/* Runtime object-group membership (clsGroup add/remove at runtime: the
   AddObjectToGroup / RemoveObjectFromGroup actions, e.g. the flashlight joins
   "LightSources" while switched on).  We layer it onto the property-override
   store under a synthetic key ("@InGroup:<group>"), so it saves/restores for
   free; '@'/':' never occur in real ADRIFT property keys.  Returns the
   effective membership: a runtime override wins, else the model's static
   <Member> list. */
static void
group_prop_key (char *buf, size_t n, const char *grpkey)
{
  snprintf (buf, n, "@InGroup:%s", grpkey ? grpkey : "");
}

int
a5state_object_in_group (const a5_state_t *st, const char *grpkey,
                         const char *objkey)
{
  char key[160];
  const char *ov;
  int i, m;
  if (grpkey == NULL || objkey == NULL)
    return 0;
  group_prop_key (key, sizeof key, grpkey);
  for (i = 0; i < st->n_ov; i++)
    if (streq (st->ov[i].entity, objkey) && streq (st->ov[i].prop, key))
      return streq (st->ov[i].value, "1");
  for (i = 0; i < st->adv->n_groups; i++)
    if (streq (st->adv->groups[i].key, grpkey))
      {
        for (m = 0; m < st->adv->groups[i].n_members; m++)
          if (streq (st->adv->groups[i].members[m], objkey))
            return 1;
        return 0;
      }
  return 0;
}

void
a5state_set_object_in_group (a5_state_t *st, const char *grpkey,
                             const char *objkey, int present)
{
  char key[160];
  if (grpkey == NULL || objkey == NULL)
    return;
  group_prop_key (key, sizeof key, grpkey);
  a5state_set_prop (st, objkey, key, present ? "1" : "0");
}

void
a5state_set_prop (a5_state_t *st, const char *entkey, const char *propkey,
                  const char *value)
{
  int i;
  if (entkey == NULL || propkey == NULL)
    return;
  for (i = 0; i < st->n_ov; i++)
    if (streq (st->ov[i].entity, entkey) && streq (st->ov[i].prop, propkey))
      {
        free (st->ov[i].value);
        st->ov[i].value = strdup (value ? value : "");
        return;
      }
  if (st->n_ov >= st->cap_ov)
    {
      int nc = st->cap_ov ? st->cap_ov * 2 : 8;
      st->ov = (a5_prop_ov_t *) realloc (st->ov, (size_t) nc * sizeof *st->ov);
      st->cap_ov = nc;
    }
  st->ov[st->n_ov].entity = strdup (entkey);
  st->ov[st->n_ov].prop = strdup (propkey);
  st->ov[st->n_ov].value = strdup (value ? value : "");
  st->n_ov++;
}

int
a5state_object_index (const a5_state_t *st, const char *key)
{
  int i;
  if (key == NULL)
    return -1;
  for (i = 0; i < st->adv->n_objects; i++)
    if (streq (st->adv->objects[i].key, key))
      return i;
  return -1;
}

int
a5state_character_index (const a5_state_t *st, const char *key)
{
  int i;
  if (key == NULL)
    return -1;
  for (i = 0; i < st->adv->n_characters; i++)
    if (streq (st->adv->characters[i].key, key))
      return i;
  return -1;
}

int
a5state_variable_index (const a5_state_t *st, const char *key)
{
  int i;
  if (key == NULL)
    return -1;
  for (i = 0; i < st->adv->n_variables; i++)
    if (streq (st->adv->variables[i].key, key))
      return i;
  return -1;
}

int
a5state_task_index (const a5_state_t *st, const char *key)
{
  int i;
  if (key == NULL)
    return -1;
  for (i = 0; i < st->adv->n_tasks; i++)
    if (streq (st->adv->tasks[i].key, key))
      return i;
  return -1;
}

const char *
a5state_player_location (const a5_state_t *st)
{
  int pi = a5state_character_index (st, "Player");
  if (pi < 0)
    return NULL;
  return st->char_loc[pi];
}

/* Recursive ExistsAtLocation, with a depth guard against cyclic placements. */
static int
exists_at (const a5_state_t *st, int oi, const char *lockey, int directly, int depth)
{
  const a5_objloc_t *loc;
  int parent;

  if (oi < 0 || depth > 32)
    return 0;
  loc = &st->obj[oi];

  switch (loc->where)
    {
    case A5_OWHERE_ALLROOMS:
      return 1;
    case A5_OWHERE_HIDDEN:
    case A5_OWHERE_NONE:
      return streq (lockey, "Hidden");
    case A5_OWHERE_LOCATION:
      return streq (loc->key, lockey);
    case A5_OWHERE_LOCGROUP:
      {
        const a5_group_t *g;
        int i;
        for (i = 0; i < st->adv->n_groups; i++)
          if (streq (st->adv->groups[i].key, loc->key))
            {
              g = &st->adv->groups[i];
              for (int m = 0; m < g->n_members; m++)
                if (streq (g->members[m], lockey))
                  return 1;
              return 0;
            }
        return 0;
      }
    case A5_OWHERE_ON_OBJECT:
    case A5_OWHERE_IN_OBJECT:
    case A5_OWHERE_PART_OBJECT:
      if (directly)
        return 0;
      parent = a5state_object_index (st, loc->key);
      return exists_at (st, parent, lockey, 0, depth + 1);
    case A5_OWHERE_HELD_BY:
    case A5_OWHERE_WORN_BY:
    case A5_OWHERE_PART_CHAR:
      {
        int ci;
        if (directly)
          return 0;
        ci = a5state_character_index (st, loc->key);
        if (ci < 0)
          return 0;
        return streq (st->char_loc[ci], lockey);
      }
    }
  return 0;
}

int
a5state_object_at_location (const a5_state_t *st, int oi, const char *lockey,
                            int directly)
{
  return exists_at (st, oi, lockey, directly, 0);
}

int
a5state_object_key_at_location (const a5_state_t *st, const char *objkey,
                                const char *lockey, int directly)
{
  return exists_at (st, a5state_object_index (st, objkey), lockey, directly, 0);
}

int
a5state_character_at_location (const a5_state_t *st, int ci, const char *lockey)
{
  if (ci < 0 || lockey == NULL || st->char_loc == NULL)
    return 0;
  /* "At Location": directly compare the recorded location key. */
  if (st->char_loc[ci] != NULL)
    return streq (st->char_loc[ci], lockey);
  /* "On Object"/"In Object": present wherever the carrier object is (its
     container chain reaches lockey).  clsObject.BoundVisible would hide a char
     inside a closed opaque container; that nuance is unused by the corpus. */
  if (st->char_onobj != NULL && st->char_onobj[ci] != NULL)
    return a5state_object_key_at_location (st, st->char_onobj[ci], lockey, 0);
  return 0;
}

int
a5state_var_num_by_name (const a5_state_t *st, const char *name, long *out)
{
  int i;
  for (i = 0; i < st->adv->n_variables; i++)
    if (streq (st->adv->variables[i].name, name))
      {
        if (out != NULL)
          *out = st->var_num[i];
        return 1;
      }
  return 0;
}

const char *
a5state_var_text_by_name (const a5_state_t *st, const char *name)
{
  int i;
  for (i = 0; i < st->adv->n_variables; i++)
    if (streq (st->adv->variables[i].name, name))
      return st->var_text[i];
  return NULL;
}
