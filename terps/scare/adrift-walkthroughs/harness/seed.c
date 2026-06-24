/*
 * Deterministic-seed shim for the headless SCARE walkthrough harness.
 *
 * Linked into the standalone ANSI `scare` build. A constructor forces SCARE's
 * portable (platform-independent) RNG and a fixed seed BEFORE main() runs, so
 * the ADRIFT Battle System and any other randomness are reproducible across
 * runs. Without this, the native build seeds rand() from time() and combat
 * outcomes (and scores) vary between identical command sequences.
 *
 * Also opts in to SCARE's Battle-System "combat assist" when SC_ASSUME_COMBAT
 * is set in the environment: many of these games leave Accuracy/Agility at 0,
 * which disables combat entirely; the assist makes hits land so combat plays
 * out on the author's intended strength-vs-defence basis (opt-in, non-faithful).
 */
#include <stdlib.h>
extern void sc_set_portable_random(int flag);
extern void sc_reseed_random_sequence(unsigned long new_seed);
extern void sc_set_combat_assist(int flag);

__attribute__((constructor)) static void seed_det(void) {
  sc_set_portable_random(1);
  sc_reseed_random_sequence(1);
  if (getenv("SC_ASSUME_COMBAT"))
    sc_set_combat_assist(1);
}
