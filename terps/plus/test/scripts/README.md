# Command script replays

`make -f Makefile.headless scriptcheck` (from `terps/plus`) plays each command
script in this folder through `plus_hl` on every release of its game in
`../images/games/` and compares the transcript with a hash in `scripts.tsv`.
41 replays, a few seconds.

`plus_hl` is the interpreter on plain CheapGlk: commands on standard input,
transcript on standard output, no pictures (those are tested by
`../images/`). It forces the fixed-seed random generator, so a replay is
repeatable.

The runner is `../../../common_imagetest/run_script_tests.py`, shared with
TaylorMade. The transcript is game text, so only its SHA-256 and line count
are kept. To see what changed, save the transcripts of two builds and diff
them:

    R=../common_imagetest/run_script_tests.py
    $R test/scripts test/images ./plus_hl --save /tmp/before
    $R test/scripts test/images <other plus_hl> --save /tmp/after
    diff -r /tmp/before /tmp/after

`--list` prints how each replay ends, `--bless` rewrites the hashes. CheapGlk's
"Glk library error" lines (window calls it cannot honour) are dropped before
hashing.

## The scripts

Written by hand in Spatterlight, one command per line. A `<HIT ENTER>` pause
swallows the whole next line of a script, so a command that pauses is followed
by an empty line. Where a script has to wait, `INVENTORY` takes a turn.

| script | ends | notes |
| --- | --- | --- |
| `fantastic-four-us` | won, on all 10 images | Questprobe 3, US release. The UK one is a TaylorMade game: `terps/taylor/test/scripts/fantastic-four-uk.txt` |
| `buckaroo-banzai` | won, on all 11 images | |
| `buckaroo-banzai-alt` | won, on its 4 images | another route |
| `spider-man` | won with 100, on all 12 images | the same file as in the Scott folder, where it does not finish |
| `claymorgue` | won, both images | re-derived for the fixed seed: the tin can, the ageing and the float spell are the random parts |
| `claymorgue-uitest` | stops early, as written | `UITests/Supporting Files/Command scripts/Plus command script.txt` |
