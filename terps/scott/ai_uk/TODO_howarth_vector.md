# TODO: make the Howarth (Mysterious Adventures) vector graphics more accurate

Renderer: `ai_uk/line_drawing.c` (`DrawHowarthVectorPicture`), shared plumbing
in `saga/vector_common.c`. Format is Brian Howarth / Digital Fantasia
"Mysterious Adventures" line-drawing vectors (`HOWARTH_FORMAT`, picture format
99): a stream of MOVE_TO / draw-line / FILL / END opcodes over a 255×96-ish
coordinate space.

## Where we stand (measured)

The harness exists (`saga/test/zxvectortest.c` + `.zxvec` goldens; `zxextract`
dumps the `LineImages[]` streams) but the renderer is **not** byte-exact:

- Golden Baton opening room: **94.11%** overall.
- **Fill** pixels: 95.6% match — close.
- **Thin line** pixels: only **38%** match.

So the dominant error is the line rasterizer, not the fills.

## Root cause

`scott_linegraphics_draw_line()` is a generic Bresenham inherited from the
Level9 interpreter, not the original Digital Fantasia ROM line routine. It
places interior pixels ~1px off the original because the decision/rounding and
the choice of major axis differ. Lines are 1px wide, so a 1px lateral shift
misses most line pixels even though the shape looks right — hence 38%.

## Plan

### 1. Reverse-engineer the original line routine (the big win)
Disassemble the original AIUK / Digital Fantasia interpreter's line-draw
subroutine and capture its exact per-pixel output, then replicate it. Use the
same MAME ground-truth method already proven elsewhere in this tree:

- See the **`unquill-illustrator-renderer`** memory note (RPLOT direction-table
  RE: dispatch/line/fill addresses + MAME pixel ground-truth) and the
  **`saga-apple2-groundtruth` / `saga-atari8-groundtruth`** notes (MAME
  op-snapshot capture) for the established recipe.
- Pin down: octant selection (which axis is "major"), the balance/error
  initialisation, the tie-break direction when `balance == 0`, and whether both
  endpoints are plotted. Those three details are what move pixels by 1.
- The format shipped on ZX Spectrum, C64 and Atari 8-bit. Capture the ROM
  routine on whichever platform is easiest to single-step; confirm the others
  match (the *data* is the same vector stream, so one correct rasterizer should
  satisfy all platforms).

### 2. Replace the Bresenham with the RE'd algorithm
Rewrite `scott_linegraphics_draw_line()` to match the original's pixel
placement exactly. Re-run `zxvectortest`; target byte-exact on Golden Baton
room 0, then widen the corpus.

### 3. Re-check the fill once lines are exact
`diamond_fill()` is a 4-connected flood into `bg_colour` only. At 95.6% it's
nearly right, and some of its misses are almost certainly *line* pixels (the
fill stops against lines, so a 1px-shifted line leaks/clips the fill by a row).
Re-measure after step 2 before touching it. If a real gap remains, RE the
original fill's seed order and boundary test (fill-to-set-pixel vs
fill-bg-only), and note the **2048-byte ring buffer** in `diamond_fill()` can
silently drop coordinates on very large areas → holes; size it to the bitmap.

### 4. Confirm the line/background colour rule
`DrawHowarthVectorPicture()` picks the ink heuristically:
`line_colour = (bg_colour == 0) ? 7 : 0`. Verify against the original — the real
interpreter may store an ink per image or use a fixed platform ink, and "7" is
platform-dependent (and runs through `Remap()`/`Game->palette`). Getting this
wrong is a whole-image colour error, not a 1px one.

### 5. Fix the clip off-by-one
`scott_linegraphics_plot_clip()` accepts `x <= MYSTERIOUS_WIDTH` (255) but
`picture_bitmap` rows are only `MYSTERIOUS_WIDTH` (255) wide, so `x == 255`
writes into the next row (`y*255 + 255` == `(y+1)*255 + 0`) and `PutPixel`
plots one column past the intended edge. Decide whether the display is 255 or
256 wide; either widen the buffer/`MYSTERIOUS_WIDTH` to 256 or clip with `x <
MYSTERIOUS_WIDTH`. Re-verify after — this can account for a column of edge
mismatches.

### 6. Coordinate space / aspect sanity
Confirm `MYSTERIOUS_WIDTH 255`, `MYSTERIOUS_CLIPHEIGHT 95`, and
`ConvertY = 191 - y` map onto the original display region, and that on-screen
scaling (double-width? aspect) matches the source platform rather than just
filling the Glk graphics window.

### 7. Commit the golden and grow coverage
Once Golden Baton room 0 is byte-exact, commit its `.zxvec` golden and wire
`zxvectortest` into the test target (see `saga/test/TODO.md`, currently marked
`[~]`). Then add more Mysterious Adventures rooms, and ideally a C64 and an
Atari capture, so the rasterizer is proven across platforms.

## Pointers
- Renderer: `ai_uk/line_drawing.c`; opcodes `MOVE_TO 0xc0`, `FILL 0xc1`,
  `END 0xff`, anything else = draw-line-to.
- Harness: `saga/test/zxvectortest.c`, `saga/test/zxextract.c`,
  `saga/test/TODO.md` (ZX Howarth/vector entry).
- Method memory notes: `unquill-illustrator-renderer`,
  `saga-apple2-groundtruth`, `saga-atari8-groundtruth`.
