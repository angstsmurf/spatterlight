# Comprehend engine — code-review bug list (2026-07-11)

Findings from a whole-engine review. Ordered by severity. Check off as fixed.

## Memory safety

- [x] **1. `graphics_magician_dhgr.cpp:480` — DHGR flood-fill OOB write.**
  `fb_write()` writes `page[g_row_addr + (col>>1)]` with no `A2_PAGE_SIZE`
  bound, unlike every other `dhgr_put` caller (212, 300, 408, 670). An op15/2
  RESET stores unclamped bottom/right operands into `g_fillBottom/g_fillRight`;
  `flood_fill` copies them to the clip bounds. On an all-`0xff` (fillable) page
  the span reaches rows ~192..255, so `CALC(row)+(col>>1)` exceeds `0x2000`
  and the write lands past `s_main/s_aux[8192]`.
  *Fix:* bound-check the byte offset in `fb_write()` (mirror the guard the
  other callers use), and/or clamp `g_fillBottom/g_fillRight` on RESET.

- [x] **2. `graphics_magician_pcjr.cpp:216` — `s_fillSubindex` OOB read.**
  Array is `[256]` but indexed `s_fillSubindex[sel*2 + (y&1)]` with `sel` a
  raw op6 byte (0..255) → index up to 511. The CGA path sizes the identical
  table at 512.
  *Fix:* size `s_fillSubindex` at 512 to match CGA.

- [x] **3. `draw_surface.cpp:228` — `getPenColor` OOB read.**
  Returns `PEN_COLORS[param]` where `PEN_COLORS` has 8 entries and `param`
  is the opcode low nibble (0..15, from `pics.cpp` `opcode & 0xf`).
  *Fix:* mask/clamp `param` to the valid range.

## Correctness / crashes

- [x] **4. `game_opcodes.cpp:111` — `OPCODE_HAVE_CURRENT_OBJECT` null deref.**
  Derefs `get_item_by_noun(noun)` result unguarded; sibling
  `OPCODE_CURRENT_IS_OBJECT` (line ~88) null-checks. Crash when noun==0 or
  names a non-item.
  *Fix:* null-check like the sibling (`item && item->_room == ROOM_INVENTORY`).

- [x] **5. `game_opcodes.cpp:533` — V1 `OPCODE_INVENTORY_FULL` null deref.**
  Derefs a possibly-null item under the default weight limit; the V2 twin
  (~line 858) guards this exact case with `item ? ... : 0`.
  *Fix:* mirror the V2 guard.

- [x] **6. `draw_surface.cpp:435` — recursive flood fill stack overflow.**
  `floodFillRow` recurses with color-equality as boundary and no visited set;
  a white fill color recurses forever, and any large white region recurses
  thousands deep.
  *Fix:* skip/guard the fill-color == boundary-color case (already-target),
  matching upstream behaviour.

- [x] **7. `game_oo.cpp:422` — `checkShipFuel()` fuel gate always passes.**
  Reads `funcState._testResult` but calls `execute_opcode(&test, nullptr,
  nullptr)`, so the `VAR_GT2` result is discarded into a dummy state;
  `funcState._testResult` stays at `clear()`'s default `true`. OO-Topos
  departure is permitted regardless of actual fuel.
  *Fix:* pass `&funcState` to `execute_opcode`.

- [x] **8. `game.cpp:1450` — `weighInventory()` off-by-one.**
  Loop `for (idx = _itemCount-1; idx > 0; --idx)` never inspects `_items[0]`,
  omitting item index 0's weight from the total, undercounting the carry
  limit.
  *Fix:* change bound to `idx >= 0`.
