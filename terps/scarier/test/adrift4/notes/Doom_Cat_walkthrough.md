# Doom Cat!! — walkthrough

- **Engine:** ADRIFT 4 (`ticktick.taf`, **3rd ADRIFT One-Hour Game
  Competition**, 2003). You wake up. The cat is ticking.
- **Result:** **UNWINNABLE BY DESIGN — the only ending is your death.** This
  route is the complete game.
- **Solution:** `goldens/ticktick_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Marker: `I'm afraid you are dead!`

## Structural verdict

10 tasks, 4 rooms, no NPC objects beyond the cat. The game has **no scoring
system** and exactly one ending: task 7 (`read note`), an `EndGame(death)`.
There is no `EndGame(win)` anywhere in the file, so getting the cat out of the
flat is *not* a win — the joke is that the bomb was never the cat.

## Route

```
s
e
open oven
take tin
feed cat
get cat
w
break window
throw cat out window
n
look in mattress
read note
```

## Notes

- **The tin of cat food is in the oven** — not the fridge and not the cupboards,
  both of which open onto nothing.
- Task 1 (`feed cat`) needs the tin held; task 2 (`get cat`) is gated on task 1,
  because the cat can only be picked up once it has been *"bloated and slowed by
  the tuna bison cat food"*.
- Task 3 (`throw * cat * window`) is gated on task 4 (`break window`), so smash
  the window **before** throwing, and task 6 (`look in mattress`) is gated on
  task 3.
- The nagging one-liners between turns (*"TICK tick TICK…"*) come from
  `EVENT 0 [tickcat]`, which restarts with a period of 8, so its message choice
  varies with the turn count. The seeded harness makes that reproducible.
