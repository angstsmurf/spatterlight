# Illegal Socks — walkthrough (**UNWINNABLE as authored**)

- **Engine:** ADRIFT 4.0, battle system enabled. A short comic sci-fi/action
  game (illegal socks smuggling is the throwaway premise; the real plot is a
  teleporter-hopping fight with a mad scientist, "Dr. Myanus Hurts"). No
  sexual content, no minors.
- **Result:** **UNWINNABLE**. Both endings — the "evil" branch (TASK 26,
  `Push Green Button` in the first Battle Dome) and the true win (TASK 31,
  `Push Green Button` in the second Battle Dome, gating the game's own
  `WINTEXT`) — require first defeating the boss NPC "Dr. Myanus Hurts"
  (NPC index 6 and its duplicate index 13) in battle. That NPC can never be
  targeted by any typed attack command, for a genuine original-authoring
  reason, confirmed with `SCR_DEBUGGER_ENABLED=1`/`SCR_DUMP_TASKS=1` and
  direct `fprintf` instrumentation of `lib_battle_npc_is_target`
  (`sclibrar.cpp`):

  - The NPC's `Name` field is the literal string `"Dr. Myanus Hurts"` — it
    contains a `". "` (period-space).
  - ADRIFT 4.0's own top-level input splitter, `run_find_split_400`
    (`scrunner.cpp`), runs unconditionally before any command dispatch and
    cuts a typed line at `", "`, `". "`, `" and "`, `" then "`. A cut is
    suppressed only if the tail's first word names a known **object**
    (`run_split_word_names_object`) — there is no equivalent suppression for
    NPC names. So `attack dr. myanus hurts` is split into two separate
    lines (`attack dr` / `myanus hurts`) before any attack handler ever sees
    the full name. Confirmed empirically: instrumenting
    `lib_battle_npc_is_target` showed `input=[attack dr]` then
    `input=[attack myanus hurts]` for that one typed line.
  - Typing the name with no space after the period
    (`attack dr.myanus hurts`) survives the splitter (no `". "` substring),
    but then fails the engine's literal whole-`Name` substring test
    (`lib_input_contains_word(input, name)`) inside
    `lib_battle_npc_is_target`, since the real `Name` field *does* have a
    space between "Dr." and "Myanus" that the input now lacks. Confirmed
    empirically: `named_by=[(null)]` even though the NPC is present
    (`in_room=1`).
  - Every alias/prefix phrasing that *does* parse at the earlier
    `%character%` grammar stage (`attack doctor`, `attack the great doctor`)
    dispatches correctly to the attack handler, but then fails the same
    literal-`Name`-only re-check in `lib_battle_unnamed_target`. For a
    `TAF_VERSION_400` game, `lib_battle_npc_is_target`'s alias fallback
    branch (`else if (!lib_is_version_400(game) && ...)`) is unconditionally
    skipped, so aliases never count for this specific test at 4.0, even
    though they do for the grammar-matching stage.
  - The "Who do you want to attack?" continuation-prefix mechanism offers no
    escape either: the reconstructed line goes through the exact same
    unconditional splitter on the next turn.
  - There is no scripted bypass: `SCR_DUMP_TASKS=1` shows zero `WALK`/`EVENT`
    entries anywhere in the game, and the only tasks referencing either
    Battle Dome room are the NPC's own internally-fired "You Killed Him!!!"
    kill-tasks (unreachable) and their button-push follow-ups (gated on
    those kill-tasks).
  - Even setting the naming bug aside, the fight itself cannot resolve:
    `status` in battle shows the player's Accuracy fixed at `0-0`, and 40
    consecutive `wait` turns against the ally NPC Quzar's own fight with the
    Doctor never produced a single hit in either direction — this specific
    matchup is a stalemate by the numbers, not just bad luck.

  This is the same class of issue as `bedlam_solution.txt` and `Sandy.taf`
  (an authoring bug makes a required action permanently unsatisfiable), not
  a Scarier defect — `Dr. Myanus Hurts` is presumably meant to be addressed
  as "Doctor" or "the doctor," but the engine's battle-target check never
  accepts anything but the literal, unsplittable full `Name` string.

- **Wired as:**
  `illegalsocks_solution.txt|illegalsocks.taf|Your score is 745 out of a maximum of 2155.`,
  no env. The walkthrough plays to the best reachable state: solves the
  Sam/Tim/Tom/jump room, the computer/poster/controller puzzle, and the
  gas/matches secret door, meets the boss's ultimatum, then takes the
  teleporter into the Evil-side Battle Dome (+150, its own task), through to
  the Good room — killing Wauk and Trace via the typed `Get Sword`/
  `Get Armor` task commands (not the broken attack verb) plus the `points`
  bonus — and into the second Battle Dome, where the fight with the Doctor
  is confirmed permanently stalled. Final score: 745 of a possible 2155
  (34%).

## The walkthrough

```
Petter
male
use floppy disk
n
n
get book
get mom's card
s
w
use card
n
w
get all
e
n
e
buy
n
use teleporter
n
search sam
s
jump
w
search tim
search tom
e
e
use teleporter
put cd-roms in computer
move poster
take toaststation controller
n
d
use key
e
get flashlight
use flashlight
u
get matches
use gas
n
e
use teleporter
s
get sword
get armor
points
s
score
```

## With the engine's game patches on — still unwinnable

Illegal Socks is one of the four games in Scarier's targeted game-patch table
(`PATCH_TABLE` in `sctafpar.cpp`, applied at the end of `parse_game()` just
before `prop_solidify`, off unless `glk patches on` / `SCR_ASSUME_PATCHES=1`).
The patch matches on the game's name and author *and* on both copies of the
boss NPC still being named with the period, then takes it out:

```
NPCs/6/Name    "Dr. Myanus Hurts" -> "Dr Myanus Hurts"
NPCs/13/Name   "Dr. Myanus Hurts" -> "Dr Myanus Hurts"
```

That is the whole fix for the *naming* bug: with no `". "` in the Name, 4.0's
`run_find_split_400` no longer cuts `attack dr myanus hurts` in half, the
literal whole-`Name` test in `lib_battle_npc_is_target` matches, and the
Doctor becomes a legal attack target for the first time.

**It is still not winnable, and no data patch here can make it so.** With the
name fixed the fight actually starts, but it cannot be won on the author's own
numbers: every Accuracy in the game is 0 (so with faithful combat no blow ever
lands in either direction), and with the combat assist on to force hits, the
Doctor's 40 stamina / 35 strength / 20 defence against the player's 10 / 8 / 8
(+20 defence from the Full Suit of Armor) means his hit does 7 and kills in
two, while the player's 8 strength never beats his 20 defence at all. That is
a game that needs rebalancing, not a field fix, and rebalancing is outside
what this table does. The patched row therefore uses faithful combat and
asserts the miss line the fight now prints instead of "Who do you want to
attack?":
`illegalsocks_patched_solution.txt|illegalsocks.taf|The Great Doctor manages to avoid your attack with Awesome Sword|SCR_ASSUME_PATCHES=1`,
alongside the faithful row above; both are kept.
