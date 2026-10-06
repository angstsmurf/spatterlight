# Scott pictures: tests and fidelity — TODO

Goal: a byte-exact regression test for every image-format renderer the Scott
interpreter ships, and pictures that look like the originals'. See
`scott-image-format-tests` memory note for full context.

## Done
- [x] Apple II vector / Atari 8-bit vector — `make test` / `make groundtruth`
- [x] Apple II bitmap (`apple2draw.c`) — `make apple2test`
- [x] DOS CGA bitmap (`pcdraw.c`) — `make dostest`
- [x] TI-99/4A RTPI (`ti994a/rtpi_graphics.c`) — `make titest`
- [x] C64 "tiny" / mini-C64 (`saga/c64_small.c`) — `make c64test`
      - keystone `build/extract_images_c64` (real unp64 + c64decrunch) unblocks
        packed C64 games; composite room rendering via `.spec`; Pirate room 0 100%
- [x] **C64/Atari-8 full bitmap** (`common_sagadraw/c64a8draw.c`,
      `DrawC64A8ImageFromData`) — `make c64a8test`
      - Blobs: `build/c64extract` reads the loose `R###`/`B###`/`S###` files off
        the SagaPlus disks (Spider-Man `questprobe_spider-man[sharedata_1987]
        .d64`). Harness `c64a8test.c` unity-includes `c64a8draw.c`; `.spec`
        format like c64test plus `!cliptop N` / `!atari8` / `!voodoo` directives.
      - Golden via VICE (`vice-mcp` x64sc) + `c64a8_decode_png.py` (own decoder:
        wider X search for the +24px draw margin, dx=56; edge-weighted alignment
        so the periodic checker floor can't fool it).
      - `spiderman/room02_corridor` (blob R002) **100% byte-exact** (44240 px).
      - Gotcha found: rooms are drawn 2px into the PAL **top overscan border**
        (all have top=0,bottom=158), so their top 2 rows show as black on real
        hardware → `!cliptop 2`.

## Remaining renderers (untested)

- [x] **ZX Spectrum Irmak/tile** (`aiukgraphics/irmak.c`) — `make zxtest`,
      golden `groundtruth_zx/gremlins/town_overrun` = **100%** (24576/24576).
      - Blocker solved: ZX graphics don't reach `USImages`, so a new keystone
        `build/zxextract` runs the real `DetectGame`→`SagaGraphicsSetup` and dumps
        the irmak globals (`tiles.bin` + per-picture `pic<NNN>.dat` + geometry).
        `zxtest.c` unity-includes irmak.c, supplies the PutPixel/RectFill sinks,
        links palette.c. Golden = real Spectrum `SCREEN$` (0x4000/0x5800) dumped
        from MAME `spectrum -snapshot <game.z80>`, decoded with the ZXOPT palette
        (`/tmp/zx_decode.py`). Used Gremlins' full-screen death scene (no item
        overlays). See groundtruth_zx/README.md.
      - `zxtest cmp` also takes a raw `SCREEN$` (`.scr`) as golden, so a
        picture can be captured with `zx_capture.lua` and no decoding step.
        Five such goldens pin the direct-overlay rule (after a command with
        ADD_128: +128 in versions 3–4, not in 0–2) for the old versions: Hulk
        pictures 14 and 40 (v0), Claymorgue 2 (v1), Spider-Man 2 and 20 (v2) —
        the only v0–2 pictures that have the case. No script walks to them;
        `!hwpoke` lines in each `capture.scene` point the original's picture
        table at them instead.

- [x] **ZX Spectrum Howarth/vector** (`ai_uk/line_drawing.c`,
      `DrawHowarthVectorPicture`) — harness built (`zxvectortest`) + `zxextract`
      dumps the `LineImages[]` vector streams. The rasterizer is now **ROM-exact**
      (RE'd from the Golden Baton `.z80` in Ghidra): line geometry is **100%**
      (1740/1740 drawn px on Golden Baton's opening room, was 38%) and end-to-end
      colour match is **95.26%** (was 94.11%).
      - The residual ~5% is **inherent**: the Spectrum's 8×8 attribute clash makes
        a white outline crossing a red-filled cell display as red. Reproducing it
        would make Spatterlight's output *worse* for users, so it is deliberately
        not done — hence no exact-colour `.zxvec` golden is committed. If a
        regression guard is wanted, assert set-bit *geometry* (lines == 100%).
      - See `ai_uk/NOTES_howarth_vector.md` for the ROM anchors and method.

- [x] **Atari 8-bit bitmap** (`c64a8draw.c` with `is_c64=0`) — `make c64a8test`,
      **four** Atari goldens, all 100% byte-exact:
      `hulk_atari8/room01_chair`, `count_atari8/room01_bed`,
      `voodoo_atari8/room01_coffin`, `claymorgue_atari8/room01_castle`.
      - Unblocked: the companion-disk pairs live in `~/Desktop/Interactive
        Fiction experiments/Saga and TaylorMade stuff/Atari 8-bit saga/`. Dump
        blobs with `build/extract_images <Side A/Disk 1 .atr> <outdir>` (real
        `DetectAtari8`; the `(Side B)`/`(Disk 2)` sibling is auto-found).
      - Coverage: this exercises the `is_c64=0` `TranslateColorAtari8` +
        `ataricolors[256]` path (so c64a8 is covered on both C64 and Atari), the
        `!voodoo` RLE variant (Count/Voodoo) **and** the plain bit-7 RLE branch
        (Hulk/Claymorgue), plus the first Atari **object-overlay** golden
        (Voodoo coffin = bg `u0_i001` + object `u1_i017`; bg alone is 94.98%).
      - Goldens captured live in **MAME `a400 -ramsize 48K`** (a800 hangs on the
        SAGA loader); see groundtruth_c64a8/README.md for the keyboard/disk-swap
        recipe and `/tmp/scan_atari.py` blob-identifier. Hulk's golden reused the
        ready-made `hulk03.png`.
      - Avoid Hulk room 0 (`a8croplist` cropright=32) and index 19 (atari8
        width++ special case) — the harness `adjust_room` mirror models neither;
        other rooms use the plain `width-1` room adjustment and stay exact.

## Scene tests (the picture paths above the renderers)

- [x] **`make scenetest`** — `scenetest.c` runs the real `glk_main()` on a
      `.scene` command script and compares the graphics window with screen
      dumps of the original at the same point of the same script. Goldens are
      captured by playing the same script on the original: `zx_capture.lua`
      (MAME) and `c64_capture.py` (VICE). See `groundtruth_scene/README.md` for
      the scene table, what is inexact and why, and the capture recipes.
      - Covers: Irmak v0/1/2/4 (Hulk, Adventureland, Secret Mission,
        Claymorgue, Spider-Man, Robin, Seas of Blood) next to zxtest's v3;
        Seas of Blood's `taylordraw.c` pictures; Howarth vector pictures as the
        game draws them, with slow draw; Robin's forest composites and
        waterfalls; the Gremlins animations frame by frame; the ZX tape
        loading screen; C64 tile graphics in all four palettes (C64A–D), the
        C64 image patches and the Claymorgue copy with broken pictures.
      - Found and fixed on the way: Irmak v3 direct overlays ignored the
        128-tile bank bit (`irmak.c`), and Robin's cave waterfall was drawn in
        inverted colours (`robin_of_sherwood.c`).
      - `seas_zx/cave.scene`: the same direct-overlay case in v4 (the
        underwater cave). The original is poked there (`!hwpoke`); the
        interpreter walks, which takes three battles — the scene build defines
        `AUTOWIN`. That walk found `draw_border()` looping four billion times
        on a battle window without rows (fixed).
- [x] **Every picture of Seas of Blood**, `seas_pictures_zx/` (124 scenes) and
      `seas_pictures_c64/` (123), written by `seas_scenes.py`: every room as
      the game starts, every room without its things, and every thing that is
      elsewhere at the start (or shares its room) alone where it is drawn. No
      walk gets to them all, so each scene writes the room, the items and,
      for what is seen from the ship, the position counters into the game
      state: `!hwpoke` for the original, the new `!room`, `!item`, `!counter`
      and `!flag` for the interpreter. Chance is switched off the same way
      (the merchant ships, the roc of room 45, the crevasse of room 71).
      All exact but the two entries under Divergences. `c64_capture.py` got
      `!hwsnap` (start from a VICE snapshot instead of loading the disk: 93 s
      down to 9 s a scene) and `VICE_PORT` (several captures at once).
- [ ] Not covered yet: the Gremlins gang animations (square, road), German and
      Spanish Gremlins, Super Gran, Savage Island, C64 animations
      (`c64_capture.py` has no `!hwnext`).

## Divergences from the originals (found by the scene tests)

Where a picture is compared anyway it is a `!check ... min=N` in its scene,
with a comment there; the last three are left out of the scenes. Only the
ticked one is ported: decide per item whether the interpreter should follow
the original.

- [x] **Seas of Blood, the stone hall (C64), two cells.** Deliberate: the
      patch table of `sagadraw.c` (`SEAS_OF_BLOOD_C64`, image 34, offset 471)
      takes a stray flip bit off one byte. Rooms 28, 61 and 69 share
      sub-image 0x22, which is mirrored to the right. Its cell at the foot of
      the outer pillar, (2,11), is `a5 08 e0 03` on the C64 disk where the
      eleven cells above it, and all twelve on the Spectrum, are
      `a5 08 a0 03`: the overlay tile 3, a vertical line, turned by 180
      degrees and then mirrored, which puts it back where it began (0x20 a
      row for 0x04). The C64 draw routine (`$1E48`: rotate, then flip on
      `$40`, then overlay) does what the byte says, as `irmak.c` would. 32
      pixels (`seas_pictures_c64`, `min=24544` in 8 scenes).
- [x] **Seas of Blood, the crypt (ZX).** Deliberate: `PatchCryptImage()`
      takes the BRIGHT off four cells beside the sarcophagus. 141 pixels
      (`seas_pictures_zx/room13`, `item051`, `min=24435`); the C64 picture is
      exact.
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
- [x] **Gremlins, kitchen after the blender has run.** The original only
      redraws a picture when something in the room changes, so the blender
      keeps the frame it stopped on until then (the chute opening brings back
      the first). Ported (`GremlinsKeepBlenderFrame` in `ai_uk/gremlins.c`);
      `gremlins_c64` is exact now and `gremlins_zx/blender.scene` pins the
      second frame on the Spectrum.
- [ ] **Hulk (C64), last line of every picture.** The original switches from
      bitmap to text mode in the middle of the picture's bottom line: from 112
      pixels in, that line shows the empty text screen. 99.29–99.83%.
- [ ] **Spider-Man, Madame Web (C64), one cell.** The tile at cell (24,3) has
      the wrong colours in the T64 copy; the interpreter patches it on purpose
      (`image_patches` in `sagadraw.c`). Nothing to do unless the patch is
      dropped.
- [x] **Claymorgue (ZX), ENTER MOAT.** Not a divergence: the `.z80` the scene
      used is a damaged copy (pictures 9–35 broken, crashes to BASIC; the
      interpreter patches them out). Whole copies exist — `claymorgue.sna` and
      both `.tzx` tapes — and all 37 pictures are byte-exact against the
      original (`groundtruth_zx/claymorgue_N`, `claymorgue_zx/moat.scene`).
- [x] **Claymorgue (C64), ENTER MOAT.** Not a divergence either: the crack in
      `claymorgue.t64` (SORCCLAY.T64) has sixteen broken pictures, 12–27
      except 16 (the original fills the screen with garbage; the interpreter
      patches them out), and so has the "[ABC]" T64. Both were saved without
      `$a000`–`$e7ff`, where those pictures live: the data is not in the file
      in any form, so nothing more can be salvaged (only the first 389 of
      picture 12's 1016 bytes are left). The plain tape
      (`claymorgue_whole.t64`), SORCCLAYALT.T64 and both disk copies (c64.com's
      and "cl.morgue castle") are whole, and ENTER MOAT draws the moat on the
      original. All 44 pictures of the tape match the original
      (`claymorgue_c64/pictures.scene`, with `!hwpoke` of the picture table at
      `$7b18` and the harness's `!picture`; `moat.scene`).
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
- `atari_8bit_vector_draw.c:554` `RGBpalette[16]` is the **Atari** GTIA palette,
  a different chip (NTSC). Out of scope for C64 accuracy; track separately if
  Atari fidelity is also wanted.

#### 6. Verify
- `make c64test && make c64a8test && make groundtruth` — still byte-exact.
- Eyeball a known room (e.g. Pirate room 0) in Spatterlight against a VICE
  screenshot of the same room; greys and the red/blue ramp should now match.

### References
- Colodore: https://www.colodore.com (generator + .vpl)
- VICE palettes ship as `colodore.vpl`, `pepto-pal.vpl`, `pepto-ntsc.vpl`
- Related: `saga-c64-groundtruth` work in the project memory notes

## Possible follow-ups
- [ ] More C64-tiny goldens (Voodoo Castle; other Pirate rooms — object-free
      rooms need no `.spec` overlays).
- [x] **C64/A8 object-overlay golden — investigated, NOT 100%-reachable.**
      Spider-Man's west room (background R001 + objects B015 gem-pedestal +
      B016 orb-altar) composites to **99.80%** (`!cliptop 2`, dx=56 dy=33) and
      that is the ceiling — _not_ the orb. The 90 stuck px are all identical:
      the renderer plots them blue (`#5f48e9`, R001 background slot 2 = palette
      value 135) where VICE shows purple (`#aa40f5` → nearest renderer colour
      `#b159b9`). They are the ceiling perspective lines along the top edge, all
      from the R001 background (B015/B016 never touch them — verified), so it is
      not a missing/mis-placed object: brute-forcing a third blob only ever made
      it worse (best +B071 → 99.03%).
      - Root cause: real-C64 multicolor takes the `%10` colour from screen-RAM
        lower nibble **per 8×8 cell**, so the original art can make these ceiling
        cells purple while other `%10` cells stay blue. The SagaPlus image format
        carries only **one global 4-colour palette** per blob (4 bytes, and the
        renderer in fact only ever plots slots 0–3), so every `%10` pixel in an
        image is the same colour. Value 135→blue is _validated_ by the corridor
        (R002, slot 2 = 135, 4804 blue px, 100% byte-exact), so it can't be
        remapped to purple without breaking the corridor, and there is no
        per-image / per-cell hook. → inherent flat-palette fidelity gap vs. a
        real-hardware screenshot; the object-free corridor stays our C64/A8
        golden. (Artifacts: `/tmp/spider/room_west.{png,spec}`, blobs in
        `/tmp/spider/`; mismatch map `/tmp/spider/mismatch_zoom.png`.)
- [x] Fold the per-format targets into one `make` aggregate — `make all-tests`
      runs test + dostest + apple2test + titest + c64test + c64a8test + zxtest +
      scenetest.
