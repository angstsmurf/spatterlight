# Archetype regression test — "The Gorreven Papers"

A full, deterministic winning playthrough of Derek T. Jones' *The Gorreven
Papers* (the larger of the two demo games shipped with Archetype 1.01),
used as an end-to-end regression test for the interpreter port.

## Files

- `gorreven_walkthrough.txt` — player input, one command per line. **Blank
  lines are meaningful**: they are the "wait" command (the game treats a bare
  RETURN as waiting), and several are timing-critical (the cell-guard alert
  cycle, the elevator-arrival delays). The whole run is deterministic because
  the interpreter never calls `srand()`, so `?`-randomness is a fixed sequence.
  Do not reorder or remove lines without re-checking the guard/elevator timing.
- `gorreven_expected.txt` — the byte-exact transcript the walkthrough must
  produce, ending on the win ("…I have saved the free world… again").

## Running

```
make -f Makefile.headless regress GORREVEN=/path/to/GORREVEN.ACX
```

The story file is not vendored; point `GORREVEN` at a copy of `GORREVEN.ACX`
from the Archetype 1.01 distribution. The target builds `archetype_hl`
(CheapGlk-backed, stdio), replays the walkthrough, and diffs against the
ground truth, also asserting the win marker is present.

## What it exercises / why it exists

Building this walkthrough surfaced three latent interpreter bugs, all now
fixed (see `parser.cpp` / `interpreter.cpp`):

1. **`parse_sentence_next_chunk` offset** — a chunk offset was computed
   relative to a substring but used as an absolute index, so `examine cot`
   (and any examine of a hollow object) looped forever.
2. **`convert_to` boolean→numeric** — the `case 'B'` converted the operand in
   place but then `break`'d into `return false`, so every `attr = TRUE` /
   `attr = FALSE` comparison mis-compared. This broke worn-state tracking,
   hence the central disguise puzzle.
3. **`parse_sentence_substitute` marker length** — a hard-coded `+4` assumed
   the original fixed-width object marker; the port emits variable-length
   decimal markers, so chunks following a 1- or 3-digit object number desynced
   (e.g. `put poison in food`, poison being object 116).

Ground truth for these was confirmed against the original DOS reference
interpreter (`PERFORM.EXE`) under DOSBox.
