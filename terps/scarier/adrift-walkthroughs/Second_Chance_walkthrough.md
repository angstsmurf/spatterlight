# Second Chance — walkthrough

- **Engine:** ADRIFT 4 (written with "ADRIFT V.4 R.45"), David Whyld, started
  February 2004 / finished March 2005.
- **Result:** **WIN — the good ending, reached in 49 commands.**
- Solution file: `harness/second_chance_solution.txt` (49 commands + `quit`).
- Harness row: `second_chance_solution.txt|second chance.taf|congratulating me on a job well done.|SCR_SKIP_WAITKEY=1`
- Source document: `downloaded/SecondChance_walkthrough.pdf`.

## Derivation

This one is a gift. The shipped PDF is not a prose walkthrough but a **full
session log** — every player command appears on its own `>`-prefixed line, in
order, interleaved with the game's output. So the command list falls straight
out of the document: take every `>` line, strip the marker, done.

It then replays **VERBATIM** — 49 commands, not one repair, straight to the
good ending. That is unusual enough in this corpus to be worth stating plainly:
no synonym substitutions, no inserted `z`, no reordering.

The single thing that has to be known is a harness detail, not a game one: the
title sequence embeds **two `<waitkey>` pauses** before the first prompt. Under
the headless driver those swallow the first two commands and the whole run
desyncs by two, so the row carries `SCR_SKIP_WAITKEY=1`.

## Why the marker is a sentence and not a score

The game keeps **no score at all** — `score` answers "No one's keeping score."
Its several endings are not ranked by points but by whether the three vignettes
were played well:

| Vignette | What "well" means |
| --- | --- |
| **Dolores** and the thugs | call the police (`push button` on the mobile), don't intervene physically |
| **Jenny** | talk to her (`3` then `2`); never take the "Ask her about sex" branch |
| **Doug** | talk him down — three separate `talk to doug` turns |
| **Antonia** | actually search her room (`x desktop`, `x icons`, `click picture`, `click diary`, `click internet explorer`, `make bed`, `x posters`, `get calendar`) |

The payoff is entirely in the closing scene, where each person you saved turns
up on the far side of the road and Mr ER applauds:

> …out of the corner of my eye I catch a glimpse of the unmistakeble Mr ER
> himself standing in the crowd. He gives me a wide-as-his-face smile and claps
> his hands together as if **congratulating me on a job well done**.

So the harness win marker is the last line of that scene. (Markers are matched
with `grep -F` against the wrapped transcript, so it has to be a fragment that
survives on one output line — hence the short tail rather than the whole
sentence.)

## Structure

Street corner → the incident over the road (Dolores, the mobile, `push button`)
→ upstairs (`open door`, `u`) → the menu conversation with Jenny (`3 2 1 3 1`,
`shake hand`) → downstairs and the run of `1`s through the argument → `yes`,
`wait` → east/west/up/north to Doug → three `talk to doug` → the computer
(`turn on computer`, password `ainotna` — "Antonia" backwards) → Antonia's room
→ `out`, `s` into the closing scene.

## Status

Wired and blessed; `golden = harness/second_chance_solution.expected.txt`
(683 lines). PASS.
