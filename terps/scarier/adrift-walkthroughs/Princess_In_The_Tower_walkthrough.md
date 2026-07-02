# The Princess In The Tower — walkthrough

- **Engine:** ADRIFT 4 (native). David Whyld, 1st 1-Hour Comp 2002. Fetch three
  proofs of love — an orc's head, a goblin's rib, and a golf-ball-sized diamond.
- **Result:** **WIN** — ends with `It seems you've won.` Solution:
  `harness/princess_in_the_tower_solution.txt`; golden
  `princess_in_the_tower_solution.expected.txt`.
- **Walkthrough source:** *Key & Compass* (native-ADRIFT solve, matches 1:1).

## Derivation

The mechanical extraction wins straight through — **no key-wait pauses**, so no
blank-line surgery was needed. Retained a few "expected-failure" flavour
commands from the archive (the early `in` the princess refuses, `climb tree` /
`climb ladder` refusals); they produce refusals and don't derail the run.

**Death traps avoided** (per the archive's Endings list, both dodged by the
route): never `kill Grimbel` (throw the dagger instead), and never `climb rope`
a *second* time outside the far tower (cut it after the first "SOD OFF").

Route: from Outside the tower go to Yoshi's tent for the **dagger** → cave,
`throw dagger at Grimbel` for the **rib**, retrieve dagger → oasis, `shake tree`
to stun the alligators and grab the orc's **head** (never throw the dagger at the
oasis — it sinks) → far tower, `cut rope` for a rope → chasm, `tie rope to tree`
+ `climb rope` for a **flower** → clearing, `give flower to Lorel` for the
**diamond** → back to the tower, `in` to win.
