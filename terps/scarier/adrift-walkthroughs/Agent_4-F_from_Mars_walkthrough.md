# Agent 4-F from Mars — walkthrough

- **Engine:** ADRIFT 4 (`agent_4F[1].A.taf`, Michael Arnaud, ScummVM gameid
  `1h_agent4fmars`). A 1-Hour Game Comp entry: you have "borrowed" a Model
  M-7-double-Oh and are flying into the sun to show up the scientists on Mars.
- **Result:** ★ **WON**, 0/0 — no scoring.
- **Solution:** `harness/agent4f_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `Congratulations!`.

## Structural verdict

Two rooms and 15 tasks, most of which are scenery. It is very nearly a cutscene:

```
intro  ->  task 3 "# introscreen 4 #"
       ->  event 2 "## lude 1 ##"  (2 turns)  -> task 9
       ->  event 3 "## lude 2 ##"  (3 turns)  -> task 10
       ->  event 4 "## lud 3 ##"   (3 turns)  -> task 11
       ->  task 11 moves you to room 1, "back on mars"
       ->  task 14, pattern [*], is the EndGame(win)
```

Task 14 matches **any** command at all, so the first thing you type once you are
back on Mars wins the game.

## Route

```
tighten restraint
z
z
z
z
```

## Notes

- **Never `push the red button`** (task 4). It arms event 1 `# AUTO DESTRUCT #`,
  and task 6 `## chekkTNT ##` then `EndGame(stop)`s you two turns later —
  "URZATZ dutifully detonates the onboard solidified nitroglycerin explosive" —
  well before the interlude chain can finish. (`push the green button` after the
  red one, task 7, unsets it again, but there is no reason to go there.)
- `tighten restraint` is the other red herring: the ending text later points out
  that the fibroid personnel restraint was useless anyway, and it does not
  affect the timing. Five bare `z`s win just as well; the restraint is kept in
  the route only because it reads better than five waits.
- The intro has no press-a-key pauses, so the solution file needs no leading
  blank lines.
