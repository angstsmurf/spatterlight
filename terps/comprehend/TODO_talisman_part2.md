# Talisman part 2 — TODO

Finishing a full-game Talisman walkthrough (the second half: the desert) is
blocked on engine bugs in the part-2 transition. This file records what is
known, how to confirm the fixes against the original interpreter, and how to
build out the rest of the walkthrough.

## Status

Committed and working:

- First-half walkthrough: `test/walkthrough/scripts/talisman.txt` plays from the
  cell to boarding the ship ("departing the Persian"), verified via
  `test/walkthrough/run_walkthroughs.sh` (all 5 games PASS).
- Magic-lamp room save/restore fix (special opcode 3) — commit `0f6171d6`.
- Bare-number answers re-use the question verb (shop clerk) — commit `d2470653`.
- Inverted `VAR_GTE`→`VAR_LT` comparison opcodes — commit `972f2c47`.

Not done: everything after `SAIL SHIP` (the desert half).

## Tooling / reproduction

- Headless build: `make -f Makefile.headless` → `comprehend_hl`. **Clean rebuild
  (`make -f Makefile.headless clean` first) after any change to `game.h`** — the
  Makefile doesn't track header deps, and adding a member to `ComprehendGame`
  silently corrupts incrementally-built objects (ODR/sizeof mismatch).
- Run a script:
  `COMPREHEND_SCRIPT=/tmp/foo.txt ./comprehend_hl -g talisman "<ABSOLUTE path>/Talisman- Challenging the Sands of Time side 1 - Boot.woz"`
  Use an **absolute** woz path: `glk_main` chdir's into the game dir first, so a
  relative path stops resolving. The companion sides (Empire / Lands Beyond) are
  auto-loaded from the same directory.
- RNG is `std::rand()` with **no seed** (see `Comprehend::getRandomNumber`), so
  every run is reproducible. The "unmappable" desert is therefore solvable by a
  fixed move sequence — it just has to be found by brute force for this RNG.
- Debugging technique used throughout: a `getenv`-gated scan/trace block in
  `TalismanGame::playGame` to dump bytecode functions
  (`for (ii...) fprintf(..., fn[ii]._opcode, fn[ii]._operand[...])`), plus a
  per-instruction trace in `ComprehendGame::eval_instruction` and a PRINT trace
  in `OPCODE_PRINT`. Map raw opcodes through `_opcodeMap[getOpcode(&instr)]`.

## Reverse-engineering anchors (NOVEL1.EXE, open in Ghidra)

- `tm_main_command_loop` @ `1000:05f0` — the turn loop and the pending-question
  redirect (`var124/125/126` at `0x35b3/0x35b5/0x35b7`; the forced verb `0x9593`).
- `tm_handle_special_opcode` @ `1000:06f0` — special-opcode dispatcher; reads the
  opcode from `0x35e2`.
- `tm_run_function_by_index` @ `1000:0690`; `FUN_1000_1b20` @ `1000:1b20` runs the
  function indexed by `0x34b4`.
- `tm_match_action_to_function` @ `1000:0a40`; `tm_search_action_table` @ `1000:1290`.
- Opcode interpreter dispatcher: `FUN_1000_0c77` (table at DS:0x13f, handlers at
  DS:0xb0).
- Variables: base `0x34bb` (= var0), 2 bytes each. Flags are a bit array.
- Sentence parse: `tm_parse_sentence` @ `1000:09a3`, tokenizer `FUN_1000_1170`,
  number handler `FUN_1000_1670` (writes var0 = `0x34bb`), dict lookup
  `FUN_1000_1400`, word store `FUN_1000_1630`.

## BUG 1 — boat departure loops (part-2 entry)

### Symptom
After `SAIL SHIP`, every turn re-prints "Ye splash on board the boat… / sailing
alone… / [buoy] / What doth ye wish to take?" and the player's answer is never
processed. The "what to take" question can't be answered, so the desert is
unreachable.

### Diagnosis (confirmed in the loaded bytecode)
- Room **51** (`0x33`) is the part-2 scene room.
- Daemon **function 20** (an each-turn function) at ~off 17–19:
  `TEST flag45` → `PRINT(103,130)` "splash on board" → `SPECIAL 16`.
- `SPECIAL 16` (our `game_tm.cpp` `case 16`) sets room 51, runs **function 44**,
  and sets `REDO_TURN`.
- Function 44: `TEST NOT_IN_ROOM(5)` → `CALL_FUNC(30)` → prints; function **30**
  is the "What doth ye wish to take?" handler — it `PRINT(148,129)` + `SAVE_ACTION`
  when no object is named, else takes the named object (`TAKE_CURRENT_OBJECT`).
- **flag 45 is SET (funcs 128 off 8/29, 462 off 10) and CLEARED nowhere** in the
  bytecode (scanned all functions for `op99`/`op9d` with operand 45). So the
  daemon re-fires the departure every turn.
- The question is asked by a **daemon** (during `beforeTurn`), i.e. *before* the
  player's reply is parsed that turn, so function 30 never sees the answer.

### Why the obvious fix is wrong
Removing `REDO_TURN` from `case 16` stops the infinite redo **but breaks the
opening**: the cell→King reprieve also routes through `SPECIAL 16`, and without
the redo the intro prints `BAD_STRING(8011)/(0033)`. So `SPECIAL 16` is shared
between a fire-once scene (reprieve, needs redo) and an every-turn daemon
(boat). The real bug is the boat daemon re-firing, not the redo.

### What to confirm against the original, then fix
In `tm_handle_special_opcode` case `0x10` (decompiled):
`FUN_1000_08bb()` (sets `0x9590 = 1`), `0x34b9 = 0x33` (room 51),
`if (0x34ba != 5) FUN_1000_1a36()`, then `tm_run_function_by_index()` — **no
redo in the original**. Open questions to answer in Ghidra/DOSBox:
1. **Which function** does the original's case 0x10 run? (Determine the `AX`
   passed to `tm_run_function_by_index`; our port hardcodes func 44 — verify.)
2. **How does the original fire the boat scene only once?** flag 45 never
   clears, so there must be a second gate. Candidates: `0x9590` (set to 1 by
   `FUN_1000_08bb`), or the run-function setting a "departed" flag, or the
   daemon's flag-45 block being guarded by another test our trace mis-grouped.
   Re-dump function 20 and resolve the exact test/command grouping around
   off 15–19 (the `CLEAR flag2 / SET flag3 / TEST flag45` sequence).
3. Best confirmation: run **NOVEL1.EXE in DOSBox (dosbox-mcp)** to the boat and
   watch the scene fire once, inspecting the flag bitfield before/after to see
   what changes. (Reaching it needs a DOS savegame or a scripted playthrough.)
4. Likely fix shape: make the boat daemon's `SPECIAL 16` self-limit (set/clear
   the gate it tests), without touching the reprieve's redo path — i.e. the fix
   belongs in how the daemon or the run-function manages its gate, not in
   `case 16`'s `REDO_TURN`.

## BUG 2+ — anything past the boat

Once the boat is answerable, continue building `scripts/talisman.txt` from the
source walkthrough's "SECOND PART", expecting more divergences/bugs:

```
U, GET PARROT, WAIT (until you wake on a shore)
OASIS:  EXAMINE TREE, GET FRUIT, TELL ABU TO KILL SNAKE, GET FRUIT
CAMEL:  GIVE FRUIT, OPEN CYLINDER, LOOK IN CYLINDER, GET CARPET
STATUE: GIVE WATER TO PARROT, GET STAFF, INSERT STAFF IN HOLE, IN, LIGHT TORCH
maze:   N E E N D  (walls/levers move; WAIT here and there)
        W W S, TELL ABU TO PULL LEVER, then set levers (long left / short front),
        E, S E, N, U S W W W S  -> secret room
        GET AMULET, PUT 42 COINS IN BOWL
        N E E E N D W W S, TELL ABU TO PULL LEVER, N E S E N, U S W W S S S
end:    DROP RUG, SIT, FLY RUG, WAIT, GET UP, E, E, SPEAK,
        POUR WATER ON LAMP, GET LAMP, RUB LAMP, TELL GENIE TO KILL WIZARD, YES,
        W, W, SIT, FLY, WAIT, BOW   (-> game won)
```

Notes:
- The desert "cannot be mapped" — directions land you in random places. Because
  the headless RNG is fixed, brute-force the working sequence move-by-move and
  hard-code it. Watch the thirst hazard ("Ye art growing rather thirsty"):
  DRINK / FILL FLASK as needed; minimise wasted turns since the maze and end are
  time-limited.
- The moving-wall maze needs the lever orientations described above; verify each
  step's room string against the transcript.
- 42 coins must be in inventory for `PUT 42 COINS IN BOWL` — the first half
  collects them at the pit (`GET COINS`); confirm the count survives.

## Finishing

- Capture the final victory string after the closing `BOW` and use it as the new
  win marker in `run_walkthroughs.sh` (replacing "departing the Persian").
- Keep the whole run deterministic; re-run `run_walkthroughs.sh` to confirm all
  games still PASS.
