# Albridge Manor — walkthrough

- **Engine:** ADRIFT 3.9 (`manor.taf`, 31,353 bytes), by **Woody Ross**,
  Feb–July 2002. *"An interactive haunted house."* 27 rooms, 56 objects, 30
  tasks, 3 NPCs (Angie, Theo, a ghost), 4 events.
- **Result:** ★ **WON, 50/50** — the sum of every `ACT type=4` in the file.
  There are no negative ones.
- **Solution:** `goldens/manor_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `You bury the crucifix with the other items.`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS` +
  `SCR_DUMP_OBJLOC`.

## Route

```
Sam / male                            <- the two opening prompts, not commands
take matches                          (Foyer, on the end table)
n / w / open cupboard / take cat food (Kitchen)
e / e / take candelabra                (Dining Room)
w / n / n / n / n / take shovel        (Garden, through the Solarium)
s / s / s / s / s / u                  (back to the Foyer, up to the Balcony)
w / n / e / search shelf / take key    (Closet)
w / w / search bed / take necklace     (Master Bedroom)
e / n / search clothes / take ball     (Spare Bedroom — Angie screams)
s / s / e / e / n / e                  (Child's Bedroom)
give food to cat                       (+5)
search vanity / take doll
w / n / u / search junk / take crucifix (Attic)
d / s / s / w / d                      (back down to the Foyer)
unlock door                            (+5)
in / take pipe / out                   (Secret Alcove)
light candelabra                       (+5)
n / w / d / move rack / s              (Kitchen → Wine Cellar → Secret Room)
bury doll / bury necklace / bury pipe / bury ball   (+5 each)
score
bury crucifix                          (+15, EndGame win)
```

## Scoring — all 50

| Task | Command | Points |
|---|---|---|
| 14 | `light candelabra` | +5 |
| 15 | `unlock door` | +5 |
| 18 | `give food to cat` | +5 |
| 22 | `bury doll` | +5 |
| 23 | `bury necklace` | +5 |
| 24 | `bury pipe` | +5 |
| 25 | `bury ball` | +5 |
| 26 | `bury crucifix` (EndGame win) | +15 |
| | **total** | **50** |

## The shape of the game

Four ghost-children's keepsakes and a crucifix have to be buried in the
Secret Room under the wine cellar. **T26 carries six restrictions** —
crucifix held, shovel held, and T22/T23/T24/T25 all done — so the crucifix
goes in last, and the other four can go in any order.

The keepsakes come from four `search` tasks. Each one has an `ACT type=0`
whose room number is **1-based**, i.e. it drops the item into the room you are
standing in; the item then has to be picked up separately:

| Task | Where | Reveals |
|---|---|---|
| 10 `search junk` | Attic (23) | crucifix |
| 11 `search shelf` | Closet (24) | old brass key |
| 19 `search vanity` | Child's Bedroom (21) | doll — **needs T18 done** |
| 20 `search bed` | Master Bedroom (20) | necklace |
| 21 `search clothes` | Spare Bedroom (25) | ball |

The **pipe** is the exception: it is not searched for. It sits on the table
in the Secret Alcove, behind the panelling under the stairs, and
`EXIT room=0 IN -> 13` is `gateTask=15 wantDone=1` — you need the brass key
from the upstairs closet before you can reach it.

## Two order constraints

- **Take the cat food before you go upstairs.** T18 `give food to cat` is
  `where=1 room=21` and needs the tin from the kitchen cupboard in hand, and
  T19 `search vanity` needs T18 done (*"You search the vanity now that the cat
  is busy"*). Forget the tin and it is a full round trip. The route empties
  the whole ground floor — matches, cat food, candelabra, shovel — before
  climbing the stairs once.
- **The key has to come back down.** Everything else upstairs is optional
  until you have it; the Secret Alcove and therefore the pipe are unreachable
  without it.

`move rack` in the Wine Cellar (T1) opens `EXIT room=14 S -> 26`
(`gateTask=1 wantDone=1`) and is the only way into the Secret Room.

## Three instant deaths

All `ACT type=6 v1=2`, none of them on the route, none of them signposted:

| Task | Command | Where |
|---|---|---|
| 7 | `jump` | anywhere the balcony railing is in scope |
| 9 | `jump window` | Child's Bedroom, **only while the window is open** (`RESTR type=1 v1=3 v2=0`) |
| 12 | `jump window` | Spare Bedroom |
| 29 | `kill ghost` | anywhere the ghost is present |

The ghost of a little girl starts in the Solarium (`NPC 2 startRoom=6`), which
the route walks through twice on the way to the Garden and back. She is not
hostile; attacking her is.

## Notes

- **Nothing is timed.** All four events are `texts=S--` with `affTask=0` —
  Whispers, Door, Storm and Ball are pure atmosphere on random intervals. The
  Ball event carries `pauseTask=23` (task 22, `bury doll`), so the ghostly
  bouncing stops once you start the burial.
- **The ball costs you Angie.** T21 `search clothes` also fires `ACT type=1
  v1=2 v3=0` — NPC 0, Angie, to nowhere. A ghost boy in Victorian clothes
  appears, yells *"That's mine! No fair! I'll get you for that!"*, and a
  second later you hear a scream from the foyer. She is not in the ending.
  Neither is Theo (`NPC 1 startRoom=-1` — he is never anywhere).
- **The game asks your name and gender**, and the name comes back at you in
  the whispers: *"You suddenly feel a chill in the air, and hear a whispery
  voice say 'Sam!'."* The solution file answers `Sam` / `male`; changing
  either would churn the golden.
- **`light candelabra` puts it down.** T14's actions destroy the matches and
  the unlit candelabra and place a *lit* candelabra (obj51) in the current
  room — so the light source stays wherever you struck the match. It is worth
  +5 and nothing else; no room in the game is actually dark.
- **The cellphone is scenery with a joke attached.** Theo's phone lies in the
  middle of the foyer floor; T2 `use phone` exists and, as in the
  introduction, gets no signal.
- **No `<waitkey>` anywhere** (`SCR_MARK_WAITKEY=1`).
- **The ending is bleak.** You escape, but *"no house is found"* and you
  spend the rest of your days in a mental institution, haunted nightly by
  Theo and Angie. *"At least you made it."*
