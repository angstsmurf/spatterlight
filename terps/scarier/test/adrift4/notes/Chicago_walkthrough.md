# Chicago — walkthrough

- **Engine:** ADRIFT 3.9 (`chicago.taf`, 19,755 bytes). Chicago, 1954: your
  first case as a private eye is Daisy Wild's murdered bootlegger husband
  Donny. 9 rooms, 3 NPCs (Bartender, Shoeshiner, Daisy), 31 tasks, no events,
  no variables.
- **Result:** ★ **WON, 75** — every `ACT type=4` in the file.
- **Solution:** `goldens/chicago_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `Daisy was found guilty of double homicide`.
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS`.

## Route

```
score
take whisky / open desk / take trench coat / wear trench coat
take cigarettes / feel head
n / e / e                             (Street → Front of Office → Alley)
give money to shoeshiner
ask shoeshiner about donny            (he hands over the letter)
read letter
take poster / search boxes
w / n / e                             (→ Dr. Louis Mack's Office)
search body / search desk / search cabinet / take glasses
w / w                                 (→ Donny's Office)
open desk / read paper
search can / search body / open painting
e / s / w / n                         (→ The Jazz Stop)
ask bartender about paper / listen
s / w                                 (→ Donny's House)
take note / read note / listen
score
confront daisy                        (+10, EndGame win)
```

## Scoring

| Task | Command | Room | Points |
|---|---|---|---|
| 1 | `take whisky` | Your Office | +5 |
| 2 | `open desk` | Your Office | +5 |
| 5 | `wear trench coat` | Your Office | +5 |
| 7 | `give money to shoeshiner` | Alley | +5 |
| 10 | `ask shoeshiner about donny` | Alley | +5 |
| 12 | `read letter` | anywhere | +10 |
| 11 | `search body` | Mack's Office | +5 |
| 15 | `open desk` | Donny's Office | +5 |
| 16 | `read paper` | anywhere | +5 |
| 17 | `ask bartender about paper` | The Jazz Stop | +10 |
| 20 | `read note` | anywhere | +5 |
| 23 | `confront daisy` | Donny's House | +10 |
| | **total** | | **75** |

The status line reports *"out of a maximum of 0"* — the author never filled in
a maximum — so 75 is established by counting the twelve `ACT type=4` lines in
the dump, not by the game agreeing.

## The evidence chain

Only two real dependencies exist, and both are `RESTR type=0` "holding"
checks rather than a puzzle:

- `open desk` in **Your Office** yields the money; the money buys the
  **Shoeshiner** (TASK 7, `RESTR type=0 v1=5`), and only then does
  `ask shoeshiner about donny` (TASK 10, `RESTR type=2 v1=8` — TASK 7 done)
  hand over Donny's **letter**: *"if you need to know who did it, you must ask
  Louis."*
- The winning TASK 23 needs the **note** (obj 21, from the ashtray in Donny's
  House) **and** the **letter** (obj 13) in hand. Those are its only two
  restrictions — Mack's needle, Donny's slip of paper and the bartender's
  answer are all optional, they just carry two thirds of the points.

Everything else is a flat, ungated map: nine rooms, sixteen exits, not one
`gateTask=`.

## Notes

- **Three `confront` tasks, two of them fatal.** `conrfont bartender` (TASK 21
  — the author's typo *is* the command; the intended phrasing simply fails)
  and `confront shoeshiner` (TASK 22) both carry `ACT type=6 v1=2`. The game's
  own preamble ("When you believe you have enough, confront who you think it
  is") invites exactly the experiment that kills you, and there is no undo
  short of a restart.
- **`read paper` is farmable.** TASK 16 is `rep=1` and awards +5 on every
  repetition, so the score has no real ceiling. The route reads it once; 75 is
  the honest total.
- **Louis is a corpse before you get there** — `search body` in his office
  finds an empty needle, his desk and cabinet are already ransacked, and the
  letter's "ask Louis" lead is dead on arrival. The needle is never used by
  any task; it is set dressing pointing at Daisy's doctor lover.
- **The solution is entirely in the note**, which is the only thing Daisy
  never thought to burn: *"Daisy, it's over between us… Don't you dare step
  foot in the office again."* The slip of paper (`Donny $1000 Jazz Stop
  Status- Not Paid`) exists to be shown to the bartender, who clears the mob
  of any motive — *"Donny paid me back, he always paid me back."*
- The Office opener is pure atmosphere and pure points: whisky, cigarettes,
  a trench coat to wear, `feel head` for the hangover, and `sleep` (TASK 0) if
  you would rather not solve anything.
