# terps/tads/glk: TADS 2 and 3 on Glk, with HTML TADS

This is the OS layer that Spatterlight's TADS 2 and TADS 3 interpreters
share. It runs HTML TADS games by translating their markup into Glk calls,
plus a few Spatterlight extensions. It doesn't embed an HTML renderer.

The test corpus is `terps/tads/test/htmltads`, 235 games (see
`test/GAMES.md`), and `test/xtads`, 1343 test games.

## Ground rule: stay inside the Glk spec

Everything must also run, in a reduced form, on a Glk library that follows
the spec and has none of Spatterlight's extensions.

- Use only the 11 standard styles. Set hints before the window that needs
  them opens, and never rely on a hint changing an open window.
- Extensions are additive: each is detected through `glk_gestalt` and has
  a spec-only fallback. The ones used are `garglk_set_zcolors`,
  `garglk_set_reversevideo`, `garglk_set_story_title`,
  `garglk_add_resource_from_file` and `win_setbgnd`.
- Spec features are used for what the spec defines them for: hyperlinks,
  `glk_image_draw*` and `glk_window_flow_break`, `glk_schannel_*` (with
  volume durations and notify events), and `glk_style_measure`.
- A hyperlink, sound notify or arrange event during line input doesn't
  cancel the input unless it has to.

## Files

| File | What it does |
|---|---|
| `oshtml.c`, `oshtml.h`, `oshtml_priv.h` | The markup parser: tags (one handler per tag, `tag_handlers[]`), entities, the tag stack, and the sinks it writes to. |
| `oshtml_scan.c/.h` | Stateless markup readers: names, attributes (`attr_iter`/`attr_next`), and the scanners of a banner's whole text. |
| `oshtml_banner.c` | A TADS 2 `<BANNER>`'s text, laid out at `</BANNER>`: picture tables, cells over pictures, or text. |
| `osstyles.c/.h` | The 11 Glk styles, chosen per game by scanning the game file. |
| `osglk.c`, `osglk.h` | Windows, input and the event loop, links, `systemInfo`, colour measuring. |
| `osglkban.c`, `osglkban.h` | The banner API (`os_banner_*`) and banner pictures (`picture_fit`). |
| `osglktagban.c` | Tag banners (`osglk_tag_banner_*`), including their tables and cells. |
| `osglkpage.c` | Pictures beside the main window's page, and a page of pictures on a background. |
| `osres.c/.h` | Resources: HTMLRES, `.rsN`/`.3rN` and loose files, as Glk resources. |
| `ossound.c/.h` | `<SOUND>`, in four layers on Glk sound channels. |
| `osportable.cc` | FrobTADS's portable OS functions, and the virtual clock. |
| `osfrobtads.h`, `osspatter.h` | The OS header (`tads2/os.h` → `osspatter.h` → `osfrobtads.h`). `osfrobtads.h` keeps FrobTADS's unused parts as `#if 0` on purpose, for diffing with new FrobTADS releases. |
| `osparse.c` | Character sets; upstream Gargoyle code. |
| `t23run.cpp` | `glk_main`: picks the engine, sets up resources (`CVmHostIfcGlk`, built `-fno-rtti` like tads3). |

## How markup gets out of the engines

- **TADS 2** builds `output.c` with `USE_HTML` (`osspatter.h`), so
  `html_target` is TRUE. After `\H+`, tags and entities reach `os_print`
  unmodified. Games without `\H+` keep their text-mode behaviour.
- **TADS 3** links `vmconhtm.cpp`. Markup reaches `os_printz` and
  `os_banner_disp`. Grid banners and the log file stay plain text.
- Both formatters add markup of their own, which the parser handles:
  `<BR HEIGHT=0>` per newline, `&nbsp;` for `\ `, and TADS 2's `\(`/`\)`
  as `<B>`/`</B>`.
- Entities come out as Unicode in both engines.
- `&nbsp;` is a visible character, as HTML TADS draws it. A line of only
  `&nbsp;` and a break is a blank line. *Tomorrow Never Comes*' credits
  depend on this.
- The parser reproduces TADS 3's formatter rules for the main window:
  - A line is written if it has characters, or tab spaces after a flush
    that didn't end a line.
  - `<br>` writes a blank line only into an empty buffer.
  - The spaces before a right or centre `<TAB>` are kept.
  - `<TAB TO>` ids ignore case.
- These differences from text mode are intended:
  - No flush at `<br>`, so text can reach Glk later than before.
  - Text banners always let Glk wrap.
  - `<TAB MULTIPLE=n>` tabs to the next stop where `(column + 1) % n` is 0,
    where text mode treated it as `INDENT`. Whether HTML TADS counts from
    column 0 instead is unchecked.

### Text only (`-nohtml`)

`tadsr -nohtml game` runs as a text-only interpreter, as before HTML TADS.
`glkunix_startup_code` (`t23run.cpp`) takes the option out of argv before
the engines see it, and `osglk_set_html(0)` turns HTML off:

- `os_html_target()` (`osspatter.h`) is 0, so `output.c` and `vmconhtm.cpp`
  start with `html_target` FALSE. The formatters handle the markup
  themselves, as `vmconnom.cpp` and a build without `USE_HTML` do.
- `os_start_html`, `os_end_html` and `os_banner_start_html` do nothing, so
  `oshtml.c` never starts: no pictures, sounds, links, tables or tag
  banners.
- `os_get_sysinfo` gives a text interpreter's answers: class
  `ICLASS_TEXTGUI`, `SYSINFO_HTML` 0, and no images, sounds or links.
  Banners, highlighting and RGB colours stay.
- The startup line reads "A text-only TADS ... Interpreter"
  (`G_tads_oem_display_mode`).

Games written for HTML show what they show in any text interpreter, such
as *Tomorrow Never Comes*' `<b>` in a `setscore` status line, or the
runtime error in *Home Opener*'s credits.

### systemInfo

Every answer in `os_get_sysinfo` comes from a `glk_gestalt`. Spatterlight's
answers follow the theme: the app sends its Graphics and Sound settings at
`HELLO` and again in `EVTPREFS` when they change.

- `SYSINFO_HTML` is 1 and the class is `ICLASS_HTML`, in both engines.
  adv3's `aHrefAlt` writes links only in HTML mode.
- Images (`JPEG`, `PNG*`, `PREF_IMAGES`) follow `gestalt_Graphics` and
  `gestalt_DrawImage`.
- Sound (`WAV`, `MIDI`, `OGG`, `MPEG*`, the fades, `PREF_SOUNDS`,
  `PREF_MUSIC`) follows `gestalt_Sound2`. Spatterlight has one switch for
  effects and music.
- `PREF_LINKS` follows `gestalt_Hyperlinks`.
- The `LINKS_*` URL schemes are 0: URL links are plain text (see "Not done").
- `MNG*` is 1 in the Spatterlight build only, where the app converts MNG.
- ⚠️ Changing these changes library behaviour: adv.t and adv3 pick their
  status line and banners from them. Run the corpus before and after.

## Styles (`osstyles.c`)

HTML can combine more than Glk's 11 styles hold, so most of it is reduced:

- underline is italic, and headings are bold;
- font face, font size and super/subscript are ignored, except that
  `TADS-Input` is `style_Input` and typewriter faces are fixed;
- left indents (`blockquote`, `dd`, lists) are two non-breaking spaces per
  level at the start of each line the parser starts, so wrapped lines
  aren't indented. Right indents are ignored.

What's left is bold, italic, fixed, Input and centred (right counts as
centred). Colour is separate, through zcolors.

**Choosing the styles.** Before the first window opens, the game file is
scanned (at most 15 ms, so nothing is cached):

- TADS 2: the text properties of `OBJ` and `FMTSTR`, XOR-decrypted with the
  `XSI` seed and increment.
- TADS 3: every `CPPG` page, XOR'd with its mask.

The scan follows the tags in each string and counts the combinations used.
`\(`/`\)` count as bold. Then:

1. Normal is plain and Input is Input, always.
2. The 9 most used other combinations get a slot. Bold, italic and bold
   italic are ranked even at a count of 0. Ties go by combination number.
3. Each goes to its natural slot if free: italic → Emphasized, bold →
   Subheader, bold italic → Alert, fixed → Preformatted, centred bold →
   Header, centred plain → BlockQuote. The rest fill User1, User2, Note,
   then any unused natural slot.
4. A combination without a slot falls back by dropping bold on fixed or
   Input text, then italic, then centring, then bold, else Normal.
5. A slot nothing uses gets no hints and keeps the theme's look.

If the scan finds nothing, there is a default table: Emphasized is italic,
Subheader bold, Alert bold italic, Preformatted fixed, BlockQuote centred,
Header centred bold, User1 centred italic, User2 centred bold italic and
Note centred fixed. Every slot in use besides Normal and Input gets seven
hints (Weight, Oblique, Justification, Proportional except for Input, Size 0,
Indentation 0, ParaIndentation 0). Colour and font are left to the theme.

Hints are set for `wintype_TextBuffer` only, in `os_init`, since
`wintype_AllTypes` would centre grid text too. With the theme's "use game
styles" switch off, the theme's styles win, by the user's choice.

Only *Plant* in the corpus loses a combination (centred bold fixed), and
*Chancel of Death* does when `\(` is counted. A combination built at
runtime (`'<font size=' + n + '>'`) falls back as in step 4.

**Is the centred style centred?** `glk_style_measure(stylehint_Justification)`
is asked once per combination (`osglk_centers`), and cached until the windows
are rearranged. If the style isn't centred, the line is padded with spaces,
as `vmconhmp.cpp` pads `<TAB ALIGN=center>`. A centred `<TAB>` at the start
of a banner line uses `<CENTER>`'s style where the window can. The app
answers the measure for buffer windows (grid windows are always LeftFlush),
and `glkdrive.py` answers it from the hints, as the app does.

## Text formatting (`oshtml.c`)

- The tag stack holds bold, italic, fixed, Input, alignment, colours, indent
  and list state. An end tag restores what its start tag changed and closes
  the tags left open inside it.
- An end tag with no start tag open closes the innermost open tag, as HTML
  TADS's `end_normal_tag` does. So *Drool*'s `<b>pamphlets</A>` is bold only
  to the `</A>`.
- Clearing the main window closes every open tag, as `clear_page` does
  (*1893* leaves a `<div align=center>` open).
- `<center>`, `<p align>`, `<div align>` and `<hN align>` are blocks. An
  unclosed `<center>` centres everything after it, prompt included, as in
  HTML TADS.
- `<P>` doesn't stack: `text<P><P>` leaves one blank line. An explicit `<P>`
  in HTML TADS is half a line, which is a whole blank line here.
- `font color`, `bgcolor` and `body bgcolor/text` are zcolors, with a
  colour stack. Parameter colours (`text`, `statusbg`, ...) are theme
  colours. A `<body>` or cell with a literal `bgcolor` and no `text` gets
  black or white text by the background's brightness. `<body bgcolor>` is
  the window background (`win_setbgnd`), and it outlasts a clear.
- Lists are paragraphs starting with "• " or "n. ". `<hr>` is a rule of `─`.
  `<q>` gives curly quotes. `<title>` is the story title. `<aboutbox>`'s
  contents are suppressed.
- `os_set_text_attr` goes through the parser (`oshtml_set_os_attr`), so
  `OS_ATTR_STRONG`, `EM` and `HILITE` work.

## Banners

**Tag banners** (`<banner>`, TADS 2) are one Glk window per id:

- `align` sets the split side, `n%` makes it proportional,
  `height=previous` keeps the size, and no height sizes it to its contents.
- A pixel size is exact: a graphics window that many pixels wide is split
  by the banner's window at 100%. Without graphics, a cell counts as 8×16
  pixels. A banner set in pixels is at least as tall as its text and its
  margins (*Cruise*).
- Re-opening an id replaces its contents. A clear screen removes the tag
  banners, as HTML TADS's `start_new_page` does; API banners stay. Without
  this, a full-height menu banner covers the main window for good.
- `id=StatusLine` is a grid window, with `statusbg`/`statustext` as the theme's
  status colours.

**API banners** (TADS 3, `os_banner_*`) keep their contents as chunks, to
write them again when they're redrawn, including pictures and links. A
banner of size 0 still has a window, so it keeps its text and can be fitted
to it. `os_banner_set_size` does nothing when the size is unchanged.

**Both kinds:**

- Banner colours come from `measure_colors` (`osglk.c`): style measures of
  the main window and a grid, taken at start and on every redraw. A status
  banner is a reversed grid; any other has the main window's colours.
  `os_banner_set_screen_color` survives a reopen.
- A banner's text starts at the top whatever its `align`, and scrolls to its
  end when it overflows, as QTads's `fBannerStyleAutoVScroll` does.
  ⛔ Don't anchor a banner's text at the bottom. That once hid the real
  fault in *Tomorrow Never Comes*' credits (the `&nbsp;` lines).
- **Sizing to contents** (`picture_fit`, `osglkban.c`) counts pictures. The
  app counts a line with a picture as one line and a margin picture as none,
  so the missing lines are added in `gbufcellh`/`gbufcellw` units. At the
  side, a banner is as wide as its widest line plus 2. A picture whose size
  isn't known leaves the width as it is. `osglk_tag_banner_refit` refits a
  banner after the windows around it change, so it grows with its rewrapped
  text.
- The trace harness answers every `BANNERLINES` with 24 and every picture
  size with 0×0, so fitted banners look wrong in traces. Check them in the
  app.

**Laid out as pictures** (`oshtml_banner.c`): a TADS 2 banner's text is
looked over at `</BANNER>`:

- *Picture tables*: top-level tables of only pictures (*1893*'s title over
  its compass) are laid out in pixels as one clickable picture in a graphics
  window. A clickable `<IMG USEMAP>` right before them joins it
  (*Helpfulman*). Picture tables take `rowspan`, cell `width`, side-aligned
  cell pictures, and a tiled `<BODY BACKGROUND>`. The tiles start 3 px above
  the window, as in QTads (*A Sugared Pill*).
- *Cells over pictures* (*Threnody*'s status line): the first cell's text
  is the banner's own window; each other cell (up to `OSHTML_CELLS_MAX`) is a
  grid as wide as its column, at the right; the pictures go under them.
- *A one-cell `HEIGHT='100%'` table* with a centred picture
  (`build_cellpic`; *Tomorrow Never Comes*' cards) is the picture over its
  caption.
- *A page of pictures*: only centred pictures and breaks on a `<BODY
  BACKGROUND>` (*Six*'s title) is a graphics window with the background
  tiled under it.

## Hyperlinks (`<a>`)

- `<a href>` is `glk_set_hyperlink(n)`. The link is part of the colour state,
  so `</a>` restores it.
- One link table serves all windows. A repeated href gets the same `n`. The
  table is never cleared and stops at 8192 links; later links are plain
  text.
- `glk_request_hyperlink_event` goes to the banners, then the main window,
  before every wait and after each click that doesn't end it.
- A click during line input cancels the line with echo off. Then:
  - a plain link's href is the line, echoed in `style_Input`;
  - `noenter` puts the href in the line to be edited;
  - `append` adds it to what was typed.

  In a key wait a click is a space. `os_get_event` returns `OS_EVT_HREF`.
- The OS layer's own text (echo, MORE) is never part of a link.
- `plain` is ignored, so it's an ordinary link: adv3 writes its menus and
  exit lists with `<a plain href>`. `title`, `target` and `hidden` are
  ignored too.
- `test/run_traces.py` can click links: `link:N` or `click:TEXT` in a
  script.

## Images (`<img>`)

- `src` is looked up in the resources, then drawn in the current text-buffer
  window with `glk_image_draw` or `glk_image_draw_scaled`. A missing
  dimension keeps the aspect ratio. Percentages are computed once, at draw,
  with `imagerule_WidthRatio`. Glk can't rescale a drawn picture.
- A picture takes the current style, so it centres in a centred block.
- `align=left/right` puts the picture in the margin, and the other aligns
  are inline. A margin picture after text in a line starts the next line,
  as HTML TADS floats it. `br clear` is `glk_window_flow_break`.
  `hspace`/`vspace`/`border` are ignored.
- `alt` is printed when the picture can't be drawn, and in grid banners.
- A picture alone in a table cell can go **beside the page** (`osglkpage.c`)
  in a graphics window at the main window's side, until the page is cleared.
  This happens for a picture in a row's first cell when the row's last cell
  is one too, as in *1893*'s borders. Otherwise a first-cell picture of
  64×64 or more floats in the margin with the row's text beside it
  (*Threnody*'s cat, *According to Cain*'s vignette).
- **A main-window page of pictures** on a `<BODY BACKGROUND>`, before a key
  wait, gets the background tiled in a graphics window at each side
  (`oshtml_main_page`; *Macrocosm*'s intro). Text that wouldn't show on the
  `BGCOLOR` gets the opposite background by brightness. A background behind
  ordinary text isn't drawn.
- **Image maps** (`usemap`, `<map>`, `<area>`): Glk can't make part of a
  picture clickable, so the areas become a row of command links under the
  picture, separated by ` · `.
  - Maps are kept by name for the whole game, shared by every parser (32,
    recycled oldest first).
  - Areas with `nohref`, a URL or a repeated href are left out.
  - The row waits until the pictures end (*Golden Skull*'s nine-tile
    compass), then prints before the next text, paragraph, input, the end
    of a banner, or `\H-`.
  - TADS 3 banners print the row only at their next text.
- **MNG** is converted to an animated GIF in the app
  (`Images/MNGConverter.m`) when `ImageResource` first decodes it; the
  terp is unchanged. It reads `MHDR`, the PNG runs, `DEFI`, `BACK`, `FRAM`
  modes 1–4, `LOOP`/`ENDL` and `TERM`, and stops at any other critical
  chunk. *Macrocosm* and *Antiquest* use it. Tests are in
  `SpatterlightTests/ImageHandlerTests.m`.

## Resources (`osres.c`)

- TADS 2 passes `osres_t2_appctx()` to `os0main2`. TADS 3 records resources
  through `CVmHostIfcGlk` as the loader adds them.
- A name is looked up in the resource maps (the last entry of a name wins,
  so a `.rsN` replaces the game's placeholder; *Threnody*), then the debug
  build's links, then as a loose file under the game's folder, matched
  without regard to case.
- Names are URL-style: `%xx` is decoded, case is ignored, and `\` or `/`
  separate. `..`, `:` and a leading `/` are refused.
- A resource is registered on first use. The number is a hash, not a Blorb
  index.

## Sound (`ossound.c`)

This follows HTML TADS's `htmlsnd.cpp`/`htmltags.cpp` and QTads's
`qtadssound.cc`.

- There are four layers (`foreground`, `ambient`, `bgambient`,
  `background`), each with a queue and two channels: one plays, the other
  fades out.
- A sound with no layer goes where HTML TADS puts it: MIDI to background,
  WAV to foreground (to ambient with `random`). An MP3 or Ogg without a
  layer is an error and prints `alt`.
- `repeat=loop` is `0xFFFFFFFF`. `interrupt` cancels the layer first.
  `random=n` gives the sound n% each time it comes up, and again once a
  second while the layer is idle, on a 1 s Glk timer.
- `volume`, `fadein`, `fadeout` and `crossfade` map to
  `glk_schannel_set_volume_ext` with a duration.
- Glk can't do a fade-out at a sound's natural end, or a cross-fade into a
  queued sound, because both need the sound's length.
- Every wait (`select_event` in `osglk.c`) passes sound and volume notify
  events to `ossound_event`.
- There's no TADS autosave in Spatterlight. If one is added, the layer
  queues must be saved or rebuilt.

## Tables (`<table>`)

Tables are linearised in reading order, except for the banner tables laid out
as pictures above.

- Each row is a line. `<tr>` or `<td>` closes what's left open. `<th>` is
  bold.
- Cells are separated by three spaces.
- Empty cells before a cell (the menu libraries' `<td width=x> </td>`) turn
  the gap into a four-space indent for every line of the cell.
- A first cell under 100 pixels wide is a marker cell, like *Threnody*'s
  menu pointer. Its `align` stays in the cell, and its picture replaces the
  gap.
- `align=center` (or `right`, on the first cell) on a row or first cell
  centres the line. A later cell's `right` is a right `<TAB>` where one can
  align right (*Arrival*'s score).
- A `bgcolor` is the text's zcolors background.
- Dropped: padding, spacing, `width`, `height`, `valign`, `colspan`,
  `rowspan`, `border` and `background`.

## Testing

- **Traces** (`test/run_traces.py`): a recording Glk logs every window,
  style hint, colour, print, image, link and sound. There are no goldens.
  Run the old build and the new one, then `--compare`. Pass `--build` with
  the DerivedData `Build/Products/Debug`, since the repo's `build/Debug` is
  stale.
  - `--walk` types the walkthroughs in `test/htmltads/goldens`.
  - `--args -nohtml` runs the text-only mode.
  - `--corpus xtads` runs the 1343 xtads games (`infloop.t3` always times
    out).
  - Under load, `mazes.t3` and `pi.t3` can hit the 60 s timeout. Re-run one
    game with `--timeout 300 <name>` before believing it.
- **Determinism**: the harness runs in testing mode. The RNG has a fixed
  seed. A virtual clock (`osportable.cc`) reads noon on 1 Jan 2000 UTC and
  moves only when read (1 ms), slept on, or waited on. Two runs give the
  same traces. With Delays off, a game's sleep is skipped and the clock
  moved on by its length.
- **Styles**: `test/check_styles.py` builds `osstyles.c` alone
  (`test/stylescan.c`) and checks its counts against `stylecount.py`'s
  rules on both corpora.
- **Scanner unit test**: `test/scantest.c` tests `oshtml_scan.c` alone; the
  build line is in its header.
- A loop guard (`os_break`, `osmisc.c`) stops a TADS 2 game that runs 100
  million instructions (about five seconds) without input or output, says
  so, and undoes the turn. *Six* hangs that way at its death prompt in HTML
  mode, a bug in the game.

Games that use most features: *1893* (`1893.gam`), *Arrival*
(`arrival.gam` with its `.rsN`), *According to Cain* (`cain.t3`), *Drool*
(`drool_bundled.gam`, external images), *Golden Skull* (`gold.gam`),
*Helpfulman* (`help_bundled.gam`), *Fusillade* (MIDI), *Prince Quisborne*
(`PrinceQuisborne.t3`, fades), *Threnody*, *Tomorrow Never Comes*.

## Not done

- **On purpose:**
  - URL links (`http:`, `mailto:`, `news:` and the rest) stay plain text:
    most of the pages are gone, and letting a game open a browser or mail
    client is an attack vector.
  - `<log>`/`<nolog>`: Glk transcripts cover this.
  - Pixel-exact layout outside picture tables: `hspace`, `vspace`,
    padding and table widths.
- **Not possible in plain Glk:** underline, font size and face, super- and
  subscript, strikethrough, right indents, and indenting wrapped lines.
  A text-attribute extension (`spatterlight_set_textattr`, gestalt-gated)
  could add them; the corpus doesn't justify it yet.
- **Open:**
  - `<ol type>` and `start`: every list counts 1, 2, 3.
  - A `<BODY BACKGROUND>` behind text.
  - Real tables (`NSTextTable` through an extension), if linearising ever
    breaks a game that matters.
  - Drool's airport-ending Caltech page (a margin picture with an
    upper-case `.PNG` name) hasn't been seen.

## Refactoring: decided against

These came up in the 2026-10 refactor pass and were rejected after reading
the code; don't re-propose them without new reasons:

- A shared `window_sink` for the three sinks: only `centers` was a copy, and
  the rest differ per sink.
- Sub-structs in the parser: `oshtml_clear` resets single fields inside
  the groups, so nothing gets simpler.
- One `banner_metrics` group for the char/pixel conversions: they round
  differently on purpose (see `glk_window_get_size` floor vs Fixed-split
  ceil).
- A banner-event facade in `osglk.c`: it would only hide the order in
  `redraw_windows`.
- Moving `url_utf8` to `osres.c`: it's an encoder, not URL handling.
- Logging the fixed limits (`MAPS_MAX`, `ELEMS_MAX`, ...): each has a
  commented overflow policy, and logging would change traces.
- Generating `osparse.c`'s conversion tables: it's upstream Gargoyle code.
