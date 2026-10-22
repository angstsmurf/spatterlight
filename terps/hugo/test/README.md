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
`0 changed`. If line numbers move in `hecugel.c`, the `.err` files of the
cases that crash change with them: check `CHANGED ... .err` lines before
trusting a count above 0.

A build with ASan and UBSan (`-enableAddressSanitizer YES
-enableUndefinedBehaviorSanitizer YES`) is the most useful one to trace. It
reports these engine bugs on every run. They are not in heglk:

- AWARD.HEX: heap overflow in `PokeWord`, called from `MoveObj`
  (`source/heobject.c:546`). Every award case aborts.
- EAST.HEX: heap overflow in `RunRoutine` (`source/herun.c:1555`). Every
  east case aborts.
- `source/herun.c:2396`: signed overflow (`16777215 * 256`). Seen in the
  distress and trading walks.

horror2 stops early, probably because it needs data saved by part I.

## What the cases cover

Each game runs as a short walk, a walk with window resizes (`arrange:`
entries), fixed window sizes, and a run with graphics off. Every picture and
sound is traced by the digest of its bytes. The tracer answers `SIZEIMAGE`
with the real JPEG/PNG size of the picture last loaded, so the code that
fits and places pictures is exercised.

Not covered:

- Marjorie and Crimson, which have hacks in `hecugel.c` but are not
  in `GAMES`.
- An arrange event that does not change the size. Spatterlight sends one
  after theme and margin changes.

## Decisions from the heglk refactor (2026-10)

These came out of the read-through in the former `heglk/TODO.md`, and are
recorded here so they are not reopened by accident.

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
- The Future Boy picture size ranges in `heglk_detect_fb_graphics` stay as
  commented literals. Each range is used once, and the names would only
  repeat the comments.
- The three lists of tracked window indices (the recycler's exclusions in
  `heglk_new_wincontext`, and the resets in `hugo_unmapcleared` and
  `hugo_clearfullscreen`) differ on purpose. They are not one array.
