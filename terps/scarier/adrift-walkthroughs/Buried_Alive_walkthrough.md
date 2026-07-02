# Buried Alive — walkthrough

- **Engine:** ADRIFT 4 (native). David Whyld, ADRIFT 2nd 3-Hour Comp 2004 (1st
  place). Your wife buried you alive over £12.50; claw out and get even.
- **Result:** **WIN** — ends `Well done. You got to the end of this short but
  extremely silly game.` Solution: `harness/buried_alive_solution.txt`; golden
  `buried_alive_solution.expected.txt`.
- **Walkthrough source:** *Key & Compass* (native-ADRIFT, matches 1:1).

## Derivation

The mechanical extraction desyncs because a **`[more]` pause in the intro** eats
the first `dig`, leaving only two — not enough to escape the grave, so every
subsequent move fails ("run out of exits"). Fix: **one blank line at the very
top** to clear that pause. After that the route runs straight through.

Death traps dodged by the route (per the archive): don't open the coffin
without carrying the head; you get only one safe bedroom visit (the head freaks
out your wife). Note `get tv` works where `take tv` doesn't. Route: `dig`×3 out
→ kitchen for the head from the fridge → give head to Bob in the bathroom →
cellar, feed the ghoul (open coffin) for the knife → fetch the TV from the
lounge → `give tv to Bob`, `w`, `w` to win.
