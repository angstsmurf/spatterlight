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
