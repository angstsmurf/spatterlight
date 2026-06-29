# TODO — C++ modernization of the ADRIFT 4 / 3.9 / 3.8 path (SCARE)

The legacy SCARE engine (`sc*.cpp` — `sctaffil` → `sctafpar` → `scgamest` /
`sctasks` / `screstrs` / `scrunner` / `scprintf` / `scexpr` …) was renamed from
`.c` to `.cpp` and now compiles with the C++ compiler, **but the conversion is
nominal**: there are **zero STL headers in any `sc*.cpp`**, and the only visible
sign of C++ is mechanical `(decltype(x))` casts sprinkled on `void*` returns to
satisfy C++'s stricter rules (e.g. `scprintf.cpp:232`, `scutils.cpp:scr_malloc`).
It is "C compiled by a C++ compiler."

This file tracks turning that nominal conversion into real wins. The in-tree
model for "how to do it right" is the **a5 engine** (the ADRIFT 5 path), which
uses `std::string` / `std::vector` / `std::set` / `std::map` freely and contains
the bulk of the world logic in a C-like-but-modern style — follow its idioms.

> ⚠️ **Upstream-divergence caveat.** SCARE is Simon Baldwin's upstream code.
> This fork already diverges heavily (battle system, locale, RNG, dozens of
> bug-fixes), so the gap is partly a sunk cost — but every item below, especially
> P3/P4, widens it and complicates any future upstream merge. P1/P2 are the most
> defensible: self-contained, tied to a known crash/perf footgun, and exactly
> what "it's C++ now" makes cheap. Weigh each phase against the merge cost.

---

## 0. Ground rules / invariants

- **Byte-identical output is the bar.** SCARE has golden-transcript and
  ground-truth regressions; no modernization may change a single byte of game
  output. Validate every phase against:
  - `make -f Makefile.headless test` (battle / precedence / badparent `.taf`
    harnesses + the a5 suite),
  - the v4 walkthrough corpus in `adrift-walkthroughs/` (replayed headlessly),
  - the FrankenDrift/`run400`-derived expectations where they exist.
- **ASan/UBSan-clean** is mandatory on every change (the established bar for this
  tree). Build the headless harness with `-fsanitize=address,undefined` and run
  the corpus.
- **Bound every soak run.** Headless SCARE can exhaust RAM on event-heavy games
  (Menagerie/circus) — always wrap corpus runs in `ulimit -t 12` + `head -c 4M`
  (the scratchpad `safeplay.sh` recipe); never run them unbounded in the
  background.
- **Keep the public `extern "C"` surface unchanged** (`scr_*` in `scinterf.cpp`)
  so the os layers (`os_glk.cpp`, `os_ansi.cpp`) and Spatterlight need no changes.
- **One concern per commit**, smallest blast radius first, mirroring the a5
  history.

---

## P1. Rewrite the hot string builders on `std::string` (best value/risk)

**Why.** `scprintf.cpp` builds output **quadratically**. Every append does
`buffer = scr_realloc(buffer, strlen(buffer) + …)` followed by `strncat(...)` —
both re-scan the *entire accumulated buffer* on every fragment, and it reallocs
on every append with no capacity growth (`scprintf.cpp:232, 246, 260, 266, 290,
357, 370`). For a long turn's output this is O(n²) in length and hammers the
allocator — a prime suspect for the event-heavy-game OOM/slowness footgun. The
`(decltype(buffer))` casts are the smoking gun that this file was converted but
never modernized.

**Scope.** Contained to `scprintf.cpp` (~30 alloc sites) and any local string
assembly it calls. Internal only — the file's public results can still be
returned as `char*` (allocated once from the finished `std::string`) so callers
are untouched.

**Approach.**
- Replace the grow-by-realloc `char *buffer` accumulators with `std::string`
  (amortized-O(1) append; reserve where the final size is known).
- Keep the `%var%` / tag-scanning logic identical; only the accumulation
  changes. Hand back `char*` at the boundary via a single `strdup`/copy of
  `.c_str()` if the caller frees, or migrate the caller to `std::string` if it's
  local.
- Audit the sibling builders that share the pattern (the untagged-text pass at
  `scprintf.cpp:624`, and any `scr_realloc(... strlen ...)` accumulators in
  `scrunner.cpp` / `scparser.cpp`).

**Validate.** Golden transcripts byte-identical; re-run the Menagerie/circus
soak under `ulimit`/`head` and confirm the RAM/CPU blow-up shrinks; ASan/UBSan
clean.

- [ ] Rewrite `scprintf.cpp` accumulators on `std::string`.
- [ ] Sweep other `scr_realloc(buf, strlen(buf)+…)` builders in the hot path.
- [ ] Re-measure the event-heavy soak (before/after RSS + wall-clock).

---

## P2. Stop `abort()`-ing the host application on bad/huge games

**Why.** Every error path bottoms out at `scr_fatal()` → **`abort()`**
(`scutils.cpp:91`). That includes **allocation failure** (`scr_malloc` calls
`scr_fatal` when `malloc` returns NULL) and every malformed-game / internal
consistency check (`prop_put` alone has several `scr_fatal`s, e.g.
`scprops.cpp:461, 531, 547`). There is **no recovery boundary** — the
`setjmp(game->quitter)` in `scrunner.cpp:2066` is only for a normal QUIT, not
errors. In Spatterlight a corrupt or pathological game therefore takes down the
**whole application**. (`scexpr.cpp` already uses `setjmp`/`longjmp` for parse
errors — `scexpr.cpp:646, 1164, …` — so the engine already has a non-local error
idiom; this just generalises and tames it.)

**Approach (C++ exceptions, caught at the boundary).**
- Define a small exception type (e.g. `struct scr_error { std::string what; };`)
  and have `scr_fatal` (and ideally the `scexpr` `longjmp`s) `throw` it instead
  of `abort()`. Keep a `scr_fatal` signature so call sites don't change.
- Wrap the public entry points in `scinterf.cpp` (`scr_interpret_game`,
  `scr_game_from_*`, save/load) in `try { … } catch (const scr_error &)` and
  return a clean failure (the existing "invalid game" path at `scinterf.cpp:455`)
  instead of crashing.
- **Exception-safety caveat:** SCARE leaks on its current abort-and-die model
  (it never unwinds). Throwing past code that owns raw `malloc`'d buffers will
  *leak on the error path* — acceptable for a fatal-but-recoverable game error
  (the session is torn down anyway), but do P3 (RAII) before relying on clean
  unwinding for non-fatal cases. Document which catch sites tear the whole game
  down vs. attempt to continue.
- Convert `scexpr.cpp`'s `longjmp(expr_parse_error)` sites to the same exception
  so there is one error mechanism, and the `setjmp` buffer can go.

**Validate.** Add a regression that feeds a deliberately-corrupt `.taf` (extend
`test/make_badparent_taf.py` or a new generator) and asserts `scr_interpret_game`
returns an error rather than aborting; existing golden transcripts unchanged;
ASan/UBSan clean (watch for leaks newly *reachable* on the throw path — expected,
note them, fix in P3).

- [ ] `scr_error` exception type; `scr_fatal` throws.
- [ ] `try`/`catch` boundary in `scinterf.cpp` public entries.
- [ ] Migrate `scexpr.cpp` `longjmp` → the same exception; drop `setjmp.h`.
- [ ] Corrupt-game regression (no abort, clean error return).

---

## P3. RAII to eliminate leak classes (large, incremental)

**Why.** Manual `malloc`/`free` is pervasive — `scgamest.cpp` (31 sites),
`scprintf.cpp` (30), `scrunner.cpp` / `scdebug.cpp` (18 each), `scparser.cpp`
(17), `scprops.cpp` (16), `sctaffil.cpp` / `scexpr.cpp` (15), `sctafpar.cpp`
(14)… `scr_malloc` is a *non-failing* wrapper, so leaks accumulate silently
until `abort()`. RAII removes whole categories of leak/OOM and is the
prerequisite for clean exception unwinding (P2).

**Approach (hottest/most-leak-prone first, file by file).**
- Replace pervasive `char *` ownership with `std::string`; hand-rolled dynamic
  arrays with `std::vector`; owning pointers freed at scope exit with
  `std::unique_ptr` (or `std::vector<std::unique_ptr<…>>`).
- Mirror the a5 engine's containers — don't invent a bespoke abstraction.
- Keep struct layouts that cross the `extern "C"` boundary POD; confine STL to
  internals.
- Candidate order: `scprintf` (after P1 it's half-done) → `scparser` →
  `scrunner`/`sctasks` → `scgamest`/`scprops` (the property tree, biggest and
  most cross-cutting; coordinate with P4).

**Validate.** Per-file: golden transcripts byte-identical; ASan/UBSan **leak**
counts drop monotonically across the corpus; the event-heavy soak's peak RSS
drops.

- [ ] `scprintf.cpp` ownership → `std::string`/RAII (finish P1).
- [ ] `scparser.cpp`.
- [ ] `scrunner.cpp` / `sctasks.cpp`.
- [ ] `scgamest.cpp` / `scprops.cpp` (with P4).
- [ ] Track corpus leak count before/after each file.

---

## P4. The property-tree hot path (highest ceiling, highest risk)

**Why.** `prop_get`/`prop_put` **parse a printf-style format string and walk a
string-keyed tree on every access** (`scprops.cpp:451+`; paths like
`"Objects[%d].Name"` assembled and re-parsed at runtime, vararg key array). This
is the single most-called operation in the engine — every restriction, action,
variable read, and text substitution funnels through it. Runtime format parsing +
tree descent per access is the dominant cost.

**Approach (only after P1–P3; this is the risky one).**
- **Low-risk first step:** cache the parsed format path (the `%s`/`%d` template →
  a pre-split step list) so repeated accesses skip the `sscanf`/format re-parse.
- **Bigger step:** replace string-keyed child lookup with `std::unordered_map`
  per node, or precompute integer field indices, or move toward typed accessors
  like the a5 model (typed structs + key lookup instead of formatted paths).
- This changes the engine's core data structure — budget heavily for byte-exact
  validation and treat it as its own multi-commit project. The a5 engine
  (typed records, no formatted-path access) is the reference design.

**Validate.** Whole walkthrough corpus byte-identical; profile before/after on
the event-heavy games to confirm the win justifies the risk.

- [ ] Profile to confirm `prop_*` is the hot path (don't assume).
- [ ] Cache parsed format paths (no structural change).
- [ ] (Optional) typed/`unordered_map` node lookup.

---

## 5. Testing & tooling (shared across phases)

- **Golden transcripts**: ensure each touched subsystem has a replayable
  before/after transcript (extend the existing `.taf` harnesses where a feature
  is uncovered).
- **ASan/UBSan harness**: a one-liner build of the headless runner with
  sanitizers + a corpus sweep; wire a `make -f Makefile.headless sanitize`
  convenience target if it helps.
- **Memory soak**: a bounded (`ulimit -t` / `head -c`) Menagerie/circus run that
  records peak RSS + wall-clock, so P1/P3 wins are measured, not assumed.
- **Leak ledger**: record the corpus LeakSanitizer count per commit so P3
  progress is visible.

---

## 6. Recommended order & rationale

1. **P1** — contained, high-confidence, directly attacks the OOM/perf footgun.
2. **P2** — robustness; stops a bad game crashing the host. Small surface.
3. **P3** — incremental cleanup; unlocks clean unwinding for P2 and shrinks RSS.
4. **P4** — biggest ceiling, biggest risk; only with profiling justification.

Each phase is independently shippable and independently revertible. Stop after
any phase if the upstream-divergence cost outweighs the benefit.
