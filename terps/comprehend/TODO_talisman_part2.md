# Talisman part 2 (the desert) — finishing the walkthrough

The goal: extend `test/walkthrough/scripts/talisman.txt` from `SAIL SHIP` all
the way to the winning `BOW`, deterministically, then set the Talisman win
marker in `test/walkthrough/run_walkthroughs.sh` (replacing the part-1 marker
`"departing the Persian"`). Done when `sh test/walkthrough/run_walkthroughs.sh`
shows all 5 games PASS on the new marker.

## Current status (working tree) — COMPLETE ✅

**The full game is won, deterministically.** `talisman.txt` now drives all the
way from the executioner's cell to the winning `BOW` before King Darius. The win
marker is `"the Empire shalt flourish through eternity"` (King Darius's victory
speech) — all 5 walkthroughs PASS, and the Talisman win replays identically
across runs (the engine's `std::rand` is unseeded, so a fixed command prefix is
byte-reproducible).

Two root-cause bugs blocked the finale; both are fixed:

1. **The flying carpet was lost forever (roc steal).** `GET CARPET` (func 119)
   keeps the carpet only when **flag 40** is set; otherwise the roc snatches it
   and nothing ever returns it from limbo, so the carpet flight — and the whole
   ending — was unwinnable. Flag 40 is a *carried part-1* flag, set by the
   mail-order **order-confirmation** function (func 12) which fires only when the
   two Arabian-Express card codes are typed **together** on one line
   (`rj8906 ta3756`). The committed script had split them across two lines, which
   completes the order via the ordinary path (sets only flag 39). **Fix: one
   line `rj8906 ta3756`** — no engine change. (The card form still delivers the
   bridge log, so part 1 is unaffected.)
2. **The genie's lamp never appeared in room 39 (step 8 dead end).** The part-2
   initialiser **func 44** (→ func 45 → func 160: endgame room graphics, item
   long-descriptions, **lamp → room 39**, and arming the maze via flag 79) has no
   in-bytecode caller — NOVEL.EXE runs it from special-opcode 16. `enterPart2()`
   had dropped that call, so the red-hot lamp was missing and the genie could
   never be summoned. **Fix: `eval_function(44, nullptr)` at the end of
   `enterPart2()`** (game_tm.cpp). Verified safe: the maze still reaches room 72
   (the entry lever puzzle re-clears flag 79), and the lamp is now present.

### Steps 7–9 solution (now in the script)
- **7. Maze exit + carpet flight.** Taking the amulet starts a collapse timer
  (the throb; statue buries you if you dawdle). Escape: `N E E E N D W W S`
  to the single-lever room (the only one whose `TELL ABU TO PULL LEVER` sets the
  maze wall-flag 79 *with the talisman held*), arm flag 79, then reset the lever
  bank to the one wall configuration that opens the walk back to the statue
  interior (room 73) — `E E, PULL LONG LEVER, N, PULL LEVER, W, PULL LEVER` →
  `E U S W W S OUT` — and the statue collapses, dropping you on the open desert
  (room 54) with the carpet still in hand. Then `DROP RUG, SIT CARPET, FLY`,
  ride it (`WAIT`×3) to the cliff lip, `GET UP, E, E` into the magician's cave.
- **8. The genie defeats Schazabe.** `DOUSE LAMP` (twice — his talisman-grab eats
  the first turn), `GET LAMP, RUB LAMP` (the genie erupts and stalls his spell),
  `GENIE, KILL WIZARD`, `YES`. (Needs water in the flask — filled at the oasis.)
- **9. Palace + bow.** `W W, SIT CARPET, FLY`, `WAIT`×2 to the gates,
  `GET UP, N, E, BOW` → victory.

---

## Historical RE notes (the path to the solution — superseded by the above)

**Steps 0–6** drive deterministically from `SAIL SHIP` through the desert, both
mazes, into the secret demon room (room 72), and solve the counterweight bowl to
escape.

### Step 6 breakthrough — the bowl, and the part-1 economy fix it forced

The escape was gated on TWO bugs, both now fixed:

1. **Missing opcodes 0x8d / 0x91.** These V2 opcodes were unmapped
   (`OPCODE_UNKNOWN`, silent no-ops). They are `var[op0] += var[0]` and
   `var[op0] -= var[0]`, where var 0 is the input-number register (the count the
   player typed). Confirmed byte-exact against the **original DOS interpreter**
   (NOVEL1.EXE in Ghidra: dispatcher `tm_dispatch_opcode` @1000:0c77; opcode table
   @12e7:013e, handler table @12e7:00b0; handlers `tm_op_var_add_input` @1000:0ec2
   and `tm_op_var_sub_input` @1000:0ee5, both `xor si,si` then the shared
   add/sub core @0ed5/0ef7 over var base 0x34bb). Now mapped + implemented
   (`OPCODE_VAR_ADD1`/`OPCODE_VAR_SUB1`, game_data.h + game_opcodes.cpp). With them,
   **`PUT 42 COINS IN BOWL`** deposits exactly 42 (Talisman bowl funcs 170/173).
2. **Part-1 coins were never spent.** The bazaar haggle (Hosni) uses 0x8d/0x91 to
   record the bid and subtract it from the coin pile (var34). With the opcodes
   no-op, the committed part-1 script could overpay for free (rope 60 + flask 50).
   Once the opcodes work, the **ship fare check is real**: `SAIL SHIP` (func 128)
   refuses if `var34 < var31` and **var31 = 42**. And the bowl itself needs 42
   coins. So Abu must reach the boat holding ≥ 42 of his 137. The haggle now bids
   modestly (rope closes at 45, flask at 43 → ~49 left). **Haggle model:** ask
   erodes ~3/offer, Hosni accepts once `offer ≥ ask − 5`, charging your bid;
   min total spend ≈ `130 − 3·(total offers)`. The desert proved **robust** to the
   changed part-1 turn count (the `WAIT`×20 buffer at part-2 start absorbs the
   rand-stream shift) — room 72 still reached, contradicting the old "never edit
   upstream" fear for this magnitude of change.

**Escape recipe (in the script):** `GET AMULET` (lifts the talisman out of the
bowl; bowl arm rises 42 lighter → `var122` 128→86; wall seals) then
`PUT 42 COINS IN BOWL` (`var122` 86→128, balanced → wall reopens). Then `N` exits
room 72 into the moving-wall network (rooms 64–81).

The step-0–4 desert routes were found by BFS over `(room, var72)`; they are
prefix-specific, but tolerant of small part-1 perturbations (see above).

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

### 6. Secret room → amulet (the counterweight puzzle) — **DONE (in the script)**
A statue with a **bowl + dome counterweight** (`[s2:543]`). The talisman sits in
the bowl; the arms balance at level 128 (`var120` = dome reference, `var122` = bowl
arm = `128 + coins_in_bowl − amulet_weight_removed`). The N exit (`func 21`,
room-72 branch) is blocked while `var120 != var122`.

**Solution (pinned):** `GET AMULET`, `PUT 42 COINS IN BOWL`.
- `GET AMULET` lifts the talisman out (`func 176`: `var36 += var31`(=42), then
  `func 175`: `var122 -= var36` → 86); the wall slams shut (`[s2:674]`, room flag
  205) and **without 42 the player is trapped**.
- `PUT 42 COINS IN BOWL` (`func 173`) deposits exactly the talisman's weight back:
  `var34 -= 42; var36 += 42; var123 += 42`, then `func 171`: `var122 += var36` →
  128. Balanced → `[s2:…]` "the wall behind ye hath slid back, opening the exit!".
  `var36` is a per-turn scratch (reset between prompts), so the +42 lands cleanly.
- The **count form needed opcodes 0x8d/0x91** (see Current status) — previously
  unmapped, so `PUT 42 COINS` was a silent no-op. Fixed.

(`TAKE COINS FROM BOWL` = `func 173`'s all-path; `WEAR AMULET` activates the throb,
flag 111 / `func 16` — flavour, not needed.)

### 7. Maze exit → carpet flight (rooms 83–86) — **SOLVED (see "Steps 7–9" above)**
These were the open questions at the time; the answers are in the solution
section at the top. Kept for the RE trail.

After the bowl, `N` drops back into the moving-wall network (rooms 64–81). The
source walkthrough's exit moves (`N E E E N D W W S`, `TELL ABU TO PULL LEVER`,
`N E S E N`, `U S W W S S S`) **do not transfer** (different RNG; tried live —
stalls at room 67, "exit only through the pit"). So the maze exit needs the same
`(room, var33 % 3, flags)` heuristic BFS as step 5 (entry), driven forward from
the post-`PUT 42 COINS` endpoint.

Findings so far (this session):
- **The rug is not in inventory and the Persian carpet (item 21) is in limbo
  (`room=255`)** — the roc stole it in step 2. So the flying rug must be obtained
  somewhere on the way out (roc's nest?) or is a different item; `func 119`
  (`carpet` handler), `func 256` (`carpet`+`bird`/`huff` — the roc), and
  `func 270` (`EXAMINE carpet`) are the leads.
- The flight rooms **27–30** (mirror **83–86**) carry the "soaring upon thy magic
  carpet" descriptions but have **no static exits** — they're a scripted state
  machine (`func 10` / `func 15`, the `FLY`/ride handler). `DROP RUG`, `SIT`,
  `FLY RUG`, `WAIT`, `GET UP` is the cadence to confirm once the rug is in hand.
- Endgame room map (from the dump): `31` = green-glow cave with the magician;
  `32` = magician + genie over the lantern; `33`/`35` cave→`34` palace gates→
  `36` = King Darius's audience chamber (the winning `BOW`).

### 8. Genie / wizard / palace ending → BOW — **SOLVED (see "Steps 7–9" above)**
The first guess (from the source walkthrough) was
`E, E, SPEAK, POUR WATER ON LAMP, GET LAMP, RUB LAMP, TELL GENIE TO KILL WIZARD,
YES, W, W, SIT, FLY, WAIT, BOW`. The genie/wizard cave is rooms 31/32, the
palace ending 33–36. The working cadence — and the victory string now used as the
win marker — are in the solution section.

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
- **Route search (BFS):** two getenv-gated probes live in `game_tm.cpp`
  (read-only, committed deliberately as tooling — they are inert unless the env
  var is set):
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
- **DOS build — DONE.** The same walkthrough now also wins on the DOS data set
  (G0 = part 1, magic 0xa429; **H0 = part 2, magic 0x94c6**; strings in
  `novel.exe`/`NOVEL1.EXE`). `enterPart2()` points the loader at `H0` when not on
  the Apple disk-image VFS, `loadStrings()` reads novel.exe's **second** string
  segment (0x22fa0) into the same gapped layout as the Apple Lands Beyond banks,
  and the special-opcode-16 handler no longer gates on `DiskImageFS::active()`.
  `NOVEL1.EXE` (head md5 `15cd75f9…`) is byte-identical to the recognised
  `NOVEL.EXE` (`2e18c88c…`) from 0x400 on, so it is now accepted (with a
  `novel1.exe` filename fallback). Verified: deterministic win on
  `comprehend games/talisman-challenging-the-sands-of-time` and on `TalismanPC`,
  part-2 text byte-for-byte matching the Apple transcript.
- **Cosmetic `@`-leak — no longer reproduces.** The old report (after `INVENTORY`
  the next room redraw showed "span the 137ern horizon", the coin count leaking
  into the beach's `@` direction slot) does not occur in the current build. The
  inventory verb still leaves the shared `@` replacement in number mode (var 34 =
  coins, so the gold item prints "49 glittering gold coins"), but every desert
  room description re-establishes word mode before substituting `@`, so a redraw
  right after `INVENTORY` correctly renders "span the western horizon" on both the
  Apple and DOS data. Verified directly (INVENTORY → beach `N`) and across a wide
  desert wander; no numeric leak in any `@` room.
- **Pictures** (lower priority): part-2 room/item art is RE–RG / OE–OF on the
  Lands Beyond disk and resolves through the active-disk VFS, so it should "just
  work"; spot-check against MAME, treat mismatches as a separate graphics task.
