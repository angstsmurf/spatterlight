# RON: Toxically Earth — walkthrough

- **Author:** Tobias Schmitt (`www.tobi-as.de.vu`) — part of his surreal German
  "RON" series. Broken-English absurdist comedy.
- **Engine:** ADRIFT 4 (Battle System present in the `.taf` but never used for a
  reachable goal).
- **Result:** **WIN reached, deterministic.** This is a **0/0 game — it has no
  score at all** (no `ChangeScore` action exists anywhere in its 136 tasks), so
  there is no "maximum" to chase: the only objective is to reach an ending.
- **Multiple endings by design.** The game has **eight separate win-ending
  rooms**, and the author confirms this in the win text itself:

  > *"This is one end of the game — but you can find any more endings… please
  > play again….. ———— TOBIAS SCHMITT"*

- Solution file: `harness/toxically_earth_solution.txt` (11 commands).

## The verified win (the "spacerabbit" ending)

```
down
down
down
down
call spacerabbit
east
get branch
move branch
ring bell
north
down
```

Output ends with *"RON-TIME ERROR AT PORT 32390329… please type down!"* →
`down` → *"Thanks for playing RON: TOXICALLY EARTH … This is one end of the
game."* Verified identical 3× under the deterministic harness.

### Phase by phase

1. **Intro descent — `down` ×4.** The title screen and Hevlop's
   "the blue planet has bad times now / poisoned gases" preamble are a forced
   chain of `down` presses (rooms #1→#4). The fourth `down` drops you into the
   **Jail** (room 0): *"you can see a key, but you can't reach it. There is a
   keyhole in the cage."*
2. **Leave the jail — `call spacerabbit`.** The Jail has two exits, both
   initially sealed:
   - **North** opens only after `open keyhole with needle` (you start holding
     the needle) — this is the entrance to the **main toxic-earth world** (the
     shops/paradises, and the other seven endings).
   - **East** opens after `call spacerabbit` — a task with **no restrictions**,
     so it always works. This is the fast lane to one ending; we take it.
3. **`east` → "Room of the spacerabbit"** (room 112): *"a very big branch is
   here… and the door is protected by a magic wall."*
4. **`get branch` / `move branch`.** You can't take the branch ("it's too big"),
   but `move branch` is the real trigger: *"I found a hidden bell in the
   bottom…"* (In data terms, `ring bell` carries a task-state restriction
   requiring `move branch` to be done first.)
5. **`ring bell`.** The spacerabbit appears: *"Hey, Knoffel… just follow me! Go
   north!"* — this completes the gate task on the north door.
6. **`north` → "End"** (room 113): *"RON-TIME ERROR AT PORT 32390329… please
   type down!"*
7. **`down`** fires the end-game (a type-6 win action) → the win text. **Done.**

## Structure (why this is "complete" for a no-score game)

Dumping all 136 tasks confirms **zero `ChangeScore` (type-4) actions and eight
`EndGame` (type-6, var1=0 = win) actions.** Each win action lives on a different
task in a different "bottom of a sub-world" room:

| Win task | Trigger command | Ending room |
|---|---|---|
| 20 | `use spell` | 25 "RON-Underworld #3" |
| 50 | `down` | 44 "Beachanim4" |
| 64 | `down` | 61 "Loveanim" |
| 66 | `look hevlop` (gated) | 62 "Big Spider" |
| 87 | `look monster` (gated) | 79 "Monster" |
| 91 | `down` | 93 "End-freedom" |
| 126 | `down` | 111 "Inside US" |
| **132** | **`down`** | **113 "End" ← the route above** |

Seven of these sit at the far end of the large surreal world entered through the
Jail's **north** door (`open keyhole with needle` → north → **Street #1**, a hub
of shops, "Paradise #1–#6", a casino with Reality-Dollars, a chicken with the
cage key, a bomb, a saw-in-a-box, etc.). Each sub-world is a short
fetch-and-unlock puzzle chain ending in its own "you win" room — the player is
explicitly invited to replay and find them. Reaching **any one** ends the game;
there is no cumulative state and no score, so the spacerabbit ending above is a
genuine, complete win. (Note also rooms 26 "Death" and 38 "Game Over" are *not*
endings — they trap you until `get life`.)

## Honest notes

- **No score is reachable because none exists** — faithful to the data and to
  the original ADRIFT Runner; not a SCARE limitation.
- The Battle System flags are set in the header but no reachable task uses a
  combat kill, so `SC_ASSUME_COMBAT` is irrelevant here.
- Deterministic seed 1234 harness; re-running the solution file always yields
  the same win text.
