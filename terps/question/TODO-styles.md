# Question: game-derived style hints (TODO)

This is a plan for giving Quest games more than the four Glk styles Question
uses today (Subheader for bold, Emphasized for italic, Alert for both, User2
for underline), by scanning the game for the text attribute combinations it
uses and assigning the 11 Glk styles to them before the main window opens.
It is the same idea as `osstyles.c` on the `html-tads` branch
(`terps/tads/glk/osstyles.c` there) and as `terps/scarier/TODO-styles.md`,
which should be read first: the ranking, fallback and hint-setting rules are
the same and only the scan differs. Where Scarier can share one module for
both of its engines, Question needs two scans, because Quest 4 text uses
`|` codes and Quest 5 text uses HTML.

## Where we are now

Both frontends go through `glk_style_for(bold, italic, underlined)` in
`questglk-common.cc`:

| Attribute state | Glk style |
|---|---|
| bold + italic | Alert |
| italic | Emphasized |
| bold | Subheader |
| underline | User2 |
| otherwise | Normal |

Hints: User2 reverse-colour in text buffers (`questionglk.cc` ~643) and
User1 reverse-colour in the text grid (~523). Nothing else is hinted.

**Quest 4** (`question-runner.cc`): `QuestionFontStyle` (`QuestionRunner.hh`)
already carries everything the `.asl` text can ask for: bold / italic /
underline, colour, font name, `size` (`|sNN`, default 10, parsed ~256-274)
and `justify` (`|jl` `|jr` `|jc`, ~278-292). `QuestionGlkInterface::set_style`
(`questionglk.cc` ~1373) passes only the three flags to `glk_style_for`, so
size, font and justification are dropped on the floor. The ordering problem:
`glk_main` opens `mainglkwin` (~645) *before* `boot_game` loads the file
(`set_game`, ~584 / 596 / 610), so there is no point at which the game text is
known and the window is not yet open.

**Quest 5** (`quest5/aslxglk.cc`, `aslxglk-output.cc`): `aslx_glk_main`
(~1097) opens `gwin` and `gbanner` first and `run_session` loads the file
(`load_file`, ~814) afterwards. The output filter keeps a `StylePush{b,i,u}`
stack; `css_style()` reads only `font-weight`, `font-style` and
`text-decoration`; the tag dispatch (~557) knows `br b strong i em u span a
img audio`. `<font>`, `font-size`, `h1`-`h6`, `<center>`, `text-align`,
`<pre>`, `<tt>` / `<code>` and `font-family` are not handled. The world
model (`quest5/aslx.hh`, `World` / `Element` with `fields` of `Value`) holds
every attribute string, the `templates` lists hold the library text, and
JavaScript-generated markup is out of reach.

Consequences:

- A Quest 4 title written as `|jc|s20|b` prints as plain bold, left-aligned,
  default size. Quest 4 games use `|s` and `|j` almost only in titles and
  room headings, which is exactly the case that needs the most styles.
- A Quest 5 `<h1>` or `<span style="font-size:20pt">` prints as Normal.
- Underline takes User2, which Spatterlight themes show as reverse video,
  so an underlined heading looks like a selection.

What already helps:

- `test/glkdrive.py` (byte-identical with Scarier's) records `STYLEHINT`,
  `CLEARHINT` and the style of each `PRINT`.
- `questglk_unit_tests.cc` tests the shared frontend helpers, so the
  combo table and fallback chain can be unit-tested without a game.
- Quest 4 has an in-memory copy of the whole `.asl` (`QuestionFile::data`,
  `questionfile.hh`) after `set_game`, and Quest 5 has the parsed `World`,
  so neither scan has to re-read the file.

## Ground rule: stay inside the Glk spec

Same as Scarier: 11 styles, hints set before the window opens, no reliance
on re-hinting an open window, identical printed text with styles off or on
Gargoyle (which ignores `stylehint_Size`), extensions only behind
`glk_gestalt`. Both frontends already use `garglk_set_zcolors` for colours,
and colours stay outside this plan.

## Shared steps

- [x] Move the combo type, ranking, fallback chain and hint-setting code
      into `questglk-common.cc` / `.hh`: `QuestStyleCombo` (bold 1, italic
      2, underline 4, monospaced 8, centre 16, right 32, size bucket in the
      high bits), `quest_styles_assign(census)`, `quest_styles_set_hints()`,
      `quest_style_for(combo)` replacing `glk_style_for(b, i, u)`. The
      ranking rules are the ones in Scarier's TODO Step 2: natural homes
      first (bold Subheader, italic / underline Emphasized, bold+italic
      Alert, monospaced Preformatted, centred User1, centred bold User2,
      right Note), every intro / title combo gets a slot, remaining combos
      by character count, fallback drops size, then underline, italic,
      alignment, bold.
      *Done:* as in Scarier, with `StyleCombo` bold 1, italic 2, mono 4, centre 8, right 16 and the size step in bits 8-15 (underline folds into italic, so it has no bit); `style_assign`, `style_install`, `style_set_hints`, `quest_style_for`.  Spares are Header and BlockQuote, then unused User1 / User2 / Note / Alert; Subheader, Emphasized and Preformatted never, because the panes and notes use them.  Lookup and the alignment post-pass are Scarier's (fb7aa86d0, b2a7b5efb).
- [x] Underline moves off User2 onto Emphasized (its natural home above)
      unless the game also uses italic and has a spare slot, in which case
      underline gets the spare with Oblique 0 and no reverse-colour hint.
      This is a visible change for every game that underlines; note it in
      the CHANGELOG when it lands.
      *Done:* underline is always Emphasized (no spare of its own).  CHANGELOG noted.
- [x] Hints per assigned style: clear, then Weight, Oblique, Proportional,
      Justification, Size `(points - default) / 2` clamped to `[-3, +6]`,
      Indentation and ParaIndentation 0. The grid window keeps its one
      reverse-colour hint; the scan does not touch it.
      *Done:* the grid's User1 reverse hint is now set by `style_set_hints` as well.
- [x] Unit tests in `questglk_unit_tests.cc` for the assignment: a census
      with only today's combos gets today's styles; an overflowing census
      falls back in the documented order; the intro combos always win a
      slot.
      *Done:* `test_style_combo`, `test_style_assign`.

## Quest 4

### Step 1: load before opening

- [x] Reorder `glk_main` so `boot_game` (the `set_game` call and the
      `.asl` / `.cas` parse) runs before `mainglkwin` opens. Anything that
      prints during loading (file errors, the "not a Quest 4 game" message,
      the `asl-version` complaint) is buffered and printed after the window
      opens, which is what Scarier does with its load errors. The autosave
      path (`autosave/run_autosave_tests.py` cases) must still find the
      window open when it restores, so check where the autorestore hook
      sits relative to the new order.
      *Done:* not reordered: `assign_game_styles` reads the file a second time, silently (`question_style_scan`, `read_question_file`), before the window opens, so `set_game` and the autosave paths are untouched.

### Step 2: scan the `.asl`

- [x] `question-styles.cc`: walk `QuestionFile::data` line by line and feed
      every string that can reach the screen through the same `|` code
      parser the runner uses (`question-runner.cc` ~201-330): `|b` `|i`
      `|u` and their `|xb` closes, `|sNN` and `|s00` reset, `|jl` `|jr`
      `|jc`, `|c*` colours (ignored here), `|n` `|w` `|cls` (ignored).
      Factor that parser out of the print path so the scan and the runner
      cannot drift. Count characters per combo, and record the combos used
      by the `intro`, `startscript` `msg` strings and the game `name` as the
      title set.
      *Done:* in `question-runner.cc` (`question_style_scan`); the `|` parser is factored out as `question_parse_format_code` and the print path uses it.  Intro set: `intro` text, `startscript`, procedures and text blocks reached from them by `do` / `displaytext`, and the game name (printed italic).
- [x] Which lines count: `msg <...>`, `description <...>`, `look <...>`,
      `examine <...>`, `prefix` / `alias` text, `define text` blocks,
      `|`-coded strings inside `startscript` and `define game`, menu and
      `ask` strings. Some Quest 4 games also carry raw HTML in text (`<b>`,
      `<font size=>`, `<center>`). The runner does not interpret it today
      (`question-runner.cc` has no tag handling), so the scan ignores it
      too; adding HTML to Quest 4 is a separate decision for the runner
      first.
      *Done:* every `<...>` parameter of every line outside `synonyms` blocks, plus `define text` lines.
- [x] Sizes: `|s` is in points with default 10. Bucket relative to 10.
      The `size` field of `QuestionFontStyle` is already an int; keep it.
      *Done:* `size` now defaults to 0 ("the game's default"), and `default fontsize` is read from the game block.

### Step 3: print through the table

- [x] `QuestionGlkInterface::set_style` builds the combo from the whole
      `QuestionFontStyle` (flags, size, justify, and `font` when it names
      a monospaced family: Courier, Lucida Console, Consolas, "Fixedsys")
      and calls `quest_style_for`. The fallback chain covers combos the
      scan did not see (text built at runtime with `|s` inside a string
      variable).
      *Done:* but without monospace: Quest 4 font names are not mapped (a game choosing Courier is rare and a font name is a theme decision).
- [x] `|jc` and `|jr` emit nothing today (no spacing is faked; the
      `justify` field is set and never read), so the Justification hint is
      a pure addition and no printed text changes.

## Quest 5

### Step 1: load before opening

- [x] In `aslx_glk_main`, move `load_file` out of `run_session` and ahead
      of the `gwin` / `gbanner` opens, buffering load-time output the same
      way as Quest 4. `gbanner` is a grid and needs no change, but it is
      opened in the same block, so keep the relative order of the two
      opens for the autosave window-id matching in `autosave/`.
      *Done:* not moved: `aslx_glk_main` loads the game into a `World` before any window opens, runs the census on it and hands it to the first `run_session`, so the file is still parsed once.  If that load fails, `run_session` loads again and prints the errors as before; restarts and restores reload as before.  The window order is unchanged.

### Step 2: scan the world model

- [x] `quest5/aslx-styles.cc`: walk `World::elements` and every `Value` of
      string kind in `Element::fields`, plus `World::templates`,
      `dynamic_templates` and `verb_templates`, plus the `game` element's
      `description`, `subtitle`, `gameid` and `firstpublished` only for the
      title set (together with whatever the `start` script prints through
      a literal `msg ("...")`; a cheap regex over script text for `msg (`
      string literals is enough, no script execution).
      *Done:* `assign_game_styles` at the end of `aslxglk-output.cc` (no new file, so no pbxproj change).  It walks every string, string list, dictionary and script field (every string literal in a script, not only `msg`) plus the three template lists.  The intro set is the `start` script and the title Core's StartGame prints when `showtitle` is on: the name centred at 260%, subtitle 130%, "by" author 140%.  Core is counted with the game; on its own it adds only plain, bold and italic text and 2 characters of its own title literal, so there is no library exclusion.  `ASLXGLK_STYLE_CENSUS=1` prints the rows and their styles to stderr.
- [x] Feed each string through an HTML tag walker shared with
      `aslxglk-output.cc` (factor the tag dispatch and `css_style()` so the
      scan and the printer share one parser): `b strong i em u`, `span`
      and `font` with `style=` (`font-weight`, `font-style`,
      `text-decoration`, `font-size` in pt / px / em / `%` and the
      keywords `small` `large` `x-large`, `font-family`, `text-align`),
      `font size=1..7` and `size=+N`, `h1`-`h6` (sizes 24 20 16 14 12 10
      bold, matching the QuestViva default stylesheet; check
      `questviva` CSS before fixing the numbers), `center`, `pre` `tt`
      `code` (monospaced), `p style="text-align:..."`, `div align=`.
      Quest 5's `<br>` and `<img>` are already handled and do not affect
      the combo. Character counts per combo as before.
      *Done:* the scan and the printer share `tag_style`, `css_style` and `tag_is_block`.  Headings use the browser defaults (2, 1.5, 1.17, 1, .83, .67 em): QuestViva's stylesheet sets no heading sizes.  The text processor's `{b:` `{i:` `{u:` count as their tags.  Scripts: a literal on a `PrintCentered` line or between `SetAlignment ("center")` and the next `SetAlignment` is centred.
- [x] Default size: in QuestViva the `game` element's `defaultfontsize`
      (12 if unset) and `defaultfont` set the base font through CSS.
      Question reads neither today (no hit for `defaultfontsize` in
      `quest5/`), which is fine for text but matters here: a
      `<span style="font-size:12pt">` in a game with `defaultfontsize` 14
      is *smaller* than normal, so the scan reads the field and buckets
      against the game's default, not against 12.
      *Done:* both the scan and the printer bucket against `defaultfontsize` (inherited, 12 if unset).  When `defaultfont` / `defaultwebfont` is itself monospaced, no `font-family` counts as mono.
- [x] `SetFontSize` / `SetFontName` are Quest 5 script functions that
      change the default at runtime. Question does not implement them yet
      (no hit in `quest5/`); when it does, treat a runtime size change as a
      combo change for text printed after it, not as a re-hint, and have
      the scan count `SetFontSize (N)` literals in scripts as sizes the game
      uses.
      *Done:* already so: Core's `SetFontSize` sets `game.defaultfontsize`, and Core wraps every message in a span at that size, so the printer sees a size change without any new code.  The scan counts the literals after a `SetFontSize (N)` in the same script at N points.  `SetFontName` is not followed.

### Step 3: print through the table

- [x] Extend `StylePush` with `size`, `mono` and `align`, and make the
      tag dispatch push and pop them for every tag the scan understands.
      `apply_style()` builds the combo and calls `quest_style_for`.
      *Done:* the style state is now `InlineStyle` (counters for the flags, stacks for size and alignment); `createNewDiv` (`SetAlignment`) sets the alignment of the text that follows.
- [ ] `h1`-`h6` and `p` / `div` / `center` are block tags: emit the line
      break they imply before and after (QuestViva does), and set the
      alignment for the block.
      *Partly done:* the alignment, size and weight of a block are set and undone at its close, but no line breaks are added, so the printed text is unchanged.  Adding them changes text in Spatterlight only, and is a separate decision.

## Tests (both engines)

- [x] `test/glkdrive.py --styles`: the same report as Scarier's (hints set
      per style, style of every printed span). Keep the file byte-identical
      with `terps/scarier/test/glkdrive.py`; `make check` enforces it, so
      land the driver change in both trees in one commit.
      *Done:* `Driver.hints` and `Driver.spans`, synced from scarier-styles with the Quest 4 step.
- [x] Quest 4 fixtures under `test/quest4/fixtures/` (the `make check`
      games): a `|jc|s20` title, a game using `|b` and `|s14` separately in
      play, an overflow case, a no-markup case.
      *Done:* under `test/styles/` instead (`styles_*.asl`), run by `styles/run_styles_tests.py` / `make styles`, since they need the Spatterlight build.
- [x] Quest 5 fixtures under `test/quest5/`: `<h1>` title, `font-size`
      spans, `defaultfontsize` 14 with a 12pt span, a `pre` block, an
      overflow case.
      *Done:* in `test/styles/` beside the Quest 4 ones: `styles_q5title.aslx` (Core's title, `<b>` / `{b:}`, a 14pt span) and `styles_q5sizes.aslx` (`defaultfontsize` 14, a 12pt span, Courier and `<code>`, `<h2>`, `PrintCentered`, `SetAlignment`, `SetFontSize`).  The overflow case is the Quest 4 fixture's and the unit tests'; the assignment is shared.
- [x] The Quest 4 sweep (`quest4/harness/run_walkthroughs.sh`, 150 games)
      and the Quest 5 native replays (`quest5/harness/run_replays.sh`) must
      produce identical text. Styles are not in the goldens; a text diff is
      the regression. Watch for Quest 4 alignment spacing (Quest 4 Step 3)
      and Quest 5 block tags (Quest 5 Step 3), which change text and need
      new goldens if they are deliberate.
      *Done:* Quest 4 sweep pass=150; Quest 5 native replays 367 passed plus the 7 documented rows (these link only the engine).  The Glk frontend's own text, `aslxglk_smoke` over all 374 `.cmd` scripts, is byte-identical with the base commit.
- [x] `make autosave`: autosave during a sized title and after the first
      command, restore both, check the next span's style. Also autosave
      across the load reorder in both engines, since the window creation
      order is what the autorestore matches on.
      *Done:* `probe4-styles.asl` / `probe5-styles.aslx` and the cases
      `q4-styles-wait`, `q4-styles`, `q5-styles-wait`, `q5-styles`; the
      suite now marks every style change in the compared output, so all
      its cases check styles.  There was no load reorder in the end (both
      engines scan the file before the window opens and load as before).
- [ ] Check a handful of games against the .NET / QuestViva oracle
      (`quest5-dotnet-groundtruth` notes) and the Quest 4.1.5 Wine harness
      for whether the sizes the scan found are what the originals draw.
- [x] Spatterlight with game styles off, and Gargoyle, must look as today
      except for underline moving off reverse video. If a reassigned spare
      (Header, BlockQuote) looks wrong for a centred title in the Gargoyle
      theme, prefer User1 / User2 as spares ahead of them.
      *Done:* checked with uranus.asl and A Christmas Game.quest.  Gargoyle
      centres the titles and changes nothing else.  Spatterlight with styles
      off ignores every hint and draws a style in the theme's default look
      (Header bold, Alert bold italic, the other spares plain), so a fixed
      spare order swapped bold and plain titles; `style_assign` now gives
      each state the spare whose default look is nearest its own, and with
      styles off only the centring and size are lost (a bold title on Header
      is the theme's two points larger).

## Out of scope

- Colours, images, hyperlinks, the Quest 5 panes and the Quest 4 side
  pane; all unchanged.
- JavaScript-generated markup in Quest 5. The fallback chain handles
  whatever it produces at print time.
- Re-hinting the main window mid-game. If a game turns out to need more
  than 9 combos, use the Scarier TODO's Step 5 (a temporary title window)
  rather than closing the main window.
