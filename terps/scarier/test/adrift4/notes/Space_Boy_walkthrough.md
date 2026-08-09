# Space Boy's First Adventure — walkthrough (**WON, 934/1374**, deterministic)

> **DONE 2026-06-27.** Full win banked & verified 3× identical
> (`goldens/space_boy_solution.txt`, 142 cmds). All four power items
> (Flight Boots, Ice Gloves, Heat Goggles, **Strength Belt**) obtained, the cape
> recovered, and the win ending reached (marker *"STAY TUNED FOR MORE EXCITING
> EPISODES OF ACTION WITH SPACE BOY AND WONDER DOG!"*). **934/1374 (68%)** — the
> win does not need max score; the unbanked points are repeatable/optional side
> tasks (and the dead Ice-gloves +30 typo, so true max ≤ 1344).
> Run: `sh harness/play.sh games/"Space Boy's First Adventure.taf" goldens/space_boy_solution.txt`


*Space Boy's First Adventure* v2.0 (David Parish, 2005; ADRIFT Generator 4.0).
Large game: **78 tasks, ~74 rooms, max score 1374**, one true win ending.
Derived via the deterministic headless SCARE harness (`harness/scare`, seeded).
Structural dump: `space_boy_dump.txt`. Solution-so-far: `goldens/space_boy_solution.txt`.

Run:
```sh
sh harness/play.sh games/"Space Boy's First Adventure.taf" goldens/space_boy_solution.txt
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

### Return to the hub (from Treasure Island) ✅ SOLVED
After `wear goggles` on Treasure Island (room 25 area), the magma bridge is gone
but there is an island to the NE: `fly ne`→25 *Islands of LAVaaH*, then `n`→11
*Landing Platform* (the hub). (Compass is rotated — the in-game `n` is the dump's
NE exit; navigate by the prose, not the dumped labels.)

### East — Mountain Top Garage / "TO THE GARAGE" letter maze (→ Strength Belt) ✅ SOLVED
From the hub `fly east`→26 *Entrance to the Mountain Top Garage*. `read sign`,
then `enter cave` — a scripted **cave-in** drops you into **72 The Cave-In**.
Dig down to the maze: `take small shovel`, `read small sign` (it spells
**T-O-T-H-E-G-A-R-A-G-E** — the maze key), `dig a hole in sand` (→27 Top of
Hole), `dig more` (→28 Bottom of Hole), `w`→**29 Under the Mountain** (dark).

**Light the way:** `take stick`, `read note` (*"...look for the letters..."*),
`light stick with goggles` (+18; Heat Goggles ignite the stick — without the lit
stick the maze rooms are dark). Each maze room shows one carved letter; **follow
the path that spells "TOTHEGARAGE"** (the maze is *not* compass-rotated — the dump
directions work):

```
29 T  -w→  30 O  -s→  31 T  -w→  32 H  -w→  33 E  -n→  34 G  ... [ICE GATE]
... 35 A  -w→  36 R  -n→  37 A  ... [FIRE GATE]  -e→  38 G  -e→  39 E  -u→  40 Garage Bay
```

Two **elemental gates** block the spelling path — this is where the powers pay off:
- **34 "G Room"** — a block of ice blocks N & W. `melt ice with goggles` (+30)
  opens it, but a gust **blows out your stick** and whisks you to the *Dark Room*.
  `light stick with goggles` (+15) relights and returns you to the G Room (ice now
  gone); `n`→35 *A Room*.
- **37 "A Room (second)"** — a fireball blocks the path. `freeze fire with ice
  gloves` (+30) extinguishes it; `e`→38 → `e`→39 → `u`→**40 Garage Bay**.

**The Garage cluster + Phased Ion Bridge.** From 40: `e`→59 *Break Room*, `n`→68
*Garage Office*. `take phased ion bridge` (+15 — also opens the office window),
`out`→72. Back to home: `w`→11 hub, `w`→8, `w`→0 Living Room, `s`→2 *Hangar Bay*,
`in`→**10 The Transporter Unit**.

**Power the transporter.** Pushing the blue button fails until the bridge is in
the **Transporter Power Plant** (an openable container in the Hangar Bay, *not*
the transporter): `out` (→2), `put bridge in power plant`, `in` (→10),
`push blue button` (+30) → **69 Moon Base Transporter Room**.

**Get the belt.** `out`→67 (Moon Base; a hole + Mess Hall E + Offices W).
`e`→60 *Mess Hall*, `take fork`, `w`→67, `use fork on hole` (+50; the fork also
powers the Beam Generator) → **66**: `read paper` (photo of a rock in a garden —
the hint), `take belt` (+30), `wear belt`. **Strength Belt obtained — 514/1374.**

### Endgame (Room Key → Evil Man's lair → cape → win) ✅ SOLVED
Return home through the Moon Base transporter: `u`→67, `in`→69,
`push red button` (+30; needs the Beam Generator powered by the fork) → 10,
`out`→2 Hangar Bay, `sw`→3 Back Porch, `w`→**4 Backyard Garden**.

`move huge rock` (task 43, +30; needs the belt **worn**) drops you into **71 Under
the rock**; `take key` (+40, **Room Key**) bounces you back to 4. Now to the lair:
`e`→3, `ne`→2, `n`→**0 Living Room**. `unlock door` (task 44, +15, needs the Room
Key) then `open room door` (task 51) steps W into **7 Space Boy's Room** — Evil
Man is here and swings at you, **but the Battle System never lands lethal damage
and no fight is required** (*"Evil Man hits you, but it doesn't seem to do any
damage"* — `str−def ≤ 0`). Score the cape finale, then read the note:
- `take cape` (task 45, **+105**) — *"the Power of Space returns to you"*; this
  also **drops your four power items** (their power drained), which is harmless at
  the endgame.
- `drop cape to the floor` (task 72, **+250**) — a scripted, no-restriction
  scoring task. (The library also echoes a cosmetic *"Drop what?"* because the
  cape object was consumed by `take cape`; the **task still fires and scores** —
  it is matched by command, not by an in-hand object.)
- `w`→**65 Space Boy's Secret Hide-Out**, `read scribbled note` (task 46, **+200**,
  EndGame win) → **WIN**.

## Verified result
- **WON, 1184/1374 (86%)**, all four power items obtained + cape recovered,
  verified 3× identical (`goldens/space_boy_solution.txt`, 145 cmds). Win marker
  *"STAY TUNED FOR MORE EXCITING EPISODES OF ACTION WITH SPACE BOY AND WONDER
  DOG!"*
- The win does **not** require max score — many scoring tasks are repeatable
  side-content, and the Ice-gloves +30 (task 11) is a dead author typo
  (`{take\get}` backslash, see below) so the true ceiling is ≤ 1344. 1184 is the
  clean single-pass total of the full spine + both cape tasks.

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

## Optional follow-up (the win is complete; these are extra points only)
- The banked 1184 is a clean spine; it leaves repeatable/side-content unpicked
  (e.g. maze dead-end flavour, the `down`→wooden-stick task, multiple book/sign
  reads). A true-max pass toward ≤ 1344 is possible but unnecessary — Space Boy's
  combat is **harmless** (Evil Man does 0 damage), so there is no RNG-stream
  caveat to tune; the route is fully deterministic.
- Author debug cheats exist (`gimme gimme gimme` = all 4 items, `shout spade` =
  the Phased Ion Bridge, `shout hobbit` = teleport to room 29) — **not used**;
  the banked route earns every item legitimately.
