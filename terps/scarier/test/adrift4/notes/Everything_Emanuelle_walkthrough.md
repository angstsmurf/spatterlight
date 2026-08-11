# Everything Emanuelle — walkthrough

- **Engine:** ADRIFT 3.9 (`everything.taf`, 20,065 bytes), Stewart J. McAbney,
  subtitled *An Experiment With Character*. **One room** — the dead
  Emanuelle's Bed Chamber — 34 objects, 16 tasks, 2 events, 3 variables, no
  NPCs, no exits but `out`.
- **Result:** ★ **completed at the fullest ending.** There is no score at all
  (no `ACT type=4` anywhere), and TASK 15 (`out`) is unrestricted, so the game
  can be "won" on turn one. What the route maximises is which of the four
  *written* endings you earn.
- **Solution:** `goldens/everything_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `I'll smile as I curse her name and everything Emanuelle.`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS`.

## Route

```
<blank ×2>                       (title screen + prologue keypresses)
x dust / x bed / x sheets / x pillows / x posters / x portraits
x blooms / x window / x wind / x curtains
x wardrobes / x clothing / x dresses / x gowns / x leisurewear
x table / x mirror / x stool / x hairbrushes / x perfumes / x creams
n / u                            (the two "I can't leave yet" refusals)
open top drawer
open middle drawer               (starts EVENT 1 → the necklace)
open bottom drawer               (starts EVENT 0 → the wig)
x mirror                         (the turn on which both events fire)
x wig / x necklace / x key
close top drawer / close middle drawer / close bottom drawer
look under pillow                (the diary)
i
open diary with key
read diary                       (%opinion% := 5)
out                              (EndGame win, the title-drop ending)
```

## The whole game is one variable

`%opinion%` starts at 0 and TASK 15's text is an ALT chain keyed on it:

| `%opinion%` | How you get there | Ending |
|---|---|---|
| 0 | leave at once | *"I will always love her. And nothing would change that!"* |
| 1 | the wig **or** the necklace | *"…albeit with certain doubts. Those doubts, however, will soon subside"* |
| 2 | the wig **and** the necklace | *"…albeit with severe questions to be answered — the wig, the necklace, the woman herself"* |
| 5 | plus the diary, read | the full title-drop: *"…I'll smile as I curse her name and everything Emanuelle."* |
| 6, 7 | the diary **first**, then a drawer | the literal string `ending6` / `ending7` |

**TASK 14 (`read diary`) sets `%opinion%` to 5 — it does not add to it.** So
the order matters in exactly the opposite way to the intuition: open the
drawers *first* and read the diary *last*. Reading it first and then finding
the wig and necklace pushes the counter to 6 or 7, and those ALTs were never
written — the game prints the author's placeholder text `ending6` / `ending7`
and then ends normally. That is a genuine authoring bug, easy to hit and
invisible unless you know the variable exists.

## How the wig and the necklace appear

Neither is in the room to start with. The two drawers are `RESTR type=1`
open/closed state checks with `ACT type=2` state changes, and each *starts an
event*:

| Event | Started by | Fires (after 1 turn) | Effect |
|---|---|---|---|
| 0 `## Find A Wig` | TASK 4 `open bottom drawer` | TASK 8 | `%opinion% + 1`, reveals the wig |
| 1 `## Find a necklace` | TASK 3 `open middle drawer` | TASK 9 | `%opinion% + 1`, reveals the necklace |

Both are `time1=0 time2=1`, so their text lands on the turn *after* the drawer
opens — which is why the route spends a turn on `x mirror` before examining
what it just found. The top drawer (TASK 2/5) opens and closes and contains
nothing; it exists so the dressing table has three.

The **key** hangs from the necklace ("Strange that I should never have noticed
the small key that depends from it") and unlocks the diary via TASK 11.

## Notes

- **`out` is unrestricted.** TASK 15 has `restr=0` and its `ACT type=6 v1=0`
  is the game's only ending; TASK 0 and TASK 1 exist purely to intercept every
  other direction word (44 alternatives between them) with *"I can't leave my
  Emanuelle's bed chamber just yet."* The single `EXIT room=0 OUT` points back
  at room 0 and is never used.
- **`read diary` does not require unlocking it.** TASK 14 has no restrictions,
  so `read diary` works straight out from under the pillow; `open diary with
  key` (TASK 11) is a pure flourish. The route does it anyway, because the
  key's discovery is the point of the necklace.
- **TASK 10 is dead code.** `## Light candle with lighter` needs objects 29
  (candle) and 30 (lighter) in hand, and neither object is placed anywhere in
  the game — `take candle` answers *"Take what?"*. `%candle%` (variable 0) is
  likewise never read. A cut puzzle left in the file.
- **Two `<waitkey>` pauses before the first prompt**, located with
  `SCR_MARK_WAITKEY=1`: the title screen and the prologue. Without the two
  leading blank lines the first two commands of any script are swallowed.
- The prose is the game. Twenty-odd scenery objects each carry a paragraph of
  the widower's increasingly uncomfortable devotion, and the route examines
  all of them so the transcript records the voice the endings are reacting to.
