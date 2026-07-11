# Archetype interpreter — remaining known bugs

Findings from the 2026-07-11 code review that were **not** fixed. The five
confirmed runtime bugs (article-stripping `del`, format-string on literals, heap
sorter, boolean `FALSE`, `dispose_list` header leak) are already fixed and
covered by `make -f Makefile.headless regress-all` (incl. `regress-heap`,
`regress-article`). Everything below is left open on purpose — either a design
call or dead code in this playback-only build.

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
runtime caller, so these are latent — fix only if the compiler path is revived.

- [ ] **`binary_search` bounds wrong for 1-based tables** — `token.cpp:79`
      (`left = 0, right = elements - 1`) against `keywords.cpp:29,67`, whose
      `[0]` is a `nullptr` sentinel. A token that descends to index 0 compares a
      `std::string` against `nullptr` → `strlen(nullptr)` crash (e.g. operator
      `&`); and the last real entry (`writes`, `~=`) is never found.
      *Fix:* `left = 1, right = elements`.

- [ ] **`verify_expr` OP_DOT fallthrough + precedence mistranslation** —
      `semantic.cpp:174-190` (OP_DOT falls into the assignment-check case; there
      is a `// FIXME: is this fallthrough intentional?`) and `:205`
      (`!(A && B)` where the Pascal means `(!A) && B`).

- [ ] **Format-string on source-line text** — `misc.cpp:123,129` and
      `error.cpp:61` pass runtime strings (a source line, caret line, error
      message) as the printf format. Same class as the fixed `archetype.cpp:1096`
      bug. *Fix:* route through `"%s"`.

- [ ] **`add_ident` hashes the second char, not the first** — `id_table.cpp:38`
      uses `id_str[1]` (0-based → 2nd char); the Pascal `id_str[1]` (1-based) and
      the comment mean the first. Distribution-only (in-bucket lookup compares
      full strings), not corrupting. *Fix:* `id_str[0]`.

## Checked and cleared (not bugs — recorded so they aren't re-investigated)

`OP_OR` missing `result._kind = RESERVED` (entry `cleanup(result)` pre-sets it);
`crypt.cpp` SIMPLE/PURPLE/UNPURPLE; save/load DUMP↔LOAD symmetry and int widths;
`getRandomNumber` range; `readLine` buffer bounds; the undo/restart snapshot
state machine.
