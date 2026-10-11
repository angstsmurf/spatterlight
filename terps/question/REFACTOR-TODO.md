# Question: refactoring / cleanup / dedup TODO

Survey of `terps/question` (Quest 1-4 engine `question-*.cc`, Quest 5 engine
`quest5/aslx*.cc`, the two Glk frontends). Written 2026-10-11, worked through
2026-10-12. Every item is either done (`[x]`) or looked at and declined
(`[-]`, with the reason); nothing is left open.

Safety net used for every item: `make -C test check` (syntax of every source
with `-Wshadow`, unit tests, Quest 5 loader/runtime tests), the Quest 4
walkthrough sweep (150/150), the Quest 5 native replays (348 pass, the same 6
failing as before the work started), and
`python3 test/autosave/run_autosave_tests.py --build`, which is the only step
that compiles the `#ifdef SPATTERLIGHT` code for real.

A new source file has to be registered by hand:

- Quest 4 engine `.cc`: `Spatterlight.xcodeproj`, the unity include list in
  `test/quest4/harness/headless_common.hh`, and
  `SpatterlightTests/QuestionRegressionTests.mm`.
- Quest 5 `.cc`: `Spatterlight.xcodeproj` and `test/Makefile` (`ASLXGLKSRC`
  for a frontend file).

Sizes now: questionglk.cc 1818, readfile.cc 1712, question-verbs.cc 1335,
question-rooms.cc 1284, quest5/aslxglk.cc 1138 (was 4984),
quest5/aslxglk-input.cc 1135, question-parse.cc 1071.

---

## 1. Split the monster functions

- [x] **run_script** (was ~1716 lines): a table of 51 `exec_<name>` handlers.
      `question-script.cc` keeps tokenising, dispatch and `eval_cond`;
      the handlers live in `question-script-flow.cc` (if/for/repeat/do/exec/
      select/choose), `question-script-world.cc` (create/clone/destroy/move/
      give/set/property) and `question-script-output.cc` (msg, sounds, the
      deliberately ignored font/pane statements).
- [x] **try_match** (was ~1240 lines): an ordered list of `try_<verb>`
      handlers in `question-verbs.cc`; the order is Quest's `ExecCommand`
      order and is noted where the list is.
- [x] `run_function`: split into `game_function` and `text_function`.
- [-] ...into a table of ~25 one-function-per-name methods: tried on paper,
      set aside. Most bodies are two or three lines, so the table would be
      mostly declarations in `question-impl.hh`.
- [x] `set_game`: `seed_random_for_new_game`, `read_version_and_options`,
      `init_variables`, `read_game_directives`, `run_startscripts`,
      `enter_start_room`. `regen_var_dirs`: `regen_doorways_dirs`, `_out`,
      `_places`, `_folded`.
- [x] `look` (was 210 lines): `run_room_description`,
      `print_room_location_line`, `print_room_objects_and_exits`.
- [-] The other 150-210 line functions were read and left alone:
      `eval_cond`, `eval_string_body`, `game_function`, `text_function`,
      `static_eval` and `try_builtin_verb` are flat keyword dispatchers whose
      branches are short and whose length is mostly the comments citing the
      Quest source; `read_block_line`, `ensure_cached`, `print_formatted`,
      `object_verbs`, `get_places`, `try_take`, `try_use` each walk one
      piece of state start to finish, and a cut would only pass that state
      around; the `QuestionState` constructor is one loop over the blocks.

## 2. `readfile.cc`

- [x] `preprocess` is its passes: `collect_addtos`, `splice_addtos`,
      `convert_friendly_ifs`, `strip_comments`, `deinline_braces`,
      `trim_and_join_elses`. `read_into`'s per-line rewrite is
      `QuestionFile::read_block_line`. The two open TODO comments are
      answered in place.
- [x] The built-in libraries are `.cc` files (`standardlib_builtin.cc`,
      `stdverbs_builtin.cc`, `typelib_builtin.cc`, `q3ext_builtin.cc`)
      declared in `builtin_libs.hh`.
- [x] Shared helpers: see §5.

## 3. De-duplicate the two Glk frontends

- [x] UTF-8 encoding of a Glk buffer: `questglk::utf8_append` and
      `questglk::utf8_from_uni`.
- [x] Resource registration: `questglk::register_path_resource (path, sound,
      offset, length, verify)` carries the "does Glk really know it" probe;
      Quest 5's `load_media_resource` calls it.
- [x] `cap_first`: the character form now capitalises through
      `aslx::utf8_case`, like the pane labels.
- [x] Cancel / re-arm of the live line request: `questglk::cancel_line_request`;
      Quest 4 wraps it in `CommandLine::cancel/request`, Quest 5 in
      `read_line`'s `cancel_line`/`request_line`.
- [-] Metaverb glue dispatcher: each handler is about five lines, and what is
      left after the shared matcher and the shared print is the
      frontend-specific part (which window echoes, which pane refreshes). A
      callbacks struct would be longer than the code it replaced.
- [x] One autosave format for both engines (first declined because it
      breaks existing autosaves; done once that was accepted).
      `questglk-common` has the codec (`blob_num`, `blob_str`, `BlobReader`)
      and `AutosaveRecord`: autosave.glksave is engine state, undo history
      and the replay record for either engine, under the stamp
      `QUESTION-AUTOSAVE 1`. autosave.plist carries one blob per frontend
      under the key `question_frontend_state`; each frontend still lays out
      its own fields (`QUESTIONGLK-AUTOSAVE 1`, `ASLXGLK-AUTOSAVE 8`).
      `questionglk-autosave.mm` has one write/read/restore-library path
      where it had two. Gone with it: the `GEASAUTO1` container, the
      `geas_*` NSCoder keys, Quest 5's `ASLXGLK-BOOT` sentinel, and every
      branch that read a pre-keypress replay record. An autosave in an old
      format is discarded and the game starts fresh (`q4-old-format`,
      `q5-old-format` in the autosave suite; the record itself is unit-tested
      in `aslxglk_link_tests`).

## 4. Split `quest5/aslxglk.cc`

- [x] One anonymous namespace became `namespace aslxglk` over
      `aslxglk-internal.hh` (types, prototypes, every global as a C++17
      `inline` variable) and `aslxglk.cc` (core, host hooks, session loop,
      entry points), `aslxglk-output.cc`, `aslxglk-panes.cc`,
      `aslxglk-input.cc` (input, character dialog, save/restore, metaverbs),
      `aslxglk-media.cc`, `aslxglk-autosave.cc`.
      `test/quest5/harness/aslxglk_link_tests.cc` includes the internal
      header instead of the source file.
- [x] Long functions: `run_session` 454 -> 190 (`boot_from_autosave`,
      `answer_pending_menu/_question/_wait`, `run_parser_prompt`; the
      `goto turn_done` is gone), `read_line` 250 -> 171
      (`transcript_link_clicked`), `install_host_hooks` 228 -> 127
      (`provide_menu`, `provide_answer`, `provide_input`,
      `retire_command_links`, `queue_js_event`).
- [x] `questionglk.cc`: `run_turn_loop` (`CommandLine`, `timer_at_prompt`,
      `objwin_link_clicked`), `glk_main` (`open_secondary_windows`,
      `boot_game`, `set_story_title`), `update_objwin`
      (`draw_objwin_sections`, `echo_objlist_to_transcript`). Nothing there
      is over 120 lines now.
- [-] Folding the `g_*` globals into structs: the names are already grouped
      by prefix; `reset_frontend_state` is selective on purpose (some state
      has to outlive a session), so a struct would not turn it into one
      assignment; the autosave codec names every field anyway; and it is a
      ~1500-reference rename with the shadow trap attached.
- [-] `questionglk-objwin.cc`: the side pane is ~280 lines sharing about 16
      file-statics with the rest of the file, so moving it needs an internal
      header exposing most of the file's state to relocate 15% of it. The
      function-level split above was done instead.

## 5. Shared string helpers

- [x] `qstr.hh`, header-only, no dependency on either engine: `is_space`,
      `lower_char`, `trim`, `ascii_lower`, `starts_with`, `ends_with`,
      `iequal`, `ifind`. Both engines and both frontends use it; the
      per-file copies are gone.
- [-] Kept apart because they do something different: readfile's
      `trim (s, trim_modes)`, `trim_braces`, `lcase`, `ci_equal` (istring),
      `starts_with_word`/`_phrase`, and the two line splitters
      (`split_lines` in readfile, `split_into_lines` in aslx.cc, which
      trims every line and drops the empty ones).
- [x] `make syntax` stays the gate, and now runs with `-Wshadow`.

## 6. Hygiene / small fixes

- [x] Stale `file:line` references in comments name the function instead.
- [x] `-Wshadow` is clean across every source and `SYNTAXFLAGS` uses it in
      place of `-w`.
- [x] `g_replaying_boot` no longer warns in a non-`SPATTERLIGHT` build.
- [x] The two open readfile TODO comments are answered.
- [x] The "font and panes are TODO" comment says they are ignored on purpose
      (now on `exec_ignored` in `question-script-output.cc`).
- [x] Role comments at the top of `QuestionRunner.hh` and `question-impl.hh`.

## 7. Considered, not worth it

- Merging the two engines' parsers (`question-parse.cc` and
  `quest5/aslx-runtime-parse.cc`): different languages; share helpers (§5),
  not structure.
- Touching `quest5/aslx-core/**/*.aslx`: vendored Quest 5 core and language
  files, keep byte-for-byte with upstream.
- Decomposing `question_implementation` into composed sub-objects: large
  churn in a golden-tested engine; the per-verb / per-statement extraction
  in §1 gets most of the readability win without moving state.
