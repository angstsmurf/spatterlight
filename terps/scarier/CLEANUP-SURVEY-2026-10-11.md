# Scarier: second cleanup survey — results

Survey taken 2026-10-11 (master @ 9189587ac) after REFACTOR-TODO.md was
closed; worked through the same day.  Each section below keeps the original
suggestion and opens with a **Result** line saying what was done.

How it was measured (so a later pass can repeat it):

- Dead code: `nm -P` over `build/headless/{plain,dump}` plus `glk/*.o`
  (defined global, never undefined anywhere), then `grep -w` over the whole
  Spatterlight tree to separate "no callers" from "called only inside its own
  TU".
- Duplicates: every column-0 `{ … }` function body with at least 10 lines,
  identifiers / literals / numbers normalised, MD5-bucketed.
- Size: every function from a column-0 `{` to the next column-0 `}`.

## Outcome

- **A** done, except the public-API list, which is an open question (below).
- **B** done for four of seven; three left exported on purpose.
- **C** 1, 3 and 5 done; 2, 4, 6–9 declined.
- **D** 19 of 20 split; `render_link` declined.
- **E** nothing done (all optional; see the section).

Every step was behaviour-neutral.  Verified at the end, on the whole
working tree:

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

Found on the way, not caused by this work: the committed
`test/adrift4/runner_transcripts/manifest.tsv` is stale against HEAD's own
binary (617 rows "identical" as committed, 494 when recompared with a
binary built from HEAD; `3monkeys` "is" → "are" is one).  The recompare
above was therefore HEAD-binary vs new-binary, not committed vs new.

---

## A. Dead code — delete (no caller anywhere in the tree)

**Result: done.**  Every row of the table is deleted.  `headless/sxutils.cpp`,
`sxglob.cpp` and `sxprotos.h` are gone entirely (nothing else in them was
used); `sxstubs.cpp` keeps only the live os-layer stubs.  The public-API
list is **not** touched: whether `scarier.h` is a supported library surface
is the maintainer's call.

Each has only its definition and declaration (plus, in places, a comment).

| symbol | file | note |
|---|---|---|
| `sxr_malloc`, `sxr_realloc`, `sxr_free`, `sxr_trim_string`, `sxr_normalize_string`, `sxr_fopen` | `headless/sxutils.cpp`, `sxprotos.h` | SCARE `sx` leftovers; `sxr_malloc` / `sxr_realloc` are line-for-line copies of `scr_malloc` / `scr_realloc` (`core/scutils.cpp`).  Check `sxr_zero_allocation` and any `sxr_fatal` users go with them |
| `stub_debug_trace`, `stub_attach_handlers`, `stub_detach_handlers` | `headless/sxstubs.cpp`, `sxprotos.h` | handler hooks from the old `sx` build; never called |
| `glob_match` | `headless/sxglob.cpp`, `sxprotos.h` | check whether anything in `sxglob.cpp` besides it is used; if not the file goes |
| `gs_event_valid` | `core/scgamest.cpp`, `scgamest_api.h` | |
| `pf_buffer_join_always` | `core/scprintf_buffer.cpp`, `scprintf.h` | two comments in `scnpcs.cpp` mention it; no call.  Remove with the comment block that justifies it |
| `a5model_variable` | `adrift5/a5model.cpp`, `.h` | |
| `a5state_var_text_by_name` | `adrift5/a5state.cpp`, `.h` | |
| `a5run_input_pending` | `adrift5/run/a5run_save.cpp`, `a5run.h` | |
| `render_comp_test` | `adrift5/run/a5run_resp.cpp`, `a5run_internal.h` | the neighbouring "render_comp_test, additionally handing back the MARKED-UP render" variant is the live one; confirm and drop the plain one |

Public-API functions in `core/scarier.h` / `scinterf.cpp` with **no caller in
this tree** (the Glk front end never uses them).  Decide per function whether
`scarier.h` is a supported library surface (Gargoyle takes the folder as a
drop-in, per `common_utils/README`) — if it is, leave them and say so in the
header; if not, delete:

`scr_does_game_use_graphics`, `scr_get_game_compile_date`,
`scr_get_game_max_score`, `scr_get_game_preferred_font`,
`scr_get_game_bold_room_names` / `scr_set_game_bold_room_names`,
`scr_get_game_notify_score_change` / `scr_set_game_notify_score_change`,
`scr_get_game_patches`, `scr_get_capacity_assist`, `scr_get_end_keyprompt`,
`scr_get_game_debugger_enabled`, `scr_run_game_debugger_command`,
`scr_scarier_emulation`, `scr_scarier_version`,
`scr_load_game_from_filename` / `_stream`, `scr_save_game` /
`scr_save_game_to_filename` / `_stream`.

The `scr_save/load_*_stream/filename` family is the clearest case: nothing
outside `scinterf.cpp` calls them, and the Glk front end goes through the
`*_callback` forms.  Deleting them would also make `if_file_read_callback`
/ `if_file_write_callback` unreachable.  (`scr_game_from_stream` is *not*
in the family: `platform/os_ansi.cpp` loads through it.)

## B. Exported but used only inside one TU — make `static`

**Result:** `a5blorb_find`, `var_put`, `mark_player_arrival_seen` and
`replace_expressions` are `static` and out of their headers.  `obj_seen_p`,
`obj_visible` and `a5state_object_key_at_location` stay exported: each is
called above its definition, so `static` would trade the header line for a
forward declaration in the `.cpp`, and the first two sit in
`a5run_internal.h` beside `char_visible` / `char_seen_p`, which other TUs do
use.

Not dead, but wider than they need to be; a header line each can go.
(`nm` says nobody else links to them.)

- `a5blorb_find` (`adrift5/a5blorb.cpp`; used once, line 150)
- `var_put` (`core/scvars.cpp`; `var_put_integer` / `var_put_string` are
  the only callers)
- `obj_seen_p`, `obj_visible` (`adrift5/run/a5run_ref.cpp`)
- `mark_player_arrival_seen` (`adrift5/run/a5run_act.cpp`)
- `replace_expressions` (`adrift5/text/a5text.cpp`, declared in
  `a5text_internal.h`)
- `a5state_object_key_at_location` (`adrift5/a5state.cpp`)

(`a5run_undo_peek`, `a5run_undo_push_blob`, `a5run_pending_save`,
`a5run_pending_restore` are used only by `glk/os_glk_autosave.cpp`; that
is their purpose and `a5run.h` already says so above each group.)

## C. Duplicates and near-duplicates worth a decision

**Result:** 1 done (`lib_definite_prefix_400` in dispatch, comments
cross-referenced; the rules differ, so neither calls the other).  3 done
(one alignment helper under the two tag handlers).  5 done (one hint-text
helper under the four getters).  2 and 7 need nothing, as written.  4
declined: it is part of the verb family 1.1 already declined, and merging
it buys 30 lines against Runner-parity risk.  6, 8, 9 declined
as low value, as the survey itself rates them.

Exact or whitespace-only clones are almost gone (confirmed again: no two
function bodies of 6+ lines are byte-identical after whitespace
normalisation).  Structural clones that remain, strongest first:

1. **`lib_definite_prefix`, defined twice with different signatures**
   (`library/sclibrar_dispatch.cpp:199` returns `const char*` with a buffer;
   `library/sclibrar_print.cpp:791` takes `is_pre_390` and appends to a
   `std::string`).  Both `static`, so it links, but the shared name and
   different semantics (one is 4.0's name-builder rule; the other carries
   the pre-3.9 `tense()` arm and the measured empty-prefix note) invite a
   wrong fix.  Rename one
   (`lib_definite_prefix_400` for the dispatch one) and cross-reference the
   two comment blocks; if the rules really are the same, one calls the other.
2. **`glk/os_glk_locale.cpp`**: `cp850 → unicode`/`cp866 → unicode`
   and `cp1252`/`cp1251` blocks hash identically — that is just table data
   in the same shape, not code.  Nothing to do; noted so it isn't
   rediscovered.
3. **`gsc_handle_center_tag` / `gsc_handle_right_tag`**
   (`glk/os_glk_output.cpp:997/1027`): the same ten-line body modulo
   center/right; only the comment block differs.  One helper with the
   alignment as a parameter, two one-line tags on top.
4. **`lib_cmd_wear_all` / `lib_cmd_remove_all`**
   (`library/sclibrar_drop.cpp:1627/2413`, 30 lines): the verb-quartet
   family 1.1 declined, but this pair is a *pair*, not a quartet, and the
   difference is two strings and one predicate.  Take it only with the
   `runner_transcripts` diff in hand.
5. **`core/scinterf.cpp` hint getters** (`scr_get_next_game_hint`,
   `_hint_question`, `_subtle_hint`, `_unsubtle_hint`: 17 lines each,
   identical shape).  A `static if_hint_text (game, name, fn)` helper
   removes ~50 lines.  Same for the `*_to_stream` / `*_to_callback` pairs —
   but see section A: most of that family may just go.
6. **`scr_platform_rand` / `scr_congruential_rand`** (`core/scutils.cpp`
   ~286/305): identical but the seed-0 default (0 vs 1).  The comment
   explains the bodies must stay distinct for function-pointer identity; a
   `static scr_int rand_with_default_seed (seed, dflt)` template/inline
   *called by* two thin distinct functions would keep that and drop the
   copy.  Low value; only if touching the file.
7. **`restr_match` (screstrs.cpp:1452) vs `expr_parse_match` (scexpr.cpp:1432)**:
   the same "if token == expected advance else error" step in two parsers.
   They're static to different TUs with different error types; fine to
   leave, listed for completeness.
8. **`core/scdebug.cpp` `debug_print_room` / `debug_print_event`**
   (16 lines each, same skeleton).  Debugger-only; skip unless the file is
   being restructured.
9. **`adrift5/a5model.cpp`**: `a5_load_objects` / `a5_load_groups` and
   `a5_load_propdefs` / `a5_load_hints` share the "for each child node,
   fetch these fields" skeleton.  A `for_each_child(a, "Object", fn)`
   helper would remove ~25 lines; the loader has no tests of its own
   beyond the corpora, so low priority.

## D. Functions still over 150 lines (the earlier census missed these)

**Result: every row split except `render_link`.**  No new files; each
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

`render_link` declined: 210 lines of which about half are comments, a
linear run of gates that was itself already extracted from `map_render`;
cutting it further would only scatter the gates.

REFACTOR-TODO.md says "all 31 functions over 200 lines" were split.  A
column-0 `{`…`}` census (test/, build/ and glkscarier/ excluded) finds
**20 still over 200 and 61 over 150**; none of the 20 was in the old
table.  The splits below are the same kind as the 31 already done
(context struct + stage members, or driver + stages).  Biggest first
(line numbers as surveyed):

| lines | function | file | shape |
|---|---|---|---|
| 829 | `lib_disambiguate_object_common` | library/sclibrar_disambig.cpp:495 | pre-4.0 vs 4.0 arms interleaved; the old log says the versions *interleave*, so cut by stage (candidate scan / question / answer / commit), not by version |
| 822 | `lib_take_backend_common` | library/sclibrar_take.cpp:877 | the log explicitly says "versions interleave"; stage split like `lib_take_multiple_common` was |
| 510 | `var_get_system` | core/scvars.cpp:1086 | a flat `strcmp` ladder — table of `{name, getter}` instead; also a good test of 6a "tables over ladders" |
| 472 | `run_match_task_commands` | runner/scrun_match.cpp:1060 | the largest function in the runner; `scrun_match.cpp` is already 1642 lines |
| 460 | `lib_put_in_backend` | library/sclibrar_put.cpp:723 | same story as take |
| 390 | `ser_load_game_body` | core/scserial.cpp:1623 | mirror of `ser_save_game_body`, which *was* split into six sections |
| 363 | `resolve_refine` | adrift5/run/a5run_ref.cpp:1108 | |
| 361 | `execute_task_with_overrides` | adrift5/run/a5run_action.cpp:610 | |
| 360 | `scan_tasks` | adrift5/run/a5run.cpp:669 | |
| 346 / 289 / 270 | `act_set_tasks`, `act_set_variable`, `act_move_character` | adrift5/run/a5run_act.cpp | switch over action kinds — one function per kind |
| 246 | `task_run_move_npc_action` | core/sctasks_move.cpp:616 | |
| 245 | `run_task_refusal` | runner/scrun_match_refusal.cpp:441 | |
| 241 | `lib_lock_backend` | library/sclibrar_open.cpp:1039 | |
| 235 | `run_game_commands_common` | runner/scrun_match_run.cpp:556 | |
| 227 | `gsc_startup_code` | glk/os_glk.cpp:996 | init / loader choice / restore / locale |
| 215 | `lib_two_verb_line_pre400` | library/sclibrar_drop.cpp:2186 | |
| 214 | `pf_filter_input` | core/scprintf_input.cpp:694 | |
| 210 | `render_link` | mapdraw.cpp:2135 | |

Three that are the cheapest wins: `var_get_system` (table), `ser_load_game_body`
(mirror of a split already done, so the section boundaries are known), and
`gsc_startup_code` (no Runner parity involved).

Do the take/put/disambiguate ones last and only with the corpus diff:
those are where Runner-parity ordering lives.

## E. Layout and housekeeping

**Result: nothing done.**  The header move and the `map/` directory are
cosmetic and cost Xcode search-path churn; the `<cstdio>` sweep is noise by
the survey's own account.  The `SCR_TRACE_IF` macro is the one item with a
real payoff (cached reads) and is still open.

- `adrift5/a5run.h`, `a5text.h`, `a5util.h` sit in `adrift5/` while their
  implementations are in `adrift5/run/` and `adrift5/text/`.  Either move
  the public headers beside the implementation (the `_internal.h` files
  already live there) or leave a comment in each saying why they are the
  public face.  Only the first costs includes: Xcode header search paths
  matter.
- `mapdraw.cpp` (2726 lines) / `mapdraw.h` / `sessrec.h` at the root: the
  Makefile explains "engine-neutral map renderer, stays at root".  A
  `map/` directory (mapdraw + `core/scmap` + `adrift5/a5map`?) is the
  alternative; `scmap` is 4.x-only, so only `mapdraw` and `sessrec.h`
  would move.  Cosmetic; decline is a reasonable outcome.
- 47 of the `.cpp` files still `#include <stdio.h>`-style C headers;
  `<cstdio>`/`<cstring>` would be consistent with `std::string` /
  lambdas elsewhere.  Pure noise, include only if something else is
  touched.
- `SCARIER_DUMP_TOOLS` trace blocks: 29 `getenv ("SCR_TRACE_…")` reads
  in 12 files (some cached in a `static`, some re-read on every call as
  in `library/sclibrar_dispatch.cpp:274`), each guarding its own
  `fprintf (stderr, …)`.  A `SCR_TRACE_IF (flag, fmt, …)` macro with a
  cached flag in `core/scdump.h`, next to `SCR_DEVIATION`, would make the
  uncached reads cached (and so determinism-neutral) and halve the line
  count.  This is *not* the declined env registry (2.1): no new names, no
  generated reference.

## F. Things I looked at and would leave alone

- ADRIFT 5 `malloc` / `free` (~50 `malloc`/`calloc`, ~360 `free`):
  already in "Left on purpose".
- 78 raw `new` / 28 `delete`, 7 `goto`s: spot-checked; the `goto`s are
  all in `library/` (five `goto rewrite` in `lib_two_verb_line_pre400`,
  one each in put-multiple and disambig) and the `new`s are struct owners
  with destroy functions.  Same verdict as the earlier malloc audit.
- `getenv` reads are spread over 22 files, but each is for a debug flag
  and the determinism harness depends on read time (2.1, declined).
- `using namespace` in headers: none.  `#if 0` blocks: none.  TODO / FIXME
  markers in the engine: none (the only hits are references to
  `RUNNER_TESTS_TODO.md` / `WINE-TRANSCRIPTS-TODO.md` in comments).
