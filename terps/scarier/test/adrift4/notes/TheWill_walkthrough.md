# The Will — walkthrough (**best reachable state**)

- **Engine:** ADRIFT 3.90 Release 19 (`ambrosine@mindspring.com`, 2001).
  A 150-point treasure hunt: the player's late uncle's fenced yard leads
  into the house (hallway/foyer/gallery/living room/dining room/kitchen and
  an upstairs), and separately into a woods area (shack, hill, tunnels)
  behind the yard. Confirmed via `SCR_DUMP_TASKS`/`SCR_DUMP_OBJLOC` to be a
  large game (~50 rooms, 200+ tasks) with several two-step consumable and
  cross-room puzzles.
- **Result:** best-reachable partial, **35/150**. The route opens the front
  gate, gets into the house, scores the newel statue and the gallery window,
  finds the matchbook and uses it to fix the shack's leaning cabinet, then
  detours to the woods for the emerald egg. Wired as
  `thewill_solution.txt|The_Will.taf||`, no env, no win marker (the script
  ends mid-hunt, not at a scripted conclusion).
- **The intro's `<waitkey>`** ("Press any key when ready to play.") eats the
  solution's first line; line 1 is blank to absorb it (confirmed with
  `SCR_MARK_WAITKEY=1`).
- **Front Gate is locked** and needs two one-shot consumables, in order:
  `unlock gate` (breaks the entrance key off in the lock) then `oil gate`
  (empties the oilcan, found via `search hedges` at the Northwest Corner of
  Yard) then `open gate`. Skipping either leaves the hinges "too rusty" or
  the lock un-turned. The open gate's exit is **up**, not the compass
  direction the room text implies.
- **The Inside Shack's cabinet** leans and its drawer won't open until it is
  leveled. The lever is the matchbook found by `move pillows` in the Living
  Room (east of the Foyer) — carry it back to the shack and `level cabinet
  with matchbook`, then `open drawer`.
- **Left unsolved:** the crowbar (on the shack's cabinet), the pocket watch
  (inside the Hallway's grandfather clock) and the battery charger (in the
  cabinet drawer) all answer plain `get`/`take X from Y` with "What do you
  want to take?"/"You can't do that." `SCR_TRACE_FLAGS=256` shows the
  crowbar and watch never even reach a task's restriction check (a genuine
  parser-level refusal, not a scripted one); no lever, tool, or verb tried
  freed any of the three. The clock's own restriction chain gates a
  matchbook-style take on an integer variable ("hour") that nothing found
  on this route sets.
- **Content note:** the route never reaches any content requiring a check;
  no minors appear anywhere in the game text searched.

## The walkthrough

```
w
n
search hedges
s
e
unlock gate
oil gate
open gate
u
n
open door
n
n
move statue
n
open window
s
e
move pillows
w
s
s
s
d
w
s
s
w
level cabinet with matchbook
open drawer
e
e
e
climb tree
shake branches
d
get emerald
```
