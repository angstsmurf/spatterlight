# Montahue Scott and the Mobius Belt — walkthrough

- **Engine:** ADRIFT 3.9 (`ms_mobius.taf`, 23,225 bytes). **One room** — the
  twist in the Mobius Belt — 17 objects, 18 tasks, 3 NPCs (Virgil, Chelsea,
  Bo), 2 events, and one variable the game never reads.
- **Result:** ★ **WON, 3/3** — the game's own stated maximum, and every
  `ACT type=4` on a winning line.
- **Solution:** `goldens/ms_mobius_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `That little TV screen for the inside of your hat was a good investment.`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS`.

## Route

```
n                                (blockade to the north — the astronauts)
n                               (the other way round; you find "something")
throw something                 (+1, smashes the window, frees all three)
open comm                       (it's a piece of crap)
ask chelsea about comm          (must come BEFORE Virgil — see below)
bo fix comm                     (ditto)
vir fix comm                    (+1, calls the Universal Parliament)
break glass / smell paint
x belt / x twist / x planet / x stars / x hole
look / look / score             (padding while the shuttle flies in)
board ship                      (+1, EndGame win)
```

## Scoring

| Task | Command | Points |
|---|---|---|
| 7 | `throw something` | +1 |
| 8 | `vir fix comm` | +1 |
| 12 | `board ship` | +1 |
| | **total** | **3** |

The only other `ACT type=4` in the file is TASK 15 `#ship leaves`: **−1**,
followed by `ACT type=6 v1=2`. It is the loss, not a route.

## The order trap

Three tasks share the restriction `RESTR type=2 v1=9 v2=1` — *task 8 must
**not** be done*:

| Task | Command |
|---|---|
| 3 | `open comm` / `use comm` |
| 9 | `ask che comm` / `show comm che` … |
| 10 | `bo fix comm` / `ask bo comm` … |

TASK 8 is `vir fix comm`. So the moment Virgil fixes the communicator, the
other three lock out permanently and answer

> It's fixed already, don't give it to a nonprofessional to break.

Chelsea and Bo are pure flavour (*"Don't ask me, I don't know anything about
that stuff. Ask Virgil, he's an engineer."*; *"Bo just stands there, regarding
you and your stupidity."*) but they are the joke the middle of the game is
built on, and asking Virgil first throws them away. This is the one thing in
the game a player can permanently miss.

## The clock

| Event | Started by | Fires | After |
|---|---|---|---|
| 0 `Shuttle to the Belt` | TASK 8 `vir fix comm` | TASK 11 `#ship arrives at belt` | 5–15 turns (**8** under the fixed seed) |
| 1 `Ship leaves` | TASK 11 | TASK 15 `#ship leaves` | exactly 15 turns |

TASK 11 moves all three NPCs to *nowhere* (`ACT type=1 v3=0` ×3) — they board
the shuttle — and flips the room's alternate description on
(`ALT room=0 v2=12`: *"A ship pokes through the outer wall of the Belt's
surrounding glass shell, waiting for you to board."*). From that moment you
have fifteen turns. The flavour block in the route is padding to cover the
arrival wait and lands well inside the window; the timing is not tight, but
the countdown is real and the failure is fatal.

## Notes

- **The hat is worn from turn one.** TASK 12 `board ship` needs
  `RESTR type=0 v1=3 v2=2` (hat *worn*), and nothing in the game puts it on —
  there are only ways to lose it. TASK 5 (`throw hat` while holding) and
  TASK 6 (`throw hat` while wearing) exist purely so the player can throw the
  wrong thing at the blockade window; the hat is also the file's only real
  `CONTAINER`.
- **`n` and `s` are the same task, three times over.** TASK 0, 1 and 2 all
  match `n`/`north`/`s`/`south`/`go …`/`leave …` in a one-room game, chained
  by task-done restrictions: TASK 0 (first trip, find the blockade), TASK 1
  (second trip, `ACT type=0` puts *something* in your inventory), TASK 2
  (after the throw, repeatable flavour). The corridor "runs north and south"
  and both ends are the same place, which is the entire conceit — the Belt
  has one side.
- **63 phantom containers.** The dump lists `CONTAINER idx=1..63 obj=16
  [space ship]`, all pointing at the same object. That is not a puzzle; it is
  what an ADRIFT 3.9 file looks like when the author set the container count
  wrong. Only `idx=0` (the hat) is real.
- **`something` is never identified.** *"a nice big chunk of… of something
  unidentifiable"*, prefix `[...]`, alias `thing`. TASK 7 accepts eighteen
  phrasings for throwing it, including `throw cube` and `throw ball` — the
  author covering guesses at what it might be.
- **The ending is the punchline.** The pilot has room for three passengers,
  Virgil looks at you, Chelsea looks at you, and *"You discreetly shove Bo
  down the boarding ramp and pull it shut."* Then you tip the hat over your
  eyes and watch the TV screen sewn into its lining.
