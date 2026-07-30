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
  or after it (`sbplay <name>.wav /s`). `viewer`, `sbplay` and `pause`
  dispatch, anything else is silently ignored (nothing is ever executed).
  Lookup in the game directory (dirname of `advnam`) is case-insensitive;
  absent files degrade to silence with no error text. `ALAN2_MEDIA_DEBUG=1`
  in the environment traces every `SYSTEM` string to stderr.

- **Pictures.** PCX (8-bpp VGA and 1-bpp/≤4-plane EGA) and IFF (planar ILBM
  and chunky `PBM `, byterun or uncompressed) are decoded by file magic, not
  extension, and written once per session to a temp 8-bpp BMP —
  a format ImageIO reads natively. The file is registered under a small
  resource number with `win_loadimage(resno, path, 0, len)`
  (`glkimp/connect.c`), after which the standard `glk_image_*` calls hit the
  app-side cache and the `PIC<n>` fallback in `loadimage()` is never
  consulted. Display opens a `wintype_Graphics` split above the buffer window
  (transcript survives, unlike the DOS full-screen flip), sized via
  `glk_window_set_arrangement()` to the picture's scaled height, draws
  centred and letterboxed on black, waits for a key, and closes the split.
  Honours `gli_enable_graphics`.

- **`pause`.** Prints a `[Press any key to continue]` prompt (not DOS's text)
  and blocks on `glk_request_char_event()`/`glk_select()` right at the call
  site, so the interpreter's text-file position is never disturbed. Arrange
  and Redraw events redraw the status line (and the picture, while one is up).

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

Verified in the app (title/share/secret pictures, pause prompt, radio playing
`suspense.wav` audibly) and headless. The headless CheapGlk harness (see the
`alan2` build recipe used for the 2.6 work) builds with `-DSPATTERLIGHT` plus
`-Itest`: `test/glkimp.h` is a stub of the Spatterlight extensions and
`test/headless_stubs.c` defines them, in the style of
`terps/scott/test/headless_stubs.c`. CheapGlk refuses graphics windows and
reports images as unsupported, so scripted runs still exercise parsing,
lookup, conversion and the pause prompt, but drawing is inert. Useful sites:
`examine julie` in the room north of the start hits `pause` + `viewer
secret.pcx`; `turn on fan` plays `switch.wav`; asking Harold about anything
plays the rejected `nuthin.wav`; 2.8 regression against `bugged.acd` and
`chasing.acd`.
