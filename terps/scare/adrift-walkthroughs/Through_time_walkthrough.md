# Through time — walkthrough

- **Engine:** ADRIFT 4. A time-travel story: 1954, Texas panhandle. You are Joe
  Gatling, a failed farmer who nightly dreams of being abducted by aliens and
  becoming a superhero.
- **Result:** **UNFINISHED DEMO.** Only the opening **farm** section is
  implemented. The author walled off the rest of the map with an explicit
  in-game message, so there is **no reachable win, no reachable score** — not by
  hostile design but because the adventure was never completed.

## The wall (the game tells you itself)

The whole playable game is the farmhouse and its porch. The moment you try to
step off the porch into the yard, the game stops you with the author's note:

> **You can't leave the porch....**
> **This is as far as this adventure will take you at this point.**
> **Take care ;-)**

Every direction off the porch (north/south/east/down) is blocked the same way.
That porch is the edge of the implemented world.

## Reachable walkthrough (the entire playable game)

Banked, deterministic, in `harness/through_time_solution.txt`. (Blank lines are
deliberate — they answer the game's "Press any key." pauses.) The farm is just a
short tour; none of it is required, there is nothing to "solve", and it all dead-ends
at the porch.

```
            <- (intro "Press any key.")
take magazines
read magazines      (a box of matches + the TV remote fall out)
take control
turn on tv
            <- (TV news flash "Press any key.")
south               (-> the kitchen)
open cupboard
take can            (drag the stinky garbage can out of the cupboard)
look in cupboard    (reveals a sixpack of beer)
take pizza
north               (-> the living room)
open bedroom door
west                (-> the bedroom; your wife is here)
            <- (bedroom "Press any key." x2)
out                 (-> the porch)
down                (THE WALL: "...as far as this adventure will take you")
```

- **The TV is the plot hook.** Turn it on and a news flash reports a UFO heading
  for *Duff's Waterhole* — i.e. your own farm. "Duff's Waterhole? Why... that's
  here!!!" This is the set-up for the alien abduction that the demo never
  delivers.
- **The bedroom is a scripted cul-de-sac.** You step in, see your chocolate-eating,
  nagging wife, mutter an excuse and are immediately bounced back to the living
  room — you cannot stay.
- The kitchen items (garbage can, sixpack, pizza slice, the matches from the
  magazines, the kerosene lamp) lead nowhere in the demo; they are inventory for
  the unimplemented later acts.

`score` reads **0 out of a maximum of 0** the whole way through.

## Structural verdict — what was never wired up

A `SC_DUMP_TASKS` dump shows **164 tasks**, but only ~30 of them belong to the
reachable farm. The rest implement a complete-on-paper but **disconnected**
time-travel adventure that the porch wall seals off:

- **An alien spaceship** — airlock, north–south corridor, reception (a clerk
  hands you a magnetic key card), an elevator (swipe card to reach the 3rd
  floor), a council chamber (passphrase *"through adversity to the stars"*), an
  elevator lab and a **laboratory / time machine** with an "analyzer" instrument
  for scanning artifacts.
- **Ancient Rome** — Caesar's Villa, the Venus Victrix Temple, Pompey's Theater,
  and the **Curia Pompeii** (the Senate house of Caesar's assassination), plus a
  *pugio* (Roman dagger) and a silver Roman coin to analyze.
- **The Battle of Tours, 732 AD** — the game even embeds the Wikipedia link
  *en.wikipedia.org/wiki/Battle_of_Tours* as a task. Charles Martel's camp
  (east/west halves), four Muslim camps, the northern/southern battlefield and
  woods, an enemy tent (cut the wall with a scimitar), and weapons to analyze
  (scimitar, battleaxe).

**Endings (all of them sit in this unreachable content):**

| EndGame `var1` | meaning | count | where |
|---|---|---|---|
| 0 | **win** | **0** | — none exists anywhere |
| 1 | lose | 1 | *talk to the guards* at the Battle of Tours |
| 2 | death | 1 | *"Busted bladder"* (the beer/pizza → toilet timer) |
| 3 | stop | 8 | walking off the edges of the Rome / Tours maps |

There are **no `ChangeScore` (type-4) actions reachable**, and **no `var1=0`
victory ending exists at all**. In the engine's terms the game therefore cannot
be won or scored — it is an **unfinished demo** whose only honest "ending" is the
author's own porch sign-off, *"This is as far as this adventure will take you at
this point. Take care ;-)"*.
