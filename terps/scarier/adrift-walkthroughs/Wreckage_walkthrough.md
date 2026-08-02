# SE: Wreckage — walkthrough

- **Engine:** ADRIFT 4 (`Wreckage.taf`, ScummVM gameid `1h_wreckage`). A 1-Hour
  Game Comp entry: your escape pod is falling into a star and a tong is jammed
  in the hull.
- **Result:** ★ **WON, 50/50 MAX** — all five `+10` tasks.
- **Solution:** `harness/wreckage_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `you've rescued yourself`.

## Structural verdict

Five `ChangeScore` tasks at `+10`, and their restrictions force a single order.
The pod's fall is a turn counter (variable 2, checked every turn by tasks 1 and
2), so the route has to be short — there is no room to explore.

| task | step | why it must come here |
|---|---|---|
| 10 | `open hatch` **while wearing the suit** | without the suit, task 9 fires instead and kills you |
| 11 | `attach cable to suit` | also cancels event 1, the "drifted too far from the pod" death |
| 13 | `turn on the repairbot` | needs it **held** and in state 1 |
| 6 | `tell repairbot to cut the tong` | needs the bot back in state 0 |
| 14 | `use the computer` | needs task 6 done; execs task 3 `#WINNING TEXT` |

## Route

```
open compartment
take suit
wear suit
open hatch
attach cable to suit
take repairbot
turn on the repairbot
tell repairbot to cut the tong
in
use the computer
```

## Notes

- `reset the repairbot` — the phrasing the room description suggests — is **not
  understood**. Task 13's pattern requires the literal word `on`, so it has to be
  `turn on the repairbot`.
- The transcript ends with `Congratulations!` printed **twice**: task 3's winning
  text and the engine's own end-of-game line.
