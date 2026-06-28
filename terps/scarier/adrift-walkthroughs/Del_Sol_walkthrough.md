# Del Sol (Del Sol Madness) — walkthrough

- **Engine:** ADRIFT 4 (Battle System used in the surreal "dream" sequences).
- **Result:** **UNWINNABLE — no reachable ending. Max score reached by a robust
  deterministic tour: 24/46.** The structural maximum is 46, but the win and the
  final +10 are both orphaned (see below), and the timed class windows / mutually
  exclusive dialogue choices put the remainder out of reach in a single play.
- Solution file: `harness/del_sol_solution.txt` — a 112-line cycling tour (one
  command per turn, repeated across the timed classes so each scoring task fires
  when its room is active). Reaches 24/46 deterministically.

## Premise / structure

You are *Noslen*, surviving one school day across four timed classes — **Wood
shop → Physics → Physical Education → Chemistry** — each of which auto-advances
on a turn timer and is punctuated by a surreal **dream** interlude (the
`WoODsHoP<dream>` / `PhYSiCs<dream>` / `CHeMisTrY<dream>` rooms). Score comes from
in-class antics: `work`, `help`, `take notes`, `pay attention`, `practice`,
`write note to hina`, declining the gamblers (`no`, +10), etc.

## Why it is unwinnable (the broken win chain)

The only win is task 26, **`# super win`**, in the Chemistry-dream room. A full
structural dump shows **task 26 has no trigger whatsoever** — no event
`TaskAffected`, no NPC `KilledTask`, and no other task's exec (type-5) action
points at it. The only thing that even gestures at it is task 27,
**`# bring Hina`** (+10), which is the `KilledTask` of the NPC **Moreland**…

…but **Moreland is dead on arrival.** His battle record is *all zeros, including
Stamina 0*, so SCARE (like the Runner) skips him in combat and he can never be
killed — his `KilledTask` never fires, so the +10 is unreachable and nothing
ever leads toward the win. (The monster actually present in the dream,
`MoReLaND`, is a *different* NPC with `KilledTask = 0` and joke-impossible stats —
Agility 100, Defense 85 — so killing *it* would do nothing anyway.) The dream's
acid/base bottles accept no useful verb. This is an unfinished win, faithful to
the data — not a SCARE bug.

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
banks **24/46** every run; pushing higher would require frame-perfect alignment
to each class's short window for no payoff (there is still no ending).

## Notes

- The classes advance purely on a turn timer — you cannot walk between rooms;
  you wait (or act) and are moved automatically.
- The cycling-tour solution is deliberately timing-agnostic: it repeats the full
  per-class command set many times so each non-repeatable scoring task lands the
  first time its class is active, regardless of how the dream interludes shuffle
  the schedule.
