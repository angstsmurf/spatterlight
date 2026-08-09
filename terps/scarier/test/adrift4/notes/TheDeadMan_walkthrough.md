# The Dead Man — walkthrough (**WIN, 41/43 — the winning ceiling**)

- **Author:** 30otsix, 2003. One room, a countdown, and a first-person amnesia
  plot: you are Sgt. Tom Perkins, sole surviving caretaker of a cold-war
  retaliation bunker, and the abort transmission has not come.
- **Engine:** **ADRIFT 4.00** (`xxd -p -l 12 "games/The Dead Man.taf" | cut -c17-22`
  → `93453e`).
- **Result:** **WON, 41 of 43**, verified in seeded Scarier
  (`goldens/deadman_solution.txt`, PASSing golden, win marker `ABORT SUCSESFUL`).
- **Source:** `downloaded/TheDeadMan_walkthrough.html` — the delron ("The Home of
  Otter Interactive Fiction") command list, 48 lines. It is a real route, but it
  is *not* a replay: three corrections below, and it leaves 9 points on the
  floor.

## Why the file cannot be piped in as-is

Every one of the game's blackout visions ends in an ADRIFT `<waitkey>` pause,
and the intro carries ten of them before the first prompt. Under the plain ANSI
port each pause eats one line of stdin, so the published list desyncs on line
one: its `l`, `roll over body`, … are swallowed by the intro and the game
answers commands the walkthrough never issued.

The row is therefore wired with **`SCR_SKIP_WAITKEY=1`** (the same treatment
`afdfr_solution.txt` gets), which makes pauses transparent and restores
one-line-one-command. Derived by hand the alternative needs ~18 blank filler
lines at counts (10, 2, 2, …) that have to be bisected per vision — and every
vision that gains a paragraph in a later build would move them.

## Correction 1 — the blackouts make you drop everything

`!black-out` (EVENT 2/3/4/5, firing on turns 15, 25, 35 and 40) lays the player
on the floor and empties their hands. The published route opens the first aid
kit at turn 12 and then dawdles — so anything taken out of the kit is on the
floor again a turn or two later, which is why its own `get axe` / `chop off
hand` sequence fires against an empty inventory the first time you try it.

The wired route front-loads instead: the kit is looted and *used* (panel,
bandage) before turn 15, and the syringe is taken only after that blackout has
passed.

## Correction 2 — 26 waits are 23

The published `z`×26 is timed against a session that lost turns to the pauses.
With `SCR_SKIP_WAITKEY=1`, `!knock-knock` (EVENT 6, `start=30`) brings Lefty to
the door after exactly **23** waits, and `open door` is refused until then:
"You probably shouldn't open it until there is someone knocking."

## Correction 3 — the three scoring actions the walkthrough skips

The published route reaches 32. `SCR_DUMP_TASKS` shows twelve `ACT type=4`
(score) awards; three are not in it:

| task | command | points |
| --- | --- | --- |
| TASK 22 | `open panel with scissors` — the tampered radio panel; the scissors are in the first aid kit | +5 |
| TASK 25 | `wear bandage on neck` — the "strange warm wetness at the base of your neck" from the opening paragraph | +3 |
| TASK 2 | `turn camera to camera 1` — the security dial beside the joystick; any camera scores once | +1 |

Taking all three gives **41**.

## Why 43 is unreachable in a winning game

The remaining 2 points are **TASK 19**, `shoot myself`:

```
RESTR type=0 v1=3 v2=1 v3=0 obj5=[gun]      (gun held)
ACT type=6 v1=3 v2=0 v3=0                    (game over)
ACT type=4 v1=2                              (+2)
```

It pays 2 and then kills you, so the two point totals are mutually exclusive:
43 is the sum of every award in the file, 41 is the most any surviving player
can hold. The route stands at the ceiling.

## The route

`goldens/deadman_solution.txt` — 47 lines:

```
roll over body            +1   (also reveals the bullets)
x body
get all                   +1   (touching the gun; the vision is scripted, not fatal)
get gun
load gun                       ("load" is a synonym for "put bullets in")
shoot locker              +5
open locker               +3
get kit
open kit
get scissors
get bandage
open panel with scissors  +5
wear bandage on neck      +3
turn camera to camera 1   +1
get syringe                    (after the turn-15 blackout, which drops everything)
use syringe               +5
use radio                 +2
z ×23                          (Blair answers; the visions play; Lefty starts pounding)
stand
open door                 +2   (the axe embedded in the door falls inside)
get axe
chop off hand with axe    +3   (Capt. Mattingly is handcuffed to the door handle)
get hand
put hand on green plate
put hand on green plate   +10  (two thumbprints: his, then yours — WIN)
```

The abort plate wants two prints, which is the whole point of the title: the
dead man's switch needs a dead man.
