# TODO — remaining Adrift 4 (SCARE) code-review items

Findings from a correctness/memory-safety review of the SCARE (`sc*.cpp` /
`sx*.cpp`) engine and the v4 portions of `os_glk.cpp`. The three High items and
one Medium leak were already fixed in commit `e2512135`:

  * `scresour.cpp` / `sctafpar.cpp` — `strlen >= 2` guard on the `"##"` suffix test
  * `sclibrar.cpp` — `obj_stateful_index` -> `obj_stateful_object` in `lib_use_room_alt`
  * `sctaffil.cpp` — `inflateEnd` before the inflate-error `return FALSE`

Most are hostile/malformed-file hardening rather than bugs seen in normal play;
a couple need testing against real games before changing. Severity is the
reviewer's estimate.

All four Medium items and every actionable Low item are now fixed (see the
checked boxes below), including the size-math hardening in `sctafpar.cpp`. The
only remaining open box is the minor/cosmetic group, which has been reviewed and
deliberately left as safe-today (details there).

## Medium

- [x] **`scserial.cpp` (~986-1015, 1071, 831): restore trusts per-record indices.**
  Object/room/NPC *counts* are validated up front, but the values inside records
  — object `parent`/`position`, player `playerroom`/`parent`, NPC `location`, and
  the loop `count` at 831 — are stored verbatim from an untrusted save. The `gs_*`
  accessors only `assert` their argument (no-op under `NDEBUG`), so a corrupt save
  can plant out-of-range indices that later read/write arrays out of bounds.
  Fixed: added `ser_reject_if` plus `ser_object_position_valid` /
  `ser_object_parent_valid` helpers; the restore now rejects (via the error
  longjmp) any playerroom / playerparent / dynamic-object position+parent / NPC
  location / event starter-task / static room-list count that is out of range for
  the current game. Static objects are skipped (their position comes from the
  bundle and their parent is never used as an index).

- [x] **`os_glk.cpp` (~1940, `gsc_resource_id`): 64-bit offset truncated to glui32.**
  `(glui32)(((scr_uint)offset << 12) ^ length)` is computed in 64 bits then
  truncated. Resources at TAF `offset >= 2^20` lose their high bits, so distinct
  chunks can collide on one id; since the id is cached, a wrong image/sound is
  drawn/played in large games. Fixed: fold the full 64-bit offset+length into 32
  bits with a multiply-and-xor-fold hash instead of shift-then-truncate. NOTE:
  still worth spot-testing against a large graphics/sound game.

- [x] **`os_glk.cpp` (~679, `gsc_put_char_locale`): line-start flag clobbered.**
  `gsc_main_at_line_start` was updated on every char, including status-window
  rendering, so `SCR_TAG_CLS` / `os_show_graphic` (graphics builds) misjudge
  whether to emit a leading newline before an inline image. Fixed: update the
  flag only when the current output stream is the main window's stream (same
  test the code already uses elsewhere).

- [x] **`sclibrar.cpp` (~7379, `lib_compare_subject`): advances the wrong cursor.**
  The internal-whitespace collapse did `subject++` while the rest of the function
  indexes `subject[word_posn]` and advances `word_posn`. Fixed: `word_posn++;`.

## Low (hardening)

- [x] **`screstrs.cpp` (~919): restriction AND/OR stack underflow guarded only by
  `assert`.** Fixed: promoted to a real `scr_fatal` (as `scexpr.cpp:561` does)
  so it survives `NDEBUG`.

- [x] **`scevents.cpp` (342 `TaskAffected`; 490/521/552 `TaskNum`/`PauseTask`/
  `ResumeTask`): task indices lower-bounded only.** Fixed: added explicit
  `< gs_task_count(game)` upper-bound checks at all four sites.

- [x] **`sctafpar.cpp`: unchecked size math on attacker-controlled counts.**
  `count * sizeof(...)`, `2 * restriction_count`, and vector sizings use parsed
  counts with no overflow/sanity cap (DoS, or under-allocation on 32-bit).
  Fixed with checked-multiply guards that only fatal on genuine overflow (no
  arbitrary cap, so large-but-valid games are unaffected): added
  `parse_checked_multiply` (rejects negative counts and any `count * element`
  that would overflow `size_t`) applied to both `2 * restriction_count`
  restrmask allocations and the resources-table `scr_realloc`, and
  `parse_checked_count` (rejects negative counts) applied to the three
  `std::vector` sizings (`object_type`, `movetimes`, `alr_lengths`). The
  `movetimes` case also fixed a latent bug: a negative `waittimes` sized the
  vector to zero yet was still used as the index `movetimes[waittimes]`, an
  out-of-bounds write now caught by the guard.

- [x] **`sctafpar.cpp` `parse_read_multiline`: leak on `longjmp`.** Fixed: the
  growing buffer is now held in an `scr_owned_string` (freed during the
  `scr_fatal_error` unwind), releasing/adopting around each `scr_realloc`.

- [x] **`scrunner.cpp` (~1402): `escaped` leaks if `var_set_ref_text` throws.**
  Fixed: wrapped in `scr_owned_string` like the sibling code at ~1370.

- [x] **`sctasks.cpp` (~389): dead `if (var3 == 0)` branch** inside the `else` of
  `if (var3 == 0)`. Fixed: removed the unreachable branch, keeping the reachable
  `gs_object_to_room` call (with a clarifying comment).

- [x] **`scbattle.cpp` (~391): asymmetric stamina guard.** Fixed: guarded the
  player's stamina roll `(hi > 0) ? scr_randomint(lo,hi) : 0` to match the NPC's.

- [x] Minor / cosmetic — `sprintf` -> `snprintf` at `scvars.cpp` (both referenced-
  number sites) and `sxutils.cpp:169`.

- [ ] Minor / cosmetic — STILL OPEN, left as safe-today: `scutils.cpp:167`
  `scr_free` self-guard is not actually dead (it is a self-referential canary that
  detects writes into a zero-byte allocation; leave it); `scexpr.cpp`
  `abs(LONG_MIN)` UB (unreachable from game data); `scrunner.cpp` `strncpy` sites
  that rely on sources already being `LINE_BUFFER_SIZE`-bounded.

## Faithful-port quirks to leave alone (verify, don't "fix")

- `screstrs.cpp` (~448-457): NPC standing/sitting/lying cases test the NPC's
  position but the *player's* parent (`gs_playerparent`). Reproduces the original
  SCARE C behavior; don't "correct" into a divergence without evidence.
- `scserial.cpp` (~762-776): legacy v2 battle-block reader uses a different field
  order than any current writer. Only runs against saves from a removed writer;
  check against a real v2-legacy save if cross-build compatibility matters.
