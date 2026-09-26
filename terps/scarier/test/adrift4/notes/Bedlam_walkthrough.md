# Bedlam — walkthrough (**UNWINNABLE as authored**)

- **Author:** Wild N Mild. Compiled 2 Dec 2001 (`SCR_DEBUGGER_ENABLED=1`
  `game`: `Game "BEDLAM" Compiled "02 Dec 2001", Author "Wild N Mild"`).
- **Engine:** **ADRIFT 3.90**, second person, battle system enabled, 11
  rooms, 31 objects, 41 tasks. Reads as a short, unfinished preview build
  (roughly a quarter of a larger intended game — the intro text calls it a
  demo).
- **Result:** **UNWINNABLE**, same class of bug as `Sandy.taf` (see
  `sandy_solution.txt`'s row/comment). Confirmed with `SCR_DUMP_TASKS=1`:
  Task 37 (`ask barbara about keys`) narrates the NPC handing over the car
  keys, but its action list is empty — it contains **zero `ACT` lines** — so
  object 30 ("your car keys") never actually moves into the player's
  inventory. `SCR_DEBUGGER_ENABLED=1`'s `objects 29 30` confirms object 30
  stays `Not seen` / `Hidden` for the whole game. Task 38 (`start car`), the
  only path to the game's one win ending (`ACT type=6 v1=0`), requires
  holding that object (`RESTR type=0 v1=18 v2=1 v3=0 obj30=[keys]`), which
  can now never be satisfied — a straightforward missing-action authoring
  bug in this preview build, not a puzzle. In-game, `get keys` / `x keys`
  confirm the parser doesn't even recognize "keys" as an object ("Take
  what?" / "Nothing special."). Wired as
  `bedlam_solution.txt|bedlam.taf|These type of vehicles usually require keys to operate`,
  no env.

## The walkthrough

```
look at dresser
get alarm clock
look under bed
hit clock with bat
stand
s
s
open cabinet
x cabinet
get bottle of pills
n
e
n
n
get magnifying glass
s
s
open curtains
burn cord with glass
burn cord with glass
ask barbara about keys
n
se
open washing machine
x washing machine
get clothes
wear clothes
open car
sit on car
start car
```

Plays through every reachable room and object in the demo — the bedroom
(dresser, alarm clock you smash with a baseball bat rather than turning off,
looking under the bed), the bathroom (medicine cabinet, pill bottle), a hall
room with a magnifying glass, back to the bedroom's window to burn through
the phone cord tying the curtains (twice — the first burn merely "starts a
small fire", the second actually severs it), then talks to the NPC Barbara
about the keys she never actually hands over, and finally the garage (washing
machine, a change of clothes) and the car itself, ending at the genuine dead
end: sitting in the car and trying to start it with no keys, which is as far
as this preview build's content goes.

## With the engine's game patches on

Bedlam is one of the four games in Scarier's targeted game-patch table
(`PATCH_TABLE` in `sctafpar.cpp`, applied at the end of `parse_game()` just
before `prop_solidify`, off unless `glk patches on` / `SCR_ASSUME_PATCHES=1`).
The patch matches on the game's name and author *and* on Task 37 still
carrying the command `ask barbara about keys`, then gives that task the one
action its author left out:

```
Tasks/37/Actions/0  Type=0 (move object) Var1=18 Var2=4 Var3=0
```

i.e. a move-object action whose Var1=18 selects dynamic object 15 (the move
handler takes Var1−3), which is object 30, "your car keys"; Var2=4 is "held
by" and Var3=0 is the player. Nothing is inserted or removed anywhere else,
so every task, object and variable keeps its index and old saved games still
load.

With that in place Barbara really does hand the keys over and the preview can
be finished. One wrinkle in the route: `start car` has to be typed **twice**.
The game has two tasks with that command, and task 31 — the "these type of
vehicles usually require keys" message, restricted to sitting in the car —
matches first. It is not repeatable, so the second `start car` falls through
to task 38, the ending. Wired as
`bedlam_patched_solution.txt|bedlam.taf|You have just completed the Bedlam preview|SCR_ASSUME_PATCHES=1`,
alongside the faithful row above; both are kept.
