# Mortality — walkthrough (**one of the two good endings — verbatim replay**)

- **Author:** David Whyld, started December 2003, finished August 2005.
- **Engine:** **ADRIFT 4.00** (`xxd -l 16 mortality.taf` → `… c2 cf 93 45 3e 61 …`).
- **Size** (the game's own "Game information" screen): *KB: 93; Rooms: 32;
  Objects: 139; Tasks: 476; Events: 5; Characters: 4.*
- **Result:** reaches **one of the two good endings**, verified in seeded
  Scarier (`harness/mortality_solution.txt`, PASSing golden, win marker
  `one of the two good endings`, env `SCR_SKIP_WAITKEY=1`).
- **Score:** there is none. `score` answers **"No one is keeping score."** and
  `SCR_DUMP_TASKS` finds **zero `ACT type=4`** in all 476 tasks, so reaching
  the ending is the only measure of the route.
- **Source:** `downloaded/Mortality_walkthrough.txt` — the game's own doc file,
  which contains a complete `run400.exe` session transcript. **All 78 commands
  replay verbatim, with no repairs**, and the responses are word-for-word
  identical to the published ones (word-level diff ratio 0.9952; every
  difference is an echoed command line, which the headless build does not
  echo). Zero parser errors anywhere in the transcript.

## What kind of game this is

*Mortality* is a menu-driven noir novella rather than a puzzle game: Steve
Rogers is hired as bodyguard to the ageing Wilfred Gamble, murders him at the
behest of Gamble's wife Stephanie, and then discovers that Gamble is not
staying dead. Most turns are numbered dialogue choices; the handful of real
commands are the physical beats between them. Two of the 78 commands are the
title menu (`1` = "Begin Mortality") and the final choice; 55 of the rest are
menu digits.

## The route (78 commands, in order)

| # | Command | Beat |
| --- | --- | --- |
| 1 | `1` | title menu → Begin Mortality (the cemetery, after the funeral) |
| 2 | `e` | leave the cemetery with Stephanie |
| 3 | `2` | in the car: "he's old and senile, so an accident is the likely way" |
| 4 | `wait` | Wilfred Gamble limps in |
| 5–6 | `1` `1` | the interview; you are hired, "you start now" |
| 7 | `se` | balcony overlooking the gardens — first scene alone with Stephanie |
| 8 | `3` | the drive: the answer that makes Stephanie laugh ("you've got balls") |
| 9 | `out` | out of the car at the nightclub |
| 10 | `ne` | through into the second room |
| 11 | `2` | back Stephanie against Crimmons |
| 12 | `hit boxer` | drop the bouncer |
| 13 | `open door` | **the murder night** — out of bed without waking Stephanie |
| 14 | `s` | the corridor with the ceiling trapdoor |
| 15 | `x trapdoor` | reveals a chain |
| 16 | `pull chain` | the ladder slides down |
| 17 | `u` | the attic (its light is already on — nobody has been up here) |
| 18 | `l` | look: the junk, and the chest |
| 19 | `get boxes` | something unseen shoves you — the first haunting |
| 20–26 | `1` `2` `2` `2` `1` `1` `3` | the inspector's first interview |
| 27–30 | `wait` ×4 | wait in the shadows until Gamble comes down the stairs |
| 31 | `open chest` | stiff hinges, but it opens |
| 32 | `get diary` | Gamble's diary — the blackmail material |
| 33 | `2` | "I'm not paying a fucking thing" |
| 34 | `e` | the dining room; a door swings shut on the far side |
| 35 | `n` | the dark corridor |
| 36 | `3` | lunge at the figure in the shadows |
| 37–41 | `2` `1` `1` `1` `2` | the inspector's second interview (he has read the coroner's report, and your service file) |
| 42 | `talk to stephanie` | she is shaking with rage |
| 43–47 | `1` `1` `1` `1` `2` | talk her down, then let her ask about the pub brawl |
| 48 | `stand` | **the flashback** — leave your seat and stand at the bar |
| 49–51 | `wait` ×3 | Seamus O'Riley bangs in and picks the fight |
| 52 | `kill seamus` | the killing the inspector was fishing for |
| 53–56 | `wait` ×4 | back in the present; a cool wind, and a third figure is there |
| 57–77 | `1` ×21 | the confrontation: Gamble, alive in a borrowed body, and Stephanie's confession |
| 78 | `2` | **"Let's give it another try."** → good ending |

## The last choice: both branches are "good"

The final menu is

```
1: "I think this is where we go our separate ways, Stephanie. Goodbye."
2: "Let's give it another try."
```

and **both** print the same closing card, *"You've managed to reach one of the
two good endings in the game."* — checked by re-running the route with the last
command flipped to `1`, which gives the walking-out epilogue instead of the
staying-with-her one. So "the two good endings" *are* these two options; the
committed route takes `2` because that is what the author's transcript takes.

## No end-game action anywhere

`SCR_DUMP_TASKS`'s action histogram for the whole file is

```
 17 ACT type=0   (move object)
 98 ACT type=1   (move player/NPC)
223 ACT type=3   (change variable)
 71 ACT type=5   (execute task)
```

— no `ACT type=4` (score) and **no `ACT type=6` (EndGame)**. Both good endings,
and the death ending (`<END GAME>`, *"All about me is the endless darkness of
death. I have failed. I am undone."*), are plain text: the game prints its
closing card and leaves the player standing at a prompt. That is why the
golden's last lines are the harness's appended `quit` and its `[Y/N]`
confirmation rather than a `*** You have won ***` banner.

## The crash it flushed out (fixed)

The `kill seamus` beat aborted the interpreter:

```
Task: running task 314 forwards, depth 2
Task: task 314 running 4 actions
Task: moving object 91 to hidden
Task: moving object -1 to hidden
Assertion failed: (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count)),
  function gs_object_make_hidden, file scgamest.cpp, line 568.
```

`kill *seamus*` (task 310) redirects to task 313 `[* after kill someone]`,
which redirects to task 314 `[? the return]`, whose second action is

```
ACT type=0 v1=2 v2=0 v3=0
```

`Var1 = 2` is **"the referenced object"**, but task 314 is only ever reached by
redirection — no `%object%` was ever matched — so `var_get_ref_object()` hands
back `-1` and SCARE walked straight into the range assertion in
`gs_object_make_hidden()`.

`task_move_object()` now ignores negative object indexes, exactly as
`evt_move_object()` already did ("Ignore negative values of object"). This is
the same family as the known unset-combo rule — an ADRIFT selector the author
left blank (or a reference that was never bound) is a silent no-op in the
Runner's `Select Case`, never a fatal. Guarding at the top of
`task_move_object()` rather than in one branch covers every destination
(hidden / room / roomgroup / held / worn / NPC's room), all of which assert the
same way. The whole v4 suite was re-run afterwards: **160/160 PASS**, no golden
moved.

## `br>` in the Walkthrough menu screen is the author's typo

Choosing `4` (Walkthrough) from the title menu prints a stray `br>` on its own
line between the two redraws of the menu. That is faithful. The task's text is

```
… (although be aware that this is a very long section of text).<br><wait><cls>
<br>×9<info1>br><br>×6<info1>
```

`<info1>` is one of the game's own ALRs, expanding to the six-line title menu;
the author typed `<info1>br>` where he meant `<info1><br>`, so the `br>` is
printed as literal text. (The screen is also where the game advertises its two
built-in hint commands, `complete` and `reveal` — `reveal` dumps the same
start-to-finish transcript that the doc file carries.)

## Notes for re-running

* **`SCR_SKIP_WAITKEY=1` is required.** The game is wall-to-wall
  `<wait>`/`<waitkey>` cut-scenes; without the skip every pause eats a command
  and the route desyncs within a dozen turns.
* The doc file's transcript **prints its ending twice** — the last block is
  duplicated verbatim — so the extracted command list is truncated to the first
  78 `>` lines. Extraction:

  ```python
  d = open('downloaded/Mortality_walkthrough.txt','rb').read().decode('cp1252').replace('\r','')
  cmds = [l[1:].strip() for l in d.split('\n') if l.startswith('>')][:78]
  ```

* The file is **CP1252**, not UTF-8 (`…` = 0x85). It is only read here, never
  piped into the interpreter, so this costs nothing — but `grep` on the
  resulting transcript treats it as binary and silently reports no matches.
  Check for parser errors with Python, not `grep`.
