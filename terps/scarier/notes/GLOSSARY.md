# Glossary and comment style

The comments in `core/`, `library/`, `runner/` and `glk/` use the names
below in one sense each. `RUNNER_ADDRESSES.md` covers the addresses they
cite.

## Names

**Scarier**
: This interpreter, the code in this tree. "Scarier does X" describes what
  the code does now.

**SCARE**
: Simon Baldwin's C interpreter, which Scarier was forked from. "SCARE does
  X" describes the upstream code and is usually the reason a comment exists:
  Scarier does something else, because a Runner does.

**jAsea**
: Mark J. Tilford's Java interpreter, which SCARE was cloned from. It is
  cited only where SCARE inherited a name or a guess from it.

**ADRIFT**
: Campbell Wild's authoring system. The **Generator** is its editor; the
  `.taf` files are what the Generator saves.

**the Runner**
: Any of the original Windows players for ADRIFT 3.7 to 4.0, as opposed to
  SCARE or Scarier. When the build matters it is named: **run370**,
  **run380**, **run390**, **run400** are `run370.exe` to `run400.exe`, the
  four builds measured under Wine and decompiled.

**run370x … run400x**
: The same four Runners with an extra import that hooks Visual Basic's
  `Rnd`/`Randomize` and draws from Scarier's xoshiro generator when `VBRNG`
  is set. Without `VBRNG` they behave exactly like the originals. A
  transcript taken on an `x` build can be compared draw for draw with a
  headless Scarier replay (`SCR_RNG=xoshiro`).

**3.7, 3.8, 3.9, 4.0**
: Versions of a *game file*. Scarier plays a game the way the Runner of its
  own version does, so "at 3.9" means "for a 3.9 game, as run390 plays it",
  and `lib_is_version_400()`-style tests read the game's version, not a
  Runner's. "Pre-4.0" is 3.7 to 3.9.

**FrankenDrift**
: Adrian Welcker's port of the ADRIFT 5 Runner. It is the reference for
  `adrift5/` only and is never cited for ADRIFT 4 behaviour.

## Evidence

**probe**
: A small game built to isolate one behaviour, usually named `p<version><TOPIC>`
  (`p39NUMREF`, `p4PFX`) and made by a `make_*.py` script in
  `test/adrift4/harness/`. Its Runner output is kept in
  `test/adrift4/runner_probes/`.

**runner transcript**
: A Runner playing a real game, kept in `test/adrift4/runner_transcripts/`
  and cited by turn (`T27`) or line.

**golden**
: Scarier's own expected output for a replay, in `test/adrift4/goldens/`.
  A golden is checked against a Runner transcript; it is not evidence by
  itself.

**deliberate deviation**
: A place where Scarier knowingly answers differently from the Runner (a
  Runner bug no game needs, a convenience such as UNDO). The comment at the
  site says so in those words; the larger ones are also listed under
  "Deliberate deviations" in `test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md`.

## Comment style

- `/* */` comments, GNU layout: a ` * ` prefix on every line, two spaces
  after a full stop, and lines under 80 columns.
- A function header starts with the function's name and says what the
  function does. Runner behaviour comes next, with the probe or transcript
  that measured it, then any deliberate deviation. The story of how the
  code got there belongs in the commit message, not the comment.
- ` -- ` (two hyphens, spaced) is the dash. There are no em dashes.
- Files that came from SCARE, and Scarier's own C++ files, start with the
  modeline `/* vi: set ts=2 shiftwidth=2 expandtab:`.
- `platform/scarier-autosave.{h,mm}` and `common_utils/sc_garglk.{c,h}`
  follow the Spatterlight sources around them (four-space indent), not the
  GNU layout, and carry no modeline. `common_utils/randomness.{c,h}` and
  `rgbsurface.h` are byte-identical copies of shared files (see
  `common_utils/README`), so they keep the originals' style, `//` comments
  included, and must not be restyled here.
