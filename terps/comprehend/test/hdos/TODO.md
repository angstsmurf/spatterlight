# Making the DOS Talisman CGA renderer 100% pixel-perfect

Goal: `hdos_talisman.cpp` renders every DOS picture **byte-for-byte identical**
to `NOVEL1.EXE` over the 280×160 window, i.e. `0 diffs` for every fixture.

Current state (2026-06-11): **throne 0, cell 73, title 371, courtyard 1017**
(0 % / 0.16 % / 0.83 % / 2.27 %), down from 1285 / 1385 / 705 / 2113.  The big
remaining wins came from two primitive ports:

- **draw_line**: was the symmetric two-axis Bresenham; replaced with the native
  major-axis form (PicDrawLineBresenham 0x2c00, error init = minor - major).
  The two variants tie-break opposite ways on ~45deg lines, shifting a line --
  and the fill it bounds -- by 1 px.  This alone took throne to 0 and roughly
  halved the others.
- **hdos_flood_fill**: was white-only (== 3); replaced with a scanline fill
  over a visited bitmap, boundary == black (!= 0), matching FUN_29a0.

Diagnostics confirmed (via test/diff_hdos, which prints a rendered->golden
transition histogram, per-row bands, a spatial diff PPM, and a raw index dump;
plus a one-off per-pixel last-writer-opcode trace built by temporarily tagging
write_2bpp/fill_hline with the current opcode):
- The residual is **op12 BRUSH** (cell 36 / title 269 / courtyard 622) then
  **op14 PAINT** (36 / 89 / 347), with a few LINE/CIRCLE.
- It is **not** a dither phase/parity bug: throne uses 10 dither selectors and
  is pixel-exact, and the even->low / odd->high subindex mapping matches
  FUN_23dd + FUN_24a4 exactly.  b02d (x-offset) = 20 = the window origin and is
  a multiple of 4, so it adds no sub-byte phase.  Brush params verified:
  stride 9d80 = 2 (8 px), parity b031 = 0, y-transform 9d3b = 0.
- The title is a ~988-stamp pixel spray of brush 0 (one set bit, byte 23 ->
  one pixel at (+8,+7)); sweeping that offset doesn't change the diff count, so
  the surviving-brush positions are right -- the residual is at the **edges**
  of dithered fills and in dense foliage stipple (courtyard), where which exact
  pixel ends up in a region depends on the native fill's span-queue traversal
  *order* (it reads the screen mid-fill, so painted dither-black pixels can
  self-limit later spans).

Reaching 0 on title/courtyard needs a faithful port of PicOp14Paint's 32-entry
circular-queue traversal (decompiled; see project memory), which is high-risk
for the ~1-2 % gain.  Deferred.  No scene uses op15 param 1/2, so flood-fill
clip-rect handling is moot for the current fixtures.

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
- [x] **Flood-fill boundary test** — DONE for the predicate: now "pixel != 0
      (black)" (`FUN_29a0`) over a visited bitmap (the visited map plays the role
      of the native done-flags, so `!=0` no longer over-fills).  Reproducing the
      native span-queue *traversal order* (which self-limits via mid-fill screen
      reads) to nail the last edge pixels in complex regions is the remaining
      long tail; deferred (see top of file).

## 3. Per-primitive opcode audit vs Ghidra
- [x] Line / box — now the native major-axis Bresenham; throne (pure lines+fills,
      incl. 10 dither fills) is pixel-exact.
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
