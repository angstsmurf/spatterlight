# Archetype interpreter — remaining known bugs

Findings from the 2026-07-11 code review that were **not** fixed. The five
confirmed runtime bugs (article-stripping `del`, format-string on literals, heap
sorter, boolean `FALSE`, `dispose_list` header leak) are already fixed and
covered by `make -f Makefile.headless regress-all` (incl. `regress-heap`,
`regress-article`). Everything below is left open on purpose — either a design
call or dead code in this playback-only build.

## Runtime — low severity / needs a design decision

- [ ] **`save`/`load` heuristic can eat a player turn** — `archetype.cpp:476`.
      `readLine()` skips the input read when the *last output fragment* contains
      the substring `save` or `load` (ScummVM workaround for the games' own
      filename prompts). False-positives on `reload`, `unloaded`, a status line
      like `Load: heavy`, etc. Narrowed by the fact that `_lastOutputText` holds
      only the final `write()` (usually the prompt), but still fragile.
      *Fix:* word-boundary match, or key off the known filename-prompt strings
      instead of a bare `contains()`. Needs care not to break the real
      save/load prompt interception — verify against a game that actually saves.
      **Left open**: a naive word-boundary tweak would break real prompts whose
      only match is a longer word ("Restore which *saved* game?"), so this still
      needs the actual shipped prompt strings to key off — not a blind change.

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
