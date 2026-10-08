# Command script replays

`make -f Makefile.headless scriptcheck` (from `terps/scott`) plays each command
script in this folder through `scott_hl` on every image of its game in
`../formats/games/` and compares the transcript with a hash in `scripts.tsv`.
243 replays, about ten seconds.

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

## The old random numbers

The plain-named scripts (see below) were written in the app with the
Determinism option on, when that meant `srand(1234)` and the C library's
`rand()`. In February 2026 the interpreter got the generator of Glulxe
instead. Which turn the bell tolls on in The Count, when Banner turns into the
Hulk, where a ship attacks in Seas of Blood: all of that is decided by the
random numbers, so a script written for the old ones wanders off with the new.

For the scripts listed as `OLD_RANDOM` in `run_script_tests.py` the runner
therefore sets `SCOTT_OLD_RANDOM`, and `scott_hl` hands the interpreter the
numbers of old (`../old_random.c`; a test build only, the app is not changed).
Those are the scripts that play to the end that way: `hulk`,
`robin-of-sherwood`, both `seas-of-blood`, `the-count` and `waxworks`. The
other plain-named ones do as well or better with today's numbers, some because
they were written later, some because they have been adapted since.

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
  not understand, takes a turn all the same. Spanish Gremlins has `mirar`;
  a word it does not understand takes no turn there.

Seas of Blood's dice are thrown by timer and key events, which CheapGlk does
not have. `scott_hl` is built with `AUTOWIN` (`ai_uk/seas_of_blood.c`), as the
app was when the two scripts were written: a battle is won without a roll, and
takes only the `<HIT ENTER>` before it. So the dice code itself is not run
here.

## The scripts

Two sources. The plain names were written by hand in Spatterlight for one
release of each game, before the random generator was replaced. The `bunyon-`
ones are the walkthroughs that come with Bunyon 0.3 (`bunyon-0.3/scripts`),
written for the TI-99/4A releases; they also win on many of the others.
`gremlins-german` is the walkthrough on the C64-Wiki page
(c64-wiki.de/wiki/Gremlins_–_The_Adventure).
`knight-ironheart` was worked out from the game's own action table.

`gremlins-german-verb-first` is that walkthrough again with the verb moved to
the front of every command: `gehen nach unten` for `nach unten gehen`,
`holen saebel` for `saebel holen`, and `lass saebel fallen` and
`fallen lassen saebel` in turn for `saebel fallen lassen`. The game's
own examples have the verb last, but the original takes it in either place
(the ZX Spectrum release in MAME: `holen saebel`, `angreifen gremlin`,
`gehen nach oben`, `lass saebel fallen` and `fallen lassen saebel` all do what
the verb-last forms do), and so does the interpreter (`CommandFromStrings()`
in `parser.c`). So besides its own hashes this script
has a second check (`SAME_AS` in `run_script_tests.py`): apart from the lines
that echo the commands, its transcript has to be that of `gremlins-german` on
every image. A command that is understood in one word order only fails the
test by name, and `--bless` does not get it past that.

A replay that does not finish is still deterministic, and that is all a
regression test needs, but it covers less of the game.

| script | ends | notes |
| --- | --- | --- |
| `adventureland` | won, all 14 | the Bunyon route made portable: empty lines after the genie, `scream bear`, `unlock door`, the mud picked up twice |
| `adventureland-c64` | won, both | the C64 release has no tunnel before the bees and no endless corridor, so two moves fewer |
| `antica-grecia` | won | the Italian Perseus and Andromeda: `perseus-and-andromeda` put into Italian word for word |
| `arrow-of-death-1`, `-2` | won | all 8 and all 7 |
| `bunyon-adventureland` | won | `make holes` added after `get bees`: with the fixed seed they suffocate otherwise |
| `bunyon-ghost-town` | won, all 3 | the bonus score is -81 on the TI-99/4A, whose counters go below -1, and -1 elsewhere |
| `bunyon-golden-voyage` | won, both | |
| `bunyon-mission-impossible` | won, all 11 | the saboteur dies one room earlier with this seed, so the detour to find him is shorter |
| `bunyon-mystery-fun-house` | won, all 3 | |
| `bunyon-pirate-adventure` | won, both | TI-99/4A only; the other releases differ. A `look` after `give rum` and a `wait` before `set sail`, for the pirate and the tide |
| `bunyon-pyramid-of-doom` | won, both | `get glove` added in the two places where it slips off |
| `bunyon-savage-island-1` | dead | random nearly every turn: the hurricane and the bear |
| `bunyon-savage-island-2` | won | |
| `bunyon-strange-odyssey` | won, all 4 | |
| `bunyon-the-count` | lost | as shipped; the bell and nightfall come on other turns than the script expects |
| `bunyon-voodoo-castle` | won, both | |
| `circus-c64`, `-zx` | won, all 5 | the solution of Jacob Gunness (solutionarchive.com) as written. The seal of the C64 release is a sea-lion on ZX, so one command differs |
| `claymorgue` | won on 17 of 18 | one Atari image ends in the star room |
| `escape-from-pulsar-7` | won, all 6 | C64 and ZX. The solution of Jacob Gunness as written; the C64 release has no watch to drop |
| `feasibility-experiment` | won on C64 | the ZX images end mid-game |
| `golden-baton` | won on C64 | dead on ZX (the Gorgon) |
| `gremlins` | won on 6 of 8 | English only; the "alternate" ZX release stops one command short |
| `gremlins-german` | won, all 4 | C64 and ZX. Changed from the wiki: `holen` for `nehmen`, which the game does not know; `anzeige` for `ladentisch`, which only the C64 knows; `taste druecken` once more at the pool; `laufen` for the waits |
| `gremlins-german-verb-first` | won, all 4 | the same commands with the verb first; has to play out as `gremlins-german` does |
| `gremlins-spanish` | won, all 4 | C64 and ZX. The English script put into Spanish word for word, but: the camera button is pressed as soon as the gang is at the pool, and `mirar` for the waits |
| `hulk` | won on 14 of 18 | old random numbers. The two C64 and two of the four ZX images end in limbo: Banner is gassed on another turn there |
| `knight-ironheart` | won | TI-99/4A home-brew (M. Weiand, Tex-Comp 1984). The full ending: the princess and the bag of diamonds, then `say yes`. No random event fires on the way; the open trapdoor and the sandstorm could |
| `perseus-and-andromeda` | won, all 7 | C64 and ZX. The solution of Jacob Gunness (solutionarchive.com) as written |
| `robin-of-sherwood` | won, all 8 | old random numbers |
| `seas-of-blood-c64`, `-zx` | won | old random numbers and `AUTOWIN`. Each wins on its own platform (2 C64 images, 4 ZX) and gets lost on the other |
| `secret-mission` | won, both | C64 only; every other release is covered by `bunyon-mission-impossible` |
| `spider-man` | open | none finish; it is a script for the Plus release and wins there (`terps/plus/test/scripts`) |
| `super-gran` | won | all 4 |
| `ten-little-indians` | won | 4 of 5 |
| `the-count` | won on 11 of 13 | old random numbers. Lost on the TI-99/4A image and one Apple II disk |
| `time-machine-c64`, `-zx` | won, all 5 | the solution of Jacob Gunness. Where PRESS FOR lands is random until the three prisms are in, so each script has the number of presses that the random numbers of its release take |
| `voodoo-castle` | won | all 15 |
| `waxworks` | won, all 6 | old random numbers |
| `wizard-of-akyrz` | won | all 6 |

The TI-99/4A images (`fiad-adv*` and the `phd50*` disks) run every implicit
action of the game, which costs one more random draw per turn than the other
releases, so the random events come on other turns there. Four scripts have a
`-ti99` copy that those images replay in place of the shared one:

| Script | Outcome | Differs from the shared script by |
| --- | --- | --- |
| `adventureland-ti99` | won, both | a second `get bees` (they suffocate on the turn they are bottled) and a `look` before the meadow |
| `bunyon-adventureland-ti99` | won, both | `get mud`, `drop mud` after `get gas` to cure the chigger bites, and three `look`s before `drop bees` |
| `bunyon-mission-impossible-ti99` | won, both | the saboteur dies in the yellow room on the first `wait`, so the route fetches his picture there |
| `bunyon-strange-odyssey-ti99` | won, both | a `look` before `wake hound` |

Scripts for the other two Scott Adams interpreters are in
`terps/taylor/test/scripts/` and `terps/plus/test/scripts/`.
