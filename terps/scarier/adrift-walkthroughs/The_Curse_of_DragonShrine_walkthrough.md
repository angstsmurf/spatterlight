# The Curse of DragonShrine — walkthrough

- **Engine:** ADRIFT 4 (`DragonShrineR43.taf`, "Mystery", InsideADRIFT Spring
  Comp 2004). You are a coachman caught in a storm; two men flag you down
  looking for a drowned girl, and the trail leads into Dragon Shrine castle.
- **Result:** ★ **WON, 95/100 — and 95 is the ceiling on the winning path.**
  See *Scoring* below for why the last 5 can never be banked in a winning game.
- **Solution:** `harness/dragonshrine_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `ended the Curse of Dragon Shrine`.

## Route

The author's own transcript, which ships in the comp package as
`springcomp04.zip → walkthru/walkthru/wt-dragonshrine.txt`, is correct and
complete; it replays with zero parser failures. Two repairs were made:

- his `drop lined, drop spoon` is a typo — ADRIFT splits it on the comma,
  answers `Drop what?` to the first half, and drops the spoon on the second.
  The solution spells it `drop spoon`.
- a `score` is inserted before the final `open door`, so the golden records the
  tally (`Your score is 83 out of a maximum of 100`) before the +12 for the
  escape.

```
(blank line — intro waitkey)
n / n / in / feel wall / pull lever / z / z
x table / take candle / light candle
s / x table / take potion / s / s / w / n
take bottle / move tile / i
w / n / e / take wood / w / s / se / e / up
read floor
n / e / x straw / take potion / w / n / w
take linen / e / n / e / wipe painting / w / w
x table / take bowl / open wardrobe / take cloak / wear cloak
x desk / look under desk / x vase / take flower
e / s / s / s / d / e / ne / n / w
x shelf / take potion / pull book / in / d / e
take body
w / up / out / e / n / nw / w / s
put blue potion in pot / put green potion in pot / put red potion in pot
put flower in pot / put wood in pit / x cauldron / light fire
x sink / take spoon / stir potion
i / drop spoon / drop linen
s / s / s / x spear / pull spear
n / n / d / n / up / up / n
x dragon / take scroll / put body on slab / give potion to woman
read incantation
(blank line — resurrection waitkey)
s / d / d / s / up / s / s
score
open door
```

(one command per line in the solution file; slashes here are only for
compactness.)

## Scoring

Thirteen positive awards in the task dump sum to exactly 100, but two of them
are the two halves of the same scene and are mutually exclusive:

| task | pts | fires when |
|---|---|---|
| 38 `#Stir Potion` | 5 | the cauldron holds the three potions **without** the flower |
| 39 `#Stir Potion Correct` | 25 | the same, **with** the flower in |

Both are `rep=0`, and either one empties the cauldron (`ACT type=0 … v2=0`
hides all three potions and the bottle), so at most one can ever fire. Only
task 39 brews the purple potion, and only the purple potion satisfies task 51
`#Give potion to Jenny purple`, which gates task 47 `#Read Scroll`, which gates
the winning task 53. **A winning game therefore always tops out at 95.**

The other awards, in route order: 2 (pull lever), 4, 2, 2, 2, 10 (`look under
desk` — needs the lit candle), 4, 20 (`take body` — needs the cloak worn, task
36 `#Take Body Correct`), 25 (the potion), 2, 10 (`read incantation`), 12 (the
escape). The dump also holds a −5 and a −1; this route springs neither.

## Notes

- **Two `<waitkey>` pauses**, located with `SCR_MARK_WAITKEY=1`: one in the
  intro (the men at the roadside) and one mid-resurrection, right after
  `read incantation`. Both print `More` with no newline, which is why the
  marker lands mid-line in the transcript. Get the blank lines wrong and the
  command list slips by one — the visible symptom is being caught and killed by
  the search party near the end, which reads like a wrong route rather than a
  desync.
- The endgame is a timed chase (task 47's comment in the .taf literally says
  "_make event to run for endgame race and win task"). It is nevertheless
  slack enough to absorb the inserted `score` turn — verified, the escape still
  succeeds.
- Task 53's restriction is *body held by the player*: you must be carrying
  Jenny when you open the door, so the corpse never gets dropped after the
  slab scene.
- Hints are built in (`HINTQ`/`HINT1`/`HINT2` on most tasks) and the dump
  exposes them, which made confirming the potion-recipe restrictions a read
  rather than a search.
