# PCjr 16-colour renderer harness (graphics_magician_pcjr.cpp)

`pcjrtest` renders one Graphics Magician picture through the IBM PCjr / Tandy
16-colour renderer and writes a 280×160 PPM, for eyeballing the colours and
geometry. Self-contained (no Glk/disk deps).

```
make test/gmpcjr/pcjrtest
./test/gmpcjr/pcjrtest <gamedir> RA.MS1 0  out.ppm   # room file, image index
./test/gmpcjr/pcjrtest <gamedir> RA.MS1 1  out.ppm
./test/gmpcjr/pcjrtest <gamedir> TRTITLE.MS1 -1 title.ppm   # -1 = single-image title file
```

`<gamedir>` is the IBM PC/PCjr release directory containing `JR_GRAPH.OVR`,
`NOVEL1.EXE` (or `NOVEL.EXE`) and `CHARSET.GDA` (e.g. the Transylvania v1 or
Crimson Crown folders).

## Pixel model (reverse-engineered from JR_GRAPH.OVR + NOVEL.EXE in Ghidra)

The original NOVEL.EXE checks the BIOS model byte (F000:FFFE == 0xFD) and, on a
PCjr, loads `JR_GRAPH.OVR` instead of `PC_GRAPH.OVR` and drives BIOS video
mode 9 (320×200, 16 colours). Key facts:

* **Mode 9, 4 bits/pixel, 2 pixels/byte**; the leftmost pixel of a byte is its
  high nibble. VRAM offset = `(y&3)*0x2000 + 160*(y>>2) + ((x+20)>>1)`
  (4-bank interleave; `jr_calc_row_base` @ OVR 0x4d, `jr_calc_column_nibble`
  @ 0x85). Picture window is +20 px, so the logical picture is the same 280×160
  as the CGA/Apple renderers.
* **Pen plot** (`jr_plot_pixel` @ 0x0e): pen index `p = pen & 7` selects a colour
  nibble from `colLow[]`/`colHigh[]` (OVR 0xafe/0xb06) OR'd into the target
  nibble. White (entries 3 and 7) carries `0xff` in `colHigh`, so it whitens the
  adjacent low nibble too — a faithful one-pixel bleed.
* **Fills**: 30 × 4-byte 16-colour dither tiles (OVR 0xb29) selected per op6
  byte through a 108-entry subindex (OVR 0xba1; one pattern index per row
  parity), exactly like the v1 CGA path.
* **Brushes** come from NOVEL.EXE (same brush-5 signature the CGA loader uses);
  the in-picture **font** comes from `CHARSET.GDA`. Both are mode-independent.
* **Palette**: the standard PCjr/CGA 16-colour RGBI palette (no custom palette
  is programmed).

The tables are located by signature in `gmpcjrInstallDrawingTables()` (the
`00 20 0f f0` = bankStride/masks anchor), so they resolve in both the
Transylvania v1 and Crimson Crown overlays.

## In the game

The renderer is opt-in via the `#pcjr [on|off]` debug command (mirroring
`#dhgr`), available whenever `JR_GRAPH.OVR` loaded. `#pcjr on` routes pictures
through the 16-colour renderer; `#pcjr off` restores the 4-colour CGA renderer.

## Ground truth note

Live PCjr VRAM capture under DOSBox-X was not feasible: `machine=pcjr` reports
the wrong BIOS model byte (0xFC, not 0xFD) so the game falls back to CGA, and
forcing the jr_graph branch (patching the `cmp al,0xfd`) loads the overlay but
the emulator's PCjr video does not engage the game's mode-9 renderer (the title
stays in text mode). The drawing tables here were therefore extracted
*statically* from the overlay (deterministic) rather than dumped from a live
DGROUP, and validated by comparing rendered geometry against the (validated)
CGA renderer for the same pictures.
