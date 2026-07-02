# We are coming to get you! — walkthrough

- **Engine:** ADRIFT 4 (native). Richard Otter (as rotter), ADRIFT 2nd 3-Hour
  Comp 2004 (3rd place). You are a germ; infect all five body parts and leave.
- **Result:** **WIN** — you get sneezed out: `…you are out and flying through
  the air. You land on the face of another flesh-sack!` Solution:
  `harness/togetyou_solution.txt`; golden `togetyou_solution.expected.txt`.
  (Game file `togetyou.taf`.)
- **Walkthrough source:** *Key & Compass* (native-ADRIFT).

## Derivation

Movement is by **body-part name** (`stomach`, `ear`, `nose`…), not compass, and
infecting uses the special verb `infect <part>`. The vessels can't be infected
until acid is spat in the nose (eat acid in the stomach, carry it up, spit it,
then `infect vessels` — the 5th infection).

**Key finding:** the archive's final move `hit adenoids` is wrong for this
engine — SCARE maps `hit`→`punch`, which the game rejects ("Who do you think
you are, Mike Tyson?"). The winning verb maps to **`smash`**: `smash adenoids`
(or `kick`/`tickle adenoids`) after all five infections sneezes you out to win.
The solution substitutes `smash adenoids` for the archive's `hit adenoids`.
