# The Cat in the Tree — walkthrough

- **Author:** Artemis (2004). Entry for the One-Hour Mini-Comp. ADRIFT 4.00.
- **Result:** **WIN, full 50/50 (100%), deterministic.** A two-room comedy: get
  a cat down from a maple tree for an attractive woman (with a punchline ending —
  it turns out not to be her cat).
- Solution file: `harness/the_cat_in_the_tree_solution.txt` (no name/gender
  prompts).

## Scope

15 tasks, 2 rooms (the *Sidewalk*, room 0, and *In a Tree*, room 1). The only
`ChangeScore` action is the +50 on offering the milk to the cat (task 12); the
only win `EndGame` is climbing back down with the cat (task 8). So 50 is the
maximum.

## The win (full 50/50)

```
z                          <- wait one turn so Huey the Contractor walks up
ask huey for ladder        <- borrow his ladder
lean ladder against tree   <- place it against the trunk
ask girl for milk          <- the little girl sells you a cup of milk ("25 cents")
climb up ladder            <- you reach the top, but the cat saunters out of reach
climb up tree              <- THIS is what actually puts you up in the branches
                              (room 1); climbing the ladder alone only advances the
                              state, it doesn't move you
offer cup of milk to cat   <- +50; the cat approaches, laps the milk, and you scoop
                              it up in the same action
get cat                    <- (already in hand from the milk lure; harmless to repeat)
climb down ladder          <- the win: "You rescued the cat!" → Congratulations!
```

## Gotchas

- **Two climbs, not one.** There is a deliberate two-step state machine: `climb
  up ladder` only sets the progress flag (and prints the "cat is still out of
  reach" gag); you must then `climb up tree` to enter the *In a Tree* room where
  the cat and milk tasks live. Doing only one leaves you stranded on the sidewalk.
- **Say "cup of milk", not "milk".** The offer task's noun alternation accepts
  `cup of milk` / `cup` / `mil` but **not** the bare word `milk` — `offer milk to
  cat` parses as an unknown object.
- **NPCs arrive on a timer.** Huey isn't present on turn 0; the leading `z`
  (wait) lets him walk up before you ask him for the ladder.
- The milk-offer both scores the +50 *and* grabs the cat, so the separate `get
  cat` is redundant but harmless.
