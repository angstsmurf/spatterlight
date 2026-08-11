# ADRIFT 4 walkthroughs — derivation COMPLETE

Original goal: produce a verified, reproducible, near-maximum-score walkthrough
for every `.taf` in `test/adrift4/games/`, in the style of
`Sun_Empire_walkthrough.md` — full command list, annotated phases, and an honest
note on any unreachable points and *why*. These are obscure 2000–2005 ADRIFT
comp games with no published walkthroughs (checked Key & Compass, IF Archive,
CASA); they were derived by driving each game through a headless, deterministic
Scarier build and reading its internals.

**Status 2026-08-04: done**, re-verified 2026-08-10. The suite was **203 rows,
203 PASS** — 0 FAIL, 0 SKIP, 0 NEEDGOLD, 0 NOSCRIPT, exit 0. Every game has a
route or a documented verdict, and every walkthrough in `downloaded/` has a game
and a row. This file is now the method, the cautions, and an index.

**Second wave 2026-08-10 → 2026-08-11: 22 more games wired, suite 225 rows,
225 PASS — then PARKED.** See *Second wave* below for what was done and what is
left.

**Third wave 2026-08-11: 6 more, suite now 231 rows, 231 PASS — PARKED again.**
Unparked to work the remaining v3.90 files smallest-first, and parked at the
user's request after *Where Is Richard?*. See *Third wave* below.

Nothing here is open work. The one exception this file used to carry — the
Runner's **"You can't do that here!"** refusal for a task typed outside its
`Where` rooms — was probed and **implemented 2026-08-10**, along with its
sibling **"You have already done that."**; see the follow-up under
*The Hangover*. Everything else that once read as open below has since
closed — where a later finding overturned a dated entry it carries a superseding
note in place; the entry itself is left as written.

```
test/adrift4/harness/run_v4_walkthroughs.sh          # the whole suite
test/adrift4/harness/run_v4_walkthroughs.sh <regex>  # one row
test/adrift4/harness/run_v4_walkthroughs.sh --bless  # re-record goldens
```

## Second wave (2026-08-10 → 2026-08-11) — 22 games, then PARKED

The first wave covered every `.taf` that was in `games/` when it ran. The
manifest has grown since (issue #119's corpus fetch), so a second pass worked
**smallest unwired file first** through the ADRIFT 3.8/3.9 leftovers. Twenty-two
went in; the suite went 203 → **225 rows, 225 PASS**. Same four artefacts per
game as before: commented `goldens/<name>_solution.txt`, blessed
`.expected.txt`, a commented row in `harness/run_v4_walkthroughs.sh`, and
`notes/<Game>_walkthrough.md`.

| Bytes | Game | Result |
|---|---|---|
| 5,164 | I | ending reached; no score in the file |
| 6,508 | Dreamland | **WON 50/50** |
| 8,292 | Forest On The Norm | completed; no scoring system |
| 10,310 | The Adventures of Bob Bobsly | **WON 155/155** |
| 12,028 | Druggy Lane | **WON** — debt cleared at $1,955,720,463 |
| 12,660 | Escape from Insanity | **WON 1000/1000** |
| 16,695 | Lost Souls | **WON**; no scoring system |
| 19,755 | Chicago | **WON 75** |
| 20,065 | Everything Emanuelle | fullest of four endings; no score |
| 20,652 | Textident Evil | **WON 100/100** |
| 22,214 | Impulso | **WON**; no score |
| 23,225 | Montahue Scott and the Mobius Belt | **WON 3/3** |
| 26,973 | A Morning With A Headache | **WON 115/115** |
| 29,811 | Sleaze City | **WON 100/100** |
| 31,353 | Albridge Manor | **WON 50/50** |
| 37,088 | The Lost Mines | **WON 100/100** |
| 39,485 | The Dark Tower | **WON** (T8 `turn on power`) |
| 41,801 | Report Espionage | **WON 100/100**, all 23 tasks on the route |
| 42,118 | Far From Home | **WON 50/50** |
| 42,463 | S Tar Dus T | **WON**, richest of four endings; no score anywhere |
| 43,334 | Diary of a Stripper | **WON 13/13**, best of fourteen ALR endings — **AIF, solution/golden/notes gitignored** |
| 71,345 | Silk Noil | **WON**; no score. Added after the park, from the author's own walkthrough (see below) |

**AIF in the corpus.** *Diary of a Stripper* is sexually explicit, so its
solution, golden and notes are gitignored and only its `run_v4_walkthroughs.sh`
row is committed — the same call made for *Archie's Birthday* on 2026-07-13 (see
`.gitignore`). Its row is still worth having: the ALR-table score, the two
chained events and the rejected-command-does-not-tick rule are all pinned by it.
Everything else wired in this wave is clean, and so is the rest of the corpus: a
scan of every committed golden for explicit vocabulary turns up nothing but
profanity (*Mortality*, *The Plague*, *Second Chance* and *Light Up the
Darkness* swear, and *Second Chance* adds two anatomical words — none of it
pornographic).

Three findings from this wave are worth carrying forward, and each is written
up in full in its own note:

- **A `<waitkey>` can sit in front of the name prompt.** *Far From Home* pauses
  inside the introduction, *before* `Please enter your name:`, so a scripted run
  must spend a line on the keypress and give the name on line two — otherwise
  the player is called `look` for the rest of the game. Always run
  `SCR_MARK_WAITKEY=1` before writing the first line of a solution file.
- **`ACT type=0` "into object" indexes the container list directly** — no `-1`,
  unlike the room case. *S Tar Dus T* drops its missing page in the lake, not
  in the outhouse hole, and reading the destination as 1-based sends you to the
  wrong room for the game's last puzzle. (`sctasks.cpp task_move_object`,
  `case 2` / `case 3`.)
- **A command the parser rejects does not advance the turn counter**, so
  gibberish padding will never make a timed event fire. Found while pacing
  *Diary of a Stripper*'s two chained events; `z` works, `wait1` does not.

## Third wave (2026-08-11) — 6 games, then PARKED again

Unparked to continue smallest-unwired-3.90-first. Six went in and the suite
went 225 → **231 rows, 231 PASS**.

| Bytes | Game | Result |
|---|---|---|
| 44,503 | Asylum | ending reached; no score in the file |
| 44,666 | The Wheels Must Turn | **WON** |
| 45,737 | Life | ✗ **UNFINISHABLE** — no `ACT type=6` and no `ACT type=4` at all; demonstration row |
| 48,764 | Renuntio | **WON** — first Spanish game in the v4 corpus |
| 51,820 | House Of Horror | **WON 145/155**, and 145 is a *proven ceiling*: T109 scores the doubloons on `v3=0`, which tests OBJ_HIDDEN rather than a room, so the +10 is reachable only by not shooting the zombie (worth +20) |
| 55,039 | Where Is Richard? | **WON 1000/1000** in 68 commands |

Two things came out of this wave beyond the rows:

- **`SCR_DUMP_TASKS` was dying on `Richard.taf`.** The LOCKKEY and OPENABLE
  loops in `scdump.cpp` called the *fatal* `prop_get_integer()` on
  `Objects[i].Openable`, and that game has an object without one, so the dump
  aborted right after the OBJNAME block — before ROOM, TASK, EVENT or NPC were
  printed. Fixed with the tolerant `prop_get()`. The engine itself was never
  affected; this was dev-only instrumentation.
- **A second corpus witness for one-level container nesting in "held by the
  player".** *Where Is Richard?* kills its spider with a cupcake that never
  leaves the backpack the player is carrying — see `probe p39held` and the
  comment above `restr_object_in_place` in `screstrs.cpp`.

**Parked again 2026-08-11 at the user's request.** Nothing is broken and
nothing is half-finished — the suite is green at 231/231 and every wired game
has all four artefacts. What remains is **23 unwired `.taf` files, 17 of them
v3.90 and 6 v4.00**; the table below is the original 29, with the six done in
the third wave struck through.

**Two cautions about that list.** *Byte size does not compare across versions*:
a 4.00 `.taf` is zlib-compressed and a 3.90 one is only XOR-obfuscated, so the
13,868-byte 4.00 file and the 44,145-byte 3.90 file in the Azra row below are
the *same game*. And *the smallest-first ordering is by file size, not by game
size* — read the Ver column before picking. Version comes from the 14-byte
header signature (`sctaffil.cpp`, `V400_SIGNATURE`/`V390_SIGNATURE`/…), which
is also what tells 3.80 and 3.70 apart from 3.90; every 3.80 and 3.70 file in
the manifest is already wired.

| Bytes | Ver | File | Title | Note |
|---|---|---|---|---|
| 5,591 | 4.00 | `salutations.taf` | Salutations | English; **missed by the smallest-first sweep — start here**, since the 3.90 files are now wired down to 55 KB |
| 19,083 | 4.00 | `iachini.taf` | A Day at the Iachini House | English, Butcher Basic ALR; also missed |
| 21,775 | 4.00 | `relojero.taf` | La hija del relojero | **Spanish** |
| 44,145 | 3.90 | `The Town Of Azra.taf` | The Town of Azra | **the adrift.co release of an already-wired game** — `The_Town_Of_Azra.taf` (4.00, 13,868 bytes, IF Archive) is row `the_town_of_azra_solution.txt`, and the existing route replays on this build too, so this is a second file, not a second game |
| 44,503 | 3.90 | ~~`as.taf`~~ | ~~Asylum~~ | **WIRED, third wave** |
| 44,666 | 3.90 | ~~`Wheel105.taf`~~ | ~~The Wheels Must Turn~~ | **WIRED, third wave** |
| 45,737 | 3.90 | ~~`life.taf`~~ | ~~Life~~ | **WIRED, third wave** |
| 48,764 | 3.90 | ~~`Renuntio.taf`~~ | ~~Renuntio~~ | **WIRED, third wave** |
| 51,820 | 3.90 | ~~`hhorror.taf`~~ | ~~House Of Horror~~ | **WIRED, third wave** |
| 52,248 | 4.00 | `vetknow.taf` | Veteran Knowledge | English; also missed by the sweep |
| 52,290 | 4.00 | `vetknow2.taf` | Veteran Knowledge [Version 2] | second release of the above |
| 55,039 | 3.90 | ~~`Richard.taf`~~ | ~~Where Is Richard?~~ | **WIRED, third wave** |
| 56,336 | 3.90 | `losttombv2.taf` | The Lost Tomb | |
| 59,124 | 3.90 | `Journ2.taf` | The Long Journey Home | |
| 59,896 | 3.90 | `mudergreatfalls.taf` | Murder In Great Falls | |
| 63,183 | 3.90 | `Vampire.taf` | The Vampire With A Conscience | |
| 69,489 | 3.90 | `Merry_Murders.taf` | Merry Murders | |
| 71,216 | 3.90 | `thewoods.taf` | The Woods Are Dark | |
| 74,568 | 3.90 | `Captive.taf` | Captive Universe | |
| 101,668 | 3.90 | `enc1.taf` | Encounter 1: Tim's Mom | **AIF** |
| 107,200 | 3.90 | `wonderwombat.taf` | Adventures of Thumper – Wonder Wombat | |
| 114,698 | 3.90 | `windy.taf` | Camp Windy Lake | **AIF** |
| 120,335 | 3.90 | `enc2.taf` | Encounter 2: The Study Group | **AIF** |
| 125,581 | 3.90 | `Buffy Before the Date.taf` | Buffy: Before the Date | **AIF** |
| 148,447 | 3.90 | `croft.taf` | Lara Croft: The Sun Obelisk | **AIF** |
| 166,913 | 3.90 | `dr-who-vortex-lust.taf` | Doctor Who and the Vortex of Lust | **AIF** |
| 191,548 | 3.90 | `windy2.taf` | Camp Windy Lake: Part 2 | **AIF** |
| 277,834 | 3.90 | `gamma.taf` | The Gamma Gals | **AIF** |
| 2,928,980 | 4.00 | `Vardock Bates.taf` | Vardock Bates | **Spanish**, and by far the largest file in the corpus |

**Author material already on this machine, for whoever picks this up next.**
Several of the remaining games shipped documentation in their IF Archive /
adrift.co packages, unpacked under `~/Downloads`: `windy2walk.txt` +
`cw2faq.txt` (Camp Windy Lake 2), `croftwlk.txt` + `lcsofaq.txt` (Lara Croft),
`score.txt` (Camp Windy Lake), `enc1.txt` / `enc2.txt` (the Encounters), and
`readme.txt` (The Long Journey Home). *Where Is Richard?* ships `Map.jpg` — a
hand-drawn sketch of the swamp, readable and accurate — but no walkthrough,
and that was the pattern: a map or a FAQ is common, a solution is not.

**Eight of the twenty-three that remain are AIF** and will need the *Diary of a Stripper*
treatment when they are wired — gitignore the solution, the golden and the
notes, commit the row alone. Measured, not guessed: de-obfuscating each `.taf`
and counting explicit vocabulary gives `gamma.taf` 1416, `dr-who-vortex-lust`
705, `windy2` 670, `windy` 630, `croft` 582, `enc1` 550, `enc2` 369 and
`Buffy Before the Date` 339, against 0 for every other unwired file (bar three
incidental hits in `wonderwombat.taf`). They are marked **AIF** in the table
above.

**Check the game's own zip before deriving anything.** *Silk Noil* was wired
after the park because its author shipped a ten-command walkthrough inside
`sn_zip.zip`; it is now `downloaded/SilkNoil_walkthrough.txt`. The
`downloaded/` harvest only covered walkthroughs IFDB links to, so a bundled
`*walk*.txt`, `*.sol` or `readme` in the game's own archive is a free oracle
that the harvest will have missed. `~/Downloads/More Adrift games/` holds the
unpacked archives.

Regenerate the unwired list at any time with a set difference between the second
field of every non-comment row in `harness/run_v4_walkthroughs.sh` and the
first column of `games.manifest.tsv`; re-run the vocabulary scan with
`harness/taf_pattern_scan.py`'s `plaintext()`, which de-obfuscates both TAF
generations.

## Where everything is

| What | Where |
|---|---|
| The manifest — one line per row, `solution\|game\|win-marker\|env` | the table at the top of `harness/run_v4_walkthroughs.sh` |
| Routes and their recorded transcripts | `goldens/<name>_solution.txt` + `<name>_solution.expected.txt` |
| **Per-game analysis, route prose and score accounting** | `notes/<Game>_walkthrough.md` — 177 of them |
| Engine fidelity questions raised along the way | `../../../RUNNER_TESTS_TODO.md` |
| The full session-by-session derivation log (2026-06-24 → 2026-08-04) | git history of this file; it was pruned in the commit that added this line, so `git log --follow -p -- test/adrift4/notes/WALKTHROUGH_TODO.md` has all 4134 lines of it |

## Standing cautions

**Per-game verdicts were revised more than once — trust the per-game doc, not a
remembered summary.** Four "unwinnable" calls were overturned by later work, and
each reversal is written into its own note: WesGHN (30/100 → **WON 100/100**,
the gold ring was never orphaned), Mr Smith and Villains & Kings (both won once
the 3.9 battle path was version-gated), and The Plague – Redux (won via the pole
bypass, after `Where`/Type 0 was settled against run400). If a game's ceiling
matters, read `notes/<Game>_walkthrough.md` — it carries the current verdict and
the evidence for it.

**A published *session transcript* is Runner ground truth; a hand-written
command list is not.** Two transcripts caught real engine bugs. But a transcript
produced by stock SCARE (ClubFloyd's Floyd, recognisable by "Welcome to the
Cheap Glk Implementation" in the log) is not an oracle — it is our own engine,
one version back.

**Before declaring an object orphaned, decode every event's `o2`/`o3` with the
raw−1 rule** — especially when an event is named after the object. That is what
made WesGHN look unwinnable for two months.

**Author "to-do"/objective lists, hint menus and flavour text can all lie.** A
task can print "you can't…" and still score, if its restriction is an OR with a
passing branch. Trust the trace and `score`, not the prose.

## Per-game workflow

1. **Boot & look.** Run with an empty solution; read the intro, first room,
   objects, NPCs. Keep a running `solution.txt` (one command per line) of the
   *confirmed* path; replay it every iteration (`harness/play.sh`), appending
   probes.

2. **Dump the structure up front** (this is what makes it fast — don't brute the
   parser). With `SCR_DEBUGGER_ENABLED=1`, type `debug` then:
   - `tasks 0 N` — every task's **exact command pattern** (the verbs the author
     expects, e.g. `perform dna analysis`, `get sample from limb`). Find N from
     the "valid values are 0 to N" error.
   - `rooms 0 N` / `objects 0 N` / `npcs 0 N` — names, **locations**, container
     contents, hidden/locked flags. This locates keys, weapons, the win item,
     without searching blindly.
   - `events 0 N` — timed/triggered plot (alarms, attacks).

   Kill the process fast (it EOF-loops after the dump): wrap with
   `perl -e 'alarm 8; exec @ARGV' env SCR_DEBUGGER_ENABLED=1 ./scare GAME`, and
   read the output with `grep -a` (the stream contains NUL bytes).

3. **Get the exact scoring map.** `SCR_DUMP_TASKS=1` prints every task's actions,
   including `ACT type=4` (ChangeScore) with its points, and `expr=[...]` for
   type-3 variable actions. The points should sum to the game's stated maximum;
   that tells you exactly which tasks to complete. Where the score is an author
   *variable* rather than the engine score (The PK Girl, Three Monkeys), count
   the type-3 actions on that variable instead.

4. **Play to a win**, banking confirmed steps into `solution.txt`. Use the
   `tasks` patterns for verbs and the `objects`/`npcs` dumps for "where is X".
   Wandering NPCs: find the deterministic turn they are present (trace with
   several `look`s) rather than guessing.

5. **Push toward max score.** Compare the scoring map to what you have; for each
   missing point, find the task and satisfy it.

6. **Diagnose anything that won't score** before calling it unreachable.
   `SCR_TRACE_TASKS=1` (or `SCR_TRACE_FLAGS=256`, `+8` for the parser) prints,
   per task, the restriction bracket expression (`#A#A(#O#)` = R0 AND R1 AND
   (R2 OR R3)) and each restriction PASS/FAIL with its operands — so you learn
   *why* a task fails. `SCR_TRACE_MATCH=1` shows which task claimed a command.

7. **Attribute unreachable points honestly.** Decide whether it is the **game
   data** or **Scarier**. The restriction operators live in the `.taf`, and both
   Scarier (`screstrs.cpp`) and the real Runner (`evaluaterestrictions` =
   `Sub_20_57`, in `~/Desktop/run400.txt`) evaluate the same expression. If we
   match the Runner, the bug is the author's and exists in the Runner too — say
   so. Only call it a divergence when our evaluation actually differs, and
   prefer settling it by *playing* run400/run390 under Wine over reading P-code.
   For "is it really unwinnable?", dump the action histogram and check it
   against a known-winnable file **of the same TAF version** as a positive
   control — 3.9 `circus.taf` has 24 type-6 EndGame actions, inverness has none.

8. **Verify & write up.** Re-run the final `solution.txt` three times (identical
   score and win marker — determinism guarantees this). Write
   `notes/<Game>_walkthrough.md`: header (author/comp/result), full command
   list, phase-by-phase prose, and a closing note on unreachable points with the
   evidence. Save the route as `goldens/<game>_solution.txt` and add the row to
   the manifest.

## Footguns / lessons learned

- **Rebuild before trusting anything.** A stale `harness/scare` cost real time
  on a seed sweep. `./build.sh` first. (The built binaries in `harness/` are
  untracked; only the sources and scripts beside them are committed.)
- **Always `git checkout` temporary instrumentation** (`scbattle.cpp`,
  `sctasks.cpp`, …) — leave the tree clean.
- **`Globals.WaitTurns` is per game**, and in Cursed it is **3**: one `z` runs
  three turns of events, so any cut-scene stepped beat-by-beat desyncs if you
  count `z`s as turns. Measure it before counting:
  ```
  printf 'z\nquit\ny\n' | SCR_TRACE_EVENTS=1 ./scare GAME 2>&1 >/dev/null \
    | grep -ac '^Event: ticking event 0:'
  ```
- **Check `OBJNAME … prefix=[…]` before believing "I see no such thing"** — the
  author's prefix is often part of the only accepted phrasing.
- **Determinism = combat reproducibility.** The Battle System is RNG-driven and
  the seed shim is what makes scores stable. "Doesn't seem to do any damage" is
  the faithful `damage = strength − defence ≤ 0` branch, not a bug.
- **The debugger EOF-loops** after a dump and floods MB of output — cap it with
  `perl -e 'alarm S; exec @ARGV'` (there is no `timeout(1)` on this Mac) and
  read with `grep -a`.
- **`tasks` with no range lists only *currently-runnable* tasks**; use a range.
- **zsh: `r=$(… | grep -c pat)` can yield an empty string**, so `[ "$r" != "0" ]`
  is true every iteration — a seed sweep once reported 40 consecutive false
  wins. Write the run to a file and use `grep -q`.
- **Goldens are extended-ASCII with NEL terminators**: `export LC_ALL=C` and
  `grep -a`, always.
- **Fetching games:** `Range: bytes=0-13` classifies a remote `.taf` cheaply,
  but adrift.co sometimes answers a ranged GET with an empty body — retry a
  short reply unranged or the file is silently misfiled. Percent-encode
  filenames with spaces or curl fails outright. The corpus is pinned by sha256
  in `test/adrift4/games.manifest.tsv`, fetched by `test/fetch_games.sh`, and
  the whole arrangement is explained in `test/GAMES.md`.
- Use the scratchpad for throwaway files; keep only the solution, the
  walkthrough and the harness here.


## Appendix — derivation entries kept because nothing else records them

The session-by-session log this file used to carry was pruned once the corpus
was complete; the whole of it is in git (`git log --follow -p -- this file`).
These fourteen entries are kept **verbatim** because they are the only write-up
for one or more games — those games never got a `notes/<Game>_walkthrough.md`,
so pruning them would have lost the route reasoning outright. Newest first, as
in the original log.

The games that depend on these entries: **The Hangover** (unwinnable, max 5/7),
**Troll!**, **Locked door with water trap**, **A Spot Of Bother**, **The
Amulet**, **Monsters (r2)**, **Shadrick's Travels**, **Beanstalk**, **Dancing
Even Him**, **Doomed Xycanthus**, **Black Sheep's Gold**, **akron**,
**twilight**, **Duck McCloud**, **Fistandantalus**, **James Bond 2000**,
**Microwave Man**, **Life of Mike**, **Super Liam**, **Where Are My Keys** and
the two French games (**Qui a tué Dana?**, **Enquête à hauts risques**).

Rows banked from the external adrift-battle corpus in 2026-06 — ptbad, vague,
Escape To New York, unauthorized termination, To Hell In A Hamper, marika,
Vendetta, Unraveling God, mishmash — were never written up here either; for
those, the manifest row (win marker + env) and the golden transcript are the
record.

---
## 2026-08-04 (latest) — the five missing tafs, wired — **203 PASS** — and an engine bug in every pre-4.0 game

The last five `downloaded/` walkthroughs whose `.taf` was not in `games/`. The
files arrived as `/Users/administrator/Downloads/missing` and are now
`games/imagi.taf`, `games/CD.taf`, `games/Chosen.taf`, `games/TheCellar.taf`
and `games/panic.taf`. All five are wired, blessed and green, and
**`downloaded/` is now fully wired — every walkthrough in it has a game and a
row.** Per-game docs: `ImagiDroids_walkthrough.md`,
`CrimsonDetritus_walkthrough.md`, `Chosen_walkthrough.md`,
`TheCellar_walkthrough.md`, `Panic_walkthrough.md`.

* **ImagiDroids** (Woodfish, 4.00) — the single ending, 20 commands, one
  authorial repair: `open it` after `x clean area` / `take brick` resolves the
  pronoun to the clean area, so it has to be `open brick`. No score system.
* **Crimson Detritus** (Mystery, 2003, 4.00) — ★ **WON 100/100**, all eight
  `ACT type=4`, 16 commands. One repair: the upstream transcript pasted two
  commands onto one line (`take uniform and wear it`), split into
  `take uniform` / `wear uniform`.
* **Chosen** (Ryan J. Bury, ADRIFT MiniComp 2001, 3.90) — ★ **WON 300/300**,
  the game's own stated maximum, 51 commands. The upstream file is **prose
  hints, not a command list**, so the route is derived from the dump. Three
  traps: `pull lever` in room 8 must come before room 7 or unguarded TASK 9
  kills you; every phrasing of `take block` in room 14 is TASK 14, a death; and
  the six sockets have to be spelled **A-D-R-I-F-T** in order. Two repairs the
  route needs: `take belt` first (it starts inside the trousers and TASK 0
  wants it *held*), and full block names only — `take block` is "Take what?".
* **The Cellar** (David Whyld, 2007, 4.00) — the ending, 132 commands replayed
  **verbatim** from the ClubFloyd log of 12 June 2022, nothing repaired. No
  score and no `ACT type=6`; it ends by setting `VAR 12 [game over]`. The game
  also ships **its own 24-command walkthrough** on TASK 1 (`walkthrough`), which
  replays verbatim too — so the path is confirmed from two independent sources.
  The ClubFloyd route is the one wired because it reaches far more of the 141
  tasks. Its two divergences from the log are both **ours to be proud of**:
  ClubFloyd's Floyd plays through stock SCARE ("Welcome to the Cheap Glk
  Implementation" is in the log), so the log is not an oracle, and both places
  we differ are places scarier has since been made more faithful — the
  in-container description and the run400 postfixed one-object format recorded
  in `lib_list_in_object()`, and a real U+2026 that cheapglk folded to `...`.
  Word-level agreement 98.3%.
* **Panic!** (Stewart J. McAbney, 3.90) — the ending, "Your rating is Messiah.",
  69 commands from the author's own full-session walkthrough, replayed
  verbatim. It found a real bug — below.

**The engine bug: the 3.9/3.8 immediate-restart fixup was eating StartText.**
`evt_fixup_v390_v380_immediate_restart()` (`scevents.cpp`) restarted a pre-4.0
`RestartType=1` event by hand — state to `ES_RUNNING`, clock to one less than a
fresh roll — instead of calling `evt_start_event()`. The event *cycled*
correctly, and its TaskAffected kept firing (which is the half §2 of
`RUNNER_TESTS_TODO.md` probed live, and it was never wrong), but its start
actions, StartText included, ran only on the very first start. So an
always-restarting one-turn event printed its line once per game and was mute
after that.

Panic! is built out of these, and two of them carry a StartText and **no**
LookText (`texts=S--`), so nothing else could be printing them. Against the
author's transcript: the priest's cough 66 published / 1 before / **66** after,
the stigmata 9 / 1 / **9**, the shaking 13 / 1 / **13**, the wraith 21 / 1 / 24.

Fixed 2026-08-04: call `evt_start_event()`, then take the one silent turn off
the clock. The length roll must come from `evt_start_event()` **alone** — the
first attempt kept the fixup's own `scr_randomint()` as well, and the extra
roll per restart churned the RNG stream enough to break three previously
winning routes (circus, thetest_win, wrecked). Two rolls where the Runner has
one is its own bug.

**Corpus effect:** 13 of 203 rows gained text, every diff a *pure addition* of
a line that had been silently swallowed. Mostly ambience — troll's "Sid sips
from his mug.", haunt's grandfather clock, wrecked's seagulls, spirits_flight's
wind, tq3's rattlesnakes, colony's lightning, twilight's apparition,
timmy_reid's Billy on his bike, cybercow_win's robot — but four are **plot**:
secret_of_lost_world's "It starts to rain." and "The volcano is erupting.",
marooned's rescue ship coming over the horizon, and the entire paragraph in
alices_restaurant where Obie takes your wallet and your belt at the station.
No route broke, no win marker moved, and `thetest`, `gateway` and `inverness` —
the games probed live in run390 for §2 — are byte-identical. Written up as
`RUNNER_TESTS_TODO.md` **§8**, which left two open probes: the wraith's
visibility while you are up the rope (Panic!'s one residual divergence, turns
44–46), and whether a restarted period with `Time1=5` is 5 turns or 4.
**Both were closed the same day** — the period keeps its full authored length
and the restart is silent, and `make_39_evseeprobe.py` refuted the visibility
theory outright (run390 prints event text while the player is sitting on a
surface or inside a container, exactly as we do; the 24-vs-21 count that raised
it was an RNG divergence, the player is never up the statue on those turns).
See §8 for both.

## 2026-08-04 — the three non-plain-text documents, wired — 169/169 PASS

The last three entries in the `downloaded/` queue were the ones that are not
plain text: two PDFs and a Word document. All three are now wired; the
"not plain text" bullet in the PARKED section below is struck through.

**Second Chance** (David Whyld, 2005) — ★ **WIN, the good ending, VERBATIM.**
`SecondChance_walkthrough.pdf` is not prose but a **full session log**: every
command is on its own `>`-prefixed line, so the 49-command script falls out of
the document mechanically and replays without one repair. The only thing that
needed knowing was a harness detail — the title sequence embeds two
`<waitkey>` pauses, which eat the first two commands and desync the whole run,
so the row carries `SCR_SKIP_WAITKEY=1`. The game keeps **no score**
(`score` → "No one's keeping score."); its endings are ranked by whether the
three vignettes went well (Dolores → `push button` to call the police; Jenny →
talk, never the "ask about sex" branch; Doug → three `talk to doug`; Antonia →
actually search her room). The marker is therefore the closing line of the
good ending, `congratulating me on a job well done.` See
`Second_Chance_walkthrough.md`.

**Private Eye** (David Whyld, 2006) — ★ **WIN, score 4, best ending,
VERBATIM.** A pure numbered-choice game: no parser verb appears anywhere in
the 73-choice route, so the solution file is a column of digits. The
walkthrough section of the shipped 116-page `PrivateEye_guide.pdf` replays
exactly. Two harness facts closed it: the PDF silently omits the title menu
(the script carries a leading `3` = "Play Private Eye"), and
`SCR_SKIP_WAITKEY=1` is mandatory or the run never leaves the menu. One
12-word sentence in the PDF — "No sooner have I put the phone down than Jim
ambles in." — has no counterpart in the transcript; it is the author bridging
two scenes in the write-up, **not** output we drop: the game's own wording is
longer ("…and plonks himself down in the other chair"), it lives on the
ex-girlfriend phone-call tasks rather than Layla's, and the short form appears
nowhere in the inflated `.taf`. Word-diffed, 19293/19371 words identical
(0.9975). See `Private_Eye_walkthrough.md`.

**The Plague – Redux** — ~~**UNFINISHABLE AS SHIPPED**~~ **[VERDICT REVERSED
2026-08-04 — the game is ★ WINNABLE via the where=1 pole-bypass tasks; see
the entry at the top of this file. The where=0 analysis below stands; the
dead-end conclusion does not.]** Original entry, kept for the probe record: The game's whole `[F] Fight / [E] Escape` system is seven
identical task blocks (`ZOMBIE 1`…`ZOMBIE 7`) in which **every** task sits at
Where/Type = 0 (`ROOMLIST_NO_ROOMS`) — 243 of the game's 696 tasks are parked
there. Nothing `ExecTask`s the `[f]`/`[e]` pair, so once "[F] Fight or [E]
Escape?" prints there is no input that answers it (`f` → "That didn't make any
sense!", `fight` → "That wasn't the answer.", `e` is eaten by the library as
*east*). The first mandatory fight is the Women's Toilet cubicle, whose coins
are the last 10p of the £1.20 the water vending machine wants — so the route
dead-ends at £1.10 and everything downstream (Kate, the office vent, the
camera batteries for the torch, Ray, the staff-area keys, the tunnels,
Candice, the ending) is unreachable.

Proved against the real Runner **twice**, which is what makes this a verdict
rather than a suspicion:

1. **Isolated probe** — new `test/adrift4/harness/make_400_whereprobe.py` builds a minimal 4.0
   game with `alpha` at where=0, `beta` at where=3, `gamma` at where=1 scoped
   to the other room, repacked with `taftool.py`. `run400.exe` fires beta,
   refuses gamma, and refuses alpha — identical to SCARE. So SCARE's
   `ROOMLIST_NO_ROOMS` handling is *correct*, and a where=0 task genuinely
   cannot be typed.
2. **Game-level probe** — a copy of this very game with `#StartRoom` patched
   `0` → `15` (Women's Toilets; line 80 of the unpacked plain body, right
   after the `bd d0` separator at line 79) loaded in `run400.exe` under Wine
   reaches the byte-identical cubicle scene and answers `f` with "That didn't
   make any sense!".

This is the **second** game in the corpus killed by this exact mistake — The
Hangover (2026-08-03, below) loses its two endgame tasks the same way, checked
against run390. Worth remembering as a diagnosis: when an ADRIFT 4 walkthrough
asks for a command the game flatly does not understand, dump the tasks and look
at `where=` before assuming a parser bug.

The row is blessed as a documented dead end — a maximal-reachable run that
replays the shipped `.doc` as far as the game allows, types `f`/`fight`/`kill
zombies` at the prompt to record the three refusals, collects all five
reachable coin caches, and ends at the vending machine. Marker: `I pressed the
vend button but nothing happened.` See `The_Plague_Redux_walkthrough.md`.

Two derivation footguns from this game, both costly: **the discovery verb is
`SEARCH`, not `EXAMINE`** — the `.doc` says "Examine the till" throughout, but
`x till` answers "nothing interesting" and only `search till` works (same for
rides, body, counter, windows, highchairs, bench, condom machine, Nick, bin,
footwear, desk) — and **`x coins` is the money counter**, `count coins` is not
a verb.

## 2026-08-04 — six more 3.80 games, and the only two surviving 3.70 games

The 2026-08-03 survey below concluded "exactly 11" ADRIFT 3.80 games exist
online. That was too low, for one reason worth writing down: **adrift.co serves
files its adventure database never lists**, so enumerating the database (634
entries) misses them, and the IF Archive never had them either.

The name list has to come from somewhere else — the Wayback copies of Campbell
Wild's own *Adventures* pages: `tardis.ed.ac.uk/~jcw/adventure/adventure.html`
and `~jcw/adrift/adventure.html` (Jul 2000 – May 2001) and
`jcwild.pwp.blueyonder.co.uk/adrift/adventure.html` (Jul 2001 – Mar 2002),
11 captures, **130 distinct filenames**. Probing each against
`www.adrift.co/files/games/<f>` found six more 3.80 games, now in `games/` with
MAP rows (all six load and run; suite **168 PASS / 14 NOSCRIPT**, exit 0):

| file | title | size |
|---|---|---|
| `duck.taf` | Duck McCloud — The Fight Begins (Dale Trigg & Ashley Canning) | 7.8 K |
| `first.taf` | The book of Fistandantalus | 12 K |
| `jb2000.taf` | James Bond — Happy Landings | 11 K |
| `microwaveman.taf` | Microwave Man! | 5.2 K |
| `mikes.taf` | The life of Mike | 19 K |
| `superliam.taf` | Super Liam 1. A hero is born | 31 K |

So **17 ADRIFT 3.80 games survive**, and `microwaveman.taf` (5.2 K) is now the
smallest 3.8 game — a better first walkthrough target than `secret.taf`.

**Two ADRIFT 3.70 games also survive**, both unlisted on adrift.co and on no
other host: `arlo.taf` (*Alice's Restaurant Anti-Massacree Adventure*, Laura
Lee, 18-03-2000, 70 K) and `castle.taf` (*Castle Quest*, Andrew Cornish,
10-06-2000, 14 K). Header `3c423fc96a87c2cf94453961 39fa` — note byte 10 is
`0x39`, **not** the `0x35` a naive "one less than 3.80" guess predicts, which
is why an earlier sweep with a guessed signature found nothing. The container is
identical (same XOR'd CRLF plaintext, same `taf38schema.xor`).

*(Same day, later: **both are now playable and in `games/` with MAP rows.**
`V370_PARSE_SCHEMA` in `sctafpar.cpp` covers the four layout differences — the
extra header integer is the **0-based winning-task index**, movements are pairs
over one flat destination list, tasks have no `BWinGame`, and a fixed block of
17 renameable built-in command words replaces the 3.80 synonyms table — and
every one of those, plus the pooled burden model and the object initial-position
list, was then **measured against the genuine `run370.exe`** rather than left as
inference. Two real bugs fell out of the probing, one of them the 3.80-wide
`Parent = -1` bug that had been hiding worn objects in `tra.taf`. See
`ADRIFT_370.md` and `../../../RUNNER_TESTS_TODO.md` §6.)*

*(Same day, later still: **both 3.70 games are now solved**, at full score —
`castle_quest_solution.txt` 17 moves / **50 of 50**, and
`alices_restaurant_solution.txt` 85 moves / **190 of 190**, every one of Arlo's
30 scoring tasks. Both PASS against blessed goldens; see
`Castle_Quest_walkthrough.md` and `Alices_Restaurant_walkthrough.md`. Castle
Quest is a pure trap-avoidance map with five scoring and five killing tasks and
no objects at all; Arlo's one hard puzzle is that `play guitar` starts the
horse's *follow-the-player* walk, so you lead the horse out of the stable and
then `stop playing` — reversing the start task cancels the walk and strands it
in the garden, freeing the cabinet.)*

*(Unrelated, same day: 38 of the `games/` entries were symlinks into
`~/Downloads/plover-adrift-2026-08/`, which briefly vanished and SKIPped 38 rows.
**Every symlink in `games/` has since been replaced by a real file** — all 72 of
them, not just the 38 — so the corpus no longer depends on anything under
`~/Downloads`. 191 files, 25 MB. Suite: **173 PASS / 0 FAIL / 0 SKIP /
14 NOSCRIPT**, exit 0 — every game in the MAP is present and passing.)*

Nothing **3.60 or older** survives anywhere: the IF Archive holds zero (its
Jan-2001 index listed the same seven 3.80 files it has today, and re-decoding
its 9 unclassified `.taf` headers shows all nine are 5.00), the adrift.co store
holds zero across 619 DB entries + 540 further Wayback-known filenames, and the
oldest game Campbell ever listed is `haunted.taf` (13-06-1999) — which survives
only as a 3.80 re-save. `jacjim.taf` (*Jacaranda Jim*) is gone from every
reachable host. Note that ADRIFT 3.23 (13 Jun 1999) was the first version to
encrypt TAFs at all, so anything older would be **plain ASCII** starting
`Version 3.xx` — no such file turned up either.

Method gotcha worth keeping: `Range: bytes=0-13` classifies a remote `.taf`
cheaply, but adrift.co sometimes answers a ranged GET with an **empty body**;
a short reply must be retried unranged or the file is silently misfiled as
unknown. Filenames with spaces must be percent-encoded or curl fails outright.

Aside, same sweep: `www.adrift.co/files/games/tagv2.zip` is Campbell's **TAG**
(1994–97 DOS Pascal ancestor of ADRIFT) — `gen2.exe`, `genrun2.exe`,
`space.tag`, `BAR.TAG`. Its `genrun2.exe` is MD5-identical to the one shipped
with Tony Ash's *The Edge of the Abyss*, i.e. this is the `tag.zip` that
"Abyss"'s ABOUT.TXT says vanished with Campbell's old URL. Not a taf; nothing
for Scarier to do with it, but it is the whole surviving TAG corpus (3 games).

## 2026-08-03 — the complete TAF 3.80 corpus is in `games/` — 165/165 PASS, 8 NOSCRIPT

**There are exactly 11 ADRIFT 3.80 games online, and we now have all of them.**
Surveyed by reading the 14-byte header of every candidate on the only two hosts
that still carry ADRIFT games: all 240 files under
`ifarchive.org/if-archive/games/adrift/` (`/`, `competitions/`, `italian/`,
`old/`, `spanish/`, opening every zip and blorb) and every pre-2005 download in
the adrift.co adventure DB (634 entries; `POST /cgi/adrift.cgi` with
`page=adventures&offset=N&compid=-1&compilation=0&sort=1&asc=0&category=0&perpage=50`,
which also tags each row with its own version icon `/img/380.gif` — that tagging
agreed with the bytes on all 9 of its 3.80 rows). The Archive holds 7, adrift.co
9, overlapping in 5. For scale, the same sweep counted ~49 v3.90 and ~349 v4.00
taf instances on the Archive alone: **3.80 is ~1.5% of the ADRIFT corpus.**

Catalogues cannot answer this question. IFDB's oldest ADRIFT format is
`adrift39` (id 44) and IFWiki's oldest category is "ADRIFT 3.9 works", so every
3.8 game is filed as 3.9 in both. The only reliable test is the header: a 3.x/4.x
TAF opens with `"Version X.YZ\r\n"` XOR the fixed VB6 keystream, so the first 14
bytes are a constant per version (`V380_SIGNATURE` &c., `sctaffil.cpp:53`), and a
`Range: bytes=0-13` request classifies a remote file without downloading it.
`3c423fc96a87c2cf94453661 39fa` is 3.80; 5.00 files share the first 8 bytes and
decode to `"Version 5.00"`.

| file | title | source |
|---|---|---|
| `marooned.taf` | Marooned | IF Archive **(had, WIN 80/140)** |
| `wrecked.taf` | Wrecked | both **(had, WIN 250/250)** |
| `Crime_Adventure.taf` | Crime Adventure | IF Archive **(had, WIN 95/95)** |
| `akron.taf` | Akron | IF Archive |
| `cave.taf` | Cave of Wonders | both |
| `haunt.taf` | House of the Damned | both |
| `twilight.taf` | The Twilight | both |
| `haunted.taf` | The Haunted House (Campbell, Jun 1999 — the oldest) | adrift.co only |
| `great.taf` | The Great Escape | adrift.co only |
| `secret.taf` | Tom Ceader: Escape from the south | adrift.co only |
| `tra.taf` | The Timmy Reid Adventure | adrift.co only |

The eight new ones are in `games/` and have MAP rows in
`run_v4_walkthroughs.sh`, so they report **NOSCRIPT** until routes are derived
(suite unchanged at 165 PASS, exit 0). Their win markers are deliberately blank
— filling one in before the route exists would bless a marker nobody has seen
the game print. All eight were smoke-run in `scare` (`look` / `score` / `quit`):
every one loads, prints its title and intro, and takes commands.

**Why this matters beyond completeness:** the 3.80 burden and container model
settled against the genuine `run380.exe` (pooled burden, class costs
`0→1 1→3 2→7 3→3 4→7`, capacity `= #MaxCarried`; contents free, `Capacity` a plain
object count, dynamic containers must be *held*) was derived on three games and
can now be exercised on eleven.

**One lead fell straight out of the smoke run.** `tra.taf` prints
`gs_create: object held by nonexistent NPC, -2` three times at load — i.e.
`initialparent == -1` on the `InitialPosition == 1` ("held by") path in
`scgamest.cpp:977`, so those objects are silently **hidden** instead of starting
in someone's inventory. `-1` is 3.80's generic "no parent" filler (48 of
`tra.taf`'s 248 objects carry it), which is the tell: 4.00's convention is
`Parent 0 == held by the player`, and 3.80 looks like it uses `-1` there
instead. Worse, the same file has objects with `InitialPosition 1` and `Parent`
21 / 2 / 28 (`garage key`, `baseball glove`, `baseball`, `guitar`), which the
4.00 rule reads as NPC `parent-1` — if 3.80's index is 0-based those are all off
by one, and being off by one is *silent*. Nothing in `sctafpar.cpp`'s
`|V380_OBJECT:_InitialPositions_|` fixup touches `Parent`; it only handles
positions `2` and `> 2`. Settle it the way the size/weight model was settled —
a `make38probe.py` file with one object held by the player and one held by NPC 0,
run under `run380.exe` in the adrift-battle Wine prefix. No other 3.80 game in
the corpus trips the warning, so `tra.taf` is the test case.

> **CLOSED 2026-08-04.** Measured exactly that way, and the guess above was
> right: pre-4.0 has no way to start an object *on an NPC* at all, so `Parent`
> is meaningless on the held and worn entries. `run380` gives `tra.taf`'s red
> sox hat (`Parent 0`) and its loose change (`Parent -1`) both **to the
> player**, and `run370` gives `castle.taf`'s sweatshirt to the player whatever
> `Parent` says. The fixup — now shared by 3.8 and 3.7 as
> `|V380_OBJECT:_InitialPositions_|` / `|V370_OBJECT:_InitialPositions_|`,
> `sctafpar.cpp` — forces the holder to the player on both entries, so the
> hidden objects come back. Separately, `gs_create()` now *validates* an
> out-of-range parent instead of storing it and assert-crashing on the first
> turn update (`scgamest.cpp`; regression `harness/badparent_test.cpp` +
> `make_badparent_taf.py`, which is where The Timmy Reid Adventure and Blood
> Relatives are pinned).

## PARKED 2026-08-03 — the `downloaded/` wiring run stops here, at 161/161 PASS

> **Superseded 2026-08-04 — this park is over and every item below is struck.**
> The run resumed and finished: Ba'Roo!, Lair of the Vampire, The Fugitive and
> The Dead Man were all derived and won, and the five undownloaded games
> (Chosen, Crimson Detritus, ImagiDroids, Panic!, The Cellar) arrived and were
> wired. **`downloaded/` is fully wired — every walkthrough in it has a game
> and a row** — and the suite is at 203 PASS / 0 NOSCRIPT. Kept for the
> resume-order notes and the two habits at the end.

Clean stopping point: suite green (no FAIL / SKIP / NEEDGOLD / NOSCRIPT /
REGRESSIONS), every golden blessed against the current binary, **nothing
committed**. The two `scparser.cpp` fixes, the `sctasks.cpp` negative-object
guard, the `scdump.cpp` additions below and the `sctafpar.cpp` 3.8 size/weight
fixup are in the working tree only.

To resume, the remaining `downloaded/` candidates that already have a staged
`.taf` are, roughly easiest first:

* transcripts (the format that has replayed near-verbatim seven times running)
  — ~~`LockedDoorWithWaterTrap_transcript.txt`~~ (wired),
  ~~`Marooned_walkthrough.txt`~~ (wired, 80/140 — see below),
  ~~`Wrecked_walkthrough.txt`~~ (wired, **250/250** — see below),
  ~~`Mortality_walkthrough.txt`~~ (wired, verbatim — see above)
* command lists / sectioned prose — ~~`ThreeMonkeysOneCage_solution.txt`~~ →
  `3monkeys.taf` (wired, **98/100 = ceiling** — see above),
  ~~`LargoWinch_solution.txt`~~ (wired, **97/97** — see
  above), ~~`Humbug_walkthrough.sol`~~ → `humbug.taf` (wired,
  **2000/2000 = full score** — see above),
  ~~`CrimeAdventure_walkthrough.sol`~~ → `Crime_Adventure.taf` (wired,
  **95/95 = full score** — see above)
* prose only, needs real derivation — ~~`TheSisters_walkthrough.txt`~~ →
  `TheSisters.taf` (wired, **109/109 = full score** — see above),
  ~~`ThePKGirl_walkthrough.txt`~~ → `the_pk_girl.taf` (wired, **Katryn 55/60,
  Katryn's ending** — see above)
* not plain text — ~~`SecondChance_walkthrough.pdf`~~ → `second chance.taf`
  (wired, **WIN, good ending, verbatim** — see below),
  ~~`PrivateEye_guide.pdf`~~ → `Private Eye.taf` (wired, **WIN, score 4 =
  best ending, verbatim** — see below),
  ~~`ThePlagueRedux_walkthrough.doc`~~ → `The Plague - Redux.taf` (wired,
  **UNFINISHABLE as shipped** — see below)

~~Still parked from earlier: Ba'Roo! (needs real derivation), Lair of the
Vampire (desyncs badly), The Fugitive (prose only). `TheDeadMan_walkthrough.html`
has no `.taf`. Five games are still undownloaded: Chosen, Crimson Detritus,
Imagidroids, Panic, The Cellar.~~ *(All nine wired 2026-08-04 — see the banner
at the top of this section.)*

Two habits from this batch worth keeping: check `OBJNAME ... prefix=[...]`
before believing "I see no such thing", and treat a published *session
transcript* (as opposed to a hand-written command list) as Runner ground truth
— two of them caught real engine bugs today.

## 2026-08-03 — Marooned — **WIN, 80/140** — and the TAF 3.8 size/weight bug behind it — 158/158 PASS

`marooned.taf` is the first **TAF version 3.80** game in the corpus (`xxd -p -l
12 f.taf | cut -c17-22` → `944536`; `934536` = 4.0, `944537` = 3.9), and it did
not merely desync — it was *unfinishable*, because two objects the game hands
you could not be picked up:

```
>get tires
Your hands are full.
>get seal
The dead seal is too heavy for you to carry.
```

**The cause is a parser bug, not the game.** Version 3.8 stores a single
"Size/weight" **class index**, 0..4, per object. Every 3.8 grammar in
`sctafpar.cpp` read `#SizeWeight` raw and handed it to the 4.0 model, which
packs *size* in the tens digit and *weight* in the units and scales each as
`base^digit`. So class 4 ("very large") arrived as weight `3^4 = 81` against
Marooned's limit of `8 * 3^2 = 72`, and class 2 as `3^2`… wrong axis, wrong
magnitude, silently. New fixup `|V380_OBJECT:_SizeWeight_|` normalises every
3.8 object to 4.0 "normal" (22).

**Why normalise instead of converting the class.** ADRIFT Generator 3.90 does
convert it, and we know its exact table, because gen390 is available under Wine
and its conversion of `marooned.taf` diffs cleanly against our parse (new `sw=`
field on `SCR_DUMP_OBJLOC`, plus a new `PLAYERLIMITS` line):

```
3.8 class  ->  3.9/4.0 SizeWeight
0 normal   ->  22        3 large      -> 32
1 heavy    ->  23        4 very large -> 42
2 very heavy-> 24
```

That table is what ADRIFT itself does, and it is **wrong for playing the 3.8
file**: it breaks the games. On gen390's own conversion, the genuine
`run390.exe` refuses the tires with the same "Your hands are full." we used to
print. Crime Adventure (`Crime_Adventure.taf`, also 3.80, limit 45) goes the
same way — its kettle is class 2 → weight 81 — yet its published solution says
"Get all the stuff in Fenwick kitchen". Two independent 3.8 games, both
unwinnable for their own authors. The 3.8 header carries only **MaxCarried**, a
plain object count, and the two neighbouring 3.8 fixups already assume counts
(`Capacity*10+2`, `MaxCarried*10+2`) — those only mean "N objects" if every
object is normal-sized. Normalising makes the model self-consistent and
reproduces the author's own walkthrough line for line ("You pick up the couple
of tires."). Recorded as an ADRIFT-conversion bug, not ours.

~~Not provable to the last inch: **run380.exe cannot be obtained.**~~ adrift.co
serves `/files/run380.zip` and `/files/gen380.zip` with HTTP 200, but both
archives contain the **3.90** binaries; IF Archive and its mirrors, the Wayback
Machine (ftp.gmd.de, ftp.tardis.ed.ac.uk, the egroups files areas) and
archive.org have no copy. run390 refuses 3.80 files outright ("You will need to
convert it with ADRIFT Generator 3.90"), ~~so gen390 conversion is the only
ground-truth path available for 3.8 games~~.

> **Superseded the same day — `run380.exe` is not lost.** David Whyld's dead
> `delron.org.uk` still serves `adrift38.zip` through the Wayback Machine (and
> `adrift37.zip` beside it); both are installed in the adrift-battle Wine
> prefix, so 3.8 and 3.7 now have direct ground truth like 3.9 and 4.0 and
> nothing about them depends on a gen390 conversion any more. Measured against
> it, the conclusion above survives but the mechanism grew: version 3.8 has
> **one pooled burden**, per-class costs `1/3/7/3/7`, limit exactly
> `#MaxCarried` — a cost of 7 that 4.0's packed `base^digit` cannot express,
> which is precisely why gen390's conversion breaks the games. So the class is
> no longer merely discarded: the fixup keeps it verbatim in `SizeWeightClass`
> and `obj_get_burden()` (`scobjcts.cpp`) spends it, while `SizeWeight` stays
> normalised to `22` so container capacities remain the plain object counts 3.8
> means them to be. Marooned's route was re-derived under the real model — it
> can carry only one heavy object at a time and now **ferries**, 119 commands,
> same **80/140** win. Full record: the "TAF 3.8 object Size/weight class" row
> of `../../../RUNNER_TESTS_TODO.md` §4; game-level write-up:
> `Marooned_walkthrough.md`.

**The route: 80/140, and 80 is the ceiling.** The published walkthrough is for a
*different build* (its boat description differs from ours; our title bar says
"Marooned v1"), so this one is freshly derived from the task dump + the
`EXIT`/`ROOM` map. The remaining 60 points are structurally unreachable:

* **task 24 (get trash, 10)** — its two commands, `get *trash` / `take *trash`,
  are both already ALTCMDs of the repeatable task 14 in the only room the trash
  exists in, so 14 always wins the match.
* **task 27 (swallow pill, 10)** — requires the berries eaten, but the berries
  are the monkey's price for the flint, and the flint is mandatory to light the
  fire.
* **task 35 (shoot flare gun at shark, 10)** — requires holding object 20, the
  *unloaded* gun; loading it (task 19) destroys object 20 and creates object 37,
  and firing object 37 runs task 18, which kills you. Unreachable by
  construction.
* **tasks 9 + 28 (scratched can pour/light, 20)** — only the *dented* can's
  lighting task (10) starts event 5 "Rescue" (`startTask=11 affTask=30` → the
  `#win` task 29). Pour the scratched can too and task 9 wins the `light tires`
  match, the tires burn up, and the ship never comes. So the scratched can goes
  to the shark (task 33) instead — same 10 points, and the win survives.

One flourish worth keeping: at the lagoon, `throw <anything>` is stolen by the
non-repeatable, score-0 task 15 (`throw %object% *`). Burn it on the map you
picked up off the bridge, and the next throw reaches task 33 for its 10 points.

## 2026-08-03 (earlier) — The Amulet — **WIN**, verbatim, and the double-"Congratulations!" settled — 156/156 PASS

*The Amulet* (3-hour comp, Daniel Hiebert again) is a **verbatim** replay: all
12 commands from the author's transcript, no repairs, flavour lines (`notes`,
`spells`) included. The game has **no scoring whatsoever** — `score` answers
"0 out of a maximum of 0" and `SCR_DUMP_TASKS` finds zero `ACT type=4` — so
reaching the ending is the only measure of the route.

The one difference from the published transcript is that we print
**"Congratulations!" twice**, and it is worth writing down how that was
settled, because "our transcript has an extra line" normally means a bug.
`SCR_DUMP_TASKS` now prints a `WINTEXT [...]` line; all three of the games
wired today have an **empty** `WinText`, which is exactly the case where
`task_run_end_game_action()` falls back to its hard-coded "Congratulations!".
Shadrick's Travels also has an empty `WINTEXT`, its winning task text does not
contain the word, and its published transcript still shows one
"Congratulations!" — which is only possible if the real Runner prints the
default too. So in The Amulet the *first* one belongs to the winning task's own
CompleteText and the second is the engine's; the Runner would show both, and
the author trimmed the duplicate when writing the walkthrough up. Our
transcript is the faithful one.

## 2026-08-03 (earlier) — Monsters (Release 2) — **WIN, 40/40**, and two real parser bugs — 155/155 PASS

*Monsters (Release 2)* by Daniel Hiebert wins at **40/40**, which is the whole
game: `SCR_DUMP_TASKS` counts eight `ACT type=4` actions worth 5 each, and this
route fires all eight. The upstream file is a real-Runner session transcript
with no prompt glyph at all — commands and responses simply alternate — so the
38 commands were lifted by hand.

Exactly **one repair**: `open the bedroom door` → `open door`. The game has two
door objects, obj13 (the bedroom's own door, room 2) and obj48 (Mommy's, rooms
3 and 9); "bedroom door" collides and the parser answers "Open what?", leaving
`s` blocked on `RESTR type=1 v1=7 obj48`. The bare noun reaches the right one.

**The interesting part: this game's transcript is ground truth, and it caught
two genuine SCARE parser bugs.** Both are fixed, and neither moved any other
golden in the 154-row suite.

1. **`uip_match_optional()` did not rewind on a failed look-ahead.** It rewinds
   when the look-ahead *succeeds* and consumed text, but when the look-ahead
   fails it fell straight into `uip_match_alternatives()` at whatever position
   the failed attempt had already advanced to — and `uip_match_list()` has no
   backtracking of its own. Task 2's pattern is

   ```
   [defeat/shine/turn/put] {the} [flashlight/light] {on} {the} {brainsucker} {brain}  {monster}
   ```

   Against `shine flashlight on the brainsucker`, the look-ahead from
   `{brainsucker}` let `{brain}` eat the first five letters of "brainsucker"
   (`uip_match_word()` is a prefix compare with no word-boundary check), then
   died on the trailing "sucker"; the alternatives were then tried from
   "sucker", `{brainsucker}` matched nothing, and the pattern failed. Net
   effect: the command the author's transcript shows working was rejected and
   the game lost 5 points. `shine flashlight` (no object words) worked fine,
   which is what makes this kind of bug so easy to miss.

2. **`%object%` never answered to a partial prefix.** `uip_build_candidate()`
   composed exactly two strings, `"Prefix Short"` and the bare `Short`, so
   `examine the four poster bed` — Prefix `Sissy's four poster`, Short `bed` —
   was "I see no such thing", while the author's transcript prints the object's
   description. The candidate now also carries the prefix with its leading
   words dropped one at a time (`four poster bed`, `poster bed`, `bed`); the
   *name* is never cut down, so a two-word Short still has to be given whole.

   This one has **independent confirmation**: re-running the suite changed
   exactly one line in one other golden, Shadrick's Travels, where
   `climb oak tree` went from "You can't climb that." to "You can't climb the
   old oak tree." — and line 80 of *that* game's upstream transcript reads
   "You can't climb the old oak tree." Two unrelated real-Runner transcripts,
   same verdict.

`SCR_DUMP_TASKS`'s `OBJNAME` line now also prints `prefix=[...]` and every
`alias=[...]`, which is what turned "why doesn't this noun match?" from a
guess into a lookup. Use it first whenever a walkthrough line comes back "I see
no such thing".

## 2026-08-03 (earlier) — Shadrick's Travels — **WIN, 100/100**, verbatim — 154/154 PASS

A fifth verbatim replay, and the cheapest one yet: *Shadrick's Travels* by
Mystery replays **exactly as published**, 22 commands, no repairs, ending on
`Congratulations!` with the full 100 points.

The only trick is extraction. The upstream file is a session transcript whose
prompt glyph is a **CP1252 `Ø` (0xD8)**, not `>`; the commands are the lines
that start with that byte:

```python
d = open('downloaded/ShadricksTravels_walkthrough.txt','rb').read().decode('cp1252').replace('\r','')
cmds = [l[1:].strip() for l in d.split('\n') if l.startswith('Ø')]
```

`SCR_DUMP_TASKS` reports `TOTAL 100 4` — the game contains exactly four
`ACT type=4` scoring actions (`tie rope to wood` +20, `tie swing to tree` +20,
`throw rock at hive` +10, `swing on swing` +50) and this route fires all four,
so 100 is provably the maximum, not just a high score.

Three of the 22 commands are the author's own duds and are **kept on purpose**:
`x wood` and `climb tree` both land on the disambiguator, and `tire swing to
tree` is a typo for `tie`. ADRIFT's "Please be more clear…" does not consume
the following line, so a dud costs nothing but keeping it means the golden is a
faithful record of the published transcript. No env is needed — the game never
paginates, and the transcript is byte-identical with and without
`SCR_SKIP_WAITKEY=1`.

## 2026-08-03 (later, 8) — four verbatim delron replays, wired — 150/150 PASS

Four games whose delron command lists replay **without a single repair**, which
is worth recording because it is the exception, not the rule:

| Game | Cmds | Ending | Row env |
|---|---|---|---|
| Beanstalk the and Jack (David Welbourn, 2008) | 49 | `*** You have won ***` | none |
| Black Sheep's Gold (2004) | 99 | "You've beaten Black Sheep's Gold!" | `SCR_SKIP_WAITKEY=1` |
| Doomed Xycanthus (2006) | 82 | "Congratulations!" | none |
| Dancing Even Him? (Richard Otter, 2006) | 17 | anagram reveal | none |

Notes:

* *Beanstalk the and Jack* is a reverse-chronology retelling, so the command
  list reads backwards — it opens on `chop beanstalk` and ends with Jack waking
  up. That is the game, not a scrambled walkthrough.
* *Black Sheep's Gold* is the only one of the four that needs
  `SCR_SKIP_WAITKEY=1`: its epilogue stops on "(press any key to continue)" and
  without the skip the pause eats the trailing `quit`, so the ending never
  prints and the win marker looks absent.
* *Dancing Even Him?* — the title is an anagram of "Vending Machine", which the
  ending text spells out, so that line is the win marker.

The only work here was trimming the delron page footers ("Any donation would be
much appreciated", "Home | About Me") off the extracted text.

**Correction (2026-08-04):** that trim had in fact only been done for two of the
four. *Beanstalk the and Jack* (56 → 49) and *Doomed Xycanthus* (89 → 82) still
carried the seven footer lines as trailing commands. They were harmless — both
games end on an EndGame action (`sleep` and `jump` respectively) and SCARE stops
reading input at that point, so the junk was never consumed and the goldens are
byte-identical before and after — but the counts above were wrong and the files
now stop at the last real command. Both still PASS unchanged.

## 2026-08-03 (later, 7) — A Spot Of Bother — **WIN, 100/100**, wired — 146/146 PASS

*A Spot Of Bother* (David Whyld, 2005) reaches the author's own stated maximum,
and the upstream transcript needed exactly **one** repair in 270 commands: a
second `push door` in the gymnasium. The first push is a task that only reveals
the trap wire ("A wire, previously hidden, slips into view") — it does not open
the door, so after `break wire` the east exit still answers "You need to open
the door first." and `open door` answers "You can't open the door!". Pushing
again opens it and the remaining 197 commands replay verbatim.

The upstream file is a full session log including the title menu, so the first
two commands of the solution are the menu picks `2` (read the introduction) and
`1` (play). The row needs `SCR_SKIP_WAITKEY=1`: the game paginates heavily with
`[MORE]`, and without it every pause eats a command.

Everything else — including the `enter password elephant` crossword answer, the
three consecutive `3`s at the computer menu, and the long combination-lock
sequence on the safe — matched the upstream text response-for-response on the
first try, which is a good sign for the engine on a 2005-era 4.0 game with this
much task machinery.

## 2026-08-03 (later, 6) — Troll! — **WIN, 185/190**, wired — 145/145 PASS

*Troll!* (Peter Frøhlich-style pastiche, 2003 ADRIFT) is winnable, and the
route now reaches the ending with **zero** parser errors in 145 commands. Its
real ceiling is **185 of 190**, not 190: the game has 38 scoring tasks worth 5
points each, and one of them is structurally unreachable.

```
TASK 80 [* pay * barman *]    needs obj67 "fourtune", turns it into obj68
TASK 82 [* pay * landlord *]  needs obj67 "fourtune", turns it into obj68
TASK 81 [* pay * landlord *]  needs obj68 "fortune",  turns it into obj69
```

80 and 82 consume the same object, so at most one can fire — and 80 is
compulsory, because it is the task that summons the coach home (it moves
`obj21 coach` and `obj78 horse` and walks the player to room 3). 80 + 81 is
therefore the best pair available and **82's 5 points are dead**.

The published walkthrough scores 160/190. Everything else was recoverable:

| Recovered | Why the walkthrough missed it |
|---|---|
| `pay landlord` (TASK 81, +5) | it never pays the landlord at all — the `in` / `pay landlord` / `out` detour at Outside Inn on the way home is ours |
| `take breadcrumbs` (TASK 85, +5) | it says `get crumbs`, which the standard library resolves happily without firing the task |
| `w` upstairs **before** `unlock door` (TASK 86, +5) | TASK 86 is gated on TASK 64 `[* unlock * door *]` **not** being done, so the order matters: you score for walking into the locked door, *then* unlock it |
| `put breadcrumbs in basin` a **second** time (TASK 51, +5) | TASK 51 wants `obj48`, and `obj48` only reaches the player's hands as an *action* of TASK 52 (the firewater step). The first put is the library's |

Two of the walkthrough's commands are simply the wrong way round for the
task patterns, neither of which carries a reversed `ALTCMD`:

* TASK 61 is `[* show * barman * medallion *]` → `show barman medallion`
* TASK 83 is `[* give * chief * backward burp berries *]` → `give chief backward burp berries`

And two inventory hazards: the opening `get all` overflows the carry-weight
limit on a pink flyer, a green notice and a blue advert (dropped; five
explicit drops added instead), and the tavern drops must happen **on entry**,
not after `drink whiskey` — a timed event throws the player outside three
turns after the last drink, and the walkthrough's following `out` then fails.

The game ends inside `tickle frog` and never prints a final score, so the
golden's win marker is the closing line and the `score` just before it reads
180; the last task's 5 points land in the ending text.

## 2026-08-03 (later, 5) — The Hangover — **UNWINNABLE, max 5/7**, wired — 144/144 PASS

*The Hangover* (Red Conine, IFComp 2009 ADRIFT) is unwinnable as shipped, and
both dead ends are the author's. The two tasks the published walkthrough's
endgame turns on carry `Where/Type = 0` (`ROOMLIST_NO_ROOMS`), so they can
never run in **any** room:

```
TASK 10 where=0 room=-1 restr=1 cmd=[give the doctor some french fries]
TASK 14 where=0 room=-1 restr=3 cmd=[give approval notes to platypus]
```

Every other room-scoped task in the game is `where=1`. Confirmed against the
real run390.exe, which answers "You can't do that here!" to both. So the
ceiling is **5 of 7**: bill→fries (Deby), kick bum, mail→secretary, open the
filing cabinet, `type 119-228-337-446`. The two lost points are the doctor's
approval form and the ending itself.

Losing the doctor costs only the point — the Psycho Hospital's south exit is
*not* gated on his keys, so the route runs all the way through the jet, the
bathrobe parachute and back to the Form Process Office. The office even prints
"Except this time, the platypus is here. He seems happy. You give approval
notes to platypus." as room description while the task that would do it sits
permanently unreachable, which is a good demonstration that the author never
tested the ending.

Row (`the_hangover_solution.txt`, 53 commands, no `SCR_SKIP_WAITKEY` needed):
`the_hangover_solution.txt|hangover.taf|Your score is 5 out of a maximum of 7.`

Two derivation notes:

- **The room descriptions lie about exits.** Fedrick Avenue says "To your east
  is a bus stop"; the bus stop is **west**. The Approval Form Office says the
  first form is "through the door behind me to the north" and that one is
  right, but the walkthrough says south. The Psycho Hospital Room's exit is
  west, not east. Trust `You can't go in that direction, but you can move …`.
- The first command of the transcript is a dead `Take what?` — the walkthrough
  opens with "Get Up", which this build does not implement (`get` with no
  noun). It is left out of the solution file; the player starts standing.

### Follow-up: the task refusals — **IMPLEMENTED 2026-08-10**

Both run390.exe and run400.exe carry the string ` can't do that here!` (VB6
UTF-16; grep the .exe decoded as `utf-16-le`, plain `strings` misses it), and
run390 prints it for these two tasks. SCARE had no such message anywhere:
`task_can_run_task` simply returned FALSE for a room-gated task, and the command
fell through to the standard library — here to "Give what?".

The condition was unproven when this was first filed, so it was left unfixed.
It has since been probed live — a synthetic 3.90 game
(`harness/make_39_whereprobe.py`, tasks with every `Where/Type`, plus a
task-state restriction and a non-repeatable task, plus a one-turn always-
restarting event so a turn is visible) driven through run390 under Wine, with
run370/run380/run400 checked against real corpus games. Measured:

| Runner | out-of-room task command | `Where/Type = 0` task | nonsense word | library-handled command | task whose restriction fails silently | done non-repeatable task |
|---|---|---|---|---|---|---|
| run370 (castle.taf)   | "You can't do that here." | — | DontUnderstand | — | — | — |
| run380 (marooned.taf) | "You can't do that here." | — | DontUnderstand | — | — | — |
| run390 (probe, hangover) | "You can't do that here!" (types 1 **and** 2) | same refusal | DontUnderstand | library wins | DontUnderstand | "You have already done that." |
| run400 (4.0 probe)    | DontUnderstand | DontUnderstand | DontUnderstand | — | — | — |

So the condition is **narrow and purely about the room**: the task's command
pattern must match, and the *only* thing blocking the run must be the `Where`
room list. Restrictions are irrelevant — a task whose restriction fails
silently gets DontUnderstand, not the refusal — and so is anything the standard
library already handled. The message is pre-4.0 only; 4.0 dropped it and prints
DontUnderstand instead. Punctuation follows the period: 3.7/3.8 end in `.`,
3.9 in `!`. The leading word follows `Globals/Perspective` — "I" for
`LIB_FIRST_PERSON`, "You" otherwise (pre-4.0 has only the two; run390 answers
"You" for perspectives 1, 2 and 3 alike). And it **consumes a turn**: with a
one-turn ticking event running, `gamma` prints the refusal followed by the
event text, where a nonsense word prints DontUnderstand and no tick.

Implemented as `run_where_refusal()` in `scrunner.cpp`, last in
`run_all_commands()` after `run_standard_commands()`, using the new
`task_is_room_refused()` predicate (`sctasks.cpp`) — which is
`task_can_run_task_directional()` with the room half inverted, the two halves
having been split into `task_state_allows_run()` / `task_where_allows_run()`.
An empty input line returns early, the same guard the DontUnderstand fallback
uses: without it a game with a bare `*` wildcard task command outside the
player's room turns every press-a-key blank line into a refusal
(`archie_solution.txt` caught exactly that).

**The corpus did not move**: 203/203 PASS with zero re-blessing. A solved
walkthrough route never types a task command in a room the task cannot run in,
which is why the feature needed synthetic coverage of its own —
`make -f Makefile.headless wheretest` runs the three probe games (Perspective
1, 0 and 2) against `harness/where_refusal_expected.txt`,
`harness/where_refusal_1p_expected.txt` and
`harness/where_refusal_3p_expected.txt`, and is part of `make test`.

One gap, accepted:

- The 3.7/3.8 period wording is proved live (run370 *Castle Quest*, run380
  *Marooned*) and gated on `version < TAF_VERSION_390`, but has no synthetic
  regression — the probe generator writes 3.90 only, and the V380/V370 GLOBAL,
  ROOM, OBJECT and TASK schemas all differ enough to need a second generator.
  The 3.7/3.8 corpus rows (`castle_quest`, `alices_restaurant`, `marooned`,
  `twilight`, …) pass unchanged.
The sibling divergence in the last column above — pre-4.0 Runners answer a
completed non-repeatable task with **"You have already done that."**, also a
turn — was **implemented the same day**, on the same probe extended with two
more tasks (`eta`, carrying a RepeatText; `theta`, both done *and* out of its
room) and a matching 4.0 pair. Three things the extension settled: an authored
**RepeatText displaces** the message; that half is **not** pre-4.0 — run400
prints RepeatText too, and only the bare message was dropped; and when both
blockers apply the **room wins**, so the already-done test carries the room
condition. `run_where_refusal()` became `run_task_refusal()` and covers both.
Again zero corpus movement (203/203, nothing re-blessed) even though **62 of
196** corpus games author a non-repeatable task with a RepeatText (528 tasks —
`SCR_DUMP_TASKS=1`, new `rpt=` column): solved routes do not re-type completed
one-shot tasks. Full record in `RUNNER_TESTS_TODO.md` §4/§5.

The last finding from the same probe run — pre-4.0 has **only two
perspectives**, so run390 renders `Globals/Perspective` 1, 2 and 3 alike in the
second person where Scarier read 2 as third — was **implemented 2026-08-10**
too, as `lib_get_perspective()` in `sclibrar.cpp`. The probe is generated a
third time at Perspective 2 and the script types `i`, so the regression is that
`where_refusal_3p_expected.txt` stays byte-identical to
`where_refusal_expected.txt`. A census of the corpus (new `GAME version=
perspective=` dump line) found **no pre-4.0 game authoring Perspective 2** — the
three that do are all 4.0 and keep their third person — so again no walkthrough
golden moved. The **capacity probe** goldens did: both are authored
Perspective 2 at 3.9, and their 144 changed lines each move onto the run390
transcript recorded in `RUNNER_TESTS_TODO.md` ("You put the b1 inside the
c52t.").

## 2026-06-25: deaths (*Death's Door*) — **WON, full 100/100**

`deaths_walkthrough.md`; solution `goldens/deaths_solution.txt` (1st two lines =
name `Hero` + gender `male` — both start-up prompts). ADRIFT **3.90** dungeon
crawl ("free the house of death"): break into a 3-storey building, collect four
coloured keys, kill the **Dark force** in the attic. Was on the "untouched/boot
via name+gender prompt" list. **Not a hang — boots fine once name/gender are
fed** (same class as Theannihilationofthink2/CyberCow). Deterministic; **100/100
is the true max** (9 ChangeScore tasks sum exactly to 100, no orphans). Key
mechanics RE'd from the structural dump: **(1) one long key/door chain** — mail
box→silver-key→unlock Red kitchen→`kill jim`→gold-key→unlock Lit dining→`kill
ireen`→red-key→unlock Dark closet→`kill rooth`→crystal-key→unlock Living
room→Debbie/Beth/Ross (+45); **(2) the win stair (Third floor→Attic) opens via
`kill witch`** (task 23, a no-restriction scripted task that runs anywhere on the
Third floor — the witch needn't be present), and **`kill force`** in the Attic is
the type-6 EndGame win. **(3) Real Battle System: most enemies are harmless
(`str−def≤0`) but Rooth one-shots an unarmoured player on room entry** — the
intro's "upgrade your armor" is mandatory: buy `scale male` then `plate male` at
the Third-floor shop (money from the 500-gp kills + `sell silver-key` after using
it); plate male reduces incoming damage to 0, making Rooth AND the Attic boss
safe. The "kill *name*" tasks are scripted (restr=0) so the command itself kills;
the shop's other gear/special-attacks and the Driveway monsters
(Ghost/Wolfe/Cat/Vampire) are unscored flavour. Faithful to the 3.90 Runner; no
SCARE change needed. Verified 3× identical (100/100 + "crumbles into dust" win).
