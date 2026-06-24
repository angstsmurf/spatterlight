# Invasion of the Second-Hand Shirts — walkthrough

- **Engine:** ADRIFT 4. A short surreal escape/puzzle across seven rooms
  (a button, a tree, a brook with a fallen-log bridge, a cottage with a table
  over a trapdoor, a fireplace/chimney, a helium balloon).
- **Result:** **0/0, no ending — an unfinished game.** There is no scored or
  winnable outcome in the data.

## Structural verdict

19 tasks, **7 rooms**, 0 events. A full dump shows **zero `ChangeScore` actions
and zero `EndGame` actions** — no points and no win/lose state. The tasks form a
plausible puzzle chain (push the button, climb the tree, cross the brook on the
fallen tree, move the table → open the trapdoor, examine the fireplace → climb
the chimney, take the helium balloon) but **nothing ever fires a victory
ending**, so the chain leads nowhere terminal. Faithful to the data; the game
appears abandoned before its win condition (and any scoring) was wired up.

## Play (the implemented puzzle chain)

```
push button            <- room 0 (control room)
... (n / down / enter lift to descend)
climb tree             <- room 1
cross fallen tree      <- room 2 (the brook)
move table             <- room 4
open trapdoor
examine fireplace
climb chimney
take helium balloon    <- room 5
```

These advance you through the seven rooms, but with no `EndGame` task the
sequence simply terminates in exploration — there is no reachable ending to bank.
