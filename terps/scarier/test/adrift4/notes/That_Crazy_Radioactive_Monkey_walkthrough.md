# That Crazy Radioactive Monkey! — walkthrough

- **Engine:** ADRIFT 4 (`CRM.taf`, **3rd ADRIFT One-Hour Game Competition**,
  2003). A sitcom filmed in front of a live studio audience; your job is to
  make 'em laugh, and the laughs are all at Jojo's expense.
- **Result:** ★ **WON, 25/25 MAX.**
- **Solution:** `goldens/crm_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `You take a long bow as the curtains close for the show, and the dead body`.

## Structural verdict

29 tasks, 4 rooms. Jojo is **scenery, not an NPC** — three separate static
`jojo` objects, one per set he appears on. Three tasks score: task 5 (`hit
jojo`, +5), task 14 (`put pie on spring`, +10) and task 25 (`put banana in
pipe`, +10), which is also the `EndGame(win)`. Maximum 25.

## Route

```
take spring
use phone
dial 555-5555
take matches
talk to jojo
use spring on jojo
push sofa
hit jojo
n
open oven
take glue
read box
put spring on glue
put pie on spring
        ← blank
        ← blank
s
e
light toilet on fire
w
w
hit car
put banana in pipe
```

## Notes

- The prank chain is a strict order, each link gated on the previous one:
  - `put spring on glue` (task 13) rigs the fridge door;
  - `put pie on spring` (task 14, needs 13) puts the pie in Jojo's face and
    sends him to the bathroom sink;
  - `light toilet on fire` (task 22, needs the matches held **and** task 14)
    makes him slip in his own puddle and flee to the garage;
  - `w` into the living room (task 23, needs 22) — only then does the Garage
    exist at all; the task teleports you;
  - `put banana in pipe` (task 25) finishes it. The banana comes from task 16,
    `dial 555-5555`, the number printed on the cereal box.
- **The two blank lines after `put pie on spring` are load-bearing.** The
  pie-in-the-face cutscene contains two keypress pauses, and each swallows one
  line of input. Without the blanks the following `s` and `e` are eaten and you
  try to light the toilet from the kitchen (*"You can't light that"*).
