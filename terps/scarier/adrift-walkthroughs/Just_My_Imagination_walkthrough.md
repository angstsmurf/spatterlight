# Just My Imagination — Musings of a child — walkthrough

- **Engine:** ADRIFT 4 (`Imagination.taf`, **3rd ADRIFT One-Hour Game
  Competition**, 2003). Little Jenny falls down a hole; you are what she finds
  down there.
- **Result:** ★ **WON, 100/100 MAX.**
- **Solution:** `harness/imagination_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `Was this all just in your
  imagination?`

## Structural verdict

6 tasks, 3 rooms, one NPC (Jenny, who follows you). Five of the six tasks award
`ChangeScore(+20)`, so the maximum is 100, and task 5 (`give * ring * girl *`)
is also the `EndGame(win)`.

## Route

```
Jenny
take stick
dig dirt
s
take floss
take rock
tie the string to the rock
throw rock at spoon
stand on spoon
s
open shell
take ring
give ring to girl
```

## Notes

- **The first line is an answer, not a command.** The game asks *"Please enter
  your name:"* before the first room description. The harness strips `#` comment
  lines before feeding the file, so the first *real* line is what answers that
  prompt — hence the explicit `Jenny`. Without it the name prompt swallows
  `take stick` and the run dies at *"You don't have anything suitable for
  digging."*
- **`tie floss to rock` does not match.** Task 1's four command patterns are
  `[tie * string * rock *]`, `[tie * string * floss *]`, `[tie * rock * string *]`
  and `[tie * rock * floss *]` — floss-before-rock only parses if you call the
  floss *string*. `tie the string to the rock` is the phrasing that works.
- Each step is one of the six tasks, in the only order the gates allow:
  `dig dirt` (task 0, needs the stick) opens the room 0 → 1 exit; tasks 1–3 in
  the water pipe end with task 3 opening the 1 → 2 exit; task 4 (`s`) floats you
  and Jenny down to your chamber; task 5 needs the ring from inside the nut
  shell.
