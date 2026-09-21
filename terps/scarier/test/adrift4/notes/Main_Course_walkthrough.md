# Main Course — walkthrough

- **Author:** quantumsheep (2008).
- **Engine:** ADRIFT 4 (Battle System present — the SoMorph kills the cat and the
  pilot with it; each dies to a landed hit, so combat works as authored and no
  combat-assist is needed).
- **Result:** **WIN, deterministic under the seed. 0/0 (no score — the game has no
  `ChangeScore` actions; the single ending is the victory).** Win marker:
  *"Congratulations! You're on your way home with just a little indigestion!"*
  Wins in the real Runner too: `runner_transcripts/maincourse.txt` (run400x, seed
  17) is identical to the golden on all 28 turns.
- Solution file: `goldens/maincourse_solution.txt` (begins with two blank lines —
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
north                   <- Corridor Alpha
north                   <- Command Deck
take catnip
south
west                    <- Cryo Stasis Room
drop catnip             <- a blur of fur rushes out of the flap on the cryo tube,
                           eats the catnip and runs into the corridor
east                    <- Corridor Alpha; the cat is here and attacks you (misses)
attack cat              <- "hit Jones, but it doesn't seem to do any damage"
attack cat              <- the second swipe kills Mr. Jones
eat cat                 <- drops some cat fur here
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
west                    <- Corridor Alpha; the human is here, then "runs screaming
                           to the east" (into the bathroom)
look                    <- empty corridor
look                    <- "Human runs screaming from the east." — back in reach
attack human            <- the cat disguise lets the blow land
attack human            <- the second blow kills Alan Davies
eat human               <- "SoMorph prepare for main course" — now you ARE human
remove cat fur          <- take the cat disguise off so FRANK sees a human
north                   <- Command Deck
main course             <- change course for home = WIN
```

Run with `sh harness/play.sh "<…>/Main Course.taf" goldens/maincourse_solution.txt`
(`SCR_RNG=xoshiro SCR_SEED=17`, as the regression row sets them).

## Why each step is needed (structural dump)

The win is task 8 (`* course *`, in the Command Deck), whose only restriction is
**task 0 (`eat human`) complete**; its action is the type-6 EndGame victory.
Working backwards:

- **The cat only exists after the catnip.** Mr. Jones (NPC 0) starts hidden
  (room -1). Task 3 (`drop * catnip *` in the Cryo Stasis Room — the catnip is on
  the Command Deck) moves him to Corridor Alpha and stops his walk (the walk's
  StoppingTask is that task), so he waits there to be killed. Nothing else
  produces him: a route that just walks about the corridor never meets a cat,
  and `attack cat` is "I don't understand what you mean!" (an unresolved NPC
  falls through the grammar and does not advance the turn). This is why the
  previous route (`north`, `look`, `attack cat`, …) was not a win.
- **`eat human` (task 0)** needs the *dead human* object present. The human
  starts as a frozen body in the Cryo Tube; the **only** thing that frees it is
  the bathroom's **big red button** (task 10), and that button does nothing until
  **`use toilet` (task 2)** has been done. `use toilet` in turn requires the
  **toilet open** *and the bathroom door closed* (its two object-state
  restrictions) plus the cat already eaten (**`eat cat`**, task 1, which needs the
  dead cat present and drops the cat fur).
- Pushing the button wakes the human as a **live, fleeing NPC** (walk 1 starts on
  the tube opening). He screams and paces Corridor Alpha ↔ Bathroom, so you can't
  normally land a blow — unless you are **wearing the cat fur** ("…hoping to fool
  the human into not fighting back"). The fur can only be **worn after
  `use toilet`** (task 6's restriction). With the disguise on, a landed swipe
  hits him and a second kills Alan Davies; his death (task 7) reveals the
  dead-human object, which you then eat.
- Eating the human makes the SoMorph **appear human** — but only once the cat
  disguise is off, so **`remove cat fur`** before talking to FRANK, who otherwise
  refuses: *"I am only programmed to accept requests from humans. Disgusting
  purple aliens with tentacles cannot change the course of the ship."*

## Notes

- **No score:** a full task dump (11 tasks) shows zero `ChangeScore` actions, so
  the game is 0/0; the sole ending is this victory (there are no lose/death
  endings either). Documented like the other 0/0-win games in this corpus.
- **Combat is seed-dependent.** Under `SCR_RNG=xoshiro SCR_SEED=17` (the Runner's
  own draws, matched by run400x at `VBRNG_SEED=17`) the first swipe at the cat
  misses and the second kills it; the human takes two landed hits, and his pacing
  through the corridor decides how many turns to wait (`look` ×2). A different
  seed reshuffles both — re-derive rather than assume. The old LCG-seed route
  differed for exactly this reason.
- `eat human` opens with the Runner's "SoMorph can't see the dead human." — the
  task hides the body first and therest()'s seen-but-absent clause then speaks
  (Scarier's `eat *` row → `lib_cmd_verb_absent_400`, added 2026-09-21 after this
  route's Runner drive found it).
- **Re-derived 2026-07-14** (NPCs-before-events tick order) and **again
  2026-09-21**, rerouted through the catnip so the cat is caught before it can
  wander off; the earlier route waited for the wandering cat to re-enter the corridor,
  which the real Runner (run400, 2026-08-24) did not reproduce, so it was not a win there. The route is a `run_v4_walkthroughs.sh` regression row with a
  win-marker and a blessed golden, and the Runner transcript twin agrees.
