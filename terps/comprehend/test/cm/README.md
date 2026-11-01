# The Coveted Mirror — Apple II hourglass ground truth

Captured from MAME (`apple2e` + the woz-a-day CM disks) on 2026-06-13.
Boot side B, press **S** (standard hi-res), swap to side A at "turn thy
diskette over", press a key. Active video = hi-res **page 1 ($2000)**.

## Files
(Both the `.png` reference screenshots, in `images/`, and the `.page` golden
compare targets, in `fixtures/`, are kept local-only — gitignored — as they
derive from the copyrighted game.)
- `fixtures/throne_sand60.page` / `images/throne_sand60.png` — throne room (room
  0x12) at the game start, VM sand var 0x11 = 60. Full panel: logo + complete
  hourglass.
- `fixtures/prison_displayed60.page` / `images/prison_displayed60.png` — the
  prison (after the throne-room imprisonment trap; imprisonment RESETS sand to
  74, displayed counter held 60).
- `images/hourglass_sand60_6x.png` — the throne hourglass cropped + scaled 6x.
- `hourglass_anatomy.txt` — ASCII bit map of cols 22..39, rows 55..150
  (regenerate with /tmp/hgmap.py).

`.page` files are the raw 0x2000-byte hi-res page; render with
`test/pixtest --render fixtures/<file.page> out.ppm`. Our renderer reproduces them
pixel-faithfully (validates the NTSC artifact-colour algorithm).

## Hourglass = OG image 0 + top-bulb grains (SOLVED, pixel-exact)
Tracing the boot draw in MAME (watchpoint on a mound pixel + the image-index
register $4539) showed the panel is composed of two ordinary Graphics Magician
images, drawn in order: RG image 0 = the logo, then **OG image 0 = the
hourglass** (orange frame: posts, top/bottom bars, bowtie glass; the static
bottom-bulb mound; and the neck stream dots). OG0's disk bytes match the live
$1700 image buffer byte-for-byte, and our renderer draws all 54 of its ops
cleanly. The draining **top-bulb sand** (white ▽, rows ~86-100) is NOT in OG0 —
it is grain-drawn by `cm_draw_hourglass_grain` ($4347), ported as
`gmDrawCMHourglass(sand)`.

`pics.cpp` now builds the panel as RG0 + OG0 and stamps the top-bulb grains for
the live sand var (0x11). Verified PIXEL-EXACT vs `fixtures/throne_sand60.page`: 0
lit-pixel diffs over cols 24-39, rows 0-146 (only the background palette bit
0x80-vs-0x00 differs, which renders identically black). Render references:
`images/hourglass_OG0_empty.png` (OG0 alone) and
`images/hourglass_OG0_plus60grains_6x.png`.

For reference, the original anatomy (cols ~33..39): top bar rows 64-65, bottom
bar 137-138, posts 66-136, bowtie diagonals converge 86-100 / diverge 100-114;
top-bulb ▽ rows ~86-100; bottom mound rows ~124-136; stream dots rows ~100-123
(single white px → blue/orange via NTSC artifact).

## Strings oracle (2026-11-01)

`strings_oracle.sh` checks our string decoder against the original interpreter's:
`strings_oracle.lua`, run in MAME on the booted game, patches the game's own
string printer (`$5698` → decoder `$57cd` → character output `$58d1`) to append
each character to a RAM buffer, calls it for every string reference of both
tables (S1 `$80`/`$81` × 0–154, S2 `$82`–`$84` × 0–255, i.e. the twelve MA–ML
banks) and writes the result as `fixtures/original_strings.txt` in the format of
`COMPREHEND_STRDUMP` (local-only, it is the game's text). The item-name
substitution the original makes for a capitalised space is replaced by our `@`
placeholder, so the two sides compare directly. The 900 reachable strings (all
155 of S1, S2 0–744) match byte for byte; S2 745–767 are stale offset-table
slots at the end of bank ML, referenced by nothing, that the two decoders turn
into different junk, and are skipped.

The check found one divergence, since fixed in `ComprehendGame::stringLookup`:
function 22 (EXIT at the Color Fairy's with the door open) prints S1 428 of a
155-entry table. The original's seek walks past the table into zeroed RAM and
prints a blank line; ours printed `BAD_STRING(81ac)`.

## Open

- Eyeball the hourglass grain-fall animation once in the Spatterlight GUI. The
  timer cannot run in the headless CheapGlk build; the wiring mirrors the proven
  slow-draw path and the *resting* sand level is validated above, but the falling
  motion has never been watched in the app.

## Not applicable -- the king's face animation

The king's face does **not** animate in the Comprehend release of the game; that
and the other animated effects exist only in the non-Comprehend release. Nothing
to port, do not add them (the colour-flash XOR at $4284/$428e is the *other*
release's code).

The hourglass grain fall is **deliberate and stays**, even though it is ours
rather than something the Comprehend release animates: the resting level is
pixel-exact against `fixtures/throne_sand60.page` and the falling-grain motion
rides the proven slow-draw path. Don't "correct" it away by appealing to the
paragraph above.

## Reference (verified in MAME)

Boot: `apple2e -flop1 "...side B (boot).woz"`, press **S** (standard hi-res), at
"TURN THY DISKETTE OVER" load side A into the same drive via Lua, then a key.
Debugger `statesave`/`stateload <name>` is an instant reset to the throne room
(a state saved with the drive idle loads safely). Headless:
`COMPREHEND_SCRIPT=cmds.txt ./comprehend_hl -g covetedmirror "<side B.woz>"`.

- VM data (G0, header modified at load) at **$5a00**; variables base **$5a3f**
  (16-bit each), flags array base $5b3f, current-room byte $5a3d, functions block
  base $7bec. Hourglass = var 0x11 ($5a61), displayed counter at $42f7.
  Per-command dispatch $4092; main loop $404c; daemon = function 0.
- String printer $5698 ($5a38 index low, $5a39 table byte), decoder $57cd,
  character output $58d1; S1 table at ($5a36) = $a5e4; the MA-ML banks are
  loaded on demand into $1700 (see the strings oracle above).
- startRoom = **0x12 (18)** = throne (S1[67], only exit S to 0x11). **Room 1 =
  prison** (S1[80], exits S to 6, W to 2). Voar = ITEM[159], word 0x32
  (`voar`/`king`), room 0x12.
- Bytecode operands are 1-BASED item numbers; PRINT string refs `(idx, 8x)`:
  0x82 = S2[idx], 0x83 = S2[idx+0x100], 0x84 = S2[idx+0x200].
- Mirror pieces: var 7 counts, 4 needed (var 8). Rooms 0x57 / 0x16 / 0x01 / 0x0b
  (gated by flags 0x15, 2/0x1b). Endgame: weight <= var 9 for the Perilous Pass
  squeeze, WEAR COAT for the summit, plateau 0x51 WAIT x7 + GET MIRROR when
  var 0x18 == var 0x1b (7).
- Bribe items: BROOM/PICTURE/COOKIE/FLOWER/TELESCOPE/AX/JUG; Bernt's cookies
  (GIVE MOOSE at the bakery, room 0x32) are the renewable bribe. Riddle at
  Winder's: answer `UNCLE`.
- A full transcript replay against MAME was considered and dropped (2026-11-01):
  the original's RNG is unknown and ours is erkyrath's, so wanderer spawns, GET
  SHADOW retries and random messages diverge within a few moves. The strings
  oracle plus the winning walkthrough cover what it was meant to catch.
