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
  `appctx`, so the HTMLRES entries and the `.rsN` files are skipped.
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

- [ ] **Get the raw markup out of both engines.**
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
- [ ] **A shared markup parser, `glk/oshtml.c`.** It takes the byte stream
  from `os_printz` and `os_banner_disp` and produces Glk calls.
  - Tokenise tags, attributes and entities, which can be split across print
    calls.
  - Keep a tag stack (`<b><i></b>` must be tolerated) and a style state.
  - TADS 3 currently handles `<tab>` (all of it), `<wrap>` and `<log>` itself.
    Once markup passes through, the new parser must reproduce whatever of
    these the games need. The `<tab>` logic in `vmconhmp.cpp:609-923` can be
    ported for grid windows.
- [ ] **Entities as Unicode for both engines.** Route TADS 2 through the same
  entity table as TADS 3 and output with `glk_put_char_uni`. This fixes the
  ASCII-only entities in TADS 2 even in text mode.
- [ ] **Answer `systemInfo` honestly.**
  - Set `SYSINFO_HTML=1` and `INTERP_CLASS=ICLASS_HTML`.
  - Report `JPEG`, `PNG`, `PNG_TRANS`, `PNG_ALPHA`, `WAV`, `MIDI`, `OGG`,
    `MPEG`, `MPEG1`-`3`, `WAV_MIDI_OVL`, `WAV_OVL` and `AUDIO_FADE` from the
    matching `glk_gestalt` results.
  - Report `PREF_IMAGES`, `PREF_SOUNDS`, `PREF_MUSIC` and `PREF_LINKS` from
    Spatterlight's settings.
  - Report `LINKS_*` once links work.
  - Report `MNG*` as 1 (Step 5 converts MNG to GIF).
  - ⚠️ This changes library behaviour. adv.t switches its status line to a
    `<banner>`, and adv3 picks between the banner API, `<banner>` tags and
    plain text. Run the corpus before and after.
- [ ] **Resources.**
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
- [ ] **A Glk trace harness for regression tests.** Use a recording Glk,
  like the Bocfel v6 trace suite, that logs styles, colours, images, links,
  sounds and windows. Give it a script that runs the first turns of each
  corpus game and diffs the logs against goldens. Without this, regressions
  across 235 games can't be seen.

## Step 1: 11 styles chosen per game

Glk has 11 styles, and their hints apply only to windows opened after the
hints are set. HTML can combine bold, italic, underline, fixed pitch, font
face, size, superscript, alignment and indent in any way. Most of that can
be dropped without losing anything that matters:

- underline is shown as italic;
- headings become bold;
- indent, font face, font size and superscript/subscript are ignored.

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

- [ ] **Read the game's strings straight from the file.** The scan doesn't
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
- [ ] **Follow the tags in each string**, the way `.work/bin/stylecount.py`
  does. On each run of visible text, count how often each reduced
  combination of bold, italic, fixed, Input and centred appears.
  - TADS 2's `\(`/`\)` escapes count as bold on and off.
  - Text-only games without `\H+` that use no tags give an empty count.
    They get the default table below, since all they can use is bold and
    italic through `os_set_text_attr`.
- [ ] **Choose the 11 styles** from that count:
  1. Normal is always plain and Input is always Input. Line input uses
     `style_Input`, so its slot is fixed.
  2. The other combinations are ranked by count, and the 9 most used get
     a slot.
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
     centring.
  5. A slot that nothing uses keeps the theme's look, so it gets no hints.
- [ ] **Default table, when the scan finds nothing.** This happens with an
  unreadable or unknown file format and with plain text-mode games.

  | Glk style | Hints | Used for |
  |---|---|---|
  | Normal | (theme) | plain text |
  | Emphasized | Oblique | `i`, `em`, `u`, `cite`, … |
  | Subheader | Weight 1 | `b`, `strong`, `h1`-`h6` |
  | Alert | Weight 1, Oblique | bold italic |
  | Preformatted | (theme) | `tt`, `pre`, `code`, a typewriter `font face` |
  | Input | (theme) | `font face=TADS-Input` |
  | BlockQuote | Justification Centered | centred plain |
  | Header | Justification Centered, Weight 1 | centred bold |
  | User1 | Justification Centered, Oblique | centred italic |
  | User2 | Justification Centered, Weight 1, Oblique | centred bold italic |
  | Note | Justification Centered, Proportional 0 | centred fixed |

**Result on the corpus:**

- With the default table, 19 games lose a combination: bold Input,
  bold/italic fixed, or centred fixed variants.
- With the per-game choice, only *Plant* does, for a single passage of
  centred bold fixed text.
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
  table uses also sets `Size 0`, `Proportional` and `Indentation 0`.
  Colour is left to the theme.

All of this is plain Glk, so it works the same on Gargoyle.

- [ ] **Hints.** Set them for `wintype_TextBuffer` before the main window
  opens, and set them again before each text banner opens. Hints set only
  for `wintype_AllTypes` would also centre text in grid windows, so don't
  use that.
  - Leave every hint the table doesn't name at the theme's value. Weight,
    Oblique and Justification are the only hints the game sets, so the
    user's fonts and sizes are kept.
- [ ] **HTML parser state.** A tag stack holding:
  - bold, italic (also set by underline), fixed, Input
  - alignment: left, or centre (right counts as centre)
  - the colour stack

  On each text run, look up the state's slot in the per-game table from
  the scan.
- [ ] **Centring is per paragraph.** `<center>`, `<p align>`, `<div
  align>` and `<hN align>` switch the style at the start of a paragraph.
  A change in the middle of a line forces a line break first; HTML TADS
  does the same, because alignment is a block property.
- [ ] **Text mode as well.** `os_set_text_attr` (`osglk.c:404`) and
  `os_banner_set_attr` go through the same mapping. That makes `<strong>`
  bold (`OS_ATTR_STRONG` is 0 today) and fixes the banner bug where text
  can't be bold and coloured at once (`os_banner_set_attr` and the colour
  code share one style field). Setting the hints before each banner opens
  ends the close-and-reopen of every banner in `osglkban.c:300-343`.
- [ ] **Check with `glk_style_distinguish`** whether the library tells the
  centred styles apart from their uncentred twins. If it doesn't, centre
  by padding with spaces to the `glk_window_get_size` width, the way `vmconhmp.cpp` (~line 705) pads
  `<tab align=center>` in text mode.
- [ ] **Theme switch.** With the theme's "use game styles" switch
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
| `body bgcolor`, `text` | 125 (outside the status line) | The window background via `win_setbgnd` and the default foreground for that window. Fix `os_set_text_color` passing the TADS status mode as a window id (`osglk.c` `curwin`). |
| `h1`-`h4` (+`align`) | 31, 29, 55, 6 | Bold with blank lines around. `align=center` → centred bold (`style_Header`). |
| `center`, `p align`, `div align` | 159, 11, 8 | The centred styles in Step 1; `align=right` counts as centred. |
| `blockquote`, `bq` | 24, 5 | A blank line around; no indent (`style_BlockQuote` is used for centring). |
| `ul`, `ol`, `li`, `lh` | 17, 8, 21, 2 | Paragraphs that start with "• " or "n. ", generated by the parser; nested lists add two spaces per level. |
| `pre` | 5 | `style_Preformatted`; keep whitespace (TADS 2 already tracks it). |
| `hr` (`size`, `width`) | 26 | A rule of `─` characters across the window; Glk has no horizontal rule. |
| `sup`, `sub` | 31, 1 | Ignored. Most of the 31 come from the adv3 library. |
| `q` | 102 | Curly quotes, alternating when nested. Both engines already do this; keep it. |
| `br clear`, `br height` | 5, 6 | `height` as now. `clear` → `glk_window_flow_break` (see images). |
| `nobr`, `wrap`, `address`, `basefont` | 1, -, 2, 1 | `nobr`: non-breaking spaces. `address`: italic. Ignore the rest. |
| `tab` | 218 (mostly the status line) | In grid windows, port the TADS 3 tab-stop code: `align=right/center`, `to`/`id`, `indent`, `multiple` (4 games). In buffer windows, use spaces or a tab character; true tab stops are not available. |
| `title` | 78 | `garglk_set_story_title`, as now. |
| `aboutbox` | 93 | Suppress the contents, as now. Possible extra: keep the text and show it in Spatterlight's game info panel. |

## Step 3: Banners (59 games with custom banners; the status line in almost all)

- [ ] **Tag banners.** `<banner id=… height=… width=… align=… border remove
  removeall>` maps onto the TADS 3 banner machinery already in `osglkban.c`:
  - one Glk window per id
  - `align` top/bottom/left/right → `winmethod_Above`/`Below`/`Left`/`Right`
  - `n%` → proportional
  - `height=previous` → keep the current size
  - no height → size to contents with `win_lines`
  - pixel sizes → convert through the font metrics from `glk_style_measure`
    instead of treating pixels as character counts (an existing bug in
    `osglkban.c:486-528`)

  Re-opening a banner id **replaces** its contents. This fixes TADS 2's
  current fault of printing status-line contents inline in the main text.
- [ ] **The status line** (`id=StatusLine`, `<tab align=right>`) goes to a
  grid window, with `body bgcolor=statusbg text=statustext` as theme status
  colours.
- [ ] `border` → `winmethod_Border` vs `NoBorder`.
- [ ] Ids seen in the corpus include `MenuTitle`/`MenuBody` (menu libraries),
  `credits`, `intro`, `left`/`right`/`middle`, `convmenu` and `story`.
  These are text-buffer banners that need styles, links and images like the
  main window.
- [ ] Fix `os_banner_set_screen_color` so it isn't overwritten on reopen
  (`osglkban.c:236`). Most of this goes away with Step 1.

## Step 4: Hyperlinks (`<a>`, 91 games)

Both engines already handle links: they expect an `OS_EVT_HREF` event
(`tads2/osifc.h`) with the href text in `info->href`. T2 handles it in
`bif.c` (`inputevent()`, ~3636) and T3 in `vmbiftio.cpp` (`inputEvent()`,
~470). The missing part is the Glk side in `osglk.c`, which returns only
`OS_EVT_KEY` and `OS_EVT_TIMEOUT` today.

- [ ] `<a href=…>` → `glk_set_hyperlink(n)` with a per-window table from n to
  href, and `glk_request_hyperlink_event` on every window that has links.
  - Link values must be nonzero, because 0 ends a link.
  - Give a repeated href the same n, so a status line that is redrawn every
    turn doesn't grow the table. Clear a window's table when the window is
    cleared or closed.
  - Request hyperlink events on the main window, the banners and grid
    windows. A request is used up by its event, so request again after
    each click.
  - `<a name=…>` without `href` is an anchor, not a link: print the text
    plainly.
  - Check `gestalt_Hyperlinks` and `gestalt_HyperlinkInput` once. Without
    them, print every link as plain text.
- [ ] **Command links** (65 games: `href=north`, `go west`, `look`, …). On a
  click, the href becomes the player's command, as in HTML TADS. Glk can't
  insert text into a pending line request, so:
  - Cancel the line request with `glk_cancel_line_event`, which also gives
    us whatever the player had typed so far.
  - Plain link: submit the href as the line. Line echo is off for our input
    (the timer code already uses `glk_set_echo_line_event`), so print the
    href in `style_Input` followed by a newline. The transcript then reads as
    if the player had typed it.
  - `noenter`: request line input again, prefilled with the href through
    `initlen`.
  - `append`: same, but prefill the typed text plus the href.
  - During `os_waitkey`, `os_getc_raw` or a MORE prompt, a click counts as
    a key press. Cancel the char request first.
- [ ] **Every wait loop handles `evtype_Hyperlink`.** In `osglk.c` that is the
  line-input loop (~753–775, which today handles only Arrange, Timer and
  LineInput), `os_waitkey`/`os_getc_raw`, the MORE prompt, and
  `os_get_event` (~1017).
  - `os_get_event` returns `OS_EVT_HREF` with the href copied into
    `info->href`, cut to 255 bytes. Games that run their own event loop
    (menus, hint systems) read clicks this way.
- [ ] Image-map links in Step 5 use the same table and event path, so they
  come after this step.
- [ ] **URL links** (52 games: `news:`, `mailto:`, `http:`, mostly in about
  text). Open them through the app, behind the user's link setting, and
  report `LINKS_HTTP`, `LINKS_MAILTO` and `LINKS_NEWS` accordingly.
- [ ] `plain` (26 games): no link styling. Glk has no way to ask for an
  unstyled link, so print the text without `glk_set_hyperlink`. Most
  `plain` links are in status-line or menu banners, where a click is
  rarely needed. Revisit if a game depends on one.
- [ ] `title`, `target`: ignore.

## Step 5: Images (`<img>`, 119 games; JPEG 76, PNG 37, MNG 2)

- [ ] `src` → resource lookup (Step 0) → `glk_image_draw` /
  `glk_image_draw_scaled` in the current text-buffer window, which is the
  main window or a banner.
  - `align=left`/`right` (38 games) → `imagealign_MarginLeft`/`Right`.
  - `align=top`/`middle`/`bottom` or none → `InlineUp`/`InlineCenter`/
    `InlineDown`.
  - `width`/`height` (16 games) → scaled drawing. A percentage scales
    against the window size, using `glk_image_draw_scaled_ext` the way
    Question does.
  - `hspace`/`vspace`/`border`: ignore at first. Most values are 0.
- [ ] `br clear=left/right/all` → `glk_window_flow_break`.
- [ ] `alt` (16 games): print it when the image is missing or images are
  turned off. This is what the text mode does today.
- [ ] Images in banners: the same code, because tag banners are text-buffer
  windows. A banner without a height must size to its contents, image
  included: use `glk_image_get_info` for the image height and convert it to
  lines with `glk_style_measure`.
- [ ] Percentage sizes are computed once, when the image is drawn. Glk
  can't rescale an image that's already drawn when the window is resized,
  so accept that rather than redrawing.
- [ ] `usemap` with `<map>`/`<area>` (8 games, among them Arrival, Golden
  Skull, Helpfulman and Six Stories). Glk can't make parts of an image
  clickable. Instead, print the areas as a row of command links under the
  image; their hrefs are commands such as `goto bridge`, `look` or `save`.
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

- [ ] One Glk channel per HTML TADS layer: `foreground`, `ambient`,
  `bgambient` and `background`. Create them with
  `glk_schannel_create_ext`.
- [ ] `src`, `layer` (45 games), `cancel[=layer]` (21), `repeat=n|loop` (11)
  → `glk_schannel_play_ext` with a repeat count, `glk_schannel_stop`.
- [ ] HTML TADS queueing rules (see the `<SOUND>` section of the HTML TADS
  reference, which isn't in this repo):
  - A foreground sound interrupts only with `interrupt` (11 games);
    otherwise it queues behind the current one.
  - Background sounds queue.
  - `ambient` with `random=n` (4 games) plays with a per-turn chance.
  - `sequence=replace|random|cycle` applies only to looping sounds.

  Implementing the queues needs `evtype_SoundNotify` handled by every
  event wait in `osglk.c`: line input, `os_waitkey`, `os_get_event` with
  timeout, and the MORE prompt. Today those waits only handle line, char
  and arrange events. Pass a nonzero notify value to
  `glk_schannel_play_ext` for each queued sound, so the end of a sound
  starts the next one.
- [ ] `volume` (5), `fadein`/`fadeout` (3) → `glk_schannel_set_volume_ext`
  with a duration. Report `AUDIO_FADE`.
- [ ] `alt`: print it when sound is off.
- [ ] Check `gestalt_Sound2` and give up on layers that can't be created,
  instead of failing. Without sound support, `SYSINFO_*` must report the
  formats as 0 so the game knows.
- [ ] Autosave: the layer queues must be restorable, or the sounds rebuilt
  after a restore, the same way Scarier does it.

## Step 7: Tables (`<table>`, 92 games)

Glk has no tables, and many of these games use tables for layout: `border=0
width=100%`, side-by-side image and text, menus, and score sheets.

- [ ] **First pass: linearise.**
  - `<tr>` → paragraph break.
  - `<td>`/`<th>` → separated by an em space or tab; `th` is bold.
  - `align` → justification only when a row has a single cell.
  - Drop `cellpadding`, `cellspacing`, `width` and `height`.
  - Show `bgcolor` (9 games) as the zcolors background of the cell's text.

  This keeps all the text readable, in reading order.
- [ ] **Possible later work: real tables.** AppKit has `NSTextTable` and
  `NSTextTableBlock` in paragraph styles, and a Spatterlight extension
  (begin table, cell, end table) could pass the structure through. It only
  makes sense if the linearised output turns out to break games that matter,
  such as 1893, Arrival, According to Cain and Drool.

## Not worth doing (or not possible in Glk)

- Pixel-exact layout: `hspace`/`vspace`, cell padding, banner borders and
  widths in pixels beyond the conversion to character cells.
- `<body background>` tiling (until an extension
  exists), and image maps beyond the link-row fallback.
- `<log>`/`<nolog>` (scripting already goes through Glk transcripts).
- Font size, font face, superscript/subscript, underline, strikethrough and
  indent, unless the text-attribute extension in Step 1 is built after all.

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
