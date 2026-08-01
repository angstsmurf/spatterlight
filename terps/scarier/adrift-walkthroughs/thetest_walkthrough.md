# The Test — walkthrough / unwinnability analysis

- **Author:** unknown (a short, whimsical ADRIFT 4 "puzzle box"; the narrator is
  a chatty in-game voice). No published walkthrough.
- **Engine:** ADRIFT 4 (Battle System present but unused on any reachable path).
- **Result:** **UNWINNABLE — max reachable 5/25, deterministic.** The single
  victory ending exists in the data (`use key with keyhole`, task 28, in the
  final room) but is **structurally sealed** behind a *circular lock* on the very
  first door. Triaged earlier as "winnable puzzle-box, route not yet banked";
  full reverse-engineering shows it is in fact unwinnable.
- Solution file: `harness/thetest_solution.txt` (a short deterministic tour that
  reaches the 5-point cap and demonstrates the sealed door).

## Setup

Gordon, dreading a maths test, watches his teacher's watch spin backwards — the
teacher is a time lord — and is flung into a sensory-deprivation chamber:
**Room 0, "Somewhere you don't want to be."** The intended journey is

```
Room 0 (machine + colour door)  --E-->  Room 1 "The Room of Eternal Dialing"
   (telephone + Robot Guard holding a key)  --E-->  Room 2 "The Orange Room"
   (portable teleporter)  ~teleport~>  Room 3 "Morse Room" (slot)
   --E-->  Room 4 "The Empty Room"  ==  use key with keyhole  ==  WIN
```

## The 5 points you can actually get

In Room 0 you are strapped to a conveyer feeding a machine that is **allergic to
fluff**, and your shorts have a zipped **pocket** with an endless supply of it.
Feed the machine three times (it "has 3 sets of giant teeth") and it sneezes
itself to pieces, leaving a **colour-changing key** inside. That cut-scene
(`#finalfluff`, task 3) is the **only reachable `ChangeScore` action**, worth
**+5**:

```
open clothes                 <- "You unzip the pocket."  (the pocket is a part-of
                                static; the *container* is the clothes themselves)
take fluff                   <- the pocket refills after every fluff you remove
drop fluff                   <- "The fluff is dragged away into the machine."
z                            <- the machine grabs it on the FOLLOWING turn
take fluff / drop fluff / z  <- second set of teeth
take fluff / drop fluff / z  <- third: "(score has increased by 5)"
take key                     <- a key whose colour changes every turn
unlock door                  <- never works (see below); the door just recolours
east                         <- "You can't go in any direction!"  (sealed)
score                        <- 5 out of a maximum of 25
```

The fluff must be **taken out of the pocket and dropped by hand** — waiting does
nothing, because `#finalfluff`'s restriction is "fluff is *not* held by the
player" and, in ADRIFT 4, fluff sitting in a container you are *wearing* counts
as held. SCARE used to get that wrong (see `screstrs.cpp`, the "Held by" case),
which made a bare `z z z z z z` appear to solve it; that was an engine bug, not
the intended route. Verified against the real ADRIFT 3.9 Runner under Wine:
same commands, same cut-scene text, `Score: 5`.

Note the one-turn lag on the `z` lines: SCARE runs the machine's grab task on the
turn *after* the drop, whereas the Runner prints "You drop the fluff." and the
grab in the same turn. The reachable score is identical either way; the lag is a
separate, still-open task-ordering difference (`*`-wildcard / any-turn tasks) and
is unrelated to the object-location decode.

Every other point source is in Rooms 1–4, which are unreachable.

## Why the door can never open (the circular lock)

Movement Room 0 → Room 1 (the only exit) is gated, in the room-exit table, on
**task 15 `#unlockdoor` being complete**. Reverse-engineering that task:

- **`#unlockdoor` (task 15)** has restriction `robot2 == 3` (game variable 6).
  Until `robot2` equals 3, the task can't fire and the door stays shut.
- **`robot2` is written by exactly one action in the whole game** — task 16
  `#shoutrobots` (verified: the only `ChangeVariable` action targeting variable
  6). Nothing else ever touches it; it starts at 0.
- **`#shoutrobots` requires the player to be *in the same room as the Robot
  Guard*** (restriction "player in same room as NPC 0"). The Robot Guard sits in
  **Room 1** and **never moves** (confirmed over 80+ turns; its single walk is
  never triggered, and the timed event that calls `#shoutrobots`, EVENT 2, only
  runs the task if the room check passes — events do **not** bypass restrictions
  in ADRIFT).

So: the door needs `#unlockdoor` → which needs `robot2 == 3` → which needs
`#shoutrobots` → which needs you in Room 1 → which needs the door open. A closed
loop. You are trapped in Room 0 from the first turn. A 45-verb × 25-repetition
brute force of Room 0 finds no escape.

## The colour-key minigame is a red herring

The obvious-looking puzzle — match the colour-changing key to the colour-changing
door — is a **non-functional decoy**. `unlock door` (task 14) maintains a
"consecutive match" counter, `addything` (variable 4):
`addything = if(doorcolor == keycolor, addything + 1, 1)`. But **no task, event,
or exit anywhere references `addything`** (verified across all 29 tasks). It is
computed and thrown away; the door opening depends solely on the unreachable
`robot2`, never on colour matches. Inserting the key only ever recolours the
door ("…instead of unlocking, the door turns *<colour>*").

## Verdict — game data, not a SCARE bug

This is an authoring dead-end (a circular dependency), faithful to the original
ADRIFT Runner: SCARE evaluates the room-exit task gate, the variable restriction
`robot2 == 3`, and the "same room as" character restriction exactly as the
Runner's own routines do — all standard ADRIFT constructs. The win ending is real
in the file but can never be reached in any faithful interpreter. Honest maximum
is **5/25**. (The remaining 20 points — `listen` +5 in Room 0, which is itself
gated on an uncompleteable task; plus `#unlockdoor`, `enter %number%`, and
`#shoutrobots` at +5 each in Rooms 1–4 — are all behind the sealed door.)
