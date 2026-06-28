# Shadow of the Past — walkthrough

- **Game:** `Shadow_Of_The_Past.taf` — *Shadow of the Past* (ADRIFT 4.0; the 272 KB
  file is mostly embedded graphics — the game itself is tiny: 8 rooms).
- **Result:** **WON, max reachable 90/100, deterministic.**
- **Solution:** `harness/shadow_of_the_past_solution.txt`.

A ten-year-old paperboy is knocked off his bike and wakes as a grown man on the
floor of an ancient cave — the statue of a long-dead hero who, the journal
reveals, is a past life of the player. The quest: free the trapped souls by
destroying the crystal that imprisons them, then leave.

## Structure (from a `SC_DUMP_TASKS`/exit/NPC dump)

- **8 rooms**, **24 objects**, **1 NPC** (the Beast), **0 events**, **15 tasks**.
- The map is two clusters joined only at the **Ruined Statue** hub:
  - *East side:* Ruined Statue ⇄ **Ancient Gallery** ⇄ **Passage** (→ outside / win).
  - *West side:* Ruined Statue ⇄ **Cage** ⇄ **Humming Chamber** ⇄ **Blocked
    Passage** ⇄ **The Ledge**.
- **Scoring (ChangeScore, type-4) — 11 tasks, summing to 100:**

  | pts | task | where |
  |----:|------|-------|
  | +5  | climb the ruined statue | Ruined Statue |
  | +10 | listen to the cries | Ancient Gallery |
  | +15 | remove the portraits from the wall | Ancient Gallery |
  | +5  | pull the lever down (raises the sunken cage) | Cage |
  | +5  | get the gold crown | Cage |
  | +5  | examine the good book | Humming Chamber |
  | +10 | hang the rope on the hooks | Blocked Passage |
  | +10 | get the tuning fork | The Ledge |
  | +15 | touch the tuning fork to the crystal | Ancient Gallery |
  | +10 | go NE out of the Passage (**the win**, EndGame type-6 var1=0) | Passage |
  | +10 | "beast killed" — **orphaned, unreachable (see below)** | — |

- **Two hazards to avoid:**
  - **`touch crystal`** with a bare hand is an EndGame **death** (and −10). The
    portraits' plaque and the cries both warn you off it — you destroy the
    crystal with the *tuning fork*, not your hand.
  - The **Beast** in the cage (see below).

## The 90-point route (the solution file)

Directions follow the game's own "you can move…" text (the compass labels are
rotated relative to the raw exit table, a known ADRIFT quirk):

```
(blank — clear the intro)
climb statue            +5
get down
n                       → Ancient Gallery
listen                  +10
remove portraits        +15
s                       → Ruined Statue
w                       → Cage
pull lever              +5   (raises the cage; Beast still dormant)
open door               (the cage door — needed before the crown is reachable)
w                       → Humming Chamber
examine good book       +5
nw                      → Blocked Passage
hang rope on hooks      +10  (unlocks the Down exit to the Ledge)
d                       → The Ledge
get tuning fork         +10
u                       → Blocked Passage
se                      → Humming Chamber
e                       → Cage
get crown               +5   (the Beast wakes and attacks — flee, don't fight)
e                       → Ruined Statue
n                       → Ancient Gallery   (Beast can't follow off its hub)
touch tuning fork to crystal   +15  (shatters the crystal, frees the souls)
ne                      → Passage
ne                      WIN  +10  ("…reborn and fate allowed you to finish…")
```

Final score **90/100**. Verified deterministic (seed 1234): identical win,
no death, on three consecutive runs.

### Key ordering facts

- **The gold crown is in the Cage, not on the statue.** The "crown" on the
  statue's brow (with the yellow gem that reflects your adult face) is scenery;
  the scoring crown sits inside a sunken cage that the **lever** raises, behind a
  cage **door** you must `open` first.
- **`touch fork to crystal` requires the portraits already removed** (task-state
  restriction), so do the Gallery work before the fork.
- **`hang rope on hooks` unlocks the Ledge** (the Blocked Passage's Down exit is
  gated on that task), where the tuning fork waits.
- **Pulling the lever and opening the cage door do NOT wake the Beast** — only
  `get crown` does. So raise/open the cage on the way *in*, do the whole west
  loop, and grab the crown on the way *out* as the very last west-side act, so
  the Beast is loose for the fewest turns.

## The Beast — and why max is 90, not 100

Grabbing the crown wakes a Beast (Battle System: Stamina/Strength/Defence/
Accuracy/Agility all 35). The player's stats are wide random ranges
(Strength 0–50, Defence 0–49, Stamina 50), so the fight is a pure RNG gamble:
the Beast can do up to 35 a hit and a couple of unlucky rolls kill you outright.
**Do not fight it** — the route grabs the crown and immediately leaves the Cage
(`e`, `n`) for the Ancient Gallery; the Beast guards the Cage/hub and cannot
follow into the Gallery, and under the fixed seed its couple of parting swings
both roll zero damage, so the banked solution always survives. (In live,
unseeded play the crown's +5 is a genuine coin-flip; the rest of the 85 is safe.)

The 15th task, **"beast killed" (+10)**, is **orphaned and unreachable**: it has
no command you can type (a no-rooms internal task), the Beast's **KilledTask is
"No Task" (0)** so killing it runs nothing, no task carries a type-5 exec to fire
it, and the game has **no events**. The author built the reward but never wired
it to the Beast — so it is dead in the original ADRIFT Runner too (a faithful
authoring omission, not a SCARE limitation). Hence the honest maximum is
**90/100**.
