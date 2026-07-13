# Alexis: Dalskee — walkthrough (WON, 55/65; win verified deterministic)

> **Two routes are documented here.** The **main route** below *carries* the
> magic cube (HitValue 50, fast clean kills) on **Easy** → **55/65**. An
> **alternative route** at the very bottom *wears* the cube (ProtectionValue 50,
> total immunity) on **Hard** → **58/65**. See
> *"Alternative route — WEAR the cube (58/65, Hard)"*. Solutions:
> `harness/alexis_solution.txt` (carry/Easy) and
> `harness/alexis_worn_cube_solution.txt` (wear/Hard).
>
> **Both are BANKED (2026-07-13):** each is a row in
> `harness/run_v4_walkthroughs.sh` with a committed golden
> (`*_solution.expected.txt`) and the win marker `you have beaten Urgorn`. The v4
> suite is 22/22 PASS. Re-verify with `sh harness/run_v4_walkthroughs.sh alexis`;
> re-bless with `--bless` after any intentional engine change.

**Game:** `ALEXIS.TAF` — *"Alexis: Dalskee"* by Kingsbury. **Native ADRIFT 3.9**
(TAF sig byte8=`0x94`/byte10=`0x37`), a fantasy quest with the ADRIFT Battle
System. You are Alexis; the diary sets the goal: *journey to the castle of Uron
and destroy the Elves' enemy, **Urgorn***. Your companion **Serond** (a friendly
wizard NPC) wanders the land and fights at your side.

**Result:** **WIN** ("Congratulations, you have beaten Urgorn and saved the
Country Dalskee…"), deterministic, banked at **55/65** (85%). This is the
**magic-cube route**: it picks up the cube early, which trivialises every fight
and makes the run *robust* (the old 26/65 route died on a single extra turn —
combat was on a knife's edge). Solution: `harness/alexis_solution.txt`.

## What changed vs the old 23–26/65 route

The earlier write-up banked 23–26 and noted that *"every change re-tunes the RNG
stream and must be re-validated; … a heavy weapon is the next lever."* That lever
turned out to be **the magic cube** (HitValue 50). Once you wield it, combat
stops being fragile, so the score-by-combat tasks (four monster kills) and the
on-path puzzle points all become safe to collect. The route now banks **55**:
the entire scored set **except the four hardest** points (difficulty bonus +
two unreachable-without-cost extras — see "Why not 60–65" below).

## The magic cube — the enabler

The cube is **real authored content** (tasks 46/47 in the `.taf`). Type the
in-game spell words once, anywhere, at the very start:

```
nnamen tutem selronden flar darg
```

→ *"A magic cube appears in your hands."* The cube has **HitValue 50** and
**ProtectionValue 50**. Because `battle_best_weapon` auto-wields the highest-hit
**carried** object, just *holding* the cube makes every one of Alexis's blows
land for ~50 — so:

- **Urgorn** (Stamina 200, Defence 5) dies in **4 hits, taking zero damage** —
  he is dead before he gets a meaningful swing in. (The old route needed armour
  *and* lucky targeting flips and still ended at ~23 stamina.)
- Every optional kill below dies in 1–3 swings, so the *exposure* of detouring
  to collect them is tiny.

> Wielded vs worn: the cube is also wearable (`wear cube` → Defence +50,
> near-immunity), **but worn objects are not used as weapons** (offense drops to
> the small sword, which can't out-damage Urgorn's Defence on medium/hard). So
> we **carry** it, not wear it. (The companion spell `dard dard larna dard`
> grants +1000 Accuracy — useless here, since 3.9 legacy combat has no accuracy
> roll. Not used.)

## Two gotchas that still apply

1. **Light the lantern AT HOME.** Task 1 (`light *lantern *`, +5) is scoped to
   the cottage; fuel it (`put oil in lantern`, +3) and `light lantern` *before*
   you leave, and carry the lit lantern into the Caves of Eternal Night (and the
   dark Marshy cave).
2. **The compass labels in the structural dump are scrambled** (ADRIFT's
   internal slot order ≠ the dump's N/E/S/W). **Navigate by the typed directions
   in the solution**, which were all verified by play (room *connectivity* in
   the dump is correct; only the direction labels are unreliable).

## Score map (every scored task, from `scdump` action type 4)

`scdump.c` now prints each task's `score=` and the `ACT type=4 v1=N` score
deltas. The full scored set:

| task | +pts | banked? | where / how |
|------|------|---------|-------------|
| put oil in lantern | 3 | ✅ | cottage |
| light lantern | 5 | ✅ | cottage (scoped!) |
| push tree | 2 | ✅ | Dusteron river bank |
| **turn ring** | **3** | ✅ | Large cave (r21) — same room as the Glaven stone |
| **Narfild dies** | **5** | ✅ | Main Cave (r18) |
| **bridgekeeper dies** | **5** | ✅ | Bridge of Tonerith (r10) — short detour |
| wolf dies | 2 | ✅ | Tonerith Pass (r3) |
| water in pan | 1 | ✅ | South hut (r38) |
| fill pot with water | 1 | ✅ | Elven water point (r34) |
| unlock chest (r41) | 1 | ✅ | Marshy cave, ornate key |
| **Swamp/Longmore king dies** | **2** | ✅ | Longmore marshes (r40) — verb is `attack monster` |
| **forester goblin dies** | **2** | ✅ | Kedarn forest clearing (r27) |
| **eagle dies** | **3** | ✅ | Somlost Ravine (r44) — detour from Longmore jn |
| **Larnt dies** | **5** | ✅ | Uron Main Hall (r65) — he flies there after the bridge |
| open castle door | 3 | ✅ | castle entrance, after `say the password` |
| say the password | 2 | ✅ | castle entrance |
| open bedroom chest | 1 | ✅ | bedroom, small key |
| unlock stone door | 2 | ✅ | Long room, large key |
| enter the Dungeon (north) | 3 | ✅ | Torture room → Dungeon (the last scored move) |
| give food to Tarin | 2 | ✅ | Long Glaven path (r32) |
| **dig** | **2** | ✅ | Glaven junction (r24) — needs the **metal spade** (r20) |
| Narn dies | 1 | ❌ | repeatable; adding it re-shuffled the stream and broke the win |
| touch ball | 1 | ❌ | **teleports you out to r10** — would force a full re-traverse |
| **difficulty** | medium +5 / hard +10 | ❌ | see below |

Banked total = **55**. The win task (`urgorn dies186457`) is itself unscored.

## Why not 60–65 (medium/hard + the last extras)

- **Difficulty (+5 medium / +10 hard).** `easy` gives **+20 Stamina** (max 120);
  medium is neutral (100), hard is −20 (80). The cube is *offense*, not defence
  (held items don't add Defence). The early enemies **wield weapons**, so their
  *effective* Strength is high — Narfild hits for **33**, the bridgekeeper/king
  for ~23 — and those fights happen **before** the elven-village armour. A
  stamina trace (`SC_TRACE_STAM`, temporary) shows the pre-armour stretch costs
  ~107 stamina (8+23+33+23+5+15): survivable on Easy's 120, fatal on medium's
  100. Worse, each medium-enabling sacrifice (skip a kill to save the hit)
  *cancels its own +5* on this carry-the-cube route. **The way to actually bank
  Hard's +10 is to *wear* the cube instead of carrying it — that gives total
  immunity, so the stamina penalty stops mattering.** That is the alternative
  route documented at the bottom of this file (**58/65**); the trade-off is that
  a worn cube is no longer your weapon, so the weak small sword can only finish
  *slump-death* enemies, not *flee* enemies — see there.
- **touch ball (+1):** task 31 has a `MovePlayer → room 10` action — it is a
  teleport trap that flings you out of the castle to the Bridge of Tonerith. Not
  worth re-walking the entire Uron approach for one point.
- **Narn (+1):** killable on the Nelone/Uron path, but inserting the two attack
  turns reshuffled the RNG enough to lose the otherwise-robust Urgorn fight.

## Route outline (see `harness/alexis_solution.txt` for the exact 172 lines)

1. **Cottage (r0):** `nnamen tutem selronden flar darg` (cube), loot, fuel +
   light the lantern (+3, +5).
2. **West loop:** Tonerith Pass — `attack wolf` (+2); detour `e e` to the Bridge
   of Tonerith — `attack bridgekeeper` (+5) — `w w` back; grab the Tonerith
   stone, the small sword (Hills of Forecarn), the Dusteron stone; `push tree`
   (+2) to open the way north into the caves.
3. **Caves:** Main Cave — `attack narfild` (+5); Large cave (r21) — take the
   Glaven stone and `turn ring` (+3); detour `w w` to the Square cave for the
   **metal spade**, then back out.
4. **Hub:** `give food to tarin` (+2) opens Glaven junction (r24) — `dig` (+2)
   with the spade.
5. **Kedarn / elven village (re-ordered so armour comes *before* the clearing):**
   water point — take pot + **ornate key**, `fill pot with water` (+1); village
   huts — jacket, helmet, `put water in pan` (+1), elven armour (Defence 2→10);
   *then* the forest clearing — `attack goblin` (+2), take the Kedarn stone.
6. **Longmore chest detour:** Longmore marshes — `attack monster` (the king, +2);
   Marshy cave (dark — the lit lantern carries you) — `unlock chest` (+1) with
   the ornate key, take + wear the **longmore chest plate** (Defence →15).
7. **Eagle detour:** from Longmore junction `e e` to Somlost Ravine — `attack
   eagle` (+3) — `w w` back.
8. **To Uron:** climb to Nelone, take the Nelone stone (all 7), `give stones to
   larnt` (he turns traitor and flies to the castle, opening the bridge); cross
   the Uron dark path to the castle.
9. **Castle:** `say the password` (+2), `open door` (+3); **Larnt is now in the
   Main Hall** — `attack larnt` (+5); fetch the small key (Long room) and the
   large key (bedroom chest, +1); `unlock door` (+2) to the Mirror room.
10. **Mirror room → Urgorn:** go **north** twice (Mirror→Torture→**Dungeon**,
    +3; east/west in the Mirror room are death traps) — `attack urgorn` until he
    falls (4 cube hits, no damage taken). **WIN.**

## Reproduce

```
sh harness/play.sh "<path>/ALEXIS.TAF" harness/alexis_solution.txt
```

Native 3.9, deterministic; no SCARE engine change needed. `scdump.c` gained a
`score=` field on the `TASK` dump line (action-type-4 deltas are the real score
source in this game). Verified: `Congratulations, you have beaten Urgorn…`,
final score **55/65**.

---

# Alternative route — WEAR the cube (WON, 58/65, Hard; deterministic)

Solution: `harness/alexis_worn_cube_solution.txt`. **WIN verified, zero deaths,
final score 58/65** — three points above the carry-the-cube route, and it does
it on **Hard**.

## The idea

`wear cube` instead of carrying it. The cube's **ProtectionValue 50** is added to
Defence only while **worn**, giving effective Defence ≥ 50 → **every enemy in the
game does 0 damage** ("…but it doesn't seem to do any damage"). With total
immunity, the only thing the difficulty setting changes — **Stamina** (Easy +20,
Hard −20; it does *not* touch Strength) — becomes irrelevant, so you can take
**Hard for a free +10** and never come close to dying. That +10 is the whole
reason this route beats the 55 carry route.

## The catch — worn ≠ wielded

`battle_best_weapon` only wields a **carried** weapon, so once the cube is worn
your weapon drops to the **small sword (HitValue 15)**. That matters because
ADRIFT enemies split into two kinds at low stamina:

- **slump-death** enemies just *die* when dropped below ~10% (their death task
  fires, you score): **Narfild +5, Forester Goblin +2, Larnt +5**. The weak
  small sword reaches that threshold fine (it just takes more swings), so these
  **all score while you stand there immune**.
- **flee** enemies *run away* at low stamina before reaching 0 ("the wolf limps
  away…"), and the only way to kill them is a single overshoot blow from above
  10% straight to ≤0 — which needs the HitValue-50 cube *in hand*. So the small
  sword **cannot** kill **wolf (+2), bridgekeeper (+5), Longmore king (+2),
  eagle (+3)** — those 12 points are forfeit on this route. (Swapping the cube
  out mid-fight to land an overshoot doesn't work reliably: `remove cube` costs a
  combat turn that lets Serond chip the target into the flee zone first, and the
  re-wield is finicky. The large hammer would solve it but you can't afford it.)

## Navigation difference (important)

Killing/slumping **Narfild reshapes the cave**: the south-west passage (room 22)
opens, which *re-labels the compass exits of the Main Cave*. So the cave exit is
**different from the carry route** — here it is `nw nw e` (Main Cave → Winding
passage → Small cave entrance → **Glaven junction**), reached with Narfild merely
*slumped*. Tarin (+2) is then a quick `sw` / `give food to tarin` / `ne` detour
from Glaven junction. Everything past the caves is overworld and identical to the
main route. (This cave routing was found with a breadth-first path-finder against
the live game, because the dump's compass labels are scrambled **and** shift with
Narfild's state.)

## Score (58/65)

Hard **+10**, every puzzle point of the main route (**31**: oil 3, light 5, push
tree 2, turn ring 3, fill pot 1, water in pan 1, room-41 chest 1, password 2,
castle door 3, bedroom chest 1, stone door 2, dungeon north 3, Tarin 2, dig 2),
plus the three slump-kills **Narfild 5 + Goblin 2 + Larnt 5 = 12** → **58**. The
four flee-kills (wolf/bridgekeeper/king/eagle = 12) are the gap to the 65 cap.

> Don't trim the solution file: the run is tuned to the deterministic RNG stream,
> so even the *non-scoring* `attack king`/`attack eagle` attempts must stay —
> removing them reshuffles the stream and drops other kills.

## Reproduce

```
sh harness/play.sh "<path>/ALEXIS.TAF" harness/alexis_worn_cube_solution.txt
```

Verified: `Congratulations, you have beaten Urgorn…`, **58/65**, on Hard, taking
zero damage the entire game.
