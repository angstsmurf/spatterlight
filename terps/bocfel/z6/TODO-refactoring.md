# Refactoring opportunities in `terps/bocfel/z6/`

Findings from a read-through of the directory (about 18.6k lines). Nothing
here has been changed; each item says where it is and why it is worth doing.
Ordered by estimated value for effort. Line numbers are approximate.

Biggest files, for scale: `entrypoints.cpp` 3799, `zorkzero.cpp` 3189,
`journey.cpp` 1940, `v6_shared.cpp` 1941, `shogun.cpp` 1495.

## 1. `entrypoints.cpp`: split it and replace the `fn ==` if-chains

- **Split by game.** The file mixes the engine (pattern search, claiming,
  lookup index, `find_entrypoints`, `check_entrypoints`) with four large
  per-game blocks: pattern tables (lines ~165-1980) and "globals finders"
  (~2208-3615). Move each game's table and finder into
  `entrypoints_{arthur,journey,shogun,zorkzero}.cpp`. The
  `#pragma mark` sections already mark the seams. Keep the generic helpers
  (`find_pattern_in_mem`, `find_*_in_pattern`, `patch_*`) in a shared
  `entrypoints_search.hpp/.cpp`.
- **Replace the `entrypoint.fn == X` chains.** `find_arthur_globals`,
  `find_journey_globals`, `find_shogun_globals` and `find_zork0_globals` are
  long `for (...) { if (fn == A && found) ... else if (fn == B && found) ... }`
  chains (87 `entrypoint.fn ==` comparisons). Every branch repeats
  `&& entrypoint.found_at_address != 0`. Options:
  - add an optional `void (*on_found)(EntryPoint &)` field to the table row,
    so each finder sits next to the pattern that triggers it; or
  - at least add a small `EntryPoint *find_row(fn)` and look up each row
    once, instead of scanning the whole table for every branch.
  Doing this also removes the need for the dummy functions below.
- **Remove the dummy `fn` bodies** (`RT_SEE_QST`, `BLINK`, `V_COLOR`, ...,
  lines 58-85). Their comment says they exist so the function pointer can
  serve as an identity tag ("I'll clean it up once everything works").
  Use an explicit enum or string id in the row instead, with
  `fn = nullptr` for rows that only locate an address. `check_entrypoints`
  then needs a null check, or the index skips rows without `fn`.
- **Make the `find_*_in_pattern` family one function.** The `globals`,
  `routines`, `values` and `16_bit_values` variants (lines ~2023-2070)
  differ only in the type of the output pointer and how a byte or word is
  decoded. A single template, or one function taking a small capture-kind
  descriptor, would do.
- **Pull the special-case match rejection out of `find_entrypoints`.** The
  `while (... fn == z0_UPDATE_STATUS_LINE && pattern[0] == 0x03 && memory[offset+9] == 0xda)`
  loop plus its 12-line comment is game-specific logic inside the generic
  scanner. Give `EntryPoint` an optional `bool (*accept)(uint32_t offset)`
  predicate and put this one in the Zork Zero file.
- **Error reporting.** About 112 `fprintf(stderr, ...)` calls, mostly
  "Error! Could not find X" with inconsistent wording. A `warn_missing(const char *what)`
  helper (or a macro that stringifies the field name) would make them uniform
  and greppable. Several finders also keep going silently after a failed
  lookup; decide per case whether to abort the finder or continue.
- **Replace the magic opcode bytes inside patterns** with named constants
  where practical. The file already defines `kOpRet`, `kOpRfalse` and so on
  but leaves the patterns as raw numbers, and says so deliberately.
  A tiny pattern builder (`op(...)`) would let the long Arthur and Zork Zero
  patterns carry comments per instruction.
- `dump_entrypoint_state` (debug, `HEADLESS_V6` only) hard-codes every
  per-game struct name. Have each game provide a `dump_state()` instead so
  adding a struct doesn't touch this file.
- Declare `fg_global_idx` and `bg_global_idx` once. They are `extern` in both
  `entrypoints.hpp` and `v6_shared.hpp` and defined in `entrypoints.cpp`.
  `entrypoints.hpp` also uses `uint8_t` without including `<cstdint>`.

## 2. Per-game state hooks: one interface instead of copy-pasted function sets

`arthur.cpp`, `journey.cpp`, `shogun.cpp` and `zorkzero.cpp` each export the
same family of functions, called from outside via `is_spatterlight_*` flags
(40 flag tests across the directory):

- `*_stash_state` / `*_recover_state`
- `*_update_after_restore` / `*_update_after_autorestore`
- `*_update_after_restart` (Shogun only)
- `*_autorestore_internal_read_char_hacks`
- `*_update_on_resize`
- `*_display_picture` (Arthur, Zork Zero) and the Shogun/Journey equivalents

Suggested: a `struct V6Game` of function pointers (or an abstract class)
with a null-op default for each hook, one static instance per game, and a
single `current_v6_game` pointer. The `if (is_spatterlight_arthur) ... else if (journey) ...`
chains in callers (`bocfel` core, `screen.cpp`, `entrypoints.cpp`
`find_entrypoints`) become one call. It also makes adding a hook a
compile-time requirement for all games rather than a grep job. The four
`is_spatterlight_*` booleans plus `is_spatterlight_v6` could become one enum
(`Game` already exists in `entrypoints.cpp`, so check that they do not
duplicate each other).

Note: `screenmode` is a global (`V6ScreenMode` in `v6_specific.h`) that all
games mutate directly. Wrap it in `set_screenmode()` / `screenmode()` so
mode transitions can be traced, and so the "reconcile stale mode after
restore" guard in `z0_update_after_restore` can be shared with the other games.

## 3. Shared pieces that each game reimplements

- **Stash/recover of graphics windows.** `v6_stash_graphics_windows` and
  `v6_recover_graphics_windows` already exist, but Arthur and Shogun each
  also abuse `dat->slideshow_pic` to carry a game-specific value
  (`last_slideshow_pic`, `current_border`). Give the autosave struct named
  fields (or a generic `uint32_t game_state[N]`). Journey's stash copies word
  records field by field in two loops; a struct assignment or `std::copy`
  would do, and `journey_recover_state` should share its clamp to
  `BOCFEL_MAX_JOURNEY_WORDS` with the code that appends words (line ~1684).
- **After-autorestore recolouring.** Arthur, Shogun and Zork Zero all do
  `update_user_defined_colours(); v6_set_window_colors_from_globals();` then
  game-specific work, then `window_change()` or `v6_close_and_reopen_front_graphics_window()`.
  Factor out `v6_common_autorestore_recolor()`.
- **`windows[N]` index macros.** Each game redefines window slots as local
  `#define`s (`ARTHUR_ROOM_GRAPHIC_WIN windows[2]`, `SHOGUN_MENU_WINDOW windows[2]`,
  `DEFINITIONS_WINDOW windows[2]`, `Z0_ENCYCLOPEDIA_WINDOW windows[3]`,
  `JOURNEY_GRAPHICS_WIN windows[3]`, ...). The same slot means different
  things per game, and `V6_TEXT_BUFFER_WINDOW`/`V6_STATUS_WINDOW`/`V6_GRAPHICS_BG`
  are macros in `v6_shared.hpp`. Replace all of them with one per-game
  `enum WindowSlot` and an inline accessor (`window_at(Slot)`), so a slot
  collision between a shared feature (definitions, hints) and a game window
  is visible in one place. At least move the macros out of `.cpp` files
  into a section of `v6_shared.hpp`, and convert to `inline Window &` / `constexpr`.
- **Image-less fallback text.** `v6_graphics_off()` / `v6_pictures_unseen()`
  checks are scattered through the games with slightly different fallbacks
  ("press any key" after a skipped picture, opaque status window, picture
  descriptions). Review whether a `v6_picture_fallback(...)` helper can hold
  the common decision.
- **VoiceOver menu building.** `MenuString` is a good start. Callers still
  hand-assemble `win_menuitem(type, index, column, line, str, length)` with
  the magic `18` and `0` arguments (see the comment example in
  `v6_shared.hpp`). A `send_menu_item(type, index, MenuString&)` wrapper
  with named parameters would remove the long positional argument lists in
  `journey.cpp`, `shogun.cpp` and `v6_shared.cpp`.
- **Hand-rolled "delete a window then null it" patterns.** `v6_delete_win`
  has 28 call sites; check that every site that closes a window also
  clears mouse/hyperlink requests and any file-static pointer cached
  elsewhere (e.g. `fanucci_command_grid`, `current_graphics_buf_win`), ideally
  by using one RAII-ish helper.

## 4. `zorkzero.cpp` (3189 lines)

Already organised with `#pragma mark` sections, but it is four or five
programs in one file. Suggested split:

- `zorkzero_border.cpp`: Border Display, Hint Border Display
  (`DISPLAY_BORDER` alone is 135 lines).
- `zorkzero_fanucci.cpp`: Double Fanucci (`redraw_fanucci` 127 lines,
  `pick_play` 119 lines).
- `zorkzero_games.cpp` (Snarfem/Nim, Peggleboz, Tower of Bozbar): each has its
  own state, timers and mouse handling and shares nothing with the others
  except the `Window` helpers.
- `zorkzero_resize.cpp`: Resize Handling, `z0_resize_status_windows`,
  `z0_update_on_resize` (75 lines of mode switching).
- Keep a slim `zorkzero.cpp` with the entry points, image dispatch and save/restore.

Other points:
- The long comments in `z0_update_after_restore` / `z0_update_after_autorestore`
  document real crash reasons. Keep them, but turn the repeated invariant
  ("screenmode is not saved", "per-window colours are not saved") into one
  comment near `screenmode` and a named helper like `z0_reconcile_screenmode_after_restore()`.
- Many functions are 80-135 lines (`DISPLAY_BORDER`, `z0_update_colors`,
  `redraw_fanucci`, `pick_play`, `z0_UPDATE_STATUS_LINE`, `display_fullscreen_picture`).
  Split along the existing comment blocks.
- Peggleboz (`restore_selected_peg`) and Fanucci (`restore_fanucci_command_grid`)
  rebuild C++ state from the Z-machine state after autorestore. If other
  minigames need that, a `std::vector<void(*)()> post_autorestore_fixups`
  registered by each minigame would keep `z0_update_after_autorestore` short.

## 5. `v6_shared.cpp` (1941 lines) is three features

It holds the Definitions screen (`V_DEFINE` is 254 lines, the longest function
in the directory), the Hints screen (lines ~778-1755), Credits, the
graphics-off helpers, the skip-puzzle prompt, margin images, and the
Z-string to C-string conversion.

- Split into `v6_definitions.cpp`, `v6_hints.cpp`, `v6_credits.cpp`,
  `v6_strings.cpp` (zstr helpers, `MenuString`), `v6_graphics_settings.cpp`
  (graphics off, unseen pictures, skip prompt), keeping `v6_shared.cpp` for
  margin images and window helpers.
- Break `V_DEFINE` into window setup, key-table display, input loop, and
  redraw functions. Same for `hint_inner_menu_loop` (111 lines),
  `DO_HINTS` (107 lines) and `display_hints` (103 lines): these are menu state
  machines. A small `HintMenuState` struct passed by reference (instead of
  `uint16_t *index, bool *exit_hint_menu` out-params) would help.
- The file-level globals in `v6_shared.hpp` (`hints_depth`, `hints_table_addr`,
  `seen_hints_table_addr`, `fkeys_table_addr`, `fnames_table_addr`,
  `global_define_line`, `stored_bufferwin`, `fg_global_idx`, ...) should
  be grouped into a `V6SharedGlobals`/`HintGlobals` struct (like the per-game
  `ag`/`jg`/`sg`/`zg` structs) so the hex dump and any reset on restart can
  handle them in one place.
- `call_routine(uint32_t, std::vector<uint16_t> args = {})` allocates a
  vector on every call; use `std::initializer_list<uint16_t>` or a
  `std::span` if the language level allows.
- `print_zstr_to_cstr(addr, char *str)` has no size parameter (a bounded
  variant `print_long_zstr_to_cstr(..., maxlen)` exists). Make every caller
  use the bounded one, or return `std::string` and drop the fixed buffers.
  `STRING_BUFFER_SIZE = 15`, `kHintTitleMax = 64` and the per-call
  `char str[60]` buffers show the same thing: a `std::string` or a
  `MenuString` that owns its storage would be simpler.

## 6. `journey.cpp`, `shogun.cpp`, `arthur.cpp`

- `journey.cpp`: `journey_print_character_commands` (113 lines),
  `journey_adjust_windows` (106), `journey_init_screen` (105) and
  `update_internal_globals` (103) are layout code with many magic numbers.
  Name the layout constants (cell sizes, margins), and make `journey_adjust_windows(bool restoring)`
  two functions instead of one with a flag.
- `journey_pre_save_hacks` / `journey_post_save_hacks` hard-code Frotz's
  80x25, 8x16 values (`0x08`, `0x10`, `0x70`, `0x19`, `0x50`). Name them.
- `shogun.cpp`: three separable parts (Menu System ~400 lines, Display Border
  ~320, Maze ~220) plus window layout. Split out `shogun_maze.cpp` and
  `shogun_border.cpp`.
- `shogun_update_after_autorestore` and `journey_*` use many static globals
  (`current_menu`, `current_menu_selection`, `current_border`,
  `selected_journey_line/column`, ...). Group per game in a state struct so
  stash/recover can copy it wholesale.
- `arthur.cpp`: `arthur_modes` table is a good pattern; do the same for the
  `is_spatterlight_*`-dependent branches elsewhere. `RT_HOT_KEY` rewrites
  `variable(1)` for the graphics-off case; do that through the table
  (a `requires_graphics` flag) rather than a special case.

## 7. Graphics pipeline (`draw_*.cpp`, `extract_*.cpp`, `decompress_*.cpp`)

- `draw_image.cpp` mixes five jobs: the `Pixmap` class, TIFF export and cache
  (`write_to_tiff`, `create_temp_tiff_file_name`, `send_image`, `send_pixmap`,
  the sent-image ring), palette handling, CGA/Amiga/VGA/EGA decoders, and the
  public draw API. Split into `pixmap.cpp`, `image_cache.cpp` (TIFF + ring),
  `draw_image.cpp`.
- `write_to_tiff` in `draw_image.cpp` is a thin wrapper over `writetiff()` in
  `writetiff.cpp`. Move the wrapper into `writetiff.cpp`, so that `FILE`
  handling lives in one place. `writetiff.hpp` includes `io.h` only for `FILE`;
  use `<cstdio>`.
- Palette constants (`kPaletteSize`, `kReservedPaletteSlots`, `kBytesPerColor`)
  are private to `draw_image.cpp`, but `global_palette` lives in
  `draw_png.cpp`, with the palette layouts documented in `v6_image.h`. Put
  the constants and a `Palette` struct next to `ImageStruct`, and make
  `ImageStruct::palette` a `std::vector<uint8_t>` or `std::array` so the
  "every producer allocates at least 48 bytes" invariant in
  `extract_palette()` is enforced by the type.
- `ImageStruct` holds a raw `uint8_t *data` and `palette`; `free_images()`
  frees them by hand. Use `std::unique_ptr<uint8_t[], free>` or
  `std::vector`. Same for `Pixmap`.
- `MemAlloc`/`MemCalloc` (exit on failure) and `image_alloc` (returns null)
  coexist in `memory_allocation.cpp`, and the older raw `malloc`/`calloc`
  calls remain in `draw_image.cpp`, `draw_png.cpp`, `extract_apple_2.cpp`,
  `extract_image_data.cpp` and `find_graphics_files.cpp`. Pick one
  convention (preferably containers or `unique_ptr`) and migrate. `exit(1)`
  inside a library-ish allocator is also a poor fit for a Glk interpreter
  that tries to autosave.
- `draw_border.cpp`: the `blit_rows` overloads, `copy_*_from_bitmap` and
  `erase_lines_in_bitmap` duplicate `Pixmap::draw/fill/erase_rows` in
  `draw_image.cpp` (same job on the same pixmap, with a different
  `Strip` type). Make `Strip` a `Pixmap` view, or have `draw_border.cpp` use
  the `Pixmap` API.
  `draw_arthur_side_images` (230 lines) and `extend_mac_bw_castle_pillars` (70
  lines) should be broken up.
- Decompressors (`decompress_amiga`, `decompress_vga`) return `uint8_t *` owned
  by the caller with sizes implied by `ImageStruct`; return a
  `std::vector<uint8_t>` and make the error paths (`Read past end of data`
  prints) return an empty result instead of a half-filled buffer.
- Many headers use inconsistent extensions (`.h`, `.hpp`) and naming
  (`v6_image.h`, `v6_specific.h`, `v6_shared.hpp`). Pick `.hpp` for C++ only
  headers. `v6_specific.h` has both enums and macros (`GLOBAL`, `get_global`,
  `set_global`) that should become inline functions. `GLOBAL(X)` does not
  parenthesise `X` (`header.globals + X * 2`), so `get_global(a + b)` is wrong.
- `draw_png.cpp` has a `DEBUG_PNG` macro with `debug_png_print`; fold into a
  shared logging helper (see "Diagnostics" below).
- `mkdemo.py` (story file to demo converter) is a build-time tool sitting
  among the interpreter sources; consider moving it to a `tools/` folder.

## 8. Cross-cutting

- **Diagnostics.** 152 raw `fprintf(stderr, ...)` calls across the directory
  (112 in `entrypoints.cpp`). Add one `v6_log(level, fmt, ...)` that can be
  silenced in release builds and enabled by the existing `BOCFEL_EP_DUMP`
  environment variable.
- **Magic numbers for Z-machine values.** `0xf8` WILDCARD, `0x7f` delete,
  Frotz sizes, and menu column `18` appear inline; `ZSCII_*` constants exist
  for keys but the code compares `character == 0x7f` directly
  (`journey.cpp:810`, next to `ZSCII_BACKSPACE`).
- **Header hygiene.** `entrypoints.hpp` and `v6_shared.hpp` depend on
  includes made by their users (`uint8_t`, `Window`, `library_state_data`,
  `InfocomV6MenuType`). Make them self-contained and add include guards
  consistently (`#pragma once` vs `#ifndef x_hpp` are mixed).
- **Tests.** The only test infrastructure is the headless build
  (`Makefile.headless`, `run_transcript.sh`, `EPDUMP`). Before doing the
  entrypoints split in item 1, capture `BOCFEL_EP_DUMP` output for each
  supported game revision and diff it after the refactor; the dump already
  includes a memory hash, so a regression in pattern matching or patching
  shows immediately.

## Suggested order

1. Add the EPDUMP baseline for every game revision (no code change).
2. Item 1: remove dummy functions, add the `find_row` / `on_found` mechanism,
   then split files (EPDUMP output must stay byte-identical).
3. Item 7: palette and allocation clean-up; macros to inline functions.
4. Item 2/3: per-game hook interface and window-slot enum.
5. Items 4-6: file splits and long-function breakups, one game at a time.
