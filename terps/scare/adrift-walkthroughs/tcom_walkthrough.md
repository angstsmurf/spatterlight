# The Cave of Morpheus — walkthrough (WON, 0/0 — no score)

**Game:** `tcom.taf` ("The Cave of Morpheus", Part 1). ADRIFT 3.9, a short
surreal anxiety-dream: a nude undergraduate wakes late in his dorm at Ionesco
Hall and must race across campus to reach his 9 AM Western Civ exam before the
bell — while Death himself gives chase through the streets. Deterministic.

**Result:** WIN. Reaching the classroom door and opening it ends Part 1
(marker: *"This ends the first part of 'The Cave of Morpheus.' To continue,
please open the file entitled 'tcom2'"*). **0/0 — there are zero ChangeScore
(type-4) actions in the game, so the win is the maximum result.** Solution:
`harness/tcom_solution.txt`.

## Solution (13 commands)

```
n
n
d
n
n
n
n
n
d
n
n
n
open wooden door
```

## Route, room by room

The whole game is a single straight dash from the dorm to the exam-hall door;
the only real decision is "keep heading toward the door and don't dawdle."

1. **In My Dorm Room** — `n` → *Outside of My Dorm Room*. (Standing up off the
   old pizza box happens automatically.)
2. `n` → **At the Top of the Stairwell**.
3. `d` → **At the Bottom of the Stairwell** (the door's been removed; through
   the puke).
4. `n` → **At the Back of the Courtyard** (four grass patches — see hazards).
5. `n` → **Beside the Fountain** (something moves under the water; flavor only).
6. `n` → **At the Front of the Courtyard**.
7. `n` → **In the Foyer, By the Hall Porter's Desk** (Lester Squeems is out;
   his room's door is shut).
8. `n` → **On the Front Steps of Ionesco Hall** (Purgebody Avenue ahead).
9. `d` → step down into the street → **Standing on Purgebody Avenue**. You
   realise you're naked, the crowd recoils — **and Death appears and begins
   chasing you from the south.**
10. `n` → **Down a Moist and Unlit Alleyway** (Death hits you — harmless).
11. `n` → **Further Down an Increasingly Dank and Unpleasant Alleyway** (the
    huge dark wooden door looms ahead).
12. `n` → **Before the Great Wooden Door** — your classroom.
13. `open wooden door` → *"...and then I'm falling...falling..."* → **WIN**
    (Part 1 ends).

## Mechanics & notes

- **The win** is task 0, `open wooden door`, `Where = ONE_ROOM` (the Great
  Wooden Door room), **no restriction**, a single `ACT type=6 v1=0` (EndGame /
  win). So any route that reaches that room and opens the door wins; nothing
  needs to be collected, worn, or solved first.
- **Death is the ADRIFT Battle System but is harmless.** Death (NPC 0, starts
  in the Purgebody Avenue room and follows on a walk) prints *"Death hits you"*
  but has no player-killing damage/KilledTask, so he never actually stops you
  before the door. The chase is pure atmosphere; under the fixed seed the dash
  reaches the door every time (verified 3× identical).
- **Two ways to lose** (both avoided by the route above):
  - `eat pizza` while holding the pizza (task 5) → a lose ending.
  - Walking / doing anything to the **grass** in the courtyard (`* grass`,
    task 8) → a lose ending ("the hand of God squashes you"). Stay on the
    gravel paths — i.e. just keep going `n`.
- **Side scenery (optional, unscored):** the **fountain** (`x fountain` then
  `enter fountain`, task 13 gated on having examined it), the foyer **crest**,
  porter **Lester**, and a one-way **maze** room reachable east of Purgebody
  Avenue whose only escape is `push` (a button) — none of these are needed and
  none affect the (nonexistent) score.
- **Faithful to the Runner.** No SCARE change was needed; this is a clean,
  small ADRIFT 3.9 game. No combat-assist required (the only combatant, Death,
  does no damage by design).

## Reproduce

```
sh harness/play.sh "games/tcom.taf" harness/tcom_solution.txt
```
