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

SCARE has an opt-in combat-assist (`SC_ASSUME_COMBAT=1`) that auto-lands hits in
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

Crucially, **no task in the game contains a "Change Battle Attribute" action**,
so the player's Accuracy can never be raised above 0 by any means. This is a
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
