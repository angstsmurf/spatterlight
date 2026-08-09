# ADRIFT-O-RAMA — walkthrough

- **Engine:** ADRIFT 4 (`adriftorama.taf`, Mystery, ScummVM gameid
  `sm03_adriftorama`). A novelty **18-hole mini-golf** course, each hole themed
  after a member of the ADRIFT community forums ("Bob the Newbie's Hole", "The
  Mad Monk's Hole", "KF's Hole", "Guess the Verb Mountain", …).
- **Result:** ★ **WON**, 0/0 — *"There is no scoring involved because Mystery
  doesn't know squat about golf"*. The game keeps its golf strokes in its own
  variables, not the ADRIFT score system, so `score` always reads 0/0.
- **Solution:** `goldens/adriftorama_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `*****You Win!*****`.

## Structural verdict

51 tasks, **20 rooms** (18 holes + the lobby and `THE END`), 4 events, 1 NPC
(Campbell). Zero `ChangeScore` (type-4) actions anywhere.

There is exactly **one `EndGame` action** — task 48 `#Kill Campbell`, and it is a
`var1=1` **lose**. The victory is *not* an `EndGame`: sinking the eighteenth putt
runs task 26 `#Campbell's Hole`, which simply **moves you to room 19, `THE END`**,
whose description is the win text:

```
THE END

 *****You Win!*****        Thanks for playing ADRIFT-O-RAMA!  You'll find
that it is different the next time you play! …
```

So the game *is* completable — an earlier revision of this file said otherwise
because it only looked for an `ACT type=6 v1=0`.

## The mechanic

Every hole is the same two-command cycle:

```
put ball on marker
hit ball
```

- **The ball has to be on the marker.** The hole tasks (9…26, one per room) carry
  an object-location restriction on the marker — a **SURFACE, object 11**. With
  the ball merely in hand, `hit ball` falls through to that room's `"<name> CLUB"`
  joke task and nothing happens. This is the single thing that makes the game
  look unplayable.
- **Each putt is a dice roll.** Every hole task also has a variable restriction,
  and event 0 *Variable Change* re-rolls all ~95 variables **every turn** (task
  5). You just keep swinging until it drops — there is no skill component and no
  stroke limit.

## Route

```
(blank) ×2                     <- the title screen's two "...Continue..." pauses
buy ball                       <- task 1: feeds the gold token into the machine
north                          <- task 8: opens the door, slides you to hole 1
put ball on marker / hit ball  <- ×30 under the harness seed: clears holes 1-17
guess the verb                 <- task 25: Guess the Verb Mountain, no putt
put ball on marker / hit ball  <- Campbell's Hole (18) — goes in first try here
```

Hole order under the fixed seed:

> Bob the Newbie → The Mad Monk → Mel S. → Cowboy → Matt (Dark Baron) →
> Hanadorobou → KF → David W. → Cannibal → DuoDave → The Amazing Poodle Boy →
> DS490 → Mut → MileOut → Woodfish → Mystery → Guess the Verb Mountain →
> Campbell

Thirty cycles was found by bisection (29 is not enough, 30 is); because the putts
are random the count is seed-specific, which is exactly why the row belongs in
the deterministic harness rather than in a hand-played transcript.

## Notes

- **Guess the Verb Mountain has no putt.** Task 25 wants the literal
  `guess the verb`; keep swinging there and you will never leave.
- **Do not `hit Campbell`.** Task 48 `#Kill Campbell` is the game's only
  `EndGame`, and it is a **lose**.
- The 18 per-hole "CLUB" tasks are jokes, not puzzles — they fire when you swing
  without the ball on the marker.
