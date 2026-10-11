# terps/scott: refactoring, cleanup and deduplication TODO

Findings from a read-only review, re-verified against the tree on 2026-10-11.
Items that turned out to be already done, stale, or wrong were removed; line
numbers are current as of that date and will drift.
Effort: XS/S/M/L. Risk: none/low/medium/high.

Validation after each batch: `make -f Makefile.headless check formatcheck`,
and `make all-tests` in `saga/test` for anything touching renderers/loaders.

---

## 1. Bugs and risks (fix first)

- [ ] **`BitFlags` is `long` but shifted as 64-bit.** scott.h:172, scott.c:104,
  restore_state.h:18; shifted as `(uint64_t)1 << bit` in scott_actions.c:148,
  155,395,402; saved/loaded with `%ld` at scott_meta.c:66,101. Fine on LP64,
  wrong on LLP64 (Windows). Make it `uint64_t`, add `TestBitFlag(int)` beside
  Set/ClearBitFlag. Watch the save-file format. (S, low-medium)
- [ ] **Typo'd macro `TI99OP_DRTI99OP_ITEM`** (scott_defines.h:152, used in
  ti994a/ti99_4a_terp.c:179); should be `TI99OP_DROP_ITEM`. (XS, none)
- [ ] **Variable `seTI99CND_param`** in ti994a/ti99_4a_terp.c (decl :74, 7
  uses) is a bad find/replace of `second_param`. Rename. (XS, none)
- [ ] **`-w` option has no effect:** scott.c:689-690 sets `split_screen = 0`,
  but glk_main sets it to 1 in both branches (scott.c:789,793), so the `else`
  at :791-794 is dead for `split_screen`. Honour it or delete the option.
  `-f` and `-z` are handled (:695-702) but missing from `glkunix_arguments`
  (:633-654).
- [ ] **`GetRTPILines` (title_image.c:197-242):** unbounded write to
  `line[128]` (:210); `lines[]` entries not referenced by `order[]` (indices
  6, 7, 10, 16) never freed; `outlines` returned to `RTPITitle` (:288) and
  never freed; `linelenghts[]` (:217) write-only. `title_screen` is freed at
  :289,349,387 but never set to NULL.
- [ ] **Leaks in saga:**
  - `DetectAtari8` (atari8detect.c:922-945) leaks `companionfile` on the
    `else` branch when the DB lookup fails.
  - `DetectApple2` (apple2detect.c:763,770) returns early leaking `datafile`
    and `companionfile` (`invimg` is handed to `SetA2InventoryImage`, not
    leaked).
  - c64decrunch.c:711-713 leaves an unconditional `C64_DUMP_US_IMAGE` env-var
    dump in production code, and `writeToFile` (:529) opens with `"w"`
    instead of `"wb"`.
- [ ] **rtpi_graphics.c:42-68 macros lack parentheses** (`RTPI_IMAGE_SIZE`,
  `GROM_SIZE`, `GROM_FLAT_SIZE`, `NUMBER_OF_RTPI_ROOM_IMAGES 24 + 9`). No
  current use site is affected (all are `[M]`, `< M`, `ptr + M`, args), so
  this is style, not a live bug. `NUMBER_OF_RTPI_IMAGES` is unused. `op > 7`
  at :295 is dead (`op` is a 3-bit field, :277).
- [ ] **Minor data oddities:**
  - game_specific.c:613-617 Voodoo `case 7` missing `break` (falls into
    `default: break;`, harmless today).
  - robin_of_sherwood.c:369 overwrites `sys[HIT_ENTER]` right after the
    key-list map; Spectrum key list has `OK, OK` (:375-376) and
    `YOU_SEE, YOU_SEE` (:388-389) duplicated (last wins). Both look
    deliberate; add a comment or drop the dead entries.
  - scott_actions.c `COND_IN_PLAY` (:426-428) is the only condition that
    range-checks its argument; the others index `Items[arg]` unchecked.

---

## 2. Dead code (verified)

- [ ] Delete `UNKNOWN_ACTIONS_TYPE` scott_defines.h:459 (unused).
- [ ] `FlushRoomDescription` scott_display.c:148-151: `if (buf != NULL) {
  free(buf); buf = NULL; }` -> `free(buf)` (nulling a parameter is pointless).
- [ ] Merge `case OP_LOOK: break;` (scott_actions.c:553-554) and
  `case 0: /* NOP */` (:614-615).
- [ ] Delete whole file `ai_uk/debug.c` (includes a non-existent `debug.h`,
  defines `list_action_addresses` with no callers) and the `filter-out` in
  Makefile.headless:50-52.
- [ ] Delete `errorcount` (sagadraw.c:35), `circular_buf_full`
  (ringbuffer.c:60; `circular_buf_reset` is used by `circular_buf_init`,
  make it static instead), `ATARI8_PLANE_SIZE` (atari_8bit_vector_draw.h:22;
  same value as `A8_IMAGE_SIZE`), declarations of `FindUSImageDataInDisk` /
  `CompareUSImagesWithDisk` (rtpi_graphics.h:15-16, never defined), unused
  `CASESENSITIVITY` / `WRITEBUFFERSIZE` / `<ctype.h>` in unzip_in_mem.c.
- [ ] seas_of_blood.c: `#define ERROR 99` (:50, one use at :661),
  `//#define AUTOWIN` at :689 (saga/test/Makefile:316-328 passes `-DAUTOWIN`;
  document that there instead).
- [ ] Commented-out code: `//#define DEBUG_ACTIONS` (scott_actions.c:39),
  `PrintTI99HeaderInfo` (load_ti99_4a.c:102), apple2_vector_draw.c:519
  `fprintf`, unzip_in_mem.c:85.

### Make file-local symbols `static` (zero risk; all grep-checked repo-wide)

- [ ] parser.c: `SplitIntoWords` (:684), `LineInput` (:751);
  `ExtraCommandsKey` (:159), `Abbreviations` (:217), `AbbreviationsKey` (:220)
  -> `static const`.
- [ ] scott.c `RunTests` (:629); scott_display.c `DrawRoomImage` (:326, drop
  decl scott_display.h:24); scott_actions.c `ClearScreen` (:275, drop decl
  scott_actions.h:14).
- [ ] saga.c: `ReadUSDictionary`, `DrawApple2Image`, `DrawDOSImage`,
  `DrawUSImage`, `DrawInventoryImages`, `LoadHeader`, `ParseDictionary`,
  `HulkUKSpecificParsing`, `AddGameFileExtension`, `DrawRoomObjectImages`,
  `CompareFilenames`, `LookForCompanionFilenameInDatabase` (the last three
  are declared in saga.h:15,24,28 but used nowhere else).
- [ ] atari_8bit_vector_draw.c: `DrawSingleAtari8ImageByte` (:768),
  `delay_active` (:779); atari8detect.c `PatchMissionImpossible` (:891).
  `TestApple2ImageWithName` / `TestAtari8ImageWithName` are called from
  saga/test/filltest.c, so they stay exported; put them behind a
  `TEST_HOOKS` header or `#ifdef`.
- [ ] apple2detect.c: `ReadA2DiskImageFile` (:838).
- [ ] c64decrunch.c: `LoadC64USImages` (:437), `LookForSoCCompanion` (:499),
  `writeToFile` (:527). hulk.c `HulkShowImageOnExamineUS` (:15).
  robin_of_sherwood.c `animate_lightning`: the prototype at :27 is `static`
  but the definition at :176 is not; add `static` to the definition.
- [ ] seas_of_blood.c: `blood_battle` (:58/:340), `PatchCryptImage` (:142),
  `background_colour` (:37), `dice_*` (:53) and the dice windows (:36); only
  `blood_image_data` is in the header.
- [ ] load_ti99_4a.c `max_messages`, `max_item_descr` (:52-53);
  `const static` -> `static const` (rtpi_graphics.c:76).

---

## 3. Duplication

### Core (scott*.c, parser.c, detect_game.c)

- [ ] **`DEBUG_ACTIONS` blocks:** 49 `#ifdef/debug_print/#endif` triplets in
  scott_actions.c -> `ACT_DEBUG(...)` macro (~100 lines less). (S, none)
- [ ] `should_look_in_transcript = should_draw_image = 1;` appears 8x in
  scott_actions.c (:126,141,188,198,241,584,678,863) + scott_display.c:622
  -> `RequestRedraw()`. Single-flag variants (:110,:560,:567) stay as is.
- [ ] `OP_FORCE_CARRY` (scott_actions.c:577-586) and `OP_DESTROY_ITEM`
  (:676-680) are identical except the target location -> `MoveItem(item,
  loc)`. `OP_GET_ITEM` (:555-563) differs (carry-limit check, draw-only
  flag); leave it. Existing `PutItemAInRoomB()` (:201) calls
  `LookWithPause()` so it is not the helper to reuse.
- [ ] Lamp countdown inlined in `glk_main` (scott.c:925-945) ->
  `TickLampTimer()`.
- [ ] US sys overrides scott.c:831-848 (14 unconditional assignments + one
  `SYS_MSDOS` conditional) -> `ApplyUSSysmess()`; low value, single site.
- [ ] **detect_game.c:**
  - Two raw `if (SeekIfNeeded(...) == 0)` remain (:397, :1028) beside the
    `SEEK_OR_FAIL` macro (:806); convert them.
  - Magic `99` at :392 and :859 -> `HOWARTH_FORMAT` (already in scott.h:56).
  - `ReadHeader` (:119) hard-codes 15; `int header[24]` is defined in
    scott.c:403 but only `extern`'d and used in detect_game.c (:37,433,472,
    475). Move the definition there.
- [ ] `WriteToRoomDescriptionStream` (scott_display.c:405) has its own
  vsnprintf next to the shared `vDisplay()`; it writes to a non-Unicode
  memory stream so `vDisplay` does not fit directly. Low value.

### saga/ and ai_uk/

- [ ] Trailing-empty-image trim duplicated in c64_small.c:303-316 and
  atari8detect.c:869-878 (c64 adds a PIRATE_US duplicate step inside it);
  c64_small.c:320-323 re-inlines the existing `FreeUSImagesIfEmpty()`
  (saga.c:961). -> `FreeTrailingEmptyImage()`.
- [ ] Examine-closeup remnants: game_specific.c and hulk.c already use
  `IsPresent()` (36 + 5 uses) and `ShowCloseup()` (game_specific.c:554). The
  9+9 remaining `Items[n].Location == MyLoc` tests are room-only (a
  different predicate); a `{noun,item,mode,image}` row table could replace
  both switches. The hulk US table (hulk.c:15-63) has rows (ants, BASE/HOLE)
  with no non-US counterpart and no constant image-base offset, so it needs
  per-row pairs, not a base-offset fold. Remaining inline closeup triplets
  (game_specific.c:81-83,146-149; gremlins.c:389-391; robin:55-56) each
  differ in output details; folding them changes output, so leave them.
- [ ] Small copies: load_ti99_4a.c:86-100 `GetMaxTI99Messages`/`Items`
  (identical bodies, differ in one field + output global); load_ti99_4a.c
  :518,532,576 repeated `data_ptr -= file_baseline_offset`; load_ti99_4a.c
  :165-176 unrolled 11-clause pointer validation -> loop over an offset
  table.
- [ ] `vector_common.c:42-50 TimerDelay()` hard-codes 10/20/30 and image
  indices 80/88; line_drawing.c:214,286 call `glk_request_timer_events(20)`
  directly -> call `TimerDelay()` (returns 20 in that case, behaviour kept).
- [ ] `vector_image_shown`: move definition from ai_uk/line_drawing.c:36 to
  saga/vector_common.c (declared vector_common.h:36, used only there; removes
  the stub in saga/test/filltest.c:44).

### Tests and tooling

- [ ] `Fatal()` stub x4 (apple2test.c:33, extract_stubs.c:37, filltest.c:50,
  zxvectortest.c:64) -> one copy in test_util.h or headless_glk_stubs.c.
  (XS, none)
- [ ] **Python decoders:** dos_decode_png.py:20 and ti_decode_fb.py:34 have
  their own `nearest`/`_nearest` (different palettes; marginal);
  c64_decode_png.py:154 writes `/tmp/_c64_cmp.grid` / `/tmp/_c64a8_cmp.grid`
  -> use `tempfile`. (XS, none)

---

## 4. Table-driven opportunities

- [ ] **`ConditionHolds()`** scott_actions.c:339-484 (20 cases of `if (!pred)
  return 0`, ~145 lines of which ~90 are DEBUG_ACTIONS triplets) ->
  predicate table. (The TI99 side already translates via `ti99_conditions[]`
  and calls `ConditionHolds()`; `PerformTI99Line` is a byte-stream
  interpreter with TI99-only opcodes, so do not try to fold it into
  `PerformLine`.)
- [ ] **Per-game EXAMINE hooks** scott_actions.c:879-913 (7 cases of
  `if (vb == X_EXAMINE_VERB) XShowImageOnExamine(no)`) -> `{GameIDType,
  verb, fn}` rows; drops the 7 `*_EXAMINE_VERB` macros (scott_defines.h:103-109).
- [ ] **`PerformExtraCommand`** scott_meta.c:236-333: guards are not uniform
  (`noun == 0 || GAME`, `noun == 0 || COMMAND`, RAMLOAD/RAMSAVE, ON/OFF
  pairs, EXCEPT falls through), so a `{verb, mask, handler}` table needs two
  rows per ON/OFF verb. Judge after the EXAMINE table.
- [ ] **`games[]`** scott_game_info.c: 55 comments say `start_of_characters`
  but the field is `start_of_tiles` (scott_defines.h:505; same stale comment
  at rtpi_graphics.c:1191); file header says "gameinfo.c" (line 2); OLD_STYLE
  entries repeat values (0x2451 x4, 0x3a50 x4, 0x2539 x4, 0x28de x3, 269
  `FOLLOWS` lines in runs of 5). Designated initialisers plus family-default
  macros. (M, low, ~2900-line diff)
- [ ] `saga.c:305-380 LookUS`: the HULK_US room-alias switch (:317-339) can
  be a small table; the per-game item checks (:355-377) mix predicate kinds
  and need a `kind` column if tabled.

---

## 5. Long functions to split (do last; high risk, low return)

Measured 2026-10-11:

- [ ] `PerformLine` scott_actions.c:493-826 (334 lines) ->
  `CheckConditions()` / `ExecuteOpcode()`.
- [ ] `PerformActions` scott_actions.c:837-1069 (233): `HandleGo`,
  `RunActionTable`, `ResolveItemForVerb` (TAKE/DROP cascades); `flag == 0`
  at :932 compares an `ExplicitResultType` against a literal (`ER_SUCCESS`).
- [ ] `PerformTI99Line` ti99_4a_terp.c:64-408 (345);
  `DrawRTPIImageWithOptionalFuzz` rtpi_graphics.c:231-486 (256, one handler
  per op); `TryLoadingTI994A` load_ti99_4a.c:403-621 (219).
- [ ] `LoadDatabase` scott.c:463-624 (162), `glk_main` scott.c:748-948 (201),
  `TryLoading` detect_game.c:912-1059 (148), `CommandFromStrings`
  parser.c:1020-1138 (119).
- [ ] saga/ai_uk: `DetectApple2` apple2detect.c:706-836 (131),
  `handle_all_in_one` c64_small.c:214-362 (149), `DetectC64`
  c64decrunch.c:549-691 (143), `LoadExtraSherwoodData`
  robin_of_sherwood.c:273-422 (150), `LoadExtraSeasOfBloodData`
  seas_of_blood.c:776-894 (119), `UpdateGremlinsAnimations` gremlins.c:57-164
  (108), `LookUS` saga.c:305-420 (116), `SagaGraphicsSetup`
  sagadraw.c:143-354 (212).
- unzip_in_mem.c is vendored minizip: leave alone.

---

## 6. Global state and encapsulation

- [ ] Introduce `typedef struct GameState` (Counters, RoomSaved, BitFlags,
  CurrentCounter, SavedRoom, StopTime, JustStarted, AutoInventory...) so
  `SaveCurrentState`/`RestoreState` (restore_state.c:23,65) and
  `SaveGame`/`ReadSavedGame` (scott_meta.c:52,83) become one copy. (L)
- [ ] `lastwasnewline` is a hidden side channel: `ToUnicode` (parser.c:479,
  sets it at :497/:500) mutates it, `vDisplay` (scott.c:173-176)
  saves/restores it. Make `ToUnicode` pure.
- [ ] `split_screen`, `Top`, `Bottom`, `Graphics` are mutated from four files
  (scott.c, scott_display.c, title_image.c, game_specific.c).
- [ ] Move `battle_messages`/`enemy_table` (scott.c:126-127, sizes
  `NUM_BATTLE_MESSAGES 33`/`ENEMY_TABLE_SIZE 126` defined at scott.c:74-75)
  to seas_of_blood.c, their only user, which re-declares them with literal
  sizes 33/126 (:39,:42).
- [ ] `static char lastword[16]` in `MapSynonym` (scott.c:319-337) returns a
  pointer to static storage; not re-entrant.
- [ ] scott.c:728 redundant `if (game_file)` inside `if (argc == 2)` (:716).

---

## 7. Header hygiene

- [ ] scott.h: move `#include "glk.h"` (:129) to the top; use
  `NUM_CONDITIONS`/`NUM_EXITS` instead of literal 5/6 (:86,:92);
  declarations whose definitions live in scott_meta.c (`YesOrNo`, `HitEnter`,
  `SelectGameFromMenu`, `SaveGame`/`LoadGame`/`RestartGame`,
  `PerformExtraCommand`; scott.h:147-153,202-205) -> new `scott_meta.h`;
  `SeekToPos` (scott.h:152, defined detect_game.c:243) -> detect_game.h.
- [ ] Redundant externs: detect_game.c:36-37 (`sysdict_zx`, `header[]`;
  already declared by scott_game_info.h / detect_game.h, both included),
  parser.c:42 (`CurrentCommand`, in scott.h), scott.c:122-123 (`sysdict`,
  `sysdict_i_am`: include scott_game_info.h instead), duplicate
  `extern SavedState *InitialState;` (scott.c:141, scott_meta.c:26 ->
  declare once in restore_state.h). `screenmem` (apple2_vector_draw.c:43)
  and `DirPath` (sagagraphics.c:126) are sole declarations; give them a
  header rather than deleting them.
- [ ] parser.h exports 23 language tables; users outside parser.c are
  gremlins.c, game_specific.c, detect_game.c and scott_actions.c
  (`UnicodeWords`) -> `parser_tables.h`. Pick one of `NUMBER_OF_*` /
  `NUM_*` (parser.h, parser.c, rtpi_graphics.c use the former; scott.h,
  scott.c the latter).
- [ ] Split scott_defines.h (COND_/OP_ :31-99, TI99CND_/TI99OP_ :121-, game
  ID enum, `GameInfo`, sysmess enum :326-398) into `scott_opcodes.h`,
  `scott_sysmess.h`. Fix stale comments at :119-120/:141-142 saying COND_/OP_
  live in scott_actions.c. Move `MAX_MESSAGE_OPCODE` (:191), `MAX_TRY_DEPTH`
  (:194; both ti99-only) to a ti99 header.
- [ ] Name sizes: `NUM_SYSTEM_MESSAGES 60` (scott.c:73) vs `MAX_SYSMESS`
  (~72) are different arrays; comment which is which. Literals in
  detect_game.c: 20 (:887), 40 (:892), 6 (:896), 45 (:1039), 15 (:1451).
- [ ] `RectFill` declared in both sagadraw.h:15-16 and sagagraphics.h:55.
  Add `<stddef.h>` to sagadraw.h (uses `size_t` only transitively;
  vector_common.h already has it), then drop `-include stddef.h` from
  Makefile.headless:42 and saga/test/Makefile:108,148,248,273,284.
  `DetectRTPI` declaration (load_ti99_4a.c:118) -> rtpi_graphics.h;
  `LoadDrawingDataFromDisk` (apple2_vector_draw.h:21, plus a stray
  prototype in saga/test/filltest.c:37) -> apple2detect.h; hulk.h:15,17
  `LoadDOSImages`/`InventoryUS` -> saga.h. `RunVectorTests`,
  `ReadTestDataFromFile` (vector_common.h:17-18) are test hooks but
  `RunVectorTests` is called from scott.c:630; move both behind the same
  `TEST_HOOKS` header as the image tests.
- [ ] `OPCODE_END`/`OPCODE_MOVE_TO` are file-local in apple2_vector_draw.c:
  50-54, atari_8bit_vector_draw.c:606-610 and line_drawing.c:17-19 (no real
  clash); prefix `A2_OP_`, `A8_OP_`, `HOWARTH_OP_` for grep-ability only.
- [ ] Unneeded includes (no direct symbol use; rebuild to confirm
  transitive needs): scott_display.c `apple2draw.h`; scott_actions.c
  `<stdio.h>`; scott.c `hulk.h`; saga.c `<ctype.h>`; atari8detect.c
  `<stdbool.h>`; c64decrunch.c `<ctype.h>`/`<stdbool.h>`;
  robin_of_sherwood.c `<string.h>`; sagadraw.c
  `<math.h>`/`<stdlib.h>`/`<string.h>`; seas_of_blood.c `<ctype.h>`;
  load_ti99_4a.c `<ctype.h>`; rtpi_graphics.c `<time.h>`;
  ti99_4a_terp.c `<string.h>`.
- [ ] Guard style: bsd.h `SCOTT_BSD_H` vs lower-case elsewhere;
  atari8detect.h guard `detectatari8_h` vs `atari8detect_h` end comment.

---

## 8. Magic numbers (give names)

- [ ] scott_display.c:170-171 `32 * 8` (= `ZX_SCREEN_WIDTH` from
  decompressz80.h) and `12 * 8` (96, half-height; no existing define);
  :243,:267 `60` (graphics window proportional percentage, twice).
- [ ] title_image.c: `40/39/+2` (:365-380, RTPI title width; add
  `RTPI_TITLE_COLS` beside the existing `RTPI_TITLE_LINES`); `99` as the
  `order[]` blank-line sentinel (:232,235) -> `BLANK_LINE_MARK`. Note `99` at
  :83,:88,:324 is the US title *room image number*, a different thing.
- [ ] scott_actions.c `StopTime = 2` x3 (:618,628,661); scott.c:807-811
  status-window sizes (64/11 for TRS80 `-t`, 80/10 otherwise);
  parser.c:1292,1294 `xstrncasecmp(verbword, "restore", 7)` -> `strlen`.
- [ ] saga: `560/320` repeated at apple2detect.c:786-787 and :1064-1065
  (Apple II DHGR size); picture `98` (inventory image) at apple2detect.c:699,
  atari8detect.c:864, saga.c:430 -> `INVENTORY_IMAGE`; `0x135` at
  apple2detect.c:757 and :809 (second use bypasses the `data_start` local);
  atari_8bit_vector_draw.c:598 `if (x >= 320)` (pixel-doubled width; the
  file's `A8_SCREEN_WIDTH` is 160, so it needs its own name);
  robin_of_sherwood.c `63` means both "forest room count" and "scroll
  offset" (:162,256,294) -> name the room count; seas_of_blood.c:813,824
  `2010` used twice (reuse the `data_length` local).

---

## 9. Naming and style

- [ ] Mixed naming: `wait_for_key_on_title_screen`, `initRTPITitle`,
  `Map_Spanish` vs `MapLatin1`/`MapTI994A`, `xstrncasecmp`.
- [ ] `#pragma mark` (50x: detect_game.c 21, load_ti99_4a.c 7,
  seas_of_blood.c 6, rtpi_graphics.c 6, scott_actions.c 4,
  apple2_vector_draw.c 3, saga.c 2, atari8detect.c 1) is clang-only noise
  -> plain comments.
- [ ] Add a fallthrough annotation at detect_game.c:1383-1392
  (`SECRET_MISSION` -> `ADVENTURELAND`; the file uses `// fallthrough`
  elsewhere at :1417,1426,1429).
- [ ] Typos: `linelenghts` (title_image.c:199,217), U+2011 non-breaking
  hyphen in "Latin‑1" (parser.c:312), "a Adventure" (detect_game.c:902),
  `add_dupliceate_image_entries` (c64_small.c:177,308).
- [ ] ai_uk/sagadraw.c: one tab-indented line (:145); `pos == 0` on a
  pointer (:277).

---

## 10. Build files

- [ ] Makefile.headless `CC ?= clang`/`CXX ?= clang++` (:30-31) never take
  effect (make predefines `CC = cc`, `CXX = c++`): use `:=` or
  `ifeq ($(origin CC),default)`.
- [ ] Three tool targets (`scott_death_test` :103-104, `scott_all_audit`
  :146-147, `scott_format_probe` :155-156, plus the three `*_OBJS` lines)
  repeat one link recipe -> `define TOOL` template.
- [ ] `-include $(OBJS:.o=.d)` (:138) misses the `.d` files for
  scott_deathmain.o, death_test.o, all_audit.o, format_probe.o; CXX_OBJS
  (unp64) get no `-MMD` at all (:66). Move `.PHONY formatcheck` (:136)
  beside its rule (:160).
- [ ] Drop `-Wno-unused-function` (:43; also `-Wno-unknown-pragmas`) once
  dead code and `#pragma mark` are removed; `-w` on CXXFLAGS (:66) is for
  vendored unp64 and can stay.
- [ ] saga/test/Makefile: `-std=c11 -g -O0 -w -DSPATTERLIGHT` is still
  repeated in `CFLAGS` (:13), `EXTRACT_CFLAGS` (:148) and inline at :329-330
  -> `BASE_CFLAGS`; `EXTRACT_SRCS` (:125-147) hand-lists 48 files vs
  wildcards in Makefile.headless; the unp64 (:214-215) and `compile_each`
  (:49-53) shell loops bypass make deps; `build/scenetest` (:323) depends on
  `build/extract_images_c64` only to reuse `build/unp64/*.o` (:331) -> real
  `build/unp64/%.o` rule.

---

## Suggested order

1. Section 1 bug fixes and section 2 trivia (dead code, `static`, macro
   parentheses, leaks). Very low risk.
2. Section 7 header fixes, `BASE_CFLAGS`, section 8/9 renames.
3. `ACT_DEBUG`, `RequestRedraw`, `MoveItem`, `TickLampTimer`, detect_game.c
   leftovers, `Fatal()` stub, Python `tempfile`.
4. saga/ai_uk small dedups (empty-image trim, TimerDelay, TI99 helpers).
   Run the full test suites after each.
5. Table-driven conversions (conditions, EXAMINE, `games[]`), then the
   `GameState` struct and long-function splits.
