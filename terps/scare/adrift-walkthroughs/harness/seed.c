/*
 * Deterministic-seed shim for the headless SCARE walkthrough harness.
 *
 * Linked into the standalone ANSI `scare` build. A constructor forces SCARE's
 * portable (platform-independent) RNG and a fixed seed BEFORE main() runs, so
 * the ADRIFT Battle System and any other randomness are reproducible across
 * runs. Without this, the native build seeds rand() from time() and combat
 * outcomes (and scores) vary between identical command sequences.
 *
 * Opt-in, non-faithful aids (off unless their env var is set):
 *  - SC_ASSUME_COMBAT -> sc_set_combat_assist: many games leave Accuracy/Agility
 *    at 0, which disables combat entirely; the assist makes hits land so combat
 *    plays out on the author's intended strength-vs-defence basis.
 *  - SC_ASSUME_MOVES -> sc_set_move_assist: a few native-4.0 games authored a
 *    move task action's "To:" combo as VB's default -1 (destination in Var3);
 *    the Runner ignores such moves, which can strand the player (To Hell &
 *    Beyond). The assist honours an unset (-1) move whose Var3 names a real room.
 *
 * SC_TRACE_TASKS turns on SCARE's task/restriction tracing (to stderr), handy
 * for diagnosing why a task does or doesn't fire while deriving a walkthrough.
 */
#include <stdlib.h>
extern void sc_set_portable_random(int flag);
extern void sc_reseed_random_sequence(unsigned long new_seed);
extern void sc_set_combat_assist(int flag);
extern void sc_set_move_assist(int flag);
extern void task_debug_trace(int flag);
extern void restr_debug_trace(int flag);

__attribute__((constructor)) static void seed_det(void) {
  sc_set_portable_random(1);
  sc_reseed_random_sequence(1);
  if (getenv("SC_ASSUME_COMBAT"))
    sc_set_combat_assist(1);
  if (getenv("SC_ASSUME_MOVES"))
    sc_set_move_assist(1);
  if (getenv("SC_TRACE_TASKS")) {
    task_debug_trace(1);
    restr_debug_trace(1);
  }
}
