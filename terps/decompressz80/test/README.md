# Testing terps/decompressz80

The ZX Spectrum file library: `.z80` snapshots, TAP and TZX tapes, the
Alkatraz loader decryption and the SCREEN$ layout. Rewritten in October 2026
from one 1934-line file into four TUs behind one public header,
`decompressz80.h`, which exports exactly fifteen names and states the contract
of each (`nm -gU` on the library should show nothing else).

| File | What |
|---|---|
| `z80snap.c` | the .z80 reader (Fuse/libspectrum derived, GPL notice kept): `DecompressZ80`, `DecompressZ80Pages` |
| `tapetzx.c` | one `tzx_layouts[]` table for every TZX 1.20 block ID; `GetTZXBlock`, `GetTAPBlock`, `ExtractTapePayloads`, `FindTapeLoadingScreen` |
| `alkatraz.c` | `ZXNewMemory`, `ZXLdir`, `ZXLddr`, `DeAlkatraz`, `DeshuffleAlkatraz` (original code) |
| `zxscreen.c` | palette and screen layout: `ZXBitmapRow`, `ZXBitmapOffset`, `ZXDecodeAttr`, `ZXScreenIsBlackOnWhite` |
| `decompressz80_internal.h` | `ZX_LOG`, `ReadLE` |

Consumers: scott (`detect_game.c`, `title_image.c`), taylor (`extracttape.c`,
`loading_screen.c`, `decrypttotloader.c`, `loadtotpicture.c`), level9, unquill
(`unquill.c`, `illustrator.c`) and `common_zxscreen/zx_title.c`. Built by the
Xcode project and by `scott/Makefile.headless`, `scott/saga/test/Makefile`,
`taylor/Makefile.headless`.

## The suites

```
cd terps/decompressz80/test
./unit.sh                        # 3415 checks, no game files needed
./snapshot.sh > before.tsv       # before a change
./snapshot.sh | diff before.tsv -
./malformed.py

cd ../../scott  && make -f Makefile.headless formatcheck companioncheck scriptcheck savecheck check
cd saga/test    && make test scenetest
cd ../../../taylor && make -f Makefile.headless imagecheck mamecheck scriptcheck
cd ../level9    && make -f Makefile.headless check
xcodebuild -project ../../Spatterlight.xcodeproj -scheme Spatterlight build
```

**unit.sh** builds `z80_unit.c` with the library under ASan and UBSan. It
makes its own snapshots and tapes: v1/v2/v3, every machine type in both
versions, banked page, 16K, missing / duplicate / unknown / short pages, SLT,
`ED ED` at the end, one block of every TZX ID, TAP edge cases, Alkatraz by
hand-worked vectors and round trip, the screen helpers. Eight deliberate
breakages of the library were each caught when it was written.

**snapshot.sh** builds `z80_probe` and prints a TSV of what all five entry
points make of every .z80, .slt, .sna, .tzx and .tap under the scott and
taylor test trees (1692 rows). The corpora are not in the repository, so the
table is not either: diff a run before the change against one after.

**malformed.py** damages one image of each kind (a v1 .z80, a 48K, a 128K, a
.tzx, a .tap) and runs every operation of the probe on each under ASan/UBSan
(4780 runs). Only a normal exit in time is checked.

**The consumer suites** at the time of the rewrite: scott 299 / 114 / 244 /
36; taylor 65 / 538 / 115; level9 151 images; plus an Xcode Debug build of
scott, taylor, level9 and unquill.

## Not covered

- Pentagon / Scorpion pages past the eighth: read past and dropped. No corpus
  file has any.
- Tapes whose turbo blocks lack the final 0x80 byte: one such (`tzx-heman-2`)
  is in the taylor corpus; nothing else.
- The old 0x2001f-byte 128K image: gone, see below; nothing tests for it.

## Behaviour the rewrite settled

Change any of these only with a `snapshot.sh` diff and a `unit.sh` row.

- **The v1 reader is strict, on purpose.** A v1 header is only register
  values, and taylor hands every file to `DecompressZ80`, so the reader's
  strictness is the file-type check. Compressed: the first `00 ED ED 00` must
  end the file and what precedes it must decode to exactly 48K. Uncompressed:
  the body must be exactly 48K. A tolerant reader took tapes and .sna files
  for v1 snapshots. libspectrum's "byte 12 == 255 means 1" rule was not taken.
  See the comment in `ReadV1Memory`.
- **Nothing is printed.** `-DDECOMPRESSZ80_VERBOSE` gives the reason on stderr.
- **Snapshots that load now and did not**: .slt trailers, +2/+2A/+3/Pentagon/
  Scorpion with the right banked page, 16K and Timex as 48K, files with pages
  missing (zero filled).
- **Snapshots turned away now and not before**: a page that is not 16K, a page
  number the format has not, the same page twice, a v2/v3 file with no RAM
  page.
- **The 128K image is 0x20000 bytes**, each page once: 5, 2, the banked page
  (`port & 7`, as `DecompressZ80Pages` reports it), then the rest in page
  order; with 5 or 2 banked in, those two and then the six others. The old
  image was 0x2001f bytes with 0x1f bytes of nothing at the end to match a
  128K .sna. Consumers that depended on the old layout: taylor's Blizzard Pass
  now addresses the image by page (`6 * ZX_PAGE_SIZE`, addresses less
  `ZX_RAM_BASE`) and strips the four bytes after the third page of a 128K .sna;
  level9 swaps the third and fourth pages and retries when nothing is found
  (`find_game_in_z80_image`, needed by Return to Eden 128 and the first
  Scapeghost snapshot); unquill and scott read the first 48K only.
- **Block numbers count data blocks** (TZX 0x10 and 0x11; every block of a
  TAP), so text, pause and group blocks do not shift them and a .tzx and a .tap
  of one tape number alike. `GetTZXBlock` steps over every block kind of TZX
  1.20, the deprecated 0x16, 0x17, 0x34 and 0x40 too. `GetTZXBlock` and
  `GetTAPBlock` share one contract: input length its own argument,
  `*block_length` written on success only, the **whole** block returned, flag
  and checksum included. Stripping is the caller's job
  (`ZX_TAPE_FLAG_BYTES`, `ZX_TAPE_FRAME_BYTES`): an Alkatraz block has no flag
  and no checksum, and the first byte of Temple of Terror side B's loader block
  is data that `DeAlkatraz` decrypts. The block numbers in scott, taylor and
  level9 were re-derived from the corpus tapes when this changed.
- **A TAP block of no length ends the tape**, for `GetTAPBlock` as for
  `ExtractTapePayloads`. `GetTAPBlock` on a file that is not a tape returns
  blocks or NULL, never crashes.
- **`DeAlkatraz(src, src_length, src_offset, mem, target, count, &key,
  &schedule)`** returns 1/0, never allocates (`ZXNewMemory()` does), and carries
  no game: `AlkatrazKey { adjust, step, adjust_address, step_address }` is the
  caller's, taylor supplies 0xe022/0xe02f for Temple of Terror. The
  self-modification no longer leaks into the caller's numbers.
- **The ZX constants live in the header** (`ZX_MEMORY_SIZE`, `ZX_PAGE_SIZE`,
  `ZX_RAM_BASE`, `ZX_BITMAP_BASE`, `ZX_ATTR_BASE`, `Z80_IMAGE_SIZE_48K`,
  `Z80_IMAGE_SIZE_128K`); the copies in scott, taylor and common_zxscreen are
  gone. `READ_LE_UINT16` is not public: the library has a private `ReadLE` and
  scott keeps its own macro. `SPECTRUM_PRINTER_BUFFER` is taylor's.
- **All input buffers are `const`.**
- **scott's `not_z80` pre-check stays** (7603ab93e): with the reader strict
  and silent it is an optimisation only. There is no `IsZ80Snapshot`.

## Declined, with the reason

- Real libspectrum as a dependency: about twenty times the code for the three
  features used, and its full tape parser rejects whole tapes the tolerant
  walker accepts.
- `DecompressZ80` returning a struct or eight fixed pages: four call sites
  built on the flat buffer, low payoff.
- Auto-detecting z80 vs sna vs tap inside `DecompressZ80`: detection stays in
  the caller.
- Separate `zxscreen.h` / `alkatraz.h` headers: one umbrella header is enough.
