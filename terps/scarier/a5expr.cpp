/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- the property-expression engine.  See a5expr.h.
 *
 * A port of frankendrift Global.ReplaceOOProperty (Global.vb:655-1620): a key (or
 * pipe-joined key list) navigated by ".Function(args)" steps.  The first key is
 * resolved to an object / character / location / group / direction (the VB "final
 * Else" block), then each step either navigates to a new item set
 * (Parent/Children/Contents/Location/Held/Worn/Objects) or terminates
 * (Name/List/Description/Count/Sum/<property>).
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "a5expr.h"
#include "a5restr.h"
#include "a5text.h"
#include "a5xml.h"

static int
streq (const char *a, const char *b)
{
  return a != NULL && b != NULL && strcmp (a, b) == 0;
}

static std::string
lower (const std::string &s)
{
  std::string o = s;
  for (char &c : o) c = (char) tolower ((unsigned char) c);
  return o;
}

static int
contains (const std::string &hay, const char *needle)
{
  return hay.find (needle) != std::string::npos;
}

/* ----------------------------------------------------- item-set navigation */

/* A navigated item-set: a list of entity keys, plus an optional carried
   "property key" used by the .Sum aggregator, mirroring sPropertyKey. */
struct Ctx {
  std::vector<std::string> keys;   /* entity keys (objects/characters/locations) */
  std::vector<std::string> dirs;   /* direction names (the lstDirs path)         */
  std::string prop_key;            /* sPropertyKey carried for Sum               */
  int is_list;                     /* lst path (vs a single resolved item)       */
  int is_dirs;                     /* the lstDirs path (a .Exits direction list) */
  Ctx () : is_list (0), is_dirs (0) {}
};

/* The twelve directions in DirectionsEnum order (North=0 .. NorthWest=11);
   `.Exits` enumerates them in this order (Global.vb:1468). */
static const char *kEnumDirs[12] = {
  "North", "East", "South", "West", "Up", "Down",
  "In", "Out", "NorthEast", "SouthEast", "SouthWest", "NorthWest"
};

/* A location has an exit in `dir` if its Movement table names that direction
   with a non-empty Destination -- frankendrift's location `.Exits` checks
   arlDirections(d).LocationKey <> "" and does NOT apply the route's
   restrictions (unlike a character's HasRouteInDirection). */
static int
loc_has_exit (a5_state_t *st, const char *lockey, const char *dir)
{
  const a5_location_t *l = a5model_location (st->adv, lockey);
  const a5_xml_node_t *c;
  if (l == NULL)
    return 0;
  for (c = l->node->first_child; c != NULL; c = c->next)
    {
      const char *d, *dest;
      if (strcmp (c->name, "Movement") != 0)
        continue;
      d = a5xml_child_text (c, "Direction");
      if (!streq (d, dir))
        continue;
      dest = a5xml_child_text (c, "Destination");
      return dest != NULL && dest[0] != '\0';
    }
  return 0;
}

/* Lowercase a direction's canonical name for the `.List` display
   ("NorthEast" -> "northeast", matching LCase(DirectionName)). */
static std::string
dir_display (const std::string &canon)
{
  return lower (canon);
}

/* Join a direction list the way clsLocation.List does: ", " between, " and "
   (or " or " if the args ask) before the last. */
static std::string
render_dirs (const std::vector<std::string> &dirs, const std::string &args)
{
  std::string sep = contains (lower (args), "or") ? " or " : " and ";
  if (dirs.empty ())
    return "nothing";
  std::string out;
  int n = (int) dirs.size ();
  for (int i = 0; i < n; i++)
    {
      if (i > 0 && i < n - 1) out += ", ";
      if (n > 1 && i == n - 1) out += sep;
      out += dir_display (dirs[i]);
    }
  return out;
}

static char item_kind (a5_state_t *st, const char *key);   /* 'o'/'c'/'l'/'g'/0 */

/* Objects held (where == HELD_BY) by a character key. */
static std::vector<std::string>
objs_held_by (a5_state_t *st, const char *charkey)
{
  std::vector<std::string> v;
  for (int i = 0; i < st->adv->n_objects; i++)
    if (st->obj[i].where == A5_OWHERE_HELD_BY && streq (st->obj[i].key, charkey))
      v.push_back (st->adv->objects[i].key);
  return v;
}

static std::vector<std::string>
objs_worn_by (a5_state_t *st, const char *charkey)
{
  std::vector<std::string> v;
  for (int i = 0; i < st->adv->n_objects; i++)
    if (st->obj[i].where == A5_OWHERE_WORN_BY && streq (st->obj[i].key, charkey))
      v.push_back (st->adv->objects[i].key);
  return v;
}

/* Objects directly inside (in_only) or inside+on a container object. */
static std::vector<std::string>
objs_children (a5_state_t *st, const char *objkey, int in_only)
{
  std::vector<std::string> v;
  for (int i = 0; i < st->adv->n_objects; i++)
    {
      a5_owhere_t w = st->obj[i].where;
      int hit = (w == A5_OWHERE_IN_OBJECT)
              || (!in_only && w == A5_OWHERE_ON_OBJECT);
      if (hit && streq (st->obj[i].key, objkey))
        v.push_back (st->adv->objects[i].key);
    }
  return v;
}

/* Objects directly in a location (ExistsAtLocation, directly). */
static std::vector<std::string>
objs_in_location (a5_state_t *st, const char *lockey)
{
  std::vector<std::string> v;
  for (int i = 0; i < st->adv->n_objects; i++)
    if (a5state_object_at_location (st, i, lockey, 1))
      v.push_back (st->adv->objects[i].key);
  return v;
}

/* The parent key of an object/character (the thing it is in/on/held/worn by). */
static const char *
parent_key (a5_state_t *st, const char *key)
{
  int oi = a5state_object_index (st, key);
  if (oi >= 0)
    {
      a5_owhere_t w = st->obj[oi].where;
      if (w == A5_OWHERE_ON_OBJECT || w == A5_OWHERE_IN_OBJECT
          || w == A5_OWHERE_HELD_BY || w == A5_OWHERE_WORN_BY
          || w == A5_OWHERE_PART_OBJECT || w == A5_OWHERE_PART_CHAR)
        return st->obj[oi].key;
    }
  return NULL;
}

/* The location key(s) an object is in (its location roots). */
static std::vector<std::string>
location_roots (a5_state_t *st, const char *objkey)
{
  std::vector<std::string> v;
  int oi = a5state_object_index (st, objkey);
  for (int li = 0; oi >= 0 && li < st->adv->n_locations; li++)
    if (a5state_object_at_location (st, oi, st->adv->locations[li].key, 0))
      v.push_back (st->adv->locations[li].key);
  return v;
}

static char
item_kind (a5_state_t *st, const char *key)
{
  if (key == NULL) return 0;
  if (a5model_object (st->adv, key))    return 'o';
  if (a5model_character (st->adv, key)) return 'c';
  if (a5model_location (st->adv, key))  return 'l';
  for (int i = 0; i < st->adv->n_groups; i++)
    if (streq (st->adv->groups[i].key, key)) return 'g';
  return 0;
}

/* ------------------------------------------------------------- name rendering */

/* The display name of a single item key, with an article (definite/indef/none)
   for objects; characters use their plain name. */
static std::string
item_name (a5_state_t *st, const std::string &key, a5_article_t art)
{
  const a5_object_t *o = a5model_object (st->adv, key.c_str ());
  if (o != NULL)
    {
      char *n = a5text_object_name (o, art);
      std::string r = n ? n : "";
      free (n);
      return r;
    }
  const a5_character_t *c = a5model_character (st->adv, key.c_str ());
  if (c != NULL)
    {
      char *n = a5text_character_subjective (st, c);
      std::string r = n ? n : key;
      free (n);
      return r;
    }
  const a5_location_t *l = a5model_location (st->adv, key.c_str ());
  if (l != NULL)
    {
      char *n = a5text_describe (st, a5xml_child (l->node, "ShortDescription"));
      std::string r = n ? n : "";
      free (n);
      return r;
    }
  return key;
}

/* Join a key list as a "List"/"Name" string per the args (and/or, article). */
static std::string
render_list (a5_state_t *st, const std::vector<std::string> &keys,
             const std::string &args, int is_name)
{
  std::string a = lower (args);
  std::string sep = " and ";
  if (contains (a, "or")) sep = " or ";
  int rows = contains (a, "rows");
  a5_article_t art = A5_ART_DEFINITE;
  if (contains (a, "indefinite")) art = A5_ART_INDEFINITE;
  else if (contains (a, "none"))  art = A5_ART_NONE;
  (void) is_name;

  if (keys.empty ())
    return "nothing";

  std::string out;
  int n = (int) keys.size ();
  for (int i = 0; i < n; i++)
    {
      if (rows)
        { if (i > 0) out += "\n"; }
      else
        {
          if (i > 0 && i < n - 1) out += ", ";
          if (n > 1 && i == n - 1) out += sep;
        }
      /* characters default to indefinite unless the args force definite */
      a5_article_t ia = art;
      if (a5model_character (st->adv, keys[i].c_str ()) != NULL)
        {
          ia = A5_ART_INDEFINITE;
          if (contains (a, "definite") && !contains (a, "indefinite"))
            ia = A5_ART_DEFINITE;
        }
      out += item_name (st, keys[i], ia);
    }
  return out;
}

/* ------------------------------------------------------------- the evaluator */

static std::string oo_prop (a5_state_t *st, Ctx ctx, const std::string &sProperty,
                            int depth, int *ok);

/* Parse "Func(args).Rest" -> function / args / remainder, per ReplaceOOProperty. */
static void
split_property (const std::string &p, std::string &fn, std::string &args,
                std::string &rem)
{
  fn = p; args.clear (); rem.clear ();
  size_t dot = p.find ('.');
  size_t ob  = p.find ('(');
  if (ob != std::string::npos && p.find (')') != std::string::npos)
    {
      size_t cb = p.find (')', ob);
      if (dot != std::string::npos)
        {
          if (dot < ob)
            { rem = p.substr (dot + 1); fn = p.substr (0, dot); }
          else
            { args = p.substr (ob + 1, cb - ob - 1);
              rem  = (cb + 2 <= p.size ()) ? p.substr (cb + 2) : std::string ();
              fn   = p.substr (0, ob); }
        }
      else
        { size_t lcb = p.rfind (')');
          args = p.substr (ob + 1, lcb - ob - 1);
          rem  = (cb + 1 <= p.size ()) ? p.substr (cb + 1) : std::string ();
          fn   = p.substr (0, ob); }
    }
  else if (dot != std::string::npos && dot > 0)
    { rem = p.substr (dot + 1); fn = p.substr (0, dot); }
}

/* Terminal: an entity's description text. */
static std::string
item_description (a5_state_t *st, const std::string &key)
{
  const a5_object_t *o = a5model_object (st->adv, key.c_str ());
  if (o != NULL)
    { char *d = a5text_describe (st, a5xml_child (o->node, "Description"));
      std::string r = d ? d : ""; free (d); return r; }
  const a5_character_t *c = a5model_character (st->adv, key.c_str ());
  if (c != NULL)
    { char *d = a5text_describe (st, a5xml_child (c->node, "Description"));
      std::string r = d ? d : ""; free (d); return r; }
  const a5_location_t *l = a5model_location (st->adv, key.c_str ());
  if (l != NULL)
    { char *d = a5text_view_location (st); std::string r = d ? d : ""; free (d);
      return r.empty () ? "There is nothing of interest here." : r; }
  return "";
}

/* Build a Ctx (single item or pipe-list) from a resolved first-key string. */
static Ctx
resolve_first (a5_state_t *st, const std::string &firstkeys)
{
  Ctx ctx;
  /* split on '|' */
  size_t start = 0;
  std::vector<std::string> parts;
  while (start <= firstkeys.size ())
    {
      size_t bar = firstkeys.find ('|', start);
      std::string seg = firstkeys.substr (start, bar == std::string::npos
                                          ? std::string::npos : bar - start);
      if (!seg.empty ()) parts.push_back (seg);
      if (bar == std::string::npos) break;
      start = bar + 1;
    }
  if (parts.size () > 1)
    {
      ctx.is_list = 1;
      for (auto &k : parts)
        ctx.keys.push_back (k);
      return ctx;
    }
  if (parts.size () == 1)
    {
      char kind = item_kind (st, parts[0].c_str ());
      if (kind == 'g')
        {
          ctx.is_list = 1;
          for (int i = 0; i < st->adv->n_groups; i++)
            if (streq (st->adv->groups[i].key, parts[0].c_str ()))
              for (int m = 0; m < st->adv->groups[i].n_members; m++)
                ctx.keys.push_back (st->adv->groups[i].members[m]);
          return ctx;
        }
      if (kind != 0)
        { ctx.keys.push_back (parts[0]); ctx.is_list = 0; return ctx; }
    }
  return ctx;   /* empty: unresolved */
}

/*
 * Evaluate the navigation step `sProperty` against `ctx`.  *ok stays 1 unless the
 * whole expression turns out to be unresolvable (the "#*!~#" sentinel).
 */
static std::string
oo_prop (a5_state_t *st, Ctx ctx, const std::string &sProperty, int depth, int *ok)
{
  std::string fn, args, rem;
  if (depth > 24) { *ok = 0; return ""; }
  split_property (sProperty, fn, args, rem);

  /* ---- direction list (a .Exits result) ---- */
  if (ctx.is_dirs)
    {
      if (fn.empty ())
        { std::string r; for (size_t i = 0; i < ctx.dirs.size (); i++)
            { if (i) r += "|"; r += ctx.dirs[i]; } return r; }
      if (fn == "Count")
        return std::to_string (ctx.dirs.size ());
      if (fn == "List" || fn == "Name")
        return render_dirs (ctx.dirs, args);
      return "";
    }

  /* ---- list path (also single items routed through here when convenient) ---- */
  if (ctx.is_list)
    {
      if (fn.empty ())
        { std::string r; for (size_t i = 0; i < ctx.keys.size (); i++)
            { if (i) r += "|"; r += ctx.keys[i]; } return r; }
      if (fn == "Count")
        return std::to_string (ctx.keys.size ());
      if (fn == "Sum")
        {
          long sum = 0;
          for (auto &k : ctx.keys)
            { const char *v = a5state_entity_prop (st, k.c_str (), ctx.prop_key.c_str ());
              if (v) sum += strtol (v, NULL, 10); }
          return std::to_string (sum);
        }
      if (fn == "Description")
        { std::string r; for (auto &k : ctx.keys)
            { if (!r.empty ()) r += "  "; r += item_description (st, k); } return r; }
      if (fn == "List" || fn == "Name")
        return render_list (st, ctx.keys, args, fn == "Name");
      if (fn == "Parent")
        {
          Ctx nc; nc.is_list = 1;
          for (auto &k : ctx.keys)
            { const char *pk = parent_key (st, k.c_str ());
              if (pk) { int dup = 0; for (auto &e : nc.keys) if (e == pk) dup = 1;
                        if (!dup) nc.keys.push_back (pk); } }
          if (!rem.empty ()) return oo_prop (st, nc, rem, depth + 1, ok);
          return "";
        }
      if (fn == "Children" || fn == "Contents")
        {
          int in_only = (fn == "Contents") || contains (lower (args), "in");
          Ctx nc; nc.is_list = 1;
          for (auto &k : ctx.keys)
            for (auto &c : objs_children (st, k.c_str (), in_only))
              nc.keys.push_back (c);
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      if (fn == "Objects")
        {
          Ctx nc; nc.is_list = 1;
          for (auto &k : ctx.keys)
            if (a5model_location (st->adv, k.c_str ()))
              for (auto &o : objs_in_location (st, k.c_str ()))
                nc.keys.push_back (o);
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      /* else: a property key -> filter / aggregate.  Carry it for .Sum. */
      {
        Ctx nc; nc.is_list = 1; nc.keys = ctx.keys; nc.prop_key = fn;
        if (!rem.empty ())
          return oo_prop (st, nc, rem, depth + 1, ok);
        /* terminal property on a list: emit the first item's value */
        for (auto &k : ctx.keys)
          { const char *v = a5state_entity_prop (st, k.c_str (), fn.c_str ());
            if (v) return v; }
        return "";
      }
    }

  /* ---- single item ---- */
  if (ctx.keys.empty ()) { *ok = 0; return ""; }
  const std::string key = ctx.keys[0];
  char kind = item_kind (st, key.c_str ());

  if (kind == 'o')
    {
      const a5_object_t *o = a5model_object (st->adv, key.c_str ());
      if (fn == "Adjective" || fn == "Prefix") return o->prefix ? o->prefix : "";
      if (fn == "Article")                     return o->article ? o->article : "";
      if (fn == "Count")                       return "1";
      if (fn == "Noun")                        return o->n_names > 0 ? o->names[0] : "";
      if (fn.empty ())                         return key;
      if (fn == "Description")                 return item_description (st, key);
      if (fn == "Name" || fn == "List")
        {
          a5_article_t art = A5_ART_DEFINITE;
          if (contains (lower (args), "indefinite")) art = A5_ART_INDEFINITE;
          else if (contains (lower (args), "none"))  art = A5_ART_NONE;
          return item_name (st, key, art);
        }
      if (fn == "Parent")
        {
          const char *pk = parent_key (st, key.c_str ());
          Ctx nc; if (pk) nc.keys.push_back (pk);
          if (pk == NULL) { return ""; }
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      if (fn == "Location")
        {
          Ctx nc; nc.is_list = 1; nc.keys = location_roots (st, key.c_str ());
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      if (fn == "Children" || fn == "Contents")
        {
          int in_only = (fn == "Contents") || contains (lower (args), "in");
          Ctx nc; nc.is_list = 1; nc.keys = objs_children (st, key.c_str (), in_only);
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      /* a property key on the object */
      {
        const char *v = a5state_entity_prop (st, key.c_str (), fn.c_str ());
        if (v != NULL)
          {
            /* if the value is itself an entity key and there's a remainder, walk it */
            if (!rem.empty () && item_kind (st, v) != 0)
              { Ctx nc; nc.keys.push_back (v); return oo_prop (st, nc, rem, depth + 1, ok); }
            return v;
          }
        if (!rem.empty ()) { *ok = 0; return ""; }
        return "0";
      }
    }

  if (kind == 'c')
    {
      const a5_character_t *c = a5model_character (st->adv, key.c_str ());
      if (fn == "Count")        return "1";
      if (fn.empty ())          return key;
      if (fn == "Description")  return item_description (st, key);
      if (fn == "Name")
        { char *n = a5text_character_subjective (st, c);
          std::string r = n ? n : key; free (n); return r; }
      if (fn == "Descriptor")   return c->n_descriptors > 0 ? c->descriptors[0] : "";
      if (fn == "Exits")
        {
          int ci = a5state_character_index (st, key.c_str ());
          const char *cl = (ci >= 0) ? st->char_loc[ci] : NULL;
          Ctx nc; nc.is_dirs = 1;
          if (cl != NULL)
            for (int i = 0; i < 12; i++)
              if (a5restr_exit_in_direction (st, cl, kEnumDirs[i], NULL) != NULL)
                nc.dirs.push_back (kEnumDirs[i]);
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      if (fn == "Held" || fn == "Worn")
        {
          std::string a = lower (args);
          Ctx nc; nc.is_list = 1;
          nc.keys = (fn == "Held") ? objs_held_by (st, key.c_str ())
                                   : objs_worn_by (st, key.c_str ());
          (void) a;
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      if (fn == "WornAndHeld")
        {
          /* clsCharacter WornAndHeld OO step (Global.vb:1350): worn objects
             followed by held objects, as one list. */
          Ctx nc; nc.is_list = 1;
          nc.keys = objs_worn_by (st, key.c_str ());
          std::vector<std::string> held = objs_held_by (st, key.c_str ());
          for (auto &h : held) nc.keys.push_back (h);
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      if (fn == "Location")
        {
          int ci = a5state_character_index (st, key.c_str ());
          Ctx nc; if (ci >= 0 && st->char_loc[ci]) nc.keys.push_back (st->char_loc[ci]);
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      if (fn == "Parent")
        {
          const char *pk = parent_key (st, key.c_str ());
          Ctx nc; if (pk) nc.keys.push_back (pk);
          if (pk == NULL) return "";
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      {
        const char *v = a5state_entity_prop (st, key.c_str (), fn.c_str ());
        if (v != NULL)
          {
            if (!rem.empty () && item_kind (st, v) != 0)
              { Ctx nc; nc.keys.push_back (v); return oo_prop (st, nc, rem, depth + 1, ok); }
            return v;
          }
        if (!rem.empty ()) { *ok = 0; return ""; }
        return "0";
      }
    }

  if (kind == 'l')
    {
      const a5_location_t *l = a5model_location (st->adv, key.c_str ());
      (void) l;
      if (fn.empty ())          return key;
      if (fn == "Name" || fn == "List")
        { char *n = a5text_describe (st, a5xml_child (l->node, "ShortDescription"));
          std::string r = n ? n : ""; free (n); return r; }
      if (fn == "Description")  return item_description (st, key);
      if (fn == "Exits")
        {
          Ctx nc; nc.is_dirs = 1;
          for (int i = 0; i < 12; i++)
            if (loc_has_exit (st, key.c_str (), kEnumDirs[i]))
              nc.dirs.push_back (kEnumDirs[i]);
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      if (fn == "Objects")
        {
          Ctx nc; nc.is_list = 1; nc.keys = objs_in_location (st, key.c_str ());
          return oo_prop (st, nc, rem, depth + 1, ok);
        }
      {
        const char *v = a5state_entity_prop (st, key.c_str (), fn.c_str ());
        if (v != NULL)
          {
            if (!rem.empty () && item_kind (st, v) != 0)
              { Ctx nc; nc.keys.push_back (v); return oo_prop (st, nc, rem, depth + 1, ok); }
            return v;
          }
        if (!rem.empty ()) { *ok = 0; return ""; }
        return "0";
      }
    }

  *ok = 0;
  return "";
}

char *
a5expr_eval (a5_state_t *st, const char *firstkeys, const char *chain)
{
  if (firstkeys == NULL || chain == NULL)
    return NULL;
  Ctx ctx = resolve_first (st, firstkeys);
  if (ctx.keys.empty () && !ctx.is_list)
    return NULL;

  std::string rem = chain;
  while (!rem.empty () && rem[0] == '.') rem.erase (rem.begin ());
  if (rem.empty ())
    return NULL;

  int ok = 1;
  std::string r = oo_prop (st, ctx, rem, 0, &ok);
  if (!ok)
    return NULL;
  return strdup (r.c_str ());
}

/* ------------------------------------------------------- whole-text OO pass */

static int
is_key_char (char c)
{
  return isalnum ((unsigned char) c) || c == '_' || c == '|';
}

/* The byte just past a ".step(args)?..." chain that begins at `dot`. */
static const char *
scan_chain (const char *dot)
{
  const char *r = dot;
  while (*r == '.')
    {
      const char *seg = r + 1;
      if (!isalpha ((unsigned char) *seg))
        break;
      r = seg;
      while (isalnum ((unsigned char) *r) || *r == '_' || *r == '|')
        r++;
      if (*r == '(')
        {
          int depth = 1;
          r++;
          while (*r && depth > 0)
            { if (*r == '(') depth++; else if (*r == ')') depth--; r++; }
        }
    }
  return r;
}

char *
a5expr_replace (a5_state_t *st, const char *text)
{
  std::string out;
  const char *p = text ? text : "";
  while (*p)
    {
      /* a key token starts at a word boundary */
      int boundary = (p == text) || !is_key_char (p[-1]);
      if (boundary && (isalpha ((unsigned char) *p)))
        {
          const char *k = p;
          while (is_key_char (*k)) k++;
          if (*k == '.')
            {
              std::string firstkeys (p, (size_t) (k - p));
              /* require the first pipe-segment to be a real entity key */
              std::string first = firstkeys.substr (0, firstkeys.find ('|'));
              if (item_kind (st, first.c_str ()) != 0)
                {
                  const char *chain_end = scan_chain (k);
                  if (chain_end > k)        /* at least one valid ".step" */
                    {
                      std::string chain (k, (size_t) (chain_end - k));
                      char *v = a5expr_eval (st, firstkeys.c_str (), chain.c_str ());
                      if (v != NULL)
                        { out += v; free (v); p = chain_end; continue; }
                    }
                }
            }
          /* not an expression: copy the whole key token verbatim */
          out.append (p, (size_t) (k - p));
          p = k;
          continue;
        }
      out += *p++;
    }
  return strdup (out.c_str ());
}
