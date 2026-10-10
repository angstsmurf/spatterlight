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

#ifndef SCTASKS_INTERNAL_H
#define SCTASKS_INTERNAL_H

/*
 * Internal header of the task module: what sctasks.cpp and the four files
 * split out of it share.
 *
 *   sctasks.cpp          the dispatched-run bracket, assists, hints, the
 *                        per-task property cache, task_can_run_task()
 *   sctasks_move.cpp     the move object and move character actions
 *   sctasks_action.cpp   the other actions and the action dispatch
 *   sctasks_endgame.cpp  the end game action and its score summary
 *   sctasks_run.cpp      task_run_task_unrestricted(), task_run_task()
 */

/*
 * Module notes:
 *
 * o Implements task return FALSE on no output, a slight extension of
 *   current jAsea behavior.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"

/* Trace flag, set before running.  Defined in sctasks.cpp. */
extern scr_bool task_trace;

/* The move assist switch; see task_set_move_assist() in sctasks.cpp. */
extern scr_bool task_move_assist;

/* sctasks.cpp */
extern scr_bool task_in_dispatched_run (void);
extern scr_bool task_is_repeatable (scr_gameref_t game, scr_int task);
extern scr_bool task_repeattext_is_empty (scr_gameref_t game, scr_int task);

/* sctasks_move.cpp */
extern void task_run_move_object_action (scr_gameref_t game, scr_int var1,
                                         scr_int var2, scr_int var3);
extern void task_run_move_npc_action (scr_gameref_t game, scr_int var1,
                                      scr_int var2, scr_int var3);

/* sctasks_endgame.cpp */
extern scr_bool task_run_end_game_action (scr_gameref_t game, scr_int var1);

/* sctasks_action.cpp */
extern scr_bool task_run_task_actions (scr_gameref_t game, scr_int task);

#endif
