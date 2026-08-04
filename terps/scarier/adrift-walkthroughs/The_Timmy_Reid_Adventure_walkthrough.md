# The Timmy Reid Adventure — walkthrough

- **Engine:** ADRIFT **3.80** (native, `tra.taf`). *The Timmy Reid Adventure
  (The Jonny Reid Adventure — Part II)*, Jonathan R. Reid, Reidville
  Adventures, 2000. 102 rooms, 248 objects, 147 tasks, 9 events.
- **Result:** **WIN**, **360 of 372** — the real ceiling (see "The twelve
  missing points"). 182 commands. Solution:
  `harness/timmy_reid_solution.txt`; golden
  `timmy_reid_solution.expected.txt`; win marker
  `Thanks for getting us back home!`.
- **Walkthrough source:** none published. Derived here from a dump of the
  parsed 3.80 game.

## The shape of the game

Timmy and his little brother Jon fall through a closet into Old Orchard Beach,
Maine, circa 1975. To go home you must fill a garbage container with **twenty
souvenirs**, sorted into seven coloured bags, the bags into three boxes, the
boxes into the container — then climb a stepladder in the road in front of the
cottage and go `up`.

The sort order is fixed by tasks 101–107 and is not hinted anywhere in play:

| bag | contents |
|---|---|
| red | thumb wrestler, skeleton key, wiffle ball |
| orange | garage key, sour grapes, usable baseball glove |
| yellow | new baseball, american flags, shovel |
| green | medical kit, soap-on-a-rope, strawberry shampoo |
| blue | white towel, push broom *(the tennis ball is added for you)* |
| purple | aba basketball, tim's key, pop-tarts |
| black | guitar, playboy magazine, incredible edibles |

then glass box = red + orange + yellow, metal box = green + blue, wooden box =
purple + black, and `put the boxes into the garbage container` (task 111, +50).
The bags and boxes never have to be picked up or even seen — every one of these
tasks is `where=ALL` and checks only that the *items* are held.

## The stepladder toggle

The ratty step ladder (Cottage Cellar, behind the panels) is the game's one
piece of state machinery.

- `open the ladder` = task 52, and it **drops the ladder into the room**.
- `climb up the ladder` = task 51, which needs task 52 done and the ladder
  held-or-here.
- While task 51 is *done*, tasks 53–56 and 59 intercept `n`/`s`/`e`/`w`/`out`
  and you are stuck. `climb down the ladder` reverses task 51 (only task 51 —
  task 52 stays done for the rest of the game, so you never type
  `open the ladder` twice; re-typing it would reverse it).
- The ABA basketball is on a rafter in the Salvation Army Gym and is only
  reachable with task 51 done, so the ladder must be carried across town and
  back.
- The winning task 112 (`up`, in the start room, +100) requires **task 51 done
  *and* the full garbage container held**. So the very last three commands are
  `put the boxes into the garbage container`, `climb up the ladder`, `up`.

## Where the twenty items come from

| item | how |
|---|---|
| thumb wrestler | `hug jon` (task 16, +25) |
| skeleton key | police-station chain, below |
| wiffle ball | `play skeeball` in Wonderland Arcade (needs loose change) → `buy the wiffle ball` (+10) |
| garage key | `ask the attendant for the key` at the DODGEM Cars |
| sour grapes | `pick some grapes` on any rr-track room (+5) |
| usable baseball glove | dropped by `play baseball` in the Ball Field |
| new baseball | hidden by `play baseball`; `pray` in the Salvation Army sanctuary moves it to the Gym |
| american flags | `take the flags` at Uncle Joe's, Odessa Ave. |
| shovel | `push the panels` in the Cottage Cellar (+10) |
| medical kit | `ring the bell for service` then `get a tattoo` in the Tattoo Parlor (+5) |
| soap-on-a-rope, shampoo | In the Shower Stall |
| white towel | `pull the clothesline` (+5) then **`get towel from clothesline`** |
| push broom | `open the maintenance closet` with the garage key |
| aba basketball | on the Gym rafter, ladder up |
| tim's key | `search for the cottage key` on the Cottage Deck (task 13, +10) — also what unlocks the kitchen door |
| pop-tarts | kitchen cupboard |
| guitar | `light the cap bomb` in the Biker Bar (+10) |
| playboy magazine | `examine the bed` upstairs, then take it |
| incredible edibles | `get the marine animals` off Googin's Rocks, then `taste the small marine animals` |

Two disambiguation traps: there are **two** baseball gloves ("a crappy" and "a
usable"), so always give the adjective; and `take towel` alone is swallowed by
task 34, which prints a one-shot failure and does nothing — the working phrase
is `get towel from clothesline`.

## The police-station detour, and the only lost points

The skeleton key is mandatory (red bag) and lives on Captain Miller's belt.
The only way into the station is to be arrested:

1. `pull my pants down` — task 19, **−2**.
2. `piss` in any public room — task 17, **−10**, teleports you to the station.
3. `get jelly donut`, `give captain miller a donut` (+5), `steal the skeleton
   key` (+10) — which throws you out into Saco Ave. & Old Orchard St.

Both penalties are unavoidable, and 2 + 10 = **the twelve points that 360/372
is missing**. Task 17 is also a free one-way teleport from the cottage end of
town to the top of Old Orchard St., so the route spends it on the trip out.

## The train, and the other ways to die

Three tasks kill:

- task 24 `eat the grapes`;
- task 29 `pet the dog` (the route uses `feed the dog` instead, which is what
  clears the Oakland Ave. side yard);
- task 73, the train. Standing on any rr-track room starts EVENT 5, which
  fires task 72 (*"You hear a faint sound of a train"*) after one turn; EVENT 6
  then fires task 73 four turns later. `jump` (task 75) saves you once task 72
  has fired. The route never spends more than three turns on the tracks, so it
  never has to.

## Route notes

- `sing` (task 4, +15) works from anywhere and **teleports you to the Band
  Shell**, so it doubles as a shortcut from the convenience store into the
  Grove/Washington Ave. leg.
- `bribe the bouncer` costs you the red sox hat permanently; the hat is not one
  of the twenty items, so this is free.
- `light the cap bomb` teleports you back to Washington Ave. (Mid) — do the
  Odessa Ave. / Uncle Joe's / Googin's Rocks leg *before* the Biker Bar or you
  walk the whole length of Grand Ave. twice.
- The donuts are `a box of plain donuts`; `take donuts` does not parse. Buy them
  (`take the box of plain donuts`, then `buy some plain donuts`, task 87) and
  `paper for reid` (task 86) in one visit — leaving with unpaid donuts triggers
  task 89 and the shopkeeper takes them back.
- Task 85 (`+5`, hand grandpa the paper and donuts) is a **system task fired by
  Hovey's walk**, not something you type. Carry both back to the cottage and it
  fires the next time Hovey shuffles into your room; the route reaches him
  during the bag-filling, so no explicit waiting is needed.

## Score accounting

62 (cottage) → 105 (yard/cellar/gym approach) → 110 (ball field, `pray`,
basketball) → 150 (sing, Wortman's, cap bomb, police) → 190 (tattoo, grapes,
apple, seagulls, wiffle ball, dodgem) → 205 (bouncer, guitar) → 260 (grandpa,
garbage container) → **360** on the winning `up`.
