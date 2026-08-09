# The Revenge Of Clueless Bob Newbie! — walkthrough

- **Engine:** ADRIFT 4 (`CBN.taf`, **3rd ADRIFT One-Hour Game Competition**,
  2003). Clueless Bob Newbie breaks into Adventure Corps to deliver his
  masterpiece CD. Sequel: `cbn2.taf`.
- **Result:** ★ **WON, 45/45 MAX**, on the ending the game itself calls *"the
  best possible ending in the game"*.
- **Solution:** `goldens/cbn_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `you excelled yourself`.

## Structural verdict

39 tasks, 12 rooms, one NPC (Rudolph the guard). Nine tasks carry
`ChangeScore(+5)` — 2, 4, 8, 13, 15, 16, 17, 18 and 19 — and nothing else
scores, so the maximum is 45 and this route takes all nine.

Four tasks end the game (22–25, all `EndGame(win)`). Which one fires is decided
by two flags at the moment you finally walk west out of the car park; task 21
executes 22–25 in turn and the first whose restrictions hold prints the ending.
Task 22 — **rules changed AND *101 Classics* smashed** — is the best of the
four.

## Route

Six leading blanks and two mid-route blanks, all load-bearing:

```
        ← blank ×5   (intro "...press a key...")
        ← blank      (throwaway command for task 38)
open door
e
ne
give cd to rudolph
ne
se
x desk
        ← blank
x desk
        ← blank
nw
sw
sw
e
x notepads
um
s
e
x reams
change rules
w
sw
read messages
type eject
smash 101 classics
ne
n
w
ne
ne
e
prise container
w
sw
sw
w
w
```

## Notes

- **Blank lines do two different jobs here.** The intro ends in
  `...press a key...<waitkey>` and the five leading blanks ride that out; the
  sixth is the throwaway command that task 38 turns into the move from room 0
  *[The Story So Far…]* into room 1.
- **`x desk` has a `<waitkey>` in the MIDDLE of its message** — *"Your eagle eye
  sweeps over the desk and finds…`<waitkey>` nothing!"* — and so does the second
  look (*"Again you look and… `<waitkey>`this time you find a pen!"*). Each eats
  the following line of input, so the two desk searches need a blank apiece.
  Without the pen, `change rules` answers *"You've nothing to change them
  with!"* and you silently lose two of the nine awards. This is the footgun that
  taught the corpus to grep the decoded `.taf` for `waitkey` **before** probing.
- `give cd to rudolph` (task 4) is the game's best joke: the CD is not an object
  at all (you are *"carrying nothing"*, and task 1, `drop *cd*`, is a refusal),
  yet giving it works and Rudolph explodes. He otherwise blocks NE out of the
  corridor via a character restriction on task 6.
- `x notepads` (task 16) is where the password `um` is written; typing it
  anywhere (task 17) conjures the crowbar. `type eject` in the Testing Room
  (task 8) produces *101 Classics*; `smash 101 classics` (task 19) sets the
  vandalism flag; `prise container` (task 18, needs the crowbar) is the actual
  mission.
- The win marker is `you excelled yourself` rather than the more obvious *"best
  possible ending in the game"* because that phrase **wraps across a line break**
  in the transcript and `grep -F` would never match it.
