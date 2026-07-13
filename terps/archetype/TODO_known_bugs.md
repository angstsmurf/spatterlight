# Archetype interpreter — remaining known bugs

Findings from the 2026-07-11 code review. The five confirmed runtime bugs
(article-stripping `del`, format-string on literals, heap sorter, boolean
`FALSE`, `dispose_list` header leak) were fixed first and are covered by
`make -f Makefile.headless regress-all` (incl. `regress-heap`,
`regress-article`). The remaining items below were fixed 2026-07-13 —
**nothing is left open**. This file is kept as a record of what was checked.

## Runtime — low severity / needs a design decision

- [x] **`save`/`load` heuristic can eat a player turn** — `archetype.cpp`
      `readLine()`. FIXED 2026-07-11. The stale scripted filename read is now
      skipped by keying off the interpreter's own state instead of sniffing the
      prompt text: `'SAVE STATE'/'LOAD STATE' -> system` leaves `sys_state` at
      `SAVE_STATE`/`LOAD_STATE` for exactly the one `read` that follows, so
      `system_awaiting_filename()` (new accessor in `sys_object.cpp`, since the
      enum/`sys_state` are file-local) reports precisely that window. This drops
      the `_lastOutputText.contains("save"/"load")` test entirely, so narrative
      prose like Gorreven's win line "I have *saved* the free world" (which
      contains the substring "save") — or `reload`/`unloaded`/`Load: heavy` — can
      no longer swallow a turn, and a filename prompt that omits the words (e.g.
      ANIMAL's load "Name of file?") is now handled too. Verified end-to-end
      against `BARE.ACX` (`save`/`load` both round-trip through the Glk file
      dialog) and `regress-all` stays byte-exact.

- [x] **Transcript stream/fileref leaked at quit** — `deinitialize()`
      (`archetype.cpp`). FIXED 2026-07-11: `deinitialize()` now detaches the echo
      stream, `glk_stream_close`s `_transcriptStream`, and
      `glk_fileref_destroy`s `_transcriptRef` before `glk_window_close`. Done
      inline rather than via `toggleTranscript(false)` to avoid a spurious
      "Transcript stopped." write into a closing window. `regress-meta` still
      passes (transcript on/off path).

- [x] **`LEFTFROM` / `RIGHTFROM` with out-of-range index returns the whole
      string instead of empty** — `archetype.cpp` `OP_LEFTFROM`/`OP_RIGHTFROM`.
      FIXED 2026-07-11: the signed operand is clamped to `[0, len]` (as
      `count`, with RIGHTFROM's `len - n + 1` computed first) before the
      `size_t` cast, so a negative or oversized index now yields `""` instead of
      wrapping to a huge count that `MIN(count,len)` clamped back to `len`.

## Dead code — compiler front-end (ACH→ACX), not reached when playing .ACX

The port only interprets pre-compiled bytecode (`runGame` → `interpret()` →
`load_game`). The tokenizer / syntax / error-reporting units below have no
runtime caller, so these were latent. All four were fixed 2026-07-13, each
verified against the original Pascal in the Archetype 1.01 distribution
(`~/Downloads/arch101/*.PAS`); `regress-all` stays byte-exact.

- [x] **`binary_search` bounds wrong for 1-based tables** — `token.cpp`.
      FIXED 2026-07-13: `left = 1, right = elements`, matching `TOKEN.PAS`
      (`left := 1; right := elements`). The lookup tables (`keywords.cpp`)
      keep a `nullptr` sentinel at `[0]`, so the old `left = 0` could compare
      against `nullptr` (e.g. operator `&` descending to index 0) and could
      never find the last real entry (`writes`, `~=`).

- [x] **`verify_expr` OP_DOT fallthrough** — `semantic.cpp`. FIXED 2026-07-13:
      the OP_DOT case now `break`s instead of falling into the assignment-lhs
      check (Pascal `case` branches don't fall through — confirmed in
      `SEMANTIC.PAS`, where OP_DOT and the assignment operators are separate
      branches). The fallthrough made every dot expression also run the
      assignment check, flagging e.g. `obj.attr` ("Left side of assignment is
      not an attribute") because the left of a dot is normally an object, not
      an attribute.

- [x] **Format-string on source-line text** — `misc.cpp` `sourcePos()` and
      `error.cpp` `error_message()`. FIXED 2026-07-13: the source line, caret
      line and error message are now routed through `"%s"` instead of being
      passed as the printf format (`writeln_internal` always runs
      `String::vformat` on its first argument, and the headless build compiles
      with `-Wno-format`, so a `%` in a source line was silent UB). Same class
      as the fixed `archetype.cpp:1096` bug.

- [x] **`add_ident` hashes the second char, not the first** — `id_table.cpp`.
      FIXED 2026-07-13: `id_str[1]` → `id_str[0]`. The Pascal `id_str[1]` is
      1-based (first char, per `ID_TABLE.PAS`). Distribution-only (in-bucket
      lookup compares full strings), not corrupting.

## Checked and cleared (not bugs — recorded so they aren't re-investigated)

- **`verify_expr` alleged precedence mistranslation** — `semantic.cpp`
  (the `!(A && B)` guard in the assignment-lhs check). The 2026-07-11 review
  claimed the Pascal meant `(!A) && B`; the actual `SEMANTIC.PAS` is fully
  parenthesized — `not ((left^.kind = OPER) and (left^.op_name = OP_DOT))` —
  so the C++ is a correct translation. No change.

`OP_OR` missing `result._kind = RESERVED` (entry `cleanup(result)` pre-sets it);
`crypt.cpp` SIMPLE/PURPLE/UNPURPLE; save/load DUMP↔LOAD symmetry and int widths;
`getRandomNumber` range; `readLine` buffer bounds; the undo/restart snapshot
state machine.
