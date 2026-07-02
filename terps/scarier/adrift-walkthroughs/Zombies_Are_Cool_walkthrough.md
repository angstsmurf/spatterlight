# Zombies Are Cool… (ZAC) — walkthrough

- **Engine:** ADRIFT **3** (native). Mel S, ADRIFT 2nd 3-Hour Comp 2004. Full
  title: *Zombies Are Cool, But Not So Cool When They're Eating Your Head.*
- **Result:** **WIN (which is also your death)** — ends `Later that day, you and
  Stu were eaten by zombies.` / `The End`. Solution:
  `harness/zombies_solution.txt`; golden `zombies_solution.expected.txt`.
  (Game file `ZAC.taf`.)
- **Walkthrough source:** *Key & Compass* (native-ADRIFT).

## Derivation

Runs straight through from the mechanical extraction — no pause surgery needed.
Exercises the ADRIFT **3** code path. Death traps (per the archive): a bare
`kill zombies` / `cut zombies` is instant death — you must specify the weapon
(`kill zombies with chainsaw`, `cut boards with chainsaw`). The winning
`blow whistle` in the pet shop ends the game with the intended win-is-death gag,
so the marker is the ending line `you and Stu were eaten by zombies` rather than
a triumphant string.
