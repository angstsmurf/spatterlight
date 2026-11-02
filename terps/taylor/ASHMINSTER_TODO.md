# TODO: support the earlier Ashminster games

How to add *Dodgy Geezers*, *Terrormolinos*, *Hampstead* and *Denis Through the
Drinking Glass* to this interpreter. [Appendix C of FORMAT.md](FORMAT.md#appendix-c-earlier-games-on-the-same-engine)
has the summary; this file has the detail and the plan.

Everything below comes from reading the images statically. None of these games
has been run, in an emulator or otherwise, and no opcode meanings have been
worked out, so treat the addresses as a starting point.

## Order of work

1. *Dodgy Geezers*, Spectrum: the same table layout as the supported games, and
   the pictures are already decoded.
2. *Terrormolinos*, Spectrum: the same table layout, plus a text encoding and a
   vector picture format to add.
3. BBC Micro *Dodgy Geezers*, *Terrormolinos*, *Hampstead* and *Denis Through the
   Drinking Glass*: optional. They need a new file format and, for *Terrormolinos*
   and *Hampstead*, a different table layout. The layout of *Denis* is not known.

Keep all of this in `terps/taylor`. The table walker, parser, save code and test
suites can be shared. PAW and the Quill have a different structure, so
`terps/unquill` is not a better base.

## What is known

### Dodgy Geezers, Spectrum

`Dodgy Geezers.tzx`, standard blocks `code` and `code2`. These are the two parts
of the game, 40960 bytes each, loaded at &6000. The BASIC loader enters at &609C
(24732); how Part 2 is entered has not been checked.

| | Part 1 | Part 2 |
|---|---|---|
| Text | &7000 | &7100 |
| Tokens | &B145 | &B4F0 |
| Status table | &A398 | not located |
| Event table | &A939 | not located |
| Exits | &B67C | &B010 |
| Dictionary | &B33B | &B12F |
| Picture routine | &6CC2 | &6C8B |
| Picture pointers | &6E4C | &6E15 |

- **Tables:** bit-7 layout ([§10.1](FORMAT.md#101-table-layout)), status entries
  have no head. The BBC operand thresholds are: conditions 1 operand below 15,
  otherwise 2; actions none below 12, one below 21, otherwise 2. The Z80 code has
  `FE 0F D8`, which matches for conditions. Check the actions on the Spectrum.
- **Dictionary:** XOR `42`, then 4 letters and a value, ending `XXXX FF`. GET is
  100. The XOR is in the original game (`EE 42` in the Z80 code).
- **Text:** XOR `4A`, then each byte indexes a 256-entry bit-7 token table. Tokens
  2 and 3 are "." plus a terminator, and a string ends at a decoded byte below 4.
  Byte 5 is an escape that takes the following byte.
- **Exits:** `80+room`, then pairs. Checked on the BBC layout only.

**Pictures:** 10 in Part 1 and 8 in Part 2, 192×128 pixels, drawn at column 4 in
the top 16 character rows. Rendered and recognisable, not compared with an
emulator. The handler at &6890 takes the picture number (from 1) in B. Picture 10
in Part 1 calls &6880 first.

Pixels run until 3072 bytes are written, top row first. A byte whose low nibble
isn't 5 or A is a literal.

| Code | Output |
|---|---|
| `05 n` | n zero bytes (n = 0 means 256) |
| `0A n` | n `FF` bytes (n = 0 means 256) |
| `15`–`75` | 2–8 zero bytes |
| `1A`–`7A` | 2–8 `FF` bytes |
| `95 v n` | n copies of v |
| `85` | The sequence `0A 0C 15 1F 6A E0`, expanded by these same rules |
| `A5 B5 C5 D5 E5 F5 8A 9A AA BA CA DA EA` | The pair at that index in `00 03`, `80 00`, `00 01`, `15 80`, `FF 00`, `03 FF`, `01 15`, `25 01`, `FF FE`, `80 35`, `00 80`, `01 00`, `1A FE`, expanded the same way |

The list and pair table are at &6E3F and &6E25 in Part 1. Attributes follow the
pixels at once: bit 7 clear is a literal, bit 7 set repeats the last value (low 7
bits are the count), `FF` ends. There are 384 of them, 24×16.

### Terrormolinos, Spectrum

`Terrormolinos!.tzx`. The loading screen block holds the loader, run with
USR 16384. It reads two headerless blocks, 22045 bytes at &5CCB and 19535 at
&B3B0, then jumps to &A8E7.

| | Address |
|---|---|
| Text | &5CCB |
| Tokens | &93D4 |
| Exits | &9505–&96C2 |
| Dictionary | &96C3 |
| Status table | &99DE, 81 entries, ends `7F` at &9E25 |
| Event table | &9E26, 229 entries, ends `7F` at &A82C |
| Table walker | &AA32 |
| Picture entry | &B3B0 (A = 1–20) |
| Picture pointers | &B41B |
| Picture decoder | &FA34 |

- **Tables:** bit-7 layout, but each status entry starts `7E 7E`. Conditions take
  1 operand below 14, otherwise 2. Actions take none below 13, one below 25,
  otherwise 2.
- **Dictionary:** plain ASCII, 158 entries, ending in zero bytes, with no
  `XXXX FF`. The order is `N`, `NORT`, `S`.
- **Text:** literals, then tokens from `80` (bit-7 ended). `16` is a newline and
  `1F` ends a string.
- **Exits:** `80+room`, a count byte, then pairs. 79 rooms, no `FF`. The code stops
  at the address &96C3.

**Pictures:** 20, rendered and recognisable, each ending exactly where the next
begins. Not compared with an emulator. The border is magenta below picture 11,
otherwise red. Pictures 11/17 and 14/18 share data. Picture 19 is then mirrored,
pixels only (&B3F5).

The 9-byte header is:

| Offset | Contents |
|---|---|
| +0 | Word: end of the bitstream, from the picture start |
| +2 | Bit offset of the end |
| +3 | Word: attribute offset |
| +5–8 | Nibble-width pairs for xy-x, xy-y, x-only, y-only |

- **Bitstream:** MSB first from +9, starting at x = y = 0. One bit gives the
  operation (1 plot, 0 line from the current point). Then 1 means x and y both
  change, 00 x only, 01 y only.
- **Coordinates:** a selector bit picks the width (0 low nibble, 1 high nibble). A
  width of 8 is absolute, anything else a signed delta.
- **Lines:** these must match the original or fills leak. Step the major axis, start
  the error at major/2, add minor, and step minor at >= major.
- **Fills:** after the stream, rounded up to a byte, come `y,x` flood-fill seeds.
- **Attributes:** at the attribute offset, rows and columns (24, 32), then three
  run-length passes. Ink: top 3 bits, plus low 5 as count-1. Paper: the same. Bright
  and flash: top 2 bits, plus low 6 as count-1.

### BBC Micro

| | *Hampstead* | *Terrormolinos* | *Dodgy Geezers* |
|---|---|---|---|
| Image | `Disc091-HampsteadSTT.ssd` | `Disc093-TerrormolinosSTT.ssd` | `Disc999-DodgyGeezers.ssd` |
| Files | `$.HAMPST2` | `$.TERROR2`, `$.TERROR4` | `$.Part1`, `$.Part2` |
| Entry layout | `FE verb noun nconds nacts`, `FF` any, `FD` end | The same without `FE`, 227 entries | Bit-7 layout |
| Text | Backwards, `7E`/`0D`, with tokens; not worked out | As Spectrum *Terrormolinos*, tokens at file offset 0x119A | As Spectrum *Dodgy Geezers* |

*Hampstead* loads at &1900 and is copied to &0900. Its tables are at &0CB7 and
&1758, its dictionary at &252B (202 entries). *Terrormolinos* `$.TERROR2` loads at
&0400 and `$.TERROR4` at &1D00. *Dodgy Geezers* loads at &1100 and is copied to
&0900.

226 of the 227 BBC *Terrormolinos* entries match the Spectrum event table, ignoring
verb, noun and action 24. The two use different word numbers.

## Still to find out

Do these first, in MAME, with the Z80 code alongside
(`sna2skool.py`, see [test/README.md](test/README.md)).

- [ ] Confirm that both Spectrum games load and play, and capture real screens.
- [ ] Locate the rest of the data for both games: rooms, objects, messages, initial
      object locations, initial flags. Part 2 of *Dodgy Geezers* also needs its
      status and event tables.
- [ ] Find the opcode meanings from the condition and action routines. Don't assume
      any existing numbering. The BBC Questprobe 3 source
      ([Appendix A](FORMAT.md#appendix-a-the-bbc-micro-source)) helps with names.
- [ ] Find the parser's word-slotting threshold ([§8.1](FORMAT.md#81-assigning-words-to-slots)).
- [ ] Find which opcode draws a picture, and how a room or event selects one.
- [ ] Find out how *Dodgy Geezers* moves from Part 1 to Part 2 and what carries over.
      This is the main design unknown.
- [ ] Read the routine at &6880 in *Dodgy Geezers*.
- [ ] Explain the 9 bytes between most Part 2 pictures. They start `03`, then often
      the following picture's length, so may be leftover headers.
- [ ] Check the original save format, for [§13.4](FORMAT.md#134-original-spectrum-releases).
- [ ] Look at the Spectrum *Hampstead*, not examined yet, and the C64 releases of all
      three. Check whether `terps/unquill` already runs the Spectrum *Hampstead*.
- [ ] Check whether the BBC discs have pictures. None have been looked for.
- [ ] Find a BBC Micro or Electron image of *Denis Through the Drinking Glass*. IFWiki
      says Roger Taylor's company, Applications Software Specialities, released it on
      this engine, replacing a BASIC port. Compare its tables with *Hampstead* and
      *Terrormolinos*. Not examined yet.
- [ ] Look for the unfinished BBC Micro and Electron port of *Tower of Despair*, named
      by IFWiki. It may not survive. The Spectrum release is plain Quill, so it is out
      of scope.
- [ ] Check whether the C64 and Amstrad CPC releases of these games use this engine.
      IFWiki says the engine was ported to both, but doesn't say which games. CPC
      versions would be a separate job, as this interpreter has no CPC support.
- [ ] Check other games by reading their tables. IFWiki's list is the only source, and
      may be incomplete.

## Implementation

### Loading (`extracttape.c`, `gameload.c`)

- [ ] Add each game to `ProcessFile()`. *Dodgy Geezers* has two 40960-byte images, so
      decide how the second one is kept. *Terrormolinos* has headerless blocks, which
      `process_tzx_extract()` may need to be told about.
- [ ] Support `.z80` and `.sna` snapshots too, as for the other games.
- [ ] Remove the XORs at load time, before `FindVerbTable()`, so the dictionary and
      text code see plain bytes. Identify the game by a code signature first.
- [ ] Extend detection. `FindVerbTable()` searches for `NORT\001N`, which neither game
      has, and `DetectGame()` relies on the distance from dictionary to tokens.

### Registry (`taylor.h`, `gameinfo.c`, `gamedefs.h`)

- [ ] Add a `GameVersion` variant for each engine revision, plus game IDs and
      `games[]` rows. Addresses go in those rows, game-specific numbers in `gamedefs.h`.
- [ ] Add picture fields to `GameInfo` rather than reusing the tile and block ones.

### Tables and opcodes (`actions.c`, `player.c`)

- [ ] Add translation tables like `Q3Condition`, mapping to the canonical enums.
- [ ] Give `ConditionParamCount()` and `ActionParamCount()` the new thresholds. Actions
      currently have just one configurable boundary; these games use different ones.
- [ ] Skip the `7E 7E` status head in *Terrormolinos*. Questprobe 3's status padding
      may be a useful model.
- [ ] Implement any opcode with no counterpart.

### Text and parser (`textoutput.c`, `parseinput.c`)

- [ ] Add string ends below 4 and the escape byte 5 (*Dodgy Geezers*).
- [ ] Add literals, tokens from `80`, `16` newline and `1F` end (*Terrormolinos*).
- [ ] Read the dictionary without `XXXX FF` for *Terrormolinos*.
- [ ] Set the word-slotting threshold. Add the exits with a count byte and no `FF`,
      perhaps by rewriting them to the usual form at load time.
- [ ] Set the system message numbers, the direction limit and the no-match replies.

### Pictures (`graphics.c`)

- [ ] Add the *Dodgy Geezers* bitmap decoder and the *Terrormolinos* vector decoder.
      Both can draw through `PutPixel()` or an existing buffer path.
- [ ] Keep the layout: a 192×128 window in *Dodgy Geezers*, full-screen pictures with
      a border in *Terrormolinos*. Resize and restore redraws need checking against
      `DrawBufferedPicture()`.
- [ ] Clean the scripts in the session scratchpad into C. They aren't in the repo, so
      the tables above are the reference if they are gone.

### Saves and extras (`restorestate.c`, `extracommands.c`)

- [ ] Make sure autosave, undo and saves include the current part of *Dodgy Geezers*.
- [ ] Check the extra commands against each game's vocabulary.

### BBC Micro, if wanted

- [ ] Add a DFS reader for `.ssd` and check that the app accepts the extension.
- [ ] Add a walker for count-prefixed entries, or convert them at load time to the
      bit-7 form (preferred, as the rest of the code then stays unchanged).
- [ ] Work out the backwards text of *Hampstead*.
- [ ] Add *Denis Through the Drinking Glass*, once an image has been examined.

## Tests

Follow [test/README.md](test/README.md).

- [ ] Add the images to the corpus manifest, and a walkthrough of each game to
      `scriptcheck`. No walkthroughs have been looked for yet.
- [ ] Add every picture to `imagecheck`, with goldens checked against MAME captures
      through `mamecheck`.
- [ ] Add cut-short files to `damagecheck`.
- [ ] Run all the suites before and after each step, since the shared code changes.
      New `.c` files need a pbxproj entry.

## Documentation

- [ ] Move the confirmed format details to FORMAT.md, expanding Appendix C or adding
      to [§1.1](FORMAT.md#11-engine-variants), then delete this file.
- [ ] Add the games to the root `CHANGELOG.md`.
