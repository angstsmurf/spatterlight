# Hugo tests

`run_hugo_traces.py` records protocol traces of the Spatterlight Hugo
interpreter (`terps/hugo/heglk`), for regression checks on its window layout.
Its docstring describes how it works.
No goldens are stored, so a check compares two runs of the same tree:

    build hugo (scheme `hugo`), e.g. into $B
    python3 -I run_hugo_traces.py --build $B --out before
    ... change, rebuild ...
    python3 -I run_hugo_traces.py --build $B --out after --compare before

The last line of the output is `N changed, M new`. A pure refactor should give
`0 changed`.

A build with ASan and UBSan (`-enableAddressSanitizer YES
-enableUndefinedBehaviorSanitizer YES`) is the most useful one to trace. A
clean run has no sanitizer reports. Three engine bugs that used to show up
were fixed in 2026-10:

- AWARD.HEX moves an object to "object" 2066 (there are 37).
  `MoveObj` now leaves the new parent alone when it isn't an object, and
  `GrandParent` stops at it. The award walk used to abort; now it
  plays to the end.
- EAST.HEX is a v1.0 file, whose header the engine cannot read. The
  version check compared against `HEVERSION` (3) and let it through.
  It now refuses files before v2.0 with an error dialog.
- Saving split negative values into bytes with a signed overflow
  (`SaveGameData`). It now masks. The bytes written are the same.

horror2 stops early, probably because it needs data saved by part I.

## What the cases cover

Each game runs as a short walk, a walk with window resizes (`arrange:`
entries), fixed window sizes, and a run with graphics off. Every picture and
sound is traced by the digest of its bytes. The tracer answers `SIZEIMAGE`
with the real JPEG/PNG size of the picture last loaded, so the code that
fits and places pictures is exercised.

Every game with special handling in `hecugel.c` has cases. A Crimson Spring
is `scourge`. Its hack runs when a picture is drawn after a restore, and the
generic walk saves and restores. Marjorie's hack is matched by file name, so
the story must stay `marjorie.hex`.

Not covered:

- An arrange event that does not change the size. Spatterlight sends one
  after theme and margin changes.

## Decisions from the heglk refactor (2026-10)

These came out of the read-throughs in the former `heglk/TODO.md` (two
passes), and are recorded here so they are not reopened by accident.

- `hugo_handlearrange` checks whether anything changed and returns early,
  but its pixel clause can never hold (`heglk_get_linelength()` leaves two
  columns spare). So every arrange event redoes the whole layout. A check
  that works gave 0 trace changes. It was still not kept: the same-size
  arrange after a theme change relies on the relayout to redraw backgrounds
  and Future Boy pictures, and the suite cannot see that. The cost is CPU
  only.
- `hecugel.c` stays one file, apart from `hecugel_sound.c` (resources and
  sound, which share no state). The window code, the per-game hacks and
  `hugo_displaypicture` share about 80 file statics. Splitting them would
  turn those into externs. Gather the state into a struct first.
- The Future Boy picture size ranges in `heglk_note_future_boy_picture` stay as
  commented literals. Each range is used once, and the names would only
  repeat the comments.
- The three lists of tracked window indices (the recycler's exclusions in
  `heglk_new_wincontext`, and the resets in `hugo_unmapcleared` and
  `hugo_clearfullscreen`) differ on purpose. They are not one array.
- `menutop` is set at the first menu and never reset. A clear of the menu
  window looked as if it reset it, but that branch could never run and was
  removed. The traces don't show a game that needs a reset.
- The menu state (`mono`, `inmenu`, `menuwin`, `menutop`) is documented in
  one comment above `menutop` and not put in a struct, because the variables
  are set from too many places for a struct to help.
- `heglk_find_window_edge` does not skip cleared windows or the caller's
  own window. Callers that care check the result. Changing this would change
  layout.
- No formatter run: there is no `.clang-format`, and reformatting the whole
  file would bury blame. Fix indentation where code is touched.
- The window-context recycling path has no trace of its own, because games
  never use 256 contexts. To exercise it, build with
  `OTHER_CFLAGS='$(inherited) -DMAXWINS=8'` (ASan on) and run the suite.
  In 2026-10 that recycled contexts in 13 cases and ran out in 5, with no
  new sanitizer reports. The output differs from the goldens, as expected.
