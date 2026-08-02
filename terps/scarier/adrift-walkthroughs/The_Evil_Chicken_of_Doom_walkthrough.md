# The Evil Chicken of Doom! — walkthrough

- **Engine:** ADRIFT 4 (`chicken.taf`, **1st ADRIFT One-Hour Game
  Competition**, 2002). The first of the four Evil Chicken games in this
  corpus (see also `ECOD2`/`ECOD3` and the ADRIFT 5 `ECOD3D`). A chicken has
  two noses; you and Steve resolve to do something about it.
- **Result:** **WON**, 0/0 — no scoring system (no `ChangeScore` anywhere).
- **Solution:** `harness/chicken_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `That was the last time either of you threw a brick at something.`

## Structural verdict

19 tasks, 5 rooms, two NPCs (the chicken and Steve). The one ending is task 18,
`shoot chicken` — an ADRIFT `EndGame(win)` whose text has you arrested for
poultrycide and jailed for life. That *is* the victory condition; there is
nothing better in the file.

## Route

```
take the spade
scratch back
n
take key
n
take hammer
s
e
dig in dirt
w
use chainsaw on shed
w
take toolbox
hit lock
hit lock with hammer
take gun
e
s
listen
smell
hit chicken with hammer
shoot chicken
```

## Notes

- The game is a strict tool chain — each task's `MoveObject(→ held)` hands you
  the next tool:

  | command | task | yields |
  |---|---|---|
  | `take the spade` | 3 | spade (Chicken Coop) |
  | `take key` | 0 | key, hidden in the Yard's weeds and **not** in the room text |
  | `n` | 1 | needs the key held; the task itself teleports you to the Ruins — this is not an exit-table move |
  | `take hammer` | 2 | hammer |
  | `dig in dirt` | 9 | chainsaw (Garden, needs the spade) |
  | `use chainsaw on shed` | 13 | opens the Yard ↔ Shed exits (`EXIT room=1 W gateTask=13`) |
  | `hit lock with hammer` | 16 | gun |

- `take toolbox` (task 14) and the bare `hit lock` (task 15) are deliberate
  refusals, kept in the route because that is where the shed's jokes live.
  `hit chicken with hammer` (task 10) is the game's own *you need a better
  weapon* nudge, and `listen` / `smell` / `scratch back` are one-shot gags.
