# Castle Quest — walkthrough

- **Engine:** ADRIFT **3.70** (native, `castle.taf`). Andrew Cornish, 10-06-2000.
  Loot Castle Oggleboggle without stepping into one of its five instant deaths.
- **Result:** **WIN**, 50/50 — ends `Thanks for playing!` Solution:
  `goldens/castle_quest_solution.txt`; golden `castle_quest_solution.expected.txt`.
- **Walkthrough source:** none published. Derived here from a dump of the
  parsed 3.70 game (see `ADRIFT_370.md`).

## Derivation

Only ten tasks exist, and they are cleanly split: five score 10 each and five
kill you. There are no objects to carry, no NPC to satisfy (Sir Badmood only
ever grumbles) and no timers, so the route is just "visit the five scoring
tasks in dependency order and never type one of the five fatal commands".

The four gated exits chain the whole game:

| gate | opened by | opens |
|---|---|---|
| main hallway `n` | `press green button` (King's study) | dining room |
| Brass Hall `e` | `pull blue lever` | circular room |
| circular room `e` | `stand on red floorplate` (balcony) | puzzle room |
| puzzle room | `eight` | teleports you to the treasure room |

so the order is forced. `take treasure chest` in the treasure room is the
winning task (header `WinTask=9`).

**Do not type:** `pull black lever` (Brass Hall), `south` in the death room —
which is why the bone room / Skull Hall / death room branch north of the
circular room is skipped entirely — `stand on white floorplate` (balcony),
`six` in the puzzle room, or `eat hamburger` in the treasure room. The puzzle
room locks its door behind you, so the `eight`/`six` answer is the one place a
wrong guess cannot be walked back from.
