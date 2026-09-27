# FunHouse — walkthrough

- **Author:** unknown (a short, undated ADRIFT 4 carnival game; no published
  walkthrough on Key & Compass, IF Archive, or CASA).
- **Engine:** ADRIFT 4 (Battle System present — the carnival NPCs swing at each
  other and at you in the funhouse — but **no fight has to be won**; you only
  pass through, so no combat-assist is needed).
- **Result:** **WIN, 310/410 as shipped, 410/410 with the engine's game patch
  on — deterministic.** The win is the scripted hand-off of a hidden cassette
  to the ticket man.
- Solution files: `goldens/funhouse_solution.txt` (faithful) and
  `goldens/funhouse_patched_solution.txt` (`SCR_ASSUME_PATCHES=1`). No
  start-up prompts.

The win screen:

> *thank you for bravely protecting this important information*
> **Congratulations!**

## Story / setup

You are Mr. Simmons, arriving by car at a carnival and meant to meet your kids
at the main ticket booth. The real plot is quieter: a *violent mafia man* roams
the funhouse "looking for a cassette — he knows where to look!" The cassette is
hidden inside the **kewbie doll** in the Dark Room; your job is to grab it first
and deliver it to the **ticket man**.

## Map

The funhouse is a small "scrambled mirror maze." Exits are deterministic (seed
1234) but disorienting; navigate by the commands below, not by the compass.

```
Car (start) --south--> Main ticket booth --east--> Enter Hall of Mirrors
   Hall --north--> Strobe light room --east--> Vampire lair --east--> Dark Room
   Dark Room --north--> Fun slide (diamond ring)
   return:  Dark Room --west--> Vampire lair --west--> Strobe light room
            --south--> Hall of Mirrors --west--> Main ticket booth
```

## Walkthrough (310/410, then win)

```
south                       <- Car to the Main ticket booth
take hundred dollars        <- +100
pick up money               <- +100  (an all-rooms scoring task)
east                        <- into the Hall of Mirrors
north                       <- Strobe light room
east                        <- Vampire lair
east                        <- Dark Room (the kewbie doll is here)
north                       <- Fun slide
take ring                   <- +110  (the diamond ring)
south                       <- back to the Dark Room
take kewbie doll            <- reveals the hidden cassette in this room
take cassette
west                        <- Vampire lair
west                        <- Strobe light room
south                       <- Hall of Mirrors
west                        <- Main ticket booth
score                       <- "Your score is 310 out of a maximum of 410"
give ticket man cassette    <- WIN: "Congratulations!"
```

Run it with `sh harness/play.sh "<…>/FunHouse.taf" goldens/funhouse_solution.txt`.

## How it works (structural dump)

A full task dump (31 tasks; **every task has zero restrictions** — this is a very
simple game gated only by *which room* a task runs in):

- **The cassette is hidden at start.** `take kewbie doll` (task 17) carries a
  *move-object* action targeting dynamic object 11 = the cassette, dropping it
  into the current room. So the doll *is* the reveal; there is a flavour task
  `find cassette` but it has no actions and does nothing.
- **The win** is `give ticket man cassette` (task 24, room 0) — a type-6 EndGame
  with var1=0 (victory). `drop cassette` (task 19) is a second, identical win
  ending. Both only need the cassette held; the ticket man is always at the booth.
- **Score sources (type-4 ChangeScore):** `take ring` +110 (Fun slide),
  `take hundred dollars` +100 (booth), `pick up money` +100 (all rooms) — the
  three reachable ones, summing to **310**.

## Why 310 is the maximum as shipped (the locked 100)

The game's stored max score is **410**, and the whole pool is four type-4
ChangeScore actions: `take ring` +110 and **three** +100 tasks that all pay out
for picking up the same hundred-dollar bill at the booth — `take hundred
dollars` (task 12, room 0), `pick up money` (task 11, all rooms) and `take
money` (task 10). 110 + 3×100 is exactly 410, so the author meant all three
phrasings to be typed and paid; none of them carries a restriction.

Task 10's room list is **NO_ROOMS** (`Where` type 0), so it can never fire from
a typed command; it could only run if another task *executed* it, and **no task
in the game has a type-5 (execute-task) action**. So the third hundred is
orphaned and the honest faithful ceiling is **310/410** — exactly what the
original Runner scores too. (Seven other tasks share the empty room list, task
13 `take drink` among them; none of those has any action at all, so nothing but
this one hundred is lost. `take drink` just falls to the library take, and the
matching death task `spill drink` makes the drink a trap, not a prize.)

## With the engine's game patches on — 410/410

FunHouse is in Scarier's targeted game-patch table (`PATCH_TABLE` in
`sctafpar.cpp`, applied at the end of `parse_game()` just before
`prop_solidify`, off unless `glk patches on` / `SCR_ASSUME_PATCHES=1`). The
patch matches on name and author, pins task 10's text and both its twins'
commands, and opens the one field:

```
Tasks/10/Where/Type   0 (NO_ROOMS) -> 3 (ALL_ROOMS)
```

That is task 11's own room list, one slot along, and task 10 is unrepeatable,
so the hundred is still paid exactly once. The patched row is the faithful
script plus the single command the task names:
`funhouse_patched_solution.txt|FunHouse.taf|You scored 410 out of the maximum 410!|SCR_ASSUME_PATCHES=1`,
alongside the faithful row; both are kept. The only other difference in the
golden is one turn of Battle System RNG shifting, because the run is a turn
longer.

## Hazards avoided

- The funhouse NPCs (Brat Kid, Bumpy the clown, the mafia man) trade Battle
  System swings, but under seed 1234 their attacks against the player all miss
  on the banked route, so no fight is entered and the run is deterministic.
- Two type-6 *death* endings exist — `spill drink` (task 7) and `kill`
  (task 18); the route touches neither.
