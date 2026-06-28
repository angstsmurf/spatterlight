/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- arithmetic expression evaluator regression.
 *
 * Self-contained (links only a5arith.cpp, no game data): asserts the integer
 * arithmetic core a5_eval_arith reduces variable-assignment expressions
 * correctly -- precedence, associativity, mod/power, parentheses, unary minus
 * -- and that non-arithmetic input is rejected (so eval_num_value falls back to
 * strtol).  Guards the scoring path (e.g. Six Silver Bullets'
 * "Sneakscore = %roller%+%sneakbonus%", which strtol would truncate to %roller%).
 */

#include <stdio.h>

#include "a5arith.h"

static int failures = 0;

static void
expect (const char *expr, long want)
{
  bool ok = false;
  long got = a5_eval_arith (expr, &ok);
  if (!ok || got != want)
    {
      printf ("FAIL: \"%s\" -> %ld ok=%d (want %ld, ok=1)\n",
              expr, got, (int) ok, want);
      failures++;
    }
}

static void
expect_bad (const char *expr)
{
  bool ok = true;
  a5_eval_arith (expr, &ok);
  if (ok)
    {
      printf ("FAIL: \"%s\" accepted as arithmetic (want rejected)\n", expr);
      failures++;
    }
}

int
main (void)
{
  /* The scoring case strtol used to truncate. */
  expect ("3+2", 5);
  expect ("750000", 750000);

  /* Precedence and associativity. */
  expect ("2+3*4", 14);
  expect ("2*3+4", 10);
  expect ("10-3-2", 5);          /* left-associative subtraction */
  expect ("20/4/5", 1);          /* left-associative division     */
  expect ("2^3^2", 512);         /* right-associative power       */

  /* mod, parentheses, unary minus. */
  expect ("17 mod 5", 2);
  expect ("(2+3)*4", 20);
  expect ("-5+8", 3);
  expect ("-(3+4)", -7);
  expect ("2 * -3", -6);

  /* Division / modulo by zero guarded to 0 (frankendrift SafeInt). */
  expect ("5/0", 0);
  expect ("5 mod 0", 0);

  /* Whitespace tolerance. */
  expect ("  6  +  1  ", 7);

  /* Non-arithmetic -> rejected (caller falls back to strtol). */
  expect_bad ("Object3");        /* a key, not a number */
  expect_bad ("3 + ");           /* trailing operator   */
  expect_bad ("3 4");            /* trailing garbage    */
  expect_bad ("");               /* empty               */

  if (failures == 0)
    printf ("a5arith_test: all checks passed\n");
  else
    printf ("a5arith_test: %d FAILURE(S)\n", failures);
  return failures == 0 ? 0 : 1;
}
