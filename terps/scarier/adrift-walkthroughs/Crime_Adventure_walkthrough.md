# Crime Adventure — walkthrough (**WIN, 95/95 — full score**)

- **Game:** *Crime Adventure* by M. Whitmore (`mwhitmore12@yahoo.co.uk`). You
  are outside a seedy arcade when a car screeches up and someone bundles Mrs
  Fenwick out of the phone booth. Find her.
- **Engine:** **ADRIFT 3.80** (`xxd -l 16 games/Crime_Adventure.taf` →
  `… 94 45 36 61 …`). 36 rooms, 29 objects, **23 tasks**, 2 NPCs, 3 events.
- **Result:** **WIN**, ending on *"You stand on the chair and see that Mrs
  Fenwick is there safe and sound. Well there you have it. Mrs Fenwick was in no
  danger at all, it was a friend who picked her up at the booth (she was in a
  rush)."*
- **Score: 95 out of 95 — every point in the game**, in **90 commands**.
- **Harness row:**
  `crime_adventure_solution.txt|Crime_Adventure.taf|Mrs Fenwick was in no danger at all, it was a friend`
  (no env), PASSing golden.
- **Source:** `downloaded/CrimeAdventure_walkthrough.sol` — 29 lines of prose by
  "sasi", **for an earlier build of the game**.

## The .sol describes a game that no longer exists

Most of what it tells you to do is simply not in this `.taf`:

| The .sol says | This build |
| --- | --- |
| *"Read computer in IBM → stew recipe"* | there is no IBM room and no computer; the recipe is the **cookery book** lying in the Fenwick kitchen |
| *"Read Fenwick note & Dig ground with shovel → coin"* | there is no note and no `dig` task; the **penny is in the spare-bedroom dresser**, along with the golf ball |
| *"Pick lock with hairpin"* | `pick lock with hairpin` is not understood — the underground door just **opens** |
| *"Examine dresser → cash"* | the dresser holds the *penny*; the **cash** is £30 won out of the arcade's casino machine |
| *extras: pay the gypsy a penny; get her painting and she throws you out; hit the arcade machines twice and you get thrown out* | none of these exist. The gypsy says *"A penny for my thoughts.. What do you want to know?"* and then answers nothing; you can walk off with her painting; `hit casino` gives *"nothing happens"* however often you try |

Only the fourth "extra" survives: `east` or `west` in the north–south road
room called *"In the middle of a street"* (the one between the two parking lots
and the Fenwick house) is instant death — *"You got hit by a Car!! Don't play in
the street. I'm afraid you are dead!"* The similarly named *"North-south
street"* further west is harmless.

**Unused in this build:** shovel, hairpin, fortune cookie (hint only), hat,
picture, diary (hint only), painting, mirror, advertisement, kettle, phone,
flag, arcade-token dispenser, and both NPCs' conversation. The whole west half
of the map — beauty salon, gypsy's house, driveway, sidewalks — is scenery.

## What actually has to be re-derived: three authoring quirks

### 1. Two scoring tasks are shadowed by unscored duplicates

The author wrote each of these puzzles twice, and the **unscored** copy sorts
first, so ADRIFT runs it and the scored one never fires:

| Shadowing task | Shadowed task |
| --- | --- |
| 14 `wear *shoes*` — 0 points | 15 `wear *golf* shoes` — **10 points** |
| 12 `give *food* to mr fenwick` (alt `give *stew* to…`) — 10 points | 17 `give *stew* to mr fenwick` — **10 points** |

`*` matches any words, so `wear golf shoes` matches task 14 too, and task 14
wins on order. Both tasks are non-repeatable, though — so the fix is to **do
each thing twice**:

```
wear golf shoes      <- task 14 fires, 0 points, task 14 is now spent
remove shoes
wear golf shoes      <- task 15 fires, +10
```

The stew is the same shape, with a twist: task 12's own action drops the
saucepan on the dining-room floor, so it has to be picked back up first.

```
give food to mr fenwick    <- task 12, +10, and Mr Fenwick hands over the putter
drop golf ball             <- (burden; see below)
get saucepan
give stew to mr fenwick    <- task 17, +10, and he hands over the putter again
```

**A player who types each command once finishes the game at 75/95** and has no
way of knowing what the missing 20 were for. Verified by running the route with
the duplicates removed: 55 at the chair, 75 at the ending.

### 2. The scoring `get cash` does not actually give you the cash

Task 19 (`get cash` in the arcade, +5) has exactly one action — add 5 points. It
prints

> You grab the £30.00 from the machine

and leaves the cash inside the casino. Since the task is non-repeatable, a
second `get cash` falls through to the library and really takes it (`get money`
works too). And the cash **is** load-bearing: it is the held-object restriction
on both `buy shoes` (task 16) and `wear golf shoes` (task 15).

### 3. ADRIFT 3.8's pooled burden model is tight enough to block the route

`Crime_Adventure.taf` is a version 3.80 file, so carrying is governed by the
3.8 pooled burden: **limit 5**, and the putter alone costs **3** (everything
else portable in this game costs 1). Worn items count.

That makes putter + golf ball + worn golf shoes = **exactly 5**, and the route
has to be planned around it in two places:

* the **cash** must be dropped (the route does it in the kitchen) before the
  saucepan is carried to the dining room — otherwise the putter arrives and
  there is no room to pick the saucepan back up for the second `give`;
* the **golf ball** must be dropped before `get saucepan`, and the **putter and
  ball** both dropped before `get chair` in the final room.

Get it wrong and the answer is *"Your hands are full at the moment."*

One corroboration in the other direction: MaxCarried is **5**, and the stew
needs exactly five things (carrots, onions, potatoes, meat, saucepan — all
class 0, cost 1). `get kettle` (class 2, cost 7) answers *"Your hands are
full."*, and the cookery book would be a sixth. So the .sol's *"Get all the
stuff in Fenwick kitchen. Make stew."* comes out exactly right under the
normalised model: everything the recipe names fits, and nothing else does.

*(Note for whoever is working on the 3.8 burden model: this route was derived
and blessed against a working tree that has that model in flight —
`obj_uses_burden_model()`, `V380_BURDEN_COST[] = {1,3,7,3,7}`, reported here as
`burdenmodel=1 maxburden=5`. The golden is therefore sensitive to it. If the
per-class costs move, this row and the two other V380 rows will need re-blessing
together.)*

## The two clues the game does give you

Neither is needed for a point, but without them the central puzzle — putting a
golf ball into a hole in the back garden — is unguessable, so the route collects
both.

* **Mrs Fenwick's diary** (master bedroom): *"Today our phone went out of order
  so I'll have to take a trip down to the booth and call for a repairman. I'am
  making stew tonight."* — which is why she was at the booth, and what to cook.
* **The fortune cookie** (restaurant, `break cookie`): *"Mrs Fenwick is
  underground somewhere. Try looking somewhere with a golf green. Putt the golf
  ball. You will then be in a underground world"*.

The cookery book in the kitchen is the recipe: *"To stew - Find saucepan, get
carrots, onions, potatoes, meat and put on low boil"*. The cooker never has to
be opened or loaded — `switch on cooker` starts the event wherever the saucepan
is, one `wait` finishes the stew (*"The oven has finnished making the stew"*),
and `switch off cooker` is the task the two `give` tasks actually check for.

## The route, and where the 95 points are

| Where | Command | Points |
| --- | --- | --- |
| Restaurant | `break cookie` | 0 (hint) |
| Master bedroom | `read diary` | 0 (hint) |
| Spare bedroom | `open dresser`, `get penny`, `get golf ball` | 0 |
| Arcade | `use penny on machine` | **10** |
| Arcade | `get cash` (then `get cash` again to really take it) | **5** |
| Shoe Store | `buy shoes` | **5** |
| Shoe Store | `wear golf shoes` / `remove shoes` / `wear golf shoes` | **10** |
| Kitchen | four ingredients into the saucepan, `switch on cooker`, `wait`, `switch off cooker` | 0 |
| Dining room | `give food to mr fenwick` (→ putter) | **10** |
| Dining room | `get saucepan`, `give stew to mr fenwick` | **10** |
| Back yard | `putt golf ball` (opens the hole) | **15** |
| Room, below | `move chair under ceiling` | **10** |
| Room, below | `stand on chair` | **20** → WIN |

Total **95**. The ending prints no score line, so the route runs `score` on the
turn before `stand on chair`; the golden records **75/95** there and the last 20
arrive with the win.

## Reproducing

```sh
cd terps/scarier/adrift-walkthroughs/harness
sh run_v4_walkthroughs.sh crime_adventure     # PASS against the committed golden
```
