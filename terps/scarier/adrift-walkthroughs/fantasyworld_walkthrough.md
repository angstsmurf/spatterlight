# The Quest — walkthrough

- **Engine:** ADRIFT 4 (`fantasyworld.taf`, September 2001). The author signs
  himself only as *chlestron*; the readme calls the game *"my Adrift game
  lamely called The Quest"*. 250 tasks, 59 rooms, 45 NPCs, 63 objects, 8
  events. Adult game — the first command in the route is the author's own
  `NOSEX` switch, which turns all of that off.
- **Result:** **WIN** (`Congratulations!`). There is no score; the game's own
  progress meter is experience points, and the route finishes on **8535**,
  turn undead 3 / climb 3 / magic missile 1.
- **Solution:** `harness/fantasyworld_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`; win marker `Congratulations!`).
- **Provenance:** the game ships `walkthru.txt`, a 240-line bulleted action
  list with **no movement commands at all** and a wrong endgame ordering (see
  *Where the author's list is wrong*). The route below is derived here from
  the `SCR_DUMP_TASKS` dump and verified end to end.

## The shape of the game

You are the orphan hero of a mountain hamlet. A dragon has been eating the
livestock, the Elder sends you over the pass to King Harmon for help, and the
King turns out to be the actual villain — he wants a book so he can summon a
demon. The dragon is a red herring; the demon is the game.

Three systems run underneath:

- **Experience.** Every skill is bought from a trainer at a fixed threshold:
  turn undead 20/200/300, steal 500, magic missile 600/1200/1500, climb
  750/1000/1250. Only **climb 3 (1250 exp)** is load-bearing — `climb rock`
  at the Cleft in the rock (TASK 39, `RESTR type=4 v1=16 v2=3 v3=3`) is the
  one route to the temple, and the temple is where the Dark Gem is. Turn
  undead 3 is needed twice (the skeleton warrior gates the peak, the temple
  guard's ghost gates the Temple of Jedimah).
- **The farm.** `turn greater ghoul` in the upper Mountain Pass (room 17) is
  the only repeatable +50 exp task in the game. Twenty-one turnings is what
  pays for climb 3; that is why the solution file has twenty-one identical
  lines in a row. Everything else — the mage, the goblin chief, the Skull
  Collector, each demon wound — is a one-shot lump.
- **Gold.** You start with 1. `give amulet to king harmon` (TASK 91) is the
  only real income at +200. The Highwayman's toll north is 100 (TASK 44,
  one-shot), meat at the Inn is 1, and the poison is free on this route (see
  below). Nothing else in the run costs money.

## Map

North of the pass (your side):

```
   1 Elder's hut
   |n
   0 Hamlet --s-- 2 Trail out of village --s-- 3 The bridge --d-- 4 River Thale
                                                     |s
   7 Wilderness --e-- 5 The trail --e-- 6 Wilderness --e-- 8 Mage's Cabin
        |s                 |s                |s
   9 Graveyard          11 Trail --e-- 12 Wilderness --d-- 13 The hole
        |in                 |s
  10 Crypt            14 Mouth of the pass --e-- 15 Cleft --u-- 22 Chimney
                            |s                                       |e
                      16 Mountain pass --w-- 17 Mountain Pass    23 Stone Fortress
                                                 |s   \w                 |n
                                          18 Mountain peak  19/20 Cave  24 Temple
                                                 |s                      |e
                                          21 Mountain Pass          25 Fortress Keep
                                                 |s
                                          26 Base of Mountain
```

South of the pass (Harmon's Glade):

```
  27-31 Forest maze --s(31)-- 33 Dryad Grove          26 Base of Mountain
                                                            |s
  49 Meggle's House --e-- 35 Meggle's Farm --e--  32 Road  --e-- 34 Hobbles Farm --e-- 48 Hobbles House
                                                            |s
   50 Cemetary --e-- 45 Dark Alley --e--  36 Main Road  --e-- 37 Inn --e-- 39 Tavern
        |n              |d                                  |s              (38 = your room, s of 37)
   51 Mausoleum      47 Thieves Guild                  40 Main Road --w-- 44 University
                                                            |s
                              43 Throne Room --e--  41 Harmon's Keep --e-- 42 Dungeon --s-- 46 Cell
                                                                                |d
                                                              54 --w-- 53 Center of Catacombs --n-- 52 Catacombs
                                                               |w        |s        \e
                                                        58 Dark Room  56 --s-- 57 Guard Room   55
```

## Route

Line numbers are section boundaries in the solution file; every non-comment
line there is one turn (this game has **no** `<waitkey>` pauses at all, so the
file has no blank lines).

1. **Hamlet.** `nosex`, take the flower, `ask elder about quest`, `put hand on
   box` and `get wand`. `invite aline` / `invite mat` — you need both for the
   rest of the game and cannot dismiss them.
2. **The dragon.** `point wand at dragon` on the Trail out of the village,
   then `get scale`. This wounds the dragon; it matters again at the very end.
3. **The bridge and river.** `give flower to gelry` at the River Thale, `give
   lock to guard` on the bridge to open the trail south.
4. **The Mage's Cabin.** Pick up the rose on the way (needed only for the
   Elizabeth branch, but it is free) and the **mirror** — the mirror *is* the
   Dark Gem branch. `invite gail`, and Edwin teaches `train turn undead`.
5. **The graveyard.** `x grave` / `turn zombie` for the rope; `ask caroline
   about elder` in the crypt.
6. **The hole.** `tie rope to tree` in the eastern Wilderness — that is what
   opens the descent (`EXIT room=12 D gateTask=32`).
7. **Grind.** Ghoul in room 16, turn undead 2, then twenty-one `turn greater
   ghoul` in room 17, then turn undead 3.
8. **The Mountain Cave.** `resurrect dead mage` (room 20) leaves **human
   blood** and the King's stolen **book**, scatters the bandits, and moves the
   Bandit Lord down to room 17. It also seals the cave behind you.
9. **South.** `turn skeleton warrior` on the peak opens the way over.
10. **Meggle's Farm.** `search farm` then **`get nugget`** — the nugget lands
    on the ground, not in your hands.
11. **The University.** `give sphere to melvin` resurrects the Elder, which is
    what makes `ask elder about caroline` possible; Mary teaches `train magic
    missile`. Melvin takes two hits: `cast magic missile at melvin`, then
    `throw dagger at melvin`. He drops the **amulet**.
12. **The Throne Room.** `give amulet to king harmon` → +200 gold and the
    errand for the book.
13. **The Thieves Guild** (down from the Dark Alley): Marissa trains climb ×3.
14. **The Inn:** `buy meat`.
15. **North again.** `give gold to highwayman` (100), then `buy poison` from
    the Bandit Lord at the summit, then `poison meat`.
16. **The hole.** `cast magic missile at goblin chief` breaks his chant; only
    then does `kill goblin chief` work (Mat lands the blow). `get goblin
    blood`.
17. **The crypt.** `give goblin blood to caroline` → the **shrink potion**;
    `give human blood to caroline` → her promise that undeath will not claim
    you.
18. **The temple.** `climb rock`, east to the Stone Fortress. `kill ghost`
    beheads the guard but does not stop him; `turn ghost` does. North, east,
    and `put mirror on statue` — the mirror throws sunlight on the Altar of
    Silence, the altar shatters, Jedimah wakes, and the **Dark Gem** drops.
    `get dark gem`.
19. **Hobbles Farm.** `give meat to dogs` (poisoned) opens the house; `show
    gold to jed` sends Jed off to check Hal's fence; `open trunk`; `get
    circlet`.
20. **Dryad Grove.** `give circlet to merna` → the **Dryad's Kiss**.
21. **The betrayal.** `give book to king harmon`. He has you clubbed and
    thrown in the Cell.
22. **The escape.** `drink shrink potion` (Gail must be in the cell) puts you
    in the Dungeon. Down into the catacombs, south to the Ancient Guard Room,
    `reach into hole` to spawn the **Skull Collector** — nothing in your party
    can scratch it — and lead it west to the **Shade** in the Dark Room, which
    can. Three turns of waiting and it falls apart; `get heart`, take the
    heart up to the jailer for the key, `open door`.
23. **The sacrifice.** At the Base of Mountain, `ask harmon about book` makes
    the King demand a virgin. `suggest king` is the only answer that spends
    nobody you have met — he finds his own child to bleed, the gate opens, and
    the demon takes him.
24. **The alliance.** Now `ask merna about demon`, `ask jedimah about demon`,
    `ask caroline about demon`, and `ask dragon about demon` **twice** — the
    second asking produces the **Ruby Box**. `put dryad's kiss in box` and
    `put dark gem in box`.
25. **Sealing the gate.** Back to the Base of Mountain: `throw ruby box into
    gate`. The gate collapses and the demon is cut loose; from here it follows
    you room to room and neither of you can hurt the other.
26. **The five wounds**, in the only order the game accepts: **Jedimah** at the
    Fortress Keep, **Merna** in the grove, the **dragon** on the village trail,
    **Caroline** in her crypt, **Gelry** in the river. Each `kill demon` is
    gated on the demon-health variable left by the one before.
27. `ask dragon about quest` → the dragon flies north, and *Congratulations!*

## Where the author's list is wrong

- **`SUGGEST` comes before the allies, not after.** `walkthru.txt` says
  *"Don't suggest anybody yet"*, then has you ask Merna, Jedimah, Caroline and
  the dragon about the demon, and only then suggest a victim. That cannot
  work: TASKs 131/137/139/140 (the four `ask … about demon` tasks) all carry
  `RESTR type=4 v1=28 v2=2 v3=1`, and the variable they test is set **only**
  by the four working `suggest` tasks (120/123/125/127/129 all end
  `ACT type=3 v1=26 v2=0 v3=1`). Ask first and every ally gives you their
  generic brush-off — *"Pray that you don't meet one"* — and you never get the
  Ruby Box. The demon has to be loose before anyone will talk about it.
- **`get gold` is missing.** `search farm` (TASK 60) puts the nugget on the
  ground, not in inventory. Without `get nugget` the whole Hobbles House
  branch — and therefore the circlet, and therefore the Dryad's Kiss, and
  therefore the Ruby Box — is unreachable. The author lists `get gold
  <nugget>` in the flat action list but it is easy to read as a gloss on
  `search`.
- **The poison price is inverted.** *"you can buy earlier, but you'll have to
  pay"* is right, but which task fires is decided by whether the **mage** has
  been resurrected: TASK 113 (mage *not* resurrected) wants gold > 20 and
  charges 20; TASK 114 (mage resurrected, 113 not yet used) hands it over for
  nothing. Doing the mage first — which you want anyway, for the book — makes
  the poison free.

## Notes and traps

- **Two winning endgames.** `ACT type=6 v1=0` appears exactly twice. TASK 148
  wins at the River Thale if the dragon **died** fighting the demon; TASK 147
  (`ask dragon about quest`) wins if it survived. Which one you get is decided
  by the dragon's health variable at the moment of TASK 143/144 — this route
  wounds the dragon once, early, with the wand, and it survives, so the run
  ends on the dragon's farewell rather than at the river. It is the better
  ending: the dragon leaves of its own accord, alive, and the hero keeps his
  word without killing anything.
- **The temple guard's five turns.** `kill ghost` starts EVENT 0
  (`pissedTempleGuard`, 5 turns) whose affected task is TASK 51
  `Templekillsplayer` — an unrestricted `ACT type=6 v1=2`, i.e. instant death,
  scoped to rooms 23/24/25. `turn ghost` does not cancel it. The route spends
  exactly four turns inside the fortress after the kill, which is why the
  ordering there is tight.
- **Don't invite the Shade.** `invite shade` (TASK 96) is worth +50 exp over
  the plain route and starts EVENT 4, a ten-turn clock ending in TASK 100
  `shadekillsplayer`. The route asks the Shade about the light for the scene
  and then leaves it in the dark.
- **Don't cast at the dragon late.** The author warns about this and the dump
  agrees: the dragon has to be present for TASKs 143/144 and 147, and hitting
  it once the demon is loose drives it off for good.
- **`kill goblins`** (TASK 33) is a repeatable life-point sink, and TASK 34
  turns it into an instant loss when you are down to your last one. The chief
  is the target, not the mob.
- **The Elizabeth branch is the other half of the fork.** `throw ruby box into
  gate` has two versions: TASK 151 wants the **Dark Gem**, TASK 152 wants
  **Jedimah's Holy Symbol**, and both want the Dryad's Kiss. The Holy Symbol
  comes from `pleasure statue` in the Temple of Jedimah, which needs Aline
  unarmoured and is the entry point to the game's adult content. The mirror
  route reaches the same object slot with no such requirement, which is why
  `nosex` and the Dark Gem go together.
- **Debug commands left in.** `gamestatus` and `arousalstatus` are documented;
  `iamcheatinggimmeexperience` (TASK 135, +2000 exp) and
  `iamcheatinggimmearousal` (TASK 40) are not. `summon muse` spawns the
  in-game hint character next to Aline — the readme explains that ADRIFT will
  not let the author spawn an NPC into the player's own room.
- **`wait` is not one turn.** SCARE's library `wait` runs several turns at
  once; the three `wait`s in the catacombs are what let the Shade finish the
  Skull Collector. Replacing them with three no-op commands does not work.
- **Naming.** The game opens with a bare `Please enter your name:` prompt
  before the first turn. The solution file answers `Corin` on its first
  non-comment line; the golden was blessed with that name, so changing it
  changes the transcript.
