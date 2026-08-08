# TODO: Derive walkthroughs for the ADRIFT games in `games/`

Goal: produce a verified, reproducible, near-maximum-score walkthrough for each
`.taf` in `games/`, in the style of `Sun_Empire_walkthrough.md` (full command
list + annotated phases + an honest note on any unreachable points and *why*).

These are obscure 2000–2005 ADRIFT comp games with no published walkthroughs
(checked Key & Compass, IF Archive, CASA). We derive them by driving the game
through a headless, deterministic SCARE build and reading its internals.

## 2026-08-04 (latest) — the five missing tafs, wired — **203 PASS** — and an engine bug in every pre-4.0 game

The last five `downloaded/` walkthroughs whose `.taf` was not in `games/`. The
files arrived as `/Users/administrator/Downloads/missing` and are now
`games/imagi.taf`, `games/CD.taf`, `games/Chosen.taf`, `games/TheCellar.taf`
and `games/panic.taf`. All five are wired, blessed and green, and
**`downloaded/` is now fully wired — every walkthrough in it has a game and a
row.** Per-game docs: `ImagiDroids_walkthrough.md`,
`CrimsonDetritus_walkthrough.md`, `Chosen_walkthrough.md`,
`TheCellar_walkthrough.md`, `Panic_walkthrough.md`.

* **ImagiDroids** (Woodfish, 4.00) — the single ending, 20 commands, one
  authorial repair: `open it` after `x clean area` / `take brick` resolves the
  pronoun to the clean area, so it has to be `open brick`. No score system.
* **Crimson Detritus** (Mystery, 2003, 4.00) — ★ **WON 100/100**, all eight
  `ACT type=4`, 16 commands. One repair: the upstream transcript pasted two
  commands onto one line (`take uniform and wear it`), split into
  `take uniform` / `wear uniform`.
* **Chosen** (Ryan J. Bury, ADRIFT MiniComp 2001, 3.90) — ★ **WON 300/300**,
  the game's own stated maximum, 51 commands. The upstream file is **prose
  hints, not a command list**, so the route is derived from the dump. Three
  traps: `pull lever` in room 8 must come before room 7 or unguarded TASK 9
  kills you; every phrasing of `take block` in room 14 is TASK 14, a death; and
  the six sockets have to be spelled **A-D-R-I-F-T** in order. Two repairs the
  route needs: `take belt` first (it starts inside the trousers and TASK 0
  wants it *held*), and full block names only — `take block` is "Take what?".
* **The Cellar** (David Whyld, 2007, 4.00) — the ending, 132 commands replayed
  **verbatim** from the ClubFloyd log of 12 June 2022, nothing repaired. No
  score and no `ACT type=6`; it ends by setting `VAR 12 [game over]`. The game
  also ships **its own 24-command walkthrough** on TASK 1 (`walkthrough`), which
  replays verbatim too — so the path is confirmed from two independent sources.
  The ClubFloyd route is the one wired because it reaches far more of the 141
  tasks. Its two divergences from the log are both **ours to be proud of**:
  ClubFloyd's Floyd plays through stock SCARE ("Welcome to the Cheap Glk
  Implementation" is in the log), so the log is not an oracle, and both places
  we differ are places scarier has since been made more faithful — the
  in-container description and the run400 postfixed one-object format recorded
  in `lib_list_in_object()`, and a real U+2026 that cheapglk folded to `...`.
  Word-level agreement 98.3%.
* **Panic!** (Stewart J. McAbney, 3.90) — the ending, "Your rating is Messiah.",
  69 commands from the author's own full-session walkthrough, replayed
  verbatim. It found a real bug — below.

**The engine bug: the 3.9/3.8 immediate-restart fixup was eating StartText.**
`evt_fixup_v390_v380_immediate_restart()` (`scevents.cpp`) restarted a pre-4.0
`RestartType=1` event by hand — state to `ES_RUNNING`, clock to one less than a
fresh roll — instead of calling `evt_start_event()`. The event *cycled*
correctly, and its TaskAffected kept firing (which is the half §2 of
`RUNNER_TESTS_TODO.md` probed live, and it was never wrong), but its start
actions, StartText included, ran only on the very first start. So an
always-restarting one-turn event printed its line once per game and was mute
after that.

Panic! is built out of these, and two of them carry a StartText and **no**
LookText (`texts=S--`), so nothing else could be printing them. Against the
author's transcript: the priest's cough 66 published / 1 before / **66** after,
the stigmata 9 / 1 / **9**, the shaking 13 / 1 / **13**, the wraith 21 / 1 / 24.

Fixed 2026-08-04: call `evt_start_event()`, then take the one silent turn off
the clock. The length roll must come from `evt_start_event()` **alone** — the
first attempt kept the fixup's own `scr_randomint()` as well, and the extra
roll per restart churned the RNG stream enough to break three previously
winning routes (circus, thetest_win, wrecked). Two rolls where the Runner has
one is its own bug.

**Corpus effect:** 13 of 203 rows gained text, every diff a *pure addition* of
a line that had been silently swallowed. Mostly ambience — troll's "Sid sips
from his mug.", haunt's grandfather clock, wrecked's seagulls, spirits_flight's
wind, tq3's rattlesnakes, colony's lightning, twilight's apparition,
timmy_reid's Billy on his bike, cybercow_win's robot — but four are **plot**:
secret_of_lost_world's "It starts to rain." and "The volcano is erupting.",
marooned's rescue ship coming over the horizon, and the entire paragraph in
alices_restaurant where Obie takes your wallet and your belt at the station.
No route broke, no win marker moved, and `thetest`, `gateway` and `inverness` —
the games probed live in run390 for §2 — are byte-identical. Written up as
`RUNNER_TESTS_TODO.md` **§8**, with two open probes: the wraith's visibility
while you are up the rope (Panic!'s one residual divergence, turns 44–46), and
whether a restarted period with `Time1=5` is 5 turns or 4.

## 2026-08-04 — the six ClubFloyd/hints games — ★ **6 wins** — **198 PASS**

The remainder of `downloaded/`: the six games whose upstream file is a
**ClubFloyd group-play log or a hints file**, not a command list. All six were
unrouted; all six now win. None of the six has a usable score system (five
report 0/0; Cut the Red Wire has a one-point scale and reaches 1/1), so the
ending is the measure in every case. Per-game docs:
`MammothVacuumButtonOfDeath_walkthrough.md`,
`TeenageHeadlessExperiment_walkthrough.md`, `CutTheRedWire_walkthrough.md`,
`IAmTheLaw_walkthrough.md`, `InMemory_walkthrough.md`,
`HappyValley_walkthrough.md`. All six are ADRIFT 4.00. No interpreter bug came
out of this batch — every divergence is the game's own design or the upstream
file's.

* **Cut the Red Wire! No, the Blue Wire!** (David Whyld, InsideADRIFT #41,
  2012). The solution file is one line: `undo`. Every wire and every wait
  kills you on a one-turn fuse; the winning move is to undo the move that
  brought you into the warehouse. It works because **game tasks beat the
  standard library** — `run_game_commands_in_parser_context()` runs before
  `run_standard_commands()` (`scrunner.cpp` ~1616), so the authored `[undo]`
  task fires and `lib_cmd_undo` never does (`SCR_TRACE_MATCH`:
  `MATCH task=0 pattern=[undo] input=[undo]`). Scores 1/1, the maximum. The
  game has **no game-over action at all** — it prints the ending and loops back
  to the warehouse — so the transcript keeps running past the win and the
  appended `quit`/`y` are answered by the game. That tail is in the golden on
  purpose; the marker is the score line ahead of it.
* **`SCR_SKIP_WAITKEY=1` is a correctness flag, not a cosmetic one, for
  I Was a Teenage Headless Experiment** (Duncan Bowsman, EctoComp 2010). The
  game **opens with a fake death** — a joke "I'm afraid you are dead! / You
  scored 0 out of the maximum 0!" screen with a `<waitkey>` on it — *before*
  the first prompt. Without the flag that waitkey eats command #1, the whole
  script shifts by one, and the route silently plays a different game without
  ever erroring. Its one real puzzle is that `put head on body` (TASK 57/58)
  restricts Formula X to `v2=1`, **held**, not merely present, so `get syringe`
  has to sit between `kill gerchis` and `put head on body`.
* **In Memory** (Jacqueline A. Lott, Indigo New Language Speed IF 2011) is a
  counter, not a puzzle. One ending task, `TASK 178 EndGameScene`, gated on
  `RESTR type=4 v1=2 v2=2 v3=7` — variable 0 == 7. Naming one of the seven
  memory objects in Unconsciousness \<2\> drops you into that memory's room;
  each room has 10–20 one-shot answer tasks with dozens of ALTCMDs apiece, and
  whichever one matches sets a text variable, bumps the counter and walks you
  back. The text variables are never read back mechanically, so **there is no
  wrong answer** — 15 commands, and any other legal answer set wins too.
* **I am the Law** (djchallis, Odd Comp 2010) hides its endgame in one
  variable: `make verdict` → verdict=2, naming V → 4 (anyone else → 3),
  `mission` wins on 4 and loses on anything ≥ 3. Three commands would do it
  cold. The route does the real investigation instead, and the one thing it
  actually has to find is the creativity password: TASK 5 needs var4 == 1 and
  sets 2, TASK 6 (`grant`) needs 2 and sets 3, and **TASK 7's pattern is a bare
  `*`** — any other input — which resets it to 1, so a wrong guess drops you
  out of the prompt with no message. `grant` comes from `ask luke about
  password`, after Calvin has established the engine exists.
* **Happy Valley** (Jacqueline H. / "Lumin", 2008) is the only game in
  `downloaded/` whose upstream file *is* a bare command list — and it does not
  run; it reads as if written against a later revision. `x patch`/`x weeds`
  are listed at Outside the Mine but objects 96/97 are in room 0; `n`/`s` are
  listed where room 2 has only E and W; `enter 3436` cannot match TASK 56's
  `enter 3436 *`; and the cup is watered before it is filled. Re-derived from
  the dump. Three real constraints: `give cup to granny` (TASK 46) wants
  `variable 0 == 5` **exactly**, and the pink spotted leaf is a decoy that
  TASK 40 accepts without incrementing; the gardening gloves must be **worn**
  (`v2=2`) for TASK 36, not carried; and `x tools` is the action that places
  the crowbar in the smithy, on a visit you only get once because `n` out of
  it (TASK 2) teleports you away.
* **Mammoth Vacuum Button of Death** (Daniel Airey, New Year's Speed IF 2012)
  is 11 commands and one joke: `strip` and `strip guard` are different tasks
  and you need both, in that order — undressing yourself is what makes the
  guard pass out, and only then can you take his uniform.

## 2026-08-04 — The Fugitive — ★ **WON 656/666 — the reachable maximum** — **192 PASS**

Fourth of the ten `downloaded/` routes with a staged `.taf`, and the first one
**derived from scratch**: **The Fugitive** (Renata Burianova, 2001–2006, TAF
3.90). The upstream file is the author's own page and it is prose only — no
command list, five vehicles described as interchangeable, and from the city
onward it is "go to the motel", "then you go to the factory". 156 rooms, 130
tasks, 36 of them scorers. Details in `TheFugitive_walkthrough.md`. No
interpreter bug this time — every surprise is the game's own design:

* **656 is the ceiling, not a shortfall.** 34 tasks pay 10, one pays 16, one
  pays 310 = 666 exactly. The unreachable one is
  `TASK 73 cmd=[look * mirror] ACT type=4 v1=10`, the rear-view mirror in a
  stolen car. The game *also* declares the input synonym `look` → `l`, and the
  synonym filter runs before task matching, so the pattern's literal `look` is
  never present when the matcher sees the input (`SCR_TRACE_FLAGS=512`:
  `Printfilter: synonym "l in mirror"`). No alternative command, no other task
  references it. run400 rewrites the same way. The route's second-to-last
  command is a deliberate `score` so the golden pins 346 as well as the ending.
* **The same collision, with an escape hatch.** The two fountains are the game's
  seed money and the authored pattern is `[get * coin*]` — but `coins` → `money`
  is also declared, so `get coins` arrives as `get money` and the money-report
  library answers it. The ALTCMD `dive` reaches TASK 81/82 untouched.
* **Take the taxi, not your car.** Both dump you in the same street maze, but
  only the taxi lets you `fight` the driver (TASK 27, +10) and keep his pistol,
  and only *with a pistol in hand* does the punker ambush resolve as TASK 35
  instead of TASK 33 — 35 leaves a dead punker carrying the can of beer TASK 37
  `[drink * beer]` wants (+10). The car's only exclusive scorer is the dead
  mirror task, so the taxi wins 20–0.
* **Three ordering traps.** `jump out` of the train is TASK 29 and its first
  action drops everything held, so the beer has to be drunk in the maze. EVENT
  26 arms six turns after `undress soldier` and the walk to the jeep is five, so
  `get boots` goes first and the undress goes last. `unlock` at the church arms
  EVENT 46, five turns from death in the cemetery, and shovel/dig/score/seal is
  four — so the church interior (the +16) happens before the unlock.
* **`play` is a `set`, not an `add`.** `ACT type=3` on the money variable: 2181
  became 56. Still +10, so it stays, but it has to run after `buy bomb` (500).

`sleep` (which needs `lie on bed`, not `lie down`) flips the clock to 23:00 and
that is the switch for the whole night half of the city, so everything bought or
sold has to be done before it. Needs `SCR_SKIP_WAITKEY=1`.

## 2026-08-04 — Lair of the Vampire — ★ **WON 226/271 (83%)** — **191 PASS** — and a synonym divergence fixed

Third of the ten `downloaded/` routes with a staged `.taf`: **Lair of the
Vampire** (Chris Cole / delron, TAF 4.00), from delron's own 276-line command
list. Details in `LairOfTheVampire_walkthrough.md`. Three things came out of it:

* **A real run400 divergence in the input-synonym filter, now fixed.** The game
  authors `[harris] -> [steve]` *and* `[steve] -> [harris]`, the usual trick for
  letting two spellings reach one NPC. `pf_filter_input()` walked the input word
  by word, took the **first** synonym that matched and skipped past it, so
  `harris` became a `steve` the character has no alias for — `ask harris about
  key`, the published list's fourth command and the only way to the picklock the
  whole game hinges on, answered "Stop bugging me with pointless questions!"
  run400 accepts it (verified live under Wine), so later synonyms do see an
  earlier one's output. But only *as a whole*: **Yak Shaving for Kicks and
  Giggles!** maps `flags`, `line` and `clothes` all onto `clothes line`, and
  run400 still answers `x flags` with the laundry description — applying every
  matching synonym instead loops forever, growing one `clothes line` per pass
  until the harness's `ulimit -t 30` fires. The filter now fires the first match
  at each position and lets later synonyms re-fire only when their original is
  the *entire* replacement region. **Zero golden churn** across the (then)
  190-row suite. Row added to `RUNNER_TESTS_TODO.md` §4.
* **Route correction 1 — the stairs are a coin flip.** TASK 139 rerolls VAR 44
  `stairs` every turn; TASK 140 carries you up only while it is < 3, TASK 141 is
  the collapse. Going *down* lands you at the bottom on either roll, which is
  why the author never noticed; going *up* on a bad roll leaves you put. One
  extra `up`.
* **Route correction 2 — two moves missing from the published list.** It jumps
  from `east` straight to `ne`/`ne`, but those two `ne`s are Ancient Feasthall →
  Corridor → The Statue; the Eastern passage has no NE exit at all. `east` and
  `north` restore the walk to the Feasthall.

226/271 is a winning, not a maximal, score, and the game says so on its own last
screen ("There are a good number of tasks you can complete which add to your
score but which are not required to complete the game"). Needs
`SCR_SKIP_WAITKEY=1`.

## 2026-08-04 — Ba'Roo! — ★ **WON 16/16 (full score)** — **190 PASS** — and a parser divergence fixed

Second of the ten `downloaded/` routes with a staged `.taf`: **Ba'Roo!** (Eric
Anderson / Hensman Int'l, 2010, TAF 4.00), from the delron command list.
Details in `BaRoo_walkthrough.md`. Two things came out of it:

* **A real run400 divergence in the pattern parser, now fixed.** TASK 62's
  command is `take/get/eat stew` — a `/` outside any `[]`. `uip_parse_list()`
  treated that as an alternatives separator at *every* depth, so the top-level
  list returned at the first slash **without appending its `NODE_EOS`**: the
  pattern collapsed to `take` and prefix-matched any `take X` in the room,
  burying TASK 63 (`[take/pull/tear] [meat/animal/roast]`) and with it the
  game's only food. run400's matcher (`Proc_9_4_45D940`, decompiled in
  `~/adrift-battle/decompiled/NewParse.bas`) only ever looks for `/` between
  `[]`/`{}` delimiters, so a bare one is a literal character and the task is
  simply dead. `scparser.cpp` now tracks group depth and parses a depth-0 `/`
  as a literal word node. Corpus exposure: 8 games, 36 patterns, nearly all
  `#`-labels/dashes that had been silently prefix-matching. **Zero golden
  churn** across the (then) 189-row suite; `make -f Makefile.headless test`
  clean. Row added to `RUNNER_TESTS_TODO.md` §4.
* **Route correction:** the published list's two `put backpack in capsule` both
  fail — `wear suit` narrates taking the backpack off but TASK 300–302 put it
  back *on*, and TASK 258/286 want it inside the capsule. `remove backpack`
  before each. Without it the game reaches a losing ending (the bomb is armed
  on the wrong side of the trip) rather than desyncing visibly.

The route needs no env at all and scores **16/16**, which the game itself
confirms (`Your score is 14 out of a maximum of 16`, one move before the last
`+2`).

## 2026-08-04 — resuming the `downloaded/` wiring run — The Dead Man — ★ **WON 41/43** — **189 PASS**

Ten of the 61 files in `downloaded/` had a staged `.taf` but no route (five more
had no game at all; those five arrived on 2026-08-04 and are queued below).
First one back: **The Dead Man** (30otsix, 2003, TAF 4.00, one room), from the
delron command list. Details in `TheDeadMan_walkthrough.md`; the three things
that generalise:

* **`SCR_SKIP_WAITKEY=1` is the right default for any game whose cutscenes are
  `<waitkey>` pauses.** The intro alone carries ten of them
  (`SCR_MARK_WAITKEY=1 | grep -c WAITKEY`), so the published list desyncs on its
  very first command. Without the flag the route needs ~18 blank filler lines at
  counts that have to be bisected per vision; with it, one line is one command.
* **A blackout that lays the player down can also empty their hands.** Here
  `!black-out` (EVENT 2/3/4/5 at turns 15/25/35/40) does exactly that, so
  anything taken out of the first aid kit before turn 15 must also be *used*
  before turn 15. That is the real reason the published route's `get axe` fails.
* **A published route is not a scoring oracle.** This one stops at 32/43;
  `SCR_DUMP_TASKS` lists twelve `ACT type=4` awards and it skips three of them
  (`open panel with scissors` +5, `wear bandage on neck` +3, the security camera
  +1). 41 is the ceiling for a *surviving* player — the last 2 points are
  TASK 19 `shoot myself`, which pays 2 and then fires `ACT type=6` (game over).

## 2026-08-04 — The Timmy Reid Adventure — ★ **WON 360/372** — the corpus is now **188 PASS / 0 NOSCRIPT**

*The Timmy Reid Adventure (The Jonny Reid Adventure — Part II)* (Jonathan R.
Reid, Reidville Adventures, 2000, TAF **3.80**) was the **last NOSCRIPT row in
the MAP**. 102 rooms, 248 objects, 147 tasks, 9 events; **182 commands**,
**360 of 372**, marker `Thanks for getting us back home!`. Full route and
mechanics in `The_Timmy_Reid_Adventure_walkthrough.md`.

The endgame is a sort, and it is hinted nowhere in play: twenty souvenirs →
seven coloured bags (tasks 101–107, fixed membership) → three boxes (108–110)
→ `put the boxes into the garbage container` (111, +50) → `up` in the start
room (112, +100). Every one of those tasks is `where=ALL` and checks only that
the **items** are held — the bags and boxes themselves never have to be
carried, or even looked at, which is the whole reason the route is derivable
without ever opening the kitchen garbage container.

Three things worth keeping:

* **The stepladder is a toggle, and the win depends on it.** `open the ladder`
  (task 52) *drops* the ladder into the room; `climb up the ladder` (51) needs
  52 done. While 51 is done, tasks 53–56/59 intercept `n`/`s`/`e`/`w`/`out` and
  you cannot move — `climb down the ladder` reverses **51 only** (52 stays done
  for good, so typing `open the ladder` a second time would *reverse* it). The
  ladder is needed twice, once for the ABA basketball on the Gym rafter and once
  for the winning `up`, and must be carried across town in between.
* **The twelve missing points are structural.** The skeleton key is mandatory
  (red bag) and sits on Captain Miller's belt; the only route into the OOB
  Police Station is `pull my pants down` (task 19, **−2**) then `piss` (17,
  **−10**, teleports you in). 372 − 12 = **360 is the ceiling**, not a shortfall.
  Task 17 is also a free one-way teleport to the top of Old Orchard St., so the
  route spends it on the outbound leg rather than paying for it twice.
* **Task 85 (+5, the paper and donuts) is fired by an NPC walk, not typed.**
  It is a `$`-prefixed system task on Hovey's walk `CharTask`; carrying the
  *Portland Herald* and a bought box of plain donuts back to the cottage is the
  whole trigger. Its sibling task 93 is the same walk hook firing the *request*
  while the donuts are still on the shelf.

Two dumper bugs surfaced and are worth recording, because they cost most of the
derivation time:

* `dumpv3.py` prints object parents as `in/on ?N` and never resolves them,
  because it reads `o['Container'][1]`/`o['Surface'][1]` — **fields that do not
  exist in the 3.80 OBJECT schema.** The real field is `#SurfaceContainer`
  (1 = container, 2 = surface); rebuilding the candidate list as
  `[i+1 for i,o in enumerate(g.objects) if o['SurfaceContainer'][1] in (1,2)]`
  and indexing it 0-based resolves them correctly (verified against seven known
  placements). For task `move` records the index is `v2-1` into the
  **SC==1-only** list for `inside`, and appears off-by-one for `onto`.
* NPC restrictions are printed with the **raw** value, which is the NPC index
  **plus one** — `npc6 here` is Jon (NPC 5) and `npc23 here` is Sting (NPC 22).

Parser footguns that ate real turns: `take towel` on the deck is swallowed by
task 34, which prints a one-shot failure and moves nothing (use `get towel from
clothesline`); `take donuts` does not parse at all (the object is *a box of
plain donuts*); and there are **two** baseball gloves, so `crappy` / `usable`
are always required.

Suite after wiring: **188 PASS, 0 FAIL, 0 SKIP, 0 NEEDGOLD, 0 NOSCRIPT** —
every `.taf` in `games/` now has a derived, blessed, winning (or
documented-unwinnable) route.

## 2026-08-04 — The Plague – Redux — **VERDICT REVERSED: ★ WON** — suite all-PASS

The "UNFINISHABLE AS SHIPPED" entry below (2026-08-04, three-documents
section) is **wrong in its conclusion** — the game was played to its only
EndGame-win action (task 94) and the golden re-blessed as a ★ WIN: 260
commands, zero parser errors, marker `spilling zombie blood once`.

What stands: the where=0 analysis and both run400 probes. All 243 `[F]/[E]`
combat tasks are dead exactly as documented. What was missed: the author
*also* wired weapon-based auto-resolution into ordinary where=1 tasks, and
the shipped `.doc` route — pole in hand — never needs the combat system at
all. The decisive command is `in` at the Women's Toilets (task 196: pole
held → ExecTasks task 43 `#are zombies killed 4`, the single where=1 task
among the 243, no restrictions — full fight prose, coins awarded, £1.20
complete). The old golden typed `open door` (task 199, the fight-prompt
variant) one command short of the win path while already carrying the pole.
Every other mandatory fight auto-resolves the same way: escalators (task
59), disused tracks/Ray (72), main platform (146), carriage (176 — without
the pole its sibling 177 ExecTasks GAME OVER); the café fight is the
give-water cutscene (373), and the rails-east block's kill flag (var124) is
required by nothing. Full mechanics + route + footguns (the `x`-verb
pattern quirk, `search body`, the cutters carry-limit) rewritten in
`The_Plague_Redux_walkthrough.md`.

Moral for future verdicts: a dead task *system* does not make a game dead —
before ruling UNFINISHABLE, sweep the where=1 tasks that set or ExecTask the
same flags the dead system would have set (here: one `grep` for the kill-flag
setters would have found tasks 59/72/146/176 immediately).

**Runner-proved same day:** SCARE `.tas` saves transplanted into the real
`run400.exe` under Wine at three route positions — the cubicle `in` bypass,
the escalator `d`, and the carriage `d` all print the identical fight prose,
and the session was played on through the final cutscene to `open door`:
full ending text + **Congratulations!** in the Runner status bar. Details in
`The_Plague_Redux_walkthrough.md` §"Runner ground truth".

## 2026-08-04 — Irvine Quik & the Search for the Fish of Traglea — **WIN, no score at all** — 182 PASS

*Irvine Quik & the Search for the Fish of Traglea* (Duncan Bowsman, IFComp
2012, TAF **4.00**, "Version 3. 3 November 2012") is a six-chapter
Saturday-morning space serial with a menu title screen — 62 rooms, 366 objects,
**1637 tasks**, 186 variables, 57 events. The route is **179 commands** and
ends on `o` out of the secret aquarium, which runs EPILOGUE → *"Thus one
courageous space cadet saved the fish, the people, and the planet of Traglea"*
(the win marker; the full sentence wraps, so the marker is only its first
clause) → THE END → a sequel tease. `SCR_SKIP_WAITKEY=1` is required — the
title screen and the epilogue are both behind *"press almost any key"*.

**There is no score system whatsoever.** `score` answers *"Irvine's score is 0
out of a maximum of 0. (0%)"* on turn 1 and on turn 179, and there is not a
single `ACT type=4` in the entire task dump; the status bar tracks a chapter
number (VAR 152) instead. So "maximal" here can only mean *reaches the
epilogue*, and the one optional scene the route keeps is chapter 6's fan
servant, because the last page of the game calls it back (*"Where's your
coat?" / "Gave it away."*).

The oracle is the author's own 12-page illustrated PDF
(`downloaded/IrvineQuik_walkthrough.pdf`). It is indispensable — the locker
code `3142` and the palace keypad code `98843` are not derivable from play —
but it is **not** a replay. Six steps need repair:

* `open hirby's compartment` → `open compartment` (the possessive parses as
  nothing, and `get papyr` only becomes available once it is open);
* `x card` / `x card key` in chapter 4 has no object behind it at all;
* `get hairball` needs a `forward` in front of it, because `give flower to
  smitty` teleports Irvine from the LABORATORY to the INFIRMARY;
* the jungle exit `retreat, S, W, S, S` loses its first `w` to a stalagmite
  trip at the CAVE MOUTH — the working form is `retreat, s, w, w, s, s, s`;
* chapter 5's `claw elite` / `unlock door` order is backwards in practice;
* and chapter 5's *"fighting your way past any enemies"* is 46 of the 179
  commands, spelled out nowhere.

Chapter 2 is worth a note of its own: the PDF says *"There are three solutions
to landing the spaceship"* and then lists **five**. The route takes **#2**
(prime the flight panel, hand the Captain the flight sheet) precisely because
#1 and #4 end in *"memorize and recall seven random digits at least 3/5
times"* — a genuine RNG memory minigame, unusable in a deterministic golden —
#3 needs the Captain knocked unconscious, and #5 is a detour to the art-grav
core. Answering `no` to *"Think you know how to do it, meow?"* is what opens
#1 and #2.

**Chapter 5 is a real combat system**, and it is where the derivation went.
From `SCR_DUMP_TASKS` (tasks 1217–1361, NPCs 16–20):

* The PDF's vulnerability table is accurate — sentry folds to punch/kick,
  guard to kick/sweep, patrol to sweep/throw, soldier to throw/punch, and the
  doorway sentinel to anything. Each attack exists in four counter-gated copies
  (`RESTR type=4` on VAR 36–39 `Punch #`/`Kick #`/`Sweep #`/`Throw #`) but the
  copies only rotate the prose; every copy of a correct attack does the same KO.
* **The palace cannot be cleared, only outrun.** There are four mooks total,
  one NPC each, and a KO is not the end of any of them: EVENTs 15–18
  `[Sentry/Guard/Patrol/Soldier Respawn]` restart each on its own timer — 7,
  9–14, 6–10 and 9–11 turns — *into whatever room the player is standing in*.
  Since you cannot leave a room with anyone in it (*"Irvine has to deal with
  his enemies before he can leave!"*), every doorway costs a fresh sweep, and
  the doubled `sweep patrol` / `kick guard` in the route are the respawn timer
  landing on the very turn of the KO, not missed swings.
* **`claw` (TASK 1217) is an area attack held in reserve.** It is gated on
  `claw_count >= 3` and resets the counter, and every attack hit or miss bumps
  the counter, so it recharges over three swings. TASK 1292
  `#elite_clawed_(POW!)` is the only thing in the game that touches the elite
  guard, and it carries four *"NPC not in room"* restrictions, one per mook —
  that is the PDF's "the elite must be alone", in the data. The route banks a
  charge, clears the corridor, unlocks the door **first**, and spends the
  charge the turn the elite is alone.
* **Health is a damage counter with a display mirror.** VAR 41
  `Irvine_Health` starts a fight at 0 and each `#<mook>_attack` adds 1, per
  enemy per turn; VAR 63 `HP` is only the mirror, set to `12 − damage` by TASK
  1349–1361 (`#IrH0`…`#IrH12`) dispatched from TASK 1342 `###IRVINE_HEALTH###`.
  At damage 12, TASK 1343 fires and, inside the palace (rooms 42–53), TASK 1347
  `#imprisoned!` ends the game — which happened twice during derivation, both
  times from letting three enemies stack in one room. TASK 1348
  `#heal_over_time` gives one back and EVENT 45 runs it every 3–6 turns.
  `breathe` also gives one back per turn but is refused unless Irvine is alone;
  it works (9 → 12 in three turns) and the route still skips it, because the
  next respawn lands on the fourth quiet turn wherever you are. The route
  enters the throne hall at HP 9 and finishes chapter 5 at HP 9.
* The health pill the tasks talk about (TASK 349/350/609/887) is unreachable:
  **obj310 has no `Where` node at all** and nothing anywhere moves it, so
  `SCR_DUMP_OBJLOC` reports `pos=-1 room=-1` from load to epilogue. The
  automatic regen is the entire healing economy.

See `Irvine_Quik_walkthrough.md`. Full corpus after wiring:
**182 PASS, 0 FAIL, 5 NOSCRIPT, 1 NEEDGOLD.** The NEEDGOLD is pre-existing and
unrelated: `haunt_solution.txt` is an untracked solution file for a committed
MAP row whose golden was never blessed or committed.

## 2026-08-04 — Mangiasaur — **WIN, 63/74 = the real ceiling** — 171 PASS

*Mangiasaur* (DCBSupafly, ADRIFT Spring Comp 2011, TAF **4.00**) is a game with
one verb. You are a just-woken dinosaur and everything you do is `eat <noun>`;
the plot is that eating things makes you bigger and bigger things become
edible. 8 rooms, 190 tasks, 33 variables, 33 events. The route is **87
commands** and ends on `eat platter` in the Hall of Humans, which runs the
ending chain TASK 177 → 178..186 and prints *"Thanks for playing Mangiasaur!"*
— the win marker.

**63/74 is not a shortfall, it is the maximum.** Both missing chunks are author
bugs, and proving that was most of the work:

* **10 points — `eat NAMGUAGL` (TASK 76) can never be typed successfully,
  because the NAMGUAGL is never placed in the world.** `SCR_DUMP_OBJLOC` at
  load gives `OBJLOC obj=15 pos=-1 room=-1 … [NAMGUAGL]`, i.e. hidden. Grep the
  entire task dump for obj15 and there are six restrictions and exactly one
  action — TASK 76's own `ACT type=0 v1=7 v2=0 v3=0`, which *hides* it. And no
  event in the game moves any object at all: every `EVENT` line in the dump
  ends `o2=0->0 o3=0->0`. So the three warning tasks (TASK 77/78), the death
  (TASK 79 `# DEATH BY NAMGUAGL`) and EVENTs 16/26/27 `FEAR THE NAMGUAGL2/1/0`
  are all dead code. This was found the hard way — a probe stood on the Forest
  Floor for fifteen turns waiting for a monster that does not exist.
* **1 point — TASK 123 `eat mutilated carcass` carries two `ACT type=4`
  actions (+5 and +1).** `task_run_change_score_action` guards with
  `increase_score = !gs_task_scored(game, task)` and only lets
  `version <= TAF_VERSION_380` re-score, so a 4.00 game pays the first and
  drops the second. Same once-per-task rule that has bitten several earlier
  corpus entries.

**Eight counter variables are declared, read, and never written** by any
`ACT type=3`: `eatenMoths`, `eatenBugs`, `eatenMoss`, `eatenHoppers`,
`eatenBuzzBirds`, `eatenBushes`, `eatenRoots`, `hunterHasSpear`. So the
escalating bat attack (TASK 49, gated on `eatenBugs == 5/10/15`) can never
fire, moss/bugs/hoppers/bushes are infinite — and, visibly in the golden,
**two of the eight ending "you taste. . ." paragraphs can never print**
(TASK 179 moss, TASK 185 roots). The winning transcript shows six.

The one genuinely hard piece of engine behaviour is the air sac. `eat air sac`
(TASK 86) sets `carcassEdible = 1`, which does double duty: it suppresses the
drown timer (`down` runs TASK 101 only when `carcassEdible == 0`, and TASK 101
is what starts EVENT 28's 5-turn countdown to TASK 89 `# Drown`) and it is the
gate on eating the carcass up on the mesa. But **EVENT 17 has `restart=0`** —
the *first* sac you ever eat starts a 10–20 turn countdown that runs exactly
once, and it ends by executing TASK 88, which sets `carcassEdible` back to 0
and drowns you if you are still underwater. Eating more sacs does not restart
it and does not protect you from it. Two drafts died to this: one drowned
mid-ocean, one reached the mesa and got *"The stench of it is too much. Maybe
if you held your breath…"* (`hold breath`, TASK 125, is flavour with no
actions). The route therefore eats one sac, dives immediately, clears the ocean
in six turns, and then eats **five sacs in a row** in the valley so that the
one-shot fires (command 74) and the sac after it (75) sticks for good.

Two more facts worth keeping: `burp on sap` (TASK 162) is not a door-opener but
an `ACT type=1` that *moves the player to the Mesa Top* — it is the rocket tree
the credits thank Foam for — and it needs `canBurp`, which comes from eating
the hut's still-lit torch. And `eat moth` needs `timeSinceChirp <= 1`, which
stops being a constraint the moment you chirp outside the cavern: EVENT 10's
pause task is TASK 38 `# Chirp Empty Response`, whose `WHERE_ROOMS` is every
room *but* the cavern, so one canopy chirp pins the ticker at 0 forever.

The ClubFloyd session of 2012-02-12 (`downloaded/Mangiasaur_clubfloyd.html`,
247 commands) did reach `eat platter`, so it is a winning oracle in principle,
but half of it is jokes and it has `undo` in it. It confirmed the endgame shape
(spear → hole → `d` → platter) and nothing else. The game also ships its own
hint list under `walkthru` (TASK 189), which gives the progression spine:
cocoon → fly, torch → burp, grass → beetles → sharp teeth, bark → sap → mesa,
spear → vomit into hole.

See `Mangiasaur_walkthrough.md`. Full corpus after wiring:
**171 PASS, 0 FAIL, 16 NOSCRIPT.**

## 2026-08-04 — A Fine Day for Reaping — **WIN, all five souls** — 170/170 PASS

*A Fine Day for Reaping* (James Webb / revgiblet, IFComp 2007, TAF **4.00**) is
the first game in the corpus where the shipped walkthrough deliberately
**refuses to be a walkthrough**. The author documents 2–3 independent solutions
for each of the five souls and says so up front — *"this walkthrough will simply
detail the different ways to bring the individual souls to justice"* — because
the ending prints a different epilogue paragraph per soul depending on which
branch you took. So there is no route to transcribe; there is a menu to choose
from. The ClubFloyd session of 2010-04-17 (now in `downloaded/` as
`AFineDayForReaping_clubfloyd.html`) *is* a winning oracle, but it is 713
commands of group flailing with `save`/`restore`/`undo` mixed in, so it is
useful only for confirming which branches work.

The chosen route reaps all five in **73 moves**: mask for Splong5b, time
machine to 10,097 BC for Ernest Busset, repaired shovel for Lord Nigel
McWorthington, chess guide for Jimiyu Wangai, cigarette lighter for Agathe
Laurent. Four of those are the author's "way 1"; the mask (his "way 2" for
Splong5b) beats the meshomatic because the meshomatic needs a *second* round
trip in the time machine, and the shoe that buys the mask is lying in the
Manchester cellar the route already visits.

**There is no score system** — `score` answers "0 out of a maximum of 0" at
every point — so the marker is the last distinctive single line of the ending,
`Life is good for Death.` (The following sentence, "Today has, all things
considered, been a fine day for reaping.", wraps across two lines and would
break `grep -F`.)

Four engine facts from `SCR_DUMP_TASKS`, all worth writing down:

* **Win** = TASK 6 via EVENT 2 when `soulsreaped` hits 5; **loss** = TASK 5 via
  EVENT 1 when `timea` hits 47, and EVENT 0 bumps `timea` every 15 turns. The
  "twelve hours" the game keeps nagging about are therefore a **~705-turn
  budget**. 73 moves spends two of them: `x hourglass` on the turn before the
  final reap still says *"ten hours left"*. The timer is real but it never
  constrains a route.
* **The horse only travels from hub rooms.** Every `say <place> to horse` task
  carries `WHERE_ROOMS=[5 6 7 13 17 25 34 39 40 41 42 46 51]`; anywhere else is
  *"No-one pays any attention to you."* That is why the route keeps walking
  back to the Area 51 Storage Cupboard before each departure.
* **Arrival auto-moves are `rep=0`.** TASK 70 walks you into Jimiyu's hut on
  the *first* Kenya arrival only; the second lands in the Village and needs an
  explicit `n`. This silently broke an early draft's `play chess`.
* **`take tape` is refused by design** (*"If you ever need it then you know
  where to find it"*). The masking tape is consumed implicitly by
  `repair shovel` (TASK 182), which itself requires `x workbench` (TASK 94)
  first — the tape never enters the inventory.

One genuine documentation bug found in passing: the author's *"search the
wreckage of the time machine in the cellar"* works with neither word.
`wreckage` is not a noun the game knows, and SEARCH on the machine answers *"I
don't understand what you want me to do with the time machine."* The alias is
`wreck` and the verb is EXAMINE — `x wreck` is the only way to learn the
10,097 BC year in-game. It is cosmetic here: TASK 195 (`cmd=[[10097/10,097]]`)
is gated only on the coil being fitted and the red button having selected BC,
so the year can be typed cold.

`SCR_SKIP_WAITKEY=1` is mandatory — the interactive title screen ("1 : Read Me
First / 2 : Credits and Thanks / 3 : Begin Reaping") sits behind a wait-for-key
and the run otherwise stalls at "Loading game...". The solution's first line is
the menu answer `3`.

See `A_Fine_Day_For_Reaping_walkthrough.md`. Full corpus after wiring:
**170 PASS, 0 FAIL, 14 NOSCRIPT.**

## 2026-08-04 — the three non-plain-text documents, wired — 169/169 PASS

The last three entries in the `downloaded/` queue were the ones that are not
plain text: two PDFs and a Word document. All three are now wired; the
"not plain text" bullet in the PARKED section below is struck through.

**Second Chance** (David Whyld, 2005) — ★ **WIN, the good ending, VERBATIM.**
`SecondChance_walkthrough.pdf` is not prose but a **full session log**: every
command is on its own `>`-prefixed line, so the 49-command script falls out of
the document mechanically and replays without one repair. The only thing that
needed knowing was a harness detail — the title sequence embeds two
`<waitkey>` pauses, which eat the first two commands and desync the whole run,
so the row carries `SCR_SKIP_WAITKEY=1`. The game keeps **no score**
(`score` → "No one's keeping score."); its endings are ranked by whether the
three vignettes went well (Dolores → `push button` to call the police; Jenny →
talk, never the "ask about sex" branch; Doug → three `talk to doug`; Antonia →
actually search her room). The marker is therefore the closing line of the
good ending, `congratulating me on a job well done.` See
`Second_Chance_walkthrough.md`.

**Private Eye** (David Whyld, 2006) — ★ **WIN, score 4, best ending,
VERBATIM.** A pure numbered-choice game: no parser verb appears anywhere in
the 73-choice route, so the solution file is a column of digits. The
walkthrough section of the shipped 116-page `PrivateEye_guide.pdf` replays
exactly. Two harness facts closed it: the PDF silently omits the title menu
(the script carries a leading `3` = "Play Private Eye"), and
`SCR_SKIP_WAITKEY=1` is mandatory or the run never leaves the menu. One
12-word sentence in the PDF — "No sooner have I put the phone down than Jim
ambles in." — has no counterpart in the transcript; it is the author bridging
two scenes in the write-up, **not** output we drop: the game's own wording is
longer ("…and plonks himself down in the other chair"), it lives on the
ex-girlfriend phone-call tasks rather than Layla's, and the short form appears
nowhere in the inflated `.taf`. Word-diffed, 19293/19371 words identical
(0.9975). See `Private_Eye_walkthrough.md`.

**The Plague – Redux** — ~~**UNFINISHABLE AS SHIPPED**~~ **[VERDICT REVERSED
2026-08-04 — the game is ★ WINNABLE via the where=1 pole-bypass tasks; see
the entry at the top of this file. The where=0 analysis below stands; the
dead-end conclusion does not.]** Original entry, kept for the probe record: The game's whole `[F] Fight / [E] Escape` system is seven
identical task blocks (`ZOMBIE 1`…`ZOMBIE 7`) in which **every** task sits at
Where/Type = 0 (`ROOMLIST_NO_ROOMS`) — 243 of the game's 696 tasks are parked
there. Nothing `ExecTask`s the `[f]`/`[e]` pair, so once "[F] Fight or [E]
Escape?" prints there is no input that answers it (`f` → "That didn't make any
sense!", `fight` → "That wasn't the answer.", `e` is eaten by the library as
*east*). The first mandatory fight is the Women's Toilet cubicle, whose coins
are the last 10p of the £1.20 the water vending machine wants — so the route
dead-ends at £1.10 and everything downstream (Kate, the office vent, the
camera batteries for the torch, Ray, the staff-area keys, the tunnels,
Candice, the ending) is unreachable.

Proved against the real Runner **twice**, which is what makes this a verdict
rather than a suspicion:

1. **Isolated probe** — new `test/make_400_whereprobe.py` builds a minimal 4.0
   game with `alpha` at where=0, `beta` at where=3, `gamma` at where=1 scoped
   to the other room, repacked with `taftool.py`. `run400.exe` fires beta,
   refuses gamma, and refuses alpha — identical to SCARE. So SCARE's
   `ROOMLIST_NO_ROOMS` handling is *correct*, and a where=0 task genuinely
   cannot be typed.
2. **Game-level probe** — a copy of this very game with `#StartRoom` patched
   `0` → `15` (Women's Toilets; line 80 of the unpacked plain body, right
   after the `bd d0` separator at line 79) loaded in `run400.exe` under Wine
   reaches the byte-identical cubicle scene and answers `f` with "That didn't
   make any sense!".

This is the **second** game in the corpus killed by this exact mistake — The
Hangover (2026-08-03, below) loses its two endgame tasks the same way, checked
against run390. Worth remembering as a diagnosis: when an ADRIFT 4 walkthrough
asks for a command the game flatly does not understand, dump the tasks and look
at `where=` before assuming a parser bug.

The row is blessed as a documented dead end — a maximal-reachable run that
replays the shipped `.doc` as far as the game allows, types `f`/`fight`/`kill
zombies` at the prompt to record the three refusals, collects all five
reachable coin caches, and ends at the vending machine. Marker: `I pressed the
vend button but nothing happened.` See `The_Plague_Redux_walkthrough.md`.

Two derivation footguns from this game, both costly: **the discovery verb is
`SEARCH`, not `EXAMINE`** — the `.doc` says "Examine the till" throughout, but
`x till` answers "nothing interesting" and only `search till` works (same for
rides, body, counter, windows, highchairs, bench, condom machine, Nick, bin,
footwear, desk) — and **`x coins` is the money counter**, `count coins` is not
a verb.

## 2026-08-04 — six more 3.80 games, and the only two surviving 3.70 games

The 2026-08-03 survey below concluded "exactly 11" ADRIFT 3.80 games exist
online. That was too low, for one reason worth writing down: **adrift.co serves
files its adventure database never lists**, so enumerating the database (634
entries) misses them, and the IF Archive never had them either.

The name list has to come from somewhere else — the Wayback copies of Campbell
Wild's own *Adventures* pages: `tardis.ed.ac.uk/~jcw/adventure/adventure.html`
and `~jcw/adrift/adventure.html` (Jul 2000 – May 2001) and
`jcwild.pwp.blueyonder.co.uk/adrift/adventure.html` (Jul 2001 – Mar 2002),
11 captures, **130 distinct filenames**. Probing each against
`www.adrift.co/files/games/<f>` found six more 3.80 games, now in `games/` with
MAP rows (all six load and run; suite **168 PASS / 14 NOSCRIPT**, exit 0):

| file | title | size |
|---|---|---|
| `duck.taf` | Duck McCloud — The Fight Begins (Dale Trigg & Ashley Canning) | 7.8 K |
| `first.taf` | The book of Fistandantalus | 12 K |
| `jb2000.taf` | James Bond — Happy Landings | 11 K |
| `microwaveman.taf` | Microwave Man! | 5.2 K |
| `mikes.taf` | The life of Mike | 19 K |
| `superliam.taf` | Super Liam 1. A hero is born | 31 K |

So **17 ADRIFT 3.80 games survive**, and `microwaveman.taf` (5.2 K) is now the
smallest 3.8 game — a better first walkthrough target than `secret.taf`.

**Two ADRIFT 3.70 games also survive**, both unlisted on adrift.co and on no
other host: `arlo.taf` (*Alice's Restaurant Anti-Massacree Adventure*, Laura
Lee, 18-03-2000, 70 K) and `castle.taf` (*Castle Quest*, Andrew Cornish,
10-06-2000, 14 K). Header `3c423fc96a87c2cf94453961 39fa` — note byte 10 is
`0x39`, **not** the `0x35` a naive "one less than 3.80" guess predicts, which
is why an earlier sweep with a guessed signature found nothing. The container is
identical (same XOR'd CRLF plaintext, same `taf38schema.xor`).

*(Same day, later: **both are now playable and in `games/` with MAP rows.**
`V370_PARSE_SCHEMA` in `sctafpar.cpp` covers the four layout differences — the
extra header integer is the **0-based winning-task index**, movements are pairs
over one flat destination list, tasks have no `BWinGame`, and a fixed block of
17 renameable built-in command words replaces the 3.80 synonyms table — and
every one of those, plus the pooled burden model and the object initial-position
list, was then **measured against the genuine `run370.exe`** rather than left as
inference. Two real bugs fell out of the probing, one of them the 3.80-wide
`Parent = -1` bug that had been hiding worn objects in `tra.taf`. See
`ADRIFT_370.md` and `../RUNNER_TESTS_TODO.md` §6.)*

*(Same day, later still: **both 3.70 games are now solved**, at full score —
`castle_quest_solution.txt` 17 moves / **50 of 50**, and
`alices_restaurant_solution.txt` 85 moves / **190 of 190**, every one of Arlo's
30 scoring tasks. Both PASS against blessed goldens; see
`Castle_Quest_walkthrough.md` and `Alices_Restaurant_walkthrough.md`. Castle
Quest is a pure trap-avoidance map with five scoring and five killing tasks and
no objects at all; Arlo's one hard puzzle is that `play guitar` starts the
horse's *follow-the-player* walk, so you lead the horse out of the stable and
then `stop playing` — reversing the start task cancels the walk and strands it
in the garden, freeing the cabinet.)*

*(Unrelated, same day: 38 of the `games/` entries were symlinks into
`~/Downloads/plover-adrift-2026-08/`, which briefly vanished and SKIPped 38 rows.
**Every symlink in `games/` has since been replaced by a real file** — all 72 of
them, not just the 38 — so the corpus no longer depends on anything under
`~/Downloads`. 191 files, 25 MB. Suite: **173 PASS / 0 FAIL / 0 SKIP /
14 NOSCRIPT**, exit 0 — every game in the MAP is present and passing.)*

Nothing **3.60 or older** survives anywhere: the IF Archive holds zero (its
Jan-2001 index listed the same seven 3.80 files it has today, and re-decoding
its 9 unclassified `.taf` headers shows all nine are 5.00), the adrift.co store
holds zero across 619 DB entries + 540 further Wayback-known filenames, and the
oldest game Campbell ever listed is `haunted.taf` (13-06-1999) — which survives
only as a 3.80 re-save. `jacjim.taf` (*Jacaranda Jim*) is gone from every
reachable host. Note that ADRIFT 3.23 (13 Jun 1999) was the first version to
encrypt TAFs at all, so anything older would be **plain ASCII** starting
`Version 3.xx` — no such file turned up either.

Method gotcha worth keeping: `Range: bytes=0-13` classifies a remote `.taf`
cheaply, but adrift.co sometimes answers a ranged GET with an **empty body**;
a short reply must be retried unranged or the file is silently misfiled as
unknown. Filenames with spaces must be percent-encoded or curl fails outright.

Aside, same sweep: `www.adrift.co/files/games/tagv2.zip` is Campbell's **TAG**
(1994–97 DOS Pascal ancestor of ADRIFT) — `gen2.exe`, `genrun2.exe`,
`space.tag`, `BAR.TAG`. Its `genrun2.exe` is MD5-identical to the one shipped
with Tony Ash's *The Edge of the Abyss*, i.e. this is the `tag.zip` that
"Abyss"'s ABOUT.TXT says vanished with Campbell's old URL. Not a taf; nothing
for Scarier to do with it, but it is the whole surviving TAG corpus (3 games).

## 2026-08-03 — The PK Girl: the biggest game in the corpus, and it has no score

`the_pk_girl.taf` — *The PK Girl* (Robert Goodwin, 2002, 4th release Aug 2006),
ADRIFT 4.00, **116 rooms / 2260 tasks / 332 events / 29 NPCs / 187 variables /
1.6 MB**. **WIN in 407 commands with Katryn 55 out of a possible 60**, ending on
*"Congratulations! You got Katryn's ending. Your Secret Letter is: E"*. Row
`thepkgirl_solution.txt|the_pk_girl.taf|Your Secret Letter is: E|SCR_SKIP_WAITKEY=1`,
write-up `ThePKGirl_walkthrough.md`. **166/166 PASS.**

The game warns you in its own banner that "scoring in this game works
differently", and it means it. **No task carries a `score=` value and there is
no `ACT type=4` anywhere in 2260 tasks.** Instead there are eight relationship
variables (VAR 158–165, one per girl, each "out of a possible 60"); a scoring
task sets `change_score` (VAR 168) and redirects to one of eight per-girl adder
tasks, 2141 josie … 2148 laurie. The ending is picked by TASKs 2211–2218 tested
in the fixed order Laurie, Cassie, Monika, Saffy, Aileen, Katryn, Bengte,
Josie — first girl with score ≥ 40 *and* `know_<girl>` set takes it, and
`name_of_girl` then latches so nothing later can fire. **So courting two girls
at once is actively harmful**: Laurie is tested first and would steal the
ending. The route holds her at 11 and everyone else at 0. Each ending prints
one letter of a password; the eight spell **ICECREAM**, which is also the .taf's
own author password (recovered separately from the "Wild" trailer, see
[[adrift4-taf-wild-trailer]] — a nice confluence).

`downloaded/ThePKGirl_walkthrough.txt` exists and is the route's spine, but it
promises only "a basic ending", courts nobody, and leaves every timed stretch as
bracketed prose (`wait (for 37 turns, while Monika makes dinner)`,
`[walk around the general vicinity until you find the umbrella peddler]`).
`downloaded/ThePKGirl_hints.htm` quotes a **45**-point ending threshold where
the tasks say **40**; trust the tasks. Nothing ships inside the game — `hint`
says hints are unavailable and the decompressed source has no "walkthrough" or
"spoiler" string.

Seven things had to be re-derived. Three are worth repeating here:

* **The Chapter 1 bar detour is worth 10 points in Chapter 7.** `talk to dustin`
  / `3` / `1` sets `know_dustin`, which is what puts Dustin in the R.O.S.A.
  complex (TASK 1817) → magnets → 1917 → 1982 → EVENTs 271/272/273 → 1998
  "# Dustin down" → EVENT 279 → **TASK 1999 "# Katryn clutches you", +10**, the
  single largest award in the game. But `know_dustin` *also* inserts an extra
  Dustin beat into the Chapter 4 cafe scene, which is a long run of numbered
  menu answers — so one turn has to be spent deliberately (`wait` / `2` /
  `talk to dustin`) or every later answer lands one turn early and the
  conversation silently collapses (Katryn finished on 3 instead of 30 in an
  early attempt, with nothing wrong-looking in the transcript).
* **The umbrella peddler walks a ~9-turn circuit and is therefore the route's
  clock.** `give money to peddler` becomes "Please be more clear, who do you
  want to give to?" whenever he has drifted, and *every* turn added or removed
  anywhere earlier changes his phase. This row broke twice while the earlier
  chapters were still being tuned.
* **The silo endgame window is exactly four turns wide.** `get band`, then
  `put band on octal` by the fourth turn — TASK 2039 "# Katryn has a solution"
  fires on the fifth and takes the +5 away. Putting the band on *early* is
  worse: it ends the scene and the `hug katryn` +2 can no longer be had (probed
  at 47 vs 55). `head butt octal` and `knee octal` both redirect to 2025
  "# Octal runs" and each requires it not done, so only one of the two +5s can
  ever land.

**A conversation-state idiom worth carrying to other Robert Goodwin games:**
answering option *n* sets `<npc>_talk_state = <npc>_talk_state * 3 + n`, so the
state variable encodes the entire path through the menu tree, and awards are
written as tests on exact (situation, state) pairs — e.g. TASK 1234 pays +3 only
at `katryn_talk_situation=8 & katryn_talk_state=9`, which is `2` then `3` at the
security-booth monitor. Easy to walk straight past.

Why 55 and not 60: TASK 1684 "# Katryn advances" (situation 7, +3) is the
*alternative* to the Chapter 5 kiss, which pays +5; and the situation-10 +3
needs `katryn_done_talking` (VAR 115) back at 0, which every conversation task
sets to 1 on the way out and nothing ever resets. **55/60 is the practical
ceiling**, and it buys exactly the ending 40 would have bought.

## 2026-08-03 — the complete TAF 3.80 corpus is in `games/` — 165/165 PASS, 8 NOSCRIPT

**There are exactly 11 ADRIFT 3.80 games online, and we now have all of them.**
Surveyed by reading the 14-byte header of every candidate on the only two hosts
that still carry ADRIFT games: all 240 files under
`ifarchive.org/if-archive/games/adrift/` (`/`, `competitions/`, `italian/`,
`old/`, `spanish/`, opening every zip and blorb) and every pre-2005 download in
the adrift.co adventure DB (634 entries; `POST /cgi/adrift.cgi` with
`page=adventures&offset=N&compid=-1&compilation=0&sort=1&asc=0&category=0&perpage=50`,
which also tags each row with its own version icon `/img/380.gif` — that tagging
agreed with the bytes on all 9 of its 3.80 rows). The Archive holds 7, adrift.co
9, overlapping in 5. For scale, the same sweep counted ~49 v3.90 and ~349 v4.00
taf instances on the Archive alone: **3.80 is ~1.5% of the ADRIFT corpus.**

Catalogues cannot answer this question. IFDB's oldest ADRIFT format is
`adrift39` (id 44) and IFWiki's oldest category is "ADRIFT 3.9 works", so every
3.8 game is filed as 3.9 in both. The only reliable test is the header: a 3.x/4.x
TAF opens with `"Version X.YZ\r\n"` XOR the fixed VB6 keystream, so the first 14
bytes are a constant per version (`V380_SIGNATURE` &c., `sctaffil.cpp:53`), and a
`Range: bytes=0-13` request classifies a remote file without downloading it.
`3c423fc96a87c2cf94453661 39fa` is 3.80; 5.00 files share the first 8 bytes and
decode to `"Version 5.00"`.

| file | title | source |
|---|---|---|
| `marooned.taf` | Marooned | IF Archive **(had, WIN 80/140)** |
| `wrecked.taf` | Wrecked | both **(had, WIN 250/250)** |
| `Crime_Adventure.taf` | Crime Adventure | IF Archive **(had, WIN 95/95)** |
| `akron.taf` | Akron | IF Archive |
| `cave.taf` | Cave of Wonders | both |
| `haunt.taf` | House of the Damned | both |
| `twilight.taf` | The Twilight | both |
| `haunted.taf` | The Haunted House (Campbell, Jun 1999 — the oldest) | adrift.co only |
| `great.taf` | The Great Escape | adrift.co only |
| `secret.taf` | Tom Ceader: Escape from the south | adrift.co only |
| `tra.taf` | The Timmy Reid Adventure | adrift.co only |

The eight new ones are in `games/` and have MAP rows in
`run_v4_walkthroughs.sh`, so they report **NOSCRIPT** until routes are derived
(suite unchanged at 165 PASS, exit 0). Their win markers are deliberately blank
— filling one in before the route exists would bless a marker nobody has seen
the game print. All eight were smoke-run in `scare` (`look` / `score` / `quit`):
every one loads, prints its title and intro, and takes commands.

**Why this matters beyond completeness:** the 3.80 burden and container model
settled against the genuine `run380.exe` (pooled burden, class costs
`0→1 1→3 2→7 3→3 4→7`, capacity `= #MaxCarried`; contents free, `Capacity` a plain
object count, dynamic containers must be *held*) was derived on three games and
can now be exercised on eleven.

**One lead fell straight out of the smoke run.** `tra.taf` prints
`gs_create: object held by nonexistent NPC, -2` three times at load — i.e.
`initialparent == -1` on the `InitialPosition == 1` ("held by") path in
`scgamest.cpp:977`, so those objects are silently **hidden** instead of starting
in someone's inventory. `-1` is 3.80's generic "no parent" filler (48 of
`tra.taf`'s 248 objects carry it), which is the tell: 4.00's convention is
`Parent 0 == held by the player`, and 3.80 looks like it uses `-1` there
instead. Worse, the same file has objects with `InitialPosition 1` and `Parent`
21 / 2 / 28 (`garage key`, `baseball glove`, `baseball`, `guitar`), which the
4.00 rule reads as NPC `parent-1` — if 3.80's index is 0-based those are all off
by one, and being off by one is *silent*. Nothing in `sctafpar.cpp`'s
`|V380_OBJECT:_InitialPositions_|` fixup touches `Parent`; it only handles
positions `2` and `> 2`. Settle it the way the size/weight model was settled —
a `make38probe.py` file with one object held by the player and one held by NPC 0,
run under `run380.exe` in the adrift-battle Wine prefix. No other 3.80 game in
the corpus trips the warning, so `tra.taf` is the test case.

## 2026-08-03 — The Sisters — **WIN, 109/109 = full score** — 165/165 PASS

*The Sisters* (James Webb, 04 Dec 2006, TAF **4.00**, 50 rooms, 123 tasks, 9
events) is **151 commands** ending on the epilogue's last line, *"Inside, with
hands and feet bound and eyes staring vacantly upwards, lay the lifeless body
of Trisha Seabourne."* Row:
`thesisters_solution.txt|TheSisters.taf|lifeless body of Trisha Seabourne.|SCR_SKIP_WAITKEY=1`.
Details in `TheSisters_walkthrough.md`.

Every one of the 109 points lives in an `ACT type=4` add-score action — all 123
tasks have `score=0` — and there are **exactly 38** such actions, summing to
109. The route fires all 38, which the golden records as 38 *"(Your score has
increased by N)"* lines, so the transcript is its own proof of completeness.

`downloaded/TheSisters_walkthrough.txt` is the rare thing in this corpus: an
honest, accurate 10-section prose guide that explicitly sets out to reach 100%,
and it does. Only five things had to come out of the engine:

* `get tin` is *"Take what?"* — the pickled herrings answer to **`can`**.
* `get key` in the music room is *"You need to be more specific about which key
  you mean"*, which is task 34, a catch-all `exam key` task with `get key` /
  `take key` / `drop key` among its 10 alt-commands, sitting in front of the
  three keys the game hands you. The object wanted is **`iron key`** (the prose
  calls it "a large metal key").
* On the lake **`row west` does not parse**. Movement is plain compass; `row
  east` is task 70, which exists only on the east lake square (39) to climb
  back out onto the jetty. `go fishing` only scores on square **43**, the
  middle-west square where the sisters are standing.
* The penknife **must be closed** before `climb down` at the steep decline.
  Tasks 12 and 13 have identical command lists and differ only in the
  penknife's open flag (`RESTR type=1 v1=3 v2=1/0`); task 13 is
  `ACT type=6 v1=2` — instant death.
* `SCR_SKIP_WAITKEY=1` is load-bearing: the collapse at the front door ends in
  a `[Press any key]` that otherwise eats the first command in the guest room
  and desyncs everything after it.

One authoring curiosity: `open music box` is written **twice** (tasks 74 and
75) with identical commands, differing only in the diary restriction — 74 for
the diary in some other state, 75 for the diary held. Unlike Crime Adventure's
shadowed pairs, both do the same thing and both are unscored, so it costs
nothing.

## 2026-08-03 — Crime Adventure — **WIN, 95/95 = full score** — 164/164 PASS

*Crime Adventure* (M. Whitmore, TAF **3.80**, 36 rooms, 23 tasks) is **90
commands** ending on *"Mrs Fenwick was in no danger at all, it was a friend who
picked her up at the booth (she was in a rush)."* Row:
`crime_adventure_solution.txt|Crime_Adventure.taf|Mrs Fenwick was in no danger at all, it was a friend`
(no env). Details in `Crime_Adventure_walkthrough.md`.

`downloaded/CrimeAdventure_walkthrough.sol` (29 lines, by "sasi") turns out to
describe an **earlier build**: it wants a computer in an "IBM" room for the stew
recipe (no such room — it is the cookery book in the kitchen), the penny dug out
of the ground with the shovel (it is in the spare-bedroom dresser), and the
underground door's lock picked with the hairpin (the door just opens). Its four
"extras" — paying the gypsy, being thrown out over the painting, being thrown
out of the arcade — are all gone too; only the street death survives. Shovel,
hairpin, hat, picture, painting, mirror, kettle and both NPCs' conversation are
unused, and the whole west half of the map is scenery.

**Two scored tasks are shadowed by unscored duplicates, so each has to be done
twice.** Task 14 `wear *shoes*` (0 pts) sorts before task 15 `wear *golf* shoes`
(10 pts), and `*` matches anything, so the scored copy never fires — until task
14 is spent, hence `wear` / `remove` / `wear`. Task 12 `give *food* to mr
fenwick` (10, alt `give *stew*…`) likewise shadows task 17 `give *stew* to mr
fenwick` (10), and task 12's own action drops the saucepan on the floor, so the
second give needs `get saucepan` first. **Play each command once and the game
ends at 75/95** with no indication of what the missing 20 were.

Also: the scoring `get cash` (task 19, +5) prints *"You grab the £30.00 from the
machine"* and has **no action that moves the object** — a second `get cash`
falls through to the library and really takes it. The cash is the held-object
restriction on both `buy shoes` and `wear golf shoes`, so missing this stalls
the route entirely.

**This row is sensitive to the in-flight 3.8 burden model.** The dump reports
`burdenmodel=1 maxburden=5`; the putter costs 3 and everything else 1, so
putter + ball + worn shoes is exactly the limit, and the route has to drop the
cash before carrying the saucepan and drop putter+ball before taking the chair.
Two `Your hands are full at the moment.` dead ends were hit deriving it. If
`V380_BURDEN_COST[]` or the limit changes, this golden moves with the
`marooned` / `wrecked` pair.

**A datapoint for the burden work.** MaxCarried is **5**, and the stew needs
exactly five things — carrots, onions, potatoes, meat, saucepan, all class 0
(cost 1). `get kettle` (class 2, cost 7) answers *"Your hands are full."* and
the cookery book would be the sixth. So under the normalised model the .sol's
own line *"Get all the stuff in Fenwick kitchen. Make stew."* is exactly
satisfiable — five items in, kettle and book left behind — which is the same
kind of author-walkthrough corroboration the `marooned` tires give.

**Suite: 164/164 PASS.** The `marooned` / `wrecked` pair, which had been
failing against the in-flight burden model earlier in the day, was re-blessed by
the burden work itself while this game was being derived; Crime Adventure joins
them as the third V380 row and the first one that reaches full score.

## 2026-08-03 — Humbug — **WIN, 2000/2000 = full score** — 163 rows

*Humbug* (Graham Cluley, 1990–1997; **converted to ADRIFT 4.00 by Campbell
Wild** from "Version 5.0 (r2)") is a large, proper old-school adventure — six
gemstones, a manor, a wumpus, an evil dentist. **1048 commands**, ending on
`You scored 2000 out of a possible 2000 and managed to complete 100% of this
adventure.  Grandad would probably describe you as a winner.. or a cheat.`
Row: `humbug_solution.txt|humbug.taf|Grandad would probably describe you as a
winner.. or a cheat.|SCR_SKIP_WAITKEY=1`. Details in `Humbug_walkthrough.md`.

**The headline is a negative result: the conversion is exact.**
`downloaded/Humbug_walkthrough.sol` is a 1152-line solution by **pjg** written
for the *original* v5.0 game, years before the port, describing a different
engine's parser — and **all 125 of its annotated `(N/total)` awards fire in the
same order with the same deltas and the same running totals, zero mismatches**.
Nothing about the port's task/score model had to be worked around. That is a
useful datapoint for the engine: this is by some way the largest score ledger
in the corpus, and it lines up award-for-award with a third party's independent
transcript of the game it was converted from.

What a text file cannot carry, and so had to be re-derived from the engine:

* **Turn counts.** pjg writes *"keep looking until Grandad shows up"*, *"wait
  about 35 moves or so"*, *"you may have to wait 25 moves or so"*. Each is a
  real, narrow window. The Golden Gulp bouncer admits you on exactly **two**
  turns (probe: `S` after 9 `Look`s refused, after 10 or 11 admitted, after 12
  refused); Horace's snuff tin surfaces on an exact **10-turn** cycle and the
  paper aeroplane must be thrown on the turn it is out.
* **`Get sheet` → `Get sheets`.** The bed linen only answers to its plural
  noun, so the .sol's singular silently takes nothing; `Tie Dennis with sheets`
  then fails five rooms later and Dennis wakes up and kills you three rooms
  after *that*. The kind of failure that looks like an engine bug and is not.
* **A seven-segment door.** The eight buttons in the neon tunnel (`4 3 5 2 7 1
  6 0`) are segments, not digits, with `7` as the commit key — and **the
  segments are not cleared when a digit is committed**, so each digit is the
  symmetric difference against what is already lit.
* **Four values that only exist inside the game** (the filofax number, Olaf's
  National Insurance number, the aunty's phone number off the computer, the
  magic word `Jisanajen` read through Grandad's monocle). The .sol leaves them
  as `<placeholders>`; they are worth **exactly 70 points**, so skipping them
  still *wins* — at 1930/2000.

**`SCR_SKIP_WAITKEY=1` is load-bearing.** The ASCII-art title screen ends in
`[Press any key]`, which swallows the first two commands, desyncs the route —
and the run still finishes, at 1930. The win marker therefore quotes the
full-score rank line, so it guards the score and not just the ending.

**Suite state: 161 PASS + 2 FAIL, and the two failures are not this work.**
`marooned_solution.txt` and `wrecked_solution.txt` (both **V380**) now miss
their win markers with *"Your hands are full at the moment"*, against a working
tree that carries an in-flight, uncommitted **3.8 pooled-burden carrying
model** (`obj_uses_burden_model()` / `V380_BURDEN_COST[]` across ~12 engine
files). Humbug is V400 and untouched by it. Those two rows were **deliberately
left un-blessed** — they belong to whoever finishes the burden work.

## 2026-08-03 — Three Monkeys, One Cage — **WIN, 98/100 = the ceiling** — 162/162 PASS

*Three Monkeys, One Cage* (Robert Goodwin, 2003, TAF **4.00**, 801 tasks) is
**112 commands** ending on `*** Congratulations, you did it!  (What took you so
long?) ***`. Details in `Three_Monkeys_One_Cage_walkthrough.md`; row:
`3monkeys_solution.txt|3monkeys.taf|Congratulations, you did it!` (no env).

**98/100 is the maximum, and the missing 2 are an authoring bug.** 22 tasks add
to `player_score` (variable 56) for a total of 97, and the anvil event adds 3 —
100 advertised. This route fires 21 of the 22 plus the anvils. The 22nd is task
603 `jump * out*`, whose action list is `exec 604` / `exec 608` / `moves--` /
`score += 2`; 604 (no mattress → death) and 608 (mattress → win) are mutually
exclusive on task 614 and **both end the game**, and
`task_run_task_actions()` returns at the first action that ends the game. So the
`+2` is unreachable in *either* branch, and no score line is ever printed after
the ending anyway. Logged as an open run400 probe in `RUNNER_TESTS_TODO.md` §4.

**This is the game that exposed the `$RestrMask` left-association bug** (fixed
the same day, `screstrs.cpp`): its author-written `winnable` oracle — task 21,
55 restrictions, the corpus maximum — said "no longer winnable" on turn 1 of a
pristine game. With the fix it says "still winnable" at every point on this
route, which is how the route was steered.

The author's own prose solution (`downloaded/ThreeMonkeysOneCage_solution.txt`,
190 lines) is a plan, not a command list. What had to be re-derived:

* **The cage is a 2×2 grid with two walking monkeys and a real-time fire**, so
  ordering dominates. The mandrill kills on contact and grants exactly **one**
  action once it shares your corner. Two fences: the fire permanently blocks SW,
  and smoke blocks whichever corner the fan is aimed at (`north`→NW,
  `east`→SE, `northeast`→NE, and smoke needs `fire >= 3`). The fan is re-aimed
  four times purely as a safety interlock. Three deaths during derivation.
* **Do not pick the sheet up early.** `make fire` burns whatever tinder you are
  carrying — the sheet is 3 fuel against the jersey's 5, and the sheet is needed
  intact later as hornet armour. (Fuel: jersey 5, blanket 7, sheet 3, peel 2,
  husk 2; −1 per fire cycle; over 13 in SW the bed catches, task 415.)
* **`cover myself with the sheet` hits the wrong task.** Tasks 637 (cover the
  *chimp*) and 638 (cover *yourself*) share that alt-command and 637 wins on
  index; 638's primary form `put the sheet over my head` is the one that works.
* **`se` is a wall bump** in a 2×2 grid — SW→SE is `e`.
* **Leaving SE silently unties the waist cord**, so `tie cord to me` must be the
  last action before `jump out`.
* **`chimp, …` orders need the chimp up a tree** (`chimp_elevated == 1`), and
  the long "gesticulating wildly" paragraph is only a preamble — the actual
  outcome is the last sentence of the reply.
* **The 38 `z` in the middle are the design.** Ceiling panels open on turn 100;
  the first anvil wave is +3 wherever you stand, but after that only `hide under
  bed` survives, and once the bombs replace the anvils there are nine turns left.

## 2026-08-03 — Largo Winch — **WIN, 97/97, maximum** — 161/161 PASS

*Largo Winch* (Jérôme Marchand, French, TAF **3.90**) is the longest route in
the corpus so far: **323 commands**, ending on `Congratulations!` with **97 out
of 97**. The maximum is proved from the file — `SCR_DUMP_TASKS` finds **96 `ACT
type=4` actions summing to exactly 97** (the first fight's finishing punch
awards 2), and the route fires all 96. Details in `Largo_Winch_walkthrough.md`;
row: `largo_winch_solution.txt|largo-winch.taf|Congratulations!` (no env — the
transcript is byte-identical with and without `SCR_SKIP_WAITKEY`).

The source is the author's own published list (`downloaded/LargoWinch_solution.txt`,
251 lines), which uses two shorthands the interpreter cannot take literally:
`commande (N)` means *repeat N times*, and `commande (prose)` is a stage
direction — and **every one of those is a fight**. `combattre` only *opens* a
fight; each blow is its own turn, and the list never says which blows.

* **Five fights, and each enemy answers to exactly one of `coup de poing` /
  `coup de pied`.** The wrong blow is usually not a miss but an `ACT type=6` —
  instant death (22 of them in the file). Fight 3's clubman is fatal to punch
  but needs only *one* kick, because that cues Simon to chain him.
* **Fight 2 cannot be won as described.** "Terrasser les deux ennemis" is
  impossible: whichever of Boris and André you fell, the other always flees.
  Three kicks at Boris is the only line that lands every blow and takes no
  damage in return; the score is identical either way.
* **`ouvrir la porte avec le badge` can never work — and it's a *game* bug.**
  It answers SCARE's English "Open what?" because the game defines its own
  input synonym **`ouvrir` → `open`**, applied *before* task matching
  (`SCR_TRACE_FLAGS=512`: `Printfilter: synonym "open la porte avec le badge"`).
  Tasks 213/214/216 carry only the `ouvrir …` alt-commands, with no `open …`
  twin — unlike window task 15 and the wardrobe task, which carry **both**
  spellings, which is why `ouvrir l'armoire` does work. `utiliser le badge` is
  the same task's primary command and is untouched by the synonym.
* Four route repairs besides the fights: `ouest`→`nord` out of the corridor,
  `est`→`nord` into Sharon's salon, three separate geography errors in five
  lines of the Omega basement (giving Olga the devis already descends the
  stairs; the way back up is a *different* room), and the electrical cabinet
  accepts only `la bague métallique **plate**` — hammering renames the ring.
* CP1252 again, and this one *is* piped in, so it matters: `prendre la clé`
  only parses when `é` arrives as a single 0xE9 byte.

## 2026-08-03 — Mortality — **good ending, verbatim** — and a crash — 160/160 PASS

*Mortality* (David Whyld, 2003–2005) is a **verbatim** replay: all 78 commands
from the session transcript inside the game's own doc file, no repairs, and the
responses are word-for-word identical (word-level diff 0.9952 — every
difference is an echoed command line, which the headless build does not echo).
Zero parser errors in 78 commands. Details in `Mortality_walkthrough.md`; row:
`mortality_solution.txt|mortality.taf|one of the two good endings|SCR_SKIP_WAITKEY=1`.

It is a menu-driven noir novella (55 of the 78 commands are dialogue digits),
with **no scoring at all** — `score` answers "No one is keeping score." and all
476 tasks contain **zero `ACT type=4`** — and, more unusually, **no `ACT
type=6` (EndGame) either**. Both good endings *and* the death ending are plain
text; the game prints its closing card and leaves the player standing at a
prompt, which is why the golden ends on the harness's appended `quit`. The two
"good endings" the closing card refers to are just the two options of the final
menu: re-running with the last `2` flipped to `1` gives the walking-out
epilogue and the identical card.

**The crash it flushed out (fixed).** `kill seamus` (task 310) redirects to
task 313 `[* after kill someone]` → task 314 `[? the return]`, whose second
action is `ACT type=0 v1=2 v2=0 v3=0`. `Var1 = 2` is **"the referenced
object"**, but 314 is only ever reached by redirection, so
`var_get_ref_object()` returns `-1` and SCARE aborted on the range assertion in
`gs_object_make_hidden()`. `task_move_object()` now ignores negative object
indexes, exactly as `evt_move_object()` already did — same family as the
known unset-combo rule (an ADRIFT selector the author left blank, or a
reference that was never bound, is a silent no-op in the Runner's `Select
Case`, never a fatal). The guard sits at the top of `task_move_object()` rather
than in one branch, because every destination (hidden / room / roomgroup /
held / worn / NPC's room) asserts the same way. No golden moved.

Two smaller things worth keeping:

* The doc file's transcript **prints its ending twice**, so the command list is
  truncated to the first 78 `>` lines.
* Choosing `4` (Walkthrough) at the title menu prints a stray `br>` on its own
  line. Faithful: the text is `…<info1>br><br>…` where `<info1>` is one of the
  game's own ALRs (it expands to the title menu) and the author typed
  `<info1>br>` for `<info1><br>`.
* The file is **CP1252**. It is only read, never piped in, so nothing breaks —
  but `grep` treats the resulting transcript as binary and silently reports no
  matches. Scan for parser errors with Python, not `grep`.

## 2026-08-03 — Wrecked — **WIN, 250/250, maximum** — 159/159 PASS

Campbell Wild's own 2000 game, TAF **3.80**, replayed from Campbell Wild's own
published walkthrough (`downloaded/Wrecked_walkthrough.txt`, 12/09/00). Every
`[+ N points]` in it is reproduced; the full 250 is reachable. Details and the
route are in `Wrecked_walkthrough.md`. Four things the published file leaves to
the reader had to be turned into commands:

* **The `[wait for train...]` notes are turns.** Under the fixed seed: 2 waits
  for the first train to pull in, 3 in the toilet until it moves off, 10 for
  the second train, 7 to reach Redstown, 5 to come back. The first ride is
  ticketless on purpose — Boris throws you off onto the wasteland, which is the
  only way to reach the scrapyard's gate button (+5).
* **Porkie wanders.** After `wave wand` (+10) the pig walks a random circuit,
  so the second wave outside the Post Office (+5) has to land on a turn when he
  is in the room. 3 `wait`s then **two** `wave wand`s: the first misses, and
  his arrival is announced on that same turn. Substituting a fourth `wait` for
  the wasted wave changes the RNG draw and he passes straight through.
* **Two blocker tasks whose FailMessage the author left as the placeholder
  `x`.** Task 96 swallows `in` at the pub once you take the scuba outfit off;
  task 84 swallows `up` (and `u`, `climb roof`, `go roof`) at the Post Office
  once you have climbed the statue — its restriction is task 83 **not** done,
  so climbing the statue is what breaks it. Both faithful: run390 on a gen390
  conversion prints `x` too (checked live for the pub), and gen390 re-encodes
  task 84's restriction byte-identically to our parse. Workaround is a parser
  fact, not a fix: **`go in` / `go up` are absent from both command lists**, so
  no task matches and the movement falls through to the room exit.
* **`turn it` binds to the wrong noun.** After `put key in ignition`, `it` is
  the ignition, and `turn ignition` hits task 37's blocker ("I can't turn the
  ignition"). **`turn key`** is what reaches the scoring task 55 (+5). Likewise
  `throw anchor overboard` only works after `push lever` has taken the boat out
  to the wreck.

## PARKED 2026-08-03 — the `downloaded/` wiring run stops here, at 161/161 PASS

Clean stopping point: suite green (no FAIL / SKIP / NEEDGOLD / NOSCRIPT /
REGRESSIONS), every golden blessed against the current binary, **nothing
committed**. The two `scparser.cpp` fixes, the `sctasks.cpp` negative-object
guard, the `scdump.cpp` additions below and the `sctafpar.cpp` 3.8 size/weight
fixup are in the working tree only.

To resume, the remaining `downloaded/` candidates that already have a staged
`.taf` are, roughly easiest first:

* transcripts (the format that has replayed near-verbatim seven times running)
  — ~~`LockedDoorWithWaterTrap_transcript.txt`~~ (wired),
  ~~`Marooned_walkthrough.txt`~~ (wired, 80/140 — see below),
  ~~`Wrecked_walkthrough.txt`~~ (wired, **250/250** — see below),
  ~~`Mortality_walkthrough.txt`~~ (wired, verbatim — see above)
* command lists / sectioned prose — ~~`ThreeMonkeysOneCage_solution.txt`~~ →
  `3monkeys.taf` (wired, **98/100 = ceiling** — see above),
  ~~`LargoWinch_solution.txt`~~ (wired, **97/97** — see
  above), ~~`Humbug_walkthrough.sol`~~ → `humbug.taf` (wired,
  **2000/2000 = full score** — see above),
  ~~`CrimeAdventure_walkthrough.sol`~~ → `Crime_Adventure.taf` (wired,
  **95/95 = full score** — see above)
* prose only, needs real derivation — ~~`TheSisters_walkthrough.txt`~~ →
  `TheSisters.taf` (wired, **109/109 = full score** — see above),
  ~~`ThePKGirl_walkthrough.txt`~~ → `the_pk_girl.taf` (wired, **Katryn 55/60,
  Katryn's ending** — see above)
* not plain text — ~~`SecondChance_walkthrough.pdf`~~ → `second chance.taf`
  (wired, **WIN, good ending, verbatim** — see below),
  ~~`PrivateEye_guide.pdf`~~ → `Private Eye.taf` (wired, **WIN, score 4 =
  best ending, verbatim** — see below),
  ~~`ThePlagueRedux_walkthrough.doc`~~ → `The Plague - Redux.taf` (wired,
  **UNFINISHABLE as shipped** — see below)

Still parked from earlier: Ba'Roo! (needs real derivation), Lair of the Vampire
(desyncs badly), The Fugitive (prose only). `TheDeadMan_walkthrough.html` has no
`.taf`. Five games are still undownloaded: Chosen, Crimson Detritus,
Imagidroids, Panic, The Cellar.

Two habits from this batch worth keeping: check `OBJNAME ... prefix=[...]`
before believing "I see no such thing", and treat a published *session
transcript* (as opposed to a hand-written command list) as Runner ground truth
— two of them caught real engine bugs today.

## 2026-08-03 — Marooned — **WIN, 80/140** — and the TAF 3.8 size/weight bug behind it — 158/158 PASS

`marooned.taf` is the first **TAF version 3.80** game in the corpus (`xxd -p -l
12 f.taf | cut -c17-22` → `944536`; `934536` = 4.0, `944537` = 3.9), and it did
not merely desync — it was *unfinishable*, because two objects the game hands
you could not be picked up:

```
>get tires
Your hands are full.
>get seal
The dead seal is too heavy for you to carry.
```

**The cause is a parser bug, not the game.** Version 3.8 stores a single
"Size/weight" **class index**, 0..4, per object. Every 3.8 grammar in
`sctafpar.cpp` read `#SizeWeight` raw and handed it to the 4.0 model, which
packs *size* in the tens digit and *weight* in the units and scales each as
`base^digit`. So class 4 ("very large") arrived as weight `3^4 = 81` against
Marooned's limit of `8 * 3^2 = 72`, and class 2 as `3^2`… wrong axis, wrong
magnitude, silently. New fixup `|V380_OBJECT:_SizeWeight_|` normalises every
3.8 object to 4.0 "normal" (22).

**Why normalise instead of converting the class.** ADRIFT Generator 3.90 does
convert it, and we know its exact table, because gen390 is available under Wine
and its conversion of `marooned.taf` diffs cleanly against our parse (new `sw=`
field on `SCR_DUMP_OBJLOC`, plus a new `PLAYERLIMITS` line):

```
3.8 class  ->  3.9/4.0 SizeWeight
0 normal   ->  22        3 large      -> 32
1 heavy    ->  23        4 very large -> 42
2 very heavy-> 24
```

That table is what ADRIFT itself does, and it is **wrong for playing the 3.8
file**: it breaks the games. On gen390's own conversion, the genuine
`run390.exe` refuses the tires with the same "Your hands are full." we used to
print. Crime Adventure (`Crime_Adventure.taf`, also 3.80, limit 45) goes the
same way — its kettle is class 2 → weight 81 — yet its published solution says
"Get all the stuff in Fenwick kitchen". Two independent 3.8 games, both
unwinnable for their own authors. The 3.8 header carries only **MaxCarried**, a
plain object count, and the two neighbouring 3.8 fixups already assume counts
(`Capacity*10+2`, `MaxCarried*10+2`) — those only mean "N objects" if every
object is normal-sized. Normalising makes the model self-consistent and
reproduces the author's own walkthrough line for line ("You pick up the couple
of tires."). Recorded as an ADRIFT-conversion bug, not ours.

Not provable to the last inch: **run380.exe cannot be obtained.** adrift.co
serves `/files/run380.zip` and `/files/gen380.zip` with HTTP 200, but both
archives contain the **3.90** binaries; IF Archive and its mirrors, the Wayback
Machine (ftp.gmd.de, ftp.tardis.ed.ac.uk, the egroups files areas) and
archive.org have no copy. run390 refuses 3.80 files outright ("You will need to
convert it with ADRIFT Generator 3.90"), so gen390 conversion is the only
ground-truth path available for 3.8 games — and it is the path the Runner
itself prescribes.

**The route: 80/140, and 80 is the ceiling.** The published walkthrough is for a
*different build* (its boat description differs from ours; our title bar says
"Marooned v1"), so this one is freshly derived from the task dump + the
`EXIT`/`ROOM` map. The remaining 60 points are structurally unreachable:

* **task 24 (get trash, 10)** — its two commands, `get *trash` / `take *trash`,
  are both already ALTCMDs of the repeatable task 14 in the only room the trash
  exists in, so 14 always wins the match.
* **task 27 (swallow pill, 10)** — requires the berries eaten, but the berries
  are the monkey's price for the flint, and the flint is mandatory to light the
  fire.
* **task 35 (shoot flare gun at shark, 10)** — requires holding object 20, the
  *unloaded* gun; loading it (task 19) destroys object 20 and creates object 37,
  and firing object 37 runs task 18, which kills you. Unreachable by
  construction.
* **tasks 9 + 28 (scratched can pour/light, 20)** — only the *dented* can's
  lighting task (10) starts event 5 "Rescue" (`startTask=11 affTask=30` → the
  `#win` task 29). Pour the scratched can too and task 9 wins the `light tires`
  match, the tires burn up, and the ship never comes. So the scratched can goes
  to the shark (task 33) instead — same 10 points, and the win survives.

One flourish worth keeping: at the lagoon, `throw <anything>` is stolen by the
non-repeatable, score-0 task 15 (`throw %object% *`). Burn it on the map you
picked up off the bridge, and the next throw reaches task 33 for its 10 points.

## 2026-08-03 (earlier) — The Amulet — **WIN**, verbatim, and the double-"Congratulations!" settled — 156/156 PASS

*The Amulet* (3-hour comp, Daniel Hiebert again) is a **verbatim** replay: all
12 commands from the author's transcript, no repairs, flavour lines (`notes`,
`spells`) included. The game has **no scoring whatsoever** — `score` answers
"0 out of a maximum of 0" and `SCR_DUMP_TASKS` finds zero `ACT type=4` — so
reaching the ending is the only measure of the route.

The one difference from the published transcript is that we print
**"Congratulations!" twice**, and it is worth writing down how that was
settled, because "our transcript has an extra line" normally means a bug.
`SCR_DUMP_TASKS` now prints a `WINTEXT [...]` line; all three of the games
wired today have an **empty** `WinText`, which is exactly the case where
`task_run_end_game_action()` falls back to its hard-coded "Congratulations!".
Shadrick's Travels also has an empty `WINTEXT`, its winning task text does not
contain the word, and its published transcript still shows one
"Congratulations!" — which is only possible if the real Runner prints the
default too. So in The Amulet the *first* one belongs to the winning task's own
CompleteText and the second is the engine's; the Runner would show both, and
the author trimmed the duplicate when writing the walkthrough up. Our
transcript is the faithful one.

## 2026-08-03 (earlier) — Monsters (Release 2) — **WIN, 40/40**, and two real parser bugs — 155/155 PASS

*Monsters (Release 2)* by Daniel Hiebert wins at **40/40**, which is the whole
game: `SCR_DUMP_TASKS` counts eight `ACT type=4` actions worth 5 each, and this
route fires all eight. The upstream file is a real-Runner session transcript
with no prompt glyph at all — commands and responses simply alternate — so the
38 commands were lifted by hand.

Exactly **one repair**: `open the bedroom door` → `open door`. The game has two
door objects, obj13 (the bedroom's own door, room 2) and obj48 (Mommy's, rooms
3 and 9); "bedroom door" collides and the parser answers "Open what?", leaving
`s` blocked on `RESTR type=1 v1=7 obj48`. The bare noun reaches the right one.

**The interesting part: this game's transcript is ground truth, and it caught
two genuine SCARE parser bugs.** Both are fixed, and neither moved any other
golden in the 154-row suite.

1. **`uip_match_optional()` did not rewind on a failed look-ahead.** It rewinds
   when the look-ahead *succeeds* and consumed text, but when the look-ahead
   fails it fell straight into `uip_match_alternatives()` at whatever position
   the failed attempt had already advanced to — and `uip_match_list()` has no
   backtracking of its own. Task 2's pattern is

   ```
   [defeat/shine/turn/put] {the} [flashlight/light] {on} {the} {brainsucker} {brain}  {monster}
   ```

   Against `shine flashlight on the brainsucker`, the look-ahead from
   `{brainsucker}` let `{brain}` eat the first five letters of "brainsucker"
   (`uip_match_word()` is a prefix compare with no word-boundary check), then
   died on the trailing "sucker"; the alternatives were then tried from
   "sucker", `{brainsucker}` matched nothing, and the pattern failed. Net
   effect: the command the author's transcript shows working was rejected and
   the game lost 5 points. `shine flashlight` (no object words) worked fine,
   which is what makes this kind of bug so easy to miss.

2. **`%object%` never answered to a partial prefix.** `uip_build_candidate()`
   composed exactly two strings, `"Prefix Short"` and the bare `Short`, so
   `examine the four poster bed` — Prefix `Sissy's four poster`, Short `bed` —
   was "I see no such thing", while the author's transcript prints the object's
   description. The candidate now also carries the prefix with its leading
   words dropped one at a time (`four poster bed`, `poster bed`, `bed`); the
   *name* is never cut down, so a two-word Short still has to be given whole.

   This one has **independent confirmation**: re-running the suite changed
   exactly one line in one other golden, Shadrick's Travels, where
   `climb oak tree` went from "You can't climb that." to "You can't climb the
   old oak tree." — and line 80 of *that* game's upstream transcript reads
   "You can't climb the old oak tree." Two unrelated real-Runner transcripts,
   same verdict.

`SCR_DUMP_TASKS`'s `OBJNAME` line now also prints `prefix=[...]` and every
`alias=[...]`, which is what turned "why doesn't this noun match?" from a
guess into a lookup. Use it first whenever a walkthrough line comes back "I see
no such thing".

## 2026-08-03 (earlier) — Shadrick's Travels — **WIN, 100/100**, verbatim — 154/154 PASS

A fifth verbatim replay, and the cheapest one yet: *Shadrick's Travels* by
Mystery replays **exactly as published**, 22 commands, no repairs, ending on
`Congratulations!` with the full 100 points.

The only trick is extraction. The upstream file is a session transcript whose
prompt glyph is a **CP1252 `Ø` (0xD8)**, not `>`; the commands are the lines
that start with that byte:

```python
d = open('downloaded/ShadricksTravels_walkthrough.txt','rb').read().decode('cp1252').replace('\r','')
cmds = [l[1:].strip() for l in d.split('\n') if l.startswith('Ø')]
```

`SCR_DUMP_TASKS` reports `TOTAL 100 4` — the game contains exactly four
`ACT type=4` scoring actions (`tie rope to wood` +20, `tie swing to tree` +20,
`throw rock at hive` +10, `swing on swing` +50) and this route fires all four,
so 100 is provably the maximum, not just a high score.

Three of the 22 commands are the author's own duds and are **kept on purpose**:
`x wood` and `climb tree` both land on the disambiguator, and `tire swing to
tree` is a typo for `tie`. ADRIFT's "Please be more clear…" does not consume
the following line, so a dud costs nothing but keeping it means the golden is a
faithful record of the published transcript. No env is needed — the game never
paginates, and the transcript is byte-identical with and without
`SCR_SKIP_WAITKEY=1`.

## 2026-08-03 (earlier) — the two French games — **both full marks**, wired — 153/153 PASS

*Qui a tué Dana ?* (**100/100**) and *Enquête à hauts risques* (**59/59**), both
by Volcy Bucherie with solutions by Hugo Labrande. Both now reach the maximum.

**Encoding first.** These solution files are stored in **CP1252, not UTF-8**.
The harness `cat`s the solution straight into the interpreter, so a UTF-8 file
delivers `é` as two bytes and the game's noun matcher never sees it: in *Dana*
that silently loses `prendre téléphone` and `x téléphone` (and with them the
whole phone-memory subplot), with no error message that looks like an encoding
problem — just "Prendre quoi?". Worth remembering for any other non-English
ADRIFT game we wire up.

**Qui a tué Dana ? — four repairs.**

| # | Repair | Why |
|---|---|---|
| 1 | third bare `parler` at the crime scene | three NPCs, three talk tasks; TASK 16 (chef scientifique) is restricted on TASK 18 `soulever drap`, so it can only fire *after* the sheet is lifted — the upstream order never gets to it, and `EXIT room=3 U` is gated on TASK 16 |
| 2 | `u` → `up` | the U exit and TASK 19 `cmd=[[up]]` are two different things; `u` uses the exit only, and `EXIT room=2 IN` is gated on TASK 19, so the police station stays shut |
| 3 | `w` `w` … `e` `e` around the phone presses | TASK 24/25/26 are `where=1 room=4` — *your* office — and the upstream list presses the memory keys in MALKOWITCH's office |
| 4 | `donner dossier` → `donner malkowitch dossier` | the winning task is `cmd=[[give] {malkowitch/…} {dossier}]` and wants both words |

Repair 3 is the interesting one: the upstream file hedges at exactly that point
("*parfois ça ne marche pas, prendre 2 puis appuyer 2 marche peut-être*"). It
isn't flaky — it's the wrong room.

**Enquête à hauts risques — four repairs.** 42 rooms, 165 tasks, and a task
style that is the opposite of most 4.0 games: whole literal sentences with huge
ALTCMD lists (TASK 23 `prendre l'arme de service` carries 17 alternates), so
the upstream abbreviations mostly *do* parse. Everything that went wrong was
movement or timing:

* an extra `n` on arrival at the commissariat (`e` from home parks you at
  Devant le commissariat, and the list has one `n` where two are needed);
* the stray `s` after `rez-de-chaussée` deleted (the lift already returns you
  to Le couloir, and the `s` dropped you to L'accueil, which has only N/S, so
  the gun-cupboard detour fell off the map);
* departure lounge: three `z` → four (the boarding call is on a timed event and
  lands on the fourth wait);
* on board: two `z` → four. `EVENT 6 [Décollage]` fires `TASK 71 [d747]`, and
  `TASK 72 regarder sous le siège` is restricted on it — look under the seat
  before take-off and the crate, the wire cutters and all three cables stay
  unreachable.

Verifying the ceiling here needed a trick worth reusing: `SCR_TRACE_TASKS=1`
gives `Task: running task N forwards`, so intersecting that set against every
task carrying an `ACT type=4` proves *no scoring task was skipped* — much
faster than reading a 145-command transcript for missing points.

## 2026-08-03 (later, 9) — The Demon Hunter — **WIN, 200/200**, wired — 151/151 PASS

*The Demon Hunter* (2003) reaches the full 200, but only after two repairs to
the delron command list — and the first one is a nice illustration of how
literal ADRIFT 4 task matching is.

**Repair 1: `south` → `s`.** The walkthrough leaves the Armory southwards with
`south`, which just moves you. The Chapel arrival is supposed to *also* fire a
task:

```
TASK 1 where=1 room=6 restr=0 rep=1 cmd=[s]
    ALTCMD[1]=[go s]  [2]=[go south]  [3]=[walk south]  [4]=[wlk s]  [5]=[walk s]
    ACT type=1 v1=0 v2=0 v3=7      (move player to room 7, The Chapel)
```

The author enumerated five alternate phrasings and never listed the bare word
`south`, so the walkthrough's `south` misses the task entirely. That matters
because task 1 is the *starter task* of

```
EVENT 0 [monk's death] starter=3 startTask=2 time1=0 time2=0 o2=3->10
```

— StarterType 3 (after task), zero length, and `o2=3->10` decodes (raw−1) to
"move global object 2, the monk's prayer book, to room 7". The book starts at
`pos=-1`, i.e. nowhere. With `south` the event sits in `ES_AWAITING` forever
(`SCR_TRACE_EVENTS=1` prints `ticking awaiting event 0` every turn and nothing
else), the book is never created, and `get book` answers "Take what?". With
`s` the task completes, `evt_starter_task_is_complete()` finally returns TRUE,
the zero-length event starts and `evt_finish_event()`s on the spot, and the
book is lying in the Chapel. Engine behaviour here is correct — the walkthrough
was simply written with a synonym the game does not accept.

**Repair 2: `read book` added.** Having picked the book up, the walkthrough
never reads it, and reading it is worth 15:

```
TASK 2 where=3 rep=1 cmd=[read {it/the/a} {monk's} {prayer} {book}]
    RESTR type=0 v1=5 v2=1 v3=0 obj2=[monk's prayer book]   (must be holding it)
    ACT type=4 v1=15
```

Without it the route tops out at 185/200.

**Eight kills, not six.** The walkthrough lists `kill hajar` six times. Six
*and* seven both leave the fight unresolved: the score stops at 127 and the
northeast exit never opens. The eighth attack is the one that scores 43 and
destroys him, after which the two `northeast` moves score 30 and end the game
on "Well done, my good and faithful servant."

Row needs `SCR_SKIP_WAITKEY=1` (the ending paginates). The win marker is
`"Well done, my good and faithful` — the closing line wraps, so `servant.`
lands on the next line and a longer marker would never match.

## 2026-08-03 (later, 8) — four verbatim delron replays, wired — 150/150 PASS

Four games whose delron command lists replay **without a single repair**, which
is worth recording because it is the exception, not the rule:

| Game | Cmds | Ending | Row env |
|---|---|---|---|
| Beanstalk the and Jack (David Welbourn, 2008) | 49 | `*** You have won ***` | none |
| Black Sheep's Gold (2004) | 99 | "You've beaten Black Sheep's Gold!" | `SCR_SKIP_WAITKEY=1` |
| Doomed Xycanthus (2006) | 82 | "Congratulations!" | none |
| Dancing Even Him? (Richard Otter, 2006) | 17 | anagram reveal | none |

Notes:

* *Beanstalk the and Jack* is a reverse-chronology retelling, so the command
  list reads backwards — it opens on `chop beanstalk` and ends with Jack waking
  up. That is the game, not a scrambled walkthrough.
* *Black Sheep's Gold* is the only one of the four that needs
  `SCR_SKIP_WAITKEY=1`: its epilogue stops on "(press any key to continue)" and
  without the skip the pause eats the trailing `quit`, so the ending never
  prints and the win marker looks absent.
* *Dancing Even Him?* — the title is an anagram of "Vending Machine", which the
  ending text spells out, so that line is the win marker.

The only work here was trimming the delron page footers ("Any donation would be
much appreciated", "Home | About Me") off the extracted text.

**Correction (2026-08-04):** that trim had in fact only been done for two of the
four. *Beanstalk the and Jack* (56 → 49) and *Doomed Xycanthus* (89 → 82) still
carried the seven footer lines as trailing commands. They were harmless — both
games end on an EndGame action (`sleep` and `jump` respectively) and SCARE stops
reading input at that point, so the junk was never consumed and the goldens are
byte-identical before and after — but the counts above were wrong and the files
now stop at the last real command. Both still PASS unchanged.

## 2026-08-03 (later, 7) — A Spot Of Bother — **WIN, 100/100**, wired — 146/146 PASS

*A Spot Of Bother* (David Whyld, 2005) reaches the author's own stated maximum,
and the upstream transcript needed exactly **one** repair in 270 commands: a
second `push door` in the gymnasium. The first push is a task that only reveals
the trap wire ("A wire, previously hidden, slips into view") — it does not open
the door, so after `break wire` the east exit still answers "You need to open
the door first." and `open door` answers "You can't open the door!". Pushing
again opens it and the remaining 197 commands replay verbatim.

The upstream file is a full session log including the title menu, so the first
two commands of the solution are the menu picks `2` (read the introduction) and
`1` (play). The row needs `SCR_SKIP_WAITKEY=1`: the game paginates heavily with
`[MORE]`, and without it every pause eats a command.

Everything else — including the `enter password elephant` crossword answer, the
three consecutive `3`s at the computer menu, and the long combination-lock
sequence on the safe — matched the upstream text response-for-response on the
first try, which is a good sign for the engine on a 2005-era 4.0 game with this
much task machinery.

## 2026-08-03 (later, 6) — Troll! — **WIN, 185/190**, wired — 145/145 PASS

*Troll!* (Peter Frøhlich-style pastiche, 2003 ADRIFT) is winnable, and the
route now reaches the ending with **zero** parser errors in 145 commands. Its
real ceiling is **185 of 190**, not 190: the game has 38 scoring tasks worth 5
points each, and one of them is structurally unreachable.

```
TASK 80 [* pay * barman *]    needs obj67 "fourtune", turns it into obj68
TASK 82 [* pay * landlord *]  needs obj67 "fourtune", turns it into obj68
TASK 81 [* pay * landlord *]  needs obj68 "fortune",  turns it into obj69
```

80 and 82 consume the same object, so at most one can fire — and 80 is
compulsory, because it is the task that summons the coach home (it moves
`obj21 coach` and `obj78 horse` and walks the player to room 3). 80 + 81 is
therefore the best pair available and **82's 5 points are dead**.

The published walkthrough scores 160/190. Everything else was recoverable:

| Recovered | Why the walkthrough missed it |
|---|---|
| `pay landlord` (TASK 81, +5) | it never pays the landlord at all — the `in` / `pay landlord` / `out` detour at Outside Inn on the way home is ours |
| `take breadcrumbs` (TASK 85, +5) | it says `get crumbs`, which the standard library resolves happily without firing the task |
| `w` upstairs **before** `unlock door` (TASK 86, +5) | TASK 86 is gated on TASK 64 `[* unlock * door *]` **not** being done, so the order matters: you score for walking into the locked door, *then* unlock it |
| `put breadcrumbs in basin` a **second** time (TASK 51, +5) | TASK 51 wants `obj48`, and `obj48` only reaches the player's hands as an *action* of TASK 52 (the firewater step). The first put is the library's |

Two of the walkthrough's commands are simply the wrong way round for the
task patterns, neither of which carries a reversed `ALTCMD`:

* TASK 61 is `[* show * barman * medallion *]` → `show barman medallion`
* TASK 83 is `[* give * chief * backward burp berries *]` → `give chief backward burp berries`

And two inventory hazards: the opening `get all` overflows the carry-weight
limit on a pink flyer, a green notice and a blue advert (dropped; five
explicit drops added instead), and the tavern drops must happen **on entry**,
not after `drink whiskey` — a timed event throws the player outside three
turns after the last drink, and the walkthrough's following `out` then fails.

The game ends inside `tickle frog` and never prints a final score, so the
golden's win marker is the closing line and the `score` just before it reads
180; the last task's 5 points land in the ending text.

## 2026-08-03 (later, 5) — The Hangover — **UNWINNABLE, max 5/7**, wired — 144/144 PASS

*The Hangover* (Red Conine, IFComp 2009 ADRIFT) is unwinnable as shipped, and
both dead ends are the author's. The two tasks the published walkthrough's
endgame turns on carry `Where/Type = 0` (`ROOMLIST_NO_ROOMS`), so they can
never run in **any** room:

```
TASK 10 where=0 room=-1 restr=1 cmd=[give the doctor some french fries]
TASK 14 where=0 room=-1 restr=3 cmd=[give approval notes to platypus]
```

Every other room-scoped task in the game is `where=1`. Confirmed against the
real run390.exe, which answers "You can't do that here!" to both. So the
ceiling is **5 of 7**: bill→fries (Deby), kick bum, mail→secretary, open the
filing cabinet, `type 119-228-337-446`. The two lost points are the doctor's
approval form and the ending itself.

Losing the doctor costs only the point — the Psycho Hospital's south exit is
*not* gated on his keys, so the route runs all the way through the jet, the
bathrobe parachute and back to the Form Process Office. The office even prints
"Except this time, the platypus is here. He seems happy. You give approval
notes to platypus." as room description while the task that would do it sits
permanently unreachable, which is a good demonstration that the author never
tested the ending.

Row (`the_hangover_solution.txt`, 53 commands, no `SCR_SKIP_WAITKEY` needed):
`the_hangover_solution.txt|hangover.taf|Your score is 5 out of a maximum of 7.`

Two derivation notes:

- **The room descriptions lie about exits.** Fedrick Avenue says "To your east
  is a bus stop"; the bus stop is **west**. The Approval Form Office says the
  first form is "through the door behind me to the north" and that one is
  right, but the walkthrough says south. The Psycho Hospital Room's exit is
  west, not east. Trust `You can't go in that direction, but you can move …`.
- The first command of the transcript is a dead `Take what?` — the walkthrough
  opens with "Get Up", which this build does not implement (`get` with no
  noun). It is left out of the solution file; the player starts standing.

### Follow-up: SCARE has no "You can't do that here!"

Both run390.exe and run400.exe carry the string ` can't do that here!` (VB6
UTF-16; grep the .exe decoded as `utf-16-le`, plain `strings` misses it), and
run390 prints it for these two tasks. SCARE has no such message anywhere:
`task_can_run_task` simply returns FALSE for a room-gated task, and the command
falls through to the standard library — here to "Give what?".

So a command that matches a task's pattern but is typed in the wrong room gets
a parser-ish fallback from us and a specific refusal from the Runner. Unproven
how narrow the Runner's condition is (task matched in *some* room? restrictions
already passed?), so this is **not** implemented — it needs a live run400 probe
on a corpus game before anything changes, because every golden that contains an
out-of-room task command would move. Filed here rather than fixed.

## 2026-08-03 (later, 4) — Yon Astounding Castle! of some sort — ★ **WON**, wired — 130/130 PASS

*Yon Astounding Castle! of some sort* (Tiberius Thingamus / Duncan Bowsman)
wired at 187 commands. Row:
`yonastoundingcastle_solution.txt|yonastoundingcastle.taf|Incredible victory!|SCR_SKIP_WAITKEY=1`
(the title crawl ends in a waitkey that would eat the first command).
Write-up: `YonAstoundingCastle_walkthrough.md`. Ends on `Incredible victory!`
/ `FINAL SCORE: YE OLDE INNKEEPER`, 5 treasures kept, and the transcript
contains no parser errors or failure messages anywhere.

**The shipped `YAC_walkthrough.doc` does not match the shipped `.taf`.** The
doc is dated 29 Sept 2009 (IFComp release); the game file is "Ye Second
Version", 31 Jan 2010. Both of the doc's routes stall. Differences confirmed
against `SCR_DUMP_TASKS`:

- the riddling gnome moved from the drawerbridge to the **Skull Gate** (room
  42, `TASK 509`–`TASK 534`) and got new riddles — v2 answers are `language`
  / `footsteps` / `yorick`; the v1 answers `mamy` and `envelope` have **zero**
  occurrences in the v2 task table. The gnome also now opens with
  *"Shall ye attempteth yon riddles? (Y/N)"*, so a bare `y` is needed first.
- `klarthaphmo` is spent transforming the **giant slug** into the shiny orb;
  the frozen speechery (obj 110) is referenced by no v2 task, so the doc's
  ice-platform `get speechery` step is dead.
- **Goblin Bob** now robs the treasure sack. Stolen goods land in the takery
  trunkle (obj 60, room 22) and can be picked back up.
- the shining orb needs the **magical oven mitt** worn, otherwise it
  "slippeth from ye grip".
- `unlock door` on the golden door no longer opens it — `open door` is a
  separate turn.

**Goblin Bob is a turn-timer hazard, not a puzzle.** `TASK 588 #gob_steals!`
is gated on `TASK 578` (`*sleep*goblin*`) not being complete, but the ambush
fires on the same turn he appears, so you never get the sleep command in
first. The committed route dodges him by parity instead: a single deliberate
`x hamish` delay turn on the way out of the Quakery. Without it he is standing
in the Makery on the next move and steals the just-reclaimed intercontinental
title back out of the sack, which then makes it un-nameable at the toll booth
and derails the three-treasure bribe (`TASK 984`,
`RESTR type=4 v1=4 v2=3 v3=3` = variable 4 ≥ 3). Fixed-seed harness, so this
is reproducible — but the route is turn-count sensitive.

Two treasures the shipped Speedy Route skips are collected: the **golden
cowbell** (`TASK 826` / `TASK 828 #IN_HOPPER`, grasses from the Lakery into
the hopper in Ye Cow Feedery) and the **chalice** in the undocumented secret
room reached by plain `in` from Ye Pointless Parlour. Together they lift the
ending from `YON SCROLL SORTER` (3 treasures) to `YE OLDE INNKEEPER` (5).

**Left on the table, deliberately:** the **antique stamp collection**
(`TASK 926`/`927`, room 40). The dungeon door's nominal key (obj 95) is named
`nonexistente`; the real opener (`TASK 916`) wants Fred held *and* shaped as a
tungsten key, and Fred can only be re-shaped while standing in Ye Makery,
which is across the lake. That is ~25 extra moves — all of which would re-arm
Goblin Bob's timer — for one treasure, in a room that also holds Thrug the
ogre. Matches the author's own warning that "ye walkthroughs shan't getteth ye
yon best possible ending".

No engine changes were needed; the game is a good v4 breadth stress case (62
rooms, boat travel between disconnected map halves, `Y/N` prompts, an
interactive named-object prompt loop, and an inventory-mutating wandering NPC).

## 2026-08-03 (later still) — It's Easter, Peeps! — ★ **WON**, wired — 129/129 PASS

*It's Easter, Peeps!* (Sara Brookside, One Room Game Competition 2006) wired
from the author's shipped `EasterWalk.txt`, which replays through scarier
command for command with no derivation work at all. Row:
`easter_solution.txt|easter.taf|***You have won***|` — no env vars, the game's
only `[Press any key to end]` comes after the win text. Write-up:
`Easter_walkthrough.md`. Scoreless one-room collection puzzle: fill Max's
basket with the eight items on his list.

**The walkthrough is a real `run400.exe` transcript, so it is an oracle, not
just a route.** The author kept every examine and every reply, which is why the
committed solution keeps all 71 commands rather than the 26-command minimal
win — diffing the golden against the shipped transcript audits scarier's
wording line by line. Three findings, one fixed:

**Fixed: the container-listing style selector, previously commented in
`lib_list_in_object()` as "frankly, a mystery".** run400 picks purely on the
*number* of contained objects — the helper at `0006A418` counts objects at
position 246 whose parent is this container into `var_98`, then `== 1` and
`== 2` take the postfixed form ("*A and B are inside C.*") and everything else
the prefixed one ("*Inside C is …*"). **No test on static-vs-dynamic anywhere
in that chain**, which is what scarier had been keying on. The transcript
exercises all of it: a static 1 (umbrella stand), a static 2 (pay phone), a
dynamic 2 (the wallet, in every `i`) and a dynamic 6 (the basket). 37 corpus
goldens re-blessed plus `test/capacity_nest_expected.txt`; two of the rewrites
are corroboration rather than churn, because in `yak_shaving` the *author's own
ALRs* only match the postfixed phrasing and had never fired before.

**Not fixed, documented:** `take <floor object>` answers "You take the creme
egg." in run400 where scarier says "You pick up the creme egg." (both agree on
the "from the newspaper rack" case), and `g` echoes the whole repeated command
`(hit pinata with umbrella)` where scarier prints only `(with umbrella)`. See
`RUNNER_TESTS_TODO.md`, 2026-08-03. The ambient shopkeeper events land on
different turns in the two transcripts; that is the seeded PRNG and is not a
divergence.

## 2026-08-03 (later) — Cursed — ★ **WON 93/101**, wired — 128/128 PASS

*Cursed* (Nick Rogers, IFComp 2011; post-comp build 2.1.10) derived from the
ClubFloyd transcript of 31 Jan / 7 & 14 Feb 2013. 298 commands, fox path,
finishes on **93/101** — the same score the ClubFloyd session reached. Row:
`cursed_solution.txt|cursed.taf|The honour will be all mine, father|SCR_SKIP_WAITKEY=1`.
Write-up: `Cursed_walkthrough.md`.

**One real engine fix fell out of it.** The second interlude gates the magical
veil on *"No object is held by the player"* while the PC wears un-removable
street clothes, so scarier — which counted worn as held — made the game
unwinnable. `run400.exe` distinguishes them: the **quantified** (Any/No object)
loop at `00080871` in `mdlSpreadTheLoad.Sub_20_3` tests only `location == 0`
(held), plus a container arm on `246` whose *parent* may be held (`0`) or worn
(`156`); there is no `location == 156` test on the object itself, where the
**single-object** path at `00080C9B` plainly has one.
`restr_object_in_place()` now takes a `quantified` flag and matches worn-by-
player on the single-object path only. Whole v4 suite re-run: **128/128 PASS**.

**Derivation footgun worth remembering: `Globals.WaitTurns` is per game, and in
Cursed it is 3.** A single `z` runs *three* turns of events, so any cut-scene
that has to be stepped beat-by-beat desyncs if you count `z`s as turns. Measure
it before counting:

```
printf 'z\nquit\ny\n'      | SCR_TRACE_EVENTS=1 ./scare GAME 2>&1 >/dev/null | grep -ac '^Event: ticking event 0:'
printf 'listen\nquit\ny\n' | SCR_TRACE_EVENTS=1 ./scare GAME 2>&1 >/dev/null | grep -ac '^Event: ticking event 0:'
```

(3 vs 1 here.) Use a one-turn no-op such as `listen` for the fine stepping. Two
scenes need it: the Vetan confrontation (`invisible` must land on turn 11 after
`push indentation`, `attack vetan` on 12, and `wink at vonisor` must have been
used at least once or Vonisor's thrust misses) and the Epilogue hug (six turns
after the last topic, exactly one turn of window).

**Known-divergent output kept in the golden:** unresolved ALR-style tokens
(`[playermove=Rithusar]`, `[ridingpaddlewheeldescription=0]`, `[listen-Dead]`,
`[listen-Complete]`). The 2013 SCARE 1.03.10 transcript prints the identical
tokens in the identical places, so they are pre-existing SCARE behaviour, not a
scarier regression — but this golden will need re-blessing when the
substitution model is fixed.

**The last 8 points are unattributed.** `full score` lists 24 items summing to
93 of 101. ClubFloyd played three evenings, tried the other animals, and also
finished on 93. Best guess is the King interlude ("4 points for your responses
to the king's questions" reads partial), unconfirmed by the transcript or the
shipped `cursed_hints.taf`.

## 2026-08-02 — WesGHN verdict OVERTURNED: **WON 100/100** (the gold ring was never orphaned)

The 2026-06-24 "UNWINNABLE, max 30/100" verdict below is wrong. Its structural
claim — "no task, event, or NPC character-walk ever un-hides the severed hand"
— misread the event table: the `SCR_DUMP_TASKS` **EVENT lines print raw
1-based `.taf` fields**. Event 1, named **[Davidshand]**, has `o2=4->5,
startTask=6`, which decodes (raw−1; `evt_move_object` dest semantics: −1
hidden / 0 held / 1 player's room / else room = dest−2) to *severed hand
(object 3) → Waiting Room (room 2), started by task 5 `ring bell`, one turn
long*. Read 0-based it looks like the Journal — hence the false "orphan".
In play: `ring bell` ("nothing appears to happen"), wait one turn, the
receptionist's window opens and the hand thuds down with the **gold ring** on
it. The old route rang the bell and left without looking back.

Full win banked: **100/100, all 12 scoring tasks**, `wes_ghn_solution.txt`
re-derived + re-blessed, harness row marker now `You've Won the Game!`
(was the 30/100 tour line). Highlights: Hope dies **twice** (#Hopedies with
task 15 undone, then task 15 respawns her with the Stripper Sword and
#Hopedies2 fires — Charity fights alongside); `slash dr micheals portrait`
must omit the period (`.` splits input); the scalpel drops in Radiology, the
La Virgencita key materialises back in the Medicine Cabinet; the Magna Mater
door wants the key + scalpel + scythe **unsummoned** and its vision eats ~8
keypresses; then Prudence (Waiting Room) → `give sigh prudence to hope` →
`take spirit` → `put hope's spirit into box` = win. `open altar` is a
stamina-90 boss trap and is skipped. Details in `WesGHN_walkthrough.md`.

**Lesson (now also in memory):** before declaring an object orphaned, decode
every event's o2/o3 with the raw−1 rule — especially when an event is named
after the object. Spirit's Flight was re-audited under the correct decoding
the same day: its event 5 moves objects 36 and 7 (Dagger), NOT the Ice Totem
(which would be raw 2), so its 50/95 unwinnable verdict **stands**.

## 2026-08-02 — To Hell & Beyond: the assisted ceiling is **265**, and 373 is not the maximum

Second assisted row wired: `to_hell_and_beyond_assisted_max_solution.txt`
(same env, same win marker) banks **265/373**, up from the 248 row. Accounting:
`373 − 80 − 25 − 3 = 265`.

- **−80, and this is the real headline: 373 is not the ceiling — 293 is.**
  Tasks 86 `go home` (+80) and 87 `claim the throne` (+150) *both* carry an
  `ACT type=6` (EndGame), so exactly one of them can ever be banked. The
  advertised maximum counts both.
- **+20 = task 72 `^^aquired armor^^`**, Theeve's death reward, which **nothing
  in the game executes**. THB is an upgraded 3.9 file and 3.9 has no
  execute-task action at all (the `|V390_TASK_ACTION:Type>4?#Type++|` fixup
  splices in 4.0's type-5 slot on upgrade), so every chain runs through events /
  NPC walks / battle `KilledTask` — and Theeve (NPC 28, a fully configured
  hostile, 50/7/4, attitude 2) is the one hostile left with `killedTask = -1`.
  The only way to fire it is to walk to room 128 and type the author's internal
  task name. **That makes the new row an EXPLOIT row, not an honest maximum** —
  the 248 row stays as the honest assisted result. run400 unprobed for whether
  it also accepts a bare task name.
- Knock-on: tasks 73/74 (`buy studded leather` / `buy studded pants`) are gated
  on `RESTR type=2 v1=73 v2=0`, so the **Mika armor shop is permanently
  locked** — a third independent breakage after the combat and the `Var2=-1`
  moves.
- **−3** is the `^^Days^^` timer, unavoidable: trimming the trailing waits
  18→4 still wins but does not dodge it, and 21 idle `z` at the same point cost
  *two* decreases.
- **−25 = task 83 `greet Trace`**, structurally unreachable: entering room 166
  *is* the trigger — an NPC walk's `charTask` fires task 89 `^^discussion^^`,
  which teleports the player out on the entry turn.

v4 suite now **127 rows**. Details in `To_Hell_And_Beyond_walkthrough.md`.

## 2026-08-02 — Azra re-checked, verdict CONFIRMED; the "unconfigured-combat" sweep is now complete

**V400** (`c2 cf 93 45 3e 61`), so no `battle_legacy`. **All 38 objects dump
`acc=0`** (best weapons are gold armor / heavy sword / Azranian Soldier's Sword
at hit 8, hunting sword 5 — every one of them +0 accuracy). Player is
Stamina 100 / Str 1-1 / Acc **0-0** / Def 0-0 / Agi **0-0**; only 3 of 12 NPCs
are configured at all (bandit 30/5/3 `KilledTask=19`, Ormulus 30/3/2 speed 3,
deer 20/0/0 `KilledTask=37`). `0 > 0` is false forever. **No score (0/0), no
win** — and there is **no `ACT type=6` anywhere in the file**, so no ending
exists to reach even with the assist. Verdict unchanged.

*Correction to the old note below:* it claimed the game has **no** type-7
ChangeBattle action. It has **22** — but their `Var1` is only 0 (Attitude),
1 (Stamina) or 2 (MaxStamina); the range indices 3/5/7/9 and the cap indices
4/6/8/0xA never appear. Right conclusion, wrong evidence. Also fixed the
walkthrough's `SC_ASSUME_COMBAT` typo (the variable is `SCR_ASSUME_COMBAT`).

Incidental: tasks 19/37 give the killed bandit/deer `+30`/`+20` stamina back —
the author designed them as a **renewable income farm**, which is how $7,500 for
the house was supposed to be payable.

**Sweep complete.** All four games once filed as "unconfigured-combat
casualties" are now settled: Mr Smith **WIN 90/100** and Villains & Kings
**31/37** (both V390, `battle_legacy`, both old verdicts wrong); Jason Vs. Salm
**WIN, honest 0/1000** and To Hell & Beyond **unwinnable ≈23/373** and Azra
**0/0 sandbox** (all V400, verdicts stand). Only To Hell & Beyond still
justifies a `SCR_ASSUME_COMBAT` corpus row.

## 2026-08-02 — To Hell & Beyond re-checked, verdict CONFIRMED (and its "upgrades" are no-ops)

Last of the four "unconfigured-combat" games to be re-audited. **V400**
(`c2 cf 93 45 3e 61`), so no `battle_legacy`; and unlike Jason Vs. Salm there is
no hidden weapon accuracy — **all 44 objects dump `acc=0`**, and **no type-7
action anywhere touches Accuracy or Agility**. Player is Stamina 20 / Str 5-5 /
Acc **0-0** / Def 3-3 / Agi **0-0**; all 41 NPCs likewise 0/0, every range
degenerate (the upgraded-3.9 fingerprint). Xozim = NPC 38, Stamina 120, Str 9,
Def 8, `KilledTask = 85` (`^^xozimisdead^^`), which is the only restriction on
tasks 86 `go home` (+80) and 87 `claim the throne` (+150) — the file's only two
`ACT type=6`. **UNWINNABLE, faithful max ≈ 23/373 stands**, and it remains the
one legitimate `SCR_ASSUME_COMBAT` row (with `SCR_ASSUME_MOVES`, 248/373).

**New finding worth remembering:** the twelve type-7 actions this game *does*
have are stamina heals plus `v1=4` **Max** Strength +3/+3 and `v1=8` **Max**
Defence +2/+5 — and those are **no-ops in combat**. `battle->max[]` is read only
by `battle_attribute_max()`, i.e. the *Max* column of `status`; every roll comes
from `lo[]`/`hi[]`. The author wanted indices 3 and 7 (the ranges) and used 4
and 8 (the caps). Same shape of slip as Jason Vs. Salm buffing the wrong
character. Cannot change the verdict — no hit lands at any strength.

## 2026-08-02 — Jason Vs. Salm combat re-derived (verdict unchanged, reasoning was wrong)

Checked after the V390 sweep. **`Jason Vs. Salm.taf` is V400**
(`c2 cf 93 45 3e 61`), so `battle_legacy` does *not* apply and the 4.0
`accuracy > agility` gate is the correct model — the row and its **WIN, honest
max 0/1000** verdict stand unchanged, and the suite stays green. But the
walkthrough's mechanics were wrong in three ways, now fixed:

- Attributes are **`[lo,hi)` ranges re-rolled every turn** (`battle_roll`,
  high bound *exclusive*), not the fixed numbers the old text quoted. The
  player is 0–50 across the board, so a roll is 0–49.
- The player is carrying a **long blade: HitValue 10, Accuracy +5, Method 4**,
  handed over with the rest of NPC 0's kit when you pick "[1] Jason".
  `status` hides it — it says "wielding nothing" and shows `(0)` bonuses —
  which is exactly why the old analysis concluded no hit could ever land.
  **`SCR_DUMP_OBJLOC` now prints `acc=` beside `hit=`** so this can't recur.
- "Zero hits connect on all three raised difficulties" is false for **hard**.
  The difficulty tasks (10/11/12, `where=ONE_ROOM room=1`, so they must be
  typed *at the character-select screen*) add +20/+40/+60 to Salm's degenerate
  32–32 agility ⇒ **52 / 72 / 92** against a maximum effective accuracy of
  49+5 = **54**. Hard therefore lands on rolls 48–49 = **4 %** (measured 10
  hits in 298 swings over 60 seeds); extra hard and fucking crazy are
  **arithmetically impossible** (0 hits in the same sweep).

Hard is still not winnable in practice — Salm's defence rises to 36 so only
~46 % of those 4 % hits damage him (~275 attack turns per kill), while his
accuracy 20–54 and strength 50 kill the player's unbuffed 25 stamina in one or
two blows; 0 wins in 30 seeds. **The authoring bug:** every type-7 action in
the difficulty tasks targets NPC 0 ("Jason Evans") or NPC 1 (Salm), never
`Var2 = 0` (the player) — the author took NPC 0 for the player character, so
on a Jason run half the buffs go to someone who isn't in the fight.

Also corrected: the player is **not** invulnerable on Normal (Salm's revolver
is Method 3, so under 4.0 it *replaces* his strength: `25 − roll(0..49)` vs 25
stamina and no healing), and the win is **seed-dependent** — 60 Normal seeds
give 42 wins / 4 deaths / 14 Salm-wanders-off, hence the row's `SCR_SEED=11`.

## 2026-08-02 (after the One-Hour batch) — suite repaired for the load-time immediate-event start — 127/127 PASS

An uncommitted engine change turned the suite red: `StarterType=1` ("immediate")
events now start during **game load**, before the opening room description
(`evt_start_load_events()` / `evt_finish_load_events()` either side of the
description in `run_main_loop()`, with a `+1` compensation so the finish turn is
unchanged). Validated against the Runner probes — so the goldens and routes are
what move, not the engine. Visible effects: an immediate event's LookText joins
the opening description, its StartText is never seen (it prints into the screen
the intro clears), and a zero-length one finishes *below* the description.

The change also re-threads every downstream RNG draw, which is what actually
broke things: battle outcomes, weather, NPC wander phase and ambience-variant
selection all shift. **14 rows** were affected.

- **9 pure ambience/RNG-variant shifts** — marker and score intact, so just
  re-blessed: `alexis`, `alexis_worn_cube`, `circus`, `colony`,
  `melbourne_beach`, `del_sol`, `the_town_of_azra`, `villains_and_kings`,
  `villains_and_kings_assisted`, `ticktick`.
- **2 re-seeded** — `maincourse` → `SCR_SEED=17`, `light_up` → `SCR_SEED=45`.
  (Padding `maincourse` with extra attack turns instead of re-seeding *backfires*
  at every seed: the surplus swings leave the cat alive and Frank rejects the
  final `main course`.)
- **3 re-derived** — all three Shadowpeak routes broke at the Damastus maze
  chase, and no seed in 1–2000 wins the old base-route chase shape. New seeds
  102 / 230 / 201, chases regenerated: **700** (up from 680) / **715** / **735**,
  0 deaths each. See `Shadowpeak_walkthrough.md` § Session 24.

Carry-forward:

- **`run_v4_walkthroughs.sh` auto-rebuilds `scare`** when any `terps/scarier/sc*.cpp`
  or `*.h` is newer than the binary. That is what silently changed the engine
  under already-blessed goldens — a red suite after an unrelated edit is
  usually this, not a broken route.
- **`harness/shadowpeak_chase.py <solution-basename> <seed>`** is now
  parameterised and self-contained (it regenerates the EXIT graph itself; no
  scratch state). It had rotted to hardcoded dead paths. The repair recipe for
  the next re-thread is fixed: screen seeds on *upstream* cleanliness (first new
  command failure at or after the chase; add a prompt-count floor so dead-early
  runs don't screen as clean), then re-derive the chase under the winner.
- **zsh footgun that cost real time:** `r=$(… | grep -c "pat")` can yield an
  *empty* string, so `[ "$r" != "0" ]` is true for every iteration — a seed
  sweep reported 40 consecutive false wins. Write the run to a file and use
  `grep -q`. And goldens are extended-ASCII with NEL terminators: always
  `export LC_ALL=C` and `grep -a`.

## 2026-08-03 — One-Hour Game Comps 1/2/3: all 21 derived and wired — 127/127 PASS

The complete entry lists of the **1st (2002), 2nd (2003) and 3rd (2003) ADRIFT
One-Hour Game Competitions**, unpacked into
`~/Downloads/onehour-adrift-2026-08/{1hourgamecomp,ohc2,ohc3}/` and symlinked
into `games/`. Every one now has a solution file, a golden, a
`<Name>_walkthrough.md` and a row in `run_v4_walkthroughs.sh`. Suite is **127
rows, all PASS**.

Sixteen wins (six of them a verified score maximum), one best-reachable CYOA
rank, one deliberate death ending, and three "wins" whose text is anything but.

| game | title | result | cmds | note |
| --- | --- | --- | --- | --- |
| `frog` | The Green Princess | WON | 10 | no score; the first `e` doesn't move you (see below) |
| `chicken` | The Evil Chicken of Doom! | WON | 22 | no score; the "win" is life imprisonment for poultrycide |
| `endgame` | The Game To End All Games | WON | 13 | no score; "wins" three times before the real ending |
| `hauntedhouse` | The Haunted House of Hideous Horror | WON | 42 | no score; 4 of the 5 endings are deaths |
| `microbe_willie` | Microbe Willie vs. The Rat | ★ 7/7 **MAX** | 18 | timed kidney trap |
| `amonkeytoomany` | A monkey too many | ★ 25/25 **MAX** | 12 | 5-command win exists; route takes the long way for the text |
| `DFU` | Dance Fever USA | ★ 999999999 **MAX** | 21 | one `ChangeScore(+999999999)`, and that really is the maximum |
| `Percy` | The Saga of Percy the Viking | best rank ("prince") | 7 | CYOA; **no `EndGame` at all**; "king" is arithmetically unreachable |
| `forum` | Forum | WON | 20 | no score; two timed fights |
| `CBN` | The Revenge Of Clueless Bob Newbie! | ★ 45/45 **MAX** | 35 | the game's own "best possible ending" |
| `cbn2` | …Part 2: This Time It's Personal | ★ 30/30 **MAX** | 19 | |
| `CRM` | That Crazy Radioactive Monkey! | ★ 25/25 **MAX** | 22 | |
| `ECOD2` | …Evil Chicken of Doom…Returns! | WON | 24 | no score; the in-game book *is* the walkthrough |
| `Imagination` | Just My Imagination | ★ 100/100 **MAX** | 12 | name prompt eats line 1 |
| `asdfa` | A.S.D.F.A. | ★ 35/35 **MAX** | 24 | one wrong `x shelf` makes it silently unwinnable |
| `demonhunter` | Apprentice of the Demonhunter | ★ 6/6 **MAX** | 15 | 7-turn kill clock |
| `forum2` | Forum 2 | WON | 22 | no score; its own `walkthru` command is wrong |
| `pyramid` | The Pyramid of Hamaratum | ★ 100/100 **MAX** | 10 | one-turn beheading fuse |
| `saffire` | Saffire | WON (Heaven) | 16 | four equally-`win` endings; Wingdings bug |
| `shore` | The Farthest Shore | WON | 24 | no score; the "win" text is a drowning |
| `ticktick` | Doom Cat!! | **death (only ending)** | 12 | no `EndGame(win)` in the file at all |

Things worth carrying forward:

- **A `<waitkey>` can sit in the MIDDLE of a message, not just at the end**, and
  each one silently eats one line of input. `CBN` lost 10 of its 45 points to
  this: `x desk` prints *"…finds…`<waitkey>` nothing!"*, so the second `x desk`
  vanished and the pen was never found — with no error message anywhere. `CRM`,
  `cbn2` and `forum2` all have the same shape (`forum2`'s two are inside the
  **winning** message, so the harness's own `quit`/`y` get eaten unless two
  trailing blanks are banked). **New standard pre-check:** decode the `.taf` and
  grep the extracted strings for `waitkey` *before* probing, then budget one
  blank line per pause.
- **`SCR_DUMP_TASKS` prints nothing until a turn actually runs.** Games with a
  keypress intro produce an empty dump if you feed `/dev/null`; feed a file of
  blank lines instead.
- **Task-order shadowing steals commands.** `cbn2`'s task 4 is `[*lisa*]` and
  sits ahead of task 10 (`give *coffee*`), so `give coffee to lisa` only ever
  gets *"Back off, buster!"*. Drop the name.
- **Noun-token traps.** `forum`'s pattern is `[ds/monster/man/figure]{490}` —
  `smack ds490` is one token and never matches, so it falls through to a library
  refusal that reads like a puzzle hint. And an optional group written **without
  spaces** can be unmatchable outright: `forum2`'s
  `[third{review}scroll/scroll three/…]` can only ever match `thirdscroll`, which
  is why the game's own built-in `walkthru` fails on its first move.
- **A task can be a trap that leaves no trace.** `asdfa`'s Pantry invites you to
  `x shelf`; doing so sets a flag that permanently swaps the sword-granting task
  for a silent no-op, and the game becomes unwinnable with no diagnostic. The
  author left `cheat` responses keyed to that same flag, which is how it was
  confirmed.
- **Movement can be a lie.** `frog`'s first `e` matches a task with *no action
  at all* while an alternate room description renames the room — the transcript
  is indistinguishable from a successful move. `SCR_TRACE_FLAGS=16` prints
  `Library: moving player from X to Y` for genuine moves; `SCR_TRACE_PLAYER`
  prints the room at the *end* of the turn, which reads as an off-by-one.
- **Deliberately unwinnable / non-victory endings are common in this batch.**
  `ticktick` has no `EndGame(win)` at all; `Percy` has no `EndGame` of any kind;
  `shore` and `chicken` are engine-wins whose text is a drowning and a life
  sentence. Regression rows for these use the game's own final line as the
  marker, not a victory string.
- **Wingdings joins Webdings as an unmapped symbol font.** `saffire`'s Heaven
  ending is `<font face="Wingdings">V</font>`; `0x56` is the glyph `crossshadow`
  (U+271E, a shadowed Latin cross) in the shipped `wingding.ttf`, but
  `os_glk.cpp` only carries a Webdings table, so a bare `V` prints. Same shape of
  fix as the Topaz dove.

## 2026-08-02 (last) — Key & Compass batch: all 17 derived and wired — 106/106 PASS

Every game from the sweep below now has a solution file, a golden, a
`<Name>_walkthrough.md` and a row in `run_v4_walkthroughs.sh`. **Fifteen wins,
one deliberate death ending, one tour.** Suite is 106 rows, all PASS.

| game | result | cmds | note |
| --- | --- | --- | --- |
| `wax_worx` | WON (single ending) | 41 | no scoring system at all; the ending *is* `[PRESS ANY KEY TO DIE]` |
| `sommeril` | ★ 85/100 **MAX** | 74 | last 10 behind a restriction that can never hold |
| `DragonShrineR43` | ★ 95/100 **MAX** | 116 | last 5 unbankable on any winning path |
| `shardsofmemory` | WON | 108 | no score; 803 tasks, not one `ACT type=4` |
| `TheADRIFTProject` | ★ 90/100 **MAX** | 83 | author's own run also ends 10 short |
| `ShadricksUnderground` | ★ 100/100 | 95 | all seventeen awards |
| `ticket` | ★ 110/110 | 323 | all twenty-two awards; matches the author's total |
| `cleft` | ★ 100/100 | 90 | 5+10+10+5+3+12+15+40 |
| `Tear` | ★ 6/6 | 34 | six one-point awards |
| `tq3` | ★ 60 (max) | 51 | Chris Moody's *The Quest* |
| `yeh` | **TOUR** 3100/3400 | 18 | **no EndGame action anywhere** — cannot be won |
| `ADRIFTMAS_Party` | ★ 100/100 | 63 | |
| `Glum_Fiddle` | ★ 100/100 | 70 | ten ten-point tasks |
| `JGrim` | ★ WON | 101 | no score; marker is the last word, `WHOOOOOSH` |
| `mysteryofcaves` | ★ 125/125 "Godlike Adventurer" | 113 | game confirms it is the maximum |
| `chooseyourown` | THE END (deepest ending) | 52 | gamebook; **no EndGame action** either — marker is ending prose |
| `fantasyworld` | WON, 8535 exp | 322 | `Congratulations!`; adult game, run with its own `NOSEX` switch |

Things worth carrying forward:

- **Two of the seventeen have no `ACT type=6` at all** (`yeh`, `chooseyourown`).
  For those the regression row's win marker has to be a line of prose, and
  `--bless` still refuses without one — pick a line that occurs exactly once.
- **`RESTR type=2` var2 polarity, corrected:** `screstrs.cpp`
  `restr_pass_task_task_state()` reads **var2 = 0 → the task must be DONE**,
  **var2 = 1 → must NOT be done**. An earlier note here had it backwards and
  cost an afternoon on `fantasyworld`'s endgame.
- **Dump index offsets, re-confirmed:** `RESTR type=3` character index and
  `RESTR type=4` variable index are both `v1 - 2`; `ACT type=3`'s variable index
  is plain `v1`, i.e. two less than the restriction's. `ACT type=1 v1=0` moves
  the **player** and its destination is `v3` *verbatim*; `v1>=2` moves NPC
  `v1-2` to room `v3-1` (`v3=0` = hidden). See `sctasks.cpp:559-690`.
- **`RESTR type=4` comparison codes** (`screstrs.cpp:657-665`): 1 `<=`,
  2 `==`, 3 `>=`, 4 `>`, 5 `!=`.
- **A `#` comment line is skipped even at a mid-game prompt.** `fantasyworld`
  opens with a bare `Please enter your name:` before turn 1; the header comment
  block passes straight through it and the name is answered on the first
  non-comment line. Blank lines are *not* skipped — they are waitkey padding or
  wasted turns, so a game with zero waitkeys (like `fantasyworld`) must have a
  solution file with zero blank lines or the RNG shifts.
- **`search` does not imply `get`.** `ACT type=0 v2=6` puts the object in the
  player's *room*, not their hands (`fantasyworld`'s gold nugget); `v2=4` is the
  one that means "to the player".
- **Author walkthroughs are worth reading and worth distrusting.** Four of the
  seven that shipped with these games have at least one step that cannot work as
  written; `fantasyworld`'s reverses a hard ordering constraint outright
  (*"don't suggest anybody yet"* — the SUGGEST is what unlocks every subsequent
  `ask … about demon`).

## 2026-08-02 (later 3) — Sophie's Adventure, comp build — ★ WON 183 — 96/96 PASS

The IF Archive IFComp 2003 release `sophie.taf` is now wired alongside `sa.taf`,
once the parser bug below was fixed. Row:
`sophie_comp_solution.txt|sophie.taf|You have won.|SCR_SKIP_WAITKEY=1`
(255 commands, 183 points, ending 3 of 5). Both builds now pass.

The archived `walkthru.txt` was written against **this** build, so it fits it
better than it fits `sa.taf` — but it still needs seven line edits plus a
rewritten endgame. Details in `Sophies_Adventure_walkthrough.md`; the parts
worth carrying forward:

- **The endgame is a different quest from `sa.taf`'s.** No orb on the study
  desk. You raise **Kridlor's ghost** with `cast fire blast` on the bowl of
  ashes in the Small Study, and he asks for bell + comb + compass — all three of
  which you already have (bell from the Chamber of Battle armour, task 4469;
  compass from killing Benthem, task 4438).
- **The hand-in is a movement, not a `give`.** Every `give <item> to kridlor` is
  refused. Task 4470 is `w` in the **Crumbling Passage** (room 118) holding all
  three; it strips them, grants `obj800=[orb]` and moves you into the Small
  Study — so you step `e` out and `w` back in. Kridlor's menu option `1` only
  narrates the quest; the hand-in fires without it.
- **`w` in the Shadowy Hall is a decoy.** It advertises a "dark alcove" and just
  leads to the Wine Cellar. Fortress room numbers (`SCR_TRACE_PLAYER=1`):
  Shadowy Hall 92, Art Room 94, Portraits Room 95, Empty Hall 96, Crumbling
  Passage 118, Small Study 119. `SCR_TRACE_PLAYER=1` is the fastest way to pin a
  dumped task's `room=` to a real room.
- **`sleep` is the only recharge**, and the Corridor northwest of the Gallery is
  the only place with a spare turn to use it — once the Chamber of Battle's
  grills drop you cannot leave and cannot cast without energy.

## 2026-08-02 (later 2) — Sophie's Adventure — ★ WON 193, wired — 88/88 PASS

David Whyld, IFComp 2003, ScummVM gameid `if03_sophie`. See
`Sophies_Adventure_walkthrough.md`. Row:
`sophie_solution.txt|sa.taf|You have won.|SCR_SKIP_WAITKEY=1` (255 commands).

Two things worth carrying forward:

- **SCARE refused the IF Archive comp build `sophie.taf` (531015 bytes) — real
  parser bug, now FIXED in `sctafpar.cpp`.** Symptom was
  `parse_get_taf_integer: invalid integer at line 225749`, stack
  `Tasks/4489/Actions/1/Type`. Two earlier notes here were wrong: the file is
  **not** truncated (full 3,186,839 plain bytes / 254,549 lines, intact trailer),
  and while its data *is* damaged, that damage is survivable — real `run400.exe`
  under Wine loads that exact file (md5 `b2ebc41262384db587533ed547a6220f`) and
  plays it normally.
  - The bad record is a **task restriction**, not an action: the
    `cast *summon*` task has one restriction with `Type=12` and Vars
    `7667826 / 7209070 / 7471205`. Those are the LE dwords of the UTF-16 string
    **`"runner"`**, and 12 is its byte length — a stray string blob written over
    the restriction.
  - v4.0 `TASK_RESTR` enumerated only types 0–4, and the schema `?` test supports
    **`=` only** (`parse_test_expression`), so there was no `Case Else`: Type 12
    consumed zero Vars, the parse slid three lines, and it died on `''`.
  - Fix: `|V400_TASK_RESTR:Type>4?#Var1,#Var2,#Var3|` + a new
    `parse_fixup_v400()` (the v4.0 arm of `parse_fixup()` was
    `scr_fatal("unexpected call")`). Three-Var count **inferred from one sample**
    — instrumented, it fires once on the comp build and never across all 99
    `games/*.taf`. Suite **94/94 PASS**, no golden changed.
  - The two builds are *not* the same game: the archived one-line walkthrough
    (`walkthru.txt`) was written against the comp build and diverges from
    `sa.taf` in the crypt layout, in item names, and in several steps it simply
    never performs. Both are wired now — see the entry above.
- **`SCR_DEBUGGER_ENABLED=1` is useless for mid-run inspection in the headless
  build.** `debug_game_started()` opens the debug dialog *before turn 1* and the
  ANSI front end has no `#debug` command, so a piped
  `{ head -N sol; echo '#debug'; echo 'variables *'; }` reports **initial**
  values and eats the walkthrough as debugger commands (`e` → `Event`). Use
  `SCR_TRACE_TASKS=1` and read `Task: variable N (name) += x`.

## 2026-08-02 (later) — Key & Compass ADRIFT index swept: +17 games, +5 walkthrough pages

Source: `https://www.plover.net/~davidw/sol/idx_adrift.html` (720 ADRIFT works
indexed; **48 of them have a Key & Compass walkthrough/map**, spread over 21
pages). K&C **hosts no game files** — only walkthroughs — so the games had to be
sourced separately. 16 of the 21 pages were already on the Desktop as
`.webarchive`; the 5 that were not are now saved as plain HTML (plus `sol1.css`)
in `~/Desktop/Plover adrift walkthroughs/`:

`adr3quests.html` · `adrspr04.html` · `adrsum04.html` · `adrwcc06.html` ·
`cleft.html`

**Games added to `games/` (17 new symlinks, all boot clean under
`harness/scare`).** Staging dir: `~/Downloads/plover-adrift-2026-08/`.

| game | file | source |
| --- | --- | --- |
| The Curse of DragonShrine | `DragonShrineR43.taf` | IF Archive `mini-comps/adrift/springcomp04.zip` |
| Sommeril | `sommeril.taf` | ditto |
| Wax Worx | `wax_worx.taf` | ditto |
| Shards of Memory *(no K&C walkthrough)* | `shardsofmemory.taf` | ditto |
| The ADRIFT Project: Classified | `TheADRIFTProject.taf` | IF Archive `mini-comps/adrift/summercomp04.zip` |
| Shadrick's Underground Adventures | `ShadricksUnderground.taf` | ditto |
| Tears of a Tough Man | `Tear.taf` | ditto |
| Choose Your Own... | `chooseyourown.taf` | ditto |
| The Mystery of the Darkhaven Caves | `mysteryofcaves.taf` | ditto (K&C has a map for it, though the index doesn't link it) |
| Ticket to No Where *(no K&C walkthrough)* | `ticket.taf` | ditto |
| Glum Fiddle | `Glum Fiddle.taf` | IF Archive `mini-comps/adrift/writing-challenges-comp-2006.zip` |
| Jonathan Grimshaw: Space Tourist | `JGrim1.0.taf` | ditto |
| The Cleft in the Rock | `cleft.taf` | adrift.co `/game/681` → `/files/games/cleft.taf` |
| ADRIFTMAS Party | `ADRIFTMAS_Party.taf` | Wayback `shadowvault.net/games/adriftmasparty.taf` (3 MB) |
| The Quest *(Chris Moody)* | `tq3.taf` | Wayback `shadowvault.net/games/tq3.taf` |
| The quest *(BoyBiz)* | `yeh.taf` | Wayback `shadowvault.net/games/yeh.taf` |
| The Quest *(Chlestron, AIF)* | `fantasyworld.taf` | Wayback `delron.org.uk/games/thequest.zip` |

`tq3.taf` / `yeh.taf` are byte-exact against ScummVM `detection_tables.h`
(5000-byte MD5 + filesize). The three same-titled *Quest*s are disambiguated by
that table: `tq3` = Chris Moody, `yeh` = BoyBiz, `fantasyworld` = Chlestron.

**Not missing after all:** *The Murder of Jack Morely* is an a.k.a. of
**Confession**, already in the corpus as `Confession(1).taf` — and it is *absent*
from `3hourcompnov04.zip` despite that zip's blurb listing it. *Quest for the
Magic Healing Plant* is `mhpquest.taf`; *Color of Milk Coffee* is ADRIFT **5**
(`test/adrift5-games/coloromc.taf`).

**Author walkthroughs that came free with the comp zips** (bonus over K&C):
`springcomp04/walkthru/wt-{dragonshrine,sommeril,shards,waxwurx}.txt`,
`summercomp04/competition/wthroughs/{Shadrickwalkthrough,The ADRIFT Project
Walkthrough,ticketwalkthru}.txt`, `thequest/The Quest/walkthru.txt`.

Dead ends worth not re-walking: delron.org.uk's `/games/*.zip` are all 404 in the
Wayback Machine (only the review pages survive); adrift.co's 619-game listing has
none of ADRIFTMAS Party or the three *Quest*s; IF Archive has no ADRIFT End of
Year Comp 2002 package.

All 17 are now wired — see the *"Key & Compass batch: all 17 derived"* entry at
the top of this file.

## 2026-08-02 — the eleven unwired `.taf` in `games/` — ALL WIRED — 87/87 PASS

The coverage audit against ScummVM's `engines/glk/adrift/detection_tables.h`
(md5 over the first 5000 bytes, `engines/glk/detection.cpp:250`) turned up
twelve `.taf` sitting in `games/` with no regression row. One (`BeThere.taf`)
is ADRIFT 5 and already covered by `test/run_a5_walkthroughs.sh`; the other
eleven are now derived, wired and blessed. **Ten are wins**, one is a tour:

| row | file | result |
|---|---|---|
| `argh_solution.txt` | ARGH_sGreatEscape.taf | ★ WON |
| `spam_solution.txt` | SPAM.taf | ★ WON |
| `wreckage_solution.txt` | Wreckage.taf | ★ WON |
| `vagabond_solution.txt` | Vagabond.taf | ★ WON |
| `woof_solution.txt` | Woof.taf | ★ WON, **30/30 MAX** |
| `undefined_solution.txt` | Undefined1.taf | ★ WON, **3/3 MAX** |
| `ecod3_solution.txt` | ECOD3.taf (**3.9**) | ★ WON |
| `goblinhunt_solution.txt` | goblinhunt.taf | ★ WON |
| `agent4f_solution.txt` | agent_4F[1].A.taf | ★ WON |
| `adriftorama_solution.txt` | adriftorama.taf | ★ WON |
| `invasion_shirts_solution.txt` | Invasion of the Second-Hand Shirts.taf | tour row (no `EndGame` in the data at all) |

Things worth keeping:

- **`adriftorama_walkthrough.md` was wrong** and is rewritten. The game *is*
  completable; the win is **not** an `EndGame` — task 26 `#Campbell's Hole`
  just moves you to room 19 `THE END`, whose description is `*****You Win!*****`.
  The only `ACT type=6` in the file is task 48 `#Kill Campbell`, a **lose**.
  Grepping for `type=6 v1=0` alone will mis-triage this class of game.
  Its mechanic is also non-obvious: **`put ball on marker` before every
  `hit ball`** (the marker is a SURFACE, obj 11) — with the ball in hand the
  swing falls through to a per-room "<name> CLUB" joke task. Event 0 re-rolls
  ~95 variables every turn, so each putt is a dice roll; 30 cycles clear holes
  1–17 under the fixed seed (bisected: 29 fails), `guess the verb` clears Guess
  the Verb Mountain, one more cycle sinks Campbell's.
- **`SCR_TAG_WAITKEY` does not skip comments.** `os_read_line` skips `#` lines
  unconditionally (`os_ansi.cpp:274`), but the waitkey path is a raw `fgets`
  (`os_ansi.cpp:133`), so a press-a-key pause happily eats a comment line. The
  corpus convention of explicit blank lines is what makes goblinhunt (3+3+1+1
  pauses) and invasion (2 `[More]` pauses) reproducible; miscount and the next
  command is silently swallowed.
- **Wildcard tasks earlier in the table steal commands.** `define you` in
  *Undefined* scores nothing because task 1's `[* you *]` sits above task 4;
  `define voice` is the way in.
- **Worn ≠ held.** Goblin Hunt's task 21 restricts on the armour's *worn*
  state; carrying it drops you into task 22, an `EndGame(kill)`.
- **`the` is not a free optional word.** Invasion's task 8 is
  `{go/goto/move} {cross} {fallen} [e/east/tree/bridge/log]`, so
  `cross the fallen tree` is not understood but `cross fallen tree` is.
- Re-confirmed: SPAM.taf and Vagabond.taf transcripts contain a raw `0xAE`,
  so `export LC_ALL=C` and `grep -a` (BSD `grep -c` prints *nothing*, not `0`,
  on a file it decides is binary).

Two already-wired games — `Les Feux de l'enfer.taf` and
`Space Boy's First Adventure.taf` — have **no** `detection_tables.h` row and are
upstream-submission candidates.

## 2026-07-14 (later) — To_Hell_And_Beyond assisted route — ★ CLOSED (248/373, wired) — 75/75 PASS

**Section A is empty: the last open derivation item is done.** The 224-cmd
`harness/to_hell_and_beyond_assisted_solution.txt` was never broken — the
"DESYNCS near the end" claim below came from replaying it with only
`SCR_ASSUME_COMBAT=1`. This game needs **BOTH** assists: without
`SCR_ASSUME_MOVES=1` the dead `jump down` (Var2=-1) move leaves the player
trapped in the mansion, the rest of the script whiffs, and the closing `claim
the throne` gets "I don't understand what you mean!" — the exact reported
symptom (reproduced both ways today). With `SCR_ASSUME_COMBAT=1
SCR_ASSUME_MOVES=1` it wins **248/373** deterministically (verified 3×
byte-identical): 23 faithful + shore +5 + scorpion bounty +10 + ship approach
+10 + `^^xozimisdead^^` +50 + `claim the throne` +150, ending *"You are now
ruler of Beyond....... Congratulations!"*. Wired as a golden regression row
(marker `You are now ruler of Beyond`, env in the 4th MAP column) → the v4
suite is now **75/75 PASS** (twice, back-to-back). Note the TODO section A
estimate of "~293/373" was wrong; the walkthrough's 248/373 is what the route
scores. Writeup in `To_Hell_And_Beyond_walkthrough.md` (whose stale
`SC_ASSUME_*` env names were also refreshed to `SCR_ASSUME_*`).

## 2026-07-14 — WHOLE CORPUS WIRED into `run_v4_walkthroughs.sh` — 74/74 PASS

Every banked corpus solution is now a golden-diffed regression row (was 30 rows
covering only the Plover/Shadowpeak/ALEXIS games; now **74**, deterministic
across back-to-back full runs). What it took:

- **New optional 4th MAP column: per-row env** (`SCR_SEED=2` for circus,
  `SCR_ASSUME_COMBAT=1` for the Villains-and-Kings assisted row). Also
  restructured the MAP heredoc into a `map_rows()` function: macOS
  `/bin/bash` 3.2 mis-parses heredocs inside `$()` when the content's quote
  count is odd — one apostrophe in a new win marker broke the whole script.
- **Tour rows lock their documented maxima**: solutions that didn't already end
  with `score` got one appended, and the row's win-marker is the exact final
  `Your score is N out of a maximum of M.` line (wes_ghn 30/100 — a win row
  since the 2026-08-02 100/100 re-derivation, spirits_flight 50/95, thetest
  5/25, del_sol 24/46, les_feux 25/115
  (`Votre score est…`), villains 31/37 (was 13/37 + an assisted 30/37 row until
  the 2026-08-02 V390 re-derivation retired the assist), questi 5/10,
  cybercow-tour 6/10, matts_house 5/5, lifesimulation 0/0). Win rows use the
  game's own victory text. inverness (75/205) joined them on 2026-08-02: with
  the route's current two `z` pad turns after the knockout the cut-scene no
  longer swallows the trailing `score`, so the row is now marker-guarded by
  `Your score is 75 out of a maximum of 205.` like the rest. It previously used
  the caught-scene prose, and that is exactly how it hid a desync — the
  `scr_randomint` low-bit fix re-rolled the witches' riddle, `answer step`
  stopped matching, and the row kept passing at 65/205.
- **Main Course was silently BROKEN and is re-derived** (win restored,
  wired): the banked solution had lost its two leading waitkey blanks, and the
  NPCs-before-events tick-order fix re-timed the wandering cat + combat RNG.
  Diagnosis notes are in `Main_Course_walkthrough.md`. Footgun worth keeping:
  an `attack <npc>` with the NPC absent falls through the grammar to "I don't
  understand what you mean!" and does NOT advance the turn.
- **Stale-claim correction:** `harness/to_hell_and_beyond_assisted_solution.txt`
  (224 cmds, ends `claim the throne`) *does* exist — but it DESYNCS near the
  end (the final command is not understood; no win). Banking the full assisted
  To-Hell route therefore remains the one open derivation item (section A).
  Its faithful 3-command opening row IS wired (`to_hell_and_beyond_solution.txt`).
  **[2026-07-14 later: WRONG — that replay was missing `SCR_ASSUME_MOVES=1`.
  With both assists the script wins 248/373; now wired. See the entry above.]**

Everything else replayed byte-identically on today's scarier binary — i.e. the
tick-order fix broke exactly one of the ~45 unwired solutions.

## 2026-07-02 — Shadowpeak re-derived for the tick-order fix — parity restored (710/715/740)

The NPCs-before-events tick-order fix broke all three Shadowpeak solutions (one
turn short vs the EVENT-92 Morac timer, plus three NPC-walk meet/charTask
triggers that now fire only when the NPC steps onto the player). All three
re-derived to exact old-order score parity, 0 deaths, and added to
`run_v4_walkthroughs.sh` with blessed goldens (regression 20/20 PASS). Session
writeup at the top of `Shadowpeak_walkthrough.md` (which also carries the
mechanism details, after the type-5/turn-order TODO it used to cite was pruned);
the Damastus chase is re-derivable in one run with `harness/shadowpeak_chase.py`. Copies + fresh
`scare` synced to `~/adrift-battle/harness` (36 other solutions verified
unaffected by the binary refresh) and `~/scare/adrift-walkthroughs/harness`.

## 2026-07-13 — ALEXIS max-score — ★ BANKED AND WIRED (55/65 and 58/65)

**Closed.** Both cube routes are now regression rows in `run_v4_walkthroughs.sh`
with blessed goldens, and the whole v4 suite is **22/22 PASS**:

| solution | difficulty | score | route |
|---|---|---|---|
| `alexis_solution.txt` (171 cmds) | Easy | **55/65** WIN | *carry* the magic cube (hit 50 → clean kills) |
| `alexis_worn_cube_solution.txt` (252 cmds) | Hard | **58/65** WIN | *wear* the cube (Defence +50 = immunity) + Hard's +10 |

58/65 is the max reached: worn ≠ wielded, so offense drops to the small sword and
the four *flee*-type enemies (wolf/bridgekeeper/king/eagle) escape before dying,
costing points the carry route collects — the two routes trade 12 points of kills
against 10 points of Hard bonus plus 5 more survivable fights. Win marker for both
rows is `you have beaten Urgorn`; scores confirmed by injecting `score` before the
killing blow (the game ends in credits, so a trailing `score` never executes).

Also fixed while doing this: `harness/build.sh` no longer links — `scmap.cpp` (the
new ADRIFT 4 map port) calls `map_free()` from `mapdraw.cpp`, which the `sc*.cpp`
glob doesn't match. Added `mapdraw.cpp` to the harness link line (it is plain C++,
no Glk).

### Background — the 2026-06-28 combat RE that made this possible

**Reverse-engineered the 3.9 Battle
System** with a temp `battle_resolve` `SC_TRACE_DMG` trace + a new committed
`SC_DUMP_OBJLOC` scdump path (object positions + weapon/armour Battle props):

- **Corrected two false claims** in `ALEXIS_walkthrough.md`: the gourd does NOT
  give +20 defence and the potion does NOT give +200 stamina — both are pure
  flavour (zero type-7 ChangeBattle actions; the only type-7s are the difficulty
  tasks). `easy` = +20 **Stamina** +20 Strength (the survival buffer), `hard` =
  −20/−20 +10 score, `medium` = neutral +5.
- **Why the win is fragile:** player effective **Defence = 2** (glass cannon),
  enemies wield weapons (effective Strength up to **35**: Narfild 35, Larnt 30,
  Longmore king 25), and enemy **targeting is a per-attack coin-flip** between the
  player and Serond. The banked stream survives only because the flips land on
  Serond (player hit 4×, 54 dmg, ends 66/120). **Any added turn reshuffles the
  RNG → the flips change → an unlucky run takes >120 dmg and dies** (adding
  `turn ring` → Larnt one-shots in the Main Hall). Confirmed: phasing `look`s
  don't rescue it; you must harden the player first.
- **Gear table dumped** (HitValue / ProtectionValue / location; armour STACKS):
  large knife 30 (room21 but "too heavy" w/o a Strength boost), **magic cube
  50/50** (hidden master item), elven armour 3 (cupboard r37), longmore chest
  plate 5, steel vest/metal helmet 3, elven chain mail 5 (NPC-worn). 
- **Resume route as mapped then:** detour the **combat-safe elven village**
  (rooms 29–38, no hostiles) for **water +1+1** and **wear elven armour**, stack
  surface armour, THEN add turn ring +3 + on-path kills (Serond present) +
  goblin→juice→carry the large knife → hard +10. *(Superseded: the **magic cube**
  — spell `nnamen tutem selronden flar darg` at game start, hit/protection 50/50 —
  hardens the player outright and made the armour grind unnecessary. See the
  banked entry above.)* Committed: `SC_DUMP_OBJLOC` tooling + walkthrough
  rewrite. (This session's env was unstable — classifier outages + OOM; always
  `ulimit`-bound single runs, never background, per scare-harness-oom note.)

## 2026-06-28 — Through time — DONE: **UNFINISHED DEMO (0/0, no win)**

The last untouched game in the corpus. Turns out to be an **incomplete demo**:
only the opening **1954 Texas farm** is wired up. Trying to step off the porch
into the yard hits the author's own wall — *"You can't leave the porch.... This
is as far as this adventure will take you at this point. Take care ;-)"* — in
every direction.

- **Playable content** = living room (read magazines → matches + TV remote drop),
  kitchen (garbage can / sixpack / pizza), a scripted bedroom bounce (the
  nagging wife ejects you), and the porch dead-end. Turning on the **TV** plays
  the plot hook: a news flash about a UFO heading for *Duff's Waterhole* (= the
  farm). Then the demo ends.
- **Unreachable data** (164 tasks, ~130 of them sealed off): an alien spaceship
  (airlock/corridors/reception card/elevator/council passphrase *"through
  adversity to the stars"*/lab/**time machine** + artifact analyzer), **Ancient
  Rome** (Caesar's Villa, Venus Victrix Temple, Pompey's Theater, **Curia
  Pompeii** = Caesar's assassination; pugio + Roman coin), and the **Battle of
  Tours, 732 AD** (Charles Martel's camp vs four Muslim camps; embeds the
  Wikipedia link as a task; scimitar/battleaxe).
- **Endings** all sit past the wall: 0 win (`var1=0` — none exists), 1 lose
  (talk to guards), 1 death ("Busted bladder"), 8 stop (map-edge walks). **No
  reachable `ChangeScore`** → `score` is permanently 0/0.
- Banked `harness/through_time_solution.txt` (deterministic tour to the wall) +
  `Through_time_walkthrough.md`. Triage section D updated.

**This completes the corpus triage** — every `.taf` in `games/` now has a
walkthrough or a documented verdict. Remaining open work is the parked **circus**
token-economy grind (below) and the optional ~80 missable extras in Shadowpeak.

## 2026-06-28 (cont.) — circus (*Menagerie!*) — ★ WON 64/140 (economy SOLVED)

Picked the parked circus back up and **banked a deterministic full win**
(`harness/circus_solution.txt`, `SC_SEED=2`; verified via `play.sh`; marker *"PETA
plants a Willow tree in your name"*). The grind that had it parked is solved:

- **FOOTGUN: the committed `harness/scare` binary was stale** — `SC_SEED=2` died in
  the funhouse until I re-ran `build.sh`. After rebuild, seed 2 survives (fundeath=8);
  seeds 1 & 9 die (fundeath==1, ~1/11). Always rebuild before trusting seed behaviour.
- **Economy cracked:** the closer is the **toy-knife chain**, not the food pump.
  `buy peanut` ($1) → `give peanut to pringles` (monkey, room14, +10) → `take knife`
  → `sell knife` to Marie (**+$2 +5pts**) = net **+$1 +18pts**. Token granularity
  ($1=2 tok, 9 spent) forces buying **10 tokens/$5**; start $2 + knife + selling ~30
  points covers it. (`play wheel`/`give tip cecily`/food are net-negative — avoided.)
- **Combo is randomised** = the three tarot-card numerals from `ask reading`
  (re-read `x first/second/third card`). The old note's "13/10/5" was wrong; **seed 2
  = XIII/IX/V → 13, 9, 5**. The reading resets combo vars 27/28/29 to 0; cards
  repopulate them (lock tasks 82–90).
- **Camera must be LOADED:** holding the parts isn't enough — `put battery in camera`
  + `put tape in camera` (it's a container), then `use camera` in room4 sets
  `videodone`, then `home` = WIN (+20 tier; Willow dies in a car crash but the footage
  reaches PETA — the author's bittersweet victory).
- **NPC timing solved by spam-until-present:** Joe (peanut, room0) and Barb (tape,
  room4) wander deterministically; repeat the action until they walk in (robust to
  turn drift). Pringles & Bill are stationary (Bill only leaves on `give popcorn zap`).
- Tape is in the lion cage (Barb fetches it — never `open` a cage = death); battery on
  the Platform (never `jump` = death). Compass rotated at the entrance/bleachers
  (East Bleachers→entrance is `sw`, by prose).

**The corpus is now complete: every winnable game in `games/` has a banked win.**

## 2026-06-28 — circus (*Menagerie!*) — STRUCTURE FULLY DECODED (economy grind remains)

Picked up the parked circus. **Cracked the entire win/recovery/economy graph** — see
`Circus_walkthrough.md` for the full writeup. Key results:
- **Win = set `videodone`(var22)=1 via `use camera` (room4), then `home`** (tasks 48/49/50,
  3 score tiers on `thescore`(var31) vs `escore1/2` = 120/90; bare win = task50, any score).
- **Theft scatter** (pickpocket on winning the ticket): videocamera→**trunk(room5)**,
  videotape→**lion cage(room4)**, battery→**room12(Platform)**, case→**NPC15(Zap)** (case
  optional — task31 films with camera+battery+tape only).
- **Recovery**: funhouse `show mirror to bill`(+5, mandatory prereq) → fortune `ask reading`
  (4 tokens, +6, reveals trunk combo, needs the mirror task done) → trunk combo **13/10/5**
  + `open trunk` → take camera → `ask barb for tape`(room4) → `take battery`(room12) →
  `use camera`×N → `home`. (Do NOT open any cage / `jump` the Platform = deaths.)
- **Funhouse is an RNG death** (`fundeath`=rnd(0,10); ==1 ⇒ instant clown-heart-attack on
  `west`). **Default harness seed 1 ⇒ fundeath==1 ⇒ UNWINNABLE**; ~1/11 seeds are deadly.
  Made `harness/seed.c` read **`SC_SEED`** (default 1). **Use `SC_SEED=2`** (fundeath=8).
- **Economy is the unfinished part**: need 9 tokens (4 duck + 1 funhouse + 4 reading),
  start with 4 ($2). Sources are marginal: `sell 10 pts→$1` (2 tokens), `look under
  bleachers` +5, food pump (`buy`+`eat` peanut/popcorn = +3 net pts each, vendor must be
  present), ring toss (+0.2 token/win). Funhouse "$5 tip" (task67) is **blocked** — needs
  Bill absent and nothing moves Bill. Banked `harness/circus_solution.txt` (seed 2) reaches
  Madame Elsa **1 token short of the reading** — resume by filling the token grind there.
- **Tooling**: `seed.c` now `SC_SEED`-configurable; restriction var-index = `Var1-2`,
  action var-index = `Var1` direct (documented in Circus_walkthrough.md).

## 2026-06-28 — Shadowpeak — ★ COMPLETE: WON 710/790, 0 deaths (21 sessions)

Deterministic winning walkthrough banked in `harness/shadowpeak_solution.txt`; full
session-by-session writeup in `Shadowpeak_walkthrough.md`, author-hint dump in
`Shadowpeak_hints.txt`. The last big winnable ADRIFT game in the corpus is done. The
remaining ~80 points are scattered missable/exclusive extras, deliberately not chased
to protect the clean win. (Details below were the in-progress log.)

## 2026-06-27 (cont. 2) — Shadowpeak — STARTED (opening banked @ 20/790, multi-session)

`Shadowpeak_walkthrough.md`; solution `harness/shadowpeak_solution.txt`. Began the
last big winnable game. **Foundation laid + a deterministic opening to 20/790.**

- **Win fully decoded:** task 417 `blow horn` with 3 restrictions — hold **The horn
  of the angels** (obj147, Hidden/task-revealed) + hold the **sceptre** (obj112,
  Hidden) + **be in the same room as Asmodeus** (NPC 39; type-3 char restr v3=41 →
  npc 41−2=39). The endgame is a **Hell realm** (NPCs 36–41 = Cerberus / Charon /
  lost souls / Asmodeus / Devils / Lazaraz). Morac is a scripted **"seeker"**
  chasing-enemy (tasks `#Moracyboyarrives`/`#Morackillsplayer`/`evade morac`/`kill
  morac` +50); your starting sword is named "Seeker". Max 790 / 69 score tasks /
  1 win / 1 lose / **41 death endings**.
- **`hint` command = the author's built-in, context-sensitive puzzle guide** — shows
  only the hints relevant to your current area (consult per-area). Also `status`
  (combat HP), `wield`, `kill <name>`, `ask <name> about <x>`, `say <x>`.
- **Opening banked (20/790):** from Stonehenge (0) `s`→3 (Lightning tree, Fetlar)
  `se`→4 (Chasm edge) `u`→6 (Leaning tree); `examine nest`, `take egg`, `take
  medallion`; `d`,`nw`→3; **`give fet egg`** → broadsword "Seeker" (+10); `n`→0;
  **`fet read runes`** (+5, a riddle *"the only thing you can be sure to achieve in
  your life?"*); answer **`death`** (+5) → a Shield appears, `take shield`. Fetlar
  is a companion who follows you room-to-room.
- **NAVIGATION FOOTGUN (important):** Shadowpeak has **severe compass rotation**,
  SCARE room *indices ≠ display names*, AND a room's in-game offered directions
  don't line up with the dump's exit slots. **Navigate strictly by room prose /
  "you can move…"**, never by the dumped N/E/S/W. (Verified opening room-name map is
  in the walkthrough.)
- **Resume:** explore the rest of Realm 1 (Great Swamp, ledges, rooms ~14–33), find
  what activates the Stonehenge **portal** (tasks 292/293; currently "What portal?")
  = the link to the other realms. Then the long haul: NPC-kill points, `snap staff`
  +50 (`say borantha` repairs it first), `kill morac` +50, the Hell endgame →
  horn + sceptre → `blow horn` win. Faithful native-4.0; no engine change.

## 2026-06-27 (cont.) — Space Boy's First Adventure — **WON, 1184/1374** (was parked 275)

`Space_Boy_walkthrough.md`; solution `harness/space_boy_solution.txt` (145 cmds).
Picked up the parked 275/1374 (Castle + Volcano done, 3/4 power items) and **banked
the full deterministic win** (verified 3× identical; marker *"STAY TUNED FOR MORE
EXCITING EPISODES…"*). The East region + endgame, fully solved:

- **Return from Treasure Island:** `fly ne`→25 LAVaaH, `n`→11 hub (compass rotated —
  in-game `n` = the dump's NE exit).
- **East = the "TO THE GARAGE" letter maze.** `fly east`→26, `enter cave` triggers a
  scripted cave-in into room 72; `take small shovel`, `dig a hole in sand`→27,
  `dig more`→28, `w`→29 (dark). `take stick` + `light stick with goggles` (Heat
  Goggles ignite it). Each maze room shows one carved letter; follow **T-O-T-H-E-G-
  A-R-A-G-E** (maze is NOT compass-rotated — dump dirs work: `w,s,w,w,n` to the G
  room, then `w,n` past the A/R to the second A, then `e,e,u`→40 Garage Bay).
- **Two elemental gates inside the maze** (the powers pay off): the **G room** has a
  block of ice (`melt ice with goggles` → a wind blows out the stick → Dark Room →
  `light stick with goggles` returns you, ice gone → `n`); the **second A room** has
  a fireball (`freeze fire with ice gloves`).
- **Transporter + Phased Ion Bridge → Strength Belt.** Garage Office (68) has the
  Phased Ion Bridge (`take` it, opens the window→`out`); back home, **`put bridge in
  power plant`** (an openable container in the Hangar Bay — the home transporter's
  blue button fails until the bridge is *inside* container #3, decoded from the
  object-**location** restriction `(16, Inside, container-3)`). `push blue button`
  →Moon Base 69, `out`→67, `e`→Mess Hall 60 `take fork`, `w`, `use fork on hole`
  (also powers the Beam Generator) → room 66 **Strength Belt** (`take`+`wear`).
- **Endgame:** Moon Base `push red button`→home, `move huge rock` (belt worn) →71
  `take key` (Room Key) → Living Room `unlock door`+`open room door`→7 (Evil Man,
  **harmless** — 0 damage) → `take cape` (+105) + `drop cape to the floor` (+250, a
  scripted no-restriction scoring task that fires despite a cosmetic "Drop what?"
  library echo) → `w`→65 `read scribbled note` (+200) = **WIN**.

**Class:** faithful native-4.0 win, fully deterministic (no combat-assist, no engine
change). Reusable lessons: (1) maze rooms reuse the *same room index* as the volcano
elemental-challenge rooms (room 34 is the "G room" AND the melt-ice room); (2) SCARE
room-exit gate decode — `Var3`=type (0 task / 1 object-state), `Var1−1`=task or
stateful-object index, `Var2`=expected; (3) restriction **type-0 = object LOCATION**
(`restr_object_in_place`: Var2 0/6=hidden-at-room, 1/7=held, 2/8=worn, 3/9=visible,
4/10=inside-container, 5/11=on-surface; Var3 0=player/nothing else 1-based
char/container/surface) — this is how the bridge-in-Power-Plant gate was cracked.
Compass is rotated in the home/volcano zones (navigate by prose) but NOT in the maze.

## 2026-06-26 (cont. 3) — Les Feux de l'enfer — TESTED: **UNWINNABLE by design** (score-only, max 115)

`Les Feux de l'enfer` ("The Fires of Hell") — **native ADRIFT 4.0** (header byte8
`0x93`), **French**, by ?. A Battle-System dungeon crawl: you return to your home
town of Calah and descend into the underground dungeon of the demon **Anarazel** (in
the Wastes of Chaos) to slay him. **289 tasks, 23 NPCs, 59 exits, 1 event; max score
115** (18 all-positive ChangeScore tasks — mostly Battle-System enemy kills +5 each:
homchove/goule/demon_effroi/voyou/voleur/garde/capo/chef/tarator/assassin, plus
puzzles: disarm trap +10, give potions to the femme +10, `jouer note mi do` +10,
`Hors d'ici` +10, examine objet +5, push right button +5, etc.).

**Boots & plays fine** (NOT a hang): start menu `1.Nouvelle partie / 2.Préface /
3.Aide / 4.Note de l'auteur` — pick `1`, then type `intro` or `passer` to begin.
You start with épée longue + armure chainmail + sac à dos + bourse.

**No win exists.** All **5 `ACT type=6` endings are `v1=2` (death/lose)** — flee
n/s from room8 (92/93), `***mort du player***` (106), `[s]` room24 (174), `[jump]`
room33 (203). **Zero `v1=0` EndGame.** No victory text anywhere (grep gagn/victoire/
félicitation/… = nil). The "kill the demon" tasks (85/105/146 demon_effroi/2mob/
demonkill) are `type=7` ChangeBattle + `type=1` move actions, NOT endings.

**The finale chain proves it (the killer evidence):** the deepest path is room34's
~50-task two-note music puzzle (only `jouer note mi do` = task 232 +10 opens EXIT
room34 W→35), then room35 `n`→ room36. Room 36 = the demon's lair (NPC20 rhinocéros
guard + NPC21 **mourant** = the dying demon): `rhinoceros_kill` → `kill mourant`
(spawns obj110 cadavre) → `examine cadavre` (reveals obj111 **lumière**) →
**`enter lumière` → `ACT type=1 v1=0 v3=8` = moves the PLAYER back to room 8** (the
entrance), with NO EndGame. Room 8 then only offers `Hors d'ici` (+10, needs the
magic mirror) or death. So you slay Anarazel, enter the light, and get dumped at the
start — **the author never wired a win ending.**

**Class & verdict:** faithful, unwinnable-by-design (incomplete authoring) — same
class as IceCream / SRSintro / Through-time, but notable because the Préface *promises*
the Anarazel kill as the goal while the `.taf` delivers no victory. **Native 4.0
(not a 3.9→4.0 conversion), so NOT the conversion-damage hypothesis and NOT a SCARE
divergence** — there is simply no win action for any Runner to fire.

### 2026-06-27: max-score derivation — **115 is IMPOSSIBLE (orphan); true max ≤ 105, RNG-hard**

Pursued the 115/115 max-score route (`Les_Feux_de_l_enfer_walkthrough.md` +
`harness/les_feux_solution.txt`). **Cracked all mechanics, banked a verified
deterministic route to 25/115** (5 combat kills: ogre/goule/demon/voyou/voleur), and
found two structural reasons 115 is unreachable:

1. **Disarm-trap +10 (task 116) is an unreachable ORPHAN — faithful authoring bug.**
   The crystal key is initialised **inside** the trap (container #4; `get cristal` =
   task 112, restr "object in place 39,**4**,5" PASS — verified by SC_TRACE_TASKS),
   but every disarm-family task (113/114/115/118/119/120) requires the key **on
   surface #2** (restr type 5), and **NO action in the game ever moves obj39 onto a
   surface** (all its move-actions are dest-type 4=to-player or 0=to-room). So the
   +10 can never fire. Real Runner fails identically (same data). **→ true max ≤ 105.**
2. **Room-19 capo/chef (+10 of +15) = one-shot RNG (var#16 via task 153, rep=0)** —
   the guard count is fixed by upstream RNG-draw count, NOT tunable by neutral turns;
   seed gives a lone garde (+5). Plus **all combat is RNG-stream-position-dependent**
   (kill-swing counts shift if any upstream turn changes), so the route must be tuned
   holistically. The rope (+10, room 33) is RNG but **retryable** (`throw grappin`
   repeats). So reliably-deterministic ceiling ≈ **85–95**; ≤105 only with RNG luck.

**Mechanics cracked (reusable):** verbs are SCARE's built-in **ENGLISH** library
(French `tue`/`bois` fail; use `attack`/`drink`/`open`/`get`, dirs `n/s/e/o`);
`attack <enemy>` combat works no-assist; enemies are dormant until triggered
(`look` wakes the goule, `ouvre coffre` the ogre, `open porte` the petit homme);
two-step doors (a `[n]` task grants an item w/o moving → need a 2nd `n`); flee=death
only while enemy present; max endurance 13 (fiole bleue heals to cap, drink mid-fight);
petit-homme answer **1/2 not 3** (3's task-105 chain eats the voleur points).
**Restriction operators (from screstrs.c):** type-4 var cmp v2: 0=`<` 1=`<=` 2=`==`
3=`>=` 4=`>` 5=`!=` (10-15 = compare-to-variable); type-3 = player/NPC presence
(v2=0 "in same room as NPC v3-2"); char/obj move dests are 1-based (v3=11→room10,
v3=0→off-stage). No engine change; no combat-assist needed. All runs via `safeplay.sh`.

**Resume:** the remaining ~10 deterministic points (buttons/vieillard/tarator/femme→
mirror/assassin/grappin/note/Hors d'ici) need the item-fetch web mapped + holistic
RNG tuning — a multi-session grind for an ~85-105 cap on a no-win game. Table of all
18 events + methods is in `Les_Feux_de_l_enfer_walkthrough.md`.

## 2026-06-26 (cont. 2) — circus (*Menagerie!*) TRIAGED + opening banked, PARKED

> ⚠️ **SAFETY (learned the hard way): playing event-heavy ADRIFT games through the
> headless harness can eat ALL system RAM and HARD-CRASH the machine.** It happened
> this session while driving `circus.taf`. SCARE buffers a turn's output text in
> memory; Menagerie is event-saturated (`runmeeveryturn`, `randomwalks`, 5 per-turn
> music events, timed clown/pickpocket/tightrope events, ~16 walking NPCs), so a
> command that puts two tasks/events into a mutual re-trigger within one turn grows
> the buffer without bound → OOM. **Always run bounded** — use
> `scratchpad/safeplay.sh` (`ulimit -t 12` CPU-seconds, SIGXCPU-killed on macOS, +
> `head -c 4M` output cap). NEVER `run_in_background` a harness run. A run that dies
> at the CPU limit = you hit the loop trigger; bisect the command list to isolate it.
> (Memory: `scare-harness-oom-bound-runs`.)

**circus.taf = David Good's "Menagerie!"** (v1.03, 2001; **WON 1st place** ADRIFT
Spring 2001 Minicomp). Native **ADRIFT 3.90.17**. You are **Willow Murphy**, a PETA
spy, sent to the Waleri Bros. Menagerie & Circus to **film animal cruelty on a
videocamera without getting caught**. **18 rooms, 158 tasks, 16 NPCs, 18 events.**
Difficulty (`Easy`/`Medium`/`Hard`, Medium default — type before any scoring) gates
the clown/pac timers and the stored max (**Easy max = 140**).

**Win = `home`** (the only `ACT type=6 v1=0` endings: tasks 48/49/50, 3 tiers gated
on var#33 ≥ 52/53 and var#24==1). `v1=2` type-6 = death endings (opening ANY animal
cage = instant death: lion/tiger/gorilla/elephant tasks 10/11/36/38; plus mad-clown,
tightrope/guido, lion-tamer, magic-act, elephant-stampede, gorillas-loose, timed-die).

**Confirmed opening spine (deterministic, banked in `harness/circus_solution.txt`,
reaches score 10/140):** `Easy` → `open case` (camera is inside) → `n` (Main Entrance,
room0) → `buy 4 tokens` ($2 start = 4 tokens, 2-for-$1 from Marie) → `s e` (Midway
East, room15) → `play duck pond` ×4. The duck pond is a **random carnival game**
(var#8 = which duck, set per play under the seed); the seed sequence yields
duck-toy / clown / frog-toy / **FREE TICKET** on the 4th play (**task 13** moves
obj2=[ticket] to you, +10) — **and the ticket is what actually opens the big top**
(the tent-interior exits gate on task 13, NOT on a bought ticket; the $17 Marie
"Adult Admission" is a **red herring**).

**The two hard gates:**
1. **Camera theft.** Winning the ticket triggers the **pickpocket** (EVENT 2): the
   camera case + videocamera are stolen ("Maybe if you can find it..."). The filming
   tasks (31/32/33 `use camera`, room4, +10 each, repeatable) need the camera back.
2. **Funhouse recovery.** Camera is recovered in **room10 = the Funhouse interior**,
   reached by `west` from the Midway (**task 19**, costs 1 token, gated var#14==1 —
   almost certainly set BY the theft, i.e. the thief flees into the funhouse). Bill
   (NPC 8) is there; the recovery chain is the **mirror** tasks: `show mirror to bill`
   (task 66, +5) → `take money` (task 67, +5) and/or `give popcorn to bill` (116, +5).

**The core challenge = a BOOTSTRAP ECONOMY with RNG-seeded outcomes.** $2 start = 4
tokens, and winning the ticket consumes all 4 → **$0/0 tokens, cannot afford the
1-token funhouse entry.** So you must interleave the other point/cash sources:
midway games (ring toss room9 +6, wheel room16 +5, pac-man room17 +3 — all random
outcomes via var#16/21/47/48 set by per-turn events), prize-selling (`sell toy`
room0 +5), and the **points↔cash↔score** loop (`sell N points` room0 trades score
for money but reduces var#33, which the win needs ≥52/53; `sell/buy N tokens`
room0). Vars: **var#4 = dollars, var#5 = tokens, var#31/#33 = points/footage,
var#7 = "have a token" flag.** Because every game outcome is drawn from the shared
`erkyrath` seed-1234 stream (like Bomb Threat / Melbourne Beach), the route is
**turn-order sensitive** — a deterministic win exists but needs iterative tuning of
which games to play in which order to bank ≥52 points AND keep enough tokens/cash
for the funhouse + big-top filming.

**Map (from EXIT dump):** room9 Midway(start) — N→0 MainEntrance, E→15 MidwayEast
(duck pond), S→16 (wheel), NW→13, W→10 Funhouse(task19/token). room0 — N/NE/SW into
tent(2/1/7) **gated task13=ticket**, S→9. room1/7 = E/W bleachers (room1 D→14 "look
under bleachers" task93 +5). room4 = the ring/cages (filming `use camera` here).
room2 U→12 = highwire (jump task9 = death; save guido task41). room11 fortune teller
(ask reading task4 +6). room17 = pac-man (W of room16). NPCs: Marie(booth), Bill,
Wemmie, Pringles(walks), Lance, etc.

**Resume next:** solve the token/cash bootstrap so you can (a) recover the camera in
the funhouse, then (b) enter the big top and `use camera` on each cruelty act until
var#33 ≥ 52 (Easy tier, task 50), then `home`. Decode the type-3/type-4 operator
semantics from `screstrs.c`/`sctasks.c` if the RNG tuning needs it (v2 codes 10/13
on the win restr are non-standard — confirm they're var-vs-var, not var-vs-const).
All runs via `safeplay.sh`. Tree: only docs + `harness/circus_solution.txt` +
`WALKTHROUGH_TODO.md` touched; no terp `.c` edits.

## 2026-06-26 session (cont.) — PARKED (2 more banked: Matt's House, Screen Savers)

**Parked here.** This continuation added **Matt's House** (sandbox, 5/5, no win)
and **The Screen Savers on Planet X** (**WON 142/142**, full max, verified 3×) —
entries below. Also consolidated all walkthrough docs/solutions into the repo
(`terps/scare/adrift-walkthroughs/`) as the single source of truth; the
`~/adrift-battle` copies are working mirrors. Tree clean (docs/solutions only; no
terp `.c` edits). **Uncommitted** in the repo: `Matts_House_walkthrough.md`,
`The_Screen_Savers_On_Planet_X_walkthrough.md`, the two new `harness/*_solution.txt`,
and the modified `WALKTHROUGH_TODO.md` — ready to commit when desired.

## 2026-06-26 session — PARKED (3 games banked: tcom, SRSintro, ALEXIS)

**Parked here.** This session added 3 deliverables (entries below): **tcom**
(WON 0/0), **SRSintro** (intro demo, 0/0 no ending), **ALEXIS** (WON 23/65,
native 3.9, win verified 3×). Tree clean (docs/solutions only; no terp `.c`
edits — `scdump` is already committed in the harness).

**Resume points (untouched / partial), smallest-effort first:**
- ~~**ALEXIS max-score pass**~~ — **DONE (55/65 carry-cube, 58/65 worn-cube; both
  wired with goldens)** — see the 2026-07-13 entry at the top.
- ~~**Matt's House**~~ — **DONE** (sandbox, max 5/5, no win) — see 2026-06-26 entry below.
- ~~**The Screen Savers On Planet X**~~ — **DONE (WON 142/142)** — see 2026-06-26 entry below.
- **circus** (*Menagerie!*, 158 tasks) — **TRIAGED + opening banked (10/140), PARKED**;
  resume at the token/cash bootstrap → funhouse camera recovery → big-top filming →
  `home` (see the 2026-06-26 cont.2 entry above).
- ~~**Les Feux de l'enfer**~~ — **TESTED: UNWINNABLE by design** (native 4.0, French,
  289 tasks, death-only endings). **115/115 is IMPOSSIBLE** — the disarm-trap +10 is
  an unreachable authoring orphan, so true max ≤ 105 (and that's RNG-hard). Verified
  deterministic route to 25/115 banked; full analysis in
  `Les_Feux_de_l_enfer_walkthrough.md` (see 2026-06-27 entry above).
- ~~**Space Boy's First Adventure**~~ — **DONE, WON 1184/1374** (see the 2026-06-27
  entry at the top).
- **Shadowpeak** (574 tasks, win route not banked) — the last big multi-session one.
  circus (*Menagerie!*) is winnable but parked at the RNG token/cash bootstrap
  (10/140, OOM-risky); ALEXIS's max-score pass is since **DONE** (55/65 + 58/65),
  Les Feux's remains optional.

## 2026-06-26: The Screen Savers on Planet X — **WON, full 142/142**

`The_Screen_Savers_On_Planet_X_walkthrough.md`; solution
`harness/screen_savers_solution.txt` (132 cmds). Native **ADRIFT 3.9** TechTV fan
game (38 rooms, 10 NPCs, no combat): all **13 *Screen Savers* cast members** are
scattered across Planet X + a space section, and you must herd them back to the
studio. **The win is a single all-rooms `*` task (task 0) gated on game-variable
#1 == 13** — a "cast gathered" counter bumped by **13 milestone tasks**; the win
fires on the first command *after* the 13th completes (solution ends on a bare
`look`). **Stored max 142 = the thirteen +10 milestones + `search room` +6 +
`drop bulldozer` +6** (the `push green button` travel tasks score nothing — an
earlier read of a restriction `Var1` as a ChangeScore was wrong). Reached the win
and the max simultaneously, verified 3× identical (marker *"You've managed to get
everyone to the set! Congratulations!"*).

**Structure RE'd from the dump (incl. the full EXIT table + event list):** studio
hub (Main hall → Lab A **teleporter** to the office cubes) + planet overworld
(Library/Dark Room, Hotel/Cafe, Mr. Universe, Sea of Dust, Rocket Pad) + a
**rocket/space finale** flown by `set dial to <code>`+`push green button`
(1119=Galactic Mart, 3071=rift, 4692=satellite, 7438=saucer; `push blue button`
returns to pad; EVA needs the spacesuit **worn**). Cast: Megan `install disc`,
Jessica `drop peel`, Martin `play video`, Darci `flip switch`, Joshua `shoot
bulldozer`, Tom `talk to Tom about John Hanson` (riddle = first president of the
U.S. Congress), Roger `show printout to Roger` (ID→tube→printout), Patrick `drop
bulldozer`, Scott `buy modulator`, Yosh `install modulator`, Cat (rift), Morgan
`kick satellite`, Leo `enter saucer`. **Three ordering traps:** (1) the bazooka is
"Hotel property" — `drop bazooka` before leaving; (2) `enter rift` is allowed only
*before* the rift is fixed — that's how you reach the Storage Room for the patch
(sticker→red crate→patch→`fix rift`), so it's not circular; (3) **the saucer must
be the LAST space trip** because `enter saucer` warps you back to Outside studio.
Faithful; no SCARE change, no combat-assist. (Object-move actions use 1-based room
refs, player-moves are direct — harmless quirk, noted because it explains where
scattered items land.)

## 2026-06-26: Matt's House — **sandbox, max 5/5, no win**

`Matts_House_walkthrough.md`; solution `harness/matts_house_solution.txt` (7
cmds). Native **ADRIFT 3.9** (byte8 `0x94`/byte10 `0x37`) — a juvenile author's
"day-in-the-life" model of his house (~24 rooms, family NPCs Mom/Dad/Rachel + the
dog Dozer). Structural dump of all **105 tasks: zero `ACT type=6` (EndGame) ⇒ no
win/lose/death ending**, and **exactly one `ACT type=4` (ChangeScore)** = task 11
`eat apple` (+5) ⇒ stored **max 5, 100%-completable in one command**. Route is a
straight dash to the Kitchen: `n` (stand up, leave Your Room → upstairs Hallway
hub) → `e e` (east through the hallway chain to the stairwell) → `d` (Downstairs
Hallway) → `n` (Kitchen) → `open refridgerator` → `eat apple` = **+5 (5/5)**.
Verified 3× identical. **The Battle System is enabled but vestigial:** 19
`type=7` (ChangeBattle) actions hang off self-care verbs (`drink root
beer`/`eat turkey`/`eat soup`/`drink coffee`/`turn on treadmill`/`wash
hands`/`fart`/`pet`+`hit dozer`) nudging hidden stamina/strength, but there is no
enemy, no `KilledTask`, and no EndGame, so the stats do nothing observable
(`hit dozer` is the only attack verb and leads nowhere). Same "score, no win"
sandbox class as `lifesimulation`/`The_Town_Of_Azra`, but with one token scoring
task instead of zero. Faithful; no SCARE change, no combat-assist.

## 2026-06-24 session: the newly-added games

The `games/` folder grew to 50 `.taf` since this file was first written; all 32
not-yet-covered games were triaged with the structural dump. (The triage is
finished — every game below is now banked, and the per-game classification lives
in each game's own `*_walkthrough.md`, so the separate triage table was pruned.)
Progress this session (12 new walkthroughs):

- **Wins, verified deterministic:** `Cyber_walkthrough.md` (150/150),
  `cyber2_walkthrough.md` (355/355), `TheCatintheTree_walkthrough.md` (50/50),
  `Colony_walkthrough.md` (200/200), `Jason_Vs_Salm_walkthrough.md` (WIN, honest
  max 0/1000 — scored difficulties are an unwinnable combat-balance bug;
  mechanics re-derived 2026-08-02, see below),
  `donuts_intro_walkthrough.md` (0/0 win intro).
- **Documented unwinnable / sandbox / no-ending:** `Del_Sol_walkthrough.md`
  (orphaned win — Moreland's KilledTask gated behind a stamina-0 NPC; 24/46),
  `QuestI_walkthrough.md`, `IceCream_walkthrough.md`, `Trabula_walkthrough.md`,
  `Invasion_of_the_Second-Hand_Shirts_walkthrough.md`, `adriftorama_walkthrough.md`.
- **Deferred (winnable, route not yet banked):** *(none — all three banked)*.
- **Banked since:** `FunHouse_walkthrough.md` (**WON, max 310/410**),
  `thetest_walkthrough.md` (**UNWINNABLE, max 5/25** — circular first-door lock),
  and `Main_Course_walkthrough.md` (**WON, 0/0** — cat-fur disguise puzzle) — see
  the 2026-06-24 (later) entries below.
- **Banked since:** `Melbourne_Beach_walkthrough.md` (**WON, max 38/41** — see
  the 2026-06-24 (later) entry below).
- **Still untouched:** only **Shadowpeak** (winnable, large) remains among the
  WINNABLE list; **circus** (*Menagerie!*) is winnable but parked at the RNG
  bootstrap (10/140). The Screen Savers, ALEXIS, **Space Boy's First Adventure
  (WON 1184/1374)**, Matt's House are DONE; Les Feux de l'enfer (score, no win)
  and Through time (lose-only) are documented unwinnable.
  **tcom (win, 0-score), SRSintro (0/0 intro) and ALEXIS (WON 23/65) are now
  DONE** — see the 2026-06-26 entries above.
  Bomb Threat (win, 0-score), lair-of-the-cybercow (win 10/10), and **deaths
  (WON 100/100)** are now **DONE** — see the entries below.
- **Banked since:** `WesGHN_walkthrough.md` (**UNWINNABLE, max 30/100** — since
  overturned, **WON 100/100**, see the 2026-08-02 entry at the top) and
  `Melbourne_Beach_walkthrough.md` (**WON, max 38/41**) — see the 2026-06-24
  (later) entries below.

## 2026-06-26: ALEXIS (*Alexis: Dalskee*) — **WON, 23/65** (win verified deterministic)

> **SUPERSEDED (2026-07-13):** the magic-cube routes now bank **55/65** and
> **58/65**, both wired with goldens — see the entry at the top of this file.
> `harness/alexis_solution.txt` is no longer the 101-cmd 23/65 script described
> below; it is the 171-cmd carry-the-cube route.

`ALEXIS_walkthrough.md`; solution `harness/alexis_solution.txt` (101 cmds).
Native **ADRIFT 3.9** (byte8 `0x94`/byte10 `0x37`) fantasy quest w/ Battle
System + a companion NPC (**Serond**) who fights alongside you. **Win = kill
Urgorn** in the Dungeon (r72) inside Uron Castle. **The only mandatory combat is
Urgorn** — every one of the 7 elemental stones just lies in a room, and the
central hub opens by *giving food to Tarin* (+2), not by fighting the
Bridgekeeper. Spine: collect 7 stones (Forecarn start / Tonerith r3 / Dusteron
r5 / Glaven r21 caves / Longmore r42 / Kedarn r27 / Nelone r56) → give all to
**Larnt** at Nelone Bridge (a traitor — opens the bridge to Uron) → `say the
password`+`open door` into the castle → small key (Long-room table) + large key
(bedroom chest, opened w/ small key) → unlock the stone door → **Mirror room:
`north` is the only safe exit** (`east`/`west` are teleport death-traps) →
Torture → Dungeon → `attack urgorn` (Serond assists; legacy 3.9 str-vs-def
combat works, plain small sword + one gourd drink on Easy is enough). Verified
3× identical win. **Two time-sinks worth recording: (1) light the lantern AT
HOME** — task 1 `light *lantern *` (+5) is cottage-scoped, not runnable at the
cave entrance (looked like a glued-`*lantern` parser bug, but SCARE matches it
fine in scope — **faithful, no engine issue**); **(2) the scdump compass labels
are scrambled** (ADRIFT's real dir order ≠ dump slot order) — navigate by the
game's room prose, the dump connectivity is still correct. **Banked 23/65 is the
minimum-combat WIN path**; the other ~42 pts are an optional max-score pass
(Hard +10 but −40 stats; ~8 optional monster kills w/ Serond; turn-ring/water/
dig/marsh-chest side puzzles; touch-ball r66 is a teleport trap; power-ups =
Haron's +200-stamina potion, +20-str juice, leaves, elven armour, the
`dard dard larna dard` buff). Faithful; no SCARE change, no combat-assist.

## 2026-06-26: SRSintro (*Silk Road Secrets*) — **INTRO DEMO, 0/0 no ending**

`SRSintro_walkthrough.md`; tour solution `harness/srsintro_solution.txt`.
*"Silk Road Secrets (Samarkand to Lop Nor)"* by C. Henshaw (ADRIFT 3.9) — you
are Beghram of Tokharia, summoned to Samarkand; the Khan offers the Sword of
Nismus for recovering stolen "Heavenly" beasts from China. **Structural dump of
all 37 tasks: zero `ACT type=6` (EndGame) AND zero `ACT type=4` (ChangeScore) ⇒
no win/lose/death and no score** — it's an introduction/demo only (same class as
IceCream/Invasion/lifesimulation). 3 rooms: Marketplace (start) → `E` Citadel
(the Khan gives the **Jan-wa** sword + cryptic mission) → `NE` Zoroastrian
Shrine (a fire-priest answers `ask priest about beasts/omens/articles/khan/
mission` lore). **Gate:** the Marketplace→Shrine `NE` exit is gated on **task 1
`take the sword from the khan`**, so visit the Khan first. (Shrine exit displays
as `sw` though the table lists SE — rotated labels.) No name/gender prompt
(despite the earlier "F" mention; it boots straight to the intro). Faithful;
no SCARE change, no combat-assist needed. NOT a 3.9→4.0 conversion (native 3.9).

## 2026-06-26: tcom (*The Cave of Morpheus*, Part 1) — **WON, 0/0 (no score)**

`tcom_walkthrough.md`; solution `harness/tcom_solution.txt` (13 cmds). ADRIFT
3.9 anxiety-dream: a nude undergraduate wakes late in Ionesco Hall and must dash
across campus to his 9 AM Western Civ exam while **Death himself gives chase**.
**0/0 — zero ChangeScore (type-4) actions; the win is the max result.** The win
is **task 0 `open wooden door`** (Where=ONE_ROOM = the Great Wooden Door room,
**no restriction**, single `ACT type=6 v1=0`), so any route that reaches that
room and opens the door wins — nothing to collect/wear/solve. Route is one
straight dash: from the dorm `n n d n n n n n` (Dorm→Outside→Top-Stairwell→
Bottom→Back-of-Courtyard→Fountain→Front-of-Courtyard→Foyer→Front-Steps), then
`d` steps down into the street (Death appears, room 13), then `n n n` up the
alley to the door, `open wooden door` = WIN (*"This ends the first part… open
the file entitled 'tcom2'"*). **Death is the Battle System NPC but harmless** —
he prints *"Death hits you"* but has no player-kill damage/KilledTask, so the
chase is pure atmosphere (deterministic, verified 3× identical). Two lose-ends
avoided by the route: `eat pizza` (task 5) and touching the courtyard **grass**
(`* grass`, task 8 = "hand of God squashes you" — stay on the gravel paths,
i.e. just keep going `n`). Optional unscored scenery (fountain/crest/Lester/a
one-way `push`-escape maze) is irrelevant. Faithful to the Runner; no SCARE
change, no combat-assist needed.

## 2026-06-25: deaths (*Death's Door*) — **WON, full 100/100**

`deaths_walkthrough.md`; solution `harness/deaths_solution.txt` (1st two lines =
name `Hero` + gender `male` — both start-up prompts). ADRIFT **3.90** dungeon
crawl ("free the house of death"): break into a 3-storey building, collect four
coloured keys, kill the **Dark force** in the attic. Was on the "untouched/boot
via name+gender prompt" list. **Not a hang — boots fine once name/gender are
fed** (same class as Theannihilationofthink2/CyberCow). Deterministic; **100/100
is the true max** (9 ChangeScore tasks sum exactly to 100, no orphans). Key
mechanics RE'd from the structural dump: **(1) one long key/door chain** — mail
box→silver-key→unlock Red kitchen→`kill jim`→gold-key→unlock Lit dining→`kill
ireen`→red-key→unlock Dark closet→`kill rooth`→crystal-key→unlock Living
room→Debbie/Beth/Ross (+45); **(2) the win stair (Third floor→Attic) opens via
`kill witch`** (task 23, a no-restriction scripted task that runs anywhere on the
Third floor — the witch needn't be present), and **`kill force`** in the Attic is
the type-6 EndGame win. **(3) Real Battle System: most enemies are harmless
(`str−def≤0`) but Rooth one-shots an unarmoured player on room entry** — the
intro's "upgrade your armor" is mandatory: buy `scale male` then `plate male` at
the Third-floor shop (money from the 500-gp kills + `sell silver-key` after using
it); plate male reduces incoming damage to 0, making Rooth AND the Attic boss
safe. The "kill *name*" tasks are scripted (restr=0) so the command itself kills;
the shop's other gear/special-attacks and the Driveway monsters
(Ghost/Wolfe/Cat/Vampire) are unscored flavour. Faithful to the 3.90 Runner; no
SCARE change needed. Verified 3× identical (100/100 + "crumbles into dust" win).

## 2026-06-24 (later): Bomb Threat — **WON, 0/0** (+ scdump event-dump fix)

`Bomb_Threat_walkthrough.md`; solution `harness/bomb_threat_solution.txt`. An
FBI agent (Jack Wayne) follows the bomber's floor-by-floor phone clues through a
skyscraper to find and defuse a bomb in the sewers, then wins the final
shoot-out. **0/0 — zero ChangeScore actions; the win is the max result.** Route:
booth→footpath→lobby→elevator up to 28F (office: `open desk`/`take key`/`unlock
cabinet`/`open portfolio`/`look at piece of paper`; conference: `open
folder`/`take card` — the **security card** is the one genuinely-required item)→
`read magazine` (an unrestricted command-only clue → enables `press 3rd floor`)
→ 3F Tool Room (`take pliers`/`take crowbar`)→ground→Footpath: holding the
crowbar auto-opens the manhole (an every-turn event) → `down` to the Sewers →
`open package` → `cut red wire` (**blue = instant death**) → `shoot edgar`.
**Key mechanic: `shoot edgar` rolls `hit += random(1,3)` — 1/2 = win
(`#hitedgar1/2`), 3 = death (`#hitedgar3`); cutting the red wire arms a 2-turn
`#die2` countdown so you can only fire on the next turn (no re-roll).** The
street "traffic" event draws 2 randoms/turn, so the roll is stream-position
dependent; under seed 1234, one `z` before `cut red wire` lands a headshot
deterministically. The 43rd-floor detour (`press 43rd floor`, gated on `look at
piece of paper`) is **optional** — the "piece of paper" and "magazine" are
command-only clue tasks with no object/location restriction. **Tooling fix:**
`terps/scare/scdump.c`'s event dump used `prop_get_integer` (which `sc_fatal`s
on a missing field) and so aborted on any game whose events omit `Obj2`/`Obj3`
(like this one); converted to the tolerant `prop_get` pattern + added
`Time1`/`Time2`/`PauseTask` (which made the Edgar/timer interplay legible).
Harness-only (gated behind `-DSCARE_DUMP_TOOLS`; Spatterlight never builds it).

## 2026-06-24 (later): Theannihilationofthink2 — **WON 35/35** (+ real engine fix)

`Theannihilationofthink2_walkthrough.md`; solution `harness/think2_solution.txt`.
*The Annihilation of think.com* (ADRIFT **3.90**) — tiny linear satire: log into
think.com from your bedroom and fight up through the site's pages to beat Herald
the dog (`4`=Defend in his office = +15 win). Route: `login to think.com` +5 →
`take paper` → `say icons are banned` (past the Guard) → `put paper on Mrs Mac
Intire` +10 → `out` → `2` (duck Mrs Assface's gun) +5 → `n`,`n`,`u`,`w` →
Herald's office → `4` +15 WIN ("Think.com has been restored…").

**This game was UNWINNABLE in SCARE until a real engine bug was found+fixed:**
SCARE split player input on **any** bare `.`/`,` (`SEPARATORS=".,"`), chopping
the only bedroom exit command `login to think.com` into `login to think`+`com`,
which never matched → sealed first room. The ADRIFT Runner does NOT do this —
RE of the 3.90 Runner input splitter (`run390.txt` @~`0x5EC80`) shows it
normalises on `","`, `". "` (period **+ space**) and `"then"`; a period inside a
word (`think.com`, decimals like `3.5`) is part of the command. **Fix in
`scrunner.c`:** new `run_is_separator()` — comma always splits, period splits
only when followed by whitespace/EOL. Verified `look. look` / `look,look` still
split into two commands while `login to think.com` is one. Engine-level (applies
to Spatterlight, not just the harness).

**Also fixed (user request: "fix the debugger output") + harness robustness in
`os_ansi.c`:** (1) the SCARE debugger spun forever at EOF printing
`[SCARE debug]>` + `run_quit: game is not running` — `os_read_line` now exits
cleanly when `fgets` hits EOF instead of looping (the old `feof→sc_quit_game`
can't quit an already-ended game, e.g. the end-of-game debug dialog). (2) A
real **global-buffer-overflow** in `partial_flush()` (ASAN-confirmed): the
word-wrap `memmove` used `strlen(line_break)+1` (counts the space) while copying
from `line_break+1`, over-reading past the 79-byte `line_buffer` on a full line
— now `strlen(line_break+1)+1`. (3) New opt-in **`SC_SKIP_WAITKEY=1`** makes the
harness ignore `<waitkey>` "press a key" pauses for clean one-line-per-command
derivation; the faithful default still consumes one input line per pause (the
banked solution has 3 blank lines after `2` for the duck text's three waitkeys).

**Triage correction:** the three games filed under "Hangs after Loading…"
(`Theannihilationofthink2`, `deaths`, `lair-of-the-cybercow`) are NOT hangs —
they just block on the name+gender start-up prompts under `</dev/null`. All
three boot and play with `name`/`male` fed; all have a real win ending. `deaths`
(62 tasks) and `lair-of-the-cybercow` (226 tasks) remain to be banked.

## 2026-06-24 (later): Melbourne Beach — **WON, max 38/41**

`Melbourne_Beach_walkthrough.md`; solution `harness/melbourne_beach_solution.txt`.
*Melbourne Beach* v1.0f by David D. Good (2001), ADRIFT 3.90 — a domestic
slice-of-life game (no Battle System): an overnight guest does helpful morning
chores around David & Judy's beach house, then drives to the beach; the win is
`use shower` to rinse sandy feet (type-6 EndGame). Deterministic. Key RE'd
mechanics: **`oil trumpet` (+5) secretly drops the hidden leather purse (car
keys) into the Eating area** (the only way the keys appear); **"music" = the
folder** from the car, given to Judy after the trumpet (`give music` needs
`give trumpet` done); the **shower needs sandy feet** (set by walking east onto
the Beach), and **volleyball is a deterministic skill counter** (variable[8]
+=1 per play; the 7th play scores +2). Two wandering NPCs (Judy, David) whose
walks consume the shared RNG — met by **repeating** the give/play command until
present (robust under determinism). **Max 38/41; the 3 lost points are all
faithful game data:** (1) `wash clothes` is non-repeatable so the single wet
batch makes **turn-on-dryer +1 mutually exclusive with fold +5** (take fold);
(2) **red-cup coffee #2 (+1) is a logical contradiction** — the red cup is only
revealed by `give coffee to the Captain`, which needs `drink oil` DONE, but the
scoring drink needs `drink oil` NOT done; (3) **red-cup coffee #3 (+1) is gated
behind `drink oil` (−1)** = net 0. Verified 3× identical. **Tooling note:** this
session the `SC_DUMP_TASKS` instrumentation was finally factored into a
committed, opt-in module — `terps/scare/scdump.c` (built only into the harness
via `-DSCARE_DUMP_TOOLS`; one-line `#ifdef`-guarded hooks in sctasks.c /
scnpcs.c) — so it no longer needs re-deriving each session. It adds
`SC_TRACE_JUDY` (per-turn NPC-room trace) and `SC_TRACE_TASKS` (built-in
task/restr trace) alongside the structural dump (tasks/restrictions/actions +
container table + event table, with object/task names resolved). The Spatterlight
build never compiles it.

## 2026-06-24 (later): WesGHN (Wes Garden's Halting Nightmare) — **UNWINNABLE, max 30/100**

> **SUPERSEDED (2026-08-02): WRONG — the game is WINNABLE, 100/100.** The
> "orphaned gold ring" below rests on a misread of the raw event dump: event 1
> [Davidshand] (started by `ring bell`) drops the severed hand + ring into the
> Waiting Room. See the 2026-08-02 entry at the top of this file; the
> walkthrough and the solution/golden pair are re-derived and re-blessed.

`WesGHN_walkthrough.md`; solution `harness/wes_ghn_solution.txt`. *Wes Garden's
Halting Nightmare* by Jubell (ADRIFT Spring Thing 2010) — a 3.5 MB graphics file
but a tiny 10-room / 25-task / 3-NPC game. A surreal Mercy-Hospital nightmare:
summon the **Soul Scythe**, kill the candy-striper **Hope**, box her spirit.
**Triaged "winnable (1 win-ending)"; full RE proves it is NOT.** The win
(`Put Hope's spirit into box`, task 24) and the *entire* back half (Radiology →
eyeball → Medicine Cabinet → Ward → spirit) are fully built and verified to work
when force-injected, but they are all sealed behind **one orphaned key object**:
the **gold ring** needed to solve the first gate, the **Lovers' Fountain**
(`Give ring and candle to fountain`, +20), sits **on a `severed hand` that no
task, event, or NPC character-walk ever un-hides** (the hand is `OBJ_HIDDEN`,
parented to nothing; the only two actions touching the hand/ring *hide* them).
Fountain unsolvable ⇒ `#OpenRadiology` (its only non-circular opener) never fires
⇒ everything downstream unreachable. **Constructively confirmed the ring is the
SOLE break**: with it injected, fountain +20 → drops the **vial** (= the Medicine
Cabinet door's key) → Radiology → `slash painting` +5 (eyeball, the Grand
Corridor *optical scanner*'s key) → Medicine Cabinet `talk to charity` +10 →
unlock with vial → Ward, all work. Faithful to the Runner (same `.taf` data, no
reveal action — same class as Spirit's Flight's orphaned Ice Totem). **Max
reachable 30** = talk Charity +10, closely-examine-figure +5 (also one-ways the
Foyer shut), drink water +5, kill Hope +10. **Combat correctly configured** (no
assist): summon the scythe → acc 50 > Hope's agi 30, she dies in ~6 hits, player
has 40 stamina. **Footgun:** the attack verb is `attack hope`, NOT `attack hope
with scythe` (grammar rejects the latter), and the scythe must be summoned first.
**Tooling note:** used the reusable `SC_DUMP_TASKS` block in `sctasks.c` (extended
with an object-position/door-`Key` dump + a one-shot `SC_GIVE_RING` injector to
prove the chain) plus a temporary stamina/accuracy trace in `scbattle.c`; both
files `git checkout`'d afterward — tree clean.

## 2026-06-24 (later): light_up_4summer_comp — **WON 73/75**

`Light_Up_walkthrough.md`; solution `harness/light_up_solution.txt`. *Light Up:
An Interactive Horror* by TDS — a full 5-chapter game (House → suffocation Field
maze → six Village "trials" → a Death combat gauntlet → Waste Land / Arkot ending,
marker **"THE END / Congratulations!"**), deterministic. **The Battle System here
is correctly configured (non-zero accuracy), so combat actually works — no
combat-assist needed** (contrast Azra/V&K/Mr-Smith/To-Hell). Notable mechanics
reverse-engineered: Ch2 bird/medallion needs the woman's walk `CharTask` to fire
`#encounter` when she re-meets the player (give her the medallion, then chase her
down); the medallion must be **worn** (not held) to open the gate. Ch3 trials:
*close the blue box* to smother the bomb, *take the child then blanket it*, nest
the 7 orbs by visibility then `shout`, kill Chip with the lighter. Ch4: kill 10
Ozgat/Riven/Higher across rooms 24–32 (the NPCs also fight each other) → +15 →
Waste Land. **Max reachable 75** = 60 story + `hard` +15; banked **73** omits the
licence-plate laptop password +2 (910-CCC) — that puzzle works, but its 2 extra
upstream turns reshuffle the shared `erkyrath_random` stream and get the
`hard`-weakened player killed in the gauntlet, so the fixed turn-list can't bank
both at once (a human with live combat feedback can). **Tooling note:** a
reusable `SC_DUMP_TASKS` structural dump (tasks + restrictions/actions + room
exits + events + NPC walks/CharTask + battle stats) was added to `sctasks.c` to
RE this, then **fully removed** (tree clean; the committed move-assist change was
left intact). `git checkout` is NOT needed — instrumentation already stripped.

**Tooling note:** the reusable `SC_DUMP_TASKS` structural-dump block (+ per-NPC
battle-stat line) is currently live in `terps/scare/sctasks.c` (uncommitted) so
the remaining games can be triaged/derived; `git checkout sctasks.c` when the
batch is finished. Rebuild with `sh harness/build.sh`.

## 2026-06-24 (later): FunHouse — **WON 310/410**

`FunHouse_walkthrough.md`; solution `harness/funhouse_solution.txt`. A tiny
carnival game with **zero restrictions on any of its 31 tasks** — gated purely by
*which room* each task runs in. The plot: a hidden cassette (inside the **kewbie
doll** in the Dark Room) must be delivered to the **ticket man** at the booth.
`take kewbie doll` (task 17) carries the move-object action that drops dynamic
object 11 = the cassette into the room; `give ticket man cassette` (task 24) is
the type-6 win (var1=0). The "scrambled mirror maze" was mapped with a small
**Python BFS driver** that replays full paths from start through the headless
`scare` and parses the room name + "you can move…" line (`/tmp/funbfs*.py`) —
robust against any path-history dependence since every probe replays from turn 0.
Route: booth (`take hundred dollars` +100, `pick up money` +100) → Hall→Strobe→
Vampire→Dark Room→Fun slide (`take ring` +110) → back to Dark Room (`take doll`,
`take cassette`) → return to booth → `give ticket man cassette`. **Max reachable
310/410:** the stored max is 410 but the missing +100 sits in two ChangeScore
tasks (`take money`, `take drink`) whose `Where` is **NO_ROOMS** and which **no
task ever executes** (zero type-5 actions in the game) — orphaned, unreachable,
faithful to the data. Combat present (Brat Kid/Bumpy/mafia man) but no fight is
entered (their swings miss the player under seed 1234), so no assist is needed.
**Tooling note:** re-added the `SC_DUMP_TASKS` block to `sctasks.c`, used it,
then `git checkout terps/scare/sctasks.c` — tree is clean.

## 2026-06-24 (later): thetest — **UNWINNABLE, max 5/25**

`thetest_walkthrough.md`; solution `harness/thetest_solution.txt`. A whimsical
ADRIFT 4 puzzle-box: Gordon is flung by a time-lord teacher into Room 0
("Somewhere you don't want to be"), meant to traverse Room 1 (phone + Robot
Guard with a key) → Room 2 (teleporter) → Room 3 (Morse slot) → Room 4 (`use key
with keyhole` = the win, task 28). **Triaged as winnable; full RE proves it is
NOT.** The very first exit, Room 0 → Room 1, is gated (room-exit table) on task
15 `#unlockdoor` being complete; `#unlockdoor` needs **`robot2 == 3`** (var 6);
`robot2` is written by **exactly one** action in the game — task 16
`#shoutrobots` — whose restriction requires the player to be **in the same room
as the Robot Guard**, who sits in Room 1 and never moves. So the door needs an
action only possible *beyond* the door: a closed loop. EVENT 2 calls
`#shoutrobots` on a timer but `evt_run` only runs the affected task if its
room-check passes (no restriction bypass), so it never fires in Room 0. A 45-verb
× 25-rep brute force of Room 0 finds no escape. **The colour-key/colour-door
minigame is a deliberate red herring:** `unlock door` (task 14) keeps a
consecutive-match counter `addything` (var 4) that **no task/event/exit ever
reads** — inserting the key only recolours the door, never opens it. Max reachable
= **5/25**, the one cut-scene `#finalfluff` (task 3, the fluff-allergic machine
breaking) — `listen` +5 is itself gated on an uncompleteable task, and the other
+5s live in the sealed Rooms 1–4. Faithful to the data and the Runner (standard
ADRIFT exit/variable/character restrictions, evaluated identically). **Tooling:**
extended the `SC_DUMP_TASKS` block (added Variables + room-exit + Events dumps),
used it, then `git checkout terps/scare/sctasks.c` — tree clean.

## 2026-06-24 (later): Main Course — **WON (0/0, no score)**

`Main_Course_walkthrough.md`; solution `harness/maincourse_solution.txt`. By
quantumsheep (2008). You are a SoMorph (shape-shifting alien) that "appears as
its most recent prey." 5 rooms. **No score (zero ChangeScore actions); the lone
ending is the win** (task 8 `* course *` in the Command Deck, type-6 EndGame,
gated solely on task 0 `eat human`). The puzzle chain, fully RE'd: eat the cat
(1-hit kill → drops cat fur) → bathroom: `open door`/`east`/`close door` (privacy)
/`open loo`/`use toilet` (digest, unlocks wearing the fur) → `wear cat fur`
(disguise) → `push button` ("Premature Ejection during Hyperspace" opens the Cryo
Tube and wakes the frozen pilot Alan Davies as a fleeing NPC) → cryo room: the
disguise stops him fleeing so `attack human` lands (1-hit kill) → `eat human`
(now you appear human) → `remove cat fur` (so FRANK the computer sees a human,
not a cat) → Command Deck → `main course` = WIN ("…on your way home with just a
little indigestion!"). Combat is faithful/deterministic (both kills are 1 hit);
catnip + wandering-cat lines are decoys. **Tooling:** extended `SC_DUMP_TASKS`
(Variables, room exits, Events, object Openable/aliases), used it, then
`git checkout terps/scare/sctasks.c` — tree clean.

## 2026-06-25 TODO: 4.0 conversion-damage deep-dive (untested 4.0 games)

> **PARKED 2026-06-25.** Progress banked below: Shadowpeak side-check RESOLVED
> (native-4.0 MeetObject fixup correct); Space Boy's triaged + scdump off-by-one
> fix; Through time = probable-but-unconfirmed No-Rooms SCARE divergence (a
> `Sub_20_74` decode plan was drawn up). Resume next with Les Feux de l'enfer
> (the last untouched 4.0 candidate) and/or executing that decode plan.
> *(Since superseded — see the RESOLVED verdict further down: SCARE is faithful.)*

**Question to answer:** are any of the *untested* 4.0 games unwinnable because
their authors converted them from 3.9 in the ADRIFT 4.0 Generator and the
conversion broke their tasks — and if so, is it **faithful data damage** (the
real ADRIFT 4.0 Runner fails too → document as unwinnable, do NOT "fix") or a
**SCARE interpreter divergence** on a converted 4.0 file (→ real engine fix, the
class of the CyberCow `MeetObject`/event bugs committed in `ff6d0567`)?

**What we already know (scan over all 50 `games/`):**
- Version split by header byte 8 (`0x93`=4.0, `0x94`=3.x): **27 are 4.0, 23 are
  3.9, 0 are 3.8.**
- **No 4.0 game has any out-of-range task/event reference** (no type-2 restriction
  or event `affTask`/`TaskNum` past the task table). So there is **no gross
  table-shift corruption** from a botched conversion — any damage would be
  subtle (off-by-one within range, or a wrong field meaning), not wholesale.
- Only 3 of the 27 use a walk **ObjectTask**: **FunHouse** and **Sun_Empire**
  (both already WON with the `MeetObject` dynamic→global fix in place) and
  **Shadowpeak** (untested). So SCARE's `>3.8` converted-walk path is sound where
  tested.

**Candidates (4.0, no checked-in walkthrough — *untested*, not proven
unwinnable):** Shadowpeak (574 tasks, 43 endings — biggest), Space Boy's First
Adventure (78), Through time (164, lose-only?), Les Feux de l'enfer (289,
French), Invasion of the Second-Hand Shirts (19), Trabula (4 — stub), SRSintro /
adriftorama (intros). **Start with Shadowpeak and Space Boy's** (substantial,
plausibly winnable).

**Method (per game, on top of the standard per-game workflow below):**
1. Derive a route with the headless harness as usual. If it dead-ends, classify
   the block, then check it against the **3.9↔4.0 schema divergences** that a
   conversion can expose:
   - **Walk `MeetObject`** — 4.0 reads it raw; SCARE converts dynamic→global for
     all `>3.8` (`npc_walk_meetobject_needs_fixup`). Confirm the converted index
     lands on a *sensible* object that actually appears in the walk's rooms. **If
     a native-4.0 game stores `MeetObject` as a global index, this fix would
     MIS-convert it** — then the gate must become 3.9-only. (FunHouse/Sun_Empire
     say dynamic is right for them; verify on Shadowpeak's four ObjectTask
     walks — now done, see the 2026-06-25 progress note below.)
   - **Walk `MeetChar`** — 4.0 reads `#MeetChar`, 3.9 has `ZMeetChar` (absent /
     defaulted 0 = "meet player"). A converted game may carry a stray/garbage
     `MeetChar`; check whether a CharTask is meeting the wrong character.
   - **Action `Type` renumbering** — the V390 schema applies `Type>4?#Type++`;
     V400 does not. A file the Generator converted but did not renumber would
     have 3.9-numbered actions read with 4.0 meanings (e.g. ChangeScore vs
     exec/unset vs EndGame mismatch). Symptom: a task whose actions don't do what
     its prose says.
   - **Event `TaskFinished`** (set-incomplete) — now clears the done flag
     directly (`scevents.c`, `ff6d0567`); confirm any "reset" event behaves.
2. **Decide data-damage vs SCARE divergence.** Reproduce the exact task/event in
   the **ADRIFT Runner P-code** (`~/Desktop/run400.txt` for 4.0,
   `~/Desktop/run390.txt` for 3.9; `grep -a`, `LC_ALL=C`). If SCARE matches the
   Runner, the breakage is the *author's converted data* → document the game as
   unwinnable with the evidence, do not patch. Only patch if SCARE's evaluation
   actually differs from the Runner.
3. **Deliverables:** `<Game>_walkthrough.md` (header + full command list +
   annotated phases + honest unreachable-point note) and
   `harness/<game>_solution.txt`; if a real divergence is found, an engine fix +
   note that the bundled-walkthrough corpus stays byte-identical
   (`diff -rq` two corpus runs, as in the CyberCow work) and
   `make -f Makefile.headless test` is green.

**Side check while here:** re-confirm `npc_walk_meetobject_needs_fixup` (`>380`,
i.e. 3.9 **and** 4.0) is correct for *native* 4.0 — the inverse of the CyberCow
bug. Evidence so far (FunHouse/Sun_Empire win) says yes; nail it on Shadowpeak.

### 2026-06-25 progress: Shadowpeak triage — side-check **RESOLVED** (native-4.0 MeetObject fixup is correct)

Started the deep-dive on **Shadowpeak** (the biggest candidate). Triage from the
`SC_DUMP_TASKS`/object/exit dump:
- **Native 4.0** (header byte8 `0x93`/byte10 `0x3e`), no name/gender prompt
  (you are *Loralang*). **574 tasks, 194 objects, 50 NPCs, 157 events, ~125 used
  rooms** (multi-realm, portal-connected). **Max score 790** across 69 ChangeScore
  tasks; **exactly 1 win** (`ACT type=6 v1=0`) = **task 417 `blow * horn`** (where=3,
  restr=3), **1 lose**, **41 death endings**. Full max-score route is a genuine
  **multi-session** job (left as the open next step).
- **MeetObject side-check — decisively confirmed correct for native 4.0.** Shadowpeak
  has **4** ObjectTask walks (not 3); scdump prints the **raw** stored MeetObject.
  Runtime does `meetobject = stored−1`, then `obj_dynamic_object()` for any
  version `>380`. The clincher is **NPC 45 Reevling**, whose ObjectTask (**task 549**)
  is *self-documenting*: its command is `////when Reevling sees sword//`. Its
  MeetObject raw=7 → `−1`=6 → `obj_dynamic_object(6)` = **global obj 10 "sword"** ✓.
  Read **raw-as-global** it would be obj 7 **"nest"** ✗ — i.e. without the fixup the
  "sees sword" task would silently check the wrong object and never fire. Corroborated
  by **NPC 24 Melvin** (objTask 422): raw 78 → dyn#77 → **obj 147 "The horn of the
  angels"** (the *win item* — `blow horn`), vs raw-as-global obj 78 "table". (Berto
  objTask 550 → sword too; Cerberus objTask 510 → a hidden-utility object, the one
  ambiguous case, but it's a death-ending tail task.) **Conclusion:** the CyberCow
  `>380` dynamic→global conversion is **right for native 4.0**, not just 3.9 — no
  3.9-only narrowing needed. The deep-dive's central "did conversion break a walk"
  worry does **not** apply to Shadowpeak's walks.
- **Open next step:** bank the full Shadowpeak route to the `blow horn` win (790
  max). Structure dumped to scratch; no engine change needed so far.

### 2026-06-25 progress: Space Boy's First Adventure — triage + **scdump off-by-one fix**

Triaged the 2nd deep-dive candidate. **Native 4.0** (the banner says *"created
using ADRIFT Generator 4.0"*, ver 2.0, 2005, David Parish) — i.e. **not a 3.9→4.0
conversion**, so the conversion-damage hypothesis simply does not apply; it's just
an untested, winnable game needing a normal (large) derivation. **78 tasks, 73
rooms, 1 NPC (Evil Man), 1 event, 3 death endings, 1 win** = **task 46 `read
scribbled note`** in room 65 (Space Boy's Secret Hide-Out, *no* restriction).
Score-task sum 1319 (many repeatable; true max TBD). **Win dependency chain
(decoded):** the only way into the endgame is room 0 (Living Room) →W→ room 7
(Space Boy's Room, Evil Man) →W→ room 65 (note). Room 0→7 is `gateTask=51` =
**task 51 `open room door`**, which needs the Room Door in the state set by **task
44 `unlock door`** (needs the **Room Key** held). The Room Key is **task 57 `take
key`** in room 71 (*Under the rock*), reachable only via **task 43 `move huge
rock`** in room 4 (Backyard Garden) — which requires the **Strength Belt** worn.
So the spine is *Strength Belt → move rock → Room Key → unlock+open door → room 7
→ room 65 note = WIN*, on top of the four power-item puzzles (Flight Boots / Ice
Gloves / Heat Goggles / Strength Belt mimic the lost cape powers) and several
letter-mazes (LAVAaH islands rooms 18–25; "TO THE GARAGE" / B–Z letter rooms
29–58). Author left debug cheats (`gimme gimme gimme` = all 4 items, `shout spade`
= the ion bridge) — avoid for a legit scored route. **Open next step:** bank the
full route (multi-iteration; map the power-item gating + letter mazes).

**Real harness fix found & applied (`scdump.c`):** decoding the door chain
surfaced a genuine off-by-one in the dump's **object-status (type-2) ACTION**
labels. ADRIFT indexes the stateful-object list **0-based for object-status
actions** (`sctasks.c task_run_change_object_status` → `obj_stateful_object
(var1)`) but **1-based for the matching type-1 restriction** (`screstrs.c` →
`obj_stateful_object(var1-1)`, with 0 = "the referenced object"). This asymmetry
is **original upstream SCARE and correct** (matches the Runner; the whole corpus
depends on it) — but scdump applied the restriction's `-1` to *both*, so every
type-2 action object was mislabelled by one (it claimed Space Boy's `unlock door`
set the *Rock*, and `open window` set the *Room Door* — both wrong; really
*Room Door* and *Office Window*). Fixed by passing `v1+1` for type-2 actions so
the resolver's `-1` cancels. Harness-only (Spatterlight never builds scdump); the
projectile-combat regression golden is unaffected (no dump output in it).
**Lesson for future ADRIFT analysis: trust this corrected labelling — a type-2
action's Var1 is one *less* than the restriction Var1 that checks the same
object.**

### 2026-06-25 (later): Space Boy's First Adventure — route banking, **PARKED 275/1374**

Picked up the triage above and began banking the real route. `Space_Boy_walkthrough.md`
+ `harness/space_boy_solution.txt` (deterministic; re-verify with `play.sh`).
**Done & verified: opening → flight hub (room 11, `fly north/south/east` to the
three regions) → full Castle (Ice-Gloves tile/safe puzzle, +the orange/bluish
shrink bottles) → full Volcano (Fire God statue: shout `ell/aay/vee/aay aay/ach`
to open the 5 letter-doors for feet/legs/chest/arms/head → assemble → put on base
→ `freeze wall` → Treasure Island **Heat Goggles**). 3 of 4 power items worn.**
Gotchas banked: can't fly off the magma island (leave room 18 via plain `north`);
side-room returns are the *opposite* compass dir; the dump's compass labels don't
match the display — navigate by the game's own exit hints. **Resume at the East
region (room 26, hub `fly east`)** for the Strength Belt + Transporter maze +
Phased Ion Bridge, then the Room-Key/Evil-Man endgame (spine already decoded above).

**Resolved fidelity question (not a bug):** the Ice-Gloves +30 (task 11) never
fires because its command is `{take\get}` with a **backslash** (lone author typo;
the 8 other take/get tasks use the correct `{take/get}`). ADRIFT command syntax
splits alternatives only on `/`, so `{take\get}` is a dead single-alternative
("take\get") in **both** SCARE (`scparser.c` `TOK_ALTERNATES_SEPARATOR` = `/`) and
the decompiled real Runner (`NewParse.bas`; chr(92) never special in any `.bas`).
So `take gloves` falls through to the library take with no score in the original
too — **SCARE is faithful; the +30 is lost for everyone, true max ≤ 1344.**

### 2026-06-25 progress: Through time — **probable (UNCONFIRMED) SCARE "No Rooms" divergence**

Triaged the 3rd deep-dive candidate (*Through time*, native 4.0, a 1954-Texas
farmer-abducted-by-aliens / Rome time-travel comedy by ?). **0/0 no score, NO
win-flagged ending** (1 lose = `talk to guards`; 1 death = `*Busted bladder*`
timer; 8 silent-stop `type-6 var=3` endings). Confirms the old "lose-only?" flag.
**But the real story: 135 of its 164 tasks (82%) have `Where = No Rooms`
(`ROOMLIST_NO_ROOMS`, type 0)** — and they are the actual gameplay (movement
`go east/south`, `use toilet`, `talk to alien`, `swipe card`, …). The game has
only **2 hard room-exits**; everything past the opening house is reached via
these No-Rooms tasks.

**In SCARE the game is unplayable past the porch:** `sctasks.c
task_can_run_task_directional` returns `FALSE` for `ROOMLIST_NO_ROOMS`
unconditionally, so a player `south` on the porch prints "You can't go in any
direction!" — the No-Rooms movement tasks never fire (verified). Parsing is
sound (SCARE reads 4.0 correctly — Space Boy's & Shadowpeak have **zero**
where=0 tasks and play fine; the positional parse is aligned, restrictions/actions
decode coherently), so the file genuinely stores "No Rooms".

**Why this is the deep-dive's most interesting hit — a *probable* real divergence:**
the ADRIFT Runner's task-runnable gate is **`mdlSpreadTheLoad.Sub_20_74`**
(run400.txt; called from the dispatcher `Sub_20_12` → predicate `Sub_20_64`,
then executor `Sub_20_11`). Unlike SCARE's unconditional reject, Sub_20_74 has a
**conditional** path for the where-type-0 case (returns True under a condition on
the task's Where sub-record), i.e. the Runner does *not* hard-block No-Rooms
tasks. A released comp game with 82% No-Rooms tasks was presumably playable in
the real Runner, which also points to SCARE diverging.

**But it is NOT a one-line fix, and I did NOT change the engine.** A naive
experiment (make `NO_ROOMS` return `TRUE`, i.e. treat as ALL_ROOMS; reverted,
tree clean) makes `south` fire the **wrong** task — task 91 (`south`, *no*
restriction) intercepts everywhere, printing spaceship-corridor text on the
porch. The author *did* sequence many No-Rooms tasks with task-state restrictions
(e.g. task 55 `south` requires task 54 `say through adversity to the stars`), but
others (task 91) have none, so a blanket enable is incoherent. The Runner must be
applying a **location-specific** condition (the Sub_20_74 conditional) that
SCARE lacks — its exact form (the VB6 task `Where` record layout / param
semantics) was not fully decoded.

**Verdict (2026-06-25, RESOLVED — faithful, unplayable-by-design; do NOT patch).**
The `Sub_20_74` deep-dive settled this. Three independent lines of evidence
converge on *SCARE is faithful*; the earlier "probable divergence" rested on a
misread of the Runner.

1. **The `Sub_20_74` premise was wrong.** That routine is **not** the task
   room-gate. Re-RE of `run400.txt` shows it is a command-**reference / exit
   scope filter**: it switches on a reference-type (0/1/2 with sub-types 0–5,
   *not* the 0–4 `ROOMLIST_*` enum), indexes the object/character arrays, tests
   accessibility via the ubiquitous `General.Sub_22_54`, and uses the
   `0x9C (156)` "nowhere" location sentinel. It is called only from the
   string/pattern builders (`Sub_20_64`, `Sub_20_75`), whose results feed
   command matching — never a player/room runnability decision. The "conditional
   where-type-0 path" earlier read as a No-Rooms exception is just the
   *reference-type-0* branch. The Runner's task-match path
   (`Sub_20_12` → executor `Sub_20_11`, the **only** caller of `Sub_20_11`)
   carries no special No-Rooms enablement.

2. **Structural: Through time's working tasks are indistinguishable from
   blocked subroutine tasks elsewhere.** Dumped the full structure
   (`SC_DUMP_TASKS`): 135 No-Rooms / 16 One-Room / 2 Some-Rooms / 11 All-Rooms,
   **3 events, and zero execute-task chaining** (every `ACT type=6` is an
   end-game: lose / death / silent-stop `v1=1/2/3` — matching the known
   endings). So the homebrew "every map node is a No-Rooms task gated by
   task-state restrictions + FinishText" navigation can only function if
   No-Rooms tasks are **directly player-runnable**. But its movement tasks
   (e.g. task 89 `north` gated on task 104; task 55 `south` gated on task 54)
   are *structurally identical* — No-Rooms + restriction + action/FinishText —
   to **Melbourne Beach's** No-Rooms subroutine tasks (task 60 `get* dry*`,
   task 61 `get* fold*`). No predicate distinguishes "should run" from "should
   not run".

3. **Empirical corpus regression proves No-Rooms-blocked is required.** Flipping
   `ROOMLIST_NO_ROOMS → TRUE` (the only change that could make Through time move)
   and replaying the bundled solutions: FunHouse, To_Hell_And_Beyond (×2),
   Sun_Empire stay byte-identical, **but Melbourne Beach diverges** — No-Rooms
   task 60 hijacks `get dry clothes` (`"You now have the dry clothes."` instead
   of the author's dryer-specific `"You take the dry clothes from the dryer."`).
   Reverted; tree clean; Melbourne Beach byte-identical again. This is exactly
   the corpus breakage the warning predicted, and it is direct evidence that the
   real Runner (and standard ADRIFT) **blocks** No-Rooms tasks from direct player
   execution — the ADRIFT idiom for "subroutine task, call via Execute-Task /
   event only."

4. **CONFIRMED on the real Win32 `run400.exe` Runner (ground truth, 2026-06-25).**
   At the very first prompt (living room — real exits south/west only), the
   No-Rooms probes all returned the *faithful* responses: bare directions →
   *"You can only move south."* (No-Rooms directional tasks 27/30/97/99 did not
   fire); `say through adversity to the stars` → ADRIFT's generic library reply
   *"That's the most interesting thing I've ever heard!"* (No-Rooms task 54 did
   not fire); `examine outhouse` → *"You see no such thing."* (No-Rooms task 26
   did not fire). The real Runner blocks No-Rooms tasks from direct player
   execution — identical to SCARE. (Had it diverged, those probes would have
   printed the spaceship/Rome node text, as SCARE does when `NO_ROOMS` is
   force-flipped to `TRUE`.)

**Conclusion.** `Through time`'s author built the entire post-house game out of
No-Rooms tasks with no Execute-Task callers, so those tasks are unreachable in
the real ADRIFT 4 Runner too. The game is **unplayable past the opening house by
design (an authoring error), not a SCARE divergence.** SCARE's unconditional
`NO_ROOMS → FALSE` is **faithful** and must stay. (The Win32 `run400.exe`
ground-truth shortcut was moot anyway: only the disassembly `run400.txt` is on
disk — the `.exe` itself is not present, and the host is Apple-Silicon with no
Wine.) The `Sub_20_74` decode TODO is closed and pruned; its verdict is the one
recorded above.

## Combat-assist note (opt-in, committed)

Several Battle-System games here ship with every character's Accuracy/Agility
left at 0, which (since ADRIFT gates hits on `accuracy > agility`) silently
disables combat the author intended — see the per-game walkthroughs. SCARE now
has an opt-in `sc_set_combat_assist` (harness: `SC_ASSUME_COMBAT=1`, committed
`61e04e0f`) that auto-lands hits *only* in such fully-unconfigured games, so
combat plays out on strength-vs-defence as intended (faithful default is off;
configured games like Sun Empire are unaffected). It applies to **4.0 files
only**: **Azra** combat goals 1/2/6 become reachable, and **To Hell & Beyond**
becomes winnable in principle (full route not yet banked).

**Two games were wrongly filed here and have been removed (2026-08-02).**
`xxd -l 24` says both are **V390** (`c2 cf 94 45 37 61`), which puts them on
the `battle_legacy` path — no accuracy gate at all — so the assist was never
relevant and both were understated:

- **Mr. Smith** is a plain **WIN 90/100**. The note that once stood here
  ("Fernelli's defence 25 > best accessible weapon 20") mis-read the Colt .45's
  effective strength as 20 rather than 10+20=30.
- **Villains & Kings** is **31/37 unassisted** (was "13/37 faithful, 30/37
  assisted"). One sword stroke kills the assassin. The assisted corpus row and
  `harness/villains_and_kings_assisted_solution{,.expected}.txt` are retired;
  `SCR_ASSUME_COMBAT=1` now appears on the To-Hell-And-Beyond row only.

**Always check the TAF signature before diagnosing a combat deadlock.**

## Remaining work (what's left to do)

Done so far (12): Sun_Empire, Orient_Express, Nonsense_Machine, Town_of_Azra,
Villains_And_Kings, To_Hell_And_Beyond (analysis), Mr_Smith, lifesimulation,
The_Spirits_Flight, inverness, **SecretOfLostWorld (WIN 3300/3300)**,
**Toxically_Earth (WIN; 0/0 multi-ending)**, **gateway (WIN 30/30)**,
**Phoenix_Destiny (unwinnable 0/0 beta)**, **hyper_b_s (WIN 100/100)**,
**Shadow_Of_The_Past (WIN 90/100)**, **X-Files (WIN 299/299)**.
**Untouched: none — every game on the original list is done.** The last
optional follow-up — banking the full assisted To_Hell_And_Beyond route —
closed 2026-07-14 (248/373, wired; see the entry at the top of this file).

## Player name/gender start-up prompts (real SCARE fixes, committed)

ADRIFT shows two optional start-up prompts that **SCARE parsed but never
honoured**, both now implemented (commits `2e74a6e6` gender, `8d9c9426` name):

- **Gender** — gender is stored Male/Female/**Unknown**; when Unknown the Runner
  shows *"Please choose player gender"* (Runner `Form8`) and records the answer,
  which restrictions test ("player is male/female"). Unimplemented ⇒ an
  Unknown-gender game gating on gender is unwinnable (both lock-task variants
  fail forever). `run_prompt_player_gender()` in `scrunner.c` (asked at start,
  only when Unknown) + `prop_put_integer()` in `scprops.c`. This made
  **SecretOfLostWorld** winnable.
- **Name** — the `PromptName` global makes the Runner ask *"Please enter your
  name:"* (blank ⇒ "Anonymous") and use it for `%player%`. `run_prompt_player_
  name()` (asked just before gender, matching Runner order) + `prop_put_string()`.

A third corpus-robustness fix (commit `a3426bb3`, surfaced by the boot-all
regression): **empty room groups no longer abort.** Some games define a room
group but assign no rooms to it, then point an NPC walk / object / player move
at it; `lib_random_roomgroup_member()` `sc_fatal`'d on the zero count.
`Through time.taf` (two empty groups + NPCs walking into them) crashed at
startup and was unplayable. Now the function returns −1 and every caller leaves
the mover in place (like the Runner). All 49 corpus games boot (was 48).

Both prompt answers store in the session-persistent property bundle (survives
save/undo; a fresh load re-asks — like the Runner). No save-format change;
games not setting the option get no prompt. **Footgun:** adding these shifted
the input stream for prompting games, so several existing harness solutions
gained a leading name (and gender) answer line — `harness/*_solution.txt` for
name-prompting games (lifesimulation, Mr_Smith, Spirits_Flight, Town_of_Azra,
Villains_And_Kings, SecretOfLostWorld) now begin with `Hero` (and `male`/
`female`). Documented maxima re-verified unchanged after the prelude lines.

### A. Finish the in-progress / large ones
- [x] **The_X-Files_A_New_Beginning** — **WON, full game verified end-to-end at
      299/299 (100%), deterministic.** Walkthrough
      `The_X-Files_A_New_Beginning_walkthrough.md`; solution
      `harness/xfiles_solution.txt`. Linear conversation story, no combat. Act 1
      (DC: office→Cancerman case file→warehouse key→Doggett knock→LGM camera→call
      Ruth) + an Arlington home detour (tavern Jukebox +1, apartment Feed Fish +1
      — both **DC-only, missable once you take the van**) + 3 office "gag" +1s
      (Clean, Burn, **Strip** then re-`wear holster`) + **Donate +10** (the FBI-HQ
      charity Volunteer). Act 2 = the van west: **open the wrapped gift to get the
      gun first** (the van refuses you without it), then `get in the van` (+25).
      Act 3 Bellefleur: motel-room Touch Labtop +1 / Sleep +1 / **the phone-book
      `Look up` +1** (see below), Dean's Diner `buzzer` +10, Arroway Hardware
      `take directions` +25, `get in the van` (+25 *Van* + +10 arrive-forest),
      `look`, `enter the forest` +25, `ko`, **`the end`** = the scripted WIN
      ("Welcome to the Resistance"). Avoid the gag bad-ends (`*End Game` −500,
      Kill Cancerman/Car/Langly, Suicide). **The last +1, `Look up *%character%*`
      (phone book in room 21), had me briefly mis-diagnose a "SCARE wildcard bug"
      — WRONG: SCARE matches the pattern fine; the task just has two restrictions:
      R0 = HOLD the phone book, R1 = it must be OPEN. So `take phone book` →
      `open phone book` → `look up byers` scores it (early tries only opened it,
      not held it → R0 failed → fell through to the library "look up" = "Have
      X-Ray vision"). No engine obstacle, no code change — full 299/299.** (Aside:
      confirmed ADRIFT's `*` is zero-or-more and both run390/run400 match it.)
      Compass labels in Bellefleur are rotated vs the exit table; navigate by the
      game's "you can move…" text.
- [x] **To_Hell_And_Beyond** — **DONE (2026-07-14): the existing 224-cmd
      `to_hell_and_beyond_assisted_solution.txt` WINS 248/373 deterministically**
      under `SCR_ASSUME_COMBAT=1 SCR_ASSUME_MOVES=1` (both assists are
      REQUIRED — the earlier "DESYNCS" verdict came from a replay missing the
      move assist, which leaves the player trapped in the mansion). Wired as a
      golden regression row → v4 suite 75/75 PASS. The "~293/373" estimate
      here was wrong; 248/373 matches the walkthrough. See the dated entry at
      the top of this file.

### B. Untouched games — derive walkthroughs (smallest first)
For each: boot, dump structure (re-add the `SC_DUMP_MAP` block to sctasks.c —
see git history / the X-Files session — `git checkout` it after), get max score
+ score map + exits + NPC/item locations, find the win, bank a deterministic
`harness/<game>_solution.txt`, write `<Game>_walkthrough.md`, verify 3×.
**First check if the Battle System is enabled and whether acc/agi are all 0**
(the zero-accuracy pattern seen in Azra/V&K/To-Hell/Mr-Smith) — if so, note
faithful-unwinnable + test with `SC_ASSUME_COMBAT=1`.

- [x] **lifesimulation** (35 KB) — **0/0 sandbox, no win** (like Azra/Nonsense).
      Walkthrough `lifesimulation_walkthrough.md`; tour solution
      `harness/lifesimulation_solution.txt`. Open-ended life-sim (apartment +
      Hudson high street: shops, bank/ATM/loans, TV, bills economy, online
      lottery). Structural dump of all 67 tasks: **zero ChangeScore (type 4) and
      zero EndGame (type 6) actions** ⇒ no score, no win/lose ending possible.
      `#winner` (task 17) is just the lottery payoff (sets money var = $5000),
      not an ending. `shoot lisa` (the supermarket clerk) is the only Battle-
      System use & the only "you can die"; still leads nowhere scored.
- [x] **The_Spirits_Flight** (37 KB) — **UNWINNABLE; max reachable 50/95**
      (deterministic). Walkthrough `The_Spirits_Flight_walkthrough.md`; solution
      `harness/spirits_flight_solution.txt`. Battle System (faithful, non-zero
      stats — no assist needed). 4-realm elemental quest. **Root-cause bug: the
      Ice Totem (water elemental, dyn1/global obj1) is orphaned — no task,
      KilledTask, or event ever un-hides it** (Crynasalda's KilledTask=task12
      awards the Sea Serpent's Scales armour instead). ⇒ `invoke elementals`
      (+5, needs Amber+Ice+Orb held) impossible ⇒ **Rocky Path→Descent exit is
      gated on invoke**, sealing the ONLY entry to the deep-earth cluster ⇒
      Acuru (+10) + Griffon/Carnifern/Spirit Paladin (+10 ea) all unreachable
      (45 pts lost) + the win incantation (task29) never fires. Faithful to the
      Runner (gates/KilledTasks in the .taf). **Guardian mechanic: type the
      literal ADRIFT topic command `<name> t1` (e.g. `kelorano t1`) — Kelorano
      has no KilledTask so killing him gives nothing.** Bootstrap: `wake kilfuno`
      moves the dagger to the Stone Circle → `show dagger to lamanluie` opens
      the air realm; order air→water→fire→earth. **NOT a 3.9→4.0 conversion
      break: natively ADRIFT 3.90** (TAF sig byte8=0x94/byte10=0x37 vs 4.0's
      0x93/0x3e; SCARE parses the simpler 6-field 3.9 NPC_BATTLE correctly, EVENT
      schema identical to 4.0). Root cause = plain authoring omission:
      Fergo/Kelorano/Acuru kill-tasks each carry a "move elemental to player's
      room" action; Crynasalda's (task12) has the +10 and a Scales drop but is
      just MISSING the Ice-Totem move — unwinnable in the real 3.9 Runner too.
- [x] **inverness** (45 KB) — *Inverness Castle* v0.3c, a Macbeth Act 1–2
      dramatisation. **UNWINNABLE (no ending); max reachable 75/205**,
      deterministic. Walkthrough `Inverness_walkthrough.md`; solution
      `harness/inverness_solution.txt`. **Zero EndGame (type-6) actions** ⇒ no
      win/lose state (unfinished beta). **130 of 205 pts structurally
      unreachable:** the witches' riddle box has 14 alternative answer tasks
      (T29–T42, +10 each) gated on one "current riddle id" var, but the box poses
      ONE random riddle and the first correct answer opens it — so only one
      riddle (+10) is reachable per game, never 140. The reachable 75 = knock 5,
      torch 10 (distract Porter with pantry bread→take wall torch), push statue 5
      + torch-in-hole + look-behind-painting 10 (reveals box), carry box to heath
      ask-witch 5 + answer riddle 10 (opens box→old key), unlock+open desk 10
      (old key), search-bedroom 20 (T8+T9, overhear the murder plot in the
      Dressing Room). NB front door re-locks on exit → re-enter via back door
      (Road→W→Behind Inverness→S→Kitchen). The final +20 REQUIRES getting caught,
      which dead-ends you tied up in the cellar (escape task `drop belt`=T54 is an
      empty stub; cellar exit gated on a flag nothing sets — `$getcaught` sets
      vars 11/12 to 1 and T50 blocks every direction while var 12 is 1, and
      nothing ever writes them again). Faithful to the 3.90 Runner. (Battle
      System present but not used for any reachable point.)
      **Re-audited 2026-08-02** — verdict unchanged, route repaired: the
      `scr_randomint` low-bit fix re-rolled `$initriddle`, so the posed riddle
      is now the *bookmark* one and the banked `answer step` scored nothing
      (65/205 while still passing its prose marker). Solution now answers
      `bookmark`, ends with `score`, golden re-blessed, marker = the 75/205
      score line. Method note for future "is it really unwinnable?" audits:
      dump the action histogram (`SCR_DUMP_TASKS=1`) and check it against a
      known-winnable file of the SAME version as a positive control — 3.9
      `circus.taf` shows 24 type-6 EndGame actions, inverness shows none, and
      on-disk 3.9 EndGame is raw type 5 (`V390_TASK_ACTION:Type>4?#Type++`).
- [x] **SecretOfLostWorld** (49 KB) — **WON, full 3300/3300**, deterministic.
      Walkthrough `SecretOfLostWorld_walkthrough.md`; solution
      `harness/secret_of_lost_world_solution.txt` (1st line = gender answer
      `female`). *The Secret of The Lost World* — a real Atlantis treasure-hunt:
      33 scoring tasks ×100, win = `turn wheel` on the rescued ship. **Required
      a real SCARE fix: the player-gender start-up prompt** (see above) — the
      castle seal-ring lock is gender-gated and the game was unwinnable in
      Spatterlight without it (NOT a game-data dead end; winnable in the Runner
      once you pick a gender). Route notes: solar-order planet placement on the
      throne (weight limit forces fetch-and-place), grab the lighted torch
      before the dark caves, beat the treasure-guarding Ghost with the green
      potion and **save the red potion (+50 stamina) for Kronos** (he one-shots
      a weak player on entry; the kill itself is a scripted task, his 1000
      stamina irrelevant), and return Excalibur to the Goddess **before** giving
      the Princess the ring (which arms the winning wheel).
- [x] **Toxically_Earth** (51 KB) — **WIN reached, deterministic; 0/0 (no
      score).** Walkthrough `Toxically_Earth_walkthrough.md`; solution
      `harness/toxically_earth_solution.txt` (11 cmds). Tobias Schmitt's surreal
      German "RON" comedy. Structural dump: **zero ChangeScore (type-4) actions
      ⇒ no score**, but **eight EndGame (type-6, var1=0=win) actions** — eight
      separate win-ending rooms (author's win text literally says "you can find
      any more endings… play again"). Banked the shortest = the **spacerabbit
      ending**: `down`×4 → Jail → `call spacerabbit` (no-restriction task, opens
      the jail's EAST door) → `east` → `move branch` (reveals a hidden bell;
      `ring bell` is task-state-gated on it) → `ring bell` → `north` → "End"
      room → `down` = win. The Jail's NORTH door (`open keyhole with needle`,
      needle held from start) is the entrance to the large toxic-earth world
      (Street #1 hub → shops/"Paradise #1–6") holding the other 7 endings, each
      a short fetch-unlock chain. No combat used (assist irrelevant).
- [x] **gateway** (81 KB) — *Gateway: Guardian Child* by Michael R. Grice. **WON,
      full 30/30, deterministic.** Walkthrough `Gateway_walkthrough.md`; solution
      `harness/gateway_solution.txt` (1st two lines = name `Hero` + gender
      `male` — this game uses BOTH start-up prompts). **Unfinished beta** (only
      the Fayn-temple zone is implemented; the END room is a literal "type End
      Game to win" stub), but a real scored/winnable zone. Dump: the **only**
      ChangeScore actions are 3×+10 flower pickups (tasks 59/75/89) ⇒ **max 30**;
      EndGame = win (task 0, room 26) + one death (task 86 = opening the wrong
      jail cell). Route: forced guards/priest cut-scene → courtyard hub → Garden
      flower → Prayer-Room flower → Guard Room (`get key` from Kadfast) → open
      the **East** jail cell (flower #3 + a Rapist; grab & flee, do NOT open the
      West cell = death) → escape (guards confiscate the key on the way out, so a
      double-`up` is needed) → THE END → `end game`. Battle System present (jail
      Rapist) but no fight must be won (no assist needed). Timed cut-scene fires
      on the first turn after entering the courtyard and eats that command.
- [x] **Phoenix_Destiny** (121 KB) — *Phoenix Destiny: Book One* by Chris Tyson
      ("Eternal Adriftware"). **UNWINNABLE & 0/0 (no score) — unfinished beta.**
      Walkthrough `Phoenix_Destiny_walkthrough.md`; deterministic systems-tour
      solution `harness/phoenix_destiny_solution.txt` (13 leading blank lines
      clear the intro cut-scene, then name/gender/race/class/stats). The intro
      promises an amulet-delivery quest to Opus atop Mount Yuko, but **all 34
      rooms are the Town of Azeroth — no Mount Yuko/Opus/amulet task exists.**
      Dump of 177 tasks: **zero ChangeScore ⇒ no score**; **no var1=0 win**; only
      EndGame actions are 1 timed lose (#endgame at year≥1241, ~5 yrs away =
      effectively unreachable) + 4 survival deaths (#nofood/#nowater/#noenergy/
      #nostomach). So the only reachable outcomes are starvation/thirst deaths.
      What IS built and works: full char creation (Human/Elf/Dwarve ×
      Warrior/Wizard/Ranger + height/weight/age), survival vitals
      (Energy/Food/Water/Stomach/Alcohol+hangover), a real-time clock with
      **shop opening hours** ("shop isn't open yet"), a town economy (Bank loans
      + buy/sell shares; start 100g, warrior class costs 312g), 3 class trees w/
      spells (wizard) & bows (ranger), and ~10 shops + many chat NPCs. Faithful
      to data/Runner; not a SCARE bug. Uses both name+gender prompts.
- [x] **hyper_b_s** (132 KB) — **WON, full 100/100, deterministic.** Walkthrough
      `hyper_b_s_walkthrough.md`; solution `harness/hyper_b_s_solution.txt`. Not a
      story game — *"HYPER Battle System" v1.1* (Seciden Mencarde, 2002), a
      shareware **tech-demo of a hand-rolled custom menu battle engine** (NOT the
      ADRIFT Battle System — plain tasks/variables, so faithful, no assist).
      One fight: from the Lobby `down` → Basement Tutorial Lounge → `2` (First
      Battle) → Flare Rat room → `battle rat` → then **10× the menu loop
      `a` / `p` / `<blank>`** (Attack→Punch→dismiss the `][` key-screen). Punch =
      3 dmg, rat has 30 HP, you have 100 ⇒ 10 punches kill it. Dump: the only
      type-4 (ChangeScore +100) and type-6 (EndGame win) live in task 8 "KILLRAT"
      (fires the turn rat HP hits 0) ⇒ **max 100**. Menu gotcha: `p`/`w` valid
      only on the Attack sub-menu, `a`/`d`/`f`/`m` only on the Battle Menu, and
      the blank-line `][` dismiss is required or the menu de-syncs.
- [x] **Shadow_Of_The_Past** (272 KB — largest file, but only embedded graphics;
      the game is tiny: 8 rooms) — **WON, max reachable 90/100, deterministic.**
      Walkthrough `Shadow_Of_The_Past_walkthrough.md`; solution
      `harness/shadow_of_the_past_solution.txt`. A reborn-hero cave-escape: free
      trapped souls by **destroying the crystal with the tuning fork** (touching
      it bare-handed = EndGame death −10). Map = two clusters joined only at the
      Ruined-Statue hub (E side: Statue⇄Gallery⇄Passage→win; W side:
      Statue⇄Cage⇄Humming⇄Blocked⇄Ledge). 11 ChangeScore tasks summing to 100;
      route: climb statue +5 / listen +10 / remove portraits +15 / pull lever +5
      (raises sunken cage) + open cage door / examine good book +5 / hang rope on
      hooks +10 (unlocks Ledge Down exit) / get tuning fork +10 / get crown +5 /
      touch fork to crystal +15 (needs portraits removed first) / **NE out of the
      Passage = the win +10**. Nav by the game's own "you can move…" text
      (compass labels rotated vs the exit table). **The +10 "beast killed" task is
      orphaned/unreachable** (NPC KilledTask="No Task"/0, no type-5 exec, no
      events, command is a no-rooms internal) ⇒ honest max 90, faithful (dead in
      the Runner too). **Beast handling:** grabbing the gold crown (in the Cage,
      raised by the lever — NOT the scenery crown on the statue) wakes a Beast
      (all stats 35; player stats are wide random ranges so the fight is a pure
      RNG gamble that can one-shot you). DON'T fight — grab crown last on the way
      out and immediately leave (`e`,`n`) to the Gallery, which the Beast can't
      enter; under seed 1234 its parting swings roll 0 damage so the banked win is
      deterministic (lever/door do NOT wake it — only `get crown` does).

### Reusable harness facts
- Build: `sh harness/build.sh` (rebuild after any terp change). Play:
  `sh harness/play.sh <game.taf> <solution.txt> [extra cmds…]`.
- Deterministic seed 1234 (`seed.c`); `SC_ASSUME_COMBAT=1` opts in to combat-assist.
- Debugger dumps: `perl -e 'alarm 8; exec @ARGV' env SC_DEBUGGER_ENABLED=1
  ./scare GAME` then `debug` + `tasks 0 N` / `rooms` / `objects` / `npcs`.
  Find N from the "valid values are 0 to N" error. Read with `grep -a`
  (NUL bytes); set `LC_ALL=C` for games with non-UTF8 room names.
- **Score/win existence test (definitive, fast):** the dump is now a **committed,
  opt-in module — `terps/scare/scdump.c`** (built only into the harness via
  `-DSCARE_DUMP_TOOLS`; `#ifdef`-guarded one-line hooks in `sctasks.c` /
  `scnpcs.c`; the Spatterlight build omits it). Just `SC_DUMP_TASKS=1
  ./harness/scare GAME </dev/null 2>dump.txt`. It prints, per task: `Command[0]`
  (read `S<-sisi`, index 0 — `Command` is an ARRAY node, not a leaf!), `Where`,
  `Repeatable`, and every restriction/action `Type`+Vars with object/task names
  resolved, plus a container-index table and the full event table. It uses raw
  `prop_get` (returns FALSE on missing), NOT `prop_get_string/integer` (they
  `sc_fatal`). Also adds `SC_TRACE_TASKS=1` (built-in task/restr PASS/FAIL trace)
  and `SC_TRACE_JUDY=1` (per-turn NPC-room dump, for pinning a wandering NPC's
  walk). No more re-deriving or `git checkout` of the dump each session. Action
  types: 0 move-obj,
  1 move-char, 2 obj-status, 3 change-var, 4 **ChangeScore**, 5 exec/unset-task,
  6 **EndGame** (var1: 0 win/WinText, 1 lose, 2 death, 3 silent stop), 7 battle-
  attr. **No type-4 ⇒ 0/0 no score; no type-6 ⇒ no win/lose ending** (proved
  lifesimulation is a pure sandbox in one pass). `git checkout sctasks.c` after.
- **Extended dump (room/event/NPC) — decisive for Spirit's Flight.** The same
  block can also walk, into stderr: (a) **room exits + gates** —
  `Rooms[r].Exits[d].Dest` (dir order N,E,S,W,U,D,IN,OUT,NE,NW,SE,SW; Dest is
  1-based) and the exit's `Var1`(restriction task+1)/`Var2`(expected done
  0/1)/`Var3`(0=task-gate); (b) **events** — `Events[e]` fields TaskNum (starter
  task), Obj1/Obj1Dest/Obj2.../TaskAffected; (c) **NPC KilledTask/StaminaTask** —
  `NPCs[n].Battle.KilledTask` (1-based; -1=none), the task an NPC runs when
  killed. **Object index decode: restriction/action `var1>=3` ⇒ dynamic object
  `var1-3`** (`obj_dynamic_object` = n-th non-static global index). This is how a
  **Two more handy fields for the same dump block (used to crack inverness):**
  (1) the task's **`Where`** — `Type` (0 NO_ROOMS,1 ONE_ROOM,2 SOME_ROOMS,
  3 ALL_ROOMS) + `Room` (ONE_ROOM) or the `Rooms[r]` boolean array (SOME_ROOMS) —
  tells you *which room a "Not runnable" command-task fires in* (e.g. inverness
  `search bedroom` only runs in the Dressing Room). (2) **Events**: walk
  `Events[e]` `StarterType` (1=running/timed, 3=on-task-completion), `TaskNum`
  (starter task, 1-based; 0=none) and `TaskAffected` (task run at the event's
  end) to trace plot triggers (inverness: search→overhear→"get caught"
  event→knockout→cellar). This is how a
  whole game's reachability graph is proven on paper: an elemental/key referenced
  by a win/gate but moved by NO task/event/KilledTask = an orphaned object =
  unwinnable. **Guardian/NPC "X t1" tasks fire by typing the literal ADRIFT topic
  command (`kelorano t1`) OR via the NPC's KilledTask (kill it) if set.** Game
  direction labels can be rotated vs. the dump's index order — navigate by the
  game's own "you can move …" text, not the dumped NE/NW/SE/SW.
- Two committed SCARE changes on `claudeslop`: the `close window` crash fix
  (`2666ec95`) and the opt-in combat-assist (`61e04e0f`). Keep the tree clean —
  always `git checkout` temporary dump instrumentation.

## Status

- [x] **Sun_Empire_Quest_For_The_Founders** — 140/145, win verified. Walkthrough
      `Sun_Empire_walkthrough.md`; solution `harness/sun_empire_solution.txt`.
      (The last point is a game-data bug: a sample task uses AND where it needs
      OR. Faithful in SCARE *and* the original Runner — see that file.)
- [x] **Orient_Express** ("The Oriental Express") — 300/300, win verified.
      Walkthrough `Orient_Express_walkthrough.md`; solution
      `harness/orient_express_solution.txt`. (Comedy jewel-heist on a train;
      17 scoring tasks. The single coin scores BOTH `put coin in slot` (+3) and
      `give coin to waiter` (+20) — the waiter task's restriction doesn't
      require holding the coin, the "toss" line is flavor. Full 300 reachable.)
- [x] Phoenix_Destiny — DONE (unwinnable 0/0 beta; see its entry above). Wired 2026-07-14.
- [x] SecretOfLostWorld — DONE (WON 3300/3300; see its entry above). Wired 2026-07-14.
- [x] **Shadow_Of_The_Past** — WON, max 90/100 (see entry above; orphaned
      "beast killed" +10 caps it at 90).
- [x] **The_Nonsense_Machine_6000** — *not a game.* Toy random-nonsense
      generator: 1 room, 1 object (lever), 1 task (`*pull*`), **max score 0/0**,
      no win state. Walkthrough `The_Nonsense_Machine_6000_walkthrough.md`;
      solution `harness/the_nonsense_machine_6000_solution.txt`.
- [x] **The_Search_For_Mr_Smith** — **WIN 90/100** (re-derived 2026-08-02;
      the earlier "UNWINNABLE, max 25/100" verdict was WRONG).
      Walkthrough `The_Search_For_Mr_Smith_walkthrough.md`; solution
      `harness/mr_smith_solution.txt`; marker = the victory line
      `Congratulations! I hope you liked our game.`
      **The file is V390**, so `battle_legacy` skips the `acc>agi` gate
      entirely — every attack lands and the old zero-accuracy analysis simply
      does not apply to it. Colt .45 kills Fernelli in 12 shots (10+20−25=5 ×12
      vs 60 stamina) and he drops the GOLD KEY; the Attic **flak jacket**
      (protection 30) cuts his shotgun from 35 to 5 a turn, and the Courtyard
      fountain is a repeatable +50 heal. That unlocks unchain butler → bed
      descent → the whole lower map → `snipe fuel tank with rifle` (type-6 win).
      Traps: `lie on bed` is a **one-way** drop (rooms 12–29 have no way back to
      0–11, so the fountain is gone after it); `open cabinet` must be typed
      **twice** (the task scores while the object stays shut); `take boulder` is
      a scoring-less red herring; `destroy boulder` starts a **30-turn** timer to
      task 21 `lost game` (−20).
      The last 10 points (`###bear dead`, the bear's KilledTask) are
      **unreachable — an authoring oversight**: shotgun damage 10+40−40=10 ⇒ 7
      shots for the bear's 70 stamina, bear damage 60−50=10 every turn (Speed 0,
      no RNG), plus a free hit on the turn you walk in ⇒ 80 damage against a
      60 (potatoes-boosted max) + 20 (sandwich) budget, and death is at
      `stamina <= 0`. Short by exactly one hit, bear left on 10.
- [x] The_Spirits_Flight — DONE (unwinnable; max 50/95; see its entry above). Wired 2026-07-14.
- [x] **The_Town_Of_Azra** — unfinished RPG sandbox; **no score (0/0), no win**.
      Walkthrough `The_Town_Of_Azra_walkthrough.md`; solution
      `harness/the_town_of_azra_solution.txt`. Of the author's 6 intro goals,
      only **3 (shopping) & 4 (Gralle's Inn)** are reachable. Goals **1 (kill
      bandit) & 2 (kill deer→sell)** are impossible because EVERY character has
      Accuracy 0 / Agility 0 and every weapon's Accuracy bonus is 0, so the
      Battle System's `acc>agi` hit test is always false (no hit ever lands) —
      and no type-7 ChangeBattle action touches Accuracy or Agility. (Re-audited
      2026-08-02, verdict CONFIRMED — see the dated section at the top of this
      file. The game *does* have 22 type-7 actions, contrary to an earlier note
      here; they are all Attitude/Stamina/MaxStamina.) Goals **6
      (Stealth, needs $800) & 5 (house, needs $7500)** are then unreachable
      because the only income (bandit money / deer carcass) is combat-gated and
      you start with $500. All game-data limitations, faithful to the Runner —
      NOT SCARE bugs. (Confirmed the combat-stalemate question raised mid-task.)
- [x] **The_X-Files_A_New_Beginning** — **WON, full game 299/299 (100%),
      deterministic** (see the finished entry in section A above). Win = `The End`
      (task31) after `ko` in the Forest Clearing. The last +1 (`Look up
      *%character%*`) just needs the phone book HELD and OPEN (`take phone book` →
      `open phone book` → `look up byers`) — not a parser/engine issue.
- [x] **To_Hell_And_Beyond** — **UNWINNABLE** (max reachable ~68/373).
      Analysis `To_Hell_And_Beyond_walkthrough.md`; opening solution
      `harness/to_hell_and_beyond_solution.txt`. Large (190 rooms) but its whole
      win is gated behind combat that can't work: player + all 41 NPCs + all 10
      weapons have Accuracy/Agility 0 (and NO type-7 action or weapon ever raises
      accuracy — kills only boost stamina/str/def), so `acc>agi` = `0>0` never
      hits. The 4 kills are battle KilledTasks (zero type-5 actions in the game,
      no event triggers them) and BOTH endings (`go home` +80, `claim throne`
      +150) require `^^xozimisdead^^`. Same zero-accuracy data as Azra/V&K,
      faithful to Runner (SCARE reads non-zero stats fine, cf. Sun Empire).
      Reachable non-combat: rope/cliff +3, meat pkg/nimf +10, meat/dog +5,
      crowbar/manhole +5, armor +20, greet Trace +25; `^^Days^^` docks -3/day.
      **ASSISTED (`SC_ASSUME_COMBAT=1`): winnable, ~293/373 (claim throne +150).**
      Full reverse-engineered route map added to the walkthrough (Oran→Tinev→
      ship→shore/forest→Mika→castle/Sulfan→final<B>→Large cave/Xozim r189 via
      `^^movetolargecave^^` teleport; Megasword in Sulfan Weapons shop r177 kills
      Xozim). Verified the assisted opening only (`greet zifan`/`open door`/Oran/
      hook in Ilsar's house, `harness/to_hell_and_beyond_assisted_opening.txt`);
      full 190-room turn-by-turn list NOT banked — needs multi-session play-
      discovery of the conversation/plot teleports. Roadmap is RE'd from data.
- [x] Toxically_Earth — DONE (WIN, 0/0 multi-ending; see its entry above). Wired 2026-07-14.
- [x] **Villains_And_Kings** — **31/37**, no win ending (re-derived 2026-08-02;
      the earlier "13/37 faithful, 30/37 with combat-assist" verdict was WRONG).
      Walkthrough `Villains_And_Kings_walkthrough.md`; solution
      `harness/villains_and_kings_solution.txt`. **Cause of the old error: this
      is a V390 file** (`c2 cf 94 45 37 61`), so `battle_is_legacy_version()`
      routes it through `battle_legacy`, which **skips the `accuracy > agility`
      gate entirely** — every blow lands and the acc/agi-all-0 "unkillable
      assassin" reading (borrowed from the 4.0 game Azra) simply doesn't apply.
      The assassin has 3 stamina / defence 1; the Armory sword (hit 2) gives
      2+2−1 = **3 damage, a one-stroke kill**; the Armory grenade (hit 10,
      Method 5) one-shots it thrown; bare hands do 1 and take three turns. It
      hits back for 1−1 = 0. So `jackassdies` +2, `search guy` +10 and the
      golden soap → `give golden soap to king` +5 are all reachable with **no
      assist**, and the assisted row plus
      `harness/villains_and_kings_assisted_solution{,.expected}.txt` are retired.
      `open window` (+1) is reachable too — it needs the window in CLOSED(6),
      which is its *starting* state, so type it **before `push tile`** (the tile
      moves it to OPEN and `close window` to LOCKED; it never returns).
      Genuinely dead, 6 pts: **`take soap` task 5 (+1) has `Where =
      ROOMLIST_NO_ROOMS`** so it can never run in any room (not the verb race
      the old note claimed), and **`yes` (task 17, +5)** duplicates
      `give soap to king` (task 2) over the one `soap on a rope` — verified
      live, after giving it `yes` only prints nag text. No `ACT type=6` exists
      anywhere in the file, so there is still no ending.
      **Found+fixed a real SCARE crash**: `close window` with no referenced
      object passed -1 to prop_get_integer (abort); guarded in
      `screstrs.cpp` `restr_pass_task_object_state` (object<0 ⇒ FALSE).
      Battle verbs confirmed working (attack/hit/stab via NPC alias "guy");
      user independently confirmed noun resolution parity in the real Runner.
- [x] gateway — DONE (WON 30/30; see its entry above). Wired 2026-07-14.
- [x] **hyper_b_s** — WON 100/100 (see entry above; HYPER Battle System demo).
- [x] inverness — DONE (no ending; max 75/205; see its entry above). Wired 2026-07-14.
- [x] lifesimulation — DONE (0/0 sandbox; see its entry above). Wired 2026-07-14.

Suggested order: smallest first (`The_Nonsense_Machine_6000`, `Orient_Express`,
`The_Town_Of_Azra`) to validate the workflow, then the rest.

## The harness (already built, in `harness/`)

- `harness/build.sh` — builds a standalone, **deterministic** ANSI `scare`
  (stdin→stdout) from `…/spatterlight/terps/scare`. Rebuild after any terp
  change: `sh harness/build.sh`.
- `harness/seed.c` — constructor that forces SCARE's portable RNG + fixed seed
  so the **ADRIFT Battle System and all randomness are reproducible**. Without
  it, identical commands give different scores (native build seeds from time()).
- `harness/play.sh` — `sh play.sh <game.taf> <solution.txt> [extra cmds…]`.
  Appends `quit`/`y`; extra args probe the next step without editing the file.
- `harness/scare` — the built binary (rebuild if missing/stale).

## Per-game workflow

1. **Boot & look.** Run with an empty solution; read the intro, first room,
   objects, NPCs. Keep a running `solution.txt` (one command per line) of the
   *confirmed* path; replay it every iteration (`play.sh`), appending probes.

2. **Dump the structure up front** (this is what makes it fast — don't brute the
   parser). With `SC_DEBUGGER_ENABLED=1`, type `debug` then:
   - `tasks 0 N` — every task's **exact command pattern** (the verbs the author
     expects, e.g. `perform dna analysis`, `get sample from limb`). Find N from
     the "valid values are 0 to N" error.
   - `rooms 0 N` / `objects 0 N` / `npcs 0 N` — names, **locations**, container
     contents, hidden/locked flags. This locates keys, weapons, the win item,
     etc. without searching blindly.
   - `events 0 N` — timed/triggered plot (alarms, attacks).
   Kill the process fast (it EOF-loops after the dump): wrap with
   `perl -e 'alarm 8; exec @ARGV' env SC_DEBUGGER_ENABLED=1 ./scare GAME`,
   and grep the output with `grep -a` (the stream contains NUL bytes).

3. **Get the exact scoring map.** Determine every point source and the maximum.
   Temporarily instrument `sctasks.c` `task_run_task_actions()` to enumerate, on
   first call, all tasks whose action `Type==4` (ChangeScore) with their `Var1`
   points and the task `Command` — gated on an env var so it's a one-liner to
   trigger. (See git history / the Sun Empire session for the exact block.)
   `git checkout sctasks.c` and rebuild when done. The points should sum to the
   game's max score; that tells you exactly which tasks to complete.

4. **Play to a win**, banking confirmed steps into `solution.txt`. Use the
   `tasks` command patterns for verbs and the `objects`/`npcs` dumps for "where
   is X". Watch `score`. Wandering NPCs: find the deterministic turn they're
   present (trace with several `look`s) rather than guessing.

5. **Push toward max score.** Compare the scoring map to what you've collected.
   For each missing point, find the task and satisfy it.

6. **Diagnose anything that won't score** before calling it unreachable. Turn on
   tracing: `SC_TRACE_FLAGS=256` (tasks+restrictions; add `+8` for the parser).
   It prints, per task, the restriction **bracket expression** (e.g.
   `#A#A(#O#)` = R0 AND R1 AND (R2 OR R3)) and each restriction PASS/FAIL with
   its operands. This tells you *why* a task fails and whether a point is a real
   game bug vs. a wrong command/timing. (Footgun: the flavor text can lie — a
   task can print "you can't…" and still score if its restriction is an OR with
   a passing branch. Trust the trace + the `score`, not the prose.)

7. **Attribute unreachable points honestly.** If a point is unreachable, decide
   whether it's the **game data** or **SCARE**. The restriction bracket
   operators (AND/OR) live in the `.taf`; both SCARE (`screstrs.c` tokenizer,
   `TOK_AND`/`TOK_OR`) and the original ADRIFT Runner (its `evaluaterestrictions`
   routine, visible in `~/Desktop/run400.txt`) evaluate the same expression. If
   SCARE matches that, the bug is the author's and exists in the Runner too —
   state that. Only call it a SCARE divergence if SCARE's evaluation actually
   differs from the Runner P-code.

8. **Verify & write up.** Re-run the final `solution.txt` 3× (must be identical
   score + "Congratulations"/win marker — determinism guarantees this). Write
   `<Game>_walkthrough.md`: header (author/comp/result), the full command list,
   phase-by-phase prose, and a closing note on any unreachable points with the
   evidence. Save the raw solution as `harness/<game>_solution.txt`.

## Footguns / lessons learned

- **Rebuild after editing any terp `.c`.** And **always `git checkout`** any
  temporary instrumentation (`scbattle.c`, `sctasks.c`, …) — leave the tree clean.
- **Determinism = combat reproducibility.** The Battle System is RNG-driven; the
  seed shim is what makes scores stable. "doesn't seem to do any damage" is the
  faithful `damage = strength − defence ≤ 0` branch, not a bug.
- **The debugger EOF-loops** after a dump and floods MB of output — always cap
  with `perl alarm` and read with `grep -a` (NUL bytes ⇒ "binary file").
- **`tasks` with no range lists only *currently-runnable* tasks**; use a range
  (`tasks 0 N`) to see everything.
- **Author "to-do"/objective lists can be pure flavor** — cross-check against
  the real task list before chasing them.
- **No `timeout(1)` on this Mac** — use `perl -e 'alarm S; exec @ARGV'`.
- Use the scratchpad for throwaway files; keep only the solution + walkthrough +
  harness here.
