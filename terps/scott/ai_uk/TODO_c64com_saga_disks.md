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
| Voodoo Castle            | 0x2ab00 | 0x7822 | TODO |
| Secret Mission           | 0x2ab00 | 0x167b | TODO |
| Strange Odyssey          | 0x2ab00 | 0x12d2 | TODO |
| The Count                | 0x2ab00 | 0x2827 | TODO (needs engine support) |

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
- [ ] Babel identifiers — add all five new checksums to `babel/scott.c`
      (PIRATE_ADVENTURE_IFID / VOODOO_CASTLE_IFID / SECRET_MISSION_IFID /
      STRANGE_ODYSSEY_IFID / THE_COUNT_IFID). Pure size+checksum identification,
      independent of interpreter playability — safe to add for all five now.

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

## TODO — per-game RE

All four are US S.A.G.A. games loadable via the generic adventure-number path.
The blocker is the right unp64 parameters and S.A.G.A. memory layout for *this*
crack, plus (for graphics) a `handle_all_in_one` case.

- [ ] **Voodoo Castle** (0x7822) — single packed PRG `voodoo castle`, BASIC is
      `1 REM "* VOODOO CASTLE *" : 2 SYS 2112` ($0840). Detected as
      `VOODOO_CASTLE_US` but DB loads empty.
      - Suspect: the leading REM line stops unp64's SYS-finder, so decompression
        silently no-ops and `handle_all_in_one` reads still-packed bytes.
      - Tried `TYPE_US,"VOODOO CASTLE",imgoffset 0x6c30` with iterations 0/1/2/3
        and a `-e0x840` forced entry (parameter=1) — all still empty.
      - Next: decompress with upstream unp64 (force entry past the REM), confirm
        the S.A.G.A. signature, derive the correct `imgoffset`; check whether the
        fork's unp64 needs the forced-entry switch wired differently for TYPE_US
        (decompression runs inside the TYPE_US branch via `ProcessC64`).

- [ ] **Secret Mission** (0x167b) — single packed PRG `secret mission`, loader
      string "64738 '84 BY SECTI 8" → **Section8 packer** (scanner present).
      Likely **SECRET_MISSION_US** (adventure 3) via the US path, not the
      `SECRET_MISSION_C64` (UK Mysterious-style) I tried first.
      - Tried `SECRET_MISSION_C64` `TYPE_D64`, iters 0/1 × imgoffset {0,
        -0x1bff}: 0 → "Unsupported game", -0x1bff → "Game could not be read", so
        decompression produced *something* but it's not that format/offset.
      - Next: try `TYPE_US` (extract the `SECRET MISSION` file, decompress,
        let the adventure-number loader pick SECRET_MISSION_US); diff decompressed
        image vs upstream unp64 to find the DB start → imgoffset.

- [ ] **Strange Odyssey** (0x12d2) — single packed PRG `strange odyssey`,
      `SYS 2061`. Loads as **STRANGE_ODYSSEY_US** (adventure 6) via the generic
      path; no other Strange Odyssey C64 release is catalogued to model on.
      - Next: decompress, identify packer + iterations, locate the DB; try
        `TYPE_US` imgoffset=0 first (generic LoadBinaryDatabase), then all-in-one
        (+ a `handle_all_in_one` STRANGE_ODYSSEY_US case) if graphics are wanted.

- [ ] **The Count** (0x2827) — single packed PRG `the count`, German crack
      ("DATEN ID 2A", "KLAUSUR"), `SYS 2240`. Loads as **COUNT_US** (adventure 5)
      via the generic adventure-number path — *no new GameInfo needed*.
      - Next: decompress, identify packer + iterations, locate the DB; try
        `TYPE_US` imgoffset=0 first; for C64 graphics add a `handle_all_in_one`
        COUNT_US case + image-offset list.
