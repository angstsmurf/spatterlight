# Replaying the walkthroughs in stock SCARE

This is the measurement behind "Games that work in Scarier but not in SCARE"
in `../../../notes/SCARIER_vs_SCARE.md`. It replays every row of
`../harness/run_v4_walkthroughs.sh` that has a win marker through SCARE 1.3.10
and records which ones reach the marker.

    ./build.sh                                  # -> build/scare
    python3 -I replay.py build/scare results.tsv
    python3 -I analyze.py results.tsv

The games must be in `../games/`, as for the walkthrough suite. A full run
takes about a minute on all cores.

## What the patch changes

`scare1310.patch` applies to `terps/scare` at `ed31bf462`, SCARE 1.3.10 as
Spatterlight took it from Gargoyle.

- `os_ansi.c`, harness only: lines starting with `#` are skipped, both at the
  prompt and where a key pause would eat a line; `SCR_SKIP_WAITKEY` makes key
  pauses transparent; `SCR_SEED` turns on SCARE's portable generator with that
  seed, before the game is loaded, since loading draws the events' start
  times.
- `scvars.c`, also harness only: while `SCR_SEED` is set the clock behind
  `%time%` stands still, as Scarier's does when seeded.
- `sctasks.c` and `scrunner.c`, the one change to what the engine does: the Runner's
  end-of-game score summary ("You scored N out of the maximum M!" and the lines
  after it) is printed at the end of the turn that ended the game, as
  `task_print_end_game_summary()` in Scarier prints it. About a third of the
  win markers are a line of it.

## How a row is judged

`replay.py` feeds the solution file without the lines that answer Scarier's
name and gender prompts, which SCARE never asks. It finds those lines by
lining up the golden's `> x` echoes with the solution. Each row gets five
seeds, its own `SCR_SEED` first, and counts as a win if any one of them reaches
the marker, with runs of white space collapsed because SCARE's front end
wraps lines. A row that does not win is `ABORT` if SCARE exited with an error
or assertion, `NOLOAD` if it refused the file, and `NOWIN` otherwise.

The `SCR_ASSUME_*` switches of a row have no SCARE equivalent and are ignored.
The `SCR_SEED` field is read the way the suite's `env` call reads it, so a row
written `SCR_SEED=149|SCR_SKIP_WAITKEY=1` gets seed 149 and no skip.

`analyze.py` groups the rows by game and by ADRIFT version, which it reads
from the `.taf` signature. It prints lists A and B, and diffs list C against
the note.

## Results

| Date | Rows won | Games won | Not won | Aborts | Not loaded |
|---|---|---|---|---|---|
| 2026-10-11 | 439 / 663 | 432 / 609 | 163 | 11 | 3 |
| 2026-10-19 | 438 / 663 | 431 / 609 | 164 | 11 | 3 |

The 2026-10-11 build set the seed after loading the game, so the events'
start times came from the clock and runs could differ. *Mangiasaur* won only
in some of them. With the seed set first, repeated runs give byte-identical
`results.tsv`, *Mangiasaur* does not win, and everything else is as on
2026-10-11.
