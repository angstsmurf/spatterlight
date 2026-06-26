# Matt's House — walkthrough (sandbox; max 5/5, no win)

**Game:** `Matt's House.taf` by (juvenile author, ~2000). ADRIFT **3.9** (TAF
header byte8 `0x94` / byte10 `0x37`, native — not a 4.0 conversion). A
"day-in-the-life" sandbox: *"You are playing as Matt Mancini. You can explore
his house and do the things he does in an average day."* Deterministic.

**Result:** **No win, no lose — there is no ending.** Structural dump of all
**105 tasks** shows **zero `ACT type=6` (EndGame) actions**, so no win/lose/death
state can ever be reached. There is exactly **one `ACT type=4` (ChangeScore)
action** in the whole game, on task 11 `eat apple` (+5), so the stored maximum is
**5 and the game is 100%-completable in a single command once you reach the
kitchen.** Solution: `harness/matts_house_solution.txt`.

## Solution (7 commands → 5/5)

```
n
e
e
d
n
open refridgerator
eat apple
```

`n` (stand up, leave Your Room) → upstairs Hallway hub → `e`, `e` east through
the hallway chain to the stairwell hallway → `d` down to the Downstairs Hallway →
`n` into the **Kitchen** → `open refridgerator` → `eat apple` = **+5 (score
5/5, 100%)**. Verified 3× identical.

## What the game actually is

A teenager's first ADRIFT project: a faithful model of his house (≈24 rooms over
two floors — bedrooms, bathrooms, closet, computer room, kitchen, living/dining/
family rooms, laundry, garage, front/back yard) populated with mundane "things he
does in an average day." The 105 tasks are activities, not a puzzle chain:
`take a shower`, `flush toilet`, `put zelda in N64` / `play zelda game`,
`turn computer on` → `get on aol` → `read mail`, `drink root beer`,
`put turkey in oven` / `eat turkey`, `play super smash brothers`,
`get on treadmill`, `drink water` (×9 repeats), `eat soup`, `drink coffee`,
`wash clothes`, `play on swingset`, `call alex`, `watch TV`, `pet dozer` /
`hit dozer` (Dozer is the family dog). Family NPCs (Mom, Dad, Rachel, the dog
Dozer) stand in their rooms.

**The Battle System is enabled but vestigial.** 19 of the actions are `type=7`
(ChangeBattle stat tweaks) hung off the self-care verbs — `drink root beer`,
`eat turkey`, `eat soup`, `drink coffee`, `turn on treadmill`, `wash hands`,
`fart`, and `pet`/`hit dozer` nudge the hidden stamina/strength counters up or
down. There is no enemy to fight, no `KilledTask`, and no EndGame, so the stats
do nothing observable; `hit dozer` is the only attack verb and leads nowhere.

## Why 5/5 is the true maximum

The stored max score is 5 and the game reports `5 out of a maximum of 5 (100%)`
after `eat apple`. The whole-game dump confirms only one ChangeScore action
exists, so there are no other points to find and no hidden ending — the author
wired a single +5 onto eating an apple and never added a win condition. Faithful
to the data and to the ADRIFT 3.9 Runner; no SCARE divergence, no combat-assist
needed (same "score, no win" sandbox class as `lifesimulation` /
`The_Town_Of_Azra`, but with one token scoring task instead of zero).
