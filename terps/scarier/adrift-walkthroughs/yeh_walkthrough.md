# The quest ("yeh") — max-score tour

- **Engine:** ADRIFT 4 (`yeh.taf`; the .taf carries no author name and the
  title inside is just "The quest"). A half-built fantasy RPG: 40 rooms, 25
  NPCs, three inns, four shops with a gold-piece economy — and 18 tasks.
- **Result:** **TOUR at the documented max, 3100** — *not* a win. **The game
  has no EndGame action anywhere**, so it cannot be completed. `SCR_DUMP_TASKS`
  reports zero `ACT type=6` in all 18 tasks.
- **Solution:** `harness/yeh_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Marker:
  `Your score is 3100 out of a maximum of 3400.`
- **Provenance:** no author walkthrough exists — plover.net lists the game
  without one. The route was derived from `SCR_DUMP_TASKS`.

## Route

```
point up / drink dragon potion / s / w / s / e / e / u
u / get bow of icy arrows / u / s / drop bow of icy arrows / trikletarts / s / u
destroy tree master / score
```

(one command per line in the solution file; no blank lines — the game has no
`<waitkey>` pauses, checked with `SCR_MARK_WAITKEY=1`.)

## Why 3100 and not 3400

The status line advertises `a maximum of 3400`. The dump contains exactly four
`ACT type=4` awards, and this route fires all four:

| Task | Command | Points |
|---|---|---|
| 1 | `Point up` | 100 |
| 17 | `Drink dragon potion` | 1000 |
| 0 | `Trikletarts` | 1000 |
| 2 | `Destroy Tree Master` | 1000 |

3100. The remaining 300 exists only in the author's declared maximum — same
sort of typo as `tq3.taf`'s "maximum of 2400" for a 60-point game.

## Notes

- **`Drink dragon potion` scores from the starting bedroom.** Task 17 is
  `where=3` (anywhere) with `restr=0` — no restrictions whatsoever. That is an
  authoring oversight, not an engine divergence: ADRIFT 4 task commands are
  literal text patterns, not object references, so nothing checks that you
  hold the potion. The object it destroys (obj22) is carried by the *2nd Small
  Dragon* in room 39, at the far end of the map past an Ogre, and reaching it
  would mean winning a battle-system fight for no extra points.
- **`Trikletarts` wants the bow on the floor, not in your hands.** The
  restriction is `type=0 v2=0 v3=17` — the Bow of Icy Arrows must be *in* the
  Room of portals (room 16). It starts one floor below on Tower Floor 3, so
  the route carries it up and immediately drops it.
- **The parser wants full object names.** `get bow` answers "Take what?";
  `get bow of icy arrows` works.
- `Destroy Tree Master` teleports the player back out of the tower as its
  first action, which is why the route ends there.
- Leon, the companion NPC who follows you out of the bedroom, trades blows
  with the Animated Vine on Tower Floor 3 while you walk past. Combat rolls
  are seeded, so the transcript is stable.
- Everything else in the game — the gold economy (`Point up` also tops up your
  purse), the ale, the pies, the elven marketplace, the Justin sword and
  Greataxe on the weapon rack, the temple, the Frag Grenade — is reachable and
  scores nothing. None of it is on the route.
