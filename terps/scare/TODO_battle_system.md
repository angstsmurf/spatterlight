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

## Done since the first cut

1. **Persistent `wield` command.** DONE. New `playerwield` field in
   `sc_game_t` (gs accessors `gs_(set_)playerwield`, copied in `gs_copy`,
   initialised to -1, undo-safe). `wield <weapon>` / `unwield` commands in
   `sclibrar.c` (`lib_cmd_wield` / `lib_cmd_unwield`, grammar in `scrunner.c`),
   requiring a held weapon. `battle_player_default_weapon()` returns the wielded
   weapon if still held and a weapon, else the best carried; both
   `battle_player_attack` and the `status` display default through it.

2. **Weapon attack-method verbs.** DONE. `attack/fight/kill/kick/slap` stay
   generic (no method requirement); `chop/cut/hit/shoot/stab` (and
   `throw <weapon> at <character>`) require a wielded weapon whose *method*
   matches (0 chop, 1 cut, 2 hit, 3 shoot, 4 stab, 5 throw), printing
   "You can't <verb> with X!" on a mismatch. `attack X with <non-weapon>` now
   says "X is not a weapon!" (`battle_is_weapon` / `battle_weapon_method`
   exposed). Shared cores `lib_battle_attack_bare` / `_with` + thin per-verb
   wrappers; method verbs without a prior grammar entry (`chop/cut/stab/throw`)
   fall through unchanged when the Battle System is off, so non-battle games are
   unaffected (legacy verbs keep their canned responses). **Interpretation
   note:** the method check only fires when a weapon is actually held — an
   unarmed `chop/hit/...` still lands a bare-handed blow rather than being
   rejected; this keeps unarmed combat working and matches "the *wielded*
   weapon's method must match" literally.

3. **Drop a dead NPC's objects.** DONE. `battle_kill` re-homes the dead NPC's
   held and worn objects to the room it died in (captured before any KilledTask
   can relocate it) via `gs_object_to_room`, then hides the NPC. Validated in
   Sun Empire (Mhentsh's object 19 moves from held to its room on death).

4. **Player death flow.** DONE. `battle_kill` for the player now prints
   "I'm afraid you are dead!" and sets `is_running = FALSE` **and**
   `has_completed = TRUE`, ending the game through SCARE's normal completion
   path (so the interpreter offers its restart/restore prompt). Validated in
   Shadow of the Past.

6. **`status` layout.** DONE. Each attribute now prints as
   "Lo-Hi   <current incl. weapons/armour>" (fresh effective roll via
   `battle_attribute_report`), and the wielded weapon is named last
   ("You are wielding …" / "<NPC> is wielding …"). Validated in Sun Empire.

5. **Save/restore of live stamina.** DONE for SCARE's own save/restore. The
   live combat state — player stamina / stamina-counter / wielded weapon and
   every NPC's stamina / stamina-counter / attack-counter — is now serialised in
   `scserial.c`, written (for battle games only) as extra lines after the ADRIFT
   turns count, introduced by the sentinel `SER_BATTLE_MARKER`
   ("ScareBattleState/1"). `ser_load_game` first re-rolls via `battle_start`,
   then, if the sentinel follows the turns count, overrides with the saved
   values. The sentinel's absence — a real ADRIFT save, or a SCARE save from
   before this change — falls back to the re-roll (`taf_next_line` returns NULL
   at end of data without faulting, so the peek is safe). Validated: live state
   round-trips byte-for-byte across the battle corpus; non-battle saves are
   unchanged; a battle save with the trailing block stripped still restores.

   The real Runner's `.tas` layout is now **reverse-engineered** from
   `run400.exe`'s `savegame` (@477318, in `Form1.frm` of the decompile — the
   sub bodies are present but the `.frm` carries embedded NULs, so `grep -a` /
   strip-NULs is needed to read them; `opensave` @460010 is the *high-score*
   reader, not the game restore). The Runner **interleaves** the combat state,
   both blocks gated on the battle-system flag (`MemVar_494282`):
   - **Player** (15 values, sub-struct `player(40)`), written right after the
     player position/encumbrance fields and before the room-seen loop, in this
     order of sub-struct indices: `6 2 | 12 10 8 | 24 22 20 | 18 16 14 |
     30 28 26 | 38` — i.e. MaxStamina, **live Stamina (idx 2)**, then
     Max/Hi/Lo of Strength, Defence, Accuracy, Agility, then one trailing field.
   - **NPC** (17 values, sub-struct `npc(172)`), written inside each NPC's loop
     iteration *after* location (`npc(14)`) and seen (`npc(26)`) and *before*
     the walk-step loop: `0 6 2 | 12 10 8 | 24 22 20 | 18 16 14 | 30 28 26 |
     32 36` — Attitude, MaxStamina, **live Stamina**, the four Max/Hi/Lo
     attribute triples, then two trailing fields.

   **Still open (full Runner interop):** matching this byte-for-byte is blocked
   by two pre-existing gaps, not by missing knowledge:
   1. SCARE's stream already omits ADRIFT's leading version line
      (`<chr 172>` + `major`/`minor`/`rev`), so SCARE `.tas` files are not
      Runner-loadable, and SCARE cannot load Runner saves, irrespective of
      battle data.
   2. The interleaved blocks are mostly the **mutable** Strength/Defence/
      Accuracy/Agility Max/Hi/Lo values that ADRIFT's "Change <attribute>" task
      action edits at runtime; SCARE does not implement that action and re-rolls
      those attributes from the bundle, so it has nowhere to round-trip them.
   The wielded weapon is **not** in the Runner's save at all — it resets it to
   "none" on load (`global_78 = &HFF`); SCARE's choice to persist it is a benign
   superset. Closing item 5 fully therefore means adding the version-line header
   and a mutable battle-attribute state (plus the Change-attribute task action),
   a larger change than the live-state preservation this task delivered. The
   mutable battle-attribute half is now **done** (`TODO_battle_attributes.md`);
   the remaining file-format work has its own plan in **`TODO_save_compat.md`**
   (version header, interleaved battle blocks, field parity, bidirectional
   Runner interop).

8. **Mutable battle attributes (the "Change `<attribute>`" task action).**
   ADRIFT task action type 7 lets tasks change a character's Attitude / Stamina /
   Strength / etc. at runtime; SCARE currently parses-and-discards it
   (`?#Type=7:iVar1,iVar2,iVar3`) and the executor is a stub
   (`case 7: ... break;`). Used by **11 of 17** corpus games, so worth doing.
   Full plan, corpus usage, RE anchors and the cheap Attitude-only first slice
   are in **`TODO_battle_attributes.md`**.

7. **RNG fidelity note (won't-fix-able).** Combat is RNG-driven and the Runner
   uses VB `Rnd()`; SCARE uses `sc_randomint`, so turn-by-turn results can't be
   bit-matched — only the probabilities and damage are faithful. No regression
   golden is possible here (unlike the graphics ports); validate by behaviour.

## Test harness

`~/adrift-battle/harness/scbattle_test.c` and `scbattle_test2.c` drive combat
directly: load a game, set `g->is_running = 1`, `battle_start`, force a kill by
setting a target's stamina to 1 and calling `battle_player_attack`, force an
enemy attack by relocating an enemy into the player's room and calling
`battle_tick` (test2 also checks dead-NPC object drop, player-death
`has_completed`, and `wield`/default-weapon), and read output via
`pf_get_buffer`. `scserial_test.c` round-trips live battle state through
`ser_save_game`/`ser_load_game` over an in-memory compressed buffer (set
distinctive stamina/wield, save, perturb, load, assert), and
`scserial_compat.c` checks that battle and non-battle saves both round-trip
turns/score/room. Build a harness with
`clang -O1 -w -I. sc*.c sxstubs.c sxglob.c sxutils.c <probe>.c -lz -o out`, or
the console interpreter with `clang -O2 -w sc*.c os_ansi.c -lz -o scare` (the
bundled `zlib123.zip` is absent; link the system `-lz`). Battle game corpus
lives at `~/adrift-battle/games/` (see `scare-battle-system-port` memo).

## Footgun

`battle_object_is_weapon` must use the non-fatal `prop_get`, **not**
`prop_get_boolean`: static objects have no `Weapon` property (schema
`?!BStatic:BWeapon`) and `prop_get_boolean` aborts with "can't retrieve
property".
