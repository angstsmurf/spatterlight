# Quest 5 native engine — code-review backlog

Findings from the 2026-07-21 multi-agent review of `terps/geas/quest5`.
**Status: ALL 23 backlog findings fixed (2026-07-21, second pass), plus one
bonus divergence found during verification.** Native goldens stayed 74/74
byte-identical after every priority group (`aslx_replay` sweep vs
`quest5-oracle/golden/`), `make check` + `make asan` green, and the Glk
frontend verified by a before/after `ASLXGLK_PROMPT_FIRST=1` smoke sweep over
all 74 goldens (byte-identical stdout).

New regression tests (in `test/aslx_loader_test.cc` / `test/aslx_runtime_test.cc`):
`test_zip_hostile`, `test_save_hostile`, `test_number_parse_oracle`,
`test_parser_depth_caps`, `test_attribute_names_deep`,
`test_firsttime_bake_oneliners`, `test_save_degenerate_state`. Each was
positive-controlled: run against the pre-fix engine they crash (ASan OOB /
allocation-size-too-big / stack-overflow) or FAIL.

## First pass (earlier 2026-07-21) — for context
- Use-after-free in `list add`/`list remove`, `dictionary add`/`remove`, and
  `do <obj>,<script>,<params>` (snapshot before `ev()`).
- Cycle guards in `collect_field_chain` (path-based), `does_inherit`, `Clone`.

## Second pass — what was fixed where

### P1 untrusted-file crashes / OOB (`aslx.cc`, `aslx-state.inc`)
- [x] zip stored-entry OOB read: copy the bounds-checked `comp_size`, not `raw_size`.
- [x] central-directory filename OOB: `pos + 46 + name_len` bounds check; the
  whole CD walk is index-based now (no pointer formed before its range check,
  which also closes the `data + cd_off`/`local_off` UB item).
- [x] save preallocation: counts bounded by `end - p`; element/inherit/field
  vectors grow per parsed item instead of `resize(count)`.
- [x] `rd_value` recursion: depth cap 1000, fail closed.
- [x] `inflate_raw`: `rawlen` capped at 512 MB.
- [x] `rd_num`/`rd_blob`: 18-digit cap (no long/size_t wrap).
- [x] `XML_Parse` int truncation: `len > INT_MAX` rejected in `parse_buffer_into`.
- [x] `<include ref>` traversal: `..` path segments refused.
- [x] `load_file` and `restore_game` are function-try-blocks catching
  `bad_alloc`/`length_error` into clean load/restore errors.

### P2 save-writer silent corruption (`aslx-savenative.inc`)
- [x] one-line `firsttime`/`otherwise`/block bodies: the baker now locates
  bodies the way `parse_one_statement` does (after keyword/parameter) instead
  of at the first `{`; `bake_switch` likewise (`split_block` deleted). Fixes
  both the deleted bodies and the flag-stream misalignment for one-line nests.
- [x] ScriptDict entries route through `bake_firsttime_source` (both the
  field-position and value-position writers).
- [x] parent-cycle elements: seen-set in `emit` + cycle orphans emitted as
  extra roots; `wr_value` (v1) and `write_value` (native) truncate
  self-referential collections to null at depth 100.

### P3 oracle-divergence correctness
- [x] restore onto non-save-family element: parse rejects non-save-family
  `elem_type`; a pre-mutation pass rejects name collisions with statics.
- [x] `GetAttributeNames(obj, true)`: recurses each type's own inherited types
  (verified against QuestViva `Fields.GetAttributeNames`, Stack order), with a
  cycle guard.
- [x] `ToInt`/`ToDouble` on strings are `int.Parse`/`double.Parse` (invariant):
  .NET exception messages raised as script errors; numeric args range-guarded
  before `llround` (no NaN/overflow garbage). `CInt`/`CDbl` (native-only
  extensions) share the guards.
- [x] `IsInt`: Int32 range check (`int.TryParse`).
- [x] `strtod` locale: all number parses (lexer, loader, save reader,
  builtins) go through `c_strtod` (aslx.hh), pinned to the C locale.
- **BONUS (found by the stricter ToDouble):** `GetUIOption` returned `""`
  where the oracle's headless player returns **null**, silently flipping every
  `value = null` check in Core (UIOptionUseGameFont etc.) — masked in the
  goldens only because HTML styles are stripped. Now returns null.

### P4 runtime state-machine
- [x] `park_suspension`: queues new frame groups behind a live park instead of
  overwriting.
- [x] prompt cancel reentrancy: all four statement sites (wait / get input /
  ask / show menu) clear their pending flag BEFORE `end_pending_callback`, so
  a nested prompt in the on-ready flush cannot double-end the same callback
  (count going negative = permanent wedge). Flush timing itself is unchanged
  (it matches the reference TCS synchronous-continuation semantics).
- [x] `Rng::between`: unsigned span arithmetic, full-range span guarded.
- [x] parser recursion: statement parse capped at 200 nested blocks
  (`g_stmt_parse_depth`), expression descent capped at 200 via `DepthGuard`
  on `parse_ternary`/`parse_not`/`parse_unary`; both surface as parse/script
  errors.

### P5 Glk frontend (`aslxglk.cc`, `aslxglk-map.inc`)
- [x] `read_line` buffer is per-frame (every return path completes or cancels
  the request, so it cannot outlive the frame); nested menu/ask no longer
  clobbers the player's half-typed line.
- [x] `tag_attr`: whole-attribute-name match (`data-src` no longer answers a
  `src` query).
- [x] map raster: `gm_fill_rect` clamps, `gm_line` bbox-rejects + Liang-Barsky
  clips far endpoints (sane lines untouched), `gm_fill_circle` rejects/clamps
  with double radius math, `px`/`py` clamp to ±1e9 (no lround wrap), nib width
  capped.
- [x] `g_out_log` capped at 4096 chunks (trim on every append, sections
  clamped); spent `g_links` payloads released in memory and serialized empty
  (blob format unchanged, old autosaves still restore).
- [x] `PromptBreak` saves/restores the prior print handler; menu/question
  captions and options print through `in.print` (prompt-retract path).
- [x] `aslx_is_quest5_file`: bails on `ftell` failure / empty file.
