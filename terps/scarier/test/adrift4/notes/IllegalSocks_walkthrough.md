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

## The swing is Runner-measured -- and names the sword by its Alias

Driven through run400 under Wine on the patched file (2026-09-26/27): the patch
works there too (`attack dr myanus hurts` now answers "What do you want to
attack Dr Myanus Hurts with?"), and the marker above is the Runner's own text.
`attack dr myanus hurts with sword2` strikes in one turn and prints "The Great
Doctor manages to avoid your attack with Awesome Sword."; `status` then reads
"You are wielding Awesome Sword", hit 28 (20) (Adrift_305_socks6.txt line 119).
The golden plays that command for exactly this reason.

`with awesome sword` -- the obvious phrasing, and the one the golden used first
-- is where the engines part, and **not because of scope**: both swords are
carried at the Battle Dome in both engines (`i` agrees line for line; the Cool
Sword starts hidden but a task hands it over), and "You are wielding nothing"
is just what either engine prints before a first successful strike. It is 4.0
weapon-term resolution, and the probe series
`harness/make_400_battlewith{2,3,4,5}probe.py` has now measured the rule over
68 cases -- the fifth generator's docstring is the full record. Two parts of it
decide this game:

- **run400 compares an object's Short case-sensitively** to the lower-cased
  input, while it compares Aliases and Prefix words without regard to case. A
  Short stored capitalised is therefore unreachable by typing, and Illegal
  Socks capitalises everything. Both swords' Short is "Sword", so neither
  answers to "sword" that way; only Cool Sword's Alias "Sword" does. Awesome
  Sword's Alias is "Sword2", and its Prefix "Awesome" cannot make it a
  candidate, so the Awesome Sword is not in the running at all.
- **The winner then has to pass an ambiguity gate**, and Cool Sword fails it:
  it matched through an Alias that is another object's name (the Awesome
  Sword's Short). Nothing binds.

The catch-all that follows comes from a separate, ordinary name resolver --
Short 1, first matching Alias +1, +1 per matching Prefix word, and a Prefix
match enough on its own -- under which the two swords tie at 1. A tie there
names no object, which is why the message is the *character* catch-all "I don't
understand what you want to do with Dr Myanus Hurts." and takes no turn
(Adrift_306_socks7.txt; the same against the one-word Quzar, so the duplicate
NPC names are not in it). A strict winner there that is not a usable weapon
gives the *object* catch-all instead ("... what you want me to do with Full
Suit of Armor."), also turn-free.

The capitalisation half shows up outside the battle path too: `with armor`
finds no object at all although the Leather Armor is worn, while `with full
suit of armor` reaches the Full Suit through its three Prefix words
(Adrift_305_socks6.txt). So `with sword2` is not a workaround, it is the only
phrasing that reaches the Awesome Sword in run400.

Scarier asks "Which Sword?  Cool Sword or Awesome Sword?" and swings on the
repeat, because it matches the Short without regard to case and has no such
gate: `lib_battle_scan_with()`'s walk-every-named-object / last-weapon-wins was
measured on thesorc, a 3.90 game, and over-fires at 4.0.

That is kept as a **deliberate deviation**, written up in the comment above
`lib_battle_scan_with()` in `sclibrar_battle.inc`. The 4.0 rule is a parser
accident rather than a design: it makes an author's own naming unreachable --
here it hides both swords behind their capitalised Shorts and then throws out
the one object that did match -- so honouring it would refuse commands the game
was written to accept. The divergence is one-directional, Scarier accepting
strictly more phrasings and never fewer, and this game is the only place in the
corpus where it shows. The golden plays `with sword2` regardless, which binds
under both models, so the row is Runner-measured either way.
