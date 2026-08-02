# Quest I (Quest for Flesh) — walkthrough

- **Engine:** ADRIFT 4 (Samuel Mathews, 16 Oct 2003 — 3rd One Hour Game Comp).
  A one-room horror vignette: you are a ravenous creature in a dead-end alley,
  driven by a constantly rising **hunger** variable.
- **Result:** **UNWINNABLE — the game has no victory ending.** Every ending is a
  death. The full **10/10** score *is* reachable, though.
  Solution file: `harness/questi_solution.txt`.

## Structural verdict (re-derived 2026-08-02)

13 tasks, **1 room**, 1 event (`#No Pulse to the Game`, the per-turn hunger
pulse). Full `SCR_DUMP_TASKS` dump:

- The only two `EndGame` actions in the whole game are **`type=6 var1=2`**
  (= `task_run_end_game_action` case 2, *"I'm afraid you are dead!"*):
  - **T3 `#Check for Death from Hunger`** — fires once hunger passes its limit.
  - **T12 `south` / `s`** — walking out of the alley into the street.
- **There is no `EndGame var1=0`** anywhere, and the header carries **no
  WinText** (confirmed by unpacking the `.taf` with `taftool.py` — the string
  simply isn't in the file). `var1=0` is the only path that sets a win, and
  SCARE has no score-based auto-win: `MaxScore` is read only by the `score`
  command's percentage line (`sclibrar.cpp:9474`).

So the verdict stands: this is a survival-flavoured dead end. You feed, hunger
drops briefly, and the timer kills you regardless. This is *authorial intent*,
not a broken/unwired win — the death text even says so:

> *"...Your hunger pains eat you from the inside out... You didn't find the
> flesh you desperately sought."*

## Correction to the previous verdict

The old note claimed the reachable maximum was one +5 feeding (its solution
scored **5/10** and got `Eat what cat?`). That was wrong: the cat is **inside
the closed dumpster** (`OBJLOC obj=4 pos=-10 parent=0` — obj 0 is the dumpster,
`OPENABLE ... openable=6` = closed). Opening it first makes `eat cat` resolve
and the score reaches the full **10/10**.

## Play (max score, then the ending)

```
open dumpster   <- the cat is shut inside; without this, "Eat what cat?"
eat cat         <- +5  (hunger -3)
eat man         <- +5  (hunger -5); "search trash/bags/newspapers" are aliases
score           <- 10 out of a maximum of 10.  (100%)
z x12           <- hunger climbs to the T3 threshold; death ending
```

Both meals are **one-shot** — T5 and T10 each carry a task-state restriction on
themselves, so a second `eat cat` / `search trash` just gives *"There is
nothing else of interest in the trash pile."* With no repeatable food source,
starvation is unavoidable. `south` is an instant alternate death.

The dumpster's `chain`, `hook` and `lid` are pure scenery (no tasks reference
them); `listen`/`smell` are flavour tasks gated on whether you have already
eaten.
