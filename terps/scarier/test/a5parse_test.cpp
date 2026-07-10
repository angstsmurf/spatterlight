/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- Phase 3 matcher regression test.
 *
 * Self-contained (links only a5parse.cpp, no game data): asserts the command
 * pattern -> match behaviour for the ConvertToRE constructs ([a/b] alternation,
 * {..} optional, %references%, the bare-direction edge case) plus the direction
 * canonicaliser.  Exits non-zero on any mismatch so `make -f Makefile.headless
 * a5parsetest` is a CI guard for the riskiest single piece of the v5 engine.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a5parse.h"

static int failures = 0;

/* Expect input to (not) match pattern, and optionally check one bound ref.
   The pattern is bracket-corrected first (a5_correct_command), mirroring the
   real flow: a5model applies CorrectCommand to every task command at load, and
   a5parse_match_command then matches the corrected form (e.g. the bare-direction
   "se" matches only after "{...} %direction%" becomes "{... }%direction%"). */
static void
expect (const char *pat, const char *in, int want_match,
        const char *ref, const char *want_val)
{
  a5_match_t m;
  char *cpat = a5_correct_command (pat);
  int got = a5parse_match_command (cpat, in, &m);
  free (cpat);
  if (got != want_match)
    {
      printf ("FAIL match: \"%s\" vs \"%s\" -> %d (want %d)\n", pat, in, got, want_match);
      failures++;
      return;
    }
  if (got && ref != NULL)
    {
      const char *v = a5parse_ref (&m, ref);
      if (v == NULL || strcmp (v, want_val) != 0)
        {
          printf ("FAIL ref:   \"%s\" vs \"%s\" [%s]=\"%s\" (want \"%s\")\n",
                  pat, in, ref, v ? v : "(null)", want_val);
          failures++;
        }
    }
}

static void
expect_dir (const char *text, const char *want)
{
  const char *got = a5parse_canonical_direction (text);
  int ok = (want == NULL) ? (got == NULL) : (got != NULL && strcmp (got, want) == 0);
  if (!ok)
    {
      printf ("FAIL dir:   \"%s\" -> %s (want %s)\n", text,
              got ? got : "(null)", want ? want : "(null)");
      failures++;
    }
}

int
main (void)
{
  /* Alternation + two object references. */
  expect ("[get/take/pick up/remove] %objects% from %object2%",
          "take gun from table", 1, "objects", "gun");
  expect ("[get/take/pick up/remove] %objects% from %object2%",
          "take gun from table", 1, "object2", "table");
  expect ("[get/take/pick up/remove] %objects% from %object2%", "x gun", 0, NULL, NULL);

  /* Nested optional groups. */
  expect ("[examine/exam/ex/x/look {at/in{side}/under}] %object%", "x gun", 1, "object1", "gun");
  expect ("[examine/exam/ex/x/look {at/in{side}/under}] %object%",
          "look inside box", 1, "object1", "box");

  /* A *bare* (un-bracketed) word alternation connector, e.g. Son of Camelot's
     "[fork] on/against [box]".  ADRIFT's ConvertToRE does a GLOBAL '/'->'|', so
     the connector stays unscoped ("...(fork) on|against (box)$") where '|' binds
     '^' to the left branch and '$' to the right.  FrankenDrift (and real ADRIFT)
     test with .NET Regex.IsMatch (a SEARCH), which still matches the left branch
     "^...(fork) on" against the input's prefix, so both connectors accept -- we
     mirror that with std::regex_search (not regex_match).  Regression guard for
     SoC "put fork on box" matching the game's own task, not the generic put. */
  expect ("[place/hold/put] {the {crystal}} [fork] on/against {the {crystal}} [box]",
          "put fork on box", 1, NULL, NULL);
  expect ("[place/hold/put] {the {crystal}} [fork] on/against {the {crystal}} [box]",
          "put fork against box", 1, NULL, NULL);
  expect ("[place/hold/put] {the {crystal}} [fork] on/against {the {crystal}} [box]",
          "place fork on box", 1, NULL, NULL);

  /* The bare-direction edge case (optional prefix collapses to empty). */
  expect ("{[go/walk/move/run] {to {the}}} %direction%", "se", 1, "direction1", "se");
  expect ("{[go/walk/move/run] {to {the}}} %direction%",
          "go to the north", 1, "direction1", "north");
  expect ("{[go/walk/move/run] {to {the}}} %direction%", "xyzzy", 0, NULL, NULL);

  /* Singular reference normalisation (%object% -> %object1%) and plain verbs. */
  expect ("drink %object%", "drink potion", 1, "object1", "potion");
  expect ("say %text%", "say borantha", 1, "text1", "borantha");
  expect ("Take the money", "take the money", 1, NULL, NULL);
  expect ("open %object%", "open door", 1, "object1", "door");

  /* A leading '#' marks a disabled command. */
  expect ("# Unlock %object1% with %object2% ", "unlock door with key", 0, NULL, NULL);

  /* Direction canonicalisation. */
  expect_dir ("se", "SouthEast");
  expect_dir ("North-West", "NorthWest");
  expect_dir ("n", "North");
  expect_dir ("inside", "In");
  expect_dir ("xyzzy", NULL);

  if (failures == 0)
    printf ("a5parse_test: all checks passed\n");
  else
    printf ("a5parse_test: %d FAILURE(S)\n", failures);
  return failures == 0 ? 0 : 1;
}
