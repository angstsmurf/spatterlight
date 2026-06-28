/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- text engine (Phase 2 MVP).  See a5text.h.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "a5expr.h"
#include "a5restr.h"
#include "a5sexpr.h"
#include "a5text.h"

/* ------------------------------------------------------- string builder */

typedef struct { char *p; size_t len, cap; } sb_t;

static void sb_init (sb_t *b) { b->p = NULL; b->len = b->cap = 0; }

static void
sb_need (sb_t *b, size_t extra)
{
  if (b->len + extra + 1 > b->cap)
    {
      size_t cap = b->cap ? b->cap * 2 : 64;
      while (cap < b->len + extra + 1)
        cap *= 2;
      b->p = (char *) realloc (b->p, cap);
      b->cap = cap;
    }
}

static void
sb_putc (sb_t *b, char c)
{
  sb_need (b, 1);
  b->p[b->len++] = c;
  b->p[b->len] = '\0';
}

static void
sb_puts (sb_t *b, const char *s)
{
  size_t n;
  if (s == NULL)
    return;
  n = strlen (s);
  sb_need (b, n);
  memcpy (b->p + b->len, s, n);
  b->len += n;
  b->p[b->len] = '\0';
}

static char *
sb_finish (sb_t *b)
{
  if (b->p == NULL)
    return strdup ("");
  return b->p;
}

/* ----------------------------------------------------------- small helpers */

static int
streq (const char *a, const char *b)
{
  return a != NULL && b != NULL && strcmp (a, b) == 0;
}

static int
ci_eq (const char *a, const char *b)
{
  if (a == NULL || b == NULL)
    return 0;
  for (; *a && *b; a++, b++)
    if (tolower ((unsigned char) *a) != tolower ((unsigned char) *b))
      return 0;
  return *a == *b;
}

/* Does the trailing run of s look like an OO property expression "X.Prop..."? */
static int
ends_with_oo_expr (const char *s, size_t len)
{
  size_t i = len;
  int saw_dot = 0;
  /* walk back over an OO-expression tail: word chars, '.', '%', '|', '-', '(),' */
  while (i > 0)
    {
      char c = s[i - 1];
      if (isalnum ((unsigned char) c) || c == '_' || c == '%' || c == '|'
          || c == '-' || c == '(' || c == ')' || c == ',' || c == ' ')
        { i--; continue; }
      if (c == '.') { saw_dot = 1; i--; continue; }
      break;
    }
  if (!saw_dot || i >= len)
    return 0;
  /* the run must start with a letter or % (a key / reference) */
  return isalpha ((unsigned char) s[i]) || s[i] == '%';
}

/* Does s end with text that warrants a double space before the next segment? */
static int
add_space (const char *s, size_t len)
{
  char c;
  if (len == 0)
    return 0;
  c = s[len - 1];
  if (c == ' ' || c == '\n')
    return 0;
  if (c == '.' || c == '!' || c == '?' || c == '%')
    return 1;
  /* a trailing OO expression (which will expand at display time) -- frankendrift
     AddSpace's regex rule (Global.vb:3873). */
  return ends_with_oo_expr (s, len);
}

/* ----------------------------------------------------- description -> source */

char *
a5text_eval_description (a5_state_t *st, const a5_xml_node_t *wrapper)
{
  sb_t sb;
  const a5_xml_node_t *c;
  const char *default_text = NULL;
  int first = 1;

  sb_init (&sb);
  if (wrapper == NULL)
    return sb_finish (&sb);

  for (c = wrapper->first_child; c != NULL; c = c->next)
    {
      const a5_xml_node_t *restr;
      const char *when, *text;
      int once;
      if (strcmp (c->name, "Description") != 0)
        continue;
      text = a5xml_child_text (c, "Text");
      if (text == NULL)
        text = "";
      if (first)
        default_text = text;

      /* A <DisplayOnce> segment is suppressed once it has been shown (the start
         room's "soft rain" line in Stone of Wisdom shows at game start, then not
         on a later LOOK) -- clsDescription.ToString: "If Not sd.DisplayOnce
         OrElse Not sd.Displayed". */
      once = streq (a5xml_child_text (c, "DisplayOnce"), "1");
      if (once && a5state_disp_once_seen (st, c))
        {
          first = 0;
          continue;
        }

      restr = a5xml_child (c, "Restrictions");
      if (restr != NULL && !a5restr_pass (st, restr))
        {
          first = 0;
          continue;             /* (Phase 4: show the description's FailMessage) */
        }

      /* Retire the segment only when this is real output, not a value peek
         (Displayed is set unless UserSession.bTestingOutput). */
      if (once && st->marking_display)
        a5state_disp_once_mark (st, c);

      when = a5xml_child_text (c, "DisplayWhen");
      if (streq (when, "StartDescriptionWithThis"))
        {
          sb.len = 0;
          if (sb.p) sb.p[0] = '\0';
          sb_puts (&sb, text);
        }
      else if (streq (when, "StartAfterDefaultDescription"))
        {
          sb.len = 0;
          if (sb.p) sb.p[0] = '\0';
          if (!first)
            {
              sb_puts (&sb, default_text);
              if (add_space (sb.p, sb.len))
                sb_puts (&sb, "  ");
            }
          sb_puts (&sb, text);
        }
      else /* AppendToPreviousDescription (default) */
        {
          if (add_space (sb.p, sb.len))
            sb_puts (&sb, "  ");
          /* The "<>" marker (frankendrift ToString) is a zero-width separator: it
             keeps an unexpanded "%x%.Prop" at the join from merging with the next
             segment's text into one corrupt token.  render_plain drops it. */
          sb_puts (&sb, "<>");
          sb_puts (&sb, text);
        }
      first = 0;
      /* A DisplayOnce segment terminates the description: clsDescription.ToString
         does `Return sb.ToString` inside `If .DisplayOnce`, so once a not-yet-
         shown DisplayOnce segment is output, later segments are NOT considered.
         This is what makes a location's "long first-visit / short thereafter"
         pair work -- the long block is DisplayOnce + StartDescriptionWithThis, so
         a plain StartDescriptionWithThis follow-up only wins once the first has
         been retired. */
      if (once)
        break;
    }

  return sb_finish (&sb);
}

/* --------------------------------------------------------- object naming */

char *
a5text_object_name (const a5_object_t *o, a5_article_t art)
{
  sb_t sb;
  const char *noun = (o->n_names > 0) ? o->names[0] : o->key;
  const char *prefix = o->prefix;

  sb_init (&sb);
  if (art == A5_ART_DEFINITE)
    sb_puts (&sb, "the ");
  else if (art == A5_ART_INDEFINITE && o->article != NULL && o->article[0] != '\0')
    {
      sb_puts (&sb, o->article);
      sb_putc (&sb, ' ');
    }
  if (prefix != NULL && prefix[0] != '\0')
    {
      sb_puts (&sb, prefix);
      sb_putc (&sb, ' ');
    }
  sb_puts (&sb, noun != NULL ? noun : "");
  return sb_finish (&sb);
}

/* ---------------------------------------------------- function replacement */

static char *replace_functions (a5_state_t *st, const char *src);

/* Resolve a function argument (a key or display name) to an object key. */
static const char *
resolve_object_arg (a5_state_t *st, const char *args)
{
  if (args == NULL || args[0] == '\0')
    return NULL;
  if (a5model_object (st->adv, args) != NULL)
    return args;
  for (int i = 0; i < st->adv->n_objects; i++)
    {
      const a5_object_t *o = &st->adv->objects[i];
      for (int j = 0; j < o->n_names; j++)
        {
          if (ci_eq (o->names[j], args)) return o->key;
          if (o->prefix && o->prefix[0])
            { std::string full = std::string (o->prefix) + " " + o->names[j];
              if (ci_eq (full.c_str (), args)) return o->key; }
        }
    }
  return NULL;
}

/* Join a list of object keys as an indefinite "a, b and c" list ("" if empty). */
static char *
list_objects (a5_state_t *st, const std::vector<const char *> &keys)
{
  sb_t sb; sb_init (&sb);
  int n = (int) keys.size ();
  for (int i = 0; i < n; i++)
    {
      const a5_object_t *o = a5model_object (st->adv, keys[i]);
      char *nm = o ? a5text_object_name (o, A5_ART_INDEFINITE) : strdup (keys[i]);
      if (i > 0) sb_puts (&sb, (i == n - 1) ? " and " : ", ");
      sb_puts (&sb, nm);
      free (nm);
    }
  return sb_finish (&sb);
}

/* Objects placed on (want=A5_OWHERE_ON_OBJECT) or in a container object key. */
static std::vector<const char *>
objects_on_in (a5_state_t *st, const char *objkey, a5_owhere_t want)
{
  std::vector<const char *> v;
  for (int i = 0; i < st->adv->n_objects; i++)
    if (st->obj[i].where == want && streq (st->obj[i].key, objkey))
      v.push_back (st->adv->objects[i].key);
  return v;
}

/* Characters on/in an object key. */
static std::vector<const a5_character_t *>
chars_on_in (a5_state_t *st, const char *objkey, int on)
{
  std::vector<const a5_character_t *> v;
  for (int i = 0; i < st->adv->n_characters; i++)
    {
      const a5_character_t *c = &st->adv->characters[i];
      const char *cl = a5state_entity_prop (st, c->key, "CharacterLocation");
      const char *want = on ? "On Object" : "Inside Object";
      const char *whatprop = on ? "OnWhat" : "InsideWhat";
      const char *what = a5state_entity_prop (st, c->key, whatprop);
      if (streq (cl, want) && streq (what, objkey))
        v.push_back (c);
    }
  return v;
}

/*
 * Port of Global.ToProper (bForceRestLower=True): upper-case the first char and
 * lower-case the rest of the string.  Used by DisplayObjectChildren on the
 * on-object list so "The Yellow Note, The Silver Gun and The Silver Bullets"
 * renders as "The yellow note, the silver gun and the silver bullets".
 */
static std::string
to_proper (const std::string &s)
{
  std::string r = s;
  for (size_t i = 0; i < r.size (); i++)
    r[i] = (i == 0) ? (char) toupper ((unsigned char) r[i])
                    : (char) tolower ((unsigned char) r[i]);
  return r;
}

/*
 * Port of clsObject.DisplayObjectChildren: "<on-list> is/are on <the object>[,
 * and inside is/are <in-list>]." — the on-list ToProper-cased, both lists using
 * the objects' own (indefinite) articles, the parent definite.  The inside list
 * is suppressed for a closed openable object.  Returns "" when there is nothing
 * to show (the ListObjectsOnAndIn caller only invokes it for objects that have
 * children, so the "Nothing is on or inside ..." fallback never surfaces here).
 */
static std::string
display_object_children (a5_state_t *st, const char *objkey)
{
  const a5_object_t *o = a5model_object (st->adv, objkey);
  std::vector<const char *> on = objects_on_in (st, objkey, A5_OWHERE_ON_OBJECT);
  std::vector<const char *> in = objects_on_in (st, objkey, A5_OWHERE_IN_OBJECT);

  /* Inside children are only listed when the object is open (or not openable). */
  int openable = o && a5_prop_find (o->props, o->n_props, "Openable") != NULL;
  if (openable)
    {
      const char *os = a5state_entity_prop (st, objkey, "OpenStatus");
      if (os == NULL || !streq (os, "Open"))
        in.clear ();
    }

  std::string s;
  char *defn = o ? a5text_object_name (o, A5_ART_DEFINITE) : strdup (objkey);
  if (!on.empty ())
    {
      char *lst = list_objects (st, on);
      s += to_proper (lst);
      free (lst);
      s += (on.size () == 1) ? " is on " : " are on ";
      s += defn;
    }
  if (!in.empty ())
    {
      if (!on.empty ())
        s += ", and inside";
      else
        { s += "Inside "; s += defn; }
      s += (in.size () == 1) ? " is " : " are ";
      char *lst = list_objects (st, in);
      s += lst;
      free (lst);
    }
  if (!on.empty () || !in.empty ())
    s += ".";
  free (defn);
  return s;
}

/* A character's perspective (FirstPerson=1 / SecondPerson=2 / ThirdPerson=3). */
static int
char_perspective (const a5_character_t *c)
{
  if (c != NULL && c->perspective != NULL)
    {
      if (ci_eq (c->perspective, "FirstPerson"))  return 1;
      if (ci_eq (c->perspective, "ThirdPerson"))  return 3;
    }
  /* NPCs default to third person; the player defaults to second. */
  if (c != NULL && c->type != NULL && ci_eq (c->type, "NonPlayer"))
    return 3;
  return 2;
}

typedef enum { A5_PRO_SUBJ, A5_PRO_OBJ, A5_PRO_POSS, A5_PRO_REFL } a5_pronoun_t;

/*
 * The displayed name of a character with a pronoun, mirroring clsCharacter.Name:
 * first/second person always render as a pronoun (I/you/...); third person
 * renders the proper name (we don't pronoun-replace NPCs without mention
 * tracking, which is the safe default).
 */
static char *
character_name (a5_state_t *st, const a5_character_t *c, a5_pronoun_t pr)
{
  int persp = char_perspective (c);
  (void) st;
  if (persp == 1)
    return strdup (pr == A5_PRO_OBJ ? "me" : pr == A5_PRO_POSS ? "my"
                 : pr == A5_PRO_REFL ? "myself" : "I");
  if (persp == 2)
    return strdup (pr == A5_PRO_POSS ? "your" : pr == A5_PRO_REFL ? "yourself"
                 : "you");
  /* third person: the character's name (possessive adds 's) */
  {
    const char *nm = (c && c->name) ? c->name : "";
    if (pr == A5_PRO_POSS)
      { size_t n = strlen (nm); char *s = (char *) malloc (n + 3);
        memcpy (s, nm, n); s[n] = '\''; s[n + 1] = 's'; s[n + 2] = '\0'; return s; }
    return strdup (nm);
  }
}

char *
a5text_character_subjective (a5_state_t *st, const a5_character_t *c)
{
  return character_name (st, c, A5_PRO_SUBJ);
}

/*
 * Map a bare %reference% name (the token between the percents, already without
 * them) to the resolved entity key(s) for a property expression %name%.Chain.
 * Returns a malloc'd "key" / "key1|key2" string, or NULL if `name` is not an
 * OO-addressable reference or entity key.  Mirrors the %Player%/%objectN%/%objects%
 * -> key substitution frankendrift does before ReplaceOO (Global.vb:1752+).
 */
static char *
oo_firstkey (a5_state_t *st, const char *name)
{
  const char *k = NULL;
  if (ci_eq (name, "player"))
    return strdup ("Player");
  if (ci_eq (name, "objects"))
    k = a5state_lookup_ref (st, "ReferencedObjects");
  else if (ci_eq (name, "object") || ci_eq (name, "object1"))
    k = a5state_lookup_ref (st, "ReferencedObject");
  else if (ci_eq (name, "object2")) k = a5state_lookup_ref (st, "ReferencedObject2");
  else if (ci_eq (name, "object3")) k = a5state_lookup_ref (st, "ReferencedObject3");
  else if (ci_eq (name, "object4")) k = a5state_lookup_ref (st, "ReferencedObject4");
  else if (ci_eq (name, "object5")) k = a5state_lookup_ref (st, "ReferencedObject5");
  else if (ci_eq (name, "characters") || ci_eq (name, "character")
           || ci_eq (name, "character1"))
    k = a5state_lookup_ref (st, "ReferencedCharacter");
  if (k == NULL && (ci_eq (name, "object") || ci_eq (name, "objects")))
    k = a5state_lookup_ref (st, "ReferencedObject");
  if (k != NULL)
    return strdup (k);
  /* a literal entity key (object/character/location/group)? */
  if (a5model_object (st->adv, name) || a5model_character (st->adv, name)
      || a5model_location (st->adv, name))
    return strdup (name);
  for (int i = 0; i < st->adv->n_groups; i++)
    if (streq (st->adv->groups[i].key, name))
      return strdup (name);
  return NULL;
}


/* Evaluate one %Name[args]% (args already function-expanded), or NULL. */
static char *
eval_function (a5_state_t *st, const char *name, const char *args)
{
  int i;

  if (ci_eq (name, "player"))
    {
      const a5_character_t *p = a5model_character (st->adv, "Player");
      return character_name (st, p, A5_PRO_SUBJ);
    }
  if (ci_eq (name, "charactername"))
    {
      /* args may be "<key>" or "<key>, <pronoun>"; default key = Player. */
      std::string a = args ? args : "";
      std::string key = a, pron;
      size_t comma = a.find (',');
      if (comma != std::string::npos)
        { key = a.substr (0, comma); pron = a.substr (comma + 1); }
      /* trim */
      auto trim = [](std::string &s){
        size_t b = s.find_first_not_of (" \t");
        size_t e = s.find_last_not_of (" \t");
        s = (b == std::string::npos) ? "" : s.substr (b, e - b + 1); };
      trim (key); trim (pron);
      if (key.empty ()) key = "Player";
      const a5_character_t *ch = a5model_character (st->adv, key.c_str ());
      if (ch == NULL) ch = a5model_character (st->adv, "Player");
      a5_pronoun_t pr = A5_PRO_SUBJ;
      std::string pl = pron;
      for (char &c : pl) c = (char) tolower ((unsigned char) c);
      if (pl.find ("object") != std::string::npos || pl.find ("target") != std::string::npos)
        pr = A5_PRO_OBJ;
      else if (pl.find ("possess") != std::string::npos) pr = A5_PRO_POSS;
      else if (pl.find ("reflect") != std::string::npos) pr = A5_PRO_REFL;
      return character_name (st, ch, pr);
    }
  if (ci_eq (name, "locationname"))
    {
      const char *key = (args && args[0]) ? args : a5state_player_location (st);
      const a5_location_t *l = key ? a5model_location (st->adv, key) : NULL;
      if (l != NULL)
        {
          char *s = a5text_eval_description (st, a5xml_child (l->node, "ShortDescription"));
          return s;
        }
      return strdup ("");
    }
  if (args != NULL
      && (ci_eq (name, "theobject") || ci_eq (name, "aobject")
          || ci_eq (name, "objectname") || ci_eq (name, "object")
          || ci_eq (name, "theobjects") || ci_eq (name, "aobjects")
          || ci_eq (name, "objects")))
    {
      /* The %TheObject[key]% / %ObjectName[key]% function form (args present);
         bare %object%/%objects% (no args) fall through to the bound-reference
         block below. args is a key, or a resolved object name. */
      const a5_object_t *o = a5model_object (st->adv, args);
      a5_article_t art = (ci_eq (name, "theobject") || ci_eq (name, "theobjects"))
                           ? A5_ART_DEFINITE
                       : (ci_eq (name, "aobject") || ci_eq (name, "aobjects"))
                           ? A5_ART_INDEFINITE
                           : A5_ART_NONE;
      if (o == NULL && args != NULL && args[0] != '\0')
        {
          int k;
          for (k = 0; k < st->adv->n_objects; k++)
            {
              const a5_object_t *cand = &st->adv->objects[k];
              int j;
              for (j = 0; j < cand->n_names; j++)
                if (ci_eq (cand->names[j], args)) { o = cand; break; }
              if (o != NULL) break;
            }
        }
      if (o != NULL)
        return a5text_object_name (o, art);
      return strdup (args ? args : "");
    }
  if (ci_eq (name, "propertyvalue") && args != NULL)
    {
      /* %PropertyValue[entity, propkey]% -> the entity's value for that property.
         (Global.vb:2322 literally loops every object/character/location, but in
         practice the call is always made for the single bound entity whose
         property is in context -- the observed Runner/FrankenDrift output is just
         that one entity's value, so resolve arg0 to a key and read it.)  A Text
         property stores its value as a rich <Description> block (value_node), so
         render that; otherwise return the scalar. */
      std::string a = args;
      size_t comma = a.find (',');
      if (comma == std::string::npos)
        return strdup ("");
      std::string ent = a.substr (0, comma), prop = a.substr (comma + 1);
      auto strip = [](std::string &s){
        s.erase (std::remove (s.begin (), s.end (), ' '), s.end ()); };
      strip (prop);
      /* arg0 may be a key or a display name; map to a key. */
      const char *ekey = resolve_object_arg (st, ent.c_str ());
      const a5_prop_t *props = NULL; int n_props = 0;
      const a5_object_t *o = ekey ? a5model_object (st->adv, ekey) : NULL;
      if (o != NULL) { props = o->props; n_props = o->n_props; }
      else {
        const a5_character_t *c = ekey ? a5model_character (st->adv, ekey) : NULL;
        if (c != NULL) { props = c->props; n_props = c->n_props; }
        else {
          const a5_location_t *l = ekey ? a5model_location (st->adv, ekey) : NULL;
          if (l != NULL) { props = l->props; n_props = l->n_props; }
        }
      }
      const a5_prop_t *pr = props ? a5_prop_find (props, n_props, prop.c_str ()) : NULL;
      if (pr == NULL)
        return strdup ("");
      if (pr->value_node != NULL)
        return a5text_eval_description (st, pr->value_node);
      return strdup (pr->value ? pr->value : "");
    }
  if (ci_eq (name, "parentof") && args != NULL)
    {
      /* args is a key or a display name; emit the parent's key. */
      const a5_object_t *o = a5model_object (st->adv, args);
      const char *k = NULL;
      if (o == NULL)
        for (i = 0; i < st->adv->n_objects; i++)
          { const a5_object_t *cand = &st->adv->objects[i];
            for (int j = 0; j < cand->n_names; j++)
              {
                if (ci_eq (cand->names[j], args)) { o = cand; break; }
                /* also match "<prefix> <noun>" (the display form %objects% yields) */
                if (cand->prefix && cand->prefix[0])
                  { std::string full = std::string (cand->prefix) + " " + cand->names[j];
                    if (ci_eq (full.c_str (), args)) { o = cand; break; } }
              }
            if (o != NULL) break; }
      if (o != NULL)
        { char *pp = a5expr_eval (st, o->key, ".Parent"); if (pp) return pp; k = ""; }
      return strdup (k ? k : "");
    }
  if (args != NULL
      && (ci_eq (name, "listobjectson") || ci_eq (name, "listobjectsin")))
    {
      /* Bare indefinite-article list of the children on / in the object,
         mirroring Children(On|Inside).List(, , Indefinite). */
      const char *k = resolve_object_arg (st, args);
      a5_owhere_t want = ci_eq (name, "listobjectson") ? A5_OWHERE_ON_OBJECT
                                                       : A5_OWHERE_IN_OBJECT;
      std::vector<const char *> v;
      if (k != NULL) v = objects_on_in (st, k, want);
      return list_objects (st, v);
    }
  if (args != NULL && ci_eq (name, "listobjectsonandin"))
    {
      /* clsUserSession.ListObjectsOnAndIn: ignore the argument and concatenate
         DisplayObjectChildren for every object with on/in children (joined by
         pSpace's two spaces). */
      std::string out;
      for (int oi = 0; oi < st->adv->n_objects; oi++)
        {
          const char *ok = st->adv->objects[oi].key;
          int has_on = !objects_on_in (st, ok, A5_OWHERE_ON_OBJECT).empty ();
          int has_in = !objects_on_in (st, ok, A5_OWHERE_IN_OBJECT).empty ();
          if (st->adv->n_objects != 1 && !has_on && !has_in)
            continue;
          std::string d = display_object_children (st, ok);
          if (d.empty ())
            continue;
          if (!out.empty ())
            out += "  ";
          out += d;
        }
      return strdup (out.c_str ());
    }
  if (args != NULL
      && (ci_eq (name, "listcharacterson") || ci_eq (name, "listcharactersin")
          || ci_eq (name, "listcharactersonandin")))
    {
      const char *k = resolve_object_arg (st, args);
      const a5_object_t *o = k ? a5model_object (st->adv, k) : NULL;
      std::vector<const a5_character_t *> on, in;
      if (k != NULL && !ci_eq (name, "listcharactersin")) on = chars_on_in (st, k, 1);
      if (k != NULL && !ci_eq (name, "listcharacterson")) in = chars_on_in (st, k, 0);
      if (on.empty () && in.empty ())
        return strdup ("");
      /* DisplayCharacterChildren: "X is on/inside the <object>." */
      std::string out;
      char *defn = o ? a5text_object_name (o, A5_ART_DEFINITE) : strdup (k ? k : "");
      auto names = [&](std::vector<const a5_character_t *> &cs) {
        std::string s; int n = (int) cs.size ();
        for (int i = 0; i < n; i++)
          { char *nm = a5text_character_subjective (st, cs[i]);
            if (i > 0) s += (i == n - 1) ? " and " : ", ";
            s += nm; free (nm); }
        return s; };
      if (!on.empty ())
        { out += names (on); out += (on.size () == 1) ? " is on " : " are on ";
          out += defn; }
      if (!in.empty ())
        { if (!on.empty ()) out += ", and " + names (in);
          else out += names (in);
          out += (in.size () == 1) ? " is " : " are ";
          out += "inside "; out += defn; }
      out += ".";
      free (defn);
      return strdup (out.c_str ());
    }
  if (args != NULL && (ci_eq (name, "listheld") || ci_eq (name, "listworn")))
    {
      const a5_character_t *c = a5model_character (st->adv, args);
      std::vector<const char *> v;
      if (c != NULL)
        {
          a5_owhere_t want = ci_eq (name, "listheld") ? A5_OWHERE_HELD_BY
                                                      : A5_OWHERE_WORN_BY;
          for (int i = 0; i < st->adv->n_objects; i++)
            if (st->obj[i].where == want && streq (st->obj[i].key, c->key))
              v.push_back (st->adv->objects[i].key);
        }
      if (v.empty ()) return strdup ("nothing");
      return list_objects (st, v);
    }
  if (args != NULL && ci_eq (name, "listobjectsatlocation"))
    {
      std::vector<const char *> v;
      for (int i = 0; i < st->adv->n_objects; i++)
        if (a5state_object_at_location (st, i, args, 1))
          v.push_back (st->adv->objects[i].key);
      return list_objects (st, v);
    }
  if (ci_eq (name, "numberastext") && args != NULL)
    {
      static const char *ones[] = { "zero","one","two","three","four","five",
        "six","seven","eight","nine","ten","eleven","twelve" };
      long n = strtol (args, NULL, 10);
      if (n >= 0 && n <= 12) return strdup (ones[n]);
      return strdup (args);
    }
  if (ci_eq (name, "pcase") || ci_eq (name, "ucase") || ci_eq (name, "lcase"))
    {
      char *s = strdup (args ? args : "");
      if (ci_eq (name, "ucase"))
        for (i = 0; s[i]; i++) s[i] = toupper ((unsigned char) s[i]);
      else if (ci_eq (name, "lcase"))
        for (i = 0; s[i]; i++) s[i] = tolower ((unsigned char) s[i]);
      else if (s[0])
        s[0] = toupper ((unsigned char) s[0]);
      return s;
    }

  /* A bound reference produced by the parser this turn (%direction%, %text%). */
  if (args == NULL)
    {
      const char *bound = NULL;
      if (ci_eq (name, "direction") || ci_eq (name, "direction1"))
        bound = a5state_lookup_ref (st, "ReferencedDirection");
      else if (ci_eq (name, "text") || ci_eq (name, "text1"))
        bound = a5state_lookup_ref (st, "ReferencedText");
      else if (ci_eq (name, "number") || ci_eq (name, "number1"))
        bound = a5state_lookup_ref (st, "ReferencedNumber");
      else if (ci_eq (name, "object") || ci_eq (name, "objects")
               || ci_eq (name, "object1") || ci_eq (name, "object2"))
        {
          /* the bound object('s display name) */
          const char *ref = ci_eq (name, "object2") ? "ReferencedObject2"
                                                     : "ReferencedObject";
          const char *key = a5state_lookup_ref (st, ref);
          const a5_object_t *o = key ? a5model_object (st->adv, key) : NULL;
          if (o != NULL)
            return a5text_object_name (o, A5_ART_NONE);
        }
      else if (ci_eq (name, "character") || ci_eq (name, "characters")
               || ci_eq (name, "character1"))
        {
          const char *key = a5state_lookup_ref (st, "ReferencedCharacter");
          const a5_character_t *ch = key ? a5model_character (st->adv, key) : NULL;
          if (ch != NULL && ch->name != NULL)
            return strdup (ch->name);
        }
      if (bound != NULL)
        return strdup (bound);
    }

  /* A bare %VariableName% (or %Score% etc.). */
  if (args == NULL)
    {
      for (i = 0; i < st->adv->n_variables; i++)
        if (ci_eq (st->adv->variables[i].name, name))
          {
            if (streq (st->adv->variables[i].type, "Text"))
              return strdup (st->var_text[i] ? st->var_text[i] : "");
            else
              {
                char buf[32];
                snprintf (buf, sizeof buf, "%ld", st->var_num[i]);
                return strdup (buf);
              }
          }
    }

  return NULL;                  /* unhandled: leave the token verbatim */
}

/* Replace %function%/%variable% tokens in src (single pass; args recurse). */
static char *
replace_functions (a5_state_t *st, const char *src)
{
  sb_t sb;
  const char *p = src;

  sb_init (&sb);
  while (*p != '\0')
    {
      if (*p == '%')
        {
          const char *q = p + 1;
          const char *name_start = q;
          char *name, *args = NULL, *value = NULL;
          int ok = 0;

          while (*q && (isalnum ((unsigned char) *q) || *q == '_'))
            q++;
          if (q > name_start)
            {
              size_t nlen = (size_t) (q - name_start);
              if (*q == '%')
                {
                  name = strndup (name_start, nlen);
                  /* %reference%.Property...  : emit the resolved entity key and
                     leave the ".chain" as literal text for the later OO pass
                     (mirrors frankendrift substituting %object%->key before
                     ReplaceOO, so embedded %Func[]% are gone by then). */
                  if (q[1] == '.')
                    {
                      char *fk = oo_firstkey (st, name);
                      if (fk != NULL)
                        {
                          sb_puts (&sb, fk);
                          free (fk); free (name);
                          p = q + 1;        /* past the closing %, before the '.' */
                          continue;
                        }
                    }
                  value = eval_function (st, name, NULL);
                  free (name);
                  q++;
                  ok = 1;
                }
              else if (*q == '[')
                {
                  int depth = 1;
                  const char *a = q + 1;
                  const char *astart = a;
                  while (*a && depth > 0)
                    {
                      if (*a == '[') depth++;
                      else if (*a == ']') depth--;
                      if (depth > 0) a++;
                    }
                  if (*a == ']' && a[1] == '%')
                    {
                      char *rawargs = strndup (astart, (size_t) (a - astart));
                      name = strndup (name_start, nlen);
                      args = replace_functions (st, rawargs);
                      value = eval_function (st, name, args);
                      free (rawargs);
                      free (args);
                      free (name);
                      q = a + 2;
                      ok = 1;
                    }
                }
            }

          if (ok)
            {
              if (value != NULL)
                {
                  sb_puts (&sb, value);
                  free (value);
                }
              else
                {
                  /* leave the original token verbatim */
                  sb_need (&sb, (size_t) (q - p));
                  memcpy (sb.p + sb.len, p, (size_t) (q - p));
                  sb.len += (size_t) (q - p);
                  sb.p[sb.len] = '\0';
                }
              p = q;
              continue;
            }
        }
      sb_putc (&sb, *p);
      p++;
    }
  return sb_finish (&sb);
}

/* --------------------------------------------------------- ALR replacement */

static char *process_inner (a5_state_t *st, const char *src, int depth);

/* Replace every occurrence of `find` in `src` with `repl`. */
static char *
str_replace_all (const char *src, const char *find, const char *repl)
{
  sb_t sb;
  size_t flen = strlen (find);
  const char *p = src;
  sb_init (&sb);
  if (flen == 0)
    return strdup (src);
  while (*p)
    {
      const char *hit = strstr (p, find);
      if (hit == NULL)
        {
          sb_puts (&sb, p);
          break;
        }
      sb_need (&sb, (size_t) (hit - p));
      memcpy (sb.p + sb.len, p, (size_t) (hit - p));
      sb.len += (size_t) (hit - p);
      sb.p[sb.len] = '\0';
      sb_puts (&sb, repl);
      p = hit + flen;
    }
  return sb_finish (&sb);
}

static char *
replace_alrs (a5_state_t *st, const char *src, int depth)
{
  char *cur = strdup (src);
  int i;
  if (depth > 8)
    return cur;
  for (i = 0; i < st->adv->n_alrs; i++)
    {
      const a5_alr_t *alr = &st->adv->alrs[i];
      if (alr->old_text == NULL || alr->old_text[0] == '\0')
        continue;
      if (strstr (cur, alr->old_text) != NULL)
        {
          char *raw = a5text_eval_description (st, alr->new_text);
          char *val = process_inner (st, raw, depth + 1);
          if (!streq (cur, val))     /* guard against self-referential ALRs */
            {
              char *next = str_replace_all (cur, alr->old_text, val);
              free (cur);
              cur = next;
            }
          free (raw);
          free (val);
        }
    }
  return cur;
}

/* ------------------------------------------------------ auto-capitalisation */

static int
is_sentence_start (const char *s, size_t i)
{
  if (i == 0)
    return 1;
  if (s[i - 1] == '\n')
    return 1;
  /* "x. y" or "x.  y" where x is a letter */
  if (i >= 2 && s[i - 1] == ' '
      && (s[i - 2] == '.' || s[i - 2] == '!' || s[i - 2] == '?'))
    return 1;
  if (i >= 3 && s[i - 1] == ' ' && s[i - 2] == ' '
      && (s[i - 3] == '.' || s[i - 3] == '!' || s[i - 3] == '?'))
    return 1;
  return 0;
}

static char *
auto_capitalise (const char *src)
{
  char *s = strdup (src);
  size_t i;
  for (i = 0; s[i]; i++)
    if (islower ((unsigned char) s[i]) && is_sentence_start (s, i))
      s[i] = toupper ((unsigned char) s[i]);
  return s;
}

/* --------------------------------------------------- perspective conjugation */

/* Player perspective -> index into a [1st/2nd/3rd] verb-ending bracket. */
static int
perspective_index (a5_state_t *st)
{
  const a5_character_t *p = a5model_character (st->adv, "Player");
  if (p != NULL && p->perspective != NULL)
    {
      if (ci_eq (p->perspective, "FirstPerson"))  return 0;
      if (ci_eq (p->perspective, "ThirdPerson"))  return 2;
    }
  return 1;                       /* SecondPerson (ADRIFT default) */
}

/*
 * Resolve ADRIFT verb-conjugation brackets: "move[//s]" -> "move"/"moves" by the
 * player's perspective.  Only a "[a/b/c]" group (slash-separated, no nested
 * markup) is touched; everything else passes through.  Mirrors the perspective
 * half of frankendrift Global.ReplaceFunctions / _FrankenDrift_Pronouns.
 */
static char *
resolve_perspective (a5_state_t *st, const char *src)
{
  sb_t sb;
  const char *p = src;
  int idx = perspective_index (st);

  sb_init (&sb);
  while (*p != '\0')
    {
      if (*p == '[')
        {
          const char *q = p + 1;
          int has_slash = 0;
          while (*q && *q != ']' && *q != '[')
            { if (*q == '/') has_slash = 1; q++; }
          if (*q == ']' && has_slash)
            {
              /* split p+1..q by '/', pick option idx (clamp to last). */
              const char *s = p + 1;
              int opt = 0;
              const char *opt_start = s;
              const char *chosen_a = NULL, *chosen_b = NULL;
              while (s <= q)
                {
                  if (s == q || *s == '/')
                    {
                      if (opt == idx) { chosen_a = opt_start; chosen_b = s; }
                      opt++;
                      opt_start = s + 1;
                    }
                  s++;
                }
              if (chosen_a == NULL)       /* fewer options than idx: take last */
                { /* recompute last */
                  const char *r = p + 1; const char *ls = r;
                  while (r <= q) { if (r == q || *r == '/') { chosen_a = ls; chosen_b = r; ls = r + 1; } r++; }
                }
              if (chosen_a != NULL)
                for (const char *c = chosen_a; c < chosen_b; c++)
                  sb_putc (&sb, *c);
              p = q + 1;
              continue;
            }
        }
      sb_putc (&sb, *p);
      p++;
    }
  return sb_finish (&sb);
}

/* ------------------------------------------------- embedded <#...#> expressions */

/* A signed decimal integer? (frankendrift's bIntValue -- expression results
   that are pure integers are left unquoted; everything else is quoted.) */
static int
is_signed_int (const char *s)
{
  if (s == NULL || *s == '\0')
    return 0;
  const char *p = s;
  if (*p == '+' || *p == '-') p++;
  if (*p == '\0') return 0;
  for (; *p; p++)
    if (!isdigit ((unsigned char) *p))
      return 0;
  return 1;
}

/* End of a ".step(args)..." property chain that starts at `dot` (mirrors the
   scan in a5expr_replace). */
static const char *
sexpr_scan_chain (const char *dot)
{
  const char *r = dot;
  while (*r == '.')
    {
      const char *seg = r + 1;
      if (!isalpha ((unsigned char) *seg))
        break;
      r = seg;
      while (isalnum ((unsigned char) *r) || *r == '_' || *r == '|') r++;
      if (*r == '(')
        { int d = 1; r++; while (*r && d > 0) { if (*r == '(') d++; else if (*r == ')') d--; r++; } }
    }
  return r;
}

/* Emit a substituted value into `sb`, quoting it (frankendrift's bExpression
   path) unless it is a bare integer or we are already inside a quoted literal. */
static void
sb_quote_val (sb_t *sb, const char *val, int in_quote)
{
  if (in_quote || is_signed_int (val))
    sb_puts (sb, val);
  else
    { sb_putc (sb, '"'); sb_puts (sb, val); sb_putc (sb, '"'); }
}

/*
 * Substitute the %references%, %variables% and OO property-expressions inside a
 * `<#...#>` expression body, quoting non-numeric results so the evaluator sees a
 * self-contained expression.  Ports frankendrift's ReplaceFunctions(expr, True)
 * + ReplaceOO(bExpression:=True) (Global.vb:639-647): a substituted value is
 * wrapped in double quotes unless it is an integer or already lies between
 * quotes in the source.
 */
static char *
expr_substitute (a5_state_t *st, const char *src)
{
  sb_t sb;
  const char *p = src;
  int in_quote = 0;

  sb_init (&sb);
  while (*p != '\0')
    {
      if (*p == '"')
        { in_quote = !in_quote; sb_putc (&sb, '"'); p++; continue; }
      if (*p == '%')
        {
          const char *q = p + 1;
          const char *name_start = q;
          while (*q && (isalnum ((unsigned char) *q) || *q == '_')) q++;
          if (q > name_start && *q == '%')
            {
              size_t nlen = (size_t) (q - name_start);
              char *name = strndup (name_start, nlen);
              /* %reference%.Property... : resolve the OO chain (quoted) */
              if (q[1] == '.')
                {
                  char *fk = oo_firstkey (st, name);
                  if (fk != NULL)
                    {
                      const char *chain_end = sexpr_scan_chain (q + 1);
                      char *chain = strndup (q + 1, (size_t) (chain_end - (q + 1)));
                      char *val = a5expr_eval (st, fk, chain);
                      sb_quote_val (&sb, val ? val : "", in_quote);
                      free (val); free (chain); free (fk); free (name);
                      p = chain_end;
                      continue;
                    }
                }
              /* plain %name% */
              {
                char *val = eval_function (st, name, NULL);
                free (name);
                if (val != NULL)
                  { sb_quote_val (&sb, val, in_quote); free (val); p = q + 1; continue; }
              }
              /* unresolved: leave the %name% token verbatim */
              sb_need (&sb, (size_t) (q + 1 - p));
              memcpy (sb.p + sb.len, p, (size_t) (q + 1 - p));
              sb.len += (size_t) (q + 1 - p);
              sb.p[sb.len] = '\0';
              p = q + 1;
              continue;
            }
          if (q > name_start && *q == '[')
            {
              int depth = 1;
              const char *a = q + 1;
              const char *astart = a;
              while (*a && depth > 0)
                { if (*a == '[') depth++; else if (*a == ']') depth--; if (depth > 0) a++; }
              if (*a == ']' && a[1] == '%')
                {
                  char *rawargs = strndup (astart, (size_t) (a - astart));
                  char *name = strndup (name_start, (size_t) (q - name_start));
                  char *args = replace_functions (st, rawargs);
                  char *val = eval_function (st, name, args);
                  free (rawargs); free (args); free (name);
                  if (val != NULL)
                    { sb_quote_val (&sb, val, in_quote); free (val); p = a + 2; continue; }
                  /* unresolved: leave verbatim */
                  sb_need (&sb, (size_t) (a + 2 - p));
                  memcpy (sb.p + sb.len, p, (size_t) (a + 2 - p));
                  sb.len += (size_t) (a + 2 - p);
                  sb.p[sb.len] = '\0';
                  p = a + 2;
                  continue;
                }
            }
          /* a lone '%' */
          sb_putc (&sb, '%');
          p++;
          continue;
        }
      sb_putc (&sb, *p);
      p++;
    }
  return sb_finish (&sb);
}

/* Evaluate every `<#...#>` expression in `src` (frankendrift ReplaceExpressions:
   substitute the body, then reduce it to its string value). */
static char *
replace_expressions (a5_state_t *st, const char *src)
{
  sb_t sb;
  const char *p = src;

  if (strstr (src, "<#") == NULL)
    return strdup (src);

  sb_init (&sb);
  while (*p != '\0')
    {
      if (p[0] == '<' && p[1] == '#')
        {
          const char *end = strstr (p + 2, "#>");
          if (end != NULL)
            {
              char *inner = strndup (p + 2, (size_t) (end - (p + 2)));
              char *sub = expr_substitute (st, inner);
              char *val = a5_eval_sexpr (sub);
              sb_puts (&sb, val);
              free (inner); free (sub); free (val);
              p = end + 2;
              continue;
            }
        }
      sb_putc (&sb, *p);
      p++;
    }
  return sb_finish (&sb);
}

/* Swap each `<#...#>` for a \x01<n>\x01 sentinel so the %function%/OO passes
   leave the expression body untouched (frankendrift guards them behind GUIDs);
   the sentinel survives both passes verbatim and is restored before
   replace_expressions evaluates the bodies. */
static std::string
protect_exprs (const char *src, std::vector<std::string> &saved)
{
  std::string out;
  const char *p = src;
  while (*p != '\0')
    {
      if (p[0] == '<' && p[1] == '#')
        {
          const char *end = strstr (p + 2, "#>");
          if (end != NULL)
            {
              char buf[24];
              snprintf (buf, sizeof buf, "\x01%d\x01", (int) saved.size ());
              saved.push_back (std::string (p, (size_t) (end + 2 - p)));
              out += buf;
              p = end + 2;
              continue;
            }
        }
      out += *p++;
    }
  return out;
}

static std::string
restore_exprs (const char *src, const std::vector<std::string> &saved)
{
  std::string out;
  const char *p = src;
  while (*p != '\0')
    {
      if (*p == '\x01')
        {
          const char *q = p + 1;
          while (isdigit ((unsigned char) *q)) q++;
          if (*q == '\x01' && q > p + 1)
            {
              int idx = atoi (p + 1);
              if (idx >= 0 && idx < (int) saved.size ())
                out += saved[idx];
              p = q + 1;
              continue;
            }
        }
      out += *p++;
    }
  return out;
}

/* ------------------------------------------------------------- process core */

static char *
process_inner (a5_state_t *st, const char *src, int depth)
{
  std::vector<std::string> saved;
  std::string prot = protect_exprs (src, saved);
  char *funcs = replace_functions (st, prot.c_str ());
  char *oo = a5expr_replace (st, funcs);    /* resolve key.Property expressions */
  std::string restored = restore_exprs (oo, saved);
  char *exprs = replace_expressions (st, restored.c_str ());  /* <#...#> */
  char *alrs;
  free (funcs);
  free (oo);
  if (depth > 8)
    return exprs;
  alrs = replace_alrs (st, exprs, depth);
  free (exprs);
  return alrs;
}

char *
a5text_process (a5_state_t *st, const char *src)
{
  char *inner = process_inner (st, src, 0);
  char *persp = resolve_perspective (st, inner);
  char *capped = auto_capitalise (persp);
  free (inner);
  free (persp);
  return capped;
}

/* ----------------------------------------------------------- plain renderer */

char *
a5text_render_plain (const char *src)
{
  sb_t sb;
  const char *p = src;
  sb_init (&sb);

  while (*p != '\0')
    {
      if (*p == '<')
        {
          const char *q = p + 1;
          char tag[16];
          size_t tl = 0;
          while (*q && *q != '>')
            {
              if (tl + 1 < sizeof tag)
                tag[tl++] = (char) tolower ((unsigned char) *q);
              q++;
            }
          tag[tl] = '\0';
          if (*q == '>')
            q++;
          if (strcmp (tag, "br") == 0)
            sb_putc (&sb, '\n');
          else if (strcmp (tag, "cls") == 0)
            {
              /* Screen clear: drop everything buffered so far, mirroring the
                 GlkHtmlWin / FrankenDrift.Headless handling of <cls>.  An Anno
                 1700-style intro ends with a <cls>, so its credits/preamble are
                 wiped and only the post-clear text (here, none) survives. */
              sb.len = 0;
              if (sb.p != NULL) sb.p[0] = '\0';
            }
          /* every other tag (<>, <c>, </c>, <b>, <i>, <font...>, <waitkey>...) drops */
          p = q;
          continue;
        }
      if (*p == '&')
        {
          if (strncmp (p, "&perc;", 6) == 0) { sb_putc (&sb, '%'); p += 6; continue; }
          if (strncmp (p, "&amp;", 5) == 0)  { sb_putc (&sb, '&'); p += 5; continue; }
          if (strncmp (p, "&lt;", 4) == 0)   { sb_putc (&sb, '<'); p += 4; continue; }
          if (strncmp (p, "&gt;", 4) == 0)   { sb_putc (&sb, '>'); p += 4; continue; }
          if (strncmp (p, "&quot;", 6) == 0) { sb_putc (&sb, '"'); p += 6; continue; }
        }
      sb_putc (&sb, *p);
      p++;
    }
  return sb_finish (&sb);
}

char *
a5text_describe (a5_state_t *st, const a5_xml_node_t *wrapper)
{
  char *raw = a5text_eval_description (st, wrapper);
  char *proc = a5text_process (st, raw);
  char *plain = a5text_render_plain (proc);
  free (raw);
  free (proc);
  return plain;
}

/* ----------------------------------------------------------- LOOK / location */

/* Get an object's "list description" text (static/dynamic), or NULL. */
static char *
object_list_desc (a5_state_t *st, const a5_object_t *o, int is_static)
{
  const char *key = is_static ? "ListDescription" : "ListDescriptionDynamic";
  const a5_prop_t *prop = a5_prop_find (o->props, o->n_props, key);
  if (prop == NULL)
    return NULL;
  if (prop->value_node != NULL)
    return a5text_eval_description (st, prop->value_node);
  if (prop->value != NULL && prop->value[0] != '\0')
    return strdup (prop->value);
  return NULL;
}

char *
a5text_view_location (a5_state_t *st)
{
  const char *lockey = a5state_player_location (st);
  const a5_location_t *loc;
  sb_t sb;
  char *plain;
  int i, listed = 0;

  sb_init (&sb);
  if (lockey == NULL)
    return strdup ("");
  loc = a5model_location (st->adv, lockey);
  if (loc == NULL)
    return strdup ("");

  /* A v5 location view is prefixed with a blank line before the room name
     (clsLocation.ViewLocation: "If Adventure.dVersion >= 5 Then sView = vbCrLf
     & sView"), so after a "> look" echo the room name has a paragraph break. */
  sb_putc (&sb, '\n');

  /* Room name (bold), then the long description. */
  {
    char *name = a5text_describe (st, a5xml_child (loc->node, "ShortDescription"));
    sb_puts (&sb, name);
    sb_putc (&sb, '\n');
    free (name);
  }
  {
    /* The location view is real output: retire any <DisplayOnce> segments it
       shows so they do not reappear on a later LOOK. */
    int prev_mark = st->marking_display;
    char *raw, *proc;
    st->marking_display = 1;
    raw = a5text_eval_description (st, a5xml_child (loc->node, "LongDescription"));
    st->marking_display = prev_mark;
    proc = a5text_process (st, raw);
    free (raw);
    /* Special-listed objects directly here. */
    for (i = 0; i < st->adv->n_objects; i++)
      {
        const a5_object_t *o = &st->adv->objects[i];
        int is_static = st->obj[i].is_static;
        int include = (!is_static && !a5_prop_find (o->props, o->n_props, "ExplicitlyExclude"))
                    || (is_static && a5_prop_find (o->props, o->n_props, "ExplicitlyList"));
        char *ld;
        if (!include || !a5state_object_at_location (st, i, lockey, 1))
          continue;
        ld = object_list_desc (st, o, is_static);
        if (ld != NULL && ld[0] != '\0')
          {
            char *p = process_inner (st, ld, 0);
            size_t plen = strlen (proc);
            /* pSpace: two spaces unless the previous text ends with a newline. */
            const char *sep = (plen == 0 || proc[plen - 1] == '\n') ? "" : "  ";
            size_t need = plen + 2 + strlen (p) + 1;
            char *joined = (char *) malloc (need);
            snprintf (joined, need, "%s%s%s", proc, sep, p);
            free (proc);
            proc = joined;
            free (p);
          }
        free (ld);
      }
    sb_puts (&sb, proc);
    free (proc);
  }

  /* General listed objects ("Also here is ..."). */
  {
    const char **names = (const char **) malloc (sizeof (char *)
                          * (size_t) (st->adv->n_objects + 1));
    char **owned = (char **) malloc (sizeof (char *)
                          * (size_t) (st->adv->n_objects + 1));
    int n = 0;
    for (i = 0; i < st->adv->n_objects; i++)
      {
        const a5_object_t *o = &st->adv->objects[i];
        int is_static = st->obj[i].is_static;
        int include = (!is_static && !a5_prop_find (o->props, o->n_props, "ExplicitlyExclude"))
                    || (is_static && a5_prop_find (o->props, o->n_props, "ExplicitlyList"));
        char *ld;
        if (!include || !a5state_object_at_location (st, i, lockey, 1))
          continue;
        ld = object_list_desc (st, o, is_static);
        if (ld != NULL && ld[0] != '\0') { free (ld); continue; } /* special-listed */
        free (ld);
        owned[n] = a5text_object_name (o, A5_ART_INDEFINITE);
        names[n] = owned[n];
        n++;
      }
    if (n > 0)
      {
        sb_puts (&sb, "  Also here is ");
        for (i = 0; i < n; i++)
          {
            if (i > 0)
              sb_puts (&sb, (i == n - 1) ? " and " : ", ");
            sb_puts (&sb, names[i]);
          }
        sb_putc (&sb, '.');
        listed = 1;
      }
    for (i = 0; i < n; i++)
      free (owned[i]);
    free (owned);
    free ((void *) names);
  }
  (void) listed;

  plain = a5text_render_plain (sb.p ? sb.p : "");
  free (sb.p);
  return plain;
}
