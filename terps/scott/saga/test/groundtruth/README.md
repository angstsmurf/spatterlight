# Apple II SAGA room-image ground-truth pages

Golden `$2000`-byte Apple II hi-res page-1 dumps (`$2000..$3FFF`, 8192 bytes each)
of every room image in the four Scott Adams vector-graphics SAGA games, captured
from the **real interpreter** running under MAME (Apple //e). Used to regression-test
`apple2_vector_draw.c` byte-for-byte.

| dir             | game                | rooms `R*` | objects `B*` |
|-----------------|---------------------|-----------:|-------------:|
| `adventureland/`| Adventureland (#1)  | 48 | 45 |
| `pirate/`       | Pirate Adventure (#2)| 39 | 49 |
| `mission/`      | Mission Impossible (#3)| 33 | 30 |
| `strange/`      | Strange Odyssey (#6)| 39 | 32 |

Each file is named after its on-disk DOS 3.3 image file. Room pictures are
`R01nn`..`R06nn` (last two digits = room index; `R0111` = Adventureland room 11, the
opening forest). Object overlays are `Bggnnn` (`gg` = game id, `nnn` = object index).
Pages are the raw 8192-byte hi-res page; rows 0..159 hold the picture, rows 160..191
the text overlay (drawn separately by the Glk port).

The 5-character `B0100` file on each disk is a non-image marker (skipped by the loader)
and is not included.

### A note on the `B*` object overlays
In play, an object overlay is drawn over the current room *without* clearing the
screen, so its flood-fills are bounded by the room's existing lines. These fixtures
render each object in isolation (`DRAW_OPCODE` clears to white first), so fill-based
objects spill their fill across the page and look odd — but the result is fully
deterministic and valid for byte-for-byte renderer regression (the renderer, fed the
same object in isolation, produces the same page).

## How they were captured

The four games store each picture as a separate DOS file containing a
Graphics-Magician (Penguin Software) vector-opcode stream, drawn by the interpreter's
`DRAW_OPCODE` routine (`$8E30` in Adventureland). The drawing code and pattern/brush
tables are identical across all four games, so a single running Adventureland instance
renders every game's images.

Capture harness (driven over the MAME MCP bridge):

1. Boot Adventureland (Apple //e, Disk II slot 6) to the opening room so the real game
   establishes canonical interpreter state; snapshot zero page (`$00..$FF`) and the
   draw-state region (`$8E00..$96FF`).
2. Install a small 6502 stub (interrupts disabled — `SEI`) that polls a flag and calls
   `JSR $8E30`; run it free.
3. Per image: restore the canonical state snapshot, write the file's vector stream
   (after its 4-byte BLOAD header) to a scratch buffer (`$7F50`; `$4000` for images
   >~3.7 KB), point the draw pointer `$8E00/$8E01` at it, raise the flag, wait for the
   draw to finish (these games use deliberately slow fills — some, e.g. Mission's death
   image, take many seconds; isolated object overlays whose fills spill are also slow),
   then dump `$2000..$3FFF`. Drive only one capture at a time: a second overlapping
   `exec_lua` interleaves with the first via its `emu.wait` yields and corrupts the
   shared draw state.

Restoring canonical state before each draw is required: `DRAW_OPCODE` alone does not
re-init a persistent palette/colour-phase byte (`$A9`) that the game's per-image wrapper
resets, so without it fills come out bit-complemented or flood indefinitely.

### Validation
* `adventureland/R0111.page` is byte-identical to the forest page the real game draws
  on its own during play (verified in-emulator).
* Identical pages across rooms correspond either to identical raw vector data on disk
  (rooms reusing one picture) or to intentionally blank images — Strange Odyssey's
  all-white "snowstorm" rooms (`R0618/R0619/R0620/R0691`, etc.) and teleport-animation
  frames.
