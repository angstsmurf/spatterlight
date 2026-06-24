# Trabula — walkthrough

- **Engine:** ADRIFT 4. A small bridge-and-huts fantasy errand: deliver treasure
  to the wizard **Trabula**.
- **Result:** **No ending (no win/lose state). Max score 125/125.** A score-only
  game in the style of *The Town of Azra* — you can reach the maximum but the
  game never declares victory.

## Structural verdict

Only **4 tasks**, 16 rooms, 0 events, 3 NPCs (Trabula, a Troll, a Soldier). The
two `ChangeScore` actions are the deliveries to Trabula (in *Trabula's*, room
14): **`give gold to Trabula` +50** and **`give ring to Trabula` +75** (= the
125 maximum). The other two tasks are `open steel [chest]` (room 15) and an
internal `&&foo`. **There are no `EndGame` actions at all**, so there is no
victory or death ending — banking both gifts simply leaves you at 125/125 with
the game still running.

## Route (outline)

The gold and ring are locked in the **wooden/steel chests**; the keys are held
by the **Troll** (Under the Bridge, room 11 — a rusty key + a bar) and the
**Soldier** (Middle Bridge, room 1 — a silver key + a rapier). You start armed
with a sword. Recover the keys, open the chests for the gold and the ring, carry
them to **Trabula's** hut (room 14), and `give gold to Trabula` then `give ring
to Trabula` for the full 125. (No deterministic turn-list is banked here — with
no ending to verify against, the structural maximum is the meaningful result.)
