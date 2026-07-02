# FinnsBigAdventure — walkthrough derivation progress (IN PROGRESS)

Deriving a full-win command script for **FBA v.3c.blorb** (Larry Horsfield,
MaxScore **500**). No public walkthrough exists (author emails it on request;
the in-game `WT`/`WALKTHROUGH` is vestigial — see the wiring TODO). Derived by
blind play through `test/a5run_dump` replay scripts, guided by the game's
**`WWDD`** ("What Would Dad Do?") per-location hint tasks and the model's
scoring tasks.

## Status: **160 / 500** (byte-verified in BOTH Scarier and FrankenDrift), script = `test/FinnsBigAdventure_walkthrough.txt`

> **UPDATE (session 2, 2026-07-02):** catacombs interior + Rock Island + Kong
> recon now fully derived and FD-byte-clean (only residual = one pre-existing
> intro blank line, `6a7`, unrelated to any move). Reached **160/500**, currently
> **On Pier 2 at Pirate Island** (`cl_Location55`). Sections 9–12 below are NEW.
> **Surfaced + FIXED a 2nd real Scarier engine bug** in the process (see
> `TODO_a5_walkthrough_bugs.md`): the `MoveCharacter … InsideObject/OntoObject`
> and `ToParentLocation` action forms were unhandled no-ops in
> `a5run_action.cpp`, so FBA's `hide in niche` (custodian stealth) never put the
> player inside `cl_Niche1` → the custodian "caught" you where FD plays
> "custodian goes past" (+5). Added the 3 cases (in/on set `char_onobj`+`char_in`;
> ToParentLocation clears them). **Corpus-clean** (all 25 games at baseline,
> vanilla). Rebuild: `make -f Makefile.headless a5run`.

> **Now derived against FrankenDrift (the reference runner).** Surfaced + FIXED a
> real Scarier engine bug in the process: the handfire "winks out when you walk
> into light" mechanic (a repeating TurnBased event → System task) stopped
> working after the first extinguish, because a plural `get X and Y` command's
> event-task path fired completion controls even on restriction failure, killing
> the handfire event (full write-up in `TODO_a5_walkthrough_bugs.md`, top entry).
> With that fixed, both engines behave identically. The script carries an extra
> `lumino` at the West End of Dungeons — needed by BOTH engines now, since the
> handfire correctly winks out during the lit-corridor sneak-out and the dungeon
> is entered dark. Drive FD with: `dotnet <fd-headless.dll> "<game>" <script>`.

The script is a WORK IN PROGRESS — it does NOT win yet, so it is **not** wired
into `run_a5_walkthroughs.sh` MAP. Re-run to confirm the checkpoint:

```
./test/a5run_dump "test/adrift5-games/FBA v.3c.blorb" test/FinnsBigAdventure_walkthrough.txt | grep scored
# -> You have scored 100 points out of a maximum of 500
```

### Sections SOLVED (all in the verified script)

1. **Handshake**: `o` / `no` / `b` (O to continue, NO to the visual-impairment
   graphics prompt, B to begin).
2. **Magor's Chamber** (opening, timed): stand up → `move pouffe by the fireplace`
   → `stand on pouffe` → `get tinderbox and telescope` (one turn) → `step down
   from pouffe` (NOT "get off" — only `cl_StepDown` clears `cl_OnStool`) → `move
   pouffe` (moves it back, +5; the pouffe bursts into flames ≈turn 7 near the
   fire = instant LOSS if you dawdle). `read parchment` (learns the Lumino/Snuffio
   handfire spell + sets `cl_NotesRead`, +5). 10 pts.
3. **Magor's Library** (E, dark): `conjure handfire` (or `lumino`), `look under
   desk`+`get paper`, `read notepad` (reveals "THE KEY!!!!" dungeon combo, +10),
   `x books`+`get thinnest book`, `write the key on the paper` (+5 — copies the
   combo onto the scrap so you have it in the dungeon), `read thin book` (+5,
   REQUIRED to be allowed to leave — points you to the dungeons). 25 pts.
4. **Your Room** (navigate W,out,E,in → dinner cutscene drops you here, dark):
   `lumino`; wear clothes from chest (`wear shirt/trousers/jacket` — shirt before
   jacket), `get boots`+`wear boots`; `remove slab` (+5) → `get gold coins` (from
   the hole) → `replace slab`; collect from table + under bed into the rucksack:
   toy hecatean soldiers + toy Xixon warriors (BOTH sets needed), telescope,
   tinderbox, parchment, paper, wooden horse, slingshot, dusty (pork) steak, ball
   of fluff → `put all in rucksack` between grabs (small hands). Coins do NOT go
   in the rucksack (theft) — they go in the **warbelt pouch**.
5. **Warbelt** (E → "Near Door of Your Room", `cl_Location80`): `get warbelt`
   (hangs on the door hook), `wear warbelt`, `put coins in pouch`. Wicker basket
   here has your discarded bedclothes (tunic/britches/blouse) — not taken; revisit
   if a later disguise needs them.
6. **Sneak-out**: `wear rucksack` (REQUIRED — leaving without it wearing is
   gated: "you might need something to carry things in"). Then E, out (now night,
   no dinner replay), out, w, w (spiral stairway), d (bottom), e, s (into the
   dungeon level). **NB: Your Room's "down" exit is a dead `<Dummy Location>`
   bug-trap (cl_Location6) — do NOT use it.**
7. **Dungeon combination lock** (the big one — fully reverse-engineered, 90 pts):
   The notepad KEY:
   ```
   1: Left to the right   2: Right to the left   3: Right to the right
   4: Leave alone         5: Left to the right, right to the left
   Barred: count 20 up 7 across - push   East: Pull   West: Leave alone
   ```
   Guard Room (In from the Passageway): `search detritus` (rusty key, +5), `get
   mug`. Fill mug at a slimy wall (`fill mug with slime`), `dip key in mug`,
   `unlock south door with rusty key` (cell 5, +5). Enter cell 5, `smear left
   ringbolt with slime` (does ALL cells at once, mug shatters, +5, and auto-kicks
   you back out to Near West End). Then re-enter each cell and turn ringbolts per
   the KEY — the 6 correct moves (cells 1,2,3 = 1 each, cell 5 = 2, + `pull sconce`
   at East End = 6) accumulate `cl_SecretDoor`→6 → door mechanism unlocks (+5+5).
   Cell 4 + West sconce = leave alone. Barred Cell: `count twenty bricks up and
   seven across` (+5) → `press brick` (+5, needs the ringbolts done) → `crawl into
   gap` (+5) → Small Chamber → `d` → Bottom of Stairway → `e` → **The Catacombs**.
   Dungeon map (extracted): cl_Location25 West End(N=passage,E=16);
   16 Near West End(N=18 Barred,E=22,S=20 Cell5,W=25); 22 Near East End(N=19 Cell4,
   E=17,S=21 Cell3,W=16); 17 East End(N=23 Cell2,S=24 Cell1,W=22); 15 Guard Room
   (Out=14 passage); 18 Barred(S=16,In=Small Chamber).
8. **Catacombs — spider web** (blocks south of the central aisle): `get fluff`,
   `get tinderbox`, `burn fluff with tinderbox` (+5), `throw fluff at web` (+5 —
   do it the SAME/next turn, the fluff burns out in ~2 turns; approaching the web
   with the tinderbox directly fails — the spider drives you off). 100 pts.

9. **Catacombs interior — custodian stealth + coffin scorpion** (SOLVED, 100→150).
   Map (aisle `<N>` markers): 49`<0>`(W=27 stairway,E=39); 39`<2>` central aisle
   (N=41,E=43,S=40 niche,W=49); 40`<3 Niche>`(N=39; **red-cross hiding niche**);
   42`<4 Coffin>`(S=43); 43`<5>`(N=42,E=46,S=45,W=39); 46`<8>`(N=44,E=48,S=47,W=43);
   47`<9 Bat>`(N=46); 48`<11>`(N=50,E=52,S=51,W=46); 50`<10>`(N=53,S=48);
   52 By Metal Doors(W=48,Out=90 Rock Is); 53 Custodian's Chamber(S=50).
   - **Custodian**: enter chamber 53 (`e e e n n` from 39) → he sees you
     (`cl_CustodianC=1`) and chases. **Flee with `run` cmds** (a scripted chase:
     `run south`×2, `run west`×3, `run south`) back to niche room 40, then
     `hide in niche` → **`wait`×2** (custodian must shuffle up and pass;
     `cl_CustodianG` +5, `cl_CoastIsCle=1`) → `out`. Fewer than 2 waits = the
     `cl_CaughtByCu` fake-ending (mother sends you to your room = loss).
   - **Chamber loot** (custodian now gone; re-enter `n e e e n n`): table has
     napkin+meal+ale — `get napkin`,`wear napkin`,`eat meal`(+5),`drink ale`(+5);
     `stand on chair`(+5, reveals gauntlet on shelf),`read ledgers`(business card
     falls),`get gauntlet`(while ON chair),`get off chair`,`get card`,
     `wear gauntlet`. `x diagram`(+5 map, red cross = niche 3). **`get napkin` +
     `put napkin in rucksack` BEFORE leaving** — the S exit is gated on the napkin
     (needed for the Pirate oven puzzle) else "something on the table you'll need".
   - **Coffin scorpion** (room 42, `s s w w n` from 53): `search behind coffin`
     (scorpion appears on coffin) → `knock scorpion off coffin`(+5, gauntlet
     needed) → `stamp on scorpion`(+5) → `get scorpion`(auto-rucksacks) →
     `search behind coffin` again (+5 lever) → `pull lever`(+5, `cl_LeverMoved=1`
     unlocks the metal doors).
   - **Bat** (room 47, `s e s`): entering auto-kicks something in the mist →
     `search mist`(dead bat appears) → `get bat`(auto-rucksacks).
   - **Leave**: `n e e` to 52 → `pull handle`(needs LeverMoved=1; doors open) →
     `out` → **Rock Island** (Location90). The door-exit REQUIRES `cl_Bat1` +
     `cl_Scorpion1` in the rucksack + gauntlet visible ("something left behind"
     otherwise) — bat+scorpion are later given to the **herbalist** on Sankora
     (`cl_GiveBatSco`, ingredients for a cure/powder).
10. **Rock Island** (SOLVED, 150→155). Hub. `ne` to the boat, `get in boat`,
    then **stow everything** (put tinderbox/card/gauntlet in rucksack; remove
    gauntlet — worn gauntlet blocks rowing "both hands free"), `get telescope`,
    `look SE through telescope`(+5 first-use `cl_TelescopeS`; the 4 dirs scout:
    SE=Kong/gorillas, NE=Pirate/wharf, E=Sankora/houses, W blocked). Boat is
    **beached**: on shore `push off` to float, then `row <dir>`. The E-side
    **keypad device** (buttons 0-9) is the *return* door mechanism (code TBD).
    Hint `cl_Island1` "Row SE first" → `row se` beaches on **Kong**.
11. **Kong/Gorilla Island — recon** (SOLVED, 155→160; full puzzle DEFERRED, needs
    the costume). Map: OnShore(E=124,In=boat); 124 Boundary(E=33,W=shore; has a
    **sapling** = Sankora Paddy-tether later); 33 Forest`<1>`(N=32,W=124,NE=36);
    32 Forest`<2>`(S=33,NE=34); 34 Small Clearing (**dung** appears after the
    gorilla defecates; `cl_RollInDung`); 36 Forest`<3>`(N=34,E=37,SW=33);
    37 Large Clearing (~12 gorillas + **rocky outcrop**, Up=54); 54 On Top of
    Outcrop (the **glowing orange orb** `cl_Stone2` in a boulder-niche; Down=37).
    - **Departure gate**: can't leave Kong until `cl_ObjectSeen=1` — at **room 36**
      `look east through telescope` (see the outcrop/boulder/orb) then `x outcrop`
      (+5 `cl_SeeStoneSc`). THEN back to shore, `push off`, `row n` → **Pirate**.
    - REMAINING Kong (needs items from Pirate): wear **gorilla costume** +
      `roll in dung`(+5 smell) to pass the gorillas; reach outcrop 54;
      `take orb`(+5 `cl_TakeStone` — this **spawns the silverback at boundary 124**);
      at 124 `burst bag`(+5, inflated paper bag scares it off — the "loud noise",
      `cl_Gorillaisl10-12`; mirrors how Finn scared sister Polgara pre-dinner);
      give the **wooden horse** to a mother gorilla → **ornament** (`cl_GiveHorseS`
      +5). Then row on to Sankora with the orb.
12. **Pirate Island — town** (IN PROGRESS from here, 160). Arrival = Pier 2 (55).
    Town map: 55 Pier2(N=56); 56 Blubber Wharf(E=57,W=58); 57 East End Wharf
    (N=59,S=61 Pier3); 58 West End Wharf(In=62 Warehouse); 59 Fisherman's Lane
    (N=63); 61 Pier3 (**pull rope→box/metal case**, `cl_PullRope`+5,
    `cl_OpenBoxSco`); 62 Warehouse (**newspaper**, `cl_ReadNewspa`+5); 63 Yardarm
    St(E=67,W=64,In=81 Pie Place); 64 Yardarm St2(N=65,W=68,In=79 Bookshop);
    65 Mansion House Rd(N=69); 66 Mannbroom's Academy (**learn to fight**,
    `cl_LearnToFig1`; In from 68); 67 East End Yardarm(In=70 **Fancy Dress Shop**);
    68(W=72,In=66); 69 By Large Mansion(E=74); 70 Fancy Dress Shop (**hire gorilla
    costume, 120 gold**, `cl_HireCostum`+5); 72 Outside Davy Jones' Locker(In=73);
    73 **Tavern** (menu/book, food ordering, **key** `cl_PutKeyOnHo`,
    `cl_Attable*`); 74 Grand Drive(E=71 Portico,SE=75); 75 Path Around Mansion
    (E=77); 76/78 **The Kitchen** (napkin-in-oven `cl_PutNapkinI`, `cl_Kitchen21`
    "get the key"); 77 Back of Mansion(In=76); 79 **Bookshop** (`cl_BuyBookSco`);
    81 Pie Place. **Economy**: gold starts 18 (from Your Room); need **120** for
    the costume; **ring a bell** `cl_PullBellpu1` gives **+150 gold** (location
    TBD, likely mansion); Sankora uses **goons** (money-change gold→goons,
    `cl_ExchangeGo*` / `cl_MoneyChang` "don't exchange ALL your goons").

### Sections REMAINING to script

- **Pirate Island** (12 above): buy/hire costume (need 120 gold → ring the bell
  for +150), paper bag (from tavern food?), pull rope on Pier 3 → box, read
  newspaper, tavern food+book+key, kitchen napkin-in-oven, learn to fight (?).
- **Kong return** (11 above): costume+dung → outcrop → orb → burst-bag escape +
  give-horse ornament.
- **Sankora Island + endgame** (`cl_BlankHint*`): disguise past guards, temple
  offering (flowers `cl_PutFlowers`), money-change, feed the dog (leash/collar,
  `cl_FeedDogSco`/`cl_MakeLeash`), rice-hat via slingshot+stone (`cl_ThrowObjAt1`),
  farm/goat/dead-kid, herbalist (give dead **bat**+**scorpion** → cure powder),
  **temple orb swap** `cl_GetOrbInTe` (+25, `cl_StoneRepla=1`, Goons=500) →
  `cl_ToEndgame` (+25 → **`YouHaveWon`**). Win marker TBD (grep `YouHaveWon`).

## Method / tooling (reuse for continuation)

- Full model dump: `./test/a5dump "test/adrift5-games/FBA v.3c.blorb" > /tmp/fba_dump.txt`
  (123k lines). Extracted helpers already saved:
  - `/tmp/fba_wwdd.txt` — all 90 `[wwdd]` hint texts (the puzzle skeleton).
  - `/tmp/fba_score.txt` — 116 scoring tasks with their exact `<Command>` syntax
    and point values (the action backbone; grep for the verb you need).
  - `/tmp/fba_locs.txt` — 130+ locations (key → short name).
- Replay loop: keep the growing master at `/tmp/fba_wt.txt`; append candidate
  commands, run `a5run_dump`, and isolate the NEW output by `awk` from the unique
  final master command (currently `/^> throw fluff at web$/{go=1} go`). Filter
  `grep -vE "handfire|^$"` to drop the repeated handfire status line.
- To decode a puzzle: find its scoring task in `/tmp/fba_score.txt`, then
  `awk '/<Key>KEY<\/Key>/,/<\/Task>/'` on the dump for its exact command +
  restrictions (the gating variables tell you the prerequisite order).
- Extract any location's exits: the per-`<Location>` `<Direction>`/`<Destination>`
  pairs (see the perl one-liner used for the dungeon map).
- `ulimit -t 40` on every run (Horsfield games are event-heavy).

## When it finally wins

Wire per the standard playbook: `test/a5_groundtruth.sh` in both RNG modes,
add a `FinnsBigAdventure|FBA v.3c.blorb|v|x` MAP line, and (if stable) a golden.
FBA is a Horsfield game with likely RAND-independent conformance — expect it to
behave like the other native-solution wins (LostLabyrinth / TheEuripidesEnigma).
