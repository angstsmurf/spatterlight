# Gateway: Guardian Child — walkthrough

- **Author:** Michael R. Grice ("Kinvadren").
- **Engine:** ADRIFT 4 (Battle System present — used by the jail Rapist — plus a
  visible RPG layer: LEVEL / EXP / NEXT-LEVEL, day/night, moods).
- **Result:** **WIN, full 30/30 — deterministic.** This is an **unfinished beta**
  (the END room states it outright), but it ships a real, scored, winnable
  *temple zone* with a deliberate victory.
- Solution file: `harness/gateway_solution.txt` (starts with the name answer
  `Hero` and gender answer `male` — this game uses **both** ADRIFT start-up
  prompts, which SCARE now honours).

The game's own win screen confirms the maximum:

> *CONGRADULATIONS!!! … SCORES: 10 = one purple flower / 20 = 2 out of 3 /
> **30 = All the purple flowers were found** … Gateway: Guardian Child doesn't
> go beyond the Fayn Temple of The Stars.*

## Scope (why 30 is the maximum)

A full structural dump (96 tasks) shows the **only** `ChangeScore` actions are
the three +10 flower pickups (tasks 59, 75, 89); there is no other score source.
The two `EndGame` actions are the win (`END GAME`, task 0, room 26) and one
death (`#killed`, task 86 — opening the wrong jail cell). The west forest /
Fayn / blacksmith quest the priest sends you on **is not implemented** — the END
room is a stub: *"Type End Game if you wish to quit now and win! … Note: This
game is no where near completion. Only the temple zone is playable."*

## The win (full 30/30)

```
Hero            <- name prompt
male            <- gender prompt
walk through door
east            <- timed "guards capture you" cut-scene fires here; you wake
east               seated in Bantrek's office, this stands you up into Bright Hall
south
down
north
west            <- back in the Temple Courtyard
south
pick flower     <- FLOWER 1 (purple), +10  [Garden]
north
east
north
east
get flower      <- FLOWER 2 (purple), +10  [Prayer Room]
west
north
down
get key         <- Kadfast hands you the gold jail key  [Guard Room]
south
east
get flower      <- FLOWER 3 (purple), +10  [East Jail Cell — a Rapist attacks]
west            <- flee before he can do real harm
north
up              <- the guards confiscate the key here (eats this turn)
up              <- now you actually climb the stairs
south
south
west
west
west            <- THE END room
end game        <- WIN, score 30/30
```

### Phase by phase

1. **The Gateway & the forced intro.** You start in a void facing the Guardian
   Child's image and a door. `walk through door` drops you in the **Temple
   Courtyard**; after one turn a **timed** cut-scene runs — guards charge, the
   priest **Bantrek** has you brought up to his office, seats you, and tells you
   to seek the Forest of Visions and deliver a note to the Fayn blacksmith (both
   unimplemented). The first command after arriving is always consumed by this
   event; the next `east` stands you out of the chair into **Bright Hall**.
2. **Back to the courtyard.** From Bright Hall: `south, down, north, west` →
   Temple Courtyard (the temple hub: N stable, E entry hall, S garden, W gate).
3. **Flower 1 — Garden** (south). `pick flower` takes the purple flower (+10);
   the Temple Gardener ignores you. `north` back.
4. **Flower 2 — Prayer Room.** `east` (Entry Hall) → `north` (Corridore) →
   `east` (Prayer Room). `get flower` (+10). `west` back.
5. **Flower 3 — the jail** (the only puzzle). `north` (North End of the
   Corridor) → `down` (**Guard Room**, where the soldier **Kadfast** is playing
   cards). `get key` and she hands you the gold key that opens the cells.
   `south` into the **Dark Hall** (three cells). `east` uses the key to open the
   **East Jail Cell** — which holds both the third flower **and a Rapist**.
   `get flower` (+10, score now 30), then immediately `west` to flee.
   - **Do NOT open the West cell** — that path runs the `#killed` death ending.
   - The torch in the guard room is a red herring: it isn't needed in the cell,
     and (like the key) the guards confiscate it the moment you try to leave.
6. **Escape & win.** `north` into the Guard Room; the guards take the key back
   (this consumes one turn, hence the **double `up`**). Then `up, south, south,
   west` returns you to the Courtyard; `west, west` reaches **Outside the Temple
   Gate** then **THE END**; `end game` triggers the victory with 30/30.

## Honest notes

- **30/30 is the true maximum** — confirmed both by the data (three +10 actions,
  nothing else) and by the game's own end screen. Faithful to the data and the
  original ADRIFT Runner; not a SCARE limitation.
- **Two start-up prompts.** Gateway asks for *both* a player **name** and a
  **gender** at boot. SCARE now honours both (commits `8d9c9426` name,
  `2e74a6e6` gender); without those fixes the game would stall at the prompts.
  The gender is cosmetic here (no reachable gender-gated task), but the prompt
  must still be answered.
- **Combat is real but optional to engage.** The jail Rapist uses the Battle
  System; you only need to grab the flower and run, so no `SC_ASSUME_COMBAT` is
  required and no fight must be won.
- The "guards confiscate what you carry out of the basement" mechanic costs one
  extra turn on the way out — the extra `up` in the solution accounts for it.
- Deterministic seed-1234 harness; the solution reproduces 30/30 +
  "CONGRADULATIONS" identically every run (verified 3×).
