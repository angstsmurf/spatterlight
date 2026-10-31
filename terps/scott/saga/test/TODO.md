# Scott pictures: open work

Goal: pictures that look like the originals'. What is already tested and settled
is in `README.md`.

## Divergences from the originals (found by the scene tests)

Where a picture is compared anyway it is a `!check ... min=N` in its scene,
with a comment there; those that can't be compared are left out of the scenes.
Decide per item whether the interpreter should follow the original.

- [ ] **Savage Island part I, room 24 (ZX).** Action 59 makes this room dark,
      and the interpreter goes dark. The original stays lit, across turns and
      on a natural walk from room 23. Not understood yet. `min=` in
      `savage_pictures_zx/room24` (63%).
- [ ] **Savage Island part I, the bear (ZX).** In room 11 the interpreter draws
      the bear's own picture 13, the original the beach bear, picture 9; on the
      beach (room 8) the interpreter draws the beach bear (`scott_display.c`),
      the original no bear at all. `min=` in `room11` (70%) and
      `item020_room08` (87%).
- [ ] **Howarth vector pictures, line colour (ZX).** The Spectrum original
      draws its lines in the complement of the background colour (cyan on the
      oak's red, magenta on the hedge's green); the interpreter uses black,
      white on a black background. The C64 original uses black too, so this
      would be a Spectrum-only change: `line_colour = bg_colour ^ 7` in
      `DrawHowarthVectorPicture` (`ai_uk/line_drawing.c`) takes `baton_zx` oak
      from 93.36% to 97.61% and hedge from 91.36% to 98.75%.
- [ ] **Howarth vector pictures, colour clash (ZX and C64).** Both machines
      have two colours per 8x8 cell, so an outline changes colour where a fill
      touches its cell; the interpreter gives every pixel its own colour. The
      fills match everywhere. `baton_zx` and `baton_c64`: forest 95.26%, stream
      93.97%, clearing 97.63%, oak 93.36% / 95.96%, hedge 91.36% / 98.75%.
- [ ] **Adventureland forest (ZX), one cell.** The picture asks for tile
      132 + 128 = 260 at cell (5,8); the original reads the 8 bytes past its
      256-tile table (the start of the image address table) and shows them as
      noise, the interpreter wraps around to tile 4, a foliage tile. 22 pixels.
- [ ] **Robin of Sherwood, Herne's cave (ZX).** The original leaves a mark of
      the outside waterfall on the cave picture: the six columns it occupied
      get scrolled down one more pixel on some rows, which ones depending on
      the timing of the keypresses (236 pixels in the capture, 99.04%).
- [ ] **Hulk (C64), last line of every picture.** The original switches from
      bitmap to text mode in the middle of the picture's bottom line: from 112
      pixels in, that line shows the empty text screen. 99.29–99.83%.
- [ ] **Hulk and Claymorgue US (C64), the palette is global.** Every picture
      drawn writes its four colours into the whole screen and colour RAM, so
      a room with object pictures over it ends up in the colours of the last
      one drawn: Hulk's chair room after the ring (98.4%), its bees after the
      wax (99.7%), the large hole in the ring's red (94.8%). The interpreter
      keeps each picture's own colours, which is what the pictures were drawn
      with. Likely leave as it is.
- [ ] **Spider-Man, Madame Web (C64), one cell.** The tile at cell (24,3) has
      the wrong colours in the T64 copy; the interpreter patches it on purpose
      (`image_patches` in `sagadraw.c`). Nothing to do unless the patch is
      dropped.
- [ ] **Seas of Blood, start room before the first move.** The original draws
      it before the opening automatic actions have placed the coastline; the
      interpreter draws it after. Not checked for that reason.
- [ ] **Hulk tape loading screen (ZX).** The original starts by itself once
      loaded, and the ROM prints `Bytes: hulk (tm)` across the picture for the
      next block; the interpreter shows the finished picture until a key.
