# Disk and tape image loader tests

`make -f Makefile.headless formatcheck` (from `terps/scott`) loads every game
image in `games.manifest.tsv` through `DetectGame()` and compares what the
loaders produced with a golden.

## What is compared

`scott_format_probe` (`test/format_probe.c`) runs `DetectGame()` on one file and
prints a fingerprint: the detected title, system and variant, the header
counts, and a CRC32 of each table (rooms, items, verbs, nouns, messages,
actions, system messages) and of each kind of picture data (ZX loading screen,
Irmak tile pictures, Howarth line pictures, the US releases' per-picture
blobs, the Apple II descramble table, the TI-99/4A title text and bytecode).

A fingerprint holds no game text, so `expected/<id>.txt` is committed although
the games are not. A regression shows up as the line that changed: a wrong
picture offset in a disk extractor changes `usimages` and nothing else, a
depacker regression changes everything or ends in `NO FINGERPRINT`.

To see what is behind a changed CRC, run the probe by hand with
`SCOTT_PROBE_VERBOSE=1`; it then prints the tables in full.

## The images

The images are copyrighted and git-ignored (`games/`). A row whose image is
missing is skipped, so the suite passes, testing nothing, on a bare checkout.

    test/formats/run_format_tests.py --collect ~/Downloads ~/Desktop

searches the given folders for the files the manifest names, checks each
candidate's sha256, and copies the right ones to `games/<dir>/<file>`. A file
in `games/` with the wrong sha256 fails the run rather than being tested
against a golden made from a different dump.

The two-disk games (Atari 8-bit, Apple II, the C64 Claymorgue) find their
other disk by file name in the folder of the disk that was opened. So the
manifest keeps the original file names, gives every game its own folder, and
lists both disks; each disk is a row, because either can be the one opened.
If one disk of a folder is missing the whole folder is skipped.

## Coverage

288 rows, one per distinct loader result found among about a thousand local
images:

| Container | Rows | Variants |
|-----------|------|----------|
| ZX Spectrum `.z80` | 41 | v1 compressed, v2, v3; the Parsec-cracked dumps that need the second RLE pass |
| ZX Spectrum `.sna` | 14 | |
| ZX Spectrum `.tap` | 19 | with and without loading screen |
| ZX Spectrum `.tzx` | 32 | incl. the Alkatraz-protected *Scott Adams Scoops*, all four menu choices |
| C64 `.d64` | 50 | plain and packed UK releases, US releases with pictures, the two-disk Claymorgue, the compilation disks (*Mysterious Adventures* 1 and 2, *Savage Island*, the twelve-game *Adventure Pack*), every menu choice |
| C64 `.t64` | 26 | UK and US releases |
| Atari 8-bit `.atr` | 20 | both header variants, two-disk sets from either side, the DOS-less UK 16K *Spider-Man* boot disk |
| Apple II `.dsk`, `.do` | 28 | two-disk sets from either side, single boot disks, with and without descramble table |
| Apple II `.woz` | 18 | WOZ1 and WOZ2 |
| TI-99/4A `.dsk` | 11 | the ten cartridge-era adventures, *Return to Pirate's Isle* |
| TI-99/4A `.fiad` | 26 | |
| TI-99/4A `.cart`, `.rpk`, `.zip` | 3 | *Return to Pirate's Isle*; `.rpk` and `.zip` go through the in-memory unzip |

Not covered, for want of a sample: Apple II `.nib`. `.dat`, `.saga` and `.sag` are not containers (any of
the above, or a plain ScottFree database, under another name); the plain
database is covered by `make check`.

## Adding or updating

- New image: add a manifest row (`shasum -a 256`), put the file in
  `games/<dir>/`, run `run_format_tests.py --bless <id>`, read the new golden.
- Intended loader change: `run_format_tests.py --bless`, then review
  `git diff expected/`.
- `run_format_tests.py <id-prefix>...` runs some rows, `--list` shows which
  images are present.
