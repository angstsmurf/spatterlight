# Too Much Exercise — walkthrough

- **Engine:** ADRIFT 4 (native). Robert Street, Writing Challenges Comp 2006.
  A weight-loss-habitat gag: cook a meal and escape a valley/cave/cottage/hilltop.
- **Result:** **WIN** — ends with `Congratulations!`. Solution:
  `harness/too_much_exercise_solution.txt`; golden
  `too_much_exercise_solution.expected.txt`.
- **Walkthrough source:** David Welbourn's *Key & Compass* (native-ADRIFT solve,
  so commands match the game 1:1 — no port caveat).

## Derivation (the real work — §6 worked example)

The mechanically-extracted `>` command list does **not** win as-is: it desyncs
at `chop tree`. Two interactive `(Press a key)` pauses interrupt the game and
the line-based harness feeds the next *command* into each key-wait, eating moves
and derailing the rest of the run.

Fixes applied to the raw extraction:

- **`chop tree`** triggers **two** `(Press a key)` pauses ("After pushing any
  key twice, the tree will be felled"). Insert **two blank lines** after it.
- **`dance`** triggers **one** `(Press a key)` pause (you collapse on the bed).
  Insert **one blank line** after it. (In the raw run this pause happened to eat
  the harmless `x meat`; the blank keeps intent explicit and the transcript
  stable.)

Every other command maps straight through. Route: examine around the valley →
`in`, grab wallet → `w` into the cave, `put coin in slot` + `dance` for raw meat
→ back to cottage, `drop all` for the climb → `u`, `take axe` + `chop tree`
(fell it) → `push tree` / `roll down hill` → collect stick + branches → cook
(`put branches in fireplace`) → `take all`, `out`, `move boulders`, `w`, `w` to
win.
