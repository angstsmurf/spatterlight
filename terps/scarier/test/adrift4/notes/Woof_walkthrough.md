# Woof — walkthrough

- **Engine:** ADRIFT 4 (`Woof.taf`, ScummVM gameid `1h_woof`). A 1-Hour Game
  Comp entry: you are the family dog, and a thief is about to get in through the
  window.
- **Result:** ★ **WON, 30/30 MAX.**
- **Solution:** `goldens/woof_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `I'm back.`

## Map

Six rooms, no compass prose to speak of — the dump is the map:

| # | room | exits |
|---|---|---|
| 0 | Garden — near my basket (the ball is in the basket) | N→4, E→1, W→2 |
| 1 | Garden — near the old oak (bone here, doll in the oak) | N→3, W→0 |
| 2 | Garden — near the neighbour's fence | N→5, E→0 |
| 3 | Garden — the patio (Bizet the cat) | S→1, W→4 |
| 4 | House — main door | E→3, S→0, W→5 |
| 5 | House — near the window | E→4, S→2 |

## Scoring

Six `+5` tasks, all of them dog business:

| task | action |
|---|---|
| 4 | `piss on the tree` at the old oak |
| 5 | get the doll out of the oak |
| 6 | eat the bone |
| 2 | chase Bizet the cat off the patio |
| 3 | put the mouse in the basket |
| 7 | drop the ball under the window |

Chasing the cat is also what *lets the thief in* — the window gets broken while
you are away — and dropping the ball under the window is what he slips on.

## Route

```
Rex              <- answers "Please enter your name:"
take ball
e
piss on the tree
take doll
take bone
eat the bone
n
chase cat
take mouse
s
w
put mouse in basket
w
n
drop ball
z
z
z
z
z
z
z
z
```

## Notes

- Task 8 `@--The owner gets back` is the `EndGame(win)` (`ACT type=6 v1=0`).
  It is fired by event 5, which starts once the thief has been dealt with and
  runs out **eight turns later** — hence the eight `z`s. Nothing else brings the
  owner home; waiting in a particular room does not help.
- The ending quotes the name you typed back at you (`"Rex!!!! I'm back." / Rex I
  love you!`), so the row's win marker is the name-independent `I'm back.`.
- Woof prints **"My score is 30 out of a maximum of 30."**, not "Your score" —
  worth knowing if this ever has to fall back to the corpus's tour-row marker
  convention.
