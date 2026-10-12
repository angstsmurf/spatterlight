# PCjr 16-colour renderer harness (graphics_magician_pcjr.cpp)

`pcjrtest` renders one Graphics Magician picture through the IBM PCjr / Tandy
16-colour renderer and writes a 280×160 PPM, for eyeballing the colours and
geometry. Self-contained (no Glk/disk deps).

```
make test/gmpcjr/pcjrtest
./test/gmpcjr/pcjrtest <gamedir> RA.MS1 0  out.ppm   # room file, image index
./test/gmpcjr/pcjrtest <gamedir> RA.MS1 1  out.ppm
./test/gmpcjr/pcjrtest <gamedir> TRTITLE.MS1 -1 title.ppm   # -1 = single-image title file
```

`<gamedir>` is the IBM PC/PCjr release directory containing `JR_GRAPH.OVR`,
`NOVEL1.EXE` (or `NOVEL.EXE`) and `CHARSET.GDA` (e.g. the Transylvania v1 or
Crimson Crown folders).

## Regression test

`test/test_gmpcjr_pics` (part of `make test`) renders every picture of both
PCjr-capable releases, 331 in all, and compares a hash of each against the
committed `hashes.txt`. That part is a change detector, not a correctness proof:
a failure means the output moved, and the PPM from `pcjrtest` is how to judge
whether it moved for the better. It then compares the pictures that have an
emulator capture in `fixtures/captures/` (see "Ground truth" at the end) pixel
for pixel; those must match exactly. A capture is named after what it shows,
`<game-folder>.<FILE>.<EXT>.<index>.ppm`, and one of a room with objects in it
names them all in drawing order
(`transylvania.RA.MS1.6+OA.MS1.0+OB.MS1.7.ppm`).

It reads the game folders themselves, which are local-only: put (or symlink) them
at `test/gmpcjr/fixtures/transylvania/` and `test/gmpcjr/fixtures/crimson-crown/`,
or pass another directory holding those two folders. A missing game is skipped
with a message and the test still passes.

```
./test/test_gmpcjr_pics [games-dir]           # check
./test/test_gmpcjr_pics --bless [games-dir]   # rewrite hashes.txt after an intended change
```

Each hash is FNV-1a over the pixel payload of the PPM `pcjrtest` writes for the
same picture, so a single failing line can be reproduced and looked at directly.

## Pixel model (reverse-engineered from JR_GRAPH.OVR + NOVEL.EXE in Ghidra)

The original NOVEL.EXE checks the BIOS model byte (F000:FFFE == 0xFD) and, on a
PCjr, loads `JR_GRAPH.OVR` instead of `PC_GRAPH.OVR` and drives BIOS video
mode 9 (320×200, 16 colours). Key facts:

* **Mode 9, 4 bits/pixel, 2 pixels/byte**; the leftmost pixel of a byte is its
  high nibble. VRAM offset = `(y&3)*0x2000 + 160*(y>>2) + ((x+20)>>1)`
  (4-bank interleave; `jr_calc_row_base` @ OVR 0x4d, `jr_calc_column_nibble`
  @ 0x85). Picture window is +20 px, so the logical picture is the same 280×160
  as the CGA/Apple renderers.
* **Pen plot** (`jr_plot_pixel` @ 0x0e): pen index `p = pen & 7` selects a colour
  nibble from `colLow[]`/`colHigh[]` (OVR 0xafe/0xb06) OR'd into the target
  nibble. White (entries 3 and 7) carries `0xff` in `colHigh`, so it whitens the
  adjacent low nibble too — a faithful one-pixel bleed.
* **Fills**: 30 × 4-byte 16-colour dither tiles (OVR 0xb29) selected per op6
  byte through a 108-entry subindex (OVR 0xba1; one pattern index per row
  parity), exactly like the v1 CGA path.
* **Brushes** come from NOVEL.EXE (same brush-5 signature the CGA loader uses);
  the in-picture **font** comes from `CHARSET.GDA`. Both are mode-independent.
* **Palette**: the standard PCjr/CGA 16-colour RGBI palette (no custom palette
  is programmed).

The tables are located by signature in `gmpcjrInstallDrawingTables()` (the
`00 20 0f f0` = bankStride/masks anchor), so they resolve in both the
Transylvania v1 and Crimson Crown overlays.

## In the game

The renderer is opt-in via the `#pcjr [on|off]` debug command (mirroring
`#dhgr`), available whenever `JR_GRAPH.OVR` loaded. `#pcjr on` routes pictures
through the 16-colour renderer; `#pcjr off` restores the 4-colour CGA renderer.

## Ground truth

DOSBox-X runs the game's 16-colour path: the stock NOVEL1.EXE loads
`JR_GRAPH.OVR` when the BIOS model byte is 0xFD, which `machine=pcjr` provides
(linear 0xFFFFE reads 0xFD). The game folder must be mounted as A:
(`mount a: <folder> -t floppy`); NOVEL.EXE reads the disk through INT 13h and
does not boot from a directory. The picture window is the 280x160 region at
(20,0) of the 320x200 frame. The game starts in its all-text view; a bare
Return switches to the picture view and back.

Screenshots taken that way, cropped to P6 PPMs in `fixtures/captures/`
(local-only), are checked by `test_gmpcjr_pics`. 73 so far, all pixel-exact:

| game | captured | exact |
|---|---|---|
| Transylvania | title; all 39 room pictures, 36 bare and 8 with objects on them | 45 of 45 |
| Crimson Crown, disk 1 | title; 27 of the 32 rooms walked into, 4 of them with an object | 28 of 28 |

Two renderer bugs came out of them, both in the fill tiles:

* **Row parity.** The subindex pair is (odd-row tile, even-row tile), the
  reverse of the CGA overlay. The Crimson Crown title settled it: its crown has
  two-tile brush stamps, and the CGA way round got 50 pixels wrong.
* **Word fill.** The flood fill tests and paints a byte pair at a time and only
  ever uses bytes 0 and 2 of the 4-byte tile; brushes and glyphs use all four.
  Treating the fill like the brush put the wrong dither in most Transylvania
  rooms.

Not captured:

* Five Transylvania screens with objects (`RA.10+OC.3`, `RB.12+OB.14`,
  `RB.8+OB.0`, `RB.9+OA.9`, `RC.4+OA.13`): the game could not be put in the
  state that draws them without playing it there. The rooms themselves are
  covered.
* Five Crimson Crown rooms (4, 6, 15, 27, 28): walking in from the neighbour the
  exit table names landed in a different room, so the shots show other pictures.
* Crimson Crown disk 2 (`CC2.GDA`, the `.MS2` files): not attempted.

### Taking more

`dosbox_capture_pcjr.py tr|cc` does a whole game: it asks the headless
interpreter for every room's exits and pictures (the `#room [n]` debug command,
headless build only), boots the game in DOSBox-X, shows each room by poking the
current-room byte and making the game redraw, and saves the picture window
under the name of the pictures the interpreter draws there. `test_gmpcjr_pics`
then keeps the shots it reproduces and the rest go to
`fixtures/captures_rejected/`. A rejected shot is a question, not a verdict:
check first whether the game was in the state assumed.

The current-room byte is at linear 0xD999 in Transylvania and 0xE7B9 in Crimson
Crown (folder mounted as A:, `machine=pcjr`, `cycles=20000`). Two ways of
redrawing, neither complete:

* poke the room and type LOOK: no route needed, but most objects are not drawn,
  and Crimson Crown has no bare LOOK;
* poke a neighbouring room and type the direction leading in: objects are
  drawn, but only for rooms something leads into.

To see what the interpreter draws for the same screen, run the headless build
with `COMPREHEND_GFX=0` (PCjr) and `COMPREHEND_PICDUMP=<dir>`: every frame it
blits is written there as a PPM.

Things that do not work: toggling the picture off and on with Return to force a
redraw (the second switch to the picture view shows garbage until the game next
draws a room), and DOSBox-X savestates (loading one does not bring the game
back). Crimson Crown now and then drops to the all-text view on its own, and
draws slowly: wait for the screen to stop changing before taking the shot.
