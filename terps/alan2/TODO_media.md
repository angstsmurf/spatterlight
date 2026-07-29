# TODO: map Alan 2 `SYSTEM` media calls to Glk images and sound channels

Some Alan 2 games drive graphics and sound by shelling out to DOS helper
programs through the A-machine's `SYSTEM` instruction. `sys()` in `exe.c` builds
the command string and then throws it away — the `system()` call is compiled out
under `#if 0` — so such games currently run as pure text.

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
calls inside the acode, so a Glk implementation gets those for free.

## What the game actually asks for

91 `SYSTEM` call sites in `Hm.acd`, extracted by scanning for the `I_SYSTEM`
opcode word `0x10000035` and reading the two preceding `PUSH const` words as
`(len, fpos)` into `Hm.dat`:

- 30 × `viewer <file>.pcx` over 24 distinct files
- 36 × `sbplay <file>.wav /s` over 17 distinct files
- 25 × `pause` (24 bare, one as the literal `pause $n $n` — `getstr()` does not
  expand Alan's `$n` escape, so the dollar signs really do reach the command
  string; the trailing junk must be ignored)

The dominant idiom is `pause` immediately followed by `viewer`: hold the text on
screen until the player presses a key, *then* flip to graphics. So `pause` is
part of the picture-display sequence, not an independent effect.

Occasionally two `viewer` calls run back to back with only one `pause` in front
(`trees.pcx` then `treehous.pcx`), and sounds are sometimes issued two or three
times in a row (`clicker.wav` ×3 at one site) — in DOS those played strictly
sequentially because `SBPLAY.EXE` blocked.

### Media inventory (shareware build)

Only 7 of the 24 referenced pictures and 9 of the 17 referenced sounds are
present; the rest were held back for the registered version ("includes many
additional graphics, sound effects and the complete storyline" — `README.TXT`).
Two shipped files are not what their names claim:

| file | on disk | notes |
| --- | --- | --- |
| `carpark`, `harold`, `loan`, `old`, `secret`, `share` | `CARPARK.PCX`, `Harold.pcx`, `LOAN.PCX`, `Old.pcx`, `SECRET.PCX`, `SHARE.pcx` | PCX version 5, 640×480, 8 bpp, 1 plane, encoding 1 (RLE), bytes-per-line 640 — i.e. 256-colour with a trailing 769-byte VGA palette block |
| `title.pcx` | `title.pcx` | **IFF/ILBM, 640×400** despite the `.pcx` name — Viewer 2.2 sniffs content, so this worked in DOS |
| `car`, `clicker`, `dial`, `gun`, `hello`, `lady`, `suspense`, `switch` | same names | RIFF/WAVE, PCM, mono, 8-bit; 8000 Hz (`car`), 11025 Hz (`dial`, `gun`), 22050 Hz (the rest) |
| `nuthin.wav` | `nuthin.wav` | **plain CRLF text** — it is a copy of the shareware blurb, misnamed |
| the other 17 pictures and 8 sounds | — | absent |

On-disk case does not match the command strings (`CARPARK.PCX` vs
`carpark.pcx`), because DOS did not care. Lookup must be case-insensitive.

## Proposed implementation

### 1. Parse the command in `sys()`

Keep `sys()` (`exe.c:115`) as the single entry point, guarded by
`#ifdef SPATTERLIGHT` so non-Spatterlight builds keep today's no-op behaviour.
Split the command string on whitespace, lowercase `argv[0]`, and dispatch:

- `viewer` → show `argv[1]`
- `sbplay` → play `argv[1]`, ignoring `/s` and any other switch
- `pause` → wait for a key, ignoring any trailing words
- anything else → ignore silently (do not try to run it)

The media file lives next to the game: `advnam` (`main.c:92`) is the game path
with the extension stripped, so take its directory and scan it once at startup,
building a case-insensitive name → real-filename map. That also lets an
unrecognised extension fall through to whatever is actually on disk.

### 2. Pictures

The picture files are photographic 640×480 256-colour images. The
decode-to-pixels-then-`glk_window_fill_rect` approach used by `terps/comprehend`
(`comprehend.cpp:1257`) and `terps/scott` (`scott_display.c:166`) is fine for
flat-shaded line art but would emit six-figure rectangle counts here, so it is
the wrong tool.

Two better options, in order of preference:

**(a) Convert to a format the app already reads, and register the file.**
Spatterlight's image path is `glk_image_draw*` → `loadimage()`
(`glkimp/image.c:7`) → `win_loadimage(resno, filename, offset, len)` →
`ImageHandler`, which is just `[[NSImage alloc] initWithContentsOfFile:]`
(`application/Images/ImageHandler.m:136`). So anything ImageIO reads works —
including **BMP**, which an 8-bpp palettised PCX maps onto almost 1:1 (RLE
decode the rows, emit `BITMAPFILEHEADER` + `BITMAPINFOHEADER` + the 256-entry
palette + bottom-up rows). No PNG encoder needed.

No new glkimp entry point is needed to point a resource number at an arbitrary
path: `win_loadimage(resno, filename, offset, reslen)` (`glkimp/connect.c:582`,
declared in `glkimp/glkimp.h:157`) already takes a path, and `loadimage()` opens
with `if (win_findimage(image)) return TRUE;`. So the terp can register the
converted file itself and then use the standard API, exactly as `terps/taylor`
and `terps/unquill` call `win_beep_*` directly under `#ifdef SPATTERLIGHT`:

```c
#include "glkimp.h"
...
win_loadimage(resno, bmppath, 0, bmplen);   /* arbitrary path, no Blorb */
glk_image_draw_scaled(gfxwin, resno, ...);  /* find hits, PIC<n> never consulted */
```

The `PIC<n>` / `PIC<n>.jpg` fallback inside `loadimage()` is only reached when
the resource has *not* been pre-registered, so nothing else changes.

Assign each distinct media name a stable small resource number on first use, so
`win_findimage()`'s app-side cache does its job on the repeated pictures
(`harold`, `secret`, `wing`, `rec`, `murderer`, `bar` are each shown twice) —
i.e. `win_loadimage()` only needs calling once per name. Convert to a temp BMP
at the same time and keep it for the session.

**(b) Teach the app side to read PCX and ILBM directly** (an `NSImage`
subclass or a CGImageSource plug-in) and hand `win_loadimage()` the original
file. Cleaner in the long run and reusable by other terps, but
much more work than the ~100-line BMP writer, and it puts format decoding on
the wrong side of the protocol.

Either way `title.pcx` needs an IFF/ILBM path as well as PCX — 640×400, planar,
possibly byte-run compressed (`BMHD.compression`). Sniff the file's magic
(`0x0A` for PCX, `FORM....ILBM` for IFF) and ignore the extension, exactly as
Viewer 2.2 did.

### 3. Window layout

Current window tree (`glkstart.c:38-46`): root is a pair whose children are the
one-line `glkStatusWin` text grid, above `glkMainWin`, the text buffer.

DOS behaviour was a full-screen flip: whole screen becomes the picture, any key
returns to the text page. Reproduce that without destroying the buffer (and its
scrollback) by opening the graphics window **once**, lazily, on the first
`viewer`:

```c
glk_window_open(glkMainWin, winmethod_Above | winmethod_Fixed, h,
                wintype_Graphics, GLK_GRAPHICS_ROCK);
```

and then flipping its share of the split with
`glk_window_set_arrangement()` — the picture's scaled height while it is up, 0
(or `glk_window_close()`) once the player has dismissed it. `terps/scott`'s
`title_image.c:60` does the close-everything-and-reopen variant, which is fine
for a title screen but would lose the transcript here.

Scale to fit the available width with `glk_image_draw_scaled()`, preserving the
4:3 (and 8:5 for `title`) aspect ratio; see the Glk 0.7.6 `GLK_MODULE_IMAGE2`
scaling rule in `glkimp/protocol.h:28` before picking a rule.

Honour `gli_enable_graphics` (`glkimp/glkimp.h:26`) — the Spatterlight
"graphics" preference — as `terps/scott` does at `scott_display.c:214`. When it
is off, skip the picture entirely but still honour `pause`.

### 4. `pause` and dismissing a picture

- `pause` → flush pending text, print a `[Press a key]`-style prompt in
  `glkMainWin`, then `glk_request_char_event()` + `glk_select()` until a
  character arrives. Do not print DOS's "Press any key to continue . . .".
- After a picture is drawn, wait for a key again before flipping back to text,
  matching Viewer 2.2 and the `README.TXT` promise: "The game will load graphic
  screens periodically throughout the game. To switch back to text mode press
  the Space Bar." Accept any key, not just space.
- Be careful with re-entrancy: `sys()` can be reached from inside `print()`
  (`exe.c:53`), which saves and restores decoding state, so blocking for input
  there must not disturb the text-file position. Prefer draining the input at
  the point `sys()` is called rather than deferring it to the next
  `glk_select()` in `readline()`.

### 5. Sounds

Nothing needs converting: `SoundHandler` sniffs the format from the data
(`application/Sounds/SoundHandler.m:144`, `RIFF` → `GlkSoundBlorbFormatWave`)
and `NSSound` handles 8-bit mono PCM at all three sample rates. The arbitrary
path is already handled too, by the same trick as the pictures:
`win_loadsound(resno, filename, offset, reslen)` (`glkimp/connect.c:664`,
declared in `glkimp/glkimp.h:170`) takes a path, and `glk_schannel_play_ext()`
→ `loadsound()` (`glkimp/sound.c:12`) opens with
`if (win_findsound(sound)) return TRUE;`, so a pre-registered resource never
reaches the `<gamedir>/SND<n>` fallback.

Then, per `sbplay`:

1. resolve the name case-insensitively; skip silently if absent (8 of the 17)
2. reject files whose magic is not a format the app supports — this is what
   catches the misnamed text file `nuthin.wav`
3. `win_loadsound()` once per distinct name, then `glk_schannel_play()` on a
   channel created lazily with `glk_schannel_create()`

Note the app tracks a single `_lastsoundresno`:
`handlePlaySoundOnChannel:` plays whatever `handleFindSoundNumber:` /
`handleLoadSoundNumber:` last touched (`application/Sounds/SoundHandler.m:299`
onwards), not the resource number in the `PLAYSOUND` message. Going through
`glk_schannel_play()` is what keeps that in step, since its `loadsound()` call
issues the `FINDSOUND` that sets it. Calling `win_playsound()` directly would
play the wrong sample.

Honour `gli_enable_sound` (`glkimp/glkimp.h:27`).

**Open question — sequencing.** `SBPLAY.EXE` blocked, so consecutive `sbplay`
calls played one after another. Glk playback is asynchronous and a second
`glk_schannel_play()` on the same channel stops the first, so the three
consecutive `clicker.wav` calls would collapse into one audible click.
Recommended default: round-robin over three channels so short repeats overlap
rather than cancel — it is not faithful but it is what a player expects. The
faithful alternative is to play with a notify value and `glk_select()` until the
`evtype_SoundNotify` arrives, which reproduces DOS timing at the cost of
blocking the interpreter; `terps/unquill` deliberately went the other way for
Spectrum beeps (`condact.c:351`) and lets the app queue notes instead.

## Testing

The 640×480 assets and the two misnamed files make the shareware build a good
test case for the fallback paths, but only 7 pictures and 8 usable sounds are
reachable, and several are late-game. Worth reaching for:

- `viewer title.pcx` and `viewer share.pcx` bracket the whole session (the
  intro title screen and the closing shareware plug), so they are reachable
  immediately — and `title.pcx` is the IFF case.
- `sbplay gun.wav /s` and `sbplay lady.wav /s` fire between those two, so the
  end-of-game sequence exercises image and sound together.
- Every absent file must degrade to silence with no error text, since 17 of 24
  pictures and 8 of 17 sounds are simply not there.

The headless CheapGlk harness (see the `alan2` build recipe used for the 2.6
work) needs stubs for whichever `win_*` extensions get added, in the style of
`terps/scott/test/headless_stubs.c`.
