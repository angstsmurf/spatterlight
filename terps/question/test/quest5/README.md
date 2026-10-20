# Quest 5 (`.aslx`/`.quest`) tests

The aslx engine's test tree. Mirrors `../quest4/`, but the shape of the testing
is different: Quest 5 has a runnable ground truth. QuestViva, the cross-platform
.NET port of the real Quest 5 engine, is built into a headless driver (`qvh`)
and its transcripts *are* the goldens, so a corpus game is checked by
byte-diffing our output against what real Quest prints rather than by looking
for a win marker.

This file also records what the engine's long build-up (July to October 2026)
settled: where the code is, which behaviours are deliberate, what was declined
and why, and the safety net for the next change. The day-by-day log that used
to live in `quest5/TODO-quest5.md` is in the git history of that file.

## Layout

- `fixtures/` — hand-written `.aslx`/`.quest` files. Ours, and committed. The
  loader and runtime tests open them by name; a few are for driving by hand
  (see below).
- `goldens/` — the answer keys, `<Game>.cmd` (the frozen command script) plus
  `<Game>.txt` (the QuestViva transcript it produces), 374 games. Both halves
  are committed; the `.cmd` is what everything replays, so a diff here is a
  change in what "real Quest does".
- `harness/` — the native harnesses, plus `harness/oracle/`, the QuestViva
  driver that produces the goldens. Binaries build in `harness/` and are
  gitignored.
- `games/` — the `.quest` corpus: 377 game files, 374 of them wired to a
  golden, the rest duplicate releases and games with no route yet.
  **Untracked** (`../.gitignore`), like `quest4/games/` — third-party game
  data is never committed. The oracle scripts use it automatically when it is
  here, and fall back to `~/Downloads/Quest 5 games` when it is not, so a
  machine that keeps the corpus elsewhere still works (or set `GAMES=`).
- `games.manifest.tsv` — the committed sha256 pin for every file in `games/`,
  with its source URL where one exists. This is what makes the corpus
  reproducible without redistributing it; see `../GAMES.md` and
  `../fetch_games.sh`.
- `CORPUS.md` — the prose companion to that manifest: the deliberate version
  duplicates, the unwired extra copies, the files renamed to their in-game
  titles when they were wired, and sourcing facts.
- `downloaded/` — the same arrangement for the third-party walkthrough
  documents `extract_walkthrough.py` reads, falling back to
  `~/Downloads/Quest 5 walkthroughs` (or set `WALKS=`). Only the
  extractor-driven corpus rows need them (27 of 374); every other row is
  driven by a curated script in `harness/oracle/overrides/`, which is
  committed.

## Where the code is

Everything is under `terps/question/quest5/`, one translation unit per file:

- `aslx.cc` / `aslx.hh` — the loader: the in-memory zip reader that pulls
  `game.aslx` out of a `.quest` package, the expat SAX parse, `<include>`
  resolution (game directory first, then the bundled Core, then
  `Core/Languages`, QuestViva's `GetLibraryStream` order), load-time
  `[template]` substitution, and the `aslx_is_quest5_file` sniff that
  `glk_main` dispatches on (a zip holding `game.aslx`, or XML whose root is
  `<asl`).
- `aslx-runtime.cc` — the interpreter proper (`Interp`): fields and
  inheritance, `changed<attr>` notification, the UndoLogger port, timers, the
  fire-and-forget input model (`get input` / `show menu` / `ask` / `wait`),
  the `request (...)` and `JS.*` dispatch, and the host hooks the frontends
  fill in. `aslx-runtime-parse.cc` is the script and expression parser,
  `aslx-runtime-builtins.cc` the built-in function table,
  `aslx-runtime-internal.hh` what the three share and `aslx-runtime.hh` the
  public face.
- `aslx-state.cc` — the engine's own save format (v1 snapshot), used by
  `save`/`restore` and by the Spatterlight autosave.
  `aslx-savenative.cc` reads and writes Quest's own `.quest-save` (a port of
  `GameSaver`), so a save from desktop Quest or QuestViva loads here and ours
  loads there.
- `aslx-core/` — the non-editor Core libraries, copied verbatim from the
  QuestViva checkout (MIT); `aslx-core/README.txt` has the refresh recipe.
  One version is enough: Core branches on the game's `<asl version>`. The
  loader takes the directory from `load_file` or `$ASLX_CORE`; the app points
  it at the resource bundle.
- `aslxglk*.cc` — the Glk frontend, all in `namespace aslxglk`:
  `aslxglk.cc` the session loop and prompt, `-output.cc` the HTML-subset to
  Glk-style renderer and the command links, `-panes.cc` the side pane
  (compass, inventory, places and objects, status attributes),
  `-input.cc` the menu/question/wait prompts and the bundled-JS callback
  index, `-media.cc` pictures and the one sound channel, `-map.cc` /
  `-form.cc` the grid map, `-autosave.cc` the Spatterlight autosave blob.
  `aslxglk-internal.hh` holds the types, prototypes and every global.
- What both engines share is in `../questglk-common.cc` and `../qstr.hh`;
  `../README.md` lists it.

Environment knobs the engine and harnesses read: `ASLX_CORE` (Core
directory), `ASLX_SEED` (overrides the 1234 default; never set it under a
regression), `ASLX_TRACE_RAND` and `ASLX_TRACE_CALLS` (engine traces),
`ASLXGLK_JS_TRACE` (dumps the bundled-JS callback index and each `JS.*` call
the frontend bridges), `ASLXGLK_PROMPT_FIRST` (forces the prompt-first manual-echo mode under a
piped harness); and on `aslx_replay`, `ASLX_RAW` (mirrors each print's raw
HTML to stderr), `ASLX_GRID_TRACE` (grid-map paint commands;
installing the hook changes what is replayed, so never under a regression)
and `ASLX_RESTORE=<save>` (boot from a save instead of `StartGame`).

## Automatic tests (`make check`, from `../`)

| binary | covers |
| --- | --- |
| `harness/aslx_loader_test` | the `.aslx`/`.quest` loader — XML, includes, the zip container |
| `harness/aslx_runtime_test` | the script/expression runtime, undo, typed lists, RNG stream capture, save/restore of native state |
| `harness/aslxglk_link_tests` | the Glk frontend's HTML link parser (`link_action`, in `aslxglk-output.cc`, declared in `aslxglk-internal.hh`) |

All three open their fixtures as `../fixtures/<name>`, so run them from
`harness/`:

```sh
cd harness && ./aslx_runtime_test
```

`aslxglk_link_tests` exists because CheapGlk reports no hyperlink support, so
the smoke harness below never registers or clicks a link and cannot reach that
code at all.

`make check` is deliberately corpus-free: it must pass on a machine without
`games/`. The corpus sweeps below are separate targets for that reason.

## Corpus regression (`harness/oracle/check_golden.sh`)

Replays every frozen `goldens/<Game>.cmd` through QuestViva and diffs the result
against `goldens/<Game>.txt`. A FAIL means the *oracle* drifted — a QuestViva
upstream change, a .NET/RNG regression, a `Program.cs` edit — not that our
engine is wrong. It also cross-checks `oracle/corpus.tsv` against the goldens in
both directions, so a driven row with no golden, or a golden with no row, is an
error rather than a silent gap.

```sh
cd harness/oracle
./build.sh                     # clone + build QuestViva (outside the repo)
./check_golden.sh              # the whole corpus, in parallel
./check_golden.sh "hobbit"     # just the matching games
./update_golden.sh             # re-freeze after an INTENDED change
```

The oracle is pinned to QuestViva v6.0.0+69 (`290a87df`, since 2026-10-17);
`build.sh` checks that commit out and `patch_questviva.py` applies the
determinism patches (`ErkyrathRandom.cs` replaces .NET's `Random`, with
QuestViva's own per-expression streams kept); `Program.cs` is the driver,
with the `DrainTimers` model both sides share. `oracle/README.md` is the long-form document: how a game gets
wired, what the extractor modes mean, what each curated override in
`oracle/overrides/` deviates from, and the full account of every expected
native diff.

## Native replay (`make replays`, `harness/run_replays.sh`)

The other half of the same regression: our engine against the same frozen
scripts. `run_replays.sh` is the mirror image of `oracle/check_golden.sh` over
exactly the same two files — it replays every `goldens/<Game>.cmd` through
`aslx_replay` and diffs against `goldens/<Game>.txt` — but a FAIL here means
*we* diverged from what real Quest prints, not that the oracle drifted. It
builds the binary itself, fans out across cores, and prints the head of each
failing diff (golden on the left).

```sh
make replays                           # from ../: the whole corpus
cd quest5/harness
./run_replays.sh acreage hobbit        # just the matching games
WORK=/tmp/replays ./run_replays.sh     # keep the full transcripts
```

Games with a golden but no game file are `MISS`, not `FAIL`; a run that
replayed nothing at all exits non-zero rather than reading green. `GAMES=`,
`JOBS=` and `DIFF_LINES=` override the corpus directory, the fan-out and the
diff excerpt.

**The expected result is 364 passed, 7 failed** (2026-10-17). The seven are
documented deviations, each with its diff size, and any *other* line in their
diffs is a regression:

| row | lines | why |
| --- | --- | --- |
| The Acreage (pub 6.29 revision) | 5 | Quest Viva #2189: the oracle prints grid errors on arriving in an uncharted room; we chart the room (`chart_uncharted_room`) |
| Woo Rebooted 3.7 | 7 | same |
| Thyme (ASL 600 build) | 5 | same (the arrest teleport at the end) |
| GiantKiller Too 2.15 | 45 | same, five arrivals |
| Rogues Adventure | 54 | the `void` room's message is a glitched source dump the oracle rejects as an expression; we print it as text |
| Heliacal Hostility | 3 | an attribute named `in jungle`: QuestViva says "Invalid attribute name", we accept it (unresolved, not a decision) |
| TabblewoopGOTY | 21 | `put intragate in intragate`: refused by every engine since 2012, allowed for this one game by `gameid` (`allows_self_parent`) so it can be finished |

A single game by hand, which is also how the sweep invokes it:

```sh
make quest5/harness/aslx_replay                        # from ../
cd quest5/harness
ASLX_CORE=../../../quest5/aslx-core ./aslx_replay \
    "<game.quest>" "../goldens/<Game>.cmd" | diff "../goldens/<Game>.txt" -
```

`aslx_replay` installs its menu/ask/input providers *before* `StartGame`, so
an expression-form `Ask` at boot is fed the way the oracle feeds it. A `.cmd`
may carry a `#!clock=N` header, which makes both drivers advance a typing
clock of N seconds per command instead of draining timers.

`harness/aslx_linkcapture` replays a script the same way but prints the visible
*text* of the command link each step clicks, and
`harness/oracle/save_compat.sh` cross-tests `.quest-save` compatibility in both
directions (we load a save QuestViva wrote; QuestViva loads a save we wrote),
using the first half of each golden script as the setup.

`run_replays.sh` has no per-game timeout; wrap a suspect game in
`perl -e 'alarm N; exec @ARGV' ./aslx_replay ...` (macOS has no `timeout`).

## Glk frontend smoke (`harness/aslxglk_smoke`)

The real frontend (`quest5/aslxglk*.cc`) against the in-repo CheapGlk, driven by piping
commands to stdin. Built on demand; useful for anything that only goes wrong
once the frontend is in the loop.

## Autosave (`make autosave`, from `../`)

`../autosave/run_autosave_tests.py` drives the real terp through
`../glkdrive.py`, a fake Spatterlight over the glkimp protocol, and checks that
quitting at a prompt and relaunching gives the same transcript as playing
straight through. The Quest 5 cases (`q5-*`) use `../autosave/probe5.aslx`
and a few corpus games when `games/` is present; they cover every command,
the RNG streams, attribute writes, undo and timers. Needs the Spatterlight
build (the autosave code is `#ifdef SPATTERLIGHT`); `--build` runs
`xcodebuild` first.

## Manual fixtures (not run by any script)

A few things only exist on a graphical Glk host, so no in-repo harness can
assert on them — CheapGlk has neither graphics nor hyperlinks. These are driven
**by hand in Spatterlight**, and each carries a header comment listing the
commands to type and what to look for.

| fixture | checks |
|---|---|
| `fixtures/framepicture.aslx` | the frame-picture band: opens, sizes itself to the picture without upscaling, and — via **both** the `SetPanelContents` and Quest 5.0 `RunScript` channels — closes again leaving no gap |
| `fixtures/timertest.aslx` | the prompt-retract/preload path in `aslxglk`'s `read_line` timer arm: a recurring wall-clock timer printing while the parser prompt is up must leave no stranded `> ` above the tick lines |

They are kept here rather than in a scratch directory because the paths they
cover are otherwise reachable only by finding a shipped game that happens to use
them: the picture-clear path went unverified through two commits for exactly
that reason.

One fixture in the same directory is *scripted*, not manual:
`fixtures/clockchase.aslx` with `clockchase.cmd` (a `#!clock=5` script) and
`clockchase.txt` (its frozen oracle transcript). It packs both real-time chase
shapes that kill a script under the default `DrainTimers` model into three rooms
(see the file's header comment for the two diff commands), and both drivers must
reproduce the transcript byte for byte. It is not part of the corpus sweeps; run
it by hand after touching `SettleClock`/`settle_clock` or the timer code.

## Behaviour settled while the engine was built

These are decisions, not accidents. Each was checked against QuestViva's
source or a live run; reverse one only with a golden that shows why.

- **Determinism.** `GetRandomInt`/`GetRandomDouble` draw from a
  xoshiro128**+SplitMix32 port of `erkyrath_random()` (`aslx::Rng`, seed
  1234), byte-identical to `terps/common_utils/randomness.c` and to the
  oracle's `ErkyrathRandom.cs`. Like QuestViva, every compiled expression
  owns a lazily seeded stream, so a draw in one script does not move
  another's; the fallback stream serves the rest. Under `SPATTERLIGHT` the
  app's own `erkyrath_random()` is used. An autosave captures every stream
  (`Interp::capture_rng_streams`, including the statement parser's roots)
  and restores them after `InitInterface` has drawn, so the restored prompt's
  randomness is the saved one.
- **Lists and dictionaries are reference types** behind a shared backing, so
  copies alias and `list add` propagates, as QuestList does; entries are
  full typed values; collections compare by identity. `list remove` takes
  the first occurrence only, `ListExclude` all.
- **Transactions are lazy.** Core's `start transaction` per parsed command
  commits the previous one; nothing commits at end of turn; boot is never
  logged. `undo` rolls back one transaction, quirks included (undo, command,
  undo, undo rolls the same transaction back twice, the second time
  forward), as in `UndoLogger.cs`.
- **Quest 5 undo history does not survive an autosave**, and cannot
  faithfully: the logged actions hold live runtime references with no
  process-independent identity, and QuestViva saves carry no undo history
  either (after any load, `undo` has nothing to undo). Quest 4's flat
  snapshot ring does survive; the asymmetry is deliberate.
- **An autosave is taken only at the parser prompt**, after the prompt is
  printed and before input is requested, and after a state-changing timer
  tick. The object verb menu in the side pane is part of the snapshot
  (the click starts no script, so the world under it is clean) and a relaunch
  resumes into it; the expression forms of `show menu`/`ask` block inside
  `eval` with a half-run script on the stack, are unrestorable, and suspend
  autosave while up. A pane fold refreshes the snapshot even though it
  passes no turn. A looping sound is resumed by the app from its own channel
  archive, never replayed by the terp; the terp only keeps the staged sound
  file alive in glkimp's per-session temp directory.
- **Input is fire-and-forget**, QuestViva's model rather than classic
  Quest's blocking thread: `get input`/`show menu`/`ask`/`wait` register a
  callback and the next input resolves it, so the Glk prompt loop needs no
  nested `glk_select`. The pre-540 `request (Wait)`/`request (Pause)` are
  the exception: genuinely blocking host hooks, and a silent no-op when the
  host leaves them unset (headless), which is byte-identical to the oracle's
  AutoAdvance.
- **Headless never blocks.** Every host hook (`grid_draw`, `play_sound`,
  `do_wait`, `request_restart`, `show_picture`…) is null in `aslx_replay`
  and behaves like the oracle's immediate advance; `JS.eval` is ignored
  with its argument unevaluated.
- **`firsttime` is per attribute**, not per script text: the compile cache
  is keyed by owner, attribute and source, so twenty pages with the same
  `firsttime { ... }` each fire once. `invoke` stays unscoped and does not
  bind `this`.
- **Templates substitute at load time**, later registrations winning and
  unmatched `[refs]` left verbatim; `simplepattern` attributes are exempt
  and convert at parse time, as `CommandLoader` does.
- **`Chr` above 127 is Windows-1252** and NCalc list literals beyond
  `in`/`not in`/`foreach`/`ListCount` are ours; both are logged as
  deviations in `oracle/README.md`.
- **The grid map canvas is the game's background colour** (`setBackground`
  paints `#gridPanel` in the reference player), fed by both `JS.setBackground`
  and the Quest 5.0-era `request (Background)`. Without it a black-background
  game draws its exits white on white.
- **Bundled JavaScript is indexed, not executed.** `index_bundled_js()`
  reads the `<javascript src>` files out of the package and records, per
  function and transitively, the `ASLEvent` completions its body would fire,
  so a `JS.act0Clear()` whose animation ends in an event still moves the
  game on. A computed callback name is not indexed.
- **Legacy dispatch.** `.asl`/`.cas` go to the Quest 4 engine, never
  through Quest 5's converter; `QCGF001` stays unsupported.
- **A self-parent is refused** with QuestViva's message for every game
  except TabblewoopGOTY (above). An uncharted-room arrival is charted
  rather than erroring (the #2189 rows above).

## Declined, with the reason

- **`<backgroundimage>`** (`SetBackgroundImage` → `JS.SetBackgroundImage`).
  A Glk text buffer has no background image; the call falls through
  `js_fallback` unharmed. The grid map already takes the background
  *colour*. Nothing in the corpus depends on it.
- **Libraries bundled inside a `.quest` zip.** Only `game.aslx` is pulled
  out; a game-local `.aslx` in the package would not resolve. A scan of all
  372 corpus packages found no such file (every game inlines its libraries at
  publish time), so there is nothing to test against.
- **Locked inherited collections.** QuestViva throws "Cannot modify the
  contents of this list as it is defined by an inherited type" when a script
  mutates a type-provided list without assigning it first; we copy on write
  silently. No golden contains the message.
- **`TypeOf` on a `QuestList<object>`** returns "list" in QuestViva; we have
  no untyped list, so ours says "stringlist" or "objectlist". No corpus
  game branches on it.
- **A JavaScript engine.** Games built around custom JS UIs are out of
  scope; `JS.eval` warns once and is dropped, and the bundled-JS index above
  covers the completion callbacks that matter.
- **Timeout per game in `run_replays.sh`.** Deeper's hang was an engine bug
  (`GetExitByName`), now fixed; a wrapper does the job when needed.

## Safety net for the next change

1. `make check` from `../`: loader, runtime and link-parser tests plus the
   Quest 4 fixtures. Corpus-free.
2. `make replays`: 367 passed, 7 failed with exactly the diffs in the table
   above. An eighth row, or a new line inside one of the seven, is a
   regression.
3. `harness/oracle/check_golden.sh` when a golden changes or the oracle
   is rebuilt: 374/374, else the oracle moved and the native sweep is
   measuring against a stale baseline.
4. `make asan` for anything touching the runtime or the loader; the corpus
   has been clean under ASan/UBSan since the engine landed.
5. `make autosave` after touching the frontend's prompt, menu, timer or
   snapshot code; `fixtures/clockchase.*` by hand after touching the typing
   clock.
6. `harness/oracle/save_compat.sh` after touching `aslx-savenative.cc`.
7. The two manual fixtures in Spatterlight after touching the frame picture
   or the timer arm of `read_line`.
