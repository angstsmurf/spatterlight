# Scarier tests

Two engines live in `terps/scarier`, and the test tree is split the same
way, with mirrored subfolders in each half (`terps/question/test` uses the
same layout):

| | |
| --- | --- |
| [`adrift4/`](adrift4/README.md) | ADRIFT 3.7 / 3.8 / 3.9 / 4.0 (`.taf`), the SCARE-derived engine in `core/`, `library/`, `runner/` |
| `adrift5/` | ADRIFT 5 (`.taf` / `.blorb`), the engine in `adrift5/` |

Each half holds `notes/` (one walkthrough per game, the deliverable),
`goldens/` (the command scripts and their committed transcripts),
`harness/` (the headless tooling and the unit harnesses), an untracked
`games/` for the third-party corpus, the `games.manifest.tsv` that pins it,
and an untracked `downloaded/` for the walkthrough documents the routes came
from.  `adrift4/` also keeps the Runner evidence the goldens were checked
against: [`runner_transcripts/`](adrift4/runner_transcripts/README.md), one
real-Runner transcript per walkthrough row, and
[`runner_probes/`](adrift4/runner_probes/README.md), the transcripts a
"measured" note in the source cites.  `adrift5/probes/` is the synthetic
conformance suite (`src/` builds the `.taf` files; `map_golden.sha256` is
their map manifest).

Only engine-agnostic things stay at this level: this README,
[`GAMES.md`](GAMES.md) and `fetch_games.sh` (the corpora, both engines),
`run_stages.sh` (runs the suite's stages side by side and prints their
logs in order), `run_map_corpus.sh` with `map_corpus_golden.txt` and
`map_ppm.h` (the whole-corpus map raster regression, both engines), and
`glkdrive.py`, a fake Spatterlight app that drives the built terp over the
glkimp protocol for the autosave suite.  `glkdrive.py` is terp-agnostic and
is the same file as `terps/question/test/glkdrive.py`, kept byte-identical
so each tree stays self-contained; `make check` in `terps/question/test`
fails if the two copies drift, so edit one and copy it over the other.

## Running

The suites are driven from `terps/scarier` by `Makefile.headless`, which
builds the plain stdout engine plus the `a5*` tools against `headless/`'s
os-layer stub and shares the engine objects between harnesses.  The root
`README` lists every target; the ones that matter day to day:

```sh
make -f Makefile.headless quick     # ~4 s: everything but the two walkthrough corpora
make -f Makefile.headless test      # ~45 s: the full suite, stages in parallel
make -f Makefile.headless sanitize  # ASan/UBSan build of the unit harnesses
make -f Makefile.headless test-fd   # opt-in: ADRIFT 5 goldens diffed against FrankenDrift (needs dotnet)
sh test/run_map_corpus.sh           # map rasters of every corpus game against their hashes
python3 test/adrift4/harness/run_autosave_tests.py --build   # Spatterlight autosave, both engines
```

`test` builds every binary first, then runs `unittest`, `vendorcheck`,
`capacitytest`, `wheretest`, `soundtest`, `gamescheck`, `a5runtest`,
`v4walkthroughs`, `a5maptest`, `scprojregress` and `a5walkthroughs-golden`
through `run_stages.sh`; a stage whose corpus is absent skips, and
`gamescheck` fails only on a file whose bytes differ from the manifest.
`quick` is the same list minus the two walkthrough stages and is not a
substitute for `test` before a commit: the corpora are what catch
Runner-parity regressions.  Replays run with determinism on
(`SCR_RNG=xoshiro`, `SCR_SEED`), which is what makes a golden repeatable
and what ties it to the Runner transcript it was checked against.

The autosave suite is the one thing outside `Makefile.headless`: the
autosave code is compiled only into the Spatterlight build of the terp and
talks to `libglkimp`, so the suite drives `build/Debug/scarier` through
`glkdrive.py`.  `adrift4/README.md` describes its cases, including the
`a5-*` ones.

## Where the code is

Dispatch happens at load time in `glk/os_glk.cpp`: the ADRIFT 5 loader is
tried first and returns cleanly on any other file, then the ADRIFT 4 path
takes over.  The root `README` has the tour; what a change here needs to
know:

- **ADRIFT 4 engine.** `core/` is the SCARE-derived engine, one prototype
  header per module (`core/scgamest.h`, `core/scparser.h`, ...) and
  `core/scprotos.h` a short umbrella over them; `core/scarier.h` is the
  public API.  `library/` is the command library (one `sclibrar_<verb>.cpp`
  per topic, `library/sclibrar.h` the umbrella), `runner/` the command
  runner.  A module split over several TUs has its own `*_internal.h`
  (`scparser_internal.h`, `scprintf_internal.h`, `sctasks_internal.h`,
  `sclibrar_disambig_internal.h`, `sclibrar_take_internal.h`,
  `sclibrar_put_internal.h`, `scrun_match_internal.h`), and each module's
  file-scope state sits in one struct (`uip`, `pf`, `expr`, `restr`, `ser`,
  `tasks`, `palette`, `disambig`).
- **ADRIFT 5 engine.** `adrift5/` with the turn loop in `adrift5/run/` and
  the text engine in `adrift5/text/`; `a5run.h`, `a5text.h` and `a5util.h`
  at `adrift5/` are the public faces of those folders, the `_internal.h`
  files live beside the implementations.
- **Hosts.** `glk/` is the Glk front end both engines share
  (`os_glk_platform.cpp` holds `glk_main`, `glkunix_startup_code` and the
  Windows Glk entry points, and the one open-by-path function);
  `platform/os_ansi.cpp` is the plain stdout front end the goldens are
  recorded through, `platform/scarier-autosave.mm` the Spatterlight
  autosave bridge; `headless/sxstubs.cpp` is the stub the harnesses link
  instead of `glk/`; `tools/` is the dump and trace instrumentation,
  compiled only with `-DSCARIER_DUMP_TOOLS` into the headless binaries.
  `mapdraw.cpp` and `sessrec.h` stay at the root because both engines use
  them.
- **Tracing.** `SCR_TRACE_ON (name)` / `SCR_TRACE_IF (name, format, ...)`
  in `core/scdump.h`, beside `SCR_DEVIATION`, read their environment
  variable once and compile to nothing without `SCARIER_DUMP_TOOLS`.
  `SCR_TRACE_BATTLE`, `SCR_TRACE_RAND`, `SCR_TRACE_SOUND` and
  `SCR_TRACE_FLAGS` are deliberately not under them: they work in the plain
  build (`SCR_TRACE_RAND` is also re-read at each seeding), and
  `SCR_TRACE_RCACHE` keeps its own `#ifdef` because it carries a task
  number.

Both makefiles find their sources by wildcard; only Xcode needs a hand
edit.  A new `.cpp` **or `.h`** goes into `Spatterlight.xcodeproj` by hand
(a header missing from the project breaks the app build, not the
makefiles); the file ids used here are the `5C1B1A0000000000000001xx`
series, next free `0118`.

## Behaviour settled by the passes of October 2026

Three refactoring surveys were worked through between 2026-10-10 and
2026-10-17 (the item lists, with their reasoning and commit hashes, are in
the last revision of `REFACTOR-TODO.md`: `git log --diff-filter=D --
terps/scarier/REFACTOR-TODO.md` names the commit that removed it).  Every
step was behaviour-neutral under the full suite and the Runner transcript
recompare.  What they fixed as decisions:

- **`core/scarier.h` is a supported embedding API.** The folder is meant
  to drop into other hosts (Gargoyle among them), so the entry points no
  front end in this tree calls (the filename and stream save/load forms,
  the compile-date, max-score, font, bold-room-name, score-notify, patch,
  capacity-assist and debugger queries, `scr_scarier_version` /
  `scr_scarier_emulation`) stay and are kept working.  The header says so
  above the prototypes.  Do not propose deleting them again.
- **Accessor guards.** Every accessor in `core/scgamest.cpp` opens with
  `GS_CHECK (gs)` or `GS_CHECK_<KIND> (gs, index)`.  The game half is an
  invariant and an `assert`; it tests the file-local inline `gs_game_ok()`
  rather than calling the exported `gs_is_game_valid()`, which was 5% of
  self time in the 2026-10-04 profile, and it compiles out under `NDEBUG`.
  The index half is not an invariant (published games carry out-of-range
  indexes), so it is an `scr_fatal()` that the `scinterf.cpp` entry points
  catch, not an `abort()` of the host.  `GS_FOR_EACH_*` are the entity
  loops.
- **Stage splits follow the Runner's order, not the version.** The
  ADRIFT versions interleave inside take, put and disambiguation, so
  `lib_take_t`, `lib_put_in_t` and `lib_disambig_t` were cut by stage in
  the original order and checked against the corpus diff: that is where
  Runner-parity ordering lives.  The same holds for `run_commands_t`,
  `run_command_match_t`, `task_scan_t`, `refine_t` and `override_frame_t`.
  A stage that settles a question returns TRUE with the answer in
  `result`.
- **No `goto`, no `#if 0`, no `sprintf` / `strcpy` / `strcat`** in the
  engine, no TODO / FIXME markers (the hits are pointers to
  `RUNNER_TESTS_TODO.md` / `WINE-TRANSCRIPTS-TODO.md`), `nullptr` in code
  with the word `NULL` kept in comments and strings, no `using namespace`
  in headers.  A pass that finds one of these has found a regression.
- **One RNG core.** `core/scutils.cpp`'s Runner-compatible generator and
  `adrift5/a5rand.cpp` both call `erkyrath_random()` in
  `common_utils/randomness.c`; the layers above stay apart on purpose
  (4.x reproduces VB's `Int(Rnd * n)`, ADRIFT 5 takes `w % span` as .NET's
  `Next` does, each with its own draw count and trace format).
  `scr_platform_rand` / `scr_congruential_rand` stay distinct functions
  for function-pointer identity.
- **`uip` options are held by guards, not passed by value.**
  `scr_containment_guard`, `scr_binary_input_guard` and the two older
  guards in `core/scparser.h` replace every hand-written set/restore pair
  around `uip_match()`; the runner's own copy of the lenient flag is gone
  for `uip_get_lenient_tasks()`.  The one pair left is
  `run_command_match_t::tree_match()` toggling strict binding around its
  own match.
- **Fatal messages reach the window.** `if_report_fatal()` keeps the
  message for `scr_take_fatal_message()`; the Glk front end's
  `gsc_report_fatal()` reports it through `win_showerror()` under
  `SPATTERLIGHT` and in the main window elsewhere.  ADRIFT 5 has no fatal
  path of its own: its load and out-of-memory stops print in the window.
- **`Makefile` tracks header dependencies** with `-MMD -MP` and the `.d`
  files; `make clean` is built from the object lists.
- **`-Wshadow` is part of the check** for any split: a copied
  `type name = ...;` can shadow a struct member, and `Makefile.headless`
  builds with `-w`, so the syntax check has to be run by hand (safety net,
  item 7).
- **`locale`:** `loc_table_lookup<T>` with shared low halves of the
  code-page tables; `NUL` / `WHITESPACE` are defined once in
  `core/sctypes.h`, and `WHITESPACE` differs from `scr_isspace()` on 0xa0
  on purpose (SCARE heritage, Runner-facing; the header records it).

## Declined, with the reason

Do not re-propose these without new evidence.

- **Verb quartets** (the all / except / multiple / common stacks per verb,
  `lib_cmd_wear_all` / `_remove_all` included) and the other thin verb
  pairs: measured Runner behaviour per verb, ordering accidents included,
  pinned by `adrift4/runner_transcripts/`.  A generic driver would need
  every per-verb difference as a parameter.  Revisit only with the corpus
  diff in hand.
- **Person-form triples** (`lib_select_response` You / I / %player%): the
  triples are what make the Runner text greppable against the transcripts.
- **Version-gate rename / `lib_dialect_t` predicates**: ~475 call sites,
  cosmetic, and the spellings are greppable.
- **A `lib_ctx_t` for the `bundle` / `filter` / `vars` prologue lines**: a
  signature redesign across 313 table-dispatched `lib_cmd_*` handlers in
  40 files, not a mechanical step.  The mechanical half, if ever wanted,
  is to include `scgamest.h` from `library/` and read `game->bundle`.
- **Parse-once dispatch input**: the 87 `run_get_dispatch_input ()` sites
  do not share a tokenisation.
- **An env-flag registry** (`scr_env`): cosmetic and wide (67 `getenv`
  lines in 17 files, each a debug flag), and the determinism harness
  depends on when flags are read.
- **`strcasecmp` wrappers in `adrift5/`**: the text there is UTF-8 and the
  `scr_` wrappers would fold bytes >= 0x80 through the 4.x code-page table.
- **`scr_fatal` `default:` arms as asserts**: a throw the host boundary
  catches is the gentler failure.
- **A `-Wconversion`-clean tree**: 803 warnings, 777 of them
  sign-conversion; fixing them means ~800 casts of the kind an earlier
  step removed.
- **Loop index renames** (`object` / `index_` / `obj` / `i`): renaming is
  where the risk is, and `GS_FOR_EACH_*` makes the drift harmless.
- **`for_each_walk_control`, the scexpr operator case lists, the per-version
  TAF record macros, the a5parse / a5state tails**: each shares a shape,
  not a body; the lists and the field order are the dispatch.
- **Near-duplicates that are not the same body**: `restr_match` vs
  `expr_parse_match` (two parsers, two error types); the code-page tables
  in `glk/os_glk_locale.cpp` (data in the same shape); `debug_print_room`
  / `_event` (debugger-only); the `a5_load_*` loaders' "for each child
  node" skeleton (~25 lines, no tests of its own beyond the corpora);
  `have_seen_character` / `_object` / `_location` (the location one
  answers 0 for a non-player observer on purpose); `inout_site` /
  `ud_site_primary` in `mapdraw.cpp` (the two switches read as the port
  layout they draw).
- **Making the remaining single-TU exports `static`**: `obj_seen_p`,
  `obj_visible`, `a5state_object_key_at_location` and `gsc_a5_media_fire`
  are each called above their definition, so `static` would only trade
  the header line for a forward declaration; the `a5run_undo_*` /
  `a5run_pending_*` group exists for `glk/os_glk_autosave.cpp`, and
  `a5run.h` says so.
- **Cutting `render_link`** (`mapdraw.cpp`, half comments, already
  extracted from `map_render`), **`pass_object`** and
  **`restr_pass_task_char`** (flat operator dispatches in the Runner's case
  order) and **`lib_stand_sit_lie`** (per-movement message switches):
  split one only when a change touches it.  29 functions are over 150
  lines; counted as code lines the largest is 128, and most of the length
  is Runner-citation comments.
- **Moving `adrift5/a5run.h` and friends into `run/` / `text/`, a `map/`
  folder for `mapdraw`, a `<cstdio>` / `<cstring>` sweep**: cosmetic, and
  the header moves cost Xcode search-path churn.
- **Sharing map, save, text or input code between the engines**: surveyed
  once per layer; only the RNG core was the same thing twice.  The two map
  modules meet at `map_t` and share `mapdraw.cpp` already; the save
  formats are fixed by the files; the text and input layers port different
  Runners.
- **`InitGlk` extern in `glk/os_glk.cpp`** and `os_ansi.cpp`'s hand-written
  `extern run_probe_strict`: the first is Windows Glk's (no `WinGlk.h`
  here), the second keeps `scprotos.h`'s TRUE / FALSE macros off that
  file's enum.
- **`scarier.hdr`**: maintained by hand, not a build input.

## Left on purpose

Candidates, not tasks; each wants its own decision.

- **ADRIFT 5 `char *` returns.** ~50 `malloc` / `calloc` and ~360 `free`
  in `adrift5/`: text functions return a `malloc`ed string the caller
  frees.  Wrapping that is an API change across a5text / a5run / the Glk
  front end / every harness, and wants a leak checker that runs here
  (LeakSanitizer does not on macOS/arm64; `leaks --atExit` might).
- **The ~45 raw `scr_malloc` sites and the raw `new` / `delete` in the 4.x
  engine** are struct owners with a destroy function, strings handed to
  `prop_adopt()`, pool entries, or stack-buffer-with-heap-fallback.
- **The rest of the file-scope statics.** The state-struct pass covered
  the modules its survey named; the other library files have their own
  reset hooks (`lib_battle_who_reset()`, `lib_with_prefix_390_reset()`,
  `lib_put_reset()`), `runner/` has its line buffers, and
  `core/sctafpar.cpp`, `glk/os_glk_map.cpp` and `glk/os_glk_autosave.cpp`
  have the most.  A census of those is the first step if two games in one
  process is ever a goal.
- **File sizes.** The largest are `library/sclibrar_drop.cpp` and
  `mapdraw.cpp` at about 2700 lines, with a dozen more between 2000 and
  2550; none stands out.
- **Lines over 80 columns in `adrift5/`** (about 1460): its compact
  one-line bodies are that code's own style.
- **Five `(void) x` casts under `#if`** in `glk/`: `[[maybe_unused]]` is
  C++17 and `Makefile.headless` builds `-std=c++11`.
- **Macros that stay**: `scr_setjmp` / `scr_longjmp`, `GS_CHECK*`,
  `GS_FOR_EACH_*`, `PATCH_*`, `SCR_DEVIATION`, `RUN_TOKEN_IS`; each needs
  the caller's line, a loop header, an aggregate initialiser or `sizeof`
  of a literal.

## Measuring a pass

The three surveys were measured the same way, so a later one can repeat it
and compare:

- **Dead code**: `nm -P` over the `plain/` and `dump/` objects of
  `Makefile.headless` plus `glk/*.cpp` compiled by hand against
  `../cheapglk/glk.h` (defined global, never undefined anywhere), then
  `grep -w` over the whole Spatterlight tree to separate "no callers" from
  "called only inside its own TU".  Expect the `scarier.h` API above,
  `glkunix_arguments` / `glkunix_startup_code`, and the `lib_*_t` /
  `run_*_t` stage members (called from their own file by construction).
- **Duplicates**: every column-0 `{ ... }` body of 10+ lines with
  identifiers, strings and numbers normalised, MD5-bucketed.  The last
  count was 26 buckets, all in the declined list above; no two bodies are
  byte-identical after whitespace normalisation.
- **Size**: every function from a column-0 `{` to the next column-0 `}`,
  and the same counted as code lines (not blank, not comment-only), since
  Runner citations are most of the length.
- **Sanity**: the greps under "No `goto`" above, plus `getenv` lines and
  file-scope statics per file.

## Safety net

1. `make -f Makefile.headless quick` after each small step.
2. Full `make -f Makefile.headless test` before a commit; diff the whole
   output against a pre-change run with the compile lines stripped and the
   rest sorted, not just pass/fail.
3. `make -j8 scarier glkscarier` and `xcodebuild -target scarier`; a new
   or renamed file must be added to the Xcode project.
4. `make -f Makefile.headless sanitize` after any memory-management change.
   It runs with `detect_leaks=0`, so it checks overruns and use-after-free,
   not leaks.
5. For text or ordering changes in `library/`: diff the
   `adrift4/runner_transcripts/` corpus with `runner_transcripts.py
   recompare`; "Runner-identical" must stay identical.  Recompare against a
   binary built from the pre-change tree rather than trusting the committed
   `manifest.tsv`, which has been stale before.
6. `adrift4/harness/run_autosave_tests.py --build` after touching
   serialisation or the Glk main loops.
7. For a file split: the sorted line multiset of the new files against the
   old one, and a `-fsyntax-only -Wall -Wextra -Wshadow` check of each new
   TU.
8. `tools/scdump.cpp` changes: diff `SCR_DUMP_*` output over the corpus.
9. One commit per mechanical step, behaviour-neutral, so a bisect lands on
   a single rename or extraction.
