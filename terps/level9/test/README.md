# Loader test of the Level 9 interpreter

`make -f Makefile.headless check` (from `terps/level9`) loads every game image
of `games.manifest.tsv` in `level9_hl`, plays the commands of `probe.txt` and
compares the transcript with a hash in `scripts.tsv`. 151 images, under a
second.

It is a test of the loaders: of finding the game in a Spectrum snapshot
(`.z80`, 48K and 128K, `.sna`), on a tape (`.tzx`, `.tap`), on a +3 or Amstrad
CPC disk (`.dsk`) and in a bare data file (`.dat`, `.l9`), and of joining the
data and the a-code of the releases that keep them apart. A game that is found
at the wrong place, or joined wrongly, prints an error or garbage. The
transcript starts with the name the interpreter gives the game, so the
detection is covered as well.

`level9_hl` is the interpreter with its Glk port on plain CheapGlk: commands
on standard input, transcript on standard output, no pictures. It forces the
fixed-seed random generator, so a replay is repeatable.

The runner is `../../common_imagetest/run_script_tests.py`, shared with Plus
and TaylorMade. The transcript is game text, so only its SHA-256 and line
count are kept. To see what changed, save the transcripts of two builds and
diff them:

    R=../common_imagetest/run_script_tests.py
    $R test test ./level9_hl --save /tmp/before
    $R test test <other level9_hl> --save /tmp/after
    diff -r /tmp/before /tmp/after

`--list` prints how each replay ends, `--bless` rewrites the hashes, and a
prefix (`z80-`, `tzx-scapeghost`) picks rows.

## The games

The games are copyrighted and not in the repository: `games/` is git-ignored,
and a row whose file is missing (or has another SHA-256) is skipped, so the
test passes, testing nothing, on a bare checkout.

`collect.py DIR...` adds every file under the folders that loads and is not in
the manifest yet: it copies the file to `games/<id>/` and appends a row to both
tables. Then run the runner with `--bless` and look at `--list`. Each file gets
a folder of its own, which is wrong for a file that needs another one beside
it: the second side of a disk (`dsk-*-side-b` live with their side A) and the
parts of a game that are told apart by file name alone (`dat-lancelot`). Move
those by hand and change the `dir` column.

## The script

`probe.txt` is not a walkthrough. It is a dozen commands that every game has
an answer to (`look`, `inventory`, the directions, `score`) and the digits that
the multiple choice games (Adrian Mole, The Archers) take. Some games end
during it, and a few ask whether to go on to the next part. That is fine: the
replay is deterministic, and that is all the test needs.

## Rows worth knowing

| row | why |
| --- | --- |
| `z80-eden128` | Return to Eden, 128K snapshot saved with page 4 at 0xC000. Its data runs from page 5 through page 2 into page 0, which is the fourth page of the `DecompressZ80` image and not the third, so the scanners only find it after `find_game_in_z80_image` in `level9.c` has exchanged the two |
| `z80-scape1`…`3`, `z80-ghost1`, `z80-ingrid128` | 128K snapshots of the releases with separate a-code (`try_z80_split_join`). `z80-scape1` has a page other than 0 at 0xC000 and is only joined after the exchange |
| `tzx-*-128k-*`, `dsk-*` | the same join from tape blocks and from +3/CPC disk files |
| `tzx-jewels-of-darkness-*`, `dsk-jewels-of-darkness` | a compilation: the first game is carved out |
