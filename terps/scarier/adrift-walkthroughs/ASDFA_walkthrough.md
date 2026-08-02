# A.S.D.F.A. — A Short Damn Fantasy Adventure — walkthrough

- **Engine:** ADRIFT 4 (`asdfa.taf`, **3rd ADRIFT One-Hour Game Competition**,
  2003). You are dreaming, you are in a haunted mansion, and Rancid the butler
  is being unusually attentive.
- **Result:** ★ **WON, 35/35 MAX** (*"Your score is 35 out of a maximum of 35.
  (100%)"*).
- **Solution:** `harness/asdfa_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `bottle of Nightmare Inducer fluid back in his pocket`.

## Structural verdict

35 tasks, 8 rooms, four NPCs (Rancid, Otto, Toto, the Chef). Seven tasks carry
`ChangeScore(+5)` — 2, 4, 7, 13, 15, 18 and 31 — and nothing else scores.
Exactly one task ends the game: task 21, `s` at the Entrance while **holding the
deed**, an `EndGame(win)`.

## Route

```
        ← blank   (intro "...press a key...")
        ← blank   (throwaway command for task 0)
n
ne
x cauldron
open door
ne
sw
sw
s
nw
sit chair
se
e
d
kill toto
u
w
nw
give axe
se
e
d
open coffin
u
w
s
```

## Notes

- **The two leading blanks do different jobs.** The intro ends in
  `...press a key...<waitkey>` — the only `waitkey` in the whole game, confirmed
  by grepping the decoded `.taf`. Task 0 is then `[*]` in room 0 *[The Story So
  Far…]* and its only action is to move you to room 1, so the second blank is
  the throwaway command that buys the walk into the Entrance.
- **Do not examine the shelf.** The Pantry description all but begs you to
  (*"a single shelf that is just crying out for someone to come along and
  examine it"*), and task 5 (`x shelf`) answers *"you find absolutely nothing"*
  while quietly setting the flag that task 6 (`sw failed`) tests. From then on
  leaving the Pantry runs task 6 instead of task 7, and **task 7 is the only
  source of the sword**. Nothing ever clears the flag, task 7 is one-shot, and
  without the sword task 31 (`sit *chair*`) can never fire — which means no
  task 11 (`d`), no cellar, no deed, and the game is **silently unwinnable**.
  Tasks 22/23 (`*cheat*`) are the author's own nod to this: the response depends
  on whether task 6 has already fired.
- `open door` (task 4) is a pure gift of 5 points — the pantry door is not
  actually locked (`ne` works without it), and task 3, the other user of that
  flag, guards a direction the Kitchen does not even have.
- `sit chair` (task 31) needs the sword held; it is what sends Toto down to the
  Cellar and sets the flag task 11 (`d`) tests at the Top Of Steps.
- Leaving the Sitting Room (task 8) puts Otto in it, and task 9 (`nw`,
  restricted on task 8) is the way back to him. This route does the cellar trip
  in between, so the axe is already in hand when Otto turns up.
- `give axe` (task 15) is the switch on the coffin: tasks 16/17/19 all require
  task 15 **not** done and are refusals; task 18 wants it done and yields the
  deed.
