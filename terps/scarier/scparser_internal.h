/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

#ifndef SCPARSER_INTERNAL_H
#define SCPARSER_INTERNAL_H

/*
 * Internal header of the pattern parser: what scparser.cpp and the four
 * files split out of it share.  They were one translation unit, so much here
 * is shared state rather than interface.
 *
 *   scparser.cpp          uip_match() and its cache of compiled patterns,
 *                         tracing, string cleansing
 *   scparser_pattern.cpp  tokenizer and tree builder, the node and word
 *                         pools, uip_compile_pattern()
 *   scparser_match.cpp    the per-node matchers and their state, the
 *                         matching options, the "*" wildcard matchers,
 *                         uip_match_pattern()
 *   scparser_entity.cpp   %object% and %character% matching and the
 *                         candidate cache
 *   scparser_pronoun.cpp  pronoun substitution, the antecedent registers,
 *                         named-NPC noting, the ask/talk rewrite
 */

/*
 * Module notes:
 *
 * o Pattern matching follows the Runners where it has been measured: the
 *   "*" wildcard and %text% rules are in uip_wildcard_match_400() /
 *   uip_wildcard_match_pre400() and uip_match_text(), and %character% /
 *   %object% matching, prefixes included, in uip_build_entities() and
 *   uip_match_entity().  Each carries its own measurement notes and the
 *   deliberate deviations from the Runner are marked as such.
 */

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
static const scr_char NUL = '\0';
static const scr_char MINUS = '-';
static const scr_char PLUS = '+';
static const scr_char PERCENT = '%';
static const scr_char *const WHITESPACE = "\t\n\v\f\r ";


/*
 * Parsed pattern tree node definition.   The tree is a left child, right
 * sibling representation, with token type, and word at nodes of type TOK_WORD.
 * NODE_UNUSED must be zero to ensure that the statically allocated array that
 * forms the node pool appears initially as containing only unused nodes.
 */
typedef enum
{
  NODE_UNUSED = 0,
  NODE_CHOICE, NODE_OPTIONAL, NODE_WILDCARD, NODE_WHITESPACE,
  NODE_HARD_WHITESPACE, NODE_DOUBLE_WHITESPACE, NODE_GROUP_DOUBLE_WHITESPACE,
  NODE_JOIN,
  NODE_CHARACTER_REFERENCE, NODE_OBJECT_REFERENCE, NODE_TEXT_REFERENCE,
  NODE_NUMBER_REFERENCE, NODE_WORD, NODE_VARIABLE, NODE_LIST, NODE_EOS
} scr_pttype_t;
typedef struct scr_ptnode_s
{
  struct scr_ptnode_s *left_child;
  struct scr_ptnode_s *right_sibling;

  scr_pttype_t type;
  scr_char *word;
  scr_bool is_allocated;
} scr_ptnode_t;
typedef scr_ptnode_t *scr_ptnoderef_t;


/*
 * A parsed pattern: its tree (NULL if the pattern failed to parse), and
 * whether uip_set_binary_input() can apply to it, which depends on the
 * pattern's text alone.  Matching never mutates either.
 */
struct scr_uip_pattern_s
{
  scr_ptnoderef_t tree;
  scr_bool binary_candidate;
};


/* Buffer size below which cleansed strings and tokens avoid malloc(). */
enum { UIP_ALLOCATION_AVOIDANCE_SIZE = 128 };


/* Shared across the files; each is defined in the file named. */

/* scparser.cpp */
extern scr_bool uip_trace;
extern scr_char *
uip_cleanse_string (const scr_char *original, scr_char *buffer, scr_int length,
                    scr_bool trim_trailing = TRUE);
extern scr_char *
uip_free_cleansed_string (scr_char *string, const scr_char *buffer);

/* scparser_pattern.cpp */
extern scr_ptnoderef_t uip_new_node (scr_pttype_t type);
extern void uip_destroy_node (scr_ptnoderef_t node);
extern void uip_debug_dump (scr_ptnoderef_t tree);

/* scparser_match.cpp */
extern const scr_char *uip_string;
extern scr_int uip_posn;
extern scr_gameref_t uip_get_game (void);
extern scr_bool uip_task_commands;
extern scr_bool uip_lenient_tasks;
extern scr_int uip_skip_article (const scr_char *string, scr_int start);
extern scr_bool uip_match_node (scr_ptnoderef_t node);

/* scparser_entity.cpp */
extern const scr_char *uip_word_end_punctuation;
extern scr_bool
uip_case_folds_name_in (const scr_char *command, const scr_char *name);
extern scr_bool uip_match_character (scr_ptnoderef_t node);
extern scr_bool uip_match_object (scr_ptnoderef_t node);

#endif
