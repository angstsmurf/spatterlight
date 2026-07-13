# Quest for the Magic Healing Plant — walkthrough

- **Engine:** ADRIFT 3.9. Adam G. Crutchlow, 1995/1996, ported from Inform to
  ADRIFT in 2002 (`mhpquest.taf`).
- **Result:** ★ **WON, 140/140 MAX.** Every scoring task in the game.
- **Solution:** `harness/mhpquest_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`).

## Structural verdict

Small and sparse: 33 rooms (most of them an undifferentiated "Gloomy Forest"
maze), 14 tasks. The seven `ChangeScore` actions sum to exactly the 140 the game
reports as its maximum, so a full clear is provable from the dump alone:

| pts | task | where |
|----:|------|-------|
| 10 | `kill orc` (needs the knife) | East Road's End |
| 10 | `search bits` | Dark Cave |
| 10 | `search fire` | Gloomy Forest (NW corner) |
| 20 | `throw coin in fountain` | Fountain |
| 20 | `play flute` | Edge of Forest |
| 20 | `get clover` | Secluded Lake |
| 50 | `feed clover to crystal` + **EndGame(win)** | Small Cottage |

Two instant deaths, both `EndGame(death)` tasks, and neither is on the route:
**entering the pit** in the Gloomy Forest, and going **east off the Mountain
Path**.

## The port caveat (TODO §5)

The Key & Compass walkthrough solves the **Inform original**, and three of its
commands simply do not exist in the ADRIFT port:

| K&C (Inform) | ADRIFT |
|---|---|
| `enter cave` | the cave is a plain exit: **`in`** (or `n`) from Mountains |
| `put coin in fountain` | the task command is **`throw coin in fountain`** |
| `enter wall` | **`in`** from Edge of Forest — an exit gated on having played the flute |

It also omits the `e` from **Grassy Hills** to the **Mountains**, so following it
literally leaves you one room short of the cave and the whole coin → fountain →
flute → wall → clover chain never starts. Welbourn's route scores 20/140.

## Route

`out` → Small Clearing, then round to the **Woodland** south of the cottage for the
**knife** (`take knife`). East and north to the **East Road**, follow it east to
**East Road's End** and `kill orc` **(+10)** — the orc is harmless but can only be
killed while carrying the knife.

Back west along the road, then `n n n` to **Grassy Hills** and **`e`** to the
**Mountains**. **`in`** → **Dark Cave**: `search bits` **(+10)** turns up an old
**coin**; `take coin`.

`out`, then `w w w` to the fire in the north-west of the forest: `search fire`
**(+10)** turns up the **torn note** ("only the rare magic clover plant will cure
any ill"). `se se e e` → **Fountain**: **`throw coin in fountain`** **(+20)** and a
**flute** surfaces; `take flute`.

`w w w w` → **Edge of Forest**, where a vertical wall of stone blocks the west
(remember the carving in the Dark Wood: *"Walls have ears"*). `play flute` **(+20)**
opens the rock, **`in`** → **Secluded Lake**, `take clover` **(+20)**.

Home: `e e e se e se s` → **Small Cottage**. `feed clover to crystal` **(+50)** —
she perks up, recovers in a few days, and you keep a few leaves back just in case.

## Note — a dumper bug found here

The route above initially walked into the wrong rooms because `scdump.cpp` listed
the exit array's diagonals as **NE, NW, SE, SW**. ADRIFT's real order is
**NE, SE, SW, NW** (`sclibrar.cpp`'s DIRNAMES, and `mapdraw.cpp`'s `map_dirs`,
which both engines share), so every diagonal exit in every dump was mislabelled.
Fixed; cardinals were never affected, which is why it had gone unnoticed.
