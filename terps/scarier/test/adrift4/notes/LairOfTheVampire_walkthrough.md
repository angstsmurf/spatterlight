# Lair of the Vampire — walkthrough (**WIN, 226/271 — 83%**)

- **Author:** Chris Cole (delron / "The Home of Otter Interactive Fiction").
  You wake in a cell under a castle with four other prisoners and no memory of
  how you got there. The castle belongs to Vaslock, lord of the vampires; you
  are Marlon, and the reason you can't remember is the twist.
- **Engine:** **ADRIFT 4.00**
  (`xxd -p -l 12 "games/Lair of the Vampire.taf" | cut -c17-22` → `93453e`).
- **Result:** **WON, 226 of 271 (83%)**. That is a *winning* score, not a
  maximal one, and the game says so itself on the last screen: "Depending on
  the path you took through the game, your final score may well be less than
  100%. There are a good number of tasks you can complete which add to your
  score but which are not required to complete the game."
- **Source:** `downloaded/LairOfTheVampire_walkthrough.html` — delron's own
  command list for the game, 276 lines. The route as wired is **279 lines**:
  the published list plus three, both corrections explained below.
- Row: `lair_solution.txt|Lair of the Vampire.taf|the lord of the vampires, lies dead|SCR_SKIP_WAITKEY=1`.
  The waitkey flag is mandatory — the intro, the archway into the Deathly
  Chamber and the whole ending paginate.

## The interpreter bug this game exposed (fixed)

The published list opens `ask harris about key`, and before the fix that was
answered "Stop bugging me with pointless questions!" — the cellmate holding the
picklock the entire game hinges on was unaddressable, and so was `x harris`.

The game authors a mutually-referring synonym pair, the usual way of letting two
spellings reach one NPC:

```
[priest] -> [vardo]
[harris] -> [steve]
[steve]  -> [harris]
```

Scarier's `pf_filter_input()` walked the input word by word and took the *first*
synonym that matched, spliced it in, and skipped past it — so `harris` became a
`steve` that the character has no alias for, and the second synonym never got to
turn it back. run400 accepts both spellings (verified live under Wine), so later
synonyms *do* act on an earlier one's output.

They do not act on it freely, though. *Yak Shaving for Kicks and Giggles!* maps
`flags`, then `line`, then `clothes` all onto `clothes line`, and run400 still
answers `x flags` with the laundry description — so `line` and `clothes` must
not fire on the words *inside* the `clothes line` that the first synonym wrote.
Letting them isn't just wrong, it doesn't terminate: `x flags` grows one
`clothes line` per pass until the harness's 30 s `ulimit -t` kills the run.

`pf_filter_input()` now splits the difference the way both games demand: at each
word position the first matching synonym fires, and every synonym after it in
the list gets a look at the replacement **only as a whole** — it fires again
only when its original is the entire replacement region, and then replaces all
of it. Zero golden churn across the suite. Logged in `RUNNER_TESTS_TODO.md` §4.

## Correction 1 — the ruined stairs are a coin flip

The published list climbs back up with a single `up` and it doesn't work:

> You move up, stepping as carefully as you can. Alas, it seems, not carefully
> enough. One of the steps gives way beneath your feet, sending you crashing to
> the bottom of the stairs.

The two stair rooms are gated on a variable that is rerolled every turn:

```
VAR 44 [stairs]
TASK 139  where=2 rooms=[11 14]  cmd=[-change stairs]   (rerolls VAR 44)
TASK 140  room=11 cmd=[u]  RESTR var stairs < 3   ACT move player -> room 14
TASK 141  room=11 cmd=[u]  RESTR var stairs > 2   (collapse, no move)
TASK 143  room=14 cmd=[d]  RESTR var stairs < 3   ACT move player -> room 11
TASK 142  room=14 cmd=[d]  RESTR var stairs > 2   ACT move player -> room 11
```

Note the asymmetry, which is why the author never noticed: going *down* lands
you at the bottom either way — gracefully or in a heap. Going *up* on a bad roll
leaves you exactly where you were. So the route needs a second `up`; under the
harness seed the retry lands. (The published `down` at the same spot collapses
too, and that is left alone — it is cosmetic.)

## Correction 2 — two missing moves before the Feasthall

The published list runs `east`, then `ne`, `ne`. There is no northeast exit from
the Eastern passage and never is:

```
EXIT room=18 [Eastern passage] E -> 21   W -> 15      (that is all of them)
EXIT room=22 [Ancient Feasthall] NE -> 28 [Corridor]
EXIT room=28 [Corridor]          NE -> 29 [The Statue]
```

The two `ne`s are Feasthall → Corridor → The Statue, so the list is simply
missing the `east` (Eastern passage → Sloping Corridor) and `north` (Sloping
Corridor → Feasthall) that reach the Feasthall in the first place.

## The route

`goldens/lair_solution.txt`, 279 lines. It opens with `1` (start the game) and
`male` (the game asks). Where the points are:

| # | command | pts | note |
| --- | --- | --- | --- |
| 8 | `ask harris about picklock` | +3 | Lara tips you off first; Harris denies it as a "key", then names it |
| 10 | `open door` | +4 | two turns of picking |
| 12 | `ask vardo about crucifix` | +4 | the dying priest gives it up once the door is open |
| 36 | `pull candleholders` | +3 | opens the dining-room passage |
| 38 | `get weave` | +3 | summons Havelock's spectre |
| 40 | `ask havelock's spectre about morlock` | +2 | |
| 45 | `use salts` | +3 | wakes Lorgrim in the Laboratory |
| 48 | `ask lorgrim about herbs` | +3 | the herb-box key |
| 51 | `x reflection` | +3 | the mirror in the Study — this is the twist |
| 54 | `ask morlock's shade about escape` | +5 | |
| 59 | `open box` | +3 | the herbs |
| 64 | `use herbs` | +5 | revives Garrick, who then follows you |
| 70 | `put coin in statue's hand` | +5 | opens the way south/up |
| 94 | `open coffin` | +3 | inside the Dream |
| 112 | `ne` | +20 | leaving the Dream, all eight gifts delivered |
| 120–135 | `read scroll` ×8 | +1 ea | after `touch <beast>` on each of the eight alcove statues |
| 138 | `say the eight in sequence` | +5 | the figures at the Pit |
| 140 | `raise orb` | +10 | the orb, in the Cells |
| 160 | `throw vial at demon` | +5 | the chalk figure in the Eastern passage |
| 164 | `extinguish fire` | +1 | Goblin Chamber |
| 179 | `use powder` | +5 | Viralee, the singing statue in the Pool |
| 182 | `climb walls` | +3 | the Strange Room |
| 193 | `open drawer` | +3 | Toril's desk — the cobalt key |
| 197 | `give ring to greera` | +2 | the kitchen |
| 203 | `throw dagger at chad` | +7 | the vampire in the blood-daubed Room |
| 204 | `give doll to lara` | +3 | |
| 207 | `read note` | +5 | in the chest, unlocked with the cobalt key |
| 218 | `give cloak to toril` | +5 | back down in the Dimly-lit room |
| 219 | `ask gladrin about orb` | +2 | |
| 231 | `kill torodim` | +7 | the silver dagger, at The Statue |
| 238 | `ask garrick about mordrel` | +5 | Garrick, cowering in the Small Alcove |
| 243 | `say at the end of life lies only death` | +10 | the words at the Top of Steps |
| 249 | `ask mordrel about garrick` | +10 | the vampire in the Crypt is an ally |
| 250 | `give jug to mordrel` | +10 | the jug filled with blood in the Deathly Chamber |
| 256 | `smash door` | +2 | with Gragor |
| 258 / 259 | `x sacks` / `x shelves` | +1 / +1 | the Storeroom — the silver amulet |
| 263 | `kill vampire` | +5 | the orb does it |
| 266 | `raise orb` | +5 | opens the stone block at the end of the Dark Tunnel |
| 269 | `open door` | +5 | Gragor goes first, and dies |
| 270 | `x gragor` | +2 | recover his amulet |
| 271 | `west` | +10 | into the Lair |
| 278 | `kill vaslock` | +20 | the fourth blow — **WIN** |

Shape of it:

```
1 / male                                  the cell, four prisoners
ask lara about harris / about something   Lara saw Harris pocket something
ask harris about key / about picklock     he hands it over
open door / open door                     two turns of picking
ask vardo about crucifix / east           out of the cell
(hallways) x dust / read book ×10         Morlock's journal, the whole backstory
x robes / get salts
(dining room) pull candleholders / get weave / talk to havelock's spectre
(laboratory) use salts / ask lorgrim about herbs
(study) look into mirror / x reflection   you are a vampire
talk to morlock's shade / ask about escape
open box / use herbs / ask garrick to follow you
put coin in statue's hand                 south and up now open
(library) x bookshelves / x scroll        the eight-beasts spell
south                                     into the Dream
give spear/crown/sword/cross/goblet/mirror/stake to the seven sleepers
open coffin / ne                          +20, out of the Dream
(alcove hall) touch <beast> / read scroll  ×8, in order
say the eight in sequence / (cells) raise orb
down / north                              back to the hallways
south / up / up                           <-- the coin-flip stairs
north / east / east                       throw vial at demon
(third level) extinguish fire / use powder / climb walls
(Toril's chambers) get cloak / rub faceplate / get hood / wear hood / open drawer
(kitchen) give ring to greera / get jug
(Room) throw dagger at chad / give doll to lara / open chest / read note
down / (dimly-lit room) give cloak to toril / ask gladrin about orb
up / north / east / east / north / ne / ne     <-- the two missing moves
kill torodim / north / fill jug
(alcove) ask garrick about mordrel
say at the end of life lies only death / down
open coffin / talk to man / ask mordrel about garrick / give jug to mordrel
(junction) east / smash door / se / x sacks / x shelves
west / west / kill vampire / west / west / raise orb
west / west / open door / x gragor / west
2 / open drapes / raise orb / kill vaslock ×4     — WIN
```

The endgame is a fixed four-blow sequence: the crucifix twice (he takes it), then
the silver dagger twice. `open drapes` and `raise orb` first — daylight and the
orb are what make him vulnerable enough for the dagger to land.
