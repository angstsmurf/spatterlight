# terps/scott: refactoring, cleanup and deduplication TODO

No open items. The review of 2026-10-15 gave three, all done (the commit
that closed this list). The review of 2026-10-14 gave eight: six are done
(161801bf0, 6ba49fd65, 545bf11fa and the commit that closed this list),
the TI-99/4A command switch and parts of the long-function item were
turned down, see the end of "Looked at and declined". The review of
2026-10-11 (after the two cleanup passes, d24161fc4 and 5bb6785f3) gave
seventeen; fourteen were done and three turned down. Kept here: how to
validate a change, the state of the warning pass, what the corpus audit
still reports, and the items that were looked at and turned down, with the
reason, so they are not proposed again.

Validation: `make -f Makefile.headless check formatcheck scriptcheck
companioncheck savecheck`, `make -j8 all-tests` in `saga/test` for anything
touching renderers or loaders, and the Xcode `scott` target (`-arch arm64`
when building that target on its own, or the link fails on the arm64
libraries). Run `savecheck` after touching scott_meta.c or restore_state.c.

Warning pass: `clang -fsyntax-only -Wall -Wextra -Wmissing-prototypes` over
the headless sources, with the include paths and defines of
Makefile.headless, gives 6 missing-field-initializers (declined below)
and nothing else. It is clean for missing-prototypes,
unused-parameter and unused-variable; keep it so. Adding `-Wcast-qual
-Wformat=2 -Wshadow -Wundef -Wredundant-decls -Wunreachable-code
-Wstrict-prototypes` gives three more, all declined below: two `-Wcast-qual`
(`FreeStringArray` in scott.c, `TRANSCRIPT_ON` in scott_meta.c) and the
`AUTOWIN` early return of `battle_loop` (seas_of_blood.c) as unreachable
code. Run it under bash: zsh does not word-split `$INC`. Where a
parameter or variable is read by `debug_print` only, it gets a `(void)`
cast with a comment (`PrintHeaderInfo`, `Skip`, two in rtpi_graphics.c).

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
- The slow-draw tick of the Apple II and Atari 8-bit vector drawers is
  `VectorOpListPlayChunk()` (saga/vector_oplist.h); a drawer supplies a
  step function that returns next / extend / pause.
- `MapSysRange()` returns the next source index, so consecutive ranges
  chain instead of each start index being typed.
- Per-game item and room numbers get a file-local `enum` (`ROOM_KITCHEN`,
  `HULK_ITEM_BIO_GEM`), checked against the game data.

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

- **Folding the inline close-ups of the game actions into
  `ShowCloseup()`** (game_specific.c, gremlins.c, robin_of_sherwood.c).
  They differ in output: Adventureland prints a newline before the
  prompt, Gremlins and Robin of Sherwood use `Display(Bottom, "\n%s\n")`,
  Robin does not set `showing_closeup`, and the Secret Mission ID card
  draws a label over the picture. (The examine close-ups are a table
  already: each release's own, read by 06562d976 and 5f57e3f46.)
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
  `CloseTopWindow`, `OpenGraphicsWindow`, `CloseGraphicsWindow`), apart
  from the Seas of Blood battle screen, which opens a status window for
  the battle when there is none (seas_of_blood.c); `split_screen` is
  cleared by the `-w` option and when the status window cannot be
  opened.
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
  28 outside the tests, 6 of them the reports of the saga tests
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
  cast at the six sites where neither fits.)
- **Sharing `ExecuteOpcode()` with the TI-99/4A command switch**
  (ti99_4a_terp.c). The cases that are the same are already one call each
  (flags, dark/light, goto, swaps, counters, save, print noun). The rest
  differ: drop, destroy and force-carry set the location without the
  room redraw, counters are signed 16-bit and go below -1, LOOK/LOOK_2,
  clear screen, newline and inventory have no Spectrum-style, US or Seas
  of Blood branches, and auto-inventory, success on/off and the message
  ranges are TI-only; parameters come from the bytecode rather than the
  action line. That is most of the switch, not 4-5 cases.
- **One `SetUp*Specifics()` per family in `SetUpGameSpecifics`**
  (detect_game.c). The longest case body is the `default`, at 14 lines,
  for the Mysterious Adventures messages; the other long ones are already
  calls (`LoadExtra*Data`, `*Sysmess`).
- **A shared image-matching step for `ExtractImagesFromAtariCompanionFile`
  and `ExtractImagesFromApple2CompanionFile`.** The Apple II one hands the
  disk to `ReadA2VectorImageFiles`/`ReadA2BitmapImages`; the Atari one walks
  its own picture table and cuts out the VTOC sector. They share the game
  whitelist and `FreeUSImagesIfEmpty()` only. The Atari crop rules are
  `SetAtari8ImageCrop()` now.
- **The three warnings left by the `-Wcast-qual -Wformat=2` pass.**
  `Verbs`, `Nouns` and `Messages` are `const char **` for the parser,
  which only reads them, but the loaders allocate the strings, so
  `FreeStringArray` casts the `const` away to free them: one cast instead
  of eleven in parser.c. `glk_put_string_stream()` takes a `char *`, so
  the cast of `sys[TRANSCRIPT_ON]` in scott_meta.c stays. The `AUTOWIN`
  return in `battle_loop` is the headless build's dice-free battle on
  purpose.
