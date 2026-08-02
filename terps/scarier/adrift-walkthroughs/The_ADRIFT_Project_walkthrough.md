# The ADRIFT Project — walkthrough

- **Engine:** ADRIFT 4 (`TheADRIFTProject.taf`, "Mystery" / Dana Crane,
  InsideADRIFT Summer Comp 2004). You are beamed aboard the *Beta-Drifter* at
  3 am to find out why the ship's readings are wrong; the culprit is DARWIN, an
  under-tested robot, and the fix is to build a bomb and blow him out of the
  waste dump.
- **Result:** ★ **WON, 90/100 — and 90 is the ceiling.** The author's own run
  ends "You finished 10 points short"; the missing ten really are unreachable.
- **Solution:** `harness/TheADRIFTProject_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `the entire ADRIFT community greet you`.

## Route

```
(2 blank lines — title waitkeys)
Drifter                       <- the game asks for a player name
male                          <- ...then a gender
take suit / wear it / open door / s / s / stand on phone / put battery in charger / n
(2 blank lines)
sw / x table / take cup / x sofa / take string and tin / ne / se / open wardrobe
take gum and orb / chew gum / put gum on string / put gum in drain / nw / sw / open box / unlock box
open it / take remote / put batter in remote / put battery in remote / ne / d / s / s
take pill / open tin / take pill / take slime / put pill in cup / n / n / up
s / s / pour slime in box / put green orb in tube / x tray / take orb / activate orb / twist green orb
where is sweeper / n / n / d / n / up / where is sweeper / n
s / push button / where is darwin / z / z / z / z / z
z / where is darwin / z / zz / z / where is darwin / z / z
n / s / d / s / x vent / up / x screen / score
push button
```

(one command per line in the solution file.) `put batter in remote` and `zz`
are the author's own typos, kept because they cost a turn each and the turn
count matters — see *Repair 2*.

## Repairs to the author's transcript

The route is the author's, from `competition/wthroughs/The ADRIFT Project
Walkthrough.txt`, with three changes.

1. **(RESOLVED 2026-08-02 — the author's order is back.)** A former repair
   moved `take slime` before `put pill in cup`, because TASK 41
   `#Put Pill IN Slime` used to match the put command, fail its restrictions,
   and eat the turn with "Not yet." instead of falling through to the library
   put. Probed live against run400 (`.tas` transplant + probes `FM4`–`FM7`,
   see `RUNNER_TESTS_TODO.md` §4): the real Runner's put-in family runs ahead
   of a matched-but-failing task whenever the put can complete, and the engine
   now does the same, so the author's original order works: the pill goes
   into the cup via the library, and `take slime` (TASK 40) chains the
   radioactive mix. One residual cosmetic difference is blessed into the
   golden: the game's `#Pill Check` zero-length event sees the pill in the
   cup at the end of the put turn itself, so Scarier prints the
   radioactive-mix text (and its +10) one turn earlier than the author's
   transcript — same score, same ending.
2. **One extra `z` before the endgame dash.** The bomb's fuse is genuinely
   random: TASK 38 rerolls `bigboom = random(1,500863)` *every turn*, and the
   blast is what teleports you to the Bridge. Under our pinned seed it lands
   one turn later than it did in the author's live run, so the escape has to
   start one turn later too. Nothing about this is a divergence; it is what a
   seeded RNG does to a transcript recorded against an unseeded one.
3. **A `score` before the winning `push button`**, so the golden records the
   tally (`Your score is 80 out of a maximum of 100`) before the final +10.

## Scoring

Eleven tasks award points and they sum to exactly 100:

| pts | task | trigger |
|---|---|---|
| 5 | 33 `#Charge battery` | `put battery in charger` |
| 5 | 30 `#Chew Gum` | `chew gum` |
| 5 | 31 `#Stick gum on string` | `put gum on string` |
| 5 | 32 `#Put string down drain` | `put gum in drain` |
| 10 | 41 `#Put Pill IN Slime` | pill + slime together in the cup |
| 5 | 43 `#Pour Radioactive slime in box` | `pour slime in box` |
| 5 | 59 `#Green bomb - create` | `put green orb in tube` |
| 10 | 60 `#ActivateGreen Bomb` | `twist green orb` |
| 30 | 64 `#Blow Me Correct Green` | the blast, once DARWIN is in the dump |
| 10 | 65 `#Push Button` | the Secret DRIFT-O-Com, the endgame |
| ~~10~~ | 69 `#Put Transmitter on Darwin` | **unreachable** |

**Task 69 can never fire.** It needs the tracking transmitter held
(`RESTR type=0 v1=17 v2=1 v3=0`), but objects 59 (receiver) and 60
(transmitter) both start hidden — `SCR_DUMP_OBJLOC` reports `pos=-1 room=-1
unmoved=1` for each — and **no task and no event in the file ever moves them
into play**. The only `ACT` that touches the transmitter is task 69's own, which
puts it *on* DARWIN. So the ten points, and the "proper" way to track the robot
with `where is darwin`, were designed and then never wired up. That is why the
author's ending prints "You finished 10 points short", and why 90 is the
maximum a winning — or any — game can reach.

## Notes

- `where is darwin` / `where is sweeper` work anyway, via the *fail* branch of
  TASK 68 `#Where is DARWIN`: it wants the receiver in hand, doesn't get it,
  and prints the location regardless. That is the game's only usable tracker.
  The wording differs from the Runner's (`DARWIN -- The Corridor` vs
  `DARWIN is The Corridor`), which is itself a fail-message-vs-complete-message
  tell.
- The three orb colours are three separate bomb builds (tasks 47/53/59 for
  yellow/blue/green), but only the green one awards points and only the green
  one has an `#ActivateGreen Bomb` follow-up. Yellow and blue are decoys with
  shorter fuses.
- The endgame is a chase in the loosest sense: you wait for DARWIN to wander
  into the ADRIFT-O-Dump (`where is darwin` until it says `ADRIFT-O-Dump`),
  then keep waiting for the fuse.
