/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- command-pattern matcher.  See a5parse.h.
 *
 * A direct port of frankendrift clsUserSession.ConvertToRE (clsUserSession.vb:
 * 6298) and the syntactic half of InputMatchesCommand (5575): the structural
 * transforms ([a/b] alternation, {..} optional, '_' space, '*' wildcard) and the
 * reference -> capture-group substitution, then a std::regex match that hands the
 * captured reference text back to the (semantic) resolver in a5run.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <regex>
#include <string>
#include <vector>

#include "a5parse.h"

/* --------------------------------------------------------- direction table */

/*
 * Maps every spelling ADRIFT accepts to the PascalCase key used in a location's
 * <Movement><Direction> (clsAdventure.vb:748 sDirectionsRE).  The lowercase
 * alternation strings here feed both the regex and a5parse_canonical_direction.
 */
struct dir_entry { const char *canonical; const char *re; };
static const dir_entry kDirs[] = {
  { "North",     "north|n" },
  { "NorthEast", "northeast|ne|north-east|n-e" },
  { "East",      "east|e" },
  { "SouthEast", "southeast|se|south-east|s-e" },
  { "South",     "south|s" },
  { "SouthWest", "southwest|sw|south-west|s-w" },
  { "West",      "west|w" },
  { "NorthWest", "northwest|nw|north-west|n-w" },
  { "In",        "in|inside" },
  { "Out",       "out|o|outside" },
  { "Up",        "up|u" },
  { "Down",      "down|d" },
};
static const int kNumDirs = (int) (sizeof kDirs / sizeof kDirs[0]);

/*
 * Per-direction runtime spec (the localization subsystem).  Defaults to the
 * English kDirs; a5parse_set_directions overrides selected entries from the
 * game's <Direction*> header fields (clsAdventure.sDirectionsRE).  `re` is the
 * lowercased "|"-separated synonym alternation used for parsing/regex; `name`
 * is the display name -- the first synonym in its authored case
 * (Global.DirectionName, the substring before the first separator).
 */
static struct { std::string re; std::string name; } g_dirs[kNumDirs];
static bool g_dirs_init = false;

/* DirectionsEnum value -> canonical key, for mapping the model's enum-ordered
   <Direction*> array onto kDirs. */
static const char *const kDirEnum[12] = {
  "North", "East", "South", "West", "Up", "Down",
  "In", "Out", "NorthEast", "SouthEast", "SouthWest", "NorthWest" };

/* The English display name of a canonical direction is the canonical key
   itself ("North/N" -> DirectionName "North"). */
static void
ensure_dirs_default ()
{
  int j;
  if (g_dirs_init)
    return;
  for (j = 0; j < kNumDirs; j++)
    {
      g_dirs[j].re = kDirs[j].re;
      g_dirs[j].name = kDirs[j].canonical;
    }
  g_dirs_init = true;
}

static std::string cached_dirs_re;   /* invalidated by a5parse_set_directions */

void
a5parse_set_directions (const char *const *raw_by_enum)
{
  int e, j;
  /* Reset to the English defaults first so the call is idempotent and a later
     game with no overrides cannot inherit an earlier game's localization. */
  g_dirs_init = false;
  ensure_dirs_default ();
  cached_dirs_re.clear ();
  if (raw_by_enum == NULL)
    return;
  for (e = 0; e < 12; e++)
    {
      const char *raw = raw_by_enum[e];
      if (raw == NULL || raw[0] == '\0')
        continue;
      for (j = 0; j < kNumDirs; j++)
        if (strcmp (kDirs[j].canonical, kDirEnum[e]) == 0)
          {
            /* Display name = first synonym (authored case), before the first
               '/' (Global.DirectionName). */
            const char *slash = strchr (raw, '/');
            g_dirs[j].name = slash ? std::string (raw, slash - raw) : std::string (raw);
            /* Parse/regex form: lowercased, '/' -> '|' (Global.DirectionRE). */
            std::string re;
            for (const char *p = raw; *p; p++)
              re += (*p == '/') ? '|' : (char) tolower ((unsigned char) *p);
            g_dirs[j].re = re;
            break;
          }
    }
}

const char *
a5parse_direction_name (const char *canonical)
{
  int j;
  if (canonical == NULL)
    return NULL;
  ensure_dirs_default ();
  for (j = 0; j < kNumDirs; j++)
    if (strcmp (kDirs[j].canonical, canonical) == 0)
      return g_dirs[j].name.c_str ();
  return NULL;
}

/* The full direction alternation, lowercased, built once.  frankendrift's
   %direction% builder loops `For eDirection = North To NorthWest` -- and by the
   DirectionsEnum VALUES (North=0, East=1, South=2, West=3, Up=4, Down=5, In=6,
   Out=7, NorthEast=8, SouthEast=9, SouthWest=10, NorthWest=11) that range spans
   ALL TWELVE directions, including Up/Down/In/Out.  (An earlier reading took the
   names literally and emitted only the 8 compass points, which made bare "up"/
   "u"/"in"/"out" fail to match movement tasks -- e.g. Anno 1700's upstairs.) */
static std::string
directions_re ()
{
  std::string s;
  const int n = 12;
  ensure_dirs_default ();
  for (int i = 0; i < n; i++)
    {
      for (int j = 0; j < kNumDirs; j++)
        if (strcmp (kDirs[j].canonical, kDirEnum[i]) == 0)
          { s += g_dirs[j].re; break; }
      if (i != n - 1)
        s += "|";
    }
  return s;
}

/* The full lowercased direction alternation ("north|n|...|down|d"), exposed so
   the turn loop can seed listKnownWords (clsUserSession NotUnderstood splits
   sDirectionsRE on "|"). */
const char *
a5parse_directions_re (void)
{
  if (cached_dirs_re.empty ())
    cached_dirs_re = directions_re ();
  return cached_dirs_re.c_str ();
}

const char *
a5parse_canonical_direction (const char *text)
{
  std::string t;
  if (text == NULL)
    return NULL;
  for (const char *p = text; *p; p++)
    if (!isspace ((unsigned char) *p))
      t += (char) tolower ((unsigned char) *p);
  if (t.empty ())
    return NULL;
  ensure_dirs_default ();
  for (int j = 0; j < kNumDirs; j++)
    {
      std::string re = g_dirs[j].re;
      size_t start = 0, bar;
      do {
        bar = re.find ('|', start);
        std::string alt = re.substr (start, bar - start);
        /* compare ignoring '-' so "south-east" == "southeast" */
        std::string a, b;
        for (char c : alt) if (c != '-') a += c;
        for (char c : t)   if (c != '-') b += c;
        if (a == b)
          return kDirs[j].canonical;
        start = bar + 1;
      } while (bar != std::string::npos);
    }
  return NULL;
}

/* ---------------------------------------------------------- string helpers */

static void
replace_all (std::string &s, const std::string &from, const std::string &to)
{
  if (from.empty ())
    return;
  size_t pos = 0;
  while ((pos = s.find (from, pos)) != std::string::npos)
    {
      s.replace (pos, from.size (), to);
      pos += to.size ();
    }
}

/* Normalise singular references to the numbered form (FileIO.vb:647). */
static void
normalise_refs (std::string &s)
{
  replace_all (s, "%object%",    "%object1%");
  replace_all (s, "%character%", "%character1%");
  replace_all (s, "%location%",  "%location1%");
  replace_all (s, "%number%",    "%number1%");
  replace_all (s, "%text%",      "%text1%");
  replace_all (s, "%item%",      "%item1%");
  replace_all (s, "%direction%", "%direction1%");
}

/* ------------------------------------------------------------- ConvertToRE */

/*
 * Build the regex pattern for a command and record the reference group names in
 * capture order.  Structural groups are non-capturing ((?:...)) so the only
 * capturing groups -- and hence the only ones std::smatch reports -- are the
 * references, left to right in `out_refs`.
 */
static bool
convert_to_re (const char *pattern, std::string &out_pattern,
               std::vector<std::string> &out_refs)
{
  std::string sC = pattern ? pattern : "";

  /* A leading '#' marks a disabled/comment command. */
  {
    size_t i = 0;
    while (i < sC.size () && sC[i] == ' ') i++;
    if (i < sC.size () && sC[i] == '#')
      return false;
  }

  normalise_refs (sC);

  /* Escape literal regex specials that ADRIFT treats as text. */
  replace_all (sC, "+", "\\+");
  replace_all (sC, "(", "\\(");
  replace_all (sC, ")", "\\)");

  /* Wildcards (non-capturing variants of frankendrift's groups).  Like
     frankendrift (ConvertToRE), substitute a placeholder for the wildcard body
     and expand it to ".*?" only at the very end -- otherwise the bare-'*' pass
     re-rewrites the '*' inside an already-inserted ".*?", corrupting the regex
     (e.g. "{the} *[string/twig]" never matched "get twig"). */
  replace_all (sC, " * ", " (?:\x01 )?");
  replace_all (sC, "* ",  "(?:\x01 )?");
  replace_all (sC, " *",  "(?: \x01)?");
  replace_all (sC, "*",   "\x01");
  replace_all (sC, "\x01", ".*?");

  replace_all (sC, "_", " ");

  /* Optional / mandatory text and alternation -> non-capturing groups. */
  replace_all (sC, "{", "(?:");
  replace_all (sC, "}", ")?");
  replace_all (sC, "[", "(?:");
  replace_all (sC, "]", ")");
  replace_all (sC, "/", "|");

  /* Replace references with capturing groups, recording names in order. */
  std::string dirRE = directions_re ();
  std::string result;
  result.reserve (sC.size () + 32);
  for (size_t i = 0; i < sC.size (); )
    {
      if (sC[i] == '%')
        {
          size_t j = i + 1;
          while (j < sC.size () && (isalnum ((unsigned char) sC[j]) || sC[j] == '_'))
            j++;
          if (j < sC.size () && sC[j] == '%' && j > i + 1)
            {
              std::string name = sC.substr (i + 1, j - i - 1);
              /* base = name without trailing digits, to pick the value class */
              std::string base = name;
              while (!base.empty () && isdigit ((unsigned char) base.back ()))
                base.pop_back ();
              if (base == "number")
                result += "(-?[0-9]+)";
              else if (base == "direction")
                result += "(" + dirRE + ")";
              else /* object(s), character(s), text, location, item */
                result += "(.+?)";
              out_refs.push_back (name);
              i = j + 1;
              continue;
            }
        }
      result += sC[i];
      i++;
    }

  /*
   * NOTE: Scarier used to relax a literal space following an optional group
   * (")? " -> ")? ?") here, to fake the bare-form matching that real ADRIFT gets
   * from clsUserSession.CorrectCommand restructuring `{x} y` into `{x }y`.  Now
   * that a5model applies CorrectCommand (a5_correct_command) at load -- exactly
   * as FrankenDrift does at game-start init -- this matcher must mirror FD's
   * ConvertToRE *verbatim* (no extra space relaxation), or the two double-relax
   * and over-match.
   */

  /* Trim and anchor. */
  size_t a = result.find_first_not_of (' ');
  size_t b = result.find_last_not_of (' ');
  if (a == std::string::npos)
    result.clear ();
  else
    result = result.substr (a, b - a + 1);
  out_pattern = "^" + result + "$";
  return true;
}

/* ------------------------------------------------ CorrectCommand (bracket fix) */

/*
 * A verbatim port of clsUserSession.CorrectCommand / ProcessBlock / GetSubBlock
 * / ContainsMandatoryText (clsUserSession.vb:6126-6295).  FrankenDrift applies
 * this to every task command at game-start init, restructuring an optional
 * group so an adjacent literal space becomes optional too: `{x} y` -> `{x }y`,
 * `{a/b}` -> `{ [a/b]}`.  a5model calls a5_correct_command at load on every
 * command, after which a5parse_match_command's ConvertToRE mirrors FD exactly.
 */

/* GetSubBlock: peel the next token off sBlock (modified in place) -- either the
   leading plain text up to a top-level bracket, a complete bracketed block, or
   text up to and including a top-level '/'. */
static std::string
cc_get_sub_block (std::string &sBlock)
{
  int iDepth = 0;
  std::string sNewBlock;
  int len = (int) sBlock.size ();

  for (int i = 0; i < len; i++)
    {
      char c = sBlock[i];
      sNewBlock += c;
      switch (c)
        {
        case '{':
        case '[':
          if (iDepth == 0 && i > 0)
            {
              sBlock = sBlock.substr (i);       /* keep bracket char onward */
              return sNewBlock.substr (0, i);   /* sLeft(sNewBlock, i)      */
            }
          iDepth += 1;
          break;
        case ']':
        case '}':
          iDepth -= 1;
          if (iDepth == 0)
            {
              sBlock = sBlock.substr (i + 1);
              return sNewBlock;
            }
          break;
        case '/':
          if (iDepth == 0)
            {
              sBlock = sBlock.substr (i + 1);
              return sNewBlock;
            }
          break;
        }
    }
  sBlock = "";
  return sNewBlock;
}

/* ContainsMandatoryText: true if any part of the block is mandatory (inside []
   or not in {} brackets at all -- spaces ignored). */
static bool
cc_contains_mandatory (const std::string &sBlock)
{
  if (sBlock.empty ())
    return false;

  int iLevel = 0;
  for (char c : sBlock)
    {
      switch (c)
        {
        case ' ':
          break;
        case '{':
          iLevel += 1;
          break;
        case '}':
          iLevel -= 1;
          break;
        default:
          if (iLevel == 0)
            return true;
        }
    }
  return false;
}

static std::string
cc_process_block (const std::string &sBlock)
{
  std::string sAfter = sBlock;
  std::string sNextBlock;
  std::string sBefore;

  do
    {
      sNextBlock = cc_get_sub_block (sAfter);
      if (!sNextBlock.empty ())
        {
          if (sNextBlock[0] == '{')
            {
              bool bContainsMandatory = cc_contains_mandatory (sAfter);
              if (bContainsMandatory && !sAfter.empty () && sAfter[0] == ' ')
                {
                  /* before final [] block: _{x}_ => _{x_} */
                  if (sBefore.empty () || sBefore.back () == ' '
                      || sBefore.back () == '}')
                    {
                      if (sNextBlock.find ('/') != std::string::npos)
                        sNextBlock = "{["
                            + sNextBlock.substr (1, sNextBlock.size () - 2)
                            + "] }";
                      else
                        sNextBlock = sNextBlock.substr (0, sNextBlock.size () - 1)
                            + " }";
                      sAfter = sAfter.substr (1);
                    }
                }
              else if (!bContainsMandatory && !sBefore.empty ()
                       && sBefore.back () == ' ')
                {
                  /* after final [] block: _{x}_ => {_x}_ */
                  if (sNextBlock.find ('/') != std::string::npos)
                    sNextBlock = "{ ["
                        + sNextBlock.substr (1, sNextBlock.size () - 2) + "]}";
                  else
                    sNextBlock = "{ " + sNextBlock.substr (1);
                  sBefore = sBefore.substr (0, sBefore.size () - 1);
                }

              /* End block: " {#}" => "{ #}" or "{ [#/#]}" */
              if (!sBefore.empty () && sBefore.back () == ' ' && sAfter.empty ())
                {
                  if (sNextBlock.find ('/') != std::string::npos)
                    sNextBlock = "{ ["
                        + sNextBlock.substr (1, sNextBlock.size () - 2) + "]}";
                  else
                    sNextBlock = "{ " + sNextBlock.substr (1);
                  sBefore = sBefore.substr (0, sBefore.size () - 1);
                }
              sBefore += "{"
                  + cc_process_block (sNextBlock.substr (1, sNextBlock.size () - 2))
                  + "}";
            }
          else if (sNextBlock[0] == '[')
            {
              sBefore += "["
                  + cc_process_block (sNextBlock.substr (1, sNextBlock.size () - 2))
                  + "]";
            }
          else if (sNextBlock.back () == '/')
            {
              sBefore += cc_process_block (
                             sNextBlock.substr (0, sNextBlock.size () - 1))
                  + "/";
            }
          else
            {
              sBefore += sNextBlock;
            }
        }
    }
  while (!sAfter.empty ());
  return sBefore;
}

char *
a5_correct_command (const char *cmd)
{
  std::string in = cmd ? cmd : "";
  std::string out = cc_process_block (in);
  char *r = (char *) malloc (out.size () + 1);
  if (r != NULL)
    memcpy (r, out.c_str (), out.size () + 1);
  return r;
}

/* ---------------------------------------------------------------- matching */

static std::string
trim (const std::string &s)
{
  size_t a = s.find_first_not_of (" \t");
  size_t b = s.find_last_not_of (" \t");
  if (a == std::string::npos)
    return "";
  return s.substr (a, b - a + 1);
}

int
a5parse_match_command (const char *pattern, const char *input, a5_match_t *m)
{
  std::string re_str;
  std::vector<std::string> refs;

  if (m != NULL)
    m->n_refs = 0;
  if (!convert_to_re (pattern, re_str, refs))
    return 0;

  try
    {
      std::regex re (re_str, std::regex::ECMAScript | std::regex::icase);
      std::smatch mt;
      std::string in = input ? input : "";
      if (!std::regex_match (in, mt, re))
        return 0;
      if (m != NULL)
        {
          int n = 0;
          for (size_t g = 1; g < mt.size () && n < A5_MAX_REFS; g++)
            {
              /* capture group g corresponds to refs[g-1] (refs are the only
                 capturing groups, in order). */
              if (g - 1 >= refs.size ())
                break;
              std::string val = trim (mt[g].str ());
              snprintf (m->ref_name[n], A5_REF_NAMELEN, "%s", refs[g - 1].c_str ());
              snprintf (m->ref_text[n], A5_REF_TEXTLEN, "%s", val.c_str ());
              n++;
            }
          m->n_refs = n;
        }
      return 1;
    }
  catch (const std::regex_error &)
    {
      return 0;
    }
}

const char *
a5parse_ref (const a5_match_t *m, const char *name)
{
  if (m == NULL || name == NULL)
    return NULL;
  for (int i = 0; i < m->n_refs; i++)
    if (strcmp (m->ref_name[i], name) == 0)
      return m->ref_text[i];
  return NULL;
}
