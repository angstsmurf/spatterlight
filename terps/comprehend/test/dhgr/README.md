# Double hi-res renderer fixtures

`fixtures/` is local-only (gitignored): every file derives from the copyrighted
game disks of *Talisman -- Challenging the Sands of Time* (woz-a-day collection).

| file | contents |
|------|----------|
| `T5` | the boot disk's double hi-res interpreter (3994 bytes). It loads into AUX RAM at `$4000`; the renderer's colour subindex (`$41B2`), sub-table (`$421E`), dither patterns (`$441E`), brushes (`$4B2E`) and picture font (`$4C2E`) are read from it at runtime by `gmDhgrInstallDrawingTables()`. The Coveted Mirror's T5 is byte-identical. |
| `RA` | room picture file of the Empire disk (16 pictures, LE16 offset table first) |
| `RF`, `RG` | room picture files of the Lands Beyond disk (part 2) |
| `cell_main.bin`, `cell_aux.bin` | main and aux `$2000..$3FFF` pages of RA 4, the prison cell, captured from MAME |
| `lb_<file><idx>_main.bin`, `..._aux.bin` | the same for Lands Beyond pictures: RF 3 and RG 2, 6, 10, 11, 12 |

`test_dhgr_pics` renders each case and asserts the pages are byte-exact over the
picture rows (0..0x9f); `test_dhgr_determinism` renders every RA picture after
every other one. Both run from `make -C terps/comprehend test`.

The Lands Beyond cases are the pictures that re-patch the colour their current
fill pattern was built from (op7 `PALSUB` after `FILLCOL` of the same colour),
which the interpreter answers by rebuilding the pattern at once (`$0951`:
`cpx $0837 / bne / jmp $0d1a`); RG 10 does it twelve times. RF 3 ends in forty
tiny brush-1 dots.

## Capturing a page pair from MAME

MAME `apple2e`. The woz-a-day Talisman disks drop to `ERR` if booted from the
drive at power-on: launch with no disk, mount the boot side into
`:sl6:diskiing:0:525` from Lua, then `PR#6`. Answer `D` to the standard/double
hi-res question. Save a load-safe state only after the drive has been idle for
several seconds (a state saved mid-flux freezes the Lua bridge on load).

Any picture can be rendered without playing to it. Part 2's game database sits at
`$5a00`: the room byte is `$5a3d` and the room graphics table starts at `$5f1e`,
one byte per room; picture numbers 65..80 are RE 0..15, 81..96 RF 0..15, 97..112
RG 0..15. With the Lands Beyond side mounted:

```lua
local m = manager.machine; local sp = m.devices[":maincpu"].spaces["program"]
sp:write_u8(0x5a3d, 54)        -- room 54, open desert: no items to overlay
sp:write_u8(0x5f53, 97 + 10)   -- room 54's picture = RG 10 ($5f1e + 54 - 1)
m.video.throttled = false
m.natkeyboard:post("\n"); emu.wait(3)   -- Return toggles text/picture ...
m.natkeyboard:post("\n"); emu.wait(8)   -- ... and the picture is redrawn
m.video.throttled = true
```

Wait for the whole picture: a long stream takes several seconds even
unthrottled, and a page captured before its last ops have run looks like a
renderer bug (RF 3's closing brush dots were missing from a first capture of it).
A logpoint on the brush routine `$105a` or on `$0888` (picture init) shows when
the draw has ended; capturing twice and comparing also works.

Then dump the pages. With 80STORE on, `$2000..$3FFF` follows PAGE2, so the aux
page is read with `$C055`, not RAMRD:

```lua
local d = m.debugger
sp:write_u8(0xC001, 0); sp:write_u8(0xC054, 0)
d:command("save /path/lb_RG10_main.bin,0x2000,0x2000")
sp:write_u8(0xC055, 0)
d:command("save /path/lb_RG10_aux.bin,0x2000,0x2000")
sp:write_u8(0xC054, 0)
```

(The debugger's `save` takes its length in hex.) The raw picture files come off
the disk with `dhgrtrace`:

    make -C terps/comprehend test/dhgrtrace
    test/dhgrtrace <lands-beyond.woz> <boot.woz> extract RG test/dhgr/fixtures/RG

`dhgrtrace ... disasm RG 10` lists the picture's ops, and a build with
`-DDHGR_WATCH` plus `WATCHOFF=<page offset hex> WATCHPAGE=1|0` (main|aux)
reports every op that writes one page byte, which is how a differing byte is
traced to its op. `pixtest` renders a stream through the same renderer with
`PIXTEST_DHGR_MAIN=<out> PIXTEST_DHGR_AUX=<out>`.
