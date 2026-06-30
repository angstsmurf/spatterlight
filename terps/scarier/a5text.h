/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- text engine (Phase 2 MVP).
 *
 * Turns the model's <Description> blocks into display text: it evaluates the
 * SingleDescription list (DisplayWhen + per-description restrictions), runs the
 * %function% / %variable% substitution and ALR ("Text Override") passes, applies
 * ADRIFT's sentence auto-capitalisation, and renders the resulting HTML-ish
 * markup (<br>, <c>, <b>, <i>, entities) down to plain text.
 *
 * Covers the variables, core functions and tags the static world needs; the
 * full perspective/pronoun/UDF engine is Phase 4.  Mirrors frankendrift
 * SharedModule.Description.ToString, ReplaceALRs, ReplaceFunctions and the
 * GlkHtmlWin tag renderer.
 *
 * Every returned char* is heap-allocated; the caller frees it.
 */

#ifndef SCARIER_A5TEXT_H
#define SCARIER_A5TEXT_H

#include "a5state.h"
#include "a5xml.h"

/*
 * Evaluate a description wrapper node (e.g. a location's <LongDescription>, an
 * object's <Description>, the <Introduction>) into raw source text with markup
 * still embedded.  Returns "" (never NULL) if node is NULL/empty.
 */
extern char *a5text_eval_description (a5_state_t *st, const a5_xml_node_t *wrapper);
/* The effective short description of a location (base <ShortDescription> plus any
   inherited ShortLocationDescription group-property alternates -- darkness). */
extern char *a5text_location_short (a5_state_t *st, const char *lockey);
/* As a5text_location_short but fully rendered to plain text (the room NAME). */
extern char *a5text_location_short_plain (a5_state_t *st, const char *lockey);

/* Run the %function%/%variable% and ALR passes + auto-capitalisation on src. */
extern char *a5text_process (a5_state_t *st, const char *src);

/* Like a5text_process but WITHOUT the trailing auto-capitalisation -- for
   evaluating a value to an entity KEY (case-sensitive), where capitalising the
   first letter ("s_SkeletonKe" -> "S_SkeletonKe") would corrupt the key.  FD
   applies auto-cap only at Display time, never when resolving a reference. */
extern char *a5text_process_nocap (a5_state_t *st, const char *src);

/* Evaluate a raw expression string (Global.EvaluateExpression): substitute its
   %references%/OO chains then reduce to a string value.  Heap; never NULL. */
extern char *a5text_eval_expression (a5_state_t *st, const char *expr);

/* Render markup (tags + entities) to plain text. */
extern char *a5text_render_plain (const char *src);

/*
 * The Display() ALR boundary (clsUserSession.Display -> Global.ReplaceALRs):
 * apply the game's ALR ("Text Override") substitutions + auto-capitalisation to
 * an already-assembled, already-plain turn output, then render any markup an ALR
 * NewText introduced.  This is how a non-English game (e.g. Danish Halloween)
 * translates the engine's stock literals (NotUnderstood, "Also here is ",
 * "Time passes...") -- which never passed through a5text_process per fragment.
 * A no-op for a game without ALRs.  Heap; never NULL (caller frees).
 */
extern char *a5text_display_alr (a5_state_t *st, const char *plain);

/*
 * Convenience: eval_description -> process -> render_plain, i.e. the finished
 * plain text for a description wrapper.
 */
extern char *a5text_describe (a5_state_t *st, const a5_xml_node_t *wrapper);

/* The full "LOOK" view of the player's current location (plain text). */
extern char *a5text_view_location (a5_state_t *st);

/* An object's display name with an article ("a"/"the"/none). */
typedef enum { A5_ART_NONE, A5_ART_INDEFINITE, A5_ART_DEFINITE } a5_article_t;
extern char *a5text_object_name (const a5_object_t *o, a5_article_t art);

/*
 * A character's displayed name honouring perspective: first/second person render
 * as the subjective pronoun ("I"/"you"), third person as the character's name.
 * Mirrors clsCharacter.Name(Subjective).  Heap-allocated.
 */
extern char *a5text_character_subjective (a5_state_t *st, const a5_character_t *c);

/*
 * A character's display name with only the first letter upper-cased
 * (Global.ToProper(.Name, bForceRestLower:=False)), used by a walk's
 * ShowEnterExit "<Name> enters/exits ..." message.  Heap-allocated.
 */
extern char *a5text_char_proper_name (a5_state_t *st, const char *charkey);

/*
 * A character's Known-aware display name (clsCharacter.Name's non-pronoun
 * branch): the proper name once Known/Selected, else the descriptor with an
 * article ("a young woman" indefinite / "the young woman" definite).  Used by
 * the NotUnderstood seen-character branch.  Heap-allocated.
 */
extern char *a5text_character_known_name (a5_state_t *st, const a5_character_t *c,
                                          int definite);

#endif
