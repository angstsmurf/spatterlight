# TODO — C++ modernization of the ADRIFT 4 / 3.9 / 3.8 path (SCARE)

The legacy SCARE engine (`sc*.cpp` — `sctaffil` → `sctafpar` → `scgamest` /
`sctasks` / `screstrs` / `scrunner` / `scprintf` / `scexpr` …) was renamed from
`.c` to `.cpp` and now compiles with the C++ compiler, **but the conversion is
nominal**: there are **zero STL headers in any `sc*.cpp`**, and the only visible
sign of C++ is mechanical `(decltype(x))` casts sprinkled on `void*` returns to
satisfy C++'s stricter rules (e.g. `scprintf.cpp:232`, `scutils.cpp:scr_malloc`).
It is "C compiled by a C++ compiler."

This file tracks turning that nominal conversion into real wins. The in-tree
model for "how to do it right" is the **a5 engine** (the ADRIFT 5 path), which
uses `std::string` / `std::vector` / `std::set` / `std::map` freely and contains
the bulk of the world logic in a C-like-but-modern style — follow its idioms.

> ⚠️ **Upstream-divergence caveat.** SCARE is Simon Baldwin's upstream code.
> This fork already diverges heavily (battle system, locale, RNG, dozens of
> bug-fixes), so the gap is partly a sunk cost — but every item below, especially
> P3/P4, widens it and complicates any future upstream merge. P1/P2 are the most
> defensible: self-contained, tied to a known crash/perf footgun, and exactly
> what "it's C++ now" makes cheap. Weigh each phase against the merge cost.

---

## 0. Ground rules / invariants

- **Byte-identical output is the bar.** SCARE has golden-transcript and
  ground-truth regressions; no modernization may change a single byte of game
  output. Validate every phase against:
  - `make -f Makefile.headless test` (battle / precedence / badparent `.taf`
    harnesses + the a5 suite),
  - the v4 walkthrough corpus in `adrift-walkthroughs/` (replayed headlessly),
  - the FrankenDrift/`run400`-derived expectations where they exist.
- **ASan/UBSan-clean** is mandatory on every change (the established bar for this
  tree). Build the headless harness with `-fsanitize=address,undefined` and run
  the corpus.
- **Bound every soak run.** Headless SCARE can exhaust RAM on event-heavy games
  (Menagerie/circus) — always wrap corpus runs in `ulimit -t 12` + `head -c 4M`
  (the scratchpad `safeplay.sh` recipe); never run them unbounded in the
  background.
- **Keep the public `extern "C"` surface unchanged** (`scr_*` in `scinterf.cpp`)
  so the os layers (`os_glk.cpp`, `os_ansi.cpp`) and Spatterlight need no changes.
- **One concern per commit**, smallest blast radius first, mirroring the a5
  history.

---

## P1. Rewrite the hot string builders on `std::string` (best value/risk)

**Why.** `scprintf.cpp` builds output **quadratically**. Every append does
`buffer = scr_realloc(buffer, strlen(buffer) + …)` followed by `strncat(...)` —
both re-scan the *entire accumulated buffer* on every fragment, and it reallocs
on every append with no capacity growth (`scprintf.cpp:232, 246, 260, 266, 290,
357, 370`). For a long turn's output this is O(n²) in length and hammers the
allocator — a prime suspect for the event-heavy-game OOM/slowness footgun. The
`(decltype(buffer))` casts are the smoking gun that this file was converted but
never modernized.

**Scope.** Contained to `scprintf.cpp` (~30 alloc sites) and any local string
assembly it calls. Internal only — the file's public results can still be
returned as `char*` (allocated once from the finished `std::string`) so callers
are untouched.

**Approach.**
- Replace the grow-by-realloc `char *buffer` accumulators with `std::string`
  (amortized-O(1) append; reserve where the final size is known).
- Keep the `%var%` / tag-scanning logic identical; only the accumulation
  changes. Hand back `char*` at the boundary via a single `strdup`/copy of
  `.c_str()` if the caller frees, or migrate the caller to `std::string` if it's
  local.
- Audit the sibling builders that share the pattern (the untagged-text pass at
  `scprintf.cpp:624`, and any `scr_realloc(... strlen ...)` accumulators in
  `scrunner.cpp` / `scparser.cpp`).

**Validate.** Golden transcripts byte-identical; re-run the Menagerie/circus
soak under `ulimit`/`head` and confirm the RAM/CPU blow-up shrinks; ASan/UBSan
clean.

- [x] Rewrite `scprintf.cpp` accumulators on `std::string`
  (`pf_interpolate_vars`, `pf_replace_alr`, `pf_escape`; `pf_strdup` boundary
  hands a `scr_malloc`'d `char*` back so callers are untouched).
- [x] Sweep other `scr_realloc(buf, strlen(buf)+…)` builders in the hot path —
  scprintf held the **only** per-fragment accumulator loops. The remaining
  reallocs there are the amortized filter buffer (`pf_append_string`, tracks
  `buffer_allocation`) and the memmove-based `pf_filter_input`, neither
  quadratic. Outside scprintf: `scvars.cpp` reallocs resize a reusable scratch
  buffer to one string's length (not accumulating); `scexpr.cpp:1128` is a
  single pairwise string concat (no per-fragment loop, and owned by P2).
- [x] Re-measure: full-corpus replay wall-clock **unchanged** (game strings are
  short, so the O(n²) never bit there), but an isolated accumulator microbench
  shows the old realloc+`strncat` is quadratic (4× length → ~16× time:
  0.7→9.7→145 ms at 2k/8k/32k fragments) while the `std::string` version is
  linear (0.02→0.08→0.30 ms) — ~480× at 32k. The OOM/slowness footgun on
  pathological long single strings is now linear. ASan/UBSan clean on the heavy
  games (light_up, Shadowpeak, ALEXIS, Sun Empire, Secret of Lost World, …).

---

## P2. Stop `abort()`-ing the host application on bad/huge games

**Why.** Every error path bottoms out at `scr_fatal()` → **`abort()`**
(`scutils.cpp:91`). That includes **allocation failure** (`scr_malloc` calls
`scr_fatal` when `malloc` returns NULL) and every malformed-game / internal
consistency check (`prop_put` alone has several `scr_fatal`s, e.g.
`scprops.cpp:461, 531, 547`). There is **no recovery boundary** — the
`setjmp(game->quitter)` in `scrunner.cpp:2066` is only for a normal QUIT, not
errors. In Spatterlight a corrupt or pathological game therefore takes down the
**whole application**. (`scexpr.cpp` already uses `setjmp`/`longjmp` for parse
errors — `scexpr.cpp:646, 1164, …` — so the engine already has a non-local error
idiom; this just generalises and tames it.)

**Approach (C++ exceptions, caught at the boundary).**
- Define a small exception type (e.g. `struct scr_error { std::string what; };`)
  and have `scr_fatal` (and ideally the `scexpr` `longjmp`s) `throw` it instead
  of `abort()`. Keep a `scr_fatal` signature so call sites don't change.
- Wrap the public entry points in `scinterf.cpp` (`scr_interpret_game`,
  `scr_game_from_*`, save/load) in `try { … } catch (const scr_error &)` and
  return a clean failure (the existing "invalid game" path at `scinterf.cpp:455`)
  instead of crashing.
- **Exception-safety caveat:** SCARE leaks on its current abort-and-die model
  (it never unwinds). Throwing past code that owns raw `malloc`'d buffers will
  *leak on the error path* — acceptable for a fatal-but-recoverable game error
  (the session is torn down anyway), but do P3 (RAII) before relying on clean
  unwinding for non-fatal cases. Document which catch sites tear the whole game
  down vs. attempt to continue.
- Convert `scexpr.cpp`'s `longjmp(expr_parse_error)` sites to the same exception
  so there is one error mechanism, and the `setjmp` buffer can go.

**Validate.** Add a regression that feeds a deliberately-corrupt `.taf` (extend
`test/make_badparent_taf.py` or a new generator) and asserts `scr_interpret_game`
returns an error rather than aborting; existing golden transcripts unchanged;
ASan/UBSan clean (watch for leaks newly *reachable* on the throw path — expected,
note them, fix in P3).

- [x] `scr_fatal_error` exception type (`scprotos.h`, guarded `#ifdef
  __cplusplus`); `scr_fatal` formats the message once to stderr and `throw`s it
  instead of `abort()`-ing. Signature unchanged, still `__noreturn__`-compatible.
- [x] `try`/`catch` boundary on every active `scinterf.cpp` public entry
  (`scr_game_from_{filename,stream,callback}`, `scr_interpret_game`,
  `scr_restart_game`, `scr_{save,load}_game`, `scr_undo_game_turn`, and the six
  `scr_{save,load}_game_{to,from}_*` variants) via the `if_report_fatal`
  helper — each returns the existing clean failure (NULL / FALSE / no-op). The
  read-only getters are left unwrapped (low risk; an uncaught fatal there still
  only terminates the process as before, never worse). The normal-quit /
  successful-restore `longjmp(game->quitter)` flows pass through the new
  try-blocks, but those blocks hold no non-trivial-destructor locals, so the
  longjmp stays well-defined.
- [~] **Declined:** migrate `scexpr.cpp` `longjmp` → exception. These are **not**
  the fatal/abort mechanism — `expr_evaluate_expression`'s
  `setjmp(expr_parse_error)` is *local recovery*: on a bad expression it cleans
  up the tokenizer, garbage-collects, returns FALSE, and **the game continues**.
  The same is true of the parser / serial / restr / taf `setjmp`s. Converting
  them to the top-level `scr_fatal_error` would change behavior (tear the game
  down instead of recovering) and skip their local cleanup. They are a correct,
  self-contained idiom and stay as-is; `setjmp.h` remains needed.
- [x] Corrupt-game regression: `test/corrupt_test.cpp` (wired into
  `make -f Makefile.headless test`). Asserts `scr_fatal` throws and carries its
  message; that empty / garbage / bad-header inputs return NULL without
  aborting; and — the real integration check — that a `scr_fatal` raised deep
  inside the `run_create` parse call stack is caught at the boundary and
  reported as NULL (where the host previously `abort()`ed). ASan/UBSan clean
  (throw-path leaks are expected and deferred to P3).

---

## P3. RAII to eliminate leak classes (large, incremental)

**Why.** Manual `malloc`/`free` is pervasive — `scgamest.cpp` (31 sites),
`scprintf.cpp` (30), `scrunner.cpp` / `scdebug.cpp` (18 each), `scparser.cpp`
(17), `scprops.cpp` (16), `sctaffil.cpp` / `scexpr.cpp` (15), `sctafpar.cpp`
(14)… `scr_malloc` is a *non-failing* wrapper, so leaks accumulate silently
until `abort()`. RAII removes whole categories of leak/OOM and is the
prerequisite for clean exception unwinding (P2).

**Approach (hottest/most-leak-prone first, file by file).**
- Replace pervasive `char *` ownership with `std::string`; hand-rolled dynamic
  arrays with `std::vector`; owning pointers freed at scope exit with
  `std::unique_ptr` (or `std::vector<std::unique_ptr<…>>`).
- Mirror the a5 engine's containers — don't invent a bespoke abstraction.
- Keep struct layouts that cross the `extern "C"` boundary POD; confine STL to
  internals.
- Candidate order: `scprintf` (after P1 it's half-done) → `scparser` →
  `scrunner`/`sctasks` → `scgamest`/`scprops` (the property tree, biggest and
  most cross-cutting; coordinate with P4).

**Validate.** Per-file: golden transcripts byte-identical; ASan/UBSan **leak**
counts drop monotonically across the corpus; the event-heavy soak's peak RSS
drops.

- [x] `scprintf.cpp` ownership → `std::string`/RAII (finish P1). **DONE across
  five commits** — every owning accumulator and scratch buffer in scprintf is now
  RAII; the only remaining `scr_malloc`/`scr_free` are the deliberate `pf_strdup`
  char* boundary handed to callers and immediate free-after-use of the internal
  char* return contract (`scr_free(intermediate)`/`scr_free(filtered)`), neither
  a leak-on-throw accumulator. **Self-contained
  scratch buffers done** (commit): `alr_applied` flag array → `std::vector<scr_bool>`
  in `pf_filter_internal` (passes `.data()` to `pf_replace_alrs`); `pf_output_text`
  entity-decode scratch → `std::string`; `pf_output_untagged` tag-parse scratch →
  `std::vector<scr_char>`; `pf_prepend_string` saved copy → `std::string`. These
  were the leak-on-throw-prone locals (a `scr_fatal` raised by a `prop_*` call
  mid-function now unwinds them cleanly). **ALR replacement path also done**
  (second commit): `pf_replace_alr` now hands its result back via a
  `std::string &out` (was a reallocating `scr_char **buffer`), and
  `pf_replace_alrs` replaces the `buffer1`/`buffer2` double-buffer juggling with
  a single `std::string` accumulator (`std::string`'s amortized growth makes the
  two-buffer alloc-reuse trick unnecessary), still returning `char*` via
  `pf_strdup` so `pf_filter_internal` is untouched. **`pf_filter_input` done**
  (third commit): the synonym copy-on-write editor's `scr_malloc`/`scr_realloc` +
  pointer-walked in-place `memmove`/`memcpy` is now a `std::string` with
  offset-based walking and `std::string::replace` for the splice (offsets survive
  reallocation); still takes `const char*` and returns `char*` via `pf_strdup`,
  so callers are untouched. The match branch is genuinely exercised by the corpus
  (secret_of_lost_world 32×, circus 16×, cybercow_win 4×, les_feux 3×,
  melbourne_beach 2×, inverness 1× — all byte-identical). **`scr_filter_t::buffer`
  accumulator done** (fourth commit): the filter's grow-by-`scr_realloc` char*
  accumulator (with `buffer_length`/`buffer_allocation`/`BUFFER_GROW_INCREMENT`
  chunked growth and `strncat`) is now a `std::string`. The struct is fully
  opaque (only a forward-declared `scr_filter_s *` escapes via `scprotos.h`; no
  other TU touches `->buffer`), so it is now `new`/`delete`d (was `scr_malloc` +
  `memset(0xaa)` poison — incompatible with a `std::string` member). `pf_append`
  → `.append`, `pf_get_buffer` → `.c_str()`, `pf_empty`/flush/checkpoint →
  `.clear()`/`.assign()`. `pf_transfer_buffer` now hands the caller a `pf_strdup`
  copy (the one caller, `sctasks.cpp:1417`, already `scr_free`s it — semantics
  unchanged, loses only the zero-copy pointer steal on a single turn's text).
  **`pf_filter_internal` accumulator done** (fifth commit): its `current` char*
  (which leaked on a throw from a `prop_*` deep in `pf_interpolate_vars` /
  `pf_replace_alrs`) is now a `std::string` + `have_current` flag; the
  no-change detection switched from `current == initial` pointer compare to a
  `changed` flag. The public `pf_filter` / `pf_filter_for_info` /
  `pf_interpolate_vars` / `pf_replace_alrs` still return `char*` by contract
  (callers in `scrunner`/`sclibrar`/`scdebug` unchanged); `pf_filter_internal`
  copies each returned `char*` into the `std::string` and frees it immediately,
  and hands its own result back via `pf_strdup`. Both the interpolate and ALR
  paths are corpus-exercised (secret_of_lost_world 115 ALR replacements,
  sun_empire 2 interpolations, alexis 13 — byte-identical).  Validation: `make -f
  Makefile.headless test` green; **byte-identical across the deterministic v4
  corpus** — a determinism-filtered diff (run the baseline binary twice, compare
  the new binary only where the baseline is stable) gives **up to 47 MATCH / 0
  DIFFER / 0–1 NONDET** across the 47 walkthrough scripts (Shadowpeak is
  *time-dependent* in the `os_ansi` player — it can flip to NONDET in the
  baseline binary too, so it can't always be byte-validated this way; never a
  regression). Harness: standalone `os_ansi` player, `SCR_STABLE_RANDOM_ENABLED`.
  ASan/UBSan clean across heavy + ALR/synonym/task-heavy games (light_up,
  X-Files, Screen Savers, Alexis, secret_of_lost_world, circus, space_boy,
  sun_empire). NB: LeakSanitizer
  is unavailable on macOS/arm64, so the "leak ledger" can't be measured here —
  but the change only removes manual `malloc`/`free`, so the leak surface can
  only shrink. **Reusable validation harness:** build a standalone player with
  `c++ -O2 -std=c++17 os_ansi.cpp sc*.cpp -lz -o scares` and replay the
  `adrift-walkthroughs/harness/*_solution.txt` scripts; always compare against a
  *determinism-checked* baseline (the v4 player is not deterministic on every
  game even with a fixed seed).
- [x] `scparser.cpp` — **safe sites done; the rest deliberately left raw.**
  Converted (both standalone / match-phase, off the longjmp path): the
  `uip_replace_pronouns` copy-on-write pronoun editor → `std::string` +
  offset-walk + `.replace()` (heavily corpus-exercised: light_up 582, shadowpeak
  924, secret_of_lost_world 336, alexis 304 …), and the `uip_match_text` `%text%`
  reference buffer → `std::string` (always heap-allocated anyway). Added a
  `uip_strdup` boundary helper. **Left raw on purpose:** (a) everything on the
  parser's `setjmp(uip_parse_error)`/`longjmp` path — the tokenizer
  (`uip_tokenize_start`/`uip_temporary`), the node/word pools and their overflow
  fallbacks (`uip_new_node`/`uip_new_word`), and the parse builders — because the
  two `longjmp` sites (`uip_parse_match`, `uip_parse_element`) would skip
  non-trivial destructors (UB) and leak; (b) `uip_cleanse_string` and
  `uip_compare_prefixed_name`, which use intentional stack-buffer/`malloc`-
  overflow optimizations with immediate frees (no leak risk; RAII would force a
  heap alloc on a hot path or change the caller-buffer return contract). These
  are correct as-is and stay. Validation: same harness, byte-identical corpus,
  ASan/UBSan clean.
- **Prerequisite — DONE (commits `74c9bcdf` + `b07515ee`): the runner quit
  `longjmp` is now a C++ exception, so RAII across `scrunner`/`sctasks`/`scgamest`
  is unblocked.** Background: the runner used `setjmp(game->quitter)` in
  `run_interpret` with `longjmp(game->quitter)` in
  `run_quit`/`run_restart`/`run_restore`/`run_undo`, invoked re-entrantly via the
  scinterf public API and `scdebug`. That longjmp unwinds the *entire*
  `run_main_loop` stack, skipping the destructors of any non-trivial local
  (`std::string`/`std::vector`) live in the command/task/print/expr tree — so RAII
  reachable from `run_main_loop` would have turned today's benign leak-on-longjmp
  into UB. The four sites now `throw` a file-local `run_loop_halt`, caught
  specifically (not `catch (...)`, so a P2 `scr_fatal_error` still propagates) in
  `run_interpret`'s loop; the `do_restart`/`do_restore` flags carry the meaning as
  before, and the vestigial `game->quitter` `jmp_buf` is gone.
  - **Why it was headlessly validatable** (an earlier draft wrongly called this a
    "Spatterlight-side task, unvalidatable headless"): the `os_ansi` player's
    `os_read_line` calls `scr_quit_game` at stdin EOF → `run_quit` → the throw,
    unwinding the live `run_main_loop` — for any walkthrough that ends **while the
    game is still running** (verified firing on `alexis`, `to_hell_and_beyond`;
    walkthroughs ending in win/death set `is_running=FALSE` first, so the EOF quit
    hits `run_quit`'s `!is_running` early-return). Real Spatterlight, conversely,
    **never takes this path**: window-close/host-quit is glkimp `EVTQUIT` →
    `glk_exit()` → `exit(0)` (connect.c:1119, misc.c:63) — the process is killed
    inside `glk_select`, never unwound; the Glk `os_read_line` never calls
    `scr_quit_game`/`restart`/`restore`/`undo`. So the mechanism is dead in the
    shipping app's normal flow (only the headless player + interactive `scdebug`
    reach it). (Aside: `terps/scarier` isn't wired into the Xcode project at all —
    the app builds `terps/scare` — so this could not be "tested in the real app"
    regardless.)
  - **Validation:** new deterministic in-repo fixture `test/quit_test.cpp` (in
    `make test` + `sanitize`) drives the re-entrant `scr_restart_game` *and*
    `scr_quit_game` from inside `os_read_line` and asserts both
    throw→catch→re-loop and throw→catch→stop return cleanly; determinism-checked v4
    corpus byte-identical; `sanitize` ASan/UBSan-clean incl. the throw-unwind.
- [x] `scrunner.cpp` / `sctasks.cpp` RAII — **DONE (two commits).** With no
  `longjmp` over live frames, every self-contained owning `char*`/hand-rolled
  array in these two files now uses RAII. New shared owner
  `scr_owned_string` (`std::unique_ptr<scr_char>` + an `scr_free` deleter,
  added to `scprotos.h` guarded by `#ifdef __cplusplus`) carries the engine's
  char* contract — `.get()` still hands the raw pointer to the existing C call
  sites and to the pointer-aliasing logic that picks the "winning" buffer, so
  callers are unchanged. Converted:
  - **scrunner.cpp** — `run_is_task_function`'s sscanf scratch (`argument`) and
    `run_text_ends_in_newline`'s tag-strip scratch (`stripped`) →
    `std::vector<scr_char>`; `run_game_commands_common`'s match cache
    (`is_matching`) → `std::vector<scr_bool>` (empty vector replaces the old
    NULL sentinel; `task_run_task` in the loop can throw, which leaked the
    array); `run_player_input`'s per-command synonym/pronoun buffers
    (`filtered`/`replaced`) → `scr_owned_string` (`run_all_commands` between
    acquire and the old frees can throw).
  - **sctasks.cpp** — `task_run_change_variable_action` case 2's string-expr
    result → block-scoped `scr_owned_string` (`var_put_string`→`prop_put` can
    throw); `task_run_task_unrestricted`'s `pf_transfer_buffer` result →
    `scr_owned_string` across `task_start_npc_walks`/`task_run_task_actions`
    (both can throw).

  Validated **byte-identical** across the determinism-checked v4 corpus (45
  cross-binary-stable games MATCH / 0 DIFFER); `make -f Makefile.headless test`
  + `sanitize` green; ASan/UBSan-clean on heavy/task-heavy real games
  (light_up, secret_of_lost_world, circus, space_boy, sun_empire).
  - ⚠️ **Validation-harness refinement (important for the remaining P3/P4
    work).** The standalone `os_ansi` corpus player's run-twice determinism
    check is **necessary but not sufficient**: four combat/RNG games — `alexis`,
    `alexis_worn_cube`, `cybercow`, `cybercow_win` — are *stable within a binary*
    yet **diverge across two builds of byte-identical HEAD source** (different
    Mach-O link → different heap layout → a latent uninitialised-read in the v4
    engine returns different garbage, stable per-binary). They therefore can't
    be byte-validated by a cross-binary baseline diff and must be **excluded**,
    the same class of exclusion as the time-dependent Shadowpeak runs. The
    robust baseline is the set of games that agree between **two** identical-source
    builds (`scares_base` vs a fresh HEAD rebuild) — 45 of the 49 mapped
    walkthroughs. (Earlier phases that reported alexis/cybercow as "MATCH" were
    comparing within a single baseline link and got lucky on layout.)
- [~] `scgamest.cpp` (done) / `scprops.cpp` (with P4, still open) — unblocked now. Much of the
  runner-adjacent ownership here is **game-struct fields** (`current_room_name`,
  `status_line`, `hint_text`, the property tree) freed during the turn loop; the
  old `longjmp` hazard over those is gone, so RAII on them is now safe to pursue.
  - **DONE — the five owning game-status strings** (`current_room_name`,
    `status_line`, `title`, `author`, `hint_text`) → `scr_owned_string` (the
    `unique_ptr<scr_char>`+`scr_free` owner from the scrunner/sctasks work). These
    are the leak-on-throw class the item names: the turn loop replaced them with
    `scr_free(game->X); game->X = filtered;` where `filtered` came from a
    `pf_filter` that can `throw` (a `prop_*` `scr_fatal`) mid-acquire. `.reset()`
    now carries the free; readers take `.get()` (NULL stays NULL, so no
    empty-vs-unset ambiguity — important because `run_get_attributes` lazily
    computes `title`/`author` keyed on `if (!game->title)`). Adding non-trivial
    members makes `scr_game_s` non-POD, so `gs_create` now `new scr_game_s ()`s it
    (value-init zeroes the POD fields it already set explicitly) and `gs_destroy`
    `delete`s it — the old `scr_malloc` + `memset(game, 0xaa)` poison is gone
    (poison would corrupt the `unique_ptr`s the destructor frees, exactly the
    `scr_filter_s` precedent). `gs_string_copy` (used by `gs_copy` for undo/
    restore deep-copies) now takes `scr_owned_string &`. Sites: `scgamest.h`
    (5 field types), `scgamest.cpp` (create/copy/destroy), `scrunner.cpp`
    (`run_update_status` ×2, `run_quit`-time reset, `run_get_attributes`
    title/author/room/status, `run_get_game_hint`). Validated **byte-identical**
    across the determinism-checked v4 corpus (**49 MATCH / 0 DIFFER**; the 3
    NONDET are the time-dependent Shadowpeak variants whose two identical-source
    baselines already disagree — never a regression); `make -f Makefile.headless
    test` + `sanitize` green; ASan/UBSan clean.
  - **DONE — the `scgamest.cpp` POD state arrays** (`rooms`, `objects`, `tasks`,
    `events`, `npcs` and each NPC's `walksteps`, and the `object_references`/
    `multiple_references`/`npc_references` flags) → `std::vector`. These were the
    last hand-`scr_malloc`'d owners in the game struct, allocated one-by-one in
    `gs_create`: a `prop_*` `scr_fatal` thrown partway through construction (the
    object-placement and walks loops make many `prop_get_*` calls) leaked every
    array already allocated before it — exactly the P3 leak-on-throw class. Now
    the vectors free themselves when the (already non-POD, `new`/`delete`d) game
    is destroyed, so a throw unwinds them cleanly. The `*_count` fields stay as
    the iteration source of truth (every loop, the serializer, and the asserts
    are unchanged); `.resize()`/`.assign()` size the vectors to match. Notes:
    `gs_create`'s per-element init loops still set every field, so vector
    value-init (zero) vs. the old uninitialised `scr_malloc` is behaviourally
    inert on the corpus; the `gs_copy` undo/restore `memcpy`s became vector
    copy-assignment (POD element copy; the NPC `walksteps` inner vector deep-copies
    too); `gs_destroy`'s manual `scr_free` block is gone; `scr_bool` is `int`
    (not `bool`), so `std::vector<scr_bool>` stays contiguous and `.data()` is
    valid for the two `sclibrar.cpp` reference-buffer `memcpy`s. Boundary fixes
    where callers took a raw array pointer: `scdebug.cpp` (`from->objects + i` →
    `&from->objects[i]`, NPC walkstep compare → `==` on the vectors),
    `scrunner.cpp` (`game->tasks + task` → `&game->tasks[task]`; `hint -
    game->tasks` → `hint - game->tasks.data()`). Validated **byte-identical**
    across the determinism-checked v4 corpus (**51 MATCH / 0 DIFFER**; the 1
    NONDET is a layout-sensitive Shadowpeak variant excluded as before — never a
    regression); `make -f Makefile.headless test` + `sanitize` green; ASan/UBSan
    clean on the heavy games (light_up, secret_of_lost_world, circus, sun_empire,
    space_boy).
  - **STILL OPEN — `scprops.cpp` property tree** (`node_pools`, dictionary,
    orphans). The property bundle uses a deliberate **arena/pool allocator** that
    P4 profiling confirmed is the hot path — converting it to `std::vector`/RAII
    risks both the byte-exact bar and the arena's performance, so it stays its own
    coordinated P4-adjacent project, not a mechanical sweep.
- [x] `sctaffil.cpp` — **the load-time decompression/unobfuscation scratch
  buffers** (same P2 follow-up class as `sctafpar` below). The two file readers
  each `scr_malloc` a fixed-size working buffer and then call `taf_append_buffer`
  (which `scr_realloc`s → can `scr_fatal`/throw on OOM) and finally `scr_fatal`
  on an unterminated final slab — so a corrupt/truncated game leaked the raw
  buffer(s) on the throw path. Converted: `taf_unobfuscate`'s `buffer` and
  `taf_decompress`'s `in_buffer`/`out_buffer` → fixed-size `std::vector<scr_byte>`
  owners, handing `.data()` (a `scr_byte *const`) to the unchanged
  pointer-walked body (callback fill, `taf_random` XOR, `memmove` compaction, the
  zlib `z_stream` next_in/next_out). All five `scr_free` calls (success + the
  three early `return FALSE` error paths) are gone; the vectors free themselves on
  every exit including the throw. The zlib `inflateEnd` ordering is unchanged
  (still skipped on the mid-stream `inflate` error early-return, as before — not
  my buffers' concern). Validated **byte-identical** across the
  determinism-checked v4 corpus (**37 MATCH / 0 DIFFER**, both the v4.0
  zlib-decompress and the v3.9/3.8 unobfuscate paths exercised; the 1 NONDET is
  the time-dependent `shadowpeak`, nondeterministic within the baseline binary
  too — never a regression); `make -f Makefile.headless test` + `sanitize` green;
  ASan/UBSan clean. (Byte-identity is structural: the `std::vector` is the same
  fixed size, `.data()` is the same kind of contiguous buffer used identically.)
- [x] `sctafpar.cpp` — **the load-time leak-on-throw scratch arrays** (P2
  follow-up: P2 noted that a `scr_fatal` thrown through the TAF parser leaks raw
  buffers, so RAII here makes the corrupt-game error path unwind cleanly).
  Converted the three plain local `scr_int` scratch arrays that are
  indexed-only, not aliased by any raw pointer, and bracket throwable `prop_*`
  calls between `scr_malloc` and `scr_free` → `std::vector<scr_int>`:
  `parse_fixup_v380`'s `object_type` (the v3.8→v4.0 container/surface fixup),
  `parse_add_movetimes`'s `movetimes` (the `memset(0)` becomes vector
  value-init), and `parse_add_alrs_index`'s `alr_lengths`. These are TAF
  load-time only (not the per-turn hot path) and free themselves if a
  `prop_get_*`/`prop_put` in the loops throws. **Left raw on purpose:**
  `restrmask` (×2) is an ownership *transfer* to the bundle via `prop_adopt`,
  not a local free; `multiline` is a returned `char*` accumulator (separate
  contract); `parse_resources`/`clean_name` are a file-static growable table
  (broader, low throw-risk). Validated **byte-identical** across the
  determinism-checked v4 corpus (**51 MATCH / 0 DIFFER**; the 1 NONDET is the
  layout-sensitive `shadowpeak_allgargoyles` whose two HEAD baselines already
  disagree — never a regression; the corpus exercises all three paths: 3.8/3.9
  fixups, `secret_of_lost_world` ALRs, walk-bearing games); `make -f
  Makefile.headless test` + `sanitize` green; ASan/UBSan clean. (Byte-identity
  is structural: every element is written before it is read, so vector
  value-init vs. the old uninitialised/`memset`'d `scr_malloc` is inert.)
- [ ] Track corpus leak count before/after each file. (Blocked on tooling:
  LeakSanitizer is unavailable on macOS/arm64, so the ledger can't be produced in
  this environment; the proxy used so far is "changes only remove manual
  malloc/free, so the leak surface can only shrink", plus ASan/UBSan-clean runs.)

---

## P4. The property-tree hot path (highest ceiling, highest risk)

**Why.** `prop_get`/`prop_put` walk a string-keyed tree on every access
(`scprops.cpp:451+`, vararg key array). This is the single most-called operation
in the engine — every restriction, action, variable read, and text substitution
funnels through it.

> ⚠️ **Premise correction (from profiling, see below).** An earlier draft of
> this section claimed `prop_*` *"parse[s] a printf-style format string"* with
> *`"Objects[%d].Name"`*-style paths *"assembled and re-parsed at runtime"*.
> That is **not** how the code works. The format strings are compile-time
> literals (`"I<-sis"`, `"S<-sisis"`, …) whose characters after index 3 directly
> encode key types ('s' = string, 'i' = integer); the "parse" is a trivial
> char-by-char read (`format[index_ + 3]`) — **no `sscanf`/`sprintf`, no runtime
> path assembly**. So the TODO's first proposed remedy below (*"cache parsed
> format paths"*) would save essentially nothing and is **withdrawn**. The real
> per-access cost is the **string-keyed child scan** (`prop_find_child`'s
> `strcmp` loop) and sheer **call volume**.

**Approach (only after P1–P3; this is the risky one).**
- **Low-risk first step:** cache the parsed format path (the `%s`/`%d` template →
  a pre-split step list) so repeated accesses skip the `sscanf`/format re-parse.
- **Bigger step:** replace string-keyed child lookup with `std::unordered_map`
  per node, or precompute integer field indices, or move toward typed accessors
  like the a5 model (typed structs + key lookup instead of formatted paths).
- This changes the engine's core data structure — budget heavily for byte-exact
  validation and treat it as its own multi-commit project. The a5 engine
  (typed records, no formatted-path access) is the reference design.

**Validate.** Whole walkthrough corpus byte-identical; profile before/after on
the event-heavy games to confirm the win justifies the risk.

- [x] **Profile to confirm `prop_*` is the hot path (don't assume).** Done, two
  ways. (1) **Call volume** via opt-in counters (`-DSCARE_PROFILE_PROPS`, since
  reverted) over the corpus — Shadowpeak's 849-command walkthrough:
  **9.98 M `prop_get`**, 56.7 K `prop_put`, **36.4 M `prop_find_child`** (65 %
  string-key scans), **48.3 M `strcmp`**. (2) **Self-time** via macOS `sample` on
  a standalone `os_ansi` replay (build: `c++ -O2 -g -std=c++17 -I. -I../cheapglk
  sc*.cpp os_ansi.cpp ../common_utils/randomness.c -lz`). `prop_find_child` is the
  largest engine leaf, with `prop_get` + its `strcmp`/`memmove` share close behind;
  the parser (`uip_match_node`/`scr_compare_word`) is a clear second. **`prop_*`
  is confirmed the hot path.** Two important nuances the profile surfaced:
  - **The move-to-front heuristic already works:** avg **~2.0 `strcmp` per
    string-key lookup** (child lists are short and recency-sorted), so the scans
    are *not* pathologically deep. But the heuristic fires a `memmove` on ~31 % of
    string lookups (7.2 M shuffles on Shadowpeak) — possibly net-negative on such
    short lists vs. a single swap-toward-front. Candidate micro-opt, low ceiling.
  - **The biggest *avoidable* steady-state cost was not `prop_*` at all** — it was
    `setjmp` saving the signal mask (`sigprocmask`+`sigaltstack`), see the
    realized win below. Fixed.
- [~] ~~Cache parsed format paths~~ — **withdrawn** (premise was wrong; format
  walk is already trivial char reads, see premise correction above).
- [ ] (Optional) typed/`unordered_map` node lookup, or move-to-front → single
  swap. Lower expected payoff than first assumed (scans are already ~2 deep);
  weigh against the byte-exact-validation cost before attempting.

### Realized win (from P4 profiling): skip the `setjmp` signal-mask save

The profile's single largest steady-state cost was the kernel **signal-mask
handling inside `setjmp`** (`sigprocmask` + `sigaltstack` syscalls on macOS/BSD,
**~114 sample units vs. 87 for `prop_find_child`**). The expression evaluator
(`scexpr`), command parser (`scparser`), and restriction evaluator (`screstrs`)
each arm a `setjmp` recovery point on essentially every turn; SCARE never alters
the signal mask between `setjmp` and `longjmp`, so the save/restore is pure waste.

- [x] Added `scr_setjmp`/`scr_longjmp` macros (`scprotos.h`) mapping to POSIX
  `_setjmp`/`_longjmp` (identical control flow, **no** mask save), with the
  profiling rationale documented at the macro. Converted **all 34** engine
  set/longjmp sites (`scexpr`, `scparser`, `screstrs`, `scrunner`, `sctafpar`,
  `scserial`) — every pair is file-local, so the swap is self-contained and does
  **not** alter the `longjmp`-over-live-frames hazard discussed in P3 (it is still
  a `longjmp`, just faster). Validated **byte-identical** across the
  determinism-checked v4 corpus (17 stable games, 0 differ), `make -f
  Makefile.headless test` green, ASan/UBSan clean (light_up, alexis,
  secret_of_lost_world, circus). Measured **~5–13 % whole-run speedup**
  (light_up 9.7 %, sun_empire 13.0 %, secret 4.3 %, best-of-N over 30 reps);
  steady-state turn-loop gain is larger since these numbers include the constant
  per-process TAF-load overhead, which a long Spatterlight session amortizes away.

---

## 5. Testing & tooling (shared across phases)

- **Golden transcripts**: ensure each touched subsystem has a replayable
  before/after transcript (extend the existing `.taf` harnesses where a feature
  is uncovered).
- [x] **ASan/UBSan harness** — `make -f Makefile.headless sanitize` builds every
  self-contained, no-external-data harness that `test` runs (battle, precedence,
  badparent, corrupt; a5 parse/arith/disambig/walk/objects/save) with
  `-fsanitize=address,undefined -fno-sanitize-recover=all` into `*_san` binaries
  and runs each, aborting on any finding (verified the gate traps real UBSan,
  exit 134). LeakSanitizer is left off (`detect_leaks=0`): unavailable on
  macOS/arm64, and throw-path leaks are expected per P2/P3. The game-data replays
  (`a5runtest`, the v4 walkthrough corpus) are excluded because their game files
  are uncommittable — run those under sanitizers by hand with
  `make … a5runtest CXXFLAGS="$(CXXFLAGS) $(SAN_FLAGS)"` or the standalone
  `os_ansi` corpus player (see the P3 validation-harness note). All harnesses
  currently ASan/UBSan-clean.
- **Memory soak**: a bounded (`ulimit -t` / `head -c`) Menagerie/circus run that
  records peak RSS + wall-clock, so P1/P3 wins are measured, not assumed.
- **Leak ledger**: record the corpus LeakSanitizer count per commit so P3
  progress is visible.

---

## 6. Recommended order & rationale

1. **P1** — contained, high-confidence, directly attacks the OOM/perf footgun.
2. **P2** — robustness; stops a bad game crashing the host. Small surface.
3. **P3** — incremental cleanup; unlocks clean unwinding for P2 and shrinks RSS.
4. **P4** — biggest ceiling, biggest risk; only with profiling justification.

Each phase is independently shippable and independently revertible. Stop after
any phase if the upstream-divergence cost outweighs the benefit.
