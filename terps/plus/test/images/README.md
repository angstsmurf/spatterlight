# Picture tests

`make -f Makefile.headless imagecheck` (from `terps/plus`) starts every game
in `games.manifest.tsv`, draws all its pictures and compares what ended up on
screen with a golden.

## What is compared

`plus_image_probe` is the interpreter itself (`glk_main()` and everything
below it) linked against a Glk whose graphics window is a plain pixel canvas
(`../../../common_imagetest/image_glk.c`). It runs the game up to its first
command prompt, answering key prompts on the way, and then `test/image_probe.c`
draws every picture the loader found and every room. The output is one line
per picture:

    R001      1453 3f575734  180320 px    0,0   560x322  3ceaa3e9

name, size and CRC32 of the picture's data as extracted from the disk, then
what the drawing code painted: number of pixels, bounding box, CRC32 of the
canvas. A picture shown before the first prompt is reported as `title`. The
lines `game`, `system`, `pictures` and `rooms` say what was detected.

So an extraction regression (wrong sector, wrong offset, bad depacking)
changes the first CRC, a renderer regression only the last one. The output
holds no game content, so `expected/<id>.txt` is committed although the games
are not.

To look at the pictures, run the probe by hand with `-d DIR`; it then also
writes each canvas to `DIR/<name>.png`:

    ./plus_image_probe -d /tmp/pics test/images/games/<dir>/<file> [keys]

## The games

The games are copyrighted and git-ignored (`games/`). A row whose file is
missing is skipped, so the suite passes, testing nothing, on a bare checkout.

    ../common_imagetest/run_image_tests.py test/images ./plus_image_probe --collect ~/Downloads ~/Desktop

searches the given folders for the files the manifest names, checks each
candidate's sha256, and copies the right ones to `games/<dir>/<file>`. A file
in `games/` with the wrong sha256 fails the run rather than being tested
against a golden made from a different dump.

Most games are more than one file: the two-disk sets find their other disk by
file name in the folder of the disk that was opened, and an MS-DOS game reads
its pictures from the `.PAK` files next to the database. So the manifest keeps
the original file names and gives every game its own folder. A second disk
that can be opened itself is a row of its own (`-disk2`); a file that is only
read by name, like the `.PAK` pictures, is a companion row with the id `-`.
If any file of a folder is missing the whole folder is skipped.

## Coverage

35 rows, one per distinct result found among the local images of *Buckaroo
Banzai*, *Spider-Man*, *Fantastic Four* and *Sorcerer of Claymorgue Castle*:

| System | Rows | Variants |
|--------|------|----------|
| MS-DOS | 4 | all four games, database plus `.PAK` pictures (286 companion rows) |
| C64 `.d64` | 5 | one of them under the name `.plus` |
| Apple II `.dsk`, `.do` | 13 | two-disk sets from either side |
| Apple II `.woz` | 2 | WOZ2, either side |
| Atari 8-bit `.atr` | 8 | two-disk sets from either side |
| Atari ST `.st`, `.msa` | 3 | |

Not covered: Apple II `.nib`, for want of a sample.

## Adding or updating

- New game: add a manifest row (`shasum -a 256`) and a companion row for
  every other file it needs, put the files in `games/<dir>/`, run
  `run_image_tests.py test/images ./plus_image_probe --bless <id>`, and look
  at the pictures once with `-d`.
- Intended change to a loader or renderer: `--bless`, then review
  `git diff expected/`.
- `run_image_tests.py test/images ./plus_image_probe <id-prefix>...` runs some
  rows, `--list` shows which games are present.
