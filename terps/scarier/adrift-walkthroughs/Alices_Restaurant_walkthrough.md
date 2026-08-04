# Alice's Restaurant Anti-Massacree Adventure — walkthrough

- **Engine:** ADRIFT **3.70** (native, `arlo.taf`). Laura Lee, 18-03-2000. You
  are Arlo Guthrie at Thanksgiving 1965; play out the song.
- **Result:** **WIN**, **190/190** (every one of the 30 scoring tasks) — ends
  `... recording an album that will be that hit record you've been hoping for.`
  Solution: `harness/alices_restaurant_solution.txt`; golden
  `alices_restaurant_solution.expected.txt`.
- **Walkthrough source:** none published. Derived here from a dump of the
  parsed 3.70 game (see `ADRIFT_370.md`).

## Derivation

114 tasks, 37 rooms, 9 events. Most of the tasks are scenery replies; the spine
is a chain of gated tasks, plus four **NPC walks** that do the plot beats.

**The horse.** `open cabinet` in the stable (+10, and the shovel) is blocked
while the horse is there, and nothing moves it — the horse's walk is
`StartTask = play guitar`, one stop, destination "follow the player", looping,
so playing the guitar makes it *follow* you. Lead it out (`play guitar`,
`south`), then **`stop playing`**: reversing a task whose walk is running stops
the walk (`npc_tick_npc_walk`: start task no longer done ⇒ walk cancelled), so
the horse stays in the garden. Walk back north and the cabinet is free. This is
the one genuinely non-obvious puzzle in the game.

**Getting the rake** (needed to lift the garbage) means reaching the Bell Tower,
which has no working stairs: take the brownie from Alice's Restaurant, `eat
brownie` anywhere *except* beneath the tower (task 35's room list excludes it),
then `eat half brownie` beneath the tower to float up. `ring bell` brings Alice
with a ladder, which is what makes `down` work afterwards.

**Getting into the restaurant** needs the "Closed on Thanksgiving" sign from the
Town Dump: `put sign on window` sends the Rude Customer home (it starts his
"walk to nowhere"), which is what lets `open restaurant door` past its
`npc NOT here` restriction.

**Driving.** Rooms 7/9/17/18 are "in the bus" rooms and rooms 8/19/20 their
on-foot twins; movement between them is by `drive <dir>` and `get in`/`get out
of bus`, never by compass words.

**The arrest** is a walk, not a task the player types: `put envelope under
garbage` at the cliff starts Alice's walk to the Sanctuary, and her arrival
fires `$phone call from obie`. So you must drive back and stand in the Sanctuary
for it to trigger; one more turn and Obie has you in the cell. Likewise Father
Raper's `$father raper` fires from his walk once you are on the Group W bench —
hence the `wait` at line 82.

**Fatal or dead-end commands avoided:** `kill`/`tickle`/`punch` the Rude
Customer, `not guilty` in court, `break rose window`, and naming a real crime
(`murder`, `robbery`, …) on the Group W bench. The correct answers there are
`littering` then `and creating a nuissance` (the game's spelling), and the game
is won by `ask Sargeant about paper`.
