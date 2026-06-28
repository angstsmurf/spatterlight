/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- the object-oriented property-expression engine.
 *
 * ADRIFT 5 text embeds "property expressions": a key (or %reference%) followed by
 * one or more ".Property" / ".Function(args)" navigation steps, e.g.
 *
 *     %object%.Description
 *     %objects%.Name
 *     %Player%.Held(False).List(Indefinite, False)
 *     %objects%.Parent.Name
 *
 * The first key resolves to an object/character/location/group (or a pipe-joined
 * list of keys), then each step navigates (Parent/Children/Contents/Held/Worn/
 * Location/Objects) or terminates (Name/List/Description/Count/Sum/<property>).
 *
 * This is a port of frankendrift Global.ReplaceOO / ReplaceOOProperty
 * (Global.vb:619-1620).  a5text calls a5expr_eval when it sees a %reference% (or
 * bare key) immediately followed by '.', having already substituted the
 * reference to its resolved entity key(s).
 *
 * Every returned char* is heap-allocated; the caller frees it.
 */

#ifndef SCARIER_A5EXPR_H
#define SCARIER_A5EXPR_H

#include "a5state.h"

/*
 * Evaluate the property expression "<firstkeys><chain>", where `firstkeys` is a
 * single entity key or a "key1|key2|..." pipe list and `chain` is the remainder
 * starting at the first '.' (e.g. ".Name", ".Held(False).List(Indefinite)").
 * Returns the rendered text, or NULL if the expression is not resolvable (the
 * caller then leaves the original token verbatim).
 */
extern char *a5expr_eval (a5_state_t *st, const char *firstkeys, const char *chain);

/*
 * Scan `text` for property expressions "<entitykey>.<chain>" (the first token
 * being a real entity key, or a "key1|key2" pipe list, followed by one or more
 * ".step") and replace each with its evaluated text.  Run after the %function%
 * pass, once references have been substituted to keys (mirrors ReplaceOO at the
 * end of ReplaceFunctions).  Returns a freshly allocated string.
 */
extern char *a5expr_replace (a5_state_t *st, const char *text);

#endif
