# Yon Astounding Castle! of some sort — walkthrough

- **Engine:** ADRIFT 4.0. *Yon Astounding Castle! of some sort*, by
  **Tiberius Thingamus** (Duncan Bowsman), IFComp 2009 — but the file wired
  here is **"Ye Second Version", 31 January 2010**, not the comp release.
- **Game file:** `yonastoundingcastle.taf` (copied into `games/`).
- **Solution:** `harness/yonastoundingcastle_solution.txt` — 187 commands.
- **Result:** ★ **WON**, `Incredible victory!`.
  `FINAL SCORE: YE OLDE INNKEEPER`, 5 units of treasure kept.
- **Row:** `yonastoundingcastle_solution.txt|yonastoundingcastle.taf|Incredible victory!|SCR_SKIP_WAITKEY=1`
- **Row env:** `SCR_SKIP_WAITKEY=1` — the title crawl ends in
  `[presseth of yon return key to continue]`, which would otherwise eat the
  first command of the script.

A 62-room mock-medieval castle crawl written entirely in fake Middle English
("Ye standeth in ye repository of all things flake-like"). You are cursed by
Malwegor the Ice Wizard; the win condition is to reach his lair and take the
royal scepter. Everything else is treasure-collecting, which is what the
final rank is computed from.

## The shipped walkthrough does not match the shipped game

`YAC_walkthrough.doc` is dated **29 September 2009** and documents the IFComp
release. The `.taf` is **"Ye Second Version" (31 January 2010)**. The doc's
two routes ("Ye Olde Exciting Route" and "Ye Olde Speedy Route") both walk
straight into v2 changes and stall. The committed solution is the *Speedy
Route* re-derived against the v2 task table, with two additions (see
[Beyond the shipped route](#beyond-the-shipped-route)).

What changed, confirmed against `SCR_DUMP_TASKS` output:

| shipped walkthrough (v1) | "Ye Second Version" (v2) |
| --- | --- |
| the riddling gnome guards the **drawerbridge**; answers `clock` / `mamy` / `envelope` / `yorick` | the gnome guards the **Skull Gate** (room 42), astride the giant slug; answers `language` / `footsteps` / `yorick` |
| — | the gnome first asks *"Shall ye attempteth yon riddles, adventurer? (Y/N)"* — a bare `y` is required before the first answer |
| `klarthaphmo` at the ice platform, then `get speechery` | `klarthaphmo` is spent transforming the **giant slug** into the shiny orb; the frozen speechery (obj 110) is referenced by **no** v2 task |
| the treasure sack is safe | **Goblin Bob** ambushes you and robs the sack; stolen goods pile up in the takery trunkle (obj 60, room 22) |
| `get orb` works bare-handed | the shining orb "slippeth from ye grip" unless you are wearing the **magical oven mitt** |
| `unlock door` opens the golden door | `unlock door` only *unlocks* it — `open door` is a separate turn |

`mamy` and `envelope` have **zero** occurrences in the v2 task table;
`language` (8), `footstep` (5) and `yorick` (4) are the v2 answers.
`TASK 513 [* klarthaphmo *]` is a shortcut past the gate but forfeits the
brick o' gold, so the route answers the riddles properly.

## Route

Phases, in the order the solution file comments them:

1. **Lower the drawbridge.** `u` into the nut tree, `e` along the branch,
   `open contraption` in the gatehouse. Two nuts (one from the tree, one from
   the ground) — `give nut to squirrel` yields the **gem of some sort**,
   treasure 1.
2. **Three silver keys**, all hidden behind scenery examines:
   `x greenery` (courtyard), `x pile` (flakery), `x placard` (passageway o'
   paintings). All three are spent later; the route ends holding none.
3. **The hovel and the beltery.** Sooty `string` upstairs → `unlock table` →
   the resizing wheel cronkle. `put wheel on device` repairs the resizer;
   `x rack` reveals the **intercontinental title** (treasure 2) and the magic
   belt. Three `pull lever`s shrink the belt to a wearable size.
4. **The drawerbridge and the slug.** `open wall`, then `klarthaphmo` turns
   the giant slug into the **shiny orb** (treasure 3). It falls down the
   staircase and cannot be picked up until the oven mitt is worn, so the
   route detours to the bakery first and comes back for it.
5. **Bakery / flakery.** `get mitt`, `wear mitt`, then `get orb`, then
   `open oven` for the **Fabergé muffin** (treasure 4).
6. **Ye Room of Yon Intricate Object.** The bottle opener, the magic boat
   (from `x fount`), and the fruit that pacifies the corridor monster.
7. **Alchemical storage.** `unlock desk`, `x it`, `get crystal` — the magic
   crystal, which is thrown at the lava boss to win the **bag o' sleeping
   powder** (obj 89).
8. **The secret treasure room.** From Ye Pointless Parlour, plain `in` leads
   to "YON SECRET ROOM" and the **chalice** (treasure 5). Undocumented in
   the shipped walkthrough.
9. **Ye Makery.** Reachable only by sailing `n` across the lake in the magic
   boat. `make item` → `melting wand` builds Fred, the endgame weapon.
10. **Ye Takery.** `sleep snakes` opens the way north; `x trunk` shows
    everything Goblin Bob has taken, and it can simply be picked back up
    (`get title`, `get gem`).
11. **Ye Cow Feedery.** `put grasses in hopper` (grasses taken from the
    Lakery in passing) puts the bovine to sleep; `get bell` takes the
    **golden cowbell** (treasure 6).
12. **The toll boothe.** `give treasure to dwarf` demands *three* named
    treasures (`TASK 984`, `RESTR type=4 v1=4 v2=3 v3=3` — variable 4 ≥ 3).
    The route pays `title` / `muffin` / `shiny orb` and keeps the gem, the
    cowbell and the chalice.
13. **The Skull Gate.** `y`, then `language` / `footsteps` / `yorick`. The
    gnome turns into the **brick o' gold** (treasure 7 collected, 4 kept) and
    the slug clears the nostril passage.
14. **Ye olde labyrinth.** `get fungus`, carry it west, `drop fungus` in the
    ogre's room — the Guardian Ogre eats it and dies. `get key`,
    `unlock door`, **`open door`**, `w`.
15. **Malwegor's lair.** A scripted `y`/`n` exchange, then `melt wizard` →
    `melting wand` → `look` → `get scepter` → `y`. Victory.

## Goblin Bob is on a turn timer

`TASK 588 #gob_steals!` is gated on `TASK 578` (`*sleep*goblin*` /
`*sleep*bob*`) **not** being complete — sleeping him with the bag o' sleeping
powder would stop thefts permanently — but the ambush fires the same turn he
appears, so in practice you cannot get the command in first, and the route
never manages to sleep him.

What the route does instead is dodge him by parity. Immediately after the
takery reclaim there is a single **`x hamish`** on the way out of the Quakery.
It is a deliberate delay turn: without it, Goblin Bob is standing in the
Makery on the very next move and robs the just-reclaimed intercontinental
title straight back out of the sack, which then makes it un-nameable at the
toll booth and derails the bribe. Any command that burns a turn works; the
route uses `x hamish` because he is standing right there and it produces a
real reply rather than a parser error.

Because the harness pins the RNG (`harness/seed.cpp`), this timing is exactly
reproducible; but it does mean the route is turn-count sensitive, and
inserting or removing a command anywhere before the Makery can re-arm the
theft.

## Beyond the shipped route

Two treasures the shipped Speedy Route does not take are collected here:

- the **golden cowbell** (`TASK 826`, room 24 Ye Cow Feedery, gated on
  `TASK 828 #IN_HOPPER` — the grasses (obj 63) from the Lakery must go into
  the hopper (obj 67));
- the **chalice** in the secret room off the Pointless Parlour.

Together they lift the ending from `YON SCROLL SORTER` (3 treasures) to
`YE OLDE INNKEEPER` (5).

## What this route leaves on the table

The author's own warning — *"ye walkthroughs shan't getteth ye yon best
possible ending… 'twill taketh ye brow sweats & such brain twistery for total
victory!"* — still applies. The one collectable treasure this route skips is
the **antique stamp collection** (`TASK 926` / `TASK 927`, room 40 Ye Dark &
Dank Dungeon, behind a `push stone` / `get stone` in the wall).

It is skipped deliberately. Room 40 is behind the olde dungeon door (obj 91),
whose nominal key (obj 95) is named `nonexistente`; the real opener is
`TASK 916`, which wants Fred held **and** Fred made in the shape of a
*tungsten key*. Fred can only be re-shaped while standing in Ye Makery
(*"Ye can always re-maketh ye Fred in another image if ye again typeth 'make
item' whilst in ye makery"*), and Ye Makery is across the lake. Taking the
stamps therefore means making the tungsten key, marching to the dungeon,
marching all the way back across the lake, and re-making Fred as the melting
wand — roughly 25 extra moves, all of which would re-arm Goblin Bob's timer —
for one treasure. Room 40 also contains Thrug the ogre (`TASK 929
#stamps_held_(thrug)`), so it is not a clean grab either.

The three treasures handed to the dwarf are gone for good; there is no
cheaper toll.

## Notes for the engine

Nothing in this game exercised a scarier bug — the 187-command transcript
contains no parser errors and no failure messages, and every puzzle behaved
as the task table predicts. It is a good stress case for v4 breadth, though:
62 rooms, `%object%`-bearing ALTCMDs, room-scoped and multi-room tasks,
conversation-style `Y/N` prompts, an interactive named-object prompt loop
(the dwarf's bribe), boat travel between otherwise disconnected map halves,
and a wandering NPC that mutates the player's inventory on a timer.
