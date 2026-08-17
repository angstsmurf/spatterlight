# Can It Be All So Simple? — walkthrough (**reaches the ending, no score**)

- **Author:** entered in the InsideADRIFT Summer Competition 2005 (the archive
  also ships a `Tony's Note.png` feelie). A first-person horror piece: the
  narrator wakes in a dark house, finds "monsters", and kills them.
- **Engine:** **ADRIFT 4.00** (`xxd -p -l 12 games/CIBASS.taf | cut -c17-22` →
  `93453e`).
- **Result:** **reaches the game's single ending** — the interrogation-room
  coda, then the room "oblivion" and "The game has ended." There is **no score**
  and, deliberately, no victory: the twist is that the monsters were the
  narrator's family and neighbours. The author signs the last page off with
  `[Press any key to discontinue]` instead of *continue*, and that pun is what
  the suite marker greps for (`The game has ended.` is the engine's generic
  EndGame line and would not distinguish this ending from a death). Wired as
  `cibass_solution.txt|CIBASS.taf|[Press any key to discontinue]|SCR_SKIP_WAITKEY=1`.
- **Source:** the author's own `Walkthrough.txt`, bundled inside
  `SummerComp05.zip` at `SummerComp05/cibass/Walkthrough.txt`. Copy kept as
  `downloaded/CanItBeAllSoSimple_walkthrough.txt`. Followed **verbatim**, all 40
  commands.

## The waits are load-bearing

Fifteen of the forty commands are `wait`. This is not padding — the game is
largely a timed narrative, with events firing on a turn counter, and the two
long `wait` runs (after `kill monster with bat`, and again in the interrogation
room) are what advance it. Cutting them strands the run mid-scene.

Two small handling notes for anyone re-deriving this:

- The upstream file has **no trailing newline** on its last line. Fed straight
  into the harness that concatenates the final `wait` with the appended `quit`
  into a single nonsense command (`waitquit` → "I don't understand"), which then
  cascades. `goldens/cibass_solution.txt` has the newline restored.
- `SCR_SKIP_WAITKEY=1` is required, same as *The Marlin Affair*: the game's
  keypress-paged prose otherwise eats solution lines — the opening
  `wait / wait / wait / wait / stand / turn on lights` all vanished on the first
  run.

## The route

```
wait x4 / stand / turn on lights         waking up in the dark
take crowbar / south / down / south
open fridge / examine counter / take knife
north / up / knife                       bare `knife` = the parents' bedroom
down / north / west / knock on the door  the neighbour's house
east / south / up / southwest / down
north / west
unlock door with crowbar                 the crowbar's one use
take bat / kill monster with bat         the aluminum bat -- "Yes...this feels right."
wait x5                                  the phone rings; darkness
wait x5                                  the interrogation room; the man in the brown suit
```

The final exchange is the whole point:

> "In my parents' bedroom and in my neighbor's house." I reply.
>
> A look of shock and horror sweeps the man's face…
>
> "They looked like monsters to you?"

## Why it was not found earlier

Bundled inside the comp archive rather than published separately — the same
reason *Door*, *The Marlin Affair: Prologue*, *Silk Noil* and *The Wheels Must
Turn* were missed by the IFDB harvest.
