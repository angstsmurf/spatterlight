# Magnetic — TODO

Two related projects:

1. Running games straight from the original releases (disk images or a DOS
   install folder) instead of from a pre-built `.mag` + `.gfx`.
2. Supporting more of the Magnetic Windows (MW) front end: the item windows
   ("visual inventory"), automap, compass, clickable pictures and music.

Facts marked *(verified 2026-10-20)* were checked against the Wonderland
releases (DOS 1.21, Amiga 1.27f, Atari ST) while converting them by hand. Facts
marked *(unverified)* are guesses to be checked first.

---

## Part 1 — Running games from original media

### Where we are

- Spatterlight only accepts `.mag` (`application/Constants.m` `gGameFileTypes`).
  `babel/magscrolls.c` claims a file only if it starts with `MaSc`, and maps it
  to the `magnetic` terp (`gFormatMap`).
- `Glk/glk.c` `gms_establish_filenames()` (~5840) looks next to the `.mag` for
  `X.gfx`/`X.GFX` and `X.hnt`/`X.HNT`. It always passes `NULL` for the sound
  file (~6172).
- `Generic/emu.c` `ms_init()` (662) `fopen()`s each of the four files by name.
  There is no in-memory entry point.
- `Generic/` contains the original stand-alone converters. None of them is
  built into the terp:
  - `xtractpc.c` — original MS-DOS releases (Pawn, Guild, Jinxter, Corruption,
    Fish, Myth).
  - `xtract64.c` + `myth.c` — C64 `.d64`.
  - `xtractmw.c` — MW DOS `.RSC`. It identifies the game by directory entry
    count (Wonderland = 1031) and hard-coded section sizes, and opens the paths
    written in the `.RDF`. Those are absolute DOS paths
    (`c:\wonder\TWO.RSC`), so it fails on a real install unless the `.RDF` is
    rewritten.
  - `gfxlink.c` — Amiga picture files → `.gfx` (not for MW games).
  - `gfxlink2.c` — MW "type 11" pictures → MaP2 `.gfx`. It only recognises two
    resource-file IDs (`0xA34F0026` Wonderland, `0x7D140021` Collection).
- **dMagnetic 0.36** (BSD-2, `~/Downloads/dMagnetic_0.36/src/loader/`) already
  loads most original formats into memory. It produces its own `.mag`/`.gfx`
  variants (MaP3/MaP4, gfx4/gfx5 decoders), not Magnetic's MaP2:

  | dMagnetic loader | Input |
  |---|---|
  | `loader_mw.c` | MW DOS `TWO.RSC` + siblings (Wonderland, Collection) |
  | `loader_msdos.c` | original DOS releases |
  | `loader_d64.c` | C64 `.d64` |
  | `loader_dsk.c` | Amstrad CPC and Spectrum +3 `.dsk` |
  | `loader_archimedes.c` | Archimedes `.adf` |
  | `loader_atarixl.c` | Atari XL `.atr` |
  | `loader_appleii.c` | Apple II |

  It has **no Amiga or Atari ST loader**.

### Design

- [ ] **In-memory init.** Add `ms_init_mem(mag, mag_len, gfx, gfx_len, hnt,
      hnt_len, snd, snd_len)` next to `ms_init()`. Factor the parsing out of
      `ms_init()` so the two share it.
  - `ms_init(0,…)` (restart) must still work.
  - `init_gfx1/2` and `init_snd` currently read from `FILE *`. Give them
    buffer readers.
  - Keep `ms_init()` for the existing `.mag` path.
- [ ] **Loader layer** `Generic/loaders/` (new). Each loader takes a path and
      returns `{mag, gfx, hnt, snd}` buffers in the formats `emu.c` already
      understands: `MaSc` story, MaP1/MaP2 graphics.
  - Emitting MaP2 means the emulator, animation code and Glk port stay as they
    are.
  - Port dMagnetic's loaders but convert their output to MaP2 rather than
    importing MaP3/MaP4. `emu.c` would otherwise need a second picture path.
  - Keep dMagnetic's copyright notice (BSD-2).
- [ ] **Game identification** in `babel/magscrolls.c`.
  - Today it is header-only. It needs to claim:
    - disk images by content: `.d64`, `.dsk`, `.adf`, `.st`, `.msa`, `.atr`;
    - a DOS folder by its `.RSC`/`.RDF` files.
  - It must return the existing IFIDs (`MAGNETIC-1` … `MAGNETIC-7`) so library
    entries and autosaves match `.mag` imports of the same game.
  - Shares extensions with `scott`, `taylor`, `sagaplus`, `level9` and
    `comprehend`. Add `magscrolls` to the candidate sets in
    `GameImporter.m` (~364). Claim only on a positive content match, never on
    the extension alone.
- [ ] **Which file is "the game"** for a multi-file release.
  - MW DOS: import `TWO.RSC`, as dMagnetic does (`loader_mw_substituteTwoRsc`
    finds the siblings).
  - Multi-disk images: import disk 1 and find the others by name pattern. See
    `terps/plus/companionfile.c` for how Plus finds companion files.
  - Sandbox: siblings must be readable. `FolderAccess` already grants the
    folder on import. Check that autorestore re-opens the siblings.
- [ ] **Cache, don't re-extract on every launch?** Extraction is fast
      (milliseconds), so probably not needed. Decide after measuring the MW
      Huffman decode of ~120 pictures.

### Per platform

- [ ] **MW DOS** (Wonderland 1.21; Collection: Corruption 1.12, Fish 1.10,
      Guild 1.3).
  - Story: port `xtractmw.c`'s section table. Resolve `.RDF` entries by
    basename, case-insensitively, next to the `.RDF`, and ignore the
    `c:\wonder\` prefix.
  - Pictures, verified 2026-10-20:
    - The DOS `.RSC` set has 226 type-6 (Huffman bitstream) and 226 type-7
      (609-byte tree) entries. The Amiga has 121 of each, so DOS probably holds
      EGA + VGA variants. dMagnetic's `gfxloader_gfx4` picks between them with
      an `egamode` flag.
    - Decode with the same Huffman/RLE/XOR scheme as the scratch converter,
      then write MaP2 planar records with the animation tail.
  - `USER.RSC` (the installed copy) holds 121 **type-11** pictures, verified
    2026-10-20.
    - The record header matches MaP2: palette, then `read_l2` size at 38, w/h
      at 42/44.
    - A `gfxlink2` with the `USER.RSC` ID added (`0x79380025` by its
      `ReadLong`) builds a `.gfx` that loads.
    - The **pixels are garbage**, and the animation check fails with
      "animation position array too short".
    - So the type-11 pixel layout is not MaP2 planar. Either work it out or
      ignore `USER.RSC` and use types 6/7, which is simpler.
  - Hints: the `helpee`/`helpxt` type-4 resources *(unverified: these are the
    hint text)*. Compare with dMagnetic's handling.
- [ ] **MW Amiga / Atari ST** (Wonderland 1.27f; Collection Amiga). Verified
      2026-10-20:
  - Pictures: resources concatenated in disk order (`all.1` … `all.5`; the
    `more1.*` set is separate and holds text, icons and map data). Type 6/7 decode works. The scratch converter produced 121
    pictures, 32 of them animated, and they render in Spatterlight. Amiga and
    ST produced byte-identical `.gfx`.
  - Story: **not solved, but closer than it looked.**
    - The `code` resource (0x1BF04 bytes, in the `all` set) starts `60 1A`, a
      GEMDOS/68k program header. `index` is in the `all` set too.
    - `text` (80936 bytes) is in the `more` set, see Part 2.
    - Still to find: the dictionary (`wtab`; possibly inside `code`), where the
      game code starts after the GEMDOS header, and the undo size/PC for
      1.27f.
    - Until then, an Amiga/ST import needs the DOS story. Not acceptable for
      users who own only the Amiga version.
  - Containers:
    - `.adf` needs an OFS/FFS reader (the scratch `adf.py` works; port it to
      C).
    - `.st` needs FAT12; `.msa` needs unpacking first. `terps/plus/stdetect.c`
      already has `DecodeMsaImageToRawImage()` and a FAT boot-sector parser;
      share them rather than copy.
- [ ] **Original DOS releases** (Pawn 2.x, Guild 1.1, Jinxter, Corruption 1.11,
      Fish 1.02, Myth): `xtractpc.c` covers the story. Port dMagnetic's
      `loader_msdos.c` for the pictures, including its copy-protection
      patches. `games.txt` lists the patch offsets.
- [ ] **C64 `.d64`**: port `xtract64.c`.
  - Use `terps/c64diskimage` for directory and file access instead of its own
    sector code.
  - Apply `myth.c`'s ungarbling for Myth.
  - Pictures come from dMagnetic `loader_d64.c` (gfx5).
- [ ] **Amstrad CPC / Spectrum +3 `.dsk`, Archimedes `.adf`, Atari XL `.atr`,
      Apple II**: port the matching dMagnetic loaders.
  - Apple II: check how dMagnetic's input relates to `terps/readwoz` before
    adding a second WOZ/NIB reader.
- [ ] **Copy protection.** `xtractmw.c` patches the password check
      (`Protection` offset). `emu.c` A0DE also "removes the manual check". Make
      sure every loader path ends with the same patched code as the `.mag` path
      the game list already expects (`games.txt`).

### Tests

- [ ] A headless test in the style of the scratch `gfxcheck.c`.
  - Link `emu.c` with stub callbacks.
  - For every loader and fixture: init, extract every picture, run every
    animation to the end.
  - Expect 0 failures. For Wonderland expect 121 pictures, 32 animated
    (`rablan` has 0 frames in the original data).
- [ ] Byte-compare each loader's `.mag` with the converter-produced `.mag` for
      the same release. Compare decoded pictures as PPM.
- [ ] Fixtures can't be committed (commercial). Use a manifest with sha256
      pins and a fetch/locate script, as `terps/scarier` does.

---

## Part 2 — Magnetic Windows front-end features

### What the game asks for

`emu.c` `do_line_a()` case 2 (A0DF) receives MW requests from game code.
Today:

| Type | Sub | Meaning | Status |
|---|---|---|---|
| 7 | — | show picture | works (`ms_showpic`) |
| 10 | 0 | open **carried items** window | prints "[This function is not supported.]" |
| 10 | 1 | open **room items** window | same |
| 10 | 2 | open **map** window | same |
| 10 | 3 | open **compass** window | same |
| 10 | 4 | help/hints | works (`ms_showhints`) |
| 13 | 0 / name | stop / play music | `ms_playmusic()` is an empty stub in `glk.c` (~4022) |

### What the data contains (DOS Wonderland, verified 2026-10-20)

| Type | Count | Contents |
|---|---|---|
| 3 | 2 | `mapltr`, `mapcdt` |
| 4 | 232 | data: `code`, `text`, `index`, `wtab`; `map*` automap tables (`maplev`, `mapdat`, `maplca`, `mapsir`, `maptok`); `helpee`/`helpxt`; music `t-*`; one **hotspot table per picture** |
| 5 | 285 | small 1-bit images: **207 item icons** `ic99`…`ic354`; 63 `mapN` tiles (78 bytes); 14 UI icons `I*` (`Ipen`, `Igauge`, `Igraph`, `Iblock`, `Idcard`, `Ihcard`, `Iscard`, `Iccard`, `Irun`, `Ichart`, `Iccub`, `Iocub`, `Ifold`, `Ifile`); `cpass` |
| 6 / 7 | 226 each | pictures (Huffman bitstream / tree) |
| 8 | 52 | fonts |
| 9 | 4 | `mapcdr`, `maporg`, `mapexi`, `mapcdo` |
| 10 | 4 | mouse cursors |

- **Item icons `icNNN`** (207 of them, `ic99`…`ic354` with gaps). These are
  the monochrome pictures shown in the ST/Amiga inventory window.
  - Record: 6-byte header `18 00 18 00 48 00` (LE u16 width 24, height 24,
    byte size 72), then 72 bytes of bitmap.
  - Bitmap: row-major, 3 bytes per row, **least significant bit = leftmost
    pixel**, set bit = black. Rendering MSB-first gives scrambled shapes.
  - A contact sheet decoded this way is in
    `~/Downloads/Wonderland_converted/item_icons.png`. It shows recognisable
    objects: the LARD and FLOUR jars, a teapot, keys, a bottle, the White
    Rabbit.
  - The DOS `.RSC` set and the Amiga `more*` set have the same 207 names with
    byte-identical data.
  - NNN is probably the game's object number *(unverified)*. Check against
    the object table: the icon for an object the player holds should match.
- The `I*` UI icons:
  - 32×32 pixels, 1 bit per pixel. Each record is a 6-byte header
    (`xx xx 20 00 80 00`), then 128 bytes of bitmap (`Ifold`/`Ifile` are
    smaller). Presumably the same LSB-first bit order *(unverified)*.
  - The names suggest the window-bar or tool icons of the MW interface. The
    four `I?card` look like card suits, and `Iccub`/`Iocub` look like a closed
    and open cupboard.
- The Amiga type-5 set has 320 entries. On top of the DOS set it has compass
  art: roses `bcomp`/`mcomp`/`scomp` (big/medium/small) and direction
  pointers `[b|m|s][n|s|e|w|ne|nw|se|sw|d]ptr`.
- **Amiga/ST resources come in two sets**, verified 2026-10-20:
  - `all.rdf` → `all.1` … `all.5`: `code`, `index`, pictures (types 6/7).
  - `more1/2/3.rdf` → `more1.1` (4 bytes), `more1.2` (131179 bytes; the
    copies on each disk are the same size), `more1.3` (6086 bytes). They hold
    `text` (80936 bytes), `helpee`/`helpxt`, the map tables (types 3/4/9),
    the 320 type-5 images and the cursors.
  - Loaders must read both sets. The earlier note that the Amiga lacked types
    3/5/9 was wrong: they are in the `more` set.
- Still worth a DOSBox look (the `dosbox` MCP is available) at the original
  item, map and compass windows before building anything, to copy their
  layout and click behaviour.

### Tasks

- [ ] **Item windows (carried / room items).**
  - Decide where the list comes from:
    - (a) The game code builds it and the A0DF request only opens the window.
      Find where the item list is written: trace the A-line calls made right
      after a type-10 request.
    - (b) The original front end walks the object table itself. `emu.c`
      already has `do_findprop()` and the A0F2 object-property walk; work out
      the "carried by player" and "in room" tests from them.
  - Map object → `icNNN`: confirm that NNN is the object number (see above);
    otherwise find the lookup table in `code` or the type-3/4 data.
  - Icons into Glk: the loader decodes each `icNNN` to an image and hands it
    to `glk.c` as a picture resource, the same way pictures are handed over
    now. Use a separate number range so they don't collide with room
    pictures. Scale them up 2–3×, nearest-neighbour, so they stay crisp.
  - Glk presentation:
    - A pane (side or below) refreshed every turn, showing each object's icon
      with its name: either a graphics window with a grid of icons, or inline
      images in a text-buffer window, one line per object.
    - Clicking an object types `examine <object>`: mouse events in a graphics
      window, or hyperlinks in a buffer window.
    - Objects without an icon get their name only.
  - Options:
    - Glk command `glk items on|off`, alongside `glk animations`.
    - Off by default for the old (non-MW) games.
- [ ] **Clickable pictures** (hotspots), verified 2026-10-20.
  - Every room picture has a type-4 resource of the same name.
  - Each entry is: u16 name length, name, u16 *(unknown, values 2–16)*, u16 0,
    u16 rectangle count, then that many rectangles of four u16 values
    (x1, y1, x2, y2) in picture pixels. `0xFFFF` appears in place of a
    coordinate in some rectangles. A zero name length ends the list.
  - Example `bed`: shelf, heart, crown, pillow, quilt.
  - Work out the unknown u16 and what `0xFFFF` means, then map Glk mouse events
    on the graphics window to the word, e.g. type `examine shelf`.
  - Picture scaling in the Glk port (`ad3123db5`) means click coordinates must
    be scaled back.
  - Hotspot tables are type 4, so the picture loaders above must carry them.
    Add a side table to the `.gfx`, or a separate buffer handed to `glk.c`.
- [ ] **Compass.**
  - The Amiga/ST `more` set has the original art (`bcomp`/`mcomp`/`scomp` and
    the `*ptr` direction pointers, type 5). Use it where present; otherwise
    draw an 8-way + up/down rose from the current room's exits.
  - Work out how the game reports exits; the compass request may carry them.
  - Clicks type the direction.
- [ ] **Map.**
  - MW drew an automap from the `map*` resources: tables in types 3, 4 and 9,
    and the 78-byte `mapN` tiles in type 5.
  - Formats are unknown. Start by dumping `maplev`/`mapdat` against the room
    list, then decide between a faithful tile map and a simple generated
    graph.
  - Low priority: Spatterlight users have the game's `map`-free text.
- [ ] **Music.**
  - Wonderland has `t-*` scores plus instrument samples (`Piano`, `Violin`,
    `Tuba`, …) as type-4 resources.
  - `emu.c` `sound_extract()` builds MIDI from a `.snd` file (`init_snd`),
    but `glk.c` passes no sound file, and `ms_playmusic()` does nothing.
  1. Have the loader build the `.snd` buffer from the resources.
  2. Implement `ms_playmusic()` with Glk sound (Spatterlight plays MIDI; see
     the Sounds pass).
  - The instrument samples are a later refinement.
- [ ] **Fonts and cursors** (types 8, 10): not needed. The Glk styles replace
      them.

### Ordering suggestion

1. In-memory init + MW DOS loader (Wonderland and the Collection get pictures
   with animation without a converter).
2. DOSBox check of the original item/map/compass windows.
3. Item windows + clickable pictures.
4. Remaining platform loaders (D64, DSK, Archimedes, original DOS, Atari XL,
   Apple II).
5. Amiga/ST story extraction, music, compass, map.

### Reference: scratch tools from the 2026-10-20 Wonderland conversion

- `mw2gfx.py` — MW resources → MaP2 `.gfx` with animations. A copy is in
  `~/Downloads/Wonderland_converted/`.
- Key format notes it encodes:
  - MW picture: u32 magic, 16-word palette, w/h at 0x24/0x26, bitstream size at
    0x2a, bitstream at 0x2c.
  - Huffman: tree bitmasks at 0x00/0x120, branches at 0x20/0x140, escape at
    0x240.
  - RLE: escape then 0xff = literal escape; escape then n = repeat the next
    symbol n+4 times.
  - Two pixels per symbol (low nibble first); every row is XOR'd with the row
    above.
  - Animation tail: frame count, then per frame `w, h, transparent colour key
    (≥16 = opaque), size, bitstream` using the same tree. The position tables
    and playback script that follow are already in MaP2 layout.
  - MaP2 frame: 2-byte pad, `read_l2` size, w, h, 0, planar data, then an
    optional mask (`((w-1)/8+2)&~1` bytes per row; a set bit = transparent).
