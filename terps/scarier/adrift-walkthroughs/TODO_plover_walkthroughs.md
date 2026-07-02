# TODO — add the "Plover" walkthrough corpus to Scarier's automated testing

Pairing + integration plan for the 18 walkthrough web-archives in
`~/Desktop/Plover adrift walkthroughs/` (David Welbourn's *Key & Compass* /
Plover.net solutions, plus a few comp round-up pages).

Investigated 2026-07-02. Everything below was verified by running the games
through the committed headless SCARE harness
(`adrift-walkthroughs/harness/scare`) — titles, versions and load-ability are
observed, not guessed.

---

## TL;DR

* **All of these games are ADRIFT 3.9 / 4.0** — *none* are ADRIFT 5. Verified
  by header byte and by loading them. They therefore belong to the **SCARE
  (v4) walkthrough harness** in `adrift-walkthroughs/harness/`, **not** the
  ADRIFT-5 harness in `test/` (`a5run_dump` / `run_a5_walkthroughs.sh`).
  `a5model_load()` only understands zlib-compressed ADRIFT-5 XML and returns
  NULL on every one of these `.taf`s (confirmed).
* The 18 archives cover **~35 distinct games** (two of them are comp round-up
  pages listing many games each). Of those, **~29 are ADRIFT games with a
  usable command walkthrough**; the rest are non-ADRIFT ports, a map-only page,
  or already-covered.
* **2 are already in the harness**: *Ice Cream* and *The Cat in the Tree*
  (`icecream_solution.txt`, `the_cat_in_the_tree_solution.txt`).
* **4 game files are missing from disk entirely** and must be sourced before
  anything can be tested (see §3).
* **Prerequisite:** `harness/build.sh` is stale and will not rebuild the
  harness (see §4) — fix it first.

---

## 1. Pairing table — individual walkthroughs

Game-file paths are under `~/Downloads/`. "WT engine" = the platform the
*walkthrough* was written against (Key & Compass often solves the Inform/Z-code
*port*, not the ADRIFT original — see §5).

| # | Web-archive | Game / author | ADRIFT ver | Game file on disk | WT engine | Status |
|---|-------------|---------------|-----------|-------------------|-----------|--------|
| 1 | A Masochist's Heaven | *A Masochist's Heaven* — Tne Mad Monk (1st 1-Hr Comp 2002) | 4 | **— MISSING —** | ADRIFT | need game (§3) |
| 2 | Color of Milk Coffee | *Color of Milk Coffee* — Bahri Gordebak (InsideADRIFT #41) | 4 | `Adrift games/InsideADRIFT_41/coloromc.taf` | Inform 7 | ⚠ taf **won't load** ("Not a loadable Adrift game"); game is trivial (`x me`, `z`×3) |
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
Of the 7 with walkthroughs, 2 were already carried, **4 are now DONE**, and
**Topaz is DEFERRED** (unwinnable under SCARE — see §5).

| Game | File | Status |
|---|---|---|
| ADRIFT Maze | `ADRIFTMaze.taf` | ✅ DONE — `You WIN!` (name prompt eats 1st move; seeded troll teleport) |
| Cruel and Hilarious Punishment (v3.9) | `CAH.taf` | ✅ DONE — `destroyed our reality` |
| Get Treasure For Trabula | `Trabula.taf` | ✅ DONE — `given the gold coins to Trabula` (score 125; `attack` not `kill`) — see `Trabula_walkthrough.md` |
| Shred 'Em | `shreddem.taf` | ✅ DONE — `Due to lack of evidence` |
| The Cat in the Tree | `TheCatintheTree.taf` | ✅ already in harness |
| Ice Cream | `IceCream.taf` | ✅ already in harness |
| Topaz | `topaz.taf` | ⚠ **DEFERRED — unwinnable under SCARE** (see `Topaz_walkthrough.md` / §5) |
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

Note: `harness/scproj_regress.sh` (the projectile-combat regression) has the
**same** staleness (`SCARE_DIR` → `terps/scare`, globs `sc*.c` + `sxstubs.c`,
uses `clang`/`seed.c`). Not fixed here — flagged for a follow-up.

---

## 5. Caveat: several walkthroughs solve a *port*, not the ADRIFT game

Key & Compass solves whatever version David Welbourn played. For these the
walkthrough text is written against a **non-ADRIFT port**, so the command
sequence and (especially) the expected output will **not** match the ADRIFT
original 1:1:

* **The Thorn** — solved on the Inform Z-code port.
* **Goldilocks is a FOX!** — solved on the ralphmerridew Inform 6 port.
* **Renegade Brainwave** — solved on J. J. Guest's Inform 7 port.
* **Color of Milk Coffee** — solved on the Inform 7 version (and the ADRIFT
  `.taf` won't even load — see below).
* **Yak Shaving** V2 — Z-code port; **use the V1 section** (the ADRIFT entry).

For these, the extracted commands are a *starting point* only; each needs to be
re-played and re-derived against the ADRIFT `.taf` (verb/synonym differences,
different room names, different puzzle wiring). Treat them like the
`*_walkthrough.md` derivations already in this folder.

Also flagged:

* **`coloromc.taf` fails to load** in the current SCARE engine ("Not a loadable
  Adrift game"). Either the file is damaged/an odd sub-format, or it's a loader
  gap. Investigate separately before relying on it; the game is trivial anyway.
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
* wire it into `make -f Makefile.headless test` (alongside the battle tests);
* **optional ground truth:** FrankenDrift plays v4 too, so `a5_groundtruth.sh`'s
  approach (diff Scarier vs FD) could be reused — but mind the RNG caveat (FD =
  System.Random, SCARE = seeded RNG in `seed.cpp`; RAND text won't align) and
  that FD is an uncommitted external checkout.

---

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
   - **Topaz DEFERRED** — unwinnable under SCARE (its win task's "no object
     hidden" restriction can never pass because a hidden duplicate ring is never
     un-hidden). Left out of the MAP; best-effort solution kept. Full diagnosis
     in `Topaz_walkthrough.md`. **Open engine investigation.**
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
7. **Investigate** the `coloromc.taf` load failure and the **Topaz** SCARE
   incompatibility (item 4); **skip** ADRIFTMAS Party (map only) unless a
   solution is derived.
