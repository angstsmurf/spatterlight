# A monkey too many — walkthrough

- **Engine:** ADRIFT 4 (`amonkeytoomany.taf`, **1st ADRIFT One-Hour Game
  Competition**, 2002). Two rooms, one monkey warder, one stupifyingly strong
  magnet.
- **Result:** ★ **WON, 25/25 MAX.**
- **Solution:** `harness/amonkeytoomany_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `Hooray! You've made it through the
  game!`

## Structural verdict

10 tasks, 2 rooms, one NPC. Only two tasks score: task 8 (`shoot * monkey *`,
`+10`) and task 9 (`unlock * door * with * key`, `+15`), and task 9 is also the
`EndGame(win)`. Maximum 25.

## Route

```
look
i
n
talk to monkey
talk to monkey
talk to monkey
talk to monkey
i
use magnet on pistol
use magnet on key
shoot monkey
unlock door with key
```

## Notes

- **The Dark place has no exits at all** in the exit table. Task 0 is what
  moves you, and it matches every direction word (`[go * north]` plus the bare
  `n`/`s`/`e`/`w` and their long forms). The same task hands you the magnet, so
  you arrive in the cell already equipped — `i` in the dark (task 5) is pure
  flavour and is in the route only for completeness.
- Tasks 1–4 are four *separate* one-shot `[talk * to * monkey]` tasks that fire
  in file order, so the four conversations are a fixed sequence. The third
  (*"Ah, who cares if the monkey gets angry!"*) is the funny one and the fourth
  closes the thread. None of them gate anything.
- The two `[use * magnet * on * X]` tasks pull the pistol and the key off the
  monkey's desk and into the cell while it reads the newspaper.
- The **shortest** win is `n` / `use magnet on pistol` / `use magnet on key` /
  `shoot monkey` / `unlock door with key` — five commands, same 25 points — but
  it skips half the game's text, so the banked route takes the long way.
