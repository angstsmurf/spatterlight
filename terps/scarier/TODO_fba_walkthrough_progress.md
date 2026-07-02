# FinnsBigAdventure — walkthrough derivation progress (IN PROGRESS)

Deriving a full-win command script for **FBA v.3c.blorb** (Larry Horsfield,
MaxScore **500**). No public walkthrough exists (author emails it on request;
the in-game `WT`/`WALKTHROUGH` is vestigial — see the wiring TODO). Derived by
blind play through `test/a5run_dump` replay scripts, guided by the game's
**`WWDD`** ("What Would Dad Do?") per-location hint tasks and the model's
scoring tasks.

## Status: **100 / 500** (verified), script = `test/FinnsBigAdventure_walkthrough.txt` (100 commands)

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

### Sections REMAINING (WWDD + scoring-task notes — not yet scripted)

- **Catacombs interior**: central aisle N=coffin room, S(cleared)=coffin niches
  with one **empty niche (lowest)** = the custodian hiding spot (`cl_Niche12`
  "hide beyond the spider"), E=toward the **Custodian's Chamber** (brighter
  upright lights). WWDD: `cl_Coffin*` scorpion-on-a-coffin puzzle (knock it onto
  the floor — protect your hand — then stamp/kill it before it escapes; something
  behind the coffin); `cl_Cust*` custodian stealth (use the **telescope** to
  watch him, hide in the empty niche, read the **ledgers** on his shelf → see a
  **map** = `cl_SeeMapScor`/`cl_CustodianG`). Then a **boat** to the islands.
- **Islands** (each a self-contained puzzle cluster; WWDD keys in parens):
  Rock Island (`cl_Rockisland*`, row SE first), **Kong/Gorilla Island**
  (`cl_Gorillaisl1-12`: gorilla-costume disguise + smell + paper-bag bang to scare
  the gorilla, telescope, workbench/metal-case), **Pirate Island**
  (`cl_Pier3*`, warehouse newspaper, book/menu/food-ordering at the tavern, key),
  **Sankora Island** (`cl_BlankHint*`: disguise past guards, temple offering,
  money-changing "don't exchange ALL your goons", feed the dog, rice-hat via
  slingshot, farm/goat/dead-kid, herbalist). Win = `YouHaveWon` location.

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
