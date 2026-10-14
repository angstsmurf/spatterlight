# Taylor — refactoring, cleanup and deduplication TODO

Survey date: 2026-10-12. Worked through the same day; every item carries its
outcome on a `→` line. `[x]` done, `[~]` partly, `[-]` declined or left.
Suites after the pass: image 58, mame 538 identical / 50 known, script 94.
The animation and C64 failure-path fixes are not exercised by those suites.

Line numbers are approximate (tree at 6d581d880); grep the function names.
Items marked **(behaviour)** change observable behaviour or fix a defect and
should be separate commits, each with a test row. Items marked **(verify)** are
suspicions that were read off the code but not reproduced.

Scope: `terps/taylor/*.c|h` (about 9,000 lines): actions.c 1187, dumper.c 821,
player.c 785, ui.c 567, animations.c 524, decrypttotloader.c 501,
textoutput.c 452, gameinfo.c 442, gameload.c 439, graphics.c 414,
c64decrunch.c 360, parseinput.c 334, extracttape.c 326, taylor.h 324,
extracommands.c 268, loading_screen.c 219, alkatraz_screen.c 213,
restorestate.c 132, utility.c 49, loadtotpicture.c 43, plus `test/`.

## Safety net to run before and after any item

```
cd terps/taylor && make -f Makefile.headless imagecheck mamecheck scriptcheck
```

(builds `taylor_image_probe` and `taylor_hl`; expected dumps in `test/images/`,
transcripts in `test/scripts/scripts.tsv`). Then build the Spatterlight scheme
with `xcodebuild` — the taylor sources are in the app target, so a new `.c`
needs a pbxproj entry (`plutil -lint` the staged project; do not use
`git apply --unidiff-zero` for it). `Makefile.headless` picks up `*.c` by
wildcard.

---

## A. Housekeeping and dead code (low risk, do first)

- [x] `dumper.c` includes `"taylormade.h"`, which does not exist: it cannot
      build. It is a stand-alone curses-era tool and not in either build.
      Either delete it (and the `dumper` target in `Makefile`) or port it to
      `taylor.h` and add a `Makefile.headless` target. Likewise the legacy
      `Makefile` (curses `player`/`dumper`, `player.o ui.o` only) is stale.
      → `dumper.c` and the legacy `Makefile` deleted; the `filter-out` in `Makefile.headless` went with them.
- [~] `ChangeLog`, `README`, `FORMAT` are stale: refresh or fold the useful
      parts of `FORMAT` into a `docs` note, drop the rest.
      → `FORMAT` brought in line with the interpreter (opcode tables, text, exits, objects, picture codes, a Questprobe 3 section). `ChangeLog` and `README` left alone.
- [x] ui.c: `HitEnter()` is defined (`static`) and never called — delete.
      → Deleted.
- [x] ui.c header comment names the wrong file (`restorestate.c`).
- [x] graphics.c `PutPixel`: dead `y_offset = 0`. ui.c `OptimalPictureSize`:
      dead `multiplier = 1`.
      → Both removed.
- [x] kayleth_loadscreen_data.h comment mentions a nonexistent
      `DecodeKaylethLoadingScreen()` (real one: `DecodeAlkatrazLoadingScreen`).
      extracttape.c has a stale "now live in the shared decompressz80 library"
      comment.
      → Both comments fixed (extracttape.h had the same one).
- [x] decrypttotloader.c header says `decryptloader.c`; file is
      `decrypttotloader.c`.
- [x] `.deshuffle_repeats = 05` (octal spelling) in extracttape.c: write `5`.
- [x] `const`-correctness: `OutString(char *)` → `const char *`;
      `char *start_of_transcript = "…"` in extracommands.c → `const char *`.
      → `OutString(const char *)`. The transcript strings are passed as literals instead: `glk_put_string_stream` takes `char *`, so a `const char *` local warns.
- [x] `isalpha/isspace/toupper` on plain `char` in textoutput.c/parseinput.c:
      cast to `unsigned char` (UB for negative values).
      → textoutput.c cast; parseinput.c already had the casts (stale).
- [~] Mixed `//` and `/* */` comment styles within a file (extracttape.c,
      c64decrunch.c); `glkimp.h` included unguarded in loading_screen.c while
      other files use `#ifdef SPATTERLIGHT`.
      → `glkimp.h` now guarded. Comment styles left mixed — churn only.
- [x] `MAX_TAPE_BLOCKS` is `#define`d inside a function in loading_screen.c;
      `Resizing` and `TopHeight` are defined mid-file in ui.c — move up.
      → `MAX_TAPE_BLOCKS` at file scope, `Resizing` moved up; `TopHeight` was already at the top (stale).
- [x] animations.c: redundant `KaylethAnimations[0] = NULL`.
- [~] player.c `NewGame`: the loop zeroing `Flag[4+i]` for 124 entries after
      the memset is redundant, as is `Flag[42]/[43] = 0`.
      → The finding was partly wrong: the old loop also *copied* `Flag[4..6]` from the file for non-QP3 games. Now `memset` + `memcpy(Flag, FileImage + FlagBase, QP3 ? 4 : 7)`; the `Flag[42]/[43]` zeroing is gone.
- [x] actions.c `DropAll`: `ObjectLoc[i] == Carried() && != Worn()` — second
      test is redundant (Worn() ≠ Carried()).

## B. Correctness and bounds (all **(behaviour)**)

- [x] gameload.c `FindTokens`: last-resort
      `addr = FindCode("You are in ", 0, 11) - 1; if (addr == -1)` can never be
      true when the string is missing (`(size_t)-1 - 1`), so the
      `Game->start_of_tokens + FileBaselineOffset` fallback is unreachable.
      Test the `FindCode` result before subtracting; use one signed type for
      positions.
      → `FindCode` result tested before the subtraction, so the registry fallback is reachable. No test row: no file in the corpus lacks the string.
- [x] gameload.c `FindCode`: bound `p < FileImage + FileImageLen - len` is off
      by one (should be `<=`) and underflows the pointer if `len > FileImageLen`.
- [x] gameload.c `LookForSecondTOTGame`: `DetectGame()` may return NULL and
      `AltGame->gameID` is dereferenced; `secondfile` is never freed;
      `size_t ParkedOffset` holds a `long`.
      → NULL check, `free(secondfile)`, `long ParkedOffset`, and guards for a name with no period.
- [x] gameload.c `TokenClassify` (end of `FindTokens`, also called from
      `DetectGame` per candidate) writes through `Game` (`Version`) while
      `Game` may still be NULL. **(verify)**
      → Confirmed: `Version` was read through a NULL `Game` on the first `DetectGame` candidate. Guarded; also bounded on `FileImage + FileImageLen` (not `EndOfData`: it runs on the ToT companion file too).
- [x] gameload.c `GuessLowObjectEnd` (non-Rebel-Planet loop) has no
      `EndOfData` bound.
- [x] animations.c `UpdateKaylethAnimationFrames` ends with
      `ObjectLoc[122] = DESTROYED;`. `DESTROYED` is the `ConditionType` enum
      value (14), not `LOC_DESTROYED` (0xfc) / `Destroyed()`; the object is
      also not restored afterwards, so saved state is mutated. **(verify)**
      → Confirmed and fixed: Kayleth has 120 objects, so slot 122 is spare storage for the droid on the belt and the tick left it at "room 14". Now `KAYLETH_DROID_SLOT` set to `Destroyed()`. Slot is not restored afterwards (unchanged; it is not a real object). Not covered by the suites — headless runs have no timer.
- [x] animations.c `KaylethAnimations[MyLoc]` can index beyond
      `NUMBER_OF_KAYLETH_ANIMATIONS` (92, MyLoc is uint8) and be NULL before
      load; `LoadKaylethAnimationData` has no bounds check on `ptr` and no
      check for `AnimationData == (size_t)-1`, and dereferences
      `KaylethAnimations[2]->frames[...]` unconditionally.
      → Table NULL-filled, `MyLoc` and frame index checked, loader bounded on `ptr`/`counter` (the 0xff-skip loop could write past the table), Azap and belt patches guarded. Not covered by the suites.
- [x] actions.c `DropAll`: `if (loud & !found)` uses bitwise `&`; make `&&`.
- [~] actions.c DIAGNOSE case: `char buf[5]` declared in an unbraced `case`,
      digits printed char-by-char; use a braced block and `PrintNumber`
      (check zero padding first).
      → Braced. `PrintNumber` not used: DIAGNOSE zero-pads to a fixed width.
- [x] player.c `ListExits` walks the exit table with no `0xfe` sentinel or
      bounds (`AutoExit` has the guard) — runs off the table on a malformed
      room. Share the lookup (see C).
      → `FindExitBlock()` with `EXITS_END` and an `EndOfData` bound, shared with `AutoExit`.
- [~] player.c `LoadGame` returns 1 even on a bad restore;
      `YesOrNo` `c == 250 → 0` is odd.
      → `250` explained as `(uint8_t)keycode_Return`. `LoadGame` return left: the caller redraws either way and changing it alters the restore prompt flow.
- [x] graphics.c `ExtractSingleQ3Image`: `uint16_t offset_addr` is computed
      (and truncated) before the `offsets >= FileImageLen` check; the
      `FindCode` results (base, offsets, imgdata) are used unchecked against -1.
      → `size_t` throughout, `FindCode` results checked in `ExtractQ3Images`. Goldens unchanged.
- [x] graphics.c `DecompressEarlierType` / `DecompressHemanType` /
      `RepeatOpcode`: bounds only per iteration; the nested call passes
      `&instructions[index]` with the full `MAX_INSTRUCTIONS` capacity, and
      `RepeatOpcode` writes three bytes unchecked. **(verify)**
      → Confirmed. Remaining capacity is passed into the recursion and into `RepeatOpcode`; also fixed a `free()` of an advanced pointer.
- [x] graphics.c `InitGraphics`: `SeekToPos` may return NULL in the QP3
      branch; `FindTilesStart` returns 0 on failure, indistinguishable from a
      real offset; it also sets `Game->number_of_pictures = 0` on the shared
      `games[]` registry.
      → NULL → text-only (`NoGraphics`), `FindTilesStart` returns `(size_t)-1`, registry no longer written.
- [x] ui.c `FlushRoomDescription`: `if (i < TopWidth + 1)` is always true and
      `string[i]` overflows by one when `TopWidth == 2047`;
      `ROOM_DESC_BUF_SIZE 1000` silently truncates long descriptions.
      → Clamp to `LINE_BUF_SIZE - 2`; `ROOM_DESC_BUF_SIZE` 4096.
- [x] ui.c `OpenGraphicsWindow`: second branch computes `x_offset` with
      unsigned subtraction that can underflow.
      → Signed arithmetic, in the shared `FitGraphicsWindow()`.
- [x] extracttape.c `add_block` has no bounds check on `outbuf + offset`
      (Blizzard Pass 0x2001f buffer); `process_tzx_extract` failure path may
      return `image` after `GetTZXBlock` overwrote `*length`. **(verify)**
      → `add_block` takes the buffer size and refuses a block that does not fit. Verified the second: `GetTZXBlock` did overwrite `*length`; it now gets a local.
- [x] c64decrunch.c `terror_menu`: `get_file_at_ts()` results
      `tempfile1/tempfile2` are used without a NULL check (and `size1 -= 176`
      can underflow); both temporaries are leaked; hybrid case ignores the
      return values of both `ProcessC64` calls.
      → NULL checks, frees, named sizes, hybrid size check; a failed second file is now `Fatal()` instead of a dangling `FileImage`. Failure paths not covered by the suites.
- [x] c64decrunch.c `GetFileFromT64`: `offset + size > *extent` is evaluated
      with `int offset + size_t size` (fine) but `size = *extent - offset`
      underflows when `offset > *extent` — check first.
      → Also bounds the directory entry and `end <= start`.
- [x] restorestate.c: error paths after a failed restore leave a
      half-overwritten `Flag`/`ObjectLoc`; confirm `RecoverFromBadRestore`
      covers every early return. **(verify)**
      → Verified, no change needed: one failure path, and it calls `RecoverFromBadRestore`.

## C. Deduplication

- [x] **QP3 image redraw** (`tempstop = StopTime; StopTime = 0;
      DrawImages = 255; RunStatusTable(); QP3DrawExtraImages();
      StopTime = tempstop`) is duplicated in `Look()` and `RunOneInput()` →
      `QP3RedrawImages()`.
      → `QP3RedrawImages()`.
- [x] **Exit table scan** in `ListExits` and `AutoExit` → `FindExitBlock(room)`.
- [x] **Parameter-count logic** (`op > 8` takes arg1;
      `>= TwoConditionParameters()/TwoActionParameters()` take arg2) is
      duplicated in `ExecuteLineCode` and `NextLine` →
      `ConditionParamCount()` / `ActionParamCount()`.
- [x] **TakeAll/DropAll** share the "name......Taken/Dropped" print loop.
      → `ListObjectWithDots()` / `ListOutcome()`.
- [x] **Temple of Terror "flag 12 → 60" fixup** appears in both the EQ
      condition and the ADD action → one helper.
      → `ToTPuzzleFlag()`.
- [-] **Token-stream walks** (`TokenClassify`,
      `GuessLowObjectEndRebelPlanet`, `PrintTextRebelPlanet`) use the same
      "token pointer + TOKEN_LAST_BYTE" loop → shared iterator.
      → Declined: the three loops differ in what they do per byte; an iterator would be longer than the loops.
- [-] **Skip-to-nth-entry** loops in `Q3PrintText` and `PrintText1`.
      → Declined: different terminators and token rules; two five-line loops.
- [-] **Period collapsing** (`JustWrotePeriod`) is implemented in
      textoutput.c `OutChar` *and* ui.c `PrintCharacter`; `Q3PrintChar`
      duplicates `OutChar`'s capitalisation logic; two parallel flags
      `Upper` / `Q3Upper`.
      → Declined: the two layers collapse at different stages (text decode vs. window output) and every transcript golden depends on the exact interplay.
- [x] **Attribute → ink conversion** (`attr & INK_MASK` +
      `8 * ((attr & BRIGHT_FLAG) == BRIGHT_FLAG)` + `Remap`) and the "draw
      the set bits of an 8×8 cell with `PutPixel`" loop appear 4+ times in
      animations.c (AnimateStars ×2, AnimateForcefield, FillCell, Kayleth
      click shelves — the last with literals 7/64). Check whether
      `../aiukgraphics/irmak.h` already has such a helper before adding one.
      → Stale: animations.c already uses `AttributeInk` + `Remap`. Literal 32 → `IRMAK_IMGWIDTH`.
- [x] **Animation rate bookkeeping**: `glk_request_timer_events(0);
      AnimationRunning = 0;` ×3 and `if (AnimationRunning != r) { request;
      set }` ×N → `SetAnimationRate()` / `StopAnimations()`.
      → `SetAnimationRate()` / `StopAnimations()`. The serpent/robot/crag-snapper calls stay raw: they do not touch `AnimationRunning`.
- [-] **Room/object conditions** duplicated between
      `UpdateRebelAnimations` and `StartAnimations` (room 1/obj 50, 86/42,
      88/107, 29–33/92, 50/58, 71/36) → one table `{room, object, rate, fn}`.
      → Declined: the two functions test different extra conditions per room; untestable headless.
- [x] **Game lookup by ID**: "find `TOT_HYBRID` in `games[]`" in
      gameload.c (twice) and c64decrunch.c (`terror_menu` and `ProcessC64`)
      → `FindGameByID()` (also removes the `Game = &games[index++]` oddity).
      → `FindGameByID()` in gameload.c, used at all four sites.
- [x] **String copy** (`MemAlloc` + `strncpy`) in gameload.c and
      `glkunix_startup_code` → strdup helper.
      → `CopyString()`.
- [x] **Save/load error blocks** in player.c ("Unable to open file",
      "Save failed") duplicated.
      → `FileNotice()`.
- [-] **Window teardown/rebuild**: loading_screen.c `DrawZXTitleImage`
      repeats `OpenBottomWindow`/`OpenTopWindow`-style code from ui.c and the
      Bottom-window `FindGlkWindowWithRock`/close sequence appears twice.
      → Declined: the title screen needs a different window layout from the game; no shared shape worth a helper.
- [x] **ZXLoadingScreen copy** (`MemAlloc` + `memcpy ZX_SCREEN_SIZE`)
      duplicated in the Z80 and Side-B paths in extracttape.c.
      → `CaptureLoadingScreen()`.
      → 2026-10-14 extracttape.c pass: `ShrinkToSnaSize()` only allocates now (callers free
        what they own, as with `DecryptToTSideB`). A 0xa000-byte file that `DecryptToTSideB`
        rejects used to come back from `ProcessFile()` as NULL and segfault in gameload.c;
        it now falls through to the raw image like the other tape paths ("not a TaylorMade
        game"). `FixBrokenKayleth()` is static and gated on a successful Z80 decompress.
        Unused `stdarg.h`/`taylor.h` includes dropped. Image and script suites identical.
- [x] **ui.c `UpdateSettings`**: two tri-state switch blocks
      (inventory/palette) → `SetExclusiveOption()`.
- [x] **`OpenGraphicsWindow`** two branches duplicate window open / size /
      arrange code.
      → `FitGraphicsWindow(shrink_only)`.
- [x] **Temple of Terror D64 menu**: three checksum lists
      (`chksum == 0x577e || 0x4661 || 0x7b2d || 0x7bf2` and the T64
      `0x2b54 || 0x3b37`, plus the `rec.chk` tests in `terror_menu`) repeat
      knowledge that belongs in the registry → add a `layout` field
      (`ONE_FILE`, `TWO_FILES_BY_TS`, `TWO_FILES_BY_NAME`, `T64_TWO_RECORDS`).
      → Done: `layout` and `filename` fields in `c64_registry`; the three lists are gone.
        The five registry disks the suites lacked (Triad, Trianon, FBR, two FLT) were added
        first as picture and walkthrough rows (65 / 115 rows), unchanged by the refactor.
        The relocation `switch (record.chk)` in `ProcessC64` stays.
- [-] **`Delay` / `Okay` / `Chance`** exist in terps/plus too (cross-terp,
      low priority — only if a shared helper dir appears).
      → Cross-terp; nothing to do here.
- [-] `rotate_left/right_with_carry` live in utility.c but are used only by
      animations.c → move them there as static.
      → Finding was wrong: decrypttotloader.c uses them too. They stay in utility.c.
      → 2026-10-14: decrypttotloader.c rotates inline now, so the helpers are gone from utility.c;
        animations.c has `RotatePixelRow()` (a run of cells as one long word) and `DrawPixelRow()`
        in place of its three carry loops. Image suite identical.
        `SeekToPos()` moved to graphics.c, its only user. `CleanupAndExit()` must stay
        exported: the shared common_utils.c `Fatal()` calls it. utility.c keeps that,
        `Transcript` (five users) and `EqualsIgnoringCase()` (two users).
        Later the same day utility.c was deleted: `Transcript` and `CleanupAndExit()` live in
        extracommands.c (declared in extracommands.h), `EqualsIgnoringCase()` in parseinput.c.
        utility.h stays as the umbrella of the shared headers. pbxproj entry removed.

## D. Table-driving and magic numbers

- [-] actions.c `Condition[]` / `Action[]` DEBUG name tables duplicate the
      enums in taylor.h (and `Action[12]` is patched at runtime): X-macro the
      enums. Six interleaved `#ifdef DEBUG` fprintf blocks in
      `ExecuteLineCode` → helper functions.
      → Declined: DEBUG-only; the `-DDEBUG` build still compiles.
- [x] `Q3Condition[]` / `Q3Action[]`: long runs of trailing `0` → designated
      initialisers.
      → Explicit sizes (`[128]`, `[ACTION_OP_MASK + 1]`), trailing zeros dropped.
- [~] Named flag/object/message constants instead of literals:
      `Flag[1]`/`Flag[3]` (OtherGuyLoc/OtherGuyInv), `Flag[2]`, `Flag[26]`,
      [39], [42]/[43], [59], [63], [7], [10]; `Message(31)`, `Message(24)`,
      `Message(223)`, `SysMessage(14)/(15)`; `ObjectLoc[7],[2],[18],[1],[17],
      [33],[37],[48],[29],[10],[11],[46]`; `IsDir` 57/60/≤4/≤10; MyLoc
      250/91/15/49/6/0; QP3 offsets 210/217; Blizzard `0x18000` text offset.
      → Done: `CarriedLoc`, `OtherGuyLoc/Inv`, `TurnsLow`, `Q3_OBJ_*`, `Q3XanduPresent`, `Q3_MSG_*`, `Q3_SYSMESSAGE_BASE`, `Q3_EXITS_MESSAGE`, `BLIZZARD_LONG_ROOM_TEXT`, `KAYLETH_TITLE_ROOM`, `IsIntroRoom()`, `IsDir` via `WEST`/`SOUTHWEST`, `SysMessage(YOU_SEE)`. Left literal where the meaning is not known: `Flag[42]/[43]`, [59], [63], [7], [10], `Message(31)`, objects 10/11/46/48.
- [x] `Destroyed()/Carried()/Worn()/DarkFlag()/WaitFlag()` return QP3 vs
      non-QP3 constants (43/42/49/5/7): make them table lookups off `Game`.
      → Stale: they already return `LOC_*` constants per version.
- [x] gameload.c `FindTokens`: six near-identical `case` blocks of 10-byte
      signature scans → table `{GameID, signature}` (HEMAN_64 / KAYLETH_64 /
      TOT_*_64 share one).
      → Done 2026-10-14: the earlier reason was wrong, the cases differed only in the signature. `token_signature` field in `GameInfo`.
- [x] gameinfo.c: positional initialisers with field-name comments that do
      not match the struct (`start_of_characters`/`start_of_image_data` vs
      `start_of_tiles`/`start_of_image_blocks`) → designated initialisers.
      TOT_TEXT_ONLY / TOT_HYBRID (and `_64`) rows duplicate ~20 fields.
      `DetectGame` loops over the "Unknown game" row too.
      → Comments now name the real fields. Designated initialisers done 2026-10-14: zero fields left out, table compared field by field before and after. Unused `start_of_intro_text` removed. The ToT rows still repeat their fields.
- [-] Per-game word/room fixups hard-coded inside `Parser`/actions:
      Blizzard Pass (SKIN→43, POLI→44, DRAW 134→49), QP3 (WALL/FIRE→77,
      MASTERS→121), `Word[0]==20 && Word[1]==162/141/254` (ToT, Rebel,
      Kayleth) → one `{game, verb, noun, replacement}` table.
      → Not done as a table: only the five parser rows are word replacements. The other three are
        GO DOOR in ToT (20 = GO/ENTER/CLIMB/WALK/RUN, 162 = DOOR; guard bug fix in the ZERO condition)
        and DROP ALL in Rebel Planet / Kayleth (20 = DROP, 141 / 254 = ALL; loud or silent DROPALL).
        Both are now commented in actions.c.
- [x] extracttape.c: three `ExtractSpec` consts rebuilt per call plus three
      near-identical `DecodeAlkatrazLoadingScreen(…X_SCREEN_WINDOWS, X_BLOCK,
      X_OFFSET, X_LOACON, X_ADD2, X_ATTRFILL)` calls built from per-game
      `#define`s → static table keyed by file size; magic
      0xc01b/0x3fe5/0x10000/0xb4bb named.
      → Specs are `static const`; `ZX_MEMORY_SIZE`, `SNA_SIZE`, `SNA_START_IN_MEMORY`, `BROKEN_KAYLETH_Z80_SIZE`, `BLIZZARD_PASS_IMAGE_SIZE` named. Loading-screen calls tabled 2026-10-14: `alkatraz_tapes[]` keyed by file size.
- [~] animations.c: Queen computer `offset` 3→10→27 chain → array;
      `StartAnimations` speed `switch` → table; magic image patch 118;
      `ROBOT_ANIMATION_RATE` reused for the crag snapper;
      `AnimationStage` shared between Rebel and Kayleth click-shelves.
      → `indicator_columns[]`, `AZAP_FIRST_IMAGE`. 2026-10-14: `SNAPPER_ANIMATION_RATE`, and one
        counter per animation (`QueenStage`, `SerpentStage`, `TwoFrameStage`, `ShelfScrollStage`).
        The shared counter was a real bug inside Rebel Planet, not between the games: the serpent
        left fully out, then a walk to the robot, drew picture 143 for a frame; a restore from
        the Queen computer into the sewers drew pictures 72–68 as a withdrawing serpent.
        `SerpentStage` is also reset on leaving the sewers. Speed `switch` left as it is.
- [x] c64decrunch.c: magic memmove/memcpy offsets in `ProcessC64` (0x098a,
      0xc802, 0xd802, …) and the hybrid splice (0x16ea/0x4708/0x1400,
      0x8888/0x8048/0x4123) → named fields in the registry
      (`relocate_from/to/len`); many registry rows differ only by comment.
      → Hybrid splice named (`HYBRID_*`), `TRIANON_LOADER_SIZE`, `TRIANON_MISSING_HEADER`. Relocation offsets moved to the registry 2026-10-14 (`relocate_to/from/size`), now bounds-checked. Only the FBR row is in the suites; the Rebel Planet and Popeye He-Man disks were run by hand.
- [x] restorestate.c: `memcpy(…, 128)` / `256` → `sizeof(Flag)` /
      `sizeof(ObjectLoc)`; `struct SavedState *` vs typedef.
- [x] parseinput.c: `ExtraCommands[]`/`ExtraCommandsKey[]` parallel arrays →
      array of struct, `static`; hand-rolled case-insensitive compares in
      `IsDelimiterWord` and `ParseExtraCommand` → `strcasecmp`.
      → One `static const` struct table (extracommands.c). `EqualsIgnoringCase()` in utility.c rather than `strcasecmp` (POSIX-only).
- [~] graphics.c `ExtractSingleQ3Image`: patch lookup
      `FindImagePatch(QUESTPROBE3, 55, 0)` applied to picture 17 is
      confusing — pass the table index.
      → Commented; the patch table keys are shared with other games so the lookup stays.
- [~] loading_screen.c: `AlkatrazDrawOrder[6912]` → `ZX_SCREEN_SIZE`;
      `ZX_SCREEN_BASE` duplicates `ZX_RAM_BASE` (extracttape.h);
      `DrawBlack` uses 32*8 × 12*8 vs `IMAGE_WIDTH_IN_PIXELS 255`.
      → First two done. `DrawBlack` 32*8 left: deliberate (room 3).

## E. Structure and splits

- [-] actions.c `ExecuteLineCode`/`PerformAction` return an int *and* use a
      `*done` out-parameter — two ways to stop; simplify.
      → Declined: the two signals mean different things (stop this line / stop the table).
- [x] player.c `Look()` (~150 lines: low objects, high objects, exits,
      inventory, graphics) → split; `glk_main` → load/identify, init, loop.
      → 2026-10-14: `Look()` calls `ListLowObjects()`, `ListHighObjects()` and `DrawRoomPicture()`;
        `glk_main` calls `IdentifyGame()` and `StartGame()` around the window/graphics setup.
- [~] player.c `RunOneInput`: `if (Redraw) Look(); return;` ×4; carry-forward
      `Word[]` shift. `LastNoun`/`LastVerb`/`GoVerb` belong in parseinput.c.
      → Carry-forward is a `memmove`. Statics left in player.c (`LastVerb` is reset from parseinput.c).
- [~] textoutput.c scattered statics/globals (`periods`, `JustWrotePeriod`,
      `Upper`, `LastChar`, `PendSpace`, `FirstAfterInput`) → one output-state
      struct; `Message2` doesn't reset `InventoryLower`.
      → `Message2` now resets `InventoryLower`. State struct declined (golden risk, no gain).
- [~] Game-specific text hacks (Blizzard token 0x2d in room 49, ToT object 41
      "door.", message 156 InventoryLower, QP3 sysmessage offsets) → named
      constants / patch table.
      → QP3 and Blizzard offsets named; the per-token hacks stay inline.
- [-] `PatchAndDrawQP3Cannon` lives in graphics.c but is called from
      actions.c; QP3 hacks (`&= 191`, image 46) are spread over four files.
      → Declined 2026-10-14: the cannon patch writes into `imagebuffer`, which is private to
        graphics.c, so it is in the right file; the other hacks sit where the thing they patch is.
- [x] parseinput.c `TryExtraCommand`: `noun` mixes `extra_command` enum
      values with dictionary codes (`Word[1]` equal to e.g. `GAME` is
      misread); `noun == 0` vs `NO_COMMAND`; enum `SAVE_EXTR`. `ParseWord`
      calls `ParseExtraCommand` only for its side effect.
      → New `OTHER_WORD` enum value keeps dictionary codes apart; `NO_COMMAND` used.
- [x] `LineInput` has an unreachable "Too many words!" branch and
      `SysMessage(8/9/10)` / `MyLoc != 6` literals.
      → Literals named. The branch *is* reachable (at exactly `MAX_WORDS`); comment corrected, behaviour kept.
- [-] `Display(Bottom, …)` vs `OutString` vs `glk_put_string_stream` used
      inconsistently for messages.
      → 2026-10-14: the two transcript notices now use `Display`. The `Display`/`OutString` split
        is deliberate (interpreter notices vs. buffered, word-wrapped game text) and stays.
- [x] ui.c `WordFlush` emits one `Display(win, "%c")` (vsnprintf into a
      2048-byte buffer) per character → one `glk_put_buffer_stream`.
      → One `glk_put_buffer_stream`.
- [-] alkatraz_screen.c `AlkatrazScreenOrder` (100 lines, two near-duplicate
      vertical/horizontal advance branches) — optional split.
      → 2026-10-14: `alk_load_slot()` and `alk_advance()` split out. Draw order compared against
        the old function on the three real descriptor tables and 20000 random ones: identical.
- [~] Error handling: `fprintf(stderr)` + `glk_exit()` is invisible in the GUI
      (also `InitGraphics`); route through one `Fatal()`-style helper that
      prints to the Bottom window.
      → New failure paths use `Fatal()`, and so do the three load failures (no verb table, game
        not recognised, no flag data), which now show in the game window. `InitGraphics` does not
        exit: it goes text-only and says so in the game window (2026-10-14). The missing-argument
        usage line still exits silently.
- [-] `#ifdef DEBUG` / `#ifdef SPATTERLIGHT` sprinkled in logic files.
      → Not done.
- [-] Gather `FileImage` + the 12 table-offset globals into a `TaylorFile`
      struct. (See F.)
      → Declined, see F.

## F. Probably decline / not worth it

- A global state struct replacing `Flag`, `ObjectLoc`, `Game`, `FileImage`
  and the `Version/CurrentGame/BaseGame` macros: large and risky for a
  ZX/C64 game-data interpreter; accept the globals.
- A single `gamefixes.c` for *all* per-game hacks: worthwhile in table form
  (section D) but moving the code itself would obscure the interpreter flow.
- decrypttotloader.c: the file's own header explains it is a literal C
  transcription of the Z80 passes; leave as is.
- `*_loadscreen_data.h` define `static const` arrays in headers; they are
  only included by extracttape.c, so the per-TU copy is harmless.
- Freeing the `images` / patched image buffers at exit: process lifetime.

## G. Second survey (2026-10-14)

Found by reading actions.c, player.c, parseinput.c, extracommands.c, ui.c,
textoutput.c, gameload.c, restorestate.c, graphics.c, animations.c,
c64decrunch.c, extracttape.c and loading_screen.c after the passes above.
Worked through 2026-10-14; outcomes are on the `→` lines. Grep by function
name; line numbers drift.
Not read this time: gameinfo.c, decrypttotloader.c, alkatraz_screen.c, test/.

### G1. Dead code and stale text

- [x] The `SysMessageType` members `MOVE_UNDONE`, `CANT_UNDO_ON_FIRST_TURN`,
      `NO_UNDO_STATES`, `TRANSCRIPT_*`, `STATE_SAVED`, `NO_SAVED_STATE` (taylor.h)
      are not used anywhere; extracommands.c and restorestate.c hard-code the
      English in `Display(Bottom, "...")`. Delete the members, or use them.
      Same for "Please answer Y or N.", "Restart? (Y/N) ", "Taken", "Dropped",
      "You have nothing to drop. ".
      → Deleted the unused members (`EXITS_DELIMITER` … `NO_SAVED_STATE`,
        `LAST_SYSTEM_MESSAGE`); the enum comment now says the interpreter's own
        messages are written out where printed. `NO_HELP_AVAILABLE` stays: position matters.
- [x] `RamSave(int game)`: all three callers pass 1. **(verify)** the parameter
      is unused in the body, then drop it.
      → Unused; now `RamSave(void)`.
- [x] animations.c leading comment ("inefficient in two ways ... second buffer
      array") predates the `Shown.cells` diffing in graphics.c. Update or delete.
      → Rewritten: `DrawBufferedPicture` paints only changed cells; stars,
        force field, Queen and click shelves paint directly, so repaint their whole area.
- [x] Misplaced doc comments: `UpdateSettings` sits above `SetExclusiveOption`
      (ui.c); the `LineInput` comment in parseinput.c is separated from its
      function by the `Q3_MSG_*` defines; the `terror_menu` comment in
      c64decrunch.c is split from it by the TRIANON/HYBRID defines.
      → Moved all three.
- [x] `LineInput` do-while condition (`WordsInInput == 0 || InputWordStrings
      == NULL`) is the only way the loop repeats, since the success path
      returns inside; make it `while (1)` or move the check.
      → `while (1)`; dropped the redundant `WordsInInput = 0`.
- [x] `FlushRoomDescription`: `glk_window_get_size(Top, ...)` called four
      times; the `strlen == 0` check is unreachable (the string always ends
      in `\n`); `o2` is a bad name.
      → One `get_size` before the arrangement change removed (the one after
        stays: the arrangement may change the height); unreachable check gone; `o2` → `pair`.
- [x] `JustWroteNewline` and the forward declaration of `WordFlush` sit
      mid-file in ui.c/textoutput.c; move to the top or the header.
      → Moved to the top statics; `WordFlush` moved above `PrintCharacter`.
- [x] c64decrunch.c: `DetectC64` tests `c64_registry[i].type` after copying the
      row into `record`; use `record` throughout. The `Fatal("Unknown type")`
      branch is unreachable for the registry as written.
      → Uses `record` throughout; unreachable `Fatal` removed (else = T64).
- [x] c64decrunch.c includes "utility.h" and "taylor.h"; **(verify)** both are
      needed (`MemAlloc`, `Fatal`, `Display` are in utility.h/ui.h).
      → Verified both are needed; no change.
- [x] `CompanionFile` (gameload.c) is a file-scope static used only by
      `LookForSecondTOTGame`; make it local and free it on every exit path
      (check the hybrid text-only branch that leaves it unused).
      → `LookForSecondTOTGame` rewritten with a local `companion`, one restore
        path and a free on every reject; `UnparkFileImage` gone too. Covered by the tzx `-combined` rows.
- [x] Build products `taylor_hl`, `taylor_image_probe`, `headless_obj`,
      `headless_text_obj` in the source dir: **(verify)** they are ignored by
      .gitignore, and that `make clean` removes them.
      → Ignored by .gitignore and removed by `make clean`.

### G2. Named constants still missing

taylor.h names many flags and objects; these literals remain.

- [x] Flags: `Flag[7]` (QP3 restedness; DIAGNOSE and `WaitFlag`), `Flag[42]` /
      `Flag[43]` (dark flags, SWITCHINVENTORY, `DarkFlag()`), `Flag[59]` (QP3_64
      Blob message 44 reset), `Flag[63]/[28]/[12]/[60]` (ToT guard/puzzle),
      `Flag[10]` (Kayleth droid pursuing, animations.c), `Flag[118]`.
      → In gamedefs.h: `Q3Restedness`, `Q3OtherGuyDark`/`Q3_DARK_FLAG`,
        `Q3_64BlobFlag`, `TOT_GUARD_FLAG`/`ToTGuardBlinded`/`TOT_SHARED_FLAG`/`TOT_PUZZLE_FLAG`;
        `KaylethDroidPursuing` in animations.c. `Flag[118]` is only in a comment.
- [x] `Q3XanduPresent (Flag[39])` and the `Q3_*` enums live in actions.c while
      `Q3SwitchedWatch` etc. are in taylor.h, and `Q3_OPENING_ROOM` is in
      parseinput.c (actions.c's `MyLoc == 6 || MyLoc == 0` is the same room).
      Gather the QP3/ToT/Kayleth named flags, objects and rooms in one header.
      → New gamedefs.h holds the QP3, ToT and Blizzard Pass flags, objects,
        rooms, messages and words; the names come from a dump of the games' object text and
        dictionaries. Room 6 is the small office (`Q3_ROOM_OPENING`).
- [~] Objects: `ObjectLoc[7]` (Q3UpdateFlags torch/thing fixup),
      `ObjectLoc[48]` and `arg1 == 45` (ToT serpent guard, LET and IMAGE),
      `ObjectLoc[10]/[11]/[46]` and words 54/44/77/106/121 (QP3 fixups in
      `Parser`), Blizzard words 249->43, 175->44, 134->49, `PrintObject`
      object 41, `ObjectLoc[0]` and `[122]` (animations.c).
      → Named: QP3 objects 7, 10, 11, 46, 48 and words; Blizzard words; ToT
        object 41. Correction: object 48 / picture 45 in `IMAGE` are QP3 (the fallen statue), not
        ToT. Word 106 is RING/MAST (the Ringmaster), 121 ALIC/MAST. `ObjectLoc[0]` (Kayleth Azap
        chamber) left; 122 was already `KAYLETH_DROID_SLOT`.
- [~] Rooms: Rebel objects 42/107/92/58/36 and rooms 86/88/29-34/50/71 (only
      `UNFOLDING_SPACE` is named); Kayleth rooms 1/2/3/32/36/52/53/56 and
      `KaylethAnimations[2]`; `MyLoc == 250` (Rebel) and `MyLoc == 15`
      (Kayleth) in `RunOneInput`; `MyLoc == 83` He-Man; `MyLoc == 49`
      Blizzard token 0x2d; QP3 `IsDir` words 57/60; `NumObjects()` 49/120;
      `WaitFlag()` 5/7; `GuessLowObjectEnd` 70/49; `Message(31)`.
      → Rebel rooms/objects named in animations.c, and a `RebelAnimationHere()`
        replaces the room tests duplicated in `UpdateRebelAnimations`/`StartAnimations`. QP3
        rooms 2/3/34 and `IsDir` words 57/60 named. Not done: Kayleth rooms other than 3,
        `MyLoc == 250`/`15` in `RunOneInput`, He-Man 83, Blizzard 49, `NumObjects`/`WaitFlag`/
        `GuessLowObjectEnd` values, `Message(31)`: each is used once beside a comment.
- [-] Animations: Kayleth speed `switch` literals (14, 10, 40, 80, 50, 12,
      150, 40, 20, 30) and `ClickShelfStage`/`ShelfScrollStage` 9/7/80.
      → Left: each `case` already says which room it is; the speeds are tuned
        by eye, a name would add nothing.
- [~] graphics.c: QP3 tile count 246, `InitIrmak(..., 4)`, the image opcodes
      `0xfe/0xef/0xee/0xeb/0xf3/0xfa/0x82` in the decompressors; the QP3
      status-table padding `0x7e` in `RunStatusTable` is the `MSG_END` value.
      → Decompressor bytes named (`IMAGE_END`, `REPEAT_COMMAND`, `TOKEN_*`).
        Left: 246, `InitIrmak(..., 4)`, the QP3 `0x7e` padding.
- [x] `(size_t)-1` is compared as `== -1` in many places (FindCode,
      FindTilesStart, ...). Define `NOT_FOUND` once.
      → `NOT_FOUND` in gameload.h, used for every `FindCode` result.
- [~] ui.c: `OutCount > 127` against `OutWord[128]` (use `sizeof`); `buf[512]`
      / `511` in `LineInput`; `Word[5]`, `WordPositions[5]`, `offset < 5`
      against a `WORD_COUNT` constant (`MAX_WORDS` exists in parseinput.h
      but is the input word limit, not this); `IMAGE_WIDTH_IN_PIXELS 255`
      against the irmak constants. **(verify)** 255 vs 256.
      → `OutCount` is `size_t` and checked against `sizeof OutWord` (this also
        fixed an overflow of `OutWord` when `TopWidth` > 128); `WORDS_PER_COMMAND` replaces the
        5s. `buf[512]`/511 left (see G5). `IMAGE_WIDTH_IN_PIXELS` not checked.
- [x] c64decrunch.c: the 17 `HYBRID_*`/`TRIANON_*` values are named, but
      `terror_menu` still hard-codes track/sector pairs (17,1) and (20,16);
      give them names. The registry's `parameter` field is unexplained at
      the call: it is the iteration on which `switches` applies.
      → `TRIANON_GRAPHICS_TRACK/SECTOR`, `TRIANON_TEXT_TRACK/SECTOR`;
        `parameter` → `switch_pass`. Also fixed the menu typo "one with without pictures":
        72 C64 ToT script rows re-blessed for that line alone.
- [x] extracttape.c: `0xeeab` / `0xeea9` (ToT side B descriptor table and
      count) and the `FixBrokenKayleth` addresses are unnamed; the Blizzard
      Pass block numbers `{7,11,13,15,17}` / `{11,15,17,19,21}` are per-TZX/TAP
      tables and could be one 2-row table indexed by `is_tzx`.
      → `TOT_SIDE_B_SCREEN_WINDOWS/DESCRIPTORS`, and the window count is now
        bounded by the snapshot size. `FixBrokenKayleth` has a comment per
        step instead of names. Blizzard Pass: one `{tzx_block, offset}` table;
        a TAP has 4 more blocks before each. Loading screens, draw orders and
        lengths of all 30 Spectrum corpus files hash the same as before.

### G3. Deduplication

- [x] `FindCode("NORT\001N", 0, 6)` (verb-table signature) is repeated in
      `IdentifyGame` (player.c) and `LookForSecondTOTGame` (gameload.c):
      one `FindVerbTable()`.
      → `FindVerbTable()` in gameload.c.
- [x] `RunOneInput`: `if (Redraw) Look(); return;` appears three times in the
      extra-command paths, and `IsDir(OriginalVerb) || (Word[0] == GoVerb &&
      IsDir(Word[1]))` twice. Fold into a helper or an early-return layout.
      → `LookIfRedrawn()` and `IsMoveCommand(verb)`.
- [x] `Display()` (ui.c) and `WriteToRoomDescriptionStream()` (textoutput.c)
      both vsnprintf into a 2048-byte buffer; share one formatter.
      → `PrintToStreamV()` in ui.c.
- [x] `PrintCharacter`: the Bottom and Top branches repeat the buffer-append
      and whitespace logic.
      → `AppendToWord(c, win, limit)`.
- [x] `LookForSecondTOTGame`: `UnparkFileImage(...)` is called from four early
      returns; use one exit label.
      → See `CompanionFile` above.
- [x] `Q3Condition`/`Q3Action` and `Q3AdjustConditions/Actions`: one
      `TranslateQ3(op, &arg1, &arg2)`. The NULL guards on arg1/arg2 exist
      only for the DEBUG call with dummy pointers.
      → `Q3TranslateCondition`/`Q3TranslateAction` translate the opcode
        and its flags; the DEBUG trace now prints after translation, so the NULL guards went.
- [x] `Carried() || Worn()` is spelled out in CARRIED/NOTCARRIED, Inventory,
      GetObject and Present: add `HeldByPlayer(obj)`.
      → `HeldByPlayer(obj)`, also used by `Present`.
- [x] `Abbreviations[]` / `AbbreviationValue[]` (parseinput.c) are parallel
      arrays; use a struct table as was done for `ExtraCommands`.
      → One `{letter, word}` table.
- [-] restorestate.c writes Flag + ObjectLoc three ways (SaveCurrentState /
      RestoreState memcpy, LoadGame's two `glk_get_buffer_stream`, SaveGame's
      two `put_buffer`). Share one helper or read/write the `SavedState`.
      → Two `memcpy`s, two reads, two writes: already plain, and the save
        file layout (Flag then ObjectLoc) reads off the code. A helper would hide it.
- [x] `BEEP`: 25-line comment and three `#if` variants in `PerformAction`;
      move to `PlayBeep(arg1, arg2)`. The `IMAGE` case's per-game fixups
      (room 3 DrawBlack, statue 45 / object 48) likewise.
      → `PlayBeep(duration, pitch)`. The `IMAGE` fixups stay in place (two
        lines, now named).
- [x] `SWITCHINVENTORY` swaps by hand; `PerformAction` is ~260 lines and
      shares one `tmp` across cases (declare per case).
      → `SwapBytes()` for SWAPF and SWITCHINVENTORY; no shared `tmp`
        (ADD/SUB clamp inline, SWAP scopes its own).
- [x] `PatchAndDrawQP3Cannon`: four near-identical `IRMAK_IMGWIDTH * 8 + n`
      lines; loop over a small table.
      → Loops over a `bright_cells` table.
- [x] `AnimateKaylethClickShelves`: ink and ink2 are both computed every
      iteration of the 4-deep loop although a half uses only one.
      → One ink per half. Equivalent by inspection; animations are not
        covered by the headless suites.
- [x] extracttape.c: `ProcessAlkatrazTape` and the three `ExtractSpec` /
      `AlkatrazTape` rows repeat `*_SCREEN_*` constants per game; a macro or
      a screen-spec struct in each `*_loadscreen_data.h` would make the
      table one line per tape.
      → `AlkatrazScreen` (loading_screen.h); each `*_loadscreen_data.h`
        defines one, and `AlkatrazTape` is one line per tape. Also fixed the
        Kayleth comment "tape block 4": it is block 3, counting from 0.
- [-] `DetectC64`: the D64 largest-file and T64 first-record branches each
      free `*sf` and replace it; `GetFileFromT64`'s caller in `terror_menu`
      does the same twice. One `ExtractFromContainer(layout, ...)`.
      → Left: the branches differ in what they free and keep; a shared
        helper would need flags for each.

### G4. Structure and readability

- [~] `TryExtraCommand`: relies on enum ordering (`verb > SCRIPT`) to detect
      "the first game word is the second word"; derefs
      `InputWordStrings[verb_position]`. **(verify)** it cannot be reached
      with `InputWordStrings == NULL` from the `Word[0]==0 && Word[1]==0`
      path in `RunOneInput`. The `switch` is a `{verb, noun-set, handler}`
      table. The `_EXTR` suffixes exist only to dodge enum clashes.
      → Verified: `LineInput` loops until a word is entered, so
        `InputWordStrings` is never NULL when `RunOneInput` runs. Table and `_EXTR` not done.
- [x] `ParseWord` sets `FoundExtraCommand` as a side effect of looking a word
      up; the hidden coupling with `Parser` and `TryExtraCommand` (which
      resets it) wants a single owner.
      → `ParseWord` only looks up. `Parser` sets `FoundExtraCommand` when a
        word is not found but `IsExtraCommand(word)` (extracommands.c).
        The reset in `TryExtraCommand` was redundant and went.
- [~] gameload.c: `FindFlags` is nested if/else with fall-through;
      `TokenClassify` classifies by side effect and writes `Version = 0`
      through the `Version` macro, i.e. into the shared `games[]` row
      (**verify** the intent: NO_TYPE rather than REBEL_PLANET_TYPE); return a
      bool instead. `DetectGame` loops through the "Unknown game" row and
      sets `FileBaselineOffset` as a side effect; `FindTokens` re-scans the
      file per candidate; the `glkunix_startup_code` option loop uses
      `argv++; argc--` and then `argv[1]`.
      → `TokenClassify` deleted: an `abort()` probe showed it never fires on
        the whole corpus, and if it did it would set NO_TYPE, which is wrong; the type comes from the
        `games[]` row. The rest not done.
- [-] player.c: `Destroyed()`, `Carried()`, `Worn()` are exported functions
      returning a constant or a flag; inline in taylor.h. The QP3/non-QP3
      differences (`NumObjects`, `WaitFlag`, `CarryItem`, `DropItem`,
      `DarkFlag`, `Worn`, `Present`) are `Version == QUESTPROBE3_TYPE` checks
      scattered through the file: group them or put the values on `GameInfo`.
      → Left: small and exported; `HeldByPlayer` covers the common use.
- [x] `TopWidth` is `glui32` and compared with `int` (`OutCount == TopWidth`).
      → See the `OutCount` item in G2.
- [~] graphics.c: `ExtractImage` copies `instructions` into a
      MAX_INSTRUCTIONS stack buffer and then memcpy's again; decompress
      directly. `DecompressHemanType` takes no capacity parameter unlike
      `EarlierType`. `AdjustBlizzardPassImageWidth`'s list wants a table.
      `#pragma mark` and `/* --- */` section markers are mixed.
      → `DecompressHemanType` takes a capacity. Left: `ExtractImage`'s stack
        buffer (the length is only known after decompressing), the Blizzard switch (six cases),
        the section markers.
- [x] animations.c: `KaylethAnimations` is `MemAlloc` + a NULL loop; use
      `MemCalloc`.
      → `MemCalloc`.
- [x] actions.c `DropObject`: an unresolved FIXME ("check if this is how the
      real game behaves"). Check against MAME / the Z80 code, then remove.
      → Checked in the Z80 code with Ghidra: He-Man's action table is at
        0x6C45 (DROP 0x6A20, GET 0x69DF) and the same DROP bytes are in all
        five non-QP3 Spectrum snapshots; QP3's tables are at 0x96A8
        (conditions) and 0x991C (actions; DROP 0x97AB, GET 0x976B).
        Fixed, all checked against that code:
        - DROP of a worn object puts it in the room, with no message and the
          carried count unchanged (we printed "You are wearing it.").
        - GET of a worn object prints YOU_ARE_WEARING_IT (we printed
          YOU_HAVE_IT).
        - A failed GET or DROP sets the loop's stop flag (0x5DFC in He-Man,
          0x5BE6 in QP3) in every game, which ends the table scan at once. We
          did this only for Rebel Planet's DROP, and for QP3's GET without
          leaving the line.
        - The loop sets that flag *before* an action with bit 6 and returns
          after it, so the rest of the line is skipped. We ran the rest.
          It matters once in the corpus: a He-Man LOOK line goes on to
          `LET 4 11`, so every LOOK raised MaxCarried from 7 to 11.
        - QP3 has nothing worn: its CARRIED condition and inventory test only
          Flag[2], and the game has no "worn" text. `Worn()` returns 0
          there, which is a real room, so `HeldByPlayer` counted objects in
          room 0 as held. It now ignores Worn() for QP3. The watch is carried
          (in Thing's inventory, 254), not worn.
        New `worn-drop` script (4 Blizzard Pass rows: wear, get, drop, drop
        again). No walkthrough moved.
        Left: action 8, our PRINTOK ("Guess"), is 0x6997 in He-Man: it only
        sets the stop flag, like QP3's DONE. Where "Okay" is printed from
        (perhaps flag 0x5B85, which every failure clears) is not traced.
        Also left: QP3's inventory loop covers objects 0-47, while our
        `NumObjects()` gives 49 for QP3.
- [x] loading_screen.c: `DrawZXTitleImage` closes and reopens all three
      windows by hand, as the Scott title routine does; **(verify)** whether a
      shared helper exists in the common code before keeping two copies.
      `MAX_TAPE_BLOCKS` (64) silently truncates a longer tape: say so in the
      comment or return the count.
      → Verified: there is no shared helper. Scott has its own
        `OpenTitleWindows`/`CloseTitleWindows`, and zx_title.h leaves the
        windows to the caller, so two copies stay. `MAX_TAPE_BLOCKS`
        comment now says a screen further in is not found. Possible
        follow-up: `FindTapeLoadingScreen` is almost the same in
        scott/title_image.c and could move into decompressz80.

### G5. Possible behaviour issues (each needs a test row)

- [x] **(behaviour)(verify)** `Chance(n)`: `erkyrath_random() % 100 <= n`
      is n+1 percent. Compare with the original Z80 routine (MAME). If the
      original is `< n`, fix it and note every seeded transcript that moves.
      → The original is `< n`. He-Man (0x688D, Ghidra) and the other
        non-QP3 games compute floor(100 * b / 256) for a byte b from an
        8-entry table indexed by FRAMES & 7, and succeed if that is below
        n. QP3 uses FRAMES mod 100, also `< n`. Fixed. Moved: he-man (9 rows:
        one "The earth tremors more violently." fewer) and rebel-planet (8
        rows: Energy 1000 instead of 990 once). Both still end as before;
        re-blessed.
- [x] **(behaviour)(verify)** `LineInput` buffer: `buf[length] = 0` with
      `length == ev.val1`; the request is 511 chars into 512, so fine, but
      check `ev.val1` is never larger on a Glk that ignores the length.
      → Glk guarantees `val1` ≤ the requested 511; no change.
- [x] **(verify)** `GetFileFromT64` treats `number_of_records == filenum` as
      "the last file runs to the end of the image", while `end_addr` of the
      last record may be correct; check a T64 with trailing garbage.
      → Verified: every corpus T64's last record has a correct end address,
        and the only caller (`terror_menu`) reads records 1 and 2 of a
        3-record image, so the branch is never taken. Kept, as some T64
        writers get the end address wrong; the doc comment now says so.
