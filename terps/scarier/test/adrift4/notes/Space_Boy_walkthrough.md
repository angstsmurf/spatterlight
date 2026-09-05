# Space Boy's First Adventure — walkthrough (**WON, 1009/1374**, deterministic)

> **DONE 2026-06-27; extended to the ceiling 2026-09-05.** Full win banked &
> verified (`goldens/space_boy_solution.txt`). All four power items (Flight
> Boots, Ice Gloves, Heat Goggles, **Strength Belt**) obtained, the cape
> recovered, and the win ending reached (marker *"STAY TUNED FOR MORE EXCITING
> EPISODES OF ACTION WITH SPACE BOY AND WONDER DOG!"*).
> **1009/1374 (73%) is the ceiling for a run that never repeats a scoring
> task** — see *Where the missing 365 points went* below.
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
then `north`→hub. **Heat Goggles obtained.** *(Running total here: 280/1374.)*

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
the transporter): `out` (→2), `install bridge` (task 15, **+30**; the library `put bridge in power
plant` also works but scores nothing), `in` (→10), `push blue button` (+30) →
**69 Moon Base Transporter Room**.

**Get the belt.** `out`→67 (Moon Base; a hole + Mess Hall E + Offices W).
`e`→60 *Mess Hall*, `take fork`, `w`→67, `use fork on hole` (+50; the fork also
powers the Beam Generator) → **66**: `read paper` (photo of a rock in a garden —
the hint), `take belt` (+30), `wear belt`. **Strength Belt obtained — 549/1374.**

### Endgame (Room Key → Evil Man's lair → cape → win) ✅ SOLVED
First collect the office button: `u`→67, `w`→61 *Outer Office*, `w`→**64 Main
Office**, `press button` (task 41, **+20**), `e`, `e`→67. Then return home with
`enter transporter` (task 52, **+50**) → 10. (`in`→69, `push red button` (+30)
is the other way home and the one the author's own walkthrough uses; it is 20
points worse and cannot be combined with 52 — see *Where the missing 365 points
went*.) `out`→2 Hangar Bay, `sw`→3 Back Porch, `w`→**4 Backyard Garden**.

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
- `drop cape to the floor` (task 72, **+250**) — **does not fire.** The command
  is kept in the solution only as in-transcript evidence: run400 lets the
  library's ordinary `drop` claim the line outright and the task never runs
  (Adrift_8_pET2.txt / Adrift_10_pET4.txt, measured 2026-08-23; the engine side
  is `run_task_reachable_by_library_callback()` in `scrunner.cpp`). The
  transcript shows the library's *"You drop the red Cape."* and no score
  change.
- `w`→**65 Space Boy's Secret Hide-Out**, `read scribbled note` (task 46, **+200**,
  EndGame win) → **WIN**.

## Verified result
- **WON, 1009/1374 (73%)**, all four power items obtained + cape recovered.
  Win marker *"STAY TUNED FOR MORE EXCITING EPISODES OF ACTION WITH SPACE BOY
  AND WONDER DOG!"*

## Where the missing 365 points went
The game advertises **1374** but its own ChangeScore actions only add up to
**1319** — the author's declared maximum is 55 points larger than anything the
game can pay out. Of that 1319:

| lost | why |
| --- | --- |
| **30** | task 11, the ice gloves: the pattern is `{take\get} ... gloves`, a backslash where the author meant a slash, so nothing can ever match it (see *Resolved findings*). |
| **250** | task 72, `drop cape to the floor`: run400 lets the library's own `drop` claim the command and the task never runs. |
| **30** | one of the two +30 transporter buttons. Blue (task 53, room 10) is the only way to the Moon Base and red (task 54, room 69) the only way home by button, but the Moon Base Garage also holds task 52, `enter transporter` (**+50**), which gets you home for 20 points more. Taking 52 costs the red button; taking both would mean a second blue press, and 53/54 are `Repeatable`, so that is score farming, not a longer route. |

`1319 − 30 − 250 − 30 = 1009`, which the golden reaches exactly. The four
points the 2026-06-27 route left on the table were `read computer` in Wonder
Dog's Room (+5), `install bridge` instead of the library `put` (+30), the Main
Office button behind the Moon Base offices (+20) and `enter transporter` (+50,
net +20 against the red button).

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
   other 8 tasks). **Net: the +30 is lost to a shipped author bug.**

## Optional follow-up
- Nothing is left that a non-repeating run can reach. Higher totals exist only
  by re-pressing the repeatable blue/red buttons (+30 each, unlimited) or
  re-reading the computer (+5, unlimited); the golden deliberately does neither.
  Space Boy's combat is **harmless** (Evil Man does 0 damage), so there is no
  RNG-stream caveat and the route is fully deterministic.
- Author debug cheats exist (`gimme gimme gimme` = all 4 items, `shout spade` =
  the Phased Ion Bridge, `shout hobbit` = teleport to room 29) — **not used**;
  the banked route earns every item legitimately.
