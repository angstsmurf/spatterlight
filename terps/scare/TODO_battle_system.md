# TODO: finish the ADRIFT 4 Battle System port

## Status

SCARE historically did **not** implement ADRIFT's optional Battle System — the
TAF parser even discarded the combat data (the schema tables in `sctafpar.c`
tagged every battle field with the `i` "ignore integer" prefix). It now does,
to a faithful first cut. The combat model was reverse-engineered from the real
Runner's VB6 P-code (`run400.exe`, DotFix decompile → `Battles.bas`); jAsea, the
project SCARE derives from, has no combat code at all.

### Implemented (validated, no crashes across a 17-game battle corpus)

- Parser now **stores** battle data (`i`→`#` in the V4.0 and V3.9 `BATTLE`,
  `OBJ_BATTLE`, `NPC_BATTLE` tables, `sctafpar.c`).
- Runtime state: per-NPC `stamina` / `staminacounter` / `attackcounter` and
  player `playerstamina` / `playerstaminacounter` in `sc_npcstate_t` /
  `sc_game_t`, copied in `gs_copy()` (undo-safe) with `gs_*` accessors.
- Battle module in `scnpcs.c`: attribute rolls (`lo + Int(rnd*(hi-lo))`,
  high bound exclusive), effective strength/accuracy/agility/defence with
  weapon and worn-armour bonuses, `battle_resolve` (hit iff
  `effAccuracy > effAgility`; damage `effStrength − effDefence`), death,
  low-stamina task, attitude-based target selection, Speed-based attack
  cadence, and automatic stamina recovery.
- `status` / `status <character>` command (`sclibrar.c`); falls back to the
  status line when the Battle System is off.
- Player attacks wired into the existing `lib_cmd_attack_npc` /
  `lib_cmd_attack_npc_with` handlers (covers `attack/shoot/hit/kick` and
  `... with <weapon>`).
- `battle_tick()` hooked once per real turn after `npc_tick_npcs()`
  (`scrunner.c`). Obsolete `lib_warn_battle_system` call removed.

See `Battles.bas` analysis / formulas in the project memory
(`adrift-battle-formulas-re`, `scare-battle-system-port`).

## Remaining work

1. **Persistent `wield` command.** The Runner lets the player `wield <weapon>`
   and remembers it (`global_78`); SCARE currently auto-selects the best held
   weapon for every strike, and only honours an explicit weapon via
   `attack X with Y` for that one strike. Add a `wield`/`unwield` command and a
   stored wielded-weapon field, and use it as the default in
   `battle_best_weapon` / `battle_player_attack`.

2. **Weapon attack-method verbs.** Only `attack/shoot/hit/kick` reach combat.
   The Runner also accepts `chop/cut/stab/throw` and requires the wielded
   weapon's *method* to match the verb (method codes: 0 chop, 1 cut, 2 hit,
   3 shoot, 4 stab, 5 throw), printing e.g. "You can't chop with X!" on a
   mismatch. Also: `attack X with <non-weapon>` should say "X is not a weapon!"
   rather than silently giving a zero-bonus attack (expose
   `battle_object_is_weapon`, or re-check in the command handler).

3. **Drop a dead NPC's objects.** On death the Runner moves the deceased's held
   objects to the current room; `battle_kill` only hides the NPC, orphaning its
   inventory. Re-home held/worn objects to the room before hiding.

4. **Player death flow.** `battle_kill` for the player just prints
   "You are dead!" and sets `is_running = FALSE`. The Runner shows a proper
   death message and a restart/restore prompt — hook into SCARE's normal
   end-of-game handling instead.

5. **Save/restore of live stamina.** `scserial.c` is the ADRIFT-compatible
   `.tas` format; the in-stream position of the battle fields has not been
   reverse-engineered, so `ser_load_game` currently re-rolls stamina via
   `battle_start` (valid state, but mid-combat stamina is not preserved).
   RE the `.tas` battle layout and serialise the real values.

6. **`status` layout.** SCARE prints each attribute at its configured maximum.
   The Runner shows "Lo-Hi   <current incl. weapons/armour>" plus the wielded
   weapon. Match that once `wield` (item 1) lands.

7. **RNG fidelity note (won't-fix-able).** Combat is RNG-driven and the Runner
   uses VB `Rnd()`; SCARE uses `sc_randomint`, so turn-by-turn results can't be
   bit-matched — only the probabilities and damage are faithful. No regression
   golden is possible here (unlike the graphics ports); validate by behaviour.

## Test harness

`/tmp/scbattle_test.c` (kept in chat history, not in-tree) drives combat
directly: load a game, set `g->is_running = 1`, `battle_start`, force a kill by
setting a target's stamina to 1 and calling `battle_player_attack`, force an
enemy attack by relocating an enemy into the player's room and calling
`battle_tick`, and read output via `pf_get_buffer`. Build the console
interpreter with `clang -O2 -w sc*.c os_ansi.c -lz -o scare` (the bundled
`zlib123.zip` is absent; link the system `-lz`). Battle game corpus lives at
`/tmp/adrift40/battlegames/corpus/` (see `scare-battle-system-port` memo).

## Footgun

`battle_object_is_weapon` must use the non-fatal `prop_get`, **not**
`prop_get_boolean`: static objects have no `Weapon` property (schema
`?!BStatic:BWeapon`) and `prop_get_boolean` aborts with "can't retrieve
property".
