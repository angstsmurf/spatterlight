# TODO: make the combat assist and move assist user-settable

> **Status: Approach A DONE.** Both aids are now reachable in the Glk build as
> `glk combatassist on|off` and `glk moveassist on|off` (os_glk.c), each with an
> on-switch faithfulness warning, and both appear in `glk summary` and
> `glk help`. Public getters `sc_get_combat_assist()` / `sc_get_move_assist()`
> were added (scinterf.c / scare.h, backed by `battle_get_combat_assist` /
> `task_get_move_assist`). Faithful default is unchanged (both off). Approach B
> (a persistent Spatterlight GUI preference) is still open — see below.


SCARE has two opt-in, non-faithful aids that let an *otherwise-unwinnable*
amateur ADRIFT game be completed:

- **Combat assist** — `sc_set_combat_assist()` (`scbattle.c` / `scinterf.c`,
  committed `61e04e0f`). Forces an automatic hit in games that leave every
  character's Accuracy/Agility at 0 (so `acc>agi` = `0>0` never lands), letting
  combat play on the author's strength-vs-defence basis. Only acts on
  *fully-unconfigured* games (`battle_unconfigured`, detected at `battle_start`);
  configured games (e.g. Sun Empire) are never affected.
- **Move assist** — `sc_set_move_assist()` (`sctasks.c` / `scinterf.c`, committed
  `3cb9924e`). Honours a move task action whose "To:" combo was left unset
  (`Var2 = -1`, destination in `Var3`) as "to room"; the reference Runner ignores
  these, which in e.g. *To Hell & Beyond* traps the player in the mansion.

**Both are off by default** (SCARE stays byte-faithful to run400.exe) and are
currently reachable **only from the headless walkthrough harness**, via the env
vars `SC_ASSUME_COMBAT` / `SC_ASSUME_MOVES` in `harness/seed.c`. They are **not
wired into the Spatterlight Glk build at all** (`grep sc_set_combat_assist
os_glk.c` → nothing), so a normal player can never turn them on.

**Goal:** expose both as user-settable runtime toggles, so a player stuck in a
faithfully-broken game can opt in (with a clear "this diverges from the original
Runner" warning), without changing the faithful default.

---

## Approach A (recommended): `glk` metacommands in os_glk.c

SCARE's Glk port already has a local-command layer — the player types
`glk <subcommand> [arg]` and `os_read_line()` intercepts it before the game sees
it (gated on `gsc_commands_enabled`). The dispatch table is `GSC_COMMAND_TABLE`
in `terps/scare/os_glk.c` (~line 2523), e.g. `glk abbreviations on`,
`glk capacity 50`, `glk version`, `glk summary`.

Add two entries mirroring `gsc_command_abbreviations()` (the on/off + state-report
template, ~line 2292):

```
{"combatassist",  gsc_command_combat_assist, TRUE},
{"moveassist",    gsc_command_move_assist,   TRUE},
```

Each handler:
1. Parses `on`/`off` (empty arg ⇒ just report current state, for `glk summary`).
2. Calls `sc_set_combat_assist()` / `sc_set_move_assist()`.
3. Prints a confirmation **and a one-line faithfulness warning** the first time
   it's switched on (e.g. *"Combat assist on — note this deviates from the
   original ADRIFT Runner and is intended only for games whose combat data is
   broken."*).
4. Tracks its own `static int` enabled-flag for the report (there are currently
   **no getters** — either keep a local mirror flag like `gsc_commands_enabled`,
   or add `sc_get_combat_assist()` / `sc_get_move_assist()` to `scinterf.c`).

Also: include both in `gsc_command_summary()` and document them in
`gsc_command_help()` (and the `summary`/`help` text near line 2523–2620).

### Gotchas
- **Combat assist timing.** `sc_set_combat_assist(1)` only arms the assist; the
  actual auto-hit is gated on `battle_unconfigured`, which is computed once in
  `battle_start`. Verify that enabling it *before the first fight* works, and
  decide whether toggling mid-game (after `battle_start`) should re-run the
  unconfigured detection. (For move assist there's no such timing issue — it's
  checked per move action.)
- **Persistence.** Metacommands are per-session; a fresh load/restart reverts to
  off. That's acceptable (re-type the command), but decide whether to also
  persist the choice (see Approach B). Do **not** bake it into the save file.
- **Naming.** The `glk ` prefix avoids collision with game verbs; keep it.
- **`glk commands off`** disables the whole local-command layer — make sure the
  help text still mentions how to re-enable.

## Approach B (optional, complementary): a Spatterlight preference

For a GUI toggle (and persistence across sessions), add a SCARE preference in the
app, read at startup and on settings change, applied through the same two setters
— mirroring how other terps re-read options live (e.g. `gli_determinism` is read
in `os_glk.c:3849`; Scott/Comprehend re-read prefs in `UpdateSettings`/`onArrange`,
see memory `comprehend-graphics-pref-onthefly`). This is independent of Approach A
and can come later; the metacommands are the minimal, self-contained win.

## Verification
- **Faithful default unchanged:** with neither toggle, `To Hell & Beyond` is still
  unwinnable (`jump down` no-ops) and zero-accuracy combat still stalemates.
- **Combat assist metacommand:** in a zero-accuracy combat game (e.g. Town of
  Azra / Villains & Kings), `glk combatassist on` then fight → hits land.
- **Move assist metacommand:** in `To Hell & Beyond`, `glk moveassist on` then run
  the route in `harness/to_hell_and_beyond_assisted_solution.txt` → reaches the
  ship and ultimately the 248/373 throne win.
- **No regression:** with move assist on, X-Files (299/299) and Hyperbole
  (100/100) are byte-identical to assist-off (their unset moves are inert
  offstage NPC moves).

## Files
- `terps/scare/os_glk.c` — table entries, two handlers, summary + help text.
- `terps/scare/scinterf.c` / `scare.h` — *optional* `sc_get_*` getters (if not
  mirroring the flag locally in os_glk.c).
- (Approach B) Spatterlight settings UI + the SCARE controller that reads it.
