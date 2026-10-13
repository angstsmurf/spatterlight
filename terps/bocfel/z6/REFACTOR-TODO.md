# z6 refactoring / cleanup / deduplication TODO

## Status (2026-10-13, after the second pass)

Every item below carries a tag: **[DONE]**, **[PARTLY: what]**,
**[DECLINED: why]**, and in section 7 **[FIXED]** / **[NO CHANGE]**. Nothing
is left untagged or merely "not done": what a PARTLY leaves over is either
declined with a reason in the same tag or says why it was left. Line numbers
in the items are from before the first pass.

Safety net, in `terps/bocfel/test/`:

- `run_v6_traces.py --out DIR [--compare DIR] [substring...]` drives the
  Spatterlight build of bocfel through 540 scripted sessions (the four
  walkthroughs in `SpatterlightTests/Supporting Files/`, hint / define /
  resize / graphics-switch / mouse scripts, every graphics type and
  interpreter number, every release in `~/Downloads/{ZorkZeroes,Arthurs,
  Journeys,Shoguns}`) and logs each Glk protocol message with images hashed.
  Twelve of them (`zork0-mouse-*`, added 2026-10-14) restore a saved game
  inside each Zork Zero minigame at three window sizes and click the first
  pixel, the last pixel and the pixel just outside every hit box; they need
  the saved games in the directory named by `SAVES` in the script and are
  skipped without them (`v6trace.py --save FILE` answers a RESTORE prompt).
  It keeps no goldens: take a baseline before a change, compare after.
  The pictures are kept in `DIR/pictures`, and for every case whose trace
  changed `--compare` also replays both runs into screens and compares the
  pixels (`v6screens.py`): `SAME SCREENS`, `SHIFTED PIXELS` (a picture
  scaled a fraction of a pixel differently) or `SCREENS DIFFER`, with
  old | new | difference montages in `DIR/montages`.
- `v6_epdump.py --out DIR [--compare DIR]` dumps every entrypoint and global
  the pattern scanner finds, for 64 story files (headless build with
  `BOCFEL_EP_DUMP=1`).

Each step of both passes left both unchanged, apart from the differences
listed under the bugs below and stderr lines that were removed on purpose.

Not covered by either: the
Apple II disk image extractor (checked with a standalone build, old against
new, on the disk images at hand), `writetiff.cpp` and `mkdemo.py` (checked
the same way: byte-identical output).

Bugs found and fixed along the way:

- Journey: text entry as the first thing after an autorestore dereferenced a
  null text buffer window.
- Arthur: starting with interpreter number 2 (Apple IIe), 9 or 10 crashed
  in the front graphics window setup (fix is in screen.cpp).
- Journey (second pass): after an autorestore the cached screen size in
  characters was 0, so every cursor move was clamped to (-1, -1) until the
  next resize ("moving cursor out of bounds", 1509 times over the suite).
  `journey_move_cursor` now asks Glk for the size when it has none. This
  changes 17 Journey autorestore / sessions traces: real `MOVETO`
  coordinates and column widths in place of `MOVETO 0 -1 -1`.
- Zork Zero (second pass): `within()` shifted the hit boxes of the minigame
  buttons, cards and pegs right and down by the image scale (7.3).
- V6S define menu (first pass): a click on the blank separator row selected
  a non-entry (2.1).
- Zork Zero (2026-10-14, found by the `zork0-mouse-*` cases): the Double
  Fanucci command grid took one mouse click and then no more, as the mouse
  request was made once at setup and Glk mouse requests are one-shot; a
  play could not be picked with the mouse (it takes two clicks).
  `pick_play()` now requests before every read.
- Zork Zero (2026-10-14, shown up by the fix above in `zork0-walk-sessions`):
  after an autorestore into a game of Double Fanucci the command grid
  window still existed, but the static pointer to it was gone, so the
  highlighted play was never drawn ("window_move_cursor: invalid ref", 35
  times in that case), clicks were not asked for, and the window was never
  deleted afterwards. The grid now has a rock of its own and
  `z0_update_after_autorestore` finds it again
  (`restore_fanucci_command_grid`). An autosave made before this change has
  the grid with rock 0 and behaves as before.
- Zork Zero (2026-10-14, same cases): the Snarfem click hook's answer was
  thrown away. Peggleboz and Tower patch a return after their click hooks,
  Snarfem did not, so the game's own SN-CLICK ran afterwards and its answer
  (inclusive edges, about a pixel off) was the one used.
  `find_zork0_globals` now patches `RET L02` there too.
- V6S define menu (2026-10-14): moving the selection with the arrow keys
  sent the deselected line to the VoiceOver menu handler again, which
  appended it to its item list every time (harmless, as the list is read by
  index, but it grew without bound). Not sent any more; a mouse click
  already did not send it. Not tested with VoiceOver on.

---

Survey of `terps/bocfel/z6/` (2026-10-13). This is the list of suggestions as
first written. Findings were collected by three read-only reviews
(Zork Zero + Arthur + entrypoints, Journey + Shogun + shared, graphics
pipeline). Duplication claims were checked by reading both sites, but line
numbers drift: re-grep before editing. Items marked **verify** are suspected
bugs or behaviour changes, not pure refactors, so check them before touching.

Abbreviations: EP = entrypoints.cpp, Z0 = zorkzero.cpp, Z0H = zorkzero.hpp,
AR = arthur.cpp, SH = shogun.cpp, JO = journey.cpp, V6S = v6_shared.cpp,
DI = draw_image.cpp, DB = draw_border.cpp.

Risk: L = mechanical / compile-checked, M = needs visual or behavioural test,
H = touches modal input or cross-TU state.

## Suggested order

1. Pass 1: all of section 1 (dead code, stale comments, header hygiene). Near
   zero risk, one commit.
2. Pass 2: section 2 (named constants) and section 3 (small helpers inside one
   TU).
3. Pass 3: section 4 (cross-game dedup via v6_shared), one item per commit.
4. Pass 4: section 5 (long-function splits), then section 6 (state grouping,
   RAII, entrypoint table).
5. Section 7 lists suspected bugs. Triage those first or alongside pass 1.

---

## 1. Dead code, stale comments, header hygiene (L)

### 1.1 Delete dead and commented-out code
- **[DONE: both maze debug routines and the commented Apple II block deleted]** SH 1236-1264 `debug_draw_maze()` (no callers) and 1296-1394 `solve_maze()`
  (only a commented call at 1405): about 130 lines. Delete, or put behind
  `#ifdef SHOGUN_MAZE_DEBUG`. Also the commented Apple II block at SH 1078-1081.
- **[DONE]** EP 1005-1043 and 1212-1220: commented-out Zork Zero rows (incl. a commented
  `after_SPLIT_BY_PICTURE` at 1032). EP 3273-3283: commented COMPASS_CHANGED
  search with leftover braces. Dozens of commented `fprintf(` lines in the
  Arthur / Shogun / Journey finders (EP 2223-2846, 3782).
- **[DONE]** Z0: lines 372, 519-520, 596, 804-806, 846, 857, 1662-1665 and the
  pseudo-code block 610-624. Z0H: commented `//    uint8_t COMPASS_CHANGED;`.
- **[DONE]** SH 206-219 `update_status_line`: `if (scene != 0) {...} else if (scene == 0)
  { "Scene is zero!" }` is unreachable (scene == 0 already returned at
  176-185). Remove the wrapper and the else.
- **[DONE]** `free_images` sharing logic is dead (find_graphics_files.cpp 51-58): O(n^2)
  scan nulling shared Huffman trees, but `extract_images` always allocates one
  tree per image. Drop the inner loop. (Latent hazard noted in 7.8.)
- **[DONE]** `draw_image.hpp:34` `extend_zork_zero_border()` has no definition or caller.
- **[PARTLY: `STATUS_LINES` is now used by `shogun_status_height()`, `ARTHUR_LAST_OBJECT` removed. DECLINED for the two unused `ShogunTables` fields: the struct is dumped raw by the entrypoint-dump test and removing them buys nothing]** Unused: SH 88 `STATUS_LINES` (the literal is repeated instead, see 2.1),
  V6S 285 `ARTHUR_LAST_OBJECT`, shogun.hpp 102 `ShogunTables::MAZE_BOX_TABLE`,
  shogun.hpp 100 `ShogunTables::K_HINT_ITEMS` (code uses `at.K_HINT_ITEMS`).
- **[DONE: the Zork Zero and Arthur hooks are both deleted, with their calls in screen.cpp. screen.cpp ignores the return value of the Journey and Shogun ones; Shogun's sets `dont_repeat_question_on_autorestore`, which screen.cpp resets straight afterwards (left alone)]** Dead hooks: `z0_autorestore_internal_read_char_hacks` (Z0 470, Z0H 190) is
  empty and "currently unused" but called at screen.cpp:5124; Arthur's
  equivalent (AR 891) is `return screenmode == MODE_HINTS`. Make consistent or
  drop the Z0 one together with the call.
- **[DONE]** DI 559-563 writes four zero bytes into a buffer `image_alloc` already
  zeroed; replace with `continue`.
- **[DONE]** DB 244-245 sets `left_margin = 0` for Apple II, already the initial value.
- **[DONE: the loop now runs over whole source rows only, which is what the impossible checks amounted to]** `draw_hint_menu_feet_mac` (DB 185-221) has impossible checks
  (`xpos < 0`, `ypos >= height + y || ypos < y`).

### 1.2 Stale / misleading comments
- **[DONE]** DB 136-148: doc comment for `tile_strip_down`'s old signature sits above
  `tile_section_down`; the real `tile_strip_down` (164) has none. Split it.
- **[DONE: both layouts are described next to `ImageStruct::palette` in v6_image.h]** Palette layout (DI 154-156, extract_image_data.cpp 46, draw_png.cpp 74): the
  "1-byte count then RGB" layout is true only for legacy images; PNG palettes
  (`extract_palette_from_png_data`, draw_png.cpp 262-290) are a raw 48-byte
  table. Document the layouts once next to `ImageStruct::palette` in
  v6_image.h.
- **[DONE]** v6_shared.hpp 46-51: two comment blocks are shifted by one declaration. The
  "Swaps in an opaque status window" text belongs above
  `v6_match_status_window_to_graphics_setting`, the "Make the next @read_char
  return at once" text above `v6_skip_next_keypress`.
- **[DONE]** V6S file header (1-21) lists only Definitions / Credits / color / puzzle
  skip, but the file also holds the graphics-off logic (1791-1839),
  `MenuString`, autosave stashing and window recovery.
- **[DONE]** SH 647 says crests/waves swapped relative to SH 712 ("On Apple 2: P_BORDER is
  crests, P_BORDER2 is waves").
- **[DONE]** SH 578: `FCN` parameter of `get_from_menu` is "not used directly here"; drop
  the parameter. SH 816-819: border doc comment mis-indented.
- **[DONE]** JO 114-119 and 533-538: two near-identical VoiceOver menu paragraphs. JO
  1722 `REFRESH_SCREEN` says no-op but does `store_variable(1, 0)`.
- **[DONE]** V6S 1620-1621: Arthur "topmost window" comment sits after the code it
  describes.
- **[DONE]** EP 2873-2901: "Second-pass scanner for Zork Zero..." comment is above
  `unhook_incomplete_fanucci` (2907) instead of `find_zork0_globals` (2921).
- **[DONE]** arthur.hpp: `// G0e•` and `// Ga3•` copy-pasted onto four unrelated fields
  (UPDATE, NAME_COLUMN, GL_MAP_GRID_Y, GL_WINDOW_TYPE).
- **[DONE]** Z0 1313 "(-1 = none)" but the value resets to 0. Z0 2116 "What does this do,
  exactly?" resolve or remove. AR 731 stale "Pictures 106-149 are map images"
  on the room-image branch. draw_apple_2.cpp header says `draw_apple_2.c`.
- **[DONE: `constexpr HINT_MAUVE`]** `BROWN` (v6_shared.hpp) is 0xd47fd4, i.e. mauve (DB 753 already calls it
  mauve). Rename (e.g. `HINT_MAUVE`), make `constexpr`.

### 1.3 Duplicate / misplaced declarations
- **[DONE]** `imagescalex/y` declared in v6_image.h:80-81 and again in draw_png.cpp:43-44,
  AR 47, SH 41 (Z0 relies on a transitive one). Keep the v6_image.h one only.
- **[DONE: `mainwin`/`curwin`/`pending_flowbreak` are in screen.h; `active_blorb_file_stream` was already declared in spatterlight-autosave.h, which find_graphics_files.cpp now includes]** `extern Window *curwin` (JO 43), `mainwin, curwin` (AR 81): put in
  screen.h / v6_shared.hpp. `extern bool pending_flowbreak` (Z0 2988; defined
  screen.cpp:147) -> header. `extern strid_t active_blorb_file_stream`
  (find_graphics_files.cpp 24, used by screen.cpp:7915) -> header.
- **[DONE]** SH 732-733 re-declare `margin_images` / `number_of_margin_images` (already in
  v6_shared.hpp 24-25).
- **[DONE: code uses `V6_STATUS_WINDOW.id`]** `z0_left_status_window`: Z0H 179 declares an `extern winid_t` that is never
  defined and is masked by `#define z0_left_status_window windows[1].id` (Z0
  55). Remove the extern; replace the macro with an inline accessor or
  `V6_STATUS_WINDOW.id`. Z0 679 assigns through it.
- **[DONE]** Duplicate macros in Z0H: `UNDO_BOX` (465), `RESTART_BOX` (466),
  `DIM_SHOW_MOVES_BOX` (467), `DIM_UNDO_BOX` (468), `EXIT_BOX` (463) each
  defined twice; `Z0_HINT_BORDER 8` duplicates enum `HINT_BORDER`; `F_CARD 101`
  defined at Z0 1158 and 1172. Unify hex/decimal style (`TEXT_WINDOW_PIC_LOC
  0x183` vs `COMPASS_PIC_LOC 381`).
- **[DONE]** `#define MAX(a,b)` (Z0 57; used 722, 731, 1669) -> `std::max`.
  `#define MIN` in v6_image.h:13 (used only extract_apple_2.cpp 389-391) ->
  `std::min`, delete. `typedef struct ImageStruct ImageStruct;`
  (v6_image.h:39) redundant in C++.
- **[DONE: `margin_images[BOCFEL_MAX_MARGIN_IMAGES]`, `MAX_MARGIN_IMAGES` is gone, so there is nothing left to assert]** `margin_images[100]` (v6_shared.hpp:25) vs `MAX_MARGIN_IMAGES 100`
  (V6S 51-54) vs `BOCFEL_MAX_MARGIN_IMAGES` (spatterlight-autosave.h:62): use
  one, add a `static_assert`. JO 80 / 1679 `printed_journey_words[4]`/`< 4` ->
  `BOCFEL_MAX_JOURNEY_WORDS`.
- **[DECLINED: screen.cpp uses the literal 2 in three places of its own; moving one definition does not make it shared]** `V6_STATUS_ROCK 2` (V6S 1808) is defined away from where the window is
  created (screen.cpp init_screen): one shared definition.
- **[DONE: draw_png.h now only includes `<cstddef>`, `<cstdint>` and v6_image.h]** `uint8_t`/`size_t` used without includes: entrypoints.hpp, journey.hpp,
  shogun.hpp, v6_shared.hpp. Add `<cstdint>`/`<cstddef>`; drop unused
  `<stdio.h>` (journey.hpp:11, shogun.hpp:11, decompress_vga.hpp twice,
  writetiff.hpp, draw_png.h). `draw_png.h` has an unneeded ZTERP_GLK glk
  include plus io.h/types.h. `find_graphics_files.hpp` uses
  `std::array`/`std::string` without `<array>`/`<string>`; its `7` should be a
  `kGraphicsFileCount` sentinel in the enum.
- **[PARTLY: guard fixed, the two messages print the size and a newline, and a comment in the header says where the image allocators are declared. DECLINED for the rest: `MemCalloc` is three lines and renaming the file is project-file churn]** memory_allocation.hpp: guard `memory_allocation_h` vs trailer
  `memory_allocation_hpp`; includes types.h for `size_t`;
  `image_alloc`/`grow_image_buffer`/`image_buffer_size` are defined in
  memory_allocation.cpp but declared in v6_image.h. Either rename the file
  (image memory) or move them. `MemCalloc` duplicates `MemAlloc` apart from
  malloc/calloc; "Out of memory" lacks newline and size.
- **[DONE: the header keeps a short contract comment, the algorithm is described in the .cpp only]** decompress_vga.hpp duplicates the algorithm description from the .cpp
  header.
- **[DONE]** Header parameter names disagree with definitions: `cga_lowest_adjust` vs
  `bottom_offset` (draw_image.hpp:53-54 / DB 642); `hint_foot` vs
  `bw_hint_foot` for `extend_pillars`.

### 1.4 Make file-local things `static` (check headless_v6_stubs.cpp /
entrypoints first)
- **[DONE]** JO: `journey_adjust_image` (also drop from journey.hpp), `journey_read_text_input`,
  `journey_change_name`, `TAG_ROUTE_PRINT`, `redraw_vertical_lines_if_needed`,
  `journey_init_screen`, `journey_update_after_restore`. Keep
  `journey_draw_picture` (used by screen.cpp).
- **[DONE]** SH: `shogun_display_apple_ii_border`, `deadend`, `simplify_maze`,
  `current_menu`, `current_menu_selection`, `current_border`.
- **[DONE]** V6S: `shift_margin_image_list`, `send_edited_menu_line`,
  `define_window_width`.
- **[DONE]** Z0: `SPLIT_BY_PICTURE` (298), `ADJUST_TEXT_WINDOW` (329),
  `z0_hide_right_status` (151), `F_MOUSE_CARD_PICK` (1613; removed 2026-10-14,
  nothing called it: the game picks cards itself), `pick_weight`
  (the EP table may only need the `after_*` wrappers).
- **[DONE: now `static arthur_count_lines`]** AR: `count_lines` (577, unprefixed).
- **[DONE]** find_graphics_files.cpp: `find_graphics_files` (73), `read_from_file` (178),
  `extract_from_file` (205), `load_best_graphics` (292); remove the orphan
  comment at 28.
- **[DONE]** extract_apple_2.cpp: `disk_images` / `disk_image_lengths` (118-119).
- **[DONE: static; it was already defined above its only caller]** `common_extend_border` (draw_image.hpp:33, only used DB 694): static, move
  above `draw_border_common`.

### 1.5 Debug output
- **[DONE: hot-path prints removed, the vague "Error!" messages reworded, the "Shaved off" trim block removed, 31 commented-out prints deleted from EP. The two noisy ones are gone too: the null-pixmap print went with `draw_to_pixmap_palette_optional`, and the 1509 "moving cursor out of bounds" lines were a real Journey bug (see the top of the file). DECLINED: an `EP_LOG` macro, since the remaining EP prints only fire on a failed pattern search; and the WINPROP "Patching 0x%x" print stays, as it is in every Zork Zero `.err` baseline]** Unconditional `fprintf(stderr, ...)` in hot paths: Z0 680, 711
  (`z0_resize_status_windows`), Z0 864-871 (`z0_UPDATE_STATUS_LINE`, every
  status line), EP 3324. The other finders have theirs commented out. Add one
  `EP_LOG(...)` gated macro. Replace vague "Error!" (EP 2640, 2643; Z0 153,
  2263). draw_apple_2.cpp 133-146 prints "Shaved off %lu bytes" (`%zu`) and
  `exit(1)`s on a failed shrinking realloc: remove the trim block.

### 1.6 Small style items
- **[DONE]** JO 1284 `x ? true : false`; JO 1391 passes a `uint16_t` as bool where 1383
  and 1386 use `== 1`; JO 1737 `NULL` vs `nullptr`; JO 1534 subtracts a bool.
- **[DONE]** Z0 1679 mis-indented `card_num_y += 1;`, Z0 1284 stray `};`, AR 255 missing
  space. `ptr` is `uint16_t` compared with `int new_ptr`.
- **[DONE: `update_score` uses `snprintf`. `center_put_char` stays, it is passed as a callback]** `center_put_char` (Z0 223) is a trivial wrapper. `update_score` (Z0
  1211-1222) hand-pads zeros: `snprintf("%03d")`.
- **[DONE: `typedef struct X {} X;` is `struct X {};` in all four game headers. The order of declarations in zorkzero.hpp is unchanged]** `typedef struct` after prototypes in Z0H.
- **[PARTLY: named slots added where raw indices were used (`Z0_ENCYCLOPEDIA_WINDOW`, Arthur banner windows, `V6_GRAPHICS_BG`). DECLINED for the rest: DO_HINTS deletes `windows[2]`/`[4]`/`[5]` by number because their names differ per game (a comment there says so), and one shared enum whose meaning depends on the game is less clear than per-game names]** `V6_TEXT_BUFFER_WINDOW` / `V6_STATUS_WINDOW` / `V6_GRAPHICS_BG` macros plus
  raw `windows[N]` at SH 261, 429, 431; V6S 1614, 1623, 1624; Z0 1100;
  JO 271 (`windows[3].id` instead of `JOURNEY_GRAPHICS_WIN`);
  `ARTHUR_ROOM_GRAPHIC_WIN windows[2]` / `ARTHUR_ERROR_WINDOW windows[3]`.
  Replace with an enum + inline accessor and named per-game slots. Slot 2 means
  DEFINITIONS_WINDOW / SHOGUN_MENU_WINDOW / ARTHUR_ROOM_GRAPHIC_WIN / Z0 soft
  window depending on game.
- **[DONE: every numeric `#define` in the game files and headers (171 of them, incl. `P_*`, `K_PIC_*`, the Zork Zero picture numbers) is a `constexpr int`; the LZW macros are `constexpr` too. Only the `windows[N]` aliases and `V6_STATUS_ROCK` / `V6_OPAQUE_STATUS_ROCK` are still macros]** Macro-style constants -> `constexpr`: SH 44, 87 `P_*`; DB `kBWHint*`,
  `kZ0Mac*`, `kAmigaHintCornerHeight` (316-326, 467-472); decompress_vga.cpp
  `LZW_*`, `PREFIX`, `PIXEL`.
- **[DONE: `MAZE_TILE_WALL` / `MAZE_TILE_STREET`]** SH 1180-1181 `P_MAZE_WALL` / `P_MAZE_STREET` are tile bytes, not pictures:
  rename `MAZE_TILE_*`.

---

## 2. Magic numbers to name (L)

### 2.1 Shared / Shogun
- **[DONE]** `2 * (gcellh + ggridmarginy)` at SH 121, 173, 1418 (two with the
  `get_global(sg.SCENE) == 0 ? 0 : ...` wrapper): `shogun_status_height()`.
- **[DONE: 18 is the `total` argument of `handleMenuItemOfType:index:total:` — the item count VoiceOver speaks as "item N of 18"; ignored for `kV6MenuSelectionChanged`. Now `kDefineMenuItemCount` / `kDefineMenuBlankRow` / `define_menu_item_index()`. Also FIXED: a click on the blank separator row or below the last row selected a non-entry (Return then called a routine at a garbage address)]** V6S define menu: literal `18` item count (392, 432, 515, 712) and
  `global_define_line < 15 ? global_define_line : global_define_line - 1`
  twice. Name them; confirm what the front-end does with 18.
- **[DONE]** V6S 107-124 `print_right_justified_number`: 999/-99/99/-9/9 ladder ->
  `snprintf("%4d")`. `print_number` uses `<sstream>` for one integer.
- **[DONE: `kRGBWhite`]** V6S 884-905 repeated `0xffffff`.
- **[DONE: `kHintTitleMax` (64) for both title buffers, `sizeof` at the call sites]** Hint title buffer sizes 64 / 30 / 18 vs 4000 in `print_and_send_menuitem`
  (V6S 1161-1183) -> one `HINT_TITLE_MAX`; error text says "send_menuitem".
- **[DONE: `MENU_MESSAGE_MAX`]** SH menu message buffers 100 / 40 / 100 at 381-383, 448-450, 599 -> one
  `kMenuMessageMax` (confirm 40 is not deliberate for VoiceOver).

### 2.2 Journey
- **[PARTLY: the erase-width and column tests got names (`is_rightmost_column`, `adjust_erase_width`, glyph constants). DECLINED for `HERE == 216`, `< 55` and `0.5`: each is used once and has a comment giving the ZIL it comes from]** Width thresholds 0x32 / 0x47 (882, 887), 55 (1005), 50 (1122), 71 (1270),
  0.7 / 0.5 (932, 950, 1318). Font-3 glyphs 40, 38/46/47/48/49 (1176-1231,
  1452), "JOURNEY" 7 chars (1789-1790). Picture ids 160 (235-237), 44 -> 116
  (254-256), 216 (1641), 0x14 / 0x32 (1082-1083).
- **[DONE: `std::vector<JourneyVOMenu>`]** `struct JourneyVOMenu menu[10]` literal 10 (636, 659).

### 2.3 Zork Zero / Arthur
- **[PARTLY: the identical CGA / EGA branches are merged, `kTowerWeights`. Object `0x166` is `ROOMS` (r343 and later only), the loop clears the compass-drawn flags `64 - direction`: now `zo.ROOMS`, read from DRAW-COMPASS-ROSE, and `LOW_DIRECTION`. FIXED with it: the r242/r296/r66 betas, where `0x166` is the fan / rooster / altar, lost flags 1-13 of that object at every status line redraw; and the loop ran one flag too far (13). DECLINED for the other literals: they are single uses of values copied from the game's own routines, next to the comment naming the routine]** Fanucci column widths `8 : 14` (Z0 1296, 1331), 5x3 grid (`ptr += 12`,
  `% 3`, `/ 3`); `kPegBoardTableSize = 43`, 22-peg loops (2201, 2209);
  attribute 7 / `7*2` (2204, 2122); object 0x166 `i<14` (582-584); property
  0x29 (2491); `store_variable(5,0)/(6,0)` (2350); BLINK_TBL `3*2`/`4*2`
  (2964-2965); CX/CY defaults 4/5 (2584). `text_area_height` ladder (695-703):
  CGA and EGA branches are identical, merge.
- **[DONE]** `draw_card` rebuilds `int rank_order[]` per call (Z0 1246): `static const`.
- **[PARTLY: `K_PIC_*` are in arthur.hpp and used by draw_border.cpp; banner windows are named. DECLINED for the y offsets and `0.9`: single uses, found by eye against the original, with comments]** AR y offsets 7/11/-1 (196-206), map margin -4 (172), window indices 5/6/7
  (546-551). K_PIC_BANNER / K_PIC_BANNER_MARGIN (AR 65-66) are needed in
  DB 269-283 (pictures 54/100/170/171, offsets 2, 9, 18, 3, 6): move to
  arthur.hpp. `0.9` at DB 296.
- **[DONE: `kOpRtrue` / `kOpRfalse` / `kOpRet` / `kOpRetPopped` / `kOpNop` / `kOpQuit`, `WILDCARD` is `constexpr`, `kFirstGlobal` + `global_index()`, `kScanTailMargin`; used where the byte stands alone (bytes inside a multi-byte pattern stay numbers, they read as a hex dump). `patch_arthur_pauses` has named constants and static replacement vectors. Z0 uses `kOpRtrue`]** EP: opcode bytes 0xb0, 0xb8, 0xab, 0xba, 0x2d/0x0d, WILDCARD 0xf8, global
  base 0x10 (0xb0 also hard-coded at Z0 2702). `patch_arthur_pauses`
  (EP 2110-2131): 0x10000, 0x14, 0xf6, 0x53/0x43, 0x32, 600/300; pattern
  vector rebuilt per match. `memory_size - 12` (EP 3640, 2444, 2580).

### 2.4 Graphics
- **[DONE: `kPixmapResourceId`, `rows`]** Pixmap resource id `600` (DI 226), unreadable height expression (DI 228)
  -> `rows = pixlength / (kBytesPerPixel * hw_screenwidth)`.
- **[DONE: no bare 4 left in the code; `kCGAStripOverlap`; the `35 / 22 / 2` overlaps are in one `overlap` variable with a comment]** DB bare `4` vs `kBytesPerPixel` (58, 83, 84, 108, 202, 570); DB 587-594
  `35, 22, 2`; DB 610 `7`.
- **[DONE: `kColorMask` / `kRunLengthBias`, `kLeafFlag` / `kMaxColor`, `kLzwMaxCodeWidth`, `low_bits()`]** draw_apple_2.cpp 120-123 `0x0f`/`0x0e`; decompress_amiga.cpp 46, 73, 75
  `0x80`/`0x10`/`0x0f`; decompress_vga.cpp 91 `12` -> `kLzwMaxCodeWidth`;
  `bit_masks[]` -> `(1u << n) - 1`.
- **[DONE: `side_count`, `std::string alternate_path`]** extract_apple_2.cpp 171 `kMaxDiskSides - (game_id == 0)`; `alternate_path[1024]`.
- **[DONE: `kFileHeaderSize`, and the comments list all four entry sizes]** extract_image_data.cpp: `header_size = 16` (148); comments at 76 and
  212-214 list entry sizes 8/14/16 and omit the 12-byte format.
- **[DONE: rewritten with the offsets derived from named sizes; output is byte-identical]** writetiff.cpp: `194` and `+= 8` (101, 131, 136, 147) are derived
  (IFD = 2 + 15*12 + 4 = 186, +8 header): name `kIfdBytes`.
- **[DONE: named offsets]** mkdemo.py: header offsets 0x0c/0x1a/0x1c/0x40, 16-byte tail, 0x10 global
  base.

---

## 3. Local helpers within one file (L-M)

### Zork Zero / Arthur
- **[DECLINED: the five sites differ in which events they cancel and in what they do to `screenmode`; a shared helper would need flags for each difference]** "Leave fullscreen graphics, fall back to bg window" sequence
  (`win_sizewin(graphics_fg_glk->peer,0,0,0,0); glk_cancel_*_event;
  current_graphics_buf_win = graphics_bg_glk; screenmode = MODE_NORMAL`) at
  Z0 356-368, 504-508, 1573-1577; variants at AR 494-496, 778. -> shared
  `v6_leave_fullscreen_graphics()` (variants differ in whether events are
  cancelled; M).
- **[DONE]** Z0 208-215 (V_REFRESH) vs 1566-1570 (`z0_erase_window`, "exactly as
  V_REFRESH does"): `z0_close_encyclopedia_window()`.
- **[DONE: `z0_set_bordered_status_colors()` and `v6_set_grid_colors(style)`; the Z0 stylehint lines are not a loop]** TextGrid hint colour block Z0 563-569 vs 1651-1656:
  `z0_set_bordered_status_colors()`. Z0 1009-1018 repeated `glk_stylehint_set`
  lines: loop. AR 335-338, 599-606, 660-662 same idiom: `v6_set_grid_colors`.
- **[DONE: `v6_describe_fullscreen_image(message)`, with `arthur_fullscreen_image_description()` and `z0_fullscreen_image_description()`. Shogun and Journey have no such code]** VoiceOver "Showing a full screen image" skeleton at AR 700-724 and Z0
  2992-3013: `v6_describe_fullscreen_image(msg)`. Check SH/JO.
- **[DONE: `arthur_fill_status_bar()` / `arthur_invalidate_status_line()`]** AR 553-561 vs 904-910: status-bar reverse-video fill; the `GL_SL_*` reset
  block at 564-570 vs 911-915. `arthur_fill_status_bar()`. See 7.2 for the
  reverse-video difference.
- **[DONE: `arthur_modes[]`]** AR 276-298 `arthur_sync_screenmode` vs 847-862 `hotkeys`: one
  `constexpr` (wintype, mode) table.
- **[DECLINED: the two predicates are short lists read once per picture; a sorted table plus binary search is more code, not less]** `is_arthur_stamp_image` / `is_arthur_room_image`: sorted constexpr array +
  `std::binary_search`.
- **[DONE: table of triples, and `call_routine(address, args)` at all 42 sites]** Z0 873-880 eight unrolled `DRAW_COMPASS_ROSE` calls: table of triples; add a
  `call_routine(addr, args)` helper for the pervasive
  `internal_call(pack_routine(zr.X), {...})`.
- **[DONE: `draw_box(pic, location_pic)` / `box_clicked(pic, location_pic)` replace the repeated triples, `kTowerWeights` loops in `clear_peg_table`, `solve_tower`, `draw_tower_of_bozbar`, `pick_weight`, `draw_peg`]** Peggleboz / tower (Z0 2027-2158, 2267-2283, 2386-2425, 2473-2480):
  `draw_peggles`, `z0_draw_peggleboz_box_image`, `pboz_click` share the same
  3 pic triples -> `struct Button { loc, pic, dim_pic }`; `solve_tower` 6
  unrolled `internal_insert`/`store_word`, `draw_tower_of_bozbar` 6 pairs,
  `clear_peg_table` hard-codes 6: loop. The unused `x, y` parameters of
  `z0_draw_peggleboz_box_image` (2027-2047).
- **[DONE for `within()`, see 7.3. DECLINED: a shared `mouse_click_x()/y()`: `within()` works in image pixels, `mouse_ptr_in_grid()` in 1-based text grid characters]** `within()` (Z0 1597-1609) and `mouse_ptr_in_grid()` (1330) both read
  `header.extension_table + 2` differently: `mouse_click_x()/y()`. See 7.3.

### Journey / Shogun / shared
- **[DONE: `is_tag_with_custom_name()` / `print_tag_name()`]** JO name-printing loop in `journey_create_vo_menu` 664-672, `PRINT_DESC`
  901-909, `TAG_ROUTE_PRINT` 1264-1269, plus the
  `(obj == jo.TAG_OBJECT || obj == jo.TAG) && tag_name_length != 0` test
  (667, 904): `is_player_with_custom_name()` + one name iterator.
- **[DONE]** JO `journey_erase_command_chars` 925-959 vs `ERASE_COMMAND` 1305-1330
  share the 0.7 width / `!font3 -> +1` / `AMIGA -> -1` / MSDOS shift:
  `adjust_erase_width()`.
- **[DONE]** JO malloc'd submenu array (609, 721) + `free` loop: `std::vector` or fixed
  array.
- **[PARTLY: `journey_text_buffer()` accessor (this also fixed a crash, see the top of the file). DECLINED: moving `buffer_window_index` out of `JourneyAttributes`: it is found by the pattern scanner like the attributes next to it and is part of the entrypoint dump]** JO `journey_text_buffer == nullptr` lazy init in 4 places (1528, 1738, 1816,
  1071): one `journey_text_window()`. `buffer_window_index`
  (journey.hpp 128) is not an attribute; move it out of `JourneyAttributes`.
- **[DONE]** SH `menu_select` 499-505, 517-543, 553-559: `unselect_line; sel = X;
  select_line` four times -> `move_selection()`.
- **[DONE: `struct MazeInfo`]** SH maze: `deadend`, `debug_draw_maze`, `simplify_maze`, `solve_maze` repeat
  `get_maze_width_and_height; MAZE_MAP = get_global(...); start = X + Y*W`;
  `deadend` re-queries globals per cell: `struct MazeInfo` computed once.
- **[DECLINED: the three differ in whether they select the window, and the hint screen depends on that]** V6S `center_line` / `right_line` / `left_line` (1002-1062): common prologue,
  explicit lengths equal `strlen`, `right_line` calls `glk_set_window` while
  the others do not; `right_line("                  ", 2, 18)` appears twice
  (1304, 1354): one `status_line_put(str, line, align, reverse)` and a
  `kBlankReturnHint`.
- **[DONE]** V6S `display_soft` 402-406: if/else on `garglk_set_reversevideo` ->
  `!inverse`. Name or comment the `index - 1` vs `index` asymmetry in
  `send_menu` (392 vs 432).
- **[PARTLY: `hint_item_name()` and `seen_hints_address()`. DECLINED: `tolower` (the key value also carries ZSCII specials, and the case pairs read clearly) and the `get_seen_hints()` double call (cheap, and the two calls are in different layers)]** V6S `hint_put_up_frobs` (1208-1214) and `hint_new_cursor` (1257-1261) both
  branch `hints_depth == kV6MenuTypeTopic ? hint_topic_name :
  hint_question_name` -> `hint_item_name()`. `get_seen_hints` (1147-1156) and
  `store_hints_seen` (1269-1283) compute the same address/odd bit ->
  `seen_hints_address(bool *odd)`. `display_hints` (1378-1388) and
  `hint_inner_menu_loop` (1479-1486) handle 'm'/'M', 'q'/'Q' pairs ->
  `tolower` once. `get_seen_hints()` called twice in `display_hints`
  (1299 via `init_hints`, 1335).
- **[DONE]** V6S `case 'm'` into `'q'` and click into ZSCII_NEWLINE fall through without
  `[[fallthrough]]` (1378-1381, 1520-1531). `case kGraphicsTypeApple2:` into
  `NoGraphics: break;` at 890-893.
- **[DONE: `fkey_entry(line)`, `is_command_entry(function_key)`]** V6S `V_DEFINE` repeats `fkeys_table_addr + 2 + 4 * line` (481, 545, 578,
  710, 713) and `(int16_t)user_word(fkey) < 0` (382, 587, 593, 643):
  `fkey_entry(line)`, `is_command_entry(fkey)`.

### Graphics (DI / DB)
- **[DONE: `blit_rows()` (and `blit_rect()`)]** Full-width blit `draw_bitmap_on_bitmap(buf, size, hw_screenwidth, &pixmap,
  &pixlength, hw_screenwidth, 0, y, flip)` at DB 157, 398, 442, 444, 445, 461,
  508, 512, 521, 527, 572: `blit_to_pixmap(...)`.
- **[DONE: `using Strip = std::vector<uint8_t>`; `copy_lines_from_bitmap` / `copy_rect_from_bitmap` return one, an empty one on failure, and nothing is freed by hand]** Strip copy/free: `extend_pillars` (355-359), `extend_underground_pillars`
  (421-432), `extend_mac_bw_castle_pillars` (491-500), `common_extend_border`
  (564): return `std::vector<uint8_t>` / unique_ptr instead of
  `copy_lines_from_bitmap(..., &n)` + `free`. Also fixes unchecked nullptr
  (`tile_strip_down` 168-172).
- **[DONE: `screen_height_in_pixmap_rows()` (static: nothing outside draw_border.cpp needs it)]** Screen-height computation `factor = (float)gscreenw / hw_screenwidth /
  pixelwidth; desired = ceil(gscreenh / factor)` x6 at DB 289-290, 343-344,
  414-415, 482-483, 539-540, 645-646: one exported helper.
- **[DONE: `rgb_to_bytes` / `bytes_to_rgb` in v6_image.h]** Colour unpack open-coded at DI 331-333, 446-449, 497-503 and the `GET_RGB`
  macro DB 177 (sign-quirk cast): `rgb_to_bytes` / `bytes_to_rgb` in v6_image.h.
- **[DONE: `expand_indexed()` in v6_image.h]** Indexed -> RGBA expansion: draw_apple_2.cpp 72-82 (`deindex`) vs
  DI 554-569 differ only in palette, mask, transparency: shared
  `expand_indexed()`; convert `apple2_palette` (`rgb_t`) to `uint8_t[16][3]`.
- **[DONE: `prepare_dest()`]** `draw_bitmap_on_bitmap` (DI 266-287) and `draw_rectangle_on_bitmap`
  (311-329) share the clamp / above-canvas / `grow_image_buffer` prologue
  (+ `draw_indexed_png` 335): `prepare_dest()`.
- **[DONE: one static `draw_picture(picnum, x, y, flipped, keep_palette)`; the public functions are one-line wrappers]** draw_to_pixmap family: `draw_to_pixmap_palette_optional` (DI 618) takes
  `pixmap`/`pixmapsize`/`screenwidth` but every caller (653, 719, 726, 734)
  passes the globals; only `draw_to_buffer` uses scale. Collapse into one
  static `draw_picture(picnum, x, y, sx, sy, flip, keep_palette)`.
  Parameter `pixmap` shadows the global (also draw_png.cpp 399).
- **[DONE: `decompress_image` extracts the palette itself, once]** Second redundant `extract_palette` for Blorb images (draw_png.cpp 402-403,
  DI 625-626, 669).
- **[DONE: `try_slot()`, one `sources[]` table, `set_blorb_map()`, `append_eg2_images()`, `directory_length()`. The separator search keeps its odd rule (the first of `/`, `\`, `:` that occurs at all wins, not the last separator of any kind): changing it would change which directory is searched]** find_graphics_files.cpp: 109-113, 128-132, 146-151 "open to test, close":
  `try_slot()`; hoist the `ZORK0.` rewrite out of the 3-iteration loop
  (140-143); 370-378 four identical branches -> table; 314-339 `switch`
  mirrors `fallback_table` (294-302); 259-286 EGA companion block 6 levels
  deep -> `append_eg2_images()`; 73-174 find-last-separator in disguise:
  `find_last_of("/\\:")`.
- **[DONE: `set_hw_screen_geometry(GraphicsType)` in v6_image.h, and `load_apple2_graphics()` for the duplicated extraction in screen.cpp]** Graphics geometry set in four places: find_graphics_files.cpp 236-257,
  zterp.cpp 545-546, screen.cpp 4074-4075 (`140/2.0`), screen.cpp 7679
  (`480`): `set_hw_screen_geometry(GraphicsType)`. screen.cpp 4070 and 4085
  duplicate `extract_apple2_disk_images(...kGraphicsFileWoz...)` (M, cross-TU).
- **[DONE: `remaining_bytes` removed, `disk_table_offset(n)`, `find_side_digit_pos()`, `std::string alternate_path`, forward declarations gone. Checked with a standalone build of the extractor against the old one on the Apple II disk images at hand: identical story files]** extract_apple_2.cpp: `remaining_bytes` always equals `total_story_size -
  write_offset` (updated together in `transfer_block` 397-398): delete with
  its `MIN` (391). `story_header` memcpy (426) unnecessary. Two forward
  declarations (114, 122) vanish with reordering. `resolve_logical_block` and
  `find_graphics_data_offset` both walk the per-disk header table:
  `disk_table_offset(n)`. `populate_disk_images` should validate `filenamelen`
  (210) first; extract `find_side_digit_pos()` (181-206); `std::string` for
  `alternate_path`.
- **[DONE: `find_root()`, `struct LzwEntry`]** decompress_vga.cpp clear-code and chain-walk blocks duplicate the
  `chain_limit` guard (159-166 vs 181-188): `find_root(code)`; `lzw_table[..][2]`
  -> struct.
- **[DONE: `draw_png(ImageStruct *)`, simplified `draw_indexed_png`, chunk type constants, one `kMaxChunks`, `unique_ptr` in `decompress_idat`]** draw_png.cpp: `draw_indexed_png` takes dest_x/dest_y/flipped/canvas_width
  but the sole caller passes `0, 0, false`; per-pixel bounds checks. Simplify.
  `IFF::TypeID("IDAT").val()` recomputed per chunk (185-209): `static const`.
  Local `png_chunk chunks[64]` (263, 313) duplicates `kMaxChunks = 64` (145).
  `decompress_idat` (88) mixes `new[]`/`delete[]` with free-based code.
- **[DONE: also checks width > 0xffff]** writetiff.cpp: `write16BE()` for width/height (87, 93, 121); width > 0xffff
  unchecked while height is (62); `pixarray` should be `const`; build the
  200-byte footer locally, one `fwrite`.
- **[DONE: rewritten; same output on the story files at hand]** mkdemo.py: `info()` returns a 7-tuple; global-table address computed twice
  (47-50, 61-62); use `with open`, `os.path.basename`; `checksum()` raises
  `KeyError` on non-Z version byte.

---

## 4. Cross-game dedup via v6_shared (M)

1. **[DONE: `v6_redraw_game_screen()` for V_DEFINE and DO_HINTS. `after_V_COLOR` keeps its own three-game dispatch]** **Resize dispatch** (V6S 724-730 vs 1686-1692 vs `after_V_COLOR` 1781-1787):
   `if (is_spatterlight_zork0) z0_update_on_resize(); else {
   internal_call(V_REFRESH); shogun_update_on_resize(); }` is repeated word for
   word; the else is implicitly Shogun, so V_DEFINE would run Shogun code for
   Arthur. `after_V_COLOR` includes Arthur. -> one
   `v6_update_current_game_on_resize()`.
2. **[DONE: status window bg is `Color()` and text buffers get `win_setbgnd` in the shared helper; Zork Zero calls the helper. Shogun is the exception: its status window uses swapped ANSI colours, reapplied at autorestore by `shogun_set_status_window_colors()`. Visual test with yellow-on-blue, before/after relaunch: fixes Arthur's status text turning blue-on-yellow after relaunch; Shogun and Zork Zero unchanged. Traces: 34 Arthur/Shogun autorestore cases gain one `SETBGND` for the text buffer, nothing else]** **Window colours**: `v6_set_window_colors_from_globals` (V6S 213-221) sets
   `bg_color = Color(ANSI, bg)` on all windows including the status window;
   `after_V_COLOR` (1744-1779) and Z0 2937-2945 reset the status window to
   `Color()`. Z0's loop is what the shared helper is meant to do apart from
   that. Make the shared helper handle the status window like `after_V_COLOR`
   and have Z0 call it (check Z0's "deliberately do not call after_V_COLOR()"
   comment; confirm Arthur first; visual test on autorestore).
3. **[DECLINED: two short functions per game; a base function plus a per-game hook is not shorter]** **Stash / recover**: `*_stash_state` / `*_recover_state` are structurally
   identical (AR 806-824, Z0 3214-3235): base function + game-specific field.
4. **[PARTLY: `find_seen_hints_table()` is shared by `find_display_hint_globals` and Arthur's DISPLAY_HINT branch. DECLINED: Arthur V_COLOR through `find_v_color_globals`: on reading both, the patterns and the search for the return differ, not only the `{bg,fg}` order]** **EP finders**: Arthur V_COLOR branch (EP 2218-2230) and Shogun (2563-2569)
   share a pattern and differ only in `{bg,fg}` order: reuse
   `find_v_color_globals`; `end_of_color_addr` (EP 2136) is a static shared
   across games. Arthur DO_HINTS (EP 2310-2325) and DISPLAY_HINT (2326-2330)
   duplicate `find_hints_table` / `find_display_hint_globals` (2192) incl.
   the identical `{0x01,0x00,0xcf,0x2f,W,W,0x00,0x04}`.
5. **[DECLINED: part of the entrypoint table rewrite, see section 6]** **EP table rows** that are identical/near-identical across games: V-COLOR
   (Arthur 170 vs Shogun 845), "after V-COLOR" x3, DO-HINTS alt (Arthur 279 vs
   Shogun 983), DISPLAY-HINT (Arthur 290 vs Shogun 993).
6. **[DECLINED: see section 6]** **Pixmap / hw_screenwidth geometry**: see section 6.

---

## 5. Long functions to split

**Status:** each row is tagged in its last column. `find_entrypoints` (below
the table) is **[DECLINED: a per-row `validate` callback is a new field in
every table row, which is the table rewrite declined in section 6]**.

| function | where | lines | suggested split |
|---|---|---|---|
| `V_DEFINE` | V6S 526-731 | ~205 | `edit_definition(chr, fdef, line)` for the default branch (637-703); loop becomes navigation only (H) **[DECLINED: modal loop with early exits that the traces only cover on the main paths; `fkey_entry` / `is_command_entry` / `define_menu_item_index` took out the repetition]** |
| `DO_HINTS` | V6S 1600-1698 | ~100 | `hints_enter/loop/leave` + per-game window prepare/restore hooks (M) **[DECLINED: same reason; the per-game window handling is interleaved with state that must be restored in order]** |
| `draw_hints_windows` | V6S 869-998 | ~130 | `hint_colours_for_graphics()`, `hint_status_geometry()`, `hint_draw_game_border()`; ideally per-game pieces into zorkzero/shogun/arthur (M) **[DONE: `set_hint_window_colours()`, `measure_hint_windows()`]** |
| `find_zork0_globals` | EP 2921-3612 | ~690 | one static function per branch (`z0_find_update_status_line_globals` 3154-3243, `z0_find_v_map_loop_globals` 3449-3540, `z0_find_tower_win_check_globals` 3055-3113, fallbacks 3564-3611) **[DONE: `z0_find_draw_new_comp`, `z0_find_tower_globals`, `z0_find_status_line_globals`, `z0_find_peggleboz_globals`, `z0_find_map_globals`, `z0_find_peg_move_number_fallback`, `z0_find_color_globals_fallback`]** |
| `find_shogun_globals` | EP 2551-2871 | ~320 | same **[PARTLY: `shogun_find_status_line_globals`, `shogun_find_maze_globals`. The SCENE_SELECT and GET_FROM_MENU branches share locals with the loop and are left]** |
| `find_journey_globals` | EP 2342-2546 | ~200 | same **[DECLINED: no branch is longer than about 30 lines]** |
| `find_arthur_globals` | EP 2215-2337 | ~120 | same **[DECLINED: same]** |
| `z0_update_on_resize` | Z0 2714-2848 | ~135 | `z0_resize_map/normal/slideshow()`; map paths repeat `z0_current_room_map_coords` + `update_blink_coordinates` (2766-2768, 2790-2792) **[DONE: `redraw_map_on_resize()`, `redraw_slideshow_on_resize()`]** |
| `z0_display_picture` | Z0 3051-3203 | ~150 | fullscreen / inline / default **[DONE: `display_fullscreen_picture()`, `display_inline_picture()`]** |
| `z0_update_after_autorestore` | Z0 2892-2986 | ~95 | encyclopedia, colours, `z0_recover_selected_peg()` **[DONE: `restore_selected_peg()`]** |
| `z0_init_status_line` | Z0 518-601 | ~85 | `z0_show_pending_notes()`; comment the `glk_request_char_event` + `glk_cancel_char_event` flush hack (535-536) **[DONE: `z0_show_pending_notes()`, with the comment (both calls go through `win_flush()`)]** |
| `z0_UPDATE_STATUS_LINE` | Z0 802-893 | ~90 | **[DECLINED: it follows the ZIL routine line by line, with the ZIL in comments; splitting it would lose that]** |
| `arthur_update_on_resize` | AR 330-474 | ~145 | `arthur_layout_windows`, `arthur_make_upper_window`, `arthur_redraw_mode`; early return for hints mode (400-405)  **[DONE: split into four helpers]** |
| `draw_border_common` | DB 640-791 | ~150 | `select_covering_rectangle()` (697-781, 90-line nested switch), `draw_hint_pillars()`; `find_image(BL)` + `get_image_size(BL)` double lookup **[PARTLY: `draw_covering_rectangle(kind, left_margin)`. The pillar step is 25 lines and stays]** |
| `extract_images` | extract_image_data.cpp 144-334 | ~190 | `parse_header/directory`, `compute_data_extents`, `build_image`; `ImageStruct &img = (*image_list)[entry_index]` (about 20 repeats); `read_u16(&ptr, swap)` for the 10 `conditional_byte_swap(READ_BE_UINT16_AND_ADVANCE)` sites; `std::vector` instead of `MemCalloc` **[DONE: `read_file_header`, `read_directory_entry`, `set_data_boundaries`, `load_image`]** |

Also: `find_entrypoints` (EP 3636-3766) leaks game-specific logic into the
generic loop (0xda false-match rejection for z0_UPDATE_STATUS_LINE 3703-3707,
V_MAP_LOOP dedup): per-row `validate` callback or per-game post-filter; flatten
`is_game` nesting.

---

## 6. State grouping, RAII, entrypoint table (M-H)

- **[DECLINED: large mechanical churn across autosave code for no behaviour change]** **Journey**: ~20 file statics (JO 75-112) with mixed prefixes (`journey_*`,
  bare `max_length` / `input_column` / `input_line` / `input_table` /
  `serial_as_int`, `global_*`) -> `struct JourneyUI`; `stash/recover` becomes
  trivial. **Shogun**: `current_menu*`, `current_border`, `maze_*`,
  `shogun_banner_width_left`, `a2_graphical_banner_height`,
  `last_was_interlude` -> `struct ShogunUI`. **Zork Zero**:
  `shown_rebus_hint_message`, `should_show_rebus_hint_message`,
  `should_show_map_note`, `map_entry_imagescalex/y`,
  `number_of_update_color_calls`, `fanucci_command_grid`,
  `fanucci_window_is_narrow`, `last_bold_move`, `last_pile`, `snarfem_asked`,
  `selected_peg_pos`, `skip_pboz`, `tower_solved`, `undoing`,
  `z0_right_status_width` -> `Z0State` (one reset/stash point).
- **[DECLINED: churn]** **Short global names** `ag`/`ar`/`at`, `zg zr zt zo zp`, `jg jr jo ja`,
  `sg sr st sm so`: namespace or lengthen. Z-routine hook names: keep ZIL
  upper-case for entry points only (`GET_COMMAND`, `PRINT_COMMAND`,
  `GET_DESC`, `PRINT_DESC` in JO are static helpers; `arthur_INIT_STATUS_LINE`,
  `shogun_DISPLAY_BORDER` are inconsistent). `ShogunMenus` mixes `uint8_t`
  `YOU_MAY_CHOOSE` with `uint16_t` `PART_MENU` / `SCENE_SELECT_F` (a routine
  address).
- **[DONE, in a pass of its own with the picture comparison of `v6screens.py`: `struct Pixmap` in draw_image.hpp (`create`, `clear`, `ensure_rows`, `clip_to_height`, `erase_rows`, `draw`, `fill`); `pixlength`, the `(uint8_t**, int*)` pairs and `grow_image_buffer` are gone. Done in two steps. First the one behaviour change, in the old code: a new pixmap is always 200 whole rows (it used to be 256000 bytes whatever the width -- 133.33 rows on Mac B/W, 457.14 on Apple II -- or whatever size the last one had been grown or clipped to), and a change of graphics format replaces an existing pixmap with an empty one instead of reading the old bytes at the new width. Of 528 cases 185 traces changed: 84 with the same screens, 101 with pictures shifted by at most one pixel (the stretch is computed from another row count), none more. Then the rewrite: 0 of 528 traces changed]** **Pixmap**: `pixmap` + `pixlength` (DI) are only used by DI and DB
  (verified). `struct Pixmap { std::vector<uint8_t> data; int width_px;
  rows(); ensure_rows(); clear(); clip_to_height(); }` removes the
  `(uint8_t**, int*)` parameter pairs and size desync.
- **[DONE with the Pixmap struct: `int`, the casts are gone; no arithmetic changed, 0 of 528 traces]** **`hw_screenwidth` is a `float` that is always integral** (140/320/480/640):
  forces `(int)` casts (DI 311, 358; DB 58, 83, 108, 131) and implicit
  float -> int -> uint32 conversions (`write_to_tiff`, `draw_bitmap_on_bitmap`).
  Make `int`; needs a grep over screen.cpp, shogun.cpp, zorkzero.cpp.
- **[DONE: `ImageBuf` in v6_image.h, used by the decoders; no `exit(1)` in `draw_opaque_cga`; `std::string` temp TIFF name]** **RAII**: `using ImageBuf = std::unique_ptr<uint8_t, decltype(&free)>`
  (decompress_vga.cpp already does it). Apply to decompress_amiga.cpp 32-50,
  `draw_opaque_cga` (DI 405-475; also `exit(1)` on OOM at 461),
  `draw_amiga_mac_cga_ega_vga` 548-571, `deindex`, draw_png.cpp 397-409.
  `create_temp_tiff_file_name` (DI 187-201) -> `std::string`.
- **[DONE: `struct AppleDiskSet disks`]** **extract_apple_2.cpp** has 9 file statics (`block_table`,
  `remaining_bytes`, `total_story_size`, `assembled_story`, `write_offset`,
  `table_is_set`, `global_numdisks`, `disk_images[]`, `disk_image_lengths[]`):
  `struct AppleDiskSet { ... }`.
- **[DECLINED: 1850 lines of table would be rewritten by hand; any typo in a byte pattern silently disables a hook, and the dump test only covers the 64 story files at hand]** **Entrypoint table** (EP 134-1977, ~1850 lines): one-line `EP(game, title,
  {pattern}, offset, stub, fn)` factory with defaults for the `0, 0, false`
  boilerplate; one entry holding a vector of alternative patterns instead of
  "alt" rows + `has_alt_pattern` / `found_with_alt` /
  `found_z0_update_status_line` / `found_v_map_loop` flags; EP 70-92 are 23
  dummy stub functions used only as identities ("ugly, I'll clean it up once
  everything works") and Z0's V_MAP_LOOP branch compares
  `entrypoint.title == "V-MAP-LOOP alt"` (EP 3458, 3480): add `EntryId id`
  and `switch`. Titles are inconsistent ("V_REFRESH" vs "V-REFRESH", "SCENE-
  SELECT alt " with trailing space at EP 622, "J-PLAY").
- **[DONE: `struct EntryPointIndex { by_address, lowest, highest, hooked; rebuild(); lookup(); }`; "the later row wins" is documented there]** Group `entrypoint_map` / `lowest_entrypoint` / `highest_entrypoint` /
  `entrypoint_bits` (EP 3618-3629) in one struct. Two rows resolving to the
  same address silently overwrite (last wins): document or assert.
- **[PARTLY: the four are thin wrappers over one template `find_wildcards_in_pattern()`. DECLINED: non-overlapping captures and `nullptr` slots: that changes what every caller's slot list means, for ~60 call sites checked only against the 64 story files at hand. The overlapping-pair behaviour is documented instead]** Merge the four `find_*_in_pattern` helpers (EP 2017-2104) into one
  `scan_pattern<T>` with non-overlapping wildcard captures (callers currently
  pass duplicate slot pointers like `{&hints_table_addr x3}`) and `nullptr`
  slots to skip a capture (replaces `uint8_t dummy` sinks at EP 2658, 3096,
  3118).
- **[PARTLY: `patch_bytes()`, `patch_ret_local()`, and `store_byte()` everywhere instead of raw `memory[x] =`. DECLINED: `take_routine` / `require`: about eight 3-line sites that differ in `- 1` or not and print or not; a helper would need a flag per difference]** Helpers for "take routine address and clear" (`zr.X = found_at_address;
  fprintf; found_at_address = 0`; Arthur `ar.X = found - 1`; Shogun
  `sr.X = found - 1; found = 0`; ~40 "find; if -1 print not found else print
  found" in the zork0 finder): `take_routine(...)` and `require(name, start)`.
  Patch helpers `patch_bytes`, `patch_ret_local`, `patch_ret_const` at
  EP 2603-2610, 2764-2779, 3321-3326, 2536-2537, 2460-2461, 2795-2796,
  3133-3134; use `store_byte()` consistently rather than raw `memory[x] =`.
- **[DONE: both use `find_zork0_v_map_head(blink_call, max_distance)`; r242 resolves to the same address]** Unify the two V-MAP head back-scanners: inline stride-4 loop (EP 3470-3478)
  vs `find_zork0_v_map_head` (EP 2881-2896).
- **[DECLINED: project file churn]** `.h` vs `.hpp`: pick `.hpp` for C++-only headers (`draw_png.h`,
  `draw_apple_2.h`, `extract_apple_2.h`, `v6_image.h`, `v6_specific.h`);
  include-guard styles differ (`ZTERP_DRAWPNG_H`, `draw_apple_2_h`,
  `writetiff_hpp`). Project-wide churn; low priority.
- **[DONE]** New `draw_border.hpp` for DB exports (`draw_arthur_side_images`,
  `extend_*`, `BorderKind`, `draw_border_common`); `draw_image.hpp` drops about
  25 lines. Callers: arthur.cpp, shogun.cpp, zorkzero.cpp (check v6_shared.cpp).
- **[DECLINED: churn]** Decoder naming: `draw_apple2`, `draw_png`, `draw_opaque_cga`,
  `draw_amiga_mac_cga_ega_vga` return RGBA and draw nothing -> `decode_*_rgba`;
  `decompress_vga/amiga` -> `unpack_*_indexed`; `decompress_image` CGA case
  uses a fall-through (DI 587-591).
- **[DECLINED: the upper-case names match the ZIL routine names they stand in for]** Journey `ShogunTables`-style naming: Journey ALL-CAPS C++ helpers
  (`GET_COMMAND` etc.) -> lower_snake_case statics.

---

## 7. Suspected bugs / behaviour to verify first

1. **[FIXED: in-place row swap]** **Null deref after failed flip** (DI 683-687, `recreate_image`):
   `flip_bitmap` returns nullptr on OOM and frees the source, then
   `write_to_tiff(..., result, ...)` does `fwrite(nullptr, ...)`. Fix by an
   in-place row swap (`std::swap_ranges` over `height/2` rows), deleting
   `flip_bitmap` (377-398).
2. **[NO CHANGE, verified 2026-10-14 against the r74 ZIL (`defs.zil`): INIT-STATUS-LINE ends with `HLIGHT H-NORMAL`, UPDATE-STATUS-LINE starts with `HLIGHT H-INVERSE` and prints the line in it, so the two routines match the game]** **Arthur status line left in reverse video** (AR 553-561 vs 904-910):
   `arthur_INIT_STATUS_LINE` resets reverse to 0, `ARTHUR_UPDATE_STATUS_LINE`
   does not. May be intentional (Z-code); verify.
3. **[FIXED: clicks in a graphics window arrive as window pixels and `flush_bitmap` stretches the pixmap from (0,0), so the image position is `click / scale`. Subtracting the scale shifted every minigame hit box right and down by `scale` image pixels. Now half-open rectangle tests on `click / imagescalex`, `click / imagescaley`. Established by reading the code, then confirmed 2026-10-14 by the `zork0-mouse-*` cases: in Tower, Peggleboz and Snarfem every first and last pixel of a box hits and every pixel just outside misses, at 800x600, 1000x700 and 430x600. The Fanucci cards are hit-tested by the game's own Z-code, not by `within()`]** **`within()`** (Z0 1597-1609) `x / imagescalex - imagescalex` subtracts the
   scale rather than 1; `mouse_ptr_in_grid` (1330) uses `-1` vs
   `/scale - scale`. Verify before unifying.
4. **[FIXED: `MenuString::append_zscii`]** **JO 664-672 possible overflow**: `m->name[j] = user_byte(NAME_TBL++)`
   loops `TAG_NAME_LENGTH` (a game byte) into `name[STRING_BUFFER_SIZE]` =
   15 bytes. Use `MenuString::append_zscii` like `print_tag_route_to_str`
   (545-563).
5. **[FIXED: `i <= end`, `size_t j`, empty-pattern guard. No entrypoint or global changed on any of the 64 story files]** **`find_pattern_in_mem` off-by-one** (EP ~2000-2104): `i < end` misses a
   match ending exactly at the window end; `int j < pattern.size()`;
   `find_*` return `int32_t` but callers store into `uint32_t` and compare to
   -1 (EP 2355, 2373, 2423); `pattern.size() - 1` underflows for an empty
   pattern. Fixing the off-by-one may change which patterns match.
6. **[FIXED]** **V6S 68-76 `add_margin_image_to_list`** shifts a full list before the
   duplicate check, so re-adding an existing image drops the oldest entry
   needlessly. Swap the order.
7. **[FIXED: table lookup that returns on a miss]** **AR 876 `hotkeys[variable(1)]`** inserts a default entry for an unknown
   key (maps to K_WIN_NONE / mode 0, silently switching to "no graphics").
   Use `find()` and return on a miss.
8. **[NO CHANGE]** **`.EG2` extraction memcpy's a new `default_huffman_tree`** over the one
   `.EG1` images point at; harmless today only because EGA uses LZW.
9. **[FIXED: the underflow is gone: a name without an extension is replaced whole. NO CHANGE: still only `/`, which is all a macOS path needs]** **find_graphics_files.cpp 370-378** only searches `'/'` (unlike
   `find_graphics_files`, which tries `/ \ :`), and `dotpos - delimiterpos`
   underflows if the dot precedes the slash or is absent.
10. **[FIXED]** **DB 107-116 `erase_lines_in_bitmap`**: `startpos > pixlength` should be
    `>=`; negative `y`/`height` writes before `pixmap`. **DB 81-102
    `copy_rect_from_bitmap`**: no negative / `x+width` bounds and no
    `pixmap == nullptr` guard. Callers currently pass sane values (defensive).
11. **[FIXED]** **SH 1207 `deadend()`**: west-neighbour test `index > 1` should be
    `index > 0` (harmless with borders skipped). 
12. **[FIXED: skips the byte instead of asserting]** **EP `unhook_incomplete_fanucci`** (2907-2919) restores `pattern[offset]`
    as the original byte, which would write 0xf8 into the story if that byte
    were a WILDCARD. Safe today; add an assert.
13. **[DONE]** **SH 206-219** unreachable "Scene is zero!" branch: see 1.1.
14. **[FIXED]** V6S `print_and_send_menuitem` error text "send_menuitem: Illegal menu
    type!" does not match the function name.
15. **[DONE: both replaced; nothing measured]** **Possible Zork Zero hotspots to measure, not assumed**: V6S `print_number`
    `ostringstream`, JO per-redraw malloc (not a real cost; just remove).

---

## 8. Not worth doing

- Merging `journey_adjust_image` with Shogun's image fit code (SH 1471,
  1005-1016): scaling rules differ per game; only `v6_inline_image_scale()` is
  genuinely shared and already is.
- Moving the hint / define / soft-key code into per-game files: it already
  lives only in v6_shared.cpp and games call into it. (Only the per-game
  *pieces* inside `draw_hints_windows` are worth pushing out, see section 5.)
