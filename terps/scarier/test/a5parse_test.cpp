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
#include <string.h>

#include "a5parse.h"

static int failures = 0;

/* Expect input to (not) match pattern, and optionally check one bound ref. */
static void
expect (const char *pat, const char *in, int want_match,
        const char *ref, const char *want_val)
{
  a5_match_t m;
  int got = a5parse_match_command (pat, in, &m);
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
