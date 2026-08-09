# Wax Worx — walkthrough

- **Engine:** ADRIFT 4 (`wax_worx.taf`, Eric Mayer, InsideADRIFT Spring Comp
  2004). A one-room-at-a-time noir: you wake up on the floor of a wax museum
  with no memory, and reconstruct a murder.
- **Result:** **WON** (the game's single ending). There is **no scoring system
  at all** — every task in the dump carries `score=0` and the game never prints
  a score line.
- **Solution:** `goldens/wax_worx_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `[PRESS ANY KEY TO DIE]`.

## Route

Straight from the author's own walkthrough, which ships in the comp package as
`springcomp04.zip → walkthru/walkthru/wt-waxwurx.txt`. It replays verbatim with
no divergence — no rephrasing was needed, which is unusual enough to be worth
recording.

```
stand
open wooden door with credit card
w
look under shelf
move boxes
crawl
ask marie about wily
get marie
crawl
e
get axe
e
e
x bushes
n
ask charlie about house
e
put nose on clown
n
x floorboards
hit floorboards with axe
d
creepy crawl
u
give marie to dog
g
g
s
e
x sideboard
get wallet
open wallet
x license
z
z
kill
z
z
z
z
z
```

## Notes

- `give marie to dog` / `g` / `g` — the repeats really are *again*, and each one
  is a distinct feeding turn. (Whether `g` means *again* or *get* is a recurring
  ADRIFT trap; here the engine is right, see the `scare-g-means-get` note.)
- The ending is not a victory. `x license` is the reveal — the name on the
  driver's licence in the wallet is **yours** — and the game then runs you
  through the trial and the electric chair. "Winning" is being told you are the
  murderer and being executed for it.
- `[PRESS ANY KEY TO DIE]` is the **last text the game ever prints**: the
  keypress fires the `EndGame`, so nothing after it reaches the parser. Verified
  by appending distinct probe tokens — none of them come back. That is why the
  row's win marker is that line and not something from an epilogue.
- The trailing `z`s are the timed cut-scene at Old Sparky; the game drives
  itself once `kill` is entered.
