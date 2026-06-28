/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- integer arithmetic expression evaluator.
 * See a5arith.h.  Recursive-descent over the grammar
 *
 *     expr  := term  (('+'|'-') term)*
 *     term  := power (('*'|'/'|'mod') power)*
 *     power := atom  ('^' power)?            ; right-associative
 *     atom  := number | '(' expr ')' | ('+'|'-') atom
 *
 * with integer math (matching ADRIFT's CInt-based reduction); division and
 * modulo by zero yield 0 (frankendrift guards the same way via SafeInt).
 */

#include <stdlib.h>

#include "a5arith.h"

namespace {

struct arith_p { const char *s; bool ok; };

long arith_expr (arith_p &p);

void
arith_skip_ws (arith_p &p)
{
  while (*p.s == ' ' || *p.s == '\t') p.s++;
}

long
arith_atom (arith_p &p)
{
  arith_skip_ws (p);
  if (*p.s == '(')
    {
      p.s++;
      long v = arith_expr (p);
      arith_skip_ws (p);
      if (*p.s == ')') p.s++; else p.ok = false;
      return v;
    }
  if (*p.s == '-') { p.s++; return -arith_atom (p); }
  if (*p.s == '+') { p.s++; return  arith_atom (p); }
  if (*p.s >= '0' && *p.s <= '9')
    {
      char *end;
      long v = strtol (p.s, &end, 10);
      if (end == p.s) { p.ok = false; return 0; }
      p.s = end;
      return v;
    }
  /* anything else (a stray identifier / function name) -> not arithmetic */
  p.ok = false;
  return 0;
}

long
arith_power (arith_p &p)
{
  long base = arith_atom (p);
  arith_skip_ws (p);
  if (*p.s == '^')
    {
      p.s++;
      long exp = arith_power (p);      /* right-associative */
      long r = 1;
      for (long i = 0; i < exp && i < 64; i++) r *= base;
      return r;
    }
  return base;
}

long
arith_term (arith_p &p)
{
  long v = arith_power (p);
  for (;;)
    {
      arith_skip_ws (p);
      if (*p.s == '*') { p.s++; v *= arith_power (p); }
      else if (*p.s == '/')
        { p.s++; long d = arith_power (p); v = d ? v / d : 0; }
      else if ((p.s[0] == 'm' || p.s[0] == 'M') && (p.s[1] == 'o' || p.s[1] == 'O')
               && (p.s[2] == 'd' || p.s[2] == 'D'))
        { p.s += 3; long d = arith_power (p); v = d ? v % d : 0; }
      else break;
    }
  return v;
}

long
arith_expr (arith_p &p)
{
  long v = arith_term (p);
  for (;;)
    {
      arith_skip_ws (p);
      if (*p.s == '+') { p.s++; v += arith_term (p); }
      else if (*p.s == '-') { p.s++; v -= arith_term (p); }
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
  long v = arith_expr (p);
  arith_skip_ws (p);
  if (*p.s != '\0') p.ok = false;   /* trailing garbage */
  if (ok) *ok = p.ok;
  return v;
}
