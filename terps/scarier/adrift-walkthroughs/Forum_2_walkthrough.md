# Forum 2 — walkthrough

- **Engine:** ADRIFT 4 (`forum2.taf`, Woodfish, **3rd ADRIFT One-Hour Game
  Competition**, 2003). Sequel to `forum.taf`: King Campbell has been kidnapped
  through a glitch in the Review Forum.
- **Result:** **WON**, 0/0 — no scoring system anywhere; the closing *"You
  scored full points"* is flavour text.
- **Solution:** `harness/forum2_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `***You have won!***`

## Structural verdict

56 tasks, 12 rooms, one NPC (the guard). The win is reaching room 10 *[You have
won!]* via task 38, `say shoehorn`, in the Cell Corridor. The only `EndGame`
action in the entire file is task 49 (`exist`) in room 11, and it is
`EndGame(stop)` — the epilogue, not the ending (see the easter egg below).

## Route

The leading blank and the **two** trailing blanks are all load-bearing:

```
        ← blank
e
x scroll three
attack guard
attack guard
n
u
open blue door
e
open desk
take paper
w
d
pour potion in fountain
drink fountain water
u
smash window
out
w
punch pillar
w
in
say shoehorn
        ← blank
        ← blank
```

## Notes

- **The blanks.** One leading blank rides out the intro's
  `... press any key ...<waitkey><cls>`. The two trailing blanks exist because
  the win message has two `<waitkey>`s buried in the **middle** of it — after
  *"you make it back to the forum"* and after Woodfish's *"It's just one big
  text adventure!"*. Without them the harness's own `quit` and `y` are eaten by
  the pauses instead of the blanks.
- **The game's own built-in walkthrough is wrong on its first move.** Type
  `walkthru` and it tells you to `x third scroll`. Task 3's command pattern is
  `[examine/move/x/l/look at/touch]{the}[third{review}scroll/scroll three/…]`,
  and that first alternative genuinely has no spaces around the optional group,
  so it can only ever match `thirdscroll`/`thirdreviewscroll`. Use a spelled-out
  synonym instead — `x scroll three` (used here), or `third review`, `scroll 3`,
  `review three`, `scroll number three`. Everything else in the built-in
  walkthrough replays accurately.
- `attack guard` twice: task 6 rocks him, task 7 tips him over and he shatters
  — he was a statue all along. Task 11 (`n`) is restricted on task 7, so the
  alley stays shut until the second blow lands.
- `open blue door` sets the door object open; task 24 (`e`) is gated on that
  object state and is one-shot — **it is what puts the red vial in your hands**,
  so the Study visit is mandatory, not optional.
- `open desk` / `take paper` yields *"A word is scribbled across the paper: p/w
  SHOEHORN."* Pure clue: task 38 has no restrictions at all and would accept the
  password even if you never found the note.
- `pour potion in fountain` (task 29) swaps the full vial for the empty one and
  turns the whole fountain red; `drink fountain water` (task 30) is the strength
  potion and the gate on task 35, the pillar. Task 32 (`hit green door`) is the
  *you're too weak* refusal testing the same flag inverted — **the green door is
  scenery**, the Landing has no west exit at all.
- `smash window` (task 33) opens the Landing's `out` exit; `punch pillar` (task
  35) opens room 7's west exit.

## Easter egg

Deliberately not taken, so the golden stays on the canonical ending.
The win screen says *"Type Restore, Restart, Quit, or Cease To Exist."* Task 44
(`cease to exist`) moves you to room 11 *[You have ceased to exist!]*, and task
49 (`exist`) there is the game's one and only `EndGame` — `stop`, not a win.
