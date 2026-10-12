# v1 DOS CGA renderer: fixtures and reverse-engineering tools

The v1 DOS releases (Crimson Crown, Transylvania v1) draw with the same CGA
renderer as the v2 games (`graphics_magician_cga.cpp`) but load their tables
differently: fills from `PC_GRAPH.OVR`, brushes from `NOVEL.EXE`, the in-picture
font from `CHARSET.GDA`. `make test` checks this path with
`test/test_gmcgav1_pics.cpp` against the goldens in `fixtures/`.

The fixtures (`fixtures/*.img`, `*.fb`, `*.bin`) and the screenshots in `images/`
derive from copyrighted games and are kept local-only (gitignored).

`.fb` here is a raw 16 KB CGA VRAM dump (`0xB8000`, two interlaced 2-bit banks);
the tests unpack it to 320x200 indices and compare the 280x160 picture window at
(20, 0). (The v2 fixtures in `test/gmcga/` use an already-unpacked 64000-byte
`.fb` instead.)

## Tools

Everything here except `dosbox_capture_fixtures.py` is a one-shot tool written to
answer one question during the reverse engineering. They are kept because the
answers are cited in source comments and re-running them is the only way to
re-check those claims. All the DOSBox ones drive
dosbox-x-remotedebug (DOSBox-X with a GDB stub and a QMP socket), and hard-code local paths to it and to the game folders
at the top of the file.

| script | what question it answered | output |
|---|---|---|
| `dosbox_capture_fixtures.py` | **The one to re-run.** Regenerates the `make test` goldens that are not plain rooms: the title at boot, then the text-cutscene and object-over-room pictures via the native room/object entry points (`MODE=cc`, `tr` or `trmerged`). | `$V1OUT/*.fb` (default `/tmp/v1fix`), to copy into `fixtures/` |
| `dosbox_capture_v1.py [cc\|tr]` | What do the first few rooms look like when reached by actually playing? (Crimson Crown: lakeshore, woods, crypt, cave; Transylvania: the start room.) The original ground truth, before the entry points were known. | `fixtures/<room>.fb` |
| `dosbox_dump_all_v1.py` | Can every picture in a v1 game be dumped by calling the picture loader directly? Parked: calibration works, the sweep does not (see its header). | `<V1OUT>/manifest.tsv` + `.fb` |
| `validate_dump.py` | Does the renderer match every picture in such a dump? Renders each manifest row with `gmcgav1test` and counts mismatches. | per-picture counts |
| `dosbox_capture_op14_v1.py` | What is on screen before each op14 flood fill of one picture in the native interpreter? | per-fill VRAM dumps |
| `compare_op14.py` | Which flood fill is the first to differ from the renderer? Pairs those dumps with the renderer's `GMCGA_TRACE_DIR` snapshots (needs a `-DGM_TRACE` build, e.g. `test/gmcgav1test`). Answer: none; every fill is exact. | first diverging fill |
| `find_magenta_op.py` | Which op paints the crypt's stray magenta pixel at (188,25), given the fills are exact? Answer: an op12 brush. | per-hit effect on the byte |
| `trace_blit_row.py` | How does the brush blitter walk its dither phase on that row? Answer: per painted byte, not per column (now in `blit_col`). | ordered store trace |
| `trace_crypt.cpp` | Renderer-side counterpart of the two above: op-by-op trace of the crypt picture. | stdout |

`test/gmcga/README.md` describes the v2 (Talisman) tools the same way.
