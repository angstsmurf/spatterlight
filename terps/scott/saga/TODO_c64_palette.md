# TODO: make the C64 colours hardware-accurate

## What's wrong now

`colorC64[16]` in `c64_small.c` (around line 346) is an ad-hoc palette that
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

## Why the regression tests don't catch this

`c64_decode_png.py` (and the c64a8 decoder) classify each VICE screenshot pixel
to the **nearest renderer colour** and write the golden in the renderer's own
colour space, so `make c64test` / `make c64a8test` / `make groundtruth` pass
regardless of how far the palette drifts. This is therefore purely an
**on-screen fidelity** fix for Spatterlight, not a test-correctness one — but see
step 4 for turning the test into a real palette guard once it's fixed.

## Plan

### 1. Pick the reference palette
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

### 2. Replace `colorC64[16]` with the Colodore values
Drop in the table above, in index order, with correct grey labels. This is the
whole functional change.

### 3. Make the renderer and the golden captures agree
Confirm which palette the existing VICE screenshots under
`groundtruth_c64/` and `groundtruth_c64a8/` were taken with (x64sc's
`-VICIIpalette` / Settings → Video). If they used Colodore, renderer and capture
now match exactly. If they used something else, either re-shoot the goldens with
Colodore or set the renderer to that same palette — the two must use the same one.

### 4. (Optional) Turn the test into a palette guard
Once renderer and captures share a palette, tighten the decoders so the golden
is written in **true C64 RGB** and the compare is exact RGB-equality (drop the
"nearest renderer colour" classification in `c64_decode_png.py` /
`c64a8_decode_png.py`). Then any future palette drift fails `make groundtruth`.

### 5. Don't forget the sibling tables
- `c64a8draw.c` (`common_sagadraw/`) — full-bitmap C64 path; check whether it
  has its own colour table or shares `colorC64`. Fix both if separate.
- `atari_8bit_vector_draw.c:554` `RGBpalette[16]` is the **Atari** GTIA palette,
  a different chip (NTSC). Out of scope for C64 accuracy; track separately if
  Atari fidelity is also wanted.

### 6. Verify
- `make c64test && make c64a8test && make groundtruth` — still byte-exact.
- Eyeball a known room (e.g. Pirate room 0) in Spatterlight against a VICE
  screenshot of the same room; greys and the red/blue ramp should now match.

## References
- Colodore: https://www.colodore.com (generator + .vpl)
- VICE palettes ship as `colodore.vpl`, `pepto-pal.vpl`, `pepto-ntsc.vpl`
- Related: `saga-c64-groundtruth` work in the project memory notes
