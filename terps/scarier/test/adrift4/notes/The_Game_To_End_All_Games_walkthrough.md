# The Game To End All Games — walkthrough

- **Engine:** ADRIFT 4 (`endgame.taf`, **1st ADRIFT One-Hour Game
  Competition**, 2002). A joke game about games that refuse to end: it "wins"
  three times before the real finish.
- **Result:** **WON**, 0/0. The `You scored a total of 4 points in that game!`
  lines are *printed text*, not the status line — there is no `ChangeScore`
  action anywhere in the file.
- **Solution:** `goldens/endgame_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `Really really.`

## Structural verdict

11 tasks, 8 rooms, one NPC (the Search Monster). Only task 7 — `*` in room 7 —
carries `EndGame(win)`. Rooms 0–2 are the deliberately awful "dragun of doom"
game the whole thing is parodying; task 0 (`s` on the Path, restricted to
holding the crystal) is what teleports you out of it and into room 3, the
ADRIFT Main Site. Tasks 1/2/3/5 chain the remaining teleports, and task 6 (`*`
in room 6) re-executes task 5, so the PC switches itself back on no matter what
you type at it.

## Route

Three of these lines are **blank on purpose**:

```
n
n
take crystal
s
s
        ← blank
n
e
search for unmasking the bold
        ← blank
turn on pc
        ← blank
z
```

## Notes

- Each fake ending pauses on `Please press any key to begin...` /
  `Press any key...`, and each pause eats one line of input. Drop the blanks and
  every later command lands one step early. Comment lines are no help — the
  harness strips `#` lines before feeding the file, but preserves blanks, which
  is exactly why blanks are the tool for this job.
- `search for unmasking the bold` is a real ADRIFT-forum thread title; the
  search box on the fake website is the way out of room 4.
