# Testing terps/readwoz

`ciderpress.c` is a cut-down CiderPress that reads Apple II DOS 3.3 and
ProDOS disk images (`.dsk`, `.nib`, and WOZ by way of `woz2nib.c`). It is
compiled from `../readwoz/` by the scott, plus and comprehend Makefiles and
by the Xcode project, so a new `.c` file here needs entries in all three
Makefiles *and* the pbxproj (`plutil -lint` the staged project). The two
files stay separate on purpose: comprehend's `op5scan` and scott's WOZ sniff
want the converter without the filesystem reader.

Who calls what (grep `ciderpress.h` under `terps/`):

| function | callers |
| --- | --- |
| `InitNibImage`, `InitDskImage`, `FreeDiskImage` | scott and plus `apple2detect.c`, scott `apple2_vector_draw.c`, bocfel `z6/extract_apple_2.cpp`, comprehend |
| `ReadApple2DOSFile` | scott `apple2detect.c` |
| `GetAllApple2DOSFiles` | scott `apple2detect.c`, comprehend `test/op5scan.c` |
| `GetAllProDOSFiles` | comprehend `apple2.cpp` and its tests |
| `ReadFullImageFromNib` | comprehend `apple2.cpp` and its tests |
| `ReadImageFromNib` | scott `apple2detect.c` |
| `ReadInfocomV6File` | bocfel `z6/extract_apple_2.cpp` |
| `is_woz_image`, `woz2nib` | every caller above that accepts a `.woz` |

## The suites

Run all of them before and after a change to either file. The first four
live here; the rest are the consumers, which only notice a change when a
*game* breaks.

```
cd terps/readwoz/test && ./snapshot.sh > new.tsv   # every file of every corpus disk: name, size, hash
cd terps/readwoz/test && ./malformed.py            # damaged images under ASan/UBSan
cd terps/readwoz/test && ./unit.sh                 # woz2nib.c piece by piece
cd terps/readwoz/test && ./v6check.py              # ReadInfocomV6File on the Infocom V6 disks
cd terps/scott           && make -f Makefile.headless formatcheck companioncheck scriptcheck
cd terps/scott/saga/test && make test
cd terps/plus            && make -f Makefile.headless imagecheck companioncheck trunccheck scriptcheck
cd terps/comprehend      && make check
```

plus a build of the Spatterlight scheme (`xcodebuild`) for bocfel's
`extract_apple_2.cpp`, whose reader side only `v6check.py` covers.

- `snapshot.sh` builds `ciderpress_probe.c` and runs every operation of the
  public API over every Apple II image under the scott, plus and comprehend
  test corpora (or the directories given), writing one TSV row per
  extracted file: name, size, SHA-256. For a `.woz` the `nib` op also
  records the hash of the converted image and of each track. There are no
  goldens: write a snapshot before the change and diff afterwards. A crash
  becomes a `CRASH` row rather than ending the run.
- `malformed.py` builds the probe with the address and undefined behaviour
  sanitizers and feeds it damaged copies of one DOS 3.3 disk, one ProDOS
  disk and one WOZ: truncations, flipped bytes, catalog loops, directory
  chains longer than their `eof`. A WOZ is damaged both as a whole (which
  the file checksum catches) and inside its chunks with the checksum
  recomputed, so the damage reaches the parser. Only normal exit is
  checked, about 2900 cases; the old converter failed 664 of them.
- `unit.sh` builds `woz2nib_unit.c`, which includes `../woz2nib.c`, under
  the sanitizers. It makes its own bitstreams and needs no images: CRC,
  sync search, the decoder against a bit-at-a-time reference, synthetic
  WOZ1 and WOZ2 files, every truncation. Thirteen mutations seeded into
  `woz2nib.c` were all caught.
- `v6check.py` checks what `ReadInfocomV6File` extracts from the Apple II
  disks of Zork Zero, Shogun, Journey and Arthur against `v6_expected.tsv`:
  34 rows, 19 disks as `.woz`, 10 of them also as `.nib`, 5 as `.dsk`. The
  disks are not in the repository; they are looked for under `v6games/`
  (gitignored, a link will do) and known by SHA-256, not name. Every disk
  gives the same file in each format except Shogun side 4, whose `.dsk` is
  a damaged conversion. Shogun 1-3 and Arthur 3 are byte-identical to
  CiderPress's own extractions. `--bless` adds a row for a new image.

The `.raw` files next to the V6 disks in Downloads are old extractions,
not ground truth (Journey side 5 is mostly zeros). `~/mame/Arthur.dsk` is a
Macintosh HFS floppy of the Mac release, not an Apple II disk.

## Behaviour the rewrite of October 2026 settled

Both files were rewritten on 2026-10-12 (`ciderpress.c` from 2997 to about
1500 lines, `woz2nib.c` to about 530) with the snapshot held byte-identical
except where listed here. These are decisions, not accidents; a change to
any of them wants a test row.

`ciderpress.c`:

- Every read is bounds-checked against the image length. This exposed a
  caller bug in scott's `DetectApple2`: `ReadApple2DOSFile` overwrites its
  `*len` with the database length, which was then passed on as the disk
  size. scott keeps `disksize` and `datasize` apart now.
- `ReadApple2DOSFile` does *not* reset its out-parameters on entry. scott
  calls it once per disk of a two-disk game and needs the first disk's
  inventory image and tables to survive the second call. The header says
  so; a new caller must zero them first.
- A file's length is the smaller of the catalog's sector count and its T/S
  list. Files with two T/S-list sectors (ELIMDEMO W/SOUND, RG1/DEMO,
  SORCEROR OF CLAYMORGUE CASTLE, THE INCREDIBLE HULK) come out 256 bytes
  shorter than before: the dropped tail was a sector that was not theirs.
  No game golden changed.
- A bad T/S-list link ends the list instead of failing the file.
  Adventureland's database "R0106" links to track 32, sector 32.
- The file list is freed on every new image, so a second call on the same
  image returns N files, not 2N.
- The inventory image is reported without its four bytes of binary header.
  Its length is still the file's sectors, not the picture's; every picture
  in the corpus ends by its own width and height well before that.
- A bad catalog is reported with `debug_print` and the files found before
  the damage are still returned.
- Per-sector checksum verification stays off and the tolerance for odd
  address prologs and epilogs in the sector index stays: comprehend and
  Talisman read damaged, copy-protected sectors on purpose.
- 13-sector (DOS 3.2), 40-track and 3.5" images are not supported, and the
  scaffolding that suggested they might be was removed rather than finished.
- Infocom V6 `.nib` and DOS-order `.dsk` images extract; they used to fail
  or crash. `.woz` output did not change.

`woz2nib.c`:

- Nothing is read past the end of the file, whatever the header, chunk
  sizes, track table or bit counts say. A WOZ1 bit count is clamped to the
  6646 bytes of its record. Fewer than 8 bytes after the last chunk are
  ignored. The magic ("WOZ1"/"WOZ2") decides the layout, as the
  specification says; all 69 images at hand agree with their INFO version.
- Lenient track policy: a track that cannot be used (unmapped, a TMAP value
  of 160 or more, too short to hold a sync pattern, lying past the end of
  the file) becomes an empty track of zeros instead of failing the image.
  Quarter-track neighbours are not consulted for an unmapped whole track;
  copy-protected disks do that on purpose and this is a DOS 3.3/ProDOS
  reader.
- A stored checksum of 0 means "not calculated" and is accepted. A repeated
  chunk is reported in the debug build and the later one counts.
- `*data_size` is written only on success; out of memory returns NULL, no
  `exit()`.
- Both the WOZ checksum and the nibble decode use tables built on first use;
  the bitwise alternatives were measured slower on a path that is timed
  (1.24 ms per image, unchanged by the rewrite). Comment in the file.
- The sync search does not wrap round the end of the stream. Of 2415 corpus
  tracks, 35 have no sync pattern, all on one Talisman boot disk, and a
  wrapping search finds none there either.
- `ciderpress.c` recovers the true revolution length of a track itself
  (`FindRevolutionLength`); the two files point at each other so that
  neither is "fixed" alone.
