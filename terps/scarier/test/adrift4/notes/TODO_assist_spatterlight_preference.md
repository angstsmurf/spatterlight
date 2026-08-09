# TODO: a persistent Spatterlight preference for the two assists

The only open half of the old "make the assists user-settable" plan. Everything
else landed; this file was trimmed to its remaining scope when it did
(`git show 1f9ee127:terps/scarier/test/adrift4/notes/TODO_user_assist_metacommands.md`
for the full original plan and its verification matrix).

## What the assists are

Two opt-in, non-faithful aids that let an *otherwise-unwinnable* amateur ADRIFT
game be completed:

- **Combat assist** (`battle_set_combat_assist`, `scbattle.cpp`) — forces an
  automatic hit in games that leave every character's Accuracy/Agility at 0, so
  `acc > agi` = `0 > 0` never lands. Only acts on *fully-unconfigured* games
  (`battle_unconfigured`, computed once in `battle_start`); configured games
  (e.g. Sun Empire) are never affected.
- **Move assist** (`task_set_move_assist`, `sctasks.cpp`) — honours a move task
  action whose "To:" combo was left unset (`Var2 = -1`, destination in `Var3`)
  as "to room". The reference Runner ignores these, which in e.g. *To Hell &
  Beyond* traps the player in the mansion.

Public API is `scr_set_combat_assist` / `scr_get_combat_assist` and
`scr_set_move_assist` / `scr_get_move_assist` (`scarier.h`, `scinterf.cpp`).

## What already exists (do not re-do)

- **Metacommands** — `glk combatassist on|off` and `glk moveassist on|off` in
  `os_glk.cpp` (`GSC_COMMAND_TABLE`), each with an on-switch faithfulness
  warning, both listed in `glk summary` and `glk help`.
- **Per-game auto-defaults** — the `gsc_game_assist_t` table in `os_glk.cpp`
  turns the matching assist on at game start for the handful of known-broken
  games (matched on the TAF's GameName + GameAuthor, so every release is
  covered), printing a one-line notice explaining why. `glk ...assist off`
  still overrides. True 3.9/3.8-signature games are deliberately absent — the
  engine's legacy hit model repairs their combat unconditionally.
- **Headless harness** — `SCR_ASSUME_COMBAT` / `SCR_ASSUME_MOVES` env vars in
  `test/adrift4/harness/seed.cpp`.

Faithful default is unchanged: with no metacommand, no auto-list match and no
env var, Scarier stays byte-faithful to run400.exe.

## What is left

A GUI toggle in Spatterlight that **persists across sessions** — the
metacommands are per-session, and a fresh load or restart reverts to the
faithful default (or to the auto-list default). Add a Scarier preference in the
app, read at startup and again on settings change, applied through the same two
setters. Precedent for reading a preference live rather than only at boot:
`gli_determinism` in `os_glk.cpp`, and the Comprehend/Scott terps re-reading
theirs in `UpdateSettings`/`onArrange` (memory note
`comprehend-graphics-pref-onthefly`).

Do **not** bake the choice into the save file — it is a player preference, not
game state.

Files: the Spatterlight settings UI plus the Scarier controller that reads it.
No engine change is needed; the setters and getters are already public.
