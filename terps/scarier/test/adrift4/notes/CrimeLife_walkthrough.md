# Crime Life — walkthrough (**definitive death ending, no "win" exists**)

- **Author:** Skypig. Compiled 27 May 2002 (`SCR_DEBUGGER_ENABLED=1` `game`
  command: `Game "Crime Life" Compiled "27 May 2002", Author "Skypig"`).
- **Engine:** **ADRIFT 4.00**, second person, 39 rooms, 84 objects, 12 NPCs,
  161 tasks.
- **Content review:** dark-comedy crime sim, no sexual content anywhere. Two
  NPCs, "Punk Kid" and "Old Woman", are legitimate targets in the game's own
  design, but the walkthrough deliberately never interacts with either of
  them — every action it performs targets an adult NPC (Landlord, Mofo).
- **Result:** the game has **no winning ending**. `SCR_DUMP_TASKS=1` shows
  all 21 implemented `ACT type=6` endings are `v1=3` ("you're dead") — this
  is the game's intentional nihilistic framing ("There is no real object, no
  winning... maybe you come out on top or maybe you continue to feed of the
  bottom"), not a bug. The walkthrough instead plays a representative slice
  of the crime-life sim and ends on one of its many quotable deaths. Wired as
  `crimelife_solution.txt|crimelife.taf|You just got waxed by a punk gangsta`,
  no env.

## The walkthrough

```
Petter
male
e
flush the toilet
get the pistol
get the clip
w
n
shoot landlord
take rent box
take cash
e
u
w
n
take the picture
s
e
d
d
d
w
attack mofo
```

Flushing the apartment's clogged toilet reveals a pistol hidden inside it
(+100) and taking the accompanying 9mm clip (+10) arms it — no explicit
"load" step exists; holding both objects together is enough. Heading out into
the hallway, the landlord is waiting to collect rent; shooting him (+100)
lets you loot his rent box for $90. A short detour up to the 6th floor's
"Crime Scene" — a neighbor's room, recently the site of an unrelated murder,
police tape still up — nets a keepsake picture off the wall (+10) and reveals
a hole behind it (unexplored; not needed for this walkthrough's purpose).
Heading back down to the 3rd floor finds Mofo, a fixed-location drug-dealer
NPC; attacking him bare-handed (no bottle/knife/gun required for this
particular response) has him draw first, ending the game instantly:

> You take a swing at Mofo. He responds by whipping out a gun and blowing
> your head off. You just got waxed by a punk gangsta. You are dead.

The game does not print a restart/quit prompt after this — the interpreter
simply halts, matching ADRIFT's pre-4.0-style unconditional-ending behavior.

## Why not more of the map

The building/street map is 39 rooms with dozens of one-shot and repeatable
scoring tasks (buying/selling drugs and guns, robbing further NPCs, pawning
stolen goods) and no score ceiling — several combat tasks can be repeated
indefinitely against roaming NPCs for unlimited points. Since there is no
win condition to chase and no finite score to max out, a full map crawl would
only pad the transcript without demonstrating anything new; this walkthrough
instead shows one clean scoring loop (toilet → landlord → loot) plus one
scenery beat (the Crime Scene) before reaching a clean, deterministic ending.
