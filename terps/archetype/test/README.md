# Archetype regression tests

Full, deterministic winning playthroughs of the two demo games shipped with
Archetype 1.01 (Derek T. Jones), used as end-to-end regression tests for the
interpreter port:

- **The Gorreven Papers** (`gorreven_*`) — the larger game; see below.
- **Starship Solitaire** (`starship_*`) — see "Starship Solitaire" section.

Run them with:

```
make -f Makefile.headless regress            GORREVEN=/path/to/GORREVEN.ACX
make -f Makefile.headless regress-starship   STARSHIP=/path/to/STARSHIP.ACX
make -f Makefile.headless regress-all         # both (uses ~/Downloads/arch101 defaults)
```

## The Gorreven Papers

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

# Starship Solitaire

A full, deterministic winning playthrough of Derek T. Jones' *Starship
Solitaire*, the other demo game in Archetype 1.01. You wake alone from
cryogenic suspension and must (the objective is hidden) restore the ship: get
the engines running and pilot it back on course.

## Files

- `starship_walkthrough.txt` — player input, one command per line, 140
  commands. Fully deterministic: the port never calls `srand()`, so the
  combination lock's random digits (`?9 & ?9 & ...`) are a fixed sequence — the
  diary page reads `58-63-59` every run, which is hard-coded into the
  walkthrough.
- `starship_expected.txt` — the byte-exact transcript, ending on the win
  ("…You have succeeded!… Congratulations!").

## What the walkthrough does (high level)

The game is a logistics puzzle gated by a strict carry **capacity** (9 size
units; the lead coveralls alone are 5, the sheet metal 4). There is no global
turn limit — only the scuba air counts down, and only once the airlock is
evacuated — so the route makes several elevator trips and stages items rather
than hoarding them. The spine:

1. Escape the coffin (open cover → reveals latch → open latch → open cover →
   get out), decontaminate to **unlock the elevator**.
2. Level 5: read the diary page for the locker combination, drink the muscle
   regenerator (raises capacity 3 → 9), open the locker for the lead coveralls.
3. Start the engines (level 4): power the **gravlifter** with the blaster's
   antimatter cell, use it to drop the (radioactive — wear coveralls) uranium
   into the reactor, close it, push the green button.
4. Stage the space gear in the antechamber: fuel/kick/start the air compressor
   to fill the scuba tanks, patch the spacesuit, clean the helmet (Windex then
   towel), put the sunglasses on **before** the helmet (so they end up inside
   it), build an arc welder from the blaster + calculator battery.
5. Space-walk out the airlock, climb the hull, enter the bridge through the
   gaping hole, **weld the hole shut** (sunglasses prevent the welding flash
   blinding you), restore life support, then sit at the pilot's console with
   the engines running.

## Why it exists / the bug it surfaced

`parser.cpp isWordChar()` had mis-transcribed the original Pascal `Word_Chars`
set: the original includes `chr(39)` (the apostrophe) so that possessive object
names stay intact, but the port instead listed `'"'` (`chr(34)`, the double
quote). With the apostrophe treated as a word separator, input `pilot's
console` normalized to the words `pilot` `s` `console`, none of which matched
the vocabulary entry `pilot's console` generated from the object's
description — and bare `console` resolves to the *computer* console. The game
is **unwinnable** without this fix, because the win is triggered only by
sitting at the pilot's console. Fixed to match the Pascal set
(`alnum || '-' || '\''`); the Gorreven regression is unaffected.
