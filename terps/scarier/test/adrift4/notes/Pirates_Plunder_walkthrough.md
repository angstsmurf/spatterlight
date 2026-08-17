# Pirate's Plunder! — walkthrough (**WIN, 10/10 — full score**)

- **Author:** "Tiberius Thingamus", written for the 2010 Summer Comp. Six rooms
  on Loot Island: Ye Olde Pirate Ship, Ye Beach, Ye Deadly Marshes, Ye Olde
  Ruins, Ye Cliffside, Ye Treasure Beach. Everything is narrated in mock
  pirate-speak ("Ye swaggereth east").
- **Engine:** **ADRIFT 4.00** (`xxd -p -l 12 games/plunder_gargoyle.taf | cut -c17-22`
  → `93453e`).
- **Result:** **WON, 10 of 10** — "FINAL SCORE: REAL PIRATE! / Ye scored 10 out
  of the maximum 10!". Wired as
  `plunder_gargoyle_solution.txt|plunder_gargoyle.taf|Ye scored 10 out of the maximum 10!`,
  no env at all.
- **Source: none external.** The game is one of the four unwired v4 corpus games
  that carry an in-game hint menu, and the only source used here is that menu.

## Where the route came from

`SCR_DUMP_TASKS=1 harness/scare games/plunder_gargoyle.taf` prints every task's
`HINTQ`/`HINT1`/`HINT2`. Pirate's Plunder! fills all three tiers for nine
questions, and the tier-2 ("sledgehammer") answers are literal commands:

| # | HINTQ | HINT2 |
| --- | --- | --- |
| 1 | How do I get through the deadly marshes? | Cut the brambles. |
| 2 | How do I get past the snake? | Cut or fight the snake. |
| 3 | How do I get through the crack in the old ruins? | Ichabod can fit. Call Ichabod and then tell ichabod to go north. |
| 4 | How do I get down the cliffside? | Tie the vine to the hook and put the grappling hook in the tree. |
| 5 | How do I find the treasure on Treasure Beach? | Search the sandy patches and then dig. |
| 6 | How do I get the treasure up the cliff? | Tie the dangling rope to the chest, then go up and pull the rope. |
| 7 | How do I defeat Captain Hookhead? | Push the cannon off your ship to the cliffside and then shoot the ghost ship. |
| 8 | How do I uncurse the dubloons? | Bring the chest to the ruins, open it, and say ye magic word. |
| 9 | What is the magic word? | Say "please". |

That is nearly the whole game. Three things the hints leave out:

- **`pull rope` needs Ichabod at the *bottom*.** With the chest tied on and the
  player on the cliff it answers "Ye'll never haul that treasure up the cliff
  this waye, me matey! Ye'll have to findeth some waye, thing, or person to
  steady it from ye bottom." `call ichabod` on Ye Treasure Beach fetches him
  down the rope — and is also what triggers Captain Hookhead's ghost ship
  appearing offshore, so the cannon leg cannot be done any earlier.
- **The cannon moves one room per turn** (`push cannon east` etc., TASK 60–85),
  ship → beach → marsh → ruins → cliff, and the marsh→ruins leg only exists
  after `cut brambles` has opened the east path. The chest goes back the same
  way, ruins → marsh → beach → ship, with `push treasure <dir>` (TASK 33–54);
  both need Ichabod present, which he is once he has climbed up with you.
- **The snake never appears on this route.** It only shows up if you wander the
  marsh instead of cutting the brambles on the first turn there (the game
  counts `marsh_moves`), so hint 2 is dead code for a straight-line walkthrough.

## The trap

The riddle scroll says "Through ye marshes navigate / But toward ye end don't
celebrate!" — that is literal. `TASK 10 cmd=[* celebrate *]` is live from the
start. The route never types it.

## The route

`goldens/plunder_gargoyle_solution.txt`, 43 lines.

```
read scroll / x cannon                    the riddle; the cannon is pushable
e / get shovel / x footprints             Ye Beach
s / cut brambles                          opens the marsh's east path
e / x totem / read totem / x crack        the totem states the uncursing rite
call ichabod / tell ichabod to go north   he squeezes through and drops the hook
get hook / e                              Ye Cliffside
x viney tree / get vine / tie vine to hook
put grappling hook in tree                now `d` is safe
d / search sandy patches / dig            the X marks it; chest is too heavy
call ichabod                              he steadies it -- and Hookhead arrives
tie rope to chest / u / pull rope         chest hoisted to the cliff
w / w / n / w                             back to the ship for the cannon
push cannon east / south / east / east    ship -> beach -> marsh -> ruins -> cliff
shoot ghost ship                          Captain Hookhead, blown to smithereens
push treasure west / open chest / please   the dubloons uncurse in front of the totem
x chest
push treasure west / north / west          chest back aboard
set sail                                   -- WIN, 10/10
```

The ending pointedly notes you left the cannon on the island; that costs
nothing, the score is already maximum.
