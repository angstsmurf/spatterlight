/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- expression-evaluator regression.
 *
 * Self-contained (links a5sexpr.cpp + a5rand.cpp, no game data): asserts the
 * expression engine a5_eval_sexpr reduces variable-assignment expressions the
 * way the Adrift 5 runner's clsVariable does -- precedence, associativity,
 * mod/power, parentheses, unary minus, quoted literals -- read back the way
 * eval_num_value (a5run_action.cpp) does: VB Val() of the result, i.e. its
 * leading integer, else 0.  a5_eval_sexpr is the sole evaluator for the value
 * path since the integer-only a5arith was folded into it; these cases guard the
 * scoring path (e.g. Six Silver Bullets' "Sneakscore = %roller%+%sneakbonus%",
 * which a bare strtol would truncate to %roller%).
 *
 * Note the power cases: clsVariable reduces `^` on run 2 -- the same pass as
 * `+`/`-`/`mod`, left to right -- so `^` binds LOOSER than `*`/`/` and is
 * LEFT-associative (see FrankenDrift clsVariable.vb).  The old a5arith evaluator
 * treated `^` as the tightest, right-associative operator, which diverged from
 * the runner; "2^3^2" is 64 (== (2^3)^2), not 512, and "2*3^2" is 36.
 */

#include <stdio.h>
#include <stdlib.h>

#include "a5sexpr.h"

static int failures = 0;

/* Mirror eval_num_value's read-back: VB Val() of the evaluated result. */
static long
caller_val (const char *expr)
{
  char *sv = a5_eval_sexpr (expr);
  long e = strtol (sv, NULL, 10);
  free (sv);
  return e;
}

static void
expect (const char *expr, long want)
{
  long got = caller_val (expr);
  if (got != want)
    {
      printf ("FAIL: \"%s\" -> %ld (want %ld)\n", expr, got, want);
      failures++;
    }
}

int
main (void)
{
  /* The scoring case a bare strtol used to truncate. */
  expect ("3+2", 5);
  expect ("750000", 750000);

  /* Precedence and associativity. */
  expect ("2+3*4", 14);
  expect ("2*3+4", 10);
  expect ("10-3-2", 5);          /* left-associative subtraction */
  expect ("20/4/5", 1);          /* left-associative division     */

  /* Power (clsVariable run 2: looser than * and /, left-associative). */
  expect ("2^3^2", 64);          /* (2^3)^2, NOT 2^(3^2)=512      */
  expect ("2*3^2", 36);          /* (2*3)^2, ^ looser than *      */
  expect ("2^3", 8);

  /* Division rounds to nearest, half away from zero (FD Math.Round /
     MidpointRounding.AwayFromZero), per operator -- not C truncation. */
  expect ("171/60", 3);          /* 2.85 -> 3  (Amazon clock carry)   */
  expect ("(201-30)/60", 3);     /* the actual ts_varHour expression  */
  expect ("5/2", 3);             /* 2.5  -> 3  (half away from zero)   */
  expect ("7/2", 4);             /* 3.5  -> 4                          */
  expect ("1/2", 1);             /* 0.5  -> 1                          */
  expect ("5/3", 2);             /* 1.66 -> 2                          */
  expect ("-5/2", -3);           /* -2.5 -> -3 (away from zero)        */
  expect ("7/2*2", 8);           /* round at '/' first: 4*2, not 7     */

  /* mod, parentheses, unary minus. */
  expect ("17 mod 5", 2);
  expect ("(2+3)*4", 20);
  expect ("-5+8", 3);
  expect ("-(3+4)", -7);
  expect ("2 * -3", -6);

  /* Division / modulo by zero: the runner reads SafeInt(Val(...)) of the
     result.  a5sexpr guards mod-by-zero to 0 directly; division by zero
     evaluates to "\xe2\x88\x9e" (Infinity), whose Val() is 0 -- so both read
     back as 0 (matching the former a5arith guard), not the raw leading digit. */
  expect ("5/0", 0);
  expect ("5 mod 0", 0);

  /* Whitespace tolerance. */
  expect ("  6  +  1  ", 7);

  /* Quoted string literals are value tokens (clsVariable "vlu"), valued
     numerically via VB Val.  The doubled-quote serialisation `= ""1""` reaches
     the evaluator as `"1"` after the action parser strips one pair, so these
     must read as their inner number. */
  expect ("\"1\"", 1);
  expect ("\"0\"", 0);
  expect ("\"5\"", 5);
  expect ("'7'", 7);
  expect ("\"x\"", 0);             /* non-numeric quoted literal -> 0 (Val) */
  expect ("\"5\" + \"3\"", 8);     /* numeric quoted literals add           */

  /* Non-numeric values read back as 0 (VB Val) -- a bare key, an empty value. */
  expect ("Object3", 0);           /* a key, not a number */
  expect ("", 0);                  /* empty               */

  if (failures == 0)
    printf ("a5sexpr_test: all checks passed\n");
  else
    printf ("a5sexpr_test: %d FAILURE(S)\n", failures);
  return failures == 0 ? 0 : 1;
}
