# DONE: Restriction type 5 and turn-loop tick order

Both bugs confirmed and fixed (uncommitted working-tree changes to
`screstrs.cpp` and `scrunner.cpp`).

---

## Bug 1 — Missing restriction type 5 (screstrs.cpp) — FIXED

Added `case 5:` before `default:` in `restr_pass_task_restriction()`.
No TAF in the corpus emits type 5 (the format string in `sctafpar.cpp` only
defines types 0–4), but returning `scr_fatal` was wrong.  The case now logs a
trace message and returns `FALSE` so the restriction silently fails.

Les Feux de l'enfer uses type 5 for tasks 113–120 (crystal-key-on-surface
check), but those tasks are unreachable orphans — the FALSE return is correct.

---

## Bug 2 — NPC-walk/event-tick order reversed (scrunner.cpp) — FIXED

Swapped both `evt_tick_events / npc_tick_npcs` call pairs to
`npc_tick_npcs / evt_tick_events` (faithful to Runner Sub_20_62).

### Walkthrough impact

The correct NPCs-before-events order changes the timing of several games.
Post-fix corpus status:

| Game | Before fix | After fix | Note |
|---|---|---|---|
| Les Feux de l'enfer | 25/115 | 25/115 | startup re-sequenced (5 wk + `2` cmd) |
| Cybercow (main)     | 6/10   | 6/10   | unchanged |
| Cybercow (WIN)      | win    | win    | unchanged |
| Villains (main)     | 13/37  | 13/37  | unchanged |
| Villains (assisted) | 30/37  | 30/37  | unchanged |
| Space Boy           | win    | win    | unchanged |
| Shadow of the Past  | ok     | ok     | unchanged |
| **Shadowpeak**      | 710/790 | ~~DEAD~~ **710 again (FIXED 2026-07-02)** | was one turn short vs the EVENT-92 death timer (mechanism re-diagnosed below); all three solutions re-derived to exact score parity — see the RESOLVED section |

### Shadowpeak regression detail

Shadowpeak's death trigger is an event that checks `player_room == Morac_room`.
Old binary (events first): event fires at start of turn → Morac hasn't walked
yet → no kill.  New binary (NPCs first): Morac walks first → is in room 101
(The tower) when the event checks → player dies.  All three Shadowpeak solution
files (849/895/737 lines) hit this issue.  To fix: re-derive the Shadowpeak
route after the tick-order change (either avoid room 101 while Morac is on the
loose, or kill Morac earlier).

---

## ✅ 2026-07-02 (later) — Shadowpeak RESOLVED: all three solutions re-derived, full parity

All three solution files now **win with 0 deaths at exact old-order score parity**
(main 710, allgargoyles 715, killwraith 740; score-event multisets identical to the
old-order transcripts). Added to `run_v4_walkthroughs.sh` (3 rows + blessed goldens,
win marker `completed the adventure Shadowpeak`); full v4 regression 20/20 PASS.
Fixed copies + a freshly built `scare` synced to `~/adrift-battle/harness` and
`~/scare/adrift-walkthroughs/harness` (the stale pre-fix binaries there were
refreshed; all 36 other adrift-battle solutions verified byte-outcome-identical
under the new binary, so the swap is safe).

The winning recipe (per solution file, see in-file comments):

1. **Garlic +5 (T156)** — Maretta's walk **charTask** no longer fires on her
   arrival turn under NPCs-first; it fires when **her walk step lands on the
   player**. Insert `z` at the castle entrance (room 63) before `throw bottle`
   (+ a few extra `z` to re-tune the castle RNG stream; main=4, allgargoyles=18,
   killwraith=6 total).
2. **Morac window one turn short** — hoist `take venison` (room 72) to *before*
   the timer starts (the sword has not yet entered room 77, so task 193 can't
   fire), shaving one in-window turn; the climb reaches room 104 exactly on the
   task-327 kill-check turn. (The pre-window wait knob also re-rolls the EVENT-92
   duration draw, 40–50.)
3. **NPC-walk meet/char tasks generally** — same class as (1): they only fire on
   the NPC's own step onto the player now. killwraith's **Fang** (+5) is a
   one-hit kill but only when he "darts into the area" (wait 25 at room 27);
   allgargoyles' **Melvin meet (T295)**, which gates the behead-teleport (T333),
   needs a 17-turn pre-window wait at room 71 until he waddles in.
4. **Edna wait re-tune + Damastus chase re-derive** — mechanical;
   `harness/shadowpeak_chase.py` (greedy BFS over the dumped EXIT graph with
   `SCR_TRACE_PLAYER`/`SCR_TRACE_JUDY npc=35`) re-derives the chase in one run.
   killwraith's wraith (Haraxis) fight needed 3 pre-fight waits to dodge a −29
   mega-hit.

The behead-timing race in the parked plan below was never needed — the venison
hoist + garlic-z knob was sufficient. Diagnosis kept for reference:

## 2026-07-02 — Shadowpeak diagnosis (superseded; kept for the event/task numbers)

**The theory above is WRONG about the mechanism.** Morac (NPC 26) never moves —
`SCR_TRACE_JUDY` shows him fixed in room 108 (his lab) the whole game. The death
is a pure **timer**, not a co-location check:

- **EVENT 92 `[moracyboyarrives]`** starter=3, startTask=193, time1=40 time2=50,
  affTask → **task 326** `#Moracyboyarrives` (the "grotesque form … Morac!"
  message). Starts on the FIRST time task 193 `SeekerhummsZandos` fires =
  first entry to room 77 (pentagram) **holding the sword**. Under this seed the
  drawn duration is **46 turns**.
- **EVENT 93 `[#1morckillsplayer]`** starter=3, startTask → task 326, time1=2,
  affTask → **task 327** `#Morackillsplayer` (ACT type=6 v1=2 = EndGame death).
  So the kill fires **exactly 2 turns after Morac "arrives".**
- **task 327 `where=2`, `WHERE_ROOMS=[65 69-103]`.** The kill only lands if the
  player is in that group. **Rooms 104/105/108 are NOT in it** → climbing to 104
  is the escape (as the walkthrough intended).

**Why it now dies (canonical `shadowpeak_solution.txt`):** the route from the
timer-start (`w→77`, line 457) to the `unlock steel door` is **exactly 46 turns**,
so Morac always "arrives" the instant you unlock (turn 46). Kill fires turn 48.
The climb needs `open steel door`(47) → `n`→101(48, **in kill zone → dead**) →
`n`→104(49). The player is **one turn short**: the old events-first order gave
one extra tick, letting them reach 104 by the check. Confirmed via
`SCR_TRACE_JUDY`+`SCR_TRACE_TASKS`: task 326 at turn 390, task 327 at turn 392,
player in room 101.

**Fix attempts (all on /tmp copies; committed files untouched):**
1. *Shave one in-window turn* → survives the timer but every candidate turn is
   load-bearing:
   - drop `take venison`(469): survives Morac but a **later stamina death**
     (Melvin/gargoyle #1 chip damage — the venison at 528 is the golem-fight heal).
   - drop `destroy painting #6`(497): **breaks navigation** — it opens the 90→91
     "passage behind the painting" to reach the scroll.
   - drop `attack cat`(500): **witch Rucktebar (NPC 20) kills you** in room 93
     ("your mind fries") — the cat kill fells her.
2. *Behead-teleport escape* (user's idea — "kill all gargoyles, retrieve stuff
   after apologizing"). Killing gargoyle #1 (NPC 15) with all other behead
   conditions met (gargoyles #2–4 dead, painting destroyed, Melvin met) fires
   **task 333 `melvinbeheadsplayer`** → teleports player to **room 107 (Max Head
   room, OUTSIDE the kill zone)**, dumps inventory to storage (room 81/82).
   `say sorry` + `say yes` reattaches the head → player back in room 71; then
   `take all` + re-equip. This is a genuine escape from the WHERE_ROOMS check.
   - **Timing is the crux (unsolved):** the behead fires the instant the LAST
     condition is met, same turn. Kill #1 at **turn 47** → teleport to 107 →
     task 327 (turn 48) checks 107 → SAFE. Kill at turn 48 → **loses the race**
     (task 327's event tick resolves before task 333). Kill at turn ≤46 → you
     recover back into the kill zone (71) before turn 48 → dead.
   - **Blocker:** gargoyle #1's death is **RNG per turn** — a single
     `attack gargoyle #1 with sword` one-shots it at the main hall and at turn 48,
     but NOT at turn 47 (just "You hit"). Need to guarantee #1 dies exactly on
     turn 47. Options to try next: (a) pre-weaken #1 with earlier hits so any
     turn-47 hit kills (need NPC 15 stamina vs Seeker HitValue — was about to
     dump these); (b) skip the `unlock steel door` during the race (defer to the
     re-climb) to free turns 46+47 for two hits on #1, but guard against a
     turn-46 kill firing the behead too early.

**Resume plan:** finish approach 2. Get NPC 15 stamina + sword battle props
(`SCR_DUMP_OBJLOC=10`, and the Battle block for NPC 15). Then either pre-weaken
#1 or find a 2-hit window that lands the kill on turn 47 exactly. Apply the same
pattern to all three solution files (`shadowpeak_solution.txt` [710],
`shadowpeak_allgargoyles_solution.txt` [715], `shadowpeak_killwraith_solution.txt`),
re-verify 0-death win + score, then update `Shadowpeak_walkthrough.md` and the
memory note. Tooling: `SCR_DUMP_TASKS` / `SCR_TRACE_TASKS` / `SCR_TRACE_JUDY` /
`SCR_DUMP_OBJLOC` on the harness `scare` binary (built with `-DSCARIER_DUMP_TOOLS`).
Grep the dumps with `grep -a` (non-UTF8 bytes).

### Les Feux startup change

Old binary: `1, passer, 2, s, s, s, ouvre coffre` (2=wk1 dismiss, s×3=wk2-4)
New binary: `1, passer, X, X, 2, X, X, X, s, s, s, ouvre coffre`
  - wk1 = stats display (NPC fires first)
  - wk2 = alchemist welcome
  - `2` = game command: select fiole bleue (teleports to ENTREE DU DONJON)
  - wk3 = alchemist farewell
  - wk4 = road cutscene
  - wk5 = dungeon entrance cutscene
  - `s, s, s` = ENTREE DU DONJON → UN TUNNEL → JONCTION → PIECE CIRCULAIRE (coffre)
  Attack counts also shifted: ogre×4, goule×2, demon×6, voyou×3, voleur×6.
