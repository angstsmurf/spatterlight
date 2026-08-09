# ADRIFT 3.9/4.0 walkthroughs and harness

Working home for the derived ADRIFT 4 walkthroughs and the headless tooling that
produces and re-checks them. It grew out of the SCARE Battle System port
(commit `bf2b595c`, "SCARE: implement the ADRIFT 4 Battle System"); see the
memory notes `scare-battle-system-port`, `adrift-battle-formulas-re` and
`adrift-walkthrough-derivation`.

## Layout

Mirrors `test/adrift5/` (same subfolder names, same policies):

- `notes/` — `*_walkthrough.md`, one per game: the route, the score reached,
  and the reasoning behind any non-obvious step. These are the deliverable.
  Also `WALKTHROUGH_TODO.md` — the derivation method, the standing cautions
  and an index; the corpus is complete, and the session-by-session log it used
  to carry was pruned once it was (recover it from this file's git history) — and `TODO_assist_spatterlight_preference.md`, which
  covers the `SCR_ASSUME_*` assists and the one piece of their exposure that
  is still open. Walkthroughs here also cite two `TODO_*.md` docs that were
  pruned once finished — `TODO_plover_walkthroughs.md` (`8a0bbb66`) and
  `TODO_decode_sub_20_74.md` (`aa30ba4f`); recover either with
  `git show <sha>^:terps/scarier/adrift-walkthroughs/<name>` (both predate the
  2026-08-09 test-tree reorg, so they sit under the old path).
- `goldens/` — a `*_solution.txt` (the command script) and
  `*_solution.expected.txt` (its committed golden transcript) per route.
- `games/` — the `.taf` corpus. **Untracked** (`.gitignore`): third-party game
  data is never committed. Recreate it on another machine with
  `sh test/fetch_games.sh fetch adrift4`, or point `GAMES_DIR` / `ALT_DIRS`
  elsewhere.
- `games.manifest.tsv` — the committed sha256 pin for every file in `games/`,
  with its source URL where one exists. This is what makes the corpus
  reproducible without redistributing it; read `../GAMES.md` before editing it,
  and note that upstream has silently republished several of these games, so a
  row without a source is not an oversight.
- `downloaded/` — third-party walkthrough documents the routes were derived
  from. **Untracked**, same policy as the games.
- `harness/` — the headless tooling.

## Harness

Everything runs against `harness/scare`: a standalone, deterministic build of
the ADRIFT 4 engine from `terps/scarier` — the plain ANSI port, no Glk, with
`seed.cpp` pinning the RNG so a given (game, solution) always yields the same
transcript.

```sh
cd harness
sh build.sh                                   # -> harness/scare

# replay a route and read the transcript
sh play.sh ../games/IceCream.taf ../goldens/icecream_solution.txt

# same, but capped at 12s CPU / 4MB output -- use this while probing an
# unknown route, where a wrong turn can spin an event-heavy game forever
sh safeplay.sh ../games/circus.taf ../goldens/circus_solution.txt

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
longer build; their durable content survives as `terps/scarier/test/adrift4/harness/battle_test.cpp`
and `harness/scproj_test.cpp`.
