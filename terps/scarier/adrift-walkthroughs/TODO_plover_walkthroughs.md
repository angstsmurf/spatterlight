# TODO — add the "Plover" walkthrough corpus to Scarier's automated testing

> **PARKED 2026-07-13.** Everything below is committed and the tree is green
> (`make -f Makefile.headless test` exits 0; v4 suite **23/23 PASS**). This
> session closed: the `make test` wiring (§7), the `coloromc.taf` "load failure"
> (it is an ADRIFT 5 game — §TL;DR), and **Topaz**, which was never unwinnable —
> it was a SCARE bug affecting every v4 game (§2, `Topaz_walkthrough.md`, and
> §7b for the run400 anchors). A second `build.sh` link break was fixed on the
> way (§4).
>
> **Resume points, smallest first:**
> 1. **Port-based re-derivations** (§5, priority item 5) — *The Thorn*,
>    *Goldilocks is a FOX!*, *Renegade Brainwave*. All three `.taf`s are on disk
>    and load; only the *walkthroughs* are for the Inform ports, so each needs
>    re-deriving against the ADRIFT original with the §6 recipe. This is the
>    obvious next chunk.
> 2. **An unexplained Runner divergence in Topaz's conversation menu.** Petter
>    ran the real ADRIFT 4 Runner and got stuck: after the two numbered answers
>    every later command replied *"Choose an option to speak"*, as if a menu were
>    still open. Under SCARE the menu is exactly **two** answers deep (`1` →
>    Topaz introduces himself → `1` → *"You will explore now, mortal"*) and then
>    play continues. The menu is not an engine feature — the author faked it with
>    two dummy rooms (4 and 5, both "Darkness") and tasks whose command is
>    literally `1`/`2`, each doing a move-player action. So a disagreement here
>    smells like an **off-by-one in the move-player destination** — the same
>    family as the restriction bug just fixed, and worth a run400 check of the
>    move-player action (does it store `roomIndex + 1`?). Topaz still wins under
>    SCARE regardless; this is about faithfulness, not winnability.
> 3. **Source the 4 missing games** (§3) — needs downloads, not code.

Pairing + integration plan for the 18 walkthrough web-archives in
`~/Desktop/Plover adrift walkthroughs/` (David Welbourn's *Key & Compass* /
Plover.net solutions, plus a few comp round-up pages).

Investigated 2026-07-02. Everything below was verified by running the games
through the committed headless SCARE harness
(`adrift-walkthroughs/harness/scare`) — titles, versions and load-ability are
observed, not guessed.

---

## TL;DR

* **Nearly all of these games are ADRIFT 3.9 / 4.0**, so they belong to the
  **SCARE (v4) walkthrough harness** in `adrift-walkthroughs/harness/`, not the
  ADRIFT-5 harness in `test/`.
  **Correction (2026-07-13):** the original claim that *none* are ADRIFT 5 was
  wrong. `coloromc.taf` (*Color of Milk Coffee*) is **ADRIFT 5.00** — its header
  is the v5 signature (byte 8 = `0x92`, version tag ASCII `02…`, where 3.9/4.0
  have `0x94`/`0x93` and end `39 fa`), followed by a plaintext Babel iFiction
  block (IFID `ADRIFT-500-…`) and then the compressed payload. SCARE is right to
  reject it; the a5 engine plays it fine. It is already wired into the **a5**
  corpus as `ColorOfMilkCoffee|coloromc.taf|0|0` (FrankenDrift-verified) — see
  `test/run_a5_walkthroughs.sh`. Nothing to do.
* The 18 archives cover **~35 distinct games** (two of them are comp round-up
  pages listing many games each). Of those, **~29 are ADRIFT games with a
  usable command walkthrough**; the rest are non-ADRIFT ports, a map-only page,
  or already-covered.
* **2 are already in the harness**: *Ice Cream* and *The Cat in the Tree*
  (`icecream_solution.txt`, `the_cat_in_the_tree_solution.txt`).
* **4 game files are missing from disk entirely** and must be sourced before
  anything can be tested (see §3).
* ~~**Prerequisite:** `harness/build.sh` is stale~~ — fixed (§4), twice: once for
  the `terps/scare` → `terps/scarier` rename, and again in 2026-07 when
  `scmap.cpp` started calling `map_free()` from the un-globbed `mapdraw.cpp`.

---

## 1. Pairing table — individual walkthroughs

Game-file paths are under `~/Downloads/`. "WT engine" = the platform the
*walkthrough* was written against (Key & Compass often solves the Inform/Z-code
*port*, not the ADRIFT original — see §5).

| # | Web-archive | Game / author | ADRIFT ver | Game file on disk | WT engine | Status |
|---|-------------|---------------|-----------|-------------------|-----------|--------|
| 1 | A Masochist's Heaven | *A Masochist's Heaven* — Tne Mad Monk (1st 1-Hr Comp 2002) | 4 | **— MISSING —** | ADRIFT | need game (§3) |
| 2 | Color of Milk Coffee | *Color of Milk Coffee* — Bahri Gordebak (InsideADRIFT #41) | **5** | `Adrift games/InsideADRIFT_41/coloromc.taf` | Inform 7 | ✅ **DONE — it is an ADRIFT 5 game**, not v4; already in the a5 corpus (MATCH 0\|0). Not a SCARE loader gap. |
| 3 | Man Overboard!!! | *Man Overboard!!!* — TonyB (Writing Challenges Comp 2006) | 4 | `Adrift games/ifcomps_v4_new/man overboard.taf` | ADRIFT | ✅ **DONE** — wins (boat ending), in harness |
| 4 | Pieces of eden | *Pieces of eden* — Nicodemus (Comp With No Name 2008) | 4 | `Adrift games/adrift_offarchive_new/Pieces of eden.taf` | ADRIFT | ✅ **DONE** — wins (`END OF PART ONE`), in harness |
| 5 | The Princess In The Tower | *The Princess In The Tower* — David Whyld (1st 1-Hr Comp 2002) | 4 | `Adrift games/1hourgamecomp/princess1.taf` (title confirmed) | ADRIFT | ✅ **DONE** — wins (`It seems you've won.`), in harness |
| 6 | The Thorn | *The Thorn* — Eric Mayer / David Whyld (Summer MiniComp 2003) | 4 | `Adrift games/Minicomp/Thorn by Eric Mayer/Thorn.taf` | Inform (Z) | ready, but WT is for the Inform port (§5) |
| 7 | Too Much Exercise | *Too Much Exercise* — Robert Street (Writing Challenges 2006) | 4 | `Adrift games/ifarchive_v4_new/exercise.taf` | ADRIFT | ✅ **DONE** — wins (`Congratulations!`), in harness (worked example, §6) |
| 8 | Goldilocks is a FOX! | *Goldilocks is a FOX!* — J. J. Guest (2002) | 4 | `Adrift games/goldilocks.taf` | Inform 6 | ready, but WT is for the Inform 6 port (§5) |
| 9 | Renegade Brainwave | *Renegade Brainwave* — J. J. Guest (Ectocomp 2010) | 4 | `Adrift games/Ectocomp_2010/Renegade_Brainwave.taf` | Inform 7 | ready, but WT is for the Inform 7 port (§5) |
| 10 | Yak Shaving for Kicks and Giggles! | *Yak Shaving…* — J. J. Guest (The Odd Competition 2008) | 4 | `Adrift games/yak_shaving.taf` | ADRIFT (V1) | ✅ **DONE** — wins (V1 section; `completed the Odd Competition`), in harness |
| 11 | Archie's Birthday – Ch.1: Reggie's Gift | Purple Dragon (2005) — **adult content** | 3.90 | **— MISSING —** | ADRIFT | need game (§3); decide whether to include NSFW game |
| 12 | Quest for the Magic Healing Plant | Adam G. Crutchlow (1995/2002) | 3.9 | **— MISSING —** (only AGT/Inform ports ever existed; none on disk) | Inform 6 | need game (§3); low priority |
| 13 | ADRIFTMAS Party | Mystery (2002) | 4 | `Adrift games/adrift_offarchive_new/ADRIFTmas Party.taf`, `Adrift games/aparty.taf` | — | ⚠ archive is **MAP ONLY** — no command list to convert |

---

## 2. Pairing table — comp round-up pages (multi-game)

### ADRIFT 2nd 3-Hour Comp 2004 — all 6 present in `~/Downloads/3hrcomp/`
All load & titles verified. **✅ ALL 6 DONE** — derived, winning, blessed, in
`run_v4_walkthroughs.sh`. See each `*_walkthrough.md`.

| Game / author | ADRIFT | File | Status (win marker) |
|---|---|---|---|
| Buried Alive — David Whyld (1st) | 4 | `buried.taf` | ✅ `Well done. You got to the end` |
| The Murder of Jack Morely / "Confession" — Mystery (4th=) | 4 | `Confession(1).taf` | ✅ `Striking a plea deal` (non-interactive; z-spam) |
| Snakes and Ladders — Ken Franklin (4th=) | 4 | `sandl.taf` | ✅ `made it to the end of the game` (seeded, 39 rolls) |
| Veteran Experience — Robert Street/Rafgon (2nd) | **3** | `veteran.taf` | ✅ `fulfilling your destiny` (needed `take crowbar`) |
| We are coming to get you! — Richard Otter (3rd) | 4 | `togetyou/togetyou.taf` | ✅ `another flesh-sack` (`smash`, not `hit`, adenoids) |
| Zombies Are Cool… — Mel S (6th) | **3** | `ZAC.taf` | ✅ `you and Stu were eaten by zombies` (win = death) |

### ADRIFT 4th 1-Hour Comp 2004 — all 16 present in `~/Downloads/4th1hrComp/`
(The three `4th 1-Hour Comp` archives — base, "(Adrift Maze)", "(Cruel and
Hilarious Punishment)" — are **byte-identical**; it's one walkthrough page
saved thrice.)

Only **7 of the 16** have an actual command walkthrough on the page; the other
**8 are map-only** (room diagrams, no command list) and one (*Spam*) is broken.
Of the 7 with walkthroughs, 2 were already carried and **5 are now DONE**
(Topaz included — see §5).

| Game | File | Status |
|---|---|---|
| ADRIFT Maze | `ADRIFTMaze.taf` | ✅ DONE — `You WIN!` (name prompt eats 1st move; seeded troll teleport) |
| Cruel and Hilarious Punishment (v3.9) | `CAH.taf` | ✅ DONE — `destroyed our reality` |
| Get Treasure For Trabula | `Trabula.taf` | ✅ DONE — `given the gold coins to Trabula` (score 125; `attack` not `kill`) — see `Trabula_walkthrough.md` |
| Shred 'Em | `shreddem.taf` | ✅ DONE — `Due to lack of evidence` |
| The Cat in the Tree | `TheCatintheTree.taf` | ✅ already in harness |
| Ice Cream | `IceCream.taf` | ✅ already in harness |
| Topaz | `topaz.taf` | ✅ **DONE — WON** (`The two of you set out into the forest.`); the "unwinnable" verdict was a SCARE bug, now fixed — see `Topaz_walkthrough.md` |
| The Quest for Spam | `SPAM.taf` | ⛔ broken/incomplete game (crashes the Runner) — no walkthrough on page |
| Agent 4-F From Mars | `agent_4F[1].A.taf` | — map-only page, no command walkthrough |
| ARGH's Great Escape | `ARGH_sGreatEscape.taf` | — map-only page |
| An Evening With The Evil Chicken Of Doom (v3.9) | `ECOD3.taf` | — map-only page |
| Goblin Hunt | `goblinhunt.taf` | — map-only page |
| Undefined | `Undefined1.taf` | — map-only page |
| Vagabond | `Vagabond.taf` | — map-only page |
| Woof | `Woof.taf` | — map-only page |
| Wreckage | `Wreckage.taf` | — map-only page (a `wreckage_walkthru.txt` ships with the game) |

### IntroComp 2005 — only ONE ADRIFT game on the page
| Game / author | ADRIFT | File |
|---|---|---|
| The Amazing Uncle Griswold — David Whyld | ADRIFT (v4) | **— MISSING —** (`Whatever_Happened_to_Uncle_Grumble.taf` is a *different* game) |

The other six IntroComp entries (Deadsville, The Fox/Dragon/Loaf, The Hobbit,
Negotis, Somewhen, Weishaupt Scholars) are Z-code/TADS — **not ADRIFT**, ignore.

---

## 3. Games we must source before testing (missing from disk)

Search across `~` (excluding Library/Trash/caches) found nothing for:

1. **A Masochist's Heaven** (Tne Mad Monk, ADRIFT 1st 1-Hr Comp 2002)
2. **Archie's Birthday – Ch.1: Reggie's Gift** (Purple Dragon, 2005) — adult
3. **The Amazing Uncle Griswold** (David Whyld, IntroComp 2005)
4. **Quest for the Magic Healing Plant** (Adam Crutchlow) — ADRIFT 3.9 version

Sources to try: the IF Archive (`if-archive/games/adrift/`), the ADRIFT forum
game DB, and the ShadowVault / IFWiki comp pages the archives link to. #1/#3 are
David Whyld games and usually on the IF Archive. #2 (AIF) is likely only on the
adult IF sites. #4's ADRIFT 3.9 port may be hard to find — the AGT and Inform 6
versions are the common ones, and the walkthrough itself is Inform-based, so
this one is low priority.

---

## 4. Prerequisite: fix the stale harness build — ✅ DONE

`adrift-walkthroughs/harness/build.sh` predated the `terps/scare` →
`terps/scarier` merge and would not build as-is:

* `SCARE_DIR` defaults to `/Users/administrator/spatterlight/terps/scare` — that
  directory no longer exists.
* It compiles a hand-listed set of `sc*.c` files — the sources are now `.cpp`
  (24 of them) under `terps/scarier`, plus `os_ansi.cpp`.
* It links `seed.c`; the harness now ships `seed.cpp`.
* Because the sources are C++, it must link with `clang++`, not `clang`
  (linking with `clang` fails with missing `std::` symbols).

The committed `harness/scare` binary (built 2026-07-01) worked fine — but
nobody could *reproduce* it from `build.sh`. Fixed: `build.sh` now defaults
`SCARE_DIR` to the repo's `terps/scarier` (derived from its own location),
globs `sc*.cpp`, links `seed.cpp`, and invokes `clang++`. Verified: `sh
build.sh` produces a binary behaviourally identical to the committed one.

```sh
# what build.sh now runs, from terps/scarier:
clang++ -O2 -w -I. -DSCARE_DUMP_TOOLS \
    sc*.cpp os_ansi.cpp adrift-walkthroughs/harness/seed.cpp \
    -lz -o adrift-walkthroughs/harness/scare
```

Note: `harness/scproj_regress.sh` (the projectile-combat regression) had the
**same** staleness — since fixed (commit `1fde32ba`).

**Second build fix (2026-07-13):** `build.sh` globs `sc*.cpp`, which does not
match `mapdraw.cpp` — but `scmap.cpp` (the ADRIFT 4 map port) calls `map_free()`
from it, so the harness stopped linking entirely. `mapdraw.cpp` is now on the
link line; it is plain C++ with no Glk dependency.

---

## 5. Caveat: several walkthroughs solve a *port*, not the ADRIFT game

Key & Compass solves whatever version David Welbourn played. For these the
walkthrough text is written against a **non-ADRIFT port**, so the command
sequence and (especially) the expected output will **not** match the ADRIFT
original 1:1:

* **The Thorn** — solved on the Inform Z-code port.
* **Goldilocks is a FOX!** — solved on the ralphmerridew Inform 6 port.
* **Renegade Brainwave** — solved on J. J. Guest's Inform 7 port.
* **Color of Milk Coffee** — solved on the Inform 7 version; the `.taf` is an
  **ADRIFT 5** game and is already covered by the a5 corpus (see the TL;DR).
* **Yak Shaving** V2 — Z-code port; **use the V1 section** (the ADRIFT entry).

For these, the extracted commands are a *starting point* only; each needs to be
re-played and re-derived against the ADRIFT `.taf` (verb/synonym differences,
different room names, different puzzle wiring). Treat them like the
`*_walkthrough.md` derivations already in this folder.

Also flagged:

* ~~**`coloromc.taf` fails to load**~~ — **RESOLVED (2026-07-13): not a loader
  gap.** It is an **ADRIFT 5.00** game, so SCARE correctly refuses it; the a5
  engine plays it and it is already banked in the a5 corpus (MATCH 0|0).
* **ADRIFTMAS Party** — the archive is a *map*, not a walkthrough. No commands
  to convert. Skip unless someone derives a fresh solution from the map.

---

## 6. How to add one game (recipe) — worked example: *Too Much Exercise*

### Step 1 — extract a solution file from the web-archive
```sh
# webarchive → text (macOS)
textutil -convert txt -stdout "Too Much Exercise …webarchive" > wt.txt

# pull the '>' command lines, drop the '> ' prefix and (parenthetical notes),
# split multi-command lines on '. ' → one command per line
grep '^>' wt.txt \
 | sed -E 's/^> ?//; s/\([^)]*\)//g' \
 | tr '.' '\n' \
 | sed -E 's/^[[:space:]]+//; s/[[:space:]]+$//' \
 | grep -v '^$' > harness/exercise_solution.txt
```

### Step 2 — run it through the deterministic harness
```sh
cd terps/scarier/adrift-walkthroughs/harness
sh play.sh "~/Downloads/Adrift games/ifarchive_v4_new/exercise.taf" \
   exercise_solution.txt > /tmp/ex.txt
grep -iE 'won|congratulat|score|the end|\*\*\*' /tmp/ex.txt   # win check
```
(The `scare` binary seeds RNG via `seed.cpp`, so runs are reproducible — safe
to diff.)

### Step 3 — fix the divergences (this is the real work)
Running the mechanically-extracted list above does **not** win — it desyncs at
`chop tree`. **Key finding:** Key & Compass walkthroughs assume interactive
"(Press a key)" pauses (the archive literally says *"After pushing any key
twice, the tree will be felled"*). The line-based harness feeds the next
*command* into that key-wait, eating a move and derailing the rest. Each such
pause needs a **blank line** (or an explicit wait) inserted into the solution,
and any `SPACE` / `1.)`-style conversation tokens in the archive (e.g. *Pieces
of eden*, *Man Overboard*) need translating to what the ADRIFT parser expects.

So adding a game is: extract → play → patch the solution until it wins →
capture the winning transcript. Record the derivation in
`<Game>_walkthrough.md` alongside the others.

### Step 4 — commit the solution + a golden transcript
Solution files here **are** git-tracked; **game `.taf`s are not** (they're
third-party data — the same policy as `test/adrift5-games/`, which the a5
harness `SKIP`s when the file is absent). So commit:
* `harness/<game>_solution.txt`
* a golden expected transcript for regression (see §7)
* `<Game>_walkthrough.md` notes

Keep the game file out of git; document its expected path/filename so the
regression can `SKIP` cleanly when it's absent on another machine.

---

## 7. General v4 walkthrough *regression* runner — ✅ ADDED (landing pad)

Previously the a5 side had `run_a5_walkthroughs.sh` (per-game golden diff +
FrankenDrift ground-truth, non-zero exit on regression) but the v4 side had
**only** `scproj_regress.sh` (projectile combat vs `scproj_regress.golden`) —
no full-transcript regression; the `*_solution.txt` wins were validated ad-hoc
via `play.sh` + eyeball.

Added `harness/run_v4_walkthroughs.sh`, modelled on `run_a5_walkthroughs.sh`:

* a `MAP` of `solution | game-.taf-basename | optional-win-marker`;
* for each present game, runs the seeded `scare` binary and strict-diffs the
  normalised transcript against a committed golden
  (`<solution>.expected.txt`), the same trailing-ws / `cat -s` normalisation
  the a5 golden path uses;
* states: `PASS` / `FAIL` (golden mismatch **or** declared win marker absent) /
  `NEEDGOLD` (derived, not yet blessed) / `SKIP` (game .taf absent) /
  `NOSCRIPT` (no solution file); non-zero exit on any `FAIL`;
* `--bless [substr]` records goldens — but **refuses** to bless a run whose win
  marker is absent, so a silently-desynced walkthrough (§6 step 3) can't be
  frozen in as "passing";
* auto-builds `scare` via `build.sh` if the binary is missing;
* finds games in `GAMES_DIR` (default `harness/../games`) then
  `~/adrift-battle/games`, by basename — games stay untracked (§6 step 4).

Verified end-to-end: `--bless` recorded goldens for the two already-carried
comp games (*Ice Cream*, *The Cat in the Tree* → `Congratulations!`), a plain
run then reports `PASS` for both, and a tampered golden produces `FAIL` + exit
1. The other MAP rows `NOSCRIPT`/`SKIP` until their solution + game land.

Still TODO on the runner:
* ~~wire it into `make -f Makefile.headless test`~~ — **DONE (2026-07-13).**
  `Makefile.headless` gained a `v4walkthroughs` target, called from `test` right
  after `a5runtest`.  It skips outright (without even building the harness) when
  no ADRIFT 4 corpus is present on the machine, so it is safe on a fresh
  checkout; the runner itself SKIPs any row whose `.taf` is missing and only
  exits non-zero on a real regression.
* **ground truth for v4 is the run400 disassembly, not FrankenDrift.** The claim
  once made here that "FrankenDrift plays v4 too" is unverified and probably
  wrong — FD is a port of the **ADRIFT 5** runner. (jAsea, `~/adrift-battle/
  jasea-0.2t.jar`, is likewise no help: it is a clean-room reimplementation that
  shares SCARE's exact function names and its bugs — its `objectInPlace` has the
  same static-object flaw, so it corroborates nothing.)

  The real ground truth is **`~/Desktop/run400.txt`**, the P-code disassembly of
  the actual Win32 Runner (`~/adrift-battle/runner/run400.exe`, alongside
  `gen400.exe`, whose UTF-16 UI strings spell out the Generator's dropdown
  enums in order — very handy for decoding Var1/Var2/Var3 meanings).
  Useful anchors found so far:
  - `mdlSpreadTheLoad.Sub_20_65` — restriction *aggregator*: walks a task's
    restrictions, calls `Sub_20_3` per restriction, `Replace`s `#` with `T`/`F`
    in a boolean-expression string (this is why ADRIFT 4 evaluates *all*
    restrictions rather than short-circuiting).
  - `mdlSpreadTheLoad.Sub_20_3` — the per-restriction evaluator. Type 0 (object
    location) decoding confirmed here; **its object loop filters out statics**
    (`Objects(i).[18] == 0`), which is what the Topaz fix restores.
  - `mdlSpreadTheLoad.Sub_20_7` — "is object in the player's room", showing the
    Runner's split model: dynamics carry a location int at `[1A]` (-1 = hidden,
    0 = held, N>=1 = room N-1), statics instead carry a per-room byte array at
    `[1C]` and have **no** location int at all.

---

## 7b. Open faithfulness questions raised by the run400 decode (2026-07-13)

Decoding `Sub_20_3` for the Topaz fix turned up two places where SCARE is
*better behaved* than the real Runner. Neither is fixed, because "faithful" and
"correct" point in opposite directions here and no game in the corpus depends on
either. Recorded so the next person does not have to re-derive them:

1. **The Runner does not implement negation for "any/no object" restrictions.**
   The `Var2 > 5` negation flag and the `Var2 Mod 6` selector exist only in the
   Runner's specific-object / referenced-object path. In the `Var1 = 0/1` loop the
   switch is exact-equality on `Var2 = 0..5` with no `Mod` — so in the real Runner
   **"ANY object must NOT be …" is always FALSE and "NO object must NOT be …" is
   always TRUE**. SCARE inverts `should_be` and evaluates it properly. Unknown
   whether `gen400.exe` even lets an author build that combination; if a game ever
   turns up that depends on the Runner's behaviour, this is the place to look.

2. **`Var1 = 2` (referenced object) with a *static* referenced object.** The Runner
   reads `[1A]` — the dynamic-only location field — for a static, which is never
   maintained (probably VB zero-init `0`, i.e. "held by player"). SCARE explicitly
   returns FALSE for that case (`restr_pass_task_object_location`). SCARE's answer
   is saner; the Runner's is whatever garbage `[1A]` holds. Not reproduced.

## 8. Suggested priority order

1. ~~**Fix `build.sh`** (§4)~~ — ✅ done.
2. ~~**Add `run_v4_walkthroughs.sh`** (§7)~~ — ✅ done (landing pad + goldens for
   the two already-carried comp games).
3. ~~**Low-hanging native-ADRIFT games**~~ — ✅ done. Man Overboard, Pieces of
   eden, The Princess In The Tower, Too Much Exercise, Yak Shaving (V1) all
   derived, winning, blessed, and in `run_v4_walkthroughs.sh` (7/7 PASS). See
   `<Game>_walkthrough.md` for each derivation.

   **Games dir:** `run_v4_walkthroughs.sh` looks in `adrift-walkthroughs/games/`
   (default `GAMES_DIR`) by basename. All corpus games are symlinked there from
   `~/Downloads/…`; the dir is untracked (§6 step 4 policy). Recreate on another
   machine with symlinks (or set `GAMES_DIR`); absent games just `SKIP`.
4. ~~**3-Hour and 4th 1-Hour comp corpora**~~ — ✅ done. **All 6** 3-Hour games
   and the **4 winnable** 4th-1-Hour games with walkthroughs are derived,
   winning, blessed, and in the runner (**17/17 PASS**, deterministic; exercises
   ADRIFT 3 *and* 4 paths). See §2 tables + each `*_walkthrough.md`. Notes:
   - **Topaz — ✅ DONE, WON (2026-07-13).** The "unwinnable" verdict was a real
     **SCARE bug**, not a game defect: the object-location restriction looped over
     *all* objects, but a static object's `position` field sits at -1, which is
     also the `OBJ_HIDDEN` sentinel — so every piece of scenery read as "hidden"
     and a "no object is hidden" restriction could never pass in any game with
     scenery. The run400 disassembly (`Sub_20_3`) shows the real Runner filters
     statics out of that loop. Fixed in `screstrs.cpp`; all 22 prior goldens are
     byte-identical, Topaz is the 23rd. Full write-up in `Topaz_walkthrough.md`.
   - The other 8 4th-1-Hour games are **map-only** on the page and *Spam* is a
     broken game (§2) — none actionable.
   - **Fixed a second `build.sh` bug:** it defined `-DSCARE_DUMP_TOOLS`, but the
     post-rename guard is `SCARIER_DUMP_TOOLS`, so the dump/trace tools had been
     compiled out (dead code). Now `-DSCARIER_DUMP_TOOLS`; env-gated so goldens
     are unaffected, and `SCR_DUMP_TASKS`/`SCR_TRACE_TASKS`/`SCR_DUMP_OBJLOC` now
     work for route debugging (used to diagnose Topaz).
5. **Port-based games** (§5): The Thorn, Goldilocks, Renegade Brainwave — need
   re-derivation against the ADRIFT `.taf`.
6. **Source the 4 missing games** (§3), then add them.
7. ~~**Investigate** the `coloromc.taf` load failure and the **Topaz** SCARE
   incompatibility~~ — **both DONE (2026-07-13).** coloromc is an ADRIFT 5 game
   (already in the a5 corpus, not a SCARE bug); Topaz was a genuine SCARE
   restriction bug, now fixed and banked. **Skip** ADRIFTMAS Party (map only)
   unless a solution is derived.
