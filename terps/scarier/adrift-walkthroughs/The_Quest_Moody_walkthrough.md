# The Quest (Chris Moody) — walkthrough

- **Engine:** ADRIFT 4 (`tq3.taf`, Chris Moody, "Beta 2 Build 3", 27 Aug 2000).
  A small fetch-quest: wake in an inn, find the herb the wizard wants, get
  teleported into the cave, and heal what is dying at the bottom of it.
  (Not to be confused with the unrelated adult game also called *The Quest*,
  `fantasyworld.taf`.)
- **Result:** ★ **WON, 60 points** — every point the game can award.
- **Solution:** `harness/tq3_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `you have sucessfully completed my first IF game`.
- **Provenance:** no author walkthrough exists — plover.net lists the game
  without one. The route was derived from `SCR_DUMP_TASKS`.

## Route

```
open dresser / get necklace / get scroll / e / d / s / e / s
s / s / e / e / get rope / e / s / s / s / u
get herb / d / n / n / n / w / n / open door / n / e
give herb to wizard / w / s / s / w / s / get water / shicknaw
s / open box / get latern / light lantern / tie rope to rock / d / s
push river button / s / w / w / n / w / score / give water to unicorn
```

(one command per line in the solution file; no blank lines — the game has no
`<waitkey>` pauses, checked with `SCR_MARK_WAITKEY=1`.)

## Scoring

The status line claims **"a maximum of 2400"**. That is an author typo, not a
missed 2340 points: the dump contains exactly five `ACT type=4` awards and this
route fires all five.

| Task | Command | Points |
|---|---|---|
| 0 | `open door` (Front of house, necklace in hand) | 15 |
| 3 | `shicknaw` | 5 |
| 6 | `tie rope to rock` | 5 |
| 10 | `push river button` | 20 |
| 13 | `give water to unicorn` — EndGame(win) | 15 |

The `score` on the second-to-last line reads `45 out of a maximum of 2400`, so
the finale supplies the last 15.

## Notes

- **The golden necklace is the "special key."** The front door's only
  restriction is `type=0 v1=3` — the necklace held by the player — and the
  game's own hint says so: "You need a special key… Go back to the Inn, and
  look really well." It is in the oak dresser in the very first room, along
  with a scroll.
- **The special herb grows on the red vine at the Tree Top**, four rooms south
  of the Hidden Trail and then up. The wizard in the Lab takes it and hands
  back the blue crystal, which he tells you activates `shicknaw`.
- **`shicknaw` is the only way into the cave half of the map.** No exit
  connects the Cliff to the Mouth of cave — the crystal teleport is the
  bridge that isn't there any more. So the **healing water must be picked up
  at the Cliff before saying the word**; there is no way back for it except
  `wankish`, the return spell (restricted to rooms 26–39).
- **Three instant deaths sit one keystroke away**, and this route dodges all
  of them: `eat herb` (TASK 1), `drink water` (12) and `give water to dragon`
  (14) each run `ACT type=6 v1=2` — EndGame(lose). The unicorn is the *west*
  half of the Lava room; the dragon is the east.
- Two exits are task-gated: `tie rope to rock` opens the descent from the Cave
  Entrance into the Circular Room, and `push river button` drains the river so
  the Control room's south door opens.
- The lantern is a red herring for this route — it comes out of the box at the
  Cave Entrance and `light lantern` swaps the unlit `latern` for the lit
  `lantern`, but the only dark room (30) is off the critical path. It is kept
  in the route because it is clearly the intended preparation.
- The author's spelling is preserved throughout, including the object called
  `latern` before it is lit and "Congradulations" / "sucessfully" in the
  ending — the win marker quotes the typo deliberately.
