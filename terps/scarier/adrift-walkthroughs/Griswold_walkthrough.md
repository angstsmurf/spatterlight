# The Amazing Uncle Griswold — walkthrough

- **Engine:** ADRIFT 4 (Release 46). David Whyld, IntroComp 2005 — **1st place**
  (`Griswold.taf`).
- **Result:** **Intro completed. 0/0 — there is no score and no win state.**
- **Solution:** `harness/griswold_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`).

## Structural verdict

This is an IntroComp *intro*, and the task dump says so plainly: **no `ChangeScore`
(type 4) action and no `EndGame` (type 6) action exists anywhere in the game.**
There is nothing to score and nothing to win. "Finishing" means reaching the
author's sign-off —

> And there you have it: the intro to The Amazing Uncle Griswold.

— and the room past it, a deliberately unwritten void ("You're floating in a dark,
empty void which looks suspiciously like a part of the game that hasn't yet been
written"). The full game was never released; the site it points at
(shadowvault.net/griswold.htm) is long gone.

That makes it the second corpus game whose faithful maximum is *no ending at all*
(cf. *Trabula*, which has a max score but no ending). The golden's win marker is
the sign-off line rather than a victory message.

## Route

Three rooms (Front Room, Hallway, and the void), and one "puzzle" which is really
a **timer**:

1. The title and the two intro screens each end on `...press a key...` — the
   solution file starts with **three blank lines** to answer them. Without them
   the first three commands are eaten (§6 step 3 of the TODO).
2. A doorbell event fires **around turn 15** (it barks — mother had the bell
   replaced). Only then does `open door` in the Hallway pass its variable
   restriction (TASK 97 → TASK 96, which moves you on).
3. So everything before `e` in the solution is just marking time. Any 15 commands
   would do; the ones used are Welbourn's examines, which is also the most
   interesting reading — the front room, the coat stand, the hat dresser.
4. `e` to the Hallway, `open door` → Uncle Griswold picks you up, carries you in,
   and starts telling you about the goblinoids. Intro over.

## Notes

- Welbourn's page says "There seems to be some confusion whether the front door is
  north or east of the Hallway." The exit table settles it: the front door is
  **east**; `n` from the Hallway goes back to the Front Room.
- Waiting in the Hallway with `z` does *not* short-cut the timer — the event fires
  on its own schedule regardless of where you stand (it is a `where=2` task over
  rooms 0 and 1).
