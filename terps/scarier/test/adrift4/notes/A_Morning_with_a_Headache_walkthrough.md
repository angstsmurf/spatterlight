# A Morning with a Headache — walkthrough

- **Engine:** ADRIFT 3.9 (`A_Morning_with_a_Headache.taf`, 26,973 bytes). You
  are Frank, you have a blackout, a naked stripper asleep on your couch, an
  alarm clock you can't reach, a landlord downstairs and — somewhere behind
  the hangover — a wedding to be best man at. 6 rooms, 33 objects, 39 tasks,
  3 NPCs (the Woman, Hanna, the Landlord), **8 events**.
- **Result:** ★ **WON, 115/115** — the game's own stated maximum, and the sum
  of all seventeen positive `ACT type=4` in the file. There are no negative
  ones.
- **Solution:** `goldens/morning_headache_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `This has turned out to be an altogether OK morning.`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS` +
  `SCR_DUMP_OBJLOC`.

## Route

```
look under bed / take trouser / take alarm clock          (+5)
w                                                          (+3, automatic)
put trouser on woman                                       (+10)
ask woman about phone / kiss woman / eat woman / fuck woman (+10 +2 +2 +2)
turn off alarm clock                                       (+5, before turn 15)
take bottle / take invitation
n / take pepsi / put pepsi in bottle / put water in bottle  (+5 +5)
s / e / take cellular phone / take suit / take boxer shorts
n / wash me                                                (+5, while naked)
s / wear boxer shorts / wear suit
read invitation / x business card / x picture / x book / x window / score
look ×4                                                    (Hanna arrives, +10)
eat hanna                                                  (+10)
s / e / n                                                  (follow her to the bathroom)
ask hanna about invitation                                 (+10, the ring)
call jonas                                                 (+11)
s / w / n / n                                              (Staircase opens)
take shoes / wear shoes / d
give bottle to landlord                                    (+10)
score / open door                                          (+10, EndGame win)
```

## Scoring — all 115

| Task | Command | Points |
|---|---|---|
| 2 | `look under bed` (finds the trouser) | +5 |
| 14 | *automatic:* the buzzer fails to wake her | +3 |
| 4 | `put trouser on woman` | +10 |
| 19 | `ask woman about phone` | +10 |
| 7 / 5 / 6 | `kiss woman` / `eat woman` / `fuck woman` | +2 each |
| 0 | `turn off alarm clock` | +5 |
| 21 | `put pepsi in bottle` | +5 |
| 23 | `put water in bottle` | +5 |
| 35 | `wash me` | +5 |
| 12 | *automatic:* Hanna arrives | +10 |
| 13 | `eat hanna` | +10 |
| 32 | `ask hanna about invitation` | +10 |
| 36 | `call jonas` | +11 |
| 31 | `give bottle to landlord` | +10 |
| 34 | `open door` | +10 |
| | **total** | **115** |

## Four deadlines, all fatal

Everything in this game is on a fixed global clock, and there is no undo.

| Turn | Event | Runs | Kills you if |
|---|---|---|---|
| 15 | 0 `Landlord` | T1 | the alarm clock is still buzzing — *"That's it, you are out off here."* |
| 30 | 3 `Girlfriend` | T11 | the stripper hasn't left (T10 not done) |
| 35 | 4 `Girlfriend2` | T12 | — (+10; **teleports** you and Hanna to the Kitchen from wherever you were) |
| 55 | 7 `JonasIsWaiting` | T30 | you're not at the wedding |

Plus EVENT 2 `DressingUpTheWoman`, which fires **10 turns after** `put
trouser on woman` and sends her home. That is what has to happen before turn
30 — so T4 must land by turn 20, and this route does it on turn 5.

## Order traps

- **`take clock` doesn't work; `take alarm clock` does.** T0 needs the clock
  in hand (`RESTR type=0 v1=3 v2=1`), and the bare noun is ambiguous — you
  get *"Take what?"*, then *"You must take the clock first."*, then evicted.
- **The +3 wants the alarm left on.** T14 (fired by EVENT 5
  `WakeStripperWithBuzzer`, retried every turn from turn 1) needs the alarm
  **not** switched off *and* the woman present: you have to carry the buzzing
  clock into the living room before silencing it. A tidy player who kills the
  alarm on turn 1 loses those three points permanently.
- **Shower before you dress, and before Hanna forgives you.** T35 `wash me`
  needs `task13 NOT done` and the player wearing nothing. Once Hanna arrives
  and you have gone down on her, the +5 is unreachable — and T34 needs T35
  done, so *the game is unwinnable from that point* even though nothing says
  so.
- **`wear suit` needs the boxer shorts on first** (T37 `RESTR type=0 v1=4
  v2=2`). T38 is the same task with a different shorts state; the author
  wrote it twice.
- **The apartment door out is `gateTask=32 wantDone=1`** — Kitchen → Staircase
  opens only after `ask hanna about invitation`, which is also the only way to
  reach the **shoes**, which start on the Staircase and are required to win.
  Until then the room says *"you have no reason to leave the apartment"*.

## Notes

- **The bottle is the landlord puzzle.** The empty Bushmills bottle is on the
  living-room coffee table; T21 pours the glass of pepsi into it and T23 tops
  it up with water from the kitchen sink, producing *"bottle with Bushmills
  mixture"*. T33 `pay landlord` exists and refuses — you have no money. The
  ending admits the con will not hold: *"He is going to evict you for sure."*
- **`open drawer` is a red herring.** T3 has no actions and answers *"you have
  no plans of leaving the apartment yet, and you prefer to walk around in your
  bathrobe as long as possible"*. The suit and boxer shorts are on the
  **chair**, not in the dresser.
- **The author put the hints in the file.** T4 and T12 carry `HINTQ`/`HINT1`/
  `HINT2` fields, and T4's second hint is disarmingly honest: *"Type 'put
  trouser on woman'. You will have to get the trouser first, of course. I
  promise you that this is the only crazy puzzle in the game. I think."*
- **`xyzzy` kills you** (T1, `ACT type=6 v1=1`), and so does `jump window`
  (T9) if the window is open. The `xyzzy2`…`xyzzy8` tasks are not commands at
  all — they are the author's naming convention for event-driven tasks, which
  is why the dump is full of them.
- **63 phantom containers**, all pointing at the couch — the same ADRIFT 3.9
  container-count artefact seen in `ms_mobius.taf`.
- **The synonym table is the funniest thing in the file**: `fuck` → `have sex
  with`, `stripper` → `woman`, `hangover` → `booze`, `her name` → `name`,
  `groom` → `jonas`. The author anticipated the vocabulary his players would
  actually reach for.
