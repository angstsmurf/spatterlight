# Question: game colours and colour detection (TODO)

This is a plan for porting Scarier's text colour system ("glk colour"), along
with the load-time check that decides whether a game needs it, to both
Question frontends: Quest 4 (`questionglk.cc`) and Quest 5 (`quest5/aslxglk*.cc`).
It follows the same pattern as the style work that has already landed (see
the `assign_game_styles` comments in `questionglk.cc` and
`quest5/aslxglk-output.cc`): the parts both frontends share go in
`questglk-common.cc`, and each frontend feeds them its own game data.

## What Scarier does (the model)

All of this lives in `terps/scarier/glk/`. The other files call into it
through `os_glk_internal.h`.

- **Feature gate.** `GSC_HAVE_ZCOLORS` (`os_glk_internal.h`) is defined for
  Spatterlight, Gargoyle, and any Glk that has `GLK_MODULE_GARGLKTEXT`. Without
  it the whole mode compiles out, and the "glk colour" command says the mode is
  unavailable. CheapGlk and glkterm builds, and so the headless goldens, never
  see it.
- **Palette.** There are three globals in `os_glk.cpp`:
  `gsc_colour_background`, `gsc_colour_output` (the replies colour) and
  `gsc_colour_input` (typed text and `<c>`). ADRIFT 4 uses the Runner's
  defaults (black, #00FF00, #FF3232). ADRIFT 5 uses the adventure's own four
  colours (`a5model.h`).
- **Off by default.** The story uses the interpreter's theme. The mode turns on
  in three ways: the player types `glk colour on`, the player passes `-c` on
  the command line (`os_glk_platform.cpp`), or the game is detected as needing
  it (`gsc_colour_detect`).
- **Detection** (`os_glk_output.cpp`, `gsc_colour_detect` /
  `gsc_colour_wanted_by`) runs at load time, while the loading window is still
  open, so the theme's real `style_Normal` BackColor can be measured. If the
  library can't measure, it assumes white. The mode turns on if any of these is
  true:
  1. ADRIFT 5 only: the adventure's palette differs from the Runner's default
     (`a5model_custom_palette`).
  2. Some string contains `<bgcolour=...>`, which repaints the whole pane.
  3. Some string has a `<font colour=...>` that fails a WCAG 3:1 contrast test
     against the measured theme background (`gsc_colour_legible`, an
     integer-only luminance with gamma ≈ 2).

  A plain coloured word that is readable on the theme does **not** count;
  roughly a quarter of the A4 corpus has one. Text in the input colour needs no
  rule of its own. Detection is never consulted on autorestore.
- **Startup without a rebuild.** `gsc_colour_startup_prepare` publishes Normal
  TextColor/BackColor stylehints (for buffer and grid) *before* the first
  window opens. `gsc_colour_startup_apply` then turns the mode on after the
  windows exist. `gsc_colour_hints_preapplied` stops `gsc_set_colour` from
  tearing down a window tree that already has the hints.
- **Toggling mid-session** (`os_glk_commands.cpp`, `gsc_set_colour`):
  1. Set or clear the Normal hints.
  2. Rebuild the window tree (`gsc_colour_rebuild_windows`). Libraries
     snapshot hints when a window opens, so the panes reopen in the order the
     session opened them.
  3. Repaint each text window with `gsc_colour_repaint`: on means zcolors then
     clear; off means Default, measure BackColor, clear with that colour, then
     Default again.
  4. Reset the status stream to Default, because Gargoyle's zcolors are
     global.
  5. Redraw the status bar and the map.

  Both directions wipe the transcript.
- **Per-span colour.** `gsc_colour_apply(win, fg)` uses
  `garglk_set_zcolors_stream(fg or output colour, background)`. It does nothing
  when the mode is off. It's called from `gsc_set_glk_style` on every style
  change: the innermost `<font colour>` wins, then the input colour while
  `<c>` is open, then the output colour. Tag colours go through
  `gsc_colour_from_tag`, which accepts both spellings and matches the key at a
  word boundary so `bgcolour` isn't read as `colour`. Names go through
  `gsc_colour_lookup`, using the Runner's table. Short hex is zero-padded the
  Runner's way. `GSC_COLOUR_NONE` (0x01000000) means "inherit".
- **Prompt and echo.** `gsc_colour_echo(TRUE/FALSE)` switches the main window
  between the input and output colour around the prompt and the player's line.
- **"Is it actually showing?"** `gsc_colour_visible` checks two things: the
  mode is on, and measuring Normal gives back the published palette. When
  Spatterlight's "Games can set colors and styles" is off, or Gargoyle's
  stylehint preference is off, the calls are accepted but the theme is drawn.
  Places that use colour *in place of* a style (the reverse-video status bar,
  `<c>` text that falls back to Emphasized) then fall back to the style. The
  measurement is cached in `gsc_normal_measure_state` and reset on a toggle and
  on every Arrange event.
- **Status bar** (`os_glk_status.cpp`). In colour mode the bar is drawn in
  Normal with the palette inverted: zcolors are named again after every grid
  clear, and the story window's last fg/bg (`gsc_colour_main_fg`) are restored
  afterwards. Outside colour mode it uses reverse-video User1.
- **Mid-game background change.** `<bgcolour=...>` (`gsc_handle_bgcolour_tag`)
  changes `gsc_colour_background`. If the mode is on and the window can be
  cleared without losing text, it repaints.
- **Autosave.** Only the `colour_on` flag is archived. The restored streams and
  windows already carry their zcolors and backgrounds.

## Where Question is now

- **Quest 4.**
  - `define game` takes `background <col>` and `foreground <col>`
    (`question-session.cc` ~350/378).
  - The `background`/`foreground` script statements can change them in play
    (`question-script-output.cc`, `exec_colour`).
  - Text uses `|cr |cl |cy |cg` (red, blue, yellow, green) and `|cb` (back to
    the default) (`question-runner.cc` ~213, which sets `QuestionFontStyle::color`).
  - Every one of these reaches `QuestionGlkInterface::set_foreground` and
    `set_background` (`questionglk.cc` ~1422), which are **empty**.
    `style_combo_of` ignores `style.color`.
  - Ground truth is `~/questviva-oracle/questviva/src/Legacy/`:
    - `GetHTMLColour` (`V4Game.cs:3532`) knows only six names: white, black,
      blue, yellow, red and green. Any other value passes through and gets a
      `#` in front, so `000080` works and `navy` becomes `#navy`, which the
      browser ignores.
    - The defaults are white for the background and black for the
      foreground. **Quest's native look is the light theme**, the opposite of
      ADRIFT's.
    - Room and object aliases are wrapped in `|cr...|cb` (`V4Game.Part2.cs`
      1791/3748), so most games print red text even if the author never asked
      for colour.
- **Quest 5.**
  - Core sets `game.defaultbackground`, `defaultforeground` and
    `defaultlinkforeground` (CoreTypes.aslx: White, Black, Blue). The `theme_*`
    types change them; `theme_retro`, for example, is black and white.
  - `SetBackgroundColour` calls `JS.setBackground` and sets the attribute.
    `SetForegroundColour` and `SetLinkForegroundColour` only set the attribute.
  - `GetCurrentTextFormat` writes `color:<defaultforeground>` into **every**
    `OutputText` span.
  - The frontend drops all colour: `css_style` and `tag_style` in
    `aslxglk-output.cc` ignore `color`, and so does `<font color>`. The one
    exception is the grid map, which already follows `JS.setBackground` and
    `PlayerUI.SetBackground` (`g_gm_canvas`, `aslx-runtime.cc` ~2939/3183) and
    has a CSS3 colour table (`gm_colour`, `kGmColours` in `aslxglk-map.cc`).
- Neither frontend has a meta-command layer like Scarier's `glk ...` commands,
  or a `-c` switch.

## Steps

### 1. Shared colour primitives (`questglk-common.cc/.hh`)

- [x] Add a `QUESTGLK_HAVE_ZCOLORS` gate, defined the same way as
  `GSC_HAVE_ZCOLORS`, so that the CheapGlk smoke harness, `make replays` and
  every golden compile the mode out and stay byte-identical.
- [x] Add a `COLOUR_NONE` sentinel and a palette struct `{bg, fg, link, input}`
  with the on/off flag, the startup flag, the preapplied flag and the last
  main-window fg.
- [x] Add colour parsing. **There must be one table for Quest 5:** move
  `gm_colour` and `kGmColours` out of `aslxglk-map.cc` into the shared file and
  let the map call the shared version. Quest 4 needs its own small parser:
  the `GetHTMLColour` six names, then bare hex with or without `#`, and
  "unparseable" for anything else (the browser would ignore it). Decide whether
  Quest 4 should also accept CSS names; the real runner didn't.
- [x] Port `gsc_colour_luminance` and `gsc_colour_legible` unchanged.
- [x] Port `normal_measure` with its cache and `colour_visible`.
- [x] Call `normal_measure_reset` in both frontends' `evtype_Arrange` handlers
  (`questionglk.cc` ~1054/1594, `aslxglk-input.cc` ~386/516/801). Moved to
  Step 4: the reset only means something next to the rebuild it triggers.
- [x] Port `set_normal_hints`, `colour_apply`, `colour_repaint` and the
  startup prepare/apply pair. The rebuild is per frontend (Step 4), since each
  has its own window set.
- [x] Add unit tests in `test/questglk_unit_tests.cc`: parsing for both
  dialects, and the legibility verdicts from Scarier's comment (red, green,
  blue, teal, navy and grey readable on white; white, silver, yellow, lime,
  cyan, orange and pink not).

### 2. Detection

Rewrite the rules for Quest's defaults. The question stays the same: "was
this written for a pane the theme can't stand in for?"

- [x] **Quest 4.** Hook into the silent pre-open scan that already exists
  (`question_style_scan`), so the file isn't read a third time. Turn the mode on
  if any of these is true:
  - The `define game` `background`/`foreground` differ from white/black
    (the header rule, like `a5model_custom_palette`).
  - A `background` or `foreground` *statement* appears anywhere in the script
    (like `<bgcolour>`). This is a static scan, since the scan can't know
    whether the statement will run.
  - Any `|cy`, `|cg`, `|cl` or `|cr` text fails the legibility test against the
    measured theme background. **Measure against the theme, not against white:**
    `|cy` on Quest's own white page was already unreadable, so a game that uses
    it on a white page isn't "asking for" anything. The test should only fire
    when the game's *declared* background makes that colour readable and the
    theme doesn't. Write this as: legible on the game background and illegible
    on the theme background. The alias `|cr` red passes on any theme and never
    fires.
- [x] **Quest 5.** Do this in `aslx_glk_main` after the World preload, next to
  `assign_game_styles` (end of `aslxglk-output.cc`). Read the *resolved*
  attributes, so that type inheritance (`theme_*`) counts. Turn the mode on if
  any of these is true:
  - `game.defaultbackground`, `defaultforeground` or `defaultlinkforeground`
    differ from Core's White/Black/Blue. Compare *parsed* values, so that
    `white` and `#FFFFFF` are equal.
  - Any script calls `SetBackgroundColour` or `SetForegroundColour`
    (`JS.setBackground`, or `request (Background, ...)` for 5.0-era cores).
    Scan the script text the same way the style census scans markup.
  - Any authored `<font color>` or `style="color:..."` fails the
    game-bg-versus-theme-bg test above. Skip the `color:` that Core's
    `GetCurrentTextFormat` adds to every span; only the game's own markup
    counts. Core's own text only uses the default foreground.
- [x] Measure the theme background while a window exists, before the colour
  hints are published. Quest 4 opens no loading window, so open and close a
  throwaway buffer window first, the way Scarier does, or measure on the first
  real window before any hint is set. Check which of these Spatterlight
  answers correctly (glkimp measures from the window's own style table).
- [x] Add an env-var census dump like `ASLXGLK_STYLE_CENSUS`
  (`QUESTION_COLOUR_CENSUS=1`) that prints which rule fired and on what string.
  Run it over both corpora (`test/quest4/games`, `test/quest5/games`) and
  record how many games turn the mode on and why, as `styles_census` did for
  Scarier. **Look at the results before choosing defaults.** If Quest 5
  `theme_*` games make up a large share, that's expected and correct. If plain
  Quest 4 games trip the statement rule only because of a dead `foreground`
  line in a library, narrow that rule.

Done. Notes from building it:

- The theme is measured on the main window, right after it opens and before
  any colour hint reaches it (`colour_detect_settle`). The header and
  statement rules are known from the pre-open scan, so their hints can still
  be published in time; only the span rule waits for the window. Skipped on
  autorestore. `QUESTION_COLOUR_THEME=#rrggbb` stands in for a library that
  can't measure (CheapGlk, the census).
- Narrowed rules:
  - A link colour alone doesn't turn the mode on. 26 Quest 5 games set only
    `defaultlinkforeground`, mostly to Black to hide links; the colour is
    still kept for when the mode is on.
  - A `foreground` statement (Quest 4) or a `SetForegroundColour` call with a
    literal (Quest 5) is judged like coloured text, not as an automatic hit.
    Arguments that read `game.default*` are skipped, as is Core's own
    `InitInterface` call and Core's definitions of the functions.
  - Core's `DbgMsg` is its one coloured literal and is left out of the scan.
- Census (`QUESTION_COLOUR_CENSUS=1`), light theme / dark theme:
  - Quest 4, 156 games: 59 / 74 on. Light: 34 header only, 12 header+span,
    2 header+statement, 11 statement only (each a real background change:
    a hitmans life, Beyond Exile 2.2, drom_bennacht, gooth913, La Mansion 2's
    colour menu, Londe Perplex, Motorbike Keyfinder, Nearco/Nearco3,
    Ponyville, sleepover). Dark adds about 15 games whose `|cl` blue goes
    unreadable.
  - Quest 5, 376 games: 186 / 204 on. Light: 128 header, 28 call, 19
    call+header, 6 header+span, 5 all three. Most common headers: Black
    background (74), White foreground (50). Dark adds 18 span-only games,
    mostly blue or navy text.
- For Step 3: `do what you want.asl` declares a black background and leaves
  the foreground black. Consider the convenience of flipping an illegible
  declared foreground to the theme's.

### 3. Per-span colour in the printers

- [x] **Quest 4.** Fill in `set_foreground` and `set_background` to update the
  palette. Background is the `<bgcolour>` case: if the mode is on, repaint when
  this doesn't lose text, using the same "window still empty" test Scarier
  uses (`gsc_main_window_empty`). Make `set_style` call `colour_apply` with
  `style.color` or the game foreground. `|cb` (empty) means inherit.
  - Decide what the alias `|cr` red is: Scarier's "input colour" or a plain
    red. Under Quest it's literally `red`.
- [x] **Quest 5.**
  - Add `color` to `StylePush` and a colour stack to `InlineStyle`, pushed
    and popped like `sizes`. `css_style` reads `color`. `tag_style` reads
    `<font color>`.
  - `apply_style` calls `colour_apply` with the innermost colour.
  - Track `game.defaultforeground` at runtime through the attribute, not a
    copy, so a later `SetForegroundColour` takes effect in the next span the
    way it does in the browser.
  - Handle `SetBackgroundColour` (the `setBackground` JS case) in the text
    window as well as the map when the mode is on.
- [ ] **Links.** Glk hyperlinks are drawn in the theme's link look. In colour
  mode, colour `cmdlink` spans with `defaultlinkforeground`
  (`aslxglk-output.cc` ~851) through zcolors, if Spatterlight and Gargoyle
  respect zcolors on hyperlinked text. Check this before committing to it.
- [ ] **Echo and prompt.** Quest has no separate input colour. Quest 5 colours
  `#txtCommandDiv` with `defaultforeground` (Core.aslx:177), so the echo
  probably just uses fg. Confirm against QuestViva and the Quest 4.1.5 Wine
  runner before porting `gsc_colour_echo` at all.

Done, except where noted below. Notes:

- Quest 4 reads colour names the way QuestViva's `GetHTMLColour` does: it
  knows only white, black, blue, yellow, red and green. Any other name
  becomes `#name`, which the browser ignores, so `navy` is rightly
  `COLOUR_NONE`; bare hex such as `000080` works. The alias `|cr` is plain red.
- Quest 5 links take only the colour from Core's `GetCurrentLinkTextFormat`
  style; their font and size are the text's. Whether Spatterlight and
  Gargoyle draw zcolors on hyperlinked text is still to be seen (Step 5).
- Echo and prompt: not ported. The echo is written in the current span
  colour, which is the game foreground. Comparing with QuestViva and the
  Wine runner is still open.
- Not done: the `do what you want.asl` convenience (see the end of Step 2).

### 4. Windows, panes and the toggle

- [x] Add a rebuild per frontend:
  - **Quest 4:** main, banner, object pane, divider.
  - **Quest 5:** main, banner, side pane, divider, picture panel, map.

  Reopen them in session order, as `gsc_colour_rebuild_windows` does, and
  repaint every text pane the game owns (the `gsc_a5_side_window` lesson).
  These already measure Normal and so follow the theme today: the divider fill
  (`questglk-common.cc` ~1022), `panel_set_background` (`aslxglk-media.cc`
  ~588), and the map canvas. Check that each one picks up the palette after a
  rebuild, without code of its own.
- [x] Status bar and banner: port the inverted-Normal-in-colour-mode /
  reverse-User1-otherwise rule (`questglk-common.cc` ~646 already hints User1
  reverse in the grid). Name zcolors again after every grid clear, put the main
  stream's colours back afterwards, and set Default on the status stream when
  turning the mode off (Gargoyle's global zcolors).
- [x] **Decision for the user: how does the player toggle the mode?** Question
  passes every command to the game. Choose between:
  - a new frontend-intercepted command (Scarier's `glk colour on|off`), or
  - a command-line switch only, plus Spatterlight's existing "Games can set
    colors and styles" theme setting as the real off switch. Since
    `colour_visible` already treats "hints ignored" as off, this may be all
    that's needed.
- [x] Autosave: add `colour_on` to `QuestionGlkFrontendState`
  (`questionglk-autosave.h`) and to the Quest 5 blob (`aslxglk-autosave.cc`).
  Add the palette too, unless it's shown to come back on its own. A Quest 4
  `background` statement run in play is frontend state, so it's lost if only
  the flag is saved. Bump the blob version, and decode old blobs as "off".
  Make sure detection is skipped on autorestore.

Done. Notes:

- The toggle is option (a): `COLOURS`, `COLOURS ON` and `COLOURS OFF`
  (and `colour`/`color`/`colors`, with or without a leading `#`), intercepted
  by both frontends and listed in the frontend help.
- The rebuild closes the root window and reopens the tree in session order.
  Quest 4: main, banner/input, object pane. Quest 5: main, banner, picture
  panel (`panel_reopen`), map (`grid_map_reopen`), with the side pane and
  divider redrawn from game state and the hyperlink request made again.
- Status bar: Normal with the game's colours inverted while colours are
  visible, otherwise reverse User1. The main stream's colours are put back
  after every draw (`colour_status_end`). Turning the mode off sets Default
  zcolors on the banner.
- Autosave: the flag, the palette and the current span colour are saved.
  The Quest 4 blob is now version 2 and the Quest 5 blob is version 11;
  older blobs decode as "off". Quest 4 needs the saved palette: zeroing the
  saved background makes `q4-colours` fail. Quest 5 would recover without it,
  because Core's `SetBackgroundColour` sets `game.defaultbackground` and
  `InitInterface` applies it again.

### 5. Tests and checks

- [x] Add fixtures beside `test/styles/`: `colours_q4decl.asl` (dark declared
  background), `colours_q4yellow.asl` (`|cy` on a white declaration, which
  must *not* turn the mode on), `colours_q5theme.aslx` (`theme_retro`),
  `colours_q5plain.aslx` (only a readable `<font color=red>`, which must stay
  off), and a `SetBackgroundColour`-in-play case. The `run_styles_tests.py`
  transcripts would need a `{fg/bg}` marker next to `{StyleName}` to see
  zcolors. Check whether glkimp's test hooks expose them.
- [x] Add `make autosave` cases `q4-colours` and `q5-colours`: start in
  colour, save, relaunch, and check that the mode and the background survive.
- [x] Text gates: `make check`, `make replays`, and the `aslxglk_smoke` diff
  against a base build (the recipe is in the Quest 5 styles notes). All should
  be byte-identical, because the mode compiles out under CheapGlk.
- [ ] Visual checks in Spatterlight, with "Games can set colors and styles" on
  and off, and in the Gargoyle fork (build the `question` plugin): a
  `theme_retro` game, a Quest 4 game with a dark `define game` background, and
  a plain game, which should look exactly as it does today. With the setting
  off, the status bar has to fall back to reverse User1, not draw invisibly in
  Normal.
- [ ] Sync the garglk fork afterwards (see the Question sync notes).

Done so far:

- Fixtures (with a `.script` sidecar that replaces the single LOOK):
  - `styles_colours.asl`: a Quest 4 game with a black/white `define game`,
    run through look / colours off / look / colours on / look.
  - `styles_q5colours.aslx`: a Quest 5 game with Black/White defaults that
    calls `SetBackgroundColour("Navy")` in its start script.

  `run_styles_tests.py` prints `ZCOLOR peer fg bg` lines between the SPAN
  lines, using the zcolors that `glkdrive.py` now records. The two copies of
  `glkdrive.py` (Question and Scarier) are kept identical.
- Autosave cases `q4-colours` and `q5-colours` cut after each step of:
  look, paint, look, colours off, look, colours on, look.
- Gates: `make check`, `make styles`, `make autosave` and Scarier's styles
  suite pass. `make replays` shows 367 passed and 7 failed, the same as before
  this work. The `aslxglk_smoke` output is byte-identical to a build of
  59b8710dc on all 374 Quest 5 goldens.
- The census fixtures listed above (`colours_q4yellow` and the others) were
  not added. The census run in Step 2 covers detection.
