# Talisman part 2 (the desert) — finishing the walkthrough

The goal: extend `test/walkthrough/scripts/talisman.txt` from `SAIL SHIP` all
the way to the winning `BOW`, deterministically, then set the Talisman win
marker in `test/walkthrough/run_walkthroughs.sh` (replacing the part-1 marker
`"departing the Persian"`). Done when `sh test/walkthrough/run_walkthroughs.sh`
shows all 5 games PASS on the new marker.

## Current status (working tree)

**Steps 0–5 are DONE and in the script.** `talisman.txt` now drives
deterministically from `SAIL SHIP` through the desert, the torch-gated statue
maze, AND the moving-wall lever maze into the **secret demon room (room 72)**.
The interim marker is set to
`"A horrific figure of a demon is embedded within the room's southern wall"` —
all 5 games PASS. This validates the entire part-2 pipeline plus the fully-RE'd
maze (see step 5). The remaining work is **step 6 onward** (the counterweight
puzzle → amulet → carpet flight → genie/wizard → palace `BOW`); when that is
finished, replace the interim marker with the real victory string.

All step-0–4 routes below were found by BFS over `(room, var72)` with the
headless harness and pinned into the script. They are **prefix-specific**:
editing any earlier line re-rolls the whole desert, so never edit upstream.

## What is already done (committed: 74ee92c7, claudeslop; +steps 0–4 in tree)

- **Part-2 infrastructure.** Part 2 is a wholly separate game database in its
  own `g0` on the **Lands Beyond disk** (magic `0x94c6`): 87 rooms, 67 items,
  its own functions and string banks. `SAIL SHIP` fires `SPECIAL 16`, which
  `TalismanGame::enterPart2()` handles by swapping the database in (disk select
  + `loadGame()` + `loadStrings()`), carrying over flags/variables, and dropping
  the player on the ship's deck (room 51).
- **Inventory carry-over.** Abu walks ashore still holding everything from the
  end of part 1 (scimitar, coins, flask, **torch**, jade rod, **flint**, rope,
  shield, wet book). `enterPart2()` snapshots the held items and re-places them
  after the reload. Verified identical to the Apple II original (MAME).
- **The torch question is RESOLVED.** The torch (item 6) and flint (item 14) are
  **carried from part 1, never found in the desert.** Their static part-2
  placement (rooms 22/21, in the unreachable "canonical" room set 1–26) is
  vestigial — nothing in the bytecode moves the player there or moves those
  items. `LIGHT TORCH` now works (needs the flint; → "a flaming torch"), which
  is the hard gate for the statue maze (see below).

## How desert navigation works (read this first)

Movement in the desert is **not** a normal room graph. The rooms you actually
walk are the **scramble/hub set** (53–86, flagged `0x40`/`0x01`); their exits
all loop back, and each turn a `var72` (= var `0x48`) state machine — driven by
the direction handlers `FUNC 21/22/23/24` — decides which "feature" you see.
Landmarks appear at fixed `var72` values: **oasis** at 31, **statue** at
{78,81,82,100,102}, **camel** nearby; generic wandering shows room 54 ("trekking
over the dunes"). RNG (`var0x62 = rand()` each turn) only flavours messages; the
`var72` transitions are **deterministic** given the exact command sequence.

Consequences for scripting:
- **The route to each landmark is prefix-specific.** Change any earlier command
  and the whole desert re-rolls. So **build the script strictly forwards, never
  editing earlier lines** — re-search the next leg after each leg is locked.
- Find routes by **BFS over `(currentRoom, var72)` state** with the headless
  harness (see Tooling). Routes found this session (with a 16-`WAIT` wake
  prefix, illustrative only): oasis = `N W`; camel from oasis-with-figs =
  `N W E`. They will differ once the prefix changes.
- `std::rand()` is unseeded (`Comprehend::getRandomNumber`), so a fixed command
  prefix is byte-reproducible — a found route replays exactly.

## Step-by-step plan

Each step lists the **known commands** (confirmed live this session unless
marked) and the **open work** to turn it into script lines.

### 0.–4. Ship → desert → torch-gated maze entry — **DONE (in the script)**
The exact pinned sequence is now in `talisman.txt`. For reference:
- **0. Ship → beach.** `U U GET PARROT`, then **`WAIT`×20** (knockout ~13th
  wait, wake on beach room 55 ~17th; over-waiting is harmless, so 20 is safe).
- **1. Beach → oasis (room 63):** `N W`. Then `EXAMINE TREE`, `GET FIGS` (snake
  freezes you), `ABU, KILL SNAKE`, `GET FIGS` again (the **two** `GET FIGS` are
  mandatory).
- **2. Oasis → camel (room 61):** `N W E`. Then `GIVE FIGS` (must precede),
  `OPEN CYLINDER`, `GET CARPET` (the roc steals the carpet + drops the
  auto-kept note — by design). The note/catalog sub-plot is **flavour**; nothing
  gates the win on it.
- **3. Camel → statue base (room 62):** `E N E N N N N N E E`. Then
  `GIVE WATER TO PARROT` (staff drops), `GET STAFF`, `INSERT STAFF IN HOLE`,
  `IN` → room 73 (statue interior).
- **4. Torch gate:** `LIGHT TORCH` (carried torch + flint; sets flag 25), then
  `N` (`FUNC 247` checks flag 25) → room 69, the first maze room. Confirmed: the
  torch stays lit, `N` enters cleanly.

### 5. The statue maze → secret room (room 72) — **DONE (in the script)**

**Solution (from room 69, the maze entry):**
`N E E N D S W W` → `TELL ABU TO PULL LEVER` → `N E E U` → `S W W W S` → room 72.
Found by heuristic search over `(room, var33 % 3, flags)`; verified by full replay
(lands in room 72, "A horrific figure of a demon …"); the interim marker is now
that string and all 5 games PASS.

**Why it works (the maze fully RE'd):**
- Network rooms 64–75: each direction handler `func 21/22/23/24` (N/S/E/W) gates on
  a single wall flag — **N=80, S=82, E=81, W=83** (a *set* flag = wall *present* =
  blocked). Those flags are recomputed every turn, per room, by `func 9 →
  func 137`(rm 68)/`138`(rm 69)/… , each a **pure function of the lever-config
  flags 73,74,75,79** (table of setters: `func 144`=80,83; `147`=80; `150`=83;
  `151`=82,83; etc.). The "moving wall" is **flag 73 toggling on every third turn**
  (`var33 % 3 == 0`) — so the period is 3 and `var33 % 3` is a sound dedup phase.
- The secret room 72's only in-edge is `68 → S → 72`; `func 137` leaves S open
  (flag 82 clear) **only when flag 79 is clear**. Flag 79 is **set** at entry and
  the only thing that clears it is **`TELL ABU TO PULL LEVER`** in the right
  single-lever room (player's own pull can't — `func 167` short-circuits on
  `NOT_FLAG 6`; Abu's `func 265/269` takes a clearing branch). Clearing 79 also
  clears 74, so reaching 68 afterward needs the phase-timed walk above.
- Earlier searches missed it for one reason only: they used player `PULL LEVER`,
  never `TELL ABU TO PULL LEVER`. That single action was the whole gate.

<details><summary>Original investigation notes (superseded)</summary>

#### structure fully RE'd

The maze is two zones, both `var72`-display rooms:
- **Moving-wall network, rooms 64–75 (room flag `0x81`).** Walls cycle **every
  turn**, but **deterministically** (driven by the turn counter, e.g. `var33`,
  not by `rand`) — so a fixed command sequence replays exactly, but a move that
  worked at turn *t* is blocked at turn *t+1*. The direction handlers are
  `FUNC 21/22/23/24`; they gate each transition on the **wall flags 80/81/82**
  (`func 21` tests flag 80, `22` tests 82, `23` tests 81).
- **Static lever maze, rooms 76–81 (room flag `0x01`).** Here walls do **not**
  cycle (verified: from room 78, `N` is always blocked, `W`→77, `S`→81). Exits
  are recomputed from the **wall flags 76–82** (`funcs 135–140`, `SET_ROOM_GRAPHIC`
  per config). This is the "walls move no more — move them yourself with the
  levers" zone.

**The secret room is room 72** — the demon room with the **counterweight
statue** (`[s2:543]` "one arm holds a silver bowl, the other a silver dome";
examined by `func 194`, which gates on `IN_ROOM 72`). Its only static exit is
`N=68`; entering it slides a wall shut behind you (`[s2:674]`/`[s2:675]`). The
coin/counterweight state lives in `var120/121/122` (`func 12`/`func 13`).

**How the levers reach it.** A lever pull toggles a wall flag (the "swish",
`[s2:652]`): e.g. pulling the lever in room 78 sets **flag 76**, and `func 136`
then propagates `flag 76 → set flags 80,81,82,83`, which the *network* handlers
(`func 21/22/23`) read to open network exits. So you **set levers in the static
maze (76–81), then go back up into the network (67↔78 hole) and walk to room
72.** Lever commands:
- `PULL LEVER` → `func 167` (player). `TELL ABU TO PULL LEVER` → `func 169/265`
  ("Using all of his might, Abu shifts the stubborn lever!").
- 2-lever rooms (**76, 81**) ask "The short lever or the long lever?"
  (`[s2:653]`); answer **`LONG LEVER`** or **`SHORT LEVER`** (bare `LONG`
  fails). Per the source walkthrough: **long = left (west) wall, short = front
  (north) wall.**
- A single-lever pull is 1 turn; a 2-lever pull-then-answer is **2 turns** (the
  answer advances the turn counter — matters for network phase).

**You do NOT need the throb homing** (confirmed with the user). The throb
(`func 16`, `[s2:779]/[780]` louder, `[s2:650]/[651]` volume) is gated on flag
111 = **`WEAR AMULET`** (`func 239`) — but the amulet is the maze's *prize*, so
the throb can't be the search tool. It's flavour; the maze is a deterministic
lever puzzle.

**What was tried (this session).**
- The static lever maze (76–81) was **exhaustively** BFS'd over `(room,
  wall-flags)` — 96 reachable states, stable — and room 72/82 are **not**
  reachable from inside it. Confirms levers act on the *network*, not locally.
- Movement-only BFS through the network reaches 64–71,73,75 but never 72/74/82
  (those exits need the right wall flags). Pulling only lever 76 then navigating
  still didn't open 72.
- **Nothing does `MOVE_TO_ROOM 82`** — room 82 ("emerald glow … from the
  eastern exit") is the *gateway visual* (≈ canonical room 16), never the
  player's room. The actual secret room you stand in is **72**.

**The wall mechanics are now nailed down (this session).**
- **Walls are gated by flags 80/81/82/83 only**, tested by the direction handlers
  `func 21/22/23/24` (N/S/E/W, via action table 6, words 1–4). Those flags cycle
  **deterministically with the turn counter `var33`, period 3** (measured cleanly:
  from room 69, `W`→68 opens exactly when `var33 % 3 == 0`, over many cycles; room
  70's exits are turn-constant, i.e. period 1). The cycle is **not** rand-driven.
- **Everything else the handlers touch is constant in the maze:** `var72`=119,
  `var106`=0, `var107`=1, `var30`=0, `var125`=116, `var126`=117, `var127`=0 — all
  unchanging across network/lever moves. So the sound dedup key is just
  `(room, var33 % 3, flags)`; no hidden maze counter.
- **Levers do fire** (player *or* `TELL ABU` — identical; the ceiling-lever flavour
  is not a gate): in room 78 a pull **sets flag 76**; in room 81 `LONG LEVER`
  **clears flag 78** ("swish"). 2-lever rooms (76, 81) ask "short or long?" and the
  `LONG LEVER`/`SHORT LEVER` answer parses correctly in the headless harness.
- **Negative result — room 72 is unreachable from the maze entry.** A **sound,
  exhaustive** BFS with key `(room, var33 % 3, all flags)` over {N,S,E,W,U,D +
  every single- and 2-lever pull} explored ~1700 states (depth ≤24) and **never
  reached 72** (re-confirmed with the wider `var33 % 6` key and with the maze-var
  key — same negative). Room 72's *only* in-edge is `68 → S → 72`, and in **no**
  reachable `(phase, lever-config)` is 68's S exit open. This matches the prior
  session's failure: the lever→network propagation that *should* open 68→72 never
  happens in this port.

**Prime suspects for *why* 68→72 never opens (next leads, in order):**
1. **A stale carried part-1 flag** (see Open caveats: `enterPart2()` copies *all*
   part-1 flags). If a carried flag sits in the 80–83 wall set or disables the
   propagation `func 136` (flag 76 → set 80,81,82,83), the levers can't reach the
   network. Diff the room-69 flag set against the real Apple II maze entry in MAME
   and clear the offender; then re-run the BFS.
2. **A bug in the lever→network propagation.** Pulls visibly toggle 76–79 but the
   exhaustive search shows those never translate into an open 68→S. Decode `func 22`
   (SOUTH) precisely — find the branch that does the 68→72 move and the exact flag
   predicate it needs — then check whether any lever combo can satisfy it. This is
   the principled finish; the BFS proves it's *not* a search-coverage gap.

The source walkthrough hint (different RNG/version — **moves do not transfer**,
only the method): `N E E N D … W W S, TELL ABU TO PULL LEVER, (long left /
short front), E, S E, N, U S W W W S → secret room`. Re-reading this — the
`TELL ABU TO PULL LEVER` and the `U S W W W S` tail — was what cracked it.

</details>

### 6. Secret room → amulet (the counterweight puzzle) — **MOSTLY DONE; escape OPEN**
A statue with a **bowl + dome counterweight** (`[s2:543]`). Confirmed live in room 72:
- **`PUT COINS IN BOWL`** drops *all* carried coins into the bowl at once (no count
  form — `PUT 50 COINS` is a no-op, `FILL BOWL` "I understand thee not"). The bowl
  arm lowers / dome rises (`[s2:672]`), **and a wall slides shut sealing the only
  exit** (the demon's expression flips to "jeering / trapped"; `func 11` sets room
  graphic via flag 205 while `var123` = coins-in-bowl > 0).
- **`GET AMULET`** then succeeds → **"a throbbing talisman"** enters inventory
  ("As ye take the talisman … it is a bit warm"); the coins leave inventory.
- `TAKE COINS FROM BOWL` empties the bowl (`var123 → 0`, `[s2:673]`, "the two arms
  slowly pass the point of equivalence").
- **`WEAR AMULET`** → "THUMP! THUMP! (THROB! THROB!)" — activates the throb (flag
  111 / `func 16`).

**OPEN — escaping room 72.** After `GET AMULET` the exit `N` stays blocked even
after emptying the bowl. The counterweight vars: `var121` (= bowl-arm level) tracks
`128 + coins_in_bowl − amulet_weight`; it always equals `var122`, so "balance" must
be `var121 == var120 (128)`. 137 coins → 265; 0 coins (after taking the amulet) →
86; so the new balance needs ~`amulet_weight` (≈42?) coins left in the bowl — but
there's no count form yet. Decode `func 11`/`func 12` and the wall object (item 28,
`CLEAR_INVISIBLE 28` / room-graphic 205) to find what re-opens N: likely *exact*
re-balance, or the throb/amulet itself. Brute-force from the step-5 endpoint.

### 7. Maze exit → carpet flight (rooms 83–86)
The rideable rug for the endgame is **not** the stolen camel carpet — work out
where it comes from (the secret room is the likely source; check what the amulet
/ secret room yields). Then the flight: `DROP RUG`, `SIT`, `FLY RUG`, `WAIT`,
`GET UP`, navigate (rooms 27–30 / 83–86 are the flight scenes).
- Open work: identify the rug item + where you obtain it; confirm the flight
  command cadence.

### 8. Genie / wizard / palace ending → BOW
Roughly (from the source walkthrough, to be verified line-by-line):
`E, E, SPEAK, POUR WATER ON LAMP, GET LAMP, RUB LAMP, TELL GENIE TO KILL WIZARD,
YES, W, W, SIT, FLY, WAIT, BOW`. The genie/wizard cave is rooms 31/32, the
palace ending 33–36.
- Open work: drive it to the final `BOW`, capture the **exact victory string**,
  and set it as the new Talisman win marker.

## Tooling / reproduction

- **Build:** `make -f Makefile.headless` → `comprehend_hl`. **Clean rebuild
  (`make -f Makefile.headless clean` first) after any change to `game.h`** — the
  Makefile doesn't track header deps (ODR/sizeof footgun).
- **Run a script:**
  `COMPREHEND_SCRIPT=/abs/cmds.txt ./comprehend_hl -g talisman "/abs/path/Talisman… Boot.woz"`
  Use an **absolute** woz path (`glk_main` chdir's into the game dir first). The
  Empire / Lands Beyond sides auto-load from the same directory.
- **Strip comments** (the committed script has `#`/blank lines that the runner
  filters): `grep -v '^[[:space:]]*#' s.txt | grep -v '^[[:space:]]*$' > cmds.txt`.
- **Checkpoint meta-commands `#savestate <path>` / `#loadstate <path>`** (added
  this session, `game.cpp` + `Comprehend::saveStateToPath/loadStateFromPath`):
  dump/restore the live game state to a host file, no turn elapses. **The save
  trailer also records the `std::rand()` call count** (`Comprehend::_randCalls`);
  on load it does `srand(1)` + burns that many draws to realign the PRNG (the
  engine never seeds, so the live stream is the default `srand(1)` sequence). This
  makes a checkpoint faithful for *variables, flags, item/room state, and rand
  position*. **Caveat — it is NOT faithful for transient parser state** (the
  current-noun / `SAVE_ACTION` query carried from the previous sentence): the
  Talisman maze handlers `func 21/24` (N/W) read it, so a checkpoint taken at room
  69 diverges from a live run on the *first* move after restore. Use full-prefix
  replay (below) for maze search, where every run is truthful; the checkpoint is
  good for non-parser-sensitive debugging and was how the rand-gating was found.
- **Route search (BFS):** two getenv-gated probes are **currently present** in
  `game_tm.cpp` (read-only; remove before a clean commit, this section documents
  re-adding them):
  - `TM_TRACE` in `beforePrompt`: one line per prompt — `room=`, `var72=`,
    `var98=`, `held={…}`, **all nonzero `vars={i:v …}`**, and **all set
    `flags={…}`**. (The vars/flags dumps were what let step 5 be RE'd.)
  - `TM_DUMP` at the end of `enterPart2()`: dumps rooms (exits+desc), items,
    functions (normalized opcode via `getScriptOpcode(&in)` + operands), **both
    string tables (`_strings` and `_strings2`, indexed `[s0:n]`/`[s2:n]`)**, the
    dictionary (idx/type/text), and the action tables (with resolved word text),
    to `/tmp/tm_part2_dump.txt`.
  Then a Python BFS that replays `prefix + moves + LOOK`, parses the last
  `TM_TRACE` line, and enqueues new states. **Soundness of the dedup key:**
  - Desert hub (53–63) and static lever maze (76–81): walls don't cycle per
    turn, so key by `(room, var72[, wall-flags])` — sound and small. (Static
    maze fully enumerates at 96 states.)
  - Moving-wall network (64–75): walls cycle by turn counter, so the phase =
    turn count matters; a sound key must add it (`var33`), which makes the state
    space explode. A **found** path is always a real replay regardless, so an
    unsound key still yields valid (if not shortest/exhaustive) paths.
  - **Footgun:** drive each candidate from one **fixed temp file** run
    sequentially. A shell `for`-loop calling a throwaway `diffprobe` gave
    *inconsistent* room results (harness artifact, not the game — the binary is
    deterministic on a fixed input file). `bfs.py` (one reused temp file per
    run) is reliable.
- **Reading the dump:** opcode operands referencing items are **1-based** (figs =
  dump item 20 → operand 21; torch = item 6 → operand 7). Strings: `PRINT n 128`
  = table `0x80` (`_strings[n]`); `PRINT n 132` = `0x84` (`_strings2[0x200+n]`);
  `PRINT n 133` = `0x85` (`_strings2[0x300+n]`). Raw-opcode bit `0x40` **inverts**
  a test (raw `0x59` = `NOT_FLAG`). A function runs the command block of the
  **first** satisfied test-group, then stops at the next test.

## MAME ground truth (Apple II = the real interpreter)

`~/mame/sta/apple2e/beach.sta` is on the shore. Launch
`mame_launch apple2e -flop1 <Boot.woz>`, `manager.machine:load("beach")`, then
mount Lands Beyond:
`manager.machine.images[":sl6:diskiing:0:525"]:load(<side3.woz>)`. Read the
40-col text screen via the interleave `base = 0x400 + (n%8)*0x80 + (n//8)*0x28`,
40 chars/row, `byte & 0x7f`. Use it to learn **mechanics and exact room/message
text** — do **not** copy its move list (its RNG ≠ the headless `std::rand`).
Footgun: a `.sta` saved mid-disk-flux freezes the bridge on load; save with the
drive idle. (See [[mame-mcp-savestate-bridge-freeze]].)

## Reverse-engineering anchors (NOVEL1.EXE in Ghidra; DOS proxy for the Apple logic)

- `tm_main_command_loop` @ `1000:05f0`; `tm_handle_special_opcode` @ `1000:06f0`
  (reads the opcode from `0x35e2`; cases 16/17 fall through into a fresh main
  command loop — modelled by `REDO_TURN`).
- `tm_run_function_by_index` @ `1000:0690`; `FUN_1000_1b20` runs the function in
  `0x34b4`. Opcode dispatcher `FUN_1000_0c77`. Variables base `0x34bb`; current
  room `0x34b9`; flags a bit array.
- Key part-2 functions (from the dump): direction/scramble handlers 21/22/23/24
  (mutate `var72`; gate transitions on wall flags 80/81/82); feature pickers 6 &
  72 (`var72` → oasis/camel/statue rooms); camel `GIVE FIGS` = 120,
  `OPEN CYLINDER` = 116, roc-steal = 119; statue arrival = 38, staff drop = 256,
  statue-`IN` = 247; `LIGHT TORCH` = 67, generic `LIGHT` = 51; `DIG` = 192
  (flavour only).
- **Maze (step 5) functions:** `func 167` = `PULL LEVER`; `func 169/265` =
  `TELL ABU TO PULL LEVER`; `funcs 135–140` recompute the static-maze exits/
  graphics from wall flags 76–82; `func 136`: `flag 76 → set flags 80,81,82,83`
  (propagates a lever pull into the network); `func 12/13` = counterweight
  balance daemon (`var120/121/122`); `func 194` = `EXAMINE STATUE` (gates on
  `IN_ROOM 72`); `func 16` = throb homing (gated on flag 111 = `WEAR AMULET`/
  `func 239`). Wall-config flags = **76,77,78,79,80,81,82**.
- **Endgame:** carpet flight is `func 10` (room state machine 83→84→85→86→37→4)
  driven by `func 15` (the `FLY`/ride handler, called repeatedly); the genie/
  wizard cave is rooms 31/32, the palace 33–36, the winning `BOW` in the
  audience chamber room 36 (`[s0:86]`).
- **Canonical room set 1–26.** Part 2's data carries a *second, clean* copy of
  the whole desert+maze with **real fixed exits** (room 1 beach → 2 oasis → 5/6
  camel → 7 statue → 8–17 maze, **17 = the green-glow cavern**, 14/26 = demon).
  The walked rooms 53–86 are the `var72`-scramble mirror of this; 1–26 is the
  cleanest reference for the underlying topology. (Earlier notes called 1–26
  "vestigial" — they're not used as `_currentRoom`, but they document the map.)

## Open caveats

- **Carried flags.** `enterPart2()` copies *all* part-1 flags into part 2. Only
  the coin variable is known-required; a stale part-1 flag could mean something
  different in part 2. If a later puzzle misbehaves, suspect a carried flag.
- **DOS build.** The part-2 disk swap is gated on the Apple disk-image VFS; the
  DOS `novel.exe` build would need its own part-2 path (its strings are two
  embedded segments). Out of scope for the Apple walkthrough.
- **Cosmetic.** After `INVENTORY`, the next room redraw leaks the coin count into
  an `@` placeholder ("span the 137ern horizon") — a pre-existing string-
  replacement slot collision, unrelated to the part-2 work. Worth a separate fix
  but doesn't block the (text-marker) regression.
- **Pictures** (lower priority): part-2 room/item art is RE–RG / OE–OF on the
  Lands Beyond disk and resolves through the active-disk VFS, so it should "just
  work"; spot-check against MAME, treat mismatches as a separate graphics task.
