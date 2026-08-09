# Del Sol (Del Sol Madness) — walkthrough

- **Engine:** ADRIFT 4 (Battle System used in the surreal "dream" sequences).
- **Result:** **UNWINNABLE — no reachable ending. Max score reached by a robust
  deterministic tour: 26/46.** The structural maximum is 46, but the win task
  can only fire in a room its trigger can never fire from, the final +10 is
  orphaned (see below), and the timed class windows / mutually exclusive
  dialogue choices put the remainder out of reach in a single play.
- Solution file: `goldens/del_sol_solution.txt` — a 112-line cycling tour (one
  command per turn, repeated across the timed classes so each scoring task fires
  when its room is active). Reaches 26/46 deterministically.

## Premise / structure

You are *Noslen*, surviving one school day across four timed classes — **Wood
shop → Physics → Physical Education → Chemistry** — each of which auto-advances
on a turn timer and is punctuated by a surreal **dream** interlude (the
`WoODsHoP<dream>` / `PhYSiCs<dream>` / `CHeMisTrY<dream>` rooms). Score comes from
in-class antics: `work`, `help`, `take notes`, `pay attention`, `practice`,
`write note to hina`, declining the gamblers (`no`, +10), etc.

## Why it is unwinnable (the broken win chain) — re-verified live 2026-08-02

The only win is task 26, **`# super win`** (`ACT type=6 v1=0` = ENDGAME win),
whose `Where` list is **room 6 only** (the `CHeMisTrY <dream>` room). It *does*
have a trigger — an earlier version of this file claimed it had none, an
off-by-one misread from before the dumper's index offsets were nailed down:
**task 26 is the `KilledTask` of NPC 9, Ms. Moreland** (the teacher, room 3 =
Chemistry, all-zero battle stats including Stamina 0). Task 27 `# bring hina`
(+10) is referenced by nothing at all and stays orphaned.

And Moreland **can be killed** — the "the Runner skips stamina-0 NPCs in
combat" rule only governs *NPC target selection*; a player-initiated attack has
no such guard. `attack moreland` on arrival in Chemistry lands (her Agility is
0), any positive damage drops her 0 Stamina to death, and her `KilledTask`
dispatches. Verified live in run400 (Scarier `.tas` transplanted at the
Chemistry arrival turn): "You hit Moreland with the backpack." — and she is
silently removed from the room.

**But the win still never fires, because the Runner gates task dispatch on the
task's room list.** Task 26 is only runnable in room 6; Moreland can only ever
be killed in room 3 (she never moves, and nothing else can damage her), so the
dispatch is silently discarded — no death line (a set KilledTask replaces it),
no win, Score 0. Probed live in run400: after the kill the chem dream still
arrives, still says "Ms moreland just accidentally knocked over the lab
stuff…", and loops forever. The dream's `MoReLaND` (NPC 10) has
`KilledTask = 0`, so killing *it* does nothing; the acid/base bottles accept no
useful verb.

Nor can the player just type the command: run400's input handler strips leading
`#` characters (`Text1_KeyPress`: `While Left(input,1)="#" : input=Right(…)`),
so `# super win` arrives as ` super win` → "I don't understand" (probed live in
the dream room; Scarier's parser equivalently excludes `SPECIAL_PATTERN` tasks
from player matching, scrunner.cpp). So the win is authored, wired, and
unreachable: **unwinnable, faithful to the Runner.**

**Scarier divergence found by this probe — FIXED 2026-08-02:** Scarier used to
run a KilledTask unconditionally (`battle_kill` → `task_run_task`), so
`attack moreland` in Chemistry *won the game* under Scarier. Both battle-task
channels (KilledTask, StaminaTask) now gate on
`task_can_run_task_directional`, the same gate as the type-5 exec channel —
both halves Runner-verified live (room list: Del Sol; done-state: arena probe
KT2, see `RUNNER_TESTS_TODO.md`). Scarier now prints exactly what run400
prints on this kill: "You hit Moreland with the backpack." and nothing else.

So the reachable game is a **0-ending, score-only sandbox**: you accrue points
through the day and then the Chemistry nightmare simply loops (the monsters can't
hurt you — the player's Agility 40 beats their Accuracy, "it doesn't seem to do
any damage").

## Why 24, not 36

With task 27's +10 unreachable, the structural ceiling is 36. The rest is lost to
**timed windows** (each class is only open for a handful of turns before the
dream/next-class events fire, and the dream interludes shift the timing
unpredictably) and **mutually exclusive choices** (e.g. accepting `gamble` +1 vs
declining `no` +10; `pass note` vs `write note to hina`; the various `make out`
flirtations vs Noslen's standing with Hina). The banked cycling tour reliably
banks **26/46** every run; pushing higher would require frame-perfect alignment
to each class's short window for no payoff (there is still no ending).

The tour banked 24/46 until 2026-08-01, when Scarier's "held by the player"
restriction was corrected to match the Runner (an object inside a container the
player carries or wears counts as held — see `ADRIFT4_vs_ADRIFT5.md` §6). Task 6
requires "pencil held by the player" and the pencil lives inside the carried
backpack, so `take notes` now succeeds in Physics instead of answering "Take
what?", for +2. Verified on the real 4.0 Runner by transplanting a Scarier-written
`.tas` save 60 turns in and replaying the turn: it answers "Staring at the
direction of the teacher, you write the stuff you see on the board." at Score 8,
exactly as Scarier now does.

## Notes

- The classes advance purely on a turn timer — you cannot walk between rooms;
  you wait (or act) and are moved automatically.
- The cycling-tour solution is deliberately timing-agnostic: it repeats the full
  per-class command set many times so each non-repeatable scoring task lands the
  first time its class is active, regardless of how the dream interludes shuffle
  the schedule.
