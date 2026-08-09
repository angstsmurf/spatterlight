# Apprentice of the Demonhunter — walkthrough

- **Engine:** ADRIFT 4 (`demonhunter.taf`, **3rd ADRIFT One-Hour Game
  Competition**, 2003). Your mentor has sent you out to kill your first demon.
- **Result:** ★ **WON, 6/6 MAX.**
- **Solution:** `goldens/demonhunter_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `journey to the beginning of your new life. You're a demonhunter.`

## Structural verdict

Only 5 tasks, 7 rooms, two demon NPCs. Three scoring actions exist in the whole
game — `+2` (task 0), `+1` (task 2) and `+3` (task 4) — so the maximum is 6.
Task 4 is also the `EndGame(win)`.

## Route

```
n
e
s
take plate
n
n
take water
s
w
n
take orb
put orb in water
s
w
hit demon with plate
```

## Notes

- **All three objects are hidden inside scenery** and are never listed in a room
  description:

  | object | where | reveal with |
  |---|---|---|
  | ceramic plate | A House (`s` from Near Some Houses) | `x table` |
  | flask of water | stone foundation (`n` from Near Some Houses) | `x foundation` |
  | black orb | ruined pulpit (`n` from the village centre) | `x pulpit` |

- Task 0 (`put*orb*water`) wants all three held at once and is defined in every
  room **except** the corn field. It summons the demon.
- **The finish is on a seven-turn clock.** Walking into the corn field fires
  task 2 (`#player encounters demon`), which teleports you into the church and
  starts `EVENT 1 [demonattacktwo]`; seven turns later that runs task 3
  (`#playerdeath`, `EndGame(death)`). Kill it immediately.
- Task 4 (`hit*demon*plate*`) needs only the plate held — so do **not** put the
  plate down anywhere along the way.
