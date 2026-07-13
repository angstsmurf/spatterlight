# c64.com Scott Adams D64 support — TODO

Adding the ten `D64 scott from c64.com` disk images to the C64 detection
registry (`ai_uk/c64decrunch.c`) and the Babel identifier (`babel/scott.c`).

All ten are 35-track `.d64` (0x2ab00 bytes, except Pirate at 0x2adab). They are
the European Adventure International / Adventuresoft C64 releases — the same
family already partially imported (Adventureland, Gremlins, Robin of Sherwood,
Seas of Blood, Claymorgue are the *same* c64.com cracks already in the registry).

## Identification (size + 16-bit additive checksum)

| Game                     | size    | chk    | registry status |
|--------------------------|---------|--------|-----------------|
| Adventureland            | 0x2ab00 | 0x6638 | already present (`ADVENTURELAND_C64`) |
| Gremlins                 | 0x2ab00 | 0xabf8 | already present (`GREMLINS_C64`) |
| Robin of Sherwood        | 0x2ab00 | 0xcf9e | already present (`ROBIN_OF_SHERWOOD_C64`) |
| Seas of Blood            | 0x2ab00 | 0xe308 | already present (`SEAS_OF_BLOOD_C64`) |
| Sorcerer of Claymorgue   | 0x2ab00 | 0xfd67 | already present (`CLAYMORGUE_C64`) |
| Pirate Adventure         | 0x2adab | 0xbea0 | **DONE** (this branch) |
| Voodoo Castle            | 0x2ab00 | 0x7822 | **DONE** (text) — `-e0x840 -d0x7f50`, DB carved at imgoffset 0x3800 |
| Secret Mission           | 0x2ab00 | 0x167b | **DONE** — SECRET_MISSION_C64 (Irmak tiles), TYPE_D64 Section8 |
| Strange Odyssey          | 0x2ab00 | 0x12d2 | **DONE** (text) — generic imgoffset=0 path |
| The Count                | 0x2ab00 | 0x2827 | **DONE** (text) — `-d0x7f50`, DB carved at imgoffset 0x3800 |

## Verification harness

`terps/scott/saga/test/build/extract_images_c64 <disk.d64> <outdir>` runs the
real `DetectGame` → `DetectC64` → unp64 → binary-DB pipeline. To see whether a
DB actually parsed (vs. just matching the registry), add `-DDUMP_GAME_CONTENT`
to the C64 compile line in `saga/test/Makefile` and the temporary dump block in
`extract_images.c` (prints `HDR rooms=… items=…`, first messages, first rooms).
A correct load shows real room/message text; a bad one shows zeros or garbage.

The upstream UNP64 v2.39 (csdb.dk release 292844) builds on macOS and is the
ground-truth decompressor — run it on the extracted PRG to identify the packer
and produce a reference decompressed image to diff against.

## Done

- [x] **Pirate Adventure** (0x2adab / 0xbea0)
      `{ PIRATE_US, 0x2adab, 0xbea0, TYPE_US, 0, NULL, "PIRATE", 0,0,0,0, 0x6d30 }`
      Verified: rooms=26 items=66 msgs=88 words=79 actions=177, ROOM[1]="Flat in
      london". Disk has `saga`/`load1`/`pirate`; the named `PIRATE` file is the
      S.A.G.A. all-in-one image (handled by `handle_all_in_one`, imgoffset 0x6d30,
      same as the existing 0x04c5 Pirate entry).
- [x] **Strange Odyssey** (0x12d2) — text playable.
      `{ STRANGE_ODYSSEY_US, 0x2ab00, 0x12d2, TYPE_US, 1, NULL, "STRANGE ODYSSEY", 0,0,0,0, 0 }`
      The `STRANGE ODYSSEY` PRG is PUCrunch-packed; the fork's unp64 depacks it in
      one pass to a $8030-based image whose **S.A.G.A. preamble is at offset 0**, so
      the generic `LoadBinaryDatabase` (imgoffset 0) path loads it directly. No
      `handle_all_in_one` case needed; no separate image files on the disk (text
      only — graphics would need the all-in-one route). Verified:
      rooms=35 items=55 actions=223 words=79 msgs=94, ROOM[1]="one man scoutship",
      MSG[1]="Welcome to Adventure: 6 \"STRANGE ODYSSEY\" by Scott Adams."
- [x] **Voodoo Castle** (0x7822) and **The Count** (0x2827) — text playable.
      `{ COUNT_US,         0x2ab00, 0x2827, TYPE_US, 1, "-d0x7f50",         "THE COUNT",     1,0,0,0, 0x3800 }`
      `{ VOODOO_CASTLE_US, 0x2ab00, 0x7822, TYPE_US, 1, "-e0x840 -d0x7f50", "VOODOO CASTLE", 1,0,0,0, 0x3800 }`
      Both are all-in-one images: after decompression the S.A.G.A. database preamble
      sits at C64 $4000, so `handle_all_in_one` carves it at imgoffset 0x3800 (NOT
      0x3802 — the preamble is one word shorter than the canonical 0x38, so carving
      two bytes early re-aligns version/adv to the 0x34/0x36 slots `LoadHeader`
      expects). `handle_all_in_one` now loads text-only when no image table matches
      the layout (COUNT_US has none; the c64.com Voodoo layout differs from
      `listVoodoo`, which is gated to `cutoff == 0x6c30`). Verified in VICE that the
      depack is complete (decompressed RAM == unp64 output byte-for-byte at $4000 and
      in the message text), and the loaded text is correct:
        Count  rooms=22 items=72 — ROOM[1]="*I'm lying in a large brass bed",
               MSG[3]="Welcome to ADVENTURE: 5 \"THE COUNT\". Dedicated to Alvin Files."
        Voodoo rooms=25 items=65 — ROOM[1]="chapel",
               MSG[1]="Count Cristo's been CURSED!..."
- [x] Babel identifiers — all five c64.com checksums added to `babel/scott.c`
      (PIRATE 0xbea0, VOODOO 0x7822, SECRET_MISSION 0x167b, STRANGE_ODYSSEY 0x12d2,
      THE_COUNT 0x2827). Verified each by re-summing the disk bytes.

## How the depack-done ($7f50) and imgoffset (0x3800) were found

`vice_breakpoint_set` in the mcp-vice-emu server was broken (encoded checkpoint
addresses as 4-byte UInt32LE in a 15-byte body; VICE wants 2-byte UInt16LE — the
malformed CHECKPOINT_SET wedged/killed the monitor, and each failed reconnect
spawned a duplicate x64sc fighting over port 6502). Fixed in
`mcp-vice-emu/src/vice-protocol.ts` (`checkpointSet`, 9-byte UInt16LE body).
With breakpoints working: break at the relocated depacker entry ($8000 for Count),
set an exec watchpoint over $4000-$7fff, continue → the depacker's hand-off lands
at $7f50 (the shared S.A.G.A. interpreter cold-start; same for Voodoo). Passing
`-d0x7f50` makes both the fork's unp64 and v2.39 stop there and dump $0800-$ffff.
The DB preamble's C64 address ($4000) → output offset 0x3800 (= ($4000-$0800)+2-2).

## unp64 reality check (v2.36 local vs upstream v2.39, csdb 292844)

Built upstream v2.39 from source on macOS (`/tmp/unp64_239/.../src/Release/unp64`).
Both versions, and the fork's ScummVM port, agree:
- **Strange Odyssey** — PUCrunch, depacks clean, 1 pass. ✅
- **Secret Mission** — Section8 packer, depacks clean to `$4000-$cfff`. But that
  image is a kernal LOAD/SETLFS/CHROUT **title-and-loader stage** (the title
  "* ADVENTURE * (CBM64 V1.0) ADVENTURE #3" + restart text), *not* a standard
  28-word S.A.G.A. preamble + header. No offset in it parses as adv=3. The real
  interpreter+DB is loaded by that stage from elsewhere — needs a VICE/MAME trace
  of the second load to find the DB image.
- **Voodoo Castle** — DONE (text). See the [x] entry above: `-e0x840 -d0x7f50`,
  imgoffset 0x3800. Graphics still need a c64.com-specific image table (this
  crack's $0800-based layout differs from `listVoodoo`'s 0x6c30 layout).
- **The Count** — DONE (text). `-d0x7f50`, imgoffset 0x3800. The depacker DOES
  install an IRQ vector and unp64 hits "Max Iterations" with no `-d`; the fix was
  simply to give it the depack-done address $7f50 (found by tracing in VICE), at
  which point the IRQ loop has already terminated and unp64 dumps a complete image.
- **Secret Mission** — DONE, but it is NOT a US S.A.G.A. game like the others: it
  is the C64-native **SECRET_MISSION_C64** release (Irmak tile graphics), which is
  why a full scan of the depacked image found no US 0x38-preamble. The fix is to
  let the normal C64 path handle it: `{ SECRET_MISSION_C64, 0x2ab00, 0x167b,
  TYPE_D64, 1, NULL, NULL, 0,0,0,0, 0 }`. TYPE_D64 extracts the largest file
  (`SECRET MISSION`), ProcessC64 runs the Section8 depacker (1 iteration), then
  `GetId` finds the THREE_LETTER_UNCOMPRESSED dictionary (at 0x2c26) and
  `TryLoading` self-aligns via that dictionary position — no fixed imgoffset needed
  for the DB; imgoffset 0 for graphics matches the sibling T64 Section8 entry.
  Loads correct: rooms=23 items=49 actions=154 — ROOM[2]="briefing room",
  MSG[1]="Good morning Mr Phelps. Your Mission ... TIME BOMB!". (The decompressed
  $4000 image starting with a kernal loader + a `U1:` block-read string was a red
  herring — the C64 loader path reads the DB straight from the depacked buffer via
  GetId, it does not execute that stub. Irmak tile graphics should be confirmed
  visually in the app.)

## How the US S.A.G.A. loader picks the game (important)

The US binary database is **self-describing**. `LoadHeader` (saga.c, the
`dict_start == 0` branch) scans the 0x38-byte preamble for the version and
adventure number and does `CurrentGame = adventure_number` directly. The Scott
Adams adventure numbers line up with the enum: Adventureland 1, **Pirate 2 =
PIRATE_US**, **Secret Mission/Mission Impossible 3 = SECRET_MISSION_US**,
**Voodoo 4 = VOODOO_CASTLE_US**, **The Count 5 = COUNT_US**, **Strange Odyssey
6 = STRANGE_ODYSSEY_US**.

So none of the four below need a new `games[]` GameInfo entry — the generic US
loader reads counts/offsets straight from the file. (My earlier note that "The
Count needs a whole game definition" was wrong: it is simply `COUNT_US`.)

Two load routes for a TYPE_US disk:
- **imgoffset == 0** → `DetectC64` calls `LoadBinaryDatabase` on the extracted
  file directly. Generic; works for any adventure number incl. COUNT_US. Use
  this if the decompressed file *is* (or starts with) the S.A.G.A. DB.
- **imgoffset > 0** → `handle_all_in_one` (c64_small.c) carves the DB out of a
  combined interpreter+images+DB image and also builds the C64 graphics. NOTE:
  its `switch (CurrentGame)` only has `PIRATE_US` / `VOODOO_CASTLE_US` cases and
  `default: return 0` — so Count / Secret Mission / Strange Odyssey via this
  route need a `case` + an image-offset list (like `listPirate`/`listVoodoo`)
  added there, otherwise the load fails *after* the DB parsed fine.

## Open — C64 graphics for the all-in-one games

All ten disks are detected and all are **text-playable**; the per-game
decompression RE is done (see the table and the Done section above). The one
thing left is **graphics**:

- [ ] `handle_all_in_one`'s `switch (CurrentGame)` (`saga/c64_small.c:~238`) still
      has only `PIRATE_US` / `VOODOO_CASTLE_US` cases and `default: return 0`, so
      **The Count**, **Strange Odyssey** and the **c64.com Voodoo Castle** crack
      load text-only. Each needs a `case` plus an image-offset list (modelled on
      `listPirate` / `listVoodoo`). Note the c64.com Voodoo layout is $0800-based
      and differs from `listVoodoo`, which is gated to `cutoff == 0x6c30`.
- [ ] Secret Mission loads via the C64-native `SECRET_MISSION_C64` path (Irmak
      tile graphics) — those should be confirmed visually in the app.
