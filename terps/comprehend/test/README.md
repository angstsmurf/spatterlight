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
- **The Coveted Mirror panel share is conservative**: `cm_panel.h` holds the
  pile-grain walk and the grain-fall state machine; page capture, overlay and
  rebuild stay per renderer because one page and aux+main differ for real. The
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

## Declined, with the reason

- One renderer for `graphics_magician.cpp` and `graphics_magician_dhgr.cpp`:
  the aux/main interleave, 7 vs 14 pixel columns and the single-pixel-span
  rule are hardware differences. Share the algorithms, not the file.
- Merging the CGA 2-bpp phase-walk fill with the PCjr 4-bpp fill: they
  genuinely diverge (`graphics_magician_vector.h` says so).
- A templated CGA/PCjr op-walker: about eight hooks to share about forty
  lines. Only `gmvMirrorByte()` is shared.
- One `runFixtures(renderFn, dir)` test driver: the drivers differ in table
  loading and case shape; the shared part is `test/pic_fixture_test.h`.
- Sharing `comprehend_compat.h` with `archetype_compat.h`: low value for one
  other consumer. It was pruned of unused members instead (698 to 612 lines).
- A common RNG state serializer with scarier: only the 17-byte packing is
  local, on the shared `erkyrath_random_get/set_detstate` accessors already.
