# Testing terps/plus

The Plus sources are in the app target, so a new `.c` file needs a pbxproj
entry (`plutil -lint` the staged project; do not use `git apply
--unidiff-zero` on it) and a line in `Makefile.headless`. Run all of this
before and after a change:

```
cd terps/plus
make -f Makefile.headless imagecheck companioncheck trunccheck scriptcheck synthcheck
make -f Makefile.headless check-asan        # the same again under ASan+UBSan, plus containercheck (~5 min)
xcodebuild -project ../../Spatterlight.xcodeproj -scheme Spatterlight build
```

Each suite has its own README next to its data:

- `images/`: `imagecheck`, every detector (Apple II, Atari 8-bit, C64, ST,
  MS-DOS) draws every picture of every corpus disk against a golden;
  `companioncheck`, the two-disk sets with one disk missing or under other
  names; `trunccheck`, the binary database of each game cut short at about
  1200 lengths, each copy ending at an unreadable page, every one refused.
- `scripts/`: `scriptcheck`, walkthrough replays of the six games through
  `plus_hl`, transcript hashes. Includes the unfinished X-Men, the only
  replay that runs logic 27, the "with" list, flag 60 and actions 111, 115
  and 117.
- `synthetic/`: `synthcheck`, replays on made-up databases that are in the
  repository, for the opcodes, system commands, condition guards and loader
  edge cases no game reaches. With the walkthroughs it leaves nothing in
  `PerformLine`, `SysCommand`, `NextItemAt` and `TestConditions`
  unexecuted but three debug prints.
- `run_truncation_tests.py --containers` (`containercheck`, run by
  `check-asan`): the disk images themselves cut at about 30 lengths and
  with a byte in 300 flipped, about 1200 copies; only normal exit is
  checked. Its first run found four bugs, one of them in the shared
  `common_sagadraw/c64a8draw.c`, so a change there must also pass Scott's
  `make c64a8test`.

Not covered by any of them: timers (the dismiss timer of `AnyKey`,
animations, colour cycling), MS-DOS disk images, and drawing time in the
app, which was never measured. The app's own Claymorgue transcript
(`Transcript of SPL13P.txt`) takes a different random path from the
headless replay of the same script; never edit it from a headless run, and
never run `UITests/testPlus` against the real app (its `setUp` deletes the
library). `GameImportXCTests/testPlus` in the isolation rig is the way to
check it.

## Behaviour settled by the passes of October 2026

The interpreter was refactored and rewritten in parts between 2026-10-12
and 2026-10-17. These are the decisions a later change must not undo by
accident; each has a test row unless it says otherwise.

- **Where the originals and we disagree on purpose.** System command 110-5
  remembers the special picture as the animation background and draws it
  when the animation runs; the MS-DOS EXE draws it at once. Save asks Glk
  for a file and prints the database's message; the EXE prints "Saving
  game. SAVE GAME AREA (A-D)" and writes SAVEGA.DAT silently. 110-7, 8, 10
  and 11 take the counter as the picture number; the later 6502 and 68000
  builds look it up in a database column, which gives the same number for
  every shipped game.
- **How conditions combine** follows the EXEs exactly: a line fails at the
  first false result unless an OR or 31.8 came before, so "A, OR, B" is
  plain "A"; the first result after a left parenthesis starts the group
  afresh; a group's result is negated like a condition's. Do not "fix" it.
  `TEST GROUPS` in `synthetic/` pins it. The way to check such a change is
  a truth table of both rules over every condition list of every database.
- **Numbers in an action line** are two bytes when the first is 128 or
  more, opcodes and operands alike (so 128 is never an opcode). Conditions
  are `value << 5 | code`. Only opcode 64 maps 998 to a noun.
  `Item.Location` is 16 bits (X-Men parks used-up items in "room 500") and
  a saved game accepts any room.
- **Random percent (opcode 108)** is 0 to 99 as in the MS-DOS EXEs; the
  6502 builds give 1 to 100. Only X-Men uses it.
- **The order of `RandomPercent` calls in `IsMatch`** defines the random
  stream every walkthrough golden depends on. Flattening `IsMatch` or
  merging the implicit and explicit scan loops was declined for that
  reason.
- **A command with a participle** does not match an action whose extra
  words have no participle (every original agrees). The old 6502 builds
  skip a trailing adverb after a matched participle and noun; the later
  ones check it, as Claymorgue's DROP MIRROR needs, and so do we.
- **Operand guards.** Every `Items[]` and `Counters[]` access indexed by an
  operand is checked; an invalid operand makes a condition false and a
  command do nothing. `PlayerRoom()` is the one reader of `Rooms[MyLoc]`
  and `Fatal`s on a room past the table, as the BASIC originals do with
  "Subscript out of range"; the write side is not guarded, because
  Fantastic Four swaps counter 32 into the main counter as scratch
  mid-line. Pinned by `BADARGS`, `GUARDS` and `fatal-room`.
- **Action 61** leaves counter 17 alone (the old `&= 0x0f` is gone); 77 and
  83 no longer clamp at zero; 111 clears both the matched and the done
  state. "I didn't completely understand your command" follows the EXE's
  rule but not its spelling ("commmand").
- **Endless action tables** (a damaged ST Claymorgue) stop with
  `Fatal("Endless loop in the action table")` past a million scanned lines;
  the most any walkthrough reaches is 8111.
- **Loaders.** Every binary read goes through `NeedBytes`; a new reader
  must call it. `FreeGameData()` is called from every failure exit that
  returns, so a new table that holds pointers must be allocated zeroed and
  freed there. `SetGame` compares whole titles; a title that is a prefix
  of a known one is "Could not detect game type!" (`prefix-title`). The
  ST section pointers are big-endian file offsets padded to even
  addresses; `SectionStart()` replaces the old per-game nudges. `Rooms` is
  calloc'd and the exit table stays 8 columns because GETEXIT/SETEXIT
  index it from a counter.
- **Drawing.** Rectangles of one colour that touch on a row are merged
  (`FillRun`/`FlushPixels`), and a scene the window already shows is not
  redrawn. Whatever draws to `Graphics` outside `graphics.c` calls
  `GraphicsWindowChanged()` (or `GraphicsWindowRecoloured()`) and clears
  with `ClearGraphicsWindow()`. The per-turn canvas of every walkthrough
  is unchanged by this.
- **Left as the EXE does not:** 91 (remainder and quotient order with
  negative or aliased operands), 114 and 124 (loop depth below zero), 120
  (a no-op in three EXEs, we always sync counters 48-53), 88 and 93 (busy
  loops). No database reaches the difference.

## The original interpreters

Every opcode and condition was read against the originals in October 2026
and REFACTOR-TODO's item list recorded it. The short form, so that a 6502
oddity is not taken for a finding twice:

- **MS-DOS** (XMEN.EXE, EXEPACKed; FANTFOUR.EXE; Spider-Man MENU.EXE;
  Claymorgue YOHO.EXE; Buckaroo START.EXE) are compiled IBM/Microsoft
  BASIC: one code pattern per statement, GOSUB and ON...GOTO as runtime
  calls with inline tables. No decompiler exists and Ghidra makes little
  of it; a capstone script that knows the patterns was enough. Quickest
  ground truth is the dosbox MCP: mount the game folder, answer `80` to
  the columns question, read the text page.
- **Apple II** Spider-Man (`~/sagaplus/spidermem3.bin`, Ghidra
  `~/ghidrasaga2.gpr`) is the same source compiled: commands 52-127 at
  $8DCB, conditions $0CC9, 110 sub-commands $99D5. Apple II Fantastic Four
  (`~/sagaplus/ff4.bin`): commands $8D82, conditions $0CC9.
- **C64** Spider-Man is `skit.bas` in the Spider-Man folder under
  Downloads (load $3F5B; commands $55F7, conditions $4DF6, matcher $4910);
  the `MENU*.BAS` beside it are slices of MENU.EXE, not source. C64
  Fantastic Four is `SAGA.C64` on QUESTPR3.D64 (load $3A00; commands
  $55AF, conditions $4DB0), a later build of the same source, and
  `SAGA.TED` on the same disk is its Plus/4 build (load $2400). Compare
  handler by handler through the jump tables, then diff lifted listings
  with addresses masked; a "chunk of bytes found" test lies when addresses
  shift.
- **Atari 8-bit** disks boot without a file system; memory address = ATR
  body offset + $4D00, database at $1000, variables = C64 address minus
  $0E30. Three builds: Spider-Man (also on the second Buckaroo disk),
  Buckaroo (with a "CMD:" trace), Fantastic Four (the later source).
  Conditions 24, 25 and 27 have code only here and are broken or unused.
- **Atari ST** Spider-Man `AUTO\SCOTTA.PRG` and Fantastic Four
  `PROGRAM.TOS` are one interpreter (43 bytes differ); the latter keeps a
  symbol table with BASIC line labels. Commands through $17D6, conditions
  $0E78, 110 sub-commands $2778.
- Scott Adams' own `SAGAPLUS.DOC` (v5.0, in Downloads) names the
  per-platform bugs itself ("ap/cm/at" broken for 61 and 86, 28 and 31.9
  broken everywhere but IBM). Read it first.

## Declined, with the reason

- A table of handler functions for `PerformLine`: the cases share its loop
  state and many fall through on purpose. A function pointer per system in
  `DrawImageWithName`: the decoders take different arguments.
- Merging the `RestoreState` / `RestartGame` / `LoadGame` tails: they
  differ in order, not only content. A bitmask for `JustStarted`,
  `JustRestored`, `JustRestarted`: different lifetimes.
- Moving display code out of `plusmain.c`, or a SPATTERLIGHT glue file:
  both would re-export the statics the pass made file-local.
- One list of section readers for the plain text and binary loaders: the
  sources and failure modes differ, so it would be two tables behind one
  name. `AllocateTables()` is the shared part.
- `SplitCommaList`, `VFormat`, an X-macro for the flag names, sharing
  `ColorCyclingPixel`'s flip math, `GetAnyDictWord` returning an error
  (noun 0 is "ANY" in all 37 databases).
