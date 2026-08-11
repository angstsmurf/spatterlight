# The Adventures of Bob Bobsly — walkthrough

- **Engine:** ADRIFT 3.9 (`BobBobsly.taf`, 10,310 bytes). You are Bob Bobsly,
  "the dumb peanut butter hating secret agent" of the SSVSSS, sent into the
  Pennington Nightclub after Ihava Headache. 10 rooms, 10 objects, 2 NPCs,
  16 tasks, no events.
- **Result:** ★ **WON, 155/155** — all ten `ACT type=4` tasks in the file.
- **Solution:** `goldens/bob_bobsly_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `Be sure to play, Adventures of Bob Bobsly 2`.
- **Provenance:** no published walkthrough, and the manifest row has no source
  URL (`no online source found`). Derived from `SCR_DUMP_TASKS`.

## Route

```
i / score
add yourself to the guestlist          (+15)
n / e / buy a drink / take gum
w / w / w
use toilet                             (+10)
remove air vent                        (+15)
w / n
chew gum                               (+10)
blow bubble                            (+20, drops you into the Office)
yes                                    (+10, Don's phone call)
beam me up scotty                      (+10, warps you to the Dancefloor)
u
buy coffee                             (+10)
use poison pen with coffee
give coffee to guard                   (+20)
n / score
shoot Ihava                            (+35, EndGame win)
```

## Scoring

| Task | Command | Points |
|---|---|---|
| 0 | `add yourself to the guestlist` | +15 |
| 6 | `use toilet` | +10 |
| 2 | `remove air vent` | +15 |
| 4 | `chew gum` | +10 |
| 5 | `blow bubble` | +20 |
| 7 | `yes` / `recieve call` | +10 |
| 15 | `beam me up scotty` | +10 |
| 8 | `buy coffee` | +10 |
| 11 | `give coffee to guard` | +20 |
| 14 | `shoot Ihava` | +35 |
| | **total** | **155** |

That matches the game's own `out of a maximum of 155`, so the route is exact.
The five unscored tasks are `buy a drink` (a refusal — your boss banned
alcohol tonight), `use poison pen with coffee` (the step that converts the
coffee into the poison mixture), and the three ways to lose.

## Notes

- **`take gum` in The Bar is the only findable step.** `chew gum` is a
  `where=anywhere` task whose sole restriction is *holding object 5*, so
  outside the Bar it just says "You don't have the right equipment" — which
  reads like a wrong-verb message rather than a missing-object one. The wad
  hanging off the Bar table is the game's only takeable item and its coin is
  the only money, so nothing after `buy coffee` is reachable without it.
- **You start with everything else** — tuxedo (worn), camera, poison pen,
  9mm handgun, laser writer — and the writer is what TASK 0 checks.
- **The Office Room has no exits.** `blow bubble` (Air Vent Section) uses an
  `ACT type=1` to drop you into room 7, and `beam me up scotty` uses another
  to warp you back to the Dancefloor. `beam me up scotty` is gated on TASK 7,
  the phone call, so answer `yes` before trying to leave — the call text is
  also where the game tells you the command.
- **Two instant losses, both plain compass moves:** `down` in the Air Vent
  Section (TASK 3) and `south` out of the Secret Lab (TASK 13) both carry
  `ACT type=6 v1=2`. The Lab's only listed exit *is* south, so leaving the
  room the way you came in is fatal — shoot first.
- `drink coffee` (TASK 9) and `drink potion` (TASK 12) are unscored jokes; the
  potion one is the poisoned cup you brewed for the guard.
- The win is a defeat in the fiction: Bob misses Ihava "by a good 75cm", the
  villain warps away in the Wapromatic 5000, and the game closes on an ad for
  a sequel.
