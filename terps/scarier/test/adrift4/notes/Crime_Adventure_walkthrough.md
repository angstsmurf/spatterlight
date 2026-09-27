# Crime Adventure — walkthrough (**WIN, 65/95 — and 65 is the 3.80 ceiling**)

- **Game:** *Crime Adventure* by M. Whitmore (`mwhitmore12@yahoo.co.uk`). You
  are outside a seedy arcade when a car screeches up and someone bundles Mrs
  Fenwick out of the phone booth. Find her.
- **Engine:** **ADRIFT 3.80** (`xxd -l 16 games/Crime_Adventure.taf` →
  `… 94 45 36 61 …`). 36 rooms, 29 objects, **23 tasks**, 2 NPCs, 3 events.
- **Result:** **WIN**, ending on *"You stand on the chair and see that Mrs
  Fenwick is there safe and sound. Well there you have it. Mrs Fenwick was in no
  danger at all, it was a friend who picked her up at the booth (she was in a
  rush)."*
- **Score: 65 out of 95**, in **90 commands** — and **65 is the real maximum in
  a 3.8 Runner**, not a missed puzzle. The other 30 points are all gated behind
  the arcade cash, which ADRIFT 3.8 will not let you pick up (see
  [§2](#2-the-cash-cannot-be-picked-up-in-38--and-that-costs-30-points)). A
  3.90/4.00 Runner has no such gate and would score the full 95: **this is a
  version divergence, not an author bug**, so there is nothing here for the
  engine's patch table to repair.
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
each thing twice**. In this 3.80 file only the **stew** half of that pattern
ever pays: the shoes are bought with the arcade cash, and §2 below shows the
cash can never be held, so tasks 14 *and* 15 both answer *"You don't have enough
money"* however many times you type them. The route still types
wear/remove/wear, because that is what the measured run380 transcript does and
the refusals are its record of the gate.

```
wear golf shoes      <- task 14 would fire, 0 points, spending task 14
remove shoes
wear golf shoes      <- task 15 would fire, +10   (neither fires in 3.8)
```

The stew is the same shape, with a twist: task 12's own action drops the
saucepan on the dining-room floor, so it has to be picked back up first.

```
give food to mr fenwick    <- task 12, +10, and Mr Fenwick hands over the putter
drop golf ball             <- (burden; see below)
get saucepan
give stew to mr fenwick    <- task 17, +10, and he hands over the putter again
```

**A player who types the stew `give` once finishes the game at 55/95** and has
no way of knowing what the missing 10 was for. Measured 2026-09-27 by running
the route with the second `give stew to mr fenwick` removed: **35** at the
`score` turn instead of 45, and 55 at the ending.

### 2. The cash cannot be picked up in 3.8 — and that costs 30 points

Task 19 (`get cash` in the arcade, +5) has exactly one action — add 5 points. It
prints

> You grab the £30.00 from the machine

and **leaves the cash inside the casino**. The +5 is banked all the same, since
the task does not care whether the object moves. Taking it for real is another
matter, and in a 3.80 file it is impossible:

* the task is non-repeatable, so a second `get cash` falls through to the
  library take;
* the cash (obj 26) sits **inside the casino machine** (obj 25), a *dynamic*
  container, and pre-3.9 refuses a take out of a dynamic container the player is
  not holding or wearing (`run380 insides()` @446CAB/446CFB — the jb2000 row);
* the casino is class 4 = burden **7** against this game's MaxCarried **5**, so
  it can never be held, empty hands or not.

So the second `get cash` answers *"You are not holding a casino."*, and the cash
is unreachable. With it die every task that carries the held-cash restriction
(`RESTR type=0 obj26=[cash] v2=1`) — tasks 13/14/15/16, the shoe purchase and
both `wear golf shoes` copies — and task 20 `putt golf ball`, which needs the
shoes worn:

| Dead in 3.8 | Points | What the game answers |
| --- | --- | --- |
| `buy shoes` (task 16) | 5 | *"You don't have enough money"* |
| `wear golf shoes` (task 15) | 10 | *"You don't have enough money"* |
| `putt golf ball` (task 20) | 15 | *"You aren't wearing the right clothing."* |

**30 points dead, 65/95 the true maximum.** Every one of those refusals is
verbatim run380 text, measured on this very game (`Adven_1_crime.rtf`,
2026-09-04: the whole solution echoed, zero engine differences).

The win survives it. `putt golf ball` is *supposed* to open the way down, but the
underground passage is reachable anyway — `down` from the back yard answers *"You
move down. In a underground passage."* — so the chair, the ceiling and the
ending are all still banked. Only the points are lost.

### 3. ADRIFT 3.8's pooled burden model is tight enough to block the route

`Crime_Adventure.taf` is a version 3.80 file, so carrying is governed by the
3.8 pooled burden: **limit 5**, and the putter alone costs **3** (everything
else portable in this game costs 1). Worn items count.

That makes putter + golf ball + worn golf shoes = **exactly 5** — the budget the
author was writing to, even though the shoes are never actually worn here — and
the route still has to be planned around the limit in two places:

* the route's `drop cash` in the kitchen is a **leftover from the 3.9 reading**
  of this game, where the cash really is carried and would have to be put down
  before the putter arrives. In 3.8 it answers *"You don't have a penny!"* and
  costs nothing; it is kept because the measured transcript has it;
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

## The route, and where the 65 points are

| Where | Command | Points |
| --- | --- | --- |
| Restaurant | `break cookie` | 0 (hint) |
| Master bedroom | `read diary` | 0 (hint) |
| Spare bedroom | `open dresser`, `get penny`, `get golf ball` | 0 |
| Arcade | `use penny on machine` | **10** |
| Arcade | `get cash` (the second one refuses: *"You are not holding a casino."*) | **5** |
| Shoe Store | `buy shoes` | 0 — *dead in 3.8, worth 5 in 3.9+* |
| Shoe Store | `wear golf shoes` / `remove shoes` / `wear golf shoes` | 0 — *dead in 3.8, worth 10 in 3.9+* |
| Kitchen | four ingredients into the saucepan, `switch on cooker`, `wait`, `switch off cooker` | 0 |
| Dining room | `give food to mr fenwick` (→ putter) | **10** |
| Dining room | `get saucepan`, `give stew to mr fenwick` | **10** |
| Back yard | `putt golf ball` | 0 — *dead in 3.8, worth 15 in 3.9+; `down` works without it* |
| Room, below | `move chair under ceiling` | **10** |
| Room, below | `stand on chair` | **20** → WIN |

Total **65** in 3.8, 95 under a 3.90/4.00 Runner. The ending prints no score
line, so the route runs `score` on the turn before `stand on chair`; the golden
records **45/95** there and the last 20 arrive with the win.

The solution file is kept **exactly as measured** — byte-identical to
`~/adrift-battle/runner/wine/cmdfile_w_crime.txt` — so the commands the 3.8 gate
kills (`get cash` #2, `buy shoes`, `wear golf shoes` ×2, `remove shoes`,
`drop cash`, `putt golf ball`) are left in deliberately: they are the row's
record of the divergence. Do not "tidy" them away.

## Reproducing

```sh
cd terps/scarier/test/adrift4/harness
sh run_v4_walkthroughs.sh crime_adventure     # PASS against the committed golden
```
