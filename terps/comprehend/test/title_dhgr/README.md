# Talisman double hi-res (`<D>`) title fixtures

Ground truth + inputs for `test/titledhgrtest` (the DHGR title regression).

All fixtures (the `.page` MAME goldens and the `.bin` inputs — all derived from
the copyrighted game) are kept local-only in `fixtures/`, gitignored;
`titledhgrtest` reads them from there (its default fixture dir is
`test/title_dhgr/fixtures`). When the fixtures are absent the test prints `SKIP`
and passes, so `make test` still works on a fresh checkout.

| file | what |
|------|------|
| `fixtures/title_main.page` | MAME DHGR main page1 (`$2000`) of the rendered title, 8 KB |
| `fixtures/title_aux.page`  | MAME DHGR aux  page1 (`$2000`) of the rendered title, 8 KB |
| `fixtures/title_t0.bin`    | the title image stream (boot disk `t0`, a raw Graphics Magician vector stream from byte 0) |
| `fixtures/T5.bin`          | the boot disk's `<D>` drawing tables (boot disk `T5`) |

## What it guards

The title is the cleanest exercise of the three `<D>` primitives the renderer was
missing; the test asserts the whole picture (page cols `0..39`, rows `0..159`, both
pages, bit 7 masked) is byte-exact:

- **op1/op3/op5 text glyphs** — the "Challenging the Sands of Time" subtitle. The
  picture font shares the brush-table base: `glyph(ch) = T5[0xB2E + ch*8]`. op1
  stores the (doubled) text cursor; op3 XORs the glyph (solid text), op5 blends it
  with the fill pattern; both advance the cursor by 14.
- **op11 DRAW_CIRCLE** — the lamp-knob circle (`DRAW_CIRCLE r=4 @135,90`). While it
  was a no-op the outline had a 1-cell gap and the following `fill=62` PAINT
  (`@145,96`) leaked out of the lamp and flooded the whole sky (grey hatch instead
  of lavender). The port mirrors the 6502 routine (`$0aeb`): centre = pen position,
  x extents `±2r`, x stepped at double resolution so the curve is round and gap-free.
- **the full 512-byte op6 sub-table** — a fill colour's subindex can be up to 255,
  so the builder reads `subtbl[2*idx{,+1}]` up to `510/511`. Loading only 500 bytes
  left those zero, so high-subindex fills (the lamp body, `fill 60 -> idx 255 ->
  subtbl[510]=0x0a`) resolved to pattern row 0 = black instead of their grey dither.

## Recapture recipe

MAME `apple2e`, launch with **no** disk, wait for the `Apple ][` prompt, then mount
the boot WOZ into a running machine (the woz-a-day copy protection needs this) and
boot:

```lua
manager.machine.images[":sl6:diskiing:0:525"]:load(".../Talisman ... side 1 - Boot.woz")
```
type `PR#6`, answer `D` (double hi-res), wait for the title to finish drawing.

Inputs: extract `t0` and `T5` from the boot disk with
`dhgrtrace <bootwoz> <bootwoz> extract t0 fixtures/title_t0.bin` (and `T5`).

Goldens: read `$2000` for **main** (`$C054` PAGE2 off), then **aux** with PAGE2 on
(`$C055`) — RAMRD/`$C003` does NOT switch the `$2000-$3FFF` hi-res region when
80STORE is on, so it silently returns main. Dump each 8 KB page:
`dbg:command("dump main.txt,0x2000,0x2000")` and convert the hex dump to a raw
`.page` (offsets relative to `$2000`).
