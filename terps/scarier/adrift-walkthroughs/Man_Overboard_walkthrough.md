# Man Overboard!!! — walkthrough

- **Engine:** ADRIFT 4 (native). TonyB, Writing Challenges Comp 2006. A very
  silly ship game — half the items are useless — escape HMS Challenged.
- **Result:** **WIN** — the rubber-boat ending; final line
  `Maybe it wasn't all a waste of time...` (the game then exits). Solution:
  `harness/man_overboard_solution.txt`; golden
  `man_overboard_solution.expected.txt`.
- **Walkthrough source:** *Key & Compass* (native-ADRIFT solve, matches 1:1).

## Derivation

The mechanical extraction runs almost straight through. Notes:

- The archive offers **two win options** (rubber boat vs. can of Red Bull). We
  use the **boat**: `inflate boat` (the game auto-uses the foot pump — no
  `with pump` needed), then `w` off the plank wins. The solution therefore
  **ends at that winning `w`**; the archive's trailing `drink red bull` line
  (the alternative ending) is dropped.
- **`(using the …)` parentheticals are not commands here** — the game
  auto-selects the held tool. `open drawers` works once you hold the silver key
  (the note-flagged `unlock drawers with key` is a game bug); `open pantry`
  works once you hold the crowbar. Plain verbs suffice.
- The winning move fires a chain of ending pages, then the game **exits on the
  last key-wait** — so no blank-line padding is needed; the transcript ends
  naturally and identically whether or not trailing input follows.
- `open it` (pronoun) resolves fine for the cupboard and wardrobe.

Route: kit up in the Captain's quarters → grab items round the ship (chocolate,
map auto-taken in the kitchen, silver key in the cargo hold) → `open drawers`
for the boat → collect the crowbar from the crow's-nest toolbox → `open pantry`
for the foot pump → to the plank → `inflate boat`, `w`.
