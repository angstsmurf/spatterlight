# Testing terps/taylor

The Taylor sources are in the app target, so a new `.c` file needs a pbxproj
entry (`plutil -lint` the staged project; do not use `git apply
--unidiff-zero` on it). `Makefile.headless` picks up `*.c` by wildcard. Run
all of this before and after a change:

```
cd terps/taylor
make -f Makefile.headless imagecheck mamecheck scriptcheck damagecheck totcheck
xcodebuild -project ../../Spatterlight.xcodeproj -scheme Spatterlight build
```

About fifteen seconds without the MAME capture. The suites and their data
are described next to them:

- `images/README.md`: `imagecheck`, every picture and room of every corpus
  file (Spectrum `.z80`/`.sna`/`.tap`/`.tzx`, C64 `.d64`/`.t64`, `.tay`)
  against a golden, with the Rebel Planet and Kayleth animation frames
  fired by hand; `mamecheck`, the room pictures pixel by pixel against
  screens of the original Spectrum games captured in MAME
  (`mame_capture.py`, half an hour, gitignored); `damagecheck`, files with
  unreadable pictures must play on and files cut short must not crash or
  hang; `totcheck`, the Temple of Terror version menus.
- `scripts/README.md`: `scriptcheck`, the walkthroughs of all six games on
  every release, plus the extra commands, `worn-drop` and the UITest
  script, as transcript hashes through `taylor_hl`.

Not covered by any of them: the animations as they run in the app (the
headless builds have no timer; the probe fires ticks by hand), the C64 and
tape failure paths beyond "does not crash", and window sizes (the suites do
not size windows, which is how a 255-pixel picture width went unnoticed).
The script runner discards stderr. macOS has no `timeout`; use
`perl -e 'alarm shift; exec @ARGV'`.

## Behaviour settled by the passes of October 2026

The interpreter was refactored between 2026-10-12 and 2026-10-17 and
checked against the Z80 code of the Spectrum releases (Ghidra headless with
`-processor z80:LE:16:default`; skoolkit's `sna2skool.py` is faster for a
look) and the BBC BASIC source of Questprobe 3 (`~/Downloads/f4-*.ssd`).
Addresses are He-Man's unless said otherwise: conditions at 0x68E3, actions
at 0x6C45, flags at 0x5D5C; Questprobe 3 conditions 0x96A8, actions 0x991C,
flags 0x5B70. These are decisions; a change to any of them wants a test row.

- **`Chance(n)` succeeds when the draw is below n**, not at or below. The
  non-QP3 games take floor(100 * b / 256) from an 8-entry table indexed by
  FRAMES & 7; QP3 takes FRAMES mod 100. The fix moved nine He-Man and eight
  Rebel Planet rows, re-blessed; both still end as before.
- **GET and DROP.** DROP of a worn object puts it in the room silently;
  GET of a worn object prints "You are wearing it." A failed GET or DROP
  sets the stop flag and ends the table scan in every game. An action with
  bit 6 set stops the scan right after itself: in He-Man a LOOK line went
  on to `LET 4 11` and raised MaxCarried from 7 to 11 on every LOOK.
  `worn-drop` pins the first two.
- **Action 8 (PRINTOK) is a silent DONE.** No non-QP3 game ever prints the
  OKAY message; the only `LD A,0x13` is a store inside the text output.
  Six lines use it and no script reaches them, so there is no row.
- **Questprobe 3 has no worn objects** (its CARRIED and inventory test
  only flag 2; `Worn()` is 0 there, a real room), has 49 objects (the
  Spectrum copies 49 locations and lists 0 to 48; only its INVENT loop
  stops at 47, and object 48, the fallen statue, is only ever SWAPped with
  47 so the off-by-one cannot show), and copies 4 flags from the file where
  the other games copy 7.
- **Tables end** 0xFF (exits) and `XXXX` + 0xFF (dictionary) in every
  Spectrum snapshot; the code once had 0xFE and 0x7E. The QP3 status-table
  padding 0x7E equals `MSG_END` but is not a terminator and has its own
  name.
- **Pictures are 256 pixels wide** (32 cells; Blizzard Pass widens its
  from 16). The old 255 was copied from Scott's Mysterious pictures and
  clipped the last column at some window widths.
- **Kayleth** has 120 objects; `ObjectLoc[122]` is spare storage for the
  droid on the belt (`KAYLETH_DROID_SLOT`) and is set to `Destroyed()`,
  not the condition enum of the same name. Moving object 0 into a room is
  how the animation table says "always animate" (13 of 22 animated rooms).
- **Rebel Planet** parks the player in room 250, which does not exist,
  for GET X FROM CUBE and DROP X IN KUBE until the next turn; Kayleth's
  AZAP CODE ELY moves the player before the chamber's description object
  follows. Both are skipped in `RunOneInput`, not drawn.
- **Temple of Terror** on C64 holds both versions on one image and
  `terror_menu` offers graphics, text only and the hybrid; the Spectrum
  tape asks whether to combine the two sides when the other is next to the
  file. `CurrentWindow` must be `Bottom` when gameload.c asks, or the
  prompt loses its spaces. The combined version plays exactly like the
  text-only one.
- **`FindTokens`** searches with `Game`'s signature and falls back to its
  `start_of_tokens`; `DetectGame` passes no signature, because a
  candidate's own would let a Spectrum file match a C64 row. The generic
  search starts at offset 2.
- **The registry's "Unknown game" row** is never tried by `DetectGame`
  (all its offsets are 0). `TokenClassify` is gone: a probe showed it never
  fired on the corpus, and it would have set the wrong type.
- **Menu text** is a transcript change: fixing "one with without pictures"
  re-blessed 72 C64 Temple of Terror rows. Say so when committing such a
  thing.

Per-game flags, objects, rooms and words are named in `gamedefs.h` (QP3,
Temple of Terror, Blizzard Pass, He-Man) and `animations.c` (Rebel Planet,
Kayleth). `FORMAT.md` is the file format against the code; `ChangeLog` is
still the 0.3-era text.

## Declined, with the reason

- A global state struct in place of `Flag`, `ObjectLoc`, `Game`, `FileImage`
  and the `Version` macros: large and risky for a game-data interpreter.
  An output-state struct in textoutput.c, or merging `Upper` and
  `Q3Upper`: transcript goldens for no gain.
- `ExecuteLineCode`/`PerformAction` returning an int and a `*done`
  out-parameter: the two signals mean different things (stop this line,
  stop the table).
- A `strcasecmp`: POSIX-only, so `EqualsIgnoringCase()` in utility.c.
- One helper for the three ways restorestate.c writes Flag and ObjectLoc:
  the save file layout reads off the code as it is.
- One `ExtractFromContainer` for the D64 and T64 branches of `DetectC64`:
  they differ in what they free and keep.
- `ExtractImage`'s stack buffer (the length is only known after
  decompressing) and the six-case Blizzard Pass width switch.
- Inlining `Destroyed()`, `Carried()`, `Worn()`: `HeldByPlayer` covers the
  common use. decrypttotloader.c stays a literal transcription of the Z80
  passes. The `*_loadscreen_data.h` arrays are only included once.
