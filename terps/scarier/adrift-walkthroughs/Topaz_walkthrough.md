# Topaz — walkthrough — ⚠ DEFERRED (SCARE cannot win it)

- **Engine:** ADRIFT 4 (native). Woodfish, ADRIFT 4th 1-Hour Comp 2004. Dig up
  the sentient sword Topaz, reach the skeleton of its old master Paragon, take
  the silver ring and wear it.
- **Status:** **Not in the regression** — under the current SCARE engine the
  game is **unwinnable**, so no golden is blessed. Best-effort path is banked in
  `harness/topaz_solution.txt` for future investigation; there is deliberately
  **no MAP row** for it (a persistent FAIL would break the suite's exit code).
- **Walkthrough source:** *Key & Compass* (native-ADRIFT — the walkthrough wins
  on the real ADRIFT Runner).

## Why it can't be won under SCARE (2026-07-02)

The winning task is `TASK 22 = [wear/put on/try on]{the}{silver}[ring]` in the
skeleton room, whose action is `EndGame(win)`. But it carries one restriction —
an **object-location restriction `(var1=0, var2=0, var3=0)`** which, decoded
(`screstrs.cpp` `restr_pass_task_object_location` → `restr_object_in_place`),
means **"no object anywhere may be hidden."**

The game ships **two** ring objects: a static ring on the skeleton (obj 7) and a
**dynamic ring (obj 8) that starts hidden** (`OBJLOC obj=8 pos=-1 room=-1`).
`EVENT 3` (`o2=8->0`, started by "talk to sword/topaz") is supposed to move it,
but under SCARE it never fires — a task+event trace shows only `moving object 5
to held by 0` (the sword) across the whole endgame; obj 8 stays hidden through
every command/choice permutation tried (`1`/`2` at the choice, take-before/after
-talk, extra talks, etc.). With a hidden object always present, TASK 22's
restriction always FAILs, so the wear-ring win never triggers — SCARE reports
the ADRIFT default "You can't wear the silver ring."

This is a genuine **SCARE ↔ ADRIFT Runner incompatibility** (event/duplicate-
object handling), not a walkthrough error — the same class of finding this
harness exists to surface. Investigate `EVENT 3` firing / the hidden-duplicate
object before banking a golden.

## How it was diagnosed (reusable)

Enabled by fixing the harness build's dump macro (see below). With the
dump-capable `scare`:

```sh
printf 'look\nquit\ny\n' | SCR_DUMP_TASKS=1 ./scare games/topaz.taf 2>tasks.txt   # dump every task's cmd+restrictions+events
{ cat path.txt; echo 'wear silver ring'; } | SCR_TRACE_TASKS=1 ./scare games/topaz.taf 2>trace.txt  # per-restriction PASS/FAIL
printf 'look\nquit\ny\n' | SCR_DUMP_OBJLOC=1 ./scare games/topaz.taf 2>obj.txt     # initial object positions (spot the hidden obj 8)
```

**Build fix that unlocked this:** `harness/build.sh` was defining
`-DSCARE_DUMP_TOOLS`, but after the `scare`→`scarier` rename the instrumentation
guard is `SCARIER_DUMP_TOOLS` — so the dump/trace tools had been silently
compiled out (dead code). `build.sh` now defines `-DSCARIER_DUMP_TOOLS`; the
tools are env-var-gated, so the binary is behaviourally identical with the vars
unset (all 17 goldens still pass) but `SCR_DUMP_TASKS` / `SCR_TRACE_TASKS` /
`SCR_DUMP_OBJLOC` now work for route debugging.
