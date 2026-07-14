# TODO: Derive walkthroughs for the ADRIFT games in `games/`

Goal: produce a verified, reproducible, near-maximum-score walkthrough for each
`.taf` in `games/`, in the style of `Sun_Empire_walkthrough.md` (full command
list + annotated phases + an honest note on any unreachable points and *why*).

These are obscure 2000–2005 ADRIFT comp games with no published walkthroughs
(checked Key & Compass, IF Archive, CASA). We derive them by driving the game
through a headless, deterministic SCARE build and reading its internals.

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
  `Your score is N out of a maximum of M.` line (mr_smith 25/100, wes_ghn
  30/100, spirits_flight 50/95, thetest 5/25, del_sol 24/46, les_feux 25/115
  (`Votre score est…`), villains 13/37 + assisted 30/37, questi 5/10,
  cybercow-tour 6/10, matts_house 5/5, lifesimulation 0/0). Win rows use the
  game's own victory text. inverness (75/205) is marker-guarded by the
  caught-scene prose instead — the knockout cut-scene swallows an input line,
  so a trailing `score` never executes (verified 75 by scoring pre-capture).
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
  max 0/1000 — scored difficulties are an unwinnable combat-balance bug),
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
- **Banked since:** `WesGHN_walkthrough.md` (**UNWINNABLE, max 30/100**) and
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
- [x] The_Spirits_Flight — DONE (unwinnable; max 50/95; see its entry above). Wired 2026-07-14.
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
- [x] Toxically_Earth — DONE (WIN, 0/0 multi-ending; see its entry above). Wired 2026-07-14.
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
