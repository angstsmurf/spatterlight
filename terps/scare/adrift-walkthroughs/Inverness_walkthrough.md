# Inverness Castle — walkthrough & analysis

- **Game:** *Inverness Castle* (file `inverness.taf`), version **0.3c**, February 5, 2001
- **Author:** David Good (david@dreamnetstudios.com)
- **Engine:** ADRIFT 3.90.12 (Campbell Wild)
- **Result:** **UNWINNABLE — no ending exists.** Max reachable **75 / 205 (36%)**,
  deterministic. Solution: `harness/inverness_solution.txt`.

A faithful, abridged dramatisation of *Macbeth*, Act 1–2: you are the Thane of
Ross, arriving at Inverness Castle the night Duncan is murdered. The game is an
**unfinished beta** — it has a scoring system but **no win/lose state at all**,
and its climax dead-ends with the player knocked unconscious and tied up in the
cellar with no implemented escape.

## Why it can't be won / why 130 points are unreachable

Two structural facts, both proven from the `.taf` data (task/event dump) and
confirmed in play:

1. **No ending.** Across all 57 tasks there is **not a single EndGame action
   (ADRIFT action type 6)** — no win, no lose, no death-stop. The game can never
   declare itself over. (Same situation as `lifesimulation`, `Town of Azra`,
   `Nonsense Machine`: a scored sandbox/fragment, not a completable game.)

2. **130 of the 205 points are mutually exclusive.** The headline scorer is the
   witches' **riddle box**: tasks T29–T42 are **fourteen** different riddle
   answers worth **+10 each (140 pts)**, but they are gated on one variable
   (`var8` = "current riddle id", values 10–23). The box poses **exactly one**
   randomly-chosen riddle, and the **first correct answer opens the box**, after
   which the same riddle simply re-poses and never re-scores. So **only one of
   the fourteen riddle tasks is reachable in any single playthrough** — worth
   **+10**, not +140. (Under the deterministic harness seed the riddle is always
   *"First put your foot out…"* → answer **step**; in the real Runner you'd get a
   random single riddle, but still only one solvable per game.)

That caps the real maximum at **205 − 130 (the 13 unanswerable riddles) = 75**,
and every one of those 75 points is collected by the solution below.

## The 75 reachable points

| Pts | Task | How |
|----:|------|-----|
| +5  | knock (T25) | Knock 5× at the locked front door; the Porter's "hell-gate" speech plays, then he opens up |
| +10 | get torch (T3) | Distract the door Porter with bread, then take the lit wall torch |
| +5  | push statue (T15) | In the Sitting Room, push the statue aside to reveal a hole |
| +10 | look behind painting (T7) | Put the lit torch in the hole (a click sounds), then look behind the painting → a wooden box |
| +5  | ask witch about box (T44) | Carry the box to the heath; the witches identify it as a riddle box |
| +10 | answer riddle (T34, *step*) | Solve the posed riddle; the box opens, revealing a scroll and an old key |
| +5  | unlock desk (T48) | Old key unlocks Macbeth's desk in the Master Bedroom |
| +5  | open desk (T47) | Open the drawer → Macbeth's letter to his wife |
| +20 | search bedroom (T8 + T9) | In the Dressing Room, overhear the murder plot ("search bedroom"); the discovery completes one turn later — and Macbeth catches you |

After that final +20 you are knocked out, dragged to the **cellar and tied to a
pillar**. This is the unfinished edge of the game: the only "untie" task
(`drop belt`, T54) is an **empty stub with no actions**, and the cellar's exit is
gated on a flag nothing ever clears, so you are stuck at 75/205 forever. There is
no further content and no ending.

## Full command list (deterministic, harness seed 1234)

```
knock
knock
knock
knock
knock              (Porter opens the door, +5; enter Entrance Hall)
n
n                  (-> Kitchen)
open pantry
take bread
se
s                  (-> back to Entrance Hall, Porter present)
give bread to porter
get torch          (+10)
e                  (-> Sitting Room)
push statue        (+5, reveals hole)
put torch in hole
look behind painting   (+10, reveals wooden box)
take box
open box           ("can't open it — perhaps someone can help")
w
s
ne
s                  (-> Blasted Heath, the three witches)
ask witches about box  (+5)
answer step        (+10, the box opens)
take key
take scroll
n
w                  (Behind Inverness — the front door has re-locked)
s                  (-> Kitchen via the back door)
se
up                 (-> upstairs)
w                  (-> Master Bedroom)
unlock desk        (+5)
open desk          (+5)
take letter
s                  (-> Dressing Room; the murder-plot scene plays)
search bedroom     (+10; overhear the plot)
look               (+10; discovery completes — Macbeth catches you)
```

Final score **75/205**. (After "look" the caught-and-cellar animation runs over
the next couple of turns; the game then sits idle in the cellar.)

## Notes on the route

- **Bread → Porter → torch.** The lit torch is on the Entrance-Hall wall, but the
  Porter blocks you taking it. Get the bread from the Kitchen pantry and
  `give bread to porter`; he wanders off to find a drink, leaving the torch free
  (T3 literally checks "bread held by Porter").
- **Torch / statue / painting.** `push statue` exposes a hole; `put torch in hole`
  satisfies the hidden-light requirement so `look behind painting` works and drops
  the box into the Sitting Room.
- **You must leave the castle to open the box.** The old key (for the desk) is
  *inside* the box, and the box only opens after a witch riddle on the heath. The
  front door **re-locks** once you step outside, so re-enter through the **back
  door**: Road → W (Behind Inverness) → S (Kitchen).
- **The catch is mandatory for the last +20.** `search bedroom` in the Dressing
  Room scores T8; the very next turn fires T9 (its companion +10) *and* trips the
  "get caught" event that ends your freedom. There is no way to get those points
  without being caught, and being caught is a dead-end — so 75 is simultaneously
  the maximum and the terminus.

## Provenance

Derived with the headless deterministic SCARE harness (`harness/`, seed 1234).
The reachability was proven from a temporary task/event/where dump added to
`sctasks.c` (gated on `SC_DUMP_TASKS`, reverted afterwards — tree left clean):
57 tasks, 26 rooms, 43 objects, 16 NPCs, 9 events. Verified 3× identical at
75/205. This is a faithful reading of the published 3.90 `.taf`, not a SCARE
limitation — the missing ending and the single-riddle box behaviour exist in the
original ADRIFT Runner too.
