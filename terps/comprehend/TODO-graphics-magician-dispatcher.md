# TODO: one opcode dispatcher template for all four Graphics Magician renderers

Follow-up to `TODO-graphics-magician-consolidation.md` (items A6 and B3). That file lists the duplication. This one is the step-by-step plan for the largest single piece: replacing the four hand-written opcode loops with one `gmRunStream<R>()`.

Today's loops:

| Renderer | Loop | State |
|---|---|---|
| Apple hi-res | `doImageOp` + `gmDrawImage` (`graphics_magician.cpp:410-596`, `888-927`) | `a2_ctx` |
| Double hi-res | inline `switch` in `gmDhgrDrawImage` (`graphics_magician_dhgr.cpp:701-841`) | file statics (`s_penX`, `s_penColor`, `g_brush`, `s_textX`, `g_fill*`) |
| CGA | `doImageOp` + `gmcgaDrawImage` (`graphics_magician_cga.cpp:1241-1463`) | `GmcgaCtx` + file statics (`s_fill_*`, `s_pf*`) |
| PCjr | `doImageOp` + `gmpcjrDrawImage` (`graphics_magician_pcjr.cpp:362-477`) | `PcjrCtx` |

All four decode the same byte stream: high nibble = opcode, low nibble = `param`, with bit 0 of `param` as the x-high bit for coordinate operands. The loop skeleton, operand decoding and the order of `DRAW_BOX` lines are identical. What differs is a short list of per-opcode leaf behaviours, which become the renderer's side of the interface.

## Design

- [ ] New header-only `gm_stream.h` (same style as `slow_draw_page.h` and `graphics_magician_vector.h`). It must stay header-only: the test harnesses link exactly one renderer `.cpp`.
- [ ] `template <class R> void gmRunStream(R &r, const uint8_t *data, size_t size)`.
  - `R` is a stateful object that each `.cpp` defines locally and wraps its existing statics or ctx struct. It is **not** CRTP and has no virtuals. The template inlines it.
  - It owns: the `guard < 100000` loop, the `ptr >= end` check, the opcode/`param` split, operand decode, and the `DRAW_BOX` sequence.
  - It calls `r.begin()` once and `r.end()` once. `end()` runs on every exit path, including `OPCODE_END`, and replaces DHGR's `SyncOnExit` guard and the explicit `syncIfNotRecording()` calls.
- [ ] Per-opcode contract. Coordinates arrive as *source* coordinates (x 0..511, y 0..255) and the renderer scales them:

| Opcode | Shared code does | Renderer hook | Per-renderer behaviour to keep |
|---|---|---|---|
| 0 END | return | none | all four identical |
| 1 SET_TEXT_POS | decode x,y | `setTextPos(x,y)` | DHGR stores `x << 1` |
| 2 SET_PEN_COLOR | pass `param` | `setPen(param)` | CGA maps through `kPenTo2bpp`; PCjr `param & 0xf`; DHGR stores raw; Apple `gm_set_color` |
| 3 TEXT_CHAR / 5 TEXT_OUTLINE | read `ch` | `drawText(ch, normal)` then `r.textAdvance()` | advance is 7 (Apple), 14 (DHGR), 8 (CGA/PCjr) |
| 4 SET_SHAPE | pass `param` | `setShape(param)` | none |
| 6 SET_FILL_COLOR | read byte | `setFill(b)` | Apple/DHGR resolve to pattern rows; CGA/PCjr keep the selector |
| 7 OP7 | read 2 bytes | `op7(i,v)`, returns `bool end` | Apple: dialect-dependent end or no-op (`s_legacyFormat`); DHGR: `SET_PALETTE_SUBINDEX` (`s_subidx[i]=v`); CGA/PCjr: no-op |
| 8 MOVE_TO | decode x,y | `moveTo(x,y)` | DHGR doubles x |
| 9 DRAW_BOX | decode a,b; remember pen start; call `lineTo` four times; `moveTo(a,b)` | `lineTo(x,y)`, `penX()`, `penY()`, `scaleX()` | see "Pen-state subtleties" |
| 10 DRAW_LINE | decode x,y | `lineTo(x,y)` | DHGR colour pens align x down to 4 and leave the pen on the aligned x |
| 11 DRAW_CIRCLE | read radius | `circle(r)` | Apple restores the pen to the centre after; DHGR and CGA/PCjr leave it |
| 12 DRAW_SHAPE | decode x,y | `drawBrush(x,y)` | CGA/PCjr subtract 1 from x (`PicStampBrushShape` starts with `DEC AX`); DHGR doubles x |
| 13 DELAY | read units | `slow().recordDelay(units)` | identical in all four |
| 14 PAINT | decode x,y | `paint(x,y)` | DHGR doubles x; others differ in clip-rect handling |
| 15 RESET | pass `param`, `ptr`, `end` | `opReset(param, ptr, end)`, returns `bool end` | the one hook that stays a per-renderer sub-switch (Apple legacy end, Talisman sub-ops, CGA v1 clip vs v2 rect, PCjr skip-4) |

- [ ] Hooks that need no shared code are plain methods on `R`. Optional hooks (trace) use `if constexpr (R::kTrace)` so the PCjr and release builds carry zero overhead.
- [ ] Handlers that can end the image (`op7`, `opReset`) return `true`. This replaces Apple's `done` flag and DHGR's early `return`.

## Steps

### 0. Freeze a baseline

- [ ] Run and record all existing harnesses green before touching anything: `test_dhgr_pics`, `test_dhgr_determinism`, `cmdhgrtest`, `cmhourglasstest`, `pixtest`, `test_gmcga_pics`, `test_gmcgav1_pics`, `test_gmpcjr_pics`, `diff_gmcga`.
- [ ] For each renderer, capture the full page buffer after every corpus image into a throwaway directory (reuse the dumps the harnesses already produce where they exist). The dispatcher change must leave these byte-identical, **including the slow-draw op sequence**, not just the final page. Compare `SlowDrawEngine`'s recorded op list too, since the reveal order is observable. Add a one-off dump of that list if no harness has one.

### 1. Land the small shared helpers (from the consolidation doc)

- [ ] `GmFillBounds` (A2), `gmReadXY` (A6) and `gmDrawBox` (A6). These make step 2 smaller and each is separately verifiable.
- [ ] Switch the four existing loops to use them first, still hand-written. This isolates any behaviour change from the template work.

### 2. Write `gm_stream.h` and a mock-renderer test

- [ ] Implement `gmRunStream<R>` with the table above.
- [ ] Add `test/test_gm_stream.cpp` using a `RecordingRenderer` whose hooks append `"lineTo 10,20"`-style strings. Check: operand decode with x-high bit; `DRAW_BOX` call order and final `moveTo`; `end()` is always called, even on `OP7`-ends, `RESET`-ends, truncated input and the guard limit; `textAdvance()` is applied after both text ops; unknown or reserved opcodes behave as today (fall through with no operands consumed).
- [ ] No renderer `.cpp` is changed in this step, so it cannot affect any golden.

### 3. Port PCjr first (smallest, strict subset)

- [ ] Define `PcjrRenderer` in `graphics_magician_pcjr.cpp` wrapping `PcjrCtx`: `begin()` does the per-image init (pen `(140,6)`, brush 5, `fill_sel` 0, text `(140,96)`; JR_GRAPH `0xa33`) and `end()` calls `s_slow.syncIfNotRecording()`.
- [ ] Delete `doImageOp` and the loop in `gmpcjrDrawImage`; it becomes `PcjrRenderer r; gmRunStream(r, data, size);`.
- [ ] Verify: `test_gmpcjr_pics` and the step-0 comparison.

### 4. Port CGA

- [ ] `CgaRenderer` wraps `GmcgaCtx` and the `s_fill_*` and `s_pf*` statics. `begin()` also resets the fill rectangle and the flood-fill clip (`FUN_2556`).
- [ ] `opReset` keeps both the v1 clip branch and the v2 rect branch (using `GmFillBounds` from step 1).
- [ ] Re-home the `GM_TRACE` blocks (`GMCGA_OPLOG`, per-PAINT dump, `GMCGA_WATCH`) onto optional hooks `onOp()`, `beforePaint()/afterPaint()` and `afterOp()`. They must still compile out in non-trace builds.
- [ ] Verify: `test_gmcga_pics`, `test_gmcgav1_pics`, `diff_gmcga`, and the v1 crypt trace in `test/gmcgav1`.

### 5. Port double hi-res

- [ ] `DhgrRenderer` wraps `s_penX/Y`, `s_penColor`, `g_brush`, `s_textX/Y` and `g_fill*`. `scaleX(x)` returns `x << 1`, and `textAdvance()` returns 14.
- [ ] `begin()` carries the order-sensitive init from `gmDhgrDrawImage`: restore `s_subidx` from `s_subidxDefault` **first**, then pen colour 4 at `(280,96)`, `set_fill_color(0x4d)`, full-screen fill bounds and `g_brush = 5`. The comment explaining why the order matters must move with the code.
- [ ] `op7` writes `s_subidx[i] = v`; `opReset` dispatches to `rle_bitmap_dhgr`, the bounds sub-ops and `fill_rect`.
- [ ] Keep `DHGR_WATCH` (`s_dhgrOpTag`) behind `onOp()`.
- [ ] Verify: `test_dhgr_pics`, `test_dhgr_determinism`, `cmdhgrtest`, `titledhgrtest` and `dhgrtrace`.

### 6. Port Apple hi-res (most dialect logic, so last)

- [ ] `AppleRenderer` wraps `a2_ctx` and delegates pen, line, brush and colour work to the `gm_*` functions in `common_sagadraw`; those take `ctx->gv`, so the renderer holds it by value.
- [ ] `op7()` and `opReset()` consult `s_legacyFormat` and return `true` for the legacy end. Check that `fill_bottom` is still `0xbf` for the legacy dialect and `0x9f` for Talisman in `begin()`.
- [ ] `circle(r)` ends with `gm_set_draw_position(centre)` so the pen is restored as before.
- [ ] Keep `GM_TRACE` (`g_currentPrim`, `g_gmOnOp`) behind `onOp()`.
- [ ] Verify: `pixtest` (24 call sites, highest coverage of this file), `cmhourglasstest`, and a legacy-dialect image (Transylvania or Crimson Crown) as well as a Talisman one, because `s_legacyFormat` is only exercised by the former.

### 7. Cleanup

- [ ] Delete the dead per-renderer ctx structs, `doImageOp`s and now-unused statics. Update the `Opcode` comment in `graphics_magician.h` to mention `gm_stream.h`.
- [ ] Update the top-of-file comments of the four `.cpp` files. They describe "the opcode interpreter" and should now point to the shared one.
- [ ] If `gm_stream.h` needs a Spatterlight Xcode entry, add it to `Spatterlight.xcodeproj/project.pbxproj` and check the other build files (CMake/Makefiles under `test/`) pick up the new include path.

### 8. Optional follow-ups (behaviour changes, do separately)

- [ ] **Bounds-checked operand reads.** Today the loops read operand bytes without checking `end`, so a truncated stream reads past the buffer. The template can check that `n` operand bytes remain before each opcode and stop otherwise. That is a behaviour change for malformed data only, so keep it as its own commit, and check that no shipped image trips it (the DHGR RLE already has such checks).
- [ ] **Unify the `guard` limit and the unknown-opcode policy.** Currently all four use 100000 iterations, but Apple's `done` and DHGR's `return` differ on truncation.
- [ ] **Shared op7 naming.** `OPCODE_OP7` is "2-operand no-op / end / `SET_PALETTE_SUBINDEX`" depending on renderer. Once it is one `op7(i,v)` hook, rename the enum value in `graphics_magician.h` if the host code allows.

## Pen-state subtleties to get right in the template

These are the easiest ways to introduce a one-pixel diff:

- [ ] **`DRAW_BOX` pen.** After the four lines the pen is at the far corner `(a,b)`, not the start (comment at `graphics_magician.cpp:491-494`: a Talisman RA #1 regression of 11 bytes). The template must call `moveTo(a,b)` itself.
- [ ] **DHGR colour-pen alignment.** `line()` rounds x to a multiple of 4 and leaves the pen on the aligned x. Capture `x0,y0` from `penX()/penY()` **before** the first `lineTo`, exactly as `gmDhgrDrawImage` does, so chained boxes line up as today.
- [ ] **Operand `x` scaling happens in the renderer**, not the template: the box corner `a` must be scaled once (`scaleX(x)`) and passed to `lineTo` unchanged on every edge.
- [ ] **Circle and pen**: Apple restores the pen after the circle, DHGR and CGA/PCjr never move it. Keep this inside each renderer's `circle()`.
- [ ] **Text advance after a glyph**, not before, and only for ops 3 and 5.

## Risks

- **Pixel diffs from a mis-ordered init.** `begin()` is where the dialect-specific defaults live. Compare the full page after each port, not just the harness's pass/fail.
- **Template compile-time cost and the single-TU harnesses.** The hooks are small and inline, so this should be negligible; if not, move rarely-used hooks out of line.
- **Large diff in the Apple file.** The Apple dispatcher carries the legacy dialect and the trace hooks, so port it last, with the others already proven.
