# Welcome to Wonderland — walkthrough (**WIN** 215, needs `SCR_ASSUME_COMBAT=1` + `SCR_ASSUME_CAPACITY=1`)

- **Engine:** ADRIFT 4.00, `wonderland.taf` (7 KB, 27 rooms, 12 tasks, 4 NPCs).
  Alice falls back into a nightmare Wonderland: a card-guarded gnome mine,
  a vault, a giant drill that breaks the gates of the Red Queen's castle, and
  the Red Queen herself. The win text sets up a sequel that was never made
  (*"Before you stands not only the next part of your adventure... but the
  rest of Wonderland."*).
- **Result:** WIN, 215 points, 78 commands. The win text comes from TASK 11
  `push button` (EndGame, +100) in the Nightmare Gate (room 26).
- **Row:** `wonderland_solution.txt|wonderland.taf|The Tempest has put you someplace different|SCR_ASSUME_COMBAT=1 SCR_ASSUME_CAPACITY=1 SCR_SKIP_WAITKEY=1`
- **Assists:** both of them, and both are defaulted on for this game in the
  app by `GSC_GAME_ASSIST_TABLE` (os_glk.cpp, 2026-09-26), so the GUI plays
  the same route without typing anything. `SCR_ASSUME_CAPACITY` is the harness
  hook added on 2026-09-26 for the capacity switch (`glk capacity on`): unlike
  the other four it is per-game state, so the env var sets the default that
  each newly created game picks up (`gs_capacity_assist`, scgamest.cpp).
  Without it the route stalls at command 1, `get knife` — see below.

## Why the combat assist is needed (same case as athylon)

The Battle System is on, and every character has Accuracy 0–0 and Agility
0–0 (`SCR_DUMP_TASKS=1 SCR_DUMP_BATTLE=1`: player St 100 Str 20 Def 10; Card
Guards St 65 Str 15 Def 10; Red Pawn St 20; Red Queen St 150 Str 25 Def 10).
Under the 4.00 `accuracy > agility` test (0 > 0) every blow misses both ways.
I measured this without the assist: 30 × `attack card guard with knife` gives
30 × "manages to avoid your attack", and the guard never lands a hit either.
The small key (door +5), the combination parchment, and above all the Red
Queen's **iron key** are only held by NPCs you have to kill. TASK 10 (unlock
the Throne Room door, +25) requires the iron key, and the exit E to room 26
is gated on it. So the game can't be won in faithful play. With the assist the
fights play out on strength against defence, and the author clearly intended
that: the notes teach `attack (target) with (weapon)` and `status`.

## Why the capacity assist is needed

Command 1 of the route is `get knife`, and in faithful play it never succeeds:
"The ethereal knife is too heavy for you to carry at the moment." The player's
MaxWt is 90 and the knife weighs 4, but run400's running carried-load total
also counts objects held or worn by NPCs whose parent chain passes through the
player-visible accounting (the rod, the letter, the small key and the Red
Queen's Staff of Hearts), which puts the total at 94 before the knife is
picked up. The real Runner does the same, so the game is genuinely unwinnable
as authored — the knife is the only weapon, and every ending needs kills.

`glk capacity on` (harness `SCR_ASSUME_CAPACITY=1`) switches the accounting
back to the legacy recompute-from-held-objects, which weighs only what the
player actually carries, so the knife is takeable and the route runs.

## Scoring (every `ACT type=4`)

| Task | Command | Pts |
|---|---|---|
| 0 | `unlock door with key` (Entrance to mines) | 5 |
| 1 | `unlock door with mine key` (Mine Entrance Hall) | 10 |
| 4 | `enter 25 17 38` (vault) | 15 |
| 6 | `pull lever` (Drill Room East) | 10 |
| 5 | `pull switch` (Drill Room West; needs the lever first) | 10 |
| 7 | `press forward` (Front Gates; needs the lever) | 20 |
| 8 | `push portrait` (Gallery) | 20 |
| 10 | `unlock door with iron key` (Throne Room) | 25 |
| 11 | `push button` (Nightmare Gate, ends the game) | 100 |

Total 215, all of it taken. TASK 0 is repeatable, but a second unlock does
not score again (verified: the score stays at 5). The declared MaxScore is
**10**, an authoring slip, so the end summary reads "You scored 215 out of the
maximum 10! That is 2150% of the game! You finished -205 points short." That
is why the win marker is on the closing text and not on the score.

## Route

1. Rabbit Hole: `get knife`, then read the tutorial note and message.
2. Entrance to mines: kill Card Guard #1 (4 hits). `look` first, then
   `get all` (the dropped rod, letter and key are unseen until you look;
   `get all` straight after the kill says "nothing worth taking"). Then
   `unlock door with key`.
3. Gnome Village → elevator, `press down button`, get the Mine Key in the
   Equipment room, `press up button`, go N and `unlock door with mine key`.
4. Mining Complex: kill Card Guard #2, then `look` and `get all` (the
   parchment reads "Combonatiun: 25, 17, 38"). In the Vault, `enter 25 17 38`
   and `get blade cards`. The Blade Cards are a *worse* weapon: Hit strength
   15 against the knife's 30, so keep attacking with the knife.
5. `e n n pull lever`, then back and round `s s w w n n pull switch`, then
   `n e press forward`. The drill breaks the gates.
6. Count Hall: kill the Red Pawn (2 hits). Gallery: `push portrait`. Treasure
   Room: `get all` (Mushroom and Shield), then `wear shield` (Defence 20, so
   the Queen's staff "doesn't seem to do any damage").
7. Throne Room: `eat mushroom` (Stamina 150/150), then kill the Red Queen
   with 8 knife hits. Then `look`, `get all` (you get the iron key, and the
   Staff doesn't fit: "Your hands are full"), `unlock door with iron key`,
   `e`, `push button`.

## Oddities

- The NPC names are two words ("Card Guard", "Red Pawn", "Red Queen") and
  have no one-word alias, so `attack guard` asks "Who do you want to attack?"
  and `x guard` gives "You see no such thing". Use the full name.
- The Entrance to mines exit E has no gate. The "locked door" is only room
  text (ALT 0), so TASK 0 is points and flavour and does not block the way.
- The letter says "drink that bottle that was dropped", but the game has no
  bottle.
