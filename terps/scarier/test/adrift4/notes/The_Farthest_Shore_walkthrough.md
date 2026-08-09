# The Farthest Shore — walkthrough

- **Engine:** ADRIFT 4 (`shore.taf`, Stewart J. McAbney, **3rd ADRIFT One-Hour
  Game Competition**, 2003). *"An Interactive Reverie."*
- **Result:** **WON** by the engine's own reckoning — the single ending is an
  `EndGame(win)` whose text is a drowning.
- **Solution:** `goldens/shore_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `an island shrouded in a steel fog.`

## Structural verdict

10 tasks, 6 rooms, one NPC (a cow). There is **no scoring system** — not one
task carries a `ChangeScore`, so the status line stays at 0 and "maximum score"
is meaningless here. Task 7 (`# Beach - South`) is the only ending, and ADRIFT
counts it as a win. This route reaches it after touring every room and firing
every other task in the game.

## Route

```
listen
smell
e
z
z
e
x tree
u
x signpost
w
x cabin
in
x table
take basket
take turnip
take leek
eat leek
out
e
s
give leek to cow
n
d
s
```

## Notes

- **You cannot leave the dinghy at will.** Task 2 (any direction) and task 3
  (`@ Dinghy - Land the boat`) both test the same variable, and task 3 is fired
  by `EVENT 0 [@ Dinghy - Restriction]` five turns into the game. Until then
  every `e` answers *"you decide against stepping out of the dinghy"* — you have
  to burn the five turns. `listen`, `smell` and two waits do that while showing
  the dinghy's three set pieces.
- The basket and the turnips are deliberate refusals (tasks 8 and 4: *"you
  couldn't hope to lift the basket"*, *"it is so rotten that it almost fades in
  your hand"*).
- **The leek is the only takeable object in the game**, and feeding it to the cow
  (task 6) sets the variable that puts the line *"A light, dim in the fog,
  shines in from the south."* into the beach description — the cue for the
  closing move.
