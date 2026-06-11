# Making the DOS Talisman CGA renderer 100% pixel-perfect

Goal: `hdos_talisman.cpp` renders every DOS picture **byte-for-byte identical**
to `NOVEL1.EXE` over the 280×160 window, i.e. `0 diffs` for every fixture.

Current state (2026-06-11): **1.6 %–4.7 % mismatch**, down from 67–80 %.  Sections
0–3 below are essentially done (see README "Status" for the four fixes); the only
material work left is the fill-edge long tail (§2 boundary test).

## 0. Lock down the harness first — DONE
- [x] `test_hdos_pics.cpp` renders each scene from its committed `.img` stream and
      diffs the 280×160 window at (20,0) against `<scene>.fb`; ceilings catch
      regressions.
- [x] Wired into `make -C terps/comprehend test`.
- [x] Pinned scenes: `title`=`T0`@4, `throne`=`RA`@0x22, `courtyard`=`RA`@0x8c1,
      `cell`=`RA`@0x2128 (offsets brute-forced against the goldens).
- [x] **x-alignment**: window origin is (20,0) (content spans cols 20–299).  The
      apparent ±1 shift was the mirrored fill bytes (§2), not a window offset.

## 1. Background / screen-clear fill — DONE
- [x] Root-caused: `RESET3` fills with the current fill pattern (`FUN_1ec0`),
      and the cold-start selector is 0 → solid white (`[0x9d46]` statically 0).
      The renderer had defaulted the selector to 3 (a dither); fixed to 0.

## 2. Fill patterns & flood-fill (dither phase/density)
- [x] Fill tables confirmed (fill @0xc8d0, subindex @0xca5c) and op6→subindex→
      pattern mapping validated against `FUN_23dd`.
- [x] Row-parity matches (`[0xb031]`=0, renderer uses `y&1`).
- [x] 2-bpp **bit order**: interpreter is MSB-first; the renderer is LSB-first, so
      fill-table bytes are now reversed at load (this was the dominant ~30 % bug).
- [ ] **Flood-fill boundary test** — the remaining long tail.  Native predicate is
      "pixel != 0 (black)" (`FUN_29a0` masks the screen word per pixel), but the
      renderer's simplified queue relies on "== 3 (white)" to avoid double-fill;
      switching to `!=0` over-fills.  A faithful port of `PicOp14Paint` @0x2630
      (32-entry circular queue + merge) is needed to reach 0 diffs at fill edges.

## 3. Per-primitive opcode audit vs Ghidra
- [x] Line / box — match (throne is pure lines+fills at 3.1 %).
- [x] Brush plot: quadrant order was L/R-swapped; corrected to bytes 0-7=upper-
      left, 8-15=lower-left, 16-23=upper-right, 24-31=lower-right
      (`PicStampBrushShape` @0x2967).
- [x] Text glyphs: op3 XORs the glyph (`pixel ^= 0b11`); op5 fills with the
      current pattern (`PicBlitBrushColumn2bpp` @0x2a30).  8-px advance.
- [ ] op13 DELAY / slow-draw end-state — spot-check it doesn't perturb the final
      frame (the offline test renders the final frame only).

## 4. Palette / display colour (cosmetic, not index-level)
- [ ] Decide high- vs low-intensity for actual display: the real game is CGA
      **palette 1 low-intensity** (`00aaaa/aa00aa/aaaaaa`); `kHdosColor[]` uses
      high-intensity. Match the game (or confirm Spatterlight intentionally
      brightens). Index-space tests are unaffected either way.

## 5. Widen coverage, then certify
- [ ] Capture more ground truth: item pics (`OA`,`OB`,`OE`,`OF`), remaining rooms
      (`RA`–`RG` across both disks), and any special screens — same
      `png_to_fb.py` flow.
- [ ] Iterate 1–3 until **every** fixture is `0 diffs`.
- [ ] Once green, treat the `.fb` set as the locked golden; any renderer change
      must keep `make test` at 0 diffs.

## Handy commands
```
# render one DOS picture and diff against a golden
hdostest <NOVEL1.EXE> <RA|OA|T0> <offset> /tmp/r.ppm [white|black]
python3 compare_fb.py /tmp/r.ppm <scene>.fb /tmp/diff.png   # red = mismatch

# (re)capture a scene from DOSBox
python3 png_to_fb.py <scene>.png <scene>.fb
```
