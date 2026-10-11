# terps/scott: refactoring, cleanup and deduplication TODO

There are no open items. The read-only review of 2026-10-11 (after the two
cleanup passes, 8ab02d171 and 5650c12ca) gave seventeen; fourteen are done
and three were turned down, see the end of "Looked at and declined". Kept
here: how to validate a change, the state of the warning pass, what the
corpus audit still reports, and the items that were looked at and turned
down, with the reason, so they are not proposed again.

Validation: `make -f Makefile.headless check formatcheck scriptcheck
companioncheck savecheck`, `make -j8 all-tests` in `saga/test` for anything
touching renderers or loaders, and the Xcode `scott` target (`-arch arm64`
when building that target on its own, or the link fails on the arm64
libraries). Run `savecheck` after touching scott_meta.c or restore_state.c.

Warning pass: `clang -fsyntax-only -Wall -Wextra -Wmissing-prototypes` over
the headless sources, with the include paths and defines of
Makefile.headless, gives 20 sign-compare and 6 missing-field-initializers
(both declined below) and nothing else. It is clean for missing-prototypes,
unused-parameter and unused-variable; keep it so. Where a parameter or
variable is read by `debug_print` only, it gets a `(void)` cast with a
comment (`PrintHeaderInfo`, `Skip`, two in rtpi_graphics.c).

Conventions the last pass settled:

- `LoadTrace(...)` (scott.h) is the loader trace, shown with the `-d`
  option; `ACT_DEBUG(...)` (scott_actions.h) is the action trace of both
  interpreters, compiled in with `DEBUG_ACTIONS`.
- `DetectGame()` owns the game file handle and closes it; `LoadDatabase()`
  never does.
- New `USImage` list nodes go through `AppendImage()` (sagagraphics.h,
  `static inline` so the saga tests that stub `NewImage()` still link).
- `ParseItemSlashAutoGet()` is in scott.c next to `MatchUpItem()`.
  `ParseItemDotAutoGet()` stayed `static` in detect_game.c, which has its
  only caller.

---

## Corpus audit of action-line arguments

`scott_cond_audit` (test/cond_audit.c) checks every condition and opcode
parameter of every action line, and the TI-99/4A bytecode, against
NumItems / NumRooms / 64 flags. Run over all of
test/formats/games.manifest.tsv plus the loose .dat files (291 images), it
reports nothing but these, all harmless:

- Ten Little Indians `z80-m10india`: line 101, `NOT_PRESENT(217)` of 73
  (the .tzx and the other .z80 have 0 there). An item above NumItems is
  treated as not in play (`ItemExists()`), so nothing is read past
  `Items[]`.
- Waxworks (Spectrum; the C64 table is right) line 95 has opcodes 72 72 52
  with four parameters, so the GET reads a fifth, which `PerformLine`
  zero-fills: item 0. An authored slip.

Any new finding is more likely a loader offset or a damaged image than
game data: that is what all the earlier ones were (Spiderman Atari 16K,
Escape from Pulsar 7 C64, Hulk C64, Voodoo Castle C64). No flag above 31
is used, so `TestBitFlag`'s shift is safe.

---

## Looked at and declined

- **Row table for the examine closeups** (game_specific.c, hulk.c). The
  predicates are not uniform: item in the room only, item present, two
  items or-ed, `Items[44].Location == 0 && IsPresent(50)`, else-if chains
  that pick between images, and several nouns per row. A table needs a
  predicate kind, two items and a noun list per row and reads worse than
  the switches. The inline closeup triplets in game_specific.c, gremlins.c
  and robin_of_sherwood.c differ in output details; folding them changes
  output.
- **Family-default macros for `games[]`.** The table uses designated
  initialisers now, with zero fields left out, so an entry only lists what
  it has. The shared values (0x2451, 0x3a50, 0x2539, 0x28de) are common to
  four entries at most; a macro for each family hides the offsets one
  greps for.
- **Table for `DrawGameSpecificOverlays`** (saga.c). The per-game item
  checks mix predicate kinds and would need a `kind` column; the Return to
  Pirate's Isle branch does not table at all.
- **Splitting `TryLoading`** (detect_game.c). It is already one
  `SEEK_OR_FAIL` plus one `Read*()` call per table, in file order; the
  macro returns from the function, so helpers would each need the
  seek state passed in and a result checked.
- **Splitting `UpdateGremlinsAnimations`** (gremlins.c). A flat switch on
  the room, a few `DrawImage()` calls per case. One function per room is
  not clearer.
- **Splitting the rest of `CommandFromStrings`** (parser.c). The direction
  case is `DirectionCommand()` now. What is left is one decision chain over
  shared state (`verb`, `list`, `i`, `verbindex`, `lastverb`, the
  noun-before-verb flag) with an early return per outcome.
- **Splitting the enemy table, battle messages and image data loads out of
  `LoadExtraSeasOfBloodData`.** Five lines each; the system message map
  and the DROP HELMET patch are separate functions now.
- **A live `GameState` struct** holding `Counters`, `RoomSaved`,
  `BitFlags`, `MyLoc` and the rest. Several hundred references across
  every file, for no change in behaviour. The copying is in one place
  instead: `SaveGame`/`LoadGame` go through the `SavedState` snapshot of
  restore_state.c, so only `SaveCurrentState`/`RestoreState` touch the
  globals, and a bad save file no longer overwrites the game before it is
  rejected.
- **Moving the title screen's window handling out of title_image.c.** It
  tears the whole window tree down and builds it again around a picture,
  which is its job. `Bottom`, `Top` and `Graphics` are otherwise only
  assigned in scott_display.c (`OpenBottomWindow`, `OpenTopWindow`,
  `CloseTopWindow`, `OpenGraphicsWindow`); `split_screen` is cleared by the
  `-w` option and when the status window cannot be opened.
- unzip_in_mem.c is vendored minizip: left alone, apart from the `filesize`
  parameter of `unzGoToNextFile()`, which nothing read.
- **Predicate table for `ConditionHolds`.** With the `ACT_DEBUG` macro the
  switch is ~110 lines of three-line cases; a table would need 20 one-line
  functions plus the debug strings and reads worse.
- **`{verb, mask, handler}` table for `PerformExtraCommand`.** The guards
  are not uniform (`noun == 0 || GAME`, `noun == 0 || COMMAND`,
  RAMLOAD/RAMSAVE, ON/OFF pairs, EXCEPT), so the table needs two rows per
  ON/OFF verb and a handler per row for a 75-line switch.
- **`WriteToRoomDescriptionStream` through `vDisplay()`.** It writes to a
  non-Unicode memory stream; the only shared part is one `vsnprintf`.
- **`parser_tables.h`.** parser.h is ~90 lines; splitting the language
  tables out buys nothing.
- **One shared `Fatal()` stub for the saga tests.** The four copies are
  one-liners in separately linked binaries, some of which also link
  extract_stubs.c; a shared definition needs an opt-in macro that is longer
  than the stub.
- **Moving `vector_image_shown` to vector_common.c / calling `TimerDelay()`
  from line_drawing.c.** zxvectortest unity-includes line_drawing.c without
  linking vector_common.c, so both changes only move a stub from filltest.c
  to zxvectortest.c. The delays in `TimerDelay()` are named now.
- **`SEEK_OR_FAIL` for the two raw `SeekIfNeeded` calls in detect_game.c.**
  They do not return `UNKNOWN_GAME` on failure (`LoadVectorData` is `void`;
  the system-message seek returns `info->gameID`), so the macro does not fit.
- **Renaming `xstrncasecmp`.** It mirrors the libc name on purpose.
- **Local `nearest()` helpers in dos_decode_png.py / ti_decode_fb.py.**
  Different palettes; a shared helper is not shorter.
- **One base list for the `messagekey[]` tables of game_specific.c.**
  Against `c64_sysmess_keys` (39 keys), `Claymorgue64Sysmess` adds
  `YOU_HAVE_IT` at 21; `Spiderman64Sysmess` adds that and `DROPPED`,
  `TAKEN` at 30; `SpidermanAtari8Sysmess` adds `NONE` at 6 and
  `YOU_HAVE_IT` at 21 and moves `INVENTORY` and `PLAY_AGAIN`;
  `Supergran64Sysmess` (30 keys) shares little. "The base list, plus these
  at these indices" for two of the four reads worse than the lists, which
  are in the order of the original tables and can be checked against them.
- **Turning the `fprintf(stderr, ...)` diagnostics into `debug_print`.**
  33 outside the tests, 6 of them the reports of the saga tests
  (vector_common.c). `debug_print` prints nothing in a normal build, so
  the change would silence them in the headless harness and the Xcode
  console, which is where a damaged image or an unhandled opcode gets
  noticed. The load failures of c64decrunch.c return `UNKNOWN_GAME`, so
  the player is told through the normal unknown-game path.
- **The 6 missing-field-initializers of a `-Wextra` build**: `{ NULL }`
  terminators and table rows that rely on the trailing fields being
  zeroed. Spelling the zeros out is noise. (The 20 sign-compare notes that
  were declined with them are fixed: mostly by giving the variable the
  type of what it is compared against or by comparing pointers, with a
  cast at the five sites where neither fits.)
