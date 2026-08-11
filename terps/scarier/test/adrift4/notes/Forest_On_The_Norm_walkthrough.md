# Forest On The Norm — walkthrough

- **Engine:** ADRIFT 3.9 (`forest.taf`, 8,292 bytes), Tobias Schmitt 2002, a
  *Reality On The Norm* fan game ("DIARY OF AN ALIEN"). 16 rooms, 4 objects,
  4 NPCs, 19 tasks, no events.
- **Result:** ★ **completed.** There is no scoring system —
  `score` answers *"My score is 0 out of a maximum of 0"* — so the finish
  line is TASK 15 `show end`, the credits the Jail text explicitly tells you
  to type.
- **Solution:** `goldens/forest_on_the_norm_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `Thank you for playing my Aliengame`.
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS`
  (the `EXIT … gateTask=` lines are the whole game).

## Route

```
x tree / climb tree / kick tree / Sprich Deutsch mit Baum      (jokes, Wood)
n / e
give easteregg to chicken / i
n / w / unlock keyhole
n / ask roger about way
n / climb big tree
u / e / dive
u / get needle
s / n / talk to snake
u / enter farm
n / show end
```

## The map is a corridor of task-gated doors

Every non-trivial exit in the game is `gateTask=<task> wantDone=1`, and the
gating task always lives in the room the exit leaves from. There is no
puzzle beyond finding the phrasing, and the phrasings are all literal:

| From | Exit | Gate task | Command |
|---|---|---|---|
| Wood (6) | N → Wood (7) | 0 | `give easteregg to chicken` |
| Outside Cave (8) | N → Cave | 1 | `unlock keyhole` |
| Cave (9) | N → Big tree | 2 | `ask roger about way` |
| Big tree (5) | U → Alley | 3 | `climb big tree` |
| Sea (3) | U → YAHTZEEBRAND | 4 | `dive` |
| YAHTZEEBRAND (2) | S → Church | 13 | `get needle` |
| Under church (13) | U → Farm | 17 | `talk to snake` |
| Farm (10) | N → Jail | 14 | `enter farm` |

The ungated links are `0 n 1`, `1 e 6`, `7 w 8`, `4 e 3`, `12 n 13`, and the
two you never need: `14 (Bar) d → Alley` and `13 n → Game over`.

## Objects and NPCs

- The **easteregg** starts in your inventory; the **key** starts *held by the
  Chicken* (NPC 0) and TASK 0's `ACT type=0` hands it over when you trade the
  egg — "Thank you.... uh, there was a key inside." You never have to `take`
  it, and `unlock keyhole` never checks that you hold it.
- The **Needle** sits loose in YAHTZEEBRAND and exists only so `get needle`
  can open the south door.
- **Mika** (NPC 3) starts hidden and is revealed by `enter farm`; **Roger W.**
  is a *Mission Supernova* guest star, which the credits explain.

## Notes

- **`talk to snake` does not talk to the snake.** Under the church the game
  warns "Don`t go north....." — `13 n` leads straight to *Game over*. TASK 17
  answers "I should run.... The Snake don`t want to talk with me," and that
  refusal is what opens the way up to the Farm.
- **The ending is not an EndGame action.** The Jail text says *Type in "show
  end" to see the rest (important)*, and TASK 15 prints the credits with no
  `ACT type=6` — the interpreter is still at a prompt afterwards. That is why
  the row's marker is the credit line and not a score or a death message.
- The eight `* tree` tasks in the starting Wood (`climb`, `kick`, `eat`,
  `kill`, `piss`, `fuck`, `Talk`, and the German `Sprich Deutsch mit Baum`)
  are one-liner jokes with no effect; the route keeps three of them so the
  transcript records them. `cheat now` and `show alien` are two more, usable
  anywhere.
