# DOS Talisman CGA ground-truth fixtures

Byte-exact framebuffer captures of the real **IBM PC / DOS** release of
*Talisman — Challenging the Sands of Time* (`NOVEL1.EXE`, Polarware 1987),
recorded from DOSBox-X, for regression-testing the DOS CGA picture renderer
(`hdos_talisman.cpp`, exercised by `test/hdostest`).

These are the DOS counterpart to the Apple II golden pages in `test/talisman/`
(which validate `graphics_magician.cpp`).  The Apple and DOS releases share the
vector *geometry* but use **different fill-colour bytes / dithers**, so the DOS
renderer must be checked against DOS output, not the Apple pages.

## Files

| file            | what                                                                 |
|-----------------|----------------------------------------------------------------------|
| `<scene>.png`   | raw DOSBox screenshot, 640×400 (exact 2× of CGA mode 4, 320×200)     |
| `<scene>.fb`    | golden framebuffer: 320×200 = 64000 bytes, one palette index 0-3/px  |

`.fb` is produced from `.png` by `png_to_fb.py` and is the byte-exact compare
target; `.png` is kept for eyeballing.

## Geometry

DOSBox renders CGA mode 4 as a 640×400 PNG (clean nearest-neighbour 2×, only the
four palette colours ever appear), so collapsing 2→1 is lossless.

The Graphics Magician **picture window is 280×160 at screen origin (20, 0)** —
i.e. a 20-px left/right margin inside the 320-wide screen, picture rows 0..159,
game text below.  This matches `HDOS_WIDTH`×`HDOS_HEIGHT` in `hdos_talisman.h`.
`hdostest` emits exactly that 280×160 window, so the comparison crops the golden
`.fb` at (20, 0).

## Palette

The real game runs CGA **palette 1, low intensity**: black `000000`, cyan
`00aaaa`, magenta `aa00aa`, grey `aaaaaa`.  The renderer's `kHdosColor[]` now
matches these exactly (it used the *high*-intensity `55ffff / ff55ff / ffffff`
before 2026-06-11).  Comparison is done in **index space**, so the intensity
choice is cosmetic.  Index convention, matching the renderer: `0=black 1=cyan
2=magenta 3=grey`.

## Scenes captured

| scene       | game file / picture                       | reached by                        |
|-------------|-------------------------------------------|-----------------------------------|
| `title`     | `T0`                                      | program start                     |
| `cell`      | opening prison cell (RA)                  | start of game                     |
| `throne`    | King Darius on his throne — `RA` pic0 @0x22 | WAIT×4, BOW, advance cutscene    |
| `courtyard` | palace courtyard (RA)                     | from throne room, BOW, then WEST  |

`RA` is an offset-table file: a 2-byte LE table (`RA[0]` = table length in bytes
= 34 → 17 entries) of picture stream offsets `0x22, 0x8c1, 0x1061, 0x195d, …`.
Picture #0 (`0x22`) is the throne; the DOS picture *order* differs from the
Apple `RA #N` numbering in `../talisman/README.md`.

## Recapture / add a scene

1. Drive DOSBox-X (dosbox-mcp): `MOUNT A <gamedir> -t floppy`, `A:`,
   `NOVEL1.EXE`, answer the master-disk prompt with `A`.  To leave the opening
   dungeon: type `WAIT` four times then `BOW`, then advance the cutscene with
   Space.
2. At the scene, `dosbox_screenshot(path=…)` → save as `<scene>.png`.
3. `python3 png_to_fb.py <scene>.png <scene>.fb` (asserts a 640×400 input and a
   clean 4-colour palette).

## Compare the renderer against a golden

```
hdostest <NOVEL1.EXE> <picture-file> <offset> /tmp/r.ppm [white|black]
python3 compare_fb.py /tmp/r.ppm <scene>.fb /tmp/diff.png
```

`compare_fb.py` diffs the 280×160 window in index space, prints a mismatch
percentage, and (with a 3rd arg) writes a diff image (red = mismatched pixel).

## Automated regression test

`make -C terps/comprehend test` runs `test_hdos_pics`, which renders each scene
from its committed vector stream (`<scene>.img`) through `hdos_talisman.cpp` and
compares the 280×160 window to `<scene>.fb` in index space.  It is self-contained
— the drawing tables are a small slice of NOVEL1.EXE in `novel_tables.bin`, so no
copyrighted binary is needed at test time.  Each case asserts its mismatch count
is at or below a recorded ceiling; lower the ceilings as the renderer improves.

| file                 | what                                                       |
|----------------------|------------------------------------------------------------|
| `novel_tables.bin`   | fill / subindex / brush / font tables sliced from NOVEL1.EXE |
| `<scene>.img`        | the scene's raw Graphics-Magician vector stream            |

Scene → picture mapping: `title` = `T0`@4, `throne` = `RA`@0x22 (pic #0),
`courtyard` = `RA`@0x8c1 (#1), `cell` = `RA`@0x2128 (#4).

### Room fixtures (all RA–RG)

Beyond the four hand-picked scenes, every Talisman room picture (`RA`–`RG`, 94
streams) is committed as a regression fixture: 93 are pixel-exact and `RG_07` is
within 98 px.  These were captured by driving the native interpreter directly —
see `dosbox_capture_pics.py` (boots NOVEL1 under the GDB stub and patches a stub
to draw each picture, dumping CGA VRAM) and `gen_room_fixtures.py` (slices the
streams and packs the 280×160 goldens).  `test_hdos_pics` loads them from:

| file                 | what                                                       |
|----------------------|------------------------------------------------------------|
| `rooms_streams.bin`  | concatenated `RA`–`RG` vector streams                       |
| `rooms_goldens.bin`  | concatenated 280×160 goldens, packed 2 bpp (MSB-first)      |
| `rooms.tsv`          | `name  stream_off  stream_len  golden_off  ceil`            |

Object/overlay pictures (`OA`/`OB`/`OE`/`OF`) are sprites drawn over the live
room — capturing them this way needs their real in-game predecessor, so they are
left for a natural-play pass.  See `TODO.md` §5 for the addressing and op14
prior-page details.

## Status (2026-06-11)

The renderer now matches DOSBox closely on every fixture — **1.6 %–4.7 %** index
mismatch, down from 67–80 %.  Four bugs were found and fixed:

1. **Fill patterns were sampled mirrored.** The interpreter packs CGA bytes
   MSB-first (pixel 0 in bits 7:6, per `PicPlotPixel2bpp` @0x2470); the renderer
   reads its framebuffer LSB-first.  Each fill-table byte's four 2-bit groups are
   now reversed at load.  (This was the dominant error — it read as a 1-px dither
   phase shift on the period-2 dithers, ~30 %.)
2. **`RESET3` (op15 sub-op 3) cleared to a dither, not white.** The cold-start
   fill selector is 0 → solid white (NOVEL.EXE's statically-zeroed `[0x9d46]`);
   the renderer defaulted the selector to 3, a cyan/magenta/white dither, which
   then broke the white-region flood-fill predicate.
3. **op3 TextChar XORs the glyph** (`pixel ^= 0b11`) — it does not write the pen
   colour — so black-on-white subtitle text rendered magenta before the fix.
4. **Brush quadrants were left/right swapped** (`PicStampBrushShape` @0x2967 draws
   the left byte-column before the right), garbling the `DRAWSHP` logo.

Remaining diffs are thin **fill/brush edge pixels**: the renderer's simplified
queue flood-fill diverges by ~1 px at boundaries from the native 32-entry
circular-queue fill (`PicOp14Paint` @0x2630).  Closing that needs a faithful port
of that routine; the ceilings in `test_hdos_pics` hold the line meanwhile.
