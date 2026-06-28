/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- turn loop (Phase 3).
 *
 * Drives the player turn: lower-cases the input, walks the General tasks in
 * priority order (frankendrift TaskSorter / GetGeneralTask), matches each task's
 * command patterns (a5parse), resolves the captured references to model keys in
 * scope, evaluates restrictions (a5restr), and -- on the first fully-passing
 * task -- runs its actions (a5run executes the core action set: MoveObject,
 * MoveCharacter, Set/Inc/DecVariable, SetProperty, SetTasks Execute, EndGame)
 * and prints its completion message, else surfaces the first failing
 * restriction's message.  Movement, take/drop, open and examine work.
 *
 * Mirrors clsUserSession.EvaluateInput / GetGeneralTask / ExecuteActions; the
 * full event/character/walk/score machinery is Phase 4.
 */

#ifndef SCARIER_A5RUN_H
#define SCARIER_A5RUN_H

#include "a5model.h"
#include "a5state.h"

typedef struct a5_run_s a5_run_t;

extern a5_run_t *a5run_new  (const a5_adventure_t *adv);
extern void      a5run_free (a5_run_t *run);

/* The runtime state (for harness inspection). */
extern a5_state_t *a5run_state (a5_run_t *run);

/* Introduction + the opening LOOK, as plain text (caller frees). */
extern char *a5run_intro (a5_run_t *run);

/* Process one command line; returns this turn's output as plain text (freed by
   caller).  Never NULL. */
extern char *a5run_input (a5_run_t *run, const char *line);

/* Non-zero once an EndGame action has fired. */
extern int a5run_is_over (a5_run_t *run);

/* Trace task matching / action execution to stderr. */
extern int a5run_trace;

#endif
