# Archetype regression tests

Full, deterministic winning playthroughs of the two demo games shipped with
Archetype 1.01 (Derek T. Jones), used as end-to-end regression tests for the
interpreter port:

- **The Gorreven Papers** (`gorreven_*`) — the larger game; see below.
- **Starship Solitaire** (`starship_*`) — see "Starship Solitaire" section.
- **Bare Bones** (`bare_*`) — the tutorial demo; see "Bare Bones" section.
- **Meta commands** (`bare_meta_*`) — transcript / undo / undo-after-death; see
  "Meta commands" section.

Run them with:

```
make -f Makefile.headless regress            GORREVEN=/path/to/GORREVEN.ACX
make -f Makefile.headless regress-starship   STARSHIP=/path/to/STARSHIP.ACX
make -f Makefile.headless regress-bare       BARE=/path/to/BARE.ACX
make -f Makefile.headless regress-meta       BARE=/path/to/BARE.ACX
make -f Makefile.headless regress-all         # all four (uses ~/Downloads/arch101 defaults)
```

Note: the winning playthroughs now end by answering the engine's end-of-game
menu (`Would you like to UNDO, RESTART or QUIT?`) with `quit` — see "Meta
commands" for why that menu exists.

### Determinism

The `?` operator (Archetype's random-number / random-element operator) draws
from the shared `erkyrath_random()` in `terps/common_utils/randomness.c`, the
same RNG used by the Scott, TaylorMade, Plus and Comprehend ports. `initialize()`
seeds it with the fixed value `1234` whenever determinism is requested — the
user's testing-mode theme option (`gli_determinism`) under Spatterlight, and
unconditionally in the headless harness — so every `?`-dependent event (the
Gorreven cell-guard alert cycle and elevators, the Starship combination lock)
replays as a fixed sequence on every host. The seed is shared across the ports,
so the exact sequence can change if `randomness.c` ever changes; re-tune the
timing-critical walkthrough lines and the combination if that happens.

## The Gorreven Papers

## Files

- `gorreven_walkthrough.txt` — player input, one command per line. **Blank
  lines are meaningful**: they are the "wait" command (the game treats a bare
  RETURN as waiting), and several are timing-critical (the cell-guard alert
  cycle, the elevator-arrival delays). The whole run is deterministic because
  `?`-randomness draws from the shared `erkyrath_random()` seeded with a fixed
  value (see "Determinism" above), so it is a fixed sequence on every host. Do
  not reorder or remove lines without re-checking the guard/elevator timing.
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
  commands. Fully deterministic: `?`-randomness draws from the shared
  `erkyrath_random()` seeded with a fixed value (see "Determinism" above), so the combination
  lock's random digits (`?9 & ?9 & ...`) are a fixed sequence — the diary page
  reads `39-73-89` every run, which is hard-coded into the walkthrough.
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

# Bare Bones

`BARE.ACX` is the tutorial demo from the Archetype distribution (source
`BARE.ACH`, "A bare-bones adventure written in Archetype for demonstration
purposes", Derek T. Jones). Unlike the other two it has **no win condition** —
it exists to show off language features — so the walkthrough is a guided tour
rather than a solution, and "success" is simply completing the run with a clean
quit.

## Files

- `bare_walkthrough.txt` — player input, one command per line (33 commands).
  Fully deterministic.
- `bare_expected.txt` — the byte-exact transcript, ending on the quit message
  (`Goodbye; thanks for playing.`), which the target asserts is present.

## What the walkthrough exercises

Three rooms and (almost) every feature the demo was written to illustrate:

1. **Start room** — the gated `down` exit (`I can't go that way.` until the
   trapdoor is pushed open), the stationary `trapdoor` with its open/closed
   `look` variants and `get` guard.
2. **Bedroom** — `furniture` you can `enter` (the brass bed), the `openable`
   type (closet / chest / drawers, each with the open/closed `look`), the
   `plural`/indefinite naming (`some books`, `a set of dresser drawers`), the
   default `look` fallback (`Nothing strikes me as unusual about…`), and the
   custom `'put...in'` lexicon verb across three of its branches —
   stationary subject (`I can't move the brass bed.`), closed container
   (`…isn't open.`), and success.
3. **Basement** — falling through the open trapdoor (the ASCII-art `FIRSTDESC`),
   and the `after`-event rats whose `curious` counter escalates every turn the
   player is present and **kills the player on turn 4** — so the escape (`get
   rope`, `throw rope up`, `up`) is timing-critical and must not gain extra
   turns in the basement.

## Verb note

BARE's lexicon is the stock `standard` set: examining is `look at <obj>`
(`examine` / `x` / `l <obj>` are not defined and report
`I can't find a verb in that sentence.`).

# Meta commands

The Archetype stories define no `undo`, `restart` or `transcript` verbs, so the
interpreter adds them itself. Player input is intercepted in `readLine()` before
it reaches the story's parser:

- **`transcript on` / `script`**, **`transcript off` / `unscript`** — toggle a
  Glk transcript stream echoing the session to a file. The real (Spatterlight)
  build asks for the file with a Glk prompt; the headless build honors
  `$ARCHETYPE_TRANSCRIPT` (default `archetype_transcript.txt`) so tests stay
  non-interactive.
- **`undo`** — revert the previous turn. Before each real command the dynamic
  game state is snapshotted (the same `save_game_state` serialization used by
  the `save`/`load` verbs, to an in-memory buffer); `undo` reloads it.
- After the story ends with a `stop` (a death, or any non-`quit` ending) the
  engine shows **`Would you like to UNDO, RESTART or QUIT?`**. UNDO returns to
  just before the fatal command and play continues; RESTART returns to the
  opening position. (The `quit` verb sets a flag so it exits straight away
  rather than re-prompting.)

## Files

- `bare_meta_walkthrough.txt` / `bare_meta_expected.txt` — a BARE session that
  toggles a transcript, does an in-play `undo` (un-pushes the trapdoor), then
  walks into the rat pit, **dies**, picks `undo` from the end-of-game menu, and
  goes on to escape with the rope. Run with `make -f Makefile.headless
  regress-meta`.

## Two implementation hazards this surfaced

1. **Dangling parser words.** `MENTION` does `create pronoun_object named it`
   and registers the new object's word with the parser the first time anything
   is referred to — but `save_game_state` does **not** serialize the parser
   vocabulary. Reloading a snapshot disposes that pronoun object while its word
   still sits in `object_names`, so `find_object` would hand back a dead index
   and the interpreter would abort (`cannot reference object N`). Fixed by
   `prune_parse_lists()` (parser.cpp), called after every restore, which drops
   parse-words whose object index is past the end of the restored object list.
2. **Replaying START needs a pristine object set.** Resuming after death means
   re-running the story's `START` (its command loop lives inside it). Re-running
   its one-time INITIAL/ASSEMBLE setup over the dynamic objects a played game
   has accumulated spins the vocabulary-assembly loop forever, so `interpret()`
   first restores a *pristine* (freshly-loaded, static-only) snapshot, then lets
   `readLine()` reinstate the real snapshot at the first prompt with output
   silenced in between. Mid-play `undo` doesn't have this problem (the loop is
   still running) and just restores in place.
