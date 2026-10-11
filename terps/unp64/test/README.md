# unp64 regression tests

unp64 identifies a packed Commodore 64 program, runs its depacker in a 6502
emulator and returns what the depacker left in memory. These tests check that
it still returns the same bytes for every packed program the Scott and Taylor
interpreters hand it.

    make check      unit tests, every sample against expected.tsv, and -c
    make bless      rewrite expected.tsv from what the samples unpack to now
    make trace      one line per sample: a hash of every emulated CPU step
    make capture    collect the samples (needs the Scott and Taylor images)
    make clean

## Samples

A sample is one program that `unp64()` was called with, and the switches it
was called with: `samples/<id>.prg` and, if there were switches,
`samples/<id>.sw`. The id is a hash (FNV-1a) of the switches and the
program. `samples/log.tsv` says which disk or tape image each one came from.

The samples are cut out of copyrighted games, so `samples/` is git-ignored.
Rows of `expected.tsv` whose sample is missing are skipped: on a bare checkout
`make check` runs the unit tests and passes without unpacking anything.

`make capture` (capture.sh) collects them. It builds the format probe of
`terps/scott` and the image probe of `terps/taylor` with `capture_unp64.cpp`
in the place of `unp64.cpp`, which saves every input before unpacking it, and
runs both suites. That needs the game images those suites use, which are not
in the repository either. The two probes are rebuilt without the capture
afterwards.

A packed game is often packed more than once. The callers call `unp64()` on
its own output until it fails, so every layer is a sample of its own, and the
last one is a row that says `fail`: that is how the caller finds out that
there is nothing left to peel off.

## What is checked

`unp64_unit` tests the parts that need no game: the BASIC SYS line parser,
the number parser of the switches, input that is no program, and switch
strings that cannot be parsed.

`run_tests.py` runs `unp64_probe` on every sample and compares the result,
the length and the SHA-256 of the output with the row in `expected.tsv`.
Name one or more id prefixes to run only those.

`run_tests.py --chain` checks the `-c` switch, which makes `unp64()` unpack
its own output until that fails: the result has to be what calling the probe
again and again gives.

## Changing unp64

After a change that should not alter what the emulator does, compare traces:

    make -s trace > before.tsv      # twice, if it had to build first
    ...
    make -s trace > after.tsv
    cmp before.tsv after.tsv

`unp64_trace` is the probe built with `-DUNP64_TRACE`, which hashes the
program counter, the registers, the flags, the cycle count and the byte at
$01 in front of every emulated instruction. Two identical traces mean the
same instructions ran in the same state, which is a much finer test than the
output alone: most of what a depacker computes on the way never reaches it.

After a change that is meant to alter the output, look at what changed with
`make check`, and when the new output is right, `make bless`.

The scanners match on the bytes of some ninety depackers and their variants,
and the samples run only a part of them (`make -s trace` shows which succeed).
A scanner that no sample reaches can only be changed by inspection.
