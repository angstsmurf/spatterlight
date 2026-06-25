# Space Boy's First Adventure — walkthrough (PARKED 2026-06-25 @ 275/1374)

> **PARKED.** Castle + Volcano fully solved & banked (3/4 power items). Resume at
> the **East region** (room 26, reached via hub `fly east`): Strength Belt +
> Transporter maze + Phased Ion Bridge, then the Room-Key/Evil-Man endgame.
> Resume command: `sh harness/play.sh games/"Space Boy's First Adventure.taf" harness/space_boy_solution.txt`


*Space Boy's First Adventure* v2.0 (David Parish, 2005; ADRIFT Generator 4.0).
Large game: **78 tasks, ~74 rooms, max score 1374**, one true win ending.
Derived via the deterministic headless SCARE harness (`harness/scare`, seeded).
Structural dump: `space_boy_dump.txt`. Solution-so-far: `harness/space_boy_solution.txt`.

Run:
```sh
sh harness/play.sh games/"Space Boy's First Adventure.taf" harness/space_boy_solution.txt
```

## Premise / win condition
Evil Man attacked Space Boy's home, stole his **cape** (the source of his
powers) and kidnapped **Wonder Dog**. Powerless, Space Boy must find **four
items that mimic his powers** — Flight Boots, Ice Gloves, Heat Goggles,
Strength Belt — then break into Evil Man's lair, recover the cape and win.

- **WIN** = TASK 46 `read scribbled note` in **room 65** (+200, EndGame win,var1=0).
  Room 65 is **W of room 7** (Evil Man's lair), which is **W of room 0** (start),
  gated by unlocking Space Boy's door (need the Room Key).

## Map skeleton (room indices from the dump; compass labels are the game's)
- **0 Living Room (START)**: N→1 (computer), S→2, E→8, W→7 *(locked: endgame)*.
- **2 Hangar Bay**: N→0, IN→10 (Transporter Unit), SW→3.
- **3 Back Porch**: NE→2, W→4, S→5 (oak tree).
- **4**: E→3. *Huge rock* (move with Strength Belt → room 71 → Room Key); flowers.
- **5 Oak Tree base** → **6 Up the Oak Tree** (`climb up oak tree`): **Flight Boots**.
- **8** (E of 0): the flight-hub approach → E→**11 Landing Platform** (the hub).
  *Also a dev-cheat room:* `gimme gimme gimme` grants all 4 items, `shout spade`
  grants the Phased Ion Bridge, `shout hobbit` teleports to 29 — **do not use**.
- **11 Landing Platform (HUB)**: with boots, `fly north`→9 (Castle), `fly south`
  →18 (Volcano Islands), `fly east`→26 (Tall Mountains). W→8.

### North — Castle of Halls (→ Ice Gloves) ✅ SOLVED
9 Entrance→ E→12 HUGE Hall → NE→13 (old **painting**, +5, clue: panel+safe);
E→14 Medium Hall (**book page**: *"the Fire God's name is ell; aay; vee; aay aay;
ach"* — the Volcano letter-door order); S→15 Small Hall (**orange "drink me"
bottle**: drink to *shrink*, +15, opens S→16); 16 Tiny Hall (control panel + safe
+ bin of 4 tiles + bluish "tiny bottle" to grow back). Put the **whiteredblue
tile** on the panel (+30, matches painting) → safe opens → IN→17 The Safe →
**Ice Gloves**. *(Ice-gloves task-11 +30 is shadowed by the library `take`; see
Open questions.)*

### South — Volcano Islands (→ Heat Goggles + Fire God statue) ✅ SOLVED
From hub `fly south`→18 *The Islands of LAVaaH* (magma; `read sign` = "shout his
name"). **You cannot fly off the magma** — leave room 18 via plain `north`→hub.
Cross the bridge `w`→70 *Statue Base Island*: `read plaque` ("L-A-V-aa-H") opens
a hole `d`→19 *Under the Base* (pentagon). Shout the five name-syllables to open
all five doors (each scores): `shout ell` (+10, **nw**=feet), `shout aay`
(+15, **ne**=legs), `shout vee` (+15, **se**=chest), `shout aay aay` (+20,
**sw**=arms), `shout ach` (+20, **s**=head). Side rooms return via the *opposite*
direction (nw↔se, ne↔sw, s↔n). Collect all 5 parts, `u`→70, `assemble statue`
(+20), `put statue on base` (+35 — island rises S but a magma wall blocks),
`freeze wall` (Ice Gloves, +15 — forms a bridge), `south`→25 *Treasure Island*,
`take goggles` (+30; Fire God wakes, bridge melts). Exit: `fly northeast`→18,
then `north`→hub. **Heat Goggles obtained.** *(Running total here: 275/1374.)*

### East — Tall Mountains / underground complex (→ Strength Belt + bridge) ⏳ TODO
26 (`enter cave`→72); 72 (`take small shovel`; `dig`→27); 27 (`dig more`→28);
**28 is a death room** — `fly up`/`fly out of hole`/`climb out` are all LOSE
endings; the only safe exit is **W→29**. 29→ big maze (30–58, the Transporter
Power Plant) leading to: room 37 `freeze fire` (Ice Gloves, +30); room 39 `melt
ice` (Heat Goggles, +30)→73; room 64 `push button` (+20); **room 66 Strength
Belt** (task 40, reached via `use fork on hole`/`unlock hole with fork` in room
67); room 68 **Phased Ion Bridge** (task 55) + `open window`; transporter loop
rooms 10/67/69 (blue/red buttons). Heat Goggles also: `light stick with goggles`
(rooms 29/36/59).

### Endgame ⏳ TODO
Strength Belt → room 4 `move huge rock` (task 43, +30) → room 71 `take key`
(Room Key, +40) → room 0 `unlock door` (task 44, +15) → `open space boy's door`
(task 51) → W→**7** Evil Man. Fight: `freeze evil man` / `melt evil man` /
`hit evil man` (tasks 73/74/75 chip stamina) → `kill evil man` (task 76) →
`get cape` (task 45, +105) or `drop cape to the floor` (task 72, +250) →
W→**65** → `read scribbled note` (task 46, +200) → **WIN**.

## Verified progress
- **275 / 1374**, three of four power items worn (Flight Boots, Ice Gloves,
  Heat Goggles). (`harness/space_boy_solution.txt`)
- Opening + hub + full Castle (Ice Gloves) + full Volcano (statue + Heat Goggles)
  confirmed deterministic. East region entry (room 26) reached via `fly east`.

## Resolved findings
1. **Ice-gloves +30 (task 11) is an author typo — unreachable for everyone, and
   SCARE is FAITHFUL (not a bug).** Task 11's command is `{take\get} ... gloves`
   with a **backslash**, whereas all 8 other take/get tasks (boots/goggles/belt/
   cape/key/shovel/bridge/transporter) use the correct `{take/get}` slash. ADRIFT's
   command-pattern syntax only splits alternatives on `/`; backslash is a literal
   character, so `{take\get}` is a single dead alternative ("take\get") that can
   never match the player typing "take"/"get". Confirmed in **both** SCARE's parser
   (scparser.c: `/` is the sole `TOK_ALTERNATES_SEPARATOR`) **and** the decompiled
   real Runner (`decompiled/NewParse.bas` counts/extracts `/`-separated alternatives
   inside `{}`/`[]`; chr(92)/backslash is never special in any `.bas`). So `take
   gloves` falls through to the library take with no score in the original Runner
   too. SCARE's `.taf` decoder is not at fault (it renders `/` correctly for the
   other 8 tasks). **Net: the +30 is lost to a shipped author bug; true max ≤ 1344.**

## TODO
2. Derive South (statue + Heat Goggles) and East (Strength Belt + bridge) routes.
3. Bank the endgame (Evil Man combat — Battle System? check stamina model).
4. Establish the true max score reachable on a single fixed turn-list (the
   `erkyrath_random`/shared-stream caveat seen in Light Up may apply if combat
   is RNG-driven). Account for the dead task-11 +30 (max ≤ 1344).
