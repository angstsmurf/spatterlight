# terps/unp64: refactoring, cleanup and deduplication TODO

Read-only review, 2026-10-11, every claim re-verified against the tree the
same day (counts and line numbers corrected where they were off). Effort:
XS/S/M/L. Risk: none/low/medium/high.

**Worked through 2026-10-12: see "Status" right below.** The sections after it
are the review as it was written, kept for the reasoning; their line numbers
and counts describe the tree before the work.

Callers of `unp64()` (C ABI): `terps/unp64/c64unpack.h`,
`terps/scott/ai_uk/c64decrunch.c`, `terps/taylor/c64decrunch.c`,
`terps/unquill/unquill.c` (which re-declares the prototype itself instead of
including `unp64_interface.h`). Both headless Makefiles
(`terps/scott|taylor/Makefile.headless`, `UNP64_DIR := ../unp64`) and the
Xcode `unp64` dylib build these files, so any file add/remove/rename touches
all three.

Validation after each batch (there is **no test of its own in this directory**,
see 6.1): `make -f Makefile.headless check formatcheck` in `terps/scott` and
`terps/taylor` (the C64 .d64/.t64 rows in formatcheck are the only coverage),
`terps/unquill` C64 rows, plus the Xcode `unp64` target. Every change below
that touches emulation must leave the unpacked bytes of the whole C64 corpus
byte-identical (see 6.1 for a cheap way to pin that down first).

---

## Status (2026-10-12)

Checked with `make check` in `test/` (unit tests; 90 samples against
`expected.tsv`; `-c` against repeated calls), the CPU trace of all 90 samples
identical to the one taken before any change, `check` and `formatcheck` in
`terps/scott` (288 format rows), `imagecheck` in `terps/taylor` (58), and
Xcode builds of the `unp64`, `unquill`, `scott` and `taylor` targets. Changes
said to be "identical code" below were compared as `otool -t` of every
object at `-O2`, before and after.

| Item | State | Notes |
|---|---|---|
| 6.1 | done | `test/`: `unp64_probe`, `unp64_trace`, `unp64_unit`, `run_tests.py`, `capture.sh`; see `test/README.md`. Samples are captured from the Scott and Taylor images and git-ignored. |
| 1.1 | done | `op_rra` calls `op_ror`. No sample runs an RRA. |
| 1.2 | done | Index wraps with `%`. |
| 1.3, 1.3b | done | `InstArg arg[1] = {}`; `CpuCtx` has default initialisers, the X case is commented in `seedC64State`. |
| 1.4 | done | `-l` loop bounded; `_lfMemF = _ecaFlg`. |
| 1.5 | done | Memory is `kMemSize + kMemSlack` (0x1000 of slack, not 0x100: the scanners read up to `p + 0x3xx`). |
| 1.6 | done | `loadData` returns `bool`; under 2 bytes fails. |
| 1.7 | done | `strMem` clamped. |
| 1.8, 3.8 | done | **A switch that cannot be parsed, a fifth switch, or a switch of 64 characters or more now fails the call** instead of being dropped. Every registry string in the four callers is well-formed. |
| 1.9, 1.11 | done | |
| 1.10 | done | Dead `_idFlag == 2` branch gone; `pu_crunch.cpp` and the TimeCruncher 5 chain use a local `bool found`. |
| 1.12 | done | `<stddef.h>`, `const uint8_t *compressed`, `unquill.c` includes the header. |
| 1.13 | done | Documented in the header as `UNP64_OUTPUT_CAPACITY` (0x10000). **The 65538 in the review is wrong:** the output is 2 + (`endAdr` − `strMem`) with `strMem` ≥ 2, so 65536 at most, and `-a` on every sample gives exactly that. No capacity parameter was added. |
| 2 | done | `Globals`, `globals.{h,cpp}` gone; `UnpStr` and `CpuCtx` live on the stack of `unp64cpp`. |
| 3.1, 3.2, 3.4, 3.5, 3.6, 3.7 | done | |
| 3.3 | partly | `bankBits()` names the ROM test; `stubKernalState()` holds the SETNAM and IOINIT stubs both phases shared. The rest is not shared: phase 1 stubs before the step and phase 2 after it, with different sets, so a common `emulateRomEntry` would be mostly parameters. |
| 3.9, 4.2 | done | `types.h` gone, the readers are in `exo_util.h`. `sizeof`, not `std::size`: the Scott headless build compiles without `-std=c++17`. |
| 4.1 | done | |
| 4.3 | done | The zlib notice and the credits are in `LICENSE`; each of the 30 source files keeps its copyright lines and a pointer to it. `unp64_interface.h` never carried the notice and is unchanged. |
| 4.4 | done | `scanners/scanners.h` declares the 22 scanners; the table in `scanners.cpp` is `const` with a comment on its order. |
| 4.5 | done | Protocol comment above `UnpStr`. `_name`, `_r`, `_debugP` removed; `_idOnly` kept. |
| 5.1 | partly | The `if (unp->_idFlag) return;` at the top of each scanner is gone. **The `_depAdr == 0` guards stay:** several blocks set `_depAdr` without `_idFlag` and fall through on purpose, so the guard is not equivalent to return-after-match. |
| 5.2 | **declined** | Most match bodies are not reached by any sample, the compiled code would change, and nothing could show that a rewritten signature still matches. |
| 5.3 | done | 110 adjacent reads became `READ_LE_UINT16`, 37 split ones `readSplit(mem, lo, hi)`. About 100 of the review's 147 were in trailing comments that repeated code already converted; those comments are deleted. |
| 5.4, 5.5 | done | `forceEntryIfNoSys`, `setBasicReturn`, `kBasicRun` in `scanners.h` / `unp64.h`. Identical code. |
| 5.6 | done | Tabs, one prologue (`uint8_t *mem`, `int q, p;`). Identical code. `q`/`p` are still declared at the top. |
| 5.7 | **left** | No patch comments added, beyond the three-layer comment in `final_super_comp.cpp`. |
| 5.8 | done for `time_cruncher.cpp` | `tryTimeCruncher4/5/3/Clones`; the `_debugP` branch is deleted. `cruel.cpp` and `intros.cpp` not split. |
| 5.9 | done, **and a bug fixed** | See below. |
| 6.2, 6.3 | done | Table of function pointers; `fetchOperand`, `rmwTarget`. Trace identical. |
| 6.4 | not needed | Both headless builds compile with `-w`, and Xcode prints nothing for these files. |
| 6.5 | **declined** | It would change which stores to the I/O page are dropped (`$81`, SAX, the RMW illegals), which is a change of behaviour that the samples give no way to judge. The constants in `interceptIORead` are still literals. |
| 6.6 | done | Plus one more, found while reading: **`op_sax` left A & X in A**, which a real 6510 does not. Fixed: it now only stores. The trace of all 90 samples is unchanged, so none of them runs a SAX. |
| 7.1, 7.3 | done | Unit tests for `findSys` and `strToInt` in `test/unp64_unit.cpp`. |
| 7.2 | left | The name stays. |

**The recursion bug (5.9).** A second pass, whether from `-c` or from
`_recurs = RECUMAX - 2` in the FinalSuperCompressor scanner, loaded the
*original* input again and not the output of the pass before. So `-c` ran the
same unpack sixteen times and returned what one pass returns, and the
FinalSuperCompressor layers could never be reached that way (the scanner would
have reset the counter on every pass). Now a later pass loads the output of
the one before, starts from a clean `UnpStr`, CPU and memory, and if it fails
the output of the last good pass is kept and the call succeeds. `RECUMAX - 2`
is "three passes in all"; the protocol is described at the top of
`scnFinalSuperComp`. `run_tests.py --chain` checks that `-c` gives what
calling `unp64()` on its own output gives, for all 90 samples (37 have more
than one layer). No caller passes `-c`, and no sample is FinalSuperCompressor
layer 1, so that path itself is still untested.

Xcode: `globals.cpp`, `globals.h` and `types.h` are out of `project.pbxproj`,
`scanners.h` is in.

---

## 1. Bugs and risks (fix first; each is small)

| # | Where | Problem | Effort | Risk |
|---|---|---|---|---|
| 1.1 | `6502_emu.cpp` `op_rra` | Calls `op_rol` then `op_adc`. RRA is **ROR** then ADC. Looks like a copy-paste from `op_rla`. Only matters if a packer runs an undocumented `$63/$67/$6f/$73/$77/$7b/$7f` opcode, so the corpus probably never hits it; confirm with a trace before and after (6.1). | XS | low |
| 1.2 | `unp64.cpp:386` | `if (_keypressIdx > ARRAYSIZE(fpressedchars)) _keypressIdx = 0;` should be `>=`. With 14 entries the index 14 is read, one past the end of the static array (value is whatever follows it in .rodata, so GETIN returns a junk key). Fix: `idx = (idx + 1) % ARRAYSIZE(...)`. **This changes emulation once the 15th key is asked for**, so check the corpus. | XS | low |
| 1.3 | `6502_emu.cpp` `nextInst` | `InstArg arg[1]` is uninitialised and `arg->_ea._value` is read for the I/O-page test even for implied/accumulator/immediate/relative modes (those set 0 or 1 byte of the union). Harmless today only because every intercepted opcode is absolute, but it is a read of indeterminate bytes. Zero-initialise `arg`, or compute `targetsIO` only for the modes that produce an effective address. | XS | none |
| 1.3b | `unp64.cpp` `unp64cpp` / `seedC64State` | `CpuCtx r[1]` (line 615) is never zeroed, and `seedC64State` sets `_a`, `_y`, `_sp`, `_flags`, `_pc` but only sets `_x = 0` when `info->_run > 0x351` (line 347, comment "temporary for XIP"). For any entry point at or below `$351` the depacker starts with whatever X was on the stack, so the run is not deterministic across builds or callers. Also `_cycles` is set but no other field is pre-cleared. Fix: `CpuCtx r = {};` (or `memset`) and make the XIP special case an explicit "leave X as the SYS evaluator left it" value rather than an omission. Check the corpus: if any game has a sub-`$351` entry and currently depacks, it is doing so with X indeterminate. | XS | low |
| 1.4 | `unp64.cpp` `-l` block, ~line 837 | `oldmem[--_lfMemF]` has no lower bound. `_lfMemF` starts at `info->_end`; if every byte from $FFFF down matches the snapshot (the area above the program is all zero in both), the index passes 0 and reads `oldmem[-1]` (heap, before the buffer). Guard with `_lfMemF > 0`. Also `_lfMemF = 0 \| _ecaFlg` (line 841) is a roundabout `= _ecaFlg`. | XS | low |
| 1.5 | emulator and scanners | `mem` is exactly 64 KiB. `u32eq(mem + p + 0x36, …)` with `p` from `_info->_run`/`_start` (up to $FFFF), `mode_abs` reading `mem[pc + 2]` at `pc == $FFFF`, `mem[p + 1]` in scanners etc. all read past the end. Cheapest fix: allocate `65536 + 0x100` (zeroed) in `unp64cpp` and `loadData`'s contract (`uint8_t mem[65536]` in `exo_util.h` is a lie as a parameter type anyway, it decays to a pointer). No scanner needs to change. | XS | none |
| 1.6 | `exo_util.cpp` `loadData` | Reads the 2-byte load address and does `dataLength - 2` (size_t) with no length check; a 0/1-byte input underflows and `memcpy`s `len` bytes. Add `if (dataLength < 2) return false/…` and let `unp64cpp` fail cleanly. | XS | none |
| 1.7 | `unp64.cpp` `writeUnpackedOutput` | `mem[strMem - 2]` with `strMem < 2` (possible via `_fStrBf`/`_fStrAf` reading 0 or 1 out of RAM) indexes before the buffer. `endAdr` is clamped but `strMem` is not. Clamp `strMem` to `[2, 0xffff]` next to the existing `_endAdr` clamps. | XS | low |
| 1.8 | `unp64.cpp:688-690` | `strToInt(settings[0] + 2, (int *)&_G(_unp)._filler)` casts `unsigned int*` to `int*`. Make `_filler` an `int`, or parse into a local. Return value of `strToInt` is also ignored in every switch case, so `-eXYZ` silently becomes "0, keep default"; decide whether that is intended and document it. | XS | none |
| 1.9 | `unp64.cpp` `applySwitches` | `case 'm'` has no `break`/comment and relies on being last; add the `break`. Cases `x`, `B`, `K` are empty or commented-out stubs, delete them (a switch with unknown letters already falls out). | XS | none |
| 1.10 | `unp64.cpp` scanners return | `scanners()` can return `_idFlag == 2` ("not a packer, stop"), but **no scanner sets 2** (`grep "_idFlag = 2"` is empty). Either drop the dead branch and the "2=not a packer" comment, or document which scanner is meant to. Related: `pu_crunch.cpp:153/162` uses `_idFlag` as scratch (`= 0`, then `= 385` as a "version found" tag, tested by the `if (unp->_idFlag)` at line 166), and the "TimeCruncher 5" chain (`time_cruncher.cpp:199-261`) sets `_idFlag = 1` at the top of each branch and tests it afterwards to decide whether to run the patch loop; give both a local `bool found` so `_idFlag` only ever holds 0/1 and is set last, as 5.1 assumes. | XS | none |
| 1.11 | `unp64.cpp` bit-or in condition | `if (_wrMemF \| _lfMemF)` is a bitwise or of two ints used as a boolean; write `\|\|`. | XS | none |
| 1.12 | `unp64_interface.h` | Uses `size_t` but includes only `<stdint.h>` (relies on the includer having pulled in `<stddef.h>`), and takes `uint8_t *compressed` non-const although the callee never writes to it (the C++ side already takes `const uint8_t *`). The `extern` on a function prototype is redundant. Include `<stddef.h>`, make `compressed` `const`, and have `unquill.c` include the header instead of its own `extern` (its prototype uses `uchar`). | XS | low |
| 1.13 | `unp64.cpp` C wrapper | The wrapper hard-codes the output capacity 0x10000 and "assumes the caller allocated 64 KiB" (documented). Max real output is `endAdr - strMem + 2` = 65538 bytes (`strMem == 2`, `endAdr == 0x10000`, i.e. the `-a` switch), which overruns a 0x10000 buffer by 2 bytes: `writeUnpackedOutput` already rejects it, so `-a` of a full image fails rather than overflows. Either document the 65538 maximum in the header, or add a `size_t destCapacity` parameter to the C ABI (4 callers). Not a bug, but surprising. | S | low |

---

## 2. Global state: `Globals` / `_G(_unp)`

The `thread_local Globals *g_globals` + `_G(FIELD)` macro exists because the
ScummVM original had a singleton. Here it carries four things, and none of
them needs to be global:

* `_unp` (the `UnpStr`): scanners already receive `UnpStr *unp`; only
  `unp64.cpp` reaches for `_G(_unp)`, and it does so **~200 times**
  (`_G(_unp)._strMem`, …). The scanners never use `_G`.
* `_parsePar`: always 1, never assigned anywhere (`grep _parsePar`: one
  read). Dead, remove it and the `&& _G(_parsePar)` in the `-f` test.
* `_iter`: only ever a loop counter inside `emulateUntilDepack` /
  `emulateUntilReturn`, both of which reset it on entry. Make it a local.
* `_byted011[2]`, `_retfire`, `_retspace`: emulator-private raster/joystick
  shadows. They belong in `CpuCtx` (or a small `IoShadow` struct the CPU
  context points to), not in a process-wide singleton.

Plan (M, medium risk only in how much text it touches, none in behaviour):

1. Add `UnpStr &u = _G(_unp);` at the top of each function in `unp64.cpp`
   and replace the macro spellings mechanically (`_G(_unp)._x` -> `u._x`).
   Pass `UnpStr &` into `applySwitches`, `applyEndStartOffsets`,
   `seedC64State`, `emulateUntilDepack`, `emulateUntilReturn` instead of
   having each of them call `_G`.
2. Move the three emulator fields into `CpuCtx` and make `flipfire`/
   `flipspace` (currently **non-static, un-declared** free functions,
   `6502_emu.cpp:1117/1121`) static helpers on the context. `CpuCtx` is
   `memset`-free already; initialise `retfire = retspace = 0xff`,
   `d011 = {0,0}` in `seedC64State`.
3. Drop `Globals` and `globals.{h,cpp}` entirely, plus the `Globals globals;`
   in the C wrapper. `unp64cpp` then owns an `UnpStr` on the stack, which
   also makes the code reentrant without `thread_local` and removes the
   `ScopedInfoAlias` guard (the pointer lives only as long as the stack
   frame that owns `info`).

After this, `globals.cpp`/`globals.h` are gone and the thread-safety argument
in `globals.h` becomes "there is no shared state". Side benefit: those two
files are the only ones in the directory that carry a **GPL** (ScummVM)
header rather than the zlib notice, so removing them also removes the one
license oddity in an otherwise zlib-licensed tree (see 4.3).

---

## 3. `unp64.cpp` structure and duplication

3.1 **`applyEndStartOffsets` repeats one block six times** (A, X, Y for
`_endAdr`; A, X, Y for `_strMem`). Fold into a helper

```cpp
static int regPlusAdc(unsigned adc, int base, bool isStart, const CpuCtx *r);
```
or, simpler, loop over `{EA_USE_A, r->_a}, {EA_USE_X, r->_x}, {EA_USE_Y, r->_y}`
for each of the two accumulators. The start-address branch has the `EA_ADDFF`
twist (`$ff` wrap and `reg == 0` -> `+0x100`), the end-address branch does
not; keep those two as separate loops over the same register table. -40 lines,
and the `& 0xffff` noise goes. Effort S, risk low (covered by the corpus).

3.2 **Duplicated end/start clamp.** `_endAdr <= 0 -> 0x10000`, `> 0x10000 ->
0x10000`, `< _strMem -> 0x10000` appears twice back to back
(lines 866-874 and 882-889) around `applyEndStartOffsets`. Make it
`static void clampEndAddress(UnpStr &)`. XS, none.

3.3 **KERNAL stubbing is written twice with slight differences.**
`emulateUntilDepack` and `emulateUntilReturn` both hand-code "SETNAM stores
A/X/Y at $B7/$BB/$BC", "IOINIT sets $01 = $E7, A = $D7, X = $FF", "else
fake an RTS via `mem[0] = 0x60; pc = 0`". The conditions (what counts as
"ROM visible") are also spelled three different ways:
`((mem[1] & 7) >= 6 && pc >= 0xe000)`, `((mem[1] & 7) > 6)` for BASIC ROM,
`((mem[1] & 7) >= 6 && <= 7)` (which is `>= 6` as `& 7` is at most 7).
Extract `kernalRomVisible(mem)`, `basicRomVisible(mem)` and
`static void emulateRomEntry(CpuCtx*, uint8_t *mem, int pc)` (SETNAM,
IOINIT, RESTOR, CLRSCR, GETIN). Keep the **behavioural difference** between
the two phases explicit as a parameter or two small wrappers (phase 1 breaks
out on `$FFD5/$F4A2`, phase 2 also on `$EA31-$EB76/$FCE2`). Effort M, risk
medium: this is the region where one mis-merged condition silently changes
which depackers work, so do it with the trace harness from 6.1.

3.4 **Magic addresses.** `0xa7ae`, `0xa659`, `0xe536`, `0xffd2`, `0xfce2`,
`0xfda3`, `0xffbd`, `0x314`, `0x2b`..`0x32`, `0xae/0xaf`, `0x39/0x3a`,
`0x52/0x53` are bare. A page of `constexpr` names (`kBasicRunEntry`,
`kKernalClrscr`, `kZpTxtTab`, …) next to `isBasicRun1/2` makes both the
emulation stubs and `isBasicRun*` readable; the comments already say what
each is. XS-S, none.

3.5 **`isBasicRun1/2` and `reinitUnp` are non-static** (external linkage,
not declared in a header, but compiled into a dylib that exports them).
Make them `static` (or put them in an anonymous namespace) and `bool`.
Also `unp64cpp` itself has external linkage in `Unp64::` with no header
declaration; declare it in `unp64.h` (it is the real API; the C wrapper is the
shim) or make it static. XS, none.

3.6 **Indentation of the main loop.** The `while (true) {` body in `unp64cpp`
is not indented (it was a `goto` target before, see the comment at line 697),
and lines 726-728 and 820-823 are space-indented in a tab file. Re-indent
and, while there, split the function: the body from `loadData` to
`writeUnpackedOutput` is ~200 lines. Natural cuts that match what already
exists: `loadAndIdentify()` (load + SYS find + scanners + switches),
`runEmulation()` (seed + two phases), `resolveEndStart()` (the `_fEndAf` /
`_fStrAf` / `_exoFnd` / `$FCE2` / `$A7AE` block), `applyDiffTrim()`
(`-u` / `-l`), then `emit()`. Effort M, risk none if done as a pure move.

3.7 **`-u` loop** zeroes bytes one at a time; since `u32eq(oldmem + addr,
READ_LE_UINT32(mem + addr))` is "four bytes equal", use `memcmp`/`memset`.
Cosmetic, XS.

3.8 **`parseSwitchString`**: the `>= 100`-char rejection and the
4-token/64-char caps are silent. A switch string that is too long or has a
fifth token is dropped without any indication. At minimum a comment; better,
size the tables from one constant and make an over-long string a failure so
the registry author notices (the registry entries in the C callers are short,
but a truncated `-d… -t… -r… -e…` would be extremely hard to debug).
S, low.

3.9 **`SIZE_MAX`/`MIN`/`ARRAYSIZE` in `types.h`.** The `#define SIZE_MAX
0xFFFFFFFF` is dead: it is guarded by `#if !defined(SIZE_MAX)` right after
`<cstdint>`, which always defines it (and the fallback value would be wrong
on 64-bit anyway). `MIN(a,b)` is unparenthesised. Only `MIN` (once, in
`exo_util.cpp`) and `ARRAYSIZE` (once, in `unp64.cpp`) are used and
`SIZE_MAX` not at all. Replace with `std::min` / `std::size()` and delete all
three macros.

---

## 4. Headers and file inventory

4.1 **`read_le.hpp` is dead**: not included anywhere (the only `read_le`
includes in `terps/` are scott's and taylor's unrelated `read_le16.h`), and
it is an older, shorter copy of `types.h` (same include guard
`UNP64_TYPES_H`, no `READ_LE_UINT32`, no `<cstddef>`). It is not referenced
by `project.pbxproj` either, so a plain `git rm` is enough. XS, none.

4.2 **`types.h` is a misnomer** (it holds `READ_LE_UINT16/32` and the three
macros above). Rename to `read_le.h`/`endian.h` or fold the two inline
functions into `exo_util.h`, which is where `u16eq`, `u32eq`… live. 25 files
include it, but most only need it for `READ_LE_UINT16`; a few include it and
nothing else from it. XS-S, none.

4.3 **License header repeated verbatim in 31 files** (the zlib text in each
of `unp64.h`, `unp64.cpp`, `6502_emu.{h,cpp}`, `exo_util.{h,cpp}`,
`types.h`, `read_le.hpp` and all 23 scanner files; the only files without it
are `unp64_interface.h` and `globals.{h,cpp}`, which carry a ScummVM GPL
header instead, see section 2). Keep one `LICENSE`
(or `NOTICE`) file in this directory that carries the Lind / CooG / Maurya
text and the "cut-down version" statement, and leave a 3-line pointer in
each source file. Check what zlib-license clause 3 ("This notice may not be
removed or altered from any source distribution") requires before deleting
anything: keeping the notice in a single file that ships with every copy of
the sources is normally acceptable, but that is the author's call, not a
mechanical cleanup. S, none (legal, not technical).

4.4 **Namespace-closing comments, include order, forward declarations.**
`scanners.cpp` forward-declares the 22 `scn*` functions by hand and also the
table; put the declarations in `unp64.h` (or a new `scanners.h`) so that the
signature is checked against the definition in each scanner file and the
compiler tells you when one is added but not registered. The table itself
has one space-indented entry and is the only place that fixes the **order**,
which matters (first match wins): add a comment saying so and why `Expert`
is before `AR`, `Exomizer` is last, etc.

4.5 **`unp64.h` mixes the public struct with the C++ API** (`UnpStr`,
`Scnptr`, `EA_*`, `ITERMAX`, `RECUMAX`, `scanners`). `UnpStr` is really
"scanner result record"; its field comments are the only documentation of the
scanner protocol. Consider a short `SCANNERS.md` or an expanded comment above
`UnpStr` that states, once, the invariants that all 22 scanners follow:
"return immediately if `_idFlag`", "only match when `_depAdr == 0`", "set
`_idFlag = 1` last", "`_forced` only when `_info->_run == -1`", and what
`_endAdC`/`_strAdC` encodings mean. Today that knowledge is only in the
heads of the people who wrote the scanners.

---

## 5. Scanners (the bulk: 5,400 lines in 23 files)

The scanners are pattern tables in imperative clothing. The same five-line
skeleton appears **133 times** (`if (unp->_depAdr == 0) { if (u32eq(...) &&
u32eq(...) && ...) { ...set fields...; unp->_idFlag = 1; return; } }`), and
the same idioms recur in all of them.

5.1 **Boilerplate wrapper.** Every scanner function begins with
`if (unp->_idFlag) return; mem = unp->_mem;` and every match ends with
`unp->_idFlag = 1; return;` (142 occurrences of `_idFlag = 1`). Since
`scanners()` already stops on `_idFlag`, the early return at the top of each
scanner is redundant; the outer loop is the only place that needs it. The
`if (unp->_depAdr == 0)` guard in front of each block is equivalent to
"nothing matched yet in this scanner" and could be implemented by
`return`-after-match alone (the scanner already returns on match), except in
the blocks that set `_idFlag` as a half-way marker and keep scanning
(pu_crunch.cpp:153-166, time_cruncher.cpp:261, and the `else if` chain in
"TimeCruncher 5", see 1.10); fix those first. Candidate result: each block shrinks to
`if (match) { ...; return finish(unp); }`. Effort L (mechanical but 142
sites), risk low if done with a script and the corpus.

5.2 **Table-driven matcher for the common 3-5 `u32eq` signature.** Most
blocks are "N (offset, dword) pairs must all match, then fill fields from
offsets/constants". A tiny helper

```cpp
struct Sig { uint16_t off; uint32_t val; uint32_t mask; };
static bool matchAll(const uint8_t *mem, int base, std::initializer_list<Sig>);
```

collapses the condition and, more importantly, lets the **offset base**
(`p`, which is already used in about half of the scanners as `mem + p + 0x..`)
be written once. It also removes the `0x811 + 0x84` arithmetic-in-literals
in `time_cruncher.cpp` ("pre-header" variants are the same signature shifted
by a constant, which is exactly what the base parameter is for). Effort L,
risk low; do one scanner (`time_cruncher.cpp`, the worst offender) as a
trial and measure line count before committing to the rest.

5.3 **Hand-rolled little-endian reads.** `mem[a] | mem[b] << 8` appears 147
times next to 283 uses of `READ_LE_UINT16`. A few are legitimate (the two
bytes are *not* adjacent: `mem[0x86c] | mem[0x872] << 8` or
`mem[q + 1] | mem[q + 3] << 8` is "LDA #lo ... LDY #hi", the two immediates
of separate instructions, a very common packer pattern) and should become
`readSplit(mem, loAddr, hiAddr)` so the intent has a name; the rest are
adjacent and should just be `READ_LE_UINT16(&mem[x])`. Either way
precedence of `a | b << 8` is correct but reads like a bug every time.
Effort M (sed + review), risk none.

5.4 **`if (unp->_info->_run == -1) unp->_forced = X;`** occurs 67 times
(31 in `time_cruncher.cpp`, 10 in `ccs.cpp`, 9 in `cruel.cpp`). That is
"force this entry point unless the file has a SYS line". `forceEntryIfNoSys(unp, X)`
(or a field `_forcedIfNoSys`) names it. S, none.

5.5 **Common packer-specific idioms worth naming.**
`unp->_endAdr = 0x2d; unp->_retAdr = 0xa7ae; unp->_rtAFrc = 1;
unp->_strMem = 0x801;` is "intro/virus that returns to BASIC and uses the
BASIC end-of-program pointer" (`intros.cpp`: five blocks set
`_retAdr = 0xa7ae`, eight set `_endAdr = 0x2d`). A `setBasicReturn(unp,
rtAFrc)` removes those copies and gives the magic numbers a home next to
`kBasicRunEntry` (3.4).

5.6 **Mixed indentation and declarations.** `time_cruncher.cpp` (891
space-indented lines) and `ar.cpp` (58) are 4-space; everything else is tabs.
The `unsigned char *mem` vs `uint8_t *mem` split is 7/15, and `int q, p;`
uninitialised declarations at the top of each function hide that `q`/`p`
are reused with different meanings inside one function (`intros.cpp` "Triad"
leaves `q == 8` if the inner loop does not break, and only works because the
`if (_depAdr)` guard that follows is false in that case). Declare at the use
site. XS-S per file, none.

5.7 **Scanners mutate `mem` to patch the packer.** E.g. `time_cruncher.cpp`
writes `mem[0x808] = 0x35; mem[0x809] = 0x39; mem[0x80a] = 0x00;` ("rewrite the
SYS line to `59`"), final_super_comp writes a 6-byte trampoline at
`$2a7`/`$2ac`. These are the real knowledge in the scanners and are
undocumented beyond a word. When touching a scanner, add one comment per
patch saying *what the original code would have done* and why the emulator
needs it (no I/O, no KERNAL, no raster...). No code change.

5.8 **`scnTimeCruncher` is 940 lines** for a family of ~27 labelled
variant blocks (3.0/3.1/3.3/3.x, 4.0, 4.2 ×4, 4.3, 4.4, 5 ×5, 6, Fast
pack'em, FBI, Triumwyrat, Triad, Relax, generic hacks…). Split by version
into `static bool tryTimeCruncher3(unp)`, `…4`, `…5`, `…Clones`, each of
which is the existing `if`-chain, and have
`scnTimeCruncher` call them in order. There is also a `_debugP`-only branch at
~line 506 that XOR-decrypts a string and has no observable effect on the
unpacked output (it only runs when a debug flag the C API can never set is
on), delete it. Effort M, risk low. Same treatment, smaller, for
`cruel.cpp` (390), `intros.cpp` (446, already a flat list of unrelated
intros: consider `intros_virus.cpp` / `intros_other.cpp`), `final_super_comp.cpp`.

5.9 **Layered scanners rely on `_info->_start`.**
`final_super_comp.cpp`'s "2nd layer"/"3rd layer" blocks key on
`_info->_start == 0x2a7`/`0x2ac`, i.e. on the *previous iteration's output
address*, and the first layer sets `_recurs = RECUMAX - 2`. That is
fragile (any other packer whose output starts at `$2a7` matches), and the
`-1` slack in `RECUMAX - 2` is unexplained. Document the three-layer
protocol in one comment block at the top of the scanner. Not a refactor
candidate until a test proves it.

---

## 6. Emulator (`6502_emu.cpp`, 1,328 lines)

6.1 **Pin behaviour down first.** Before any change to the emulator, add a
`terps/unp64/test/` with a single driver that takes the C64 `.prg`/`.d64`
crunched payloads that the scott/taylor/unquill corpora already contain,
runs `unp64()` with the switches from the registry, and records
`sha256(out)`+length. Compare before/after each batch. The current state is
that this directory has no test, and regressions would only show up as
"this one C64 game no longer loads" in a Scott or Taylor format test, much
later. Optional extra: a `--trace` build flag that dumps `(pc, a, x, y, sp,
flags)` per instruction to diff two runs after touching `nextInst`
(see 1.1, 1.3, 1.3b, 6.3). Effort M, none (additive), and a prerequisite for
every medium-risk item in this document.

6.2 **The 70 named `static OpInfo op_xxx_o = {&op_xxx, "xxx"}` objects and 13
`ModeInfo` objects are only there to be pointed at by `g_ops[]`,
and the `_fmt` strings (`"lda"`, `"#$%02x"`, …) are never read** (there is
no disassembler or trace output, check with `grep _fmt`). Replace by
`struct InstInfo { op_f *op; mode_f *mode; uint8_t cycles; }` and write the
table with function pointers directly: -90 lines of pure noise, and the
mnemonic comments in the table can stay. Effort S, none.

6.3 **`op_X` helpers repeat "fetch operand" 12 times.**
`switch (mode) { case MODE_IMMEDIATE: value = arg->_imm._value; break;
default: value = r->_mem[arg->_ea._value]; }` is copied in adc, and, cmp,
cpx, cpy, eor, lda, ldx, ldy, ora, sbc, anc. A single
`static uint8_t fetchOperand(CpuCtx*, int mode, const InstArg*)`; same for
the "modify register-or-memory" prologue in asl/lsr/rol/ror
(`static uint8_t *rmwTarget(...)`). -100 lines. The illegal opcodes then
become one-liners over those. Effort S, none (the flag semantics don't change).

6.4 **Unused parameters.** Every `op_*` and `mode_*` takes `(r, mode, arg)`
and ignores most of them. Fine for a function-pointer table, but with a
`static` + `[[maybe_unused]]` on the table type (or `-Wno-unused-parameter`
for this file only) the compiler stops complaining. Only worth doing if the
build prints these warnings; "UNP64: Silence warnings" (22373ff28) suggests
it did.

6.5 **Magic constants.** `$d000-$dfff` I/O window, `$dc00/$dc01` CIA1,
`$d011/$d012` VIC, `$dd01-$dd0f` CIA2, opcode bytes `0xad/0xaf/0x2d/…` as
literals in `interceptIORead`; the opcode list in `isAbsoluteWriteOpcode`
and the `switch (opCode)` in `interceptIORead` could both be driven by the
`InstInfo` table (add a `flags` byte: `READS_MEM`, `WRITES_MEM`) so the
write-intercept no longer needs a separate hand-written opcode list that
has to be kept in sync with `g_ops[]`. Note the write list contains `0x91`
(`STA (zp),y`) but not `0x81` (`STA (zp,x)`) or any of the undocumented
store-like opcodes (SAX, RMW illegals), so those write straight into $D000
RAM instead of being dropped; a flag-driven version would fix that for
free. Effort M, risk medium (changes which stores are suppressed).

6.6 **Accuracy shortcuts to document, not fix:** `op_brk` pushes but does not
vector (the caller stops on opcode 0 anyway); `op_sbx` ignores
carry-in; `$ab` LXA and `$8b` ANE use the magic constant `0xee`; `$6b` ARR
is simply not implemented (`NULL_OP`, so a packer using it aborts); BCD mode
is not implemented (`op_adc`/`op_sbc` ignore the D flag, which `cld`/`sed`
only toggle); cycle counts are only used for the raster shadow. None of these
matter for the corpus, but each deserves a one-line comment so nobody
"fixes" them (cf. the "fix:" comments scattered through the table, which
record what iAN changed relative to Exomizer's original, worth keeping).

---

## 7. `exo_util.{h,cpp}`

7.1 **The `u32eq` family**: `u32eq`, `u32eqmasked`, `u32eqxored` and the u16
versions all re-assemble bytes by hand (and `u32eq` does it with four
compares, `u32eqmasked` with shifts). All could be one `readLe32()` and
become one-liners (`readLe32(a) == v`, `(readLe32(a) & m) == v`).
`u32eqxored` takes a param named `ormask` in the header and `xormask` in the
.cpp. Make these `inline` in the header so every scanner call site inlines
(they are called hundreds of times per scan on the hot path of unpacking
every C64 file). `u16gteq`/`u16lteq` have one or two callers; check before
keeping. S, none.

7.2 **`findSys` and `loadData`** are the only non-trivial functions in the
file and the only part that is shared with nothing else; `strToInt` is only
used by `applySwitches`. The file is really three unrelated things
(PRG loading, BASIC SYS parsing, signature helpers). Not worth splitting at 260
lines, but rename `exo_util` to something that does not claim an origin
this code no longer has (`prg_util`?), or leave the name and put the
reason in the header comment. Low value.

7.3 `findSys`: the `"Workaround for hidden SYS line"` block starts its
scan at `i = 5` with no explanation, and `FIND_SYS_SCAN_LIMIT` (1000) is
tested in two places with different off-by-one behaviour
(`i >= LIMIT` after the loop vs `i < LIMIT` in it). Behaviourally fine, but
worth one comment and a unit test with a `SYS 2061`, `SYS(2061)`,
`SYS 2048+13`, `SYS 5E3`, hidden-NUL variant (these are the cases the
comment promises; there is no test for any of them).

---

## 8. Declined / not worth doing

* **Rewriting the scanners as a data-driven signature DB (JSON/table in a
  data file).** The scanners contain arbitrary code (loops over a window,
  XOR decoding, mem patches, dependent fields), so a data format would need an
  embedded scripting layer. 5.2 gets most of the gain without that.
* **Replacing the hand-written 6502 with a third-party core.** The existing
  emulator is wrong in the "right" ways for this job (no ROMs, intercepted
  I/O, undocumented opcodes with iAN's tweaks, cycle counts only for raster
  shadows). A "more correct" core would break depackers that rely on those.
* **Removing the `_name`, `_r`, `_filler`/`-f` switch.** `_name`, `_r`,
  `_monStr`, `_mon1st`, `_keypressIdx`, `_fStack` are never set by scanners
  (the driver is the only user; the `-f` switch is reachable from the C
  `settings` string). Listed here only so nobody spends time on "unused field"
  cleanups: `_name` and `_r` really are unused and can go, the rest are
  driver state.
* **Making `UnpStr` a class with accessors.** The scanners write
  18 fields directly 1,500 times; the struct is the protocol.

---

## Suggested order

1. 6.1 (test harness, pins current output) and 4.1 (delete `read_le.hpp`).
2. Section 1 bugs, one commit each, corpus check after each (1.1, 1.2 and
   1.3b are the ones that can change output).
3. Section 2 (globals) and 3.5/3.2/3.1 (pure restructuring of `unp64.cpp`).
4. 6.2/6.3 (emulator table and operand fetch), then 7.1.
5. Scanners: 5.6 formatting, 5.3/5.4/5.5 helpers, then 5.8 (time_cruncher
   split) as the trial for 5.1/5.2 before applying those to the rest.
6. 3.3 (ROM-stub merge) and 6.5 last, each only with the trace diff from 6.1.
