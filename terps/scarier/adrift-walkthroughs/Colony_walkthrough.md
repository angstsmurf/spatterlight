# Colony — walkthrough

- **Engine:** ADRIFT 4. A short, sturdy sci-fi escape: your ship crash-lands on a
  toxic mining colony and you must reach a small ship on the building roof and
  fly out.
- **Result:** **WIN, full 200/200 (100%), deterministic.** Win screen: *"You get
  in the ship and take control… Congratulations!"*
- Solution file: `harness/colony_solution.txt` (no name/gender prompts).

## Scope

10 tasks, 18 rooms. The four `ChangeScore` actions are **climb cliff +20, climb
cable +30, load bullets +50, shoot rubble +100** (= 200, the maximum). The win
`EndGame` (var1=0) is `fly ship` on the roof; the other endings are deaths
(falling off the cliff without equipment, an explosion, going `east` in the
mine).

## Gear you need (and where it is)

- **body armour** — ship's crew quarters (one deck down).
- **space suit** + **colt 45** — ship's storage hull (two decks down).
- **climbing equipment** — the *waste disposal area* (planet → north tunnel →
  up). Required, or `climb cliff` kills you ("plummet to a bloody…").
- **light helmet** + **explode-on-impact bullets** — the *top of the tower*,
  reached by climbing the cable. The helmet lights the pitch-black mine; the
  bullets load the colt and blast the rubble.

## The win (full 200/200)

```
d                       <- crew quarters
take armour
wear armour
d                       <- storage hull
take colt 45
wear space suit
open door
out                     <- onto the planet
n
n                       <- through the tunnel to its upper exit
u                       <- waste disposal area
take climbing equipment
d
s
s                       <- back out to the planet
e                       <- Near the cliff
n                       <- On the catwalk
climb cliff             <- +20 (needs the climbing equipment) → Main entrance
e                       <- East of the main entrance
u                       <- On the roof (the launch pad — the win is here later)
in                      <- In the bottom of the tower (elevator shaft + cable)
climb cable             <- +30; carries you up to the top of the tower
take light helmet
take bullets
wear light helmet
load bullets            <- +50
d
d                       <- down through the shaft to the mine entrance (now lit)
s                       <- Near a large pile of rubble
shoot rubble            <- +100; the blast opens a passage south to the waste-
                           disposal/sewer, looping you back toward the surface
s
d
s
s                       <- waste disposal → upper exit → tunnel → planet
e
n
climb cliff             <- re-climb to the Main entrance (movement; already scored)
e
u                       <- On the roof
fly ship                <- the win
```

## Gotchas

- **Equipment gates the cliff and the mine.** `climb cliff` is fatal without the
  climbing equipment; the mine is unnavigable (pitch black) without the light
  helmet, and you can't blast the rubble without the loaded colt.
- **The cable climb relocates you** from the bottom to the top of the tower —
  that's how you reach the bullets/helmet; it isn't just a flavour line.
- **The rubble blast is the way back.** There's no "up" out of the mine; shooting
  the rubble opens the southern passage into the waste-disposal area, completing
  a loop back to the surface so you can re-ascend to the roof.
- **Don't go `east` in the mine** — it's an instant death ending.
- The cliff climb is repeatable for movement, so re-climbing it on the way back
  to the roof is fine.
