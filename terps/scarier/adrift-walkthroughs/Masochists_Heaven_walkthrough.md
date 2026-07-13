# A Masochist's Heaven — walkthrough

- **Engine:** ADRIFT 4. Tne Mad Monk, ADRIFT 1st 1-Hour Comp 2002 (`1HRGAME.taf`).
- **Result:** ★ **WON, 15/15 MAX.** Three 5-point tasks, then the ending.
- **Solution:** `harness/masochists_heaven_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`).

## Structural verdict

A one-hour comp game and it shows: four rooms, three scoring tasks, one ending.
You died in a bathtub, Death sent you to Heaven, and you are a masochist — so the
whole game is *get thrown out of Heaven*.

## Route

The Key & Compass walkthrough transferred to the ADRIFT original **unchanged** —
no port caveat here (Welbourn played the ADRIFT game). The route is:

1. **Apartment** — `take bottle` (of bubbles) from the table. The room prose says
   the exit is *south*; it is really **north**. (Welbourn flags this too.)
2. **Cloud 9 → Garden of Eden** (`n`, `n`) — `put bubbles in fountain` **(+5)**.
   The gardener drops his apple-shaped watering can; `take can`.
3. **Cloud 9** (`s`) — `water plant` **(+5)**. The potted plant by God's
   administration building grows teeth and chases off the angel at the door.
4. The door does not open with `n` or `in`: the command is **`go entry`**.
5. **In the Big Man's office** — `fuck` **(+5)**. God points a divine finger (the
   middle one), a trap door opens, and you drop into a lake of lava. *"Ah...
   that's more like it!"*

## Notes

- The intro ends on a press-a-key, so the solution file starts with a blank line;
  without it the first command is eaten (§6 step 3 of the TODO).
- Win marker: `Congratulations!`. The game prints no score summary at the end, but
  the three `+5`s are the whole of its `ChangeScore` table — 15 is the maximum.
