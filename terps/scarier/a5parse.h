/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- command-pattern matcher (Phase 3).
 *
 * Ports frankendrift's ConvertToRE / GetRegularExpression / InputMatchesCommand
 * syntactic layer: an author command pattern such as
 *
 *     [get/take/pick up/remove] %objects% from %object2%
 *
 * is compiled to a regular expression (alternation [a/b], optional {..}, the
 * '_' literal-space and '*' wildcard) whose general references (%object1%,
 * %objects%, %character1%, %direction1%, %number1%, %text1%, ...) become capture
 * groups.  Matching an input line yields the raw text each reference captured;
 * the turn loop (a5run) then resolves that text to a concrete model key in scope.
 *
 * Singular references (%object%, %character%, %text%, ...) are normalised to the
 * numbered form (%object1% ...) exactly as frankendrift does at load
 * (FileIO.vb:647), so callers may pass patterns verbatim from the model.
 *
 * Self-contained: uses the C++ <regex> already pulled in at the container edge;
 * the surface is plain C structs so the C-like engine can call it.
 */

#ifndef SCARIER_A5PARSE_H
#define SCARIER_A5PARSE_H

#ifdef __cplusplus
extern "C" {
#endif

#define A5_MAX_REFS 8
#define A5_REF_NAMELEN 24
#define A5_REF_TEXTLEN 256

/* The bound references produced by a successful command match, in pattern order. */
typedef struct a5_match_s {
  int  n_refs;
  char ref_name[A5_MAX_REFS][A5_REF_NAMELEN];  /* "object1","objects","direction1",... */
  char ref_text[A5_MAX_REFS][A5_REF_TEXTLEN];  /* the captured, trimmed substring        */
} a5_match_t;

/*
 * Try to match `input` against a single author command `pattern`.  On a match
 * returns 1 and fills `m` with the captured references (m may be NULL if the
 * caller only wants a yes/no).  Returns 0 on no match (or on a malformed
 * pattern that fails to compile).
 */
extern int a5parse_match_command (const char *pattern, const char *input,
                                  a5_match_t *m);

/* Look up the value of a named reference in a match, or NULL if absent. */
extern const char *a5parse_ref (const a5_match_t *m, const char *name);

/*
 * Canonicalise a direction word ("se", "south-east", "South East") to the
 * PascalCase key ADRIFT uses in a location's Movement table ("SouthEast"), or
 * NULL if the text is not a direction.
 */
extern const char *a5parse_canonical_direction (const char *text);

#ifdef __cplusplus
}
#endif

#endif
