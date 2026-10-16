# common_sagadraw: refactoring / cleanup TODO

Findings from a read-through of every file in this directory (no code changed).
Items are grouped by theme and tagged **[bug]**, **[dedup]**, **[cleanup]** or
**[design]**. Anything touching rendering output must be verified with the
existing pixel-exact tests in `terps/scott/saga/test/` (`apple2test`,
`c64a8test`, `dostest`, ...) and `terps/comprehend/test/`.

## 1. Probable bugs / hazards (fix first, each is small)

- **[bug] `gm_draw_brush` index overflow** (`gm_vector.c`, `gm_draw_brush`).
  `s.bitmap_index` is `uint8_t`, set to `brush * 8 * 4`. For `brush >= 8` the
  product is truncated (brush 8 -> 256 -> 0) *before* the
  `> GM_BRUSH_BITMAPS_SIZE` guard, so the guard never rejects it. Compute in
  `int`, or check `brush >= 8` up front. The `+= BRUSH_PART_SIZE` steps can wrap
  the same way.
- **[bug] `DrawApple2ImageFromData` loop counter** (`apple2draw.c`). `i` is
  `uint8_t` and the loop is `i < repetitions + 1`. `repetitions` is `uint8_t`
  but `repetitions + 1` is `int`, so with `repetitions == 255` the condition is
  always true and the loop only ends via the early `return 1` paths. Use
  `int i`, as `c64a8draw.c` does.
- **[bug] Scrambled-image writes can go out of range** (`apple2draw.c`,
  `DrawScrambledApple2Image`). `DescrambleScreenAddress()` returns `0` on error,
  so a bad row silently writes to `screenmem[0 + xpos]`, corrupting row 0.
  Also `DescrambleScreenAddress(ypos) + xpos` is not bounds-checked, so a large
  `xpos` can run past the 0x2000-byte buffer. Return an error / skip the write
  instead of using 0 as the failure value, and clamp the final index.
- **[bug] Unsigned underflow on short data** (`apple2draw.c`, `c64a8draw.c`).
  `ptr - origptr < datasize - 2` wraps if `datasize < 2`. `c64a8draw.c`
  checks `datasize < 12` first. `DrawApple2ImageFromData` has no such guard
  (only `ptr == NULL`). It also reads the 4 header bytes without checking
  `datasize >= 4`. Add one size check at the top.
- **[bug?] Dead / suspicious duplicate loop** (`apple2draw.c`). After the
  `if (!is_the_count) {...} else {...}` block there is a second
  `while (right == 0 && bottom == 0) { right = *ptr++; ... }`. In the
  `is_the_count` branch it is dead code. In the other branch it reads past the
  header if both are zero, with no bounds check. Decide which behaviour is
  intended and remove or bound it.
- **[bug?] `endrow` off-by-one candidate** (`apple2draw.c`,
  `DrawApple2ImageFromVideoMemWithFlip`). `for (row = 0; row <= endrow; ...)`
  with `endrow = MIN(ImageHeight / 2, 160)` draws one more row than
  `ImageHeight / 2`. Check whether that's intentional (the Apple hi-res page is
  192 rows; 160 is the old image area); add a comment or fix.
- **[hazard] Unterminated `TranslateColorC64` semantics.** Many `case` labels
  fall to `break` *without* calling `SetColor`, so the palette slot silently
  keeps the previous image's colour. That is behaviour, not an accident (see
  the "Unused color" markers). Any table rewrite (section 3) has to preserve
  "leave unchanged" as a distinct result.

## 2. Duplicated code

- **[dedup] Hi-res row-address formula is written out in many places.**
  `gm_row_address()` already exists in `gm_vector.h`, yet the same expression is
  hand-inlined in:
  - `apple2draw.c`: `WriteByteAndAdvanceY()` and `DrawApple2ImageFromVideoMemWithFlip()`
  - `terps/comprehend/graphics_magician_dhgr.cpp` (`CALC(y)` macro)
  - `terps/scott/saga/test/apple2test.c`
  - probably also `terps/bocfel/z6/draw_apple_2.cpp` (worth grepping)

  `apple2draw.c` should just `#include "gm_vector.h"` (or, better, move
  `gm_row_address` into a tiny `gm_hires.h` that both `gm_vector.h` and
  `gm_artifact.h` use). Consider adding the inverse
  (`gm_row_from_offset`), since `DrawSingleApple2ImageByte()` open-codes it as
  `((offset & 0x7f) / 40) * 64 + ((offset >> 7) & 7) * 8 + ((offset >> 10) & 7)`.
- **[dedup] `MAX_SCREEN_ADDR` defined three times** with three spellings:
  `apple2draw.h` (`MAX_SCREEN_ADDR`, plus `A2_SCREEN_MEM_SIZE`),
  `gm_vector.c` (`GM_MAX_SCREEN_ADDR`) and `comprehend/graphics_magician.cpp`.
  Define one `GM_HIRES_PAGE_SIZE 0x2000` / `GM_MAX_SCREEN_ADDR` in a shared
  header and derive the others. The unprefixed `MAX_SCREEN_ADDR` in a public
  header is also a collision risk.
- **[dedup] The RLE "two-byte pattern" decoder exists twice (maybe three times).**
  `DrawApple2ImageFromData` and `DrawC64A8ImageFromData` have the same skeleton:
  read repetitions, if bit 7 or "count" mode then repeat the next 2 bytes
  (`repetitions &= 0x7f` or `-= 1`), else read 2 bytes per iteration, `i` loop
  of `repetitions + 1`. They differ only in the sink (write a byte to
  `screenmem` vs. draw 8x2 pixels) and the end-of-column test.
  `terps/scott/saga/c64_small.c:498` also calls `DrawPatternAndAdvancePos`
  from its own decoder. A shared
  `rle_next_pair(&ptr, end, count_mode, &top, &bottom, &remaining)` iterator or
  callback-based `decode_pair_rle(data, size, mode, sink, user)` would remove
  the copies and the differing `repetitions`/`i` integer types (see section 1).
- **[dedup] Hand-written prototypes of host callbacks.** `pcdraw.c` and
  `c64a8draw.c` each declare `SetColor`, `PutPixelWithWidth`/`PutDoublePixel`
  locally, and `apple2draw.c` declares `extern int pixel_size, x_offset,
  y_offset`, `Graphics`, `ImageHeight` inline. These belong in one header
  (e.g. `sagadraw_host.h`) so the signature is declared once and checked by the
  compiler against the definitions (right now a mismatch would link fine in C).
- **[dedup] `(HGR_BITS * 2 + 0x40) & 0x80` + `^= 0x7f` appears four times**
  in `gm_vector.c` (`gm_set_draw_position`, `move_left`, `move_right`, plus the
  odd-column variant). Pull it into `static void flip_bits_if_needed(c)` with a
  comment explaining it is the HGR colour-phase flip between adjacent columns.
- **[dedup] Test harnesses re-implement library functions.**
  `scott/saga/test/c64test.c` has its own `DrawPatternAndAdvancePos`;
  `zxtest.c`, `zxvectortest.c`, `dostest.c` each re-define `PutDoublePixel`
  as `PutPixelWithWidth(x, y, c, 2)`. If the host-callback header above exists,
  a shared `test/sagadraw_stubs.c` can replace these (and `filltest.c`'s
  `unreached(...)` stubs).
- **[dedup] Apple II palette.** `gm_apple2_palette` lives here; check whether
  `terps/bocfel/z6/draw_apple_2.cpp` and the comprehend DHGR code keep their
  own copies and point them at `gm_artifact.h`. Note entries 5 and 10 are both
  `0x808080` (that's authentic: two greys), worth a comment so nobody "fixes" it.

## 3. `c64a8draw.c` (the biggest cleanup)

- **[cleanup] `TranslateColorC64` is ~340 lines of `case` ladder.** There are
  about 170 `/* Unused color */` markers and only 12 distinct outputs. Replace
  with a `static const struct { uint8_t value; uint8_t colour_index; }`
  sorted table or a 256-entry `int8_t` array (`-1` = "leave slot unchanged";
  see the hazard in section 1). Colour *names* should become an `enum` or
  array indexed by name instead of 12 loose `static const glui32` variables.
  Net effect: ~340 lines -> ~40, and the table can be diffed against the
  VICE-derived notes much more easily.
- **[cleanup] `ataricolors[256]` contains patched entries.** Several entries are
  hand-overridden (`0xad5f64`, `0xa23f40`, `0x4b1ead`, `0x088817`, ...) with the
  original value left commented out beside it, and the index comments become
  misleading (e.g. around 14/15, 18, 37, 50-58, 86, 102, 133, 198-200, 216,
  228, 247-255: two lines per index, one commented). Move the table to its own
  file (`atari8_palette.h`), make the overrides explicit
  (`/* index 18: remapped, was 0x521d00 */`) and keep the original values in a
  comment block only once. Note indices 198/199/200 appear to have duplicated
  `0x088817` plus a stray `0x3a6700, // 197`: **verify the table has exactly 256
  entries and every index maps to the intended value** (a `_Static_assert` on
  the array length would catch drift).
- **[cleanup] Unused declarations/includes:** `extern glui32 pal[16];`
  and `#include <stdio.h>` appear unused here.
- **[cleanup] `DrawPatternAndAdvancePos`** is exported, returns `x - 8`
  (undoing its own `x += 2` four times) and mutates `*y` as a side effect.
  Make the contract obvious (return the new x explicitly or take `int *x`),
  name the magic `8`, and document that it draws a 4x2 double-wide block
  (comment in `DrawC64A8ImageFromData` says 8x2 / "technically 4x2"; pick one).
- **[cleanup] `voodoo_or_count` and `is_c64` are int flags** threaded through
  `DrawC64A8ImageFromData`. An `enum { SAGA_RLE_COUNT, SAGA_RLE_FLAGGED }`
  and `enum { PLATFORM_C64, PLATFORM_ATARI8 }` would make call sites readable
  (`voodoo_or_count` is also used for `bottom -= 2`, a second meaning).
- `adjustments_fn` is `int(*)(int,int,int*)`: it both returns `right` and
  mutates `left` via pointer. Consider a small `struct image_bounds` in/out
  parameter. The Apple version uses a different typedef (`adjust_plus_fn`,
  `void(*)(uint8_t,uint8_t,uint8_t)`) for the same purpose; unify.

## 4. `apple2draw.c` / `apple2draw.h`

- **[cleanup] Stale or dead declarations in the header:**
  `void test_move_up_and_down(void);` has no definition or caller anywhere in
  the tree; `APPLE_SCREEN_WIDTH` (apple2draw.c) is unused;
  `#include "read_le16.h"` is unused in `apple2draw.c` (also in `pcdraw.c`
  it is used, but `<stdint.h>` is missing from `pcdraw.h`, which uses `uint8_t`
  and relies on include order). `<stdlib.h>` in the header is only needed for
  `size_t`; use `<stddef.h>`.
- **[design] Two public ways to draw** (`DrawApple2ImageFromVideoMem` and
  `...WithFlip(0)`) plus `ClearApple2ScreenMem`, `DrawSingleApple2ImageByte` and
  two exported globals (`screenmem`, `descrambletable`). Make `screenmem` /
  `descrambletable` accessed through functions or a context struct; the header
  comment says "NULL until first drawn to", which every caller has to know.
  A single `Apple2ScreenState` (screenmem + descrambletable + flip) would also
  let tests link without the 10-line stub files.
- **[cleanup] Lazy `screenmem` allocation is repeated** in
  `DrawScrambledApple2Image` and `DrawApple2ImageFromData`
  (`if (screenmem == NULL) screenmem = MemCalloc(...)`). Wrap in
  `static uint8_t *ensure_screenmem(void)`. In `DrawApple2ImageFromData` it is
  also done *before* the descrambletable branch that already allocates, i.e. the
  scrambled path does it twice.
- **[cleanup] `RenderLineWithA2ArtifactColors` run-length logic** is hard to
  follow (own comment: "not sure why" about the +1 overlap, plus a special
  case for the last pixel that re-fetches `glkcolor`, shadowing the outer
  variable). Rewrite as a plain "find run end, emit run" loop over the flat
  `colors560[]` array (the double `col`/`b` loop is not needed once it is a flat
  560-element scan). Also hoist the magic numbers (560, 319, 14, 40) into
  named constants, and make the flip a mirror helper, since
  `PutApplePixelFlippable` is the only place that knows the geometry.
- **[cleanup] Shadowed/ambiguous names:** the two inner `glkcolor` variables, the
  `uint8_t repetitions, i` mix noted above, `pattern_top/bottom` initialised to 0
  and never needed before assignment, `origptr` used only for a length.
- **[design] Host globals** (`Graphics`, `pixel_size`, `x_offset`, `y_offset`,
  `ImageHeight`) make this "common" library depend on `scott.h` globals.
  Passing a small `{winid_t, pixel_size, x_off, y_off, height}` struct (or the
  plotting function pointer) would let Scott, Plus and the tests share it
  without the stub files seen in `terps/scott/saga/test/`.

## 5. `pcdraw.c` / `pcdraw.h`

- **[cleanup]** `DrawState` has `at_last_line` used as a counter (`++`) and
  compared `<= 1`; rename to `last_line_passes` or similar and explain the
  "draw the last line twice" interleaving. `ycount`/`bottom`/`top` mix row
  counts and pixel rows without comments.
- **[cleanup]** The doc comment above `DrawPattern` ("Skip every other pixel
  when pixelwidth is 1") doesn't match the code (it draws all four pixels and
  `pixelwidth` just scales them). Fix the comment or the code.
- **[cleanup]** Header offsets `0x05`, `0x0d`, `0x0f`, `0x11`, `0x13`, `0x17`
  and the CGA palette literals should be named constants. Header-parse
  could be a separate `parse_dos_header()` returning a struct, which also makes
  it testable and lets us add a `size` sanity check (`ptr + size` is never
  compared to the caller's real buffer length; `ptr + 0x17` is not
  bounds-checked at all).
- `SetColor` is called 4 times with a constant palette on *every* image
  draw; harmless but could be done once.
- `size_t size = READ_LE_UINT16(...)` / `(void)` returns: function always
  returns 1 when `ptr` non-NULL; either return a real success value or make
  it `void`.

## 6. `gm_vector.c` / `gm_vector.h`

- **[cleanup] Misleading name `COL_BITS`** (7). It is *pixels per hi-res byte*,
  not bits per column. Rename `GM_PIXELS_PER_BYTE`. Likewise `HGR_HORIZ` is the
  column *byte* index.
- **[cleanup] Magic numbers:** 191 / 192 (rows), 40 (columns), 39, `0xc0`,
  `0x81`, `0x7f`. Add `GM_ROWS`, `GM_COLS`, etc. The off-by-one traps in
  `move_up_or_down` (`> 191` vs `(int8_t) < 0`) are already commented; naming the
  constants would remove the need.
- **[cleanup] `move_left_or_right(bool left, ...)`** is a one-line wrapper over
  `move_left`/`move_right`; same for `move_up_or_down(bool down,...)`. Fine
  as-is, but the draw-line loop can call the direction functions directly.
- **[cleanup] `gm_set_color` silently ignores `color_index >= 8`.** Either
  assert (debug builds) or document "unchanged on invalid".
- **[cleanup] Stale header text:** `gm_vector.h` describes the file as shared
  by `scott/saga/apple2_vector_draw.c` and comprehend, and the .c file says
  it is "a verbatim extraction ... must not change behaviour". Keep the
  warning, but now that the duplicates are gone make sure the paths mentioned
  still exist and the brush code (added later, not a verbatim extraction) is
  covered by the comment.
- ~~**[cleanup] `brush_quadrant`** mutates the scratch struct and then undoes it~~:
  done. `gm_brush_scratch` is gone; `brush_quadrant()` takes its column, first
  scanline and bitmap as parameters, and `brush_blend()` does the two writes.
- `brush_quadrant()` takes `uint8_t scanline` and `uint8_t column`, both of
  which are summed with offsets (`+ 8`, `+ 1`) and can wrap near the bottom/right
  edge; `brush_quadrant` re-checks `address <= GM_MAX_SCREEN_ADDR`, but
  `gm_row_address(uint8 scanline wrapping to 0..)` would wrap onto the top of the
  screen rather than clip. Use `int` for both and clip explicitly.

## 7. `gm_artifact.c` / `gm_artifact.h`

- **[cleanup] `gm_double7bits` builds its table lazily with a static flag.**
  Not thread-safe and needlessly runtime. Generate it at compile time (a
  128-entry `static const uint16_t[]`), or build it in a constructor-free
  `static` initialiser. Also note `table[0]` relies on static zero-init.
- **[cleanup]** `rotl4b` comment says "bits 4-6 of n must be a copy of bits
  0-2" but the function is a right shift with a masked count; rename to
  `rotate_color` or fix the comment. `CONTEXTBITS` and the `<< 14` should be
  tied together via a named constant.
- **[cleanup]** `gm_compute_dhgr_row_words` / `gm_render_dhgr_line_colors`
  are only used from the comprehend DHGR code. Fine, but the header comment
  block ("verbatim extraction ... the two copies were byte-for-byte identical")
  is history that belongs in git, not the source.
- **[dedup]** `gm_render_line_colors` / `gm_render_dhgr_line_colors` differ
  only in `phase`; an exposed `gm_render_line_colors_phase()` would let callers
  pass `is_80_column` directly.

## 8. Cross-cutting

- **[cleanup]** Mixed C dialect/style: tabs vs spaces, `//` vs `/* */`, inconsistent
  brace style and `uint8_t`/`int` for loop variables across the five source
  files. Pick one (the surrounding repo uses 4-space) and run a formatter on this
  directory *after* the functional changes so the diff stays reviewable.
- **[design]** No unit tests live in this directory; coverage is spread across
  `scott/saga/test/` and `comprehend/test/` using stubbed host symbols
  (`filltest.c`, `c64test.c`, ...). A `common_sagadraw/test/` with the shared
  stubs (section 2) and golden-image comparisons for `gm_artifact`,
  `gm_vector` (line/brush) and the RLE decoders would make the
  refactors above safe to do incrementally.
- **[design]** Names are inconsistent: `gm_*` (Graphics Magician), `Draw*Apple2*`,
  `DrawC64A8*`, `DrawDOS*`, `A2_*`, `MAX_SCREEN_ADDR`. Consider a consistent
  prefix for new/renamed API (`sagadraw_*`) and keep old names as thin wrappers
  until callers (`scott/saga`, `plus`) are migrated.
- Suggested order of work: (1) fix the bugs in section 1 with a test for each;
  (2) share `gm_row_address` / screen-size constants; (3) add the host-callback
  header and shared test stubs; (4) table-ise the C64/Atari colour mapping;
  (5) unify the RLE iterator; (6) formatting pass.
