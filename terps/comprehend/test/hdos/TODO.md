# Making the DOS Talisman CGA renderer 100% pixel-perfect

Goal: `hdos_talisman.cpp` renders every DOS picture **byte-for-byte identical**
to `NOVEL1.EXE` over the 280×160 window, i.e. `0 diffs` for every fixture.

**Status (2026-06-11): ACHIEVED — all four fixtures (title / throne / cell /
courtyard) are 0 / 44800, and `test_hdos_pics` ceilings are locked at 0.**

The last three bugs, found by diffing per-fill DOSBox VRAM traces against the
renderer (scripts below):

- **op14 PAINT** was rebuilt as a mechanical port of `PicOp14Paint` (0x2630):
  the 32-entry circular span FIFO (head/tail 0xb0d1/0xb0d2, six parallel
  arrays at 0xb0d3..0xb173), opposite-direction overlap merge, ±2-pixel
  overhang turn-around pushes, and the word-at-a-time masked paint
  `screen = (pat & M) | (screen ^ M)` where M is the white run found per word
  (FUN_29e1/2a07 *return* that mask in AX — the early port missed this and
  bled across boundaries).  Fillable == white (3); ANY non-white pixel is a
  boundary; the fill re-reads the screen as it goes, so painted dither-black
  self-limits later spans (this is what makes the surviving white "shadow
  pockets" in the title reproducible).  Bounds, set by FUN_2556 before every
  picture: rows 0..0x9f, global bytes 5..0x4a; rows outside read as black
  (verified: the y=-1 clamp row maps to VRAM slack at 0x1f40, all zeros).
- **op11 CIRCLE** is *not* the Apple variant: `PicDrawCircleBresenham` (0x2b10)
  starts the error term at -r, shrinks the radius as the arc closes, and tests
  `d & 0x80` (bit 7, not the sign) — its diagonal steps are 1 px wider.
- **op12 BRUSH** stamps at **x-1**: `PicStampBrushShape` (0x2967) begins with
  `DEC AX`.  Text glyphs (op3/op5, via 0x292b) do not.

## Remaining (optional polish)

- [x] §4 Palette: **DONE (2026-06-11)** — matched the real game.  The DOSBox
      goldens are unambiguously CGA palette 1 **low intensity**
      (`00aaaa/aa00aa/aaaaaa`), so `kHdosColor[]` now uses those values (was
      high intensity `55ffff/ff55ff/ffffff`).  Index-space tests are unaffected;
      the RGB→index maps in `test_hdos_pics.cpp` / `diff_hdos.cpp` / the two
      `*.py` helpers accept both intensities (or were updated to the new one).
- [x] §5 Widen coverage: **DONE (2026-06-11)** — all 94 valid `RA`–`RG` room
      pictures are captured from DOSBox and added as committed fixtures; **93 are
      pixel-exact (ceiling 0)**, the 94th (`RG_07`) is within 98 px (a localised
      magenta-vs-cyan fill band, rows 105–115 — see below).  Captured by driving
      the *native* picture interpreter directly rather than playing the game:
      `dosbox_capture_pics.py` boots NOVEL1 under the GDB stub, finds the code
      base by the op14 signature, and at the input-wait loop patches a 5-byte
      "marching" stub (`MOV AL,G; CALL 0x1cc5`) over the current IP to draw an
      arbitrary picture, then de-interleaves CGA VRAM.  `gen_room_fixtures.py`
      slices the streams + packs the 280×160 goldens (2 bpp) into
      `rooms_streams.bin` / `rooms_goldens.bin` / `rooms.tsv`; `test_hdos_pics`
      iterates them.  Two findings worth keeping:
      - **Picture addressing** (NOVEL.EXE `1cc5`→`1d25`→`1e10`): for 1-based AL
        `G`, `file_index = ((G-1)&0x7f)>>4` (→ `'A'`+idx) and `pic_index =
        (G-1)&0xf`; stream = `file[off[pic]:off[pic+1]]`.  Rooms `CALL 0x1cc5`
        ('R' prefix), objects `CALL 0x1cf5` ('O').
      - **op14 PAINT is prior-page-dependent**: full-screen rooms reproduce the
        renderer only when drawn over a **black** page (so the capture clears to
        black first).  Object/overlay pics (`OA`/`OB`/`OE`/`OF`) are sprites
        drawn over the live room and need their real in-game predecessor — they
        are *not* captured this way; do those via natural play if wanted.
      - **Stub re-execution**: a self-looping stub (JMP back) only draws once on
        this GDB stub (it won't resume over a breakpoint on the loop JMP); the
        marching stub (fresh location per picture) works.  Keep the march below
        the first picture handler (Circle @`0x2330`) — 105 stubs fit from
        `0x207F`.
- [ ] §5b `RG_07` near-miss (98 px): the renderer paints magenta where the game
      shows cyan in a rows-105–115 band (`RG`@0x32c5).  Likely an op6 fill-colour
      / subindex edge case used only here; recorded at ceiling 98 in `rooms.tsv`.
- [ ] op13 DELAY end-state: the final frame is pixel-exact on all fixtures, so
      the slow-draw path demonstrably doesn't perturb it; spot-check a scene
      with mid-picture delays if one turns up.

## Tooling

```
# render one DOS picture and diff against a golden
hdostest <NOVEL1.EXE|novel_tables.bin> <stream.img> <offset> /tmp/r.ppm [white|black]
python3 compare_fb.py /tmp/r.ppm <scene>.fb /tmp/diff.png   # red = mismatch
make -C terps/comprehend test/diff_hdos && ./test/diff_hdos  # histogram + PPM + raw dumps

# (re)capture a scene from DOSBox
python3 png_to_fb.py <scene>.png <scene>.fb
```

### Per-fill DOSBox tracing (how the last bugs were found)

`dosbox_trace_fills.py` boots the game under dosbox-x-remotedebug (GDB stub on
:2159, QMP on :4444), uses QMP `debug-break-on-exec` to halt at NOVEL1's entry,
finds the interpreter by byte signature, breakpoints `PicOp14Paint` (CS:2630)
and dumps 16 KB of CGA VRAM *before every fill* to `/tmp/nativetrace/`.
`dosbox_trace_pushes.py` additionally breakpoints the span push (CS:2da0) for
one chosen fill and logs every push.  The renderer mirrors both: set
`HDOS_TRACE_DIR=<dir>` to dump the framebuffer before each op14, and
`HDOS_TRACE_FILL=<n>` to log fill *n*'s queue pushes to stderr — then diff the
two traces to find the first divergent operation.  Mount the game folder as
floppy A: (see the conf in the scripts) to skip the master-disk prompt.

Address map for the traces: code segment is found by signature; DS = code +
0x2e70; key globals y=DS:0x9d36, byte/pixel cursor 0x9d52/0x9d53, span left
0xb0d0/0xb1d4, queue head/tail 0xb0d1/0xb0d2, file offset of a DS global =
DS_off + 0x3070.
