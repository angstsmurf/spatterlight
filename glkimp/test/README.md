# Testing glkimp

glkimp is the Glk library inside every interpreter process. It talks to the
app over a pipe (`connect.c`, `protocol.h`) and archives its object lists for
autosave (`autorestore/Temp*.m`, `autosavefiles.m`). Every terp links it, so
run all of this before and after a change:

```
glkimp/test/run_stream_test.sh               # stream.c under ASan+UBSan, a few seconds
python3 glkimp/test/run_autosave_tests.py --build   # Bocfel + Glulxe autosave/autorestore
(cd terps/question/test && python3 autosave/run_autosave_tests.py)
(cd terps/scarier/test && python3 adrift4/harness/run_autosave_tests.py)
xcodebuild -project Spatterlight.xcodeproj -scheme Spatterlight -configuration Debug CLANG_ENABLE_EXPLICIT_MODULES=NO build
```

- `run_stream_test.sh` compiles `stream_test.c` with `stream.c` and
  `cgunicod.c` (window, blorb and dispatch calls are stubbed) and diffs
  its output against `stream_test.expected`. It covers memory, file and
  resource streams in all three encodings (narrow, Unicode text = UTF-8,
  Unicode binary = big-endian 32-bit) through `get_char`, `get_buffer` and
  `get_line` at several buffer sizes, writes in every file mode, seeks,
  positions, memory overflow and the null stream. `--bless` rewrites the
  expected file; only bless a diff you can explain.
- `run_autosave_tests.py`: `glulxe/undo-across-relaunch` is an expected
  XFAIL. `--build` builds the bocfel and glulxe targets into `build/`; if
  another build left `build/` with one architecture only, the x86_64 link
  of bocfel fails on `_woz2nib` (rebuild the `readwoz` target).
- The Question suite's `q5-intro-menu`, `q5-intro-ask` and `q5-intro-wait`
  fail in the control run with stock glkimp as well (2026-10-19); that is
  not a glkimp regression.
- `run_bocfel_input_tests.py`: Bocfel line/char input through the fake app.

## Deliberate deviations from the Glk spec

- `glk_request_char_event` and `glk_request_line_event` (narrow) accept a
  new request while one is pending, and `glk_put_char` prints into a window
  with a pending line request. Adrian Mole and Dead Cities rely on both
  (commit 95bbadfe1). The `_uni` variants and `put_buffer` do refuse.
- `glk_stream_close` on a window stream warns and refuses.
- `glk_stream_open_resource(_uni)` returns NULL without a warning when there
  is no blorb, no such resource or an unknown chunk type: games probe.
- The gestalt version is 0.7.6 (IMAGE2 is in), although glk.h's header
  comment still says 0.7.5.

## Behaviour that changed in the 2026-10-19 pass

- Unicode *text* file streams report and take byte offsets in
  `get_position`/`set_position` (only binary Unicode streams use 4-byte
  units). Before, a get/set round trip lost the position.
- A narrow `get_line` on a resource stream no longer shortens the buffer by
  one extra character.
- Key, mouse and hyperlink events for a window that no longer exists are
  dropped (as line events were) instead of crashing.
- The work, autosave and temp directories are correct under a home folder
  with non-ASCII characters.
- Autosave with no windows open, a failed blorb load in a resource stream,
  and a pending hyperlink request all survive autorestore.
- Diagnostics that used to go to stderr in release builds use
  `gli_debug_log()`, which compiles out unless `DEBUG` is defined.
  Protocol errors and the sanity checks still print.

## Rules

- Do not renumber `protocol.h` messages or the `messagenames.c` table: the
  app mirrors them. `cgunigen.c` is generated.
- Never rename an autosave plist key. Bump `AUTOSAVE_SERIAL_VERSION`
  (glkimp.h), which the app's GUI autosave also uses, so both are discarded.
- The Temp* classes adopt NSSecureCoding for the typed decode calls, but the
  archive is read with `requiresSecureCoding = NO` because the terps' extra
  archive hooks store arbitrary objects.
- `gi_dispa.c`, `gi_blorb.c`, `gi_debug.c`, `cgdate.c` and `cgunicod.c` are
  stock Plotkin/cheapglk code; diff against upstream before changing them.
  `gi_debug.c` compiles to nothing without `GIDEBUG_LIBRARY_SUPPORT` but is
  in the pbxproj and the headless Makefiles, so it stays.

## Looked at and left alone

- `gli_story_name` (Hugo reads it), `gli_program_info` and `gli_debugger`
  (write-only, but part of the cheapglk interface that
  `garglk_set_program_info` and the GIDEBUG build expect).
- The list boilerplate (`new`/`delete`/`iterate`/`for_tag`/`replace_*_list`)
  for windows, streams, filerefs and channels: all four are correct, a
  macro set would not pay for itself.
- `connect.c`'s `win_*` wrappers are already one `sendmsg()` call each.
- Splitting `stream.c`, `connect.c`, `glkimp.h` and `fileref.m` into more
  files: pbxproj plus several headless Makefiles to update for little gain.
- Unverified: whether an astral character survives print -> line input ->
  echo stream the same way (`win_print` sends surrogates,
  `gli_stream_echo_line` goes through Latin-1 `gli_put_buffer`,
  `store_line_input` decodes UTF-16).
