# ADRIFT Maze — walkthrough

- **Engine:** ADRIFT 4 (native). Dana Crane (as "Mystery"), ADRIFT 4th 1-Hour
  Comp 2004. Navigate a twisty maze and grab the trophy; creatures teleport you.
- **Result:** **WIN** — ends `Congratulations!  You WIN!` Solution:
  `harness/adrift_maze_solution.txt`; golden `adrift_maze_solution.expected.txt`.
- **Walkthrough source:** *Key & Compass* (native-ADRIFT).

## Derivation

The game opens with a **`Please enter your name:` prompt** that eats the first
move (`n`), throwing the whole maze path off. Fix: prepend a name line
(`adventurer`) so the direction sequence starts intact. A troll mid-maze
sprinkles "magic dust" and teleports you randomly — but the `scare` binary is
**seeded**, so the teleport is deterministic and the archive's fixed direction
sequence lands in the Trophy Room every time. `take trophy` (`it`) wins.
Re-bless after any RNG-affecting engine change.
