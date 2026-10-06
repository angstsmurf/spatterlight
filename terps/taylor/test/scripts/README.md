# Command script replays

`make -f Makefile.headless scriptcheck` (from `terps/taylor`) plays each
command script in this folder through `taylor_hl` on every release of its game
in `../images/games/` and compares the transcript with a hash in
`scripts.tsv`. 94 replays, a few seconds.

`taylor_hl` is the interpreter on plain CheapGlk: commands on standard input,
transcript on standard output, no pictures (those are tested by
`../images/`). It forces the fixed-seed random generator, so a replay is
repeatable. Each replay starts with the keys of the game's row in
`../images/games.manifest.tsv` (the answer to "restore a saved game?", the
version of a Temple of Terror image).

The runner is `../../../common_imagetest/run_script_tests.py`, shared with
Plus. The transcript is game text, so only its SHA-256 and line count are
kept. To see what changed, save the transcripts of two builds and diff them:

    R=../common_imagetest/run_script_tests.py
    $R test/scripts test/images ./taylor_hl --save /tmp/before
    $R test/scripts test/images <other taylor_hl> --save /tmp/after
    diff -r /tmp/before /tmp/after

`--list` prints how each replay ends, `--bless` rewrites the hashes. CheapGlk
has no status window, so the room description is written into the scrollback
on every turn. Its "Glk library error" lines and the runs of empty lines that
clear the missing window are dropped before hashing.

## The scripts

Written by hand in Spatterlight, one command per line, and re-derived where
the fixed seed made them go wrong. A "press any key" pause swallows the whole
next line of a script, so a command that pauses is followed by an empty line.
Where a script has to wait, `INVENTORY` takes a turn.

| script | ends | notes |
| --- | --- | --- |
| `blizzard-pass` | won, 100%, on all 4 images | |
| `fantastic-four-uk` | won, on all 10 images | Questprobe 3, UK release. The US one is a Plus game: `terps/plus/test/scripts/fantastic-four-us.txt` |
| `he-man` | won, on all 9 images | |
| `kayleth` | won, on all 9 images | no random events |
| `rebel-planet` | won, on all 8 images | the subway train and its ticket control are random |
| `temple-of-terror` | won, on all 18 images | |
| `temple-of-terror-alt` | won, on all 18 images | another route |
| `uitest-taylormade` | stops early, as written | Temple of Terror; `UITests/Supporting Files/Command scripts/TaylorMade command script.txt` |
