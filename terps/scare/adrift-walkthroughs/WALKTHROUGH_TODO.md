# TODO: Derive walkthroughs for the ADRIFT games in `games/`

Goal: produce a verified, reproducible, near-maximum-score walkthrough for each
`.taf` in `games/`, in the style of `Sun_Empire_walkthrough.md` (full command
list + annotated phases + an honest note on any unreachable points and *why*).

These are obscure 2000–2005 ADRIFT comp games with no published walkthroughs
(checked Key & Compass, IF Archive, CASA). We derive them by driving the game
through a headless, deterministic SCARE build and reading its internals.

## 2026-06-24 session: the newly-added games (see `TRIAGE_NOTES.md`)

The `games/` folder grew to 50 `.taf` since this file was first written; all 32
not-yet-covered games were triaged with the structural dump and `TRIAGE_NOTES.md`
records the full classification. Progress this session (12 new walkthroughs):

- **Wins, verified deterministic:** `Cyber_walkthrough.md` (150/150),
  `cyber2_walkthrough.md` (355/355), `TheCatintheTree_walkthrough.md` (50/50),
  `Colony_walkthrough.md` (200/200), `Jason_Vs_Salm_walkthrough.md` (WIN, honest
  max 0/1000 — scored difficulties are an unwinnable combat-balance bug),
  `donuts_intro_walkthrough.md` (0/0 win intro).
- **Documented unwinnable / sandbox / no-ending:** `Del_Sol_walkthrough.md`
  (orphaned win — Moreland's KilledTask gated behind a stamina-0 NPC; 24/46),
  `QuestI_walkthrough.md`, `IceCream_walkthrough.md`, `Trabula_walkthrough.md`,
  `Invasion_of_the_Second-Hand_Shirts_walkthrough.md`, `adriftorama_walkthrough.md`.
- **Deferred (winnable, route not yet banked):** thetest (color-key/phone-number/
  teleporter puzzle-box), Main Course (catnip + cat-fur-disguise puzzle).
- **Banked since:** `FunHouse_walkthrough.md` (**WON, max 310/410**) — see the
  2026-06-24 (later) FunHouse entry below.
- **Still untouched:** Melbourne Beach, The Screen Savers
  On Planet X, ALEXIS, Shadowpeak, circus, WesGHN, Space Boy's First Adventure
  (all winnable, large); Bomb Threat, tcom (win, 0-score); Matt's House, Les Feux
  de l'enfer (score, no win); Through time (lose-only); SRSintro (0/0 intro);
  Theannihilationofthink2, deaths, lair-of-the-cybercow (hang after "Loading…").

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

## Combat-assist note (opt-in, committed)

Several Battle-System games here ship with every character's Accuracy/Agility
left at 0, which (since ADRIFT gates hits on `accuracy > agility`) silently
disables combat the author intended — see the per-game walkthroughs. SCARE now
has an opt-in `sc_set_combat_assist` (harness: `SC_ASSUME_COMBAT=1`, committed
`61e04e0f`) that auto-lands hits *only* in such fully-unconfigured games, so
combat plays out on strength-vs-defence as intended (faithful default is off;
configured games like Sun Empire are unaffected). Assisted maxima derived so
far: **Villains & Kings 13→30/37** (`harness/villains_and_kings_assisted_solution.txt`);
**Azra** combat goals 1/2/6 become reachable; **To Hell & Beyond** becomes
winnable in principle (full route not yet banked); **Mr. Smith stays stuck**
(Fernelli's defence 25 > best accessible weapon 20 — a strength imbalance, not
just accuracy).

## Remaining work (what's left to do)

Done so far (12): Sun_Empire, Orient_Express, Nonsense_Machine, Town_of_Azra,
Villains_And_Kings, To_Hell_And_Beyond (analysis), Mr_Smith, lifesimulation,
The_Spirits_Flight, inverness, **SecretOfLostWorld (WIN 3300/3300)**,
**Toxically_Earth (WIN; 0/0 multi-ending)**, **gateway (WIN 30/30)**,
**Phoenix_Destiny (unwinnable 0/0 beta)**, **hyper_b_s (WIN 100/100)**,
**Shadow_Of_The_Past (WIN 90/100)**, **X-Files (WIN 299/299)**.
**Untouched: none — every game on the original list is done.** Only optional
follow-up left: banking the full assisted To_Hell_And_Beyond route (already
proven winnable, just not the full 190-room turn list).

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
- [ ] **To_Hell_And_Beyond** (optional, large) — bank the full *assisted*
      (`SC_ASSUME_COMBAT=1`) ~293/373 route across 190 rooms. Roadmap already in
      the walkthrough (Oran→Tinev→ship→shore/forest→Mika→Sulfan(Megasword)→
      final<B>→`movetolargecave`→kill Xozim→claim throne). Multi-session.

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
      empty stub; cellar exit gated on a flag nothing sets). Faithful to the 3.90
      Runner. (Battle System present but not used for any reachable point.)
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
- **Score/win existence test (definitive, fast):** temporarily add a
  `task_dump_all(game)` block to `sctasks.c` (gated on `getenv("SC_DUMP_TASKS")`,
  static-guard, called at the top of `task_can_run_task_directional`). It walks
  `gs_task_count` tasks and `prop_get_child_count` for "Restrictions"/"Actions",
  printing each task's `Command[0]` (read `S<-sisi`, index 0 — `Command` is an
  ARRAY node, not a leaf!), `RestrMask`, and every restriction/action `Type`+Vars
  to stderr. **Use raw `prop_get` (returns FALSE on missing), NOT
  `prop_get_string/integer` (they `sc_fatal`).** Action types: 0 move-obj,
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
- [ ] Phoenix_Destiny
- [ ] SecretOfLostWorld
- [x] **Shadow_Of_The_Past** — WON, max 90/100 (see entry above; orphaned
      "beast killed" +10 caps it at 90).
- [x] **The_Nonsense_Machine_6000** — *not a game.* Toy random-nonsense
      generator: 1 room, 1 object (lever), 1 task (`*pull*`), **max score 0/0**,
      no win state. Walkthrough `The_Nonsense_Machine_6000_walkthrough.md`;
      solution `harness/the_nonsense_machine_6000_solution.txt`.
- [x] **The_Search_For_Mr_Smith** — **UNWINNABLE** (max reachable 25/100).
      Walkthrough `The_Search_For_Mr_Smith_walkthrough.md`; solution
      `harness/mr_smith_solution.txt`. Win = scripted `snipe fuel tank with rifle`
      (type-6 win), but the rifle is behind: bed-descent (task4) ← needs unchain
      butler (task2) ← needs the GOLD KEY held ← held by mobster Fernelli, only
      lootable by killing him ← impossible (acc/agi all 0, verified 25 shots
      leave him 60/60). No task gives the key (the one that moves it consumes it).
      Reachable: open bookcase +5, light torch +5, open curtain +5, untie girl
      +5, drink water +5 = 25. Same zero-accuracy combat as the others; intro's
      "attack force vs defense" = the damage step only (acc>agi gate comes first).
- [ ] The_Spirits_Flight
- [x] **The_Town_Of_Azra** — unfinished RPG sandbox; **no score (0/0), no win**.
      Walkthrough `The_Town_Of_Azra_walkthrough.md`; solution
      `harness/the_town_of_azra_solution.txt`. Of the author's 6 intro goals,
      only **3 (shopping) & 4 (Gralle's Inn)** are reachable. Goals **1 (kill
      bandit) & 2 (kill deer→sell)** are impossible because EVERY character has
      Accuracy 0 / Agility 0 and every weapon's Accuracy bonus is 0, so the
      Battle System's `acc>agi` hit test is always false (no hit ever lands) —
      and no task has a type-7 ChangeBattle action to raise it. Goals **6
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
- [ ] Toxically_Earth
- [x] **Villains_And_Kings** — 13/37, no win ending (max reachable; verified
      deterministic). Walkthrough `Villains_And_Kings_walkthrough.md`; solution
      `harness/villains_and_kings_solution.txt`. The other 24 pts are unreachable
      in any faithful interp: **17** behind an unkillable assassin (Battle System
      acc/agi all 0, like Azra — `jackassdies` +2, `search guy` +10, and the
      golden soap it yields → `give golden soap` +5); **5** from mutually-
      exclusive duplicate soap-give tasks (`give soap`=task2 vs `yes`=task17, one
      soap); **2** window tasks that can't meet their state (`open window` needs
      CLOSED(6) but window only goes broken→open→locked; `take soap` task loses
      to the library "take"). **Found+fixed a real SCARE crash**: `close window`
      with no referenced object passed -1 to prop_get_integer (abort);
      guarded in screstrs.c restr_pass_task_object_state (object<0 ⇒ FALSE).
      Battle verbs confirmed working (attack/hit/stab via NPC alias "guy");
      user independently confirmed noun resolution parity in the real Runner.
- [ ] gateway
- [x] **hyper_b_s** — WON 100/100 (see entry above; HYPER Battle System demo).
- [ ] inverness
- [ ] lifesimulation

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
