# The Search for Mr. Smith — Walkthrough / Analysis

An ADRIFT detective game (30 rooms, 5 NPCs, 23 tasks, Battle System enabled).
Analysed under SCARE (deterministic seed) against the game's task/exit/Battle
data and cross-checked against the original ADRIFT Runner
(`decompiled/Battles.bas`).

**Result: the game is UNWINNABLE in any faithful interpreter; maximum reachable
score 25 / 100.** You play a detective ("gumshoe") sent to find the missing
Mr. Smith. The intended ending is reached by sniping a fuel tank with a rifle —
but the rifle is locked behind a chain of prerequisites that ultimately requires
defeating the mobster **Fernelli** in combat, and the game's combat data makes
*every* fight impossible. This is the author's data, not a SCARE defect; the
original Runner behaves identically. Evidence below.

---

## The reachable path (25 points, deterministic)

```
                       (radio intro plays; you start in The Hall)
n                      (-> The Lobby)
e                      (-> Study)
take poker
w                      (-> Lobby)
w                      (-> Library)
open bookcase with poker   (+5; the poker pries the bookcase open -> secret passage down)
d                      (-> Dungeon)
take torch
u                      (-> Library)
e                      (-> Lobby)
e                      (-> Study)
light torch in fireplace   (+5; the Study fireplace lights the torch)
w                      (-> Lobby)
u                      (-> Upper Lobby)
e                      (-> Bathroom)
open curtain           (+5; the shower curtain hides a tied-up girl)
untie girl             (+5)
w                      (-> Upper Lobby)
d                      (-> Lobby)
w                      (-> Library)
d                      (-> Dungeon)
n                      (-> Courtyard)
drink water            (+5; from the bubbling fountain)
```

That collects all five reachable scoring tasks: open bookcase, light torch, open
curtain, untie girl, drink water = **25 / 100**.

---

## Why the game can't be won (combat is non-functional)

The ending is **task 18, "snipe fuel tank with rifle"** — a scripted action with
a type-6 *win* and +10 points. It needs the **AUG assault rifle**, which sits in
a gun cabinet in the Billiard Room. But the Billiard Room is in the lower half of
the mansion, reachable only by descending from the Bedroom, and that descent is
gated behind a hard prerequisite chain:

```
snipe fuel tank  ->  need rifle  ->  Billiard Room (lower mansion)
   lower mansion  ->  Bedroom "lie on bed" (task 4)
   lie on bed     ->  requires open bookcase + light torch + UNCHAIN BUTLER + UNTIE GIRL
   unchain butler ->  requires HOLDING the gold key
   gold key       ->  held by the mobster Fernelli, who must be defeated in combat
```

And combat can never succeed. I dumped the shipped Battle-System attributes:

- **Player:** Strength 10, **Accuracy 0**, Defense 20, **Agility 0**, Stamina 50.
- **Fernelli:** Strength 15, **Accuracy 0**, Defense 25, **Agility 0**, Stamina 60.
- Every NPC and every weapon: **Accuracy 0 / +0** (the Colt .45 and rifle add
  damage only, no accuracy).

ADRIFT decides a hit with **`accuracy > agility`** *before* applying
`strength − defence` damage (confirmed in the Runner source `Battles.bas`: the
hit/avoid test is a distinct comparison from the damage step). With every
accuracy and agility at 0, the test is `0 > 0` = false, so **no attack ever
lands** — verified directly: 25 shots at Fernelli left him at 60/60 stamina, and
he can't hurt you either. Nothing in the 23 tasks ever raises the player's
accuracy, and **no task gives the player the gold key** — the only task that
moves it (`unchain butler`) *consumes* it. So the gold key can be obtained only
from Fernelli's combat-death drop, which is impossible. The descent — and thus
the rifle, the fuel tank, and the win — is unreachable.

> The game's intro says *"if their attack force is stronger than your defense,
> you will take damage"* — that describes the strength−defence **damage** step,
> which the author evidently mistook for the whole of combat. In ADRIFT the
> accuracy>agility **hit** gate comes first, and with accuracy left at the
> editor's default of 0, it can never pass. The author appears never to have
> tested that combat actually lands a blow; the result is a game that cannot be
> completed in SCARE *or* the original Runner.

---

## Score map (what's reachable vs not)

| Task | Pts | Reachable? | Why not |
|------|----:|:----------:|---------|
| open bookcase with poker | +5 | ✅ | |
| light torch in fireplace | +5 | ✅ | |
| open curtain | +5 | ✅ | |
| untie girl | +5 | ✅ | (needs open curtain) |
| drink water | +5 | ✅ | (Courtyard fountain) |
| unchain butler | +5 | ❌ | needs the gold key (Fernelli, uncombatable) |
| lie on bed | +5 | ❌ | needs butler + girl freed |
| kick cabinet / open cabinet | +5 / +5 | ❌ | Billiard Room, past the bed-descent gate |
| destroy boulder | +5 | ❌ | outdoors, past the front door (past the gate) |
| snipe fuel tank with rifle (WIN) | +10 | ❌ | needs the rifle, past the gate |
| `###bear dead` | +10 | ❌ | melee kill (accuracy 0) |
| `lost game` | −20 | (death) | |

Reachable: 5×5 = **25**. Everything else is gated behind the Fernelli combat
deadlock and is therefore unreachable.

---

## Even SCARE's combat-assist doesn't save it

SCARE has an opt-in combat-assist (`SC_ASSUME_COMBAT=1`) that auto-lands hits in
games with unconfigured accuracy. It rescues some of these games (e.g. Villains
& Kings, The Town of Azra), but **not this one**: with assist on you *hit*
Fernelli, but "it doesn't seem to do any damage" — your best accessible weapon
is the Colt .45 (effective strength 20), and Fernelli's **defence is 25**. The
only weapon strong enough is the AUG rifle, which is locked behind Fernelli.
Meanwhile Fernelli now hits back (strength 40 with his shotgun − your defence 20)
and kills you in a few rounds. So the deadlock is *also* a strength/defence
imbalance, and the game stays at **25/100** even with assist.

## Bottom line

The Search for Mr. Smith is a coherent little mystery, but its author left every
character's Battle-System Accuracy/Agility at 0, so no fight can land a hit — and
because the essential **gold key** is held by a mobster who can only be looted by
killing him, the whole second half of the game (and the only ending) is sealed
off. **Maximum 25 / 100**, no victory, faithful to the original Runner. This is
the same unconfigured-combat failure seen in Azra, Villains & Kings and To Hell &
Beyond — here fatal to completion, as in To Hell & Beyond.
