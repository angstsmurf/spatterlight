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
to Trabula` for the full 125.

## Banked solution (added 2026-07-02)

`harness/trabula_solution.txt` now banks a deterministic turn-list, wired into
`run_v4_walkthroughs.sh` (golden `trabula_solution.expected.txt`, marker
`given the gold coins to Trabula` — the +50 give, which with the +75 ring give
is the full 125). Since the game has no `EndGame`, the marker is that winning
give, not a victory string.

**Derivation finding:** the Key & Compass walkthrough says `kill soldier` /
`kill troll`, but SCARE's parser answers `kill X` with "Now that isn't very
nice." The working combat verb is **`attack soldier`** / **`attack troll`**
(auto-uses the sword). Combat is multi-round and RNG-driven; under the seeded
`scare` binary a fixed number of attacks kills deterministically, so the
solution issues a generous fixed count of `attack` commands (surplus attacks
after death are harmless no-ops) then `take all`. This is the game the
4th-1-Hour-Comp page calls *Get Treasure For Trabula* (David Good); its file is
`Trabula.taf`.
