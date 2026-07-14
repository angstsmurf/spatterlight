# Main Course — walkthrough

- **Author:** quantumsheep (2008).
- **Engine:** ADRIFT 4 (Battle System present — the SoMorph kills the cat and the
  pilot with it; each dies to **one landed hit**, though the RNG makes the first
  swing at each miss under the current seed stream, so combat works as authored
  and no combat-assist is needed).
- **Result:** **WIN, deterministic. 0/0 (no score — the game has no `ChangeScore`
  actions; the single ending is the victory).** Win marker:
  *"Congratulations! You're on your way home with just a little indigestion!"*
- Solution file: `harness/maincourse_solution.txt` (begins with two blank lines —
  the intro has two "press any key" prompts).

## Premise

You are a **SoMorph** — a purple, tentacled, shape-shifting alien — woken with a
hangover in the cargo hold of a freighter bound for Earth. A SoMorph *"can
appear to others in the form of its most recent prey,"* and its mouth *"can morph
to eat any creature."* You want to eat the cat (Mr. Jones) and the pilot (Alan
Davies), take human form, command the ship's computer **FRANK**, and go home.

## Map (5 rooms)

```
            Command Deck (FRANK computer, pilot Frank, catnip)
                 |
   Cryo Stasis --+-- Corridor Alpha --+-- Bathroom (toilet + big red button)
   (Cryo Tube,   |   (hub)            
    the human)   |
              Cargo Bay (start)
```
From Cargo Bay go **north** to the Corridor; from the hub: **north** = Command
Deck, **west** = Cryo Stasis Room, **east** = Bathroom (door starts closed),
**south** = Cargo Bay.

## Walkthrough

```
<blank>                 <- "press any key to start"
<blank>                 <- "press any key to continue"
north                   <- Corridor Alpha (the cat is here but exits west)
look                    <- the cat re-enters and takes a swipe at you
attack cat              <- Mr. Jones dodges and slips out
look                    <- the cat re-enters again
attack cat              <- this swipe kills Mr. Jones
eat cat                 <- "SoMorph eat cat"; this drops some cat fur here
take cat fur
open door               <- the bathroom door (east) starts closed
east                    <- Bathroom
close door              <- privacy: the toilet won't be used with the door open
open loo                <- the toilet starts closed
use toilet              <- digests the cat (needed before the fur can be worn)
wear cat fur            <- disguise: now you look like the cat, not a scary alien
push button             <- "Premature Ejection during Hyperspace!" opens the Cryo
                           Tube and wakes the frozen human, Alan Davies
open door
west                    <- Corridor
west                    <- Cryo Stasis Room (the woken human is here)
attack human            <- the disguise lets you land the blow; he flees east
look
look                    <- he runs screaming back into the dead-end Cryo room
attack human            <- the second blow kills Alan Davies
eat human               <- "SoMorph prepare for main course" — now you ARE human
remove cat fur          <- take the cat disguise off so FRANK sees a human
east                    <- Corridor
north                   <- Command Deck
main course             <- change course for home = WIN
```

Run with `sh harness/play.sh "<…>/Main Course.taf" harness/maincourse_solution.txt`.

## Why each step is needed (structural dump)

The win is task 8 (`* course *`, in the Command Deck), whose only restriction is
**task 0 (`eat human`) complete**; its action is the type-6 EndGame victory.
Working backwards:

- **`eat human` (task 0)** needs the *dead human* object present. The human
  starts as a frozen body in the Cryo Tube; the **only** thing that frees it is
  the bathroom's **big red button** (task 10), and that button does nothing until
  **`use toilet` (task 2)** has been done. `use toilet` in turn requires the
  **toilet open** *and the bathroom door closed* (its two object-state
  restrictions) plus the cat already eaten.
- Pushing the button wakes the human as a **live, fleeing NPC**. He screams and
  runs every turn, so you can't normally land a blow — unless you are **wearing
  the cat fur** ("…hoping to fool the human into not fighting"), the disguise that
  makes you appear as his pet cat. The fur is produced by **eating the cat**
  (task 1's hidden action drops it) and can only be **worn after `use toilet`**
  (task 6's restriction). With the disguise on, a landed swipe kills Alan Davies
  (under the current seed the second swing is the one that lands — he dodges the
  first and flees into the dead-end Cryo room, returning two turns later); his
  death (task 7) reveals the dead-human object, which you then eat.
- Eating the human makes the SoMorph **appear human** — but only once the cat
  disguise is off, so **`remove cat fur`** before talking to FRANK, who otherwise
  refuses: *"I am only programmed to accept requests from humans. Disgusting
  purple aliens with tentacles cannot change the course of the ship."*

## Notes

- **No score:** a full task dump (11 tasks) shows zero `ChangeScore` actions, so
  the game is 0/0; the sole ending is this victory (there are no lose/death
  endings either). Documented like the other 0/0-win games in this corpus.
- The catnip in the Command Deck and the wandering "Cat sheepishly enters/exits"
  lines are flavour/decoy — neither is needed for the win.
- Combat is faithful (deterministic under the harness seed): both the cat and the
  disguised-approach human die to a single *landed* hit; the route just has to
  ride out one dodge each.
- **Re-derived 2026-07-14** for the scarier engine: the banked solution had lost
  its two leading waitkey blanks (the intro eats the first two input lines), and
  the NPCs-before-events tick-order fix shifted the wandering cat's schedule and
  the combat RNG stream, so the old "one swipe each" route desynced (`attack cat`
  fired with the cat absent — an unresolved-NPC attack falls through the grammar
  to "I don't understand", which does not advance the turn). The rewired route
  above is a `run_v4_walkthroughs.sh` regression row with a blessed golden.
