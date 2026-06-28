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
  static const char *order[] = {
    "North", "East", "South", "West", "Up", "Down",
    "In", "Out", "NorthEast", "SouthEast", "SouthWest", "NorthWest" };
  const int n = (int) (sizeof order / sizeof order[0]);
  for (int i = 0; i < n; i++)
    {
      for (int j = 0; j < kNumDirs; j++)
        if (strcmp (kDirs[j].canonical, order[i]) == 0)
          { s += kDirs[j].re; break; }
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
  static std::string cached;
  if (cached.empty ())
    cached = directions_re ();
  return cached.c_str ();
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
  for (int j = 0; j < kNumDirs; j++)
    {
      std::string re = kDirs[j].re;
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
   * A literal space that immediately follows an optional group ")?" must itself
   * be optional, or a command like "{[go] {to {the}}} %direction%" could never
   * match a bare "se" (the optional prefix collapses to empty but the space
   * before the reference would remain mandatory).  ADRIFT accepts the bare form,
   * so relax exactly those spaces -- and no others, so "xgun" still fails.
   */
  replace_all (result, ")? ", ")? ?");

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
