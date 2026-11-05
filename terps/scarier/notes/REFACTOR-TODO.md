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
| `lib_two_verb_line_pre400` | `library/sclibrar_two_verb_pre400.cpp:340` | 173 |
| `lib_put_named_pre400` | `library/sclibrar_put_pre400.cpp:467` | 171 |
| `run_does_command_match` | `runner/scrun_match_run.cpp:918` | 169 |
| `lib_cmd_examine_object` | `library/sclibrar_examine.cpp:1651` | 168 |
| `a5run_resolve_plural_objects` (name approx) | `adrift5/run/a5run_ref.cpp:854` | 204 |
| `a5run_intro` | `adrift5/run/a5run.cpp:1416` | 157 |
| `pass_object` | `adrift5/a5restr.cpp:342` | 160 |
| `mapdraw` pass 1 per-link renderer (`render_link`) | `map/map_links.cpp:768` | 209 |

**Done 2026-11-05** (full `test` identical, `make`, `xcodebuild` clean):

- `gsc_read_command` is now `gsc_prompt_prelude`, `gsc_take_supplied_line`,
  the Glk read, `gsc_take_clicked_walk_step` and `gsc_filter_typed_line`,
  with statement order unchanged. The headless suite does not reach this
  file, so it was checked with an A/B run of cheapglk `glkscarier` (HEAD
  vs new, mortality.taf, a script of abbreviations, quoted input, help,
  `glk` commands and restart): output identical. The Spatterlight autosave
  suite (`test/adrift4/harness/run_autosave_tests.py`) passes. Its one
  failure at the time, `wesghn-4.00-sound`, predated this change and was
  fixed separately: `has_notified` now travels in the session state.
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

  **Done 2026-11-05.** `mapdraw.cpp` / `mapdraw.h` moved to a new `map/`
  folder (not `core/`, which is the ADRIFT 4 engine; `scmap` / `a5map` /
  `os_glk_map` stay with the engines that feed them). The file is split by
  its existing section banners:

  | TU | Holds | Lines |
  | --- | --- | --- |
  | `map/mapdraw.cpp` | model, pages and views, `render_node`, `map_render`, hit testing and walking | ~540 |
  | `map/map_raster.cpp` | palette, drawing primitives, bitmap text | ~670 |
  | `map/map_layout.cpp` | projection, framing, pan/zoom chrome | ~490 |
  | `map/map_links.cpp` | link geometry, badge sites and layout, links and exit stubs | ~1030 |

  What crosses files is declared in `map/map_internal.h`, inside
  `namespace map_internal`, so names like `palette` and `fill_rect` stay
  off the global namespace; definitions are `map_internal::name`. Bodies
  are unchanged. The Makefiles glob `map/*.cpp`. Gates: the
  `run_map_corpus.sh` manifest is byte-identical to HEAD's, the full
  headless `test` is identical, `make`, `build.sh` and the xcodebuild
  scarier target are clean, and every TU is clean under `-Wshadow`. Note
  that `run_map_corpus.sh` already reported FAIL at HEAD. The cause is not
  a changed hash: four entries are missing from the committed golden,
  the `start` and `all` views of `AFP2.taf` and of
  `EscapeFromTheCastleOfCarnalIntent.blorb`, both in the ADRIFT 5 corpus.
  They are now blessed (no other row changed), and the script exports
  `LC_ALL=C`: rows follow glob order, which follows the collation locale,
  so a bless under en_NZ.UTF-8 reordered the whole file. The GARGLK build (`-std=c++17
  -DGARGLK`, linked against `libgarglk`) compiles and links. At the next
  fork sync, the hand-kept `terp(scarier SRCS …)` list in
  `garglk/terps/CMakeLists.txt` must replace `mapdraw.cpp` with the four
  `map/*.cpp` files.
- **`library/sclibrar_drop.cpp` (2665), `sclibrar_battle.cpp` (2567),
  `sclibrar_take_multiple.cpp` (2348).** Over 2k lines each. `drop` holds
  `lib_two_verb_line_pre400`, which is not a drop concern; it belongs in
  the `sclibrar_dispatch`/`respell` neighbourhood. The pre-4.0 twins already
  have a naming convention (`*_pre400`, `*_400`). Count: 49 `pre400`
  mentions in `drop`, 42 in `take_multiple`, 28 in `put_multiple`. Consider
  `sclibrar_*_pre400.cpp` TUs so a version's behaviour can be read as one
  file, mirroring what was done for `disambig_400`.

  **Done 2026-11-05** for `drop`, `take_multiple` and `put_multiple`
  (`battle` is not version-split and stays). Blocks moved whole, bodies
  unchanged:

  | New TU | Holds | Lines |
  | --- | --- | --- |
  | `library/sclibrar_two_verb_pre400.cpp` | `lib_two_verb_line_pre400`, `lib_two_verb_rewrite_pre400`, `lib_earlier_handler_claims_pre400` and their word tables, out of `drop` | ~515 |
  | `library/sclibrar_take_pre400.cpp` | `lib_co_pre400`, the 3.7-3.9 helpers (`isheld`, `obhere`, `present`, the InStr ranges), the takes() crowd and held-namesake answers, `lib_take_and_pre400`, `lib_move_named_whole_line_pre400` | ~1070 |
  | `library/sclibrar_put_pre400.cpp` | the pre-4.0 "put X in Y" answers, `lib_put_named_pre400`, the 3.9 refusal-first sweep and co helpers, `lib_cmd_put_in/on_that_390` | ~1030 |

  `drop` is now ~2180 lines, `take_multiple` ~1310, `put_multiple` ~905.
  Statics that now cross a file are declared in the private headers:
  `lib_drop/wear/remove_filter` in `sclibrar_internal.h`; the take ones in
  `sclibrar_internal.h` and `sclibrar_take_internal.h`; the put ones in
  `sclibrar_put_internal.h`. `drop`'s small `lib_drop_and_arm_pre400` pair
  stays, since only `lib_drop_multiple_common` uses it. Gates: `quick`, the
  full headless `test` with an identical signature, `make`, the xcodebuild
  scarier target, `-Wshadow` and the GARGLK `-std=c++17` syntax check. At
  the next fork sync the three files join the `terp(scarier SRCS …)` list
  in `garglk/terps/CMakeLists.txt`.
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
  *Done 2026-11-05:* eight structs in `os_glk_internal.h`, one commit each:
  `gsc_map_pane`, `gsc_io`, `gsc_replay` (waitkey/silent/A5 popup replay),
  `gsc_colour`, `gsc_walk`, `gsc_options`, `gsc_title`, `gsc_a5_media`.
  Left alone on purpose: the running-game handles (`gsc_game`, `gsc_a5_adv`,
  `gsc_is_a5`, `gsc_game_path`...; `gsc_game` alone has 84 uses), the
  autosave flags and file-statics (already private beside their functions),
  and a few unrelated singletons (`gsc_patches_enabled`, `gsc_a5_real_time`,
  `gsc_line_echo_manual`, `gsc_main_at_line_start`...). GARGLK `-Wshadow`
  syntax check, `quick` and the autosave suite passed after every commit.
- **`scr_bool` vs `bool`.** About 3,160 `scr_bool` against 129 plain `bool`.
  The new ADRIFT 5 code and recent v4 additions mix both. Pick one rule
  (v4 and library keep `scr_bool`, `adrift5/` uses `bool`) and write it in
  `GLOSSARY.md`; do not mass-convert.
  *Done 2026-11-05:* the split already held (`core/` 1 `bool`, a functor;
  `library/` 0; `adrift5/` 0 `scr_bool`; `glk/`'s 20 `bool`s are the
  autosave container and one sort comparator). The rule is in
  `GLOSSARY.md` under "Comment style"; nothing was converted.
- **C-style casts.** About 85 `(char *)`/`(scr_char *)` casts and 3
  `const_cast`s. Replace with `reinterpret_cast`/`static_cast`, or fix the
  signature, where the cast papers over `const`. Check `core/scutils.cpp` first.
  *Done 2026-11-05* for the casts that strip `const` inside the engine.
  Redundant ones were dropped (C++ `strchr` on a `char *`, the
  `scr_vartype_t.string` store in `scdump`). Read-only pointers became
  `const char *` (`op` in `a5restr`, `tok_end` in `a5text_func`, the
  `scprintf_output` cursor). `a5_state_t::ov_index`, a lazy cache, is now
  `mutable` in place of the `const_cast`. The real puns use named casts
  (`sctaffil` slab bytes, the inflated XML and in-situ step text in
  `a5model`). Left alone: the roughly 55 casts on `malloc`/`realloc`
  results, which go with the raw-allocation item below, and the Glk-boundary
  casts (`glk_put_string`, `glk_put_buffer_stream`, the `glkunix_arguments`
  table). Glk's API takes `char *`, as `gsc_put_literal` documents. The
  full headless `test` passed.
- **Raw allocation.** About 14 `malloc`-family calls, mostly `core/scutils.cpp`
  (5) plus `glk/os_glk*.cpp`. Candidates for `std::vector`/`std::unique_ptr`;
  109 files already use `std::` containers. Leave anything that crosses
  `setjmp` frames alone (see next item).
  *Done 2026-11-05 for `glk/`:* the scratch buffers that were malloc'd and
  freed in one function are now `std::string`/`std::vector`. That covers the
  help span and the `glk` command copy, the colour/font tag copies in
  `os_glk_output` (which also cleared that file's `-Wshadow` warning), both
  unicode line buffers, the ADRIFT 5 restore read buffer and the A5 display
  plain run. The `scr_malloc`/`gsc_malloc` wrappers and `scutils` are the
  allocators themselves and stay. Buffers whose ownership crosses into the
  engine (`a5model_load_buffer`, `a5_inflate`'s result, `a5run_*` blobs)
  stay too, as does the C-style `adrift5/` code. Gates: the Glk gates, plus
  cheapglk `glkscarier` at HEAD and after on every 9th corpus game with
  `glk help`, save and restore under `SCR_STABLE_RANDOM_ENABLED`: 98
  transcripts and 89 save files were byte-identical.
- **`setjmp`/`longjmp` (37 mentions in 5 files: `scexpr`, `scparser_pattern`,
  `screstrs`, `sctypes.h`, `scparser_internal.h`).** The top-level review
  already fixed the worst longjmp-over-C++-object bugs. A remaining sweep is
  to confirm no non-trivial destructor lives in any frame between a
  `setjmp` and its `longjmp`, especially anything using `std::string` or
  `std::vector` in `scexpr`'s tokenizer (it has an RAII guard,
  `expr_tokenize_guard`, so check that its destructor path is covered).
  Longer term, replace with C++ exceptions inside the engine and keep the C
  boundary only in `scarier.h`. This is a big change, not a cleanup.
  *Audited 2026-11-05: clean.* Each parser has one `setjmp`. Every frame a
  `longjmp` can unwind belongs to a `static` function in the same file:
  20 in `scexpr`, 3 in `screstrs`, 7 in `scparser_pattern`. None is passed
  as a function pointer, and all hold only plain-data locals.
  `expr_tokenize_guard` is built before the `setjmp`, in the frame the jump
  returns to, so its destructor still runs. The restriction evaluators call
  out only to getters (`gs_`, `obj_`, `prop_`, `var_`), which neither jump
  nor re-enter `restr_eval_task_restrictions` (whose global `jmp_buf` a
  nested call would overwrite). A comment at each `setjmp` now states the
  rule. Moving to exceptions stays open.

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
