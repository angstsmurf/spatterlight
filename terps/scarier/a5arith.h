/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- integer arithmetic expression evaluator.
 *
 * ADRIFT numeric variable values may be expressions ("%roller%+%sneakbonus%",
 * "(%a%-1)*2", "%n% mod 4").  Once a5text_process has substituted the %vars%
 * to their numeric strings, what remains is a plain arithmetic expression that
 * this evaluator reduces to an integer.  It ports the integer arithmetic core
 * of the Adrift 5 runner's clsVariable.SetToExpression token reducer (+ - * / mod ^,
 * unary minus, parentheses) -- the part the games actually use in variable
 * assignments.  The fuller function library (min/max/if/abs/...) is not ported.
 */

#ifndef A5ARITH_H
#define A5ARITH_H

/* Evaluate an already-substituted arithmetic string to an integer.  Sets *ok
   (when non-NULL) to false if `str` is not a well-formed arithmetic expression
   -- e.g. it contains a stray identifier or trailing garbage -- so the caller
   can fall back to plain strtol() semantics. */
long a5_eval_arith (const char *str, bool *ok);

#endif /* A5ARITH_H */
