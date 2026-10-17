# Question tests

Two engines live in `terps/question`, and the test tree is split the same way, with
mirrored subfolders in each half (the layout `terps/scarier/test` uses):

| | |
| --- | --- |
| [`quest4/`](quest4/README.md) | Quest 4 (`.asl`/`.cas`) — the Question engine |
| [`quest5/`](quest5/README.md) | Quest 5 (`.aslx`/`.quest`) — the aslx engine and its QuestViva oracle |

Each half holds `fixtures/` (small hand-written games that are ours, committed),
`goldens/` (the committed answer keys), `harness/` (the tooling), an untracked
`games/` for the third-party corpus, and the `games.manifest.tsv` that pins it.
Only engine-agnostic things stay at this level: this README,
[`GAMES.md`](GAMES.md) and `fetch_games.sh` (the corpora, for both engines), the
`Makefile` that builds both halves, `questglk_unit_tests.cc` (the helpers both
Glk frontends share, `../questglk-common.cc`), `glkdrive.py` (a fake
Spatterlight app that drives the built terp over the glkimp protocol) and
`autosave/`, the one suite that spans both engines (below). `glkdrive.py` is
terp-agnostic and is the same file as `terps/scarier/test/glkdrive.py`, kept
byte-identical so each tree stays self-contained; `make check` fails if the
two copies drift, so edit one and copy it over the other.

## Build

```sh
make            # build every in-repo harness, both engines
make syntax     # per-file syntax check of every engine source (see below)
make check      # syntax, + the unit tests, + the Quest 4 fixture games
make asan       # same under AddressSanitizer/UBSan -- see quest4/README.md
make clean

make gamescheck # verify both game corpora against their manifests
make gamesfetch # download whatever of them is still online

make autosave   # Spatterlight autosave/autorestore, both engines (Xcode build)
```

Everything `make check` runs is self-contained — no game corpus — so it is the
part worth wiring into CI. The harnesses build beside their own sources
(`quest4/harness/`, `quest5/harness/`) and are gitignored; those that open a
fixture at run time resolve it relative to their own directory, so run them from
there (`cd quest5/harness && ./aslx_runtime_test`), which is what the Makefile's
own rules do.

## Per-file syntax check (`make syntax`)

The Quest 4 harnesses here unity-include the engine: `question_walkthrough_runner.cc`
`#include`s every engine `.cc` file (through `headless_common.hh`), so they become **one** translation unit.
That means a function defined in one `.cc` and called from another compiles
cleanly even when no header declares it — the caller simply sees the earlier
definition. The Xcode build compiles those files separately and rejects it.

This is a real failure mode, not a hypothetical one: a missing `starts_with_i`
prototype in `question-util.hh` passed the fixtures, the unit tests *and* all 111
walkthroughs, and broke the app build. `make syntax` runs `-fsyntax-only` over
each source on its own — including `questionglk.cc`, the `quest5/aslxglk*.cc` frontend files and
`questionglkterm.c`, which no harness here compiles at all — in a few seconds, and
is a prerequisite of `make check`.

## Spatterlight autosave (`autosave/run_autosave_tests.py`)

Everything above runs the engines through the in-repo harnesses and CheapGlk.
The autosave and autorestore code never gets there: it is compiled only into
the Spatterlight build of the terp (`#ifdef SPATTERLIGHT`,
`../questionglk-autosave.mm` and the `ASLXGLK-AUTOSAVE` blob in
`../quest5/aslxglk-autosave.cc`) and talks to `libglkimp`. This suite tests it the way
the app exercises it. `glkdrive.py` plays the app: it launches
`build/Debug/question` with the library, answers the protocol, feeds scripted
input, and ends each session with the window-closed event (`EVTQUIT`), which is
the exit that keeps the autosave, so the next launch of the same game
autorestores.

```sh
make autosave                                   # builds the terp, runs all cases
python3 autosave/run_autosave_tests.py -v       # existing build; -v prints diffs
python3 autosave/run_autosave_tests.py q5-      # name filter
python3 autosave/run_autosave_tests.py --terp path/to/question
```

The check is equivalence, not goldens: a script played in one session must
print, command for command, what it prints when the process is killed and
relaunched after chosen commands, with determinism on. Only the main text
window is compared, because the status line and the side pane are repainted at
boot on a relaunch. Every relaunch must also open and delete the same number
of windows (the boot windows are replaced by the restored ones) and print
nothing before its first input. The autosave directory is the app's
(`~/Library/Application Support/Spatterlight/Quest Files/Autosaves/`), under a
`question-autosave-test-*` signature that is created and removed per case.

The probe games in `autosave/` (`probe4.asl`, `probe5.aslx`; the `-intro` pair ask their questions from the start script) have one
command per piece of state a relaunch has to carry. The cases:

| | Quest 4 (`questionglk.cc`) | Quest 5 (`quest5/aslxglk.cc`) |
| --- | --- | --- |
| relaunch after every command | `q4-every-command` | `q5-every-command` |
| RNG position | `q4-rng` | `q5-rng` (the fallback stream and every per-expression stream, including those inside script bodies) |
| variables / attributes | `q4-variable` | `q5-attribute` |
| undo history | `q4-undo` | `q5-undo`, `q5-which-undo`, `q5-which-undo-error` (after a replayed disambiguation turn) |
| real-time timers | `q4-timer`, `q4-timer-midcycle` | `q5-timer`, `q5-timer-midcycle`, `q5-timeout` |
| prompts the game can be closed on | `q4-question`, `q4-menu`, `q4-enter` (autosaved; the relaunch replays the turn back to the prompt), `q4-intro-gender` (the same for a question of the startscript) | `q5-get-input`, `q5-show-menu`, `q5-ask`, `q5-intro-menu`, `q5-intro-ask` (same), `q5-wait` (no autosave; the relaunch resumes at the turn prompt) |
| hyperlinks | `q4-pane-links` (fold, Take, Drop through the pane) | `q5-inline-link`, `q5-link-menu` (an object link's verb menu stays autosaved and reopens on relaunch) |
| walkthroughs from `goldens/` | Bear Campsite, Mansion, Gathered in Darkness | Exit the Room, Bear's Epic Quest, ARC II |
| damaged record | `q4-corrupt` (discarded, fresh boot), `q4-old-format` (the pre-unification container, likewise), `q4-bad-undo` (engine state restored, undo history ignored with a log line) | `q5-corrupt`, `q5-old-format` |

Walkthrough cases are skipped when the game is absent from `games/`.
Restored transcripts are not re-sent as link runs, so a link clicked after a
relaunch is addressed by number (`link:PEER:N`) rather than by text; see the
`glkdrive.py` docstring for the script vocabulary (`tick:N`, `key:X`,
`click:TEXT`, `link:N`).

## Shared frontend helpers (`questglk_unit_tests.cc`)

Both Glk frontends (`questionglk.cc` and `quest5/aslxglk.cc`) draw on
`../questglk-common.cc`; this binary compiles that file in and links the
in-repo CheapGlk for the `glk_*` symbols its helpers reference. It belongs to
neither engine, so it stays here and is built at the root as
`./questglk_unit_tests`. Exit 0 is a pass.

## Where the code is

Two engines and two Glk frontends share `terps/question`:

- **Quest 4 engine.** `QuestionRunner.hh` is the interface a frontend sees,
  `question-impl.hh` the class behind it, `question-*.cc` the engine.
  `readfile.cc` reads and preprocesses the `.asl`/`.cas` file; `preprocess`
  is its passes (`collect_addtos`, `splice_addtos`, `convert_friendly_ifs`,
  `strip_comments`, `deinline_braces`, `trim_and_join_elses`). A script runs
  through a table of `exec_<statement>` handlers: `question-script.cc`
  tokenises, dispatches and evaluates conditions, the handlers are in
  `question-script-flow.cc`, `question-script-world.cc` and
  `question-script-output.cc`. A command goes down an ordered list of
  `try_<verb>` handlers in `question-verbs.cc`, in the order of Quest's
  `ExecCommand`. The libraries Quest bundles are compiled in verbatim as
  `standardlib_builtin.cc`, `stdverbs_builtin.cc`, `typelib_builtin.cc` and
  `q3ext_builtin.cc`, declared in `builtin_libs.hh`.
- **Quest 5 engine.** `quest5/aslx*.cc`; `quest5/README.md` here maps
  the files and records what its build-up settled.
- **Frontends.** `questionglk.cc` for Quest 4; `quest5/aslxglk*.cc` for
  Quest 5, all in `namespace aslxglk`, with `aslxglk-internal.hh` holding
  the types, the prototypes and every global as a C++17 `inline` variable.
  What both need is in `questglk-common.cc` (UTF-8 encoding of a Glk
  buffer, resource registration with the "does Glk really know it" probe,
  cancel and re-arm of the live line request, the autosave codec) and in
  `qstr.hh`, the header-only string helpers (`trim`, `ascii_lower`,
  `starts_with`, `ends_with`, `iequal`, `ifind`) both engines use too.
  readfile's own `trim (s, modes)`, `trim_braces`, `lcase`, `ci_equal` and
  the two line splitters (`split_lines`, and aslx's `split_into_lines`,
  which trims and drops empty lines) stay apart because they do something
  different.

A new source file is registered by hand:

- a Quest 4 engine `.cc`: `Spatterlight.xcodeproj`, the unity include list
  in `quest4/harness/headless_common.hh`, and
  `SpatterlightTests/QuestionRegressionTests.mm`;
- a Quest 5 `.cc`: `Spatterlight.xcodeproj` and the `Makefile` here
  (`ASLXGLKSRC` for a frontend file).

## Behaviour settled by the pass of October 2026

The tree was split into the layout above between 2026-10-11 and
2026-10-17, checked against Quest's own source (`V4Game.cs` and
`V4Game.Part2.cs` in the QuestViva checkout the Quest 5 oracle builds from)
where a question of behaviour came up. The gates for any change here are
`make check`, the Quest 4 sweep (`quest4/harness/run_walkthroughs.sh`,
150 games), the Quest 5 native replays (`quest5/harness/run_replays.sh`) and
`make autosave`. These are decisions; a change to one wants a fixture.

- **One autosave format for both engines.** `autosave.glksave` is a
  `questglk::AutosaveRecord` under the stamp `QUESTION-AUTOSAVE 1`: engine
  state, undo history and the replay record for either engine.
  `autosave.plist` carries one blob per frontend under the key
  `question_frontend_state`, and each frontend lays out its own fields
  (`QUESTIONGLK-AUTOSAVE 1`, `ASLXGLK-AUTOSAVE 8`). An autosave in any
  older layout (the `GEASAUTO1` container, the `geas_*` NSCoder keys,
  Quest 5's `ASLXGLK-BOOT` sentinel) is discarded and the game starts
  fresh: `q4-old-format`, `q5-old-format`. The record itself is unit-tested
  in `aslxglk_link_tests`.
- **`!addto` in a library takes three forms,** `game`, `synonyms` and
  `type <name>` for any type name, compared lowercased against the game's
  `define type <name>` line; a type the game lacks gets a block of its own.
  The lines go in just before the block's `end define`, because commands
  and verbs match in file order and lines at the front would beat the
  author's own. `addtotype` pins it; the engine used to refuse every type
  but `<default>` and `<defaultroom>`.
- **A parenthesised condition** after `if`, `until`, `while`, `not`, `and`
  or `or` is rewritten to `is <a;op;b>`; `not` and `while` were missing
  once, and a raw `<` left behind opens a parameter and loses the whole
  statement (Kingdom's village never rioted). The comparison found first in
  Quest's order (`!=`, `<>`, `<=`, `>=`, `<`, `>`, `=`) is the one the
  condition splits at. `compare` and `iscompare` pin the rewrite.
- **Font and pane statements in a script are ignored on purpose**
  (`exec_ignored` in `question-script-output.cc`); they are not a gap.
- **`make syntax` runs with `-Wshadow`** and the tree is clean under it.
  A struct split or a function split tends to leave a copied local
  shadowing a member, and the harnesses' unity build hides it.
- **Quest 4 and Quest 5 keep separate parsers** (`question-parse.cc`,
  `quest5/aslx-runtime-parse.cc`): different languages, sharing helpers
  and not structure.

## Declined, with the reason

- `game_function`/`text_function` as a table of one method per built-in
  name: most bodies are two or three lines, so the table would be mostly
  declarations in `question-impl.hh`.
- A shared metaverb dispatcher for the two frontends: each handler is
  about five lines, and what is left after the shared matcher and the
  shared print is the frontend-specific part (which window echoes, which
  pane refreshes). A callbacks struct would be longer than the code.
- Folding the `aslxglk` globals into structs: the names are grouped by
  prefix already; `reset_frontend_state` is selective on purpose, since
  some state outlives a session, so a struct would not become one
  assignment; the autosave codec names every field anyway; and it is a
  rename of about 1500 references with the shadow trap attached.
- A `questionglk-objwin.cc`: the side pane is about 280 lines sharing
  sixteen file-statics with the rest of `questionglk.cc`, so moving it
  means an internal header exposing most of the file's state to relocate
  15% of it. The functions were split instead (`draw_objwin_sections`,
  `echo_objlist_to_transcript`, `objwin_link_clicked`).
- Cutting the remaining 150-210 line functions: `eval_cond`,
  `eval_string_body`, `game_function`, `text_function`, `static_eval` and
  `try_builtin_verb` are flat keyword dispatchers whose length is mostly
  the comments citing Quest; `read_block_line`, `ensure_cached`,
  `print_formatted`, `object_verbs`, `get_places`, `try_take` and
  `try_use` each walk one piece of state start to finish, and a cut would
  only pass that state around.
- Decomposing `question_implementation` into sub-objects: large churn in a
  golden-tested engine for a readability win the per-verb and
  per-statement split already gave.
- Touching `quest5/aslx-core/**/*.aslx`: the vendored Quest 5 core and
  language files stay byte-for-byte with upstream.
