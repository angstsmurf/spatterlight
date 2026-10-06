# Command script replays

`make -f Makefile.headless scriptcheck` (from `terps/scott`) plays each command
script in this folder through `scott_hl` on every image of its game in
`../formats/games/` and compares the transcript with a hash in `scripts.tsv`.
210 replays, about ten seconds.

Where `formatcheck` shows that the loaders still produce the same tables, this
shows that the interpreter still does the same things with them: the parser,
the action engine, the game-specific code and the text output, over whole
playthroughs and on every platform's wording of the game.

## What is compared

The transcript is game text, so only its SHA-256 and line count are kept. To
see what changed, save the transcripts of two builds and diff them:

    test/scripts/run_script_tests.py --save /tmp/before
    test/scripts/run_script_tests.py --save /tmp/after --hl <other scott_hl>
    diff -r /tmp/before /tmp/after

`--list` prints how each replay ends, `--bless` rewrites the hashes. CheapGlk's
"Glk library error" lines (window calls it cannot honour) are dropped before
hashing.

`scott_hl` forces the fixed-seed random generator (see `../README.md`), so a
replay is repeatable.

## How a script is read

One command per line. Three things are not commands:

- **Start-up keys.** What a release asks before the first prompt (the menu of
  a compilation disk, "restore a saved game?") is answered by the runner, from
  the manifest row and from the optional fifth column of `scripts.tsv`, so the
  script itself starts at the first command. The TI-99/4A images need `_N`.
- **Key pauses.** The runner sets `SCOTT_SCRIPT_KEYS`, which makes CheapGlk
  answer a `<HIT ENTER>` pause from the script as the app would. A pause
  swallows the whole next line, so a command that pauses is followed by an
  empty line. On a release that does not pause there, the empty line is an
  empty command ("Huh?") and costs no turn; that is what lets one script run
  on every release.
- **Idle turns.** Where a script has to wait, `LOOK` takes a turn and
  `INVENTORY` does not. German Gremlins has no `LOOK`; `laufen`, which it does
  not understand, takes a turn all the same.

Seas of Blood's dice combat cannot run headless at all.

## The scripts

Two sources. The plain names were written by hand in Spatterlight for one
release of each game, before the random generator was replaced. The `bunyon-`
ones are the walkthroughs that come with Bunyon 0.3 (`bunyon-0.3/scripts`),
written for the TI-99/4A releases; they also win on many of the others.
`gremlins-german` is the walkthrough on the C64-Wiki page
(c64-wiki.de/wiki/Gremlins_–_The_Adventure).

A replay that does not finish is still deterministic, and that is all a
regression test needs, but it covers less of the game.

| script | ends | notes |
| --- | --- | --- |
| `adventureland` | won, all 16 | the Bunyon route made portable: empty lines after the genie, `scream bear`, `unlock door`, the mud picked up twice |
| `adventureland-c64` | won, both | the C64 release has no tunnel before the bees and no endless corridor, so two moves fewer |
| `arrow-of-death-1`, `-2` | won | all 8 and all 7 |
| `bunyon-adventureland` | won, all 3 | `make holes` added after `get bees`: with the fixed seed they suffocate otherwise |
| `bunyon-ghost-town` | won, all 3 | |
| `bunyon-golden-voyage` | won, both | |
| `bunyon-mission-impossible` | won, all 13 | the saboteur dies one room earlier with this seed, so the detour to find him is shorter |
| `bunyon-mystery-fun-house` | won, all 3 | |
| `bunyon-pirate-adventure` | won, both | TI-99/4A only; the other releases differ |
| `bunyon-pyramid-of-doom` | won, both | `get glove` added where it slips off |
| `bunyon-savage-island-1` | dead | random nearly every turn: the hurricane and the bear |
| `bunyon-savage-island-2` | won | |
| `bunyon-strange-odyssey` | won, all 6 | |
| `bunyon-the-count` | lost | as shipped; the bell and nightfall come on other turns than the script expects |
| `bunyon-voodoo-castle` | won, both | |
| `claymorgue` | won on 17 of 18 | one Atari image ends in the star room |
| `feasibility-experiment` | won on C64 | the ZX images end mid-game |
| `golden-baton` | won on C64 | dead on ZX (the Gorgon) |
| `gremlins` | won on 6 of 8 | English only; the "alternate" ZX release stops one command short |
| `gremlins-german` | won, all 4 | C64 and ZX. Changed from the wiki: `holen` for `nehmen`, which the game does not know; `anzeige` for `ladentisch`, which only the C64 knows; `taste druecken` once more at the pool; `laufen` for the waits |
| `hulk` | open | none finish |
| `robin-of-sherwood` | open | stuck at the locked grating, all 8 |
| `seas-of-blood-c64`, `-zx` | open | stops at the first battle |
| `secret-mission` | won, both | C64 only; every other release is covered by `bunyon-mission-impossible` |
| `spider-man` | open | none finish; it is a script for the Plus release and wins there (`terps/plus/test/scripts`) |
| `super-gran` | won | all 4 |
| `ten-little-indians` | won | 4 of 5 |
| `the-count` | lost | all 13 |
| `voodoo-castle` | won | all 15 |
| `waxworks` | dead | all 6 |
| `wizard-of-akyrz` | won | all 6 |

Scripts for the other two Scott Adams interpreters are in
`terps/taylor/test/scripts/` and `terps/plus/test/scripts/`.
