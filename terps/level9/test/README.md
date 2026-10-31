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

## Walkthroughs

`make -f Makefile.headless walkthroughs` plays each script of `walkthroughs/`
through to the end of its game and checks the last score and a phrase from the
ending. The game images are not in the repository: they are looked for under
`$L9_GAMES`, default `~/Downloads/Level 9 stuff`, and a row whose image is missing
or differs is skipped.

The scripts come from Andreas Scherrer's L9TestSuite, some of them based on
solutions by Jacob Gunness and Dorothy Millard. They have been repaired to reach
the ending in `level9_hl`, whose random numbers come from the determinism array
unless a `#SEED` line asks for a seed, so they are only repeatable in that build.

Price of Magik, Red Moon and Knight Orc depend heavily on random events, so a small
edit to their scripts can break a later step. Try a few extra `WAIT`s in that case.

Knight Orc's gnome asks for a word from the manual. `ORK` works for every question
on this image, as does `GROK`. `#cheat` cannot find it: it only reports a word when
the game calls its `restore` function, which this mid-game check never does.

`snowball_v3` expects 950 of 1000. The winning `PULL LEVER` sets the last of the 20
score flags, but the game then accepts only `SAVE`, `RESTORE` and `RESTART`, so the
final score is never printed. The script's last `SCORE` comes just before the lever.

Lancelot's mid-script `SCORE` before the part change is needed: only part 1's score
routine awards the 20 points for emptying the dungeon, and it does so when `SCORE` is
typed.
