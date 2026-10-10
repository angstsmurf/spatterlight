# Scarier: refactoring / clean-up / de-duplication TODO

Survey of `terps/scarier` taken 2026-10-10 (master @ 1b41f85be). **No code was
changed to produce this list.** All counts are from grep / a throw-away
8-line-window duplicate detector over tracked `*.cpp *.h *.mm *.c` outside
`test/`; line numbers will drift, so re-grep the quoted identifiers.

Items the audit declined (1.1, 1.2, 1.3, 1.6, 2.1, the walk-control helper,
the schema delta macros, `scarier.hdr`) have been removed from the body; the
"Declined" list in the Outcome section keeps their one-line reason so they
are not re-proposed.  Section numbers are kept so the Outcome references
still resolve.

## Reality check first

* Exact copy-paste is **low**: only ~136 duplicated 8-line windows in ~155k
  lines, and no static function name is defined in two files. The earlier
  TU split (`library/` = 21 TUs, `runner/`, `glk/`, `adrift5/{run,text}`) already
  removed the worst of it.
* What is left is **structural** duplication: families of near-identical
  functions that differ only in a verb word, a resolver callback or an ADRIFT
  version gate. Those were audited and declined (see Outcome), because the
  version-gated text is Runner-parity behaviour pinned by the transcripts.
* Therefore every item below carries a **safety net** requirement (bottom of
  this file).

Legend: **[H]** high value, **[M]** medium, **[L]** low / mechanical.
Risk: *low* = behaviour cannot change, *med* = covered by suite, *high* =
Runner-parity text/ordering, needs transcript diff.

---

## 1. Structural duplication

### 1.4 [M] Glk event loops duplicated between the v4 and v5 mains — risk: med
`glk/os_glk.cpp:~532` (`gsc_main`, 231 lines) and `glk/os_glk_a5.cpp:~225`
(`gsc_a5_main`, 352 lines) each hand-roll the same `glk_select` switch
(`evtype_Arrange/Redraw` → `gsc_refresh_windows`, `evtype_SoundNotify` →
`gsc_a5_sound_finished`, `evtype_MouseInput` map click …). Two more selects at
`os_glk.cpp:~1040` and `os_glk_resources.cpp:~328`.

- [x] Extract `gsc_dispatch_common_event (event)` returning "handled?" and have
      both mains call it; keep engine-specific cases (line/char input) local.
      → `gsc_service_window_event()`, 18e8b35d5; the loops stay separate.
- [x] Then split both mains: init, event loop, shutdown.
      → 209c1092f (a5), 1d8a133e3 (v4); see "Main split" below.

### 1.5 [M] ADRIFT 5 recursion guards — risk: low
- [x] The `depth > 32` style recursion guards (6 hits in `adrift5/`) want one
      named constant.  → `A5_PLACEMENT_DEPTH_MAX`, 6a41cb252.

### 1.7 [M] Map layout fallback chain — risk: low
`scmap.cpp` `sm_set_grid` (228 lines) repeated the
`GRID_EMPTY → N → S → exit-1 N → exit-1 S → FAIL_STUCK` ladder four times.

- [x] Extract `sm_shift_perpendicular (L, r, rno, x, y)`; then break the
      function by direction.  → `sm_try_sidestep()`, d0e8ef874.

### 1.8 [M] TAF schema descriptor strings — risk: med
- [x] Name every record whose descriptor is identical across versions
      (d1cda3926).  The per-version records stay verbatim by design, see
      Outcome.

### 1.9 [L] Parse/dispatch tails that repeat in `a5parse.cpp` and `a5state.cpp`
- [x] Read; declined, see "Small mechanical items" below.

---

## 2. Cross-cutting patterns

### 2.2 [M] Include / license boilerplate — risk: none
- [x] ~16 `library/*.cpp` opened with the same 20-line include block; the
      system includes live in `library/sclibrar.h` (8f9a7ae69).
- [x] `adrift5/run/*.cpp` repeated the same ~10 `../a5*.h` includes;
      `a5run_internal.h` is the umbrella (8f9a7ae69).
- The 18-line GPL header is pasted in every file (fine legally; just don't
  count it in duplication metrics, and make sure new files copy it from one
  template).

### 2.3 [M] Memory management and unsafe string ops — risk: med
~448 `malloc/free/sc_malloc/sc_free` mentions vs ~144 `new`; 18
`sprintf/strcpy/strcat`. Throw-path leaks are *documented* in
`Makefile.headless` as expected, which is a smell, not a design.

- [x] Function-local raw buffers in the 4.x engine: done for what was left
      (c.f. "2.3" below).  ADRIFT 5's `char *` returns are NOT taken.
- [x] Replace the 18 unsafe string calls with `snprintf`/`std::string`.
      → 718cc3c1d, 7c0950702; the tree has none left.
- [x] Re-run the `sanitize` target after each batch.  (It runs with
      `detect_leaks=0` — LeakSanitizer does not exist on macOS/arm64 — so
      it checks overruns and use-after-free, not the throw-path leaks.)

### 2.4 [L] ~160 explicit `(scr_int)` / `(scr_bool)` casts —
audit; many are probably redundant after the typedefs; do last, with `-Wconversion`.

---

## 3. Big functions to split (heuristic: > 200 lines; 33 of them, 119 > 100)

Splitting them is mostly mechanical but each needs its own equivalence run.

| lines | function | file |
|---|---|---|
| ~~520~~ | ~~`run_task`~~ | DONE: `run_task_ctx` + four phases, see "run_task split" |
| ~~500~~ | ~~`pass_character`~~ | DONE: `char_restr_t` + operator table, see "pass_character split" |
| ~~385~~ | ~~`lib_cmd_verb_object`~~ | DONE: eight stages + driver, see "lib_cmd_verb_object split" |
| ~~371~~ | ~~`uip_replace_pronouns`~~ | DONE: `uip_pronoun_t` + six stage members, see "uip_replace_pronouns split" |
| ~~353~~ | ~~`run_main_loop`~~ | DONE: six stages + driver, see "run_main_loop split" |
| ~~345~~ | ~~`eval_function`~~ | DONE: built-in table + four lookups, see "eval_function split" |
| ~~336~~ | ~~`scdump_tasks`~~ | DONE: `scdump_task_t` + eight section members, see "scdump_tasks split" |
| 323 | `oo_prop` | `adrift5/a5expr.cpp` ~626 |
| 309 | `lib_print_room_description` | `library/sclibrar_room.cpp` ~382 |
| 308 | `gsc_a5_display` | `glk/os_glk_a5_display.cpp` ~200 |
| 306 | `debug_help` | `scdebug.cpp` ~240 (data, not logic: make it a table) |
| 301 | `view_location_impl` | `adrift5/text/a5text_view.cpp` ~141 |
| 289 | `lib_take_multiple_common` | `library/sclibrar_take.cpp` ~3554 |
| 274 | `npc_tick_npc_walk` | `scnpcs.cpp` ~944 |
| 267 | `task_run_task_unrestricted` | `sctasks_run.cpp` |
| 260 | `uip_match_entity` | `scparser_entity.cpp` |

Ideas that recur: `eval_function`, `oo_prop`, `debug_help`, `gsc_a5_display`
are giant `if/else` or `switch` on a string/enum → table of
`{name, handler}`; `pass_character` / `run_task` are phase-sequential →
named phase functions sharing a context struct.

---

## 4. Parallel v4 / v5 implementations (investigate before touching)

Two engines live side by side; some concepts exist twice. These are *candidates
for sharing*, not defects — each needs a decision on whether the semantics are
genuinely the same.

| concept | v4 | v5 | shared today |
|---|---|---|---|
| RNG | `scutils.cpp` (congruential + erkyrath) | `adrift5/a5rand.cpp` | `common_utils/randomness.c` |
| map | `scmap.cpp` | `adrift5/a5map.cpp`, `glk/os_glk_map.cpp` | `mapdraw.cpp` |
| save / serialise | `scserial.cpp` (2010 l.) | `adrift5/run/a5run_save.cpp`, `a5state.cpp` | – |
| text / markers | `scprintf*.cpp` | `adrift5/text/*` | – |
| input / word tables | `scparser*.cpp` + `runner/` | `adrift5/run/a5run_words.cpp` | – |

- [x] `scutils.cpp` has **two** `scr_congruential_rand` definitions selected by
      `#ifdef SPATTERLIGHT`.  → both branches are live (SPATTERLIGHT vs
      headless); the comment says so since 31b77eaac.
- [ ] `glk/os_glk_a5.cpp` defines `gsc_a5_open_game_path` twice
      (`_WIN32 && !GARGLK` vs not, ~l.992–1029): fine, but isolate the Win32
      half in a platform file.

---

## 5. Tree layout, build files, dev tooling

- [x] **Source lists live in three places**: hand-maintained `OBJECTS` in
      `Makefile` (220 l.), wildcards in `Makefile.headless` (709 l.), and the
      Xcode project. Adding a file touches all three. Put the list in one
      `sources.mk` included by both makefiles.  → both makefiles use
      wildcards (42e2cb5ac); only the Xcode project still needs a hand edit.
- [ ] `scdump.cpp` (1903 l., `SCARIER_DUMP_TOOLS`, 74 `fprintf(stderr…)`) and the
      census/respell/tables parts of `runner/` are developer tooling that
      share the shipping directory. Move under `tools/` (or `dev/`) so "what is
      the interpreter" is obvious; route the 74 prints through one log sink.
- [ ] `scdebug.cpp` (2612 l.): confirm it is needed in the shipping build
      (`SCR_DEBUGGER_ENABLED`); if only developers use it, same treatment.
- [ ] Top level still has 20+ `sc*.cpp` (4.x core) mixed with `os_ansi.cpp`,
      `mapdraw.cpp`, `scarier-autosave.mm`. Consider `core/` (engine),
      `platform/` (os_ansi, sx*), mirroring the `library/ runner/ glk/` split.
- [ ] `sxstubs.cpp / sxglob.cpp / sxutils.cpp` and `sxprotos.h` — headless
      only; give them an obvious home (`headless/`).
- [ ] Header weight: `scprotos.h` (1511 l.) is one catch-all prototype header.
      Split by module (`sctasks.h`, `scparser.h`, …) so TUs include only what
      they use and rebuilds shrink.

---

## 6. Stale comments and references — all done in 31b77eaac

- [x] ~30 comments in 26 files said "see/split out of `sclibrar.cpp`"; every
      pointer names the owning topic file.
- [x] 32 comments cited `RUNNER_TESTS_TODO.md section N` and 11
      `WINE-TRANSCRIPTS-TODO.md`; they spell the full `test/adrift4/notes/`
      path.
- [x] `runner/scrunner.cpp` "TODO Rationalize what happens here"
      (run_restore_handler) is a note of what is true now.
- [x] `adrift5/run/a5run_save.cpp` pointer at a pruned TODO file: the comment
      says the writers ARE the mapping; the pointer is history only.
- [x] `Makefile.headless` "TODO ground rule" comment reworded.
- [x] Headers of `library/*.cpp` carry a one-line responsibility statement.

---

## 6a. Second-pass findings

### Accessor-prologue and boilerplate lines (risk: none, high line count)
Most-repeated non-comment lines in the tree:

| times | line |
|---|---|
| 251 | `const scr_prop_setref_t bundle = gs_get_bundle (game);` |
| 197 | `const scr_filterref_t filter = gs_get_filter (game);` |
| 87 | `run_get_dispatch_input ()` (59 as `const scr_char *input =`, 28 as `line =`; 87 call sites in `library/`) |
| 70 + 26 | `for (object = 0; object < gs_object_count (game); object++)` (and the `index_` spelling; ~237 `gs_object_count` loops in all) |
| 59 | `const scr_var_setref_t vars = gs_get_vars (game);` |
| 27 | `assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));` |

- [ ] **Context struct for library handlers**: a small `lib_ctx_t { game, bundle,
      filter, vars, input }` built once per command in `lib_dispatch` and passed
      (or reachable from `game`) removes ~500 lines of prologue and the
      per-handler re-fetch.
- [ ] **Iteration helpers**: `for (auto object : gs_objects (game))` or a
      `GS_FOR_EACH_OBJECT (game, object)` macro (same for npc/room/task/event).
      Variable naming for the loop index currently drifts between `object`,
      `index_`, `obj`, `o`, `i`, `n`.
- [ ] **`run_get_dispatch_input`**: handlers re-tokenise or `strstr` the raw
      line 87 times (cf. `lib_input_contains_word`). Parse once into a word
      list on the dispatch context and give handlers `ctx.has_word ("with")`.
- [ ] `scgamest.cpp`: 108 `assert (gs_is_game_valid (...))` and 77
      `gs_in_range` asserts are the same two-line accessor guard; a single
      `GS_CHECK_OBJECT (gs, object)` macro halves each accessor. Careful: the
      profiling note says `gs_is_game_valid` is a hotspot — fold the guard and
      make it `#ifndef NDEBUG`-cheap in the same change.

### Module-global state that should hang off the game or a context (risk: med)
175 mutable file-scope statics across the tree; the biggest block was
`scparser.cpp` with 29 (now `scr_uip_state_t`), then `mapdraw.cpp` 13,
`screstrs.cpp` 9, `sclibrar_disambig*.cpp` 9 (four of them now shared through
`sclibrar_disambig_internal.h`), `scserial.cpp` 8, `scexpr.cpp` 7,
and the eight printfilter cache globals (now `scr_pf_state_t`).

- [x] Group the `uip_*` globals into `struct uip_state_t` (parser mode flags,
      pronoun memory, pools, lookahead) held in one file-static instance with
      `uip_state_reset()`. That makes the autosave / re-entry story explicit
      (which of these are *saved*, which are per-parse scratch) and is the
      precondition for ever running two games in one process.
      → done, see "uip_state_t taken" below.
- [x] Audit each static for "set by option, read in several places": the
      `uip_strict_*` / `uip_lenient_*` booleans are really parser *options*;
      pass them as one `uip_options_t`.  → they are `uip.options` now; still
      set through the `uip_set_*()` pairs rather than passed, which is the
      next step if ever wanted.
- [x] A `pf_state_t` for the printfilter caches.  → `scr_pf_state_t pf`,
      see "pf_state_t taken" below.
- [x] Same audit for `scexpr.cpp`, `screstrs.cpp`, `scserial.cpp`.
      → `scr_expr_state_t expr` (ede2fd91e), `scr_restr_state_t restr`
      (ae6ebb6e6), `scr_ser_state_t ser` (a9f9f36e5); see "expr / restr /
      ser state taken" below.  Each is file-static in its own TU, grouped
      by lifetime, with the existing drop hooks (`restr_forget_bundle()`,
      `restr_cache_reset()`, `ser_forget_game()`) unchanged.
- [x] `task_state_t` for the sctasks statics.  → `scr_task_state_t tasks`,
      see "task state taken" below.
- [x] `mapdraw.cpp` and `sclibrar_disambig*.cpp`.  → `map_palette_t
      palette` (60e45292c), `scr_disambig_state_t disambig` (2c080c546);
      see "mapdraw / disambig state taken" below.  That closes the pass
      for every module the survey named; other library files still have
      their own statics (see the note there).

### Error handling (risk: med)
- 438 `assert()` and 299 `scr_error/scr_fatal` uses, mixed. Assertions guard
  both programmer errors and (in `scgamest.cpp`) "corrupt game file" cases that
  can legitimately happen on bad input.
- [ ] Classify: *invariant* → `assert`; *bad game data* → `scr_error` + clean
      abort path; *unsupported feature* → one `scr_unsupported (…)`.
- [x] `extern` declarations in `.cpp` files: `os_glk_a5.cpp` includes
      `glkstart.h` for `glkunix_stream_open_pathname` (ee3c2a185);
      `os_glk.cpp`'s `InitGlk` stays, see "Small mechanical items" below.
      `os_ansi.cpp` keeps its
      hand-written `extern run_probe_strict` on purpose (9d8981d2a:
      runner/scrunner.h drags scprotos.h's TRUE/FALSE macros onto its enum).

### Modernisation, mechanical (risk: none-low)
- [x] **`NULL` → `nullptr`**: DONE in code (0a36d0eca; comments and strings
      keep the word `NULL`). `common_utils/rgbsurface.h` is left alone: `make
      vendorcheck` requires it to match `../common_utils`, and
      `common_utils/*.c` are C.
- [x] **`strcasecmp` wrappers** (declined, see "Small mechanical items"
      below): `scr_strcasecmp` ×95, `scr_strncasecmp` ×89 but
      also raw `strcasecmp` ×18 and `strncasecmp` ×8 (many behind the
      `sc_garglk.h` shim). Use the `scr_` wrappers everywhere (they are locale
      independent, which matters for the cp1251 games) or replace both with a
      single `scr_iequals (a, b)`.
- [ ] 40 `memset (` and 25 `(void) x;` unused-parameter casts: replace the
      second with unnamed parameters or `[[maybe_unused]]` once `-std=c++17`
      is acceptable; zero-init structs with `= {}` for the first.
- [ ] ~34 lines > 100 columns and 12 function-like macros — convert the macros
      to `static inline` where they take no token-pasting.

### Locale and ctype layers (risk: low)
- `glk/os_glk_locale.cpp` (748 l.) is almost entirely four 256-entry
  code-page tables (cp1252/cp850, cp1251/cp866, each *to unicode* and *to
  ascii*) pasted as literal hex/strings. Low bytes 0x00–0x7F are the same in
  all four, and cp1252 and cp850 share the ASCII transliteration for ASCII.
- [x] Build the shared 0x00–0x7F half programmatically (or via a macro) and
      keep only the 0x80–0xFF halves literal.  → `GSC_ASCII_TO_UNICODE` /
      `GSC_ASCII_TO_ASCII`, 6258795aa (780 → 715 lines).  No generator
      script: the upper halves are hand-chosen transliterations, there is
      nothing to generate them from.
- [x] `sclocale.cpp` (574 l.) has its own ctype tables (`loc_bool_template`
      and `loc_char_template` have identical bodies, differing only in the
      element type → one template function).  → `loc_table_lookup<T>`,
      2d53d5857.
- [ ] Check overlap with `os_glk_locale.cpp` and with `scprintf_input.cpp`
      character handling; there should be exactly one cp1251/cp1252
      definition of "is alpha / to upper".

### File-size splits (risk: low, mostly moving code)
Five non-library files exceeded ~3000 lines and mixed several concerns:

| file | lines | suggested split |
|---|---|---|
| ~~`scparser.cpp`~~ | 4848 | DONE 82f5fb5e2: core · pattern · match · entity · pronoun |
| ~~`scprintf.cpp`~~ | 3899 | DONE 0f18acb3f: core · ALR/variables · output · buffer · input |
| ~~`sctasks.cpp`~~ | 3344 | DONE 26365ef84: core · move · action · endgame · run |
| ~~`library/sclibrar_disambig.cpp`~~ | 4183 | DONE 8811b5eee: core · pre-4.0 scan · 4.0 question |
| `library/sclibrar_take.cpp` / `_put.cpp` | 3849 / 3812 | **not** pre-4.0 vs 4.0: the versions interleave inside the functions (`lib_take_backend_common`, `lib_put_in_backend`), so there are no halves to cut.  A topical seam exists if still wanted — take: capacity + NPC take + backend (~1–1757) vs. all / absent / `and` / multiple (~1758–end); put: in-backend and filters (~1–1674) · all / named / multiple (~1675–3486) · put-on (~3487–end) |

The library split used a shared `sclibrar_internal.h`; the parser,
printfilter, task and disambiguation splits used the same pattern
(`scparser_internal.h`, `scprintf_internal.h`, `sctasks_internal.h`,
`library/sclibrar_disambig_internal.h`) so statics shared across the new TUs
are declared once.

### Tests and harness hygiene (risk: none)
- [x] `Makefile.headless` three object sets (plain / san / dump) — already
      factored with define/foreach; nothing to do.
- [x] `test/` `__pycache__/` is git-ignored (`test/.gitignore` line 4).
- [x] `glkdrive.py` is shared by geas and scarier (one file in both trees);
      the header comment already carries the sync rule.

---

## 7. Safety net (use for **every** item above)

Per the project's own verification recipe:

1. `make -f Makefile.headless quick` (≈4 s, no corpora) after each small step.
2. Full `make -f Makefile.headless test` (≈43 s, parallel stages) before commit;
   compare the **suite signature**, not just pass/fail.
3. `make` (Glk build) and `xcodebuild` of Spatterlight — new/renamed files must
   be added to the Xcode project (both makefiles pick them up by wildcard).
4. Sanitizer target after any memory-management change (2.3).
5. For text/ordering changes in `library/`: diff the `runner_transcripts/`
   corpus; "Runner-identical" must stay identical.
6. `run_autosave_tests.py` (geas + scarier suites) after touching serialise /
   the Glk main loops (1.4).
7. One commit per mechanical step, behaviour-neutral, so a bisect lands on a
   single rename or extraction.

## Suggested order (what is left)

1. File splits (6a): only take/put are left, and only by topic (see the
   table) — optional.  `runner/scrun_match.cpp` (3795 l.) is the other
   file still that size and was never on the list.
2. Module-global state (6a): DONE for every module the survey named
   (parser, printfilter, expr, restr, ser, tasks, mapdraw, disambig).
3. Small mechanical: DONE or declined (see "Small mechanical items").
4. 1.4 main split and 2.3: DONE (see "Main split" and "2.3" below).  3 is
   open from `oo_prop` down; `run_task`, `pass_character`,
   `lib_cmd_verb_object`, `uip_replace_pronouns`, `run_main_loop`,
   `eval_function` and `scdump_tasks` are taken (see their "split"
   sections).
5. Accessor-prologue context struct and iteration helpers (6a) — wide, own
   pass.
6. 5 (directory reshuffle, `scprotos.h` split) and 4 last; they touch
   everything.

## Outcome (2026-10-10, master 31b77eaac..18e8b35d5)

Every item was audited against the tree; the ones below were done as
behaviour-neutral steps, one commit each.  Verification per step: the full
`make -f Makefile.headless test` signature (921 lines) is identical to the one
recorded at 1b41f85be, `make -j8 scarier glkscarier` is warning-free, the
Debug Xcode `scarier` target builds, and the Spatterlight autosave suite is
58/58.

Done:

- **6 stale comments** — 31b77eaac.  Every "in sclibrar.cpp" pointer names the
  owning topic file; note citations spell their full path; Makefile.headless,
  run_restore_handler and the scutils.cpp RNG comment say what is true now.
  (9d8981d2a follows up: os_ansi.cpp keeps its hand-written `extern` because
  runner/scrunner.h drags scprotos.h's TRUE/FALSE macros onto its own enum.)
- **2.2 include umbrellas** — 8f9a7ae69.  library/sclibrar.h and
  adrift5/run/a5run_internal.h carry the system and engine includes; each
  topic TU opens with one include (+ sclibrar_internal.h where it needs it).
- **5 single source list** — 42e2cb5ac.  Makefile object and header lists are
  wildcards over sc*.cpp, library/, runner/, adrift5/{,text,run}/ and glk/.
  Makefile.headless was already factored (define/foreach); left as is.
- **1.7 map fallback chain** — d0e8ef874.  sm_try_sidestep() replaces the
  four copied perpendicular-sidestep tails in scmap.cpp.
- **1.5 (partial)** — 6a41cb252.  The five bare `depth > 32` guards in
  a5state.cpp are A5_PLACEMENT_DEPTH_MAX.
- **6a locale templates** — 2d53d5857.  One loc_table_lookup<T> instead of the
  bool and char copies in sclocale.cpp.
- **1.8 schema strings** — d1cda3926.  The records whose descriptor is
  byte-identical across versions (ROOM_LIST0/1, TOPIC, ROOM_GROUP, SYNONYM,
  ALR; HEADER for 4.0/3.9/3.8; EVENT for 4.0/3.9) are named constants.
- **1.4 Glk event loops** — 18e8b35d5.  gsc_service_window_event() handles
  Arrange/Redraw/SoundNotify for the three wait loops; the loops themselves
  stay separate because each waits on a different event set.
- **Tests: glkdrive.py** — already carried the "one file in two places, edit
  one and copy" note; nothing to do.

Declined, removed from the body above (so the next pass does not re-propose
them):

- **1.1 verb quartets** (the "all / except / multiple / common" stacks per
  verb) — measured Runner behaviour per verb, including the ordering accidents
  the runner_transcripts/ corpus pins.  A generic driver would have to
  reproduce every per-verb difference as a parameter; the risk of a text or
  ordering change outweighs the line count.  Revisit only with the corpus
  diff in hand.
- **1.2 version-gate rename / `lib_dialect_t` predicates** — ~475 call sites
  across library/ and runner/; cosmetic, and the inconsistent spellings are
  at least greppable today.
- **1.3 person-form triples** (`lib_select_response` You/I/%player%) — the
  triples are what make the Runner text greppable against the transcripts; a
  table would hide them.
- **1.5 `for_each_walk_control`** — the nine loops share a shape but not a
  body (different stop conditions and side effects); a shared helper would
  need flags that make it harder to read than the loops.
- **1.6 operator case lists** in scexpr.cpp — the claim was wrong: the lists
  are the dispatch, each case does something different.
- **1.8 delta macros** for the per-version TAF records (GLOBAL, ROOM, OBJECT,
  TASK, NPC, WALK) — a macro that builds them from a shared base plus deltas
  would hide the exact field order, and the order is the whole point of the
  table.  The "unit check for generated strings" is moot with them.
- **2.1 env registry** (`scr_env.{h,cpp}`, trace-flag folding, generated env
  reference, harness name sync) — cosmetic, wide (every getenv site), no
  behaviour win; and the determinism harness depends on *when* some flags
  are read.
- **4 os_* `SCR_MARK_*` / wrap / echo sharing** — depended on 2.1.
- **scarier.hdr** — the IFP header is maintained by hand; not a build input.

## Big item taken (2026-10-10, master 82f5fb5e2)

- **6a file split: scparser.cpp** — five TUs sharing `scparser_internal.h`
  (scparser.cpp 194 · _pattern 1047 · _match 1073 · _entity 1239 ·
  _pronoun 1336 lines).  Pure code motion, suite signature identical.  The
  seventeen externs the header listed were the inventory for the
  `uip_state_t` item (now taken, below).

## uip_state_t taken (2026-10-10, master 5edd9064b; NULL→nullptr is 0a36d0eca)

- **6a `uip_*` globals → `scr_uip_state_t`** — one instance, `uip`, defined
  in scparser.cpp and declared in `scparser_internal.h`, which carries the
  lifetime story as a doc comment.  Every former file-scope static of the
  five parser TUs is now a member of one of seven groups:
  `uip.trace` (top level), `uip.options` (binary_input, task_commands,
  lenient_tasks, strict_reference, strict_case, containment),
  `uip.tokenizer` (pattern, index, token_value, temporaries, the two
  multi-space flags), `uip.parse` (lookahead, `jmp_buf error`, group_depth,
  tree), `uip.pools` (nodes/words with cursors and counts), `uip.match`
  (string, posn, game, binary_active, comma_is_space, word_end_punctuation,
  last_group_*, strict_is_character), `uip.cache` (game, objects, npcs),
  `uip.pronoun` (used, pending_definite, the antecedent triple, the five
  `*_before_noting` registers).  The enum `scr_uip_tok_t`, the pool sizes,
  `scr_ptshortword_t` and the candidate/entity structs moved to the header
  so the struct can name them.  `uip_set_*()` entry points, the
  `uip_get/set_pronoun_flags()` autosave hooks and every other external
  function keep their names and signatures; the header's extern list drops
  the six data externs and keeps the eleven function ones.
- **`uip_state_reset()`** (scprotos.h, scparser.cpp) resets `options`,
  drops the entity cache through `uip_forget_game()`, and resets `pronoun`.
  `run_create()` calls it right after `restr_cache_reset()`.  It leaves
  `trace` alone because `scr_set_trace_flags()` sets it before any game
  exists; it leaves `pools` alone because the compiled-pattern cache in
  scparser.cpp owns nodes for the life of the process; the per-call groups
  (`tokenizer`, `parse`, `match`) are cleared by their own start/end pairs.
- **Saved vs scratch, now explicit**: the only parser state in a
  Spatterlight autosave is `uip.pronoun.used` / `uip.pronoun.pending_definite`
  (`pronoun_flags` in scrunner.cpp); everything else is per-pass or
  per-process.
- Verification: headless `quick` after each step; full `test` signature
  identical to the pre-change run (the only differences were PASS lines the
  earlier capture had dropped, nothing failing); `make scarier glkscarier`
  warning-free after replacing the anonymous `typedef struct {…}` with a
  tagged struct (`-Wnon-c-typedef-for-linkage` with default member
  initialisers); `run_autosave_tests.py --build` 58/58 with the Xcode Debug
  scarier build.  No line over 80 columns in the parser files.
- Not taken: passing `uip.options` by value into `uip_match()` instead of
  setting it through the `uip_set_*()` pairs (the runner's call sites are
  the obstacle, not the parser); the same grouping for `scexpr.cpp`,
  `screstrs.cpp`, `scserial.cpp`.

## scprintf.cpp split taken (2026-10-11, master 0f18acb3f)

- **6a file split: scprintf.cpp** — five TUs sharing `scprintf_internal.h`
  (scprintf.cpp 306 · _alr 587 · _output 556 · _buffer 1501 · _input 913
  lines; header 234).  Pure code motion in contiguous slices: core (filter
  object, create/destroy, the ALR and synonym caches, tracing) · alr
  (`pf_interpolate_vars`, the ALR walk) · output (tag dissection, entity
  unescaping, `pf_filter*`, `pf_flush`) · buffer (`pf_buffer_*`, printed /
  reference bookkeeping, paragraph / break / join rules) · input (tag
  stripping, escaping, word rewrites, synonyms, `pf_filter_input`).  The
  header carries the constants, `scr_filter_s`, `pf_str_pair_t`, the tag
  table and the cache globals as externs, and ten function externs:
  `pf_is_valid`, `pf_reset_state`, `pf_alr_key`, `pf_alr_cache_build`,
  `pf_synonym_cache_build` (core); `pf_interpolate_vars`, `pf_replace_alr`,
  `pf_alr_walk`, `pf_replace_alrs` (alr); `pf_filter_buffer` (output).
  The buffer and input files export nothing to their siblings.  Those eight
  cache globals (`pf_alr_cache`, `_built`, `_trailing_space`, `pf_alr_index`,
  `pf_alr_short`, `pf_synonym_cache`, `_built`, plus `pf_alr_on_path` in
  the alr file) are the inventory for a `pf_state_t` on the `uip_state_t`
  model, if ever wanted; `pf_trace` stays a process flag.
- Comment repoints: sctafpar_patches.cpp and the four harness docstrings
  (make_39_alrprobe, make_400_alrprobe, make_rewriteprobe, syn.py) name the
  owning file; ADRIFT4_vs_ADRIFT5.md says `scprintf_*.cpp`.
- Verification: full headless `test` signature identical to the pre-split
  run (the only diff lines are the compile lines for the new objects);
  `make -j8 scarier glkscarier` warning-free; Xcode Debug `scarier` target
  builds; `run_autosave_tests.py` all passed.  Ten lines over 80 columns in
  scprintf_input.cpp were over 80 before the split too (Runner trace
  strings); left alone.

## sctasks.cpp and sclibrar_disambig.cpp splits taken (2026-10-11, master 8811b5eee)

- **6a file split: sctasks.cpp** (26365ef84) — five TUs sharing
  `sctasks_internal.h` (sctasks.cpp 566 · _move 864 · _action 629 ·
  _endgame 701 · _run 676 lines; header 76).  Pure code motion: core
  (dispatched-run bracket, move/room assists, hints, the per-task property
  cache, `task_can_run_task()` and its refusal predicates) · move (move
  object, move character) · action (object status, variable, score, set
  task, battle; `task_run_task_action(s)`) · endgame (`task_percent_x87`,
  the score summary, the key prompt, `task_run_end_game_action`) · run
  (walk alerts, room description rules, `task_run_task_unrestricted`,
  `task_run_task`).  The action file is the two runs either side of the end
  game block; `TASK_MAXIMUM_RECURSION` moved next to its one user.  Nine
  symbols cross a boundary: `task_trace`, `task_move_assist`,
  `task_in_dispatched_run`, `task_is_repeatable`,
  `task_repeattext_is_empty`, the two move actions,
  `task_run_end_game_action`, `task_run_task_actions`.  Module statics left:
  `task_dispatch_depth`, `task_move_assist`, `task_room_assist`, the task
  cache pair (sctasks.cpp) and `task_executions` (sctasks_run.cpp) — the
  inventory for a `task_state_t` if the module-global pass gets here.
- **6a file split: sclibrar_disambig.cpp** (8811b5eee) — three TUs sharing
  `library/sclibrar_disambig_internal.h` (sclibrar_disambig.cpp 1446 ·
  _scan 1116 · _400 1686 lines; header 72).  The seam the table asked for
  is by Runner generation, not disambiguate-vs-resolve: scan = the pre-4.0
  checkobject port (`lib_co_contains` … `lib_prepass_seen_3738`), 400 = the
  4.0 question and answer slot (`lib_co_400_*`, `lib_npc_400_*`), core =
  what was either side (the "which" wording, the NPC pick,
  `lib_disambiguate_object_common`, `lib_input_contains_word*`).  Sixteen
  symbols cross a boundary and live in the new header, which is for these
  three files only; `sclibrar_internal.h` is unchanged apart from a label.
- Comment repoints: scgamest.cpp, scinterf.cpp, runner/scrun_match.cpp,
  runner/scrun_input.cpp, make_arena_probe.py, ADRIFT4_vs_ADRIFT5.md and
  the section labels in sclibrar.h / sclibrar_internal.h.  The dated notes
  under test/adrift4/notes/ still say `sctasks.cpp`; they are history.
- Verification, each split: full headless `test` output identical to the
  pre-split run once the `c++` compile lines are dropped (stricter than the
  old ok/TOTAL signature); sorted non-blank line multiset old vs new shows
  only the dropped `static`s and the new heads; `make -j8 scarier
  glkscarier` warning-free; Xcode Debug `scarier` target builds;
  `run_autosave_tests.py --build` all passed.  No new line over 80 columns.
- Not taken: take/put (no version halves, see the table); the stale
  `task_run_move_player_action` name in the scgamest.cpp comment (the
  function is `task_run_move_npc_action` today) was repointed, not renamed.

## pf_state_t taken (2026-10-11)

- **6a printfilter globals → `scr_pf_state_t`** — one instance, `pf`,
  defined in scprintf.cpp and declared in `scprintf_internal.h`, which
  carries the lifetime story as a doc comment on the `uip_state_t` model.
  The eight cache globals the scprintf split listed as the inventory, plus
  the trace flag, are its members in three groups: `pf.trace` (top level,
  process lifetime, set by `pf_debug_trace()`), `pf.alr` (`cache`, `built`,
  `trailing_space`, `index`, `unkeyed` = the old `pf_alr_short`, and
  `on_path`, which was the alr file's private static), `pf.synonym`
  (`cache`, `built`).  The header's data externs are gone; its function
  externs are unchanged.  No name outside the five printfilter files
  changes.
- **`pf_state_reset()`** replaces `pf_cache_reset()`: `pf.alr = {}` and
  `pf.synonym = {}`, called from `pf_create()` as before (run_create() is
  its sole caller, once per game), leaving `trace` alone because the
  interface sets it before any game exists.  Dropping `on_path` with the
  rest is behaviour-neutral: it is all-clear between walks and re-sized
  against the cache on the next one.
- Saved vs scratch: nothing here is saved.  Both caches rebuild lazily from
  the bundle on first use, which is why a restart (which keeps the bundle)
  needs no hook.
- Verification: headless `quick`; full `test` output identical to the
  pre-change run with the compile lines dropped; `make -j8 scarier
  glkscarier` warning-free; Xcode Debug `scarier` target builds;
  `run_autosave_tests.py` all passed against that build.  The ten lines
  over 80 columns in scprintf_input.cpp predate the change.
- Not taken: the same grouping for `scexpr.cpp`, `screstrs.cpp`,
  `scserial.cpp` (inventories above); `task_state_t` for the five sctasks
  statics.

## expr / restr / ser state taken (2026-10-11, master ede2fd91e..a9f9f36e5)

- **6a `scexpr.cpp` → `scr_expr_state_t expr`** (ede2fd91e).  Ten statics
  in three groups, each owned by a start/end pair: `expr.tokenizer`
  (expression, index, token_value, temporary, current_token),
  `expr.eval` (stack, stack_index, varset), `expr.parse` (jmp_buf error,
  lookahead, depth).  Everything is scratch for one
  `expr_evaluate_expression()` call, so there is no reset hook; the
  `scr_stack_t` typedef moved up so the struct can name it.
- **6a `screstrs.cpp` → `scr_restr_state_t restr`** (ae6ebb6e6).  Sixteen
  statics in six groups: `restr.trace` (process), `restr.props` (bundle +
  tasks: the decoded restrictions, per bundle, `restr_forget_bundle()`),
  `restr.cache` (game + bits: run400's per-task "T"/"F" strings, per game,
  `restr_cache_sync()` / `restr_cache_reset()`), and the per-evaluation
  `restr.tokenizer` (expression, index), `restr.eval` (values, depth,
  restriction, game, task, lowest_fail, bits) and `restr.parse` (error,
  lookahead).  `restr_decoded_t` / `restr_task_t` moved to the top of the
  file with their comment, because C++11 `std::vector` wants a complete
  element type where the instance is defined.
- **6a `scserial.cpp` → `scr_ser_state_t ser`** (a9f9f36e5).  Eighteen
  statics (the audit count of 13 missed `ser_callback`, `ser_tas` and the
  deflate trio): `ser.pre_v4` and `ser.raw_memo` top level (per run, both
  entry points self-clear them), `ser.write` (buffer, length, callback,
  opaque), `ser.deflate` (initialized, out_buffer, out_size, stream),
  `ser.cache` (game, object_rooms, event_task, var_count, var_name,
  var_type; per game, `ser_forget_game()` from `gs_destroy()`),
  `ser.read` (tas, line).  The pre-4.0, raw-memo and cache essays stay
  where they were; the struct comment points at them.
- Saved vs scratch: nothing in any of the three is saved.  The per-game
  groups are `restr.props` (per bundle), `restr.cache`, `ser.cache`, and
  their drop hooks are the ones that already existed; no new reset
  function was needed, unlike `uip_state_reset()` / `pf_state_reset()`.
- Verification, each commit: headless `quick`; full `test` output
  identical to the pre-change run with the compile and "Nothing to be
  done" lines dropped; `make -j8 scarier glkscarier` warning-free; Xcode
  Debug `scarier` target builds; `run_autosave_tests.py --build` all
  passed (the serialise round trip, which is the one that matters for
  `ser`).  Every renamed line that crossed 80 columns was wrapped; no
  line over 80 in the three files.
- Not taken: `task_state_t` (sctasks.cpp / sctasks_run.cpp, five
  statics), `mapdraw.cpp` (13), `sclibrar_disambig*` (five, four already
  through `sclibrar_disambig_internal.h`).

## task state taken (2026-10-11)

- **6a `sctasks*.cpp` → `scr_task_state_t tasks`** — one instance, defined
  in sctasks.cpp and declared in `sctasks_internal.h`, which carries the
  lifetime story as a doc comment.  Seven variables, not the five the
  split note counted (it left out `task_trace` and counted the cache pair
  as one): `tasks.trace` (process, `task_debug_trace()`), `tasks.options`
  (move_assist, room_assist; process, set by the interface),
  `tasks.run` (dispatch_depth, which the push/pop bracket returns to
  zero, and executions, a counter that only grows), `tasks.cache` (game,
  props; per game, `task_forget_game()` from `gs_destroy()`).  The
  instance is `tasks` because `task` is the index parameter of half the
  module.  `scr_task_props_t` moved to the header as a tagged struct so
  the state struct can name it; the `TASK_CACHE_*` values and the cache
  essay stay in sctasks.cpp.  The header's two data externs are gone;
  no function name or signature changes, nothing outside the five files
  is touched.
- Saved vs scratch: nothing is saved.  No reset function: the cache has
  its drop hook, the assists belong to whoever loads the next game, and
  `task_execution_count()`'s callers compare two readings.
- Verification: headless `quick`; full `test` output identical to the
  pre-change run with the compile and "Nothing to be done" lines dropped;
  `make -j8 scarier glkscarier` warning-free; `run_autosave_tests.py
  --build` all passed.  One line over 80 columns in sctasks_run.cpp
  (a quoted p-code listing) predates the change.
- Not taken: `mapdraw.cpp` (13), `sclibrar_disambig*` (five, four already
  through `sclibrar_disambig_internal.h`).

## mapdraw / disambig state taken (2026-10-11, master 60e45292c, 2c080c546)

- **6a `mapdraw.cpp` → `map_palette_t palette`** (60e45292c).  All
  thirteen statics were palette: `palette.bg` / `palette.fg` (what the
  host passed to `map_set_palette()`) and `palette.derived` (ready, the
  eight resolved colours, the two link alphas;
  `rebuild_derived_palette()`).  File-static, process lifetime, the
  host's and not a game's, nothing saved.  `kBigFont` / `kSmallFont` are
  const references, not state.  Nothing outside the file is touched.
- **6a `sclibrar_disambig*.cpp` → `scr_disambig_state_t disambig`**
  (2c080c546).  Twenty variables, not five: the survey counted only the
  `static` ones of one file.  Groups: `disambig.which` (bundle,
  runner_form; per bundle, dropped by `lib_co_400_reset()`),
  `disambig.long_names` (bundle, names[2]; per bundle,
  `lib_forget_bundle()`), `disambig.line` (prompt_370_blocked, top_mode,
  top_fits; `lib_co_note_line_top()`), `disambig.resolve_parent_400`
  (one call, take-from), and the 4.0 trio `disambig.question` (pending,
  refused, was_pending, term, command, candidates, spend),
  `disambig.element` (named_raised, therest_split), `disambig.offered`
  (list, seen, flagged).  The struct is declared in
  `sclibrar_internal.h`, not `sclibrar_disambig_internal.h`, because
  battle / refuse / takefrom / verbobj write five of the flags; those
  five externs and the four data externs of the disambig header are
  gone.  The instance is defined in sclibrar_disambig.cpp.
  `LIB_LONG_NAME_CATEGORIES` moved to the header with the struct.
- Saved vs scratch: `disambig.question.pending/term/command/candidates`
  and the offered list (when `offered.flagged && offered.seen`) travel in
  a Spatterlight autosave through `lib_co_400_get/set_question()`; the
  struct comment says so.  Everything in `palette` and the rest of
  `disambig` is scratch or rebuilds from the bundle.
  `lib_co_400_reset()` clears `which`, `question`, `element`, `offered`
  (the same members as before, a group at a time) and still leaves
  `line`, `long_names` and `resolve_parent_400` to their own owners.
- Verification, each commit: headless `quick`; full `test` output
  identical to the pre-change run with the compile and "Nothing to be
  done" lines dropped (the a5 map views cover mapdraw); `make -j8
  scarier glkscarier` warning-free; `run_autosave_tests.py --build` all
  passed after the second (the open-question cases are the ones that
  matter for `disambig`).  No changed line over 80 columns.
- Not taken: the other library files' own statics, which the survey did
  not list -- `run_input_reset()` shows three more reset hooks of the
  same kind (`lib_battle_who_reset()`, `lib_with_prefix_390_reset()`,
  `lib_put_reset()`), and runner/ has its line buffers.  A census of
  those is the natural next step if the two-games-in-one-process goal is
  ever real.

## Small mechanical items (2026-10-11, master ee3c2a185, 6258795aa)

- **`.cpp` externs** — ee3c2a185.  os_glk_a5.cpp includes `glkstart.h`
  (inside `extern "C"`, on the non-Windows-Glk side of its `#if`, the way
  os_glk.cpp does) instead of redeclaring
  `glkunix_stream_open_pathname()`.  Not in os_glk_internal.h: Windows Glk
  has no such header.  **`InitGlk` left**: it is Windows Glk's, `WinGlk.h`
  is not in the tree and the `_WIN32 && !GARGLK` branch cannot be compiled
  here, so there is no way to check that a header declares it.
- **os_glk_locale tables** — 6258795aa.  The low halves of all four
  unicode tables are identical, and so are the four ascii ones (checked by
  parsing the initialisers); they are two macros now and each table lists
  only 0x80–0xff, sixteen rows of eight.  Proof of neutrality: `c++
  -DNDEBUG -E -P` of the file is identical before and after, whitespace
  removed (without NDEBUG the only difference is assert()'s line numbers).
- **`strcasecmp` wrappers — declined.**  The survey's count was of the
  whole tree; the 4.x engine, library/, runner/ and glk/ have no raw call
  left.  Every raw `strcasecmp` / `strncasecmp` is in `adrift5/` (plus
  one test harness), and there it is the right function: ADRIFT 5 text is
  UTF-8 and those compares are of ASCII keywords, property values and
  names, where the `scr_` wrappers would fold bytes >= 0x80 through the
  4.x cp1251/cp1252 locale table.  `scr_strncasecmp()` also does not stop
  at a NUL.  The MSVC spelling is already one shim
  (`common_utils/sc_garglk.h`), included from four places.
- **1.9 — declined.**  a5parse.cpp `re_group_end()` /
  `re_has_toplevel_alt()` share a nine-line escape / class / depth walk
  but leave it differently (first balanced `)` vs. any `|` at depth 0); a
  shared walker would need a callback for two callers.  a5state.cpp
  `exists_at()` / `location_root_at()` are ports of two different Runner
  functions (ExistsAtLocation, LocationRoots) whose switch arms differ on
  purpose in four of the cases; the comment on the second says where.
- Verification: `make -j8 scarier glkscarier` warning-free and
  `run_autosave_tests.py --build` all passed after each commit.  The
  headless suite does not compile glk/, so it was not the check here.

## Main split (2026-10-11, master 209c1092f, 1d8a133e3)

- **`gsc_a5_main`** (350 l.) is the turn loop only (~45 l.) over
  `gsc_a5_main_setup`, `gsc_a5_main_opening` (intro or autorestore, then
  the load-error stop), `gsc_a5_game_over` (the banner),
  `gsc_a5_next_command` (prompt / autosave / read or replay),
  `gsc_a5_host_command` (returns `gsc_a5_command_t`: GAME / HANDLED /
  QUIT), `gsc_a5_play_turn`, `gsc_a5_main_shutdown`.  Helpers take
  `a5_run_t *&run` like `gsc_a5_restart_run`, so the `gsc_a5_run` alias
  survives a swap.  The banner has its own input buffer now (the loop's
  was always overwritten before its next use).
- **`gsc_main`** (230 l.) keeps the type check, the ordering of the
  startup stages and the restart loop; `gsc_main_open_windows`,
  `gsc_main_startup_notes`, `gsc_main_autorestore` (SPATTERLIGHT only;
  the `goto restored` is a `return`), `gsc_main_game_over` (returns
  "run again?"), `gsc_main_shutdown` hold the rest.
- `gsc_startup_code` (230 l.) not touched: it is one long sequence of
  option handling with early returns, and nothing in it repeats.
- Verification: `make -j8 scarier glkscarier` warning-free, headless
  `quick`, `run_autosave_tests.py --build` all passed.

## 2.3 (2026-10-11, master 718cc3c1d, 7c0950702, 6fb39dcb7)

- **`scr_strdup (const scr_char *)`** — 718cc3c1d.  New overload beside
  the `std::string` one; replaces 14 `scr_malloc (strlen (s) + 1)` +
  `memcpy`/`strcpy` pairs (scvars, scmemos, scresour, scexpr, sctafpar,
  scgamest, scprops x2, scprintf_output x2, library misc / meta / examine
  / disambig_400).  `scexpr.cpp:239` and `scdebug.cpp` allocate that size
  without copying, so they stay.
- **sprintf / strcpy** — 7c0950702.  The other twelve: sxstubs `sprintf`
  -> `snprintf` (8), scmap `strdup`, sctafpar_patches `memcpy` +
  `snprintf`, a5run and os_glk_a5 `memcpy` of a length already known.
  All were bounded already; the point is that `grep -w
  'sprintf\|strcpy\|strcat'` over the sources is now empty (two comments
  aside), so a new one stands out.
- **Function-local owners** — 6fb39dcb7.  Eight scratch copies freed by hand
  on every return path now sit in `scr_owned_string` (scdebug
  `debug_parse_command` x2, scresour, library misc / meta / examine /
  disambig_400 / talk).
- **What is left on purpose.**  The other ~45 raw `scr_malloc` /
  `scr_realloc` sites in the 4.x engine are not function-local: struct
  owners with a matching destroy function (scdebug, scmemos, sctaffil,
  scvars, scserial), strings handed to `prop_adopt()`, pool / cache
  entries (scparser, scprops), and stack-buffer-with-heap-fallback
  (scparser.cpp:76, scparser_pattern.cpp:100, sclibrar_dispatch x3), which
  an owner type would make slower for nothing.
- **ADRIFT 5 not taken.**  `adrift5/` has ~30 `malloc` but ~360 `free`:
  its convention is that text functions return a `malloc`ed `char *` the
  caller frees (`a5run_intro`, `a5run_input`, `a5text_*`).  Wrapping that
  is an API change across a5text / a5run / the Glk front end / every
  harness, not a mechanical batch; it wants its own item and a leak
  checker that runs here (LeakSanitizer does not), e.g. `leaks --atExit`.
- Verification per commit: `make -j8 scarier glkscarier` warning-free,
  full headless `test` output identical to the 71339c36f baseline,
  `sanitize` clean; autosave suite after the glk/ change.

## 3: run_task split (2026-10-11)

- **`run_task`** (520 l.) in `adrift5/run/a5run_action.cpp` is a 44-line
  driver over `run_task_look` (148: the stock Look's probe / pin dance,
  which was the early-return block) and four phases sharing a
  `run_task_ctx` (run, st, t, depth, out, before, comp, act, self_ti; the
  resp-path pre-render triple; the flat interleaved quintet plus its side
  buffer; `imm0`; the deferred `Execute Look` claim and the four saved
  look registers): `run_task_before_message` (116: the resp / flat fast /
  flat interleaved pre-action renders), `run_task_actions` (70: the
  local defer-look claim, the loop with the imm frame, the splice),
  `run_task_before_finalize` (78: the frozen-template compare + finalize,
  the resp compare, and the release of both sets of renders),
  `run_task_after_message` (87: the three After paths).  The done mark
  between phases 1 and 2 stays in the driver because its comment is about
  the ordering.  Pure code motion: every comment moved with its code, the
  loop variable in phase 2 is `n` because `c` is the context, and the
  frees that were at function end now sit at the end of the phase that
  consumed the renders (same order: flat trio, side buffer, resp pair).
- Verification: headless `quick`; full `test` output identical to the
  pre-change run with the compile and "Nothing to be done" lines dropped
  (the 176 ADRIFT 5 walkthroughs pin the draw stream the three Before
  paths exist for); `make -j8 scarier glkscarier` warning-free;
  `sanitize` clean.  No new line over 80 columns (the file had 161
  before, 142 after).
- Next in the table: `pass_character` (a5restr.cpp), then
  `lib_cmd_verb_object`.

## 3: pass_character split (2026-10-11)

- **`pass_character`** (500 l.) in `adrift5/a5restr.cpp` was not
  phase-sequential, as the table guessed, but a 26-way `streq (r->op, …)`
  chain, so it took the other recurring idea: a name -> handler table.
  `char_restr_t` (anonymous namespace: st, r, k1, k2, ci, cloc -- the
  function's four-line prologue) has one `const` member per operator, 22 of
  them (`be_at_location` … `be_on_character`, snake case of the operator
  name), and `char_restr_ops[]` maps the 26 names to them in the order the
  chain tested them; the five on/in-object operators share
  `be_on_or_in_object`, which still reads `r->op` for the stance and the
  in/on half.  `pass_character` is 15 lines: fill the struct, walk the
  table, fall back to the best-effort `return 1`.  Members rather than free
  functions taking a context, so the bodies are verbatim (dedented four
  columns, nothing renamed): every comment moved with its code, and the two
  group comments sit above the first function of their group.  The longest
  handler is `be_on_or_in_object` (77 l.); its ANYCHARACTER half is a
  possible further cut, not taken.
- Verification: headless `quick`; full `test` output identical to the
  pre-change run with the compile and "Nothing to be done" lines dropped;
  `make -j8 scarier glkscarier` warning-free; `sanitize` clean.  Lines over
  80 columns in the file: 96 before, 78 after (the dedent), none new.
- `pass_object` / `pass_location` in the same file are the same shape
  (shorter) and could follow the same model if wanted.
- Next in the table: `lib_cmd_verb_object` (sclibrar_verbobj.cpp), which is
  Runner-parity library text -- transcript diff, safety net step 5.

## 3: lib_cmd_verb_object split (2026-10-11)

- **`lib_cmd_verb_object`** (385 l.) in `library/sclibrar_verbobj.cpp` is a
  70-line driver (half of it the 4.0 ended-game essay, which stays beside
  its two-line test) over eight stages, each carrying the comment that sat
  on its block: `lib_verb_object_count_present`, `_tie_400` (the 4.0
  ambiguity prompt for a count that is not 1), `_first_named_370`,
  `_first_named_390`, `_absent` (the can't-see family, 93 l.),
  `_resolve_400`, `_top_taken_400` (the "must be in the same room" arm) and
  `_answer` ("What <Short>?" / "I don't understand what you want me to do
  with").  The 3.8 arm stays in the driver: it is one call and its result
  mapping.  The stages share `count` / `object` through pointers, the way
  the file's other out-parameters go, and the three that can end the
  command return `lib_vo_stage_t` (GO_ON / RETURN_TRUE / RETURN_FALSE).
  Code motion apart from `count` -> `*count`, `object` -> `*object` and the
  stage returns; the TAF version is read once in the driver.
- Verification: headless `quick`; full `test` output identical to the
  pre-change run with the compile and "Nothing to be done" lines dropped;
  `make -j8 scarier glkscarier` warning-free.  Safety net step 5:
  `runner_transcripts.py recompare` over the whole corpus, once with the
  old file and once with the new, leaves byte-identical `manifest.tsv` and
  `compare/` trees.
- Found on the way, not fixed here: (a) `harness/patchtable.py` still read
  the patch table from `sctafpar.cpp`; it lives in `sctafpar_patches.cpp`,
  so `recompare` died on the first patched row (path repointed, see git).
  (b) Against the committed manifest, a whole-corpus `recompare` at HEAD
  moves ~120 rows from "identical" to "N differing turn(s)" -- e.g.
  3monkeys `look in bed` ("is" vs "are"), aquarius2 `take stone` (the
  "Which Stone?" question is gone).  The manifest predates later engine
  work; which rows are deliberate deviations and which are regressions is
  its own job.
- Next in the table: `uip_replace_pronouns` (scparser_pronoun.cpp).

## 3: uip_replace_pronouns split (2026-10-11)

- **`uip_replace_pronouns`** (371 l., two thirds of it measurement
  comments) in `scparser_pronoun.cpp` is a 90-line driver -- the two
  passes, the word walk, the copy-on-write and the offset bookkeeping --
  over a per-word `uip_pronoun_t` (anonymous namespace: game, bundle, word,
  is_character_word; then what the word resolves to: object, npc, extent,
  prefix, name, echo, and `definite`, which owns a composed prefix).  Six
  members, each carrying the comment that sat on its block:
  `find_register` (which of it / him / her holds something),
  `object_phrase` (the Prefix forms by version and `it_form`), `no_object`
  ("Absolutely nothing"), `no_character` ("No male" / "No female" /
  "Nobody"), `resolve` (the reset, the four-way choice with the NPC arm
  inline, and the old `prefix && name && extent > 0` test as its result)
  and `substitute` (build the replacement, the bracket echo, the splice
  and re-lower-casing; returns the length to skip).  Members rather than
  functions taking a context, on the `char_restr_t` model, so the bodies
  are verbatim: the file was rebuilt from line slices of the old one,
  dedented, with nothing renamed.  `uip.pronoun.used`, the copy-on-write
  and the trace line stay in the driver, in the order they had.
- Verification: headless `quick`; full `test` output identical to the
  pre-change run with the compile and "Nothing to be done" lines dropped;
  `make -j8 scarier glkscarier` warning-free; `sanitize` clean.  Safety
  net step 5: `runner_transcripts.py recompare` over the whole corpus,
  once with the old file and once with the new, leaves byte-identical
  `manifest.tsv` and `compare/` trees.  **`recompare` does not rebuild**:
  it runs `test/adrift4/harness/scare` as it finds it, so make that target
  (`make -f Makefile.headless test/adrift4/harness/scare`) after each
  swap of the source, or the two runs compare one binary with itself.
  No line over 80 columns in the file, before or after.
- Next in the table: `run_main_loop` (runner/scrunner.cpp).

## 3: run_main_loop split (2026-10-11)

- **`run_main_loop`** (353 l.) in `runner/scrunner.cpp` is a 55-line driver
  -- the census exit, the fresh-game test, the loop with its resource sync
  and flush, the shutdown call -- over six stages under one head comment:
  `run_main_intro` (clear, title line, StartupText, the alignment and
  emphasis closers; the block an autorestore to a startup prompt skips),
  `run_main_startup` (the rest of the first-time branch: name and gender
  prompts, load events, opening look, intro resource, NPC and battle
  setup, the 3.90+ startup tick, the debugger notice, room seen),
  `run_main_turn_input` (read a line or spend a wait turn; returns the
  status), `run_main_turn_tick` (the SCR_TRACE_ADMIN line and the
  turn count / NPC / event tick the status earned), `run_main_turn_end`
  (the pre-4.0 ambiguity prompt, the `go <place>` arrival, the armed
  ending) and `run_main_shutdown` (everything after the loop).  No context
  struct: the stages share only `game`, and each re-fetches the filter /
  vars / bundle it uses, as the rest of the file does.  The sync and flush
  stay in the driver because `game->is_admin = FALSE` between them and the
  input is the ordering the tick depends on.  Bodies are verbatim: the
  function was rebuilt from line slices of the old one, dedented, nothing
  renamed, every comment with its code; a sorted line multiset old vs new
  shows only the new heads, prologues and calls.
- Verification: headless `quick`; full `test` output identical to the
  pre-change run with the compile and "Nothing to be done" lines dropped;
  `make -j8 scarier glkscarier` warning-free; `sanitize` clean;
  `test/adrift4/harness/run_autosave_tests.py --build` all passed (the
  startup-prompt resume cases are the ones that matter for
  `run_startup_resume`).  Lines over 80 columns in the file: 14 before, 4
  after (the dedent), none new.
- Next in the table: `eval_function` (adrift5/text/a5text_func.cpp), a
  name -> handler table candidate.

## 3: eval_function split (2026-10-11)

- **`eval_function`** (345 l.) in `adrift5/text/a5text_func.cpp` is a
  25-line driver.  The `fn_*` handlers already existed; what was left was
  the 45-arm `ci_eq (name, ...)` chain that picked one, and three inline
  blocks.  The chain is `eval_builtins[]`, 48 rows of {name, `fn_args_t`,
  handler} in the order the chain tested them, where `fn_args_t` says in
  which shape the name is a call: `FN_ARGS_ANY` (%Name% and %Name[args]%),
  `FN_ARGS_REQUIRED` (only with an [args] block) or `FN_ARGS_NONE` (only
  without).  No name sat in two arms, so first match in the table is the
  chain.  The four arms that returned inline have handlers now
  (`fn_convcharacter`, `fn_replace`, `fn_version`, `fn_release`), each
  with its comment.  The blocks are `eval_udf` (the user-defined function
  pass, with its `udf_depth` static; NULL = no UDF of that name),
  `eval_bound_reference` (the parser's %direction% / %text% / %objectN% /
  %characterN% slots), `eval_variable` (bare %Name%) and
  `eval_variable_element` (%Name[index]%).  The reference block has one
  `return nullptr` that is final (a singular %object% off a plural
  binding stays verbatim and must NOT go on to the variables), so it
  takes an `int *unbound` it sets only when it falls off the end; that
  kept its body verbatim.  All four bodies are line slices of the old
  function, dedented.
- Verification: headless `quick`; full `test` output identical to the
  pre-change run with the compile and "Nothing to be done" lines dropped;
  `make -j8 scarier glkscarier` warning-free; `sanitize` clean.  A sorted
  line multiset old vs new loses only the chain's `if` / `return fn_*`
  lines.  Lines over 80 columns in the file: 83 before, 76 after, none
  new.
- Next in the table: `scdump_tasks` (scdump.cpp, developer tooling), then
  `oo_prop` (adrift5/a5expr.cpp).

## 3: scdump_tasks split (2026-10-11)

- **`scdump_tasks`** (336 l.) in `scdump.cpp` is a 30-line loop over a
  per-task `scdump_task_t` (anonymous namespace: game, bundle, t; the
  property key `k[5]` and `vt` every section reuses; and what the TASK
  line reads that later sections need -- cmd, wtype, wroom, the command /
  restriction / action counts, rep, rev).  Eight members, one per section
  of the dump, each under the comment that sat on its block: `summary`
  (the TASK line), `where_rooms`, `hints`, `texts`, `alt_commands`,
  `reverse_commands`, `restriction (i)` and `action (i)`.  Members on the
  `uip_pronoun_t` model, so the bodies are line slices of the old loop,
  dedented, nothing renamed; `ci` is a local of the one member that uses
  it.  The struct is `= {}`-initialised where the old locals were not.
- Verification: the suite does not read this output, so the check was the
  output itself -- `SCR_DUMP_TASKS=1 test/adrift4/harness/scare <game>`
  over all 955 `.taf` files under test/adrift4 (100113 lines, 10770
  TASK, 11968 RESTR / ACT), stderr byte-identical per game before and
  after.  Also headless full `test` output identical with the compile
  lines dropped, and `make -j8 scarier glkscarier` warning-free.  Two
  lines over 80 columns in the file, before and after.
- Next in the table: `oo_prop` (adrift5/a5expr.cpp).
