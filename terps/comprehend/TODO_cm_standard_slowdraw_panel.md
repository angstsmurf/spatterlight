# TODO: standard-hires CM panel survival during a slow-draw reveal

## STATUS: DONE

Implemented exactly as the fix steps below describe (`graphics_magician.cpp`):
`init_offset_tables()` now builds a parallel `s_colOfOffset` + `col_of_offset()`
accessor; `AppleSlowPolicy::apply` forwards to a forward-declared
`apple_apply_slow()` defined next to `gmOverlayCMPanel`, which returns `false`
(skipping the write *and* the dirty mark) for columns in
`[s_cmPanelCol0,s_cmPanelCol1]` while `s_cmPanelValid`. Added the diagnostic
`gmSlowPagePtr()` accessor. Test `test_panel_survives_reveal()` (TEST 6 in
`test/cmhourglasstest.cpp`) records a full-width line reveal via the public draw
path, overlays the panel, fully drains, and asserts the panel columns 24..39 keep
the panel while cols 0..23 reveal the room. Verified it fails pre-fix (80 panel-
clobber failures) and passes after; the byte-exact regressions are unaffected.

## Problem

The double-hi-res renderer (`graphics_magician_dhgr.cpp`) excludes the Coveted
Mirror panel columns from the progressive slow-draw reveal; the standard-hires
renderer (`graphics_magician.cpp`) does **not**. This is the one remaining
behavioural asymmetry between the two renderers.

Sequence that exposes it (standard hi-res CM, slow-draw on):

1. A room picture is drawn with recording enabled, so every plotted byte —
   including bytes in the panel columns 24..39 — is appended to the slow-draw op
   list (`s_slow.record(...)` inside `write_screen`).
2. `gmOverlayCMPanel()` composites the saved panel onto **both** `s_screenmem`
   (final page) and `s_slowScreen` (the page the host reveals).
3. The host then drains the reveal a chunk at a time (`gmAdvanceSlowDraw`).
   `AppleSlowPolicy::apply` replays each recorded byte onto `s_slowScreen`
   unconditionally — **including the room bytes in columns 24..39**, painting
   room content over the just-overlaid panel until the reveal passes those ops.

Net effect: the panel flickers / shows room content over its columns *during*
the animated reveal in standard hi-res. The **final** page is correct (the
overlay wrote `s_screenmem` and the reveal finishes by definition), so this is a
transient mid-reveal artifact, not a wrong final image.

Reachable because CM can run in standard hi-res (`comprehend.cpp:1137`,
`!_useDhgr && getGameID() == "covetedmirror"`), the CM panel paths are wired for
both renderers (`pics.cpp:688`/`748`), and slow-draw is on in normal play
(it is only suppressed in the Determinism/testing theme).

## Reference implementation (DHGR — already does this)

`graphics_magician_dhgr.cpp`:

- `s_colOfOffset[A2_PAGE_SIZE]` + `init_offset_tables()` build an offset→column
  table (alongside offset→row) once.
- `DhgrSlowPolicy::apply` forwards to `dhgr_apply_slow(op)`.
- `dhgr_apply_slow` (see its comment block): when `s_cmPanelValid`, looks up the
  byte's column and **returns `false` for columns in `[s_cmPanelCol0,
  s_cmPanelCol1]`**, which makes `SlowDrawEngine::applyOne` skip both the write
  and the dirty marking (`slow_draw_page.h` `applyOne`: `if (!_p.apply(op)) return;`).
  Non-panel bytes are written as before.

The standard renderer should mirror this exactly.

## Fix steps (graphics_magician.cpp)

1. **Add an offset→column table.** The renderer currently builds only
   `s_rowOfOffset` (in `row_of_offset`, ~line 164). Add a parallel
   `s_colOfOffset[A2_SCREEN_MEM_SIZE]`, memset to `0xff`, filled in the same
   `row==0..191 / col==0..39` loop:
   `s_colOfOffset[base + col] = (uint8_t)col;`
   Either extend `row_of_offset`'s init block or factor an `init_offset_tables()`
   like DHGR. Add a `col_of_offset(uint16_t)` accessor returning `-1` for gaps.

2. **Forward-declare the apply hook.** `AppleSlowPolicy` (~line 180) is defined
   *before* the panel state (`s_cmPanelValid`, `s_cmPanelCol0/1`, ~line 886), so
   `apply` cannot reference them inline. Mirror DHGR: declare
   `static bool apple_apply_slow(const SlowWriteOp &op);` before the policy, have
   `AppleSlowPolicy::apply` call it, and define `apple_apply_slow` *after* the
   panel state (next to `gmOverlayCMPanel`):

   ```cpp
   // (replaces: bool apply(const SlowWriteOp &op) { s_slowScreen[op.offset] = op.value; return true; })
   static bool apple_apply_slow(const SlowWriteOp &op);          // fwd decl
   struct AppleSlowPolicy {
       bool apply(const SlowWriteOp &op) { return apple_apply_slow(op); }
       ...
   };

   // ...later, after s_cmPanel/s_cmPanelValid/s_cmPanelCol0..1 are defined:
   static bool apple_apply_slow(const SlowWriteOp &op) {
       if (s_cmPanelValid) {
           int c = col_of_offset(op.offset);
           if (c >= s_cmPanelCol0 && c <= s_cmPanelCol1)
               return false;                 // keep the overlaid panel intact
       }
       s_slowScreen[op.offset] = op.value;
       return true;
   }
   ```

3. **Confirm no impact off CM.** The skip is gated on `s_cmPanelValid`, which is
   only true after `gmCaptureCMPanel` (CM only). Every other game keeps the old
   unconditional write, so `test_talisman_pics` / `rgbacrc` are unaffected.

## Test (extend test/cmhourglasstest.cpp, or a new test/cmrevealtest.cpp)

Goal: a recorded reveal must not wipe the panel columns off the slow page.

Blocker: the slow page `s_slowScreen` has no public accessor (only
`gmBlitSlowToSurface` → RGBA). Pick one:

- **(preferred)** add a diagnostic `const uint8_t *gmSlowPagePtr()` next to
  `gmPagePtr()` (mirrors it; one line) and read bytes directly; **or**
- blit the slow page to a surface and assert the panel columns are the panel
  colour, not the background — coarser, and the artifact LUT makes exact
  asserts awkward.

With a slow-page accessor, the test is fixture-free (uses `gmSetPage` /
`gmSetSlowDraw` / `gmAdvanceSlowDraw`):

1. `gmResetScreen(false)` (clears pages + op list).
2. Build a synthetic full-width "room" page (distinct value in every column) and
   `gmSetPage(room)`; `gmCaptureCMPanel(24,39)`. (Or capture a distinct panel
   first, then a distinct room — the point is panel ≠ room in cols 24..39.)
3. `gmSetSlowDraw(true)`, then **record** a full-width write stream. Easiest:
   call the renderer's write path by drawing a tiny image whose ops touch cols
   24..39 — or expose a test-only "record this page" helper. Simplest robust
   option: record by replaying a known page through the public draw path
   (`gmDrawImage` with a stream that fills the row, e.g. op15/1 + op15/3 using a
   T2 fixture) — but that reintroduces a fixture. If avoiding fixtures, add a
   tiny test seam that calls `s_slow.record()` for a row of panel-column offsets.
4. `gmOverlayCMPanel()`.
5. Drain: `while (gmAdvanceSlowDraw(100000)) {}`.
6. Assert: `gmSlowPagePtr()` bytes in cols 24..39 (sample rows 0,1,95,146,191)
   still equal the **panel** values, not the room/recorded values. Cols 0..23
   reflect the revealed room.

Run **before** the fix to see it fail (panel columns clobbered), then after the
fix to see it pass. Wire into the `make test` target + `clean` rule like
`cmhourglasstest`.

## MAME / ground-truth note

A static golden page **cannot** capture this: the final page is identical with
or without the fix (the overlay + completed reveal converge), and in CM the
op15/2 room bounds end at col 23 so the panel region is always panel content at
rest. The defect is purely a *mid-reveal transient*. So this is validated by the
unit test above (panel survives a partially/ fully drained reveal), not by a
golden capture. Optionally eyeball it live: CM in standard hi-res with slow-draw
on, watch the panel during a room reveal before/after.

## Effort / risk

- Small, localised (one table + one forward-declared hook), directly modelled on
  the DHGR code.
- Low risk: behaviour change is gated to CM (`s_cmPanelValid`); all other games
  and the existing byte-exact regressions are untouched.
- The only API surface change is the optional diagnostic `gmSlowPagePtr()`.
