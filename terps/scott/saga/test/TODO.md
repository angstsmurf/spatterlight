# Scott image-format byte-exact tests — TODO

Goal: a byte-exact regression test for every image-format renderer the Scott
interpreter ships. See `scott-image-format-tests` memory note for full context.

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
      runs test + dostest + apple2test + titest + c64test + c64a8test.
