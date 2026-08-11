# Life — walkthrough

- **Engine:** ADRIFT 3.90 (`life.taf`, 45,737 bytes, from ifarchive). **17
  rooms, 5 NPCs, 59 tasks, 25 events, 30 variables**, empty WINTEXT.
- **Result:** ✗ **UNFINISHABLE.** The file contains **no `ACT type=6`** — not
  one task in the game has an EndGame action — and **no `ACT type=4`**, so
  there is no score either. There is nothing to win and nothing to maximise.
- **Solution:** `goldens/life_solution.txt` — a *demonstration* route, in the
  manner of `hangover_solution.txt` and the Penrhyn row. Golden blessed, in
  `run_v4_walkthroughs.sh` with `SCR_SKIP_WAITKEY=1`. Marker: `Health=%health%`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS`,
  `SCR_TRACE_TASKS` and play.

## What the game promises and what it contains

The title screen says: *"In this life sim you must get a job, get a girl/guy
and get rich."* None of the three exists. The complete task list is 59
entries, 25 of which are internal `#…` clock and decay tasks; what is left is
the whole of the implemented game:

| Where | Commands |
|---|---|
| anywhere | `time`, `stats`, `help`, `cat help` |
| Kitchen | `prepare meal`, `clean * dishes` |
| Living room | `watch tv`, `clean up *` |
| Bedroom | `sleep` |
| Bathroom | `piss` |
| three shops | `* in`, `* out` |
| computer store | `buy * power games master` (100$) |
| pet shop | `buy * cat` (150$), `buy * milk`, `buy * cat food` |
| with the cat | `stroke * cat`, `kick * cat`, `cat feeling` |

There is no job task, no NPC conversation of any kind, and **no task anywhere
that increases money**. The video rental, the movie theatre, the supermarket's
interior, the friend's house on Linden street, and all four street NPCs (Anne,
Carl, Jake, Elise) are scenery. Jake is the only one who does anything, and
only as the recipient of the computer purchase.

## Route

```
Bedroom        Bob / male                  (name and gender prompts)
               s
Kitchen        prepare meal / clean dishes
Bathroom       e / piss
Kitchen        w
Living room    w / watch tv / clean up
Hindburg st    out / n
computer store in / buy power games master / out
               n / e
pet shop       in / buy cat / buy milk / buy cat food / out
               w / s / s
Living room    in
Kitchen        e / cat feeling / stroke cat
Bedroom        n / put computer on desk
               time / stats
```

Thirty-four lines including the two startup answers. The clock starts at
**11:06 on a Sunday in December** and all three shops are already open, so
nothing has to be waited for.

## Money is a one-way valve

`VAR 22 money` is decremented by exactly two tasks — T28 `−100` for the
computer, T41 `−150` for the cat — and incremented by none. `buy milk` (T37)
announces *"It costs 15$"* and its only action is `milk += 100`; `buy cat
food` (T38) announces 20$ and **has no actions at all**, so the food bowl the
help text describes is not modelled. Money is never displayed anywhere, not
even by `stats`. Whatever the starting balance is, 250 of it is spendable and
the rest is unreachable.

## Three authoring bugs, all of them the game's

**`stats` prints `Health=%health%`.** The 30 variables are listed by
`SCR_DUMP_TASKS` and there is no `health` among them, so the reference is
never substituted. This is the harness's win marker, which felt like the
honest choice for a game whose only reachable end state is its own bug.

**`piss` works and looks like it doesn't.** In the bathroom it answers *"I
don't understand what you mean!"*, which is the parser's failure message —
but `SCR_TRACE_TASKS` shows `task 24 running 2 actions` (bladder back to 100,
+15 minutes) then `finished, return false`. T24's completion text is empty, so
the engine has nothing to print and falls through to the failure line. In
every *other* room the same command correctly says *"You can't do that here!"*,
which makes the bathroom the one place where the command appears not to be
understood. It is on the route deliberately, to record this.

**Two dangling references in the help text.** `cat help` ends mid-sentence —
*"To know what your cat is thinking at the moment you type ''"* — and the
`buy cat food` message tells you to type `cat food level` in the kitchen.
There is no `cat food level` task.

## Shop hours

The three `* in` tasks are gated on `VAR 8 hour`, and a matching `#…close`
event throws you out at 18:00:

| Shop | Entered from | Open | Enter / eject |
|---|---|---|---|
| computer store | room 6 | `hour >= 10 and <= 17` | T26 / T25 |
| pet shop | room 10 | `hour >= 11 and <= 17` | T30 / T29 |
| supermarket | room 15 | `hour >= 9 and <= 19` | T33 / T32 |

All three `#…close` tasks fire on `hour == 18`. The supermarket's sign says
it closes at 20:00 and its `in` task agrees, so between 18:00 and 19:59 you
can walk in and be thrown straight back out.

A turn is three minutes (T1); the big tasks add their own on top —
`watch tv` +30, `prepare meal` +30, `clean dishes` +35, `piss` +15.

`sleep` (T21–23) never fires on any realistic route: it wants `VAR 15 sleep`
below 600, and the decay is 3 per 5 turns from a starting 997, so you would
have to idle some 660 turns first. Until then it answers *"You aren't tired
enough to go to sleep."*

## The cat

This is the one subsystem with real depth, and it is also the one that goes
nowhere. `buy cat` moves the cat NPC into the Bedroom and sets `cat`=1; after
that it wanders the house, so `stroke`/`kick` only work in whatever room it
happens to be in — the route catches it in the Kitchen on the way back from
the shops, and anywhere else you get *"Your cat isn't here."* (`cat feeling`
has no restrictions at all and reports from across the house.)

Each of `stroke` and `kick` is four tasks (T42–45, T46–49), one per value of
`VAR 26 catfeeling` — 0–3, reported as *aggressive / peaceful / sad /
sleepy* — and each nudges one of `catmood`, `catlonely`, `catsleepy` by ±3.
Events 17–24 re-roll `catfeeling` every three hours on the hour, via T50–57's
`ACT type=3 v2=2`, which is *Var = rnd(range)*.

**Nothing ever reads the result back.** The only restrictions anywhere on
`catmood`, `catlonely` and `catsleepy` are the ±197 bounds inside the very
stroke/kick tasks that write them — clamps, not gates — and `cat feeling`
reports the roll, not the mood. The cat can be raised and its raising can
never be observed, which is the whole game in miniature.

## Notes

- The house is entered from the street with `in` and left with `out` from the
  Living room; the shops are the same. The title screen says so explicitly,
  which is unusual and welcome.
- `put computer on desk` needs no task — the desk is a `SURFACE` and the
  engine's own put-on handles it. It is the only thing the computer does; no
  task ever reads `VAR 19 compu`.
- The file has **64 identical `CONTAINER` entries for the milk bowl**, the
  same Generator artefact as Asylum's 64 `door` entries.
- Two `#CHRISTMAS` tasks (T35, T36) fire on 24 December, which is about three
  weeks of real turns away from the start.
