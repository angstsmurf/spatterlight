# The Town of Azra — Walkthrough

ADRIFT game by Stan Tencza. An explicitly **unfinished** RPG-sandbox ("This
game is still under development… The game is nowhere near completion."). Derived
by playing under SCARE (deterministic seed) and cross-checked against the game's
internal task, exit, and **Battle System** data.

**Result: there is no score and no winning ending** (`score` reports a maximum
of **0/0**). "Completing" the game means working through the six goals the
author lists in the intro. **Three are reachable; three are not** — and the
three that aren't are blocked by the game's own incomplete data, not by SCARE.
See the analysis at the end.

You are **Pindarus**, a man in the Roman-ish town of Azra, starting with **$500**
and the clothes on your back.

The author's intro goals:
1. Kill a bandit, and get his money
2. Kill a deer, take the carcass and sell it to Drako
3. Make a few purchases from Harthro's, Calado's, Golapho's
4. Stay at Gralle's Inn
5. Purchase a house
6. Learn Stealth Tactics

---

## Town map

```
        Northern Forest (9)          [deer]
              |  S
        Bandit Trail (3)             [bandit]
              |  S
        Northern Trail (6)
              |  S
  Golapho(2)-W- Busy Street (1) -E- Harthro's Tavern (0)
              |  S                  [Ormulus, Kiris in the street]
        Poel's Plaza (7) -SE- Gralle's Inn (4) -in- rented room (5)
         /  |  \
       W    E   S
   Outside  Residential(12)   Calado's
   Stores(10)  |  in           Metal Works(8)
      |S       Your House(13)->Bedroom(14)/Dining(15)   [needs $7500]
   Butcher(11)
   [Drako]
```
*(Town Council Hall (16/17) and the Law Building (18, Pahlidro) have no walking
entrance — they are only reachable via an "enter town council hall" task.)*

---

## Full command list (the reachable goals: shopping + the inn)

```
                           (press a key past the 3 intro screens)
e                          (Busy Street -> Harthro's Golden Tavern)
change a dollar into cents (the tavern sells in cents; you start with $0 cents)
buy a coffee               (Goal 3a: $1.50)
take the mug of coffee     (the mug stays on the bar)
gulp the coffee
sip the coffee
buy a steak                (Goal 3a: $8)
take a bite out of the steak
w                          (-> Busy Street)
w                          (-> Golapho's Armor)
buy a bronze helmet        (Goal 3b: $24)
buy rawhide armor          (Goal 3b: $90)
wear the rawhide armor
e                          (-> Busy Street)
s                          (-> Poel's Plaza)
s                          (-> Calado's Metal Works)
buy a hunting sword        (Goal 3c: $80)
wield the hunting sword
n                          (-> Poel's Plaza)
se                         (-> Gralle's Inn)
rent a room                (Goal 4: $50)
in                         (-> your rented room)
lie on the bed
nap on the bed             (you take a nap and "feel a little better")
out                        (-> Gralle's Inn)
nw                         (-> Poel's Plaza)
stats
wealth                     (ends at $246 and 50 cents)
```

This visits and buys from all three shops (**Goal 3**) and rents and sleeps at
Gralle's Inn (**Goal 4**), deterministically, leaving you at **$246.50**.

---

## With SCARE's combat-assist: the combat goals open up

SCARE has an opt-in combat-assist (`SCR_ASSUME_COMBAT=1`) that auto-lands hits in
games whose author left Accuracy/Agility at 0. With it on, Azra's intended
strength-vs-defence combat works: buy the **hunting sword** ($80) from Calado,
`wield the hunting sword`, go to Bandit Trail and `attack bandit` (~10×) — *"You
kill the vile bandit!"* and *"You search the dead body… you now have $…"*. So
**Goal 1 (kill a bandit, get his money)** becomes reachable, as does **Goal 2**
(kill the deer and sell its carcass to Drako). Repeated bandit/deer income can
then fund **Goal 6 (Stealth, $800)** and chip at **Goal 5 (house, $7500)**,
though the house is a long grind. The assist is non-faithful (the real Runner
can't land a hit either); the section below describes what the shipped game does.

## Why Goals 1, 2, 5 and 6 are unreachable (game data, not a SCARE bug)

I dumped the game's Battle System data and economy thresholds directly from the
`.taf`. The four remaining goals form a dead end built into the unfinished data.

### Combat can never deal damage (blocks Goals 1 & 2)

ADRIFT's Battle System resolves a hit with **`accuracy > agility`** (strict),
then **`damage = strength − defence`** — the exact formula in the original
Runner's P-code (`run400.exe`). The shipped character data is:

| Character | Str | **Acc** | Def | **Agi** | Stamina |
|-----------|-----|---------|-----|---------|---------|
| **Player (Pindarus)** | 1 | **0** | 0 | **0** | 100 |
| Bandit | 5 | **0** | 3 | **0** | 30 |
| Ormulus | 3 | **0** | 2 | **0** | 30 |
| Deer | 0 | **0** | 0 | **0** | 20 |
| (all other NPCs) | 0 | **0** | 0 | **0** | 0 |

**Every** character has Accuracy 0 and Agility 0, and **every** weapon in the
game (the hunting sword included) carries an Accuracy bonus of **+0** — they only
add Strength. So the hit test is always `0 > 0`, which is false: **no attack by
anyone can ever land**, which is exactly what you see — both you and the bandit
endlessly "manage to avoid" each other. The bandit never dies, so `search for
money` (Goal 1) has no body to search; the deer never dies, so there is no
carcass to sell to Drako (Goal 2).

Crucially, **no "Change Battle Attribute" action anywhere in the game touches
Accuracy or Agility**, so the player's Accuracy can never be raised above 0 by
any means. This is a
property of the author's incomplete data: load the same `.taf` in the original
ADRIFT Runner and combat stalemates identically. SCARE reproduces it faithfully
(the "doesn't seem to do any damage" / "manages to avoid" branches in
`scbattle.c` match the Runner).

### The money walls (block Goals 5 & 6)

- **Goal 5 — Purchase a house** costs **$7,500** (Garthus's home, on the
  Residential Block). Entering "Your House" (rooms 13–15) is gated behind the
  *buy house* task.
- **Goal 6 — Learn Stealth Tactics** from Ormulus requires **$800** (a variable
  check: `money ≥ 800`).

You start with **$500**, and the *only* ways the game offers to earn more money
are killing bandits (`search for money`) and selling deer carcasses to Drako —
both of which require combat that can never succeed (above). So your wealth is
capped at $500, well short of either threshold. Both goals are therefore
unreachable, again by the shipped data rather than by SCARE.

### 2026-08-02 — re-checked against the V390 sweep and the hidden-weapon-accuracy trap; verdict CONFIRMED

Azra was the fourth of the games once filed together as "unconfigured-combat
casualties". Three of those were rewritten in 2026-08-02's sweep, so this one
was re-audited with the same two checks. Both come back negative — the verdict
above stands unchanged.

- **`The_Town_Of_Azra.taf` is V400** (`c2 cf 93 45 3e 61` at offset 6), so
  `battle_is_legacy_version()` is false and the 4.0 `accuracy > agility` gate
  really does apply. The V390 → `battle_legacy` correction that overturned
  *The Search for Mr Smith* and *Villains & Kings* — where the hit test is
  **skipped entirely** — does not reach this file.
- **All 38 objects dump `acc=0`.** This is the trap that had made the
  *Jason Vs. Salm* analysis wrong: a weapon's Accuracy bonus is added straight
  onto the wielder's roll in `battle_eff_accuracy()`, but `status` never shows
  it. `SCR_DUMP_OBJLOC=1` now prints `acc=` beside `hit=`, and every armed
  object here is +0. The eight objects with any battle value are gold armor
  (hit 8 / prot 8), heavy sword 8, Azranian Soldier's Sword 8, broad sword 6,
  hunting sword 5, Calado Special Edition Knife 5, hunting dagger 4, steel
  switchblade 3 — all `acc=0`. The original claim that no weapon carries an
  Accuracy bonus was correct; it is now positively verified rather than assumed.
- **The game does contain 22 type-7 "Change Battle Attribute" actions** (an
  earlier draft of this file said there were none — that was wrong), but their
  `Var1` values are only **0 (Attitude, set)**, **1 (Stamina, delta)** and
  **2 (MaxStamina, delta)**. The range indices that matter — 3/5/7/9 for
  Str/Acc/Def/Agi, and even the display-only caps 4/6/8/0xA — never appear.
  Nothing in the file can move Accuracy off 0. The conclusion is unchanged; only
  its stated evidence needed correcting.
- Player, dumped live: **Stamina 100, Hit strength 1-1, Accuracy 0-0,
  Defense 0-0, Agility 0-0**, wielding nothing. Every range is degenerate
  (`Lo == Hi`) across all 12 NPCs too — the "upgraded-3.9 fingerprint" — but
  here the 4.0 signature means the engine still enforces the hit test.
- Only three NPCs are configured at all: **bandit** (npc 2, Stamina 30, Str 5,
  Def 3, `KilledTask = 19`), **Ormulus** (npc 6, Stamina 30, Str 3, Def 2,
  speed 3, no KilledTask) and the **deer** (npc 5, Stamina 20, everything else
  0, `KilledTask = 37`). The other nine are all-zero.
- **There is no `ACT type=6` anywhere in the file** — no win, no lose, no death
  ending exists to reach. That is the structural reason `score` reports 0/0, and
  it is independent of the combat stalemate: even with the assist, "finishing"
  Azra means ticking off the author's prose goals, not reaching an ending.

Two incidental findings from the dump, both harmless:

- **The bandit and the deer were meant to respawn.** Task 19 `#banditkristdies`
  moves in the *dead body of the bandit*, teleports the NPC away, and then hands
  it `type=7 v1=1 v2=4 v3=30` — a full +30 stamina restore. Task 37 `#deerdies`
  does the same with +20. So the author's income design was a renewable
  bandit/deer farm, which is why $7,500 for the house looked achievable to them.
- Damage arithmetic, for the assisted run: bare-handed the player deals
  `1 − 3 = 0` against the bandit, so the hunting sword is not optional even with
  the assist — `1 + 5 − 3 = 3` per blow against 30 stamina, i.e. the ~10 swings
  the section above reports.

### Summary

| Goal | Status | Why |
|------|--------|-----|
| 3. Shop at Harthro's / Calado's / Golapho's | ✅ reachable | done above |
| 4. Stay at Gralle's Inn | ✅ reachable | done above ($50) |
| 1. Kill a bandit & take his money | ❌ unreachable | acc 0 vs agi 0 ⇒ no hit ever lands |
| 2. Kill a deer & sell the carcass | ❌ unreachable | same combat stalemate |
| 6. Learn Stealth Tactics | ❌ unreachable | needs $800; no income without combat |
| 5. Purchase a house | ❌ unreachable | needs $7,500; no income without combat |

Half the game's stated goals are simply not completable in the released data —
consistent with the author's own warning that "the game is nowhere near
completion." None of this is a SCARE divergence from the original Runner.
