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
- Any `winid_t` kept outside `windows[]` is lost at autorestore and has to
  be found again: by tag through the autosave (`stored_bufferwin`,
  `z0_right_status_window`) or by rock (`v6_grid_window_with_rock()`, for
  the opaque status window and the Fanucci command grid).

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
including the untested Apple II extractor).
