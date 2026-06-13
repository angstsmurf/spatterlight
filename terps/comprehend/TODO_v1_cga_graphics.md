# v1 Comprehend DOS (CGA) graphics

Crimson Crown and Transylvania v1 render through `graphics_magician_cga.cpp` (the v2 renderer)
via `gmcgaInstallV1DrawingTables` — fill pattern + subindex tables from `PC_GRAPH.OVR`, brushes
from `NOVEL.EXE`, v1's 30×4 phase-byte fill format. Validated pixel-exact (`make test`,
`test/test_gmcgav1_pics`: Crimson Crown lakeshore + woods). Full RE record: memory
`comprehend-v1-cga-graphics`.

## Remaining follow-ups
- [ ] Pixel-lock more fixtures: more CC rooms, the object (`OA`/`OB`) and title images, and at
      least one **Transylvania v1** room (it renders correctly today but isn't in the regression
      set). Capture with `test/gmcgav1/dosbox_capture_v1.py`.
- [ ] v1 in-picture text comes from `CHARSET.GDA` (the CharSet path), not the embedded font —
      exercise if any v1 picture draws op3/op5 text.
