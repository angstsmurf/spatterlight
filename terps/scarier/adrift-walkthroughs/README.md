# ADRIFT 3.9/4.0 walkthroughs and harness

Working home for the derived ADRIFT 4 walkthroughs and the headless tooling that
produces and re-checks them. It grew out of the SCARE Battle System port
(commit `bf2b595c`, "SCARE: implement the ADRIFT 4 Battle System"); see the
memory notes `scare-battle-system-port`, `adrift-battle-formulas-re` and
`adrift-walkthrough-derivation`.

## Layout

- `*_walkthrough.md` — one per game: the route, the score reached, and the
  reasoning behind any non-obvious step. These are the deliverable.
- `WALKTHROUGH_TODO.md` — the running derivation log and the queue of games not
  yet done. `TODO_user_assist_metacommands.md` covers the `SCR_ASSUME_*` assists.
- `games/` — the `.taf` corpus. **Untracked** (`.gitignore`): third-party game
  data is never committed. A mix of real files and symlinks into wherever the
  originals were downloaded; recreate it on another machine, or point
  `GAMES_DIR` / `ALT_DIRS` elsewhere.
- `harness/` — the headless tooling, plus a `*_solution.txt` (the command
  script) and `*_solution.expected.txt` (its committed golden transcript) per
  route.

## Harness

Everything runs against `harness/scare`: a standalone, deterministic build of
the ADRIFT 4 engine from `terps/scarier` — the plain ANSI port, no Glk, with
`seed.cpp` pinning the RNG so a given (game, solution) always yields the same
transcript.

```sh
cd harness
sh build.sh                                   # -> harness/scare

# replay a route and read the transcript
sh play.sh ../games/IceCream.taf icecream_solution.txt

# same, but capped at 12s CPU / 4MB output -- use this while probing an
# unknown route, where a wrong turn can spin an event-heavy game forever
sh safeplay.sh ../games/circus.taf circus_solution.txt

# the regression: every route replayed and strict-diffed against its golden
sh run_v4_walkthroughs.sh          # table + exit code
sh run_v4_walkthroughs.sh -v foo   # dump the diff for matching rows
sh run_v4_walkthroughs.sh --bless  # (re)record goldens

# deterministic projectile-combat regression (five games, own golden)
sh scproj_regress.sh
```

`run_v4_walkthroughs.sh` is wired into `make -f Makefile.headless test` via the
`v4walkthroughs` target, which skips the whole thing when no `games/` corpus
exists on the machine. Rows whose `.taf` is missing SKIP rather than fail.

## Elsewhere

The reverse-engineering inputs behind the battle port are not in the repo — the
DotFix VB Decompiler output of `run400.exe` (`Battles.bas` is the combat engine
the port was derived from), the original ADRIFT 4 Runner/Generator and manual,
and the jAsea jar — and live under `~/adrift-battle/`. The one-off C probes that
sat beside them were written against the pre-rename C `terps/scare` and no
longer build; their durable content survives as `terps/scarier/test/battle_test.cpp`
and `harness/scproj_test.cpp`.
