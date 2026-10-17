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

## What the October 2026 pass settled

The source was reviewed and reworked on 2026-10-11/12 with the trace compare
above as the oracle: the trace of all 90 samples was identical before and
after every change that was not meant to alter emulation. Behaviour that
changed, and that a caller or a reader of a diff might otherwise take for a
bug:

* A switch string that cannot be parsed, a fifth switch, or a switch of 64
  characters or more fails the `unp64()` call instead of being ignored. The
  registry strings of the four callers (`c64unpack.h`, the Scott and Taylor
  `c64decrunch.c`, `unquill.c`) are all well-formed.
* `-c`, and the FinalSuperCompressor scanner's own request for another pass,
  now load the output of the pass before and not the original input again.
  Before, `-c` ran the same unpack sixteen times and returned what one pass
  returns. If a later pass fails, the output of the last good pass is kept
  and the call succeeds. `run_tests.py --chain` pins this.
* `SAX` (undocumented `$83/$87/$8f/$97`) only stores A & X; it used to leave
  the result in A as well. `RRA` rotates right, not left. No sample runs
  either, so the traces did not move.
* The 14-entry fake keypress table wraps instead of reading one past its end.
* The emulated memory is 64 KiB plus 0x1000 of zeroed slack, because the
  scanners compare up to `p + 0x3xx` with `p` anywhere in memory.
* `UNP64_OUTPUT_CAPACITY` (0x10000) in `unp64_interface.h` is the most
  `unp64()` can write: 2 bytes of load address plus memory from `$0002` up.

## Known gaps

* No caller passes `-c`, and no sample is the first layer of a
  FinalSuperCompressor file, so the three-pass protocol described at the top
  of `scnFinalSuperComp` is reached only by `run_tests.py --chain`, which
  drives it from outside.
* The write intercept for the I/O page (`isAbsoluteWriteOpcode` in
  `6502_emu.cpp`) recognises `STA (zp),y` but not `STA (zp,x)`, `SAX` or the
  read-modify-write illegals, so a depacker using those writes straight into
  $D000 RAM instead of having the store dropped. No sample does. Driving the
  list from the opcode table would close the gap but changes which stores
  are suppressed, so it needs a sample that shows the difference first.
* The scanners patch the program in memory in some 68 places (rewriting a
  SYS line, planting a trampoline, disabling a raster wait), and only
  `final_super_comp.cpp` says why. The others are what the depacker's
  original code would have needed from I/O, the KERNAL or the raster that
  the emulator does not provide. When a scanner is touched for another
  reason, say so next to the patch.
* `_depAdr == 0` guards in front of scanner blocks are not redundant with
  returning after a match: several blocks set `_depAdr` without `_idFlag`
  and fall through on purpose.

## Declined

* Rewriting the scanner signatures as a table or a data file. Most match
  bodies are reached by no sample, so nothing could show a rewritten
  signature still matches, and the bodies contain loops, XOR decoding and
  memory patches that a data format would need a script layer for.
* Replacing the hand-written 6502 with a third-party core. This one is wrong
  in the ways the depackers need: no ROMs, intercepted I/O, undocumented
  opcodes as the original UNP64 implements them, cycle counts only for the
  raster shadows.
* Merging the two KERNAL-stub phases of `emulateUntilDepack` and
  `emulateUntilReturn` further than `bankBits()` and `stubKernalState()`.
  Phase 1 stubs before the step and phase 2 after it, with different sets of
  entry points, so a shared function would be mostly parameters.
* Splitting `cruel.cpp` and `intros.cpp` the way `time_cruncher.cpp` was
  split into `tryTimeCruncher4/5/3/Clones`. At 350 and 429 lines they do not
  need it.
* Making `UnpStr` a class with accessors. The scanners write its fields
  directly about 1,500 times; the struct is the protocol, described in the
  comment above it in `unp64.h`.
* Renaming `exo_util`. The header comment says what the name means now.
