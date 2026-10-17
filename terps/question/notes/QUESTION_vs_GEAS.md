# Question vs. Geas: the Quest 4 side

This compares the Quest 2–4 half of Question (the `question-*.cc` engine, the
bundled libraries and the Glk front end in `questionglk.cc`) with legacy Geas,
the interpreter it was forked from. The Quest 5 engine (`quest5/`, `aslx*`) has
no Geas counterpart and is left out.

"Geas" here means Geas 0.5 (Tilford, Jones, Baldwin; Quest 4 only), in the copy
Spatterlight took from Gargoyle in `482eca8d0` (2021, "Add geas support"). The
baseline used for every measurement below is `513b0f4ff` (2024-08-11), the last
commit before the 2026 rework began at `307e63c25`. The interpreter was
renamed from Geas to Question on 2026-09-28; the credits line still says
"based on Geas 0.5", and a few legacy names are kept on purpose (`GEASUNDO2/3`
undo magic, "Saved by Geas" accepted when loading). Figures are from
2026-10-12.

## In short

| | Geas 0.5 | Question (Quest 4 side) |
|---|---|---|
| ASL versions played | 3.11–3.53 (the source says so) | 2.00–4.10, plus compiled `QCGF001`, `QCGF002`, `QCGF003` |
| Reference for behaviour | the Geas author's reading of Quest | real Quest 4.1.5, line by line against QuestViva's `V4Game.cs` |
| Bundled Quest libraries | none | `standard.lib`, `stdverbs.lib`, `Typelib.qlb`, `q3ext.qlb` |
| Corpus rows that reach their ending (of 150) | 30 | 150 |
| Language | C++ in one 4,150-line `geas-runner.cc` | C++, 15 `question-*.cc` units, 51 script handlers in a table |
| Size, engine and front end | about 8,500 lines, 19 files | about 17,100 engine + 3,100 library sources + 3,600 Glk, shared with Quest 5 |
| Time for the same 150 routes | 8.7 s (30 reach the end) | 3.3 s (all 150 do) |
| Pictures, sound, links, autosave | none | yes |
| Tests | none in tree | 150 walkthrough rows with goldens, 179 `.asl` fixtures, unit, `-Wshadow`, ASan and autosave suites, oracle replay |
| Maintained | No (Gargoyle's copy is frozen) | Yes |

The route figures come from the replay described under
[Games that work in Question but not in Geas](#games-that-work-in-question-but-not-in-geas).
Read the caveats there before quoting them.

## Where Question is better

### It plays far more games

Geas documents that it supports ASL 3.11 to 3.53, and the numbers agree.
Question's corpus spans ASL 2.00 to 4.10: the text games are 200×1, 210×6,
217×3, 300×4, 310×4, 311×6, 350×19, 400×28, 410×35, and the compiled ones
are `QCGF001`×3, `QCGF002`×36, `QCGF003`×5. Geas decompiles only `QCGF002`;
the other two compiled formats are read as text and refused.

Even inside its documented range Geas is thin. It has no `standard.lib` or
`stdverbs.lib`, so every game that includes them (most 4.x games) loses the
verbs those libraries define, and its own native verb set is small: look,
examine, take, drop, use, give, speak to, go, inventory, quit, help, about,
undo. `open`, `close`, `read`, `wear`, `remove`, `put in/on`, `search`, `push`,
`pull`, `lift`, `shake`, `watch`, `attack`, `wait` and the rest answer "I don't
understand your command". Question implements those, along with Quest's
container model, room-entry order, `oops`, `startin`, `moveobject`,
`revealobject`/`concealobject`, runtime `create` of objects and types, and a
`VERBS` meta-command.

### It behaves like Quest

The oracle in `test/quest4/harness/oracle/` replays the same 150 scripts
through QuestViva's `V4Game.cs`, a line-by-line C# translation of Quest 4's
VB6. 139 of 150 transcripts are identical and the other 11 differ by 398
lines, 338 of them from one desync. The differences are catalogued in
`FINDINGS.md` (about 80 numbered findings) and the intentional ones are
carried as `deliberate:` rows. Geas has no such reference and had never been
compared against one.

Specific rules fixed along the way: `and`/`or` in conditions fold left to
right (`A or B and C` means `A and C`), 4.x room-entry order, the spacing of
timer and `wait` output matched to 4.1.5, and `ASL` keywords are
case-insensitive.

### It is nicer to play

- A side pane with hyperlinked objects, exits and verb menus, status
  variables in the banner, `SIDEBAR WIDE`/`NORMAL`, `STATUS`, and a `#HELP`
  table.
- Pictures (`@WxH` sizes stripped), MIDI/MOD/WAV sound with the synchronous
  `play` continuation.
- UTF-8 output and Unicode line input. Geas mangles Latin-1 and UTF-8
  characters in game text ("Chlo\xef\xbf\xbd").
- A "Continue..." link for `wait`, an end-of-game menu (undo, restart,
  restore, quit), undo after the game has ended, and timers started only
  when the game uses them.
- Spatterlight autosave and autorestore (unified format
  `QUESTION-AUTOSAVE 1` / `QUESTIONGLK-AUTOSAVE 1`), including mid-menu and
  mid-timer-cycle states.
- Randomness from `erkyrath_random` (xoshiro128**), so `QUESTION_SEED` or the
  app's testing mode makes a game replayable. Geas uses libc `rand()`.

### It is sturdier and faster

Geas asserts on malformed game data and aborts. Question recovers instead;
the loader was hardened against malformed games, four script-engine crashes
and a hang were fixed, a 17-bug review was cleared, and save, restore and
undo were fixed (undo across restore works; a save the game could not have
written is refused). The full tests run under ASan. Script nesting is capped
at 500 (175 under ASan) so runaway recursion ends as an error rather than a
stack overflow. It is at least 30 percent faster, see [Timing](#timing).

### It is tested

Geas shipped without tests in this tree. Question has 150 walkthrough rows
with committed command scripts and transcripts, 179 `.asl` fixtures for
individual statements, `question_unit_tests`, `make syntax` (per-TU
`-fsyntax-only` with `-Wshadow`), `make check`, `make asan`, a corpus pinned
by sha256 in `games.manifest.tsv` with `fetch_games.sh`, and an autosave
suite with cases for every-command, RNG, variables, undo, timers, menus,
questions, corrupt and old-format files.

## Where Geas is better

- **Smaller.** About 8,500 lines in 19 files against about 24,000 in the
  whole of `terps/question` (of which the Quest 4 engine alone is about
  17,000). If you only need to run a handful of 3.x games it is easier to
  read end to end.
- **Stable shared base.** Gargoyle embeds the same code; fixes there can be
  compared or pulled directly. Question has diverged and the two no longer
  share a diff.
- **Plainer behaviour.** Geas has no catalogue of deliberate deviations to
  learn; Question has a documented one (pre-2.80 `place <prefix;room>`
  fencepost, trimmed declared names, `<ERROR>`-at-load compromise, obeyed
  `outputoff <>`, accepted `pause <>`, lone command pattern, object called
  by pronoun, verb with trailing space) and a lenient internal-name mode for
  players that real Quest does not have.
- **Provenance.** Geas is an old, human-written, widely used codebase. The
  2026 work in Question is largely done with AI assistance and verified by
  the oracle and the corpus, but several goldens are self-goldens: they pin
  Question's own current output, and only the oracle replay ties them to
  Quest.
- **No bundled libraries to maintain.** Question carries 3,079 lines of
  built-in library source (`standardlib_builtin.cc`, `stdverbs_builtin.cc`,
  `typelib_builtin.cc`, `q3ext_builtin.cc`) that must stay in step with the
  original `.lib`/`.qlb` files.
- **Licence is unchanged.** Both are GPL 2+; the headers carry Tilford 2006
  on derived files and Sjölund 2021–2026 on new ones, and the built-in
  libraries keep their own authors' notices.

## Games that work in Question but not in Geas

### How this was measured

All 150 Quest 4 corpus rows in `test/quest4/harness/run_walkthroughs.sh` that
have a win marker were replayed through two engines:

- the current Question, with its existing walkthrough runner
  (`QUESTION_SEED`, strict names);
- the legacy Geas at `513b0f4ff`, with a headless adapter that unity-includes
  its engine sources and applies the same script rules (header skipped to
  `Start:`, trailing `  (note)` stripped, menu answers from the same queue,
  `--tick` calling `tick_timers` after each command). The legacy `$rand` was
  patched to draw from Question's generator (`erkyrath_random`, same seed,
  same signed-range-then-modulo mapping), so both engines see the same random
  stream for the same seed. An earlier run with libc `rand()` and seeds 1–5
  gave the identical 30 games, so the generator is not what separates them.

A row counts as passing if the marker text appears in the output.

### What the numbers do and do not show

- The routes were derived against Question, using Quest's strict
  alias-only noun matching. They are a Question-biased test: a route that
  needs a verb Geas lacks stops there, while a route written for Geas
  might have gone further. For the same reason there are **no** games that
  win in Geas but not in Question. That is a property of the method.
- Not reaching the marker is not proof the game is unwinnable in Geas.
  Most first divergences are an unknown verb, after which everything
  cascades.
- Random draws matter in only 20 of the 150 rows; the other 130 never call
  `$rand` in either engine. Of those 20, Geas never gets as far as a draw in
  11 (it does not load, or stops at once: Forward and Back, Clay Pigeon,
  World's End, Pure Chaos, Green Light, Devil's Bargain, Magic Sword,
  Kingdom, Sim Political Career, Nearco 3, Beyond Manage). In 3 (Schoolgirl,
  Easy Money, Sleepover) the two engines draw the same number of times after
  every command. In the remaining 6 the counts part ways, always because the
  game is on a different path: Barbarian (`attack` unknown to Geas, so the
  fight menu never opens), Wizard (spells and `wait`), Zombies Attack
  (`use aerial on locked drawer` rolls a lock-picking chance in Question;
  Geas says it doesn't see the aerial), Shipwrecked (Geas has no coconut
  where Question has), Sir Loin 3 and King's Quest V. Only one of the 30 games
  Geas completes, Sleepover, rolls at all, so the shared generator can't
  explain the gap. The counts were taken by counting calls to the generator
  per command in both engines, with the matched generator.
- A marker is a marker; it is not a full-score win.

### Results

| Group (rows) | Question | Geas |
|---|---|---|
| `QCGF001` (3) | 3 | 0 |
| `QCGF002`, asl 311–353 (6) | 6 | 4 |
| `QCGF002`, asl 400–410 (30) | 30 | 2 |
| `QCGF003` (4) | 4 | 0 |
| text, asl < 311 (18) | 18 | 3 |
| text, asl 311–353 (26) | 26 | 8 |
| text, asl 400–410 (63) | 63 | 13 |
| **All (150)** | **150** | **30** |

Of the 120 that fail in Geas, 112 run to the end of the script without
reaching the marker and 8 never get going.

### Why they fail

1. **Unsupported versions.** ASL below 3.11 loads but runs silent or with no
   output (Koww, Uranus, Lovesong, Hobbit, DreamWeaver, Oscar, Black
   Forest). I did not find the exact reason for the silence; Geas warns
   "Geas only supports ASL versions 3.11 to 3.53" and Uranus also logs a
   duplicate block registration.
2. **Compiled formats.** `QCGF001` and `QCGF003` are read as text and
   produce "Braces Unbalanced" or "game not running".
3. **Preprocessor trouble on 4.x text games.** "Braces Unbalanced" on games
   with a `' "Title"` first line, `!include <stdverbs.lib>` or 4.x script
   syntax: Riddle Run, Clay Pigeon, Exits of the World, World's End, Kurri
   Ming, Skate Ur Ass Off, Pure Chaos, Pilgrim's Progress, Birthday
   Assignment, Rainbow Room, Pyramid of Terror, Prison Break, Michael's Game,
   Statue of Riddles, Sutekh, Ponyville, Shadow Masters, Doctor Who 50, Sim
   Political Career, The Bomb, Beyond Manage, Realm of Chaos demo and, among
   `QCGF003`, Forward and Back, Dog Catcher, Caravan Chaos, Nearco 3. A few
   others load and stay silent (Briny Blue, Holes, On Time, The Lazst Resort).
4. **Missing verbs.** "I don't understand your command" where Question
   answers: `open tinted jar` (Adventure), `destroy sword` (Magic World),
   `read it` (Sir Loin), `search lake` (Bear Campsite), `open fridge`
   (Sir Loin 3), `open door` (Red Sauce Monday, Office), `open case`
   (Darkness, Christmas Day), `watch tv` (Permanent Room), `attack`
   (Londe Perplex), `wear T-shirt` (Freshman Fantastic), `lift tapestry`
   (Blight of Elantria), `read map` (Shiversword 2), `shake tree`
   (Shipwrecked), `push gates` (Ghost Light), `read textbook` (Theses of
   Canada).
5. **Different output.** "You are in the: Entrance Hall" and "You can go to
   Car." rather than Quest's room text and "or to your Car"; "There is Cord
   here"; inventories without descriptions; container contents not listed
   (Escape Prison, What Do You Do); `(first removing it from coat rack)`
   auto-take missing (Tardis Escape).
6. **Missing game mechanics.** `speak to X` with no speak property found
   (Cell Phone, Hospital Visit), `use … on` replies ("You don't see any
   emergency exit"), give-to-NPC silent (Escape House), `~var~` substitution
   unprocessed (Easy Money prints "(~BCBBchange~)"), menu and choice flows
   (Wizard memorize; Metroid Lite numeric answer).
7. **Aborts.** SIGABRT in Green Light, Get Out of the House, The Former and
   Mario is Missing 2; SIGSEGV in Kingdom; "game not running" (exit 3) in
   Romantic Music, Devil's Bargain and Magic Sword, all `QCGF001`.

### The 30 games Geas completes

Adventure, Magic World, Assassination, Darkness, Haunted Horror, Beam,
Christmas Day, Posh, Mystery Quest, Theses of Canada, Awakening of the Dead
2, Defenders of Gondor, Mysts, Brice Ball, What Do You Do, All the News,
Detective, Lazy Gun Cult, Drom Bennacht, Burglary, Sleepover, Gaiaonline Q&A,
Digimon, Easter Day, Lands of Unknown, Incident Undead 2, Incident Undead 3,
Time Rift, Hungry Goblin, Enterprise.

Several of these still differ from Question earlier in the transcript (an
unknown verb the route happened to recover from); the marker is all this
checks.

### Timing

Wall-clock time of the two headless runners over the same 150 rows, one run
each, sequentially, both built `-O2`, on this Mac, including process start-up
and loading:

| | Question | Geas |
|---|---|---|
| All 150 rows | 3.3 s | 8.7 s |
| The 30 rows both complete | 0.22 s | 0.33 s |
| Slowest row (The Maze) | 0.96 s | 4.1 s |

Where both finish, Question is ahead by about a tenth of a second across 30
games, which is small enough to call level. The gap in the total comes from
the 120 rows where Geas does not finish: it keeps working through commands
it can't answer, and a couple of games spend a long time in its slower
condition and script paths. This is not a fair speed benchmark of the
engines; it is the cost of running the corpus. Both runners were measured
again on 2026-10-12, after the synonym fix in `663a08009`; Geas is unchanged,
and its figures moved only because the machine was quieter.

## Reproducing

The adapter and the row list are not in the repository. They were built
for this comparison in a scratch directory: `legacy_runner.cc`, which
`#include`s legacy `geas-util`, `istring`, `readfile`, `geasfile`,
`geas-state` and `geas-runner` from `git archive 513b0f4ff terps/geas`
(with the one `$rand` line in `geas-runner.cc` patched to call
`erkyrath_random()` and linked against `common_utils/randomness.c`), and
a loop over the `play` lines of `run_walkthroughs.sh`. Rebuilding it takes
an afternoon; ask if you want it committed under `test/quest4/harness/`.
