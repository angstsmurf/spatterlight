# Adriftorama — walkthrough

- **Engine:** ADRIFT 4. A novelty **18-hole mini-golf** course, each hole themed
  after a member of the ADRIFT community forums ("Bob's Hole", "The Mad Monk's
  Hole", "KF's Hole", "Guess The Verb Mountain Hole", etc.).
- **Result:** **No ADRIFT-score and no `var1=0` win — one lose ending only.**
  Playable as golf, but there is no victory state in the engine sense.

## Structural verdict

51 tasks, **20 rooms** (18 holes + framing), 4 events, 1 NPC. The dump shows
**zero `ChangeScore` (type-4) actions** — the game keeps your *golf strokes* in
its own **variables**, not the ADRIFT score system, so `score` always reads
0/0. There is exactly **one `EndGame` action and it is a `var1=1` lose** (run out
of balls / fail), with **no `var1=0` victory ending** anywhere. So in the
engine's terms the game cannot be "won"; you simply play the holes (buying a
ball, then `throw ball` / `hit ball` / `throw club` per hole — tasks
`#Buy Ball`, `#Throw Ball`, `#Hit Ball`, and a per-hole pair of `…'s Hole` /
`… CLUB` tasks for all 18 named holes) and the only scripted ending is the
failure stop.

## Notes

- Each of the 18 holes is a one-room task pair: a "Hole" task (sinking the ball)
  and a "CLUB" task (retrieving/throwing the club). Progress is tracked entirely
  in golf-stroke variables.
- Because the win/lose logic is variable-driven with only a lose `EndGame`, this
  is documented as a **no-victory novelty game** (analogous to the other
  variable-only sandboxes in this corpus). The "goal" is the lowest stroke count,
  which the ADRIFT score readout does not reflect.
