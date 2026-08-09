# Dance Fever USA — walkthrough

- **Engine:** ADRIFT 4 (`DFU.taf`, **2nd ADRIFT One-Hour Game Competition**,
  2003). In 2018 an epidemic of Dance Fever sweeps the country; you are Agent
  Wasp.
- **Result:** ★ **WON, 999999999/999999999 MAX** — and that is not a typo, see
  below.
- **Solution:** `goldens/dfu_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `Thank you, and good night.`

## Structural verdict

24 tasks, 4 rooms, one NPC (Bruno the bouncer). Two tasks end the game, both
`EndGame(win)`:

- task 21, `use wrench on machine` — a joke ending in the alley, no award;
- task 19, `hit battery * wrench` — the real finish, carrying
  `ChangeScore(+999999999)`.

That single absurd award is the **only** scoring action in the file, so the
maximum genuinely is 999999999 and this route takes it.

## Route

```
take card
smell
e
x graffiti
pet cat
pee on the ground
x pee
push red button
w
n
smell
listen
dance
take acid
use tissue on acid
kick door
use wrench on door
use acid on door
n
hit pack
hit battery with wrench
```

## Notes

- The gate chain is entirely non-obvious, and the graffiti in the alley is the
  only clue to its first link:
  - `pee on the ground` (task 7) →
  - `push red button` (task 2, restricted on task 7 being done) — the shock
    through the puddle gives you the smouldering afro →
  - `n` (task 1, restricted on task 2) — Bruno only lets a smouldering afro
    into the club. This task **teleports** you; the exit table has no north
    exit from the alley.
- `pet cat` (task 5) is what makes the cat cough up the **wrench**. `hit cat`
  is task 4, an instant `EndGame(death)`.
- `dance` on the cardboard (task 11) produces the tissue;
  `use tissue on acid` (task 13) makes the acid-soaked tissue;
  `use acid on door` (task 17) opens the way north.
- `kick door` and `use wrench on door` are the two refusals that tell you the
  door needs chemistry, and `hit pack` (task 18) is the *you need something
  stronger* nudge before the finish.
