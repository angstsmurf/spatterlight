# The Curse of the Revenge of the Ghost of the Evil Chicken of Doom…Returns! — walkthrough

- **Engine:** ADRIFT 4 (`ECOD2.taf`, **3rd ADRIFT One-Hour Game Competition**,
  2003). The year is 2800, Earth is a memory and you live on McMars. Second of
  the four Evil Chicken games in this corpus (`chicken.taf`, `ECOD2`, `ECOD3`,
  and the ADRIFT 5 `ECOD3D`).
- **Result:** **WON**, 0/0 — no scoring system (no `ChangeScore` anywhere).
- **Solution:** `harness/ecod2_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `has been captured`.

## Structural verdict

28 tasks, 6 rooms. Donny and Jojo are **static objects, not NPCs**. Exactly one
task ends the game: task 1, `push button`, an `EndGame(win)` whose single
restriction is that task 27 (`use CAG on smell-o-matic`) be done. Pushing it
early just phones the Smell Division.

## Route

Three leading blanks ride out the intro's three `<waitkey><cls>` pauses
(*"…make sure when you kill a chicken…"*, *"You finish the job…"*); without them
the first three commands vanish.

```
        ← blank ×3
take book
read book
break ice
open wallet
e
e
talk to donny
2
4
w
n
n
d
take remote
use saw on remote
put battery in microwave
u
s
s
w
use saw on statue
combine battery and spear
use CAG on smell-o-matic
push button
```

## Notes

- **The book in the bookshelf is the walkthrough**, and it is worth reading in
  full: a C.A.G. is *"an antenna (which can be made out of any long metal piece)
  and a melted battery"*, which names all three of the puzzles at once.
- `break ice` (task 7) releases the ghost and starts the plot. Nothing else in
  the game is possible while it is still frozen.
- `open wallet` (task 22) — you start holding the wallet, and this is the only
  route to the credit card.
- `talk to donny` / `2` / `4` (tasks 19 then 21): option 4 is the purchase, and
  its only restriction is the credit card, so the $12,000 saw costs you nothing.
- `use saw on remote` (task 24) — the remote is in Big Al's Bathroom Diner, two
  floors down through a toilet stall — yields the battery;
  `put battery in microwave` (task 25) melts it;
  `use saw on statue` (task 23) takes the top half of Abe Vigoda's spear as the
  antenna; `combine battery and spear` (task 26) builds the C.A.G.
