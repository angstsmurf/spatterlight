# Death's Door — walkthrough

- **Game:** `deaths.taf` — *Death's Door* (the intro banner reads "deaths door";
  ADRIFT **3.90**, 25 KB, author unknown). Uses the ADRIFT **Battle System**.
- **Result:** **WON, full 100/100, deterministic.**
- **Solution:** `harness/deaths_solution.txt` (first two lines are the start-up
  prompt answers: name `Hero`, gender `male`).

You "have been called to free the house of death." Armed with a short sword,
you break into a three-storey building, fight your way up through its apartments
collecting four coloured keys, and finally destroy the **Dark force** in the
attic. The intro's advice — *"upgrade your weapon and armor in order to
survive"* — is real: combat is the ADRIFT Battle System and one mid-game enemy
(**Rooth**) will kill an unarmoured player on sight.

## Structure (from a `SC_DUMP_TASKS`/exit/NPC dump)

- **19 rooms**, **62 tasks**, **15 NPCs**, **1 event**.
- **18 rooms** form the building; you start outside on the **Driveway** (room 1).
- **Scoring (ChangeScore, type-4) — 9 tasks summing to 100:**

  | pts | task            | where                  |
  |----:|-----------------|------------------------|
  | +5  | open mail box   | Driveway               |
  | +10 | unlock door (silver-key) | Third floor → Red kitchen |
  | +10 | unlock door (gold-key)   | Second floor → Lit dining room |
  | +10 | unlock door (red-key)    | Third floor → Dark closet |
  | +10 | unlock door (crystal-key) | Second floor → Living room |
  | +15 | kill debbie     | Hall1 west             |
  | +15 | kill beth       | Guest room             |
  | +15 | kill ross       | Master bedroom         |
  | +10 | kill *force*    | Attic (this is the type-6 **EndGame win**) |

  All nine are reachable, so the honest maximum is the full **100/100**.

## The key/door chain

The four coloured keys gate the four locked apartment doors, and each key is
locked behind the previous door — one long dependency chain bootstrapped from
the mailbox:

```
mail box ─▶ silver-key
silver-key unlocks Red kitchen   ─▶ kill Jim   ─▶ gold-key
gold-key   unlocks Lit dining    ─▶ kill Ireen ─▶ red-key
red-key    unlocks Dark closet   ─▶ kill Rooth ─▶ crystal-key
crystal-key unlocks Living room  ─▶ Debbie / Beth / Ross cluster (+45)
```

The attic (and thus the win) is gated separately: the **Third floor → Attic**
stair only opens once you type **`kill witch`** (task 23, a no-restriction
scripted task that runs anywhere on the Third floor — the witch herself never
has to be present). The final **`kill force`** in the Attic is the EndGame win.

## Combat notes (important)

- The "kill *name*" tasks are scripted (no restrictions) — typing the command in
  the right room kills the NPC and runs its drop/score actions. **But you are
  still in a Battle-System fight**, so the enemy gets attack turns too.
- Most enemies are harmless to you (their `strength − defence ≤ 0`, so SCARE
  prints *"…but it doesn't seem to do any damage"* — the five Driveway monsters
  Ghost/Wolfe/Cat/Vampire/Witch and the apartment NPCs Jim/Ireen do nothing).
- **Rooth is lethal.** With only the short sword he one-shots you the moment you
  step into the Dark closet. The fix is the intro's advice: buy armour first.
  The shop is on the **Third floor** (`buy scale male`, then `buy plate male`).
  Money comes from the kills (Jim/Ireen/Rooth each pay 500) and from **selling
  the keys after you've used them** (`sell silver-key` pays 500). Wearing
  **plate male** raises your defence enough that Rooth — and the Attic's Dark
  force — do no damage. (Determinism note: the Battle System is RNG-driven; the
  harness seed makes the outcome reproducible, but the armour is what makes the
  win robust, not luck.)
- **Joshua** (the helpful NPC from the Short hall) starts following you once you
  reach the upper apartments; he is harmless flavour.

## Walkthrough (full command list — 100/100)

Start outside on the Driveway.

```
open mail box            # +5, blows up the box, leaves the silver-key
take silver-key
west                     # → Front door
press pound              # intercom keypad: the code is # 5 7 2 4
press 5
press 7
press 2
press 4                  # front door unlocks with a click
south                    # → Lobey
up                       # → Second floor
up                       # → Third floor (the shop / hub)
unlock door              # silver-key → opens Red kitchen, +10
east                     # → Red kitchen
kill jim                 # Jim dies, drops gold-key (+ knife & shield), +500 gp
take gold-key
west                     # → Third floor
sell silver-key          # +500 gp (already used it)
buy scale male           # armour
buy plate male           # better armour — now you survive Rooth
down                     # → Second floor
unlock door              # gold-key → opens Lit dining room, +10
west                     # → Lit dining room
kill ireen               # drops red-key, +500 gp
take red-key
east                     # → Second floor
up                       # → Third floor
unlock door              # red-key → opens Dark closet, +10
west                     # → Dark closet
kill rooth               # (plate male keeps you alive) drops crystal-key
take crystal-key
east                     # → Third floor
down                     # → Second floor
unlock door              # crystal-key → opens Living room, +10
east                     # → Living room
north                    # → Corner of hall
east                     # → Hall1 west
kill debbie              # +15, opens the east door
east                     # → Hall1 east
north                    # → Guest room
kill beth                # +15, opens the south door
south                    # → Hall1 east
south                    # → Master bedroom
kill ross                # +15
north                    # → Hall1 east
west                     # → Hall1 west
west                     # → Corner of hall
south                    # → Living room
west                     # → Second floor
up                       # → Third floor
kill witch               # opens the stairs up to the Attic
up                       # → Attic
kill force               # +10 — the Dark force crumbles to dust = WIN
```

Win text: *"as the dark force crumbles into dust you are swept out of the house
and placed back on the driveway … you head off knowing you did a good job."*
Final score **100/100**.

## Notes on the design / unreachable points

- There are **no orphaned scoring points** — all 100 are on the route above.
- The shop carries far more gear than you need (band, helmet, shield, ruby
  armor, a 1000-gp `laser` weapon) and the special-attack verbs each weapon
  unlocks (`cross cut`, `fire bird`, `iron smash`, `criss cross slash`, …). None
  of it scores; `plate male` is sufficient to survive every required fight, so
  the route buys only that. A player who over-buys still wins — the economy is
  deliberately generous (selling all four used keys pays 500/1000/2000/4000).
- The Driveway monsters and the Third-floor `kill ghost/wolfe/cat/vampire`
  commands are flavour: only `kill witch` (which opens the Attic) is required,
  and none of them score.
- Faithful to the ADRIFT 3.90 Runner: standard exit/key/Battle-System data, no
  SCARE divergence. Verified 3× identical (100/100 + win marker).
