# Marooned — walkthrough (**WIN, 80/140** — and 80 is the ceiling)

- **Author:** unknown; the title screen reads "Marooned v1". A shipwreck-and-
  signal-fire island game.
- **Engine:** **ADRIFT 3.80** — the first TAF version 3.80 game in this corpus.
  (`xxd -p -l 12 marooned.taf | cut -c17-22` → `944536`.)
- **Result:** **WON, 80/140**, verified in seeded Scarier
  (`harness/marooned_solution.txt`, PASSing golden, win marker
  "Congratulations, you are no longer Marooned!"). The remaining 60 points are
  structurally unreachable — see the last section.
- **Physics:** the route is a win under the **real 3.80 pooled-burden model**
  (one axis, costs 1/3/7/3/7 by class, limit = `MaxCarried` = 8), measured in
  the genuine `run380.exe` and implemented in Scarier on 2026-08-03. Only one
  of this game's four heavy objects can be carried at a time, so the route
  **ferries** — see "What that means for the route" below.
- **Not a replay.** A walkthrough for *Marooned* is published, but it is for a
  **different build**: its wrecked-boat description ("you can't see why it came
  to rest here… Every so often, the boat rocks gently against the waves") is not
  the one in our file ("you can't see what happened to the boat, but the stern
  is under water. The slant of the deck makes it difficult…"). The route below
  is derived from the task dump and the room/exit map.

## The engine bug this game exposed

Before anything else, `marooned.taf` was unfinishable in Scarier:

```
>get tires
Your hands are full.
>get seal
The dead seal is too heavy for you to carry.
```

ADRIFT 3.8 stores a single **"Size/weight" class index, 0..4**, per object.
Every 3.8 grammar in `sctafpar.cpp` read `#SizeWeight` raw and passed it to the
version 4.0 model, which packs *size* in the tens digit and *weight* in the
units and scales each as `base^digit`. Class 4 therefore arrived as weight
`3^4 = 81` against this game's limit of `8 * 3^2 = 72`, and the tires became
uncarryable. The first fix normalised every 3.8 object to 4.0 "normal" (22),
which made the limits plain object counts again; the measurement below then
showed that the classes are real, so the class is now kept verbatim in
`SizeWeightClass` and enforced by a separate pooled-burden gate
(`obj_get_burden` / `obj_get_player_burden_limit` in `scobjcts.cpp`).
Normalising `SizeWeight` to 22 stayed, because it is what keeps 3.8 *container*
volumes (`Capacity*10+2`) plain object counts — and because every class costs
at least 1, the pooled burden is never looser than the size axis it shadows.

ADRIFT Generator 3.90 converts the class instead (`0→22 1→23 2→24 3→32 4→42`,
read off a gen390 conversion of this very file under Wine), and that conversion
**breaks the game**: the genuine `run390.exe`, playing gen390's own output,
refuses the tires with exactly the message we used to print.

### What the genuine 3.80 Runner actually does — measured 2026-08-03

`run380.exe` was assumed lost. It is not: David Whyld's dead delron.org.uk still
has `adrift38.zip` in the Wayback Machine, and it is now installed in the
adrift-battle Wine prefix (see `~/adrift-battle/runner/wine/README.md`). So the
question above is no longer a deduction. It was measured, and **both readings
were wrong**.

Method — a 3.80 `.taf` is *plaintext, CRLF-delimited, XOR'd with the VB6 PRNG
from seed `0x00a09e86`* and nothing else: no length header, no zlib, no "Wild"
trailer. So it decodes, edits (any length) and re-encodes losslessly
(`scratchpad/dec38.py`; round-trip `cmp`-clean against `marooned.taf`). Probe
files patched three plaintext fields — `#MaxCarried` (the line after
`$GameAuthor`), `#StartRoom` (the line after the first `**`, a **direct 0-based
room index**, not the `+3` offset objects use), and an object's `#SizeWeight`
(its short-name line **+11**) — then played them in `run380.exe` under Wine.

The 3.80 carrying model is a **single pooled burden**, not two axes:

| class | 0 | 1 | 2 | 3 | 4 |
|---|---|---|---|---|---|
| cost | 1 | 3 | 7 | 3 | 7 |

and the player's capacity is exactly **`MaxCarried`**. Pinned at MaxCarried =
1, 2, 3, 6, 7 and 8: at 2 a class-1 or class-3 object is refused and a class-0
accepted, at 3 both go; at 6 the tires (class 4) are refused, at 7 they are
accepted alone, at 8 tires + map fit but tires + flint + map do not, and tires +
the gas can (class 2, 7+7=14) never do. That last pair also kills the
two-axis idea outright — a size axis and a weight axis would have let the two
heavies coexist.

Consequences, in order of what they overturn:

* **Flattening every object to `22` was wrong on its own.** The classes are real
  and the genuine Runner enforces them; normalising alone let the player carry
  eight heavy objects where 3.80 allows one. Hence the burden gate.
* **gen390's table is wrong too, but only by one step.** A faithful 4.0 encoding
  would need cost 7 for the top class; the packed `base^digit` model cannot
  express 7, so gen390 rounds it to `3^2 = 9` — and 9 against a limit of 8 is
  exactly why converted 3.8 games stop being finishable. The conversion is
  lossy, but the classes it preserves are not the invention.
* **`Crime_Adventure.taf` is an author fault, not a conversion fault.** Its
  kettle is class 2 = cost 7 against MaxCarried 5, so the genuine 3.80 answers
  `get kettle` with "Your hands are full." **with empty hands** — while the five
  class-0 kitchen items are all picked up and fill the limit exactly. Its
  published "Get all the stuff in Fenwick kitchen" was never executable.
* **A 3.8-specific burden model was needed**, not a fixup that launders 3.8 data
  into 4.0 fields. Done, 2026-08-03: `|V380_OBJECT:_SizeWeight_|` now also
  writes `SizeWeightClass`, the globals fixup raises `BurdenModel`, and
  `sclibrar.cpp` gates every take through the pooled sum instead of the two 4.0
  axes. 3.8 has no "too heavy" refusal at all — the pooled check speaks, and it
  always says "Your hands are full."

### What that means for the route

The old route was a single-trip route: it ended the wreck phase holding flint +
tires + map + both gas cans, and later added the seal. Under real 3.80 physics
it breaks at **`get map`** — flint (1) + tires (7) is already the whole limit of
8, and Scarier now answers "Your hands are full at the moment."

The burdens in this game, at a limit of 8:

| object | class | cost |
|---|---|---|
| tires, dead seal, dented gas can, scratched gas can | 4 / 2 / 2 / 2 | **7** |
| trash, driftwood | 3 / 1 | 3 |
| knife, flint, map, berries, everything else | 0 | 1 |

So **one heavy object plus at most one light one per trip**, and the four
heavies start in four different places (tires on the bow, dented can in the
engine room, scratched can in the flooded engine room, seal under the trash on
the northern beach). What makes the win survivable is that the two cliff tasks
want their objects merely **visible**, not held: task 40 (`pour *gas on *tires`)
checks the dented can and the tires with `v2=3`, and task 10 (`light *tires`)
checks knife, flint and tires the same way. So the cliff can be *stocked* one
object per trip and the sequence fired at the end. The three lagoon throws, by
contrast, are `v2=1` — held — which is why each of them costs its own trip.

The route below therefore makes four loaded trips instead of one, and wins with
the same 80/140 on turn ~120, comfortably inside the Hunger event's ~210-turn
budget (event 1 starts on a random turn 10..20 and kills 200 turns later).

## The route

Full command list: `harness/marooned_solution.txt`. Phases:

**1 — The magic word (10).** `say yoho` scores 10 anywhere, any time, with no
restrictions at all. Do it on turn one.

**2 — Berries → monkey → flint (10).** North, west to the berry patch; get the
berries; north, north, west to the monkey along the river. `give berries to
monkey` puts him to sleep and drops the flint. The flint is mandatory for the
ending, which is why the pill (below) can never be scored.

**3 — Trip one: the tires (and the flint) to the cliff.** Carrying the flint
(1), west ×3 to the bow and `get tires` — 7 + 1 is exactly the limit. East ×7,
south ×3 brings you out on the cliff by way of the river and the jungle;
`drop tires`, `drop flint`. Both stay legible to the cliff tasks from the
ground, so the hands are free again.

**4 — Trip two: the dented can (and the map) to the cliff and the lagoon.**
North ×3, west ×8 back to the bridge; `get map` (it has no puzzle use — it is
ammunition for the lagoon), `down` to the engine room, `get dented gas can`
(7 + 1 again). Up, east ×8, south ×3 to the cliff; `drop dented gas can`; east
to the lagoon and `throw map`. That throw is a *sacrifice*: task 15
(`throw %object% *`, room 2, score 0, **non-repeatable**) steals the first throw
made at the lagoon, so spending it on the map lets the next throw reach a
scoring task.

**5 — Trip three: the scratched gas can (10).** North ×3, west ×9, down and
`dive` for the flooded half of the engine room; `get scratched gas can`, up, up,
east ×9, south ×3 to the lagoon; `throw gas can` fires task 33 for **10**. With
only the scratched can in hand the noun is unambiguous — the dented one is
already lying on the cliff, which is what the finale needs.

**6 — Trip four: the seal (20).** North ×3, west, north, west, north to the
eastern end of the northern beach. `move trash` scores **10** and uncovers the
dead seal; `get seal` (7 alone), then south, east, south, east, south ×3 back
to the lagoon and `throw seal` — task 3, **10**, and it is what opens the south
exit (`gateTask=3 wantDone=1`). The shark swims off with the carcass.

**7 — The knife (10).** South, south, in. `move skeleton` scores **10** and
reveals the knife; take it (1), then out, north, north, west to the cliff.

**8 — The signal fire (20) and the rescue.** Everything is already on the cliff
top: `open dented gas can`, `pour dented gas on tires` (**10**), `light tires`
(**10**). Knife plus flint plus gas equals fire, and the fire is what the ship
sees. Wait three turns while it closes, then **west** onto the eastern end of
the southern beach and wait one more, and the lifeboat comes ashore:

> Congratulations, you are no longer Marooned!

Going west matters: the `#win` task (29) only runs in rooms 0, 2, 3 and 8 — the
beaches. Stay on the cliff and you watch your own rescue sail past.

## The 60 points that cannot be scored

All four are author faults, verified against the task dump; none is a Scarier
divergence.

* **Task 24, "get trash" (10).** Its only two commands, `get *trash` and
  `take *trash`, are already ALTCMDs of task 14 — which is repeatable, sits
  earlier in the task list, and shares the one room the trash exists in. Task 14
  always wins the match, so 24 can never be the task that runs.
* **Task 27, "swallow pill" (10).** Needs task 7 ("eat berries") done. There is
  one bunch of berries in the game and the monkey requires it for the flint,
  without which the tires cannot be lit. Mutually exclusive with finishing.
* **Task 35, "shoot flare gun at shark" (10).** Requires holding object 20, the
  **unloaded** flare gun. Loading it (task 19) deletes object 20 and creates
  object 37, the loaded gun — and firing *that* matches task 18 first, which is
  fatal ("there is a fantastic explosion as it misfires"). The author's own hint
  describes a sequence the data cannot execute.
* **Tasks 9 + 28, the scratched can's pour and light (20).** Event 5 "Rescue"
  is started by **task 10** — the lighting that follows task 40, the *dented*
  can's pour. Pour the scratched can as well and task 9 wins the `light tires`
  match instead; the tires burn up (task 9 removes them), task 10's "tires in
  room" restriction then fails, and no ship ever appears. Trading those 20
  points for task 33's 10 is what makes the win possible; the net cost is 10.

Maximum reachable with a win: **80 of 140**.
