# The Test — walkthrough (WINNABLE — old "unwinnable" verdict was wrong)

- **Author:** unknown (a short, whimsical ADRIFT 3.9 "puzzle box"; the narrator
  is a chatty in-game voice). No published walkthrough.
- **Engine:** ADRIFT 3.9 (Battle System on — the Robot Guard really hits you).
- **Result:** **WINNABLE — verified 2026-08-01 both in seeded Scarier
  (`goldens/thetest_win_solution.txt`, PASSing golden) and LIVE in the genuine
  `run390.exe` under Wine, played end to end to "Well done!  You won!  ...
  You scored 20 out of the maximum 25!"** (status bar "Congratulations!").
- The old 5-point tour (`goldens/thetest_solution.txt`) still passes and is
  kept as a second row; its "max reachable 5/25, deterministic, UNWINNABLE"
  verdict below is retracted.

## Why the old "circular lock" analysis was wrong

The earlier revision of this file claimed movement out of Room 0 was sealed:
exit gated on task 15 `#unlockdoor` → restriction "`robot2` == 3 (game
variable 6)" → only writable by `#shoutrobots` → needs the Robot Guard's room
→ behind the door.  Two decode errors produced that circle:

1. **Task 15's variable restriction was misindexed.**  A type-4 (variable)
   task restriction addresses variable `Var1 - 2` (`restr_pass_task_var`,
   `screstrs.cpp` — var1 0/1 are the referenced number/text).  Task 15 has
   `RESTR type=4 v1=6 v2=2 v3=3`: variable index 4 = **`addything` == 3**, not
   `robot2`.  `addything` is exactly the colour-match counter task 14
   (`unlock door`) maintains: `if(%doorcolor%=%keycolor%, %addything%+1, 2-1)`
   — the "decoy" colour minigame is the real lock.  Two consecutive matches
   (addything 1 → 2 → 3) open the door, precisely as the game's own hint
   says: "You will have to put the key in when its the same colour as the
   door.  Twice.  Without getting it wrong in the middle."
2. **Event 2's affected task was misread as `#shoutrobots`.**  The dump's
   `affTask=16` is 1-based: the every-turn "always" event dispatches task 15
   `#unlockdoor` — through the 3.9 matcher (wildcards steal it; that dispatch
   is what makes the fish-face `*` tasks fire every turn), and the moment
   `addything` hits 3 it is what fires `#unlockdoor` unaided.

`#shoutrobots` (task 16) is typeable after all — its ALTCMDs are starred
`%number%` patterns (`shout 3` matches), `where` = all rooms — and `robot2`
(initial 1) is the number to shout: the triangle number of `robot1`, updated
on each success (`robot1+1` while ≤ 5, then `rand(6,20)`;
`robot2 = ((robot1+1)*robot1)/2`).

## The route (20/25; `goldens/thetest_win_solution.txt`)

1. **Room 0 — fluff (+5):** `open clothes`, then 3 × (`take fluff` /
   `drop fluff` / `z`) — the machine sneezes itself to bits — `take key`
   (the colour-changing key).
2. **Room 0 — the colour door (+5):** spam `unlock door`.  Each attempt
   reports the inserted key colour and the door's NEW colour; a match against
   the door's pre-attempt colour bumps `addything`, a mismatch resets it.
   Two matches in a row → "You hear the key fall into some storage space,
   and the door click open.  You walk through."  (~240 attempts under the
   harness seed; RNG-dependent in the real Runner — 10 colours, so expect a
   few hundred.)
3. **Room 1 — the phone (+5):** `dial 987` (the next Fibonacci number after
   the hinted 1,2,3,5,8,…,610 series; the only number `#phonedialsystem`
   accepts).  The call reads the door's serial and speaks the code —
   `enter <code>` (seeded harness: `enter 9303235`), then `east`.
4. **Orange Room — the robot (+5 on first success):** shout the robot's
   triangle numbers whenever it is in your room (it storms in and out every
   couple of turns, hitting you with its chain — battle damage is real but
   ~1/hit vs 100 stamina): `shout 1`, `shout 3`, `shout 6`, `shout 10`,
   `shout 15`, `shout 21`.  A number shouted while the room is empty is
   wasted ("Why shout numbers now?") and the sequence does NOT advance, so
   each number must be spammed until the guard's visit coincides (under the
   harness seed: 9/5/10/5/10/5 attempts — re-timed 2026-08-31 when the
   exact-tick walk gate changed the guard's schedule).  After the 6th
   success (`guard` > 5 → `guard2`=1) the next command is stolen by
   `#findoutsecret`: "…the robot guard just dropped his key."  `take key`
   (again — the first was stolen), `take teleporter`.
5. **Teleport to the Morse Room:** the teleporter refuses in the Orange Room
   (orange paint), so `west` first, then `teleport` repeatedly (random
   destination) until `look` shows the Morse Room.
6. **Morse Room:** the MIDI morse spells the magic word — say `tiddlywink`
   ("your pocket feels slight heavier"), `take tiddlywink`,
   `put tiddlywink in slot` → a new east exit.
7. **Empty Room — win:** `east`, `push south` (the wall swivels, exposing a
   keyhole), `use key with keyhole` → "Well done!  You won!"

The 5 missing points are task 1 `listen` in Room 0 (gated on `#run`), which
this route skips; the endgame reports 20/25, "You finished 5 points short."

## Engine notes

- The wildcard/task machinery this game leans on — the every-turn event
  dispatch through the matcher, `*`-task steals (fish-face lines, the stolen
  `take key`), walk CharTask dispatch — is the run390-verified behaviour
  described in RUNNER_TESTS_TODO.md §2 (commits cff102c9, 3999a6b6).
- The live run390 session used the Wine harness (`~/adrift-battle/runner/wine`)
  with blind `unlock door` spam; the Runner's own RNG differs from the seeded
  harness, so its command counts differ, but every mechanism above fired
  identically.
- 2026-08-31: the golden route was re-derived (191 commands) after the
  exact-tick walk-move gate landed in `scnpcs.cpp` (the Runner-verified rule
  that the whole walk step runs only when `counter == suffix_sum`; see
  WINE-TRANSCRIPTS-TODO.md, Merry_Murders section).  The guard's visit
  schedule under the fixed seed shifted, so every shout was re-timed; the
  mechanism and score (20/25) are unchanged.
