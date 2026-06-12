# Comprehend headless walkthrough tests

A small text regression harness for the Comprehend interpreter, modelled on
`terps/geas/test`. It drives the existing headless build (`comprehend_hl`) from a
command script and checks the transcript for a **win marker** -- a line the game
only prints once the script has driven it to that point. This is far more robust
than diffing a byte-exact golden transcript: it pins the behaviour that matters
(the game reaches a known state) without breaking on incidental wording changes.

The games are **not** in this repo (copyrighted); point the runner at your own
copies. The command scripts in `scripts/` are the "raw" form geas also commits:
one input per line, `#`-comments and blank lines ignored.

## Build

The harness uses the headless interpreter, built from the parent directory:

```sh
make -C ../.. -f Makefile.headless      # produces ../../comprehend_hl
```

## Run

One game:

```sh
./run_walkthrough.sh covetedmirror \
    "~/Downloads/comprehend games/The Coveted Mirror 2.0 (woz-a-day collection)/The Coveted Mirror 2.0 side B (boot).woz" \
    scripts/covetedmirror.txt \
    "My horoscope was horrible."
```

Exit status is 0 if the marker is seen, else 1 (and the transcript tail is
printed to stderr so a desync is easy to locate).

The whole table:

```sh
./run_walkthroughs.sh "~/Downloads/comprehend games"
```

## A note on timers

The interpreter has no real-time clock here, but turn timers still tick: the
Coveted Mirror hourglass runs down per turn and, when it expires, the jailer
teleports you back to the throne -- so a full Coveted Mirror walkthrough only
completes if the script bribes the jailer on a strict cadence (the geas runner
solves the same class of problem with `--tick`). The committed
`scripts/covetedmirror.txt` is therefore a **timeout-free smoke prefix**: it
plays out of the cell, through the town, and solves the astrologer's
constellation puzzle -- exercising string-bank decoding, the parser, NPC
dialogue and the throne-room override -- all well before the sand runs out.

## Extending

To add a game, write `scripts/<gameid>.txt` (drive it to a distinctive line),
add a `play` entry to `run_walkthroughs.sh`, and pick that line as the marker.
Full-completion scripts for OO-Topos, Crimson Crown, Talisman and Transylvania
are still to be authored -- their prose walkthroughs need route counts pinned
down and, where a game is timed or has random hazards, the cadence worked out.
