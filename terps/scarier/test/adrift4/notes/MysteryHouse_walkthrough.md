# Mystery House — walkthrough (**UNWINNABLE as authored**)

- **Engine:** ADRIFT, second person. A short haunted-house puzzle game
  built around a hidden key, a secret passage behind a bookshelf, and a
  Treasure Chest that is supposed to be openable near the end.
- **Result:** **UNWINNABLE**, the same class of bug as `Bedlam.taf` — a
  missing `ACT` line. Confirmed with `SCR_DUMP_TASKS=1`: TASK 3
  ("open chest") only prints "Wow! A Chest full of what???" and has zero
  `ACT` lines, so the Treasure Chest's Openable engine state is never
  actually flipped. TASK 0, the game's only win condition
  (`drop treasure Chest`), has a `RESTR type=1` checking that the chest is
  genuinely open, which can now never pass. Confirmed empirically
  3x-deterministic: the final `drop treasure Chest` always answers with the
  generic library message "You drop the Treasure Chest.", never the
  WINTEXT. Wired as
  `mysteryhouse_solution.txt|MysteryHouse.taf|You drop the Treasure Chest.`,
  no env.

## The walkthrough

```
n
e
in
examine sofa
get key
n
n
x desk
get matches
x bookshelf
get candle
move bookshelf
e
light candle
s
s
push button
s
get Treasure Chest
open chest
w
w
drop treasure Chest
```

Solves every real puzzle in the house — the sofa's hidden key, the desk's
matches, the bookshelf's secret passage revealed once a candle is lit, and a
button that opens a hidden vault — and ends at the proven dead end: opening
the Treasure Chest (which only narrates, never actually opens it) and
dropping it, which can never trigger the win.
