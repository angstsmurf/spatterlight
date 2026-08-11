# Escape from Insanity — walkthrough

- **Engine:** ADRIFT 3.9 (`Insane.taf`, 12,660 bytes). One room — a padded
  cell in the New Hazefield Institution, where you were committed for calling
  *Kangaroo Jack* "a hopping good time". 20 objects, 31 tasks, no NPCs, no
  events.
- **Result:** ★ **WON, 1000/1000** — the game's single scoring action.
- **Solution:** `goldens/escape_from_insanity_solution.txt` (golden blessed,
  in `run_v4_walkthroughs.sh`). Win marker:
  `Congratulations psychopath, you're now a pyro.`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS`.

## Route

```
<blank ×3>                (title-screen keypresses)
i / score / look / listen / cry / open door
take pipe
use glass with pipe
use pipe on light
use rock on button
use rock on button
sharpen knife
cut cord                  (+1000, EndGame win)
```

## The tool chain

You start holding one item — *a shard of glass* — and every step converts
what you have into the next thing:

| Task | Command | Result |
|---|---|---|
| 2 | `take pipe` | tugs the top pipe off the wall |
| 6 | `use glass with pipe` | glass + pipe → **extended pipe** |
| 8 | `use pipe on light` | smashes the fluorescent; the *chunk of rock* inside falls out (and the pipe is discarded) |
| 11 | `use rock on button` | pries the call **button** off the wall |
| 12 | `use rock on button` (again) | tears the button open → **knife** |
| 13 | `sharpen knife` | sharpens it on the rock |
| 7 | `cut cord` | +1000, sparks, fire, EndGame |

## Notes

- **`use rock on button` twice is not a typo.** TASK 11 and TASK 12 share the
  pattern `use rock * button`. TASK 11 is earlier in the list and `rep=0`, so
  the first attempt fires it (pops the button off) and the second falls
  through to TASK 12, which needs the small button in hand and yields the
  knife. Typing it once leaves you a button short of a knife.
- **TASK 8 needs the *extended* pipe, not the pipe.** Its restriction is on
  object 11; the bare pipe (object 8) cannot reach the ceiling light. `use
  glass with pipe` is the only step that consumes your starting item, and it
  destroys both inputs.
- **There are two winning endings and only one of them scores.** TASK 7
  (`cut cord`, `use knife on cord`) carries `ACT type=6` *and* `ACT type=4
  v1=1000`; TASK 14 (`cut wire`, `cut cable`, `use knife on wire`) has the
  same restrictions and the same EndGame but **no score action**. They differ
  only in which noun you use for the same cord, so a player who types
  `cut wire` wins the game with 0/1000. TASK 7 is earlier in the list, so
  `cut cord` reliably claims the command.
- This is another instance of the pattern in
  `notes/Three_Monkeys_One_Cage_walkthrough.md` / the
  `adrift4-actions-after-endgame` note: the `ACT type=4` sits *after* the
  `ACT type=6` in TASK 7's action list and still runs, so the score reaches
  1000 before the game ends.
- The other 24 tasks are all one-line jokes about the cell (`listen`, `cry`,
  `jump`, `open door`, `smash camera`, `take bar`, `sharpen glass`, …). None
  of them score or change state; the route keeps three so the transcript
  shows the voice.
