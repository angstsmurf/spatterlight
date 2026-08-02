/*
 * Deterministic-seed shim for the headless SCARIER walkthrough harness.
 *
 * Linked into the standalone ANSI `scarier` build. A constructor forces SCARIER's
 * portable (platform-independent) RNG and a fixed seed BEFORE main() runs, so
 * the ADRIFT Battle System and any other randomness are reproducible across
 * runs. Without this, the native build seeds rand() from time() and combat
 * outcomes (and scores) vary between identical command sequences.
 *
 * Also opts in to SCARIER's Battle-System "combat assist" when SCR_ASSUME_COMBAT
 * is set in the environment: many of these games leave Accuracy/Agility at 0,
 * which disables combat entirely; the assist makes hits land so combat plays
 * out on the author's intended strength-vs-defence basis (opt-in, non-faithful).
 */
#include <stdlib.h>

#include "scarier.h"
#include "scprotos.h"

__attribute__((constructor)) static void seed_det(void) {
  const char *s = getenv("SCR_SEED");
  unsigned long seed = (s && *s) ? strtoul(s, 0, 10) : 1;
  scr_set_portable_random(1);
  scr_reseed_random_sequence(seed);
  if (getenv("SCR_ASSUME_COMBAT"))
    scr_set_combat_assist(1);
  if (getenv("SCR_ASSUME_MOVES"))
    scr_set_move_assist(1);
  if (getenv("SCR_TRACE_TASKS")) {
    task_debug_trace(1);
    restr_debug_trace(1);
  }
}
