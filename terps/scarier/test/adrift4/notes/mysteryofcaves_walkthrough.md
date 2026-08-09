# The Mystery Of The Darkhaven Caves — walkthrough

- **Engine:** ADRIFT 4 (`mysteryofcaves.taf`), David Whyld, Summer Comp 2004.
  A straight treasure hunt: 202 tasks, 29 playable rooms, 6 NPCs.
- **Result:** ★ **125/125, "Godlike Adventurer"** — the game's own maximum, and
  it says so (*"You got the maximum possible score! (Did you cheat?)"*).
- **Solution:** `goldens/mysteryofcaves_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`).
- **Provenance:** the game carries its own walkthrough. Typing `walkthrough`
  at any prompt offers `long` (a full transcript) or `short` (a comma-separated
  command list); `clues` prints a five-screen staged hint list. The route in
  the solution file is the author's `short` list with one command dropped — it
  gives `give necklace to cyana` twice and the second copy only answers
  *"Give what?"*.

## Map

```
                              Goblin cavern (11)
                                     |n
    Island (27) ~swim~ Lake (8)      |
                         |se         |          Mad Tom (14)
                   Junction (7) -e- Crumbling (9) -e- Grisly jn (10) -ne- Curving (13)
                         |se                             |s                 |se
                   Twisty psg (5) -w- Troll (6)       Cells (12)      Mossy cavern (15)
                         |se                                          /   |   \    \
                   Lengthy psg (4) [Cyana]                    Grumbleguts(20) |  Merchant(24)
                         |se                                       |s      Painted(16)   |n
    Grotto (28) -ne- Dead end (3) -ne- Winding (2) -e- Twisty little psgs (1)   |e     Outside hall (25)
                                                        |n    |e                Bridge (22)    |in
                                                   Graves(18) Glowing(21)          |e      Harpist (26)
                                                        |n                    Treasure trove (23)
                                                   Narrow (19) -n- Grumbleguts (20)
                                                        Dark!!! (17) links 16-sw / 21-n / 18-e

                                   Entrance (0) --s--> END GAME (29)
```

## Route

```
n / w / sw / x boulder / knock on door / sw / get all / wear all / ne / ne / e
n / n / n / e / x moss / nw / x corpse / drink vial / ne
talk to mad tom / give book to mad tom / talk to mad tom / sw / w / w / sw / se / w
get meat / throw meat / talk to troll / give toothpick / get thighbone
e / get meat / nw / nw / x lake / throw meat / swim
smash chest / x chest / get seaweed / get scammin's ring / wear scammin's ring
swim / se / se / se / se / e / w / n / dig / x graves / s / nw
talk to cyana / talk to cyana / give necklace to cyana / wear helm
se / n / n / n / e / nw / ne / give seaweed to mad tom / sw / w
n / give thighbone to goblins / s / s / open cells / n / e / se / se
get sword / e / kill sir pargus / e / get treasure / get all / w / w / nw / w
open cage / e / e / n / ne
talk to mad harpist / give cello to mad harpist / give guitar to mad harpist
out / s / talk to merchant / buy diamond / buy pearl / buy ruby
w / se / sw / s / w / s / s / s
```

Two `<waitkey>` pauses, both before the first prompt (title screen and the
instructions screen); the solution file starts with two blank lines for them.
There are no others in the game.

## Where the 125 points come from

25 scoring events, +5 each. 20 of them are ordinary tasks:

| Task | Trigger |
|---|---|
| 4 | `x corpse` in the Curving passage (book + purple vial) |
| 31 | `dig` at the Graves while carrying the shovel |
| 27 | `x graves` afterwards (necklace) |
| 84 | `x moss` in the Mossy cavern (toothpick) |
| 135 | `drink vial` |
| 34 | `give book to mad tom` (shovel) |
| 58 | `give seaweed to mad tom` (key) |
| 64 | `throw meat` in the Troll's cavern |
| 70 | `give toothpick` to Snugg (cello, frees the thighbone) |
| 57 | `throw meat` at the Underground lake (distracts the shark) |
| 55 | `smash chest` on the Island (Scammin's Ring) |
| 94 | `knock on door` at the Dead end |
| 77 | `give bone` to the goblins |
| 88 | `open cells` (the goblins repay the debt; +25 gold and a guitar) |
| 59 | `open cage` in Grumbleguts' lair with the key (+15 gold) |
| 119 | `give necklace to cyana` (helm) |
| 126 | wearing the helm when the dwarf strikes (+15 gold) |
| 108 | `kill sir pargus` holding the sword *and* wearing the armour |
| 112 | `get treasure` in the trove (+55 gold) |
| 91, 92 | `give cello`/`give guitar` to the mad harpist (+5 gold each) |

The last five come from the endgame tally: TASK 137 fires on the second `s`
at the Entrance and runs TASKs 138–142, one per treasure still in hand —
diamond, ruby, pearl, jewelled rod, jade sceptre. TASK 143 then picks a rank
band; ≥125 selects TASK 144, *Godlike Adventurer*.

## Notes

- **The two rings are the map.** Four moves — `nw` from the twisty passages,
  `se` from the Twisty passageway, `n` from the Glowing passage, `e` from the
  Graves — fire TASKs 9–12, which roll a die and teleport you to one of six
  rooms instead. Wearing the **Scammin's Ring** (in the rusted chest on the
  island, `smash chest`) suppresses all four. Separately TASKs 19–20 make the
  Painted passageway unreachable from either side until the **Blocker's Ring**
  is worn; it is in the Hidden grotto, behind the boulder that is really a
  door. `x boulder` notices it, `knock on door` opens it — `smash`, `move` and
  `open` all fail.
- **The helm is a dwarf repeller.** EVENT 0 runs every turn and, via TASKs
  101–104, has a dwarf snatch your sword in the Painted passageway and drop it
  in a random room; there is no way to stop him except TASK 126, which needs
  Cyana's helm **worn**. The runes on it, `exbsg sfqfmmfs`, are a Caesar shift
  of *dwarf repeller*. Without the sword TASKs 110/111 make `kill sir pargus`
  cost you 5 points instead of gaining 5, so the helm has to be collected
  before the first trip east.
- **Cyana takes four different "pretty" things**, and three of them are
  scoring treasures. `give necklace` (TASK 119) is the right one; giving her
  the pearl, ruby or diamond (TASKs 120–122) works identically but throws away
  a +5 at the tally. The clue text warns about this without saying which.
- **Sir Pargus needs both** the sword (TASK 108 restriction 1) and the armour
  **worn** (restriction 2); the armour is in the grotto with the Blocker's
  Ring, which is why `wear all` there matters.
- **Gold is a separate variable from score** and is not listed in the
  inventory; `x gold` or `count gold` reports it. The run finishes 120 gold in
  hand exactly, all of which goes to the gnome for the diamond (60), pearl
  (30) and ruby (30). Killing the merchant is not implemented (TASK 60 just
  refuses).
- **Grumbleguts never appears.** The ogre's lair holds only the caged young
  woman; the clue list says so outright. `kill ogre` (TASK 39) is a joke
  response.
- **The meat is used twice.** `throw meat` in the Troll's cavern scores and
  puts it in the room to the east (TASK 64); pick it up again on the way out
  and throw it into the lake for the shark (TASK 57). Throwing it anywhere
  else (TASK 65) wastes nothing but does not score.
- **Do not type `cunt`.** TASK 183 subtracts 100000 points.
