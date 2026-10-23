# TODO: consolidate duplicated patterns in the Graphics Magician renderers

Scope:

* **Part A**: `graphics_magician.cpp` (Apple II hi-res) and `graphics_magician_dhgr.cpp` (Apple II double hi-res)
* **Part B**: `graphics_magician_cga.cpp` and `graphics_magician_pcjr.cpp` (DOS)

Some consolidation is already done and is not repeated here. Both Apple files use `SlowDrawEngine` (`slow_draw_page.h`), `GmSpanFill` (`gm_span_fill.h`), `gm_artifact.h`, `gm_hires.h`, the `Opcode` enum and `cm_panel.h` (`CMGrainFall`, `cmForEachPileGrain`). CGA and PCjr share `gmvDrawLine`, `gmvDrawCircle` and `gmvMirrorByte` (`graphics_magician_vector.h`) and `SlowDrawPage`.

Line numbers below are for the tree at commit `c26123b`.

**Done on branch `gm-dispatcher`:** A6/B3 (the shared dispatcher `gm_stream.h`, see `TODO-graphics-magician-dispatcher.md`) A1 (`gm_row_of_offset`/`gm_col_of_offset` in `gm_hires.h`, replacing the two tables), A2 (`GmFillBounds`), A3 (`gmDecodeRleBitmap`), A4's panel store (`CMPanelStore<Planes>` in `cm_panel.h`: capture, rebuild, overlay and reveal-skip for one or two planes; the hourglass arm state and grain-position function are still defined in `graphics_magician.cpp`, so the DHGR file still links against it) B2 (`gmvStampBrush`, `gmvDrawGlyph`) and B1 (`gmvFindBrushTable`, `gmvLoadCharsetGda` in `graphics_magician_vector.h`; the generic `gmvFindSignature` was left out because the other scans each carry their own bounds logic). A7's sampling half is done too (`gmSampleRow560` in `gm_blit.h`).

**Not done, and why:**
- **A5 (slow-draw policy template):** after A1 and A4 each policy is a row lookup, a one-line adjacency test and a call to `CMPanelStore::skipsReveal`; a template over the plane count would be longer than the two structs it replaces.
- **A7 reset:** the two reset functions differ in background byte and plane count, three lines each.
- **B4 (flat-page shell for CGA/PCjr):** about ten lines per file; the pixel extraction and palette in `blit_page` differ, so only the memsets and forwarders would be shared.
- **Part C:** `gm_backend.h` already is the table of entry points the host calls through. The per-prefix functions remain because the stand-alone tests call them directly.
- **Hourglass arm state:** still defined in `graphics_magician.cpp`, so `graphics_magician_dhgr.cpp` still links against it. Moving it into `cm_panel.h` is possible but would only change the link line.

## Ground rules for every step

- [ ] **Pixel-exact refactors only.** These renderers are byte-faithful ports of 6502 / x86 code, and the goldens are the safety net. Run the harnesses in `test/` before and after each step: `test_dhgr_pics`, `test_dhgr_determinism`, `cmdhgrtest`, `cmhourglasstest`, `pixtest`, `test_gmcga_pics`, `test_gmcgav1_pics`, `test_gmpcjr_pics`, `diff_gmcga`.
- [ ] **Prefer header-only shared code.** The test harnesses link a single renderer `.cpp` and nothing else (see the note at the top of `slow_draw_page.h`). Adding a new `.cpp` means touching every harness build line and `Spatterlight.xcodeproj/project.pbxproj`.
- [ ] **One step per commit.** Where two copies look the same but differ subtly (listed under "Differences to preserve"), keep the difference as an explicit parameter instead of silently picking one.
- [ ] **Keep the public API unchanged until Part C.** Hosts (`gm_backend.cpp`, `pics.cpp`, `comprehend_gfx.cpp`, `game.cpp`) call the `gm*` / `gmDhgr*` / `gmcga*` / `gmpcjr*` names directly.

---

## Part A: Apple hi-res vs double hi-res

### A1. Offset to row/column tables (trivial, do first)

`graphics_magician.cpp:157-190` and `graphics_magician_dhgr.cpp:69-96` are the same code: `s_rowOfOffset`, `s_colOfOffset`, `s_rowTableInit`, `init_offset_tables()` and a row lookup (`row_of_offset` / `dhgr_row_of`). Only the buffer size constant and the `gm_row_address` / `CALC` spelling differ.

- [ ] Move the tables and `gmRowOfOffset(off)` / `gmColOfOffset(off)` next to `gm_row_address` in `gm_hires.h` (or a small header-only `gm_hires_offsets.h`).
- [ ] Delete both local copies. `apple_apply_slow` uses `col_of_offset`, and `dhgr_apply_slow` pokes `s_colOfOffset` directly. Both become `gmColOfOffset`.
- [ ] Drop the `#define CALC(y) gm_row_address(y)` alias in the DHGR file.

### A2. Fill-bounds rectangle and op15 sub-ops 1 and 2

The same state and parsing appear four times:

- Apple: `a2_ctx.fill_*` (`graphics_magician.cpp:249`) and `578-587`
- DHGR: `g_fillLeft/Right/Top/Bottom` (`graphics_magician_dhgr.cpp:419`) and `822-833`
- CGA v2: `s_fill_*` (`graphics_magician_cga.cpp:1192`) and `1390-1399`
- CGA v1 (`s_pf*`) and PCjr (`*ptr += 4`) consume the same four bytes in the stream order right, left, bottom, top.

- [ ] Add `struct GmFillBounds { uint8_t left, right, top, bottom; void reset(uint8_t bottomRow); bool read(const uint8_t *&p, const uint8_t *end); }` to a shared header.
- [ ] Use it from Apple, DHGR and CGA. Make the PCjr stub call `read()` rather than `+= 4`, so the stream-length rule lives in one place.
- [ ] Differences to preserve: the reset bottom row (`0x9f` Talisman, `0xbf` legacy Apple, `159` CGA), and DHGR's doubling of columns when it hands the bounds to the span fill (`2*left`, `2*right+1`).

### A3. RLE bitmap rectangle (op15/0)

`graphics_magician.cpp:539-577` and `rle_bitmap_dhgr` (`graphics_magician_dhgr.cpp:492-535`) have an identical parser: 4 bounds bytes, a column-major walk, the `0x80` escape followed by count and value, and termination when the bottom-right cell is written, even mid-run. They differ only in the cell write.

- [ ] Add `template <class WriteCell> const uint8_t *gmDecodeRleBitmap(const uint8_t *p, const uint8_t *end, GmFillBounds &b, WriteCell w)`, calling `w(row, col, byte)`.
- [ ] Apple's `w` is `write_screen(gm_row_address(row)+col, b)`. DHGR's `w` splits the byte through `DHGR_DOUBLE_BITS` into aux and main.
- [ ] Differences to preserve: Apple has no `p + 4 > end` guard, whereas DHGR does. The row and column limits are `APPLE2_SCREEN_HEIGHT` / `APPLE2_SCREEN_COLS` on Apple but `0x9f` / `40` on DHGR. Decide whether the guard and limits become parameters or are unified, and check against `cmdhgrtest`.

### A4. Coveted Mirror panel (largest Apple-side duplication, about 200 lines)

Standard (`graphics_magician.cpp:653-886`) and DHGR (`graphics_magician_dhgr.cpp:537-688`) are the same state machine over one plane vs two planes:

| Piece | Apple | DHGR |
|---|---|---|
| Capture and valid flag | `gmCaptureCMPanel`, `s_cmPanel*` | `gmDhgrCaptureCMPanel`, `s_cmPanelAux/Main` |
| Begin/End rebuild (save pages and slow pages, suppress recording, clear, capture 24..39, restore) | `690-703` | `566-584` |
| Copy panel columns to final and slow pages | `cm_panel_to_pages` | `cm_panel_to_pages` |
| Skip panel columns in the reveal | `apple_apply_slow` | `dhgr_apply_slow` |
| Band overlay for the fall animation | `cmOverlayPanelBand` | `cmDhgrOverlayPanelBand` |
| Fall Begin/Active/Step/Abort wrappers | `867-886` | `670-688` |
| Hourglass pile stamp | `gm_draw_brush` loop | `set_fill_color(0x34)` + `draw_brush` loop |

- [ ] Introduce `template <int Planes> struct CMPanel` (header-only, probably `cm_panel.h`) that owns the saved panel planes, `valid`, `col0/col1`, the rebuild save buffers and the recording flag. It is driven through plane pointers: `capture(const uint8_t* const live[Planes])`, `begin()`, `end()`, `toPages(y0,y1)`, `finalToSlow(y0,y1)` and `isPanelOffset(off)`.
- [ ] Hoist the hourglass bookkeeping out of `graphics_magician.cpp`. `s_cmDisplayedSand`, `s_cmFallArmed`, `gmCMHourglassArm`, `Reset` and `ConsumeFallArmed` are renderer-independent, but they live in the standard-hires file. As a result `graphics_magician_dhgr.cpp` includes `graphics_magician.h` and cannot link without it. Move them next to `CMGrainFall` in `cm_panel.h` (inline or a single `CMHourglassState`).
- [ ] `gmCMHourglassGrainPos` (`graphics_magician.cpp:749`) is also renderer-independent and belongs with `cmForEachPileGrain`.
- [ ] Both fall wrappers become one-liners over the shared `CMGrainFall`, with a renderer-supplied `stampGrain` / `restoreBand`.
- [ ] Differences to preserve: DHGR sets `x << 1`, and it mirrors the final pages to the slow pages after stamping (`cm_panel_final_to_slow`), while the standard renderer does `memcpy(s_slowScreen, s_screenmem)`. DHGR's `gmDhgrDrawCMHourglass` also returns early if the panel is not valid.

### A5. Slow-draw policy

`AppleSlowPolicy` and `DhgrSlowPolicy` share `rowOf`, the offset-adjacency test and a panel-skipping `apply`.

- [ ] After A1 and A4, make one `template <int Planes> struct HiresSlowPolicy` in `slow_draw_page.h` or the CM header. It has plane pointers for the live pages, the slow pages and the panel, and an `adjacent()` that also compares `page` when `Planes == 2`.
- [ ] Replace the six-line `*SetSlowDraw/Active/Advance/Finish/ConsumeDirty/BlitSlow` forwarders. They are identical in all four renderers (see Part C).

### A6. Opcode stream decode and dispatch

Both files repeat `*p++ + (param & 1 ? 256 : 0)`, followed by `*p++`, in about eight opcodes. The `DRAW_BOX` four-line sequence is also duplicated (Apple `481-497`, DHGR `778-790`), as are the `guard < 100000` loop and the "sync slow page on exit" step.

- [ ] Add a shared `gmReadXY(const uint8_t *&p, uint8_t param, uint16_t &x, uint8_t &y)`. Note the CGA and PCjr dispatchers use an `a`/`b` pair in the same way. See B3, which should use the same helper.
- [ ] Add `gmDrawBox(x0, y0, a, b, lineFn)`, calling the chained `line(x0,b)`, `line(a,b)`, `line(a,y0)`, `line(x0,y0)` in order. All four renderers use this, with the pen left at `(a,b)`.
- [ ] Longer term, a single `template <class R> void gmRunStream(R &r, const uint8_t *data, size_t size)`. It owns the loop, the guard, the op/param split, operand decode and the `RESET` sub-op switch. The renderer supplies `onSetPen/Shape/Fill`, `moveTo/line/circle/brush/paint/text`, `rle/fillRect` and the dialect flags.
  - Doing this for Apple, DHGR, CGA and PCjr together removes about 100 lines per file.
  - Apple ends via `done` and DHGR via `return`. The scope-guard `SyncOnExit` in DHGR becomes the one place that calls `syncIfNotRecording`.
  - Differences to preserve: Apple `OPCODE_OP7` is a no-op or an end marker depending on `s_legacyFormat`. DHGR treats op7 as `SET_PALETTE_SUBINDEX`, CGA and PCjr as a 2-byte skip. `RESET` also differs: Apple legacy ends, CGA v1 sets the clip, DHGR dispatches into the shared rect/RLE code.

### A7. Output conversion and reset

- [ ] `screenmem_to_rgba` (Apple `607-622`) and `blit_pages` (DHGR `843-858`) share the "per scanline, expand to 560 colours, then sample and palette-lookup to `w`" step. Share `gmSample560(colors560, out+row*w, w)`.
  - DHGR currently short-circuits `sx = xx` when `w >= 560`, while Apple always uses `(x*560)/w`. These are **not** the same for `w > 560`. Keep the difference as a flag unless you verify the host only ever passes `w <= 560`.
- [ ] `gmResetScreen` and `gmDhgrResetScreen` differ only in the background byte (`0xff` vs `0x7f`) and the plane count. They become a method on the shared page struct from A4/A5.

### A8. Things that look duplicated but should NOT be merged

- **Circle** (`draw_circle`, Apple `350-386` vs DHGR `335-355`). These are two different 6502 routines (`$0b92` byte-carry midpoint vs `FUN_0aeb`/`FUN_0bcb` half-step double-resolution), and the DHGR header comment says the Talisman lamp-knob fill depends on it. At most share the eight-point plot helper.
- **Line**: Apple uses the shared `gm_draw_line` from `common_sagadraw`, while DHGR uses a run-slice port with 4-pixel colour pens.
- **Glyph and brush blit**: the pixel spreading is 7-bit hi-res vs nibble-doubled DHGR. The Apple `glyph_blit_half` does not match the DHGR one, so only the loop shell (8 rows) is common. Do not share.
- **Text advance**: 7 px (Apple) vs 14 px (DHGR, already doubled).

---

## Part B: CGA vs PCjr

### B1. Resource loading helpers (easy, low risk)

- [ ] **Brush-table signature search.** The `kBrush5Sig` array and the "find brush 5, step back `5*32`, bounds-check, memcpy 256" block are copied verbatim in `gmcgaInstallV1DrawingTables` (`graphics_magician_cga.cpp:466-483`) and `gmpcjrInstallDrawingTables` (`graphics_magician_pcjr.cpp:94-110`). Extract `gmvFindBrushTable(const uint8_t *exe, size_t size, uint8_t out[256])` into `graphics_magician_vector.h`. Add a generic `gmvFindSignature(buf, size, sig, sigLen)` as well, since five more linear `memcmp` scans exist (the fill signature, `kPatSig` and PCjr `kSig`).
- [ ] **CHARSET.GDA loader.** `gmcgaSetV1Font` (`518-530`) and `gmpcjrSetFont` (`120-130`) are identical except for the destination. Extract `gmvLoadCharsetGda(uint8_t dst[96*8], const uint8_t *gda, size_t size)`, which includes the `0x1100` version check, the `4 + 96*8` length check and `gmvMirrorByte`.
- [ ] Comments in `graphics_magician_vector.h` should say that these are shared by *loading* too, not only by geometry.

### B2. Brush and glyph shells

- [ ] **Brush stamp.** `draw_brush` is the same four-quadrant stamp in both (CGA `1142-1149`, PCjr `315-322`): the `brush >= 8` guard, `brush*32`, and the quadrants at `(x,y) (x,y+8) (x+8,y) (x+8,y+8)` with data offsets `0, 8, 16, 24`. Add `gmvStampBrush(const uint8_t *bitmaps, int x, int y, uint8_t brush, BlitQuadrant bq)`. The CGA `blit_col` (3-byte spill and dither phase walk) and the PCjr `blit_quadrant` (per-pixel nibble writes) stay as the callbacks.
- [ ] **Glyph.** `draw_glyph` has the same shell in both (CGA `1163-1183`, PCjr `330-344`): range check `32 <= ch < 128`, `glyph = font + (ch-32)*8`, row and bit loops MSB-first, with a per-pixel op (invert vs fill pattern). Add `gmvDrawGlyph(font, ch, x, y, perPixel)`.
  - Differences to preserve: CGA invert is `^ 0x3`, PCjr is `^ 0x0f`. CGA bounds-checks inside the loop, while PCjr relies on `write_pixel`. CGA takes an unused `pen_val` parameter that can simply be dropped.
- [ ] Compare the Apple quadrant order too. DHGR `draw_brush` places `bb+8` at the *upper-right* and `bb+16` at the *lower-left*, whereas CGA/PCjr place `+8` lower-left and `+16` upper-right. This is plausibly intentional, since the DOS data is column-major, but document it in the shared helper so nobody "fixes" it.

### B3. One DOS opcode dispatcher

`doImageOp` (CGA `1241-1428`, PCjr `362-456`) is the same function over a different pixel model. `GmcgaCtx` and `PcjrCtx` are the same struct (`pen_x`, `pen_y`, pen value, `brush`, `fill_idx`/`fill_sel`, `text_x`, `text_y`). The per-image init in `gmcgaDrawImage` (`1430-1463`) and `gmpcjrDrawImage` (`458-477`) is the same: pen `(140,6)`, brush 5, fill selector 0, text `(140,96)`, a `guard < 100000` loop and `syncIfNotRecording`.

- [ ] Introduce `struct GmDosCtx` and `template <class Px> bool gmDosDoOp(...)` (header `graphics_magician_dos.h`), where the `Px` traits class provides:
  - `mapPen(param)`: CGA `kPenTo2bpp[param]` (`param < 8`, else 0), PCjr `param & 0xf`.
  - `line(x0,y0,x1,y1,pen)` and `circle(cx,cy,r,pen)`, which are already one-line bindings to `gmvDrawLine` / `gmvDrawCircle`.
  - `brush(x,y,brush,fill)`, `glyph(x,y,ch,normal,fill)`, `paint(x,y,fill)`.
  - `reset(param, ptr)`, which holds the op15 behaviour.
- [ ] The shared dispatcher owns op7 skip-2, the text-pos decode, the `x-1` brush quirk (`PicStampBrushShape` starts with `DEC AX`, identical in both), the `DRAW_BOX` sequence (see A6), `DELAY` to `recordDelay`, and the `RESET` operand consumption.
- [ ] PCjr is effectively a strict subset of CGA, so the refactor mostly deletes PCjr's copy.
- [ ] Differences to preserve: CGA `OPCODE_RESET` has the v1 clip path and the v2 rect-fill path, PCjr only skips 4 bytes when `param >= 2`. The `GM_TRACE` / `GMCGA_*` env hooks in the CGA dispatcher stay behind `Px` or `#ifdef`, and must be kept out of the PCjr build.
- [ ] Fold the A6 `gmRunStream` work into this one if both parts are done. Parts A and B then share a single decode and dispatch template.

### B4. Framebuffer shell, reset, blit and slow wrappers

- [ ] Both have `s_screen*` + `s_slowScreen` + `SlowDrawPage s_slow(...)`, `store_byte(off,val)` (`write` and `record`), a `*ResetScreen` that memsets both pages and calls `s_slow.clear()`, a nearest-neighbour `blit_page` (differing only in pixel extraction and palette), and the same six slow-draw forwarders.
- [ ] Wrap these as `template <class Geometry> struct GmFlatPage` (page pair, `store`, `reset(white)`, `blit(pixelAt, palette)`) in `slow_draw_page.h` or the DOS header.
- [ ] Unify the constants. `GMCGA_WIDTH/HEIGHT` and `PCJR_PIC_W/H` are both 280x160. Only the stride (70 vs 160) and the x offset (0 vs 20 px) differ.
- [ ] `CGA fill_rect` / `fill_hline` and PCjr flood fill are **not** shared (see B5).

### B5. Not mergeable without behaviour risk (document, do not touch)

- **Flood fill.** CGA is a faithful port of the native word-queue fill (`pf_*`, `kPf*` masks, clip rectangle `s_pf*`), while PCjr is a generic visited-mask scanline fill (`gmpcjr_flood_fill`, "exact traversal order... not observable without hardware ground truth") that ignores the op15 clip. A shared fill would either degrade CGA or need the native PCjr word fill reverse-engineered. Leave a comment in both pointing at each other, and track a separate item "port the JR_GRAPH word fill from `0x47f`".
- **Pixel model and fill lookup.** CGA `fill_pattern` (2-bpp, v1 four-phase and v2 single-byte period-4) and PCjr `fill_pixel` (4-bpp, `wordFill` uses tile bytes 0 and 2 only) are genuinely different. CGA v2 vs v1 selection (`s_v1`) has no PCjr analogue.
- **Install functions.** The signatures and offsets differ (`kFillSig` / `kPatSig` vs `kSig` + `bankStride`), so only the helpers in B1 are shared.

---

## Part C: Public API and file-level structure (do last)

The four headers declare the same slow-draw contract (`SetSlowDraw`, `SlowDrawActive`, `AdvanceSlowDraw`, `FinishSlowDraw`, `SlowConsumeDirty`, `BlitSlowToSurface`) plus `ResetScreen`, `DrawImage` and `BlitToSurface`, under four prefixes. `gm_backend.cpp` (12 references) and `pics.cpp` (6) already dispatch among them.

- [ ] Review `gm_backend.cpp` and `pics.cpp` and check whether they already abstract this. If not, add `struct GmRenderer { resetScreen, drawImage, blitToSurface, setSlowDraw, slowDrawActive, advanceSlowDraw, finishSlowDraw, slowConsumeDirty, blitSlowToSurface }` plus a CM-panel sub-interface for the two Apple backends.
- [ ] Keep the old free functions as thin forwarders until all callers and tests (`test/*.cpp` use them directly: `pixtest` 24, `dhgrtrace` 17, `gmcgav1test` 9) are migrated.
- [ ] The CM panel API is only meaningful on the Apple pair. DOS has no panel, so keep it off the generic interface.
- [ ] Merge the duplicated header prose. Each header re-explains the slow-draw model. Keep the long explanation in one header and point the rest at it.
- [ ] Debug hooks. Apple has `GM_TRACE` (`g_gmWriteLog`, `g_gmOnOp`), DHGR has `DHGR_WATCH`, and CGA has `GMCGA_OPLOG` / `GMCGA_WATCH` / `GMCGA_TRACE_*`. Do these as one `gmTrace.h` after the dispatcher template exists, because the op-level hooks belong in the shared dispatcher.

## Suggested order

1. A1, B1 (mechanical, self-contained)
2. A2, A3, B2 (small shared helpers)
3. A4, A5 (CM panel and slow policy; run `cmdhgrtest` and `cmhourglasstest`)
4. B4, A7 (page shells and output sampling)
5. A6 and B3 together (shared dispatcher)
6. Part C (API unification)

Estimated saving: about 250-350 lines on the Apple side, mostly from the CM panel and the dispatcher, and about 150-200 lines on the DOS side, mostly from the PCjr dispatcher and shell. The larger benefit is that each of these 6502 / x86 quirks (the `x-1` brush offset, the `DRAW_BOX` pen position, the RLE termination rule) would live in exactly one place.
