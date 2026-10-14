# Picture tests

`make -f Makefile.headless imagecheck` (from `terps/taylor`) starts every game
in `games.manifest.tsv`, draws all its pictures and compares what ended up on
screen with a golden.

## What is compared

`taylor_image_probe` is the interpreter itself (`glk_main()` and everything
below it) linked against a Glk whose graphics window is a plain pixel canvas
(`../../../common_imagetest/image_glk.c`). It runs the game up to its first
command prompt, typing the manifest row's keys at the prompts on the way, and
then `test/image_probe.c` draws every picture and every room. The output is
one line per picture:

    pic14     268 65edecbf    24576 px    0,0   256x96   ceb462dd
    room7                     24576 px    0,0   256x96   55fb0f21

For `pic<n>`, the picture pieces in the game's image table: size and CRC32 of
the unpacked data, then what drawing it painted (number of pixels, bounding
box, CRC32 of the canvas). For `room<n>`, the canvas after running the room's
draw instruction stream, which composes pieces, flips, fills and colour
changes; *Questprobe 3* has no such streams and no `room` lines. A ZX
Spectrum loading screen shown before the first prompt is reported as `title`.

*Rebel Planet* and *Kayleth* animate some of their pictures, and their
goldens end with the frames:

    anim queen               tick   3 b7aa48bc
    anim queen               17 ticks, timer 400

The probe puts the player in the room, sets the flags and objects the
animation depends on, and fires the timer by hand: a line for every tick that
changed the canvas, with its CRC32, then how many ticks were fired and the
timer interval the animation left behind (0 when it has stopped). For *Rebel
Planet* that is each of its animations (stars, force field, queen computer,
robot, crag snapper, serpent out and back) and what is left running after
walking from one animated room into another; for *Kayleth*, every room that
starts the timer.

So an extraction regression (tape block, depacker, snapshot offset) changes
the first CRC, a renderer regression only the last one. The output holds no
game content, so `expected/<id>.txt` is committed although the games are not.

Rooms are numbered up to the end of the exit table. That is more rooms than
have pictures, and the streams of the last ones are whatever data follows the
real ones. They are drawn anyway: the result is stable, and it is the
interpreter's bounds checks that are being exercised.

To look at the pictures, run the probe by hand with `-d DIR`; it then also
writes each canvas to `DIR/<name>.png`:

    ./taylor_image_probe -d /tmp/pics test/images/games/<dir>/<file> [keys]

To play a game through instead, give it a script with `-s`, one command per
line. The text of the game goes to stdout, with a CRC of the canvas after each
command, so the output of two builds can be compared with `diff`:

    ./taylor_image_probe -s script.txt <file> [keys] > transcript.txt

## The keys column

Every game but *Kayleth* opens with "load a saved game?", answered `n`. A C64
*Temple of Terror* image holding both versions first asks which to play: `1`
graphics, `2` text only, `3` pictures of one with the text of the other. The
ZX Spectrum *Temple of Terror* looks for the other tape side by file name
(`... Side A.tzx` / `... Side B.tzx`) and, when it is there, asks whether to
combine them: `y` or `n`, then the `n` for the saved game.

A game that asks for more keys than the row gives is answered with Return; if
it never reaches a command prompt the probe gives up with "the game keeps
asking for a key".

## The games

The games are copyrighted and git-ignored (`games/`). A row whose file is
missing is skipped, so the suite passes, testing nothing, on a bare checkout.

    ../common_imagetest/run_image_tests.py test/images ./taylor_image_probe --collect ~/Downloads ~/Desktop

searches the given folders for the files the manifest names, checks each
candidate's sha256, and copies the right ones to `games/<dir>/<file>`. A file
in `games/` with the wrong sha256 fails the run rather than being tested
against a golden made from a different dump.

Every game has its own folder and keeps its original file name, because of
the two-sided *Temple of Terror* tape. If one file of a folder is missing the
whole folder is skipped.

## Coverage

72 rows: one per distinct result found among about 115 local images of
*Rebel Planet*, *Blizzard Pass*, *Temple of Terror*, *Kayleth*, *Masters of
the Universe* and *Questprobe 3*, and one to three for every C64 *Temple of
Terror* image the loader knows, whose files are found in four different ways:

| Container | Rows | Variants |
|-----------|------|----------|
| ZX Spectrum `.z80` | 12 | v1, v2, v3 |
| ZX Spectrum `.sna` | 6 | |
| ZX Spectrum `.tap` | 3 | |
| ZX Spectrum `.tzx` | 11 | incl. the two-sided *Temple of Terror*, either side, combined or not |
| C64 `.d64` | 18 | incl. *Temple of Terror* graphics, text only and combined versions |
| C64 `.t64` | 8 | likewise |
| `.tay` | 14 | any of the above under Spatterlight's own extension |

The text-only rows have no pictures at all ("pictures 0"); they are there for
the loader, and for the command scripts of `../scripts/`, which play them.

## Damaged files

`make -f Makefile.headless damagecheck` runs `damaged.py`, which plays damaged
copies of the same games in `taylor_hl`, the text build, for one command:

- Six files whose game data is whole but whose pictures cannot be read (five
  `.sna` snapshots cut off above the game, and a *Questprobe 3* with its
  picture table signature spoiled) must print "The pictures in this file
  could not be read. Playing without graphics." and play on.
- Every row of the manifest, cut off at each eighth of its length, must end
  in an orderly way: a game that plays, or an error message and an exit. A
  crash or a hang fails.

It has no goldens: what a truncated file prints is not pinned down, only that
the interpreter survives it. A second or two.

## The versions of Temple of Terror

`make -f Makefile.headless totcheck` runs `tot_versions.py`, which checks the
version menus themselves, on every C64 image holding both versions and on the
two-sided Spectrum tape:

- C64: the menu is there (it says "disk" or "datasette"), 1, 2 and 3 load
  the graphics, text-only and combined versions, and other keys are ignored.
- Spectrum: the question is asked when the other side is next to the file,
  under the tape's names or under others (`tot a.tzx`, `tot b.tzx`), and not
  when it is missing, is the same version, or is another game. `y`/`Y` and
  `n`/`N` work from either side, and any other key is answered "Please
  answer Y or N.".
- The combined version has the pictures of the graphics version and plays
  `../scripts/temple-of-terror.txt` exactly like the text-only version. The
  graphics version plays differently.
- Deleting the files while the menu or the question waits for its key
  changes nothing: both files are already in memory by then.

It needs both builds (the probe for the game and its pictures, `taylor_hl`
for the text) and has no goldens. About 3 seconds.

## Comparing with the originals

The goldens above only say that nothing changed. `make -f Makefile.headless
mamecheck` says that the pictures are right: it compares the interpreter's
room pictures, pixel by pixel, with the screens of the original ZX Spectrum
games running in MAME.

    test/images/mame_capture.py [--mame ~/mame] [--jobs N] [game | game:room ...]

starts the original once for every room, pokes the room number, types a
command so that the game draws the room, and saves the display file as
`mame/<game>/room<N>.scr`. The typed command is a turn of the game, in which
things arrive, leave and change, so the flags and object locations the picture
was drawn from are saved as well (`room<N>.state`), and the probe, given
`TAYLOR_PROBE_STATE=mame/<game>`, draws each room from that state. Room 0 is
never drawn by the games, so `room0.scr` is the picture of the first room.
The whole capture is 627 runs and takes about half an hour; `mame/` is
git-ignored like `games/`, and without it `mamecheck` passes, testing nothing.

    test/images/mame_compare.py [--diff DIR] [-v] [game...]

Not compared ("not drawn"): rooms the interpreter leaves the previous picture
in, and rooms where the original still shows the first room's picture because
the player died before the room was drawn. `mame_known.tsv` lists the rooms
that are expected to differ, with the reason: animated pictures (the original
is caught at some frame, the probe compares the picture before the first),
room numbers past the last room of the game, the dark cave of Rebel Planet (black in play, but the
original draws its picture when the player is poked into it), and the
briefing of Kayleth (room 69), which the original prints over the animated
picture of the first room. `--diff DIR` writes
original, interpreter and difference side by side for every room that differs.

Covered: Rebel Planet, He-Man, Temple of Terror, Kayleth and Questprobe 3,
48K snapshots run in the `spectrum` driver, and Blizzard Pass, a 128K snapshot
run in `spec128` (it starts at the title screen, which wants a `y`).
Questprobe 3 draws its pictures from the game's own action table, so the
probe runs `Look()` on the captured state (69 flags there, not 67); its 34
rooms are listed in `mame_capture.py`, as the golden has no room lines.
Not covered: the Commodore 64 versions.

## Adding or updating

- New game: add a manifest row (`shasum -a 256`), put the file in
  `games/<dir>/`, run
  `run_image_tests.py test/images ./taylor_image_probe --bless <id>`, and look
  at the pictures once with `-d`.
- Intended change to a loader or renderer: `--bless`, then review
  `git diff expected/`.
- `run_image_tests.py test/images ./taylor_image_probe <id-prefix>...` runs
  some rows, `--list` shows which games are present.
