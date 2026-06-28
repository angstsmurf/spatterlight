# The Nonsense Machine 6000 — "Walkthrough"

ADRIFT toy/novelty game (3.5 KB `.taf`). Derived under SCARE (deterministic
seed) and cross-checked against the game's internal structure.

**Result: there is nothing to win — max score is 0 / 0.**

This is not an adventure but a **random-nonsense generator**. Its entire content
is one room, one object, and one task:

- **Room 0** — "Nonsense Room"
- **Static object 0** — "a lever"
- **Task 0** — `*pull*` (pull the lever)

Pulling the lever prints two lines of randomly assembled absurd "news" (the
"up to one million different bits of random nonsense" promised by the intro).
There is no scoring task, no exit, and no ending.

## Full command list

```
pull lever        (prints a random nonsense headline; repeat for more)
pull lever
pull lever
```

## Notes

- The game's maximum score is literally `0`, confirmed by `score` after any
  number of pulls. There is no winning state to reach — the "walkthrough" is
  simply to pull the lever as many times as you like.
- Every pull is reproducible under the deterministic seed; with a real
  (time-seeded) build the output varies, as the title promises.
