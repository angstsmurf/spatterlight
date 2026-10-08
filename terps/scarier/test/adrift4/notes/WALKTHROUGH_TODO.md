# ADRIFT 4 walkthroughs — 36 screened games still to derive

Goal: a verified, reproducible, near-maximum-score walkthrough for every
`.taf` in `test/adrift4/games/`, in the style of `Sun_Empire_walkthrough.md` —
full command list, annotated phases, and an honest note on any unreachable
points and *why*. Most of these are obscure 2000–2005 ADRIFT comp games with
no published walkthrough (checked Key & Compass, IF Archive, CASA); they were
derived by driving each game through the headless, deterministic Scarier
build and reading its internals.

This file is the **method, the standing cautions and an index**. The
session-by-session log it used to carry (2026-06-24 → 2026-09-05, ~5,700
lines) was pruned on 2026-09-19; recover any dated entry from git history:

    git log --follow -p -- test/adrift4/notes/WALKTHROUGH_TODO.md

## Where things stand (2026-09-27)

- Full suite: **670/670 PASS**, exit 0 — no FAIL, no SKIP, no NEEDGOLD, no
  NOSCRIPT. `VGM1_3.taf` (re-screened clean 2026-09-27, see *Content policy*)
  is now wired: WON 45/56 (80%), the true ceiling — the declared 56
  double-counts two mutually exclusive branches (see
  `notes/VGM1_3_walkthrough.md`, gitignored, AIF).
- **Three games joined the built-in patch table on 2026-09-27** (`PATCH_TABLE`
  in `sctafpar.cpp`; 29 games at that point, counting Melbourne Beach and the
  three further ones below — 34 by the end of the day, see below), each keeping
  its faithful row and gaining a `SCR_ASSUME_PATCHES=1` one that reaches the
  declared maximum: FunHouse 310→410 (task 10 `take money` was the third of
  three hundred-dollar tasks and had an empty room list), Goldilocks – Breaking
  & Entering 32→35 (task 46 `get egg` tested "the referenced object" its pattern
  never binds; task 53 `tie rope to bar` had no actions while its dead twin
  carried the score) and The Crime Scene 78→80 (task 0 `look at door` moved the
  player to room -2, which kills Scarier and the Runner alike, instead of just
  paying its +2). Candidates were found by sweeping all 700 `.taf` files for the
  two known-sweepable shapes — a typed, consequential task with `Where` =
  NO_ROOMS, and a restriction on a "referenced" thing no pattern binds — then
  filtered against the goldens that score short of their declared maximum.
  `TheADRIFTProject.taf` was rejected the same day: its missing ten points are
  task 69 `#Put Transmitter on Darwin`, whose Where is also NO_ROOMS, but the
  transmitter and receiver are hidden objects nothing in the file ever places,
  and the author's own `#NOTES` task still lists "Make some sort of transmitter"
  as a to-do. Placing an object the author never placed is authoring, not
  unblocking, so 90/100 stands.
- **That lead resolved, 2026-09-27 (user report: "can't turn on drier, because
  it gets rewritten as 'turn on dryer'"):** `Melbourne Beach.taf`'s missing +1
  *is* a data bug after all, and the 24th patch-table entry. The author's
  synonyms 3 and 4 are an inverse pair (`dryer`→`drier`, `drier`→`dryer`) and
  the Runner applies the table as a sequence, so both spellings reach the tasks
  as `dryer` and task 48's `turn* drier` is out of reach of any ordinary line —
  the line falls to task 56, eight slots along, same room, same restriction,
  same first action, no ChangeScore. The patch re-spells the pattern
  (`turn* dryer`) and repairs the slip in the same task's second action
  (`Var1 28`, the *dirty* clothes, → `25`, the dry ones, as 56 has) — without
  that second edit, forcing the task through the synonym gate with
  `turn driers drier` costs the fold (+5), because `wash clothes` cannot be
  repeated. The coffee triple is now settled too, and is *not* patchable: the
  red cup is hidden until the Captain trade, which requires `drink oil` done,
  which is exactly what the red-cup scoring task forbids, and the third twin's
  +1 is paid for by that task's own −1. So 39 of 41 is the real ceiling, and
  `melbourne_patched_solution.txt` reaches it. `CrimeScene.taf`'s faithful row
  is in the same family and was left faithful on purpose: typing `look at
  door` for its missing +2 kills the Runner.  (Its patch was retired
  2026-10-09 -- Scarier now survives the command unpatched, see
  `crimescene_door_solution.txt`.)
- **Three more games joined the patch table later the same day
  (2026-09-27), on the user's "add more games to the patch table":** each keeps
  its faithful row and gains a `SCR_ASSUME_PATCHES=1` one.
  - *Professor Von Witt's Fabulous Flying Machine* 154→**229/229**, the whole
    declared maximum and the win. The green button has three endings: task 52
    (not aboard), task 53 (aboard) and task 54 (aboard *and* holding the bottle
    cap), which is the flight that wins, +75 and EndGame 0. 53's one
    restriction is a subset of 54's two and 53 has the lower number, so it
    claimed every press — the author's own bundled walkthrough holds the cap
    and still gets 53. 53 gains the complement gate it lacks (cap NOT held,
    Var2 = 7), and its `RestrMask` goes `#` → `#A#`, since a mask that names
    one restriction leaves the rest unevaluated. Not one command of the
    faithful script changes.
  - *The Adventures of Space Boy! Volume I* 1009→**1039/1374**. Task 11, the
    Ice Gloves, is patterned `{take\get} {them/it/} {the} gloves` — a
    backslash where every other alternation in the game has a slash — so no
    line could match and the library take handed the gloves over unscored. One
    character; the take-family pre-match then gives `take gloves` to the task.
    The rest of the gap is not patchable: the declared 1374 is 55 more than the
    game's own ChangeScore pool, the cape's +250 drop is a command the 4.0
    library answers without asking the tasks, and the two +30 transporter
    buttons are exclusive with `enter transporter` (+50).
  - *The Night The Moon Shone Grey* 360→**380/400**. Task 17 `behead drow`
    wants the elf's body lying in the Library *and* "the referenced object"
    held, and every `behead` spelling binds that reference to the body itself,
    so it asked for the corpse to be carried and on the floor at once ("You do
    not have dead dark elf.", run390 too). The second restriction becomes
    Var1 = 1, "any object" held — the blade in hand its own
    `behead dark elf with %object%` command and "You do not have %object%."
    were written for. The patched row adds the single command `behead drow`.
    The game's other missing +20 is **left alone**: task 12 is the wolf's
    KilledTask, the wolf starts hidden, and the only thing that would place it
    is task 4, which the "Baying at the moon" event forgets to run
    (TaskAffected 0) and which would drop it in one unpredictable room on one
    turn with no restart. Pointing the event at it would scatter a point, not
    unblock one.
- **Five more games joined the patch table the same day (2026-09-27), on the
  user's "add patches for the suitable candidate games":** the shortlist in
  `UNREACHABLE.md` (now `INCOMPLETE.md`) was re-measured game by game and five of its entries turned
  out to be one-edit author slips with the repair already pinned down. The
  table now stands at **34 games, 207 edits** (`harness/patchtable.py` counts
  it). Each keeps its faithful row and gains a `SCR_ASSUME_PATCHES=1` one, and
  four of the five patched rows are the faithful script command for command —
  the data was the only thing in the way.
  - *The Twilight* 485→**500/500**. Task 59's single 3.8 object-state
    restriction reads "object 57 inside container 7": container 7 is the gas
    stove and object 57 is the stove as well, so the author named the holder
    twice and never named the moldy cheese sitting immediately before it in the
    object list. A static object is inside nothing, so `cook cheese` answered
    "You can't do that yet." in run380 too, and the game's own T113 tally said
    485. The conversion already lands the test on the cheese (a static's index
    falls back to the preceding dynamic object,
    `parse_fixup_v380_objstate_restr`), so the repair is the holder the author
    meant to leave in place: `Restrictions/0/Var3` 0 → 7. The faithful script
    already types `put cheese in stove` then `cook cheese`, and the largest
    single score in the game is the +15 it was refusing.
  - *House Of Horror* 145→**155/155**. Tasks 101–109 tally the nine treasures,
    ten points each, one restriction each: "this treasure is in room 36", *Home
    Free!*. Eight say 36; task 109, the bag of doubloons, says 0 — and a zero
    in that field is not a room but the object-location restriction's "is
    hidden" test (`restr_object_in_place`). The doubloons start hidden and the
    only thing that places them is the blunderbuss shot that kills the zombie,
    so as shipped they could be carried home or counted, never both, and either
    trade capped the game at 145. `Restrictions/0/Var3` 0 → 36 and 109 asks
    what its eight siblings ask.
  - *Sun Empire: Quest for the Founders (Part I)* 140→**145/145**. Task 59's
    mask `#A#A(#A#)` asks for Skynd alive in the room **and** his corpse on the
    floor; its twin task 58 for Skyrv writes the same pair with OR and can be
    sampled either side of the killing. One character — the inner operator
    becomes the OR — and the sample can be pressed while the Orgaan is still
    standing. Skynd is worth ten: task 9 `#Death of Skynd` pays the other five,
    and as shipped only the death was reachable. The patched row adds two
    commands, `get sample from skynd` and one `z`: the extra turn shifts the
    battle stream, and without the wait Malthew never lands the killing blow
    inside the route, so the sample's +5 merely replaces the death's (swept
    0–12 waits on `SCR_SEED=10`; 1, 4, 5, 6 and 9 all bank both).
  - *Terrified* 60→**65/65**. Task 89 is the +5 for climbing the compound
    fence — no command a player types ("- score for getting over fence"), no
    restrictions, one ChangeScore — and is executed by each of the four tasks
    that cross the fence (gloves, rag, shirt, trousers). Every one of them
    moves the player west out of room 19 *first*, and 89's own Where is that
    one room, so it could never run; nothing else in the file executes it.
    `Where/Type` 1 → 3 (ALL_ROOMS), which is what a scoring task called by
    another task wants, and its four callers stay the only things that run it.
  - *Sentor* 12→**13/13**. Task 2 wakes Stefcho the talking skull and is
    patterned `slap Stefcho`, but 3.90 rewrites `slap` to `hit` before any task
    is matched — one of the Runner's own built-ins (`BUILTIN[]` in
    `scprintf.cpp`), not a synonym of this game's, which has none at all — so
    the line reached the battle library instead ("You hit the skull, but it
    doesn't seem to do any damage.") and the game ended "You finished 1 points
    short." The pattern is re-spelt `hit Stefcho`, what the rewrite leaves
    behind; a task that matches the line is dispatched ahead of the battle
    library. Same shape as Melbourne Beach, Crime Scene 2 and The Fugitive.
    *Patch removed 2026-09-30:* the `spelling_task` deviation lets the
    author's `slap Stefcho` reach the task, so the faithful row scores 13/13
    without it.
- **A sixth joined on 2026-09-27, *The Studio* 96→100/100**, taking the table
  to **35 games, 212 edits**. Twenty-seven ChangeScore tasks sum to the declared
  100, and the four points of the Bedroom shoot's opening act were out of reach.
  Task 110 is the Bedroom's staging of a scene the Main Studio already has as
  task 73 — the same command pattern, thirty-seven slots earlier, locked to room
  0 — so 73 claims the line and, out of its room, answers "You can't do that
  here!". 110 *is* still reached on the fall-through; what stops it there is its
  own first restriction, a character test addressing Var1 = 0, "the Player",
  against Var3 = 0, "the Player": the Player must be in the same room as the
  Player. 3.90's `passrest` leaves that exit with the result still zero and
  without copying the FailMessage ("What?"), so the refusal is silent, the
  fall-through lands back on 73's message, and the points are gone. SCARE
  reproduces that exit deliberately (`restr_pass_task_char`), so 96 is the
  faithful ceiling — `runner_transcripts/studio.txt` pins it. Var1 = 2 is
  Shelby, and that is what the restriction was meant to say: tasks 111 and 112,
  the two acts that follow in the same shoot, both carry Var1 = 2 against
  Var3 = 0 behind the identical "What?". `Restrictions/0/Var1` 0 → 2 is the
  whole repair, and the patched row is the faithful script **byte for byte** —
  the line was already on the turn it belongs to and simply stops being refused.
  Two entries from the same shortlist were re-measured and **rejected**: *The
  X-Files* (its missing 3 were a route bug in the walkthrough, not the data —
  the stray `n` after the van's journey event, now fixed, 299/299 faithful) and
  *NAT_01* (осмотреть and рассмотреть both contain "см", so its `*см*` task
  patterns were always reachable and the committed route already scores the
  maximum). *Marooned* was re-read too: T24 is a duplicate of T14, which
  already pays the same +10 for the same act, so those ten are phantom maximum
  rather than blocked score and patching them would pay twice.
- **A seventh joined the same day, *The Long Journey Home* — a second, female
  career opened from 20 to 30/90**, taking the table to **36 games, 230 edits**.
  The 90 is two careers: the first move is `male` or `female`, and each scoring
  act exists twice, the female twin always at the lower task index. Rage's valve
  is the exception. Its female twin, task 24 `#12 release pressure (f)`, is
  `Where/Type` 0 (NO_ROOMS — `task_where_allows_run()` returns a flat FALSE, so
  nothing can ever run it) and its only `Command/0` is that author's label, not a
  phrasing a player could type; the male twin 25 carries the four real ones
  (`* turn * valve *`, `open`, `release`, `use`). Three slots above both sits
  task 22, the unrestricted debris refusal, which repeats all four patterns and
  therefore claims every spelling of the line for *both* genders once the debris
  is gone — so even the male +10 was unreachable. `PATCH_JOURN2` gives 24 a room
  (9, Rage) and the four commands, and hangs 22's one missing restriction on it:
  type 2, Var1 = 19, Var2 = 1 — "task 18 (`#12 remove debris`) must NOT be
  done" — with an empty FailMessage, so the refusal goes quiet and the forward
  scan falls through to the twin the way the author's own T18/T19 and T20/T21
  pairs already do. The female row needs `SCR_ASSUME_REPEATS=1` as well: the
  Lair is still walled by the spent T3 catch-all (T4/T5 claim all 20 direction
  words the same way, so that half is not data-fixable). Ceiling for a female
  career is 30 either way — T69 +5, T11 +10, T12 +5, T24 +10, three Queens in
  hand — because Rage's exit gate names the *male* twin, so she ends in the room
  she just scored in. Left alone deliberately: an ADRIFT exit can name only one
  task, so opening that door for her would shut it on him. Not derived: the same
  T22 repair unblocks the male T25, which would take a male career to 40 and
  still walk out.
- **Score-shortfall sweep, 2026-09-27.** Every `goldens/*_solution.expected.txt`
  was re-read for its closing tally and the 83 rows that end below their declared
  maximum were triaged against their manifest comments. All but one are accounted
  for as phantom maxima (a declared `MaxScore` above the sum of the file's own
  ChangeScore actions, or a sum over mutually exclusive endings), as documented
  author bugs (see `INCOMPLETE.md`), as the faithful half of a patched pair, or
  as a deliberate content-policy omission. The one genuine route gap was
  **Heist**, 7→**8/8**: the author's intro dares the player to find a "secret
  thing … not mentioned in the walkthroughs", which is T4 `buy checker mix`, and
  the old manifest comment wrongly called it exclusive with the candy bar. T0 and
  T4 both test *money >= 2* (`RESTR type=4 v1=2 v2=3 v3=2`) and neither spends
  the variable, so both buys land. The mix sits on the back-room rack and needs
  `x rack` first: until the rack is examined its contents are out of the take
  resolver's reach and both `take checker mix` and the alias `take mix` answer
  "Take what?". Route now ends "Well done - you scored maximum points!".
  Still short, but now fully accounted for, is *British Fox*, raised from 43/50
  to **46/50** on 2026-09-27: `ring bell` swapped in place of the no-op basement
  `attack guard` banks T333 (+1), and playing the opening Welsh Fox scene out to
  its end instead of breaking it off banks T52 "# Beverley cums" (+2), paid for
  by deleting 13 zero-point HQ flavour lines so the route stays turn-for-turn
  neutral (its
  later damage rolls are RNG-stream sensitive: measured safe draw offsets
  0,+1..+4,+7,+8; -1,+5,+6 all end in recapture). The remaining 4 are branch
  facts, not route bugs: T460 (+1) and T189 (+2) are structurally excluded once
  she is captured, Grace's arrest is the other, mutually exclusive branch, and
  T564 (+1) would need the whole RNG-sensitive gauntlet re-derived.
  **Superseded 2026-10-09: British Fox is now 50/50.** That "branch facts"
  reading was wrong — XavierHawkUk's walkthrough shows capture and Grace's arrest
  are NOT exclusive. The middle of the route was re-derived as a no-alarm run
  (attack a dungeon visitor with the collar off, `ring bell`, subdue Grace, let
  Eugene finish with her, unlock the cells and outer doors from the computer);
  with the alarm never raised there is no combat left and so no RNG-sensitive
  gauntlet. See the row's entry in `v4_walkthrough_rows.md`.
- **2026-09-27 footgun hit while adding the row:** the `Edit` tool round-trips
  `run_v4_walkthroughs.sh` as UTF-8, but the file is cp1251 — this silently
  mangled every pre-existing non-ASCII byte run elsewhere in the file (7
  unrelated Russian-game win markers) into U+FFFD on the first attempt,
  surfacing as spurious `REGRESSIONS:` on govard/govard2/dolg/nat01/relife/
  zanoza/proba. Fixed by `git checkout --` the file back to pristine and
  re-inserting the new row with raw byte splicing (Python `rb`/`wb`, no
  encoding), verified with `git diff` showing only the intended 5-line
  insertion. **Never use `Edit` (or any UTF-8-text tool) on this file again —
  splice bytes directly.**
- **Open work: 2 unwired-but-screened games**, both pending an owner call:
  `ghostjustice.taf`, `bluesky.taf`. Of the remaining `.taf` files without a
  row, 85 are content declines (83 earlier + 2 from 2026-09-26:
  `latework.taf`, `Legend of Akhbar.taf` — see *Content policy*).

  None has a source walkthrough in `downloaded/`, so every route is derived
  from scratch.
- `games/Older.zip` (268 entries) is not in the manifest and has no row. It is
  an archive, not a game.
- Every walkthrough in `downloaded/` has a game and a row.
- Every partial checkpoint has been finished (Ghost town, YADFA, Uncle
  Grumble, Lights Camera Action, Cowboy Blues, mould, blood, House, magicshow
  — all closed 2026-09-04/05).
- Three rows walled deliberately on 2026-09-13 when the pre-4.0 spent-task
  claim was ported (`run_spent_task_390()`): journ2 5/90, vampire 70/100,
  merry_murders 120/135. Those are Runner-true, not regressions.

```
test/adrift4/harness/run_v4_walkthroughs.sh          # the whole suite
test/adrift4/harness/run_v4_walkthroughs.sh <regex>  # one row
test/adrift4/harness/run_v4_walkthroughs.sh -v foo   # diff one row
test/adrift4/harness/run_v4_walkthroughs.sh --bless  # re-record goldens
```

Row count: `grep -cE "^[A-Za-z0-9_.,' -]+_solution[A-Za-z0-9_]*\.txt\|" harness/run_v4_walkthroughs.sh`.
Unwired list: set difference of the second field of every non-comment row in
`harness/run_v4_walkthroughs.sh` against the first column of
`games.manifest.tsv` (`comm -23`; use `find -print0 | xargs -0`, unique
basenames — `ALEXIS.TAF` and `LOST.TAF` have more than one row).

## Where everything is

| What | Where |
|---|---|
| The manifest: one row per game, `solution\|game\|win-marker\|env`, with a comment carrying the verdict | top of `harness/run_v4_walkthroughs.sh` |
| Solutions and goldens | `goldens/<name>_solution.txt`, `goldens/<name>_solution.expected.txt` |
| Per-game write-ups (206 exist) | `notes/<Game>_walkthrough.md` |
| Games whose *only* write-up is this file | the *Game index* below |
| Runner ground-truth probes this work opened | `../../../RUNNER_TESTS_TODO.md` (§2 events, §4/§5 refusals, §6 3.70, §8 restart) |
| Wine Runner transcripts and ported rules | `WINE-TRANSCRIPTS-TODO.md` |
| Waitkey audit | `python3 harness/waitkey_audit.py` |
| Source walkthroughs, FAQs, score sheets | `downloaded/` (`CampWindyLake2_walkthrough/_faq`, `LaraCroft_SunObelisk_walkthrough/_faq`, `DrWho_VortexOfLust_scoresheet`, `GammaGals_scoresheet/_readme`, `SilkNoil_walkthrough`, `ShadricksTravels_walkthrough`, …); unpacked archives in `~/Downloads/More Adrift games/` |
| Corpus pins | sha256 in `games.manifest.tsv`, `test/fetch_games.sh`, `test/GAMES.md` |
| Harness binary | `harness/scare`, built by `build.sh` (gitignored) |
| Helpers | `harness/play.sh`, `safeplay.sh` (12s CPU / 4MB cap), `scproj_regress.sh`, `run_autosave_tests.py`, `taf_pattern_scan.py` (`plaintext()` de-obfuscates both TAF generations), `taftool.py`, `make_400_whereprobe.py`, `make_39_whereprobe.py`, `make_39_evseeprobe.py`, `make38probe.py`, `make_badparent_taf.py` + `badparent_test.cpp` |
| The full derivation log | git history of this file |

Four artefacts per game: commented `goldens/<name>_solution.txt`, blessed
`.expected.txt`, a commented manifest row, and (where the game is not AIF)
`notes/<Game>_walkthrough.md`. For AIF adults the row alone is committed and
the solution, golden and notes are gitignored (*Diary of a Stripper* terms).

## Per-game workflow

1. **Boot and look.** `harness/play.sh GAME` — read the title screens; note
   name/gender prompts and any `<waitkey>` (`SCR_MARK_WAITKEY=1`). Always run
   the mark pass before writing line 1 of a solution.
2. **Dump the structure.** `SCR_DEBUGGER_ENABLED=1` (`debug`, `tasks 0 N`,
   `rooms 0 N`, `objects 0 N`, `npcs 0 N`, `events 0 N` — a range is
   required) and `SCR_DUMP_TASKS=1` (stderr, at the first prompt):
   `printf '\n\n\nquit\ny\n' | SCR_DUMP_TASKS=1 ./scare ../games/X.taf 2>/tmp/x.err >/dev/null`.
   Read the `EXIT room=N DIR -> dest=M` block *first* — it is the map (mould's
   act 2 is not guessable from prose).
3. **Scoring map.** Every `ACT type=4` with its delta, negatives included:
   `awk '/^TASK /{n=$2} /^    ACT type=4 v1=/{split($0,a,"v1="); v=a[2]+0; print n"\t"v}' tasks.err | sort -n`.
   Author-variable scores (PK Girl, Three Monkeys, LCA `var1 [scor]`) count
   `ACT type=3` instead. Then grep `HINTQ=`/`HINT1=`/`HINT2=` — an author hint
   menu often names the literal command (Veteran Knowledge, Lost Tomb; not
   Great Falls). Never type `hint` in a script: the `[Y/N]` echo desyncs it.
   3b. **Check the game's own zip and `SYNONYM` table** before deriving — a
   bundled `*walk*.txt`/`.sol`/readme is a free oracle (Silk Noil), and a
   synonym rewrite changes what the matcher sees (croft, relojero, Vardock).
4. **Play to win.** NPC turns are deterministic under the harness seed.
5. **Push to the ceiling.** Sum fired `ACT type=4` from `SCR_TRACE_TASKS=1`
   (`running task N forwards`, *with multiplicity*) against the map; prove
   every missing award unreachable or take it.
6. **Diagnose** with `SCR_TRACE_TASKS=1` / `SCR_TRACE_FLAGS=256` (+8 parser;
   bracket expr `#A#A(#O#)`), `SCR_TRACE_MATCH=1`, `SCR_TRACE_EVENTS=1`,
   `SCR_TRACE_VARS=all` (hidden vars — datewithdeath `ritual`),
   `SCR_DUMP_OBJLOC=1` (`sw=`, `PLAYERLIMITS`), `SCR_DUMP_BATTLE=1`.
7. **Attribute honestly.** `screstrs.cpp` vs the Runner's
   `evaluaterestrictions` (`Sub_20_57` in `~/Desktop/run400.txt`); prefer
   playing run400/run390 under Wine (`~/adrift-battle/runner/wine/`). Positive
   control on the *same* TAF version — 3.9 `circus.taf` has 24 `ACT type=6`,
   inverness none.
8. **Verify three identical runs**, write the notes, bless the golden, add
   the row, run the whole suite.

## Standing cautions

- **A session transcript is Runner truth; a hand-written command list is a
  route for a human.** The gap is usually a prompt, a timer, or a paraphrased
  verb (Camp Windy Lake 2). ClubFloyd logs are *stock SCARE* ("Welcome to the
  Cheap Glk Implementation"), not an oracle (The Cellar).
- **Decode every event's `o2`/`o3` as raw−1 before declaring anything
  orphaned.** Revised verdicts: WesGHN 30/100 → **WON 100/100** (the gold
  ring is never orphaned); Mr Smith and Villains & Kings (3.9 battle is
  version-gated, `battle_legacy`); Plague Redux UNFINISHABLE → **WINNABLE**
  via the `where=1` pole-bypass tasks once Where Type 0 was settled against
  run400.
- **Hint menus, objective lists and flavour text lie.** Trust the trace and
  `score`. Room descriptions lie about exits (Hangover); trust "You can't go
  in that direction, but you can move …".
- **Check for a sibling build before writing "unreachable".** Azra's 4.00
  file is an editor upconversion with every battle attribute degenerate
  (`Lo == Hi`, acc/agi/recovery 0 — the upgraded-3.9 fingerprint); the 3.90
  original plays through all six goals.
- **"No `ACT type=6`" does not mean "no ending".** Look for a *terminal room*
  reached only by `ACT type=1 v1=0 v2=0` (YADFA room 13, CowboyBlues room 35,
  mould room 103, LCA room 22) or a stop chain (blood TASK 724→5→41,
  `ACT type=6 v1=3`). Histogram destinations sorted by *room*, not frequency,
  and never `head` it — the once-referenced terminal room is the row you drop.
- **Never anchor a win marker on the score summary when MaxScore is 0**
  (WonderWombat prints "100% of the game" on a corpse) or when the declared
  maximum is exceedable/approximate (YADFA 243/231). Prefer a closing-text
  line. Markers must not straddle a wrapped line (`grep -F` is line-oriented)
  and follow the manifest's no-apostrophe rule.
- **Establish whether failed probes were turns before deleting them.** A
  parser-rejected command ("That is not an option or command.") costs no turn
  and can be deleted freely (Azra: 49 deleted, 505 turns unchanged;
  substituting `z` gave 756 and desynced every NPC). But a route with a
  ticking meter or fixed clock is length-sensitive (WonderWombat alcohol;
  Ghost town day/night — removing one duplicated `open door 101` moved the
  `sleep`s before nightfall). Rebuild from `head -N` of the blessed file, never
  `sed -i '/pattern/d'` (deleted the cellar `light lamp` too).
- **Never trust an agent's self-report** (ONNAFA "completion" was fake). Re-run
  the literal `harness/scare` / `--bless`, check solution file sizes, diff.
- **Keep measured divergences in the script.** `crime_adventure` keeps its
  six refusals because the 90-command file is byte-identical to
  `cmdfile_w_crime.txt`, the run380 replay (`Adven_1_crime.rtf`); do not tidy
  them away.
- **Regressions that are not yours:** a peer engine session may leave stale
  goldens (`shadowpeak*`, `thetest_win`, `great_escape`, `textident_evil`,
  `merry_murders`, `provenance`, `thehunter`, `super_liam`, `wrecked`,
  `james_bond`, `life_of_mike`). A `REGRESSIONS:` set that needs only
  `--bless` is bookkeeping; one that needs a route change (crime_adventure
  under the pre-3.9 take gate) is a finding.

## Footguns

- Rebuild after every engine change: `./build.sh`. `git checkout` any
  instrumentation left in `scbattle.cpp`/`sctasks.cpp`.
- `Edit` on `run_v4_walkthroughs.sh` drops `+x` → `chmod +x`.
- `transcript()` does a bare `cat "$2"`, so a `#` header line in a solution
  file *is* typed — the engine skips it (`os_ansi.cpp:286`; the pause read
  honours `#` too since 2026-08-16). `drive_ckpt_safe.sh` line 128 skips `#`
  lines as well, so a `#`-labelled task cannot be typed through either harness.
- A stray trailing blank line is a turn (Picture). `cbn`/`cbn2` leading blanks
  are *real* empty-command turns (TASK 38) — do not delete every blank.
- `--bless <substring>` also matches `microbe_willie` when blessing `will`.
- Uppercase `.TAF` is missed by a case-sensitive glob; `Older.zip` is not a
  game.
- `command grep`, not `grep` — the ugrep wrapper returns zero matches on some
  patterns. zsh `grep -c` on an empty string misbehaves: write to a file and
  `grep -q`. Goldens contain extended-ASCII NEL: `export LC_ALL=C`, `grep -a`.
- `grep -c waitkey` on a `SCR_DUMP_TASKS` capture is 0 — the dump is stderr,
  the tags are in the transcript; use `SCR_MARK_WAITKEY=1`.
- Debugger EOF-loops: `perl -e 'alarm 8; exec @ARGV' env SCR_DEBUGGER_ENABLED=1 ./scare GAME`, never `timeout(1)`.
- `WaitTurns` check: `printf 'z\nquit\ny\n' | SCR_TRACE_EVENTS=1 ./scare GAME 2>&1 >/dev/null | grep -ac '^Event: ticking event 0:'`.
  `Globals.WaitTurns`=3 in Cursed, Vampire, Captive — `z` is three turns; when
  event timings look ~3× too fast read this before suspecting the engine
  (`sclibrar.cpp:2347`, `scrunner.cpp:2415`).
- `OBJNAME … prefix=[…] alias=[…]` before believing "I see no such thing".
- Two `ACT type=5` follow-up texts in a row can each swallow the next fed
  command (CowboyBlues sheriff / Coral warning; House `wind clock hands back`;
  magicshow Dinosaur Room needs a second `down`). Budget a throwaway turn.
- SCARE's "(Press a key)" eats a stdin line — a blank in the solution is the
  bridge (light_up / mould); the Runner runs a typed line there.
- Remote `.taf` triage: `Range: bytes=0-13` classifies by header, but adrift.co
  sometimes answers ranged GETs with an empty body — retry unranged; percent-
  encode spaces.
- Failed probes are deletable only if parser-rejected (Azra), not if a meter
  ticks on turn count (WonderWombat).
- Runner under Wine: the first scripted command after launch is routinely
  lost (pad with two `look`s); the echo is the only trustworthy record of what
  the Runner received.

## Content policy

Criterion: **no minors**, not "no non-consent" (BSG22 / magicshow precedent).
A narration-depicted minor counts even if the player cannot participate
(aparty); a twist reveal does not exempt (Choices, plains); a protective
refusal of a child NPC is clean (suzy). Dark non-sexual themes wire normally
(thelasthour, ForestHouse3, Patient7, A View to a Home).

**14 permanent declines** — never committed, no `.gitignore` entry, each
verdict quoted from the game's own shipped text: `aparty.taf`, `delight.taf`,
`awakening.taf`, `enc1.taf`, `enc2.taf`, `windy.taf`,
`Buffy Before the Date.taf`, `ssteacher.taf`, `sibling seduction.taf`,
`Choices.taf`, `plains.taf`, `A Dream Come True.taf`, `latework.taf`,
`Legend of Akhbar.taf` (the last 2 added 2026-09-26; full verdicts below
under *2026-09-26 declines*).

**Rows kept despite the list above (owner call, 2026-09-25):** `Hunting Ground.taf`
(row `huntingground`), `British.Fox.and.the.Celebrity.Abductions.taf` (`britishfox`,
43/50) and `fantasyworld.taf` (NOSEX route, 0/500) stay wired, with the solution,
golden and `runner_transcripts/` gitignored like the AIF rows below. britishfox was
already ignored; the other two were untracked with `git rm --cached`
(fantasyworld's files had already been pushed, so they remain in history).

`BeThere.taf` is excluded as
an ADRIFT 5 duplicate (deleted from `games/`).  Downloaded walkthroughs that
were left unwired because the TAF itself names under-21 characters:
`enc4.taf` (ex-girlfriend Tammi "a year younger than you", explicit sexual
content), `Eva's secret.taf` (Eva's own diary: sexual activity "began 4 years
ago when she was 15"),
`midsomer bottom manor ver1.8.taf` (Maisie 14, Caroline 16, Leslie 17),
`the_burbs.taf` (Jimmy and Jen are 14-year-old twins).

`sororityHouse.taf` was re-reviewed 2026-09-24: no character anywhere in the
TAF text is given an age or grade below college (Ginger has just graduated,
Jenni is "a junior at the local university", Holli/Kaytie are freshman
sorority sisters); the earlier "little girl" note was a sarcastic aside, not
an age claim. Moved to the AIF-between-adults list below.

**AIF between adults** — row committed, solution/golden(/notes) gitignored:
Archie's Birthday, Diary of a Stripper, windy2, croft, dr-who-vortex-lust,
gamma, Temple_Of_The_Sun, amy, The_Strange_Tale_of_Dr_Wilkins, BSG TWENTY TWO
Final, warlock, BarneysProblem, Dear Diary, Dear Diary 2, Riding_Home, hcw,
Scandal, cldone, magicshow, goblin, ss whore, Sex is Mental, The Worst Game In
The World, DOA_X_B_S, The Silver Maiden, Trapped With A Girl, Practice Policy,
To Be King, Harem Prologue, Duchess of Desire, Sorority House, Filthy Bill,
EscapePod, Handyman, xclue1.0a, ovaloffice, Planescape-Encounters1,
RodneyandthePrincess40v3-1, salvation, christmas present 1.0, studio, fun town
(wired 2026-09-26); The GameMaster: Resident Lust (`VGM1_3.taf`, wired
2026-09-27 after its 2026-09-27 re-screen below).

Further GAMES_WITH_HINTS declines (TAF text): `Deadly Climax 1.0 final.taf`
(Asia a "fifteen year old pupil"; Jo compared to thirteen); `party.taf`
(Becky 17, Amber 19, Karen/Kelli 16); `DC.taf` (player is a "15 year old
boy"); `score.taf` (player is "a teenager"); `zara.taf` (Claire "the teenage
daughter"); `lauren.taf` ("teenage boy, nearly 18"); `Birthday.taf` (school
friends / Junior); `practice-procedures-1.5_4.0.taf` (schoolgirl Amy);
`Fairy Tails Remixed v1.5.taf` (Muffet "little child"); `Pay Back.taf`
(Clara 18); `Unexpected Proposal.taf` (Lucy is 17); Crossworlds 0/1/3
(Janey sixteen); `options.taf` (Melissa 16);
`consequences.taf` (girl "can't be more than eighteen"); `Big Stuff.taf`
("dream schoolgirl"); `Janey's Diary.taf` / `Sleep Over.taf` (sixteen).
`windy.taf` / `windy-2005.taf` stay declined as Camp Windy Lake.
`pta5.taf` (Nikki “naughty schoolgirl”; Ellie “barely seventeen”);
`The Search.taf` (Sharon a “teenager” in sexual scenes);
`LastWeek.taf` (Kirsten section: “teenagers tend to be happier…” in a sexual AIF).
`Hotown1.taf` (Linda “a young lady of about 18” in sexual scenes).
`power play 1.3c.taf` (Joseph’s daughter “a pretty seventeen year old” / “innocent teen” in sexual scenes).
`relatives.taf` (Felicia is 17, “just the same as you”; Heather is 18, in sexual scenes).
`Hidden Assets.taf` (self-declared "underage" 16yo).
`Kissing.taf` (16yo cousin, explicit sexual content; also 15yo "Freshman" content).
`Paradise_Hotel.taf` (16yo daughter, extensive explicit content).
`Plan69.taf` (explicit schoolgirl/student sexual scene).
`Virgin.taf` (Felicia 16, Heather 15, sexual content, despite in-game 18+ disclaimer).
`fantasyworld.taf` (Carrie explicitly 16, explicit sexual content; also a 10-11yo child killed in the narrative).
`graduation.taf` (Kiko, schoolgirl-coded despite ambiguous stated age — same troubling-framing pattern as Hotown1/Pay Back).
`normvillehigh.taf` (Samantha stated 18 but framed throughout as a current high-school senior — same pattern).
`school plan 1.2.taf` (Jenny and Jilly both explicitly "sixteen year old", sexualized descriptions).
`darkfantasy.taf` (the captive is repeatedly called "a young girl", "girlish torso", "young ass" through explicit BDSM content; no adult age is ever stated for her — only the player disclaimer says "you should only proceed if you are an adult").
`Private Teacher.taf` (player is explicitly "a young kid" "called back after school" by their teacher, with homework/"skipping school"/"your parents" framing and no adult age stated for the player, followed by explicit sexual content).
`ronweasley4.taf` (Luna Lovegood is textually anchored a "sixth year" while every other named character is explicitly "seventh year" and turning 18 this week — a minor by the game's own internal chronology — with reachable explicit content keyed to her character; the file is disqualified regardless of whether a given walkthrough route avoids triggering it).

**2026-09-25 screen of the 68 unscreened files from the 2026-09-22 pin
(`ee50c8de9`) and `Relife.taf`** — 30 declines, each quote verified verbatim
against `taf_pattern_scan.py plaintext()`:
`mount.taf` (Rio "a teenaged Japanese 'bikini idol'", no adult age, "perfect
teenage breasts" in a sex action; Sydney's schoolgirl outfit alone would have
been fine — adult model in costume);
`Turnberry Manor.taf` (intro: "girls as young as 16 years old"; Sara "a bubbly
sixteen year old girl");
`decisions.taf` (*Choices* sequel; player Melissa "a seventeen year old
lesbian" fixated on her teacher);
`lasthurrah.taf` (Teagan/Riley "teenaged Australian tourist[s]", then "The
girls back at high school won't believe this!");
`SBFT.taf` (player transformed to "a high school sophomore" body; one ending a
"twelve-year-old");
`bad day 2.1.taf` (bus scene: schoolgirl "probably about fourteen" climaxing;
Jamie "fifteen or sixteen");
`community.taf` (Amy "I'm fifteen Mister", "our local jailbait", exposed and
groped);
`gross.taf` (player and classmates at "Bialik High School"; `feel alissa's ass`,
`jerk off`);
`1st_time.taf` ("as a teenage boy"; "most of the guys at school");
`abduction.taf` (Samantha "a senior student at the high school" in a peeping
scene; Rebecca "a picture of high school contradictions");
`switchedit.taf` ("Little Johnny … Only 13" joins the abuse of the captive);
`Home alone.taf` ("Jill, she is your sister, she is only 16");
`Getting Even.taf` (player "an average sixteen year old girl");
`santababy.taf` (Heather "looks about 16-years old … in pigtails");
`oakwood.taf` (teacher at "Oakwood Sixth Form for Girls", students sexualised);
`truck.taf` (menu: "If you'd like girl less than 16, type 1");
`stowaway.taf` ("I am, after all, still in high school");
`Time.taf` ("your sister, seventeen years old, naked");
`lessons (part 1–4).taf` (Jack "a 18 year old student at Thomson High" — the
normvillehigh pattern; all four parts share Jack and Miss Jones);
`xmen.taf` ("even at 17 years old Jubilee");
`x-men.taf` ("Jean Grey, age 17"; Rogue 16; Kitty and Jubilee 15);
`cabin.taf` (Rachel "about sixteen years old, seventeen perhaps, but she could
be younger");
`train1.taf` (Emma "about 17 years old", "the helpless schoolgirl");
`SilverWolf.taf` (SacredMoon "a cute, sixteen-year old teenager");
`enc3.taf` ("You're a 15-year old boy" — same series as enc1/enc2/enc4);
`casino.taf` (maid "a young woman no older than 19"; Kelly "you wonder if she's
old enough to be in the casino" — the consequences/Hotown1 around-18 pattern);
`Drone Academy.taf` (partner Kadey has no stated age; "watch this young girl
masturbating", "those little teenie bopper whores" — the darkfantasy pattern).

**2026-09-26 declines, surfaced during full derivation of the "13 AIF between
adults" batch** (both had cleared the earlier pre-screening pass on a
narrower check; live play / the full task dump surfaced content the
pre-screen missed — same verbatim-quote standard as above.  A third game
declined that day, `VGM1_3.taf`, was re-screened on 2026-09-27 and is *not* a
decline; see the note after the two verdicts):
`latework.taf` — the magazine found in the desk drawer is the game's own
gating item (the walkthrough cannot route around it: reading it, then
paging through it, is what starts the only plot line), and the page the
player is made to turn to reads "There is an image of a girl and a boy, they
do not look much older than 15. They are standing on the floor and the boy
is pounding the girls ass." That is sexual content depicting minors in the
shipped text, on the criterion above. Permanent decline, non-negotiable
regardless of any future instruction to include it. (The original verdict
also cited the endgame's forced-act branch. That is a non-consent finding,
which is *not* a ground under the stated criterion — see the re-screen note
below — so the decline rests on the minors finding alone, which is
sufficient by itself.)
`Legend of Akhbar.taf` — the task dump's harem area (ROOM 14/19) contains an
NPC named literally "Harem girl" and room-description text that calls the
same background characters "young girls giggling" in one sentence and
"young women" in the next, with no explicit adult age ever stated for
either reading. An initial relayed "include it" decision (favoring the
"young women" reading to resolve the contradiction) was overridden: picking
the charitable reading to route around an unresolved age-adjacent signal is
exactly the kind of judgment call this policy exists to keep out of the
walkthrough-derivation loop. Permanent decline.

**2026-09-27 re-screen: `VGM1_3.taf` is not a decline.** Its 2026-09-26
verdict ("The GameMaster: Resident Lust" — the Jill sequence is revealed
after the fact to have happened while she was involuntarily incapacitated by
a third party's power, and it is not skippable) rested *entirely* on
non-consent, which the criterion at the head of this section explicitly
disclaims. A full age scan of the game's 91,530 characters of plaintext
(4.0 zlib body, via `harness/taf_pattern_scan.py`) finds no minor: the only
hits are "some minor modifications" on a game controller, "A twelve inch
serrated blade", "This here is Company Thirteen" and "for what seems like
ages" — no school, teen, child, student or daughter text anywhere. It is
therefore screened clean, like ghostjustice and bluesky. Derived and wired
2026-09-27: WON 45/56 (80%), see `notes/VGM1_3_walkthrough.md`. The same scan
re-verified latework's quote above verbatim, so that decline stands.

Two lessons from the pair: a verdict must name which stated ground it rests
on, and a decline resting on more than one ground must survive the removal of
any single one. `Hunting Ground.taf` is the control case — it is kept wired
(owner call, 2026-09-25) on exactly the ground VGM1_3 was refused for.

**2026-09-27: `ghostjustice.taf` owner call resolved, confirmed directly (not
just relayed) — clean, finalized as wired.** Candi is judged "around 21 or 22"
by the narrator; her "little girl routine" during the scene ("Her words and
mannerisms might be those of a little girl") is age-play/kink framing on a
stated adult, not a claim that she is one. Everyone else is adult (Susan "20
years old"). Derived and wired 2026-09-27: **WON, 100/100 (100%), in 182
commands**, deterministic across 3 runs — see
`notes/ghostjustice_walkthrough.md` (gitignored, AIF). Margaret's climax
branches into "kill her" vs. "torture her"; the route takes "kill her" for
content-policy reasons (routing around graphic non-consensual content),
proven zero-cost against the full score census (the torture branch, TASK228–
276, carries no `ACT type=4` markers at all).

`bluesky.taf` — no age is stated anywhere; the player stays at home on a summer
job while "Your family went on holiday", a neighbour calls him "son", and Maria
"is about your own age". Nothing frames a minor, but nothing establishes an
adult either. **2026-09-27: confirmed directly — proceed, treated the same as
every other "no signal either way" AIF game in this corpus.** Derived and
wired 2026-09-27: **WIN, in 74 commands**, deterministic across 3 runs — see
`notes/bluesky_walkthrough.md` (gitignored, AIF). The game declares a maximum
score of 0 (no `ACT type=4` anywhere in the file); the win marker is the
game's WINTEXT header, `You have finished Blue Sky (v. 0.5)`.

**Screened AIF between adults — all wired 2026-09-26** except 3 that turned
out on full derivation to require permanent decline (see below): EscapePod
(Jenna "a twenty-year old ensign"), Handyman ("You are a 21 year old man",
college sorority), xclue1.0a, ovaloffice ("25-year old intern"),
Planescape-Encounters1 (self-disclosed "could be considered bestiality";
confirmed on full play to be an adult, sapient Bariaur NPC, not literal
animal — a consensual heist/honeypot con), RodneyandthePrincess40v3-1,
salvation, christmas present 1.0, studio (Shelby "I'm 19-years old and I'm
attending college"; the one "teen" line is about her), fun town (best-effort
200/200 — the game's own literal WIN action is gated behind a DEATH-only
task and can never actually fire; an authoring bug, not a missed puzzle).

**Screened clean** (23): `zanoza`, `Dolg`, `Govard`, `Govard2`, `shablon`,
`CS2`, `NAT_01`, `WanderersGoW 0.04`, `Relife` (the nine cp1251 Russian games;
hits were закончил/страх/член-as-member false positives), `smercenary`,
`hdigit1`, `The_World_According_to_CBN`, `toronto` (a teen NPC's only
sexual line is the refusal "get away perve" — the suzy precedent),
`the_view_is_better_here`, `virtual`, `imagings`, `wonderland`, `shortlived`,
`monster`, `hammurabi`, `TAOT3`, `Last_Knight`, `tempest7` (chaste Ferdinand/
Miranda courtship with a marriage ending; flagged only because the game's own
chronology makes Miranda about 12).

Vocabulary scan (`taf_pattern_scan.py plaintext()`) false positives worth not
re-chasing: draped/scraped/grapefruit → rape; circumstances/cucumber/succumb/
Documents/documentary → cum; fifteen → teen; cocky/cockroach/cockles/poppycock
→ cock; sexton → sex; profanity-blocklist easter eggs; David Whyld's
credits-reel blurbs; trophy/video-library blurbs in LCA and warlord.

## Dump decoding reference

| Field | Meaning |
|---|---|
| `RESTR type=4 v1` | variable index **+2** (`Var1=0` = referenced number, `Var1>=2` variable); `v2` compare 1 `<=` 2 `==` 3 `>=` 4 `>` 5 `!=` |
| `RESTR type=3 v1` | character index **−2** |
| `RESTR type=2 v1/v2` | task 1-based; `v2=0` must be DONE, `v2=1` must NOT be done |
| `RESTR type=0 v2` | 1/7 held (incl. worn + one container level), 2/8 **worn only** (Lost Tomb −20), 4 inside container `v3-1` |
| `ACT type=0 v2` | 4 = hands, 6 = room floor, 0 = room `v3-1`; "into object" indexes the container list **directly** (no −1; S Tar Dus T) |
| `ACT type=1 v1=0` | player to room `v3` **raw** (`v2=1 v3=0` = random room); `v1>=2` NPC `v1-2` to room `v3-1` |
| `ACT type=3 v1` | **raw** variable index; `v2` 0 set, 1 increment (`RESTR type=4 v1 = ACT type=3 v1 + 2`) |
| `ACT type=4` | score delta |
| `ACT type=5 v2` | execute/unset task, **0-based** |
| `ACT type=6 v1` | 0 win, 1 lose, 2 death, 3 stop |
| event `StartTask`/`TaskAffected` | 1-based |
| event `PauseTask`/`ResumeTask` | **2-based** (1 = any task; `evt_pauser_task_is_complete()`) |
| event `affTask (fin=1)` = own `startTask` | self-uncompleting task (Captive, Mangiasaur, To_Hell_And_Beyond, Vendetta, humbug, losttombv2, the_pk_girl, tra, wrecked) |
| event object move | room = destination **−2** (A Witch Tale), besides `Obj2Dest(raw)-1` |
| event `restart=0`/`1` | one-shot / always restarting |
| `Where` Type 0 | runnable in **no** room (Hangover T10/T14, Journ2 T86, relojero T6/T7, Great Falls T47; `make_400_whereprobe.py`); `where=3` anywhere |
| headers | `934536` 4.00, `944537` 3.90, `944536` 3.80; `3c423fc96a87c2cf94453661 39fa` 3.80, `…94453961 39fa` 3.70 (`V400_SIGNATURE` … `sctaffil.cpp:53`); 4.00 zlib at offset 22, 3.90 XOR — byte sizes are not comparable across versions |

## Transferable engine findings (index)

Each pinned by a corpus game; the Wine-measured ones are indexed in
`WINE-TRANSCRIPTS-TODO.md` and the memory `scare-adrift4-engine-index`.

- `SYNONYM` rewrites run **before** task matching (relojero, croft
  `make_39_synprobe.py`/`pSYN.taf`/`synA.png`/`synB.png`, Vardock, Worst Game,
  Seance `n`, Blood_Relatives `exam`, warlock, Business As Usual, Grumble
  `[unlock] -> [open]`). The gate needs a whole word, so an abbreviation such
  as Grumble's `unlo door` reaches an `unlo*` task the synonym would block.
- Lowest-index match wins (`run_game_commands_common()`); a lower unrestricted
  task with a trailing `*` eats the line (croft T117 vs T123; Journ2 T22 vs
  T25; Merry Murders T37 vs T39; Grumble T63 vs T459; Marooned T14 vs T24).
  When a gated exit will not open, grep every task whose `cmd`/`ALTCMD`
  matches and take the lowest index. Sibling pairs elsewhere in the file tell
  you the author's assumed order.
- A trailing-space `ALTCMD` never matches (Merry Murders `read paper `).
- Wildcards match whole words; a **medial `*` matches zero words**; a bare `*`
  never populates `%text%` (The_Hunter `say * name *`); `* *e *` matches any
  word ending in "e" plus one more (Journ2 T71).
- Task matching runs on the **raw input string**; the built-in take/drop noun
  parser knows only the object's name + prefix words (WonderWombat pills;
  Vardock `coger el revolver`). A refused `take` says nothing about a task.
- Literal `#`/`!` task labels are typeable in the Runner (Journ2 `!goto lair`,
  `!random`); an "unreachable" task needs no pattern, no `affTask` and no
  `ACT type=5` pointing at it (Journ2 T76 — zero `ACT type=5` in the file).
- A messageless restriction falls through to the library (Salutations T2); a
  task without `COMPLETE=` prints the library failure yet fires (R2DC T24,
  ADayAtTheSeaside `do form`).
- Pre-4.0 refusals: "You can't do that here!" (3.9; `.` in 3.7/3.8) for a
  matched task blocked *only* by its `Where` list, and "You have already done
  that." for a spent non-repeatable task — both consume a turn; an authored
  RepeatText displaces the message (4.0 keeps RepeatText, drops the bare
  message); room wins over done. `run_task_refusal()`, `task_is_room_refused()`,
  `task_state_allows_run()`/`task_where_allows_run()`, `make_39_whereprobe.py`,
  `make -f Makefile.headless wheretest` (`where_refusal_expected.txt`,
  `_1p_`, `_3p_`; re-blessed after commit `0318bd25`). Pre-4.0 has two
  perspectives only (`lib_get_perspective()`). The spent-task *claim*
  (`run_spent_task_390()`, 2026-09-13) walls journ2 in the Lair
  (`Adrift_2_journ2_end.txt`, `Adrift_3_journ2_t5.txt`).
- 4.0 task verbs are literal — no take↔get (`lib_typed_verb()`; TenebraeSemper,
  COBL, Greek School, man overboard).
- Rejected commands cost no turn (Diary of a Stripper, Azra); `z` ticks,
  `wait1` does not.
- Repeatable tasks pay score **once** (YADFA `blow whistle`); a second task
  with the same command does pay (`untie isabella`/`untie princess`).
- Declared MaxScore is exceedable (YADFA 314 authored / 231 declared / 243
  scored; igor 1100/1000; Reluctant Vampire 103/100) and can be stale
  (Station XIII 200/9) or 0 (Skydiver, hub).
- `WINTEXT` empty → `task_run_end_game_action()` prints the default
  "Congratulations!" — a doubled one is faithful (The Amulet, Shadrick).
- `ACT type=5` follow-up text swallows the next command; a `WALK` overrides
  `ACT type=1` NPC moves (Merry Murders Trey); an award task need not move you
  (Merry Murders T46).
- Object *seen* model: nothing is referenceable until something lists it
  (Grumble Vahla, blood bone, Ghost town blanket, Dream Quest, Dr Wilkins).
- 3.8 burden model: one pooled burden, class costs `1/3/7/3/7`, limit
  `#MaxCarried`; `|V380_OBJECT:_SizeWeight_|` normalises `SizeWeight` to 22,
  `SizeWeightClass` + `obj_get_burden()` spend the class; gen390's table
  (0→22 1→23 2→24 3→32 4→42) breaks the games (Marooned tires, Crime kettle).
  3.8 `insides()` (446CAB/446CFB) refuses takes from an unheld dynamic
  container — crime_adventure 65/95 is the true 3.8 ceiling.
  `|V380/V370_OBJECT:_InitialPositions_|` forces held/worn to the player
  (`Parent` is meaningless pre-4.0; tra red sox hat). `gs_create()` validates
  parents (`badparent_test.cpp`).
- `evt_fixup_v390_v380_immediate_restart()` must call `evt_start_event()`
  exactly once (Panic! StartText; extra roll broke circus/thetest_win/wrecked);
  restarted periods keep full length (`make_39_evseeprobe.py` refuted the
  visibility theory).
- `uip_match_optional()` rewinds on failed look-ahead; `uip_build_candidate()`
  tries the prefix with leading words dropped (Monsters r2, Shadrick oak).
- Battle: `battle_is_legacy_version()`/`battle_legacy` — damage = strength −
  defence, no hit gate; 4.0 gates on `accuracy > agility`. Armour past enemy
  strength makes a fight free (Azra 7 vs 5; deaths plate male).
- Printing: `lib_print_object()` "a " / `lib_print_object_np()` "the ";
  `%status_` lowest index; `lib_list_in_object()` run400 postfixed one-object
  format.
- `scdump.cpp` uses tolerant `prop_get()` (Richard.taf has an object with no
  `Openable`).
- `restr_object_in_place` (`screstrs.cpp`, probe `p39held`): "held" is true
  one container level down (Where Is Richard cupcake, House thyme in snuffbox).

## The waitkey audit

`harness/waitkey_audit.py` asks per row: does it set `SCR_SKIP_WAITKEY=1`,
does the de-obfuscated `.taf` contain `<waitkey>`, and does adding the
variable change the number of `>` prompts reached.

| Bucket | 2026-08-12 (233) | 2026-08-16 (242) | Meaning |
|---|---|---|---|
| IMMUNE | 50 | 97 | row sets `SCR_SKIP_WAITKEY=1` |
| NO-WAITKEY | 109 | 113 | no tag in the file |
| ABSORBED | 19 | 8 | tag never reached on the route |
| FILLED | 37 | 24 | swallowed lines covered by blank fillers |
| SUSPECT | 18 | **0** | more swallowed than fillers |

Rule since then: a row whose route reaches a `<waitkey>` sets
`SCR_SKIP_WAITKEY=1`, and its fillers are deleted; the pause read honours `#`
(`os_ansi.cpp`), which had let 25 rows' pauses eat comment lines. Witnesses:
`man_overboard` lost seven real commands; `circus` needs **ten** `give peanut
to pringles` under `SCR_SEED=12` (twelve used); `TheADRIFTProject` had been
blessing a broken run (fillers answering the name prompt); `griswold` regained
`x cassette`. Far From Home has a `<waitkey>` *before* the name prompt; Great
Falls has one *between* the name and gender prompts. Re-run the audit after
touching any waitkey row.

## Game index — games whose only write-up is this file

One line each: file / verdict / the fact that mattered / row env. Games with a
`notes/<Game>_walkthrough.md` are not repeated here (see that file), except
where a later rule moved the row. `SKIP` = `SCR_SKIP_WAITKEY=1`.

### Rows moved by later ports

| Row | Now | Why |
|---|---|---|
| `journ2_solution.txt\|Journ2.taf\|You are carrying the King of Hearts.` | 5/90, 23 cmds | spent T3 catch-all claims every Lair command (run390-true); old 30/90 route commented inside the solution |
| `journ2_patched_solution.txt\|Journ2.taf\|Your score is 30 out of a maximum of 90.\|SCR_SEED=2 SCR_ASSUME_REPEATS=1 SCR_ASSUME_PATCHES=1` | 30/90, 41 cmds | female career (`female` on move one); `PATCH_JOURN2` gives Rage's female valve twin T24 a room and the four typable phrasings and quiets T22's debris claim, REPEATS gets past the same spent T3 Lair brick; 30 is the female ceiling — Rage's exit gate names the male twin |
| `vampire_solution.txt\|Vampire.taf\|Your score is 70 out of a maximum of 100.\|SCR_SKIP_WAITKEY=1` | 70/100 | spent T61 claim; was 100/100 |
| `merry_murders` marker `My score is 120 out of a maximum of 135.` | 120/135 | spent T46 claims the second archives `n`; winning tail in git history |
| `crime_adventure` | 65/95, 90 cmds | 3.8 `insides()` take gate; byte-identical to `cmdfile_w_crime.txt` / `Adven_1_crime.rtf` |
| `great_escape`, `james_bond`, `life_of_mike` | re-blessed | pre-3.9 take gate (`take jacket` before `take key card`, `take bag` before `open bag`, `take box` before `take case`) |

### First wave (2026-06 → 2026-08-04), games without a notes file

| Game | Result | Key fact |
|---|---|---|
| The Hangover (`hangover.taf`, 3.90) | UNWINNABLE 5/7, 53 cmds | T10/T14 `where=0`; row `the_hangover_solution.txt\|hangover.taf\|Your score is 5 out of a maximum of 7.`; opened the refusal port |
| Troll! | WON 185/190, 145 cmds | T80/T82 consume the same object, 82 dead; `w` upstairs *before* `unlock door` (T86); second `put breadcrumbs in basin`; tavern drops on entry |
| Locked door with water trap | transcript replay | — |
| A Spot Of Bother (Whyld) | WON 100/100, 270 cmds, SKIP | one repair: second `push door`; menu picks `2`,`1` first |
| The Amulet (Hiebert) | ending, verbatim 12 cmds | no score; doubled "Congratulations!" is faithful |
| Monsters r2 (Hiebert) | WON 40/40, 38 cmds | `open door` not `open the bedroom door`; found the two parser bugs above |
| Shadrick's Travels (Mystery) | WON 100/100, verbatim 22 cmds | prompt glyph is CP1252 `Ø`: `cmds = [l[1:].strip() for l in d.split('\n') if l.startswith('Ø')]`; three author duds kept |
| Beanstalk the and Jack | `*** You have won ***`, 49 cmds | reverse chronology; delron footer trimmed |
| Black Sheep's Gold | won, 99 cmds, SKIP | epilogue pause eats `quit` without SKIP |
| Doomed Xycanthus | "Congratulations!", 82 cmds | verbatim |
| Dancing Even Him? (Otter) | anagram reveal, 17 cmds | title = "Vending Machine" |
| akron, twilight, cave, haunt, haunted, great, secret, tra | 3.80 rows | see corpus table |
| Duck McCloud, Fistandantalus, James Bond 2000, Microwave Man, Life of Mike, Super Liam | 3.80 rows | see corpus table |
| Qui a tué Dana?, Enquête à hauts risques | French rows | — |
| adrift-battle rows: ptbad, vague, Escape To New York, unauthorized termination, To Hell In A Hamper, marika, Vendetta, Unraveling God, mishmash | row + golden are the record | — |
| ImagiDroids, Crimson Detritus, Chosen, The Cellar, Panic!, Second Chance, Private Eye, Plague Redux, Marooned, deaths, Castle Quest, Alice's Restaurant | have notes files | Panic! found the restart-fixup bug; Private Eye needs SKIP + leading `3`; Chosen sockets spell A-D-R-I-F-T; Plague discovery verb is `SEARCH`, `x coins` is the counter |

### Fourth wave batches 1–10

| Game | Result | Key fact |
|---|---|---|
| Sandy | UNWINNABLE 0/0 | `RESTR type=4 Var1=0` referenced number (author bug) |
| Newton | won | turn-4 `get apple` |
| Phoneb | won | `x me` then `take phone booth` |
| PTGOOD | won | `open vial` |
| Smote | won | ice / pyramid / flood |
| JINXTRON | no ending; `jinxtron_full_solution.txt` | `Jinx` 1→2→7→8→9→10; RNG word |
| Picture | won | `ask picture about bird/parrot`; trailing blank line = turn |
| Rift | won | `x the floorboards` (article) |
| Foggy Banana | won | `use hoover`/`use phone` twice |
| Just Another Day | won, 135 lines, SKIP | `Jump`; `w` from Cubicles |
| Blast | won | 100 HP demon; Var1 +2 quirk |
| Pilfers | 107/107, SKIP | `DOOR:2`; RestrMask AND |
| The_Stowaway | 10000/10000 | `use kid as a shield` |
| Witness_Demon_vs_Vampire | won, SKIP | pentagram order |
| The Vault | won | `read bible` first; zero restrictions |
| hiker | Ending Three | `kill the hitchhiker` |
| raccoon | won, SKIP | — |
| Way Out | won | — |
| The Fly Human | unwinnable by design | — |
| zombiecow | 100/130, SKIP | — |
| outline | 5/5 | `x*outline` |
| hungry | won | — |
| The Long Barrow | won | dark patch defuses the 5-turn timer |
| Asteroid Aftermath | won, SKIP | valves |
| rollingthedough | 50/50, SKIP | — |
| The_Shuffling_Room | won | `use switch` twice |
| herrdoktor | won | squirrel jetpack |
| Angel Devil Human | won | leading `1` |
| Existence | won, SKIP | `use fan`/`use pencil` |
| zacksmackfoot | won, SKIP | — |
| boiled eggs | won | — |
| P2P | 30/30, SKIP | — |
| MurderMansionntro | no ending, SKIP | — |
| whitterscap | 2/2 | Q-word is not `quit` |
| Dangers of Driving at Night | won, SKIP | — |
| All Hallows Eve | 23/26, SKIP | — |
| gorxungula | won, SKIP | `restart` from the death screen gives the coin |
| lobster | death >6000, **no SKIP** | blanks are pauses |
| Business As Usual | won | SYNONYM nouns → "go to X"; `take all`/`drop all` |
| Oh_Human | 60/200 door; 200/200 ladder (ohhuman_200) | `theroom==4`; `look` then `wait` for event 3 |
| Skydiver | 1000/1000 | MaxScore 0 → closing-line marker |
| the_road | won, 42, SKIP | — |
| Perfect Spy | 10/10 | transient form flags |
| seciden_oddcomp | 102/102, SKIP | 17-turn timer; fang = pauseTask |
| Perspectives | Negotiation ending, SKIP | T10 dead behind T9 |
| Big City Laundry | won | back door open/closed |
| Over the Edge | won, SKIP | `open your eyes` |
| Drinks | won, SKIP | `go south` first |
| R2DC | 1,000,000, SKIP | T24 fires without `COMPLETE=` |
| Forest House v2 | 13/12, SKIP | — |
| Shetland Enigma | 210 vs declared 100 | `look` re-seeds the ice |
| Take One | won, SKIP | turn-16 demon |
| Tenebrae Semper | unwinnable, SKIP | `get pen` (verb-literal); `Adrift_1_tenebrae_probe4.txt` |
| Helsing | won, SKIP | `play #4` |
| Worst Game (AIF) | won, SKIP | SYNONYM |
| Spooked | 8/8, SKIP | — |
| video.tape | won, 1850, SKIP | `RESTR type=0 v2=4` relics in bowl; weight 90 |
| Regrets | won, SKIP | — |
| Terrified | 60/65 faithful, 65/65 patched | T89 `where` after move (patched 2026-09-27) |
| rain | won, SKIP | `ACT type=0` raw−1 vs `ACT type=1` 0-based |
| howitstarted | 6/6 | — |
| Station XIII | 200/9 | stale MaxScore; `wt<=108` |
| Choose Your Own Three Hour | 9/14, SKIP | — |
| thelasthour | ending, 121 cmds | turn-120 EVENT |
| Sex is Mental (AIF) | won | — |
| Pete's Punkin | 505/575 | — |
| Crooked Estate | unfinishable, SKIP | — |
| Alias Undercover Agent | 35/35 | — |
| A View to a Home | three medals | — |
| briefcase | won | 2-turn window |
| The_Seance | 100/100, SKIP | `open door` first; full direction words; SYNONYM `n` |
| reactor_1 | won, SKIP | — |
| Motion | 100/100, 137 lines, SKIP | — |
| tophat | won | — |
| 3 minutes1.0 | won, SKIP | `pull rope real hard right` |
| neighbours | 100, SKIP | `x boxes` ×2 extra |
| First To Arise Alone With A Pug | 100/100 | `open front door with danthil` |
| ForestHouse3 | won | dark theme, wired normally |
| DayAtTheOffice | won, SKIP | — |
| beer | 50/50 | `search dirt` |

### Fourth wave batches 11–21

| Game | Result | Key fact |
|---|---|---|
| Mr_Fluffykins | only happy ending, 5 cmds, SKIP | CYOA `turn to page N` |
| A Witch Tale | win-only, 44, SKIP | event move `room = dest−2` |
| Door to Utopia | Heaven ending, 59, SKIP | `success` 0–6; cheat `i'm a cheat` avoided |
| Patient7 | won, 59, SKIP | betray both secrets Day 2, `demon` Day 3 |
| The_Final_Question (Whyld) | won, 17, SKIP | stall `z`; 4 books |
| mustescape | won, 83, SKIP | shared health pool; medkit before fight 2; `cheat` avoided |
| Caida libre (Spanish) | 8 cmds | ALR `score`→`puntos`: "You puntosd 0 fuera of the maximum 0!" |
| Temple_Of_The_Sun (AIF) | auto win, 31, SKIP | robe + headdress |
| Wolves_at_the_Door | turn-25 "rescue" death, 26, SKIP | no surviving branch; omit `x small area` |
| apokalupsis | won, 46, SKIP | `evidence`≥5; ALR `[regjim=N]` |
| dusk | 10/10, 33 | `stuff` walkthrough; `x sapling` twice |
| The_Hunter | 50/50, 62 | `say * name *` bare-`*` bug |
| Will | 200/200, 124 | marbles → "go down" death; banister |
| COBL | 160/230, 107, SKIP | newspapers on a static object in an empty-room list |
| puzzlebox | win-only, 85 | clock minutes +5 off-by-one |
| amy (AIF) | won, 19, SKIP | shortcut code |
| YNKaboom | $96,300,000, 25 | yes/no CYOA; off-topic counter must be 3 |
| hub | 80/80 internal, 112 | MaxScore 0; `walkthrough` cmd has ~6 bugs |
| darkness | 50/50, 111, SKIP | 7 of 8 notes; flare after lever |
| Dream Quest | 100/100, 187 | seen model sparrow/nail; one-way bridge; Freezing Passage |
| Dr Wilkins (AIF) | 117/95, 178 | softlock room avoided |
| jailbreakbob | WINNABLE 38/38, 31, SKIP | T35 1-based → Hoggins in YOUR CELL; comb→coin→phone opt 4→`ne`→`get gun`,`n` T72; `Adrift_2_jailbreakbob_win.txt` |
| In_the_Claws_of_Clueless_Bob | 12/12, 40, SKIP | SetVar score |
| BSG22 (AIF) | won, 14, SKIP | 8 topics |
| Back Home | lose-ALR "GAME OVER" as ending, 54, SKIP | — |
| zelda | 197/197, SKIP | small key from Like-Like in Graveyard with Ganon mask + shield; `Adrift_2_zelda_win.txt` 188/188 |
| Showtime_at_the_Gallows | End room, 165, SKIP | Zero turn 9–10; hounds period 4/5; ask Zero's name then brick |
| The Old Church | won, 17, SKIP | sword to sexton = early-end trap; cheese→mouse |
| suzy | 22/22 custom, 31, SKIP | `b walk`; vase water + extinguisher; parking lot ≥10 turns = bad ending |
| Rock Band | won, 24, SKIP | 1000-point minigame; `use finger on xbox`; unplug |
| Aegis | End room, 74, SKIP | omit "with the sword"; marker " END" |
| warlock (AIF) | won, 58 | lick→kiss SYNONYM |
| For_Love_of_Digby | WON 95, SKIP | — |
| Sigurd_Fafnesbane | won, 12, SKIP | — |
| Unfortunately | won, 60, SKIP | "An ending to be sure - and the best one in the game to boot!" |
| frustrated | max, 85, SKIP | — |
| Camelot 1,5 | won, 56, SKIP | "Looks like the old Merlin did read your mind correctly after all." |
| JimPond | won, 127, SKIP | score confirmed via scratch copy |
| Greek School Adventure | 185/275, 165 | verb-literal probe |
| Trick or Treat | 270/270 variables, 213, SKIP | menu battle `1,3,6,8,9`; carrot/donkey +5 |
| volant | WON, 60 | second `talk to techthon` |
| BarneysProblem (AIF) | 114/121, 133, SKIP | marker `BABYLON` |
| HalloweenHijinks | 8 treats, 89, SKIP | — |
| Full_Circle | 52/52, 220, SKIP | "Full Circle has ended."; embedded CompleteText walkthrough |
| Dear Diary (AIF) | 300/300, 134, SKIP | marker `FUCK YOU ERIK` |
| Riding_Home (AIF) | 100/100, 57, SKIP | `GameProgress`≥90 before the bus |
| Dear Diary 2 (AIF) | 300/300, 68, SKIP | — |
| Paint | best ending, 81, SKIP | `xscor` 76−5=71 |
| YADFA (Whyld) | 243/231, 341 | terminal room 13; marker `gained yourself a nice (haunted) castle. Not bad for a day's work.`; `notes/YADFA_walkthrough.md` |
| Blood_Relatives | 11/13, 6 | referenced-number bug like Sandy; `exam desk` SYNONYM |
| DeadReckoning | best of three endings, 32, SKIP | no score |
| hcw (AIF) | 11/13, 250, SKIP | two hidden `[Press Any Key to Continue]` |
| Scandal (AIF) | won, 49 | `tell penelope about byng`; 20× `single shot` |
| cldone (AIF) | 30/30, 34 | `mir`≥15 via 17 tasks |
| blood (Otter) | 140/140, 87, SKIP | TASK 724→5→41 stop chain; marker `You managed to score 140 out of 140.`; gloves mandatory; Alison's bracelet sets three clues; kill teleports to Cemetery |
| mind of master | WON, 29, SKIP | "You are victorious, whoever you might" |
| magicshow (AIF) | 67/67 + magic 47/47, 152, SKIP | 18 `z` after `take hoops` until Tiffany leaves (EVENT 0, time 27); deck rides in the hat — `take hat`/`wear hat` after `get rabbit from hat`; marker `Well done - you scored maximum points!` |
| Whatever Happened to Uncle Grumble | 226/404, 267, SKIP | hero `a` (`var8==0`); −50 bomb trap T224 (give bomb *then* love); T280/T459 unreachable; T388 via `unlo door` (synonym bypass); +beer to Kringle, Dip `cut rope`/`chop tree with blade`/`climb tree` (2026-09-26); endgame `no`,`wait`,`give nulgas potion to grumble`,`u`; marker `Your score is 226 out of a maximum of 404` |
| CowboyBlues | 177/401 easy; medium 188 patched (T365), rows cowboyblues_medium[_patched] | row `cowboyblues_solution.txt\|CowboyBlues.taf\|how does it feel to be a hero then, Fingle Bodge?\|`; terminal room 35 via T591; `var0` 0=difficult 1=medium 2=easy; `blow whistle` first command in Jake's hideout; third `get key` is death |
| requiem | won, 74, SKIP | "you have reached the game's best ending" |
| WithoutAClue | won, 125, SKIP | "you've managed to finish the game" |
| The Potter and the Mould | 150/150, 333 | row `mould_solution.txt\|mould.taf\|Congratulations on winning The Potter and the Mould\|`; terminal room 103; clay dog shaken via `push red`,`n`,`close south`,`n`,`d` then `d s s e u w`; boss `5 2 3 1`, final `2` |
| rking | 100/100, 111, SKIP | — |
| hero | 208/200, 191, SKIP | "the world is a better place for your actions" |
| datewithdeath | won, 303, SKIP | hidden `ritual` var; "And you have a whole life ahead of you to live" |
| alchemist | 500/500, 520, SKIP | "That is 100% of the game"; `get star` after `drink potion` |
| ONNAFA | 76, 188, SKIP | fake agent completion caught; `RESTR type=4 v1` footgun |
| House | 30/30, 284, `SCR_SEED=1 SCR_SKIP_WAITKEY=1` | charmed snuff box (task 340 `noinimod on sah emit`) defeats both aging tasks; empty and open the box first; marker `YOU HAVE COMPLETED HOUSE.`; `examine notebook` not `read`; `look` after `wind clock hands back` is load-bearing; `%drunk% west.` is the game's bug |
| Lights, Camera, Action! (Whyld) | score 85 best ending, 261 | no EndGame; room 22; marker `best ending in the game!`; `pull creeper` after `kick saucer`; give trident *after* both haunted-house trips (T237 strands Witherspoon); `talk daisy` twice, `talk violetta` ×4; murder strand mandatory (T704) |
| mutaydid | 1300/1300, 28, SKIP | — |
| goblin (AIF) | King ending, 54 | "Well done - you scored maximum points!" |
| ADayAtTheSeaside | 9/9, 40 | `do form` fires without `COMPLETE=` |
| The_Reluctant_Vampire | 103/100, 198, SKIP | `job` var |
| ss whore (AIF) | 7/7, 136, SKIP | orders before execution soft-lock |
| warlord | 100/100, 356, SKIP | `megacheat` avoided; `b1` shortcut |
| igor | 1100/1000, 22 | switches 5-4-3 |
| Tic-Tac-Toe | 0/0, 8 | cells 1-3-5-7 |
| El ascensor (Spanish) | won, 10 | — |
| MikeDesert_SuburbanProdigy3 | 80/80, 30 | washed paw to the voodoo priest |
| The White Singularity | 0/0, 6 | three binary choices |
| Il Golem (Italian) | 42/45, 89 | 7/4/3 flasks |
| wumpusRun | 0/0, 11 | — |
| Bandera (Spanish) | 100/100, 39, SKIP | marker "Well done - you puntosd maximum points!" |
| Egg_Hunt | 950/1000, 52 | "You finished 50 points short." |
| Ghost town v1,05 | won, 248 | day/night EVENT 2/3 (40/35); `Evidence`==4 from newspaper / door 105 (night) / knife (day, `reach in compartment`) / Boot Hill `open hand` (`read marker` exactly 4×); Ninette parked one room away; `climb up rope` immediately after `blow up safe`; marker `Slowly two figures are seen shimmering in the air. One of a pretty young girl`; command *count* before the `sleep`s is frozen |

### Second/third wave and 2026-08 singles — all have notes files

I, Dreamland, Forest On The Norm, Bob Bobsly (155/155), Druggy Lane, Escape
from Insanity (1000/1000), Lost Souls, Chicago (75), Everything Emanuelle,
Textident Evil, Impulso, Montahue Scott (3/3), A Morning With A Headache
(115/115), Sleaze City, Albridge Manor, The Lost Mines, The Dark Tower (T8
`turn on power`), Report Espionage, Far From Home, S Tar Dus T, Silk Noil;
Asylum, The Wheels Must Turn, Life (UNFINISHABLE demonstration row — no
`ACT type=6`, no `ACT type=4`), Renuntio, House Of Horror (145/155 faithful
ceiling: T109 tests OBJ_HIDDEN; 155/155 patched 2026-09-27), Where Is
Richard? (1000/1000); Salutations,
A Day at the Iachini House (115 of 140 authored), La hija del relojero,
Veteran Knowledge ×2 (`vetknow2.taf` differs in three strings), The Lost Tomb
(175/175), The Long Journey Home, Murder in Great Falls (200/200), The Vampire
With A Conscience, The Merry Murders, The Woods Are Dark (100/100; T16 scores
only in Drew's Bedroom; T10 teleports), Captive Universe (100/100; one-shot
clock turns 8/18/18/20; T3 `hint` −356), WonderWombat (217 cmds; marker
`THUMPER KICKS ASS!!!`; random maze rooms 32–41; nine cheats `win chunka oozle
thoof skam joxxx glenn rottencop joepoe` unused), The Town Of Azra 3.90 (414
cmds / 505 turns; rooms 16/17/18 orphaned in both builds), Vardock Bates (103
cmds; TASK 35 needs TASK 36 undone; EVENT 0 ten turns to `decir museo`, EVENT 3
one turn to `esquivar el baston`), deaths (100/100; `Hero`/`male` prompts;
plate male makes Rooth and the boss safe).

AIF rows without notes (row only): windy2 (150/150, name prompt line 1, EVENT
0 five-turn skinny-dip gate, shed unlock and enter are two tasks), croft
(150/150; author's `croftwlk.txt` tops out at 147 — T117 eats the rewritten
line; `tomorrow` typed bare; pull the altar), dr-who-vortex-lust (150/150;
`Sam` line 1; Leela *before* the Lab; hat `Var2=8` strips 12 of Ace's 15;
`12553m`; `122-663a` is a red herring), gamma (150/150; `Sam`; `stacey sex ==
7` exactly; Sharron solo chain before T63 moves Shannon; `wendy` keystone).

## Corpus provenance

**3.80 — 17 files survive, all here.** IF Archive holds 7, adrift.co 9
(overlap 5), plus six that adrift.co serves but never lists (found by probing
130 filenames from Wayback captures of `tardis.ed.ac.uk/~jcw/adventure/` and
`jcwild.pwp.blueyonder.co.uk/adrift/`).

| file | title |
|---|---|
| `marooned.taf` | Marooned (80/140 ceiling; ferries under the pooled burden) |
| `wrecked.taf` | Wrecked (250/250) |
| `Crime_Adventure.taf` | Crime Adventure (65/95 in 3.8) |
| `akron.taf` | Akron |
| `cave.taf` | Cave of Wonders |
| `haunt.taf` | House of the Damned |
| `twilight.taf` | The Twilight (485/500 faithful, 500/500 patched) |
| `haunted.taf` | The Haunted House (Campbell Wild, Jun 1999 — the oldest) |
| `great.taf` | The Great Escape |
| `secret.taf` | Tom Ceader: Escape from the south |
| `tra.taf` | The Timmy Reid Adventure (the `Parent -1` witness) |
| `duck.taf` | Duck McCloud — The Fight Begins |
| `first.taf` | The book of Fistandantalus |
| `jb2000.taf` | James Bond — Happy Landings |
| `microwaveman.taf` | Microwave Man! (smallest 3.8 game) |
| `mikes.taf` | The life of Mike |
| `superliam.taf` | Super Liam 1. A hero is born |

**3.70 — two files**, both unlisted anywhere: `arlo.taf` (*Alice's Restaurant
Anti-Massacree Adventure*, 190/190) and `castle.taf` (*Castle Quest*, 50/50).
`V370_PARSE_SCHEMA` (`sctafpar.cpp`): extra header integer = 0-based winning
task, movements as pairs over one destination list, no `BWinGame`, 17
renameable built-in words. Nothing 3.60 or older survives; ADRIFT 3.23 was the
first to encrypt, so anything older would be plain `Version 3.xx`. `run380.exe`
and `run370.exe` come from delron's `adrift38.zip`/`adrift37.zip` via the
Wayback Machine (adrift.co's `run380.zip` contains 3.90 binaries). `tagv2.zip`
is Campbell's TAG (DOS Pascal ancestor), not a taf.

Every file in `games/` is a real file (72 former symlinks replaced); the corpus
does not depend on `~/Downloads`.
