# Synthetic tests for the Plus interpreter

    make -f Makefile.headless synthcheck

The walkthrough replays in `../scripts` only run what the six games happen to
do on the way to their endings. The opcodes, system commands and condition
guards they never reach are run here instead, on a database that is not a
game: `mkdatabase.py` writes it, and it is in the repository
(`games/rare-opcodes/SYNTH.DAT`), so this suite runs on a bare checkout.

| File | |
|---|---|
| `mkdatabase.py` | the source of the databases and of `games.manifest.tsv`; `--check` fails if any is stale |
| `games/rare-opcodes/SYNTH.DAT` | the database, in the plain text format of the MS-DOS games. Its title is `X-MEN `, because the loader only takes titles it knows and X-Men has no pictures to miss |
| `games/loader-edges/EDGES.DAT` | the same database with a verb group that has no words and a message of 1100 characters added at the end |
| `games/prefix-title/PREFIX.DAT` | the same database titled `X-ME`, the start of a title the loader knows |
| `rare-opcodes.txt` | the commands: one test each, mostly `TEST <name>` |
| `loader-edges.txt`, `prefix-title.txt` | the commands for the two variants |
| `fatal-*.txt` | one script for each error that stops the interpreter |
| `scripts.tsv` | line count and SHA-256 of each transcript, as in `../scripts` |

## What is covered

Not reached by any walkthrough before this, now run:

- actions 52 with full hands, 61, 70, 71, 86, 88, 103, 105, 106, 107, 108,
  118, the unknown-opcode default, and 110 with a two-byte second operand;
- system commands 110-1 (answered yes), 2, 4, 5, 8, 9, 10, 11, 12 in all four
  modes with the rescan of mode 3, and the unknown ones;
- 115 inside a nested loop, and the 116/117/118 combinations;
- the out-of-range guards of conditions 1, 2, 3, 5, 13, 14, 18 and of the
  others, and of the player's room (a GOTO to a room that does not exist
  does nothing, while a counter command may write anything into counter 32,
  `TEST BADARGS`), logic flags 2 (XOR), 9, 10 and an unknown one, and how
  results
  are combined (`TEST GROUPS`: an OR that comes too late, groups, a negated
  group, 31.8);
- the operands 997 (the "with" list) and 999 (second noun), and the two-byte
  form of 998 on the opcodes that read their own last operand (62, 72, 75,
  80, 110), a room number of two bytes (`BANISH`, X-Men's "room 500") and
  the two-byte form of a small number (`TEST PROMPT4`), and of an opcode, a
  counter, a value and one of the first 51 messages (`TEST WIDE`); a message
  number beyond the last message, which prints nothing (`TEST UNKNOWN`);
- the fatal errors: conditions nested more than five parentheses deep, a
  closing parenthesis without an opening one, more than 32 nested loops,
  and a room that does not exist left in counter 32 (`fatal-room`);
- in the plain text loader: an empty dictionary slot, which is a group
  without words, so that the verb after it keeps its number (`LATER`); a
  message longer than the 1000 characters kept of a string, of which the
  rest is dropped while the next message is still read from the right place
  (`TEST LONG`); and a title that is only the start of a known one, which is
  not taken for that game (`prefix-title`: the interpreter says it cannot
  tell what game it is).

The drawing commands (110-2, 4, 5, 8, 10, 11) are only shown not to crash
without pictures. What they draw is the business of `imagecheck`.

## Reading a result

The transcripts say what they expect:

    ABS expect 5 7 got 5 7
    GUARDS expect 1 2 3 5 55 8 ... got 1 2 3 5 55 8 ...

so a failure can be read rather than guessed at:

    python3 ../../../common_imagetest/run_script_tests.py . . ../../plus_hl --save /tmp/out

The hashes pin what this interpreter does today. The opcodes and conditions
were afterwards read against the MS-DOS interpreters of X-Men, Fantastic
Four and Spider-Man, and what differed was changed (`REFACTOR-TODO.md`,
I4, which also lists what was left different). The tests themselves were
never run on an original interpreter.

## Adding a test

Add the actions to `mkdatabase.py`, the command to a script, then

    python3 mkdatabase.py
    python3 ../../../common_imagetest/run_script_tests.py . . ../../plus_hl --bless

An action holds at most 16 command bytes; `pack()` splits longer runs over
continuation lines. An error that ends the run needs a script of its own.
