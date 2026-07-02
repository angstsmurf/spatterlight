# Pieces of eden — walkthrough

- **Engine:** ADRIFT 4 (native). Nicodemus, Comp With No Name 2008. Part one of
  an assassin story — pay your tab, disguise as a cop, prove loyalty to "the
  shadows".
- **Result:** **WIN** — ends with `END OF PART ONE...`. Solution:
  `harness/pieces_of_eden_solution.txt`; golden
  `pieces_of_eden_solution.expected.txt`.
- **Walkthrough source:** *Key & Compass* (native-ADRIFT solve, matches 1:1).

## Derivation (the SPACE / `1.)` token game — §6 caveat)

This is the archetypal case the TODO flags: the Welbourn page uses `SPACE` and
`1.)` pseudo-commands and assumes interactive key-waits, so the mechanical
extraction is useless. Rebuilt by hand:

- **`SPACE` → a blank line** (a page-advance keypress). The game is
  keypress-heavy: it opens with an intro that needs **six** key-waits
  (the two `SPACE SPACE SPACE` groups) *before* the first real command prompt in
  the **Coffee shop**, which is the start room.
- **The leading `n` in the archive is a map-arrow artifact, not a command** —
  there is no `n` to type; the intro already places you in the Coffee shop.
- **`look at number`, not `x number`** (the archive says so explicitly).
- **`1.)` is a literal command** — type one-period-right-bracket to pick the
  conversation option; `104` is the passphrase answer.
- After `w` into the Alley there is **one** more key-wait (`> SPACE`); after `n`
  back to the Front of shop there are **two** (`> SPACE SPACE`).

Exact keypress budget baked into the solution: `6` blanks (intro) → `take
wallet` / `pay Steve` / `w` → `1` blank → alley actions (`look at number`,
`take uniform`, `wear uniform`) → `n` → `2` blanks → `1.)` → `104`.

(The intro narration mentions being "about to be arrested" — that is flavour, not
the lose ending. Going east, not paying Steve, dawdling, or picking `1a.)`/`2.)`
are the actual arrests.)
