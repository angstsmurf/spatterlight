# ScottFree: structure TODO

Recommendations from a read-only survey on 2026-11-16 (master at 4ac564bce).
Line numbers are from that commit. A1 to A6 and B are done (uncommitted
working tree, same day); see Status. Everything else is still open.

## Status

Done, as pure moves, with every check of section 0 passing afterwards:

| Item | Result |
|---|---|
| A1 | `detect_game.c` keeps `DetectGame`. New `uk_loader.c/.h` (UK binary loader) and `zxdetect.c/.h` (snapshots, Parsec, Alkatraz). |
| A2 | `saga/saga.c/.h` renamed `saga/us_loader.c/.h`. New `saga/us_display.c/.h`, `saga/us_companion.c/.h`, `saga/dos_images.c/.h`. The two image-freeing helpers went to `sagagraphics.c`. |
| A3 | New `scottfree_loader.c/.h` (plain-text loader) and `scott_main.c` (options, `glk_main`). `scott.c` keeps the globals, settings, output, `SetUpGame` and `PlayTurn`. |
| B | New `scott_database.c/.h`. |
| A4 | New `ti994a/rtpi_loader.c/.h`; `rtpi_graphics.c` is the decoder only. |
| A5 | New `ai_uk/secret_mission.c/.h`, `adventureland.c/.h`, `sysmess_maps.c/.h`, `closeups.c/.h`. `SetParserWordLists` is in `parser.c`. `game_specific.c` holds the two colour functions. |
| A6 | `ai_uk/c64decrunch.c/.h` renamed `saga/scott_c64detect.c/.h`. |

Where the result differs from the text below:

- Names are `scott_main.c`, `scott_database.c` and `scott_c64detect.c`, not
  `main.c`, `database.c` and `c64detect.c`, because those names exist in
  other interpreters (see D3).
- The `extern` declarations of the database arrays are still in `scott.h`;
  `scott_database.h` declares the functions only. Moving them is part of D1.
- `FreeGameResources()` is still there (B said it can go). Removing it is a
  code change, not a move.
- These lost `static` to cross the new file boundaries: `forced_style`,
  `SelectSystemMessages`, `SetUpGame`, `PlayTurn` (`scott.h`),
  `DetectZXSpectrum` (`zxdetect.h`), `rtpi_tile_font`, `SetupRTPIColors`
  (`rtpi_graphics.h`).
- A6: the disk menus are still in `scott_c64detect.c`, and the two
  `SCOTT_HEADLESS` debug dumps are untouched.

Still to do outside this folder: the Gargoyle fork's `terps/CMakeLists.txt`
lists the ScottFree sources by name and needs the new and renamed files at the
next sync.

The code inside most files is in good shape after the comment and licence
passes. What is unclear is where things live: file and folder names no longer
say what is in them, four database loaders are spread over five files, and a
new game has to be wired into switches in six places. The items below are
ordered by value for effort. Sections A to C are pure moves of code between
files; D onwards change declarations or control flow.

## 0. Safety net

Run before and after every item. All of these exist today.

- `make -f Makefile.headless check formatcheck companioncheck scriptcheck savecheck`
- `cd saga/test && make test c64a8test scenetest` (plus the other picture
  targets listed in `saga/test/README.md`)
- `xcodebuild -scheme Spatterlight build`, because the app build is the only
  one that uses Xcode's header map and the `SPATTERLIGHT` branches.
- For pure moves, an object-level check is cheap: the set of global symbols
  over all objects (`nm -g`) must be the same before and after, apart from
  statics that had to become global.

A moved or new file has to be registered in four places: the Xcode project
(groups `scott`, `saga`, `ai_uk`, `ti994a`), `Makefile.headless` (`SRCS`
wildcards, `VPATH`, `-I`), `saga/test/Makefile` (explicit source lists and
`vpath`) and the Gargoyle fork's CMake list. This cost is why the file splits
come before any folder moves.

## A. Split the files that hold several subsystems

### A1. `detect_game.c` (1603 lines) is mostly not detection

It holds three things:

| Lines | What | Proposed home |
|---|---|---|
| 86–1082 | The binary database loader for the UK releases (`ReadHeader`, `ParseHeader`, `ReadDictionary`, `TryLoadingOld`, `TryLoading`, …) | `uk_loader.c` / `uk_loader.h` |
| 1084–1368 | ZX Spectrum containers: `IsMysterious`, `DecompressParsec`, `DeAlkatrazTape`, `TryGamesWithDictionary`, `DetectZXSpectrum` | `zxdetect.c`, next to its C64, Atari and Apple II siblings |
| 1370–1603 | `SetUpGameSpecifics`, `DetectGame` | stays, about 250 lines |

`detect_game.h` then shrinks to `DetectGame()`. Today it also exports loader
internals (`SeekIfNeeded`, `ReadHeader`, `ParseHeader`, `PrintHeaderInfo`,
`SeekToPos`, the global `int header[24]`), and it includes all of `scott.h`,
so every file that only wants to load a table sees everything.

### A2. `saga/saga.c` (1075 lines) is four things

| Lines | What | Proposed home |
|---|---|---|
| 66–116 | `LoadDOSImages` | with the MS-DOS picture code, e.g. `dosdetect.c` |
| 162–492 | Drawing a US room: `DrawUSRoom`, `DrawGameSpecificOverlays`, `LookUS`, `InventoryUS` | `us_display.c` |
| 494–926 | The US binary database loader (`LoadBinaryDatabase`, `Parse*`) | `us_loader.c` |
| 928–1075 | Companion disk file names (`LookInDatabase`, `SwapWithCompanionFile`, …) | `us_companion.c`, or into `common_utils/companion_search.c` if Plus can share it |

`saga.h` mixes the same four groups and should split the same way.

### A3. `scott.c`: move the plain-text loader and the main program apart

- Lines 403–700 (`ReadString` … `LoadDatabase`) are the ScottFree `.dat`
  loader: move to `scottfree_loader.c`.
- `glkunix_arguments`, `ParseOptions`, `glkunix_startup_code` and `glk_main`
  could go to a small `main.c`. The tools in `Makefile.headless` could then
  leave that one file out, in place of compiling `scott.c` a second time
  with `-Dglk_main=scott_real_glk_main`.
- What stays is the turn loop, settings and the event handler.

### A4. `ti994a/rtpi_graphics.c` (1286 lines) is half loader

Lines 106–615 draw pictures. Lines 616–1286 load Return to Pirate's Isle:
picture extraction from cartridge, disk and zip (`LoadRTPIGraphics`,
`load_rtpi_zip_payload`, `load_rtpi_disk_payload`), the dictionary
(`ReadRTPIDictionary`), the database (`LoadRTPI`, `DetectRTPI`) and the system
messages. Split into `rtpi_graphics.c` and `rtpi_loader.c`.

### A5. `ai_uk/game_specific.c` (749 lines) is a drawer of leftovers

| Lines | What | Proposed home |
|---|---|---|
| 32–59 | `WhiteColour`, `BlueColour` | with the palette users, or keep |
| 61–198 | Secret Mission and Adventureland opcode handlers and animations | `secret_mission.c`, `adventureland.c`, like `gremlins.c` and `hulk.c` |
| 200–599 | System message mapping: `MapSysMessages`, `MapSysRange` and eight `*Sysmess()` functions | `sysmess_maps.c` |
| 217–279 | `SetParserWordLists` | `parser.c`, see D2 |
| 601–749 | The close-up table | `closeups.c` / `closeups.h` |

### A6. `ai_uk/c64decrunch.c` (875 lines) is misnamed and misfiled

It is the C64 counterpart of `apple2detect.c` and `atari8detect.c`:
`DetectC64`, D64 and T64 extraction, the multi-game disk menus, and the US
picture loader (`LoadC64USImages`, `SetC64USCloseups`). It serves the US games
as much as the UK ones, and `saga/c64_small.h` includes it. Rename to
`c64detect.c` and put it beside the other two. The disk menus (219–371) could
be their own file.

The two `#ifdef SCOTT_HEADLESS` blocks (547, 795) are a debug dump driven by
an environment variable. They belong in the test tools, or behind one helper,
not inside the detection path.

### A7. Test code inside the renderers

`saga/vector_common.c` 88–156 (`ReadTestDataFromFile`, `LoadTestImage`,
`CompareWithTestFile`, `RunVectorTests`), `saga/apple2_vector_draw.c` from
about 625 and `saga/atari_8bit_vector_draw.c` 780–846 are the `-z` self-test
that `SpatterlightTests testScottDraw` runs. `saga/test/filltest.c` covers the
same pictures headless. Either move the self-test into one `vector_selftest.c`
that only the Spatterlight target compiles, or point the XCTest at `filltest`
and delete the `-z` option. `vector_common.h` then holds only the vector
drawing state.

## B. One owner for the database arrays

`Rooms`, `Items`, `Actions`, `Verbs`, `Nouns` and `Messages` are defined in
`scott.c`, allocated by `AllocateGameData()` and `SetGameHeader()` in
`detect_game.c`, freed by `FreeDatabase()` in `scott.c`, and freed again
through `FreeGameResources()` in `saga/saga.c`. All four loaders (plain text,
UK binary, US binary, TI-99/4A) call into `detect_game.c` only for those two
functions.

Put the definitions, `SetGameHeader`, `AllocateGameData`, `FreeDatabase` and
the small accessors now in `scott.c` (`MapSynonym`, `MatchUpItem`,
`ParseItemSlashAutoGet`) into `database.c` / `database.h`. The four loaders
then depend on that file and not on each other. `FreeGameResources()` is
`FreeDatabase()` plus a return value and can go.

## C. Folder layout

After A and B the folder names can be made true. Today:

- `ai_uk/` also holds C64 container code for all games, a generic ring buffer
  and the Howarth vector renderer.
- `saga/` holds renderers, container detection, a database loader and display
  logic. `sagagraphics.c` (pixel output) and `vector_common.c` are used by
  the UK games too.
- The root holds the UK loader.
- Three unrelated files are called `saga.c`, `sagadraw.c` and
  `sagagraphics.c`.

A layout that follows what the code does:

```
scott/
  main.c scott.c scott_actions.c scott_display.c scott_meta.c
  parser.c restore_state.c title_image.c database.c
  scott_game_info.c detect_game.c
  loaders/     scottfree_loader, uk_loader, us_loader, ti99 loader, rtpi_loader
  containers/  c64detect, c64_small, apple2detect, atari8detect, zxdetect,
               dosdetect, ti99_disk, unzip_in_mem, decompress_text, companion
  graphics/    sagagraphics, vector_common, vector_oplist, line_drawing,
               apple2_vector_draw, atari_8bit_vector_draw, c64a8scott,
               sagadraw (UK tile setup), rtpi_graphics, ringbuffer
  games/       gremlins, robin_of_sherwood, seas_of_blood, hulk,
               secret_mission, adventureland, us_display, closeups,
               sysmess_maps
  ti994a/      ti99_4a_terp (the separate TI bytecode interpreter)
  test/
```

This is the most expensive item for the least change in behaviour (four build
lists, see section 0), and it breaks `git blame` unless done with plain
`git mv` commits. Do it last, in one commit with no content changes, or skip
it and keep the A and B splits inside the current folders.

## D. Narrow the headers

### D1. `scott.h` exports 42 globals to every file

Regroup the `extern` lines by owner. No code changes, only where a
declaration and its definition live:

- Display state, defined in `scott.c` but owned by `scott_display.c`:
  `Top`, `Bottom`, `Graphics`, `TopWidth`, `TopHeight`, `split_screen`,
  `showing_inventory`, `showing_closeup`, `last_image_index`,
  `should_draw_image`, `lastwasnewline`, `should_look_in_transcript`,
  `ImageWidth`, `ImageHeight`. Move the definitions to `scott_display.c` and
  the declarations to `scott_display.h`.
- Loader state: `entire_file`, `file_length`, `file_baseline_offset`,
  `header[]`, `title_screen`, `ZXLoadingScreen`, `system_messages[]`,
  `DirPath`, `game_file`. Move to the loader header from A1.
- Game state: `GameHeader`, `Counters`, `RoomSaved`, `BitFlags`, `SavedRoom`,
  `CurrentCounter`, `AutoInventory`, `LightRefill`, `StopTime`. These are the
  fields of `SavedState` in `restore_state.h`. Keep them together in
  `scott.h` or `database.h`.

Optional follow-up: make the live game state one struct of the same type as
`SavedState`, so that save, undo and restore copy a struct. It touches every
user of `Counters[]` and `BitFlags`, so only do it if save or undo code is
about to change anyway. The regrouping above gives most of the clarity.

### D2. `parser.h` exports 25 word-list arrays

The English, German, Spanish and Italian lists are only read by `parser.c`
and by the callers of `SetParserWordLists()` (`detect_game.c`, `gremlins.c`,
`game_specific.c`). Replace it with `SetParserLanguage(lang)` in `parser.c`
with a four-value enum, and make all the lists static. `parser.h` is then the
`Command` struct, the `extra_command` enum and six functions.

### D3. Header names that exist in other interpreters

Xcode's header map is flat across the project, and `Makefile.headless` puts
`../taylor` on the include path. These names exist twice:

- `c64decrunch.h` (also in `taylor/`)
- `apple2detect.h`, `atari8detect.h` (also in `plus/`)
- `parser.h` (also in `archetype/`, `jacl/`)

It works today because of include order and same-directory lookup. When these
files are renamed or moved anyway (A6, C), give them names that are unique in
the project, and consider the same for other generic names: `bsd.h`,
`saga.h`, `game_specific.h`, `title_image.h`, `restore_state.h`,
`ringbuffer.h`.

### D4. `scott_defines.h` pulls in everything

It includes `aiukpalette.h`, `scott_opcodes.h` and `scott_sysmess.h`, and
nearly every header includes it, often only for `GameIDType`. Only
`scott_actions.c` and the two audit tools in `test/` use the opcode names, so
`scott_opcodes.h` can be included by those three alone. The system messages
are used by about twenty files, so `scott_sysmess.h` is better included from
`scott.h` than taken out. `GameInfo` needs `palette_type`, so `aiukpalette.h`
stays.

## E. Per-game code is wired in at six places

Adding or changing a game with special behaviour means editing these
switches:

- `SetUpGameSpecifics`, `detect_game.c:1376` (setup after load)
- `GameSpecificOpcode`, `scott_actions.c:544` (the game-specific opcode)
- `DrawRoomImage`, `scott_display.c:333` (room picture)
- `Updates`, `scott.c:314` (timer animations)
- `DrawGameSpecificOverlays`, `saga/saga.c:366` (US overlays)
- `SelectMapper`, `parser.c:393` (character set)

Because of them `scott_actions.c` and `scott_display.c` include `gremlins.h`,
`hulk.h`, `robin_of_sherwood.h`, `seas_of_blood.h` and `rtpi_graphics.h`.

Two ways to gather this, smaller first:

1. Move the four UK switches into one file, `game_dispatch.c`, with four
   functions (`GameSetup`, `GameOpcode`, `GameRoomImage`, `GameTimer`). The
   core files include one header and no game headers. No change in control
   flow.
2. A `GameHooks` struct of four function pointers per game family, set once
   in `DetectGame()`. Tidier, but the Plus pass declined a similar function
   pointer table, and the switches fall through between games in ways a
   table hides (`SAVAGE_ISLAND` → `SAVAGE_ISLAND2` → `GREMLINS`). Only worth
   it if option 1 still feels scattered.

Related: `SetUpGameSpecifics` patches game data inline (`Items[3].Image = 23`,
`Rooms[30].Image = 20`, `MyLoc = 30`). Those belong with the image patch
table in `ai_uk/sagadraw.c` or in the game's own file, with a name saying what
is fixed.

`DetectGame()` also changes the detected identity afterwards
(`CurrentGame = detectedGame; Game->type = US_VARIANT` at 1578–1584), and
`SetUpGame()` in `scott.c` changes it again to `SCOTTFREE` when a US game has
no pictures. One function that sets identity, called from both places, would
make the three states (detected, US with pictures, US fallback) visible.

## F. Smaller items

- `bsd.c` / `bsd.h` hold one function, `xstrncasecmp`, used by `parser.c`
  (four calls) and `scott.c` (one). No other interpreter has it. Move it to
  `common_utils`, or into `parser.c` with a declaration in `parser.h`.
- Tool binaries (`scott_hl`, `scott_cond_audit`, `scott_format_probe`,
  `scott_resize_test`, …) are built into the source folder and each needs a
  `.gitignore` line. Build them into `headless_obj/` or a `build/` folder,
  as `saga/test` already does.
- There are two test trees with their own Glk stubs: `test/headless_stubs.c`
  and `saga/test/headless_glk_stubs.c` plus `extract_stubs.c` and
  `scene_glk.h`. Moving `saga/test` to `test/pictures` and sharing the stubs
  would make one place to look. Low value unless the folder moves in C
  happen, since `saga/test` then needs new paths anyway.
- File naming is mixed: `atari_8bit_vector_draw.c` beside `atari8detect.c`,
  `load_ti99_4a.c` beside `ti99_4a_terp.c`. Function naming is mixed too:
  `snake_case` statics in `seas_of_blood.c`, `rtpi_graphics.c` and the
  `c64decrunch.c` menus, `CamelCase` elsewhere. Fix names only in files that
  are being split or renamed for another reason.
- `README` and `Definition.txt` in the root are the original ScottFree
  documents. A short `README.md` with the file map (which loader handles
  which release, where each renderer lives) would replace most of the need
  to read this TODO. `test/README.md` already carries the licence rules.

## Not recommended

- Merging `ti994a/ti99_4a_terp.c` into `scott_actions.c`. It is a different
  bytecode with its own opcode numbering. Sharing the state helpers it
  already calls is enough.
- Reordering condition or opcode evaluation, or the calls to
  `RandomPercent` / `erkyrath_random`, in the name of tidiness. The
  walkthrough goldens depend on the RNG stream.
- Splitting `scott_game_info.c` (2721 lines). It is one data table and reads
  fine as one.
- Merging the UK and US binary loaders. They share `SetGameHeader` and
  `AllocateGameData` and little else, and the UK Hulk already shows the
  cost of the one crossover (`HulkUKSpecificParsing` in `saga.c`).
- Renaming the ROM labels and the original author's comments that
  `test/README.md` says stay.

## Suggested order

1. Done: A1, A2, A3 and B together: they untangle the loaders and give `main.c`.
2. A5 (done) and D2: `game_specific.c` and the parser word lists.
3. A4 (done), A6 (done), A7.
4. D1, D4, then E option 1.
5. D3 and C, if the folder moves are wanted at all.
