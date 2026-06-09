# Talisman renderer regression fixtures

Each test case is a pair of files:

| file          | contents                                                              |
|---------------|-----------------------------------------------------------------------|
| `<name>.img`  | the picture's raw vector-opcode stream, as the renderer consumes it   |
| `<name>.page` | golden 0x2000-byte Apple II hi-res page ($2000..$3FFF) captured from MAME running the real game |

`test_talisman_pics` renders each `.img` through `apple2_talisman.cpp` and asserts
the resulting hi-res page matches the golden `.page` byte-for-byte over the
picture rows (Apple rows 0..159; rows 160..191 hold the game's text overlay,
which the Glk port draws separately).

The renderer's drawing tables (pattern data, fill-colour subindices, brush
bitmaps) are extracted at runtime from the boot disk's `T2` file (Graphics
Magician code, loads at `$0800`). `test/talisman/t2.bin` is a captured copy of
that file; `test_talisman_pics` and `pixtest` install it via
`talismanInstallDrawingTables()` before drawing.

Run with:

    make -C terps/comprehend test

## Current cases

All three are bright (white-background) rooms from the Empire disk of
*Talisman — Challenging the Sands of Time* (woz-a-day collection):

| name          | picture | notes                                                |
|---------------|---------|------------------------------------------------------|
| `courtyard`   | RA #1   | exercises the `DRAW_BOX` far-corner pen fix ($095b/$0990) |
| `executioner` | RA #2   |                                                      |
| `cell`        | RA #4   | the opening prison cell                              |

## Adding a case

1. **Capture the golden page from MAME.** Run the game in MAME (Apple //e, Disk II
   in slot 6), reach the room *in picture mode* (Enter toggles picture/text; the
   picture is only drawn on entering a new room), and dump page 1:

       save /tmp/room.page,0x2000,0x2000      # MAME debugger / Lua

   The game draws to page 1 (`$2000`); page 2 holds the blinking cursor and text.

2. **Identify the picture file + index.** Build the helper tool and brute-force
   match the dump against every room image (`RA`..`RG`, indices 0..15), comparing
   only rows 0..159:

       make -C terps/comprehend test/pixtest

   For each candidate, `PIXTEST_PAGE=/tmp/cand.bin test/pixtest <disk.woz> <FILE> <index> /dev/null`
   dumps the renderer's page; the one with zero diffs over the picture rows is the
   match. (Room files `RA`..`RD` live on the Empire disk, `RE`..`RG` on Lands Beyond.)

3. **Extract the renderer input** for that file/index:

       PIXTEST_IMG=test/talisman/<name>.img test/pixtest <disk.woz> <FILE> <index> /dev/null

4. **Copy the golden page** to `test/talisman/<name>.page`.

5. **Register the case** in the `kCases[]` table in `test/test_talisman_pics.cpp`.

`test/pixtest` also renders a page to a PPM (`test/pixtest --render in.page out.ppm`)
and logs every screen write tagged by primitive, which is how the original
byte-level MAME-vs-renderer diffs were done.
