# Ebony's World — walkthrough (**UNWINNABLE as authored**)

- **Engine:** ADRIFT 4.0. A short comic fantasy adventure (x-ray specs,
  casino chips, a seaman's boat, a captured "strange animal," an elixir
  that floats the player through a ceiling, and a pipe-room control puzzle
  gating a final "Conference room" scene with an NPC named Bardo). No
  sexual content, no minors.
- **Result:** **UNWINNABLE**. The final task, TASK 26 (`flip switch`, in the
  Conference room), is gated on four named variables reading specific
  values: `dial1==3`, `dial2==2`, `lever==2`, `valve==2`. Confirmed with
  `SCR_TRACE_VARS=all` and `SCR_DUMP_TASKS=1`:
  - The pipe-room walkthrough (`turn dial to high`, `turn dial to on`,
    `flip lever up`) correctly sets `dial1`, `dial2` and `lever` to the
    required values.
  - `valve` never leaves its default of `1`. Every task that could plausibly
    touch it — both `flip lever up`/`flip lever down` **and** `turn valve
    on`/`turn valve off` — writes to variable index 2 (`lever`) only; no
    `ACT` anywhere in the game's task table (`SCR_DUMP_TASKS=1`, all 9
    `ACT type=3` variable-assignment lines) ever targets variable index 3
    (`valve`). The "valve" object/verb is a red herring that happens to
    print flavor text ("you switch the valve on") while silently mutating
    the *lever's* variable instead of its own.
  - `flip switch` therefore always answers "you flip it but nothing
    happens something must not be right so you put it back", regardless of
    command order or which of the redundant lever/valve phrasings is used.
  - This is the same class of bug as `bedlam_solution.txt` (Bedlam) and
    `Sandy.taf` — a required game-state variable that no task in the
    finished game ever actually sets — not a Scarier defect.
- **Wired as:**
  `ebonysworld_solution.txt|ebonysworld.taf|Your score is 1450 out of a maximum of 0.`,
  no env. (The game itself never sets a max-score total, hence "maximum of
  0" even with a nonzero score — this is the game's own scorer output, not
  a Scarier artifact.) The walkthrough solves every other puzzle in the
  game — the x-ray specs and key, the casino-chip-for-beer swap, giving
  beer to the seaman and sailing his boat, capturing the strange animal and
  caging it, the reeds/pond/elixir sequence and the ceiling float, and the
  full (correct but ultimately insufficient) pipe-room startup — before
  confirming the dead end at `flip switch`.

## The walkthrough

```
move picture
get x-ray specs
wear x-ray specs
n
get key
use key with lock
s
w
s
get sharp stick
s
w
get casino chips
get bottle
n
swap chips for beer
put beer in bottle
slide rug
d
w
n
w
u
give beer to seaman
open boat
sail boat
w
n
take animal
d
use sharp stick with strange square
put animal in cage
turn switch
w
get reeds
swim in pond
w
get HAR elexir
e
n
u
w
lie on bed
pull cord
x urn
get warp stone quarter
drink HAR elexir
e
n
turn dial to high
turn dial to on
flip lever up
turn valve on
s
e
flip switch
score
```
