//
//  taylordraw.c
//  Spatterlight
//
//  Created by Administrator on 2025-12-10.
//
//  High-level image composition for TaylorMade (Adventure Soft) adventure games
//  (Fantastic Four, He-Man, Rebel Planet, Blizzard Pass, Kayleth, Temple of Terror.)
//  Also used by the ScottFree interpreter for Seas of Blood.
//
//  Images are assembled by executing a bytecode instruction
//  stream stored per location. Instructions draw "Irmak" format sub-images
//  into the shared imagebuffer, then apply post-processing transforms
//  (mirror, flip, colour replacement) to build the final frame.
//
//  The instruction set uses opcodes 0xE8–0xFF for special operations
//  and treats all other byte values as sub-image indices to draw at
//  a given (x, y) position via DrawPictureAtPos().
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "taylor.h"
#include "taylordraw.h"

#include "irmak.h"

/* Mirror a rectangular area horizontally: copy the left half of
   the region to the right half, flipping each tile. Used to create
   symmetric scenes (e.g. doorways, corridors) from half an image.
   The area spans columns [x1, x1+width) and rows [y1, y2). */
static void mirror_area(int x1, int y1, int width, int y2)
{
    for (int line = y1; line < y2; line++) {
        uint16_t source = line * IRMAK_IMGWIDTH + x1;
        uint16_t target = source + width - 1;
        for (int col = 0; col < width / 2; col++) {
            if (target >= IRMAK_IMGSIZE || source >= IRMAK_IMGSIZE)
                return;
            imagebuffer[target][8] = imagebuffer[source][8];
            for (int pixrow = 0; pixrow < 8; pixrow++)
                imagebuffer[target][pixrow] = imagebuffer[source][pixrow];
            Flip(imagebuffer[target]);
            source++;
            target--;
        }
    }
}

/* Mirror the top half of the full image downward: row 0 → row 11,
   row 1 → row 10, etc. Each tile's pixel rows are also reversed
   vertically to produce a proper reflection. */
static void mirror_top_half(void)
{
    for (int line = 0; line < IRMAK_IMGHEIGHT / 2; line++) {
        for (int col = 0; col < IRMAK_IMGWIDTH; col++) {
            uint16_t target = (11 - line) * IRMAK_IMGWIDTH + col;
            uint16_t source = line * IRMAK_IMGWIDTH + col;
            if (target >= IRMAK_IMGSIZE || source >= IRMAK_IMGSIZE)
                return;
            imagebuffer[target][8] =
            imagebuffer[source][8];
            for (int pixrow = 0; pixrow < 8; pixrow++)
                imagebuffer[target][7 - pixrow] =
                imagebuffer[source][pixrow];
        }
    }
}

/* Flip the entire image left-to-right: column 0 ↔ column 31.
   Each tile is also flipped internally via Flip() so the pixels
   within each cell are correctly mirrored. Works via a temporary
   buffer to avoid overwriting source data. */
static void flip_image_horizontally(void)
{
    uint8_t mirror[IRMAK_IMGSIZE][9];

    for (int line = 0; line < IRMAK_IMGHEIGHT; line++) {
        for (int col = 32; col > 0; col--) {
            uint16_t target = line * IRMAK_IMGWIDTH + col - 1;
            uint16_t source = line * IRMAK_IMGWIDTH + (IRMAK_IMGWIDTH - col);
            if (target >= IRMAK_IMGSIZE || source >= IRMAK_IMGSIZE)
                return;
            for (int pixrow = 0; pixrow < 9; pixrow++)
                mirror[target][pixrow] = imagebuffer[source][pixrow];
            Flip(mirror[target]);
        }
    }

    memcpy(imagebuffer, mirror, IRMAK_IMGSIZE * 9);
}

/* Flip the entire image top-to-bottom: row 0 ↔ row 11. Within
   each tile, pixel rows are reversed (row 0 ↔ row 7) so the
   internal content is also flipped vertically. */
static void flip_image_vertically(void)
{
    uint8_t mirror[IRMAK_IMGSIZE][9];

    for (int line = 0; line < IRMAK_IMGHEIGHT; line++) {
        for (int col = 0; col < IRMAK_IMGWIDTH; col++) {
            uint16_t target = (11 - line) * IRMAK_IMGWIDTH + col;
            uint16_t source = line * IRMAK_IMGWIDTH + col;
            if (target >= IRMAK_IMGSIZE || source >= IRMAK_IMGSIZE)
                return;
            for (int pixrow = 0; pixrow < 8; pixrow++)
                mirror[target][7 - pixrow] =
                imagebuffer[source][pixrow];
            mirror[target][8] =
            imagebuffer[source][8];
        }
    }
    memcpy(imagebuffer, mirror, IRMAK_IMGSIZE * 9);
}

/* Flip a rectangular sub-region top-to-bottom within the imagebuffer.
   Rows [y1..y2] in columns [x1, x1+width) are reversed, with pixel
   rows within each tile also inverted. Uses a scratch buffer to avoid
   in-place corruption. */
static void flip_area_vertically(uint8_t x1, uint8_t y1, uint8_t width, uint8_t y2)
{
    uint8_t mirror[IRMAK_IMGSIZE][9];

    for (int line = 0; line <= y2 && line < IRMAK_IMGHEIGHT - y1; line++) {
        for (int col = x1; col < x1 + width; col++) {
            uint16_t target = (y2 - line) * IRMAK_IMGWIDTH + col;
            if (target >= IRMAK_IMGSIZE)
                return;
            uint16_t source = (y1 + line) * IRMAK_IMGWIDTH + col;
            if (source >= IRMAK_IMGSIZE)
                return;
            for (int pixrow = 0; pixrow < 8; pixrow++)
                mirror[target][7 - pixrow] = imagebuffer[source][pixrow];
            mirror[target][8] = imagebuffer[source][8];
        }
    }
    for (int line = y1; line <= y2; line++) {
        for (int col = x1; col < x1 + width; col++) {
            uint16_t index = line * IRMAK_IMGWIDTH + col;
            if (index >= IRMAK_IMGSIZE)
                return;
            for (int pixrow = 0; pixrow < 8; pixrow++)
                imagebuffer[index][pixrow] =
                mirror[index][pixrow];
            imagebuffer[index][8] =
            mirror[index][8];
        }
    }
}

/* Mirror a rectangular area vertically: copy the top half downward
   into the bottom half, reversing pixel rows within each tile.
   Similar to mirror_top_half but confined to a sub-region. */
static void mirror_area_vertically(uint8_t x1, uint8_t y1, uint8_t width, uint8_t y2)
{
    for (int line = 0; line <= y2 / 2; line++) {
        for (int col = x1; col < x1 + width; col++) {
            uint16_t target = (y2 - line) * IRMAK_IMGWIDTH + col;
            uint16_t source = (y1 + line) * IRMAK_IMGWIDTH + col;
            if (target >= IRMAK_IMGSIZE || source >= IRMAK_IMGSIZE)
                return;
            imagebuffer[target][8] =
            imagebuffer[source][8];
            for (int pixrow = 0; pixrow < 8; pixrow++)
                imagebuffer[target][7 - pixrow] =
                imagebuffer[source][pixrow];
        }
    }
}

/* Flip a rectangular sub-region left-to-right within the imagebuffer.
   Columns within [x1, x1+width) for rows [y1, y2) are reversed,
   with each tile also flipped internally. */
static void flip_area_horizontally(uint8_t x1, uint8_t y1, uint8_t width, uint8_t y2)
{
    uint8_t mirror[IRMAK_IMGSIZE][9];

    for (int line = y1; line < y2; line++) {
        for (int col = 0; col < width; col++) {
            uint16_t target = line * IRMAK_IMGWIDTH + x1 + col;
            uint16_t source = line * IRMAK_IMGWIDTH + x1 + width - col - 1;
            if (target > IRMAK_IMGSIZE || source >= IRMAK_IMGSIZE)
                return;
            for (int pixrow = 0; pixrow < 9; pixrow++)
                mirror[target][pixrow] =
                imagebuffer[source][pixrow];
            Flip(mirror[target]);
        }
    }

    for (int line = y1; line < y2; line++) {
        for (int col = x1; col < x1 + width; col++) {
            uint16_t index = line * IRMAK_IMGWIDTH + col;
            if (index > IRMAK_IMGSIZE)
                return;
            for (int pixrow = 0; pixrow < 8; pixrow++)
                imagebuffer[index][pixrow] = mirror[index][pixrow];
            imagebuffer[index][8] = mirror[index][8];
        }
    }
}

/* Paint a horizontal run of tile cells with a colour attribute.
   Used by the older instruction format (Blizzard Pass, Rebel Planet)
   where 0xFC takes (x, y, colour, length). */
static void draw_colour_old(uint8_t x, uint8_t y, uint8_t colour, uint8_t length)
{
    uint16_t index = y * IRMAK_IMGWIDTH + x;
    if (index >= IRMAK_IMGSIZE)
        return;
    if (index + length >= IRMAK_IMGSIZE)
        length = IRMAK_IMGSIZE - index - 1;
    for (int i = 0; i < length; i++) {
        imagebuffer[index + i][8] = colour;
    }
}

/* Paint a rectangular area of tile cells with a colour attribute.
   Used by the newer instruction format (He-Man and later) where
   0xFC takes (y, x, height, colour, width). */
static void draw_colour(uint8_t colour, uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    for (int h = 0; h < height; h++) {
        for (int w = 0; w < width; w++) {
            int index = (y + h) * IRMAK_IMGWIDTH + x + w;
            if (index >= IRMAK_IMGSIZE)
                return;
            imagebuffer[index][8] = colour;
        }
    }
}

/* Set the BRIGHT flag on every tile cell in the imagebuffer,
   making all colours use their bright variant. */
static void make_light(void)
{
    for (int i = 0; i < IRMAK_IMGSIZE; i++) {
        imagebuffer[i][8] = imagebuffer[i][8] | BRIGHT_FLAG;
    }
}

/* Replace one colour with another across the whole image.
   Both ink and paper fields are checked independently: if the ink
   matches 'before', it becomes 'after', and likewise for paper
   (shifted into the paper field position). */
static void replace_colour(uint8_t before, uint8_t after)
{
    uint8_t beforeink = before & INK_MASK;
    uint8_t afterink = after & INK_MASK;

    uint8_t beforepaper = beforeink << 3;
    uint8_t afterpaper = afterink << 3;

    for (int j = 0; j < IRMAK_IMGSIZE; j++) {
        if ((imagebuffer[j][8] & INK_MASK) == beforeink) {
            imagebuffer[j][8] = (imagebuffer[j][8] & ~INK_MASK) | afterink;
        }

        if ((imagebuffer[j][8] & PAPER_MASK) == beforepaper) {
            imagebuffer[j][8] = (imagebuffer[j][8] & ~PAPER_MASK) | afterpaper;
        }
    }
}

/* Convert an ink-format colour value to paper-format by shifting
   the ink bits into the paper field position, preserving brightness. */
static uint8_t ink2paper(uint8_t ink)
{
    uint8_t paper = (ink & INK_MASK) << 3;
    paper = paper & PAPER_MASK;
    return (ink & BRIGHT_FLAG) | paper;
}

/* Replace all occurrences of 'before' with 'after' in the masked
   portion of every cell's attribute byte. The mask selects which
   bits to compare and replace (e.g. INK_MASK or PAPER_MASK). */
static void replace(uint8_t before, uint8_t after, uint8_t mask)
{
    for (int j = 0; j < IRMAK_IMGSIZE; j++) {
        uint8_t col = imagebuffer[j][8] & mask;
        if (col == before) {
            uint8_t newcol = imagebuffer[j][8] | mask;
            newcol = newcol ^ mask;
            imagebuffer[j][8] = newcol | after;
        }
    }
}

/* Replace a colour in both ink and paper fields simultaneously.
   'before' and 'after' are specified in ink format; ink2paper()
   converts them for the paper field replacement. */
static void replace_paper_and_ink(uint8_t before, uint8_t after)
{
    uint8_t beforeink = before & (INK_MASK | BRIGHT_FLAG);
    replace(beforeink, after, INK_MASK | BRIGHT_FLAG);
    uint8_t beforepaper = ink2paper(before);
    uint8_t afterpaper = ink2paper(after);
    replace(beforepaper, afterpaper, PAPER_MASK | BRIGHT_FLAG);
}

/* Pointers into the game file's image instruction data. */
static uint8_t *TaylorImageData;
static uint8_t *EndOfTaylorDrawData;

/* Precomputed index of stream start pointers. stream_offsets[i]
   points to the first byte of stream i's instruction data (the
   byte after the i-th 0xFF terminator, or the start for stream 0).
   Built once during InitTaylor() for O(1) lookup in DrawTaylor(). */
static uint8_t **stream_offsets;
static int stream_count;

/* Object location array: ObjectLocInternal[obj] == room means object
   'obj' is in that room. Used by conditional draw instructions
   (0xF8, 0xF4) to show/hide object images in the scene. */
static uint8_t *ObjectLocInternal;

/* Flags for game-specific instruction format differences. */
static int UseOlderVersion = 0;
static int IsRebelplanet = 0;

/* Optional callback for Seas of Blood object drawing. When non-NULL,
   opcode 0xF9 calls this instead of recursing into DrawTaylor(). */
static draw_obj_fn draw_seas_obj = NULL;

/* Initialise the TaylorMade drawing system with pointers into the
   game data and game-specific configuration flags. Builds the
   stream offset index by scanning for 0xFF terminators. */
void InitTaylor(uint8_t *data, uint8_t *end, uint8_t *objloc, int older, int rebel, draw_obj_fn obj_draw) {
    TaylorImageData = data;
    EndOfTaylorDrawData = end;
    ObjectLocInternal = objloc;
    UseOlderVersion = older;
    IsRebelplanet = rebel;
    draw_seas_obj = obj_draw;

    /* Count 0xFF-terminated streams to size the index. */
    int count = 0;
    for (uint8_t *p = data; p < end; p++) {
        if (*p == 0xff)
            count++;
    }
    /* count is the number of terminators; there are count streams
       (stream 0 starts at data, each 0xFF starts the next). */
    stream_count = count;

    free(stream_offsets);
    stream_offsets = malloc(sizeof(uint8_t *) * (count + 1));
    if (!stream_offsets) {
        stream_count = 0;
        return;
    }

    stream_offsets[0] = data;
    int idx = 1;
    for (uint8_t *p = data; p < end && idx <= count; p++) {
        if (*p == 0xff)
            stream_offsets[idx++] = p + 1;
    }
}

/* Return the number of bytes remaining after ptr (not counting *ptr). */
static inline int BytesLeft(const uint8_t *ptr, const uint8_t *end)
{
    return (int)(end - ptr) - 1;
}

/* Execute the draw instruction stream for location 'loc'.
   'current_location' is the player's actual room — usually the same
   as 'loc', but differs when opcode 0xF9 recurses to draw another
   location's image as part of the current scene. Object visibility
   checks (0xF8, 0xF4) always test against current_location.

   The target stream is found via the precomputed stream_offsets[]
   index built during InitTaylor().

   Returns 1 on success, 0 on error or missing data. */
int DrawTaylor(int loc, int current_location)
{
    if (loc < 0 || loc >= stream_count || !stream_offsets)
        return 0;

    uint8_t *ptr = stream_offsets[loc];
    if (ptr >= EndOfTaylorDrawData)
        return 0;

    /* Execute instructions until 0xFF (end) or data runs out. */
    while (ptr < EndOfTaylorDrawData) {
        switch (*ptr) {
        case 0xff: /* End of picture */
            return 1;

        case 0xfe: /* Mirror the left half of the image to the right */
            mirror_area(0, 0, IRMAK_IMGWIDTH, IRMAK_IMGHEIGHT);
            break;

        case 0xfd: /* Replace colour: args (before, after) */
            if (BytesLeft(ptr, EndOfTaylorDrawData) < 2) return 0;
            replace_colour(ptr[1], ptr[2]);
            ptr += 2;
            break;

        case 0xfc: /* Draw colour attribute over an area.
                      Older format: (x, y, colour, length) — linear run.
                      Newer format: (y, x, height, colour, width) — rectangle. */
            if (UseOlderVersion) {
                if (BytesLeft(ptr, EndOfTaylorDrawData) < 4) return 0;
                draw_colour_old(ptr[1], ptr[2], ptr[3], ptr[4]);
                ptr += 4;
            } else {
                if (BytesLeft(ptr, EndOfTaylorDrawData) < 5) return 0;
                draw_colour(ptr[4], ptr[2], ptr[1], ptr[5], ptr[3]);
                ptr += 5;
            }
            break;

        case 0xfb: /* Make all colours bright */
            make_light();
            break;

        case 0xfa: /* Flip entire image horizontally */
            flip_image_horizontally();
            break;

        case 0xf9: /* Draw another image recursively or call Seas of Blood
                      object draw callback. For Seas of Blood, args are (x, y);
                      for other games, arg is a location number. */
            if (draw_seas_obj != NULL) {
                if (BytesLeft(ptr, EndOfTaylorDrawData) < 2) return 0;
                draw_seas_obj(ptr[1], ptr[2]);
                ptr += 2;
            } else {
                if (BytesLeft(ptr, EndOfTaylorDrawData) < 1) return 0;
                DrawTaylor(ptr[1], current_location);
                ptr++;
            }
            break;

        case 0xf8: /* Conditional draw based on object presence.
                      Older: if object arg1 is here, draw image arg2 at (arg3, arg4).
                      Newer: stop drawing if object arg1 is NOT here. */
            if (UseOlderVersion) {
                if (BytesLeft(ptr, EndOfTaylorDrawData) < 4) return 0;
                if (ObjectLocInternal[ptr[1]] == current_location) {
                    DrawPictureAtPos(ptr[2], ptr[3], ptr[4], 1);
                }
                ptr += 4;
            } else {
                if (BytesLeft(ptr, EndOfTaylorDrawData) < 1) return 0;
                if (ObjectLocInternal[ptr[1]] != current_location) {
                    return 1;
                }
                ptr++;
            }
            break;

        case 0xf4: /* Stop drawing if object arg1 IS present */
            if (BytesLeft(ptr, EndOfTaylorDrawData) < 1) return 0;
            if (ObjectLocInternal[ptr[1]] == current_location)
                return 1;
            ptr++;
            break;

        case 0xf3: /* Mirror top half of image downward */
            mirror_top_half();
            break;

        case 0xf2: /* Mirror a sub-area horizontally: args (y1, x, y2, width) */
            if (BytesLeft(ptr, EndOfTaylorDrawData) < 4) return 0;
            mirror_area(ptr[2], ptr[1], ptr[4], ptr[3]);
            ptr += 4;
            break;

        case 0xf1: /* Mirror a sub-area vertically: args (y1, x, y2, width) */
            if (BytesLeft(ptr, EndOfTaylorDrawData) < 4) return 0;
            mirror_area_vertically(ptr[1], ptr[2], ptr[4], ptr[3]);
            ptr += 4;
            break;

        case 0xee: /* Flip a sub-area horizontally: args (y1, x, y2, width) */
            if (BytesLeft(ptr, EndOfTaylorDrawData) < 4) return 0;
            flip_area_horizontally(ptr[2], ptr[1], ptr[4], ptr[3]);
            ptr += 4;
            break;

        case 0xed: /* Flip entire image vertically */
            flip_image_vertically();
            break;

        case 0xec: /* Flip a sub-area vertically: args (y1, x, y2, width) */
            if (BytesLeft(ptr, EndOfTaylorDrawData) < 4) return 0;
            flip_area_vertically(ptr[1], ptr[2], ptr[4], ptr[3]);
            ptr += 4;
            break;

        case 0xeb:
        case 0xea:
            fprintf(stderr, "Unimplemented draw instruction 0x%02x!\n", *ptr);
            return 0;

        case 0xe9: /* Replace both ink and paper for a colour: args (before, after) */
            if (BytesLeft(ptr, EndOfTaylorDrawData) < 2) return 0;
            replace_paper_and_ink(ptr[1], ptr[2]);
            ptr += 2;
            break;

        case 0xe8: /* Clear the compositing buffer */
            ClearGraphMem();
            break;

        case 0xf7: /* Vestigial opcode: sets A register to 0x0C then draws,
                      but A is unused by the draw routine. Falls through to
                      the default draw-image handler.

                      Rebel Planet special case: the Zodiac Hotel basement
                      (room 43) uses 0xF7 to draw bottles in a locked alcove.
                      The original interpreter never renders them. We show
                      them only when the alcove is unlocked (detected by the
                      phonic fork, object 131, being out of play in room 252).
                      Safe because no other game/image uses 0xF7. */
            if (IsRebelplanet && current_location == 43 && ObjectLocInternal[131] == 252)
                return 1;
            /* Deliberate fallthrough */
        case 0xf6: /* Vestigial: sets A to 0x04, falls through to draw */
        case 0xf5: /* Vestigial: sets A to 0x08, falls through to draw */
            if (BytesLeft(ptr, EndOfTaylorDrawData) < 3) return 0;
            ptr++; /* Skip the extra byte, then fall through to default */
            /* Deliberate fallthrough */

        default: /* Draw sub-image: byte is image index, args (x, y) */
            if (BytesLeft(ptr, EndOfTaylorDrawData) < 2) return 0;
            DrawPictureAtPos(ptr[0], ptr[1], ptr[2], 1);
            ptr += 2;
            break;
        }
        ptr++;
    }
    return 0;
}
