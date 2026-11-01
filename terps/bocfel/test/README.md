# Testing terps/bocfel/z6

The z6/ code is almost all window choreography, image compositing and
patches to the four Infocom V6 games, so what a change to it must leave
alone is the stream of Glk messages a session produces and the addresses
the pattern scanner hooks in every story revision. Neither suite keeps
goldens: take a baseline before a change, compare after. Each script's
docstring has the options.

- `run_v6_traces.py --out DIR [--compare DIR] [PATTERN...]` drives the
  Spatterlight build of bocfel (`build/Debug`) through 540 scripted
  sessions: the four walkthroughs in `SpatterlightTests/Supporting Files/`,
  hint / define / resize / graphics-switch / mouse scripts, every graphics
  type and interpreter number, and every release in
  `~/Downloads/{ZorkZeroes,Arthurs,Journeys,Shoguns}`. Each case's trace
  (`v6trace.py`: one line per Glk message, pictures named by a hash of the
  file) goes to `DIR/<case>.trace`, the interpreter's stderr to
  `DIR/<case>.err`, the pictures to `DIR/pictures`. A case whose story,
  graphics files or saved games are missing is skipped; only the "tracked"
  cases, which run the `.zblorb` files of SpatterlightTests, need nothing
  from outside the repository.
- The twelve `zork0-mouse-*` cases restore a saved game inside each Zork
  Zero minigame at three window sizes and click the first pixel, the last
  pixel and the pixel just outside every hit box. They need the saved games
  in the directory named by `SAVES` in the script.
- `--compare` prints the first differing line of every changed trace and
  then replays both runs into screens (`v6screens.py`, graphics windows
  only): `SAME SCREENS` is a change in how the pictures are sent that
  nobody could see, `SHIFTED PIXELS` a picture scaled a fraction of a pixel
  differently, `SCREENS DIFFER` a change to look at, with old | new |
  difference montages in `DIR/montages`.
- `v6_epdump.py --out DIR [--compare DIR]` runs the headless build
  (`make -f Makefile.headless` in `terps/bocfel`, with `BOCFEL_EP_DUMP`
  set) on 64 story files and dumps every entrypoint it hooks, the per-game
  aggregates the finders fill in and a checksum of the patched story.
  `--compare` prints only the first differing line per story; diff the
  files for the rest.

Not covered by either: the Apple II disk image extractor, `writetiff.cpp`
and `mkdemo.py` (the story-to-demo converter, in this folder). Those were
checked with standalone builds, old output
against new, on the files at hand.

`run_v6_traces.py` also runs seven non-Infocom V6 stories from
`~/Downloads/NonInfocomV6` (`advent`, `mystad01`, `mystad05`, `mysterious01`,
`moments`, `sunburst`, `wyrmward`), each as `name`, `name-autorestore` and
`name-small`. These are the only cases that exercise `v6_panels.cpp` and
`mysterious_canvas.cpp`. The folder is local-only, and the cases are skipped
when it is missing.

## Bugs the suites caught

Found during the refactor passes of October 2026, listed so that the kind of
thing the traces show is on record:

- Journey: text entry as the first thing after an autorestore dereferenced
  a null text buffer window.
- Arthur: starting with interpreter number 2 (Apple IIe), 9 or 10 crashed
  in the front graphics window setup (screen.cpp).
- Journey: after an autorestore the cached screen size in characters was 0,
  so every cursor move was clamped to (-1, -1) until the next resize
  ("moving cursor out of bounds", 1509 times over the suite).
  `journey_move_cursor` now asks Glk for the size when it has none.
- Zork Zero: `within()` shifted the hit boxes of the minigame buttons,
  cards and pegs right and down by the image scale.
- Define menu (v6_shared): a click on the blank separator row selected a
  non-entry.
- Zork Zero (the `zork0-mouse-*` cases): the Double Fanucci command grid
  took one mouse click and then no more, as the mouse request was made once
  at setup and Glk mouse requests are one-shot; a play could not be picked
  with the mouse (it takes two clicks). `pick_play()` now requests before
  every read.
- Zork Zero (shown up by the fix above in `zork0-walk-sessions`): after an
  autorestore into a game of Double Fanucci the command grid window still
  existed, but the static pointer to it was gone, so the highlighted play
  was never drawn ("window_move_cursor: invalid ref", 35 times in that
  case), clicks were not asked for, and the window was never deleted
  afterwards. The grid now has a rock of its own and
  `z0_update_after_autorestore` finds it again
  (`restore_fanucci_command_grid`). An autosave made before this change has
  the grid with rock 0 and behaves as before.
- Zork Zero (same cases): the Snarfem click hook's answer was thrown away.
  Peggleboz and Tower patch a return after their click hooks, Snarfem did
  not, so the game's own SN-CLICK ran afterwards and its answer (inclusive
  edges, about a pixel off) was the one used. `find_zork0_globals` now
  patches `RET L02` there too.
- Define menu: moving the selection with the arrow keys sent the deselected
  line to the VoiceOver menu handler again, which appended it to its item
  list every time (harmless, as the list is read by index, but it grew
  without bound). Not sent any more; a mouse click already did not send it.
  Not tested with VoiceOver on.
- Entrypoint table (2026-10-17, `v6_epdump.py`): several rows hooked a
  routine a second time at an address that is never reached, as hooks fire
  on instruction fetch: an anchor inside another instruction ("V-MODE alt"
  in every Zork Zero revision, "CENTER-1 alt" in the releases) or a second
  stub behind the one at the routine's entry (Journey's "WCENTER older",
  Shogun's DISPLAY-BORDER and UPDATE-STATUS-LINE). `find_entrypoints()` now
  skips the remaining rows of a routine once one row has claimed it, so each
  hook function is hooked in one place, and the dump shows exactly those
  rows gone.
- Z-string conversion (2026-10-24, by reading): `print_long_zstr_to_cstr()`
  left the buffer untouched for a string shorter than two bytes, so callers
  with a stack buffer (Journey's WCENTER, Shogun's menu messages) went on
  to read uninitialised memory. It now writes an empty string.
- Entrypoint finders (2026-10-24, `v6_epdump.py`): each `find_*_globals()`
  walked the combined table of all four games, so a row of another game
  with the same hook function also got an address: Zork Zero's "after
  V-CREDITS alt" pattern matched inside Shogun and took the hook from
  Shogun's own row (the same function, so no change in behaviour). With
  one table per game the dump now labels Shogun's own row as hooked, and the
  other games' rows are gone from it; the aggregates and memory checksums
  did not change.
- Headers (2026-10-24): the game headers and `v6_shared.hpp` included the
  C++ header `spatterlight-autosave.h` inside `extern "C"`, which compiled
  only because every includer had already pulled in `<optional>`. Every
  z6 header now compiles on its own.

## Code layout

- `entrypoints.cpp` is the engine (pattern search, the claim rule, the
  lookup index, the dump); each game's table and globals finder is in
  `entrypoints_{arthur,journey,shogun,zorkzero}.cpp`, with the shared
  declarations in `entrypoints_internal.hpp`. A row's hook function is
  its identity: the finders and the claim rule compare `fn`, which is why
  rows that only locate an address point at an empty dummy function.
- The Zork Zero mini-games (Double Fanucci, Snarfem, Peggleboz, Tower of
  Bozbar) are in `zorkzero_minigames.cpp`; `zorkzero_internal.hpp` holds the
  few helpers the two files share.
- The Definitions and Hints screens are in `v6_definitions.cpp` and
  `v6_hints.cpp`; `v6_shared.cpp` keeps the rest of the shared UI and the
  per-game hook tables. `v6_game()` returns the current game's
  `V6GameHooks` (stash/recover state, after restore/autorestore/restart,
  resize, the autorestore read_char hack), so `screen.cpp` makes one call
  instead of an `is_spatterlight_*` chain. The chains left in
  `v6_redraw_game_screen()` and `after_V_COLOR()` deliberately leave
  Journey out.
- `v6_windows.cpp` has the V6 window primitives (`v6_sizewin`,
  `v6_define_window`, `v6_remap_win`, `v6_apply_interpreter_number`).
  `v6_panels.cpp` has the band layout of the non-Infocom V6 games, and
  `v6_is_generic()` is the one test for those games. `mysterious_canvas.cpp`
  holds the 2011 Mysterious Adventures picture runs. Its painter
  (`mysterious_paint`, `redraw_mysterious`) stays in `screen.cpp` because it
  needs `GraphicsWindow`, as do `v6_grid_panel_put_char` and `v6_clear_panel`
  (static output helpers) and the held-newline code in `put_char_base`.
- `V6GameHooks::display_picture` is the Spatterlight half of `@draw_picture`.
  A `false` return falls through to the upstream code in `zdraw_picture`.
- The upstream `hack == Hack::*` paths in `screen.cpp` (`draw_arthur`,
  `draw_journey`, `GraphicsWindow`, ...) look dead but are not:
  `create_graphicswin()` sets `hack` for the Infocom games and `zdraw_picture`
  falls through to them. `journey_window` is live too; `init_screen` and
  autorestore assign it. Only regions the preprocessor proves absent from
  both the app and the headless build were deleted.
- Any `winid_t` kept outside `windows[]` is lost at autorestore and has to
  be found again: by tag through the autosave (`stored_bufferwin`,
  `z0_right_status_window`) or by rock (`v6_grid_window_with_rock()`, for
  the opaque status window and the Fanucci command grid).

Header rules from the November 2026 hygiene pass:

- `screen.h` no longer includes `v6_image.h`, and `v6_shared.hpp` no longer
  includes `v6_windows.hpp`, so a file using those names includes them itself.
- Constants used by one file live in that file. `zorkzero_internal.hpp` holds
  what `zorkzero.cpp` and `zorkzero_minigames.cpp` share.
- The per-game headers are sectioned the same way: entrypoint table rows,
  `V6GameHooks`, calls from other files, state other files read. `v6_shared.hpp`
  is sectioned by the file that defines each name.
- `make -f Makefile.headless check-headers` compiles every z6 header on its own
  with both sets of flags. `v6_panels.hpp` and `v6_windows.hpp` are app-only.
- `Makefile.headless` now tracks header dependencies, so a stale
  `headless_v6_stubs.o` no longer needs deleting by hand.

Declined in the October 2026 passes; do not propose again without new
evidence: rewriting the pattern tables as an `op()` builder or replacing
the dummy hook functions with ids (a typo in a byte pattern silently
disables a hook, and `fn` is the row identity); per-row accept or validate
predicates (the one Zork Zero rejection stays in `find_entrypoints()`);
splitting the long finders and the long UI functions (`V_DEFINE`,
`DO_HINTS`, `z0_UPDATE_STATUS_LINE`, `DISPLAY_BORDER`, ...); per-game UI
state structs and a shared-globals struct; `.h` to `.hpp` renames; an enum
for the `is_spatterlight_*` flags (`is_game()` exists); new autosave fields
(existing autosaves would break); a `screenmode` setter; a `v6_log()` /
`warn_missing()` layer over the stderr messages; per-game `dump_state()`
(debug only); splitting `shogun.cpp`, `journey.cpp` or `draw_image.cpp`
further; a window-slot enum (the slot numbers are the games' own window
numbers, reused on purpose); a `send_menu_item()` wrapper (the
`win_menuitem()` arguments mean different things per message type); a
shared autorestore recolouring helper (Zork Zero has to reset globals
between the two calls); turning `get_global`/`set_global` into inline
functions (`GLOBAL()` now parenthesises its argument); moving `ImageStruct`,
the decompressors and the allocators to containers (touches every decoder,
including the untested Apple II extractor); moving stash/recover to its own
file (it needs `screen.cpp` statics) or a key table for it (no tests against
old autosaves); hooks for `zget_wind_prop` (two Arthur lines),
`flush_image_buffer` and the mouse branch; a keypad table
(the rows sit among upstream's identical function-key rows); a
`zterp_os_autosave_name(bool tmp)` parameter (the call runs through upstream's
save code); a single ID list in `zterp.cpp` (the app and headless builds
already share one arm); moving the Woz hack, `pixversion`, `pack_routine` or
`active_blorb_file_stream`; an `objects_internal.h`; a real `zwindow_style` in
the headless build (it calls the app-only `v6_mark_layout_dirty()`); a name for
the Arthur `-4` in `print_handler` (no reason is recorded).

Declined in the November 2026 header pass: forward-declaring
`library_state_data` in the game headers (`spatterlight-autosave.h` reach only
falls from 22 to 20 of 51 files, as `v6_shared.hpp` needs the full definition);
a shared wrapper header for the `extern "C"` Glk includes; forward-declaring
`ImageStruct` in the decoder headers (every consumer needs the definition) and
splitting the render state out of `v6_image.h`; moving `rgb_to_bytes` away from
`bytes_to_rgb`; moving `ROSE_TAUPE` away from `HINT_MAUVE`; separate
`v6_hints.hpp` and `v6_definitions.hpp` (sections do the job); moving `kOp*` and
similar out of the already internal `entrypoints_internal.hpp`; renaming the
two-letter address globals (`-Wshadow` finds no clash); own header first in
every include list; `()` versus `(void)`; rewriting the Xcode "Created by"
banners.

November 2026 comment pass, for later ones:

- Zork Zero revisions are named as in the table at the top of
  `entrypoints_zorkzero.cpp`: "early revisions" are r242 to r343 (no BLINK-TBL),
  "the releases" are r366 and later. The compass fallback is used on r242, r296
  and r66; DRAW-NEW-COMP starts at r343.
- Left as they are: ZIL tags and names from other programs' sources (`gfx.c`,
  `pic.h`, `zip.equ`, `ms_extract1`, opcode names), which are provenance; "Step
  1-4" labels and the `Z-machine entry point:` prefix; statements of what wasn't
  checked (`entrypoints_arthur.cpp`, `arthur.cpp`); the long evidence comments,
  which get reworded but not shortened; labels repeated across different files
  ("lighter UI rebuild", "edge cases"), since each is accurate where it is.
- `journey_update_after_restore()` no longer sets START-LOC's SEEN. Per the r83
  ZIL, `GO` runs `INIT-SCREEN`, whose `QSET?` sets it, before any save or
  `ISAVE`, and the title image also needs `MODE_INITIAL_QUESTION`, which a
  restore leaves. Dev r142 has no such `QSET?`, but `jo.START_LOC` stays 0 there.
