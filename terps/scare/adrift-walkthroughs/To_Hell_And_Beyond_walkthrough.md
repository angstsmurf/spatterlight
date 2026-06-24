# To Hell & Beyond — Walkthrough / Analysis

A large ADRIFT adventure (190 rooms, 41 NPCs, 92 tasks, a conversation `greet`
system, and the Battle System enabled). Analysed under SCARE (deterministic
seed) against the game's internal task, event, exit and Battle-System data, and
cross-checked against the original ADRIFT Runner (`decompiled/Battles.bas`).

**Result: the game is UNWINNABLE in any faithful interpreter, and the maximum
reachable score is ~68 / 373.** Both of the game's endings are gated behind
killing the final boss, and the shipped combat data makes *every* fight
unwinnable. This is the author's data, not a SCARE defect — the original Runner
behaves identically. Full evidence below.

You play an adventurer who falls into "beyond" (the town of Oran) and is meant
to defeat the tyrant **Xozim** and either **go home** or **claim the throne**.

---

## Why the game cannot be won (combat is non-functional)

ADRIFT resolves a combat hit with **`accuracy > agility`** (strict; confirmed in
the Runner source `decompiled/Battles.bas`, the `>` test at the attack
routines), then deals `strength − defence` damage. I dumped the shipped battle
attributes for the player and all 41 NPCs and every weapon:

- **Player:** Strength 5, **Accuracy 0**, Defense 3, **Agility 0**, Stamina 20.
- **Every enemy** (Evil frog, Big rat, Theeve, Bandit, Thief, Big scorpion,
  Soldiers, **Xozim** = the boss, Stamina 120, etc.): **Accuracy 0, Agility 0**.
- **Every one of the 10 weapons** (incl. the strongest, Hit +50): **Accuracy
  bonus +0** — they add only damage, never accuracy.

So in *every* fight the hit test is `0 > 0` = false: **no blow, by anyone, can
ever land** (you and the enemies endlessly "manage to avoid" each other). The
enemies can't hurt you either, so you never die — you just can't win a fight.

Crucially, **nothing in the game ever raises the player's accuracy**:
- I checked all 92 tasks: every "Change Battle Attribute" (type-7) action raises
  only **Max Stamina / Strength / Defense** (the RPG "kills make you tougher"
  rewards on `^^scorpiondead^^`, `^^banditdead^^`, `^^thiefdead^^`) — **never
  Accuracy or Agility**.
- No weapon, event, or variable touches accuracy/agility.

And there is **no scripted (non-combat) way to register a kill**: the
`^^scorpiondead^^`/`^^banditdead^^`/`^^thiefdead^^`/`^^xozimisdead^^` tasks are
the NPCs' Battle-System *KilledTasks* (they fire only when an NPC's stamina hits
0 in combat). There are **zero "execute task" (type-5) actions in the entire
game**, and no event triggers them, so they can be reached *only* through a
combat kill — which is impossible.

Finally, **both endings require the boss dead:**
- `go home` (**+80**, ends game) and `claim the throne` (**+150**, ends game)
  each have the single restriction "task `^^xozimisdead^^` is complete".

Since Xozim (120 stamina, Accuracy/Agility 0) can never be killed,
`^^xozimisdead^^` never fires, and **neither ending is reachable**. The game
cannot be completed. (SCARE reads non-zero combat stats correctly where they
exist — e.g. Sun Empire's configured player stats — so this is genuinely To Hell
& Beyond's unconfigured data, faithful to the Runner.)

---

## Maximum reachable score: ~68 / 373

With combat dead, only the **non-combat** scoring tasks are obtainable. The full
list of point sources (dumped from the task data) is:

| Task | Points | Reachable? | Note |
|------|-------:|:----------:|------|
| `Use rope with cliff` | +3 | ✅ | rope + cliff |
| `give meat package to nimf` | +10 | ✅ | NPC Nimf |
| `give meat to dog` | +5 | ✅ | NPC Dog |
| `Use crowbar with manhole` | +5 | ✅ | crowbar + manhole |
| `^^aquired armor^^` | +20 | ✅ | buy/take armor (Mika armor shop, room 121) |
| `greet Trace` | +25 | ✅ | NPC Trace |
| `^^Days^^` | **−3 / day** | (penalty) | a timed event docks 3 points per day — be quick |
| `^^scorpiondead^^` | +5 | ❌ | kill Big scorpion (combat impossible) |
| `^^banditdead^^` | +10 | ❌ | kill Bandit |
| `^^thiefdead^^` | +10 | ❌ | kill Thief |
| `^^xozimisdead^^` | +50 | ❌ | kill Xozim (boss) |
| `go home` | +80 | ❌ | requires `^^xozimisdead^^` |
| `claim the throne` | +150 | ❌ | requires `^^xozimisdead^^` |

Reachable: 3 + 10 + 5 + 5 + 20 + 25 = **68** (before the per-day penalty).
Unreachable (combat chain + both endings): 5 + 10 + 10 + 50 + 80 + 150 = **305**.
(68 + 305 = 373, matching the declared maximum to within the usual few-point
author rounding.)

The map is fully traversable — **no exit is gated on any task or kill** (the
hostile NPCs can't block you because they can't damage you), so all six
reachable points can be collected by exploration. The opening is verified:

```
                       (start: Entrance to beyond - a cave, Zifan at the door)
greet zifan            (the game's conversation system: greet <character>)
open door
s                      (-> Oran entrance, the town; NPC Nekar here)
```

From Oran the town opens up (east and south). The reachable objectives are
scattered across the 190-room map: the **armor shop** is room 121 ("Mika armor
shop"); **Trace**, **Nimf**, the **Dog**, the **rope**/**cliff**, and the
**crowbar**/**manhole** are spread through the town and surrounding areas. Each
is collected with the literal command shown in the table (e.g. `greet Trace`,
`give meat to dog`, `use crowbar with manhole`, `use rope with cliff`).

> A full room-by-room route to bank all 68 reachable points is not given here:
> with no winning state to verify against (and a deterministic but unwinnable
> ending), the route can't be validated the way the other walkthroughs are. The
> point sources, their commands, and the hard 68-point cap are the meaningful,
> verifiable result.

---

## Assisted route map (reverse-engineered)

With `SC_ASSUME_COMBAT=1` the game is winnable, but it is a full-length
adventure — **190 rooms, 41 NPCs, ~14 task-gated doors, a `greet`/`ask`
conversation system, and plot teleports between towns that are not on the exit
map**. Rather than a turn-by-turn list (which I have only verified through the
opening — see the caveat at the end), here is the complete route skeleton I
reverse-engineered from the game data: every area, its scoring tasks, the gate
that opens it, and where the items/NPCs are. The win itself is short once you
reach the end; most of the game is the journey.

**Win condition.** Both endings need `^^xozimisdead^^` (kill Xozim). Xozim is in
room 189 "Large cave", which you don't walk to — a `^^movetolargecave^^` task
teleports you there at the climax. Kill Xozim with the **Megasword** (Hit +50;
in the Sulfan Weapons shop, room 177) — three blows beat his Defense 8 / 120
stamina — then `claim the throne` (**+150**, ends the game) or `go home`
(**+80**). Max with assist ≈ **293 / 373** (the throne ending; the two endings
are mutually exclusive, and `^^Days^^` docks −3/day so don't dawdle).

| Area (rooms) | Scoring there | Gate out / key step | Items & NPCs |
|---|---|---|---|
| **Start cave** (0) | — | `open door` (task6) opens S→Oran | Zifan, big door |
| **Oran** (1–13) | `Use rope with cliff` **+3** (r10) | climb the cliff after the rope | take the **hook** (Ilsar's house r7), find the **rope**, combine → "rope with hook"; Inn (r3) |
| **Tinev** (14–41,153) | `give meat to dog` **+5** (Dog r17); `give meat package to nimf` **+10** (Nimf, tavern r20); `Use crowbar with manhole` **+5** (r23); `greet Trace` **+25** (r23) | get **captured** → "tie you up… old mansion" (task29) → escape via **use lever** (task33) + **take key** (task32) | butcher **Kiren** r18 (`ask Kiren about delivery`/`buy meat`→meat); **crowbar**; **Trace**; sewers r29–30 (crowbar/manhole) |
| **Ship** (42–61) | — | `kick door` (task45) → `take sword` (task46); `take key` to deck (task48) → stairs up to Deck (r54) | **Koir** r47 (sword r21); **Trace** r53 (sword r22); key to deck |
| **Shore / Grassland / Forest** (62–110) | combat: `^^scorpiondead^^` **+5**, `^^banditdead^^` **+10**, `^^thiefdead^^` **+10** (spawned encounters — each also boosts your Strength/Defense/Max-stamina) | traverse toward Mika | Evil frog r72, Forest spirit r90, **Big rat** r94 |
| **Mika** (111–135,154–155) | `^^aquired armor^^` **+20** (buy armor) | `greet mayor` (task78) opens the way on | **Barez** armor shop r121 (**studded leather**); **Gareen** potion shop r120 (health potions); Theeve's house r129 |
| **Castle / Sulfan** (136–177) | — | `sleep` (task77) in a bed advances the plot | **Warth** (castle entrance r136); mayor **Enevez** r152; **Narjaje** Weapons shop r177 → **the Megasword** |
| **Final `<B>` area** (156–160,178–189) | `^^xozimisdead^^` **+50**; `claim the throne` **+150** / `go home` **+80** | `^^movetolargecave^^` teleports you to the Large cave | **Xozim** r189; throne / mayor's bedroom |

The plot beats (the Tinev capture, the ship voyage, the moves between Oran →
Tinev → ship → mainland → the final `<B>` city) are driven by conversations and
events, which is why the towns look disconnected on the raw exit map.

> **Verification caveat.** I verified the opening under assist — `greet zifan` →
> `open door` → south into Oran, and the **hook** in Ilsar's house — and the
> combat mechanism itself (the assist makes hits land; confirmed on the sister
> games' enemies, which die and fire their KilledTasks). The full turn-by-turn
> command list across all 190 rooms is **not** banked here: it needs extensive
> play-discovery of the conversation triggers and plot teleports, a multi-session
> effort. The table above is an accurate structural roadmap derived from the
> game's task/exit/character data, not a step-verified solution.

## Bottom line

To Hell & Beyond is a substantial, fully-explorable world, but its author left
every character's Battle-System Accuracy and Agility at 0 (the ADRIFT editor's
default) and gave no weapon an accuracy bonus, so **no fight can ever be won** —
and because both endings are locked behind killing the boss, **the game has no
reachable victory** and tops out around **68 / 373**. This is faithful to the
original ADRIFT Runner, which uses the identical `accuracy > agility` hit test
on the identical data; it is not a SCARE limitation.
