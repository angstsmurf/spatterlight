# Snakes and Ladders — walkthrough

- **Engine:** ADRIFT 4 (native). Ken Franklin, ADRIFT 2nd 3-Hour Comp 2004. The
  board game as IF: one location, one die, play solo until you reach square 100.
- **Result:** **WIN** — ends `Yes! You have made it to the end of the game.` /
  `Congratulations!` Solution: `harness/snakes_and_ladders_solution.txt`; golden
  `snakes_and_ladders_solution.expected.txt`.
- **Walkthrough source:** *Key & Compass* ("Repeat r until you win").

## Derivation

Pure chance, but the `scare` binary's RNG is **seeded**, so a fixed sequence of
rolls deterministically wins. Two `Press a key to continue` pauses open the game
(they eat the archive's `board`/`i` examines), so the solution is: **two blank
lines** (clear the pauses), `x dice`, then `r` repeated. Under the seed the die
reaches square 100 in **39 rolls**; the solution issues a few extra `r`s as
buffer (the game exits at the win, so surplus rolls never execute). Re-blessing
after any RNG-affecting engine change will re-record the roll transcript.
