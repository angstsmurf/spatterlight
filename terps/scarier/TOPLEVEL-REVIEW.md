# Top-level source review (terps/scarier/*.cpp, *.h) — 2026-09-28

Same rules as the library/ and runner/ refactors: behaviour-preserving,
verify with `make -f Makefile.headless test` (suite signature), `make -j8`,
`xcodebuild -target scarier`, and for the map files `test/run_map_corpus.sh`
+ `make -f Makefile.headless a5maptest`.

## Applied 2026-09-28 (steps 1, 2, 7-dead, and the sx/scinterf parts of 6)

Suite signature identical (adrift4 706 ok, adrift5 174 ok, MATCH=173),
`make -j8` and `xcodebuild -target scarier` clean, a5maptest 6/6, and the
whole-corpus map manifest byte-identical to a clean HEAD build.  (Note:
`test/run_map_corpus.sh` itself reports FAIL against
test/map_corpus_golden.txt both before and after -- the golden predates
32ff2819e "add shades to default color scheme" and the test-tree reorg, so it
needs a deliberate `-b` re-bless; not done here.)

- Error paths: sctafpar parse_taf_fail() and the whole scserial load side
  now throw (`parse_taf_error_t`, `ser_tas_error_t`) instead of longjmp, so
  the scr_owned_string in parse_read_multiline and the std::vector in
  ser_restore_object_location unwind.  ser_load_game is split into
  ser_load_game_body / ser_load_game_finish; ser_save_game_internal wraps a
  new ser_save_game_body and on scr_fatal calls ser_flush_abort() (deflateEnd
  + buffer release) before rethrowing, so no ser_* static survives a fatal.
  setjmp remains only in scexpr/screstrs (hot paths, no C++ objects).
- task_run_set_task_action range-checks Var2 (trace + FALSE);
  task_run_task's recursion_depth is an RAII guard; screstrs type-5
  scr_trace gated on restr_trace; SCR_TMP_NOIMM / SCR_TMP_NOTICKED getenvs
  gone; the 10 dead `return` after scr_fatal gone; the three strict-warning
  items fixed (sclocale cast, scparser FALLTHROUGH).
- scinterf: `if_guarded()` template (value and void overloads) now wraps the
  31 extern "C" entries that had no try/catch; the 16 pre-existing explicit
  blocks were left as they were.  Stale longjmp/P3 comments fixed.
- Dead code removed: battle_attribute, memo_clear_games, obj_object_index /
  obj_container_index / obj_surface_index / obj_stateful_index,
  ser_set_fast_compression (+ ser_compression), evt_buffer_text's event
  parameter, uip_assign_pronouns' unreachable gender else, the `#if 0`
  precedence table, unused <math.h>/<limits.h>/<time.h>/<errno.h>/<setjmp.h>
  includes; map_find / evt_can_see_event / pf_buffer_paragraph / scr_rand /
  task_in_dispatched_run made static with their scprotos.h prototypes
  dropped; map_pt_t.z and map_page_t.label (write-only) removed.
- os_glk.cpp: LINUX_GRAPHICS blocks (`-ng`, xv shell-out), the non-Unicode
  stubs, `#ifdef TRUE`, and the unreachable "No ADRIFT 5 game loaded" guard
  dropped; os_ansi.cpp os_show_graphic is the plain stub.  GARGLK/WinGlk
  blocks kept.
- sx verifier: sxmain/sxtester/sxscript/sxfile.cpp `git rm`'d, Makefile
  XOBJECTS/`sx` target/clean entry gone, sxprotos.h trimmed to what
  sxstubs/sxglob/sxutils (headless harness) still export, README updated.

## Applied 2026-09-28, round 2 (de-dups, stale comments, first splits)

Same verification as round 1: suite signature identical, `make -j8`,
`make glkscarier` and `xcodebuild -target scarier` clean, a5maptest 6/6,
whole-corpus map manifest byte-identical, and three Glk walkthrough
transcripts identical.  Behaviour-preserving throughout; all uncommitted.

- scparser/scexpr/scvars: uip_wildcard_prepare / uip_match_right_siblings /
  uip_match_wildcard / uip_entity_admitted / uip_record_entity_match pulled
  out of the wildcard and entity matchers; uip_debug_dump takes the tree;
  scexpr expr_tokenize_guard (RAII) replaces the paired set/clear;
  scr_lowercase in scutils replaces the hand-rolled loops.
- sctasks/screstrs/scevents/scnpcs/scbattle: task_selector_npc replaces one
  NPC selector ladder (the other copies are still owed, see 3.C below);
  restr_cache_sync / evt_cache_sync share one shape; evt_cache_entry,
  evt_taf_version, evt_gate_task_is_complete, npc_walk_chartask,
  battle_kill_drained extracted; gs_carried_suspend_guard (RAII) declared
  in scprotos.h.
- sctafpar/sctaffil/scprops/scresour: C1 (res_lookup_resource with a
  scr_res_lookup_t), C2 (taf_drain_callback shared by taf_unobfuscate and
  taf_populate_raw / taf_populate_fn), C4 (parse_put_property +
  parse_put_indexed_*), C5 (V400_ROOM_EXIT_DESC), C6 (prop_find_leaf);
  B1/B2 stale comments (prop_put_integer doc block).
- scinterf: if_report_fatal shared by the if_guarded overloads;
  if_attrs_t / if_get_attributes replaces the repeated attribute triple.
- mapdraw/scmap: draw_dir_icon_site + N_BADGE_SITES replace draw_dir_icon_xy
  and node_has_link_dir; scmap tidy-ups.
- os_glk.cpp: gsc_echo_input, gsc_colour_startup_prepare, gsc_undo_refusal,
  gsc_readlog_line, gsc_map_click, gsc_autorestore_replace_state and
  gsc_a5_undo_command factor the sc/a5 pairs that were byte-alike;
  gsc_put_buffer removed (~530-line diff).  The 12-TU split is NOT done.
- scprotos.h: the 43 lib_* prototypes that were interleaved with the run_*
  ones now sit together under "Library hooks called from the runner's line
  passes"; the duplicate `#include <string>` is gone.  They stay in
  scprotos.h rather than moving to a library header (the runner and the
  library include each other through it, so a new header bought nothing).
- Stale comments: every "scrunner.c" / "sclibrar.c" / "scdump.c" /
  "scnpcs.c" mention now names the real file (runner/scrun_dispatch.cpp,
  runner/scrun_match.cpp, library/sclibrar_put.cpp, scdump.cpp, scnpcs.cpp);
  B3 partly (env catalogue not extended); Makefile comments for the adrift5
  `-I.` and MAPOBJECTS lines; scnpcs unused `bundle` local removed.
- scdebug.cpp 2049-2673 read (watchpoint checks, command parser and
  dispatcher, dialog, game start/end/turn hooks): no findings beyond the
  SCARIER prose below.
- Build/docs: Makefile.headless `clean` now removes the map/dup tools;
  README build paragraph (make = ANSI, make glkscarier = Glk, Spatterlight
  via Xcode) and walkthrough paragraph (test/adrift4, test/adrift5);
  ADRIFT4_vs_ADRIFT5.md no longer names the deleted a5arith.cpp;
  scarier.hdr says 1.4.0 (IFP_ENGINE_VERSION too).
- By user request the upper-case "SCARIER" prose (user-facing strings such
  as the banner and version line, plus comments) is now "Scarier"
  everywhere; macro names (SCARIER_*, IDI_SCARIER, SCARIEREXT_*) and the
  golden tag SCARIER-SURPASSES-FD are untouched.
- Map corpus golden re-blessed with `test/run_map_corpus.sh -b` after
  eyeballing a rendered view (see the round-1 note on why it was stale).

## Applied 2026-09-28, round 3 (selector ladders, scvars, section 5 smalls)

Suite signature identical, `make -j8` and `xcodebuild -target scarier` clean.

- task_selector_npc now covers every var-2 ladder: task_move_object held/
  worn/same-room (ref and NPC-id arms merged; the NPC-id-only run400
  pre-stamp is kept behind `var3 != 1`), the move-NPC same-room arm (one
  trace line), the battle-attribute arm, and screstrs npc1/npc2.
  Latent, NOT changed: the move-NPC "same room as referenced NPC" arm has
  no unset-reference guard, so gs_npc_location(-1) asserts; the object
  movers abandon instead.  Unmeasured at the Runner.
- scvars: var_find_object_by_short (status_/marker scans), var_clear_temp /
  var_set_temp, var_number_text (%t_number%/%t_var%), var_openness_word
  (obstatus/status_), var_object_is_stateful + var_set_temp_state
  (obstate/state_), var_definite_name (var_print_object_np and
  %theobject%), var_collect_at_object + var_list_at_clause (at/onin
  listers; the nested ", and ... inside" clause untouched); %object% built
  with one snprintf.
- scexpr: expr_parse_variable_factor replaces the numeric/string
  TOK_VARIABLE copies.
- scgamest: gs_object_place_unchecked (+ gs_rp_mode_t) behind the 8
  unchecked movers; gs_copy copies npcs whole; the second NPC child-count
  read gone; loadtime accessors assert gs_is_game_valid; scgamest.h
  includes inside its guard.
- scserial: raw-memo and pre-4.0 ser_flush branches merged; stale
  encumbrance comment fixed.  scmemos: memo_set_text.  scprintf:
  pf_interpolate_vars' redundant buffer_used flag gone.

## Applied 2026-09-28, round 4 (the big splits, C7, the review gaps)

Suite signature identical, `make -j8 glkscarier` (same three pre-existing
unused-variable warnings) and `xcodebuild -target scarier` clean.  The dump
splits were also checked by running the old and new `scare` over the whole
corpus with SCR_DUMP_TASKS/OBJLOC/ALRS + SCR_DUMP_BATTLE: byte-identical
stderr apart from the WALK loop fix below.

- scdebug: `triggered |= TRUE` -> `= TRUE`; debug_set_enabled only
  initializes/finalizes on an actual state change.  2049-2673 now read;
  nothing else found.
- a5model_load_buffer file_buf ownership: checked, correct, no change.
- scevents: evt_tick_event is a dispatcher over evt_pause_is_due and one
  evt_tick_<state> per event state.  evt_finish_event keeps the finish
  text/resource and the Obj2/Obj3 moves, then calls
  evt_finish_affected_task (task range check, TaskFinished clear, the
  snapshot + run + lower-index recheck) and evt_finish_restart.  That
  helper's one-shot gate now returns instead of setting restarttype = -1,
  so case -1 is gone.  evt_run_affected_task is the four-arm run ladder.
- scgamest: gs_populate split into gs_populate_objects (+
  gs_populate_dynamic_object), _events, _npcs, _player and _seen_sweep.
  The tasks loop stays inline.
- scdump: scr_dump_structure_once is a driver over one scdump_<section>
  helper per section.  C7: scdump_rule_table prints both ALR and SYNONYM;
  scdump_battle_ranges prints both the player and the NPC BATTLE lines.
  **Fixed:** WALK `loop` was read as nv.integer from a boolean property
  and printed garbage (e.g. 34359738369 in athylon); it is now nv.boolean.
- sctafpar: parse_fixup_v380 is a dispatcher over _task_restrictions,
  _initial_positions(is_v370) (the 3.7/3.8 comment moved into its header),
  _max_score and _meet_object.
- os_glk: the map pane (map globals, walk state, gsc_map_click, and the
  whole "ADRIFT 5 map window" section through gsc_command_zoom) moved
  verbatim to os_glk_map.cpp (1260 lines).  os_glk.cpp is down to ~10060
  lines.  os_glk_internal.h carries only the crossing names:
  - 11 map variables and 17 map functions out of os_glk_map.cpp;
  - gsc_game, gsc_a5_adv, gsc_a5_run, gsc_is_a5, gsc_game_key,
    gsc_main_window and 5 output/help helpers in the other direction.
  gsc_main_window's joint declaration with gsc_status_window is split so
  that only gsc_main_window leaves static.  The Makefile builds
  $(GLKOBJECTS) with one pattern rule for glkscarier and the plugin.  The
  Xcode scarier target has the new file refs and a Sources entry.

## Applied 2026-09-28, round 5 (the rest of the os_glk split)

- os_glk.cpp (10059 -> 1701 lines) keeps module state and utilities,
  events, files, options/startup, the ADRIFT <=4 main loop (glk_main) and
  the UNIX/WinGlk linkage.  Moved out verbatim, by section:
  - os_glk_locale.cpp (732): codepages, character output;
  - os_glk_status.cpp (447): the status line;
  - os_glk_output.cpp (1357): tags, fonts, colour mode, inline graphics, hints;
  - os_glk_symbols.cpp (395): the symbol-font tables;
  - os_glk_resources.cpp (427): ADRIFT <=4 sound/graphics, title window;
  - os_glk_commands.cpp (1978): the "glk" commands, help, GSC_PORT_VERSION;
  - os_glk_input.cpp (444): ADRIFT <=4 line input;
  - os_glk_a5.cpp (1353): the ADRIFT 5 driver and main loop;
  - os_glk_a5_display.cpp (772): ADRIFT 5 text display and media;
  - os_glk_autosave.cpp (528, all under SPATTERLIGHT).
  os_glk_map.cpp is unchanged.
- os_glk_internal.h now carries all the includes, the host feature macros
  (GSC_HAVE_ZCOLORS / _TITLE_WINDOW / _UNPUT), the shared types and
  constants, and the crossing names grouped by defining file (40 -> 147
  externs; 104 former statics promoted, nothing else).  Its top comment
  maps the TUs.
- Static forward declarations that would have masked a cross-TU use were
  removed (gsc_sc_apply_all, gsc_autosave, gsc_refresh_windows,
  gsc_a5_open_side_window); sound_channel is non-static in both of its
  #if branches; gsc_status_printed_width lives with the locale output that
  sets it.  "Above/below/earlier in the file" comments were re-pointed.
- The Makefile's GLKOBJECTS and the Xcode scarier target list the new files.
- Verified: zero errors/warnings for every os_glk TU in both configs
  (cheapglk -Wall -Wextra, and the Xcode -DGARGLK -DSPATTERLIGHT args);
  `make glkscarier`, `make -j8`, and xcodebuild scarier (Debug+Release)
  succeed; seven cheapglk transcripts (a4 3monkeys/Glum/Party/"glk"
  commands, a5 4rooms/Puzzle/"glk" commands, with SCR_STABLE_RANDOM_ENABLED
  + xoshiro) are byte-identical to the pre-split binary; suite signature
  unchanged (adrift4 706 ok, adrift5 174 ok, MATCH=173).

- scarier-autosave.mm (345 lines) read.  It is clean plumbing (exists /
  wanted / discard / write / read / restore, with plist archive hooks
  mirroring gsc_stash/recover_frontend_state field for field).  One real
  gap: write() moved the new glksave into place before it wrote the plist,
  so a crash or a failed plist write between the two paired the new engine
  state with the previous turn's library plist (a one-turn skew in the
  restored windows, not a failed restore).  FIXED (same day, on request):
  both temp files are written first, then both renamed; a failed plist
  rename rolls the glksave back from its -bak (roll_back()), and a failed
  write deletes the other temp.  Only a crash in the instant between the
  two renames can still split the pair.  Question's autosave
  (questionglk-autosave.mm write_autosave_pair) got the identical fix;
  its suite passes 29 ok + the known q4-timer-midcycle XFAIL.  Verified: xcodebuild scarier, run_autosave_tests.py 48/48 (the
  suite exercises the normal path only; the failure branches are
  untested).  Also:
  stale os_glk.cpp references in the .h/.mm now name os_glk_autosave.cpp,
  and the hand-sized [9] channel arrays in ScarierGlkFrontendState are
  static_assert'd against GSC_A5_MAX_CHANNELS.  (xcodebuild scarier ok.)
- Afterwards the whole front end (os_glk*.cpp + os_glk_internal.h) moved
  into glk/, like library/ and runner/: Makefile OSGLKDIR/GLKOBJECTS and
  clean, an Xcode "glk" group (path = glk), a .gitignore line for glk/*.o,
  and the prose that names the files (README, ADRIFT4_vs_ADRIFT5.md,
  test/adrift4/README.md, run_autosave_tests.py, the saffire golden's
  comment).  No #include changes: both builds already have terps/scarier
  on the include path.  Re-verified: make glkscarier, make -j8, xcodebuild
  scarier Debug+Release, the 7 transcripts identical, suite signature
  unchanged.

Still open: nothing from the review itself.

Covered: every top-level file (sections 1-7), including scdebug.cpp in
full; plus a strict-warning sweep of everything.

## 0. Cross-cutting (verified by hand)

- Strict-warning sweep (-Weverything minus noise, clang, SPATTERLIGHT defined)
  over every top-level .cpp finds only three items:
  - scgamest.cpp:251 `return;` after scr_fatal is unreachable.
  - sclocale.cpp:489 `%02lx` given a `scr_int` (long) — cast to scr_uint.
  - scparser.cpp:964 fall-through is intentional but unannotated.
- Ten `scr_fatal (...); return;` pairs across sc*.cpp: the return is dead since
  scr_fatal throws (scprotos.h:104 marks it noreturn).  Drop them.

## 1. sctafpar / sctaffil / scprops / scresour / scdump

### Defect (the one real one)
- **A1 sctafpar.cpp:1266-1294 parse_read_multiline**: `scr_owned_string
  multiline` (a unique_ptr) is live across parse_get_taf_string(), which on
  truncated data calls parse_taf_fail() (:911-919) = `scr_longjmp` back to
  parse_game (:5438).  longjmp over a non-trivial destructor is UB; in
  practice a leak on corrupt TAFs.  Confirmed by reading both sites.  Fix:
  make parse_taf_fail throw a small exception caught in parse_game, removing
  the last setjmp/longjmp in the loader (the other RAII objects at 2787,
  3488, 3608, 3652 are not live across any parse_get_taf_* call).  M / low.
- A2 sctafpar.cpp:5456-5475: the longjmp branch does not reset
  parse_use_pushback (harmless today; one-line hardening).
- A3/A4: leak-on-throw of scr_malloc'd names in parse_get_v400_resource_offset
  (:1466..1522) and res_handle_resource (scresour.cpp:157-159 → 252-253);
  scr_owned_string fixes both.  Low value (documented leak-on-fatal policy).

### Duplication
- C1 scresour.cpp:163-210 vs 213-249: sound/graphics blocks are the same ~45
  lines; one helper (bundle, partial_format, key prefix, target).  Also the
  snprintf at 180/185 and 230/235 runs twice with identical args.
- C2 sctaffil.cpp:429-517 taf_unobfuscate vs 530-572 taf_read_raw: same drain
  loop; raw = obfuscated minus 14+8 header skip and XOR.  ~40 lines.
- C3 sctaffil.cpp:865-891 vs 922-945: identical try/destroy/rethrow scaffold.
- C4 sctafpar.cpp:602-621/623-645 parse_put_property/parse_get_property and
  851-865/867-881 put_indexed_integer/boolean: same format builder.
- C5 sctafpar.cpp:109 and :1670 spell "{V400_ROOM_EXIT:...}" twice; the
  3.9/3.8 twin already has V390_V380_ROOM_EXIT_DESC (:181).
- C6 scprops.cpp:649-670/684-711/714-792: shared format check + child walk
  (only if editing the walk anyway).
- C7 scdump.cpp repeated blocks: ALR 153-189/554-570 vs SYNONYM 573-589;
  LOCKKEY 472-500 vs OPENABLE 502-527; player BATTLE 999-1031 vs NPC BATTLE
  1079-1112; restriction 762-813 vs action 815-873 prologue.  Dev-only.

### Oversized
- D1 scdump.cpp:120-1271 scr_dump_structure_once (1151 lines): linear
  sections with no shared state except `dumped`; split into dump_objloc /
  dump_tasks / dump_events / dump_npcs / dump_rooms.  Zero product risk.
- D2 sctafpar.cpp:2657-2990 parse_fixup_v380: extract "_Restrictions_"
  (2700-2767) and "_InitialPositions_" (2768-2900) → dispatcher matches
  v390/v400 fixups in shape.  S-M / none.

### Stale / dead
- B1 scprops.cpp:159-169 prop_ensure_capacity comment still describes the
  orphans array, which is now a std::vector (:92-101).
- B2 scprops.cpp:636-641 prop_get() doc block sits above prop_put_integer;
  prop_get (:714) has none.
- B3 scdump.cpp:19-27 names scdump.c / sctasks.c / scnpcs.c / harness/build.sh
  (real: test/adrift4/harness/build.sh:35); env catalogue at 30-75 omits
  SCR_DUMP_ALRS, SCR_TRACE_EVENTS, SCR_DUMP_BATTLE, SCR_TRACE_PLAYER.  Same
  "scdump.c" in sctasks.cpp:441, scnpcs.cpp:1409, scprotos.h ~1419.
  scdump.cpp also lacks `#include <string.h>` for strlen/strcmp/strstr.
- B4 empty "Module notes: o ..." placeholders in scprops/scresour/sctaffil.
- B5 sctaffil.cpp:853 unreachable `default:` naming the wrong function
  ("taf_create"); taf_sniff_tas_version returns only 400/390.
- B6 sctaffil.cpp:115,161,162 repeat 0x00a09e86 instead of PRNG_INITIAL_STATE.
- B8 sctafpar.cpp:3636-3692 tautological `assert (alr == alr_count)`.
- No dead functions, no gotos, no TODO markers, scprotos.h consistent.
- Fine: sscanf into PARSE_TEMP_LENGTH (literal inputs only); strcpy in
  parse_patch_edit guarded; parser statics reset by parse_game; overflow
  guards parse_checked_multiply/count used at every vector sizing.

## 2. scparser / scexpr / scvars

(No gotos exist; the grep hits were the ADRIFT "goto" verb in comments.)

### Dead / stale
- 1.1 scparser.cpp:4367-4392 unreachable `else` in uip_assign_pronouns
  (outer gate :4325 `<= TAF_VERSION_380`, inner :4355 repeats it).  ~26 lines.
- 1.2 scexpr.cpp:1187-1203 `#if 0` conventional PRECEDENCE_TABLE; fold its
  one-sentence rationale into the live table's comment (:1205-1208).
- 1.3 scexpr.cpp:32 <limits.h> and :36 <time.h> unused.
- 1.4 scparser.cpp:21-29 and 1.5 scvars.cpp:21-31 stale SCARE Module notes,
  contradicted by measured sections (e.g. p4STATE probe scvars.cpp:1447).
- 1.6 scexpr.cpp:73-76 says name lengths unused; expr_tokenize_start :150-169
  uses them.
- 1.7 scvars.cpp:2270-2272 "scrunner.c" → runner/scrun_dispatch.cpp:1087/1093.
- 1.8 scparser.cpp:3178-3184 + static at :3200 claim uip_match re-enters
  itself via %variable% matching; nothing in 1400-3030 calls back into
  uip_match/var_get any more.  Reword; make `cleansed` (:3200) a plain local
  (3.1) so the re-entry leak trap cannot return.
- Header hygiene: all 56 externs declared, defined, and used cross-file.

### Duplication (where var_get_system's 758 lines come from)
- 2.1 scvars.cpp:884-913 var_status_object vs :935-961
  var_marker_object_by_short: identical except the Openable==0 skip at :894.
- 2.2 scvars.cpp:1525-1532 and :1562-1569 number-to-word blocks; var_number_word
  (:1938) already does the table half.
- 2.3 scvars.cpp:1249-1263 vs :1492-1506 openness switch.
- 2.4 scvars.cpp in_/on_/onin_/state_ (1062-1110, 1267-1300, 1302-1328,
  1404-1469) share the resolve/clear-temporary/restore skeleton (~40 lines).
  Keep the asymmetry (in_/on_ scan Short first; onin_/state_ don't) as a
  parameter — it is unmeasured.
- 2.5 scvars.cpp:1205-1214 vs :1435-1445 obj_state_name copy block.
- 2.6 article ladder ×4 in scvars (461-482, 499-506, 1613-1641, 1656-1663);
  theobject arm is var_print_object_np rewritten.  Cross-file copies in
  scparser.cpp:3465 and library/sclibrar_print.cpp:726 are measured — don't
  merge across files without a measurement pass.
- 2.7 scvars.cpp:648-691 var_list_at_object vs :714-807 var_list_onin_object
  ("on" half and "in" fallback are the same body); nested ", and inside is"
  clause (764-782) is unmeasured, leave text alone.
- 2.8 scparser.cpp:1307-1329 vs :1387-1408 wildcard_match_400/pre400: same
  ~20-line preamble.
- 2.9 scparser.cpp:1804-1826 uip_match_wildcard vs :1889-1907 uip_match_text:
  identical right-sibling list loop.
- 2.10 lower-casing loop written 8× (scparser 1310, 1390, 1923, 2622, 3632,
  uip_lowered :3662; scvars 1463, 1920).  Add scr_lowercase to scutils.cpp.
- 2.11 scexpr.cpp:1333-1355 vs :1509-1531 TOK_VARIABLE factor (int/string).

### Correctness
- 3.2 scexpr.cpp: a scr_fatal thrown between expr_tokenize_start (:179 assert)
  and expr_tokenize_end (:191) leaves expr_temporary set; next expression
  in the same process trips the assert (asserts are live: no NDEBUG in any
  build).  Only multi-game processes (test harnesses) see it.  RAII guard
  like run_dispatch_input_guard.
- 3.3 scparser.cpp:526-527 and scexpr.cpp:1168-1169: scr_error prints
  expected/got swapped.  Trace-only.
- 3.4 scparser.cpp:2809-2817 three nested `for`s at the same indentation.
- Fine: %ld/scr_int formats all match; no raw ctype calls (cp1251 safe);
  buffers bounded; uip_tree_cache game-independent; entity caches cleared by
  uip_forget_game; duplicate_keys set (:2076) intentionally never cleared
  (var->name points into it) — add a comment saying so.

### Oversized
- var_get_system (:971-1729): do the de-dup first (~100 lines off); then at
  most lift the four marker arms.  No handler table (prefix arms + ordering).
- uip_match_entity (:2653-3011): worthwhile; seams = admission gate
  (~2830-2876) → uip_entity_admitted, and save-match block (2938-2967) →
  uip_record_entity_match.
- expr_eval_action (:619-1144): keep the switch, move case bodies into ~5
  helpers (pushes / numeric fns / binary+RANDOM / DIVIDE-MOD-POWER / string
  fns / CONCATENATE).  Second-order.
- uip_replace_pronouns, uip_assign_pronouns: leave.
- Small: uip_debug_dump take a tree parameter (:3263-3268); scexpr.cpp:1226
  return type scr_bool; scvars.cpp:1153-1165 two reallocs where one snprintf
  does it.

## 3. sctasks / screstrs / scevents / scnpcs / scobjcts / scbattle

### Dead
- obj_container_index, obj_surface_index, obj_stateful_index (scobjcts.cpp:141,
  :160, :194; scprotos.h:1348-1350): zero callers; removing them orphans the
  static obj_object_index (:112-125).  Delete all four.
- battle_attribute (scbattle.cpp:265, scprotos.h:1285): zero callers
  (_range/_max are the used ones).
- evt_buffer_text's `event` parameter (scevents.cpp:496-502 `(void) event`);
  4 callers pass it for nothing.  evt_taf_version(game, 0) at :1688 passes a
  dummy event -- the arg only syncs a per-game cache; give it a (game) form.
- sctasks.cpp:29 `#include <math.h>` unused.
- Stale module notes: sctasks.cpp:21-26 (jAsea-era), scevents.cpp:21-25
  ("pause/resume need more testing" -- now measured at 1103-1151), empty
  "o ..." placeholders in screstrs/scnpcs/scobjcts.
- TODO/FIXME hits in these files are all references to RUNNER_TESTS_TODO.md /
  WINE-TRANSCRIPTS-TODO.md; no real markers, no #if 0.
- task_in_dispatched_run (scprotos.h:1238) only used in sctasks.cpp:2719 ->
  static.  The push/pop pair stays (runner/scrun_match.cpp:2480/2632).

### Correctness
- sctasks.cpp:1608-1632 task_run_set_task_action: Var2 (task index from the
  TAF) is never range-checked before task_cache[task] (:234) /
  gs_set_task_done (:1632).  scevents guards the same reference (:696, 1008,
  1089, 1118, 1144).  Add the same guard; out-of-range = cannot redirect.
- screstrs.cpp:989-996 type-5 restriction: scr_trace not gated on
  restr_trace -> unconditional stderr spam every evaluation.
- sctasks.cpp:1625 getenv("SCR_TMP_NOIMM") and scevents.cpp:1323-1324
  getenv("SCR_TMP_NOTICKED"): forgotten bisect switches, referenced nowhere
  else, evaluated on every execute-task action / 4.0 event tick.  Remove or
  hoist to a static const.
- sctasks.cpp:2995-2997 recursion_depth++/-- around task_run_task_unrestricted
  is not exception-safe (static :2916 survives a scr_fatal into the next game
  in the same process).  Use the hide_guard RAII shape (:2771-2778).
- scbattle.cpp:1491, 1704, 1919: three separate `static const battle_trace =
  getenv (SCR_TRACE_BATTLE)` with raw fprintf(stderr); one file-scope static
  like npc_trace/obj_trace.
- Mis-named diagnostics: scobjcts.cpp:1086 ("scr_object_indirectly_in_room"),
  scevents.cpp:281 ("evt_can_see_event" inside _in_room).
- No buffer/format problems (no sprintf/strcpy/strcat; name[32] in
  battle_bundle_range bounded; roomlist[12] bounded by length <= 12).

### Duplication
- NPC/player selector ladder (var==0 player / 1 referenced char / else var-2)
  written 9x: screstrs.cpp:155-179, 186-191, 198-204; sctasks.cpp:709-749,
  756-780, 793-812, 1078-1083, 1198-1231, 2165-2195.  One task_selector_npc
  helper, ~80 lines.  M / low.
- sctasks.cpp:2649-2674 re-reads Repeatable/RepeatText by hand; cached
  task_is_repeatable / task_repeattext_is_empty (:265-298) exist.
- "Version" read by hand: sctasks.cpp:1921, 2012; scnpcs.cpp:65-91;
  scbattle.cpp:117-127 -> prop_get_taf_version (scprops.cpp:943).
- scevents.cpp:1103-1151 evt_pauser/resumer_task_is_complete identical bar
  property name + ES_ enum; :1161-1200 PrefTime1/PrefTime2 blocks -> loop.
- screstrs.cpp:1373, 1388, 1404 restr_get_fail_message reads Type 3x.
- sctasks.cpp:632..831 gs_set_carried_suspend bracket with 3 duplicated
  early-outs (727, 770, 803), same at scevents.cpp:349/385 -> suspend_guard
  RAII (also exception-safe).
- scbattle.cpp:593-604 vs 1351-1359 kill sequence; scnpcs.cpp:1126-1140 vs
  1371-1390 CharTask/MeetChar read; NUL constant in screstrs:42/scobjcts:37
  used once each.

### Oversized
- evt_tick_event (scevents.cpp:1212-1584): YES -- switch on gs_event_state,
  cases self-contained (WAITING 1223-1280, RUNNING 1282-1405, AWAITING
  1407-1537, PAUSED 1558-1573) -> evt_tick_waiting/running/awaiting/paused.
- evt_finish_event (:649-962): YES -- TaskAffected block 693-837 and restart
  block 839-958 -> two helpers, 80-line driver remains.
- task_move_object (sctasks.cpp:543-874): after the selector helper and
  suspend_guard, split switch(var2) into room/roomgroup vs into/onto/held/worn.
- task_run_move_npc_action (:1039-1280): cheap seam at var1 == 0.
- battle_resolve (scbattle.cpp:1434-1622): extract hit (1503-1535) / miss
  (1557-1621) narration printers.
- task_run_task_unrestricted, npc_tick_npc_walk, restr_object_in_place: leave
  (bulk is measurement commentary that must stay beside the code).

### Other
- screstrs.cpp:1576 restr_cache is untagged by game (task_cache and evt_cache
  are); safe only because runner/scrunner.cpp:1279/1417 call
  restr_cache_reset().  Tag it or add restr_forget_game to gs_destroy.
- Fine (deliberate Runner emulation, do not touch): screstrs.cpp:722-724
  player-parent test in cases 4-6; scnpcs.cpp:561-577 index_-1; NPC_WALK_EXPIRED
  and the 256-turn replay; scbattle statics rewritten in battle_start; the
  documented deliberate deviations and assist modes.

## 4. mapdraw / scmap (+ adrift5/a5map for comparison)

- Dead/over-exported: map_find (mapdraw.h:156) has no external caller → static;
  map_pt_t.z (mapdraw.h:81, written a5map.cpp:102, never read); map_page_t.label
  (mapdraw.h:128, written a5map.cpp:243 and scmap.cpp:1178 = NULL, never read);
  node_has_link_dir (:1829) duplicates find_dir_link (:1165) → `!= NULL`;
  draw_dir_icon_xy (:1578) has one caller (:1602), fold; redundant
  ensure_derived_palette in draw_out_arrow (:1561); literal 12 for MAP_N_DIRS
  at :2197 and :2384.
- Stale comments: scmap.cpp:925 "scrunner.c" (now runner/scrunner.cpp:64);
  scmap.cpp:251-253 claims tidy_up re-derives the extent — it doesn't (only
  sm_note_extent does); runner-faithful, fix the comment.
- Duplication: scmap vs a5map do NOT share an algorithm (layout vs XML
  parse); leave sm_opp vs compass_opposite (different defaults, deliberate).
  Per-direction switches replaceable by a dx/dy table: link_point
  (mapdraw.cpp:1063-1074), draw_out_arrow (:1544-1555), compass_opposite
  (:1672-1685), scmap sm_set_grid offsets (:456-467; 207-214; 279-286).
  Leave bezier_assister (:1250) and sm_opp alone.  badge_pct (:1124-1146)
  and compass_site (:1149) → const tables.  map_render pass 1 draws the same
  draw_bezier three times (:2114-2117, :2156-2159, :2165-2166) → choose
  points, one call.  blend (:443-448) re-unpacks RGB that rgb_chan (:101)
  provides.  scmap_build formats the room key twice (:1204, :1283) and
  re-parses it with atol (:1255) → node_of[rno] index (also removes the
  OOM-only NULL deref there).
- Correctness: all pixel writes go through blend (:431-454) with bounds
  checks — no OOB.  Division guarded.  Leaks: none (verified every exit).
  Statics: palette host-owned, sm_cache_* keyed by game and cleared via
  scmap_forget_game.  Overflow: bounded for v4; a5map node_int (:194-198)
  does not clamp authored X/Y (hostile v5 data only; low priority).
  Glyph fonts drop bytes >126 (UTF-8 names render as gaps) — known limit.
- Oversized: map_render (:1955-2318) worth splitting into render_links /
  render_stubs / render_nodes (+render_badges); sm_set_grid: do NOT split
  (faithful Form29 port whose stale `match` spans the body); scmap_build:
  modest split at the "--- the map ---" banner (:1159), only if touching.
- Header hygiene: struct layouts must stay public (two loaders + harness
  fill them); scmap_forget_game in scprotos.h:655 is deliberate.

## 5. scprintf / scgamest / scserial / scmemos / scinterf

### Defects (error paths only)
- scserial.cpp:1311-1364 ser_restore_object_location: local `std::vector<scr_int>
  rooms` (:1327) is live across ser_get_int/ser_reject_if (:1330-1333), which
  scr_longjmp to ser_tas_error (:1461).  Same UB/leak shape as the loader A1.
  Read into a plain count + gs accessors, or hoist to a static like
  new_game/new_vars.
- scserial.cpp:1733 scr_fatal ("unknown variable type") throws through the
  setjmp frame: statics new_game/new_vars (:1421-1422) never destroyed,
  ser_tas/ser_pre_v4/ser_raw_memo (:1469-1473) stay set for the next call.
  Make it a ser_reject_if, or try/catch that does the :1463-1475 cleanup.
  Save side: scserial.cpp:1034 throws with ser_callback/ser_opaque/
  ser_pre_v4/ser_raw_memo still set (reset only :1056-1059), ser_buffer
  allocated, and a possibly half-finished z_stream (`initialized` :187)
  surviving into the NEXT save while the game continues (scr_save_game
  wrappers return FALSE).  RAII guard + ser_flush_abort().  M / none.
- scinterf.cpp public entries WITHOUT try/catch: scr_does_command_match
  (:1024 -> parser, 6 scr_fatal sites), hint functions (:1392-1469 -> prop_get),
  debugger (:1479-1510), scr_quit_game, attribute getters/setters.  scarier.h
  is extern "C", so an escaping scr_fatal_error = std::terminate.  Programming
  -error paths only; falls out free of the wrapper template below.

### Dead
- scserial.cpp:141-147 ser_set_fast_compression + static ser_compression:
  zero callers (confirmed); memos use ser_set_raw_memo (:161-167).  Comment
  :133-140 describes the old scheme.  Fold into a constant at deflateInit
  (:255).
- scmemos.cpp:486-491 memo_clear_games: zero callers (confirmed), scprotos.h:451.
- scserial.cpp:28 <errno.h> unused.
- Empty "Module notes: o ..." in scmemos/scinterf/scserial/scgamest :21-25
  (scprintf's :21-28 are real open questions, keep).
- Static candidates: pf_buffer_paragraph (scprotos.h:356; only user is
  pf_buffer_paragraph_line, scprintf.cpp:2111).
- 18 scr_* API entries with no caller anywhere in the Spatterlight tree
  (scr_does_game_use_graphics/sounds, scr_get_capacity_assist,
  scr_get_end_keyprompt, scr_get/set_game_bold_room_names,
  scr_get_game_compile_date, scr_get_game_debugger_enabled,
  scr_get_game_max_score, scr_get/set_game_notify_score_change,
  scr_get_game_patches, scr_get_game_preferred_font,
  scr_load_game_from_filename/stream, scr_save_game,
  scr_save_game_to_filename/stream).  Inherited SCARE C API; keep or prune is
  a policy call, zero runtime cost.  Only scr_get_game_name is called from
  Objective-C (application/GlkController+GlkRequests.m).

### Stale comments
- scinterf.cpp:536-543 "scr_quit_game() is implemented as a longjmp()" -- run_quit
  (runner/scrunner.cpp:1625-1637) just clears is_running.
- scinterf.cpp:518-520 if_report_fatal "leaks ... see the RAII phase, P3" -- P3
  is done (gs_create try/catch scgamest.cpp:1874); save callers continue.
- scinterf.cpp:1388 "scr_get_game_sledgehammer_hint" -> unsubtle_hint (:1454);
  :1474 "scr_is_game_debugger_enabled" -> scr_get_game_debugger_enabled.
- scserial.cpp:1550 "encumbrance ... not currently maintained" -- saved live
  (:884-887), recomputed on load (~:1795).
- scgamest.cpp:1994 "copy NPC states individually to avoid walks problems" --
  all 12 fields are copied, so `to->npcs = from->npcs` is identical (as events
  at :1992 already do).

### Duplication (~300 lines)
- scinterf.cpp: 16 identical try/catch/if_report_fatal blocks (:431, 454,
  476, 553, 571, 589, 608, 627, 692, 720, 746, 779, 806, 833, 874, 904) ->
  one template `if_guarded (name, f, fail)`.  M / none.
- scinterf.cpp:1037-1205: 12 getters each call run_get_attributes with 12
  NULLs + one out-param; 3 setters (:1246/1260/1274) get-all + set-all.  A
  local attrs struct filled once.  ~150 lines.
- scgamest.cpp:766-880: 8 gs_object_*_unchecked movers share a 6-line body
  -> gs_object_place_unchecked (position, parent, runner_parent, detach);
  keep the checked wrappers :882-964 and the exact predicate/assign order.
- scserial.cpp:199-213 vs 221-236 ser_flush raw vs pre_v4 branches identical
  bar taf_obfuscate_buffer.
- scmemos.cpp:228-234 and 458-462 free+strdup text -> memo_set_text.
- scgamest.cpp:1300-1301 and 1644-1645 read NPCs child count twice.
- Save/load mirroring is by hand (only SER_BATTLE_SLOTS, ser_variable_*,
  ser_openness_pre_v4 shared).  Don't restructure (byte order); add paired
  "MIRROR: ser_load_game step N" comments per section.

### Oversized
- gs_populate (scgamest.cpp:1248-1849): worthwhile -- objects (1304-1574,
  itself placement switch 1348-1435 + runner_parent seed 1437-1507), tasks,
  events (1588-1641), NPCs (1643-1675), player (1677-1713), misc (1715-1768),
  seen sweep (1783-1848).  Each reads only bundle + game.  M / none.
- ser_load_game / ser_save_game_internal: split the LOADER per section (the
  error-path fixes fall out); keep longjmp discipline (statics for anything
  with a destructor) in every helper.
- gs_copy, ser_flush, pf_filter_input: leave (pf_filter_input's bulk is
  measured-behaviour commentary).

### Small
- scprintf.cpp:433-520 pf_interpolate_vars: `buffer_used` flag is redundant
  (marker == string when the loop never ran).
- scgamest.cpp:444-456 loadtime accessors lack the gs_is_game_valid assert.
- scgamest.h:21-22 includes before the guard; scinterf.cpp:76 tab indent.
- Fine: all %ld formats match; every fixed buffer bounded (value[32],
  digits[24], escape_buffer[3], sscanf targets sized strlen+1); scprintf
  ALR/synonym caches reset by pf_create; ser_cache_* dropped by
  ser_forget_game; lenient ser_get_boolean / out-of-range room return are
  deliberate Runner-save tolerance.

## 6. scutils / sclocale / scdebug / headers / sx* / build files (partial)

Not reached: scdebug.cpp:2049-2673 and its overlap with scdump.cpp; the
"defined non-static but undeclared" census list.

### scprotos.h census (878 prototypes)
- Declared-but-undefined: 0.  Used cross-file: 868.
- Own-file-only (make static, drop prototype): battle_attribute,
  evt_can_see_event (scevents.cpp:287; :579/666/1398), memo_clear_games (dead),
  obj_container/surface/stateful_index (dead), pf_buffer_paragraph, scr_rand
  (scutils.cpp:564), task_in_dispatched_run, var_put (scvars.cpp:290;
  :2008/2042 -- used, NOT dead: the agent's "dead" claim was wrong).
- runner/scrunner.h re-declares 4 scprotos.h names: uip_set_lenient_tasks,
  uip_set_task_commands, var_set_ref_character, var_set_ref_object.
- Organisation: battle_ block comment says "in scnpcs.c" (~:1281); "(scdump.c)"
  (~:1419); lib_* declarations interleaved in the run_* section (:1085-1154:
  lib_verb_object_note_line_top, lib_co_400_*, lib_battle_who_*,
  lib_with_prefix_390_*, lib_prepass_seen_3738, lib_co_ambiguity_prompt) ->
  move to library/sclibrar.h; <string>/<vector> included before the guard
  (:23-24) and <string> again at :108; :57 "SCARE never alters the signal
  mask".
- Ad hoc externs: only Glk-library symbols in os_glk.cpp (:3612-3616, 8156,
  11192).  Fine.

### scarier.h
- All 14 os_* implemented by os_glk, os_ansi and sxstubs.  Every scr_* is
  defined (scr_take_scripted_line lives in runner/scrun_dispatch.cpp:2266 --
  the agent's "undefined" claim was wrong).
- Used only by sx*/test harnesses: scr_game_from_filename,
  scr_run_game_debugger_command, scr_scarier_emulation, scr_scarier_version,
  scr_set_capacity_assist.  Plus the 18 uncalled entries listed in section 5.

### Build files / docs
- `make -n`, `make -n sx`, `make -n glkscarier`, `make -n -f Makefile.headless
  test` all resolve.  Xcode compiles everything except scdump.cpp (gated),
  os_ansi.cpp and sx* (intentional).
- Makefile:86-87 "bundled zlib headers unzipped into the Scarier root"
  contradicts :27-29 "links the system zlib".  Makefile:116-118 says
  MAPOBJECTS stay out of the ANSI build but :155 links them into it.
- Makefile.headless clean (:553-554) misses $(A5DUP_BIN), $(A5MAP_BIN),
  $(SCMAP_BIN), $(A5DUP_BIN)_san.
- sx harness: no *.scr scripts anywhere, no test script runs `sx`; only
  sxstubs/sxglob/sxutils are reused (Makefile.headless:46).  sxmain,
  sxtester, sxscript, sxfile + the `sx` target + XOBJECTS are dead weight
  (sxprotos.h needs trimming with them).  M / none.
- Rename leftovers: scarier.hdr:1 "SCARIER 1.3.10" and IFP_ENGINE_VERSION
  1.3.10 vs 1.4.0; upper-case "SCARIER" in prose (scdebug.cpp:324,
  sclocale.cpp header); README:22-23 calls sx* engine sources; README:40-43
  says default make builds Glk (Makefile:151 `all: scarier` = ANSI);
  README:54 `adrift-walkthroughs/` does not exist; ADRIFT4_vs_ADRIFT5.md
  cites a5arith.cpp (missing), autosave.h (is scarier-autosave.h), gi_blorb.h.

### scutils / sclocale / scdebug
- scutils.cpp:72 comment "calls abort()" (throws since :107); :24 module note
  stale.  SPATTERLIGHT platform/congruential rand handlers (:231-279)
  identical bar seed; scr_runner_rand (:392-462) is a deliberate separate
  xoshiro copy (autosave state) -- leave.
- Three InStr variants: lib_instr (library/sclibrar_put.cpp:2944),
  lib_instr_nocase (sclibrar_take.cpp:2788), run_instr
  (runner/scrun_split.cpp:438) -- semantics differ (word-boundary /
  substring / case); verify before unifying.
- sclocale.cpp:380-401 loc_ascii_tolower / loc_ascii_strncasecmp: no callers
  outside the file -> static if used, delete if not.  LATIN1_LOCALE
  (:193-204) and CYRILLIC_LOCALE (:211-227) repeat identical isspace/isdigit
  ranges.
- scdebug.cpp: no mutable file-scope statics (state lives in game->debugger).
  debug_help (:238-544) could be a table; low value.  sxglob.cpp:104-106
  unreachable break, cosmetic.

## 7. os_glk.cpp / os_ansi.cpp (complete; split APPLIED and scarier-autosave.mm read, round 5)

### Section map of os_glk.cpp (11269 lines, 88 file-scope statics)
| lines | topic | ~lines |
|---|---|---|
| 1-154 | header, includes, SPATTERLIGHT/GARGLK glue (non-static `gamefile[1024]` :104) | 154 |
| 155-515 | module variables (windows, streams, colour globals, gsc_game/path/key, a5 state, all gsc_map_*, walk state) | 360 |
| 516-690 | port utilities (gsc_put_literal, gsc_fatal, gsc_malloc/realloc, window open) | 175 |
| 691-1381 | locale tables (cp1252/cp850/cp1251/cp866) + conversion, put_char/put_string, gsc_read_line | 690 |
| 1382-1875 | status line, gsc_status_notify/redraw | 495 |
| 1876-3573 | output: tags, fonts, colour lookup/detect/apply, symbol-font tables (2538-2795), hints | 1700 |
| 3574-3943 | resources: sound, title window, gsc_refresh_windows, os_show_graphic x4 | 370 |
| 3944-5866 | command escapes: GSC_COMMAND_TABLE (5095-5163), help prose (5332-5755), gsc_command_escape | 1920 |
| 5867-6289 | input: abbreviations, os_read_line(_debug), gsc_get_choice_key, os_confirm | 420 |
| 6290-6410 | events: gsc_short_delay, gsc_event_wait(_2) incl. sc map-click walk | 120 |
| 6411-6487 | os_open/read/write/close_file | 75 |
| 6488-7286 | main/options: known-game assist tables (6619-6695), gsc_hash_game_stream, gsc_startup_code (6809), gsc_main (7041), linkage flags | 800 |
| 7287-8107 | ADRIFT 5 driver 1: put_string/prompt, real time, await_line (a5 map-click walk :7487), read_line, popups, command escape, save/restore | 820 |
| 8108-8197 | ADRIFT 5 graphics + sound (gsc_a5_channels[9]) | 90 |
| 8198-8674 | SPATTERLIGHT autosave/autorestore stash/recover | 480 |
| 8675-9420 | a5 display, span styles, side window, colour stack, media, undo_look, cover, a5 status | 745 |
| 9421-10478 | map window (both engines), name cache, walk_next, prefs, show/hide/zoom | 1060 |
| 10479-10953 | a5 undo/restore, intro, restart_run, meta_perform, gsc_a5_main (10625) | 475 |
| 10954-11269 | glk_main, UNIX linkage (`#ifdef TRUE` :10979), WinGlk linkage | 315 |

### Split proposal (mirrors the library split)
`os_glk_internal.h`: feature macros, TRUE/FALSE, Glk includes, the typedefs
(gsc_status_writer_t, gsc_meta_t/GSC_META_*, GSC_COLOUR_NONE,
GSC_MAX_STYLE_NESTING, GSC_CONF_*), externs for ~45 of the 88 statics
(windows, streams, colour globals, gsc_game/path/key, a5 state, gsc_map_*,
walk arrays, title/sound/channel state, gsc_meta_pending, gsc_autorestored,
gsc_in_debug_read, gsc_patches_enabled), ~60 prototypes.  Locale,
abbreviation, command, assist and symbol tables, font stack, help flags,
autosave magic, map name cache stay private.

TUs: os_glk_core (vars + utilities + events + files + flags, ~950),
os_glk_locale (~690), os_glk_status (~495), os_glk_output (~1450) +
os_glk_symbols (~260 pure data), os_glk_resources (~370), os_glk_commands
(~1500) + os_glk_help (~425, or a `help` field per table row),
os_glk_input (~420), os_glk_main (options + glk_main + linkage, ~1100),
os_glk_a5 (~2040; optional os_glk_a5_media ~320), os_glk_autosave (~480,
whole file SPATTERLIGHT-only), os_glk_map (~1060; move the map globals
here -- only autosave recover :8572-8597 and status_redraw touch them from
outside).  The map is the coupling knot (status_redraw, os_read_line, both
event loops, colour_rebuild_windows, autosave recover) -- extract it first.
L / none (mechanical moves).

### Dead code
- LINUX_GRAPHICS blocks os_glk.cpp:3901-3931 (dd/xv sprintf hack),
  :10999-11002, :11070-11075, :11146-11149; os_ansi.cpp:273-286.  No
  definer anywhere in terps/, glkimp or the .xcodeproj.  Also the file's
  only sprintf into a buffer.  S / none.
- Non-Unicode fallback stubs :977-1004 (static glk_put_char_uni,
  glk_request_line_event_uni, gestalt_Unicode=15): both cheapglk/glk.h:26
  and glkimp/glk.h:26 define GLK_MODULE_UNICODE, so the #else never
  compiles here; gsc_has_unicode (:6854) is always TRUE.  Keep only for a
  hypothetical non-Unicode Glk.
- Unreachable guard :10679-10683 "No ADRIFT 5 game loaded." (glk_main
  :10969 only calls gsc_a5_main when gsc_is_a5, set solely at :6931 under
  `if (gsc_a5_adv)`).
- `#ifdef TRUE` :10979 ... `#endif /* __unix */` :11173 -- TRUE is always
  a macro (:150-153); the guard is a no-op and the comment is wrong.
- Dead HERE but deliberate portability (decide, don't just delete):
  GARGLK / GLK_MODULE_GARGLK_FILE_RESOURCES blocks (:99-118, 3577-3608,
  3843-3866, 6952-6964, 7029-7035, 11034-11038, 11151-11167, 11179-11181),
  WinGlk linkage :11183-11269.  `char gamefile[1024]` :104 can be static
  for free.  Xglk repaint hack :1845-1857 is historical, harmless.
- Checked and NOT dead: gsc_get_capacity (function-pointer table entry
  :6688), gsc_header_string (:7087), gsc_realloc (:8083), gsc_put_buffer
  (:6043); every static function with <=3 references was checked.

### Duplication (sc/a5 pairs)
- Map-click-to-walk: gsc_event_wait_2 :6348-6377 vs gsc_a5_await_line
  :7487-7514 -> one `gsc_map_click (event)` returning whether a walk
  started.  M / low.
- "Echo a line in input style" idiom x5: :6017-6024, :6042-6047,
  :7622-7628, :7706-7709, :7735-7741.
- Readlog replay: os_read_line :6032-6059 vs gsc_a5_read_line_raw
  :7610-7634.
- Startup-in-colour block gsc_main :7070-7080 == gsc_a5_main :10645-10655;
  autorestore-failure block :7152-7159 == :10732-10740.
- Undo-failure wording :10600-10602 vs :10920-10923; endgame banner loop
  :10806-10851 re-implements the GSC_META_* dispatch of gsc_a5_meta_perform
  :10586-10614.
- gsc_command_is_action's handler list and gsc_command_summary's is_log
  list duplicate GSC_COMMAND_TABLE knowledge; gsc_command_help :5332-5755
  (423 lines of if/else prose) -> a `help` string per row.  Removes two
  lists that must be kept in sync.  M / low.
- os_show_graphic x4, os_play/stop_sound x3: inherent to the #if ladder,
  collapses if the dead platform branches go.

### Correctness
- OPEN: gsc_startup_code :6915-6925 mallocs `file_buf` for
  a5model_load_buffer; a5model.cpp:1159-1161 frees on NULL/0 input, but
  not every failure return from :1148 on was checked for freeing it.  If
  one leaks, every ADRIFT <=4 load leaks one whole-file buffer once per
  process.  Two-minute check.
- Keep the `run = NULL` alias-clear order at :10943-10947 (resize redraws
  during teardown) if moving code.
- Looks wrong but is fine: `sscanf ("size=+%lu", &scr_uint)` (scr_uint is
  unsigned long); sound_channel defined twice under exclusive #if
  (:3579/:3634); a5 map name pointer-identity cache cleared each redraw
  (:9674); gsc_hash_game_stream and gsc_a5_restore doubling loops
  terminate; os_read_line buffer[characters] in bounds (:6065-6067);
  colour token[32]/american[16] guarded; exit(0) after win_reset on bad
  autosave is intentional.
- Only literal TODO in the three files: os_glk.cpp:1181 "Using unicode
  output currently disrupts transcript output" (gsc_put_char_uni detaches
  and reattaches the transcript stream).  Not re-tested.

### Simplifications
- gsc_a5_display :8859-8868 15-way `*p != A5_*_MARK` chain -> `strchr
  (A5_MARKS, *p)` or a lookup.  S / none.

### os_ansi.cpp
Compiles clean with -Wall -Wextra; implements all 14 os_* functions
(scarier.h:69-85), as does os_glk.cpp.  Default `make` links it (ANSI
`scarier`); not in the pbxproj.  It is the headless dev/test player
carrying SCR_WRAP_WIDTH / SCR_ECHO_INPUT / SCR_MARK_* / SCR_SKIP_WAITKEY /
'#'-comment aids the golden harness depends on -- keep.  Only its
LINUX_GRAPHICS block is dead.

## Suggested order
1. Real fixes: A1 (loader longjmp → exception) and its twin in
   ser_restore_object_location; scserial fatal-through-setjmp cleanup (save
   and load); unchecked task Var2 in
   task_run_set_task_action; ungated scr_trace in screstrs type 5; the two
   SCR_TMP_* getenvs; recursion_depth RAII; then the 10 dead returns + 3
   warnings.
2. Dead code: 4 obj_*_index/battle_attribute functions, evt_buffer_text
   parameter, unused includes, unreachable else in uip_assign_pronouns, #if 0
   precedence table, map_find static + 2 write-only fields.
3. De-dup: NPC selector ladder (9x), scvars marker/article/openness/number
   blocks, scr_lowercase helper (8x), sound/graphics block in scresour,
   taf_unobfuscate/read_raw, parse_put/get format builders.
4. Stale comments (all "scrunner.c"/"scdump.c" references, module notes).
5. Structural splits: evt_tick_event, evt_finish_event, map_render,
   uip_match_entity, scdump sections, parse_fixup_v380, expr_eval_action,
   task_move_object (after the de-dup).
6. scinterf wrapper template + attrs struct; gs_object_place_unchecked;
   move lib_* prototypes out of scprotos.h; drop the 4 dead sx files.
7. os_glk.cpp: drop LINUX_GRAPHICS / non-Unicode stubs / `#ifdef TRUE` /
   unreachable a5 guard; de-dup the five sc/a5 pairs; command table
   fields; then the 12-TU split, map pane first.  Finish scdebug.cpp
   2049-2673 and the a5model_load_buffer ownership check.
