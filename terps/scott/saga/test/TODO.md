# Scott pictures: open work

Goal: pictures that look like the originals'. What is already tested and settled
is in `README.md`.

## Coverage

- [ ] Not covered yet: the Gremlins gang animations (square, road), German and
      Spanish Gremlins, Super Gran, Savage Island, C64 animations
      (`c64_capture.py` has no `!hwnext`).
- [ ] Not covered yet: the US SAGA bitmap pictures on MS-DOS and the Atari
      8-bit as the interpreter draws them. `dostest` and `c64a8test` plot
      through their own stubs, so nothing goes through `PutPixelWithWidth()`
      in `sagagraphics.c` and its run merging (2026-10-14). That was checked
      once by hand: scratch scenes with a `!check` after every walkthrough
      command, `scenetest -d` with a binary from before and one from after,
      all 967 frames identical. The C64 ones are covered since 2026-10-31
      (`groundtruth_scene/hulkus_pictures_c64`, `claymorgueus_pictures_c64`,
      every picture of both games, `us_scenes.py`); a scene with a hardware
      dump of one room per other machine would keep those checked.
      `scenetest -c` prints the rectangles filled per check.

## Divergences from the originals (found by the scene tests)

Where a picture is compared anyway it is a `!check ... min=N` in its scene,
with a comment there; those that can't be compared are left out of the scenes.
Decide per item whether the interpreter should follow the original.

- [ ] **Howarth vector pictures, line colour (ZX).** The Spectrum original
      draws its lines in the complement of the background colour (cyan on the
      oak's red, magenta on the hedge's green); the interpreter uses black,
      white on a black background. The C64 original uses black too, so this
      would be a Spectrum-only change: `line_colour = bg_colour ^ 7` in
      `DrawHowarthVectorPicture` (`ai_uk/line_drawing.c`) takes `baton_zx` oak
      from 93.36% to 97.61% and hedge from 91.36% to 98.75%.
- [ ] **Howarth vector pictures, colour clash (ZX and C64).** Both machines
      have two colours per 8x8 cell, so an outline changes colour where a fill
      touches its cell; the interpreter gives every pixel its own colour. The
      fills match everywhere. `baton_zx` and `baton_c64`: forest 95.26%, stream
      93.97%, clearing 97.63%, oak 93.36% / 95.96%, hedge 91.36% / 98.75%.
- [ ] **Adventureland forest (ZX), one cell.** The picture asks for tile
      132 + 128 = 260 at cell (5,8); the original reads the 8 bytes past its
      256-tile table (the start of the image address table) and shows them as
      noise, the interpreter wraps around to tile 4, a foliage tile. 22 pixels.
- [ ] **Robin of Sherwood, Herne's cave (ZX).** The original leaves a mark of
      the outside waterfall on the cave picture: the six columns it occupied
      get scrolled down one more pixel on some rows, which ones depending on
      the timing of the keypresses (236 pixels in the capture, 99.04%).
- [ ] **Hulk (C64), last line of every picture.** The original switches from
      bitmap to text mode in the middle of the picture's bottom line: from 112
      pixels in, that line shows the empty text screen. 99.29–99.83%.
- [ ] **Hulk and Claymorgue US (C64), the palette is global.** Every picture
      drawn writes its four colours into the whole screen and colour RAM, so
      a room with object pictures over it ends up in the colours of the last
      one drawn: Hulk's chair room after the ring (98.4%), its bees after the
      wax (99.7%), the large hole in the ring's red (94.8%). The interpreter
      keeps each picture's own colours, which is what the pictures were drawn
      with. Likely leave as it is.
- [ ] **Spider-Man, Madame Web (C64), one cell.** The tile at cell (24,3) has
      the wrong colours in the T64 copy; the interpreter patches it on purpose
      (`image_patches` in `sagadraw.c`). Nothing to do unless the patch is
      dropped.
- [ ] **Seas of Blood, start room before the first move.** The original draws
      it before the opening automatic actions have placed the coastline; the
      interpreter draws it after. Not checked for that reason.
- [ ] **Hulk tape loading screen (ZX).** The original starts by itself once
      loaded, and the ROM prints `Bytes: hulk (tm)` across the picture for the
      next block; the interpreter shows the finished picture until a key.

## C64 colours: make the palette hardware-accurate

- [ ] Not started. The plan:

### What's wrong now

`colorC64[16]` in `saga/c64_small.c` is an ad-hoc palette that
matches no recognised C64 reference (not Colodore, not Pepto, not VICE):

```c
glui32 colorC64[16] = {
    0x000000, // black
    0xffffff, // white
    0xbf6148, // red
    0x99e6f9, // cyan
    ...
    0x7B7B7B, // light grey   <-- index 11
    0xa7a7a7, // grey         <-- index 12
    0xc0ffb9, // light green
    0xa28fff, // light blue
    0x454545, // dark grey     <-- index 15
};
```

Two separate problems:

1. **The RGB values are eyeballed**, not derived from the VIC-II. Reds/cyans/
   blues are noticeably off from any measured palette.
2. **The grey ramp is mislabeled and out of order.** Standard C64 colour order
   is `11 = dark grey`, `12 = medium grey`, `15 = light grey`. Here index 11 is
   tagged "light grey" with a medium value, and index 15 is tagged "dark grey".
   So even the *indices* are wrong, which can pick the wrong grey in art that
   uses the ramp.

### Why the regression tests don't catch this

`c64_decode_png.py` (and the c64a8 decoder) classify each VICE screenshot pixel
to the **nearest renderer colour**, and `scenetest.c` reads `.c64` dumps as
colour indices, and write the golden in the renderer's own
colour space, so `make c64test` / `make c64a8test` / `make groundtruth` pass
regardless of how far the palette drifts. This is therefore purely an
**on-screen fidelity** fix for Spatterlight, not a test-correctness one — but see
step 4 for turning the test into a real palette guard once it's fixed.

### Plan

#### 1. Pick the reference palette
Use **Colodore** (Pepto's 2016 measured PAL palette; VICE's default since 3.x).
It's the de-facto "correct" C64 palette. sRGB values:

| # | name        | hex      | | # | name        | hex      |
|---|-------------|----------|-|---|-------------|----------|
| 0 | black       | `000000` | | 8 | orange      | `8e5029` |
| 1 | white       | `ffffff` | | 9 | brown       | `553800` |
| 2 | red         | `813338` | |10 | light red   | `c46c71` |
| 3 | cyan        | `75cec8` | |11 | dark grey   | `4a4a4a` |
| 4 | purple      | `8e3c97` | |12 | grey        | `7b7b7b` |
| 5 | green       | `56ac4d` | |13 | light green | `a9ff9f` |
| 6 | blue        | `2e2c9b` | |14 | light blue  | `706deb` |
| 7 | yellow      | `edf171` | |15 | light grey  | `b2b2b2` |

(Alternative if a capture turns out to use it: the older **Pepto PAL** palette —
`68372b` red, `70a4b2` cyan, `352879` blue, `444444`/`6c6c6c`/`959595` greys.)

#### 2. Replace `colorC64[16]` with the Colodore values
Drop in the table above, in index order, with correct grey labels. This is the
whole functional change.

#### 3. Make the renderer and the golden captures agree
Confirm which palette the existing VICE screenshots under
`groundtruth_c64/` and `groundtruth_c64a8/` were taken with (x64sc's
`-VICIIpalette` / Settings → Video). If they used Colodore, renderer and capture
now match exactly. If they used something else, either re-shoot the goldens with
Colodore or set the renderer to that same palette — the two must use the same one.

#### 4. (Optional) Turn the test into a palette guard
Once renderer and captures share a palette, tighten the decoders so the golden
is written in **true C64 RGB** and the compare is exact RGB-equality (drop the
"nearest renderer colour" classification in `c64_decode_png.py` /
`c64a8_decode_png.py`). Then any future palette drift fails `make groundtruth`.

#### 5. Don't forget the sibling tables
- `c64a8draw.c` (`common_sagadraw/`) — full-bitmap C64 path; check whether it
  has its own colour table or shares `colorC64`. Fix both if separate.
- The Atari GTIA palette is a different chip (NTSC). Out of scope for C64
  accuracy; track separately if Atari fidelity is also wanted.

#### 6. Verify
- `make c64test && make c64a8test && make groundtruth` — still byte-exact.
- Eyeball a known room (e.g. Pirate room 0) in Spatterlight against a VICE
  screenshot of the same room; greys and the red/blue ramp should now match.

### References
- Colodore: https://www.colodore.com (generator + .vpl)
- VICE palettes ship as `colodore.vpl`, `pepto-pal.vpl`, `pepto-ntsc.vpl`

