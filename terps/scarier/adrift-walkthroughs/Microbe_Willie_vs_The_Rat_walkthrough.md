# Microbe Willie vs. The Rat — walkthrough

- **Engine:** ADRIFT 4 (`microbe_willie.taf`, Jeremy Yoder, **1st ADRIFT
  One-Hour Game Competition**, 2002). You are a microbe on a suicide mission
  inside a rat.
- **Result:** ★ **WON, 7/7 MAX.**
- **Solution:** `harness/microbe_willie_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `pestilence (basically, more of your kind) throughout the world.`

## Structural verdict

18 tasks, 8 rooms, two NPCs (Cynthia and the White Blood Cell). Seven tasks
carry `ChangeScore(+1)` — 1, 2, 3, 5, 7, 9 and 10 — and nothing else scores, so
the maximum is 7 and this route takes all of it. Task 10, `put cheese in
nerve`, is also the only `EndGame(win)`.

## Route

```
get cheese
s
get worm
e
get alveoli
w
s
e
open alveoli
talk to cynthia
w
n
e
n
n
give virus to white blood cell
put cheese in nerve
```

## Notes

- **The Kidneys are a timed trap, and they are the reason the alveoli exists.**
  `e` in the Intestines is task 13, which teleports you into room 5 and shuts
  the way back behind you: `EXIT room=5 W gateObj=8 wantState=0` — the exit is
  literally gated on the sac of air being open. Task 5 (`open alveoli`) needs
  the sac **held with the air still in it**, so you must pick it up back in the
  Lungs first. Opening it re-opens the exit, scores, and consumes the sac.
- The clock is real: `EVENT 0 [OPEN KIDNEY]` runs task 8 seven turns later and
  shuts the opening again, and `EVENT 1 [LOCK KIDNEY]` runs task 14
  (`DIE FROM INSIDE KIDNEY`, `EndGame(death)`). Do the two kidney moves — open
  the sac, talk to Cynthia — and get out.
- In the Brain, task 10 carries a character restriction as well as needing the
  cheese, so the White Blood Cell must be gone first. That is what task 9
  (`give virus to white blood cell`) is for: the cell chases the virus out of
  the room.
