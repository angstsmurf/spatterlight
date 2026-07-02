# Shadowpeak — walkthrough (★ COMPLETE — WON @ 710/790, 0 deaths)

## ✅ SESSION 22 (2026-07-02): re-derived for the NPC/event tick-order fix — parity restored (710/715/740)

The faithful-to-Runner tick-order change (NPCs walk **before** events,
`scrunner.cpp`, commit 68b83f93) broke all three solution files: the Morac
climb arrived at room 101 — inside the `{65,69-103}` kill group — exactly on
the task-327 check turn, one turn short of room 104. Re-derivation restored
**exact score parity with the old-order transcripts** (score-event multisets
identical): main **710**, allgargoyles **715**, killwraith **740**, all 0 deaths.

Key mechanics discovered (full detail in
`terps/scarier/TODO_restriction_type5_and_turn_order.md`):

- **NPC-walk charTask/meetChar tasks now fire only when the NPC's own walk step
  lands on the player**, not when the player walks into the NPC's room. This
  cost three formerly-free triggers, each fixed with tuned `z` waits:
  Maretta's garlic save (T156 +5, wait at room 63), killwraith's Fang one-hit
  kill (+5, wait 25 at room 27 for "Fang darts into the area"), and
  allgargoyles' Melvin meet (T295, gates the behead-teleport T333; 17 pre-window
  waits at room 71 until he waddles in).
- **The Morac window turn was recovered by hoisting `take venison` to before
  the timer starts** — room 72 is reachable without entering room 77, and task
  193 (timer start) needs the sword in 77, so the grab is clock-free.
- The EVENT-92 duration draw (40–50) shifts with every upstream RNG change; the
  garlic-z / pre-window-wait counts double as the re-tune knob.
- Edna waits re-tuned; the Damastus chase is now re-derivable in one run with
  `harness/shadowpeak_chase.py` (greedy BFS over the dumped EXIT graph,
  `SCR_TRACE_PLAYER` + `SCR_TRACE_JUDY npc=35`). killwraith's Haraxis fight
  needed 3 pre-fight waits to dodge a −29 mega-hit.

All three rows are now in `harness/run_v4_walkthroughs.sh` with blessed goldens
and the win marker `completed the adventure Shadowpeak` (regression 20/20 PASS).
Fixed copies + a current `scare` binary synced to `~/adrift-battle/harness` and
`~/scare/adrift-walkthroughs/harness`; the 36 other adrift-battle solutions were
verified outcome-identical under the new binary.

## 🔬 SESSION 14 (2026-06-27): deep-dungeon entrance decoded (no new points banked — 355 held)

Reconnaissance only — the deep dungeon (rooms 128–138, the next big scoring cluster) is
reached from forest **room 16 `n`→128**, but the gate is the **locked double doors
(obj149)**. Established that the key is almost certainly **obj106 in the prison (room 98,
reachable now via the crypt)**, and that room 128 itself is a fresco/lever/`crawl`
death-trap puzzle. Full findings folded into the "Open for session 14 → 15" list in the
session-13 section below. Nothing banked because entering requires (a) confirming/fetching
the prison key and (b) solving a death-trap room — not a verifiable one-step increment.
The prior history-cleanup also squashed sessions 1–13 into one walkthrough commit (plus
the 7 interpreter/scdump commits kept separate).

## ✅ SESSION 13 (2026-06-27): 270 → 355/790 — crypt portal + deep-mountain riddles + boulder avalanche

**+85 in one clean deterministic chain off the end of the pentagram route.** No engine
change; pure walkthrough extension. Verified 355/790, 0 deaths, regression PASS.

**1. Crypt → portal (T291 +5).** From the pentagram (room 77): `e`→71 main hall, `d`→97
crypt entrance, `n`→99 the main crypt. The crypt door gate (97↔99↔100, exit object-state
restriction on a stateful object) is already open. **Open coffin `#11`** (`open coffin
#11`) — it is the one fully-intact coffin; this springs a spell that opens a **magical
portal on the north wall of the family crypt (room 100)**. (Do NOT open coffin `#9`: it
gasses you to death; the family-crypt coffin in room 100 holds only a holy symbol and is
a different object — obj126, not coffin #11 obj125 which lives in room 99.) Then `n`→100,
`enter portal` (**T291 +5**) → warps back to **Stonehenge (room 0)**, and a portal now
stands at the henge too. The crypt vampires (event → `vampsgetplayer` death) don't catch
you if you pass straight through room 99 to 100.

**2. Grassy plains → Andro the sphinx's riddle room (room 123).** From Stonehenge:
`e`→33 the gap, `e`→110, `ne`→114, `n`→115, `e`→116 (Grumski's old abode), `e`→122
(this exit is gated on the fairy/vial subquest **T381**, already done back in session 5),
`e`→123. Andro the sphinx blocks the east passage with three riddles whose answers are
literally the questioned words: **`g` (T404 +5), `silence` (T405 +10), `secret`
(T406 +15)** — must be solved in that order (each gates the next), and solving all three
opens 123→124.

**3. Boulder avalanche (T410 +50).** `e`→124 the eastern mountainside. **`move tree
trunk`** topples an enormous boulder stack down onto the undead horde below (a chain
reaction crushes a Morac captain/general). **T410 +50** — the single biggest point in the
game. **DO NOT go further east into room 125** — `playerdeathbystrayarrow` is an
instant-death trap there. Retreat west (`w`→123 is free; 124→123 needs T406, done).

**Open for session 14 → 15 (deep dungeon entrance DECODED, not yet banked):**
- **The deep dungeon (rooms 128–138) is a SEPARATE area, NOT off the mountain.** Reached
  from forest **room 16 "Doors in the mountains" `n`→128**. The `gateTask=29` object-state
  gate is the **huge double doors (static obj149) — and they are LOCKED.** Route to room
  16 from the henge: `w`→27, `n`→28, then via the swamp to 15, `n`→16 (compass is rotated,
  trace it). `open double doors` → "you can't open … it is locked!"; none of the keys we
  carry (brass obj24, etc.) fit.
- **The double-doors key is almost certainly obj106 — a "key" sitting in the PRISON (room
  98), reachable NOW via the crypt** (`97 s`→98, gate already open). Reasoning: the four
  keys are brass obj24 (→wooden chest, used), golden obj130 (→upper-castle steel door, and
  Morac-gated), obj172 (→ in room 172 "atop the pillar of debris", which is *inside* the
  deep area so it can't be the entrance key), leaving **obj106 (prison) as the doors key**.
  NEXT SESSION: detour into room 98 during the crypt visit, grab obj106, then unlock the
  double doors at room 16 to open the whole deep-dungeon scoring region. (Untested — room
  98 prison may hold a guard; verify before banking.)
- **Inside room 128 ("Doors in the mountains" → north) is a death-trap puzzle room:**
  frescoes (T427-429), a `get lever`→`pull lever` (T430/431; the pull needs the **chest
  obj153** in some state), then **`crawl` east (T432 +10, moves you to room 129)** — but
  the room also has a "stone block slams down" / "huge stone block" **instant-death**
  (T425/T434). Solve carefully. Beyond: 129 `e`→130 hub (gate=crawl done) → 133 throw
  fleshy mound (T443 +20), 134 `say alam nadeer` (T446 +20, "Ernamadlea" reversed), 135→
  136 (gate451)→137 the **colour-tile rainbow** (T454–460 red/yellow/pink/green/purple/
  orange/blue, +20 final) + button puzzle (T479/T484 +10).
- **The upper castle (kiss lady T305 +10, carom T320 +5) is END-GAME-gated.** Both ways
  into the 101/102/104 cluster are blocked: the room-96 steel door needs the **golden key
  obj130 — which you only get BY kissing the lady (circular)**, and the ballista-tower
  back-route (68→106→109→108→105→104) is gated by `attack morac with sceptre` (T344) and
  the golem (T319/carom). So the intended first entry is via the Morac/sceptre endgame,
  not before it. Defer the upper castle until the Morac fight is solved.
- Gargoyle #1 still trails the player (harmless, speed-1); killing it now would trip the
  rep-0 behead, so leave it.

---

## ✅ SESSION 12 (2026-06-27): 255 → 270/790 — PENTAGRAM BANKED (Morac timer + gargoyle behead both solved)

**The pentagram (+15) is now banked and the player survives.** The two session-11
blockers — the Morac death-timer and the Melvin behead — were both resolved with the
structural dump, no engine change. Net 255 → 270.

**Root causes (both were off-by-one / item-strip traps, now understood):**
- **The Morac death-timer (event 92) starts on FIRST ENTRY to room 77, not on
  carrying the sword there.** `evt_starter_task_is_complete` keys on
  `gs_task_done(TaskNum-1)`, so the dump's `startTask=193` is 1-based and the real
  0-based trigger is **task 192 `[zandysayshi]`** = simply being in room 77 with
  Zandos (no sword needed). The clock is a fixed **50 turns** (seeded). Our whole
  castle tour fits in ~33, so the timer is a non-issue once the route is tight.
- **The Melvin behead (T333) needs ALL FOUR gargoyles dead AND painting #6 destroyed
  AND Melvin met** (restr=6). It fired the instant we ripped the painting in the
  full-kill route, teleporting us to room 107 and stripping every item. **Fix: kill
  only gargoyles #2/#3/#4 and leave #1 ALIVE** — `task202 (#1 dies)` stays undone, so
  T333's conjunction can never complete. One slow speed-1 gargoyle trailing us does
  almost nothing (end stamina **25/50**). No behead, no item strip.

**The banked castle route (replaces all the session-9/10/11 castle scaffolding):**
1. Enter castle `s s s` → room 71; the four gargoyles spring to life.
2. `z` then `attack gargoyle #2` / `#3` / `#4` — Seeker one-shots each; **#1 left alive.**
3. **Lantern run** (this starts the 50-turn clock): `w`→77, `s`→78 (meet Melvin),
   `s`→79, `take lantern`, `turn lantern on` (needs the tinderbox), `n n e`→71.
4. **Cellar run** (lit lantern opens the dark cellar, T215): `e e d d e s`→86,
   `take chisel`, back `n w u u w w`→71.
5. **Scroll + burst:** `w w`→90, `destroy painting #6` (T231 +5), `n n`→93,
   `attack cat` (T237 +10, also fells the witch), `n`→94, `take scroll`, `s s s e`→77,
   `say nomorey palenvoid` (T243 +5; scroll burns, Zandos backs off), then
   `chip pentagram with chisel` (T244 +10). Order matters: chipping first matches the
   death-redirect T221.
- Verified deterministic: 270/790, 0 deaths, behead never fires, regression PASS.
- **Open for next session:** gargoyle #1 is still alive and trails the player; it must
  be dealt with before the post-pentagram content (portal/crypt/dragon), but killing
  it once the painting is destroyed + Melvin met WILL trip the (rep-0) behead — so it
  has to wait until we're clear of room 90's gallery or until a route is found that
  defers the painting/Melvin conditions.

## 🔬 SESSION 10 (2026-06-27): the gargoyle "strength blocker" was a MISDIAGNOSIS — whole upper castle de-risked (0 new banked, 245 held)

**★ THE BREAKTHROUGH: gargoyles are trivially killable — `attack gargoyle #1` (a
SPECIFIC target) ONE-SHOTS each one.** The bare `attack gargoyle` only triggers the
parser's **"Which one?  #1, #2, #3, or #4."** disambiguation prompt, so in sessions
8–9 our attacks never resolved to a target and we just stood there getting hit until
dead. That — not strength — was the entire "blocker." (Effective attack =
`base_strength_roll + wielded weapon HitValue`; the sword adds ~30, so even at the
fatigue floor our hit is ~45, and each gargoyle has Stamina/Defence ≤30 → dies in one
blow.) **Session 8's "slash village padding to reach Str 45" plan was wrong on two
counts:** (a) Strength has a hard **floor of 15** (verified: 30 extra waits leave it
at 15; `sleep` only oscillates 15↔25, cap 25/55; the fatigue drain is the rep-0 T516
plus a floor) so padding-cutting can't raise it, AND (b) it's unnecessary — the
weapon bonus already carries the damage.

**FULL PENTAGRAM +25 CHAIN MAPPED AND INDIVIDUALLY VERIFIED (every step works; only
the boots gate below stops it being banked this session):**
1. **Scroll** (obj102) sits on the *shrine* (static obj101) in **room 94** — just
   north of the cat room, already reachable. `n`→94, `take scroll`.
2. **Lantern** (obj90) in **room 79** (77 S→78 S→79): `take lantern`, `turn lantern
   on` (uses the tinderbox we carry). The cellar is dark — its interior exits are
   gated by **T215** = lantern *held + lit*.
3. **Kill the 4 gargoyles** in dining-room 72. Proven survivable deterministic prefix
   from the cat room (room 93): `sleep, s, s, e, e, e, attack gargoyle #1, #2, #3, #4`
   (ends Stamina 11; the RNG is timing-sensitive, so this exact prefix must run first,
   before any scroll/lantern detour, or the gargoyles' rolls turn lethal).
4. **Venison** (obj82) on the dining table (room 72): **`eat venison` = Stamina +10,
   REPEATABLE** (T189 re-armed by T190) — the castle stamina-restore. (Strength sleep,
   Stamina venison/water — water bottle was smashed on Maretta.)
5. **The cellar BEHEAD set-piece (unavoidable, but escapable):** descending to the
   cellar stairs (**room 84**, Melvin the Butler) fires **T333 `#melvinbeheadsplayer`**
   the moment all of {4 gargoyles dead, painting #6 done, Melvin met} hold — it
   teleports you to **room 107 "Max Head room"** (your head on a pole). **ESCAPE:
   `say sorry` (T337) then `say yes` (T338) → teleports you back to room 71**, body
   intact, inventory intact. T333 is rep=0, so re-enter the cellar freely afterward.
6. **Chisel** (obj91) — FrontDoorsSlam relocated it to **room 86**. Path
   73(kitchen)→`d`→84→`d`→83→`e`→85→`s`→86, `take chisel`. **⚠ ROOM 85 = T217
   `#ratsattack` = INSTANT DEATH unless you are WEARING the thick leather boots
   (obj5).** This is the one missing piece (see boots blocker below).
7. **Pentagram** (room 77, Zandos harmlessly entrapped): `chip pentagram with chisel
   of the ages` (**T221 +10**, disables Zandos' T222 batter), `say nomorey palenvoid`
   (**T243 +5**, needs scroll held + Zandos present), `chip pentagram with chisel of
   the ages` again (**T244 +10**) ⇒ **+25**, and opens 95→96 to the upper castle
   (kiss lady +10 / carom +5 / Colos golem / ballista bolt in room 102, etc.).

**★ THE BOOTS BLOCKER (the real next-session task) ★** The thick leather boots (obj5)
needed for room 85 are **under the villager at room 3** in the opening forest, and
collecting them also scores `roll oak tree` (T5 +5) + `move villager` (T18 +5) = +10.
BUT collecting them **anywhere before the village combat desyncs the tuned village
zombie/spider RNG** — verified twice (boots at the egg visit AND boots at the
pre-fly room-3 pass both land at **235**, i.e. +10 boots − 20 from two desynced
combat +10s = net −10). After the village we fly to the castle and never return to
room 3. **⇒ To bank the boots (+10) and unlock the pentagram (+25) we must first
RE-TUNE the village zombie/spider combat to a fixed, RNG-robust form, THEN insert the
boots at the post-gauntlet room-3 pass (PLAYERROOM index ~100, 2 cmds before the fly).**

**Misc reusable facts:** dining-room 72 `e` → the **kitchen** (room 73), not "room 73
straight"; cellar = kitchen `d` 84 (Melvin) `d` 83 (entrance). Roaming castle NPCs:
**Woody the wererat** (NPC44, neutral, Stamina 45 / Agility 60, **T529 +10** — but too
evasive to land hits in spam-testing; needs a smarter lure), Berto the bald raven
(T569 `shout berto`), Lazaraz (avoid). Battle damage = `battle_eff_strength` =
`base_str_roll + weapon HitValue` (scbattle.c). The chapel (room 75, **T188
pipe-on-altar +10**) is a 3-death-condition puzzle (no sword T187, no shield/breastplate
worn T182–185; reach via 74 `swim`/75 `dive`) — not attempted.

## ✅ SESSION 9 (2026-06-27): 230 → 245/790 — the castle west wing (painting + cat, gargoyle-free)

Banked the only two castle scoring tasks that are reachable **without** defeating
the gargoyles. Route appended to `harness/shadowpeak_solution.txt` (re-verified
deterministic, 245):

- From the castle entrance (room 63, Maretta dead): `s`→64 courtyard, `s`→65 long
  hall (the **front doors slam** — T162 relocates the carried rubble + the chisel to
  the cellar), `s`→71 **main hall** (entering wakes the 4 gargoyles, who immediately
  move **EAST** to the dining room 72; go **WEST** and they never engage).
- `w`→77 **pentagram room**: Zandos the demon is **entrapped in the pentagram** and
  cannot attack while it is intact — the room is safe to walk through (his death-trap
  T222 only fires if provoked, NOT on entry).
- `w`→90 picture hall → **`destroy painting #6`** = **T231 +5**; the ripped painting
  opens a secret passage north (90→91).
- `n`→91 dark passage, `n`→93 **the witch's cave** (Shadow the black cat + Rucktebar
  the witch). **`attack cat`** the instant you arrive (Shadow has 1 stamina, dies in
  one hit) = **T237 +10**; killing the cat also fells the witch, so you beat her
  retaliation (T240 `#Rucktebargetsnasty` = an ACT-type-6 instakill) by one turn.

**★ THE GARGOYLE / STRENGTH BLOCKER (master gate for the rest of the castle) ★**
Everything else in the castle is locked behind the 4 gargoyles in dining-room 72
(the only route to the cellar/chisel and the dining-room venison) or behind the
pentagram chip (which needs the chisel). The gargoyles have **Defence 0–30**, so
to damage them with the sword you need **Strength ≳ 35–45**. But the fatigue clock
(EVENT 147 → T516 drains Strength −10 every 7 turns) has us at **Str 15** on castle
arrival, and `sleep` (T517 +10) only **oscillates 15↔25** — verified it cannot climb
past **25/55** because each fatigue tick drains −10 and re-arms exactly one +10 sleep
(net zero). There is **no other Strength source** (only `sleep` is ACT type=7 v1=3
positive; `drink water`/`eat venison` restore *Stamina* v1=1, not Strength). The
battleaxes in guard-room 70 are **scenery (can't be taken)** — no better weapon.
**⇒ NEXT SESSION must arrive at the gargoyles with ~Str 45 by SLASHING the village
combat padding** (the 30-cycle spider + 18-cycle zombie loops each burn ~1 fatigue
tick per 7 turns); fewer turns elapsed = fewer net Strength drains. Only then can the
gargoyles fall → chisel → pentagram (+25) → upper castle (kiss lady/carom/ballista).

## ✅ SESSION 8 (2026-06-27): castle fully mapped + author's 42-hint guide extracted (0 new banked, clean stop @ 230)

Mapped the entire castle critical path, decoded the fatigue/strength survival
mechanic, and added permanent scdump tooling (per-task `HINTQ/HINT1/HINT2`, surface/
container index enumeration). Full author hint guide + ACT-type-4 scoring map in
`Shadowpeak_hints.txt`. No deterministic increment was buildable within budget
through the death-trap gargoyle/Zandos rooms, so 230 was kept solid.

## ✅ SESSION 7 (2026-06-27): 130 → 230/790 — the whole village scoring cluster + exit to the castle

Extended the deterministic route from the end of the zombie phase (room 41,
blacksmith breastplate) through the entire Windthrush village and out across the
drawbridge to the castle entrance. All steps are in `harness/shadowpeak_solution.txt`
(re-verified, banks 230). The +100 chain, in order:

- **Tinderbox** — at room 41 `open cupboard`, `take tinderbox` (obj33; lights the pipe later).
- **Bottle** (Inn 44) — it sits high on a shelf: `stand on bar`, `get bottle`. It comes
  full of a *strange liquid* (poison T84/T85 — never drink it).
- **Silver coin** (Inn upstairs 47, `u u`) — `examine shadow` reveals it (T81), `take coin`.
- **Bambi** (Inn kitchen 45, `w`) — `open wooden box` (drops the **leg of lamb** + spawns
  Bambi the child zombie), `take lamb`, `attack bambi` → **T71 +10**.
- **Water** (well bottom 38, `e e n d` from kitchen via square) — `pour liquid out`,
  `fill bottle with water`. Then `u` back to the square and `take garlic` (Holga's corpse
  dropped the **garlic bud** here when she died in the zombie phase).
- **Wolves** (footpath 53, `s s w`) — `give leg of lamb to wolves` → **T115 +10**
  (this exit-gate also unlocks the Webbed Woods / Gypsy Grove beyond room 53).
- **Church** (49, `ne se`, `open door`, `s`) — `say horamis` summons the god (NPC8),
  `say loralang` → he lays the **smoking pipe** on the altar (**T96 +5**) and fixes
  Borantha's staff; `take pipe`. Then `put bottle on altar` → blesses the water into
  **holy water** (**T91 +10**); `take bottle` (it keeps the holy water). `light pipe`.
- **Web** (Webbed Woods east 57, `n w w s e`) — `examine web` snags your feet and drops
  you into room 59 "Stuck in the web"; `burn web` (lit pipe) → **T126 +5**, frees you.
- **Giant spider Haraxis** (up the oak 58, `u`) — a **battle NPC** (NOT a stalemate; it
  just flees constantly to room 57). Spam `attack haraxis` / `z` cycles; ~7 landed hits
  kill it. Corpse obj61 spawns → **T122 +30**. (The route uses 30 cycles for a safe
  deterministic margin; post-death cycles are harmless "I don't understand" no-ops.)
- **Madam Evenov** (Gypsy Grove 56, `d w`) — the fortune is **gated on the spider being
  dead** (T143/T139 both require T122). `give coin to madam evenov` fires **T138 +5 AND
  the fog spell T139 +5** together (she neutralises Morac's fog). Then pick a card:
  `one` → **T144 +5** (the four cards are mutually exclusive — only one scores).
- **The fog** (`se` → fog 60, then `s s s` → 61 Drawbridge → 62 → 63 castle entrance).
- **Maretta, vampire wife of Morac** (room 63) — she materialises to block the gate. The
  **garlic bud** you're carrying auto-repels her (**T156 +5**). `throw bottle at maretta`
  douses her in holy water and destroys her (**T155 +10**) — no sword needed; this opens
  63 → castle courtyard (64).

**Score banked: 230/790.** Compass note: the village is heavily rotated, so every move
above was mapped empirically with `SC_TRACE_PLAYER=1`, not from the EXIT-table slots
(e.g. Gypsy Grove 56 `se`→fog, not the dump's NW). No engine code changed this session —
the `#`-comment skip (commit 4afa4086) lets the route be documented inline.

**Next (NOT yet banked):** stroke Rex (+5, NPC10 Rex's spawn location still TBD); then the
**castle** (RB, rooms 64+): front-doors slam puzzle drops the **chisel** (T162), `enter
portal` (T291 +5, room 100 crypt realm, gated on opening coffin #11 T288), pentagram /
`chip pentagram with chisel of the ages` (T221/T244 +10), and the long road to the
endgame (`kill morac` +50, `snap staff` +50, the Hell-realm horn finale).

## ✅ SESSION 6 (2026-06-27): 90 → 130/790 — the village zombies are killable (combat decoded)

**The "village combat is a damage stalemate" conclusion from session 5 was WRONG —
a misread of the combat log.** The five "villagers" are Battle-System NPCs
(Holga=NPC3, Jarris=NPC4, Boris=NPC5, Arthur=NPC6, Bambi=NPC7) plus a stationary
zombie mob (NPC9, graveyard room 51). Each named zombie's death runs its KilledTask
(68–72) which awards **+10** once (rep=0). Decoded with a new harness dump
`SC_DUMP_BATTLE=1` (scdump.c, NPC battle stats):

| NPC | Stamina | Strength | Accuracy | Defence | Agility | killedTask |
|-----|---------|----------|----------|---------|---------|------------|
| Holga | 0–50 | 25 | 0–30 | **0–10** | 0 | 72 (also drops the garlic bud) |
| Jarris | 0–100 | 25 | 0–30 | 0–30 | 0 | 68 |
| Boris | 0–50 | 0–30 | 30 | **0** | 0 | 70 |
| Arthur | 0–30 | 0–30 | 0–30 | **0** | 0 | 69 |
| Bambi | 0–50 | 0–25 | 0–50 | 0–25 | 0–25 | 71 (not yet spawned — needs the inn box, room 45) |
| zombie mob | 203 | 0–40 | 0–40 | 0–25 | 0 | 109 (places teeth/nametag, **no score**) |

With the sword wielded the player rolls **Strength 45 / Accuracy 80 / Defence 26**.
`damage = attacker.Strength − target.Defence` and the 4.0 hit test is
`accuracy > agility`. So every player hit lands (80 > agi≤45) for **20–45 damage**,
and the zombies' own hits never dent our Defence 26 ("…doesn't seem to do any
damage" is the *zombie* failing, not us). Boris/Arthur die in **1 hit**, Holga ~2,
Jarris/Bambi/mob a handful.

**Two combat-timing facts that make the route deterministic:**
1. NPC walks (random, `dest=203`) fire **after** the player's command, so you can
   only attack an NPC that is already co-located at the **start** of your turn —
   i.e. one that wandered in last turn.
2. An unrecognised `attack <absent-name>` ("I don't understand what you mean…")
   **does NOT advance the turn** — so a naïve attack-spam *freezes time* the moment
   the target leaves and nobody ever wanders back. **Fix: end every attack cycle
   with `z`** (a real wait that advances time). Absent-target attacks are then free
   no-ops and `z` lets a zombie wander back into range.

**Banked route (appended to `shadowpeak_solution.txt`, +40 → 130):**
- `s` into the Village Square (37); the named zombies oscillate 37↔38↔40↔41.
- **Phase A** (camp Square 37): 18× `[attack boris / attack holga / attack jarris / z]`
  → kills Boris, Holga, Jarris (Holga's death also spawns the garlic bud).
- **Phase B** (`e e` to the blacksmith/breastplate 40–41, where Arthur lives):
  16× `[attack arthur / z]` → kills Arthur. **Score 130.**

**Dead zombies persist as re-attackable "zombie villager" NPCs that keep wandering**
(their +10 only fires once, so 130 is legit; re-attacks hit the corpse object →
"I don't understand what you want me to do with the dead X zombie", a no-op).

**FOOTGUN re-confirmed the hard way:** I first wrote an inline `#`-prefixed prose
comment block above the kill phase; words like "east"/"down"/"turn" in it were
parsed as commands and desynced the route (130 → 110). Inline comments with ANY
direction/verb token break the timing — keep docs in THIS .md, commands-only in the
solution file (the header already warns this).

**zombie mob (room 51):** reached from the Square `s`→42 `e`→43 then **`se`** (the
rotated compass: at room 43 the dump's NW→51 is typed `se`; `sw`→52, `w`→42). It is
stationary, dies in ~7 `attack zombie mob`, but **grants no points** (task 109 just
drops the teeth obj15 + nametag obj59 into room 52 for a later sub-quest) — so it is
deliberately **left out of the banked solution** for now.

**NEW TOOLING:** `SC_DUMP_BATTLE=1` (scdump.c, under `-DSCARE_DUMP_TOOLS`) appends a
`BATTLE npc=N Stamina=lo-hi Strength=… Accuracy=… Defense=… Agility=… attitude=…
speed=… killedTask=…` line per NPC to the `SC_DUMP_TASKS` dump. Battle regression
(`scproj_regress.sh`) still PASS; no engine code touched.

**Next (still NOT banked):** the rest of the village cluster — `examine houses` +10,
church `put bottle on altar` +10 (room 49, bottle from inn 44), Madam Evenov fortunes
(+5×4, rooms 54–56, needs the silver coin), wolves `give leg of lamb to wolves` +10
(room 53), the giant spider +30 (T122), Bambi (spawn via the inn wooden box, room 45).

## ✅ SESSION 5 (2026-06-27): 50 → 90/790 — the stone building, Grumski, ring & flight

Deterministic route extended from the basilisk cave (room 34) all the way across
the chasm into **Windthrush village (room 36)**. Banked **+40** (50→90). All in
`harness/shadowpeak_solution.txt` (command-only). Beats, in order:

1. **Reach the stone building (room 119).** From the basilisk cave the route is
   `34→24→19→13→10→3→0` (`e e se se ne n`) → gap/plains/chasm `0→33→110→113→117→118`
   (`e e e d d`). Room 118 "bottom of the chasm" has a tomb whose doorway is a
   **stone slab** ("Let he who enters be of knightly kin…"). `open slab` then `e`
   enters — we *are* knightly (we hold Seeker), so no death. A stone wall drops
   behind you, sealing the exit until the wheel is turned.
2. **Sarcophagus + the wraith.** `open sarcophagus` → reveals the **vial** AND
   spawns **Alam Nadeer** (the wraith, NPC 30, ref v3=32). `take vial`.
3. **Open the secret passage (+10) and the exit (+10).** The iron handle is in the
   ceiling: you must be **standing on the sarcophagus** to reach it (restriction
   type=3 v2=4 = "player standing on object"). `stand on sarcophagus` → `turn
   handle` (**T399 +10**, opens 119 N→121). `n` to the **Secret room (121)**,
   `turn wheel` (**T401 +10**, runs "reach sky" → moves the slab, clears 119 W→118).
   `s` back, `w` out to 118.
4. **Vial → fairies (+10).** Climb back to the giant cave entrance (room 116):
   `118→117→113→114→115→116` (`u u n n e`). `open vial` near the **Fairies** (NPC 28)
   → it explodes, the fairies vanish, **Grumski is freed** (**T381 +10**).
5. **Grumski moves the boulder.** Trek back to the **basilisk cave (34)**:
   `116→115→111→110→33→0` (`w sw s w w`) then `0→3→10→13→19→24→34`
   (`s sw nw nw w w`). **Do NOT `shout grumski`** — Fetlar (your companion) sits on
   his head and farts, driving Grumski off. Instead **wait ~6 turns** (`z`×6):
   Grumski wanders to room 34 on his own, and when he meets Fetlar there it is
   **Fetlar** who flies off (Grumski stays). Then `say move boulder` (T365) → he
   pushes it aside, opening 34 W→35.
6. **The ring.** `w` to **the cave end (35)**. The **large wooden chest** is locked
   (small keyhole): `unlock large wooden chest with brass key` → `open …` →
   `take all` (copper ring + rubble + triangular metal piece) → `wear ring`.
7. **Fly across the chasm (+10).** `say fly`/`fly` is a **ROOMLIST_SOME_ROOMS**
   task (where=2) — it only fires from a chasm-overlook room, **NOT** underground.
   Go back to **room 4 "Chasm edge"**: `35→34→24→19→13→10→3→4`
   (`e e e se se ne se`) → `fly` (**T44 +10**) → lands at **Windthrush (room 36)**,
   also grants the **mace** (obj187). Score **90/790**.

**Next (room 36, NOT yet banked):** Lazaraz (NPC 41) is in room 36 — flee `s` to the
Village Square. Remaining easy points: the village NPC "(hehe) when X dies" kills
(Jarris/Arthur/Boris/Bambi/Holga +10 each), `put bottle on altar` church +10,
Madam Evenov fortunes (give coin + one/two/three/four, room 56), wolves
(`give leg of lamb to wolves` +10), spider +30, plus the long endgame (Morac,
Hell realm, `blow horn` win). **The Alam-Nadeer +20 (T393) is still open** — he is a
Battle-System wraith (his death runs T393); `kill`/sunlight didn't fell him, deferred.

> **STARTED 2026-06-27.** Foundation laid + a deterministic route banked to
> **50/790** (opening + early logistics + Rossi pacified + basilisk). The full win
> is a genuine multi-session derivation (574 tasks, ~125 used rooms across multiple
> portal-linked realms, 41 death endings).
> Run: `sh harness/play.sh games/"Shadowpeak.taf" harness/shadowpeak_solution.txt`
>
> ⚠️ **The solution file is COMMAND-ONLY** (docs live here, not inline): the SCARE
> parser extracts direction/verb tokens from *any* input line, so a prose comment
> mid-route fires spurious moves and desyncs Rossi's turn-based theft timing. (Plain
> unparseable lines are harmless — they print "I don't understand" and do **not**
> advance game time — but a comment containing e.g. a bare `s`/`take`/`up` does.)

*Shadowpeak* — a sword-and-sorcery ADRIFT **4.0** game (native 4.0, header byte8
`0x93`/byte10 `0x3e`; no name/gender prompt — you are **Loralang**). **574 tasks,
194 objects, 50 NPCs, 157 events, ~125 used rooms, max score 790.** Derived via the
deterministic headless SCARE harness.

## Premise / win condition
King Henrious sends Loralang, his champion knight, **back in time** (naked — the
time-spell only works on a naked body) to **retrieve the golden sceptre of
rulership and destroy the undead lord Morac** before Morac destroys it. A previous
emissary, the wizard **Borantha**, was sent first and died (his burnt body + cracked
staff lie at the start). The creature **Fetlar** (summoned by Borantha) helps you.

- **WIN = task 417 `blow horn`** (`where=3`, no fixed room) with **3 restrictions**:
  1. holding **The horn of the angels** (obj147 — task-revealed, starts Hidden),
  2. **in the same room as Asmodeus** (NPC 39 — the final boss in a **Hell** realm:
     Cerberus / Charon / lost souls / Devils / Asmodeus / Lazaraz are NPCs 36–41),
  3. holding the **sceptre** (obj112 — task-revealed, starts Hidden).
  → `ACT type=6 v1=0` EndGame win. **Max 790** over 69 ChangeScore tasks (many are
  NPC kills); **1 win, 1 lose, 41 death endings.**
- **Morac** is a scripted **"seeker"** (chasing-enemy) encounter — tasks
  `#Moracyboyarrives` / `#Morackillsplayer` / `evade morac` / `examine morac` /
  `kill morac` (+50). Your starting broadsword is even *named* "Seeker".

## Key mechanics (from the in-game `hint` list + the dump)
- **`hint`** — the author's built-in, **context-sensitive** puzzle guide (shows only
  hints relevant to your current area; consult it whenever stuck). **`status`** /
  `<name> status` for combat HP. `wield <weapon>`, `kill <name>`, `ask <name> about
  <x>`, `say <x>`, `x`/`examine`.
- **Fetlar** (NPC 0) is a flying companion who **follows you** room-to-room; he
  *reads the henge runes* and *likes the egg* (trade it for the sword).
- **The Borantha staff:** *"once the name Borantha is spoken in the ring of stones,
  the staff is ready to use"* → `say borantha` repairs the staff; then **`snap
  staff` (+50, task 358)**. (Endgame-flavoured; both at Stonehenge, room 0.)
- **Dragon = evade** (too powerful to fight). **Lazaraz = run away** (don't speak).
- **NAVIGATION FOOTGUN:** the game has **severe compass rotation** AND the SCARE
  room *indices ≠ display names*, and a room's in-game offered directions don't
  match the dump's exit slots. **Navigate purely by room prose / "you can move…"**,
  not by the dumped N/E/S/W. (Verified room-name map for the opening is below.)

## Realm 1 — Shadow Forest (Stonehenge hub), rooms 0–~30
Verified room names: 0 Stonehenge (START) · 1 Trees · 2 Up a tree · 3 Lightning
tree (**Fetlar**) · 4 Chasm edge · 5 Swamp edge · 6 Leaning tree (**magpie nest**)
· 7 The ledge · 8 Fallen ledge · 10 Swamp over chasm · 11 Edge of great swamp ·
12/13 Great Swamp.

**Banked route (deterministic, 50/790):**
1. From Stonehenge `s`→3 Lightning tree, `se`→4 Chasm edge, `u`→6 Leaning tree.
2. `examine nest` (reveals its contents), `take egg`, `take medallion` (a
   chain-and-medallion — it is the **basilisk-mirror**, see below; keep it).
3. `d`→4, `nw`→3 Lightning tree (Fetlar is here). **`give fet egg`** → Fetlar eats
   it and gives you the broadsword **"Seeker"** (+10).
4. `n`→0 Stonehenge (Fetlar follows). **`fet read runes`** (+5) — the runes pose a
   riddle: *"What is the only thing you can be sure to achieve in your life?"*
5. Answer **`death`** (+5) — the henge hums and a **Shield** appears. `take shield`.
6. **`hide from dragon`** (+10) — banks the dragon task & makes Shade "move on"
   (safe to do here at the henge; see DRAGON below). **Score 30/790.**
7. **Logistics loop (gloves → brass key → rose).** Gloves: `n`→1 Trees,
   `climb up`→2, `take gloves`, **`wear gloves`**, `d`→1, `s`→0. Brass key:
   `e`→33, `n`→32, `n`→31, `w`→30 log room, **`feel inside`** (gloves protect; the
   key falls out), **`get brass key`** (works now — the sclibrar.c ambiguous-name
   fix). Rose: `n`→50, **`pick flower`** (the Rossi pacifier).
8. **Return + pacify Rossi.** `s`→30, `e`→31, `s`→32, `s`→33, `w`→0 henge. Rossi is
   lying in wait here (his fixed walk parks him at the henge ~turns 30-39) and
   **snatches the sword** — the theft drops it on the ground in **room 12**. Then
   **`give rose to rossi`** (**+10, T382**) — he recoils at the perfume and is
   pacified *permanently* (all his thefts now require the rose NOT-given). **40/790.**
9. **To the Basilisk cave, recovering the sword en route** (compass scrambled — use
   the verified keystrokes): `s`→3, `sw`→10, **`w`→12** *(the dropped sword is here:
   `take sword`)*, `n`→13, `nw`→19, `w`→24 Cave entrance, **`wear medallion`**,
   `w`→34. Entering 34 with the medallion worn reflects the basilisk's gaze →
   *"turned to stone"* (**+10 → 50/790**). The basilisk becomes a stone statue
   (obj20). End inventory: sword + Shield + brass key + rose; medallion + gloves
   worn; Rossi pacified — i.e. **every ring→village prerequisite is now in hand.**

## Early-game hazards (the opening gauntlet) — derived 2026-06-27
Three roaming threats make the early overworld a timing puzzle. **Keep the opening
short and don't linger outdoors** until they're handled:

- **DRAGON (Shade, the black dragon)** — a roaming outdoor threat driven by
  EVENT 116 (`dragononthemove`; mechanics fully decoded 2026-07-02): random start
  delay **29–31 turns**, then a **4-turn flyover** ("spots you" / "goes into a
  dive"), and at the event **finish** it runs death task **T385** if you stand in
  T385's room group — **most of the outdoors, INCLUDING Stonehenge** (the old
  "henge is a safe haven" note is wrong: pure waiting there dies ~turn 33; it only
  looked safe because the banked route hides by turn ~6). The event **restarts**
  (~every 30+4 turns), each finish another death check, until task 348 `fire` is
  done (room 68, much later; all the dragon tasks gate on `fire` NOT-done).
  **`hide from dragon`** = **+10 (one-time, T386)** and **pauses EVENT 116
  permanently** (PauseTask 388 −2 = T386) — so bank it early.
  **`examine dragon` (+10, T387) IS obtainable but is MUTUALLY EXCLUSIVE with
  T386** (verified live 2026-07-02): T387 needs the invisible `#~dragon counter`
  (obj138) **HIDDEN** + T386 never done; the counter's only transition is one-way
  81→hidden at an EVENT-116 finish, and hiding pauses the event before it ever
  finishes. To bank T387 instead: never hide, shelter outside T385's room group
  during the flyover finish (basilisk cave 34 works; room 56 is the only T387
  room not in T385's group), then `examine dragon` outside → +10 with the full
  description. Score-neutral swap (+10 either way) that trades away the pause —
  you then live under the recurring flyover death-timer until `fire`.
- **ROSSI the muck dweller (NPC29)** — a **serial thief** on a **fixed turn-based
  walk** (starts at room 13; traced path lingers at room 14 turns ~3-12, sweeps the
  swamp 15-26, then parks at the **henge room 0 turns ~30-39**, then returns toward
  13). His theft chain is staff → medallion → egg → sword → shield (TASKs 376-380,
  EVENTs 110-113), but the staff/medallion/egg links are **dead** (we never take the
  staff, and each later link needs the previous done). The only **live** thefts are
  the **sword (T379)** and **shield (T380)** — each just needs Rossi co-located +
  the item held/worn + `give rose to rossi` not-yet-done. Practical handling (used
  in the banked route): he parks at the henge exactly as you return from the
  logistics loop and grabs the **sword**, which the theft's MoveObject (`var3-1`)
  drops in **room 12** (NOT 13). Immediately **`give rose to rossi`** at the henge
  (**+10, T382**, permanent pacify), then re-collect the sword in room 12 on the way
  to the basilisk. The **rose is NOT deep** — it's `pick flower` at room 50, one step
  off the log room 30 (~6 moves from the henge). After the rose, `shout rossi` (T392).
- **REEVLING the Warrior (NPC47)** — reacts to seeing the player / the sword
  (T548/T549; the `MeetObject` global-fixup case). Behaviour TBD; appeared at the
  henge alongside Rossi when we lingered.

## Clothing (you start NAKED) — locations mapped, NOT yet banked
Needed deeper (cold rooms; the medallion must be **worn** for the basilisk).
Collecting it in the opening is unsafe (gives the dragon+Rossi time to catch you),
so grab it on the way out *after* the rose, or while moving (never stationary):
- **boots + pants** — under the crushed villager at room 3 (Lightning tree):
  `roll oak tree` (+5, reveals the villager) → `move villager` (+5, reveals the
  boots & pants) → `take boots`/`take pants`/`wear …`.
- **gloves** — room 2 (Up a tree): N from henge→1 Trees, `climb up`→2, `take gloves`.
- **medallion** — already taken from the nest; **`wear medallion`** before the
  Basilisk cave.

## Basilisk (room 34) — medallion = the mirror — BANKED (+10)
Entering the **Basilisk cave (room 34)** with the **medallion worn** fires T41
(*Baz death*, **+10**, basilisk turns to stone) and you survive; **without** it
T40 = instant death. The **stone basilisk** (obj20) is then in room 34 but is *"far
too heavy to hold"* — you can't carry it (so `get basilisk` just drops it); it's
likely interacted with in place later. Verified keystroke route (see opening step
7): `s sw nw nw w` then `wear medallion` then `w`. (The Cave entrance→34 exit is
ungated — `move moss`/`say move boulder` are NOT required to pass.) The chasm to
the village across the gap needs the **flying ring** (obj25): `say fly` (T44, +10)
teleports to room 36.

### Great Swamp map (verified KEYSTROKES, via SC_TRACE_PLAYER)
The swamp rooms share prose; these are the *typed* directions (post-rotation), not
the dump's exit labels. Room 0 Stonehenge · 3 Lightning tree · 5 Swamp edge ·
10 Swamp over chasm · 12-26 Great Swamp · 24 Cave entrance · 34 Basilisk cave.
- **0**: `s`→3
- **3**: `n`→0, `s`→5, `se`→4, `sw`→10
- **10**: `e`→5, `n`→11, `w`→12, `ne`→3, `nw`→13
- **13**: `n`→14, `e`→11, `s`→12, `w`→20, `ne`→27, `nw`→19, `se`→10, `sw`→21
- **19**: `n`→18, `e`→14, `s`→20, `w`→24, `ne`→15, `nw`→25, `se`→13, `sw`→23
- **24** (Cave entrance, statues+sign): `e`→19, `w`→34
The full EXIT-table connectivity for ALL rooms is in `/tmp/sp_dump.txt` (lines
`EXIT room=…`) — but its N/E/S/W labels are rotated vs keystrokes, so always
re-derive the keystroke per room with SC_TRACE_PLAYER.

## Map of the realms (derived 2026-06-27 via debugger `room N` + STATIC dump)
- **Realm 1 / Shadow Forest + Great Swamp (rooms 0–35):** henge 0 · forest 27–33
  (gap 33 is `e` from henge) · swamp 10–26 · Cave entrance 24 · **Basilisk cave 34**
  · **cave end 35** (behind the boulder). Druids grove **50** (the rose) hangs off
  room 30 (`s` from the log room).
- **Realm A / Windthrush village (36–52):** Village Square 37, well 38, Shop 39,
  Residential 42 (`examine houses` +10), Inn 44/47, church 48/49 (`put bottle on
  altar` +10), graveyard 51, fields 52. Entered by **flying** (`say fly`, ring) →
  room 36. Also family crypt 100, battlements 106.
- **Realm B (101–109):** tower 101, magical prison 102 (`kiss lady` +10 → key),
  iron steps 104/105, window ledge 109.
- **Realm C / Grassy plains (110–127):** plains 110–115 (walk **`e` from gap 33`**),
  **giant cave entrance 116** (Grumski + the fairy/vial subquest T381 +10), chasm
  footpath 117, bottom of chasm 118 (`examine stone building` +10), stone building
  119, secret room 121, battlefield 125.
- **Realm D (128–138) + Maze (139+):** long corridor 128, crawl way 129, crossroads
  130, long passage 133–135, pedestal 136, multicoloured room 137, behind wall of
  fire 138.

## THE RING → VILLAGE CRITICAL PATH (fully decoded 2026-06-27)
The village (Realm A, a dense scoring cluster) is reached by **flying**: wear the
**copper ring** (obj25) and `say fly` (**T44, +10**, also gives a **mace** obj187)
→ teleport to room 36. The ring is locked inside the **large wooden chest (static
obj23) in room 35 "cave end"** — *behind the boulder* in the Basilisk cave. The
chain:
1. **Gloves** — henge `n`→1 Trees, `climb up`→2, `take gloves`, **`wear gloves`**.
2. **Brass key (obj24)** — to the **log room 30** (`e` henge→33 gap, `n`→32, `n`→31,
   `w`→30) and **`feel inside`** (T49; needs gloves worn) → *"a brass key falls out
   of the log"*, then **`get brass key`**. ✅ **UNBLOCKED 2026-06-27** (see below).
3. **Grumski the giant** moves the boulder: `say move boulder` (**T365**) at room 34
   requires **Grumski (NPC29) present** + basilisk already stone. You summon him with
   **`shout grumski` (T391)**. Corrected gate analysis (restr polarity decoded from
   the dump: task-restr `v2=1` = *must be UNdone*, `v2=0` = *must be done*): **T391
   requires T381 DONE** (the fairy/vial subquest at the giant cave room 116) **AND
   T44 `say fly` not-yet-done AND T365 `say move boulder` not-yet-done** — i.e. you
   summon Grumski BEFORE moving the boulder/flying, *after* finishing the fairy quest.
   Room 116 is reached by walking **`e` from the gap (33→110→…→116)** across the
   grassy plains. (T365 then warps Grumski to room 123.) After the boulder moves,
   room 34 `w`→35 cave end.
   - **The FAIRY/VIAL subquest (T381) decoded 2026-06-27 — the vial is GUARDED.** The
     **vial (dynamic obj135) starts INSIDE the sarcophagus (static obj137) in room
     119** (debugger `objects 135` = *"Inside Static 137 sarcophagus"*). Opening the
     sarcophagus (T388) spawns **Alam Nadeer (NPC30/"a lamnadeer")** — a **"seeker"
     combat** enemy (T389 moves him to room 120; T390 `#~seeker alam nadeer` needs
     the **sword** obj10 — this is why we keep Seeker!; T393/T394 = his death/near-
     death). So: get to room 119, open sarcophagus, kill Alam Nadeer with the sword,
     take the vial, carry it to room 116, **open the vial near the Fairies (NPC28)**
     → T381 +10. **Plains topology:** gap 33 `e`→110; 110→{111,113,114}; **115 = the
     Fairies/giant-cave room, 115 `e`→116**; the sarcophagus is down the chasm —
     113 `d`→117, 117 `d`→118, **118 `e`→119 (gated by gateTask 27 — decode next)**;
     118 `s`→126. Room 116 `e`→122 only opens AFTER T381 (gateTask 381).
4. **Open chest** with the brass key → **take ring** → **wear ring** → **`say fly`**
   (+10) → room 36 Windthrush. Then score the village.
- The **rose** for pacifying Rossi is *close*: **room 50** (`pick flower`, T103)
  hangs directly off the **log room 30** (`s`). `give rose to rossi` = **T382 +10**
  and stops the serial thefts (the doc previously thought the rose was "deep past
  the swamp" — it is NOT; it is ~5 moves from the henge). `put fet on flowers`
  (T106 +10) needs Fetlar *dead* first (a separate subquest).

## ✅ RESOLVED: the brass-key blocker (SCARE command-dispatch divergence) — fixed 2026-06-27
**Fixed** in sclibrar.c (commit `0dfc46e8`): the library get/drop/wear/remove
override retry now **skips the bare-name attempt when the object's Short name is
ambiguous** (shared by ≥2 objects) via `lib_object_short_name_is_ambiguous()`.
Three objects are named "key" here (brass/silver/iron), so the bare "get key" is a
disambiguation case the Runner would never match — the fully-qualified
`get prefix name` retry still fires legitimate single-object overrides. The real
Runner (`run400.txt`) never reconstructs a bare noun: its standard-take handler
(Sub @≈0x7B6xx) takes the resolved object directly, and `uip_match` (already
faithful) does not match literal "get key" to "get brass key". **Verified:** brass
key now takeable; all **37 banked ADRIFT walkthroughs byte-identical** before/after;
`scproj_regress.sh` passes; headless SCARE test suite passes. **Verified end-to-end
early route** (`/tmp/sp_early.txt`): henge `n`→1 `climb up`→2 `take gloves`
`wear gloves` `d` `s` (→0) `e n n w`→30 `feel inside` `get brass key` `n`→50
`pick flower` ⇒ inventory holds *gloves (worn) + brass key + rose*, no theft.

### Original root-cause notes (for reference)
After `feel inside`, the brass key lies in room 30, but
**no `get/take … key` command can pick it up** — every such command prints the
author's *"Which one? (e.g. silver key, iron key, brass key, etc.)"* and the key
stays on the ground. (That message is hand-written game text from **TASK 512
`get key`** — a generic catch-all disambiguation task, `where=anywhere`, no
restrictions; it even lists a nonexistent "iron key".)
- **Why it fires:** SCARE's library get-handler (`lib_try_game_command_common`,
  sclibrar.c) resolves "get brass key" to the brass-key object, then retries game
  tasks with BOTH the full name *and the bare object noun*: it calls
  `run_game_task_commands(game, "get key")`. That **bare-noun fallback** matches the
  generic Task 512, which "handles" the command (prints text) and so **aborts the
  physical take**. Confirmed with `SC_TRACE_MATCH=1`: `MATCH task=512 pattern=[get
  key] input=[get key]`. The author clearly intends Task 512 only to catch a *bare*
  ambiguous "get key" and prompt the player to be specific ("get brass key"); the
  bare-noun callback defeats that design and makes the ring (and thus the village)
  unreachable.
- **NOT a synonym issue** (the game has only `x`/`search`→`examine`). **NOT** the
  `uip_match` pattern matcher (a literal "get key" does *not* match "get brass key").
  It is specifically the library get/drop **bare-noun retry**.
- **Fix is deferred — it touches core command-dispatch used by EVERY corpus game's
  take/drop**, so it must be (a) checked against the real ADRIFT Runner behavior
  (does the Runner's get-handler also try the bare noun? — see `~/Desktop/run400.txt`
  P-code) and (b) regression-tested against the whole Quest/ADRIFT corpus
  (`harness/scproj_regress.sh`) before any change. Candidate fix: when the get/drop
  bare-noun retry matches a task that is a pure message (no state-changing action),
  do not treat it as overriding the physical take. **Until fixed, the ring/village
  branch is unreachable in SCARE and the opening stays banked at 40/790.**

## Tooling added this session (scdump.c / scrunner.c, harness-only, `-DSCARE_DUMP_TOOLS`)
- `SC_DUMP_TASKS` now also prints **`STATIC obj=N [name] rooms=…`** (every room a
  static object is in — the only way to pin a locked chest/scenery to a room; that's
  how the ring-chest was found in room 35), **`ALTCMD[i]=[…]`** (a task's extra
  command synonyms beyond Command[0]), and **`SYNONYM [orig] -> [repl]`** (the input
  rewrite rules applied before matching).
- **`SC_TRACE_MATCH=1`** prints `MATCH task=… pattern=[…] input=[…]` whenever a task
  command pattern matches the (possibly library-rewritten) input — the tool that
  pinned the Task-512 over-match above. All gated under `#ifdef SCARE_DUMP_TOOLS`;
  the Spatterlight build never compiles them.

## Resume / next steps (NOT yet banked)
- ✅ **Brass-key blocker RESOLVED** (committed `0dfc46e8`); early logistics
  gloves→key→rose verified end-to-end. Ring→village branch is now mechanically open.
- **Next concrete chunk (the genuinely multi-session part):** integrate a single
  coherent solution that (a) keeps the proven 40-pt opening, (b) gets the rose early
  and **`give rose to rossi` (+10, T382)** to stop the serial thefts — needs Rossi
  *present* (he roams; find his trigger/where he appears), (c) crosses the grassy
  plains `e` to room 116, does the fairy/**vial subquest** (T381 +10 — need the
  **vial obj135** *open* near the fairies; locate the vial first), (d) `shout grumski`
  (T391) then `say move boulder` (T365) at room 34, (e) `w`→35, **open chest with the
  brass key** → take ring → wear ring → **`say fly` (+10)** → room 36 village, then
  the village scoring cluster (`examine houses` +10, church `put bottle on altar`
  +10, madam evenov fortune one/two/three/four +5 each, wolves T115/T116 +40, …).
- **Sword retention:** the opening sword "Seeker" (obj10) is needed for the endgame
  seeker battles (T418/T390); don't let Rossi steal it on the longer route — pacify
  him with the rose *before* lingering, or re-verify he never reaches the forest path.
- The Stonehenge **portal** (`enter portal`, T291 +5, room 100) links to the **crypt
  realm**; gated on **T288** = open coffin #11 (obj125) in the family crypt.
- Big-ticket scoring later: the many NPC "(hehe) when X dies" kills, `snap staff`
  +50 & `kill morac` +50 (endgame), the Hell-realm finale (Cerberus/Damastus/
  Asmodeus) gating the horn + `blow horn` win.
- **Method:** structural dump `SC_DUMP_TASKS=1 ./harness/scare Shadowpeak.taf`
  (`/tmp/sp_dump.txt` — read with `grep -a`, it has non-UTF8 bytes!) — this dump
  ALSO contains the full **`EXIT room=…`** connectivity table for every room.
  Object/NPC locations via `SC_DEBUGGER_ENABLED=1` + `objects/npcs/rooms`; consult
  in-game `hint`. **`SC_TRACE_PLAYER=1`** (new, in scdump.c) prints `PLAYERROOM
  room=N` once per turn — the reliable way to map the scrambled compass: feed a
  command sequence and read the resulting room. (`SC_TRACE_JUDY` does the same per
  NPC; Fetlar lags a turn, so prefer SC_TRACE_PLAYER.) The debugger `room N` prints
  only the NAME (no exits).

## Faithfulness notes
- Native 4.0; the `MeetObject` dynamic→global fixup was previously confirmed
  correct for this game (Reevling's "sees sword" task; see `WALKTHROUGH_TODO.md`).
  No SCARE change; no combat-assist (combat appears properly configured —
  `status`/`kill` work).

## SESSION 11 (2026-06-27): 245 → 255/790 (BOOTS banked) + pentagram fully diagnosed

**✅ BANKED 255/790 — the thick leather boots (+10), with an RNG-robust combat re-tune.**
The boots (obj5) are under the dead villager at **room 3** (the lightning-tree clearing).
On the way back from the basilisk cave to the chasm edge the route already passes through
room 3, so we insert there: **`roll oak tree`** (T5 +5) → **`move villager`** (T18 +5,
needs T5 done) → **`take boots`** → **`wear boots`**. The boots are mandatory later for the
castle cellar (room 85 rats = instant death unless worn).

**The boots detour costs 4 turns that shift the village-combat RNG** — verified this
desynced Phase A so Boris and Jarris stopped dying (235, net −10). **Fix = a self-syncing
single-target focus loop** instead of the old camp-and-attack-all-three loop. The named
zombies oscillate in/out of the Village Square (37) every turn; **`attack X; z`** does
nothing (no turn passes) when X is absent and only the `z` advances time, so we step one
turn at a time and land a hit whenever X wanders in. Order: **Holga ×12, then Jarris ×18**
(100 stamina = many hits), **then Boris ×24** (he roams in slowly from room 42). This is
robust to the RNG shift and re-banks the full +40 zombie cluster. Deterministic (255 on
repeated runs); battle regression `scproj_regress.sh` PASS; no engine code touched.

**★ PENTAGRAM (+15, not +25) — fully decoded, but BLOCKED by the Morac death-timer.★**
The restructured castle route works end-to-end *up to* the pentagram and was verified
piece by piece, but cannot yet be finished within the Morac window. Findings:

- **Gargoyles are killed IN ROOM 71**, not the dining room. Entering 71 wakes them; on the
  *next* turn they "spring to life" and attack you in 71 (they do NOT pre-teleport to 72
  on a fresh entry — that only looked true in earlier sessions because session-9 had
  already activated them). Recipe: `s s s` (→71) `z` (let them wake) then **`attack
  gargoyle #1` / `#2` / `#3` / `#4`** (specific # targets — bare `attack gargoyle` only
  prompts "which one?"). All four shatter. Killing them early removes their room-to-room
  chase (left alive they whittle your stamina down and kill you over ~25 turns).
- **The chisel** (obj91) sits on the cellar rubble (room 86), put there by FrontDoorsSlam
  (T162) — NOT by the behead. The cellar is **dark** (room 83 east exit gated on T215 =
  lantern *held + lit*). Get the **lantern** (obj90, room 79, via 71→77→78→79) and
  **`turn lantern on`** (needs the tinderbox). Then 73→84→83→85(rats, boots protect)→86,
  `take chisel`.
- **The Melvin behead (T333)** fires when **gargoyles dead + painting #6 destroyed +
  Melvin met** (Melvin is in **room 78**, met en route to the lantern). It teleports you
  to room 107 and **strips ALL carried items to the Storage room (room 81)**. Escape:
  enter 78 → behead → 107, then `z` (one turn for Melvin to speak) → **`say sorry`**
  (T337) → **`say yes`** (T338) → back to room 71. Recover with `take all` at room 81 and
  **re-`wear boots`** (worn items get stripped too). rep=0, so it never refires.
- **The cat (Shadow, obj100, room 93) needs the sword** — bare-handed always misses it
  ("Shadow manages to avoid your attack"). Killing the cat also fells the witch
  **Rucktebar**, whose `#Rucktebargetsnasty` (T240) instakill otherwise catches you ~6
  turns after you first enter room 93 (she follows you out). The **scroll** (obj102, room
  94, north of the cat) can be grabbed by dashing past the *live* cat, but then the witch
  clock is running.
- **★ THE PENTAGRAM CHIP ORDER (corrected — session-10's note was WRONG) ★** In room 77
  (Zandos the demon is entrapped in the pentagram): the obvious `chip pentagram with
  chisel of the ages` matches **T221**, whose **`ACT type=5 v1=0 v2=222` is a REDIRECT
  that *RUNS* task 222** (`#zandosbattersplayer`, ACT type=6 = death) — i.e. **the first
  chip is a DEATH TRAP** ("Zandos: Free, free at last! … plucks your head"). Verified in
  `sctasks.c task_run_set_task_action`: type=5/var1=0 redirects-forward (runs the task);
  only var1≠0 undoes a task. So **the safe order is `say nomorey palenvoid` FIRST**
  (T243 +5; needs the scroll held + Zandos present; "Zandos suddenly backs off … the
  scroll burns away"), **THEN `chip pentagram with chisel of the ages`** (now matches
  **T244** +10, ACT type=1 moves Zandos away — safe). T221's +10 is unreachable without
  dying, so the pentagram is worth **+15** (T243 +5 + T244 +10), **not +25**.
- **★ THE BLOCKER: the Morac death-timer (EVENT 92 → T327 `#Morackillsplayer`, ACT type=6,
  ~40–50 turns).** It starts somewhere early in the castle prep (it killed us while merely
  *waiting* at the post-behead Storage checkpoint — "Morac spots you … rips out your
  throat" — even with the sword dropped and never carried into room 77; it does NOT fire
  while waiting at the pre-castle checkpoint room 63). By the time the prep (gargoyles +
  lantern + chisel + behead + recovery) and the final sword burst (pick up sword → 77 →
  cat → scroll → back to 77 → say-nomorey → chip) finish, the timer has expired — we get
  `say nomorey` (+5 → 260) but Morac kills before the T244 chip. **Next session: (a) pin
  down EXACTLY what starts EVENT 92 (task 193 needs sword-held-in-77, which we avoid — so
  something else triggers it; suspect task 192 `zandysayshi` = merely being in room 77
  with Zandos, no sword needed) and whether it can be paused/reset; (b) shave prep turns
  (the behead+recovery is ~15 turns of overhead) to fit the whole castle sequence inside
  ~40 turns; or (c) accept the death-ending if a death still banks the +15.** The full
  restructured tail (gargoyle-in-71 → drop sword → lantern → chisel → behead+recover →
  scroll-dash → sword burst → say-nomorey → chip) is captured in the harness scratch files
  and reproduces every step except surviving Morac.

## Session 15 (2026-06-27): DEEP DUNGEON — 355 → 455/790 (+100)

The deep dungeon (rooms 128–138) is a self-contained gauntlet entered from
forest **room 16 "Doors in the mountains"**, appended to the solution after the
boulder avalanche. Walk back from the avalanche (room 124) to Stonehenge
(`w w w w s sw w w` — verified typed dirs, compass rotated) then to room 16
(`w`→27, `nw`→15, `n`→16).

**Entry — the double doors (obj149).** They are *not* a task gate; they are a
plain library lockable whose key is the **triangular piece of metal (obj150)**
we already carry from the cave-end chest — NOT the prison's silver key (that
opens the prison cell). New scdump tooling (`LOCKKEY obj=.. keyobj=..`, dumps
every lockable's `Key` property → real object id) pinned this immediately;
by-elimination guessing in session 14 had assumed the silver key. We still grab
the silver key during the crypt visit (`97 s`→98 prison, `take silver key`,
`n`) in case the cell/Margo sub-quest is wanted later. `unlock double doors with
triangular piece of metal`, `open double doors`, `n`→128 (the entrance then
seals behind a sliding stone block).

**Room 128 (stone-block crusher).** A golden chest is bolted to the east-wall
fresco with a **lever inside it**. `open golden chest`, `pull lever` (opens a
crawl-panel in the east wall), `crawl` → **T432 +10**, escaping east to 129 just
as the block blocks the panel.

**Room 130 "the crossroads" — Edna the flesh construct.** The TRUE exit is
**north**; east and south are instant spike-pit deaths (`T440`, fired every turn
by EVENT 129 only when you're standing in a wrong room). Edna (Stamina 93,
Defence 25–30) must be killed in melee for the **+20 (T436)** and to drop the
**fleshy mound (obj155)**. Two complications: (1) a single speed-1 gargoyle has
trailed us since the castle (we left gargoyle #1 alive to suppress the Melvin
behead — killing it here STILL fires the behead → teleports to room 107, so it
must stay alive) and it chips our very low Stamina (~13/50); (2) the seeded
battle rolls at Edna's death sometimes force-move us onto a lethal passage. Fix:
`sleep` first (Strength 15→25 = fewer hits = fewer gargoyle chip turns) and pad
with **4 `z` waits** to align the rolls so her death is clean. Sequence:
`e`, `z z z z`, `sleep`, `attack edna`×8, `take fleshy mound`.

**Room 133 "the long passage" (copper plates).** The plates electrify the way
north; **`throw flesh`** (NOT "throw fleshy mound …", which the library
intercepts) shorts them out → **T443 +20**. Then `n`.

**Room 134 "midway" (slits).** Dozens of wall slits spell *Ernamadlea* = "alam
nadeer" reversed. **`say alam nadeer`** → **T446 +20**; otherwise the plates fry
you on the next step. Then `n`→135.

**Room 135 "north end" (Olmo the genie).** A glowing lamp sits on a pedestal —
**DO NOT take it** (instant death, T447). **`rub lamp`** → **T448 +5**, summons
Olmo, who offers wishes a/b/c. Wish **b** ("a blessing from the gods … to stop
the devil's seed") is the helpful one → **`say b`** **T451 +5** and opens the way
**up**. (Wish a = meet Morac prematurely; wish c = teleport back to the henge.)
`u`→136, `n`→137.

**Room 137 "multicoloured room" (rainbow tiles).** Step the tiles in the nursery
-rhyme order *red, yellow, pink, green, purple, orange, blue* ("I can sing a
rainbow"). Any wrong colour is instant death; the final **`blue`** = **T460 +20**,
drops the wall of fire, and moves you to **room 138**. The walkthrough stops here
(deterministic 455, 0 deaths, re-verified ×2).

**Beyond (next session):** 138 `n`→139 is the maze leading to the Hell realm —
buttons in rooms 143/145/151/157 (T480-483) → all-buttons **T484 +10** (also needs
**Damastus dead**, NPC37, T486 **+25**), then **`catch the wind`** at room 165
(**T488 +15**) which teleports back to room 16. Charon/Cerberus (rooms 169/170,
T490 Cerberus +20, T503 give gold coin to Charon) and the Morac/Asmodeus finale
follow. Engine change this session: scdump `LOCKKEY` dump only (harness-only,
`-DSCARE_DUMP_TOOLS`); scproj_regress PASS + headless `make test` PASS.

## Session 16 — the maze (rooms 139-165), 455 → 505/790 (+50)

The deep-dungeon maze (rooms 139-198) is, unlike the rest of the game, **NOT
compass-rotated**: typed directions equal the map slots, so navigate by the EXIT
table directly. It contains four buttons, the roaming minotaur **Damastus**, and
the exit to the mountain top.

**The Stamina blocker (the whole reason this was hard).** The gargoyle #1 we keep
alive (to suppress the Melvin behead) follows the player *everywhere* and slowly
drains Stamina; by the maze the player is pinned at ~2-8 Stamina with only a slow
~1/10-turn regen, so the long maze (with the Damastus fight) kills you. Killing
the gargoyle is NOT an option — the behead (T333, fires anywhere once all four
gargoyles are dead + painting #6 + Melvin met) hides the *entire* inventory
permanently (verified: items go to "Hidden", unrecoverable). The only reachable
heal is the **venison** (obj82): take it off the dining-room table in **room 72**
during the castle cellar run and carry it; `eat venison` (T189, where=3 so it
works anywhere) gives **+10 Stamina** then teleports the venison back to room 72
(one heal only). Eat it right before the Damastus fight. Taking the venison adds
one turn upstream, which **desyncs the RNG-tuned Edna fight** — re-tuned its waits
from 4 → 3 (counts 0/1/2/3/5/7/8 are all clean at that position).

**Damastus (NPC35, "the minotaur").** Roams rooms 154-162, moving every 2 turns;
throws a spear. He is safe to melee *only because we are blessed* (`say b` to Olmo,
done in the deep dungeon): task 493 ("seeker damastus") otherwise kills a Seeker-
wielder co-located with him. He dies in **5 hits → T486 +25**. Because he wanders
and **combat consumes RNG** (so his path can't be precomputed from a wait-only
trace), the kill is a **greedy chase**: each turn, attack if co-located, else step
toward him (BFS on the maze graph). The derived sequence for this exact turn count
is `attack, s, attack, s, attack, n, attack, s, attack` (ends at room 155) — do
not insert/remove upstream turns without re-deriving it (`/tmp/chase.py` method:
drive the seeded binary repeatedly, read SC_TRACE_PLAYER + SC_TRACE_JUDY npc=35,
greedily extend until "+25").

**Route (all in `harness/shadowpeak_solution.txt`).** From room 137 (rainbow):
`n n`→139. Iron button (143): `e e e`. Copper (145): `w w w w w w`. Stone (157):
`n n n n n n`. `eat venison`. Damastus chase (above) → he dies at room 155. Bronze
(151), the 4th button: `n n e e e e e e`. With all four pressed AND Damastus dead,
the "all buttons" task fires (**T484 +10**, unsealing 160↔163). Exit/up: `w w w s`
→163, `u u`→165, **`catch the wind`** (**T488 +15**) → magically lifted back to
room 16. Deterministic 505, 0 deaths, re-verified ×2; ends at room 16 "Doors in
the mountains" with Stamina 15 (variant that eats venison just before the fight —
a no-venison-eat-late variant lands the same 505 but ends at Stamina 1).

**Next session (17): the Hell realm + finale.** From room 16, head to the Hell
rooms 166-173 (Charon ferryman, `give gold coin to charon` T503 / `whistle` T501;
**Cerberus T490 +20**; `touch picture` room 169 T515 golden door). Then the
Morac/Asmodeus endgame: snap staff +50/+20, kill morac +50, attack morac with
sceptre +10, fire ballista +20, **blow horn near Asmodeus = WIN (T417)**. Upper
castle (kiss lady +10, carom +5) is still Morac-gated/circular. **Carry the
Stamina problem forward** — the gargoyle still follows; venison is spent. No
engine change this session (walkthrough-only); scproj_regress PASS + headless
`make -f Makefile.headless test` PASS.

---

## SESSION 17 (2026-06-27): FULL FINALE DECODED — 0 banked (505 held), access blocker found

A research/de-risk session (like 8/10/14). The win path is mapped end-to-end; the
canonical `harness/shadowpeak_solution.txt` was **not touched** (re-verified 505/790).
All probes ran on scratchpad copies of the solution.

### The win entrance
`snap staff` (T358, room 0 Stonehenge, **+50**) has `ACT type=1 v1=0 v2=0 v3=166` —
it teleports the PLAYER into the HELL realm (room 166). Gated on `kill morac` (T350)
AND `say borantha` (T360) both done. The whole ending funnels through the Morac fight
→ Stonehenge → snap staff → Hell → `blow horn`.

### Upper castle is NOT permanently circular (session 14 was wrong)
- Steel door **obj103** (room 96 N→101) is unlocked by the **BROOCH (obj104)**, not
  the golden key: `LOCKKEY obj=103 keyobj=104 [brooch]`. The brooch is **inside coffin
  #7 (obj120) in room 99** (the crypt we already visit).
- The golden key (obj130, from `kiss lady` T305 +10, room 102) opens the **iron chest
  (obj129, room 102)** instead — which holds the **ballista bolt (obj131)**.

### Sceptre needs NO upstream change
The sceptre (obj112) is placed by the Madam-Evenov card choice:
`one`(T259)→**coffin #2 (obj111, room 99)**, `three`(T306)→iron chest room 102,
`four`(T289)→Loralious container 21, none→Morac room 109 (T339). **We bank `one`, so
the sceptre is already in coffin #2 in the crypt** — grab it there. (Swapping
one→three is unnecessary and desyncs the downstream chase.)

### Morac fight (two-stage), +75 total
1. Reach room 108 (96→101→104→105→108; 104→105 gated by `carom` T319, 105→108 by
   gate25 obj-state). **`look away` (T342) survives Morac's gaze**, then
   **`attack morac with sceptre` (T344, +10)** → moves Morac to room 0.
2. Reach Stonehenge via the crypt portal → **`kill morac` (T350, room 0, +50)**.
   Both need the sceptre held. En route: `kiss lady` (T305 +10, room 102),
   `carom` (T320 +5, room 104; password = anagram of "Morac").

### Staff + Hell tail
- Staff (obj1) appears on the chapel altar (room 75) after **T188 `pipeonaltarswitch`
  (+10)** — requires DROPPING sword/shield/breastplate first (carrying any = death).
- `say borantha` (T360 +20) readies it; `snap staff` (T358 +50) → Hell room 166.
- Hell 166-173: Charon (room 170: `whistle` T501, `give gold coin to charon` T503),
  **Cerberus dead (T490 +20, 167→168)**, `touch picture` (T515 room 169 golden door),
  Asmodeus (NPC39, room 173). **WIN = `blow horn` (T417): horn obj147 held + sceptre
  held + same room as Asmodeus.**

### ★ The access blocker (why nothing banked)
The village+castle (rooms 36-109) is an island reachable **ONLY by the fly** (T44
`say fly`, from chasm room 4) — and **fly CONSUMES the copper ring** (`ACT type=0
v1=17 v3=0` hides obj25; verified: end-state inventory has no ring). Room 50 (Druids
grove, `30 n→50`) is a dead-end; the gypsy grove (54-56) reaches the village but not
back to the forest. **So the Morac fight cannot be appended after catch-the-wind (we
end at room 16 in the forest, ring gone). It must be woven into the FIRST/only castle
visit (after the pentagram at room 77), adding ~40 turns upstream → desyncs the
RNG-tuned Edna fight + Damastus chase → a full downstream re-tune is required.**

### Plan for session 18
Restructure the castle section (insert after pentagram, before the crypt-portal step):
crypt grabs **coffin #2 (sceptre) + coffin #7 (brooch) + coffin #11 (portal)** → upper
castle (`unlock steel door with brooch`→101, `kiss lady` 102 +10 + golden-key→iron
chest, `carom` 104 +5, `look away`+`attack morac with sceptre` 108 +10) → back through
crypt → `enter portal` → Stonehenge → `kill morac` +50 → resume sphinx/avalanche/
dungeon/maze. **= +75 (→580).** Then RE-TUNE the Edna fight + Damastus chase. Confirm
the Morac timer (EVENT 92, fires 40-50t after entering room 77) fits the longer tour.
LATER: staff via chapel T188 → say borantha +20 → snap staff +50 → Hell finale → win.

---

## SESSION 18 (2026-06-27): Morac-access thread cracked — 0 banked (505 held)

Another decode session. The session-17 plan ("integrate the Morac fight into the
castle visit") is **proven impossible** by the Morac timer — but the real intended
route was found. Canonical `harness/shadowpeak_solution.txt` untouched (505/790).

### The Morac fight is reached via the GENIE, not the castle
`say a` to Olmo the genie (T450, room 135 in the deep dungeon) has
`ACT type=1 v1=0 v2=0 v3=108` — it **teleports the player to room 108 (Morac's
room)**, bypassing the upper-castle gauntlet AND the Morac timer. (`say c`/T452 →
room 0; `say b`/T451 = the +5 blessing we already bank, needed for Damastus.)
**Caveat:** `say a` also moves the sceptre (`ACT type=0 v1=66 v2=0 v3=0`) —
unverified whether kept/relocated-to-108/lost. **That is the #1 thing to test next
session** (need the sceptre in hand at room 135; the SCARE debugger is read-only so
it can't be injected — must be acquired legitimately).

### Why castle integration is impossible
Morac timer **EVENT 92**: starts on entering room 77 (T192 zandysayshi), fires at
40-50 turns (this seed: exactly 50), `pauseTask=0` (**nothing stops it**), affTask
T327 = a `where=2` castle-group instant death (verified fatal in rooms 100 AND 102).
The pentagram (~36t) + upper-castle attack-morac (~20t) + portal route is ~56-60t,
well over 50. Only Stonehenge (via portal) is safe, and you can't return (fly eats
the ring).

### kill morac mechanics
`kill morac` (T350, room 0, +50) needs Morac **present at room 0**. Morac reaches
room 0 **only** via `attack morac with sceptre` (T344, room 108, +10) — he does not
pursue on his own (NPC26 startRoom=108). After T344, EVENT 103 gives ~8 turns to
reach Stonehenge and kill him (else T352 room-0 death).

### Grazilda guards the coffin-#2 sceptre
With the `one` fortune card, the sceptre is in coffin #2 — guarded by **Grazilda the
Dark Queen** (vampire). Opening #2 wakes her; she follows like the gargoyle and
one-shots you (T262) **unless you `stake grazilda`** (T261), which needs the **wooden
stake (obj105) in room 82 "Servants' quarters"** (71 e→72 e→73 s→81 w→82). Verified:
without the stake she even chased us through the portal to Stonehenge and killed us.
The stake + coffin-#2 sceptre need no room 77, so grab them **before** the lantern
(before the timer starts).

### Intended-path hypothesis (session 19)
1. Castle, before room 77: get the stake (82), then crypt `open coffin #2` /
   `stake grazilda` / `take sceptre`. Also get the **staff** (chapel T188, drop
   sword/shield/breastplate first).
2. Castle, room-77 phase (≤50t): lantern → cellar chisel → painting/cat/scroll →
   pentagram chip (+15) → crypt coffin #11 → portal → Stonehenge.
3. Resume the banked forest/mountain journey; **re-tune** the Edna fight + Damastus
   chase for the added castle turns.
4. Deep-dungeon room 135 genie: keep `say b` (+5); test whether `say a` (or a 2nd
   wish) reaches Morac WITH a usable sceptre → room 108 `look away` + `attack morac
   with sceptre` (+10) → room 0 `kill morac` (+50) → `say borantha` (+20).
5. Finale: `snap staff` (+50) → Hell → Cerberus (+20)/Charon/`touch picture` →
   `blow horn` near Asmodeus = WIN. Potential ~+160.

## SESSION 19 (2026-06-27): 505 → 515 — CHAPEL pipe-on-altar (+10) + the staff banked

**Banked +10 (the chapel switch) and obtained the endgame staff of Borantha. Score
505 → 515, deterministic, 0 deaths. Walkthrough/solution only; no engine change.**

### The session-18 genie hypothesis is DEAD (decoded conclusively)
`say a` to Olmo (T450, room 135) does `ACT type=0 v1=66 v3=0` on the sceptre — `v3=0`
is the **hidden** destination, so it **destroys/hides the sceptre**, then teleports
you to room 108. And `attack morac with sceptre` (T344) has `RESTR type=0 v1=66 v2=1`
= **sceptre must be HELD**. So arriving at 108 via the genie leaves you with no
sceptre and no way to do T344. Worse, the three genie wishes are mutually exclusive
(picking `say b` consumes the lamp), and we need `say b` for the Damastus blessing —
so `say a` was never actually available to us. **Morac can only be fought by reaching
room 108 the hard way, holding the sceptre.**

### Room 108 is reachable ONLY through the pentagram gauntlet (confirmed from EXITs)
The ballista-tower loop (64 sw→66 u→68 s→106 u→109) reaches 108 only via `109 S→108`,
**gated by task 344 itself** (the Morac fight) — circular. The sole real entrance is
`105 N→108` (door-state gate 25), and 105 sits at the top of the upper-castle climb
71 u→95 u→96 n→101 n→104 u→105, every step gated (pentagram T244 / steel-door brooch /
Colos-golem T319 / carom). All of it under the 40-50t Morac timer. So the Morac chain
remains the hard, still-unsolved endgame — deferred again.

### What WAS banked: the chapel (T188 +10) + the staff
This sits **outside** the Morac timer and needed no new prerequisites. Inserted right
after the gargoyle kills, while still at the main hall (room 71), before the
timer-starting lantern run:
```
s                      (71 -> 74 fountain room)
drop sword             (sword/Shield/breastplate in the water or chapel = instant
drop shield             death, T182-T187; we carry no breastplate)
swim                   (74 -> 76 -> auto SwimAlive T180 -> 75 the hidden chapel)
put pipe on altar      (T188 +10: the smoking pipe vanishes, the unbroken staff of
                        Borantha appears on the altar)
take staff
dive                   (75 -> 74)
take sword
take shield
take gold coin         (a gold coin sits in the fountain - Charon's fare for the
                        Hell finale; grabbed now while we are here)
n                      (74 -> 71, resume the lantern run)
```
The **staff of Borantha** is the endgame weapon: `say borantha` (+20) then `snap
staff` (+50, the teleport into the Hell realm) both need it held. We now carry it the
whole way (it survives the portal, the deep dungeon and the maze).

### Downstream re-tune (the cost of +10 castle turns)
The ~10 extra castle turns shifted the seeded RNG of two later set-pieces:
- **Trailing gargoyle hit-RNG:** the parity shift moved the gargoyle's damaging hits
  so one now lands lethally at the **copper button** (room 145). Both routes reach the
  maze at the **same** Stamina 13/50 — it is purely an RNG-phase effect, not a deficit.
  **Fix: eat the venison at the maze entry (room 139) instead of just before Damastus**
  — the +10 Stamina carries us past the copper button and still through the fight.
- **Damastus chase:** re-derived with the greedy BFS chaser (`/tmp/chase.py`: drives
  the seeded binary, reads `SC_TRACE_PLAYER` + `SC_TRACE_JUDY npc=35`, attacks when
  co-located else BFS-steps toward him until "+25"). New chase from the stone button:
  `e e e / attack / e / attack / w / attack / e / attack / e / attack` — Damastus dies
  at **room 162**; then `e`→151 press bronze button (+10 all-buttons), exit
  `w w w s u u`→165 `catch the wind` (+15) → room 16. End Stamina 10/50.

### Resume (session 20)
515/790 banked. The remaining ~275 is the Morac/Hell endgame, still gated behind the
pentagram-gauntlet-under-the-timer problem (above). Open lines to try next:
- Can the upper-castle climb (71→95→96→101→104→105→108) + `attack morac with sceptre`
  actually fit in ≤50t if ALL prep (lantern/cellar/chisel/scroll/brooch/sceptre/staff)
  is done **before** first entering room 77? Count it move-by-move; the Colos-golem
  fight at 104 is the wildcard (RNG + several turns).
- We already hold the **staff** and a **gold coin** — two finale prerequisites banked
  early this session. Still need: sceptre (coffin #2 via the wooden stake + Grazilda),
  brooch (coffin #7, opens the steel door 96→101), and a Colos-golem kill for the carom
  gate. The horn of the angels (obj147) for the win is task-revealed in Hell.

### Session 20 (2026-06-27): 515 → 620 — THE MORAC ENDGAME, SOLVED (+105)

The blocker that stalled sessions 16–19 was a **misdiagnosis of the Morac timer**.
EVENT 92 (starts on first entry to room 77, fires ~49 turns later) runs task 326
"Morac arrives" → 2 turns → task 327 "Morac kills player". Both are **`where=2`
scoped to the room group `{65,69-103}`** (dumped via a new `WHERE_ROOMS=[…]` line in
scdump for SOME_ROOMS tasks). **Rooms 104+ are NOT in that group** — and scevents.c
only runs an event's affected task if `task_can_run_task_directional` passes (which
checks `where`). So **once you climb past room 101 into room 104, the timer can never
kill you.** Reach 104 fast (the brooch is the only hard prereq for the climb), survive
the timer there, then take the rest at leisure in the now-safe upper castle.

The full banked chain (all in `shadowpeak_solution.txt`, deterministic 620, 0 deaths):
1. **Reach room 104** (under the timer): lantern (77→79; also lights the dark crypt) →
   cellar chisel (carry venison as the heal) → crypt **brooch** (coffin #7) → painting/
   cat/scroll → pentagram chip → climb 95→96 (`unlock steel door with brooch`/`open
   steel door`)→101→104.
2. **Colos the golem** (104): `attack golem` ×5 then `say carom` (+5; "carom" = anagram
   of Morac) → opens 104→105.
3. **Descend for the sceptre** (timer now spent, lower castle safe): grab the **wooden
   stake** (room 82), refuel venison at the dining table (take/eat/take = +10 Stamina
   AND keep one), crypt coffin #2 → `stake grazilda` (the vampire Dark Queen, else she
   one-shots you) → `take sceptre`; open coffin #11 (springs the exit portal); silver key.
4. **Re-climb** with a 102 detour: `kiss lady` (+10) → golden key → `unlock iron chest
   with golden key`/`take ballista bolt`; 104→105, `wear shield`, `open wooden door`→108.
5. **Morac fight** (108): shield worn deflects the fireball (+5), `look away` (+5),
   `attack morac with sceptre` (+10) → Morac escapes on a black dragon.
6. **Ballista** (108→109→106→68): `fire` (+20) — the bolt downs the dragon, dropping
   Morac at the stone henge.
7. **Kill Morac**: 68→66(`se` rotated)→64→65→71→97→99→100→`enter portal`→room 0 →
   `kill morac` (+50).

**Stamina** is managed entirely by venison (the gargoyle #1 we keep alive to suppress
the Melvin behead drains ~1/turn; killing it would hide the whole inventory). The eaten
venison reappears on the room-72 table, so every pass refuels.

**Downstream re-tune:** the ~80 extra castle turns shifted the maze RNG. Damastus chase
re-derived (greedy BFS, `/tmp/chase.py`): from the stone button (157) `s s / attack / s
/ attack / s / attack / n / attack / n / attack` → he dies at **room 155**; then
`n n e e e e e e`→151 `press bronze button` (+10), `w w w s u u`→165 `catch the wind`
(+15) → room 16. The deep-dungeon (Edna etc.) survived the shift unchanged.

**New tooling:** scdump.c now prints `WHERE_ROOMS=[…]` for every `where=2` (SOME_ROOMS)
task — the room-group membership — which is what cracked the timer.

### Session 21 — ★ THE GAME IS WON ★ (620 → 710/790, victory)

The Hell finale is solved and the adventure is **completed** ("Well done. You have
completed the adventure Shadowpeak.") — a deterministic, 0-death winning walkthrough.

**Two items had to be collected first** (the earlier sessions never grabbed them):

1. **Horn of the angels (obj147)** — the win item. After the avalanche (`move tree
   trunk`, room 124), the knight **Quentis** rides up (T416, after T410 + endofbattle)
   and drops the horn at your feet: `z`, `take horn`. **Do NOT `follow quentis`** — that
   walks you east into room 125, a stray-arrow instant death.
2. **Arm-and-leg greaves (obj170)** — in **room 163 (Minotaur's lair)**, which the maze
   exit already passes through. Grab and **wear** them: they are required to survive
   Cerberus's tail-sting in Hell (T509/T510 = death without greaves **and** Shield, both
   worn).

Both grabs add turns upstream, so the **Edna fight** (re-tuned to 2 waits) and the
**Damastus chase** (re-derived via greedy BFS, `/tmp/chase.py`) were re-tuned. New chase
from the stone button (157): `e e e e e e s s` then attack/step until Damastus dies at
**room 150**; then `n`→151 `press bronze button` (+10), exit `w w w s` (grab greaves at
163) `u u`→165 `catch the wind` (+15) → room 16.

**The Hell finale** (from maze exit room 16):
- nav `s se e`→Stonehenge (room 0); `say borantha` (**+20**) charges the staff,
  `snap staff` (**+50**) → Morac's black magic warps the jump to **Hell, room 166**.
- **Charon ferry:** `s`→170 (River Styx); `whistle` then `z z z z z` (wait for the
  ferryman to pole over); `give gold coin to charon` → ferried to the Garden of Lost
  Souls (171); `u`→172 `take black key` `d`→171; **`stand on ferry`** → Charon takes you
  back to 170 (T505, triggered by EVENT 143 when on the ferry with the key).
- `n`→166; **`unlock iron gate with black key`** / `open iron gate`; `n`→167.
- **Cerberus** (Stamina 99, room 167): `attack cerberus` ×2 fells him (**+20**, T490).
  Survived only because the greaves + Shield are worn.
- `n`→168 (demon room — the demons would kill an unblessed soul, but `say b` to the
  genie blessed us, T494, so we pass); `n`→169; **`touch picture`** opens the golden door
  (T515) → `n`→**173 (Asmodeus)**.
- **`blow horn`** (horn + sceptre held, in Asmodeus's presence) = **T417 VICTORY** — a
  horde of angels spews from the horn, overwhelms the devils, and an angel returns
  Loralang home to the king. THE END.

**Final score: 710/790, won, 0 deaths.** The remaining ~80 points are scattered
optional/missable extras — three unused fortune cards (+15, mutually exclusive with the
`one` we take), put-fet-on-flowers (+10 but needs killing Fetlar = −10 net loss), and a
handful (Alam Nadeer kill +20, examine-dragon +10, wolfy/rex/Margo/woody +5/+5/+5/+10)
that sit inside the RNG-tuned early/mid route and would risk destabilising the win to
chase. A win at 710 was banked instead.

**Ceiling correction (2026-07-02):** examine-dragon (+10, T387) is obtainable but
**mutually exclusive with `hide from dragon` (+10, T386)** — see the DRAGON hazard
bullet for the decoded mechanics and the verified recipe — so it is NOT additive to
a route that hides. The true score ceiling is therefore **≈755, not 790**:
790 − 15 (fortune-card exclusivity) − 10 (fet-on-flowers needs the −10 Fetlar kill)
− 10 (hide/examine exclusivity). Against the best banked run (killwraith 740) the
genuinely chaseable remainder is the Alam Nadeer kill (+20) and whichever of the
wolfy/rex/Margo/woody cluster that variant misses — all inside the RNG-tuned route.

No engine code changed this session — pure walkthrough extension.
