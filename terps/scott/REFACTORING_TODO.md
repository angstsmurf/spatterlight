# terps/scott: refactoring, cleanup and deduplication TODO

Findings from a read-only review. Nothing was compiled or changed. Dead-code
claims were grep-checked across the scott tree; items marked "unverified" are
by inspection only. Line numbers are approximate and will drift.
Effort: XS/S/M/L. Risk: none/low/medium/high.

Validation after each batch: `make -f Makefile.headless check formatcheck`,
and `make all-tests` in `saga/test` for anything touching renderers/loaders.

---

## 1. Bugs and risks (fix first)

- [ ] **`BitFlags` is `long` but used as 64-bit.** scott.h:144, scott.c:98,
  restore_state.h; shifted as `uint64_t` in scott_actions.c:148,155,413,420.
  On LLP64 `long` is 32-bit. scott_meta.c:51,86 save/load with `%ld`;
  scott_display.c:161 and scott_meta.c:52 use `(1 << DARKBIT)` (int shift).
  Make it `uint64_t`, add `TestBitFlag(int)` beside Set/ClearBitFlag.
  Watch the save-file format. (S, low-medium)
- [ ] **Typo'd macro `TI99OP_DRTI99OP_ITEM`** (scott_defines.h:151, used in
  ti994a/ti99_4a_terp.c:303); should be `TI99OP_DROP_ITEM`. (XS, none)
- [ ] **Variable `seTI99CND_param`** in ti994a/ti99_4a_terp.c:49,360-408 is a
  bad find/replace of `second_param`. Rename. (XS, none)
- [ ] **`LoadExtraGermanGremlinsC64Data()` called twice** for
  `GREMLINS_GERMAN_C64` in detect_game.c (~1560 and ~1620). Confirm and drop one.
- [ ] **`-w` option has no effect:** scott.c:707 sets `split_screen = 0`, but
  glk_main forces it to 1 (scott.c:807,811). Honour it or delete the option,
  the `else` at scott.c:819-821 and the dead assignments. `-f` and `-z` are
  handled (scott.c:713,716) but missing from `glkunix_arguments`.
- [ ] **`GetRTPILines` (title_image.c:365-410):** unbounded write to
  `line[128]` (:378); `lines[]` entries not in `order[]` never freed;
  `outlines` never freed (:456); `linelenghts[]` write-only. `title_screen` is
  freed at :457,552,590 but never set to NULL.
- [ ] Redundant free in parser.c:1248-1250 (`FreeStrings()` already frees and
  NULLs `FirstErrorMessage`).
- [ ] **Leaks / unchecked reads in saga and ai_uk:**
  - `RunAtari8bitVectorTests` (atari_8bit_vector_draw.c:1017-1041) leaks `arg`
    on early returns, no NULL check on `ReadTestDataFromFile`.
  - `CompareA2ScreenMemory` (apple2_vector_draw.c:729) leaks `path`, reads
    `A2_SCREEN_MEM_SIZE` without checking `size`.
  - `DetectAtari8` (atari8detect.c:1086-1095) leaks `companionfile` when the
    DB lookup fails. `DetectApple2` (~773) returns 0 early after allocating
    `datafile`, `companionfile`, `invimg`.
  - `gli_slowdraw` fallback defaults to 0 in apple2_vector_draw.c:148 but 1 in
    line_drawing.c:123.
  - c64decrunch.c:~741 leaves a `C64_DUMP_US_IMAGE` env-var dump in
    production code, and `writeToFile` opens with `"w"` instead of `"wb"`.
- [ ] **rtpi_graphics.c:42-63 macros lack parentheses** (`RTPI_IMAGE_SIZE
  32 * 12 * 8`, `GROM_SIZE`, `GROM_FLAT_SIZE`, `NUMBER_OF_RTPI_ROOM_IMAGES
  24 + 9`, `NUMBER_OF_RTPI_IMAGES`). Latent precedence bugs. `op > 7` at ~295
  is dead (3-bit value).
- [ ] **Possible data typos to check against game data:**
  - game_specific.c:~763 Strange Odyssey `case 58`: `Items[44]` vs `Items[4]`.
  - game_specific.c:~618 Voodoo `case 7` missing `break` (harmless today).
  - robin_of_sherwood.c:357/396 loops 26 over a 28-entry key list (drops 2
    keys); Spectrum key list has `OK, OK` and `YOU_SEE, YOU_SEE` duplicated;
    C64 key list `HIT_ENTER` at index 9 is overwritten right after.
  - sagadraw.c:~240/~290 Hulk offset table indexes `(i - 10)` for pictures
    10..27 read as items elsewhere.
  - scott_actions.c `COND_IN_PLAY` checks `dv > NumItems + 1` while other
    conditions index `Items[dv]` unchecked.

---

## 2. Dead code (verified)

- [ ] Delete `InitTitleImage()` title_image.c:40-76 (no callers, no decl).
- [ ] Delete `xstrcasecmp()` bsd.c:39-48 / bsd.h:6 (only `xstrncasecmp` used).
- [ ] Delete `AnimationFlag` scott.c:132.
- [ ] Delete dead store `list = SkipList;` parser.c:~972 (`FindExtraneousWords`).
- [ ] Delete `UNKNOWN_ACTIONS_TYPE` scott_defines.h:458 (unused).
- [ ] Simplify `buf = NULL;` / `if (buf != NULL)` before free, scott_display.c:142-145.
- [ ] Merge `case OP_LOOK: break;` and `case 0: /* NOP */` in scott_actions.c.
- [ ] Delete whole file `ai_uk/debug.c` (no callers, no header) and the
  `filter-out` in Makefile.headless:44-45.
- [ ] Delete `errorcount` (sagadraw.c:35), `circular_buf_reset` and
  `circular_buf_full` (ringbuffer.c:45,60), `ATARI8_PLANE_SIZE`
  (atari_8bit_vector_draw.h:21; same value as `A8_IMAGE_SIZE`),
  `color_at_byte_slot` (atari_8bit_vector_draw.c:394, unverified: compile with
  `-Wunused-function`), declarations of `FindUSImageDataInDisk` /
  `CompareUSImagesWithDisk` (rtpi_graphics.h:15-16, never defined), unused
  `CASESENSITIVITY` / `WRITEBUFFERSIZE` / `<ctype.h>` in unzip_in_mem.c.
- [ ] Unused `img` parameter of `init_a2_vector_draw_session` (apple2:666) and
  `init_a8_vector_draw_session` (atari:870).
- [ ] seas_of_blood.c: `#define ERROR 99` (one use), orphan comment :120-122,
  `//#define AUTOWIN` at :688 (test Makefile uses `-DAUTOWIN`; document there).
- [ ] Commented-out code: `//#define DEBUG_ACTIONS` (scott_actions.c:37),
  `PrintTI99HeaderInfo` (load_ti99_4a.c:102-114), apple2_vector_draw.c:540
  `fprintf`, unzip_in_mem.c:85.
- [ ] Move 28-line comment table at title_image.c:412-439 next to `order[]`.

### Make file-local symbols `static` (zero risk, but check Xcode/ObjC callers)

- [ ] title_image.c: `ResizeTitleImage`, `wait_for_key_on_title_screen`,
  `GetRTPILines`, `initRTPITitle`, `RTPITitle`.
- [ ] parser.c: `SplitIntoWords`, `LineInput`; `ExtraCommandsKey`,
  `Abbreviations`, `AbbreviationsKey` -> `static const`.
- [ ] scott.c `RunTests`; scott_display.c `DrawRoomImage` (+ drop decl in
  scott_display.h); scott_actions.c `ClearScreen` (+ drop decl).
- [ ] saga.c: `ReadUSDictionary`, `DrawApple2Image`, `DrawDOSImage`,
  `DrawUSImage`, `DrawInventoryImages`, `LoadHeader`, `ParseDictionary`,
  `HulkUKSpecificParsing`, `AddGameFileExtension`, `DrawRoomObjectImages`,
  `CompareFilenames`, `LookForCompanionFilenameInDatabase` (the last three
  are in saga.h).
- [ ] apple2: `CompareA2ScreenMemory`, `TestApple2ImageWithName` (test hooks:
  put behind a `TEST_HOOKS` header/#ifdef); atari: `DrawSingleAtari8ImageByte`,
  `ReadAndDrawImageWithName`, `TestAtari8ImageWithName`, `delay_active`,
  `PatchMissionImpossible`.
- [ ] apple2detect.c: `LookForA2CompanionFilename`, `ReadA2DiskImageFile`.
- [ ] c64decrunch.c: `checksum`, `LoadC64USImages`, `LookForSoCCompanion`,
  `writeToFile`. hulk.c `HulkShowImageOnExamineUS`; line_drawing.c
  `FreePixels`; robin_of_sherwood.c:191 `animate_lightning` (remove redundant
  non-static definition).
- [ ] seas_of_blood.c: `blood_battle`, `PatchCryptImage`, dice/background
  globals (:35-36,52,~126); only `blood_image_data` is in the header.
- [ ] load_ti99_4a.c `max_messages`, `max_item_descr`; `const static` ->
  `static const` (rtpi_graphics.c:~69).

---

## 3. Duplication

### Core (scott*.c, parser.c, detect_game.c)

- [ ] **ExtraCommands tables** parser.c:74-105 (EN), 107-142 (DE), 144-179
  (ES): entries 0-25 and `"#flicker"` are identical. Shared base macro/array
  plus per-language tails (~55 lines). gremlins.c copies these; keep externs. (S, low)
- [ ] **`FreeDatabase`** scott.c:405-465: five identical free loops ->
  `FreeStringArray(char **, count)` helper. (S, low)
- [ ] **`WriteToLowerWindow`** scott_display.c:560-574 equals
  `Display(Bottom, ...)` (scott.c:160-176) minus the `lastwasnewline`
  save/restore. Remove it. Share a `vformat` helper with
  `WriteToRoomDescriptionStream` (:387-399).
- [ ] Room-description buffer setup repeated at scott_display.c:151-153 and
  475-477 -> `BeginRoomDescription()`.
- [ ] `ListExitsSpectrumStyle` / `ListExits` (scott_display.c:402-448): merge;
  also `(&Rooms[MyLoc])->Exits` -> `Rooms[MyLoc].Exits`.
- [ ] Item-list loops in `Look` (:516-539) and `ListInventory` (:587-607).
- [ ] `OpenGraphicsWindow` scott_display.c:211-264 repeats size/offset code in
  both branches (cast differs: :230 vs :257) -> `FitGraphicsWindow()`.
- [ ] Title-window teardown/rebuild: title_image.c:310-326, 480-500; restore
  windows :341-350, 528-537 -> `OpenTitleGraphicsWindow()`,
  `RestoreGameWindows()`. Key-wait loops :78-112 and :236-259 share a body
  (pass the redraw callback).
- [ ] Extra-noun lookup duplicated: scott_meta.c:247-260 and parser.c:1349-1363
  -> `ResolveExtraNoun()` in parser.c.
- [ ] ALL-then-EXCEPT lookahead parser.c:1108-1117 / 1141-1149 ->
  `IsAllFollowedByExcept()`. Noun-list test at 1075 and 1099 ->
  `IsNounList()`.
- [ ] **`DEBUG_ACTIONS` blocks:** ~45 `#ifdef/debug_print/#endif` triplets in
  scott_actions.c -> `ACT_DEBUG(...)` macro (~150 lines less). (S, none)
- [ ] `should_look_in_transcript = should_draw_image = 1;` appears 12x in
  scott_actions.c (+3 scott.c, scott_meta.c:147) -> `RequestRedraw()`.
- [ ] `OP_GET_ITEM` / `OP_FORCE_CARRY` / `OP_DESTROY_ITEM` share the "if item
  in room, flag redraw, set location" idiom -> `MoveItem(item, loc)`.
- [ ] `UpdateSettings` scott.c:181-215: two identical tri-state switches.
- [ ] Lamp countdown inlined in `glk_main` (scott.c:943-967) ->
  `TickLampTimer()`; the `LIGHT_SOURCE == CARRIED || == MyLoc` test repeats.
- [ ] US sys overrides scott.c:831-851 (13 assignments) -> table or
  `ApplyUSSysmess()`.
- [ ] `gli_slowdraw` save/zero/restore x3 (scott.c:256-275, title_image.c:96-105,
  :333, scott_actions.c:93) -> `WithSlowDrawOff()` helper.
- [ ] **detect_game.c:**
  - `for (i = A; i <= B; i++) sys[i] = system_messages[off - A + i];` x12
    (:1516-1620) -> `MapSystemMessages(first, last, src)`; name offsets.
  - `if (SeekIfNeeded(...) == 0) return UNKNOWN_GAME;` x22 -> macro.
  - `TryLoadingOld` (:789-890) and `TryLoading` (:896-1115) share stages.
  - Magic `99` (:834,857,1065) -> `HOWARTH_FORMAT` (already in scott.h).
  - `ReadHeader` (:116-125) hard-codes 15; `header[24]` is defined in scott.c:400
    but belongs in detect_game.c.
- [ ] **TI99/RTPI/SAGA header setup:** load_ti99_4a.c:439-470,
  rtpi_graphics.c:1366-1389, saga.c `LoadHeader`:490: same ~20 lines ->
  `AllocateGameFromCounts()`. (M, medium)

### saga/ and ai_uk/

- [ ] **Slow-draw op-buffer machinery x3:** `ensure_capacity`,
  `shrink_capacity`, `FreeOps`, session init at apple2_vector_draw.c:157-198
  (init 666-677), atari_8bit_vector_draw.c:114-153 (init 870-881),
  line_drawing.c:62-109 (growth rule differs: no `MAX(..., total+1)` guard,
  int vs size_t). Shared growable array in `saga/vector_common.c`. Also share
  the "finished" tail of `DrawSome*VectorBytes`, the `DrawingXxxVector()`
  one-liners, and the `IMG_ROOM/SHOWING/bytes_to_write==NULL` preamble
  (`BeginVectorSession`). (M, medium; byte-exact renderers)
- [ ] **Apple2/Atari companion-disk detection:** apple2detect.c:894-964 vs
  atari8detect.c:756-825 (identical A/B/1/2/one/two switch),
  `GetApple2CompanionFile` (:966-1027) vs `GetAtari8CompanionFile` (:827-882)
  (identical scanner), `StripTrailingParenthetical` vs `StripBrackets`, and
  the swap-sf-with-companion block (apple2:728-742, atari:1070-1078). Move to
  saga.c with `ClassifyCompanion`, `ApplyCompanionType`, `StripDelimited`. (M)
- [ ] Image extraction loops in apple2detect.c, atari8detect.c,
  c64_small.c:~236-362 -> `AppendUSImage()`, `FreeTrailingEmptyImage()`
  (also atari8detect.c:1008-1020).
- [ ] **12 `SysMessageType messagekey[]` tables** with hand-counted loop
  bounds: game_specific.c:153/198, 210/256, 271/313, 323/366, 376/419,
  481, 524/566; gremlins.c:244/275, 352/378; robin_of_sherwood.c:357/396;
  seas_of_blood.c:831 -> `ApplySysKeys(keys, n, first)` using `sizeof`.
  Diff resulting `sys[]` before/after: the bound mismatches may be load-bearing.
- [ ] **Examine-closeup tables:** game_specific.c:586-805 and hulk.c:14-60,
  70-105 repeat `Items[n].Location == MyLoc || == CARRIED` ~45x -> `ItemHere()`
  / `ItemCarriedOrHere()` in scott.h, or `{noun,item,mode,image}` rows. Fold
  `HulkShowImageOnExamineUS` and the non-US table (different image base only).
  The "draw closeup, `showing_closeup=1`, HIT_ENTER, `HitEnter()`" triplet
  (hulk.c:61-70,110-116,178-186; game_specific.c:575-582,79,146;
  gremlins.c:389; robin_of_sherwood.c:55) -> `ShowCloseupAndWait(image)`.
- [ ] Small copies: atari `DrawByteAt` (:599-611) vs `DrawSingleByteAt`
  (:791-804); sagadraw.c:~122-148 Claymorgue C64/ZX patch functions;
  sagadraw.c:~55-75 duplicate SECRET_MISSION / _C64 entries;
  load_ti99_4a.c:86-100 `GetMaxTI99Messages`/`Items`; load_ti99_4a.c:418-420
  repeated `data_ptr -= file_baseline_offset`; load_ti99_4a.c:~145-160
  unrolled 11-clause pointer validation; sagagraphics.c:41-62 A2/A8 branches
  -> `VectorKickoff()`.
- [ ] `vector_common.c:46-53 TimerDelay()` hard-codes 20/10/30 and images
  80/88; line_drawing.c:~148,~246 hard-code 20 -> call `TimerDelay()`.
- [ ] `vector_image_shown`: move definition from line_drawing.c:38 to
  vector_common.c (layering; removes the stub in filltest.c:41-46).

### Tests and tooling

- [ ] **saga/test C boilerplate:** `read_file` copied 9x (apple2test.c:33,
  c64a8test.c:92, c64extract.c:32, c64test.c:103, dostest.c:58, titest.c:51,
  filltest.c:88, zxtest.c:78, zxvectortest.c:75; production
  `ReadFileIfExists` already exists); `plot/dump_ascii/write_grid/
  compare_golden` + C64C2 parser copied across c64test, c64a8test, dostest,
  zxtest, zxvectortest; `Fatal()` stub x4 (apple2test.c:31, filltest.c:48,
  zxvectortest.c:69, extract_stubs.c:46); `win_testresult` in both
  extract_stubs.c:57 and test/headless_stubs.c:29. -> `harness_common.[ch]`
  plus `headless_glk_stubs.c`. (M, low)
- [ ] **Python decoders:** c64_decode_png.py:21-100 vs c64a8_decode_png.py:24-119
  share `load_png`, `nearest`, `renderer_colours`, `build_golden`,
  `write_golden`; dos_decode_png.py and ti_decode_fb.py have own `nearest`.
  -> `groundtruth_lib.py`, merge c64/c64a8 with `--edges`; use `tempfile`
  instead of `/tmp/_c64_cmp.grid`. (S, low)

---

## 4. Table-driven opportunities

- [ ] **Condition switch** scott_actions.c:361-480 (20 cases of
  `if (pred) return ACT_FAILURE`) -> predicate table (~120 lines -> ~40).
  Unify `TI99CND_*`/`COND_*` and `TI99OP_*`/`OP_*` (scott_defines.h) with a
  canonical enum + translation table so ti99_4a_terp.c can reuse
  `PerformLine`.
- [ ] **Per-game EXAMINE hooks** scott_actions.c:~875-910 (7 cases) ->
  `{GameIDType, verb, fn}` rows; drops the 7 `*_EXAMINE_VERB` macros
  (scott_defines.h:103-109).
- [ ] **`OP_GAME_SPECIFIC`** scott_actions.c:~690-735 (15 cases) -> rows.
- [ ] **`PerformExtraCommand`** scott_meta.c:264-338: repeated
  `noun == 0 || noun == X` guard -> `{verb, noun mask, handler}`; centralise
  the scattered `verb -= NumWords`.
- [ ] **`ParseHeader`** detect_game.c:~267-420: 150-line switch assigning the
  same 11 outputs from permuted `h[]` indices -> field table
  `{idx, shift, mask}` per header type, return a struct instead of 12
  out-params (also `SetGameHeader`, `PrintHeaderInfo`). (M, medium: verify
  against every `games[]` entry)
- [ ] `GetListAndLength` parser.c:814-846 -> array (minor).
- [ ] **`games[]`** scott_game_info.c (~2640 lines of positional
  initialisers): 55 comments say `start_of_characters` but the field is
  `start_of_tiles`; file header says "gameinfo.c"; many OLD_STYLE entries
  repeat values (0x2451, 0x3a50, 0x2539 x4, 0x28de, runs of 5x `FOLLOWS`).
  Use designated initialisers plus family defaults macros;
  `number_of_nouns/verbs` may be derivable. (M, low, large diff)
- [ ] `saga.c:310-340 LookUS`: per-game item/room numbers -> table.

---

## 5. Long functions to split (do last; high risk, low return)

- [ ] `PerformLine` scott_actions.c:331-810 (~480 lines) -> `CheckConditions()`
  / `ExecuteOpcode()`.
- [ ] `PerformActions` scott_actions.c:812-1048 (~235): `HandleGo`,
  `RunActionTable`, `ResolveItemForVerb` (TAKE/DROP cascades ~975-1010);
  `flag == 0` compared against an enum at ~914.
- [ ] `PerformTI99Line` ti99_4a_terp.c:39-528 (~490); `DrawRTPIImage-
  WithOptionalFuzz` rtpi_graphics.c:251-506 (~255, one handler per op);
  `TryLoadingTI994A` load_ti99_4a.c:426-673.
- [ ] `DetectGame`, `TryLoading` (~220 each), `LoadDatabase` scott.c:470-640,
  `glk_main` scott.c:766-966, `SplitIntoWords` parser.c:618-736 (`goto done`,
  fallthrough 'd'/'m' -> 'y'), `CommandFromStrings` parser.c:1030-1160.
- [ ] saga: `DetectApple2` (153), atari8detect.c:884-1026, c64_small.c:214-362,
  c64decrunch.c:563-705, robin_of_sherwood.c:294-447, seas_of_blood.c:775-897,
  gremlins.c:56-163, `LookUS` (115), `SagaGraphicsSetup` sagadraw.c:150-366.
- unzip_in_mem.c is vendored minizip: leave alone.

---

## 6. Global state and encapsulation

- [ ] Introduce `typedef struct GameState` (Counters, RoomSaved, BitFlags,
  CurrentCounter, SavedRoom, StopTime, JustStarted, AutoInventory...) so
  `SaveCurrentState`, `RestoreState`, `SaveGame`, `ReadSavedGame` become one
  copy; removes hard-coded 16s (restore_state.c:32,73; restore_state.h:14-15)
  in favour of `NUM_COUNTERS`. (L)
- [ ] Globals used by one file only should leave scott.h: `game_file`,
  `last_image_index`; `showing_closeup`, `CurrentSys`, `showing_inventory`
  (two files each).
- [ ] `lastwasnewline` is a hidden side channel: `ToUnicode` (parser.c:~472)
  mutates it, `Display` (scott.c:170-174) saves/restores. Make `ToUnicode`
  pure.
- [ ] `split_screen`, `Top`, `Bottom`, `Graphics` mutated from 5 files.
- [ ] Move `battle_messages`/`enemy_table` (scott.c:126-127) to
  seas_of_blood.c (which re-declares them with hard-coded sizes 33/126:38,41)
  and `system_messages` (scott.c:124) to detect_game.c.
- [ ] `static char lastword[16]` in `MapSynonym` (scott.c:313-333) returns a
  pointer to static storage; not re-entrant.
- [ ] Scott.c:746 redundant `if (game_file)` inside `if (argc == 2)`;
  `LineInput` (parser.c:738-772) redundant loop condition.

---

## 7. Header hygiene

- [ ] scott.h: move `#include "glk.h"` (mid-file ~106) to the top; use
  `NUM_CONDITIONS`/`NUM_EXITS` instead of literal 5/6 (and 2 for Opcode);
  declarations whose definitions live in scott_meta.c (`YesOrNo`, `HitEnter`,
  `SelectGameFromMenu`, `SaveGame`/`LoadGame`/`RestartGame`,
  `PerformExtraCommand`) -> new `scott_meta.h`; `SeekToPos` -> detect_game.h.
- [ ] Remove redundant externs: restore_state.c:16-20 (`CurrentCounter`,
  `RoomSaved`, `StopTime`, `JustStarted`), detect_game.c:37 (`header[]`) and
  :36 (`sysdict_zx`), scott.c:120-121 (`sysdict`, `sysdict_i_am`: include
  scott_game_info.h), parser.c:41 (`CurrentCommand`), duplicate
  `extern SavedState *InitialState;` (scott.c:141, scott_meta.c:26 ->
  restore_state.h), apple2_vector_draw.c:42 `screenmem`, sagagraphics.c:131
  `DirPath`.
- [ ] parser.h exports 20+ language tables only parser.c and gremlins.c need
  -> `parser_tables.h`. Pick one of `NUMBER_OF_*` / `NUM_*`.
- [ ] Split scott_defines.h (opcodes COND_/OP_/TI99*, game ID enum, `GameInfo`,
  sysmess enum) into `scott_opcodes.h`, `scott_sysmess.h`. Fix stale comments
  at :129/:139 pointing at scott_actions.c. Move `MAX_MESSAGE_OPCODE`,
  `MAX_TRY_DEPTH` (ti99-only) to a ti99 header.
- [ ] Name sizes: `NUM_SYSTEM_MESSAGES 60` (scott.c:73) vs `MAX_SYSMESS`;
  literals 20/40/45/6/15 at detect_game.c:871-875.
- [ ] sagadraw.h:9-10 redeclares `RectFill`; add `<stddef.h>` to sagadraw.h and
  vector_common.h (also uses `uint8_t`), then drop `-include stddef.h` from
  both Makefiles. `DetectRTPI` declaration (load_ti99_4a.c:118) ->
  rtpi_graphics.h; `LoadDrawingDataFromDisk` -> apple2detect.h;
  hulk.h:17 `LoadDOSImages`/`InventoryUS` -> saga.h. Move `RunVectorTests`,
  `ReadTestDataFromFile` out of production vector_common.h.
- [ ] Opcode name clashes: `OPCODE_END`/`OPCODE_MOVE_TO` in apple2, atari
  and line_drawing.c:15-17 -> prefix `A2_OP_`, `A8_OP_`, `HOWARTH_OP_`.
- [ ] Probably unneeded includes (unverified; delete and rebuild):
  scott_display.c (`apple2draw.h`, `irmak.h`, `line_drawing.h`, `<stdlib.h>`),
  scott_actions.c (`<stdio.h>`, `robin_of_sherwood.h`, `gremlins.h`),
  scott.c (`hulk.h`, `gremlins.h`, `robin_of_sherwood.h`, `rtpi_graphics.h`),
  saga.c `<ctype.h>`, atari8detect.c `<stdbool.h>`, c64decrunch.c
  `<ctype.h>`/`<stdbool.h>`, robin_of_sherwood.c `<string.h>`, sagadraw.c
  `<math.h>`/`<stdlib.h>`/`<string.h>`, seas_of_blood.c `<ctype.h>`,
  load_ti99_4a.c `<ctype.h>`, rtpi_graphics.c `<time.h>`,
  ti99_4a_terp.c `<string.h>`.
- [ ] Guard style: bsd.h `SCOTT_BSD_H` vs lower-case elsewhere;
  atari8detect.h guard `detectatari8_h` vs `atari8detect_h` end comment.

---

## 8. Magic numbers (give names)

- [ ] scott_display.c:166-167 `32 * 8`, `12 * 8` (use `ZX_*` from
  decompressz80.h); :227,253 `60`; Delay constants :138.
- [ ] title_image.c: `0x4000`/`0x5800` (:124,143,151,162,212; add
  `ZX_ATTR_BASE`), `40/39/+2` (:568-582), `99` (:399, `BLANK_LINE_MARK`).
- [ ] scott_actions.c `StopTime = 2` x4 and `Delay(...)`; scott_meta.c:387 `35`;
  scott.c:822,830 (64/11/80/10); parser.c:1304-1306 `7` (use `strlen`).
- [ ] saga: `560/320`, `280/158`, `320/96`, picture 98 (inventory background),
  `0x988e`, `0x5fd`, `0xb390`, `0xb410`, `0x135`, `0x3d00`, `0x3803`;
  atari_8bit_vector_draw.c:~607 `320`; robin_of_sherwood.c 63/61/555;
  seas_of_blood.c 2010; scott_game_info.c hex values.

---

## 9. Naming and style

- [ ] Mixed naming: `wait_for_key_on_title_screen`, `initRTPITitle`,
  `Map_Spanish` vs `MapLatin1`/`MapTI994A`, `xstrncasecmp`.
- [ ] `#pragma mark` (25x, 21 in detect_game.c, plus load_ti99_4a.c) is
  clang-only noise -> plain comments.
- [ ] Add fallthrough annotations: parser.c:921-934, parser.c 'd'/'m' -> 'y',
  detect_game.c `SECRET_MISSION` -> `ADVENTURELAND`.
- [ ] Typos: `linelenghts` (title_image.c:367), non-ASCII hyphen in "Latin-1"
  (parser.c:320), "a Adventure" (detect_game.c:~880), `add_dupliceate_image_entries`
  (c64_small.c:177).
- [ ] sagadraw.c mixed tabs/spaces (:151), `pos == 0` on pointer.
- [ ] Core: `x_offset` `(int)` cast inconsistency scott_display.c:230 vs :257.

---

## 10. Build files

- [ ] Makefile.headless `CC ?= clang`/`CXX ?= clang++` (:29-30) never take
  effect (make predefines them): use `:=` or `ifeq ($(origin CC),default)`.
- [ ] Three tool targets (`scott_death_test`, `scott_all_audit`,
  `scott_format_probe`, :76-82,121-126,132-137) repeat one link recipe ->
  `define TOOL` template.
- [ ] `-include $(OBJS:.o=.d)` (:114) misses `.d` for scott_deathmain.o and
  test objects; move `.PHONY formatcheck` beside its rule.
- [ ] Drop blanket `-w`/`-Wno-unused-function` once dead code is removed.
- [ ] saga/test/Makefile: `-std=c11 -g -O0 -w -DSPATTERLIGHT -include
  stddef.h` pasted 8x (:96,102,128,171,188,194,215,246) -> `BASE_CFLAGS`;
  golden-runner shell loop copied 7x (dos, apple2, ti, c64, c64a8, zx, scene)
  -> `define RUN_GOLDENS` or `run_goldens.sh`; `EXTRACT_SRCS` (:111-129)
  hand-lists ~45 files vs wildcards in Makefile.headless; unp64 compile
  shell loops bypass make deps; `build/scenetest` depends on
  `build/extract_images_c64` only for unp64 objects -> real
  `build/unp64/%.o` rule.

---

## Suggested order

1. Section 1 bug fixes and section 2 trivia (dead code, `static`, macro
   parentheses, leaks). Very low risk.
2. Section 7 header fixes, `BASE_CFLAGS`, section 8/9 renames.
3. Test and Python dedup, `ApplySysKeys`, `ACT_DEBUG`, `RequestRedraw`,
   ExtraCommands base, `FreeDatabase`, display helpers.
4. Renderer/loader dedup (vector buffer, companion detection, header setup,
   closeup tables). Run the full test suites after each.
5. Table-driven conversions (`ParseHeader`, conditions, EXAMINE,
   `games[]`), then `GameState` struct and long-function splits.
