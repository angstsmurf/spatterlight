/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
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

#ifndef SCRUNNER_H
#define SCRUNNER_H

/*
 * Internal header of the runner, shared by scrunner.cpp, scrun_split.cpp,
 * scrun_respell.cpp, scrun_match.cpp and scrun_dispatch.cpp.  Nothing here is
 * part of the interface the rest of Scarier uses; that stays in scprotos.h.
 */

#include <string>
#include <vector>

#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"

/* Assorted definitions and constants. */
enum { LINE_BUFFER_SIZE = 256 };
static const scr_char NUL = '\0';
static const scr_char SPECIAL_PATTERN = '#';
static const scr_char WILDCARD_PATTERN = '*';
static const scr_char *const WHITESPACE = "\t\n\v\f\r ";


/* State shared across the runner files, each defined in the file named. */

/* scrun_match.cpp */
extern scr_bool run_rerun_skips_tasks;
extern scr_bool run_rerun_exact_spaces;
extern scr_bool run_lenient_tasks;
extern scr_bool run_matcher_second_pass;
extern scr_bool run_put_class_only;
extern scr_bool run_repeat_assist;

/* scrun_dispatch.cpp */
#ifdef SCARIER_DUMP_TOOLS
extern std::string run_trace_last_input;
#endif
extern scr_bool run_priority_deferred;
extern const scr_char *run_dispatch_input;
extern std::string run_goto_arrival;
extern scr_bool run_goto_arrival_due;
extern std::vector<scr_bool> run_tasks_ran_this_command;
extern scr_bool run_repeat_found_400;
extern std::string run_co_pending_input;
extern scr_bool run_co_task_claimed;
extern scr_char run_prior_element[LINE_BUFFER_SIZE];
extern std::string run_previous_typed_line;
extern std::string run_typed_line;
extern std::string run_undo_text;



/* Structure used to associate a pattern with a handler function. */
typedef struct scr_commands_s
{
  const scr_char *const command;
  scr_bool (*const handler) (scr_gameref_t game);
} scr_commands_t;

typedef scr_commands_t *scr_commandsref_t;

/*
 * scr_ref_entity_guard
 *
 * The referenced object and character, restored on scope exit.  For the
 * speculative table probes (run_is_put_command() and friends), which ask
 * "would this row match?" without running anything: the answer must not
 * leave a binding behind.  3.9 and 4.0 forget both references at the top of
 * every command (see run_player_input()), so a probe's leftover is exactly
 * what the Runner never has.  Professor (Adrift_p4profmail.txt, turn 21):
 * the put-row probe bound the mailbox from `get mail from mailbox on-a rope`
 * and task 7's state restriction on the referenced object passed on it,
 * where run400 fails the restriction silently and takes the mail.
 */
class scr_ref_entity_guard
{
public:
  explicit scr_ref_entity_guard (scr_gameref_t game)
    : vars_ (gs_get_vars (game)),
      object_ (var_get_ref_object (vars_)),
      character_ (var_get_ref_character (vars_))
  {
  }

  ~scr_ref_entity_guard ()
  {
    var_set_ref_object (vars_, object_);
    var_set_ref_character (vars_, character_);
  }

  scr_ref_entity_guard (const scr_ref_entity_guard &) = delete;
  scr_ref_entity_guard &operator= (const scr_ref_entity_guard &) = delete;

private:
  const scr_var_setref_t vars_;
  const scr_int object_;
  const scr_int character_;
};

enum {
  RUN_GOTO_NONE, RUN_GOTO_EXAMINE, RUN_GOTO_TAKE, RUN_GOTO_DROP,
  RUN_GOTO_KEEP, RUN_GOTO_OPEN, RUN_GOTO_BELOW
};

enum {
  RUN_BATTLE_WEAR = 1 << 0, RUN_BATTLE_REMOVE = 1 << 1,
  RUN_BATTLE_TAKE = 1 << 2, RUN_BATTLE_DROP = 1 << 3,
  RUN_BATTLE_SIT = 1 << 4, RUN_BATTLE_OPEN = 1 << 5,
  RUN_BATTLE_EXAMINE = 1 << 6, RUN_BATTLE_SCORE = 1 << 7,
  RUN_BATTLE_GIVE = 1 << 8, RUN_BATTLE_WHERE = 1 << 9,
  RUN_BATTLE_GOTO = 1 << 10, RUN_BATTLE_WAIT = 1 << 11,
  RUN_BATTLE_TALK = 1 << 12, RUN_BATTLE_ASK = 1 << 13,
  RUN_BATTLE_PLAIN = 1 << 14
};

/*
 * scr_task_commands_guard
 *
 * Marks the matches in its lifetime as task commands for the parser, which
 * keeps the pre-3.9 comma boundary to the library's patterns; see
 * uip_match_whitespace().
 */
class scr_task_commands_guard
{
public:
  scr_task_commands_guard () { uip_set_task_commands (TRUE); }
  ~scr_task_commands_guard () { uip_set_task_commands (FALSE); }

  scr_task_commands_guard (const scr_task_commands_guard &) = delete;
  scr_task_commands_guard &
  operator= (const scr_task_commands_guard &) = delete;
};

/*
 * scr_lenient_tasks_guard
 *
 * Turns lenient task matching on or off for one run_all_commands() line,
 * and puts back what was there before when the line is done.
 */
class scr_lenient_tasks_guard
{
public:
  explicit scr_lenient_tasks_guard (scr_bool lenient)
    : saved_ (run_lenient_tasks)
  {
    run_lenient_tasks = lenient;
    uip_set_lenient_tasks (lenient);
  }

  ~scr_lenient_tasks_guard ()
  {
    run_lenient_tasks = saved_;
    uip_set_lenient_tasks (saved_);
  }

  scr_lenient_tasks_guard (const scr_lenient_tasks_guard &) = delete;
  scr_lenient_tasks_guard &
  operator= (const scr_lenient_tasks_guard &) = delete;

private:
  const scr_bool saved_;
};

enum run_refusal_pass_t
{
  REFUSAL_PASS_PRE = 0,
  REFUSAL_PASS_MID,
  REFUSAL_PASS_POST,
  REFUSAL_PASS_PROBE
};


/* Functions shared across the runner files, each defined in the file named. */

/* scrun_split.cpp */
extern scr_bool run_counts_line_elements (scr_gameref_t game);
extern scr_int run_find_split_pre400 (scr_int version, const scr_char *line,
                                      scr_int *tail, scr_bool comma_splits);
extern std::string run_empty_then_head_390 (const scr_char *line);
extern scr_int run_find_split_400 (scr_gameref_t game, const scr_char *line,
                                   scr_int *sep_length);
extern scr_int run_get_version (const scr_prop_setref_t bundle);
extern void run_squeeze_spaces (const scr_char *string, scr_char *buffer);

/* scrun_respell.cpp */
extern scr_int run_goto_line_class (scr_gameref_t game, const scr_char *line);
extern scr_bool run_goto_strip_head (scr_gameref_t game, const scr_char *line,
                                     std::string &rest);
extern scr_bool run_goto_after (scr_gameref_t game, const scr_char *typed,
                                scr_int goto_class, size_t mark,
                                const std::vector<scr_int> &places,
                                scr_bool status);
extern scr_int run_battle_line_class (scr_gameref_t game, const scr_char *line);
extern std::vector<scr_int> run_battle_places (scr_gameref_t game);
extern const scr_char *
run_battle_kind_word (scr_int version, const scr_char *line, scr_int kind);
extern std::string run_battle_respell (scr_gameref_t game,
                                       const scr_char *line, scr_int kind,
                                       const scr_char *head);
extern scr_bool run_battle_line (scr_gameref_t game, const scr_char *typed,
                                 scr_int kinds);
extern scr_bool run_two_verb_line_400 (scr_gameref_t game,
                                       const scr_char *line,
                                       std::string &hoisted);
extern scr_bool run_hoist_verb_line (scr_gameref_t game,
                                     const scr_char *string,
                                     std::string &hoisted);
extern scr_bool run_standard_commands (scr_gameref_t game,
                                       const scr_char *string);

/* scrun_match.cpp */
extern const std::vector<const scr_char *> &
run_task_command_patterns (scr_gameref_t game, scr_int task, scr_bool forwards);
extern scr_bool run_any_task_ran_this_command (void);
extern void run_restriction_cache_task_pick (scr_gameref_t game,
                                             const scr_char *string);
extern scr_bool
run_line_matches_task_strictly (scr_gameref_t game, const scr_char *string);
extern scr_bool run_game_commands_common (scr_gameref_t game,
                                          const scr_char *string,
                                          scr_bool include_restrictions,
                                          scr_bool is_library,
                                          scr_bool exclude_silent_literal);
extern scr_bool
run_game_commands_in_parser_context (scr_gameref_t game,
                                     const scr_char *string,
                                     scr_bool include_restrictions,
                                     scr_bool exclude_silent_literal);
extern scr_bool
run_defer_loud_tasks_to_movement (scr_gameref_t game, const scr_char *string);
extern scr_int run_spent_task_390 (scr_gameref_t game, const scr_char *string,
                                   const scr_char **message);
extern scr_bool run_spent_survivor_390 (scr_gameref_t game,
                                        const scr_char *string);
extern void run_spent_claim_390 (scr_gameref_t game, const scr_char *message);
extern scr_bool run_task_refusal (scr_gameref_t game, const scr_char *string,
                                  run_refusal_pass_t pass);
extern scr_bool run_put_take_400 (scr_gameref_t game, const scr_char *string);
extern void run_takes_second_pass_370 (scr_gameref_t game,
                                       const scr_char *string,
                                       const scr_char *task_string,
                                       size_t task_mark);

/* scrun_dispatch.cpp */
extern scr_bool run_priority_commands (scr_gameref_t game,
                                       const scr_char *string);
extern scr_bool run_try_command_table (scr_commandsref_t command,
                                       scr_gameref_t game,
                                       const scr_char *string);
extern scr_bool run_movement_succeeds (scr_gameref_t game,
                                       const scr_char *string);
extern scr_bool run_standard_verb_commands (scr_gameref_t game,
                                            const scr_char *string);
extern scr_bool
run_standard_give_npc_commands (scr_gameref_t game, const scr_char *string);
extern scr_bool
run_standard_fallback_commands (scr_gameref_t game, const scr_char *string);
extern scr_bool run_therest_arm_at (scr_int version, const scr_char *word);
extern scr_bool run_therest_pre400 (scr_gameref_t game, const scr_char *string);
extern scr_bool run_line_for_anywhere (scr_gameref_t game,
                                       const scr_char *line);
extern void run_finish_goto_walk (void);
extern void run_cancel_goto_walk (void);
extern scr_bool run_player_input (scr_gameref_t game);

#endif
