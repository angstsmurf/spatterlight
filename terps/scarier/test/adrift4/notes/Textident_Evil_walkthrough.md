# Textident Evil — walkthrough

- **Engine:** ADRIFT 3.9 (`Textident_Evil.taf`, 20,652 bytes). A *Resident
  Evil* tribute — "a short text adventure designed to test the capabilities of
  the Adrift engine". 11 rooms, 2 NPCs (a mangled zombie, a mutated dog),
  25 tasks, 4 events.
- **Result:** ★ **WON, 100/100** — the game's own stated maximum, and every
  `ACT type=4` in the file.
- **Solution:** `goldens/textident_evil_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `Congratulations! You've successfully beaten Textident Evil.`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS`,
  with the turn alignment brute-forced (see below).

## Route

```
instructions                      (load-bearing: see "The clock")
search wrecked street / take brick / use brick     (+5, +10; smashes the shop window)
w / n                             (Intersection → Highway; the zombie is not yet awake)
search highway / take ammo        (+5)
e                                 (Pawn Shop)
search pawn shop / take pistol    (+5; this starts the zombie's second WALK)
shoot zombie                      (+10)
e / search office                 (+5)
take newspaper / take adrenalin / read newspaper
w / w / use ammo                  (back to the Highway, reload)
n / shoot dog                     (+10; opens End of Highway → Quiet Street)
e / search quiet street           (+5)
take herb / use herb              (heal — this is the survival step)
n / use adrenalin                 (+10; the dying soldier drops the shotgun)
take shotgun / u / south
blast zombie                      (+10)
open window                       (+5)
s / score
up                                (+20, EndGame win)
```

## Scoring

| Task | Command | Points |
|---|---|---|
| 0 | `search wrecked street` | +5 |
| 1 | `use brick` | +10 |
| 23 | `search highway` | +5 |
| 7 | `search pawn shop` | +5 |
| 14 | `shoot zombie` | +10 |
| 8 | `search office` | +5 |
| 22 | `shoot dog` | +10 |
| 11 | `search quiet street` | +5 |
| 2 | `use adrenalin` | +10 |
| 17 | `blast zombie` | +10 |
| 4 | `open window` | +5 |
| 5 | `up` (Fire-escape) | +20 |
| | **total** | **100** |

which is exactly the `out of a maximum of 100` the status line claims.

## The clock — why this route is turn-critical

Four events run on a **fixed global cadence** from turn 0, independently of
where you are; each only bites when its monster shares your room:

| Event | Cadence | Fires | Effect |
|---|---|---|---|
| 0 `Attack` | every 2 turns | TASK 9 | Healed → **Wounded** (zombie present) |
| 1 `Kill` | every 2–6 turns | TASK 13 | Wounded + zombie present → **death** |
| 2 `Attack2` | every 2 turns | TASK 20 | Healed → **Wounded** (dog present) |
| 3 `Kill2` | every 2–6 turns | TASK 21 | Wounded + dog present → **death** |

And the zombie is not a monster you can leave behind. NPC 0 carries **two
walks**, `startTask=2` (TASK 1 `use brick`) and `startTask=8` (TASK 7 `search
pawn shop`), both of which teleport it onto the player every turn once
started. `shoot zombie` does *not* remove it — TASK 14's `ACT type=1` moves it
away and the walk drags it straight back a few turns later. Getting wounded in
the Pawn Shop is unavoidable, because the pistol takes one turn to reveal
(`search`) and another to pick up, and the wound tick lands in between.

So the whole game is a race from `search pawn shop` to `use herb`, and
**adding or removing a single turn anywhere before the dog fight re-rolls
every subsequent event tick.** The opening `instructions` and the exact
command count of the middle section are padding chosen by brute-forcing the
pad sizes at three points; most alignments die, either to the zombie on the
way back through the Pawn Shop or to the dog on arrival at End of Highway.
The transcript is only reproducible because `scare` links the fixed seed.

## Notes

- **Search reveals, it does not take.** The game's own `instructions` say so:
  *"Objects you find will be listed, but remember, you still have to pick
  them up!"* `use brick` before `take brick` answers *"I don't understand what
  you want me to do with the hunk of brick"*, which reads like a parser
  failure rather than a missing-object one.
- **Ammo before the pawn shop.** `search highway` is on the way (Highway is
  east of the Pawn Shop through an ungated exit), and doing it first saves two
  turns off the critical stretch — the pistol is emptied by `shoot zombie` and
  the dog needs it loaded.
- **`blast zombie` must be typed at End of Hall, not on the Fire-escape.**
  TASK 16 (`south` on the Second Floor) explicitly moves the zombie to room 10
  (the Fire-escape) — but the walk immediately drags it back to wherever the
  player actually is, i.e. End of Hall. Waiting until you have climbed out of
  the window loses the +10 to *"There's nothing there! Don't be so twitchy."*
  That cost this route 10 points on the first pass.
- **Only three exits are gated**, all `wantDone=1`: Wrecked Street → Pawn Shop
  by `use brick`, End of Highway → Quiet Street by `shoot dog`, Stinking
  Hallway → Second Floor by `use adrenalin`, plus End of Hall → Fire-escape by
  `open window`. There is no other locked door in the game.
- **One herb, one heal.** TASK 11 puts a single green herb in Quiet Street and
  TASK 12 consumes it. It is behind the dog, so the dog has to be shot while
  Wounded — survivable only because TASK 22 fires before EVENT 3's next tick
  at this alignment.
- The ending is the joke the whole thing is built for: the rescue helicopter
  is *Umbrella-7*, and the pilot is taking you "back to the Island for
  questioning".
