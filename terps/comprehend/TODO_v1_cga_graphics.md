# v1 Comprehend DOS (CGA) graphics

Crimson Crown and Transylvania v1 render through `graphics_magician_cga.cpp` (the v2 renderer)
via `gmcgaInstallV1DrawingTables` — fill pattern + subindex tables from `PC_GRAPH.OVR`, brushes
from `NOVEL.EXE`. Full RE record: memory `comprehend-v1-cga-graphics`.

## Fill pattern model (corrected 2026-06-13)
v1 fills are **period-4**, like v2: one pattern byte replicated across every column — they are
**not** indexed by the CGA byte column. Each 30-entry pattern slot stores that byte four times;
the renderer reads `entry[0]`. The two-colour dithers that span more than 4 px are shipped as
**phase-shifted index pairs** (12 `80 40..` / 13 `40 80..`, 14/15, 16/17, the period-16 pair
20/21), and the subindex picks whichever index carries the right alignment for that fill. The
earlier "4 explicit column-phase bytes, indexed by byte column" model was wrong: it swapped
cyan↔magenta across every dither (Crimson Crown crypt: 2827 px). Fixed in
`fill_pattern()` / `pf_pattern_word()`; validated against a DOSBox CGA capture.

## Regression set (`make test`, `test/test_gmcgav1_pics`)
- Crimson Crown: lakeshore (RA pic 0), woods (RA pic 2), cave (RA pic 6): pixel-exact, ceiling 0.
- crypt (RA pic 3): the only dithered fixture, so it guards the fill-pattern path. Ceiling 3: an
  **op12 brush** spill-byte phase residual at x=188 (cyan vs magenta on 3 odd rows). The flood fill
  itself is pixel-exact — proven by the live capture; the 3 px are a later brush stamp, same as
  residual #2 (see "OPEN" below). Capture with `test/gmcgav1/find_magenta_op.py`.
- Transylvania v1: tr_start (RA pic 0), pixel-exact, ceiling 0 — its fill/subindex/brush tables
  are byte-identical to Crimson Crown's, so the same committed table slices render both games.

## In-picture text (op3/op5) — DONE
v1 pictures **do** draw op3/op5 text (Transylvania map screens MA–ME, Crimson Crown MA/RC). v1
`NOVEL.EXE` has no embedded picture font; the glyphs come from **CHARSET.GDA** (version 0x1100,
96 glyphs ×8 bytes at offset 4, LSB-first). `gmcgaSetV1Font()` loads them bit-reversed into the
renderer's glyph table; `pics.cpp` calls it on the v1 path. Validated: a synthetic op3 string
renders legible, correctly-oriented upper/lower-case + digits + punctuation.

## Whole-corpus ground truth (2026-06-13)
Every v1 picture is now dumped from the real interpreter and diffed against the renderer:
`test/gmcgav1/dosbox_dump_all_v1.py` (MODE=sweep) captures raw CGA VRAM per picture by driving
`NOVEL.EXE`'s high-level entry points (FUN_1000_0b9f room / 0bc3 object → load+draw), and
`test/gmcgav1/validate_dump.py` renders each through `gmcgav1test` and reports the windowed
mismatch count. Objects are composited over their room base (from the `.GDA` item→room→graphic map)
and `GMCGA_CHARSET` is passed for the op3/op5 text cutscenes. Other v1 games are dumped by booting
Crimson Crown's binary on a merged mount (shared engine + their `.MS1`). Full method: memory
`comprehend-v1-dumpall-harness`.

Result after the 2026-06-13 brush blitter painted-byte phase-walk fix: **Crimson Crown 71/71**,
**Transylvania 81/81** pixel-exact at ceiling 0 — the WHOLE v1 corpus is now byte-exact. (History:
64/71 + 67/81 raw; 65/71 + 70/81 after the per-column phase fix; 71/71 + 81/81 after the byte-walk.)
Corpus dumps: `/tmp/v1cap/cc`, `/tmp/v1cap/tr`. Re-validate with `validate_dump.py 0`.

## MOSTLY FIXED: dither residual was a BRUSH/text/rect pattern-PHASE bug, not flood fill (2026-06-13)
The "flood-fill span-boundary residual" hypothesis was **wrong**. The live per-op14 capture
(`test/gmcgav1/dosbox_capture_op14_v1.py` + `compare_op14.py`, the workflow this TODO called for)
proved it: it dumps native VRAM *before and after* every op14 PAINT and diffs fill-by-fill against
the renderer's `GMCGA_TRACE_DIR` snapshots. For CC `RB_03` the single flood fill is **pixel-exact
before AND after** — the 51-px residual is written *later*, by an **op12 brush stamp** (op#186,
`fill_idx=73`) painting the `[40 80 40 80]` dither over the fill.

**Root cause.** `fill_pattern()` (the brush op12 / fill-pattern-text op5 / fill-rect op15 path —
NOT the flood fill) collapsed every v1 dither to pattern `entry[0]`. That is right only for the
flood fill, whose word painter is even-byte-aligned (phase 0/2, equal for the shipped dithers).
The per-pixel blitters stamp at **arbitrary** byte columns. PC_GRAPH.OVR's brush blitter (`0x88c`)
indexes `pattern[idx*4 + ([0x9382] & 3)]` where `[0x9382] = (x+20)>>2` is the **global** CGA byte,
advancing one phase per byte written (`0x94c`). The picture window is +20 px = +5 bytes ≡ +1 (mod 4),
so the correct phase = `(logByte + 1) & 3`. Fixed in `fill_pattern()`; Ghidra-confirmed (OVR
`0x88c`/`0x94c`, both verified live).

**A/B over the full corpus** (`compare_op14.py` capture + an old-vs-new diff of every picture):
every changed picture improved, none regressed except TR `OB_09` +1 px.
- CC: `RB_03` 51→**0**, `RB_12` 125→33, `OB_01` 204→69. Pixel-exact 64→65/71.
- TR: `OA_02` 57→**2**, `OA_13` 93→6, `OA_09` 65→10, `OB_02` 159→43, `OC_05` 54→11, `RA_08` 44→12,
  + several 1-px → 0. Pixel-exact 67→**70**/81.
- Regression set fixtures (`make test`) unchanged: lakeshore/woods/cave/tr_start exact, crypt 3.

## The rectangle op (op15) — v1 semantics fixed 2026-06-13
op15 in the v1 games is **not** a rectangle paint. The native handler (PC_GRAPH.OVR `0x2de`) is
flood-fill **clip-rect setup**: param 0 = no-op; param 1 = reset the clip to the window defaults
(`[0x9387]`=left byte 4, `[0x9388]`=right byte 0x4a, `[0x9389]`=top row 0, `[0x938a]`=bottom row
0x9f on CGA, `[0x238f]==2`); param ≥2 = read 4 stream bytes into those four bounds. It never writes
a pixel. The shared renderer's previous op15 (fill the rect with the pattern) is the **v2 / Apple
Talisman** behaviour and is kept for `!s_v1`. The v1 path now matches the native: it sets
`s_pfLeftByte/RightByte/TopRow/BottomRow` (the flood-fill clip, used by `gmcga_flood_fill` /
`pf_scan_paint`) and paints nothing. **Inert for the current corpus** — a full scan of every CC and
TR room/object/title/map picture shows *no* valid picture emits op15 (the only hits are bytes read
past a valid 17-word offset table). Kept faithful anyway so the stream pointer stays aligned and any
clip-narrowing picture would render correctly. Zero pixel change; v1 fixtures + CC 65/71 + TR 70/81
+ v2 94/94 all unchanged.

## RESOLVED 2026-06-13: the residual family was the brush blitter's painted-byte dither phase
**All of the below is now FIXED** (`blit_col` in `graphics_magician_cga.cpp`); kept as the RE record.
Both residuals (#1 crypt magenta, #2 RB_12/OB_01 cyan) were one bug: the native brush blitter advances
the dither phase **per painted output byte, not per byte column**. The spill helper `0x94c` does
`cmp dl,0; je 0x96d` *before* `inc al; inc di`, so a blank intermediate output byte advances `BX` but
**not** the pattern index. A `trace_blit_row.py` single-step of crypt row 25 (gb=50, idx4=52, cols 50
& 51 blank, col 52 painted) showed the live store `col=52 BX=0x23f4 DI=55 0x40->0x80`: phase =
(gb&3=2) + 1 (the single painted byte) = **3 = `0x80` magenta**, not col52&3 = 0 = cyan. `blit_col`
now bit-doubles the row, shifts by the sub-pixel offset into a 3-byte mask, and walks the phase only
for painted bytes (routed through `fill_pattern`, so v2 — which is phase-independent — is untouched).
Result: **make test crypt 3→0**, v2 **94/94** unchanged, full corpus **CC 65→71/71, TR 70→81/81**.
The DI off-by-one (blocker #1) and v2-compat (blocker #2) are both closed. Trace: `trace_blit_row.py`.

1. **crypt RA3 (3 px @ x188, odd rows 25/27/33) — NOT a flood-fill corner. It is an op12 BRUSH,
   same family as #2 (reclassified 2026-06-13 by live DOSBox capture).** The old "span-queue
   TRAVERSAL corner in op14_paint" hypothesis was **wrong**. Proof:
   - `dosbox_capture_op14_v1.py TARGET=RA:3` + `compare_op14.py`: all **13** op14 flood fills are
     pixel-exact BEFORE *and* AFTER. The state right after the last fill (fill 13, sel 71) has
     (188,25)=**cyan in BOTH** native and renderer (`fill_013_after.vram` vs our `_after.raw`). The
     fill itself is byte-perfect — nothing for the word-paint-order trace to find.
   - The magenta is written **later**, by op#258 of the brush blitter (`OVR 0x88c`). `find_magenta_op.py`
     breakpoints the blitter and watches the target byte: it flips `0x40→0x80` (cyan→magenta) during a
     brush stamp at `x1=181, y1=48`, pattern `idx4=52` (= pattern 13, the `40 80 40 80` dither). Global
     x=201 ⇒ sub-pixel 1 (off the byte grid), so the brush spills across global bytes 50/51/**52** and
     the spill byte covering x188 lands pattern phase **1 = `0x80` = magenta**, not the phase-0 `0x40`
     our `blit_col` paints there.
   - Our `fill_pattern()` ties the phase to **each pixel's own picture byte** (`(x>>2 + 1)&3`; x188 ⇒
     `(47+1)&3 = 0` ⇒ cyan). The native ties it to the **blitter's output-byte cursor** as the shifted
     column spills right, which for an off-grid brush puts a different phase on the spill byte. That is
     **exactly residual #2's mechanism** ("brush stamped in the margin whose 3-byte spill lands on a
     fill"). Capped at 3 (benign). Repro: `find_magenta_op.py`; local: `test/gmcgav1/trace_crypt.cpp`
     (`GMCGA_OPLOG=1` lists ops; `GMCGA_WATCH="188,25"` names the writer; `GMCGA_TRACE_FILL=n` logs the
     Nth fill's pushes + per-word `WORD seq=` paints).
2. **"Brush residual" at `x&3==1`, odd rows (CC `RB_12` 33 px, `OB_01` 69) — NOT a brush-blitter
   bug (ruled out 2026-06-13).** A faithful, independent reimplementation of the native column
   blitter — bit-double the 8-bit row to a 16-bit 2-bpp mask (`0x88c` `SHL DL,1;RCL AX,1` ×8),
   shift right by the `[0x9383]` sub-pixel offset spilling into a third byte, paint the pattern
   through the mask with per-output-byte phase (`0x94c`) — produces output **byte-identical** to
   `blit_col()` on these pictures. So the 8-bits→8-px quadrant model is provably correct; the
   bit-doubling is just the 1bpp→2bpp mask expansion (8 px wide, not 16). The residual traces to a
   brush stamped in the **left margin** (e.g. RB_12: brush 7 at x=-1 → global byte 4, sub-pixel 3)
   whose 3-byte spill lands on top of an earlier op14 flood fill. **#1 is the same bug in the other
   direction**: there the native *does* cover the spill pixel (magenta) where ours leaves the fill's
   cyan, because our per-pixel phase (`x>>2`) disagrees with the native's output-byte-cursor phase on
   the off-grid spill byte. So #1 and #2 are one issue: `blit_col`'s pattern phase must follow the
   shifted column's spill-byte cursor, not each pixel's own picture byte.
   `GMCGA_DIFF=1 ./gmcgav1test … golden.fb` lists the per-pixel diffs; `GMCGA_WATCH="x,y"` names the
   writing op.

**Reproduce** (golden = DOSBox VRAM `.fb`, render = `gmcgav1test` windowed compare):
- Full corpus dumps live in `/tmp/v1cap/cc` + `/tmp/v1cap/tr` (6-col manifests); validate with
  `V1GAME=… V1OUT=… python3 test/gmcgav1/validate_dump.py 0`.
- Single fill study: `test/gmcgav1/dosbox_capture_op14_v1.py` (`TARGET=RB:3`) then `compare_op14.py`.
- Find the op writing a bad pixel: `GMCGA_WATCH="x,y" ./gmcgav1test …` (logs op#/op/param per change).

## DONE — matched the brush blitter's painted-byte pattern phase (fixed #1 AND #2)
**Implemented 2026-06-13.** The single-step trace below was run (`trace_blit_row.py`) and pinned the
off-by-one: it is NOT a `[0x9382]→BX` +1, but the `0x94c` `je 0x96d`-before-`inc di` that skips the
phase advance on a blank output byte. `blit_col` now replicates the bit-double → sub-pixel shift →
3-byte mask → painted-byte phase walk, routed through `fill_pattern` for v2 safety. The prose below
is the original plan, kept for the record.

Both residuals are the **brush column blitter** (`OVR 0x88c` + spill helper `0x94c`, our `blit_col`):
when a brush is stamped off the byte grid (global x not ≡0 mod 4), the native shifts the 8-px column
right by the sub-pixel offset (`[0x9383]`) so it spills across **three** output bytes, and the spill
byte's fill-pattern phase follows the blitter's **DI byte-walk**, not the pixel's own byte column.

**Root cause PROVEN at the instruction level (2026-06-13, `find_magenta_op.py` store-trace).** For crypt
the painter is the op12 brush at picX≈181→`gb=50, sub=2`, selector 71 (odd-row pattern idx 13 =
`[40 80 40 80]`), pattern-mode (`[0x9381]=0`). Our `blit_col` DOES cover x188 (it is NOT a coverage
gap — `GMCGA_BLITLOG` confirmed it paints there) but uses pattern phase `(x>>2 + 1)&3 = 0` ⇒
`entry13[0]=0x40`=cyan. The native store to that VRAM byte uses **`DI=55` ⇒ `pattern[55]=entry13[3]=0x80`
=magenta** (verified live: `STORE@spill TGT 0x40->0x80 DI=55 pat=0x80`). So the spill byte takes phase
3, not 0.

**Two OPEN sub-problems before this can be ported safely:**
1. **The DI-walk has a byte-cursor off-by-one the static disasm read doesn't reproduce.** From `0x88c`/
   `0x94c` the walk should put byte `gb+2` (=byte52) at `DI = idx4 + ((gb&3 + 2)&3) = 52` (phase 0,
   cyan) — yet the live store shows byte52 painted at `DI=55` (phase 3), i.e. it was the **2nd** output
   byte of the walk (`DI_1`), meaning the paint started at byte `gb+1`, not `gb`. The relationship
   between `[0x9382]` (read as 50) and the actual `BX`/`DI` start needs a **single-step trace of one
   full `0x88c` row** (watch `BX`, `DI`, `[0x9380]` at `0x8e7`→`0x90b`→`0x94c`) to pin the +1. Until
   that's known, any closed-form phase is guesswork.
2. **A faithful `blit_col` byte-walk MUST stay v2-compatible.** A v1-only rewrite (indexing
   `s_v1PatRaw[sub][phase]` directly) regressed the v2 corpus to **39/94** — v2 fills use `s_fillTable`
   (period-4, phase-independent) via `fill_pattern`. The port must branch on `s_v1` like `fill_pattern`
   does, or route the per-output-byte phase THROUGH `fill_pattern` (which already handles both). Tested
   2026-06-13 and reverted; `blit_col` is back to the per-pixel version (94/94, crypt 3).

When both are resolved: paint each output byte `gb+k` through its shifted mask using the pattern phase
the DI-walk actually produces (per #1), via `fill_pattern` so v2 is unaffected (per #2). `make test`
(crypt→**0**, rb_03 stays 0, v2 94/94) + `validate_dump.py 0` on `/tmp/v1cap/{cc,tr}` (hold CC 65/71,
TR 70/81). Re-test RB_12 brush 7 (x=-1, left spill) and the crypt brush (x=181, right spill).

### NEXT STEP — single-step trace of one `0x88c` row to pin the DI/BX off-by-one (blocker #1)
The goal is one table: for the crypt painter brush, on **row 25**, the ordered sequence of
`(store address → VRAM byte, DI, pattern[DI], BX)` for the 1st store (`0x90b`) and each spill store
(`0x96a`), so the `[0x9382]→BX` and `DI` start/increment/wrap are nailed exactly. Build on
`find_magenta_op.py` (it already boots NOVEL1.EXE → crypt RA3, locates `code_lin`, arms the stores at
brush hit 256, and reaches the painter stamp #257 = gb 50, sub 2, rows 18–25). Procedure:

1. **Re-derive the linear addresses at runtime** (don't hardcode `code_lin`): the script already finds
   `code_lin` via the `FUN_1000_0c4c` signature and `ds_lin = code_lin + 0x3900`. Brush blitter
   `brush_lin = code_lin + 0x2de7 + 0x88c`. Single-step landmarks inside `0x88c` (add `0x2de7` to each
   OVR offset, then `+ code_lin`): `0x8e7` `ADD BX,AX` (BX gets the byte column from `[0x9382]`),
   `0x8f0`–`0x90b` 1st-byte paint+store, `0x912`/`0x919` the two `CALL 0x94c` (2nd/3rd spill bytes),
   `0x94c`–`0x96a` the spill paint+store.

2. **Gate to row 25 of the painter.** When a `brush_lin` hit has `gb=[0x9382]==50 && [0x9366]<=25` and
   it is the Q0 stamp (the one whose `[0x9366]` start is 18), single-step from entry with `gdb.step()`
   until `[0x9366]==25` is the row being painted (the row loop `INC [0x9366]` is at `0x944`). Cheaper:
   set a breakpoint at `0x8e7` and continue until `[0x9366]==25` for this stamp, then single-step the
   rest of that row only (down to the next `0x944`).

3. **Log at every step that touches BX/DI/the stores.** At `0x8e7` after it executes: `BX` (row base +
   byte col), `[0x9382]`, `[0x9380]` (idx4, set per row by the `0x44` call at `0x89e`). At `0x8f8`
   `MOV DI,AX`: the initial `DI` (`= (gb&3)|idx4`). At each store (`0x90b`, `0x96a`): `EBX&0xffff`,
   `EDI&0xffff`, `pattern[ds_lin+0x9111+DI]`, `DX` (DH=value stored, DL=mask). The decisive question:
   **what VRAM byte does the FIRST store (`0x90b`) hit — byte 50 or byte 51?** If 51, the paint starts
   at `gb+1` and the `[0x9382]→BX` map carries a +1 (the off-by-one); reconcile against `0x44`’s BX
   (it returns `bank + (row>>1)*80`, no byte col — so the +1, if real, is in the `ADD BX,AX` operand or
   a pre-increment of `[0x9382]` the op12 handler `0x85d`/`0x821` does that the entry-time read misses).

4. **Cross-check the walk formula.** With the per-byte `(BX, DI)` pairs in hand, the phase for output
   byte at global byte `N` is `DI&3` (since `DI = idx4 + phase`, `idx4` a multiple of 4). Confirm the
   closed form (candidates: `N&3`, `(N+1)&3`, or `(gb&3 + k)&3` with the corrected start byte). The fix
   in `blit_col` then phases output byte `gb+k` by that form, routed through `fill_pattern` (blocker #2).
   Sanity: it must yield `entry13[3]=0x80` (magenta) at byte 52 for `gb=50,sub=2` and `entry13[0]=0x40`
   for the byte-aligned brushes that already pass.

5. **Watch the harness footguns.** `find_magenta_op.py` reads DGROUP state at the `0x88c` *entry*
   breakpoint, which is **stale** for per-row values (`[0x9380]` is recomputed by `0x44` each row; the
   first capture mis-read it as the next stamp's). Read per-row values only *after* the row’s `0x44`
   runs. The wrapper has no hardware watchpoints (only code breakpoints) — that's why this is a
   single-step trace, not a memory watch. Keep the store filter tight (only `ES*16+BX ∈ {row25 bytes
   50,51,52}`) or the GDB round-trips are slow.

**Capture tooling (all committed in `test/gmcgav1/`, harness verified working 2026-06-13):**
- `find_magenta_op.py` — boots NOVEL1.EXE, reaches crypt (RA pic 3 / id 4), breakpoints the brush
  blitter `OVR 0x88c` and reports each hit's effect on a target VRAM byte. This is what pinned #1 to
  brush op#258 (`x1=181`, pattern 13, `0x40→0x80`).
- `dosbox_capture_op14_v1.py` + `compare_op14.py` (`TARGET=RA:3`) — fill-by-fill BEFORE/AFTER; proved
  every flood fill exact.
- `trace_crypt.cpp` — single-picture local driver (`GMCGA_OPLOG`/`GMCGA_WATCH`/`GMCGA_TRACE_FILL`).

## Other remaining follow-ups
- [ ] Lock a **map** golden (Transylvania MA.MS1 — version 0x2000 composite, drawn cumulatively
      across fragments) so the CHARSET.GDA text path is regression-tested pixel-exact, not just
      eyeballed. (MA–ME load via FUN_1000_0c9f, the 'm'-prefix path — not covered by the room/object
      sweep, which only drives 0b9f/0bc3.)
- [ ] Object (`OA`/`OB`) and title images. The title (`CCTITLE.MS1`) is a **full-screen** 320×200
      image (art extends outside the 280×160 picture window), so it needs a full-frame compare,
      not the windowed one the room test uses.
