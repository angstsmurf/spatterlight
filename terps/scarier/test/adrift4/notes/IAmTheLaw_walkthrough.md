# I am the Law — walkthrough (**WIN**, 26 commands)

- **Author:** djchallis, *The Odd Competition* 2010 (second place). The Odd
  Comp capped rooms, objects, tasks, events and characters, and the game says
  so in its own preamble. You are Joshua, a detective, on a six-person space
  station where the captain has been murdered.
- **Engine:** **ADRIFT 4.00**
  (`xxd -p -l 12 "games/I am the Law.taf" | cut -c17-22` → `93453e`).
- **Result:** **WON.** No score system (`score` reports 0 of 0); the ending
  ("Congratulations!") is the only measure.
- **Source:** `downloaded/IAmTheLaw_clubfloyd.html`, the 4 September 2010
  ClubFloyd session (same session as the already-wired *Main Course*). It wins,
  but as a group play session it is not replayable. Route derived against the
  task dump, with the password hint taken from the log.
- Row: `law_solution.txt|I am the Law.taf|Congratulations!|SCR_SKIP_WAITKEY=1`.

## The endgame is a three-command variable machine

The accusation is not a puzzle in the usual sense — it is one variable:

| command | condition | effect |
| --- | --- | --- |
| `make verdict` | — | verdict := 2 (prompts for a name) |
| `V` | verdict == 2 | verdict := 4 (prompts for a motive) |
| *any other name* | verdict == 2 | verdict := 3 |
| `mission` | verdict == 4 | **WIN** |
| anything | verdict >= 3 | lose |

So the mechanical minimum is three commands typed at the start of the game:
`make verdict` / `V` / `mission`. Everything else in the route is the actual
investigation, which is what the game is for, and without which the transcript
is meaningless.

## The one thing you have to find

The computer, V, will not talk usefully until her creativity engine is on, and
turning it on needs a password. The prompt is its own small state machine:

```
TASK 5  [enter creativity password]  RESTR var4 == 1   ACT var4 := 2
TASK 6  [grant]                      RESTR var4 == 2   ACT var4 := 3, %text% := "V"
TASK 7  [*]                          RESTR var4 == 2   ACT var4 := 1
```

TASK 7's pattern is a bare `*` — literally any other input — so a wrong guess
silently drops you back out of the prompt with no message saying so. The
password is `grant`, Seth's mother's maiden name, and Luke gives it up in the
Lounge if you `ask luke about password` **after** Calvin has told you the
creativity engine exists. The wired route does both, in that order.

## Shape of it

`goldens/law_solution.txt`, 26 lines:

```
n / w                                     Corridor -> Captain's room
x seth / x bookcase / get diary
read 4th november                         Seth suspected V was lying to him
e / s / e                                 -> Workstations
ask calvin about creativity               there is a creativity engine, it is off
w / w                                     -> Lounge
ask luke about password                   the password is 'Grant'
e / n / n                                 -> Computer room
enter creativity password / grant         V wakes up properly
s / e                                     -> William's room
ask william about behind the curtain      William's confession
w / n                                     back to the Computer room
make verdict / V / mission                -- WIN
```
