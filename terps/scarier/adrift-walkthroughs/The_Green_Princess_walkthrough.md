# The Green Princess — walkthrough

- **Engine:** ADRIFT 4 (`frog.taf`, Rich Pizor, **1st ADRIFT One-Hour Game
  Competition**, 2002). You are the frog Reggie's mate; a princess kissed him
  into a prince and you want him back.
- **Result:** **WON**, 0/0 — there is no scoring system (not one `ChangeScore`
  action in the file).
- **Solution:** `harness/frog_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `So you hop away with your fairy princess, to live hoppily ever after.`

## Structural verdict

7 tasks, 5 rooms, 3 NPCs. The single `EndGame(win)` is task 6,
`* flick * tongue *` in the throne room. `flick tongue` is effectively the
game's only verb — it opens the game and it closes it.

## Route

```
flick tongue
e
e
e
x mud
take hat
wear hat
s
s
flick tongue
```

## Notes

- **The first `e` does not move you**, and this cost half an hour to spot. Task
  1 (`* e *`) matches it, prints *"You hop off to confront the princess."* and
  has **no action at all** — while task 0's alternate room description renames
  the lily pad to *Pond*, so the transcript reads exactly like a successful
  move. You are still in room 0. A second `e` reaches the real Pond and a third
  the Shore.
- The way to check that class of illusion is `SCR_TRACE_FLAGS=16`, which prints
  `Library: moving player from X to Y` for genuine moves. `SCR_TRACE_PLAYER`
  prints the room at the **end** of the turn, which reads as an off-by-one turn
  if you assume otherwise.
- The hat is buried in the mud on the Shore and is never listed in the room
  description; `x mud` is what reveals it. Wearing it is what gets you past the
  guards into the throne room.
