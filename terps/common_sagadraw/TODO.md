# common_sagadraw: refactoring / cleanup TODO

Findings from a read-through of every file in this directory (no code changed).
Items are grouped by theme and tagged **[bug]**, **[dedup]**, **[cleanup]** or
**[design]**. Anything touching rendering output must be verified with the
existing pixel-exact tests in `terps/scott/saga/test/` (`apple2test`,
`c64a8test`, `dostest`, ...) and `terps/comprehend/test/`.

## Status (worked through 2026-10-17, committed in steps; see `git log -- terps/common_sagadraw`)

`[x]` = done, `[~]` = partly done, `[-]` = declined or not done; the line
after `→` says what was done or why not. Line numbers in the findings are
those of the survey and no longer match; grep the function name.

New headers: `gm_hires.h` (page size, rows, columns, `gm_row_address`,
`gm_row_from_offset`), `sagadraw_host.h` (the host globals and callbacks)
and `atari8_palette.h`. `DrawDOSImageFromData` now takes the data size, so
`scott/saga/saga.c`, `plus/graphics.c` and `scott/saga/test/dostest.c` were
updated.

How it was verified:

- A differential harness in the session scratchpad. It hashed every host
  call plus the screen memory for every entry point, on all real blobs and
  four fuzzed copies of each (13270 runs), comparing the old build with the
  new one.
  - The bug fixes in section 1 were the only intended changes. After them
    the harness was re-baselined, and every later step was identical to that
    baseline.
  - The DOS size check changes one real image: B019.PAK, which is a 0-byte
    file. It now returns 0 instead of reading past its end.
  - It also changes fuzzed blobs whose header claims more data than they
    hold.
- The same runs under ASan and UBSan (exact-size buffers): clean, with no
  timeouts.
- `scott/saga/test`: groundtruth (319/319), dostest, apple2test, c64test,
  c64a8test, titest, zxtest and scenetest all pass byte-exact.
- `scott`: `make -f Makefile.headless check formatcheck` passes.
- `comprehend`: `make test` passes.
- `plus`: `make -f Makefile.headless imagecheck scriptcheck synthcheck`
  passes.
- Every `.c` file here builds cleanly with `-Wall -Wextra -Wshadow
  -Wpedantic`.
- The Xcode build (scheme Spatterlight) succeeds. Xcode finds these headers
  through the `common_sagadraw` group in the project file, not through a
  search path, so the three new headers were added to that group
  (`plutil -lint` OK).

## 1. Probable bugs / hazards (fix first, each is small)

- [-] **[bug] `gm_draw_brush` index overflow** (`gm_vector.c`, `gm_draw_brush`).
  `s.bitmap_index` is `uint8_t`, set to `brush * 8 * 4`. For `brush >= 8` the
  product is truncated (brush 8 -> 256 -> 0) *before* the
  `> GM_BRUSH_BITMAPS_SIZE` guard, so the guard never rejects it. Compute in
  `int`, or check `brush >= 8` up front. The `+= BRUSH_PART_SIZE` steps can wrap
  the same way.
  → Declined. The 6502 routine indexes the brush table with a byte too, so
  brush 8 and up wrap onto brushes 0..7. The renderer is matched pixel for
  pixel against hardware captures, so the wrap stays. It now has a comment,
  and the unreachable guard is gone.
- [x] **[bug] `DrawApple2ImageFromData` loop counter** (`apple2draw.c`). `i` is
  `uint8_t` and the loop is `i < repetitions + 1`. `repetitions` is `uint8_t`
  but `repetitions + 1` is `int`, so with `repetitions == 255` the condition is
  always true and the loop only ends via the early `return 1` paths. Use
  `int i`, as `c64a8draw.c` does.
  → `int i`. Repetitions 255 now gives 256 iterations, like a 6502 counter
  where 0 means 256, instead of looping until the data runs out. Only count
  mode (COUNT_US) reaches this. Every Count disk in the corpus is scrambled,
  so no real image changes.
- [x] **[bug] Scrambled-image writes can go out of range** (`apple2draw.c`,
  `DrawScrambledApple2Image`). `DescrambleScreenAddress()` returns `0` on error,
  so a bad row silently writes to `screenmem[0 + xpos]`, corrupting row 0.
  Also `DescrambleScreenAddress(ypos) + xpos` is not bounds-checked, so a large
  `xpos` can run past the 0x2000-byte buffer. Return an error / skip the write
  instead of using 0 as the failure value, and clamp the final index.
  → `DescrambleScreenAddress` returns -1 on error. A new
  `WriteScrambledByte()` skips the write when the row is bad or the address
  would leave the page. Before this, the harness saw writes up to 0x10000
  bytes past the page.
- [x] **[bug] Unsigned underflow on short data** (`apple2draw.c`, `c64a8draw.c`).
  `ptr - origptr < datasize - 2` wraps if `datasize < 2`. `c64a8draw.c`
  checks `datasize < 12` first. `DrawApple2ImageFromData` has no such guard
  (only `ptr == NULL`). It also reads the 4 header bytes without checking
  `datasize >= 4`. Add one size check at the top.
  → `ptr == NULL || datasize < 4` returns early. The comparisons now cast
  the pointer difference to `size_t`, which silences `-Wsign-compare`.
- [x] **[bug?] Dead / suspicious duplicate loop** (`apple2draw.c`). After the
  `if (!is_the_count) {...} else {...}` block there is a second
  `while (right == 0 && bottom == 0) { right = *ptr++; ... }`. In the
  `is_the_count` branch it is dead code. In the other branch it reads past the
  header if both are zero, with no bounds check. Decide which behaviour is
  intended and remove or bound it.
  → Merged into one loop that skips zero-sized headers. It stops at the end
  of the data (`end_of_data - ptr < 2` returns 1). Real images are unchanged.
- [-] **[bug?] `endrow` off-by-one candidate** (`apple2draw.c`,
  `DrawApple2ImageFromVideoMemWithFlip`). `for (row = 0; row <= endrow; ...)`
  with `endrow = MIN(ImageHeight / 2, 160)` draws one more row than
  `ImageHeight / 2`. Check whether that's intentional (the Apple hi-res page is
  192 rows; 160 is the old image area); add a comment or fix.
  → Kept, with a comment. It draws ImageHeight/2 + 1 rows (row 160 at
  height 320). The Scott and Plus Apple II goldens include that row, so
  changing it would mean regenerating them for no visible gain.
- [x] **[hazard] Unterminated `TranslateColorC64` semantics.** Many `case` labels
  fall to `break` *without* calling `SetColor`, so the palette slot silently
  keeps the previous image's colour. That is behaviour, not an accident (see
  the "Unused color" markers). Any table rewrite (section 3) has to preserve
  "leave unchanged" as a distinct result.
  → Kept as `C64_UNUSED` (-1) in the new table. `TranslateColorC64` calls
  `SetColor` only for other entries. All 256 inputs were checked against the
  old function.
  → Later (2026-10-17) the map was replaced by the original interpreter's
  algorithm, so every value now gets a colour and nothing is left
  unchanged. Values the old map left stale ($E4, $F1, $F3, $F9, $FD) now get
  their original colours.

## 2. Duplicated code

- [x] **[dedup] Hi-res row-address formula is written out in many places.**
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
  → Both functions now live in the new `gm_hires.h`, which `gm_vector.h`
  includes. Every copy listed above was replaced. In the DHGR file,
  `CALC(y)` is now a one-line alias. Bocfel's `draw_apple_2.cpp` has no copy;
  it decodes a different format. The copies in `comprehend/test/` were left
  alone: they serve as independent oracles, and their build rules have no
  `-I` for this directory.
  `DrawSingleApple2ImageByte` now also rejects the 8-byte "screen holes"
  (`(offset & 0x7f) >= 120`), which belong to no scanline.
- [x] **[dedup] `MAX_SCREEN_ADDR` defined three times** with three spellings:
  `apple2draw.h` (`MAX_SCREEN_ADDR`, plus `A2_SCREEN_MEM_SIZE`),
  `gm_vector.c` (`GM_MAX_SCREEN_ADDR`) and `comprehend/graphics_magician.cpp`.
  Define one `GM_HIRES_PAGE_SIZE 0x2000` / `GM_MAX_SCREEN_ADDR` in a shared
  header and derive the others. The unprefixed `MAX_SCREEN_ADDR` in a public
  header is also a collision risk.
  → `GM_HIRES_PAGE_SIZE` and `GM_MAX_SCREEN_ADDR` are in `gm_hires.h`.
  `A2_SCREEN_MEM_SIZE`, Comprehend's constants and
  `scott/saga/apple2_vector_draw.c` all use them. `MAX_SCREEN_ADDR` is gone.
- [-] **[dedup] The RLE "two-byte pattern" decoder exists twice (maybe three times).**
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
  → Declined. The common part is about 20 lines. The two loops leave at
  different points: the Apple one returns from inside the column test, and
  the C64 one checks the end of the data before each pair. An iterator would
  have to reproduce both exactly, which risks more than the duplication
  costs. `c64_small.c` reads a different layout and only shares the pixel
  helper. The integer-type mismatch is fixed (section 1).
- [x] **[dedup] Hand-written prototypes of host callbacks.** `pcdraw.c` and
  `c64a8draw.c` each declare `SetColor`, `PutPixelWithWidth`/`PutDoublePixel`
  locally, and `apple2draw.c` declares `extern int pixel_size, x_offset,
  y_offset`, `Graphics`, `ImageHeight` inline. These belong in one header
  (e.g. `sagadraw_host.h`) so the signature is declared once and checked by the
  compiler against the definitions (right now a mismatch would link fine in C).
  → Added `sagadraw_host.h`. The three library files include it, and so do
  the files that define these symbols: `scott/saga/sagagraphics.c`,
  `scott/scott.c`, `plus/graphics.c` and `plus/plusmain.c`. The compiler now
  checks the declarations against the definitions.
- [x] **[dedup] `(HGR_BITS * 2 + 0x40) & 0x80` + `^= 0x7f` appears four times**
  in `gm_vector.c` (`gm_set_draw_position`, `move_left`, `move_right`, plus the
  odd-column variant). Pull it into `static void flip_bits_if_needed(c)` with a
  comment explaining it is the HGR colour-phase flip between adjacent columns.
  → `flip_color_phase()`, with that comment.
- [-] **[dedup] Test harnesses re-implement library functions.**
  `scott/saga/test/c64test.c` has its own `DrawPatternAndAdvancePos`;
  `zxtest.c`, `zxvectortest.c`, `dostest.c` each re-define `PutDoublePixel`
  as `PutPixelWithWidth(x, y, c, 2)`. If the host-callback header above exists,
  a shared `test/sagadraw_stubs.c` can replace these (and `filltest.c`'s
  `unreached(...)` stubs).
  → Declined. Each harness links a different subset of the library, and
  each stub records pixels into that test's own grid. `c64test` keeps its own
  `DrawPatternAndAdvancePos` because it does not link `c64a8draw.c`. A
  shared stub file would need weak symbols or per-test ifdefs, which saves
  only a few lines per test.
- [~] **[dedup] Apple II palette.** `gm_apple2_palette` lives here; check whether
  `terps/bocfel/z6/draw_apple_2.cpp` and the comprehend DHGR code keep their
  own copies and point them at `gm_artifact.h`. Note entries 5 and 10 are both
  `0x808080` (that's authentic: two greys), worth a comment so nobody "fixes" it.
  → Added the comment on the duplicated grey. Comprehend already uses
  `gm_apple2_palette`. Bocfel's table is not a copy: it has different values
  (tuned to AppleWin) in a different index order, for a different renderer,
  so it stays.

## 3. `c64a8draw.c` (the biggest cleanup)

- [x] **[cleanup] `TranslateColorC64` is ~340 lines of `case` ladder.** There are
  about 170 `/* Unused color */` markers and only 12 distinct outputs. Replace
  with a `static const struct { uint8_t value; uint8_t colour_index; }`
  sorted table or a 256-entry `int8_t` array (`-1` = "leave slot unchanged";
  see the hazard in section 1). Colour *names* should become an `enum` or
  array indexed by name instead of 12 loose `static const glui32` variables.
  Net effect: ~340 lines -> ~40, and the table can be diffed against the
  VICE-derived notes much more easily.
  → `enum c64_color`, a `c64colors[]` table with designated initialisers,
  and a 256-entry `int8_t c64colormap[]` that uses `C64_UNUSED` for "leave
  unchanged". The map was generated from the old function's output, checked
  for all 256 inputs, and has a `_Static_assert` on its length. The file went
  from 778 lines to 218.
  → 2026-10-17: `c64colormap` was replaced by the original C64 interpreter's
  algorithm: two 16-entry hue tables picked by luminance, with Claymorgue's
  variant. A short per-interpreter override list holds the deliberate
  readability fixes.
- [x] **[cleanup] `ataricolors[256]` contains patched entries.** Several entries are
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
  → Moved to `atari8_palette.h`, eight entries per line with index comments.
  Remapped lines are marked, and one comment block lists the 18 original
  values. The old table did have exactly 256 entries; the "197" comment on
  index 199 was a mislabel. A `_Static_assert` checks the length, and the
  values are byte-identical.
  → 2026-10-17: the original interpreters write the colour values straight
  to the colour registers, and the GTIA ignores luminance bit 0. So the table
  now holds the unmodified palette and is looked up with `value & 0xfe`. The
  18 hand-picked colours are an override list in `c64a8draw.c` that matches
  exact values only.
- [x] **[cleanup] Unused declarations/includes:** `extern glui32 pal[16];`
  and `#include <stdio.h>` appear unused here.
  → Removed.
- [x] **[cleanup] `DrawPatternAndAdvancePos`** is exported, returns `x - 8`
  (undoing its own `x += 2` four times) and mutates `*y` as a side effect.
  Make the contract obvious (return the new x explicitly or take `int *x`),
  name the magic `8`, and document that it draws a 4x2 double-wide block
  (comment in `DrawC64A8ImageFromData` says 8x2 / "technically 4x2"; pick one).
  → Documented in `c64a8draw.h`. It draws four double-width pixels on one
  row, moves `*y` down a row, and returns `x` unchanged. The signature stays
  the same because `scott/saga/c64_small.c` calls it too and the return
  value does no harm. The "8x2" comment was still there on 2026-10-17 and
  now says "one column of 4 x 2 double-width pixels".
  → Later on 2026-10-17 it became `void`. The callers in `c64a8draw.c` and
  `c64_small.c` no longer assign its result, and the copy in
  `scott/saga/test/c64test.c` matches. `DrawApple2ImageFromData`'s
  `is_the_count` flag became `enum apple2_variant` (`APPLE2_STANDARD`,
  `APPLE2_COUNT`) at the same time, to match the C64 side.
- [x] **[cleanup] `voodoo_or_count` and `is_c64` are int flags** threaded through
  `DrawC64A8ImageFromData`. An `enum { SAGA_RLE_COUNT, SAGA_RLE_FLAGGED }`
  and `enum { PLATFORM_C64, PLATFORM_ATARI8 }` would make call sites readable
  (`voodoo_or_count` is also used for `bottom -= 2`, a second meaning).
  → `is_c64` was replaced on 2026-10-17 by `enum c64a8_palette`
  (`C64A8_ATARI8`, `C64A8_C64`, `C64A8_C64_CLAYMORGUE`), because the
  Claymorgue interpreter uses different tables.
  → Later the same day `voodoo_or_count` went too. Its "two meanings" are
  both properties of one thing, the early Atari interpreter used by The
  Count and Voodoo Castle, and the palette enum already named interpreters.
  So it is now `enum c64a8_variant` with a fourth value,
  `C64A8_ATARI8_COUNT_VOODOO`, and the function takes one parameter fewer.
  The renderer derives `counted_rle` and `c64` from it locally. Call sites
  got shorter: Plus dropped its bare `0`, Scott has a small `C64A8Variant()`
  helper, and the test's `!voodoo` directive sets the variant.
- [x] `adjustments_fn` is `int(*)(int,int,int*)`: it both returns `right` and
  mutates `left` via pointer. Consider a small `struct image_bounds` in/out
  parameter. The Apple version uses a different typedef (`adjust_plus_fn`,
  `void(*)(uint8_t,uint8_t,uint8_t)`) for the same purpose; unify.
  → 2026-10-17: now `struct c64a8_bounds { left, top, right, bottom }` and
  `typedef void (*c64a8_adjust_fn)(struct c64a8_bounds *)`. The three
  callbacks (Scott, Plus, c64a8test) read and write named fields instead of
  positional ints. Unifying with the Apple callback stays declined: it is
  read-only, carries `top` rather than `left`, and only tells Plus the image
  size.

## 4. `apple2draw.c` / `apple2draw.h`

- [x] **[cleanup] Stale or dead declarations in the header:**
  `void test_move_up_and_down(void);` has no definition or caller anywhere in
  the tree; `APPLE_SCREEN_WIDTH` (apple2draw.c) is unused;
  `#include "read_le16.h"` is unused in `apple2draw.c` (also in `pcdraw.c`
  it is used, but `<stdint.h>` is missing from `pcdraw.h`, which uses `uint8_t`
  and relies on include order). `<stdlib.h>` in the header is only needed for
  `size_t`; use `<stddef.h>`.
  → All done. `plus/graphics.c` had been getting `<stdlib.h>` through
  `apple2draw.h`, so it now includes it directly.
- [-] **[design] Two public ways to draw** (`DrawApple2ImageFromVideoMem` and
  `...WithFlip(0)`) plus `ClearApple2ScreenMem`, `DrawSingleApple2ImageByte` and
  two exported globals (`screenmem`, `descrambletable`). Make `screenmem` /
  `descrambletable` accessed through functions or a context struct; the header
  comment says "NULL until first drawn to", which every caller has to know.
  A single `Apple2ScreenState` (screenmem + descrambletable + flip) would also
  let tests link without the 10-line stub files.
  → Declined. Scott, Plus, the Apple II vector renderer and the tests all
  read `screenmem` directly. There is only ever one Apple screen, so a state
  struct would be a lot of churn with no new capability.
- [x] **[cleanup] Lazy `screenmem` allocation is repeated** in
  `DrawScrambledApple2Image` and `DrawApple2ImageFromData`
  (`if (screenmem == NULL) screenmem = MemCalloc(...)`). Wrap in
  `static uint8_t *ensure_screenmem(void)`. In `DrawApple2ImageFromData` it is
  also done *before* the descrambletable branch that already allocates, i.e. the
  scrambled path does it twice.
  → `EnsureScreenMem()`, called once.
- [x] **[cleanup] `RenderLineWithA2ArtifactColors` run-length logic** is hard to
  follow (own comment: "not sure why" about the +1 overlap, plus a special
  case for the last pixel that re-fetches `glkcolor`, shadowing the outer
  variable). Rewrite as a plain "find run end, emit run" loop over the flat
  `colors560[]` array (the double `col`/`b` loop is not needed once it is a flat
  560-element scan). Also hoist the magic numbers (560, 319, 14, 40) into
  named constants, and make the flip a mirror helper, since
  `PutApplePixelFlippable` is the only place that knows the geometry.
  → Rewritten as a flat scan over `colors560[]` that draws each run of
  one colour with one fill_rect (`A2_LINE_PIXELS`, `A2_PLOT_HEIGHT`,
  `PutApplePixels`). The old loop drew every run except the first one
  pixel to the right of where it belonged, and lost single-pixel runs.
  That is why it needed the "+1 overlap". It made the same number of
  fill_rect calls.
  Visual check: Spider-Man (SpiderMan-S1/S2.dsk) was booted in MAME
  `apple2e`, and its title screen and main menu were captured as $2000 page
  dumps and screenshots taken while paused. On the 160 picture rows the new
  renderer, and the Plus probe's own `title` picture (dsk, do and woz), match
  MAME pixel for pixel. The old renderer differed on 6954 pixels.
  The only differences left are in rows 160-191, MAME's mixed-mode text
  window, which this code never draws. The Plus `imagecheck` goldens of the
  15 Apple II rows were re-blessed, and only the pixel CRC changed.
  Comprehend is not affected: both of its blits copy the colours pixel by
  pixel. The DHGR kernel (`gm_render_dhgr_line_colors`) was checked the same
  way on the OO-Topos `<D>` title, with main and aux pages from MAME. It
  matches MAME's composite monitor with "Pixel-run (color bias)" on every
  pixel of the 560x192 screen.
  Two changes to upside-down drawing (Spider-Man on the ceiling). It is now
  an exact 180° mirror at every `pixel_size`; before, it was off by
  `pixel_size - 1` rows. It also leaves out the extra row 160, which used to
  land two rows above the top of the picture.
- [~] **[cleanup] Shadowed/ambiguous names:** the two inner `glkcolor` variables, the
  `uint8_t repetitions, i` mix noted above, `pattern_top/bottom` initialised to 0
  and never needed before assignment, `origptr` used only for a length.
  → The `glkcolor` shadowing and the `i` type are fixed. The zero
  initialisers and `origptr` are harmless and stay.
- [-] **[design] Host globals** (`Graphics`, `pixel_size`, `x_offset`, `y_offset`,
  `ImageHeight`) make this "common" library depend on `scott.h` globals.
  Passing a small `{winid_t, pixel_size, x_off, y_off, height}` struct (or the
  plotting function pointer) would let Scott, Plus and the tests share it
  without the stub files seen in `terps/scott/saga/test/`.
  → Declined. `sagadraw_host.h` now writes the contract down in one place
  and the compiler checks it. Every caller would have to pass a struct for no
  behavioural gain.

## 5. `pcdraw.c` / `pcdraw.h`

- [x] **[cleanup]** `DrawState` has `at_last_line` used as a counter (`++`) and
  compared `<= 1`; rename to `last_line_passes` or similar and explain the
  "draw the last line twice" interleaving. `ycount`/`bottom`/`top` mix row
  counts and pixel rows without comments.
  → Renamed to `passes_done` (loop test `< 2`), with comments on the
  interlaced even/odd bank layout and on the row fields.
- [x] **[cleanup]** The doc comment above `DrawPattern` ("Skip every other pixel
  when pixelwidth is 1") doesn't match the code (it draws all four pixels and
  `pixelwidth` just scales them). Fix the comment or the code.
  → Comment fixed.
- [~] **[cleanup]** Header offsets `0x05`, `0x0d`, `0x0f`, `0x11`, `0x13`, `0x17`
  and the CGA palette literals should be named constants. Header-parse
  could be a separate `parse_dos_header()` returning a struct, which also makes
  it testable and lets us add a `size` sanity check (`ptr + size` is never
  compared to the caller's real buffer length; `ptr + 0x17` is not
  bounds-checked at all).
  → The offsets are `DOS_HEADER_*` constants, and the palette is a commented
  four-entry table. `DrawDOSImageFromData(ptr, datasize)` now takes the
  buffer length: it returns 0 if the data is shorter than the header and
  clamps the declared size to the buffer. All 286 real PAKs declare a size
  no larger than their file, so only the empty B019.PAK changes. No separate
  header-parse struct: the header is read in one place.
  `st.top = rawoffset / 40` and the `- 24` left margin stay as they are.
- [-] `SetColor` is called 4 times with a constant palette on *every* image
  draw; harmless but could be done once.
  → Declined. Scott and Plus share palette slots 0..3 with the other
  renderers, so a C64 or Apple image drawn earlier may have overwritten them.
  Setting them on every draw is what keeps DOS images correct.
- [x] `size_t size = READ_LE_UINT16(...)` / `(void)` returns: function always
  returns 1 when `ptr` non-NULL; either return a real success value or make
  it `void`.
  → It now returns 0 for NULL or data too short for a header, and 1
  otherwise.

## 6. `gm_vector.c` / `gm_vector.h`

- [x] **[cleanup] Misleading name `COL_BITS`** (7). It is *pixels per hi-res byte*,
  not bits per column. Rename `GM_PIXELS_PER_BYTE`. Likewise `HGR_HORIZ` is the
  column *byte* index.
  → `GM_PIXELS_PER_BYTE` (in `gm_hires.h`; Comprehend's copy is gone too).
  The `HMASK` and `HGR_HORIZ` field comments now say what the fields hold.
- [~] **[cleanup] Magic numbers:** 191 / 192 (rows), 40 (columns), 39, `0xc0`,
  `0x81`, `0x7f`. Add `GM_ROWS`, `GM_COLS`, etc. The off-by-one traps in
  `move_up_or_down` (`> 191` vs `(int8_t) < 0`) are already commented; naming the
  constants would remove the need.
  → Rows and columns now use `GM_HIRES_ROWS` and `GM_HIRES_COLS`. The masks
  `0xc0`, `0x81` and `0x7f` stay as literals with their existing comments:
  they are the Applesoft HMASK values, and names would hide the bit
  patterns.
- [-] **[cleanup] `move_left_or_right(bool left, ...)`** is a one-line wrapper over
  `move_left`/`move_right`; same for `move_up_or_down(bool down,...)`. Fine
  as-is, but the draw-line loop can call the direction functions directly.
  → Kept, as the item itself suggests. The loop reads as HGLIN does.
- [x] **[cleanup] `gm_set_color` silently ignores `color_index >= 8`.** Either
  assert (debug builds) or document "unchanged on invalid".
  → Documented in `gm_vector.h`.
- [x] **[cleanup] Stale header text:** `gm_vector.h` describes the file as shared
  by `scott/saga/apple2_vector_draw.c` and comprehend, and the .c file says
  it is "a verbatim extraction ... must not change behaviour". Keep the
  warning, but now that the duplicates are gone make sure the paths mentioned
  still exist and the brush code (added later, not a verbatim extraction) is
  covered by the comment.
  → Both header comments are rewritten. They cover cursor, line and brush
  code, and keep the warning not to change behaviour (6502 write for write,
  including the byte wraps).
- ~~**[cleanup] `brush_quadrant`** mutates the scratch struct and then undoes it~~:
  done. `gm_brush_scratch` is gone; `brush_quadrant()` takes its column, first
  scanline and bitmap as parameters, and `brush_blend()` does the two writes.
- [-] `brush_quadrant()` takes `uint8_t scanline` and `uint8_t column`, both of
  which are summed with offsets (`+ 8`, `+ 1`) and can wrap near the bottom/right
  edge; `brush_quadrant` re-checks `address <= GM_MAX_SCREEN_ADDR`, but
  `gm_row_address(uint8 scanline wrapping to 0..)` would wrap onto the top of the
  screen rather than clip. Use `int` for both and clip explicitly.
  → Declined for the same reason as the brush index in section 1. The 6502
  wraps these bytes the same way, and the hardware captures show it. The
  comment on `brush_quadrant` now says so.

## 7. `gm_artifact.c` / `gm_artifact.h`

- [x] **[cleanup] `gm_double7bits` builds its table lazily with a static flag.**
  Not thread-safe and needlessly runtime. Generate it at compile time (a
  128-entry `static const uint16_t[]`), or build it in a constructor-free
  `static` initialiser. Also note `table[0]` relies on static zero-init.
  → It is now a compile-time `static const uint16_t double7bits[128]`.
- [x] **[cleanup]** `rotl4b` comment says "bits 4-6 of n must be a copy of bits
  0-2" but the function is a right shift with a masked count; rename to
  `rotate_color` or fix the comment. `CONTEXTBITS` and the `<< 14` should be
  tied together via a named constant.
  → The comment is corrected. It explains that a left rotate by `count`
  mod 4 is done as a right shift by `(4 - count)` mod 4, which works because
  every LUT entry repeats its nibble. `CONTEXTBITS` is commented, and the
  kernel uses `PIXELS_PER_WORD` (14).
- [x] **[cleanup]** `gm_compute_dhgr_row_words` / `gm_render_dhgr_line_colors`
  are only used from the comprehend DHGR code. Fine, but the header comment
  block ("verbatim extraction ... the two copies were byte-for-byte identical")
  is history that belongs in git, not the source.
  → History trimmed.
- [-] **[dedup]** `gm_render_line_colors` / `gm_render_dhgr_line_colors` differ
  only in `phase`; an exposed `gm_render_line_colors_phase()` would let callers
  pass `is_80_column` directly.
  → Declined. Both are already one-line wrappers over the same static
  kernel. Each caller knows at compile time which mode it draws, so a third
  public entry point would add nothing.

## 8. Cross-cutting

- [x] **[cleanup]** Mixed C dialect/style: tabs vs spaces, `//` vs `/* */`, inconsistent
  brace style and `uint8_t`/`int` for loop variables across the five source
  files. Pick one (the surrounding repo uses 4-space) and run a formatter on this
  directory *after* the functional changes so the diff stays reviewable.
  → Done as its own pass (2026-10-17), after the functional commits. The
  style is WebKit, as most Scott and Plus code uses: function braces on
  their own line, 4-space continuation indent, no statement on the same
  line as its `if`. Comments are `//`, except the `/* 0 */` index markers
  inside table rows and the include-guard `#endif` comments. There were
  no tabs. The edits were made by hand, with clang-format (WebKit style)
  used only as a checklist: run on the whole directory it puts one table
  entry per line and pulls apart aligned columns. Every `.c` file
  compiles to byte-identical objects (`-O2`, code and data) compared with
  HEAD. For that reason the one `uint8_t` loop counter (`gm_spread7`) was
  left alone: as `int` it is equivalent but compiles differently. Also
  fixed a "vaule" typo and dropped a redundant "calculate address"
  comment.
- [-] **[design]** No unit tests live in this directory; coverage is spread across
  `scott/saga/test/` and `comprehend/test/` using stubbed host symbols
  (`filltest.c`, `c64test.c`, ...). A `common_sagadraw/test/` with the shared
  stubs (section 2) and golden-image comparisons for `gm_artifact`,
  `gm_vector` (line/brush) and the RLE decoders would make the
  refactors above safe to do incrementally.
  → Not added. The existing suites already compare every entry point
  against hardware-derived goldens. For this pass, a throwaway differential
  harness (old build against new, real and fuzzed blobs, plus ASan) did the
  incremental checking.
- [-] **[design]** Names are inconsistent: `gm_*` (Graphics Magician), `Draw*Apple2*`,
  `DrawC64A8*`, `DrawDOS*`, `A2_*`, `MAX_SCREEN_ADDR`. Consider a consistent
  prefix for new/renamed API (`sagadraw_*`) and keep old names as thin wrappers
  until callers (`scott/saga`, `plus`) are migrated.
  → Declined. `MAX_SCREEN_ADDR` is gone. A rename with wrappers would
  double the API for a while and gain little.
- Suggested order of work: (1) fix the bugs in section 1 with a test for each;
  (2) share `gm_row_address` / screen-size constants; (3) add the host-callback
  header and shared test stubs; (4) table-ise the C64/Atari colour mapping;
  (5) unify the RLE iterator; (6) formatting pass.
  → Steps 1 to 4 are done, without the shared test stubs. Step 5 is
  declined, as noted above. Step 6 was done later, as its own pass. No test rows were added for the section 1 fixes:
  they only affect malformed data and count mode, which the real corpus
  never reaches. The differential harness covered them with fuzzed blobs.
