# The Marlin Affair: Prologue — walkthrough (**WIN, no score**)

- **Author:** "Lumin" (lumin_orb@myway.com), for the InsideADRIFT Summer
  Competition 2008. You are June Starr, a rookie intergalactic spy, drugged and
  handcuffed in a sabotaged shuttle that is falling into a sun.
- **Engine:** **ADRIFT 4.00**
  (`xxd -p -l 12 games/marlin_affair.taf | cut -c17-22` → `93453e`).
- **Result:** **WON** — the distress signal goes out, salvagers pick June up,
  and the game closes on "Just how good of a liar are you? Find out in… / The
  Marlin Affair: Chapter One / coming soon!" There is **no score**, so the
  marker is that sequel title. Wired as
  `marlin_affair_solution.txt|marlin_affair.taf|The Marlin Affair: Chapter One|SCR_SKIP_WAITKEY=1`.
- **Source:** the author's own `june_walkthrough.txt`, bundled inside
  `SummerCompGames08.zip` at `games/junedocs/june_walkthrough.txt` (upstream the
  game file is `junepro.taf`; the corpus name is `marlin_affair.taf`). Copy kept
  as `downloaded/MarlinAffairPrologue_walkthrough.txt`. Followed **verbatim** —
  all 40 commands after the spoiler-space banner, in order, unchanged.

## Env: SCR_SKIP_WAITKEY is load-bearing here

The game pages its prose with keypress prompts — the long backstory intro, and
several of the set pieces later on. Without `SCR_SKIP_WAITKEY=1` each of those
prompts eats a line of the solution file. Four went missing on the first run
here: `look` and `x cabinet` from the intro, then `x forcefield`, and — the one
that actually breaks the route — the `s` after `turn off generator`. Losing a
movement desynchronises everything downstream, and the run then fails in a way
that reads like a walkthrough bug but is not:

```
> unscrew bolt with screwench
I don't understand what you want me to do with the bolts.
```

The give-away that the run was desynchronised rather than the walkthrough being
wrong is the last command: `plug unit into center` answered with the "focus on
either halting or changing your course" refusal, i.e. the engine was still
running. With the env var set, `unscrew bolt with screwench` instead plays the
four-bolt set piece and the engine grinds to a halt, and the same final command
wins. **Always check for eaten commands by grepping the transcript's `> ` echoes
against the solution file before concluding a bundled walkthrough is broken.**

## The shape of it

Three decks — Back of Shuttle (where you wake), Front of Shuttle, and the
Engine Room below — plus a forcefielded compartment.

```
look / x cabinet / say open              voice-activated cabinet
get tools / kick tools / get spinner
turn on spinner / cut handcuffs          free at last
get screwench / n
x door / x panel / open panel with screwench / x panel
unplug wire                              door opens
n / x forcefield / x generator / x logo  the forcefield generator
open hatch / d / n / drop all / n / u
turn off generator                       forcefield down
s / d / get all / u / n                  retrieve everything
x gloves / get gloves                    hazard-proof, in the pilot's chair pouch
d / wear gloves
unscrew bolt with screwench              four bolts; the engine stops
u / x communications center
get unit from generator                  the forcefield generator's power cell
plug unit into center                    distress signal -- WIN
```

The two halves of the endgame are deliberately separate problems: the engine has
to be *stopped* (or the shuttle falls into the sun) **and** the comms need
*power* (the only cell aboard is the one in the forcefield generator, which is
why the generator has to be dealt with first). Calling for help before stopping
the engine is explicitly refused — there is no one close enough to reach you in
time.

## Why it was not found earlier

Bundled in the comp archive rather than published to IFDB, exactly like *Door*,
*Silk Noil* and *The Wheels Must Turn*.
