# Scarier refactoring TODO

Survey of `terps/scarier/` on 2026-11-05 (181 tracked `.cpp`/`.h`/`.mm` files
outside `test/`, about 160k lines). Static scan only: line counts, a
token-window duplicate finder, and greps. **Nothing here has been built or
run**, and every item needs reading before it is acted on. Earlier passes
(TU splits, module state structs, accessor prologue, tree layout; see the
memory index `scarier-refactor-index`) already took the cheap wins, so this
list is deliberately short and mostly small.

## Ground rules for any item below

- Behaviour-preserving only. Gate every change on the headless suite
  (`make -f Makefile.headless quick`, then the full `test`) with the suite
  signature unchanged, plus `make -j8` and the `xcodebuild` scarier target.
- Keep the public `scarier.h` API.
- Several "odd looking" things are **deliberate runner-parity deviations or
  documented Runner measurements** (the long probe comments in
  `sclibrar_dispatch.cpp`, `scrun_respell.cpp`, `scrun_tables.cpp`). Do not
  "simplify" the logic, only its shape.
- Splitting a struct can trip `-Wshadow` (`scarier-struct-split-shadow-trap`).
- New headers need `project.pbxproj` entries; new `.cpp` files need entries in
  both Makefiles.

## What the scan found clean

- No `TODO`/`FIXME`/`XXX`, no `goto` statements, almost no `sprintf`/`strcpy`
  (2 hits), and only about 14 raw `malloc`-family calls.
- No verbatim cross-file duplication. The only repeated 10-line windows are
  licence headers.
- Build products (`*.o`, `*.d`) are git-ignored, not tracked.

## P1: worth doing

### 1. Duplicated blocks inside a file: DONE 2026-11-05

Full `test` output identical to the pre-change run; `make`, `xcodebuild
-target scarier` clean.

- `tools/scdump.cpp`: `scdump_task_t::typed_vars()` reads Type/Var1..3 for
  both `restriction()` and `action()`.
- `core/scexpr.cpp`: the binary operators moved to
  `expr_eval_binary_operator()`, so `expr_eval_numeric_operator()` no longer
  lists them twice. The case list in `expr_eval_action()` is ordinary
  routing and stays.
- `runner/scrun_tables.cpp`: the rows are **not** dead. The priority pass
  defers refusals and the STANDARD twins print them, and the two tables
  list the `put`/`[drop/put down]` spellings in opposite order, which
  matters because `put %text% in %object%` also matches "put down X in Y".
  All four groups now come from one `PUT_IN_ON_ROWS(verb)` macro with each
  table's order kept. The object file's data is byte-identical; only
  `assert` line immediates moved.
- `glk/os_glk_symbols.cpp`: the four tables are sized `[256]`, and
  Wingdings 3's trailing zero rows are left to zero-fill. Data is
  byte-identical.
- `core/sctafpar.cpp` V380/V370 TASK strings: left alone. They differ in
  exactly the tokens the 3.7 comment describes, and reading them in full
  side by side is the point.

### 2. Over-long functions (split by phase, not by logic)

Lengths are rough (to the first column-0 `}`).

| Function | File | ~Lines |
|---|---|---|
| `lib_try_game_command_common` | `library/sclibrar_dispatch.cpp:414` | 195 |
| `gsc_read_command` | `glk/os_glk_input.cpp:172` | 190 |
| `run_line_t::get_outer_400` | `runner/scrun_dispatch.cpp:824` | 188 |
| `uip_match_entity` and friends | `core/scparser_entity.cpp:1062` | 188 |
| `lib_go` | `library/sclibrar_go.cpp:64` | 182 |
| `run_line_t::settle` / `::library` | `runner/scrun_dispatch.cpp:1494`, `:1307` | 182, 176 |
| `restr_object_in_place` | `core/screstrs.cpp:212` | 181 |
| `restr_pass_task_object_location` | `core/screstrs.cpp:404` | 159 |
| `restr_pass_task_char` | `core/screstrs.cpp:705` | 164 |
| `task_print_end_game_message` | `core/sctasks_endgame.cpp:481` | 174 |
| `gs_populate_dynamic_object`, `gs_populate`, `gs_populate_objects` | `core/scgamest.cpp` | 174, 154, 151 |
| `lib_two_verb_line_pre400` | `library/sclibrar_drop.cpp:2182` | 173 |
| `lib_put_named_pre400` | `library/sclibrar_put_multiple.cpp:750` | 171 |
| `run_does_command_match` | `runner/scrun_match_run.cpp:918` | 169 |
| `lib_cmd_examine_object` | `library/sclibrar_examine.cpp:1651` | 168 |
| `a5run_resolve_plural_objects` (name approx) | `adrift5/run/a5run_ref.cpp:854` | 204 |
| `a5run_intro` | `adrift5/run/a5run.cpp:1416` | 157 |
| `pass_object` | `adrift5/a5restr.cpp:342` | 160 |
| `mapdraw` pass 1 per-link renderer | `mapdraw.cpp:2134` | 209 |

**Done 2026-11-05** (full `test` identical, `make`, `xcodebuild` clean):

- `gsc_read_command` is now `gsc_prompt_prelude`, `gsc_take_supplied_line`,
  the Glk read, `gsc_take_clicked_walk_step` and `gsc_filter_typed_line`,
  with statement order unchanged. The headless suite does not reach this
  file, so it was checked with an A/B run of cheapglk `glkscarier` (HEAD
  vs new, mortality.taf, a script of abbreviations, quoted input, help,
  `glk` commands and restart): output identical. The Spatterlight autosave
  suite (`test/adrift4/harness/run_autosave_tests.py`) passes except
  `wesghn-4.00-sound`, which fails identically with HEAD's
  `os_glk_input.cpp`: a "(Your score has increased ...)" line is printed
  before input on relaunch. Pre-existing; not chased.
- `screstrs.cpp`: `restr_pass_task_char` delegates to `restr_char_player` /
  `restr_char_npc`, and the held-by-player arm of `restr_object_in_place`
  is `restr_object_held_by_player` (its long Runner comment moved with it).
  The rest of the `restr_*` length is documentation. Without comments the
  code is 50-100 readable lines, so it was left as is.

Suggested approach: `gsc_read_command` already reads as a list of prompt
preludes (help hint, style reset, status, meta report, map redraw, autorestore
skip). Pull the prelude into `gsc_prompt_prelude()` and leave the input loop.
The `restr_*` functions are big switches over restriction types, so a
per-type static helper keeps each `case` under about 30 lines. The runner
functions already document each phase in comments, which makes natural seams.
Do **not** change statement order inside the dispatch cascade.

### 3. Big top-level files that still bundle unrelated jobs

- **`mapdraw.cpp` (2722 lines).** The largest file, living at the tree top
  rather than in a module directory. It is used by `adrift5/a5map.cpp`,
  `core/scmap.h`, `tools/scdump.cpp` and both Makefiles, so it is shared
  by v4 and v5 but sits outside `core/`. Move it (and `mapdraw.h`) to
  `core/` or a new `map/` directory alongside `scmap`/`a5map`/`os_glk_map`,
  then split into layout, link routing, and rasterising TUs. The map-corpus
  golden (`test/map_corpus_golden.txt`, `run_map_corpus.sh`) is the oracle.
  Needs pbxproj and Makefile path edits.
- **`library/sclibrar_drop.cpp` (2665), `sclibrar_battle.cpp` (2567),
  `sclibrar_take_multiple.cpp` (2348).** Over 2k lines each. `drop` holds
  `lib_two_verb_line_pre400`, which is not a drop concern; it belongs in
  the `sclibrar_dispatch`/`respell` neighbourhood. The pre-4.0 twins already
  have a naming convention (`*_pre400`, `*_400`). Count: 49 `pre400`
  mentions in `drop`, 42 in `take_multiple`, 28 in `put_multiple`. Consider
  `sclibrar_*_pre400.cpp` TUs so a version's behaviour can be read as one
  file, mirroring what was done for `disambig_400`.
- **`core/sctafpar_patches.cpp` (1743).** About 1,600 lines are one data table
  of per-game TAF edits with a macro DSL (`PATCH_SET`, `PATCH_ADD`, ...).
  This is the right shape, but each game is a prose comment plus a few rows.
  Splitting into one `.inc` per game, or a sorted-by-game array of structs
  in a data-only TU, would make blame and review easier. Low value, low risk.

## P2: consistency and type hygiene

- **Global `gsc_*` state in the Glk layer.** `glk/os_glk_internal.h` declares
  about 181 `extern` entries, with 15 more non-const file-statics in the
  `.cpp`s. The v4 modules already got state structs (`scarier-module-state-structs`);
  the Glk front end did not. Group by feature (`gsc_main_window`,
  `gsc_status_window`, `gsc_*_stream` into an `io` struct; waitkey and silent
  flags into `input`; autosave and map into their own) and pass or alias via
  one `static` instance per TU. Mechanical, but it touches every `glk/` file,
  so do it one feature at a time.
- **`scr_bool` vs `bool`.** About 3,160 `scr_bool` against 129 plain `bool`.
  The new ADRIFT 5 code and recent v4 additions mix both. Pick one rule
  (v4 and library keep `scr_bool`, `adrift5/` uses `bool`) and write it in
  `GLOSSARY.md`; do not mass-convert.
- **C-style casts.** About 85 `(char *)`/`(scr_char *)` casts and 3
  `const_cast`s. Replace with `reinterpret_cast`/`static_cast`, or fix the
  signature, where the cast papers over `const`. Check `core/scutils.cpp` first.
- **Raw allocation.** About 14 `malloc`-family calls, mostly `core/scutils.cpp`
  (5) plus `glk/os_glk*.cpp`. Candidates for `std::vector`/`std::unique_ptr`;
  109 files already use `std::` containers. Leave anything that crosses
  `setjmp` frames alone (see next item).
- **`setjmp`/`longjmp` (37 mentions in 5 files: `scexpr`, `scparser_pattern`,
  `screstrs`, `sctypes.h`, `scparser_internal.h`).** The top-level review
  already fixed the worst longjmp-over-C++-object bugs. A remaining sweep is
  to confirm no non-trivial destructor lives in any frame between a
  `setjmp` and its `longjmp`, especially anything using `std::string` or
  `std::vector` in `scexpr`'s tokenizer (it has an RAII guard,
  `expr_tokenize_guard`, so check that its destructor path is covered).
  Longer term, replace with C++ exceptions inside the engine and keep the C
  boundary only in `scarier.h`. This is a big change, not a cleanup.

## P3: build and test plumbing

- **`Makefile.headless` (745 lines) vs `Makefile` (193).** Check whether the
  source lists are maintained twice. If they are, move the shared `SRCS` lists to
  one included `sources.mk`, so adding a TU is one edit instead of three
  (Makefile, headless, pbxproj).
- **`tools/scrun_census.cpp` and `tools/scdump.cpp`** compile the whole
  engine. `scdump.cpp` (2192 lines) is a debugging tool with its own
  restriction and action decoders. Check how much of the decoding duplicates
  `core/scdebug.cpp` (2527 lines) and `core/scprops.cpp`, and share one
  decoder if the two have drifted.
- **Ignored artefacts in the source tree** (`*.o`, `*.d` next to sources).
  They are ignored, so harmless, but an out-of-tree `BUILD` dir would stop them
  showing up in editor file lists and in `git status --ignored`.

## Not recommended (looked at, leaving alone)

- Merging `core/screstrs.cpp` with `adrift5/a5restr.cpp`, or `scexpr` with
  `a5expr`. They implement different engines with different semantics
  (`adrift4-vs-5-restriction-eval`, `adrift4-vs-5-division-rounding`).
  Sharing code here would risk the byte-identical goldens.
- Removing the `pre400` or lenient-matching branches. They are the documented
  runner behaviour.
- Collapsing the `sclibrar_*` TUs back together. The split was a deliberate
  earlier pass.

## Suggested order

1. The three small in-file duplicates (scdump prologue, scexpr case list, then
   the `scrun_tables` rows once checked with the census). About 1 hour.
2. `gsc_read_command` prelude, then the `restr_*` per-type helpers.
3. Move `mapdraw.*` out of the tree root.
4. The `pre400` TU split for `drop`/`take_multiple`/`put_multiple`.
5. Glk `gsc_*` state structs, one feature per commit.
