# DONE: Alan 2 `SYSTEM` media calls are mapped to Glk images and sound channels

Some Alan 2 games drive graphics and sound by shelling out to DOS helper
programs through the A-machine's `SYSTEM` instruction. `sys()` in `exe.c` used
to throw the command string away — the `system()` call is compiled out under
`#if 0` — so such games ran as pure text. Under `SPATTERLIGHT`, `sys()` now
hands the command to `glkmedia_system()` (`glkmedia.c`), which emulates the
helpers; other builds keep the no-op behaviour.

The reference game is **The Hollywood Murders** (Michael Zerbo, 1996, Alan 2.6;
see the version support added in `03d45e85`). It shells out to two DOS
utilities that ship alongside the `.acd`:

| helper | what it is | invocation used by the game |
| --- | --- | --- |
| `VIEWER.EXE` | *Viewer 2.2* by Mark Stehr, 1994. VESA VBE 1.2 SVGA image viewer for "BMP, FLI/FLC, GIF, ICO, IFF, PCD, PCX, Targa, TIFF". Centres the image, waits for a key, restores text mode (`-g` "stay in graphics mode" is **not** passed). | `viewer <name>.pcx` |
| `SBPLAY.EXE` | *SBPlay 2.11* by John A. Ball, 1995. Sound Blaster player for "VOC, WAV, SND, IFF, AIF, SAM & MOD". Blocks until the sample has finished. | `sbplay <name>.wav /s` (`/S` = suppress file information, i.e. quiet) |
| `ShowJPG.exe` | *ShowJPG* by Jan Patera. The same idea for JPEGs — used instead of `VIEWER.EXE` by *A Matter of Time* 1.2, whose art was re-rendered as JPEG for the 2003 re-release. | `showjpg <name>.jpg` |

A third command, `pause`, is not a program in the game directory at all — it is
`COMMAND.COM`'s built-in "Press any key to continue . . .".

The game's own `graphics on/off` and `noises on/off` verbs gate the `SYSTEM`
calls inside the acode, so the Glk implementation gets those for free.

## What the game asks for

91 `SYSTEM` call sites in `Hm.acd`, extracted by scanning for the `I_SYSTEM`
opcode word `0x10000035` and reading the two preceding `PUSH const` words as
`(len, fpos)` into `Hm.dat`:

- 30 × `viewer <file>.pcx` over 24 distinct files
- 36 × `sbplay <file>.wav /s` over 17 distinct files
- 25 × `pause` (24 bare, one as the literal `pause $n $n` — `getstr()` does not
  expand Alan's `$n` escape, so the dollar signs really do reach the command
  string; the trailing junk is ignored)

The dominant idiom is `pause` immediately followed by `viewer`: hold the text on
screen until the player presses a key, *then* flip to graphics. So `pause` is
part of the picture-display sequence, not an independent effect. The whole
session is bracketed by `viewer title.pcx` (an IFF file despite the name) and
`viewer share.pcx`, with `gun.wav` and `lady.wav` in between.

### Media inventory (shareware build)

Only 7 of the 24 referenced pictures and 9 of the 17 referenced sounds are
present; the rest were held back for the registered version. Two shipped files
are not what their names claim:

| file | on disk | notes |
| --- | --- | --- |
| `carpark`, `harold`, `loan`, `old`, `secret`, `share` | `CARPARK.PCX`, `Harold.pcx`, `LOAN.PCX`, `Old.pcx`, `SECRET.PCX`, `SHARE.pcx` | PCX version 5, 640×480, 8 bpp, 1 plane, RLE, with a trailing 769-byte VGA palette block |
| `title.pcx` | `title.pcx` | **IFF/ILBM, 640×400** despite the `.pcx` name — Viewer 2.2 sniffed content, so this worked in DOS |
| `car`, `clicker`, `dial`, `gun`, `hello`, `lady`, `suspense`, `switch` | same names | RIFF/WAVE, PCM, mono, 8-bit; 8000–22050 Hz |
| `nuthin.wav` | `nuthin.wav` | **plain CRLF text** — a copy of the shareware blurb, misnamed |
| the other 17 pictures and 8 sounds | — | absent |

On-disk case does not match the command strings (`CARPARK.PCX` vs
`carpark.pcx`), because DOS did not care.

## How it is implemented (`glkmedia.c`)

- **Parsing.** The verb is the first blank-separated word, the argument the
  first following word that does not start with `-` or `/` — switches can
  come before the file name (*A Matter of Time* runs `viewer -f3 title.pcx`)
  or after it (`sbplay <name>.wav /s`). `viewer`, `showjpg` and `sbplay`
  dispatch; everything else, `pause` included, is silently ignored (nothing
  is ever executed).
  Lookup in the game directory (dirname of `advnam`) is case-insensitive;
  absent files degrade to silence with no error text. `ALAN2_MEDIA_DEBUG=1`
  in the environment traces every `SYSTEM` string to stderr.

- **Pictures.** A file whose magic says the app can already read it (JPEG,
  PNG, GIF) is handed over as it lies — that is the `showjpg` case. Otherwise
  PCX (8-bpp VGA and 1-bpp/≤4-plane EGA) and IFF (planar ILBM
  and chunky `PBM `, byterun or uncompressed) are decoded by file magic, not
  extension, and written once per session to a temp 8-bpp BMP —
  a format ImageIO reads natively. The file is registered under a small
  resource number with `win_loadimage(resno, path, 0, len)`
  (`glkimp/connect.c`), after which the standard `glk_image_*` calls hit the
  app-side cache and the `PIC<n>` fallback in `loadimage()` is never
  consulted. Display draws the picture **inline in the buffer window**, on a
  paragraph of its own, with `glk_image_draw_scaled_ext()` (Glk 0.7.6
  `GLK_MODULE_IMAGE2`) under `imagerule_WidthOrig | imagerule_AspectRatio`
  and `maxwidth` 0x10000: original size, aspect ratio kept, never wider than
  the window, and re-resolved on every re-layout, so a 640×480 screenshot
  fits and follows a window resize. Unlike the DOS full-screen flip there is
  nothing to flip back from, so no keypress is waited for — the picture stays
  in the transcript where the game put it. Hosts that cannot draw into a
  buffer window (`gestalt_DrawImage`/`gestalt_Graphics`) are left alone
  before any output is emitted, so their transcripts are unchanged. Honours
  `gli_enable_graphics`.

- **Title screens.** The pictures a game shows *before it has printed a word*
  are the exception: those are title screens, and DOS gave them the whole
  screen and a keypress. So do we. Until the first non-white-space character
  reaches `glkio_printf()` the run is in its *title phase*, and a picture
  drawn during it opens a `wintype_Graphics` window over the **root**
  (`winmethod_Above | winmethod_Proportional`, size 100), so it covers the
  status line too, and holds it there until the player presses a key or
  clicks in it. The window is black and the picture is scaled to fit it —
  up as well as down, aspect ratio kept, centred — and redrawn on
  `evtype_Arrange`, so it follows a resize. Consecutive title pictures reuse
  the one window rather than closing and reopening it (*The Hollywood
  Murders* and *Inner Demons* show two, the others one).

  The phase ends — and the window closes, uncovering the buffer window with
  the game's first paragraph in it — at the first real text, or at a prompt
  (`glkmedia_flush_output()`) if the game somehow reached one without
  speaking. Every later picture goes inline as above. `glkmedia_reset()`,
  called from `init()`, starts a fresh title phase, so a `restart` shows the
  title screens again.

  There is no count of title images anywhere: "before any text" is the whole
  rule, and it generalises because the illustration idiom below always
  prints the room or object description *first*. The blank lines the games
  print to clear the DOS screen (~26 of them) are white space and so count
  as nothing. Where the host will not give up a graphics window, or cannot
  take character input in one (`gestalt_GraphicsCharInput` — the keypress is
  then read from the buffer window underneath, which the split has squeezed
  to nothing but which still has the input focus), the picture falls back to
  being drawn inline.

- **`pause`.** Ignored. It is not an independent effect: the games use it to
  hold the text on screen before VIEWER.EXE flips the whole screen to
  graphics, so it sits directly in front of a `viewer` call — the acode
  idiom is *describe, pause, viewer, describe again* on the way back
  (`examine julie`). Drawing the picture inline wipes nothing, so there is
  nothing to hold, and a prompt there would just ask the player to press a
  key to reveal a picture that is already on its way into the transcript.

- **The repeated description.** The other half of that idiom is the second
  *describe*: VIEWER.EXE restored text mode on a cleared screen, so the
  author printed the text again for the player coming back from graphics.
  Inline, the first copy is still right there above the picture and the
  repeat is glaring.

  What gets repeated is not tidy, so paragraphs are the wrong unit for the
  comparison. In *The Hollywood Murders* `examine julie` repeats a single
  sentence, and the two copies differ by a trailing space. In *A Matter of
  Time* every room repeats its description *and* its object list, and on the
  way in the first copy is glued to the end of a much longer paragraph
  ("You decide that it is too dangerous for Clarisse … There appears to be a
  trail leading to the north"). What does hold in both games is that the
  words printed after the picture are the last words printed before it, so
  that is the test: split both sides into words, ignore white space
  entirely, and look for the repeat as a suffix of the run-up — longest
  match wins, so a repeat the game followed with something new is still
  recognised. Short matches are left alone (fewer than 4 words or 24
  characters is more likely a stray "Yes." or a bare room name than a
  description).

  So the text of the turn so far is remembered, and from the moment a
  picture is drawn the rest of the turn is held back instead of printed;
  at the end of the turn whatever repeats the run-up is dropped and the
  rest goes out. Everything the interpreter prints already funnels through
  `glkio_printf()`, which is where the filter sits (`newline()` was routed
  through it too, so paragraph breaks are seen as well); output to the
  status window is passed straight through, since `statusline()` uses the
  same funnel. Text can never be lost: whatever is held is resolved and
  released at the next prompt (`readline()`), at the next picture, at
  `terminate()`, and released untouched if it outgrows the buffer. The turn
  record restarts at the prompt too — otherwise it would begin with the
  `"> "` that `agetline()` prints *before* asking for input, followed by
  nothing at all for the line the player types, which the library echoes
  rather than sending through the funnel. A text-only host draws no
  picture, so nothing is ever held there and its transcript is untouched.

- **Sounds.** No conversion needed: `SoundHandler` sniffs formats from data
  and `NSSound` plays 8-bit mono PCM at all three sample rates. Files whose
  magic is neither `RIFF` nor `FORM` are rejected — this is what catches the
  misnamed text file `nuthin.wav`. Registration goes through
  `win_loadsound(resno, path, 0, len)`, playback through
  `glk_schannel_play()` — whose internal `loadsound()` issues the `FINDSOUND`
  that keeps the app's single `_lastsoundresno` slot in step; calling
  `win_playsound()` directly would play the wrong sample. `SBPLAY.EXE`
  blocked, so consecutive calls played sequentially in DOS; Glk playback is
  asynchronous and a second play on one channel cancels the first, so playback
  round-robins over three channels and short repeats (`clicker.wav` ×3)
  overlap instead of cancelling. Honours `gli_enable_sound`.

## Testing

Verified in the app (both *Hollywood Murders* title screens full-frame in the
graphics window, advanced by a keypress and by a mouse click, the window
closing on the first text and coming back after `restart`; share/secret
pictures drawn inline and re-fitting on a window resize; the repeated
description gone; radio playing `suspense.wav` audibly) and headless. Driving
the app from a script needs a real `CGEventPost` click — System Events'
`click at {x, y}` silently does nothing to a graphics window. The headless
CheapGlk harness (see the
`alan2` build recipe used for the 2.6 work) builds with `-DSPATTERLIGHT`
plus `-Itest`: `test/glkimp.h` is a stub of the Spatterlight extensions and
`test/headless_stubs.c` defines them, in the style of
`terps/scott/test/headless_stubs.c`. CheapGlk reports images as unsupported,
so scripted runs still exercise parsing, lookup and conversion, but drawing
is inert and adds nothing to the transcript. Since `pause` no longer
prompts, scripted input no longer needs a blank line per `pause` either.

Because nothing is drawn there, the headless build also never holds text
back, so the duplicate-description filter is invisible to it — which is
exactly the point (its transcripts are byte-identical to the ones from
before any of this). To exercise the filter headless, temporarily skip the
`gestalt_Graphics`/`gestalt_DrawImage`/`glk_image_get_info` guard in
`showPicture()` and diff the run against an unmodified one: every
difference should be a deletion, and every deletion an exact repeat of the
text just above it.

Useful sites: `examine julie` in the room north of the start hits `pause` +
`viewer secret.pcx` and is the single-sentence duplicate case; `turn on fan`
plays `switch.wav`; asking Harold about anything plays the rejected
`nuthin.wav`. *A Matter of Time* (`~/Downloads/Alan 2.6/AMattero/TIME1.ACD`,
Alan 2.5) covers the switch-before-name form `viewer -f3 title.pcx` and is
the harder duplicate case — a picture in every room, each repeating a
multi-paragraph block, so simply walking around it (`n`, `n`, `n`, `s`,
`n`, `look`) is the regression test. The same game's registered build
(`~/Downloads/Alan 2.6/A-Matter-of-Time_DOS_EN_v12/Time1.acd`, v1.2 — the
sibling `Old\Time1.ACD` is the older DPMI-free build the README tells you
to fall back to, same media) is the `showjpg` case: 14 JPEGs, 5 WAVs, and
no missing files at all. 2.8 regression against `bugged.acd` and
`chasing.acd`.
