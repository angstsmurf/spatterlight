# The Coveted Mirror — Double Hi-Res (`#DHGR`) — DONE

## The bug (fixed)
Typing `#DHGR` in The Coveted Mirror (Apple II) wiped the picture to **all white**.

Root cause: CM room art is drawn with **op15 sub-op 0 = RLE raster bitmap**. The DHGR
interpreter `graphics_magician_dhgr.cpp gmDhgrDrawImage()` only handled op15 par==3 (fill)
and par==2 (skip 4 bytes); for **par==0 it did nothing AND consumed no operand bytes**, so
the bounds + RLE stream after op15/0 got misparsed as opcodes → the image collapsed to the
white reset background.

## The fix (committed)
Implemented op15/0 in the DHGR renderer (`rle_bitmap_dhgr()` + the `case 15` dispatch). It
mirrors the byte-exact standard-mode drawer in `graphics_magician.cpp` (4 bounds bytes
right/left/bottom/top; raw page bytes streamed column-major top→bottom then next column; a
`0x80` byte escapes a `count,value` run written `count+1` times; stops at the bottom-right
cell) **but** expands each source byte into a DHGR (aux,main) pair through the
nibble-doubling table `DHGR_DBL` — the same expansion the 6502 `<D>` routine (`$0a7b`) and
the port's own brush blitter already use:

```c
uint8_t hi = DHGR_DBL[b >> 4], lo = DHGR_DBL[b & 0xf];
uint8_t mainByte = (hi << 1) | (lo >> 7);   // -> s_main[CALC(row)+col]
uint8_t auxByte  = lo & 0x7f;               // -> s_aux [CALC(row)+col]
```

`col` (0..23) is the page byte offset; storing aux+main at the same offset places source
column `col` at logical DHGR columns `2*col`/`2*col+1` (the horizontal pixel-doubling the
6502 finalises with `ASL $13` → right bound 0x17→0x2F). par==1 (full-screen bounds) and
par==2 (read 4 bounds) consume the right number of bytes; `fill_rect` (par==3) fills the
whole screen with the pattern as before.

## Key finding (corrects the earlier "separate G-file DHGR art" theory)
CM `<D>` room art is the **standard Graphics Magician vector streams in the room files**
(`RA`..`RG`) — **not** separately-authored DHGR bitmaps and **not** in the `G`-files (those
hold the engine's pointer tables / text — a red herring). The throne room is `RB` image 1
(`op15/2, op15/3, op15/0`). `<D>` mode runs the same streams and rasterises every primitive
to the 560-wide aux+main pages.

This was confirmed by live MAME tracing: G0 loads verbatim at `$5A00`; the `<D>` image
interpreter lives at `$0800` (op = high nibble, par = low nibble, jump table `$08f7`, op15
dispatch `$0a17`, op15/0 `$0a7b`); the throne draw never executes op15/0 from the G-files
because it runs the `RB` room stream. (The prior `cm_ram_raw.bin` RE addresses `$09e2`/`$0a3f`
were a slightly different build offset; this build's op15/0 is `$0a7b`.)

## Validation
- `test/cmdhgrtest` renders the throne (`RB` idx 1) and asserts the DHGR aux+main pages are
  **byte-exact** vs the MAME ground truth over page offsets 0..20 of rows 0..159
  (offsets 21..23 are the picture/panel boundary, drawn on a separate pass). PASS.
- Regression: `test/test_dhgr_pics` (Talisman prison cell) still byte-exact — the vector
  path is unchanged.
- Wired into `make test`. Fixtures + recapture recipe in `test/cm_dhgr/README.md`.

## Tooling added
- `test/cmextract` — list/extract files from a Comprehend `.woz`/`.dsk` disk.
- `test/cmdhgrtest` — the op15/0 DHGR byte-exact regression (self-contained).

## Pointers
- Port DHGR code: `graphics_magician_dhgr.cpp` (`rle_bitmap_dhgr`, `DHGR_DBL`), NTSC kernel
  `common_sagadraw/gm_artifact.c`, routing in `pics.cpp` + `comprehend.cpp setDhgrMode()`.
- Memory: `comprehend-cm-dhgr-whiteout` (full RE log).
