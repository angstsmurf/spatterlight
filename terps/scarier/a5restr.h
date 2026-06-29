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
 * 1 if the block contains a restriction with the `Exist` operator on the given
 * reference type ('o' = Object, 'c' = Character).  Mirrors clsTask's
 * HasObjectExistRestriction / HasCharacterExistRestriction: it is the gate that
 * decides whether a command whose reference resolves to nothing nonetheless
 * "matches" the task (so the task can surface its Must-Exist message on the
 * second-chance pass) rather than not matching at all.  Must/MustNot is ignored,
 * exactly like the originals (they test eObject/eCharacter == Exist only).
 */
extern int a5restr_has_exist (const a5_xml_node_t *restrictions, char type);

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
 *
 * When `blocked_msg` is non-NULL it is set to the blocking exit-restriction's
 * <Message> node if the exit exists but its restriction fails (left untouched
 * otherwise) -- frankendrift's sRouteError, which overrides the movement task's
 * generic "There is no route..." message.
 */
extern const char *a5restr_exit_in_direction (a5_state_t *st, const char *lockey,
                                              const char *dir,
                                              const a5_xml_node_t **blocked_msg);

/* Trace restriction evaluation to stderr (driven by the harness/A5_TRACE). */
extern int a5restr_trace;

#endif
