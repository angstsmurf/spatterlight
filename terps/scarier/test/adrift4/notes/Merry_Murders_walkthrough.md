# The Merry Murders — walkthrough (**WIN, 135/135 — full score**)

- **Game:** *The Merry Murders* (`Merry_Murders.taf`, 69,489 bytes), dated
  **December 16, 2003**. `games.manifest.tsv`,
  `https://www.adrift.co/files/games/Merry_Murders.taf`.
- **Engine:** **ADRIFT 3.90** (`SCR_DUMP_TASKS=1` prints `GAME version=390`).
- **Size:** 15 rooms, 76 tasks, 63 objects (54 of them static), 8 NPCs,
  **2 events, 0 variables** — the least machinery of any wired game in the v4
  corpus.
- **Result:** **WIN** through T53 `use needle on eric` (`ACT type=6 v1=0`).
- **Score: 135 out of a declared 135 — every point the file can pay out.**
- **Harness row:**
  `merry_murders_solution.txt|Merry_Murders.taf|Congratulations!|SCR_SKIP_WAITKEY=1`,
  **64 input lines** (62 moves, one `score` check and the final blow),
  PASSing golden.
- **Source:** none. No published walkthrough (Key & Compass, IF Archive, CASA,
  IFDB); adrift.co serves the bare `.taf`. The file carries a per-act hint
  menu which is genuinely useful — it is the only reason the safe combination
  (132) does not have to be brute-forced — and the rest was derived from
  `SCR_DUMP_TASKS`.

You are **Larry James**, a drone at SynTex, stuck at the annual Christmas
party on floor 23. A scream goes up; the boss, Max Thurman, is dead in a
toilet stall; the head of security seals the elevators and then dies himself;
and over seven acts every remaining guest is murdered one at a time by
somebody signing themselves **"Alex Boss"**.

## 135 is the ceiling, and nothing is missable

The file holds exactly **20 `ACT type=4` awards**, they sum to exactly the
declared MaxScore, and — unusually for this corpus — not one of them is
optional, mutually exclusive, or reachable only on a side branch. Every award
is a link in the single chain that advances the plot:

| task | pts | what |
|------|-----|------|
| T0 | +10 | `open stall` — Max's body |
| T7 | +5 | `x pocket` — the gold key |
| T12 | +5 | `unlock door` |
| T9 | +10 | `n` — Barry dead in the chair |
| T15 | +5 | `take file` from the lab trash can |
| T27 | +10 | `research alex` |
| T31 | +10 | `open microwave` — Frank's head |
| T33 | +5 | `use keycard on computer` |
| T35 | +10 | `take syringe` from Ruth's leg |
| T36 | +5 | `enter 132` — the safe |
| T37 | +5 | `read list` |
| T38 | +5 | `show list to trey` — the small key |
| T2 | +5 | `open desk` |
| T43 | +5 | `move pile` |
| T44 | +5 | `open panel` |
| T41 | +10 | `x message` — "Alex Boss was here." |
| T45 | +5 | `ask gina about archives` |
| T46 | +5 | `n` — unlock the archives |
| T49 | +5 | `take journal` |
| T53 | +10 | `use needle on eric` |

The `score` command sits second-to-last in the solution file and reads
**125 out of 135**; the final +10 is paid by the winning blow itself, so it
cannot be observed with `score` at all.

## Seven acts, six hinges

There is no clock and there are no variables. The plot is a chain of six
tasks that each print an act banner, kill the next victim by moving an NPC to
room 13 `[DEAD]` (or hiding them), and teleport the player back to the Plaza:

| hinge | act | victim |
|-------|-----|--------|
| T0 `open stall` | I → II | Max (already dead); Barry seals the elevators |
| T9 `n` | II → III | Barry, a pen in his chest, the phone lines cut |
| T31 `open microwave` | III → IV | Frank — his head is in the microwave |
| T35 `take syringe` | IV → V | Ruth, foaming, a green syringe in her leg |
| T41 `x message` | V → VI | Trey, head through a monitor |
| T50 `read journal` | VI → VII | nobody — the killer is behind you |

The only two events in the file are both trivial. **EVENT 0 `[ACT2]`** fires
one turn after T0 and re-runs T3 `*ACT2`, which just moves the player to the
Plaza again. **EVENT 1 `[End Battle]`** is the game's one real timer and it
exists only in Act VII: it starts when T51 `s` completes, runs `time1=1
time2=8`, and its affected task is T52 `Die`. Its `PauseTask` is 55, which by
the two-based `PauseTask`/`ResumeTask` convention in `scevents.cpp` resolves
to **task 53** — `use needle on eric`. So there are at most eight turns on the
roof, and the only thing that stops the count is the syringe. `attack eric`
(T54) is an instant death.

## The chain, act by act

**Act I.** Plaza → `w` → `se` into the Mens Bathroom, `open stall` (T0, +10).

**Act II.** Back to the stall for `x pocket` (T7, +5) — Max is carrying the
gold key to his own office. East to *Outside Thurman's Office*, `unlock door`
(T12, +5), `n` (T9, +10). Note that T12 is authored `ACT type=0 v1=3 v2=0
v3=0`, i.e. it moves the gold key to room −1: the key is **consumed**. That
matters later, because T2 `open desk` demands the small key held *and* the
gold key **not** held.

**Act III.** The out-of-place trash can in the Computer Lab holds a file that
is "still warm, meaning someone recently just tried to destroy it" (T15, +5);
Topic 3 of the December 10 meeting log names an employee called **Alex Boss**
in connection with missing funds. The desk computer outside Thurman's office
came on when T9 completed (room 11 `ALT v2=10`), so `research alex` (T27, +10)
now returns *"This file is missing from database. Please report to archives to
fix the error."* — and a scream. `open microwave` in the Staff Room (T31, +10,
requires T27).

**Act IV.** T31 moved Eric into the Plaza, where the act dumps you, so
`ask eric about alex` (T32) works on the spot and he hands over his keycard.
`use keycard on computer` (T33, +5) kills Ruth; `take syringe` (T35, +10) in
the Women's Bathroom takes the murder weapon off her body. It is also the
weapon that wins the game.

**Act V.** Taking the syringe is what rips the sales graph off Trey's office
wall and exposes the safe (room 3 `ALT alt=1 v2=36`). `enter 132` (T36, +5) —
the combination is handed over in HINT2 of T35 — yields the employee list;
`read list` (T37, +5) highlights Trey Steele and Alex Boss;
`show list to trey` (T38, +5) buys the small key to Max's desk;
`open desk` (T2, +5) yields Max's own note. `read piece of paper` (T39)
smashes the lock off the janitor's closet — room 1's N exit is
`gateTask=39 wantDone=1`. `move pile` (T43, +5) and `open panel` (T44, +5)
kill Trey. `x message` (T41, +10) reads the wall behind the tipped bookcase.

**Act VI.** `ask gina about archives` (T45, +5, requires T41) gets the archive
key; `n` twice in the Computer Lab (T46, +5, then the exit itself);
`push bookcases` (T48) opens the hole the scratch marks point at; `in`,
`take journal` (T49, +5), `read journal` (T50) — *"I am Alex Boss."*

**Act VII.** `s` (T51, requires T50) takes the elevator to the roof, where
Eric explains that the keycard was the frame-up. `use needle on eric` (T53,
+10) wins.

## Two parser traps, each of which cost a run

1. **`read paper` reads the wrong thing.** It is `ALTCMD[1]` of **T37
   `read list`**, which has a lower index than **T39 `read piece of paper`**
   (whose own `ALTCMD[1]` is `read paper ` — *with a trailing space*).
   Forward first-match dispatch therefore re-reads the employee list, T39
   never completes, and the janitor's closet stays locked for the rest of the
   game with no diagnostic at all. The note must be read as
   `read piece of paper`.
2. **T46 `n` only unlocks the archive door.** Its award (+5) and its message
   ("The lock opened, allowing me access into the archives") both fire while
   the player stays in the Computer Lab. A second `n` walks through.

## Where Trey is

T27 `research alex` is authored `ACT type=1 v1=7 v2=0 v3=3` — "move NPC 5
(Trey) to room 2". But Trey also owns a `WALK` with `startTask=10` (task 9),
which has already fired by then, and he actually ends up in the **Plaza**.
T38 `show list to trey` is `where=2` over `WHERE_ROOMS=[0 2]`, so the Plaza is
a legal venue; showing him the list in the east Hallway just answers
*"Trey's not here!"*. A reminder that an `ACT type=1` NPC move is only the
last word if no walk overrides it.

## `<waitkey>`

The dump reports no `<waitkey>` because `SCR_DUMP_TASKS` writes to stderr and
the tags appear in the transcript. `SCR_MARK_WAITKEY=1` finds **six**, one per
act transition, so the row needs `SCR_SKIP_WAITKEY=1` or the script runs six
lines short.
