# HTML TADS in Glk: TODO

This is a plan for running HTML TADS games in Spatterlight's TADS 2 and TADS 3
interpreters. The idea is to translate HTML TADS markup into Glk calls, plus a
few Spatterlight extensions, instead of embedding an HTML renderer.

The numbers come from the corpus in `terps/tads/test/htmltads`: 235 games,
206 of them TADS 2 and 29 TADS 3 (see `test/GAMES.md`). "Games" means the
number of games whose compiled strings contain the tag or attribute. Library
boilerplate inflates some counts. For example, the adv.t status line puts
`<banner>`, `<body bgcolor=statusbg>` and `<tab align=right>` into almost
every TADS 2 game, and `<font face='TADS-Input'>` into most of them.

## Where we are now

The interpreters are built text-only. `SYSINFO_HTML` is 0 and `USE_HTML` is not
defined, so markup goes through each engine's own text-mode filter:

| | TADS 2 (`tads2/output.c`) | TADS 3 (`tads3/vmconhmp.cpp`) |
|---|---|---|
| Parses markup | only after `\H+` | always |
| Acted on | `br` (HEIGHT), `b` `i` `em`, `p`, `tab` (as a bare tab), `img`/`sound` ALT, `hr`, `q`, `title`, `aboutbox` (contents suppressed), `pre` | the same, plus a full `tab` implementation, `font COLOR/BGCOLOR`, `body TEXT/BGCOLOR`, `wrap`, `log`, and `banner` contents suppressed |
| Dropped, contents printed | everything else, **including `<banner>` contents**, which then print inline in the main window | everything else |
| Entities | ASCII approximations, via `os_xlat_html4` → `CCharmapToLocalASCII` | real Unicode |

Other gaps in the current build:

- `<strong>` does nothing. `OS_ATTR_STRONG` is 0, and the overrides in
  `osspatter.h:59-61` are commented out.
- `os_set_text_attr` can only express bold, italic and bold+italic
  (`osglk.c:404`).
- No game resources are reachable from TADS 2. `t23run.cpp` passes a NULL
  `appctx`, so the HTMLRES entries and the `.rsN` files are skipped. (Fixed
  in Step 0.)
- TADS 3 can find resources: `G_host_ifc->find_resource()` returns an open
  file positioned at the resource, and `.3rN` files are loaded too.
- `glk/` contains no calls to images, sound channels or hyperlinks.

## Ground rule: stay inside the Glk spec

Everything here must also run, in a reduced form, on a Glk library that
follows the spec and has none of Spatterlight's extensions. In practice:

- Use only the 11 standard styles. Use the hints the spec defines, set before
  the window that needs them is opened. Never pass a style number of 11 or
  more to `glk_set_style`, and never rely on a hint changing a window that is
  already open. The spec says hint changes affect only windows created
  afterwards.
- Extensions are additive. They have their own calls, are detected through
  `glk_gestalt`, and have a spec-only fallback. Extensions that already exist
  are used the same way:
  - `garglk_set_zcolors`, `garglk_set_reversevideo`, `garglk_set_story_title`
  - `garglk_add_resource_from_file`
  - `win_setbgnd`
- Spec features are used for what the spec defines them for:
  - hyperlinks: `glk_set_hyperlink` and hyperlink events
  - images and margin images: `glk_image_draw*` and
    `glk_window_flow_break`
  - sound: `glk_schannel_*` (including the 0.7.3 volume duration and the
    sound notify events)
  - `glk_style_measure` and `glk_style_distinguish`, to decide whether a
    fallback is needed
- Run the input loop as the spec describes it. Handle a hyperlink, sound
  notify or arrange event that arrives during line input without cancelling
  the input unless needed. Echo cancelled input the spec way (with
  `glk_set_echo_line_event` where available).

## Step 0: Plumbing (needed by everything else)

- [x] **Get the raw markup out of both engines.**
  - TADS 2: build `output.c` with `USE_HTML` defined, so `html_target` is
    TRUE. After `\H+`, tags and entities then pass through to `os_printz`
    unmodified. Games without `\H+` keep their current text-mode
    behaviour.
  - TADS 3: link `vmconhtm.cpp` instead of `vmconnom.cpp`. Markup then reaches
    `os_printz` (main window) and `os_banner_disp` (text banners). Grid
    banners and the log file stay plain text.
  - Write `os_start_html` and `os_end_html`; no file in `glk/` defines them
    yet.
  - Both formatters now emit their own markup, so the parser must handle it:
    newlines become `<BR HEIGHT=0>`, `\ ` becomes `&nbsp;`, and TADS 2's
    `\(`/`\)` become `<B>`/`</B>`.
- [x] **A shared markup parser, `glk/oshtml.c`.** It takes the byte stream
  from `os_printz` and `os_banner_disp` and produces Glk calls.
  - Tokenise tags, attributes and entities, which can be split across print
    calls.
  - Keep a tag stack (`<b><i></b>` must be tolerated) and a style state.
  - TADS 3 currently handles `<tab>` (all of it), `<wrap>` and `<log>` itself.
    Once markup passes through, the new parser must reproduce whatever of
    these the games need. The `<tab>` logic in `vmconhmp.cpp:609-923` can be
    ported for grid windows.
  - Done for TADS 3 as well: `<tab>` (align, `to`/`id`, `indent`,
    `multiple`) is ported, and the formatter's column, trailing-space,
    `<br>` and blank-line rules are reproduced, so the main window prints
    what the text-mode console printed. The differences that remain are
    intended:
    - The formatter no longer flushes at `<br>`, so text can reach Glk later
      than before. A reply can now come after a status-line redraw
      (*Teachers Pet*, *Chaos*), and `<body bgcolor>` after a clear is
      applied at the next flush (*Macrocosm*). `<title>` timing moves too.
    - Text banners always let Glk wrap. The old code made the formatter
      wrap banners that used tab styles (*Macrocosm*, *Lydia's Heart*,
      *White Bull* status lines).
    - Text mode treated `<TAB MULTIPLE=n>` as `<TAB INDENT=n>`
      (`vmconhmp.cpp:1240`). It is now a tab to the next stop, using the
      formatter's `\t` arithmetic: the stop is where `(column + 1) % n`
      is 0. HTML TADS may count from column 0 instead; that is unchecked.
    - Entities that the text-mode parser broke are now decoded.
- [x] **Entities as Unicode for both engines.** Route TADS 2 through the same
  entity table as TADS 3 and output with `glk_put_char_uni`. This fixes the
  ASCII-only entities in TADS 2 even in text mode.
- [ ] **Answer `systemInfo` honestly.**
  - [x] Set `SYSINFO_HTML=1` and `INTERP_CLASS=ICLASS_HTML`, for TADS 2
    (`osglk.c`, with Step 3), and later for TADS 3 too. adv3's
    `checkHtmlMode()` asks for the HTML class, and `aHrefAlt` writes a
    link only in HTML mode: without it a menu had no Exit link
    (*Thaumistry*'s hints). adv3's banners still go through the banner API
    (`osglkban.c`).
  - [x] `JPEG`, `PNG`, `PNG_TRANS`, `PNG_ALPHA` and `PREF_IMAGES` (TADS 2,
    with Step 5): 1 when `gestalt_Graphics` and `gestalt_DrawImage` for
    text-buffer windows are. TADS 3 still answers 0. Turning it on needs
    its own corpus run.
    Turning this on changed *Arrival*, *bbn*, *Beet*, *Ground*, *landing*
    and *Six*, which take their picture paths.
  - Report `WAV`, `MIDI`, `OGG`, `MPEG`, `MPEG1`-`3`, `WAV_MIDI_OVL`,
    `WAV_OVL` and `AUDIO_FADE` from the matching `glk_gestalt` results.
  - Report `PREF_SOUNDS`, `PREF_MUSIC` and `PREF_LINKS` from Spatterlight's
    settings.
  - Report `LINKS_*` once links work.
  - Report `MNG*` as 1 (Step 5 converts MNG to GIF).
  - ⚠️ This changes library behaviour. adv.t switches its status line to a
    `<banner>`, and adv3 picks between the banner API, `<banner>` tags and
    plain text. Run the corpus before and after.
- [x] **Resources.** Done in `glk/osres.c`.
  - TADS 2 passes `osres_t2_appctx()` to `os0main2`; its callbacks fill a
    name → (file, offset, length) table from the HTMLRES entries and the
    `.rs0`-`.rs9` files. TADS 3 does the same through `CVmHostIfcGlk`
    (`t23run.cpp`), a host interface that records the resources as the
    loader adds them. `t23run.cpp` is built with `-fno-rtti`, like the
    tads3 library, or the subclass doesn't link.
  - `osres_find` looks in the table, then the debug build's links, then
    for a loose file in the game's folder, matched case-insensitively.
    Paths with `..`, `:` or a leading `/` are refused. `resExists()` now
    works in TADS 2 as well (`osres_exists`).
  - The notes below are what the code does.
  - TADS 2: pass a real `appctx` to `os0main2` with `add_resource` and
    `add_resfile` callbacks. Keep a name → (file, offset, length) table for
    the HTMLRES entries and for each `.rs0`-`.rs9` file.
  - TADS 3: use `find_resource()`, then `osfpos` to get the offset in the
    `.t3` or `.3rN` file.
  - Register each resource lazily on first use with
    `garglk_add_resource_from_file` (`.gam`/`.t3`, by offset and length) or
    `gli_add_resource_from_path` (a `.rsN`/`.3rN` file, or a loose file next
    to the game). Scarier's `os_glk_resources.cpp` already does this.
  - The returned number is a hash, not a Blorb index
    (`glkimp/fileresource.c`). Keep it in the name table and pass it to
    `glk_image_draw` and `glk_schannel_play_ext`.
  - `garglk_add_resource_from_file` accepts only a bare name in the game's
    folder and rejects any `/`. External media in subfolders, such as
    `pics/foo.jpg`, need `gli_add_resource_from_path` with the TADS
    sandbox check (the path must stay under the game's folder) done first.
  - Some games have GIF resources (*Macrocosm*). HTML TADS never displayed
    GIF, so treat them like any other image if NSImage reads them.
  - Resource names are URL-style paths: decode `%xx`, ignore case, and
    accept `\` or `/` as the separator.
- [x] **A Glk trace harness for regression tests.** Use a recording Glk,
  like the Bocfel v6 trace suite, that logs styles, colours, images, links,
  sounds and windows. Give it a script that runs the first turns of each
  corpus game and diffs the logs against goldens. Without this, regressions
  across 235 games can't be seen.
  - Done as `test/run_traces.py`. It keeps no goldens: run it on the old
    build and the new one, then `--compare`. `--corpus xtads` runs the
    1343 xtads test games, and `--walk` types the walkthroughs in
    `htmltads/goldens`.
  - The fake app runs in determinism (testing) mode: the RNG has a fixed
    seed, and a virtual clock (`osportable.cc`) reads noon on 1 Jan 2000
    UTC and moves only when the game reads it (1 ms a read), sleeps, or
    waits on a timer. Two runs over the whole corpus and xtads give the
    same traces, so there is no list of noisy games to skip.
  - The fake app answers every `BANNERLINES` with 24, so a banner sized to
    its contents comes out 24 lines tall in the traces (*1893*'s banners
    fill the window). The real app counts the lines.

## Step 1: 11 styles chosen per game

Glk has 11 styles, and their hints apply only to windows opened after the
hints are set. HTML can combine bold, italic, underline, fixed pitch, font
face, size, superscript, alignment and indent in any way. Most of that can
be dropped without losing anything that matters:

- underline is shown as italic;
- headings become bold;
- font face, font size and superscript/subscript are ignored;
- left indents (`blockquote`, `dd`, nested lists) become leading spaces: the
  parser keeps an indent level, two spaces per step, and writes that many
  non-breaking spaces at the start of each line it starts. Glk does the
  wrapping, so only the first line of a paragraph is indented, not the lines
  it wraps onto. Right indents are ignored.

What remains is bold, italic, fixed pitch, the TADS-Input font and
centring. Colour is separate, through `garglk_set_zcolors`.

**Corpus count** (`.work/bin/stylecount.py`, which simulates the tag stack
in each string):

- With the simplifications above, no game uses more than 8
  combinations before centring.
- Counting centring, only 2 of the 235 use more than 11: *Plant* and
  *Prince Quisborne*, with 12 each.

| combination | games | | centred | games |
|---|---|---|---|---|
| bold, italic | 227 each | | plain | 144 |
| Input | 213 | | bold | 118 |
| plain | 131 | | italic | 35 |
| bold italic | 63 | | bold italic | 14 |
| fixed | 45 | | fixed (any) | 9 |
| bold Input | 10 | | right-aligned bold / plain | 8 / 2 |
| italic / bold / bold italic fixed | 7 / 5 / 1 | | | |

The styles are chosen per game, from a scan of the game file at startup.
The scan runs before the first window opens, because the hints have to be
in place by then.

### Scan the game at startup

Done in `glk/osstyles.c`. `test/check_styles.py` builds it on its own
(`test/stylescan.c`) and checks its counts against a Python copy of
`stylecount.py`'s rules on every game of both corpora; without the `\(`
escapes, that copy matches `stylecount.py` itself on all 235 games. The
scan takes at most 15 ms (*Prince Quisborne*), so nothing is cached.

- [x] **Read the game's strings straight from the file.** The scan doesn't
  depend on interpreter state. Port `t2_scan`/`t3_scan` from
  `test/.work/bin/tadsscan.py` to C in a new `glk/osstyles.c`:
  - **TADS 2:** the text properties (types 1 and 2) of the `OBJ` section,
    plus `FMTSTR`. They are XOR-encrypted when header flag `0x08` is set.
    The seed and increment come from `XSI` and default to 17/29.
  - **TADS 3:** every `CPPG` page, XOR'd with the page's mask byte. That
    covers both the constant pool and the code pool, which holds inline
    strings.
  - The scan reads the game file once and skips the HTMLRES and MRES
    media. Time it on the largest corpus games; if it's noticeable, cache
    the result by game hash.
- [x] **Follow the tags in each string**, the way `.work/bin/stylecount.py`
  does. On each run of visible text, count how often each reduced
  combination of bold, italic, fixed, Input and centred appears.
  - TADS 2's `\(`/`\)` escapes count as bold on and off.
  - Text-only games without `\H+` that use no tags give an empty count.
    They get the default table below, since all they can use is bold and
    italic through `os_set_text_attr`.
- [x] **Choose the 11 styles** from that count:
  1. Normal is always plain and Input is always Input. Line input uses
     `style_Input`, so its slot is fixed.
  2. The other combinations are ranked by count, and the 9 most used get
     a slot. Bold, italic and bold italic are ranked even with a count of
     0, since the formatter makes them without tags (`\(`, `os_set_text_attr`).
     Ties go by combination number, so the choice is stable.
  3. Each combination goes to the slot whose meaning is closest, if that
     slot is free:
     - italic → Emphasized
     - bold → Subheader
     - bold italic → Alert
     - fixed → Preformatted
     - centred bold → Header
     - centred plain → BlockQuote

     The rest fill whatever is left, in the order User1, User2, Note, then
     the unused natural slots.
  4. A combination with no slot falls back to the nearest one that has a
     slot, by dropping first bold on fixed or Input text, then italic, then
     centring, then bold. Whatever is left with no slot is Normal.
  5. A slot that nothing uses keeps the theme's look, so it gets no hints.
- [x] **Default table, when the scan finds nothing.** This happens with an
  unreadable or unknown file format and with plain text-mode games.

  | Glk style | Hints | Used for |
  |---|---|---|
  | Normal | (theme) | plain text |
  | Emphasized | italic | `i`, `em`, `u`, `cite`, … |
  | Subheader | bold | `b`, `strong`, `h1`-`h6` |
  | Alert | bold italic | bold italic |
  | Preformatted | fixed | `tt`, `pre`, `code`, a typewriter `font face` |
  | Input | (theme) | `font face=TADS-Input` |
  | BlockQuote | centred | centred plain |
  | Header | centred bold | centred bold |
  | User1 | centred italic | centred italic |
  | User2 | centred bold italic | centred bold italic |
  | Note | centred fixed | centred fixed |

  Every slot in use but Normal and Input gets the same seven hints, so a
  slot looks like its combination whatever the theme has: Weight 0/1,
  Oblique 0/1, Justification LeftFlush/Centered, Proportional 1/0 (not
  for Input combinations), Size 0, Indentation 0 and ParaIndentation 0.

**Result on the corpus:**

- With the default table, 19 games lose a combination: bold Input,
  bold/italic fixed, or centred fixed variants.
- With the per-game choice, only *Plant* does, for a single passage of
  centred bold fixed text. Counting `\(`/`\)` as bold (`stylecount.py`
  doesn't), *Chancel of Death* does too: it gains bold fixed and bold
  italic fixed, which push out centred bold and centred italic. No xtads
  game loses one.
- Right-aligned text still shows centred, since it's folded into
  centring.

**Conditions:**

- Strings put together at runtime, such as `'<font size=' + n + '>'`, or
  tags opened in one string and closed in another, can lead to a
  combination the scan didn't see. That falls back to the nearest slot as
  in item 4, never to an error.
- The choice depends only on the game file, so it's the same on every
  run. Autorestore needs nothing new: the hints are already autosaved.
- A slot keeps the theme's look for every hint the game doesn't set. In
  Spatterlight that includes a theme's own Note or User1 font, size or
  colour. To keep a slot from looking unlike its combination, any slot the
  table uses also sets `Size 0`, `Proportional` and `Indentation 0`
  (and `ParaIndentation 0`). Colour and font are left to the theme.

All of this is plain Glk, so it works the same on Gargoyle.

- [x] **Hints.** Set for `wintype_TextBuffer` in `os_init`, before the
  main window opens; `glk_main` (`t23run.cpp`) runs the scan just before.
  Hints set only for `wintype_AllTypes` would also centre text in grid
  windows, so they aren't.
  - Banners use the same table, and their colours are zcolors (see the
    Banners item below).
  - The main window's bold and italic go through the table
    (`osstyles_style`), which gives the same three styles as before, so
    the only change in the traces is the hints.
- [x] **HTML parser state.** A tag stack (`oshtml.c` `elem`) holding:
  - bold, italic (also set by underline), fixed, Input
  - alignment: left, or centre (right counts as centre)
  - colours, indent, list kind and item count

  An end tag restores the state from before its start tag, and closes the
  tags left open inside it. On each text run, `osstyles_style` looks up
  the combination's slot in the per-game table from the scan.
  - Clearing the main window closes every open tag, as HTML TADS's
    `CHtmlParser::clear_page` does. *1893* leaves a `<div align=center>`
    open, which otherwise centred the rest of the game.
- [x] **Centring is per paragraph.** `<center>`, `<p align>`, `<div
  align>` and `<hN align>` are blocks: each starts a new line, and its
  lines get the centred combination. HTML TADS does the same, because
  alignment is a block property (`CHtmlTagCENTER` is a `<div>`).
  - An unclosed `<center>` centres everything after it, including the
    prompt and the input, as in HTML TADS (*School*).
- [x] **Text mode, for the main window.** `os_set_text_attr` goes through
  the parser (`oshtml_set_os_attr`), so the formatter's attributes join
  the tag bits, and `OS_ATTR_STRONG`, `OS_ATTR_EM` and `OS_ATTR_HILITE`
  are no longer 0.
  - [x] Banners: API banners (`osglkban.c`) now use the per-game table, as
    tag banners do. `os_banner_set_attr` and the HTML parser's attributes
    go through `osstyles_style`, so banner text can be bold, centred or
    fixed and coloured at once.
    - Colours are zcolors on each stored piece of text (`banner_zcolor`).
      A parameter colour is left to the theme where the theme draws that
      window; in a status banner or after `os_banner_set_screen_color` it
      is the measured colour. The screen colour is the window's background
      (`win_setbgnd`).
    - The hints stay set. A grid banner gets the bold and italic hints
      while it opens (`grid_hints_set`), and a tab-aligned (status) text
      banner gets `Proportional 0` on every slot (`fixed_hints_set`).
      `os_banner_styles_reset` is gone, and a colour change no longer
      closes and reopens every banner.
    - Traces: no change to any text, in the walkthroughs or xtads. Styles
      changed in 11 games. *When__When*'s map banner is in `<tt>` and is
      now fixed-width, so the map lines up. *Macrocosm* and *mazes* get
      their centred styles instead of the colour slots.
- [x] **Is the centred style centred?** `glk_style_distinguish` doesn't
  answer this (glkimp always says no), so the parser asks
  `glk_style_measure(stylehint_Justification)` once per combination
  (`main_centers`, cached until the windows are rearranged). If the style
  isn't centred, the parser pads the line with spaces to the window width,
  the way `vmconhmp.cpp` (~line 705) pads `<tab align=center>` in text
  mode.
  - glkimp passes the measure to the app for buffer windows (grid
    windows are always LeftFlush), and the app reports the style's
    paragraph alignment (`handleStyleMeasureOnWin`, tested in
    `StyleMeasureTests`). With the theme's "use game styles" switch off,
    the hints don't apply, so the measure says LeftFlush and lines are
    padded.
  - `glkdrive.py` answers the measure from the hints the game set, as the
    app does, so the traces show the centred styles too.
- [x] **Theme switch.** With the theme's "use game styles" switch
  (`doStyles`) off, Spatterlight ignores the hints, and the 11 styles look
  however the theme has them. That's the user's choice and needs no code.

**Later, maybe: a text-attribute extension.** A per-stream overlay that works
like `garglk_set_zcolors` (`spatterlight_set_textattr(attr, value)`, detected
through a gestalt selector) could add real underline, font size, font family
and superscript, and win back the 19 games' lost combinations. It would leave
the style table and the hints alone, so it stays within the spec. For now the
corpus doesn't justify the work in glkimp, the app protocol, theme
re-resolution and autosave.

## Step 2: Text formatting (nearly every game)

| Tag / attribute | Games | Plan |
|---|---|---|
| `b`, `i`, `em`, `strong`, `u`, `s`/`strike`, `tt`, `code`, `big`, `small` | 228, 228, 34, 16, 32, 4, 32, 3, 10, 1 | Bits in the tag stack (Step 1). `em`, `u` → italic; `strong` → bold; `tt`, `code` → fixed. `s`, `big`, `small` are ignored. |
| `font size` | 106 | Ignored. |
| `font face` | 63 (plus `TADS-Input` in most games) | `TADS-Input` → `style_Input`. `TADS-Typewriter` and Courier-like names → fixed. Everything else is ignored. |
| `font color`, `bgcolor` | 85 | `garglk_set_zcolors`, already per span. Add the HTML colour names and `#rrggbb`. TADS's parameterised colours (`text`, `bgcolor`, `statustext`, `statusbg`, `link`, `hlink`, `alink`, `input`) become theme colours or `zcolor_Default`. Keep a colour stack so `</font>` restores the previous colour. |
| `body bgcolor`, `text` | 125 (outside the status line) | ✅ The window background via `win_setbgnd` on the main window, and the default foreground for that window, which outlasts clearing it. A `<body>` without `text` against a literal `bgcolor` gets black or white text by the background's brightness. HTML TADS resets whatever `<body>` omits to the defaults. ✅ `os_set_text_color` no longer passes the TADS status mode as a window id (`curwin`), which repainted the whole window with each span's background. |
| `h1`-`h4` (+`align`) | 31, 29, 55, 6 | Bold with blank lines around. `align=center` → centred bold (`style_Header`). |
| `center`, `p align`, `div align` | 159, 11, 8 | The centred styles in Step 1; `align=right` counts as centred. |
| `blockquote`, `bq` | 24, 5 | A blank line around, and one indent level (leading spaces, see Step 1). Not `style_BlockQuote`, which is used for centring. |
| `ul`, `ol`, `li`, `lh` | 17, 8, 21, 2 | Paragraphs that start with "• " or "n. ", generated by the parser, after the indent of the list's level (leading spaces, see Step 1). |
| `pre` | 5 | `style_Preformatted`; keep whitespace (TADS 2 already tracks it). |
| `hr` (`size`, `width`) | 26 | A rule of `─` characters across the window; Glk has no horizontal rule. |
| `sup`, `sub` | 31, 1 | Ignored. Most of the 31 come from the adv3 library. |
| `q` | 102 | Curly quotes, alternating when nested. Both engines already do this; keep it. |
| `br clear`, `br height` | 5, 6 | `height` as now. `clear` → `glk_window_flow_break` (see images). |
| `nobr`, `wrap`, `address`, `basefont` | 1, -, 2, 1 | `nobr`: non-breaking spaces. `address`: italic. Ignore the rest. |
| `tab` | 218 (mostly the status line) | In grid windows, port the TADS 3 tab-stop code: `align=right/center`, `to`/`id`, `indent`, `multiple` (4 games). In buffer windows, use spaces or a tab character; true tab stops are not available. |
| `title` | 78 | `garglk_set_story_title`, as now. |
| `aboutbox` | 93 | Suppress the contents, as now. Possible extra: keep the text and show it in Spatterlight's game info panel. |

**Status.** Everything in the table is in `oshtml.c`, except:

- [ ] `<ol type>` and `start`: every list is numbered 1, 2, 3. The
  corpus's few `<ol>` are all plain.
- [x] `br clear`: `glk_window_flow_break`, with Step 5.
- [x] `tab` in grid windows: done with the status line in Step 3.
- [x] Banners' own text formatting: tag banners use the same parser.
- [x] The `systemInfo` change (TADS 2), with Step 3's tag banners.

Notes from the corpus traces:

- Text that HTML TADS centres is now centred: titles, *When*'s time
  headings and *Lydia's Heart*'s credits. *journey*'s second visit to the
  mountaintop puts `<TAB MULTIPLE=4>` before a `<center><img>` block, so
  its tab is on the image's line, not the paragraph's, as in HTML TADS.
- `<P>` no longer stacks: `text<P><P>` leaves one blank line, not two
  (*vegrevil*, *moonbase*, *HELP10*).
- An explicit `<P>` in HTML TADS is half a line of space, and the implicit
  break of a block (`<div>`, `<center>`, a list) is none, apart from the
  block's own margins. Both are a whole blank line or a plain line break
  here.

## Step 3: Banners (59 games with custom banners; the status line in almost all)

- [x] **Tag banners.** `<banner id=… height=… width=… align=… border remove
  removeall>` maps onto the TADS 3 banner machinery already in `osglkban.c`:
  - one Glk window per id
  - `align` top/bottom/left/right → `winmethod_Above`/`Below`/`Left`/`Right`
  - `n%` → proportional
  - `height=previous` → keep the current size
  - no height → size to contents with `win_lines`
  - pixel sizes → exact. A graphics window is opened at that many pixels
    (a fixed split keyed by a graphics window is measured in pixels) and
    split by the banner's own window at 100%, so the graphics part is 0
    wide. Without graphics, 8×16 pixels a cell (`TAG_PIX_COL`,
    `TAG_PIX_LINE`). SD3's right banner is now 120 px, pill's bottom one
    66 px. (The banner API has no pixel unit: `OS_BANNER_SIZE_ABS` is
    character cells in a text banner.)

  Re-opening a banner id **replaces** its contents. This fixes TADS 2's
  current fault of printing status-line contents inline in the main text.
- [x] **A clear screen removes the tag banners** (`oscls`), as HTML TADS's
  `start_new_page` does (`remove_all_banners`; API banners stay). Without
  this, a full-height title or menu banner (*1893*'s `middle` at 100%, the
  menu libraries of *Arrival*, *HOLD*, *School*, *Ground*, *ecdysis*,
  *futzmutz*) stayed over the main window for good: the main window had no
  height, so the game seemed to ignore Enter.
- [x] **The status line** (`id=StatusLine`, `<tab align=right>`) goes to a
  grid window, with `body bgcolor=statusbg text=statustext` as theme status
  colours.
- [x] `border` → `winmethod_Border` vs `NoBorder`.
- [x] Ids seen in the corpus include `MenuTitle`/`MenuBody` (menu libraries),
  `credits`, `intro`, `left`/`right`/`middle`, `convmenu` and `story`.
  These are text-buffer banners that need styles, links and images like the
  main window.
- [x] `os_banner_set_screen_color` is no longer overwritten on reopen
  (`bgscreen` in `osglkban.c`). The banner colours no longer come from
  glkimp's `gsfgcol`/`gsbgcol`, which made every banner a reversed grid, so
  that *Thaumistry*'s whole hint screen was grey. They come from
  `measure_colors` (`osglk.c`), which takes style measures of the main
  window and a grid. It runs at start and on every redraw, so a theme change
  carries over. A status banner is a reversed grid (fg = grid bg,
  bg = grid fg), as the text-mode status line is; any other banner has the
  main window's colours. The harness answers every measure with 0, so its
  traces show colour 0. Check colours in the app.

Notes from the corpus traces:

- *Six* (`six.gam`) hangs in HTML mode if a one-letter word (`n`, `e`) is
  typed at the death prompt. The game's own loop (decompiled with
  FictitiousFrode's `tads2.pl`) reuses the variable holding the answer's
  length, and with `systemInfo` reporting HTML it never reads input again.
  HTML TADS would hang the same way: it is the game's bug. A loop guard
  (`os_break`, `osmisc.c`) now stops a TADS 2 game that runs 100 million
  instructions without input or output, about five seconds, says so, and
  undoes the turn.
- Before HTML is switched on, adv.t opens the text-mode status line, which
  is closed again when the `StatusLine` banner first appears.

## Step 4: Hyperlinks (`<a>`, 91 games)

Both engines already handle links: they expect an `OS_EVT_HREF` event
(`tads2/osifc.h`) with the href text in `info->href`. T2 handles it in
`bif.c` (`inputevent()`, ~3636) and T3 in `vmbiftio.cpp` (`inputEvent()`,
~470). The missing part is the Glk side in `osglk.c`, which returns only
`OS_EVT_KEY` and `OS_EVT_TIMEOUT` today.

**Status.** Links work in the main window, tag banners, TADS 3 API banners
and the status line, in both engines (`osglk_link_add`, `line_link` and
`getglkchar` in `osglk.c`; the `<a>` tag in `oshtml.c`). API banners keep the
link in each content chunk, so a redraw restores it
(`html_set_link`/`osglk_api_banner_request_links` in `osglkban.c`). In the corpus traces 32 of the 235 games, and
32 of the 1343 xtads games, now set links; no other trace line changed.
`test/run_traces.py` can click them: a script line `link:N` or `click:TEXT`
(`bocfel/test/v6trace.py`). *Golden Skull*'s "north" link types `north`.

- [x] `<a href=…>` → `glk_set_hyperlink(n)`. The link `n` is a field of the
  parser's colour state, so it follows the tag stack and `</a>` restores it.
  - One table for all windows, not one per window: a repeated href (and
    option set) gets the same `n`, so a status line redrawn every turn adds
    nothing. It is never cleared; it stops growing at 8192 links, and the
    later ones are plain text.
  - `glk_request_hyperlink_event` goes to the banners and then the main
    window before every line or key wait, and again after each click we
    don't end the wait with. (The window asked last is the one a harness's
    `link:N` clicks in.)
  - `<a name=…>` has no href and so makes no link. Without `gestalt_Hyperlinks` and `gestalt_HyperlinkInput`
    every link is plain text.
  - The OS layer's own text (echoed input, the MORE prompt) is never part of
    a link: `oshtml_input` and `oshtml_put_plain` end it.
  - `SYSINFO_PREF_LINKS` is 1 when links work, in both engines. Making TADS
    3 answer HTML changed 29 of the 235 corpus traces, all TADS 3 games:
    - the status line gained links (room name, score);
    - `<.statusscore>` is now italic;
    - *When* opens its banners as text buffers, not grids;
    - *mazes.t3* no longer gets stuck and plays its whole script.

    In xtads, 7 of the 1343 traces changed outside the colour hints.
- [x] **Command links.** `line_link` cancels the line request (with echo off,
  so the typed text isn't shown), then:
  - ordinary link: the href is the line. It is echoed in `style_Input` with a
    newline, as if typed;
  - `noenter`: the line is requested again with the href as its start;
  - `append`: the same, after the text typed so far.
  - A click on a link we don't know, or with no line request, is ignored.
- [x] **Every wait loop handles `evtype_Hyperlink`**: `os_gets`,
  `os_gets_timeout` and `getglkchar` (so `os_getc`, `os_waitc`, the MORE
  prompt and `os_get_event`). In a key wait a click counts as a space.
  `os_get_event` returns `OS_EVT_HREF`, with the href cut to 255 bytes.
  Not tried on a game that runs its own event loop.
- [x] Image-map links in Step 5 use the same table and event path.
- [ ] **URL links** (52 games: `news:`, `mailto:`, `http:`, mostly in about
  text). For now they are plain text (`is_url`). Open them through the app,
  behind the user's link setting, and report `LINKS_HTTP`, `LINKS_MAILTO`
  and `LINKS_NEWS` accordingly.
- [x] `plain` (26 games) is ignored, so the link is an ordinary one. In HTML
  TADS `plain` only drops the link's highlighting. Glk has no link without
  its look, so these links look like the others. They were once left out,
  but adv3 writes its menu entries (`menucon.t`, as in *Prom Dress*'s hints)
  and the status line's exit list with `<a plain href>`. Clickable links
  changed 33 corpus traces (walk endings unchanged) and 10 xtads ones.
- [x] `title`, `target`: ignored. (`hidden` too: that link still works.)

## Step 5: Images (`<img>`, 119 games; JPEG 76, PNG 37, MNG 2)

**Status.** `<img>` and `<br clear>` are done (`put_image` in `oshtml.c`).
In the corpus traces, 63 games now load and draw pictures: 158 resources
loaded and 625 images drawn over the short script. Among them are *1893*
(inline and in a banner), *Arrival* (from `arrival.rs0`; "x fridge" draws
its picture), the loose files of *Ecdysis* and *SDA*, and *Threnody*'s menu
pointer, which used to be `[X]`. The xtads traces are unchanged.

- [x] `src` → resource lookup (Step 0) → `glk_image_draw` /
  `glk_image_draw_scaled` in the current text-buffer window, which is the
  main window or a banner.
  - A missing dimension is scaled from `glk_image_get_info`, keeping the
    image's aspect.
  - An inline image starts a line like text does: a paragraph's pending
    newline comes before it, so *1893*'s title has one more blank line.
  - Not checked yet: *Drool* (pictures in later rooms).
  - TADS 3 is told about JPEG and PNG as well. Six games in the corpus
    now draw pictures: *Edge of Chaos* (its title and start screens),
    *According to Cain* (which now asks "Do you want graphics?"),
    *homeopen*, *Macrocosm*, *mazes* and *Prince Quisborne* (whose left
    banner, sized to its picture, grows from 26 to 202 px in the
    harness).
  - `align=left`/`right` (38 games) → `imagealign_MarginLeft`/`Right`.
  - `align=top`/`middle`/`bottom` or none → `InlineUp`/`InlineCenter`/
    `InlineDown`.
  - `width`/`height` (16 games) → scaled drawing. A percentage scales
    against the window size, using `glk_image_draw_scaled_ext` the way
    Question does.
  - `hspace`/`vspace`/`border`: ignore at first. Most values are 0.
- [x] `br clear=left/right/all` → `glk_window_flow_break`.
- [x] `alt` (16 games): print it when the image is missing or images are
  turned off. This is what the text mode does today.
- [x] Images in banners: the same code, because tag banners are text-buffer
  windows. A banner from the TADS 3 banner API keeps its contents to
  write them again when it's redrawn, so the parser hands it the picture
  (the sink's `image`), which it keeps as a chunk of those contents and
  draws with `oshtml_draw_image`. Grid banners and the hidden `measure`
  banner print the `alt` text instead.
  - A centred `<TAB>` before a picture pads it as if the picture were one
    character wide (*Edge of Chaos*, *1893*).
  - [ ] A banner without a height must size to its contents, image
    included: use `glk_image_get_info` for the image height and convert it
    to lines with `glk_style_measure`.
- [x] Percentage sizes are computed once, when the image is drawn, with
  `glk_image_draw_scaled_ext` and `imagerule_WidthRatio` (when
  `gestalt_DrawImageScale` is there). Glk can't rescale an image that's
  already drawn when the window is resized, so accept that rather than
  redrawing.
- A fix in `tads2/output.c` came with this: a string that ends inside a tag
  name (*Arrival* prints `<FONT` and its attributes separately) wrote a NUL
  into the line buffer. That cut the status-line banner short, its
  `</BANNER>` never came, and every later reply went into the banner.
- [x] `usemap` with `<map>`/`<area>` (8 games, among them Arrival, Golden
  Skull, Helpfulman and Six Stories). Glk can't make parts of an image
  clickable. Instead, the areas are printed as a row of command links under
  the image, separated by ` · `; their hrefs are commands such as
  `goto bridge`, `look` or `save`.
  - In the corpus, the `<map>` always comes before its `<img>`, and no
    `<area>` has ALT text, so the label is the href with its spaces trimmed.
  - Maps are kept by name for the whole game (32 of them, recycled oldest
    first), as a game may define a map once and use it in many banners.
    `usemap` is matched with or without its `#`, ignoring case.
  - Areas with `nohref`, URL hrefs or a repeated href are left out.
  - A picture may be one tile of several: *Golden Skull*'s compass is nine,
    each with a map of its own, in a table. So the row waits until the
    pictures end, and is printed before the next visible text, paragraph
    break or input, at the end of a TADS 2 banner, or at `\H-`. A plain
    `<BR>` or the formatter's own flush doesn't print it. Each link is in
    the row once. Clearing the window drops it.
  - TADS 3 banners would print the row only at their next text, as they
    have no input of their own. No TADS 3 game in the corpus uses maps.
  - The row is shown when the picture can't be drawn and its ALT text is
    printed instead, as the links are still useful.
  - Checked in the traces: *ruinedRobots* gets "sit on chair · examine
    mirror · turn on table lamp · put hand in fire · west", and *Six
    Stories* gets its intro's choice, "Continue with Six Stories. · Skip the
    introduction." *Golden Skull*'s `examine compass` (by a `--script`)
    gives "press power button · northwest · north · … · southeast".
    *Arrival*, *School* and *Helpfulman* build theirs only after particular
    commands, which haven't been tried.
- A fix in the banners came with this: `tag_put` holds a banner's newlines
  back until more text comes, so its last line isn't blank, but a picture
  went past them. The rows of pictures in *1893*'s room banner came out on
  one line, and *Six*'s intro picture went on the line of the text before
  it. `window()` now puts out the held newlines first (`tag_window`).
- [ ] `body background` (14 games): a tiled background image. Skip it, or
  later draw it as the window background through a small extension.
- [ ] **MNG → animated GIF, in the app.** Spatterlight already animates GIFs
  in buffer windows (`Images/ImageAnimation.m`), so convert MNG data when
  it's loaded, in `ImageHandler`, wherever the data starts with the MNG
  signature. Terps don't change, and the image keeps its resource number.
  - **Which games:** *Macrocosm* has 4 MNGs (`zDAT_025/030/035/049.mng`)
    and *Antiquest* has 1 (`res/images/intro.mng`). *White Rose* names an
    `.mng`, but it is the "light" text-only build of *British Fox and the
    Nationalist Conspiracy* and has no media.
  - **The files are simple.** There is no delta coding and no JNG:
    - *Macrocosm*: 34–157 full 400×434 RGBA PNG frames, each followed by a
      `FRAM` with the frame delay in ticks at 100 ticks/s, and a `TERM`
      that plays the sequence once and stays on the last frame.
    - *Antiquest*: 12 paletted PNGs with `tRNS`, each positioned by a
      `DEFI` on a 600×200 canvas, played in a `LOOP`/`ENDL` with `SHOW` at
      3 ticks/s.
  - **The converter:**
    1. Walk the chunks. Each `IHDR`…`IEND` run, with the PNG signature
       added, is a standalone PNG.
    2. Composite it at the `DEFI` offset onto an `MHDR`-sized canvas.
    3. Take the frame delay from `FRAM` and the loop count from
       `TERM`/`LOOP`.
    4. Write the result with `CGImageDestination` as a GIF
       (`kCGImagePropertyGIFDelayTime`, `kCGImagePropertyGIFLoopCount`).

    `SHOW` ranges and the other `FRAM` modes can wait until a game needs
    them.
  - **The prototype works.** It was a Swift script, not in the repo.
    - All 5 files convert in 0.04–0.42 s.
    - The resulting GIFs are 19–470 KB; the MNGs are 0.6–7 MB.
    - `NSImageFrameCount` sees every frame, so `ImageAnimation` plays them
      as they are.
    - Frames compared by eye match the lossless APNG output. The frames are
      opaque on black, so GIF's 1-bit alpha loses nothing here.
    - APNG works through the same path, if a later game needs real alpha.
  - **The terp:** report `SYSINFO_MNG`, `SYSINFO_MNG_TRANS` and
    `SYSINFO_MNG_ALPHA` as 1 in the Spatterlight build. Glk has no gestalt
    for this, so it's a build-time answer.
  - **Without an animation** (images off, or the conversion fails), show
    the first frame, or the `alt` text.

## Step 6: Sound (`<sound>`, 53 games; WAV 26, MP3 22, MIDI 9, Ogg 4)

Spatterlight already plays all four formats.

Done in `glk/ossound.c`. It follows the queue rules of HTML TADS's
`htmlsnd.cpp` and `htmltags.cpp` (`CHtmlSoundQueue`, `CHtmlTagSOUND`) and
QTads's `qtadssound.cc`. `oshtml.c` parses the tag into an `ossound_tag`.

- [x] **Four layers** (`foreground`, `ambient`, `bgambient`,
  `background`), each with its own queue and two Glk channels. The channels
  are made lazily with `glk_schannel_create_ext`. One channel plays the
  queue; the other is for a sound fading out in the background.
  - A sound with no `layer` goes where HTML TADS puts it: MIDI to
    background, and WAV to foreground, or to ambient with `random`. An MP3
    or Ogg with no layer is an error, and its `alt` text is shown.
- [x] `src`, `layer` (45 games), `cancel[=layer]` (21), `repeat=n|loop` (11)
  → `glk_schannel_play_ext` with a repeat count (`loop` is `0xFFFFFFFF`),
  and `glk_schannel_stop`.
- [x] **Queueing.**
  - A sound starts when nothing ahead of it in its layer is playing.
  - `interrupt` (11 games) cancels the layer first.
  - `random=n` (4 games): each time the sound comes up in the queue, it
    plays with an n% chance. While its layer is idle, it gets another chance
    once a second. An ambient sound with `random` and no `repeat` loops.
  - `sequence=replace|random|cycle` (no game in the corpus uses it).
    `cycle` is the documented round robin; HTML TADS itself tests a LOOP
    sequence that no tag sets.
- [x] **Every wait handles the notify events.** `osglk.c` `select_event` is
  used by `os_gets`, `os_gets_timeout` and `getglkchar`, which covers
  waitkey, the MORE prompt and timed input. It gives `evtype_SoundNotify` and
  `evtype_VolumeNotify` to `ossound_event` and goes on waiting.
  - While a `random` sound waits and the game has no timer of its own, the
    wait runs a 1 s Glk timer for it. The fake app never fires it, so the
    traces only show `TIMER 1000`/`TIMER 0` around input.
- [x] `volume` (5 games) and `fadein`/`fadeout` (3), including
  `crossfade,n` → `glk_schannel_set_volume_ext` with a duration. Report
  `AUDIO_FADE` and `AUDIO_CROSSFADE` for all four formats.
- [x] `alt` is printed when the sound can't play: sound off, no resource, or
  no window to play for.
- [x] **Without `gestalt_Sound2`**, `SYSINFO_WAV`/`MIDI`/`MPEG*`/`OGG`/
  `PREF_SOUNDS`/`PREF_MUSIC` and the fade answers are 0. As with images,
  these were TADS 2 only at first; TADS 3 gets them too now. In the
  corpus, *According to Cain* and *Under the Cognomen of Edgar Allan Poe*
  then ask "Do you want music?", and *Verla*'s status line gets its
  SONIDO link.
- [x] **Corpus check.** 16 games changed traces, and in all of them the only
  changes are sound calls and the timer arming. The one exception is *Six
  Stories*: it sees sound support and takes its sound path, which skips
  the "this game has sound" notice and plays the theme. The walkthroughs end
  as before.
- **Glk can't do these, because they need the sound's length.** The sounds
  end without the fade:
  - a fade-out at the natural end of a sound;
  - the cross-fade into a queued sound (`fadein=crossfade` without
    `interrupt`).
- **Autosave: does not apply yet.** TADS has no autosave in Spatterlight.
  If it gets one, the layer queues must be saved with it, or the sounds
  rebuilt after a restore, the way Scarier does it.

## Step 7: Tables (`<table>`, 92 games)

Glk has no tables, and many of these games use tables for layout: `border=0
width=100%`, side-by-side image and text, menus, and score sheets.

- [x] **First pass: linearise.** Done in `oshtml.c`. All the text stays
  readable, in reading order.
  - `<table>` is a block. Each row is a line, broken at `<tr>`, which
    closes a row or cell left open; so does `<td>` with the cell before it.
    `<td>`/`<th>` outside a table are ignored, as before.
  - Cells are separated by a gap of three spaces, which takes the place of
    any spaces the cell before ends with. `th` is bold.
  - **Empty cells before a cell** (the menu library's `<td width=x> </td>`,
    in TADS 2 and adv3 `MenuBody`) turn the gap into an indent of four
    spaces for every line of the cell, through the parser's left indent.
    "Empty" means the row has shown nothing yet: no text, image or newline,
    so a cell after an `<hr>` cell is not indented (*Rematch*).
  - **`align`.** On a row or on its first cell, `center` (and on the first
    cell, `right`) centres the line, and `left` stops the centring. The `align` of later cells is ignored,
    except that `right` is a right `<TAB>` to the end of the line where a
    `<TAB>` can align right (TADS 3, and TADS 2 banners). That puts *Arrival*'s
    score at the right of its status line. `align` on `<table>` is ignored.
  - Dropped: `cellpadding`, `cellspacing`, `width`, `height`, `valign`,
    `colspan`, `rowspan`, `border` and `background`.
  - A cell's, row's or table's `bgcolor` (*Beet*, *Darksong*)
    is the zcolors background of the text, and the text gets black or white
    on it, as with `<body bgcolor>`. A theme colour (`bgcolor='bgcolor'`,
    `statusbg`) is ignored, since the window shows it already.
- [x] **Corpus check.** 41 games changed traces, all of them games with
  tables, and no err files changed. The walkthroughs end as before.
  - The verb lists of *Barons_plot*, *camp*, *CotD*, *HorizonsDC*, the
    *SD* games and others have a line per row and a centred bold heading.
  - The menus of *Ground*, *HOLD*, *School* and *1893* have a line per
    item, indented.
  - *Arrival*'s status line shows its compass rose as three lines.
  - Accepted: rows or cells that start with `<br>` or `\b` now give more
    blank lines than before. *Threnody*'s status line is one line taller,
    because its turn count and its "Turns" label are in different rows.
  - In TADS 3, a row of only empty cells can print a blank line, because of
    the formatter's rule about the spaces of a tab.
- [ ] **Possible later work: real tables.** AppKit has `NSTextTable` and
  `NSTextTableBlock` in paragraph styles, and a Spatterlight extension
  (begin table, cell, end table) could pass the structure through. It only
  makes sense if the linearised output turns out to break games that matter,
  such as 1893, Arrival, According to Cain and Drool.

## Not worth doing (or not possible in Glk)

- Pixel-exact layout: `hspace`/`vspace`, cell padding, banner borders and
  table widths in pixels beyond the conversion to character cells (tag
  banners' pixel sizes are exact).
- `<body background>` tiling (until an extension
  exists), and image maps beyond the link-row fallback.
- `<log>`/`<nolog>` (scripting already goes through Glk transcripts).
- Font size, font face, superscript/subscript, underline, strikethrough,
  right indents, and indenting the wrapped lines of a paragraph, unless the
  text-attribute extension in Step 1 is built after all.

## Suggested order

1. Step 0, without the systemInfo change yet, so the output should stay
   byte-identical: the parser reproduces today's text mode. Diff the traces
   over the corpus.
2. Step 1, the startup scan and the per-game style table. Check that the C
   scan finds the same combinations as `stylecount.py` on the corpus.
3. Step 2, text formatting, plus the systemInfo change. Diff the traces and
   check the TADS 2 status line in a dozen games.
4. Step 3, banners.
5. Step 4, links, then Step 5, images. Image maps need the link table from
   Step 4. Both steps are visible wins for the games with media.
6. Step 6, sound.
7. Step 7, tables, linearised.

Games to check at every step, because they use most features: *1893: A
World's Fair Mystery* (`1893.gam`), *Arrival* (`arrival.gam`, the rsN build),
*According to Cain* (`cain.t3`), *Drool* (`drool_bundled.gam`, with external
images), *Golden Skull* (`gold.gam`), *Adventures of Helpfulman*
(`help_bundled.gam`), *Fusillade* (`fusillade_bundled.gam`, MIDI) and *How
Prince Quisborne the Feckless Shook His Title* (`PrinceQuisborne.t3`, fades).
