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
| **Shadowpeak**      | 710/790 | **DEAD** | Morac NPC walks into room 101 (The tower) before death-event fires; with old events-first order the event fired before Morac moved so player survived; re-derivation needed |

### Shadowpeak regression detail

Shadowpeak's death trigger is an event that checks `player_room == Morac_room`.
Old binary (events first): event fires at start of turn → Morac hasn't walked
yet → no kill.  New binary (NPCs first): Morac walks first → is in room 101
(The tower) when the event checks → player dies.  All three Shadowpeak solution
files (849/895/737 lines) hit this issue.  To fix: re-derive the Shadowpeak
route after the tick-order change (either avoid room 101 while Morac is on the
loose, or kill Morac earlier).

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
