# The Pyramid of Hamaratum — walkthrough

- **Engine:** ADRIFT 4 (`pyramid.taf`, KF, **3rd ADRIFT One-Hour Game
  Competition**, 2003).
- **Result:** ★ **WON, 100/100 MAX.**
- **Solution:** `goldens/pyramid_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `moves out of your way allowing you to make a hasty retreat.`

## Structural verdict

Three tasks, four rooms, no NPCs. The only scoring action in the whole game is
task 2 (`ChangeScore(+100)`), which also ends it (`EndGame(win)`), so the
maximum is 100 and this route reaches it in 11 commands.

## Route

```
        ← blank   ("[Press return to continue]" title screen)
open southern wall
s
s
e
open sarcophagos
take beetle
w
n
n
put beetle in depression
```

## Notes

- **The title screen eats a line.** The game opens with
  `[Press return to continue]`, so the first line of the solution must be blank
  or its command is swallowed.
- **The southern wall is a closed openable that gates the exit.** Until you
  `open southern wall`, every `s` answers *"You walk into the Southern Wall."*,
  which reads exactly like a broken exit. `open wall` alone does not parse — use
  the full noun.
- **The Short passage is lethal on a one-turn fuse.** `EVENT 0 [beheaded]` is
  started by the move south and one turn later runs task 1
  (`$$ blade swings out`, an `EndGame`). Task 1 is defined only in room 1, so it
  only kills you if you are *still there*: keep moving, `s` and then `s` again on
  the very next turn. The event does not restart, so the return trip north is
  safe.
- The win (task 2) is driven by `EVENT 1 [Run each turn]` and simply checks that
  the golden beetle is inside the depression back in the Entrance Chamber. The
  game is honest about the cost: *"as you want to get out, you have to leave the
  beetle in its place rather than take it as you had hoped."*
