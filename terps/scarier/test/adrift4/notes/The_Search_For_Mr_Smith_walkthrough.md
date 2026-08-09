# The Search for Mr. Smith — Walkthrough / Analysis

An ADRIFT **3.9** detective game (30 rooms, 5 NPCs, 23 tasks, Battle System
enabled). Analysed under SCARE (deterministic seed) against the game's
task/exit/Battle data.

**Result: the game IS winnable. Best score 90 / 100** — every scoring task
except the bear kill, plus the type-6 win. Recorded as
`goldens/mr_smith_solution.txt` (golden `mr_smith_solution.expected.txt`, win
marker `Congratulations! I hope you liked our game.`).

> **Correction (2026-08-02).** This file previously claimed the game was
> "UNWINNABLE in any faithful interpreter; maximum reachable score 25 / 100",
> on the grounds that every character's Accuracy and Agility are 0 so the
> `accuracy > agility` hit gate can never pass. That analysis applied the
> **4.0** combat rules to a **3.9** file. `The_Search_For_Mr_Smith.taf` carries
> the V390 signature (`… c2 cf 94 45 37 61 …`), and `battle_is_legacy_version()`
> puts it on the pre-4.0 path: `battle_legacy` **skips the accuracy test
> entirely** (scbattle.cpp, the `battle_unconfigured || battle_legacy || …`
> gate), so every attack lands. The old verdict predates the 3.9 battle-legacy
> port. Fernelli dies in 12 pistol shots and the whole second half of the game
> opens up.

---

## Combat data (all shipped values)

| | Stamina | Strength | Defence | Speed | Notes |
|---|---:|---:|---:|---|---|
| **Player** | 50 (max 50) | 10 | 20 | — | Accuracy 0, Agility 0, Recovery 0 |
| Butler (NPC 0) | 10 | 10 | 10 | 1-in-4 | neutral |
| Girl (NPC 1) | 0 | 0 | 0 | — | ally, but stamina 0 ⇒ never fights |
| **Fernelli** (NPC 2) | 60 | 15 | 25 | most turns | enemy; holds the **gold key** and a **Remington 410 shotgun** |
| Dog (NPC 3) | 5 | 5 | 5 | every turn | enemy until you give it the bone |
| **Bear** (NPC 4) | 70 | 60 | 40 | every turn | enemy; `KilledTask` = task 22 (+10) |

Weapons / armour: Colt .45 **hit 20**, Remington 410 shotgun **hit 40**, AUG
rifle **hit 5**, flak jacket **protection 30**. No object has Method 5, so
`throw … at …` is never available (see the throw note at the end).

Under `battle_legacy` there is no hit roll: **damage = strength + weapon
HitValue − target defence**, every turn, on both sides. Recovery is 0 for
everyone, so damage is permanent.

Stamina economy — only three sources, all of them one-way in some sense:

* **Fountain** (Courtyard, room 7) — `drink water`, +50, **repeatable** (+5 the
  first time). This is the only renewable heal in the game.
* **Potatoes** (cardboard box, Attic) — `eat potatoes`, **+10 max stamina**
  (+5). Raises the ceiling to 60 for the rest of the game.
* **Sandwich** (refrigerator, Kitchen) — `eat sandwich`, +20 current (+5).
  One-shot, but portable: it can be eaten anywhere, including mid-fight.

---

## The winning route (90 / 100)

Answer the two setup prompts (name, `male`), press return past the radio
intro, then:

```
n  e                       (-> Lobby -> Study)
take poker
take pistol                (Colt .45)
w  w                       (-> Lobby -> Library)
open bookcase with poker   (+5, opens the passage down)
d                          (-> Dungeon)
take torch
u  e  e                    (-> Library -> Lobby -> Study)
light torch in fireplace   (+5, unlocks the Dungeon's N/S exits)
w  u  w                    (-> Lobby -> Upper Lobby -> Attic)
open wooden box
take flak jacket
wear flak jacket           (defence 20 -> 50; THIS is what makes the game survivable)
open cardboard box
take potatoes
eat potatoes               (+5, max stamina 50 -> 60)
e  e                       (-> Upper Lobby -> Bathroom)
open curtain               (+5)
untie girl                 (+5)
w  d  w  d                 (-> Upper Lobby -> Lobby -> Library -> Dungeon)
n                          (-> Courtyard)
drink water                (+5, heals to full)
n                          (-> Observatory: Fernelli)
shoot fernelli with pistol   x12
```

Twelve shots: 10 + 20 − 25 = **5 damage** each, Fernelli's 60 stamina to 0. He
shoots back for 15 + 40 − 50 = **5** a turn (and only on "most" turns), so the
flak jacket turns a lethal fight into a comfortable one. Killing him drops his
inventory in the room:

```
take gold key
s                          (-> Courtyard)
drink water                (top up; free, repeatable)
d  s                       (-> Dungeon -> Second dungeon)
unchain butler             (+5, consumes the gold key)
n  u  e  u  n              (-> Dungeon -> Library -> Lobby -> Upper Lobby -> Bedroom)
lie on bed                 (+5; needs bookcase + torch + butler + girl.
                            *** ONE-WAY: dumps you in the Sub-basement, and
                            nothing leads back up. Everything above is now
                            unreachable, including the fountain. ***)
n  w  n                    (-> Wine Cellar -> Catacombs -> Catacombs)
take bone
s  e                       (-> Catacombs -> Wine Cellar)
take bottle
break bottle               (+5, frees the front door key)
take front door key
n  e                       (-> Recreation Room -> Kitchen)
open refrigerator
take sandwich
eat sandwich               (+5)
w  w                       (-> Recreation Room -> Billiard Room)
kick cabinet               (+5)
open cabinet               (+5 -- the task fires but the cabinet is still shut)
open cabinet               (*** must be issued TWICE; the first one only scores ***)
take rifle                 (AUG)
e  n                       (-> Recreation Room -> Front Hall)
open door                  (+5, needs the front door key)
n                          (-> Front Yard: the dog)
give bone to dog           (+10, turns the dog from enemy to ally)
n                          (-> Road)
open automobile
take dynamite
open cab
take matches
n  n                       (-> Mountain Path (bear) -> Low Hills)
destroy boulder            (+5; needs dynamite + matches, and teleports you to
                            Higher mountains. *** STARTS A 30-TURN TIMER ***)
n                          (-> Summit)
snipe fuel tank with rifle (+10, WIN)
```

Ends on:

```
After taking careful aim you pull the trigger of the rifle. After almost no
delay, the helicopter explodes and falls from the sky. …
Congratulations! I hope you liked our game. If you didn't get 100%, you
should go back and try for it!
```

### Traps on this route

* **The bed is a one-way door.** `lie on bed` runs a type-1 *move player* to the
  Sub-basement, and rooms 12–29 have no exit back into rooms 0–11. Drink at the
  fountain until full *before* lying down; after that the only heal left in the
  game is the single sandwich.
* **`open cabinet` must be typed twice.** Task 11 scores +5 and prints the "now
  open" text while the object is still in its closed state; the second `open
  cabinet` actually opens it. Without it, `take rifle` answers "Take what?".
* **`take boulder` is a red herring.** Task 17 ("The boulder is too heavy for
  you to carry.") scores nothing and quietly shoves the boulder into the
  *Higher mountains*. Skip it.
* **`destroy boulder` starts the doom timer.** Event 1 (StarterType 3, "after
  task", TaskNum 17 = 1-based ⇒ task index 16 = `destroy boulder`) fires task
  index 21, `lost game`, exactly 30 turns later: type-6 lose and **−20**. The
  route spends two of those turns, so there is plenty of slack — but do not
  wander.

---

## The missing 10 points: the bear cannot be killed

Task 22 (`###bear dead`, +10) has no player-typeable trigger; it is only
reachable as the bear's `KilledTask`. Killing the bear is arithmetically
impossible, and it misses by exactly one turn.

The best possible weapon against it is Fernelli's Remington 410 (loot it with
`take shotgun` after he dies — it is not needed for any of the 90 points):

* our damage: 10 + 40 − 40 = **10** per shot ⇒ **7 shots** to take 70 stamina to 0;
* the bear's damage: 60 − 50 = **10** per turn, Speed 0 = **every turn**
  (`battle_speed_roll`: 0 → 1, "attacks every turn" — matches the Runner's
  Proc_11_13, so there is no RNG to get lucky with; verified identical under
  six different `SCR_SEED` values);
* the bear gets a free hit on the turn you *walk in*, before you can act.

So the encounter costs `1 (entry) + 6 (replies to shots 1–6) = 7` hits = 70
damage, and the seventh shot kills it before it can answer. Our budget is 60
(potatoes-boosted maximum, arriving full) plus the sandwich's +20 — but eating
the sandwich is itself a turn, which buys the bear an eighth hit. 60 + 20 = **80
capacity vs 80 damage**, and `battle_apply_damage` kills at `stamina <= 0`:

```
enter        us 50   bear 70
shoot 1..4   us 10   bear 30
eat sandwich us 20   bear 30      (+20, then the bear's reply)
shoot 5      us 10   bear 20
shoot 6      us  0   bear 10   <- "I'm afraid you are dead!"
```

One hit short, with the bear on 10. There is no second armour piece (the flak
jacket is the only object with a ProtectionValue), no task raises the player's
Strength or Defence, Recovery is 0 so waiting does not heal, and the fountain is
sealed behind the one-way bed descent. Retreating and returning only adds
further entry hits. The bear is an authoring oversight, not an engine
divergence: 70 bear stamina against a 60 + 20 player budget with a mandatory
free hit on entry.

**Maximum score is therefore 90 / 100.** The game's own closing line ("If you
didn't get 100%, you should go back and try for it!") is inviting you to chase
a point total that cannot be reached.

### Score map

| Task | Pts | Got it? |
|------|----:|:-------:|
| open bookcase with poker | +5 | ✅ |
| light torch in fireplace | +5 | ✅ |
| unchain butler | +5 | ✅ |
| untie girl | +5 | ✅ |
| lie on bed | +5 | ✅ |
| open curtain | +5 | ✅ |
| eat potatoes | +5 | ✅ |
| drink water | +5 | ✅ |
| break bottle | +5 | ✅ |
| kick cabinet | +5 | ✅ |
| open cabinet | +5 | ✅ |
| eat sandwich | +5 | ✅ |
| open door | +5 | ✅ |
| destroy boulder | +5 | ✅ |
| give bone to dog | +10 | ✅ |
| snipe fuel tank with rifle (WIN) | +10 | ✅ |
| `###bear dead` | +10 | ❌ unkillable (above) |
| `lost game` | −20 | (the 30-turn helicopter timer) |

14 × 5 + 10 + 10 = **90**.

---

## Footnote: throwing weapons at Fernelli

Irrelevant here, and impossible anyway. The method verb (`chop`/`cut`/`hit`/
`shoot`/`stab`/`throw`) selects only the narration; the hit gate in
`battle_resolve` is identical for all six. And **no object in this game has
Method 5** — the pistol, shotgun and rifle are all Method 3 (`shoot`) — so
`throw pistol at fernelli` answers "You can't throw with Colt .45 pistol!" and
`throw poker at fernelli` answers "The fireplace poker is not a weapon!". A
landed 4.0 throw would in any case be strictly worse, since run400 clears the
wield before the damage roll and the weapon's HitValue never contributes.
