# Scarier: game-derived style hints (TODO)

This is a plan for giving ADRIFT games more than the fixed handful of Glk
styles Scarier uses today, by scanning the loaded game for the text
attribute combinations it actually uses and assigning the 11 Glk styles to
them before the main window opens. It is the same idea as `osstyles.c` on the
`html-tads` branch (see `terps/tads/glk/osstyles.c` there), adapted to a
frontend where the whole game is already in memory when the hints are set.

The motivating case: several ADRIFT 4 games use `<font size=...>` only in
their title or intro (big game name, smaller "by ..." line, a credits block
in a monospaced face), and never again. Today those sizes are squashed onto
Header / Subheader, which also serve bold text everywhere else in the game.

## Status (2026-10-25)

Steps 1-4 and 6 are done, for ADRIFT 4 and ADRIFT 5, on the
`scarier-styles` branch. Step 5 is not done, and the census suggests it is
not needed (below). What was built differs from the plan in places; each
step below says how. Not done: checking against Runner screenshots and
looking at the result in Gargoyle (Step 6).

Corpus census, `test/adrift4/styles/styles_census -s` over the fetched games:

| | ADRIFT 4 (700 games) | ADRIFT 5 (176 games) |
|---|---|---|
| spares used: 0 | 1 | 107 |
| 1 | 419 | 24 |
| 2 | 98 | 29 |
| 3 | 91 | 10 |
| 4 | 53 | 5 |
| 5 | 30 | 1 |
| 6 / 7 | 7 / 1 | 0 |
| games with a state left to a fallback | 93 (37 of them in the intro) | 9 |
| text in a fallback style | 53,216 of 55,041,828 chars (0.10%) | 771 of 27,465,009 (0.003%) |

The worst ADRIFT 4 games leave 1-3% of their text to a fallback
(yonastoundingcastle 2.6%, TEAW_(introcomp) 2.5%, SBFT 2.1%, provenance
1.7%, warlord 1.2%). Nearly every ADRIFT 4 game spends one spare on its
title, which the Runner prints as 14pt `<c>` text (next section).

### The game title takes a spare

The ADRIFT 4 engine prints the game name itself, as
`<font size=14><c>` + GameName + `</c></font>` (`run_main_intro`,
`runner/scrunner.cpp`). The census walks `Globals/GameName` as intro text with
that markup around it, so the title gets a style of its own: size +1, not
bold. This matches the Runner's 14pt title. Today's precedence printed it in
Subheader (the >= 14pt bucket), which themes draw bold.

The trade-off: the title's style is usually Header, hinted Size +1 and
Weight 0. With "Games can set colors and styles" off, or on Gargoyle, the
hints are not applied, so the title shows in the theme's own Header look
instead of today's bold Subheader. That is a visible change for nearly every
ADRIFT 4 game, so it is worth a look in both before merging. If it reads
wrong, the fix is in `gsc_style_walk_begin()`: leave the title out of the
census, or add the bold bit so the title lands on Subheader.

ADRIFT 5 prints the title as `<c>` + title + `</c>` with no size (`a5run_intro`),
so it keeps its Emphasized stand-in there and spends nothing.

## Where we were

`glk/os_glk_output.cpp`, `gsc_set_glk_style()`, maps the current attribute
state to a style by a fixed precedence:

| Attribute state (first match wins) | Glk style |
|---|---|
| monospaced face | Preformatted |
| right-aligned | Note |
| centred, bold | User2 |
| centred | User1 |
| size >= `GSC_LARGE_FONT_SIZE` (16) | Header |
| size >= `GSC_MEDIUM_FONT_SIZE` (14) | Subheader |
| bold + (italic or underline) | Alert |
| bold | Subheader |
| italic, underline, or a secondary colour with colours off | Emphasized |
| otherwise | Normal |

The hints behind it are set once in `gsc_hint_window_styles()`
(`glk/os_glk.cpp`): User1 and User2 centred, User2 bold, Note right-flush,
Header and Subheader pinned left-flush (the Zoom theme centres Header, which
broke the drop cap in *The Warlord, The Princess & The Bulldog*), and User1
reverse-colour in the text grid. The ADRIFT 5 path
(`gsc_a5_span_style()` in `glk/os_glk_a5_display.cpp`) folds its
centre/right/bold/italic/underline depths onto the same styles and has no
size handling at all.

Consequences:

- A size change that is also bold, centred or in a different face loses one
  of those properties. A 20pt centred title becomes User1 (centred, normal
  size).
- Sizes are three buckets (default, >=14, >=16). A game that uses 10pt
  footnotes, 14pt and 24pt headings gets two of the three.
- Bold text and medium-size text are the same style (Subheader), so a game
  cannot have both.
- The style assignment is the same for every game, so the styles a game
  never uses are wasted while the ones it does use collide.

What the frontend already has that the plan builds on:

- The game is fully parsed before `gsc_hint_window_styles()` and
  `gsc_main_open_windows()` run (`glk/os_glk.cpp`, around lines 1257-1280
  for the normal start and 1501-1505 for the restart path). ADRIFT 4 text
  is reachable through `prop_get_child_count()` / `prop_get_string()`
  (`core/scprops.h`; `tools/scdump.cpp` `scdump_alr_table()` is a worked
  example of a walk). ADRIFT 5 text is in the `a5_xml_node_t` description
  blocks hung off `a5_adventure_t` (`adrift5/a5model.h`).
- `gsc_colour_rebuild_windows()` (`glk/os_glk_commands.cpp`) already closes
  and reopens the whole window set when the colour mode changes, so a
  "re-hint and reopen" path exists. It wipes the main window's scrollback,
  which is why Step 5 below treats closing the main window as a last resort.
- A temporary top-of-screen window already exists for the title graphic
  (`gsc_show_title_graphic()`, `gsc_close_title_graphic()`,
  `GSC_HAVE_TITLE_WINDOW`), opened before the intro and closed by the first
  input (`glk/os_glk.cpp` around line 562) and on autorestore (around line
  1376).
- Spatterlight snapshots the hints per window at open time
  (`GlkController+GlkRequests.m`, `deepCopyOfStyleHintsArray`) and archives
  them in the autosave (`GlkWindow.m`), so a window keeps the hints it was
  opened with across autorestore. It applies `stylehint_Size` as
  `font size + 2 * value` in text buffers only, and only when the theme's
  "Games can set colors and styles" is on (`coredata/GlkStyle.m`,
  `attributesWithHints:`). Gargoyle ignores `stylehint_Size` entirely.

## Ground rule: stay inside the Glk spec

- Only the 11 standard styles, selected with `glk_set_style`. Never a style
  number of 11 or more.
- Hints are set before the window that needs them opens. A hint changed
  after the window is open is not relied on; the spec says it affects only
  windows created afterwards, and Spatterlight's per-window snapshot makes
  that literal.
- Everything degrades to today's behaviour or better on a library that
  honours only Weight / Oblique / Proportional / Justification (Gargoyle),
  and to plain text when the player turns game styles off. The scan must
  never change *what* text is printed, only which style it is printed in.
- No extension calls are needed for this plan. If one is ever wanted
  (per-window re-hinting, say), it goes behind `glk_gestalt` with the
  spec-only path kept working.

## Steps

### Step 1: scan the game for attribute combinations

- [x] New module `glk/os_glk_styles.cpp` / `.h` with a `gsc_style_combo_t`
      bitmask: bold 1, italic 2, underline 4, monospaced 8, centre 16,
      right 32, plus a size bucket in the high bits. Size is kept as the
      actual point size, normalised so the game's default (12, or the
      `<font size>` the intro opens with) is bucket 0; anything within a
      point of a bucket joins it.
      *Changed:* bold 1, italic 2, monospaced 4, centre 8, right 16.
      Underline shares the italic bit: Glk has no underline, and today's
      precedence already printed both as Emphasized. So does `<c>` text with
      colours off, which keeps its Emphasized stand-in, but only on otherwise
      plain text, as before. The size is a signed step of two points from a
      fixed 12 (the Runner's default), clamped to [-3, +6]. That step is
      the `stylehint_Size` value as it is.
- [x] `gsc_styles_scan_a4(sc_gameref_t)`: walk every string property that
      can reach the screen. Introduction, win text, room short and long
      descriptions and alternates, object descriptions and read text, task
      completion and fail messages, event start / middle / end text, NPC
      descriptions, walks and conversation replies, the `ALRs` replacement
      strings, the status box text, and the game name / author (they are
      printed by the frontend on the `info` command). Use the same key
      strings the engine reads them with; `grep -n 'prop_get_string'
      core/*.cpp` is the inventory.
      *Changed:* `gsc_style_census_a4()` walks the whole property tree
      (`scr_game_scan_markup()`, `core/scprops.cpp`) rather than a hand-kept
      list of keys, so no printed string can be missed when the engine reads
      a new one. Only `Header/StartupText` and `Globals/GameName` count as
      intro.
- [x] Each string is fed through a tag walker that mirrors the Runner
      semantics already in `gsc_handle_attribute_tag()` and
      `gsc_handle_font_tag()` exactly: `<b>` `<i>` `<u>` set flat flags
      (no nesting count, a close clears), `<font face= size= colour=>` pushes
      on a stack of `GSC_MAX_STYLE_NESTING`, `</font>` pops, `size=+N` /
      `-N` are relative to the enclosing size, `<center>` `<right>` are
      alignments, `<waitkey>` `<cls>` and friends are ignored. ALR
      replacement text is scanned where it appears, not substituted; this is a
      census, not a render.
      The walker and the live tag handler share `gsc_style_face_is_monospaced()`
      and `gsc_style_apply_font_size()`, so the two cannot disagree.
- [x] Count how many *characters* of visible text each combo covers, per
      string class, and separately record the set of combos the intro uses.
      Characters rather than tag occurrences, because a game that writes one
      `<font size=24>` around a title is as important as one that opens and
      closes bold fifty times.
- [x] `gsc_styles_scan_a5(a5_adventure_t *)`: the same walker over every
      `a5_xml_node_t` description block (`node`, `conversation`,
      `description`, `fail_override`, `new_text` in `a5model.h`), with the
      ADRIFT 5 extras `gsc_a5_span_style()` already understands (nested
      `<center>` / `<right>` depths) plus `<font size>`, which it does not.
      Expression-generated text (`<#...#>`) cannot be scanned and is skipped.
      *Changed:* `gsc_style_census_a5()` walks every `<Text>` element in the
      file, each its own string; those under `<Introduction>`, and the title,
      are intro. Expressions and functions are counted as written, not
      skipped. `<font size>` and `face` now reach the Glk side: the colour
      span mark a `<font>` already left (`\027colour\027`) carries
      `;size=...;face=...` after the colour (`a5_render_colour()`,
      `adrift5/text/a5text_render.cpp`; documented in `a5text.h`). Headless
      mode still drops `<font>` to `A5_ALR_MARK`, so no golden can change.
- [x] `tools/scdump.cpp`: a `--styles` flag that prints the census for a
      game, ranked, so the scan can be checked by eye against the game text
      and against Runner transcripts in `test/adrift4/runner_transcripts/`.
      *Changed:* a standalone harness instead,
      `test/adrift4/styles/styles_census` (`make -f Makefile.headless
      styles`). It takes ADRIFT 4 and ADRIFT 5 games and prints the ranked
      census, the style each state lands in, and `*` for a state with a
      style of its own. `-s` prints one summary line per game.

### Step 2: rank the combos and assign styles

- [x] A fixed table of the natural home for each simple combo: bold ->
      Subheader, italic / underline -> Emphasized, bold+italic -> Alert,
      monospaced -> Preformatted, centred -> User1, centred bold -> User2,
      right -> Note. These keep today's look for games that only use
      today's combos, which is nearly all of them, so no golden transcript
      changes meaning.
- [x] Spare styles, in order: Header, BlockQuote, then whichever of User1 /
      User2 / Note / Alert the game does not need for their natural combos.
      Subheader joins the end of that list too.
      Input stays Input and Normal stays Normal.
- [x] Ranking: natural combos that appear get their natural style. Every
      combo the intro uses gets a slot, in intro character order, before
      anything else competes for the spares. Remaining combos are ranked by
      character count, ties broken by fewer attribute bits. At most 9
      styles are assigned (11 minus Normal and Input).
      *Changed:* intro characters count 20 times over instead of strictly
      first (`GSC_STYLE_INTRO_WEIGHT`). Strict intro-first let a ten-character
      byline outrank a state that covers whole rooms, and across the corpus it
      left about twice as much text to a fallback. Ties go to fewer bits,
      then to the lower combo value, so the order never depends on a map.
- [x] Nearest-combo fallback for everything left over, in this order: drop
      the size (keep the weight / slant / alignment), drop underline onto
      italic, drop italic onto bold, drop the alignment, drop bold. The
      fallback must never land on Input or on a combo that would *add* an
      attribute the text did not ask for.
      *Changed* (`gsc_style_lookup()`): first step the size towards the
      default one step at a time. Then, at the default size, drop italic,
      then bold, then the alignment, then the face, each on top of the last,
      and finally Normal. Bold goes before the alignment because a centred
      line that loses its centring reads worse than one that loses its
      weight. Underline is already italic.
- [x] Keep the Header / Subheader left-flush pin whatever they are assigned
      to, unless the combo they are assigned to is centred or right-aligned.
      A style a spare goes to is hinted from its combo, Justification
      included. A natural style keeps exactly the hints it has today.
- [x] Record the final assignment in a `combo -> style` table and the
      inverse `style -> combo`, both consulted at print time (Step 3) and
      dumped by `scdump --styles`.
      One `gsc_style_table_t` (style -> combo, plus `natural` per style);
      the lookup searches it.

### Step 3: print through the table

- [x] `gsc_set_glk_style()` builds the current combo from the font stack and
      flags, looks it up, and falls back along the Step 2 chain when it
      misses (it can miss: ALR substitution and expression text can produce
      combos the scan did not see). Remove the fixed precedence list; keep
      the colours-off "secondary colour means Emphasized" rule as an extra
      bit in the combo so it ranks like everything else.
- [x] `gsc_a5_span_style()` does the same, with a new `size` argument
      threaded from the ADRIFT 5 tag parser (`os_glk_a5_display.cpp`), and
      the frontend stops ignoring `<font size>` in ADRIFT 5 text.
      The display keeps the face and size per colour span
      (`monospaced_stack`, `points_stack`) beside the colour stack. `<c>`
      and a `<font>` without the attribute inherit the enclosing values, and
      a display commit resets them with the other spans.
- [x] The grid (status) window keeps its own two styles; the scan does not
      touch it.
      Also: the frontend's own direct `glk_set_style` calls use only Normal,
      Input and Emphasized, none of which can be a spare, and Header, for
      error banners (`gsc_header_string()`). Those banners now use Header
      only while it is natural, and otherwise the style bold text gets.

### Step 4: set the hints before the windows open

- [x] `gsc_hint_window_styles()` clears every text-buffer hint it is about
      to set (`glk_stylehint_clear` per style per hint), then for each
      assigned style sets Weight, Oblique, Proportional, Justification and
      Size from the combo. Size is `(points - default) / 2`, clamped to
      `[-3, +6]`, 0 for the default bucket. Indentation and ParaIndentation
      are set to 0 so a theme's Header or BlockQuote indent does not leak
      into a style now used for a centred title.
- [x] The call order in both start paths (`glk/os_glk.cpp` ~1257 and
      ~1501) becomes: load game, scan, hint, open windows. Restart with a
      different game file (the `restart` command path) must rescan.
      Restart reuses the same game, so it keeps the table. ADRIFT 5 scans in
      `gsc_a5_main_setup()` (`glk/os_glk_a5.cpp`).
- [x] `gsc_colour_rebuild_windows()` re-hints from the stored table before
      reopening, so a colour-mode toggle keeps the game's styles.
      Nothing needed: it changes only Normal's hints, so the others stand.
- [x] Autosave: the table is derived deterministically from the game file,
      so nothing new goes in the autosave. On autorestore the main window
      comes back with the hints it was opened with, which are the same
      ones the rescan produces. Add an assertion in debug builds that the
      rescan matches, so a future change to the scan that would desync a
      restored window shows up in `make autosave`.
      *Changed:* no debug assertion. On autorestore the windows come back
      with their archived hints and the terp sets none that reach them. The
      `styles-intro-pause` autosave case checks the real risk instead: every
      resumed session must set the same hints as the uninterrupted one, and
      print the same styled text.

### Step 5: the title window, only if Step 2 is not enough

**Not done.** Measured, the census says Step 2 is enough: 93 of 700 ADRIFT 4
games and 9 of 176 ADRIFT 5 games have any state in a fallback style, with
0.10% and 0.003% of their text in one. In the worst ADRIFT 4 game it is 2.6%.
A title window would buy back that last fraction of a percent at the cost
the bullets below list. It is a judgement call left open.

The census (Step 1) will say how many games exhaust the 9 slots. The
expectation is very few: most games use two or three combos in the intro
and one or two in play. Only if a real game needs more does the title
window pay for itself.

- [ ] A text-buffer window opened above the main window, like the title
      graphic, with hints computed from the intro's combos alone, used for
      the intro text, and closed on first input together with the graphic
      (same places: `glk/os_glk.cpp` ~562 and the autorestore ~1376).
      Main-window hints are then computed from the non-intro census with
      all 9 slots free.
- [ ] It is taken only when the intro census and the play census together
      overflow the slots. The intro still echoes nothing to the main window,
      so the scrollback starts at the first room description, which matches
      what the Runner shows after its intro is dismissed.
- [ ] `<waitkey>` and `<cls>` inside the intro must work in the title
      window; the paging code in `gsc_a5_display` and the ADRIFT 4 output
      path both assume the main window and need a window argument.
- [ ] Do **not** close and reopen the main window after the intro. It
      wipes the transcript and the scrollback, loses any pending line
      input, and on autorestore the restored window would not match. The
      title window gives the same effect without those costs.

### Step 6: tests

- [x] `test/glkdrive.py` already records `STYLEHINT`, `CLEARHINT` and the
      style of every `PRINT`. Add a `--styles` report that prints the hints
      set per style and the style each printed span used, so a fixture can
      assert "the title printed in a style with Size +6 and centred".
      Copied byte for byte to `terps/question/test/glkdrive.py`.
- [x] Fixtures: one tiny `.taf` per case under `test/adrift4/styles/`,
      built with the generator recipe in `test/README.md`: intro-only
      sizes; bold and medium size both used in play; a game that overflows
      9 combos (checks the fallback chain); a game with no markup at all
      (checks nothing changes).
      They are generated by `make_styles_tafs.py` there. Their census is pinned in
      `census_expected.txt` (`make -f Makefile.headless stylestest`, part of
      `quick`). What the Spatterlight build prints them in is pinned in
      `*_glk_expected.txt` (`run_styles_tests.py`), and so is the ADRIFT 5
      corpus game Death Shack, whose title cards are centred 20pt and 24pt
      `<font>` spans.
- [x] The headless suite (`test/adrift4/harness`, `quick`) must produce
      identical text before and after. Styles are not in the goldens, so a
      text diff is the regression; run it under both `SCR_RNG` settings as
      usual.
      `quick` and the ADRIFT 5 walkthrough goldens pass unchanged. The full
      suite matched its baseline at the ADRIFT 4 commit, run with the default
      RNG only.
- [x] `make autosave` cases for a game whose intro uses a size: autosave in
      the intro, autosave after first input, restore both, check the style
      of the next printed span.
      `styles-intro-pause` in `run_autosave_tests.py`: paused at the intro's
      `<waitkey>`, after it, and after the first command.
- [ ] Check against the Runner: pick the games the census ranks highest for
      intro sizes and compare with their `runner_transcripts/` screenshots
      or the Wine harness (`wine-adrift4-runner-harness` notes), to confirm
      the sizes and faces the scan found are the ones the Runner draws.
- [ ] Spatterlight with "Games can set colors and styles" off, and
      Gargoyle, must look exactly as they do today. The style *numbers*
      change for some text, so check the Gargoyle theme does not give a
      reassigned style (BlockQuote, Header) a look that reads wrong for a
      centred title; if it does, prefer User1 / User2 as spares ahead of
      Header / BlockQuote.
      Not checked by eye yet. See "The game title takes a spare" above for
      the one change every ADRIFT 4 game will show. The garglk fork's build
      also needs `glk/os_glk_styles.cpp` added before it can link this.

## Out of scope

- Colours. `<font colour=>` is already handled through `garglk_set_zcolors`
  and stays orthogonal to the style table.
- Mid-game re-hinting of the main window. See Step 5.
- Styles in the map and side windows. They print fixed text.
