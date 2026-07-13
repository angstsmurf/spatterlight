# Goldilocks is a FOX! — J. J. Guest (2002) — ADRIFT 4

**Status: ★ WON 100/100 MAX — wired into `run_v4_walkthroughs.sh` (PASS).**

* Game: `Adrift games/goldilocks.taf` (symlinked as `games/goldilocks.taf`)
* Solution: `harness/goldilocks_solution.txt` (208 commands, no waitkeys)
* Golden: `goldilocks_solution.expected.txt`
* Win marker: `Three Bears are no more`

Second of the "port-based" games from `TODO_plover_walkthroughs.md` §5 — the
K&C page solves ralphmerridew's **Inform 6 port**. Unlike *Renegade Brainwave*
(where the Inform release is an expansion), this port is a faithful 1:1
conversion: **same 100-point maximum, same 29 scoring items, same puzzle
chain**. Only the *phrasing* differs, so K&C's "Speedy Runthrough" was a good
skeleton — 180 extracted commands, of which the vast majority needed no change.

## Verb/noun divergences (all of them)

Every failure was a wording problem, not a puzzle problem:

| K&C (Inform) | ADRIFT original | note |
|---|---|---|
| `give turnip` | `give turnip to cow` | ADRIFT needs the indirect object |
| `take pot` | `x pots` | *examining* the pots reveals the trowel + gloves; `take pot` is refused ("I can't see that a plant pot would be of much use to me") |
| `open packet` → `plant bean` | `open packet` → **`take bean`** → `plant bean` | no implicit take |
| `u` (beanstalk) | **`climb beanstalk`** | `u` is not an exit |
| `pour hot in gigantic` ×2 | `pour porridge in large bowl into gigantic bowl` (**once**) | one pour empties *both* the hot and the cold bowl; **requires the spoon** ("I don't have anything to mix them with!") |
| `eat tiny porridge` | `eat porridge in tiny bowl` | the game prints the required format itself |
| `take pork` / `take carton` | `take chops` / `take box` | objects are "packet of frozen pork chops" and "box of porridge oats" |
| `cook pork` | `cook chops` | |
| `drop rug` (to cover) | **`cover trapdoor`** | plain `drop rug` in the hall scores nothing |
| `enter tiny bed` | **`lie on tiny bed`** | |
| `pry machine with poker` | `open machine with poker` | |
| `put sticks in toaster` | `put dynamite in toaster` | |
| `push channel` / `push buy` | **`press`** channel / buy | `push` is a different verb here; you must also **`take remote`** and **`plug in tv`** first (K&C: "You don't need to take remote to use it" — you do) |
| `ask wolf about turbine` *in the hall* | same command, **in the meadow** | see below |
| `x skeleton` | same | it *auto-takes* the poker, so no `take poker` step |

**The wolf/turbine trap.** `ask wolf about turbine` exists twice. In the hall it
is a flavour-only task; the scoring one (TASK 132) is `where=room 20` — the
meadow. Feed him the cooked chops in the hall, walk to the meadow (he follows),
and ask him *there*. Asking in the hall and then walking over gets you a wolf
standing in the meadow doing nothing forever. (`wolf, blow turbine` also fires
it, but prints a stray "I don't understand what you want to do with The Big Bad
Wolf." first, so the solution uses the clean phrasing.)

## Things worth knowing

* **Carrying capacity is real** and silently blocks puzzles: `take cue ball` →
  "The cue ball is too heavy for me to carry at the moment.", `take gigantic
  bowl` → "My hands are full at the moment." The solution drops the note,
  bottle, packet and trowel on arrival in the kitchen, and the blanket / bowls /
  spoon / dumbells / poker / paint / oil can as soon as they are spent.
* **Don't drop the rug in the hall until the very end.** Taking it is what
  reveals the trapdoor, but dropping it back in the hall *covers* the trapdoor —
  and the trapdoor is the only route to the cellar, which you need many times.
  The solution stashes it in the sitting room and fetches it at the end.
* **There is a clock** (the bears come home at noon), so the run is deliberately
  lean — `x`/`look` probes were stripped from the final script. The banked
  208-command route finishes comfortably.
* `x pockets` is *not* needed to reveal the cue ball; `take blanket` (which
  exposes the snooker table) is enough.

## The 100 points

Turnip, feed cow, sell cow, water bean, fix antenna, mix porridge, quick meal,
hot breakfast, brass key, defrost pork chops, open door (cheese wedge), feed the
wolf, start the turbine, move the wardrobe, open the crate, cut the cable, drink
the beer, buy the toaster, prepare the bomb, BOOM (switch A), paint the ball,
meet the frog, create the coach (pumpkin), create the horse (mouse), change of
wardrobe (dress), complete change of wardrobe (underwear), move the pot, start
the pot (`alakazam`), cover the trapdoor, and 4 for `lie on tiny bed`.
