/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- the string-capable expression evaluator for
 * embedded `<# ... #>` expressions.
 *
 * ADRIFT text can embed expressions between `<#` and `#>` (frankendrift
 * Global.ReplaceExpressions / EvaluateExpression -> clsVariable.SetToExpression).
 * Unlike the integer-only a5arith evaluator (used for numeric variable
 * assignments), an embedded expression yields a *string*: e.g.
 *
 *     <# IF(%Player%.Location.Exits.Count = 0, ".", ", only " + %Player%.Location.Exits.List + ".") #>
 *     <# IF(%direction% = "Up" OR %direction% = "Down", "", "to the ") #>
 *     <#100*%score%/%maxscore%#>
 *     <#if(%objects%.Count > 1, "are", "is")#>
 *
 * By the time this evaluator runs, every %reference%, %variable% and OO
 * property-expression in the body has already been substituted to a literal
 * (numbers stay bare; text is wrapped in double quotes -- frankendrift's
 * bExpression quoting).  What remains is a self-contained expression over:
 *
 *   - numbers, "quoted strings", bare identifiers (treated as string literals);
 *   - arithmetic  + - * / mod ^   (with ADRIFT's precedence: * / bind tighter
 *     than + - mod ^, all left-associative; `+` is numeric add when both sides
 *     are numeric, otherwise string concatenation; `/` rounds away from zero);
 *   - comparisons = == <> != > < >= <=  (numeric or string; yield "1"/"0");
 *   - logic  AND OR  (also && ||);
 *   - functions  if min max abs upr lwr ppr len val str mid replace lft rgt ist
 *     either oneof rand urand  (the random ones draw via a5sexpr_rng_hook when
 *     the host wires it up -- see below; otherwise they fall back to the
 *     deterministic first operand / lower bound).
 *
 * This ports the token reducer of clsVariable.SetToExpression as a clean
 * recursive-descent evaluator that reproduces the same results for the
 * expressions ADRIFT 5 games actually ship.
 *
 * The returned char* is heap-allocated; the caller frees it.  Never NULL.
 */

#ifndef A5SEXPR_H
#define A5SEXPR_H

/* Evaluate a fully-substituted `<#...#>` expression body to its string value. */
char *a5_eval_sexpr (const char *expr);

/* RNG hook for the random functions (either/oneof/rand/urand).  Must return an
   integer in the inclusive range [lo, hi], mirroring frankendrift's
   Global.Random(iMin, iMax).  The run harness sets this to a5rand_between so
   shipped <# OneOf(...) #> text picks draw from the same xoshiro stream as
   FrankenDrift; left NULL (a5text_dump, which links no RNG) the functions fall
   back to the deterministic first operand / lower bound. */
extern long (*a5sexpr_rng_hook) (long lo, long hi);

#endif /* A5SEXPR_H */
