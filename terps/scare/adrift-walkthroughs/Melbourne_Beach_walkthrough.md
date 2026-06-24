# Melbourne Beach — walkthrough

- **Author:** David D. Good (`duoing@bigfoot.com`), *Melbourne Beach* v1.0f,
  3 Feb 2001, compiled with ADRIFT 3.90.12. No published walkthrough on Key &
  Compass, IF Archive, or CASA.
- **Engine:** ADRIFT 3.90 (no Battle System — this is a domestic slice-of-life
  game, so no combat-assist is involved).
- **Result:** **WIN, max reachable 38/41 — deterministic.** The win is rinsing
  the sand off your feet at the beach shower after a morning of helping out
  around your friends' house.
- Solution file: `harness/melbourne_beach_solution.txt`. No start-up prompts.
- **The three unreachable points (41 − 38) are all faithful game-data limits —
  see the closing note.**

The win screen:

> You wash the sand off your feet.  There, isn't that better?
> **Congratulations!**
> You successfully completed the original game Melbourne Beach…

## Story / setup

You are an overnight guest at David and Judy's beach house in Melbourne Beach,
Florida. You wake up in the guest room; the game is a gentle "do helpful
morning things, then drive to the beach" sandbox. Points come from getting
dressed, returning Judy's oiled trumpet and her music folder, reading, doing a
load of laundry, a cup of coffee, a game of chess with David, and the beach
trip itself.

## Map (the parts that matter)

```
GUEST ROOM --w--> Outside Bedrooms --w--> JUDY'S BEDROOM (trumpet/case)
   Outside Bedrooms --s--> Outside Den --s--> ENTRANCE HALL
   Outside Den --e--> DEN (books/computer)   --w--> Living Room (chess)
   Entrance Hall --sw--> KITCHEN --s--> EATING AREA (purse drops here)
   Kitchen --e--> LAUNDRY --e--> GARAGE (red car)
   GARAGE --in--> Car  --(drive)-->  beach parking lot
   Parking lot --e--> Path to beach --n--> Volleyball court
                                     --e--> Crossover --e--> BEACH (sandy feet)
                                     --se--> BEACH SHOWER (the win)
```

NPCs **Judy** and **David** wander the house on walks; their routes consume the
shared RNG stream, so the exact turn either one is in a given room shifts with
the command list. The solution handles this by **repeating** the give/play
command until the NPC is present (see the notes) — robust under determinism.

## Key puzzle mechanics (reverse-engineered)

- **Oiling the trumpet drops the car keys.** The `oil trumpet` task, besides
  scoring +5, silently moves the hidden **leather purse** (containing the car
  keys) into the **Eating area**. That is the only way the keys ever appear.
- **"Music" = the folder.** Judy wants her trumpet *and* her **music folder**
  (in-game hint confirms this). The folder is in the car; fetch it, then give
  Judy first the trumpet (+5), then the folder (+10). `give music to judy`
  requires `give trumpet to judy` to be done first.
- **Dryer auto-correct.** Typing anything with "drier" is normalised to
  "dryer"; `turn on dryer` is the +1 laundry task (see the closing note).
- **The computer is a death trap** (`touch computer` drops you "inside the
  computer", and a wrong move there ends the game) — never needed; avoid it.
- **The beach shower needs sandy feet** (set by walking *east onto the Beach*).
  **Volleyball is a skill counter:** each `play volleyball` improves you one
  notch ("lousy → bad → amateur → …"); the **7th** play scores +2.

## Walkthrough (38/41, then win)

The first line of the solution is **blank** on purpose — the game opens with a
"Press any key to get up out of bed" prompt that swallows one line of input.

```
(blank line — clears the "press any key" prompt)

# Guest room: get dressed (+1 jeans, +1 shirt) and pet the cat (+1)
open dresser            (it has drawers; this opens it)
get jeans
wear jeans
open closet
get shirt
wear shirt
pet kitty

# Judy's bedroom: oil the trumpet (+5).  This also drops the leather
# purse (car keys) into the Eating area.
w
w
open case
get trumpet
get bottle
open bottle
oil trumpet

# Eating area: collect the car keys
e
s
s
sw
s
open purse
get keys

# Garage / car: unlock the car (+1), drive nothing yet — just grab the folder
n
e
e
unlock car
open car
in
get folder
out

# Judy's bedroom: give her the trumpet (+5) then the music folder (+10).
# Judy wanders, so repeat each give until she is present (harmless misses).
w
w
ne
n
n
w
give trumpet to judy   x20   (scores +5 on whichever turn Judy is here)
give music to judy     x20   (scores +10; needs give-trumpet done first)

# Den: read the computer books (+2).  Do NOT touch the computer.
e
s
e
read book

# Laundry: wash → dry → fold the basket of clothes (+5)
w
s
sw
e
get dirty clothes
open washer
put dirty clothes in washer
wash clothes
get wet clothes
open dryer
put wet clothes in dryer
dry clothes
get dry clothes
fold clothes

# Kitchen: one cup of coffee (+1)
w
get pot
pour coffee
drink coffee

# Living room: a game of chess with David (+1).  David wanders; repeat.
ne
nw
play chess   x3

# Garage / car: open the garage door and drive to the beach (+1)
se
sw
e
e
in
get opener
open garage door
drive car

# Beach: volleyball (+2), get sandy, then the winning shower (+2)
out
e
n
play volleyball   x8   (the 7th play scores +2)
s
e
e                 (step onto the Beach — feet become sandy)
w
w
se
use shower        (+2, and "Congratulations!" — the win)
```

Total **38/41**, ending on the win.

## The three unreachable points (41 − 38) — all faithful game data

The stored maximum is 41, the sum of all seventeen positive ChangeScore tasks.
Three of those points cannot be banked in any faithful interpretation (verified
against the `.taf` task/restriction/action tables, which SCARE evaluates
identically to the original 3.90 Runner):

1. **Turn-on-dryer (+1) vs. fold clothes (+5) are mutually exclusive.**
   `wash clothes` is **non-repeatable**, so the single load produces exactly one
   batch of wet clothes. `turn on dryer` (+1) consumes that batch (it hides the
   wet clothes and merely recycles the *dirty* ones back into the dryer), and so
   does drying-then-folding (+5). You can take one or the other, never both. The
   walkthrough takes **fold (+5)**, forgoing the +1 — a net gain of 4.

2. **Coffee #2 — the red cup (+1) — is a logical contradiction.** The red coffee
   cup starts hidden and is revealed by exactly one action in the game: `give
   coffee to the Captain` (the NPC inside the computer), whose restriction
   requires **`drink oil` to be DONE**. But the red-cup drink that scores this
   +1 requires **`drink oil` to be NOT done**. Once the cup exists (oil drunk),
   the scoring task can never fire. Unreachable.

3. **Coffee #3 — the cured red cup (+1) — is gated behind a −1.** This drink
   *does* require `drink oil` to be done, but `drink oil` itself is a **−1**
   ChangeScore (it is the poison half of a poison/cure gag). +1 − 1 = **net 0**,
   so it can never raise the score above what you get by leaving the oil alone.

Net effect: the highest reachable score is **38**, whether or not you bother
with the (dangerous, computer-bound) oil/Captain subplot. The yellow-cup coffee
(+1) is the only coffee point that is a real, penalty-free gain, and the
walkthrough takes it.

## Determinism / footguns

- Built and run with the deterministic headless SCARE (`harness/build.sh`,
  seed 1234). The full solution reproduces **38/41 + "Congratulations!"**
  identically across runs.
- The opening **"press any key"** prompt eats the first input line — the
  solution begins with a blank line to absorb it.
- **Judy and David wander** and their walks consume RNG, so their position on a
  given turn depends on the whole prior command list. The solution meets each by
  **repeating** the relevant command (`give … to judy`, `play chess`) rather
  than timing a single turn; the misses are harmless and the determinism makes
  the scoring turn fixed.
- Navigate the beach by the game's own "you can move…" text — the compass
  labels there are rotated relative to the internal exit table.
