# Scarier: refactoring / clean-up / de-duplication TODO

Two surveys of `terps/scarier` have been taken and worked through:

- **First pass**: surveyed 2026-10-10 (master @ 1b41f85be), worked through
  by 2026-10-11 (master 8d4895136).  Sections numbered 1–6a.
- **Second pass**: surveyed 2026-10-11 (master @ 89c8e788b) after the
  first was closed, worked through the same day (7d2f82a33).  Sections
  lettered A–F.

- **Third pass**: surveyed 2026-10-15 (wip @ b9d1635), read-only; its
  findings are under "Open" and "Third pass" below.

Every item of the first two passes was done or declined except the one
under "Open" below.  This
file keeps what a later pass needs: where things landed, what was
declined and why (so it is not re-proposed), what was left on purpose,
how the surveys were measured, and the safety net.

The full logs, with the census, the per-step reasoning, the original
suggestions and the surveyed line numbers, are in git:

    git show 8d4895136:terps/scarier/REFACTOR-TODO.md
    git show 7d2f82a33:terps/scarier/CLEANUP-SURVEY-2026-10-11.md

Section numbers and letters below are the surveys', kept so that commit
messages and the old logs still resolve.

## Open

- **A. Public-API functions in `core/scarier.h` / `scinterf.cpp` with no
  caller in this tree** (the Glk front end never uses them).  Whether
  `scarier.h` is a supported library surface is the maintainer's call
  (Gargoyle takes the folder as a drop-in, per `common_utils/README`):
  if it is, leave them and say so in the header; if not, delete.

  `scr_does_game_use_graphics`, `scr_get_game_compile_date`,
  `scr_get_game_max_score`, `scr_get_game_preferred_font`,
  `scr_get_game_bold_room_names` / `scr_set_game_bold_room_names`,
  `scr_get_game_notify_score_change` / `scr_set_game_notify_score_change`,
  `scr_get_game_patches`, `scr_get_capacity_assist`, `scr_get_end_keyprompt`,
  `scr_get_game_debugger_enabled`, `scr_run_game_debugger_command`,
  `scr_scarier_emulation`, `scr_scarier_version`,
  `scr_load_game_from_filename` / `_stream`, `scr_save_game` /
  `scr_save_game_to_filename` / `_stream`.

  The `scr_save/load_*_stream/filename` family is the clearest case:
  nothing outside `scinterf.cpp` calls them, and the Glk front end goes
  through the `*_callback` forms.  Deleting them would also make
  `if_file_read_callback` / `if_file_write_callback` unreachable.
  (`scr_game_from_stream` is *not* in the family: `platform/os_ansi.cpp`
  loads through it.)

- **N1. Third-pass candidates** (new; see "Third pass" below for the
  measurements).  None changes behaviour; all are small:
  1. `lib_carried_size` / `lib_carried_weight`
     (`library/sclibrar_take.cpp:89`, `:108`): identical loops, differing
     in the running-total test and the `obj_get_size` / `obj_get_weight`
     call.  A single static helper taking the two as parameters (or a
     small `bool weight` switch) folds ~20 lines.  Low risk; called from
     the capacity tests the take / put corpus pins.
  2. `ser_get_int` / `ser_get_uint` (`core/scserial.cpp:1333`): the same
     `sscanf` + error + `throw` body with `%ld` / `%lu`.  Fold the shared
     "read a line, scan one number, throw `ser_tas_error_t`" part (the
     boolean reader a few lines down has the same skeleton).
  3. `parse_get_keyed_integer` / `_boolean` / `_child_count`
     (`core/sctafpar.cpp:765`–`807`): three byte-identical-after-
     normalisation bodies (find keyed node, check type, return a field).
  4. `char_restr_t::have_seen_character` / `_object` / `_location`
     (`adrift5/a5restr.cpp:1269`–`1302`): three members with identical
     structure and one different state accessor.
  5. `lib_cmd_put_all_in` / `lib_cmd_put_all_on`
     (`library/sclibrar_put_multiple.cpp:217`, `:231`), `lib_cmd_lock_other`
     / `lib_cmd_unlock_other` (`sclibrar_refuse.cpp:811`),
     `lib_cmd_verbose` / `lib_cmd_brief` (`sclibrar_meta.cpp:1123`),
     `lib_cmd_kiss_object` / `lib_cmd_sell_object` (`sclibrar_verbs.cpp`):
     thin verb pairs.  Same family as the declined 1.1 / C4: **leave**
     unless the corpus diff is in hand.
  6. `inout_site` / `ud_site_primary` (`mapdraw.cpp:994`, `:1012`): same
     `switch (edge)` with a different table; could be one 4x2 table
     lookup.  Cosmetic.
  7. The `core/scvars.cpp` getter twins (`:1091`/`:1633`, `:1421`/`:1648`,
     `var_get_integer` / `var_get_string` at `:2076` / `:2110`): same
     skeleton, different table.  Check the tracing before folding.


- **N2. Mid-size functions** (150–200 lines).  Unchanged class from the
  second pass but recounted: 43 over 150, **1** over 200 (`mapdraw.cpp`
  `render_link`, declined under D).  Best candidates for a stage split, in
  order of size: `library/sclibrar_sitstand.cpp` (`lib_stand_sit_lie` and
  two helpers, 198 / 159 / 154 lines, in one file), `tools/scdump.cpp`
  (`scdump_objects`, `_objloc`, `_events`, `_exits`: 197 / 171 / 168 /
  158; debug-only, so they are the safest to cut),
  `library/sclibrar_examine.cpp` (`lib_cmd_examine_self` 194,
  `_object` 169), `library/sclibrar_go.cpp` (`lib_cmd_go_place` 184,
  `lib_go` 182), `core/sctasks_endgame.cpp`
  (`task_print_end_game_message` 173), `adrift5/run/a5run_save.cpp`
  (`restore_fd_game` 189, mirror `ser_load_game_body`'s section split),
  `adrift5/run/a5run_resp.cpp` (`resp_flush` 187),
  `runner/scrun_dispatch.cpp` (`run_line_t::get_outer_400` 176,
  `settle` 173, `library` 156), `core/scbattle.cpp` (178),
  `core/screstrs.cpp` (three at 159–179).  Each wants its own
  corpus-diffed step; `library/` ones need the transcript recompare.

- **N3. Remaining `goto`**: `goto refuse` at
  `library/sclibrar_put_multiple.cpp:1730` is the only one left in the
  tree (confirmed by grep); `lib_put_named_pre400` (176 lines) is the
  function holding it, so the N2 split and the `goto` removal are one step.

- **N4. Sanity items that came out clean** (so they need no work):
  no `#if 0`, no TODO / FIXME in code, no `using namespace` in headers,
  no `sprintf` / `strcpy` / `strcat` calls; 330 `NULL` occurrences, all in
  comments or strings; no exported function (other than group A) with
  fewer than three non-test references (definition, declaration, use).  The nm-based dead-code check from
  the second pass has not been re-run.

## What the surveys found

Exact copy-paste was low (~136 duplicated 8-line windows in ~155k lines,
no static function defined in two files).  What there was is structural:
families of near-identical functions differing in a verb word, a
resolver callback or an ADRIFT version gate.  Those are Runner-parity
behaviour pinned by the transcripts, which is why most of section 1 is
in the declined list rather than the done one.

The second pass confirmed it: no two function bodies of 6+ lines are
byte-identical after whitespace normalisation.  It also found that the
first pass's size census had missed functions: a column-0 `{`…`}` census
(test/, build/ and glkscarier/ excluded) still showed 20 over 200 lines
and 61 over 150, none of the 20 in the first table.

No `using namespace` in headers, no `#if 0` blocks, no TODO / FIXME
markers in the engine (the only hits are references to
`RUNNER_TESTS_TODO.md` / `WINE-TRANSCRIPTS-TODO.md` in comments).

### How the second pass was measured

So that a later pass can repeat it:

- Dead code: `nm -P` over `build/headless/{plain,dump}` plus `glk/*.o`
  (defined global, never undefined anywhere), then `grep -w` over the whole
  Spatterlight tree to separate "no callers" from "called only inside its
  own TU".
- Duplicates: every column-0 `{ … }` function body with at least 10 lines,
  identifiers / literals / numbers normalised, MD5-bucketed.
- Size: every function from a column-0 `{` to the next column-0 `}`.

## Done, and where it landed

### First pass

| item | result | commit |
|---|---|---|
| 1.4 Glk event loops | `gsc_service_window_event()`; both mains split into init / loop / shutdown | 18e8b35d5, 209c1092f, 1d8a133e3 |
| 1.5 a5 recursion guards | `A5_PLACEMENT_DEPTH_MAX` | 6a41cb252 |
| 1.7 map fallback chain | `sm_try_sidestep()` | d0e8ef874 |
| 1.8 TAF schema strings | descriptors identical across versions are named constants | d1cda3926 |
| 2.2 include boilerplate | `library/sclibrar.h`, `adrift5/run/a5run_internal.h` umbrellas | 8f9a7ae69 |
| 2.3 unsafe string ops, raw buffers | no `sprintf` / `strcpy` / `strcat` left; function-local buffers owned | 718cc3c1d, 7c0950702, 6fb39dcb7 |
| 2.4 `(scr_int)` / `(scr_bool)` casts | six no-ops removed; the other 147 are real conversions | 3586df343 |
| 3 big functions | the 31 functions over 200 lines that the first census listed, split into stages or a context struct with members (the 20 it missed are D below) | one commit each, 2026-10-11 |
| 4 Win32 open-by-path | `gsc_open_game_path` in `glk/os_glk_platform.cpp` | 7439c3024 |
| 5 source lists | both makefiles glob; only Xcode needs a hand edit | 42e2cb5ac |
| 5 tree layout | `core/`, `platform/`, `tools/`, `headless/`; dump prints through `scr_dump_log` | 8c8dd03e4, dc15e7ae2, 676ba0a39, 6f2f0c464, 1ae8796eb |
| 5 `scprotos.h` | 25 module headers under `core/` plus a 56-line umbrella | ea280798f, 75e954513 |
| 6 stale comments | every pointer names the owning file | 31b77eaac |
| 6a accessor guards, loops | `GS_CHECK*`, `GS_FOR_EACH_*` | 64327f19f, 19f826821 |
| 6a module state | `uip`, `pf`, `expr`, `restr`, `ser`, `tasks`, `palette`, `disambig` state structs | 5edd9064b, 5d883d191, ede2fd91e, ae6ebb6e6, a9f9f36e5, 71339c36f, 60e45292c, 2c080c546 |
| 6a error handling | index half of the `scgamest.cpp` guards is `scr_fatal`, not `assert` | 3054518b4 |
| 6a `NULL` → `nullptr` | code only; comments and strings keep the word | 0a36d0eca |
| 6a `memset`, `(void) x` | six struct `memset`s are `= {}`; 21 unused parameters unnamed | 546456ea7 |
| 6a long lines, macros | six function-like macros are functions; 32 of 34 lines over 100 columns wrapped | 4391e6fee |
| 6a locale / ctype | `loc_table_lookup<T>`; shared low halves of the code-page tables; one `NUL` / `WHITESPACE` in `core/sctypes.h` | 2d53d5857, 6258795aa, c788c4035, cb5a07a4a |
| 6a file splits | `scparser` 5 TUs, `scprintf` 5, `sctasks` 5, `sclibrar_disambig` 3, `sclibrar_take` 2, `sclibrar_put` 2, `scrun_match` 3; each with its own `*_internal.h` | 82f5fb5e2, 0f18acb3f, 26365ef84, 8811b5eee, 55fd2004b, d04c37285, 8d4895136 |

`gs_is_game_valid` was 5% of self time in the 2026-10-04 profile; the
`GS_CHECK` fold (64327f19f) kept its test to the same compare and branch
and compiles the game half out under `NDEBUG`.

### Second pass (7d2f82a33)

**A. Dead code: deleted.**  Each had only its definition and declaration
(plus, in places, a comment).

- `headless/sxutils.cpp`, `sxglob.cpp` and `sxprotos.h` are gone entirely
  (`sxr_malloc`, `sxr_realloc`, `sxr_free`, `sxr_trim_string`,
  `sxr_normalize_string`, `sxr_fopen`, `glob_match`: SCARE `sx`
  leftovers, nothing else in the files was used).  `sxstubs.cpp` keeps
  only the live os-layer stubs; `stub_debug_trace`,
  `stub_attach_handlers` and `stub_detach_handlers` are out.
- `gs_event_valid` (`core/scgamest.cpp`), `pf_buffer_join_always`
  (`core/scprintf_buffer.cpp`, with the comment block in `scnpcs.cpp`
  that justified it), `a5model_variable`, `a5state_var_text_by_name`,
  `a5run_input_pending`, and the plain `render_comp_test` (the variant
  that also hands back the marked-up render is the live one).

**B. Exported but used only inside one TU: `static` for four of seven.**
`a5blorb_find`, `var_put`, `mark_player_arrival_seen` and
`replace_expressions` are `static` and out of their headers.

**C. Duplicates: 1, 3 and 5 done.**

- C1: the two `lib_definite_prefix` functions (same name, different
  signatures and rules) are now `lib_definite_prefix_400` in
  `library/sclibrar_dispatch.cpp` (4.0's name-builder rule) and
  `lib_definite_prefix` in `library/sclibrar_print.cpp` (carries the
  pre-3.9 `tense()` arm and the measured empty-prefix note).  The comment
  blocks cross-reference each other; the rules differ, so neither calls
  the other.
- C3: one alignment helper under `gsc_handle_center_tag` /
  `gsc_handle_right_tag` (`glk/os_glk_output.cpp`).
- C5: one hint-text helper under the four `core/scinterf.cpp` hint
  getters.

**D. Functions over 200 lines: 19 of 20 split.**  No new files; each
function became a driver plus static stages, or a context struct with
stage members whose bodies moved verbatim:

- `var_get_system` → a `{name, getter}` table; `ser_load_game_body` →
  sections mirroring the save side; `gsc_startup_code` → loading window,
  ADRIFT 5 load, and one start function per engine.
- `act_set_tasks`, `act_set_variable`, `act_move_character` → one function
  per action kind; `task_run_move_npc_action` → a player and an NPC half.
- `run_game_commands_common` → `run_commands_t`; `run_match_task_commands`
  → `run_command_match_t`; `run_task_refusal` → scan + print.
- `execute_task_with_overrides` → `override_frame_t`; `scan_tasks` →
  `task_scan_t`; `resolve_refine` → `refine_t`.
- `pf_filter_input` → `pf_filter_synonyms` + `pf_filter_builtins`;
  `lib_lock_backend` → `lib_lock_has_key_400` + `lib_lock_with_key`;
  `lib_two_verb_line_pre400` → `lib_two_verb_rewrite_pre400` (the five
  `goto rewrite` are gone).
- `lib_take_backend_common` → `lib_take_t` (game commands, 3.9 fit count,
  acquire + take line, then one member per report).
- `lib_put_in_backend` → `lib_put_in_t` (game commands, the two 3.9 arms,
  the move, then one member per refusal).
- `lib_disambiguate_object_common` → `lib_disambig_t`, thirteen stages in
  the original order; a stage that settles the question returns TRUE with
  the answer in `result`.  The `goto pre400_take_done` is gone.

The versions interleave in take / put / disambiguate, so those were cut
by stage, not by version, and checked against the corpus diff: that is
where Runner-parity ordering lives.

**E. `SCR_TRACE_ON (name)` / `SCR_TRACE_IF (name, format, …)`** in
`core/scdump.h`, next to `SCR_DEVIATION` (done after 7d2f82a33).  Each
use reads its environment variable once, the first time it is reached,
and both compile to nothing without `SCARIER_DUMP_TOOLS`.  They replace
the hand-written reads of the guarded trace blocks in `runner/`
(`SCR_TRACE_MATCH`, `_ADMIN`, `_EVENT_TASK`, `_SCOPE`) and `library/`
(`_MATCH`, `_CO`); the two `SCR_TRACE_MATCH` reads and the
`SCR_TRACE_RCACHE` one that called `getenv` on every pass are now cached.
Verified by the suite signature and by the stderr of three walkthroughs
run with every flag set, both identical before and after.  Not converted,
on purpose:

- `SCR_TRACE_BATTLE` (`core/scbattle.cpp`) and `SCR_TRACE_RAND`
  (`core/scutils.cpp`) are *not* under `SCARIER_DUMP_TOOLS`: they work
  in the plain build, which the macro would end.  `SCR_TRACE_RAND` is
  also re-read at each seeding.
- `SCR_TRACE_RCACHE` carries a task number, so it keeps its `#ifdef`
  and has a cached read of its own.
- `SCR_TRACE_SOUND` (`platform/os_ansi.cpp`) prints into the game's
  output in the plain build; `SCR_TRACE_FLAGS` is SCARE's trace mask,
  read at startup.  `os_ansi.cpp`'s `SCR_TRACE_ADMIN` read was already
  cached, and the file keeps clear of the `core/` headers' TRUE / FALSE
  macros.
- `tools/scdump.cpp` caches its own flags.

**Verification of the second pass**, on the whole working tree; every
step was behaviour-neutral:

- `make -f Makefile.headless test`: exit 0, normalised output signature
  identical to the pre-survey baseline (1049 lines).
- `runner_transcripts.py recompare` with the baseline binary and with the
  new one: `manifest.tsv` and every `compare/*.txt` report identical
  (652 rows).  Run after the lock/two-verb/input-filter splits, again after
  take, and again after put + disambiguate.
- `make -f Makefile.headless sanitize`: ASan/UBSan clean.
- `run_autosave_tests.py --build`: all passed.
- `make -j8 scarier glkscarier`: no warnings.  `xcodebuild -target scarier`:
  BUILD SUCCEEDED.  No file was added, so the Xcode project is unchanged
  apart from the three removed `headless/` files (not in the project).
- Each split TU: `-fsyntax-only -Wall -Wextra -Wshadow` clean.

Found on the way, not caused by this work: the
`test/adrift4/runner_transcripts/manifest.tsv` committed at the time was
stale against HEAD's own binary (617 rows "identical" as committed, 494
when recompared with a binary built from HEAD; `3monkeys` "is" → "are"
is one).  The recompare above was therefore HEAD-binary vs new-binary,
not committed vs new.

New files go into the Xcode project by hand; the next free id in the
`5C1B1A…` series is 0118.

### Third pass (2026-10-15, wip @ b9d1635)

Read-only; no code changed.  Measured with the same recipes as the second
pass, minus the `nm` step (no build here):

- **Size**: 179 `.cpp` / `.h` files, 156,440 lines.  Largest files:
  `library/sclibrar_drop.cpp` 2736, `mapdraw.cpp` 2726, `core/scdebug.cpp`
  2527, `core/scvars.cpp` 2515, `core/scgamest.cpp` 2404.  None stands out
  against the "File sizes" note under "Left on purpose".
- **Duplicates**: column-0 `{ … }` bodies of 10+ lines with identifiers,
  strings and numbers normalised, MD5-bucketed: 26 buckets.  Of these, the declined ones are C2, C4, C6, C7, C8, C9
  and `lib_cmd_wear_all` / `_remove_all`; the rest are N1 above.
  Still no two bodies byte-identical after whitespace normalisation.
- **Dead code**: the Open A group is unchanged (each name has exactly one
  reference outside `core/scarier.h` / `scinterf.cpp`: its own
  declaration).  Every other identifier defined at column 0 appears at
  least three times in the non-test tree (definition, declaration, use);
  the only two-reference ones are `a5parse_ref` and
  `a5state_var_num_by_name`, both used by the test harnesses
  (`test/adrift5/harness/`), and `dbg_conv`, a file-static.
- **Env flags**: 67 `getenv` lines in 17 `.cpp` files outside `test/` (the second pass
  quoted 22 files; some reads were folded into `SCR_TRACE_ON` / `_IF`
  since).  2.1 stays declined.
- **File-scope statics**: files with more than 8 are `core/sctafpar.cpp`
  (19), `glk/os_glk_map.cpp` (13) and `glk/os_glk_autosave.cpp` (13);
  the same census as "The rest of the file-scope statics" would start
  with those three.

Note on commit hashes: the hashes quoted in this file name commits on
`master` (some may not exist on `wip` after its rebase; this checkout is
also shallow, 50 commits).  Use `git log --grep` on the subject, or
`git show <hash>` on a full clone of master.

## Declined (do not re-propose without new evidence)

### First pass

- **1.1 verb quartets** (the "all / except / multiple / common" stacks
  per verb): measured Runner behaviour per verb, ordering accidents
  included, pinned by `runner_transcripts/`.  A generic driver would
  need every per-verb difference as a parameter.  Revisit only with the
  corpus diff in hand.
- **1.2 version-gate rename / `lib_dialect_t` predicates**: ~475 call
  sites, cosmetic, and the inconsistent spellings are greppable.
- **1.3 person-form triples** (`lib_select_response` You / I /
  %player%): the triples are what make the Runner text greppable
  against the transcripts.
- **1.5 `for_each_walk_control`**: the nine loops share a shape, not a
  body; a helper would need flags.
- **1.6 operator case lists** in scexpr.cpp: the lists are the dispatch.
- **1.8 delta macros** for the per-version TAF records: the exact field
  order is the point of the table.
- **1.9 a5parse / a5state tails**: `re_group_end()` and
  `re_has_toplevel_alt()` leave the shared walk differently;
  `exists_at()` and `location_root_at()` port two different Runner
  functions whose arms differ on purpose.
- **2.1 env registry** (`scr_env`, generated env reference): cosmetic
  and wide, and the determinism harness depends on when flags are read
  (`getenv` reads are spread over 17 files outside `test/`, 67 lines in all, each for a debug flag).
  The `os_*` `SCR_MARK_*` / wrap / echo sharing under 4 depended on it.
- **`scarier.hdr`**: maintained by hand, not a build input.
- **`lib_ctx_t` context struct** for the 251 `bundle` / 197 `filter` /
  59 `vars` prologue lines: a signature redesign, not a mechanical
  step.  The 313 `lib_cmd_*` handlers are table-dispatched as
  `(scr_gameref_t)` and the lines sit in static helpers across 40
  files.  The mechanical half, if ever wanted, is to include scgamest.h
  from library/ and read `game->bundle` directly.
- **Parse-once dispatch input**: the 87 `run_get_dispatch_input ()`
  sites do not share a tokenisation (`strstr` tails, `std::string`
  splits at "in" / "on" / "from", two word-boundary sets).
- **`strcasecmp` wrappers**: every raw call left is in `adrift5/`,
  where it is right: the text is UTF-8 and the `scr_` wrappers would
  fold bytes >= 0x80 through the 4.x cp1251/cp1252 table.
- **`scr_unsupported`**: nothing in the 4.x engine reports an
  unsupported feature.
- **`scr_fatal` `default:` arms into asserts**: a throw the host
  boundary catches is the gentler failure.
- **`-Wconversion`-clean tree**: 803 warnings, 777 of them
  sign-conversion; fixing them means about 800 casts of the kind 2.4
  set out to remove.
- **`WHITESPACE` vs `scr_isspace()`**: they differ on 0xa0 (NBSP),
  which only the locale predicate counts.  SCARE heritage and
  Runner-facing; `core/sctypes.h` records the difference.
- **Loop index renames** (`object` / `index_` / `obj` / `i`): renaming
  is where the risk is, and `GS_FOR_EACH_*` makes the drift harmless.
- **`InitGlk` extern in os_glk.cpp**: Windows Glk's; `WinGlk.h` is not
  in the tree, so a header declaration cannot be checked here.
  `os_ansi.cpp` likewise keeps its hand-written `extern
  run_probe_strict`: scrunner.h would drag scprotos.h's TRUE / FALSE
  macros onto its enum.

### Second pass

- **B. `obj_seen_p`, `obj_visible` (`adrift5/run/a5run_ref.cpp`) and
  `a5state_object_key_at_location` stay exported**: each is called above
  its definition, so `static` would trade the header line for a forward
  declaration in the `.cpp`, and the first two sit in `a5run_internal.h`
  beside `char_visible` / `char_seen_p`, which other TUs do use.
  (`a5run_undo_peek`, `a5run_undo_push_blob`, `a5run_pending_save` and
  `a5run_pending_restore` are used only by `glk/os_glk_autosave.cpp`;
  that is their purpose and `a5run.h` says so above each group.)
- **C2 `glk/os_glk_locale.cpp`**: the `cp850 → unicode` / `cp866 →
  unicode` and `cp1252` / `cp1251` blocks hash identically, but that is
  table data in the same shape, not code.  Noted so it isn't
  rediscovered.
- **C4 `lib_cmd_wear_all` / `lib_cmd_remove_all`**
  (`library/sclibrar_drop.cpp`, 30 lines; the difference is two strings
  and one predicate): part of the verb family 1.1 declined, and merging
  it buys 30 lines against Runner-parity risk.
- **C6 `scr_platform_rand` / `scr_congruential_rand`**
  (`core/scutils.cpp`): identical but the seed-0 default (0 vs 1).  The
  bodies must stay distinct functions for function-pointer identity; a
  shared inline called by two thin distinct functions would keep that.
  Low value; only if touching the file.
- **C7 `restr_match` (screstrs.cpp) vs `expr_parse_match` (scexpr.cpp)**:
  the same "if token == expected advance else error" step in two
  parsers, static to different TUs with different error types.
- **C8 `debug_print_room` / `debug_print_event`** (`core/scdebug.cpp`,
  16 lines each, same skeleton): debugger-only; skip unless the file is
  being restructured.
- **C9 `adrift5/a5model.cpp`**: `a5_load_objects` / `a5_load_groups` and
  `a5_load_propdefs` / `a5_load_hints` share the "for each child node,
  fetch these fields" skeleton.  A `for_each_child` helper would remove
  ~25 lines; the loader has no tests of its own beyond the corpora.
- **D `render_link`** (`mapdraw.cpp`): 210 lines of which about half are
  comments, a linear run of gates that was itself already extracted from
  `map_render`; cutting it further would only scatter the gates.
- **E header move**: `adrift5/a5run.h`, `a5text.h`, `a5util.h` sit in
  `adrift5/` while their implementations are in `adrift5/run/` and
  `adrift5/text/` (where the `_internal.h` files live).  Moving them is
  cosmetic and costs Xcode header-search-path churn; a comment in each
  saying it is the public face is the cheap alternative.
- **E `map/` directory**: `mapdraw.cpp` / `mapdraw.h` / `sessrec.h` stay
  at the root (the Makefile explains "engine-neutral map renderer, stays
  at root").  `scmap` is 4.x-only, so only `mapdraw` and `sessrec.h`
  would move.  Cosmetic.
- **E `<cstdio>` / `<cstring>` sweep**: 45 of the `.cpp` files still
  include the C-style headers.  Pure noise; only if something else is
  touched.
- **Section 4's table** (RNG, map, save, text, input existing once per
  engine): surveyed; only the RNG core was worth sharing, and is.
  - *RNG*: the one real duplicate, and the one thing shared after the
    survey.  `core/scutils.cpp` had its own SplitMix32-seeded
    xoshiro128** for the Runner-compatible generator; it now calls
    `erkyrath_random()` in `common_utils/randomness.c`, as
    `adrift5/a5rand.cpp` does.  `randomness.[ch]` lost their `glk.h`
    include (`glui32` -> `uint32_t`) so the plain `scarier` target can
    link them.  The layers above are not the same and stay apart: 4.x
    presents `(w >> 8) << 7` to a multiply-shift to reproduce VB's
    `Int(Rnd * n)`, ADRIFT 5 takes `w % span` as .NET's `Next` does,
    and each keeps its own draw count and trace format.
  - *Map*: `core/scmap.cpp` computes a layout (the Runner's algorithm);
    `adrift5/a5map.cpp` reads the map the author stored in the game.
    They meet at `map_t`, and `mapdraw.cpp` and `glk/os_glk_map.cpp`
    are already single.
  - *Save*: `core/scserial.cpp` writes the Runner's line-per-field
    `.tas` layout, `a5run_save.cpp` the ADRIFT 5 Runner's XML.  Both
    are fixed by the file formats.  Their zlib use differs too:
    streamed through the caller's read/write callbacks in 4.x, whole
    buffers with an expansion ceiling in `a5_inflate()` / `a5_deflate()`.
  - *Text*: 4.x expands `%markers%` and hands tags to the front end one
    `os_print_tag()` call at a time; ADRIFT 5 evaluates functions and
    expressions into one string of marks that `gsc_a5_display()` walks.
    The Glk side under both (colour, fonts, symbol fonts, status line,
    prompt, echo) is the same `gsc_*` code already.
  - *Input*: `scparser*.cpp` matches author patterns;
    `a5run_words.cpp` is pronoun and synonym substitution and the
    NotUnderstood ladder of the ADRIFT 5 Runner.  No common table.

## Left on purpose

These are candidates, not tasks; each wants its own decision.

- **ADRIFT 5 `char *` returns.**  `adrift5/` has ~50 `malloc` /
  `calloc` and ~360 `free`: text functions return a `malloc`ed string
  the caller frees.  Wrapping that is an API change across a5text /
  a5run / the Glk front end / every harness, and wants a leak checker
  that runs here (LeakSanitizer does not on macOS/arm64; `leaks
  --atExit` might).
- **The ~45 remaining raw `scr_malloc` sites in the 4.x engine** are
  struct owners with a destroy function, strings handed to
  `prop_adopt()`, pool entries, or stack-buffer-with-heap-fallback.
  The second pass spot-checked the 78 raw `new` / 28 `delete` with the
  same verdict: struct owners with destroy functions.
- **One `goto`** is left, `goto refuse` in
  `library/sclibrar_put_multiple.cpp`; the other six went with the D
  splits.
- **The rest of the file-scope statics.**  The state-struct pass covered
  the modules the survey named.  The other library files have their own
  (`lib_battle_who_reset()`, `lib_with_prefix_390_reset()`,
  `lib_put_reset()` show three more reset hooks; the eight take / put
  flags are in their internal headers), and runner/ has its line
  buffers.  A census of those is the next step if two games in one
  process is ever a goal.
- ~~**`uip.options` passed by value**~~ into `uip_match()`: declined,
  and the hand-written pairs replaced with guards instead.  No bracket
  encloses a `uip_match()` call.  Each one holds an option over a whole
  task loop or command table (`run_game_task_commands()`,
  `run_try_command_table()`, `run_priority_commands()`,
  `run_line_matches_task_strictly()`), and the readers are many calls
  down in `scparser_entity.cpp`; the library handlers a table row runs
  match again under the same option.  By value means one more parameter
  on every function between, about forty `uip_match()` callers
  included, to say what the scope already says.  What was wrong with
  the pairs was that they were written out by hand: every one is now a
  guard in `core/scparser.h` (`scr_containment_guard`,
  `scr_binary_input_guard`, beside the two that existed), and
  `run_lenient_tasks`, the runner's own copy of the lenient flag kept
  in step by double writes, is gone for `uip_get_lenient_tasks()`.
  Left as a pair: `run_command_match_t::tree_match()` turning the
  strict binding off and on again around its own match.
- ~~**Startup linkage**~~: done.  `glk_main`, `glkunix_startup_code`
  and `WinMain` / `winglk_startup_code` moved to
  `glk/os_glk_platform.cpp` (70 -> 390 lines; `os_glk.cpp` 1885 ->
  1564).  `gsc_startup_code()`, `gsc_main()` and `gsc_game_message`
  lost `static` for it.  Checked in the Gargoyle configuration
  (`-DGARGLK` against a garglk checkout, compiled and linked); the
  plain Windows Glk branch is not built here.
- **File sizes.**  The largest are `mapdraw.cpp` (2726) and
  `library/sclibrar_drop.cpp` (2736), with a dozen more between 2000
  and 2530; none stands out, so no further split is proposed.
- **Functions between 150 and 200 lines**: the second census counted 61
  over 150; only the 20 over 200 were taken up.
- ~~**Glk Makefile dependencies**~~: done.  `Makefile` compiles with
  `-MMD -MP` and includes the `.d` files, in place of the
  `$(OBJECTS): $(HEADERS)` rules that rebuilt everything on any header
  edit.  `make clean` is rebuilt on the object lists (it still named
  `core` as a dump file and missed `core/`, `platform/` and the
  `adrift5/` subdirectories).
- ~~**Fatal message in the game window**~~: done.  `if_report_fatal()`
  keeps the message for `scr_take_fatal_message()`; the Glk front end's
  `gsc_report_fatal()` reports it after `scr_interpret_game()`,
  the game-over menu, a failed load and the meta-commands: through
  `win_showerror()` under `SPATTERLIGHT` (exiting with a failure status
  when the game must stop and errors are set to be ignored, so the
  termination sheet carries the message), in the main window for any
  other Glk library.  ADRIFT 5 has no fatal path of its own: its load
  and out-of-memory stops already print in the window.
- **Lines over 80 columns in `adrift5/`** (about 1460): its compact
  one-line bodies are that code's own style.  Two lines over 100 stay
  everywhere: sctafpar_patches.cpp's cp1251 command string and the
  expression quoted in a5sexpr.h.
- **Five `(void) x` casts under `#if`** in glk/: used on one side of a
  SPATTERLIGHT / GARGLK conditional; `[[maybe_unused]]` is C++17 and
  Makefile.headless builds `-std=c++11`.
- **Macros that stay**: `scr_setjmp` / `scr_longjmp`, `GS_CHECK*`,
  `GS_FOR_EACH_*`, `PATCH_*`, `SCR_DEVIATION`, `RUN_TOKEN_IS`; each
  needs the caller's line, a loop header, an aggregate initialiser or
  `sizeof` of a literal.

## Safety net (for any further item)

1. `make -f Makefile.headless quick` (≈4 s, no corpora) after each small
   step.
2. Full `make -f Makefile.headless test` (≈43 s, parallel stages) before
   commit; diff the whole output against a pre-change run with the
   compile lines stripped and the rest sorted, not just pass/fail.
3. `make -j8 scarier glkscarier` and `xcodebuild -target scarier`; new
   or renamed files must be added to the Xcode project.
4. `make -f Makefile.headless sanitize` after any memory-management
   change.  It runs with `detect_leaks=0`, so it checks overruns and
   use-after-free, not leaks.
5. For text or ordering changes in `library/`: diff the
   `runner_transcripts/` corpus; "Runner-identical" must stay identical.
   Recompare with a binary built from the pre-change tree rather than
   trusting the committed `manifest.tsv`, which can be stale.
6. `test/adrift4/harness/run_autosave_tests.py --build` after touching
   serialise or the Glk main loops.
7. For a file split: the sorted line multiset of the new files against
   the old one, and a `-Wall -Wextra -Wshadow` syntax check (a copied
   `type name = …;` can shadow a struct member).
8. One commit per mechanical step, behaviour-neutral, so a bisect lands
   on a single rename or extraction.
