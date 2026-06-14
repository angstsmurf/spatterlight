# Coveted Mirror double hi-res (`<D>`) fixtures

Ground truth + inputs for `test/cmdhgrtest` (the op15/0 DHGR regression).

| file | what |
|------|------|
| `throne_main.page` | MAME DHGR main page1 (`$2000`) of the throne room, 8 KB |
| `throne_aux.page`  | MAME DHGR aux  page1 (`$2000`) of the throne room, 8 KB |
| `throne_rb_idx1.bin` | the throne's room image stream (side A `RB`, image 1, image-offset..EOF) |
| `T5.bin` | the boot disk's `<D>` drawing tables (side B `T5`) |

## How CM `<D>` room art actually works (resolved 2026-06-14)

CM's room pictures are the **standard Graphics Magician vector streams in the
room files** (`RA`..`RG`) -- *not* separately-authored DHGR art and *not* in the
`G`-files (those hold the engine's pointer tables / text, a red herring). The
throne room is `RB` image 1: `op15/2` (set bounds), `op15/3` (fill background),
`op15/0` (RLE-compressed bitmap), then vector primitives.

`<D>` mode runs the *same* streams but rasterises every primitive onto the
560-wide DHGR aux+main pages. The one piece the port was missing is **op15/0**:
the 6502 `<D>` routine (`$0a7b`, traced live in MAME) reads 4 bounds bytes then
streams raw page bytes column-major (top->bottom, then next column; `0x80`
escapes a `count,value` run written `count+1` times), and splits each source
byte through the nibble-doubling table `DHGR_DBL` into a main + aux byte stored
at the same page offset (logical DHGR columns `2*col` / `2*col+1`). Before the
fix `gmDhgrDrawImage` ignored op15 par 0 *and consumed no operand bytes*, so the
bounds+RLE stream was misparsed as opcodes and the picture went all white.

The test compares page byte offsets `0..20` of rows `0..159`. Offsets `21..23`
are the picture/panel boundary: the captured page has CM's right-hand panel
composited over them (drawn on a separate pass), so they are excluded.

Recapture recipe: `apple2e -flop1 ".../side B (boot).woz"`, press `D`, space past
the title to the "turn diskette" prompt, swap to side A
(`images[":sl6:diskiing:0:525"]:load(sideA)`), space -> throne draws; dump
`$2000` for main, then PAGE2-on for aux.
