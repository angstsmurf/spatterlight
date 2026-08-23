# Ba'Roo! — walkthrough (**WIN, 16/16 — full score**)

- **Author:** Eric Anderson (Hensman Int'l), 2010. You slip on some moss in an
  undiscovered Peruvian ruin, wake up in a chamber under it, and climb out into
  a medieval village that turns out to be the *future*: an alien seed-ship has
  taken root, the "wizards" are its historians, and the warlock in the hotel
  basement wants a superconductor.
- **Engine:** **ADRIFT 4.00** (`xxd -p -l 12 games/baroo.taf | cut -c17-22` →
  `93453e`).
- **Result:** **WON, 16 of 16** — the game's own maximum, confirmed by asking it
  (`Your score is 14 out of a maximum of 16` one move before the last `+2`).
  Verified in seeded Scarier (`goldens/baroo_solution.txt`, PASSing golden, win
  marker `Congratulations!`).
- **Source:** `downloaded/BaRoo_walkthrough.html` — the delron ("The Home of
  Otter Interactive Fiction") command list for Ba'Roo!, 178 lines, captured from
  `delron.org.uk/walkthru/Ba'Roo.html` (Wayback 2022-07-06). It replays almost
  exactly; the route as wired is **180 lines** — the published list plus two
  `remove backpack`.
- No `SCR_SKIP_WAITKEY`, no seed pinning, no env at all: the row is a plain
  `baroo_solution.txt|baroo.taf|Congratulations!`.

## Correction — the capsule wants the backpack *inside* it

`lie on capsule` appears twice, once to be flung at the alien ship (TASK 258,
Conference Room) and once to come home (TASK 286, Alien Mound). Both restrict
the backpack to *in the capsule*:

```
TASK 258  cmd=[lie {down} [on/in] capsule]
    RESTR obj1=[backpack]      in container 7  (= the capsule)
    RESTR obj93=[power module] in container 1  (= the backpack)
    RESTR obj94=[unit]         in container 1
    RESTR obj92=[suit]         worn
```

and the published `put backpack in capsule` is answered "You are not holding the
backpack", because the backpack is *worn*, not carried — `wear suit` narrates
"After removing your backpack and belt, you put the suit on", but TASK 300–302
(`#wear suit (w/ belt & backpack)`) put it straight back on you. So each
`put backpack in capsule` needs a `remove backpack` in front of it. Without
that, `lie on capsule` answers "You need to keep your backpack, but wearing it
in the capsule isn't going to work", the walk carries on to `turn on assembled
unit` on the *wrong* side of the trip, and the ship leaves with the bomb still
on the ground: "Ten minutes later you see an explosion in the sky. Better luck
next time."

## The interpreter bug this game exposed (fixed)

The route died much earlier than that on first replay, at `take meat`:

> Unfortunately you don't have a way to eat it, as well as not been invited to
> partake.

That is TASK 62, whose command is **`take/get/eat stew`** — a bare `/` with no
`[]` around it. Scarier's pattern parser treated `/` as an alternatives
separator at every nesting depth, so at top level `uip_parse_list()` returned at
the first slash *without* appending its end-of-string node: the pattern became
`take`, matching any input beginning with "take". Every `take X` in the Communal
Dining Hall was captured by the stew task, and TASK 63 —

```
TASK 63  cmd=[[take/pull/tear] [meat/animal/roast]]   (rep=1)
    ACT  meat_chunks += 1 ; meat_taken += 1
```

— could never run. The meat is the game's only food, and the endgame climb up
the alien root spends two `eat meat`, so the walkthrough was unwinnable.

Ground truth for the fix is run400's matcher itself, `Proc_9_4_45D940` in the
decompiled Runner (`~/Adrift_decompile/run400-analysed/NewParse.bas`): it dispatches on
`Left(pattern,1)` being `[` or `{`, and everything else is a literal run
compared with `Left(input,n) = Left(pattern,n)` up to the next `[`/`{`. It has
no notion of a bare slash at all — so `take/get/eat stew` matches only the
literal string `take/get/eat stew`, i.e. the task is dead, and `take meat` falls
through to TASK 63 exactly as the author's transcript shows.

`scparser.cpp` now counts group depth and parses a depth-0 `/` as a literal word
node. Corpus exposure measured after the fix: 8 games author a bare top-level
`/` across 36 task commands, nearly all of them `#`-labels or dashes
(`-2/3/4`, `1 - kridlor66 who killed you/how died`) that had been quietly
prefix-matching. Zero golden churn across the 190-row v4 suite. Logged in
`RUNNER_TESTS_TODO.md` §4.

## The route

`goldens/baroo_solution.txt`, 180 lines. Where the points are:

| # | command | pts | note |
| --- | --- | --- | --- |
| 15 | `u` | +2 | out of the temple, after the button/pad puzzle in the dark chamber |
| 16 | `shoot wizard with gun` | +2 | the niche's *plastic raygun* — `press silver pad` ×3 then `press gold pad`; the glock kills instead of stunning |
| 26 | `wait` | +2 | TASK 193 — the anti-gravity platform, boarded in the wizard's house, finishes its flight and lands in the hotel parking lot |
| 85 | `d` | +2 | the cliffside rappel: piton, hammer, rope tied to piton |
| 97 | `press retrieve` | +1 | the buried console's log — "Wednesday, August seventh, twenty-three oh seven" |
| 109 | `wait` | +1 | the converter finishes turning six gemstones into the YBCO superconductor |
| 127 | `give ybco object to warlock` | +2 | in the boat-dock storeroom, before she moves upstairs |
| 148 | `lie on capsule` | +2 | after `remove backpack` / `put backpack in capsule` |
| 180 | `lie on capsule` | +2 | the ride home; the ending mood (TASK 294/295/296) pays this once |

Shape of it:

```
rest / rest / stand up            wake in the Dark Chamber
x lights / x buttons / press blue,green,yellow    open the niche
x niche / press silver pad / again / again / press gold pad / take gun
u                                 the plastic raygun stuns the wizard
shoot wizard with gun
(village talk, wizard's house, platform e to the hotel)
ask brogo/figure about ...        the warlock reveals herself
(room 10) ask m'greet about they
rise fly to village               back for supplies
x roast / take meat ×3            three chunks — the climb costs two
sw ... d                          mountain path to the cliffside gemstones:
                                  green, grey, clear, blue, yellow, then
                                  piton+rope to rappel to the brown one
s / search cloth / take flashlight
(dark caves) press retrieve
put all gemstones in machine / close machine / wait / take ybco object
(slide pool) move canoe into water / enter canoe / n,e,s,s,e / exit canoe
give ybco object to warlock
(room 21) take suit / wear suit   the pressure suit
(conference room) l               the warlock opens the capsule
put unit in backpack / put power module in backpack
remove backpack / put backpack in capsule / lie on capsule
                                  mach 6 to the alien ship
e / eat meat / (piton, hammer, rope) u / u / in / u
put power module into unit / turn on assembled unit
rappel d / pull rope / out / tie rope to piton / rappel down ×2 / w
remove backpack / put backpack in capsule / lie on capsule   — WIN
```

The bomb has a spoken countdown ("Seis minutos") from the moment it attaches,
which is why the descent is a straight run of `rappel down` with no detours.
