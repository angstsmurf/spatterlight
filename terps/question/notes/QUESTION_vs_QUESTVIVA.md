# Question vs. QuestViva

This compares Question, the Quest interpreter in Spatterlight (`terps/question`,
binary `question`, both its Quest 2–4 and its Quest 5 engines), with QuestViva,
the .NET successor to Quest 5 from which it takes its reference behaviour.
Quest 6 / QuestJS is out of scope for both.

"QuestViva" means `textadventures/quest` at the revision the oracle is pinned
to: `290a87df`, v6.0.0 plus 69 commits (2026-10-14), with the local
oracle patches described below. The corpus figures were last checked on
2026-10-17, after the move to that revision; the timings are from 2026-10-12.

This is not a comparison of equals. QuestViva *is* Quest's own engine going
forward, and Question is a from-scratch C++ engine for playing Quest games in
Spatterlight that uses QuestViva as its answer key. Most of the corpus numbers
below say how closely Question matches that key, not whether the key is right.

## In short

| | QuestViva | Question |
|---|---|---|
| What it is | Quest's engine, editor and players | A player only: a C++ engine behind Spatterlight's Glk front end |
| Quest 2–4 (`.asl`, `.cas`) | `V4Game.cs`, a line-by-line C# translation of the VB6 Quest 4 | Own engine, checked against `V4Game.cs` |
| Quest 5 (`.quest`, `.aslx`) | The reference | Own engine plus QuestViva's Core libraries (from the pinned revision), checked against it |
| Quest 4 corpus rows identical to the other (of 150) | – | 139 (11 differ, all catalogued) |
| Quest 5 corpus rows identical to the oracle (of 354) | – | 348 (6 differ, all documented) |
| Editor, game publishing | Yes | No |
| Where it runs | Windows, macOS, Linux (Electron), browser (WebAssembly and server) | macOS, inside Spatterlight |
| Interface languages | 18 language libraries, plus editor translations | Bundles QuestViva's 15 Languages files; no editor |
| Language | C# on .NET 10 | C++17, no runtime |
| Size | about 21,500 lines of engine and 18,000 of Quest 4 engine in C#, plus editor and players about 14,000 | about 18,000 lines for Quest 5 (engine and Glk front end), about 19,000 for the Quest 4 engine and its front end |
| Time for the same corpus | 35.7 s for 150 Quest 4 rows; 99.3 s for 63 Quest 5 rows | 3.0 s and 9.8 s |
| Licence | MIT | GPL 2+ (Question's own files; QuestViva's Core `.aslx` are bundled under MIT) |
| Maintained | Yes, about 4,200 commits by its main author | Yes |

## Where QuestViva is better

- **It is the reference.** Question has no authority of its own about what a
  Quest game should do; the Quest 5 goldens are QuestViva's output and the
  Quest 4 comparison runs the same scripts through its `V4Game`. Where the two
  disagree on Quest 5 the golden is the answer key.
- **It has an editor.** Writing and publishing games is there. Question only
  plays them.
- **It runs everywhere.** The same engine has a web player, an in-browser
  WebAssembly player and an Electron app for the three desktop systems.
  Question is a Spatterlight terp.
- **Multiple languages and a real test and release process.** 18 language
  libraries, a `CHANGELOG`, release automation, and about 310 MSTest methods
  plus Playwright browser tests.
- **Full JavaScript.** Quest 5 games can call arbitrary JS and embed HTML
  and CSS; the browser runs it. Question implements the `JS.*` calls the
  corpus and Core use natively (addText, grid map, pane control), not a
  JavaScript engine, so a game that does something unusual with its page
  cannot be reproduced.
- **Actively developed by the people who own the format.** Question follows
  upstream: the oracle pin moved from beta.57 to rc.4+9 on 2026-10-03, and the
  bump needed engine work on our side (inline `ask`/`show menu` prompts in
  ASL 600 games, the turn-boundary rewrites). It moved again to v6.0.0+69 on
  2026-10-17, with the bundled Core libraries re-copied from the same
  revision (they had been about five months older than the pin). That move
  changed no golden and no replay; the one engine change it called for was
  the inventory pane leaving out `hidefrominventory` objects (#2516). Of the
  other post-6.0.0 engine fixes, Question already behaved like the fixed
  QuestViva for a handler with no parameters (#2524) and needs no locale
  fix (#2501), since it never formats numbers through a locale.
- **Features Question does not have.** `GetExternalFileData`/
  `SetExternalFileData` are unsupported and `<backgroundimage>` is skipped
  (see `test/quest5/README.md`). `play sound` plays through one Glk sound
  channel in Spatterlight and only warns in the headless harnesses.

## Where Question is better

- **It plays inside Spatterlight.** Native text buffers, the Glk side pane
  with hyperlinked objects and exits, the grid map, status variables in the
  banner, media, a transcript, VoiceOver, and the app's save/restore.
  Autosave and autorestore work on both engines (`test/autosave`).
  It also reads and writes native `.quest-save` files.
- **It is small and quick to start.** Median time for one corpus game:
  7 ms for a Quest 4 game and 73 ms for a Quest 5 game, against 167 ms and
  0.85 s for the oracle. Most of the oracle's time is .NET start-up and JIT,
  so the comparison is about running a single game, not about engine speed.
  See [Timing](#timing).
- **It makes solutions for large games practical to derive.** Finding a
  route through a long game means replaying the commands so far over and
  over: from a checkpoint after a lost fight, or with each candidate tried in
  turn. Each of those replays takes a fraction of the time on Question. The
  solver drivers (`test/quest5/harness/oracle/solvers/`) play on
  `aslx_replay`'s live mode (`GE_DRIVER=native`), with commands on stdin and
  `eval:` probes of game state. That is about 6x faster than driving qvh, and
  it is what made the 3845-command full win of *The Quest for the Golden Egg*
  and the *Rise of Death* route feasible. QuestViva stays the reference: a
  finished script is replayed on qvh before it is frozen as an override.
- **No runtime.** It is a single native binary with libexpat and zlib.
- **Deterministic by design.** One generator (`erkyrath_random`, xoshiro128**)
  seeded by `QUESTION_SEED` or Spatterlight's determinism mode makes every
  walkthrough in the corpus replay byte for byte. The oracle needed a patch
  (`ErkyrathRandom.cs`, plus a per-expression generator) to get the same.
- **It found defects in its reference.** The Quest 4 comparison found
  QuestViva's `V4Game` to be wrong in a few corners: a VB6 `Type`
  (value) translated as a C# `class`, so index 0 read as `null` (patched to
  restore the old behaviour; 57 identical transcripts became 65), and a null
  dereference (`V4Game.cs:7179`) when a game loads the `TLTclothing` type or
  one of the garment types built on it, which kills `V4Game` at load while
  Question plays the game. The first is patched in the oracle, the second is
  not. The oracle's tick placement was also moved to where Quest ticks.
- **It plays a few games QuestViva will not finish.** *TabblewoopGOTY* needs
  `put intragate in intragate`, which a guard added to Quest in 2012 refuses
  and the game predates; Question allows it for that game only. *Heliacal
  Hostility* uses an attribute name with a space that QuestViva rejects.
  Question also charts rooms that QuestViva's grid map errors on (#2189).

## Where they agree and where they do not

### How this was measured

- **Quest 4 (150 rows).** Every game in `test/quest4/harness/run_walkthroughs.sh`
  is replayed from the same command script at the same seed through QuestViva's
  `V4Game` (`test/quest4/harness/oracle/compare.sh`) and through Question's
  committed golden. Both are normalised (runs of spaces, blank lines, the
  title banner), then diffed.
- **Quest 5 (354 rows).** The goldens in `test/quest5/goldens` are QuestViva's
  own transcripts of the command scripts (`check_golden.sh`). Question replays
  the same scripts natively (`run_replays.sh`) and the result is diffed. A pass
  means the C++ engine printed what QuestViva printed.

### Quest 4: 139 of 150 identical

| First divergence | Lines | Games |
|---|---:|---:|
| Desync after a trimmed declared name (finding 37) | 338 | 1 (Metal Sonic's Quest) |
| Deliberate: lone command pattern trimmed (finding 80) | 35 | 1 (Gaiaonline Q&A) |
| Deliberate: pre-2.80 `place` fencepost | 6 | 1 (Magic Sword) |
| Unclassified | 6 | 2 |
| Pre-2.80 `look` name lookup (finding 62) | 4 | 1 |
| `<ERROR>` not produced at load time | 4 | 2 |
| Trimmed declared name | 2 | 1 |
| Others (SirLoin, BladeSentinel, Mysts and a few smaller classes), about one line each | 5 | 5 |

About 398 differing lines in total, 338 of them from one desync. The deliberate rows
are a policy: where Quest is plainly harmful to the game's own text (a mangled
room name from a fencepost, throwing away an answer the author wrote) Question
keeps the right behaviour and carries the diff. `FINDINGS.md` has findings numbered to about 82
behind this table.

### Quest 5: 348 of 354 identical

All six failures are understood and none is a regression:

| Game | Diff lines | Why |
|---|---:|---|
| The Acreage (6.29 revision) | 5 | QuestViva issue #2189: an uncharted room makes the CoreGrid code raise errors in the oracle; Question charts the room (`chart_uncharted_room`), deliberately |
| Woo Rebooted 3.7 | 7 | Same |
| GiantKiller Too 2.15 | 45 | Same |
| Rogues Adventure | 54 | A malformed message in the void room: the oracle reports one script error (`Invalid token in expression`), Question prints the dump as text |
| Heliacal Hostility | 3 | An attribute named `in jungle`: QuestViva says "Invalid attribute name", Question accepts it. Unresolved; desktop Quest untested |
| TabblewoopGOTY | 21 | `put intragate in intragate` is refused by the guard QuestViva and desktop Quest 5 have had since 2012; the game was saved before it. Question allows it for this game's id, so the route reaches the end |

### What the numbers do and do not show

- **Question's Quest 5 "correct" is QuestViva's output.** 348/354 says
  Question is faithful; it says nothing about whether QuestViva is right, and
  a bug QuestViva has appears as a bug Question reproduced on purpose. The
  #2189 rows are the exception: there the oracle is wrong and Question
  differs deliberately.
- **The oracle is patched.** To make the runs repeatable the QuestViva clone
  carries local patches (`patch_questviva.py`): a per-expression generator
  (`ErkyrathRandom.cs`, byte-identical to the C one), `SetRandomSeed`, the
  invariant-culture pin (so a double does not print as `100,4`; upstream's
  #2501 fixed only concatenation inside an expression, and `msg (x)` of a bare
  double still goes through the machine's culture in
  `Utility.ExpressionResultToString`; the Quest 4 oracle pins it too, since
  #2502 fixed numeric storage but `Conversion.Str` still prints a negative
  with U+2212 under sv_SE), an error limit
  hook (`QV_ERROR_LIMIT`), and the `V4Game` Type-as-class fixes.
  "QuestViva" in the tables is QuestViva plus those patches.
- **The Quest 4 goldens are self-goldens.** They freeze what Question printed.
  Only the oracle replay ties them to Quest.
- **Corpus selection.** The Quest 5 walkthroughs were derived against
  QuestViva, so they are routes it can complete. The Quest 4 routes were
  written against Question's runner (with `QUESTION_STRICT_NAMES` set) and
  then replayed through the oracle; some were reworded after that.
- **The oracle itself was wrong in places.** Three VB6 value-type
  translation slips and a mis-placed tick accounted for thousands of
  differing lines that were oracle defects, not Question's. A ground-truth oracle that is wrong
  in a corner is worse than none there, because the diff looks like evidence.
- **Translations of the same source.** Question's Quest 5 Core libraries are
  QuestViva's, copied verbatim from the pinned revision and re-copied whenever
  the pin moves, so both engines run the same Core; the engine underneath is
  Question's own.

## Timing

Wall-clock time, each game run once, sequentially, on this Mac. The oracle is
`dotnet qv4.dll` / `dotnet qvh.dll` (JIT, .NET 10); Question is the headless
native runner (`question_walkthrough_runner`, `aslx_replay`, both `-O2`).
Process start-up and loading are included.

| | Question | QuestViva |
|---|---:|---:|
| Quest 4, all 150 rows, total | 3.0 s | 35.7 s |
| Quest 4, median row | 0.007 s | 0.167 s |
| Quest 4, slowest row | 0.82 s (The Maze) | 1.16 s (Shipwrecked) |
| Quest 5, 63 games, total | 9.8 s | 99.3 s |
| Quest 5, median game | 0.073 s | 0.85 s |
| Quest 5, slowest game | 3.37 s (A Stranger, Unregarded) | 21.3 s (same) |

- Question was faster on every one of the 213 games.
- The Quest 5 figure is the first 63 games of the corpus in alphabetical order,
  not all 354. The full run would have taken about an hour; I stopped it.
- Both sides were measured again on 2026-10-12, after the profiling fixes in
  `663a08009` and `b5ee7d054`, in one session on a quieter machine. The
  oracle did not change; its figures are lower than the first measurement
  only because of the machine.
- Most of the oracle's time per game is the runtime starting up (the median is
  a sixth of a second even for a short Quest 4 game). This measures the
  cost of running the corpus through each harness, not engine throughput.
- The 21 s outlier is the same game on both sides, so it is the game's own
  workload (a long walkthrough with heavy turnscripts), not start-up.

## Where the work stands

- Open: nothing on the Question side (`<backgroundimage>` is declined in
  `test/quest5/README.md`), and the oracle fork's `oracle-patches` branch (not regenerated since the pin
  move, outward-facing so not pushed).
- Both projects are developed with AI assistance; QuestViva's tree has a
  `CLAUDE.md`.
- Question's `quest5/aslx-core/` is QuestViva's Core library copied verbatim
  from the pinned revision (last refreshed 2026-10-17, from `290a87df`) and
  stays MIT-licensed; its `README.txt` records that and how to refresh it.
- The oracle clone (`~/questviva-oracle/questviva`) is outside the repo and is
  built by `test/quest5/harness/oracle/build.sh`; the scratch timing scripts
  were not kept in the repo.
