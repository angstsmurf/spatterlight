# Far From Home — walkthrough

- **Engine:** ADRIFT 3.90 (`FarFromHome.taf`, 42,118 bytes), by **The Mad
  Monk**, 23 July 2002. You grabbed a small spiral object out of the back of
  your refrigerator, the room went wavy, and you woke at the foot of a
  mountain. **25 rooms, 59 objects, 40 tasks, 3 NPCs, 4 events**, one variable
  (`ropestatus`) that nothing ever writes to.
- **Result:** ★ **WON, 50/50** — fourteen `+3` tasks plus the `+8` for
  answering the Puzzlelord's riddle. There are no negative scores and no
  scoring task off the route.
- **Solution:** `goldens/farfromhome_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `you realize something. This is your home!`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS` +
  `SCR_DUMP_OBJLOC`.

## Route

```
x                                      <- keypress: the intro pauses BEFORE
Sam                                       the name prompt (see below)
mountain     e
cabin        open wood box / take axe / down
cellar       take bucket / up
cabin        w
mountain     s
forest       chop down tree (+3) / take shovel
             s
field        dig (+3)                  -> opens the way down into the cave
             d
cave         dip bucket in river (+3)
             u / n / n
mountain     dump water into hole (+3) -> teleports you to the castle
castle gate  look behind tapestry (+3) / open door (+3)
             n / w
throne room  look throne (+3) / press button (+3)
             e / s / s / sw
beanstalk    climb beanstalk (+3)      -> teleports you into the ocean
x                                      <- keypress: the descent pauses
             w / n
behind l/h   open seashell / take pearl
             s / in / u / u / u
4th floor    give pearl to pirate (+3) -> the rope; do this FIRST
             take key                  (out of the sheet on the bed)
             d / out
balcony      tie rope to bar (+3)      -> opens the whole outside ladder
             d / d / d / w
west beach   climb tree / take coconut / d / take coconut
             e / u
outside 1st  hit window with coconut (+3) / open window
             d / s / in
1st floor    take box / unlock box (+3) / open box (+3)
                                       -> teleports you to the last chapter
             in
Puzzlelord   ask man about riddle / score / lost (+8, EndGame win)
```

## Two `<waitkey>` pauses eat a script line

`SCR_MARK_WAITKEY=1` finds exactly two, and both matter for a scripted run:

1. **In the introduction, before `Please enter your name:`.** So the first
   line of the solution file is a bare keypress and the *second* line is the
   name. Answer `Sam` on line one and the game addresses you as `look` for the
   rest of the story — the Puzzlelord greets you by name (*"Hello, Sam. Yes, I
   do know your name. I like to know my victims."*).
2. **Inside the beanstalk descent**, between *"you hear yourself screaming as
   you fall through the air"* and *"You wake bobbing about in the middle of an
   ocean."*

Both are why `farfromhome_solution.txt` contains two lines reading `x` that
are not commands.

## Three one-way chapters

Nothing survives the transitions, and each one is an `ACT type=1` **player**
move — so the room number is **0-based**, unlike the 1-based NPC moves:

| Task | Command | Takes you to | Destroys |
|---|---|---|---|
| 6 | `dump water into hole` | room 6, the castle entrance | axe, shovel, bucket, water |
| 12 | `climb beanstalk` | room 11, the ocean | the castle key |
| 24 | `open box` | room 22, the mysterious forest | — |

So the mountain, the cabin, the cellar, the cave, the castle and the clouds
are each visited once and shut behind you. There is nothing to come back for
— the author clears the inventory himself at every seam.

## The tool chain

Chapter one is a single dependency line, and every tool falls out of the
previous puzzle:

```
open wood box -> take axe        (the axe is INSIDE the box; `take axe`
                                  alone gets "Take what?")
  -> chop down tree   (forest)   -> the shovel drops on the forest floor
  -> dig              (field)    -> opens EXIT room=2 D -> 5
  -> dip bucket in river (cave)  (the bucket is on the bed in the cellar)
  -> dump water into hole (mountain)
```

`chop down tree` exists three times: **T2** in the forest is the real one;
**T34** at the mountain and **T35** in the field are failure messages for
chopping the wrong tree. All three want the axe held, so the refusal is a
sentence rather than a parser error and it is easy to think the axe is broken.

## The pirate walks away

NPC 0 starts on the lighthouse's 4th floor with a four-step walk whose
`stopTask` is T16 itself — so he only stands still once you have paid him.
He is in the room on the turn this route arrives and gone two turns later.
**`give pearl to pirate` has to be the first command on that floor**;
anything else first gets *"The pirate is not here!"*, and without his rope
the balcony is a dead end and the rest of the island is unreachable.

The pearl comes from the blue seashell behind the lighthouse: `open seashell`,
`take pearl`.

## The sheet on the bed

T12 (`climb beanstalk`) puts obj44 `[sheet]` **onto surface index 0**, which
is obj7 `[bed]` — and that bed is a multi-room static present in **rooms 4, 8
and 17**. So the sheet, with the bronze box key inside it, appears on the 4th
floor of the lighthouse at exactly the moment you need it, having been
"dropped" onto a bed you last saw in the cabin cellar two chapters ago.

`open sheet` is refused (*"You can't open the ruffled sheet!"*) — it is
already open, so `take key` works directly.

## The rope opens five exits at once

T17 `tie rope to bar` on the third-floor balcony is the gate on `13 U`,
`18 D`, `19 U`, `19 D`, `20 U` and `20 D` — the whole outside-the-lighthouse
ladder from the balcony down to the beach. Before it is tied, `out` from the
third floor leads to a balcony with no way off it.

## The coconut round trip

`take coconut` up the tree does not give you a coconut: T21 moves obj47 to
room 24, the Western Beach. You climb back down and pick it up there. Then,
outside the first floor:

- `hit window with coconut` (T20, +3) — `hit window` alone (T19) is a refusal
  gated on T18 *not* being done;
- `open window` (T18, restricted on T20) puts the gold-laced box **inside**
  the lighthouse, in room 14, so the last leg is back down the rope, round the
  building and in through the front to fetch it.

## Five ways to die

All `ACT type=6 v1=2`, none of them on the route:

| Task | Command | Where |
|---|---|---|
| 5 | `drink water` | anywhere, once the bucket is full |
| 11 | `jump` | on the clouds |
| 15 | `jump` | lighthouse 3rd floor |
| 32 | `hit man` | the Puzzlelord |
| 26 | — | `EVENT 3 [#riddle]` fires it **10 turns** after `ask man about …` |

The riddle timer is the only clock in the game; the other three events are
`#weather`, `#weather2` and `#weather3`, pure atmosphere on 3- and 4-turn
cycles (the breeze in the grass, the tide on the island).

## The riddle

> 50 is my first / nothing is my second / a snake will make my third / then
> three parts a cross is reckoned.

L (Roman fifty) + O (nothing) + S (a snake) + T (three parts of a cross) =
**LOST**. T27 holds fourteen wrong answers the author anticipated — *fear,
strength, sloth, panic, ghosts, anger, ignorance, pain, cold, death, monsters,
satan, ice, dark* — and T28 answers the spelling-out (`50 fang cross`)
without winning. Only T29 `lost` scores.

## Notes

- **The lighthouse keeper is scenery.** The 2nd floor holds his body with a
  knife in its back and a trail of dried blood; T14 `take knife` exists and
  does nothing, and the murder is never mentioned again.
- **The Man (NPC 1) and the Puzzlelord (NPC 2) both start in room 23** and
  are the same character described twice — the floating figure is the object
  `crown`/`guards` scenery plus an NPC that never moves.
- **`ropestatus`** is declared and never assigned; no `ACT type=3` exists in
  the file.
- **The ending is the joke.** You land on your rear end in your own garden,
  and the clock says you have been gone ten minutes.
