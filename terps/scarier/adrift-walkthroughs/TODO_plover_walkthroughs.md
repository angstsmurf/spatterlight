# TODO — add the "Plover" walkthrough corpus to Scarier's automated testing

> **PARKED 2026-07-13 (updated).** Everything below is committed and the tree is
> green (`make -f Makefile.headless test` exits 0; v4 suite **30/30 PASS**).
> Closed so far: the `make test` wiring (§7), the `coloromc.taf` "load failure"
> (it is an ADRIFT 5 game — §TL;DR), **Topaz**, which was never unwinnable —
> it was a SCARE bug affecting every v4 game (§2, `Topaz_walkthrough.md`, and
> §7b for the run400 anchors) — the **three port-based re-derivations** (§5):
> *The Thorn*, *Renegade Brainwave* and *Goldilocks is a FOX!* (100/100 MAX) —
> and, **2026-07-13, the four missing games** (§3), which Petter sourced. A
> second `build.sh` link break was fixed on the way (§4).
>
> **Resume points, smallest first:**
> 1. ~~**Port-based re-derivations**~~ — ✅ **DONE 2026-07-13.** All three are in
>    the runner with goldens; see `Thorn_walkthrough.md`,
>    `Renegade_Brainwave_walkthrough.md`, `Goldilocks_walkthrough.md`. Three
>    lessons worth carrying: (a) always try **`walkthru`** first — *The Thorn*
>    ships the author's own solution as a task; (b) a K&C page may solve a
>    *bigger* game — Guest's Inform *Renegade Brainwave* is an **expansion**, so
>    deriving from `SCR_DUMP_TASKS=1` beat patching the port script; (c) when a
>    port *is* faithful (*Goldilocks*: same 100 points, same 29 items), every
>    failure is a wording problem, and the fastest tool is the task dump, which
>    hands you the exact `cmd=[...]` the parser wants.
> 1b. ~~**Source the 4 missing games**~~ — ✅ **DONE 2026-07-13**, see §3. All four
>    are derived, winning, blessed and wired. Two of them beat Key & Compass:
>    *Quest for the Magic Healing Plant* **140/140 MAX** (his page solves the
>    Inform original and scores 20 on the ADRIFT port) and *Archie's Birthday*
>    **50/50 MAX** (his page scores 27 and gives up on the Miss Brown puzzle).
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
* ~~**4 game files are missing from disk entirely**~~ — **all four sourced and
  done (2026-07-13); see §3.** Two of them beat the Key & Compass page they came
  with: *Quest for the Magic Healing Plant* (140/140 MAX vs his 20 — his solves the
  Inform original) and *Archie's Birthday* (50/50 MAX vs his 27 — he never solved
  the Miss Brown puzzle). Archie is AIF, so its artifacts are **gitignored**.
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
| 1 | A Masochist's Heaven | *A Masochist's Heaven* — Tne Mad Monk (1st 1-Hr Comp 2002) | 4 | `1hourgamecomp/1HRGAME.taf` | ADRIFT | ✅ **DONE — WON 15/15 MAX**, in harness |
| 2 | Color of Milk Coffee | *Color of Milk Coffee* — Bahri Gordebak (InsideADRIFT #41) | **5** | `Adrift games/InsideADRIFT_41/coloromc.taf` | Inform 7 | ✅ **DONE — it is an ADRIFT 5 game**, not v4; already in the a5 corpus (MATCH 0\|0). Not a SCARE loader gap. |
| 3 | Man Overboard!!! | *Man Overboard!!!* — TonyB (Writing Challenges Comp 2006) | 4 | `Adrift games/ifcomps_v4_new/man overboard.taf` | ADRIFT | ✅ **DONE** — wins (boat ending), in harness |
| 4 | Pieces of eden | *Pieces of eden* — Nicodemus (Comp With No Name 2008) | 4 | `Adrift games/adrift_offarchive_new/Pieces of eden.taf` | ADRIFT | ✅ **DONE** — wins (`END OF PART ONE`), in harness |
| 5 | The Princess In The Tower | *The Princess In The Tower* — David Whyld (1st 1-Hr Comp 2002) | 4 | `Adrift games/1hourgamecomp/princess1.taf` (title confirmed) | ADRIFT | ✅ **DONE** — wins (`It seems you've won.`), in harness |
| 6 | The Thorn | *The Thorn* — Eric Mayer / David Whyld (Summer MiniComp 2003) | 4 | `Adrift games/Minicomp/Thorn by Eric Mayer/Thorn.taf` | Inform (Z) | ✅ **DONE** — re-derived; reaches an ending (`look upon your own mortality`), in harness. Game ships its own `walkthru` task. |
| 7 | Too Much Exercise | *Too Much Exercise* — Robert Street (Writing Challenges 2006) | 4 | `Adrift games/ifarchive_v4_new/exercise.taf` | ADRIFT | ✅ **DONE** — wins (`Congratulations!`), in harness (worked example, §6) |
| 8 | Goldilocks is a FOX! | *Goldilocks is a FOX!* — J. J. Guest (2002) | 4 | `Adrift games/goldilocks.taf` | Inform 6 | ✅ **DONE — WON 100/100 MAX**, in harness. Port is faithful; only wording differed. |
| 9 | Renegade Brainwave | *Renegade Brainwave* — J. J. Guest (Ectocomp 2010) | 4 | `Adrift games/Ectocomp_2010/Renegade_Brainwave.taf` | Inform 7 | ✅ **DONE — WON**, in harness. The Inform release is an **expansion**; WT was unusable, derived from the task dump. |
| 10 | Yak Shaving for Kicks and Giggles! | *Yak Shaving…* — J. J. Guest (The Odd Competition 2008) | 4 | `Adrift games/yak_shaving.taf` | ADRIFT (V1) | ✅ **DONE** — wins (V1 section; `completed the Odd Competition`), in harness |
| 11 | Archie's Birthday – Ch.1: Reggie's Gift | Purple Dragon (2005) — **adult content** | 3.90 | `archie/Archie's Birthday V 1-2.taf` | ADRIFT | ✅ **DONE — WON 50/50 MAX** (beats K&C's 27; he never solved Miss Brown). Artifacts **gitignored** — §3 |
| 12 | Quest for the Magic Healing Plant | Adam G. Crutchlow (1995/2002) | 3.9 | `mhpquest.taf` | Inform 6 | ✅ **DONE — WON 140/140 MAX**, in harness. K&C solves the Inform original and scores 20 here (§3) |
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
| The Amazing Uncle Griswold — David Whyld | ADRIFT (v4) | `IntroComp05/ADRIFT/Griswold/Griswold.taf` — ✅ **DONE** (intro completed; 0/0, the game has no score and no ending). Not `Whatever_Happened_to_Uncle_Grumble.taf`, a different game. |

The other six IntroComp entries (Deadsville, The Fox/Dragon/Loaf, The Hobbit,
Negotis, Somewhen, Weishaupt Scholars) are Z-code/TADS — **not ADRIFT**, ignore.

---

## 3. The 4 games that were missing — ✅ ALL SOURCED AND DONE (2026-07-13)

Petter supplied all four. Each is derived, winning, blessed and wired into
`run_v4_walkthroughs.sh`; the `.taf`s stay untracked and are symlinked into
`games/` as usual (§6 step 4).

| Game | File | Result | Notes |
|---|---|---|---|
| **A Masochist's Heaven** (Tne Mad Monk, 1st 1-Hr Comp 2002) | `1HRGAME.taf` (v4) | ★ **WON 15/15 MAX** | K&C route worked unchanged. `Masochists_Heaven_walkthrough.md` |
| **The Amazing Uncle Griswold** (David Whyld, IntroComp 2005) | `Griswold.taf` (v4) | **Intro completed, 0/0** | No `ChangeScore` and no `EndGame` action exists — it is an intro, so there is nothing to score or win. Doorbell is a ~15-turn timer, not a lock. `Griswold_walkthrough.md` |
| **Quest for the Magic Healing Plant** (Crutchlow) | `mhpquest.taf` (v3.9) | ★ **WON 140/140 MAX** | K&C solves the **Inform** original and scores only 20 here: `enter cave` → `in`, `put coin in fountain` → **`throw`**, `enter wall` → `in`, plus a missing `e` to the Mountains. `MHP_Quest_walkthrough.md` |
| **Archie's Birthday – Ch.1** (Purple Dragon, 2005) — **AIF** | `Archie's Birthday V 1-2.taf` (v3.9) | ★ **WON 50/50 MAX** | K&C scores 27 and **gives up on the Miss Brown puzzle** ("if that's possible to do, I've yet to figure it out"). See below. |

**Archie's Birthday is deliberately not committed.** The game's text is sexually
explicit, so its solution, its golden transcript and its walkthrough notes are
gitignored (`harness/.gitignore`, `adrift-walkthroughs/.gitignore`). The MAP row
stays in `run_v4_walkthroughs.sh` so the regression runs where the files exist and
`NOSCRIPT`s everywhere else. Two mechanics worth recording here, since the notes
are not in the repo:

- **Miss Brown (14 pts).** Every Supply Room task needs NPC 6 in the player's
  room, and exactly one task moves her there: TASK 108, gated on **`variable 17
  == 5`** (not `>= 5`). Six one-shot tasks bump that variable — `x` her tits/ass/
  pussy and `ask` her about tits/ass/pussy — and task 108 is checked *before* all
  six, so each command falls through to its one-shot and bumps the counter. K&C's
  route touches only four of the six (never the ass), stalls at 4, and she never
  moves. Do five, then a sixth: 108 fires, she walks to the Supply Room.
- **The shower scene (13 pts).** A counter (`variable 13`), not a puzzle: twelve
  base commands, each in three tiers keyed to it (≤13 / 14–27 / **28–41**), and
  only tier 3 scores. The win (TASK 367) needs the counter at **42**. Drive it:
  28 fillers → ten distinct scorers → filler to 42. `lick ass` is the one base
  command whose tier 3 does not score, so it is the safe filler. With 27 fillers
  the first scorer lands in tier 2 and silently scores nothing (49/50).

**Dumper bug found while deriving MHP Quest:** `scdump.cpp` listed the exit array's
diagonals as `NE,NW,SE,SW`. ADRIFT's real order is **`NE,SE,SW,NW`** (`sclibrar.cpp`
DIRNAMES; `mapdraw.cpp` `map_dirs`), so **every diagonal exit in every dump was
mislabelled** — cardinals were fine, which is why nobody noticed. Fixed.

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

* ~~**The Thorn**~~ — solved on the Inform Z-code port. **✅ re-derived 2026-07-13** (`Thorn_walkthrough.md`).
* ~~**Goldilocks is a FOX!**~~ — solved on the ralphmerridew Inform 6 port. **✅ re-derived 2026-07-13, 100/100 MAX** (`Goldilocks_walkthrough.md`).
* ~~**Renegade Brainwave**~~ — solved on J. J. Guest's Inform 7 port, which is an **expansion of the ADRIFT game**, not a port of it. **✅ re-derived 2026-07-13 from the task dump** (`Renegade_Brainwave_walkthrough.md`).
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
5. ~~**Port-based games** (§5): The Thorn, Goldilocks, Renegade Brainwave~~ — ✅
   **done 2026-07-13.** All three derived, winning, blessed and in the runner
   (**26/26 PASS**). Goldilocks is a full **100/100 MAX**.
6. ~~**Source the 4 missing games** (§3), then add them~~ — ✅ **done 2026-07-13**;
   all four derived, winning, blessed and wired (**30/30 PASS**).
7. ~~**Investigate** the `coloromc.taf` load failure and the **Topaz** SCARE
   incompatibility~~ — **both DONE (2026-07-13).** coloromc is an ADRIFT 5 game
   (already in the a5 corpus, not a SCARE bug); Topaz was a genuine SCARE
   restriction bug, now fixed and banked. **Skip** ADRIFTMAS Party (map only)
   unless a solution is derived.
