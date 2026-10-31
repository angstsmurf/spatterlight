# Scott picture tests

Byte-exact regression tests for the image-format renderers the Scott
interpreter ships, and what they found. The goldens live in the
`groundtruth_*/` folders, which are local and not committed; each has its own
README with its capture recipe. Open work is in `TODO.md`.

## Renderers

- Apple II vector / Atari 8-bit vector — `make test` / `make groundtruth`
- Apple II bitmap (`apple2draw.c`) — `make apple2test`
- DOS CGA bitmap (`pcdraw.c`) — `make dostest`
- TI-99/4A RTPI (`ti994a/rtpi_graphics.c`) — `make titest`
- C64 "tiny" / mini-C64 (`saga/c64_small.c`) — `make c64test`
      - keystone `build/extract_images_c64` (real unp64 + c64decrunch) unblocks
        packed C64 games; composite room rendering via `.spec`; Pirate room 0 100%
      - every picture of both games as scene tests (`.vic` goldens), see
        `groundtruth_scene/README.md`
- **C64/Atari-8 full bitmap** (`common_sagadraw/c64a8draw.c`,
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

- **ZX Spectrum Irmak/tile** (`aiukgraphics/irmak.c`) — `make zxtest`,
      golden `groundtruth_zx/gremlins/town_overrun` = **100%** (24576/24576).
      - Blocker solved: ZX graphics don't reach `USImages`, so a new keystone
        `build/zxextract` runs the real `DetectGame`→`SagaGraphicsSetup` and dumps
        the irmak globals (`tiles.bin` + per-picture `pic<NNN>.dat` + geometry).
        `zxtest.c` unity-includes irmak.c, supplies the PutPixel/RectFill sinks,
        links aiukpalette.c. Golden = real Spectrum `SCREEN$` (0x4000/0x5800) dumped
        from MAME `spectrum -snapshot <game.z80>`, decoded with the ZXOPT palette.
        Used Gremlins' full-screen death scene (no item
        overlays). See groundtruth_zx/README.md.
      - `zxtest cmp` also takes a raw `SCREEN$` (`.scr`) as golden, so a
        picture can be captured with `zx_capture.lua` and no decoding step.
        Five such goldens pin the direct-overlay rule (after a command with
        ADD_128: +128 in versions 3–4, not in 0–2) for the old versions: Hulk
        pictures 14 and 40 (v0), Claymorgue 2 (v1), Spider-Man 2 and 20 (v2) —
        the only v0–2 pictures that have the case. No script walks to them;
        `!hwpoke` lines in each `capture.scene` point the original's picture
        table at them instead.

- **ZX Spectrum Howarth/vector** (`ai_uk/line_drawing.c`,
      `DrawHowarthVectorPicture`) — harness built (`zxvectortest`) + `zxextract`
      dumps the `LineImages[]` vector streams. The rasterizer is **ROM-exact**
      (RE'd from the Golden Baton `.z80` in Ghidra): line geometry is **100%**
      (1740/1740 drawn px on Golden Baton's opening room, was 38%) and end-to-end
      colour match is **95.26%** (was 94.11%).
      - The residual ~5% is **inherent**: the Spectrum's 8×8 attribute clash makes
        a white outline crossing a red-filled cell display as red. Reproducing it
        would make Spatterlight's output *worse* for users, so it is deliberately
        not done — hence no exact-colour `.zxvec` golden is committed. If a
        regression guard is wanted, assert set-bit *geometry* (lines == 100%).
      - See `ai_uk/NOTES_howarth_vector.md` for the ROM anchors and method.

- **Atari 8-bit bitmap** (`c64a8draw.c` with `C64A8_ATARI8`) — `make c64a8test`,
      **four** Atari goldens, all 100% byte-exact:
      `hulk_atari8/room01_chair`, `count_atari8/room01_bed`,
      `voodoo_atari8/room01_coffin`, `claymorgue_atari8/room01_castle`.
      - Unblocked: the companion-disk pairs live in `~/Desktop/Interactive
        Fiction experiments/Saga and TaylorMade stuff/Atari 8-bit saga/`. Dump
        blobs with `build/extract_images <Side A/Disk 1 .atr> <outdir>` (real
        `DetectAtari8`; the `(Side B)`/`(Disk 2)` sibling is auto-found).
      - Coverage: this exercises the `C64A8_ATARI8` `TranslateColorAtari8` +
        `ataricolors[256]` path (so c64a8 is covered on both C64 and Atari), the
        `!voodoo` RLE variant (Count/Voodoo) **and** the plain bit-7 RLE branch
        (Hulk/Claymorgue), plus the first Atari **object-overlay** golden
        (Voodoo coffin = bg `u0_i001` + object `u1_i017`; bg alone is 94.98%).
      - Goldens captured live in **MAME `a400 -ramsize 48K`** (a800 hangs on the
        SAGA loader); see groundtruth_c64a8/README.md for the keyboard/disk-swap
        recipe. Hulk's golden reused the
        ready-made `hulk03.png`.
      - Avoid Hulk room 0 (`a8croplist` cropright=32) and index 19 (atari8
        width++ special case) — the harness `adjust_room` mirror models neither;
        other rooms use the plain `width-1` room adjustment and stay exact.

## Scene tests (the picture paths above the renderers)

- **`make scenetest`** — `scenetest.c` runs the real `glk_main()` on a
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
- **Every picture of Seas of Blood**, `seas_pictures_zx/` (124 scenes) and
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

## Settled differences from the originals

- **Seas of Blood, the stone hall (C64), two cells.** Deliberate: the
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
- **Seas of Blood, the crypt (ZX).** Deliberate: `PatchCryptImage()`
      takes the BRIGHT off four cells beside the sarcophagus. 141 pixels
      (`seas_pictures_zx/room13`, `item051`, `min=24435`); the C64 picture is
      exact.
- **Gremlins, kitchen after the blender has run.** The original only
      redraws a picture when something in the room changes, so the blender
      keeps the frame it stopped on until then (the chute opening brings back
      the first). Ported (`GremlinsKeepBlenderFrame` in `ai_uk/gremlins.c`);
      `gremlins_c64` is exact now and `gremlins_zx/blender.scene` pins the
      second frame on the Spectrum.
- **Claymorgue (ZX), ENTER MOAT.** Not a divergence: the `.z80` the scene
      used is a damaged copy (pictures 9–35 broken, crashes to BASIC; the
      interpreter patches them out). Whole copies exist — `claymorgue.sna` and
      both `.tzx` tapes — and all 37 pictures are byte-exact against the
      original (`groundtruth_zx/claymorgue_N`, `claymorgue_zx/moat.scene`).
- **Claymorgue (C64), ENTER MOAT.** Not a divergence either: the crack in
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

## Other coverage

- More C64-tiny goldens, as scene tests instead of `.spec`s:
      `groundtruth_scene/pirate_c64`, `voodoo_c64` (walked) and
      `pirate_pictures_c64`, `voodoo_pictures_c64` (every picture, written by
      `aio_scenes.py`), `.vic` goldens captured by `c64_capture.py`. The
      full set found one renderer bug: room pictures are centred by the
      original (`left = (40 - width) / 2`), which `c64_small.c` now does
      in place of its "left < 10 → column 11" guess.
- Every picture of the Spectrum Gremlins (English, German, Spanish), Super
      Gran and Savage Island I and II, gang animations included (309 scenes,
      `<game>_pictures_zx`, written by `uk_scenes.py`). Samples of those games
      on the C64, with animations (`c64_capture.py` got `!hwnext`), and of the
      US Hulk on the Atari 8-bit and MS-DOS, drawn through
      `PutPixelWithWidth()` (`hulkus_atari8`, `hulkus_dos`). Three Savage
      pictures differ; see `TODO.md`.
- The C64 colours are VICE's `colodore.vpl` in all three renderers:
      `c64colors` in `common_sagadraw/c64a8draw.c` (also used by `c64_small.c`)
      and `c64_rgb` in `aiukgraphics/aiukpalette.c`. `c64test` keeps a copy,
      since it can't link `c64a8draw.c`.
- **C64/A8 object-overlay golden — investigated, NOT 100%-reachable.**
      Spider-Man's west room (background R001 + objects B015 gem-pedestal +
      B016 orb-altar) composites to **99.80%** (`!cliptop 2`, dx=56 dy=33) and
      that is the ceiling — _not_ the orb. The 90 stuck px are all identical:
      the renderer plots them blue (R001 background slot 2 = palette
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
        golden.
- `make all-tests` runs test + dostest + apple2test + titest + c64test + c64a8test + zxtest +
      scenetest.
