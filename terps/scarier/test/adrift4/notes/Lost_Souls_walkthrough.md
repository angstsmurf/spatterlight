# Lost Souls — walkthrough

- **Engine:** ADRIFT 3.9 (`lostsouls.taf`, 16,695 bytes). A WWI-ish ghost
  story: you crawl off the battlefield into an abandoned house — Kitchen,
  Thin Hallway, Bathroom, Bedroom, Attic, and *The Door* at the bottom of the
  hatch. 6 rooms, 32 objects, 24 tasks, no NPCs, no events.
- **Result:** ★ **WON.** There is no scoring system at all — not one
  `ACT type=4` in the file, and `score` answers *"My score is 0 out of a
  maximum of 0"* — so the finish line is TASK 22's `ACT type=6 v1=0`.
- **Solution:** `goldens/lost_souls_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `I sat still. "You don't want to go down there."`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS`.

## Route

```
n / e
take note / read note / take plug
w / u
open desk / read notebook
use plug on hole                 (opens the Bedroom's U exit)
u
search body / take sticker       (the key is under a sticker)
take rifle
unlock trunk                     (NOT `open trunk` — see below)
read scrap of paper              (this is what unlocks `d` in the Kitchen)
d / d / s
d                                (hatch; <waitkey> pause)
<blank>
open door                        (EndGame win)
```

## `open trunk` is a trap: two tasks share the pattern

The whole game hinges on one accident in the Attic:

```
TASK 16  room=4  restr=0  rep=1   cmd=[open trunk]  ALT=[open * trunk]
TASK 17  room=4  restr=1  rep=0   cmd=[open trunk]
         ALT=[open the trunk] [unlock trunk] [open * trunk]
             [unlock * trunk] [use key * trunk] [unlock * trunk * key]
         RESTR type=0 v1=6  (holding the key)
         ACT  type=0 v1=7 v2=4    (scrap of paper -> inventory)
```

TASK 16 has no restrictions and is repeatable, so it claims **every**
`open … trunk` phrasing for the whole game and answers

> I gave the lid a tug, but it's locked.

even with the key in your pocket. TASK 17 is only reachable through the four
alternatives TASK 16 does *not* carry — `unlock trunk`, `unlock the trunk`,
`use key on trunk`, `unlock trunk with key`. `unlock trunk` is the shortest.

That is not an engine divergence: the first task in list order whose pattern
matches and whose restrictions pass claims the command, and TASK 16 always
qualifies. It is the author writing the "it's locked" refusal *above* the
real task instead of below it, and then forgetting that `open the trunk`
(TASK 17's own alternative #1) also matches TASK 16's wildcard `open * trunk`.
A player who never types the word "unlock" cannot finish the game.

Everything downstream depends on it:

| | |
|---|---|
| TASK 17 | gives object 28, *a scrap of paper* |
| TASK 18 `read scrap * paper` | `RESTR type=0 v1=7` — needs the paper held |
| TASK 2 `d` / `open hatch` (Kitchen) | `RESTR type=2 v1=19` — needs TASK 18 **done** |
| TASK 22 `open * door` (room 5) | the file's only `ACT type=6` |

So the paper is a single point of failure with no alternative source.

## The rest of the map

Only one exit in the game is gated: `EXIT room=3 U -> 4 gateTask=12`, i.e.
TASK 12 `use plug on hole`. The plug is the rubber sink plug in the Bathroom;
the hole is in the Bedroom ceiling panel. TASK 12 has **no restrictions** —
it does not check that you hold the plug and it does not need TASK 14
(`open panel`) first, so `use plug on hole` works the moment you walk in. The
route takes the plug anyway because the message ("I reached up and stuck the
rubber plug into the hole") is a lie otherwise.

Getting into room 5 is not an exit at all: TASK 2's `ACT type=1 v3=5` teleports
you there, and `EXIT room=5 U -> 0` is the way back.

## Notes

- **The key is under a sticker on the trunk.** Nothing in the Attic
  description mentions stickers; you have to `x trunk` ("A bunch of stickers
  are stuck to it") and then `take sticker`, which is TASK 23 —
  `ACT type=0 v1=6 v2=4`, moving the key straight to your inventory. There is
  no separate `take key`.
- **`take note` and `take plug` are the Bathroom's whole content**, and both
  are `rep=0` one-shots with `ACT type=0`. The note ("If you are reading this,
  then you will be dead soon. Please pray to god.") is pure atmosphere —
  TASK 6 `pray` exists, has no restrictions and no actions, and does nothing.
- **One `<waitkey>` inside the route**, located with `SCR_MARK_WAITKEY=1`: the
  hatch descent, *"Trembling in fear, I started my descent…"*. Without the
  blank line answering it, the following `open door` is swallowed and the
  transcript stops dead at the Door's prompt — which reads exactly like an
  unwinnable game and is worth remembering. `SCR_SKIP_WAITKEY=1` confirms the
  diagnosis (the route wins with no blank line at all under it). Three more
  pauses sit inside the ending text, after the last command, and are satisfied
  by EOF.
- **The Kitchen has an alternate description** (`ALT room=0 alt=1 type=0
  v2=19`, i.e. keyed on TASK 18): once you have read the scrap of paper,
  Stevens is dead in his chair and the handgun that was missing from his
  holster is on the floor. `take * gun` (TASK 20) and `search body` (TASK 21)
  are flavour refusals — *"I've seen enough death on the battlefield. No one
  else has to die."* — and the gun is never usable.
- **The ending is a reveal, not an escape.** You open the basement door,
  the scene cuts to the relief soldiers finding Stevens' body, and *you* are
  the thing in the basement holding the hatch shut with your foot.
