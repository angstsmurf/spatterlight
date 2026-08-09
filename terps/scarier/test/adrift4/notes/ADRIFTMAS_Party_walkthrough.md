# ADRIFTMAS Party — walkthrough

- **Engine:** ADRIFT 4 (`ADRIFTMAS_Party.taf`). A Christmas in-joke set at
  Campbell Wild's house: you are Santa, you fall off the roof, and the
  twenty-six party guests are all named after ADRIFT forum regulars. 78 tasks,
  27 rooms.
- **Result:** ★ **WON, 100/100** — every point the game can award.
- **Solution:** `goldens/ADRIFTMAS_Party_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `"Merry ADRIFTMAS TO ALL!  And to all a good night!"`.
- **Provenance:** no author walkthrough exists — plover.net lists the game
  without one. The route was derived from `SCR_DUMP_TASKS`.
- **Content warning:** the game opens with its own one — foul language, adult
  situations, tasteless humour. The route stays clear of the crude tasks, all
  of which score *negative* anyway.

## Route

```
look / kiss mystery / make snowball / make snowman / n / pee in snow / open door / n
look under table / chew gum / n / w / s / open closet door / s / get broom
sweep floor / put gum on broom / n / n / open cabinet / get nutmeg / eat nutmeg
n / ask samantha about shower / n / nw / sw / n / take key / w
put nutmeg in absinthe / ask mileout about pool / e / s / se / give money to dave
nw / sw / unlock right drawer with key / open right drawer / get springs
ne / ne / se / s / open wardrobe / get suitcase / unlock suitcase with key
open suitcase / put springs in suitcase / close suitcase / n / nw / sw / s / s / s
drop suitcase / jump on suitcase / get presents / score / go down chimney
```

Three `<waitkey>` pauses are represented as blank lines in the solution file —
one before the first prompt, one after `look`, one after `eat nutmeg`. Do not
reflow them.

## Scoring

Sixteen tasks award points and they total exactly 100; this route fires all
sixteen. Four more tasks award *negative* points and none is on the route.

| Task | Command | Points |
|---|---|---|
| 13 | `make snowball` | +5 |
| 14 | `make snowman` | +5 |
| 17 | `pee in snow` | +10 |
| 20 | `kiss mystery` | +5 |
| 23 | `look under table` | +5 |
| 24 | `chew gum` | +5 |
| 27 | `sweep floor` | +5 |
| 25 | `put gum on broom` | +10 |
| 22 | `eat nutmeg` | +5 |
| 33 | `ask samantha about shower` | +10 |
| 26 | `take key` | +5 |
| 35 | `put nutmeg in absinthe` | +5 |
| 36 | `ask mileout about pool` | +5 |
| 37 | `give money to dave` | +5 |
| 41 | `jump on suitcase` | +5 |
| 49 | `go down chimney` — executes TASK 50 `[credits]` → EndGame(win) | +10 |

The negative ones are 34 (−5), 45 (−25), 46 (−10) and 47 (−10), all for
groping an NPC. TASK 47 is `[grope/grab/fondle/fuck/suck][%character%]`, so it
is easy to trip over by accident once the parser fix below is in.

The `score` on the second-to-last line reads **90**: the chimney supplies the
last 10 and ends the game in the same turn, so 100 is never printed.

## Notes

- **The first command is a throwaway.** You start on The Roof, and TASK 12
  (`cmd=[*]`, room 25, non-repeatable) claims whatever you type first, slides
  you off the roof and drops you on the Front Steps. The route spends a `look`
  on it.
- **The puzzle chain is one long thread:** the chewed gum is under the small
  table in the Foyer → chew it → the broom is in the Kitchen Hall Closet →
  sweep the floor → stick the gum on the broom → that sticky stick is what
  lifts the shiny key off the top of the ADRIFTmas tree in the Living Room.
  TASK 26 wants the broom *held* and task 25 *done*, and it consumes the broom.
- **One key, two locks.** The shiny key unlocks the right drawer of the dining
  room sideboard (springs) *and* the suitcase in the Second Master Closet
  wardrobe. TASK 41 wants the springs **inside** the suitcase and the suitcase
  **closed** — hence unlock / open / put / close, then carry it out and drop it
  on the Front Steps before jumping.
- **`ask samantha about shower` teleports you** to the Master Bath Closet (the
  scene it prints is the reason). The n / nw / sw walk afterwards is the way
  back to the Hall.
- **Leave the cue ball on the billiards table.** TASK 35 restricts on
  `type=0 v2=5` — the cue ball must be *on top of* the table — and TASK 36
  wants task 35 done first, so drugging MileOut's absinthe has to precede the
  game of pool that yields the $10.
- **Phrasings that do not work:** `look under small table` and `x table` are
  both refused (plain `look under table` is the one that matches); `knock on
  door` gets "You're SANTA!  You don't need to knock!"; the way in is
  `open door` then `n`.

## Engine fix this game forced

`kiss mystery` originally fell through to the library's `lib_cmd_kiss_npc`
("I'm not sure he would appreciate that!") and the 5 points were unreachable.
TASK 20's command is `[kiss {the} %character%]`, and in `scparser.cpp` a
`%character%` or `%object%` that is the **last element inside a `[...]` or
`{...}` group** could never match: `uip_parse_list()` appends a `NODE_EOS` only
at the top level, so such a node has no right sibling, the temporary remainder
list `uip_match_remainder()` builds is empty, and `uip_match_list()` fails
empty lists by design. Every candidate was therefore rejected.

Ground-truthed against the real Runner first — `run400.exe` under Wine answers
`kiss mystery` on the Front Steps with "You lay a Happy Holidays kiss on
Mystery.  Turns out that Mystery didn't appreciate it much and belts you right
in the kisser." Fix: `uip_match_remainder()` returns TRUE when
`node->right_sibling` is NULL, the remainder being vacuously satisfied. The top
level is unaffected (its EOS sibling still enforces end-of-string after the
group). Two very common idioms were dead before this — `[kiss {the}
%character%]` and `[smack/hit/punch/kick]{the}[%character%]`.

Corpus fallout was one row: in `Ticket to No Where`, TASK 405
`[say][hello][to]{the}[%character%]` now fires on `say hello to john tailer`
instead of falling through to the game's default response. Re-blessed. See
`RUNNER_TESTS_TODO.md` §4.
