# ScottFree

An interpreter for the Scott Adams adventures and the games built on the same
engine: the Adventure International US releases with pictures (S.A.G.A.), the
Adventure International UK games by Brian Howarth and others, the TI-99/4A
versions and Return to Pirate's Isle. It began as Alan Cox's ScottFree and
Chris Spiegel's Glk port of it. `README` and `Definition.txt` are the documents
of the original ScottFree; `Definition.txt` describes the plain text database
format.

This file is a map of the sources. The tests are described in
`test/README.md`, which also has the licence rules for test files.

## How a game is loaded

`DetectGame()` in `detect_game.c` tries the formats in turn. A container
module unpacks the file the user opened and finds the database in it, a loader
reads the database into the arrays of `scott_database.c`, and
`games/game_dispatch.c` does what is particular to the one game.

| File opened | Container | Database loader | Pictures drawn by |
|---|---|---|---|
| `.dat` plain text | none | `loaders/scottfree_loader.c` | none |
| ZX Spectrum snapshot or tape | `containers/zxdetect.c` | `loaders/uk_loader.c` | `graphics/sagadraw.c` (tiles), `graphics/line_drawing.c` (vectors) |
| Commodore 64 disk, tape or program, UK game | `containers/scott_c64detect.c` | `loaders/uk_loader.c` | `graphics/sagadraw.c` |
| Commodore 64 disk, US game | `containers/scott_c64detect.c` | `loaders/us_loader.c` | `graphics/c64a8scott.c` |
| Commodore 64 single file with small pictures | `containers/c64_small.c` | `loaders/us_loader.c` | `graphics/c64a8scott.c` |
| Atari 8-bit disk | `containers/scott_atari8detect.c` | `loaders/us_loader.c` | `graphics/c64a8scott.c` (bitmaps), `graphics/atari_8bit_vector_draw.c` (vectors) |
| Apple II disk | `containers/scott_apple2detect.c` | `loaders/us_loader.c` | `common_sagadraw/apple2draw.c` (bitmaps), `graphics/apple2_vector_draw.c` (vectors) |
| MS-DOS Hulk, picture files beside a `.dat` | `containers/dos_images.c` | `loaders/scottfree_loader.c` | `common_sagadraw/pcdraw.c` |
| TI-99/4A game file | none | `loaders/ti99_4a_loader.c` | none |
| Return to Pirate's Isle, zip or disk | `containers/unzip_in_mem.c`, `containers/ti99_disk.c` | `loaders/rtpi_loader.c` | `graphics/rtpi_graphics.c` |

`containers/us_companion.c` finds the second disk of a US game that comes on
two. `scott_game_info.c` is the table of known games that the binary loaders
match a file against.

## The folders

### Root: the interpreter

| File | Contents |
|---|---|
| `scott_main.c` | Program entry, command line options, main loop |
| `scott.c` | Game setup, one turn of play, printing, settings, exit |
| `scott_actions.c` | The action table: conditions and opcodes |
| `scott_parser.c` | Reading and parsing commands; the word lists of each language |
| `scott_display.c` | Room description, inventory, windows and picture display |
| `scott_meta.c` | Save, load, restart, transcript, undo and the other meta-commands |
| `restore_state.c` | Snapshots of the game state for undo and RAM save |
| `title_image.c` | Title pictures and title text |
| `scott_database.c` | The database in memory: rooms, items, actions, messages, dictionary |
| `scott_game_info.c` | The table of known games |
| `detect_game.c` | `DetectGame()` and the state of the loaded file |
| `bsd.c` | `xstrncasecmp()`, under the BSD licence |
| `scott.h` | The core's globals and functions |
| `scott_defines.h` | Game, machine and option enums; `GameInfo` |
| `scott_opcodes.h`, `scott_sysmess.h` | Names of the opcodes and the system messages |

### `loaders/`: reading a database

One file per database format, as in the table above, and
`decompress_text.c`, the text compression of Robin of Sherwood and Seas of
Blood.

### `containers/`: disk images, tapes and archives

Each takes the opened file apart, finds the database and the pictures in it,
and calls a loader. The readers of the formats themselves are shared with other
interpreters and are outside this folder: `terps/c64diskimage`, `terps/unp64`,
`terps/readwoz`, `terps/decompressz80`.

### `graphics/`: pictures

| File | Contents |
|---|---|
| `sagagraphics.c` | The list of US pictures (`USImages`), pixel output and the palette |
| `c64a8scott.c` | Glue to the shared Commodore 64 and Atari 8-bit bitmap decoder |
| `apple2_vector_draw.c`, `atari_8bit_vector_draw.c` | The vector pictures of the early US releases |
| `vector_common.c`, `vector_oplist.h` | What the two vector renderers share, and the hooks the tests use |
| `sagadraw.c` | Setup of the UK tile pictures, drawn by `terps/aiukgraphics` |
| `line_drawing.c`, `ringbuffer.c` | The UK vector pictures (`HowarthPictures`) and their flood fill |
| `rtpi_graphics.c` | The pictures of Return to Pirate's Isle |

`NOTES_howarth_vector.md` and `NOTES_rtpi.md` document two of the formats.
The decoders shared with the Plus and TaylorMade interpreters are in
`terps/common_sagadraw` and `terps/aiukgraphics`.

### `games/`: code for single games

`game_dispatch.c` is the only place the core calls into this folder for a
specific game: `GameSetup()`, `GameOpcode()`, `GameRoomImage()` and
`GameTimer()`. The rest:

| File | Contents |
|---|---|
| `gremlins.c`, `robin_of_sherwood.c`, `seas_of_blood.c`, `hulk.c`, `secret_mission.c`, `adventureland.c` | Animations, special opcodes and extra data of each game |
| `us_display.c` | Room and inventory pictures of the US games |
| `closeups.c` | The pictures shown when something is examined |
| `sysmess_maps.c` | Where each port keeps its system messages |
| `game_specific.c` | Colours that depend on the machine |

### `ti994a/`

`ti99_4a_terp.c` runs the action code of the TI-99/4A games, which is not the
Scott Adams format.

### `test/`

`Makefile.headless` in the root builds `scott_hl`, the interpreter on CheapGlk,
and the test tools, into `build/`. `make -f Makefile.headless check formatcheck companioncheck
scriptcheck savecheck` runs the text suites; `cd test/pictures && make
all-tests` runs the picture suites. See `test/README.md`.

## Builds

The file lists are in three places: the Xcode project, `Makefile.headless`
(wildcards over the folders) and `test/pictures/Makefile`. The Gargoyle fork
has its own list in its `terps/CMakeLists.txt`.
