# Testing terps/c64diskimage

`c64diskimage.c` reads whole files out of C64 disk images (D64 of 35 or 40
tracks, D71, D81, each with or without trailing error info). It is cut down
from Per Olofsson's diskimage (BSD licence, header kept): no writer, no file
streams, no error-info handling. The header is the whole public API, seven
functions with `DiskImage` opaque. Nothing in it calls `exit()`; every failure
is NULL.

| Consumer | Uses it for |
|---|---|
| `scott/ai_uk/c64decrunch.c` | Scott Adams C64 disks (largest file, names, files by name) |
| `taylor/c64decrunch.c` | Taylor C64 disks |
| `plus/c64detect.c` | SAGA Plus C64 disks |
| `scott/saga/test/c64extract.c` | the saga C64 test rig |

Built by the Xcode project (target `c64diskimage`, libc64diskimage.dylib) and
by `scott/Makefile.headless`, `taylor/Makefile.headless`,
`plus/Makefile.headless` and `scott/saga/test/Makefile`. `c64unpack.h` lives
in `terps/unp64/`.

## The suites

```
cd terps/c64diskimage/test
./snapshot.sh > before.tsv      # before a change
./snapshot.sh | diff before.tsv -
python3 malformed.py

cd ../../scott  && make -f Makefile.headless formatcheck companioncheck scriptcheck
cd ../plus      && make -f Makefile.headless imagecheck companioncheck trunccheck scriptcheck
cd ../taylor    && make -f Makefile.headless imagecheck mamecheck scriptcheck
cd ../scott/saga/test && make test c64test c64a8test
xcodebuild -project ../../../../Spatterlight.xcodeproj -scheme Spatterlight build
```

**snapshot.sh** builds `c64diskimage_probe` and prints, for every D64/D71/D81
under the scott, saga, plus and taylor test trees (59 D64s when the corpora
are fetched; they are git-ignored, see each terp's manifest), what the reader
returns: file names, sizes and hashes, the largest file, and track/sector
chains. A crash is a `CRASH` row, not a stop. There is no blessed copy in the
repository: diff a run of the old code against a run of the new one.

**malformed.py** builds the probe with ASan and UBSan and runs it on disks it
makes itself and on damaged copies of them. The first part builds one disk of
every kind the reader takes (D64 35 and 40 tracks, D71, D81, with and without
error info) with known contents and checks every probe line exactly. This is
the only test of D71 and D81: no game in the corpora comes on one. The second
part damages those disks (and one scott corpus D64 if present): links to
track 0, track 36 or sector 21 of track 1, cyclic file and directory chains,
144- and 296-entry directories, truncated images, sizes that are no image,
blind damage. A few outputs are checked; all runs must exit normally and in
time. Last count: 623 runs, 0 failed. When the rig was built, seven faults
put into the reader on purpose (scratched policy, last-sector length, sector
bound, D71 side-2 offset, both cycle guards, 40-track size) each made it fail.

**The consumer suites** exercise the C64 loaders end to end. Counts when the
reader was rewritten: scott formatcheck 299, companioncheck 114, scriptcheck
244; plus imagecheck 36, companioncheck 55, trunccheck 31, scriptcheck 42;
taylor imagecheck 65, mamecheck 538, scriptcheck 115.

**Xcode:** building `-target scott -target taylor -target plus` into a scratch
SYMROOT fails to link the first time ("library 'decompressz80' not found").
Build `-target decompressz80` first, or run it twice.

## Not covered

- No real D71 or D81 in any corpus: synthetic images only.
- G64/NIB raw track images are not supported and not wanted (no corpus game
  needs them).
- The T64 readers (taylor `GetFileFromT64`, scott's inline one) and the two
  `c64_registry` loops are loader code, not this library; scott's T64 reader
  was not examined for bounds.

## Behaviour the rewrite of October 2026 settled

Change any of these only with a snapshot diff and a malformed.py row.

- **A bad track or sector never kills the interpreter.** A file whose first
  sector is off the disk, or links off the disk, is NULL. A chain that goes
  wrong later (a link off the disk, or back to a sector already read) ends
  before that sector. Directory walks stop the same way.
- **Scratched entries (type 0) do not exist**: not listed, not found by name,
  never the largest file. Two corpus disks changed for this reason, in
  scratched leftovers only. `scott/test/formats/games/d64-gremlins/gremlins.d64`
  lost ANIMATION 1-9, CAMEL TROPHY and a `----------------` entry from its
  listing. `Adventure Pack (1983)(Adventure International).d64` lost scratched
  COPYRIGHT / 1984 / ACTIVISION, so its largest file went from the scratched
  ACTIVISION (16386 bytes) to a live 9858-byte file and its first name is
  BOOT. No suite row changed.
- **Entries without a name are not listed**: the name array has no NULL
  elements. With no files it is NULL and `*numfiles` is 0.
- **A name of more than 16 characters matches nothing** (the original cut it
  to 16). `?` matches any character, `*` the rest of the name.
- **A last sector that says 0 or 1 bytes used gives 0 bytes.** The original
  gave 254 when it was the first sector of a named file.
- **No 0xffff length cap.** No corpus file is that long.
- **40-track D64: tracks 36-40 are read.** The original accepted the image
  size and then refused the tracks.
- **Error info is ignored for every image type.** It was never read for D64;
  the one corpus disk that has it (175531 bytes) reads the same.
- **One-sector chains are readable through `di_get_file_at_ts`.**
- **The DiskImage does not copy or own the data**; `di_free` leaves it alone.
- **Consumers:** scott `LoadC64USImages` has no 1024-name cap, frees the name
  array and skips names shorter than 6 characters instead of reading past
  them. plus treats running out of memory while reading a D64 as "no such
  file", not `Fatal`.

## Declined, with the reason

- A rewrite from scratch, as readwoz had: the library is vendored, licensed
  and was working, so it was fixed in place. Little of the original read path
  survived its own to-do items anyway (988 to 428 lines), but the zone
  arithmetic, the name matching and the directory layout are the original's.
- G64/NIB support: no corpus game needs it.
- A clang-format pass: the file was retyped in one style.
- Moving the T64 readers and `c64_registry` loops in here: loader work, out
  of scope.
