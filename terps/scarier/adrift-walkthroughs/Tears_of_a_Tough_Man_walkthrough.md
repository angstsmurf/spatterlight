# Tears of a Tough Man — walkthrough

- **Engine:** ADRIFT 4 (`Tear.taf`, Manuel Angel Muñoz Rodríguez, InsideADRIFT
  Summer Comp 2004). A short, bleak piece: you wake in an abandoned cottage
  with no memory, and every puzzle you solve hands back another diary entry.
- **Result:** ★ **WON, 6/6** — a genuine full score. The dump contains exactly
  six one-point awards and this route collects all of them.
- **Solution:** `harness/Tear_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `Suddenly the world seems a brighter place, and you feel there is a good`.
- **Provenance:** no author walkthrough exists — plover.net lists the game
  without one. The route was derived from `SCR_DUMP_TASKS`.

## Route

```
x stains / break poster / e / open door / get flashlight / e / get iron / get battery
put battery in flashlight / n / get axe / get rope / bend iron / tie rope to hook / s / s
cut tree / get log / n / n / work wood / s / s / w / d
get extinguisher / u / e / n / e / extinguish fire / s / score
destroy totem
```

(one command per line in the solution file; no blank lines — the game has no
`<waitkey>` pauses, checked with `SCR_MARK_WAITKEY=1`.)

## Scoring

All six points are memories, one per award:

| Task | Command | |
|---|---|---|
| 3 | `x stains` | the blood on the cottage floor |
| 26 | `#memory in cave` | fires on first entering the Dark Cave |
| 27 | `#memory in river` | fires on crossing the river |
| 22 | `work wood` | building the raft from the felled log |
| 18 | `extinguish fire` | the burning cabin |
| 33 | `destroy totem` | the last entry, and the ending |

`EVENT 2 [Daemon]` re-runs `TASK 34 [#endgame]` every turn; its only
restriction is `type=4 v1=3 v2=3 v3=5` — variable 1 (the memory tally) ≥ 5 —
so the game ends the moment the sixth memory lands.

## Notes

- **`break poster` is mandatory.** The poster in the first room hides a wooden
  plank, and removing the plank is what unlocks the cottage door. There is no
  gentle way to do it: `look behind poster`, `move poster` and `take poster`
  are all refused. (`read poster` is worth a look anyway — "reredrum,
  reredrum, reredrum".)
- **The east exit is gated on an object, not a task.** Even with the poster
  broken, `e` from Room answers "You can't go in that direction (at present)".
  The gate is `EXIT room=1 E -> dest=2 gateObj=15 [cottage door] wantState=0`
  — the door simply needs `open door`. This cost a detour hunting for a
  nonexistent task, and it is what forced the `scdump.cpp` EXIT fix below.
- **The flashlight is inside the armchair** and the iron and battery are in
  the pile of trash, but neither `search armchair` nor `search trash` is
  understood — `get flashlight` / `get iron` / `get battery` reach in directly.
- **Two gates chain through the tool shack.** The cave entrance is gated on
  the flashlight's state, so `put battery in flashlight` has to happen before
  going north; and descending into the Cave Pit needs `TASK 11`
  (`tie rope to hook`), which in turn needs `bend iron` to make the hook. The
  axe and rope both live in the shack north of the yard.
- The extinguisher is at the bottom of the pit, and it is the only thing down
  there — the descent exists purely to fetch it for the burning cabin.

## Harness fix this game forced

`scdump.cpp` printed every gated exit as `gateTask=<Var1-1>`. That is only
right when `Var3 == 0`. `lib_can_go()` reads **Var3 as the restriction type**:

- `Var3 == 0` — task state; `Var1-1` is a task index, and the exit opens when
  `done != (Var2 != 0)`, so `Var2 == 0` means *the task must have been done*;
- `Var3 == 1` — object state; `Var1-1` is a **stateful-object** index and
  `Var2` is the wanted openness/state.

The dump now branches on `Var3` and resolves the stateful-object index through
`obj_stateful_object()` so it can print the object's name. The old output
would send any derivation looking for a task that does not exist whenever an
exit is gated on a door being open — exactly what happened here.
