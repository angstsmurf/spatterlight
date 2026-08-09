# ARGH's Great Escape — walkthrough

- **Engine:** ADRIFT 4 (`ARGH_sGreatEscape.taf`, ScummVM gameid
  `1h_arghgreatescape`). A 1-Hour Game Comp entry: you are ARGH, a caveman in
  Boulder Rock Prison, and you have just invented the wheel.
- **Result:** ★ **WON, 98/125 — 98 is the true maximum**, not 125.
- **Solution:** `goldens/argh_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `Congratulations!`.

## Why 98 and not 125

The task dump has five `+25` `ChangeScore` actions, which is where the
advertised 125 comes from, but they can never all fire:

- Tasks **9** and **10** are two *alternative* `south` / `escape` endings. Both
  award `+25` and both end the game, so at most one of the pair can ever run.
- Task **0** (`#Hit bars`) is the *start task* of the beaver event, and it
  carries a mandatory **−2**. There is no route to the beaver that skips it.

So the ceiling is four `+25`s minus 2 = **98**, and this route reaches it:

| task | action | points |
|---|---|---|
| 0 | hit the bars with the drumstick | −2 |
| 3 | feed the drumstick to the beaver | +25 |
| 8 | wear the beaver | +25 |
| 6 | feed the medicine to the beaver | +25 |
| 10 | escape south | +25 |

## Route

```
take drumstick
hit bars with drumstick
feed beaver the drumstick
take beaver
get medicine
wear beaver
get dress
wear dress
feed the beaver the medicine
south
```

Hitting the bars is what brings the beaver; feeding it the drumstick tames it;
worn as a hat (with the dress) it gets you past the guard; the medicine revives
it enough to chew through the bamboo bars, and then `south` is the escape.

## Notes

- The ending text is a small tragedy: the same beaver kills ARGH a few years
  later, and its fangs end up mounted on the prison wall in his honour.
