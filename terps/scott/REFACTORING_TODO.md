# terps/scott: refactoring, cleanup and deduplication TODO

What is left of a read-only review after the two 2026-10-11 cleanup passes:
no open refactoring items. Done items were removed; items that were looked
at and turned down are listed at the end with the reason, so they are not
proposed again.

Validation after each batch: `make -f Makefile.headless check formatcheck
scriptcheck companioncheck savecheck`, `make -j8 all-tests` in `saga/test` for
anything touching renderers/loaders (it has per-object rules and header
dependencies now, so no `clean` is needed), and the Xcode `scott` target
(`-arch arm64` when building that target on its own, or the link picks up
the arm64 libraries for an x86_64 binary and fails). `savecheck` covers
saving and restoring to a file, old file layouts and damaged files; run it
after touching scott_meta.c or restore_state.c.

---

## 1. Bugs and risks

Corpus audit of action-line arguments, 2026-10-11 (`scott_cond_audit`,
test/cond_audit.c: every condition and opcode parameter of every line, and
the TI-99/4A bytecode, checked against NumItems / NumRooms / 64 flags; 291
images = all of test/formats/games.manifest.tsv plus the loose .dat files).
No database names an item it does not have: all 36 TI-99/4A images and 244
of 255 standard ones are clean. The 11 images with findings are 6 titles,
and every one was a loader offset or a damaged image, not game data. Both
loader offsets are fixed (Spiderman Atari 16K: actions at 0x1891, straight
after the header; Escape from Pulsar 7 C64: actions at 0x1df3, and
`TryLoading` cuts the header's count of 195 down to the 191 lines that fit
before the dictionary), Hulk C64 is patched in `HulkUKSpecificParsing`
(15 bytes of stray text, `N0:B...SU`, over the third condition of lines
184-191), and an item above NumItems is treated as not in play and never
moved instead of reading or writing past `Items[]` or calling `Fatal()`:
`ItemExists()` guards `ConditionHolds` and every item opcode of both the
standard and the TI-99/4A interpreter. None of the damage
is unp64: the disk images are stored unpacked and the T64s decrunch to the
same bytes.

What the audit still reports, all harmless now:

- Ten Little Indians `z80-m10india`: line 101, `NOT_PRESENT(217)` of 73
  (the .tzx and the other .z80 have 0 there). Covered by the guard.
- Voodoo Castle C64 (`d64-voodoo-castle-ai-toko`, `t64-voodooc2`): line 27,
  `AT_LOC(1096)`, one byte 0x55 for 0x01. A room number, only compared with
  `MyLoc`, so the line never fires. Could be patched like Hulk if the line
  matters.
- Escape from Pulsar 7 C64 plays differently from before the fix: the
  misread table had a junk implicit line that drew a random number every
  turn, so the dust in the air vent (line 40, 10 %) now falls on other
  turns. Hence the `escape-from-pulsar-7-c64` script.
- No opcode parameter is out of range in a sound table. One authored slip:
  Waxworks (Spectrum; the C64 table is right) line 95 has opcodes 72 72 52
  with four parameters, so the GET reads a fifth, which `PerformLine`
  zero-fills: item 0. Harmless.
- Flags: nothing above 31 outside the junk lines, so `TestBitFlag`'s shift
  is safe on sound data.

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
- unzip_in_mem.c is vendored minizip: left alone.
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
- **`TEST_HOOKS` header.** `TestApple2ImageWithName`/
  `TestAtari8ImageWithName` are now declared in the headers of the files
  that define them, and filltest.c includes those instead of carrying its
  own prototypes. `RunVectorTests`/`ReadTestDataFromFile` stay in
  vector_common.h.
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
- **`origcolptr` / `draw_opcode_names` "unused variable" warnings in
  rtpi_graphics.c** (headless build only): both are used by `debug_print`,
  which compiles to nothing there.
- **`#pragma mark`** became `// MARK:` rather than plain comments, to keep
  the Xcode jump bar entries.
