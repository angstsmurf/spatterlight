# Undefined — walkthrough

- **Engine:** ADRIFT 4 (`Undefined1.taf`, ScummVM gameid `1h_undefined`). A
  1-Hour Game Comp entry: one room called *Nowhere*, one disembodied voice, and
  the verb `define`.
- **Result:** ★ **WON, 3/3 MAX.**
- **Solution:** `goldens/undefined_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `An end is defined.`

## Structural verdict

14 tasks, one room, no objects but `darkness` and `hands`. Three `+1`
`ChangeScore` tasks and one `EndGame(win)`:

| task | command | effect |
|---|---|---|
| 2 | `define the room` | +1, "Here defined." |
| 3 | `define me` | +1, "You are defined." |
| 4 | `define voice` | +1, "I am now defined (thank you by the way)." |
| 9 | `define the end` | `ACT type=6 v1=0` — "An end is defined. You are somewhere." |

## Route

```
Undef            <- answers "Please enter your name:"
define the room
define me
define voice
define the end
```

## Notes

- **`define you` does not score.** Task 1 is a catch-all `[* you *]` that sits
  *earlier* in the table and steals the command, printing the long "when you say
  'you' I assume you are referring to me…" speech. Task 4 also accepts `voice`,
  which task 1's pattern cannot match — that is the way in.
- The game **overrides `quit`** (task 6: *"You can't quit! You just got here…
  You cannot quit until the end, and the end much like the beginning is
  undefined."*), so it cannot be quit before winning. The harness's trailing
  `quit` lands on the post-game prompt instead, which is fine.
- The name prompt takes the *first* line of the solution; there is no press-a-key
  pause in front of it.
