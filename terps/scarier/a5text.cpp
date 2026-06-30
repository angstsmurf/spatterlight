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
#include <utility>
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
          /* clsDescription.ToString does `Return sb.ToString` inside
             `If .DisplayOnce` *regardless* of whether the restrictions passed
             (it only sets Displayed=True when they did).  So a not-yet-shown
             DisplayOnce segment whose restrictions FAIL still terminates the
             loop -- later segments are not considered -- but is left un-retired
             so it can show on a future turn.  (Anno's Room 101 has two identical
             "narrow opening" appends straddling a DisplayOnce faint segment; FD
             shows the line once because the failing faint segment returns before
             the second append.)  */
          if (once)
            break;
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

static char *replace_functions (a5_state_t *st, const char *src, int as_arg = 0);
static char *view_location_impl (a5_state_t *st, const char *lockey);

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

/* Join a list of object keys as an "a, b and c" list with the given article
   ("" if empty).  Mirrors ObjectHashTable.List (StronglyTypedCollections.vb:183). */
static char *
list_objects_art (a5_state_t *st, const std::vector<const char *> &keys,
                  a5_article_t art)
{
  sb_t sb; sb_init (&sb);
  int n = (int) keys.size ();
  for (int i = 0; i < n; i++)
    {
      const a5_object_t *o = a5model_object (st->adv, keys[i]);
      char *nm = o ? a5text_object_name (o, art) : strdup (keys[i]);
      if (i > 0) sb_puts (&sb, (i == n - 1) ? " and " : ", ");
      sb_puts (&sb, nm);
      free (nm);
    }
  return sb_finish (&sb);
}

/* Join a list of object keys as an indefinite "a, b and c" list ("" if empty). */
static char *
list_objects (a5_state_t *st, const std::vector<const char *> &keys)
{
  return list_objects_art (st, keys, A5_ART_INDEFINITE);
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
  /* clsObject.DisplayObjectChildren: nothing on or inside -> the canned line
     (only surfaced by ListObjectsOnAndIn when the resolved set is a single
     childless object). */
  if (s.empty ())
    { s = "Nothing is on or inside "; s += defn ? defn : objkey; s += "."; }
  free (defn);
  return s;
}

/*
 * Port of ObjectHashTable.List(sep, bIncludeSubObjects:=True, Indefinite)
 * (StronglyTypedCollections.vb:183) -- the indefinite main list followed, for
 * each object with on/inside children, by its DisplayObjectChildren-style block
 * joined with ".  " and *no* trailing period.  Used by %ListWorn% / %ListHeld%
 * (which both pass bIncludeSubObjects=True), so e.g. examining yourself shows
 * "... a scabbard and your clothes.  Inside the backpack are three food rations.
 * Inside the scabbard is a long sword".
 */
static char *
list_objects_subobj (a5_state_t *st, const std::vector<const char *> &keys)
{
  char *main = list_objects (st, keys);
  std::string out = main ? main : "";
  free (main);
  for (const char *ok : keys)
    {
      const a5_object_t *o = a5model_object (st->adv, ok);
      std::vector<const char *> on = objects_on_in (st, ok, A5_OWHERE_ON_OBJECT);
      std::vector<const char *> in = objects_on_in (st, ok, A5_OWHERE_IN_OBJECT);
      int openable = o && a5_prop_find (o->props, o->n_props, "Openable") != NULL;
      if (openable)
        { const char *os = a5state_entity_prop (st, ok, "OpenStatus");
          if (os == NULL || !streq (os, "Open")) in.clear (); }
      char *defn = o ? a5text_object_name (o, A5_ART_DEFINITE) : strdup (ok);
      if (!on.empty ())
        {
          if (!out.empty ()) out += ".  ";
          char *lst = list_objects (st, on);
          out += to_proper (lst); free (lst);
          out += (on.size () == 1) ? " is on " : " are on ";
          out += defn;
        }
      if (!in.empty ())
        {
          if (!on.empty ())
            out += ", and inside";
          else
            { if (!out.empty ()) out += ".  "; out += "Inside "; out += defn; }
          out += (in.size () == 1) ? " is " : " are ";
          char *lst = list_objects (st, in);
          out += lst; free (lst);
        }
      free (defn);
    }
  return strdup (out.c_str ());
}

/*
 * Resolve a ListObjectsOnAndIn argument to the set of object keys it names.
 * The argument is frankendrift's ReplaceFunctions-processed value: a single key,
 * a "key1|key2" pipe list, or an OO expression like "Player.WornAndHeld" (which
 * a5expr resolves to a pipe list).  Mirrors Global.vb:2055-2069 (split on '|',
 * trim a trailing ",..." per key, keep only real object keys).
 */
static std::vector<std::string>
arg_object_keys (a5_state_t *st, const char *args)
{
  std::vector<std::string> keys;
  if (args == NULL)
    return keys;
  std::string a = args;
  size_t dot = a.find ('.');
  if (dot != std::string::npos && dot > 0)
    {
      std::string first = a.substr (0, dot);
      std::string chain = a.substr (dot);
      char *r = a5expr_eval (st, first.c_str (), chain.c_str ());
      a = r ? r : "";
      free (r);
    }
  size_t start = 0;
  while (start <= a.size ())
    {
      size_t bar = a.find ('|', start);
      std::string seg = a.substr (start, bar == std::string::npos
                                  ? std::string::npos : bar - start);
      size_t comma = seg.find (',');
      if (comma != std::string::npos)
        seg = seg.substr (0, comma);
      if (!seg.empty () && a5model_object (st->adv, seg.c_str ()) != NULL)
        keys.push_back (seg);
      if (bar == std::string::npos)
        break;
      start = bar + 1;
    }
  return keys;
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
 * A third-person character's displayed name, mirroring clsCharacter.Name's
 * non-pronoun branch: a "Known"-and-selected character (or one with no
 * descriptor) shows its ProperName; otherwise its Descriptor(Article) --
 * "a young woman" (Indefinite) / "the young woman" (Definite).  The "Known"
 * property may be set at runtime (SetProperty Susan Known <Selected>), so check
 * the override layer too.
 */
static char *
character_display_name (a5_state_t *st, const a5_character_t *c, int definite)
{
  const char *known = c ? a5state_entity_prop (st, c->key, "Known") : NULL;
  int model_known = c != NULL
                    && a5_prop_find (c->props, c->n_props, "Known") != NULL;
  int has_known = (known != NULL) || model_known;
  int selected = has_known
                 && (known == NULL || strstr (known, "Selected") != NULL);
  int use_proper = has_known ? selected : (c == NULL || c->n_descriptors == 0);
  if (use_proper)
    {
      /* clsCharacter.ProperName: the runtime CharacterProperName override
         (set via SetProperty, e.g. a typed-in player name) wins over the
         model Name; "" -> "Anonymous". */
      const char *pn = c ? a5state_entity_prop (st, c->key, "CharacterProperName")
                         : NULL;
      if (pn == NULL || pn[0] == '\0')
        pn = (c != NULL && c->name != NULL && c->name[0] != '\0') ? c->name
                                                                  : "Anonymous";
      return strdup (pn);
    }
  {
    sb_t sb;
    sb_init (&sb);
    if (c->article != NULL && c->article[0] != '\0')
      { sb_puts (&sb, definite ? "the" : c->article); sb_putc (&sb, ' '); }
    if (c->prefix != NULL && c->prefix[0] != '\0')
      { sb_puts (&sb, c->prefix); sb_putc (&sb, ' '); }
    sb_puts (&sb, c->descriptors[0]);
    return sb_finish (&sb);
  }
}

/*
 * The displayed name of a character with a pronoun, mirroring clsCharacter.Name:
 * first/second person always render as a pronoun (I/you/...); third person
 * renders the proper/descriptor name (we don't pronoun-replace NPCs without
 * mention tracking, which is the safe default).
 */
static char *
character_name (a5_state_t *st, const a5_character_t *c, a5_pronoun_t pr)
{
  int persp = char_perspective (c);
  if (persp == 1)
    return strdup (pr == A5_PRO_OBJ ? "me" : pr == A5_PRO_POSS ? "my"
                 : pr == A5_PRO_REFL ? "myself" : "I");
  if (persp == 2)
    return strdup (pr == A5_PRO_POSS ? "your" : pr == A5_PRO_REFL ? "yourself"
                 : "you");
  /* third person: the display name (possessive adds 's) */
  {
    char *nm = character_display_name (st, c, 0);
    if (pr == A5_PRO_POSS)
      { size_t n = strlen (nm); char *s = (char *) malloc (n + 3);
        memcpy (s, nm, n); s[n] = '\''; s[n + 1] = 's'; s[n + 2] = '\0';
        free (nm); return s; }
    return nm;
  }
}

char *
a5text_character_subjective (a5_state_t *st, const a5_character_t *c)
{
  return character_name (st, c, A5_PRO_SUBJ);
}

char *
a5text_char_proper_name (a5_state_t *st, const char *charkey)
{
  int ci = a5state_character_index (st, charkey);
  char *nm = character_display_name (st, ci >= 0 ? &st->adv->characters[ci] : NULL, 0);
  /* ToProper(.Name, bForceRestLower:=False): upper-case the first char only. */
  if (nm[0])
    nm[0] = (char) toupper ((unsigned char) nm[0]);
  return nm;
}

char *
a5text_character_known_name (a5_state_t *st, const a5_character_t *c, int definite)
{
  return character_display_name (st, c, definite);
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
  /* The singular %object%/%object1% (resp. %character%/%character1%) token
     resolves to nothing when the slot was filled by a *plural* %objects%/
     %characters% reference -- FD's GetReference requires ReferenceMatch="object1"
     (clsUserSession.vb:3990), so a %objects% command never sets %object%.  The
     plural %objects% / index forms %object2%.. and override matching are
     unaffected (they read the still-bound key). */
  if ((ci_eq (name, "object") || ci_eq (name, "object1")) && st->ref_object1_plural)
    return NULL;
  if ((ci_eq (name, "character") || ci_eq (name, "character1"))
      && st->ref_character1_plural)
    return NULL;
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
      /* A bare %CharacterName% (no key) resolves to the character whose text is
         being rendered (CharHereDesc / topic reply), else the Player -- v5
         rewrites char-scoped %CharacterName% to %CharacterName[Key]% at load. */
      if (key.empty ()) key = st->ctx_char ? st->ctx_char : "Player";
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
  if (ci_eq (name, "convcharacter"))
    /* Global.vb:1762 substitutes %ConvCharacter% with the conversation char KEY. */
    return strdup (st->conv_char ? st->conv_char : "");
  if (ci_eq (name, "alonewithchar"))
    {
      /* The single other character in the player's location, else NoCharacter
         (clsCharacter.AloneWithChar / Global.vb:1768). */
      const char *ploc = a5state_player_location (st);
      const char *found = NULL;
      int count = 0, i;
      for (i = 0; ploc != NULL && i < st->adv->n_characters; i++)
        {
          if (ci_eq (st->adv->characters[i].key, "Player"))
            continue;
          if (streq (st->char_loc[i], ploc))
            { count++; found = st->adv->characters[i].key; }
        }
      return strdup ((count == 1) ? found : "NoCharacter");
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
  if (ci_eq (name, "locationof"))
    {
      /* Global.vb:2237: the location KEY of a character (its LocationKey), or an
         object's location root(s) joined by '|'.  Used by Jacaranda's champagne
         teleport: %DisplayLocation[%LocationOf[%Player%]%]%. */
      const char *key = args ? args : "";
      int ci = a5state_character_index (st, key);
      if (ci >= 0)
        return strdup (st->char_loc[ci] ? st->char_loc[ci] : "");
      {
        int oi = a5state_object_index (st, key);
        if (oi >= 0)
          {
            sb_t sb; sb_init (&sb);
            for (i = 0; i < st->adv->n_locations; i++)
              if (a5state_object_at_location (st, oi, st->adv->locations[i].key, 1))
                { if (sb.len) sb_putc (&sb, '|');
                  sb_puts (&sb, st->adv->locations[i].key); }
            return sb_finish (&sb);
          }
      }
      return strdup ("");
    }
  if (ci_eq (name, "displaylocation"))
    {
      /* Global.vb:2130: render the given location's ViewLocation (or "There is
         nothing of interest here." when empty). */
      const a5_location_t *l = (args && args[0]) ? a5model_location (st->adv, args) : NULL;
      if (l != NULL)
        {
          char *s = view_location_impl (st, args);
          if (s == NULL || s[0] == '\0')
            { free (s); return strdup ("There is nothing of interest here."); }
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
      /* A piped multi-object arg (%TheObjects[%objects%]% with a "k1|k2|..."
         binding from resolve_plural) renders as FD's "the a, the b and the c"
         article list -- ReplaceFunctions builds a temp ObjectHashTable from the
         split keys and returns htblObjects.List (Global.vb:2056/2386). */
      if (o == NULL && strchr (args, '|') != NULL)
        {
          std::vector<std::string> ks = arg_object_keys (st, args);
          if (ks.size () > 1)
            {
              std::vector<const char *> kp;
              for (auto &s : ks) kp.push_back (s.c_str ());
              return list_objects_art (st, kp, art);
            }
        }
      if (o == NULL && args != NULL && args[0] != '\0')
        {
          /* args may be a key or a display name; resolve_object_arg also
             matches a "prefix + name" full name (e.g. "framed newspaper
             article"), which a bare %object% inner render produces -- the
             plain names[] search missed those and dropped the article. */
          const char *k = resolve_object_arg (st, args);
          if (k != NULL)
            o = a5model_object (st->adv, k);
        }
      if (o != NULL)
        return a5text_object_name (o, art);
      /* No object resolved.  frankendrift builds an ObjectHashTable from the key
         arg and renders htblObjects.List, which returns "nothing" for an empty
         set (Global.vb:804) -- so %TheObject[]% / %ObjectName[]% with an empty
         (or whitespace-only) argument is "nothing", not a blank.  A non-empty but
         unresolved arg is left as-is (it may be an already-rendered name). */
      {
        const char *a = args ? args : "";
        while (*a == ' ' || *a == '\t') a++;
        if (*a == '\0')
          return strdup ("nothing");
      }
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
      /* Global.vb:2213 ListObjectsOnAndIn: iterate the *argument's* object set
         (e.g. %Player%.WornAndHeld for inventory), concatenating
         DisplayObjectChildren (joined by pSpace's two spaces) for each that has
         on/in children -- or, when the set is a single object, unconditionally
         (so its "Nothing is on or inside ..." surfaces). */
      std::vector<std::string> objs = arg_object_keys (st, args);
      std::string out;
      for (const std::string &ok : objs)
        {
          int has_on = !objects_on_in (st, ok.c_str (), A5_OWHERE_ON_OBJECT).empty ();
          int has_in = !objects_on_in (st, ok.c_str (), A5_OWHERE_IN_OBJECT).empty ();
          if (objs.size () != 1 && !has_on && !has_in)
            continue;
          std::string d = display_object_children (st, ok.c_str ());
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
      /* %ListWorn% / %ListHeld% pass bIncludeSubObjects=True (Global.vb:2182/
         2225): append each object's on/inside-contents listing. */
      return list_objects_subobj (st, v);
    }
  if (args != NULL && (ci_eq (name, "displaycharacter") || ci_eq (name, "displayobject")))
    {
      /* Global.vb:2122/2138 DisplayCharacter/DisplayObject: the entity's
         Description.ToString, with the canned "sees nothing interesting about"
         fallback when empty.  a5expr's .Description already renders the node. */
      const char *key = NULL;
      int is_char = ci_eq (name, "displaycharacter");
      if (is_char)
        { const a5_character_t *c = a5model_character (st->adv, args);
          key = c ? c->key : NULL; }
      else
        key = resolve_object_arg (st, args);
      if (key != NULL)
        {
          char *d = a5expr_eval (st, key, ".Description");
          if (d != NULL && d[0] != '\0')
            return d;
          free (d);
          std::string fb = "%CharacterName% see[//s] nothing interesting about ";
          if (is_char)
            { fb += "%CharacterName["; fb += key; fb += ", target]%."; }
          else
            { const a5_object_t *o = a5model_object (st->adv, key);
              char *dn = o ? a5text_object_name (o, A5_ART_DEFINITE) : strdup (key);
              fb += dn; fb += "."; free (dn); }
          char *proc = a5text_process (st, fb.c_str ());
          char *plain = a5text_render_plain (proc);
          free (proc);
          return plain;
        }
    }
  if (args != NULL && ci_eq (name, "listexits"))
    {
      /* clsCharacter.ListExits: the comma/"and"-joined, lower-cased list of
         directions the character has a (restriction-checked) route in, or
         "nowhere" when there are none.  Reuse a5expr's character .Exits path
         (same DirectionsEnum order + HasRouteInDirection check). */
      char *cnt = a5expr_eval (st, args, ".Exits.Count");
      long n = cnt ? strtol (cnt, NULL, 10) : 0;
      free (cnt);
      if (n <= 0)
        return strdup ("nowhere");
      char *lst = a5expr_eval (st, args, ".Exits.List");
      return lst ? lst : strdup ("nowhere");
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
          /* A plural %objects% that resolved to several items renders the whole
             set as an "a, b and c" list of full names (mirroring frankendrift's
             %objects% -> "key1|key2|..." -> OO list render; each name without an
             article, like a bare %object%). */
          if (ci_eq (name, "objects") && st->n_ref_items > 1)
            {
              sb_t sb; sb_init (&sb);
              for (int i = 0; i < st->n_ref_items; i++)
                {
                  const a5_object_t *o = a5model_object (st->adv, st->ref_items[i]);
                  char *nm = o ? a5text_object_name (o, A5_ART_NONE)
                               : strdup (st->ref_items[i]);
                  if (i > 0)
                    sb_puts (&sb, (i == st->n_ref_items - 1) ? " and " : ", ");
                  sb_puts (&sb, nm);
                  free (nm);
                }
              return sb_finish (&sb);
            }
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
  /* An array-variable element: %VariableName[index]% (1-based, mirroring
     ADRIFT's clsVariable string/int arrays).  A Text array stores its elements
     newline-separated in the single var_text string (the InitialValue layout),
     so split on '\n' and return the index'th line; out-of-range -> empty. */
  else
    {
      for (i = 0; i < st->adv->n_variables; i++)
        if (ci_eq (st->adv->variables[i].name, name))
          {
            char *end = NULL;
            long idx = strtol (args, &end, 10);
            if (end == args)        /* not a numeric index -> not our case */
              break;
            if (streq (st->adv->variables[i].type, "Text"))
              {
                const char *s = st->var_text[i] ? st->var_text[i] : "";
                long line = 1;
                const char *ls = s;
                while (line < idx && *s != '\0')
                  if (*s++ == '\n')
                    { line++; ls = s; }
                if (line != idx)
                  return strdup ("");
                const char *le = strchr (ls, '\n');
                size_t n = le ? (size_t) (le - ls) : strlen (ls);
                return strndup (ls, n);
              }
            else
              {
                /* Numeric arrays aren't exercised by the corpus; fall back to
                   the scalar value. */
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
replace_functions (a5_state_t *st, const char *src, int as_arg)
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
                     ReplaceOO, so embedded %Func[]% are gone by then).  Require
                     a real property step after the dot (an alpha char) so a
                     sentence-ending period ("you take %objects%.") is not
                     mistaken for a chain -- that renders via the bare path
                     below (the name / multi-object list), not the raw key. */
                  if (q[1] == '.' && isalpha ((unsigned char) q[2]))
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
                  /* A bare reference (%object%, %character%, ...) used as the
                     argument of an outer %Func[...]% must resolve to the entity
                     *key*, not its display name: frankendrift substitutes
                     %object%->MatchingPossibilities(0) (the key) before any
                     %Func[]% is evaluated (Global.vb:1752+), so e.g.
                     "%TheObject[%object%]%" keys off the exact referenced object
                     rather than re-resolving the rendered noun by name (which
                     would pick the first object sharing that noun). */
                  if (as_arg)
                    {
                      char *fk = oo_firstkey (st, name);
                      if (fk != NULL)
                        {
                          sb_puts (&sb, fk);
                          free (fk); free (name);
                          p = q + 1;
                          continue;
                        }
                      /* An *unbound* object/character reference keyword used as a
                         function argument resolves to the empty string, just as
                         frankendrift's GetReference("ReferencedObject") returns ""
                         when no reference was captured (a reference-free task).
                         The outer %TheObject[]%/%ObjectName[]% then renders
                         "nothing" (htblObjects.List on an empty set) -- e.g. Anno's
                         OpeningHid displaying the carried "...can't see
                         %TheObject[%object%]%." as "You can't see nothing." */
                      if (ci_eq (name, "object") || ci_eq (name, "objects")
                          || ci_eq (name, "object1") || ci_eq (name, "object2")
                          || ci_eq (name, "object3") || ci_eq (name, "object4")
                          || ci_eq (name, "object5") || ci_eq (name, "character")
                          || ci_eq (name, "characters") || ci_eq (name, "character1"))
                        {
                          free (name);
                          p = q + 1;
                          continue;        /* substitute "" */
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
                      args = replace_functions (st, rawargs, 1);
                      /* Resolve any OO property chain left in the args (e.g.
                         %LCase[%object%.OpenStatus]% -> args "Closet.OpenStatus"
                         -> "Closed") before the outer function transforms it.
                         FrankenDrift's ReplaceFunctions(sArgs) recursion runs its
                         own ReplaceOO pass on the args (Global.vb:2046); Scarier's
                         OO pass otherwise runs only after the whole replace pass,
                         too late for a function that consumed the chain as an
                         argument (LCase would lower-case the raw "Closet.OpenStatus"
                         to "closet.openstatus").  a5expr_replace is a no-op unless
                         args holds a real <key>.<chain>. */
                      if (strchr (args, '.') != NULL)
                        {
                          char *ooargs = a5expr_replace (st, args);
                          free (args);
                          args = ooargs;
                        }
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

/* Mirror FrankenDrift's CapAfterFullStop regex
   "^(?<cap>[a-z])|\n(?<cap>[a-z])|[a-z][\.\!\?] ( )?(?<cap>[a-z])"
   (Global.vb:539).  The third alternative requires a *lowercase letter*
   immediately before the .!? so an ellipsis ("...") or a digit/upper char
   before the stop does NOT start a new sentence. */
#define A5_IS_LOWER(c) ((c) >= 'a' && (c) <= 'z')

static int
is_sentence_start (const char *s, size_t i)
{
  if (i == 0)
    return 1;
  if (s[i - 1] == '\n')
    return 1;
  /* "x. y" where x is a lowercase letter */
  if (i >= 3 && s[i - 1] == ' '
      && (s[i - 2] == '.' || s[i - 2] == '!' || s[i - 2] == '?')
      && A5_IS_LOWER (s[i - 3]))
    return 1;
  /* "x.  y" (two spaces) where x is a lowercase letter */
  if (i >= 4 && s[i - 1] == ' ' && s[i - 2] == ' '
      && (s[i - 3] == '.' || s[i - 3] == '!' || s[i - 3] == '?')
      && A5_IS_LOWER (s[i - 4]))
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
                      int flist = ci_eq (name, "objects")
                                  || ci_eq (name, "characters");
                      char *val = a5expr_eval (st, fk, chain, flist);
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
                  char *args = replace_functions (st, rawargs, 1);
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

/* Evaluate a raw expression string to its string value, mirroring
   Global.EvaluateExpression -> clsVariable.SetToExpression: substitute the
   %references%/%variables%/OO chains (bExpression quoting), then reduce.  Used
   for <Expression>-type task restrictions and SetVariable-to-expression.
   Returns a heap string the caller frees; never NULL. */
char *
a5text_eval_expression (a5_state_t *st, const char *expr)
{
  char *sub, *val;
  if (expr == NULL)
    return strdup ("");
  sub = expr_substitute (st, expr);
  /* Resolve any BARE OO-chain (no leading %), e.g. `Event12.Position` in a
     rain-gate restriction -- expr_substitute only handles %ref%.Prop, so the
     bare-key form needs the same ReplaceOO pass that process_inner runs on
     display text (FD's EvaluateExpression -> ReplaceFunctions includes it). */
  {
    char *oo = a5expr_replace (st, sub);
    free (sub);
    sub = oo;
  }
  val = a5_eval_sexpr (sub);
  free (sub);
  return val;   /* a5_eval_sexpr never returns NULL */
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

char *
a5text_process_nocap (a5_state_t *st, const char *src)
{
  char *inner = process_inner (st, src, 0);
  char *persp = resolve_perspective (st, inner);
  free (inner);
  return persp;
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

/* One ALR pass for the Display boundary: like replace_alrs, but the input and
   output are PLAIN text -- each NewText is fully rendered (functions/expr/ALR +
   markup -> plain) before substitution, so an override that carries markup (e.g.
   "<br>Tiden gaar...<br>") lands as plain text rather than leaking literal tags.
   The whole-output markup renderer is deliberately not used (it would eat a
   literal '<' the descriptions legitimately carry). */
static char *
boundary_alr_once (a5_state_t *st, const char *src)
{
  char *cur = strdup (src);
  int i;
  for (i = 0; i < st->adv->n_alrs; i++)
    {
      const a5_alr_t *alr = &st->adv->alrs[i];
      if (alr->old_text == NULL || alr->old_text[0] == '\0')
        continue;
      if (strstr (cur, alr->old_text) != NULL)
        {
          char *raw = a5text_eval_description (st, alr->new_text);
          char *proc = process_inner (st, raw, 1);
          char *val = a5text_render_plain (proc);
          if (!streq (cur, val))     /* guard against self-referential ALRs */
            {
              char *next = str_replace_all (cur, alr->old_text, val);
              free (cur);
              cur = next;
            }
          free (raw);
          free (proc);
          free (val);
        }
    }
  return cur;
}

char *
a5text_display_alr (a5_state_t *st, const char *plain)
{
  char *a1, *cap, *a2;
  if (plain == NULL)
    return strdup ("");
  /* No ALRs -> the boundary is a pure pass-through (English games never pay for
     this; their per-fragment ALR already ran).  Mirrors Display() doing nothing
     when htblALRs is empty. */
  if (st == NULL || st->adv == NULL || st->adv->n_alrs == 0)
    return strdup (plain);
  /* ReplaceALRs: ALR loop -> auto-capitalise -> a second ALR round (the input is
     already plain and per-fragment-processed, so the function/expression passes
     have nothing left to expand). */
  a1 = boundary_alr_once (st, plain);
  cap = auto_capitalise (a1);
  a2 = boundary_alr_once (st, cap);
  free (a1);
  free (cap);
  return a2;
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

/* A present character's "is here" line: its CharHereDesc property rendered (with
   the character as the text context, retiring its DisplayOnce segments), else
   the default "<Name> is here." (clsLocation.ViewLocation: IsHereDesc / fallback). */
static char *
char_here_desc (a5_state_t *st, const a5_character_t *c)
{
  const a5_prop_t *p = a5_prop_find (c->props, c->n_props, "CharHereDesc");
  const char *prev_ctx = st->ctx_char;
  char *result;
  st->ctx_char = c->key;
  if (p != NULL)
    {
      /* The property is present (clsItem.HasProperty == ContainsKey), so it
         overrides the default -- even when its value is blank, in which case
         IsHereDesc returns "" and the character is suppressed from the present
         list (the Customs Official in Revenge: its room prose already says "A
         customs official stands here."). */
      if (p->value_node != NULL)
        {
          int prev_mark = st->marking_display;
          char *raw;
          st->marking_display = 1;
          raw = a5text_eval_description (st, p->value_node);
          st->marking_display = prev_mark;
          result = process_inner (st, raw, 0);     /* %functions% / OO pass */
          free (raw);
        }
      else
        result = strdup ("");
    }
  else
    {
      char *nm = character_display_name (st, c, 0);
      size_t need = strlen (nm) + 11;
      result = (char *) malloc (need);
      snprintf (result, need, "%s is here.", nm);
      free (nm);
    }
  st->ctx_char = prev_ctx;
  return result;
}

/* Case-insensitive replace-all of `from` with `to` in `s`. */
static std::string
ci_replace_all (const std::string &s, const std::string &from, const std::string &to)
{
  if (from.empty ()) return s;
  std::string out, low_from, low_s;
  for (char ch : from) low_from += (char) tolower ((unsigned char) ch);
  for (char ch : s)    low_s    += (char) tolower ((unsigned char) ch);
  size_t i = 0;
  while (i < s.size ())
    {
      if (i + from.size () <= s.size ()
          && low_s.compare (i, from.size (), low_from) == 0)
        { out += to; i += from.size (); }
      else
        out += s[i++];
    }
  return out;
}

/* Render a specific location's ViewLocation (clsLocation.ViewLocation).  lockey
   NULL => the Player's current location (the common case; also the `%Player%`
   look path).  Used directly for `%DisplayLocation[loc]%`. */
static char *
view_location_impl (a5_state_t *st, const char *lockey)
{
  const a5_location_t *loc;
  sb_t sb;
  char *plain;
  int i, listed = 0;

  if (lockey == NULL)
    lockey = a5state_player_location (st);
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
        /* pSpace(sView) before the list (clsLocation.ViewLocation:134): two
           spaces unless the buffer is empty or already ends in a newline. */
        if (sb.len > 0 && sb.p[sb.len - 1] != '\n')
          sb_puts (&sb, "  ");
        sb_puts (&sb, "Also here is ");
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

  /* Event SetLook text (clsLocation.ViewLocation: "For Each e ... e.LookText()"):
     a SetLook sub-event pushed a location-gated look line onto the look stack;
     append the most-recent entry matching the player's location. */
  {
    const char *lt = a5state_player_look (st);
    if (lt != NULL && lt[0] != '\0')
      {
        if (add_space (sb.p, sb.len))
          sb_puts (&sb, "  ");
        sb_puts (&sb, lt);
      }
  }

  /* Present NPCs (clsLocation.ViewLocation character loop): each visible
     character contributes its CharHereDesc (or "<Name> is here."); identical
     descriptions are grouped, the name folded out via ##CHARNAME##, the verb
     pluralised (" is "->" are ") for a shared description, and the names listed. */
  {
    struct cgroup { std::string keyd, first_desc; std::vector<std::string> names; };
    std::vector<cgroup> groups;
    for (i = 0; i < st->adv->n_characters; i++)
      {
        const a5_character_t *c = &st->adv->characters[i];
        char *nmr, *descr;
        if (ci_eq (c->key, "Player"))
          continue;
        if (!a5state_character_at_location (st, i, lockey))   /* incl. on/in furniture */
          continue;
        nmr = character_display_name (st, c, 0);
        descr = char_here_desc (st, c);
        std::string sName = nmr ? nmr : "";
        std::string sDesc = descr ? descr : "";
        free (nmr); free (descr);
        if (sDesc.empty ())
          continue;
        std::string keyd = ci_replace_all (sDesc, sName, "##CHARNAME##");
        size_t g;
        for (g = 0; g < groups.size (); g++)
          if (groups[g].keyd == keyd)
            break;
        if (g == groups.size ())
          { cgroup ng; ng.keyd = keyd; ng.first_desc = sDesc; groups.push_back (ng); }
        if (std::find (groups[g].names.begin (), groups[g].names.end (), sName)
            == groups[g].names.end ())
          groups[g].names.push_back (sName);
      }
    for (size_t g = 0; g < groups.size (); g++)
      {
        const std::vector<std::string> &nm = groups[g].names;
        std::string d;
        /* A single character contributes its description verbatim (preserving
           authored casing); identical descriptions from several characters merge
           via the de-named form, pluralising the verb and listing the names. */
        if (nm.size () <= 1)
          d = groups[g].first_desc;
        else
          {
            d = ci_replace_all (groups[g].keyd, " is ", " are ");
            std::string list;
            for (size_t j = 0; j < nm.size (); j++)
              {
                if (j > 0)
                  list += (j == nm.size () - 1) ? " and " : ", ";
                list += nm[j];
              }
            d = ci_replace_all (d, "##CHARNAME##", list);
          }
        if (add_space (sb.p, sb.len))
          sb_puts (&sb, "  ");
        sb_puts (&sb, d.c_str ());
      }
  }

  /* Exit listing (clsLocation.ViewLocation: "If Adventure.ShowExits Then ...").
     ListExits enumerates the player's routes (restriction-unchecked, matching
     HasRouteInDirection(d, False)); >1 exits => "Exits are <list>.", exactly one
     => "An exit leads <dir>.", none => nothing. */
  if (st->adv->show_exits)
    {
      char *cnt = a5expr_eval (st, "Player", ".Exits.Count");
      long n = cnt ? strtol (cnt, NULL, 10) : 0;
      free (cnt);
      if (n >= 1)
        {
          char *lst = a5expr_eval (st, "Player", ".Exits.List");
          if (add_space (sb.p, sb.len))
            sb_puts (&sb, "  ");
          if (n > 1)
            { sb_puts (&sb, "Exits are "); sb_puts (&sb, lst ? lst : ""); }
          else
            { sb_puts (&sb, "An exit leads "); sb_puts (&sb, lst ? lst : ""); }
          sb_putc (&sb, '.');
          free (lst);
        }
    }

  plain = a5text_render_plain (sb.p ? sb.p : "");
  free (sb.p);
  return plain;
}

char *
a5text_view_location (a5_state_t *st)
{
  return view_location_impl (st, NULL);
}
