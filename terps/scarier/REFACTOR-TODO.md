# Scarier: refactoring / clean-up / de-duplication TODO

**Nothing here is open.**  The survey of `terps/scarier` was taken
2026-10-10 (master @ 1b41f85be) and worked through by 2026-10-11 (master
096834efb); every item was done or declined.  This file keeps what a
later pass needs: where things landed, what was declined and why (so it
is not re-proposed), what was left on purpose, and the safety net.

The full log, with the census, the per-step reasoning and the
verification of each item, is in git:

    git show cd0a32c0c:terps/scarier/REFACTOR-TODO.md

Section numbers below are the survey's, kept so that commit messages and
the old log still resolve.

## What the survey found

Exact copy-paste was low (~136 duplicated 8-line windows in ~155k lines,
no static function defined in two files).  What there was is structural:
families of near-identical functions differing in a verb word, a
resolver callback or an ADRIFT version gate.  Those are Runner-parity
behaviour pinned by the transcripts, which is why most of section 1 is
in the declined list rather than the done one.

## Done, and where it landed

| item | result | commit |
|---|---|---|
| 1.4 Glk event loops | `gsc_service_window_event()`; both mains split into init / loop / shutdown | 18e8b35d5, 209c1092f, 1d8a133e3 |
| 1.5 a5 recursion guards | `A5_PLACEMENT_DEPTH_MAX` | 6a41cb252 |
| 1.7 map fallback chain | `sm_try_sidestep()` | d0e8ef874 |
| 1.8 TAF schema strings | descriptors identical across versions are named constants | d1cda3926 |
| 2.2 include boilerplate | `library/sclibrar.h`, `adrift5/run/a5run_internal.h` umbrellas | 8f9a7ae69 |
| 2.3 unsafe string ops, raw buffers | no `sprintf` / `strcpy` / `strcat` left; function-local buffers owned | 718cc3c1d, 7c0950702, 6fb39dcb7 |
| 2.4 `(scr_int)` / `(scr_bool)` casts | six no-ops removed; the other 147 are real conversions | 538c1ef5f |
| 3 big functions | all 31 functions over 200 lines split into stages or a context struct with members | one commit each, 2026-10-11 |
| 4 Win32 open-by-path | `gsc_open_game_path` in `glk/os_glk_platform.cpp` | 17638c398 |
| 5 source lists | both makefiles glob; only Xcode needs a hand edit | 42e2cb5ac |
| 5 tree layout | `core/`, `platform/`, `tools/`, `headless/`; dump prints through `scr_dump_log` | 42c8da273, 0f2d5c2b2, 42df5fda4, 6e4d24f50, 7bce83428 |
| 5 `scprotos.h` | 25 module headers under `core/` plus a 56-line umbrella | e462bfbc6, 58bba8c05 |
| 6 stale comments | every pointer names the owning file | 31b77eaac |
| 6a accessor guards, loops | `GS_CHECK*`, `GS_FOR_EACH_*` | f4c069b2e, e3188ec5c |
| 6a module state | `uip`, `pf`, `expr`, `restr`, `ser`, `tasks`, `palette`, `disambig` state structs | 5edd9064b, 5d883d191, ede2fd91e, ae6ebb6e6, a9f9f36e5, 71339c36f, 60e45292c, 2c080c546 |
| 6a error handling | index half of the `scgamest.cpp` guards is `scr_fatal`, not `assert` | 5d591cf70 |
| 6a `NULL` → `nullptr` | code only; comments and strings keep the word | 0a36d0eca |
| 6a `memset`, `(void) x` | six struct `memset`s are `= {}`; 21 unused parameters unnamed | dd87c0bf2 |
| 6a long lines, macros | six function-like macros are functions; 32 of 34 lines over 100 columns wrapped | 70874b1c1 |
| 6a locale / ctype | `loc_table_lookup<T>`; shared low halves of the code-page tables; one `NUL` / `WHITESPACE` in `core/sctypes.h` | 2d53d5857, 6258795aa, 46e668b71, c84f47c6e |
| 6a file splits | `scparser` 5 TUs, `scprintf` 5, `sctasks` 5, `sclibrar_disambig` 3, `sclibrar_take` 2, `sclibrar_put` 2, `scrun_match` 3; each with its own `*_internal.h` | 82f5fb5e2, 0f18acb3f, 26365ef84, 8811b5eee, 55fd2004b, b3fb149dc, 096834efb |

`gs_is_game_valid` was 5% of self time in the 2026-10-04 profile; the
`GS_CHECK` fold (f4c069b2e) kept its test to the same compare and branch
and compiles the game half out under `NDEBUG`.

New files go into the Xcode project by hand; the next free id in the
`5C1B1A…` series is 0118.

## Declined (do not re-propose without new evidence)

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
  and wide, and the determinism harness depends on when flags are read.
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

## Left on purpose

These are candidates, not tasks; each wants its own decision.

- **ADRIFT 5 `char *` returns.**  `adrift5/` has ~30 `malloc` and ~360
  `free`: text functions return a `malloc`ed string the caller frees.
  Wrapping that is an API change across a5text / a5run / the Glk front
  end / every harness, and wants a leak checker that runs here
  (LeakSanitizer does not on macOS/arm64; `leaks --atExit` might).
- **The ~45 remaining raw `scr_malloc` sites in the 4.x engine** are
  struct owners with a destroy function, strings handed to
  `prop_adopt()`, pool entries, or stack-buffer-with-heap-fallback.
- **The rest of the file-scope statics.**  The state-struct pass covered
  the modules the survey named.  The other library files have their own
  (`lib_battle_who_reset()`, `lib_with_prefix_390_reset()`,
  `lib_put_reset()` show three more reset hooks; the eight take / put
  flags are in their internal headers), and runner/ has its line
  buffers.  A census of those is the next step if two games in one
  process is ever a goal.
- **`uip.options` passed by value** into `uip_match()` instead of set
  through the `uip_set_*()` pairs: the runner's call sites are the
  obstacle.
- **Section 4's table** (RNG, map, save, text, input existing once per
  engine): candidates for sharing, each needing a decision on whether
  the semantics are the same.
- **Startup linkage** (`glkunix_startup_code`, `WinMain`,
  `winglk_startup_code`, ~280 lines at the end of `os_glk.cpp`) could
  follow the open-by-path helper into `glk/os_glk_platform.cpp` if
  `os_glk.cpp` (1826 lines) is split further.
- **File sizes.**  The largest are `mapdraw.cpp` (2726) and
  `library/sclibrar_drop.cpp` (2715), with a dozen more between 2000
  and 2530; none stands out, so no further split is proposed.
- **Glk Makefile dependencies**: `$(OBJECTS): $(HEADERS)` rebuilds
  everything on any header edit; `-MMD` would be a small separate
  change.
- **Fatal message in the game window**: `if_report_fatal()` writes to
  stderr, so in Spatterlight a game stops without saying why.  A
  front-end change, for both engines.
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
6. `test/adrift4/harness/run_autosave_tests.py --build` after touching
   serialise or the Glk main loops.
7. For a file split: the sorted line multiset of the new files against
   the old one, and a `-Wall -Wextra -Wshadow` syntax check (a copied
   `type name = …;` can shadow a struct member).
8. One commit per mechanical step, behaviour-neutral, so a bisect lands
   on a single rename or extraction.
