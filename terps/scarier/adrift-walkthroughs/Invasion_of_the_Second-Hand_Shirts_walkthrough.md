# Invasion of the Second-Hand Shirts — walkthrough

- **Engine:** ADRIFT 4 (`Invasion of the Second-Hand Shirts.taf`, ScummVM gameid
  `invasionshirts`). A short surreal chain across seven rooms: a tree with a
  lift shaft, a grassy field, a brook with a fallen-log bridge, a lake shore, a
  cabin with a table over a trapdoor, the cabin roof, and a helium balloon.
- **Result:** **0/0, no ending — an unfinished game.** There is no scored or
  winnable outcome in the data.
- **Solution:** `harness/invasion_shirts_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). This is a **tour row**, not a win: it walks the full
  room chain and its marker is the last room's description line,
  `You're floating through the air above the trees.`

## Structural verdict

19 tasks, **7 rooms**, 0 events, 3 NPCs (a leech, Aphrodite, a silver sphere).
A full dump shows **zero `ChangeScore` actions and zero `EndGame` actions** — not
one `ACT type=6` in the whole table. The tasks form a plausible puzzle chain but
**nothing ever fires a victory ending**, so the chain simply stops in the last
room. Faithful to the data; the game appears abandoned before its win condition
(and any scoring) was wired up.

## Route (the implemented chain, in full)

```
push the button        <- Tree: opens the trunk's lift shaft (task 0)
down                   <- task 2 refuses ("It looks pretty dangerous")
down                   <- task 3 leaps -> Grassy Field
e                      <- Brook
x brook                <- task 7: reveals the woman in the water
x woman                <- task 6: Aphrodite comes ashore
cross fallen tree      <- task 8 -> the waterfall -> Lake Shore
(blank) ×2             <- the waterfall's two [More] pauses
s                      <- Small Cabin
turn crank             <- task 14: the music box pops open, loosing the sphere
x the fireplace        <- task 15
climb chimney          <- task 17 refuses ("Looks like a pretty tight fit")
climb chimney          <- task 16 does it -> Cabin Roof
take the balloon       <- task 18 -> Helium Balloon
look
```

## Notes

- **`cross the fallen tree` is not understood.** Task 8's pattern is
  `{go/goto/move} {cross} {fallen} [e/east/tree/bridge/log]` and `the` is not one
  of the optional words, so it has to be `cross fallen tree` (or just `east`).
- `move the table` and `open the trapdoor` both fail ("The table doesn't budge",
  "The table is in the way") — the trapdoor is scenery that leads nowhere, and
  the real way out of the cabin is up the chimney.
- The last room contains a microwave oven and the silver sphere. Nothing can be
  done with either.
