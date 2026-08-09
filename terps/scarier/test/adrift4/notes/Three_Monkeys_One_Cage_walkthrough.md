# Three Monkeys, One Cage — walkthrough (**WIN, 98/100 — the ceiling**)

- **Game:** *Three Monkeys, One Cage* (Robert Goodwin, 2003) — you wake up in a
  zoo cage with a chimpanzee and a mandrill, watched by an audience of humans,
  and have to prove a hypothesis about tool use by escaping.
- **Engine:** **ADRIFT 4.00** (`xxd -l 16 games/3monkeys.taf` →
  `… c2 cf 93 45 3e 61 …`). **801 tasks**, 14 `ACT type=6` (EndGame) actions.
- **Result:** **WIN**, ending on
  `*** Congratulations, you did it!  (What took you so long?) ***`, verified in
  seeded Scarier (`goldens/3monkeys_solution.txt`, PASSing golden, win marker
  `Congratulations, you did it!`, **no env**). 112 commands.
- **Score: 98 out of 100, and 98 is the maximum obtainable.** See
  [The two points nobody can score](#the-two-points-nobody-can-score).
- **Source:** `downloaded/ThreeMonkeysOneCage_solution.txt`, the author's own
  190-line prose solution. It is a plan, not a command list — most of the
  ordering below had to be re-derived.

## Why this game is on the ADRIFT 4 map twice

This is the game that exposed the **`$RestrMask` left-association bug**. Its
task 21 is a `winnable` oracle the author wrote for playtesting, and with 55
restrictions it is the corpus maximum. One of its clauses is `#O(#A#)A#` —
"(bucket on hook OR coconut set up) AND the gate is still shut" — which SCARE's
old C-precedence parser read as "bucket OR (coconut AND gate)", so the oracle
announced *"The game is no longer winnable"* on turn 1 of a pristine game.
run400's `Sub_20_57` peels the **last** top-level operator and recurses on the
head: `A` and `O` are one precedence level associating **left**. Fixed in
`screstrs.cpp`; see `RUNNER_TESTS_TODO.md` §4 and the
`adrift4-restrmask-left-assoc` note. With the fix, `winnable` answers *"The game
is still winnable."* at every point along this route.

## The cage

Four rooms in a 2×2 grid — **NW, NE, SW, SE**. There is no diagonal move: SW to
SE is `e`, and `se` is a wall bump ("Your head smarts as you bump it rudely").

| Corner | What's there |
| --- | --- |
| SW | your bed (mattress, blanket, sheet), the fan, and where the fire gets built |
| SE | the gate in the east wall, the bar above it, the hook, the cord |
| NW | the banana tree, the high platform |
| NE | the coconut tree, the water trough, the hornets' nest |

The **chimpanzee** is an ally: it takes orders (`chimp, …`) once it is up a
tree. The **mandrill** is lethal — sharing its corner gives you exactly **one**
action, and spending that action on anything but leaving kills you. Two things
fence it out:

* **the fire** permanently blocks SW, and
* **smoke** blocks whichever corner the fan is aimed at — `north` → NW,
  `east` → SE, `northeast` → NE. Smoke needs `fire >= 3`.

That is why the fan is re-aimed four times in the route; the aim is a safety
interlock, not scenery.

## The fire economy

`make fire` burns the tinder **you are carrying**, and the fuel value decides
how long everything downstream works:

| Fuel | Value |
| --- | --- |
| jersey | 5 |
| blanket | 7 |
| sheet | 3 |
| banana peel | 2 |
| coconut husk | 2 |

The fire loses 1 per `fire_cycle` wrap (8 turns while it burns in SW with the
fan on, 6 otherwise). Above 13 in SW it can set the bed alight (task 415), so
the route feeds it the jersey (5) then the blanket (7) rather than everything at
once, and tops up later with the husk.

**Do not pick the sheet up early.** Carrying it makes `make fire` consume the
sheet (3) instead of the jersey (5), and the sheet is needed intact much later
as hornet armour.

## The route

Read alongside the author's prose solution; the headings match his stages.

**1 — Wake up and strip the bed** (`quiet` first: the author's running
commentary is chatty and randomised, and turning it off is what makes the
transcript reproducible.)

```
quiet / stand up / get hook / get stone / get blanket / get jersey /
ne / get fork / sw
```

**2 — Fire and smoke.** The fire goes up in SW (which the mandrill can now never
enter), the blanket doubles its life, and the fan aimed north walls off NW.

```
make fire / turn on fan / put blanket in fire / turn fan north
```

**3 — Banana on the platform.** This is the bait that eventually puts the chimp
up on the high platform.

```
n / get banana / throw banana on platform / s / z
```

**4 — Bucket on the hook.** The fork lifts the bucket; the cord goes over the
bar; the hook takes the bucket's weight.

```
get bucket / turn fan east / e / tie cord to hook / throw cord over bar /
lift bucket with fork / put bucket on hook / z / z
```

**5 — Coconut.** The chimp has to be *up the tree* (`chimp_elevated == 1`)
before it will take a tree or trough order; the "gesticulating wildly" paragraph
is only the preamble, and the real answer is the last sentence of the reply.

```
chimp, get coconut / turn fan northeast / ne / get coconut /
hit coconut with stone / get husk / z / give coconut to chimp / z / z / z /
chimp, put coconut on trough / lift trough with fork
```

**6 — Open the gate and wedge it.** Burning the cord through drops the
counterweight; the fork keeps the gate from slamming shut again.

```
s / push up gate / w / put husk in fire / lift husk with fork / turn fan east /
e / burn cord with torch / put out torch / put fork in gate / z / get cord /
tie cord to me
```

**7 — Hornets, then the mattress out of the gate.** `cover myself with the
sheet` is the wrong phrasing: tasks 637 (cover the *chimp*) and 638 (cover
*yourself*) share that alt-command and 637 wins on index. Task 638's **primary**
form is what works.

```
w / get sheet / put the sheet over my head / n / throw stone at nest /
s / get mattress / e / throw mattress through gate
```

At this point the score reads **95%** ("the rank of Homo Sapien (or close
enough!)") on turn 60, and `winnable` still says yes.

**8 — Survive turn 100.** The remaining 38 `z` are the game's design, not
padding. On turn 98 the ceiling panels open and anvils start falling; the
first wave is worth **+3** (task 697) wherever you are standing, but from then on
they kill within five turns unless you are under the bed.

```
z ×38 / w / hide under bed / z ×6
```

**9 — Out.** Once the anvils give way to bombs there are only nine turns left.
Moving out of SE silently unties the waist cord ("(first untying the cord from
your waist)"), so `tie cord to me` has to be the **last** action before the
jump — the +4 for the first tie is already banked either way.

```
get out / e / tie cord to me / jump out
```

## Scoring

`player_score` (variable 56) is a percentage. `SCR_DUMP_TASKS` finds **22 tasks
that add to it, summing to 97**, plus the anvil event's **+3** — 100 in total.
This route fires every one of them except the last 2, which cannot be fired at
all.

| + | Task | What it is |
| --- | --- | --- |
| 2 | 15 | getting out of bed |
| 5 | 185 | picking up the hook |
| 7 | 335 | making the fire |
| 3 | 162 | first time the fan blows smoke |
| 6 | 305 | the chimp jumps to the platform |
| 5 | 69 | the chimp gets the coconut down |
| 4 | 272 | bucket onto the fork |
| 5 | 294 | bucket hanging from the hook |
| 4 | 201 | cord thrown over the bar |
| 4 | 239 | hook tied to the cord |
| 5 | 588 | husk removed from the coconut |
| 5 | 395 | making a torch |
| 5 | 375 | burning the cord |
| 6 | 91 | coconut lands on the trough |
| 6 | 281 | the gate opens |
| 3 | 600 | `push up gate` |
| 5 | 490 | fork wedged in the gate |
| 3 | 470 | hornets agitated |
| 4 | 238 | cord tied to yourself |
| 2 | 186 | picking up the mattress |
| 6 | 614 | mattress thrown through the gate |
| 3 | 697 | the first wave of anvils falls (turn 101) |
| **2** | **603** | **`jump out` — unreachable, see below** |

## The two points nobody can score

Task 603 (`jump * out*`) is:

```
ACT type=5 v1=0 v2=604   execute task 604   (# jump out -- no mattress)
ACT type=5 v1=0 v2=608   execute task 608   (# jump out -- mattress)
ACT type=3 v1=55 v2=1 v3=-1
ACT type=3 v1=56 v2=1 v3=2      <-- the +2
```

604 and 608 are mutually exclusive on whether task 614 (`throw * mat* gate`) is
done, so **one of them always runs**, and both chains end the game. SCARE's
`task_run_task_actions()` "runs every task action … if any action ends the game,
return immediately", so the `+2` behind them is dead code in both the winning
and the losing branch. The player never sees a score line after the ending
either way. **98% is therefore the game's real ceiling**, and this route reaches
it.

(Whether run400 also drops actions queued behind an EndGame is not proven; it is
listed as an open probe in `RUNNER_TESTS_TODO.md`.)

## Deaths collected on the way

* **Mandrill, ×3.** Every one was spending the single grace turn on something
  other than leaving. Track the mandrill's corner, not just your own.
* **Anvil.** Standing anywhere but under the bed after turn 100 is fatal within
  five turns — including in the corner you have safely occupied for the previous
  forty.
* **Fire.** Feeding the fire everything at once pushes it past 13 in SW and the
  bed goes up, taking the sheet and the mattress with it.
