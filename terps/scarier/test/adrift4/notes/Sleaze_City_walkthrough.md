# Sleaze City — walkthrough

- **Engine:** ADRIFT 3.9 (`sleaze.taf`, 29,811 bytes). Ten rooms of a
  cross-between-New-York-and-Chicago slum, 55 objects (48 of them `STATIC`
  scenery), 64 tasks, **no NPCs, no events, no variables**. You woke up on
  Monday owing rent and decided to get a job.
- **Result:** ★ **WON, 100/100** — the sum of every `ACT type=4` in the file.
  There are no negative ones.
- **Solution:** `goldens/sleaze_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `Good job, you somehow managed to survive Sleaze City.`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS` +
  `SCR_DUMP_OBJLOC`.

## Route

```
turn on tv / open crate                       (+5 +5, the crate holds $5)
n / e / talk to landlord                      (+5)
w / d / n
look at jack / take car jack / talk to red    (+5 +5)
s / w / s
buy tickets / take wirecutters                (+5 +10, the ticket eats the $5)
n / e / s
cut chain                                     (+5, frees a newspaper)
read newspaper                                (+5, your numbers came up)
n / u / s
use jack on couch                             (+10, a dictionary under it)
read book                                     (+5)
n / d / w / n / talk to gimpy                 (+10, drops you in the Kitchen)
out / e / u / e
give ticket to landlord                       (+10)
take sandwich                                 (+5)
w / d / w / in / score
serve                                         (+10, EndGame win)
```

## Scoring — all 100

| Task | Command | Points |
|---|---|---|
| 5 | `turn on tv` | +5 |
| 7 | `open crate` → money | +5 |
| 8 | `talk to landlord` | +5 |
| 13 | `take car jack` | +5 |
| 14 | `talk to red` | +5 |
| 22 | `buy tickets` → lottery ticket, **spends the money** | +5 |
| 23 | `take wirecutters` | +10 |
| 24 | `use wirecutters on chain` → newspaper | +5 |
| 28 | `read newspaper` | +5 |
| 16 | `use jack on couch` → dictionary | +10 |
| 19 | `read book` | +5 |
| 18 | `talk to gimpy` | +10 |
| 29 | `give ticket to landlord` | +10 |
| 31 | `take sandwich` | +5 |
| 32 | `serve` (EndGame win) | +10 |
| | **total** | **100** |

## The map

```
        Cafe(7)          Kitchen(8)
           |N                |OUT
Store(9) --N-- Archer(6) --E-- Main St(3) --N-- Garage(5)
                  ^IN(8)          |S              
                                Alley(4)
Apartment(0) --N-- Hallway(1) --E-- Landlord(2)
                      |D
                   Main St(3)
```

Two joins are one-way in practice: `1 D → 3` and `3 U → 1` are the elevator,
and the cafe has two mutually exclusive doors (below).

## The one real dependency chain

Only four of the sixty-four tasks carry restrictions at all, and three of them
form a single line:

1. **`buy tickets`** (T22) needs the money held, and its own `ACT type=0
   v1=3 v3=0` destroys it. The $5 in the crate is the only money in the game.
2. **`cut chain`** (T24) needs the wirecutters and produces the newspaper.
3. **`read newspaper`** (T28) needs T22 done *and* the paper held — that's
   how you learn the ticket won.
4. **`give ticket to landlord`** (T29) needs T28 done.

The fourth restricted task is `talk to gimpy` (T18), gated on `read book`
(T19) — you need the dictionary from under the couch before you have anything
to say to him.

## Two things that catch people

- **The cafe door reverses.** `EXIT room=6 N -> 7` is `gateTask=18
  wantDone=0` and `EXIT room=6 IN -> 8` is `gateTask=18 wantDone=1`
  (`gateTask` is 0-based). So the front entrance of the cafe works only
  *before* `talk to gimpy` and the kitchen window only *after*. T18 itself
  teleports you into the Kitchen (`ACT type=1 v1=0 v3=8`), which is why the
  route immediately walks back `out`. The Archer Street description explains
  it in-fiction: *"You decide it would be better that you do not go into the
  cafe through the front entrance if Gimpy thinks you're cooking."*
- **The sandwich is a room description.** Nothing places it. `ALT room=2
  alt=1` (keyed on `v2=30`, i.e. task 29 counted 1-based) swaps the
  Landlord's Room text to *"Your landlord is now gone, he ran away with his
  lottery ticket. His lone chair has a small sandwich sitting on it."* — and
  T31 `take sandwich` is what picks it up. Hand over the ticket first or
  there is nothing there.

## Notes

- **`give money to landlord`** (T9) is a restricted task with **no actions at
  all** — the landlord is unimpressed by five dollars. It does not take the
  money, so it is survivable, but the rent is meant to be paid with the
  winning ticket.
- **Forty-eight of the fifty-five objects are scenery**, and about thirty of
  the tasks exist only to give a scenery noun a custom refusal: `take board`,
  `take nails`, `pull rope`, `hit dumpster`, `buy weapons`, `do magic trick`,
  `take grease`, `turn on sink`. None of them score and none of them matter.
  `heh` (T40) is the author's easter egg.
- **The author shipped hints in the file.** T24, T29 and T32 carry
  `HINTQ`/`HINT1`/`HINT2`, and T32's is the structural one: *"I think you
  should get out of debt before worrying about that"* → *"Pay back your rent
  and the answer will come to you"*.
- **`perspective=0`** — the game is narrated in the *first* person (*"I move
  north."*, *"I grabbed the sandwich"*), which is unusual enough among the
  corpus that it is worth noting when reading the transcript.
- **The opening screen tells you the parser rule**: *NOTE: Type "Talk to
  character" instead of "ask someone about something"*. Three of the fifteen
  scoring tasks are `talk to …`.
- **No `<waitkey>` anywhere** (`SCR_MARK_WAITKEY=1`), and the only synonym in
  the file is `get` → `take`.
