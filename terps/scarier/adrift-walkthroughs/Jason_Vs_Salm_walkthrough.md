# Jason Vs. Salm — walkthrough

- **Engine:** ADRIFT 4 with the **Battle System enabled** (used by the Salm
  fight). Menu-driven framing (numeric options), an 8-room suburban house, and a
  single wandering enemy who chases the player.
- **Result:** **WIN reached (deterministic), honest maximum 0/1000.** The win
  screen is *"Good job then! Congratulations!"* The four scored victory options
  (0/100/500/1000) are **honour-gated by difficulty and the higher three are
  unwinnable** — see below.
- Solution file: `harness/jason_vs_salm_solution.txt` (no name/gender prompts).

## The win (deterministic)

```
1               <- title menu: [1] Start game
1               <- "Who do you wish to be?" → [1] Jason
n               <- Front Door → Den; Salm is here and opens fire (he can't hurt
                   you — his accuracy 34 never beats your agility 50)
attack salm     <- one bare-handed hit kills him (your acc 50 > his agi 32, and
                   str 50 − def 16 ≈ 34 dmg vs his 50 stamina; on the deterministic
                   seed the first swing lands). His death warps you to the
                   Victory Room.
0               <- Victory Room asks "What difficulty did you set?"; you played
                   Normal, so the honest answer is [0] Normal → the win
```

## Scope (why 0 is the honest maximum, and 100/500/1000 are unreachable)

The Victory Room offers four endings, all `var1=0` wins, differing only in score:
`[0] Normal` +0, `[1] Hard` +100, `[2] Extra Hard` +500, `[3] Fucking Crazy`
+1000. **Each is restriction-gated on the difficulty you actually set** at the
start menu — answer a harder level than you played and the game replies
*"Liar!"* and scores **0**. So the points are only legitimately available if you
*win the fight on that difficulty.*

But the difficulty levels (`hard` / `extra hard` / `fucking crazy`, typed at the
character-select screen) **over-buff Salm so that the player can never land a
hit**: across 40+ consecutive `attack salm` on each of the three raised levels,
**zero hits connect** (deterministic seed 1234), so Salm never dies, the Victory
Room is never reached, and the points can't be earned. Only on **Normal** can
Salm be killed (one hit) — and Normal's honest answer is `[0]`, worth 0.

Net: the game is **completable to a genuine victory**, but its scored payouts sit
behind an unwinnable difficulty wall (a combat-balance bug), so the honest,
faithful maximum is **0/1000 with the win**. (Lying to claim 1000 just yields
"Liar!" and 0 — the engine enforces the honour check.)

## Notes

- **Salm chases you.** He spawns in the Den and follows the player room to room
  ("Salm enters from the south/east"), so you don't need to corner him — just
  attack on the turn he's present (he's present the moment you step into the
  Den).
- **You are effectively invulnerable** at all difficulties (Salm's accuracy 34
  can't exceed your agility 50). The difficulty only changes *your* ability to
  hit *him*.
- The Battle verbs are the generic `attack` / `kill` / `fight` (bare-handed —
  there is no usable weapon object).
