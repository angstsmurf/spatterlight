# Jonathan Grimshaw: Space Tourist — walkthrough

- **Engine:** ADRIFT 4 (`JGrim1.0.taf`), Ren, Writing Challenges Comp 2006.
  You have won a "once in a lifetime" cruise on the space liner *Cittian*,
  which is on fire before you have finished unpacking. 124 tasks, 10 rooms,
  9 NPCs.
- **Result:** ★ **WON.** The game keeps no score — every one of its 124 tasks
  is worth 0 points — so the only end-of-game marker is the last word of the
  final flush, `WHOOOOOSH`.
- **Solution:** `harness/JGrim_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`).
- **Provenance:** no author walkthrough exists, and unlike most Writing
  Challenges entries this one has no hint menu either. The route was derived
  entirely from `SCR_DUMP_TASKS` + `SCR_DUMP_OBJLOC`: the game has exactly one
  `EndGame` action, TASK 68 (`enter * dustbin *` on the alien bridge), and its
  five restrictions chain back through most of the game.

## Map

```
Luxurious 3 Star Cabin ─e─ Elevator ─u─ Ventilation Shaft ─s─ Luxury Dining Area
                                                                 │        │
                                                          w─ Gift Shop    u─ Escape Bay
                                                                              │
                                                              (dustbin down the chute)
                                                                              ↓
                        Alien Cottage ─n─ Valley ─e─ Hangar ─enter─ Alien Spaceship
```

The two halves are one-way: once you go down the garbage chute you never see
the *Cittian* again, and everything you were carrying (but not wearing) is
lost in the crash.

## Route

```
x me / press switch / take spanner / break window with spanner
throw spanner through window / get box / move chair to door / throw box in hole
wait / wait / e
x map / x sign / wait
take sock / take sweets / put sweets in sock / hit man with sock / peel wall
take suitcase / open suitcase / take sandwich / take sellotape
take sunglasses / wear sunglasses / take pistol / i / remove panel / up
take boots / wear boots / take margarine / take toolbox / open toolbox
take blowtorch / take tool / take cigarettes / unfasten blowtorch / take mud
i / wear margarine / s
take dustbin / get goldfish bowl / w
drop mud / wait / wait / wait / take hose / take dollar / e / u
take space-suit / put dollar in slot / type a 1 / type a 2 / type a 3 / type a 4
take all from drawer / i / d / give space bar to woman / take gloves / wear gloves / u
wear space-suit / wear bowl / wrap dustbin with foil / attach cylinder to hose
drop all / take sandwich / take oxygen tank / take dustbin / i
put dustbin in chute / enter dustbin
in / x hand / take disc / out / e / i / open door with disc / enter space-ship
press switch / look / sit on chair
press red button / press green button / press blue button / wait
stand beside mirror / take head / use microwave / look in microwave
take space bar / take dustbin / i / put dustbin in chute / enter dustbin
```

Seven `<waitkey>` pauses are blank lines in the solution file: two before the
first prompt (title screen), two after the first `enter dustbin` (the crash
landing), three after the last one (the ending). Do not reflow them.

## The launch checklist

TASK 67, the chute launch off the *Cittian*, is the wall this game is built
around: thirteen restrictions, all of which the route above satisfies.

| # | Requirement | Where it comes from |
|---|---|---|
| 1 | TASK 63 done (`put dustbin in chute`) | needs TASK 66, `wrap dustbin with foil` |
| 2 | space-suit **worn** | lying in the Escape Bay |
| 3 | goldfish bowl **worn** | TASK 40, `get goldfish bowl` in the Dining Area |
| 4 | boots **worn** | Ventilation Shaft |
| 5 | gloves **worn** | TASK 55, the large-bodied woman |
| 6 | sunglasses **worn** | TASK 24, off the dark-suited man |
| 7 | oxygen cylinder **not** held | consumed by TASK 80 |
| 8 | oxygen tank held | TASK 80, `attach cylinder to hose` |
| 9 | prawn sandwich held | in the dark suitcase |
| 10–13 | all four Space Bars **not** held | see the note below |

TASK 66 in turn wants the steel dustbin (Dining Area), the foil (`peel wall`
in the lift) and the sellotape (in the suitcase) in hand at the same time.

## Notes

- **The vending machine's keypad is two words.** `[A/B/C/D][1/2/3/4/5/6/7/8]`
  in the task command looks like one token, but the parser wants a space:
  `type a 1`, not `type a1`. `type a1` answers "You babble confusedly", which
  reads exactly like a wrong product code.
- **The Space Bars can never be given away.** TASK 55 hands over the gloves
  and fires TASKs 59–62, which were plainly meant to remove one bar each — but
  each of those tests *"Space Bar is in `- No room -`"*, i.e. the bar must be
  hidden already, so none of them can ever run. All four bars stay in your
  inventory and TASK 67 then refuses to launch you: *"You have the sandwich,
  but you'd rather eat the boots than the chocolate bar. You aren't taking any
  space bars with you."* There is no way to drop them individually either —
  the four objects share every noun, so `drop space bar` only ever produces a
  disambiguation prompt that cannot be answered. `drop all` is the way out: it
  drops carried items but not worn ones, so all five worn requirements survive
  and only three things need picking back up.
- **`up` in the Hangar goes the wrong way.** TASK 94 (`west`/`up`/`out`, back
  to the Valley) sits ahead of TASK 97 (`up`/`in`/`enter space-ship`) in the
  task list and steals the command, so after `open door with disc` the ship
  has to be entered as `enter space-ship`.
- **The mud is the vacuum-cleaner sabotage.** `drop mud` in the Gift Shop
  chokes the maid's cleaner (EVENT 8, 1–3 turns); she detaches the hose and
  puts it aside. Pick it up and TASK 87 makes her give up and leave. The hose
  plus the oxygen cylinder from the split blowtorch is the makeshift air tank.
- **The Cittian dollar is on a timer, not a trigger.** EVENT 5 drops it on the
  Gift Shop floor four turns after you first walk in, regardless of what you
  do; the three `wait`s cover both it and the maid.
- **Do not `sing`.** TASK 41 (`* sing *`) permanently locks out TASK 40, and
  the emptied goldfish bowl is one of the five things TASK 67 wants worn.
- **The endgame is on two overlapping timers.** The third coloured button
  destroys Jupiter and starts EVENT 11, which turns the alien murderous one
  turn later; `stand beside mirror` then bounces his own laser bolt back at
  him and starts EVENT 12, which launches (and wastes) every escape pod three
  turns after that. TASK 64 will not let you re-load the garbage chute until
  the pods are gone, so the three turns are spent unscrewing the alien's head
  for a second helmet and printing a Space Bar in the alien microwave — the
  bar TASK 68 wants in your hand.
- The pistol, the cosh, the cigarettes and the flint lighter are all red
  herrings; `shoot alien` (TASK 117) and `hit alien with sock` (TASK 25) both
  exist but change nothing.
