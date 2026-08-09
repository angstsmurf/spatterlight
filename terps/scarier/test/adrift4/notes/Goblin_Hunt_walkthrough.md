# Goblin Hunt — walkthrough

- **Engine:** ADRIFT 4 (`goblinhunt.taf`, ScummVM gameid `1h_goblinhunt`). The
  annual goblin hunt is a bimonthly ritual that takes place twice a year, and
  you still have not got a kill to your name.
- **Result:** ★ **WON**, 0/0 — no `ChangeScore` action in the game.
- **Solution:** `goldens/goblinhunt_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `Congratulations!`.

## Map

Nine rooms; the graveyard is a 3×3-ish ring around the Centre.

| # | room | exits |
|---|---|---|
| 1 | Graveyard Gates | N→2 |
| 2 | Graveyard — South | N→4, S→1, NE→5, NW→3 |
| 3 | Graveyard — West | E→4, NE→6, SE→2 |
| 4 | Graveyard — Centre | N→6, E→5, S→2, W→3 |
| 5 | Graveyard — East | E→8, W→4, SW→2, NW→6 |
| 6 | Graveyard — North (the gravedigger's hut) | S→4, SE→5, SW→3 |
| 7 | Gravedigger's Hut | OUT→6 |
| 8 | Cow Field | W→5 |

## Route

```
(blank) ×3       <- the intro's three "...press a key..." pauses
n
x gravestone
nw
ne
knock door
(blank) ×3       <- the knock sequence's three pauses
talk bert ×9
(blank)          <- the pause after Bert hands over the shield
se
e
blow whistle
x corpse
wear armour
w
w
(blank)          <- the pause on Blood's first ambush
s
s
kill blood
```

## Notes

- **Do not walk into the Centre early.** `n` from Graveyard — South is task 16,
  which moves you straight to the Centre and execs task 20 `- reach centre`.
  That in turn runs tasks 21/22: with the armour *worn* you survive Blood's
  ambush, without it task 22 is an `EndGame(kill)`. Going `nw` then `ne` to the
  hut avoids the trip entirely; the same applies to `e` from the West and `w`
  from the East (tasks 19 and 18).
- **`wear armour`, not `take armour`.** Task 21's restriction is on the *worn*
  state; carrying it is not enough, and the difference is fatal.
- Bert refuses to help you eight times. The ninth `talk bert` (task 15) is the
  one that snaps: *"Oh, hell with it!"* — and thrusts the shield into your hands.
- The whistle (from the gravestone of "Thugg the Evil Git") scatters the cows so
  you can search the corpse in the field; the corpse yields the armour.
- Both endings are wins: task 25 (shield **held**) and task 26 (shield in state
  7) are separate `EndGame(win)` tasks with the same `kill blood` pattern.
- The blank lines in the solution file are not padding — each one answers a
  `...press a key...` pause. Getting the count wrong silently eats the next
  command.
