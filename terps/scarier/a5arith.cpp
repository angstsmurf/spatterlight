/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- arithmetic expression evaluator.
 * See a5arith.h.  Recursive-descent over the grammar
 *
 *     expr  := term  (('+'|'-'|'mod') term)*  ; mod shares +/- precedence (FD run=2)
 *     term  := power (('*'|'/') power)*
 *     power := atom  ('^' power)?            ; right-associative
 *     atom  := number | '(' expr ')' | ('+'|'-') atom
 *
 * Faithful to FrankenDrift's clsVariable expression reduction (clsVariable.vb
 * ~800-825): operands are doubles (VB `Val`), and only the `/` operator rounds
 * its result to an integer -- Math.Round(left/right, MidpointRounding.AwayFromZero),
 * i.e. C round().  `* + - ^` keep the (integer-valued) double, `mod` is VB Mod
 * (== C fmod), and division / modulo by zero yield 0 (frankendrift guards the
 * same way via SafeInt).  Because every `/` rounds to an integer and the other
 * operators preserve integers, the final result is integer-valued; it is
 * returned as a long (rounded away from zero to absorb any float artefact).
 *
 * The earlier version reduced with C `long` truncation, which diverged from FD
 * wherever a division had a non-integer quotient -- e.g. Amazon's per-turn clock
 * `ts_varHour += (%Minute%-30)/60` carried the wrong number of hours.
 */

#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#include "a5arith.h"

namespace {

struct arith_p { const char *s; bool ok; };

double arith_expr (arith_p &p);

void
arith_skip_ws (arith_p &p)
{
  while (*p.s == ' ' || *p.s == '\t') p.s++;
}

double
arith_atom (arith_p &p)
{
  arith_skip_ws (p);
  if (*p.s == '(')
    {
      p.s++;
      double v = arith_expr (p);
      arith_skip_ws (p);
      if (*p.s == ')') p.s++; else p.ok = false;
      return v;
    }
  if (*p.s == '-') { p.s++; return -arith_atom (p); }
  if (*p.s == '+') { p.s++; return  arith_atom (p); }
  if (*p.s == '"' || *p.s == '\'')
    {
      /* A quoted string literal "N" / 'N' is a value token (frankendrift
         clsVariable.GetToken -> "vlu" + content, valued numerically via VB Val:
         a numeric string yields its number, a non-numeric one yields 0).  The
         doubled-quote serialisation `= ""1""` reaches here as `"1"` after the
         action parser strips one surrounding pair. */
      char q = *p.s++;
      const char *start = p.s;
      char *end;
      long v;
      while (*p.s != '\0' && *p.s != q) p.s++;
      v = strtol (start, &end, 10);     /* Val(): leading integer, else 0 */
      if (end == start) v = 0;
      if (*p.s == q) p.s++;             /* consume the closing quote */
      return (double) v;
    }
  if (*p.s >= '0' && *p.s <= '9')
    {
      /* Literals are integers (ADRIFT variables and constants are integer);
         parse as such so "3 4" still leaves trailing garbage to be rejected. */
      char *end;
      long v = strtol (p.s, &end, 10);
      if (end == p.s) { p.ok = false; return 0; }
      p.s = end;
      return (double) v;
    }
  /* anything else (a stray identifier / function name) -> not arithmetic */
  p.ok = false;
  return 0;
}

double
arith_power (arith_p &p)
{
  double base = arith_atom (p);
  arith_skip_ws (p);
  if (*p.s == '^')
    {
      p.s++;
      double exp = arith_power (p);    /* right-associative */
      long e = (long) llround (exp);   /* exponents are integer-valued */
      double r = 1.0;
      for (long i = 0; i < e && i < 64; i++) r *= base;
      return r;
    }
  return base;
}

double
arith_term (arith_p &p)
{
  double v = arith_power (p);
  for (;;)
    {
      arith_skip_ws (p);
      if (*p.s == '*') { p.s++; v *= arith_power (p); }
      else if (*p.s == '/')
        { p.s++; double d = arith_power (p);
          /* FD: Math.Round(left / right, MidpointRounding.AwayFromZero). */
          v = (d != 0.0) ? round (v / d) : 0.0; }
      /* `mod` is handled in arith_expr (FD's +/- precedence), not here. */
      else break;
    }
  return v;
}

double
arith_expr (arith_p &p)
{
  double v = arith_term (p);
  for (;;)
    {
      arith_skip_ws (p);
      if (*p.s == '+') { p.s++; v += arith_term (p); }
      else if (*p.s == '-') { p.s++; v -= arith_term (p); }
      /* FD's clsVariable.SetToExpression reduces `Mod` on the same pass (run=2)
         as `+`/`-`, left-to-right -- so Mod has +/- precedence, NOT the tighter
         `*`/`/` precedence of standard arithmetic.  `22+8 Mod 24` is thus
         `(22+8) Mod 24` = 6, not `22+(8 Mod 24)` = 30 (Amazon's `%Hour%+8 Mod
         24` sleep-skip).  `*`/`/` stay tighter (still reduced inside term). */
      else if ((p.s[0] == 'm' || p.s[0] == 'M') && (p.s[1] == 'o' || p.s[1] == 'O')
               && (p.s[2] == 'd' || p.s[2] == 'D')
               && !(isalnum ((unsigned char) p.s[3]) || p.s[3] == '_'))
        { p.s += 3; double d = arith_term (p);
          v = (d != 0.0) ? fmod (v, d) : 0.0; }   /* VB Mod == C fmod */
      else break;
    }
  return v;
}

} /* namespace */

long
a5_eval_arith (const char *str, bool *ok)
{
  if (str == NULL) { if (ok) *ok = false; return 0; }
  arith_p p; p.s = str; p.ok = true;
  double v = arith_expr (p);
  arith_skip_ws (p);
  if (*p.s != '\0') p.ok = false;   /* trailing garbage */
  if (ok) *ok = p.ok;
  return (long) llround (v);
}
