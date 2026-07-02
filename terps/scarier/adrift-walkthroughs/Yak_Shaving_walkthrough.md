# Yak Shaving for Kicks and Giggles! (V1) — walkthrough

- **Engine:** ADRIFT 4 (native). J. J. Guest, The Odd Competition 2008. Shave a
  yak, knit a sock, open a jar of pickled eggs for the Dada Lama.
- **Result:** **WIN** — ends with `Congratulations! You have completed the Odd
  Competition mini version…`. Solution: `harness/yak_shaving_solution.txt`;
  golden `yak_shaving_solution.expected.txt`.
- **Walkthrough source:** *Key & Compass*. **Use the "Version 1 (ADRIFT comp
  entry)" section only** — V2 is an Inform 7 / Z-machine port with a different
  map (razor location, monk/sofa puzzle, etc.). Commands taken from V1 match the
  `.taf` 1:1.

## Derivation

The V1 command list wins straight through — **no key-wait pauses**, no blank
lines needed. `(using the chopsticks…)` / `(using the hairdryer)` parentheticals
are not commands — plain `knit sock` and `melt ice` auto-use the held tool.

Route: from Shangri-La → Mountaintop, `clap` to bury the acolyte and uncover the
**chopsticks**, take the **sock** and **peg** from the "flags" → `wear peg` to
brave the shed's stench → get the **razor** at the Glacier, `rub ice` → shave the
yak, `knit sock` (chopsticks as needles) → open the Sanctum seat for the
**hairdryer** → `melt ice` at the Glacier to reach the Ice Cave → `give socks to
yeti` (he leaves), `give eggs to yeti` (he opens the jar) → return to the Sanctum
and `give eggs to lama` to win.
