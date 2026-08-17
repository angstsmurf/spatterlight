# Pestilence — walkthrough (**WIN, 100/100**)

- **Author:** Richard Otter, version 1.00, 2006. A plague-quarantined city; you
  are locked up and have to get out alive.
- **Engine:** **ADRIFT 4.00** (`xxd -p -l 12 games/pestilence.taf | cut -c17-22`
  → `93453e`).
- **Result:** **WON** with full marks — "You have managed to survive the
  Pestilence and you are alive! ... You managed to score 100 out of the maximum
  100. / The End". Wired as
  `pestilence_solution.txt|pestilence.taf|You managed to score 100 out of the maximum 100.`,
  no env.
- **Source:** the author's own `Walkthru`, bundled with the game. Copy kept as
  `downloaded/Pestilence_walkthrough.txt`. Followed **verbatim**, all 85
  commands, no corrections needed.

## The one edit: the title menu

The game opens on a four-item menu:

```
1. Play the game
2. Read the introduction
3. Useful information
4. Credits
```

so `pestilence_solution.txt` starts with a bare `1` that the author's file does
not have. Everything after that line is his walkthrough unchanged. No
`SCR_SKIP_WAITKEY` is needed — the menu is a normal prompt, not a keypress page.

## The shape of it

```
exam rubbish / get brick / throw knife at guard / get key / unlock door
north / close door / north / put brick on desk / north / get key / north
down / south                                     down to the assistant
exam someone / talk to assistant
ask assistant about cure / ask assistant about artemis
talk to assistant / talk to assistant            his chain of replies
                                                 -- the two brief-me items
get jar / get paper / read paper                 the glass jar and the note
get card / read card / kick wall                 the victim's card
get book / read book                             the counter's book
unlock panel / press button / pull lever         the machinery
get all from shelf / mix artemis                 the artemisia ingredients
give artemis to assistant / talk to assistant
mix cure / drink artemis / talk to assistant     immunised, cure handed over
drop jar / unlock window / south                 out through the window
```

The brick on the desk and the thrown knife are the only two aggressive acts; the
rest is fetch, read and ask. The repeated `talk to assistant` lines are
load-bearing — each one advances his script a step, they are not padding.

Scoring is all-or-nothing in practice: the walkthrough's route collects every
point, so the marker greps the full-score line rather than the ending, which
means a scoring regression cannot slip through with the same closing prose.
