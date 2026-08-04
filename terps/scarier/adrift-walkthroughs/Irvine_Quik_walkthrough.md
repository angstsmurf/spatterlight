# Irvine Quik & the Search for the Fish of Traglea — walkthrough (**WIN, no score, 179 commands**)

- **Game:** *Irvine Quik & the Search for the Fish of Traglea* by Duncan
  Bowsman, 2012. A six-chapter Saturday-morning space serial: you are a cadet
  on the I.S.C. *Sagittarian*, the planet below is populated entirely by cats,
  and your robot sidekick HiRBy does all the reaching.
- **Engine:** **ADRIFT 4.00** — the 14-byte header is
  `3c 42 3f c9 6a 87 c2 cf 93 45 3e 61 39 fa`, `V400_SIGNATURE` verbatim
  (`sctaffil.cpp:55`). The title screen calls itself "Version 3. 3 November
  2012" and says so out loud: *"Be aware, this game is intended to run on the
  ADRIFT 4 Runner. It is not compatible with ADRIFT 5."* 62 rooms, 366 objects,
  **1637 tasks**, 186 variables, 57 events.
- **Result:** **WIN** — the fish are lifted out of the secret aquarium's
  reservoir, `o` (out) ends chapter 6, and the game runs EPILOGUE → *"Thus one
  courageous space cadet saved the fish, the people, and the planet of
  Traglea"* → **THE END** → the sequel tease, *"Irvine Quik & the Escape from
  the Ralthagorian Ship of Doom!"*
- **Score: there is none.** `score` answers *"Irvine's score is 0 out of a
  maximum of 0. (0%)"* on turn 1 and on the last turn alike — there is not a
  single `ACT type=4` in the whole task dump. Reaching the epilogue is the only
  result there is, so the win marker is the epilogue's opening line rather than
  a score.
- **Harness row:** `iqsfot_solution.txt|iqsfot.taf|Thus one courageous space
  cadet saved the fish|SCR_SKIP_WAITKEY=1`, PASSing golden.
  `SCR_SKIP_WAITKEY=1` is needed: the game opens on a menu title screen behind
  a *"press almost any key to continue"*, and the epilogue alone has three more.
  The marker is deliberately short — the full sentence wraps across two output
  lines and the runner matches markers with `grep -F` against single lines.
- **Sources:** `downloaded/IrvineQuik_walkthrough.pdf` (upstream
  `IQSFoT_walkthrough_document.pdf`, IF Archive comp2012) — the author's own
  12-page illustrated walkthrough, one chapter per spread, with the commands
  typed out. It is a genuine oracle for chapters 1–4 and 6 and it is the only
  reason the two door codes and the landing procedure are knowable. It is
  **not** a replay: six of its steps do not work as printed, and its chapter 5
  hand-waves the entire combat game (see below).

## The game has no score, and that changes what "maximal" means

Every other route in this corpus is judged by a number. This one cannot be.
There is not a single `ACT type=4` in the whole 1637-task dump, and the status
bar carries a chapter counter (VAR 152 `chapter`) instead of points. What the
game *does* track is optional content — the fan servant, the Xomagroth, the
five different ways to land — none of which is rewarded and none of which
gates the ending. So this route is "shortest sane path to THE END with the
optional scenes the epilogue actually calls back to", and the one optional
scene it keeps is chapter 6's fan servant, because the last page of the game
pays it off (*"Where's your coat?" / "Gave it away."*).

## Chapter 2 has five ways to land, and only one of them is deterministic

The PDF's chapter 2 page announces *"There are three solutions to landing the
spaceship"* and then lists **five**, split by whether you answered the
Captain's *"Think you know how to do it, meow?"* with yes or no:

| # | Solution | Why the route doesn't use it |
| --- | --- | --- |
| 1 | `x goggles`, `wear goggles`, then land it yourself | *"Memorize and recall seven random digits at least 3/5 times"* — a genuine RNG memory minigame. Unusable in a golden. |
| **2** | **get the Captain to land it** (flight sheet → green → blue → cover → switch → hand him the sheet) | **used.** Fixed button order, no randomness, no NPC state to set up. |
| 3 | get the First Mate to land it | requires the Captain *unconscious*, i.e. answering "yes" and then breaking something. |
| 4 | have Dr. Voss revive the Captain, then land normally | ends in the same seven-random-digit minigame as #1. |
| 5 | get Nika to reverse the artificial gravity's polarity | a side trip to the art-grav core for a scene the epilogue doesn't need. |

Answering `no` to the Captain (command 13) is what opens solutions 1 and 2, so
the route says no and takes #2.

## The six PDF steps that don't replay as written

All phrasing or a missing beat — none of them a defect in the game.

1. **`open hirby's compartment` → `open compartment`.** The possessive form
   parses as nothing (*"Open what?"*), and `get papyr` only becomes available
   once the compartment is actually open (*"Take what?"* otherwise).
2. **`x card` / `x card key` (chapter 4) has no object behind it** —
   *"Irvine sees no such thing."* It is cosmetic; dropped.
3. **`get hairball` needs a `forward` in front of it.** The PDF lists it right
   after `give flower to smitty`, but giving Smitty the flower teleports Irvine
   from the LABORATORY to the INFIRMARY, and the hairball is back in the lab.
4. **The jungle exit loses a `w`.** The PDF's `retreat, S, W, S, S` leaves you
   short: a stalagmite trip at the CAVE MOUTH eats the first `w` as a turn.
   The working form is `retreat, s, w, w, s, s, s`.
5. **Chapter 5's *"fighting your way past any enemies"* is the whole chapter.**
   The PDF gives the compass route and the vulnerability table and nothing
   else; every attack in commands 115–160 had to be derived.
6. The PDF's chapter-5 order (`claw elite`, then `unlock door`, then `w`) is
   backwards in practice — unlock first, because after the elite falls the
   respawns arrive and you want the door already open.

## Chapter 5 is a real combat system

This is where the derivation went, and it is worth writing down because the
chapter looks unwinnable until you see the timers. Everything below is from
`SCR_DUMP_TASKS` (tasks 1217–1361, NPCs 16–20).

### The vulnerability matrix is real, and the task copies are noise

|  | blocks | folds to |
| --- | --- | --- |
| sentinel (the doorman) | — | anything |
| sentry | sweep, throw | **punch, kick** |
| guard | punch, throw | **kick, sweep** |
| patrol | punch, kick | **sweep, throw** |
| soldier | kick, sweep | **throw, punch** |

Each of the four attacks exists in four counter-gated copies (`RESTR type=4`
on VAR 36–39, `Punch #`/`Kick #`/`Sweep #`/`Throw #`) so the prose rotates as
you repeat yourself. The copies are cosmetic: every copy of a *correct* attack
does the same KO.

### The palace cannot be cleared, only outrun

There are exactly four mooks in the building — one NPC each — and a KO is not
the end of any of them. **EVENTs 15–18 `[Sentry/Guard/Patrol/Soldier
Respawn]`** restart each one on its own timer — **7**, **9–14**, **6–10** and
**9–11** turns — into *whatever room the player is standing in*. That is what
*"A sentry charges in"* and *"Patrol charges after Irvine"* are.

And you cannot walk out of a room with anyone in it: *"Irvine has to deal with
his enemies before he can leave!"* So every doorway costs a full sweep of
whoever has cycled back in. Commands 134–138, 144–149 and 155–159 are those
sweeps, and the doubled `sweep patrol` / `kick guard` in them are **not**
missed swings — they are the respawn timer landing on the very turn of the KO.
Because the four timers are staggered, a room is empty for a turn or two at a
time, and the route's whole job is to spend that turn moving.

### `claw` is an area attack held in reserve for the elite

`* claw *` is **TASK 1217**: it dispatches at every enemy in the room at once
(tasks 1225/1242/1259/1276 for the four mooks, 1292 for the elite). It is
gated on `RESTR type=4` → `claw_count >= 3` and its own actions reset
`claw_count` to 0, and every attack you make — hit or miss — bumps the counter
by one. So it recharges over three swings and you get one charge at a time.

**TASK 1292 `#elite_clawed_(POW!)`** is the only thing in the game that
touches the elite guard, and it carries nine restrictions: the elite in the
room, and four *"NPC not in the room"* clauses, one per mook. That is the
PDF's *"the elite can only be defeated if it is the only enemy present"*,
spelled out in the data. The route therefore banks a full claw charge, clears
the corridor, opens the door first, and spends the charge the moment the elite
is alone.

### Health is a damage counter with a mirror

* **VAR 41 `Irvine_Health` is damage taken**, starting a fight at 0. Each
  `#<mook>_attack` adds 1 — per enemy in the room, per turn. Three in a room
  is three a turn.
* **VAR 63 `HP` is only the display.** TASK 1342 `###IRVINE_HEALTH###`
  dispatches TASK 1349–1361 (`#IrH0`…`#IrH12`), each of which sets
  `HP = 12 − damage`.
* At damage 12, TASK 1343 `#Irvine_LifeCheck` fires; inside the palace (rooms
  42–53) TASK 1347 `#imprisoned!` throws Irvine into room 62. That is the
  **IMPRISONED!** ending, and this route hit it twice during derivation, both
  times by letting three enemies stack up in one room.
* **TASK 1348 `#heal_over_time`** takes one damage back off, and **EVENT 45
  `[Heal Over Time]`** runs it every 3–6 turns — automatic, unaimable.
* `breathe` also takes one off per turn, but it is refused unless Irvine is
  alone. It genuinely works (9 → a full 12 in three turns), and the route
  still doesn't use it: the next respawn lands on the fourth quiet turn
  wherever you are, so topping up just hands the wave back at the wrong
  moment. Going straight through is cheaper. The route enters the throne hall
  at HP 9 and finishes chapter 5 at HP 9.

### The health items are unreachable

The tasks talk about a health pill (TASK 349/350/609/887) as though you could
find one. **obj310 has no `Where` node at all**, and no action anywhere in the
game moves it — `SCR_DUMP_OBJLOC` reports `pos=-1 room=-1` from load to
epilogue. So the automatic 3–6 turn regen is the entire healing economy.

## The route

179 commands. `harness/iqsfot_solution.txt`.

| # | Command(s) | What it does |
| --- | --- | --- |
| 1 | `play` | the title screen is a menu (PROLOGUE / SPECIAL COMMANDS / PLAY / CHAPTERS / RESTORE / CREDITS); `play` starts chapter 1 |
| 2–3 | `no`, `no` | *"Have you played interactive fiction before?"* and its follow-up. Saying no keeps the tutorial patter, which is harmless and keeps the transcript honest about what a first-time player sees |
| 4–9 | `x shelf`, `deploy`, `x glasses`, `take it`, `take it`, `deploy` | IRVINE'S CABIN. The glasses are on a shelf Irvine can't reach: `deploy` launches HiRBy, the first `take it` has HiRBy grab them, the second has Irvine take them off HiRBy, the second `deploy` folds the robot back up |
| 10–12 | `out`, `port`, `forward` | to the SHIP'S BRIDGE → **chapter 2** |
| 13 | `no` | *"Think you know how to do it, meow?"* — no, which is what unlocks landing solutions 1 and 2 |
| 14–21 | `x flight panel`, `get flight sheet`, `read flight sheet`, `press green`, `press blue`, `open cover`, `flick switch`, `give flight sheet to captain` | landing **solution #2**: prime the panel, then hand the Captain the checklist and let him fly it. The ship lands |
| 22–31 | `aft`, `starboard`, `starboard`, `open compartment`, `get papyr`, `read papyr`, `press 3142`, `x locker`, `get pack`, `wear pack` | back to IRVINE'S CABIN. HiRBy's own compartment holds the papyr with the locker code; **3142** opens the locker and the knapsack is inside |
| 32–39 | `port`, `port`, `port`, `aft`, `port`, `get coat`, `put coat in pack`, `close pack` | the ship's closet. The tiger coat *is* the fur, and it has to leave the ship inside a closed pack or the First Mate confiscates it |
| 40–42 | `starboard`, `forward`, `down` | out the exit hatch → **chapter 3**, on Traglea |
| 43–47 | `e`, `s`, `x brook`, `get fish from brook`, `n` | JUNGLE GROVE → BROOK. The first fish — and the whole reason for the trip |
| 48–51 | `e`, `s`, `get traglenip`, `n` | A BUSH OF TRAGLENIP, south of the city gate: catnip, on a planet of cats |
| 52–55 | `challenge grastor`, `y`, `drop traglenip`, `retreat` | the game of cat and mouse, in the CEREMONIAL ARENA. You cannot beat Grastor here; you drug him and run |
| 56–61 | `s`, `w`, `w`, `s`, `s`, `s` | CAVE TUNNEL → CAVE MOUTH → CROOKED TREES and out. The extra `w` is the stalagmite trip at the CAVE MOUTH (see above) |
| 62–68 | `e`, `s`, `s`, `push crate n`, `push crate n`, `push crate w`, `climb crate` | a monkey has stolen the pack up into the JUNGLE GROVE's trees. Neither Irvine nor HiRBy can reach it alone, so the crate from the WATERFALL gets pushed north-north-west and becomes a step-ladder |
| 69–73 | `deploy`, `get pack`, `get it`, `deploy`, `get coat` | HiRBy fetches the pack, Irvine takes it off HiRBy, and the coat comes out of it |
| 74–75 | `e`, `give coat to drash` | OUTSIDE CITY GATE. Drash the guard wants fur; the tiger coat buys the way into Ki'Parandazar |
| 76–79 | `e`, `e`, `in`, `yes` | MASTER MOJI'S DOJO → **chapter 4**, the combat tutorial |
| 80–83 | `punch dummy`, `kick dummy`, `sweep him`, `throw him` | the four attacks, learned in order |
| 84–88 | `w`, `w`, `w`, `n`, `n` | back to the dark jungle for the rematch |
| 89–93 | `punch grastor`, `kick grastor`, `throw grastor`, `claw`, `breathe` | three attacks charge the claw, the claw finishes Grastor, and `breathe` is taught on the spot — both are needed in chapter 5 |
| 94–100 | `s`, `s`, `w`, `u`, `s`, `d`, `s` | back aboard, down to the ship's LABORATORY |
| 101–103 | `give flower to smitty`, `forward`, `get hairball` | Smitty the Cat joins the Traglean Resistance. Handing him the flower teleports Irvine to the INFIRMARY, hence the `forward` before the hairball |
| 104–111 | `port`, `up`, `port`, `down`, `e`, `n`, `n`, `w` | off the ship and out to the TOP SECRET ENTRANCE |
| 112–114 | `x keypad`, `press 98843`, `in` | the palace back door → **chapter 5** |
| 115–118 | `punch sentinel`, `n`, `punch sentry`, `n` | the sentinel at the door folds to anything; the sentry to a punch |
| 119–122 | `sweep guard`, `sweep patrol`, `throw patrol`, `e` | the first stacked room. Nothing leaves until the room is empty |
| 123–128 | `punch soldier`, `s`, `s`, `punch sentry`, `s`, `e` | following the black tube down to the STORAGE ROOM |
| 129–133 | `x crates`, `deploy`, `get key`, `get key`, `deploy` | the important-looking key, out of reach on the crates — the same HiRBy two-step as the glasses, done while enemies are cycling in |
| 134–139 | `kick guard`, `throw patrol`, `throw soldier`, `punch sentry`, `throw patrol`, `w` | clearing the storage room to leave it. The second `throw patrol` is the 6–10-turn respawn landing on the KO turn |
| 140–150 | `n`, `n`, `n`, `n`, `sweep patrol`, `sweep patrol`, `punch sentry`, `throw soldier`, `kick guard`, `kick guard`, `w` | four rooms north, then the corridor sweep that buys the exit west |
| 151–154 | `n`, `w`, `unlock door`, `claw elite` | THRONE HALL ENTRANCE. Unlock **first**, then spend the banked claw charge the turn the elite is alone (TASK 1292's four "not in room" clauses) |
| 155–160 | `punch sentry`, `sweep patrol`, `sweep patrol`, `kick guard`, `throw soldier`, `w` | the wave that arrives immediately after the elite falls; `w` through the now-unlocked door → **chapter 6** |
| 161–164 | `teach fan karate`, `give jacket to fan`, `ask for help`, `w` | the fan servant. Optional by the PDF's own admission; kept because the epilogue calls back to the given-away jacket |
| 165–172 | `x fat cat`, `x top hat`, `x end table`, `x mirror`, `deploy`, `get mirror`, `look`, `get mirror` | ROYAL THRONE ROOM. The fat cat monologues with a microwave cannon pointed at you; the examines are the stall, and the mirror (again, HiRBy fetches, Irvine takes) turns the cannon back on him |
| 173–176 | `n`, `x reservoir`, `get drowsy fish`, `get tranquil fish` | the SECRET AQUARIUM behind the bookshelf — the fish of Traglea, at last |
| 177–179 | `s`, `e`, `o` | out past the wreckage of the throne room to the PALACE GATES → EPILOGUE → **THE END** |

## Reproducing

```sh
cd terps/scarier/adrift-walkthroughs
sh harness/run_v4_walkthroughs.sh iqsfot   # PASS against the committed golden
```
