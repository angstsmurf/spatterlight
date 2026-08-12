# A Day at the Iachini House — walkthrough

- **Engine:** ADRIFT 4.00 (`iachini.taf`, 19,083 bytes) by **Michael Iachini**,
  "Created using ADRIFT with Butcher Basic ALR", last updated August 2001.
  **27 rooms, 68 tasks, 5 events, 4 integer variables** (`coord`, `str`,
  `keynum`, `ph`) and **no NPCs at all**.
- **Result:** ★ **WON, 115 out of a maximum of 115 (100%)** — the declared
  maximum, and it is exactly reachable. 170 commands.
- **Solution:** `goldens/iachini_solution.txt` (golden blessed, plain row in
  `run_v4_walkthroughs.sh`, no env). Win marker:
  `You settle down in front of the TV.`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS` plus
  play; the game's own two-level HINT system (`hints`, then a question per
  room) confirms every chain.

## The shape

The Puzzle family are out for the afternoon and you have Mrs. Puzzle's
handwritten to-do list. `read list` spells out the whole game:

1. fix the broken step between the basement and the kitchen;
2. correct the chemical balance in the hot tub;
3. wash the afghan — and find the dryer sheets one of the kids has hidden;
4. lay and light a fire, with wood from the attic;
5. take a shower, leaving the towel on the towel bar;
6. find the hidden remote control and settle down to watch TV.

There is one ending: **T30 `turn on * television *`** in the front of the
family room, `ACT type=6 v1=0`. Its five restrictions are the whole list —
the remote held, the dry afghan held, T27 (the fire) and **T51** (the shower)
done, and `RESTR type=3 v1=0 v2=5 v3=5`, the player *sitting on* the couch.
`sit on couch` is a required command, not flavour.

The only losing move in the file is **T13 `sit *stair*`** before T9 has
repaired it.

## Route

The committed script is sectioned and commented; this is the skeleton.

| Section | Commands | What it buys |
|---|---|---|
| Ground floor | `u n n w get cleaner e n open drawer get opener open refrigerator get food eat food e turn on computer click breakout s w w get card` | glass cleaner, garage-door opener, T56 food (+5), T4 drawer (+5), T3 `click breakout` (+5) which sets `coord = 2` |
| Upstairs | `e u w nw clean mirror read card in mirror drop card drop cleaner se w open hamper get weights exercise drop weights e sw get basketball ne e d` | T21/T22 the backwards index card in Julie's mirror (+10, prints **PUSH KEY 80**), T8 `exercise` (+5) sets `str = 1` |
| Driveway & garage | `s d open door drop opener shoot hoops get screwdriver n open tool chest get hammer get tablets get discs move chest` | T0 garage door (+5), T1 hoops (+5) — needs `coord = 2` — knocks the screwdriver down; T5 tool chest (+5) needs `str` |
| Basement | `e fix stair drop screwdriver n get soap open machine put soap in machine drop soap s nw open shower get soap se u` | T9 the stair (+5); detergent loaded early, then the bar of Ivory |
| Hot tub | `e n w x meter add acid add acid add acid x meter drop tablets drop discs` | pH 10 → 7; T48 scores (+5) on the step from 8 |
| Upstairs again | `e s w s s u e open armoire get matches n open shower get shampoo s e get afghan w w w n get towel s sw move panel drop hammer u get firewood d ne e d` | T15 the attic panel (+5) needs `str`; matches, shampoo, afghan, towel, firewood |
| Fire & wash | `e n put firewood in fireplace w w look under table get sheets get newspaper e e put newspaper in fireplace w d n put afghan in machine close machine wash afghan` | T20 `look under table` (+5) is where the kid hid the dryer sheets; T16 starts EVENT 0 |
| While the washer runs | `s u e light match light fire w w s push key 80 open piano get remote e u w ne take shower sw e d n n d n` | T27 the fire (+5), T23 `push key 80` (+10), T51 the shower (+10) |
| Dryer | `open machine get afghan open dryer put afghan in dryer put sheet in dryer close dryer dry afghan z×6 open dryer get afghan` | T19 (+10) and the T38 `#dryer done` event (+10) |
| Endgame | `s u e s sit on couch score turn on tv` | WIN at 115/115 |

`keynum = 80` and the hot tub's starting `pH = 10` are set by immediate events
on turn 1, so they are stable across routes under the harness seed — the
number on the index card and the number of acid tablets do not move when the
route changes.

## The file adds up to 140, but only 115 can be scored

Twenty-one tasks carry an `ACT type=4` and they total **140**. Twenty-five of
those points cannot be collected, and the author's own declared maximum
already assumes it:

**Three showers, one towel (−20).** T51 (room 16, the upstairs bathroom),
T52 (room 17, the master bathroom) and T53 (room 16 again) are all
`take * shower *` and each awards 10. Every one of them does
`ACT type=0 … v2=0 v3=0` on obj71 — move the fluffy bath towel *nowhere* —
and puts a wet towel on a bar in its place. Nothing in the game dries or
replaces the towel, so exactly one shower is ever possible. T30 names T51
specifically, so the one you take has to be the upstairs bathroom.

T53 is a straight second copy of T51 with the same `room=16`, but it hangs its
wet towel on the *basement* bathroom's bar: the author plainly meant it for
room 24 and mistyped the room number. In practice it is unreachable — T51
matches `take shower` in room 16 first.

**The second pH award (−5).** The hot tub has six tasks: T44/T45/T46
`add * bas*` and T47/T48/T49 `add * acid*`, banded by the `ph` variable.
Starting at 10, each acid tablet is −1. T47 (pH > 8) carries you 10 → 9 → 8;
**T48 (pH = 8) is the one that scores**, landing on 7. From there the tub is
locked. `add acid` still matches **T47 first**, and T47's restriction now
fails *with a failure message* — "Adding more acid to the hot tub would move
the pH farther away from 7" — so the v4 scan **stops there** and T49 (pH ≤ 7)
is never reached. The same is true of T44 standing in front of T45 for the
base. You cannot deliberately overshoot 7 to collect the other award.

That is the v4 first-match rule doing exactly what it did in *Salutations*,
only in the other direction: a *messageless* failing restriction falls through
to the next matching task, a failing restriction *with* a message ends the
scan. See `adrift4-vs-5-restriction-eval`.

140 − 20 − 5 = **115**, which is what the game reports as its maximum.

## Two parser traps

**`put sheets in dryer` silently does the wrong thing.** T18's command is
`put * sheet * dryer`, and ADRIFT wildcards match **whole words**, so the
plural never matches the task. The command falls through to the library, which
cheerfully answers "You put the box of dryer sheets inside the clothes dryer"
— and then T19 refuses with "You don't want to run the dryer without a dryer
sheet", because T19's third restriction is `RESTR type=2 … task18`, i.e. the
*task* must have run, and putting the box in with the library take does not
run it. The singular `put sheet in dryer` is required.

**The remote is inside the closed piano.** T23 `push key 80` drops the remote
control into the piano bench/body rather than into the room, so `get remote`
straight after it answers "What do you want to take?". `open piano` has to go
between them.

## Carry capacity is real

The game enforces a limit and the messages are easy to misread as puzzle
refusals — "Your hands are full at the moment" and, for the wood, "The armload
of firewood is too heavy for you to carry at the moment". An early route
silently missed the towel, the basketball and the afghan for this reason. The
committed script drops each tool at its point of use (card, cleaner, weights,
opener, screwdriver, hammer, detergent bottle, tablets, discs) and that is
enough.

One related trap: the laundry detergent and the bar of Ivory in the basement
shower are **both** called "soap". Load the washer with the bottle first and
drop it before fetching the bar, or `get soap` turns ambiguous.
