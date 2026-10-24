# Testing terps/comprehend

```
cd terps/comprehend
make check
```

`make check` runs the renderer fixtures (`make test`), builds the headless
interpreter (`Makefile.headless`, `comprehend_hl`) and plays the scripted
walkthroughs (`test/walkthrough/run_walkthroughs.sh`). The walkthroughs need
the local-only game files and skip, with a message, any game that is absent.
After a change that adds a source file, also run

```
xcodebuild -project ../../Spatterlight.xcodeproj -scheme Spatterlight build
```

since every `.cpp` is listed in the Xcode project and a new one needs a pbxproj
entry.

## The suites

Each fixture directory has its own README with the ground truth and how the
fixtures were captured:

| Directory | What it proves |
|---|---|
| `gmcga/`, `gmcgav1/` | Graphics Magician CGA renderer, v2 and v1, pixel-exact against DOSBox-X captures |
| `gmpcjr/` | the PCjr renderer: 331 pictures of the two DOS v1 folders with `JR_GRAPH.OVR` (Transylvania, Crimson Crown) as hashes, 73 of them pixel for pixel against DOSBox-X captures |
| `talisman/` | Apple II hi-res Talisman pictures |
| `cm/`, `cm_dhgr/` | The Coveted Mirror panel and hourglass, hi-res and double hi-res, byte-exact page fixtures |
| `title_dhgr/` | double hi-res title screens |
| `walkthrough/` | six full-game scripted plays (CM, CC, OO, TM, TR, TR v2) with byte-identical transcripts |
| `test_gm_stream` | the shared opcode dispatcher against a recording mock renderer: operand decode, `DRAW_BOX` order, `end()` on every exit, truncation |
| `gm_stream_diff.sh <ref>` | differential: every renderer's page after random streams, working tree against a git ref; needs no fixtures |
| `gm_stream_exact.sh` | random streams, some cut short, each in a heap block of its exact length under ASan; any operand over-read aborts |

The picture logs (`COMPREHEND_PICLOG`) of every game x colour mode x slow-draw
setting were the check for the renderer dedup of October 2026; the walkthrough
transcripts cover the game layer. `test_dhgr_pics` and `test_dhgr_determinism`
are in `make test` since that pass.

## Not covered

- The Coveted Mirror for DOS was not at hand when the CGA fallback was tested
  (see below); the other five DOS games were.
- The three remaining DOSBox capture scripts in `gmcga/` and `gmcgav1/` use
  different techniques and were not merged; `dosbox_capture_v1.py [cc|tr]` is
  compile-checked only and has not been re-run against DOSBox-X.
- Performance: profiled 2026-10-13 and found nothing to fix (a whole
  walkthrough costs 5-16 ms CPU; boot is the WOZ reader). Start from a new
  profile, not from this list, before touching the string paths.

## Behaviour the pass of October 2026 settled

- **Every DOS picture goes through the CGA renderer.** The CGA drawing tables
  are the same bytes in every release (the brushes are ScummVM's SHAPE_DATA),
  so `graphics_magician_cga.cpp` carries a built-in copy
  (`gmcgaInstallBuiltinDrawingTables`) and a DOS folder without `NOVEL.EXE` or
  `PC_GRAPH.OVR` renders the same. The ScummVM vector path is gone
  (`GFX_VECTOR`, `charset.cpp`, the `draw_surface.cpp` primitives). The v2
  games still need `NOVEL.EXE` for their strings.
- **One renderer interface.** `gm_backend.h`: `GmBackend` chosen once by
  `gmBackend(GfxBackend)`; `Comprehend::gfxBackend()` derives the enum from the
  platform and the one preference, `_moreColours`. No `_useDhgr` /
  `_usePcjr` ladders remain.
- **Per-game quirks are data**: `game_traits.h/.cpp`, one `GameTraits` row per
  game. Only the id detection strings in `glk_main.cpp` and `apple2.cpp` still
  compare game ids.
- **The hi-res and DHGR flood fill share `gm_span_fill.h`**, with the DHGR
  single-pixel-span rule as the policy constant `kDropSinglePixelSpans`. That
  rule is a real difference (the prison tower floor leak) and must not be
  unified.
- **The PCjr fill tile**: row parity as the game has it, and the flood fill
  tests and paints with bytes 0 and 2 of the tile only. 228 of the 331 PCjr
  hashes were re-blessed for those two fixes against DOSBox-X captures.
- **The Coveted Mirror panel share**: `cm_panel.h` holds the pile-grain walk,
  the grain-fall state machine and `CMPanelStore<Planes>` (capture, overlay,
  rebuild, reveal-skip), instantiated with 1 plane for hi-res and 2 for
  aux+main; each renderer still owns its pages and its grain stamp. The
  `gmCMHourglass*` / `gmDhgrCMHourglass*` names stay for the stand-alone tests.
- **Restart is one base function** with two hooks, `restartPrompt()` and
  `restartGame()`; `game_restart(true)` skips the prompt. Two changes fell out
  of it: OO-Topos clears undo on restart like every other game, and Crimson
  Crown's `#restart` no longer loads disk 1 twice.
- **No pre-parser daemon pass.** The base `beforeTurn()` is empty and carries
  the one canonical comment with the RE addresses; each game's override is its
  native per-turn routine only.
- **Transylvania 1 and 2 share `game_tr_common.h`**; whether the restart verb
  asks first is a constructor argument.
- **The V1 and V2 "object in room" opcodes are not merged**: they test
  different things, and the comment at them says what.
- **Debug hooks are compiled out**: `TM_DUMP` / `TM_TRACE` need
  `-DCOMPREHEND_DEBUG`, the `GMCGA_*` trace hooks need `-DGM_TRACE`. Release
  builds call no `getenv` on those paths.
- **Indentation is tabs** in every source file.

## The dispatcher pass of October 2026

The four renderers (`graphics_magician.cpp` hi-res, `_dhgr.cpp`, `_cga.cpp`,
`_pcjr.cpp`) decode one byte stream: high nibble opcode, low nibble `param`,
bit 0 of `param` the x-high bit. They now share:

- **`gm_stream.h`**: `gmRunStream<R>(r, data, size)` owns the loop, the
  opcode/param split, operand decode, `DRAW_BOX`, `DELAY`, and calls
  `r.begin()` once and `r.end()` on every exit path. `R` is a local struct in
  each `.cpp`, no virtuals, no CRTP. Hooks that can end the image (`op7`,
  `opReset`) return `true`. `gmReadXY`, `gmHave`, `gmOperandBytes`,
  `gmSkipOp7`, `GmFillBounds` (stream order right, left, bottom, top) and
  `gmDecodeRleBitmap` live there too.
- **`cm_panel.h`**: `CMPanelStore<Planes>` (capture, rebuild, overlay,
  reveal-skip), one plane for hi-res and two for aux+main.
- **`gm_blit.h`**: `gmSampleRow560`, the 560-colour row sampled to `w`.
- **`graphics_magician_vector.h`**: `gmvFindBrushTable`, `gmvLoadCharsetGda`
  (loading), `gmvStampBrush`, `gmvDrawGlyph` (shells) beside the line, circle
  and mirror primitives, and `gmvAppleCircleSteps` / `gmvCirclePlot8`, the
  6502 midpoint circle walk shared by the hi-res and DHGR `draw_circle`
  (October 2026). Only the pixel plot, the DHGR half step and the DHGR
  trailing repaint step (kept for the reveal order) stay per renderer.
- **`gm_hires.h`** (in `common_sagadraw`): `gm_row_of_offset` /
  `gm_col_of_offset` replace the two offset tables.

Behaviour that changed, all for malformed streams only: an opcode whose
operands run past the end of the stream ends the image instead of reading
past the buffer (`gm_stream_exact.sh` is the proof), and the loop cap is
100000 for all four (CGA and PCjr used to stop one short). Every fixture suite
above is byte-identical before and after; `gm_stream_diff.sh` compared the
pages of random streams as well.

Per-renderer behaviour the hooks keep, and the one-pixel traps:

- `DRAW_BOX` leaves the pen at the far corner `(a,b)`, not the start (a
  Talisman RA #1 regression once). The template does that `moveTo` itself,
  and captures the start from `penX()/penY()` before the first `lineTo`,
  because the DHGR colour pens align x down to 4 and park the pen there.
- x scaling is the renderer's (`scaleX`: DHGR doubles), applied once to the
  box corner and never again.
- Circle: Apple restores the pen to the centre, the others leave it.
- Text advance after the glyph, ops 3 and 5 only: 7 (Apple), 14 (DHGR), 8
  (DOS). CGA and PCjr stamp brushes at `x-1` (`PicStampBrushShape` starts
  with `DEC AX`).
- `op7` is three things: a two-byte no-op in Talisman-era pictures
  (`gmSkipOp7`), the end of the image in the legacy Apple dialect (which must
  not read the operands), `SET_PALETTE_SUBINDEX` on DHGR. `opReset` stays a
  per-renderer sub-switch: legacy Apple end, Talisman sub-ops, CGA v1 clip vs
  v2 rect, PCjr skip-4.
- `begin()` carries order-sensitive init: DHGR restores `s_subidx` before
  anything else; Apple's fill bottom is `0xbf` legacy, `0x9f` Talisman.
- Debug hooks are `beforeOp` / `afterOp`, still behind `-DGM_TRACE`.

Left alone on purpose:

- The slow-draw policies: after the panel store each is a row lookup, an
  adjacency test and `skipsReveal`; a template over the plane count would be
  longer than the two structs.
- The two reset functions (background byte and plane count) and the CGA/PCjr
  page shells (pixel extraction and palette differ in `blit_page`).
- The hourglass arm state stays in `graphics_magician.cpp`, so the DHGR file
  links against it; moving it would only change the link line.
- Line, glyph and brush pixel work on Apple vs DHGR: different 6502 routines,
  and the Talisman lamp-knob fill depends on the DHGR circle (the circle
  stepper is shared, see above; the DHGR plot and half step are not).
- The DHGR brush puts `+8` upper-right and `+16` lower-left, CGA/PCjr the
  reverse. Column-major DOS data; not a bug.
- Flood fills: the CGA word-queue port, the PCjr visited-mask fill and the
  Apple/DHGR span fill stay apart (see above and B5 in the comments).
- The Scott Adams copy in `terps/scott/saga/apple2_vector_draw.c` decodes a
  3-bit opcode (`& 0xe0`, real pictures carry `0x5x` meaning SET_SHAPE), uses
  a midpoint scanline fill and its own slow-draw op list, in C. It already
  shares the cursor, line and brush primitives through `common_sagadraw`;
  the dispatcher would not fit it.
- `OPCODE_OP7` keeps its name; the enum comment in `graphics_magician.h`
  lists the three meanings.

## Declined, with the reason

- One renderer for `graphics_magician.cpp` and `graphics_magician_dhgr.cpp`:
  the aux/main interleave, 7 vs 14 pixel columns and the single-pixel-span
  rule are hardware differences. Share the algorithms, not the file.
- Merging the CGA 2-bpp phase-walk fill with the PCjr 4-bpp fill: they
  genuinely diverge (`graphics_magician_vector.h` says so).
- A generic `gmvFindSignature`: the other executable scans each carry their
  own bounds logic, so only the brush-table search is shared.
- One `runFixtures(renderFn, dir)` test driver: the drivers differ in table
  loading and case shape; the shared part is `test/pic_fixture_test.h`.
- Sharing `comprehend_compat.h` with `archetype_compat.h`: low value for one
  other consumer. It was pruned of unused members instead (698 to 612 lines).
- A common RNG state serializer with scarier: only the 17-byte packing is
  local, on the shared `erkyrath_random_get/set_detstate` accessors already.
