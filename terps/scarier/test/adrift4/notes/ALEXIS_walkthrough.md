# Alexis: Dalskee — walkthrough (WON, 55/65; win verified deterministic)

> **Two routes are documented here.** The **main route** below *carries* the
> magic cube (HitValue 50, fast clean kills) on **Easy** → **55/65**. An
> **alternative route** at the very bottom *wears* the cube (ProtectionValue 50,
> total immunity) on **Hard** → **58/65**. See
> *"Alternative route — WEAR the cube (58/65, Hard)"*. Solutions:
> `goldens/alexis_solution.txt` (carry/Easy) and
> `goldens/alexis_worn_cube_solution.txt` (wear/Hard).
>
> **Both are BANKED (2026-07-13):** each is a row in
> `harness/run_v4_walkthroughs.sh` with a committed golden
> (`*_solution.expected.txt`) and the win marker `you have beaten Urgorn`.
> Re-verify with `sh harness/run_v4_walkthroughs.sh alexis`;
> re-bless with `--bless` after any intentional engine change.
>
> **Re-derived 2026-09-13 for `SCR_RNG=xoshiro`** (the harness now forces the
> Runner-parity RNG on every row). Both routes were reordered; the carry route is
> now **151 commands** (SCR_SEED=1) and the worn route **193 commands**
> (SCR_SEED=2). Scores are unchanged (55 / 58). See *"The 35-turn lantern"*
> below for why the old orderings can never win again.

## The 35-turn lantern (why the routes were reordered, 2026-09-13)

Event 2 (*"Splash"*) unsets task 1 (`light lantern`) when it fires, and its
start delay is a **load-time draw from the 3.9 codec LCG**, not from the game
RNG -- so it is the same in every replay, for every seed and either RNG: the
lantern goes out **after command 35**, and once out it re-arms and pauses on
the unlit lantern forever (task 1 is scoped to the cottage and the South hut,
so there is no relighting it in the caves). The old routes fought Narfild first
and did the cave puzzle afterwards, past turn 35, in the dark -- which used to
work only because the old RNG happened to land the blows. Under xoshiro it does
not, and the dark cave has two hard rules that no seed can dodge:

- **A 3.9 dark room hides surfaces.** `get all from holes in the wall` takes
  nothing in the dark unless the holes were examined while lit, and objects
  first met in the dark are never stamped *seen*.
- **The lit exit is gated.** Main Cave SE and Winding passage SE both require
  task 1 (lantern lit); Main Cave NW requires task 6 (Narfild dead). So after
  turn 35 the only way out of the caves is **kill Narfild, then `nw nw e`**.

Hence the new ordering: cottage, wolf, stones, `push tree`, and the **whole
cave puzzle (`turn ring`, Square cave spade) finished exactly at command 35**
(`get all from holes in the wall` is the last lit turn); then Narfild is
fought in the dark and the caves are left by `nw nw e` to Glaven junction.
Every examine-before-take (`x old oak table`, `x shelves`, `x large stone
table`, `x holes in the wall`) is load-bearing -- dropping one to save a turn
loses the object. The wolf walks into Tonerith Pass on turn 12, so the two
wolf blows must sit at commands 13-14. Consequences of exiting NW:

- The bridgekeeper (+5) is no longer on the way west; the carry route collects
  him from the Kedarn side via the map loop Kedarn entrance -> `sw sw nw sw`
  -> Bridge of Tonerith, `attack bridgekeeper` x2, `ne se ne ne` back.
- `wear cloak` and the cellar `take coin` were dropped (the helmet costs 7 and
  the East hut coins cover it); the Forecarn sword trip was dropped from both
  routes (the cube is the carry route's weapon; the metal spade, HitValue 15
  like the small sword, is the worn route's).

Seed sensitivity: the carry route wins at SCR_SEED 1, 2, 3, 4 and 6 and dies
at 5, 7, 8 (row uses 1). The worn route is immune, so it wins at every seed
tried (1-6); row keeps 2.

**Game:** `ALEXIS.TAF` — *"Alexis: Dalskee"* by Kingsbury. **Native ADRIFT 3.9**
(TAF sig byte8=`0x94`/byte10=`0x37`), a fantasy quest with the ADRIFT Battle
System. You are Alexis; the diary sets the goal: *journey to the castle of Uron
and destroy the Elves' enemy, **Urgorn***. Your companion **Serond** (a friendly
wizard NPC) wanders the land and fights at your side.

**Result:** **WIN** ("Congratulations, you have beaten Urgorn and saved the
Country Dalskee…"), deterministic, banked at **55/65** (85%). This is the
**magic-cube route**: it picks up the cube early, which trivialises every fight
and makes the run *robust* (the old 26/65 route died on a single extra turn —
combat was on a knife's edge). Solution: `goldens/alexis_solution.txt`.

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
   you leave, and carry the lit lantern into the Caves of Eternal Night. It
   goes out after command 35 whatever you do (see *"The 35-turn lantern"*), so
   everything that needs light must be done by then.
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

## Route outline (see `goldens/alexis_solution.txt` for the exact 151 lines; order as of 2026-09-13)

1. **Cottage (r0):** `nnamen tutem selronden flar darg` (cube), examine +
   loot the table and the cellar shelves, fuel + light the lantern (+3, +5).
2. **West:** Tonerith Pass — `attack wolf` x2 (+2; arrive on turn 12, when the
   wolf walks in); take the Tonerith stone; back past the cottage, `west north
   north`, take the Dusteron stone; `push tree` (+2) to open the way north
   into the caves. (No bridgekeeper detour and no Forecarn sword any more.)
3. **Caves, lit (through command 35):** `nw nw e` to the Large cave (r21) —
   examine + take from the large stone table (Glaven stone), `turn ring` (+3);
   `w w w` to the Square cave — `x holes in the wall`, `get all from holes in
   the wall` (the **metal spade**; this is command 35, the last lit turn).
4. **Caves, dark:** `e e` back to the Main Cave — `attack narfild` (+5, one
   cube blow); leave by `nw nw e` (Winding passage → Small cave entrance →
   Glaven junction). `sw`, `give food to tarin` (+2), `ne`, `dig` (+2) with
   the spade.
5. **Kedarn / elven village (armour *before* the clearing):**
   water point — take pot + **ornate key**, `fill pot with water` (+1); village
   huts — jacket, helmet (paid with the East hut coins), `put water in pan`
   (+1), elven armour (Defence 2→10); from the Kedarn entrance loop `sw sw nw
   sw` to the Bridge of Tonerith — `attack bridgekeeper` x2 (+5) — `ne se ne
   ne` back; *then* the forest clearing — `attack goblin` (+2), take the
   Kedarn stone.
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
sh harness/play.sh "<path>/ALEXIS.TAF" goldens/alexis_solution.txt
```

Native 3.9, deterministic; no SCARE engine change needed. `scdump.c` gained a
`score=` field on the `TASK` dump line (action-type-4 deltas are the real score
source in this game). Verified: `Congratulations, you have beaten Urgorn…`,
final score **55/65**.

---

# Alternative route — WEAR the cube (WON, 58/65, Hard; deterministic)

Solution: `goldens/alexis_worn_cube_solution.txt`. **WIN verified, zero deaths,
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

## Navigation (as of 2026-09-13, same as the carry route)

Both routes now do the cave puzzle first while lit, fight Narfild in the dark
after command 35, and leave by `nw nw e` (Main Cave → Winding passage → Small
cave entrance → **Glaven junction**); the NW exit is gated on task 6 (Narfild
dies), the SE exits on the lit lantern. Tarin (+2) is a quick `sw` / `give food
to tarin` / `ne` detour from Glaven junction. Here Narfild takes **four spade
blows** (HitValue 15 vs Stamina 80) before slumping; the worn cube makes his
replies harmless. The cloak, the cellar coin, the two pointless wolf blows and
the Forecarn sword trip are gone (193 commands, was 200); Serond stays in the
village because his walk is started by the `easy` task.

## Score (58/65)

Hard **+10**, every puzzle point of the main route (**31**: oil 3, light 5, push
tree 2, turn ring 3, fill pot 1, water in pan 1, room-41 chest 1, password 2,
castle door 3, bedroom chest 1, stone door 2, dungeon north 3, Tarin 2, dig 2),
plus the three slump-kills **Narfild 5 + Goblin 2 + Larnt 5 = 12** → **58**. The
four flee-kills (wolf/bridgekeeper/king/eagle = 12) are the gap to the 65 cap.

> The *non-scoring* `attack king`/`attack eagle` attempts are still in the
> file; they were kept from the old route when it was reordered and the run was
> not re-tuned without them.

## Reproduce

```
sh harness/play.sh "<path>/ALEXIS.TAF" goldens/alexis_worn_cube_solution.txt
```

Verified: `Congratulations, you have beaten Urgorn…`, **58/65**, on Hard, taking
zero damage the entire game.
