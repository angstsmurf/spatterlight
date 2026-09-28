/*
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

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
  /* SCR_SEED overrides; the default is 1, or 1234 when SCR_RNG=xoshiro
     selects the Runner-compatible stream (see scr_default_random_seed). */
  scr_set_portable_random(1);
  scr_reseed_random_sequence(scr_default_random_seed());
  if (getenv("SCR_ASSUME_COMBAT"))
    scr_set_combat_assist(1);
  if (getenv("SCR_ASSUME_MOVES"))
    scr_set_move_assist(1);
  if (getenv("SCR_ASSUME_REPEATS"))
    scr_set_repeat_assist(1);
  if (getenv("SCR_ASSUME_ROOMS"))
    scr_set_room_assist(1);
  /* Capacity is per-game state, so this sets the default new games pick up;
     the GUI equivalent is `glk capacity on` after the game has loaded. */
  if (getenv("SCR_ASSUME_CAPACITY"))
    scr_set_capacity_assist(1);
  /* Targeted per-game data fixes for the handful of games whose own data
     makes them unwinnable; matched on name, author and on the broken value
     still being there, so it is a no-op for every other game. */
  if (getenv("SCR_ASSUME_PATCHES"))
    scr_set_game_patches(1);
  if (getenv("SCR_TRACE_TASKS")) {
    task_debug_trace(1);
    restr_debug_trace(1);
  }
  if (getenv("SCR_TRACE_NPCS")) {
    npc_debug_trace(1);
  }
}
