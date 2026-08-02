# Glum Fiddle — walkthrough

- **Engine:** ADRIFT 4 (`Glum Fiddle.taf`), C. Henshaw, Writing Challenges
  Comp 2006. You are a girl disguised as a boy ("Hawnd"); the giant Glum
  Fiddle has kidnapped your parents and you have to walk into his valley and
  walk back out again with them. 117 tasks, 9 rooms.
- **Result:** ★ **WON, 100/100** — all ten of the game's ten-point tasks.
- **Solution:** `harness/Glum_Fiddle_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `Your score:100 out of 100.`
- **Provenance:** no author walkthrough exists. Key & Compass's Writing
  Challenges Comp 2006 page (`adrwcc06.html`) offers a **map only** for this
  game. The route was derived from `SCR_DUMP_TASKS`; the map from the dump's
  `EXIT` lines matches Welbourn's exactly.
- **In-game hints:** the game has its own hint menu (`help`, then `1`…`12`,
  plus `1a` and `11a`). Several entries are room-gated, and the answers are
  numbered one step out of line with their own question titles.

## Map

```
Tiny Gatehouse ─e─ Cave entrance ─e─ Pile of boulders ─n─ Pigsty ─e─ Glacier
                        │                    └─s─ Swamp
                        n (needs a lit lamp)
                   Sloping tunnel ─n─ Mountain Ridge ─d─ Cliff
```

## Route

```
game / say cow / say sandwich / say ache / say hello / wait x4 / e / e
take branch / wait x4
take cushion / take doily / take lamp / take tray / take teapot / take cups / take plank
light lamp with branch
put cushion in sack / put doily in sack / put lamp in sack / put tray in sack
put teapot in sack / put cups in sack
n / open gate / drop plank / s / s / take slime in teapot / n / w
take doily from sack / unravel doily / tie doily to stalagmites
e / n / e / cut the ropes with the dagger / w
get in barrel / wait x8 / get out of barrel
s / w / n / take teapot from sack / pour slime on the ground
n / take cups from sack / drop cups
d / take tray from sack / drop tray / take cushion from sack / throw cushion / jump
```

Six `<waitkey>` pauses are blank lines in the solution file: one before the
opening menu, two after `say sandwich`, one on the third `wait`, two after
`jump`. Do not reflow them.

## Scoring

Ten tasks each add 10 to the score variable; this route fires all ten.

| Task | Command | Points |
|---|---|---|
| 10 | `say ache` (third riddle) | +10 |
| 50 | `open gate` (in the Pigsty) | +10 |
| 38 | `drop plank` | +10 |
| 45 | `light lamp with branch` | +10 |
| 62 | `tie doily to stalagmites` | +10 |
| 48 | `cut the ropes with the dagger` | +10 |
| 53 | `get in barrel` | +10 |
| 66 | `pour slime on the ground` | +10 |
| 73 | `drop cups` | +10 |
| 78 | `drop tray` | +10 |

## Notes

- **You have to answer the dog wrong on purpose.** The three riddles want
  `say cow`, `say sandwich` and `say ache`, and TASK 10 duly swings the iron
  gate open — but TASK 13 (`[e/east]` in the Tiny Gatehouse) *also* requires
  TASK 18, the Courier encounter, and the courier is summoned only by TASK 11
  `[say %text%]`, the catch-all wrong-answer task. So a fourth, deliberately
  wrong `say hello` is mandatory; the little man with the pony and trap turns
  up three turns later, tells you off for being "too stupid to answer his
  three questions", and has the dog open the gate again.
- **Glum leaves the boulders on the fifth turn after you arrive** (EVENT 5 →
  TASK 35 → EVENT 0, four turns). Until then TASK 19 refuses to give you any
  of his belongings ("You can't have that, it's mine!").
- **The hessian sack is the whole trick.** TASK 20 makes Glum snatch back
  every one of his possessions the moment he is in the room with you holding
  them — but TASKs 21/23/25/27/29/33 each carry a *"NOT inside container 0"*
  restriction, and container 0 is the sack you start with. Stow the doily,
  lamp, tray, teapot, cups and cushion in it and he never takes them again.
  The plank (TASK 31) has no such restriction, so it goes across the crevasse
  immediately instead.
- **Take the branch on the turn you arrive, while Glum is still there.**
  `take branch` has to reach TASK 44, which sets the branch's *burning* state;
  the generic TASK 19 comes first in the task list and would hand you a cold
  branch, after which TASK 45 (`light lamp with branch`, which wants the
  branch lit) is unsatisfiable. With Glum present TASK 19 is restricted out.
- **The lit lamp is the only way north** out of the Cave entrance (TASK 43
  checks the lamp's *state*, not where it is, so it can stay in the sack).
- **The chase is five separate death traps on 3–4 turn timers.** Once you cut
  your parents free and step back west, EVENT 13 starts and Glum comes after
  you for the rest of the game:

  | Trap | Prepared by | Kills you if skipped |
  |---|---|---|
  | Cave entrance | `unravel doily`, `tie doily to stalagmites` | TASK 70 `#Trip kill` |
  | Chasm | `get in barrel` before he arrives | TASK 49 `#Chasm kill` |
  | Sloping tunnel | `take slime in teapot` (Swamp), `pour slime on the ground` | TASK 71 `#Slime kill` |
  | Mountain Ridge | `drop cups` | TASK 74 `#mountain kill` |
  | Cliff | `drop tray` | TASK 76 `#cliff kill` |
  | The jump | `throw cushion` | TASK 90 `#die jumping off cliff` |

  There is no slack in the last stretch: `n / take cups / drop cups / d /
  take tray / drop tray` uses every turn EVENTs 17, 18 and 23 allow.
- **The barrel wait is random.** EVENT 13 fires TASK 46 somewhere between 5
  and 8 turns after you leave the Glacier, so the solution waits eight times;
  Glum steps on the plank he cannot see and falls into the chasm, taking it
  with him. Under the harness's fixed seed he goes in on the seventh wait.
- **`open gate` in the Pigsty is worth 10 points and is easy to miss.** It
  lets the two pigs out, which is what makes the lie you tell the Ogre at the
  stalagmites ("this trap is for the two pigs I let loose") work. The game's
  own hint for it is the unhelpful "Simple. You can't."
- The Ogre is pure decoration — he recites the same cow poem every time you
  enter the Cave entrance, and `tie doily to stalagmites` sends him away.
