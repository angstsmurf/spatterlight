# Cyber Warp — walkthrough

- **Engine:** ADRIFT 4 (Battle System present in the data but never used for a
  reachable point — the two "fights" are scripted task kills).
- **Result:** **Completed at full 150/150 (100%), deterministic.** Note the
  game's *only* ending is its tragic stop ("…it closes! OH NO!!!! THE END, or is
  it? Better luck next time.") — a type-6 EndGame with `var1=1` (a lose-style
  stop, not a `var1=0` win). There is **no win-type ending in the data**, so
  reaching maximum score *and* the single ending is the best possible outcome.
- Solution file: `harness/cyber_solution.txt` (no name/gender prompts).

## Scope (why 150 is the maximum)

A full structural dump (5 tasks, 12 rooms) shows exactly **three `ChangeScore`
actions, +50 each** — `use teleporter` (T0), `enter warp` (T1), `use fireball`
(T2) — and exactly **one `EndGame` action**, the `var1=1` stop on
`enter back through warp` (T4). `bring lightning home` (T3) carries no actions
at all; it exists only to set its own done-flag, which **gates** T4
(T4 has a task-state restriction requiring T3 complete). So 150 is the cap and
the warp-stop is the lone exit.

## The win (full 150/150)

```
s                     <- from the Coffee House into the Teleporter Room
open teleporter       <- REQUIRED: the teleporter is an openable object and T0's
                         restriction needs it OPEN ("use teleporter" otherwise
                         just says "You walk into the wall (ouch)!")
use teleporter        <- +50; teleports you to the BluDeath Mountains booth
out                   <- BluDeath Mountains
w                     <- Cave (a "Cyber Warp" portal is at the back)
enter warp            <- +50; you fall through to Bisstonia, but the surge sucks
                         Lightning in — "you must find him"
ne                    <- Back Alley
in                    <- Kitchen
e                     <- Dining Area
out                   <- Main Street (overlord's HQ to the north)
n                     <- Large Building (two guards)
use fireball          <- +50; kills the guards and carries you up to the Lobby,
                         where Lightning is
bring lightning home  <- T3: arms the final warp (no score, but required)
s                     <- Large Building
s                     <- Main Street
in                    <- Dining Area
e                     <- Kitchen
out                   <- Back Alley
sw                    <- Bisstonia (the Back Alley's exit reads SW in play even
                         though the exit table indexes it SE — labels are rotated)
enter back through warp  <- the ending (score now 150/150)
```

## Notes / gotchas

- **Open the teleporter first.** The single biggest snag: `use teleporter`
  matches the task but silently fails its object-state restriction until the
  teleporter (a static, openable object) is open.
- **Rotated compass labels.** As in several games in this corpus, the Back
  Alley's return exit prints as `southwest` in the room text while the exit
  table stores it under the SE slot. Navigate by the game's own "you can move…"
  line, not the dumped direction.
- **The ending is a lose-style stop by design.** The author shipped only the
  tragic outcome (Lightning is left behind as the warp closes). There is no
  alternative win ending in the task data; maximum score + this stop is the
  complete game.
