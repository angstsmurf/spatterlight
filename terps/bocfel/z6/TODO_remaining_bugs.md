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

- [ ] **`arthur.cpp:587` — divide-by-zero on the old-release banner path.**
  For `header.release <= 41`, `width_in_chars = (V6_TEXT_BUFFER_WINDOW.x_size -
  2*ggridmarginx) / gcellw` can be 0 when the text window is narrower than one
  cell; `user_word(addr) / width_in_chars` and `% width_in_chars` then SIGFPE.
  Guard `width_in_chars` (clamp to >= 1) before the division.

- [ ] **`writetiff.cpp:51` — divide-by-zero + height truncation.**
  `height = pixarraysize / (width * 4)` divides by zero when `image->width == 0`,
  and truncates `height` to `uint16`, so a > 65535-row image writes a malformed
  TIFF. Reject `width == 0`; widen the height type. (writetiff is a debug/dump
  helper, so low priority.)

## Out-of-bounds reads

- [ ] **`extract_image_data.cpp:189` — directory parse walks past `end`.**
  The bounds check reserves `image_count * directory_entry_size` bytes, but the
  non-compact per-entry parser reads a fixed 12 bytes regardless of
  `directory_entry_size`. A crafted graphics file with a small/odd entry size
  (0..11) and a large `image_count` passes the reservation check yet reads
  ~11 * image_count bytes past the `data` allocation → heap OOB read. Validate
  `directory_entry_size` against the bytes the loop actually consumes.

- [ ] **`journey.cpp:542` — `print_tag_route_to_str` NUL one past the guard.**
  The `" route"` loop guards `name_length + i < STRING_BUFFER_SIZE` for `i<=5`,
  but then writes `str[name_length + 6] = 0` at index `name_length+6` (one past
  the last guarded slot). The initial `for (i<name_length) str[i]=…` copy is also
  unbounded against the buffer. Bound both, mirroring the `print_long_zstr_to_cstr`
  fix.

- [ ] **`draw_image.cpp:190` — `extract_palette` reads a fixed 48 bytes.**
  `memcpy(global_palette, image->palette, kPaletteSize*kBytesPerColor)` copies 48
  bytes unconditionally, but for Blorb PNGs `image->palette` comes from
  `extract_palette_from_png_data()` and may be shorter (e.g. a 4-entry PLTE) →
  OOB read. Copy only `min(48, actual_palette_bytes)`; track the palette length.

- [ ] **`entrypoints.cpp` Shogun chain — remaining unchecked `memory[start±N]`.**
  The severe writes and the three flagged reads are now guarded, but the
  `find_shogun_globals` chain (≈ lines 2519–2536) still does `word(start+2)`,
  `unpack_routine(word(start+2))`, `memory[start+4]`, `memory[start-8]` with a
  `start` that a mid-chain miss can leave at -1 (small-negative OOB read; the
  centralized `find_pattern_in_mem` guard prevents the catastrophic huge-address
  case). Cleanest fix: bail out of the block on the first `start == -1`.

## Integer overflow in image decoders (signed `int` size math)

- [ ] **`draw_png.cpp:360` — `pixmapsize = width*height*4` in signed int.**
  Overflows to negative for large dimensions; `calloc(1, (size_t)negative)`
  returns nullptr (never null-checked) and the later `memcpy` uses the wrapped
  size. Compute in `size_t`, check for overflow, check the `calloc` result.

- [ ] **`draw_png.cpp:291 / 326` — canvas `required_size` / index math in signed int.**
  `draw_y*canvas_width + last_pixel_x` and `required_size` can wrap negative,
  defeating the `last_write - canvas + 4 > *canvas_size` bounds check → OOB
  write. Use `size_t` / `ptrdiff_t` and validate IHDR dimensions up front.

- [ ] **`draw_image.cpp:291 / 335` — `required_size = w*(dest_y+h)*4` in signed int.**
  Overflow makes `*dst_buffer_size < required_size` false, so the realloc-grow is
  skipped and the row loop writes past the allocation. Same fix: `size_t` math +
  dimension validation.

- [ ] **`draw_apple_2.cpp:148` — `pixel_count` int vs. `size_t` buffer size.**
  `deindex` gets `width*height` as an int while `decompress_apple2` sized its
  buffer with a `size_t` product; on overflow the two disagree and `deindex`
  reads past the decompressed buffer. (Bounded in practice by small Apple II
  images.) Make both use the same `size_t` product.

## Uninitialized-memory reads (truncated/corrupt streams)

- [ ] **`decompress_vga.cpp:108` — output buffer `malloc`'d, not zeroed.**
  A stream that emits the end code before producing `width*height` pixels leaves
  the tail uninitialized; the RGBA expansion then reads all `pixel_count` bytes
  as garbage indices (no crash — the `color_index >= kPaletteSize` guard holds —
  but wrong pixels). `calloc`, or track/clamp the actual decoded length.

- [ ] **`draw_png.cpp:93` — `decompress_idat` ignores zlib return codes.**
  `new char[max_output_size]` is uninitialized and `inflateInit2`/`inflate`
  results are unchecked, so a truncated IDAT leaves trailing bytes indeterminate
  while the render loop reads the full `decompressed_size`. Check the return
  codes and only render the bytes actually produced.

## Logic / correctness (verify before changing)

- [ ] **`entrypoints.cpp:2256` — dead CHANGE-NAME patch.**
  `find_pattern_in_mem({4-byte pattern}, …, length_to_search = 1)` can never
  match (window < pattern size), so the "Hack for later versions"
  `store_byte(offset, 0xb0)` never fires. Was the window meant to be larger, or
  is the patch obsolete? Confirm against a real later Journey release.

- [ ] **`entrypoints.cpp:2351` — possible fg/bg swap in the Shogun V-COLOR fallback.**
  The `0x0d`-form fallback maps the store idiom to `{ &bg, &fg }`, the opposite
  of the primary `0x2d`-form path (`{ &fg, &bg }`). A mirror pair exists for Zork
  Zero (≈ 2735/2737). If a revision hits the fallback, colours come out swapped.
  Verify the store order against a real binary before "fixing" — it may be
  correct for that opcode form.

- [ ] **`v6_shared.cpp:712` — unsigned underflow in `select_hint_by_mouse`.**
  `int left = (upperwidth - 15) / 2` with `upperwidth` a `glui32`; a status window
  narrower than 15 columns wraps to ~2e9, the `if (left < 0)` fallback doesn't
  fire, and click hit-testing splits at a bogus column. Compute in signed, or
  check `upperwidth < 15` first. (Degenerate window size — low severity.)

- [ ] **`v6_shared.cpp` `display_soft` — `win_menuitem` length vs. buffer fill mismatch.**
  The VoiceOver call passes `user_byte(fdef+1) + 5` as the length, a different
  source than the `len` bytes actually written into `str[60]`. The write side is
  now bounded, but if that byte is large the front-end reads uninitialized (or,
  if > 60, past-buffer) bytes. Pass the real written length, or clamp it to `len`.

## Altitude — the shared root cause

Several of the above (and the already-fixed shogun/v6_shared/journey overflows)
are the same defect: **a fixed stack/heap buffer filled from a game-supplied
length or dimension with no cap.** Rather than continue patching each site,
consider a small bounded-copy helper (or make `print_*_to_cstr` the single
choke point every caller uses) and route the raw `for (i<length) buf[i]=…` menu
loops through it. Likewise, the image decoders would benefit from one validated
`image_alloc(width, height)` that does the `size_t` overflow check and null-check
once, instead of repeating `w*h*4` signed math in each decoder.
