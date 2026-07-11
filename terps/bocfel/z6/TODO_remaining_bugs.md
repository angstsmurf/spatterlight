# z6 — remaining bugs to fix

Follow-ups from the 2026-07-11 code review of `terps/bocfel/z6`. The confirmed
top-10 (colour-reset regression, the buffer-overflow / bounds-check-after-write
cluster, the wrong-variable and self-assignment bugs, the unchecked
`find_pattern_in_mem` writes) are already fixed. The items below were flagged
but left out of that pass.

Most of these need adversarial / malformed input to trigger. Infocom's own art
and story files don't hit them, so severity is "robustness against corrupt or
crafted data" unless noted. Several share one root cause — decoders trusting a
game-supplied length/dimension without bounds-checking — so a shared helper is
often the right fix (see "Altitude" at the bottom).

## Crashes on edge-case input

- [x] **`arthur.cpp:587` — divide-by-zero on the old-release banner path.**
  Fixed: `width_in_chars` is clamped to `>= 1` before the `/` and `%`.

- [x] **`writetiff.cpp:56` — divide-by-zero + height truncation.**
  Fixed: reject `width <= 0` (avoids the divide-by-zero) and reject images taller
  than 65535 rows (the TIFF Height tag is a 16-bit SHORT, so a taller image can't
  be represented and the old code emitted a truncated file). Not "widening the
  height type" because the on-disk field is fixed at 16 bits.

## Out-of-bounds reads

- [x] **`extract_image_data.cpp:189` — directory parse walks past `end`.**
  Fixed: `directory_entry_size` is now validated to be one of the three known
  sizes (8/14/16) before the reservation check. Any other value made the
  reservation (which uses `directory_entry_size`) disagree with the 8/12/14/16
  bytes the loop actually consumes; an odd/small size could pass the check yet
  read past `end`.

- [x] **`journey.cpp:542` — `print_tag_route_to_str` NUL one past the guard.**
  Fixed: `name_length` is clamped to `STRING_BUFFER_SIZE - 7` up front (so the
  last byte written, `str[name_length + 6]`, stays in bounds) and both the name
  copy and the `" route"` copy are now bounded. `STRING_BUFFER_SIZE` is only 15.

- [x] **`draw_image.cpp:190` — `extract_palette` reads a fixed 48 bytes.**
  Investigated — **not a bug.** Both producers of `image->palette` allocate
  >= 48 bytes: `extract_palette_from_png_data()` always `calloc`s the full
  `kMaxPaletteEntries*kBytesPerColor` (48, zero-padded) regardless of PLTE entry
  count, and the legacy per-image path `MemAlloc`s `kColorPaletteSize` (512). The
  premise "a 4-entry PLTE gives a shorter palette" is false. Documented the
  invariant in a comment so a future palette source keeps it.

- [x] **`entrypoints.cpp` Shogun chain — remaining unchecked `memory[start±N]`.**
  Fixed: the two dereference groups in `find_shogun_globals`
  (`sr.TELL_THE = unpack_routine(word(start+2))` and the TELL_DIRECTION /
  SHIP_COURSE / SHIP_DIRECTION reads) are now guarded by `if (start != -1)`. The
  dangerous one was `memory[start - 8]` at `start == -1` → `memory[-9]`. (The
  `find_*` helpers take `uint32_t startpos`, so a forwarded -1 wraps huge and
  safely returns -1 again; only the raw dereferences needed guarding.)

## Integer overflow in image decoders (signed `int` size math)

- [x] **`draw_png.cpp:360` — `pixmapsize = width*height*4` in signed int.**
  Fixed: computed in `size_t`, guarded `width/height <= 0`, and the `calloc`
  result is null-checked. `read_png` also now rejects dimensions over 8192, well
  below any overflow point.

- [x] **`draw_png.cpp:291 / 326` — canvas `required_size` / index math in signed int.**
  Fixed: `required_size` is `size_t`, the per-byte bounds check uses a
  `ptrdiff_t` offset with a `< 0` guard, and `read_png` validates IHDR
  dimensions (0 < w,h <= 8192) up front. `draw_indexed_png`'s `canvas_size`
  parameter is now `size_t *`.

- [x] **`draw_image.cpp:291 / 335` — `required_size = w*(dest_y+h)*4` in signed int.**
  Fixed in both `draw_bitmap_on_bitmap` and `draw_rectangle_on_bitmap`:
  `required_size` computed in `size_t`, bail if it exceeds `INT_MAX` (the buffer
  size fields are `int`), and an early `dest_y + h <= 0` return handles the
  fully-above-canvas case so the `size_t` cast never sees a negative product.

- [x] **`draw_apple_2.cpp:148` — `pixel_count` int vs. `size_t` buffer size.**
  Fixed: the `draw_apple2` call now passes `(size_t)width * (size_t)height`, the
  same product `decompress_apple2` sized its buffer with.

## Uninitialized-memory reads (truncated/corrupt streams)

- [x] **`decompress_vga.cpp:108` — output buffer `malloc`'d, not zeroed.**
  Fixed: `calloc` so an early end code leaves the tail as palette index 0
  (transparent) rather than garbage.

- [x] **`draw_png.cpp:93` — `decompress_idat` ignores zlib return codes.**
  Fixed: the buffer is value-initialized (`new char[n]()`), `inflateInit2`'s
  return is checked, and `decompress_idat` now reports `total_out` via an
  out-param; the render loop iterates only over the bytes zlib actually produced.

## Logic / correctness (verify before changing)

- [x] **`entrypoints.cpp:2259` — dead CHANGE-NAME patch DELETED.**
  Analysis confirmed it was an author-intended "later versions" patch that
  `length_to_search = 1` had left permanently dead. Briefly enabling it (window
  `1` → `5`) proved it fires on exactly r30+, but a Glk-build test showed the
  patch *breaks* the VoiceOver name-change popup — so it was never doing anything
  useful and its intended effect is unwanted. Removed the whole
  `find_pattern_in_mem` + `store_byte(offset, 0xb0)` block.

- [x] **`entrypoints.cpp:2359` — Shogun V-COLOR fallback fg/bg order is CORRECT.**
  Verified against all Shogun releases in `~/Downloads/Shoguns`: only the early
  ones (r278, r283) reach the `0x0d` fallback (the primary `0x2d` search misses);
  r288+ hit the primary path. With the current `{ &bg, &fg }` order r283 yields
  `fg = 0x1c, bg = 0xd0`, consistent with the adjacent later release r288's
  primary-path `fg = 0x1c, bg = 0xcd` — a swap would make r283 report
  `fg = 0xd0`, contradicting r288. The apparent inconsistency with the `{ &fg,
  &bg }` at ≈2353 is a non-issue: that site is the *main routine* colour-reset,
  and its captured globals are inconsequential — 2355–2357 only patches the two
  colour literals (2, 9) to 1, and V-COLOR detection overwrites fg/bg afterward.
  No change; added a comment recording the verification.

- [x] **`v6_shared.cpp:712` — unsigned underflow in `select_hint_by_mouse`.**
  Fixed: `left` is computed as `((int)upperwidth - 15) / 2` so a status window
  narrower than 15 columns yields a negative `left` and trips the fallback.

- [x] **`v6_shared.cpp` `display_soft` — `win_menuitem` length vs. buffer fill mismatch.**
  Fixed: the VoiceOver `win_menuitem` call now passes `len` (the bytes actually
  written into `str[60]`) instead of `user_byte(fdef+1) + 5` (a screen column,
  0..260) which would have sent uninitialized/past-buffer bytes.

## Status

All items resolved. Memory-safety and crash items are fixed; the dead Journey
CHANGE-NAME patch was deleted (it broke the VoiceOver name-change popup when
enabled); the Shogun V-COLOR fallback was verified correct against real
early-release binaries (no change needed).

## Altitude — the shared root cause

Several of the above (and the already-fixed shogun/v6_shared/journey overflows)
are the same defect: **a fixed stack/heap buffer filled from a game-supplied
length or dimension with no cap.** Rather than continue patching each site,
consider a small bounded-copy helper (or make `print_*_to_cstr` the single
choke point every caller uses) and route the raw `for (i<length) buf[i]=…` menu
loops through it. Likewise, the image decoders would benefit from one validated
`image_alloc(width, height)` that does the `size_t` overflow check and null-check
once, instead of repeating `w*h*4` signed math in each decoder.
