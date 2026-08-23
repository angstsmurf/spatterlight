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

#include "../../../adrift5/a5sexpr.h"

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

  /* Unary minus, verified against the REAL clsVariable live (2026-08-01, a
     44-row battery through FrankenDrift's SetToExpression -- see
     RUNNER_TESTS_TODO.md).  clsVariable tokenises a leading '-' as an operator
     and reduces the dangling `op expr` pair on run 2 -- AFTER '/' has rounded
     on run 1 -- where a5sexpr folds the '-' into the literal before dividing.
     The two models agree everywhere BECAUSE v5's away-from-zero rounding is
     symmetric (round(-q) == -round(q)); in ADRIFT 4's epsilon-biased rounding
     the same two parses genuinely diverge (-5/2: scexpr -2, run400 -3). */
  expect ("-7/2", -4);           /* fold -3.5 -> -4 == -(3.5 -> 4)     */
  expect ("-1/2", -1);
  expect ("5/-2", -3);           /* unary on the divisor               */
  expect ("-5/-2", 3);
  expect ("--5", 5);             /* op-op-expr: both minuses reduce    */
  expect ("5--2", 7);
  expect ("10--5/2", 13);        /* 10 - (-5/2 -> -3)                  */
  expect ("-2^2", 4);            /* leading '-' reduces before ^ (run 2,
                                    left-to-right): (-2)^2, not -(2^2) */
  expect ("-7 mod 2", -1);       /* VB Mod: sign of dividend           */
  expect ("7 mod -2", 1);
  expect ("-max(5,3)/2", -3);
  expect ("-0/5", 0);
  /* 2^-2 = 0.25: read back as 0 either way.  (-2^-1 = -0.5 is the one
     divergent row of the battery: the runner reads SafeInt(Val("-0.5")),
     VB Int() flooring to -1 under an English locale -- and 0 under a
     comma-decimal locale, where Val stops at ','.  strtol's leading-integer
     0 is kept: locale-dependent even in the real runner, zero corpus
     exposure, and only reachable via ^ with a fractional result.) */
  expect ("2^-2", 0);
  expect ("-2^-1", 0);

  /* Parenthesized group ahead of a division chain -- a DELIBERATE deviation
     (see RUNNER_TESTS_TODO.md section 4, "ADRIFT 5 paren group + division
     chain").  clsVariable's run-based reducer cannot collapse a group whose
     contents need run 2 (+ - mod ^), so `*`/`/` pairs to its RIGHT reduce
     first and the chain effectively right-associates: the real runner gives
     (A+0)/B/C = A/(B/C) but (A)/B/C = (A/B)/C -- ralphmerridew's report,
     verified by source reading in ADRIFT 5.0.36.5 and FrankenDrift (both
     unfixed).  a5sexpr is uniformly LEFT-associative; corpus exposure is
     zero (2026-08-22: 85,566 expressions across 174 games, no delayed group
     followed by a 2-op mul/div chain).  Runner-divergent values noted. */
  expect ("(100)/10/5", 2);      /* both engines agree: (100/10)/5        */
  expect ("(100+0)/10/5", 2);    /* runner: 100/(10/5) = 50               */
  expect ("(2+2)/4/2", 1);       /* runner: 4/(4/2) = 2                   */
  expect ("(10-4)/3/2", 1);      /* runner: 6/(3/2) = 3                   */
  expect ("1/(2+0)/3/4", 0);     /* mid-chain group; runner: (1/2)/(3/4) = 1 */
  expect ("max(1+1,3)/4/2", 1);  /* delayed funct arg; runner: 3/(4/2) = 2 */
  expect ("(2*3)/6/2", 1);       /* run-1 group contents: both agree      */
  expect ("3*(10/3)", 9);        /* the FBA corpus shape: both agree      */
  expect ("(100*7)/50", 14);     /* the score-line corpus shape: agree    */

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
