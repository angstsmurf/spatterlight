# The Thorn — Eric Mayer (Summer MiniComp 2003) — ADRIFT 4

**Status: ★ DONE — reaches an ending, wired into `run_v4_walkthroughs.sh` (PASS).**

* Game: `Adrift games/Minicomp/Thorn by Eric Mayer/Thorn.taf` (symlinked as
  `games/Thorn.taf`)
* Solution: `harness/thorn_solution.txt` · golden: `thorn_solution.expected.txt`
* Win marker: `You have chosen to look upon your own mortality.`

This was one of the three "port-based" games in `TODO_plover_walkthroughs.md` §5:
the Key & Compass page solves **Eric Mayer's Inform/Z-code port**, not the ADRIFT
original, so the command list had to be re-derived here.

## The shortcut: the game ships its own walkthrough

`Thorn.taf` has a `walkthru` task (TASK 43). Typing `walkthru` at any prompt
prints the author's own solution — that is the backbone of
`thorn_solution.txt`. Worth trying on any unknown ADRIFT game before doing
real work (cf. *Ambassador to Dupal*).

## Where the Inform walkthrough is wrong

* **Getting Wilkens out of the study.** K&C says `z` until he leaves. In ADRIFT
  he never leaves on a timer: you must ask him about *both* the thorn and the
  abbey/priory, and then `drink scotch` (or `ask wilkens about scotch`). Only
  then does he go to fetch the Lagavulin, freeing the desk (→ map → exit east).
* **The bottle/`cut bud` ending does not exist.** K&C lists three endings
  (`south`, `x bud`, `take bottle` + `cut bud`). The ADRIFT original has **no
  bottle object at all** (the object table is window/things/chair/desk/map/van/
  vicarage/church/walk/book/gravestones/abbey/rubble/weeds/fence/snow/fireplace/
  scotch/thorn/monuments/sticks/brush/fens/windows/wall). There are **two**
  endings, both at The Ruined Wall at Dawn: `x thorn` and `s`. The solution
  banks `x thorn`, the one with the narrative payoff (you see the bud = your own
  death). `s` — "You have chosen not to look upon your own mortality." — is the
  other, and works identically.
* **`x brush` reveals the sticks**; there is no separate `take sticks` step in
  the author's own list (though `get sticks` works and is what the solution uses).

## Waitkey pauses (the real work — §6 step 3)

The game leans hard on `<waitkey>` pauses, and the harness maps one line of the
solution file to one keypress. Three separate pause runs, each needing blank
lines in the solution:

| after | pauses | what happens |
|---|---|---|
| `x thorn` + 3× `z` | **1** | `[Press Key to Follow the Vicar]` — the vicar teleports you to the vicarage |
| `ask vicar about thorn` | **5** | the dream/Worgren cutscene, ending `[Press Key to Wake]` → *Inside the Vicarage at Dawn* |
| `w` (to the dawn wall) | **2** | the arrival cutscene at *The Ruined Wall at Dawn* |

The 3× `z` before the first pause is a real event: `EVENT 0 !vicar_appear`
starts on task 16 (`x thorn`) and fires 3 turns later. Without the waits,
`make fire` is issued in the wrong room and the run desyncs.

**Counting trap.** A blank line is ambiguous — it satisfies a waitkey if one is
pending, otherwise it is an *empty command* ("I don't understand what you
mean!"). Over-supplying blanks therefore looks like a desync, and *under*-
supplying silently feeds the harness's own trailing `quit`/`y` into the waitkey
(so the transcript ends with no quit prompt). The reliable way to count a pause
run is to append N distinct probe tokens and see how many reach the parser:

```sh
# 8 probes after the last real command; each "I don't understand" = one that
# got through, so (8 - hits) is the number of pending waitkeys.
printf 'probe1\nprobe2\n...\n' >> script.txt
sh play.sh ../games/Thorn.taf script.txt | grep -c "I don't understand"
```

`SCR_SKIP_WAITKEY=1` makes the pauses transparent (one solution line = one
command) and is useful while deriving — but the committed golden is produced
*without* it, because the rest of the v4 corpus uses the blank-line convention
and enabling it globally would invalidate every existing golden.

## Route

Study: ask about thorn + abbey → `drink scotch` → Wilkens leaves → `open desk`,
`take map` → east. Then east, north, east, up (church tower) → `take book`,
`read book` → down, w, n, w (abbey) → `x brush`, `get sticks` → w (Ruined Wall)
→ `x thorn` → 3× `z` → [key] → vicarage → `make fire` → ask the vicar about the
abbey and the thorn → dream (5 keys) → dawn → `w` (2 keys) → `x thorn`.
