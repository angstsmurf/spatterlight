# FunHouse — walkthrough

- **Author:** unknown (a short, undated ADRIFT 4 carnival game; no published
  walkthrough on Key & Compass, IF Archive, or CASA).
- **Engine:** ADRIFT 4 (Battle System present — the carnival NPCs swing at each
  other and at you in the funhouse — but **no fight has to be won**; you only
  pass through, so no combat-assist is needed).
- **Result:** **WIN, max reachable 310/410 — deterministic.** The win is the
  scripted hand-off of a hidden cassette to the ticket man.
- Solution file: `harness/funhouse_solution.txt`. No start-up prompts.

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

Run it with `sh harness/play.sh "<…>/FunHouse.taf" harness/funhouse_solution.txt`.

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

## Why 310 is the maximum (the locked 100)

The game's stored max score is **410**, and the dump shows two *more* +100
ChangeScore tasks — `take money` (task 10) and `take drink` (task 13) — that
would account for the gap. Both, however, have their room-list set to
**NO_ROOMS** (`Where` type 0), so they can never fire from a typed command;
they can only run if some other task *executes* them. **No task in the game has
a type-5 (execute-task) action**, so neither is ever reachable — they are
orphaned, exactly the dead-task pattern seen elsewhere in this corpus. Typing
`take drink`/`take money` just invokes the generic library "take" (no points;
and the matching death task `spill drink` makes the drink a trap, not a prize).
Honest maximum is therefore **310/410**, faithful to the data and the original
Runner.

## Hazards avoided

- The funhouse NPCs (Brat Kid, Bumpy the clown, the mafia man) trade Battle
  System swings, but under seed 1234 their attacks against the player all miss
  on the banked route, so no fight is entered and the run is deterministic.
- Two type-6 *death* endings exist — `spill drink` (task 7) and `kill`
  (task 18); the route touches neither.
