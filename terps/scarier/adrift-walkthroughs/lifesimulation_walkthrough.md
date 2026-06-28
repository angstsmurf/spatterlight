# Life Simulation V1.3 — walkthrough / analysis

**Game:** *Life Simulation* Version 1.3 (ADRIFT 4 `.taf`)
**Type:** open-ended life-sim sandbox
**Result:** **No score (0/0) and no win state.** There is nothing to "solve";
the game is a toy city you potter around in.
**Solution file:** `harness/lifesimulation_solution.txt` (a representative,
deterministic tour — *not* a winning sequence, because there is no win).

---

## Verdict up front

*Life Simulation* is not a goal-directed adventure. Its own intro says so:

> "There is no objective to this game, just do what ever you want to do and
> have fun."

That is literally true. Dumping the game's internals confirms it:

- **Maximum score is 0/0.** The game reports it (`Your score is 0 out of a
  maximum of 0`), and a structural dump of all **67 tasks** shows the action
  types they contain are only:
  - type 0 — move object (41×)
  - type 1 — move player/NPC (4×)
  - type 3 — change variable (51×)
  - type 7 — change battle attribute (6×)
- **There is not a single `ChangeScore` (type 4) action** in the whole game,
  so no task can ever award a point — hence the 0/0 maximum.
- **There is not a single `EndGame` (type 6) action** either, so **no task can
  win, lose, or end the game.** You cannot "finish" *Life Simulation*; you just
  stop playing.

So this joins *The Town of Azra* and *The Nonsense Machine 6000* as a
no-score / no-win ADRIFT sandbox. Documented here for completeness.

---

## The "#winner" red herring

There is a hidden system task **`#winner`** (task 17) whose name suggests an
ending. It is not one. Its dump is:

```
DUMP task 17 "#winner" mask=[#]
   R0 type=4 v1=6 v2=2 v3=50      (restriction: variable 6 compared to 50)
   A0 type=3 v1=1 v2=1 v3=5000    (action: set variable 1 = 5000)
```

It is the **lottery payoff**: when an online-gambling variable reaches its
threshold it sets your *money* variable to $5000. It is a single
`ChangeVariable` action — it makes you rich, it does **not** end or score the
game. (The matching online-play tasks are `play online chance 2001` (60),
`free gift`/`#fgp` (63/64), reached via `turn on computer` → `email` →
`www.freegift.com`, etc., after you've bought the `ibn laptop 7800 package`.)

## The "you can die" caveat

The intro warns "You can die in this world, if you do anything stupid." The only
"stupid" thing the data supports is **`shoot lisa`** (task 65) — Lisa is the
supermarket clerk. That task is the only place the rudimentary **ADRIFT Battle
System** is used (its actions raise/seed battle attributes, type 7, and spawn
the "Dead Lisa" NPC). Combat death, if it happens, is handled by the Battle
System's stamina logic, **not** by any task `EndGame` action — there is no
death *task*. It still leads nowhere scored or winnable; it just ends a session
badly. Avoid it if you want to keep pottering.

---

## The city map

You start in **Your Bedroom**. The apartment is upstairs/downstairs; the world
outside is a short high street ("Main Street") with shops.

```
          Your Bathroom (1) — Inside Your Shower (2)
                |E
   Your Bedroom (0) —W— Upstairs Landing (3)
                              |Down
   Front Garden (6) —E— Lounge Room (4: Couch, TV) —E— Kitchen (5)
       |W
   Main Street - Centre (8) —W— Hudson Shopping Centre (9) —N— Supermarket (10, Lisa)
       |N  |S                         |S Newsagent (14)   |W Bank (11) — Loans (12) — ATM (13)
   Northern End (15)                 Southern End (18)
       |  Car Dealer (16) / PC Xtreme (17)      (descriptions mention McDonalds /
                                                 Book Store but those exits are blocked)
   Jail (7) — reached if you misbehave
```

NPCs: **Lisa** (supermarket clerk; becomes "Dead Lisa" if shot), **Jim** (bank),
**Bill** (loans), **Joe** (computer store), **Police Officer Charles**.

## Things you can do (the 67 tasks)

Pure sandbox activities — none scored. A representative sample:

- **Home:** `go to sleep` / `sleep` (8 h, restores health), `have a nap` (1 h),
  `open window`, `wash hands`, `use toilet`, `flush toilet`, `use the shower`,
  `pick up towels`, `turn on tv` then `channel 1` (Star Wars marathon) or
  `channel 7` (a game show), `eat chips`, `drink mountain dew`, `drink milk`,
  `eat chocolate`, `pour milk into bowl` + `pour cereal into bowl` + `eat
  cereal`, `smoke` a cigarette.
- **Money / bills:** you start with $50 and lose **$50/day** to bills (rent,
  power, water — $350/week); unpaid bills go to debt. `take a loan` (Loans Dept),
  `ask for credit card` / `ask for keys`, `withdraw $5…$100` / `deposit $5…$100`
  at the ATM, `chat to jim` / `chat to bill` / `chat to joe` (unlocks topics).
- **Shops:** `buy chips` ($2 from Lisa), `buy milk`, `buy cereal`,
  `buy chocolate`, `buy cigarettes`, `buy matchs`; newsagent: `buy total pc
  magazine`, `buy sport car magazine`, `buy newspaper`; PC Xtreme:
  `buy ibn laptop 7800 package`.
- **Computer / online:** `turn on computer`, `email`, `put total pc cd in
  laptop`, `www.hudsonbank.com`, `www.freegift.com`, `free gift`,
  `play online chance 2001` (the lottery that can trigger `#winner` → $5000).
- **Getting back in:** `unlock door` to re-enter the house from the front garden.
- Phone numbers (`1800555928`), a PIN-like string (`1024 5682 9987`), and
  voucher codes drive some of the above.

## Representative tour (the solution file)

`harness/lifesimulation_solution.txt` simply demonstrates the world is alive and
deterministic — check the time, watch both TV channels, walk to the supermarket,
buy chips from Lisa, and confirm the score is still 0/0:

```
look at watch
west            (Upstairs Landing)
down            (Lounge Room)
turn on tv
channel 1       ("Star Wars marathon … Episode 1")
channel 7       ("the game show right now")
turn off tv
west            (Front Garden)
west            (Main Street - Centre)
west            (Hudson Shopping Centre)
north           (Hudson Supermarket — Lisa)
buy chips       ("You give Lisa $2 and she hands you a pack of chips")
south
look
score           ("Your score is 0 out of a maximum of 0.  (0%)")
```

Deterministic under the seeded harness (verified identical across 3 runs).

---

## How this was determined

Built the deterministic headless SCARE (`harness/build.sh`) and:

1. Booted with an empty solution — intro states "no objective"; `score`
   reports **0/0**.
2. Dumped structure via the debugger (`SC_DEBUGGER_ENABLED=1`): **19 rooms,
   54 objects, 6 NPCs, 67 tasks**; no Battle-System-driven scoring.
3. Enumerated **every task's actions** with a temporary `SC_DUMP_TASKS` block
   in `sctasks.c` (since reverted — tree left clean): **zero `ChangeScore`
   (type 4) and zero `EndGame` (type 6) actions anywhere**, which is the
   definitive proof that the game has no score and no ending. The `#winner`
   task is a lottery `ChangeVariable`, not an ending.

All faithful to the original ADRIFT Runner — these are the author's design
(or lack of one), not SCARE limitations.
