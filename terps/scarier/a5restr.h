/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- restriction evaluation (Phase 2 subset).
 *
 * Evaluates a <Restrictions> block (the restricted-description and, later,
 * task gating mechanism) against the runtime state.  A restriction is a single
 * line "<key1> Must|MustNot <Operator> <key2>" wrapped in a type element
 * (<Object>/<Location>/<Character>/<Variable>/<Task>/...), combined by the
 * block's <BracketSequence> ('#' = a restriction, 'A' = and, 'O' = or, with
 * '('/'[' and ')'/']' brackets).
 *
 * Phase 2 implements the object/location/variable/task operators the static
 * world's descriptions depend on, plus best-effort character handling; the
 * structure (a ported EvaluateRestrictionBlock + a typed PassSingleRestriction)
 * is the one Phase 3 grows for the full task engine.  Mirrors frankendrift
 * clsUserSession.PassRestrictions / EvaluateRestrictionBlock / PassSingleRestriction.
 */

#ifndef SCARIER_A5RESTR_H
#define SCARIER_A5RESTR_H

#include "a5state.h"
#include "a5xml.h"

/* 1 if the restriction block passes (an empty/absent block passes). */
extern int a5restr_pass (a5_state_t *st, const a5_xml_node_t *restrictions);

/*
 * The <Message> description node of the first restriction that individually
 * fails (document order), or NULL.  Used to surface a task's failure text;
 * faithful for the common AND-chain case.
 */
extern const a5_xml_node_t *a5restr_fail_message (a5_state_t *st,
                                                  const a5_xml_node_t *restrictions);

/*
 * The destination key for moving out of `lockey` in the canonical direction
 * `dir` ("SouthEast", "Up", ...), honouring the exit's own restrictions, or
 * NULL if there is no (passable) route.  The result may be a location key or a
 * location-group key (the caller resolves a group to a concrete room).
 */
extern const char *a5restr_exit_in_direction (a5_state_t *st, const char *lockey,
                                              const char *dir);

/* Trace restriction evaluation to stderr (driven by the harness/A5_TRACE). */
extern int a5restr_trace;

#endif
