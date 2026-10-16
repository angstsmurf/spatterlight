# Atari 8-bit vector draw: simplify the colour/pattern pipeline — TODO

File: `atari_8bit_vector_draw.c` (mainly `create_patterns()`, `make_pattern()`,
`flood_fill()`, `plot_pixel()`, `handle_draw_line()` and the `DrawSingleByteAt()`
renderer).

## Status: DONE

Implemented in `atari_8bit_vector_draw.c`. Every item below is ticked. Deviations
from the plan, and how it was checked:

- `a8_pixel_rgb()` is an inline function (`RGBpalette[color_matrix[p & 3][p >> 2]]`),
  not a `const uint32_t[16]`. C cannot initialise a const table from the other
  const arrays, and the two lookups are cheap.
- `pixel_offset` is the only position state besides `xpos`/`ypos`. `screen_offset`,
  `pixel_remainder` and the byte-offset helpers are gone. `move_up/down` move it
  by ±160.
- The scanline patterns are `rows[2][2]` (`rows[0]` = the old `*_pattern_even`),
  separate from the built `tile[2][2]` that patterns 5..31 reuse.
- Lines-only mode stores `saved_pixel[2]`: a line in colour `[0]` is redrawn in `[1]`.
- Ops are `{offset, uint16_t pixels (four nibbles), fill_bg, delay}`, recorded
  after the write by reading the pixel buffer back.
- Checked with `make test` / `make groundtruth` (6/6 fixtures, 315/318 Atari images
  byte-exact, the same 3 known-divergent as before), and by hashing the whole
  recorded op stream (offset, both plane bytes, fill flag, delay) for all 318
  corpus images before and after: identical, so slow-draw pacing is unchanged.
  The hash hook was temporary and is not committed.

## The problem

A colour currently goes through three representations before it reaches the
screen:

1. **Colour index (0..15)** from the image data (`color_idx`, `color_a_index`,
   `color_b_index`).
2. **A pair of 2-bit values** looked up in `color_array`. The pair is split
   across the two emulated bit-planes: the first value goes to `$A000`, the
   second to `$9000`. `make_pattern()` packs each one into a separate
   "pattern byte" (`abab`, four pixels per byte), and the fill and line code
   writes those bytes into two separate screen buffers (`a8_screenmem90` /
   `a8_screenmemA0`) with bit masking.
3. **An RGB colour.** At blit time `DrawSingleByteAt()` takes the planes apart
   again, one 2-bit slot at a time, and looks up
   `RGBpalette[color_matrix[bits90][bitsA0]]`.

The two planes model how the original swapped screens every frame to fake 16
colours. But nothing reads one plane without the other. Every consumer
recombines them straight away:

- `get_pixel_color()` rebuilds a 4-bit value `A0<<2 | 90` for the
  edge/background tests.
- `fill_edge_color` is built the same way from `color_array`.
- `last_plane90_byte` / `last_planeA0_byte` (fast-path check) are the
  background colour split back into two planes.
- The renderer recombines them into a `color_matrix` index.

`color_array` is a **bijection** between the 16 colour indices and the 16
possible 4-bit pixel values. So a pixel is really just one 4-bit value, and the
pipeline packs it into two planes and unpacks it again for no gain:

| idx | A0 | 90 | pixel `A0<<2\|90` | RGB (`color_matrix[90][A0]`) |
|----:|---:|---:|:-----------------:|:-----------------------------|
|  0  | 3  | 3  | 0xf | YELLOW15 |
|  1  | 3  | 2  | 0xe | GREEN11  |
|  2  | 3  | 1  | 0xd | YELLOW07 |
|  3  | 3  | 0  | 0xc | BLUE     |
|  4  | 2  | 2  | 0xa | BLUE10   |
|  5  | 2  | 1  | 0x9 | VIOLET06 |
|  6  | 2  | 0  | 0x8 | BLUE     |
|  7  | 1  | 1  | 0x5 | ORANGE5  |
|  8  | 1  | 0  | 0x4 | YELLOW15 |
|  9  | 0  | 0  | 0x0 | BLACK    |
| 10  | 2  | 3  | 0xb | WHITE14  |
| 11  | 1  | 3  | 0x7 | GOLD13   |
| 12  | 0  | 3  | 0x3 | GOLD12   |
| 13  | 1  | 2  | 0x6 | GOLD09   |
| 14  | 0  | 2  | 0x2 | YELLOW07 |
| 15  | 0  | 1  | 0x1 | RED04    |

The dither patterns are also simpler than they look. In every row of
`pattern_defs`, the two plane bytes for a scanline pair `pattern_color[0]` with
`[1]`, or `[2]` with `[3]`. So each pixel of a pattern is **either colour A or
colour B, never a mix of the two**. Each pattern is a 2×2 tile of A/B:

| pattern | even row | odd row | what it is |
|--------:|:--------:|:-------:|:-----------|
| 0 | A A | A A | solid A |
| 1 | A B | B A | checkerboard |
| 2 | A A | B B | horizontal stripes |
| 3 | B A | B A | vertical stripes |
| 4 | A A | B A | mixed: solid row / alternating row |

(The comment above `pattern_defs` gets several of these wrong. For example, it
says pattern 0 is "horizontal stripes" when it is solid, and pattern 2 is
"mixed solid stripes".)

## Proposed design

- [x] **One 4-bit-per-pixel framebuffer.** Replace `a8_screenmem90` /
      `a8_screenmemA0` with a single `uint8_t a8_pixels[160 * 96]`. That is
      `A8_IMAGE_SIZE * 4`. The planes hold 96 rows even though
      `A8_SCREEN_HEIGHT` is 95: fills and lines can reach row 95, and the
      renderer draws it. Each entry
      holds the 4-bit hardware pixel value `A0<<2 | 90`. (Storing the colour
      index instead would also work, but the hardware value keeps the
      edge/background comparisons and the golden test files trivial to
      derive.)
- [x] **One lookup table per colour index.** Build
      `static const uint8_t a8_index_to_pixel[16]` (the table above) once,
      replacing `color_pair()` / `color_array` everywhere. Build
      `static const uint32_t a8_pixel_rgb[16]` =
      `RGBpalette[color_matrix[p & 3][p >> 2]]`, so the renderer does a single
      lookup.
- [x] **Patterns as a 2×2 tile of pixel values.** `create_patterns()` produces
      `uint8_t tile[2][2]` (`[row parity][x parity]`) straight from
      `pattern_defs`, rewritten as A/B selectors. `make_pattern()`,
      `pattern_color[]`, `pattern_byte[]` and the four
      `plane*_pattern_even/odd` fields go away. The even/odd swap becomes
      swapping the tile's two rows. Alternatively, drop the swap and index the
      tile by `(y ^ start_parity) & 1`.
      Pattern numbers 5..31 (`arg_0` is 5 bits) skip `make_pattern()` today and
      reuse the previous fill's pattern bytes, so they must leave the previous
      tile untouched. Before any fill has run, that tile is all zeros. "The
      previous tile" means the tile as `create_patterns()` built it (today's
      `pattern_byte[]`), not as `flood_fill()`'s row swaps or a line colour
      left the working copy. So keep the built tile and the working row
      state separate.
- [x] **`plot_pixel()` becomes `a8_pixels[off] = cur_row[x & 1]`.** No
      masks, no read-modify-write of two buffers. `cur_row` is today's
      `*_pattern_even`. In a fill it is whichever tile row the swaps have
      selected, which is not necessarily `y & 1`. For a line it is the solid
      line colour.
- [x] **`get_pixel_color()` becomes `a8_pixels[pixel_offset]`.**
      `fill_edge_color` becomes `a8_index_to_pixel[color_idx]`.
- [x] **Drop the redundant position state where possible.** With one byte per
      pixel, a linear pixel index alone addresses a pixel. `screen_offset` and
      `pixel_remainder` only exist to address the packed planes, so
      `move_left/right`, `byte_offset_from_pixel_offset()` etc. can shrink
      accordingly. `xpos`/`ypos` are still needed for the scan limits.
      **Careful:** today `move_up()` / `move_down()` change only
      `screen_offset`, so `pixel_offset` goes stale during `draw_line()`, and
      `plot_pixel()` addresses through `screen_offset * 4 + pixel_remainder`.
      The new index must follow that value, so `move_up/down` have to move it
      by ±160. That includes the wrap-around: an x past 159 spills into the
      next row, and an index below 0 is off-screen. Today that case is a
      wrapped `uint16_t` and is skipped, so it must not wrap into valid
      memory.
- [x] **Simplify, don't remove, the 4-pixel fast path in `flood_fill()`.** Its
      result on screen matches the per-pixel path: it only fires when the
      column is byte-aligned, all four pixels are background and erase mode
      is off, and rows are 160 wide, so it can never overrun the row end. But
      it emits *one* write op for four pixels where the per-pixel path emits
      four, so removing it changes the slow-draw animation (see below). Keep
      it as "if `x % 4 == 0` and the next four pixels equal the background,
      write them as one op". `last_plane90_byte` / `last_planeA0_byte` /
      `build_plane_byte()` can still go.
- [x] **Lines.** `handle_draw_line()` sets a single solid pixel value instead
      of building two nibble-duplicated plane bytes. Lines-only mode currently
      compares `saved_pattern[0..1]` with the line's plane bytes and then draws
      with `saved_pattern[2..3]`. That reduces to: *if the line's colour
      equals saved colour A, draw it in saved colour B*. Store two pixel values
      instead of four pattern bytes.
- [x] **Background fill** (`A8_OP_SET_BACKGROUND_OR_MODE`, arg 0) becomes
      `memset(a8_pixels, tile[0][0], ...)`.

## Things that must be preserved

- [x] **The parity-swap quirk at the start of `flood_fill()`.** Whether the
      even/odd patterns get swapped is decided by a *signed 8-bit difference of
      the packed plane bytes*, `(int8_t)(second_90 - first_90) < 0`, with a
      fallback to the A0 bytes. That is a 6502 artefact, and the output depends
      on it. Keep a small helper that packs a tile row back into the two plane
      bytes (`a<<6|b<<4|a<<2|b`) just to evaluate this condition. Or derive the
      equivalent rule on pixel values and prove it against the test corpus.
- [x] **Golden tests compare the two planes byte-for-byte.** `TestAtari8ImageWithName()`
      / `RunAtari8bitVectorTests()` here and `test/filltest.c` (via
      `GetAtari8Screenmem90()` / `GetAtari8ScreenmemA0()`) do this. Keep those
      accessors, but have them pack the pixel buffer into two plane images on
      demand: for byte `i` and slot `s`,
      `90 |= (p & 3) << (6-2s)` and `A0 |= (p >> 2) << (6-2s)`. That way the
      existing `.result` / `.90` / `.a0` goldens stay valid and prove the
      refactor is exact. Run `make test` / `make groundtruth` in `test/`
      before and after.
- [x] **Out-of-bounds behaviour.** Pixels outside the screen read as colour 0
      and count as an edge. `pixel_offset` going negative stops `scan_up()`.
      `xpos == -1` / `xpos == 160` bound the row scans. All of these need to
      survive the change of addressing.
- [x] **Slow-draw pacing.** `write_ops` records one op per `plot_pixel()`
      call and one per fast-path byte. Each op carries the *whole* screen byte
      containing the pixel(s), and `ATARI8_VECTOR_BYTES_PER_TICK` is tuned to
      that stream. To keep the animation identical, emit exactly the same op
      sequence: each op holds the byte offset and the four pixel values of
      that byte, packed in a `uint16_t`, read back from the pixel buffer after
      the write. Memory use is unchanged, and the playback side
      (`DrawSingleByteAt()`) does four `a8_pixel_rgb[]` lookups instead of
      splitting two plane bytes. Recording per pixel or per run instead would
      be simpler, but it changes the pacing and needs the constant retuned.

## Side notes found while reading

- `color_matrix` maps the 16 pixel values to only 13 distinct colours: `BLUE`
  appears twice, as do `YELLOW07` and `YELLOW15`. Meanwhile `MAGENTA`,
  `BRMAGENTA` and `BRGREEN` in `RGBpalette` are never used. That may be
  deliberate (artefact colours that look alike), but it's worth checking
  against MAME captures while the palette is being moved into a single
  16-entry table.
- In `DrawAtari8BitVectorImage()`, `ctx.color_idx = 10` is commented as
  "(2,1)", but `color_array` entry 10 is `(2,3)`.
