# Cruel and Hilarious Punishment — walkthrough

- **Engine:** ADRIFT **3.9** (native). Mel S, ADRIFT 4th 1-Hour Comp 2004.
- **Result:** **WIN** — the boss offers you a street-prankster job; you refuse
  with a speech (`…You've destroyed our reality, and replaced it with your
  own…`). Solution: `harness/cruel_solution.txt`; golden
  `cruel_solution.expected.txt`; marker `destroyed our reality`.
- **Walkthrough source:** *Key & Compass* (native-ADRIFT).

## Derivation

Runs straight through from the mechanical extraction — no pause surgery. The
archive's `(with sponge in inventory)` / `(with shard of glass in inventory)`
parentheticals are just preconditions, not commands: `take slime` and `cut cord`
auto-use the held sponge/shard. Route: gather sponge + jacket + glass, `take
slime` (with the sponge), `put slime in coffee`, then `cut cord` (with the glass
shard) to trigger the ending. Exercises the ADRIFT **3.9** code path. The win
marker drops the apostrophe from "You've" (`destroyed our reality`) so it stays
a plain string in the runner's MAP.
