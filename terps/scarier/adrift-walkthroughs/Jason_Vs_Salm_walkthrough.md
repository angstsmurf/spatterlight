# Jason Vs. Salm — walkthrough

- **Engine:** ADRIFT **4.0** (`c2 cf 93 45 3e 61`) with the **Battle System
  enabled** (used by the Salm fight). Menu-driven framing (numeric options), an
  8-room suburban house, and a single wandering enemy who chases the player.
- **Result:** **WIN reached (deterministic), honest maximum 0/1000.** The win
  screen is *"Good job then! Congratulations!"* The four scored victory options
  (0/100/500/1000) are **honour-gated by difficulty**, and the top two are
  arithmetically unreachable while the third is a lottery you will effectively
  never win — see below.
- Solution file: `harness/jason_vs_salm_solution.txt` (no name/gender prompts),
  row env `SCR_SEED=11`.

> **Correction (2026-08-02).** The mechanics section of this file used to say
> Salm's "accuracy 34 never beats your agility 50" (so the player was
> invulnerable), that one bare-handed swing kills him, and that "across 40+
> consecutive `attack salm` on each of the three raised levels, zero hits
> connect". All three are wrong. Battle attributes here are **ranges rolled
> fresh every turn** (`battle_roll`, `lo + Int(rnd*(hi-lo))` — **high bound
> exclusive**), not fixed values; the player is carrying a **long blade**
> (HitValue 10, **Accuracy +5**, Method 4) that `status` does not report; and
> `hard` really does land the occasional hit. The exact numbers are below.
> This game is **V400**, so the V390 `battle_legacy` correction that rewrote
> *Mr. Smith* and *Villains & Kings* does **not** apply here — the 4.0
> `accuracy > agility` gate is the right model, and the WIN / 0-of-1000 verdict
> is unchanged.

## The win (seed-dependent, banked at `SCR_SEED=11`)

```
1               <- title menu: [1] Start game
1               <- "Who do you wish to be?" → [1] Jason
n               <- Front Door → Den; Salm is here and opens fire
attack salm     <- keep attacking until he dies; his death warps you to the
                   Victory Room
0               <- Victory Room asks "What difficulty did you set?"; you played
                   Normal, so the honest answer is [0] Normal → the win
```

On the banked seed one exchange is enough. It is **not** guaranteed: across
seeds 1–60 on Normal the outcome is **42 wins / 4 player deaths / 14 neither**
(Salm wanders off north and `attack salm` stops parsing). Hence the row pins
`SCR_SEED=11`.

## Combat data

Everything is a `[lo,hi)` range re-rolled each turn.

| | Stamina | Strength | Accuracy | Defence | Agility | Speed |
|---|---:|---:|---:|---:|---:|---|
| **Player** (as Jason) | 25 (max 50) | 0–50 | 0–50 | 0–50 | 0–50 | — |
| **Salm** (NPC 1) | 50 | 30 | 0–34 | 16 | **32** | 1 |
| Jason Evans (NPC 0) | 0–28 | 0–53 | 0–40 | 0–57 | 0–39 | 1 |

Objects: **long blade** HitValue 10, **Accuracy +5**, Method 4 (`stab`) —
carried by the player from the start (choosing "[1] Jason" hands you NPC 0's
kit, though `status` still says "wielding nothing" and shows a `(0)` weapon
column); **revolver** HitValue 25, Accuracy +1, Method 3 (`shoot`) — Salm's;
**hockey mask**, no battle value.

So on Normal the player hits when `roll(0..49) + 5 > 32`, i.e. roll ≥ 28 =
**44 %** (measured 41 % over 267 swings), and a landed blow does
`roll(0..49) + 10 − 16` ⇒ about three damaging hits to drain Salm's 50.
Salm hits when `roll(0..33) + 1 > roll(0..49)`, which is perfectly possible —
**the player is not invulnerable**, and 4 of those 60 seeds end in
"I'm afraid you are dead!". His revolver is Method 3, so under 4.0 rules it
*replaces* his strength: damage is `25 − roll(0..49)`, usually nothing but up
to 25 against a player with 25 stamina and no healing anywhere in the game.

## Why 100 / 500 / 1000 are out of reach

The Victory Room offers four endings, all `var1=0` wins, differing only in
score: `[0] Normal` +0, `[1] Hard` +100, `[2] Extra Hard` +500,
`[3] Fucking Crazy` +1000. **Each is restriction-gated on the difficulty you
actually set** — answer a harder level than you played and the game replies
*"Liar!"* and scores **0**. So the points are only legitimately available if
you *win the fight on that difficulty.*

Difficulty is set by typing `hard` / `extra hard` / `fucking crazy` **at the
character-select screen** (tasks 10/11/12, `where=ONE_ROOM room=1`). Typed
anywhere else — including one prompt later, at the Front Door — you just get
"I don't understand what you mean!" and stay on Normal.

Each of those tasks is a pile of type-7 *Change battle attribute* actions, and
here is the authoring bug: they target **NPC 0 (Jason Evans)** and **NPC 1
(Salm)**, and **never the player** (`Var2 = 0`). The author evidently thought
NPC 0 *was* the player character, so on a Jason playthrough half the buffs go
to a character who is not in the fight, while all of Salm's land. The player
stays at 0–50 / 25 stamina at every difficulty (verified with `status` before
and after typing `hard`).

Salm's agility is the wall. It is a degenerate range (32–32), so it does not
roll, and the deltas are +20 / +40 / +60:

| Difficulty | Salm agility | Player max effective accuracy | Hit possible? |
|---|---:|---:|---|
| Normal | 32 | 49 + 5 = 54 | yes, ~44 % |
| Hard | **52** | 54 | only on rolls 48–49 = **4 %** |
| Extra Hard | **72** | 54 | **never** |
| Fucking Crazy | **92** | 54 | **never** |

Measured: 60 seeds × 60 swings at `hard` gave **10 hits in 298 resolved
attacks (3.4 %)** — matching the predicted 4 % — and **0 hits in the same
sweep at extra hard and fucking crazy**, exactly as the arithmetic demands.

`hard` is therefore *possible* but not winnable in practice. Salm's defence
also rises to 36, so only ~46 % of those 4 % hits do any damage (average ~11),
i.e. roughly **275 attack turns per kill** — while his own accuracy rises to
20–54 against your unchanged 0–49 agility and his strength to 50 against your
0–49 defence, so essentially the first or second blow he lands kills your 25
stamina. Across 30 seeds of 60 swings each at `hard`: **0 wins**, 10 player
deaths, and Salm wandering off in the rest. Extra Hard and Fucking Crazy are
not a lottery at all — no roll exists that can connect.

Net: the game is **completable to a genuine victory**, but its scored payouts
sit behind a combat-balance bug, so the honest, faithful maximum is **0/1000
with the win**. (Lying to claim 1000 just yields "Liar!" and 0 — the engine
enforces the honour check.)

## Notes

- **Salm chases you.** He spawns in the Den and follows the player room to room
  ("Salm enters from the south/east"), so you don't need to corner him — just
  attack on the turn he's present (he's present the moment you step into the
  Den). He can also wander *away* faster than you can follow, which is what
  ends the "neither" seeds above.
- The Battle verbs are the generic `attack` / `kill` / `fight`; the narration
  says "with long blade" because `battle_best_weapon` picks up the carried
  blade even though the `status` screen reports no wielded weapon.
- Dumping weapon Accuracy bonuses: `SCR_DUMP_OBJLOC=1` now prints `acc=` beside
  `hit=`. It was the missing +5 here that made the old "no hit can ever land"
  reading look right.
