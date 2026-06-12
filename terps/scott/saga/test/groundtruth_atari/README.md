# Atari 8-bit SAGA vector-image ground truth

Golden dumps of the two screen planes (`$9000` and `$A000` on real hardware,
`A8_IMAGE_SIZE` = `0xf00` = 3840 bytes each) for every room and object image in the
four Atari 8-bit Scott Adams vector-graphics SAGA games, captured from the **real
interpreter** running under MAME (`a400`, 48K). Used to regression-test
`atari_8bit_vector_draw.c` byte-for-byte (`make groundtruth`).

| dir             | game                | rooms `ROOM_IMG_*` | objects `OBJ_IMG_*` |
|-----------------|---------------------|-------------------:|--------------------:|
| `adventureland/`| Adventureland (#1)  | 49 | 45 |
| `pirate/`       | Pirate Adventure (#2)| 40 | 49 |
| `mission/`      | Mission Impossible (#3)| 34 | 30 |
| `strange/`      | Strange Odyssey (#6)| 39 | 32 |

Each image is three files:
* `<name>.dat` — the raw Graphics-Magician-style vector opcode stream (3 bytes/opcode,
  `opcode = byte >> 5`), exactly as `atari8detect.c` extracts it and feeds it to the
  C renderer. `ROOM_IMG_nn` is room index *nn*; `OBJ_IMG_nn` is object index *nn*
  (`255` = title). Streams are truncated at their first opcode-0 terminator (the over-long
  title streams otherwise run to end-of-disk); this is render-neutral since both the real
  interpreter and the C renderer stop at the terminator.
* `<name>.90` / `<name>.a0` — the golden `$9000` / `$A000` plane contents (0xf00 bytes).

The renderer combines the two planes (A0 bits as the high pair, 90 bits as the low pair)
into the 16-colour display; both planes must match for an image to pass.

## How they were captured

All four games share one drawing engine, so a single running Adventureland instance
renders every game's streams. The interpreter's vector core is `MAIN_DRAW` at `$8582`
(see the `Atari8Vectors` Ghidra project): it reads the opcode stream from a fixed buffer
at **`$B000`** and draws directly into `$9000`/`$A000` (plane-base pointers at
`$8544/$8546`, fill end `$9F00`).

Capture harness (driven over the MAME MCP bridge, `mame_exec_lua`):

1. Boot Adventureland to the first room so interpreter state is canonical.
2. Patch out the DELAY opcode (`$86D6` ← `JMP $866A`) — a pure busy-loop that never
   touches pixels but would otherwise cost many seconds of emulated time per image.
3. Install a persistent dispatcher in page 6 (`$0600`): `SEI`, `NMIEN=0`, then loop
   polling a GO flag; on GO it `JSR $8582` and sets a DONE flag. PC is set **once**.
   (Re-setting PC per image is unreliable against the 6502 prefetch — the cause of
   early non-deterministic blank captures — so the GO/DONE flag protocol is used
   instead.)
4. Per image: clear both planes to 0, write the `.dat` stream to `$B000`, zero the
   per-image state the C renderer's `ctx = {}` assumes (`COLOR_A_IDX $853B`,
   `COLOR_B_IDX $853C`, `PREV_X $8537`, `PREV_Y $8538`, `ERASE_MODE $7E98`), raise GO,
   wait for DONE, then dump `$9000` and `$A000`.

Clearing the planes to 0 before each draw matches the C renderer's `IMG_ROOM` path
(`write_to_screenmem(0,0,0,true)`), so the comparison is apples-to-apples for both room
and object streams.

### Known divergences (3, ignored by the harness)
`adventureland/OBJ_IMG_26` (bees in a bottle), `OBJ_IMG_27` (sleeping dragon) and
`OBJ_IMG_28` (flint & steel) contain a flood fill that, rendered **in isolation** on a
cleared screen, leaks across the whole screen on the real Atari (golden fully filled),
whereas the C `flood_fill` stays contained. In actual play these objects are always
composited over a room whose lines bound the fill, so the divergence never occurs in the
game. They are listed in `is_known_divergent()` in `filltest.c` and excluded from the
pass count; revisit if the C flood-fill spill behaviour is ever made bit-faithful for
unbounded regions.
