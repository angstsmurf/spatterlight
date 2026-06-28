# The Spirit's Flight — walkthrough / analysis

**Game:** *The Spirit's Flight* (ADRIFT 4 `.taf`, Battle System enabled)
**Result:** **UNWINNABLE** — maximum *reachable* score **50 / 95**, verified
deterministic. The game cannot be completed because of an author data bug (the
water elemental is an orphaned object), which also seals off roughly half the
map and the remaining 45 points.
**Solution file:** `harness/spirits_flight_solution.txt` (the full 50-point
route, replayable under the seeded harness).

---

## Premise

You are a spirit turned back into a living soul to recover the four elementals —
**Amber of Flames** (fire), **Ice Totem** (water), **Orb of Storms** (air) and
**Grass Amulet** (earth) — and return them to the rock tablet at **The Stone
Circle**, reciting an incantation to win. A companion fighter, **Lamanluie**,
helps in combat. The world is the central **Glade of Spirits** hub with four
elemental realms branching off, each guarded by a hostile elemental, plus three
inner "Spirit" caverns.

## The blocking bug (why it can't be won)

The four elemental tokens are dynamic objects 0–3 (Amber, Ice Totem, Orb, Grass).
A full structural dump of the game (all 30 tasks' actions, all 6 events, and
every NPC's Battle-System *KilledTask*) shows how three of them are obtained:

| Elemental | Source | Mechanism |
|---|---|---|
| Orb of Storms (air) | **Fergo** | his *KilledTask* (task 1) drops it on death |
| Amber of Flames (fire) | **Kelorano** | topic command `kelorano t1` (task 18) |
| Grass Amulet (earth) | **Acuru** | his *KilledTask* (task 23) |
| **Ice Totem (water)** | **— nothing —** | **never revealed by any task, KilledTask, or event** |

The water-realm guardian **Crynasalda**'s *KilledTask* (task 12) awards the
**Sea Serpent's Scales** (a piece of armour) instead of the Ice Totem. The Ice
Totem (global object 1) starts *Hidden* and is referenced by exactly two tasks —
the win incantation and `invoke elementals` — but **no action anywhere moves it
out of hiding.** It is an orphaned object: unobtainable.

Two consequences follow, and they cascade:

1. **`invoke elementals`** (task 21, +5) requires Amber **+ Ice Totem + Orb**
   held simultaneously. Without the Ice Totem it can never pass — try it and a
   voice booms *"INVOKING THE ELEMENTALS IN SEEMINGLY USELESS WAYS IS…"*.
2. The exit **Rocky Path → Descent of Earth is gated on `invoke elementals`
   being done.** Since invoke is impossible, that exit is sealed forever — and
   it is the **only** way into the entire deep-earth cluster (Descent, the bat /
   black caves, the Gravel Temple, and the Caverns of Balance/Chaos/Order). So
   **Acuru** (+10) and the three inner-cavern bosses **Griffon / Carnifern /
   Spirit Paladin** (+10 each) become unreachable.
3. The **winning incantation** (task 29) needs all four elementals on the tablet
   and so can likewise never fire.

**Unreachable: 45 points** — invoke (5) + Acuru (10) + Griffon (10) + Carnifern
(10) + Spirit Paladin (10). **Maximum reachable: 50 / 95.** This is faithful to
the original ADRIFT Runner — the KilledTask assignments and the exit gate live
in the `.taf`; SCARE reads them correctly (the other three realms work fully).

### Is it a 3.9→4.0 conversion break? No — it's a plain authoring omission

The file is **natively ADRIFT 3.90**, not a 4.0 conversion. Its 14-byte TAF
signature is `3c 42 3f c9 6a 87 c2 cf 94 45 37 61 39 fa`, which is the exact
**V3.90** signature (the V4.00 signature differs at byte 8: `93` vs `94`, and
byte 10: `3e` vs `37`). SCARE parses 3.9 correctly: the EVENT record layout is
*identical* in 3.9 and 4.0 (so events 0–4 genuinely are empty), and the NPC
battle block is read with the **simpler 3.9 schema** — `Attitude, Stamina,
Strength, Defense, Speed, KilledTask` (4.0 adds AccuracyLo/Hi, AgilityLo/Hi,
Recovery, StaminaTask). That simpler block is exactly why combat here is
strength-vs-defence with no dodge, and the fact that the KilledTasks produce the
right in-game results (Fergo→Orb, Crynasalda→Scales, Acuru→Grass) proves the 3.9
battle parse is byte-aligned, not garbled.

The real cause is visible in the data: the three *working* guardians each carry,
in their kill task, a "move the elemental into the player's room" action —
Fergo task1 A5 (Orb), Kelorano task18 A1 (Amber), Acuru task23 A1 (Grass). The
water guardian **Crynasalda's kill task (task 12) has the +10 score and even an
item drop (the Sea Serpent's Scales) — but is simply MISSING the matching
"reveal Ice Totem" action.** The author wired the kill task but pointed its drop
at armour instead of the elemental and never added the totem move. Because the
other three reveals all use plain 3.9-native mechanisms (KilledTask / topic
task), it isn't a lost 4.0-only feature (e.g. a StaminaTask) — it's an ordinary
authoring oversight that would leave the game unwinnable in the genuine ADRIFT
3.9 Runner too.

## Score map (the 12 scoring tasks)

| Pts | Task | Reachable? |
|----:|------|:--:|
| 5 | examine shelf (High Wizard's Tower) | ✅ |
| 5 | show dagger to Lamanluie | ✅ |
| 10 | Fergo (air guardian) | ✅ |
| 5 | break castle (the Beach) | ✅ |
| 5 | break mirror (Cavern of Mirages) | ✅ |
| 10 | Crynasalda (water guardian) | ✅ |
| 10 | Kelorano (fire guardian) `kelorano t1` | ✅ |
| 5 | invoke elementals | ❌ no Ice Totem |
| 10 | Acuru (earth guardian) | ❌ sealed behind invoke |
| 10 | Griffon | ❌ sealed |
| 10 | Carnifern | ❌ sealed |
| 10 | Spirit Paladin | ❌ sealed |
| **50** | **reachable total** | |

## Key mechanics

- **Hub gates.** The Glade's four forest exits are each locked until a specific
  realm task is done, forming a fixed chain. The chain is *bootstrapped* at the
  High Wizard's Tower: **`wake kilfuno`** relocates the **Spirit Dagger** to the
  Stone Circle; showing it to Lamanluie (task 0) recruits her and opens the
  first (air) forest. The intended order is **air → water → fire → earth**.
- **Guardians = topic commands.** Each elemental NPC is triggered by typing the
  ADRIFT conversation topic literally, e.g. `kelorano t1` — *not* by "ask … about
  …". For NPCs that also have a *KilledTask* (Fergo, Crynasalda, Acuru) simply
  killing them in combat fires the same task. Kelorano has **no** KilledTask, so
  he can only be done with `kelorano t1` (and he stays hostile — grab the Amber
  and flee, or he kills you/Lamanluie).
- **Stat food.** Volmor's Cake (Tower storeroom), cheese (Cliff Top), etc. raise
  Stamina/Strength/Defence; the Healing Potion restores stamina. Eating the cake
  early makes the early fights one-hit affairs.
- **Combat is faithful** (non-zero stats; no combat-assist needed). Determinism
  comes from the seeded harness.

## The 50-point route (annotated)

Opening — bootstrap at the Tower:
```
south, south, southeast          → High Wizard's Tower
wake kilfuno                      dagger → Stone Circle, potion → Tower
get potion
examine shelf (+5)                → sneaks you into the storeroom
get cake, eat cake               stat boost
out, northwest, get dagger        Stone Circle: pick up the Spirit Dagger
north, show dagger to lamanluie (+5)   recruits Lamanluie; opens the air forest
```

Air realm (west):
```
north, west, southwest, south     → Breezy Path
attack moyru                      evil blocker; opens the path south
south, get cheese, eat cheese     → Cliff Top
down, up                          → Ridge of Storms
attack fergo (+10)                drops Orb of Storms + Fletched Spear
get orb
out, north                        → Breezy Path
attack morana, attack morana      second evil blocker; opens north
north, northeast, east, east      back through Hamlet/Chirpy to the Glade, then east
```

Water realm (east):
```
break castle (+5)                 the Beach (sandcastle = map of the Merfolk's Domain)
give spear to clam                feed the clam door any spare object; opens south
south, south                      → Water Tunnel
attack serpanern, attack serpanern  tunnel guardian; opens up
up, east                          → Cavern of Mirages
break mirror (+5)                 (the *Bent* mirror, not the Ice Mirror)
down                              → Sea Serpent's Cave
attack crynasalda ×3 (+10)        water guardian — drops the Sea Serpent's Scales
up, west, down, north, north, west, west   back to the Glade
```

Fire realm (north-west of the Glade):
```
northwest, north, west, north, northeast, east   → Core of Volcano (passing Slikerma, Afernoga)
kelorano t1 (+10)                 reveals the Amber of Flames (topic command!)
get amber, west                   grab it and flee — Kelorano stays hostile
southwest, south, east            back to the Desert Path
attack slikerma, attack slikerma  needed to re-open the Desert→Dry exit
south, southeast                  back to the Glade  →  score 50/95
```

At this point every still-locked door (the earth forest is open, but **Rocky
Path → Descent** is not) leads only to content gated behind the impossible
`invoke elementals`. 50/95 is the ceiling.

## How this was determined

Built the deterministic headless SCARE (`harness/build.sh`); dumped structure
with the debugger (39 rooms, 38 objects, 18 NPCs, 30 tasks, 6 events) and with a
temporary `SC_DUMP_TASKS` block in `sctasks.c` (since reverted — tree left
clean) that printed every task's restrictions/actions, every room's exit gates,
all events, and every NPC's *KilledTask*. That dump is what proved the Ice Totem
has no reveal path and that Rocky Path's exit is gated on `invoke elementals`.
Each realm was then played to confirm the route and the 50-point ceiling, and
the final route verified identical (50/95) across three runs.
