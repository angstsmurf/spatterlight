/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- turn loop (Phase 3).
 *
 * Drives the player turn: lower-cases the input, walks the General tasks in
 * priority order (the Adrift 5 runner TaskSorter / GetGeneralTask), matches each task's
 * command patterns (a5parse), resolves the captured references to model keys in
 * scope, evaluates restrictions (a5restr), and -- on the first fully-passing
 * task -- runs its actions (a5run executes the core action set: MoveObject,
 * MoveCharacter, Set/Inc/DecVariable, SetProperty, SetTasks Execute, EndGame)
 * and prints its completion message, else surfaces the first failing
 * restriction's message.  Movement, take/drop, open and examine work.
 *
 * Mirrors clsUserSession.EvaluateInput / GetGeneralTask / ExecuteActions,
 * including general-reference disambiguation ("Which X?" + cross-turn clarifier);
 * the full event/character/walk/score machinery is the rest of Phase 4.
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

/* Status-line accessors.  a5run_location_name returns the current room NAME as
   plain text (caller frees; NULL if unknown); the others read Adventure.Score /
   MaxScore / Turns (0 when the game defines no Score/MaxScore variable). */
extern char *a5run_location_name (a5_run_t *run);
extern long  a5run_score         (a5_run_t *run);
extern long  a5run_maxscore      (a5_run_t *run);
extern int   a5run_turns         (a5_run_t *run);

/* Embedded-media events produced by the most recent a5run_intro / a5run_input,
   in display order, for the host to show images / play sounds.  `kind` is one of
   the A5_MEDIA_* values (a5text.h); `number` is the Blorb resource number (from
   <FileMappings>), or -1 when unresolved / for sound-stop.  The list is rebuilt
   each turn; pointers from a5run_media_get are valid until the next turn. */
typedef struct {
  int kind;          /* A5_MEDIA_IMAGE / A5_MEDIA_SOUND / A5_MEDIA_SOUND_STOP    */
  int number;        /* Blorb resource number, or -1                            */
  int channel;       /* sound channel (audio), else 0                           */
  int loop;          /* sound loop flag                                         */
} a5_media_event_t;

extern int                     a5run_media_count (a5_run_t *run);
extern const a5_media_event_t *a5run_media_get   (a5_run_t *run, int i);

/* Save/restore (Phase 5).  a5run_save serialises the full mutable runtime state
   -- object/character locations, variable values, completed tasks, property
   overrides, the "seen" sets, event/walk timers, displayed <DisplayOnce>
   segments, conversation state and the RNG -- to a malloc'd, NUL-terminated XML
   buffer (caller frees); *out_len receives its byte length (excluding the NUL).
   Returns NULL on failure.  a5run_restore applies such a buffer to `run` (which
   must have been created from the *same* game), returning 1 on success, 0 on a
   malformed/incompatible buffer (in which case `run` is left unchanged on the
   structural level, though a partial apply may have occurred -- callers should
   treat a 0 return as fatal for the session).  The buffer is the v5 save body;
   the Glk/Spatterlight layer may compress/wrap it. */
extern char *a5run_save    (a5_run_t *run, size_t *out_len);
extern int   a5run_restore (a5_run_t *run, const char *data, size_t len);

/* Single-level undo (mirrors the ADRIFT 4 game->undo slot; built on save/restore).
   a5run_snapshot captures the current state as the undo point -- call it from the
   frontend just BEFORE a5run_input each turn (a5run_input increments the turn
   counter on entry, so snapshotting inside it would capture mid-turn state).
   a5run_undo restores the last snapshot and consumes it (a second consecutive
   undo returns 0); it returns 1 on success, 0 when nothing is snapshotted.
   a5run_undo_forget drops the undo point without restoring (call after a RESTORE
   from file so undo does not jump back across the restore). */
extern void a5run_snapshot    (a5_run_t *run);
extern int  a5run_undo        (a5_run_t *run);
extern void a5run_undo_forget (a5_run_t *run);

/* Trace task matching / action execution to stderr. */
extern int a5run_trace;

#endif
